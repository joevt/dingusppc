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

#ifndef NV47_H
#define NV47_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

class NV47 : public PCIVideoCtrl {
public:
    NV47(const std::string &dev_name);
    ~NV47() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<NV47>(new NV47(dev_name));
    }

    // HWComponent methods
    PostInitResultType device_postinit() override;

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, const AccessDetails details) override;
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details) override;

    // I/O space access methods
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) override;
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) override;

protected:
    void notify_bar_change(int bar_num);
    const char* get_reg_name(uint32_t reg_offset);
    bool io_access_allowed(uint32_t offset);
    uint32_t read_reg(uint32_t offset, uint32_t size);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);
    uint8_t mm_regs[128] = {0}; // internal registers

    // Video RAM variables
    std::unique_ptr<uint8_t[]>  vram_ptr;
    uint32_t    vram_size;

    uint32_t aperture_count = 6;
    uint32_t aperture_base[6] = { 0, 0, 0, 0, 0, 0 };
    uint32_t aperture_size[6] = { 0x01000000UL, 0x10000000UL, 1, 0x01000000UL, 1, 0x00000080UL };
    uint32_t aperture_flag[6] = {          0  ,        0xC  , 0,          4  , 0,          1   };
    uint32_t io_base = 0;

    int  regop_index = 0;

    DisplayID*  disp_id = nullptr;
};

#endif // NV47_H
