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

/** @file Apple Network Server LCD. */

#ifndef LCDANS_H
#define LCDANS_H

#include <devices/ioctrl/macio.h>
#include <cinttypes>
#include <memory>

enum LcdAnsDataSource {
    DDRAM,
    CGRAM,
};

class LcdAns: public IobusDevice {

public:
    LcdAns(const std::string &dev_name);

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<LcdAns>(new LcdAns(dev_name));
    }

protected:

    // HWComponent methods
    PostInitResultType device_postinit() override;

    // IobusDevice methods
    uint16_t iodev_read(uint32_t address) override;
    void iodev_write(uint32_t address, uint16_t value) override;

private:
    std::string data_source_str();

    bool                DL_data_length = 1;     // 0 = 4-bit                    ; 1 = 8-bit
    bool                N_lines = 0;            // 0 = 1 line                   ; 1 = 2 lines
    bool                F_font = 0;             // 0 = 5x8 dots                 ; 1 = 5x10 dots
    bool                D_display = 0;
    bool                C_cursor = 0;
    bool                B_blinking = 0;
    bool                ID_direction = 1;
    bool                BF_busy_flag = 0;       // 0 = Instructions acceptable ; 1 = Internally operating
    bool                S_shift = 0;
    uint8_t             shift_position = 0;
    uint8_t             AC_address_counter = 0;
    LcdAnsDataSource    data_source = LcdAnsDataSource::DDRAM;

    uint8_t             characters = 80;
    uint8_t             width = 20;
    uint8_t             height = characters / width;
    uint8_t             ddram[80] = {};
    uint8_t             cgram[64] = {};
    uint8_t             cgrom[256][16] = {};
};

#endif // LCDANS_H
