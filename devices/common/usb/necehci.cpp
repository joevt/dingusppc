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

#include <devices/common/usb/necehci.h>
#include <devices/deviceregistry.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

necehci::necehci()
    : PCIDevice("NecEhci"), HWComponent("NecEhci")
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    // set up PCI configuration space header
    /* 00 */ this->vendor_id      = 0x1033; // NEC Corporation
    /* 02 */ this->device_id      = 0x00E0; // uPD72010x USB 2.0 Controller
    /* 04 */ // this->command     = 0x0000; // 0x0014 2:Bus Master, 4:Memory Write and Invalidate Enable
    /* 06 */ this->status         = 0x0210; // 4:Capabilities, 9:DEVSEL medium
    /* 08 */ this->class_rev      = (0x0C0320 << 8) | 0x02; // EHCI USB Controller
    /* 0C */ this->cache_ln_sz    = 0x08; // 8 DWORDS = 32 bytes
    /* 0D */ // this->lat_timer   = 0x20; // 32
    /* 0E */ // this->hdr_type    = 0x00;
    /* 0F */ // this->bist        = 0x00;
    /* 10 */
    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }
    /* 28 */ // this->cb_cis_ptr  = 0x00000000;
    /* 2C */ this->subsys_vndr    = 0x16B8; // Sonnet Technologies, Inc
    /* 2E */ this->subsys_id      = 0x0021;
    /* 30 */ // this->exp_rom_bar = 0x00000000;
    /* 34 */ this->cap_ptr        = 0x40;
    /* 35 */ // reserved          = 0x00;
    /* 36 */ // reserved          = 0x00;
    /* 37 */ // reserved          = 0x00;
    /* 38 */ // reserved          = 0x00000000;
    /* 3C */ this->irq_line       = 0x00; // IRQ 0
    /* 3D */ this->irq_pin        = 0x01; // 01=pin A
    /* 3E */ this->min_gnt        = 0x10;
    /* 3F */ this->max_lat        = 0x22;
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };
}

void necehci::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void necehci::notify_bar_change(int bar_num)
{
    switch (bar_num) {
        case 0: change_one_bar(this->aperture_base[bar_num], aperture_size[bar_num], this->bars[bar_num] & ~15, bar_num); break;
    }
}

uint32_t necehci::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
{
    if (reg_offs < 64) {
        uint32_t value = PCIDevice::pci_cfg_read(reg_offs, details);
        return value;
    }

    switch (reg_offs) {
        case 0x40: return 0x7E020001;
            // +0: 01 = PCI Power Management
            // +1: 00 = next capability
            // +2: 7E02 = 01111 1 1 000 0 0 0 010
            //          : Power Management version 2; Flags: PMEClk- DSI- D1+ D2+ AuxCurrent=0mA PME(D0+,D1+,D2+,D3hot+,D3cold-)
        case 0x60: return 0x003F2020;
        case 0xE0: return 0xC4303305;
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void necehci::pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details)
{
    if (reg_offs < 64) {
        if (reg_offs >= 4 && reg_offs < 8) {
            LOG_WRITE_NAMED_CONFIG_REGISTER("command/status");
        }
        else {
            LOG_WRITE_NAMED_CONFIG_REGISTER("        config");
        }
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }

    switch (reg_offs) {
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

uint32_t necehci::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < aperture_size[0]) {
        LOG_F(
            WARNING, "%s: read  aperture_base[0] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
    }
    else {
        LOG_F(
            WARNING, "%s: read  unknown aperture %08x @%08x.%c", this->name.c_str(), rgn_start, offset,
            SIZE_ARG(size)
        );
    }

    return 0;
}

void necehci::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < aperture_size[0]) {
        LOG_F(
            WARNING, "%s: write aperture_base[0] @%08x.%c = %0*x", this->name.c_str(), offset,
            SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
        );
    }
    else {
        LOG_F(
            WARNING, "%s: write unknown aperture %08x @%08x.%c = %0*x", this->name.c_str(), rgn_start, offset,
            SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
        );
    }
}

static const DeviceDescription necehci_Descriptor = {
    necehci::create, {}, {}, HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(NecEhci, necehci_Descriptor);
