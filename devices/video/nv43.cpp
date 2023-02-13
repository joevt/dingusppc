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

#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/deviceregistry.h>
#include <devices/video/nv43.h>
#include <devices/video/displayid.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>

/*
^([rw])_(\d) @([0-9a-f]{8})\.([bwl]) = ([0-9a-f]+)[ \t]*$
oneregop(\1, \2, \3, \4, \5)
*/

typedef enum
{
    r,
    w
} readwrite;

const uint32_t regsize_b = 1;
//const uint32_t regsize_w = 2;
const uint32_t regsize_l = 4;

#define flip_b(x) (x)
#define flip_w(x) ((((x) >> 8) & 255) | (((x) & 255) << 8))
#define flip_l(x) ((((x) >> 24) & 255) | (((x) >> 8) & 0x0ff00) | (((x) & 0x0ff00) << 8) | (((x) & 255) << 24))

typedef struct {
    readwrite rw;
    uint32_t aperture;
    uint32_t reg;
    uint32_t size;
    uint32_t value;
} regop;

const regop regops[] = {
    #define oneregop(rw, aperture, reg, size, value) { \
        rw, aperture, 0x ## reg, regsize_ ## size, flip_ ## size((uint32_t)0x ## value) \
    },
    #include "nv43reg.h"
    #undef oneregop
};

const int maxregop = sizeof(regops) / sizeof(regops[0]);

NV43::NV43(const std::string &dev_name)
    : PCIVideoCtrl(dev_name), HWComponent(dev_name)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL);

    this->vram_size = 256 << 20; // convert MBs to bytes

    // allocate video RAM
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // set up PCI configuration space header
    /* 00 */ this->vendor_id   = PCI_VENDOR_NVIDIA;
    /* 02 */ this->device_id   = 0x0221; // GeForce 6200
    /* 04 */ this->command     = 0x0000; // 9e00 =
    /* 06 */ this->status      = 0x02B0; // 0000  0 01 0  1 0 1 1  0 000
                                         // : Capabilities List, 66 MHz, Fast Back-to-Back, DevSel Speed: Medium
    /* 08 */ this->class_rev   = (0x030000 << 8) | 0xA1;
    /* 0C */ this->cache_ln_sz = 0; // 0 DWORDs = 0 bytes
//  /* 0D */ this->lat_timer   = 0x10; // from Sawtooth, this gets set to 0x20 on a Beige G3 anyway.
//  /* 0E */ this->hdr_type    = 0x00; // PCI endpoint device
//  /* 0F */ this->bist        = 0x00; // Built-in Self Test
    /* 10 */
    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }
//  /* 1C */ this->bars_cfg[3]
//  /* 20 */ this->bars_cfg[4]
//  /* 24 */ this->bars_cfg[5]
//  /* 28 */ this->cb_cis_ptr  = 0; // Cardbus CIS Pointer
    /* 2C */ this->subsys_vndr = PCI_VENDOR_NVIDIA; // from Sawtooth, 0x0000 for beige G3
    /* 2E */ this->subsys_id   = 0x004D; // from Sawtooth, 0x0000 for beige G3
//  /* 30 */ this->exp_rom_bar
    /* 34 */ this->cap_ptr     = 0x60;
//  /* 35,36,37 */ reserved
//  /* 38 */ reserved
//  /* 3C */ this->irq_line    = 0;
    /* 3D */ this->irq_pin     = 1;
    /* 3E */ this->min_gnt     = 5;
    /* 3F */ this->max_lat     = 1;
    this->finish_config_bars();

    this->pci_wr_cmd = [this](uint16_t val) {
        LOG_F(WARNING, "%s: write command %04x", this->name.c_str(), val);
        this->command = val;
    };

    this->pci_wr_stat = [this](uint16_t val) {
        LOG_F(WARNING, "%s: write status %04x", this->name.c_str(), val);
        this->status &= ~(0b1111100100000000 & val);
    };

    this->pci_rd_cmd = [this]() {
        LOG_F(WARNING, "%s: read command %04x", this->name.c_str(), this->command);
        return this->command;
    };

    this->pci_rd_stat = [this]() {
        LOG_F(WARNING, "%s: read status %04x", this->name.c_str(), this->status);
        return this->status;
    };

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };
}

PostInitResultType NV43::device_postinit()
{
    // initialize display identification
    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));
    return PI_SUCCESS;
}

void NV43::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void NV43::notify_bar_change(int bar_num)
{
    switch (bar_num) {
        case 0:
        case 1:
        case 2:
            change_one_bar(this->aperture_base[bar_num],
                this->aperture_size[bar_num], this->bars[bar_num] & ~15, bar_num);
            break;
    }
}

uint32_t NV43::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
{
    if (reg_offs < 64) {
        uint32_t value = PCIDevice::pci_cfg_read(reg_offs, details);
        if (reg_offs >= 4 && reg_offs < 8) {
            LOG_READ_NAMED_CONFIG_REGISTER("command/status");
        }
        else {
            LOG_READ_NAMED_CONFIG_REGISTER("        config");
        }
        return value;
    }

    switch (reg_offs) {
/*
0x40: 00000000
0x44: 02003000
0x48: 1702001f
0x4c: 00000000

0x50: 00000000
0x54: 01000000
0x58: ced62300
0x5c: 0f000000

0x98: 010440c1
*/
    case 0x60:
        return 0x00020001;
        // +0: 01 = PCI Power Management
        // +1: 00 = No next capability
        // +2: 0002 = version 2=PM1.1
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void NV43::pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details)
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

uint32_t NV43::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < this->aperture_size[0]) {
        if (regop_index < maxregop) {
            const regop *op = &regops[regop_index];
            if (op->rw == r && op->aperture == 0 && op->reg == offset && op->size == size) {
                regop_index++;
                return op->value;
            }
        }
        LOG_F(
            WARNING, "%s: read  aperture_base[0] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
        return 0;
    }

    if (rgn_start == this->aperture_base[1] && offset < this->aperture_size[1]) {
        LOG_F(
            WARNING, "%s: read  aperture_base[1] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
        return 0;
    }

    if (rgn_start == this->aperture_base[2] && offset < this->aperture_size[2]) {
        LOG_F(
            WARNING, "%s: read  aperture_base[2] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
        return 0;
    }
    
    return PCIBase::read(rgn_start, offset, size);
}

void NV43::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base[0] && offset < this->aperture_size[0]) {
        if (regop_index < maxregop) {
#if 0
            const regop *op = &regops[regop_index];
            if (op->rw == w && op->aperture == 0 && op->reg == offset && op->size == size) {
                regop_index++;
                return;
            }
#else
            return;
#endif
        }
        if (offset < 0x000c0000) {
            if (offset == 0 || offset == 0x000bfffc) {
                LOG_F(
                    WARNING, "%s: write aperture_base[0] @%08x.%c = %0*x between", this->name.c_str(), offset,
                    SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
                );
            }
        }
        else {
            LOG_F(
                WARNING, "%s: write aperture_base[0] @%08x.%c = %0*x", this->name.c_str(), offset,
                SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
            );
        }
    }
    else
    if (rgn_start == this->aperture_base[1] && offset < this->aperture_size[1]) {
        if (offset >= 0x20000 && offset < 0x98000) {
            if (offset == 0x20000 || offset == 0x97ffc) {
                LOG_F(
                    WARNING, "%s: write aperture_base[1] @%08x.%c = %0*x between", this->name.c_str(), offset,
                    SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
                );
            }
        }
        else {
            LOG_F(
                WARNING, "%s: write aperture_base[1] @%08x.%c = %0*x", this->name.c_str(), offset,
                SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
            );
        }
    }
    else
    if (rgn_start == this->aperture_base[2] && offset < this->aperture_size[2]) {
        LOG_F(
            WARNING, "%s: write aperture_base[2] @%08x.%c = %0*x", this->name.c_str(), offset,
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

static const PropMap Nv43_Properties = {
    {"rom", new StrProperty("NV43.bin")},
};

static const DeviceDescription Nv43_Descriptor = {
    NV43::create, {"Display@0"}, Nv43_Properties, HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL
};

REGISTER_DEVICE(Nv43, Nv43_Descriptor);
