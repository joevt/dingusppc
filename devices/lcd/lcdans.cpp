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

#include <devices/common/machineid.h>
#include <devices/deviceregistry.h>
#include <devices/lcd/lcdans.h>
#include <loguru.hpp>

namespace loguru {
    enum : Verbosity {
        Verbosity_VLCDANS = loguru::Verbosity_WARNING,
        Verbosity_VWRITEDDRAM =  loguru::Verbosity_9,
        Verbosity_VTIMEBASE = loguru::Verbosity_ERROR,
    };
}

namespace LcdAnsReg {

enum LcdAnsReg : uint8_t {
    LCD_R_COMMAND   = 0x00,
    LCD_S_DATA      = 0x01,
    SYNC_TIMEBASE   = 0x02, // write bit 15: 0=disable 604 CPU timebase; 1=enable 604 CPU timebase
    UNKNOWN         = 0x03, // write FFFF
};

} // namespace LcdAnsReg

LcdAns::LcdAns(const std::string &dev_name)
    : HWComponent(dev_name)
{
    supports_types(HWCompType::IOBUS_DEV);
}


std::string LcdAns::data_source_str()
{
    switch (this->data_source) {
        case DDRAM: return "DDRAM";
        case CGRAM: return "CGRAM";
    }
}


uint16_t LcdAns::iodev_read(uint32_t address)
{
    uint16_t value;
    switch (address) {
        case LcdAnsReg::LCD_R_COMMAND:
            value = (this->BF_busy_flag << 6) | this->AC_address_counter;
            LOG_F(VLCDANS, "%s: Read busy flag & address = %02x", this->name.c_str(), value);
            break;
        case LcdAnsReg::LCD_S_DATA:
            switch (this->data_source) {
                case DDRAM:
                    value = this->ddram[this->AC_address_counter];
                    LOG_F(VLCDANS, "%s: Read data from DDRAM[%d] = %02x", this->name.c_str(), this->AC_address_counter, value);
                    this->AC_address_counter = (this->AC_address_counter + 80 + ID_direction * 2 - 1) % 80;
                    break;
                case CGRAM:
                    value = this->cgram[this->AC_address_counter];
                    LOG_F(VLCDANS, "%s: Read data from CGRAM[%d] = %02x", this->name.c_str(), this->AC_address_counter, value);
                    this->AC_address_counter = (this->AC_address_counter + 64 + ID_direction * 2 - 1) % 64;
                    break;
            }
            break;
        case LcdAnsReg::SYNC_TIMEBASE:
            value = 0;
            LOG_F(ERROR, "%s: read  SYNC_TIMEBASE = %02x", this->name.c_str(), value);
            break;
        case LcdAnsReg::UNKNOWN:
            value = 0;
            LOG_F(ERROR, "%s: read  UNKNOWN = %02x", this->name.c_str(), value);
            break;
        default:
            value = 0;
            LOG_F(ERROR, "%s: read  0x%02x", this->name.c_str(), address);
    }
    return value;
}

void LcdAns::iodev_write(uint32_t address, uint16_t value)
{
    std::string command_str;

    switch (address) {
        case LcdAnsReg::LCD_R_COMMAND:
            if (0) {}
            else if (value >= 0x80) {
                this->data_source = DDRAM;
                this->AC_address_counter = value % 80;
                LOG_F(VLCDANS, "%s: Set DDRAM address = %02x", this->name.c_str(), value);
            } else if (value >= 0x40) {
                this->data_source = CGRAM;
                this->AC_address_counter = value % 64;
                LOG_F(VLCDANS, "%s: Set CGRAM address = %02x", this->name.c_str(), value);
            } else if (value >= 0x20) {
                this->DL_data_length = value & 0x10;
                this->N_lines = value & 8;
                this->F_font = value & 4;
                LOG_F(VLCDANS, "%s: Function set (DL=%d, N=%d, F=%d) = %02x", this->name.c_str(),
                    this->DL_data_length, this->N_lines, this->F_font, value);
            } else if (value >= 0x10) {
                if (value & 8) {
                    if (value & 4) {
                        LOG_F(VLCDANS, "%s: Display shift right = %02x", this->name.c_str(), value);
                        this->shift_position = (this->shift_position + this->width + 1) % this->width;
                    } else {
                        LOG_F(VLCDANS, "%s: Display shift left = %02x", this->name.c_str(), value);
                        this->shift_position = (this->shift_position + this->width - 1) % this->width;
                    }
                } else {
                    if (value & 4) {
                        LOG_F(VLCDANS, "%s: Cursor shift right = %02x", this->name.c_str(), value);
                    } else {
                        LOG_F(VLCDANS, "%s: Cursor shift left = %02x", this->name.c_str(), value);
                    }
                }
            } else if (value >= 0x08) {
                this->D_display = value & 4;
                this->C_cursor = value & 2;
                this->B_blinking = value & 1;
                LOG_F(VLCDANS, "%s: Display on/off control (D=%d, C=%d, B=%d) = %02x", this->name.c_str(),
                    this->D_display, this->C_cursor, this->B_blinking, value);
            } else if (value >= 0x04) {
                this->ID_direction = value & 2;
                this->S_shift = value & 1;
                LOG_F(VLCDANS, "%s: Entry mode set (ID=%d, S=%d) = %02x", this->name.c_str(),
                    this->ID_direction, this->S_shift, value);
            } else if (value >= 0x02) {
                this->data_source = DDRAM;
                this->AC_address_counter = 0;
                this->shift_position = 0;
                LOG_F(VLCDANS, "%s: Return home = %02x", this->name.c_str(), value);
            } else if (value >= 0x01) {
                this->data_source = DDRAM;
                this->AC_address_counter = 0;
                this->shift_position = 0;
                this->ID_direction = 1;
                std::memset(this->ddram, ' ', sizeof(this->ddram));
                LOG_F(VLCDANS, "%s: Clear display = %02x", this->name.c_str(), value);
            }
            break;
        case LcdAnsReg::LCD_S_DATA:
            switch (this->data_source) {
                case DDRAM:
                    printf("%c", value);
                    this->ddram[this->AC_address_counter] = value;
                    LOG_F(VWRITEDDRAM, "%s: write DDRAM[%d] = %02x", this->name.c_str(), this->AC_address_counter, value);
                    if (this->S_shift) {
                        this->shift_position = (this->shift_position + this->width + ID_direction * 2 - 1) % this->width;
                    }
                    this->AC_address_counter = (this->AC_address_counter + 80 + ID_direction * 2 - 1) % 80;
                    break;
                case CGRAM:
                    this->cgram[this->AC_address_counter] = value;
                    LOG_F(VLCDANS, "%s: write CGRAM[%d] = %02x", this->name.c_str(), this->AC_address_counter, value);
                    this->AC_address_counter = (this->AC_address_counter + 64 + ID_direction * 2 - 1) % 64;
                    break;
            }
            break;
        case LcdAnsReg::SYNC_TIMEBASE:
            LOG_F(VTIMEBASE, "%s: write SYNC_TIMEBASE = %04x", this->name.c_str(), value);
            if (value & 0x8000) {
                #ifdef _MSC_VER
                    #pragma message("enable timebase of CPUs")
                #else
                    #warning enable timebase of CPUs
                #endif
            } else {
                #ifdef _MSC_VER
                    #pragma message("disable timebase of CPUs")
                #else
                    #warning disable timebase of CPUs
                #endif
            }
            break;
        case LcdAnsReg::UNKNOWN:
            LOG_F(ERROR, "%s: write UNKNOWN = %04x", this->name.c_str(), value);
            break;
        default:
            LOG_F(ERROR, "%s: write 0x%02x = %04x", this->name.c_str(), address, value);
    }
}

PostInitResultType LcdAns::device_postinit()
{
    return PI_SUCCESS;
}

// ========================== Device registry stuff ==========================

static const DeviceDescription LcdAns_Descriptor = {
    LcdAns::create, {}, {}, HWCompType::IOBUS_DEV
};

REGISTER_DEVICE(LcdAns, LcdAns_Descriptor);
