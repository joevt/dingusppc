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

/** PSX Memory Controller definitions. */

#ifndef PSX_MEMCTRL_H
#define PSX_MEMCTRL_H

#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>

#include <cinttypes>
#include <memory>
#include <string>

namespace PsxReg {
    enum {
        Sys_Id          =  0,
        Revision        =  1,
        Sys_Config      =  2,
        ROM_Config      =  3,
        DRAM_Config     =  4,
        DRAM_Refresh    =  5,
        Flash_Config    =  6,
        Page_Mappings_1 =  8,
        Page_Mappings_2 =  9,
        Page_Mappings_3 = 10,
        Page_Mappings_4 = 11,
        Page_Mappings_5 = 12,
        Bus_Timeout     = 13
    };
} // namespace PsxReg

/** Bus (aka CPU) speed constants. */
enum {
    PSX_BUS_SPEED_38 = 0, // bus frequency 38 MHz
    PSX_BUS_SPEED_33 = 1, // bus frequency 33 MHz
    PSX_BUS_SPEED_40 = 2, // bus frequency 40 MHz
    PSX_BUS_SPEED_50 = 3, // bus frequency 50 MHz
};

class PsxCtrl : public MemCtrlBase, public MMIODevice {
public:
    PsxCtrl(const int bridge_num, const std::string name);
    ~PsxCtrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<PsxCtrl>(new PsxCtrl(1, "Psx"));
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    void insert_ram_dimm(int slot_num, uint32_t capacity);
    void map_phys_ram();

private:
    uint32_t    sys_id;
    uint32_t    chip_rev;
    uint32_t    sys_config;
    uint32_t    rom_cfg;
    uint32_t    dram_cfg;
    uint32_t    dram_refresh;
    uint32_t    flash_cfg;
    uint32_t    pages_cfg[5] = {0x88888888,0x88888888,0x88888888,0x88888888,0x88888888};
    uint32_t    bank_size[5] = {};
    std::unique_ptr<uint8_t[]>      dram_ptr = nullptr;
    std::vector<AddressMapEntry*>   ram_map;
};

#endif // PSX_MEMCTRL_H
