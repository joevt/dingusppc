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

/** @file Constructs a TNT (Power Macintosh 7500, 8500 etc) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/machineid.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/hammerhead.h>
#include <loguru.hpp>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <string>

static std::map<int,PciIrqMap> bandit1_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr , IntSrc::BANDIT1}},
    {DEV_FUN(0x0D,0), {"pci_A1", IntSrc::PCI_A  }},
    {DEV_FUN(0x0E,0), {"pci_B1", IntSrc::PCI_B  }},
    {DEV_FUN(0x0F,0), {"pci_C1", IntSrc::PCI_C  }},
    {DEV_FUN(0x10,0), {nullptr ,                }}, // GrandCentral
};

static std::map<int,PciIrqMap> bandit2_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr , IntSrc::BANDIT2}},
    {DEV_FUN(0x0D,0), {"pci_D2", IntSrc::PCI_D  }},
    {DEV_FUN(0x0E,0), {"pci_E2", IntSrc::PCI_E  }},
    {DEV_FUN(0x0F,0), {"pci_F2", IntSrc::PCI_F  }},
};

static std::map<int,PciIrqMap> chaos_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr,  IntSrc::CONTROL}},
    {DEV_FUN(0x0D,0), {"vci_D",  IntSrc::PLANB  }},
    {DEV_FUN(0x0E,0), {"vci_E",  IntSrc::VCI    }},
};

static std::map<int,PciIrqMap> ans_bandit1_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr    , IntSrc::ERROR    }},
    {DEV_FUN(0x0D,0), {"pci_SLOT1", IntSrc::PCI_SLOT1}},
    {DEV_FUN(0x0E,0), {"pci_SLOT2", IntSrc::PCI_SLOT2}},
    {DEV_FUN(0x0F,0), {"pci_VIDEO",                  }},
    {DEV_FUN(0x10,0), {nullptr    ,                  }}, // GrandCentral
    {DEV_FUN(0x11,0), {"pci_FW0"  , IntSrc::PCI_FW0  }},
    {DEV_FUN(0x12,0), {"pci_FW1"  , IntSrc::PCI_FW1  }},
};

static std::map<int,PciIrqMap> ans_bandit2_irq_map = {
    {DEV_FUN(0x0B,0), {nullptr    , IntSrc::ERROR    }},
    {DEV_FUN(0x0D,0), {"pci_SLOT3", IntSrc::PCI_SLOT3}},
    {DEV_FUN(0x0E,0), {"pci_SLOT4", IntSrc::PCI_SLOT4}},
    {DEV_FUN(0x0F,0), {"pci_SLOT5", IntSrc::PCI_SLOT5}},
    {DEV_FUN(0x10,0), {"pci_SLOT6", IntSrc::PCI_SLOT6}},
};

class MachineTnt : public Machine {
public:
    MachineTnt() : HWComponent("MachineTnt") {}
    int initialize(const std::string &id);
};

int MachineTnt::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine TNT...");

    bool is_tnt = id != "ans300" && id != "ans500" && id != "ans700";
    HammerheadCtrl* memctrl_obj;

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name(is_tnt ? "Bandit1" : "Bandit1Ans"));
    pci_host->set_irq_map(is_tnt ? bandit1_irq_map : ans_bandit1_irq_map);

    // get (raw) pointer to the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->add_device(DEV_FUN(0x10,0), gc_obj);

    // get video PCI controller object
    PCIHost *vci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name_optional("Chaos"));
    if (vci_host) {
        vci_host->set_irq_map(chaos_irq_map);
        // connect built-in video device to the VCI bus
        vci_host->add_device(DEV_FUN(0x0B,0),
            dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("ControlVideo")));
    }

    if (is_tnt) {
        // attach IOBus Device #1 0xF301A000
        gc_obj->add_device(0x1A000,
            new BoardRegister("BoardReg1",
                0x3F                                                                       | // pull up all PRSNT bits
                ((GET_BIN_PROP("emmo") ^ 1) << 8)                                          | // factory tests (active low)
                ((gMachineObj->get_comp_by_name_optional("Sixty6Video") == nullptr) << 13) | // composite video out (active low)
                ((gMachineObj->get_comp_by_name_optional("MeshTnt") != nullptr) << 14)     | // fast SCSI (active high)
                0x8000U                                                                      // pull up unused bits
            )
        );
    } else {
        // attach IOBus Device #1 0xF301A000
        gc_obj->add_device(0x1A000,
            new BoardRegister("BoardReg1",
                0x3F                                                                       | // pull up all PRSNT bits
                ((GET_BIN_PROP("emmo") ^ 1) << 8)                                          | // factory tests (active low)
                (GET_INT_PROP("box_id") << 11)                                             | // BoxId0 and BoxId1
                ((GET_BIN_PROP("keyswitch_service") ^ 1) << 13)                            | // Keyswitch Service (active low)
                ((GET_BIN_PROP("keyswitch_locked") ^ 1) << 14)                             | // Keyswitch Locked (active low)
                0x8000U                                                                      // pull up unused bits
            )
        );
    }

    PCIHost *pci2_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name_optional(is_tnt ? "Bandit2" : "Bandit2Ans"));
    if (pci2_host) {
        pci2_host->set_irq_map(is_tnt ? bandit2_irq_map : ans_bandit2_irq_map);
        if (is_tnt) {
            // attach IOBus Device #3 0xF301C000
            gc_obj->add_device(0x1C000,
                new BoardRegister("BoardReg2",
                    0x3F                                        | // pull up all PRSNT bits
                    0x8000U                                       // pull up unused bits
                )
            );
        } else {
            // attach IOBus Device #5 0xF301E000
            gc_obj->add_device(0x1E000,
                new BoardRegister("BoardReg2",
                    0x3F                                        | // pull up all PRSNT bits
                    (1 << 8)                                    | // FanFailDrive (active low)
                    (1 << 9)                                    | // FanFailProcessor (active low)
                    (1 << 10)                                   | // TempFailProcessor (active low)
                    (1 << 11)                                   | // TempWarnProcessor (active low)
                    (1 << 12)                                   | // FailPowSupplyLeft (active low)
                    (1 << 13)                                   | // FailPowSupplyRight (active low)
                    (1 << 14)                                   | // powSupplyHotLeft (active low)
                    (1 << 15)                                     // powSupplyHotRight (active low)
                )
            );
        }
    }

    // get (raw) pointer to the memory controller
    memctrl_obj = dynamic_cast<HammerheadCtrl*>(gMachineObj->get_comp_by_name("Hammerhead"));

    memctrl_obj->set_motherboard_id((vci_host ? Hammerhead::MBID_VCI0_PRESENT : 0) |
                                    (pci2_host ? Hammerhead::MBID_PCI2_PRESENT : 0));
    memctrl_obj->set_bus_speed(Hammerhead::BUS_SPEED_50_MHZ);

    // populate RAM banks from configuration properties
    for (int bank_num = 0; bank_num <= 12; bank_num++) {
        std::string bn = std::to_string(bank_num);
        int bank_size = GET_INT_PROP("rambank" + bn + "_size");
        memctrl_obj->insert_ram_dimm(bank_num, bank_size * DRAM_CAP_1MB);
    }

    // allocate and map physical RAM
    memctrl_obj->map_phys_ram();

    // init virtual CPU
    std::string cpu = GET_STR_PROP("cpu");
    if (cpu == "604e")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC604E, false, 12500000ULL);
    else if (cpu == "604")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC604, false, 12500000ULL);
    else if (cpu == "601")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC601, true, 7833600ULL);
    else if (cpu == "750") {
        // configure CPU clocks
        uint64_t bus_freq      = 50000000ULL;
        uint64_t timebase_freq = bus_freq / 4;

        // initialize virtual CPU and request MPC750 CPU aka G3
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC750, false, timebase_freq);

        // set CPU PLL ratio to 3.5
        ppc_state.spr[SPR::HID1] = 0xE << 28;
    }

    return 0;
}

// If this is templated, it hits a compiler bug in MSVC, so use #define instead.
#define static_const_tnt_common_settings(cpu) \
    {"rambank0_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank1_size" , new IntProperty(16, std::vector<uint32_t>({   4, 8, 16, 32, 64, 128}))}, \
    {"rambank2_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank3_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank4_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank5_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank6_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank7_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank8_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank9_size" , new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank10_size", new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank11_size", new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"rambank12_size", new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))}, \
    {"emmo", new BinProperty(0)}, \
    {"cpu", new StrProperty(# cpu, std::vector<std::string>({"601", "604", "604e", "750"}))}, \
    {"pci_dev_max", new IntProperty(0xF, 0, 0x1F)},

#define static_const_tnt_settings(cpu) \
static const PropMap tnt_settings_ ## cpu = { \
    static_const_tnt_common_settings(cpu) \
    {"hdd_config", new StrProperty("ScsiMesh/@0")}, \
    {"cdr_config", new StrProperty("ScsiMesh/@3")}, \
};

#define static_const_ans_settings(cpu, supplies) \
static const PropMap ans_settings_ ## cpu ## _ ## supplies = { \
    static_const_tnt_common_settings(cpu) \
    {"hdd_config", new StrProperty("ScsiCurio/@0")}, \
    {"cdr_config", new StrProperty("ScsiCurio/@3")}, \
    {"box_id", new IntProperty(1, std::vector<uint32_t>({0,1,2,3}))}, \
    {"keyswitch_service", new BinProperty(0)}, \
    {"keyswitch_locked", new BinProperty(0)}, \
    {"two_supplies", new BinProperty(supplies)}, \
};

static_const_tnt_settings(601)
static_const_tnt_settings(604)
static_const_tnt_settings(604e)
static_const_ans_settings(604,1)
static_const_ans_settings(604e,1)
static_const_ans_settings(604e,2)

static std::vector<std::string> pm7500_devices = {
    "BootRomOW@FFC00000",
    "Hammerhead@F8000000", "Bandit1@F2000000", "GrandCentralTnt@10", "Chaos@F0000000"
};

static std::vector<std::string> pm8500_devices = {
    "BootRomOW@FFC00000",
    "Hammerhead@F8000000", "Bandit1@F2000000", "GrandCentralTnt@10", "Chaos@F0000000",
    "Sixty6Video@1C000"
};

static std::vector<std::string> pm9500_devices = {
    "BootRomOW@FFC00000",
    "Hammerhead@F8000000", "Bandit1@F2000000", "GrandCentralTnt@10", "Bandit2@F4000000",
};

static std::vector<std::string> pmAns_devices = {
    "BootRomOW@FFC00000",
    "Hammerhead@F8000000", "Bandit1Ans@F2000000", "Bandit2Ans@F4000000",
};

static const DeviceDescription MachineTnt7300_descriptor = {
    Machine::create<MachineTnt>, pm7500_devices, tnt_settings_604e, HWCompType::MACHINE,
    "Power Macintosh 7300"
};

static const DeviceDescription MachineTnt7500_descriptor = {
    Machine::create<MachineTnt>, pm7500_devices, tnt_settings_601, HWCompType::MACHINE,
    "Power Macintosh 7500"
};

static const DeviceDescription MachineTnt8500_descriptor = {
    Machine::create<MachineTnt>, pm8500_devices, tnt_settings_604, HWCompType::MACHINE,
    "Power Macintosh 8500"
};

static const DeviceDescription MachineTnt9500_descriptor = {
    Machine::create<MachineTnt>, pm9500_devices, tnt_settings_604, HWCompType::MACHINE,
    "Power Macintosh 9500"
};

static const DeviceDescription MachineTnt7600_descriptor = {
    Machine::create<MachineTnt>, pm7500_devices, tnt_settings_604e, HWCompType::MACHINE,
    "Power Macintosh 7600"
};

static const DeviceDescription MachineTnt8600_descriptor = {
    Machine::create<MachineTnt>, pm8500_devices, tnt_settings_604e, HWCompType::MACHINE,
    "Power Macintosh 8600"
};

static const DeviceDescription MachineTnt9600_descriptor = {
    Machine::create<MachineTnt>, pm9500_devices, tnt_settings_604e, HWCompType::MACHINE,
    "Power Macintosh 9600"
};

static const DeviceDescription MachineAns500_descriptor = {
    Machine::create<MachineTnt>, pmAns_devices, ans_settings_604_1, HWCompType::MACHINE,
    "Apple Network Server 500"
};

static const DeviceDescription MachineAns700_descriptor = {
    Machine::create<MachineTnt>, pmAns_devices, ans_settings_604e_2, HWCompType::MACHINE,
    "Apple Network Server 700"
};

static const DeviceDescription MachineAns300_descriptor = {
    Machine::create<MachineTnt>, pmAns_devices, ans_settings_604e_1, HWCompType::MACHINE,
    "Apple Network Server 300"
};

REGISTER_DEVICE(pm7300, MachineTnt7300_descriptor);
REGISTER_DEVICE(pm7500, MachineTnt7500_descriptor);
REGISTER_DEVICE(pm8500, MachineTnt8500_descriptor);
REGISTER_DEVICE(pm9500, MachineTnt9500_descriptor);
REGISTER_DEVICE(pm7600, MachineTnt7600_descriptor);
REGISTER_DEVICE(pm8600, MachineTnt8600_descriptor);
REGISTER_DEVICE(pm9600, MachineTnt9600_descriptor);
REGISTER_DEVICE(ans500, MachineAns500_descriptor);
REGISTER_DEVICE(ans700, MachineAns700_descriptor);
REGISTER_DEVICE(ans300, MachineAns300_descriptor);
