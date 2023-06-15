/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <string>

class USBDevice : public HWComponent {
public:
    USBDevice(std::string name);
    ~USBDevice() = default;

protected:

private:

protected:

};

#endif // USB_DEVICE_H
