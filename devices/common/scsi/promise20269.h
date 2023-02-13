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

#ifndef PROMISE20269_H
#define PROMISE20269_H

#include <devices/common/pci/pcidevice.h>

class promise20269 : public PCIDevice {
public:
    promise20269();
    ~promise20269() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<promise20269>(new promise20269());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, const AccessDetails details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details);

    // I/O space access methods
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

protected:
    void notify_bar_change(int bar_num);
    int io_access_allowed(uint32_t offset);

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);

    uint32_t aperture_count = 6;
    uint32_t aperture_base[6] = { 0, 0, 0, 0, 0, 0 };
    uint32_t aperture_size[6] = { 8, 4, 8, 4, 16, 0x10000 };
    uint32_t aperture_flag[6] = { 1, 1, 1, 1, 1, 0 };
};

#endif // PROMISE20269_H
