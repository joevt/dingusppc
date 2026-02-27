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
    this->write_reg(WR9, this->master_int_cntrl & (WR9_NO_VECTOR | WR9_VECTOR_INCLUDES_STATUS));
    this->reg_ptr = WR0; // or RR0

    this->ch_a->reset(true);
    this->ch_b->reset(true);
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
        LOG_F(WARNING, "ESCC: reading from unimplemented register 0x%x", reg_offset);
        value = 0;
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
        LOG_F(9, "ESCC: writing 0x%X to unimplemented register 0x%x", value, reg_offset);
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
    }
}

void EsccController::write_reg(int reg_num, uint8_t value)
{
    uint8_t changed_bits;
    switch (reg_num) {
    case WR2:
        changed_bits = this->int_vec ^ value;
        this->int_vec = value;
        break;
    case WR9:
        changed_bits = (value & WR9_RESET_COMMAND_BITS) | ((this->master_int_cntrl ^ value) & WR9_INTERRUPT_CONTROL_BITS);
        this->master_int_cntrl = value & WR9_INTERRUPT_CONTROL_BITS;
        // see if some reset is requested
        switch (value & WR9_RESET_COMMAND_BITS) {
        case WR9_CHANNEL_RESET_B:
            this->ch_b->write_reg(WR9, this->master_int_cntrl & ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK);
            this->ch_b->reset(false);
            break;
        case WR9_CHANNEL_RESET_A:
            this->ch_a->write_reg(WR9, this->master_int_cntrl & ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK);
            this->ch_a->reset(false);
            break;
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
    changed_bits = this->write_regs[reg_num] ^ value;
    bool do_update_baud_rate = false;

    switch (reg_num) {
    case WR2:
        this->controller->write_reg(reg_num, value);
        return;
    case WR3:
        changed_bits = (changed_bits & ~WR3_ENTER_HUNT_MODE) | (changed_bits & value & WR3_ENTER_HUNT_MODE);
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        this->write_regs[WR3] = (this->write_regs[WR3] & WR3_ENTER_HUNT_MODE) |
            (value & WR3_ENTER_HUNT_MODE) | (value & ~WR3_ENTER_HUNT_MODE);
        if (changed_bits & WR3_ENTER_HUNT_MODE) {
            this->read_regs[RR0] |= RR0_SYNC_HUNT;
            LOG_F(9, "%s: Hunt mode entered.", this->get_name_and_unit_address().c_str());
        }
        if (changed_bits & WR3_RX_ENABLE) {
            if (value & WR3_RX_ENABLE) {
                this->chario->rcv_enable();
                LOG_F(9, "%s: receiver enabled.", this->get_name_and_unit_address().c_str());
            } else {
                this->chario->rcv_disable();
                LOG_F(9, "%s: receiver disabled.", this->get_name_and_unit_address().c_str());
                this->write_reg(WR3, this->write_regs[WR3] | WR3_ENTER_HUNT_MODE);
            }
        }
        value = this->write_regs[WR3];
        break;
    case WR4:
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        break;
    case WR5:
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        break;
    case WR7:
        break;
    case WR7Prime:
        break;
    case WR8:
        this->send_byte(value);
        return;
    case WR9:
        this->controller->write_reg(reg_num, value);
        return;
    case WR11:
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        break;
    case WR12:
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        break;
    case WR13:
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        break;
    case WR14:
        changed_bits = (value & WR14_DPLL_COMMAND_BITS) | (changed_bits & ~WR14_DPLL_COMMAND_BITS);
        if (changed_bits) {
            do_update_baud_rate = true;
        }
        switch (value & WR14_DPLL_COMMAND_BITS) {
        case WR14_DPLL_NULL_COMMAND:
            break;
        case WR14_DPLL_ENTER_SEARCH_MODE:
            this->dpll_active = 1;
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_RESET_MISSING_CLOCK:
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_DISABLE_DPLL:
            this->dpll_active = 0;
            [[fallthrough]];
        case WR14_DPLL_SET_SOURCE_BR_GENERATOR:
            this->dpll_clock_src = 0;
            break;
        case WR14_DPLL_SET_SOURCE_RTXC:
            this->dpll_clock_src = 1;
            break;
        case WR14_DPLL_SET_FM_MODE:
            this->dpll_mode = DpllMode::FM;
            break;
        case WR14_DPLL_SET_NRZI_MODE:
            this->dpll_mode = DpllMode::NRZI;
            break;
        }
        if (changed_bits & WR14_BR_GENERATOR_SOURCE) {
            this->brg_clock_src = value & WR14_BR_GENERATOR_SOURCE;
        }
        if (changed_bits & WR14_BR_GENERATOR_ENABLE) {
            this->brg_active = value & WR14_BR_GENERATOR_ENABLE;
            LOG_F(9, "%s: BRG %s", this->get_name_and_unit_address().c_str(), this->brg_active ? "enabled" : "disabled");
        }
        if (value & (WR14_LOCAL_LOOPBACK | WR14_AUTO_ECHO | WR14_DTR_REQUEST_FUNCTION)) {
            LOG_F(WARNING, "%s: unexpected value in WR14 = 0x%02X", this->get_name_and_unit_address().c_str(), value);
        }
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
        break;
    case RR2:
        return this->controller->read_reg(reg_num);
    case RR8:
        value = this->receive_byte();
        break;
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
            new_clock_mode = 1;
            start_bits = 0;
            break;
        default:
        case WR4_STOP_BITS_PER_CHARACTER_1          : stop_bits_x2 = 2; break;
        case WR4_STOP_BITS_PER_CHARACTER_1_AND_HALF : stop_bits_x2 = 3; break;
        case WR4_STOP_BITS_PER_CHARACTER_2          : stop_bits_x2 = 4; break;
    }

    uint32_t new_time_constant = this->write_regs[WR12] | (this->write_regs[WR13] << 8);
    if (new_time_constant != this->time_constant) {
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

    for (int i = DIR_MIN; i <= DIR_MAX; i++) {
        if (new_baud_rate[i] != this->baud_rate[i]) {
            this->baud_rate[i] = new_baud_rate[i];
        }
        if (new_char_rate[i] != this->char_rate[i]) {
            this->char_rate[i] = new_char_rate[i];
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
            [this]() {
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
        [this]() {
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
        [this]() {
            this->timer_id_tx = 0;
            dma_ch[DIR_TX]->end_pull_data();
    });
}

void EsccChannel::dma_flush_rx()
{
    this->dma_stop_rx();
    this->timer_id_rx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this]() {
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
