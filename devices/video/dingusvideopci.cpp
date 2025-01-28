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
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cinttypes>

namespace loguru {
    enum : Verbosity {
        Verbosity_DINGUSVIDEO = loguru::Verbosity_INFO,
    };
}

DingusVideo::DingusVideo()
    : PCIVideoCtrl("DingusVideo"), HWComponent("DingusVideo")
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

HWComponent* DingusVideo::set_property(const std::string &property, const std::string &value, int32_t unit_address) {
    if (unit_address == -1) {
        if (property == "gfxmem_size") {
            if (this->override_property(property, value)) {
                this->vram_size = get_property_int("gfxmem_size") << 20;
                this->vram_ptr = std::unique_ptr<uint8_t[]> (new uint8_t[this->vram_size - this->regs_size]);
                this->bars_cfg[0] = -this->vram_size;
                return this;
            }
        }
        return PCIVideoCtrl::set_property(property, value, unit_address);
    }
    return nullptr;
}

void DingusVideo::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void DingusVideo::notify_bar_change(int bar_num) {
    switch (bar_num) {
    case 0:
        // Use a single BAR for two different regions - this is more efficient than using two BARs
        uint32_t new_base = this->bars[bar_num] & ~15;
        change_one_bar(this->regs_base, this->regs_size, new_base, 0);
        change_one_bar(this->vram_base, this->vram_size - this->regs_size, new_base + this->regs_size, 1);
        break;
    }
}

PostInitResultType DingusVideo::device_postinit() {
    // initialize display identification
    this->disp_id = dynamic_cast<DisplayID*>(this->get_comp_by_type(HWCompType::DISPLAY));

    this->vbl_cb = [this](uint8_t irq_line_state) {
        if (irq_line_state != !!(this->int_status & VBL_IRQ_STAT)) {
            if (irq_line_state)
                this->int_status |= VBL_IRQ_STAT;
            else
                this->int_status &= ~VBL_IRQ_STAT;

            if (this->int_enable & VBL_IRQ_EN)
                this->pci_interrupt(irq_line_state);
        }
    };

    return PI_SUCCESS;
}

static const char * get_name_dingusvideoreg(int offset) {
    switch (offset >> 2) {
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

#define DO_READ(x) \
    do { \
        value = x; \
        LOG_F(DINGUSVIDEO, "%s: read  %s %03x.%c = %0*x", this->name.c_str(), \
            get_name_dingusvideoreg(offset), offset, SIZE_ARG(size), size*2, value); \
    } while (0)

uint32_t DingusVideo::read(uint32_t rgn_start, uint32_t offset, int size)
{
    if (rgn_start == this->vram_base)
        return read_mem(&this->vram_ptr[offset], size);

    if (rgn_start == this->regs_base) {
        uint32_t value;

        switch (offset >> 2) {
            case DingusVideoRegs::HACTIVE              : DO_READ(this->staged.hactive); break;
            case DingusVideoRegs::HFRONT               : DO_READ(this->staged.hfront); break;
            case DingusVideoRegs::HBACK                : DO_READ(this->staged.hback); break;
            case DingusVideoRegs::HTOTAL               : DO_READ(this->staged.htotal); break;
            case DingusVideoRegs::VACTIVE              : DO_READ(this->staged.vactive); break;
            case DingusVideoRegs::VFRONT               : DO_READ(this->staged.vfront); break;
            case DingusVideoRegs::VBACK                : DO_READ(this->staged.vback); break;
            case DingusVideoRegs::VTOTAL               : DO_READ(this->staged.vtotal); break;
            case DingusVideoRegs::TIMING_FLAGS         : DO_READ(this->staged.timing_flags); break;
            case DingusVideoRegs::IMMEDIATE_FLAGS      : DO_READ(this->immediate_flags); break;
            case DingusVideoRegs::PIXEL_CLOCK          : DO_READ(this->staged.pixel_clock); break;
            case DingusVideoRegs::PIXEL_DEPTH          : DO_READ(this->staged.pixel_depth); break;
            case DingusVideoRegs::FRAMEBUFFER_BASE     : DO_READ(this->staged.framebuffer_base); break;
            case DingusVideoRegs::FRAMEBUFFER_ROWBYTES : DO_READ(this->staged.framebuffer_rowbytes); break;
            case DingusVideoRegs::INT_ENABLE           : DO_READ(this->int_enable); break;
            case DingusVideoRegs::HWCURSOR_BASE        : DO_READ(this->staged.hwcursor_base); break;
            case DingusVideoRegs::HWCURSOR_WIDTH       : DO_READ(this->staged.hwcursor_width); break;
            case DingusVideoRegs::HWCURSOR_POS         : DO_READ(this->staged.hwcursor_pos); break;
            case DingusVideoRegs::COLOR_INDEX          : DO_READ(this->color_index); break;
            case DingusVideoRegs::MON_SENSE            : DO_READ((this->cur_mon_id << 6) | this->mon_sense); break;
            case DingusVideoRegs::INT_STATUS:
                value = this->int_status;
                if (value != this->last_int_status) {
                    LOG_F(DINGUSVIDEO, "%s: read  (previous %d times) %s %03x.%c = %0*x", this->name.c_str(),
                        last_int_status_read_count, get_name_dingusvideoreg(offset), offset, SIZE_ARG(size), size * 2, value);
                    this->last_int_status = value;
                    this->last_int_status_read_count = 0;
                }
                else {
                    this->last_int_status_read_count++;
                }
                break;
            case DingusVideoRegs::COLOR_DATA:
                value = this->staged.colors[this->color_index];
                LOG_F(DINGUSVIDEO, "%s: read  %s[0x%x] %03x.%c = %0*x", this->name.c_str(),
                    get_name_dingusvideoreg(offset), this->color_index, offset, SIZE_ARG(size), size*2, value);
                this->color_index = (this->color_index + 1) % 0x300; // 0 = indexed color, 0x100 = gamma, 0x200 = cursor
                break;
            default:
                LOG_F(ERROR, "%s: read  %s %03x.%c", this->name.c_str(), get_name_dingusvideoreg(offset), offset, SIZE_ARG(size));
                value = 0;
        }

        return value;
    }
    
    return PCIBase::read(rgn_start, offset, size);
}

#define DO_WRITE_MSG() \
    do { \
        LOG_F(DINGUSVIDEO, "%s: write %s %03x.%c = %0*x", this->name.c_str(), \
            get_name_dingusvideoreg(offset), offset, SIZE_ARG(size), size * 2, value); \
    } while (0)

#define DO_WRITE(x) \
    do { \
        x = value; \
        DO_WRITE_MSG(); \
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

        switch (offset >> 2) {
            case DingusVideoRegs::HACTIVE              : DO_WRITE(this->staged.hactive); break;
            case DingusVideoRegs::HFRONT               : DO_WRITE(this->staged.hfront); break;
            case DingusVideoRegs::HBACK                : DO_WRITE(this->staged.hback); break;
            case DingusVideoRegs::HTOTAL               : DO_WRITE(this->staged.htotal); break;
            case DingusVideoRegs::VACTIVE              : DO_WRITE(this->staged.vactive); break;
            case DingusVideoRegs::VFRONT               : DO_WRITE(this->staged.vfront); break;
            case DingusVideoRegs::VBACK                : DO_WRITE(this->staged.vback); break;
            case DingusVideoRegs::VTOTAL               : DO_WRITE(this->staged.vtotal); break;
            case DingusVideoRegs::TIMING_FLAGS         : DO_WRITE(this->staged.timing_flags); break;
            case DingusVideoRegs::PIXEL_CLOCK          : DO_WRITE(this->staged.pixel_clock); break;
            case DingusVideoRegs::PIXEL_DEPTH          : DO_WRITE(this->staged.pixel_depth); break;
            case DingusVideoRegs::FRAMEBUFFER_BASE     : DO_WRITE(this->staged.framebuffer_base); break;
            case DingusVideoRegs::FRAMEBUFFER_ROWBYTES : DO_WRITE(this->staged.framebuffer_rowbytes); break;
            case DingusVideoRegs::HWCURSOR_BASE        : DO_WRITE(this->staged.hwcursor_base); break;
            case DingusVideoRegs::HWCURSOR_WIDTH       : DO_WRITE(this->staged.hwcursor_width); break;
            case DingusVideoRegs::HWCURSOR_POS         : DO_WRITE(this->staged.hwcursor_pos); break;
            case DingusVideoRegs::COLOR_INDEX          : DO_WRITE(this->color_index); break;
            case DingusVideoRegs::MON_SENSE: {
                DO_WRITE_MSG();
                uint8_t dirs = ((value >> 3) & 7) ^ 7;
                uint8_t levels = ((value & 7) & dirs) | (dirs ^ 7);
                this->mon_sense = value & 0x3F;
                this->cur_mon_id = this->disp_id->read_monitor_sense(levels, dirs);
                break;
            }
            case DingusVideoRegs::COLOR_DATA:
                LOG_F(DINGUSVIDEO, "%s: write %s[0x%x] %03x.%c = %0*x", this->name.c_str(),
                    get_name_dingusvideoreg(offset), this->color_index, offset, SIZE_ARG(size), size*2, value);
                this->staged.colors[this->color_index] = value;
                this->color_index = (this->color_index + 1) % 0x300; // 0 = indexed color, 0x100 = gamma, 0x200 = cursor
                break;
            case DingusVideoRegs::IMMEDIATE_FLAGS: {
                DO_WRITE_MSG();

                bool do_update_display = false;
                bool do_disable_display = false;
                bool do_enable_display = false;
                bool do_check_sync = false;

                if (value & DO_LATCH) {
                    this->latched = this->staged;
                    do_update_display = true;
                }

                CHANGE_BIT(this->immediate_flags, DISABLE_TIMING,
                    {
                        if (bitval) {
                            do_disable_display = true;
                        } else {
                            do_enable_display = true;
                        }
                    }
                );
                CHANGE_BIT(this->immediate_flags, HWCURSOR_ENABLE,
                    {
                        if (bitval) {
                            this->cursor_ctrl_cb(true);
                        } else {
                            this->cursor_ctrl_cb(false);
                        }
                    }
                );
                CHANGE_BIT(this->immediate_flags, VSYNC_DISABLE, do_check_sync = true; );
                CHANGE_BIT(this->immediate_flags, HSYNC_DISABLE, do_check_sync = true; );
                CHANGE_BIT(this->immediate_flags, CSYNC_DISABLE, do_check_sync = true; );
                CHANGE_BIT(this->immediate_flags, BLANK_DISABLE,
                    {
                        if (bitval)
                            this->blank_on = false;
                        else {
                            this->blank_on = true;
                            this->blank_display();
                        }
                    }
                );

                if (do_check_sync) {

                }
                if (do_update_display || do_enable_display) {
                    this->enable_display();
                    if (do_enable_display) {
                        this->display_enabled = true;
                    }
                }
                if (do_disable_display) {
                    this->disable_display();
                }
                break;
            }
            case DingusVideoRegs::INT_ENABLE:
                DO_WRITE_MSG();

                if ((this->int_enable ^ value) & VBL_IRQ_CLR) {
                    // clear VBL IRQ on a 1-to-0 transition of INT_ENABLE[VBL_IRQ_CLR]
                    if (!(value & VBL_IRQ_CLR))
                        this->vbl_cb(0);
                }
                this->int_enable = value & 0x0F; // alternates between 0x04 and 0x0c
                break;
            default:
                LOG_F(ERROR, "%s: write %s %03x.%c = %0*x", this->name.c_str(),
                    get_name_dingusvideoreg(offset), offset, SIZE_ARG(size), size * 2, value);
        }
    }
}

void DingusVideo::enable_display()
{
    // calculate active_width and active_height from video timing parameters
    this->active_width  = this->latched.hactive;
    this->active_height = this->latched.vactive;

    // set framebuffer parameters
    this->fb_ptr   = &this->vram_ptr[this->latched.framebuffer_base];
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
        LOG_F(INFO, "%s: refresh rate set to %f Hz", this->name.c_str(), this->refresh_rate);

        this->start_refresh_task();

        this->blank_on = false;

        LOG_F(DINGUSVIDEO, "%s: display enabled", this->name.c_str());
        this->crtc_on = true;
    }
    else {
        LOG_F(DINGUSVIDEO, "%s: display not enabled", this->name.c_str());
        this->blank_on = true;
        this->crtc_on = false;
    }
}

void DingusVideo::disable_display()
{
    this->crtc_on = false;
    LOG_F(INFO, "%s: display disabled", this->name.c_str());
    this->display_enabled = false;
}

// ========================== Device registry stuff ==========================

void DingusVideo::cursor_ctrl_cb(bool cursor_on) {
    if (cursor_on) {
        this->cursor_ovl_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_hw_cursor(this->fb_ptr - 16, dst_buf, dst_pitch);
        };
    } else {
        this->cursor_ovl_cb = nullptr;
    }
}

void DingusVideo::draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch) {
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

static const PropMap DingusVideo_Properties = {
    {"gfxmem_size",
        new IntProperty(4, std::vector<uint32_t>({1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}))},
};

static const DeviceDescription DingusVideo_Descriptor = {
    DingusVideo::create, {"DingusVideoDisplay@0"}, DingusVideo_Properties, HWCompType::PCI_DEV | HWCompType::VIDEO_CTRL
};

REGISTER_DEVICE(DingusVideo, DingusVideo_Descriptor);

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
