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

#include <cpu/ppc/ppcmmu.h>
#include <devices/common/usb/usbohci.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <map>

// Host Controller Operational Registers
enum {
    HcRevision            = 0x00,
    HcControl             = 0x04,
    HcCommandStatus       = 0x08,
    HcInterruptStatus     = 0x0C,
    HcInterruptEnable     = 0x10,
    HcInterruptDisable    = 0x14,
    HcHCCA                = 0x18,
    HcPeriodCurrentED     = 0x1C,
    HcControlHeadED       = 0x20,
    HcControlCurrentED    = 0x24,
    HcBulkHeadED          = 0x28,
    HcBulkCurrentED       = 0x2C,
    HcDoneHead            = 0x30,
    HcFmInterval          = 0x34,
    HcFmRemaining         = 0x38,
    HcFmNumber            = 0x3C,
    HcPeriodicStart       = 0x40,
    HcLSThreshold         = 0x44,
    HcRhDescriptorA       = 0x48,
    HcRhDescriptorB       = 0x4C,
    HcRhStatus            = 0x50,
    HcRhPortStatus        = 0x54,
};

/* Human readable OHCI HW register names for easier debugging. */
static const std::map<uint16_t, std::string> usbohci_reg_names = {
    {0x00, "HcRevision"},
    {0x04, "HcControl"},
    {0x08, "HcCommandStatus"},
    {0x0C, "HcInterruptStatus"},
    {0x10, "HcInterruptEnable"},
    {0x14, "HcInterruptDisable"},
    {0x18, "HcHCCA"},
    {0x1C, "HcPeriodCurrentED"},
    {0x20, "HcControlHeadED"},
    {0x24, "HcControlCurrentED"},
    {0x28, "HcBulkHeadED"},
    {0x2C, "HcBulkCurrentED"},
    {0x30, "HcDoneHead"},
    {0x34, "HcFmInterval"},
    {0x38, "HcFmRemaining"},
    {0x3C, "HcFmNumber"},
    {0x40, "HcPeriodicStart"},
    {0x44, "HcLSThreshold"},
    {0x48, "HcRhDescriptorA"},
    {0x4C, "HcRhDescriptorB"},
    {0x50, "HcRhStatus"},
    {0x54, "HcRhPortStatus#1"},
    {0x58, "HcRhPortStatus#2"},
    {0x5C, "HcRhPortStatus#3"},
    {0x60, "HcRhPortStatus#4"},
    {0x64, "HcRhPortStatus#5"},
    {0x68, "HcRhPortStatus#6"},
    {0x6C, "HcRhPortStatus#7"},
    {0x70, "HcRhPortStatus#8"},
    {0x74, "HcRhPortStatus#9"},
    {0x78, "HcRhPortStatus#10"},
    {0x7C, "HcRhPortStatus#11"},
    {0x80, "HcRhPortStatus#12"},
    {0x84, "HcRhPortStatus#13"},
    {0x88, "HcRhPortStatus#14"},
    {0x8C, "HcRhPortStatus#15"},
};

static const char* get_reg_name(uint32_t reg_offset) {
    auto iter = usbohci_reg_names.find(reg_offset & ~3);
    if (iter != usbohci_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown USB OHCI register";
    }
}

static const char *get_state_name(HCFS_t v) {
    switch (v) {
        case HCFS_UsbReset       : return "UsbReset";
        case HCFS_UsbResume      : return "UsbResume";
        case HCFS_UsbOperational : return "UsbOperational";
        case HCFS_UsbSuspend     : return "UsbSuspend";
    }
}

static const char *get_routing_name(InterruptRouting_t v) {
    switch (v) {
        case IR_HostBus : return "HostBus";
        case IR_SMI     : return "SMI";
    }
}

static const char *get_power_switching_mode_name(PowerSwitchingMode_t v) {
    switch (v) {
        case PSM_AllPorts : return "AllPorts";
        case PSM_PerPort  : return "PerPort";
    }
}

static const char *get_no_power_switching_name(NoPowerSwitching_t v) {
    switch (v) {
        case NPS_PowerSwitched   : return "PowerSwitched";
        case NPS_AlwaysPoweredOn : return "AlwaysPoweredOn";
    }
}

static const char *get_device_type_name(DeviceType_t v) {
    switch (v) {
        case DT_NotACompoundDevice : return "NotACompoundDevice";
        case DT_CompoundDevice     : return "CompoundDevice";
    }
}

static const char *get_over_current_protection_mode_name(OverCurrentProtectionMode_t v) {
    switch (v) {
        case OCM_AllPorts : return "AllPorts";
        case OCM_PerPort  : return "PerPort";
    }
}

static const char *get_no_over_current_protection_name(NoOverCurrentProtection_t v) {
    switch (v) {
        case NOCP_OverCurrentProtected     : return "OverCurrentProtected";
        case NOCP_NotOverCurrentProtected  : return "NotOverCurrentProtected";
    }
}

static uint8_t* mmu_get_dma_mem(uint32_t addr, uint32_t size) {
    MapDmaResult res = mmu_map_dma_mem(addr, size, false);
    return res.host_va;
}

USBHostOHCI::USBHostOHCI(const std::string name)
    : PCIDevice(name), HWComponent(name)
{
    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    // set up PCI configuration space header
    /* 06 */ this->status         = 0x0200; // 9:DEVSEL medium
    /* 08 */ this->class_rev      = 0x0C0310 << 8; // OHCI USB Controller
    /* 0C */ this->cache_ln_sz    = 0x08; // 8 DWORDS = 32 bytes
    /* 10 */ this->bars_cfg[0]    = (uint32_t)(-0x1000); // 4K but only the first 0x90 bytes are meaningful
    /* 3D */ this->irq_pin        = 0x01; // 01=pin A
    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };
    this->finish_config_bars();

    this->HcOp.HcRevision.Revision = 0x10;
    HcOp.HcControl.HostControllerFunctionalState = HCFS_UsbReset;
}

void USBHostOHCI::change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture) {
            HardwareReset(); // init some stuff

            // the first thing the driver does is Suspend but you can't Suspend from Reset
            // so we do SoftwareReset to achieve Suspend.
            SoftwareReset(); 

            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);
        }

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

void USBHostOHCI::notify_bar_change(int bar_num)
{
    switch (bar_num) {
        case 0: change_one_bar(this->aperture_base, 0x1000, this->bars[bar_num] & ~15, bar_num); break;
    }
}

uint32_t USBHostOHCI::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
{
    uint32_t value = PCIDevice::pci_cfg_read(reg_offs, details);
    return value;
}

void USBHostOHCI::pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details)
{
    if (reg_offs < 64) {
        if (reg_offs >= 4 && reg_offs < 8) {
            LOG_WRITE_NAMED_CONFIG_REGISTER("command/status");
        }
        else {
            LOG_WRITE_NAMED_CONFIG_REGISTER("        config");
        }
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }
    LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
}

uint32_t USBHostOHCI::read_hcop_reg(uint32_t offset)
{
    uint32_t value = 0;
    switch (offset) {
        case HcRevision         : value = this->HcOp.HcRevision.val        ; break;
        case HcControl          : value = this->HcOp.HcControl.val         ; break;
        case HcCommandStatus    : value = this->HcOp.HcCommandStatus.val   ; break;
        case HcInterruptStatus  : value = this->HcOp.HcInterruptStatus.val ; break;
        case HcInterruptEnable  : value = this->HcOp.HcInterruptEnable.val ; break;
        case HcInterruptDisable : value = this->HcOp.HcInterruptEnable.val ; break; // reads HcInterruptEnable
        case HcHCCA             : value = this->HcOp.HcHCCA                ; break;
        case HcPeriodCurrentED  : value = this->HcOp.HcPeriodCurrentED     ; break;
        case HcControlHeadED    : value = this->HcOp.HcControlHeadED       ; break;
        case HcControlCurrentED : value = this->HcOp.HcControlCurrentED    ; break;
        case HcBulkHeadED       : value = this->HcOp.HcBulkHeadED          ; break;
        case HcBulkCurrentED    : value = this->HcOp.HcBulkCurrentED       ; break;
        case HcDoneHead         : value = this->HcOp.HcDoneHead            ; break;
        case HcFmInterval       : value = this->HcOp.HcFmInterval.val      ; break;
        case HcFmRemaining      : value = this->HcOp.HcFmRemaining.val     ; break;
        case HcFmNumber         : value = this->HcOp.HcFmNumber.val        ; break;
        case HcPeriodicStart    : value = this->HcOp.HcPeriodicStart.val   ; break;
        case HcLSThreshold      : value = this->HcOp.HcLSThreshold.val     ; break;
        case HcRhDescriptorA    : value = this->HcOp.HcRhDescriptorA.val   ; break;
        case HcRhDescriptorB    : value = this->HcOp.HcRhDescriptorB.val   ; break;
        case HcRhStatus         : value = this->HcOp.HcRhStatus.val        ; break;
        default:
            int port = (offset - HcRhPortStatus) / 4;
            if (port < 15) {
                value = this->HcOp.HcRhPortStatus[port].val; break;
            }
    }
    return value;
}

uint32_t USBHostOHCI::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t value = 0;
    uint32_t value2 = 0;
    if (rgn_start == this->aperture_base && offset < 0x1000) {
        value = this->read_hcop_reg(offset & ~3);
        if ((offset & 3) + size > 4) {
            value2 = this->read_hcop_reg((offset & ~3) + 4);
        }
        AccessDetails details;
        ACCESSDETAILS_SET(details, size, offset, 0);
        value = conv_rd_data(value, value2, details);
        LOG_F(
            WARNING, "%s: read  %-30s @%02x.%c = %0*x", this->name.c_str(), get_reg_name(offset), offset,
            SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
        );
    }
    else {
        LOG_F(
            ERROR, "%s: read  unknown aperture %08x @%08x.%c", this->name.c_str(), rgn_start, offset,
            SIZE_ARG(size)
        );
    }

    return value;
}

void USBHostOHCI::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    if (rgn_start == this->aperture_base && offset < 0x1000) {
        if (offset > 0x90 || (offset & 3) || size != 4) {
            LOG_F(
                ERROR, "%s: write %-30s @%02x.%c = %0*x", this->name.c_str(), get_reg_name(offset), offset,
                SIZE_ARG(size), size * 2, BYTESWAP_SIZED(value, size)
            );
            return;
        }

        value = BYTESWAP_32(value);
        LOG_F(
            WARNING, "%s: write %-30s @%02x.%c = %0*x", this->name.c_str(), get_reg_name(offset), offset,
            SIZE_ARG(size), size * 2, value
        );

        #define WR_REG_SET(word, reg)      if (v.reg)                              { ohci_wr_ ## reg(v.reg      ); }
        #define WR_REG(    word, reg)      if (v.reg != this->HcOp.word.reg)       { ohci_wr_ ## reg(v.reg      ); }
        #define WR_REG_ADDR(word)          if (value != this->HcOp.word)           { ohci_wr_ ## word(value     ); }
        #define WR_REG_PORT_SET(word, reg) if (v.reg)                              { ohci_wr_ ## reg(v.reg, port); }
        #define WR_REG_PORT(    word, reg) if (v.reg != this->HcOp.word[port].reg) { ohci_wr_ ## reg(v.reg, port); }

        switch (offset) {
            case HcRevision: {
                auto &v = (HcRevision_t&)value;
                WR_REG(HcRevision, Revision)
                WR_REG(HcRevision, Reserved8)
                break;
            }
            case HcControl: {
                auto &v = (HcControl_t&)value;
                WR_REG(HcControl, ControlBulkServiceRatio)
                WR_REG(HcControl, PeriodicListEnable)
                WR_REG(HcControl, IsochronousEnable)
                WR_REG(HcControl, ControlListEnable)
                WR_REG(HcControl, BulkListEnable)
                WR_REG(HcControl, HostControllerFunctionalState)
                WR_REG(HcControl, InterruptRouting)
                WR_REG(HcControl, RemoteWakeupConnected)
                WR_REG(HcControl, RemoteWakeupEnable)
                WR_REG(HcControl, Reserved11)
                break;
            }
            case HcCommandStatus: {
                auto &v = (HcCommandStatus_t&)value;
                WR_REG_SET(HcCommandStatus, HostControllerReset)
                WR_REG_SET(HcCommandStatus, ControlListFilled)
                WR_REG_SET(HcCommandStatus, BulkListFilled)
                WR_REG_SET(HcCommandStatus, OwnershipChangeRequest)
                WR_REG_SET(HcCommandStatus, Reserved4)
                WR_REG_SET(HcCommandStatus, SchedulingOverrunCount)
                WR_REG_SET(HcCommandStatus, Reserved18)
                break;
            }
            case HcInterruptStatus: {
                auto &v = (HcInterruptStatus_t&)value;
                WR_REG_SET(HcInterruptStatus, SchedulingOverrun)
                WR_REG_SET(HcInterruptStatus, WritebackDoneHead)
                WR_REG_SET(HcInterruptStatus, StartOfFrame)
                WR_REG_SET(HcInterruptStatus, ResumeDetected)
                WR_REG_SET(HcInterruptStatus, UnrecoverableError)
                WR_REG_SET(HcInterruptStatus, FrameNumberOverflow)
                WR_REG_SET(HcInterruptStatus, RootHubStatusChange)
                WR_REG_SET(HcInterruptStatus, Reserved7)
                WR_REG_SET(HcInterruptStatus, OwnershipChange)
                WR_REG_SET(HcInterruptStatus, Reserved31)
                break;
            }
            case HcInterruptEnable: {
                auto &v = (HcInterruptEnable_t&)value;
                WR_REG_SET(HcInterruptEnable, SchedulingOverrunEnable)
                WR_REG_SET(HcInterruptEnable, WritebackDoneHeadEnable)
                WR_REG_SET(HcInterruptEnable, StartOfFrameEnable)
                WR_REG_SET(HcInterruptEnable, ResumeDetectedEnable)
                WR_REG_SET(HcInterruptEnable, UnrecoverableErrorEnable)
                WR_REG_SET(HcInterruptEnable, FrameNumberOverflowEnable)
                WR_REG_SET(HcInterruptEnable, RootHubStatusChangeEnable)
                WR_REG_SET(HcInterruptEnable, Reserved7_2)
                WR_REG_SET(HcInterruptEnable, OwnershipChangeEnable)
                WR_REG_SET(HcInterruptEnable, MasterInterruptEnable)
                break;
            }
            case HcInterruptDisable: {
                auto &v = (HcInterruptDisable_t&)value;
                WR_REG_SET(HcInterruptDisable, SchedulingOverrunDisable)
                WR_REG_SET(HcInterruptDisable, WritebackDoneHeadDisable)
                WR_REG_SET(HcInterruptDisable, StartOfFrameDisable)
                WR_REG_SET(HcInterruptDisable, ResumeDetectedDisable)
                WR_REG_SET(HcInterruptDisable, UnrecoverableErrorDisable)
                WR_REG_SET(HcInterruptDisable, FrameNumberOverflowDisable)
                WR_REG_SET(HcInterruptDisable, RootHubStatusChangeDisable)
                WR_REG_SET(HcInterruptDisable, Reserved7_3)
                WR_REG_SET(HcInterruptDisable, OwnershipChangeDisable)
                WR_REG_SET(HcInterruptDisable, MasterInterruptDisable)
                break;
            }

            case HcHCCA:
                WR_REG_ADDR(HcHCCA)
                break;

            case HcPeriodCurrentED:
                WR_REG_ADDR(HcPeriodCurrentED)
                break;

            case HcControlHeadED:
                WR_REG_ADDR(HcControlHeadED)
                break;

            case HcControlCurrentED:
                WR_REG_ADDR(HcControlCurrentED)
                break;

            case HcBulkHeadED:
                WR_REG_ADDR(HcBulkHeadED)
                break;

            case HcBulkCurrentED:
                WR_REG_ADDR(HcBulkCurrentED)
                break;

            case HcDoneHead:
                WR_REG_ADDR(HcDoneHead)
                break;

            case HcFmInterval: {
                auto &v = (HcFmInterval_t&)value;
                WR_REG(HcFmInterval, FrameInterval)
                WR_REG(HcFmInterval, Reserved)
                WR_REG(HcFmInterval, FSLargestDataPacket)
                WR_REG(HcFmInterval, FrameIntervalToggle)
                break;
            }
            case HcFmRemaining: {
                auto &v = (HcFmRemaining_t&)value;
                WR_REG(HcFmRemaining, FrameRemaining)
                WR_REG(HcFmRemaining, Reserved14)
                WR_REG(HcFmRemaining, FrameRemainingToggle)
                break;
            }
            case HcFmNumber: {
                auto &v = (HcFmNumber_t&)value;
                WR_REG(HcFmNumber, FrameNumber)
                WR_REG(HcFmNumber, Reserved16)
                break;
            }
            case HcPeriodicStart: {
                auto &v = (HcPeriodicStart_t&)value;
                WR_REG(HcPeriodicStart, PeriodicStart)
                WR_REG(HcPeriodicStart, Reserved16_2)
                break;
            }
            case HcLSThreshold: {
                auto &v = (HcLSThreshold_t&)value;
                WR_REG(HcLSThreshold, LSThreshold)
                WR_REG(HcLSThreshold, Reserved12)
                break;
            }
            case HcRhDescriptorA: {
                auto &v = (HcRhDescriptorA_t&)value;
                WR_REG(HcRhDescriptorA, NumberDownstreamPorts)
                WR_REG(HcRhDescriptorA, PowerSwitchingMode)
                WR_REG(HcRhDescriptorA, NoPowerSwitching)
                WR_REG(HcRhDescriptorA, DeviceType)
                WR_REG(HcRhDescriptorA, OverCurrentProtectionMode)
                WR_REG(HcRhDescriptorA, NoOverCurrentProtection)
                WR_REG(HcRhDescriptorA, Reserved13)
                WR_REG(HcRhDescriptorA, PowerOnToPowerGoodTime)
                break;
            }
            case HcRhDescriptorB: {
                auto &v = (HcRhDescriptorB_t&)value;
                WR_REG(HcRhDescriptorB, DeviceRemovable)
                WR_REG(HcRhDescriptorB, PortPowerControlMask)
                break;
            }
            case HcRhStatus: {
                auto &v = (HcRhStatus_t&)value;
                WR_REG_SET(HcRhStatus, LocalPowerStatus)
                WR_REG(HcRhStatus, OverCurrentIndicator)
                WR_REG(HcRhStatus, Reserved2)
                WR_REG_SET(HcRhStatus, DeviceRemoteWakeupEnable)
                WR_REG_SET(HcRhStatus, LocalPowerStatusChange)
                WR_REG_SET(HcRhStatus, OverCurrentIndicatorChange)
                WR_REG(HcRhStatus, Reserved18_2)
                WR_REG_SET(HcRhStatus, ClearRemoteWakeupEnable)
                break;
            }
            default: {
                int port = (offset - HcRhPortStatus) / 4;
                if (port < 15) {
                    auto &v = (HcRhPortStatus_t&)value;
                    WR_REG_PORT_SET(HcRhPortStatus, CurrentConnectStatus)
                    WR_REG_PORT_SET(HcRhPortStatus, PortEnableStatus)
                    WR_REG_PORT_SET(HcRhPortStatus, PortSuspendStatus)
                    WR_REG_PORT_SET(HcRhPortStatus, PortOverCurrentIndicator)
                    WR_REG_PORT_SET(HcRhPortStatus, PortResetStatus)
                    WR_REG_PORT(HcRhPortStatus, Reserved5)
                    WR_REG_PORT_SET(HcRhPortStatus, PortPowerStatus)
                    WR_REG_PORT_SET(HcRhPortStatus, LowSpeedDeviceAttached)
                    WR_REG_PORT(HcRhPortStatus, Reserved10)
                    WR_REG_PORT_SET(HcRhPortStatus, ConnectStatusChange)
                    WR_REG_PORT_SET(HcRhPortStatus, PortEnableStatusChange)
                    WR_REG_PORT_SET(HcRhPortStatus, PortSuspendStatusChange)
                    WR_REG_PORT_SET(HcRhPortStatus, PortOverCurrentIndicatorChange)
                    WR_REG_PORT_SET(HcRhPortStatus, PortResetStatusChange)
                    WR_REG_PORT(HcRhPortStatus, Reserved21)
                }
                break;
            }
        } // switch offset
        #undef WR_REG
        #undef WR_REG_PORT
    }
    else {
        LOG_F(
            ERROR, "%s: write unknown aperture %08x @%08x.%c = %0*x", this->name.c_str(), rgn_start, offset,
            SIZE_ARG(size), size * 2, value
        );
    }
}

void USBHostOHCI::ohci_wr_Revision(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "Revision", HcOp.HcRevision.Revision, v);
}

void USBHostOHCI::ohci_wr_Reserved8(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved8", HcOp.HcRevision.Reserved8, v);
}

void USBHostOHCI::ohci_wr_ControlBulkServiceRatio(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ControlBulkServiceRatio", HcOp.HcControl.ControlBulkServiceRatio, v);
    HcOp.HcControl.ControlBulkServiceRatio = v;
}

void USBHostOHCI::ohci_wr_PeriodicListEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "PeriodicListEnable", HcOp.HcControl.PeriodicListEnable, v);
    HcOp.HcControl.PeriodicListEnable = v;
}

void USBHostOHCI::ohci_wr_IsochronousEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "IsochronousEnable", HcOp.HcControl.IsochronousEnable, v);
    HcOp.HcControl.IsochronousEnable = v;
}

void USBHostOHCI::ohci_wr_ControlListEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ControlListEnable", HcOp.HcControl.ControlListEnable, v);
    HcOp.HcControl.ControlListEnable = v;
}

void USBHostOHCI::ohci_wr_BulkListEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "BulkListEnable", HcOp.HcControl.BulkListEnable, v);
    HcOp.HcControl.BulkListEnable = v;
}

void USBHostOHCI::ohci_wr_HostControllerFunctionalState(HCFS_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s", this->get_name().c_str(),
        "HostControllerFunctionalState", get_state_name(HcOp.HcControl.HostControllerFunctionalState), get_state_name(v));
    SetHcFunctionalState(v, false);
}

void USBHostOHCI::ohci_wr_InterruptRouting(InterruptRouting_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s", this->get_name().c_str(),
        "InterruptRouting", get_routing_name(HcOp.HcControl.InterruptRouting), get_routing_name(v));
    HcOp.HcControl.InterruptRouting = v;
}

void USBHostOHCI::ohci_wr_RemoteWakeupConnected(bool v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (unsupported)", this->get_name().c_str(),
        "RemoteWakeupConnected", HcOp.HcControl.RemoteWakeupConnected, v);
    HcOp.HcControl.RemoteWakeupConnected = v;
}

void USBHostOHCI::ohci_wr_RemoteWakeupEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "RemoteWakeupEnable", HcOp.HcControl.RemoteWakeupEnable, v);
    HcOp.HcControl.RemoteWakeupEnable = v;
}

void USBHostOHCI::ohci_wr_Reserved11(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved11", HcOp.HcControl.Reserved11, v);
}

void USBHostOHCI::ohci_wr_HostControllerReset(bool v) {
    LOG_F(WARNING, "%s:       %-30s SoftwareReset", this->get_name().c_str(),
        "HostControllerReset");
    HcOp.HcCommandStatus.HostControllerReset = true;
    SoftwareReset();
    HcOp.HcCommandStatus.HostControllerReset = false;
}

void USBHostOHCI::ohci_wr_ControlListFilled(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ControlListFilled", HcOp.HcCommandStatus.ControlListFilled, v);
    HcOp.HcCommandStatus.ControlListFilled = v;
}

void USBHostOHCI::ohci_wr_BulkListFilled(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "BulkListFilled", HcOp.HcCommandStatus.BulkListFilled, v);
    HcOp.HcCommandStatus.BulkListFilled = v;
}

void USBHostOHCI::ohci_wr_OwnershipChangeRequest(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "OwnershipChangeRequest", HcOp.HcCommandStatus.OwnershipChangeRequest, v);
    HcOp.HcCommandStatus.OwnershipChangeRequest = v;
    OwnershipChange = true;
}

void USBHostOHCI::ohci_wr_Reserved4(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved4", HcOp.HcCommandStatus.Reserved4, v);
}

void USBHostOHCI::ohci_wr_SchedulingOverrunCount(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "SchedulingOverrunCount", HcOp.HcCommandStatus.SchedulingOverrunCount, v);
}

void USBHostOHCI::ohci_wr_Reserved18(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved18", HcOp.HcCommandStatus.Reserved18, v);
}

void USBHostOHCI::ohci_wr_SchedulingOverrun(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "SchedulingOverrun", HcOp.HcInterruptStatus.SchedulingOverrun, false);
    HcOp.HcInterruptStatus.SchedulingOverrun = false;
}

void USBHostOHCI::ohci_wr_WritebackDoneHead(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "WritebackDoneHead", HcOp.HcInterruptStatus.WritebackDoneHead, false);
    HcOp.HcInterruptStatus.WritebackDoneHead = false;
}

void USBHostOHCI::ohci_wr_StartOfFrame(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "StartOfFrame", HcOp.HcInterruptStatus.StartOfFrame, false);
    HcOp.HcInterruptStatus.StartOfFrame = false;
}

void USBHostOHCI::ohci_wr_ResumeDetected(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ResumeDetected", HcOp.HcInterruptStatus.ResumeDetected, false);
    HcOp.HcInterruptStatus.ResumeDetected = false;
}

void USBHostOHCI::ohci_wr_UnrecoverableError(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "UnrecoverableError", HcOp.HcInterruptStatus.UnrecoverableError, false);
    HcOp.HcInterruptStatus.UnrecoverableError = false;
}

void USBHostOHCI::ohci_wr_FrameNumberOverflow(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FrameNumberOverflow", HcOp.HcInterruptStatus.FrameNumberOverflow, false);
    HcOp.HcInterruptStatus.FrameNumberOverflow = false;
}

void USBHostOHCI::ohci_wr_RootHubStatusChange(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "RootHubStatusChange", HcOp.HcInterruptStatus.RootHubStatusChange, false);
    HcOp.HcInterruptStatus.RootHubStatusChange = false;
}

void USBHostOHCI::ohci_wr_Reserved7(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved7", HcOp.HcInterruptStatus.Reserved7, v);
}

void USBHostOHCI::ohci_wr_OwnershipChange(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "OwnershipChange", HcOp.HcInterruptStatus.OwnershipChange, false);
    HcOp.HcInterruptStatus.OwnershipChange = false;
}

void USBHostOHCI::ohci_wr_Reserved31(bool v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved31", HcOp.HcInterruptStatus.Reserved31, v);
}

void USBHostOHCI::ohci_wr_SchedulingOverrunEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "SchedulingOverrunEnable", HcOp.HcInterruptEnable.SchedulingOverrunEnable, true);
    HcOp.HcInterruptEnable.SchedulingOverrunEnable = true;
}

void USBHostOHCI::ohci_wr_WritebackDoneHeadEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "WritebackDoneHeadEnable", HcOp.HcInterruptEnable.WritebackDoneHeadEnable, true);
    HcOp.HcInterruptEnable.WritebackDoneHeadEnable = true;
}

void USBHostOHCI::ohci_wr_StartOfFrameEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "StartOfFrameEnable", HcOp.HcInterruptEnable.StartOfFrameEnable, true);
    HcOp.HcInterruptEnable.StartOfFrameEnable = true;
}

void USBHostOHCI::ohci_wr_ResumeDetectedEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ResumeDetectedEnable", HcOp.HcInterruptEnable.ResumeDetectedEnable, true);
    HcOp.HcInterruptEnable.ResumeDetectedEnable = true;
}

void USBHostOHCI::ohci_wr_UnrecoverableErrorEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "UnrecoverableErrorEnable", HcOp.HcInterruptEnable.UnrecoverableErrorEnable, true);
    HcOp.HcInterruptEnable.UnrecoverableErrorEnable = true;
}

void USBHostOHCI::ohci_wr_FrameNumberOverflowEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FrameNumberOverflowEnable", HcOp.HcInterruptEnable.FrameNumberOverflowEnable, true);
    HcOp.HcInterruptEnable.FrameNumberOverflowEnable = true;
}

void USBHostOHCI::ohci_wr_RootHubStatusChangeEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "RootHubStatusChangeEnable", HcOp.HcInterruptEnable.RootHubStatusChangeEnable, true);
    HcOp.HcInterruptEnable.RootHubStatusChangeEnable = true;
}

void USBHostOHCI::ohci_wr_Reserved7_2(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved7_2", HcOp.HcInterruptEnable.Reserved7_2, v);
}

void USBHostOHCI::ohci_wr_OwnershipChangeEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "OwnershipChangeEnable", HcOp.HcInterruptEnable.OwnershipChangeEnable, true);
    HcOp.HcInterruptEnable.OwnershipChangeEnable = true;
}

void USBHostOHCI::ohci_wr_MasterInterruptEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "MasterInterruptEnable", HcOp.HcInterruptEnable.MasterInterruptEnable, true);
    HcOp.HcInterruptEnable.MasterInterruptEnable = true;
}

void USBHostOHCI::ohci_wr_SchedulingOverrunDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "SchedulingOverrunEnable", HcOp.HcInterruptEnable.SchedulingOverrunEnable, false);
    HcOp.HcInterruptEnable.SchedulingOverrunEnable = false;
}

void USBHostOHCI::ohci_wr_WritebackDoneHeadDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "WritebackDoneHeadEnable", HcOp.HcInterruptEnable.WritebackDoneHeadEnable, false);
    HcOp.HcInterruptEnable.WritebackDoneHeadEnable = false;
}

void USBHostOHCI::ohci_wr_StartOfFrameDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "StartOfFrameEnable", HcOp.HcInterruptEnable.StartOfFrameEnable, false);
    HcOp.HcInterruptEnable.StartOfFrameEnable = false;
}

void USBHostOHCI::ohci_wr_ResumeDetectedDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "ResumeDetectedEnable", HcOp.HcInterruptEnable.ResumeDetectedEnable, false);
    HcOp.HcInterruptEnable.ResumeDetectedEnable = false;
}

void USBHostOHCI::ohci_wr_UnrecoverableErrorDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "UnrecoverableErrorEnable", HcOp.HcInterruptEnable.UnrecoverableErrorEnable, false);
    HcOp.HcInterruptEnable.UnrecoverableErrorEnable = false;
}

void USBHostOHCI::ohci_wr_FrameNumberOverflowDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FrameNumberOverflowEnable", HcOp.HcInterruptEnable.FrameNumberOverflowEnable, false);
    HcOp.HcInterruptEnable.FrameNumberOverflowEnable = false;
}

void USBHostOHCI::ohci_wr_RootHubStatusChangeDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "RootHubStatusChangeEnable", HcOp.HcInterruptEnable.RootHubStatusChangeEnable, false);
    HcOp.HcInterruptEnable.RootHubStatusChangeEnable = false;
}

void USBHostOHCI::ohci_wr_Reserved7_3(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved7_3", HcOp.HcInterruptDisable.Reserved7_3, v);
}

void USBHostOHCI::ohci_wr_OwnershipChangeDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "OwnershipChangeEnable", HcOp.HcInterruptEnable.OwnershipChangeEnable, false);
    HcOp.HcInterruptEnable.OwnershipChangeEnable = false;
}

void USBHostOHCI::ohci_wr_MasterInterruptDisable(bool v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "MasterInterruptEnable", HcOp.HcInterruptEnable.MasterInterruptEnable, false);
    HcOp.HcInterruptEnable.MasterInterruptEnable = false;
}

void USBHostOHCI::ohci_wr_HcHCCA(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %08x to %08x", this->get_name().c_str(),
        "HcHCCA", HcOp.HcHCCA, v & ~255);
    HcOp.HcHCCA = v & ~255;
    if (v == 0xffffffff) {
        this->hcca = 0;
    }
    else {
        this->hcca = (HostControllerCommunicationsArea_t *)mmu_get_dma_mem(HcOp.HcHCCA, sizeof(*this->hcca));
    }
}

void USBHostOHCI::ohci_wr_HcPeriodCurrentED(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %08x to %08x (read only)", this->get_name().c_str(),
        "HcPeriodCurrentED", HcOp.HcPeriodCurrentED, v & ~15);
    // HcOp.HcPeriodCurrentED = v & ~15;
}

void USBHostOHCI::ohci_wr_HcControlHeadED(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %08x to %08x", this->get_name().c_str(),
        "HcControlHeadED", HcOp.HcControlHeadED, v & ~15);
    HcOp.HcControlHeadED = v & ~15;
}

void USBHostOHCI::ohci_wr_HcControlCurrentED(uint32_t v) {
    if (HcOp.HcControl.ControlListEnable) {
        LOG_F(ERROR, "%s:       %-30s from %08x to %08x (should not change while control list is enabled)",
            this->get_name().c_str(), "HcControlCurrentED", HcOp.HcControlCurrentED, v & ~15);
    }
    else {
        LOG_F(WARNING, "%s:       %-30s from %08x to %08x", this->get_name().c_str(),
            "HcControlCurrentED", HcOp.HcControlCurrentED, v & ~15);
    }
    HcOp.HcControlCurrentED = v & ~15;
}

void USBHostOHCI::ohci_wr_HcBulkHeadED(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %08x to %08x", this->get_name().c_str(),
        "HcBulkHeadED", HcOp.HcBulkHeadED, v & ~15);
    HcOp.HcBulkHeadED = v & ~15;
}

void USBHostOHCI::ohci_wr_HcBulkCurrentED(uint32_t v) {
    if (HcOp.HcControl.BulkListEnable) {
        LOG_F(ERROR, "%s:       %-30s from %08x to %08x (should not change while bulk list is enabled)", this->get_name().c_str(),
            "HcBulkCurrentED", HcOp.HcBulkCurrentED, v & ~15);
    }
    else {
        LOG_F(WARNING, "%s:       %-30s from %08x to %08x", this->get_name().c_str(),
            "HcBulkCurrentED", HcOp.HcBulkCurrentED, v & ~15);
    }
    HcOp.HcBulkCurrentED = v & ~15;
}

void USBHostOHCI::ohci_wr_HcDoneHead(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %08x to %08x (read only)", this->get_name().c_str(),
        "HcDoneHead", HcOp.HcDoneHead, v & ~15);
    // HcOp.HcDoneHead = v & ~15;
}

void USBHostOHCI::ohci_wr_FrameInterval(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FrameInterval", HcOp.HcFmInterval.FrameInterval, v);
    HcOp.HcFmInterval.FrameInterval = v;
}

void USBHostOHCI::ohci_wr_Reserved(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved", HcOp.HcFmInterval.Reserved, v);
}

void USBHostOHCI::ohci_wr_FSLargestDataPacket(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FSLargestDataPacket", HcOp.HcFmInterval.FSLargestDataPacket, v);
    HcOp.HcFmInterval.FSLargestDataPacket = v;
}

void USBHostOHCI::ohci_wr_FrameIntervalToggle(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "FrameIntervalToggle", HcOp.HcFmInterval.FrameIntervalToggle, v);
    HcOp.HcFmInterval.FrameIntervalToggle = v;
}

void USBHostOHCI::ohci_wr_FrameRemaining(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "FrameRemaining", HcOp.HcFmRemaining.FrameRemaining, v);
}

void USBHostOHCI::ohci_wr_Reserved14(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved14", HcOp.HcFmRemaining.Reserved14, v);
}

void USBHostOHCI::ohci_wr_FrameRemainingToggle(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "FrameRemainingToggle", HcOp.HcFmRemaining.FrameRemainingToggle, v);
}

void USBHostOHCI::ohci_wr_FrameNumber(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "FrameNumber", HcOp.HcFmNumber.FrameNumber, v);
}

void USBHostOHCI::ohci_wr_Reserved16(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved16", HcOp.HcFmNumber.Reserved16, v);
}

void USBHostOHCI::ohci_wr_PeriodicStart(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "PeriodicStart", HcOp.HcPeriodicStart.PeriodicStart, v);
    HcOp.HcPeriodicStart.PeriodicStart = v;
}

void USBHostOHCI::ohci_wr_Reserved16_2(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved16_2", HcOp.HcPeriodicStart.Reserved16_2, v);
}

void USBHostOHCI::ohci_wr_LSThreshold(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
        "LSThreshold", HcOp.HcLSThreshold.LSThreshold, v);
    HcOp.HcLSThreshold.LSThreshold = v;
}

void USBHostOHCI::ohci_wr_Reserved12(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved12", HcOp.HcLSThreshold.Reserved12, v);
}

void USBHostOHCI::ohci_wr_NumberDownstreamPorts(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "NumberDownstreamPorts", HcOp.HcRhDescriptorA.NumberDownstreamPorts, v);
}

void USBHostOHCI::ohci_wr_PowerSwitchingMode(PowerSwitchingMode_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s", this->get_name().c_str(), "PowerSwitchingMode",
        get_power_switching_mode_name(HcOp.HcRhDescriptorA.PowerSwitchingMode), get_power_switching_mode_name(v));
    HcOp.HcRhDescriptorA.PowerSwitchingMode = v;
}

void USBHostOHCI::ohci_wr_NoPowerSwitching(NoPowerSwitching_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s", this->get_name().c_str(),
        "NoPowerSwitching", get_no_power_switching_name(HcOp.HcRhDescriptorA.NoPowerSwitching), get_no_power_switching_name(v));
    HcOp.HcRhDescriptorA.NoPowerSwitching = v;
}

void USBHostOHCI::ohci_wr_DeviceType(DeviceType_t v) {
    LOG_F(ERROR, "%s:       %-30s from %s to %s (read only)", this->get_name().c_str(),
        "DeviceType", get_device_type_name(HcOp.HcRhDescriptorA.DeviceType), get_device_type_name(v));
}

void USBHostOHCI::ohci_wr_OverCurrentProtectionMode(OverCurrentProtectionMode_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s",
        this->get_name().c_str(), "OverCurrentProtectionMode",
        get_over_current_protection_mode_name(HcOp.HcRhDescriptorA.OverCurrentProtectionMode),
        get_over_current_protection_mode_name(v));
    HcOp.HcRhDescriptorA.OverCurrentProtectionMode = v;
}

void USBHostOHCI::ohci_wr_NoOverCurrentProtection(NoOverCurrentProtection_t v) {
    LOG_F(WARNING, "%s:       %-30s from %s to %s",
        this->get_name().c_str(), "NoOverCurrentProtection",
        get_no_over_current_protection_name(HcOp.HcRhDescriptorA.NoOverCurrentProtection),
        get_no_over_current_protection_name(v));
    HcOp.HcRhDescriptorA.NoOverCurrentProtection = v;
}

void USBHostOHCI::ohci_wr_Reserved13(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved13", HcOp.HcRhDescriptorA.Reserved13, v);
}

void USBHostOHCI::ohci_wr_PowerOnToPowerGoodTime(uint32_t v) {
    LOG_F(WARNING, "%s:       %-30s from %d ms to %d ms", this->get_name().c_str(),
        "PowerOnToPowerGoodTime", HcOp.HcRhDescriptorA.PowerOnToPowerGoodTime * 2, v * 2);
    HcOp.HcRhDescriptorA.PowerOnToPowerGoodTime = v;
}

void USBHostOHCI::ohci_wr_DeviceRemovable(uint16_t v) {
    LOG_F(WARNING, "%s:       %-30s from 0x%04x to 0x%04x", this->get_name().c_str(),
        "DeviceRemovable", HcOp.HcRhDescriptorB.DeviceRemovable, v);
    HcOp.HcRhDescriptorB.DeviceRemovable = v;
}

void USBHostOHCI::ohci_wr_PortPowerControlMask(uint16_t v) {
    LOG_F(WARNING, "%s:       %-30s from 0x%04x to 0x%04x", this->get_name().c_str(),
        "PortPowerControlMask", HcOp.HcRhDescriptorB.PortPowerControlMask, v);
    HcOp.HcRhDescriptorB.PortPowerControlMask = v;
}

void USBHostOHCI::ohci_wr_LocalPowerStatus(bool v) {
    LOG_F(WARNING, "%s:       %-30s ClearGlobalPower", this->get_name().c_str(),
        "LocalPowerStatus");
    ClearGlobalPower();
}

void USBHostOHCI::ohci_wr_OverCurrentIndicator(bool v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (read only)", this->get_name().c_str(),
        "OverCurrentIndicator", HcOp.HcRhStatus.OverCurrentIndicator, v);
}

void USBHostOHCI::ohci_wr_Reserved2(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved2", HcOp.HcRhStatus.Reserved2, v);
}

void USBHostOHCI::ohci_wr_DeviceRemoteWakeupEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s SetRemoteWakeupEnable", this->get_name().c_str(),
        "DeviceRemoteWakeupEnable");
    SetRemoteWakeupEnable();
}

void USBHostOHCI::ohci_wr_LocalPowerStatusChange(bool v) {
    LOG_F(WARNING, "%s:       %-30s SetGlobalPower", this->get_name().c_str(),
        "LocalPowerStatusChange");
    SetGlobalPower();
}

void USBHostOHCI::ohci_wr_OverCurrentIndicatorChange(bool v) {
    if (HcOp.HcRhStatus.OverCurrentIndicatorChange) {
        LOG_F(WARNING, "%s:       %-30s from %d to %d", this->get_name().c_str(),
            "OverCurrentIndicatorChange", HcOp.HcRhStatus.OverCurrentIndicatorChange, false);
        HcOp.HcRhStatus.OverCurrentIndicatorChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       %-30s is already clear", this->get_name().c_str(),
            "OverCurrentIndicatorChange");
    }
}

void USBHostOHCI::ohci_wr_Reserved18_2(uint32_t v) {
    LOG_F(ERROR, "%s:       %-30s from %d to %d (ignored)", this->get_name().c_str(),
        "Reserved18_2", HcOp.HcRhStatus.Reserved18_2, v);
}

void USBHostOHCI::ohci_wr_ClearRemoteWakeupEnable(bool v) {
    LOG_F(WARNING, "%s:       %-30s ClearRemoteWakeupEnable", this->get_name().c_str(),
        "ClearRemoteWakeupEnable");
    ClearRemoteWakeupEnable();
}

void USBHostOHCI::ohci_wr_CurrentConnectStatus(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s ClearPortEnable", this->get_name().c_str(), port + 1,
        "CurrentConnectStatus");
    ClearPortEnable(port);
}

void USBHostOHCI::ohci_wr_PortEnableStatus(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s SetPortEnable", this->get_name().c_str(), port + 1,
        "PortEnableStatus");
    SetPortEnable(port);
}

void USBHostOHCI::ohci_wr_PortSuspendStatus(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s SetSuspendStatus", this->get_name().c_str(), port + 1,
        "PortSuspendStatus");
    SetSuspendStatus(port);
}

void USBHostOHCI::ohci_wr_PortOverCurrentIndicator(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s ClearSuspendStatus", this->get_name().c_str(), port + 1,
        "PortOverCurrentIndicator");
    ClearSuspendStatus(port);
}

void USBHostOHCI::ohci_wr_PortResetStatus(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s SetPortReset", this->get_name().c_str(), port + 1,
        "PortResetStatus");
    SetPortReset(port);
}

void USBHostOHCI::ohci_wr_Reserved5(uint32_t v, int port) {
    LOG_F(ERROR, "%s:       port#%d.%-30s from %d to %d (ignored)", this->get_name().c_str(), port + 1,
        "Reserved5", HcOp.HcRhPortStatus[port].Reserved5, v);
}

void USBHostOHCI::ohci_wr_PortPowerStatus(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s SetPortPower", this->get_name().c_str(), port + 1,
        "PortPowerStatus");
    SetPortPower(port);
}

void USBHostOHCI::ohci_wr_LowSpeedDeviceAttached(bool v, int port) {
    LOG_F(WARNING, "%s:       port#%d.%-30s ClearPortPower", this->get_name().c_str(), port + 1,
        "LowSpeedDeviceAttached");
    ClearPortPower(port);
}

void USBHostOHCI::ohci_wr_Reserved10(uint32_t v, int port) {
    LOG_F(ERROR, "%s:       port#%d.%-30s from %d to %d (ignored)", this->get_name().c_str(), port + 1,
        "Reserved10", HcOp.HcRhPortStatus[port].Reserved10, v);
}

void USBHostOHCI::ohci_wr_ConnectStatusChange(bool v, int port) {
    if (HcOp.HcRhPortStatus[port].ConnectStatusChange) {
        LOG_F(WARNING, "%s:       port#%d.%-30s from %d to %d", this->get_name().c_str(), port + 1,
            "ConnectStatusChange", HcOp.HcRhPortStatus[port].ConnectStatusChange, false);
        HcOp.HcRhPortStatus[port].ConnectStatusChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       port#%d.%-30s is already clear", this->get_name().c_str(), port + 1,
            "ConnectStatusChange");
    }
}

void USBHostOHCI::ohci_wr_PortEnableStatusChange(bool v, int port) {
    if (HcOp.HcRhPortStatus[port].PortEnableStatusChange) {
        LOG_F(WARNING, "%s:       port#%d.%-30s from %d to %d", this->get_name().c_str(), port + 1,
            "PortEnableStatusChange", HcOp.HcRhPortStatus[port].PortEnableStatusChange, false);
        HcOp.HcRhPortStatus[port].PortEnableStatusChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       port#%d.%-30s is already clear", this->get_name().c_str(), port + 1,
            "PortEnableStatusChange");
    }
}

void USBHostOHCI::ohci_wr_PortSuspendStatusChange(bool v, int port) {
    if (HcOp.HcRhPortStatus[port].PortSuspendStatusChange) {
        LOG_F(WARNING, "%s:       port#%d.%-30s from %d to %d", this->get_name().c_str(), port + 1,
            "PortSuspendStatusChange", HcOp.HcRhPortStatus[port].PortSuspendStatusChange, false);
        HcOp.HcRhPortStatus[port].PortSuspendStatusChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       port#%d.%-30s is already clear", this->get_name().c_str(), port + 1,
            "PortSuspendStatusChange");
    }
}

void USBHostOHCI::ohci_wr_PortOverCurrentIndicatorChange(bool v, int port) {
    if (HcOp.HcRhPortStatus[port].PortOverCurrentIndicatorChange) {
        LOG_F(WARNING, "%s:       port#%d.%-30s from %d to %d", this->get_name().c_str(), port + 1,
            "PortOverCurrentIndicatorChange", HcOp.HcRhPortStatus[port].PortOverCurrentIndicatorChange, false);
        HcOp.HcRhPortStatus[port].PortOverCurrentIndicatorChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       port#%d.%-30s is already clear", this->get_name().c_str(), port + 1,
            "PortOverCurrentIndicatorChange");
    }
}

void USBHostOHCI::ohci_wr_PortResetStatusChange(bool v, int port) {
    if (HcOp.HcRhPortStatus[port].PortResetStatusChange) {
        LOG_F(WARNING, "%s:       port#%d.%-30s from %d to %d", this->get_name().c_str(), port + 1,
            "PortResetStatusChange", HcOp.HcRhPortStatus[port].PortResetStatusChange, false);
        HcOp.HcRhPortStatus[port].PortResetStatusChange = false;
        RootHubStatusChange = true;
    }
    else {
        LOG_F(WARNING, "%s:       port#%d.%-30s is already clear", this->get_name().c_str(), port + 1,
            "PortResetStatusChange");
    }
}

void USBHostOHCI::ohci_wr_Reserved21(uint32_t v, int port) {
    LOG_F(ERROR, "%s:       port#%d.%-30s from %d to %d (ignored)", this->get_name().c_str(), port + 1,
        "Reserved21", HcOp.HcRhPortStatus[port].Reserved21, v);
}


void USBHostOHCI::SetHcFunctionalState(HCFS_t v, bool soft_reset) {
    switch (v) {
        case HCFS_UsbReset:
            ResetRegisters(false);
            BroadcastState(v);
            break;
        case HCFS_UsbResume:
            if (HcOp.HcControl.HostControllerFunctionalState != HCFS_UsbSuspend) {
                LOG_F(ERROR, "%s: Can't Resume unless Suspended", this->get_name().c_str());
            }
            else {
                for (int port = 0; port < 15; port++)
                    ClearSuspendStatus(port);
                HcOp.HcControl.HostControllerFunctionalState = v;
                BroadcastState(v);
            }
            break;
        case HCFS_UsbOperational:
            HcOp.HcControl.HostControllerFunctionalState = v;
            DoneQueueInterruptCounter = 7;
            LargestDataPacketCounter = HcOp.HcFmInterval.FSLargestDataPacket;
            HcOp.HcFmRemaining.FrameRemaining = HcOp.HcFmInterval.FrameInterval;
            BroadcastState(v);
            NewFrame();
            break;
        case HCFS_UsbSuspend:
            if (!soft_reset && HcOp.HcControl.HostControllerFunctionalState != HCFS_UsbOperational) {
                LOG_F(ERROR, "%s: Can't Suspend unless Operational", this->get_name().c_str());
            }
            else {
                HcOp.HcControl.HostControllerFunctionalState = v;
                if (soft_reset) {
                    ResetRegisters(true);
                }
                BroadcastState(v);
            }
            break;
    }
}

void USBHostOHCI::ResetRegisters(bool soft_reset)
{
    // HcOp.HcRevision.Revision = 0x10;
    HcOp.HcControl.ControlBulkServiceRatio = 0;
    HcOp.HcControl.PeriodicListEnable = false;
    HcOp.HcControl.IsochronousEnable = false;
    HcOp.HcControl.ControlListEnable = false;
    HcOp.HcControl.BulkListEnable = false;
    // HcOp.HcControl.HostControllerFunctionalState = HCFS_UsbReset; // don't alter this outside SetHcFunctionalState
    // HcOp.HcControl.InterruptRouting = IR_HostBus; // hardware reset only
    // HcOp.HcControl.RemoteWakeupConnected = false; // hardware reset only
    HcOp.HcControl.RemoteWakeupEnable = false;
    FrameControl = HcOp.HcControl;
    HcOp.HcCommandStatus.HostControllerReset = false;
    HcOp.HcCommandStatus.ControlListFilled = false;
    HcOp.HcCommandStatus.BulkListFilled = false;
    HcOp.HcCommandStatus.OwnershipChangeRequest = false;
    HcOp.HcCommandStatus.SchedulingOverrunCount = 0;
    HcOp.HcInterruptStatus.SchedulingOverrun = false;
    HcOp.HcInterruptStatus.WritebackDoneHead = false;
    HcOp.HcInterruptStatus.StartOfFrame = false;
    HcOp.HcInterruptStatus.ResumeDetected = false;
    HcOp.HcInterruptStatus.UnrecoverableError = false;
    HcOp.HcInterruptStatus.FrameNumberOverflow = false;
    HcOp.HcInterruptStatus.RootHubStatusChange = false;
    HcOp.HcInterruptStatus.OwnershipChange = false;
    HcOp.HcInterruptEnable.SchedulingOverrunEnable = false;
    HcOp.HcInterruptEnable.WritebackDoneHeadEnable = false;
    HcOp.HcInterruptEnable.StartOfFrameEnable = false;
    HcOp.HcInterruptEnable.ResumeDetectedEnable = false;
    HcOp.HcInterruptEnable.UnrecoverableErrorEnable = false;
    HcOp.HcInterruptEnable.FrameNumberOverflowEnable = false;
    HcOp.HcInterruptEnable.RootHubStatusChangeEnable = false;
    HcOp.HcInterruptEnable.OwnershipChangeEnable = false;
    HcOp.HcInterruptEnable.MasterInterruptEnable = false;
    HcOp.HcInterruptDisable.SchedulingOverrunDisable = false;
    HcOp.HcInterruptDisable.WritebackDoneHeadDisable = false;
    HcOp.HcInterruptDisable.StartOfFrameDisable = false;
    HcOp.HcInterruptDisable.ResumeDetectedDisable = false;
    HcOp.HcInterruptDisable.UnrecoverableErrorDisable = false;
    HcOp.HcInterruptDisable.FrameNumberOverflowDisable = false;
    HcOp.HcInterruptDisable.RootHubStatusChangeDisable = false;
    HcOp.HcInterruptDisable.OwnershipChangeDisable = false;
    HcOp.HcInterruptDisable.MasterInterruptDisable = false;
    HcOp.HcHCCA = 0;
    HcOp.HcPeriodCurrentED = 0;
    HcOp.HcControlHeadED = 0;
    HcOp.HcControlCurrentED = 0;
    HcOp.HcBulkHeadED = 0;
    HcOp.HcBulkCurrentED = 0;
    HcOp.HcDoneHead = 0;
    HcOp.HcFmInterval.FrameInterval = 11999;
    HcOp.HcFmInterval.FSLargestDataPacket = 0;
    HcOp.HcFmInterval.FrameIntervalToggle = 0;
    HcOp.HcFmRemaining.FrameRemaining = 0;
    HcOp.HcFmRemaining.FrameRemainingToggle = 0;
    HcOp.HcFmNumber.FrameNumber = 0;
    HcOp.HcPeriodicStart.PeriodicStart = 0;
    HcOp.HcLSThreshold.LSThreshold = 1576;

    if (soft_reset) {
        // Software Reset does not reset the root hub
    }
    else {
        HcOp.HcControl.InterruptRouting = IR_HostBus;
        HcOp.HcControl.RemoteWakeupConnected = false;

        // Hardware Reset resets the root hub
        HcOp.HcRhDescriptorA = RhDescriptorA;
        HcOp.HcRhDescriptorB = RhDescriptorB;
        HcOp.HcRhDescriptorA.DeviceType = DT_NotACompoundDevice;
        HcOp.HcRhStatus.LocalPowerStatus = false;
        HcOp.HcRhStatus.OverCurrentIndicator = false;
        HcOp.HcRhStatus.DeviceRemoteWakeupEnable = false;
        HcOp.HcRhStatus.LocalPowerStatusChange = false;
        HcOp.HcRhStatus.OverCurrentIndicatorChange = false;
        HcOp.HcRhStatus.ClearRemoteWakeupEnable = false;
        for (int port = 0; port < 15; port++) {
            HcOp.HcRhPortStatus[port].CurrentConnectStatus = false;
            HcOp.HcRhPortStatus[port].PortEnableStatus = false;
            HcOp.HcRhPortStatus[port].PortSuspendStatus = false;
            HcOp.HcRhPortStatus[port].PortOverCurrentIndicator = false;
            HcOp.HcRhPortStatus[port].PortResetStatus = false;
            HcOp.HcRhPortStatus[port].PortPowerStatus = false;
            HcOp.HcRhPortStatus[port].LowSpeedDeviceAttached = false;
            HcOp.HcRhPortStatus[port].ConnectStatusChange = (HcOp.HcRhDescriptorB.DeviceRemovable & (1 << (port+1))) != 0;
            HcOp.HcRhPortStatus[port].PortEnableStatusChange = false;
            HcOp.HcRhPortStatus[port].PortSuspendStatusChange = false;
            HcOp.HcRhPortStatus[port].PortOverCurrentIndicatorChange = false;
            HcOp.HcRhPortStatus[port].PortResetStatusChange = false;
        }
    }

    DoneQueueInterruptCounter = 7;
}

void USBHostOHCI::BroadcastState(HCFS_t v) {
    if (!UnrecoverableError && !HcOp.HcInterruptStatus.UnrecoverableError) {
        // FIXME: iterate all devices and send termination of reset or resume when operational
        // For HCFS_UsbReset, asserts subsequent reset signaling to downstream ports
    }
}

void USBHostOHCI::HardwareReset() {
    USBHostOHCI::SetHcFunctionalState(HCFS_UsbReset, false);
}

void USBHostOHCI::SoftwareReset() {
    USBHostOHCI::SetHcFunctionalState(HCFS_UsbSuspend, true);
}

void USBHostOHCI::RemoteWakeup() {
    USBHostOHCI::SetHcFunctionalState(HCFS_UsbResume, false);
}

void USBHostOHCI::NewFrame() {
    if (!UnrecoverableError && !HcOp.HcInterruptStatus.UnrecoverableError &&
        HcOp.HcControl.HostControllerFunctionalState == HCFS_UsbOperational
    ) {
        IncrementFrameNumber();
        ServiceLists();
    }
}

void USBHostOHCI::IncrementFrameNumber () {
    // assert HostControllerFunctionalState == HCFS_UsbOperational

    // update PeriodicListEnable, IsochronousEnable, ControlListEnable, BulkListEnable
    FrameControl = HcOp.HcControl;

    // update frame number
    HcOp.HcFmNumber.FrameNumber++;
    if ((HcOp.HcFmNumber.FrameNumber & 0x7fff) == 0) {
        FrameNumberOverflow = true;
    }

    WRITE_WORD_LE_A(&hcca->HccaFrameNumber, HcOp.HcFmNumber.FrameNumber);
    WRITE_WORD_LE_A(&hcca->HccaPad1, 0);

    // update interrupts
    if (FrameNumberOverflow) {
        HcOp.HcInterruptStatus.FrameNumberOverflow = true;
        FrameNumberOverflow = false;
    }

    if (SchedulingOverrun) {
        HcOp.HcInterruptStatus.SchedulingOverrun = true;
        HcOp.HcCommandStatus.SchedulingOverrunCount++;
        SchedulingOverrun = false;
    }

    if (StartOfFrame) {
        HcOp.HcInterruptStatus.StartOfFrame = true;
        StartOfFrame = false;
    }

    if (ResumeDetected) {
        if (HcOp.HcControl.HostControllerFunctionalState == HCFS_UsbSuspend) {
            HcOp.HcInterruptStatus.ResumeDetected = true;
        }
        ResumeDetected = false;
    }

    if (UnrecoverableError) {
        HcOp.HcInterruptStatus.UnrecoverableError = true;
        UnrecoverableError = false;
    }

    if (RootHubStatusChange) {
        HcOp.HcInterruptStatus.RootHubStatusChange = true;
        RootHubStatusChange = false;
    }

    if (OwnershipChange) {
        if (HasSMI) {
            HcOp.HcInterruptStatus.OwnershipChange = true;
        }
        else {
            LOG_F(WARNING, "%s: SMI Interrupt for ownership change ignored", this->get_name().c_str());
        }
        OwnershipChange = false;
    }

    if (DoneQueueInterruptCounter == 0) {
        if (HcOp.HcDoneHead & !HcOp.HcInterruptStatus.WritebackDoneHead) {
            WRITE_DWORD_LE_A(&hcca->HccaDoneHead, HcOp.HcDoneHead);
            HcOp.HcDoneHead = 0;
            HcOp.HcInterruptStatus.WritebackDoneHead = true;
            DoneQueueInterruptCounter = 7;
        }
    }
    else if (DoneQueueInterruptCounter != 7) {
        DoneQueueInterruptCounter--;
    }

    TriggerInterrupt();
}

void USBHostOHCI::SendStartOfFrame() {
    //SOF_t sof;
    //sof.frameNumber = HcOp.HcFmNumber.FrameNumber;
    //HostToLE(&sof,sizeof(sof);
    // FIXME: Broadcast the SOF
}

void USBHostOHCI::DecrementFrameRemaining(int amount) {
    int frameRemaining;
    int counterDec;

    if (amount > HcOp.HcFmRemaining.FrameRemaining) {
        frameRemaining = (HcOp.HcFmInterval.FrameInterval + 1 + HcOp.HcFmRemaining.FrameRemaining
            - (amount % (HcOp.HcFmInterval.FrameInterval + 1))) % (HcOp.HcFmInterval.FrameInterval + 1);
        amount = HcOp.HcFmInterval.FrameInterval - frameRemaining;
        HcOp.HcFmRemaining.FrameRemaining = frameRemaining;
        HcOp.HcFmRemaining.FrameRemainingToggle = HcOp.HcFmInterval.FrameIntervalToggle;

        StartOfFrame = true;
        LargestDataPacketCounter = HcOp.HcFmInterval.FSLargestDataPacket;
        counterDec = amount * 6 / 7;
        LargestDataPacketFraction += amount * 6 % 7;
        if (LargestDataPacketFraction >= 7) {
            counterDec += 1;
            LargestDataPacketFraction -= 7;
        }
        LargestDataPacketCounter -= counterDec;

        if (HcOp.HcControl.HostControllerFunctionalState == HCFS_UsbOperational) {
            if (DoingPeriodicList) {
                LOG_F(ERROR, "%s: Scheduling Overrun", this->get_name().c_str());
                // FIXME: should ClearPortEnable for the port
                SchedulingOverrun = true;
            }
            SendStartOfFrame();
            NewFrame();
        }
    }
    else {
        frameRemaining = HcOp.HcFmRemaining.FrameRemaining - amount;
        HcOp.HcFmRemaining.FrameRemaining = frameRemaining;
        if (frameRemaining == 0) {
            HcOp.HcFmRemaining.FrameRemainingToggle = HcOp.HcFmInterval.FrameIntervalToggle;
        }
        counterDec = amount * 6 / 7;
        LargestDataPacketFraction += amount * 6 % 7;
        if (LargestDataPacketFraction >= 7) {
            counterDec += 1;
            LargestDataPacketFraction -= 7;
        }
        LargestDataPacketCounter -= counterDec;
    }
}

void USBHostOHCI::TriggerInterrupt() {
    // Is it a valid interrupt for the operational state?
    // If so then interrupt.
    // trigger hw_interrupt.
    if (HcOp.HcInterruptEnable.MasterInterruptEnable) {
        HcInterruptStatus_t interrupts;
        interrupts.val = HcOp.HcInterruptEnable.val & HcOp.HcInterruptStatus.val;
        if (interrupts.val) {
            if (interrupts.OwnershipChange) {
                if (HasSMI) {
                    LOG_F(WARNING, "%s: SMI Interrupt for ownership change", this->get_name().c_str());
                }
                HcOp.HcCommandStatus.OwnershipChangeRequest = false;
            }
            else {
                // trigger PCI interrupt
                switch (HcOp.HcControl.InterruptRouting) {
                    case IR_HostBus:
                        LOG_F(WARNING, "%s: Host Bus Interrupt", this->get_name().c_str());
                        break;
                    case IR_SMI:
                        if (HasSMI) {
                            LOG_F(WARNING, "%s: SMI Interrupt", this->get_name().c_str());
                        }
                        else {
                            LOG_F(WARNING, "%s: SMI Interrupt ignored", this->get_name().c_str());
                        }
                        break;
                }
            }
        } // normal interrupt
    } // if MasterInterruptEnable
}

void USBHostOHCI::ServiceLists() {
    // FIXME: finish me

    LOG_F(WARNING, "%s: [ ServiceLists", this->get_name().c_str());

    uint32_t ed;
    EndpointDescriptor_t *edh;

    uint32_t end_bit_time = HcOp.HcPeriodicStart.PeriodicStart;

    int numProcessedEds = 0;

    while (1) {
        while (HcOp.HcFmRemaining.FrameRemaining > end_bit_time) {

            if (CurrentNonPeriodicList == ListType_Control) {

                if (FrameControl.ControlListEnable) {
                    if (HcOp.HcControlCurrentED == 0) {
                        if (HcOp.HcCommandStatus.ControlListFilled) {
                            HcOp.HcControlCurrentED = HcOp.HcControlHeadED;
                            HcOp.HcCommandStatus.ControlListFilled = false;
                        }
                    }
                    if ((ed = HcOp.HcControlCurrentED)) {
                        edh = (EndpointDescriptor_t *)mmu_get_dma_mem(ed, sizeof(*edh));
                        LOG_F(WARNING, "%s: [ Control Ed %08x", this->get_name().c_str(), ed);
                        ServiceEd(ed, *edh, ListType_Control);
                        numProcessedEds++;
                        HcOp.HcControlCurrentED = READ_DWORD_LE_A(&edh->NextED) & ~15;
                        ProcessedNonemptyControlEDs++;
                        if (ProcessedNonemptyControlEDs > HcOp.HcControl.ControlBulkServiceRatio) {
                            CurrentNonPeriodicList = ListType_Bulk;
                        }
                        LOG_F(WARNING, "%s: ] Control Ed %08x", this->get_name().c_str(), ed);
                    } // if HcControlCurrentED
                    else {
                        CurrentNonPeriodicList = ListType_Bulk;
                    }
                } // if ControlListEnable
                else {
                    CurrentNonPeriodicList = ListType_Bulk;
                }
            } // if ListType_Control

            if (CurrentNonPeriodicList == ListType_Bulk) {
                ProcessedNonemptyControlEDs = 0;

                if (FrameControl.BulkListEnable) {
                    if (HcOp.HcBulkCurrentED == 0) {
                        if (HcOp.HcCommandStatus.BulkListFilled) {
                            HcOp.HcBulkCurrentED = HcOp.HcBulkHeadED;
                            HcOp.HcCommandStatus.BulkListFilled = false;
                        }
                    }
                    if ((ed = HcOp.HcBulkCurrentED)) {
                        edh = (EndpointDescriptor_t *)mmu_get_dma_mem(ed, sizeof(*edh));
                        LOG_F(WARNING, "%s: [ Bulk Ed %08x", this->get_name().c_str(), ed);
                        ServiceEd(ed, *edh, ListType_Bulk);
                        numProcessedEds++;
                        HcOp.HcBulkCurrentED = READ_DWORD_LE_A(&edh->NextED) & ~15;
                        CurrentNonPeriodicList = ListType_Control;
                        LOG_F(WARNING, "%s: ] Bulk Ed %08x", this->get_name().c_str(), ed);
                    } // if HcBulkCurrentED
                    else {
                        CurrentNonPeriodicList = ListType_Control;
                    }
                } // if BulkListEnable
                else {
                    CurrentNonPeriodicList = ListType_Control;
                }
            } // if ListType_Bulk

            if (numProcessedEds == 0) {
                break;
            }
        } // while HcOp.HcFmRemaining.FrameRemaining > end_bit_time

        if (end_bit_time == 0) {
            break;
        }
        end_bit_time = 0;

        // FIXME: must finish before remaining is 0
        // periodic lists
        if (FrameControl.PeriodicListEnable) {
            DoingPeriodicList = true;
            // The Host Controller Driver ensures that all Interrupt Endpoint Descriptors are placed
            // on the list in front of any Isochronous Endpoint Descriptors
            ed = READ_DWORD_LE_A(&hcca->HccaInterrruptTable[HcOp.HcFmNumber.FrameNumber & 31]);
            while (ed) {
                LOG_F(WARNING, "%s: [ Periodic Ed %08x", this->get_name().c_str(), ed);
                edh = (EndpointDescriptor_t *)mmu_get_dma_mem(ed, sizeof(*edh));
                // FIXME: does this auto actually work to read Format ?
                auto &ed0 = (ed0_t&)READ_DWORD_LE_A(&edh->ed0);
                if (ed0.Format == FormatIsochronous && !FrameControl.IsochronousEnable) {
                    LOG_F(WARNING, "%s: ] Periodic Ed %08x IsochronousEnable disabled", this->get_name().c_str(), ed);
                    break;
                }
                ServiceEd(ed, *edh, ListType_Periodic);
                HcOp.HcPeriodCurrentED = ed;
                ed = READ_DWORD_LE_A(&edh->NextED) & ~15;
                LOG_F(WARNING, "%s: ] Periodic Ed %08x", this->get_name().c_str(), ed);
            }
            DoingPeriodicList = false;
        } // if PeriodicListEnable
    } // while

    LOG_F(WARNING, "%s: ] ServiceLists", this->get_name().c_str());
} // ServiceLists

void USBHostOHCI::ServiceEd(uint32_t ed, EndpointDescriptor_t &edh, ListType_t list_type) {
    auto &ed0 = (ed0_t&)READ_DWORD_LE_A(&edh.ed0);
    auto &ed2 = (ed2_t&)READ_DWORD_LE_A(&edh.ed2);
    if (ed0.sKip || ed2.Halted) {
        return;
    }

    uint32_t td = READ_DWORD_LE_A(&edh.TDQueueHeadPointer) & ~15;
    if (td == (READ_DWORD_LE_A(&edh.TDQueueTailPointer) & ~15)) {
        return;
    }

    // FIXME: When the D field of an ED is 10b (IN), the Host Controller may issue an IN
    //        token to the specified endpoint after it determines that HeadP and TailP are
    //        not the same. This indicates that a buffer exists for the data and that
    //        input of the endpoint data may occur in parallel with the HC’s access of the
    //        TD which defines the memory buffer.

    if (list_type == ListType_Periodic) {
        if (ed0.Format == FormatIsochronous) {
            IsochronousTransferDescriptor_t tds;
            IsochronousTransferDescriptor_t *tdh;

            tdh = (IsochronousTransferDescriptor_t *)mmu_get_dma_mem(td, sizeof(*tdh));
            ServiceTdIsochronous(edh, td, *tdh);
            return;
        }
    }

    if (list_type == ListType_Control) {
        HcOp.HcCommandStatus.ControlListFilled = true;
    }
    else if (list_type == ListType_Control) {
        HcOp.HcCommandStatus.BulkListFilled = true;
    }

    GeneralTransferDescriptor_t *tdh;
    tdh = (GeneralTransferDescriptor_t *)mmu_get_dma_mem(td, sizeof(*tdh));
    ServiceTdGeneral(edh, td, *tdh);
    // FIXME: The Halted bit is set by the Host Controller when it encounters an error in processing a TD.
    //        When the TD in error is moved to the Done Queue, the Host Controller updates HeadP and sets
    //        the Halted bit, causing the Host Controller to skip the ED until Halted is cleared.
}

void USBHostOHCI::ServiceTdGeneral(EndpointDescriptor_t &edh, uint32_t td, GeneralTransferDescriptor_t &tdh) {
    // FIXME: Implement 6.4.4.2 Packet Address and Size Calculation
    uint32_t buffer_end;
    uint32_t buffer1_start = READ_DWORD_LE_A(&tdh.CurrentBufferPointer);
    uint32_t buffer1_size;
    uint8_t *data1;
    uint32_t buffer2_start;
    uint32_t buffer2_size;
    uint8_t *data2;
    uint32_t total_size;
    bool time_available;

    auto &ed0 = (ed0_t&)READ_DWORD_LE_A(&edh.ed0);
    auto &gtd0 = (gtd0_t&)READ_DWORD_LE_A(&tdh.gtd0);
    bool out = ed0.Direction == DirectionOut || ((ed0.Direction == DirectionFromTD0
        || ed0.Direction == DirectionFromTD3) && (gtd0.DirectionPID == DirectionPIDOut
        || gtd0.DirectionPID == DirectionPIDSetup));

    if (buffer1_start) {
        buffer_end = READ_DWORD_LE_A(&tdh.BufferEnd);

        if ((buffer_end >= buffer1_start) && ((buffer_end & ~0xfff) == (buffer1_start & ~0xfff))) {
            buffer1_size = buffer_end - buffer1_start + 1;
            buffer2_start = 0;
            buffer2_size = 0;
            total_size = buffer1_size;
        }
        else {
            buffer1_size = 0x1000 - (buffer1_start & 0xfff);
            buffer2_start = buffer_end & ~0xfff;
            buffer2_size = (buffer_end & 0xfff) + 1;
            total_size = buffer1_size + buffer2_size;
        }

        if (out) {
            uint32_t max_size = ed0.MaximumPacketSize;
            if (total_size > max_size) {
                if (buffer1_size > max_size) {
                    buffer1_size = max_size;
                    buffer2_start = 0;
                }
                else {
                    buffer2_size = max_size - buffer1_size;
                }
                total_size = max_size;
            }
        }

        data1 = mmu_get_dma_mem(buffer1_start, buffer1_size);
        if (buffer2_start) {
            data2 = mmu_get_dma_mem(buffer2_start, buffer2_size);
        }
        else {
            data2 = 0;
        }

        time_available = (
            (ed0.Speed == SpeedFull && total_size * 8 <= LargestDataPacketCounter)
            || (ed0.Speed == SpeedLow && HcOp.HcFmRemaining.FrameRemaining >= HcOp.HcLSThreshold.LSThreshold)
        );
    }
    else {
        // A value of 0 indicates a zero-length data packet or that all bytes have been transferred.
        buffer_end = 0;
        buffer1_size = 0;
        buffer2_start = 0;
        buffer2_size = 0;
        data1 = NULL;
        data2 = NULL;
        time_available = true;
    }

    // FIXME: PerformSOFCheck here

    if (time_available) {
        uint32_t bytes_transmitted;

        ConditionCode_t condition_code;
        bool ACK;
        bool NAK;

        if (out) {
            TransmitPacket(edh, tdh, data1, buffer1_size, data2, buffer2_size, bytes_transmitted, ACK, NAK, condition_code);
        }
        else {
            ReceivePacket(edh, tdh, data1, buffer1_size, data2, buffer2_size, bytes_transmitted, ACK, NAK, condition_code);
            // FIXME: Do BufferRounding check here
        }

        // 6.4.4.5.1 General Transfer Descriptor Status Writeback

        if (!NAK) {
            if (ACK || NAK) { // FIXME: does NAK need to set DataToggle? The spec is contradictory.
                // handle ACK or NAK
                gtd0.DataToggle ^= 1;
                gtd0.DataToggle |= 2;
            }

            if (ACK || condition_code != CcNoError) {
                if (condition_code != CcDataToggleMismatch) {
                    if (bytes_transmitted < buffer1_size) {
                        buffer1_start += bytes_transmitted;
                    }
                    else {
                        buffer1_start = (buffer2_start ? buffer2_start : buffer1_start) + (bytes_transmitted - buffer1_size);
                    }

                    // FIXME: should set to 0 if completed?

                    WRITE_DWORD_LE_A(&tdh.CurrentBufferPointer, buffer1_start);
                }
            }

            if (condition_code != CcNoError) {
                gtd0.ErrorCount++;
                if (gtd0.ErrorCount > 2)
                    gtd0.ConditionCode = condition_code;
            }
            else {
                gtd0.ConditionCode = condition_code;
            }

            WRITE_DWORD_LE_A(&tdh.gtd0.val, gtd0.val);

            // FIXME: buffer1_start == buffer_end doesn't make sense if buffer1_start is to point at next byte to be transmitted
            if (gtd0.ErrorCount > 2 || buffer1_start == buffer_end) {
                RetireTd(edh, td, tdh);
            }
        }
    }
}

void USBHostOHCI::ServiceTdIsochronous(EndpointDescriptor_t &edh, uint32_t td, IsochronousTransferDescriptor_t &tdh) {
    // FIXME: Implement 6.4.4.1 Isochronous Relative Frame Number Calculation
    // FIXME: Implement 6.4.4.2 Packet Address and Size Calculation
    // FIXME: 6.4.4.5.2 Isochronous Transfer Descriptor Status Writeback
    // FIXME: finish this
}

bool USBHostOHCI::TransmitPacket(
    EndpointDescriptor_t &edh, GeneralTransferDescriptor_t &tdh, uint8_t *data1,
    uint32_t size1, uint8_t *data2, uint32_t size2, uint32_t &bytes_transmitted,
    bool &ACK, bool &NAK, ConditionCode_t &condition_code
)  {
    LOG_F(WARNING, "%s: [ TransmitPacket size:%08x", this->get_name().c_str(), size1 + size2);
    ACK = true;
    NAK = true;
    condition_code = CcNoError;
    // FIXME: do it
    // FIXME: DecrementFrameRemaining(time);
    LOG_F(WARNING, "%s: ] TransmitPacket", this->get_name().c_str());
    // FIXME: return false if error
    return true;
}

bool USBHostOHCI::ReceivePacket(
    EndpointDescriptor_t &edh, GeneralTransferDescriptor_t &tdh, uint8_t *data1,
    uint32_t size1, uint8_t *data2, uint32_t size2, uint32_t &bytes_transmitted,
    bool &ACK, bool &NAK, ConditionCode_t &condition_code
) {
    LOG_F(WARNING, "%s: [ ReceivePacket size:%08x", this->get_name().c_str(), size1 + size2);
    ACK = true;
    NAK = true;
    condition_code = CcNoError;
    // FIXME: do it
    // FIXME: DecrementFrameRemaining(time);
    LOG_F(WARNING, "%s: ] ReceivePacket", this->get_name().c_str());
    // FIXME: return false if error
    return true;
}

void USBHostOHCI::RetireTd(EndpointDescriptor_t &edh, uint32_t td, GeneralTransferDescriptor_t &tdh) {
    // dequeue the Transfer Descriptor
    uint32_t next_td = READ_DWORD_LE_A(&tdh.NextTD) & ~15;
    WRITE_DWORD_LE_A(&edh.TDQueueHeadPointer, next_td | (READ_DWORD_LE_A(&edh.TDQueueHeadPointer) & 15));

    // enque the Transfer Descriptor to the Done Queue
    WRITE_DWORD_LE_A(&tdh.NextTD, HcOp.HcDoneHead & ~15);
    HcOp.HcDoneHead = td & ~15;

    // update toggleCarry and Halted
    auto &gtd0 = (gtd0_t&)READ_DWORD_LE_A(&tdh.gtd0);
    auto &ed2 = (ed2_t&)READ_DWORD_LE_A(&edh.ed2);
    ed2.toggleCarry = gtd0.DataToggle;
    if (gtd0.ErrorCount > 2) {
        ed2.Halted = true;
    }
    WRITE_DWORD_LE_A(&edh.ed2, ed2.val);

    if (gtd0.ErrorCount > 2) {
        // 6.4.4.6 Transfer Descriptor Retirement
        DoneQueueInterruptCounter = 0;
    }
    else if (gtd0.DelayInterrupt != 7) {
        if (gtd0.DelayInterrupt >= DoneQueueInterruptCounter) {
            // In this case, another Transfer Descriptor already on the Done Queue
            // requires an interrupt earlier than the Transfer Descriptor being retired
        }
        else {
            // In this case, the Transfer Descriptor being retired requires an interrupt
            // earlier than all of the Transfer Descriptors currently on the Done Queue
            DoneQueueInterruptCounter = gtd0.DelayInterrupt;
        }
    }
}

void USBHostOHCI::ConnectDevice(int port) {
    // FIXME: connect a USB device
    HcOp.HcRhPortStatus[port].CurrentConnectStatus = true;
    HcOp.HcRhPortStatus[port].ConnectStatusChange = true;
    HcOp.HcRhPortStatus[port].PortSuspendStatusChange = true;

    // FIXME: is t a low speed or full speed device?
    HcOp.HcRhPortStatus[port].LowSpeedDeviceAttached = true;

    RootHubStatusChange = true;
    CheckWakeup();
}

void USBHostOHCI::DisconnectDevice(int port) {
    if (!(HcOp.HcRhDescriptorB.DeviceRemovable & (1 << (port + 1)))) {
        LOG_F(WARNING, "%s: port#%d Disconnecting a non-removable device", this->get_name().c_str(), port + 1);
    }
    // FIXME: disconnect a USB device
    HcOp.HcRhPortStatus[port].CurrentConnectStatus = false;
    HcOp.HcRhPortStatus[port].ConnectStatusChange = true;
    HcOp.HcRhPortStatus[port].PortSuspendStatusChange = true;
    RootHubStatusChange = true;
    ClearPortEnable(port);
    CheckWakeup();
}

void USBHostOHCI::CheckWakeup() {
    if (HcOp.HcRhStatus.DeviceRemoteWakeupEnable) {
        Resume();
    }
}

void USBHostOHCI::Resume() {
    // FIXME: upstream resume signal or connect/disconnect detection at a port
    // This bit is not set when HCD sets the HCFS_UsbResume state.


    // PortSuspendStatusChange
    // This bit is set when the full resume sequence has been completed. This sequence includes:
    // - the 20-s resume pulse,
    // - LS EOP,
    // - and 3-ms resychronization delay.
    // 0 = resume is not completed 1 = resume completed

    ResumeDetected = true;
}

void USBHostOHCI::ClearGlobalPower() {
    for (int port = 0; port < 15; port++) {
        if ((
                HcOp.HcRhDescriptorA.PowerSwitchingMode == PSM_AllPorts
                || !(HcOp.HcRhDescriptorB.PortPowerControlMask & (1 << (port + 1)))
            ) && HcOp.HcRhPortStatus[port].PortPowerStatus
        ) {
            HcOp.HcRhPortStatus[port].PortPowerStatus = false;
            ClearPortEnable(port);
            RootHubStatusChange = true;
        }
    }
}

void USBHostOHCI::SetGlobalPower() {
    for (int port = 0; port < 15; port++) {
        if ((
                HcOp.HcRhDescriptorA.PowerSwitchingMode == PSM_AllPorts
                 || !(HcOp.HcRhDescriptorB.PortPowerControlMask & (1 << (port + 1)))
            ) && !HcOp.HcRhPortStatus[port].PortPowerStatus
        ) {
            HcOp.HcRhPortStatus[port].PortPowerStatus = true;
            RootHubStatusChange = true;
        }
    }
}

void USBHostOHCI::ClearRemoteWakeupEnable() {
    if (HcOp.HcRhStatus.DeviceRemoteWakeupEnable) {
        HcOp.HcRhStatus.DeviceRemoteWakeupEnable = false;
        RootHubStatusChange = true;
    }
}

void USBHostOHCI::SetRemoteWakeupEnable() {
    if (!HcOp.HcRhStatus.DeviceRemoteWakeupEnable) {
        HcOp.HcRhStatus.DeviceRemoteWakeupEnable = true;
        RootHubStatusChange = true;
    }
}

void USBHostOHCI::SetOverCurrentIndicator(bool v) {
    LOG_F(ERROR, "%s: SetOverCurrentIndicator", this->get_name().c_str());
    if (v != HcOp.HcRhStatus.OverCurrentIndicator) {
        HcOp.HcRhStatus.OverCurrentIndicator = v;
        HcOp.HcRhStatus.OverCurrentIndicatorChange = true;
        RootHubStatusChange = true;
        if (v) {
            ClearPortEnableAll();
        }
    }
}

void USBHostOHCI::ClearPortEnable(int port) {
    // FIXME: postpone status change until transaction completes
    if (false != HcOp.HcRhPortStatus[port].PortEnableStatus) {
        HcOp.HcRhPortStatus[port].PortEnableStatus = false;
        RootHubStatusChange = true;
    }
}

void USBHostOHCI::SetPortEnable(int port) {
    // FIXME: postpone status change until transaction completes
    if (true != HcOp.HcRhPortStatus[port].PortEnableStatus) {
        if (HcOp.HcRhPortStatus[port].CurrentConnectStatus) {
            HcOp.HcRhPortStatus[port].PortEnableStatus = true;
            RootHubStatusChange = true;
        }
        else {
            HcOp.HcRhPortStatus[port].ConnectStatusChange = true;
            RootHubStatusChange = true;
        }
    }
}

void USBHostOHCI::ClearPortEnableAll() {
    for (int port = 0; port < 15; port++) {
        ClearPortEnable(port);
    }
}

void USBHostOHCI::SetSuspendStatus(int port) {
    if (true != HcOp.HcRhPortStatus[port].PortSuspendStatus) {
        if (HcOp.HcRhPortStatus[port].CurrentConnectStatus) {
            HcOp.HcRhPortStatus[port].PortSuspendStatus = true;


            // FIXME: PortSuspendStatusChange is set at the end of the resume interval which then clears PortSuspendStatus
            // HcOp.HcRhPortStatus[port].PortSuspendStatusChange = true;
            RootHubStatusChange = true;
        }
        else {
            HcOp.HcRhPortStatus[port].ConnectStatusChange = true;
            RootHubStatusChange = true;
        }
    }
}

void USBHostOHCI::ClearSuspendStatus(int port) {
    if (false != HcOp.HcRhPortStatus[port].PortSuspendStatus) {
        // FIXME: initiate RESUME
        HcOp.HcRhPortStatus[port].PortSuspendStatus = false;
        RootHubStatusChange = true;
    }
}

void USBHostOHCI::SetPortReset(int port) {
    if (true != HcOp.HcRhPortStatus[port].PortResetStatus) {
        if (HcOp.HcRhPortStatus[port].CurrentConnectStatus) {
            HcOp.HcRhPortStatus[port].PortResetStatus = true;
            RootHubStatusChange = true;
            // FIXME: finish this
            // at completion of a port reset
            // after 10-ms port reset signal
            //HcOp.HcRhPortStatus[port].PortResetStatusChange = true;
            // HcOp.HcRhPortStatus[port].PortSuspendStatusChange = false;
            //HcOp.HcRhPortStatus[port].PortResetStatus = false;
            //ClearSuspendStatus(port);
            //SetPortEnable(port);

        }
        else {
            HcOp.HcRhPortStatus[port].ConnectStatusChange = true;
            RootHubStatusChange = true;
        }
    }
}

void USBHostOHCI::SetPortPower(int port) {
    if (HcOp.HcRhPortStatus[port].PortPowerStatus) {
        LOG_F(ERROR, "%s: SetPortPower ignored because already set", this->get_name().c_str());
    }
    else if (HcOp.HcRhDescriptorA.PowerSwitchingMode == PSM_AllPorts) {
        LOG_F(ERROR, "%s: SetPortPower ignored because global switching", this->get_name().c_str());
    }
    else if (!(HcOp.HcRhDescriptorB.PortPowerControlMask & (1 << (port + 1)))) {
        LOG_F(ERROR, "%s: SetPortPower ignored because port masked", this->get_name().c_str());
    }
    else {
        HcOp.HcRhPortStatus[port].PortPowerStatus = true;
        RootHubStatusChange = true;
    }
}

void USBHostOHCI::ClearPortPower(int port) {
    if (!HcOp.HcRhPortStatus[port].PortPowerStatus) {
        LOG_F(ERROR, "%s: ClearPortPower ignored because already clear", this->get_name().c_str());
    }
    else if (HcOp.HcRhDescriptorA.PowerSwitchingMode == PSM_AllPorts) {
        LOG_F(ERROR, "%s: ClearPortPower ignored because global switching", this->get_name().c_str());
    }
    else if (!(HcOp.HcRhDescriptorB.PortPowerControlMask & (1 << (port + 1)))) {
        LOG_F(ERROR, "%s: ClearPortPower ignored because port masked", this->get_name().c_str());
    }
    else {
        HcOp.HcRhPortStatus[port].PortPowerStatus = false;
        HcOp.HcRhPortStatus[port].CurrentConnectStatus = false;
        HcOp.HcRhPortStatus[port].PortEnableStatus = false;
        HcOp.HcRhPortStatus[port].PortSuspendStatus = false;
        HcOp.HcRhPortStatus[port].PortResetStatus = false;
        RootHubStatusChange = true;
    }
}
