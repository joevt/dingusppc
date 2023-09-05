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

#ifndef KGMACROS_H_
#define KGMACROS_H_

#include <vector>

typedef struct {
    // [ start of guest kmod_info_t
    uint32_t    next;
    uint32_t    info_version;
    uint32_t    id;
    char        name[64];
    char        version[64];
    int32_t     reference_count;
    uint32_t    reference_list;
    uint32_t    address;
    uint32_t    size;
    uint32_t    hdr_size;
    uint32_t    start;
    uint32_t    stop;
    // ] end of guest kmod_info_t
    uint32_t    kmod; // guest virtual address pointer to kmod info
} kmod_info_t;

std::vector<kmod_info_t> get_kmod_infos();


void kgm();
void showkmodaddr(uint32_t arg0);
void showkmod(uint32_t arg0);
void showact(uint32_t arg0);
void showactstack(uint32_t arg0);
void showallacts();
void showallstacks();
void showmapvme(uint32_t arg0);
void showmap(uint32_t arg0);
void showallvm();
void showallvme();
void showipc(uint32_t arg0);
void showrights(uint32_t arg0);
void showtaskipc(uint32_t arg0);
void showtaskrights(uint32_t arg0);
void showallipc();
void showallrights();
void showtaskvm(uint32_t arg0);
void showtaskvme(uint32_t arg0);
void showtask(uint32_t arg0);
void showtaskacts(uint32_t arg0);
void showtaskstacks(uint32_t arg0);
void showalltasks();
void showpid(uint32_t arg0);
void showproc(uint32_t arg0);
void zprint();
void switchtoact(uint32_t arg0);
void switchtoctx(uint32_t arg0);
void resetctx();
void paniclog();

void showallkmods();

#endif // KGMACROS_H_
