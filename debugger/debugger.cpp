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

#include <core/timermanager.h>
#include <cpu/ppc/ppcdisasm.h>
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <debugger/debugger.h>
#include <devices/common/dbdma.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/ofnvram.h>
#include <devices/floppy/swim3.h>
#include <debugger/backtrace.h>
#include "memaccess.h"
#include <utils/profiler.h>
#include "symbols.h"
#include "atraps.h"
#include "kgmacros.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>
#include <map>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <string>

#ifdef DEBUG_CPU_INT
#include <devices/common/viacuda.h>
#endif

#ifdef ENABLE_68K_DEBUGGER // optionally defined in CMakeLists.txt
    #include <capstone/capstone.h>
#endif

using namespace std;

#define COUTX uppercase << hex
#define COUT0_X(w) setfill('0') << setw(w) << right << COUTX
#define COUT016X COUT0_X(16)
#define COUT08X COUT0_X(8)
#define COUT04X COUT0_X(4)

static uint32_t str2addr(string& addr_str) {
    try {
        return static_cast<uint32_t>(stoul(addr_str, NULL, 0));
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + addr_str);
    }
}

static uint32_t str2num(string& num_str) {
    try {
        return static_cast<uint32_t>(stol(num_str, NULL, 0));
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + num_str);
    }
}

static void show_help() {
    cout << "Debugger commands:" << endl;
    cout << "  step [N]       -- execute single instruction" << endl;
    cout << "                    N is an optional step count" << endl;
    cout << "  si [N]         -- shortcut for step" << endl;
    cout << "  next           -- same as step but treats subroutine calls" << endl;
    cout << "                    as single instructions." << endl;
    cout << "  ni             -- shortcut for next" << endl;
    cout << "  until X        -- execute until address X is reached" << endl;
    cout << "  go             -- exit debugger and continue emulator execution" << endl;
    cout << "  regs           -- dump content of the GPRs" << endl;
    cout << "  fregs          -- dump content of the FPRs" << endl;
    cout << "  mregs          -- dump content of the MMU registers" << endl;
    cout << "  set R=X        -- assign value X to register R" << endl;
    cout << "                    if R=loglevel, set the internal" << endl;
    cout << "                    log level to X whose range is -2...9" << endl;
    cout << "  dump NT,X      -- dump N memory cells of size T at address X" << endl;
    cout << "                    T can be b(byte), w(word), d(double)," << endl;
    cout << "                    q(quad) or c(character)." << endl;
    cout << "  setmem X=V.T   -- set memory at address X to value V of size T" << endl;
    cout << "                    T can be b(byte), w(word), d(double)," << endl;
    cout << "                    q(quad) or c(character)." << endl;
    cout << "  regions        -- dump memory regions" << endl;
    cout << "  fdd [D,][W,]P  -- insert floppy into drive D (1 = default, 2), with" << endl;
    cout << "                    writable flag W (r = readonly (default), w = writable)," << endl;
    cout << "                    and path P" << endl;
    cout << "  profile C N    -- run subcommand C on profile N" << endl;
    cout << "                    supported subcommands:" << endl;
    cout << "                    'show' - show profile report" << endl;
    cout << "                    'reset' - reset profile variables" << endl;
#ifdef PROFILER
    cout << "  profiler       -- show stats related to the processor" << endl;
#endif
    cout << "  disas N,X      -- disassemble N instructions starting at address X" << endl;
    cout << "                    X can be any number or a known register name" << endl;
    cout << "                    disas with no arguments defaults to disas 1,pc" << endl;
    cout << "  da N,X         -- shortcut for disas" << endl;
#ifdef ENABLE_68K_DEBUGGER
    cout << "  context X      -- switch to the debugging context X." << endl;
    cout << "                    X can be either 'ppc' (default), '68k'," << endl;
    cout << "                    or 'auto'." << endl;
#endif
    cout << "  printenv       -- print current NVRAM settings." << endl;
    cout << "  setenv V N     -- set NVRAM variable V to value N." << endl;
    cout << endl;
    cout << "  restart        -- restart the machine" << endl;
    cout << "  quit           -- quit the debugger" << endl;
    cout << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

#ifdef ENABLE_68K_DEBUGGER

typedef struct {
    csh cs_handle;
    cs_insn* insn;
    bool diddisasm;
} DisasmContext68K;

static uint32_t disasm_68k(uint32_t count, uint32_t address, DisasmContext68K *ctx = nullptr) {
    csh cs_handle;
    uint8_t code[12]{};
    size_t code_size;
    uint64_t dis_addr;

    if (ctx) {
        ctx->insn = nullptr;
        ctx->cs_handle = 0;
        ctx->diddisasm = false;
    }

    if (cs_open(CS_ARCH_M68K, CS_MODE_M68K_040, &cs_handle) != CS_ERR_OK) {
        cout << "Capstone initialization error" << endl;
        return address;
    }

    if (cs_option(cs_handle, CS_OPT_DETAIL, 1) != CS_ERR_OK) {
        cout << "Capstone option error" << endl;
        cs_close(&cs_handle);
        return address;
    }

    cs_insn* insn = cs_malloc(cs_handle);

    for (; power_on && count > 0; count--) {
        // prefetch opcode bytes (a 68k instruction can occupy 2...12 bytes)
        for (int i = 0; i < sizeof(code); i++) {
            try {
                code[i] = mem_read_dbg(address + i, 1);
            }
            catch(...) {
                printf("<memerror>");
            }
        }

        uint32_t phys_addr;
        int offset;
        binary_kind_t kind;
        int kinds = -1;
        mmu_translate_imem(address, &phys_addr);
        std::string name = get_name(address, phys_addr, &offset, &kind, kinds);

        cout << COUT08X << address;
        if (phys_addr != address) {
            cout << "->" << COUT08X << phys_addr;
        }
        if (!name.empty()) {
            cout << " " << setw(27) << left << setfill(' ') << name;
        }

        cout << ": ";

        const uint8_t *code_ptr  = code;
        code_size = sizeof(code);
        dis_addr  = address;

        // catch and handle F-Traps (Nanokernel calls) ourselves because
        // Capstone will likely return no meaningful assembly for them

        trap_info ti;
        if (get_atrap_info(READ_WORD_BE_U(&code[0]), ti)) {
            address += 2;
            code_size = 2;
            snprintf(insn->mnemonic, sizeof(insn->mnemonic), "%s", ti.name);
            snprintf(insn->op_str, sizeof(insn->op_str), "");
            insn->detail->regs_read_count = 0;
            insn->detail->regs_write_count = 0;
        } else if ((code[0] & 0xF0) != 0xF0 && cs_disasm_iter(cs_handle, &code_ptr, &code_size, &dis_addr, insn)) {
            code_size = sizeof(code) - code_size;
            address = static_cast<uint32_t>(dis_addr);
        } else {
            address += 2;
            code_size = 2;
            snprintf(insn->mnemonic, sizeof(insn->mnemonic), "dc.w");
            snprintf(insn->op_str, sizeof(insn->op_str), "$%04x", READ_WORD_BE_U(&code[0]));
            insn->detail->regs_read_count = 0;
            insn->detail->regs_write_count = 0;
        }

        int i = 0;
        for (; i < code_size; i += 2)
            cout << COUT04X << READ_WORD_BE_U(&code[i]) << " ";
        cout << setfill(' ') << setw((10 - (int)code_size) / 2 * 5) << "";
        cout << setfill(' ') << setw(10) << left << insn->mnemonic << insn->op_str << right << dec;
        if (ctx) {
            ctx->diddisasm = true;
        } else {
            cout << endl;
        }
    }

    if (ctx) {
        ctx->insn = insn;
        ctx->cs_handle = cs_handle;
    } else {
        cs_free(insn, 1);
        cs_close(&cs_handle);
    }
    return address;
}

uint32_t get_reg_68k(const char *reg_name) {
    if (reg_name[0] == 'd' && reg_name[1] < '8') return ppc_state.gpr[reg_name[1] - '0' + 8];
    if (reg_name[0] == 'a') return reg_name[1] < '7' ? ppc_state.gpr[reg_name[1] - '0' + 16] : ppc_state.gpr[1];
    if (!strcmp(reg_name, "pc"     )) return ppc_state.gpr[24] - 2;
    if (!strcmp(reg_name, "sr"     )) return (ppc_state.gpr[25] & 0xFF) << 8;
    if (!strcmp(reg_name, "ccr"    )) return ppc_state.gpr[26];
    return 0;
}

static void disasm_68k_in(DisasmContext68K &ctx, uint32_t address) {
    disasm_68k(1, address, &ctx);

    if (ctx.diddisasm && (ctx.insn->detail->regs_read_count > 0 || ctx.insn->detail->regs_write_count > 0)) {
        size_t instr_str_length = /*strlen(ctx.insn->mnemonic) +*/ strlen(ctx.insn->op_str);

        if (instr_str_length < 18)
            cout << setw(18 - (int)instr_str_length) << " ";
        cout << " ;";
        if (ctx.insn->detail->regs_read_count > 0) {
            cout << " in{" << COUTX;
            for (int i = 0; i < ctx.insn->detail->regs_read_count; i++) {
                const char *reg_name = cs_reg_name(ctx.cs_handle, ctx.insn->detail->regs_read[i]);
                cout << " " << reg_name << ":" << get_reg_68k(reg_name);
            }
            cout << " }" << dec;
        }
    }
}

static void disasm_68k_out(DisasmContext68K &ctx) {
    if (ctx.diddisasm) {
        if (ctx.insn->detail->regs_write_count > 0) {
            cout << " out{" << COUTX;
            for (int i = 0; i < ctx.insn->detail->regs_write_count; i++) {
                const char *reg_name = cs_reg_name(ctx.cs_handle, ctx.insn->detail->regs_write[i]);
                cout << " " << reg_name << ":" << get_reg_68k(reg_name);
            }
            cout << " }" << dec;
        }
        cout << endl;
    }
}

constexpr auto EMU_68K_START_PHYS       = 0xfff60000;
constexpr auto EMU_68K_SIZE_PHYS        =    0xa0000;
constexpr auto EMU_68K_START            = 0x68000000;
constexpr auto EMU_68K_SIZE             =  0x2000000; // includes 0x69xxxxxx

constexpr auto EMU_68K_TABLE_START_PHYS = 0xfff80000;
constexpr auto EMU_68K_TABLE_START      = 0x68080000;
constexpr auto EMU_68K_TABLE_SIZE       =    0x80000;

static int get_context() {
    if (ppc_state.pc >= EMU_68K_START && ppc_state.pc <= EMU_68K_START + EMU_68K_SIZE - 1) {
/*
        // Don't check physical address since 0x69xxxxxx points to RAM instead of ROM.
        uint32_t pcp;
        mmu_translate_imem(ppc_state.pc, &pcp);
        if (pcp >= EMU_68K_START_PHYS && pcp <= EMU_68K_START_PHYS + EMU_68K_SIZE_PHYS - 1) {
            return 2;
        }
*/
        return 2;
    }
    return 1;
}

/** Execute ppc until the 68k opcode table is reached. */
bool exec_upto_68k_opcode(bool check_ppc) {
    while (power_on) {
        uint32_t ppc_pc = ppc_state.pc;
/*
        uint32_t pcp;
        mmu_translate_imem(ppc_pc, &pcp);
*/
        if (
            (ppc_pc & 7) == 0 &&
            ppc_pc >= EMU_68K_TABLE_START &&
            ppc_pc <= EMU_68K_TABLE_START + EMU_68K_TABLE_SIZE - 1 &&
/*
            // Don't check physical address because maybe the emulator can move to RAM.
            pcp    >= EMU_68K_TABLE_START_PHYS &&
            pcp    <= EMU_68K_TABLE_START_PHYS + EMU_68K_TABLE_SIZE - 1 &&
*/
            ppc_pc == ppc_state.gpr[29]
        ) {
            return true;
        }
        if (check_ppc && get_context() == 1) {
            // we've left the emulator
            return false;
        }
        ppc_exec_single();
    }
    return false;
}

/** Execute one emulated 68k instruction. */
void exec_single_68k()
{
    uint32_t emu_table_virt, cur_68k_pc, cur_instr_tab_entry, ppc_pc;

    /* PPC r24 contains 68k PC advanced by two bytes
       as part of instruction prefetching */
    cur_68k_pc = static_cast<uint32_t>(ppc_state.gpr[24] - 2);

    /* PPC r29 contains base address of the emulator opcode table */
    emu_table_virt = ppc_state.gpr[29] & 0xFFF80000;

    /* calculate address of the current opcode table entry
       using the PPC PC.
    */

    cur_instr_tab_entry = ppc_state.pc & ~7;
    uint32_t expected_instr_tab_entry = mmu_read_vmem<uint16_t>(NO_OPCODE, cur_68k_pc) * 8 + emu_table_virt;
    if (cur_instr_tab_entry != expected_instr_tab_entry) {
        printf("opcode current:%04X != expected:%04X (r29:%04X)\n",
            (cur_instr_tab_entry - emu_table_virt) >> 3,
            (expected_instr_tab_entry - emu_table_virt) >> 3,
            (ppc_state.gpr[29] - emu_table_virt) >> 3
        );
    }

    /* grab the PPC PC too */
    ppc_pc = ppc_state.pc;

    //printf("cur_instr_tab_entry = %X\n", cur_instr_tab_entry);

    /* because the first two PPC instructions for each emulated 68k opcode
       are resided in the emulator opcode table, we need to execute them
       one by one until the execution goes outside the opcode table. */
    while (power_on && ppc_pc >= cur_instr_tab_entry && ppc_pc < cur_instr_tab_entry + 8) {
        ppc_exec_single();
        ppc_pc = ppc_state.pc;
    }

    /* Getting here means we're outside the emualtor opcode table.
       Execute PPC code until we hit the opcode table again. */
    // ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
}

/** Execute emulated 68k code until target_addr is reached. */
void exec_until_68k(uint32_t target_addr)
{
    uint32_t emu_table_virt, ppc_pc;

    emu_table_virt = ppc_state.gpr[29] & 0xFFF80000;

    while (power_on && target_addr != (ppc_state.gpr[24] - 2)) {
        ppc_pc = ppc_state.pc;

        if (ppc_pc >= emu_table_virt && ppc_pc < (emu_table_virt + EMU_68K_TABLE_SIZE - 1)) {
            ppc_exec_single();
        } else {
            ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
        }
    }
}

void print_68k_regs()
{
    int i;

    for (i = 0; i < 8; i++) {
        cout << "   D" << dec << i << " : " << COUT08X << ppc_state.gpr[i+8] << endl;
    }

    for (i = 0; i < 7; i++) {
        cout << "   A" << dec << i << " : " << COUT08X << ppc_state.gpr[i+16] << endl;
    }

    cout << "   A7 : " << COUT08X << ppc_state.gpr[1] << endl;

    cout << "   PC : " << COUT08X << ppc_state.gpr[24] - 2 << endl;

    cout << "   SR : " << COUT08X << ((ppc_state.gpr[25] & 0xFF) << 8) << endl;

    cout << "  CCR : " << COUT08X << ppc_state.gpr[26] << endl;
    cout << dec << setfill(' ');
}

#endif // ENABLE_68K_DEBUGGER

static void dump_mem(string& params) {
    int cell_size, chars_per_line;
    bool is_char;
    uint32_t count, addr;
    uint64_t val;
    string num_type_str, addr_str;
    size_t separator_pos;

    separator_pos = params.find_first_of(",");
    if (separator_pos == std::string::npos) {
        cout << "dump: not enough arguments specified." << endl;
        return;
    }

    num_type_str = params.substr(0, params.find_first_of(","));
    addr_str     = params.substr(params.find_first_of(",") + 1);

    is_char = false;

    switch (num_type_str.back()) {
    case 'b':
    case 'B':
        cell_size = 1;
        break;
    case 'w':
    case 'W':
        cell_size = 2;
        break;
    case 'd':
    case 'D':
        cell_size = 4;
        break;
    case 'q':
    case 'Q':
        cell_size = 8;
        break;
    case 'c':
    case 'C':
        cell_size = 1;
        is_char   = true;
        break;
    default:
        cout << "Invalid data type " << num_type_str << endl;
        return;
    }

    try {
        num_type_str = num_type_str.substr(0, num_type_str.length() - 1);
        count        = str2addr(num_type_str);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    try {
        addr = str2addr(addr_str);
    } catch (invalid_argument& exc) {
        try {
            /* number conversion failed, trying reg name */
            addr = get_reg(addr_str);
        } catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return;
        }
    }

    cout << "Dumping memory at address " << hex << addr << ":" << endl;

    chars_per_line = 0;

    try {
        for (int i = 0; i < count; addr += cell_size, i++) {
            if (chars_per_line + cell_size * 2 > 80) {
                cout << endl;
                chars_per_line = 0;
            }
            val = mem_read_dbg(addr, cell_size);
            if (is_char) {
                cout << (char)val;
                chars_per_line += cell_size;
            } else {
                cout << COUT0_X(cell_size * 2) << val << "  ";
                chars_per_line += cell_size * 2 + 2;
            }
        }
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    cout << endl << endl;
}

static void patch_mem(string& params) {
    int value_size = 1;
    uint32_t addr, value;
    string addr_str, value_str, size_str;

    if (params.find_first_of("=") == std::string::npos) {
        cout << "setmem: not enough arguments specified. Try 'help'." << endl;
        return;
    }

    addr_str  = params.substr(0, params.find_first_of("="));
    value_str = params.substr(params.find_first_of("=") + 1);

    if (value_str.find_first_of(".") == std::string::npos) {
        cout << "setmem: no value size specified. Try 'help'." << endl;
        return;
    }

    size_str  = value_str.substr(value_str.find_first_of(".") + 1);
    value_str = value_str.substr(0, value_str.find_first_of("."));

    switch (size_str.back()) {
    case 'b':
    case 'B':
        value_size = 1;
        break;
    case 'w':
    case 'W':
        value_size = 2;
        break;
    case 'd':
    case 'D':
        value_size = 4;
        break;
    case 'q':
    case 'Q':
        value_size = 8;
        break;
    case 'c':
    case 'C':
        value_size = 1;
        break;
    default:
        cout << "Invalid value size " << size_str << endl;
        return;
    }

    try {
        addr = str2addr(addr_str);
    } catch (invalid_argument& exc) {
        try {
            // number conversion failed, trying reg name
            addr = get_reg(addr_str);
        } catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return;
        }
    }

    try {
        value = str2num(value_str);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    try {
        mem_write_dbg(addr, value, value_size);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
    }
}

static void fdd(string params) {
    string path_str;
    string param;
    size_t separator_pos;
    bool fd_write_prot = true;
    int drive = 1;

    while (1) {
        separator_pos = params.find_first_of(",");
        if (separator_pos == std::string::npos) {
            path_str = params;
            break;
        }
        else {
            param = params.substr(0, separator_pos);
            params = params.substr(separator_pos + 1);

            if (param == "w") {
                fd_write_prot = false;
            }
            else if (param == "r") {
                fd_write_prot = true;
            }
            else if (param == "1") {
                drive = 1;
            }
            else if (param == "2") {
                drive = 2;
            }
            else {
                cout << "Invalid parameter " << param << endl;
                return;
            }
        }
    }

    Swim3::Swim3Ctrl *swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name_optional("Swim3"));
    if (swim3) {
        swim3->insert_disk(drive, path_str, fd_write_prot);
    }
    else {
        cout << "Floppy controller doesn't exist." << endl;
    }
}

// 0 = all
// 1 = named
// 2 = named && !offset

static uint32_t disasm(PPCDisasmContext &ctx) {
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    bool needs_swap = false;
    if (mem_ctrl_instance != nullptr)
        needs_swap = mem_ctrl_instance->needs_swap_endian(false);
#endif

    uint32_t phys_addr;
    uint8_t* real_addr = mmu_translate_imem(ctx.instr_addr, &phys_addr);
    ctx.instr_code =
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
        needs_swap ? READ_DWORD_LE_A(real_addr) :
#endif
        READ_DWORD_BE_A(real_addr);

    int offset;
    binary_kind_t kind;
    int kinds = ctx.kinds;
    if (kinds == 1)
        kinds = -1;

    std::string name = get_name(ctx.instr_addr, phys_addr, &offset, &kind, kinds);

    if (
        (
            ctx.level == 0 || (
                !name.empty() && (
                    offset == 0 || ctx.level == 1
                )
            )
        ) && (
            ctx.kinds == 0 || (
                ctx.kinds & (1 << kind)
            ) || (
                ctx.kinds == 1 && name.empty()
            )
        )
    ) {
        ctx.diddisasm = true;
        cout << COUT08X << ctx.instr_addr;
        if (phys_addr != ctx.instr_addr) {
            cout << "->" << COUT08X << phys_addr;
        }
        if (!name.empty()) {
            cout << " " << setw(27) << left << setfill(' ') << name;
        }
        cout << ": " << COUT08X << ctx.instr_code;
        cout << "    " << disassemble_single(&ctx) << setfill(' ') << right << dec;
    }
    else {
        ctx.diddisasm = false;
    }
    return ctx.instr_addr;
}

static uint32_t disasm(uint32_t count, uint32_t address) {
    PPCDisasmContext ctx;
    ctx.instr_addr = address;
    ctx.simplified = true;
    ctx.kinds = 0;
    ctx.level = 0;
    for (int i = 0; power_on && i < count; i++) {
        disasm(ctx);
        cout << endl;
    }
    return ctx.instr_addr;
}

static void disasm_in(PPCDisasmContext &ctx, uint32_t address) {
    ctx.instr_addr = address;
    ctx.simplified = true;
    disasm(ctx);
    if (ctx.diddisasm && (ctx.regs_in.size() > 0 || ctx.regs_out.size() > 0)) {
        if (ctx.instr_str.length() < 28)
            cout << setw(28 - (int)ctx.instr_str.length()) << " ";
        cout << " ;";
        if (ctx.regs_in.size() > 0) {
            cout << " in{" << COUTX;
            for (auto & reg_name : ctx.regs_in) {
                cout << " " << reg_name << ":" << get_reg(reg_name);
            }
            cout << " }" << dec;
        }
    }
}

static void disasm_out(PPCDisasmContext &ctx) {
    if (ctx.diddisasm) {
        if (ctx.regs_out.size() > 0) {
            cout << " out{" << COUTX;
            for (auto & reg_name : ctx.regs_out) {
                cout << " " << reg_name << ":" << get_reg(reg_name);
            }
            cout << " }" << dec;
        }
        cout << endl;
    }
}

#ifdef LOG_INSTRUCTIONS
void dumpinstructionlog(uint64_t num) {
    if (InstructionNumber == 0)
        return;
    if (num > InstructionNumber) {
        num = InstructionNumber;
    }
    if (num > InstructionLogSize) {
        num = InstructionLogSize;
    }

    printf("Dumping last %llu of %llu instructions:\n", num, InstructionNumber);

    uint64_t i, end;
    i = (InstructionNumber - num) & (InstructionLogSize - 1);
    end = (InstructionNumber) & (InstructionLogSize - 1);

    do {
        if (!power_on)
            break;
        InstructionRec * irec = &InstructionLog[i];

        PPCDisasmContext ctx;
        ctx.kinds = 0;
        ctx.level = 0;
        ctx.simplified = true;
        ctx.instr_code = irec->ins;
        ctx.instr_addr = irec->addr;

        std::string name = get_name(irec->addr, irec->paddr, nullptr, nullptr, 0);

        cout << COUT08X << ctx.instr_addr;
        if (irec->paddr != irec->addr) {
            cout << "->" << COUT08X << irec->paddr;
        }
        if (!name.empty()) {
            cout << " " << setw(27) << left << setfill(' ') << name;
        }
        cout << ": " << COUT08X << ctx.instr_code;
        cout << "    " << disassemble_single(&ctx) << setfill(' ') << left << dec;

        if (ctx.instr_str.length() < 28)
            cout << setw(28 - (int)ctx.instr_str.length()) << " ";
        cout << " ;";
        bool got_msr = false;
        if (ctx.regs_in.size() > 0) {
            cout << " in{" << COUTX;
            for (auto & reg_name : ctx.regs_in) {
                cout << " " << reg_name << ":";
                if (reg_name == "msr") {
                    cout << irec->msr;
                    got_msr = true;
                }
                else
                    cout << "?";
            }
            cout << " }" << dec;
        }

        if (ctx.regs_out.size() > 0) {
            cout << " out{" << COUTX;
            for (auto & reg_name : ctx.regs_out) {
                cout << " " << reg_name << ":" << "?";
            }
            cout << " }" << dec;
        }

        if (!got_msr) {
            cout << " misc{" << COUTX;
            cout << " " << "msr" << ":";
            cout << irec->msr;
            cout << " }" << dec;
        }

        cout << endl;
        i++;
        if (i >= InstructionLogSize)
            i = 0;
    } while (i != end);
}

#endif

static void print_gprs() {
    string reg_name;
    int i;

    for (i = 0; i < 32; i++) {
        reg_name = "r" + to_string(i);

        cout << right << std::setw(5) << setfill(' ') << reg_name << " : " <<
            COUT08X << get_reg(reg_name) << setfill(' ');

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }
    }

    array<string, 8> sprs = {"pc", "lr", "cr", "ctr", "xer", "msr", "srr0", "srr1"};

    for (auto &spr : sprs) {
        cout << right << std::setw(5) << setfill(' ') << spr << " : " <<
            COUT08X << get_reg(spr) << setfill(' ');

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }

        i++;
    }
}

static void print_fprs() {
    string reg_name;
    for (int i = 0; i < 32; i++) {
        reg_name = "f" + to_string(i);
        cout << right << std::setw(6) << setfill(' ') << reg_name << " : " <<
            COUT016X << ppc_state.fpr[i].int64_r <<
            " = " << left << setfill(' ') << ppc_state.fpr[i].dbl64_r << endl;
    }
    cout << right << std::setw(6) << setfill(' ') << "fpscr" << " : " <<
        COUT08X << ppc_state.fpscr << setfill(' ') << endl;
}

extern bool is_601;

static void print_mmu_regs()
{
    printf(" msr : %08X\n", ppc_state.msr);

    printf("\nBAT registers:\n");

    for (int i = 0; i < 4; i++) {
        printf(" ibat%du : %08X   ibat%dl : %08X\n",
              i, ppc_state.spr[528+i*2],
              i, ppc_state.spr[529+i*2]);
    }

    if (!is_601) {
        for (int i = 0; i < 4; i++) {
            printf(" dbat%du : %08X   dbat%dl : %08X\n",
                  i, ppc_state.spr[536+i*2],
                  i, ppc_state.spr[537+i*2]);
        }
    }

    printf("\n");
    printf(" sdr1 : %08X\n", ppc_state.spr[SPR::SDR1]);
    printf("\nSegment registers:\n");

    for (int i = 0; i < 16; i++) {
        printf(" %ssr%d : %08X\n", (i < 10) ? " " : "", i, ppc_state.sr[i]);
    }
}

#ifndef _WIN32

#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

static struct sigaction    old_act_sigint, new_act_sigint;
static struct sigaction    old_act_sigterm, new_act_sigterm;
static struct termios      orig_termios;

static void mysig_handler(int signum)
{
    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act_sigint.sa_handler);
    signal(SIGTERM, old_act_sigterm.sa_handler);

    LOG_F(INFO, "Old terminal state restored, SIG#=%d", signum);

    // re-post signal
    raise(signum);
}
#endif

static void delete_prompt() {
#ifndef _WIN32
    // move up, carriage return (move to column 0), erase from cursor to end of line
    cout << "\e[A\r\e[0K";
#endif
}

DppcDebugger::DppcDebugger() {

}

static bool in_getline = false;

void DppcDebugger::enter_debugger() {
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd, reg_value_str,
           inst_string, inst_num_str, profile_name, sub_cmd;
    uint32_t addr, inst_grab;
    std::stringstream ss;
    int log_level, context;
    size_t separator_pos;
    bool did_message = false;
    uint32_t next_addr_ppc;
#ifdef ENABLE_68K_DEBUGGER
    uint32_t next_addr_68k;
#endif
    bool cmd_repeat;
    int repeat_count = 0;
    bool is_sq;

    unique_ptr<OfConfigUtils> ofnvram = unique_ptr<OfConfigUtils>(new OfConfigUtils);

    context = 1; /* start with the PowerPC context */

#ifndef _WIN32
    struct winsize win_size_previous;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size_previous);
#endif

    while (1) {
        if (power_off_reason == po_shut_down) {
            power_off_reason = po_shutting_down;
            break;
        }
        if (power_off_reason == po_restart) {
            power_off_reason = po_restarting;
            break;
        }
        if (power_off_reason == po_quit) {
            power_off_reason = po_quitting;
            break;
        }
        power_on = true;

        if (power_off_reason == po_starting_up) {
            power_off_reason = po_none;
            cmd = "go";
        }
        else if (power_off_reason == po_disassemble_on) {
            inp = "si 1000000000";
            ss.str("");
            ss.clear();
            ss.str(inp);
            ss >> cmd;
        }
        else if (power_off_reason == po_disassemble_off) {
            power_off_reason = po_none;
            cmd = "go";
        }
        else
        {
            if (power_off_reason == po_enter_debugger) {
                power_off_reason = po_entered_debugger;
            }
            if (!did_message) {
                cout << endl;
                cout << "Welcome to the DingusPPC command line debugger." << endl;
                cout << "Please enter a command or 'help'." << endl << endl;
                did_message = true;
            }

            printf("%08X: dingusdbg> ", ppc_state.pc);

            while (power_on) {
                /* reset string stream */
                ss.str("");
                ss.clear();

                cmd = "";
                std::cin.clear();
                #ifndef _WIN32 // todo: fixme FlushConsoleInputBuffer
                tcflush(STDIN_FILENO, TCIFLUSH);
                #endif
                in_getline = true;
                getline(cin, inp, '\n');
                ss.str(inp);
                ss >> cmd;

    #ifndef _WIN32
                struct winsize win_size_current;
                ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size_current);
                if (win_size_current.ws_col != win_size_previous.ws_col || win_size_current.ws_row != win_size_previous.ws_row) {
                    win_size_previous = win_size_current;
                    if (cmd.empty()) {
                        continue;
                    }
                }
    #endif
                in_getline = false;
                break;
            }
        }

        if (power_off_reason == po_signal_interrupt) {
            if (in_getline) {
                power_off_reason = po_enter_debugger;
                // ignore command if interrupt happens because the input line is probably incomplete.
                last_cmd = "";
                continue;
            }
            power_on = true;
            power_off_reason = po_entered_debugger;
        }

        if (feof(stdin)) {
            printf("eof -> quit\n");
            cmd = "quit";
        }

        cmd_repeat = cmd.empty() && !last_cmd.empty();
        if (cmd_repeat) {
            cmd = last_cmd;
            repeat_count++;
        }
        else {
            repeat_count = 1;
        }
        if (cmd == "help") {
            cmd = "";
            show_help();
        } else if (cmd == "quit") {
            cmd = "";
            break;
        } else if (cmd == "restart") {
            cmd = "";
            power_on = false;
            power_off_reason = po_restart;
        } else if (cmd == "profile") {
            cmd = "";
            ss >> sub_cmd;
            ss >> profile_name;

            if (sub_cmd == "show") {
                gProfilerObj->print_profile(profile_name);
            } else if (sub_cmd == "reset") {
                gProfilerObj->reset_profile(profile_name);
            } else {
                cout << "Unknown/empty subcommand " << sub_cmd << endl;
            }
        }
        else if (cmd == "regs") {
            cmd = "";
#ifdef ENABLE_68K_DEBUGGER
            if (context == 2 || (context == 3 && get_context() == 2)) {
                print_68k_regs();
            } else
#endif
            {
                print_gprs();
            }
        } else if (cmd == "fregs") {
            cmd = "";
            print_fprs();
        } else if (cmd == "mregs") {
            cmd = "";
            print_mmu_regs();
        } else if (cmd == "set") {
            ss >> expr_str;

            separator_pos = expr_str.find_first_of("=");
            if (separator_pos == std::string::npos) {
                cout << "set: not enough arguments specified." << endl;
                continue;
            }

            try {
                reg_expr = expr_str.substr(0, expr_str.find_first_of("="));
                addr_str = expr_str.substr(expr_str.find_first_of("=") + 1);
                if (reg_expr == "loglevel") {
                    log_level = str2num(addr_str);
                    if (log_level < -2 || log_level > 9) {
                        cout << "Log level must be in the range -2...9!" << endl;
                        continue;
                    }
                    loguru::g_stderr_verbosity = log_level;
                } else {
                    addr = str2addr(addr_str);
                    set_reg(reg_expr, addr);
                }
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        } else if ((is_sq = (cmd == "sq")) || cmd == "step" || cmd == "si") {
            int count;

            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                try {
                    count = str2num(expr_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    count = 1;
                }
            } else {
                count = 1;
            }

            if (cmd_repeat) {
                delete_prompt();
            }
            for (; count > 0; count--) {
#ifdef ENABLE_68K_DEBUGGER
                if ((context == 2 || (context == 3 && get_context() == 2)) && exec_upto_68k_opcode(context == 3)) {
                    if (!power_on)
                        break;
                    DisasmContext68K ctx;
                    if (!is_sq) {
                        addr = static_cast<uint32_t>(ppc_state.gpr[24] - 2);
                        disasm_68k_in(ctx, addr);
                    }
                    exec_single_68k();
                    if (!is_sq) {
                        disasm_68k_out(ctx);
                    }
                } else
#endif
                {
                    if (!power_on)
                        break;
                    PPCDisasmContext ctx;
                    if (!is_sq) {
                        addr = ppc_state.pc;
                        ctx.kinds = 0; // (1 << kind_darwin_kernel) | (1 << kind_darwin_kext);
                        ctx.level = 0;
                        disasm_in(ctx, addr);
                    }
                    ppc_exec_single();
                    if (!is_sq) {
                        disasm_out(ctx);
                    }
                }
            }
        } else if (cmd == "next" || cmd == "ni") {
            addr = ppc_state.pc + 4;
            ppc_exec_until(addr);
        } else if (cmd == "until") {
            if (cmd_repeat) {
                delete_prompt();
                cout << repeat_count << "> " << cmd << " " << addr_str << endl;
            }
            else {
                ss >> addr_str;
            }
            try {
                addr = str2addr(addr_str);
#ifdef ENABLE_68K_DEBUGGER
                if ((context == 2 || (context == 3 && get_context() == 2)) && exec_upto_68k_opcode(context == 3)) {
                    exec_until_68k(addr);
                } else
#endif
                {
                    ppc_exec_until(addr);
                }
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        } else if (cmd == "go") {
            cmd = "";
            power_on = true;
            ppc_exec();
        } else if (cmd == "disas" || cmd == "da") {
            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                separator_pos = expr_str.find_first_of(",");
                if (separator_pos == std::string::npos) {
                    cout << "disas: not enough arguments specified." << endl;
                    continue;
                }

                inst_num_str = expr_str.substr(0, expr_str.find_first_of(","));
                try {
                    inst_grab = str2num(inst_num_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    continue;
                }

                addr_str = expr_str.substr(expr_str.find_first_of(",") + 1);
                try {
                    addr = str2addr(addr_str);
                } catch (invalid_argument& exc) {
                    try {
                        /* number conversion failed, trying reg name */
#ifdef ENABLE_68K_DEBUGGER
                        if ((context == 2 || (context == 3 && get_context() == 2)) && (addr_str == "pc" || addr_str == "PC")) {
                            addr = ppc_state.gpr[24] - 2;
                        } else
#endif
                        {
                            addr = static_cast<uint32_t>(get_reg(addr_str));
                        }
                    } catch (invalid_argument& exc) {
                        cout << exc.what() << endl;
                        continue;
                    }
                }
                try {
#ifdef ENABLE_68K_DEBUGGER
                    if (context == 2 || (context == 3 && get_context() == 2)) {
                        next_addr_68k = disasm_68k(inst_grab, addr);
                    } else
#endif
                    {
                        next_addr_ppc = disasm(inst_grab, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            } else {
                /* disas without arguments defaults to disas 1,pc */
                try {
#ifdef ENABLE_68K_DEBUGGER
                    if (context == 2 || (context == 3 && get_context() == 2)) {
                        if (cmd_repeat) {
                            delete_prompt();
                            addr = next_addr_68k;
                        }
                        else {
                            addr = static_cast<uint32_t>(ppc_state.gpr[24] - 2);
                        }
                        next_addr_68k = disasm_68k(1, addr);
                    } else
#endif
                    {
                        if (cmd_repeat) {
                            delete_prompt();
                            addr = next_addr_ppc;
                        }
                        else {
                            addr = static_cast<uint32_t>(ppc_state.pc);
                        }
                        next_addr_ppc = disasm(1, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            }
#ifdef DECREMENTER_TOGGLE
        } else if (cmd == "disabledecrementer") {
            cmd = "";
            decrementer_enabled = false;
        } else if (cmd == "enabledecrementer") {
            cmd = "";
            decrementer_enabled = true;
#endif
        } else if (cmd == "backtrace" || cmd == "bt") {
            cmd = "";
            dump_backtrace();
#ifdef LOG_INSTRUCTIONS
        } else if (cmd == "dumpinstructionlog") {
            cmd = "";
            uint64_t count;
            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                try {
                    count = str2num(expr_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    count = 0;
                }
            } else {
                count = InstructionNumber;
            }
            if (count)
                dumpinstructionlog(count);
        } else if (cmd == "clearinstructionlog") {
            cmd = "";
            InstructionNumber = 0;
#endif
        } else if (cmd == "dumpdmaprogram") {
            cmd = "";
            uint32_t cmd_ptr;
            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                try {
                    cmd_ptr = str2num(expr_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    cmd_ptr = 0;
                }
            } else {
                cmd_ptr = 0;
            }
            DMAChannel::dump_program(cmd_ptr, -1);
        } else if (cmd == "dump") {
            expr_str = "";
            ss >> expr_str;
            dump_mem(expr_str);
        } else if (cmd == "setmem") {
            expr_str = "";
            ss >> expr_str;
            patch_mem(expr_str);
#ifdef ENABLE_68K_DEBUGGER
        } else if (cmd == "context") {
            cmd = "";
            expr_str = "";
            ss >> expr_str;
            if (expr_str == "ppc" || expr_str == "PPC") {
                context = 1;
            } else if (expr_str == "68k" || expr_str == "68K") {
                context = 2;
            } else if (expr_str == "auto" || expr_str == "AUTO") {
                context = 3;
            } else {
                cout << "Unknown debugging context: " << expr_str << endl;
            }
#endif
        } else if (cmd == "regions") {
            cmd = "";
            if (mem_ctrl_instance)
                mem_ctrl_instance->dump_regions();
        } else if (cmd == "devices") {
            cmd = "";
            gMachineObj->dump_devices(4);
        } else if (cmd == "fdd") {
            cmd = "";
            std::istream::sentry se(ss); // skip white space
            getline(ss, expr_str); // get everything up to eol
            fdd(expr_str);
        } else if (cmd == "printenv") {
            cmd = "";
            if (ofnvram->init())
                continue;
            ofnvram->printenv();
        } else if (cmd == "setenv") {
            cmd = "";
            string var_name, value;
            ss >> var_name;
            std::istream::sentry se(ss); // skip white space
            getline(ss, value); // get everything up to eol
            if (ofnvram->init()) {
                cout << " Cannot open NVRAM" << endl;
                continue;
            }
            if (!ofnvram->setenv(var_name, value)) {
                cout << " Please try again" << endl;
            } else {
                cout << " ok" << endl; // mimic Forth
            }
 #ifndef _WIN32
        } else if (cmd == "nvedit") {
            cmd = "";
            cout << "===== press CNTRL-C to save =====" << endl;

            // save original terminal state
            tcgetattr(STDIN_FILENO, &orig_termios);
            struct termios new_termios = orig_termios;

            new_termios.c_cflag &= ~(CSIZE | PARENB);
            new_termios.c_cflag |= CS8;

            new_termios.c_lflag &= ~(ISIG | NOFLSH | ICANON | ECHOCTL);
            new_termios.c_lflag |= NOFLSH | ECHONL;

            // new_termios.c_iflag &= ~(ICRNL | IGNCR);
            // new_termios.c_iflag |= INLCR;

            // new_termios.c_oflag &= ~(ONOCR | ONLCR);
            // new_termios.c_oflag |= OPOST | OCRNL | ONLRET;

            tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

            // save original signal handler for SIGINT
            // then redirect SIGINT to new handler
            memset(&new_act_sigint, 0, sizeof(new_act_sigint));
            new_act_sigint.sa_handler = mysig_handler;
            sigaction(SIGINT, &new_act_sigint, &old_act_sigint);

            // save original signal handler for SIGTERM
            // then redirect SIGTERM to new handler
            memset(&new_act_sigterm, 0, sizeof(new_act_sigterm));
            new_act_sigterm.sa_handler = mysig_handler;
            sigaction(SIGTERM, &new_act_sigterm, &old_act_sigterm);

            getline(cin, inp, '\x03');

            // restore original terminal state
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

            // restore original signal handler for SIGINT
            signal(SIGINT, old_act_sigint.sa_handler);

            // restore original signal handler for SIGTERM
            signal(SIGTERM, old_act_sigterm.sa_handler);

            if (ofnvram->init())
                continue;
            ofnvram->setenv("nvramrc", inp);
#endif
#ifdef DEBUG_CPU_INT
        } else if (cmd == "nmi") {
            cmd = "";
            InterruptCtrl* int_ctrl = dynamic_cast<InterruptCtrl*>(
                gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
            int_ctrl->ack_int(int_ctrl->register_dev_int(IntSrc::NMI), 1);
        } else if (cmd == "amicint") {
            cmd = "";
            string value;
            int irq_id;
            ss >> value;
            try {
                irq_id = str2num(value);
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
                continue;
            }
            InterruptCtrl* int_ctrl = dynamic_cast<InterruptCtrl*>(
                gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
            int_ctrl->ack_int(irq_id, 1);
        } else if (cmd == "viaint") {
            cmd = "";
            string value;
            int irq_bit;
            ss >> value;
            try {
                irq_bit = str2num(value);
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
                continue;
            }
            TimerManager::get_instance()->add_oneshot_timer(0, [irq_bit](){
                ViaCuda* via_obj = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));
                via_obj->assert_int(irq_bit);
            });
#endif
        }
        else if (cmd == "showalltasks"  ) { cmd = ""; showalltasks  (); }
        else if (cmd == "showallacts"   ) { cmd = ""; showallacts   (); }
        else if (cmd == "showallstacks" ) { cmd = ""; showallstacks (); }
        else if (cmd == "showallvm"     ) { cmd = ""; showallvm     (); }
        else if (cmd == "showallvme"    ) { cmd = ""; showallvme    (); }
        else if (cmd == "showallipc"    ) { cmd = ""; showallipc    (); }
        else if (cmd == "showallrights" ) { cmd = ""; showallrights (); }
        else if (cmd == "showallkmods"  ) { cmd = ""; showallkmods  (); }
        else if (cmd == "zprint"        ) { cmd = ""; zprint        (); }
        else if (cmd == "paniclog"      ) { cmd = ""; paniclog      (); }
#define ONEARG string value; int arg0; ss >> value; try { arg0 = str2num(value); } \
    catch(invalid_argument& exc) { cout << exc.what() << endl; continue; }
        else if (cmd == "showtask"      ) { cmd = ""; ONEARG showtask      (arg0); }
        else if (cmd == "showtaskacts"  ) { cmd = ""; ONEARG showtaskacts  (arg0); }
        else if (cmd == "showtaskstacks") { cmd = ""; ONEARG showtaskstacks(arg0); }
        else if (cmd == "showtaskvm"    ) { cmd = ""; ONEARG showtaskvm    (arg0); }
        else if (cmd == "showtaskvme"   ) { cmd = ""; ONEARG showtaskvme   (arg0); }
        else if (cmd == "showtaskipc"   ) { cmd = ""; ONEARG showtaskipc   (arg0); }
        else if (cmd == "showtaskrights") { cmd = ""; ONEARG showtaskrights(arg0); }
        else if (cmd == "showact"       ) { cmd = ""; ONEARG showact       (arg0); }
        else if (cmd == "showactstack"  ) { cmd = ""; ONEARG showactstack  (arg0); }
        else if (cmd == "showmap"       ) { cmd = ""; ONEARG showmap       (arg0); }
        else if (cmd == "showmapvme"    ) { cmd = ""; ONEARG showmapvme    (arg0); }
        else if (cmd == "showipc"       ) { cmd = ""; ONEARG showipc       (arg0); }
        else if (cmd == "showrights"    ) { cmd = ""; ONEARG showrights    (arg0); }
        else if (cmd == "showpid"       ) { cmd = ""; ONEARG showpid       (arg0); }
        else if (cmd == "showproc"      ) { cmd = ""; ONEARG showproc      (arg0); }
        else if (cmd == "showkmod"      ) { cmd = ""; ONEARG showkmod      (arg0); }
        else if (cmd == "switchtoact"   ) { cmd = ""; ONEARG switchtoact   (arg0); }
        else if (cmd == "switchtoctx"   ) { cmd = ""; ONEARG switchtoctx   (arg0); }
        else if (cmd == "showkmodaddr"  ) { cmd = ""; ONEARG showkmodaddr  (arg0); printf("\n"); }
        else {
            if (!cmd.empty()) {
                cout << "Unknown command: " << cmd << endl;
                cmd = "";
            }
        }
        last_cmd = cmd;
    }
}
