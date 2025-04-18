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

/** @file SCSI bus emulation. */

#include <devices/common/hwcomponent.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/deviceregistry.h>
#include <machines/machinefactory.h>
#include <loguru.hpp>

#include <cinttypes>
#include <sstream>

namespace loguru {
    enum : Verbosity {
        Verbosity_SCSIBUS = loguru::Verbosity_9
    };
}

ScsiBus::ScsiBus(const std::string name) : HWComponent(name)
{
    supports_types(HWCompType::SCSI_BUS);

    for(int i = 0; i < SCSI_MAX_DEVS; i++) {
        this->devices[i]        = nullptr;
        this->dev_ctrl_lines[i] = 0;
    }

    this->ctrl_lines    =  0; // all control lines released
    this->data_lines    =  0; // data lines released
    this->arb_winner_id = -1;
    this->initiator_id  = -1;
    this->target_id     = -1;
}

template <class T>
HWComponent* ScsiBus::set_property(const std::string &value, int32_t unit_address) {
    int scsi_id;
    T *scsi_device;
    HWComponent* result;
    
    if (unit_address == -1) {
        // look for existing device with no image
        for (scsi_id = 0; scsi_id < SCSI_MAX_DEVS; scsi_id++) {
            scsi_device = dynamic_cast<T *>(this->devices[scsi_id]);
            if (scsi_device) {
                result = scsi_device->set_property(
                    std::is_same<T,ScsiHardDisk>::value ? "hdd_img" : "cdr_img", value, unit_address);
                if (result)
                    return result;
            }
        }

        // look for unused ID
        // do two passes because we either skip ID 3 (for hard disks) or start at ID 3 (for CD ROMs)
        for (scsi_id = (std::is_same<T,ScsiHardDisk>::value ? 0 : 3);
            scsi_id < SCSI_MAX_DEVS * 2 && ((std::is_same<T,ScsiHardDisk>::value && scsi_id == 3) ||
                this->devices[scsi_id % SCSI_MAX_DEVS]);
            scsi_id++
        ) {}
        if (scsi_id == SCSI_MAX_DEVS * 2)
            return nullptr;
        scsi_id = scsi_id % SCSI_MAX_DEVS;
    }
    else {
        if (unit_address < 0 || unit_address >= SCSI_MAX_DEVS)
            return nullptr;
        scsi_id = unit_address;
    }

    if (this->devices[scsi_id])
        scsi_device = dynamic_cast<T *>(this->devices[scsi_id]);
    else
        scsi_device = dynamic_cast<T *>(MachineFactory::create_device(
            this, (std::is_same<T,ScsiHardDisk>::value ? "ScsiHardDisk@" : "ScsiCdrom@") + std::to_string(scsi_id)));
    if (scsi_device)
        return scsi_device->set_property(std::is_same<T,ScsiHardDisk>::value ? "hdd_img" : "cdr_img", value, unit_address);
    return nullptr;
}

HWComponent* ScsiBus::set_property(const std::string &property, const std::string &value, int32_t unit_address)
{
    if (property == "hdd_img")
        return this->set_property<ScsiHardDisk>(value, unit_address);
    if (property == "cdr_img")
        return this->set_property<ScsiCdrom>(value, unit_address);
    return nullptr;
}

HWComponent* ScsiBus::add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name)
{
    this->register_device(unit_address, dynamic_cast<ScsiDevice*>(dev_obj));
    return HWComponent::add_device(unit_address, dev_obj, name);
}

int32_t ScsiBus::parse_child_unit_address_string(const std::string unit_address_string, HWComponent*& hwc) {
    return ScsiDevice::parse_unit_address_string(unit_address_string);
}

void ScsiBus::register_device(int id, ScsiDevice* dev_obj)
{
    if (this->devices[id] != nullptr) {
        ABORT_F("%s: device with ID %d already registered", this->get_name().c_str(), id);
    }
    LOG_F(INFO, "%s: Added SCSI device %s ID:%d", this->get_name().c_str(), dev_obj->get_name().c_str(), id);

    this->devices[id] = dev_obj;

    dev_obj->set_bus_object_ptr(this, id);
}

const char *get_name_bus_phase(int phase) {
    switch (phase) {
    case ScsiPhase::BUS_FREE       : return "BUS_FREE";
    case ScsiPhase::ARBITRATION    : return "ARBITRATION";
    case ScsiPhase::SELECTION      : return "SELECTION";
    case ScsiPhase::RESELECTION    : return "RESELECTION";
    case ScsiPhase::COMMAND        : return "COMMAND";
    case ScsiPhase::DATA_IN        : return "DATA_IN";
    case ScsiPhase::DATA_OUT       : return "DATA_OUT";
    case ScsiPhase::STATUS         : return "STATUS";
    case ScsiPhase::MESSAGE_IN     : return "MESSAGE_IN";
    case ScsiPhase::MESSAGE_OUT    : return "MESSAGE_OUT";
    case ScsiPhase::RESET          : return "RESET";
    default                        : return "unknown";
    }
}

void ScsiBus::change_bus_phase(int initiator_id)
{
    LOG_F(SCSIBUS, "%s: initiator:%d changing bus phase to %s", name.c_str(), initiator_id, get_name_bus_phase(this->cur_phase));
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        if (i == initiator_id)
            continue; // don't notify the initiator
        if (this->devices[i] != nullptr) {
            this->devices[i]->notify(ScsiNotification::BUS_PHASE_CHANGE, this->cur_phase);
        }
    }
}

void ScsiBus::assert_ctrl_line(int initiator_id, uint16_t mask)
{
    if (!(initiator_id >= 0 && initiator_id < SCSI_MAX_DEVS)) {
        LOG_F(ERROR, "%s: invalid initiator ID %d", this->get_name().c_str(), initiator_id);
        return;
    }

    uint16_t new_state = 0xFFFFU & mask;

    this->dev_ctrl_lines[initiator_id] |= new_state;

    if (new_state == this->ctrl_lines) {
        return;
    }

    if (new_state & SCSI_CTRL_RST) {
        this->ctrl_lines |= SCSI_CTRL_RST;
        this->cur_phase = ScsiPhase::RESET;
        change_bus_phase(initiator_id);
    }
}

void ScsiBus::release_ctrl_line(int id, uint16_t mask)
{
    if (!(id >= 0 && id < SCSI_MAX_DEVS)) {
        LOG_F(ERROR, "%s: invalid initiator ID %d", this->get_name().c_str(), id);
    }

    uint16_t new_state = 0;

    this->dev_ctrl_lines[id] &= ~mask;

    // OR control lines of all devices together
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        new_state |= this->dev_ctrl_lines[i];
    }

    if (this->ctrl_lines & SCSI_CTRL_RST) {
        if (!(new_state & SCSI_CTRL_RST)) {
            this->ctrl_lines = new_state;
            this->cur_phase = ScsiPhase::BUS_FREE;
            change_bus_phase(id);
        }
    } else {
        this->ctrl_lines = new_state;
    }
}

void ScsiBus::release_ctrl_lines(int id)
{
    this->release_ctrl_line(id, 0xFFFFUL);
}

uint16_t ScsiBus::test_ctrl_lines(uint16_t mask)
{
    uint16_t new_state = 0;

    // OR control lines of all devices together
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        new_state |= this->dev_ctrl_lines[i];
    }

    return new_state & mask;
}

int ScsiBus::switch_phase(int id, int new_phase)
{
    int old_phase = this->cur_phase;

    LOG_F(SCSIBUS, "%s: changing bus phase from %s to %s",
        name.c_str(), get_name_bus_phase(old_phase), get_name_bus_phase(new_phase));

    // leave the current phase (low-level)
    switch (old_phase) {
    case ScsiPhase::COMMAND:
        this->release_ctrl_line(id, SCSI_CTRL_CD);
        break;
    case ScsiPhase::DATA_IN:
        this->release_ctrl_line(id, SCSI_CTRL_IO);
        break;
    case ScsiPhase::STATUS:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_IO);
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG);
        break;
    case ScsiPhase::MESSAGE_IN:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG | SCSI_CTRL_IO);
        break;
    }

    // enter new phase (low-level)
    switch (new_phase) {
    case ScsiPhase::COMMAND:
        this->assert_ctrl_line(id, SCSI_CTRL_CD);
        break;
    case ScsiPhase::DATA_IN:
        this->assert_ctrl_line(id, SCSI_CTRL_IO);
        break;
    case ScsiPhase::STATUS:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_IO);
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG);
        break;
    case ScsiPhase::MESSAGE_IN:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG | SCSI_CTRL_IO);
        break;
    }

    // switch the bus to the new phase (high-level)
    this->cur_phase = new_phase;
    change_bus_phase(id);

    return old_phase;
}

bool ScsiBus::begin_arbitration(int initiator_id)
{
    if (this->cur_phase == ScsiPhase::BUS_FREE) {
        this->data_lines |= 1 << initiator_id;
        this->cur_phase = ScsiPhase::ARBITRATION;
        change_bus_phase(initiator_id);
        return true;
    } else {
        return false;
    }
}

bool ScsiBus::end_arbitration(int initiator_id)
{
    int highest_id = -1;

    // find the highest ID bit on the data lines
    for (int id = 7; id >= 0; id--) {
        if (this->data_lines & (1 << id)) {
            highest_id = id;
            break;
        }
    }

    if (highest_id >= 0) {
        this->arb_winner_id = highest_id;
    }

    return highest_id == initiator_id;
}

bool ScsiBus::begin_selection(int initiator_id, int target_id, bool atn)
{
    // perform bus integrity checks
    if (this->cur_phase != ScsiPhase::ARBITRATION || this->arb_winner_id != initiator_id)
        return false;

    LOG_F(SCSIBUS, "%s: assert SCSI_CTRL_SEL in %s", this->get_name().c_str(), __func__);
    this->assert_ctrl_line(initiator_id, SCSI_CTRL_SEL);

    this->data_lines = (1 << initiator_id) | (1 << target_id);

    if (atn) {
        LOG_F(SCSIBUS, "%s: assert SCSI_CTRL_ATN", this->get_name().c_str());
        assert_ctrl_line(initiator_id, SCSI_CTRL_ATN);
    }

    this->initiator_id = initiator_id;
    this->cur_phase = ScsiPhase::SELECTION;
    change_bus_phase(initiator_id);
    return true;
}

void ScsiBus::confirm_selection(int target_id)
{
    LOG_F(SCSIBUS, "%s: confirm_selection %d", this->get_name().c_str(), target_id);
    this->target_id = target_id;

    // notify initiator about selection confirmation from target
    if (this->initiator_id >= 0) {
        this->devices[this->initiator_id]->notify(ScsiNotification::CONFIRM_SEL, target_id);
    }
}

bool ScsiBus::end_selection(int /*initiator_id*/, int target_id)
{
    // check for selection confirmation from target
    return this->target_id == target_id;
}

bool ScsiBus::pull_data(const int id, uint8_t* dst_ptr, const int size)
{
    if (dst_ptr == nullptr || !size) {
        return false;
    }

    if (!this->devices[id]->send_data(dst_ptr, size)) {
        LOG_F(ERROR, "%s: error while transferring T->I data!", this->get_name().c_str());
        return false;
    }

    return true;
}

bool ScsiBus::push_data(const int id, const uint8_t* src_ptr, const int size)
{
    if (!this->devices[id]) {
        LOG_F(ERROR, "%s: no device %d for push_data %d bytes",
              this->get_name().c_str(), id, size);
        return false;
    }

    if (!this->devices[id]->rcv_data(src_ptr, size)) {
        if (size) {
            LOG_F(ERROR, "%s: error while transferring I->T data!", this->get_name().c_str());
            return false;
        }
    }

    return true;
}

int ScsiBus::target_xfer_data() {
    return this->devices[this->target_id]->xfer_data();
}

void ScsiBus::target_next_step()
{
    if (target_id < 0) {
        LOG_F(ERROR, "%s: target_id is not set yet.", this->get_name().c_str());
    }
    else {
        this->devices[this->target_id]->next_step();
    }
}

bool ScsiBus::negotiate_xfer(int& bytes_in, int& bytes_out)
{
    if (target_id < 0) {
        LOG_F(ERROR, "%s: target_id is not set yet.", this->get_name().c_str());
    }
    else {
        this->devices[this->target_id]->prepare_xfer(this, bytes_in, bytes_out);
    }

    return true;
}

void ScsiBus::disconnect(int dev_id)
{
    LOG_F(SCSIBUS, "%s: release all", this->get_name().c_str());
    this->release_ctrl_lines(dev_id);
    if (!(this->ctrl_lines & SCSI_CTRL_BSY) && !(this->ctrl_lines & SCSI_CTRL_SEL)) {
        this->cur_phase = ScsiPhase::BUS_FREE;
        change_bus_phase(dev_id);
    }
}

static const DeviceDescription ScsiCurio_Descriptor = {
    ScsiBus::create_ScsiCurio, {"Sc53C94Dev@7"}, {}, HWCompType::SCSI_BUS
};

static const DeviceDescription ScsiMesh_Descriptor = {
    ScsiBus::create_ScsiMesh, {"MeshDev@7"}, {}, HWCompType::SCSI_BUS
};

REGISTER_DEVICE(ScsiCurio, ScsiCurio_Descriptor);
REGISTER_DEVICE(ScsiMesh,  ScsiMesh_Descriptor);
