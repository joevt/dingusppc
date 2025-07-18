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

#ifndef SYMBOLS_OPENFIRMWARE_H_
#define SYMBOLS_OPENFIRMWARE_H_

#include <string>

std::string get_name_OpenFirmware(uint32_t addr, uint32_t addr_p, int *offset, bool append_offset = false);
bool lookup_name_OpenFirmware(const std::string &name, uint32_t &addr);

#endif // SYMBOLS_OPENFIRMWARE_H_
