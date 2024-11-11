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

#ifndef HW_INTERRUPT_H
#define HW_INTERRUPT_H

#include <devices/common/hwcomponent.h>

#define DEBUG_CPU_INT // uncomment this to enable hacks for debugging HW interrupts

/** Enumerator for various interrupt sources. */
enum IntSrc : uint32_t {
    INT_UNKNOWN = 0,
    VIA_CUDA,
    VIA2,
    SCSI_MESH,
    SCSI_CURIO,
    SWIM3,
    ESCC,
    SCCA,
    SCCB,
    ETHERNET,
    NMI,
    EXT1,
    IDE0,
    IDE1,
    DAVBUS,
    PERCH1,
    PERCH2,
    PCI_A,
    PCI_B,
    PCI_C,
    PCI_D,
    PCI_E,
    PCI_F,
    PCI_GPU,
    PCI_PERCH,
    BANDIT1,
    BANDIT2,
    CONTROL,
    SIXTY6,
    PLANB,
    VCI,
    PLATINUM,
    DMA_ALL,
    DMA_SCSI_MESH,
    DMA_SCSI_CURIO,
    DMA_SWIM3,
    DMA_IDE0,
    DMA_IDE1,
    DMA_SCCA_Tx,
    DMA_SCCA_Rx,
    DMA_SCCB_Tx,
    DMA_SCCB_Rx,
    DMA_DAVBUS_Tx,
    DMA_DAVBUS_Rx,
    DMA_ETHERNET_Tx,
    DMA_ETHERNET_Rx,
    FIREWIRE,
    PCI_J12,
    PCI_J11,
    PCI_J10,
    PCI_J9,
    ATA,
    USB,
    PIPPIN_E,
    PIPPIN_F,
    ZIVA,
    PCI_CARDBUS,
    MEDIA_BAY,
    SLOT_ALL,
    SLOT_0,
    SLOT_1,
    SLOT_2,
    SLOT_PDS,
    SLOT_VDS,
    VBL,
};

/** Base class for interrupt controllers. */
class InterruptCtrl : virtual public HWComponent {
public:
    InterruptCtrl() = default;
    virtual ~InterruptCtrl() = default;

    // register interrupt sources
    void add_intsrc(IntSrc src_id, uint64_t irq_id);

    // register interrupt sources for a device
    uint64_t register_int(IntSrc src_id);

    // acknowledge HW interrupt
    virtual void ack_int(uint64_t irq_id, uint8_t irq_line_state)     = 0;
    virtual void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) = 0;

    // logging
    const char* irq_id_to_name(uint64_t irq_id);
    const char* int_src_to_name(IntSrc irq_src);

private:
    std::map<IntSrc,uint64_t> int_src_to_irq_id;
    std::map<uint64_t,IntSrc> irq_id_to_int_src;
};

typedef struct {
    InterruptCtrl   *int_ctrl_obj;
    uint64_t        irq_id;
} IntDetails;

#endif // HW_INTERRUPT_H
