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

/** @file Constructs the Gossamer machine.

    Author: Max Poliakovski
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/clockgen/athens.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>
#include <devices/common/i2c/i2cprom.h>
#include <devices/common/machineid.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <loguru.hpp>
#include <machines/machine.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <string>

static const std::map<int,PciIrqMap> grackle_irq_map = {
    {DEV_FUN(0x00,0), {nullptr    ,                  }}, // Grackle
    {DEV_FUN(0x0C,0), {"pci_PERCH", IntSrc::PCI_PERCH}},
    {DEV_FUN(0x0D,0), {"pci_A1"   , IntSrc::PCI_A    }},
    {DEV_FUN(0x0E,0), {"pci_B1"   , IntSrc::PCI_B    }},
    {DEV_FUN(0x0F,0), {"pci_C1"   , IntSrc::PCI_C    }},
    {DEV_FUN(0x10,0), {nullptr    ,                  }}, // Heathrow
    {DEV_FUN(0x12,0), {"pci_GPU"  , IntSrc::PCI_GPU  }},
};

// Bit definitions for the Gossamer system register at 0xFF000004
enum : uint16_t {
    FDC_TYPE_SWIM3  = (1 << 15), // Macintosh style floppy disk controller
    FDC_TYPE_MFDC   = (0 << 15), // PC style floppy disk controller
    BURST_ROM_FALSE = (1 << 14), // burst ROM not present
    BURST_ROM_TRUE  = (0 << 14), // burst ROM present
    PCI_C_PRSNT_POS = 12,        // PRSNT bits for the PCI Slot C
    PCI_B_PRSNT_POS = 10,        // PRSNT bits for the PCI Slot B
    PCI_A_PRSNT_POS = 8,         // PRSNT bits for the PCI Slot A
    PCM_PID_POS     = 5,         // Processor und Cache module ID
    PCM_PID_MASK    = 0xE0,
    AIO_PRSNT_FALSE = (1 << 4),  // Whisper/Wings style card in the PERCH slot
    AIO_PRSNT_TRUE  = (0 << 4),  // All-in-one style card in the PERCH slot
    BUS_SPEED_POS   = 1,         // bus speed code, see below
    BUS_SPEED_MASK  = 0x0E,
    UNKNOWN_BIT_0   = 1          // Don't know what this bit is for
};

// Gossamer bus speed frequency codes
enum : uint8_t {
    BUS_FREQ_75P00A = 0, // 75.00 MHz
    BUS_FREQ_70P00  = 1, // 70.00 MHz
    BUS_FREQ_78P75  = 2, // 78.75 MHz
    BUS_FREQ_TRI    = 3, // clock output tristated
    BUS_FREQ_75P00B = 4, // 75.00 MHz
    BUS_FREQ_60P00  = 5, // 60.00 MHz
    BUS_FREQ_66P82  = 6, // 66.82 MHz
    BUS_FREQ_83P00  = 7, // 83.00 MHz
};

// EEPROM ID content for a Whisper personality card.
const uint8_t WhisperID[16] = {
    0x0F, 0xAA, 0x55, 0xAA, 0x57, 0x68, 0x69, 0x73, 0x70, 0x65, 0x72, 0x00,
    0x00, 0x00, 0x00, 0x02
};

static void setup_ram_slot(const std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    SpdSdram168* ram_dimm = new SpdSdram168(i2c_addr);
    i2c_bus->add_device(i2c_addr, ram_dimm, name);
    ram_dimm->set_capacity(capacity_megs);
}

class MachineGossamer : public Machine {
public:
    MachineGossamer() : HWComponent("MachineGossamer") {}
    int initialize(const std::string &id);
};

int MachineGossamer::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine Gossamer...");

    // get pointer to the memory controller/PCI host bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("GrackleGossamer"));
    grackle_obj->set_irq_map(grackle_irq_map);

    // configure the Gossamer system register
    uint16_t sys_reg = FDC_TYPE_SWIM3 | BURST_ROM_TRUE
                        | (0x3F << PCI_A_PRSNT_POS) // pull up all PRSNT bits
                        | (1 << PCM_PID_POS) // CPU/Cache speed ratio = 2:1
                        | AIO_PRSNT_FALSE // this machine is not All-in-one
                        | (BUS_FREQ_66P82 << BUS_SPEED_POS) // set bus frequency
                        | UNKNOWN_BIT_0; // pull up bit 0

    GossamerID* machine_id = new GossamerID(sys_reg);
    grackle_obj->get_parent()->add_device(0xFF000000, machine_id);
    grackle_obj->add_mmio_region(0xFF000000, 4096, machine_id);

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    setup_ram_slot("RAM_DIMM_1", 0x57, GET_INT_PROP("rambank1_size"));
    setup_ram_slot("RAM_DIMM_2", 0x56, GET_INT_PROP("rambank2_size"));
    setup_ram_slot("RAM_DIMM_3", 0x55, GET_INT_PROP("rambank3_size"));

    // add pci devices
    grackle_obj->add_device(DEV_FUN(0x10,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    // add Athens clock generator device and register it with the I2C host
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->add_device(0x28, new AthensClocks(0x28));

    // create ID EEPROM for the Whisper personality card and register it with the I2C host
    I2CProm* perch_id = new I2CProm(0x53, 256);
    i2c_bus->add_device(0x53, perch_id);
    perch_id->fill_memory(0, 256, 0);
    perch_id->fill_memory(32, 223, 0xFF);
    perch_id->set_memory(0, WhisperID, sizeof(WhisperID));

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, timebase_freq);

    // set CPU PLL ratio to 3.5
    ppc_state.spr[SPR::HID1] = 0xE << 28;

    return 0;
}

static const PropMap gossamer_desktop_settings = {
    {"rambank1_size",
        new IntProperty(256, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"rambank2_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"rambank3_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"emmo",
        new BinProperty(0)},
    {"hdd_config",
        new StrProperty("Ide0/@0")},
    {"cdr_config",
        new StrProperty("Ide1/@0")},
    {"pci_GPU",
        new StrProperty("AtiRageGT")},
    {"pci_dev_max",
        new IntProperty(0xF, 0, 0x1F)},
};

static const PropMap gossamer_tower_settings = {
    {"rambank1_size",
        new IntProperty(256, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"rambank2_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"rambank3_size",
        new IntProperty(  0, std::vector<uint32_t>({0, 8, 16, 32, 64, 128, 256, 512}))},
    {"emmo",
        new BinProperty(0)},
    {"hdd_config",
        new StrProperty("Ide0/@0")},
    {"cdr_config",
        new StrProperty("Ide1/@0")},
    {"pci_GPU",
        new StrProperty("AtiRagePro")},
    {"pci_dev_max",
        new IntProperty(0xF, 0, 0x1F)},
};

static std::vector<std::string> pmg3_devices = {
    "GrackleGossamer@80000000", "ScreamerSnd@14000", "Heathrow@10"
};

static std::vector<std::string> pmg3twr_devices = {
    "GrackleGossamer@80000000", "ScreamerSnd@14000", "Heathrow@10"
};

static const DeviceDescription MachineGossamerDesktop_descriptor = {
    Machine::create<MachineGossamer>, pmg3_devices, gossamer_desktop_settings, HWCompType::MACHINE,
    "Power Macintosh G3 (Beige) Desktop"
};

static const DeviceDescription MachineGossamerTower_descriptor = {
    Machine::create<MachineGossamer>, pmg3twr_devices, gossamer_tower_settings, HWCompType::MACHINE,
    "Power Macintosh G3 (Beige) Tower"
};

REGISTER_DEVICE(pmg3dt, MachineGossamerDesktop_descriptor);
REGISTER_DEVICE(pmg3twr, MachineGossamerTower_descriptor);
