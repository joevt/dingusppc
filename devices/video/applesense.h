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

/** @file Apple Monitor Sense Codes

    Apple monitor sense is a method for display identification
    as described in the Technical Note HW30.
 */

#ifndef APPLE_SENSE_H
#define APPLE_SENSE_H

#include <map>
#include <string>
#include <cstdint>

typedef struct {
    uint16_t      h;
    uint16_t      v;
    float         pixel_clock; // MHz
    float         h_freq;      // kHz
    float         refresh;     // Hz
} MonitorRes;

typedef struct {
    uint8_t       std_sense_code;
    uint8_t       ext_sense_code;
    const char *  apple_enum;
    const char *  name;
    const char *  description;
    MonitorRes    resolutions[10];
} MonitorInfo;

/** Mapping between monitor IDs and their sense codes. */
extern const std::map<std::string, MonitorInfo> MonitorIdToCode;

extern const std::map<std::string, std::string> MonitorAliasToId;

#endif /* APPLE_SENSE_H */
