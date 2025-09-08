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

/** @file Constructs a Catalyst (Power Macintosh 7200) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/machineid.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/platinum.h>
#include <loguru.hpp>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memctrl/memctrlbase.h>

#include <string>

static std::map<int,PciIrqMap> bandit1_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr , IntSrc::BANDIT1}},
    {DEV_FUN(0x0D,0), {"pci_A1", IntSrc::PCI_A  }},
    {DEV_FUN(0x0E,0), {"pci_B1", IntSrc::PCI_B  }},
    {DEV_FUN(0x0F,0), {"pci_C1", IntSrc::PCI_C  }},
    {DEV_FUN(0x10,0), {nullptr ,                }}, // GrandCentral
};

class MachineCatalyst : public Machine {

public:

MachineCatalyst() : HWComponent("MachineCatalyst") {}

int initialize(const std::string &id)
{
    LOG_F(INFO, "Building machine Catalyst...");

    PlatinumCtrl* platinum_obj;

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Bandit1"));
    pci_host->set_irq_map(bandit1_irq_map);

    // get (raw) pointer to the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentralCatalyst"));

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->add_device(DEV_FUN(0x10,0), gc_obj);

    // attach IOBus Device #1 0xF301A000
    gc_obj->add_device(0x1A000,
        new BoardRegister("BoardReg1",
            0x3F                                | // pull up all PRSNT bits
            ((GET_BIN_PROP("emmo") ^ 1) << 8)   | // factory tests (active low)
            (0 << 11)                           | // 2-bit box ID
            0xE000U                               // pull up unused bits
    ));

    // get (raw) pointer to the memory controller
    platinum_obj = dynamic_cast<PlatinumCtrl*>(gMachineObj->get_comp_by_name("Platinum"));

    // allocate ROM region
    if (!platinum_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // insert RAM DIMMs
    platinum_obj->insert_ram_dimm(0, GET_INT_PROP("rambank1_size") * DRAM_CAP_1MB);
    platinum_obj->insert_ram_dimm(1, GET_INT_PROP("rambank2_size") * DRAM_CAP_1MB);
    platinum_obj->insert_ram_dimm(2, GET_INT_PROP("rambank3_size") * DRAM_CAP_1MB);
    platinum_obj->insert_ram_dimm(3, GET_INT_PROP("rambank4_size") * DRAM_CAP_1MB);

    // allocate and map physical RAM
    platinum_obj->map_phys_ram();

    std::string cpu = GET_STR_PROP("cpu");
    if (cpu == "601") {
        // init virtual CPU and request MPC601
        ppc_cpu_init(platinum_obj, PPC_VER::MPC601, true, 7833600ULL);
    }
    else if (cpu == "750") {
        // configure CPU clocks
        uint64_t bus_freq      = 50000000ULL;
        uint64_t timebase_freq = bus_freq / 4;

        // initialize virtual CPU and request MPC750 CPU aka G3
        ppc_cpu_init(platinum_obj, PPC_VER::MPC750, false, timebase_freq);

        // set CPU PLL ratio to 3.5
        ppc_state.spr[SPR::HID1] = 0xE << 28;
    }

    return 0;
}

};

static const PropMap pm7200_settings = {
    {"rambank1_size",
        new IntProperty( 8, std::vector<uint32_t>({   4, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank3_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank4_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"emmo",
        new BinProperty(0)},
    {"cpu",
        new StrProperty("601", std::vector<std::string>({"601", "750"}))},
    {"hdd_config",
        new StrProperty("ScsiCurio/@0")},
    {"cdr_config",
        new StrProperty("ScsiCurio/@3")},
    {"pci_dev_max",
        new IntProperty(0xF, 0, 0x1F)},
};

static std::vector<std::string> pm7200_devices = {
    "Platinum@F8000000", "Bandit1@F2000000", "GrandCentralCatalyst@10"
};

static const DeviceDescription MachineCatalyst_descriptor = {
    Machine::create<MachineCatalyst>, pm7200_devices, pm7200_settings, HWCompType::MACHINE,
    "Power Macintosh 7200"
};

REGISTER_DEVICE(pm7200, MachineCatalyst_descriptor);
