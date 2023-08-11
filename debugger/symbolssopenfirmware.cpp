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

#include "symbolsopenfirmware.h"
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include "memaccess.h"
#include <iostream>
#include "symbols.h"

using namespace std;

enum {
    fdefd       = 0x80,
    fimm        = 0x40,
    fnohdr      = 0x20,
    falias      = 0x10,
    finstance   = 0x08,
    fvisible    = 0x04,
    finvisible  = 0x02,
    fvectored   = 0x01,
};

enum {
    ctype_colon      = 0xb7,
    ctype_value      = 0xb8,
    ctype_variable   = 0xb9,
    ctype_constant   = 0xba,
    ctype_create     = 0xbb,
    ctype_defer      = 0xbc,
    ctype_buffer     = 0xbd,
    ctype_field      = 0xbe,
    ctype_code       = 0xbf,
    ctype_settoken   = 0xdb,
};

std::string get_name_OpenFirmware(uint32_t addr, uint32_t addr_p, int *offset) {
    std::string str;

/*
    if (addr == 0x004097B0) {
        LOG_F(WARNING, "you are here");
    }
*/

    static uint32_t start_vector_ptr_saved = 0;
    static uint32_t start_vector_ptr_saved_p = 0;

    do {
        uint32_t start_vector_ptr = (uint32_t)get_reg("r25");
        if ((int32_t)(start_vector_ptr) >= 0)
            break;
        uint32_t here = (uint32_t)get_reg("r16");
        if ((int32_t)(start_vector_ptr) >= 0)
            break;

        if (addr == addr_p) {
            if (start_vector_ptr != start_vector_ptr_saved)
                break;
            here = here - start_vector_ptr + start_vector_ptr_saved_p;
            start_vector_ptr = start_vector_ptr_saved_p;
        }

        if (start_vector_ptr <= addr && addr < here) {
            uint32_t begin_addr;
            uint32_t prev_begin_addr;

            uint32_t  h_link;
            uint8_t   h_flags;
            uint8_t   h_ctype;
            uint16_t  h_token;
            uint64_t  h_name[256/8+1]; // includes h_count

            uint64_t val;

            begin_addr = addr & ~7;
            while (begin_addr >= start_vector_ptr) {
                try {
                    val = mem_read_dbg(begin_addr, 8);
                } catch (invalid_argument& exc) {
                    //cout << "get_name caused exception: " << exc.what() << endl;
                    break;
                }
                h_link = val >> 32;
                prev_begin_addr = 0;
                if ((h_link & 0xFFF00007) == 0xFFF00000)
                    prev_begin_addr = begin_addr + h_link;
                if (h_link == 0 ||
                    (start_vector_ptr <= prev_begin_addr && prev_begin_addr <= begin_addr)
                ) { // multiple of 8 and less than a megabyte
                    h_flags = val >> 24;
                    if (h_flags & fdefd) {
                        h_ctype = val >> 16;
                        if ((h_ctype >= ctype_colon && h_ctype <= ctype_code) || h_ctype == ctype_settoken) {
                            if (h_flags & fnohdr) {
                                h_token = val;
                                switch (h_ctype) {
                                    case ctype_colon    : snprintf((char*)h_name, sizeof(h_name), "colon_%x", h_token); break;
                                    case ctype_value    : snprintf((char*)h_name, sizeof(h_name), "value_%x", h_token); break;
                                    case ctype_variable : snprintf((char*)h_name, sizeof(h_name), "variable_%x", h_token); break;
                                    case ctype_constant : snprintf((char*)h_name, sizeof(h_name), "constant_%x", h_token); break;
                                    case ctype_create   : snprintf((char*)h_name, sizeof(h_name), "create_%x", h_token); break;
                                    case ctype_defer    : snprintf((char*)h_name, sizeof(h_name), "defer_%x", h_token); break;
                                    case ctype_buffer   : snprintf((char*)h_name, sizeof(h_name), "buffer_%x", h_token); break;
                                    case ctype_field    : snprintf((char*)h_name, sizeof(h_name), "field_%x", h_token); break;
                                    case ctype_code     : snprintf((char*)h_name, sizeof(h_name), "code_%x", h_token); break;
                                    case ctype_settoken : snprintf((char*)h_name, sizeof(h_name), "settoken_%x", h_token); break;
                                }
                                str = (char*)h_name;
                                begin_addr = begin_addr + 8;
                            }
                            else {
                                unsigned int i = 0;
                                do {
                                    try {
                                        val = mem_read_dbg(begin_addr + 8 + i, 8);
                                    }
                                    catch (invalid_argument& exc) {
                                        cout << "get_name caused exception while getting name: " << exc.what() << endl;
                                        val = 0;
                                    }
                                    WRITE_QWORD_BE_A(&(((char*)h_name)[i]), val);
                                    i += 8;
                                } while (i <= *(char*)h_name);
                                ((char*)h_name)[i] = '\0';
                                str = (((char*)h_name) + 1);
                                begin_addr = begin_addr + 8 + i;
                            }

                            str = get_offset_string(str, addr - begin_addr, offset);
                            if (start_vector_ptr_saved == 0) {
                                start_vector_ptr_saved = start_vector_ptr;
                                mmu_translate_dbg(start_vector_ptr, start_vector_ptr_saved_p);
                            }
                            break;
                        } // if ctype is valid
                    } // if defined
                } // if hlink is multiple of 8 and less than a megabyte
                begin_addr -= 8;
            } // while begin_addr >= start_vector_ptr
        } // if addr between startvec and here

    } while (0);
    return str;
}
