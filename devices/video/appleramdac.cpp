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

/** Apple RAMDAC ASICs (RaDACal & DACula) emulation. */

#include <core/bitops.h>
#include <devices/video/appleramdac.h>
#include <loguru.hpp>
#include <memaccess.h>

namespace loguru {
    enum : Verbosity {
        Verbosity_RAMDAC = loguru::Verbosity_9,
        Verbosity_RAMDAC_EXTRA = loguru::Verbosity_9,
    };
}

AppleRamdac::AppleRamdac(DacFlavour flavour) {
    this->flavour =  flavour;

    supports_types(HWCompType::IOBUS_DEV);

    set_name(this->flavour == DacFlavour::DACULA ? "DACula" : "RaDACal");
}

uint16_t AppleRamdac::iodev_read(uint32_t address) {
    uint16_t result;

    switch(address) {
    case RamdacRegs::MULTI:
        switch(this->dac_addr) {
        case RamdacRegs::MISC_CTRL:
            result = this->dac_cr;
            LOG_F(RAMDAC_EXTRA, "%s: read  MISC_CTRL = 0x%02x", this->name.c_str(), result);
            break;
        case RamdacRegs::PLL_CTRL:
            result = this->pll_cr;
            LOG_F(RAMDAC_EXTRA, "%s: read  PLL_CTRL = 0x%02x", this->name.c_str(), result);
            break;
        case RamdacRegs::VENDOR_ID:
            result = this->dac_vendor;
            LOG_F(RAMDAC_EXTRA, "%s: read  VENDOR_ID = 0x%02x", this->name.c_str(), result);
            break;
        default:
            LOG_F(WARNING, "%s: read  MULTI 0x%02x", this->name.c_str(), this->dac_addr);
            result = 0;
        }
        break;
    case RamdacRegs::CLUT_DATA:
        if (this->comp_index == 0) {
            this->get_clut_entry_cb(this->dac_addr, this->clut_color);
        }
        result = this->clut_color[this->comp_index];
        LOG_F(RAMDAC, "%s: read  CLUT_DATA 0x%02x.%c = %02x", this->name.c_str(), this->dac_addr, "rgb"[comp_index], result);
        this->comp_index++;
        if (this->comp_index >= 3) {
            this->dac_addr++; // auto-increment CLUT address
            this->comp_index = 0;
        }
        break;
    default:
        LOG_F(WARNING, "%s: read  0x%02x", this->name.c_str(), address);
        result = 0;
    }
    return result;
}

void AppleRamdac::iodev_write(uint32_t address, uint16_t value) {
    switch(address) {
    case RamdacRegs::ADDRESS:
        LOG_F(RAMDAC_EXTRA, "%s: write ADDRESS = 0x%02x", this->name.c_str(), value);
        this->dac_addr = value;
        this->comp_index = 0;
        break;
    case RamdacRegs::CURSOR_CLUT:
        LOG_F(RAMDAC_EXTRA, "%s: write CURSOR_CLUT 0x%02x.%c = 0x%02x",
            this->name.c_str(), this->dac_addr, "rgb"[comp_index], value);
        this->clut_color[this->comp_index++] = value;
        if (this->comp_index >= 3) {
            this->cursor_clut[this->dac_addr & 7] = (this->clut_color[0] << 16) |
            (this->clut_color[1] << 8) | this->clut_color[2];
            this->dac_addr++; // auto-increment CLUT address
            this->comp_index = 0;
        }
        break;
    case RamdacRegs::MULTI:
        switch (this->dac_addr) {
        case RamdacRegs::CURSOR_POS_HI:
            LOG_F(RAMDAC_EXTRA, "%s: write CURSOR_POS_HI = 0x%02x", this->name.c_str(), value);
#ifdef CURSOR_LO_DELAY // HACK: prevents artifacts in some cases, disabled by default
            if (this->cursor_timer_id) {
                TimerManager::get_instance()->cancel_timer(this->cursor_timer_id);
                cursor_timer_id = 0;
            }
            this->cursor_xpos = (value << 8) | this->cursor_pos_lo;
#else
            this->cursor_xpos = (value << 8) | (this->cursor_xpos & 0xff);
#endif
            break;
        case RamdacRegs::CURSOR_POS_LO:
            LOG_F(RAMDAC_EXTRA, "%s: write CURSOR_POS_LO = 0x%02x", this->name.c_str(), value);
#ifdef CURSOR_LO_DELAY // HACK: prevents artifacts in some cases, disabled by default
            if (this->cursor_timer_id) {
                TimerManager::get_instance()->cancel_timer(this->cursor_timer_id);
                this->cursor_xpos = (this->cursor_xpos & 0xff00) | (this->cursor_pos_lo & 0x00ff);
                cursor_timer_id   = 0;
            }
            this->cursor_pos_lo   = value;
            this->cursor_timer_id = TimerManager::get_instance()->add_oneshot_timer(
                NS_PER_SEC / 60, [this]() {
                    this->cursor_xpos = (this->cursor_xpos & 0xff00) | (this->cursor_pos_lo & 0x00ff);
                });
#else
            this->cursor_xpos = (this->cursor_xpos & 0xff00) | (value & 0x00ff);
#endif
            break;
        case RamdacRegs::MISC_CTRL:
            LOG_F(RAMDAC, "%s: write MISC_CTRL = 0x%02x", this->name.c_str(), value);
            if (bit_changed(this->dac_cr, value, 1)) {
                if (value & 2) {
                    LOG_F(RAMDAC, "%s: HW cursor enabled!", this->name.c_str());
                    this->cursor_ctrl_cb(true);
                } else {
                    LOG_F(RAMDAC, "%s: HW cursor disabled!", this->name.c_str());
                    this->cursor_ctrl_cb(false);
                }
            }
            this->dac_cr = value;
            break;
        case RamdacRegs::DBL_BUF_CTRL:
            LOG_F(RAMDAC, "%s: write DBL_BUF_CTRL = 0x%02x", this->name.c_str(), value);
            this->dbl_buf_cr = value;
            break;
        case RamdacRegs::TEST_CTRL:
            LOG_F(RAMDAC, "%s: write TEST_CTRL = 0x%02x", this->name.c_str(), value);
            this->tst_cr = value;
            if (value & 1)
                LOG_F(WARNING, "%s: DAC test enabled!", this->name.c_str());
            break;
        case RamdacRegs::PLL_CTRL:
            LOG_F(RAMDAC, "%s: write PLL_CTRL = 0x%02x", this->name.c_str(), value);
            this->pll_cr = value;
            break;
        case RamdacRegs::VIDCLK_M_SET_A:
            LOG_F(RAMDAC, "%s: write VIDCLK_M_SET_A = 0x%02x", this->name.c_str(), value);
            this->clk_m[0] = value;
            break;
        case RamdacRegs::VIDCLK_PN_SET_A:
            LOG_F(RAMDAC, "%s: write VIDCLK_PN_SET_A = 0x%02x", this->name.c_str(), value);
            this->clk_pn[0] = value;
            break;
        case RamdacRegs::VIDCLK_M_SET_B:
            LOG_F(RAMDAC, "%s: write VIDCLK_M_SET_B = 0x%02x", this->name.c_str(), value);
            this->clk_m[1] = value;
            break;
        case RamdacRegs::VIDCLK_PN_SET_B:
            LOG_F(RAMDAC, "%s: write VIDCLK_PN_SET_B = 0x%02x", this->name.c_str(), value);
            this->clk_pn[1] = value;
            break;
        case RamdacRegs::VENDOR_ID:
            LOG_F(ERROR, "%s: write VENDOR_ID = 0x%02x", this->name.c_str(), value);
            break;
        case RamdacRegs::DAC_29:
            LOG_F(ERROR, "%s: write DAC_29 = 0x%02x", this->name.c_str(), value);
            break;
        default:
            LOG_F(ERROR, "%s: write MULTI 0x%02x = 0x%02x", this->name.c_str(), this->dac_addr, value);
        }
        break;
    case RamdacRegs::CLUT_DATA:
        LOG_F(RAMDAC, "%s: write CLUT_DATA 0x%02x.%c = 0x%02x", this->name.c_str(), this->dac_addr, "rgb"[comp_index], value);
        if (this->comp_index == 0) {
            this->get_clut_entry_cb(this->dac_addr, this->clut_color);
        }
        this->clut_color[this->comp_index++] = value;
        if (this->comp_index >= 3) {
            this->set_clut_entry_cb(this->dac_addr, this->clut_color);
            this->dac_addr++; // auto-increment CLUT address
            this->comp_index = 0;
        }
        break;
    default:
        LOG_F(ERROR, "%s: write 0x%02x = 0x%02x", this->name.c_str(), address, value);
    }
}

int AppleRamdac::get_clock_div() {
    return 1 << (((dac_cr >> 6) + 1) & 3);
}

int AppleRamdac::get_pix_width() {
    return 1 << (((this->dac_cr >> 2) & 3) + 3);
}

int AppleRamdac::get_dot_freq() {
    uint8_t m = this->clk_m[this->pll_cr & 1];
    uint8_t p = this->clk_pn[this->pll_cr & 1] >> 5;
    uint8_t n = this->clk_pn[this->pll_cr & 1] & 0x1F;

    double dot_freq;
    if (this->dac_vendor == DACULA_VENDOR_ATT)
        dot_freq = 15000000.0f * (double)m / ((double)n + 2) / (double)(1 << p);
    else if (this->dac_vendor == DACULA_VENDOR_SIERRA)
        dot_freq = 14318180.0f * (double)m / ((double)n * (double)(1 << p));
    else {
        dot_freq = 14318180.0f * (double)m / (double)n / (double)(1 << p);
        LOG_F(ERROR, "%s: unknown VENDOR_ID", this->name.c_str());
    }
    return static_cast<int>(dot_freq + 0.5f);
}

// =========================== HW cursor stuff =============================
void AppleRamdac::measure_hw_cursor(uint8_t *fb_ptr) {
    uint8_t *src_bw_ptr = fb_ptr + this->fb_pitch * (this->video_height - 1);

    int cur_pos_y_start = -1;
    int cur_pos_y_end   = -1;

    // forward scanning to find the first line of the cursor
    for (int h = 0; h < this->video_height; h++, fb_ptr += this->fb_pitch) {
        if (((uint64_t *)fb_ptr)[0] | ((uint64_t *)fb_ptr)[1]) {
            cur_pos_y_start = h;
            break;
        }
    }

    if (cur_pos_y_start < 0)
        return; // bail out because no cursor data start was found

    // backward scanning to find the last line of the cursor
    for (int h = this->video_height - 1; h >= 0; h--, src_bw_ptr -= this->fb_pitch) {
        if (((uint64_t *)src_bw_ptr)[0] | ((uint64_t *)src_bw_ptr)[1]) {
            cur_pos_y_end = h;
            break;
        }
    }

    if (cur_pos_y_end < 0)
        return; // bail out because no cursor data end was found

    this->cursor_height = cur_pos_y_end - cur_pos_y_start + 1;
    this->cursor_ypos   = cur_pos_y_start;
}

void AppleRamdac::draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch) {
    int num_pixels = this->video_width - this->cursor_xpos;
    if (num_pixels <= 0)
        return;
    if (num_pixels > 32)
        num_pixels = 32;

    this->measure_hw_cursor(src_buf);

    int num_words = unsigned(num_pixels + 15) / 16;

    uint64_t mask0 = (~0LL) << ((num_pixels >= 16) ? 0 : ((16 - num_pixels) * 4));
    uint64_t mask1 = (num_pixels <= 16) ? 0LL : ((~0LL) << ((32 - num_pixels) * 4));

    uint8_t *src_row = src_buf + this->fb_pitch * this->cursor_ypos;
    uint8_t *dst_row = dst_buf + this->cursor_ypos * dst_pitch +
                           this->cursor_xpos * sizeof(uint32_t);
    int src_pitch = this->fb_pitch;

    uint32_t *color = &this->cursor_clut[0];

    for (int h = this->cursor_height; h > 0; h--) {
        uint8_t* dst_16 = dst_row;
        for (int x = 0; x < num_words; x++) {
            uint8_t* dst_1 = dst_16;
            uint64_t pix_data = READ_QWORD_BE_A(src_row + x * sizeof(uint64_t)) & (x ? mask1 : mask0);
            while (pix_data) {
                uint8_t pix = pix_data >> 60; // each pixel is 4 bits wide
                if (pix & 8) { // check control bit: 0 - transparent, 1 - opaque
                    WRITE_DWORD_LE_A(dst_1, color[pix & 7]);
                } else if (pix & 1) {
                    uint32_t c = (((READ_DWORD_LE_A(dst_1) >> 7) & 0x010101) * 0xFFU) ^ 0xFFFFFFU;
                    WRITE_DWORD_LE_A(dst_1, c);
                }
                pix_data <<= 4;
                dst_1 += sizeof(uint32_t);
            }
            dst_16 += 16 * sizeof(uint32_t);
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}
