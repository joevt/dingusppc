/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

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

#include <core/timermanager.h>
#include <loguru.hpp>
#include "ppcemu.h"
#include "ppcmmu.h"
#include "ppcdisasm.h"
#include <debugger/backtrace.h>
#include <debugger/symbols.h>
#include <memaccess.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <setjmp.h>
#include <stdexcept>
#include <stdio.h>
#include <string>

#ifdef __APPLE__
#include <mach/mach_time.h>
#undef EXC_SYSCALL
static struct mach_timebase_info timebase_info;
static uint64_t
ConvertHostTimeToNanos2(uint64_t host_time)
{
    if (timebase_info.numer == timebase_info.denom)
        return host_time;
    long double answer = host_time;
    answer *= timebase_info.numer;
    answer /= timebase_info.denom;
    return (uint64_t)answer;
}
#endif

using namespace std;
using namespace dppc_interpreter;

MemCtrlBase* mem_ctrl_instance = 0;

bool is_601 = false;

bool power_on = false;
Po_Cause power_off_reason = po_enter_debugger;

SetPRS ppc_state;
#ifdef LOG_INSTRUCTIONS
uint32_t pcp;
#define ATPCP , &pcp
#else
#define ATPCP
#endif

uint32_t ppc_cur_instruction;    // Current instruction for the PPC
uint32_t ppc_next_instruction_address;    // Used for branching, setting up the NIA

unsigned exec_flags; // execution control flags
// FIXME: exec_timer is read by main thread ppc_main_opcode;
// written by audio dbdma DMAChannel::update_irq .. add_immediate_timer
volatile bool exec_timer;
bool int_pin = false; // interrupt request pin state: true - asserted
bool dec_exception_pending = false;

/* copy of local variable bb_start_la. Need for correct
   calculation of CPU cycles after setjmp that clobbers
   non-volatile local variables. */
uint32_t    glob_bb_start_la;

/* variables related to virtual time */
const bool g_realtime = false;
uint64_t g_nanoseconds_base;
uint64_t g_icycles;
int      icnt_factor;

/* global variables related to the timebase facility */
uint64_t tbr_wr_timestamp;  // stores vCPU virtual time of the last TBR write
uint64_t rtc_timestamp;     // stores vCPU virtual time of the last RTC write
uint64_t tbr_wr_value;      // last value written to the TBR
uint32_t tbr_freq_ghz;      // TBR/RTC driving frequency in GHz expressed as a
                            // 32 bit fraction less than 1.0 (999.999999 MHz maximum).
uint32_t tbr_freq_shift;    // If 32 bits is not sufficient, then include a shift.
uint64_t tbr_period_ns;     // TBR/RTC period in ns expressed as a 64 bit value
                            // with 32 fractional bits (<1 Hz minimum).
uint64_t timebase_counter;  // internal timebase counter
uint64_t dec_wr_timestamp;  // stores vCPU virtual time of the last DEC write
uint32_t dec_wr_value;      // last value written to the DEC register
uint32_t rtc_lo;            // MPC601 RTC lower, counts nanoseconds
uint32_t rtc_hi;            // MPC601 RTC upper, counts seconds

#ifdef CPU_PROFILING

/* global variables for lightweight CPU profiling */
uint64_t num_executed_instrs;
uint64_t num_supervisor_instrs;
uint64_t num_int_loads;
uint64_t num_int_stores;
uint64_t exceptions_processed;
#ifdef CPU_PROFILING_OPS
std::unordered_map<uint32_t, uint64_t> num_opcodes;
#endif

#include "utils/profiler.h"
#include <memory>

class CPUProfile : public BaseProfile {
public:
    CPUProfile() : BaseProfile("PPC_CPU") {};

    void populate_variables(std::vector<ProfileVar>& vars) {
        vars.clear();

        vars.push_back({.name = "Executed Instructions Total",
                        .format = ProfileVarFmt::DEC,
                        .value = num_executed_instrs});

        vars.push_back({.name = "Executed Supervisor Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_supervisor_instrs});

        vars.push_back({.name = "Integer Load Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_int_loads});

        vars.push_back({.name = "Integer Store Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_int_stores});

        vars.push_back({.name = "Exceptions processed",
                        .format = ProfileVarFmt::DEC,
                        .value = exceptions_processed});

        // Generate top N op counts with readable names.
#ifdef CPU_PROFILING_OPS
        PPCDisasmContext ctx;
        ctx.instr_addr = 0;
        ctx.simplified = false;
        std::vector<std::pair<std::string, uint64_t>> op_name_counts;
        for (const auto& pair : num_opcodes) {
            ctx.instr_code = pair.first;
            auto op_name = disassemble_single(&ctx);
            op_name_counts.emplace_back(op_name, pair.second);
        }
        size_t top_ops_size = std::min(op_name_counts.size(), size_t(20));
        std::partial_sort(op_name_counts.begin(), op_name_counts.begin() + top_ops_size, op_name_counts.end(), [](const auto& a, const auto& b) {
            return b.second < a.second;
        });
        op_name_counts.resize(top_ops_size);
        for (const auto& pair : op_name_counts) {
            vars.push_back({.name = "Instruction " + pair.first,
                            .format = ProfileVarFmt::COUNT,
                            .value = pair.second,
                            .count_total = num_executed_instrs});
        }
#endif
    };

    void reset() {
        num_executed_instrs = 0;
        num_supervisor_instrs = 0;
        num_int_loads = 0;
        num_int_stores = 0;
        exceptions_processed = 0;
#ifdef CPU_PROFILING_OPS
        num_opcodes.clear();
#endif
    };
};

#endif

#ifdef LOG__doprnt
static bool try_doprint = false;
static uint32_t addr_doprint = 0;
static uint32_t addr_putc = 0;
#endif

#ifdef LOG_INSTRUCTIONS
InstructionRec InstructionLog[InstructionLogSize] = {0};
uint64_t InstructionNumber = 0;
#endif

/** Opcode lookup tables. */

/** Primary opcode (bits 0...5) lookup table. */
static PPCOpcode OpcodeGrabber[64];

/** Lookup tables for branch instructions. */
const static PPCOpcode SubOpcode16Grabber[] = {
    dppc_interpreter::ppc_bc<LK0, AA0>,     // bc
    dppc_interpreter::ppc_bc<LK1, AA0>,     // bcl
    dppc_interpreter::ppc_bc<LK0, AA1>,     // bca
    dppc_interpreter::ppc_bc<LK1, AA1>};    // bcla

const static PPCOpcode SubOpcode18Grabber[] = {
    dppc_interpreter::ppc_b<LK0, AA0>,      // b
    dppc_interpreter::ppc_b<LK1, AA0>,      // bl
    dppc_interpreter::ppc_b<LK0, AA1>,      // ba
    dppc_interpreter::ppc_b<LK1, AA1>};     // bla

/** Instructions decoding tables for integer,
    single floating-point, and double-floating point ops respectively */

static PPCOpcode SubOpcode31Grabber[2048];
static PPCOpcode SubOpcode59Grabber[64];
static PPCOpcode SubOpcode63Grabber[2048];

/** Exception helpers. */

void ppc_illegalop() {
    if (ppc_state.pc == 0xFF809A64 && ppc_cur_instruction == 0) {
        // back-to-MacOS from cientry in OF 1.0.5
    }
    else {
        //LOG_F(ERROR, "Illegal Operation 0x%08x", ppc_cur_instruction);
        //dump_backtrace();
    }
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
}

void ppc_fpu_off() {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::FPU_OFF);
}

void ppc_assert_int() {
    int_pin = true;
    if (ppc_state.msr & MSR::EE) {
        LOG_F(5, "CPU ExtIntHandler called");
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
    } else {
        LOG_F(5, "CPU IRQ ignored!");
    }
}

void ppc_release_int() {
    int_pin = false;
}

/** Opcode decoding functions. */

void ppc_opcode4() {
#ifndef DPPC_ALTIVEC
    ppc_illegalop();
#else
    if (ppc_cur_instruction & 32) {
        // bit 5 is one means bits 6 to 10 are vC
        // could be reduced from 5 bits to 4 bits since bit 4 is always zero
        // but then you would have to test bit 4.
        uint16_t subop_grab = ppc_cur_instruction & 31;
        switch (subop_grab) {
            case  0: altivec_vmhaddshs(); break;
            case  1: altivec_vmhraddshs(); break;
            case  2: altivec_vmladduhm(); break;
            case  4: altivec_vmsumubm(); break;
            case  5: altivec_vmsummbm(); break;
            case  6: altivec_vmsumuhm(); break;
            case  7: altivec_vmsumuhs(); break;
            case  8: altivec_vmsumshm(); break;
            case  9: altivec_vmsumshs(); break;
            case 10: altivec_vsel(); break;
            case 11: altivec_vperm(); break;
            case 12: altivec_vsldoi(); break;
            case 14: altivec_vmaddfp(); break;
            case 15: altivec_vnmsubfp(); break;
            default: ppc_illegalop();
        }
    } else if (ppc_cur_instruction & 15 == 6) {
        // could be reduced from 10 bits to 4 bits since bits 0 to 3 are always 6 and bits 4 and 5 are always zero
        // but then you would have to test bits 4 and 5.
        uint16_t subop_grab = ppc_cur_instruction & 0x3ff; // bit 10 is Rc
        switch (subop_grab) {
            case    6: altivec_vcmpequbx(); break;
            case   70: altivec_vcmpequhx(); break;
            case  134: altivec_vcmpequwx(); break;
            case  198: altivec_vcmpeqfpx(); break;
            case  454: altivec_vcmpgefpx(); break;
            case  518: altivec_vcmpgtubx(); break;
            case  582: altivec_vcmpgtuhx(); break;
            case  646: altivec_vcmpgtuwx(); break;
            case  710: altivec_vcmpgtfpx(); break;
            case  774: altivec_vcmpgtsbx(); break;
            case  838: altivec_vcmpgtshx(); break;
            case  902: altivec_vcmpgtswx(); break;
            case  966: altivec_vcmpbfpx(); break;
            default  : ppc_illegalop();
        }
    } else {
        // could be reduced from 11 bits to 9 bits since bit 4 and 5 are always zero
        // but then you would have to test bits 4 and 5.
        uint16_t subop_grab = ppc_cur_instruction & 0x7ff;
        switch subop_grab:
            case    0: altivec_vaddubm(); break;
            case    2: altivec_vmaxub(); break;
            case    4: altivec_vrlb(); break;
            case    8: altivec_vmuloub(); break;
            case   10: altivec_vaddfp(); break;
            case   12: altivec_vmrghb(); break;
            case   14: altivec_vpkuhum(); break;
            case   64: altivec_vadduhm(); break;
            case   66: altivec_vmaxuh(); break;
            case   68: altivec_vrlh(); break;
            case   72: altivec_vmulouh(); break;
            case   74: altivec_vsubfp(); break;
            case   76: altivec_vmrghh(); break;
            case   78: altivec_vpkuwum(); break;
            case  128: altivec_vadduwm(); break;
            case  130: altivec_vmaxuw(); break;
            case  132: altivec_vrlw(); break;
            case  140: altivec_vmrghw(); break;
            case  142: altivec_vpkuhus(); break;
            case  206: altivec_vpkuwus(); break;
            case  258: altivec_vmaxsb(); break;
            case  260: altivec_vslb(); break;
            case  264: altivec_vmulosb(); break;
            case  266: altivec_vrefp(); break;
            case  268: altivec_vmrglb(); break;
            case  270: altivec_vpkshus(); break;
            case  322: altivec_vmaxsh(); break;
            case  324: altivec_vslh(); break;
            case  328: altivec_vmulosh(); break;
            case  330: altivec_vrsqrtefp(); break;
            case  332: altivec_vmrglh(); break;
            case  334: altivec_vpkswus(); break;
            case  384: altivec_vaddcuw(); break;
            case  386: altivec_vmaxsw(); break;
            case  388: altivec_vslw(); break;
            case  394: altivec_vexptefp(); break;
            case  396: altivec_vmrglw(); break;
            case  398: altivec_vpkshss(); break;
            case  452: altivec_vsl(); break;
            case  458: altivec_vlogefp(); break;
            case  462: altivec_vpkswss(); break;
            case  512: altivec_vaddubs(); break;
            case  514: altivec_vminub(); break;
            case  516: altivec_vsrb(); break;
            case  520: altivec_vmuleub(); break;
            case  522: altivec_vrfin(); break;
            case  524: altivec_vspltb(); break;
            case  526: altivec_vupkhsb(); break;
            case  576: altivec_vadduhs(); break;
            case  578: altivec_vminuh(); break;
            case  580: altivec_vsrh(); break;
            case  584: altivec_vmuleuh(); break;
            case  586: altivec_vrfiz(); break;
            case  588: altivec_vsplth(); break;
            case  590: altivec_vupkhsh(); break;
            case  640: altivec_vadduws(); break;
            case  642: altivec_vminuw(); break;
            case  644: altivec_vsrw(); break;
            case  650: altivec_vrfip(); break;
            case  652: altivec_vspltw(); break;
            case  654: altivec_vupklsb(); break;
            case  708: altivec_vsr(); break;
            case  714: altivec_vrfim(); break;
            case  718: altivec_vupklsh(); break;
            case  768: altivec_vaddsbs(); break;
            case  770: altivec_vminsb(); break;
            case  772: altivec_vsrab(); break;
            case  776: altivec_vmulesb(); break;
            case  778: altivec_vcfux(); break;
            case  780: altivec_vspltisb(); break;
            case  782: altivec_vpkpx(); break;
            case  832: altivec_vaddshs(); break;
            case  834: altivec_vminsh(); break;
            case  836: altivec_vsrah(); break;
            case  840: altivec_vmulesh(); break;
            case  842: altivec_vcfsx(); break;
            case  844: altivec_vspltish(); break;
            case  846: altivec_vupkhpx(); break;
            case  896: altivec_vaddsws(); break;
            case  898: altivec_vminsw(); break;
            case  900: altivec_vsraw(); break;
            case  906: altivec_vctuxs(); break;
            case  908: altivec_vspltisw(); break;
            case  970: altivec_vctsxs(); break;
            case  974: altivec_vupklpx(); break;
            case 1024: altivec_vsububm(); break;
            case 1026: altivec_vavgub(); break;
            case 1028: altivec_vand(); break;
            case 1034: altivec_vmaxfp(); break;
            case 1036: altivec_vslo(); break;
            case 1088: altivec_vsubuhm(); break;
            case 1090: altivec_vavguh(); break;
            case 1092: altivec_vandc(); break;
            case 1098: altivec_vminfp(); break;
            case 1100: altivec_vsro(); break;
            case 1152: altivec_vsubuwm(); break;
            case 1154: altivec_vavguw(); break;
            case 1156: altivec_vor(); break;
            case 1220: altivec_vxor(); break;
            case 1282: altivec_vavgsb(); break;
            case 1284: altivec_vnor(); break;
            case 1346: altivec_vavgsh(); break;
            case 1408: altivec_vsubcuw(); break;
            case 1410: altivec_vavgsw(); break;
            case 1536: altivec_vsububs(); break;
            case 1540: altivec_mfvscr(); break;
            case 1544: altivec_vsum4ubs(); break;
            case 1600: altivec_vsubuhs(); break;
            case 1604: altivec_mtvscr(); break;
            case 1608: altivec_vsum4shs(); break;
            case 1664: altivec_vsubuws(); break;
            case 1672: altivec_vsum2sws(); break;
            case 1792: altivec_vsubsbs(); break;
            case 1800: altivec_vsum4sbs(); break;
            case 1856: altivec_vsubshs(); break;
            case 1920: altivec_vsubsws(); break;
            case 1928: altivec_vsumsws(); break;
            default  : illegalop();
        }
    }
#endif
}

void ppc_opcode16() {
    SubOpcode16Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode18() {
    SubOpcode18Grabber[ppc_cur_instruction & 3]();
}

template<field_601 for601>
void ppc_opcode19() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FF;

    switch (subop_grab) {
    case 0:
        ppc_mcrf();
        break;
    case 32:
        ppc_bclr<LK0>();
        break;
    case 33:
        ppc_bclr<LK1>();
        break;
    case 66:
        ppc_crnor();
        break;
    case 100:
        ppc_rfi();
        break;
    case 258:
        ppc_crandc();
        break;
    case 300:
        ppc_isync();
        break;
    case 386:
        ppc_crxor();
        break;
    case 450:
        ppc_crnand();
        break;
    case 514:
        ppc_crand();
        break;
    case 578:
        ppc_creqv();
        break;
    case 834:
        ppc_crorc();
        break;
    case 898:
        ppc_cror();
        break;
    case 1056:
        ppc_bcctr<LK0, for601>();
        break;
    case 1057:
        ppc_bcctr<LK1, for601>();
        break;
    default:
        ppc_illegalop();
    }
}

template void ppc_opcode19<NOT601>();
template void ppc_opcode19<IS601>();

void ppc_opcode31() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FFUL;
    SubOpcode31Grabber[subop_grab]();
}

void ppc_opcode59() {
    uint16_t subop_grab = ppc_cur_instruction & 0x3FUL;
    SubOpcode59Grabber[subop_grab]();
}

void ppc_opcode63() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FFUL;
    SubOpcode63Grabber[subop_grab]();
}

/* Dispatch using main opcode */
void ppc_main_opcode()
{
#ifdef CPU_PROFILING
    num_executed_instrs++;
#if defined(CPU_PROFILING_OPS)
    num_opcodes[ppc_cur_instruction]++;
#endif
#endif

#ifdef LOG_INSTRUCTIONS
    if (InstructionNumber && !InstructionLog[(InstructionNumber - 1) & (InstructionLogSize - 1)].flags_after) {
        // This happens for all exceptions except EXC_EXT_INT && EXC_DECR
        //LOG_F(ERROR, "previous instruction did not complete");
    }
    InstructionRec * irec = &InstructionLog[InstructionNumber & (InstructionLogSize - 1)];
    irec->cycle = InstructionNumber++;
    irec->addr = ppc_state.pc;
    irec->paddr = pcp;
    irec->ins = ppc_cur_instruction;
    irec->msr = ppc_state.msr;
    irec->flags_before = exec_flags | (exec_timer << 7);
    irec->flags_after = 0;
#endif

#ifdef LOG__doprnt
        if (try_doprint) {
            if (ppc_state.pc == addr_doprint) {
                addr_putc = ppc_state.gpr[5];
                /*
                std::string name = get_name(addr_putc);
                printf("\n__doprnt(%s):", name.c_str());
                */
            }
            if (ppc_state.pc == addr_putc) {
                printf("%c", ppc_state.gpr[3]);
            }
        }
#endif

    OpcodeGrabber[(ppc_cur_instruction >> 26) & 0x3F]();
#ifdef LOG_INSTRUCTIONS
    irec->flags_after = exec_flags | (exec_timer << 7) | 0x80000000;
    irec->msr_after = ppc_state.msr;
#endif
}

static long long cpu_now_ns() {
#ifdef __APPLE__
    return ConvertHostTimeToNanos2(mach_absolute_time());
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#endif
}

uint64_t get_virt_time_ns()
{
    if (g_realtime) {
        return cpu_now_ns() - g_nanoseconds_base;
    } else {
        return g_icycles << icnt_factor;
    }
}

static uint64_t process_events()
{
    exec_timer = false;
    uint64_t slice_ns = TimerManager::get_instance()->process_timers();
    if (slice_ns == 0) {
        // execute 25.000 cycles
        // if there are no pending timers
        return g_icycles + 25000;
    }
    return g_icycles + (slice_ns >> icnt_factor) + 1;
}

static void force_cycle_counter_reload()
{
    // tell the interpreter loop to reload cycle counter
    exec_timer = true;
}

/** Execute PPC code as long as power is on. */
// inner interpreter loop
static void ppc_exec_inner()
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;
    bool msr_le;

    max_cycles = 0;

    while (power_on) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start ATPCP); // &pcp

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {
            ppc_main_opcode();
            msr_le = (ppc_state.msr & MSR::LE) != 0;
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    if (msr_le)
                        pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                    pcp += (int)eb_start - (int)ppc_state.pc;
#endif
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                if (msr_le)
                    pc_real = mmu_translate_imem(ppc_state.pc ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                pcp += 4;
#endif
                ppc_set_cur_instruction(pc_real);
            }
        }
    }
}

// outer interpreter loop
void ppc_exec()
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    while (power_on) {
        ppc_exec_inner();
    }
}

/** Execute one PPC instruction. */
void ppc_exec_single()
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
        exec_flags = 0;
        return;
    }

    mmu_translate_imem(ppc_state.pc ATPCP); // &pcp
    ppc_main_opcode();
    g_icycles++;
    process_events();

    if (exec_flags) {
        ppc_state.pc = ppc_next_instruction_address;
        exec_flags = 0;
    } else {
        ppc_state.pc += 4;
    }
}

#ifdef WATCH_POINT
static uint32_t watch_point_value = 0x01234567;
static uint32_t watch_point_address = 0x30B404;
uint32_t *watch_point_dma = nullptr;
bool got_watch_point_value = false;
static uint32_t watch_point_paddr = watch_point_address;
#endif

#ifdef LOG_TAG
static uint32_t lastphystag = -2;
static int gotCallKernel = 0;
#endif

/** Execute PPC code until goal_addr is reached. */

// inner interpreter loop
static void ppc_exec_until_inner(const uint32_t goal_addr)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;
    bool msr_le;

    max_cycles = 0;

    do {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start ATPCP); // &pcp

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {

#ifdef LOG_TAG
            if (ppc_state.pc == 0x1c01e74) // CallKernel
                gotCallKernel = true;
            else if (ppc_state.pc == 0x00083F3C) // cpu_init
                gotCallKernel = false;

            if (gotCallKernel) {
                mmu_exception_handler = dbg_exception_handler;
                uint32_t saved_msr = ppc_state.msr;
                ppc_state.msr = (saved_msr & ~(1<<14)) | (1<<4); // mode 2 supervisor
                mmu_change_mode();
                try {
                    TLBEntry *tlb2_entry = dtlb2_refill(0x0030b000, 0, true);
                    if (tlb2_entry) {
                        if (tlb2_entry->tag == 0x0030b000 && tlb2_entry->phys_tag != lastphystag) {
                            LOG_F(ERROR, "translation changed: mode:2 tag:0x%08x phys_tag:0x%08x", tlb2_entry->tag, tlb2_entry->phys_tag);
                            dump_backtrace();
                            lastphystag = tlb2_entry->phys_tag;
                        }
                        tlb2_entry->tag = -1;
                    }
                }
                catch (...) {
                }
                ppc_state.msr = saved_msr;
                mmu_change_mode();
                mmu_exception_handler = ppc_exception_handler;
            }
#endif

#ifdef WATCH_POINT
            uint32_t paddr;
            if (mmu_translate_dbg(watch_point_address, paddr)) {
                if (paddr != watch_point_paddr) {
                    LOG_F(ERROR, "cpu_type guest_pa changed from %08x to %08x", watch_point_paddr, paddr);
                    watch_point_paddr = paddr;
                    if (paddr != watch_point_address) {
                        dump_backtrace();
                    }
                }
            }

            if (got_watch_point_value) {
                do {
                    uint32_t cur_value;
                    try {
                        if (watch_point_dma) {
                            cur_value = READ_DWORD_BE_A(watch_point_dma);
                        }
                        else {
                            break;
                        }
                    }
                    catch (...) {
                        break;
                    }

                    if (cur_value != watch_point_value) {
                        LOG_F(ERROR, "1 cpu_type Watch point at 0x%08x changed from 0x%08x to 0x%08x", watch_point_address, watch_point_value, cur_value);
                        if (cur_value != 0x12) {
                            dump_backtrace();
                            uint32_t save_cur_value = cur_value;
                            try {
                                cur_value = (uint32_t)mem_read_dbg(watch_point_address, 4);
                            }
                            catch (...) {
                                break;
                            }
                            if (cur_value != watch_point_value) {
                                if (cur_value != save_cur_value) {
                                    LOG_F(ERROR, "2 cpu_type Watch point at 0x%08x changed from 0x%08x to 0x%08x", watch_point_address, save_cur_value, cur_value);
                                }
                                watch_point_value = cur_value;
                                // dump_backtrace();
                            }
                        }
                        watch_point_value = cur_value;
                    }
                } while (0);
            }
#endif

            ppc_main_opcode();
            msr_le = (ppc_state.msr & MSR::LE) != 0;
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    if (msr_le)
                        pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                    pcp += (int)eb_start - (int)ppc_state.pc;
#endif
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                if (msr_le)
                    pc_real = mmu_translate_imem(ppc_state.pc ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                pcp += 4;
#endif
                ppc_set_cur_instruction(pc_real);
            }

            if (ppc_state.pc == goal_addr)
                break;
        }
    } while (power_on && ppc_state.pc != goal_addr);
}

// outer interpreter loop
void ppc_exec_until(volatile uint32_t goal_addr)
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    do {
        ppc_exec_until_inner(goal_addr);
    } while (power_on && ppc_state.pc != goal_addr);
}

/** Execute PPC code until control is reached the specified region. */

// inner interpreter loop
static void ppc_exec_dbg_inner(const uint32_t start_addr, const uint32_t size)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;
    bool msr_le;

    max_cycles = 0;

    while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start ATPCP); // &pcp

        // interpret execution block
        while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)
                && (ppc_state.pc < eb_end)) {
            ppc_main_opcode();
            msr_le = (ppc_state.msr & MSR::LE) != 0;
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    if (msr_le)
                        pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                    pcp += (int)eb_start - (int)ppc_state.pc;
#endif
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start ATPCP); // &pcp
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                if (msr_le)
                    pc_real = mmu_translate_imem(ppc_state.pc ATPCP); // &pcp
#ifdef LOG_INSTRUCTIONS
                pcp += 4;
#endif
                ppc_set_cur_instruction(pc_real);
            }
        }
    }
}

// outer interpreter loop
void ppc_exec_dbg(volatile uint32_t start_addr, volatile uint32_t size)
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)) {
        ppc_exec_dbg_inner(start_addr, size);
    }
}

/*
Opcode table macros:
- d is for dot (RC).
- o is for overflow (OV).
- c is for carry CARRY0/CARRY1. It also works for other options:
  SHFT0/SHFT1, RIGHT0/LEFT1, uint8_t/uint16_t/uint32_t, and int8_t/int16_t.
 */

#define OP(opcode, fn) \
do { \
    OpcodeGrabber[opcode] = fn; \
} while (0)

#define OPX(opcode, subopcode, fn) \
do { \
    opcode ## Grabber[((subopcode)<<1)] = fn; \
} while (0)

#define OPXd(opcode, subopcode, fn) \
do { \
    opcode ## Grabber[((subopcode)<<1)] = fn<RC0>; \
    opcode ## Grabber[((subopcode)<<1)+1] = fn<RC1>; \
} while (0)

#define OPXod(opcode, subopcode, fn) \
do { \
    opcode ## Grabber[((subopcode)<<1)] = fn<RC0, OV0>; \
    opcode ## Grabber[((subopcode)<<1)+1] = fn<RC1, OV0>; \
    opcode ## Grabber[1024+((subopcode)<<1)] = fn<RC0, OV1>; \
    opcode ## Grabber[1024+((subopcode)<<1)+1] = fn<RC1, OV1>; \
} while (0)

#define OPXdc(opcode, subopcode, fn, carry) \
do { \
    opcode ## Grabber[((subopcode)<<1)] = fn<carry, RC0>; \
    opcode ## Grabber[((subopcode)<<1)+1] = fn<carry, RC1>; \
} while (0)

#define OPXcod(opcode, subopcode, fn, carry) \
do { \
    opcode ## Grabber[((subopcode)<<1)] = fn<carry, RC0, OV0>; \
    opcode ## Grabber[((subopcode)<<1)+1] = fn<carry, RC1, OV0>; \
    opcode ## Grabber[1024+((subopcode)<<1)] = fn<carry, RC0, OV1>; \
    opcode ## Grabber[1024+((subopcode)<<1)+1] = fn<carry, RC1, OV1>; \
} while (0)

#define OP31(subopcode, fn) OPX(SubOpcode31, subopcode, fn)
#define OP31d(subopcode, fn) OPXd(SubOpcode31, subopcode, fn)
#define OP31od(subopcode, fn) OPXod(SubOpcode31, subopcode, fn)
#define OP31dc(subopcode, fn, carry) OPXdc(SubOpcode31, subopcode, fn, carry)
#define OP31cod(subopcode, fn, carry) OPXcod(SubOpcode31, subopcode, fn, carry)

#define OP59d(subopcode, fn) OPXd(SubOpcode59, subopcode, fn)

#define OP63(subopcode, fn) OPX(SubOpcode63, subopcode, fn)
#define OP63d(subopcode, fn) OPXd(SubOpcode63, subopcode, fn)
#define OP63dc(subopcode, fn, carry) OPXdc(SubOpcode63, subopcode, fn, carry)

void initialize_ppc_opcode_tables(bool include_601) {
    std::fill_n(OpcodeGrabber, 64, ppc_illegalop);
    OP(3,  ppc_twi);
    OP(4,  ppc_opcode4);
    OP(7,  ppc_mulli);
    OP(8,  ppc_subfic);
    if (is_601 || include_601) OP(9, power_dozi);
    OP(10, ppc_cmpli);
    OP(11, ppc_cmpi);
    OP(12, ppc_addic<RC0>);
    OP(13, ppc_addic<RC1>);
    OP(14, ppc_addi<SHFT0>);
    OP(15, ppc_addi<SHFT1>);
    OP(16, ppc_opcode16);
    OP(17, ppc_sc);
    OP(18, ppc_opcode18);
    if (is_601) OP(19, ppc_opcode19<IS601>); else OP(19, ppc_opcode19<NOT601>);
    OP(20, ppc_rlwimi);
    OP(21, ppc_rlwinm);
    if (is_601 || include_601) OP(22, power_rlmi);
    OP(23, ppc_rlwnm);
    OP(24, ppc_ori<SHFT0>);
    OP(25, ppc_ori<SHFT1>);
    OP(26, ppc_xori<SHFT0>);
    OP(27, ppc_xori<SHFT1>);
    OP(28, ppc_andirc<SHFT0>);
    OP(29, ppc_andirc<SHFT1>);
    OP(31, ppc_opcode31);
    OP(32, ppc_lz<uint32_t>);
    OP(33, ppc_lzu<uint32_t>);
    OP(34, ppc_lz<uint8_t>);
    OP(35, ppc_lzu<uint8_t>);
    OP(36, ppc_st<uint32_t>);
    OP(37, ppc_stu<uint32_t>);
    OP(38, ppc_st<uint8_t>);
    OP(39, ppc_stu<uint8_t>);
    OP(40, ppc_lz<uint16_t>);
    OP(41, ppc_lzu<uint16_t>);
    OP(42, ppc_lha);
    OP(43, ppc_lhau);
    OP(44, ppc_st<uint16_t>);
    OP(45, ppc_stu<uint16_t>);
    OP(46, ppc_lmw);
    OP(47, ppc_stmw);
    OP(48, ppc_lfs);
    OP(49, ppc_lfsu);
    OP(50, ppc_lfd);
    OP(51, ppc_lfdu);
    OP(52, ppc_stfs);
    OP(53, ppc_stfsu);
    OP(54, ppc_stfd);
    OP(55, ppc_stfdu);
    OP(59, ppc_opcode59);
    OP(63, ppc_opcode63);

    std::fill_n(SubOpcode31Grabber, 2048, ppc_illegalop);
    OP31(0,      ppc_cmp);
    OP31(4,      ppc_tw);
    OP31(32,     ppc_cmpl);

    OP31cod(8,   ppc_subf, CARRY1);
    OP31cod(40,  ppc_subf, CARRY0);
    OP31od(104,  ppc_neg);
    OP31od(136,  ppc_subfe);
    OP31od(200,  ppc_subfze);
    OP31od(232,  ppc_subfme);

    OP31cod(10,  ppc_add, CARRY1);
    OP31od(138,  ppc_adde);
    OP31od(202,  ppc_addze);
    OP31od(234,  ppc_addme);
    OP31cod(266, ppc_add, CARRY0);

    OP31d(11,    ppc_mulhwu);
    OP31d(75,    ppc_mulhw);
    OP31od(235,  ppc_mullw);
    OP31od(459,  ppc_divwu);
    OP31od(491,  ppc_divw);

    OP31(20,     ppc_lwarx);
    OP31(23,     ppc_lzx<uint32_t>);
    OP31(55,     ppc_lzux<uint32_t>);
    OP31(87,     ppc_lzx<uint8_t>);
    OP31(119,    ppc_lzux<uint8_t>);
    OP31(279,    ppc_lzx<uint16_t>);
    OP31(311,    ppc_lzux<uint16_t>);
    OP31(343,    ppc_lhax);
    OP31(375,    ppc_lhaux);
    OP31(533,    ppc_lswx);
    OP31(534,    ppc_lwbrx);
    OP31(535,    ppc_lfsx);
    OP31(567,    ppc_lfsux);
    OP31(597,    ppc_lswi);
    OP31(599,    ppc_lfdx);
    OP31(631,    ppc_lfdux);
    OP31(790,    ppc_lhbrx);

    SubOpcode31Grabber[(150<<1)+1] = ppc_stwcx; // No Rc=0 variant.
    OP31(151,    ppc_stx<uint32_t>);
    OP31(183,    ppc_stux<uint32_t>);
    OP31(215,    ppc_stx<uint8_t>);
    OP31(247,    ppc_stux<uint8_t>);
    OP31(407,    ppc_stx<uint16_t>);
    OP31(439,    ppc_stux<uint16_t>);
    OP31(661,    ppc_stswx);
    OP31(662,    ppc_stwbrx);
    OP31(663,    ppc_stfsx);
    OP31(695,    ppc_stfsux);
    OP31(725,    ppc_stswi);
    OP31(727,    ppc_stfdx);
    OP31(759,    ppc_stfdux);
    OP31(918,    ppc_sthbrx);
    if (!is_601) OP31(983, ppc_stfiwx);

    OP31(310,    ppc_eciwx);
    OP31(438,    ppc_ecowx);

    OP31dc(24,   ppc_shift, LEFT1); // slw
    OP31dc(28,   ppc_logical, ppc_and);
    OP31dc(60,   ppc_logical, ppc_andc);
    OP31dc(124,  ppc_logical, ppc_nor);
    OP31dc(284,  ppc_logical, ppc_eqv);
    OP31dc(316,  ppc_logical, ppc_xor);
    OP31dc(412,  ppc_logical, ppc_orc);
    OP31dc(444,  ppc_logical, ppc_or);
    OP31dc(476,  ppc_logical, ppc_nand);
    OP31dc(536,  ppc_shift, RIGHT0); // srw
    OP31d(792,   ppc_sraw);
    OP31d(824,   ppc_srawi);
    OP31dc(922,  ppc_exts, int16_t);
    OP31dc(954,  ppc_exts, int8_t);

    OP31d(26,    ppc_cntlzw);

    OP31(19,     ppc_mfcr);
    OP31(83,     ppc_mfmsr);
    OP31(144,    ppc_mtcrf);
    OP31(146,    ppc_mtmsr);
    OP31(210,    ppc_mtsr);
    OP31(242,    ppc_mtsrin);
    OP31(339,    ppc_mfspr);
    if (!is_601) OP31(371, ppc_mftb);
    OP31(467,    ppc_mtspr);
    OP31(512,    ppc_mcrxr);
    OP31(595,    ppc_mfsr);
    OP31(659,    ppc_mfsrin);

    OP31(54,     ppc_dcbst);
    OP31(86,     ppc_dcbf);
    OP31(246,    ppc_dcbtst);
    OP31(278,    ppc_dcbt);
    OP31(598,    ppc_sync);
    OP31(470,    ppc_dcbi);
    OP31(1014,   ppc_dcbz);

    if (is_601 || include_601) {
        OP31d(29,   power_maskg);
        OP31od(107, power_mul);
        OP31d(152,  power_slq);
        OP31d(153,  power_sle);
        OP31d(184,  power_sliq);
        OP31d(216,  power_sllq);
        OP31d(217,  power_sleq);
        OP31d(248,  power_slliq);
        OP31od(264, power_doz);
        OP31d(277,  power_lscbx);
        OP31od(331, power_div);
        OP31od(360, power_abs);
        OP31od(363, power_divs);
        OP31od(488, power_nabs);
        OP31(531,   power_clcs);
        OP31d(537,  power_rrib);
        OP31d(541,  power_maskir);
        OP31d(664,  power_srq);
        OP31d(665,  power_sre);
        OP31d(696,  power_sriq);
        OP31d(728,  power_srlq);
        OP31d(729,  power_sreq);
        OP31d(760,  power_srliq);
        OP31d(920,  power_sraq);
        OP31d(921,  power_srea);
        OP31d(952,  power_sraiq);
    }

    OP31(306,    ppc_tlbie);
    if (!is_601) OP31(370, ppc_tlbia);
    if (!is_601) OP31(566, ppc_tlbsync);
    OP31(854,    ppc_eieio);
    OP31(982,    ppc_icbi);
    if (!is_601) OP31(978, ppc_tlbld);
    if (!is_601) OP31(1010, ppc_tlbli);

#ifdef DPPC_ALTIVEC
    OP31(6,      altivec_lvsl);
    OP31(38,     altivec_lvsr);
    OP31(342,    altivec_dst); // dstt
    OP31(374,    altivec_dstst); // dststt
    OP31(822,    altivec_dss);  // dssall
    OP31(7,      altivec_lvebx);
    OP31(39,     altivec_lvehx);
    OP31(71,     altivec_lvewx);
    OP31(103,    altivec_lvx);
    OP31(359,    altivec_lvxl);
    OP31(135,    altivec_stvebx);
    OP31(167,    altivec_stvehx);
    OP31(199,    altivec_stvewx);
    OP31(231,    altivec_stvx);
    OP31(487,    altivec_stvxl);
#endif

    std::fill_n(SubOpcode59Grabber, 64, ppc_illegalop);
    OP59d(18,    ppc_fdivs);
    OP59d(20,    ppc_fsubs);
    OP59d(21,    ppc_fadds);
    if (ppc_state.spr[SPR::PVR] == PPC_VER::MPC970MP) OP59d(22, ppc_fsqrts);
    if (!is_601) OP59d(24, ppc_fres);
    OP59d(25,    ppc_fmuls);
    OP59d(28,    ppc_fmsubs);
    OP59d(29,    ppc_fmadds);
    OP59d(30,    ppc_fnmsubs);
    OP59d(31,    ppc_fnmadds);

    std::fill_n(SubOpcode63Grabber, 2048, ppc_illegalop);
    OP63(0,      ppc_fcmpu);
    OP63d(12,    ppc_frsp);
    OP63d(14,    ppc_fctiw);
    OP63d(15,    ppc_fctiwz);
    OP63d(18,    ppc_fdiv);
    OP63d(20,    ppc_fsub);
    OP63d(21,    ppc_fadd);
    if (ppc_state.spr[SPR::PVR] == PPC_VER::MPC970MP) OP63d(22, ppc_fsqrt);
    if (!is_601) OP63d(26, ppc_frsqrte);
    OP63(32,     ppc_fcmpo);
    OP63d(38,    ppc_mtfsb1);
    OP63d(40,    ppc_fneg);
    OP63(64,     ppc_mcrfs);
    OP63d(70,    ppc_mtfsb0);
    OP63d(72,    ppc_fmr);
    OP63d(134,   ppc_mtfsfi);
    OP63d(136,   ppc_fnabs);
    OP63d(264,   ppc_fabs);
    if (is_601) OP63dc(583, ppc_mffs, IS601); else OP63dc(583, ppc_mffs, NOT601);
    OP63d(711,   ppc_mtfsf);

    for (int i = 0; i < 1024; i += 32) {
        if (!is_601) OP63d(i + 23, ppc_fsel);
        OP63d(i + 25, ppc_fmul);
        OP63d(i + 28, ppc_fmsub);
        OP63d(i + 29, ppc_fmadd);
        OP63d(i + 30, ppc_fnmsub);
        OP63d(i + 31, ppc_fnmadd);
    }
}

void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, bool include_601, uint64_t tb_freq)
{
    mem_ctrl_instance = mem_ctrl;

    std::memset(&ppc_state, 0, sizeof(ppc_state));
    set_host_rounding_mode(0);

    ppc_state.spr[SPR::PVR] = cpu_version;
    is_601 = (cpu_version >> 16) == 1;

    initialize_ppc_opcode_tables(include_601);

    // initialize emulator timers
    TimerManager::get_instance()->set_time_now_cb(&get_virt_time_ns);
    TimerManager::get_instance()->set_notify_changes_cb(&force_cycle_counter_reload);

    // initialize time base facility
#ifdef __APPLE__
    mach_timebase_info(&timebase_info);
#endif
    g_nanoseconds_base = cpu_now_ns();
    g_icycles = 0;

//                    //                                        // PDM cpu clock calculated at 0x403036CC in r3
//  icnt_factor = 11; // 1 instruction = 2048 ns =    0.488 MHz // 00068034 =     0.426036 MHz = 2347.219 ns // floppy doesn't work
//  icnt_factor = 10; // 1 instruction = 1024 ns =    0.977 MHz // 000D204C =     0.860236 MHz = 1162.471 ns //  [0..10] MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  9; // 1 instruction =  512 ns =    1.953 MHz // 001A6081 =     1.728641 MHz =  578.489 ns //  [0..10] MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  8; // 1 instruction =  256 ns =    3.906 MHz // 0034E477 =     3.466359 MHz =  288.487 ns //  [0..10] MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  7; // 1 instruction =  128 ns =    7.813 MHz // 0069E54C =     6.939980 MHz =  144.092 ns //  [0..10] MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  6; // 1 instruction =   64 ns =   15.625 MHz // 00D3E6F5 =    13.887221 MHz =   72.008 ns // (10..60] = 50, (60..73] = 66, (73..100] = 80 MHz
//  icnt_factor =  5; // 1 instruction =   32 ns =   31.250 MHz // 01A7B672 =    27.768434 MHz =   36.012 ns //
    icnt_factor =  4; // 1 instruction =   16 ns =   62.500 MHz // 034F0F0F =    55.512847 MHz =   18.013 ns // 6100/60 in Apple System Profiler
//  icnt_factor =  3; // 1 instruction =    8 ns =  125.000 MHz // 069E1E1E =   111.025694 MHz =    9.006 ns // (100...) MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  2; // 1 instruction =    4 ns =  250.000 MHz // 0D3C3C3C =   222.051388 MHz =    4.503 ns // (100...) MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  1; // 1 instruction =    2 ns =  500.000 MHz // 1A611A7B =   442.571387 MHz =    2.259 ns // (100...) MHz = invalid clock for PDM gestalt calculation
//  icnt_factor =  0; // 1 instruction =    1 ns = 1500.000 MHz // 3465B2D9 =   879.080153 MHz =    1.137 ns // (100...) MHz = invalid clock for PDM gestalt calculation

    tbr_wr_timestamp = 0;
    rtc_timestamp = 0;
    tbr_wr_value = 0;
    if (is_601)
        tb_freq <<= 7;
    tbr_freq_shift = 0;
    uint64_t x;
    for (x = (tb_freq << 32) / NS_PER_SEC; x >> 32; x >>= 1)
        tbr_freq_shift++;
    tbr_freq_ghz = (uint32_t)x;
    tbr_period_ns = ((uint64_t)NS_PER_SEC << 32) / tb_freq;

    exec_flags = 0;
    exec_timer = false;

    timebase_counter = 0;
    dec_wr_value = 0;

#ifdef LOG_INSTRUCTIONS
    pcp = 0;
#endif

    if (is_601) {
        /* MPC601 sets MSR[ME] bit during hard reset / Power-On */
        ppc_state.msr = (MSR::ME + MSR::IP);
    } else {
        ppc_state.msr             = MSR::IP;
        ppc_state.spr[SPR::DEC_S] = 0xFFFFFFFFUL;
    }

    ppc_mmu_init();

    /* redirect code execution to reset vector */
    ppc_state.pc = 0xFFF00100;

#ifdef CPU_PROFILING
    gProfilerObj->register_profile("PPC_CPU",
        std::unique_ptr<BaseProfile>(new CPUProfile()));
#endif

#ifdef LOG__doprnt
    if (!addr_doprint) {
        lookup_name_kernel("__doprnt", addr_doprint);
        try_doprint = (addr_doprint != 0);
    }
#endif
}

void print_fprs() {
    for (int i = 0; i < 32; i++)
        cout << "FPR " << dec << i << " : " << ppc_state.fpr[i].dbl64_r << endl;
}

static map<string, int> SPRName2Num = {
    {"XER",    SPR::XER},       {"LR",     SPR::LR},    {"CTR",    SPR::CTR},
    {"DEC",    SPR::DEC_S},     {"PVR",    SPR::PVR},   {"SPRG0",  SPR::SPRG0},
    {"SPRG1",  SPR::SPRG1},     {"SPRG2",  SPR::SPRG2}, {"SPRG3",  SPR::SPRG3},
    {"SRR0",   SPR::SRR0},      {"SRR1",   SPR::SRR1},  {"IBAT0U", 528},
    {"IBAT0L", 529},            {"IBAT1U", 530},        {"IBAT1L", 531},
    {"IBAT2U", 532},            {"IBAT2L", 533},        {"IBAT3U", 534},
    {"IBAT3L", 535},            {"DBAT0U", 536},        {"DBAT0L", 537},
    {"DBAT1U", 538},            {"DBAT1L", 539},        {"DBAT2U", 540},
    {"DBAT2L", 541},            {"DBAT3U", 542},        {"DBAT3L", 543},
    {"HID0",   SPR::HID0},      {"HID1",   SPR::HID1},  {"IABR",   1010},
    {"DABR",   1013},           {"L2CR",   1017},       {"ICTC",   1019},
    {"THRM1",  1020},           {"THRM2",  1021},       {"THRM3",  1022},
    {"PIR",    1023},           {"TBL",    SPR::TBL_S}, {"TBU",    SPR::TBU_S},
    {"SDR1",   SPR::SDR1},      {"MQ",     SPR::MQ},    {"RTCU",   SPR::RTCU_S},
    {"RTCL",   SPR::RTCL_S},    {"DSISR",  SPR::DSISR}, {"DAR",    SPR::DAR},
    {"MMCR0",  SPR::MMCR0},     {"PMC1",   SPR::PMC1},  {"PMC2",   SPR::PMC2},
    {"SDA",    SPR::SDA},       {"SIA",    SPR::SIA},   {"MMCR1",  SPR::MMCR1}
};

static uint64_t reg_op(string& reg_name, uint64_t val, bool is_write) {
    string reg_name_u, reg_num_str;
    unsigned reg_num;
    map<string, int>::iterator spr;

    if (reg_name.length() < 2)
        goto bail_out;

    reg_name_u = reg_name;

    /* convert reg_name string to uppercase */
    std::for_each(reg_name_u.begin(), reg_name_u.end(), [](char& c) {
        c = ::toupper(c);
    });

    try {
        if (reg_name_u == "PC") {
            if (is_write)
                ppc_state.pc = (uint32_t)val;
            return ppc_state.pc;
        }
        if (reg_name_u == "MSR") {
            if (is_write)
                ppc_state.msr = (uint32_t)val;
            return ppc_state.msr;
        }
        if (reg_name_u == "CR") {
            if (is_write)
                ppc_state.cr = (uint32_t)val;
            return ppc_state.cr;
        }
        if (reg_name_u == "FPSCR") {
            if (is_write)
                ppc_state.fpscr = (uint32_t)val;
            return ppc_state.fpscr;
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 1) == "R") {
            reg_num_str = reg_name_u.substr(1);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.gpr[reg_num] = (uint32_t)val;
                return ppc_state.gpr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 1) == "F") {
            reg_num_str = reg_name_u.substr(1);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.fpr[reg_num].int64_r = val;
                return ppc_state.fpr[reg_num].int64_r;
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 3) == "SPR") {
            reg_num_str = reg_name_u.substr(3);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 1024) {
                switch (reg_num) {
                case SPR::DEC_U  : reg_num = SPR::DEC_S  ; break;
                case SPR::RTCL_U : reg_num = SPR::RTCL_S ; break;
                case SPR::RTCU_U : reg_num = SPR::RTCU_S ; break;
                case SPR::TBL_U  : reg_num = SPR::TBL_S  ; break;
                case SPR::TBU_U  : reg_num = SPR::TBU_S  ; break;
                }
                if (is_write)
                    ppc_state.spr[reg_num] = (uint32_t)val;
                return ppc_state.spr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 2) == "SR") {
            reg_num_str = reg_name_u.substr(2);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 16) {
                if (is_write)
                    ppc_state.sr[reg_num] = (uint32_t)val;
                return ppc_state.sr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        spr = SPRName2Num.find(reg_name_u);
        if (spr != SPRName2Num.end()) {
            if (is_write)
                ppc_state.spr[spr->second] = (uint32_t)val;
            return ppc_state.spr[spr->second];
        }
    } catch (...) {
    }

bail_out:
    throw std::invalid_argument(string("Unknown register ") + reg_name);
}

uint64_t get_reg(string reg_name) {
    return reg_op(reg_name, 0, false);
}

void set_reg(string reg_name, uint64_t val) {
    reg_op(reg_name, val, true);
}
