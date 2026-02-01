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

/** @file DisplayID class implementation. */

#include <devices/video/displayid.h>
#include <devices/video/applesense.h>
#include <machines/machineproperties.h>
#include <loguru.hpp>

#include <cinttypes>
#include <map>
#include <string>
#include <cstring>
#include <regex>
#include <fstream>

namespace loguru {
    enum : Verbosity {
        Verbosity_DDC = loguru::Verbosity_INFO,
        Verbosity_LEVELS = loguru::Verbosity_INFO,
    };
}

DisplayID::DisplayID(const std::string name) : HWComponent(name)
{
    supports_types(HWCompType::DISPLAY);
}

HWComponent* DisplayID::set_property(const std::string &property, const std::string &value, int32_t unit_address)
{
    if (unit_address == -1 || unit_address == 0) {
        if (this->override_property(property, value)) {

            std::smatch results;

            if (property == "mon_id") {
                std::string mon_id = this->get_property_str("mon_id");

                bool is_apple_sense = false;
                bool is_other = false;

                do {
                    if (mon_id.empty()) {
                        this->set_name("Display_DDC");
                        is_other = true;
                        break;
                    }

                    if (MonitorAliasToId.count(mon_id)) {
                        mon_id = MonitorAliasToId.at(mon_id);
                    }
                    if (MonitorIdToCode.count(mon_id)) {
                        auto &monitor = MonitorIdToCode.at(mon_id);
                        this->std_sense_code = monitor.std_sense_code;
                        this->ext_sense_code = monitor.ext_sense_code;
                        this->set_name("Display_" + mon_id);
                        is_apple_sense = true;
                        break;
                    }

                    std::regex standard_sense_re("([0-7])", std::regex_constants::icase);
                    if (std::regex_match(mon_id, results, standard_sense_re)) {
                        uint8_t standard_ext_sense_code[] = {0x00, 0x14, 0x21, 0x35, 0x0A, 0x1E, 0x2B, 0x3F };
                        this->std_sense_code = std::stol(results[1], nullptr, 10);
                        this->ext_sense_code = standard_ext_sense_code[this->std_sense_code];
                        char buf[20];
                        snprintf(buf, sizeof(buf), "Display_%d.%02X", this->std_sense_code, this->ext_sense_code);
                        this->set_name(buf);
                        is_apple_sense = true;
                        break;
                    }

                    std::regex extended_sense_re("([0-7]).([0-3][0-9A-F])", std::regex_constants::icase);
                    if (std::regex_match(mon_id, results, extended_sense_re)) {
                        this->std_sense_code = std::stol(results[1], nullptr, 10);
                        this->ext_sense_code = std::stol(results[2], nullptr, 16);
                        char buf[20];
                        snprintf(buf, sizeof(buf), "Display_%d.%02X", this->std_sense_code, this->ext_sense_code);
                        this->set_name(buf);
                        is_apple_sense = true;
                        break;
                    }
                } while(0);

                if (is_apple_sense) {
                    this->id_kind |= Disp_Id_Kind::AppleSense;
                    LOG_F(INFO, "Added Apple Sense");
                    LOG_F(INFO, "Standard sense code: %d", this->std_sense_code);
                    LOG_F(INFO, "Extended sense code: 0x%02X", this->ext_sense_code);
                    video_ctrl->update_display_connection();
                    return this;
                } else if (is_other) {
                    return this;
                } else {
                    LOG_F(ERROR, "Ignored invalid Apple Sense: \"%s\"", mon_id.c_str());
                }
            } // if property == "mon_id"
            else
            if (property == "edid") {
                std::string edid_string = this->get_property_str("edid");
                bool is_edid = false;
                bool got_edid = false;

                std::regex edid_re("(?:[0-9A-F]{256}){0,255}", std::regex_constants::icase);
                if (std::regex_match(edid_string, results, edid_re)) {
                    is_edid = true;
                    this->edid_length = (uint16_t)(edid_string.length() / 2);
                    if (this->edid_length) {
                        this->edid = std::unique_ptr<uint8_t[]>(new uint8_t[this->edid_length]);
                        for (int i = 0; i < this->edid_length; i++) {
                            this->edid[i] = (((edid_string[i*2  ] & 15) + (edid_string[i*2  ] > '9' ? 9 : 0)) << 4) +
                                             ((edid_string[i*2+1] & 15) + (edid_string[i*2+1] > '9' ? 9 : 0));
                        }
                        got_edid = true;
                    }
                } else if (!edid_string.empty()) {
                    is_edid = true;
                    std::ifstream edid_file;
                    try {
                        edid_file.open(edid_string, std::ios::in | std::ios::binary);
                        if (edid_file.fail()) {
                            throw std::runtime_error("could not open specified EDID file");
                        }

                        // validate EDID file
                        uint8_t buf[8] = { 0 };

                        edid_file.seekg(0, std::ios::beg);
                        edid_file.read((char *)buf, sizeof(buf));

                        if (memcmp(buf, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
                            throw std::runtime_error("invalid EDID header");
                        }

                        // determine EDID size
                        edid_file.seekg(0, std::ios::end);
                        size_t edid_size = edid_file.tellg();
                        if (edid_size > 256*255) {
                            throw std::runtime_error("EDID file too large");
                        }
                        if (edid_size % 128) {
                            throw std::runtime_error("EDID file size is not a multiple of 128");
                        }

                        // EDID ok - go ahead and load it
                        this->edid_length = edid_size;
                        this->edid = std::unique_ptr<uint8_t[]>(new uint8_t[this->edid_length]);

                        edid_file.seekg(0, std::ios::beg);
                        edid_file.read((char *)this->edid.get(), this->edid_length);

                        LOG_F(INFO, "%s: loaded EDID (%d bytes).",
                            this->get_name().c_str(), this->edid_length);
                        edid_file.close();
                        got_edid = true;
                    }
                    catch (const std::exception& exc) {
                        LOG_F(ERROR, "%s: %s", this->get_name().c_str(), exc.what());
                    }
                    edid_file.close();
                }
                else {
                    LOG_F(ERROR, "Ignored invalid EDID");
                }

                if (is_edid) {
                    if (got_edid) {
                        this->id_kind |= Disp_Id_Kind::DDC2B;
                        this->id_kind |= Disp_Id_Kind::DDC1;
                        if (this->edid_length > 256)
                            this->id_kind |= Disp_Id_Kind::EDDC;
                        LOG_F(INFO, "Added EDID");
                    } else if (this->edid) {
                        this->edid.reset();
                        this->id_kind &= ~Disp_Id_Kind::DDC2B;
                        this->id_kind &= ~Disp_Id_Kind::DDC1;
                        this->id_kind &= ~Disp_Id_Kind::EDDC;
                        LOG_F(INFO, "Removed EDID");
                    }
                    video_ctrl->update_display_connection();
                    return this;
                }
            } // if property == "edid"
        } // if override_property
    } // if unit_address
    return nullptr;
}

uint8_t DisplayID::read_monitor_sense(uint8_t levels, uint8_t dirs)
{
    LOG_F(LEVELS, "dirs:%d levels:%d", dirs, levels);
    levels = apply_sense(levels, dirs, true);

    uint8_t target_levels = 0b0'111;
    if (this->id_kind & Disp_Id_Kind::DDC2B) {
        uint8_t scl, sda;
        /* If GPIO pins are in the output mode, pick up their levels.
           In the input mode, GPIO pins will be read "high" */
        scl = !!(levels & 0b0'010);
        sda = !!(levels & 0b0'100);

        target_levels = update_ddc_i2c(sda, scl);
        levels &= 0b0'001 | target_levels;
        LOG_F(LEVELS, "levels (i2c):%d", levels);

        levels = apply_sense(levels, dirs, false);
    }

    return levels;
}

uint8_t DisplayID::apply_sense(uint8_t levels, uint8_t dirs, bool host)
{
    /* If GPIO pins are in the output mode (dir=1), pick up their levels.
       In the input mode (dir=0), GPIO pins will be read "high" unless grounded by Apple monitor sense. */
    if (host) {
        uint8_t new_levels = ((dirs ^ 7) | (dirs & levels)) & this->std_sense_code;
        if (new_levels != levels) {
            levels = new_levels;
            LOG_F(LEVELS, "levels (host):%d", levels);
        }
    }
    else {
        uint8_t new_levels = levels & this->std_sense_code;
        if (new_levels != levels) {
            levels = new_levels;
            LOG_F(LEVELS, "levels (target):%d", levels);
        }
    }

    uint8_t other[3][2] = { {2,1}, {2,0}, {1,0} }; // other sense lines for sense line 0, 1, 2
    for (int i = 1; i >= 0; i--) {
        for (int sense = 2; sense >= 0; sense--) { // loop sense line 2 to 0
            if ( ((1 << sense) & dirs) && !((1 << sense) & levels) ) { // if the sense line is driven low
                uint8_t new_levels = levels;
                new_levels &= ~( ((~this->ext_sense_code >> (sense * 2 + 1)) & 1) << other[sense][0] );
                new_levels &= ~( ((~this->ext_sense_code >> (sense * 2 + 0)) & 1) << other[sense][1] );
                if (new_levels != levels) {
                    levels = new_levels;
                    LOG_F(LEVELS, "levels (ext):%d", levels);
                }
            }
        }
    }
    return levels;
}

uint8_t DisplayID::set_result(uint8_t sda, uint8_t scl)
{
    uint16_t data_out;

    this->last_sda = sda;
    this->last_scl = scl;

    data_out = 0;

    if (scl) {
        data_out |= 0b0'010;
    }

    if (sda) {
        data_out |= 0b0'100;
    }

    return data_out;
}

uint8_t DisplayID::update_ddc_i2c(uint8_t sda, uint8_t scl)
{
    bool clk_gone_high = false;

    LOG_F(DDC, "update_ddc_i2c: SDA:%d->%d SCA:%d->%d", sda, this->last_sda, scl, this->last_scl);

    if (scl != this->last_scl) {
        this->last_scl = scl;
        if (scl) {
            clk_gone_high = true;
        }
    }

    if (sda != this->last_sda) {
        /* START = SDA goes high to low while SCL is high */
        /* STOP  = SDA goes low to high while SCL is high */
        if (this->last_scl) {
            if (!sda) {
                LOG_F(DDC, "DDC-I2C: START condition detected! next_state:DEV_ADDR");
                this->next_state = I2CState::DEV_ADDR;
                this->bit_count  = 0;
            } else {
                LOG_F(DDC, "DDC-I2C: STOP condition detected!");
                this->next_state = I2CState::STOP;
            }
        }
        return set_result(sda, scl);
    }

    if (!clk_gone_high) {
        return set_result(sda, scl);
    }

    switch (this->next_state) {
    case I2CState::STOP:
        LOG_F(DDC, "DDC-I2C: STOP");
        break;

    case I2CState::ACK:
        this->bit_count = 0;
        this->byte      = 0;
        switch (this->prev_state) {
        case I2CState::DEV_ADDR:
            LOG_F(DDC, "DDC-I2C: ACK DEV_ADDR");
            if ((dev_addr & 0xFE) == 0xA0) {
                LOG_F(DDC, "DDC-I2C: got address 0x%X", this->dev_addr);
                sda = 0; /* send ACK */
            } else {
                LOG_F(ERROR, "DDC-I2C: unknown device address 0x%X", this->dev_addr);
                sda = 1; /* send NACK */
            }
            if (this->dev_addr & 1) {
                this->next_state = I2CState::DATA;
                this->data_ptr   = &this->edid[0];
                this->data_pos   = 0;
                this->byte       = this->data_ptr[this->data_pos++];
            } else {
                this->next_state = I2CState::REG_ADDR;
            }
            break;
        case I2CState::REG_ADDR:
            LOG_F(DDC, "DDC-I2C: ACK REG_ADDR");
            this->next_state = I2CState::DATA;
            if (!this->reg_addr) {
                sda = 0; /* send ACK */
            } else {
                LOG_F(ERROR, "DDC-I2C: unknown register address 0x%X", this->reg_addr);
                sda = 1; /* send NACK */
            }
            break;
        case I2CState::DATA:
            LOG_F(DDC, "DDC-I2C: ACK DATA");
            this->next_state = I2CState::DATA;
            if (dev_addr & 1) {
                if (!sda) {
                    /* load next data byte */
                    if (data_pos < this->edid_length)
                        this->byte = this->data_ptr[this->data_pos++];
                    else
                        this->byte = 0;
                    LOG_F(DDC, "DDC-I2C: byte[%02X]:%02X", this->data_pos - 1, this->byte);
                } else {
                    LOG_F(ERROR, "DDC-I2C: Oops! NACK received");
                }
            } else {
                sda = 0; /* send ACK */
            }
            break;
        default:
            LOG_F(DDC, "DDC-I2C: ACK STATE:%d", this->prev_state);
        }
        break;

    case I2CState::DEV_ADDR:
    case I2CState::REG_ADDR:
        this->byte = (this->byte << 1) | this->last_sda;
        LOG_F(DDC, "DDC-I2C: %s bit:%d byte:%02X",
            this->next_state == I2CState::DEV_ADDR ? "DEV_ADDR" : "REGADDR", this->bit_count, this->byte);
        if (this->bit_count++ >= 7) {
            this->bit_count  = 0;
            this->prev_state = this->next_state;
            this->next_state = I2CState::ACK;
            if (this->prev_state == I2CState::DEV_ADDR) {
                LOG_F(DDC, "DDC-I2C: device address received, addr=0x%X", this->byte);
                this->dev_addr = this->byte;
            } else {
                LOG_F(DDC, "DDC-I2C: register address received, addr=0x%X", this->byte);
                this->reg_addr = this->byte;
            }
        }
        break;

    case I2CState::DATA:
        sda = (this->byte >> (7 - this->bit_count)) & 1;
        LOG_F(DDC, "DDC-I2C: DATA bit:%d byte:%02X", this->bit_count, this->byte);
        if (this->bit_count++ >= 7) {
            this->bit_count  = 0;
            this->prev_state = this->next_state;
            this->next_state = I2CState::ACK;
        }
        break;
    default:
        LOG_F(DDC, "DDC-I2C: STATE:%d", this->next_state);
    }

    return set_result(sda, scl);
}

static const PropMap Display_Properties = {
    {"mon_id", new StrProperty("")},
    {"edid"  , new StrProperty("")},
};

static const DeviceDescription Display_Descriptor = {
    DisplayID::create, {}, Display_Properties, HWCompType::DISPLAY
};

REGISTER_DEVICE(Display, Display_Descriptor);
