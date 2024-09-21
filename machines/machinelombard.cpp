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

/** @file Construct the PowerBook G3 Lombard machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <devices/ioctrl/macio.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static void setup_ram_slot(const std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    SpdSdram168* ram_dimm = new SpdSdram168(i2c_addr);
    i2c_bus->add_device(i2c_addr, ram_dimm, name);
    ram_dimm->set_capacity(capacity_megs);
}

static const std::vector<PciIrqMap> grackle_irq_map = {
    {nullptr      , DEV_FUN(0x00,0),                    }, // Grackle
    {"pci_USB"    , DEV_FUN(0x0E,0), IntSrc::USB        },
    {"pci_ZIVA"   , DEV_FUN(0x0F,0), IntSrc::ZIVA       }, // DVD Decoder
    {nullptr      , DEV_FUN(0x10,0),                    }, // Heathrow
    {"pci_E1"     , DEV_FUN(0x11,0), IntSrc::PCI_E      }, // GPU
    {"pci_CARDBUS", DEV_FUN(0x13,0), IntSrc::PCI_CARDBUS}, // Texas Instruments PCI1211 CardBus Controller (PCMCIA)
};

class MachineLombard : virtual public HWComponent {

public:

MachineLombard() : HWComponent("MachineLombard") {}

static std::unique_ptr<HWComponent> create() {
    MachineLombard *machine = new MachineLombard();
    if (machine && 0 == machine->initialize_lombard())
        return std::unique_ptr<MachineLombard>(machine);
    if (machine)
        delete machine;
    return nullptr;
}

int initialize_lombard()
{
    LOG_F(INFO, "Building machine Lombard...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));
    grackle_obj->set_irq_map(grackle_irq_map);

    dynamic_cast<HeathrowIC*>(gMachineObj->get_comp_by_name("Heathrow"))->set_media_bay_id(0x30); // hasATA

    grackle_obj->add_device(DEV_FUN(0x10,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    grackle_obj->add_device(DEV_FUN(0x11,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("AtiMach64Gx")));

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    // First ram slot is enumerated twice for some reason, the second slot is never enumerated, so put half the ram in each slot.
    setup_ram_slot("RAM_DIMM_1", 0x50, GET_INT_PROP("rambank1_size") / 2);
    setup_ram_slot("RAM_DIMM_2", 0x51, GET_INT_PROP("rambank1_size") / 2);

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, timebase_freq);

    // set CPU PLL ratio to 3.5
    ppc_state.spr[SPR::HID1] = 0xE << 28;

    return 0;
}

};

static const PropMap lombard_settings = {
    {"rambank1_size",
        new IntProperty(128, vector<uint32_t>({32, 64, 128}))},
    {"emmo",
        new BinProperty(0)},
    {"pci_USB",
        new StrProperty("")},
    {"pci_ZIVA",
        new StrProperty("")},
    {"pci_E1",
        new StrProperty("")},
    {"pci_CARDBUS",
        new StrProperty("")},
    {"hdd_config",
        new StrProperty("Ide0:0")},
    {"cdr_config",
        new StrProperty("Ide1:0")},
};

static vector<string> lombard_devices = {
    "Grackle@80000000", "ScreamerSnd@14000", "Heathrow@10", "AtiMach64Gx@11", "AtaHardDisk", "AtapiCdrom"
};

static const DeviceDescription MachineLombard_descriptor = {
    MachineLombard::create, lombard_devices, lombard_settings, HWCompType::MACHINE, "PowerBook G3 Lombard"
};

REGISTER_DEVICE(pbg3lb, MachineLombard_descriptor);
