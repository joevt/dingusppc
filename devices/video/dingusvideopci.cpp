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

/** @file Dingus video output emulation. */

#include <devices/deviceregistry.h>
#include <devices/video/dingusvideopci.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cinttypes>

namespace loguru {
    enum : Verbosity {
        Verbosity_DINGUSVIDEO = loguru::Verbosity_INFO,
    };
}

DingusVideo::DingusVideo()
    : PCIDevice("DingusVideo"), HWComponent("DingusVideo")
{
    supports_types(HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL);

    // set up PCI configuration space header
    this->vendor_id   = PCI_VENDOR_DINGUSPPC;
    this->device_id   = 1;
    this->class_rev   = (0x038000 << 8) | 0;

    this->setup_bars({
        {0, 0xFFC00000UL}  // base address for the HW registers and VRAM (4MB)
    });

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };
}

DingusVideoCtrl::DingusVideoCtrl()
    : HWComponent("DingusVideoCtrl")
{
    supports_types(HWCompType::VIDEO_CTRL);
}

HWComponent* DingusVideo::set_property(const std::string &property, const std::string &value, int32_t unit_address)
{
    if (unit_address == -1) {
        if (property == "gfxmem_size") {
            if (this->override_property(property, value)) {
                this->vram_size = get_property_int("gfxmem_size") << 20;
                this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size - this->regs_size]);
                this->bars_cfg[0] = -this->vram_size;
                return this;
            }
        }
        if (property == "num_displays") {
            if (this->override_property(property, value)) {
                this->num_displays = get_property_int("num_displays");
                for (int i = 1; i < this->num_displays; i++)
                    MachineFactory::create_device(this, "DingusVideoCtrl@" + std::to_string(i), HWCompType::VIDEO_CTRL);
                this->regs_size = (4 + sizeof(DingusVideoCtrl::DingusVideoRegValues) * this->num_displays + 4095) / 4095 * 4095;
            }
        }
        return PCIDevice::set_property(property, value, unit_address);
    }
    return nullptr;
}

void DingusVideo::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num)
{
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->get_name_and_unit_address().c_str(), bar_num, aperture);
    }
}

void DingusVideo::notify_bar_change(int bar_num)
{
    switch (bar_num) {
    case 0:
        // Use a single BAR for two different regions - this is more efficient than using two BARs
        uint32_t new_base = this->bars[bar_num] & ~15;
        change_one_bar(this->regs_base, this->regs_size, new_base, 0);
        change_one_bar(this->vram_base, this->vram_size - this->regs_size, new_base + this->regs_size, 1);
        break;
    }
}

PostInitResultType DingusVideoCtrl::device_postinit()
{
    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));
    this->dingus_pci = dynamic_cast<DingusVideo*>(this->parent);

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state != !!(this->int_status & VBL_IRQ_STAT)) {
            if (irq_line_state)
                this->int_status |= VBL_IRQ_STAT;
            else
                this->int_status &= ~VBL_IRQ_STAT;

            if (this->int_enable & VBL_IRQ_EN)
                this->dingus_pci->pci_interrupt(irq_line_state);
        }
    };

    return PI_SUCCESS;
}

static const char * get_name_dingusvideoreg(int reg)
{
    switch (reg) {
    case DingusVideoRegs::NUM_DISPLAYS         : return "NUM_DISPLAYS";
    case DingusVideoRegs::HACTIVE              : return "HACTIVE";
    case DingusVideoRegs::HFRONT               : return "HFRONT";
    case DingusVideoRegs::HBACK                : return "HBACK";
    case DingusVideoRegs::HTOTAL               : return "HTOTAL";
    case DingusVideoRegs::VACTIVE              : return "VACTIVE";
    case DingusVideoRegs::VFRONT               : return "VFRONT";
    case DingusVideoRegs::VBACK                : return "VBACK";
    case DingusVideoRegs::VTOTAL               : return "VTOTAL";
    case DingusVideoRegs::MON_SENSE            : return "MON_SENSE";
    case DingusVideoRegs::TIMING_FLAGS         : return "TIMING_FLAGS";
    case DingusVideoRegs::IMMEDIATE_FLAGS      : return "IMMEDIATE_FLAGS";
    case DingusVideoRegs::PIXEL_CLOCK          : return "PIXEL_CLOCK";
    case DingusVideoRegs::PIXEL_DEPTH          : return "PIXEL_DEPTH";
    case DingusVideoRegs::FRAMEBUFFER_BASE     : return "FRAMEBUFFER_BASE";
    case DingusVideoRegs::FRAMEBUFFER_ROWBYTES : return "FRAMEBUFFER_ROWBYTES";
    case DingusVideoRegs::INT_ENABLE           : return "INT_ENABLE";
    case DingusVideoRegs::INT_STATUS           : return "INT_STATUS";
    case DingusVideoRegs::HWCURSOR_BASE        : return "HWCURSOR_BASE";
    case DingusVideoRegs::HWCURSOR_WIDTH       : return "HWCURSOR_WIDTH";
    case DingusVideoRegs::HWCURSOR_POS         : return "HWCURSOR_POS";
    case DingusVideoRegs::COLOR_INDEX          : return "COLOR_INDEX";
    case DingusVideoRegs::COLOR_DATA           : return "COLOR_DAT";
    default                                    : return "unknown";
    }
}

#define DO_READ(obj, x) \
    do { \
        value = x; \
        LOG_F(DINGUSVIDEO, "%s: read  %s %03x.%c = %0*x", obj->name.c_str(), \
            get_name_dingusvideoreg(reg), offset, SIZE_ARG(size), size*2, value); \
    } while (0)

uint32_t DingusVideo::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->vram_base)
        return read_mem(&this->vram_ptr[offset], size);

    if (rgn_start == this->regs_base) {
        uint32_t value;
        uint32_t reg;

        if (offset == 0) {
            reg = -1;
            DO_READ(this, this->num_displays);
        }
        else {
            DingusVideoCtrl* ctrl = nullptr;
            uint32_t unit_address = (offset - 4) / sizeof(DingusVideoCtrl::DingusVideoRegValues);
            reg = (offset - 4 - unit_address * sizeof(DingusVideoCtrl::DingusVideoRegValues)) >> 2;
            if (this->children.count(unit_address))
                ctrl = dynamic_cast<DingusVideoCtrl*>(this->children[unit_address].get());
            if (ctrl) {
                switch (reg) {
                    case DingusVideoRegs::HACTIVE              : DO_READ(ctrl, ctrl->staged.hactive); break;
                    case DingusVideoRegs::HFRONT               : DO_READ(ctrl, ctrl->staged.hfront); break;
                    case DingusVideoRegs::HBACK                : DO_READ(ctrl, ctrl->staged.hback); break;
                    case DingusVideoRegs::HTOTAL               : DO_READ(ctrl, ctrl->staged.htotal); break;
                    case DingusVideoRegs::VACTIVE              : DO_READ(ctrl, ctrl->staged.vactive); break;
                    case DingusVideoRegs::VFRONT               : DO_READ(ctrl, ctrl->staged.vfront); break;
                    case DingusVideoRegs::VBACK                : DO_READ(ctrl, ctrl->staged.vback); break;
                    case DingusVideoRegs::VTOTAL               : DO_READ(ctrl, ctrl->staged.vtotal); break;
                    case DingusVideoRegs::TIMING_FLAGS         : DO_READ(ctrl, ctrl->staged.timing_flags); break;
                    case DingusVideoRegs::IMMEDIATE_FLAGS      : DO_READ(ctrl, ctrl->immediate_flags); break;
                    case DingusVideoRegs::PIXEL_CLOCK          : DO_READ(ctrl, ctrl->staged.pixel_clock); break;
                    case DingusVideoRegs::PIXEL_DEPTH          : DO_READ(ctrl, ctrl->staged.pixel_depth); break;
                    case DingusVideoRegs::FRAMEBUFFER_BASE     : DO_READ(ctrl, ctrl->staged.framebuffer_base); break;
                    case DingusVideoRegs::FRAMEBUFFER_ROWBYTES : DO_READ(ctrl, ctrl->staged.framebuffer_rowbytes); break;
                    case DingusVideoRegs::INT_ENABLE           : DO_READ(ctrl, ctrl->int_enable); break;
                    case DingusVideoRegs::HWCURSOR_BASE        : DO_READ(ctrl, ctrl->staged.hwcursor_base); break;
                    case DingusVideoRegs::HWCURSOR_WIDTH       : DO_READ(ctrl, ctrl->staged.hwcursor_width); break;
                    case DingusVideoRegs::HWCURSOR_POS         : DO_READ(ctrl, ctrl->staged.hwcursor_pos); break;
                    case DingusVideoRegs::COLOR_INDEX          : DO_READ(ctrl, ctrl->color_index); break;
                    case DingusVideoRegs::MON_SENSE            : DO_READ(ctrl, (ctrl->cur_mon_id << 6) | ctrl->mon_sense); break;
                    case DingusVideoRegs::INT_STATUS:
                        value = ctrl->int_status;
                        if (value != ctrl->last_int_status) {
                            LOG_F(DINGUSVIDEO, "%s: read  (previous %d times) %s %03x.%c = %0*x",
                                ctrl->get_name_and_unit_address().c_str(), ctrl->last_int_status_read_count,
                                get_name_dingusvideoreg(reg), offset, SIZE_ARG(size), size * 2, value);
                            ctrl->last_int_status = value;
                            ctrl->last_int_status_read_count = 0;
                        }
                        else {
                            ctrl->last_int_status_read_count++;
                        }
                        break;
                    case DingusVideoRegs::COLOR_DATA:
                        value = ctrl->staged.colors[ctrl->color_index];
                        LOG_F(DINGUSVIDEO, "%s: read  %s[0x%x] %03x.%c = %0*x", ctrl->get_name_and_unit_address().c_str(),
                            get_name_dingusvideoreg(reg), ctrl->color_index, offset, SIZE_ARG(size), size*2, value);
                        ctrl->color_index = (ctrl->color_index + 1) % 0x300; // 0 = indexed color, 0x100 = gamma, 0x200 = cursor
                        break;
                    default:
                        LOG_F(ERROR, "%s: read  %s %03x.%c", ctrl->get_name_and_unit_address().c_str(),
                            get_name_dingusvideoreg(reg), offset, SIZE_ARG(size));
                        value = 0;
                }
            }
            else {
                LOG_F(ERROR, "%s@%d: read  %s %03x.%c", "DingusVideoCtrl@", unit_address,
                    get_name_dingusvideoreg(reg), offset, SIZE_ARG(size));
                value = 0;
            }
        }

        return value;
    }
    
    return PCIBase::read(rgn_start, offset, size);
}

#define DO_WRITE_MSG(obj) \
    do { \
        LOG_F(DINGUSVIDEO, "%s: write %s %03x.%c = %0*x", obj->name.c_str(), \
            get_name_dingusvideoreg(reg), offset, SIZE_ARG(size), size * 2, value); \
    } while (0)

#define DO_WRITE(obj, x) \
    do { \
        x = value; \
        DO_WRITE_MSG(obj); \
    } while (0)

#define BIT_CHANGED(what, bit) \
    ((what ^ value) & bit)

#define CHANGE_BIT(what, bit, ...) \
    if (BIT_CHANGED(what, bit)) { \
        what = (what & ~bit) | (value & bit); \
        int bitval = (value & bit) != 0; \
        LOG_F(DINGUSVIDEO, #bit " flipped, new value: %d", bitval); \
        __VA_ARGS__ \
    }

void DingusVideo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->vram_base)
        return write_mem(&this->vram_ptr[offset], value, size);

    if (rgn_start == this->regs_base) {
        value = BYTESWAP_32(value);
        uint32_t reg;

        if (offset == 0) {
            reg = -1;
            LOG_F(ERROR, "%s: write %s %03x.%c = %0*x", this->get_name_and_unit_address().c_str(),
                get_name_dingusvideoreg(reg), offset, SIZE_ARG(size), size * 2, value);
        }
        else {
            DingusVideoCtrl* ctrl = nullptr;
            uint32_t unit_address = (offset - 4) / sizeof(DingusVideoCtrl::DingusVideoRegValues);
            if (this->children.count(unit_address))
                ctrl = dynamic_cast<DingusVideoCtrl*>(this->children[unit_address].get());
            if (ctrl) {
                reg = (offset - 4 - unit_address * sizeof(DingusVideoCtrl::DingusVideoRegValues)) >> 2;
                switch (reg) {
                    case DingusVideoRegs::HACTIVE              : DO_WRITE(ctrl, ctrl->staged.hactive); break;
                    case DingusVideoRegs::HFRONT               : DO_WRITE(ctrl, ctrl->staged.hfront); break;
                    case DingusVideoRegs::HBACK                : DO_WRITE(ctrl, ctrl->staged.hback); break;
                    case DingusVideoRegs::HTOTAL               : DO_WRITE(ctrl, ctrl->staged.htotal); break;
                    case DingusVideoRegs::VACTIVE              : DO_WRITE(ctrl, ctrl->staged.vactive); break;
                    case DingusVideoRegs::VFRONT               : DO_WRITE(ctrl, ctrl->staged.vfront); break;
                    case DingusVideoRegs::VBACK                : DO_WRITE(ctrl, ctrl->staged.vback); break;
                    case DingusVideoRegs::VTOTAL               : DO_WRITE(ctrl, ctrl->staged.vtotal); break;
                    case DingusVideoRegs::TIMING_FLAGS         : DO_WRITE(ctrl, ctrl->staged.timing_flags); break;
                    case DingusVideoRegs::PIXEL_CLOCK          : DO_WRITE(ctrl, ctrl->staged.pixel_clock); break;
                    case DingusVideoRegs::PIXEL_DEPTH          : DO_WRITE(ctrl, ctrl->staged.pixel_depth); break;
                    case DingusVideoRegs::FRAMEBUFFER_BASE     : DO_WRITE(ctrl, ctrl->staged.framebuffer_base); break;
                    case DingusVideoRegs::FRAMEBUFFER_ROWBYTES : DO_WRITE(ctrl, ctrl->staged.framebuffer_rowbytes); break;
                    case DingusVideoRegs::HWCURSOR_BASE        : DO_WRITE(ctrl, ctrl->staged.hwcursor_base); break;
                    case DingusVideoRegs::HWCURSOR_WIDTH       : DO_WRITE(ctrl, ctrl->staged.hwcursor_width); break;
                    case DingusVideoRegs::HWCURSOR_POS         : DO_WRITE(ctrl, ctrl->staged.hwcursor_pos); break;
                    case DingusVideoRegs::COLOR_INDEX          : DO_WRITE(ctrl, ctrl->color_index); break;
                    case DingusVideoRegs::MON_SENSE: {
                        DO_WRITE_MSG(ctrl);
                        uint8_t dirs = ((value >> 3) & 7) ^ 7;
                        uint8_t levels = ((value & 7) & dirs) | (dirs ^ 7);
                        ctrl->mon_sense = value & 0x3F;
                        ctrl->cur_mon_id = ctrl->disp_id->read_monitor_sense(levels, dirs);
                        break;
                    }
                    case DingusVideoRegs::COLOR_DATA:
                        LOG_F(DINGUSVIDEO, "%s: write %s[0x%x] %03x.%c = %0*x", ctrl->get_name_and_unit_address().c_str(),
                            get_name_dingusvideoreg(reg), ctrl->color_index, offset, SIZE_ARG(size), size*2, value);
                        ctrl->staged.colors[ctrl->color_index] = value;
                        ctrl->color_index = (ctrl->color_index + 1) % 0x300; // 0 = indexed color, 0x100 = gamma, 0x200 = cursor
                        break;
                    case DingusVideoRegs::IMMEDIATE_FLAGS: {
                        DO_WRITE_MSG(ctrl);

                        bool do_update_display = false;
                        bool do_disable_display = false;
                        bool do_enable_display = false;
                        bool do_check_sync = false;

                        if (value & DO_LATCH) {
                            ctrl->latched = ctrl->staged;
                            do_update_display = true;
                        }

                        CHANGE_BIT(ctrl->immediate_flags, DISABLE_TIMING,
                            {
                                if (bitval) {
                                    do_disable_display = true;
                                } else {
                                    do_enable_display = true;
                                }
                            }
                        );
                        CHANGE_BIT(ctrl->immediate_flags, HWCURSOR_ENABLE,
                            {
                                if (bitval) {
                                    ctrl->cursor_ctrl_cb(true);
                                } else {
                                    ctrl->cursor_ctrl_cb(false);
                                }
                            }
                        );
                        CHANGE_BIT(ctrl->immediate_flags, VSYNC_DISABLE, do_check_sync = true; );
                        CHANGE_BIT(ctrl->immediate_flags, HSYNC_DISABLE, do_check_sync = true; );
                        CHANGE_BIT(ctrl->immediate_flags, CSYNC_DISABLE, do_check_sync = true; );
                        CHANGE_BIT(ctrl->immediate_flags, BLANK_DISABLE,
                            {
                                if (bitval)
                                    ctrl->blank_on = false;
                                else {
                                    ctrl->blank_on = true;
                                    ctrl->blank_display();
                                }
                            }
                        );

                        if (do_check_sync) {

                        }
                        if (do_update_display || do_enable_display) {
                            ctrl->enable_display();
                            if (do_enable_display) {
                                ctrl->display_enabled = true;
                            }
                        }
                        if (do_disable_display) {
                            ctrl->disable_display();
                        }
                        break;
                    }
                    case DingusVideoRegs::INT_ENABLE:
                        DO_WRITE_MSG(ctrl);

                        if ((ctrl->int_enable ^ value) & VBL_IRQ_CLR) {
                            // clear VBL IRQ on a 1-to-0 transition of INT_ENABLE[VBL_IRQ_CLR]
                            if (!(value & VBL_IRQ_CLR))
                                ctrl->vbl_cb(0);
                        }
                        ctrl->int_enable = value & 0x0F; // alternates between 0x04 and 0x0c
                        break;
                    default:
                        LOG_F(ERROR, "%s: write %s %03x.%c = %0*x", ctrl->get_name_and_unit_address().c_str(),
                            get_name_dingusvideoreg(reg), offset, SIZE_ARG(size), size * 2, value);
                }
            }
        }
    }
}

void DingusVideoCtrl::enable_display()
{
    // calculate active_width and active_height from video timing parameters
    this->active_width  = this->latched.hactive;
    this->active_height = this->latched.vactive;

    // set framebuffer parameters
    this->fb_ptr   = &dingus_pci->vram_ptr[this->latched.framebuffer_base];
    this->fb_pitch = this->latched.framebuffer_rowbytes;

    // get pixel depth
    this->pixel_depth = this->latched.pixel_depth;
    switch (this->pixel_depth) {
    case 1:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_1bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 2:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_2bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 4:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_4bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 8:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_8bpp_indexed(dst_buf, dst_pitch);
        };
        break;
    case 16:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_15bpp<BE>(dst_buf, dst_pitch);
        };
        break;
    case 32:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_32bpp<BE>(dst_buf, dst_pitch);
        };
        break;
    default:
        LOG_F(ERROR, "RaDACal: Invalid pixel depth code!");
    }

    // calculate display refresh rate
    this->hori_blank = this->latched.htotal - this->latched.hactive;
    this->vert_blank = this->latched.vtotal - this->latched.vactive;

    this->hori_total = this->latched.htotal;
    this->vert_total = this->latched.vtotal;

    this->stop_refresh_task();

    // set up periodic timer for display updates
    if (this->active_width > 0 && this->active_height > 0 && this->latched.pixel_clock > 0) {
        this->refresh_rate = (double)(this->latched.pixel_clock) / (this->hori_total * this->vert_total);
        LOG_F(INFO, "%s: refresh rate set to %f Hz", this->get_name_and_unit_address().c_str(), this->refresh_rate);

        this->start_refresh_task();

        this->blank_on = false;

        LOG_F(DINGUSVIDEO, "%s: display enabled", this->get_name_and_unit_address().c_str());
        this->crtc_on = true;
    }
    else {
        LOG_F(DINGUSVIDEO, "%s: display not enabled", this->get_name_and_unit_address().c_str());
        this->blank_on = true;
        this->crtc_on = false;
    }
}

void DingusVideoCtrl::disable_display()
{
    this->crtc_on = false;
    LOG_F(INFO, "%s: display disabled", this->get_name_and_unit_address().c_str());
    this->display_enabled = false;
}

void DingusVideoCtrl::cursor_ctrl_cb(bool cursor_on)
{
    if (cursor_on) {
        this->cursor_ovl_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_hw_cursor(this->fb_ptr - 16, dst_buf, dst_pitch);
        };
    } else {
        this->cursor_ovl_cb = nullptr;
    }
}

void DingusVideoCtrl::draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch)
{
    int cursor_xpos = int32_t(this->latched.hwcursor_pos) >> 16;
    int num_pixels = this->latched.hactive - cursor_xpos;
    if (num_pixels <= 0)
        return;

    int cursor_ypos = int16_t(this->latched.hwcursor_pos);
    int num_lines = this->latched.vactive - cursor_ypos;
    if (num_lines <= 0)
        return;

    int cursor_width = this->latched.hwcursor_width;
    if (num_pixels > cursor_width)
        num_pixels = cursor_width;

    int cursor_height = this->latched.hwcursor_width;
    if (num_lines > cursor_height)
        num_lines = cursor_height;

    uint8_t *src_row = src_buf;
    uint8_t *dst_row = dst_buf + cursor_ypos * dst_pitch + cursor_xpos * sizeof(uint32_t);
    dst_pitch -= num_pixels * sizeof(uint32_t);

    if (this->latched.timing_flags & HWCURSOR_24) {
        int src_pitch = (cursor_width - num_pixels) * 4;
        for (int h = num_lines; h > 0; h--) {
            for (int x = num_pixels; x > 0; x--) {
                uint32_t pix = READ_DWORD_BE_A(src_row);
                uint32_t dst = READ_DWORD_LE_A(dst_row);

                uint32_t alpha = pix >> 24;
                uint32_t rev_alpha = 255 - alpha;

                uint32_t tempr = ((dst >> 16) & 255) * rev_alpha + ((pix >> 16) & 255) * alpha + 0x80;
                uint32_t tempg = ((dst >>  8) & 255) * rev_alpha + ((pix >>  8) & 255) * alpha + 0x80;
                uint32_t tempb = ((dst >>  0) & 255) * rev_alpha + ((pix >>  0) & 255) * alpha + 0x80;

                uint32_t c =
                    (((tempr + (tempr >> 8)) >> 8) << 16) +
                    (((tempg + (tempg >> 8)) >> 8) <<  8) +
                    (((tempb + (tempb >> 8)) >> 8) <<  0) ;

                WRITE_DWORD_LE_A(dst_row, c);
                src_row += sizeof(uint32_t);
                dst_row += sizeof(uint32_t);
            }
            dst_row += dst_pitch;
            src_row += src_pitch;
        }
    } else {
        int src_pitch = cursor_width - num_pixels;
        uint32_t *color = &this->latched.colors[0x200];
        for (int h = num_lines; h > 0; h--) {
            for (int x = num_pixels; x > 0; x--) {
                uint8_t pix = *src_row;
                if (pix == 0xff) [[unlikely]] {
                    // inverse pixels
                    uint32_t c = READ_DWORD_LE_A(dst_row) ^ 0xFFFFFFU;
                    WRITE_DWORD_LE_A(dst_row, c);
                } else if (pix != 0) {
                    // opaque pixels
                    WRITE_DWORD_LE_A(dst_row, color[pix]);
                }
                src_row += sizeof(uint8_t);
                dst_row += sizeof(uint32_t);
            }
            dst_row += dst_pitch;
            src_row += src_pitch;
        }
    }
}

// ========================== Device registry stuff ==========================

// ==================== DingusVideo

static const PropMap DingusVideo_Properties = {
    {"gfxmem_size",
        new IntProperty(4, std::vector<uint32_t>({1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}))},
    {"num_displays",
        new IntProperty(1, 1, 20)},
};

static const DeviceDescription DingusVideo_Descriptor = {
    DingusVideo::create, {"DingusVideoCtrl@0"}, DingusVideo_Properties, HWCompType::PCI_DEV
};

REGISTER_DEVICE(DingusVideo, DingusVideo_Descriptor);

// ==================== DingusVideoCtrl

static const DeviceDescription DingusVideoCtrl_Descriptor = {
    DingusVideoCtrl::create, {"DingusVideoDisplay@0"}, {}, HWCompType::VIDEO_CTRL
};

REGISTER_DEVICE(DingusVideoCtrl, DingusVideoCtrl_Descriptor);

// ==================== DingusVideoDisplay

static std::unique_ptr<HWComponent> DingusVideoDisplay_create() {
    return std::unique_ptr<DisplayID>(new DisplayID("DingusVideoDisplay"));
}

static const PropMap DingusVideoDisplay_Properties = {
    {"mon_id", new StrProperty("Multiscan20in")},
    {"edid"  , new StrProperty("")},
};

static const DeviceDescription DingusVideoDisplay_Descriptor = {
    DingusVideoDisplay_create, {}, DingusVideoDisplay_Properties, HWCompType::DISPLAY
};

REGISTER_DEVICE(DingusVideoDisplay, DingusVideoDisplay_Descriptor);

// ===========================================================================
