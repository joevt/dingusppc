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

#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/nvram.h>
#include <devices/common/ofnvram.h>
#include <devices/deviceregistry.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <loguru.hpp>

/** @file Non-volatile RAM implementation.
 */

using namespace std;

/** the signature for NVRAM backing file identification. */
static char NVRAM_FILE_ID[] = "DINGUSPPCNVRAM";

NVram::NVram(const std::string &dev_name, std::string file_name, uint32_t ram_size)
    : HWComponent(dev_name)
{
    supports_types(ram_size == 256 ? HWCompType::PRAM : HWCompType::NVRAM);

    this->file_name = file_name;
    this->ram_size  = ram_size;

    if (ram_size == 8192)
        this->of_nvram_offset = OF_NVRAM_OFFSET;

    // allocate memory storage and fill it with zeroes
    this->storage = std::unique_ptr<uint8_t[]>(new uint8_t[ram_size] ());

    this->init();
}

NVram::~NVram() {
    this->save();
}

uint8_t NVram::read_byte(uint32_t offset) {
    return (this->storage[offset]);
}

void NVram::write_byte(uint32_t offset, uint8_t val) {
    this->storage[offset] = val;
}

void NVram::prepare_read() {
    if (this->copland_nvram_host) {
        OfConfigHdrAppl *hdr_copland = (OfConfigHdrAppl *)this->copland_nvram_host;
        if (OfConfigAppl::validate_header(*hdr_copland))
            memcpy(this->storage.get(), this->copland_nvram_host, this->ram_size);
    }
}

void NVram::finish_write() {
    if (this->copland_nvram_host) {
        OfConfigHdrAppl *hdr_copland = (OfConfigHdrAppl *)this->copland_nvram_host;
        if (OfConfigAppl::validate_header(*hdr_copland))
            memcpy(this->copland_nvram_host, this->storage.get(), this->ram_size);
    }
}

void NVram::init() {
    char sig[sizeof(NVRAM_FILE_ID)];
    uint16_t data_size;

    ifstream f(this->file_name, ios::in | ios::binary);

    if (f.fail() || !f.read(sig, sizeof(NVRAM_FILE_ID)) ||
        !f.read((char*)&data_size, sizeof(data_size)) ||
        memcmp(sig, NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID)) || data_size != this->ram_size ||
        !f.read((char*)this->storage.get(), this->ram_size)) {
        LOG_F(WARNING, "Could not restore NVRAM content from the given file \"%s\".", this->file_name.c_str());
        memset(this->storage.get(), 0, this->ram_size);
    }

    f.close();
}

void NVram::save() {
    if (is_deterministic) {
        LOG_F(INFO, "Skipping NVRAM write to \"%s\" in deterministic mode", this->file_name.c_str());
        return;
    }

    this->prepare_read();

    ofstream f(this->file_name, ios::out | ios::binary);

    /* write file identification */
    f.write(NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID));
    f.write((char*)&this->ram_size, sizeof(this->ram_size));

    /* write NVRAM content */
    f.write((char*)this->storage.get(), this->ram_size);

    f.close();
}

void NVram::set_copland_nvram(uint32_t phys) {
    MapDmaResult res = mmu_map_dma_mem(phys, this->ram_size, false);
    this->copland_nvram_host = res.host_va;
    OfConfigHdrAppl *hdr_dingus = (OfConfigHdrAppl *)this->storage.get();

    if (OfConfigAppl::validate_header(*hdr_dingus)) {
        if (std::memcmp(this->storage.get(), this->copland_nvram_host, ram_size)) {
            LOG_F(INFO, "DingusPPC overrides Copland NVRAM");
            std::memcpy(this->copland_nvram_host, this->storage.get(), ram_size);
        } else {
            LOG_F(INFO, "DingusPPC and Copland NVRAM are equal");
        }
    } else {
        std::memcpy(this->storage.get(), this->copland_nvram_host, ram_size);
        LOG_F(INFO, "Copland replaces invalid DingusPPC NVRAM");
    }
}

static const DeviceDescription Nvram_Descriptor = {
    NVram::create, {}, {}, HWCompType::NVRAM
};

static const DeviceDescription Pram_Descriptor = {
    NVram::create, {}, {}, HWCompType::PRAM
};

REGISTER_DEVICE(NVRAM, Nvram_Descriptor);
REGISTER_DEVICE(PRAM, Pram_Descriptor);
REGISTER_DEVICE(NVRAMCopland, Nvram_Descriptor);
