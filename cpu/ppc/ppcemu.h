/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

#ifndef PPCEMU_H
#define PPCEMU_H

#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <memaccess.h>

#include <atomic>
#include <cinttypes>
#include <functional>
#include <setjmp.h>
#include <string>

// Uncomment this to have a more graceful approach to illegal opcodes
//#define ILLEGAL_OP_SAFE 1

//#define CPU_PROFILING // enable CPU profiling

// Uncomment this to have characters output by __doprnt to appear in stdout.
//#define LOG__doprnt

// Uncomment this to be enable logging of executed instructions.
//#define LOG_INSTRUCTIONS

// Uncomment this to be allow enabling/disabling decrementer exceptions.
//#define DECREMENTER_TOGGLE

// Uncomment this to postpone decrementer exceptions to code that is
// not inside an exception handler or a lwarx atomic method.
//#define POSTPONE_DECREMENTER

/** type of compiler used during execution */
enum EXEC_MODE:uint32_t {
    interpreter     = 0,
    debugger        = 1,
    threaded_int    = 2,
    jit             = 3
};

typedef enum { big_end = 0, little_end = 1 } endian_switch;

typedef void (*PPCOpcode)(uint32_t opcode);

union FPR_storage {
    double dbl64_r;      // double floating-point representation
    uint64_t int64_r;    // double integer representation
};

/**
Except for the floating-point registers, all registers require
32 bits for representation. Floating-point registers need 64 bits.

  gpr = General Purpose Register
  fpr = Floating Point (FP) Register
   cr = Condition Register
  tbr = Time Base Register
fpscr = FP Status and Condition Register
  spr = Special Register
  msr = Machine State Register
   sr = Segment Register
**/

typedef struct struct_ppc_state {
    FPR_storage fpr[32];
    uint32_t pc;    // Referred as the CIA in the PPC manual
    uint32_t gpr[32];
    uint32_t cr;
    uint32_t fpscr;
    uint32_t tbr[2];
    uint32_t spr[1024];
    uint32_t msr;
    uint32_t sr[16];
    bool reserve;    // reserve bit used for lwarx and stcwx
#if SUPPORTS_PPC_LITTLE_ENDIAN_MODE
    bool is_LE;
#endif
} SetPRS;

extern SetPRS ppc_state;

#ifdef POSTPONE_DECREMENTER
extern bool in_lwarx;
extern bool in_exception;
#endif

/** symbolic names for frequently used SPRs */
enum SPR : int {
    MQ      = 0,   // MQ (601)
    XER     = 1,
    RTCU_U  = 4,   // user mode RTCU (601)
    RTCL_U  = 5,   // user mode RTCL (601)
    DEC_U   = 6,   // user mode decrementer (601)
    LR      = 8,
    CTR     = 9,
    DSISR   = 18,
    DAR     = 19,
    RTCU_S  = 20,  // supervisor RTCU (601)
    RTCL_S  = 21,  // supervisor RTCL (601)
    DEC_S   = 22,  // supervisor decrementer
    SDR1    = 25,
    SRR0    = 26,
    SRR1    = 27,
    TBL_U   = 268, // user mode TBL
    TBU_U   = 269, // user mode TBU
    SPRG0   = 272,
    SPRG1   = 273,
    SPRG2   = 274,
    SPRG3   = 275,
    TBL_S   = 284, // supervisor TBL
    TBU_S   = 285, // supervisor TBU
    PVR     = 287,
    MMCR0   = 952,
    PMC1    = 953,
    PMC2    = 954,
    SIA     = 955,
    MMCR1   = 956,
    SDA     = 959,
    HID0    = 1008,
    HID1    = 1009,
};

/** symbolic names for common PPC processors */
enum PPC_VER : uint32_t {
    MPC601      = 0x00010001,
    MPC603      = 0x00030001,
    MPC604      = 0x00040001,
    MPC603E     = 0x00060101,
    MPC603EV    = 0x00070101,
    MPC750      = 0x00080200,
    MPC604E     = 0x00090202,
    MPC970MP    = 0x00440100,
};

/**
typedef struct struct_ppc64_state {
    FPR_storage fpr [32];
    uint64_t pc; //Referred as the CIA in the PPC manual
    uint64_t gpr [32];
    uint32_t cr;
    uint32_t fpscr;
    uint32_t tbr [2];
    uint64_t spr [1024];
    uint32_t msr;
    uint32_t sr [16];
    bool reserve; //reserve bit used for lwarx and stcwx
} SetPRS64;

extern SetPRS64 ppc_state64;
**/

/**
Specific SPRS to be weary of:

USER MODEL
SPR 1 - XER
SPR 8 - Link Register / Branch
  b0 - Summary Overflow
  b1 - Overflow
  b2 - Carry
  b25-31 - Number of bytes to transfer
SPR 9 - Count

SUPERVISOR MODEL
19 is the Data Address Register
22 is the Decrementer
26, 27 are the Save and Restore Registers (SRR0, SRR1)
272 - 275 are the SPRGs
284 - 285 for writing to the TBR's.
528 - 535 are the Instruction BAT registers
536 - 543 are the Data BAT registers
**/

extern uint64_t timebase_counter;
extern uint64_t tbr_wr_timestamp;
extern uint64_t dec_wr_timestamp;
extern uint64_t rtc_timestamp;
extern uint64_t tbr_wr_value;
extern uint32_t dec_wr_value;
extern uint32_t tbr_freq_ghz;
extern uint32_t tbr_freq_shift;
extern uint64_t tbr_period_ns;
extern uint32_t rtc_lo, rtc_hi;

/* Flags for controlling interpreter execution. */
enum {
    EXEF_BRANCH         = 1 << 0, // Branch taken, target PC is is in ppc_next_instruction_address
    EXEF_EXCEPTION      = 1 << 1, // Exception handler invoked
    EXEF_RFI            = 1 << 2, // RFI instruction executed
    EXEF_OPC_DECODER    = 1 << 3, // Opcode decoder has changed
};

enum CR_select : int32_t {
    CR0_field = (0xF << 28),
    CR1_field = (0xF << 24),
};

// Define bit masks for CR0.
// To use them in other CR fields, just right shift it by 4*CR_num bits.
enum CRx_bit : uint32_t {
    CR_SO = 1UL << 28,
    CR_EQ = 1UL << 29,
    CR_GT = 1UL << 30,
    CR_LT = 1UL << 31
};

enum CR1_bit : uint32_t {
    CR1_OX = 24,
    CR1_VX,
    CR1_FEX,
    CR1_FX,
};

enum FPSCR : uint32_t {
    RN_MASK     = 0x3,
    NI          = 1UL << 2,
    XE          = 1UL << 3,
    ZE          = 1UL << 4,
    UE          = 1UL << 5,
    OE          = 1UL << 6,
    VE          = 1UL << 7,
    VXCVI       = 1UL << 8,
    VXSQRT      = 1UL << 9,
    VXSOFT      = 1UL << 10,
    FPCC_FUNAN  = 1UL << 12,
    FPCC_ZERO   = 1UL << 13,
    FPCC_POS    = 1UL << 14,
    FPCC_NEG    = 1UL << 15,
    FPCC_MASK   = FPCC_NEG | FPCC_POS | FPCC_ZERO | FPCC_FUNAN,
    FPRCD       = 1UL << 16,
    FPRF_MASK   = FPRCD | FPCC_MASK,
    FI          = 1UL << 17,
    FR          = 1UL << 18,
    VXVC        = 1UL << 19,
    VXIMZ       = 1UL << 20,
    VXZDZ       = 1UL << 21,
    VXIDI       = 1UL << 22,
    VXISI       = 1UL << 23,
    VXSNAN      = 1UL << 24,
    XX          = 1UL << 25,
    ZX          = 1UL << 26,
    UX          = 1UL << 27,
    OX          = 1UL << 28,
    VX          = 1UL << 29,
    FEX         = 1UL << 30,
    FX          = 1UL << 31
};

/** Bit definitions for the Machine State Register (MSR). */
enum MSR : int {
// ----------------------------------------------------------------------------------------
//        64-bit             // 32-bit  // Notes for MPC601 (601), PowerPC (PPC),
//                           //         // MPC750 (750), Altivec (AV), Power ISA (ISA)
// ----------------------------------------------------------------------------------------
    LE   = (1 << (63 - 63)), // 31      // Little-endian mode enable (not 601);
                                        // Little-Endian Mode (ISA)
    RI   = (1 << (63 - 62)), // 30      // Recoverable exception (not 601);
                                        // Recoverable Interrupt (ISA)
    PM   = (1 << (63 - 61)), // 29      // Performance monitor marked mode (750);
                                        // Performance Monitor Mark (PMM for ISA)
//       = (1 << (63 - 60)), // 28      // Reserved
    DR   = (1 << (63 - 59)), // 27      // Data address translation (DT on 601);
                                        // Data Relocate (ISA)
    IR   = (1 << (63 - 58)), // 26      // Instruction address translation (IT on 601);
                                        // Instruction Relocate (ISA)
    IP   = (1 << (63 - 57)), // 25      // Exception prefix (EP); Reserved (ISA)
//       = (1 << (63 - 56)), // 24      // Reserved (AL for POWER)
    FE1  = (1 << (63 - 55)), // 23      // Floating-point exception mode 1
    BE   = (1 << (63 - 54)), // 22      // Branch trace enable (Optional, not 601);
                                        // Trace Enable (TE): Branch Trace (ISA)
    SE   = (1 << (63 - 53)), // 21      // Single-step trace enable (Optional);
                                        // Trace Enable (TE): Single Step Trace (ISA)
    FE0  = (1 << (63 - 52)), // 20      // Floating-point exception mode 0
    ME   = (1 << (63 - 51)), // 19      // Machine check enable;
                                        // Machine Check Interrupt Enable (ISA)
    FP   = (1 << (63 - 50)), // 18      // Floating-point available
    PR   = (1 << (63 - 49)), // 17      // Privilege level; Problem State (ISA)
    EE   = (1 << (63 - 48)), // 16      // External exception enable (601);
                                        // External interrupt enable
// ----------------------------------------------------------------------------------------
    ILE  = (1 << (63 - 47)), // 15      // Exception little-endian mode (not 601, not ISA)
    TGPR = (1 << (63 - 46)), // 14      // Temporary GPR remapping (603e)
    POW  = (1 << (63 - 45)), // 13      // Power management enable (not 601, not ISA)
//       = (1 << (63 - 44)), // 12      // Reserved
//       = (1 << (63 - 42)), // 10      // Reserved
//  S    = (1 << (63 - 41)), //  9      // Secure (ISA)
//  VSX  = (1 << (63 - 40)), //  8      // VSX Available (ISA)
//       = (1 << (63 - 39)), //  7      // Reserved
    VEC  = (1 << (63 - 38)), //  6      // AltiVec available (AV); Vector Available (ISA)
// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------
//  HV   = (1ULL << (63 -  3)), //      // Hypervisor State (ISA)
//  ISF  = (1ULL << (63 -  2)), //      // Exception 64-bit mode (optional, PPC)
//       = (1ULL << (63 -  1)), //      // Reserved
//  SF   = (1ULL << (63 -  0)), //      // Sixty-four bit mode (PowerPC, PowerISA)
// ----------------------------------------------------------------------------------------
};

enum XER : uint32_t {
    CA = 1UL << 29,
    OV = 1UL << 30,
    SO = 1UL << 31
};

//for inf and nan checks
enum FPOP : int {
    DIV    = 0x12,
    SUB    = 0x14,
    ADD    = 0x15,
    SQRT   = 0x16,
    MUL    = 0x19
};

/** PowerPC exception types. */
enum class Except_Type {
    EXC_SYSTEM_RESET = 1,
    EXC_MACHINE_CHECK,
    EXC_DSI,
    EXC_ISI,
    EXC_EXT_INT,
    EXC_ALIGNMENT,
    EXC_PROGRAM,
    EXC_NO_FPU,
    EXC_DECR,
    EXC_SYSCALL = 12,
    EXC_TRACE   = 13
};

/** Program Exception subclasses. */
enum Exc_Cause : uint32_t {
    FPU_OFF     = 1 << (31 - 11),
    ILLEGAL_OP  = 1 << (31 - 12),
    NOT_ALLOWED = 1 << (31 - 13),
    TRAP        = 1 << (31 - 14),
};

extern unsigned exec_flags;

extern jmp_buf exc_env;

enum Po_Cause : int {
    po_none,
    po_starting_up,
    po_quit,
    po_quitting,
    po_shut_down,
    po_shutting_down,
    po_restart,
    po_restarting,
    po_disassemble_on,
    po_disassemble_off,
    po_enter_debugger,
    po_entered_debugger,
    po_signal_interrupt,
    po_benchmark_exception,
    po_endian_switch,
};

extern bool power_on;
extern Po_Cause power_off_reason;
extern bool int_pin;
extern bool dec_exception_pending;

extern bool is_601;        // For PowerPC 601 Emulation
extern bool include_601;   // For non-PowerPC 601 emulation with 601 extras
                           // (matches Mac OS 9 environment which can emulate MPC 601 instructions)
extern bool is_altivec;    // For Altivec Emulation
extern bool is_64bit;      // For PowerPC G5 Emulation

// Make execution deterministic (ignore external input, used a fixed date, etc.)
extern bool is_deterministic;

// Important Addressing Integers
extern uint32_t ppc_next_instruction_address;

inline uint32_t ppc_read_instruction(const uint8_t* ptr) {
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    extern MemCtrlBase* mem_ctrl_instance;
    bool needs_swap = mem_ctrl_instance->needs_swap_endian(false);
    return needs_swap ? READ_DWORD_LE_A(ptr) : READ_DWORD_BE_A(ptr);
#else
    return READ_DWORD_BE_A(ptr);
#endif
}

// Profiling Stats
#ifdef CPU_PROFILING
extern uint64_t num_executed_instrs;
extern uint64_t num_supervisor_instrs;
extern uint64_t num_int_loads;
extern uint64_t num_int_stores;
extern uint64_t exceptions_processed;
#endif

// instruction enums
typedef enum {
    ppc_and  = 1,
    ppc_andc = 2,
    ppc_eqv  = 3,
    ppc_nand = 4,
    ppc_nor  = 5,
    ppc_or   = 6,
    ppc_orc  = 7,
    ppc_xor  = 8,
} logical_fun;

typedef enum {
    LK0,
    LK1,
} field_lk;

typedef enum {
    AA0,
    AA1,
} field_aa;

typedef enum {
    SHFT0,
    SHFT1,
} field_shift;

typedef enum {
    RIGHT0,
    LEFT1,
} field_direction;

typedef enum {
    RC0,
    RC1,
} field_rc;

typedef enum {
    OV0,
    OV1,
} field_ov;

typedef enum {
    CARRY0,
    CARRY1,
} field_carry;

typedef enum {
    NOT601,
    IS601,
} field_601;

// Placeholder value for cases where we don't have a currently-executing instruction.
constexpr uint32_t NO_OPCODE = 0;

// Function prototypes
extern void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, bool include_601, uint64_t tb_freq);
extern void ppc_mmu_init();

void ppc_illegalop(uint32_t opcode);
void ppc_assert_int();
void ppc_release_int();

void initialize_ppc_opcode_table();

void ppc_changecrf0(uint32_t set_result);
void set_host_rounding_mode(uint8_t mode);
void update_fpscr(uint32_t new_fpscr);

/* Exception handlers. */
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
[[noreturn]] void dbg_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
void ppc_floating_point_exception(uint32_t opcode);
void ppc_alignment_exception(uint32_t opcode, uint32_t ea);

// MEMORY DECLARATIONS
extern MemCtrlBase* mem_ctrl_instance;

#if 0
    typedef std::function<void()> CtxSyncCallback;
#else
    typedef void (*CtxSyncCallback)(void);
#endif
extern void add_ctx_sync_action(const CtxSyncCallback &cb);
extern void do_ctx_sync(void);

// The functions used by the PowerPC processor

namespace dppc_interpreter {
template <field_lk l, field_601 for601> extern void ppc_bcctr(uint32_t opcode);
template <field_lk l> extern void ppc_bclr(uint32_t opcode);
extern void ppc_crand(uint32_t opcode);
extern void ppc_crandc(uint32_t opcode);
extern void ppc_creqv(uint32_t opcode);
extern void ppc_crnand(uint32_t opcode);
extern void ppc_crnor(uint32_t opcode);
extern void ppc_cror(uint32_t opcode);
extern void ppc_crorc(uint32_t opcode);
extern void ppc_crxor(uint32_t opcode);
extern void ppc_isync(uint32_t opcode);

template <logical_fun logical_op, field_rc rec> extern void ppc_logical(uint32_t opcode);

template <field_carry carry, field_rc rec, field_ov ov> extern void ppc_add(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_adde(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_addme(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_addze(uint32_t opcode);
extern void ppc_cmp(uint32_t opcode);
extern void ppc_cmpl(uint32_t opcode);
template <field_rc rec> extern void ppc_cntlzw(uint32_t opcode);
extern void ppc_dcbf(uint32_t opcode);
extern void ppc_dcbi(uint32_t opcode);
extern void ppc_dcbst(uint32_t opcode);
extern void ppc_dcbt(uint32_t opcode);
extern void ppc_dcbtst(uint32_t opcode);
extern void ppc_dcbz(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_divw(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_divwu(uint32_t opcode);
extern void ppc_eciwx(uint32_t opcode);
extern void ppc_ecowx(uint32_t opcode);
extern void ppc_eieio(uint32_t opcode);
template <class T, field_rc rec>extern void ppc_exts(uint32_t opcode);
extern void ppc_icbi(uint32_t opcode);
extern void ppc_mftb(uint32_t opcode);
extern void ppc_lhaux(uint32_t opcode);
extern void ppc_lhax(uint32_t opcode);
extern void ppc_lhbrx(uint32_t opcode);
extern void ppc_lwarx(uint32_t opcode);
extern void ppc_lwbrx(uint32_t opcode);
template <class T> extern void ppc_lzx(uint32_t opcode);
template <class T> extern void ppc_lzux(uint32_t opcode);
extern void ppc_mcrxr(uint32_t opcode);
extern void ppc_mfcr(uint32_t opcode);
template <field_rc rec> extern void ppc_mulhwu(uint32_t opcode);
template <field_rc rec> extern void ppc_mulhw(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_mullw(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_neg(uint32_t opcode);
template <field_direction shift, field_rc rec> extern void ppc_shift(uint32_t opcode);
template <field_rc rec> extern void ppc_sraw(uint32_t opcode);
template <field_rc rec> extern void ppc_srawi(uint32_t opcode);
template <class T> extern void ppc_stx(uint32_t opcode);
template <class T> extern void ppc_stux(uint32_t opcode);
extern void ppc_stfiwx(uint32_t opcode);
extern void ppc_sthbrx(uint32_t opcode);
extern void ppc_stwcx(uint32_t opcode);
extern void ppc_stwbrx(uint32_t opcode);
template <field_carry carry, field_rc rec, field_ov ov> extern void ppc_subf(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_subfe(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_subfme(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void ppc_subfze(uint32_t opcode);
extern void ppc_sync(uint32_t opcode);
extern void ppc_tlbia(uint32_t opcode);
extern void ppc_tlbie(uint32_t opcode);
extern void ppc_tlbli(uint32_t opcode);
extern void ppc_tlbld(uint32_t opcode);
extern void ppc_tlbsync(uint32_t opcode);
extern void ppc_tw(uint32_t opcode);

extern void ppc_lswi(uint32_t opcode);
extern void ppc_lswx(uint32_t opcode);
extern void ppc_stswi(uint32_t opcode);
extern void ppc_stswx(uint32_t opcode);

extern void ppc_mfsr(uint32_t opcode);
extern void ppc_mfsrin(uint32_t opcode);
extern void ppc_mtsr(uint32_t opcode);
extern void ppc_mtsrin(uint32_t opcode);

extern void ppc_mcrf(uint32_t opcode);
extern void ppc_mtcrf(uint32_t opcode);
extern void ppc_mfmsr(uint32_t opcode);
extern void ppc_mfspr(uint32_t opcode);
extern void ppc_mtmsr(uint32_t opcode);
extern void ppc_mtspr(uint32_t opcode);

template <field_rc rec> extern void ppc_mtfsb0(uint32_t opcode);
template <field_rc rec> extern void ppc_mtfsb1(uint32_t opcode);
extern void ppc_mcrfs(uint32_t opcode);
template <field_rc rec> extern void ppc_fmr(uint32_t opcode);
template <field_601 for601, field_rc rec> extern void ppc_mffs(uint32_t opcode);
template <field_rc rec> extern void ppc_mtfsf(uint32_t opcode);
template <field_rc rec> extern void ppc_mtfsfi(uint32_t opcode);

template <field_shift shift> extern void ppc_addi(uint32_t opcode);
template <field_rc rec> extern void ppc_addic(uint32_t opcode);
template <field_shift shift> extern void ppc_andirc(uint32_t opcode);
template <field_lk l, field_aa a> extern void ppc_b(uint32_t opcode);
template <field_lk l, field_aa a> extern void ppc_bc(uint32_t opcode);
extern void ppc_cmpi(uint32_t opcode);
extern void ppc_cmpli(uint32_t opcode);
template <class T> extern void ppc_lz(uint32_t opcode);
template <class T> extern void ppc_lzu(uint32_t opcode);
extern void ppc_lha(uint32_t opcode);
extern void ppc_lhau(uint32_t opcode);
extern void ppc_lmw(uint32_t opcode);
extern void ppc_mulli(uint32_t opcode);
template <field_shift shift> extern void ppc_ori(uint32_t opcode);
extern void ppc_rfi(uint32_t opcode);
extern void ppc_rlwimi(uint32_t opcode);
extern void ppc_rlwinm(uint32_t opcode);
extern void ppc_rlwnm(uint32_t opcode);
extern void ppc_sc(uint32_t opcode);
template <class T> extern void ppc_st(uint32_t opcode);
template <class T> extern void ppc_stu(uint32_t opcode);
extern void ppc_stmw(uint32_t opcode);
extern void ppc_subfic(uint32_t opcode);
extern void ppc_twi(uint32_t opcode);
template <field_shift shift> extern void ppc_xori(uint32_t opcode);

extern void ppc_lfs(uint32_t opcode);
extern void ppc_lfsu(uint32_t opcode);
extern void ppc_lfsx(uint32_t opcode);
extern void ppc_lfsux(uint32_t opcode);
extern void ppc_lfd(uint32_t opcode);
extern void ppc_lfdu(uint32_t opcode);
extern void ppc_lfdx(uint32_t opcode);
extern void ppc_lfdux(uint32_t opcode);
extern void ppc_stfs(uint32_t opcode);
extern void ppc_stfsu(uint32_t opcode);
extern void ppc_stfsx(uint32_t opcode);
extern void ppc_stfsux(uint32_t opcode);
extern void ppc_stfd(uint32_t opcode);
extern void ppc_stfdu(uint32_t opcode);
extern void ppc_stfdx(uint32_t opcode);
extern void ppc_stfdux(uint32_t opcode);

template <field_rc rec> extern void ppc_fadd(uint32_t opcode);
template <field_rc rec> extern void ppc_fsub(uint32_t opcode);
template <field_rc rec> extern void ppc_fmul(uint32_t opcode);
template <field_rc rec> extern void ppc_fdiv(uint32_t opcode);
template <field_rc rec> extern void ppc_fadds(uint32_t opcode);
template <field_rc rec> extern void ppc_fsubs(uint32_t opcode);
template <field_rc rec> extern void ppc_fmuls(uint32_t opcode);
template <field_rc rec> extern void ppc_fdivs(uint32_t opcode);
template <field_rc rec> extern void ppc_fmadd(uint32_t opcode);
template <field_rc rec> extern void ppc_fmsub(uint32_t opcode);
template <field_rc rec> extern void ppc_fnmadd(uint32_t opcode);
template <field_rc rec> extern void ppc_fnmsub(uint32_t opcode);
template <field_rc rec> extern void ppc_fmadds(uint32_t opcode);
template <field_rc rec> extern void ppc_fmsubs(uint32_t opcode);
template <field_rc rec> extern void ppc_fnmadds(uint32_t opcode);
template <field_rc rec> extern void ppc_fnmsubs(uint32_t opcode);
template <field_rc rec> extern void ppc_fabs(uint32_t opcode);
template <field_rc rec> extern void ppc_fnabs(uint32_t opcode);
template <field_rc rec> extern void ppc_fneg(uint32_t opcode);
template <field_rc rec> extern void ppc_fsel(uint32_t opcode);
template <field_rc rec> extern void ppc_fres(uint32_t opcode);
template <field_rc rec> extern void ppc_fsqrts(uint32_t opcode);
template <field_rc rec> extern void ppc_fsqrt(uint32_t opcode);
template <field_rc rec> extern void ppc_frsqrte(uint32_t opcode);
template <field_rc rec> extern void ppc_frsp(uint32_t opcode);
template <field_rc rec> extern void ppc_fctiw(uint32_t opcode);
template <field_rc rec> extern void ppc_fctiwz(uint32_t opcode);

extern void ppc_fcmpo(uint32_t opcode);
extern void ppc_fcmpu(uint32_t opcode);

// Power-specific instructions
template <field_rc rec, field_ov ov> extern void power_abs(uint32_t opcode);
extern void power_clcs(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void power_div(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void power_divs(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void power_doz(uint32_t opcode);
extern void power_dozi(uint32_t opcode);
template <field_rc rec> extern void power_lscbx(uint32_t opcode);
template <field_rc rec> extern void power_maskg(uint32_t opcode);
template <field_rc rec> extern void power_maskir(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void power_mul(uint32_t opcode);
template <field_rc rec, field_ov ov> extern void power_nabs(uint32_t opcode);
extern void power_rlmi(uint32_t opcode);
template <field_rc rec> extern void power_rrib(uint32_t opcode);
template <field_rc rec> extern void power_sle(uint32_t opcode);
template <field_rc rec> extern void power_sleq(uint32_t opcode);
template <field_rc rec> extern void power_sliq(uint32_t opcode);
template <field_rc rec> extern void power_slliq(uint32_t opcode);
template <field_rc rec> extern void power_sllq(uint32_t opcode);
template <field_rc rec> extern void power_slq(uint32_t opcode);
template <field_rc rec> extern void power_sraiq(uint32_t opcode);
template <field_rc rec> extern void power_sraq(uint32_t opcode);
template <field_rc rec> extern void power_sre(uint32_t opcode);
template <field_rc rec> extern void power_srea(uint32_t opcode);
template <field_rc rec> extern void power_sreq(uint32_t opcode);
template <field_rc rec> extern void power_sriq(uint32_t opcode);
template <field_rc rec> extern void power_srliq(uint32_t opcode);
template <field_rc rec> extern void power_srlq(uint32_t opcode);
template <field_rc rec> extern void power_srq(uint32_t opcode);
}    // namespace dppc_interpreter

// AltiVec instructions

// 64-bit instructions

// G5+ instructions

extern uint64_t get_virt_time_ns(void);

extern void ppc_main_opcode(PPCOpcode* ppc_opcode_grabber, uint32_t opcode);
extern void ppc_exec(void);
extern void ppc_exec_single(void);
extern void ppc_exec_until(uint32_t goal_addr);
extern void ppc_exec_dbg(uint32_t start_addr, uint32_t size);

extern PPCOpcode* ppc_opcode_grabber;
extern void ppc_msr_did_change(uint32_t old_msr_val, uint32_t new_msr_val, bool set_next_instruction_address = true);
extern void ppc_change_endian(bool newLE);

/* debugging support API */
uint64_t get_reg(std::string reg_name); /* get content of the register reg_name */
void set_reg(std::string reg_name, uint64_t val); /* set reg_name to val */

#ifdef LOG_INSTRUCTIONS
typedef struct {
    uint64_t cycle;
    uint32_t addr;
    uint32_t paddr;
    uint32_t ins;
    uint32_t msr;
    uint32_t msr_after;
    uint32_t flags_before;
    uint32_t flags_after;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t reserved4;
    uint32_t reserved5;
    uint32_t reserved6;
    uint32_t reserved7;
} InstructionRec; // 64 bytes

#define InstructionLogSize 0x1000000
extern InstructionRec InstructionLog[InstructionLogSize];
extern uint64_t InstructionNumber;

void dumpinstructionlog();
#endif

#ifdef DECREMENTER_TOGGLE
extern bool decrementer_enabled;
#endif

#endif /* PPCEMU_H */
