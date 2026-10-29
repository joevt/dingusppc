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

/** Platinum Memory/Display Controller emulation. */

#include <core/memaccess.h>
#include <core/timermanager.h>
#include <cpu/ppc/ppcmmu.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/platinum.h>
#include <devices/video/displayid.h>
#include <loguru.hpp>

#include <cinttypes>

namespace loguru {
    enum : Verbosity {
        Verbosity_PLATOP = loguru::Verbosity_9,
    };
}

using namespace Platinum;

PlatinumCtrl::PlatinumCtrl(const std::string &dev_name)
    : MemCtrlBase(), VideoCtrlBase(), HWComponent(dev_name)
{
    supports_types(HWCompType::MEM_CTRL | HWCompType::MMIO_DEV | HWCompType::VIDEO_CTRL);

    // add MMIO region for VRAM
    this->add_mmio_region(VRAM_REGION_BASE, 0x01000000, this);

    // add MMIO region for the configuration and status registers
    this->add_mmio_region(PLATINUM_IOREG_BASE, 0x500, this);

    // get boot rom device
    boot_rom = dynamic_cast<BootRom*>(
        gMachineObj->get_comp_by_type(HWCompType::ROM));

    // initialize the CPUID register with the following CPU:
    // PowerPC 601 @ 90 MHz, bus frequency: 45 MHz
    this->cpu_id =
        ( 0x3 << 28) | // Design Center: 3 = High-End RISC Design Center
        (   0 << 24) | // Upper ID: 0 = machine with registers wider than 8-bit
        (0x01 << 16) | // Lower ID: 1 = Platinum-based machine
        ClkSrc2      | // Clock Source: bits 15 and 6
        (   0 << 12) | // Motherboard ID: Catalyst motherboard with 1 MB of VRAM
        (CpuSpeed2::CPU_90_BUS_45 << 8)
                     | // System/CPU Speed
        (   0 <<  7) ; // CPU Type: 0 = 601, 1 = 603/604

    // attach DACula RAMDAC
    this->dacula = new AppleRamdac(DacFlavour::DACULA);
    this->add_device(0x1B000, this->dacula);
    this->dacula->get_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        uint8_t a;
        this->get_palette_color(index, colors[0], colors[1], colors[2], a);
    };
    this->dacula->set_clut_entry_cb = [this](uint8_t index, uint8_t *colors) {
        this->set_palette_color(index, colors[0], colors[1], colors[2], 0xFF);
        this->draw_fb = true;
    };
    this->dacula->cursor_ctrl_cb = [this](bool cursor_on) {
        this->draw_fb = true;
        this->update_fb_ptr();
        if (cursor_on) {
            this->dacula->measure_hw_cursor(this->fb_ptr - 16);
            this->cursor_ovl_cb = [this](uint8_t *dst_buf, int dst_pitch) {
                this->dacula->draw_hw_cursor(this->fb_ptr - 16,
                    dst_buf, dst_pitch);
            };
        } else {
            this->cursor_ovl_cb = nullptr;
        }
    };
    this->dacula->cursor_update_cb = [this]() {
        this->draw_fb = true;
    };
    this->dacula->buffer_update_cb = [this]() {
        this->draw_fb = true;
        this->update_fb_ptr();
    };
    this->dacula->video_mode_ctrl_cb = [this]() {
        this->change_display();
    };

    this->draw_fb_is_dynamic = true;
}

PostInitResultType PlatinumCtrl::device_postinit() {
    // attach IOBus Device #2 0xF301B000 ; register DACula with the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentralCatalyst"));
    this->dacula->move_device(gc_obj);

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_int(IntSrc::PLATINUM);

    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state) {
            // vertical blank start is line 0
            this->enable_cursor_int();
            this->enable_animate_int();
        }
        this->update_irq(irq_line_state, SWATCH_INT_VBL);
    };

    return PI_SUCCESS;
}

HWComponent* PlatinumCtrl::set_property(const std::string &property, const std::string &value, int32_t unit_address) {
    if (unit_address == -1) {
        if (property == "gfxmem_size") {
            if (this->override_property(property, value)) {
                // get VRAM size
                int vram = this->get_property_int("gfxmem_size");
                LOG_F(INFO, "%s: setting VRAM to %d MB", this->get_name_and_unit_address().c_str(), vram);
                this->vram_megs = vram;
                this->vram_size = vram << 20;

                // enable half bank access if 1MB VRAM + FB_CONFIG_1[CFG1_FULL_BANKS] = 1
                this->half_bank = !!(vram == 1);
                this->half_access = 0;

                // allocate VRAM
                this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size]);

                return this;
            }
        }
    }
    return nullptr;
}

uint32_t PlatinumCtrl::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t value;

    if (rgn_start == VRAM_REGION_BASE) {
        if (offset < this->vram_size) {
            // HACK: half bank configurations should return invalid data
            // for the lower DWORD (in the PPC order!) to be recognized.
            // The simplest way to achieve that is to redirect access
            // to the upper DWORD by setting bit 2 of the address.
            if (this->half_access)
                offset |= 4;
            return read_mem(&this->vram_ptr[offset], size);
        } else {
            LOG_F(WARNING, "%s: read from unmapped aperture address 0x%X",
                  this->name.c_str(), VRAM_REGION_BASE + offset);
            return (uint32_t)-1;
        }
    }

    switch (offset >> 4) {
    case PlatinumReg::CPU_ID:
        value = this->cpu_id;
        break;
    case PlatinumReg::ASIC_REVISION:
        value =
            (3<<28) | // Revision: 3
            (0<<24);  // Vendor ID: 0=VTI, 1=TI
        break;
    case PlatinumReg::ROM_TIMING:
        value = (this->rom_we << 4) | this->rom_timing;
        break;
    case PlatinumReg::CACHE_CONFIG:
        value = this->cache_config;
        break;
    case PlatinumReg::DRAM_TIMING:
        value = this->dram_timing;
        break;
    case PlatinumReg::DRAM_REFRESH:
        value = this->dram_refresh;
        break;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        value = this->bank_base[(offset >> 4) - PlatinumReg::BANK_0_BASE];
        break;
    case PlatinumReg::GP_SW_SCRATCH:
        value = this->gp_sw_scratch;
        break;
    case PlatinumReg::PCI_ADDR_MASK:
        value = this->pci_address_mask;
        break;
    case PlatinumReg::FB_BASE_ADDR:
        value = Platinum::VRAM_REGION_BASE | this->fb_addr;
        break;
    case PlatinumReg::ROW_WORDS:
        value = this->row_words;
        break;
    case PlatinumReg::CLOCK_DIVISOR:
        value = this->clock_divisor;
        break;
    case PlatinumReg::FB_CONFIG_1:
        value =  this->fb_config_1;
        break;
    case PlatinumReg::FB_CONFIG_2:
        value = this->fb_config_2;
        break;
    case PlatinumReg::VMEM_PAGE_MODE:
        value = this->vmem_fp_mode;
        break;
    case PlatinumReg::MON_ID_SENSE:
        value = this->mon_sense;
        break;
    case PlatinumReg::FB_RESET:
        value = this->fb_reset;
        break;
    case PlatinumReg::DBL_BUF_CNTL:
        value = (this->rw_buf_sel << 2) | (this->disp_buf_sel << 1) | this->dbl_buf_en;
        break;
    case PlatinumReg::FB_TEST:
        value = this->fb_test;
        break;
    case PlatinumReg::VRAM_REFRESH:
        value = this->vram_refresh;
        break;
    case PlatinumReg::SWATCH_CONFIG:
        value = this->swatch_config;
        break;
    case PlatinumReg::SWATCH_INT_MASK:
        value = this->swatch_int_mask;
        break;
    case PlatinumReg::SWATCH_INT_STAT:
        value = this->swatch_int_stat;
        break;
    case PlatinumReg::CLR_CURSOR_INT:
        this->update_irq(0, SWATCH_INT_CURSOR);
        value = 0;
        break;
    case PlatinumReg::CLR_ANIM_INT:
        value = 0;
        this->update_irq(0, SWATCH_INT_ANIM);
        break;
    case PlatinumReg::CLR_VBL_INT:
        value = 0;
        this->update_irq(0, SWATCH_INT_VBL);
        break;
    case PlatinumReg::CURSOR_LINE:
        value = this->cursor_line;
        break;
    case PlatinumReg::ANIMATE_LINE:
        value = this->animate_line;
        break;
    case PlatinumReg::COUNTER_TEST:
        value = this->counter_test;
        break;
    case PlatinumReg::SWATCH_HSERR:
    case PlatinumReg::SWATCH_HLFLN:
    case PlatinumReg::SWATCH_HEQ:
    case PlatinumReg::SWATCH_HSP:
    case PlatinumReg::SWATCH_HBWAY:
    case PlatinumReg::SWATCH_HBRST:
    case PlatinumReg::SWATCH_HBP:
    case PlatinumReg::SWATCH_HAL:
    case PlatinumReg::SWATCH_HFP:
    case PlatinumReg::SWATCH_HPIX:
    case PlatinumReg::SWATCH_VHLINE:
    case PlatinumReg::SWATCH_VSYNC:
    case PlatinumReg::SWATCH_VBPEQ:
    case PlatinumReg::SWATCH_VBP:
    case PlatinumReg::SWATCH_VAL:
    case PlatinumReg::SWATCH_VFP:
    case PlatinumReg::SWATCH_VFPEQ:
        value = this->swatch_params[REG_TO_INDEX(offset >> 4)];
        break;
    case PlatinumReg::TIMING_ADJUST:
        value = this->timing_adjust;
        break;
    case PlatinumReg::CURRENT_LINE:
        value = this->pixel_clock * (TimerManager::get_instance()->current_time_ns() - this->frame_start_time) /
            (this->hori_total * 1000);
        break;
    case PlatinumReg::FG_COLOR:
        value = this->qda_fg_color;
        break;
    case PlatinumReg::BG_COLOR:
        value = this->qda_bg_color;
        break;
    case PlatinumReg::SRC_BASE:
        value = this->qda_src_base;
        break;
    case PlatinumReg::DST_BASE:
        value = this->qda_dst_base;
        break;
    case PlatinumReg::ROW_BYTES:
        value = (this->qda_dst_row_bytes << 16) | this->qda_src_row_bytes;
        break;
    case PlatinumReg::DST_SIZE:
        value = (this->qda_dst_v_size << 16) | this->qda_dst_h_size;
        break;
    case PlatinumReg::QDA_CNTL:
        value =
            (uint32_t(this->qda_processing_dir) << 21) |
            (uint32_t(this->qda_dst_depth) << 19) |
            (this->qda_bit_offset << 16) |
            (this->qda_pattern_v_pixels << 8) |
            this->qda_pattern_h_pixels;
        break;
    case PlatinumReg::QDA_CMD:
        value =
            ((int)this->qda_interrupt_enable << 31) |
            ((int)this->qda_last_command << 30) |
            ((int)this->qda_expand << 18) |
            ((int)this->qda_colorize << 17) |
            ((int)this->qda_fill << 16) |
            ((int)this->qda_pattern_operation << 3) |
             (int)this->qda_pattern_op_rop;
        break;
    case PlatinumReg::QDA_STATUS:
        value =
            (this->qda_cmd_in_progress << 31) |
            (this->qda_interrupt_status << 30) |
            (this->qda_go_multiple << 1) |
            this->qda_go;
        break;
    case PlatinumReg::QDA_PTR:
        value = this->qda_ptr;
        break;
    case PlatinumReg::IRIDIUM_CONFIG:
        value = this->iridium_cfg;
        break;
    case PlatinumReg::POWER_DOWN_CTRL:
        value = this->power_down_ctrl;
        break;
    default:
        LOG_F(WARNING, "%s: unknown register read at offset 0x%X", this->name.c_str(),
              offset);
        value = 0;
    }

    if (size == 4 && !(offset & 3))
        return value;
    else
        return extract_with_wrap_around(value, offset, size);
}

void PlatinumCtrl::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    static uint8_t vid_enable_seq[] = {3, 2, 0};

    uint8_t mon_dirs = 0;
    uint8_t mon_levels = 0;

    if (rgn_start == VRAM_REGION_BASE) {
        if (offset < this->vram_size) {
            this->draw_fb = true;
            write_mem(&this->vram_ptr[offset], value, size);
        } else
            LOG_F(WARNING, "%s: write to unmapped aperture address 0x%X",
                  this->name.c_str(), VRAM_REGION_BASE + offset);
        return;
    }

    switch (offset >> 4) {
    case PlatinumReg::ROM_TIMING: {
        bool new_rom_we = (value >> 4) & 1;
        if (new_rom_we != this->rom_we) {
            boot_rom->set_rom_write_enable(new_rom_we);
        }
        this->rom_timing = value & 0xF;
        this->rom_we = new_rom_we;
        break;
    }
    case PlatinumReg::CACHE_CONFIG:
        this->cache_config = (this->cache_config & ~7) | (value & 7);
        break;
    case PlatinumReg::DRAM_TIMING:
        this->dram_timing = value & 0x1FFF;
        break;
    case PlatinumReg::DRAM_REFRESH:
        this->dram_refresh = value & 0x7FF;
        break;
    case PlatinumReg::BANK_0_BASE:
    case PlatinumReg::BANK_1_BASE:
    case PlatinumReg::BANK_2_BASE:
    case PlatinumReg::BANK_3_BASE:
    case PlatinumReg::BANK_4_BASE:
    case PlatinumReg::BANK_5_BASE:
    case PlatinumReg::BANK_6_BASE:
    case PlatinumReg::BANK_7_BASE:
        this->bank_base[(offset >> 4) - PlatinumReg::BANK_0_BASE] = value & 0xFF700000U;
        this->map_phys_ram();
        break;
    case PlatinumReg::GP_SW_SCRATCH:
        this->gp_sw_scratch = value;
        break;
    case PlatinumReg::PCI_ADDR_MASK:
        this->pci_address_mask = value;
        break;
    case PlatinumReg::FB_BASE_ADDR:
    {
        // These are the bits that will be stored to the FB_BASE_ADDR register.
        uint32_t new_addr = value & 0x3FFFE0;

        // This is a hint from the guest OS that we'll use because we don't know
        //  how to calculate base address without it.
        uint32_t new_offset = value & 0x10;

        if (this->fb_addr != new_addr || this->fb_offset != new_offset) {
            this->fb_addr = new_addr;
            this->fb_offset = new_offset;
            this->change_display();
        }
        break;
    }
    case PlatinumReg::ROW_WORDS:
        this->row_words = value & 0x7FF8;
        this->change_display();
        break;
    case PlatinumReg::CLOCK_DIVISOR:
        this->clock_divisor = value & 0xFFU;
        this->change_display();
        break;
    case PlatinumReg::FB_CONFIG_1:
        LOG_F(INFO, "%s: write FB_CONFIG_1 %03x.%c = %0*x", this->name.c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->fb_config_1 =
            (this->fb_config_1 & ~(CFG1_FULL_BANKS | CFG1_VID_ENABLE | CFG1_INTERLACE | CFG1_SAM_512BIT | CFG1_VRAMS_2MBIT)) |
            (value             &  (CFG1_FULL_BANKS | CFG1_VID_ENABLE | CFG1_INTERLACE | CFG1_SAM_512BIT | CFG1_VRAMS_2MBIT));
        this->half_bank = !!(this->vram_megs == 1 && (value & CFG1_FULL_BANKS));
        break;
    case PlatinumReg::FB_CONFIG_2:
        this->fb_config_2 = value & 0x1FFF;
        break;
    case PlatinumReg::VMEM_PAGE_MODE:
        this->vmem_fp_mode = value & 1;
        break;
    case PlatinumReg::MON_ID_SENSE:
        value &= 7;
        mon_dirs        = value ^ 7;
        mon_levels      = (this->fb_test >> SENSE_LINE_OUTPUT_DATA_pos) & 7;
        mon_levels      = (mon_levels & mon_dirs) | (mon_dirs ^ 7);
        this->mon_sense = (mon_dirs << 3) |
            (this->disp_id->read_monitor_sense(mon_levels, mon_dirs) ^ 7);
        break;
    case PlatinumReg::FB_RESET:
        value &= 7;
        if (value == 7 && this->crtc_on) {
            LOG_F(INFO, "%s: video disabled", this->name.c_str());
            this->reset_step = 0;
        } else if (value == vid_enable_seq[this->reset_step]) {
            if (++this->reset_step >= 3) {
                if (this->fb_config_1 & CFG1_VID_ENABLE) {
                    LOG_F(INFO, "%s: video enabled", this->name.c_str());
                    this->enable_display();
                } else {
                    this->disable_display();
                }
                this->reset_step = 0;
            }
        } else
            this->reset_step = 0;
        this->fb_reset = value;
        this->half_access = !!(this->half_bank && value == 6);

        if (this->fb_reset == 7) {
            this->disable_display();
        }
        else if (this->fb_reset == 0) {
            this->change_display();
        }
        else {
            LOG_F(ERROR, "%s: display reset written with 0x%X", this->name.c_str(), value);
        }

        break;
    case PlatinumReg::DBL_BUF_CNTL:
        this->rw_buf_sel   = extract_bits(value, 2, 1);
        this->disp_buf_sel = extract_bits(value, 1, 1);
        this->dbl_buf_en   = extract_bits(value, 0, 1);
        break;
    case PlatinumReg::FB_TEST:
        this->fb_test =
            (this->fb_test & ~(SENSE_LINE_OUTPUT_DATA | SYNCS_OUTPUT_ENABLE | FORCED_SYNC_LEVELS)) |
            (value         &  (SENSE_LINE_OUTPUT_DATA | SYNCS_OUTPUT_ENABLE | FORCED_SYNC_LEVELS));
        break;
    case PlatinumReg::VRAM_REFRESH:
        this->vram_refresh = value & 0x7FF;
        break;
    case PlatinumReg::SWATCH_CONFIG:
        this->swatch_config = (this->swatch_config & ~0xFFU) | (value & 0xFFU);
        break;
    case PlatinumReg::SWATCH_INT_MASK: {
        uint32_t old_value = this->swatch_int_mask;
        this->swatch_int_mask = value & 7;
        if ((value ^ old_value) & SWATCH_INT_CURSOR) {
            LOG_F(INFO, "%s: Cursor interrupt %sabled", this->name.c_str(), (value & SWATCH_INT_CURSOR) ? "en" : "dis");
            this->enable_cursor_int();
        }
        if ((value ^ old_value) & SWATCH_INT_ANIM) {
            LOG_F(INFO, "%s: Anim interrupt %sabled", this->name.c_str(), (value & SWATCH_INT_ANIM) ? "en" : "dis");
            this->enable_animate_int();
        }
        if ((value ^ old_value) & SWATCH_INT_VBL) {
            LOG_F(INFO, "%s: VBL interrupt %sabled", this->name.c_str(), (value & SWATCH_INT_VBL) ? "en" : "dis");
        }
        break;
    }
    case PlatinumReg::SWATCH_INT_STAT:
        break;
    case PlatinumReg::CLR_CURSOR_INT:
        this->update_irq(0, SWATCH_INT_CURSOR);
        break;
    case PlatinumReg::CLR_ANIM_INT:
        this->update_irq(0, SWATCH_INT_ANIM);
        break;
    case PlatinumReg::CLR_VBL_INT:
        this->update_irq(0, SWATCH_INT_VBL);
        break;
    case PlatinumReg::CURSOR_LINE:
        this->cursor_line = value & 0x7FF;
        this->enable_cursor_int();
        break;
    case PlatinumReg::ANIMATE_LINE:
        this->cursor_line = value & 0x7FF;
        this->enable_animate_int();
        break;
    case PlatinumReg::COUNTER_TEST:
        this->counter_test = value & 0xFFF;
        break;
    case PlatinumReg::SWATCH_HSERR:
    case PlatinumReg::SWATCH_HLFLN:
    case PlatinumReg::SWATCH_HSP:
    case PlatinumReg::SWATCH_HBWAY:
    case PlatinumReg::SWATCH_HBRST:
    case PlatinumReg::SWATCH_HBP:
    case PlatinumReg::SWATCH_HAL:
    case PlatinumReg::SWATCH_HFP:
    case PlatinumReg::SWATCH_HPIX:
    case PlatinumReg::SWATCH_VHLINE:
    case PlatinumReg::SWATCH_VSYNC:
    case PlatinumReg::SWATCH_VBPEQ:
    case PlatinumReg::SWATCH_VBP:
    case PlatinumReg::SWATCH_VAL:
    case PlatinumReg::SWATCH_VFP:
    case PlatinumReg::SWATCH_VFPEQ:
        this->swatch_params[REG_TO_INDEX(offset >> 4)] = value & 0xFFF;
        this->change_display();
        break;
    case PlatinumReg::SWATCH_HEQ:
        this->swatch_params[REG_TO_INDEX(offset >> 4)] = value & 0xFF;
        this->change_display();
        break;
    case PlatinumReg::TIMING_ADJUST:
        this->timing_adjust = value & 0xFFF;
        break;
    case PlatinumReg::FG_COLOR:
        this->qda_fg_color = value;
        break;
    case PlatinumReg::BG_COLOR:
        this->qda_bg_color = value;
        break;
    case PlatinumReg::SRC_BASE:
        this->qda_src_base = value;
        break;
    case PlatinumReg::DST_BASE:
        this->qda_dst_base = value;
        break;
    case PlatinumReg::ROW_BYTES:
        this->qda_dst_row_bytes = extract_bits(value, 16, 16);
        this->qda_src_row_bytes = extract_bits(value, 0, 16);
        break;
    case PlatinumReg::DST_SIZE:
        this->qda_dst_v_size = extract_bits(value, 16, 16);
        this->qda_dst_h_size = extract_bits(value, 0, 16);
        break;
    case PlatinumReg::QDA_CNTL:
        this->qda_processing_dir = QDA_DIR(extract_bits(value, 21, 1));
        this->qda_dst_depth = QDA_DEPTH(extract_bits(value, 19, 2));
        this->qda_bit_offset = extract_bits(value, 16, 3);
        this->qda_pattern_v_pixels = extract_bits(value, 8, 6);
        this->qda_pattern_h_pixels = extract_bits(value, 0, 6);
        break;
    case PlatinumReg::QDA_CMD:
        this->qda_interrupt_enable = extract_bits(value, 31, 1);
        this->qda_last_command = extract_bits(value, 30, 1);
        this->qda_expand = (Platinum::QDA_EXPAND)extract_bits(value, 18, 1);
        this->qda_colorize = (Platinum::QDA_COLORIZE)extract_bits(value, 17, 1);
        this->qda_fill = (Platinum::QDA_FILL)extract_bits(value, 16, 1);
        this->qda_pattern_operation = (Platinum::QDA_PAT)extract_bits(value, 3, 1);
        this->qda_pattern_op_rop = (Platinum::QDA_ROP)extract_bits(value, 0, 3);
        break;
    case PlatinumReg::QDA_STATUS:
        this->qda_go_multiple = extract_bits(value, 1, 1);
        if (this->qda_go_multiple)
            LOG_F(ERROR, "%s: go multiple is not implemented", this->name.c_str());
        this->qda_go = extract_bits(value, 0, 1);
        if (this->qda_go && !qda_timer.active) {
            TimerManager::get_instance()->add_immediate_timer(this->qda_timer,
                [this](uint64_t, uint64_t) {
                    this->do_qda();
                }
            );
        } else {
            LOG_F(ERROR, "%s: qda command already in progress!", this->name.c_str());
        }
        break;
    case PlatinumReg::QDA_PTR:
        this->qda_ptr = value;
        if (this->qda_ptr)
            LOG_F(ERROR, "%s: command list register is not implemented", this->name.c_str());
        break;
    case PlatinumReg::IRIDIUM_CONFIG:
        if (!(value & 1))
            LOG_F(ERROR, "%s: little-endian system bus is not implemented", this->name.c_str());
        this->iridium_cfg = (this->iridium_cfg & ~7) | (value & 7);
        break;
    case PlatinumReg::POWER_DOWN_CTRL:
        this->power_down_ctrl = value;
        if (value & 1)
            LOG_F(INFO, "%s: power down mode enabled", this->name.c_str());
        break;
    default:
        LOG_F(WARNING, "%s: unknown register write at offset 0x%X", this->name.c_str(),
              offset);
    }
}

void PlatinumCtrl::insert_ram_dimm(int slot_num, uint32_t capacity) {
    if (slot_num < 0 || slot_num >= 4) {
        ABORT_F("%s: invalid DIMM slot %d", this->name.c_str(), slot_num);
    }

    switch (capacity) {
    case 0:
        break;
    case DRAM_CAP_2MB:
    case DRAM_CAP_4MB:
    case DRAM_CAP_8MB:
    case DRAM_CAP_16MB:
    case DRAM_CAP_32MB:
    case DRAM_CAP_64MB:
        this->bank_size[slot_num * 2 + 0] = capacity;
        break;
    case DRAM_CAP_128MB:
    case DRAM_CAP_256MB:
    case DRAM_CAP_512MB:
        this->bank_size[slot_num * 2 + 0] = capacity / 2;
        this->bank_size[slot_num * 2 + 1] = capacity / 2;
        break;
    default:
        ABORT_F("%s: unsupported DRAM capacity %d", this->name.c_str(), capacity);
    }
}

void PlatinumCtrl::map_phys_ram() {
    uint32_t total_ram = 0;

    for (int i = 0; i < 8; i++) {
        total_ram += this->bank_size[i];
    }

    if (!this->dram_ptr) {
        this->dram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[total_ram]()); /* () intializer clears the memory to zero */
        if (!this->dram_ptr) {
            ABORT_F("%s: could not allocate RAM storage", this->name.c_str());
        }
    }

    ram_map.erase(std::remove_if(ram_map.begin(), ram_map.end(),
        [this](AddressMapEntry *entry) {
            delete this->remove_region(entry);
            return true;
        }
    ), ram_map.end());

    uint32_t ram_offset = 0;
    uint32_t ram_end = 0;
    while (1) {
        // find first bank that exists after the last added region
        uint32_t ram_start = -1;
        for (int bank = 0; bank < 8; bank++) {
            if (
                this->bank_size[bank] && // has size
                this->bank_base[bank] >= ram_end && // starts after the last ending
                this->bank_base[bank] < ram_start // has earlier start
            ) {
                ram_start = this->bank_base[bank];
            }
        }
        if (ram_start + 1 == 0)
            break;
        ram_end = ram_start;
        // find the max size of this region
        for (int bank = 0; bank < 8; bank++) {
            if (
                this->bank_size[bank] && // has size
                this->bank_base[bank] <= ram_end && // starts before the end
                this->bank_base[bank] + this->bank_size[bank] > ram_end // ends after the end
            ) {
                ram_end = this->bank_base[bank] + this->bank_size[bank];
            }
        }
        AddressMapEntry *entry = this->add_ram_region(ram_start, ram_end - ram_start, &dram_ptr[ram_offset]);
        if (entry) {
            ram_map.push_back(entry);
        } else {
            ABORT_F("%s: could not allocate RAM storage", this->name.c_str());
        }
        ram_offset += ram_end - ram_start;
    }
}

// ====================== Framebuffer controller stuff =======================
void PlatinumCtrl::update_fb_ptr()
{
    // FIXME: Calculate fb_ptr without using fb_offset. Similar problem as ControlVideo::update_fb_ptr.
    this->fb_ptr = &this->vram_ptr[this->fb_addr] + 16 + this->fb_offset;
#if 0
    // FIXME: Implement double buffer
    if (this->dacula->get_dbl_buf_cr() == 0 && this->vram_banks == 3) {
        this->fb_ptr += 0x200000;
    }
#endif
}

void PlatinumCtrl::enable_display() {
    bool did_size_change = false;
    bool did_refresh_rate_change = false;
    bool did_depth_change = false;

    int clock_divisor = this->dacula->get_clock_div();

    this->pixel_clock = this->dacula->get_dot_freq();

    // calculate active_width and active_height from Swatch parameters
    int new_width  = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HFP)] -
                     swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HAL)];
    int new_height = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VFP)] -
                     swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VAL)];

    this->hori_blank = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HAL)] +
                      (swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HSP)] -
                       swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_HFP)]);

    new_width        *= clock_divisor;
    this->hori_blank *= clock_divisor;

    this->vert_blank = swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VAL)] +
                      (swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VSYNC)] -
                       swatch_params[REG_TO_INDEX(PlatinumReg::SWATCH_VFP)]);

    if (!(this->fb_config_1 & CFG1_INTERLACE)) {
        new_height       >>= 1;
        this->vert_blank >>= 1;
    }

    if (new_width != this->active_width || new_height != this->active_height)
        did_size_change = true;
    this->active_width  = new_width;
    this->active_height = new_height;

    this->hori_total = this->hori_blank + new_width;
    this->vert_total = this->vert_blank + new_height;

    // set framebuffer parameters
    this->update_fb_ptr();
    this->fb_pitch = this->row_words;
    if (this->fb_config_1 & CFG1_INTERLACE) {
        this->fb_pitch >>= 1;
    }

    int new_pixel_depth = this->dacula->get_pix_width();
    if (new_pixel_depth != this->pixel_depth)
        did_depth_change = true;
    this->pixel_depth = new_pixel_depth;

    // attach framebuffer conversion routine
    switch (this->pixel_depth) {
    case 8:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_fb = false;
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 16:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_fb = false;
            this->convert_frame_15bpp<BE>(dst_buf, dst_pitch);
        };
        break;
    case 32:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_fb = false;
            this->convert_frame_32bpp<BE>(dst_buf, dst_pitch);
        };
        break;
    default:
        LOG_F(ERROR, "%s: invalid pixel width %d", this->name.c_str(), this->pixel_depth);
    }

    this->dacula->set_fb_parameters(this->active_width, this->active_height, this->fb_pitch);

    this->stop_refresh_task();

    if (this->active_width <= 0 || this->active_height <= 0 || this->pixel_clock <= 0) {
        this->blank_on = true;
        this->crtc_on = false;
        this->blank_display();
        return;
    }

    float new_refresh_rate = (double)(this->pixel_clock) / (this->hori_total * this->vert_total);
    if (this->fb_config_1 & CFG1_INTERLACE) {
        new_refresh_rate *= 2;
    }

    if (new_refresh_rate != this->refresh_rate)
        did_refresh_rate_change = true;
    this->refresh_rate = new_refresh_rate;

    this->start_refresh_task();

    if (did_size_change)
        LOG_F(INFO, "%s: video width=%d, height=%d", this->name.c_str(), new_width, new_height);
    if (did_depth_change)
        LOG_F(INFO, "%s: video depth=%d", this->name.c_str(), new_pixel_depth);
    if (did_refresh_rate_change)
        LOG_F(INFO, "%s: refresh rate set to %f Hz", this->name.c_str(), this->refresh_rate);
    if (this->blank_on == true || this->crtc_on == false)
        LOG_F(INFO, "%s: display enabled", this->name.c_str());

    this->blank_on = false;
    this->crtc_on = true;
    this->draw_fb = true;
}

void PlatinumCtrl::disable_display() {
    this->crtc_on = false;
    this->blank_display();
}

void PlatinumCtrl::change_display() {
    if (this->fb_reset == 0) {
        this->enable_display();
    }
}

void PlatinumCtrl::enable_cursor_int() {
    if (this->cursor_timer.active)
        TimerManager::get_instance()->cancel_timer(this->cursor_timer);
    if (!(this->swatch_int_mask & SWATCH_INT_CURSOR))
        return;
    uint64_t cursor_int_time = this->frame_start_time + static_cast<uint64_t>
        (double(this->hori_total) * this->cursor_line * NS_PER_SEC / this->pixel_clock + 0.5);
    if (cursor_int_time < TimerManager::get_instance()->current_time_ns())
        return;
    TimerManager::get_instance()->add_absolute_timer(this->cursor_timer,
        cursor_int_time, 0,
        [this](uint64_t, uint64_t) {
            this->update_irq(1, SWATCH_INT_CURSOR); // generate cursor interrupt
        }
    );
}

void PlatinumCtrl::enable_animate_int() {
    if (this->animate_timer.active)
        TimerManager::get_instance()->cancel_timer(this->animate_timer);
    if (!(this->swatch_int_mask & SWATCH_INT_ANIM))
        return;
    uint64_t animate_int_time = this->frame_start_time + static_cast<uint64_t>
        (double(this->hori_total) * this->animate_line * NS_PER_SEC / this->pixel_clock + 0.5);
    if (animate_int_time < TimerManager::get_instance()->current_time_ns())
        return;
    TimerManager::get_instance()->add_absolute_timer(this->animate_timer,
        animate_int_time, 0,
        [this](uint64_t, uint64_t) {
            this->update_irq(1, SWATCH_INT_ANIM); // generate animation interrupt
        }
    );
}

void PlatinumCtrl::update_irq(uint8_t irq_line_state, uint8_t irq_mask) {
    if (irq_line_state != !!(this->swatch_int_stat & irq_mask)) {
        if (irq_line_state)
            this->swatch_int_stat |= irq_mask;
        else
            this->swatch_int_stat &= ~irq_mask;

        if (this->swatch_int_mask & irq_mask)
            this->int_ctrl->ack_int(this->irq_id, irq_line_state);
    }
}

// ====================== QuickDraw Acceleration stuff =======================

void PlatinumCtrl::do_qda() {
    this->qda_go = false;
    this->qda_cmd_in_progress = true; {

        #define OPVARIANT(expand,type,pat,dir,depth) ( \
            ((((((((int(expand) \
            ) << 1) | int(type) \
            ) << 1) | int(pat) \
            ) << 1) | int(dir) \
            ) << 2) | int(depth) \
        )

        #define DOOP(expand,pat,op,type,dir,T) \
            case OPVARIANT(QDA_EXPAND::expand, QDA_TYPE::type, QDA_PAT::pat, QDA_DIR::dir, \
                QDA_DEPTH((sizeof(T)==1)?0:(sizeof(T)==2)?1:(sizeof(T)==4)?2:3) \
            ): \
                this->do_op<QDA_OP::op, QDA_TYPE::type, QDA_DIR::dir, T>(); break;

        if (this->qda_fill == QDA_FILL::DO_FILL) {
            switch (OPVARIANT(0, 0, 0, this->qda_processing_dir, this->qda_dst_depth)) {
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, REVERSE, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, REVERSE, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, REVERSE, uint32_t)
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, FORWARD, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, FORWARD, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, FILL, NORMAL, FORWARD, uint32_t)
            }
        } // fill
        else {
            switch (OPVARIANT(
                this->qda_expand,
                (
                    this->qda_colorize == QDA_COLORIZE::NO_COLORIZE &&
                    this->qda_pattern_op_rop == QDA_ROP::SRC_COPY
                ) ? QDA_TYPE::NORMAL : QDA_TYPE::EXTRAS,
                this->qda_pattern_operation,
                this->qda_processing_dir,
                this->qda_dst_depth))
            {
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, REVERSE, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, REVERSE, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, REVERSE, uint32_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, FORWARD, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, FORWARD, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, NORMAL, FORWARD, uint32_t)

                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, REVERSE, uint8_t )
                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, REVERSE, uint16_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, REVERSE, uint32_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, FORWARD, uint8_t )
                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, FORWARD, uint16_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , NORMAL, FORWARD, uint32_t)

                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, REVERSE, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, REVERSE, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, REVERSE, uint32_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, FORWARD, uint8_t )
                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, FORWARD, uint16_t)
                DOOP(NO_EXPAND, NO_PAT, COPY, EXTRAS, FORWARD, uint32_t)

                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, REVERSE, uint8_t )
                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, REVERSE, uint16_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, REVERSE, uint32_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, FORWARD, uint8_t )
                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, FORWARD, uint16_t)
                DOOP(NO_EXPAND, DO_PAT, PAT , EXTRAS, FORWARD, uint32_t)

                default:
                {
                    LOG_F(ERROR, "%s: unimplemented op: %s%s%s%s%s%s", this->name.c_str(),
                        this->qda_fill == QDA_FILL::DO_FILL                 ? "fill, " : "",
                        this->qda_colorize == QDA_COLORIZE::DO_COLORIZE     ? "colorize, " : "",
                        this->qda_expand == QDA_EXPAND::DO_EXPAND           ? "expand, " : "",
                        this->qda_pattern_operation == QDA_PAT::DO_PAT      ? "pattern, " : "",
                        this->qda_pattern_op_rop == QDA_ROP::SRC_COPY       ? "SRC_COPY, " :
                        this->qda_pattern_op_rop == QDA_ROP::SRC_OR         ? "SRC_OR, " :
                        this->qda_pattern_op_rop == QDA_ROP::SRC_XOR        ? "SRC_XOR, " :
                        this->qda_pattern_op_rop == QDA_ROP::SRC_BIC        ? "SRC_BIC, " :
                        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_COPY   ? "NOT_SRC_COPY, " :
                        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_OR     ? "NOT_SRC_OR, " :
                        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_XOR    ? "NOT_SRC_XOR, " :
                        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_BIC    ? "NOT_SRC_BIC, " : "",
                        this->qda_dst_depth == QDA_DEPTH::BPP__8            ? "8bpp" :
                        this->qda_dst_depth == QDA_DEPTH::BPP_16            ? "16bpp" :
                        this->qda_dst_depth == QDA_DEPTH::BPP_32            ? "32bpp" : ""
                    );
                }
            }
        } // else not fill
    } this->qda_cmd_in_progress = false;

    this->qda_interrupt_status = true;
    if (this->qda_interrupt_enable) {
        this->int_ctrl->ack_int(this->irq_id, 1);
    }
}

template <QDA_OP op, QDA_TYPE type, QDA_DIR dir, class T>
void PlatinumCtrl::do_op() {
    uint32_t dst_w = this->qda_dst_h_size;
    uint32_t dst_h = this->qda_dst_v_size;
    void *dst_p; int dst_stride; bool dst_is_fb;
    if (!this->setup_ptr(dir, sizeof(T), this->qda_dst_row_bytes, this->qda_dst_base, true,
        dst_w, dst_h, dst_p, dst_stride, dst_is_fb)
    )
        return;
    T *dst = (T*)dst_p;

    T src_pixel;
    if constexpr (op == QDA_OP::FILL) {
        #ifdef __LITTLE_ENDIAN__
        if constexpr (sizeof(T) == 1) src_pixel = this->qda_fg_color;
        if constexpr (sizeof(T) == 2) src_pixel = BYTESWAP_16(this->qda_fg_color);
        if constexpr (sizeof(T) == 4) src_pixel = BYTESWAP_32(this->qda_fg_color);
        #else
        src_pixel = this->qda_fg_color;
        #endif

        LOG_F(PLATOP, "%s: fill %x (%zx)  size:%dx%d  row_bytes:%d  stride:%d  %s", this->name.c_str(),
            this->qda_dst_base, size_t(dst),
            dst_w, dst_h,
            this->qda_dst_row_bytes,
            dst_stride,
            (dir == QDA_DIR::FORWARD) ? "FORWARD" : (dir == QDA_DIR::REVERSE) ? "REVERSE" : "?"
        );
    }

    void *src_p; int src_stride; bool src_is_fb; T *src;
    if constexpr (op == QDA_OP::COPY) {
        if (!this->setup_ptr(dir, sizeof(T), this->qda_src_row_bytes, this->qda_src_base, false,
            dst_w, dst_h, src_p, src_stride, src_is_fb)
        )
            return;
        src = (T*)src_p;
        LOG_F(PLATOP, "%s: copy %x -> %x (%zx -> %zx)  size:%dx%d  row_bytes:%d,%d  stride:%d,%d  %s", this->name.c_str(),
            this->qda_src_base, this->qda_dst_base, size_t(src), size_t(dst),
            dst_w, dst_h,
            this->qda_src_row_bytes, this->qda_dst_row_bytes,
            src_stride, dst_stride,
            (dir == QDA_DIR::FORWARD) ? "FORWARD" : (dir == QDA_DIR::REVERSE) ? "REVERSE" : "?"
        );
    }

    uint32_t pat_w, pat_h, pat_x, pat_y, pat_start_x; int32_t pat_return; T *pat_0_start;
    if constexpr (op == QDA_OP::PAT) {
        pat_w = this->qda_pattern_h_pixels << 3;
        pat_h = this->qda_pattern_v_pixels << 3;
        uint32_t pat_offset = this->qda_src_base & (pat_w * pat_h * sizeof(T) - 1);
        uint32_t pat_rb = this->qda_src_row_bytes;

        if (!this->setup_ptr(dir, sizeof(T), pat_rb, this->qda_src_base - pat_offset, false,
            pat_w, pat_h, src_p, src_stride, src_is_fb)
        )
            return;
        src = (T*)((uint8_t*)src_p + pat_offset);

        pat_x = (pat_offset % pat_rb) / sizeof(T);
        pat_y = pat_offset / pat_rb;
        src_stride = (pat_x - (pat_x + dst_w) % pat_w) * sizeof(T) + pat_rb;
        if constexpr (dir == QDA_DIR::REVERSE) {
            src_stride = -src_stride;
        }

        LOG_F(PLATOP, "%s: pat %x -> %x (%zx -> %zx)  size:%dx%d -> %dx%d  pos:%ux%u  row_bytes:%d,%d  stride:%d,%d  %s",
            this->name.c_str(),
            this->qda_src_base, this->qda_dst_base, size_t(src), size_t(dst),
            pat_w, pat_h, dst_w, dst_h, pat_x, pat_y,
            pat_rb, this->qda_dst_row_bytes,
            src_stride, dst_stride,
            (dir == QDA_DIR::FORWARD) ? "FORWARD" : (dir == QDA_DIR::REVERSE) ? "REVERSE" : "?"
        );

        pat_0_start = (T*)((uint8_t*)src_p + pat_x * sizeof(T));
        pat_x = pat_w - pat_x; // x remaining
        pat_y = pat_h - pat_y; // y remaining
        pat_start_x = pat_x;
        if constexpr (dir == QDA_DIR::FORWARD) {
            pat_return = -pat_w * sizeof(T);
        }
        else if constexpr (dir == QDA_DIR::REVERSE) {
            pat_return = pat_w * sizeof(T);
        }
    }

    LOG_F(PLATOP, "%s:     options: %s%s%s%s%s%s", this->name.c_str(),
        this->qda_fill == QDA_FILL::DO_FILL                 ? "fill, " : "",
        this->qda_colorize == QDA_COLORIZE::DO_COLORIZE     ? "colorize, " : "",
        this->qda_expand == QDA_EXPAND::DO_EXPAND           ? "expand, " : "",
        this->qda_pattern_operation == QDA_PAT::DO_PAT      ? "pattern, " : "",
        this->qda_pattern_op_rop == QDA_ROP::SRC_COPY       ? "SRC_COPY, " :
        this->qda_pattern_op_rop == QDA_ROP::SRC_OR         ? "SRC_OR, " :
        this->qda_pattern_op_rop == QDA_ROP::SRC_XOR        ? "SRC_XOR, " :
        this->qda_pattern_op_rop == QDA_ROP::SRC_BIC        ? "SRC_BIC, " :
        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_COPY   ? "NOT_SRC_COPY, " :
        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_OR     ? "NOT_SRC_OR, " :
        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_XOR    ? "NOT_SRC_XOR, " :
        this->qda_pattern_op_rop == QDA_ROP::NOT_SRC_BIC    ? "NOT_SRC_BIC, " : "",
        this->qda_dst_depth == QDA_DEPTH::BPP__8            ? "8bpp" :
        this->qda_dst_depth == QDA_DEPTH::BPP_16            ? "16bpp" :
        this->qda_dst_depth == QDA_DEPTH::BPP_32            ? "32bpp" : ""
    );

    constexpr int inc = (dir == QDA_DIR::FORWARD) ? sizeof(T) : - sizeof(T);

    T fg_color;
    T bg_color;
    bool expand;
    bool colorize;
    QDA_ROP rop;
    if constexpr (type == QDA_TYPE::EXTRAS) {
        expand = this->qda_expand == QDA_EXPAND::DO_EXPAND;
        /*
            TODO: Implement the expand feature.
            Src is 1bpp bit map using this->qda_bit_offset is the first bit (0=leftmost) of byte.
            1=black qda_fg_color
            0=white qda_bg_color
        */
        colorize = this->qda_colorize == QDA_COLORIZE::DO_COLORIZE;
        if (colorize) {
            fg_color = this->qda_fg_color;
            bg_color = this->qda_bg_color;
        }
        rop = this->qda_pattern_op_rop;
    }

    for ( ; dst_h > 0; dst_h--) {
        for (int x = dst_w; x > 0; x--) {
            if constexpr (op == QDA_OP::COPY || op == QDA_OP::PAT) {
                src_pixel = *src;
                src = (T*)((uint8_t*)src + inc);
                if constexpr (op == QDA_OP::PAT) {
                    pat_x--;
                    if (pat_x == 0) {
                        pat_x = pat_w;
                        src = (T*)((uint8_t*)src + pat_return);
                    }
                }

                if constexpr (type == QDA_TYPE::EXTRAS) {
                    T dst_pixel = *dst;

                    if (expand) {
                        if (colorize) {
                            switch (rop) {
                            case QDA_ROP::SRC_COPY    : src_pixel = ( src_pixel & fg_color) | (~src_pixel & bg_color); break;
                            case QDA_ROP::SRC_OR      : src_pixel = ( src_pixel & fg_color) | (~src_pixel & dst_pixel); break;
                            case QDA_ROP::SRC_XOR     : src_pixel =   src_pixel ^ dst_pixel; break;
                            case QDA_ROP::SRC_BIC     : src_pixel = ( src_pixel & bg_color) | (~src_pixel & dst_pixel); break;
                            case QDA_ROP::NOT_SRC_COPY: src_pixel = (~src_pixel & fg_color) | ( src_pixel & bg_color); break;
                            case QDA_ROP::NOT_SRC_OR  : src_pixel = (~src_pixel & fg_color) | ( src_pixel & dst_pixel); break;
                            case QDA_ROP::NOT_SRC_XOR : src_pixel =  ~src_pixel ^ dst_pixel; break;
                            case QDA_ROP::NOT_SRC_BIC : src_pixel = (~src_pixel & bg_color) | ( src_pixel & dst_pixel); break;
                            }
                        }
                    }
                    else if constexpr (sizeof(T) == 1) {
                        if (!colorize) {
                            switch (rop) {
                            case QDA_ROP::SRC_COPY    : src_pixel = src_pixel; break;
                            case QDA_ROP::SRC_OR      : src_pixel = src_pixel  | dst_pixel; break;
                            case QDA_ROP::SRC_XOR     : src_pixel = src_pixel  ^ dst_pixel; break;
                            case QDA_ROP::SRC_BIC     : src_pixel = ~src_pixel & dst_pixel; break;
                            case QDA_ROP::NOT_SRC_COPY: break; // n/a
                            case QDA_ROP::NOT_SRC_OR  : src_pixel = ~src_pixel | dst_pixel; break;
                            case QDA_ROP::NOT_SRC_XOR : src_pixel = ~src_pixel ^ dst_pixel; break;
                            case QDA_ROP::NOT_SRC_BIC : src_pixel = src_pixel  & dst_pixel; break;
                            }
                        } else {
                            switch (rop) {
                            case QDA_ROP::SRC_COPY    : break; // n/a
                            case QDA_ROP::SRC_OR      : src_pixel = (~src_pixel & (dst_pixel ^ fg_color)) ^ fg_color; break;
                            case QDA_ROP::SRC_XOR     : src_pixel =   src_pixel ^ dst_pixel; break;
                            case QDA_ROP::SRC_BIC     : src_pixel = (~src_pixel & (dst_pixel ^ bg_color)) ^ bg_color; break;
                            case QDA_ROP::NOT_SRC_COPY: break; // n/a
                            case QDA_ROP::NOT_SRC_OR  : src_pixel = (src_pixel & (dst_pixel ^ fg_color)) ^ fg_color; break;
                            case QDA_ROP::NOT_SRC_XOR : src_pixel = ~src_pixel^ dst_pixel; break;
                            case QDA_ROP::NOT_SRC_BIC : src_pixel = (src_pixel & (dst_pixel ^ bg_color)) ^ bg_color; break;
                            }
                        }
                    } // indexed color
                    else {
                        T inv_src_pixel = src_pixel ^ ((sizeof(T)==2) ? 0x7FFF : 0xFFFFFF);
                        if (!colorize) {
                            switch (rop) {
                            case QDA_ROP::SRC_COPY    : src_pixel = src_pixel; break;
                            case QDA_ROP::SRC_OR      : src_pixel = src_pixel & dst_pixel; break;
                            case QDA_ROP::SRC_XOR     : src_pixel = inv_src_pixel ^ dst_pixel; break;
                            case QDA_ROP::SRC_BIC     : src_pixel = inv_src_pixel | dst_pixel; break;
                            case QDA_ROP::NOT_SRC_COPY: src_pixel = inv_src_pixel; break;
                            case QDA_ROP::NOT_SRC_OR  : src_pixel = inv_src_pixel & dst_pixel; break;
                            case QDA_ROP::NOT_SRC_XOR : src_pixel = src_pixel ^ dst_pixel; break;
                            case QDA_ROP::NOT_SRC_BIC : src_pixel = src_pixel | dst_pixel; break;
                            }
                        } else {
                            switch (rop) {
                            case QDA_ROP::SRC_COPY    : src_pixel = (inv_src_pixel & fg_color) | (src_pixel & bg_color); break;
                            case QDA_ROP::SRC_OR      : src_pixel = (src_pixel & (dst_pixel ^ fg_color)) ^ fg_color; break;
                            case QDA_ROP::SRC_XOR     : src_pixel = inv_src_pixel ^ dst_pixel; break;
                            case QDA_ROP::SRC_BIC     : src_pixel = (src_pixel & (dst_pixel ^ bg_color)) ^ bg_color; break;
                            case QDA_ROP::NOT_SRC_COPY: src_pixel = (src_pixel & fg_color) | (inv_src_pixel & bg_color); break;
                            case QDA_ROP::NOT_SRC_OR  : src_pixel = (inv_src_pixel & (dst_pixel ^ fg_color)) ^ fg_color; break;
                            case QDA_ROP::NOT_SRC_XOR : src_pixel = src_pixel ^ dst_pixel; break;
                            case QDA_ROP::NOT_SRC_BIC : src_pixel = (inv_src_pixel & (dst_pixel ^ bg_color)) ^ bg_color; break;
                            }
                        }
                    } // direct color
                } // EXTRAS
            } // COPY || PAT

            *dst = src_pixel;
            dst = (T*)((uint8_t*)dst + inc);
        }
        dst = (T*)((uint8_t*)dst + dst_stride);
        if constexpr (op == QDA_OP::COPY) {
            src = (T*)((uint8_t*)src + src_stride);
        }
        if constexpr (op == QDA_OP::PAT) {
            pat_x = pat_start_x;
            pat_y--;
            if (pat_y) {
               src = (T*)((uint8_t*)src + src_stride);
            } else {
                pat_y = pat_h;
                src = pat_0_start;
            }
        }
    }
    if (dst_is_fb)
        this->draw_fb = true;
}

template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint32_t>();
template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::FILL, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint32_t>();

template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint32_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint32_t>();

template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::REVERSE, uint32_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::NORMAL, QDA_DIR::FORWARD, uint32_t>();

template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint32_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::COPY, QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint32_t>();

template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::REVERSE, uint32_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint8_t >();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint16_t>();
template void PlatinumCtrl::do_op<QDA_OP::PAT , QDA_TYPE::EXTRAS, QDA_DIR::FORWARD, uint32_t>();

bool PlatinumCtrl::setup_ptr(Platinum::QDA_DIR dir, uint32_t pixel_size,
    uint32_t row_bytes, uint32_t base, bool is_dst, uint32_t h, uint32_t v,
    void* &p, int &stride, bool &is_fb)
{
    int span = (v - 1) * row_bytes + h * pixel_size;

    uint32_t base_top_left;
    if (dir == QDA_DIR::REVERSE) {
        base_top_left = base + pixel_size - span;
        stride = h * pixel_size - row_bytes;
    } else {
        base_top_left = base;
        stride = row_bytes - h * pixel_size;
    }

    MapDmaResult res = mmu_map_dma_mem(base_top_left, span, true);
    if (is_dst && !res.is_writable) {
        LOG_F(ERROR, "%s: qda is not writable", this->name.c_str());
        is_fb = false;
        return false;
    }

    if (res.type & RT_MMIO) {
        if (res.dev_obj == this) {
            p = &this->vram_ptr[base_top_left - res.dev_base];
            if (is_dst)
                is_fb = true;
        } else {
            LOG_F(ERROR, "%s: qda other MMIO device is not supported", this->name.c_str());
            is_fb = false;
            return false;
        }
    }
    else {
        is_fb = false;
        p = res.host_va;
    }
    if (dir == QDA_DIR::REVERSE) {
        p = (uint8_t*)p + span - pixel_size;
    }
    return true;
}

// ========================== Device registry stuff ==========================
static const PropMap Platinum_Properties = {
    {"gfxmem_size",
        new IntProperty(4, std::vector<uint32_t>({1, 2, 4}))},
};

static const DeviceDescription Platinum_Descriptor = {
    PlatinumCtrl::create, {"PlatinumDisplay@0"}, Platinum_Properties,
    HWCompType::MEM_CTRL | HWCompType::MMIO_DEV | HWCompType::VIDEO_CTRL
};

REGISTER_DEVICE(Platinum, Platinum_Descriptor);

static std::unique_ptr<HWComponent> PlatinumDisplay_create(const std::string &dev_name) {
    return std::unique_ptr<DisplayID>(new DisplayID(dev_name));
}

static const PropMap PlatinumDisplay_Properties = {
    {"mon_id", new StrProperty("HiRes12-14in")},
    {"edid"  , new StrProperty("")},
};

static const DeviceDescription PlatinumDisplay_Descriptor = {
    PlatinumDisplay_create, {}, PlatinumDisplay_Properties, HWCompType::DISPLAY
};

REGISTER_DEVICE(PlatinumDisplay, PlatinumDisplay_Descriptor);
