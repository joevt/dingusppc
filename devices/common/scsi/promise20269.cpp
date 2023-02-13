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

#include <devices/common/scsi/promise20269.h>
#include <devices/deviceregistry.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

Promise20269::Promise20269(const std::string &dev_name)
    : PCIDevice(dev_name), HWComponent(dev_name)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    // set up PCI configuration space header
    /* 00 */ this->vendor_id      = 0x105A; // Promise Technology, Inc.
    /* 02 */ this->device_id      = 0x4D69; // 20269
    /* 04 */ this->command        = 0x0000; // 0x0004 2:Bus Master
    /* 06 */ this->status         = 0x0430; // 4:Capabilities, 5:66 MHz, 7:Fast back to back, 9:DEVSEL slow
    /* 08 */ this->class_rev      = (0x018085 << 8) | 0x02; // Mass storage controller
    /* 0C */ this->cache_ln_sz    = 0x08; // 8 DWORDS = 32 bytes
    /* 0D */ // this->lat_timer   = 0x20; // 32
    /* 0E */ // this->hdr_type    = 0x00;
    /* 0F */ // this->bist        = 0x00;
    /* 10 */
    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }
    /* 28 */ // this->cb_cis_ptr  = 0x00000000;
    /* 2C */ this->subsys_vndr    = 0x105A; // Promise Technology, Inc.
    /* 2E */ this->subsys_id      = 0xAD69;
    /* 30 */ // this->exp_rom_bar = 0x00000000;
    /* 34 */ this->cap_ptr        = 0x60;
    /* 35 */ // reserved          = 0x00;
    /* 36 */ // reserved          = 0x00;
    /* 37 */ // reserved          = 0x00;
    /* 38 */ // reserved          = 0x00000000;
    /* 3C */ this->irq_line       = 0x0E; // IRQ 14
    /* 3D */ this->irq_pin        = 0x01; // 01=pin A
    /* 3E */ this->min_gnt        = 0x04;
    /* 3F */ this->max_lat        = 0x12;
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };
}

void Promise20269::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void Promise20269::notify_bar_change(int bar_num)
{
    switch (bar_num) {
        case 5: change_one_bar(this->aperture_base[bar_num], aperture_size[bar_num], this->bars[bar_num] & ~15, bar_num); break;
    }
}

uint32_t Promise20269::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
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
        case 0x80: return 0x00309301;
        case 0x84: return 0x0000423E;
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void Promise20269::pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details)
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

int Promise20269::io_access_allowed(uint32_t offset) {
    for (int bar = 0; bar < 5; bar++ ) {
        if (offset >= aperture_base[bar] && offset <= aperture_size[bar]) {
            if (this->command & 1) {
                return bar;
            }
            LOG_F(WARNING, "%s: I/O space disabled in the command reg", this->name.c_str());
            return -1;
        }
    }
    return -1;
}

bool Promise20269::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    int bar = this->io_access_allowed(offset);
    if (bar < 0) {
        return false;
    }
    LOG_F(
        WARNING, "%s: read  aperture_base[%d] @%08x.%c", this->name.c_str(), bar, offset,
        SIZE_ARG(size)
    );
    *res = 0;
    return true;
}

bool Promise20269::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    int bar = this->io_access_allowed(offset);
    if (bar < 0) {
        return false;
    }
    LOG_F(
        WARNING, "%s: write aperture_base[%d] @%08x.%c = %0*x", this->name.c_str(), bar, offset,
        SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
    );
    return true;
}

uint32_t Promise20269::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->aperture_base[5] && offset < aperture_size[5]) {
        LOG_F(
            WARNING, "%s: read  aperture_base[5] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
        return 0;
    }

    return PCIBase::read(rgn_start, offset, size);
}

void Promise20269::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base[5] && offset < aperture_size[5]) {
        LOG_F(
            WARNING, "%s: write aperture_base[5] @%08x.%c = %0*x", this->name.c_str(), offset,
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

static const PropMap Promise20269_Properties = {
    {"rom", new StrProperty("sonnettempotrio.bin")},
};

static const DeviceDescription Promise20269_Descriptor = {
    Promise20269::create, {}, Promise20269_Properties, HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(Promise20269, Promise20269_Descriptor);
