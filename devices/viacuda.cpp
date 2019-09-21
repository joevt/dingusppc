//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)


#include <iostream>
#include <cinttypes>
#include <vector>
#include "viacuda.h"

using namespace std;


ViaCuda::ViaCuda()
{
    /* FIXME: is this the correct
       VIA initialization? */
    this->via_regs[VIA_A]    = 0x80;
    this->via_regs[VIA_DIRB] = 0xFF;
    this->via_regs[VIA_DIRA] = 0xFF;
    this->via_regs[VIA_T1LL] = 0xFF;
    this->via_regs[VIA_T1LH] = 0xFF;
    this->via_regs[VIA_IER]  = 0x7F;

    this->cuda_init();
}

void ViaCuda::cuda_init()
{
    this->old_tip = 0;
    this->old_byteack = 0;
    this->treq = 1;
    this->in_count = 0;
    this->out_count = 0;
}

uint8_t ViaCuda::read(int reg)
{
    uint8_t res;

    cout << "Read VIA reg " << hex << (uint32_t)reg << endl;

    res = this->via_regs[reg & 0xF];

    /* reading from some VIA registers triggers special actions */
    switch(reg & 0xF) {
    case VIA_B:
        res = this->via_regs[VIA_B];
        break;
    case VIA_A:
    case VIA_ANH:
        cout << "WARNING: read attempt from VIA Port A!" << endl;
        break;
    case VIA_IER:
        res |= 0x80; /* bit 7 always reads as "1" */
    }

    return res;
}

void ViaCuda::write(int reg, uint8_t value)
{
    switch(reg & 0xF) {
    case VIA_B:
        this->via_regs[VIA_B] = value;
        cuda_write(value);
        break;
    case VIA_A:
    case VIA_ANH:
        cout << "WARNING: write attempt to VIA Port A!" << endl;
        break;
    case VIA_DIRB:
        cout << "VIA_DIRB = " << hex << (uint32_t)value << endl;
        this->via_regs[VIA_DIRB] = value;
        break;
    case VIA_DIRA:
        cout << "VIA_DIRA = " << hex << (uint32_t)value << endl;
        this->via_regs[VIA_DIRA] = value;
        break;
    case VIA_PCR:
        cout << "VIA_PCR = " << hex << (uint32_t)value << endl;
        this->via_regs[VIA_PCR] = value;
        break;
    case VIA_ACR:
        cout << "VIA_ACR = " << hex << (uint32_t)value << endl;
        this->via_regs[VIA_ACR] = value;
        break;
    case VIA_IER:
        this->via_regs[VIA_IER] = (value & 0x80) ? value & 0x7F
                                  : this->via_regs[VIA_IER] & ~value;
        cout << "VIA_IER updated to " << hex << (uint32_t)this->via_regs[VIA_IER]
             << endl;
        print_enabled_ints();
        break;
    default:
        this->via_regs[reg & 0xF] = value;
    }
}

void ViaCuda::print_enabled_ints()
{
    vector<string> via_int_src = {"CA2", "CA1", "SR", "CB2", "CB1", "T2", "T1"};

    for (int i = 0; i < 7; i++) {
        if (this->via_regs[VIA_IER] & (1 << i))
            cout << "VIA " << via_int_src[i] << " interrupt enabled." << endl;
    }
}

inline bool ViaCuda::cuda_ready()
{
    return ((this->via_regs[VIA_DIRB] & 0x38) == 0x30);
}

inline void ViaCuda::assert_sr_int()
{
    this->via_regs[VIA_IFR] |= 0x84;
}

void ViaCuda::cuda_write(uint8_t new_state)
{
    if (!cuda_ready()) {
        cout << "Cuda not ready!" << endl;
        return;
    }

    int new_tip = !!(new_state & CUDA_TIP);
    int new_byteack = !!(new_state & CUDA_BYTEACK);

    /* return if there is no state change */
    if (new_tip == this->old_tip && new_byteack == this->old_byteack)
        return;

    cout << "Cuda state changed!" << endl;

    this->old_tip = new_tip;
    this->old_byteack = new_byteack;

    if (new_tip) {
        if (new_byteack) {
            this->via_regs[VIA_B] |= CUDA_TREQ; /* negate TREQ */
            this->treq = 1;

            if (this->in_count) {
                cuda_process_packet();

                /* start response transaction */
                this->via_regs[VIA_B] &= ~CUDA_TREQ; /* assert TREQ */
                this->treq = 0;
            }

            this->in_count = 0;
        } else {
            cout << "Cuda: enter sync state" << endl;
            this->via_regs[VIA_B] &= ~CUDA_TREQ; /* assert TREQ */
            this->treq = 0;
            this->in_count = 0;
            this->out_count = 0;
        }

        assert_sr_int(); /* send dummy byte as idle acknowledge or attention */
    } else {
        if (this->via_regs[VIA_ACR] & 0x10) { /* data transfer: Host --> Cuda */
            if (this->in_count < 16) {
                this->in_buf[this->in_count++] = this->via_regs[VIA_SR];
                assert_sr_int(); /* tell the system we've read the data */
            } else {
                cout << "Cuda input buffer exhausted!" << endl;
            }
        } else { /* data transfer: Cuda --> Host */
            if (this->out_count) {
                this->via_regs[VIA_SR] = this->out_buf[this->out_pos++];

                if (this->out_pos >= this->out_count) {
                    cout << "Cuda: sending last byte" << endl;
                    this->out_count = 0;
                    this->via_regs[VIA_B] |= CUDA_TREQ; /* negate TREQ */
                    this->treq = 1;
                }

                assert_sr_int(); /* tell the system we've written the data */
            }
        }
    }
}

void ViaCuda::cuda_null_response(uint32_t pkt_type, uint32_t pkt_flag, uint32_t cmd)
{
    this->out_buf[0] = pkt_type;
    this->out_buf[1] = pkt_flag;
    this->out_buf[2] = cmd;
    this->out_count = 3;
    this->out_pos = 0;
}

void ViaCuda::cuda_process_packet()
{
    if (this->in_count < 2) {
        cout << "Cuda: invalid packet (too few data)!" << endl;
        return;
    }

    switch(this->in_buf[0]) {
    case 0:
        cout << "Cuda: ADB packet received" << endl;
        break;
    case 1:
        cout << "Cuda: pseudo command packet received" << endl;
        cout << "Command: " << hex << (uint32_t)(this->in_buf[1]) << endl;
        cout << "Data count: " << dec << this->in_count << endl;
        for (int i = 0; i < this->in_count; i++) {
            cout << hex << (uint32_t)(this->in_buf[i]) << ", ";
        }
        cout << endl;
        cuda_pseudo_command(this->in_buf[1], this->in_count - 2);
        break;
    default:
        cout << "Cuda: unsupported packet type = " << dec << (uint32_t)(this->in_buf[0]) << endl;
    }
}

void ViaCuda::cuda_pseudo_command(int cmd, int data_count)
{
    switch(cmd) {
    case CUDA_READ_WRITE_I2C:
        cuda_null_response(1, 0, cmd);
        /* bit 0 of the I2C address byte indicates operation kind:
           0 - write to device, 1 - read from device
           In the case of reading, Cuda will append one-byte result
           to the response packet header */
        if (this->in_buf[2] & 1) {
            this->out_buf[3] = 0xDD; /* send dummy byte for now */
            this->out_count++;
        }
        break;
    case CUDA_OUT_PB0: /* undocumented call! */
        cout << "Cuda: send " << dec << (int)(this->in_buf[2]) << " to PB0" << endl;
        cuda_null_response(1, 0, cmd);
        break;
    default:
        cout << "Cuda: unsupported pseudo command 0x" << hex << cmd << endl;
    }
}
