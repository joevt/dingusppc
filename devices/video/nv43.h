/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#ifndef NV43_H
#define NV43_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

class NV43 : public PCIDevice, public VideoCtrlBase {
public:
    NV43();
    ~NV43() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<NV43>(new NV43());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

protected:
    void notify_bar_change(int bar_num);

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);

    // Video RAM variables
    std::unique_ptr<uint8_t[]>  vram_ptr;
    uint32_t    vram_size;

    uint32_t aperture_count = 3;
    uint32_t aperture_base[3] = { 0, 0, 0 };
    uint32_t aperture_size[3] = { 0x01000000UL, 0x20000000UL, 0x01000000UL };
    uint32_t aperture_flag[3] = {          0  ,          8  ,          0   };

    int  regop_index = 0;

    std::unique_ptr<DisplayID>  disp_id;
};

#endif // NV43_H
