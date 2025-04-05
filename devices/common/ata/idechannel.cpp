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

/** IDE Channel (aka IDE port) emulation.

    One IDE channel is capable of controlling up to two IDE devices.

    IdeChannel class handles device registration and passing of messages
    from and to the host.

    MacioIdeChannel class implements MacIO specific registers
    and interrupt handling.
 */

#include <core/timermanager.h>
#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/common/ata/atahd.h>
#include <devices/common/ata/atapicdrom.h>
#include <devices/common/ata/idechannel.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/deviceregistry.h>
#include <machines/machinefactory.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>
#include <string>

using namespace ata_interface;

namespace loguru {
    enum : Verbosity {
        Verbosity_IDE_CHANNEL = loguru::Verbosity_9
    };
}

IdeChannel::IdeChannel(const std::string name) : HWComponent(name)
{
    this->supports_types(HWCompType::IDE_BUS);

    this->device_stub = std::unique_ptr<AtaNullDevice>(new AtaNullDevice());

    this->devices[0] = this->device_stub.get();
    this->devices[1] = this->device_stub.get();
}

template <class T>
HWComponent* IdeChannel::set_property(const std::string &value, int32_t unit_address) {
    int ata_id;
    T *ata_device;
    HWComponent* result;
    
    if (unit_address == -1) {
        // look for existing device with no image
        for (ata_id = 0; ata_id < 2; ata_id++) {
            ata_device = dynamic_cast<T *>(this->devices[ata_id]);
            if (ata_device) {
                result = ata_device->set_property(
                    std::is_same<T,AtaHardDisk>::value ? "hdd_img" : "cdr_img", value, unit_address);
                if (result)
                    return result;
            }
        }

        // look for unused ID
        for (ata_id = 0; ata_id < 2 && this->devices[ata_id] &&
             dynamic_cast<AtaNullDevice*>(this->devices[ata_id]) == nullptr; ata_id++) {}
        if (ata_id == 2)
            return nullptr;
    }
    else {
        if (unit_address < 0 || unit_address >= 2)
            return nullptr;
        ata_id = unit_address;
    }

    if (this->devices[ata_id] && dynamic_cast<AtaNullDevice*>(this->devices[ata_id]) == nullptr)
        ata_device = dynamic_cast<T *>(this->devices[ata_id]);
    else
        ata_device = dynamic_cast<T *>(MachineFactory::create_device(
            this, (std::is_same<T,AtaHardDisk>::value ? "AtaHardDisk@" : "AtapiCdrom@") + std::to_string(ata_id)));
    if (ata_device)
        return ata_device->set_property(std::is_same<T,AtaHardDisk>::value ? "cdr_img" : "hdd_img", value, unit_address);
    return nullptr;
}

HWComponent* IdeChannel::set_property(const std::string &property, const std::string &value, int32_t unit_address)
{
    if (property == "hdd_img")
        return this->set_property<AtaHardDisk>(value, unit_address);
    if (property == "cdr_img")
        return this->set_property<AtapiCdrom>(value, unit_address);
    return nullptr;
}

HWComponent* IdeChannel::add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name) {
    this->register_device(unit_address, dynamic_cast<AtaInterface*>(dev_obj));
    return HWComponent::add_device(unit_address, dev_obj, name);
}

int32_t IdeChannel::parse_child_unit_address_string(const std::string unit_address_string, HWComponent*& hwc) {
    return AtaBaseDevice::parse_unit_address_string(unit_address_string);
}

void IdeChannel::register_device(int id, AtaInterface* dev_obj) {
    if (id < 0 || id >= 2)
        ABORT_F("%s: invalid device ID", this->get_name_and_unit_address().c_str());

    this->devices[id] = dev_obj;

    ((AtaBaseDevice*)dev_obj)->set_host(this, id);
}

uint32_t IdeChannel::read(const uint8_t reg_addr, const int size) {
    uint32_t value = this->devices[this->cur_dev]->read(reg_addr);
    LOG_F(IDE_CHANNEL, "%s: read  @%02x.%c = %0*x", this->get_name_and_unit_address().c_str(),
        reg_addr, SIZE_ARG(size), size * 2, value);
    return value;
}

void IdeChannel::write(const uint8_t reg_addr, const uint32_t val, const int size)
{
    // keep track of the currently selected device
    if (reg_addr == DEVICE_HEAD) {
        this->cur_dev = (val >> 4) & 1;
        AtaBaseDevice* base = dynamic_cast<AtaBaseDevice*>(this->devices[this->cur_dev]);
        LOG_F(IDE_CHANNEL, "%s: cur_dev = %d (%s)", this->get_name_and_unit_address().c_str(),
            this->cur_dev, base ? base->get_name_and_unit_address().c_str() : "AtaNullDevice");
    }

    // redirect register writes to both devices
    for (auto& dev : this->devices) {
        LOG_F(IDE_CHANNEL, "%s: write @%02x.%c = %0*x", this->get_name_and_unit_address().c_str(),
            reg_addr, SIZE_ARG(size), size * 2, val);
        dev->write(reg_addr, val);
    }
}

int IdeChannel::xfer_from(uint8_t *buf, int len) {
    return this->devices[this->cur_dev]->pull_data(buf, len);
}

int IdeChannel::xfer_to(uint8_t *buf, int len) {
    return this->devices[this->cur_dev]->push_data(buf, len);
}

void IdeChannel::assert_dmareq(uint64_t delay) {
    TimerManager::get_instance()->add_oneshot_timer(delay, [this]() {
        //LOG_F(INFO, "%s: DMAREQ asserted", this->name.c_str());
        this->channel_obj->xfer_retry();
    });
}

PostInitResultType MacioIdeChannel::device_postinit() {
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(
        this->name == "Ide0" ? IntSrc::IDE0 : IntSrc::IDE1);

    this->irq_callback = [this](const uint8_t intrq_state) {
        this->int_ctrl->ack_int(this->irq_id, intrq_state);
    };

    return PI_SUCCESS;
}

uint32_t MacioIdeChannel::read(const uint8_t reg_addr, const int size)
{
    if (reg_addr == TIME_CONFIG) {
        if (size != 4) {
            LOG_F(WARNING, "%s: non-DWORD read from TIME_CONFIG", this->get_name_and_unit_address().c_str());
        }
        return this->ch_config;
    } else
        return IdeChannel::read(reg_addr, size);
}

void MacioIdeChannel::write(const uint8_t reg_addr, const uint32_t val, const int size)
{
    if (reg_addr == TIME_CONFIG) {
        if (size != 4) {
            LOG_F(WARNING, "%s: non-DWORD write to TIME_CONFIG", this->get_name_and_unit_address().c_str());
        }
        this->ch_config = val;
    } else
        IdeChannel::write(reg_addr, val, size);
}

static const DeviceDescription Ide_Descriptor = {
    MacioIdeChannel::create, {}, {}, HWCompType::IDE_BUS
};

REGISTER_DEVICE(Ide0, Ide_Descriptor);
REGISTER_DEVICE(Ide1, Ide_Descriptor);
