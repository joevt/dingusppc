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

#ifndef USB_HOST_OHCI_H
#define USB_HOST_OHCI_H

#include <devices/common/pci/pcidevice.h>

typedef enum {
    HCFS_UsbReset,
    HCFS_UsbResume,
    HCFS_UsbOperational,
    HCFS_UsbSuspend,
} HCFS_t;

typedef enum {
    IR_HostBus,
    IR_SMI,
} InterruptRouting_t;

typedef enum {
    PSM_AllPorts,
    PSM_PerPort,
} PowerSwitchingMode_t;

typedef enum {
    NPS_PowerSwitched,
    NPS_AlwaysPoweredOn,
} NoPowerSwitching_t;

typedef enum {
    DT_NotACompoundDevice,
    DT_CompoundDevice,
} DeviceType_t;

typedef enum {
    OCM_AllPorts,
    OCM_PerPort,
} OverCurrentProtectionMode_t;

typedef enum {
    NOCP_OverCurrentProtected,
    NOCP_NotOverCurrentProtected,
} NoOverCurrentProtection_t;

typedef union {
    struct {
        uint32_t Revision  :  8;
        uint32_t Reserved8 : 24;
    };
    uint32_t val;
} HcRevision_t;

typedef union {
    struct {
        uint32_t           ControlBulkServiceRatio       :  2;
        bool               PeriodicListEnable            :  1;
        bool               IsochronousEnable             :  1; // aka IsochronousListEnable
        bool               ControlListEnable             :  1;
        bool               BulkListEnable                :  1;
        HCFS_t             HostControllerFunctionalState :  2;
        InterruptRouting_t InterruptRouting              :  1;
        bool               RemoteWakeupConnected         :  1;
        bool               RemoteWakeupEnable            :  1;
        uint32_t           Reserved11                    : 21;
    };
    uint32_t val;
} HcControl_t;

typedef union {
    struct {
        bool     HostControllerReset    :  1;
        bool     ControlListFilled      :  1;
        bool     BulkListFilled         :  1;
        bool     OwnershipChangeRequest :  1;
        uint32_t Reserved4              : 12;
        uint32_t SchedulingOverrunCount :  2;
        uint32_t Reserved18             : 14;
    };
    uint32_t val;
} HcCommandStatus_t;

typedef union {
    struct {
        bool     SchedulingOverrun   :  1;
        bool     WritebackDoneHead   :  1;
        bool     StartOfFrame        :  1;
        bool     ResumeDetected      :  1;
        bool     UnrecoverableError  :  1;
        bool     FrameNumberOverflow :  1;
        bool     RootHubStatusChange :  1;
        uint32_t Reserved7           : 23;
        bool     OwnershipChange     :  1;
        bool     Reserved31          :  1;
    };
    uint32_t val;
} HcInterruptStatus_t;

typedef union {
    struct {
        bool     SchedulingOverrunEnable   :  1;
        bool     WritebackDoneHeadEnable   :  1;
        bool     StartOfFrameEnable        :  1;
        bool     ResumeDetectedEnable      :  1;
        bool     UnrecoverableErrorEnable  :  1;
        bool     FrameNumberOverflowEnable :  1;
        bool     RootHubStatusChangeEnable :  1;
        uint32_t Reserved7_2               : 23;
        bool     OwnershipChangeEnable     :  1;
        bool     MasterInterruptEnable     :  1;
    };
    uint32_t val; // also used for reading HcInterruptDisable; w:EnableInterrupt
} HcInterruptEnable_t;

typedef union {
    struct {
        bool     SchedulingOverrunDisable   :  1;
        bool     WritebackDoneHeadDisable   :  1;
        bool     StartOfFrameDisable        :  1;
        bool     ResumeDetectedDisable      :  1;
        bool     UnrecoverableErrorDisable  :  1;
        bool     FrameNumberOverflowDisable :  1;
        bool     RootHubStatusChangeDisable :  1;
        uint32_t Reserved7_3                : 23;
        bool     OwnershipChangeDisable     :  1;
        bool     MasterInterruptDisable     :  1;
    };
    uint32_t val; // same as HcInterruptEnable; w:DisableInterrupt
} HcInterruptDisable_t;

typedef union {
    struct {
        uint32_t FrameInterval       : 14; // 12-MHz bit times; This is usually 115999 so that a frame is 1 ms
        uint32_t Reserved            :  2;
        uint32_t FSLargestDataPacket : 15;
        uint32_t FrameIntervalToggle :  1;
    };
    uint32_t val;
} HcFmInterval_t;

typedef union {
    struct {
        uint32_t FrameRemaining       : 14; // 12-MHz bit times
        uint32_t Reserved14           : 17;
        uint32_t FrameRemainingToggle :  1;
    };
    uint32_t val;
} HcFmRemaining_t;

typedef union {
    struct {
        uint32_t FrameNumber : 16;
        uint32_t Reserved16  : 16;
    };
    uint32_t val;
} HcFmNumber_t;

typedef union {
    struct {
        uint32_t PeriodicStart : 16;
        uint32_t Reserved16_2  : 16;
    };
    uint32_t val;
} HcPeriodicStart_t;

typedef union {
    struct {
        uint32_t LSThreshold : 12;
        uint32_t Reserved12  : 20;
    };
    uint32_t val;
} HcLSThreshold_t;

typedef union {
    struct {
        uint32_t                    NumberDownstreamPorts     :  8; // 1-15
        PowerSwitchingMode_t        PowerSwitchingMode        :  1;
        NoPowerSwitching_t          NoPowerSwitching          :  1;
        DeviceType_t                DeviceType                :  1; // Always DT_NotACompoundDevice
        OverCurrentProtectionMode_t OverCurrentProtectionMode :  1;
        NoOverCurrentProtection_t   NoOverCurrentProtection   :  1;
        uint32_t                    Reserved13                : 11;
        uint32_t                    PowerOnToPowerGoodTime    :  8;
    };
    uint32_t val;
} HcRhDescriptorA_t;

typedef union {
    struct {
        uint16_t DeviceRemovable      : 16;
        uint16_t PortPowerControlMask : 16;
    };
    uint32_t val;
} HcRhDescriptorB_t;

typedef union {
    struct {
        bool     LocalPowerStatus           :  1; // w:ClearGlobalPower
        bool     OverCurrentIndicator       :  1;
        uint32_t Reserved2                  : 13;
        bool     DeviceRemoteWakeupEnable   :  1; // w:SetRemoteWakeupEnable

        bool     LocalPowerStatusChange     :  1; // w:SetGlobalPower
        bool     OverCurrentIndicatorChange :  1;
        uint32_t Reserved18_2               : 13;
        bool     ClearRemoteWakeupEnable    :  1; // r:-; w:ClearRemoteWakeupEnable
    };
    uint32_t val;
} HcRhStatus_t;

typedef union {
    struct {
        bool     CurrentConnectStatus           :  1; // w:ClearPortEnable
        bool     PortEnableStatus               :  1; // w:SetPortEnable
        bool     PortSuspendStatus              :  1; // w:SetPortSuspend -> SetSuspendStatus
        bool     PortOverCurrentIndicator       :  1; // w:ClearSuspendStatus
        bool     PortResetStatus                :  1; // w:SetPortReset
        uint32_t Reserved5                      :  3;
        bool     PortPowerStatus                :  1; // w:SetPortPower
        bool     LowSpeedDeviceAttached         :  1; // w:ClearPortPower
        uint32_t Reserved10                     :  6;
        bool     ConnectStatusChange            :  1;
        bool     PortEnableStatusChange         :  1;
        bool     PortSuspendStatusChange        :  1;
        bool     PortOverCurrentIndicatorChange :  1;
        bool     PortResetStatusChange          :  1;
        uint32_t Reserved21                     : 11;
    };
    uint32_t val;
} HcRhPortStatus_t;

typedef struct {
    // The Control and Status Partition
    HcRevision_t         HcRevision;
    HcControl_t          HcControl;
    HcCommandStatus_t    HcCommandStatus;
    HcInterruptStatus_t  HcInterruptStatus;
    HcInterruptEnable_t  HcInterruptEnable; // also used for reading HcInterruptDisable; w:EnableInterrupt
    HcInterruptDisable_t HcInterruptDisable; // same as HcInterruptEnable; w:DisableInterrupt

    // Memory Pointer Partition
    uint32_t             HcHCCA;              // HostControllerCommunicationsArea_t
    uint32_t             HcPeriodCurrentED;   // EndpointDescriptor_t // current Isochronous or Interrupt Endpoint Descriptor
    uint32_t             HcControlHeadED;     // EndpointDescriptor_t
    uint32_t             HcControlCurrentED;  // EndpointDescriptor_t
    uint32_t             HcBulkHeadED;        // EndpointDescriptor_t
    uint32_t             HcBulkCurrentED;     // EndpointDescriptor_t
    uint32_t             HcDoneHead;          // GeneralTransferDescriptor_t

    // Frame Counter Partition
    HcFmInterval_t       HcFmInterval;
    HcFmRemaining_t      HcFmRemaining;
    HcFmNumber_t         HcFmNumber;
    HcPeriodicStart_t    HcPeriodicStart;
    HcLSThreshold_t      HcLSThreshold;

    // Root Hub Partition
    HcRhDescriptorA_t    HcRhDescriptorA;
    HcRhDescriptorB_t    HcRhDescriptorB;
    HcRhStatus_t         HcRhStatus;
    HcRhPortStatus_t     HcRhPortStatus[15];
} HcOp_t;

typedef enum {
    DirectionFromTD0,
    DirectionOut,
    DirectionIn,
    DirectionFromTD3,
} Direction_t;

typedef enum {
    SpeedFull,
    SpeedLow
} Speed_t;

typedef enum {
    FormatGeneral, // Control, Bulk, or Interrupt
    FormatIsochronous
} Format_t;

typedef struct {
    uint32_t    FunctionAddress    :  7;
    uint32_t    EndpointNumber     :  4;
    Direction_t Direction          :  2; // Direction_t
    Speed_t     Speed              :  1; // Speed_t
    bool        sKip               :  1;
    Format_t    Format             :  1; // Format_t
    uint32_t    MaximumPacketSize  : 11;
    uint32_t    Reserved           :  5;
} ed0_t;

typedef union {
    struct {
        bool     Halted      :  1; // aka Halt
        bool     toggleCarry :  1; // aka DataToggleCarry
        uint32_t Zeros       :  2;
        uint32_t HeadP       : 28;
    };
    uint32_t val;
} ed2_t;

typedef struct {
    ed0_t       ed0;
    uint32_t    TDQueueTailPointer; // GeneralTransferDescriptor_t or IsochronousTransferDescriptor_t depending on Format
    union {
        ed2_t ed2;
        uint32_t TDQueueHeadPointer; // aka NextTD or NextTransferDescriptor;
                                     // GeneralTransferDescriptor_t or IsochronousTransferDescriptor_t depending on Format
    };
    uint32_t NextED;
} EndpointDescriptor_t;

typedef enum {
    DirectionPIDSetup,
    DirectionPIDOut,
    DirectionPIDIn,
    DirectionPIDReserved,
} DirectionPID_t;

typedef enum : uint16_t {
    CcNoError,
    CcCrc,
    CcBitStuffing,
    CcDataToggleMismatch,
    CcStall,
    CcDeviceNotResponding,
    CcPidCheckFailure,
    CcUnexpectedPid,
    CcDataOverrun,
    CcDataUnderrun,
    Ccreserved10,
    Ccreserved11,
    CcBufferOverrun,
    CcBufferUnderrun,
    CcNotAccessed14,
    CcNotAccessed15,
 } ConditionCode_t;

typedef union {
    struct {
        uint32_t        Reserved0      : 18;
        bool            bufferRounding :  1;
        DirectionPID_t  DirectionPID   :  2;
        uint32_t        DelayInterrupt :  3; // aka InterruptDelay
        uint32_t        DataToggle     :  2; // aka DataToggleControl
        uint32_t        ErrorCount     :  2;
        ConditionCode_t ConditionCode  :  4;
    };
    uint32_t val;
} gtd0_t;

typedef struct {
    gtd0_t          gtd0;
    uint32_t        CurrentBufferPointer;
    uint32_t        NextTD; // GeneralTransferDescriptor_t; aka NextTransferDescriptor
    uint32_t        BufferEnd;
} GeneralTransferDescriptor_t;

typedef struct {
    uint32_t       StartingFrame     : 16;
    uint32_t       Reserved16        :  5;
    uint32_t       DelayInterrupt    :  3;
    uint32_t       FrameCount        :  3;
    uint32_t       Reserved27        :  1;
    uint32_t       ConditionCode     :  4;
} itd0_t;

typedef struct {
    itd0_t        idt0;
    uint32_t       BufferPage0;
    uint32_t       NextTD; // IsochronousTransferDescriptor_t; aka NextTransferDescriptor
    uint32_t       BufferEnd;
    union {
        uint16_t       Offset[8]; // r
        struct {
            uint16_t        SizeOfPacket  : 11;
            uint16_t        Zero          :  1;
            ConditionCode_t ConditionCode :  4;
        } PacketStatusWord[8]; // w
    };
} IsochronousTransferDescriptor_t;

typedef struct {
    uint32_t HccaInterrruptTable[32]; // EndpointDescriptor_t
    uint16_t HccaFrameNumber;
    uint16_t HccaPad1;
    uint32_t HccaDoneHead; // GeneralTransferDescriptor_t
    uint32_t Reserved[29];
    uint32_t Unspecified; // spec only identifies 252 bytes
} HostControllerCommunicationsArea_t;


class USBHostOHCI : public PCIDevice {
public:
    USBHostOHCI(const std::string name);
    ~USBHostOHCI() = default;

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, const AccessDetails details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, const AccessDetails details);

protected:
    void notify_bar_change(int bar_num);

private:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);
    uint32_t aperture_base = 0;

protected:
    // Host Controller Operational Registers

#define PROC_REG(word, reg, type) void ohci_wr_ ## reg(type v);
#define PROC_ADDR(word) void ohci_wr_ ## word(uint32_t v);
#define PROC_REG_PORT(word, reg, type) void ohci_wr_ ## reg(type v, int port);

    PROC_REG(HcRevision, Revision, uint32_t)
    PROC_REG(HcRevision, Reserved8, uint32_t)
    PROC_REG(HcControl, ControlBulkServiceRatio, uint32_t)
    PROC_REG(HcControl, PeriodicListEnable, bool)
    PROC_REG(HcControl, IsochronousEnable, bool)
    PROC_REG(HcControl, ControlListEnable, bool)
    PROC_REG(HcControl, BulkListEnable, bool)
    PROC_REG(HcControl, HostControllerFunctionalState, HCFS_t)
    PROC_REG(HcControl, InterruptRouting, InterruptRouting_t)
    PROC_REG(HcControl, RemoteWakeupConnected, bool)
    PROC_REG(HcControl, RemoteWakeupEnable, bool)
    PROC_REG(HcControl, Reserved11, uint32_t)
    PROC_REG(HcCommandStatus, HostControllerReset, bool)
    PROC_REG(HcCommandStatus, ControlListFilled, bool)
    PROC_REG(HcCommandStatus, BulkListFilled, bool)
    PROC_REG(HcCommandStatus, OwnershipChangeRequest, bool)
    PROC_REG(HcCommandStatus, Reserved4, uint32_t)
    PROC_REG(HcCommandStatus, SchedulingOverrunCount, uint32_t)
    PROC_REG(HcCommandStatus, Reserved18, uint32_t)
    PROC_REG(HcInterruptStatus, SchedulingOverrun, bool)
    PROC_REG(HcInterruptStatus, WritebackDoneHead, bool)
    PROC_REG(HcInterruptStatus, StartOfFrame, bool)
    PROC_REG(HcInterruptStatus, ResumeDetected, bool)
    PROC_REG(HcInterruptStatus, UnrecoverableError, bool)
    PROC_REG(HcInterruptStatus, FrameNumberOverflow, bool)
    PROC_REG(HcInterruptStatus, RootHubStatusChange, bool)
    PROC_REG(HcInterruptStatus, Reserved7, uint32_t)
    PROC_REG(HcInterruptStatus, OwnershipChange, bool)
    PROC_REG(HcInterruptStatus, Reserved31, bool)
    PROC_REG(HcInterruptEnable, SchedulingOverrunEnable, bool)
    PROC_REG(HcInterruptEnable, WritebackDoneHeadEnable, bool)
    PROC_REG(HcInterruptEnable, StartOfFrameEnable, bool)
    PROC_REG(HcInterruptEnable, ResumeDetectedEnable, bool)
    PROC_REG(HcInterruptEnable, UnrecoverableErrorEnable, bool)
    PROC_REG(HcInterruptEnable, FrameNumberOverflowEnable, bool)
    PROC_REG(HcInterruptEnable, RootHubStatusChangeEnable, bool)
    PROC_REG(HcInterruptEnable, Reserved7_2, uint32_t)
    PROC_REG(HcInterruptEnable, OwnershipChangeEnable, bool)
    PROC_REG(HcInterruptEnable, MasterInterruptEnable, bool)
    PROC_REG(HcInterruptDisable, SchedulingOverrunDisable, bool)
    PROC_REG(HcInterruptDisable, WritebackDoneHeadDisable, bool)
    PROC_REG(HcInterruptDisable, StartOfFrameDisable, bool)
    PROC_REG(HcInterruptDisable, ResumeDetectedDisable, bool)
    PROC_REG(HcInterruptDisable, UnrecoverableErrorDisable, bool)
    PROC_REG(HcInterruptDisable, FrameNumberOverflowDisable, bool)
    PROC_REG(HcInterruptDisable, RootHubStatusChangeDisable, bool)
    PROC_REG(HcInterruptDisable, Reserved7_3, uint32_t)
    PROC_REG(HcInterruptDisable, OwnershipChangeDisable, bool)
    PROC_REG(HcInterruptDisable, MasterInterruptDisable, bool)
    PROC_ADDR(HcHCCA)
    PROC_ADDR(HcPeriodCurrentED)
    PROC_ADDR(HcControlHeadED)
    PROC_ADDR(HcControlCurrentED)
    PROC_ADDR(HcBulkHeadED)
    PROC_ADDR(HcBulkCurrentED)
    PROC_ADDR(HcDoneHead)
    PROC_REG(HcFmInterval, FrameInterval, uint32_t)
    PROC_REG(HcFmInterval, Reserved, uint32_t)
    PROC_REG(HcFmInterval, FSLargestDataPacket, uint32_t)
    PROC_REG(HcFmInterval, FrameIntervalToggle, uint32_t)
    PROC_REG(HcFmRemaining, FrameRemaining, uint32_t)
    PROC_REG(HcFmRemaining, Reserved14, uint32_t)
    PROC_REG(HcFmRemaining, FrameRemainingToggle, uint32_t)
    PROC_REG(HcFmNumber, FrameNumber, uint32_t)
    PROC_REG(HcFmNumber, Reserved16, uint32_t)
    PROC_REG(HcPeriodicStart, PeriodicStart, uint32_t)
    PROC_REG(HcPeriodicStart, Reserved16_2, uint32_t)
    PROC_REG(HcLSThreshold, LSThreshold, uint32_t)
    PROC_REG(HcLSThreshold, Reserved12, uint32_t)
    PROC_REG(HcRhDescriptorA, NumberDownstreamPorts, uint32_t)
    PROC_REG(HcRhDescriptorA, PowerSwitchingMode, PowerSwitchingMode_t)
    PROC_REG(HcRhDescriptorA, NoPowerSwitching, NoPowerSwitching_t)
    PROC_REG(HcRhDescriptorA, DeviceType, DeviceType_t)
    PROC_REG(HcRhDescriptorA, OverCurrentProtectionMode, OverCurrentProtectionMode_t)
    PROC_REG(HcRhDescriptorA, NoOverCurrentProtection, NoOverCurrentProtection_t)
    PROC_REG(HcRhDescriptorA, Reserved13, uint32_t)
    PROC_REG(HcRhDescriptorA, PowerOnToPowerGoodTime, uint32_t)
    PROC_REG(HcRhDescriptorB, DeviceRemovable, uint16_t)
    PROC_REG(HcRhDescriptorB, PortPowerControlMask, uint16_t)
    PROC_REG(HcRhStatus, LocalPowerStatus, bool)
    PROC_REG(HcRhStatus, OverCurrentIndicator, bool)
    PROC_REG(HcRhStatus, Reserved2, uint32_t)
    PROC_REG(HcRhStatus, DeviceRemoteWakeupEnable, bool)
    PROC_REG(HcRhStatus, LocalPowerStatusChange, bool)
    PROC_REG(HcRhStatus, OverCurrentIndicatorChange, bool)
    PROC_REG(HcRhStatus, Reserved18_2, uint32_t)
    PROC_REG(HcRhStatus, ClearRemoteWakeupEnable, bool)
    PROC_REG_PORT(HcRhPortStatus, CurrentConnectStatus, bool)
    PROC_REG_PORT(HcRhPortStatus, PortEnableStatus, bool)
    PROC_REG_PORT(HcRhPortStatus, PortSuspendStatus, bool)
    PROC_REG_PORT(HcRhPortStatus, PortOverCurrentIndicator, bool)
    PROC_REG_PORT(HcRhPortStatus, PortResetStatus, bool)
    PROC_REG_PORT(HcRhPortStatus, Reserved5, uint32_t)
    PROC_REG_PORT(HcRhPortStatus, PortPowerStatus, bool)
    PROC_REG_PORT(HcRhPortStatus, LowSpeedDeviceAttached, bool)
    PROC_REG_PORT(HcRhPortStatus, Reserved10, uint32_t)
    PROC_REG_PORT(HcRhPortStatus, ConnectStatusChange, bool)
    PROC_REG_PORT(HcRhPortStatus, PortEnableStatusChange, bool)
    PROC_REG_PORT(HcRhPortStatus, PortSuspendStatusChange, bool)
    PROC_REG_PORT(HcRhPortStatus, PortOverCurrentIndicatorChange, bool)
    PROC_REG_PORT(HcRhPortStatus, PortResetStatusChange, bool)
    PROC_REG_PORT(HcRhPortStatus, Reserved21, uint32_t)

#undef PROC_REG
#undef PROC_ADDR
#undef PROC_REG_PORT

    typedef enum {
        ListType_Control,
        ListType_Bulk,
        ListType_Periodic,
    } ListType_t;

    uint32_t read_hcop_reg(uint32_t offset);
    HcOp_t HcOp = { 0 };
    HostControllerCommunicationsArea_t *hcca = NULL;
    ListType_t CurrentNonPeriodicList = ListType_Control;
    int ProcessedNonemptyControlEDs = 0;
    int LargestDataPacketCounter = 0;
    int LargestDataPacketFraction = 0;
    int DoneQueueInterruptCounter = 0;
    bool DoingPeriodicList = false;
    bool SchedulingOverrun = false;
    bool StartOfFrame = false;
    bool ResumeDetected = false;
    bool UnrecoverableError = false;
    bool FrameNumberOverflow = false;
    bool RootHubStatusChange = false;
    bool OwnershipChange = false;
    bool HasSMI = false;

    HcControl_t          FrameControl  = { 0 }; // for latching at next start of frame
    HcRhDescriptorA_t    RhDescriptorA = { 0 }; // for reset
    HcRhDescriptorB_t    RhDescriptorB = { 0 }; // for reset

    void SetHcFunctionalState(HCFS_t v, bool soft_reset);
    void ResetRegisters(bool soft_reset);
    void BroadcastState(HCFS_t v);
    void HardwareReset();
    void SoftwareReset();
    void RemoteWakeup();
    void NewFrame();
    void IncrementFrameNumber ();
    void SendStartOfFrame();
    void DecrementFrameRemaining(int amount);
    void TriggerInterrupt();
    void ServiceLists();
    void ServiceEd(uint32_t ed, EndpointDescriptor_t &edh, ListType_t list_type);
    void ServiceTdGeneral(EndpointDescriptor_t &edh, uint32_t td, GeneralTransferDescriptor_t &tdh);
    void ServiceTdIsochronous(EndpointDescriptor_t &edh, uint32_t td, IsochronousTransferDescriptor_t &tdh);
    bool TransmitPacket(EndpointDescriptor_t &edh, GeneralTransferDescriptor_t &tdh,
        uint8_t *data1, uint32_t size1, uint8_t *data2, uint32_t size2,
        uint32_t &bytes_transmitted, bool &ACK, bool &NAK, ConditionCode_t &condition_code);
    bool ReceivePacket (EndpointDescriptor_t &edh, GeneralTransferDescriptor_t &tdh,
        uint8_t *data1, uint32_t size1, uint8_t *data2, uint32_t size2,
        uint32_t &bytes_transmitted, bool &ACK, bool &NAK, ConditionCode_t &condition_code);
    void RetireTd(EndpointDescriptor_t &edh, uint32_t td, GeneralTransferDescriptor_t &tdh);
    void CheckWakeup();
    void ConnectDevice(int port);
    void DisconnectDevice(int port);
    void Resume();
    void ClearGlobalPower();
    void SetGlobalPower();
    void ClearRemoteWakeupEnable();
    void SetRemoteWakeupEnable();
    void SetOverCurrentIndicator(bool v);
    void ClearPortEnable(int port);
    void SetPortEnable(int port);
    void ClearPortEnableAll();
    void SetSuspendStatus(int port);
    void ClearSuspendStatus(int port);
    void PortReset(int port);
    void SetPortReset(int port);
    void SetPortPower(int port);
    void ClearPortPower(int port);
};

#endif // USB_HOST_OHCI_H
