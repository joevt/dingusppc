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

#ifndef MACHINE_FACTORY_H
#define MACHINE_FACTORY_H

#include <machines/machineproperties.h>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <set>
#include <regex>

struct DeviceDescription;

typedef std::function<std::optional<std::string>(const std::string&)> GetSettingValueFunc;

typedef enum {
    PropertyDevice,
    PropertyMachine,
} PropScope;

typedef struct {
    PropScope property_scope;
    const std::string property_description;
} PropHelpItem;

extern const std::map<std::string, PropHelpItem> gPropHelp;

class ConfigStackItem {
public:
    enum ConfigStackItemType {
        HWC,
        HWC_WITH_UNIT_ADDRESS,
        BLOCK_BEGIN
    };

    ConfigStackItem(ConfigStackItemType dsi)
        : stack_item_type(dsi) {}
    ConfigStackItem(HWComponent* hwc)
        : stack_item_type(HWC), hwc(hwc) {}
    ConfigStackItem(HWComponent* hwc, int32_t unit_address)
        : stack_item_type(HWC_WITH_UNIT_ADDRESS), hwc(hwc), unit_address(unit_address) {}

    ~ConfigStackItem() = default;

    ConfigStackItemType stack_item_type;
    HWComponent*        hwc = nullptr;
    int32_t             unit_address = 0;
};

class MachineFactory
{
public:
    MachineFactory() = delete;

    static size_t read_boot_rom(std::string& rom_filepath, char *rom_data);
    static std::string machine_name_from_rom(char *rom_data, size_t rom_size);

    static int create(std::string& mach_id, std::vector<std::string> &app_args);
    static int create_machine_for_id(std::string& id, char *rom_data, size_t rom_size, std::vector<std::string> &app_args);
    static HWComponent* create_device(HWComponent *parent, std::string dev_name,
        HWCompType supported_types = HWCompType::UNKNOWN);

    static void register_device_settings(const std::string &name);
    static void summarize_machine_settings();
    static void summarize_device_settings();

    static void list_machines();
    static void list_properties(std::vector<std::string> machine_list);

    static GetSettingValueFunc get_setting_value;

    static std::regex path_re;

private:
    static void print_settings(const PropMap& p, PropScope scope, int indent,
        std::string path, std::string device, std::set<std::string> *properties);
    static void list_device_settings(DeviceDescription& dev, PropScope scope, int indent,
        std::string path, std::string device, std::set<std::string> *properties);
    static int  load_boot_rom(char *rom_data, size_t rom_size);
    static void register_settings(const std::string& dev_name, const PropMap& p);
    static HWComponent* set_property(const std::string &property, const std::string &value);
    static bool find_path(std::string path, HWComponent *&hwc, int32_t &unit_address, bool &is_leaf_match);
    static void apply_configs();

protected:
    static std::vector<ConfigStackItem> config_stack;
    static bool config_stack_ready;
};

#endif /* MACHINE_FACTORY_H */
