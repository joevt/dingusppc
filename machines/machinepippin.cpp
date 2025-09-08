/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

/** @file Bandai/Atmark Pippin emulation. */

/**
    Kudos to Keith Kaisershot @ blitter.net for his precious technical help!
*/

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/pcihost.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/aspen.h>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <loguru.hpp>

static std::map<int,PciIrqMap> aspen_irq_map = {
//  {DEV_FUN(0x0B,0), {nullptr , IntSrc::BANDIT1 }},
//  {DEV_FUN(0x0D,0), {"pci_A1", IntSrc::PCI_A   }},
    {DEV_FUN(0x0E,0), {"pci_B1", IntSrc::PIPPIN_E}},
    {DEV_FUN(0x0F,0), {"pci_C1", IntSrc::PIPPIN_F}},
    {DEV_FUN(0x10,0), {nullptr ,                 }}, // GrandCentral
};

class MachinePippin : public Machine {

public:

MachinePippin() : HWComponent("MachinePippin") {}

int initialize(const std::string &id)
{
    LOG_F(INFO, "Building machine Pippin...");

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("AspenPci1"));
    pci_host->set_irq_map(aspen_irq_map);

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->add_device(DEV_FUN(0x10,0),
        dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentralTnt")));

    // get (raw) pointer to the memory controller
    AspenCtrl* aspen_obj = dynamic_cast<AspenCtrl*>(gMachineObj->get_comp_by_name("Aspen"));

    // allocate ROM region
    if (!aspen_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM
    aspen_obj->insert_ram_dimm(0, GET_INT_PROP("rambank1_size")); // soldered onboard RAM
    aspen_obj->insert_ram_dimm(1, GET_INT_PROP("rambank2_size")); // soldered onboard RAM
    aspen_obj->insert_ram_dimm(2, GET_INT_PROP("rambank3_size")); // RAM expansion slot
    aspen_obj->insert_ram_dimm(3, GET_INT_PROP("rambank4_size")); // RAM expansion slot

    // init virtual CPU
    ppc_cpu_init(aspen_obj, PPC_VER::MPC603, false, 16500000ULL);

    return 0;
}

};

static const PropMap Pippin_settings = {
    {"rambank1_size",
        new IntProperty(4, std::vector<uint32_t>({4}))}, // fixed size
    {"rambank2_size",
        new IntProperty(1, std::vector<uint32_t>({1}))}, // fixed size
    {"rambank3_size",
        new IntProperty(0, std::vector<uint32_t>({0, 1, 4, 8, 16}))},
    {"rambank4_size",
        new IntProperty(0, std::vector<uint32_t>({0, 1, 4, 8, 16}))},
    {"emmo",
        new BinProperty(0)},
    {"adb_devices",
        new StrProperty("AdbAppleJack,AdbKeyboard")},
    {"hdd_config",
        new StrProperty("")},
    {"cdr_config",
        new StrProperty("ScsiMesh/@3")},
    {"pci_dev_max",
        new IntProperty(0xF, 0, 0x1F)},
};

static std::vector<std::string> Pippin_devices = {
    "Aspen@F8000000", "AspenPci1@F2000000", "GrandCentralTnt@10", "TaosVideo@F0800000"
};

static const DeviceDescription MachinePippin_descriptor = {
    Machine::create<MachinePippin>, Pippin_devices, Pippin_settings, HWCompType::MACHINE,
    "Bandai Pippin"
};

REGISTER_DEVICE(pippin, MachinePippin_descriptor);
