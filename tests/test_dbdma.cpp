/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file DBDMA channel functional tests.
 *
 * Tests exercise real command processing (NOP, STOP, STORE_QUAD, LOAD_QUAD,
 * OUTPUT_MORE, INPUT_MORE) and control logic (branching, interrupt
 * generation, wait stalling, descriptor write-back).  No tautological
 * register round-trips; every assertion verifies observable side-effects
 * produced by the DBDMA state machine.
 *
 * RAM layout used by the tests (all within a 4 KiB backing buffer):
 *   0x0000 – 0x00FF  DMA command descriptors (up to 16 × 16 B)
 *   0x0100 – 0x01FF  Source data for OUTPUT tests
 *   0x0200 – 0x02FF  Destination buffer for INPUT tests
 *   0x0300 – 0x03FF  Scratch area (STORE_QUAD target, LOAD_QUAD source)
 */

#include <core/timermanager.h>
#include <cpu/ppc/ppcemu.h>
#include <devices/common/dbdma.h>
#include <devices/common/dmacore.h>
#include <devices/common/hwinterrupt.h>
#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <memaccess.h>

#include <cstdint>
#include <cstring>
#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

// ---------------------------------------------------------------------------
// Test framework
// ---------------------------------------------------------------------------
static int tests_run    = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do {         \
    tests_run++;                             \
    if (!(cond)) {                           \
        cerr << "FAIL: " << msg             \
             << " (" << __FILE__            \
             << ":" << __LINE__ << ")"      \
             << endl;                        \
        tests_failed++;                      \
    }                                        \
} while (0)

// ---------------------------------------------------------------------------
// Global test fixtures
// ---------------------------------------------------------------------------
static constexpr uint32_t RAM_BASE = 0x00000000;
static constexpr uint32_t RAM_SIZE = 4096;

static uint8_t       g_ram[RAM_SIZE];
static MemCtrlBase  *g_mem_ctrl = nullptr;
static uint64_t      g_fake_time_ns = 0;

// Offsets within g_ram
static constexpr uint32_t CMD_BASE  = 0x0000; // DMA descriptors
static constexpr uint32_t SRC_BASE  = 0x0100; // OUTPUT source data
static constexpr uint32_t DST_BASE  = 0x0200; // INPUT  destination
static constexpr uint32_t SCR_BASE  = 0x0300; // scratch / QUAD target

// ---------------------------------------------------------------------------
// Helpers: write/read DMA channel registers in PPC bus (big-endian) format.
//
// DMAChannel::reg_write/reg_read swap bytes to/from PPC bus order.
// These helpers transparently cancel that swap so callers work with
// native host values (as visible in the channel's internal state).
// ---------------------------------------------------------------------------
static inline void dma_write(DMAChannel &ch, uint32_t reg, uint32_t val) {
    ch.reg_write(reg, BYTESWAP_32(val), 4);
}

static inline uint32_t dma_read(DMAChannel &ch, uint32_t reg) {
    return BYTESWAP_32(ch.reg_read(reg, 4));
}

// Write CH_CTRL: mask selects which status bits are updated, data is the new value.
static inline void dma_ctrl(DMAChannel &ch, uint16_t mask, uint16_t data) {
    dma_write(ch, DMAReg::CH_CTRL, ((uint32_t)mask << 16) | data);
}

// Start the DMA channel (set RUN=1, which also asserts ACTIVE).
static inline void dma_start(DMAChannel &ch) {
    dma_ctrl(ch, CH_STAT_RUN, CH_STAT_RUN);
}

// Stop the DMA channel (clear RUN=1).
static inline void dma_stop(DMAChannel &ch) {
    dma_ctrl(ch, CH_STAT_RUN, 0);
}

// Set cmd_ptr to a physical address within g_ram.
static inline void dma_set_cmd_ptr(DMAChannel &ch, uint32_t addr) {
    dma_write(ch, DMAReg::CMD_PTR_LO, addr);
}

// ---------------------------------------------------------------------------
// DMA command descriptor builder.
//
// The DMACmd struct fields use little-endian byte order (per DBDMA spec).
// On a little-endian (x86-64) host the struct layout matches memory
// directly, so we can write to g_ram via pointer casts.
// ---------------------------------------------------------------------------
static void write_cmd(uint32_t offset, uint8_t cmd, uint8_t cmd_bits,
                      uint16_t req_count, uint32_t address, uint32_t cmd_arg)
{
    DMACmd *d = reinterpret_cast<DMACmd *>(&g_ram[offset]);
    d->req_count = req_count;
    d->cmd_bits  = cmd_bits;
    d->cmd_key   = static_cast<uint8_t>(cmd << 4);   // key = 0, cmd in high nibble
    d->address   = address;
    d->cmd_arg   = cmd_arg;
    d->res_count = 0xFFFF; // sentinel – should be overwritten on completion
    d->xfer_stat = 0xFFFF; // sentinel
}

// Variant for STORE_QUAD / LOAD_QUAD which require key = 6.
static void write_quad_cmd(uint32_t offset, uint8_t cmd, uint8_t cmd_bits,
                           uint16_t req_count, uint32_t address, uint32_t cmd_arg)
{
    DMACmd *d = reinterpret_cast<DMACmd *>(&g_ram[offset]);
    d->req_count = req_count;
    d->cmd_bits  = cmd_bits;
    d->cmd_key   = static_cast<uint8_t>((cmd << 4) | 6); // key = 6 required for QUAD
    d->address   = address;
    d->cmd_arg   = cmd_arg;
    d->res_count = 0xFFFF;
    d->xfer_stat = 0xFFFF;
}

// ---------------------------------------------------------------------------
// Mock interrupt controller
// ---------------------------------------------------------------------------
class MockIntCtrl : public InterruptCtrl {
public:
    MockIntCtrl() : HWComponent("MockIntCtrl") {}
    ~MockIntCtrl() = default;
    void ack_int(uint64_t, uint8_t) override {}
    void ack_dma_int(uint64_t, uint8_t irq_line_state) override {
        if (irq_line_state) dma_irq_count++;
    }

    int dma_irq_count = 0;
};

// ---------------------------------------------------------------------------
// Per-test channel factory: fresh DMAChannel + clean RAM each time.
// ---------------------------------------------------------------------------
static DMAChannel make_channel(const char *name = "test") {
    memset(g_ram, 0, RAM_SIZE);
    return DMAChannel(name);
}

// ---------------------------------------------------------------------------
// 1. CH_CTRL always reads as zero (DBDMA spec section 5.5.1, table 74)
// ---------------------------------------------------------------------------
static void test_ch_ctrl_reads_zero() {
    cout << "  test_ch_ctrl_reads_zero..." << endl;

    DMAChannel ch = make_channel();

    // CH_CTRL must return 0 regardless of what was written
    ch.reg_write(DMAReg::CH_CTRL, 0xFFFFFFFFU, 4);
    uint32_t val = dma_read(ch, DMAReg::CH_CTRL);
    TEST_ASSERT(val == 0, "CH_CTRL should always read as 0");
}

// ---------------------------------------------------------------------------
// 2. STOP command clears ACTIVE and leaves the channel idle
// ---------------------------------------------------------------------------
static void test_stop_halts_channel() {
    cout << "  test_stop_halts_channel..." << endl;

    DMAChannel ch = make_channel();

    write_cmd(CMD_BASE, DBDMA_Cmd::STOP, 0, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "ACTIVE should be cleared after STOP");
    TEST_ASSERT(!(stat & CH_STAT_DEAD),   "DEAD should not be set after STOP");
}

// ---------------------------------------------------------------------------
// 3. NOP advances cmd_ptr by one descriptor (16 bytes), then STOP halts
// ---------------------------------------------------------------------------
static void test_nop_stop() {
    cout << "  test_nop_stop..." << endl;

    DMAChannel ch = make_channel();

    write_cmd(CMD_BASE,       DBDMA_Cmd::NOP,  0, 0, 0, 0);
    write_cmd(CMD_BASE + 16,  DBDMA_Cmd::STOP, 0, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Channel should have processed both commands and now be idle
    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "ACTIVE cleared after NOP+STOP sequence");
    TEST_ASSERT(!(stat & CH_STAT_DEAD),   "DEAD not set after clean NOP+STOP");
}

// ---------------------------------------------------------------------------
// 4. STORE_QUAD writes the cmd_arg value to the specified RAM address
// ---------------------------------------------------------------------------
static void test_store_quad_writes_ram() {
    cout << "  test_store_quad_writes_ram..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t TARGET  = SCR_BASE;
    static constexpr uint32_t PAYLOAD = 0xDEADBEEFU;

    // Write 0 to the target first so we can confirm it changed
    WRITE_DWORD_LE_A(&g_ram[TARGET], 0);

    // STORE_QUAD: req_count=4 → xfer_size=4, key must be 6
    write_quad_cmd(CMD_BASE,      DBDMA_Cmd::STORE_QUAD, 0, 4, TARGET, PAYLOAD);
    write_cmd     (CMD_BASE + 16, DBDMA_Cmd::STOP,       0, 0, 0,      0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    uint32_t stored = READ_DWORD_LE_A(&g_ram[TARGET]);
    TEST_ASSERT(stored == PAYLOAD, "STORE_QUAD must write cmd_arg to target address");

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "Channel idle after STORE_QUAD+STOP");
}

// ---------------------------------------------------------------------------
// 5. LOAD_QUAD reads a RAM word and stores it back into cmd_arg in the
//    descriptor (the mmu_map_dma_mem region is writable because g_ram is RAM)
// ---------------------------------------------------------------------------
static void test_load_quad_reads_ram() {
    cout << "  test_load_quad_reads_ram..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t SRC    = SCR_BASE;
    static constexpr uint32_t EXPECT = 0xCAFEBABEU;

    WRITE_DWORD_LE_A(&g_ram[SRC], EXPECT);

    // LOAD_QUAD: req_count=4, key=6, address=source
    write_quad_cmd(CMD_BASE,      DBDMA_Cmd::LOAD_QUAD, 0, 4, SRC, 0xFFFFFFFFU);
    write_cmd     (CMD_BASE + 16, DBDMA_Cmd::STOP,      0, 0, 0,   0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // cmd_arg field of the LOAD_QUAD descriptor must now hold the value read
    DMACmd *d = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
    uint32_t loaded = READ_DWORD_LE_A(&d->cmd_arg);
    TEST_ASSERT(loaded == EXPECT, "LOAD_QUAD must update descriptor cmd_arg with the read value");
}

// ---------------------------------------------------------------------------
// 6. Branch-always (b=11) must skip the intervening command
// ---------------------------------------------------------------------------
static void test_branch_always() {
    cout << "  test_branch_always..." << endl;

    DMAChannel ch = make_channel();

    // NOP at CMD_BASE with branch=always (cmd_bits[3:2] = 0b11 = 0x0C)
    // cmd_arg holds the branch target address
    static constexpr uint32_t SKIP_CMD  = CMD_BASE + 16;
    static constexpr uint32_t STOP_CMD  = CMD_BASE + 32;

    // Place a unique sentinel at the SKIP_CMD descriptor address field so we
    // can tell whether it was executed (STORE_QUAD would write the field).
    uint32_t sentinel = 0xBADC0FFEU;
    WRITE_DWORD_LE_A(&g_ram[SCR_BASE], sentinel);

    // If branching works, this STORE_QUAD should never run.
    write_quad_cmd(SKIP_CMD, DBDMA_Cmd::STORE_QUAD, 0, 4, SCR_BASE, 0x12345678U);
    write_cmd     (STOP_CMD, DBDMA_Cmd::STOP,        0, 0, 0,        0);

    // NOP with b=11 (branch always), cmd_arg = branch target
    DMACmd *nop = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
    nop->req_count = 0;
    nop->cmd_bits  = 0x0C; // branch = always
    nop->cmd_key   = (DBDMA_Cmd::NOP << 4);
    nop->address   = 0;
    nop->cmd_arg   = STOP_CMD; // branch target
    nop->res_count = 0;
    nop->xfer_stat = 0;

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // The STORE_QUAD was skipped; sentinel must be unchanged
    uint32_t after = READ_DWORD_LE_A(&g_ram[SCR_BASE]);
    TEST_ASSERT(after == sentinel, "Branch-always must skip the STORE_QUAD command");

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "Channel idle after branch-always + STOP");
}

// ---------------------------------------------------------------------------
// 7. OUTPUT_MORE: pull_data returns the bytes described by the command
// ---------------------------------------------------------------------------
static void test_output_pull_data() {
    cout << "  test_output_pull_data..." << endl;

    DMAChannel ch = make_channel();

    // Seed source data
    static constexpr uint32_t DATA_LEN = 8;
    for (uint32_t i = 0; i < DATA_LEN; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(0xA0 + i);

    // OUTPUT_MORE pointing at g_ram[SRC_BASE]
    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, DATA_LEN, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Channel should now be waiting (cmd_in_progress=true, queue_len=DATA_LEN)
    TEST_ASSERT(ch.is_out_active(), "Channel must be active after OUTPUT_MORE start");

    uint8_t  *p_data   = nullptr;
    uint32_t  avail    = 0;
    DmaPullResult r = ch.pull_data(DATA_LEN, &avail, &p_data);

    TEST_ASSERT(r == DmaPullResult::MoreData,   "pull_data must report MoreData");
    TEST_ASSERT(avail == DATA_LEN,               "pull_data must return all requested bytes");
    TEST_ASSERT(p_data != nullptr,               "pull_data must return a non-null pointer");
    TEST_ASSERT(memcmp(p_data, &g_ram[SRC_BASE], DATA_LEN) == 0,
                "pull_data must return the bytes from the OUTPUT_MORE source address");
}

// ---------------------------------------------------------------------------
// 8. INPUT_MORE: push_data copies bytes into the DMA buffer
// ---------------------------------------------------------------------------
static void test_input_push_data() {
    cout << "  test_input_push_data..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t DATA_LEN = 8;
    static const uint8_t SRC[DATA_LEN] = {0x11, 0x22, 0x33, 0x44,
                                           0x55, 0x66, 0x77, 0x88};

    // Fill destination with a known pattern so we can detect it being overwritten
    memset(&g_ram[DST_BASE], 0xFF, DATA_LEN);

    // INPUT_MORE pointing at g_ram[DST_BASE]
    write_cmd(CMD_BASE,      DBDMA_Cmd::INPUT_MORE, 0, DATA_LEN, DST_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,       0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    TEST_ASSERT(ch.is_in_active(), "Channel must be active after INPUT_MORE start");

    int rc = ch.push_data(reinterpret_cast<const char *>(SRC), DATA_LEN);
    TEST_ASSERT(rc == 0, "push_data must succeed");
    TEST_ASSERT(memcmp(&g_ram[DST_BASE], SRC, DATA_LEN) == 0,
                "push_data must copy bytes into the INPUT_MORE destination buffer");
}

// ---------------------------------------------------------------------------
// 9. Interrupt always (i=11): a completed NOP must fire a DMA interrupt.
//    This test validates the fix for the finish_cmd/update_irq bug where
//    cmd_ptr was advanced before interrupt bits were sampled, causing the
//    interrupt bits from the NEXT command to be used instead of the
//    completing command's bits.
// ---------------------------------------------------------------------------
static void test_interrupt_always() {
    cout << "  test_interrupt_always..." << endl;

    DMAChannel ch = make_channel();
    MockIntCtrl mock_int;
    ch.register_dma_int(&mock_int, 0);

    // NOP with i=11 (interrupt always, cmd_bits[5:4] = 0b11 = 0x30)
    write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x30, 0, 0, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0); // no interrupt bits

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Interrupt is posted via TimerManager; fire it now
    TimerManager::get_instance()->process_timers();

    TEST_ASSERT(mock_int.dma_irq_count == 1,
                "NOP with i=always must generate exactly one DMA interrupt");
}

// ---------------------------------------------------------------------------
// 10. Interrupt conditional (i=01, int_select matches): interrupt fires only
//     when the selected status bits match.
// ---------------------------------------------------------------------------
static void test_interrupt_conditional() {
    cout << "  test_interrupt_conditional..." << endl;

    DMAChannel ch = make_channel();
    MockIntCtrl mock_int;
    ch.register_dma_int(&mock_int, 0);

    // i=01 means "interrupt if condition is true"
    // INT_SELECT: mask=0x0001 (bit 0 = S0), compare value=0x0001
    // Set S0 via CH_CTRL so the condition is met
    dma_write(ch, DMAReg::INT_SELECT, (0x0001U << 16) | 0x0001U);
    dma_ctrl(ch, 0x0001, 0x0001); // set status bit S0

    write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x10, 0, 0, 0); // i=01
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    TimerManager::get_instance()->process_timers();

    TEST_ASSERT(mock_int.dma_irq_count == 1,
                "NOP with i=01 must generate interrupt when condition is true");

    // --- now with the condition NOT met (S0 cleared) ---
    DMAChannel ch2 = make_channel();
    MockIntCtrl mock_int2;
    ch2.register_dma_int(&mock_int2, 0);

    dma_write(ch2, DMAReg::INT_SELECT, (0x0001U << 16) | 0x0001U);
    // S0 not set → condition is false

    write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x10, 0, 0, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0);

    dma_set_cmd_ptr(ch2, CMD_BASE);
    dma_start(ch2);

    TimerManager::get_instance()->process_timers();

    TEST_ASSERT(mock_int2.dma_irq_count == 0,
                "NOP with i=01 must not generate interrupt when condition is false");
}

// ---------------------------------------------------------------------------
// 11. Wait condition: w=01 does NOT stall when the condition evaluates false.
//     (Testing the affirmative stall path requires multi-threading because
//     all public command-driving methods loop until active or cmd_in_progress,
//     so we verify only that a false condition lets the channel proceed.)
// ---------------------------------------------------------------------------
static void test_wait_not_triggered() {
    cout << "  test_wait_not_triggered..." << endl;

    DMAChannel ch = make_channel();

    // WAIT_SELECT: mask = S0 (bit 0), compare value = 1.
    // Condition is "(ch_stat & mask) == compare_value" = "(0 & 1) == 1" = false.
    // w=01 means wait only when condition is TRUE, so no stall here.
    dma_write(ch, DMAReg::WAIT_SELECT, (0x0001U << 16) | 0x0001U);

    write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x01, 0, 0, 0); // w=01
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE),
                "w=01 must not stall when the wait condition evaluates false");
}

// ---------------------------------------------------------------------------
// 12. CMD_PTR_LO must be ignored while the channel is ACTIVE
// ---------------------------------------------------------------------------
static void test_cmd_ptr_locked_while_running() {
    cout << "  test_cmd_ptr_locked_while_running..." << endl;

    DMAChannel ch = make_channel();

    // OUTPUT_MORE keeps the channel active with cmd_in_progress=true so
    // start() exits its loop; the channel then awaits pull_data.
    static constexpr uint32_t DATA_LEN = 4;
    for (uint32_t i = 0; i < DATA_LEN; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, DATA_LEN, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Channel is now ACTIVE (awaiting pull_data); attempt to change cmd_ptr
    TEST_ASSERT(ch.is_out_active(), "Channel must be active before cmd_ptr write test");
    dma_write(ch, DMAReg::CMD_PTR_LO, 0xDEAD0000U);

    uint32_t ptr = dma_read(ch, DMAReg::CMD_PTR_LO);
    TEST_ASSERT(ptr == CMD_BASE,
                "CMD_PTR_LO must be ignored when the channel is active/running");

    // Drain the channel so it shuts down cleanly
    uint8_t *p = nullptr; uint32_t avail = 0;
    ch.pull_data(DATA_LEN, &avail, &p);
    ch.end_pull_data();
}

// ---------------------------------------------------------------------------
// 13. xfer_stat written back into the descriptor after command completion
// ---------------------------------------------------------------------------
static void test_xfer_stat_updated() {
    cout << "  test_xfer_stat_updated..." << endl;

    DMAChannel ch = make_channel();

    write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0, 0, 0, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // After NOP completes, descriptor[14..15] must hold (ch_stat | ACTIVE)
    // at the time of completion.  RUN + ACTIVE were set at start.
    DMACmd *d = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
    uint16_t xfer_stat = READ_WORD_LE_A(&d->xfer_stat);

    TEST_ASSERT(xfer_stat != 0xFFFF,
                "xfer_stat sentinel must have been overwritten");
    TEST_ASSERT(xfer_stat & CH_STAT_ACTIVE,
                "xfer_stat must have ACTIVE set at the time the NOP completed");
}

// ---------------------------------------------------------------------------
// 14. res_count written back after a complete transfer (should become 0)
// ---------------------------------------------------------------------------
static void test_res_count_updated() {
    cout << "  test_res_count_updated..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t DATA_LEN = 4;
    for (uint32_t i = 0; i < DATA_LEN; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, DATA_LEN, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Consume all data so finish_cmd runs and writes back res_count
    uint8_t *p = nullptr; uint32_t avail = 0;
    ch.pull_data(DATA_LEN, &avail, &p);
    // After exhausting the buffer, advance the DBDMA program
    ch.end_pull_data();

    DMACmd *d = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
    uint16_t res = READ_WORD_LE_A(&d->res_count);
    TEST_ASSERT(res == 0,
                "res_count must be 0 after all bytes have been transferred");
}

// ---------------------------------------------------------------------------
// 15. Unknown command code (>7) sets DEAD and clears ACTIVE
// ---------------------------------------------------------------------------
static void test_unknown_cmd_sets_dead() {
    cout << "  test_unknown_cmd_sets_dead..." << endl;

    DMAChannel ch = make_channel();

    // cmd value 8 (0x80 in cmd_key high nibble) has no case in interpret_cmd
    write_cmd(CMD_BASE, /*cmd=*/8, 0, 0, 0, 0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(stat & CH_STAT_DEAD,    "Unknown command must set DEAD");
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "Unknown command must clear ACTIVE");
}

// ---------------------------------------------------------------------------
// 16. Clearing RUN while the channel is active: ACTIVE is cleared, stop_cb
//     is called, and cmd_in_progress is reset.
// ---------------------------------------------------------------------------
static void test_abort_clears_active() {
    cout << "  test_abort_clears_active..." << endl;

    DMAChannel ch = make_channel();
    bool stop_cb_called = false;
    ch.set_callbacks(nullptr, [&stop_cb_called]() { stop_cb_called = true; });

    // OUTPUT_MORE keeps the channel active so we can test the abort path
    static constexpr uint32_t DATA_LEN = 8;
    for (uint32_t i = 0; i < DATA_LEN; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, DATA_LEN, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    TEST_ASSERT(ch.is_out_active(), "Channel must be active before abort");

    dma_stop(ch); // clear RUN → abort path

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "ACTIVE must be cleared after abort");
    TEST_ASSERT(!(stat & CH_STAT_DEAD),   "DEAD must not be set by a clean abort");
    TEST_ASSERT(!(stat & CH_STAT_RUN),    "RUN must be cleared after abort");
    TEST_ASSERT(stop_cb_called,           "stop_cb must be invoked on abort");
}

// ---------------------------------------------------------------------------
// 17. pull_data with req_len < queue_len: only req_len bytes are returned,
//     the channel remains active with the residue still queued.
// ---------------------------------------------------------------------------
static void test_pull_data_partial() {
    cout << "  test_pull_data_partial..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t TOTAL = 8;
    static constexpr uint32_t HALF  = TOTAL / 2;
    for (uint32_t i = 0; i < TOTAL; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(0xB0 + i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, TOTAL, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,     0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // First partial pull
    uint8_t *p1 = nullptr; uint32_t avail1 = 0;
    DmaPullResult r1 = ch.pull_data(HALF, &avail1, &p1);

    TEST_ASSERT(r1 == DmaPullResult::MoreData, "First partial pull must return MoreData");
    TEST_ASSERT(avail1 == HALF,                "First partial pull must return exactly HALF bytes");
    TEST_ASSERT(p1 != nullptr,                 "First partial pull must return non-null pointer");
    TEST_ASSERT(memcmp(p1, &g_ram[SRC_BASE], HALF) == 0,
                "First partial pull must return first HALF bytes of source");
    TEST_ASSERT(ch.is_out_active(), "Channel must still be active after partial pull");

    // Second pull consumes the remainder
    uint8_t *p2 = nullptr; uint32_t avail2 = 0;
    DmaPullResult r2 = ch.pull_data(HALF, &avail2, &p2);

    TEST_ASSERT(r2 == DmaPullResult::MoreData, "Second partial pull must return MoreData");
    TEST_ASSERT(avail2 == HALF,                "Second partial pull must return remaining HALF bytes");
    TEST_ASSERT(memcmp(p2, &g_ram[SRC_BASE + HALF], HALF) == 0,
                "Second partial pull must return second HALF bytes of source");

    // Drain so STOP runs
    ch.end_pull_data();
}

// ---------------------------------------------------------------------------
// 18. pull_data returns NoMoreData once the channel has halted (STOP ran)
// ---------------------------------------------------------------------------
static void test_pull_data_no_more_data() {
    cout << "  test_pull_data_no_more_data..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t DATA_LEN = 4;
    for (uint32_t i = 0; i < DATA_LEN; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, DATA_LEN, SRC_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,        0, 0,        0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Consume all data and advance past STOP
    uint8_t *p = nullptr; uint32_t avail = 0;
    ch.pull_data(DATA_LEN, &avail, &p);
    ch.end_pull_data(); // STOP executes, ACTIVE cleared

    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "Channel must be idle after STOP");

    // Next pull_data must immediately report NoMoreData
    uint8_t *p2 = nullptr; uint32_t avail2 = 0xFFFFFFFFU;
    DmaPullResult r = ch.pull_data(DATA_LEN, &avail2, &p2);
    TEST_ASSERT(r == DmaPullResult::NoMoreData,
                "pull_data must return NoMoreData when channel is inactive");
    TEST_ASSERT(avail2 == 0,
                "avail_len must be 0 when pull_data returns NoMoreData");
}

// ---------------------------------------------------------------------------
// 19. Two consecutive OUTPUT_MORE descriptors: pull_data crosses the
//     descriptor boundary correctly, advancing cmd_ptr to the next command.
// ---------------------------------------------------------------------------
static void test_chained_output_more() {
    cout << "  test_chained_output_more..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t LEN1 = 6;
    static constexpr uint32_t LEN2 = 5;

    // Source A: SRC_BASE[0..5]
    for (uint32_t i = 0; i < LEN1; i++)
        g_ram[SRC_BASE + i] = static_cast<uint8_t>(0x10 + i);

    // Source B: SRC_BASE[16..20] (leave gap to avoid overlap with descriptors)
    for (uint32_t i = 0; i < LEN2; i++)
        g_ram[SRC_BASE + 16 + i] = static_cast<uint8_t>(0x20 + i);

    write_cmd(CMD_BASE,      DBDMA_Cmd::OUTPUT_MORE, 0, LEN1, SRC_BASE,      0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::OUTPUT_MORE, 0, LEN2, SRC_BASE + 16, 0);
    write_cmd(CMD_BASE + 32, DBDMA_Cmd::STOP,        0, 0,    0,             0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Pull first chunk
    uint8_t *p1 = nullptr; uint32_t a1 = 0;
    DmaPullResult r1 = ch.pull_data(LEN1, &a1, &p1);
    TEST_ASSERT(r1 == DmaPullResult::MoreData, "First chain pull: MoreData");
    TEST_ASSERT(a1 == LEN1,                    "First chain pull: correct length");
    TEST_ASSERT(memcmp(p1, &g_ram[SRC_BASE], LEN1) == 0,
                "First chain pull: correct data from first descriptor");

    // Pull second chunk — interpret_cmd() must advance across the boundary
    uint8_t *p2 = nullptr; uint32_t a2 = 0;
    DmaPullResult r2 = ch.pull_data(LEN2, &a2, &p2);
    TEST_ASSERT(r2 == DmaPullResult::MoreData,  "Second chain pull: MoreData");
    TEST_ASSERT(a2 == LEN2,                     "Second chain pull: correct length");
    TEST_ASSERT(memcmp(p2, &g_ram[SRC_BASE + 16], LEN2) == 0,
                "Second chain pull: correct data from second descriptor");

    // Drain → STOP, channel halts
    ch.end_pull_data();
    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_ACTIVE), "Channel idle after two chained OUTPUT_MORE + STOP");
}

// ---------------------------------------------------------------------------
// 20. STORE_QUAD 1-byte and 2-byte xfer_size variants.
//
//     req_count & 7 determines xfer_size:
//       bit 2 set → 4 bytes   (already covered by test_store_quad_writes_ram)
//       bit 1 set → 2 bytes
//       else     → 1 byte
// ---------------------------------------------------------------------------
static void test_store_quad_byte_sizes() {
    cout << "  test_store_quad_byte_sizes..." << endl;

    static constexpr uint32_t PAYLOAD = 0x44332211U;

    // ---- 1-byte write (req_count = 1, bit 1 and 2 both clear) ----
    {
        DMAChannel ch = make_channel();
        memset(&g_ram[SCR_BASE], 0x00, 4);

        write_quad_cmd(CMD_BASE,      DBDMA_Cmd::STORE_QUAD, 0, 1, SCR_BASE, PAYLOAD);
        write_cmd     (CMD_BASE + 16, DBDMA_Cmd::STOP,       0, 0, 0,        0);

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch);

        TEST_ASSERT(g_ram[SCR_BASE]     == 0x11, "STORE_QUAD 1-byte must write low byte of cmd_arg");
        TEST_ASSERT(g_ram[SCR_BASE + 1] == 0x00, "STORE_QUAD 1-byte must not touch adjacent byte");
    }

    // ---- 2-byte write (req_count = 2, bit 1 set) ----
    {
        DMAChannel ch = make_channel();
        memset(&g_ram[SCR_BASE], 0x00, 4);

        write_quad_cmd(CMD_BASE,      DBDMA_Cmd::STORE_QUAD, 0, 2, SCR_BASE, PAYLOAD);
        write_cmd     (CMD_BASE + 16, DBDMA_Cmd::STOP,       0, 0, 0,        0);

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch);

        TEST_ASSERT(g_ram[SCR_BASE]     == 0x11, "STORE_QUAD 2-byte: low byte");
        TEST_ASSERT(g_ram[SCR_BASE + 1] == 0x22, "STORE_QUAD 2-byte: next byte");
        TEST_ASSERT(g_ram[SCR_BASE + 2] == 0x00, "STORE_QUAD 2-byte: must not touch byte 2");
    }
}

// ---------------------------------------------------------------------------
// 21. Branch conditional (b=01): branch when condition is TRUE; fall through
//     when condition is FALSE.
//
//     BRANCH_SELECT mask=S0 (0x0001), compare=0x0000.
//     Condition: "(ch_stat & mask) == compare" = "(S0_bit == 0)".
//       S0 clear → condition TRUE  → branch taken
//       S0 set   → condition FALSE → fall through
// ---------------------------------------------------------------------------
static void test_branch_conditional() {
    cout << "  test_branch_conditional..." << endl;

    // ---- Part A: S0 clear → condition true → branch taken, STORE_QUAD skipped ----
    {
        DMAChannel ch = make_channel();
        // BRANCH_SELECT: mask=0x0001, compare=0x0000
        dma_write(ch, DMAReg::BRANCH_SELECT, (0x0001U << 16) | 0x0000U);

        uint32_t sentinel = 0xFEEDFACEU;
        WRITE_DWORD_LE_A(&g_ram[SCR_BASE], sentinel);

        write_quad_cmd(CMD_BASE + 16, DBDMA_Cmd::STORE_QUAD, 0, 4, SCR_BASE, 0xDEADBEEFU);
        write_cmd     (CMD_BASE + 32, DBDMA_Cmd::STOP,       0, 0, 0,        0);

        // NOP with b=01 (0x04), cmd_arg = branch target (skip STORE_QUAD)
        DMACmd *nop = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
        nop->req_count = 0;
        nop->cmd_bits  = 0x04; // b=01
        nop->cmd_key   = (DBDMA_Cmd::NOP << 4);
        nop->address   = 0;
        nop->cmd_arg   = CMD_BASE + 32; // jump to STOP
        nop->res_count = 0;
        nop->xfer_stat = 0;

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch); // S0 is clear → condition true → branch

        TEST_ASSERT(READ_DWORD_LE_A(&g_ram[SCR_BASE]) == sentinel,
                    "b=01 with true condition: STORE_QUAD must be skipped");
        uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
        TEST_ASSERT(!(stat & CH_STAT_ACTIVE),
                    "b=01 with true condition: channel must halt after STOP");
    }

    // ---- Part B: S0 set → condition false → no branch, STORE_QUAD runs ----
    {
        DMAChannel ch = make_channel();
        dma_write(ch, DMAReg::BRANCH_SELECT, (0x0001U << 16) | 0x0000U);
        dma_ctrl(ch, 0x0001, 0x0001); // set S0

        WRITE_DWORD_LE_A(&g_ram[SCR_BASE], 0x00000000U);

        write_quad_cmd(CMD_BASE + 16, DBDMA_Cmd::STORE_QUAD, 0, 4, SCR_BASE, 0xCAFEBABEU);
        write_cmd     (CMD_BASE + 32, DBDMA_Cmd::STOP,       0, 0, 0,        0);

        DMACmd *nop = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
        nop->req_count = 0;
        nop->cmd_bits  = 0x04; // b=01
        nop->cmd_key   = (DBDMA_Cmd::NOP << 4);
        nop->address   = 0;
        nop->cmd_arg   = CMD_BASE + 32; // would-be branch target
        nop->res_count = 0;
        nop->xfer_stat = 0;

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch); // S0 set → condition false → fall through to STORE_QUAD

        TEST_ASSERT(READ_DWORD_LE_A(&g_ram[SCR_BASE]) == 0xCAFEBABEU,
                    "b=01 with false condition: STORE_QUAD must run");
    }
}

// ---------------------------------------------------------------------------
// 22. CH_STAT_BT is set in the descriptor's xfer_stat field when a branch
//     is taken (per DBDMA spec §5.5.3.7: BT reflects branch-taken status).
// ---------------------------------------------------------------------------
static void test_bt_in_xfer_stat() {
    cout << "  test_bt_in_xfer_stat..." << endl;

    DMAChannel ch = make_channel();

    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0, 0, 0, 0);

    // NOP with b=11 (branch always)
    DMACmd *nop = reinterpret_cast<DMACmd *>(&g_ram[CMD_BASE]);
    nop->req_count = 0;
    nop->cmd_bits  = 0x0C; // b=11
    nop->cmd_key   = (DBDMA_Cmd::NOP << 4);
    nop->address   = 0;
    nop->cmd_arg   = CMD_BASE + 16; // branch target = STOP
    nop->res_count = 0;
    nop->xfer_stat = 0;

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    uint16_t xfer_stat = READ_WORD_LE_A(&nop->xfer_stat);
    TEST_ASSERT(xfer_stat & CH_STAT_BT,
                "xfer_stat must have CH_STAT_BT set when a branch was taken");

    // Also verify BT is no longer in ch_stat (it is cleared after the write-back)
    uint32_t stat = dma_read(ch, DMAReg::CH_STAT);
    TEST_ASSERT(!(stat & CH_STAT_BT),
                "CH_STAT_BT must be cleared from ch_stat after the descriptor write-back");
}

// ---------------------------------------------------------------------------
// 23. Interrupt invert mode (i=10): interrupt fires when the condition is
//     FALSE (the complement of i=01 behaviour).
//
//     INT_SELECT mask=S0 (0x0001), compare=0x0001.
//     Condition: "(ch_stat & mask) == compare" = "(S0_bit == 1)".
//     With i=10, interrupt fires when condition is FALSE, i.e. when S0 is clear.
// ---------------------------------------------------------------------------
static void test_interrupt_invert() {
    cout << "  test_interrupt_invert..." << endl;

    // ---- Part A: S0 clear → condition false → inverted → fire interrupt ----
    {
        DMAChannel ch = make_channel();
        MockIntCtrl mock;
        ch.register_dma_int(&mock, 0);

        dma_write(ch, DMAReg::INT_SELECT, (0x0001U << 16) | 0x0001U);
        // S0 is not set; condition "(S0==1)" is false; i=10 fires when false

        write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x20, 0, 0, 0); // i=10
        write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0);

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch);
        TimerManager::get_instance()->process_timers();

        TEST_ASSERT(mock.dma_irq_count == 1,
                    "i=10 must fire interrupt when condition is false (S0 clear)");
    }

    // ---- Part B: S0 set → condition true → inverted → no interrupt ----
    {
        DMAChannel ch = make_channel();
        MockIntCtrl mock;
        ch.register_dma_int(&mock, 0);

        dma_write(ch, DMAReg::INT_SELECT, (0x0001U << 16) | 0x0001U);
        dma_ctrl(ch, 0x0001, 0x0001); // set S0 → condition "(S0==1)" is true → inverted → silent

        write_cmd(CMD_BASE,      DBDMA_Cmd::NOP,  0x20, 0, 0, 0); // i=10
        write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP, 0x00, 0, 0, 0);

        dma_set_cmd_ptr(ch, CMD_BASE);
        dma_start(ch);
        TimerManager::get_instance()->process_timers();

        TEST_ASSERT(mock.dma_irq_count == 0,
                    "i=10 must NOT fire interrupt when condition is true (S0 set)");
    }
}

// ---------------------------------------------------------------------------
// 24. push_data with len < queue_len: only len bytes are written, the
//     remainder of the buffer is untouched, and the channel stays active.
// ---------------------------------------------------------------------------
static void test_push_data_partial() {
    cout << "  test_push_data_partial..." << endl;

    DMAChannel ch = make_channel();

    static constexpr uint32_t TOTAL = 8;
    static constexpr uint32_t HALF  = TOTAL / 2;

    static const uint8_t FIRST_HALF[HALF]  = {0xAA, 0xBB, 0xCC, 0xDD};
    static const uint8_t SECOND_HALF[HALF] = {0x11, 0x22, 0x33, 0x44};

    // Fill destination with a canary pattern
    memset(&g_ram[DST_BASE], 0xEE, TOTAL);

    write_cmd(CMD_BASE,      DBDMA_Cmd::INPUT_MORE, 0, TOTAL, DST_BASE, 0);
    write_cmd(CMD_BASE + 16, DBDMA_Cmd::STOP,       0, 0,     0,        0);

    dma_set_cmd_ptr(ch, CMD_BASE);
    dma_start(ch);

    // Push first half only
    int rc = ch.push_data(reinterpret_cast<const char *>(FIRST_HALF), HALF);
    TEST_ASSERT(rc == 0, "Partial push_data must succeed");
    TEST_ASSERT(memcmp(&g_ram[DST_BASE], FIRST_HALF, HALF) == 0,
                "Partial push must write first HALF bytes to destination");
    TEST_ASSERT(memcmp(&g_ram[DST_BASE + HALF],
                       "\xEE\xEE\xEE\xEE", HALF) == 0,
                "Partial push must leave remainder of buffer untouched (canary intact)");
    TEST_ASSERT(ch.is_in_active(),
                "Channel must remain active after partial push");

    // Push second half to fill the buffer
    rc = ch.push_data(reinterpret_cast<const char *>(SECOND_HALF), HALF);
    TEST_ASSERT(rc == 0, "Second partial push must succeed");
    TEST_ASSERT(memcmp(&g_ram[DST_BASE + HALF], SECOND_HALF, HALF) == 0,
                "Second partial push must write remaining bytes");

    ch.end_push_data();
}

// ---------------------------------------------------------------------------
// Setup: one-time initialisation of memory controller and TimerManager
// ---------------------------------------------------------------------------
static void setup() {
    // Memory controller backed by g_ram
    g_mem_ctrl = new MemCtrlBase();
    g_mem_ctrl->add_ram_region(RAM_BASE, RAM_SIZE, g_ram);
    mem_ctrl_instance = g_mem_ctrl;

    // TimerManager needs time/notify callbacks
    auto *tm = TimerManager::get_instance();
    tm->set_time_now_cb([]() -> uint64_t { return g_fake_time_ns; });
    tm->set_notify_changes_cb([]() {});
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    setup();

    cout << "Running DBDMA tests..." << endl;

    test_ch_ctrl_reads_zero();
    test_stop_halts_channel();
    test_nop_stop();
    test_store_quad_writes_ram();
    test_load_quad_reads_ram();
    test_branch_always();
    test_output_pull_data();
    test_input_push_data();
    test_interrupt_always();
    test_interrupt_conditional();
    test_wait_not_triggered();
    test_cmd_ptr_locked_while_running();
    test_xfer_stat_updated();
    test_res_count_updated();
    test_unknown_cmd_sets_dead();
    test_abort_clears_active();
    test_pull_data_partial();
    test_pull_data_no_more_data();
    test_chained_output_more();
    test_store_quad_byte_sizes();
    test_branch_conditional();
    test_bt_in_xfer_stat();
    test_interrupt_invert();
    test_push_data_partial();

    cout << tests_run    << " tests run, "
         << tests_failed << " failed." << endl;

    return tests_failed ? 1 : 0;
}
