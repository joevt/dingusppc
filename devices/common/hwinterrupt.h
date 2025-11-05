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

#include <cinttypes>

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
    ERROR,
    PCI_FW0,
    PCI_FW1,
    PCI_SLOT1,
    PCI_SLOT2,
    PCI_SLOT3,
    PCI_SLOT4,
    PCI_SLOT5,
    PCI_SLOT6,
    SEC_TO_PRI,
};

/** Base class for interrupt controllers. */
class InterruptCtrl {
public:
    InterruptCtrl() = default;
    virtual ~InterruptCtrl() = default;

    // register interrupt sources for a device
    virtual uint64_t register_dev_int(IntSrc src_id) = 0;
    virtual uint64_t register_dma_int(IntSrc src_id) = 0;

    // acknowledge HW interrupt
    virtual void ack_int(uint64_t irq_id, uint8_t irq_line_state)     = 0;
    virtual void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) = 0;

    // logging
    virtual IntSrc irq_id_to_src(uint64_t irq_id) = 0;
    const char* irq_id_to_name(uint64_t irq_id) { return irq_src_to_name(irq_id_to_src(irq_id)); }
    const char* irq_src_to_name(IntSrc irq_src) {
    #define irq_src_to_name(a) case a: return #a;
    switch (irq_src) {
        irq_src_to_name(INT_UNKNOWN)
        irq_src_to_name(VIA_CUDA)
        irq_src_to_name(VIA2)
        irq_src_to_name(SCSI_MESH)
        irq_src_to_name(SCSI_CURIO)
        irq_src_to_name(SWIM3)
        irq_src_to_name(ESCC)
        irq_src_to_name(SCCA)
        irq_src_to_name(SCCB)
        irq_src_to_name(ETHERNET)
        irq_src_to_name(NMI)
        irq_src_to_name(EXT1)
        irq_src_to_name(IDE0)
        irq_src_to_name(IDE1)
        irq_src_to_name(DAVBUS)
        irq_src_to_name(PERCH1)
        irq_src_to_name(PERCH2)
        irq_src_to_name(PCI_A)
        irq_src_to_name(PCI_B)
        irq_src_to_name(PCI_C)
        irq_src_to_name(PCI_D)
        irq_src_to_name(PCI_E)
        irq_src_to_name(PCI_F)
        irq_src_to_name(PCI_GPU)
        irq_src_to_name(PCI_PERCH)
        irq_src_to_name(BANDIT1)
        irq_src_to_name(BANDIT2)
        irq_src_to_name(CONTROL)
        irq_src_to_name(SIXTY6)
        irq_src_to_name(PLANB)
        irq_src_to_name(VCI)
        irq_src_to_name(PLATINUM)
        irq_src_to_name(DMA_ALL)
        irq_src_to_name(DMA_SCSI_MESH)
        irq_src_to_name(DMA_SCSI_CURIO)
        irq_src_to_name(DMA_SWIM3)
        irq_src_to_name(DMA_IDE0)
        irq_src_to_name(DMA_IDE1)
        irq_src_to_name(DMA_SCCA_Tx)
        irq_src_to_name(DMA_SCCA_Rx)
        irq_src_to_name(DMA_SCCB_Tx)
        irq_src_to_name(DMA_SCCB_Rx)
        irq_src_to_name(DMA_DAVBUS_Tx)
        irq_src_to_name(DMA_DAVBUS_Rx)
        irq_src_to_name(DMA_ETHERNET_Tx)
        irq_src_to_name(DMA_ETHERNET_Rx)
        irq_src_to_name(FIREWIRE)
        irq_src_to_name(PCI_J12)
        irq_src_to_name(PCI_J11)
        irq_src_to_name(PCI_J10)
        irq_src_to_name(PCI_J9)
        irq_src_to_name(ATA)
        irq_src_to_name(USB)
        irq_src_to_name(PIPPIN_E)
        irq_src_to_name(PIPPIN_F)
        irq_src_to_name(ZIVA)
        irq_src_to_name(PCI_CARDBUS)
        irq_src_to_name(MEDIA_BAY)
        irq_src_to_name(SLOT_ALL)
        irq_src_to_name(SLOT_0)
        irq_src_to_name(SLOT_1)
        irq_src_to_name(SLOT_2)
        irq_src_to_name(SLOT_PDS)
        irq_src_to_name(SLOT_VDS)
        irq_src_to_name(VBL)
        irq_src_to_name(ERROR)
        irq_src_to_name(PCI_FW0)
        irq_src_to_name(PCI_FW1)
        irq_src_to_name(PCI_SLOT1)
        irq_src_to_name(PCI_SLOT2)
        irq_src_to_name(PCI_SLOT3)
        irq_src_to_name(PCI_SLOT4)
        irq_src_to_name(PCI_SLOT5)
        irq_src_to_name(PCI_SLOT6)
        irq_src_to_name(SEC_TO_PRI)
        #undef irq_src_to_name
        default: return "unknown";
        }
    }
};

typedef struct {
    InterruptCtrl   *int_ctrl_obj;
    uint64_t        irq_id;
} IntDetails;

#endif // HW_INTERRUPT_H
