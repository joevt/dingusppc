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

#ifndef PCI_HOST_H
#define PCI_HOST_H

#include <devices/common/hwinterrupt.h>
#include <devices/common/hwcomponent.h>
#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

#define DEV_FUN(dev_num,fun_num) (((dev_num) << 3) | (fun_num))

typedef struct {
    const char *    slot_name;
    IntSrc          int_src;
} PciIrqMap;

class PCIBase;
class PCIBridgeBase;

class PCIHost : virtual public HWComponent {
friend class PCIBase;
public:
    PCIHost() : HWComponent("PCIHost") {
        this->dev_map.clear();
        io_space_devs.clear();
    }
    ~PCIHost() = default;

    // HWComponent methods

    virtual HWComponent* add_device(int32_t unit_address, HWComponent *dev_obj, const std::string &name = "") override;
    virtual bool remove_device(int32_t unit_address) override;
    virtual std::string get_child_unit_address_string(int32_t unit_address) override;
    int32_t parse_child_unit_address_string(const std::string unit_address_string, HWComponent*& hwc) override;
    virtual HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) override;

    // PCIHost methods

    virtual AddressMapEntry* pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);
    virtual bool           pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);

    virtual PCIBase *attach_pci_device(const std::string& dev_name, int slot_id);

    virtual bool pci_io_read_loop (uint32_t offset, int size, uint32_t &res);
    virtual bool pci_io_write_loop(uint32_t offset, int size, uint32_t value);

    virtual uint32_t pci_io_read_broadcast (uint32_t offset, int size);
    virtual void     pci_io_write_broadcast(uint32_t offset, int size, uint32_t value);

    virtual PCIBase *pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num);
    virtual PCIBase *pci_find_device(uint8_t dev_num, uint8_t fun_num);

    virtual void set_irq_map(const std::map<int,PciIrqMap> &irq_map) {
        this->my_irq_map = irq_map;
    }
    virtual PostInitResultType pcihost_device_postinit();

    virtual bool register_pci_int(PCIBase* dev_instance);
    virtual void set_interrupt_controller(InterruptCtrl * int_ctrl_obj) {
        this->int_ctrl = int_ctrl_obj;
    }
    virtual InterruptCtrl *get_interrupt_controller();

protected:
    std::unordered_map<int, PCIBase*> dev_map;
    std::vector<PCIBase*>             io_space_devs;
    std::vector<PCIBridgeBase*>       bridge_devs;
    std::map<int,PciIrqMap>           my_irq_map;

    InterruptCtrl   *int_ctrl = nullptr;

private:
    void pci_register_device(int dev_fun_num, PCIBase* dev_instance);
    void pci_unregister_device(int dev_fun_num);
};

#endif /* PCI_HOST_H */
