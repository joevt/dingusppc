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

/** DEC 21154 PCI-to-PCI bridge emulation. */

#include <devices/common/hwcomponent.h>
#include <devices/common/pci/dec21154.h>
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>
#include <loguru.hpp>

#include <cinttypes>

DecPciBridge::DecPciBridge(std::string name, bool for_yosemite) : PCIBridge(name)
{
    supports_types(HWCompType::PCI_HOST | HWCompType::PCI_DEV);

    // initialize PCI config header
    this->vendor_id   = PCI_VENDOR_DEC;
    this->device_id   = 0x0026;
    this->class_rev   = 0x06040002;
    this->cache_ln_sz = 0;
    this->command     = 0;
    this->status      = 0x2B0;
    this->for_yosemite = for_yosemite;
}

int DecPciBridge::device_postinit()
{
    if (!this->for_yosemite)
        return PCIBridge::device_postinit();
        
    std::string pci_dev_name;

    static const std::map<std::string, int> pci_slots = {
        {"pci_FireWire", DEV_FUN(0x00,0)},
        {"pci_ATA"     , DEV_FUN(0x01,0)},
        {"pci_J11"     , DEV_FUN(0x02,0)},
        {"pci_J10"     , DEV_FUN(0x03,0)},
        {"pci_J9"      , DEV_FUN(0x04,0)},
        {"pci_USB"     , DEV_FUN(0x06,0)},
    };

    for (auto& slot : pci_slots) {
        pci_dev_name = GET_STR_PROP(slot.first);
        if (!pci_dev_name.empty()) {
            this->attach_pci_device(pci_dev_name, slot.second, std::string("@") + slot.first);
        }
    }

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id_FireWire = this->int_ctrl->register_dev_int(IntSrc::FIREWIRE     );
    this->irq_id_ATA      = this->int_ctrl->register_dev_int(IntSrc::ATA          );
    this->irq_id_J11      = this->int_ctrl->register_dev_int(IntSrc::PCI_J11      );
    this->irq_id_J10      = this->int_ctrl->register_dev_int(IntSrc::PCI_J10      );
    this->irq_id_J9       = this->int_ctrl->register_dev_int(IntSrc::PCI_J9       );
    this->irq_id_USB      = this->int_ctrl->register_dev_int(IntSrc::USB          );

    return 0;
}

void DecPciBridge::pci_interrupt(uint8_t irq_line_state, PCIBase *dev)
{
    if (!this->for_yosemite)
        return PCIBridge::pci_interrupt(irq_line_state, dev);

    auto it = std::find_if(dev_map.begin(), dev_map.end(),
        [&dev](const std::pair<int, PCIBase*> &p) {
            return p.second == dev;
        }
    );
 
    if (it == dev_map.end()) {
        LOG_F(ERROR, "Interrupt from unknown device %s", dev->get_name().c_str());
    }
    else {
        uint32_t irq_id;
        switch (it->first) {
            case DEV_FUN(0x00,0): irq_id = this->irq_id_FireWire; break;
            case DEV_FUN(0x01,0): irq_id = this->irq_id_ATA     ; break;
            case DEV_FUN(0x02,0): irq_id = this->irq_id_J11     ; break;
            case DEV_FUN(0x03,0): irq_id = this->irq_id_J10     ; break;
            case DEV_FUN(0x04,0): irq_id = this->irq_id_J9      ; break;
            case DEV_FUN(0x06,0): irq_id = this->irq_id_USB     ; break;
            default:
                LOG_F(ERROR, "Interrupt from device %s at unexpected device/function %02x.%x", dev->get_name().c_str(), it->first >> 3, it->first & 7);
                return;
        }
        if (this->int_ctrl)
            this->int_ctrl->ack_int(irq_id, irq_line_state);
    }
}

uint32_t DecPciBridge::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    if (reg_offs < 64) {
        return PCIBridge::pci_cfg_read(reg_offs, details);
    }

    switch (reg_offs) {
    case CHIP_CTRL:
        return (this->arb_ctrl << 16) | (this-> diag_ctrl << 8) | this->chip_ctrl;
    case PSERR_EVENT_DIS:
        return (this->gpio_out_en << 16) | this->pserr_event_dis;
    case SEC_CLK_CTRL:
        return this->sec_clock_ctrl;
    default:
        LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    }

    return 0;
}

void DecPciBridge::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    if (reg_offs < 64) {
        PCIBridge::pci_cfg_write(reg_offs, value, details);
        return;
    }

    switch (reg_offs) {
    case CHIP_CTRL:
        this->chip_ctrl =  value & 0xFFU;
        this->diag_ctrl = (value >> 8) & 0xFFU;
        this->arb_ctrl  =  value >> 16;
        break;
    case PSERR_EVENT_DIS:
        this->pserr_event_dis =  value & 0xFFU;
        this->gpio_out_en     = (value >> 16) & 0xFFU;
        break;
    case SEC_CLK_CTRL:
        this->sec_clock_ctrl = value & 0xFFFFU;
        break;
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

static const PropMap Dec21154Yosemite_Properties = {
    {"pci_FireWire",
        new StrProperty("")},
    {"pci_ATA",
        new StrProperty("")},
    {"pci_J11",
        new StrProperty("")},
    {"pci_J10",
        new StrProperty("")},
    {"pci_J9",
        new StrProperty("")},
    {"pci_USB",
        new StrProperty("")},
};

static const DeviceDescription Dec21154_Descriptor = {
    DecPciBridge::create, {}, {}
};

static const DeviceDescription Dec21154Yosemite_Descriptor = {
    DecPciBridge::create_yosemite, {}, Dec21154Yosemite_Properties
};

REGISTER_DEVICE(Dec21154, Dec21154_Descriptor);
REGISTER_DEVICE(Dec21154Yosemite, Dec21154Yosemite_Descriptor);
