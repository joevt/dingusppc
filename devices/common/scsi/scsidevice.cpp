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

#include <core/timermanager.h>
#include <devices/common/scsi/scsi.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <regex>

namespace loguru {
    enum : Verbosity {
        Verbosity_SCSIDEVICE = loguru::Verbosity_9
    };
}

void ScsiDevice::notify(ScsiNotification notif_type, int param)
{
    if (notif_type == ScsiNotification::BUS_PHASE_CHANGE) {
        switch (param) {
        case ScsiPhase::RESET:
            LOG_F(SCSIDEVICE, "%s: bus reset aknowledged", this->get_name().c_str());
            break;
        case ScsiPhase::SELECTION:
            LOG_F(SCSIDEVICE, "%s: checking selected for %d", this->get_name().c_str(), scsi_id);
            // check if something tries to select us
            if (this->bus_obj->get_data_lines() & (1 << scsi_id)) {
                LOG_F(SCSIDEVICE, "%s selected", this->get_name().c_str());
                TimerManager::get_instance()->add_oneshot_timer(
                    BUS_SETTLE_DELAY,
                    [this]() {
                        // don't confirm selection if BSY or I/O are asserted
                        if (this->bus_obj->test_ctrl_lines(SCSI_CTRL_BSY | SCSI_CTRL_IO))
                            return;
                        LOG_F(SCSIDEVICE, "%s: assert SCSI_CTRL_BSY", this->get_name().c_str());
                        this->bus_obj->assert_ctrl_line(this->scsi_id, SCSI_CTRL_BSY);
                        this->bus_obj->confirm_selection(this->scsi_id);
                        this->seq_steps = nullptr;
                        this->initiator_id = this->bus_obj->get_initiator_id();
                        if (this->bus_obj->test_ctrl_lines(SCSI_CTRL_ATN)) {
                            this->last_selection_has_atention = true;
                            this->switch_phase(ScsiPhase::MESSAGE_OUT);
                        } else {
                            this->last_selection_has_atention = false;
                            this->switch_phase(ScsiPhase::COMMAND);
                        }
                });
            }
            break;
        default:
            LOG_F(SCSIDEVICE, "%s: ScsiMsg::BUS_PHASE_CHANGE unhandled phase %d", this->get_name().c_str(), param);
        }
    }
    else {
        LOG_F(SCSIDEVICE, "%s: unhandled notification type %d", this->get_name().c_str(), notif_type);
    }
}

void ScsiDevice::switch_phase(const int new_phase)
{
    this->cur_phase = new_phase;
    this->bus_obj->switch_phase(this->scsi_id, this->cur_phase);
}

bool ScsiDevice::allow_phase_change() {
    if (this->bus_obj->test_ctrl_lines(SCSI_CTRL_ATN | SCSI_CTRL_ACK) ==
                                      (SCSI_CTRL_ATN | SCSI_CTRL_ACK))
        ABORT_F("%s: reject message requested", this->name.c_str());

    if (this->data_size || this->bus_obj->test_ctrl_lines(SCSI_CTRL_ACK))
        return false;
    else
        return true;
}

void ScsiDevice::next_step()
{
    // special case: data transfers during MESSAGE_IN phase
    // require handshaking. Rejecting needs to be detected too.
    if (bus_obj->current_phase() == ScsiPhase::MESSAGE_IN &&
        !this->allow_phase_change())
        return;

    // check for pluggable sequences and follow them if applicable
    if (this->seq_steps && *this->seq_steps >= 0) {
        if (this->cur_phase == *this->seq_steps) {
            this->seq_steps++;
            if (*this->seq_steps >= 0) {
                this->switch_phase(*this->seq_steps);
                return;
            }
        }
    }

    switch (this->cur_phase) {
    case ScsiPhase::DATA_OUT:
        LOG_F(SCSIDEVICE, "%s: DATA_OUT data_size:%d incoming_size:%d in %s",
            this->get_name().c_str(), this->data_size, this->incoming_size, __func__);
        if (this->data_size >= this->incoming_size) {
            if (this->post_xfer_action != nullptr) {
                this->post_xfer_action();
            }
            this->switch_phase(ScsiPhase::STATUS);
        }
        break;
    case ScsiPhase::DATA_IN:
        if (!this->has_data()) {
            this->switch_phase(ScsiPhase::STATUS);
        }
        break;
    case ScsiPhase::COMMAND:
        this->process_command();
        if (this->cur_phase != ScsiPhase::COMMAND) {
            if (this->prepare_data()) {
                this->bus_obj->assert_ctrl_line(this->scsi_id, SCSI_CTRL_REQ);
            } else {
                ABORT_F("ScsiDevice: prepare_data() failed");
            }
        }
        break;
    case ScsiPhase::STATUS:
        this->bus_obj->release_ctrl_line(this->scsi_id, SCSI_CTRL_REQ);
        this->data_ptr  = this->msg_buf;
        this->data_size = 1;
        this->switch_phase(ScsiPhase::MESSAGE_IN);
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->switch_phase(ScsiPhase::COMMAND);
        break;
    case ScsiPhase::MESSAGE_IN:
    case ScsiPhase::BUS_FREE:
        LOG_F(SCSIDEVICE, "%s: release all", this->get_name().c_str());
        this->bus_obj->release_ctrl_lines(this->scsi_id);
        this->seq_steps = nullptr;
        this->switch_phase(ScsiPhase::BUS_FREE);
        break;
    default:
        LOG_F(WARNING, "%s: nothing to do for phase %d", this->get_name().c_str(), this->cur_phase);
    }
}

void ScsiDevice::prepare_xfer(ScsiBus* bus_obj, int& bytes_in, int& bytes_out)
{
    this->cur_phase = bus_obj->current_phase();

    switch (this->cur_phase) {
    case ScsiPhase::COMMAND:
        this->data_ptr = this->cmd_buf;
        this->data_size = 0;
        bytes_out = 0;
        break;
    case ScsiPhase::STATUS:
        this->data_ptr = &this->status;
        this->data_size = 1;
        bytes_out = 1;
        break;
    case ScsiPhase::DATA_IN:
        bytes_out = this->data_size;
        break;
    case ScsiPhase::DATA_OUT:
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->data_ptr = this->msg_buf;
        this->data_size = bytes_in;
        bytes_out = 0;
        break;
    case ScsiPhase::MESSAGE_IN:
        break;
    default:
        ABORT_F("%s: unhandled phase %d in prepare_xfer()", this->get_name().c_str(), this->cur_phase);
    }
}

static const int CmdGroupLen[8] = {6, 10, 10, -1, -1, 12, -1, -1};

int ScsiDevice::xfer_data() {
    this->cur_phase = bus_obj->current_phase();

    switch (this->cur_phase) {
    case ScsiPhase::MESSAGE_OUT:
        if (this->bus_obj->pull_data(this->initiator_id, this->msg_buf, 1)) {
            if (this->msg_buf[0] & ScsiMessage::IDENTIFY) {
                LOG_F(9, "%s: IDENTIFY MESSAGE received, code = 0x%X",
                      this->name.c_str(), this->msg_buf[0]);
            } else {
                this->process_message();
            }
            if (this->last_selection_has_atention) {
                this->last_selection_message = this->msg_buf[0];
                LOG_F(SCSIDEVICE, "%s: received message:0x%02x", this->get_name().c_str(), this->msg_buf[0]);
            }
        }
        break;
    case ScsiPhase::COMMAND:
        if (this->bus_obj->pull_data(this->initiator_id, this->cmd_buf, 1)) {
            int cmd_len = CmdGroupLen[this->cmd_buf[0] >> 5];
            if (cmd_len < 0) {
                ABORT_F("%s: unsupported command received, code = 0x%X",
                        this->name.c_str(), this->msg_buf[0]);
            }
            if (this->bus_obj->pull_data(this->initiator_id, &this->cmd_buf[1], cmd_len - 1))
                this->next_step();
        }
        break;
    default:
        ABORT_F("ScsiDevice: unhandled phase %d in xfer_data()", this->cur_phase);
    }

    return 0;
}

int ScsiDevice::send_data(uint8_t* dst_ptr, const int count)
{
    if (dst_ptr == nullptr || !count) {
        return 0;
    }

    int actual_count = std::min(this->data_size, count);

    std::memcpy(dst_ptr, this->data_ptr, actual_count);
    LOG_F(SCSIDEVICE, "%s: send_data data_size:%d -> %d", this->name.c_str(), this->data_size, this->data_size - count);
    this->data_ptr  += actual_count;
    this->data_size -= actual_count;

    // attempt to return the requested amount of data
    // when data_size drops down to zero
    if (!this->data_size) {
        if (this->get_more_data() && count > actual_count) {
            dst_ptr += actual_count;
            actual_count = std::min(this->data_size, count - actual_count);
            std::memcpy(dst_ptr, this->data_ptr, actual_count);
            this->data_ptr  += actual_count;
            this->data_size -= actual_count;
        }
    }

    return actual_count;
}

int ScsiDevice::rcv_data(const uint8_t* src_ptr, const int count)
{
    // accumulating incoming data in the pre-configured buffer
    std::memcpy(this->data_ptr, src_ptr, count);
    LOG_F(SCSIDEVICE, "%s: rcv_data data_size:%d -> %d", this->name.c_str(), this->data_size, this->data_size + count);
    this->data_ptr  += count;
    this->data_size += count;

    if (this->cur_phase == ScsiPhase::COMMAND)
        this->next_step();

    return count;
}

bool ScsiDevice::check_lun() {
    if (this->cmd_buf[1] >> 5 != this->lun) {
        LOG_F(ERROR, "%s: non-matching LUN", this->name.c_str());
        this->status = ScsiStatus::CHECK_CONDITION;
        this->sense  = ScsiSense::ILLEGAL_REQ;
        this->asc    = ScsiError::INVALID_LUN;
        this->ascq   = 0;
        this->sksv   = 0;
        this->field  = 0;
        this->switch_phase(ScsiPhase::STATUS);
        return false;
    }
    return true;
}

void ScsiDevice::illegal_command(const uint8_t *cmd) {
    LOG_F(ERROR, "%s: unsupported command: 0x%02x", this->name.c_str(), cmd[0]);
    this->status = ScsiStatus::CHECK_CONDITION;
    this->sense  = ScsiSense::ILLEGAL_REQ;
    this->asc    = ScsiError::INVALID_CMD;
    this->ascq   = 0;
    this->sksv   = 0xC0; // sksv=1, C/D=Command, BPV=0, BP=0
    this->field  = 0;
    this->switch_phase(ScsiPhase::STATUS);
}

void ScsiDevice::report_error(uint8_t sense_key, uint8_t asc) {
    this->status = ScsiStatus::CHECK_CONDITION;
    this->sense  = sense_key;
    this->asc    = asc;
    this->ascq   = 0;
    this->sksv   = 0xC0; // sksv=1, C/D=Command, BPV=0, BP=0
    this->switch_phase(ScsiPhase::STATUS);
}

void ScsiDevice::process_message() {
    static int sdtr_response_seq[4] = {ScsiPhase::MESSAGE_OUT, ScsiPhase::MESSAGE_IN,
                                       ScsiPhase::COMMAND, -1};

    if (this->msg_buf[0] == 1) { // extended messages
        if (!this->bus_obj->pull_data(this->initiator_id, &this->msg_buf[1], 1) ||
            !this->bus_obj->pull_data(this->initiator_id, &this->msg_buf[2], this->msg_buf[1]))
            ABORT_F("%s: incomplete message received", this->name.c_str());

        switch(this->msg_buf[2]) {
        case ScsiExtMessage::SYNCH_XFER_REQ:
            LOG_F(INFO, "%s: SDTR message received", this->name.c_str());
            // confirm synchronous transfer capability by sending back the SDTR message
            this->seq_steps = sdtr_response_seq;
            this->data_ptr = this->msg_buf;
            this->data_size = 5;
            break;
        default:
            LOG_F(ERROR, "%s: unsupported message %d", this->name.c_str(), this->msg_buf[2]);
        }
    } else if ((this->msg_buf[0] >> 4) == 2) { // two-byte messages
        if (!this->bus_obj->pull_data(this->initiator_id, &this->msg_buf[1], 1))
            ABORT_F("%s: incomplete message received", this->name.c_str());
    }
}

int32_t ScsiDevice::parse_self_unit_address_string(const std::string unit_address_string) {
    return ScsiDevice::parse_unit_address_string(unit_address_string);
}

int32_t ScsiDevice::parse_unit_address_string(const std::string unit_address_string) {
    std::regex unit_address_re("0*(1?[0-9])", std::regex_constants::icase);
    std::smatch results;
    if (std::regex_match(unit_address_string, results, unit_address_re)) {
        int result = (int32_t)std::stol(results[1], nullptr, 10);
        if (result >= SCSI_MAX_DEVS)
            return -1;
        return result;
    } else {
        return -1;
    }
}
