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

#include <devices/common/usb/optiohci.h>
#include <devices/deviceregistry.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

OptiOhci::OptiOhci(const std::string &dev_name)
    : USBHostOHCI(dev_name), HWComponent(dev_name)
{
    // set up PCI configuration space header
    /* 00 */ this->vendor_id      = 0x1045; // OPTi Inc.
    /* 02 */ this->device_id      = 0xc861; // 82C861 OHCI USB Host
    /* 06 */ this->status         = 0x0280; // 7:Fast Back-to-Back Capable (optional) 9:DEVSEL medium
    /* 08 */ this->class_rev     |= 0x10;   // rev 10
    /* 2C */ this->subsys_vndr    = 0x1045; // OPTi Inc.
    /* 2E */ this->subsys_id      = 0xc861; // 82C861 OHCI USB Host

    /*
    80881000: 10 01 00 00 84 00 00 00 00 00 00 00 44 00 00 00 :............D...:
    80881010: 00 00 00 00 00 00 00 00 00 2d fd 2f 00 00 00 00 :.........-./....:
    80881020: 80 33 fd 2f 00 00 00 00 00 00 00 00 00 00 00 00 :.3./............:
    80881030: 00 00 00 00 df 2e 74 a7 f3 07 00 80 63 6f 00 00 :......t.....ho..:
    80881040: 30 2a 00 00 28 06 00 00 02 09 00 03 00 00 06 00 :0*..(...........:
    80881050: 00 00 00 00 03 01 00 00 00 01 01 00 00 00 00 00 :................:
    80881100: 00 00 00 00 7f 00 00 00 ff 00 00 00 00 00 00 00 :................:
    80881ff0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 :................:
    */

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

static const DeviceDescription OptiOhci_Descriptor = {
    OptiOhci::create, {}, {}, HWCompType::MMIO_DEV | HWCompType::PCI_DEV
};

REGISTER_DEVICE(OptiOhci, OptiOhci_Descriptor);
