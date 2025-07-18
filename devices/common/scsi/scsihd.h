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

/** @file SCSI hard drive definitions. */

#ifndef SCSI_HD_H
#define SCSI_HD_H

#include <devices/common/scsi/scsi.h>
#include <utils/metaimgfile.h>

#include <cinttypes>
#include <memory>
#include <string>

class ScsiHardDisk : public ScsiDevice {
public:
    ScsiHardDisk(const std::string name);
    ~ScsiHardDisk() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<ScsiHardDisk>(new ScsiHardDisk("ScsiHardDisk"));
    }

    // HWComponent methods

    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;
    bool is_ready_for_machine() override;

    // ScsiHardDisk methods

    void insert_image(std::string filename);
    void process_command() override;
    bool prepare_data() override;
    bool get_more_data() override { return false; }

protected:
    int  test_unit_ready();
    int  req_sense(uint16_t alloc_len);
    int  send_diagnostic();
    void mode_select_6(uint8_t param_len);

    void mode_sense_6();
    void format();
    void reassign();
    uint32_t inquiry(uint8_t *cmd_ptr, uint8_t *data_ptr);
    void read_capacity_10();
    void read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len);
    void write(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len);
    void seek(uint32_t lba);
    void rewind();
    void read_buffer();
    void read_long_10(uint64_t lba, uint16_t transfer_len);

private:
    MetaImgFile     disk_img;
    uint64_t        img_size = 0;
    int             total_blocks;
    uint64_t        file_offset = 0;
    static const int sector_size = 512;
    bool            eject_allowed = true;
    int             bytes_out = 0;

    std::unique_ptr<uint8_t[]> data_buf_obj = nullptr;
    uint8_t*        data_buf = nullptr;
    uint32_t        data_buf_size = 0;

    uint8_t         error = ScsiError::NO_ERROR;
    uint8_t         msg_code = 0;

    //inquiry info
    char vendor_info[8] = {'Q', 'U', 'A', 'N', 'T', 'U', 'M', '\0'};
    char prod_info[16]  = {'E', 'm', 'u', 'l', 'a', 't', 'e', 'd', ' ', 'D', 'i', 's', 'k', '\0'};
    char rev_info[4]    = {'d', 'i', '0', '1'};
};

#endif // SCSI_HD_H
