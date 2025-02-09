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

/** @file DisplayID class definitions.

    DisplayID is a special purpose class for handling display
    identification (aka Monitor Plug-n-Play) as required by
    video cards.

    DisplayID provides two methods for display identification:
    - Apple monitor sense as described in the Technical Note HW30
    - Display Data Channel (DDC) standardized by VESA
 */

#ifndef DISPLAY_ID_H
#define DISPLAY_ID_H

#include <devices/common/hwcomponent.h>
#include <devices/video/videoctrl.h>

/* Supported diplay ID methods. */
enum Disp_Id_Kind : uint8_t {
    None       = 0,
    AppleSense = 1<<0,
    DDC1       = 1<<1,
    DDC2B      = 1<<2,
    EDDC       = 1<<3,
};

/** I2C bus states. */
enum I2CState : uint8_t {
    STOP     = 0, /* transaction ended (idle)      */
    START    = 1, /* transaction started           */
    DEV_ADDR = 2, /* receiving device address      */
    REG_ADDR = 3, /* receiving register address    */
    DATA     = 4, /* sending/receiving data        */
    ACK      = 5, /* sending/receiving acknowledge */
    NACK     = 6  /* no acknowledge (error)        */
};


class DisplayID : virtual public HWComponent {
public:
    DisplayID(const std::string name);
    ~DisplayID() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<DisplayID>(new DisplayID(dev_name));
    }

    // HWComponent methods

    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

    // DisplayID methods

    void set_video_ctrl(VideoCtrlBase* video_ctrl) {
        this->video_ctrl = video_ctrl;
    }
    uint8_t read_monitor_sense(uint8_t levels, uint8_t dirs);

protected:
    uint8_t set_result(uint8_t sda, uint8_t scl);
    uint8_t update_ddc_i2c(uint8_t sda, uint8_t scl);
    uint8_t apply_sense(uint8_t levels, uint8_t dirs, bool host);

private:
    VideoCtrlBase*  video_ctrl = nullptr;

    int id_kind = Disp_Id_Kind::None;

    uint8_t std_sense_code = 7;
    uint8_t ext_sense_code = 0x3F;

    /* DDC I2C variables. */
    uint8_t     next_state = I2CState::STOP;
    uint8_t     prev_state = I2CState::STOP;
    uint8_t     last_sda = 1;
    uint8_t     last_scl = 1;
    uint8_t     byte;           // byte value being currently transferred
    uint8_t     dev_addr;       // current device address
    uint8_t     reg_addr = 0;   // current register address
    uint8_t*    data_ptr = 0;   // ptr to data byte to be transferred next
    int         bit_count;      // number of bits processed so far
    int         data_pos = 0;   // current position in the data buffer

    std::unique_ptr<uint8_t[]> edid;
    uint16_t edid_length = 0;
};

#endif /* DISPLAY_ID_H */
