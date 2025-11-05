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

/** @file Construct the PowerBook G3 Lombard machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static const std::map<int,PciIrqMap> grackle_irq_map = {
    {DEV_FUN(0x00,0), {nullptr      ,                    }}, // Grackle
    {DEV_FUN(0x0E,0), {"pci_USB"    , IntSrc::USB        }},
    {DEV_FUN(0x0F,0), {"pci_ZIVA"   , IntSrc::ZIVA       }}, // DVD Decoder
    {DEV_FUN(0x10,0), {nullptr      ,                    }}, // Heathrow
    {DEV_FUN(0x11,0), {"pci_E1"     , IntSrc::PCI_E      }}, // GPU
    {DEV_FUN(0x13,0), {"pci_CARDBUS", IntSrc::PCI_CARDBUS}}, // Texas Instruments PCI1211 CardBus Controller (PCMCIA)
};

static void setup_ram_slot(const std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    SpdSdram168* ram_dimm = new SpdSdram168(i2c_addr);
    i2c_bus->add_device(i2c_addr, ram_dimm, name);
    ram_dimm->set_capacity(capacity_megs);
}

class MachineLombard : public Machine {

public:

MachineLombard() : HWComponent("MachineLombard") {}

int initialize(const std::string &id)
{
    LOG_F(INFO, "Building machine Lombard...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("GrackleLombard"));
    grackle_obj->set_irq_map(grackle_irq_map);

    MacIoTwo* mio_obj = dynamic_cast<MacIoTwo*>(gMachineObj->get_comp_by_name("Heathrow"));
    mio_obj->set_media_bay_id(0x30); // hasATA

    grackle_obj->add_device(DEV_FUN(0x10,0), mio_obj);

    // configure RAM slots
    // First ram slot is enumerated twice for some reason, the second slot is never enumerated, so
    // make sure both slots have the same RAM.
    uint32_t bank_1_size = GET_INT_PROP("rambank1_size");
    uint32_t bank_2_size = GET_INT_PROP("rambank2_size");
    if (bank_1_size != bank_2_size)
        LOG_F(ERROR, "rambank1_size and rambank2_size should have equal size");
    setup_ram_slot("RAM_DIMM_1", 0x50, bank_1_size);
    setup_ram_slot("RAM_DIMM_2", 0x51, bank_2_size);

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
    {"rambank1_size", new IntProperty(128, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"rambank2_size", new IntProperty(128, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"emmo", new BinProperty(0)},
    {"hdd_config", new StrProperty("Ide0/@0")},
    {"cdr_config", new StrProperty("Ide1/@0")},
    {"pci_E1", new StrProperty("AtiMach64Gx")},
    {"pci_dev_max", new IntProperty(0xF, 0, 0x1F)},
};

static std::vector<std::string> lombard_devices = {
    "BootRomNW@FFF00000",
    "GrackleLombard@80000000", "ScreamerSnd@14000", "Heathrow@10"
};

static const DeviceDescription MachineLombard_descriptor = {
    Machine::create<MachineLombard>, lombard_devices, lombard_settings, HWCompType::MACHINE, "PowerBook G3 Lombard"
};

REGISTER_DEVICE(pbg3lb, MachineLombard_descriptor);
