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

/** BootRom emulation. */

#include <cpu/ppc/ppcemu.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/bootrom.h>
#include <machines/machinefactory.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

namespace loguru {
    enum : Verbosity {
        Verbosity_BOOTROM = loguru::Verbosity_INFO,
        Verbosity_FLASH = loguru::Verbosity_WARNING
    };
}

static const char* state_string(Flash::State state)
{
    switch (state) {
        case Flash::ReadMemory      : return "ReadMemory";
        case Flash::ReadAutoSelect  : return "ReadAutoSelect";
        case Flash::EraseSetup      : return "EraseSetup";
        case Flash::EraseWrite      : return "EraseWrite";
        case Flash::EraseVerify     : return "EraseVerify";
        case Flash::ProgramSetup    : return "ProgramSetup";
        case Flash::Program         : return "Program";
        case Flash::ProgramVerify   : return "ProgramVerify";
        case Flash::Reset           : return "Reset";
        default                     : return "Unknown";
    }
}

void FlashChip::set_controller(FlashController* c)
{
    this->controller = c;
}

Am28F020::Am28F020(const std::string &dev_name) : FlashChip(dev_name), HWComponent(dev_name)
{
    if (this->vendor_id == 0x01) {
        use_intel_hack = false;
        supports_embedded = true;
        supports_non_embedded = false;
    } else {
        use_intel_hack = true; // return 0xDB for flash chip index >= 8
        supports_embedded = false;
        supports_non_embedded = true;
    }
}

uint16_t Am28F020::read(uint32_t addr)
{
    uint16_t value;
    switch (this->state) {

        case Flash::Reset:
        case Flash::ReadMemory:
            value = controller->rom_read(this, addr);
            if (addr < 8 || addr >= 0x40000 - 8)
                LOG_F(FLASH, "%s: %s %06x = %02x",
                    this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
            break;

        case Flash::ReadAutoSelect:
            switch (addr) {
                case 0:
                    value = this->vendor_id;
                    LOG_F(FLASH, "%s: ReadAutoSelect vendor_id %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    break;
                case 1:
                    value = this->device_id;
                    if (this->unit_address >= 8 && this->use_intel_hack)
                        value = 0xDB;
                    LOG_F(FLASH, "%s: ReadAutoSelect device_id %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    break;
                default:
                    value = 0;
                    LOG_F(ERROR, "%s: ReadAutoSelect unexpected address %06x",
                        this->get_name_and_unit_address().c_str(), addr);
            }
            break;

        case Flash::EraseVerify:
            value = controller->rom_read(this, this->EA);
            if (this->EA < 8 || this->EA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: EraseVerify -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), this->EA, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::ProgramVerify:
            value = controller->rom_read(this, this->PA);
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: ProgramVerify -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), this->PA, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::EmbeddedEraseWrite:
            value = controller->rom_read(this, addr);
            LOG_F(FLASH, "%s: EmbeddedEraseWrite -> ReadMemory %06x = %02x",
                this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::EmbeddedProgram:
            value = controller->rom_read(this, this->PA);
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: EmbeddedProgram -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::ReadMemory;
            break;

        default:
            value = 0;
            LOG_F(ERROR, "%s: %s unexpected read  %06x = %02x",
                this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
    }

    return value;
}

void Am28F020::write(uint32_t addr, uint16_t value)
{
    switch (this->state) {
        case Flash::ReadMemory:
            switch (value) {

                case 0x00:
                    LOG_F(FLASH, "%s: ReadMemory -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;

                case 0x80:
                case 0x90:
                    LOG_F(FLASH, "%s: ReadMemory -> ReadAutoSelect %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadAutoSelect;
                    break;

                case 0x20:
                    if (this->supports_non_embedded) {
                        LOG_F(FLASH, "%s: ReadMemory -> EraseSetup %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EraseSetup;
                    } else
                        goto unexpected1;
                    break;

                case 0x40:
                    if (this->supports_non_embedded) {
                        if (this->PA < 8 || this->PA >= 0x40000 - 8)
                            LOG_F(FLASH, "%s: ReadMemory -> ProgramSetup %06x = %02x",
                                this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::ProgramSetup;
                    } else
                        goto unexpected1;
                    break;

                case 0xA0:
                    if (this->supports_non_embedded) {
                        this->EA = addr;
                        if (this->EA < 8 || this->EA >= 0x40000 - 8)
                            LOG_F(FLASH, "%s: ReadMemory -> EraseVerify %06x = %02x",
                                this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EraseVerify;
                    } else
                        goto unexpected1;
                    break;

                case 0x30:
                    if (this->supports_embedded) {
                        LOG_F(FLASH, "%s: ReadMemory -> EmbeddedEraseSetup %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EmbeddedEraseSetup;
                    } else
                        goto unexpected1;
                    break;

                case 0x10:
                case 0x50:
                    if (this->supports_embedded) {
                        if (this->PA < 8 || this->PA >= 0x40000 - 8)
                            LOG_F(FLASH, "%s: ReadMemory -> EmbeddedProgramSetup %06x = %02x",
                                this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EmbeddedProgramSetup;
                    } else
                        goto unexpected1;
                    break;

                case 0xFF:
                    LOG_F(FLASH, "%s: ReadMemory -> Reset %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::Reset; // Flash::ReadMemory
                    break;

                default:
                unexpected1:
                    LOG_F(ERROR, "%s: ReadMemory unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::ReadAutoSelect:
            switch (value) {
                case 0x00:
                    LOG_F(FLASH, "%s: ReadAutoSelect -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;
                default:
                    LOG_F(ERROR, "%s: ReadAutoSelect unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EraseSetup:
            switch (value) {
                case 0x20:
                    if (this->supports_non_embedded) {
                        LOG_F(FLASH, "%s: EraseSetup -> EraseWrite %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EraseWrite;
                        for (int i = 0; i < 0x100000; i++)
                            controller->rom_write(this, i, 0xFF);
                        break;
                    }
                    [[fallthrough]];
                default:
                    LOG_F(ERROR, "%s: EraseSetup unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EraseWrite:
            switch (value) {
                case 0xA0:
                    this->EA = addr;
                    if (this->EA < 8 || this->EA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: EraseWrite -> EraseVerify %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::EraseVerify;
                    break;
                default:
                    LOG_F(ERROR, "%s: EraseWrite unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EmbeddedEraseSetup:
            switch (value) {
                case 0x30:
                    if (this->supports_embedded) {
                        LOG_F(FLASH, "%s: EmbeddedEraseSetup -> EmbeddedEraseWrite %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                        this->state = Flash::EmbeddedEraseWrite;
                        for (int i = 0; i < 0x100000; i++)
                            controller->rom_write(this, i, 0xFF);
                        break;
                    }
                    [[fallthrough]];
                default:
                    LOG_F(ERROR, "%s: EmbeddedEraseSetup unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::ProgramSetup:
            this->PA = addr;
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: ProgramSetup -> Program %06x = %02x",
                    this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::Program;
            controller->rom_write(this, addr, value);
            break;

        case Flash::Program:
            switch (value) {
                case 0xC0:
                    if (this->PA < 8 || this->PA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: Program -> ProgramVerify %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ProgramVerify;
                    break;
                default:
                    LOG_F(ERROR, "%s: Program unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EmbeddedProgramSetup:
            this->PA = addr;
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: EmbeddedProgramSetup -> EmbeddedProgram %06x = %02x",
                    this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::EmbeddedProgram;
            controller->rom_write(this, addr, value);
            break;

        case Flash::Reset:
            switch (value) {
                case 0xFF:
                    LOG_F(FLASH, "%s: Reset -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;
                default:
                    LOG_F(ERROR, "%s: Reset unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        default:
            LOG_F(ERROR, "%s: %s unexpected write %06x = %02x",
                this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
    }
}

Mt28F008B1::Mt28F008B1(const std::string &dev_name) : FlashChip(dev_name), HWComponent(dev_name)
{
}

uint16_t Mt28F008B1::read(uint32_t addr)
{
    uint16_t value;
    switch (this->state) {

        case Flash::Reset:
        case Flash::ReadMemory:
            value = controller->rom_read(this, addr);
            if (addr < 8 || addr >= 0x40000 - 8)
                LOG_F(FLASH, "%s: %s %06x = %02x",
                    this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
            break;

        case Flash::ReadAutoSelect:
            switch (addr) {
                case 0:
                    value = this->vendor_id;
                    LOG_F(FLASH, "%s: ReadAutoSelect vendor_id %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    break;
                case 1:
                    value = this->device_id;
                    LOG_F(FLASH, "%s: ReadAutoSelect device_id %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    break;
                default:
                    value = 0;
                    LOG_F(ERROR, "%s: ReadAutoSelect unexpected address %06x",
                        this->get_name_and_unit_address().c_str(), addr);
            }
            break;

        case Flash::EraseVerify:
            value = controller->rom_read(this, this->EA);
            if (this->EA < 8 || this->EA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: EraseVerify -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), this->EA, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::ProgramVerify:
            value = controller->rom_read(this, this->PA);
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: ProgramVerify -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), this->PA, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::EmbeddedEraseWrite:
            value = controller->rom_read(this, addr);
            LOG_F(FLASH, "%s: EmbeddedEraseWrite -> ReadMemory %06x = %02x",
                this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::ReadMemory;
            break;

        case Flash::EmbeddedProgram:
            value = controller->rom_read(this, this->PA);
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: EmbeddedProgram -> ReadMemory %06x = %02x",
                    this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::ReadMemory;
            break;

        default:
            value = 0;
            LOG_F(ERROR, "%s: %s unexpected read  %06x = %02x",
                this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
    }

    return value;
}

void Mt28F008B1::write(uint32_t addr, uint16_t value)
{
    switch (this->state) {
        case Flash::ReadMemory:
            switch (value) {

                case 0x00:
                    LOG_F(FLASH, "%s: ReadMemory -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;

                case 0x80:
                case 0x90:
                    LOG_F(FLASH, "%s: ReadMemory -> ReadAutoSelect %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadAutoSelect;
                    break;

                case 0x20:
                    LOG_F(FLASH, "%s: ReadMemory -> EraseSetup %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::EraseSetup;
                    break;

                case 0x40:
                    if (this->PA < 8 || this->PA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: ReadMemory -> ProgramSetup %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ProgramSetup;
                    break;

                case 0xA0:
                    this->EA = addr;
                    if (this->EA < 8 || this->EA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: ReadMemory -> EraseVerify %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::EraseVerify;
                    break;

                case 0xFF:
                    LOG_F(FLASH, "%s: ReadMemory -> Reset %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::Reset; // Flash::ReadMemory
                    break;

                default:
                    LOG_F(ERROR, "%s: ReadMemory unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::ReadAutoSelect:
            switch (value) {
                case 0x00:
                    LOG_F(FLASH, "%s: ReadAutoSelect -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;
                default:
                    LOG_F(ERROR, "%s: ReadAutoSelect unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EraseSetup:
            switch (value) {
                case 0x20:
                    LOG_F(FLASH, "%s: EraseSetup -> EraseWrite %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::EraseWrite;
                    for (int i = 0; i < 0x100000; i++)
                        controller->rom_write(this, i, 0xFF);
                    break;
                default:
                    LOG_F(ERROR, "%s: EraseSetup unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::EraseWrite:
            switch (value) {
                case 0xA0:
                    this->EA = addr;
                    if (this->EA < 8 || this->EA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: EraseWrite -> EraseVerify %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::EraseVerify;
                    break;
                default:
                    LOG_F(ERROR, "%s: EraseWrite unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::ProgramSetup:
            this->PA = addr;
            if (this->PA < 8 || this->PA >= 0x40000 - 8)
                LOG_F(FLASH, "%s: ProgramSetup -> Program %06x = %02x",
                    this->get_name_and_unit_address().c_str(), addr, value);
            this->state = Flash::Program;
            controller->rom_write(this, addr, value);
            break;

        case Flash::Program:
            switch (value) {
                case 0xC0:
                    if (this->PA < 8 || this->PA >= 0x40000 - 8)
                        LOG_F(FLASH, "%s: Program -> ProgramVerify %06x = %02x",
                            this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ProgramVerify;
                    break;
                default:
                    LOG_F(ERROR, "%s: Program unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        case Flash::Reset:
            switch (value) {
                case 0xFF:
                    LOG_F(FLASH, "%s: Reset -> ReadMemory %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
                    this->state = Flash::ReadMemory;
                    break;
                default:
                    LOG_F(ERROR, "%s: Reset unexpected value %06x = %02x",
                        this->get_name_and_unit_address().c_str(), addr, value);
            }
            break;

        default:
            LOG_F(ERROR, "%s: %s unexpected write %06x = %02x",
                this->get_name_and_unit_address().c_str(), state_string(this->state), addr, value);
    }
}

BootRom::BootRom(const std::string &dev_name, uint32_t size)
    : MMIODevice(), FlashController(dev_name)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::ROM | HWCompType::FLASH_CTRL);

    this->rom_addr = this->unit_address;
    this->rom_size = size;
}

PostInitResultType BootRom::device_postinit() {
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
        (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    if (mem_ctrl) {
        // allocate ROM region
        this->rom_entry = mem_ctrl->add_rom_region(this->unit_address, rom_size, this);
        return PI_SUCCESS;
    }
    return PI_RETRY;
}

HWComponent* BootRom::add_device(int32_t unit_address, HWComponent *dev_obj, const std::string &name)
{
    HWComponent *result = HWComponent::add_device(unit_address, dev_obj, name);
    FlashChip *flash_chip = dynamic_cast<FlashChip*>(result);
    if (flash_chip)
        flash_chip->set_controller(this);
    return result;
}

int BootRom::set_data(const uint8_t* data, uint32_t size)
{
    if (size > this->rom_size) {
        LOG_F(ERROR, "%s: ROM source is larger than expected.", this->name.c_str());
        return -1;
    }
    if (size < this->rom_size)
        LOG_F(ERROR, "%s: ROM source is smaller than expected.", this->name.c_str());
    std::memcpy(this->rom_entry->mem_ptr + this->rom_size - size, data, size);
    return 0;
}

void BootRom::set_rom_write_enable(const bool enable)
{
    if (this->has_flash) {
        LOG_F(WARNING, "%s: ROM write %s", this->name.c_str(), enable ? "enabled" : "disabled");
        this->rom_entry->type = enable ? RT_MMIO : RT_ROM;
    }
}

void BootRom::identify_rom()
{
    MachineFactory::machine_name_from_rom((char *)get_data(), this->rom_size);
}

static uint8_t reverse_bits(uint8_t val)
{
    return uint8_t((val * 0x0202020202ULL & 0x010884422010ULL) % 1023);
}

uint32_t BootRomOW::read(uint32_t /*rgn_start*/, uint32_t offset, int size)
{
    uint32_t value;

    if (size != 4 || offset & 3) {
        value = 0;
        LOG_F(ERROR, "%s: read  unexpected size or offset @%06x.%c",
        this->name.c_str(), offset, SIZE_ARG(size));
        return value;
    }

    value = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t val = (uint8_t)dynamic_cast<FlashChip *>(
            this->children[((offset >> 18) & 8) + (offset & 7) + i].get())->read((offset & 0x1FFFFF) / 8);
        if (offset >= 0x200000)
            val = reverse_bits(val);
        value = (value << 8) | val;
    }
    if ((offset & 0x1fffff) < 64 || (offset & 0x1fffff) >= 0x200000 - 64)
        LOG_F(BOOTROM, "%s: read  ROM offset @%06x.%c = %0*x",
            this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);

    return value;
}

void BootRomOW::write(uint32_t /*rgn_start*/, uint32_t offset, uint32_t value, int size)
{
    if (size != 4 || offset & 3) {
        LOG_F(ERROR, "%s: write unexpected size or offset @%06x.%c = %0*x",
        this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        return;
    }

    if ((offset & 0x1fffff) < 64 || (offset & 0x1fffff) >= 0x200000 - 64)
        LOG_F(BOOTROM, "%s: write ROM offset @%06x.%c = %0*x",
            this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
    for (int i = 0; i < 4; i++) {
        uint8_t val = (value >> ((3 - i) * 8)) & 0xff;
        if (offset >= 0x200000)
            val = reverse_bits(val);
        dynamic_cast<FlashChip *>(
            this->children[((offset >> 18) & 8) + (offset & 7) + i].get())->write((offset & 0x1FFFFF) / 8, val);
    }
}

uint16_t BootRomOW::rom_read(FlashChip *chip, uint32_t addr)
{
    uint32_t index = (uint32_t)chip->get_unit_address();
    uint32_t rom_addr = ((index & 8) << 18) + addr * 8 + (index & 7);
    uint8_t value = this->get_data()[rom_addr];
    if (index & 8)
        value = reverse_bits(value);
    return value;
}

void BootRomOW::rom_write(FlashChip *chip, uint32_t addr, uint16_t value)
{
    uint32_t index = (uint32_t)chip->get_unit_address();
    uint32_t rom_addr = ((index & 8) << 18) + addr * 8 + (index & 7);
    if (index & 8)
        value = reverse_bits(uint8_t(value));
    this->get_data()[rom_addr] = uint8_t(value);
}

uint32_t BootRomNW::read(uint32_t /*rgn_start*/, uint32_t offset, int size)
{
    uint32_t value;

    if (size != 4 || offset & 3) {
        value = 0;
        LOG_F(ERROR, "%s: read  unexpected size or offset @%06x.%c",
        this->name.c_str(), offset, SIZE_ARG(size));
        return value;
    }

    switch (offset) {
    case 0:
    case 4:
        value = 0;
        for (int i = 0; i < 2; i++)
            value = (value << 16) | dynamic_cast<FlashChip *>(this->children[((offset >> 1) & 2) + i].get())->read(offset / 8);
        LOG_F(BOOTROM, "%s: read  ROM offset @%06x.%c = %0*x",
            this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    default:
        value = 0;
        LOG_F(WARNING, "%s: read  unknown ROM offset @%06x.%c",
            this->name.c_str(), offset, SIZE_ARG(size));
    }

    return value;
}

void BootRomNW::write(uint32_t /*rgn_start*/, uint32_t offset, uint32_t value, int size)
{
    if (size != 4 || offset & 3) {
        LOG_F(ERROR, "%s: write unexpected size or offset @%06x.%c = %0*x",
        this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        return;
    }

    switch (offset) {
    case 0:
    case 4:
        LOG_F(BOOTROM, "%s: write ROM offset @%06x.%c = %0*x",
            this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        for (int i = 0; i < 4; i++)
            dynamic_cast<FlashChip *>(
                this->children[((offset >> 1) & 2) + i].get())->write(offset / 8, (value >> ((1 - i) * 16)) & 0xffff);
        break;
    default:
        LOG_F(WARNING, "%s: write unknown ROM offset @%06x.%c = %0*x",
            this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
    }
}

uint16_t BootRomNW::rom_read(FlashChip *chip, uint32_t addr)
{
    uint32_t index = (uint32_t)chip->get_unit_address();
    uint32_t rom_addr = addr * 8 + (index & 3) * 2;
    return *(uint16_t*)(this->get_data() + rom_addr);
}

void BootRomNW::rom_write(FlashChip *chip, uint32_t addr, uint16_t value)
{
    uint32_t index = (uint32_t)chip->get_unit_address();
    uint32_t rom_addr = addr * 8 + (index & 3) * 2;
    *(uint16_t*)(this->get_data() + rom_addr) = value;
}

static const DeviceDescription Am28F020_Descriptor = {
    Am28F020::create, {}, {}, HWCompType::FLASH
};

REGISTER_DEVICE(Am28F020, Am28F020_Descriptor);

static const DeviceDescription BootRomOW_Descriptor = {
    BootRomOW::create, {
        "Am28F020@0", "Am28F020@1", "Am28F020@2", "Am28F020@3", "Am28F020@4", "Am28F020@5", "Am28F020@6", "Am28F020@7",
        "Am28F020@8", "Am28F020@9", "Am28F020@A", "Am28F020@B", "Am28F020@C", "Am28F020@D", "Am28F020@E", "Am28F020@F",
    }, {}, HWCompType::MMIO_DEV | HWCompType::ROM | HWCompType::FLASH_CTRL
};

static const DeviceDescription Mt28F008B1_Descriptor = {
    Mt28F008B1::create, {}, {}, HWCompType::FLASH
};

REGISTER_DEVICE(Mt28F008B1, Mt28F008B1_Descriptor);

static const DeviceDescription BootRomNW_Descriptor = {
    BootRomNW::create, {
        "Mt28F008B1@0"
    }, {}, HWCompType::MMIO_DEV | HWCompType::ROM | HWCompType::FLASH_CTRL
};

REGISTER_DEVICE(BootRomOW, BootRomOW_Descriptor);
REGISTER_DEVICE(BootRomNW, BootRomNW_Descriptor);
