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
#include <devices/video/nv47.h>
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
// const uint32_t regsize_w = 2;
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
    #include "nv47reg.h"
    #undef oneregop
};

const int maxregop = sizeof(regops) / sizeof(regops[0]);

// Human readable Nv47 HW register names for easier debugging.
static const std::map<uint16_t, std::string> nv47_reg_names = {
};

NV47::NV47(const std::string &dev_name)
    : PCIVideoCtrl(dev_name), HWComponent(dev_name)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL);

    this->vram_size = 256 << 20; // convert MBs to bytes

    // allocate video RAM
    this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

    // set up PCI configuration space header
    /* 00 */ this->vendor_id   = PCI_VENDOR_NVIDIA;
    /* 02 */ this->device_id   = 0x0092; // GeForce 7800 GT
    /* 04 */ this->command     = 0x0000; // 0004 = Bus Master
    /* 06 */ this->status      = 0x0010; // 0000  0 00 0  0 0 0 1  0 000 : Capabilities list, DevSel Speed: Fast
    /* 08 */ this->class_rev   = (0x030000 << 8) | 0xA1; // Display controller:VGA compatible controller:VGA controller
                                                         // ; revision A1
    /* 0C */ this->cache_ln_sz = 0x10; // from G5 16 DWORDs = 64 bytes -> 0x08 = 32 bytes in dingusppc
//  /* 0D */ this->lat_timer   = 0x00; // from G5 ; 32 in dingusppc
//  /* 0E */ this->hdr_type    = 0x00; // PCI endpoint device
//  /* 0F */ this->bist        = 0x00; // Built-in Self Test
    /* 10 */
    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }
//  /* 28 */ this->cb_cis_ptr  = 0; // Cardbus CIS Pointer
    /* 2C */ this->subsys_vndr = PCI_VENDOR_NVIDIA; // from G5
    /* 2E */ this->subsys_id   = 0x0052; // from G5
//  /* 30 */ this->exp_rom_bar
    /* 34 */ this->cap_ptr     = 0x60;
//  /* 35,36,37 */ reserved
//  /* 38 */ reserved
//  /* 3C */ this->irq_line    = 0;
    /* 3D */ this->irq_pin     = 1;
    /* 3E */ this->min_gnt     = 0;
    /* 3F */ this->max_lat     = 0;
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

PostInitResultType NV47::device_postinit()
{
    // initialize display identification
    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));
    return PI_SUCCESS;
}

void NV47::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void NV47::notify_bar_change(int bar_num)
{
    switch (bar_num) {
        case 0:
        case 1:
        case 3:
            change_one_bar(this->aperture_base[bar_num], this->aperture_size[bar_num], this->bars[bar_num] & ~15, bar_num);
            break;
        case 5:
            this->io_base = this->bars[bar_num] & ~3; break;
    }
}

uint32_t NV47::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
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
    // case 0x40: 00000000000000000000000000000000
    // case 0x50: 0100000000000000ced6230000000000

    case 0x60: return 0x00026801;
        // +0: 01 = PCI Power Management
        // +1: 68 = next capability
        // +2: 0002 = version 2=PM1.1

    case 0x68: return 0x00807805;
        // +0: 05 = MSI
        // +1: 78 = next capability
        // +2: 80 = Enable- Count=1/1 Maskable- 64bit+
        // +3: 00 = Address: 0000000000000000  Data: 0000

    case 0x78: return 0x00010010;
    case 0x7C: return 0x000004c0;
    case 0x80: return 0x00000810;
    case 0x84: return 0x00014d01;
    case 0x88: return 0x11010008;
        //  Capabilities: [78] Express (v1) Endpoint, MSI 00
        //      DevCap: MaxPayload 128 bytes, PhantFunc 0, Latency L0s <512ns, L1 <4us
        //          ExtTag- AttnBtn- AttnInd- PwrInd- RBE- FLReset- SlotPowerLimit 0.000W
        //      DevCtl: CorrErr- NonFatalErr- FatalErr- UnsupReq-
        //          RlxdOrd+ ExtTag- PhantFunc- AuxPwr- NoSnoop+
        //          MaxPayload 128 bytes, MaxReadReq 128 bytes
        //      DevSta: CorrErr- NonFatalErr- FatalErr- UnsupReq- AuxPwr- TransPend-
        //      LnkCap: Port #0, Speed 2.5GT/s, Width x16, ASPM L0s L1, Exit Latency L0s <1us, L1 <4us
        //          ClockPM- Surprise- LLActRep- BwNot- ASPMOptComp-
        //      LnkCtl: ASPM Disabled; RCB 128 bytes, Disabled- CommClk-
        //          ExtSynch- ClockPM- AutWidDis- BWInt- AutBWInt-
        //      LnkSta: Speed 2.5GT/s (ok), Width x16 (ok)
        //          TrErr- Train- SlotClk+ DLActive- BWMgmt- ABWMgmt-
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void NV47::pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details)
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

const char* NV47::get_reg_name(uint32_t reg_offset) {
    auto iter = nv47_reg_names.find(reg_offset & ~3);
    if (iter != nv47_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown Nv47 register";
    }
}

uint32_t NV47::read_reg(uint32_t offset, uint32_t size) {
    uint32_t res;

    // perform register-specific pre-read action
    switch (offset & ~3) {
    default:
        LOG_F(
            INFO,
            "%s: read I/O reg %s at 0x%X, size=%d, val=0x%X", this->name.c_str(),
            get_reg_name(offset),
            offset,
            size,
            read_mem(&this->mm_regs[offset], size));
    }

    // reading internal registers with necessary endian conversion
    res = read_mem(&this->mm_regs[offset], size);

    return res;
}

void NV47::write_reg(uint32_t offset, uint32_t value, uint32_t size)
{
    // writing internal registers with necessary endian conversion
    write_mem(&this->mm_regs[offset], value, size);

    // perform register-specific post-write action
    switch (offset & ~3) {
    default:
        LOG_F(
            INFO,
            "NV47: %s register at 0x%X set to 0x%X",
            get_reg_name(offset),
            offset & ~3,
            READ_DWORD_LE_A(&this->mm_regs[offset & ~3]));
    }
}


bool NV47::io_access_allowed(uint32_t offset) {
    if (offset >= this->io_base && offset < (this->io_base + this->aperture_size[5])) {
        if ((this->command & 1)) {
            return true;
        }
        LOG_F(WARNING, "NV47 I/O space disabled in the command reg");
    }
    return false;
}


bool NV47::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    *res = this->read_reg(offset - this->io_base, size);
    return true;
}


bool NV47::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    if (!this->io_access_allowed(offset)) {
        return false;
    }

    this->write_reg(offset - this->io_base, value, size);
    return true;
}


uint32_t NV47::read(uint32_t rgn_start, uint32_t offset, int size)
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

    if (rgn_start == this->aperture_base[3] && offset < this->aperture_size[3]) {
        LOG_F(
            WARNING, "%s: read  aperture_base[3] @%08x.%c", this->name.c_str(), offset,
            SIZE_ARG(size)
        );
        return 0;
    }

    return PCIBase::read(rgn_start, offset, size);
}

void NV47::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
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
    if (rgn_start == this->aperture_base[3] && offset < this->aperture_size[3]) {
        LOG_F(
            WARNING, "%s: write aperture_base[3] @%08x.%c = %0*x", this->name.c_str(), offset,
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

static const PropMap Nv47_Properties = {
    {"rom", new StrProperty("NV47.bin")},
};

static const DeviceDescription Nv47_Descriptor = {
    NV47::create, {"Display@0"}, Nv47_Properties, HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL
};

REGISTER_DEVICE(Nv47, Nv47_Descriptor);
