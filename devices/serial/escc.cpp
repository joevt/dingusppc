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

/** @file Enhanced Serial Communications Controller (ESCC) emulation. */

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/serial/chario.h>
#include <devices/serial/escc.h>
#include <devices/serial/z85c30.h>
#include <loguru.hpp>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

namespace loguru {
    enum : Verbosity {
        Verbosity_ESCCCHANNEL_CRC_RESET_CODES = loguru::Verbosity_9,
        Verbosity_ESCCCHANNEL_COMMAND_CODES = loguru::Verbosity_9,
        Verbosity_ESCCCHANNEL_RESET_HIGHEST_IUS = loguru::Verbosity_9,
        Verbosity_ESCCCHANNEL_REGISTER = loguru::Verbosity_9,
        Verbosity_ESCCCHANNEL_BAUD = loguru::Verbosity_9,
    };
}

/** Remap the compatible addressing scheme to MacRISC one. */
const uint8_t compat_to_macrisc[6] = {
    EsccReg::Port_B_Cmd,    EsccReg::Port_A_Cmd,
    EsccReg::Port_B_Data,   EsccReg::Port_A_Data,
    EsccReg::Enh_Reg_B,     EsccReg::Enh_Reg_A
};

EsccController::EsccController(const std::string &dev_name)
    : HWComponent(dev_name)
{
    // allocate channels
    // ch_a should have a lower unit address than ch_b so that it will get the default socket_backend value
    this->ch_a = dynamic_cast<EsccChannel*>(MachineFactory::create_device(this, "EsccChannel@A"));
    this->ch_b = dynamic_cast<EsccChannel*>(MachineFactory::create_device(this, "EsccChannel@B"));
    this->ch_a->set_controller(this);
    this->ch_b->set_controller(this);

    this->master_int_cntrl = 0;
    this->reset();
}

void EsccController::reset()
{
    VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s:     Hardware Reset", this->get_name_and_unit_address().c_str());
    this->write_reg(WR9, this->master_int_cntrl & (WR9_NO_VECTOR | WR9_VECTOR_INCLUDES_STATUS));
    this->reg_ptr = WR0; // or RR0
    {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s:     Hardware Reset A", this->get_name_and_unit_address().c_str());
        this->ch_a->reset(true);
    }
    {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s:     Hardware Reset B", this->get_name_and_unit_address().c_str());
        this->ch_b->reset(true);
    }
}

uint8_t EsccController::read(uint8_t reg_offset)
{
    uint8_t value;

    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        value = this->read_internal(this->ch_b);
        break;
    case EsccReg::Port_A_Cmd:
        value = this->read_internal(this->ch_a);
        break;
    case EsccReg::Port_B_Data:
        value = this->ch_b->receive_byte();
        break;
    case EsccReg::Port_A_Data:
        value = this->ch_a->receive_byte();
        break;
    case EsccReg::Enh_Reg_B:
        value = this->ch_b->get_enh_reg();
        break;
    case EsccReg::Enh_Reg_A:
        value = this->ch_a->get_enh_reg();
        break;
    default:
        value = 0;
        LOG_F(WARNING, "ESCC: read unimplemented register 0x%x", reg_offset);
    }

    return value;
}

void EsccController::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        this->write_internal(this->ch_b, value);
        break;
    case EsccReg::Port_A_Cmd:
        this->write_internal(this->ch_a, value);
        break;
    case EsccReg::Port_B_Data:
        this->ch_b->send_byte(value);
        break;
    case EsccReg::Port_A_Data:
        this->ch_a->send_byte(value);
        break;
    case EsccReg::Enh_Reg_B:
        this->ch_b->set_enh_reg(value);
        break;
    case EsccReg::Enh_Reg_A:
        this->ch_a->set_enh_reg(value);
        break;
    default:
        LOG_F(WARNING, "ESCC: write unimplemented register 0x%x = 0x%02x", reg_offset, value);
    }
}

uint8_t EsccController::read_internal(EsccChannel *ch)
{
    uint8_t value = ch->read_reg(this->reg_ptr);
    this->reg_ptr = RR0; // or WR0
    return value;
}

uint8_t EsccController::read_reg(int reg_num)
{
    uint8_t value;
    switch (reg_num) {
    case RR2:
        // TODO: implement interrupt vector modifications
        value = this->int_vec;
        LOG_F(ESCCCHANNEL_REGISTER, "%s: RR2  = %02X", this->get_name_and_unit_address().c_str(), value);
        break;
    default:
        value = 0;
    }
    return value;
}

void EsccController::write_internal(EsccChannel *ch, uint8_t value)
{
    if (this->reg_ptr) {
        ch->write_reg(this->reg_ptr, value);
        this->reg_ptr = WR0; // or RR0
    } else {
        this->reg_ptr = value & WR0_REGISTER_SELECTION_CODE;
        if ((value & WR0_COMMAND_CODES) == WR0_COMMAND_POINT_HIGH)
            this->reg_ptr |= WR8; // or RR8
        ch->write_reg(WR0, value);
    }
}

void EsccController::write_reg(int reg_num, uint8_t value)
{
    uint8_t changed_bits;
    switch (reg_num) {
    case WR2:
        changed_bits = this->int_vec ^ value;
        this->int_vec = value;
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR2  = %02X", this->get_name_and_unit_address().c_str(), value);
        if (changed_bits & WR2_V0)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V0:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V0) ? "1" : "0");
        if (changed_bits & WR2_V1)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V1:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V1) ? "1" : "0");
        if (changed_bits & WR2_V2)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V2:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V2) ? "1" : "0");
        if (changed_bits & WR2_V3)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V3:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V3) ? "1" : "0");
        if (changed_bits & WR2_V4)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V4:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V4) ? "1" : "0");
        if (changed_bits & WR2_V5)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V5:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V5) ? "1" : "0");
        if (changed_bits & WR2_V6)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V6:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V6) ? "1" : "0");
        if (changed_bits & WR2_V7)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     V7:%s", this->get_name_and_unit_address().c_str(),
                (value & WR2_V7) ? "1" : "0");
        break;
    case WR9:
        changed_bits = (value & WR9_RESET_COMMAND_BITS) | ((this->master_int_cntrl ^ value) & WR9_INTERRUPT_CONTROL_BITS);
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR9  = %02X", this->get_name_and_unit_address().c_str(), value);
        this->master_int_cntrl = value & WR9_INTERRUPT_CONTROL_BITS;
        if (changed_bits & WR9_INTERRUPT_MASKING_WITHOUT_INTACK)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Interrupt Masking without INTACK:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_INTERRUPT_MASKING_WITHOUT_INTACK) ? "1" : "0");
        if (changed_bits & WR9_STATUS_HIGH_STATUS_LOW)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     STATUS HIGH/STATUS LOW:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_STATUS_HIGH_STATUS_LOW) ? "1" : "0");
        if (changed_bits & WR9_MASTER_INTERRUPT_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Master Interrupt Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_MASTER_INTERRUPT_ENABLE) ? "1" : "0");
        if (changed_bits & WR9_DISABLE_LOWER_CHAIN)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Disable Lower Chain:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_DISABLE_LOWER_CHAIN) ? "1" : "0");
        if (changed_bits & WR9_NO_VECTOR)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     No Vector:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_NO_VECTOR) ? "1" : "0");
        if (changed_bits & WR9_VECTOR_INCLUDES_STATUS)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Vector Includes Status:%s", this->get_name_and_unit_address().c_str(),
                (value & WR9_VECTOR_INCLUDES_STATUS) ? "1" : "0");
        // see if some reset is requested
        switch (value & WR9_RESET_COMMAND_BITS) {
        case WR9_CHANNEL_RESET_B: {
            VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s:     Channel Reset", this->get_name_and_unit_address().c_str());
            this->ch_b->write_reg(WR9, this->master_int_cntrl & ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK);
            this->ch_b->reset(false);
            break;
        }
        case WR9_CHANNEL_RESET_A: {
            VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s:     Channel Reset", this->get_name_and_unit_address().c_str());
            this->ch_a->write_reg(WR9, this->master_int_cntrl & ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK);
            this->ch_a->reset(false);
            break;
        }
        case WR9_FORCE_HARDWARE_RESET:
            this->reset();
            break;
        }

        break;
    }
}

// ======================== ESCC Channel methods ==============================

HWComponent* EsccChannel::set_property(const std::string &property, const std::string &value, int32_t unit_address)
{
    if (!this->override_property(property, value))
        return nullptr;

    if (property == "serial_backend") {
        this->attach_backend(
            (value == "stdio") ? CHARIO_BE_STDIO :
#ifdef _WIN32
#else
            (value == "socket") ? CHARIO_BE_SOCKET :
#endif
            CHARIO_BE_NULL
        );

        return this;
    }

    return nullptr;
}

void EsccChannel::attach_backend(int id)
{
    switch(id) {
    case CHARIO_BE_NULL:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoNull(this->get_name() + "_CharIoNull"));
        break;
    case CHARIO_BE_STDIO:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoStdin(this->get_name() + "_CharIoStdin"));
        break;
#ifdef _WIN32
#else
    case CHARIO_BE_SOCKET:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoSocket(this->get_name() + "_CharIoSocket",
            std::string("dingussocket") + ((this->unit_address == 0xA) ? "" : "b")));
        break;
#endif
    default:
        LOG_F(ERROR, "%s: unknown backend ID %d, using NULL instead", this->get_name_and_unit_address().c_str(), id);
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoNull(this->get_name() + "_CharIoNull"));
    }
}

void EsccChannel::reset(bool hw_reset)
{
    this->chario->rcv_disable();

    /*
        We use hex values here instead of enums to more
        easily compare with the z85c30 data sheet.
    */

    this->write_reg(WR0, 0x00);
    this->write_reg(WR1, this->write_regs[WR1] & 0x24);
    this->write_reg(WR3, this->write_regs[WR3] & 0xFE);
    this->write_reg(WR4, this->write_regs[WR4] | 0x04);
    this->write_reg(WR5, this->write_regs[WR5] & 0x61);
    // skip WR9 which is handled by EsccController
    if (hw_reset)
        this->write_reg(WR10, 0x00);
    else
        this->write_reg(WR10, this->write_regs[WR10] & 0x60);
    if (hw_reset)
        this->write_reg(WR11, 0x08);

    uint8_t enables;
    if (hw_reset)
        enables = 0;
    else
        enables = this->write_regs[WR14] & 3;
    this->write_reg(WR14, WR14_DPLL_DISABLE_DPLL | enables);
    this->write_reg(WR14, WR14_DPLL_RESET_MISSING_CLOCK | enables);
    this->write_reg(WR14, WR14_DPLL_SET_SOURCE_RTXC | enables);
    this->write_reg(WR14, WR14_DPLL_SET_NRZI_MODE | enables);

    this->write_reg(WR15, 0xF8);

    this->read_regs[RR0]  = (this->read_regs[RR0] & 0x38) | 0x44;
    this->read_regs[RR1]  = 0x06 | RR1_ALL_SENT; // HACK: also set ALL_SENT flag.
    this->read_regs[RR3]  = 0x00;
    this->read_regs[RR10] = 0x00;
}

void EsccChannel::write_reg(int reg_num, uint8_t value)
{
    if ((reg_num == WR7) && (this->write_regs[WR15] & WR15_SDLC_HDLC_ENHANCEMENT_ENABLE))
        reg_num = WR7Prime;

    uint8_t changed_bits;
    if ((this->init_regs & (1 << reg_num)) == 0) {
        this->init_regs |= (1 << reg_num);
        changed_bits = 0xff;
    } else {
        changed_bits = this->write_regs[reg_num] ^ value;
    }
    bool do_update_baud_rate = false;

    switch (reg_num) {
    case WR0:
        LOG_F(ESCCCHANNEL_REGISTER, "%s: WR0  = %02X", this->get_name_and_unit_address().c_str(), value);
        switch(value & WR0_CRC_RESET_CODES) {
            case WR0_RESET_RX_CRC_CHECKER:
                LOG_F(ESCCCHANNEL_CRC_RESET_CODES, "%s:     Reset Rx CRC Checker.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_RESET_TX_CRC_GENERATOR:
                LOG_F(ESCCCHANNEL_CRC_RESET_CODES, "%s:     Reset Tx CRC Generator.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_RESET_TX_UNDERRUN_EOM_LATCH:
                LOG_F(ESCCCHANNEL_CRC_RESET_CODES, "%s:     Reset Tx Underrun/EOM Latch.",
                    this->get_name_and_unit_address().c_str());
                break;
        }
        switch(value & WR0_COMMAND_CODES) {
            case WR0_COMMAND_RESET_EXT_STATUS_INTERRUPTS:
                LOG_F(ESCCCHANNEL_COMMAND_CODES, "%s:     Reset EXT/Status Interrupts.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_COMMAND_SEND_ABORT_SDLC:
                LOG_F(ESCCCHANNEL_COMMAND_CODES, "%s:     Send Abort (SDLC).",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_COMMAND_ENABLE_INT_ON_NEXT_RX_CHARACTER:
                LOG_F(ESCCCHANNEL_COMMAND_CODES, "%s:     Enable INT On Next Rx Character.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_COMMAND_RESET_TXINT_PENDING:
                LOG_F(ESCCCHANNEL_COMMAND_CODES, "%s:     Reset TxINT Pending.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_COMMAND_ERROR_RESET:
                LOG_F(ESCCCHANNEL_COMMAND_CODES, "%s:     Error Reset.",
                    this->get_name_and_unit_address().c_str());
                break;
            case WR0_COMMAND_RESET_HIGHEST_IUS:
                LOG_F(ESCCCHANNEL_RESET_HIGHEST_IUS, "%s:     Reset Highest IUS.",
                    this->get_name_and_unit_address().c_str());
                break;
        }
        break;
    case WR1:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR1  = %02X", this->get_name_and_unit_address().c_str(), value);
        if (changed_bits & WR1_WAIT_DMA_REQUEST_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     WAIT/DMA Request Enable:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR1_WAIT_DMA_REQUEST_ENABLE) ? "1" : "0");
        if (changed_bits & WR1_WAIT_DMA_REQUEST_FUNCTION)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     WAIT/DMA Request Function:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR1_WAIT_DMA_REQUEST_FUNCTION) ? "1" : "0");
        if (changed_bits & WR1_WAIT_DMA_REQUEST_ON_RECEIVE_TRANSMIT)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     WAIT/DMA Request on RECEIVE/TRANSMIT:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR1_WAIT_DMA_REQUEST_ON_RECEIVE_TRANSMIT) ? "1" : "0");
        if (changed_bits & WR1_RECEIVE_INTERRUPT_MODES)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Receive Interrupt Mode:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR1_RECEIVE_INTERRUPT_MODES) == WR1_RX_INT_DISABLE ?
                    "Rx INT Disable" :
                (value & WR1_RECEIVE_INTERRUPT_MODES) == WR1_RX_INT_ON_FIRST_CHARACTER_OR_SPECIAL_CONDITION ?
                    "Rx INT on First Character or Special Condition" :
                (value & WR1_RECEIVE_INTERRUPT_MODES) == WR1_INT_ON_ALL_RX_CHARACTERS_OR_SPECIAL_CONDITION ?
                    "INT on All Rx Characters or Special Condition" :
                (value & WR1_RECEIVE_INTERRUPT_MODES) == WR1_RX_INT_ON_SPECIAL_CONDITION_ONLY ?
                    "Rx INT on Special Condition Only"
                :
                    ""
            );
        if (changed_bits & WR1_PARITY_IS_SPECIAL_CONDITION)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Parity is Special Condition:%s", this->get_name_and_unit_address().c_str(),
                (value & WR1_PARITY_IS_SPECIAL_CONDITION) ? "1" : "0");
        if (changed_bits & WR1_TX_INT_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Tx INT Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR1_TX_INT_ENABLE) ? "1" : "0");
        if (changed_bits & WR1_EXT_INT_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     EXT INT Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR1_EXT_INT_ENABLE) ? "1" : "0");
        break;
    case WR2:
        this->controller->write_reg(reg_num, value);
        return;
    case WR3:
        changed_bits = (changed_bits & ~WR3_ENTER_HUNT_MODE) | (changed_bits & value & WR3_ENTER_HUNT_MODE);
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR3  = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        this->write_regs[WR3] = (this->write_regs[WR3] & WR3_ENTER_HUNT_MODE) |
            (value & WR3_ENTER_HUNT_MODE) | (value & ~WR3_ENTER_HUNT_MODE);
        if (changed_bits & WR3_RECEIVER_BITS_PER_CHARACTER)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Rx Bits/Character:%s", this->get_name_and_unit_address().c_str(),
                (value & WR3_RECEIVER_BITS_PER_CHARACTER) == WR3_BITS_PER_CHARACTER_5 ? "5" :
                (value & WR3_RECEIVER_BITS_PER_CHARACTER) == WR3_BITS_PER_CHARACTER_7 ? "7" :
                (value & WR3_RECEIVER_BITS_PER_CHARACTER) == WR3_BITS_PER_CHARACTER_6 ? "6" :
                (value & WR3_RECEIVER_BITS_PER_CHARACTER) == WR3_BITS_PER_CHARACTER_8 ? "8" :
                ""
            );
        if (changed_bits & WR3_AUTO_ENABLES)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Auto Enables:%s", this->get_name_and_unit_address().c_str(),
                (value & WR3_AUTO_ENABLES) ? "1" : "0");
        if (changed_bits & WR3_ENTER_HUNT_MODE) {
            this->read_regs[RR0] |= RR0_SYNC_HUNT;
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Hunt mode entered.", this->get_name_and_unit_address().c_str());
        }
        if (changed_bits & WR3_RX_CRC_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Rx CRC Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR3_RX_CRC_ENABLE) ? "1" : "0");
        if (changed_bits & WR3_ADDRESS_SEARCH_MODE_SDLC)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Address Search Mode (SDLC):%s", this->get_name_and_unit_address().c_str(),
                (value & WR3_ADDRESS_SEARCH_MODE_SDLC) ? "1" : "0");
        if (changed_bits & WR3_SYNC_CHARACTER_LOAD_INHIBIT)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Sync Character Load Inhibit:%s", this->get_name_and_unit_address().c_str(),
                (value & WR3_SYNC_CHARACTER_LOAD_INHIBIT) ? "1" : "0");
        if (changed_bits & WR3_RX_ENABLE) {
            if (value & WR3_RX_ENABLE) {
                this->chario->rcv_enable();
                LOG_F(ESCCCHANNEL_REGISTER, "%s:     Receiver enabled", this->get_name_and_unit_address().c_str());
            } else {
                this->chario->rcv_disable();
                LOG_F(ESCCCHANNEL_REGISTER, "%s:     Receiver disabled", this->get_name_and_unit_address().c_str());
                this->write_reg(WR3, this->write_regs[WR3] | WR3_ENTER_HUNT_MODE);
            }
        }
        value = this->write_regs[WR3];
        break;
    case WR4:
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR4  = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        if (changed_bits & WR4_SYNC_MODE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     SYNC Mode:%s", this->get_name_and_unit_address().c_str(),
                (value & WR4_SYNC_MODE) == WR4_MONOSYNC           ? "8-Bit Sync Character" :
                (value & WR4_SYNC_MODE) == WR4_BISYNC             ? "16-Bit Sync Character" :
                (value & WR4_SYNC_MODE) == WR4_SDLC_MODE          ? "SDLC Mode (01111110 Flag)" :
                (value & WR4_SYNC_MODE) == WR4_EXTERNAL_SYNC_MODE ? "External Sync Mode" :
                ""
            );
        if (changed_bits & WR4_STOP_BITS)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Stop Bits:%s", this->get_name_and_unit_address().c_str(),
                (value & WR4_STOP_BITS) == WR4_SYNC_MODES_ENABLE                  ? "SYNC Modes Enable" :
                (value & WR4_STOP_BITS) == WR4_STOP_BITS_PER_CHARACTER_1          ? "1" :
                (value & WR4_STOP_BITS) == WR4_STOP_BITS_PER_CHARACTER_1_AND_HALF ? "1.5" :
                (value & WR4_STOP_BITS) == WR4_STOP_BITS_PER_CHARACTER_2          ? "2" :
                ""
            );
        if (changed_bits & (WR4_PARITY_ENABLE | WR4_PARITY))
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Parity:%s", this->get_name_and_unit_address().c_str(),
                (
                    (value & WR4_PARITY_ENABLE)
                ) ? (
                    (value & WR4_PARITY) == WR4_PARITY_ODD  ? "Odd" :
                    (value & WR4_PARITY) == WR4_PARITY_EVEN ? "Even" :
                    ""
                ) : "disabled"
            );
        break;
    case WR5:
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR5  = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        if (changed_bits & WR5_DTR)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     DTR:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_DTR) ? "1" : "0");
        if (changed_bits & WR5_TX_BITS_PER_CHARACTER)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Tx Bits/Character:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_TX_BITS_PER_CHARACTER) == WR5_TX_5_BITS_OR_LESS_PER_CHARACTER ? "<=5" :
                (value & WR5_TX_BITS_PER_CHARACTER) == WR5_TX_7_BITS_PER_CHARACTER         ? "7" :
                (value & WR5_TX_BITS_PER_CHARACTER) == WR5_TX_6_BITS_PER_CHARACTER         ? "6" :
                (value & WR5_TX_BITS_PER_CHARACTER) == WR5_TX_8_BITS_PER_CHARACTER         ? "8" :
                ""
            );
        if (changed_bits & WR5_SEND_BREAK)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Send Break:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_SEND_BREAK) ? "1" : "0");
        if (changed_bits & WR5_TX_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Tx Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_TX_ENABLE) ? "1" : "0");
        if (changed_bits & WR5_SDLC_CRC16)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     SDLC/CRC-16:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_SDLC_CRC16) ? "1" : "0");
        if (changed_bits & WR5_RTS)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     RTS:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_RTS) ? "1" : "0");
        if (changed_bits & WR5_TX_CRC_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Tx CRC Enable:%s", this->get_name_and_unit_address().c_str(),
                (value & WR5_TX_CRC_ENABLE) ? "1" : "0");
        break;
    case WR6:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR6  = %02X", this->get_name_and_unit_address().c_str(), value);
        break;
    case WR7:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR7  = %02X", this->get_name_and_unit_address().c_str(), value);
        break;
    case WR7Prime:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR7' = %02X", this->get_name_and_unit_address().c_str(), value);
        if (changed_bits & WR7_RESERVED)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Reserved:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_RESERVED) ? "1" : "0");
        if (changed_bits & WR7_EXTENDED_READ_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Extended Read Enable:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_EXTENDED_READ_ENABLE) ? "1" : "0");
        if (changed_bits & WR7_RECEIVE_CRC)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Receive CRC:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_RECEIVE_CRC) ? "1" : "0");
        if (changed_bits & WR7_DTR_REQ_TIMING_MODE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     DTR/REQ Timing Mode:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_DTR_REQ_TIMING_MODE) ? "1" : "0");
        if (changed_bits & WR7_TXD_FORCED_HIGH_IN_SDLC_NRZI_MODE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     TxD forced high in SDLC NRZI Mode:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_TXD_FORCED_HIGH_IN_SDLC_NRZI_MODE) ? "1" : "0");
        if (changed_bits & WR7_AUTO_RTS_DEACTIVATION)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Auto RTS Deactivation:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_AUTO_RTS_DEACTIVATION) ? "1" : "0");
        if (changed_bits & WR7_AUTO_EOM_RESET)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Auto EOM Reset:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_AUTO_EOM_RESET) ? "1" : "0");
        if (changed_bits & WR7_AUTO_TX_FLAG)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Auto Tx Flag:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR7_AUTO_TX_FLAG) ? "1" : "0");
        break;
    case WR8:
        this->send_byte(value);
        return;
    case WR9:
        this->controller->write_reg(reg_num, value);
        return;
    case WR10:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR10 = %02X", this->get_name_and_unit_address().c_str(), value);
        if (changed_bits & WR10_CRC_PRESET)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     CRC Preset:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_CRC_PRESET) ? "1" : "0");
        if (changed_bits & WR10_DATA_ENCODING)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Data Encoding:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_DATA_ENCODING) == WR10_NRZ  ? "NRZ" :
                (value & WR10_DATA_ENCODING) == WR10_NRZI ? "NRZI" :
                (value & WR10_DATA_ENCODING) == WR10_FM1  ? "FM1" :
                (value & WR10_DATA_ENCODING) == WR10_FM0  ? "FM0" :
                ""
            );
        if (changed_bits & WR10_GO_ACTIVE_ON_POLL)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Go Active On Poll:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_GO_ACTIVE_ON_POLL) ? "1" : "0");
        if (changed_bits & WR10_MARK_FLAG_IDLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Mark/Flag Idle:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_MARK_FLAG_IDLE) ? "1" : "0");
        if (changed_bits & WR10_ABORT_FLAG_ON_UNDERRUN)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Abort/Flag On Underrun:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_ABORT_FLAG_ON_UNDERRUN) ? "1" : "0");
        if (changed_bits & WR10_LOOP_MODE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Loop Mode:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_LOOP_MODE) ? "1" : "0");
        if (changed_bits & WR10_SYNC_SIZE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     6 Bit/8 Bit Sync:%s", this->get_name_and_unit_address().c_str(),
                (value & WR10_SYNC_SIZE) ? "1" : "0");
        break;
    case WR11:
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR11 = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        if (changed_bits & WR11_RTXC_XTAL_NO_XTAL)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     RTxC:%s", this->get_name_and_unit_address().c_str(),
                (value & WR11_RTXC_XTAL_NO_XTAL) ? "XTAL" : "no XTAL");
        if (changed_bits & WR11_RECEIVER_CLOCK)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Receive Clock:%s", this->get_name_and_unit_address().c_str(),
                (value & WR11_RECEIVER_CLOCK) == WR11_RECEIVE_CLOCK_RTXC_PIN            ? "RTxC Pin" :
                (value & WR11_RECEIVER_CLOCK) == WR11_RECEIVE_CLOCK_TRXC_PIN            ? "TRxC Pin" :
                (value & WR11_RECEIVER_CLOCK) == WR11_RECEIVE_CLOCK_BR_GENERATOR_OUTPUT ? "BRG Output" :
                (value & WR11_RECEIVER_CLOCK) == WR11_RECEIVE_CLOCK_DPLL_OUTPUT         ? "DPLL Output" :
                ""
            );
        if (changed_bits & WR11_TRANSMIT_CLOCK)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Transmit Clock:%s", this->get_name_and_unit_address().c_str(),
                (value & WR11_TRANSMIT_CLOCK) == WR11_TRANSMIT_CLOCK_RTXC_PIN            ? "RTxC Pin" :
                (value & WR11_TRANSMIT_CLOCK) == WR11_TRANSMIT_CLOCK_TRXC_PIN            ? "TRxC Pin" :
                (value & WR11_TRANSMIT_CLOCK) == WR11_TRANSMIT_CLOCK_BR_GENERATOR_OUTPUT ? "BRG Output" :
                (value & WR11_TRANSMIT_CLOCK) == WR11_TRANSMIT_CLOCK_DPLL_OUTPUT         ? "DPLL Output" :
                ""
            );
        if (changed_bits & (WR11_TRXC_O_I | WR11_TRXC_OUTPUT_SOURCE))
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     TRxC:%s", this->get_name_and_unit_address().c_str(),
                (
                    (value & WR11_TRXC_O_I) &&
                    (value & WR11_TRANSMIT_CLOCK) != WR11_TRANSMIT_CLOCK_TRXC_PIN &&
                    (value & WR11_RECEIVER_CLOCK) != WR11_RECEIVE_CLOCK_TRXC_PIN
                ) ? (
                    (value & WR11_TRXC_OUTPUT_SOURCE) == WR11_TRXC_OUT_XTAL_OUTPUT         ? "XTAL Oscillator Output" :
                    (value & WR11_TRXC_OUTPUT_SOURCE) == WR11_TRXC_OUT_TRANSMIT_CLOCK      ? "Transmit Clock" :
                    (value & WR11_TRXC_OUTPUT_SOURCE) == WR11_TRXC_OUT_BR_GENERATOR_OUTPUT ? "BRG Output" :
                    (value & WR11_TRXC_OUTPUT_SOURCE) == WR11_TRXC_OUT_DPLL_OUTPUT         ? "DPLL Output" :
                    ""
                ) : "input"
            );
        break;
    case WR12:
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR12 = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        break;
    case WR13:
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR13 = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        break;
    case WR14:
        changed_bits = (value & WR14_DPLL_COMMAND_BITS) | (changed_bits & ~WR14_DPLL_COMMAND_BITS);
        if (changed_bits) {
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR14 = %02X", this->get_name_and_unit_address().c_str(), value);
            do_update_baud_rate = true;
        }
        switch (value & WR14_DPLL_COMMAND_BITS) {
        case WR14_DPLL_NULL_COMMAND:
            break;
        case WR14_DPLL_ENTER_SEARCH_MODE:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Enter Search Mode", this->get_name_and_unit_address().c_str());
            this->dpll_active = 1;
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_RESET_MISSING_CLOCK:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Reset Missing Clock", this->get_name_and_unit_address().c_str());
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_DISABLE_DPLL:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Disable DPLL", this->get_name_and_unit_address().c_str());
            this->dpll_active = 0;
            [[fallthrough]];
        case WR14_DPLL_SET_SOURCE_BR_GENERATOR:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Set Source BR Generator", this->get_name_and_unit_address().c_str());
            this->dpll_clock_src = 0;
            break;
        case WR14_DPLL_SET_SOURCE_RTXC:
            if (!this->dpll_clock_src)
                LOG_F(ESCCCHANNEL_REGISTER, "%s:     Set Source RTxC", this->get_name_and_unit_address().c_str());
            this->dpll_clock_src = 1;
            break;
        case WR14_DPLL_SET_FM_MODE:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Set FM Mode", this->get_name_and_unit_address().c_str());
            this->dpll_mode = DpllMode::FM;
            break;
        case WR14_DPLL_SET_NRZI_MODE:
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Set NRZI Mode", this->get_name_and_unit_address().c_str());
            this->dpll_mode = DpllMode::NRZI;
            break;
        }
        if (changed_bits & WR14_LOCAL_LOOPBACK)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Local Loopback:%s", this->get_name_and_unit_address().c_str(),
                (value & WR14_LOCAL_LOOPBACK) ? "1" : "0");
        if (changed_bits & WR14_AUTO_ECHO)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Auto Echo:%s", this->get_name_and_unit_address().c_str(),
                (value & WR14_AUTO_ECHO) ? "1" : "0");
        if (changed_bits & WR14_DTR_REQUEST_FUNCTION)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     DTR/REQUEST Function:%s", this->get_name_and_unit_address().c_str(),
                (value & WR14_DTR_REQUEST_FUNCTION) ? "1" : "0");
        if (changed_bits & WR14_BR_GENERATOR_SOURCE) {
            this->brg_clock_src = value & WR14_BR_GENERATOR_SOURCE;
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     BR Generator Source:%s", this->get_name_and_unit_address().c_str(),
                (value & WR14_BR_GENERATOR_SOURCE) ? "PCLK" : "RTXC or XTAL");
        }
        if (changed_bits & WR14_BR_GENERATOR_ENABLE) {
            this->brg_active = value & WR14_BR_GENERATOR_ENABLE;
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     BR Generator %s", this->get_name_and_unit_address().c_str(),
                this->brg_active ? "enabled" : "disabled");
        }
        if (value & (WR14_LOCAL_LOOPBACK | WR14_AUTO_ECHO | WR14_DTR_REQUEST_FUNCTION)) {
            LOG_F(WARNING, "%s: unexpected value in WR14 = 0x%02X", this->get_name_and_unit_address().c_str(), value);
        }
        break;
    case WR15:
        if (changed_bits)
            LOG_F(ESCCCHANNEL_REGISTER, "%s: WR15 = %02X", this->get_name_and_unit_address().c_str(), value);
        if (changed_bits & WR15_BREAK_ABORT_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Break/Abort IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_BREAK_ABORT_IE) ? "1" : "0");
        if (changed_bits & WR15_TX_UNDERRUN_EOM_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Tx Underrun/EOM IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_TX_UNDERRUN_EOM_IE) ? "1" : "0");
        if (changed_bits & WR15_CTS_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     CTS IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_CTS_IE) ? "1" : "0");
        if (changed_bits & WR15_SYNC_HUNT_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     SYNC/HUNT IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_SYNC_HUNT_IE) ? "1" : "0");
        if (changed_bits & WR15_DCD_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     DCD IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_DCD_IE) ? "1" : "0");
        if (changed_bits & WR15_10_X_19_BIT_FRAME_STATUS_FIFO_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     10 x 19-Bit Frame Status FIFO Enable:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_10_X_19_BIT_FRAME_STATUS_FIFO_ENABLE) ? "1" : "0");
        if (changed_bits & WR15_ZERO_COUNT_IE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     Zero Count IE:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_ZERO_COUNT_IE) ? "1" : "0");
        if (changed_bits & WR15_SDLC_HDLC_ENHANCEMENT_ENABLE)
            LOG_F(ESCCCHANNEL_REGISTER, "%s:     SDLC/HDLC Enhancement Enable:%s",
                this->get_name_and_unit_address().c_str(),
                (value & WR15_SDLC_HDLC_ENHANCEMENT_ENABLE) ? "1" : "0");
        break;
    }

    this->write_regs[reg_num] = value;
    if (do_update_baud_rate)
        this->update_baud_rate();
}

uint8_t EsccChannel::read_reg(int reg_num)
{
    uint8_t value = this->read_regs[reg_num];
    switch (reg_num) {
    case RR0:
        if (this->chario->rcv_char_available()) {
            value |= RR0_RX_CHARACTER_AVAILABLE;
        } else {
            value &= ~RR0_RX_CHARACTER_AVAILABLE;
        }
        //LOG_F(ESCCCHANNEL_REGISTER, "%s: RR0 = %02X", this->get_name_and_unit_address().c_str(), value);
        break;
    case RR2:
        return this->controller->read_reg(reg_num);
    case RR8:
        value = this->receive_byte();
        //LOG_F(ESCCCHANNEL_REGISTER, "%s: RR8 = %02X", this->get_name_and_unit_address().c_str(), value);
        break;
    default:
        LOG_F(ESCCCHANNEL_REGISTER, "%s: RR%-2d = %02X", this->get_name_and_unit_address().c_str(), reg_num, value);
    }
    return value;
}

void EsccChannel::send_byte(uint8_t value)
{
    // TODO: put one byte into the Data FIFO

    this->write_regs[WR8] = value;
    this->chario->xmit_char(value);
}

uint8_t EsccChannel::receive_byte()
{
    // TODO: remove one byte from the Receive FIFO

    uint8_t c;

    if (this->chario->rcv_char_available_now()) {
        this->chario->rcv_char(&c);
    } else {
        c = 0;
    }
    this->read_regs[RR0] &= ~RR0_RX_CHARACTER_AVAILABLE;
    this->read_regs[RR8] = c;
    return c;
}

uint8_t EsccChannel::get_enh_reg()
{
    return this->enh_reg;
}

void EsccChannel::set_enh_reg(uint8_t value)
{
    uint8_t changed_bits = value ^ this->enh_reg;
    if (changed_bits & WORLDPORT) {
        if (value & WORLDPORT)
            LOG_F(ERROR, "%s: CTS connected to GPIO; DCD connected to GND",
                this->get_name_and_unit_address().c_str());
        else
            LOG_F(INFO, "%s: CTS connected to TRXC_In_l; DCD connected to GPIO",
                this->get_name_and_unit_address().c_str());
        this->enh_reg = value & WORLDPORT;
    } else if (changed_bits & ~WORLDPORT) {
        if (value & ~WORLDPORT)
            LOG_F(ERROR, "%s: Ignoring attempt to set Enh_Reg bits 0x%02x",
                this->get_name_and_unit_address().c_str(), value & ~WORLDPORT);
    }
}

void EsccChannel::update_baud_rate()
{
    uint32_t new_clock_mode;
    switch (this->write_regs[WR4] & WR4_CLOCK_RATE) {
        default:
        case WR4_X1_CLOCK_MODE  : new_clock_mode =  1; break;
        case WR4_X16_CLOCK_MODE : new_clock_mode = 16; break;
        case WR4_X32_CLOCK_MODE : new_clock_mode = 32; break;
        case WR4_X64_CLOCK_MODE : new_clock_mode = 64; break;
    }
    if (new_clock_mode != this->clock_mode) {
        LOG_F(ESCCCHANNEL_REGISTER, "%s:     Clock Mode:X%d", this->get_name_and_unit_address().c_str(), new_clock_mode);
        this->clock_mode = new_clock_mode;
    }

    uint32_t start_bits = 1;
    uint32_t data_bits[DIR_MAX+1];
    uint32_t parity_bits = (write_regs[WR4] & WR4_PARITY_ENABLE) ? 1 : 0;
    uint32_t stop_bits_x2 = 0;

    switch (this->write_regs[WR3] & WR3_RECEIVER_BITS_PER_CHARACTER) {
        case WR3_BITS_PER_CHARACTER_5: data_bits[DIR_RX] = 5; break;
        case WR3_BITS_PER_CHARACTER_7: data_bits[DIR_RX] = 7; break;
        case WR3_BITS_PER_CHARACTER_6: data_bits[DIR_RX] = 6; break;
        default:
        case WR3_BITS_PER_CHARACTER_8: data_bits[DIR_RX] = 8; break;
    }

    switch (this->write_regs[WR5] & WR5_TX_BITS_PER_CHARACTER) {
        case WR5_TX_5_BITS_OR_LESS_PER_CHARACTER : data_bits[DIR_TX] = 5; break;
        case WR5_TX_7_BITS_PER_CHARACTER         : data_bits[DIR_TX] = 7; break;
        case WR5_TX_6_BITS_PER_CHARACTER         : data_bits[DIR_TX] = 6; break;
        default:
        case WR5_TX_8_BITS_PER_CHARACTER         : data_bits[DIR_TX] = 8; break;
    }

    switch (this->write_regs[WR4] & WR4_STOP_BITS) {
        case WR4_SYNC_MODES_ENABLE:
            if (new_clock_mode != 1) {
                new_clock_mode = 1;
                LOG_F(ESCCCHANNEL_REGISTER, "%s:     Clock Mode:X%d (because of SYNC Modes Enable)",
                    this->get_name_and_unit_address().c_str(), new_clock_mode);
            }
            start_bits = 0;
            break;
        default:
        case WR4_STOP_BITS_PER_CHARACTER_1          : stop_bits_x2 = 2; break;
        case WR4_STOP_BITS_PER_CHARACTER_1_AND_HALF : stop_bits_x2 = 3; break;
        case WR4_STOP_BITS_PER_CHARACTER_2          : stop_bits_x2 = 4; break;
    }

    uint32_t new_time_constant = this->write_regs[WR12] | (this->write_regs[WR13] << 8);
    if (new_time_constant != this->time_constant) {
        LOG_F(ESCCCHANNEL_REGISTER, "%s:     Time Constant:%d", this->get_name_and_unit_address().c_str(), new_time_constant);
        this->time_constant = new_time_constant;
    }

    uint32_t rtxc_clock_output = (
        (this->write_regs[WR11] & WR11_RTXC_XTAL_NO_XTAL) == WR11_RTXC_XTAL) ?
            xtal_clock
        : (this->enh_reg & WORLDPORT) ?
            this->gpi_clock
        :
            this->internal_clock;

    uint32_t brg_clock = (this->brg_clock_src == WR14_BRG_SRC_PCLK)
        ? this->controller->get_pclk() : rtxc_clock_output;

    double brg_output = brg_clock / (2 * new_clock_mode * (this->time_constant + 2));

    double dpll_clock = this->dpll_clock_src ? brg_output : rtxc_clock_output;

    double new_baud_rate[DIR_MAX+1];
    double new_char_rate[DIR_MAX+1];
    for (int i = DIR_MIN; i <= DIR_MAX; i++) {
        // do WR11_RECEIVE_CLOCK and WR11_TRANSMIT_CLOCK
        uint8_t clock_src = this->write_regs[WR11] & (
            (i == DIR_TX) ? WR11_TRANSMIT_CLOCK :
               /* DIR_RX */ WR11_RECEIVER_CLOCK
        );
        switch (clock_src) {
            default:
            case WR11_RECEIVE_CLOCK_RTXC_PIN:
            //case WR11_TRANSMIT_CLOCK_RTXC_PIN:
                new_baud_rate[i] = rtxc_clock_output;
                break;
            case WR11_RECEIVE_CLOCK_TRXC_PIN:
            case WR11_TRANSMIT_CLOCK_TRXC_PIN:
                new_baud_rate[i] = this->trxc_clock;
                break;
            case WR11_RECEIVE_CLOCK_BR_GENERATOR_OUTPUT:
            case WR11_TRANSMIT_CLOCK_BR_GENERATOR_OUTPUT:
                new_baud_rate[i] = brg_output;
                break;
            case WR11_RECEIVE_CLOCK_DPLL_OUTPUT:
            case WR11_TRANSMIT_CLOCK_DPLL_OUTPUT:
                new_baud_rate[i] = dpll_clock / (
                    (dpll_mode == DpllMode::FM) ? 16 :
                    (dpll_mode == DpllMode::NRZI) ? 32 :
                    32
                );
                break;
        }
        new_char_rate[i] = new_baud_rate[i] * 2 / ((start_bits + data_bits[i] + parity_bits)*2 + stop_bits_x2);
    }

    bool logged_both_baud = false;
    bool logged_both_char = false;
    for (int i = DIR_MIN; i <= DIR_MAX; i++) {
        if (new_baud_rate[i] != this->baud_rate[i]) {
            this->baud_rate[i] = new_baud_rate[i];
            if (!logged_both_baud) {
                logged_both_baud = new_baud_rate[DIR_TX] == new_baud_rate[DIR_RX];
                LOG_F(ESCCCHANNEL_BAUD, "%s:     %s Baud Rate:%.2f bps", this->get_name_and_unit_address().c_str(),
                    logged_both_baud ? "Tx & Rx" :
                    (i == DIR_TX) ? "Tx" :
                       /* DIR_RX */ "Rx",
                    new_baud_rate[i]);
            }
        }
        if (new_char_rate[i] != this->char_rate[i]) {
            this->char_rate[i] = new_char_rate[i];
            if (!logged_both_char) {
                logged_both_char = new_baud_rate[DIR_TX] == new_baud_rate[DIR_RX];
                LOG_F(ESCCCHANNEL_BAUD, "%s:     %s Char Rate:%.2f cps", this->get_name_and_unit_address().c_str(),
                    logged_both_char ? "Tx & Rx" :
                    (i == DIR_TX) ? "Tx" :
                       /* DIR_RX */ "Rx",
                    new_char_rate[i]);
            }
        }
    }
}

void EsccChannel::dma_start_tx()
{

}

void EsccChannel::dma_start_rx()
{

}

void EsccChannel::dma_stop_tx()
{
    if (this->timer_id_tx) {
        TimerManager::get_instance()->cancel_timer(this->timer_id_tx);
        this->timer_id_tx = 0;
    }
}

void EsccChannel::dma_stop_rx()
{
    if (this->timer_id_rx) {
        TimerManager::get_instance()->cancel_timer(this->timer_id_rx);
        this->timer_id_rx = 0;
    }
}

void EsccChannel::dma_in_tx()
{
    LOG_F(ERROR, "%s: Unexpected DMA INPUT command for transmit.", this->get_name_and_unit_address().c_str());
}

void EsccChannel::dma_in_rx()
{
    if (dma_ch[DIR_RX]->get_push_data_remaining()) {
        this->timer_id_rx = TimerManager::get_instance()->add_oneshot_timer(
            0,
            [this](uint64_t, uint64_t) {
                this->timer_id_rx = 0;
                char c = receive_byte();
                dma_ch[DIR_RX]->push_data(&c, 1);
                this->dma_in_rx();
        });
    }
}

void EsccChannel::dma_out_tx()
{
    this->timer_id_tx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this](uint64_t, uint64_t) {
            this->timer_id_tx = 0;
            uint8_t *data;
            uint32_t avail_len;

            if (dma_ch[DIR_TX]->pull_data(256, &avail_len, &data) == MoreData) {
                while(avail_len) {
                    this->send_byte(*data++);
                    avail_len--;
                }
                this->dma_out_tx();
            }
    });
}

void EsccChannel::dma_out_rx()
{
    LOG_F(ERROR, "%s: Unexpected DMA OUTPUT command for receive.", this->get_name_and_unit_address().c_str());
}

void EsccChannel::dma_flush_tx()
{
    this->dma_stop_tx();
    this->timer_id_tx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this](uint64_t, uint64_t) {
            this->timer_id_tx = 0;
            dma_ch[DIR_TX]->end_pull_data();
    });
}

void EsccChannel::dma_flush_rx()
{
    this->dma_stop_rx();
    this->timer_id_rx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this](uint64_t, uint64_t) {
            this->timer_id_rx = 0;
            dma_ch[DIR_RX]->end_push_data();
    });
}

static const std::vector<std::string> CharIoBackends = {"null", "stdio", "socket"};

static const PropMap EsccChannel_Properties = {
    {"serial_backend", new StrProperty("null", CharIoBackends)},
};

static const DeviceDescription EsccChannel_Descriptor = {
    EsccChannel::create, {}, EsccChannel_Properties, HWCompType::MMIO_DEV
};

REGISTER_DEVICE(EsccChannel, EsccChannel_Descriptor);

static const DeviceDescription Escc_Descriptor = {
    EsccController::create, {}, {}, HWCompType::MMIO_DEV
};

REGISTER_DEVICE(Escc, Escc_Descriptor);
REGISTER_DEVICE(EsccPdm, Escc_Descriptor);
