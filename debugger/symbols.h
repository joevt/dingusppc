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

#ifndef SYMBOLS_H_
#define SYMBOLS_H_

#include <cstdint>
#include <string>
#include <vector>

typedef enum {
    kind_unknown,
    kind_open_firmware,
    kind_darwin_kernel,
    kind_darwin_kext,
    kind_darwin_process,
    kind_darwin_library,
} binary_kind_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    std::string name;
} symbol_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    std::string name;
    std::vector<symbol_t> symbols;
} section_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    std::string name;
    std::vector<section_t> sections;
    std::vector<symbol_t> symbols;
} segment_t;

typedef struct {
    uint32_t start;
    uint32_t end;
    std::string name;
    binary_kind_t kind;
    std::vector <segment_t> segments;
    std::vector<symbol_t> symbols;
} binary_t;

void load_symbols(const std::string &path);
std::string get_offset_string(const std::string &name, int offset, int *offset_out);
std::string get_name(uint32_t addr, uint32_t addr_p = 0, int *offset = nullptr, binary_kind_t *kind = nullptr, int kinds = 0);
bool lookup_name_kernel(const std::string &name, uint32_t &addr);
bool lookup_name(binary_kind_t kind, const std::string &name, uint32_t &addr);
void showallkmods();

#endif // SYMBOLS_H_
