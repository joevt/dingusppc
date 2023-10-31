/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

#include <devices/common/hwcomponent.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <map>
#include <set>
#include <string>

std::unique_ptr<MachineBase> gMachineObj = 0;

MachineBase::MachineBase(std::string name) {
    this->name = name;

    /* initialize internal maps */
    this->clear_devices();
}

MachineBase::~MachineBase() {
    this->clear_devices();
}

void MachineBase::clear_devices() {
    this->device_map.clear();
}

void MachineBase::add_device(std::string name, std::unique_ptr<HWComponent> dev_obj) {
    if (this->device_map.count(name)) {
        LOG_F(ERROR, "Device %s already exists!", name.c_str());
        return;
    }

    this->device_map[name] = std::move(dev_obj);
}

void MachineBase::remove_device(HWComponent* dev_obj) {
    for (auto it = this->device_map.begin(); it != this->device_map.end(); ++it)
        if (it->second.get() == dev_obj) {
            this->device_map.erase(it->first);
            break;
        }
}

HWComponent* MachineBase::get_comp_by_name(std::string name) {
    if (this->device_map.count(name)) {
        return this->device_map[name].get();
    } else {
        LOG_F(WARNING, "Component name %s not found!", name.c_str());
        return nullptr;
    }
}

HWComponent* MachineBase::get_comp_by_name_optional(std::string name) {
    if (this->device_map.count(name)) {
        return this->device_map[name].get();
    } else {
        return nullptr;
    }
}

HWComponent* MachineBase::get_comp_by_type(HWCompType type) {
    std::string comp_name;
    bool found = false;

    for (auto it = this->device_map.begin(); it != this->device_map.end(); it++) {
        if (it->second->supports_type(type)) {
            comp_name = it->first;
            found     = true;
            break;
        }
    }

    if (found) {
        return this->get_comp_by_name(comp_name);
    } else {
        LOG_F(WARNING, "No component of type %llu was found!", type);
        return nullptr;
    }
}

int MachineBase::postinit_devices()
{
    // Allow additional devices to be registered by device_postinit, in which
    // case we'll initialize them too.
    std::set<std::string> initialized_devices;
    while (initialized_devices.size() < this->device_map.size()) {
        for (auto it = this->device_map.begin(); it != this->device_map.end(); it++) {
            if (initialized_devices.find(it->first) == initialized_devices.end()) {
                LOG_F(INFO, "%*s[ Post init %s", indent(), "", it->first.c_str());
                int postinit_result = it->second->device_postinit();
                if (postinit_result < 0) {
                    LOG_F(ERROR, "Could not initialize device %s", it->first.c_str());
                    return -1;
                }
                if (postinit_result > 0) {
                    LOG_F(INFO, "%*s] Will retry post init %s later", outdent(), "", it->first.c_str());
                } else {
                    initialized_devices.insert(it->first);
                }
                LOG_F(INFO, "%*s]", outdent(), "");
            }
        }
    }

    return 0;
}

int MachineBase::indent() {
    indent_val += 2;
    if (indent_val > 100)
        indent_val = 100;
    return indent_val - 2;
}

int MachineBase::outdent() {
    indent_val -= 2;
    if (indent_val < 0)
        indent_val = 0;
    return indent_val;
}
