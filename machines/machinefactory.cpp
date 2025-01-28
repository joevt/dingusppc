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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/bootrom.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/sound/soundserver.h>
#include <loguru.hpp>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <machines/romidentity.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <regex>

using namespace std;

map<string, unique_ptr<Setting>> gMachineSettings;

const map<string, PropHelpItem> gPropHelp = {
    {"rambank1_size",   {PropertyMachine, "specifies RAM bank 1 size in MB"}},
    {"rambank2_size",   {PropertyMachine, "specifies RAM bank 2 size in MB"}},
    {"rambank3_size",   {PropertyMachine, "specifies RAM bank 3 size in MB"}},
    {"rambank4_size",   {PropertyMachine, "specifies RAM bank 4 size in MB"}},
    {"rambank5_size",   {PropertyMachine, "specifies RAM bank 5 size in MB"}},
    {"rambank6_size",   {PropertyMachine, "specifies RAM bank 6 size in MB"}},
    {"rambank7_size",   {PropertyMachine, "specifies RAM bank 7 size in MB"}},
    {"rambank8_size",   {PropertyMachine, "specifies RAM bank 8 size in MB"}},
    {"rambank9_size",   {PropertyMachine, "specifies RAM bank 9 size in MB"}},
    {"rambank10_size",  {PropertyMachine, "specifies RAM bank 10 size in MB"}},
    {"rambank11_size",  {PropertyMachine, "specifies RAM bank 11 size in MB"}},
    {"rambank12_size",  {PropertyMachine, "specifies RAM bank 12 size in MB"}},
    {"rambank0_size",   {PropertyMachine, "specifies onboard RAM bank size in MB"}},
    {"gfxmem_banks",    {PropertyDevice , "specifies video memory layout for Control video"}},
    {"gfxmem_size",     {PropertyDevice , "specifies video memory size in MB"}},
    {"num_displays",    {PropertyDevice , "specifies the number of displays supported by a graphics controller"}},
    {"fdd_drives",      {PropertyMachine, "specifies the number of floppy drives"}},
    {"fdd_img",         {PropertyDevice , "specifies path to floppy disk image"}},
    {"fdd_fmt",         {PropertyDevice , "specifies floppy disk format (use before fdd_img)"}},
    {"fdd_wr_prot",     {PropertyDevice , "specifies floppy disk's write protection setting"}},
    {"hdd_img",         {PropertyDevice , "specifies path to a disk image that will be used as a hard disk"}},
    {"hdd_part",        {PropertyDevice , "specifies path to a disk image to be appended to a hard disk"}},
    {"cdr_config",      {PropertyMachine, "CD-ROM device path in [bus]:[device#] format"}},
    {"hdd_config",      {PropertyMachine, "HD device path in [bus]:[device#] format"}},
    {"cdr_img",         {PropertyDevice , "specifies path to CD-ROM image"}},
    {"mon_id",          {PropertyDevice , "specifies which monitor to emulate"}},
    {"edid",            {PropertyDevice , "specifies an EDID for a display"}},
    {"pci",             {PropertyDevice,  "inserts PCI device into a free slot"}},
    {"vci",             {PropertyDevice,  "inserts PCI device into a free slot of VCI"}},
    {"pci_dev_max",     {PropertyMachine, "specifies the maximum PCI device number for PCI bridges"}},
    {"pci_GPU",         {PropertyDevOnce, "specifies PCI device for Beige G3 grackle device @12"}},
    {"pci_J12",         {PropertyDevOnce, "inserts PCI device into 32-bit 66MHz slot J12"}},
    {"pci_J11",         {PropertyDevOnce, "inserts PCI device into 64-bit 33MHz slot J11"}},
    {"pci_J10",         {PropertyDevOnce, "inserts PCI device into 64-bit 33MHz slot J10"}},
    {"pci_J9",          {PropertyDevOnce, "inserts PCI device into 64-bit 33MHz slot J9"}},
    {"pci_FireWire",    {PropertyDevOnce, "inserts PCI device into PCI slot reserved for Yosemite FireWire"}},
    {"pci_UltraATA",    {PropertyDevOnce, "inserts PCI device into PCI slot reserved for Yosemite Ultra ATA"}},
    {"pci_USB",         {PropertyDevOnce, "inserts PCI device into PCI slot reserved for Yosemite USB"}},
    {"pci_PERCH",       {PropertyDevOnce, "inserts PCI device into PERCH slot"}},
    {"pci_CARDBUS",     {PropertyDevOnce, "inserts PCI device into PCI slot reserved for Lombard CardBus"}},
    {"pci_ZIVA",        {PropertyDevOnce, "inserts PCI device into PCI slot reserved for Lombard DVD Decoder"}},
    {"pci_A1",          {PropertyDevOnce, "inserts PCI device into slot A1"}},
    {"pci_B1",          {PropertyDevOnce, "inserts PCI device into slot B1"}},
    {"pci_C1",          {PropertyDevOnce, "inserts PCI device into slot C1"}},
    {"pci_E1",          {PropertyDevOnce, "inserts PCI device into slot E1"}},
    {"pci_F1",          {PropertyDevOnce, "inserts PCI device into slot F1"}},
    {"pci_D2",          {PropertyDevOnce, "inserts PCI device into slot D2"}},
    {"pci_E2",          {PropertyDevOnce, "inserts PCI device into slot E2"}},
    {"pci_F2",          {PropertyDevOnce, "inserts PCI device into slot F2"}},
    {"vci_D",           {PropertyDevOnce, "inserts VCI device 0x0D"}},
    {"vci_E",           {PropertyDevOnce, "inserts VCI device 0x0E"}},
    {"rom",             {PropertyDevice , "specifies path to NuBus or PCI ROM image"}},
    {"serial_backend",  {PropertyDevice , "specifies the backend for the serial port"}},
    {"emmo",            {PropertyMachine, "enables/disables factory HW tests during startup"}},
    {"cpu",             {PropertyMachine, "specifies CPU"}},
    {"video_out",       {PropertyMachine, "specifies Pippin video output connection type"}},
    {"adb_devices",     {PropertyMachine, "specifies which ADB device(s) to attach"}},
    {"pds",             {PropertyMachine, "specify device for the processsor direct slot"}},
    {"has_composite",   {PropertyMachine, "indicates if composite video output is connected"}},
    {"has_svideo",      {PropertyMachine, "indicates if s-video output is connected"}},
    {"debug_copland",   {PropertyMachine, "enables/disables entry into debugger during Copland Open Firmware initialization"}},
};

static uint32_t adler32(char *buf, size_t len) {
    uint32_t sum1 = 1;
    uint32_t sum2 = 0;
    while (len--) {
        sum1 = (sum1 + *(uint8_t*)buf++) % 65521;
        sum2 = (sum2 + sum1) % 65521;
    }
    return sum1 + 65536 * sum2;
}

static uint32_t oldworldchecksum(char *buf, size_t len) {
    uint32_t ck = 0;
    while (len) {
        ck += READ_WORD_BE_A(buf);
        buf += 2;
        len -= 2;
    }
    return ck;
}

static uint64_t oldworldchecksum64(char *buf, size_t len, size_t config_info_offset) {
    uint64_t ck = 0;
    for (size_t i = 0; i < len; i += 8, buf += 8) {
        if (i < config_info_offset || i >= config_info_offset + 40)
            ck += READ_QWORD_BE_A(buf);
    }
    return ck;
}

void MachineFactory::list_machines()
{
    cout << endl << "Supported machines:" << endl << endl;

    for (auto& m : DeviceRegistry::get_registry()) {
        if (m.second.supports_types & HWCompType::MACHINE)
            cout << setw(13) << right << m.first << "\t\t" << m.second.description << endl;
    }

    cout << endl;
}

HWComponent* MachineFactory::create_device(HWComponent *parent, string dev_name, HWCompType supported_types)
{
    VLOG_SCOPE_F(loguru::Verbosity_INFO, "Creating device %s", dev_name.c_str());

    std::string unit_address_string = HWComponent::extract_unit_address(dev_name);
    dev_name = HWComponent::extract_device_name(dev_name);

    DeviceDescription* dev = &DeviceRegistry::get_descriptor(dev_name);
    if (!dev) {
        LOG_F(ERROR, "%s is not a registered device", dev_name.c_str());
        return nullptr;
    }

    if (!dev->description.empty())
        LOG_F(INFO, "Description: %s", dev->description.c_str());

    if (supported_types != HWCompType::UNKNOWN && !(supported_types & dev->supports_types)) {
        LOG_F(ERROR, "Device %s is not a supported type", dev_name.c_str());
        return nullptr;
    }

    MachineFactory::register_device_settings(dev_name);

    int32_t unit_address;
    for (unit_address = -999; parent->children.count(unit_address); unit_address += 1) {}

    HWComponent *temp_obj = nullptr;
    if (dev->subdev_list.size()) {
        temp_obj = new HWComponent(dev_name + " (temporary)");
    }

    if (temp_obj) {
        temp_obj->supports_types(dev->supports_types);
        parent->add_device(unit_address, temp_obj);

        for (auto& subdev_name : dev->subdev_list) {
            create_device(temp_obj, subdev_name);
        }
    }

    std::unique_ptr<HWComponent> dev_obj = dev->m_create_func(dev_name);
    HWComponent *hwc = dev_obj.get();

    hwc->init_device_settings(*dev);

    if (hwc->get_name() != dev_name) {
        if (hwc->get_name().empty()) {
            LOG_F(INFO, "Set name to \"%s\"", dev_name.c_str());
        } else {
            LOG_F(INFO, "Changed name from \"%s\" to \"%s\"", hwc->get_name().c_str(), dev_name.c_str());
        }
        hwc->set_name(dev_name);
    }

    if (temp_obj) {
        temp_obj->move_children(hwc);
        parent->remove_device(unit_address); // delete temp_obj
    }

    if (!unit_address_string.empty()) {
        unit_address = dev_obj->parse_self_unit_address_string(unit_address_string);
    }

    parent->add_device(unit_address, dev_obj.release(), dev_name);

    if (config_stack_ready) {
        if (
            config_stack.empty() ||
            config_stack.back().stack_item_type != ConfigStackItem::HWC ||
            config_stack.back().hwc != hwc
        ) {
            if (
                !config_stack.empty() &&
                config_stack.back().stack_item_type == ConfigStackItem::HWC_WITH_UNIT_ADDRESS &&
                config_stack.back().hwc == hwc->get_parent() &&
                config_stack.back().unit_address == hwc->get_unit_address()
            )
                config_stack.pop_back();
            config_stack.push_back(ConfigStackItem(hwc));
        }
    }

    return hwc;
}

vector<ConfigStackItem> MachineFactory::config_stack;
bool MachineFactory::config_stack_ready;

int MachineFactory::create(string& mach_id, vector<std::string> &app_args)
{
    LOG_F(INFO, "Initializing hardware...");

    config_stack.clear();
    config_stack_ready = false;
    gMachineSettings.clear();
    Setting::loaded_properties.clear();

    // initialize global machine object
    gMachineObj.reset(new HWComponent("DingusPPC"));

    // create and register sound server
    gMachineObj->add_device(-1000, new SoundServer());

    // recursively create device objects
    if (!create_device(gMachineObj.get(), mach_id, HWCompType::MACHINE)) {
        LOG_F(ERROR, "Machine initialization failed!");
        gMachineObj->clear_devices();
        return -1;
    }

    if (gMachineObj->postinit_devices() == PI_FAIL) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    config_stack.push_back(ConfigStackItem(gMachineObj.get()));
    config_stack_ready = true;

    {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "Applying configs");
        MachineFactory::apply_configs();
    }

    if (gMachineObj->postinit_devices() == PI_FAIL) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    {
        std::regex argument_re("--([^=]+)(?:=(.*))?");

        VLOG_SCOPE_F(loguru::Verbosity_INFO, "Parsing remaining command line arguments");
        std::reverse(app_args.begin(), app_args.end());
        int arg_index = 0;
        while (arg_index < app_args.size()) {
            std::smatch results;
            HWComponent *matched_hwc;
            int32_t matched_unit_address;
            bool is_leaf_match;

            VLOG_SCOPE_F(loguru::Verbosity_INFO, "checking arg: %s", app_args[arg_index].c_str());

            if (app_args[arg_index] == "(") {
                config_stack.push_back(ConfigStackItem(ConfigStackItem::BLOCK_BEGIN));
                app_args.erase(app_args.begin() + arg_index);
            }
            else if (app_args[arg_index] == ")") {
                bool did_erase = false;
                for (int i = int(config_stack.size()) - 1; i >= 0; i--) {
                    if (config_stack[i].stack_item_type == ConfigStackItem::BLOCK_BEGIN) {
                        config_stack.erase(config_stack.begin() + i, config_stack.end());
                        did_erase = true;
                        app_args.erase(app_args.begin() + arg_index);
                    }
                }
                if (!did_erase) {
                    LOG_F(ERROR, "Missing matching open parenthesis \"(\".");
                    arg_index++;
                }
            }
            else if (app_args[arg_index] == ";") {
                if (config_stack.size() > 0) {
                    config_stack.pop_back();
                    app_args.erase(app_args.begin() + arg_index);
                } else {
                    LOG_F(ERROR, "Empty config stack.");
                    arg_index++;
                }
            }
            else if (app_args[arg_index] == "dump_stack") {
                app_args.erase(app_args.begin() + arg_index);
                cout << endl << "    Config stack:" << endl;
                if (config_stack.empty())
                    cout << "        Empty!" << endl;
                for (auto &cs : config_stack) {
                    switch (cs.stack_item_type) {
                        case ConfigStackItem::HWC:
                            cout << "        " << cs.hwc->get_path() << endl;
                            break;
                        case ConfigStackItem::HWC_WITH_UNIT_ADDRESS:
                            cout << "        " << cs.hwc->get_path() << "/" <<
                                cs.hwc->get_child_unit_address_string(cs.unit_address) << endl;
                            break;
                        case ConfigStackItem::BLOCK_BEGIN:
                            cout << "        (" << endl;
                            break;
                    }
                }
            }
            else if (app_args[arg_index] == "dump_devices") {
                app_args.erase(app_args.begin() + arg_index);
                cout << endl << "    Devices:" << endl;
                gMachineObj->dump_devices(8);
            }
            else if (std::regex_match(app_args[arg_index], results, argument_re)) {
                std::string property = results[1];
                std::string value;
                if (results[2].matched) {
                    value = results[2];
                } else {
                    if (arg_index + 1 < app_args.size()) {
                        value = app_args[arg_index + 1];
                        app_args[arg_index] += "=" + value;
                        LOG_F(INFO, "with value: %s", app_args[arg_index].c_str());
                        app_args.erase(app_args.begin() + arg_index + 1);
                    } else {
                        LOG_F(ERROR, "Missing value for property \"%s\".", property.c_str());
                        arg_index++;
                        continue;
                    }
                }

                matched_hwc = MachineFactory::set_property(property, value);

                if (matched_hwc) {
                    if (
                        config_stack.empty() ||
                        config_stack.back().stack_item_type != ConfigStackItem::HWC ||
                        config_stack.back().hwc != matched_hwc
                    ) {
                        config_stack.push_back(ConfigStackItem(matched_hwc));
                    }
                    app_args.erase(app_args.begin() + arg_index);
                    if (gMachineObj->postinit_devices() == PI_FAIL) {
                        LOG_F(ERROR, "Could not post-initialize devices!");
                        return -1;
                    }
                } else {
                    cout << "    Unused setting: " << property << " = " << value << endl;
                    arg_index++;
                }
            }
            else if (MachineFactory::find_path(app_args[arg_index], matched_hwc, matched_unit_address, is_leaf_match)) {
                if (is_leaf_match) {
                    if (
                        config_stack.empty() ||
                        config_stack.back().stack_item_type != ConfigStackItem::HWC_WITH_UNIT_ADDRESS ||
                        config_stack.back().hwc != matched_hwc ||
                        config_stack.back().unit_address != matched_unit_address
                    ) {
                        config_stack.push_back(ConfigStackItem(matched_hwc, matched_unit_address));
                    }
                } else {
                    if (
                        config_stack.empty() ||
                        config_stack.back().stack_item_type != ConfigStackItem::HWC ||
                        config_stack.back().hwc != matched_hwc
                    ) {
                        if (
                            !config_stack.empty() &&
                            config_stack.back().stack_item_type == ConfigStackItem::HWC_WITH_UNIT_ADDRESS &&
                            config_stack.back().hwc == matched_hwc->get_parent() &&
                            config_stack.back().unit_address == matched_hwc->get_unit_address()
                        )
                            config_stack.pop_back();
                        config_stack.push_back(ConfigStackItem(matched_hwc));
                    }
                }
                app_args.erase(app_args.begin() + arg_index);
            }
            else {
                arg_index++;
            }
        } // while app_args

    }
    config_stack.clear();

    if (!app_args.empty()) {
        cout << endl << "Unused command line arguments:" << endl;
        for (auto& arg : app_args)
            cout << "    " << arg << endl;
        cout << endl;
        LOG_F(ERROR, "Unused command line arguments!");
    }

    if (gMachineObj->postinit_devices() == PI_FAIL) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "Applying defaulted device settings");
        gMachineObj->iterate(
            [&](HWComponent *it, int depth) {
                if (it->device_settings.empty())
                    return false;
                for (auto& s : it->device_settings) {
                    if (s.second->value_commandline == s.second->value_not_inited) {
                        LOG_F(INFO, "Defaulting %s property \"%s\"", it->get_path().c_str(), s.first.c_str());
                        it->set_property(s.first, s.second->value_default);
                        s.second->value_commandline = s.second->value_defaulted;
                    }
                }
                return false;
            }
        );
    }

    if (gMachineObj->postinit_devices() != PI_SUCCESS) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "Checking for non-ready devices");
        if (
            gMachineObj->iterate(
                [&](HWComponent *it, int depth) {
                    if (!it->is_ready_for_machine()) {
                        LOG_F(ERROR, "Unready device %s", it->get_name_and_unit_address().c_str());
                        return true;
                    }
                    return false;
                }
            )
        ) {
            return -1;
        }
    }

    LOG_F(INFO, "Initialization completed.");
    printf("\nMachine after init:\n");
    gMachineObj->dump_devices(4);

    return 0;
}

HWComponent* MachineFactory::set_property(const std::string &property, const std::string &value)
{
    HWComponent *hwc = nullptr;
    // VLOG_SCOPE_F(loguru::Verbosity_INFO, "set_property %s = %s", property.c_str(), value.c_str());

    for (int i = int(config_stack.size()) - 1; i >= 0; i--) {
        // VLOG_SCOPE_F(loguru::Verbosity_INFO, "config stack %d", i);
        ConfigStackItem *cs = &config_stack[i];
        if (cs->stack_item_type == ConfigStackItem::HWC) {
            if (cs->hwc->iterate(
                [&](HWComponent *it, int depth) {
                    // VLOG_SCOPE_F(loguru::Verbosity_INFO, "checking type 1 %s", it->get_path().c_str());
                    hwc = it->set_property(property, value, -1);
                    if (hwc) {
                        // LOG_F(INFO, "found at %s", hwc->get_path().c_str());
                        return true;
                    }
                    return false;
                }
            ))
                return hwc;
        }
        else if (cs->stack_item_type == ConfigStackItem::HWC_WITH_UNIT_ADDRESS) {
            #if 0
                VLOG_SCOPE_F(loguru::Verbosity_INFO, "checking type 2 %s%s",
                    cs->hwc->get_path().c_str(), cs->hwc->get_child_unit_address_string(cs->unit_address).c_str());
            #endif
            hwc = cs->hwc->set_property(property, value, cs->unit_address);
            if (hwc)
                return hwc;
        }
    }
    return nullptr;
}

std::regex MachineFactory::path_re("(?:([^\\s]*)/)?([^\\s@]+)?(?:@([\\dA-F,]+))?", std::regex_constants::icase);

bool MachineFactory::find_path(std::string path, HWComponent *&hwc, int32_t &unit_address, bool &is_leaf_match)
{
    std::smatch results;
    if (!std::regex_match(path, results, MachineFactory::path_re) || !(results[2].matched || results[3].matched)) {
        LOG_F(ERROR, "Invalid device path \"%s\"", path.c_str());
        return false;
    }

    bool leaf_search = !results[2].matched && results[3].matched;

    for (int search_type = 0; search_type < 1 + leaf_search; search_type++) {
        for (int i = int(config_stack.size()) - 1; i >= 0; i--) {
            ConfigStackItem *cs = &config_stack[i];
            if (cs->stack_item_type == ConfigStackItem::BLOCK_BEGIN) {
                /*
                    All items on the stack are usable, so continue looking.
                    If the user didn't want previous items outside the block
                    to be usable, then the user would pop them off the stack
                    using \) or \;
                */
            } else if (cs->stack_item_type == ConfigStackItem::HWC) {
                hwc = cs->hwc->find_path(path, 1 << search_type, true, &is_leaf_match, &unit_address);
                if (hwc)
                    return true;
            } else if (search_type == 1 && cs->stack_item_type == ConfigStackItem::HWC_WITH_UNIT_ADDRESS) {
                if (cs->hwc->path_match(results[1], true)) {
                    HWComponent *it = cs->hwc;
                    if (cs->hwc->parse_child_unit_address_string(results[3], it) == cs->unit_address) {
                        hwc = it;
                        unit_address = cs->unit_address;
                        is_leaf_match = true;
                        return true;
                    }
                }
            }
        }
    }
    LOG_F(ERROR, "Device path \"%s\" not found!", path.c_str());
    return false;
}

void MachineFactory::apply_configs()
{
    std::map<std::string, std::map<HWCompType, std::string>> configs = {
        {"hdd_config", {{HWCompType::IDE_BUS, "AtaHardDisk"}, {HWCompType::SCSI_BUS, "ScsiHardDisk"}}},
        {"cdr_config", {{HWCompType::IDE_BUS, "AtapiCdrom" }, {HWCompType::SCSI_BUS, "ScsiCdrom"   }}},
    };
    for (auto& config : configs) {
        std::string the_config;
        try {
            the_config = GET_STR_PROP(config.first);
        } catch (...) {
            continue;
        }
        if (the_config.empty())
            continue;
        int32_t unit_address;
        HWComponent* bus_obj = gMachineObj->find_path(the_config, 2, true, nullptr, &unit_address);
        if (!bus_obj)
            continue;
        for (auto& bus_type : config.second) {
            if (bus_obj->supports_type(bus_type.first)) {
                MachineFactory::create_device(bus_obj, bus_type.second + bus_obj->get_child_unit_address_string(unit_address));
            }
        }
    }
}

void MachineFactory::list_properties(vector<string> machine_list)
{
    cout << endl;

    if (machine_list.empty()) {
        for (auto& mach : DeviceRegistry::get_registry()) {
            if (mach.second.supports_types & HWCompType::MACHINE) {
                cout << mach.second.description << " supported properties:" << endl << endl;
                std::set<string> properties;
                list_device_settings(mach.second, PropertyMachine, 0, "", "", &properties);
                cout << "    per device properties:" << endl << endl;
                list_device_settings(mach.second, PropertyDevice, 0, "", "", nullptr);
            }
        }
    } else {
        for (auto& name : machine_list) {
            auto it = DeviceRegistry::get_registry().find(name);
            if (it != DeviceRegistry::get_registry().end()) {
                cout << (it->second.description.empty() ? name : it->second.description)
                    << " supported properties:" << endl << endl;
                std::set<string> properties;
                list_device_settings(it->second, PropertyMachine, 0, "", "", &properties);
                cout << "    per device properties:" << endl << endl;
                list_device_settings(it->second, PropertyDevice, 0, "", "", nullptr);
            }
            else {
                cout << name << " is not a valid machine or device." << endl << endl;
            }
        }
    }

    cout << endl;
}

void MachineFactory::list_device_settings(DeviceDescription& dev, PropScope scope,
    int indent, string path, string device, std::set<string> *properties)
{
    print_settings(dev.properties, scope, indent, path, device, properties);

    for (auto& d : dev.subdev_list) {
        list_device_settings(DeviceRegistry::get_descriptor(HWComponent::extract_device_name(d)),
            scope, scope == PropertyMachine ? indent : indent + 4, path + "/" + d, d, properties
        );
    }
}

void MachineFactory::print_settings(const PropMap& prop_map, PropScope scope,
    int /*indent*/, string path, string device, std::set<string> *properties)
{
    string help;

    bool did_path = scope == PropertyMachine;

    for (auto& p : prop_map) {
        if (properties) {
            if (properties->count(p.first))
                continue;
            properties->insert(p.first);
        }

        auto phelp = gPropHelp.find(p.first);
        if (phelp != gPropHelp.end()) {
            if ((phelp->second.property_scope == PropertyMachine) != (scope == PropertyMachine))
                continue;
            help = phelp->second.property_description;
        } else {
            if (scope == PropertyMachine)
                continue;
            help = "";
        }

        if (!did_path) {
            // don't print path because registry path is not the same as config path
            cout << setw(4) << "" << device << endl;
            did_path = true;
        }

        cout << setw(16) << right << p.first << "    " << help << endl;

        cout << setw(16) << "" << "    " << "Valid values: ";

        switch(p.second->get_type()) {
        case PROP_TYPE_INTEGER:
            cout << dynamic_cast<IntProperty*>(p.second)->get_valid_values_as_str();
            break;
        case PROP_TYPE_STRING:
            cout << dynamic_cast<StrProperty*>(p.second)->get_valid_values_as_str();
            break;
        case PROP_TYPE_BINARY:
            cout << dynamic_cast<BinProperty*>(p.second)->get_valid_values_as_str();
            break;
        default:
            cout << "???";
            break;
        }
        cout << endl;
        cout << endl;
    }
}

void MachineFactory::register_device_settings(const std::string& name)
{
    auto dev = DeviceRegistry::get_descriptor(HWComponent::extract_device_name(name));
    MachineFactory::register_settings(name, dev.properties);
    for (auto& d : dev.subdev_list) {
        MachineFactory::register_device_settings(d);
    }
}

void MachineFactory::register_settings(const std::string& dev_name, const PropMap& props) {
    for (auto& p : props) {

        if (gPropHelp.count(p.first) == 0) {
            LOG_F(ERROR, "Missing help for setting \"%s\" from %s.", p.first.c_str(), dev_name.c_str());
            continue;
        }

        auto& phelp = gPropHelp.at(p.first);
        if (phelp.property_scope == PropertyDevice)
            continue;

        if (gMachineSettings.count(p.first) == 0) {
            gMachineSettings[p.first] = unique_ptr<Setting>(new Setting());

            auto override_value = get_setting_value(p.first);
            if (override_value)
                gMachineSettings[p.first]->value_commandline = *override_value;
        }

        auto &s = gMachineSettings[p.first];
        if (s->property) {
            if (Setting::loaded_properties.count(p.second) == 0) {
                /*
                    We might iterate this same device multiple times.
                    If we haven't loaded this property yet, then report that we are
                    ignoring this setting that was overridden by an ancestor device.
                */
                LOG_F(INFO, "Ignoring setting \"%s\" from %s.", p.first.c_str(), dev_name.c_str());
                Setting::loaded_properties.insert(p.second);
            }
        }
        else {
            LOG_F(INFO, "Adding setting \"%s\" = \"%s\" from %s.",
                p.first.c_str(), p.second->get_string().c_str(), dev_name.c_str());
            s->set_property_info(p.second);
            Setting::loaded_properties.insert(p.second);
        }
    }
}

void MachineFactory::summarize_machine_settings() {
    cout << endl << "Machine settings summary: " << endl;

    for (auto& s : gMachineSettings) {
        if (s.second->property) {
            cout << "    " << s.first <<
                (
                    s.second->value_commandline == s.second->value_not_inited ?
                        " (default)"
                    :
                        ""
                )
                << " : " << s.second->property->get_string()
                << endl
            ;
        }
    }
}

void MachineFactory::summarize_device_settings() {
    cout << endl << "Device settings summary: " << endl;
    gMachineObj->iterate(
        [&](HWComponent *it, int depth) {
            if (it->device_settings.empty())
                return false;
            cout << "    " << it->get_path() << endl;
            for (auto& s : it->device_settings) {
                if (s.second->property) {
                    cout << "        " << s.first <<
                        (
                            s.second->value_commandline == s.second->value_not_inited ?
                                " (default)"
                            : s.second->value_commandline == s.second->value_defaulted ?
                                " (defaulted)"
                            :
                                ""
                        )
                        << " : " << s.second->property->get_string()
                        << endl
                    ;
                }
            }
            return false;
        }
    );
}

size_t MachineFactory::read_boot_rom(string& rom_filepath, char *rom_data)
{
    ifstream rom_file;
    size_t file_size;

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Could not open the specified ROM file.");
        file_size = 0;
        goto bail_out;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    if (file_size < 64 * 1024 || file_size > 4 * 1024 * 1024) {
        LOG_F(ERROR, "Unexpected ROM file size: %zu bytes. Expected size is 1 or 4 megabytes.", file_size);
        file_size = 0;
        goto bail_out;
    }

    if (rom_data) {
        rom_file.seekg(0, ios::beg);
        rom_file.read(rom_data, file_size);
    }

bail_out:
    rom_file.close();

    return file_size;
}

string MachineFactory::machine_name_from_rom(char *rom_data, size_t rom_size) {
    uint32_t date = 0;
    uint16_t major_version = 0;
    uint16_t minor_version = 0;
    uint32_t firmware_version = 0;
    uint32_t nw_product_id = 0;
    uint32_t ow_checksum_stored          = 0; uint32_t ow_checksum_calculated          = 0;
    uint64_t ow_checksum64_stored        = 0; uint64_t ow_checksum64_calculated        = 0;
    uint32_t nw_start_checksum_stored    = 0; uint32_t nw_start_checksum_calculated    = 0;
    uint32_t nw_config_checksum_stored   = 0; uint32_t nw_config_checksum_calculated   = 0;
    uint32_t nw_recovery_checksum_stored = 0; uint32_t nw_recovery_checksum_calculated = 0;
    uint32_t nw_romimage_checksum_stored = 0; uint32_t nw_romimage_checksum_calculated = 0;
    uint16_t nw_config_signature = 0;
    bool has_nw_config = false;
    bool is_nw = false;
    uint32_t nw_subconfig_checksum_calculated = 0;

    char expected_ow[24];
    char expected_ow64[40];
    char expected_start[24];
    char expected_config[24];
    char expected_recovery[24];
    char expected_romimage[24];
    auto checksum_verbosity = loguru::Verbosity_INFO;
    auto checksum_verbosity2 = loguru::Verbosity_INFO;
    expected_ow[0] = expected_ow64[0] = 0;
    expected_start[0] = expected_config[0] = expected_recovery[0] = expected_romimage[0] = 0;

    uint32_t config_info_offset;
    char rom_id_str[17];
    rom_id_str[0] = '\0';

    int match_pass;
    int num_matches = 0;
    int best_match_count = 0;
    string machine_name = "";

    // set this to false if you want to print all info only when there's no ROM match
    bool print_all_info = true;

    /* read firmware version from file */
    date = READ_DWORD_BE_A(&rom_data[8]);
    nw_config_signature = READ_WORD_BE_A(&rom_data[0x3f00]);
    has_nw_config = nw_config_signature == 0xc99c || nw_config_signature == 0xc03c;
    if (has_nw_config || (date > 0x19990000 && date < 0x20060000)) {
        is_nw = true;
        firmware_version = READ_DWORD_BE_A(&rom_data[4]);
        {
            nw_recovery_checksum_calculated = adler32(&rom_data[0x8000], 0x77ffc);
            nw_recovery_checksum_stored = READ_DWORD_BE_A(&rom_data[0x7fffc]);
            nw_romimage_checksum_calculated = adler32(&rom_data[0x80000], 0x7fffc);
            nw_romimage_checksum_stored = READ_DWORD_BE_A(&rom_data[0xffffc]);
        }

        if (has_nw_config) {
            nw_start_checksum_calculated = adler32(&rom_data[0], 0x3efc);
            nw_start_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3efc]);
            nw_config_checksum_calculated = adler32(&rom_data[0x3f00], 0x7c);
            nw_config_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3f7c]);
            nw_subconfig_checksum_calculated = adler32(&rom_data[0x3f0c], 0x70);
            nw_product_id = (READ_WORD_BE_A(&rom_data[0x3f02]) << 8) | rom_data[0x3f13];
        }
        else {
            firmware_version &= 0xffff; // the upper 2 bytes might be a machine type: 0=iMac, 1=PowerMac, 2=PowerBook
            nw_start_checksum_calculated = adler32(&rom_data[0], 0x3ffc);
            nw_start_checksum_stored = READ_DWORD_BE_A(&rom_data[0x3ffc]);
            nw_config_checksum_calculated = 0;
            nw_config_checksum_stored = 0;
            nw_subconfig_checksum_calculated = 0;
            nw_product_id = 0;
        }
        if (nw_start_checksum_calculated != nw_start_checksum_stored)
            snprintf(expected_start, sizeof(expected_start), " (expected 0x%04x)", nw_start_checksum_stored);
        if (nw_config_checksum_calculated != nw_config_checksum_stored)
            snprintf(expected_config, sizeof(expected_config), " (expected 0x%04x)", nw_config_checksum_stored);
        if (nw_recovery_checksum_calculated != nw_recovery_checksum_stored)
            snprintf(expected_recovery, sizeof(expected_recovery), " (expected 0x%04x)", nw_recovery_checksum_stored);
        if (nw_romimage_checksum_calculated != nw_romimage_checksum_stored)
            snprintf(expected_romimage, sizeof(expected_romimage), " (expected 0x%04x)", nw_romimage_checksum_stored);
    }
    else {
        date = 0;
        major_version = READ_WORD_BE_A(&rom_data[8]);
        minor_version = 0;
        if (uint8_t(major_version) >= 0x7A)
            minor_version = READ_WORD_BE_A(&rom_data[0x12]);
        firmware_version = (major_version << 16) | minor_version;
        ow_checksum_calculated = oldworldchecksum(&rom_data[4], std::min(rom_size - 4, (size_t)0x2ffffc));
        ow_checksum_stored = READ_DWORD_BE_A(&rom_data[0]);
        if (ow_checksum_calculated != ow_checksum_stored)
            snprintf(expected_ow, sizeof(expected_ow), " (expected 0x%08x)", ow_checksum_stored);

        if (rom_size >= 0x400000) {
            /* read ConfigInfo offset from file */
            config_info_offset = READ_DWORD_BE_A(&rom_data[0x300080]);
            if (0x300000ULL + config_info_offset <= rom_size - 0x64 - 16) {
                ow_checksum64_calculated = oldworldchecksum64(&rom_data[0], rom_size, 0x300000 + config_info_offset);
                ow_checksum64_stored = READ_QWORD_BE_A(&rom_data[0x300020 + config_info_offset]);
                if (ow_checksum64_calculated != ow_checksum64_stored)
                    snprintf(expected_ow64, sizeof(expected_ow64), " (expected 0x%016llx)", ow_checksum64_stored);

                /* read ConfigInfo.BootstrapVersion field as C string */
                memcpy(rom_id_str, &rom_data[0x300064 + config_info_offset], 16);
                rom_id_str[16] = 0;
                for (int i = 0; i < 16; i++)
                    if (rom_id_str[i] < ' ' || rom_id_str[i] > '~')
                        rom_id_str[i] = '.';
            }
        }
    }


    for (match_pass = 0; match_pass < 2; match_pass++) {
        int match_index = 0;
        for (rom_info *info = &rom_identity[0]; info->firmware_size_k; info++) {
            if (
                (info->firmware_version && info->firmware_version == firmware_version) ||
                (info->nw_product_id    && info->nw_product_id    == nw_product_id   )
            ) {
                int match_count = 1
                    + (info->ow_expected_checksum
                        && info->ow_expected_checksum == ow_checksum_stored)
                    + (info->ow_expected_checksum
                        && info->ow_expected_checksum == ow_checksum_calculated)
                    + (info->ow_expected_checksum64
                        && info->ow_expected_checksum64 == ow_checksum64_stored)
                    + (info->ow_expected_checksum64
                        && info->ow_expected_checksum64 == ow_checksum64_calculated)
                    + (info->nw_subconfig_expected_checksum
                        && info->nw_subconfig_expected_checksum == nw_subconfig_checksum_calculated)
                    + (info->id_str && strcmp(rom_id_str, info->id_str) == 0)
                    ;

                if (!match_pass) {
                    if (match_count >= best_match_count) {
                        if (match_count > best_match_count) {
                            best_match_count = match_count;
                            num_matches = 0;
                        }
                        num_matches++;
                    }
                } else {
                    if (num_matches == 0) {
                        LOG_F(ERROR, "Unknown ROM");
                        print_all_info = true;
                        break;
                    }

                    if (match_count == best_match_count) {
                        match_index++;
                        LOG_F(INFO, "Found match (%d/%d):", match_index, num_matches);
                        if (info->rom_description)
                            LOG_F(INFO, "    ROM description: %s", info->rom_description);
                        if (info->dppc_description)
                            LOG_F(INFO, "    Machine identified from ROM: %s", info->dppc_description);
                        if (
                            info->nw_firmware_updater_name && info->nw_openfirmware_name &&
                            strcmp(info->nw_firmware_updater_name, info->nw_openfirmware_name) == 0
                        ) {
                            LOG_F(INFO, "    Code Name: %s", info->nw_firmware_updater_name);
                        } else {
                            if (info->nw_firmware_updater_name)
                                LOG_F(INFO, "    Code Name (from Firmware Updater): %s", info->nw_firmware_updater_name);
                            if (info->nw_openfirmware_name)
                                LOG_F(INFO, "    Code Name (from Open Firmware): %s", info->nw_openfirmware_name);
                        }
                        if (info->nw_product_id) {
                            LOG_F(INFO, "    Product ID: 0x%04x.%02x = %s%d,%d",
                                nw_product_id >> 8, nw_product_id & 0xff,
                                (nw_product_id >> 20) == 0 ? "PowerMac" :
                                (nw_product_id >> 20) == 1 ? "PowerBook" :
                                (nw_product_id >> 20) == 2 ? "PowerMac" :
                                (nw_product_id >> 20) == 3 ? "PowerBook" :
                                (nw_product_id >> 20) == 4 ? "RackMac" : "???",
                                (nw_product_id >> 14) & 31,
                                (nw_product_id >>  8) & 31
                            );
                        }
                        if (info->nw_subconfig_expected_checksum) {
                            LOG_F(INFO, "    Config Checksum: 0x%08x", nw_subconfig_checksum_calculated);
                        }
                        if (rom_size != info->firmware_size_k * 1024) {
                            LOG_F(ERROR, "    Unexpected ROM file size: %zu bytes. Expected size is %d %s.",
                                rom_size,
                                info->firmware_size_k & 0x3ff ?
                                    info->firmware_size_k :
                                    info->firmware_size_k / 1024,
                                info->firmware_size_k & 0x3ff ? "kiB" : "MiB"
                            );
                        }
                        if (info->dppc_machine) {
                            if (machine_name.empty()) {
                                machine_name = info->dppc_machine;
                            }
                        } else
                            LOG_F(ERROR, "    This ROM is not supported.");
                    }
                } // if match_pass
            } // if match
        } // for rom_info
    } // for match_pass

    if (print_all_info) {
        if (is_nw) {
            LOG_F(INFO, "Info from ROM:");
            LOG_F(INFO, "    ROM Date: %04x-%02x-%02x", date >> 16, (date >> 8) & 0xff, date & 0xff);
            if (firmware_version < 0xffff)
                LOG_F(INFO, "    ROM Version: %x.%03x", (firmware_version >> 12) & 15, firmware_version & 0xfff);
            else
                LOG_F(INFO, "    ROM Version: %x.%x.%03x",
                    firmware_version >> 16, (firmware_version >> 12) & 15, firmware_version & 0xfff);
            if (has_nw_config) {
                LOG_F(INFO, "    Product ID: 0x%04x.%02x 0x%08x = %s%d,%d",
                    nw_product_id >> 8, nw_product_id & 0xff,
                    nw_subconfig_checksum_calculated,
                    (nw_product_id >> 20) == 0 ? "PowerMac" :
                    (nw_product_id >> 20) == 1 ? "PowerBook" :
                    (nw_product_id >> 20) == 2 ? "PowerMac" :
                    (nw_product_id >> 20) == 3 ? "PowerBook" :
                    (nw_product_id >> 20) == 4 ? "RackMac" : "???",
                    (nw_product_id >> 14) & 31,
                    (nw_product_id >>  8) & 31
                );
            }
        } else {
            if (uint8_t(major_version) >= 0x7A)
                LOG_F(INFO, "    ROM Version: %04x.%04x", major_version, minor_version);
            else
                LOG_F(INFO, "    ROM Version: %04x", major_version);
            if (rom_id_str[0])
                LOG_F(INFO, "    ConfigInfo.BootstrapVersion: \"%s\"", rom_id_str);
        }
    }

    if (expected_ow[0] || expected_start[0] || expected_config[0] || expected_recovery[0] || expected_romimage[0])
        checksum_verbosity = loguru::Verbosity_ERROR;
    if (expected_ow64[0])
        checksum_verbosity2 = loguru::Verbosity_ERROR;

    if (print_all_info || checksum_verbosity != loguru::Verbosity_INFO ||
        checksum_verbosity2 != loguru::Verbosity_INFO
    ) {
        if (is_nw) {
            if (has_nw_config) {
                VLOG_F(checksum_verbosity, "    ROM Checksums: 0x%08x%s, 0x%08x%s, 0x%08x%s, 0x%08x%s",
                    nw_start_checksum_calculated, expected_start,
                    nw_config_checksum_calculated, expected_config,
                    nw_recovery_checksum_calculated, expected_recovery,
                    nw_romimage_checksum_calculated, expected_romimage
                );
            }
            else {
                VLOG_F(checksum_verbosity, "    ROM Checksums: 0x%08x%s, 0x%08x%s, 0x%08x%s",
                    nw_start_checksum_calculated, expected_start,
                    nw_recovery_checksum_calculated, expected_recovery,
                    nw_romimage_checksum_calculated, expected_romimage
                );
            }
        }
        else {
            VLOG_F(checksum_verbosity, "    ROM Checksum: 0x%08x%s",
                ow_checksum_calculated, expected_ow
            );
            if (ow_checksum64_calculated || expected_ow64[0])
                VLOG_F(checksum_verbosity2, "    ROM Checksum (64-bit): 0x%016llx%s",
                    ow_checksum64_calculated, expected_ow64
                );
        }
    }

    return machine_name;
}

/* Read ROM file content and transfer it to the dedicated ROM region */
int MachineFactory::load_boot_rom(char *rom_data, size_t rom_size) {
    if (rom_size != 0x400000 && rom_size != 0x100000) {
        LOG_F(ERROR, "Unexpected ROM File size: %zu bytes.", rom_size);
        return -1;
    }

    BootRom* boot_rom = dynamic_cast<BootRom*>(
        gMachineObj->get_comp_by_type(HWCompType::ROM));
    if (!boot_rom) {
        LOG_F(ERROR, "Could not locate ROM device!");
        return -1;
    }

    return boot_rom->set_data((uint8_t*)rom_data, (uint32_t)rom_size);
}

int MachineFactory::create_machine_for_id(string& id, char *rom_data, size_t rom_size, vector<std::string> &app_args) {
    if (MachineFactory::create(id, app_args) < 0) {
        return -1;
    }
    if (load_boot_rom(rom_data, rom_size) < 0) {
        return -1;
    }
    return 0;
}

GetSettingValueFunc MachineFactory::get_setting_value;
