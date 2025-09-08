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

/** @file Sander-Wozniak Machine 3 (SWIM3) emulation. */

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/common/dmacore.h>
#include <devices/common/hwinterrupt.h>
#include <devices/floppy/superdrive.h>
#include <devices/floppy/swim3.h>
#include <loguru.hpp>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <string>
#include <vector>

namespace loguru {
    enum : Verbosity {
        Verbosity_SWIM3      = loguru::Verbosity_9,
        Verbosity_SELECTDISK = loguru::Verbosity_9,
    };
}

using namespace Swim3;

static std::string get_reg_name(uint8_t reg_offset)
{
    switch (reg_offset) {
    #define one_reg_name(x) case x: return #x;
    one_reg_name(Data)
    one_reg_name(Timer)
    one_reg_name(Error)
    one_reg_name(Param_Data)
    one_reg_name(Phase)
    one_reg_name(Setup)
    one_reg_name(Status_Mode0)
    one_reg_name(Handshake_Mode1)
    one_reg_name(Interrupt_Flags)
    one_reg_name(Step)
    one_reg_name(Current_Track)
    one_reg_name(Current_Sector)
    one_reg_name(Gap_Format)
    one_reg_name(First_Sector)
    one_reg_name(Sectors_To_Xfer)
    one_reg_name(Interrupt_Mask)
    default: return "unknown";
    }
}

Swim3Ctrl::Swim3Ctrl()
    : HWComponent("Swim3")
{
    supports_types(HWCompType::FLOPPY_CTRL);

    this->reset();

    // Attach virtual Superdrive(s) to the internal drive connector
    int num_drives = GET_INT_PROP("fdd_drives");
    if (num_drives > 0)
        this->drive_1 = dynamic_cast<MacSuperDrive*>(MachineFactory::create_device(this, "MacSuperDrive@0"));
    if (num_drives >= 2)
        this->drive_2 = dynamic_cast<MacSuperDrive*>(MachineFactory::create_device(this, "MacSuperDrive@1"));
}

void Swim3Ctrl::reset()
{
    this->setup_reg  = 0;
    this->selected_drive = nullptr;
    this->mode_reg   = 0;
    this->int_reg    = 0;
    this->int_flags  = 0;
    this->int_mask   = 0;
    this->error      = 0;
    this->step_count = 0;
    this->xfer_cnt   = 0;
    this->first_sec  = 0xFF;

    this->cur_state = SWIM3_IDLE;

    this->cur_track     = 0xFF;
    this->cur_sector    = 0x7F;

    this->timer_val     = 0;
    this->phase_lines   = 0;

    if (this->one_us_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->one_us_timer_id);
        this->one_us_timer_id = 0;
    }
    if (this->step_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->step_timer_id);
        this->step_timer_id = 0;
    }
    if (this->access_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->access_timer_id);
        this->access_timer_id = 0;
    }
}

PostInitResultType Swim3Ctrl::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SWIM3);
    return PI_SUCCESS;
}

void Swim3Ctrl::insert_disk(int drive, const std::string& img_path, int write_flag)
{
    MacSuperDrive *the_drive = nullptr;
    switch (drive) {
    case 1:
        if (this->drive_1)
            the_drive = this->drive_1;
        break;
    case 2:
        if (this->drive_2)
            the_drive = this->drive_2;
        break;
    default:
        LOG_F(ERROR, "SWIM3: %d is not a valid drive number", drive);
        return;
    }

    if (the_drive) {
        the_drive->insert_disk(img_path, write_flag);
    }
    else {
        LOG_F(ERROR, "SWIM3: Drive %d is not connected", drive);
    }
}

uint8_t Swim3Ctrl::read(uint8_t reg_offset)
{
    uint8_t value;

    switch(reg_offset) {
    case Swim3Reg::Timer:
        value = this->calc_timer_val();
        break;
    case Swim3Reg::Error:
        value = this->error;
        this->error = 0;
        break;
    case Swim3Reg::Phase:
        value = this->phase_lines;
        break;
    case Swim3Reg::Setup:
        value = this->setup_reg;
        break;
    case Swim3Reg::Handshake_Mode1:
        if (this->selected_drive) {
            uint8_t status_addr = ((this->mode_reg & SWIM3_HEAD_SELECT) >> 2) | (this->phase_lines & 7);
            uint8_t rddata_val  = this->selected_drive->status(status_addr) & 1;

            // transfer rddata_val to both bit 2 (RDDATA) and bit 3 (SENSE)
            // because those signals seem to be historically wired together
            value = (rddata_val << 2) | (rddata_val << 3);
        }
        else {
            LOG_F(ERROR, "SWIM3: read Handshake_Mode1; no drive selected yet");
            value = 0xC; // report both RdData & Sense high
        }
        break;
    case Swim3Reg::Interrupt_Flags:
        value = this->int_flags;
        this->int_flags = 0; // read from this register clears all flags
        update_irq();
        break;
    case Swim3Reg::Step:
        value = this->step_count;
        break;
    case Swim3Reg::Current_Track:
        LOG_F(SWIM3, "SWIM3: get side:%d track:%d", this->cur_track >> 7, this->cur_track & 0x7F);
        value = this->cur_track;
        break;
    case Swim3Reg::Current_Sector:
        LOG_F(SWIM3, "SWIM3: get valid:%d sector:%d", this->cur_sector >> 7, this->cur_sector & 0x7F);
        value = this->cur_sector;
        break;
    case Swim3Reg::Gap_Format:
        LOG_F(SWIM3, "SWIM3: get format:%d", this->format);
        value = this->format;
        break;
    case Swim3Reg::First_Sector:
        value = this->first_sec;
        break;
    case Swim3Reg::Sectors_To_Xfer:
        value = this->xfer_cnt;
        break;
    case Swim3Reg::Interrupt_Mask:
        value = this->int_mask;
        break;
    default:
        LOG_F(INFO, "SWIM3: reading from 0x%X register", reg_offset);
        value = 0;
    }

    LOG_F(SWIM3, "SWIM3: read  %-15s %x.b = %02x", get_reg_name(reg_offset).c_str(), reg_offset, value);
    return value;
}

void Swim3Ctrl::write(uint8_t reg_offset, uint8_t value)
{
    LOG_F(SWIM3, "SWIM3: write %-15s %x.b = %02x", get_reg_name(reg_offset).c_str(), reg_offset, value);

    switch(reg_offset) {
    case Swim3Reg::Timer:
        this->init_timer(value);
        break;
    case Swim3Reg::Param_Data:
        this->pram = value;
        break;
    case Swim3Reg::Phase:
        this->phase_lines = value & 0xF;
        if (this->phase_lines & 8) { // CA3 aka LSTRB high -> sending a command to the drive
            uint8_t command_addr = ((this->mode_reg & SWIM3_HEAD_SELECT) >> 3) | (this->phase_lines & 3);
            uint8_t val = (value >> 2) & 1;
            if (this->selected_drive) {
                this->selected_drive->command(command_addr, val);
            } else
                LOG_F(ERROR, "SWIM3: command %-17s addr=0x%X, value=%d; no drive selected yet",
                    MacSuperDrive::get_command_name(command_addr).c_str(), command_addr, val);
        } else if (this->phase_lines == 4) {
            // Select_Head_0 or Select_Head_1
            uint8_t status_addr = ((this->mode_reg & SWIM3_HEAD_SELECT) >> 2) | (this->phase_lines & 7);
            if (this->selected_drive)
                this->rd_line = this->selected_drive->status(status_addr) & 1;
            else
                LOG_F(ERROR, "SWIM3: status %-13s 0x%X; no drive selected yet",
                    MacSuperDrive::get_status_name(status_addr).c_str(), status_addr);
        }
        break;
    case Swim3Reg::Setup:
        this->setup_reg = value;
        break;
    case Swim3Reg::Status_Mode0:
        this->mode_change(this->mode_reg & ~value);
        break;
    case Swim3Reg::Handshake_Mode1:
        this->mode_change(this->mode_reg | value);
        break;
    case Swim3Reg::Step:
        this->step_count = value;
        break;
    case Swim3Reg::Gap_Format:
        this->gap_size = value;
        break;
    case Swim3Reg::First_Sector:
        this->first_sec = value;
        break;
    case Swim3Reg::Sectors_To_Xfer:
        this->xfer_cnt = value;
        break;
    case Swim3Reg::Interrupt_Mask:
        this->int_mask = value;
        break;
    default:
        LOG_F(INFO, "SWIM3: writing 0x%X to register 0x%X", value, reg_offset);
    }
}

void Swim3Ctrl::update_irq()
{
    if (this->mode_reg & SWIM3_INT_ENA) {
        uint8_t new_irq = !!(this->int_flags & this->int_mask);
        if (new_irq != this->irq) {
            this->irq = new_irq;
            this->int_ctrl->ack_int(this->irq_id, new_irq);
        }
    }
}

void Swim3Ctrl::do_step()
{
    if (this->mode_reg & SWIM3_GO_STEP && this->step_count) { // are we still stepping?
        // instruct the drive to perform single step in current direction
        if (this->selected_drive)
            this->selected_drive->command(MacSuperDrive::CommandAddr::Do_Step, 0);
        else
            LOG_F(ERROR, "SWIM3: do_step; no drive selected yet");
        if (--this->step_count == 0) {
            if (this->step_timer_id) {
                this->stop_stepping();
            }
            this->int_flags |= INT_STEP_DONE;
            update_irq();
        }
    }
}

void Swim3Ctrl::start_stepping()
{
    if (!this->step_count) {
        LOG_F(WARNING, "SWIM3: step_count is zero while go_step is active!");
        return;
    }

    if (this->mode_reg & SWIM3_GO_STEP || this->step_timer_id) {
        LOG_F(ERROR, "SWIM3: another stepping action is running!");
        return;
    }

    if (this->mode_reg & SWIM3_GO || this->access_timer_id) {
        LOG_F(ERROR, "SWIM3: stepping attempt while disk access is in progress!");
        return;
    }

    if ((((this->mode_reg & SWIM3_HEAD_SELECT) >> 3) | (this->phase_lines & 3))
        != MacSuperDrive::CommandAddr::Do_Step) {
        LOG_F(WARNING, "SWIM3: invalid command address on the phase lines!");
        return;
    }

    this->mode_reg |= SWIM3_GO_STEP;

    // step count > 1 requires periodic task
    if (this->step_count > 1) {
        this->step_timer_id = TimerManager::get_instance()->add_cyclic_timer(
            USECS_TO_NSECS(80),
            [this]() {
                this->do_step();
            }
        );
    }

    // perform the first step immediately
    do_step();
}

void Swim3Ctrl::stop_stepping()
{
    // cancel stepping task
    if (this->step_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->step_timer_id);
        this->step_timer_id = 0;
    }
    this->step_count = 0; // not sure this one is required
}

void Swim3Ctrl::start_disk_access()
{
    if (this->mode_reg & SWIM3_GO || this->access_timer_id) {
        LOG_F(ERROR, "SWIM3: another disk access is running!");
        return;
    }

    if (this->mode_reg & SWIM3_GO_STEP || this->step_timer_id) {
        LOG_F(ERROR, "SWIM3: disk access attempt while stepping is in progress!");
        return;
    }

    if (this->mode_reg & SWIM3_WR_MODE) {
        LOG_F(ERROR, "SWIM3: writing not implemented yet");
        return;
    }

    this->mode_reg |= SWIM3_GO;
    LOG_F(9, "SWIM3: disk access started!");

    this->target_sect = this->first_sec;

    if (!this->selected_drive) {
        LOG_F(ERROR, "SWIM3: start_disk_access; no drive selected yet");
        return;
    }

    this->access_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        this->selected_drive->sync_to_disk(),
        [this]() {
            this->cur_state = SWIM3_ADDR_MARK_SEARCH;
            this->disk_access();
        }
    );
}

void Swim3Ctrl::disk_access()
{
    MacSuperDrive::SectorHdr hdr;
    uint64_t delay;

    if (!this->selected_drive) {
        LOG_F(ERROR, "SWIM3: disk access; no drive selected yet");
        return;
    }

    switch(this->cur_state) {
    case SWIM3_ADDR_MARK_SEARCH:
        hdr = this->selected_drive->current_sector_header();
        // update the corresponding SWIM3 registers
        this->cur_track  = ((hdr.side & 1) << 7) | (hdr.track & 0x7F);
        this->cur_sector = 0x80 /* CRC/checksum valid */ | (hdr.sector & 0x7F);
        this->format = hdr.format;
        LOG_F(SWIM3, "SWIM3: set side:%d track:%d valid:%d sector:%d format:%d",
            this->cur_track >> 7, this->cur_track & 0x7F, this->cur_sector >> 7, this->cur_sector & 0x7F, this->format);
        // generate ID_read interrupt
        this->int_flags |= INT_ID_READ;
        update_irq();
        if ((this->cur_sector & 0x7F) == this->target_sect) {
            // sector matches -> transfer its data
            this->cur_state = SWIM3_DATA_XFER;
            delay = this->selected_drive->sector_data_delay();
        } else {
            // move to next address mark
            this->cur_state = SWIM3_ADDR_MARK_SEARCH;
            delay = this->selected_drive->next_sector_delay();
        }
        break;
    case SWIM3_DATA_XFER:
        // transfer sector data over DMA
        this->dma_ch->push_data(this->selected_drive->get_sector_data_ptr(this->cur_sector & 0x7F), 512);
        if (--this->xfer_cnt == 0) {
            this->stop_disk_access();
            // generate sector_done interrupt
            this->int_flags |= INT_SECT_DONE;
            update_irq();
            return;
        }
        this->cur_state = SWIM3_ADDR_MARK_SEARCH;
        delay = this->selected_drive->next_addr_mark_delay(&this->target_sect);
        break;
    default:
        LOG_F(ERROR, "SWIM3: unknown disk access phase 0x%X", this->cur_state);
        return;
    }

    this->access_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        delay,
        [this]() {
            this->disk_access();
        }
    );
}

void Swim3Ctrl::stop_disk_access()
{
    // cancel disk access timer
    if (this->access_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->access_timer_id);
        this->access_timer_id = 0;
    }
}

void Swim3Ctrl::init_timer(const uint8_t start_val)
{
    if (this->timer_val) {
        LOG_F(WARNING, "SWIM3: attempt to re-arm the timer");
    }
    this->timer_val = start_val;
    if (!this->timer_val) {
        this->one_us_timer_start = 0;
        return;
    }

    this->one_us_timer_start = TimerManager::get_instance()->current_time_ns();

    this->one_us_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        uint32_t(this->timer_val) * NS_PER_USEC,
        [this]() {
            this->timer_val = 0;
            this->int_flags |= INT_TIMER_DONE;
            update_irq();
        }
    );
}

uint8_t Swim3Ctrl::calc_timer_val()
{
    if (!this->timer_val) {
        return 0;
    }

    uint64_t time_now   = TimerManager::get_instance()->current_time_ns();
    uint64_t us_elapsed = (time_now - this->one_us_timer_start) / NS_PER_USEC;
    if (us_elapsed > this->timer_val) {
        return 0;
    } else {
        return (this->timer_val - us_elapsed) & 0xFFU;
    }
}

#define motor_off

void Swim3Ctrl::mode_change(uint8_t new_mode)
{
    uint8_t changed_bits = this->mode_reg ^ new_mode;

    if (changed_bits & (SWIM3_DRIVE_1 | SWIM3_DRIVE_2)) {
        this->selected_drive = nullptr;
        this->cur_track = 0xFF;
        this->cur_sector = 0x7F;

        switch (new_mode & (SWIM3_DRIVE_1 | SWIM3_DRIVE_2)) {
        case 0:
            LOG_F(SELECTDISK, "SWIM3: no drive selected");
#ifdef motor_off
            if (this->drive_1)
                this->drive_1->set_motor_stat(0);
            if (this->drive_2)
                this->drive_2->set_motor_stat(0);
#endif
            break;
        case SWIM3_DRIVE_1:
            LOG_F(SELECTDISK, "SWIM3: selected drive 1");
#ifdef motor_off
            if (this->drive_2)
                this->drive_2->set_motor_stat(0);
#endif
            if (this->drive_1)
                this->selected_drive = this->drive_1;
            break;
        case SWIM3_DRIVE_2:
            LOG_F(SELECTDISK, "SWIM3: selected drive 2");
#ifdef motor_off
            if (this->drive_1)
                this->drive_1->set_motor_stat(0);
#endif
            if (this->drive_2)
                this->selected_drive = this->drive_2;
            break;
        case SWIM3_DRIVE_1 | SWIM3_DRIVE_2:
            LOG_F(ERROR, "SWIM3: both drives selected, selecting drive 1");
#ifdef motor_off
            if (this->drive_2)
                this->drive_2->set_motor_stat(0);
#endif
            if (this->drive_1)
                this->selected_drive = this->drive_1;
            break;
        }
        if (this->xfer_cnt) {
            LOG_F(ERROR, "SWIM3: selecting drive while xfer still in progress");
        }
    }

    if (changed_bits & SWIM3_GO_STEP) {
        if (new_mode & SWIM3_GO_STEP)
            start_stepping();
        else
            stop_stepping();
        if (changed_bits & SWIM3_GO) {
            LOG_F(ERROR, "SWIM3: attempt to change GO and GO_STEP, ignoring GO");
        }
    }
    else
    if (changed_bits & SWIM3_GO) {
        if (new_mode & SWIM3_GO)
            start_disk_access();
        else {
            stop_disk_access();
            this->cur_sector &= ~0x80;
        }
    }

    this->mode_reg = new_mode;
}

static const PropMap Swim3_Properties = {
    {"fdd_drives",
        new IntProperty(1, std::vector<uint32_t>({0, 1, 2}))},
};

static const DeviceDescription Swim3_Descriptor = {
    Swim3Ctrl::create, {}, Swim3_Properties, HWCompType::FLOPPY_CTRL
};

REGISTER_DEVICE(Swim3, Swim3_Descriptor);
