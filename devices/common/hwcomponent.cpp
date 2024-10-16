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

#include <devices/common/hwcomponent.h>
#include <loguru.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <regex>
#include <iostream>

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
            return dev_obj;
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

HWComponent* HWComponent::get_comp_by_name(const std::string name, bool optional) {
    HWComponent* hwc;
    if (this->iterate([&](HWComponent *it, int depth) {
        if (it->get_name() == name) {
            hwc = it;
            return true;
        }
        return false;
    }))
        return hwc;
    if (!optional)
        LOG_F(WARNING, "Component name %s not found!", name.c_str());
    return nullptr;
}

HWComponent* HWComponent::get_comp_by_name_optional(const std::string name) {
    return this->get_comp_by_name(name, true);
}

HWComponent* HWComponent::get_comp_by_type(HWCompType type) {
    HWComponent* hwc;
    if (this->iterate([&](HWComponent *it, int depth) {
        if (it->supports_type(type)) {
            hwc = it;
            return true;
        }
        return false;
    }))
        return hwc;
    return nullptr;
}

PostInitResultType HWComponent::postinit_devices(int &devices_inited, int &devices_skipped) {
    std::vector<HWComponent*> devices;

    // Make a copy of the children list in case a device wants to move to a different parent.
    for (auto it = this->children.begin(); it != this->children.end(); it++)
        devices.push_back(it->second.get());

    // Iterate the copy of the children list.
    for (auto it = devices.begin(); it != devices.end(); it++) {
        HWComponent* hwc = *it;
        if (hwc->postinitialized) {
            if (hwc->postinit_device(devices_inited, devices_skipped) == PI_FAIL)
                return PI_FAIL;
        }
        else {
            VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s %s", hwc->postinitialized ? "Check" : "Post init",
                hwc->get_name_and_unit_address().c_str());
            if (hwc->postinit_device(devices_inited, devices_skipped) == PI_FAIL)
                return PI_FAIL;
        }
    }

    return PI_SUCCESS;
}

PostInitResultType HWComponent::postinit_device(int &devices_inited, int &devices_skipped) {
    int postinit_result = this->postinit_devices(devices_inited, devices_skipped);
    if (postinit_result == PI_FAIL) {
        LOG_F(INFO, "A device could not be initialized.");
        return PI_FAIL;
    }
    if (!this->postinitialized) {
        postinit_result = this->device_postinit();
        if (postinit_result == PI_FAIL) {
            LOG_F(ERROR, "Could not initialize device %s", this->get_path().c_str());
            return PI_FAIL;
        }
        if (postinit_result == PI_RETRY) {
            devices_skipped++;
            LOG_F(INFO, "Will retry post init %s later", this->get_path().c_str());
        } else {
            devices_inited++;
            this->postinitialized = true;
        }
    }
    return PI_SUCCESS;
}

PostInitResultType HWComponent::postinit_devices() {
    int devices_inited, devices_skipped;
    int i = 0;
    PostInitResultType result;
    do {
        devices_inited = devices_skipped = 0;
        i++;
        {
            VLOG_SCOPE_F(loguru::Verbosity_INFO, "Post init loop %d", i);
            result = this->postinit_devices(devices_inited, devices_skipped);
            if (devices_inited)
                LOG_F(INFO, "%d devices initialized.", devices_inited);
            if (devices_skipped)
                LOG_F(INFO, "%d devices skipped.", devices_skipped);
        }
    } while ((this->parent == nullptr) && (result == PI_SUCCESS) && (devices_inited > 0));
    if (devices_skipped)
        return PI_RETRY;
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

int32_t HWComponent::parse_self_unit_address_string(const std::string unit_address_string) {
    std::regex unit_address_re("0*([0-9A-F]+)", std::regex_constants::icase);
    std::smatch results;
    if (std::regex_match(unit_address_string, results, unit_address_re)) {
        try {
            return (int32_t)std::stoul(results[1], nullptr, 16);
        } catch (...) {
            return -1;
        }
    } else {
        return -1;
    }
}

int32_t HWComponent::parse_child_unit_address_string(const std::string unit_address_string) {
    return parse_self_unit_address_string(unit_address_string);
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
    std::string path;
    for (HWComponent *hwc = this; hwc; hwc = hwc->parent)
        path = (hwc->parent ? "/" : "") + hwc->get_name_and_unit_address() + path;
    return path;
}

std::string HWComponent::get_name_and_unit_address() {
    return this->get_name() + this->get_self_unit_address_string();
}

void HWComponent::dump_devices(int indent) {
    this->iterate([&](HWComponent *it, int depth) {
        printf("%*s%s", depth * 4 + indent, "", it->get_name_and_unit_address().c_str());
        std::cout << std::endl;
        return false;
    });
}

bool HWComponent::iterate(const std::function<bool(HWComponent *it, int depth)> &func, int depth) {
    if (func(this, depth))
        return true;
    for (auto it = this->children.begin(); it != this->children.end(); it++)
        if (it->second.get()->iterate(func, depth + 1))
            return true;
    return false;
}
