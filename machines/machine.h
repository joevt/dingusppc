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

#ifndef MACHINE_H
#define MACHINE_H

#include <devices/common/hwcomponent.h>

class Machine : virtual public HWComponent
{
public:
    Machine() { supports_types(HWCompType::MACHINE); }
    ~Machine() = default;

    // Machine methods

    virtual int initialize(const std::string &dev_name) = 0;

    template <class T>
    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        std::unique_ptr<T> machine = std::unique_ptr<T>(new T());
        if (machine && 0 == machine->initialize(dev_name))
            return machine;
        return nullptr;
    }
};

#endif /* MACHINE_H */
