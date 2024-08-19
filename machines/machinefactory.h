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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#ifndef MACHINE_FACTORY_H
#define MACHINE_FACTORY_H

#include <machines/machineproperties.h>

#include <map>
#include <string>
#include <vector>

using namespace std;

struct DeviceDescription;

extern const map<string, tuple<int, string>> gPropHelp;
extern map<string, string> gMachineFactorySettings;

class MachineFactory
{
public:
    MachineFactory() = delete;

    static size_t read_boot_rom(string& rom_filepath, char *rom_data);
    static string machine_name_from_rom(char *rom_data, size_t rom_size);

    static int create(string& mach_id);
    static int create_machine_for_id(string& id, char *rom_data, size_t rom_size);

    static void get_device_settings(const string &dev_name, DeviceDescription& dev, map<string, string> &settings = gMachineFactorySettings);
    static int get_machine_settings(const string& id, map<string, string> &settings = gMachineFactorySettings);
    static void set_machine_settings(map<string, string> &settings = gMachineFactorySettings);

    static void list_machines();
    static void list_properties(vector<string> machine_list);

private:
    static void create_device(string& dev_name, DeviceDescription& dev);
    static void print_settings(PropMap& p, int type, int indent, string path);
    static void list_device_settings(DeviceDescription& dev, int type, int indent, string path);
    static int  load_boot_rom(char *rom_data, size_t rom_size);
};

#endif /* MACHINE_FACTORY_H */
