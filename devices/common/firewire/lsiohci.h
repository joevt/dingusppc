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

#ifndef LSIOHCI_H
#define LSIOHCI_H

#include <devices/common/pci/pcidevice.h>

class LsiOhci : public PCIDevice {
public:
    LsiOhci(const std::string &dev_name);
    ~LsiOhci() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<LsiOhci>(new LsiOhci(dev_name));
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, const AccessDetails details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details);

protected:
    void notify_bar_change(int bar_num);

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);

    uint32_t aperture_count = 1;
    uint32_t aperture_base[1] = { 0 };
    uint32_t aperture_size[1] = { 0x1000 };
    uint32_t aperture_flag[1] = { 0 };
};

#endif // LSIOHCI_H
