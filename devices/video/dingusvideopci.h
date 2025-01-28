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

/** @file Dingus video output definitions. */

#ifndef DINGUS_VIDEO_H
#define DINGUS_VIDEO_H

#include <devices/video/dingusvideoregs.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

class DingusVideo;

class DingusVideoCtrl : public VideoCtrlBase {
friend class DingusVideo;
public:
    DingusVideoCtrl(const std::string &dev_name);
    ~DingusVideoCtrl() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<DingusVideoCtrl>(new DingusVideoCtrl(dev_name));
    }

    void enable_display();
    void disable_display();

    // HWComponent methods
    PostInitResultType device_postinit() override;

private:
    void cursor_ctrl_cb(bool cursor_on);
    void draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch);

    typedef struct {
        uint32_t hactive;
        uint32_t hsyncbegin;
        uint32_t hsyncend;
        uint32_t htotal;

        uint32_t vactive;
        uint32_t vsyncbegin;
        uint32_t vsyncend;
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

    DisplayID*   disp_id = nullptr;
    DingusVideo* dingus_pci = nullptr;

    bool         display_enabled = false;

    uint32_t     cur_mon_id = 0;
    uint32_t     mon_sense = 0;

    uint32_t     immediate_flags = 0;
    uint32_t     int_enable = 0;
    uint32_t     int_status = 0;
    uint32_t     last_int_status = -1;
    int          last_int_status_read_count = 0;

    uint32_t     color_index;

    DingusVideoRegValues staged = {};
    DingusVideoRegValues latched = {};
};

class DingusVideo : public PCIDevice {
friend class DingusVideoCtrl;
public:
    DingusVideo(const std::string &dev_name);
    ~DingusVideo() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<DingusVideo>(new DingusVideo(dev_name));
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

protected:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new,
                        int bar_num);
    void notify_bar_change(int bar_num);

    // HWComponent methods
    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

private:
    std::unique_ptr<uint8_t[]>  vram_ptr;

    uint32_t    vram_base = 0;
    uint32_t    vram_size = 0;
    uint32_t    regs_base = 0;
    uint32_t    regs_size = 0;

    int         num_displays = 1;
};

#endif // DINGUS_VIDEO_H
