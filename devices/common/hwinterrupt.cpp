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

#include <devices/common/hwinterrupt.h>
#include <loguru.hpp>

uint64_t InterruptCtrl::register_int(IntSrc src_id)
{
    if (this->int_src_to_irq_id.count(src_id)) {
        uint64_t irq_id = this->int_src_to_irq_id[src_id];
        if (!this->irq_id_to_int_src.count(irq_id))
            this->irq_id_to_int_src[irq_id] = src_id;
        else if (this->irq_id_to_int_src[irq_id] != src_id) {
            LOG_F(WARNING, "%s: %s and %s use the same irq_id", this->name.c_str(),
                int_src_to_name(src_id), int_src_to_name(this->irq_id_to_int_src[irq_id]));
        }
        return irq_id;
    }
    ABORT_F("%s: unknown interrupt source %d", this->get_name().c_str(), src_id);
}

void InterruptCtrl::add_intsrc(IntSrc src_id, uint64_t irq_id) {
    this->int_src_to_irq_id[src_id] = irq_id;
}

const char* InterruptCtrl::irq_id_to_name(uint64_t irq_id)
{
    if (this->irq_id_to_int_src.count(irq_id))
        return int_src_to_name(irq_id_to_int_src[irq_id]);
    return("UNREGISTERED_INTERRUPT");
}

const char* InterruptCtrl::int_src_to_name(IntSrc irq_src)
{
    switch (irq_src) {
        #define onesrc(a) case a: return #a;
        onesrc(INT_UNKNOWN)
        onesrc(VIA_CUDA)
        onesrc(VIA2)
        onesrc(SCSI_MESH)
        onesrc(SCSI_CURIO)
        onesrc(SWIM3)
        onesrc(ESCC)
        onesrc(SCCA)
        onesrc(SCCB)
        onesrc(ETHERNET)
        onesrc(NMI)
        onesrc(EXT1)
        onesrc(IDE0)
        onesrc(IDE1)
        onesrc(DAVBUS)
        onesrc(PERCH1)
        onesrc(PERCH2)
        onesrc(PCI_A)
        onesrc(PCI_B)
        onesrc(PCI_C)
        onesrc(PCI_D)
        onesrc(PCI_E)
        onesrc(PCI_F)
        onesrc(PCI_GPU)
        onesrc(PCI_PERCH)
        onesrc(BANDIT1)
        onesrc(BANDIT2)
        onesrc(CONTROL)
        onesrc(SIXTY6)
        onesrc(PLANB)
        onesrc(VCI)
        onesrc(PLATINUM)
        onesrc(DMA_ALL)
        onesrc(DMA_SCSI_MESH)
        onesrc(DMA_SCSI_CURIO)
        onesrc(DMA_SWIM3)
        onesrc(DMA_IDE0)
        onesrc(DMA_IDE1)
        onesrc(DMA_SCCA_Tx)
        onesrc(DMA_SCCA_Rx)
        onesrc(DMA_SCCB_Tx)
        onesrc(DMA_SCCB_Rx)
        onesrc(DMA_DAVBUS_Tx)
        onesrc(DMA_DAVBUS_Rx)
        onesrc(DMA_ETHERNET_Tx)
        onesrc(DMA_ETHERNET_Rx)
        onesrc(FIREWIRE)
        onesrc(PCI_J12)
        onesrc(PCI_J11)
        onesrc(PCI_J10)
        onesrc(PCI_J9)
        onesrc(ATA)
        onesrc(USB)
        onesrc(PIPPIN_E)
        onesrc(PIPPIN_F)
        onesrc(ZIVA)
        onesrc(PCI_CARDBUS)
        onesrc(MEDIA_BAY)
        onesrc(SLOT_ALL)
        onesrc(SLOT_0)
        onesrc(SLOT_1)
        onesrc(SLOT_2)
        onesrc(SLOT_PDS)
        onesrc(SLOT_VDS)
        onesrc(VBL)
        #undef onesrc
        default: return "unknown";
    }
}
