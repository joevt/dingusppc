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

#ifndef NVRAM_H
#define NVRAM_H

#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>
#include <string>

/** @file Non-volatile RAM emulation.

    It implements a non-volatile random access storage whose content will be
    automatically saved to and restored from the dedicated file.
 */

class NVram : virtual public HWComponent {
public:
    NVram(std::string file_name, uint32_t ram_size);
    ~NVram();

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<NVram>(new NVram("nvram.bin", 8192));
    }

    static std::unique_ptr<HWComponent> create_pram() {
        return std::unique_ptr<NVram>(new NVram("pram.bin", 256));
    }

    static std::unique_ptr<HWComponent> create_nvram_copland() {
        return std::unique_ptr<NVram>(new NVram("nvram_copland.bin", 2048));
    }

    void set_copland_nvram(uint32_t phys);
    void prepare_read();
    void finish_write();

    uint8_t read_byte(uint32_t offset);
    void write_byte(uint32_t offset, uint8_t value);
    uint32_t get_of_nvram_offset() { return of_nvram_offset; }

private:
    std::string file_name; // file name for the backing file
    uint16_t    ram_size;  // NVRAM size
    std::unique_ptr<uint8_t[]>  storage;
    uint32_t of_nvram_offset = 0;
    uint8_t* copland_nvram_host = nullptr;

    void init();
    void save();
};

#endif /* NVRAM_H */
