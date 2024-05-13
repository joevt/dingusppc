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

/** DEC 21154 PCI-to-PCI bridge definitions. */

#ifndef DEC_P2P_H
#define DEC_P2P_H

#include <devices/common/pci/pcibridge.h>
#include <devices/common/hwinterrupt.h>

#include <cinttypes>
#include <string>

enum {
    CHIP_CTRL           = 0x40,
    PSERR_EVENT_DIS     = 0x64,
    SEC_CLK_CTRL        = 0x68,
};

class DecPciBridge : public PCIBridge {
public:
    DecPciBridge(std::string name, bool for_yosemite);
    ~DecPciBridge() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<DecPciBridge>(new DecPciBridge("DEC21154", false));
    }

    static std::unique_ptr<HWComponent> create_yosemite() {
        return std::unique_ptr<DecPciBridge>(new DecPciBridge("DEC21154Yosemite", true));
    }

    // HWComponent methods
    int device_postinit();

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    // PCIBridgeBase methods
    virtual void pci_interrupt(uint8_t irq_line_state, PCIBase *dev);

private:
    uint8_t     chip_ctrl           = 0;
    uint8_t     diag_ctrl           = 0;
    uint16_t    arb_ctrl            = 0x0200;
    uint8_t     pserr_event_dis     = 0;
    uint8_t     gpio_out_data       = 0;
    uint8_t     gpio_out_en         = 0;
    uint8_t     gpio_in_data        = 0;
    uint16_t    sec_clock_ctrl      = 0;

    bool            for_yosemite    = false;
    InterruptCtrl*  int_ctrl        = nullptr;
    uint32_t        irq_id_FireWire = 0;
    uint32_t        irq_id_ATA      = 0;
    uint32_t        irq_id_J11      = 0;
    uint32_t        irq_id_J10      = 0;
    uint32_t        irq_id_J9       = 0;
    uint32_t        irq_id_USB      = 0;
};

#endif // DEC_P2P_H
