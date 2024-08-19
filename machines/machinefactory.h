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
extern std::map<std::string, std::string> gMachineFactorySettings;

class MachineFactory
{
public:
    MachineFactory() = delete;

    static size_t read_boot_rom(std::string& rom_filepath, char *rom_data);
    static std::string machine_name_from_rom(char *rom_data, size_t rom_size);

    static int create(std::string& mach_id);
    static int create_machine_for_id(std::string& id, char *rom_data, size_t rom_size);

    static void register_device_settings(const std::string &name);
    static int  register_machine_settings(const std::string& id);

    static void list_machines();
    static void list_properties(std::vector<std::string> machine_list);

    static GetSettingValueFunc get_setting_value;

private:
    static HWComponent* create_device(HWComponent *parent, std::string dev_name, DeviceDescription& dev);
    static void print_settings(const PropMap& p, PropScope scope, int indent, std::string path, std::string device);
    static void list_device_settings(DeviceDescription& dev, PropScope scope, int indent, std::string path, std::string device);
    static int  load_boot_rom(char *rom_data, size_t rom_size);
    static void register_settings(const PropMap& p);
};

#endif /* MACHINE_FACTORY_H */
