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

/** MacIO device family emulation

    Mac I/O (MIO) is a family of ASICs to bring support for Apple legacy
    I/O hardware to the PCI-based Power Macintosh. That legacy hardware has
    existed long before Power Macintosh was introduced. It includes:
    - versatile interface adapter (VIA)
    - Sander-Woz integrated machine (SWIM) that is a floppy disk controller
    - CUDA MCU for ADB, parameter RAM, realtime clock and power management support
    - serial communication controller (SCC)
    - Macintosh Enhanced SCSI Hardware (MESH)

    In the 68k Macintosh era, all this hardware was implemented using several
    custom chips. In a PCI-compatible Power Macintosh, the above devices are part
    of the MIO chip itself. MIO's functional blocks implementing virtual devices
    are called "cells", i.e. "VIA cell", "SWIM cell" etc.

    MIO itself is PCI compliant while the legacy hardware it emulates isn't.
    MIO occupies 512Kb of the PCI memory space divided into registers space and
    DMA space. Access to emulated legacy devices is accomplished by reading from/
    writing to MIO's PCI address space at predefined offsets.

    MIO includes a DMA controller that offers up to 12 DMA channels implementing
    Apple's own DMA protocol called descriptor-based DMA (DBDMA).

    Official documentation (that is somewhat incomplete and erroneous) can be
    found in the second chapter of the book "Macintosh Technology in the Common
    Hardware Reference Platform" by Apple Computer, Inc.
*/

#ifndef MACIO_H
#define MACIO_H

#include <devices/common/ata/idechannel.h>
#include <devices/common/dbdma.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/nvram.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/scsi/mesh.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/common/viacuda.h>
#include <devices/ethernet/bigmac.h>
#include <devices/ethernet/mace.h>
#include <devices/floppy/swim3.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/serial/escc.h>
#include <devices/sound/awacs.h>

#include <cinttypes>
#include <memory>
#include <string>

/** PCI device IDs for various MacIO ASICs. */
enum {
    MIO_DEV_ID_GRANDCENTRAL = 0x0002,
    MIO_DEV_ID_OHARE        = 0x0007,
    MIO_DEV_ID_HEATHROW     = 0x0010,
    MIO_DEV_ID_PADDINGTON   = 0x0017,
};

/** Interrupt related constants. */
#define MACIO_INT_CLR    0x80UL       // clears bits in the interrupt events registers
#define MACIO_INT_MODE   0x80000000UL // interrupt mode: 0 - native, 1 - 68k-style

#define INT_TO_IRQ_ID(intx) (1ULL << intx)

/** Offsets to common MacIO interrupt registers. */
enum {
    MIO_INT_EVENTS2 = 0x10, // Heathrow/Paddington only
    MIO_INT_MASK2   = 0x14, // Heathrow/Paddington only
    MIO_INT_CLEAR2  = 0x18, // Heathrow/Paddington only
    MIO_INT_LEVELS2 = 0x1C, // Heathrow/Paddington only
    MIO_INT_EVENTS1 = 0x20,
    MIO_INT_MASK1   = 0x24,
    MIO_INT_CLEAR1  = 0x28,
    MIO_INT_LEVELS1 = 0x2C
};

/** GrandCentral DBDMA channels. */
enum : uint8_t {
    MIO_GC_DMA_SCSI_CURIO    = 0,
    MIO_GC_DMA_FLOPPY        = 1,
    MIO_GC_DMA_ETH_XMIT      = 2,
    MIO_GC_DMA_ETH_RCV       = 3,
    MIO_GC_DMA_ESCC_A_XMIT   = 4,
    MIO_GC_DMA_ESCC_A_RCV    = 5,
    MIO_GC_DMA_ESCC_B_XMIT   = 6,
    MIO_GC_DMA_ESCC_B_RCV    = 7,
    MIO_GC_DMA_AUDIO_OUT     = 8,
    MIO_GC_DMA_AUDIO_IN      = 9,
    MIO_GC_DMA_SCSI_MESH     = 0xA,
};

class IobusDevice : virtual public HWComponent {
public:
    virtual uint16_t iodev_read(uint32_t address) = 0;
    virtual void iodev_write(uint32_t address, uint16_t value) = 0;
};

class NvramAddrHiDev: public IobusDevice {
public:
    NvramAddrHiDev() : HWComponent("NvramAddrHiDev") {}
    // IobusDevice methods
    uint16_t iodev_read(uint32_t /*address*/) {
        return nvram_addr_hi;
    }

    void iodev_write(uint32_t /*address*/, uint16_t value) {
        this->nvram_addr_hi = value;
    }

protected:
    uint16_t    nvram_addr_hi = 0;
};

class NvramDev: public IobusDevice {
public:
    NvramDev(NvramAddrHiDev *addr_hi);

    // IobusDevice methods
    uint16_t iodev_read(uint32_t address) {
        return this->nvram->read_byte((addr_hi->iodev_read(0) << 5) + address);
    }

    void iodev_write(uint32_t address, uint16_t value) {
        this->nvram->write_byte((addr_hi->iodev_read(0) << 5) + address, value);
    }

protected:
    NVram* nvram = nullptr;
    NvramAddrHiDev* addr_hi = nullptr;
};

/** This class provides common building blocks for various MacIO ASICs. */
class MacIoBase : public PCIDevice, public InterruptCtrl {
public:
    MacIoBase(std::string name, uint16_t dev_id, uint8_t rev=1);
    ~MacIoBase() = default;

    uint64_t register_dma_int(IntSrc src_id) override;
    void ack_int(uint64_t irq_id, uint8_t irq_line_state) override;
    void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) override;

protected:
    void notify_bar_change(int bar_num);
    void ack_int_common(uint64_t irq_id, uint8_t irq_line_state);
    void signal_cpu_int();
    void clear_cpu_int();

    // PCI device state
    uint32_t    iomem_size = 0;
    uint32_t    base_addr  = 0;

    // interrupt state
    uint64_t int_mask      = 0;
    bool     cpu_int_latch = false;
    std::atomic<uint64_t> int_levels{0};
    std::atomic<uint64_t> int_events{0};

    // Subdevice objects
    ViaCuda*            viacuda;   // VIA cell with Cuda MCU attached to it
    Swim3::Swim3Ctrl*   swim3;     // floppy disk controller
    MacioSndCodec*      snd_codec; // audio codec instance
    EsccController*     escc;      // ESCC serial controller

    // DMA channels
    std::unique_ptr<DMAChannel>     floppy_dma;
    std::unique_ptr<DMAChannel>     snd_out_dma;
    std::unique_ptr<DMAChannel>     snd_in_dma;
    std::unique_ptr<DMAChannel>     escc_a_tx_dma;
    std::unique_ptr<DMAChannel>     escc_a_rx_dma;
    std::unique_ptr<DMAChannel>     escc_b_tx_dma;
    std::unique_ptr<DMAChannel>     escc_b_rx_dma;
};

class GrandCentral : public PCIDevice, public InterruptCtrl {
public:
    GrandCentral(const std::string name);
    ~GrandCentral() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<GrandCentral>(new GrandCentral(dev_name));
    }

    // HWComponent methods

    virtual HWComponent* add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name = "") override;

    // MMIO device methods

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // InterruptCtrl methods

    uint64_t register_dev_int(IntSrc src_id) override;
    uint64_t register_dma_int(IntSrc src_id) override;
    void ack_int(uint64_t irq_id, uint8_t irq_line_state) override;
    void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) override;
    IntSrc irq_id_to_src(uint64_t irq_id) override;

protected:
    void notify_bar_change(int bar_num);
    void ack_int_common(uint64_t irq_id, uint8_t irq_line_state);
    void signal_cpu_int(uint64_t irq_id);
    void clear_cpu_int();

private:
    void attach_iodevice(int dev_num, IobusDevice* dev_obj);

    uint32_t    base_addr = 0;

    // interrupt state
    uint32_t    int_mask      = 0;
    std::atomic<uint32_t>    int_levels{0};
    std::atomic<uint32_t>    int_events{0};
    bool        cpu_int_latch = false;

    // IOBus devices
    IobusDevice*     iobus_devs[6] = { nullptr };
    NvramAddrHiDev*  nvram_addr_hi_dev = nullptr;
    NvramDev*        nvram_dev = nullptr;

    // subdevice objects
    AwacsScreamer*      awacs;   // AWACS audio codec instance
    MeshStub*           mesh_stub;

    MaceController*     mace;       // Ethernet cell within Curio
    ViaCuda*            viacuda;    // VIA cell with Cuda MCU attached to it
    EsccController*     escc;       // ESCC serial controller cell within Curio
    MeshBase*           mesh;       // internal SCSI (fast)
    Sc53C94*            curio;      // external SCSI (slow)
    Swim3::Swim3Ctrl*   swim3;      // floppy disk controller

    std::unique_ptr<DMAChannel>     curio_dma;
    std::unique_ptr<DMAChannel>     mesh_dma = nullptr;
    std::unique_ptr<DMAChannel>     snd_out_dma;
    std::unique_ptr<DMAChannel>     snd_in_dma;
    std::unique_ptr<DMAChannel>     floppy_dma;
    std::unique_ptr<DMAChannel>     enet_tx_dma;
    std::unique_ptr<DMAChannel>     enet_rx_dma;
    std::unique_ptr<DMAChannel>     escc_a_tx_dma;
    std::unique_ptr<DMAChannel>     escc_a_rx_dma;
    std::unique_ptr<DMAChannel>     escc_b_tx_dma;
    std::unique_ptr<DMAChannel>     escc_b_rx_dma;

    uint16_t unsupported_dma_channel_read = 0;
    uint16_t unsupported_dma_channel_write = 0;
};

/** O'Hare/Heathrow specific registers. */
enum {
    MIO_OHARE_ID        = 0x34, // IDs register (MIO_HEAT_ID)
    MIO_OHARE_FEAT_CTRL = 0x38, // feature control register
    MIO_AUX_CTRL        = 0x3C,
};

/** MIO_OHARE_FEAT_CTRL bits. */
enum {
    MIO_OH_FC_IN_USE_LED               = 1 <<  0, // modem serial port in use in Open Firmware
                                                  // controls display sense on Beige G3 desktop
    MIO_OH_FC_NOT_MB_PWR               = 1 <<  1,
    MIO_OH_FC_PCI_MB_EN                = 1 <<  2,
    MIO_OH_FC_IDE_MB_EN                = 1 <<  3,
    MIO_OH_FC_FLOPPY_EN                = 1 <<  4,
    MIO_OH_FC_IDE_INT_EN               = 1 <<  5,
    MIO_OH_FC_NOT_IDE0_RESET           = 1 <<  6,
    MIO_OH_FC_NOT_MB_RESET             = 1 <<  7,
    MIO_OH_FC_IOBUS_EN                 = 1 <<  8,
    MIO_OH_FC_SCC_CELL_EN              = 1 <<  9,
    MIO_OH_FC_SCSI_CELL_EN             = 1 << 10,
    MIO_OH_FC_SWIM_CELL_EN             = 1 << 11,
    MIO_OH_FC_SND_PWR                  = 1 << 12,
    MIO_OH_FC_SND_CLK_EN               = 1 << 13,
    MIO_OH_FC_SCC_A_ENABLE             = 1 << 14,
    MIO_OH_FC_SCC_B_ENABLE             = 1 << 15,
    MIO_OH_FC_NOT_PORT_VIA_DESKTOP_VIA = 1 << 16,
    MIO_OH_FC_NOT_PWM_MON_ID           = 1 << 17,
    MIO_OH_FC_NOT_HOOKPB_MB_CNT        = 1 << 18,
    MIO_OH_FC_NOT_SWIM3_CLONEFLOPPY    = 1 << 19,
    MIO_OH_FC_AUD22RUN                 = 1 << 20,
    MIO_OH_FC_SCSI_LINKMODE            = 1 << 21,
    MIO_OH_FC_ARB_BYPASS               = 1 << 22,
    MIO_OH_FC_NOT_IDE1_RESET           = 1 << 23,
    MIO_OH_FC_SLOW_SCC_PCLK            = 1 << 24,
    MIO_OH_FC_RESET_SCC                = 1 << 25,
    MIO_OH_FC_MFDC_CELL_EN             = 1 << 26, // Heathrow/Paddington only
    MIO_OH_FC_USE_MFDC                 = 1 << 27, // Heathrow/Paddington only
    MIO_OH_FC_RESVD28                  = 1 << 28,
    MIO_OH_FC_RESVD29                  = 1 << 29,
    MIO_OH_FC_RESVD30                  = 1 << 30,
    MIO_OH_FC_RESVD31                  = 1 << 31,
};

/** O'Hare/Heathrow DBDMA channels. */
enum : uint8_t {
    MIO_OHARE_DMA_MESH          = 0,
    MIO_OHARE_DMA_FLOPPY        = 1,
    MIO_OHARE_DMA_ETH_XMIT      = 2,
    MIO_OHARE_DMA_ETH_RCV       = 3,
    MIO_OHARE_DMA_ESCC_A_XMIT   = 4,
    MIO_OHARE_DMA_ESCC_A_RCV    = 5,
    MIO_OHARE_DMA_ESCC_B_XMIT   = 6,
    MIO_OHARE_DMA_ESCC_B_RCV    = 7,
    MIO_OHARE_DMA_AUDIO_OUT     = 8,
    MIO_OHARE_DMA_AUDIO_IN      = 9,
    MIO_OHARE_DMA_IDE0          = 0xB,
    MIO_OHARE_DMA_IDE1          = 0xC
};

/** Implementation class for O'Hare/Heathrow/Paddington devices. */
class MacIoTwo : public MacIoBase {
public:
    MacIoTwo(std::string name, uint16_t dev_id);
    ~MacIoTwo() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        if (dev_name == "OHare"     ) return std::unique_ptr<MacIoTwo>(new MacIoTwo(dev_name, MIO_DEV_ID_OHARE));
        if (dev_name == "Heathrow"  ) return std::unique_ptr<MacIoTwo>(new MacIoTwo(dev_name, MIO_DEV_ID_HEATHROW));
        if (dev_name == "Paddington") return std::unique_ptr<MacIoTwo>(new MacIoTwo(dev_name, MIO_DEV_ID_PADDINGTON));
    }

    void set_fp_id(uint8_t id) {
        this->fp_id = id;
    }

    void set_mon_id(uint8_t id) {
        this->mon_id = id;
    }

    void set_media_bay_id(uint8_t id) {
        this->mb_id = id;
    }

    void set_cpu_id(uint8_t id) {
        this->cpu_id = id;
    }

    void set_emmo_mask(uint32_t mask) {
        this->emmo_mask = mask;
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // InterruptCtrl methods
    uint64_t register_dev_int(IntSrc src_id) override;
    uint64_t register_dma_int(IntSrc src_id) override;
    IntSrc irq_id_to_src(uint64_t irq_id) override;

protected:
    uint32_t dma_read(uint32_t offset, int size);
    void dma_write(uint32_t offset, uint32_t value, int size);

    uint32_t mio_ctrl_read(uint32_t offset, int size);
    uint32_t mio_ctrl_read_aligned(uint32_t offset);
    void mio_ctrl_write(uint32_t offset, uint32_t value, int size);

    void feature_control(uint32_t value);

private:
    uint32_t feat_ctrl     = 0;    // features control register
    uint32_t aux_ctrl      = 0;    // aux features control register

    uint8_t  cpu_id = 0xE0; // CPUID field (LSB of the MIO_HEAT_ID)
    uint8_t  mb_id  = 0x70; // Media Bay ID (bits 15:8 of the MIO_HEAT_ID)
    uint8_t  mon_id = 0x10; // Monitor ID (bits 23:16 of the MIO_HEAT_ID)
    uint8_t  fp_id  = 0x70; // Flat panel ID (MSB of the MIO_HEAT_ID)

    // 70 10 70 E0 = Beige G3 Desktop
    // 7A 10 30 E0 = 6500/225
    // 70 10 20 A0 = B&W G3

    uint8_t  emmo      = 0x01; // factory tester status, active low
    uint32_t emmo_mask = 0x00000010;

    // Subdevice object pointers
    NVram*              nvram;    // NVRAM
    MeshController*     mesh;     // MESH SCSI cell instance
    IdeChannel*         ide_0;    // Internal ATA
    IdeChannel*         ide_1;    // Media Bay ATA
    BigMac*             bmac;     // Ethernet MAC cell

    // DMA channels
    std::unique_ptr<DMAChannel>     mesh_dma;
    std::unique_ptr<DMAChannel>     enet_xmit_dma;
    std::unique_ptr<DMAChannel>     enet_rcv_dma;
    std::unique_ptr<DMAChannel>     ide0_dma;
    std::unique_ptr<DMAChannel>     ide1_dma;

    uint16_t unsupported_dma_channel_read = 0;
    uint16_t unsupported_dma_channel_write = 0;
};

#endif /* MACIO_H */
