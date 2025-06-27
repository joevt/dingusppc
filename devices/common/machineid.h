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

#ifndef MACHINE_ID_H
#define MACHINE_ID_H

#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <debugger/symbolsopenfirmware.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/ofnvram.h>
#include <devices/ioctrl/macio.h>
#include <machines/machinefactory.h>

#include <cinttypes>
#include <loguru.hpp>
#include <string>

/**
    @file Contains definitions for Power Macintosh machine ID registers.

    The machine ID register is a memory-based register containing hardcoded
    values the system software can read to identify machine/board it's running on.

    Register location and value meaning are board-dependent.
 */

/**
    Machine ID register for NuBus Power Macs.
    It's located at physical address 0x5FFFFFFC and contains four bytes:
      +0 uint16_t signature = 0xA55A
      +1 uint8_t  machine_type (0x30 - Power Mac)
      +2 uint8_t  model (0x10 = PDM, 0x12 = Carl Sagan, 0x13 = Cold Fusion)
 */
class NubusMacID : public MMIODevice {
public:
    NubusMacID(const uint16_t id)
        : HWComponent("Nubus-Machine-id")
    {
        this->id[0] = 0xA5;
        this->id[1] = 0x5A;
        this->id[2] = (id >> 8) & 0xFF;
        this->id[3] = id & 0xFF;
        supports_types(HWCompType::MMIO_DEV);
    }
    ~NubusMacID() = default;

    uint32_t read(uint32_t /*rgn_start*/, uint32_t offset, int size) {
        //LOG_F(INFO, "NubusMacID: read size %d, offset %d", size, offset);

        if (offset == 0 && size == 1) {
            uint32_t phys_addr;
            int name_offset;
            mmu_translate_imem(ppc_state.pc, &phys_addr);
            std::string name = get_name_OpenFirmware(ppc_state.pc, phys_addr, &name_offset, false);
            if (!name.empty()) {
                if (GET_BIN_PROP("debug_copland")) {
                    power_on = false;
                    power_off_reason = po_enter_debugger;
                }

                uint32_t nv_ram_buffer_xtoken;
                LOG_F(INFO, "Searching for nv-ram-buffer");
                bool found = lookup_name_OpenFirmware("nv-ram-buffer", nv_ram_buffer_xtoken);
                if (found) {
                    uint32_t nv_ram_buffer_phys;
                    uint32_t nv_ram_buffer = (uint32_t)mem_read_dbg(nv_ram_buffer_xtoken + 8, 4);
                    mmu_translate_imem(nv_ram_buffer, &nv_ram_buffer_phys);
                    LOG_F(INFO, "nv-ram-buffer: %08x -> %08x", nv_ram_buffer, nv_ram_buffer_phys);

                    char nvram_name_unit_address[20];
                    snprintf(nvram_name_unit_address, sizeof(nvram_name_unit_address), "NVRAMCopland@%X", nv_ram_buffer_phys);
                    NVram* nvram = dynamic_cast<NVram*>(MachineFactory::create_device(
                        gMachineObj->get_comp_by_type(HWCompType::MACHINE), nvram_name_unit_address));
                    nvram->set_copland_nvram(nv_ram_buffer_phys);
                    OfConfigUtils::setenv_from_command_line();
                }
                else
                    LOG_F(INFO, "nv-ram-buffer not found");
            }
        }

        if (size == 4 && offset == 0) {
            return *(uint32_t*)this->id;
        }
        if (size == 1 && offset < 4) {
            return this->id[offset];
        }
        ABORT_F("NubusMacID: invalid read size %d, offset %d!", size, offset);
        return 0;
    }

    /* not writable */
    void write(uint32_t /*rgn_start*/, uint32_t /*offset*/, uint32_t /*value*/, int /*size*/) {}

private:
    uint8_t id[4];
};

namespace loguru {
    enum : Verbosity {
        Verbosity_BOARDREGISTERREAD = loguru::Verbosity_9,
    };
}

/**
    TNT-style machines and derivatives provide two board registers
    telling whether some particular piece of HW is installed or not.
    Both board registers are attached to the IOBus of the I/O controller.
    See machines/machinetnt.cpp for further details.
 **/
class BoardRegister : public IobusDevice {
public:
    BoardRegister(const std::string name, const uint16_t data)
        : HWComponent(name)
    {
        supports_types(HWCompType::IOBUS_DEV);
        this->data = data;
    }
    ~BoardRegister() = default;

    uint16_t iodev_read(uint32_t address) {
        if (address == 0) {
            LOG_F(BOARDREGISTERREAD, "%s: read  0x%02x = %04x", this->name.c_str(), address, this->data);
        } else {
            LOG_F(ERROR, "%s: read  0x%02x = %04x", this->name.c_str(), address, this->data);
        }
        return this->data;
    }

    // appears read-only to guest
    void iodev_write(uint32_t address, uint16_t value) {
        LOG_F(ERROR, "%s: write 0x%02x = %04x", this->name.c_str(), address, value);
    }

    void update_bits(const uint16_t val, const uint16_t mask) {
        this->data = (this->data & ~mask) | (val & mask);
    }

private:
    uint16_t    data;
};

/**
    The machine ID for the Gossamer board is accesible at 0xFF000004 (phys).
    It contains a 16-bit value revealing machine's capabilities like bus speed,
    ROM speed, I/O configuration etc.
    See machines/machinegossamer.cpp for further details.
 */
class GossamerID : public MMIODevice {
public:
    GossamerID(const uint16_t id) : HWComponent("Machine-id") {
        this->id = id;
        supports_types(HWCompType::MMIO_DEV);
    }
    ~GossamerID() = default;

    uint32_t read(uint32_t /*rgn_start*/, uint32_t offset, int size) {
        return ((offset == 4 && size == 2) ? this->id : 0);
    }

    /* not writable */
    void write(uint32_t /*rgn_start*/, uint32_t /*offset*/, uint32_t /*value*/, int /*size*/) {}

private:
    uint16_t id;
};

#endif // MACHINE_ID_H
