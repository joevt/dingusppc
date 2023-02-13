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

#include <devices/common/usb/necohci.h>
#include <devices/deviceregistry.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

NecOhci::NecOhci(const std::string &dev_name)
    : USBHostOHCI(dev_name), HWComponent(dev_name)
{
    // set up PCI configuration space header
    /* 00 */ this->vendor_id      = 0x1033; // NEC Corporation
    /* 02 */ this->device_id      = 0x0035; // OHCI USB Controller
    /* 06 */ this->status        |= 0x10; // 4:Capabilities
    /* 08 */ this->class_rev     |= 0x41; // rev 41
    /* 2C */ this->subsys_vndr    = 0x16B8; // Sonnet Technologies, Inc
    /* 2E */ this->subsys_id      = 0x0012;
    /* 34 */ this->cap_ptr        = 0x40;
    /* 3E */ this->min_gnt        = 0x01;
    /* 3F */ this->max_lat        = 0x2A;

    /*
    FIXME: read OHCI registers from USB OHCI of Tempo Trio
    80881000: 10 01 00 00 84 00 00 00 00 00 00 00 44 00 00 00 :............D...:
    80881010: 00 00 00 00 00 00 00 00 00 2d fd 2f 00 00 00 00 :.........-./....:
    80881020: 80 33 fd 2f 00 00 00 00 00 00 00 00 00 00 00 00 :.3./............:
    80881030: 00 00 00 00 df 2e 74 a7 f3 07 00 80 63 6f 00 00 :......t.....ho..:
    80881040: 30 2a 00 00 28 06 00 00 02 09 00 03 00 00 06 00 :0*..(...........:
    80881050: 00 00 00 00 03 01 00 00 00 01 01 00 00 00 00 00 :................:
    80881100: 00 00 00 00 7f 00 00 00 ff 00 00 00 00 00 00 00 :................:
    80881ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 :................:
    */

    // FIXME: read OHCI registers from USB OHCI of Tempo Trio and update these:

    // normally 2 ports. We can set this to 15 and Open Firmware will probe all the ports.
    this->RhDescriptorA.NumberDownstreamPorts     = 2;

    this->RhDescriptorA.PowerSwitchingMode        = PSM_PerPort;
    this->RhDescriptorA.NoPowerSwitching          = NPS_PowerSwitched;
    this->RhDescriptorA.OverCurrentProtectionMode = OCM_PerPort;
    this->RhDescriptorA.NoOverCurrentProtection   = NOCP_OverCurrentProtected;
    this->RhDescriptorA.PowerOnToPowerGoodTime    = 3;

    this->RhDescriptorB.DeviceRemovable           = 0x0000; // for port 1..15. // 0 bit = device removeable.
    this->RhDescriptorB.PortPowerControlMask      = 0x0006; // for port 1..15. // Set to 6 for port 1 and port 2 by Open Firmware.
}

uint32_t NecOhci::pci_cfg_read(uint32_t reg_offs, const AccessDetails details)
{
    switch (reg_offs) {
        case 0x40: return 0x7E020001;
            // +0: 01 = PCI Power Management
            // +1: 00 = next capability
            // +2: 7E02 = 01111 1 1 000 0 0 0 010
            //          : Power Management version 2; Flags: PMEClk- DSI- D1+ D2+ AuxCurrent=0mA PME(D0+,D1+,D2+,D3hot+,D3cold-)
        case 0xE0: return 0xC4303305;
        default  : return PCIDevice::pci_cfg_read(reg_offs, details);
    }
}

static const DeviceDescription NecOhci_Descriptor = {
    NecOhci::create, {}, {}, HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(NecOhci, NecOhci_Descriptor);
