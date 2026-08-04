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

/** Burgundy sound codec emulation. */

#include <core/endianswap.h>
#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/deviceregistry.h>
#include <devices/sound/burgundy.h>
#include <loguru.hpp>

namespace loguru {
    enum : Verbosity {
        Verbosity_BURGUNDY = loguru::Verbosity_9
    };
}

std::string get_codec_reg_name(uint8_t reg, uint8_t pos) {
    for (int pass = 0; pass < 2; pass++) {
        int what = (reg << 8) | (pass ? 0 : (pos << 4));
        switch (what) {

            #define onereg(r, p, len, name) \
                case 0x ## r ## p ## 0: return # name;

            onereg(00, 0, 1, Init)
            onereg(01, 0, 1, Revision)
            onereg(01, 1, 1, Version)
            onereg(01, 2, 1, Vendor)
            onereg(01, 3, 1, Id)
            onereg(02, 0, 2, GlobalStatus)
            onereg(0F, 0, 1, ReturnZero)
            onereg(10, 0, 1, InputPreamp)
            onereg(11, 0, 1, Mux01)
            onereg(12, 0, 1, Mux2)
            onereg(13, 0, 1, VGA0)
            onereg(14, 0, 1, VGA1)
            onereg(15, 0, 1, VGA2)
            onereg(16, 0, 1, VGA3)
            onereg(17, 0, 1, DigInputPort)
            onereg(18, 0, 1, InputState)
            onereg(19, 0, 2, AD12Status)
            onereg(20, 0, 1, GAS0L)
            onereg(20, 1, 1, PAS0L)
            onereg(20, 2, 1, GAS0R)
            onereg(20, 3, 1, PAS0R)
            onereg(21, 0, 1, GAS1L)
            onereg(21, 1, 1, PAS1L)
            onereg(21, 2, 1, GAS1R)
            onereg(21, 3, 1, PAS1R)
            onereg(22, 0, 1, GAS2L)
            onereg(22, 1, 1, PAS2L)
            onereg(22, 2, 1, GAS2R)
            onereg(22, 3, 1, PAS2R)
            onereg(23, 0, 1, GAS3L)
            onereg(23, 1, 1, GAS3R)
            onereg(23, 2, 1, GAS4L)
            onereg(23, 3, 1, GAS4R)
            onereg(25, 0, 1, GASAL)
            onereg(25, 1, 1, GASAR)
            onereg(25, 2, 1, GASBL)
            onereg(25, 3, 1, GASBR)
            onereg(26, 0, 1, GASCL)
            onereg(26, 1, 1, GASCR)
            onereg(26, 2, 1, GASDL)
            onereg(26, 3, 1, GASDR)
            onereg(27, 0, 1, GASEL)
            onereg(27, 1, 1, GASER)
            onereg(27, 2, 1, GASFL)
            onereg(27, 3, 1, GASFR)
            onereg(28, 0, 1, GASGL)
            onereg(28, 1, 1, GASGR)
            onereg(28, 2, 1, GASHL)
            onereg(28, 3, 1, GASHR)
            onereg(29, 0, 4, MX0)
            onereg(2A, 0, 4, MX1)
            onereg(2B, 0, 4, MX2)
            onereg(2C, 0, 4, MX3)
            onereg(2D, 0, 1, MXEQ0L)
            onereg(2D, 1, 1, MXEQ0R)
            onereg(2D, 2, 1, MXEQ1L)
            onereg(2D, 3, 1, MXEQ1R)
            onereg(2E, 0, 1, MXEQ2L)
            onereg(2E, 1, 1, MXEQ2R)
            onereg(2E, 2, 1, MXEQ3L)
            onereg(2E, 3, 1, MXEQ3R)
            onereg(2F, 0, 4, OS)
            onereg(30, 0, 1, GAP0L)
            onereg(30, 1, 1, GAP0R)
            onereg(30, 2, 1, GAP1L)
            onereg(30, 3, 1, GAP1R)
            onereg(31, 0, 1, GAP2L)
            onereg(31, 1, 1, GAP2R)
            onereg(31, 2, 1, GAP3L)
            onereg(31, 3, 1, GAP3R)
            onereg(33, 0, 2, PeakLvl0)
            onereg(33, 2, 2, PeakLvl1)
            onereg(34, 0, 2, PeakLvl3)
            onereg(34, 2, 2, PeakLvl4)
            onereg(35, 0, 1, PeakLvl0Source)
            onereg(35, 1, 1, PeakLvl1Source)
            onereg(35, 2, 1, PeakLvl2Source)
            onereg(35, 3, 1, PeakLvl3Source)
            onereg(36, 0, 2, PeakLvlThreshold)
            onereg(37, 0, 1, ISOverflow)
            onereg(37, 1, 2, MXOverflow)
            onereg(40, 0, 3, SOSS0B0)
            onereg(41, 0, 3, SOSS0B1)
            onereg(42, 0, 3, SOSS0B2)
            onereg(43, 0, 3, SOSS0A1)
            onereg(44, 0, 3, SOSS0A2)
            onereg(45, 0, 3, SOSS1B0)
            onereg(46, 0, 3, SOSS1B1)
            onereg(47, 0, 3, SOSS1B2)
            onereg(48, 0, 3, SOSS1A1)
            onereg(49, 0, 3, SOSS1A2)
            onereg(4A, 0, 3, SOSS2B0)
            onereg(4B, 0, 3, SOSS2B1)
            onereg(4C, 0, 3, SOSS2B2)
            onereg(4D, 0, 3, SOSS2A1)
            onereg(4E, 0, 3, SOSS2A2)
            onereg(50, 0, 3, SOSS3B0)
            onereg(51, 0, 3, SOSS3B1)
            onereg(52, 0, 3, SOSS3A1)
            onereg(53, 0, 3, SOSS3A2)
            onereg(55, 0, 1, SOSControl)
            onereg(56, 0, 1, SOSOverflow)
            onereg(60, 0, 1, OutputMute)
            onereg(61, 0, 1, OutputLvlPort13)
            onereg(62, 0, 1, OutputLvlPort14)
            onereg(63, 0, 1, OutputLvlPort15)
            onereg(64, 0, 1, OutputLvlPort16)
            onereg(65, 0, 1, OutputLvlPort17)
            onereg(66, 0, 2, OutputSettleTime)
            onereg(67, 0, 1, OutputCtl0)
            onereg(68, 0, 1, OutputCtl2)
            onereg(69, 0, 1, DOutConfig)
            onereg(70, 0, 1, MClk)
            onereg(78, 0, 1, SDIn)
            onereg(79, 0, 1, SDIn2)
//          onereg(7A, 0, 1, SDOut)
            onereg(7A, 0, 1, ThresholdMask)

            #undef onereg
        } // switch what
    } // for pass
    return "Uknown";
}

BurgundyCodec::BurgundyCodec(const std::string name)
     : MacioSndCodec(name), HWComponent(name)
{
    supports_types(HWCompType::SND_CODEC);

    static int burgundy_sample_rates[1] = { 44100 };

    // Burgundy seems to supports only one sample rate
    this->sr_table  = burgundy_sample_rates;
    this->max_sr_id = 1;

    this->set_sample_rate(0); // set default sample rate

    this->reg_array[0x01] = 0x01010000; // ID 1 (Burgundy), Vendor 1 (Crystal), Version 0, Revision 0
}

uint32_t BurgundyCodec::snd_ctrl_read(uint32_t offset, int size) {
    uint32_t value = 0;

    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        value = this->snd_ctrl_reg;
        LOG_F(BURGUNDY, "%s: read  %-12s @%02x.%c = %0*x", this->name.c_str(), "SOUND_CTRL",
            offset, SIZE_ARG(size), size * 2, value);
        break;
    case AWAC_CODEC_CTRL_REG:
        value = this->last_ctrl_data;
        LOG_F(BURGUNDY, "%s: read  %-12s @%02x.%c = %0*x", this->name.c_str(), "CODEC_CTRL",
            offset, SIZE_ARG(size), size * 2, value);
        break;
    case AWAC_CODEC_STATUS_REG:
        value =
            (
                (
                    9 |
                    CODEC_STATUS::SENSE_MIC |
                    CODEC_STATUS::SENSE_HEADPHONES
                ) << CODEC_STATUS::SENSE_POS
            ) |
            (this->data_byte << CODEC_STATUS::DATA_POS) |
            (this->read_pos << CODEC_STATUS::CURRENTBYTE_POS) |
            (this->byte_counter << CODEC_STATUS::BYTECOUNTER_POS) |
            (0 << CODEC_STATUS::INDICATOR_POS) |
            CODEC_STATUS::READY |
            (this->first_valid ? CODEC_STATUS::FIRST_VALID_BYTE : 0);
        LOG_F(BURGUNDY, "%s: read  %-12s @%02x.%c = %0*x", this->name.c_str(), "CODEC_STATUS",
            offset, SIZE_ARG(size), size * 2, value);
        break;
    case AWAC_FRAME_COUNT:
        value = (uint32_t)(
            (
                (TimerManager::get_instance()->current_time_ns() - frame_count_start_time) * this->sr_table[0]
                + 500000000
            )  / 1000000000 + this->frame_count
        );
        LOG_F(BURGUNDY, "%s: read  %-12s @%02x.%c = %0*x", this->name.c_str(), "FRAME_COUNT",
            offset, SIZE_ARG(size), size * 2, value);
        break;
    default:
        LOG_F(ERROR, "%s: read  @%02x.%c", this->name.c_str(),
            offset, SIZE_ARG(size));
    }

    return BYTESWAP_32(value);
}

void BurgundyCodec::snd_ctrl_write(uint32_t offset, uint32_t value, int size) {
    value = BYTESWAP_32(value);

    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        LOG_F(BURGUNDY, "%s: write %-12s @%02x.%c = %0*x", this->name.c_str(), "SOUND_CTRL",
            offset, SIZE_ARG(size), size * 2, value);
        this->snd_ctrl_reg = value;
        //this->set_sample_rate((this->snd_ctrl_reg & SOUND_CONTROL::RATE_MASK) >> SOUND_CONTROL::RATE_POS);
        break;
    case AWAC_CODEC_CTRL_REG:
    {
        this->last_ctrl_data = value & ~CODEC_CONTROL::BUSY;
        uint8_t write_byte = (value & CODEC_CONTROL::DATA_MASK) >> CODEC_CONTROL::DATA_POS;
        uint8_t reg_addr = (value & CODEC_CONTROL::ADDR_MASK) >> CODEC_CONTROL::ADDR_POS;
        uint8_t cur_byte = (value & CODEC_CONTROL::CURRENTBYTE_MASK) >> CODEC_CONTROL::CURRENTBYTE_POS;
        uint8_t last_byte = (value & CODEC_CONTROL::LASTBYTE_MASK) >> CODEC_CONTROL::LASTBYTE_POS;
        bool reset = value & CODEC_CONTROL::RESET;
        bool write = value & CODEC_CONTROL::WRITE;
        if (write) {
            if (reg_addr < BURGUNDY_NUM_REGS) {
                uint32_t mask = 0xFFU << (cur_byte * 8);
                this->reg_array[reg_addr] = (this->reg_array[reg_addr] & ~mask) |
                                            (write_byte << (cur_byte * 8));
            }
            LOG_F(BURGUNDY, "%s: write %-12s @%02x.%c = %0*x %-6sWRITE %-16s @%02x.%d.%d = %02x",
                this->name.c_str(), "CODEC_CTRL",
                offset, SIZE_ARG(size), size * 2, value,
                reset ? "RESET," : "",
                get_codec_reg_name(reg_addr, cur_byte).c_str(),
                reg_addr,
                cur_byte,
                last_byte,
                write_byte
            );
        } else {
            this->reg_addr = reg_addr;
            this->read_pos = cur_byte;
            this->data_byte = ((reg_addr < BURGUNDY_NUM_REGS ? this->reg_array[reg_addr] : 0) >> (cur_byte * 8)) & 0xFFU;
            this->first_valid = true;
            LOG_F(BURGUNDY, "%s: write %-12s @%02x.%c = %0*x %-6sREAD  %-16s @%02x.%d.%d = %02x",
                this->name.c_str(), "CODEC_CTRL",
                offset, SIZE_ARG(size), size * 2, value,
                reset ? "RESET," : "",
                get_codec_reg_name(reg_addr, cur_byte).c_str(),
                reg_addr,
                cur_byte,
                last_byte,
                this->data_byte
            );

            TimerManager::get_instance()->add_oneshot_timer(
                USECS_TO_NSECS(22), // average is approximately 22.6 µs on a real B&W G3
                [this](uint64_t, uint64_t) {
                    this->first_valid  = false;
                    this->byte_counter = (this->byte_counter + 1) & 3;
            });
        }
        break;
    }
    case AWAC_FRAME_COUNT:
        this->frame_count = value;
        this->frame_count_start_time = TimerManager::get_instance()->current_time_ns();
        LOG_F(BURGUNDY, "%s: write %-12s @%02x.%c = %0*x", this->name.c_str(), "FRAME_COUNT",
            offset, SIZE_ARG(size), size * 2, value);
        break;
    default:
        LOG_F(ERROR, "%s: write @%02x.%c = %0*x", this->name.c_str(),
            offset, SIZE_ARG(size), size * 2, value);
    }
}

static const DeviceDescription Burgundy_Descriptor = {
    BurgundyCodec::create, {}, {}, HWCompType::SND_CODEC
};

REGISTER_DEVICE(BurgundySnd, Burgundy_Descriptor);
