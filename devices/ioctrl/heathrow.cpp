/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

#include <cpu/ppc/ppcemu.h>
#include <devices/deviceregistry.h>
#include <devices/common/ata/idechannel.h>
#include <devices/common/dbdma.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/viacuda.h>
#include <devices/floppy/swim3.h>
#include <devices/ioctrl/macio.h>
#include <devices/serial/escc.h>
#include <devices/sound/awacs.h>
#include <endianswap.h>
#include <loguru.hpp>

#include <cinttypes>
#include <functional>
#include <memory>

namespace loguru {
    enum : Verbosity {
        Verbosity_INTERRUPT = loguru::Verbosity_9,
        Verbosity_DBDMA = loguru::Verbosity_9
    };
}

/** Heathrow Mac I/O device emulation.

    Author: Max Poliakovski
*/

using namespace std;

HeathrowIC::HeathrowIC()
    : PCIDevice("Heathrow"), InterruptCtrl(), HWComponent("Heathrow")
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL);

    // populate my PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0010;
    this->class_rev   = 0xFF000001;
    this->cache_ln_sz = 8;

    this->setup_bars({{0, 0xFFF80000UL}}); // declare 512Kb of memory-mapped I/O space

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));

    // connect Cuda
    this->viacuda = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));

    // find appropriate sound chip, create a DMA output channel for sound,
    // then wire everything together
    this->snd_codec = dynamic_cast<MacioSndCodec*>(gMachineObj->get_comp_by_type(HWCompType::SND_CODEC));
    this->add_device(0x14000, this->snd_codec);
    this->snd_out_dma = std::unique_ptr<DMAChannel> (new DMAChannel("snd_out"));
    this->snd_out_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_DAVBUS_Tx));
    this->snd_codec->set_dma_out(this->snd_out_dma.get());
    this->snd_out_dma->set_callbacks(
        std::bind(&AwacsScreamer::dma_out_start, this->snd_codec),
        std::bind(&AwacsScreamer::dma_out_stop, this->snd_codec)
    );

    // connect SCSI HW and the corresponding DMA channel
    this->mesh = dynamic_cast<MeshController*>(gMachineObj->get_comp_by_name("MeshHeathrow"));
    this->mesh_dma = std::unique_ptr<DMAChannel> (new DMAChannel("mesh"));
    this->mesh_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_MESH));
    this->mesh_dma->connect(this->mesh);
    this->mesh->connect(this->mesh_dma.get());

    // connect IDE HW
    this->ide_0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide0"));
    this->ide_1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide1"));

    // connect serial HW
    this->escc = dynamic_cast<EsccController*>(gMachineObj->get_comp_by_name("Escc"));

    // connect floppy disk HW and initialize its DMA channel
    this->swim3 = dynamic_cast<Swim3::Swim3Ctrl*>(gMachineObj->get_comp_by_name("Swim3"));
    this->floppy_dma = std::unique_ptr<DMAChannel> (new DMAChannel("floppy"));
    this->swim3->set_dma_channel(this->floppy_dma.get());
    this->floppy_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SWIM3));

    // connect Ethernet HW
    this->bmac = dynamic_cast<BigMac*>(gMachineObj->get_comp_by_type(HWCompType::ETHER_MAC));
    this->enet_xmit_dma = std::unique_ptr<DMAChannel> (new DMAChannel("BmacTx"));
    this->enet_rcv_dma  = std::unique_ptr<DMAChannel> (new DMAChannel("BmacRx"));

    // set EMMO pin status (active low)
    this->emmo_pin = GET_BIN_PROP("emmo") ^ 1;
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

void HeathrowIC::set_media_bay_id(uint8_t id) {
    this->mb_id = id;
}

void HeathrowIC::notify_bar_change(int bar_num)
{
    if (bar_num) // only BAR0 is supported
        return;

    if (this->base_addr != (this->bars[bar_num] & 0xFFFFFFF0UL)) {
        if (this->base_addr) {
            this->host_instance->pci_unregister_mmio_region(this->base_addr, 0x80000, this);
        }
        this->base_addr = this->bars[0] & 0xFFFFFFF0UL;
        this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
        LOG_F(INFO, "%s: base address set to 0x%X", this->get_name().c_str(), this->base_addr);
    }
}

uint32_t HeathrowIC::read(uint32_t /*rgn_start*/, uint32_t offset, int size) {
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
        value = BYTESWAP_SIZED(this->bmac->read(offset & 0xFFFU), size);
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            value = this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            value = 0;
            LOG_F(ERROR, "%s: ESCC compatible read  @%x.%c", this->name.c_str(), offset, SIZE_ARG(size));
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
        value = this->ide_1->read((offset >> 4) & 0x1F, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            value = this->nvram->read_byte((offset - 0x60000) >> 4);
        } else {
            value = 0;
            LOG_F(WARNING, "%s: read @%x.%c", this->get_name().c_str(),
                offset, SIZE_ARG(size));
        }
    }

    return value;
}

void HeathrowIC::write(uint32_t /*rgn_start*/, uint32_t offset, uint32_t value, int size) {
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

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t value2 = 0;
    uint32_t value = mio_ctrl_read_aligned(offset & ~3);
    if ((offset & 3) + size > 4) {
        value2 = mio_ctrl_read_aligned((offset & ~3) + 4);
    }
    AccessDetails details;
    details.size = size;
    details.offset = offset & 3;
    value = pci_conv_rd_data(value, value2, details);
    return value;
}

uint32_t HeathrowIC::mio_ctrl_read_aligned(uint32_t offset) {
    uint32_t value;

    switch (offset & 0x7FFC) {
    case MIO_INT_EVENTS2:
        value = this->int_events2;
        break;
    case MIO_INT_MASK2:
        value = this->int_mask2;
        break;
    case MIO_INT_LEVELS2:
        value = this->int_levels2;
        break;
    case MIO_INT_EVENTS1:
        value = this->int_events1;
        break;
    case MIO_INT_MASK1:
        value = this->int_mask1;
        break;
    case MIO_INT_LEVELS1:
        value = this->int_levels1;
        break;
    case MIO_INT_CLEAR1:
    case MIO_INT_CLEAR2:
        // some Mac OS drivers reads from those write-only registers
        // so we return zero here as real HW does
        value = 0;
        break;
    case MIO_OHARE_ID:
        value = (this->fp_id << 24) | (this->mon_id << 16) | (this->mb_id << 8) |
            (this->cpu_id | (this->emmo_pin << 4));
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

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    if (size != 4) {
        LOG_F(ERROR, "%s: write size not supported @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value
        );
    }

    switch (offset & 0x7FFC) {
    case MIO_INT_MASK2:
        this->int_mask2 |= BYTESWAP_32(value) & ~MACIO_INT_MODE;
        LOG_F(INTERRUPT, "%s: int_mask2:0x%08x", name.c_str(), this->int_mask2);
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR2:
        this->int_events2 &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        clear_cpu_int();
        break;
    case MIO_INT_MASK1:
        this->int_mask1 = BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask2 = (this->int_mask2 & ~MACIO_INT_MODE) | (this->int_mask1 & MACIO_INT_MODE);
        LOG_F(INTERRUPT, "%s: int_mask1:0x%08x", name.c_str(), this->int_mask1);
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask1 & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events1 = 0;
            this->int_events2 = 0;
        } else {
            this->int_events1 &= ~(BYTESWAP_32(value) & 0x7FFFFFFFUL);
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

uint32_t HeathrowIC::dma_read(uint32_t offset, int size) {
    uint32_t value;
    int dma_channel = offset >> 8;
    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma)
            value = this->mesh_dma->reg_read(offset & 0xFF, size);
        else
            value = 0;
        break;
    case MIO_OHARE_DMA_FLOPPY:
        value = this->floppy_dma->reg_read(offset & 0xFF, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        //value = this->enet_xmit_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        //value = this->enet_rcv_dma->reg_read(offset & 0xFF, size);
        value = 0;
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        value = this->snd_out_dma->reg_read(offset & 0xFF, size);
        break;
    default:
        if (!(unsupported_dma_channel_read & (1 << dma_channel))) {
            unsupported_dma_channel_read |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s read  @%02x.%c", this->name.c_str(),
                  dma_channel, get_name_dma(dma_channel), offset & 0xFF, SIZE_ARG(size));
            return 0;
        }
        value = 0;
    }
    LOG_F(DBDMA, "read  %s @%02x.%c = %0*x", get_name_dma(dma_channel),
          offset & 0xFF, SIZE_ARG(size), size * 2, value);
    return value;
}

void HeathrowIC::dma_write(uint32_t offset, uint32_t value, int size) {
    int dma_channel = offset >> 8;

    LOG_F(DBDMA, "write %s @%02x.%c = %0*x", get_name_dma(dma_channel),
          offset & 0xFF, SIZE_ARG(size), size * 2, value);

    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        if (this->mesh_dma) this->mesh_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        //this->enet_xmit_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        //this->enet_rcv_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        if (!(unsupported_dma_channel_write & (1 << dma_channel))) {
            unsupported_dma_channel_write |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s write @%02x.%c = %0*x", this->name.c_str(),
                  dma_channel, get_name_dma(dma_channel), offset & 0xFF, SIZE_ARG(size), size * 2, value);
        }
    }
}

void HeathrowIC::feature_control(const uint32_t value)
{
    LOG_F(9, "write %x to MIO:Feat_Ctrl register", value);

    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "%s: Monitor sense enabled", this->get_name().c_str());
    } else {
        LOG_F(9, "%s: Monitor sense disabled", this->get_name().c_str());
    }
}

#define FIRST_INT1_BIT 12 // The first ten are DMA, the next 2 appear to be unused. We'll map 1:1 the INT1 bits 31..12 (0x1F..0x0C) as IRQ_ID bits.
#define FIRST_INT2_BIT  2 // Skip the first two which are Ethernet DMA. We'll map INT2 bits 13..2 (interrupts 45..34 or 0x2D..0x22) as IRQ_ID bits 11..0.
#define FIRST_INT1_IRQ_ID_BIT 12 // Same as INT1_BIT so there won't be any shifting required.
#define FIRST_INT2_IRQ_ID_BIT  0

#define INT1_TO_IRQ_ID(int1) (1 << (int1 - FIRST_INT1_BIT + FIRST_INT1_IRQ_ID_BIT))
#define INT2_TO_IRQ_ID(int2) (1 << (int2 - FIRST_INT2_BIT + FIRST_INT2_IRQ_ID_BIT - 32))
#define INT_TO_IRQ_ID(intx) (intx < 32 ? INT1_TO_IRQ_ID(intx) : INT2_TO_IRQ_ID(intx))

#define IS_INT1(irq_id) (irq_id >= 1 << FIRST_INT1_IRQ_ID_BIT)
#define IRQ_ID_TO_INT1_MASK(irq_id) (irq_id <<= (FIRST_INT1_BIT - FIRST_INT1_IRQ_ID_BIT))
#define IRQ_ID_TO_INT2_MASK(irq_id) (irq_id <<= (FIRST_INT2_BIT - FIRST_INT2_IRQ_ID_BIT))

uint32_t HeathrowIC::register_dev_int(IntSrc src_id)
{
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

    case IntSrc::PERCH2     : return INT_TO_IRQ_ID(0x15);
    case IntSrc::PCI_GPU    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_CARDBUS: return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_B      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_E      : return INT_TO_IRQ_ID(0x18); // Lombard GPU
    case IntSrc::PCI_C      : return INT_TO_IRQ_ID(0x19);
    case IntSrc::PERCH1     : return INT_TO_IRQ_ID(0x1A);
    case IntSrc::PCI_PERCH  : return INT_TO_IRQ_ID(0x1C);

    case IntSrc::FIREWIRE   : return INT_TO_IRQ_ID(0x15); // Yosemite built-in PCI FireWire
    case IntSrc::PCI_J12    : return INT_TO_IRQ_ID(0x16); // Yosemite 32-bit 66MHz slot for GPU
    case IntSrc::PCI_J11    : return INT_TO_IRQ_ID(0x17); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J10    : return INT_TO_IRQ_ID(0x18); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J9     : return INT_TO_IRQ_ID(0x19); // Yosemite 64-bit 33MHz slot
    case IntSrc::ATA        : return INT_TO_IRQ_ID(0x1A); // Yosemite PCI pci-ata
    case IntSrc::ZIVA       : return INT_TO_IRQ_ID(0x1A); // Lombard ZiVA DVD Decoder
    case IntSrc::USB        : return INT_TO_IRQ_ID(0x1C); // Yosemite/Lombard PCI usb
    case IntSrc::MEDIA_BAY  : return INT_TO_IRQ_ID(0x1D); // Lombard

    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x2A);

    default:
        ABORT_F("%s: unknown interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

#define FIRST_DMA_INT1_BIT  0 // bit 0 is SCSI DMA
#define FIRST_DMA_INT2_BIT  0 // bit 0 is Ethernet DMA Tx
#define FIRST_DMA_INT1_IRQ_ID_BIT  0
#define FIRST_DMA_INT2_IRQ_ID_BIT 16 // There's only 10 INT1 DMA bits but we'll put INT2 DMA bits in the upper 16 bits

#define DMA_INT1_TO_IRQ_ID(int1) (1 << (int1 - FIRST_DMA_INT1_BIT + FIRST_DMA_INT1_IRQ_ID_BIT))
#define DMA_INT2_TO_IRQ_ID(int2) (1 << (int2 - FIRST_DMA_INT2_BIT + FIRST_DMA_INT2_IRQ_ID_BIT - 32))
#define DMA_INT_TO_IRQ_ID(intx) (intx < 32 ? DMA_INT1_TO_IRQ_ID(intx) : DMA_INT2_TO_IRQ_ID(intx))

#define IS_DMA_INT1(irq_id) (irq_id < 1 << FIRST_DMA_INT2_IRQ_ID_BIT)
#define DMA_IRQ_ID_TO_INT1_MASK(irq_id) (irq_id <<= (FIRST_DMA_INT1_BIT - FIRST_DMA_INT1_IRQ_ID_BIT))
#define DMA_IRQ_ID_TO_INT2_MASK(irq_id) (irq_id >>= (FIRST_DMA_INT2_IRQ_ID_BIT - FIRST_DMA_INT2_BIT))

uint32_t HeathrowIC::register_dma_int(IntSrc src_id)
{
    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH      : return DMA_INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_SWIM3          : return DMA_INT_TO_IRQ_ID(0x01);
    case IntSrc::DMA_IDE0           : return DMA_INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_IDE1           : return DMA_INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_SCCA_Tx        : return DMA_INT_TO_IRQ_ID(0x04);
    case IntSrc::DMA_SCCA_Rx        : return DMA_INT_TO_IRQ_ID(0x05);
    case IntSrc::DMA_SCCB_Tx        : return DMA_INT_TO_IRQ_ID(0x06);
    case IntSrc::DMA_SCCB_Rx        : return DMA_INT_TO_IRQ_ID(0x07);
    case IntSrc::DMA_DAVBUS_Tx      : return DMA_INT_TO_IRQ_ID(0x08);
    case IntSrc::DMA_DAVBUS_Rx      : return DMA_INT_TO_IRQ_ID(0x09);
    case IntSrc::DMA_ETHERNET_Tx    : return DMA_INT_TO_IRQ_ID(0x20);
    case IntSrc::DMA_ETHERNET_Rx    : return DMA_INT_TO_IRQ_ID(0x21);
    default:
        ABORT_F("%s: unknown DMA interrupt source %d", this->name.c_str(), src_id);
    }
    return 0;
}

void HeathrowIC::ack_int(uint32_t irq_id, uint8_t irq_line_state)
{
#if 1
    if (!IS_INT1(irq_id)) { // does this irq_id belong to the second set?
        IRQ_ID_TO_INT2_MASK(irq_id);
#if 0
        LOG_F(INFO, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change2:%08x state:%d",
            this->name.c_str(), this->int_events1 + 0, this->int_events2 + 0, this->int_levels1 + 0, this->int_levels2 + 0, irq_id, irq_line_state
        );
#endif
        // native mode:   set IRQ bits in int_events2 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events2 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels2 & irq_id))) {
            this->int_events2 |= irq_id;
        } else {
            this->int_events2 &= ~irq_id;
        }
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels2 |= irq_id;
        } else {
            this->int_levels2 &= ~irq_id;
        }
    } else {
        IRQ_ID_TO_INT1_MASK(irq_id);
        // native mode:   set IRQ bits in int_events1 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events1 on all transitions
#if 0
        LOG_F(INFO, "%s: native interrupt events:%08x.%08x levels:%08x.%08x change1:%08x state:%d",
            this->name.c_str(), this->int_events1 + 0, this->int_events2 + 0, this->int_levels1 + 0, this->int_levels2 + 0, irq_id, irq_line_state);
#endif
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels1 & irq_id))) {
            this->int_events1 |= irq_id;
        } else {
            this->int_events1 &= ~irq_id;
        }
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels1 |= irq_id;
        } else {
            this->int_levels1 &= ~irq_id;
        }
    }

    this->signal_cpu_int();
#endif

#if 0
    if (this->int_mask1 & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        if (!IS_INT1(irq_id)) {
            IRQ_ID_TO_INT2_MASK(irq_id);
            this->int_events2 |= irq_id; // signal IRQ line change
            this->int_events2 &= this->int_mask2;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels2 |= irq_id;
            } else {
                this->int_levels2 &= ~irq_id;
            }
        } else {
            IRQ_ID_TO_INT1_MASK(irq_id);
            this->int_events1 |= irq_id; // signal IRQ line change
            this->int_events1 &= this->int_mask1;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels1 |= irq_id;
            } else {
                this->int_levels1 &= ~irq_id;
            }
        }
        this->signal_cpu_int();
    } else {
        LOG_F(WARNING, "%s: native interrupt mode not implemented", this->name.c_str());
    }
#endif
}

void HeathrowIC::ack_dma_int(uint32_t irq_id, uint8_t irq_line_state)
{
#if 1
    if (!IS_DMA_INT1(irq_id)) {
        DMA_IRQ_ID_TO_INT2_MASK(irq_id);
        // native mode:   set IRQ bits in int_events2 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events2 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels2 & irq_id))) {
            this->int_events2 |= irq_id;
        } else {
            this->int_events2 &= ~irq_id;
        }
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels2 |= irq_id;
        } else {
            this->int_levels2 &= ~irq_id;
        }
    } else {
        DMA_IRQ_ID_TO_INT1_MASK(irq_id);
        // native mode:   set IRQ bits in int_events1 on a 0-to-1 transition
        // emulated mode: set IRQ bits in int_events1 on all transitions
        if ((this->int_mask1 & MACIO_INT_MODE) ||
            (irq_line_state && !(this->int_levels1 & irq_id))) {
            this->int_events1 |= irq_id;
        } else {
            this->int_events1 &= ~irq_id;
        }
        // update IRQ line state
        if (irq_line_state) {
            this->int_levels1 |= irq_id;
        } else {
            this->int_levels1 &= ~irq_id;
        }
    }

    this->signal_cpu_int();
#endif

#if 0
    if (this->int_mask1 & MACIO_INT_MODE) { // 68k interrupt emulation mode?
        if (!IS_DMA_INT1(irq_id)) {
            DMA_IRQ_ID_TO_INT2_MASK(irq_id);
            this->int_events2 |= irq_id; // signal IRQ line change
            this->int_events2 &= this->int_mask2;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels2 |= irq_id;
            } else {
                this->int_levels2 &= ~irq_id;
            }
        } else {
            DMA_IRQ_ID_TO_INT1_MASK(irq_id);
            this->int_events1 |= irq_id; // signal IRQ line change
            this->int_events1 &= this->int_mask1;
            // update IRQ line state
            if (irq_line_state) {
                this->int_levels1 |= irq_id;
            } else {
                this->int_levels1 &= ~irq_id;
            }
        }
        this->signal_cpu_int();
    } else {
        ABORT_F("%s: native interrupt mode not implemented", this->name.c_str());
    }
#endif
}

void HeathrowIC::signal_cpu_int() {
    if ((this->int_events1 & this->int_mask1) || (this->int_events2 & this->int_mask2)) {
        if (!this->cpu_int_latch) {
            this->cpu_int_latch = true;
            ppc_assert_int();
        } else {
            LOG_F(5, "%s: CPU INT already latched", this->name.c_str());
        }
    }
}

void HeathrowIC::clear_cpu_int()
{
    if (!(this->int_events1 & this->int_mask1) && !(this->int_events2 & this->int_mask2) &&
        this->cpu_int_latch) {
        this->cpu_int_latch = false;
        ppc_release_int();
        LOG_F(5, "%s: CPU INT latch cleared", this->name.c_str());
    }
}

static const vector<string> Heathrow_Subdevices = {
    "NVRAM@60000", "ViaCuda@16000", "MeshHeathrow@10000", "Escc@13000", "Swim3@15000", "Ide0@20000", "Ide1@21000",
    "BigMacHeathrow@11000"
};

static const DeviceDescription Heathrow_Descriptor = {
    HeathrowIC::create, Heathrow_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

REGISTER_DEVICE(Heathrow, Heathrow_Descriptor);
