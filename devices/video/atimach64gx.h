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

/** ATI Mach64 GX definitions. */

#ifndef ATI_MACH64_GX_H
#define ATI_MACH64_GX_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>
#include <devices/video/atimach64defs.h>

#include <cinttypes>
#include <memory>

class AtiMach64Gx : public PCIVideoCtrl {
public:
    AtiMach64Gx(const std::string &dev_name);
    ~AtiMach64Gx() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<AtiMach64Gx>(new AtiMach64Gx(dev_name));
    }

    // PCI device methods
    bool supports_io_space(void) override {
        return true;
    }

    // I/O space access methods
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) override;
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) override;

    // HWComponent methods
    PostInitResultType device_postinit() override;
    HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

protected:
    void notify_bar_change(int bar_num);
    const char* get_reg_name(uint32_t reg_offset);
    const char* rgb514_get_reg_name(uint32_t reg_offset);
    const char* rgb514_get_ind_reg_name(uint32_t reg_offset);
    bool io_access_allowed(uint32_t offset);
    uint32_t read_reg(uint32_t reg_offset, uint32_t size);
    void write_reg(uint32_t reg_offset, uint32_t value, uint32_t size);
    void crtc_update();
    uint8_t rgb514_read_reg(uint8_t reg_addr);
    void rgb514_write_reg(uint8_t reg_addr, uint8_t value);
    uint8_t rgb514_read_ind_reg(uint8_t reg_addr);
    void rgb514_write_ind_reg(uint8_t reg_addr, uint8_t value);
    void verbose_pixel_format(int crtc_index);
    void vidc_draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) override;
    void vidc_get_cursor_position(int& x, int& y) override;

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);

    uint32_t    regs[256] = {}; // internal registers

    int         vram_size;

    // main aperture (16MB)
    const uint32_t aperture_count = 1;
    const uint32_t aperture_size[1] = { 0x1000000 };
    const uint32_t aperture_flag[1] = { 0 };
    uint32_t aperture_base[1] = { 0 };

    uint32_t    config_cntl[2] = { 2, 0 };
    uint32_t    mm_regs_offset = MM_REGS_0_OFF;

    // RGB514 RAMDAC state
    uint8_t     dac_idx_lo = 0;
    uint8_t     dac_idx_hi = 0;

    uint8_t     clut_index = 0;
    uint8_t     comp_index = 0;
    uint8_t     clut_color[3] = {0};

    uint8_t     clut_index_rd = 0;
    uint8_t     comp_index_rd = 0;
    uint8_t     clut_color_rd[3] = {0};

    uint8_t     dac_regs[256] = {0};

    DisplayID*                  disp_id = nullptr;
    std::unique_ptr<uint8_t[]>  vram_ptr;
};

#endif // ATI_MACH64_GX_H
