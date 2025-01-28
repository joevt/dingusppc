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

/** @file Dingus video output definitions. */

#ifndef DINGUS_VIDEO_H
#define DINGUS_VIDEO_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

// Memory-mapped registers.
namespace DingusVideoRegs {

enum DingusVideoRegs : int {
    HACTIVE,
    HFRONT,
    HBACK,
    HTOTAL,
    VACTIVE,
    VFRONT,
    VBACK,
    VTOTAL,
    MON_SENSE,
    TIMING_FLAGS,
    IMMEDIATE_FLAGS,
    PIXEL_CLOCK,
    PIXEL_DEPTH,
    FRAMEBUFFER_BASE,
    FRAMEBUFFER_ROWBYTES,
    INT_ENABLE,
    INT_STATUS,
    HWCURSOR_BASE,
    HWCURSOR_WIDTH,
    HWCURSOR_POS,
    COLOR_INDEX,
    COLOR_DATA
};

}; // namespace DingusVideoRegs

// Bit definitions.
enum {
    // TIMING_FLAGS
    INTERLACED      = 1 <<  0, // 0 - interlaced, 1 - progressive
    VSYNC_POLARITY  = 1 <<  1, // 0 - negative, 1 - positive
    HSYNC_POLARITY  = 1 <<  2, // 0 - negative, 1 - positive
    HWCURSOR_24     = 1 <<  3, // 0 - indexed color - 24 bit color cursor

    // IMMEDIATE_FLAGS
    HWCURSOR_ENABLE = 1 <<  0, // 1 - enable hw cursor, 0 - disable it
    DISABLE_TIMING  = 1 <<  1, // 0 - enable video timing, 1 - disable it
    VSYNC_DISABLE   = 1 <<  2, // 0 - enable vertical sync, 1 - disable it
    HSYNC_DISABLE   = 1 <<  3, // 0 - enable horizontal sync, 1 - disable it
    CSYNC_DISABLE   = 1 <<  4, // 0 - enable composite sync, 1 - disable it
    BLANK_DISABLE   = 1 <<  5, // 0 - enable blanking, 1 - disable it
    DO_LATCH        = 1 <<  6, // 1 - do latch all regs

    // INT_ENABLE
    VBL_IRQ_CLR  = 1 << 0, // VBL interrupt clear  bit
    VBL_IRQ_EN   = 1 << 1, // VBL interrupt enable bit

    // INT_STATUS
    VBL_IRQ_STAT = 1 << 0, // VBL interrupt status bit (INT_STATUS)
};

class DingusVideo : public PCIVideoCtrl {
public:
    DingusVideo();
    ~DingusVideo() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<DingusVideo>(new DingusVideo());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;


protected:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new,
                        int bar_num);
    void notify_bar_change(int bar_num);

    void enable_display();
    void disable_display();

    // HWComponent methods
    PostInitResultType device_postinit() override;
    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

private:
    void cursor_ctrl_cb(bool cursor_on);
    void draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch);

    typedef struct {
        uint32_t hactive;
        uint32_t hfront;
        uint32_t hback;
        uint32_t htotal;
        uint32_t vactive;
        uint32_t vfront;
        uint32_t vback;
        uint32_t vtotal;
        uint32_t pixel_clock;
        uint32_t pixel_depth;
        uint32_t framebuffer_base;
        uint32_t framebuffer_rowbytes;
        uint32_t hwcursor_base;
        uint32_t hwcursor_width;
        uint32_t hwcursor_pos;
        uint32_t timing_flags;
        uint32_t colors[0x300];
    } DingusVideoRegValues;

    DisplayID*  disp_id = nullptr;

    std::unique_ptr<uint8_t[]>  vram_ptr;

    uint32_t    vram_base = 0;
    uint32_t    vram_size;
    uint32_t    regs_base = 0;
    uint32_t    regs_size = 0x1000;

    bool        display_enabled = false;

    uint32_t    cur_mon_id = 0;
    uint32_t    mon_sense = 0;

    uint32_t    immediate_flags = 0;
    uint32_t    int_enable = 0;
    uint32_t    int_status = 0;
    uint32_t    last_int_status = -1;
    int         last_int_status_read_count = 0;

    uint32_t    color_index;

    DingusVideoRegValues staged = {};
    DingusVideoRegValues latched = {};
};

#endif // DINGUS_VIDEO_H
