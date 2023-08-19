/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#include "symbols.h"
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <debugger/backtrace.h>
#include <stdexcept>

using namespace std;

#define DUMPFRAMES 64
#define LRindex 2
#define VM_MAX_KERNEL_ADDRESS (0x3fffffff)
#define PPC_SID_KERNEL  0

/*
Original implementation: xnu-344/osfmk/ppc/model_dep.c
*/

static void dump_backtrace_open_firmware() {
    /* Scan the return stack */
    uint32_t raddr;
    uint32_t stackptr = ppc_state.gpr[30];
    if ((int32_t)stackptr >= 0 || stackptr & 3)
        return;

    std::string name;

    mmu_translate_dbg(ppc_state.spr[SPR::LR], raddr);
    name = get_name(ppc_state.spr[SPR::LR], raddr, nullptr, nullptr, 0);
    printf("         0x%08X %s ; LR\n", ppc_state.spr[SPR::LR], name.c_str()); /* Dump the pc */

    mmu_translate_dbg(ppc_state.gpr[19], raddr);
    name = get_name(ppc_state.gpr[19], raddr, nullptr, nullptr, 0);
    printf("         0x%08X %s ; rTOR\n", ppc_state.gpr[19], name.c_str()); /* r19 is the top of return stack */

    do {
        if ((stackptr & 0x3ff) == 0) /* The first of the stack is 1024 bytes aligned so stop there. */
            break;
        uint32_t returnaddr = (uint32_t)mem_read_dbg(stackptr, 4);
        if (!returnaddr)
            break;
        mmu_translate_dbg(returnaddr, raddr);
        name = get_name(returnaddr, raddr, nullptr, nullptr, 0);
        printf("         0x%08X %s\n", returnaddr, name.c_str());
        stackptr += 4; /* The stack grows down so go up toward first of the stack. */
    } while(1);
}

static void dump_backtrace_ppc(uint32_t stackptr, uint32_t fence = 0xffffffff) {
    uint32_t bframes[DUMPFRAMES];
    uint32_t sframe[8], raddr;
    int i;
    std::string name;

    for (i = 0; i < DUMPFRAMES; i++) { /* Dump up to max frames */

        if(!stackptr || (stackptr == fence)) {
            break; /* Hit stop point or end... */
        }

        if(stackptr & 0x00000003) { /* Is stack pointer valid? */
            printf("         backtrace terminated - unaligned frame address: 0x%08X\n", stackptr); /* No, tell 'em */
            break;
        }

        try {
#if 0
            //raddr = (uint32_t)LRA(PPC_SID_KERNEL, (void *)stackptr); /* Get physical frame address */
            mmu_translate_dbg(stackptr, raddr);

            if(!raddr || (stackptr > VM_MAX_KERNEL_ADDRESS)) { /* Is it mapped? */
                printf("         backtrace terminated - frame not mapped or invalid: 0x%08X\n", stackptr); /* No, tell 'em */
                break;
            }

            if(raddr >= mem_actual) { /* Is it within physical RAM? */
                printf("         backtrace terminated - frame outside of RAM: v=0x%08X, p=%08X\n",
                    stackptr, raddr); /* No, tell 'em */
                break;
            }
#endif
            //ReadReal(raddr, &sframe[0]); /* Fetch the stack frame */
            sframe[0] = (uint32_t)mem_read_dbg(stackptr, 4);
            sframe[1] = (uint32_t)mem_read_dbg(stackptr + 4, 4);
            sframe[2] = (uint32_t)mem_read_dbg(stackptr + 8, 4);

            bframes[i] = sframe[LRindex]; /* Save the link register */

            mmu_translate_dbg(bframes[i], raddr);
            name = get_name(bframes[i], raddr, nullptr, nullptr, 0);
            printf("         0x%08X %s\n", bframes[i], name.c_str()); /* Dump the link register */

            stackptr = sframe[0]; /* Chain back */
        }
        catch (std::invalid_argument& exc) {
            printf("         backtrace terminated - frame not mapped or invalid: 0x%08X\n", stackptr); /* No, tell 'em */
            break;
        }
        catch (...) {
            printf("         backtrace terminated - frame not mapped or invalid (miscellaneous error): 0x%08X\n",
                stackptr); /* No, tell 'em */
            break;
        }
    }
    printf("\n");
    if(i >= DUMPFRAMES) printf("      backtrace continues...\n"); /* Say we terminated early */
    //if(i) kmod_dump((vm_offset_t *)&bframes[0], i); /* Show what kmods are in trace */
}

void dump_backtrace(uint32_t stackptr, uint32_t fence = 0xffffffff) {
    uint32_t raddr;
    std::string name;

    printf("      Backtrace:\n");

    mmu_translate_dbg(ppc_state.pc, raddr);
    binary_kind_t kind;
    name = get_name(ppc_state.pc, raddr, nullptr, &kind, 0);
    printf("         0x%08X %s ; PC\n", ppc_state.pc, name.c_str()); /* Dump the pc */

    if (kind == kind_open_firmware) {
        dump_backtrace_open_firmware();
        // Some special code is required to get from Open Firmware context to Client Iterface context.
        //dump_backtrace_ppc(stackptr, fence);
    }
    else {
        dump_backtrace_ppc(stackptr, fence);
        // Some special code is required to get from Client Iterface context to Open Firmware context.
        //dump_backtrace_open_firmware();
    }
}

void dump_backtrace() {
    dump_backtrace(ppc_state.gpr[1]);
}
