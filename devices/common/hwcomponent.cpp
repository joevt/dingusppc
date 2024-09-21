/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

#include <map>
#include <set>
#include <string>
#include <vector>

std::unique_ptr<HWComponent> gMachineObj = 0;

HWComponent::HWComponent(const std::string name) {
    LOG_F(INFO, "Created %s", name.c_str());
    this->set_name(name);
    this->clear_devices();
}

HWComponent::~HWComponent() {
    this->clear_devices();
    LOG_F(INFO, "Deleted %s", name.c_str());
}

void HWComponent::clear_devices() {
    this->children.clear();
}

void HWComponent::change_unit_address(int32_t unit_address) {
    if (!this->parent || this->unit_address == unit_address)
        return;
    if (parent->children.count(unit_address)) {
        LOG_F(ERROR, "Cannot change address of %s because a device already exists at %s.",
            this->get_path().c_str(), this->children[unit_address]->get_path().c_str()
        );
        return;
    }
    parent->children[unit_address] = std::move(parent->children[this->unit_address]);
    parent->children.erase(this->unit_address);
    this->set_unit_address(unit_address);
}

HWComponent* HWComponent::add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name) {
    if (!name.empty()) {
        if (dev_obj->get_name() != name) {
            if (dev_obj->get_name().empty()) {
                LOG_F(INFO, "Set name to \"%s\"", name.c_str());
            } else {
                LOG_F(INFO, "Changed name from \"%s\" to \"%s\"", dev_obj->get_name().c_str(), name.c_str());
            }
            dev_obj->set_name(name);
        }
    }
    if (this->children.count(unit_address)) {
        if (this->children[unit_address].get() == dev_obj)
            return;
        LOG_F(ERROR, "Cannot add %s because a device already exists at %s.",
            dev_obj->get_path().c_str(), this->children[unit_address]->get_path().c_str()
        );
        return nullptr;
    }
    if (dev_obj->parent) {
        LOG_F(INFO, "Moved %s from %s to %s", dev_obj->get_name_and_unit_address().c_str(),
            dev_obj->parent->get_name_and_unit_address().c_str(), this->get_name_and_unit_address().c_str());
        this->children[unit_address] = std::move(dev_obj->parent->children[dev_obj->unit_address]);
        dev_obj->parent->children.erase(dev_obj->unit_address);
    } else {
        this->children[unit_address] = std::unique_ptr<HWComponent>(dev_obj);
    }
    dev_obj->set_unit_address(unit_address);
    dev_obj->set_parent(this);
    return dev_obj;
}

void HWComponent::move_device(HWComponent* new_parent) {
    new_parent->add_device(this->unit_address, this);
}

void HWComponent::move_children(HWComponent* dst) {
    while (this->children.size()) {
        auto it = this->children.begin();
        it->second->set_parent(nullptr);
        dst->add_device(it->first, it->second.release());
        this->children.erase(it->first);
    }
}

bool HWComponent::remove_device(int32_t unit_address) {
    if (this->children.count(unit_address)) {
        this->children.erase(unit_address);
        return true;
    }
    LOG_F(ERROR, "Cannot remove %s/%s because it does not exist!",
        this->get_path().c_str(), this->get_child_unit_address_string(unit_address).c_str());
    return false;
}

bool HWComponent::remove_device(HWComponent* dev_obj, int depth) {
    for (auto it = this->children.begin(); it != this->children.end(); it++) {
        HWComponent* hwc = it->second.get();
        if (hwc == dev_obj) {
            return this->remove_device(it->first);
        }
        if (hwc->remove_device(dev_obj, depth + 1))
            return true;
    }
    if (depth == 0)
        LOG_F(ERROR, "Cannot remove %s because it is not found!", dev_obj->get_path().c_str());
    return false;
}

HWComponent* HWComponent::get_comp_by_name(const std::string name, bool optional, int depth) {
    for (auto it = this->children.begin(); it != this->children.end(); it++) {
        HWComponent* hwc = it->second.get();
        if (hwc->get_name() == name)
            return hwc;
        if ((hwc = hwc->get_comp_by_name(name, depth + 1)))
            return hwc;
    }
    if (depth == 0 && !optional)
        LOG_F(WARNING, "Component name %s not found!", name.c_str());
    return nullptr;
}

HWComponent* HWComponent::get_comp_by_name_optional(const std::string name) {
    return this->get_comp_by_name(name, true);
}

HWComponent* HWComponent::get_comp_by_type(HWCompType type, int depth) {
    for (auto it = this->children.begin(); it != this->children.end(); it++) {
        HWComponent* hwc = it->second.get();
        if (hwc->supports_type(type))
            return hwc;
        if ((hwc = hwc->get_comp_by_type(type, depth + 1)))
            return hwc;
    }
    return nullptr;
}

int HWComponent::postinit_devices(int &devices_inited) {
    std::vector<HWComponent*> devices;

    // First, make a copy of the children list in case a device wants to move to a different parent
    for (auto it = this->children.begin(); it != this->children.end(); it++)
        devices.push_back(it->second.get());

    for (auto it = devices.begin(); it != devices.end(); it++) {
        HWComponent* hwc = *it;
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s %s", hwc->postinitialized ? "Check" : "Post init", hwc->get_name_and_unit_address().c_str());
        int postinit_result = hwc->postinit_devices(devices_inited);
        if (postinit_result < 0) {
            LOG_F(INFO, "A device could not be initialized.");
            return -1;
        }
        if (!hwc->postinitialized) {
            devices_inited++;
            postinit_result = hwc->device_postinit();
            if (postinit_result < 0) {
                LOG_F(ERROR, "Could not initialize device %s", hwc->get_path().c_str());
                return -1;
            }
            if (postinit_result > 0) {
                LOG_F(INFO, "Will retry post init %s later", hwc->get_path().c_str());
            } else {
                hwc->postinitialized = true;
            }
        }
    }

    return 0;
}

int HWComponent::postinit_devices() {
    int devices_inited;
    int i = 0;
    int result;
    do {
        devices_inited = 0;
        i++;
        {
            VLOG_SCOPE_F(loguru::Verbosity_INFO, "Post init loop %d", i);
            result = this->postinit_devices(devices_inited);
            LOG_F(INFO, "%d devices initialized.", devices_inited);
        }
    } while (!parent && !result && devices_inited);
    return result;
}

std::string HWComponent::extract_device_name(const std::string name) {
    std::size_t pos = name.find('@');
    if (pos == std::string::npos)
        return name;
    return name.substr(0, pos);
}

std::string HWComponent::extract_unit_address(const std::string name) {
    std::size_t pos = name.find('@');
    if (pos == std::string::npos)
        return "";
    return name.substr(pos + 1);
}

int32_t HWComponent::parse_self_unit_address_string(const std::string unit_address) {
    try {
        return (int32_t)std::stol(unit_address, nullptr, 16);
    } catch (...) {
        return -1;
    }
}

std::string HWComponent::get_self_unit_address_string(int32_t unit_address) {
    char buf[20];
    if (unit_address < 0 && unit_address >= -1000)
        return "";
    if (unit_address < 0)
        snprintf(buf, sizeof(buf), "@%08X", unit_address);
    else
        snprintf(buf, sizeof(buf), "@%X", unit_address);
    return buf;
}

std::string HWComponent::get_child_unit_address_string(int32_t unit_address) {
    return get_self_unit_address_string(unit_address);
}

std::string HWComponent::get_self_unit_address_string() {
    return this->get_self_unit_address_string(this->unit_address);
}

std::string HWComponent::get_path() {
    return
        (
            this->parent ?
                this->parent->get_path() + "/"
            :
                std::string("")
        )
        + this->get_name_and_unit_address()
    ;
}

std::string HWComponent::get_name_and_unit_address() {
    return this->get_name() + this->get_self_unit_address_string();
}

void HWComponent::dump_devices(int indent) {
    printf("%*s%s\n", indent, "", this->get_name_and_unit_address().c_str());
    indent += 4;
    for (auto it = this->children.begin(); it != this->children.end(); it++) {
        HWComponent* hwc = it->second.get();
        if (hwc == nullptr) {
            printf("%*snullptr%s\n", indent, "", this->get_child_unit_address_string(it->first).c_str());
        } else {
            hwc->dump_devices(indent);
        }
    }
}
