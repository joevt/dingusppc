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

/** @file MacIO 2nd generation I/O controllers emulation. */

#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <loguru.hpp>

namespace loguru {
    enum : Verbosity {
        Verbosity_INTERRUPT = loguru::Verbosity_9,
        Verbosity_DBDMA = loguru::Verbosity_9
    };
}

MacIoTwo::MacIoTwo(std::string name, uint16_t dev_id)
    : MacIoBase(name, dev_id), HWComponent(name)
{
    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));

    // connect SCSI controller cell and its DMA channel
    this->mesh = dynamic_cast<MeshController*>(gMachineObj->get_comp_by_type(HWCompType::SCSI_HOST));
    this->mesh_dma = std::unique_ptr<DMAChannel> (new DMAChannel("mesh"));
    this->mesh_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_MESH));
    this->mesh_dma->connect(this->mesh);
    this->mesh->connect(this->mesh_dma.get());

    // connect IDE HW
    this->ide_0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide0"));
    this->ide_1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name_optional("Ide1"));

    // connect Ethernet HW (Heathrow and Paddington)
    if (this->device_id != MIO_DEV_ID_OHARE) {
        this->bmac = dynamic_cast<BigMac*>(gMachineObj->get_comp_by_type(HWCompType::ETHER_MAC));
        this->enet_xmit_dma = std::unique_ptr<DMAChannel> (new DMAChannel("BmacTx"));
        this->enet_rcv_dma  = std::unique_ptr<DMAChannel> (new DMAChannel("BmacRx"));
    }

    // set EMMO status (active low)
    this->emmo = GET_BIN_PROP("emmo") ^ 1;
}

uint32_t MacIoTwo::read(uint32_t /*rgn_start*/, uint32_t offset, int size) {
    uint32_t value;

    LOG_F(9, "%s: read @%x.%c", this->get_name().c_str(),
        offset, SIZE_ARG(size));

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        value = mio_ctrl_read(offset, size);
        break;
    case 8:
        value = dma_read(offset & 0x7FFF, size);
        break;
    case 0x10: // SCSI
        value = this->mesh->read((offset >> 4) & 0xF);
        break;
    case 0x11: // Ethernet
        value = this->bmac ? BYTESWAP_SIZED(this->bmac->read(offset & 0xFFFU), size) : 0;
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            value = this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            value = 0;
            LOG_F(ERROR, "%s: ESCC compatible read  @%x.%c", this->name.c_str(),
                offset, SIZE_ARG(size));
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        value = this->escc->read((offset >> 4) & 0xF);
        break;
    case 0x14:
        value = this->snd_codec->snd_ctrl_read(offset & 0xFF, size);
        break;
    case 0x15: // SWIM3
        value = this->swim3->read((offset >> 4) & 0xF);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        value = this->viacuda->read((offset >> 9) & 0xF);
        break;
    case 0x20: // IDE 0
        value = this->ide_0->read((offset >> 4) & 0x1F, size);
        break;
    case 0x21: // IDE 1
        value = this->ide_1 ? this->ide_1->read((offset >> 4) & 0x1F, size) : 0;
        break;
    default:
        if (sub_addr >= 0x60) {
            value = this->nvram->read_byte((offset >> 4) & 0x1FFF);
        } else {
            value = 0;
            LOG_F(WARNING, "%s: read @%x.%c", this->get_name().c_str(),
                offset, SIZE_ARG(size));
        }
    }

    return value;
}

void MacIoTwo::write(uint32_t /*rgn_start*/, uint32_t offset, uint32_t value, int size) {
    LOG_F(9, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
        offset, SIZE_ARG(size), size * 2, value);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        this->mio_ctrl_write(offset, value, size);
        break;
    case 8:
        this->dma_write(offset & 0x7FFF, value, size);
        break;
    case 0x10: // SCSI
        this->mesh->write((offset >> 4) & 0xF, value);
        break;
    case 0x11: // Ethernet
        if (this->bmac)
            this->bmac->write(offset & 0xFFFU, BYTESWAP_SIZED(value, size));
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            LOG_F(ERROR, "%s: SCC write @%x.%c = %0*x", this->name.c_str(),
                offset, SIZE_ARG(size), size * 2, value);
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14:
        this->snd_codec->snd_ctrl_write(offset & 0xFF, value, size);
        break;
    case 0x15: // SWIM3
        this->swim3->write((offset >> 4) & 0xF, value);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        this->viacuda->write((offset >> 9) & 0xF, value);
        break;
    case 0x20: // IDE 0
        this->ide_0->write((offset >> 4) & 0x1F, value, size);
        break;
    case 0x21: // IDE 1
        if (this->ide_1)
            this->ide_1->write((offset >> 4) & 0x1F, value, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
                offset, SIZE_ARG(size), size * 2, value);
        }
    }
}

static const char *get_name_dma(unsigned dma_channel) {
    switch (dma_channel) {
        case MIO_OHARE_DMA_MESH        : return "DMA_MESH"       ;
        case MIO_OHARE_DMA_FLOPPY      : return "DMA_FLOPPY"     ;
        case MIO_OHARE_DMA_ETH_XMIT    : return "DMA_ETH_XMIT"   ;
        case MIO_OHARE_DMA_ETH_RCV     : return "DMA_ETH_RCV"    ;
        case MIO_OHARE_DMA_ESCC_A_XMIT : return "DMA_ESCC_A_XMIT";
        case MIO_OHARE_DMA_ESCC_A_RCV  : return "DMA_ESCC_A_RCV" ;
        case MIO_OHARE_DMA_ESCC_B_XMIT : return "DMA_ESCC_B_XMIT";
        case MIO_OHARE_DMA_ESCC_B_RCV  : return "DMA_ESCC_B_RCV" ;
        case MIO_OHARE_DMA_AUDIO_OUT   : return "DMA_AUDIO_OUT"  ;
        case MIO_OHARE_DMA_AUDIO_IN    : return "DMA_AUDIO_IN"   ;
        case MIO_OHARE_DMA_IDE0        : return "DMA_IDE0"       ;
        case MIO_OHARE_DMA_IDE1        : return "DMA_IDE1"       ;
        default                        : return "unknown"        ;
    }
}

uint32_t MacIoTwo::dma_read(uint32_t offset, int size) {
    uint32_t value;
    int dma_channel = offset >> 8;
    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        value = this->mesh_dma->reg_read(offset & 0xFF, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        value = this->floppy_dma->reg_read(offset & 0xFF, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        //value = this->enet_xmit_dma ? this->enet_xmit_dma->reg_read(offset & 0xFF, size) : 0;
        value = 0;
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        //value = this->enet_rcv_dma ? this->enet_rcv_dma->reg_read(offset & 0xFF, size) : 0;
        value = 0;
        break;
    case MIO_OHARE_DMA_ESCC_A_XMIT:
        //value = this->escc_a_tx_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_ESCC_A_RCV:
        //value = this->escc_a_rx_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_ESCC_B_XMIT:
        //value = this->escc_b_tx_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_ESCC_B_RCV:
        //value = this->escc_b_rx_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        value = this->snd_out_dma->reg_read(offset & 0xFF, size);
        break;
    default:
        if (!(unsupported_dma_channel_read & (1 << dma_channel))) {
            unsupported_dma_channel_read |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s read  @%02x.%c",
                this->name.c_str(), dma_channel, get_name_dma(dma_channel),
                offset & 0xFF, SIZE_ARG(size));
            return 0;
        }
        value = 0;
    }
    LOG_F(DBDMA, "read  %s @%02x.%c = %0*x", get_name_dma(dma_channel),
        offset & 0xFF, SIZE_ARG(size), size * 2, value);
    return value;
}

void MacIoTwo::dma_write(uint32_t offset, uint32_t value, int size) {
    int dma_channel = offset >> 8;

    LOG_F(DBDMA, "write %s @%02x.%c = %0*x", get_name_dma(dma_channel),
        offset & 0xFF, SIZE_ARG(size), size * 2, value);

    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        this->mesh_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        //if (this->enet_xmit_dma)
        //    this->enet_xmit_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        //if (this->enet_rcv_dma)
        //    this->enet_rcv_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_A_XMIT:
        //this->escc_a_tx_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_A_RCV:
        //this->escc_a_rx_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_B_XMIT:
        //this->escc_b_tx_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_B_RCV:
        //this->escc_b_rx_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        if (!(unsupported_dma_channel_write & (1 << dma_channel))) {
            unsupported_dma_channel_write |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s write @%02x.%c = %0*x",
                this->name.c_str(), dma_channel, get_name_dma(dma_channel),
                offset & 0xFF, SIZE_ARG(size), size * 2, value);
        }
    }
}

uint32_t MacIoTwo::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t value2 = 0;
    uint32_t value = mio_ctrl_read_aligned(offset & ~3);
    if ((offset & 3) + size > 4) {
        value2 = mio_ctrl_read_aligned((offset & ~3) + 4);
    }
    AccessDetails details;
    ACCESSDETAILS_SET(details, size, offset, 0);
    value = conv_rd_data(value, value2, details);
    return value;
}

uint32_t MacIoTwo::mio_ctrl_read_aligned(uint32_t offset) {
    uint32_t value;

    switch (offset & 0x7FFC) {
    case MIO_INT_EVENTS2:
        value = this->int_events >> 32;
        break;
    case MIO_INT_MASK2:
        value = this->int_mask >> 32;
        break;
    case MIO_INT_LEVELS2:
        value = this->int_levels >> 32;
        break;
    case MIO_INT_EVENTS1:
        value = uint32_t(this->int_events);
        break;
    case MIO_INT_MASK1:
        value = uint32_t(this->int_mask);
        break;
    case MIO_INT_LEVELS1:
        value = uint32_t(int_levels);
        break;
    case MIO_INT_CLEAR1:
    case MIO_INT_CLEAR2:
        // some Mac OS drivers read from these write-only registers
        // so we return zero here as real HW does
        value = 0;
        break;
    case MIO_OHARE_ID:
        value = (this->fp_id << 24) | (this->mon_id << 16) | (this->mb_id << 8) |
            (this->cpu_id | (this->emmo << 4));
        LOG_F(9, "%s: read OHARE_ID @%02x = %08x",
            this->get_name().c_str(), offset, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        value = this->feat_ctrl;
        LOG_F(9, "%s: read  FEAT_CTRL @%02x = %08x",
            this->get_name().c_str(), offset, value);
        break;
    default:
        value = 0;
        LOG_F(WARNING, "%s: read @%02x",
            this->get_name().c_str(), offset);
    }

    return value;
}

void MacIoTwo::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    if (size != 4) {
        LOG_F(ERROR, "%s: write size not supported @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value
        );
    }

    switch (offset & 0x7FFC) {
    case MIO_INT_MASK2:
        this->int_mask |= uint64_t(BYTESWAP_32(value) & ~MACIO_INT_MODE) << 32;
        LOG_F(INTERRUPT, "%s: int_mask2:0x%08x", name.c_str(), uint32_t(this->int_mask >> 32));
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR2:
        this->int_events &= ~(uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL) << 32);
        clear_cpu_int();
        break;
    case MIO_INT_MASK1:
        this->int_mask = (this->int_mask & 0x7FFFFFFF00000000ULL) | BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask |= uint64_t(this->int_mask & MACIO_INT_MODE) << 32;
        LOG_F(INTERRUPT, "%s: int_mask1:0x%08x", name.c_str(), uint32_t(this->int_mask));
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events = 0;
        } else {
            this->int_events &= ~uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        }
        clear_cpu_int();
        break;
    case MIO_INT_LEVELS1:
        LOG_F(INTERRUPT, "%s: write INT_LEVELS1 @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value); // writing 0x100000 happens often
        break;
    case MIO_OHARE_ID:
        LOG_F(ERROR, "%s: write OHARE_ID @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        LOG_F(WARNING, "%s: write FEAT_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->feature_control(BYTESWAP_32(value));
        break;
    case MIO_AUX_CTRL:
        LOG_F(9, "%s: write AUX_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->aux_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "%s: write @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
    }
}

void MacIoTwo::feature_control(uint32_t value) {
    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "%s: monitor sense enabled", this->name.c_str());
    } else {
        LOG_F(9, "%s: monitor sense disabled", this->name.c_str());
    }
}

IntSrc MacIoTwo::irq_id_to_src(uint64_t irq_id) {
    switch(irq_id) {
    case INT_TO_IRQ_ID(0x0C) : return IntSrc::SCSI_MESH;
    case INT_TO_IRQ_ID(0x0D) : return IntSrc::IDE0;
    case INT_TO_IRQ_ID(0x0E) : return IntSrc::IDE1;
    case INT_TO_IRQ_ID(0x0F) : return IntSrc::SCCA;
    case INT_TO_IRQ_ID(0x10) : return IntSrc::SCCB;
    case INT_TO_IRQ_ID(0x11) : return IntSrc::DAVBUS;
    case INT_TO_IRQ_ID(0x12) : return IntSrc::VIA_CUDA;
    case INT_TO_IRQ_ID(0x13) : return IntSrc::SWIM3;
    case INT_TO_IRQ_ID(0x14) : return IntSrc::NMI;
  //case INT_TO_IRQ_ID(0x15) : return IntSrc::EXT1;

  //case INT_TO_IRQ_ID(0x16) : return IntSrc::BANDIT1;
  //case INT_TO_IRQ_ID(0x16) : return IntSrc::PCI_E;
  //case INT_TO_IRQ_ID(0x17) : return IntSrc::PCI_A;
  //case INT_TO_IRQ_ID(0x18) : return IntSrc::PCI_F;
  //case INT_TO_IRQ_ID(0x19) : return IntSrc::PCI_B;
  //case INT_TO_IRQ_ID(0x1A) : return IntSrc::???;
  //case INT_TO_IRQ_ID(0x1B) : return IntSrc::???;
  //case INT_TO_IRQ_ID(0x1C) : return IntSrc::PCI_C;

    case INT_TO_IRQ_ID(0x15) : return IntSrc::PERCH2;
    case INT_TO_IRQ_ID(0x16) : return IntSrc::PCI_GPU;
  //case INT_TO_IRQ_ID(0x16) : return IntSrc::PCI_CARDBUS;
    case INT_TO_IRQ_ID(0x17) : return IntSrc::PCI_A;
    case INT_TO_IRQ_ID(0x18) : return IntSrc::PCI_B;
  //case INT_TO_IRQ_ID(0x18) : return IntSrc::PCI_E;
    case INT_TO_IRQ_ID(0x19) : return IntSrc::PCI_C;
    case INT_TO_IRQ_ID(0x1A) : return IntSrc::PERCH1;
    case INT_TO_IRQ_ID(0x1C) : return IntSrc::PCI_PERCH;

  //case INT_TO_IRQ_ID(0x15) : return IntSrc::FIREWIRE;
  //case INT_TO_IRQ_ID(0x16) : return IntSrc::PCI_J12;
  //case INT_TO_IRQ_ID(0x17) : return IntSrc::PCI_J11;
  //case INT_TO_IRQ_ID(0x18) : return IntSrc::PCI_J10;
  //case INT_TO_IRQ_ID(0x19) : return IntSrc::PCI_J9;
  //case INT_TO_IRQ_ID(0x1A) : return IntSrc::ATA;
  //case INT_TO_IRQ_ID(0x1A) : return IntSrc::ZIVA;
  //case INT_TO_IRQ_ID(0x1C) : return IntSrc::USB;
    case INT_TO_IRQ_ID(0x1D) : return IntSrc::MEDIA_BAY;

    case INT_TO_IRQ_ID(0x2A) : return IntSrc::ETHERNET;

    case INT_TO_IRQ_ID(0x00) : return IntSrc::DMA_SCSI_MESH;
    case INT_TO_IRQ_ID(0x01) : return IntSrc::DMA_SWIM3;
    case INT_TO_IRQ_ID(0x02) : return IntSrc::DMA_IDE0;
    case INT_TO_IRQ_ID(0x03) : return IntSrc::DMA_IDE1;
    case INT_TO_IRQ_ID(0x04) : return IntSrc::DMA_SCCA_Tx;
    case INT_TO_IRQ_ID(0x05) : return IntSrc::DMA_SCCA_Rx;
    case INT_TO_IRQ_ID(0x06) : return IntSrc::DMA_SCCB_Tx;
    case INT_TO_IRQ_ID(0x07) : return IntSrc::DMA_SCCB_Rx;
    case INT_TO_IRQ_ID(0x08) : return IntSrc::DMA_DAVBUS_Tx;
    case INT_TO_IRQ_ID(0x09) : return IntSrc::DMA_DAVBUS_Rx;
    case INT_TO_IRQ_ID(0x20) : return IntSrc::DMA_ETHERNET_Tx;
    case INT_TO_IRQ_ID(0x21) : return IntSrc::DMA_ETHERNET_Rx;
    }
    return IntSrc::INT_UNKNOWN;
}

uint64_t MacIoTwo::register_dev_int(IntSrc src_id) {
    if (this->device_id == MIO_DEV_ID_OHARE && src_id == ETHERNET) {
        ABORT_F("%s: attempt to register non-existing Ethernet device int",
                this->name.c_str());
    }

    switch (src_id) {
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::IDE0       : return INT_TO_IRQ_ID(0x0D); // Beige G3 first IDE controller, or Yosemite ata-3
    case IntSrc::IDE1       : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
    case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14); // nmiSource in AppleHeathrow/Heathrow.cpp; programmer-switch in B&W G3

    case IntSrc::BANDIT1    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_E      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x16) // same interrupt as bandit
                                   : INT_TO_IRQ_ID(0x18); // Lombard GPU
    case IntSrc::PCI_F      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_B      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x19)
                                   : INT_TO_IRQ_ID(0x18);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1A);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1B);
    case IntSrc::PCI_C      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x1C)
                                   : INT_TO_IRQ_ID(0x19);

    case IntSrc::PERCH2     : return INT_TO_IRQ_ID(0x15);
    case IntSrc::PCI_GPU    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_CARDBUS: return INT_TO_IRQ_ID(0x16); // Lombard
    case IntSrc::PERCH1     : return INT_TO_IRQ_ID(0x1A);
    case IntSrc::PCI_PERCH  : return INT_TO_IRQ_ID(0x1C);

    case IntSrc::FIREWIRE   : return INT_TO_IRQ_ID(0x15); // Yosemite built-in PCI FireWire
    case IntSrc::PCI_J12    : return INT_TO_IRQ_ID(0x16); // Yosemite 32-bit 66MHz slot for GPU
    case IntSrc::PCI_J11    : return INT_TO_IRQ_ID(0x17); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J10    : return INT_TO_IRQ_ID(0x18); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J9     : return INT_TO_IRQ_ID(0x19); // Yosemite 64-bit 33MHz slot
    case IntSrc::ATA        : return INT_TO_IRQ_ID(0x1A); // Yosemite PCI pci-ata
    case IntSrc::ZIVA       : return INT_TO_IRQ_ID(0x1A); // Lombard ZiVA DVD Decoder
    case IntSrc::USB        : return INT_TO_IRQ_ID(0x1C); // Yosemite/Lombard PCI USB
    case IntSrc::MEDIA_BAY  : return INT_TO_IRQ_ID(0x1D); // Lombard

    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x2A);

    default:
        ABORT_F("%s: unknown interrupt source %d", this->name.c_str(), src_id);
    }

    return 0;
}

uint64_t MacIoTwo::register_dma_int(IntSrc src_id) {
    if (this->device_id == MIO_DEV_ID_OHARE &&
        (src_id == IntSrc::DMA_ETHERNET_Tx || src_id == IntSrc::DMA_ETHERNET_Rx)) {
        ABORT_F("%s: attempt to register non-existing Ethernet DMA int", this->name.c_str());
    }

    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH      : return INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_IDE0           : return INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_IDE1           : return INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_ETHERNET_Tx    : return INT_TO_IRQ_ID(0x20);
    case IntSrc::DMA_ETHERNET_Rx    : return INT_TO_IRQ_ID(0x21);
    default:
        return MacIoBase::register_dma_int(src_id);
    }

    return 0;
}

//===========================================================================
static const std::vector<std::string> OHare_Subdevices = {
    "NVRAM@60000", "ViaCuda@16000", "MeshTnt@10000", "Escc@13000", "Swim3@15000", "Ide0@20000", "Ide1@21000"
};

static const std::vector<std::string> Heathrow_Subdevices = {
    "NVRAM@60000", "ViaCuda@16000", "MeshHeathrow@10000", "Escc@13000", "Swim3@15000", "Ide0@20000", "Ide1@21000",
    "BigMacHeathrow@11000"
};

static const std::vector<std::string> Paddington_Subdevices = {
    "NVRAM@60000", "ViaCuda@16000", "MeshHeathrow@10000", "Escc@13000", "Swim3@15000", "Ide0@20000", "Ide1@21000",
    "BigMacPaddington@11000"
};

static const DeviceDescription OHare_Descriptor = {
    MacIoTwo::create_ohare, OHare_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

static const DeviceDescription Heathrow_Descriptor = {
    MacIoTwo::create_heathrow, Heathrow_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

static const DeviceDescription Paddington_Descriptor = {
    MacIoTwo::create_paddington, Paddington_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

REGISTER_DEVICE(OHare, OHare_Descriptor);
REGISTER_DEVICE(Heathrow, Heathrow_Descriptor);
REGISTER_DEVICE(Paddington, Paddington_Descriptor);
