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

/** @file Construct the Yosemite machine (Power Macintosh G3 Blue & White). */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/dec21154.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static const std::map<int,PciIrqMap> grackle_irq_map = {
    {DEV_FUN(0x00,0), {nullptr  ,                }}, // Grackle
    {DEV_FUN(0x0D,0), {nullptr  ,                }}, // Dec21154Yosemite
    {DEV_FUN(0x10,0), {"pci_J12", IntSrc::PCI_J12}}, // GPU PCI slot, 66 MHz
};

// 33 MHz PCI devices behind the DEC21154 PCI-to-PCI bridge
static const std::map<int,PciIrqMap> pci_bridge_irq_map = {
    {DEV_FUN(0x00,0), {"pci_FireWire", IntSrc::FIREWIRE}},
    {DEV_FUN(0x01,0), {"pci_UltraATA", IntSrc::ATA     }},
    {DEV_FUN(0x02,0), {"pci_J11"     , IntSrc::PCI_J11 }},
    {DEV_FUN(0x03,0), {"pci_J10"     , IntSrc::PCI_J10 }},
    {DEV_FUN(0x04,0), {"pci_J9"      , IntSrc::PCI_J9  }},
    {DEV_FUN(0x05,0), {nullptr       ,                 }}, // Paddington
    {DEV_FUN(0x06,0), {"pci_USB"     , IntSrc::USB     }},
};

static void setup_ram_slot(const std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    SpdSdram168* ram_dimm = new SpdSdram168(i2c_addr);
    i2c_bus->add_device(i2c_addr, ram_dimm, name);
    ram_dimm->set_capacity(capacity_megs);
}

class MachineYosemite : public Machine {
public:
    MachineYosemite() : HWComponent("MachineYosemite") {}
    int initialize(const std::string &id);
};

int MachineYosemite::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine Yosemite...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("GrackleYosemite"));
    grackle_obj->set_irq_map(grackle_irq_map);

    // get pointer to the bridge of the secondary PCI bus
    DecPciBridge *sec_bridge = dynamic_cast<DecPciBridge*>(gMachineObj->get_comp_by_name("Dec21154Yosemite"));
    sec_bridge->set_irq_map(pci_bridge_irq_map);

    // 00:0D.0 PCI Bridge
    grackle_obj->add_device(DEV_FUN(0x0D,0), dynamic_cast<PCIBase*>(sec_bridge));

    // register CMD646U2 PCI Ultra ATA Controller
    sec_bridge->add_device(DEV_FUN(1,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("CmdAta")));

    sec_bridge->add_device(DEV_FUN(5,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Paddington")));

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    setup_ram_slot("RAM_DIMM_1", 0x50, GET_INT_PROP("rambank1_size"));
    setup_ram_slot("RAM_DIMM_2", 0x51, GET_INT_PROP("rambank2_size"));
    setup_ram_slot("RAM_DIMM_3", 0x52, GET_INT_PROP("rambank3_size"));
    setup_ram_slot("RAM_DIMM_4", 0x53, GET_INT_PROP("rambank4_size"));

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, timebase_freq);

    // set CPU PLL ratio to 3.5
    ppc_state.spr[SPR::HID1] = 0xE << 28;

    return 0;
}

static const PropMap yosemite_settings = {
    {"rambank1_size",
        new IntProperty(256, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"rambank2_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"rambank3_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"rambank4_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"emmo",
        new BinProperty(0)},
    {"hdd_config",
        new StrProperty("CmdAta0/@0")},
    {"cdr_config",
        new StrProperty("Ide0/@0")},
    {"pci_J12",
        new StrProperty("AtiMach64Gx")},
    {"pci_dev_max",
        new IntProperty(0xF, 0, 0x1F)},
};

static std::vector<std::string> yosemite_devices = {
    "GrackleYosemite@80000000",
    "Dec21154Yosemite@D",
    "CmdAta@1",
    "BurgundySnd@14000",
    "Paddington@5",
};

static const DeviceDescription MachineYosemite_descriptor = {
    Machine::create<MachineYosemite>, yosemite_devices, yosemite_settings, HWCompType::MACHINE,
    "Power Macintosh G3 Blue and White"
};

static const DeviceDescription MachineYikes_descriptor = {
    Machine::create<MachineYosemite>, yosemite_devices, yosemite_settings, HWCompType::MACHINE,
    "Power Macintosh G4 Yikes"
};

REGISTER_DEVICE(pmg3nw, MachineYosemite_descriptor);
REGISTER_DEVICE(pmyikes, MachineYikes_descriptor);
