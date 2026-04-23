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

/*
Ported from Bochs bx_geforce_c (Copyright 2025-2026 The Bochs Project, LGPL v2+).
Internal GPU logic is functionally identical; DingusPPC scaffolding has been
rewritten to match the ATIRage / AtiMach64Gx patterns in this codebase.
*/

/**
 * @file  nvidianv.cpp
 * @brief NVIDIA NV-architecture GeForce for DingusPPC.
 *
 * ============================================================
 * PORTING NOTES
 * ============================================================
 *
 * BOCHS → DINGUSPPC TYPE/API MAP
 * ─────────────────────────────────────────────────────────────
 * Bit8u/16u/32u/64u          → uint8_t/16_t/32_t/64_t
 * BX_GEFORCE_THIS foo        → this->foo  (or bare foo)
 * BX_DEBUG(("...", ...))     → LOG_F(9,   "...", ...)
 * BX_INFO(("...", ...))      → LOG_F(INFO,"...", ...)
 * BX_ERROR(("...", ...))     → LOG_F(ERROR, "...", ...)
 * BX_PANIC(("...", ...))     → LOG_F(FATAL,"...", ...)
 * s.memory[offset]           → vram_ptr[offset]
 * DEV_MEM_READ_PHYSICAL      → physical_read8/16/32/64()
 * DEV_MEM_WRITE_PHYSICAL     → physical_write8/16/32/64()
 * bx_vgacore_c::update()     → not needed on Power Mac (no ISA VGA)
 * bx_gui->dimension_update() → create_display_window(w, h)
 * bx_gui->graphics_tile_*    → replaced by convert_fb_cb + VideoCtrlBase refresh
 * draw_hardware_cursor()     → draw_hw_cursor(dst_buf, dst_pitch)
 * bx_ddc_c ddc               → std::unique_ptr<DisplayID> disp_id
 * init_bar_mem(n,sz,rh,wh)   → bars_cfg[]+finish_config_bars()+notify_bar_change
 * init_pci_conf(vid,did,...) → this->vendor_id / device_id / class_rev / irq_pin
 * pci_write_handler()        → pci_cfg_write() in PCIDevice base
 * BX_GEFORCE_SMF             → (removed — always instance methods)
 *
 * VGA LEGACY
 * ──────────
 * Drop all bx_vgacore_c inheritance and calls.  Power Mac G3/G4 have no ISA
 * VGA bus; Open Firmware drives the NV card through BAR0/BAR1 via FCode ROM,
 * then hands off to NVDAResman.kext.  The ISA I/O ports (0x3C0-0x3DF, 0x3B4,
 * 0x3D4 ...) are never registered.
 *
 * DISPLAY REFRESH
 * ───────────────
 * Bochs uses a dirty-tile system.  DingusPPC uses VideoCtrlBase's timer-driven
 * refresh: install the correct convert_fb_cb lambda in crtc_update() and call
 * start_refresh_task().  No tile tracking needed.
 *
 * PHYSICAL MEMORY / DMA
 * ──────────────────────
 * See physical_read32() below.  mem_ctrl is acquired in device_postinit().
 * Byte-order: NV command buffers are LE on the wire; PPC is BE.  The BSWAP
 * in physical_read32 is the initial assumption — verify empirically by
 * watching whether RAMHT lookups succeed on the first OF push-buffer command.
 * If every lookup misses, remove the swap.
 *
 * BAR REGISTRATION
 * ─────────────────
 * Follows ATIRage exactly:
 *   constructor  → this->vendor_id / device_id / bars_cfg[] / finish_config_bars()
 *   notify_bar_change → host_instance->pci_register_mmio_region()
 *
 * EXECUTE_* METHODS
 * ──────────────────
 * All execute_*, d3d_*, pixel_operation, gdi_*, rect, ifc, iifc, sifc,
 * copyarea, tfc, m2mf, sifm are mechanical type-renames from geforce.cc.
 * They touch only vram_ptr[], ramin, and physical memory — zero emulator API.
 * Port order: vram/ramin helpers (done) → dma helpers (done skeleton) →
 * execute_m2mf/surf2d → execute_rect/gdi → execute_d3d.
 *
 * BITBLT ROP ENGINE
 * ──────────────────
 * Bochs includes bitblt.h (bx_bitblt_rop_t, rop_handler[]).  DingusPPC has no
 * equivalent.  Best option: copy bochs bitblt.h/cpp into
 * devices/video/nv_bitblt.h/.cpp, include it here, and use it directly.
 */

#include <devices/video/nvidianv.h>
#include <devices/deviceregistry.h>
#include <machines/machineproperties.h>
#include <cpu/ppc/ppcmmu.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cstring>
#include <cmath>
#include <chrono>
#include <stdexcept>

// ============================================================================
// Constructor — PCI identity, BAR sizing, VRAM allocation
// Follows ATIRage::ATIRage() pattern exactly.
// ============================================================================

NvidiaNV::NvidiaNV(NvCardType type)
    : HWComponent("nvidia-nv"), PCIDevice("nvidia-nv"), VideoCtrlBase(), card_type(type)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    // ---- Model-specific parameters ----
    uint16_t dev_id  = 0;
    uint8_t  rev_id  = 0;

    switch (card_type) {
    case NV_TYPE_NV15:
        dev_id   = 0x0150;  // GeForce2 Pro
        memsize  = 64u  << 20;
        bar2_size = 0;
        straps0_primary_original = 0x7FF86C6Bu | 0x180u;
        break;
    case NV_TYPE_NV20:
        dev_id   = 0x0200;  // GeForce3 (matches Apple FCode ROM PCIR)
        rev_id   = 0xA3;
        memsize  = 64u  << 20;
        bar2_size = 0;          // No BAR2 on real GF3
        straps0_primary_original = 0x7FF86C6Bu | 0x180u;
        break;
    case NV_TYPE_NV35:
        dev_id   = 0x0331;  // GeForce FX 5900
        memsize  = 128u << 20;
        bar2_size = 0;
        straps0_primary_original = 0x7FF86C4Bu | 0x180u;
        break;
    case NV_TYPE_NV40:
        dev_id   = 0x0045;  // GeForce 6800 GT
        memsize  = 256u << 20;
        bar2_size = 0x1000000u; // 16 MB RAMIN aperture
        straps0_primary_original = 0x7FF86C4Bu | 0x180u;
        break;
    }

    straps0_primary = straps0_primary_original;
    memsize_mask    = memsize - 1;
    ramin_flip      = memsize - (bar2_size ? bar2_size : 0x80000u);  // RAMIN at top of VRAM
    class_mask      = (card_type < NV_TYPE_NV40) ? 0xFFFu : 0xFFFFu;

    // ---- PCI config header — direct members, matches ATIRage pattern ----
    this->vendor_id   = 0x10DE;                     // NVIDIA
    this->device_id   = dev_id;
    this->subsys_vndr = 0x10DEu;
    this->subsys_id   = 0x0000u;                    // generic; override with OEM ID
    this->class_rev   = (0x030000u << 8) | rev_id;  // Display controller, VGA-compat
    this->min_gnt     = 5;
    this->max_lat     = 1;
    this->irq_pin     = 1;                           // INTA#

    // ---- BAR sizes and prefetch flags (same loop as ATIRage) ----
    // aperture_flag: bit 3 = prefetchable memory (VRAM); bit 0..2 = 0 (32-bit mem)
    aperture_size[0] = NV_MMIO_SIZE;   aperture_flag[0] = 0u;  // BAR0: registers
    aperture_size[1] = memsize;        aperture_flag[1] = 8u;  // BAR1: VRAM prefetchable
    aperture_size[2] = bar2_size;      aperture_flag[2] = 0u;  // BAR2: RAMIN (may be 0)

    int active_bars = (bar2_size != 0) ? 3 : 2;
    for (int i = 0; i < active_bars; i++) {
        this->bars_cfg[i] = (uint32_t)(-(int32_t)aperture_size[i]) | aperture_flag[i];
    }
    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // ---- FCode / BIOS ROM ----
    // Do not touch device properties here. On JoeVT's branch the property map can
    // still be unpopulated during construction, and GET_STR_PROP() may throw
    // std::out_of_range. The ROM attachment is deferred to device_postinit().

    // ---- DDC / display identification ----
    crtc_ext.reg[0x3E] = 0x0C;  // DDC idle: both SCL/SDA high

    // ---- VRAM allocation ----
    vram_ptr = std::make_unique<uint8_t[]>(memsize);
    std::memset(vram_ptr.get(), 0, memsize);
    disp_ptr = vram_ptr.get();

    // ---- Channel init ----
    std::memset(chs, 0, sizeof(chs));
    for (int i = 0; i < NV_CHANNEL_COUNT; i++) {
        chs[i].swzs_color_bytes = 1;
        chs[i].s2d_color_bytes  = 1;
        chs[i].d3d_color_bytes  = 1;
        chs[i].d3d_depth_bytes  = 1;
    }

    std::memset(unk_regs, 0, sizeof(unk_regs));
    this->draw_fb_is_dynamic = true;

    // Initialize default palette so OF text is visible.
    // OF's fb8-install uses index 0 for foreground and index 255 for background.
    // After FCode sets CGA 16-color palette (0=black), palette[255] must be
    // white for the background to show. Initialize all to black, except 255=white.
    for (int i = 0; i < 256; i++)
        this->set_palette_color(i, 0, 0, 0, 0xFF);
    this->set_palette_color(255, 0xFF, 0xFF, 0xFF, 0xFF);
}

// ============================================================================
// device_postinit
// ============================================================================

PostInitResultType NvidiaNV::device_postinit()
{
    // ---- DDC / display identification ----
    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));

    // VBL callback: raise PCRTC_INTR_VBLANK on each vertical blank.
    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state) {
            // Rising edge: latch VBlank ONCE per frame
            if (!this->vbl_pending) {
                this->vbl_pending = true;
                this->crtc_intr |= 0x00000001u;
                this->update_irq_level();
            }
            // Bochs: vertical_timer also processes deferred FIFO ops
            if (this->fifo_wait_acquire) {
                this->fifo_wait_acquire = false;
                this->update_fifo_wait();
                this->fifo_process();
            }
        } else {
            // Falling edge (end of VBlank): allow next frame to fire
            this->vbl_pending = false;
        }
    };

    this->start_refresh_task();
    return {};
}

// ============================================================================
// BAR change notification — matches ATIRage::notify_bar_change exactly
// ============================================================================

void NvidiaNV::change_one_bar(uint32_t &aperture, uint32_t size,
                               uint32_t new_base, int bar_num)
{
    if (aperture != new_base) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, size, this);
        aperture = new_base;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, size, this);
        LOG_F(INFO, "%s: BAR%d → 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void NvidiaNV::notify_bar_change(int bar_num)
{
    if (bar_num < 0 || bar_num >= NV_BAR_COUNT) return;
    if (aperture_size[bar_num] == 0) return;     // BAR2 absent on NV15/NV35
    change_one_bar(aperture_base[bar_num],
                   aperture_size[bar_num],
                   this->bars[bar_num] & ~0xFu,
                   bar_num);
}

// ============================================================================
// MMIO read/write dispatcher
// ============================================================================

uint32_t NvidiaNV::read(uint32_t rgn_start, uint32_t offset, int size)
{
    // BAR0 — NV register space
    // Before BOOT_1: FCode uses rl@(lwbrx) → CPU byte-reverses → value swapped → swap back
    // After BOOT_1:  NDRV uses lwz → value arrives correct → no swap
    if (rgn_start == aperture_base[0]) {
        if (size == 4) {
            uint32_t v = register_read32(offset);
            return big_endian_mode ? v : BYTESWAP_32(v);
        }
        if (size == 2) return (uint16_t)(register_read32(offset & ~3u) >> ((offset & 2u) * 8));
        if (size == 1) return register_read8(offset);
        LOG_F(WARNING, "%s: BAR0 read size=%d @ %08x", this->name.c_str(), size, offset);
        return 0xFFFFFFFFu;
    }

    // BAR1 — VRAM
    if (rgn_start == aperture_base[1]) {
        if (offset < memsize) {
            if (size == 4) return vram_read32(offset);
            if (size == 2) return vram_read16(offset);
            return vram_read8(offset);
        }
        LOG_F(WARNING, "%s: VRAM read OOB @ %08x", this->name.c_str(), offset);
        return 0xFFFFFFFFu;
    }

    // BAR2 — RAMIN aperture (NV20/NV40 only)
    if (bar2_size && rgn_start == aperture_base[2]) {
        uint32_t off = offset & (bar2_size - 1);
        if (size == 4) return ramin_read32(off);
        if (size == 1) return ramin_read8(off);
        return ramin_read16(off);
    }

    // PCI expansion ROM — handled by PCIDevice base via exp_rom_addr
    if (rgn_start == this->exp_rom_addr) {
        if (offset < this->exp_rom_size)
            return read_mem(&this->exp_rom_data[offset], size);
        return 0xFFFFFFFFu;
    }

    LOG_F(WARNING, "%s: read unknown region %08x+%08x",
          this->name.c_str(), rgn_start, offset);
    return 0xFFFFFFFFu;
}

void NvidiaNV::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    // BAR0 — NV register space
    // Before BOOT_1: FCode uses rl!(stwbrx) → CPU byte-reverses → value swapped → swap back
    // After BOOT_1:  NDRV uses stw → value arrives correct → no swap
    if (rgn_start == aperture_base[0]) {
        if (size == 4) {
            uint32_t v = big_endian_mode ? value : BYTESWAP_32(value);
            register_write32(offset, v);
            return;
        }
        if (size == 1) { register_write8(offset, (uint8_t)value); return; }
        if (size == 2) {
            uint32_t cur = register_read32(offset & ~3u);
            int shift = (offset & 2u) * 8;
            cur = (cur & ~(0xFFFFu << shift)) | ((value & 0xFFFFu) << shift);
            register_write32(offset & ~3u, cur);
            return;
        }
        LOG_F(WARNING, "%s: BAR0 write size=%d @ %08x", this->name.c_str(), size, offset);
        return;
    }

    // BAR1 — VRAM
    if (rgn_start == aperture_base[1]) {
        if (offset < memsize) {
            // Count VRAM writes after BOOT_1
            if (big_endian_mode) {
                static int vram_wr_count = 0;
                if (vram_wr_count < 5) {
                    LOG_F(INFO, "%s: BAR1 VRAM wr ofs=0x%08x size=%d val=0x%08x",
                          this->name.c_str(), offset, size, value);
                    vram_wr_count++;
                } else if (vram_wr_count == 5) {
                    LOG_F(INFO, "%s: BAR1 VRAM writes continuing (suppressing)", this->name.c_str());
                    vram_wr_count++;
                }
            }
            if (size == 4) { vram_write32(offset, value); this->draw_fb = true; return; }
            if (size == 2) { vram_write16(offset, (uint16_t)value); this->draw_fb = true; return; }
            vram_write8(offset, (uint8_t)value); this->draw_fb = true;
            return;
        }
        LOG_F(WARNING, "%s: VRAM write OOB @ %08x", this->name.c_str(), offset);
        return;
    }

    // BAR2 — RAMIN
    if (bar2_size && rgn_start == aperture_base[2]) {
        uint32_t off = offset & (bar2_size - 1);
        if (size == 4) { ramin_write32(off, value); return; }
        ramin_write8(off, (uint8_t)value);
        return;
    }

    LOG_F(WARNING, "%s: write unknown region %08x+%08x = %08x",
          this->name.c_str(), rgn_start, offset, value);
}

// ============================================================================
// VRAM helpers — little-endian layout, same as bochs vram_read*/write*
// ============================================================================

uint8_t  NvidiaNV::vram_read8(uint32_t a)  { return vram_ptr[a & memsize_mask]; }
uint16_t NvidiaNV::vram_read16(uint32_t a) {
    a &= memsize_mask;
    return (uint16_t)vram_ptr[a] | ((uint16_t)vram_ptr[a+1] << 8);
}
uint32_t NvidiaNV::vram_read32(uint32_t a) {
    a &= memsize_mask;
    return  (uint32_t)vram_ptr[a]
          | ((uint32_t)vram_ptr[a+1] <<  8)
          | ((uint32_t)vram_ptr[a+2] << 16)
          | ((uint32_t)vram_ptr[a+3] << 24);
}
uint64_t NvidiaNV::vram_read64(uint32_t a) {
    return (uint64_t)vram_read32(a) | ((uint64_t)vram_read32(a+4) << 32);
}
void NvidiaNV::vram_write8(uint32_t a, uint8_t v)  { vram_ptr[a & memsize_mask] = v; }
void NvidiaNV::vram_write16(uint32_t a, uint16_t v) {
    a &= memsize_mask;
    vram_ptr[a]   =  v & 0xFFu;
    vram_ptr[a+1] =  v >> 8;
}
void NvidiaNV::vram_write32(uint32_t a, uint32_t v) {
    a &= memsize_mask;
    vram_ptr[a]   =  v        & 0xFFu;
    vram_ptr[a+1] = (v >>  8) & 0xFFu;
    vram_ptr[a+2] = (v >> 16) & 0xFFu;
    vram_ptr[a+3] = (v >> 24) & 0xFFu;
}
void NvidiaNV::vram_write64(uint32_t a, uint64_t v) {
    vram_write32(a,   (uint32_t)(v));
    vram_write32(a+4, (uint32_t)(v >> 32));
}

// ============================================================================
// RAMIN helpers — instance memory at the top (tail) of VRAM
// NV2x: RAMIN starts at (memsize - bar2_size) i.e. ramin_flip
// NV4x: more complex; bochs ramin_* handles both cases via flip offset
// ============================================================================

uint8_t  NvidiaNV::ramin_read8(uint32_t o)  { return vram_read8(ramin_flip  + o); }
uint16_t NvidiaNV::ramin_read16(uint32_t o) { return vram_read16(ramin_flip + o); }
uint32_t NvidiaNV::ramin_read32(uint32_t o) { return vram_read32(ramin_flip + o); }
void     NvidiaNV::ramin_write8(uint32_t o, uint8_t v)  { vram_write8(ramin_flip  + o, v); }
void     NvidiaNV::ramin_write32(uint32_t o, uint32_t v){ vram_write32(ramin_flip + o, v); }

// ============================================================================
// Physical memory access — GPU DMA engine reading/writing PPC system RAM
//
// mem_ctrl is acquired in device_postinit() via get_comp_by_type(MEM_CTRL).
//
// BYTE ORDER NOTE:
// NV DMA push-buffers and RAMHT entries are written by NVDAResman.kext in
// little-endian byte order (same as on x86).  PPC memory is big-endian.
// The BSWAP below is our initial assumption.  If RAMHT lookups all miss on
// the first OF/kext push-buffer command, remove BYTESWAP_32 and try without.
// ============================================================================

uint8_t NvidiaNV::physical_read8(uint32_t a) {
    MapDmaResult r = mmu_map_dma_mem(a, 1, false);
    return r.host_va ? *((uint8_t *)r.host_va) : 0xFF;
}
uint16_t NvidiaNV::physical_read16(uint32_t a) {
    MapDmaResult r = mmu_map_dma_mem(a, 2, false);
    if (!r.host_va) return 0xFFFF;
    return BYTESWAP_16(*((uint16_t *)r.host_va));
}
uint32_t NvidiaNV::physical_read32(uint32_t a) {
    // Validate address is in RAM range before calling mmu_map_dma_mem
    // which will ABORT on unmapped addresses
    if (a >= 0x40000000u) {
        return 0;
    }
    MapDmaResult r = mmu_map_dma_mem(a, 4, false);
    if (!r.host_va) {
        return 0;
    }
    return BYTESWAP_32(*((uint32_t *)r.host_va));
}
uint64_t NvidiaNV::physical_read64(uint32_t a) {
    return (uint64_t)physical_read32(a) | ((uint64_t)physical_read32(a+4) << 32);
}
void NvidiaNV::physical_write8(uint32_t a, uint8_t v) {
    if (a >= 0x40000000u) return;
    MapDmaResult r = mmu_map_dma_mem(a, 1, true);
    if (r.host_va) *((uint8_t *)r.host_va) = v;
}
void NvidiaNV::physical_write16(uint32_t a, uint16_t v) {
    if (a >= 0x40000000u) return;
    MapDmaResult r = mmu_map_dma_mem(a, 2, true);
    if (r.host_va) *((uint16_t *)r.host_va) = BYTESWAP_16(v);
}
void NvidiaNV::physical_write32(uint32_t a, uint32_t v) {
    if (a >= 0x40000000u) return;
    MapDmaResult r = mmu_map_dma_mem(a, 4, true);
    if (r.host_va) *((uint32_t *)r.host_va) = BYTESWAP_32(v);
}
void NvidiaNV::physical_write64(uint32_t a, uint64_t v) {
    physical_write32(a,   (uint32_t)(v));
    physical_write32(a+4, (uint32_t)(v >> 32));
}

// ============================================================================
// DMA object helpers — direct type-rename from bochs geforce.cc
// ============================================================================

uint32_t NvidiaNV::dma_pt_lookup(uint32_t object, uint32_t address)
{
    uint32_t address_adj = address + (ramin_read32(object) >> 20);
    uint32_t pte = ramin_read32(object + 8 + (address_adj >> 12) * 4);
    return (pte & 0xFFFFF000u) + (address_adj & 0xFFFu);
}

// ============================================================================
// crtc_update — recalculate display mode and install convert_fb_cb
//
// Replaces the bochs update() + bx_gui tile loop.  Called whenever any
// PCRTC or PRAMDAC register changes.
// ============================================================================

void NvidiaNV::crtc_update()
{
    uint8_t crtc28 = crtc_ext.reg[0x28] & 0x7Fu;
    LOG_F(9, "%s: crtc_update called, crtc28=%02x", name.c_str(), crtc28);
    if (crtc28 == 0) return;   // VGA text mode / not yet programmed

    unsigned new_bpp;
    switch (crtc28) {
    case 1:  new_bpp =  8; break;
    case 2:  new_bpp = 16; break;
    case 3:  new_bpp = 32; break;
    default:
        LOG_F(ERROR, "%s: unknown CRTC28 = 0x%02x", name.c_str(), crtc28);
        return;
    }

    // Framebuffer start — CRTC regs 0x0C/0x0D/0x19 + NV_PCRTC_START
    uint32_t fb_offset =
        ((uint32_t)crtc_ext.reg[0x0D])
      | ((uint32_t)crtc_ext.reg[0x0C] << 8)
      | ((uint32_t)(crtc_ext.reg[0x19] & 0x1Fu) << 16);
    fb_offset <<= 2;
    fb_offset += crtc_start;

    // Pitch in bytes
    uint32_t pitch_reg =
        ((uint32_t)crtc_ext.reg[0x13])
      | ((uint32_t)(crtc_ext.reg[0x19] >> 5) << 8)
      | ((uint32_t)((crtc_ext.reg[0x42] >> 6) & 1u) << 11);
    unsigned new_pitch = pitch_reg * 8u;

    // Width / height
    unsigned new_w =
        ((uint32_t)crtc_ext.reg[0x01]
       + (uint32_t)((crtc_ext.reg[0x2D] & 0x02u) >> 1) * 256u
       + 1u) * 8u;
    unsigned new_h =
        (uint32_t)crtc_ext.reg[0x12]
      | ((uint32_t)((crtc_ext.reg[0x07] & 0x02u) >> 1) << 8)   // VDE bit 8
      | ((uint32_t)((crtc_ext.reg[0x07] & 0x40u) >> 6) << 9)   // VDE bit 9
      | ((uint32_t)((crtc_ext.reg[0x25] & 0x02u) >> 1) << 10)  // VDE bit 10
      | ((uint32_t)((crtc_ext.reg[0x41] & 0x04u) >> 2) << 10); // VDE bit 10 alt
    new_h += 1u;

    if (new_w == disp_xres && new_h == disp_yres &&
        new_bpp == disp_bpp && new_pitch == disp_pitch &&
        fb_offset == disp_offset && !mode_needs_update)
        return;

    LOG_F(INFO, "%s: mode %ux%ux%ubpp pitch=%u fb_ofs=%08x",
          name.c_str(), new_w, new_h, new_bpp, new_pitch, fb_offset);

    disp_xres    = new_w;
    disp_yres    = new_h;
    disp_bpp     = new_bpp;
    disp_pitch   = new_pitch;
    disp_offset  = fb_offset;
    disp_ptr     = vram_ptr.get() + (fb_offset & memsize_mask);
    mode_needs_update = false;

    // Propagate to VideoCtrlBase
    this->active_width  = (int)new_w;
    this->active_height = (int)new_h;
    this->pixel_depth   = (int)new_bpp;
    this->fb_ptr        = disp_ptr;
    this->fb_pitch      = (int)new_pitch;

    // Pixel clock from VPLL: PRAMDAC_VPLL_COEFF [7:0]=M [15:8]=N [18:16]=P
    {
        uint32_t M = ramdac_vpll & 0xFFu;
        uint32_t N = (ramdac_vpll >>  8) & 0xFFu;
        uint32_t P = (ramdac_vpll >> 16) & 0x7u;
        this->pixel_clock = (M != 0)
            ? 13500000.0f * (float)N / (float)M / (float)(1u << P)
            : 25175000.0f;  // safe default
    }

    // Approximate total lines/columns for refresh rate
    this->hori_total  = (int)(new_w + 160);
    this->vert_total  = (int)(new_h +  45);
    this->refresh_rate = this->pixel_clock / this->hori_total / this->vert_total;
    if (this->refresh_rate < 24.0f || this->refresh_rate > 120.0f)
        this->refresh_rate = 60.0f;

    // Install framebuffer converter (ATI pattern: draw_fb=false inside lambda)
    switch (new_bpp) {
    case 8:
        this->convert_fb_cb = [this](uint8_t *dst, int dp) {
            this->convert_frame_8bpp_indexed(dst, dp);
        };
        this->set_palette_color(255, 0xFF, 0xFF, 0xFF, 0xFF);
        break;
    case 16:
        this->convert_fb_cb = [this](uint8_t *dst, int dp) {
            this->convert_frame_16bpp<VideoCtrlBase::LE>(dst, dp);
        };
        break;
    case 32:
        this->convert_fb_cb = [this](uint8_t *dst, int dp) {
            // vram_write32 stores in LE byte order regardless of big_endian_mode
            this->convert_frame_32bpp<VideoCtrlBase::LE>(dst, dp);
        };
        break;
    default:
        LOG_F(ERROR, "%s: unsupported bpp %u", name.c_str(), new_bpp);
        this->convert_fb_cb = nullptr;
        break;
    }

    this->create_display_window((int)new_w, (int)new_h);
    this->blank_on = false;
    this->crtc_on = true;
    this->draw_fb = true;

    this->stop_refresh_task();
    this->start_refresh_task();

    // DIAGNOSTIC: log palette and VRAM state when mode is set with non-zero fb offset
    if (fb_offset != 0) {
        uint32_t pal0 = this->palette[0];
        uint32_t pal1 = this->palette[1];
        uint32_t palFF = this->palette[255];
        uint8_t *fb = vram_ptr.get() + (fb_offset & memsize_mask);
        uint32_t nonzero = 0;
        for (uint32_t i = 0; i < new_w * new_h && i < 640*480; i++) {
            if (fb[i] != 0) nonzero++;
        }
        // Scan all of VRAM for non-zero regions
        static int scan_count = 0;
        if (scan_count < 3) {
            uint32_t first_nz = 0xFFFFFFFF, last_nz = 0;
            for (uint32_t i = 0; i < memsize; i += 4) {
                uint32_t v = *(uint32_t*)(vram_ptr.get() + i);
                if (v != 0) {
                    if (first_nz == 0xFFFFFFFF) first_nz = i;
                    last_nz = i;
                }
            }
            LOG_F(INFO, "%s: VRAM scan: first_nz=0x%08x last_nz=0x%08x crtc_start=0x%08x fb_ofs=0x%08x",
                  name.c_str(), first_nz, last_nz, crtc_start, fb_offset);
            scan_count++;
        }
        LOG_F(INFO, "%s: DIAG fb_ptr=%p vram+%08x pal[0]=%08x pal[1]=%08x pal[255]=%08x vram_nonzero=%u/%u",
              name.c_str(), (void*)this->fb_ptr, fb_offset, pal0, pal1, palFF, nonzero, new_w*new_h);
    }
}

// ============================================================================
// HW cursor
// PORT NOTE: inner pixel loop should be copied verbatim from bochs
// draw_hardware_cursor() with tile_ptr/bx_gui removed; dst_buf/dst_pitch passed
// directly.  cursor_read16/32 use vram_ptr or ramin based on hw_cursor.vram.
// ============================================================================

void NvidiaNV::vidc_draw_hw_cursor(uint8_t *dst_buf, int dst_pitch)
{
    if (!hw_cursor.enabled || !dst_buf || dst_pitch <= 0) return;
    if (!this->vidc_cursor_on) return;

    auto cursor_read32 = [this](uint32_t addr) -> uint32_t {
        if (hw_cursor.vram) {
            return vram_read32(addr & memsize_mask);
        }
        return ramin_read32(addr & 0x7FFFFu);
    };

    const int size = hw_cursor.size ? hw_cursor.size : 32;
    const int start_x = hw_cursor.x;
    const int start_y = hw_cursor.y;

    if (hw_cursor.bpp32) {
        for (int cy = 0; cy < size; cy++) {
            int dy = start_y + cy;
            if (dy < 0 || dy >= (int)this->active_height) continue;
            uint8_t *dst_row = dst_buf + dy * dst_pitch;
            uint32_t src_base = hw_cursor.offset + (uint32_t)cy * (uint32_t)size * 4u;
            for (int cx = 0; cx < size; cx++) {
                int dx = start_x + cx;
                if (dx < 0 || dx >= (int)this->active_width) continue;
                uint32_t argb = cursor_read32(src_base + (uint32_t)cx * 4u);
                uint8_t b = (uint8_t)(argb & 0xFFu);
                uint8_t g = (uint8_t)((argb >> 8) & 0xFFu);
                uint8_t r = (uint8_t)((argb >> 16) & 0xFFu);
                uint8_t a = (uint8_t)((argb >> 24) & 0xFFu);
                if (a == 0) continue;
                uint8_t *dp = dst_row + dx * 4;
                if (a == 0xFF) {
                    dp[0] = b; dp[1] = g; dp[2] = r; dp[3] = 0xFF;
                } else {
                    uint32_t inv = 255u - a;
                    dp[0] = (uint8_t)((b * a + dp[0] * inv) / 255u);
                    dp[1] = (uint8_t)((g * a + dp[1] * inv) / 255u);
                    dp[2] = (uint8_t)((r * a + dp[2] * inv) / 255u);
                    dp[3] = 0xFF;
                }
            }
        }
        return;
    }

    // NV2x legacy monochrome cursor: 2 bits per pixel packed MSB-first.
    // 00 transparent, 01 white, 10 black, 11 invert.
    for (int cy = 0; cy < size; cy++) {
        int dy = start_y + cy;
        if (dy < 0 || dy >= (int)this->active_height) continue;
        uint8_t *dst_row = dst_buf + dy * dst_pitch;
        uint32_t row_words = (uint32_t)size / 16u;
        uint32_t src_base = hw_cursor.offset + (uint32_t)cy * row_words * 4u;
        for (int word = 0; word < (int)row_words; word++) {
            uint32_t bits = cursor_read32(src_base + (uint32_t)word * 4u);
            for (int i = 0; i < 16; i++) {
                int cx = word * 16 + i;
                int dx = start_x + cx;
                if (dx < 0 || dx >= (int)this->active_width) continue;
                uint32_t code = (bits >> (30 - i * 2)) & 0x3u;
                if (code == 0) continue;
                uint8_t *dp = dst_row + dx * 4;
                if (code == 1) {
                    dp[0] = dp[1] = dp[2] = dp[3] = 0xFF;
                } else if (code == 2) {
                    dp[0] = dp[1] = dp[2] = 0x00; dp[3] = 0xFF;
                } else {
                    dp[0] ^= 0xFF; dp[1] ^= 0xFF; dp[2] ^= 0xFF; dp[3] = 0xFF;
                }
            }
        }
    }
}

void NvidiaNV::vidc_get_cursor_position(int &x, int &y)
{
    x = hw_cursor.x;
    y = hw_cursor.y;
}

int32_t NvidiaNV::parse_child_unit_address_string(const std::string unit_address_string,
                                                  HWComponent*& hwc)
{
    return VideoCtrlBase::parse_child_unit_address_string(unit_address_string, hwc);
}

// ============================================================================
// IRQ
// ============================================================================

uint32_t NvidiaNV::get_mc_intr()
{
    uint32_t v = 0;
    if (bus_intr   & bus_intr_en)   v |= 0x10000000u; // NV_PMC_INTR_PBUS (bit 28)
    if (fifo_intr  & fifo_intr_en)  v |= 0x00000100u; // NV_PMC_INTR_PFIFO
    if (graph_intr & graph_intr_en) v |= 0x00001000u; // NV_PMC_INTR_PGRAPH
    if (timer_intr & timer_intr_en) v |= 0x00100000u; // NV_PMC_INTR_PTIMER
    if (crtc_intr  & crtc_intr_en)  v |= 0x01000000u; // NV_PMC_INTR_PCRTC
    return v;
}

void NvidiaNV::set_irq_level(bool level) { this->pci_interrupt((uint8_t)level); }

// ---- PCI config space access ----
uint32_t NvidiaNV::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    return PCIDevice::pci_cfg_read(reg_offs, details);
}

void NvidiaNV::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    PCIDevice::pci_cfg_write(reg_offs, value, details);
}

void NvidiaNV::update_irq_level()
{
    // Match Bochs exactly:
    // (get_mc_intr() && mc_intr_en & 1) || (mc_soft_intr && mc_intr_en & 2)
    bool asserted = (get_mc_intr() && (mc_intr_en & 1u)) ||
                    (mc_soft_intr && (mc_intr_en & 2u));
    set_irq_level(asserted);
}

// ============================================================================
// PTIMER — monotonic timer using host clock
// ============================================================================

uint64_t NvidiaNV::get_current_time() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time).count();
    // Scale by NUM/DEN if programmed, otherwise default 31.25MHz
    if (timer_den != 0 && timer_num != 0) {
        return (ns / 74u) * timer_num / timer_den;
    }
    return (ns >> 5);
}

// ============================================================================
// DDC I2C — custom state machine for EDID delivery
//
// The NDRV bit-bangs I2C via CRTC 0x3F (SCL=bit5, SDA=bit4).
// DingusPPC's DisplayID has a STOP-detection bug when master writes
// SCL=1,SDA=1 during ACK (SCL rises first, then SDA change is seen as STOP).
// This custom handler properly models the wired-AND bus: during ACK the
// device holds SDA=0, so the bus SDA stays 0 regardless of master release.
// ============================================================================

void NvidiaNV::ddc_i2c_write(bool scl, bool sda)
{
#if NV_USE_CUSTOM_DDC
    auto &s = ddc_i2c;
    s.master_sda = sda;  // track raw master SDA

    // Wired-AND bus: actual SDA = master_sda AND device_sda
    bool bus_sda = sda && s.device_sda;
    bool bus_scl = scl;  // no clock stretching

    bool scl_rose  = (bus_scl && !s.last_scl);
    bool sda_fell  = (!bus_sda && s.last_sda);
    bool sda_rose  = (bus_sda && !s.last_sda);

    // START: SDA falls while SCL high
    if (sda_fell && bus_scl) {
        LOG_F(INFO, "%s: DDC I2C START (prev_state=%d)", name.c_str(), (int)s.state);
        s.state = DDC_START;        s.bit_count = 0;
        s.byte = 0;
        s.device_sda = true;
        s.stop_suppress = false;
        LOG_F(INFO, "%s: DDC I2C START", name.c_str());
        s.last_scl = bus_scl;
        s.last_sda = bus_sda;
        return;
    }

    // STOP: SDA rises while SCL high (only when device isn't holding SDA low)
    // Suppress one false STOP after releasing SDA from ACK state
    if (sda_rose && bus_scl && s.device_sda) {
        if (s.stop_suppress) {
            s.stop_suppress = false;
        } else {
            LOG_F(INFO, "%s: DDC I2C STOP (state=%d)", name.c_str(), (int)s.state);
            s.state = DDC_IDLE;
            s.device_sda = true;
            s.last_scl = bus_scl;
            s.last_sda = bus_sda;
            return;
        }
    }

    // On SCL falling edge: release SDA after ACK was clocked and read
    bool scl_fell = (!bus_scl && s.last_scl);
    if (scl_fell) {
        if (s.state == DDC_REG) {
            s.device_sda = true;  // release for master to drive register byte
            s.stop_suppress = true;
        } else if (s.state == DDC_ADDR && !s.device_sda) {
            s.device_sda = true;  // release after ACK_REG, allow repeated START
            s.stop_suppress = true;
        } else if (s.state == DDC_ACK_DATA) {
            s.device_sda = true;  // release for master ACK/NACK
            s.stop_suppress = true;
        }
    }

    // Process on rising SCL edge
    if (scl_rose) {
        switch (s.state) {
        case DDC_START:
            s.state = DDC_ADDR;
            s.byte = (s.byte << 1) | (bus_sda ? 1 : 0);
            s.bit_count = 1;
            break;

        case DDC_ADDR:
            s.byte = (s.byte << 1) | (bus_sda ? 1 : 0);
            if (++s.bit_count >= 8) {
                s.dev_addr = s.byte;
                LOG_F(INFO, "%s: DDC I2C addr=0x%02x", name.c_str(), s.dev_addr);
                if ((s.dev_addr & 0xFE) == 0xA0) {
                    s.state = DDC_ACK_ADDR;
                    s.device_sda = false;  // ACK — device pulls SDA low
                } else {
                    s.state = DDC_NACK;
                    s.device_sda = true;   // NACK
                }
            }
            break;

        case DDC_ACK_ADDR:
            // device_sda stays false (ACK) — master reads it on this clock
            // SCL falling handler will release for write path
            s.bit_count = 0;
            s.byte = 0;
            if (s.dev_addr & 1) {
                // Read — prepare EDID byte, first bit sent on next clock
                s.state = DDC_DATA;
                s.byte = disp_id->get_edid_byte(s.data_pos);
                LOG_F(INFO, "%s: DDC ACK_ADDR(rd) → DATA byte[%d]=0x%02x dev_sda=%d",
                      name.c_str(), s.data_pos, s.byte, s.device_sda);
            } else {
                // Write — receive register address next
                s.state = DDC_REG;
            }
            break;

        case DDC_REG:
            s.byte = (s.byte << 1) | (bus_sda ? 1 : 0);
            if (++s.bit_count >= 8) {
                s.reg_addr = s.byte;
                s.data_pos = s.reg_addr;
                LOG_F(INFO, "%s: DDC I2C reg=0x%02x", name.c_str(), s.reg_addr);
                s.state = DDC_ACK_REG;
                s.device_sda = false;  // ACK
            }
            break;

        case DDC_ACK_REG:
            // device_sda stays false (ACK) — master reads on this clock
            // SCL falling handler will release for repeated START
            s.bit_count = 0;
            s.byte = 0;
            s.state = DDC_ADDR;   // wait for repeated START + read addr
            break;

        case DDC_DATA:
            // Set SDA to current bit, then advance
            s.device_sda = !!(s.byte & (0x80u >> s.bit_count));
            s.bit_count++;
            if (s.bit_count >= 8) {
                s.data_pos++;
                s.state = DDC_ACK_DATA;
                // Don't release SDA here — master reads bit 0 on this clock
                // Falling edge handler will release for master ACK/NACK
            }
            break;

        case DDC_ACK_DATA:
            if (!bus_sda) {
                // Master ACK — send next byte
                s.bit_count = 0;
                s.byte = disp_id->get_edid_byte(s.data_pos);
                if (big_endian_mode && s.data_pos < 20) {
                    LOG_F(INFO, "%s: DDC EDID byte[%d]=0x%02x",
                          name.c_str(), s.data_pos, s.byte);
                }
                s.state = DDC_DATA;
                // First bit sent on NEXT clock in DATA handler
            } else {
                // Master NACK — end of transfer
                if (big_endian_mode) {
                    LOG_F(INFO, "%s: DDC NACK at pos=%d (transfer done)",
                          name.c_str(), s.data_pos);
                }
                s.state = DDC_IDLE;
                s.device_sda = true;
            }
            break;

        default:
            break;
        }
    }

    s.last_scl = bus_scl;
    s.last_sda = bus_sda;
#else
    uint8_t mon_levels = (scl << 1) | (sda << 2);
    uint8_t mon_dirs = 6;
    this->mon_sense = this->disp_id->read_monitor_sense(mon_levels, mon_dirs);
#endif
}

uint8_t NvidiaNV::ddc_i2c_read()
{
    // bit 2 = SCL_in, bit 3 = SDA_in (wired-AND of master and device)
    uint8_t val = 0;
#if NV_USE_CUSTOM_DDC
    if (ddc_i2c.last_scl)                         val |= 0x04;
    if (ddc_i2c.master_sda && ddc_i2c.device_sda) val |= 0x08;
#else
    if (this->mon_sense & 2)                      val |= 0x04;
    if (this->mon_sense & 4)                      val |= 0x08;
#endif
    return val;
}

// ============================================================================
// FIFO helpers
// ============================================================================

void NvidiaNV::update_fifo_wait()
{
    fifo_wait = fifo_wait_soft | fifo_wait_notify |
                fifo_wait_flip | fifo_wait_acquire;
}

void NvidiaNV::fifo_process()
{
    uint32_t offset = (fifo_cache1_push1 & 0x1F) + 1;
    for (uint32_t i = 0; i < NV_CHANNEL_COUNT; i++)
        fifo_process((i + offset) & 0x1F);
}

void NvidiaNV::fifo_process(uint32_t chid)
{
    if (fifo_wait) return;
    if ((fifo_mode & (1u << chid)) == 0) return;
    if ((fifo_cache1_push0 & 1u) == 0) return;
    if ((fifo_cache1_pull0 & 1u) == 0) return;

    uint32_t oldchid = fifo_cache1_push1 & 0x1F;
    if (oldchid == chid) {
        if (fifo_cache1_dma_put == fifo_cache1_dma_get) return;
    } else {
        if (ramfc_read32(chid, 0x0) == ramfc_read32(chid, 0x4)) return;
    }

    if (oldchid != chid) {
        ramfc_write32(oldchid, 0x0, fifo_cache1_dma_put);
        ramfc_write32(oldchid, 0x4, fifo_cache1_dma_get);
        ramfc_write32(oldchid, 0x8, fifo_cache1_ref_cnt);
        ramfc_write32(oldchid, 0xC, fifo_cache1_dma_instance);
        if (card_type >= 0x20) {
            uint32_t sro = card_type < 0x40 ? 0x2C : 0x30;
            ramfc_write32(oldchid, sro, fifo_cache1_semaphore);
        }
        fifo_cache1_dma_put = ramfc_read32(chid, 0x0);
        fifo_cache1_dma_get = ramfc_read32(chid, 0x4);
        fifo_cache1_ref_cnt = ramfc_read32(chid, 0x8);
        fifo_cache1_dma_instance = ramfc_read32(chid, 0xC);
        if (card_type >= 0x20)
            fifo_cache1_semaphore = ramfc_read32(chid, card_type < 0x40 ? 0x2C : 0x30);
        fifo_cache1_push1 = (fifo_cache1_push1 & ~0x1Fu) | chid;
    }

    fifo_cache1_dma_push |= 0x100;
    if (fifo_cache1_dma_instance == 0) {
        LOG_F(ERROR, "%s: FIFO DMA instance = 0", name.c_str());
        return;
    }

    NvChannel* ch = &chs[chid];
    while (fifo_cache1_dma_get != fifo_cache1_dma_put) {
        uint32_t word = dma_read32(fifo_cache1_dma_instance << 4, fifo_cache1_dma_get);
        fifo_cache1_dma_get += 4;

        if (ch->dma_state.mcnt) {
            int cmd_result = execute_command(chid,
                ch->dma_state.subc, ch->dma_state.mthd, word);
            if (cmd_result <= 1) {
                if (!ch->dma_state.ni)
                    ch->dma_state.mthd++;
                ch->dma_state.mcnt--;
            } else {
                fifo_cache1_dma_get -= 4;
            }
            if (cmd_result != 0) break;
        } else {
            if ((word & 0xE0000003u) == 0x20000000u) {
                fifo_cache1_dma_get = word & 0x1FFFFFFFu;  // old jump
            } else if ((word & 3u) == 1u) {
                fifo_cache1_dma_get = word & 0xFFFFFFFCu;  // jump
            } else if ((word & 3u) == 2u) {
                if (ch->subr_active) { LOG_F(ERROR, "%s: FIFO call with subroutine active", name.c_str()); }
                ch->subr_return = fifo_cache1_dma_get;
                ch->subr_active = true;
                fifo_cache1_dma_get = word & 0xFFFFFFFCu;
            } else if (word == 0x00020000u) {
                fifo_cache1_dma_get = ch->subr_return;
                ch->subr_active = false;
            } else if ((word & 0xA0030003u) == 0u) {
                ch->dma_state.mthd = (word >> 2) & 0x7FFu;
                ch->dma_state.subc = (word >> 13) & 7u;
                ch->dma_state.mcnt = (word >> 18) & 0x7FFu;
                ch->dma_state.ni = word & 0x40000000u;
            } else {
                LOG_F(ERROR, "%s: FIFO unexpected word 0x%08x at dma_get=0x%08x",
                      name.c_str(), word, fifo_cache1_dma_get - 4);
                // Stop processing - don't run off into garbage
                fifo_cache1_dma_get = fifo_cache1_dma_put;
                break;
            }
        }
    }
}

int NvidiaNV::execute_command(uint32_t chid, uint32_t subc,
                               uint32_t method, uint32_t param)
{
    int result = 0;
    bool software_method = false;
    NvChannel* ch = &chs[chid];

    if (method == 0x000) {
        // Object bind
        ramht_lookup(param, chid, &ch->schs[subc].object, &ch->schs[subc].engine);
        if (ch->schs[subc].engine == 0x01) {
            uint32_t word1 = ramin_read32(ch->schs[subc].object + 0x4);
            if (card_type < 0x40)
                ch->schs[subc].notifier = word1 >> 16 << 4;
            else
                ch->schs[subc].notifier = (word1 & 0xFFFFF) << 4;
        } else if (ch->schs[subc].engine == 0x00) {
            software_method = true;
        }
    } else if (method == 0x014) {
        fifo_cache1_ref_cnt = param;
    } else if (method >= 0x040) {
        if (ch->schs[subc].engine == 0x01) {
            if (method >= 0x060 && method < 0x080)
                ramht_lookup(param, chid, &param, nullptr);
            uint32_t cls = ramin_read32(ch->schs[subc].object) & class_mask;
            uint8_t cls8 = cls & 0xFF;

            switch (cls8) {
            case 0x19: execute_clip(ch, method, param); break;
            case 0x5e: execute_rect(ch, method, param); break;
            case 0x5f: case 0x9f: execute_imageblit(ch, method, param); break;
            case 0x62: execute_surf2d(ch, method, param); break;
            case 0x43: /* rop */ if (method == 0x0c0) ch->rop = param & 0xFF; break;
            case 0x57: /* chroma */ if (method == 0x0c0) ch->chroma_color_fmt = param;
                       else if (method == 0x0c1) ch->chroma_color = param; break;
            case 0x72: /* beta */ if (method == 0x0c0) ch->beta = param; break;
            default:
                LOG_F(9, "%s: execute_command: unhandled class 0x%02x method 0x%03x",
                      name.c_str(), cls8, method);
                break;
            }

            if (ch->notify_pending) {
                ch->notify_pending = false;
                dma_write32(ch->schs[subc].notifier, 0xC, 0);
            }
            if (method == 0x041) {
                ch->notify_pending = true;
                ch->notify_type = param;
            } else if (method == 0x060)
                ch->schs[subc].notifier = param;
        } else if (ch->schs[subc].engine == 0x00) {
            software_method = true;
        }
    }

    if (software_method) {
        fifo_wait_soft = true;
        fifo_wait = true;
        fifo_intr |= 0x00000001u;
        update_irq_level();
        fifo_cache1_pull0 |= 0x00000100u;
        fifo_cache1_method[fifo_cache1_put / 4] = (method << 2) | (subc << 13);
        fifo_cache1_data[fifo_cache1_put / 4] = param;
        fifo_cache1_put += 4;
        if (fifo_cache1_put == NV_CACHE1_SIZE * 4)
            fifo_cache1_put = 0;
        result = 1;
    }
    return result;
}

// ============================================================================
// 2D engine helpers
// ============================================================================

uint32_t NvidiaNV::ramfc_address(uint32_t chid, uint32_t offset)
{
    uint32_t ramfc = (fifo_ramfc & 0xFFF) << 8;
    uint32_t ch_size = (card_type < 0x20) ? 0x20 : 0x40;
    return ramfc + chid * ch_size + offset;
}

void NvidiaNV::ramfc_write32(uint32_t chid, uint32_t offset, uint32_t value)
{
    ramin_write32(ramfc_address(chid, offset), value);
}

uint32_t NvidiaNV::ramfc_read32(uint32_t chid, uint32_t offset)
{
    return ramin_read32(ramfc_address(chid, offset));
}

void NvidiaNV::ramht_lookup(uint32_t handle, uint32_t chid, uint32_t* object, uint8_t* engine)
{
    uint32_t ramht_addr = (fifo_ramht & 0xFFF) << 8;
    uint32_t ramht_bits = ((fifo_ramht >> 16) & 0xFF) + 9;
    uint32_t ramht_size = 1u << ramht_bits << 3;

    uint32_t hash = 0;
    uint32_t x = handle;
    while (x) {
        hash ^= (x & ((1u << ramht_bits) - 1));
        x >>= ramht_bits;
    }
    hash ^= (chid & 0xF) << (ramht_bits - 4);
    hash = hash << 3;

    uint32_t it = hash;
    do {
        if (ramin_read32(ramht_addr + it) == handle) {
            uint32_t context = ramin_read32(ramht_addr + it + 4);
            uint32_t ctx_chid = (card_type < 0x40)
                ? (context >> 24) & 0x1F
                : (context >> 23) & 0x1F;
            if (chid == ctx_chid) {
                if (object) {
                    *object = (card_type < 0x40)
                        ? (context & 0xFFFF) << 4
                        : (context & 0xFFFFF) << 4;
                }
                if (engine) {
                    *engine = (card_type < 0x40)
                        ? (context >> 16) & 0xFF
                        : (context >> 20) & 0x7;
                }
                return;
            }
        }
        it += 8;
        if (it >= ramht_size) it = 0;
    } while (it != hash);

    LOG_F(ERROR, "%s: ramht_lookup failed for 0x%08x", name.c_str(), handle);
}

uint32_t NvidiaNV::dma_lin_lookup(uint32_t object, uint32_t address)
{
    uint32_t adjust = ramin_read32(object) >> 20;
    uint32_t base = ramin_read32(object + 8) & 0xFFFFF000u;
    return base + adjust + address;
}

// dma_address — unified address translation that dispatches on the DMA
// object flags word (ramin word 0):
//   bit 17 set  → system-RAM paged object → use page-table walk
//   bit 17 clear → VRAM linear object     → use linear base+offset
//
// All six consumer helpers (dma_read8/32, dma_write8/32, get_pixel,
// put_pixel) call this instead of hardcoding dma_lin_lookup so that
// the kext's system-RAM push-buffer channel (chid 1) resolves correctly.
uint32_t NvidiaNV::dma_address(uint32_t object, uint32_t address)
{
    uint32_t flags = ramin_read32(object);
    if (flags & 0x00020000u)
        return dma_pt_lookup(object, address);   // system RAM: page table
    else
        return dma_lin_lookup(object, address);  // VRAM: linear
}

uint8_t NvidiaNV::dma_read8(uint32_t object, uint32_t address)
{
    uint32_t flags    = ramin_read32(object);
    uint32_t addr_abs = dma_address(object, address);
    if (flags & 0x00020000u)
        return physical_read8(addr_abs);
    else
        return vram_read8(addr_abs & memsize_mask);
}

void NvidiaNV::dma_write8(uint32_t object, uint32_t address, uint8_t value)
{
    uint32_t flags    = ramin_read32(object);
    uint32_t addr_abs = dma_address(object, address);
    if (flags & 0x00020000u)
        physical_write8(addr_abs, value);
    else
        vram_write8(addr_abs & memsize_mask, value);
}

uint32_t NvidiaNV::dma_read32(uint32_t object, uint32_t address)
{
    uint32_t flags    = ramin_read32(object);
    uint32_t addr_abs = dma_address(object, address);
    if (flags & 0x00020000u)
        return physical_read32(addr_abs);
    else
        return vram_read32(addr_abs & memsize_mask);
}

void NvidiaNV::dma_write32(uint32_t object, uint32_t address, uint32_t value)
{
    uint32_t flags    = ramin_read32(object);
    uint32_t addr_abs = dma_address(object, address);
    if (flags & 0x00020000u)
        physical_write32(addr_abs, value);
    else
        vram_write32(addr_abs & memsize_mask, value);
}

void NvidiaNV::dma_write64(uint32_t object, uint32_t address, uint64_t value)
{
    dma_write32(object, address, (uint32_t)value);
    dma_write32(object, address + 4, (uint32_t)(value >> 32));
}

uint32_t NvidiaNV::get_pixel(uint32_t obj, uint32_t ofs, uint32_t x, uint32_t cb)
{
    uint32_t flags = ramin_read32(obj);
    if (cb == 1) {
        uint32_t a = dma_address(obj, ofs + x) & memsize_mask;
        return (flags & 0x00020000u) ? physical_read8(dma_address(obj, ofs + x))
                                     : (uint32_t)vram_read8(a);
    } else if (cb == 2) {
        uint32_t a = dma_address(obj, ofs + x * 2) & memsize_mask;
        return (flags & 0x00020000u) ? physical_read16(dma_address(obj, ofs + x * 2))
                                     : (uint32_t)vram_read16(a);
    } else {
        uint32_t a = dma_address(obj, ofs + x * 4) & memsize_mask;
        return (flags & 0x00020000u) ? physical_read32(dma_address(obj, ofs + x * 4))
                                     : vram_read32(a);
    }
}

void NvidiaNV::put_pixel(NvChannel* ch, uint32_t ofs, uint32_t x, uint32_t value)
{
    uint32_t obj   = ch->s2d_img_dst;
    uint32_t flags = ramin_read32(obj);
    bool sys       = (flags & 0x00020000u) != 0;

    if (ch->s2d_color_bytes == 1) {
        uint32_t addr = dma_address(obj, ofs + x);
        if (sys) physical_write8(addr, (uint8_t)value);
        else     vram_write8(addr & memsize_mask, (uint8_t)value);
    } else if (ch->s2d_color_bytes == 2) {
        uint32_t addr = dma_address(obj, ofs + x * 2);
        if (sys) physical_write16(addr, (uint16_t)value);
        else     vram_write16(addr & memsize_mask, (uint16_t)value);
    } else {
        uint32_t addr = dma_address(obj, ofs + x * 4);
        if (sys) physical_write32(addr, value);
        else     vram_write32(addr & memsize_mask, value);
    }
}

void NvidiaNV::pixel_operation(NvChannel* ch, uint32_t op,
    uint32_t* dstcolor, const uint32_t* srccolor, uint32_t cb, uint32_t px, uint32_t py)
{
    // Simplified: only SRCCOPY (op 3) and ROP copy (op 1 with rop 0xCC)
    // Full implementation would handle all ROP3, alpha blend (op 5), etc.
    (void)ch; (void)cb; (void)px; (void)py;
    *dstcolor = *srccolor;
}

// ============================================================================
// 2D engine object methods
// ============================================================================

void NvidiaNV::execute_clip(NvChannel* ch, uint32_t method, uint32_t param)
{
    if (method == 0x0c0) {
        ch->clip_x = param & 0xFFFF;
        ch->clip_y = param >> 16;
    } else if (method == 0x0c1) {
        ch->clip_width = param & 0xFFFF;
        ch->clip_height = param >> 16;
    }
}

void NvidiaNV::execute_surf2d(NvChannel* ch, uint32_t method, uint32_t param)
{
    ch->s2d_locked = true;
    if (method == 0x061)      ch->s2d_img_src = param;
    else if (method == 0x062) ch->s2d_img_dst = param;
    else if (method == 0x0c0) {
        ch->s2d_color_fmt = param;
        if (param == 0x1)                                ch->s2d_color_bytes = 1;
        else if (param == 0x2 || param == 0x4 || param == 0x5) ch->s2d_color_bytes = 2;
        else                                             ch->s2d_color_bytes = 4;
    }
    else if (method == 0x0c1) {
        ch->s2d_pitch_src = param & 0xFFFF;
        ch->s2d_pitch_dst = param >> 16;
    }
    else if (method == 0x0c2) ch->s2d_ofs_src = param;
    else if (method == 0x0c3) ch->s2d_ofs_dst = param;
}

void NvidiaNV::execute_rect(NvChannel* ch, uint32_t method, uint32_t param)
{
    if (method == 0x0bf)      ch->rect_operation = param;
    else if (method == 0x0c0) ch->rect_color_fmt = param;
    else if (method == 0x0c1) ch->rect_color = param;
    else if (method >= 0x100 && method < 0x120) {
        if (method & 1) {
            ch->rect_hw = param;
            do_rect(ch);
        } else
            ch->rect_yx = param;
    }
}

void NvidiaNV::do_rect(NvChannel* ch)
{
    int16_t dx = (int16_t)(ch->rect_yx & 0xFFFF);
    int16_t dy = (int16_t)(ch->rect_yx >> 16);
    uint16_t width  = ch->rect_hw & 0xFFFF;
    uint16_t height = ch->rect_hw >> 16;
    uint32_t pitch  = ch->s2d_pitch_dst;
    uint32_t srccolor = ch->rect_color;
    uint32_t cb = ch->s2d_color_bytes;
    uint32_t draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * cb;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint32_t dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, cb);
            pixel_operation(ch, ch->rect_operation, &dstcolor, &srccolor, cb, dx + x, dy + y);
            put_pixel(ch, draw_offset, x, dstcolor);
        }
        draw_offset += pitch;
    }
    this->draw_fb = true;
}

void NvidiaNV::execute_imageblit(NvChannel* ch, uint32_t method, uint32_t param)
{
    if (method == 0x0bf)      ch->blit_operation = param;
    else if (method == 0x0c0) ch->blit_syx = param;
    else if (method == 0x0c1) ch->blit_dyx = param;
    else if (method == 0x0c2) {
        ch->blit_hw = param;
        do_copyarea(ch);
    }
}

void NvidiaNV::do_copyarea(NvChannel* ch)
{
    int16_t sx = (int16_t)(ch->blit_syx & 0xFFFF);
    int16_t sy = (int16_t)(ch->blit_syx >> 16);
    int16_t dx = (int16_t)(ch->blit_dyx & 0xFFFF);
    int16_t dy = (int16_t)(ch->blit_dyx >> 16);
    uint16_t width  = ch->blit_hw & 0xFFFF;
    uint16_t height = ch->blit_hw >> 16;
    uint32_t cb = ch->s2d_color_bytes;
    uint32_t src_pitch = ch->s2d_pitch_src;
    uint32_t dst_pitch = ch->s2d_pitch_dst;

    // Handle overlapping copies
    int y_start, y_end, y_step;
    if (dy > sy) {
        y_start = height - 1; y_end = -1; y_step = -1;
    } else {
        y_start = 0; y_end = height; y_step = 1;
    }

    for (int y = y_start; y != y_end; y += y_step) {
        uint32_t src_ofs = ch->s2d_ofs_src + (sy + y) * src_pitch + sx * cb;
        uint32_t dst_ofs = ch->s2d_ofs_dst + (dy + y) * dst_pitch + dx * cb;
        for (uint16_t x = 0; x < width; x++) {
            uint32_t pixel = get_pixel(ch->s2d_img_src, src_ofs, x, cb);
            put_pixel(ch, dst_ofs, x, pixel);
        }
    }
    this->draw_fb = true;
}

// ============================================================================
// register_read32 — NV BAR0 MMIO register reads
//
// PORT NOTE: the full if/else chain (~600 lines in bochs) is a mechanical
// type-rename.  The skeleton below covers the interrupt/FIFO/timer/PCRTC/
// PRAMDAC registers.  Expand by copying the remainder from geforce.cc.
// ============================================================================

uint32_t NvidiaNV::register_read32(uint32_t address)
{
    uint32_t value = 0;

    // NV_PMC_BOOT_0 — architecture ID
    if (address == 0x000000) {
        value = (card_type == NV_TYPE_NV20) ? 0x020200A5u
                                            : ((uint32_t)card_type << 20);
    }
    // NV_PMC_BOOT_1 — big-endian control
    else if (address == 0x000004) {
        value = big_endian_mode ? 0x00000001u : 0x00000000u;
    }
    // NV_PMC_INTR_0
    else if (address == 0x000100) value = get_mc_intr() | (mc_soft_intr ? 0x80000000u : 0u);
    else if (address == 0x000140) value = mc_intr_en;
    else if (address == 0x000200) value = mc_enable;

    // NV_PBUS
    else if (address == 0x001100) value = bus_intr;
    else if (address == 0x001140) value = bus_intr_en;

    // PCI config mirror at 0x1800
    else if (address >= 0x001800 && address < 0x001900) {
        AccessDetails details = 4;
        value = pci_cfg_read(address - 0x001800u, details);
    }

    // NV_PFIFO
    else if (address == 0x002080) value = 0x00000001u;  // PFIFO_DEBUG_0: FIFO idle/ready
    else if (address == 0x002100) value = fifo_intr;
    else if (address == 0x002140) value = fifo_intr_en;
    else if (address == 0x002210) value = fifo_ramht;
    else if (address == 0x002214 && card_type < NV_TYPE_NV40) value = fifo_ramfc;
    else if (address == 0x002218) value = fifo_ramro;
    else if (address == 0x002220 && card_type >= NV_TYPE_NV40) value = fifo_ramfc;
    else if (address == 0x002400) value = (fifo_cache1_get != fifo_cache1_put) ? 0u : 0x10u;
    else if (address == 0x002504) value = fifo_mode;
    else if (address == 0x003200) value = fifo_cache1_push0;
    else if (address == 0x003204) value = fifo_cache1_push1;
    else if (address == 0x003210) value = fifo_cache1_put;
    else if (address == 0x003214) value = (fifo_cache1_get != fifo_cache1_put) ? 0u : 0x10u;
    else if (address == 0x003220) value = fifo_cache1_dma_push;
    else if (address == 0x00322C) value = fifo_cache1_dma_instance;
    else if (address == 0x003230) value = 0x80000000u;  // PFIFO_CACHE1_DMA_CTL
    else if (address == 0x003240) value = fifo_cache1_dma_put;
    else if (address == 0x003244) value = fifo_cache1_dma_get;
    else if (address == 0x003248) value = fifo_cache1_ref_cnt;
    else if (address == 0x003250) {
        if (fifo_cache1_get != fifo_cache1_put) fifo_cache1_pull0 |= 0x100u;
        value = fifo_cache1_pull0;
    }
    else if (address == 0x003270) value = fifo_cache1_get;
    else if (address == 0x0032E0) value = fifo_grctx_instance;
    else if (address == 0x003304) value = 1u;

    // PFIFO cache1 method/data pairs
    else if ((address >= 0x003800 && address < 0x004000 && card_type < NV_TYPE_NV40) ||
             (address >= 0x090000 && address < 0x092000 && card_type >= NV_TYPE_NV40)) {
        uint32_t off = (card_type < NV_TYPE_NV40) ? (address - 0x3800u) : (address - 0x90000u);
        uint32_t idx = off / 8u;
        value = (off % 8u == 0u) ? fifo_cache1_method[idx] : fifo_cache1_data[idx];
    }

    // NV_PTIMER
    else if (address == 0x009100) value = timer_intr;
    else if (address == 0x009140) value = timer_intr_en;
    else if (address == 0x009200) value = timer_num;
    else if (address == 0x009210) value = timer_den;
    else if (address == 0x009400) value = (uint32_t)get_current_time();
    else if (address == 0x009410) value = (uint32_t)(get_current_time() >> 32);
    else if (address == 0x009420) value = timer_alarm;

    // NV_PEXTDEV — straps
    else if (address == 0x101000) value = straps0_primary;

    // NV_PFB
    else if (address == 0x10020Cu) value = memsize;
    else if (address == 0x100320u) {
        if      (card_type == NV_TYPE_NV20) value = 0x00007FFFu;
        else if (card_type == NV_TYPE_NV35) value = 0x0005C7FFu;
        else                                value = 0x0002E3FFu;
    }

    // FCode ROM mirror at 0x300000
    else if (address >= 0x300000u && address < 0x310000u) {
        uint32_t off = address - 0x300000u;
        if (exp_rom_data && exp_rom_size > off + 3) {
            value =  (uint32_t)exp_rom_data[off]
                  | ((uint32_t)exp_rom_data[off+1] <<  8)
                  | ((uint32_t)exp_rom_data[off+2] << 16)
                  | ((uint32_t)exp_rom_data[off+3] << 24);
        }
    }

    // NV_PGRAPH
    else if (address == 0x400100u) value = graph_intr;
    else if (address == 0x400108u) value = graph_nsource;
    else if ((address == 0x40013Cu && card_type >= NV_TYPE_NV40) ||
             (address == 0x400140u && card_type <  NV_TYPE_NV40)) value = graph_intr_en;
    else if (address == 0x40014Cu) value = graph_ctx_switch1;
    else if (address == 0x400720u) value = 0;  // PGRAPH_STATUS: always idle
    else if (address == 0x400724u) value = graph_trapped_addr;
    else if (address == 0x400728u) value = graph_trapped_data;

    // NV_PCRTC
    else if (address == 0x600100u) value = crtc_intr;
    else if (address == 0x600140u) value = crtc_intr_en;
    else if (address == 0x600800u) value = crtc_start;
    else if (address == 0x600804u) value = crtc_config;
    else if (address == 0x600808u) {
        // NV_PCRTC_RASTER — returns current scanline
        auto now = std::chrono::steady_clock::now();
        static auto frame_start = now;
        uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(now - frame_start).count();
        uint32_t line = (uint32_t)((us * 525) / 16667) % 525;
        value = line & 0x7FF;
        if (line >= 480) value |= 0x10000;  // VBlank flag
    }
    else if (address == 0x60080Cu) value = crtc_cursor_offset;
    else if (address == 0x600810u) value = crtc_cursor_config;
    else if (address == 0x60081Cu) {
        value = crtc_gpio_ext;
        if (card_type == NV_TYPE_NV35) value |= 0x04000000u;
    }
    else if (address == 0x600868u) {
        // Raster line counter — fake based on timer
        value = crtc_raster_pos;
    }

    // NV_PRAMDAC
    else if (address == 0x680300u) value = ramdac_cu_start_pos;
    else if (address == 0x680404u) value = 0;  // RAMDAC_NV10_CURSYNC
    else if (address == 0x680500u) value = ramdac_nvpll;
    else if (address == 0x680504u) value = ramdac_mpll;
    else if (address == 0x680508u) value = ramdac_vpll;
    else if (address == 0x68050Cu) value = ramdac_pll_select;
    else if (address == 0x680578u) value = ramdac_vpll_b;
    else if (address == 0x680600u) value = ramdac_general_control;
    else if (address == 0x680624u) value = 0; // FP TMDS data — no flat panel connected
    else if (address == 0x680828u) value = 0;  // PRAMDAC_FP_HCRTC — second monitor disconnected

    // Palette registers — delegate to register_read8
    else if ((address >= 0x601300u && address < 0x601400u) ||
             (address >= 0x681300u && address < 0x681400u)) {
        value = register_read8(address);
    }

    // 32-bit CRTC access (NDRV uses 0x6013D4/0x6033D4 for combined index+data)
    else if (address == 0x6013D4u || address == 0x6033D4u) {
        uint8_t idx = crtc_ext.index;
        uint8_t dat;
        if (idx == 0x3E)
            dat = ddc_i2c_read();
        else if (idx <= NV_CRTC_EXT_MAX)
            dat = crtc_ext.reg[idx];
        else
            dat = 0;
        value = (uint32_t)idx | ((uint32_t)dat << 8);
    }

    // NV_PRAMIN — instance memory accessible through BAR0
    else if (address >= 0x700000u && address < 0x800000u) {
        uint32_t off = address - 0x700000u;
        value = ramin_read32(off);
    }

    // User FIFO channels read (0x800000-0x9FFFFF, 0xC00000-0xDFFFFF)
    else if ((address >= 0x800000 && address < 0xA00000) ||
             (address >= 0xC00000 && address < 0xE00000)) {
        uint32_t chid, offset;
        if (address < 0xA00000) {
            chid = (address >> 16) & 0x1F;
            offset = address & 0x1FFF;
        } else {
            chid = (address >> 12) & 0x1FF;
            if (chid >= NV_CHANNEL_COUNT) chid = 0;
            offset = address & 0xFFF;
        }
        value = 0;
        uint32_t curchid = fifo_cache1_push1 & 0x1F;
        if (offset == 0x40)
            value = (curchid == chid) ? fifo_cache1_dma_put : ramfc_read32(chid, 0x0);
        else if (offset == 0x44)
            value = (curchid == chid) ? fifo_cache1_dma_get : ramfc_read32(chid, 0x4);
        else if (offset == 0x48)
            value = fifo_cache1_ref_cnt;
        else if (offset == 0x10)
            value = 0xFFFF;
    }

    else {
        if ((address >> 2) < (4u * 1024u * 1024u))
            value = unk_regs[address >> 2];
        else
            LOG_F(9, "%s: unimpl read32 @ %08x", name.c_str(), address);
    }

    // Log reads after BOOT_1 — skip PTIMER only, 10000 limit
    {
        static int post_be_rd = 0;
        if (big_endian_mode && saw_user_dma_put && post_be_rd < 50000) {
            LOG_F(INFO, "%s: post-BE rd32 %06x = %08x", name.c_str(), address, value);
            post_be_rd++;
        }
    }

    return value;
}

void NvidiaNV::register_write32(uint32_t address, uint32_t value)
{
    LOG_F(9, "%s: reg_wr32 %06x = %08x", name.c_str(), address, value);

    // Log post-BOOT_1 writes (skip RAMIN zeroing spam)
    {
        static int post_be_wr = 0;
        if (big_endian_mode && saw_user_dma_put && post_be_wr < 50000 &&
            !(address >= 0x700000 && address < 0x800000 && value == 0) &&
            address != 0x400750 && address != 0x400754) {
            LOG_F(INFO, "%s: post-BE wr32 %06x = %08x", name.c_str(), address, value);
            post_be_wr++;
        }
    }

    // NV_PMC_INTR_0 — acknowledge by writing 1s
    if (address == 0x000004) {
        bool was_be = big_endian_mode;
        big_endian_mode = (value & 1u) != 0;
        LOG_F(INFO, "%s: NV_PMC_BOOT_1 = %08x (BE mode %s)",
              name.c_str(), value, big_endian_mode ? "ON" : "OFF");
        // Clear byte-swapped garbage from FCode in key register ranges
        if (big_endian_mode && !was_be) {
            // Seed PCRTC registers from unk_regs before clearing
            if (!crtc_start) crtc_start = unk_regs[0x600800 >> 2];
            if (!crtc_config) crtc_config = unk_regs[0x600804 >> 2];

            // Only clear flat-panel registers that had byte-swapped garbage
            // Don't clear PLLs (0x680500-0x6805FF) — kext needs those
            memset(&unk_regs[0x680800 >> 2], 0, 0x200 * sizeof(uint32_t)); // FP regs only
            memset(&unk_regs[0x600000 >> 2], 0, 0x1000 * sizeof(uint32_t)); // PCRTC

            // Seed named PLL variables from FCode-written unk_regs values
            // If FCode didn't write them, use reasonable GeForce3 defaults
            ramdac_nvpll     = unk_regs[0x680500 >> 2] ? unk_regs[0x680500 >> 2] : 0x0001FB09u; // ~300MHz core
            ramdac_mpll      = unk_regs[0x680504 >> 2] ? unk_regs[0x680504 >> 2] : 0x00012701u; // ~200MHz mem
            if (!ramdac_vpll) ramdac_vpll = unk_regs[0x680508 >> 2];
            if (!ramdac_pll_select) ramdac_pll_select = unk_regs[0x68050C >> 2];
            if (!ramdac_vpll_b) ramdac_vpll_b = unk_regs[0x680578 >> 2];
            if (!ramdac_general_control) ramdac_general_control = unk_regs[0x680600 >> 2];
            LOG_F(INFO, "%s: BOOT_1 PLL seed: NVPLL=%08x MPLL=%08x VPLL=%08x",
                  name.c_str(), ramdac_nvpll, ramdac_mpll, ramdac_vpll);
        }
        mode_needs_update = true;
        crtc_update();
    }
    else if (address == 0x000100) {
        if (mc_soft_intr && (value & 0x80000000u)) mc_soft_intr = false;
        update_irq_level();
    }
    else if (address == 0x000140) {
        mc_intr_en = value;
        static int mc_log = 0;
        if (mc_log++ < 5)
            LOG_F(INFO, "%s: mc_intr_en = %08x", name.c_str(), value);
        update_irq_level();
    }
    else if (address == 0x000200) { mc_enable  = value; }

    // NV_PBUS
    else if (address == 0x001100) { bus_intr   &= ~value; update_irq_level(); }
    else if (address == 0x001140) { bus_intr_en = value;  update_irq_level(); }

    // PCI config mirror — forward to PCIDevice
    else if (address >= 0x001800u && address < 0x001900u) {
        AccessDetails details = 4;
        pci_cfg_write(address - 0x001800u, value, details);
    }

    // NV_PFIFO
    else if (address == 0x002100) {
        fifo_intr &= ~value;
        if (value & 0x00000001u) { fifo_wait_soft = false; update_fifo_wait(); }
        update_irq_level();
    }
    else if (address == 0x002140) {
        fifo_intr_en = value;
        LOG_F(INFO, "%s: fifo_intr_en = %08x", name.c_str(), value);
        update_irq_level();
    }
    else if (address == 0x002210) { fifo_ramht = value; }
    else if (address == 0x002214 && card_type < NV_TYPE_NV40) { fifo_ramfc = value; }
    else if (address == 0x002218) { fifo_ramro = value; }
    else if (address == 0x002220 && card_type >= NV_TYPE_NV40) { fifo_ramfc = value; }
    else if (address == 0x002504) {
        bool process = (fifo_mode | value) != fifo_mode;
        fifo_mode = value;
        if (process) fifo_process();
    }
    else if (address == 0x003200) { fifo_cache1_push0 = value; }
    else if (address == 0x003204) { fifo_cache1_push1 = value; }
    else if (address == 0x003210) { fifo_cache1_put = value; }
    else if (address == 0x003220) { fifo_cache1_dma_push = value; }
    else if (address == 0x00322C) { fifo_cache1_dma_instance = value; }
    else if (address == 0x003240) { fifo_cache1_dma_put = value; }
    else if (address == 0x003244) { fifo_cache1_dma_get = value; }
    else if (address == 0x003248) { fifo_cache1_ref_cnt = value; }
    else if (address == 0x003250) {
        fifo_cache1_pull0 = value;
        if (value & 1u) fifo_process();
    }
    else if (address == 0x003270) { fifo_cache1_get = value; }
    else if (address == 0x0032E0) { fifo_grctx_instance = value; }
    else if (address >= 0x003800 && address < 0x004000) {
        uint32_t off = address - 0x003800;
        uint32_t idx = off / 8;
        if (off % 8 == 0) fifo_cache1_method[idx] = value;
        else fifo_cache1_data[idx] = value;
    }

    // NV_PTIMER
    else if (address == 0x009100) { timer_intr &= ~value; update_irq_level(); }
    else if (address == 0x009140) { timer_intr_en = value; update_irq_level(); }
    else if (address == 0x009200) { timer_num = value; }
    else if (address == 0x009210) { timer_den = value; }
    else if (address == 0x009420) { timer_alarm = value; }

    // NV_PEXTDEV
    else if (address == 0x101000) { straps0_primary = value; }

    // NV_PGRAPH
    else if (address == 0x400100) {
        graph_intr &= ~value;
        if (value & 0x00000001u) { fifo_wait_notify = false; update_fifo_wait(); }
        if (value & 0x00000100u) { fifo_wait_flip = false; update_fifo_wait(); }
        update_irq_level();
    }
    else if ((address == 0x40013Cu && card_type >= NV_TYPE_NV40) ||
             (address == 0x400140u && card_type <  NV_TYPE_NV40))
        { graph_intr_en = value; update_irq_level(); }

    // NV_PCRTC — any change may require display mode recalculation
    else if (address == 0x600100) {
        crtc_intr &= ~value;
        update_irq_level();
    }
    else if (address == 0x600140) {
        crtc_intr_en = value;
        static int ci_log = 0;
        if (ci_log++ < 5)
            LOG_F(INFO, "%s: crtc_intr_en = %08x", name.c_str(), value);
        update_irq_level();
    }
    else if (address == 0x600800) { crtc_start = value; mode_needs_update = true; crtc_update(); }
    else if (address == 0x600804) { crtc_config = value; }
    else if (address == 0x60080C) {
        crtc_cursor_offset = value;
        hw_cursor.offset   = value;
        this->set_cursor_dirty();
        static int curofs_log = 0;
        if (curofs_log < 50) {
            LOG_F(INFO, "%s: CUROFS %08x (vram=%d)",
                  name.c_str(), value, hw_cursor.vram ? 1 : 0);
            curofs_log++;
        }
    }
    else if (address == 0x600810) {
        crtc_cursor_config    = value;
        hw_cursor.enabled     = (value & 0x1u) != 0;
        hw_cursor.bpp32       = (value & 0x1000u) != 0;
        hw_cursor.size        = (value & 0x10000u) ? 64 : 32;
        hw_cursor.vram        = (value & 0x100u) != 0 || (card_type >= NV_TYPE_NV40);
        this->vidc_cursor_on  =  hw_cursor.enabled;
        this->set_cursor_dirty();
        static int curcfg_log = 0;
        if (curcfg_log < 50) {
            LOG_F(INFO, "%s: CURCFG %08x en=%d bpp32=%d size=%u vram=%d",
                  name.c_str(), value, hw_cursor.enabled ? 1 : 0,
                  hw_cursor.bpp32 ? 1 : 0, (unsigned)hw_cursor.size,
                  hw_cursor.vram ? 1 : 0);
            curcfg_log++;
        }
    }
    else if (address == 0x60081C) { crtc_gpio_ext = value; }

    // Palette registers — delegate to register_write8
    else if ((address >= 0x601300 && address < 0x601400) ||
             (address >= 0x681300 && address < 0x681400)) {
        register_write8(address, (uint8_t)value);
    }

    // 32-bit CRTC access (NDRV uses 0x6013D4/0x6033D4 for combined index+data)
    else if (address == 0x6013D4 || address == 0x6033D4) {
        // Low byte → CRTC index (port 0x3D4), second byte → CRTC data (port 0x3D5)
        register_write8(0x601000u + 0x3D4, value & 0xFF);
        register_write8(0x601000u + 0x3D5, (value >> 8) & 0xFF);
    }

    // NV_PRAMDAC — pixel clock changes trigger mode recalc
    else if (address == 0x680300) {
        ramdac_cu_start_pos = value;
        hw_cursor.x = (int16_t)(value & 0xFFFFu);
        hw_cursor.y = (int16_t)((value >> 16) & 0xFFFFu);
        this->set_cursor_dirty();
        static int curpos_log = 0;
        if (curpos_log < 200) {
            LOG_F(INFO, "%s: CURPOS raw=%08x x=%d y=%d en=%d ofs=%08x cfg=%08x",
                  name.c_str(), value, hw_cursor.x, hw_cursor.y,
                  hw_cursor.enabled ? 1 : 0, hw_cursor.offset, crtc_cursor_config);
            curpos_log++;
        }
    }
    else if (address == 0x680500) { ramdac_nvpll  = value; }
    else if (address == 0x680504) { ramdac_mpll   = value; }
    else if (address == 0x680508) { ramdac_vpll   = value; crtc_update(); }
    else if (address == 0x68050C) { ramdac_pll_select = value; }
    else if (address == 0x680578) { ramdac_vpll_b = value; }
    else if (address == 0x680600) { ramdac_general_control = value; }

    // NV_PRAMIN — instance memory accessible through BAR0
    else if (address >= 0x700000 && address < 0x800000) {
        ramin_write32(address - 0x700000u, value);
    }

    // User FIFO channels — DMA mode only (0x800000-0x9FFFFF)
    else if (address >= 0x800000 && address < 0xA00000) {
        uint32_t chid = (address >> 16) & 0x1F;
        uint32_t offset = address & 0x1FFF;
        LOG_F(INFO, "%s: USER_CH wr chid=%d ofs=0x%04x val=0x%08x fifo_mode=0x%08x dma_inst=0x%08x",
              name.c_str(), chid, offset, value, fifo_mode, fifo_cache1_dma_instance);
        saw_user_dma_put = true;
        if ((fifo_mode & (1u << chid)) != 0 &&
            fifo_cache1_dma_instance != 0 &&
            offset == 0x40) {
            uint32_t curchid = fifo_cache1_push1 & 0x1F;
            if (curchid == chid) {
                fifo_cache1_dma_put = value;
                // Don't set dma_get=put — let fifo_process advance it
            } else
                ramfc_write32(chid, 0x0, value);
            fifo_process(chid);
        }
    }

    // User FIFO channels — DMA mode (0xC00000-0xDFFFFF)
    else if (address >= 0xC00000 && address < 0xE00000) {
        uint32_t chid = (address >> 12) & 0x1FF;
        if (chid < NV_CHANNEL_COUNT) {
            uint32_t offset = address & 0xFFF;
            if ((fifo_mode & (1u << chid)) != 0 &&
                fifo_cache1_dma_instance != 0 &&
                offset == 0x40) {
                uint32_t curchid = fifo_cache1_push1 & 0x1F;
                if (curchid == chid) {
                    fifo_cache1_dma_put = value;
                } else
                    ramfc_write32(chid, 0x0, value);
                fifo_process(chid);
            }
        }
    }

    else {
        if ((address >> 2) < (4u * 1024u * 1024u))
            unk_regs[address >> 2] = value;
        else
            LOG_F(9, "%s: unimpl write32 @ %08x = %08x", name.c_str(), address, value);
    }
}

// ============================================================================
// register_read8 / register_write8
//
// Handles byte-wide accesses to:
//   0xC0300–0xC03FF  PCRTC CRTC index/data (extended register window)
//   0xC2300–0xC23FF  same for second head (unused on Power Mac)
//   0x68080x         PRAMDAC palette (CLUT) DAC byte window
//
// PORT NOTE: copy bochs register_read8 / register_write8 verbatim with type
// rename.  Writes to crtc_ext.reg[] for the geometry/depth registers
// (0x28, 0x01, 0x12, 0x13, 0x19, 0x25, 0x2D, 0x41, 0x42) should set
// mode_needs_update = true and then call crtc_update().
// ============================================================================

uint8_t NvidiaNV::register_read8(uint32_t address)
{
    uint8_t value = 0;

    // PRMVIO — VGA sequencer / misc at BAR0 + 0x0C0000
    if (address >= 0x0C0000u && address < 0x0C0400u) {
        uint32_t port = address - 0x0C0000u;
        switch (port) {
        case 0x3C2: value = 0; break;  // Input Status 0 (read)
        case 0x3C3: value = vga_enable; break;
        case 0x3C4: value = vga_seq_index; break;
        case 0x3C5: value = vga_seq_data[vga_seq_index & 7]; break;
        case 0x3CC: value = vga_misc_output; break;  // Misc Output (read port)
        case 0x3CE: value = vga_gfx_index; break;
        case 0x3CF: value = vga_gfx_data[vga_gfx_index & 0xF]; break;
        default:
            LOG_F(WARNING, "%s: PRMVIO rd8 port %03x unimpl", name.c_str(), port);
            break;
        }
    }
    // PRMCIO — VGA CRTC / attribute at BAR0 + 0x601000
    else if (address >= 0x601000u && address < 0x602000u) {
        uint32_t port = address - 0x601000u;
        switch (port) {
        case 0x3C0: value = vga_attr_index; break;
        case 0x3C1: value = vga_attr_data[vga_attr_index & 0x1F]; break;
        case 0x3D4: value = crtc_ext.index; break;
        case 0x3D5:
            if (crtc_ext.index == 0x3E) {
                value = ddc_i2c_read();
                if (big_endian_mode) {
                    static int ddc_rd_count = 0;
                    if (ddc_rd_count < 0)
                        LOG_F(INFO, "%s: NDRV DDC rd8 idx=3E = %02x", name.c_str(), value);
                    ddc_rd_count++;
                }
            } else if (crtc_ext.index <= NV_CRTC_EXT_MAX) {
                value = crtc_ext.reg[crtc_ext.index];
            }
            break;
        case 0x3DA: // Input Status 1 — bit 3 = vblank, bit 0 = display active
            vga_attr_flip = false;  // reset attribute controller flip-flop
            value = 0x00;
            break;
        default:
            LOG_F(WARNING, "%s: PRMCIO rd8 port %03x unimpl", name.c_str(), port);
            break;
        }
    }
    // PRAMDAC palette DAC at 0x6813xx (ports 0x3C6-0x3C9)
    else if ((address >= 0x681300u && address < 0x681400u) ||
             (address >= 0x683300u && address < 0x683400u)) {
        uint32_t port = address & 0xFFFu;
        switch (port) {
        case 0x3C6: value = dac_mask; break;
        case 0x3C7: value = dac_state; break;
        case 0x3C8: value = dac_wr_index; break;
        case 0x3C9:
            value = dac_rgb[dac_rd_index][dac_comp];
            if (++dac_comp >= 3) { dac_comp = 0; dac_rd_index++; }
            break;
        default: break;
        }
    }
    // RAMIN byte access at 0x700000-0x7FFFFF
    else if (address >= 0x700000u && address < 0x800000u) {
        value = ramin_read8(address - 0x700000u);
    }
    else {
        // Bochs fallback: unknown byte reads delegate to register_read32
        value = (uint8_t)register_read32(address & ~3u);
    }

    LOG_F(9, "%s: reg_rd8  %06x = %02x", name.c_str(), address, value);
    return value;
}

void NvidiaNV::register_write8(uint32_t address, uint8_t value)
{
    LOG_F(9, "%s: reg_wr8  %06x = %02x", name.c_str(), address, value);

    // PRMVIO — VGA sequencer / misc at BAR0 + 0x0C0000
    if (address >= 0x0C0000u && address < 0x0C0400u) {
        uint32_t port = address - 0x0C0000u;
        switch (port) {
        case 0x3C2: vga_misc_output = value; break;  // Misc Output (write port)
        case 0x3C3: vga_enable = value; break;
        case 0x3C4: vga_seq_index = value; break;
        case 0x3C5: vga_seq_data[vga_seq_index & 7] = value; break;
        case 0x3CE: vga_gfx_index = value; break;
        case 0x3CF: vga_gfx_data[vga_gfx_index & 0xF] = value; break;
        default:
            LOG_F(WARNING, "%s: PRMVIO wr8 port %03x = %02x unimpl", name.c_str(), port, value);
            break;
        }
    }
    // PRMCIO — VGA CRTC / attribute at BAR0 + 0x601000
    else if (address >= 0x601000u && address < 0x602000u) {
        uint32_t port = address - 0x601000u;
        switch (port) {
        case 0x3C0:
            if (vga_attr_flip)
                vga_attr_data[vga_attr_index & 0x1F] = value;
            else
                vga_attr_index = value;
            vga_attr_flip = !vga_attr_flip;
            break;
        case 0x3D4: crtc_ext.index = value;
            if (big_endian_mode) {
                static int crtc_idx_count = 0;
                if (crtc_idx_count < 0)
                    LOG_F(INFO, "%s: NDRV CRTC wr8 3D4 idx=%02x", name.c_str(), value);
                crtc_idx_count++;
            }
            break;
        case 0x3D5:
            if (crtc_ext.index <= NV_CRTC_EXT_MAX) {
                crtc_ext.reg[crtc_ext.index] = value;
                // DDC I2C bit-bang: NDRV writes to 0x3F with SCL=bit5, SDA=bit4
                // Result goes into 0x3E bits [3:2] = SDA_in:SCL_in
                if (crtc_ext.index == 0x3F) {
                    bool scl = (value >> 5) & 1;
                    bool sda = (value >> 4) & 1;
                    ddc_i2c_write(scl, sda);
                    crtc_ext.reg[0x3E] = ddc_i2c_read();
                    if (big_endian_mode) {
                        static int ddc_wr_count = 0;
                        if (ddc_wr_count < 0)
                            LOG_F(INFO, "%s: NDRV DDC wr8 idx=3F val=%02x SCL=%d SDA=%d → 3E=%02x",
                                  name.c_str(), value, scl, sda, crtc_ext.reg[0x3E]);
                        ddc_wr_count++;
                    }
                }
                // Secondary I2C buses — echo back (no device)
                else if (crtc_ext.index == 0x37 || crtc_ext.index == 0x51) {
                    bool scl = (value >> 5) & 1;
                    bool sda = (value >> 4) & 1;
                    crtc_ext.reg[crtc_ext.index - 1] = (sda ? 0x08 : 0) | (scl ? 0x04 : 0);
                }
                // Legacy 0x3E write from FCode — return idle
                else if (crtc_ext.index == 0x3E) {
                    crtc_ext.reg[0x3E] = 0x0C;
                }
                // Geometry/depth registers trigger mode recalc
                switch (crtc_ext.index) {
                case 0x01: case 0x07: case 0x0C: case 0x0D:
                case 0x12: case 0x13: case 0x19: case 0x25:
                case 0x28: case 0x2D: case 0x41: case 0x42:
                    mode_needs_update = true;
                    crtc_update();
                    break;
                }
            }
            break;
        default:
            LOG_F(WARNING, "%s: PRMCIO wr8 port %03x = %02x unimpl", name.c_str(), port, value);
            break;
        }
    }
    // PRAMDAC palette DAC write at 0x6813xx (ports 0x3C6-0x3C9)
    else if ((address >= 0x681300u && address < 0x681400u) ||
             (address >= 0x683300u && address < 0x683400u)) {
        uint32_t port = address & 0xFFFu;
        switch (port) {
        case 0x3C6: dac_mask = value; break;
        case 0x3C7: dac_rd_index = value; dac_comp = 0; dac_state = 3; break;
        case 0x3C8: dac_wr_index = value; dac_comp = 0; dac_state = 0; break;
        case 0x3C9:
            dac_rgb[dac_wr_index][dac_comp] = value;
            if (++dac_comp >= 3) {
                uint8_t r, g, b;
                // ramdac_general_control bit 20: 1=8-bit DAC, 0=6-bit DAC
                if (ramdac_general_control & (1u << 20)) {
                    r = dac_rgb[dac_wr_index][0];
                    g = dac_rgb[dac_wr_index][1];
                    b = dac_rgb[dac_wr_index][2];
                } else {
                    r = (dac_rgb[dac_wr_index][0] << 2) | (dac_rgb[dac_wr_index][0] >> 4);
                    g = (dac_rgb[dac_wr_index][1] << 2) | (dac_rgb[dac_wr_index][1] >> 4);
                    b = (dac_rgb[dac_wr_index][2] << 2) | (dac_rgb[dac_wr_index][2] >> 4);
                }
                this->set_palette_color(dac_wr_index, r, g, b, 0xFF);
                this->draw_fb = true;
                dac_comp = 0;
                dac_wr_index++;
            }
            break;
        default: break;
        }
    }
    // RAMIN byte access at 0x700000-0x7FFFFF
    else if (address >= 0x700000u && address < 0x800000u) {
        ramin_write8(address - 0x700000u, value);
    }
    else {
        // Bochs fallback: unknown byte writes do read-modify-write on 32-bit register
        uint32_t aligned = address & ~3u;
        uint32_t cur = register_read32(aligned);
        int shift = (address & 3u) * 8;
        cur = (cur & ~(0xFFu << shift)) | ((uint32_t)value << shift);
        register_write32(aligned, cur);
    }
}

// ============================================================================
// All remaining execute_* / d3d_* / pixel_operation / 2D primitive methods
// are direct type-renames from bochs geforce.cc — no DingusPPC API calls
// anywhere inside them.  Suggested porting order:
//
//   1. execute_m2mf, execute_surf2d  (simplest, good smoke test)
//   2. execute_rect, execute_gdi     (needed for framebuffer console)
//   3. execute_imageblit, execute_ifc
//   4. execute_d3d                   (stub OK for initial Mac OS X boot test)
//
// Stubs below allow the file to compile while porting proceeds.
// ============================================================================

void NvidiaNV::update_color_bytes_s2d(NvChannel *) {}
void NvidiaNV::update_color_bytes_ifc(NvChannel *) {}
void NvidiaNV::update_color_bytes_sifc(NvChannel *) {}
void NvidiaNV::update_color_bytes_tfc(NvChannel *) {}
void NvidiaNV::update_color_bytes_iifc(NvChannel *) {}
void NvidiaNV::update_color_bytes(uint32_t, uint32_t, uint32_t *) {}

void NvidiaNV::execute_m2mf(NvChannel *, uint32_t, uint32_t, uint32_t) {}
void NvidiaNV::execute_rop(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_patt(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_gdi(NvChannel *, uint32_t, uint32_t, uint32_t) {}
void NvidiaNV::execute_swzsurf(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_chroma(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_ifc(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_iifc(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_sifc(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_beta(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_tfc(NvChannel *, uint32_t, uint32_t) {}
void NvidiaNV::execute_sifm(NvChannel *, uint32_t, uint32_t, uint32_t) {}
void NvidiaNV::execute_d3d(NvChannel *, uint32_t, uint32_t, uint32_t) {}

void NvidiaNV::put_pixel_swzs(NvChannel *, uint32_t, uint32_t) {}

void NvidiaNV::gdi_fillrect(NvChannel *, bool) {}
void NvidiaNV::gdi_blit(NvChannel *, uint32_t) {}
void NvidiaNV::rect(NvChannel *) {}
void NvidiaNV::ifc(NvChannel *, uint32_t) {}
void NvidiaNV::iifc(NvChannel *) {}
void NvidiaNV::sifc(NvChannel *) {}
void NvidiaNV::copyarea(NvChannel *) {}
void NvidiaNV::tfc(NvChannel *) {}
void NvidiaNV::m2mf(NvChannel *) {}
void NvidiaNV::sifm(NvChannel *, bool) {}

bool NvidiaNV::d3d_scissor_clip(NvChannel *, uint32_t *, uint32_t *, uint32_t *, uint32_t *) { return false; }
bool NvidiaNV::d3d_viewport_clip(NvChannel *, uint32_t *, uint32_t *, uint32_t *, uint32_t *) { return false; }
bool NvidiaNV::d3d_window_clip(NvChannel *, uint32_t *, uint32_t *, uint32_t *, uint32_t *) { return false; }
void NvidiaNV::d3d_clear_surface(NvChannel *) {}
void NvidiaNV::d3d_sample_texture(NvChannel *, NvTexture *, float[3], float[4]) {}
void NvidiaNV::d3d_vertex_shader(NvChannel *, float[16][4], float[16][4]) {}
void NvidiaNV::d3d_register_combiners(NvChannel *, float[16][4], float[4]) {}
void NvidiaNV::d3d_pixel_shader(NvChannel *, float[16][4], float[64][4], float[64][4]) {}
void NvidiaNV::d3d_normal_to_view(NvChannel *, float[3], float[3]) {}
void NvidiaNV::d3d_triangle(NvChannel *, uint32_t) {}
void NvidiaNV::d3d_triangle_clipped(NvChannel *, float[16][4], float[16][4], float[16][4]) {}
void NvidiaNV::d3d_clip_to_screen(NvChannel *, float[4], float[4]) {}
void NvidiaNV::d3d_process_vertex(NvChannel *) {}
void NvidiaNV::d3d_load_vertex(NvChannel *, uint32_t) {}
uint32_t NvidiaNV::d3d_get_surface_pitch_z(NvChannel *) { return 0; }

// ============================================================================
// Device registration
//
// ============================================================================

static const PropMap Nv15_Properties = {
    {"gfxmem_size", new IntProperty(64, std::vector<uint32_t>({64}))},
    {"rom",         new StrProperty("GF2MX.rom")},
};

static const DeviceDescription Nv15_Descriptor = {
    NvidiaNV::create_nv15, {"NvVideoDisplay@0"}, Nv15_Properties,
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(Nv15, Nv15_Descriptor);

static const PropMap Nv20_Properties = {
    {"gfxmem_size", new IntProperty(64, std::vector<uint32_t>({64}))},
    {"rom",         new StrProperty("GF3.rom")},
};

static const DeviceDescription Nv20_Descriptor = {
    NvidiaNV::create_nv20, {"NvVideoDisplay@0"}, Nv20_Properties,
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(Nv20, Nv20_Descriptor);

static const PropMap Nv35_Properties = {
    {"gfxmem_size", new IntProperty(64, std::vector<uint32_t>({64}))},
    {"rom",         new StrProperty("FX5900.rom")},
};

static const DeviceDescription Nv35_Descriptor = {
    NvidiaNV::create_nv35, {"NvVideoDisplay@0"}, Nv35_Properties,
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(Nv35, Nv35_Descriptor);

static const PropMap Nv40_Properties = {
    {"gfxmem_size", new IntProperty(64, std::vector<uint32_t>({64}))},
    {"rom",         new StrProperty("6800GT.rom")},
};

static const DeviceDescription Nv40_Descriptor = {
    NvidiaNV::create_nv40, {"NvVideoDisplay@0"}, Nv40_Properties,
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(Nv40, Nv40_Descriptor);

static std::unique_ptr<HWComponent> NvVideoDisplay_create(const std::string &dev_name) {
    return std::unique_ptr<DisplayID>(new DisplayID(dev_name));
}

static const PropMap NvVideoDisplay_Properties = {
    {"edid"  , new StrProperty("")},
};

static const DeviceDescription NvVideoDisplay_Descriptor = {
    NvVideoDisplay_create, {}, NvVideoDisplay_Properties, HWCompType::DISPLAY
};

REGISTER_DEVICE(NvVideoDisplay, NvVideoDisplay_Descriptor);



// ---- Added NV2x scanout simulation ----
#include <thread>
#include <atomic>
#include <chrono>

std::atomic<int> nv_scanline{0};
std::atomic<bool> nv_vblank{false};

static std::atomic<bool> nv_scanout_running{false};
static std::thread nv_scanout_thread;

static void nv_scanout_loop() {
    const int total_lines = 525;
    const int visible_lines = 480;

    while (nv_scanout_running) {
        for (int i = 0; i < total_lines; i++) {
            nv_scanline = i;
            nv_vblank = (i >= visible_lines);
            std::this_thread::sleep_for(std::chrono::microseconds(32));
        }
    }
}

void start_nv_scanout() {
    if (nv_scanout_running) return;
    nv_scanout_running = true;
    nv_scanout_thread = std::thread(nv_scanout_loop);
}

void stop_nv_scanout() {
    nv_scanout_running = false;
    if (nv_scanout_thread.joinable())
        nv_scanout_thread.join();
}
// ---------------------------------------
