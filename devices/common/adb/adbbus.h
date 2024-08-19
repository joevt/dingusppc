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

/** @file Apple Desktop Bus definitions. */

#ifndef ADB_BUS_H
#define ADB_BUS_H

#include <devices/common/hwcomponent.h>

#include <memory>
#include <string>
#include <vector>

constexpr auto ADB_MAX_DATA_SIZE = 8;

/** ADB status. */
enum {
    ADB_STAT_OK         = 0,
    ADB_STAT_SRQ_ACTIVE = 1 << 0,
    ADB_STAT_TIMEOUT    = 1 << 1,
    ADB_STAT_AUTOPOLL   = 1 << 6,
};

class AdbDevice; // forward declaration to prevent compiler errors

class AdbBus : virtual public HWComponent {
friend class AdbDevice;
public:
    AdbBus(const std::string name);
    ~AdbBus() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<AdbBus>(new AdbBus(dev_name));
    }

    // HWComponent methods

    int device_postinit() override;
    virtual HWComponent* add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name = "") override;
    virtual bool remove_device(int32_t unit_address) override;
    virtual std::string get_child_unit_address_string(int32_t unit_address) override;
    int32_t parse_child_unit_address_string(const std::string unit_address_string) override;

    // AdbBus methods

    uint8_t process_command(const uint8_t* in_data, int data_size);
    uint8_t get_output_count() { return this->output_count; }

    // Polls devices that have a service request flag set. Returns the talk
    // command corresponding to the first device that responded with data, or
    // 0 if no device responded.
    uint8_t poll();

    // callbacks meant to be called by devices
    const uint8_t*  get_input_buf() { return this->input_buf; }
    uint8_t*        get_output_buf() { return this->output_buf; }
    uint8_t         get_input_count() { return this->input_count; }
    void            set_output_count(uint8_t count) { this->output_count = count; }
    bool            already_answered() { return this->got_answer; }

private:
    void register_device(AdbDevice* dev_obj);
    void unregister_device(AdbDevice* dev_obj);
    std::vector<AdbDevice*> devices;

    bool            got_answer = false;
    const uint8_t*  input_buf = nullptr;
    uint8_t         output_buf[ADB_MAX_DATA_SIZE] = {};
    uint8_t         input_count = 0;
    uint8_t         output_count = 0;
};

#endif // ADB_BUS_H
