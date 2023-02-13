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

#ifndef NEC_OHCI_H
#define NEC_OHCI_H

#include <devices/common/usb/usbohci.h>

class NecOhci : public USBHostOHCI {
public:
    NecOhci(const std::string &dev_name);
    ~NecOhci() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<NecOhci>(new NecOhci(dev_name));
    }

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, const AccessDetails details);
};

#endif // NEC_OHCI_H
