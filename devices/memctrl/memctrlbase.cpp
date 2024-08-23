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

#include <devices/memctrl/memctrlbase.h>
#include <devices/common/mmiodevice.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <loguru.hpp>

MemCtrlBase::~MemCtrlBase() {
    for (auto& entry : address_map) {
        if (entry)
            delete(entry);
    }

    for (auto& reg : mem_regions) {
        if (reg)
            delete (reg);
    }
    this->mem_regions.clear();
    this->address_map.clear();
}

static std::string get_type_str(uint32_t type) {
    std::string str;
    if (type & RT_ROM) {
        if (str.length())
            str += ",";
        str += "ROM";
    }
    if (type & RT_RAM) {
        if (str.length())
            str += ",";
        str += "RAM";
    }
    if (type & RT_MMIO) {
        if (str.length())
            str += ",";
        str += "MMIO";
    }
    if (type & RT_MIRROR) {
        if (str.length())
            str += ",";
        str += "MIRROR";
    }
    return str;
}


static std::string get_entry_str(const AddressMapEntry* entry) {
    std::string str;
    char buf[50];
    if (entry) {
        snprintf(buf, sizeof(buf), "0x%08X..0x%08X", entry->start, entry->end);
        str = std::string(buf) + " (" + get_type_str(entry->type) + ")";
        if (entry->devobj)
            str += " (" + entry->devobj->get_name() + ")";
        if (entry->type & RT_MIRROR) {
            snprintf(buf, sizeof(buf), " -> 0x%08X..0x%08X", entry->mirror,
                entry->mirror + entry->end - entry->start);
            str += std::string(buf);
        }
    } else {
        str = "null";
    }
    return str;
}


static inline bool match_mem_entry(const AddressMapEntry* entry,
                                   const uint32_t start, const uint32_t end,
                                   MMIODevice* dev_instance)
{
    return start == entry->start && end == entry->end &&
        (!dev_instance || dev_instance == entry->devobj);
}


AddressMapEntry* MemCtrlBase::find_range(uint32_t addr) {
    for (auto& entry : address_map) {
        if (addr >= entry->start && addr <= entry->end)
            return entry;
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_exact(uint32_t addr, uint32_t size,
                                               MMIODevice* dev_instance)
{
    if (size) {
        const uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (match_mem_entry(entry, addr, end, dev_instance))
                return entry;
        }
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_contains(uint32_t addr, uint32_t size) {
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (addr >= entry->start && end <= entry->end)
                return entry;
        }
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_overlaps(uint32_t addr, uint32_t size) {
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (end >= entry->start && addr <= entry->end)
                return entry;
        }
    }

    return nullptr;
}


bool MemCtrlBase::is_range_free(uint32_t addr, uint32_t size) {
    bool result = true;
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (addr == entry->start && end == entry->end) {
                LOG_F(WARNING, "range already exists as mem region %s",
                    get_entry_str(entry).c_str()
                );
                result = false;
            }
            else if (addr >= entry->start && end <= entry->end) {
                LOG_F(WARNING, "range 0x%X..0x%X already exists in mem region %s",
                    addr, end, get_entry_str(entry).c_str()
                );
                result = false;
            }
            else if (end >= entry->start && addr <= entry->end) {
                LOG_F(ERROR, "range 0x%X..0x%X overlaps mem region %s",
                    addr, end, get_entry_str(entry).c_str()
                );
                result = false;
            }
        }
    }
    return result;
}


AddressMapEntry* MemCtrlBase::add_mem_region(uint32_t start_addr, uint32_t size,
                                             uint32_t dest_addr,  uint32_t type,
                                             uint8_t  *mem_ptr = nullptr)
{
    AddressMapEntry *entry;

    // bail out if a memory region for the given range already exists
    if (!is_range_free(start_addr, size))
        return nullptr;

    if (!mem_ptr) {
        mem_ptr = (uint8_t*)(new uint64_t[(size + 7) / 8]()); // allocate and clear to zero
        if (((size_t)mem_ptr & 7) != 0)
            ABORT_F("not aligned!");
    }

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = type;
    entry->devobj  = nullptr;
    entry->mem_ptr = mem_ptr;

    // Keep address_map sorted, that way the RAM region (which starts at 0 and
    // is most often requested) will be found by find_range on the first
    // iteration.
    this->address_map.insert(
        std::upper_bound(
            this->address_map.begin(),
            this->address_map.end(),
            entry,
            [](const auto& lhs, const auto& rhs) {
                return lhs->start < rhs->start;
            }),
            entry);

    LOG_F(INFO, "Added mem region %s", get_entry_str(entry).c_str());

    return entry;
}


AddressMapEntry* MemCtrlBase::add_rom_region(uint32_t start_addr, uint32_t size) {
    return add_mem_region(start_addr, size, 0, RT_ROM);
}


AddressMapEntry* MemCtrlBase::add_ram_region(uint32_t start_addr, uint32_t size) {
    return add_mem_region(start_addr, size, 0, RT_RAM);
}

AddressMapEntry* MemCtrlBase::add_ram_region(uint32_t start_addr, uint32_t size,
                                             uint8_t *mem_ptr) {
    return add_mem_region(start_addr, size, 0, RT_RAM, mem_ptr);
}


AddressMapEntry* MemCtrlBase::add_mem_mirror_common(uint32_t start_addr, uint32_t dest_addr,
                                                    uint32_t offset, uint32_t size) {
    AddressMapEntry *entry, *ref_entry;

    ref_entry = find_range(dest_addr);
    if (!ref_entry)
        return nullptr;

    // use origin's size if no size was specified
    if (!size)
        size = ref_entry->end - ref_entry->start + 1;

    if (ref_entry->start + offset + size - 1 > ref_entry->end) {
        LOG_F(ERROR, "Partial mirror outside the origin, offset=0x%X, size=0x%X",
              offset, size);
        return nullptr;
    }

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = ref_entry->type | RT_MIRROR;
    entry->devobj  = nullptr;
    entry->mem_ptr = ref_entry->mem_ptr + offset;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mem region %s points to mem region %s",
        get_entry_str(entry).c_str(),
        get_entry_str(ref_entry).c_str()
    );

    return entry;
}


AddressMapEntry* MemCtrlBase::add_mem_mirror(uint32_t start_addr, uint32_t dest_addr) {
    return this->add_mem_mirror_common(start_addr, dest_addr);
}


AddressMapEntry* MemCtrlBase::add_mem_mirror_partial(uint32_t start_addr, uint32_t dest_addr,
                                                     uint32_t offset, uint32_t size) {
    return this->add_mem_mirror_common(start_addr, dest_addr, offset, size);
}


AddressMapEntry* MemCtrlBase::set_data(uint32_t load_addr, const uint8_t* data, uint32_t size) {
    AddressMapEntry* ref_entry;
    uint32_t cpy_size;

    ref_entry = find_range(load_addr);
    if (!ref_entry)
        return nullptr;

    uint32_t load_offset = load_addr - ref_entry->start;

    cpy_size = std::min(ref_entry->end - ref_entry->start + 1, size);
    memcpy(ref_entry->mem_ptr + load_offset, data, cpy_size);

    return ref_entry;
}


void MemCtrlBase::delete_address_map_entry(AddressMapEntry* entry) {
    if (!entry || !entry->mem_ptr)
        return;

    int found = 0;

    mem_regions.erase(std::remove_if(mem_regions.begin(), mem_regions.end(),
        [entry, &found](const uint8_t* mem_ptr) {
            if (entry->mem_ptr == mem_ptr) {
                if (!found) {
                    delete entry->mem_ptr;
                    entry->mem_ptr = nullptr;
                }
                found++;
                return true;
            }
            return false;
        }
    ), mem_regions.end());

    delete entry;
}


AddressMapEntry* MemCtrlBase::add_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance)
{
    AddressMapEntry *entry;

    // bail out if a memory region for the given range already exists
    if (!is_range_free(start_addr, size))
        return nullptr;

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = 0;
    entry->type    = RT_MMIO;
    entry->devobj  = dev_instance;
    entry->mem_ptr = 0;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mem region %s",
        get_entry_str(entry).c_str()
    );

    return entry;
}


bool MemCtrlBase::remove_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance)
{
    int found = 0;

    uint32_t end = start_addr + size - 1;
    address_map.erase(std::remove_if(address_map.begin(), address_map.end(),
        [this, start_addr, end, dev_instance, &found](AddressMapEntry *entry) {
            if (match_mem_entry(entry, start_addr, end, dev_instance)) {
                if (found)
                    LOG_F(ERROR, "Removed mem region %s", get_entry_str(entry).c_str());
                else
                    LOG_F(INFO, "Removed mem region %s", get_entry_str(entry).c_str());
                found++;
                this->delete_address_map_entry(entry);
                return true;
            }
            return false;
        }
    ), address_map.end());

    if (found == 0)
        LOG_F(ERROR, "Cannot find mem region 0x%X..0x%X to remove", start_addr, end);

    return (found > 0);
}


AddressMapEntry* MemCtrlBase::remove_region(AddressMapEntry* entry)
{
    int found = 0;

    address_map.erase(std::remove_if(address_map.begin(), address_map.end(),
        [entry, &found](const AddressMapEntry *cmp_entry) {
            if (entry == cmp_entry) {
                if (found)
                    LOG_F(ERROR, "Removed mem region %s", get_entry_str(entry).c_str());
                else
                    LOG_F(INFO, "Removed mem region %s", get_entry_str(entry).c_str());
                found++;
                return true;
            }
            return false;
        }
    ), address_map.end());

    if (found == 0) {
        LOG_F(ERROR, "Cannot find mem region %s to remove",
            get_entry_str(entry).c_str()
        );
        return nullptr;
    }

    return entry;
}


#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
bool MemCtrlBase::needs_swap_endian(bool is_mmio) {
    return false;
}
#endif

AddressMapEntry* MemCtrlBase::find_rom_region()
{
    for (auto& entry : address_map) {
        if (entry->type == RT_ROM) {
            return entry;
        }
    }

    return nullptr;
}

uint8_t *MemCtrlBase::get_region_hostmem_ptr(const uint32_t addr) {
    AddressMapEntry *reg_desc = this->find_range(addr);
    if (reg_desc == nullptr || reg_desc->type == RT_MMIO)
        return nullptr;

    if (reg_desc->type == RT_MIRROR)
        return (addr - reg_desc->mirror) + reg_desc->mem_ptr;
    else
        return (addr - reg_desc->start) + reg_desc->mem_ptr;
}


void MemCtrlBase::dump_regions()
{
    int i = 0;
    for (auto& entry : address_map) {
        printf("%2d: %s\n", i, get_entry_str(entry).c_str());
        i++;
    }
}
