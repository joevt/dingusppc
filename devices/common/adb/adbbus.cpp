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

/** @file Apple Desktop Bus emulation. */

#include <devices/common/adb/adbbus.h>
#include <devices/common/adb/adbdevice.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <sstream>

AdbBus::AdbBus(const std::string name) : HWComponent(name) {
    supports_types(HWCompType::ADB_HOST);
    this->devices.clear();
}

PostInitResultType AdbBus::device_postinit() {
    std::string adb_device_list = GET_STR_PROP("adb_devices");
    if (adb_device_list.empty())
        return PI_SUCCESS;

    std::string adb_device;
    std::istringstream adb_device_stream(adb_device_list);

    while (getline(adb_device_stream, adb_device, ',')) {
        std::string dev_name = adb_device;

        if (dev_name == this->name)
            continue; // don't register a second ADB bus

        if (DeviceRegistry::device_registered(dev_name)) {
            this->add_device(0, DeviceRegistry::get_descriptor(dev_name).m_create_func(dev_name).release(), dev_name);
        } else {
            LOG_F(WARNING, "Unknown specified ADB device \"%s\"", adb_device.c_str());
        }
    }

    return PI_SUCCESS;
}

HWComponent* AdbBus::add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name) {
    unit_address = int32_t(this->devices.size());
    this->register_device(dynamic_cast<AdbDevice*>(dev_obj));
    return HWComponent::add_device(unit_address, dev_obj, name);
}

bool AdbBus::remove_device(int32_t unit_address) {
    HWComponent* hwc = this->children[unit_address].get();
    AdbDevice* dev_obj = dynamic_cast<AdbDevice*>(hwc);
    this->unregister_device(dev_obj);
    bool result = HWComponent::remove_device(unit_address);
    int i = 0;
    for (auto dev : this->devices)
        dev->change_unit_address(i++);
    return result;
}

std::string AdbBus::get_child_unit_address_string(int32_t unit_address) {
    char buf[20];
    if (unit_address < 0)
        return "";
    if (unit_address >= 0 && unit_address < devices.size())
        snprintf(buf, sizeof(buf), "@%d,%X", unit_address, this->devices[unit_address]->get_address());
    else
        snprintf(buf, sizeof(buf), "@%d", unit_address);
    return buf;
}

int32_t AdbBus::parse_child_unit_address_string(const std::string unit_address_string) {
    return AdbDevice::parse_unit_address_string(unit_address_string);
}

void AdbBus::register_device(AdbDevice* dev_obj) {
    this->devices.push_back(dev_obj);
}

void AdbBus::unregister_device(AdbDevice* dev_obj) {
    auto it = std::find(this->devices.begin(), this->devices.end(), dev_obj);
    if (it != this->devices.end()) {
        this->devices.erase(it);
    } else {
        LOG_F(ERROR, "%s: Could not unregister %s", this->name.c_str(), dev_obj->get_name_and_unit_address().c_str());
    }
}

uint8_t AdbBus::poll() {
    for (auto dev : this->devices) {
        uint8_t dev_poll = dev->poll();
        if (dev_poll) {
            return dev_poll;
        }
    }
    return 0;
}

uint8_t AdbBus::process_command(const uint8_t* in_data, int data_size) {
    uint8_t dev_addr, dev_reg;

    this->output_count  = 0;

    if (!data_size)
        return ADB_STAT_OK;

    uint8_t cmd_byte = in_data[0];
    uint8_t cmd = cmd_byte & 0xF;

    if(!cmd) { // SendReset
        LOG_F(9, "%s: SendReset issued", this->name.c_str());
        for (auto dev : this->devices)
            dev->reset();
    } else if (cmd == 1) { // Flush
        dev_addr = cmd_byte >> 4;

        LOG_F(9, "%s: Flush issued, dev_addr=0x%X", this->name.c_str(), dev_addr);
    } else if ((cmd & 0xC) == 8) { // Listen
        dev_addr = cmd_byte >> 4;
        dev_reg  = cmd_byte & 3;

        LOG_F(9, "%s: Listen R%d issued, dev_addr=0x%X", this->name.c_str(),
              dev_reg, dev_addr);

        this->input_buf   = in_data + 1;
        this->input_count = data_size - 1;

        for (auto dev : this->devices)
            dev->listen(dev_addr, dev_reg);
    } else if ((cmd & 0xC) == 0xC) { // Talk
        dev_addr = cmd_byte >> 4;
        dev_reg  = cmd_byte & 3;

        LOG_F(9, "%s: Talk R%d issued, dev_addr=0x%X", this->name.c_str(),
              dev_reg, dev_addr);

        this->got_answer = false;

        for (auto dev : this->devices) {
            this->got_answer = dev->talk(dev_addr, dev_reg);
            if (this->got_answer) {
                break;
            }
        }

        if (!this->got_answer)
            return ADB_STAT_TIMEOUT;
    } else {
        LOG_F(ERROR, "%s: unsupported ADB command 0x%X", this->name.c_str(), cmd_byte);
    }

    return ADB_STAT_OK;
}

static const PropMap AdbBus_Properties = {
    {"adb_devices", new StrProperty("AdbMouse,AdbKeyboard")},
};

static const DeviceDescription AdbBus_Descriptor = {
    AdbBus::create, {}, AdbBus_Properties, HWCompType::ADB_HOST
};

REGISTER_DEVICE(AdbBus, AdbBus_Descriptor);
