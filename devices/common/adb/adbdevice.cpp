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

/** @file Base class for Apple Desktop Bus devices. */

#include <core/timermanager.h>
#include <devices/common/adb/adbdevice.h>
#include <devices/common/adb/adbbus.h>
#include <regex>

namespace loguru {
    enum : Verbosity {
        Verbosity_ADBDEVICE = loguru::Verbosity_9
    };
}

AdbDevice::AdbDevice(const std::string name) : HWComponent(name) {
    this->supports_types(HWCompType::ADB_DEV);
}

PostInitResultType AdbDevice::device_postinit() {
    // register itself with the ADB host
    this->host_obj = dynamic_cast<AdbBus*>(gMachineObj->get_comp_by_type(HWCompType::ADB_HOST));
    return PI_SUCCESS;
}

int32_t AdbDevice::parse_self_unit_address_string(const std::string unit_address_string) {
    return AdbDevice::parse_unit_address_string(unit_address_string);
}

int32_t AdbDevice::parse_unit_address_string(const std::string unit_address_string) {
    std::regex unit_address_re("(\\d+)(?:,[0-9A-F]+)?", std::regex_constants::icase);
    std::smatch results;
    if (std::regex_match(unit_address_string, results, unit_address_re)) {
        return (int32_t)std::stol(results[1], nullptr, 16);
    } else {
        return -1;
    }
}

std::string AdbDevice::get_self_unit_address_string(int32_t unit_address) {
    char buf[20];
    if (unit_address < 0)
        return "";
    if (unit_address >= 0)
        snprintf(buf, sizeof(buf), "@%d,%X", unit_address, this->get_address());
    return buf;
}

extern std::string hex_string(const uint8_t *p, int len);

uint8_t AdbDevice::poll() {
    if (!this->srq_flag) {
        return 0;
    }
    bool has_data = this->get_register_0();

    if (has_data) {
        LOG_F(ADBDEVICE, "%s: poll   %x.%d %d %s", this->get_name_and_unit_address().c_str(), this->my_addr, 0, has_data,
            hex_string(this->host_obj->get_output_buf(), this->host_obj->get_output_count()).c_str());
    }

    if (!has_data) {
        return 0;
    }

    // Register 0 in bits 0-1 (both 0)
    // Talk command in bits 2-3 (both 1)
    // Device address in bits 4-7
    return 0xC | (this->my_addr << 4);
}

bool AdbDevice::talk(const uint8_t dev_addr, const uint8_t reg_num) {
    bool result;

    if (dev_addr == this->my_addr && !this->got_collision) {
        // see if another device already responded to this command
        if (this->host_obj->already_answered()) {
            this->got_collision = true;
            result = false;
            LOG_F(ADBDEVICE, "%s: talk   %x.%d collision detected", this->get_name_and_unit_address().c_str(), dev_addr, reg_num);
        }
        else {
            switch(reg_num & 3) {
            case 0:
                result = this->get_register_0();
                break;
            case 1:
                result = this->get_register_1();
                break;
            case 2:
                result = this->get_register_2();
                break;
            case 3:
                result = this->get_register_3();
                break;
            default:
                result = false;
            }
            LOG_F(ADBDEVICE, "%s: talk   %x.%d %d %s", this->get_name_and_unit_address().c_str(), dev_addr, reg_num, result,
                hex_string(this->host_obj->get_output_buf(), this->host_obj->get_output_count()).c_str());
        }
    } else {
        result = false;
        LOG_F(ADBDEVICE, "%s: talk   %x.%d ignore collision", this->get_name_and_unit_address().c_str(), dev_addr, reg_num);
    }
    return result;
}

void AdbDevice::listen(const uint8_t dev_addr, const uint8_t reg_num) {
    if (dev_addr == this->my_addr) {
        LOG_F(ADBDEVICE, "%s: listen %x.%d   %s", this->get_name_and_unit_address().c_str(), dev_addr, reg_num,
            hex_string(this->host_obj->get_input_buf(), this->host_obj->get_input_count()).c_str());
        switch(reg_num & 3) {
        case 0:
            this->set_register_0();
            break;
        case 1:
            this->set_register_1();
            break;
        case 2:
            this->set_register_2();
            break;
        case 3:
            this->set_register_3();
            break;
        }
    }
}

bool AdbDevice::get_register_3() {
    uint8_t* out_buf = this->host_obj->get_output_buf();
    out_buf[0] = this->gen_random_address() | (this->exc_event_flag << 6) |
                 (this->srq_flag << 5);
    out_buf[1] = this->dev_handler_id;
    this->host_obj->set_output_count(2);
    return true;
}

uint8_t AdbDevice::gen_random_address() {
    return TimerManager::get_instance()->current_time_ns() & 0xF;
}
