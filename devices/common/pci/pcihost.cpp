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

#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcibridge.h>
#include <devices/common/pci/pcihost.h>
#include <devices/deviceregistry.h>
#include <machines/machinefactory.h>
#include <endianswap.h>
#include <loguru.hpp>

#include <cinttypes>
#include <regex>
#include <cpu/ppc/ppcemu.h>

void PCIHost::pci_register_device(int dev_fun_num, PCIBase* dev_instance)
{
    if (this->dev_map.count(dev_fun_num)) {
        if (this->dev_map[dev_fun_num] == dev_instance)
            return;
        pci_unregister_device(dev_fun_num);
    }

    int fun_num = dev_fun_num & 7;
    int dev_num = (dev_fun_num >> 3) & 0x1f;
    bool is_multi_function = fun_num != 0;

    for (int other_fun_num = 0; other_fun_num < 8; other_fun_num++) {
        if (this->dev_map.count(DEV_FUN(dev_num, other_fun_num))) {
            is_multi_function = true;
            if (is_multi_function && other_fun_num == 0) {
                this->dev_map[DEV_FUN(dev_num, other_fun_num)]->set_multi_function(true);
            }
        }
    }

    this->dev_map[dev_fun_num] = dev_instance;

    dev_instance->set_host(this);
    if (is_multi_function && fun_num == 0) {
        dev_instance->set_multi_function(true);
    }

    if (dev_instance->supports_io_space()) {
        this->io_space_devs.push_back(dev_instance);
    }

    PCIBridgeBase *bridge = dynamic_cast<PCIBridgeBase*>(dev_instance);
    if (bridge) {
        this->bridge_devs.push_back(bridge);
    }

    LOG_F(INFO, "Registered %s.", dev_instance->get_name_and_unit_address().c_str());
}

bool PCIHost::remove_device(int32_t unit_address)
{
    this->pci_unregister_device(unit_address);
    return HWComponent::remove_device(unit_address);
}

void PCIHost::pci_unregister_device(int dev_fun_num)
{
    if (!this->dev_map.count(dev_fun_num)) {
        return;
    }
    auto dev_instance = this->dev_map[dev_fun_num];

    LOG_F(
        // FIXME: need destructors to remove memory regions and downstream devices.
        ERROR, "%s: pci_unregister_device(%s) not supported yet (every PCI device needs "
            "a working destructor to unregister memory and downstream devices etc.)",
        this->get_name().c_str(), dev_instance->get_name().c_str()
    );

    this->dev_map.erase(dev_fun_num);
}

AddressMapEntry* PCIHost::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->add_mmio_region(start_addr, size, obj);
}

bool PCIHost::pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->remove_mmio_region(start_addr, size, obj);
}

PCIBase *PCIHost::attach_pci_device(const std::string& dev_name, int slot_id)
{
    return dynamic_cast<PCIBase*>(MachineFactory::create_device(
        this, dev_name + this->get_child_unit_address_string(slot_id), HWCompType::PCI_DEV));
}

HWComponent* PCIHost::add_device(int32_t unit_address, HWComponent *dev_obj, const std::string &name)
{
    // register device with the PCI host
    HWComponent *result = HWComponent::add_device(unit_address, dev_obj, name);
    if (unit_address < 0) {
        LOG_F(WARNING, "Not registering %s yet.", dev_obj->get_name_and_unit_address().c_str());
    } else if (unit_address > 0xff) {
        LOG_F(WARNING, "Not registering %s.", dev_obj->get_name_and_unit_address().c_str());
    } else {
        this->pci_register_device(unit_address, dynamic_cast<PCIBase*>(dev_obj));
    }
    return result;
}

InterruptCtrl *PCIHost::get_interrupt_controller() {
    if (!this->int_ctrl) {
        InterruptCtrl *int_ctrl_obj =
            dynamic_cast<InterruptCtrl*>(gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
        this->int_ctrl = int_ctrl_obj;
    }
    return this->int_ctrl;
}

bool PCIHost::register_pci_int(PCIBase* dev_instance) {
    int dev_fun_num = dev_instance->unit_address;
    if (!this->my_irq_map.count(dev_fun_num))
        dev_fun_num &= DEV_FUN(0x1F,0);
    if (this->my_irq_map.count(dev_fun_num)) {
        auto& irq = this->my_irq_map[dev_fun_num];
        IntDetails new_int_detail;
        new_int_detail.int_ctrl_obj = this->get_interrupt_controller();
        if (new_int_detail.int_ctrl_obj && irq.int_src)
            new_int_detail.irq_id = new_int_detail.int_ctrl_obj->register_dev_int(irq.int_src);
        dev_instance->set_int_details(new_int_detail);
        return true;
    }

    PCIBridgeBase *self = dynamic_cast<PCIBridgeBase*>(this);
    if (self) {
        if (!self->int_details.int_ctrl_obj)
            self->host_instance->register_pci_int(self);
        dev_instance->set_int_details(self->int_details);
        return true;
    }
    return false;
}

bool PCIHost::pci_io_read_loop(uint32_t offset, int size, uint32_t &res)
{
    for (auto& dev : this->io_space_devs) {
        if (dev->pci_io_read(offset, size, &res)) {
            return true;
        }
    }
    return false;
}

bool PCIHost::pci_io_write_loop(uint32_t offset, int size, uint32_t value)
{
    for (auto& dev : this->io_space_devs) {
        if (dev->pci_io_write(offset, value, size)) {
            return true;
        }
    }
    return false;
}

uint32_t PCIHost::pci_io_read_broadcast(uint32_t offset, int size)
{
    uint32_t res;

    // broadcast I/O request to devices that support I/O space
    // until a device returns true that means "request accepted"
    if (pci_io_read_loop(offset, size, res))
        return res;

    // no device has accepted the request -> report error
    LOG_F(
        ERROR, "%s: Attempt to read from unmapped PCI I/O space @%08x.%c",
        this->get_name().c_str(), offset,
        SIZE_ARG(size)
    );
    // machine check exception (DEFAULT CATCH!, code=FFF00200)
    ppc_exception_handler(Except_Type::EXC_MACHINE_CHECK, 0);
    return 0;
}

void PCIHost::pci_io_write_broadcast(uint32_t offset, int size, uint32_t value)
{
    // broadcast I/O request to devices that support I/O space
    // until a device returns true that means "request accepted"
    if (pci_io_write_loop(offset, size, value))
        return;

    // no device has accepted the request -> report error
    LOG_F(
        ERROR, "%s: Attempt to write to unmapped PCI I/O space @%08x.%c = %0*x",
        this->get_name().c_str(), offset,
        SIZE_ARG(size),
        size * 2, BYTESWAP_SIZED(value, size)
    );
}

PCIBase *PCIHost::pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num)
{
    for (auto& bridge : this->bridge_devs) {
        if (bridge->secondary_bus <= bus_num) {
            if (bridge->secondary_bus == bus_num) {
                return bridge->pci_find_device(dev_num, fun_num);
            }
            if (bridge->subordinate_bus >= bus_num) {
                return bridge->pci_find_device(bus_num, dev_num, fun_num);
            }
        }
    }
    return NULL;
}

PCIBase *PCIHost::pci_find_device(uint8_t dev_num, uint8_t fun_num)
{
    if (this->dev_map.count(DEV_FUN(dev_num, fun_num))) {
        return this->dev_map[DEV_FUN(dev_num, fun_num)];
    }
    return NULL;
}

PostInitResultType PCIHost::pcihost_device_postinit()
{
    std::string pci_dev_name;

    for (auto& slot : this->my_irq_map) {
        if (slot.second.slot_name) {
            if (gMachineSettings.count(slot.second.slot_name)) {
                auto &s = gMachineSettings[slot.second.slot_name];
                if (s->value_commandline == s->value_not_inited) {
                    /*
                        Only add devices that are not specified by command line. PCI devices
                        specified by command line are handled by set_property below so that
                        they can participate in the config stack explicitly.
                    */
                    pci_dev_name = dynamic_cast<StrProperty*>(s->property.get())->get_string();
                    if (!pci_dev_name.empty()) {
                        this->attach_pci_device(pci_dev_name, slot.first);
                    }
                }
            }
        }
    }

    return PI_SUCCESS;
}

std::string PCIHost::get_child_unit_address_string(int32_t unit_address) {
    return PCIBase::get_unit_address_string(unit_address);
}

int32_t PCIHost::parse_child_unit_address_string(const std::string unit_address_string, HWComponent*& hwc) {
    return PCIBase::parse_unit_address_string(unit_address_string);
}

HWComponent* PCIHost::set_property(const std::string &property, const std::string &value, int32_t unit_address) {
    int dev_fun;

    if (unit_address == -1) {
        for (auto& slot : this->my_irq_map) {
            dev_fun = slot.first;
            if (slot.second.slot_name && !this->dev_map.count(dev_fun)) {
                if (property == slot.second.slot_name) {
                    if (value.empty()) {
                        if (this->dev_map.count(dev_fun))
                            this->remove_device(dev_fun);
                        return this;
                    } else {
                        PCIBase *dev_instance = this->attach_pci_device(value, dev_fun);
                        return dev_instance;
                    }
                }
            }
        }
    }

    if (property == "pci") {
        int max_dev = GET_INT_PROP("pci_dev_max");

        if (unit_address == -1) {
            // look for unused ID
            for (dev_fun = 0; dev_fun <= DEV_FUN(0x1F,7); dev_fun += DEV_FUN(1,0)) {
                if (
                    !this->dev_map.count(dev_fun) && ( // is not used
                        (this->my_irq_map.size() > 0 && this->my_irq_map.count(dev_fun)) || // is a valid slot
                        (this->my_irq_map.size() == 0 && dev_fun <= DEV_FUN(max_dev,7)) // or is a valid device number
                    )
                )
                    break;
            }
            if (dev_fun > DEV_FUN(0x1F,7))
                return nullptr;
        }
        else {
            if (unit_address < 0 || unit_address > DEV_FUN(0x1F,7))
                return nullptr;
            dev_fun = unit_address;
            if (this->dev_map.count(dev_fun))
                return nullptr;
        }

        PCIBase *dev_instance = this->attach_pci_device(value, dev_fun);
        return dev_instance;
    }
    return nullptr;
}
