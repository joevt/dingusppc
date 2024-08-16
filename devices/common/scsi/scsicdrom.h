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

/** @file SCSI CD-ROM definitions. */

#ifndef SCSI_CDROM_H
#define SCSI_CDROM_H

#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicdromcmds.h>

#include <cinttypes>
#include <string>

class ScsiCdrom : public ScsiPhysDevice, public ScsiCdromCmds {
public:
    ScsiCdrom(const std::string name);
    ~ScsiCdrom() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<ScsiCdrom>(new ScsiCdrom(dev_name));
    }

    // HWComponent methods

    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

    // ScsiCdrom methods

    virtual void process_command() override;

protected:
    bool is_device_ready() override { return this->is_ready; }
    uint8_t not_ready_reason() override {
        if (!this->medium_present())
            return ScsiError::MEDIUM_NOT_PRESENT;
    }

    int  get_apple_page_49(uint8_t subpage, uint8_t ctrl, uint8_t *out_ptr,
                          int avail_len);

    void mode_select_6(uint8_t param_len);

private:
    uint8_t data_buf[2048]  = {};
};

#endif // SCSI_CDROM_H
