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
            LOG_F(WARNING, "%s: IntSrc:%d and IntSrc:%d use the same irq_id", this->name.c_str(),
                src_id, this->irq_id_to_int_src[irq_id]);
        }
        return irq_id;
    }
    ABORT_F("%s: unknown interrupt source %d", this->get_name().c_str(), src_id);
}

void InterruptCtrl::add_intsrc(IntSrc src_id, uint64_t irq_id) {
    this->int_src_to_irq_id[src_id] = irq_id;
}
