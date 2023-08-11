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

#include "symbols.h"
#include "symbolsopenfirmware.h"
#include "memaccess.h"
#include <cpu/ppc/ppcmmu.h>
#include <loguru.hpp>
#include <fstream>
#include <vector>
#include <string>
#ifdef __APPLE__
#include <mach-o/loader.h>
#endif

using namespace std;

std::vector<binary_t> binaries;

void load_symbols(const std::string &path) {
    ifstream f;
    f.open(path, std::ios::in);
    if (f.fail()) {
        f.close();
        LOG_F(ERROR, "load_names: Could not open specified names file!");
        return;
    }

/*
    symbol.start = (uint32_t)stoul(start, nullptr, 16);
    symbol.end = (uint32_t)stoul(end, nullptr, 16);
*/

    std::string name;
    uint32_t start, end;
    int type;
    symbol_t *previous_sym = nullptr;
    while (f >> hex >> start >> end >> type) {
        if (type == 0) {
            binary_t bin;
            std::string kind;
            f >> kind;
            /**/ if (kind == "Open_Firmware")
                bin.kind = kind_open_firmware;
            else if (kind == "kernel")
                bin.kind = kind_darwin_kernel;
            else if (kind == "kext")
                bin.kind = kind_darwin_kext;
            else if (kind == "process")
                bin.kind = kind_darwin_process;
            else if (kind == "library")
                bin.kind = kind_darwin_library;
            else
                bin.kind = kind_unknown;

            if (getline(f, name))
                name.erase(0, 1);
            bin.start = start;
            bin.end = end;
            bin.name = name;
            binaries.push_back(bin);
            previous_sym = nullptr;
        }
        else if (binaries.empty()) {
            LOG_F(ERROR, "load_names: Expected a binary.");
        }
        else {
            if (getline(f, name))
                name.erase(0, 1);
            if (type == 1) {
                segment_t seg;
                seg.start = start;
                seg.end = end;
                if (name.empty())
                    seg.name = "seg#" + std::to_string(binaries.back().segments.size());
                else
                    seg.name = name;
                binaries.back().segments.push_back(seg);
                if (previous_sym && (previous_sym->end == 0 || start < previous_sym->end))
                    previous_sym->end = start;
                previous_sym = nullptr;
            }
            else if (type == 2) {
                if (binaries.back().segments.empty()) {
                    LOG_F(ERROR, "load_names: Expected a segment.");
                }
                else {
                    section_t sec;
                    sec.start = start;
                    sec.end = end;
                    sec.name = name;
                    binaries.back().segments.back().sections.push_back(sec);
                    if (previous_sym && (previous_sym->end == 0 || start < previous_sym->end))
                        previous_sym->end = start;
                    previous_sym = nullptr;
                }
            }
            else if (type == 3) {
                symbol_t sym;
                sym.start = start;
                sym.end = end;
                sym.name = name;
                if (binaries.back().segments.empty() || start >= binaries.back().segments.back().end ) {
                    if (previous_sym && (previous_sym->end == 0 || start < previous_sym->end))
                        previous_sym->end = start;
                    sym.end = binaries.back().end;
                    binaries.back().symbols.push_back(sym);
                    previous_sym = &binaries.back().symbols.back();
                }
                else if (binaries.back().segments.back().sections.empty() ||
                    start >= binaries.back().segments.back().sections.back().end
                ) {
                    if (previous_sym && (previous_sym->end == 0 || start < previous_sym->end))
                        previous_sym->end = start;
                    sym.end = binaries.back().segments.back().end;
                    binaries.back().segments.back().symbols.push_back(sym);
                    previous_sym = &binaries.back().segments.back().symbols.back();
                }
                else {
                    if (previous_sym && (previous_sym->end == 0 || start < previous_sym->end))
                        previous_sym->end = start;
                    sym.end = binaries.back().segments.back().sections.back().end;
                    binaries.back().segments.back().sections.back().symbols.push_back(sym);
                    previous_sym = &binaries.back().segments.back().sections.back().symbols.back();
                }
            }
        } // if binaries
    } // while line
}

binary_t* find_binary_kind(binary_kind_t kind) {
    for (auto &bin : binaries)
        if (bin.kind == kind)
            return &bin;
    return nullptr;
}

binary_t* find_binary_name(const std::string &name) {
    for (auto &bin : binaries)
        if (bin.name == name)
            return &bin;
    return nullptr;
}

symbol_t* find_symbol(vector<symbol_t> &symbols, uint32_t addr) {
    for (auto &sym : symbols)
        if (sym.start <= addr && sym.end > addr)
            return &sym;
    return nullptr;
}

std::string get_offset_string(const std::string &name, int offset, int *offset_out) {
    if (offset_out)
        *offset_out = offset;
    if (offset) {
        char offset_str[20];
        snprintf(offset_str, sizeof(offset_str), "%+-6d", offset);
        return name + offset_str;
    }
    return name + "      ";
}

std::string get_offset_string(binary_t *bin, const std::string &name, int offset, int *offset_out) {
    return get_offset_string(bin ? bin->name + ";" + name : name, offset, offset_out);
}

std::string get_offset_string(binary_t *bin, symbol_t *sym, uint32_t addr, int *offset) {
    return get_offset_string(bin ? bin->name + ";" + sym->name : sym->name, addr - sym->start, offset);
}

std::string get_name(binary_t &bin, uint32_t addr, int *offset) {
    std::string str;
    symbol_t* sym;
    if (addr >= bin.start && addr < bin.end) {
        for (auto &seg : bin.segments) {
            if (addr < seg.start || addr >= seg.end)
                continue;
            for (auto &sec : seg.sections) {
                if (addr < sec.start || addr >= sec.end)
                    continue;
                sym = find_symbol(sec.symbols, addr);
                if (sym)
                    return get_offset_string(nullptr, sym, addr, offset);
                return get_offset_string(&bin, sec.name, addr - sec.start, offset);
            }
            sym = find_symbol(seg.symbols, addr);
            if (sym)
                return get_offset_string(nullptr, sym, addr, offset);
            return get_offset_string(&bin, seg.name, addr - seg.start, offset);
        }
        sym = find_symbol(bin.symbols, addr);
        if (sym)
            return get_offset_string(nullptr, sym, addr, offset);
        return get_offset_string(nullptr, bin.name, addr - bin.start, offset);
    }
    return str;
}

std::string get_name(binary_kind_t kind, uint32_t addr, int *offset) {
    std::string str;
    for (auto &bin : binaries)
        if (bin.kind == kind) {
            str = get_name(bin, addr, offset);
            if (!str.empty()) {
                return str;
        }
    }
    return str;
}

std::string get_name_kernel(uint32_t addr, int *offset) {
    return get_name(kind_darwin_kernel, addr, offset);
}

bool lookup_name(binary_t &bin, const std::string &name, uint32_t &addr) {
    for (auto &seg : bin.segments) {
        for (auto &sec : seg.sections) {
            for (auto &sym : sec.symbols) {
                if (sym.name == name) {
                    addr = sym.start;
                    return true;
                }
            }
        }
        for (auto &sym : seg.symbols) {
            if (sym.name == name) {
                addr = sym.start;
                return true;
            }
        }
    }
    for (auto &sym : bin.symbols) {
        if (sym.name == name) {
            addr = sym.start;
            return true;
        }
    }
    return false;
}

bool lookup_name(binary_kind_t kind, const std::string &name, uint32_t &addr) {
    binary_t *bin = find_binary_kind(kind);
    if (bin)
        return lookup_name(*bin, name, addr);
    return false;
}

bool lookup_name_kernel(const std::string &name, uint32_t &addr) {
    return lookup_name(kind_darwin_kernel, name, addr);
}

typedef struct {
    // [ start of guest kmod_info_t
    uint32_t next;
    uint32_t info_version;
    uint32_t id;
    char     name[64];
    char     version[64];
    int32_t  reference_count;
    uint32_t reference_list;
    uint32_t address;
    uint32_t size;
    uint32_t hdr_size;
    uint32_t start;
    uint32_t stop;
    // ] end of guest kmod_info_t
    uint32_t kmod; // guest virtual address pointer to kmod info
} kmod_info_t;

vector<kmod_info_t> get_kmod_infos() {
    static uint32_t _kmod = 0;
    kmod_info_t info;
    vector<kmod_info_t> kmod_infos;
    if (!_kmod)
        lookup_name_kernel("_kmod", _kmod);
    if (_kmod) {
        try {
            uint32_t kmod = (uint32_t)mem_read_dbg(_kmod, 4);
            while ((!(kmod & 3)) && kmod) {
                info.kmod = kmod;
                uint64_t val;
                info.next               = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, next           ), 4);
                info.info_version       = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, info_version   ), 4);
                info.id                 = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, id             ), 4);
                info.reference_count    = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, reference_count), 4);
                info.reference_list     = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, reference_list ), 4);
                info.address            = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, address        ), 4);
                info.size               = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, size           ), 4);
                info.hdr_size           = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, hdr_size       ), 4);
                info.start              = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, start          ), 4);
                info.stop               = (uint32_t)mem_read_dbg(kmod + offsetof(kmod_info_t, stop           ), 4);
                for (int i = 0; i < 8; i++) { val = mem_read_dbg(kmod + offsetof(kmod_info_t, name           ) + i * 8, 8);
                    WRITE_QWORD_BE_A(&(((uint64_t*)(&info.name   ))[i]), val); if (!val) break; };
                for (int i = 0; i < 8; i++) { val = mem_read_dbg(kmod + offsetof(kmod_info_t, version        ) + i * 8, 8);
                    WRITE_QWORD_BE_A(&(((uint64_t*)(&info.version))[i]), val); if (!val) break; };
                kmod_infos.push_back(info);
                kmod = info.next;
            }
        } catch (invalid_argument& exc) {
        }
    }
    return kmod_infos;
}

std::string get_name_kext(uint32_t addr, int *offset) {
    std::string str;
#ifdef __APPLE__
    symbol_t *sym;

    vector<kmod_info_t> kmod_infos = get_kmod_infos();
    for (auto &info : kmod_infos) {
        if (!(info.address && info.hdr_size >= 4096 && info.address <= addr && info.address + info.size > addr))
            continue;
        binary_t* bin = find_binary_name(info.name);

        /*
        Sections are not necessarily loaded where the macho-o binary says they will be loaded so
        find the section of the kmod containing the address and match that section to one from the
        macho-o binary info.
        */

        do {
            mach_header hdr;
            hdr.magic       = (uint32_t)mem_read_dbg(info.address + offsetof(mach_header, magic     ), 4);
            if (hdr.magic != MH_MAGIC)
                break;
            hdr.sizeofcmds  = (uint32_t)mem_read_dbg(info.address + offsetof(mach_header, sizeofcmds), 4);
            if (sizeof(hdr) + hdr.sizeofcmds > info.hdr_size)
                break;
            hdr.ncmds       = (uint32_t)mem_read_dbg(info.address + offsetof(mach_header, ncmds     ), 4);

            segment_command seg;
            int seg_addr = info.address + sizeof(hdr);
            int seg_num = 0;
            for (int ncmd = 0; ncmd < hdr.ncmds; ncmd++, seg_addr += seg.cmdsize) {
                seg.cmd      = (uint32_t)mem_read_dbg(seg_addr + offsetof(segment_command, cmd     ), 4);
                seg.cmdsize  = (uint32_t)mem_read_dbg(seg_addr + offsetof(segment_command, cmdsize ), 4);
                if (seg.cmd != LC_SEGMENT)
                    continue;
                seg.vmaddr   = (uint32_t)mem_read_dbg(seg_addr + offsetof(segment_command, vmaddr  ), 4);
                if (addr < seg.vmaddr)
                    continue;
                seg.vmsize   = (uint32_t)mem_read_dbg(seg_addr + offsetof(segment_command, vmsize  ), 4);
                if (addr >= seg.vmaddr + seg.vmsize)
                    continue;
                seg.nsects   = (uint32_t)mem_read_dbg(seg_addr + offsetof(segment_command, nsects  ), 4);
                uint64_t val;
                for (int i = 0; i < 2; i++) {
                    val = mem_read_dbg(seg_addr + offsetof(segment_command, segname) + i * 8, 8);
                    WRITE_QWORD_BE_A(&(((uint64_t*)(&seg.segname))[i]), val);
                    if (!val) break;
                }

                std::string segname;
                if (seg.segname[0])
                    segname = seg.segname;
                else
                    segname = "seg#" + std::to_string(seg_num);
                segment_t *found_seg = nullptr;
                if (bin) {
                    for (auto &loop_seg : bin->segments) {
                        if (loop_seg.name == segname) {
                            found_seg = &loop_seg;
                            break;
                        }
                    }
                }
                seg_num++;

                section sec;
                int sec_addr = seg_addr + sizeof(seg);
                for (int nsect = 0; nsect < seg.nsects; nsect++, sec_addr += sizeof(sec)) {
                    sec.addr      = (uint32_t)mem_read_dbg(sec_addr + offsetof(section, addr     ), 4);
                    if (addr < sec.addr)
                        continue;
                    sec.size      = (uint32_t)mem_read_dbg(sec_addr + offsetof(section, size     ), 4);
                    if (addr >= sec.addr + sec.size)
                        continue;
                    for (int i = 0; i < 2; i++) {
                        val = mem_read_dbg(sec_addr + offsetof(section, sectname) + i * 8, 8);
                        WRITE_QWORD_BE_A(&(((uint64_t*)(&sec.sectname))[i]), val);
                        if (!val) break;
                    }
                    for (int i = 0; i < 2; i++) {
                        val = mem_read_dbg(sec_addr + offsetof(section, segname ) + i * 8, 8);
                        WRITE_QWORD_BE_A(&(((uint64_t*)(&sec.segname ))[i]), val);
                        if (!val) break;
                    }

                    section_t *found_sec = nullptr;
                    if (found_seg) {
                        for (auto &loop_sec : found_seg->sections) {
                            if (loop_sec.end - loop_sec.start == sec.size &&
                                loop_sec.name == std::string(sec.segname) + ":" + sec.sectname
                            ) {
                                found_sec = &loop_sec;
                                break;
                            }
                        }
                    }

                    if (found_sec) {
                        sym = find_symbol(found_sec->symbols, addr - sec.addr + found_sec->start);
                        if (sym)
                            return get_offset_string(nullptr, sym, addr - sec.addr + found_sec->start, offset);
                        return get_offset_string(bin, found_sec->name, addr - sec.addr, offset);
                    }
                    return get_offset_string(bin, std::string(sec.segname) + ":" + sec.sectname, addr - sec.addr, offset);
                } // for section

                if (found_seg) {
                    sym = find_symbol(found_seg->symbols, addr - seg.vmaddr + found_seg->start);
                    if (sym)
                        return get_offset_string(nullptr, sym, addr - seg.vmaddr + found_seg->start, offset);
                }
                return get_offset_string(bin, segname, addr - seg.vmaddr, offset);
            } // for segment command

        } while(0);

        if (bin && addr >= info.address + info.hdr_size && addr < info.address + info.hdr_size + bin->end - bin->start) {
            sym = find_symbol(bin->symbols, addr - (info.address + info.hdr_size) + bin->start);
            if (sym)
                return get_offset_string(nullptr, sym, addr - (info.address + info.hdr_size) + bin->start, offset);
            return get_offset_string(nullptr, bin->name, addr - (info.address + info.hdr_size), offset);
        }

        return get_offset_string(nullptr, info.name, addr - (info.address + info.hdr_size), offset);
    } // for kmod

#endif
    return str;
}

std::string get_name(uint32_t addr, uint32_t addr_p, int *offset, binary_kind_t *kind, int kinds) {
    std::string str;

    if (kinds == 0 || (kinds & (1 << kind_open_firmware))) {
        str = get_name_OpenFirmware(addr, addr_p, offset);
        if (!str.empty()) {
            if (kind)
                *kind = kind_open_firmware;
            return str;
        }
    }

    if (kinds == 0 || (kinds & (1 << kind_darwin_kernel))) {
        str = get_name_kernel(addr, offset);
        if (!str.empty()) {
            if (kind)
                *kind = kind_darwin_kernel;
            return str;
        }
    }

    if (kinds == 0 || (kinds & (1 << kind_darwin_kext))) {
        str = get_name_kext(addr, offset);
        if (!str.empty()) {
            if (kind)
                *kind = kind_darwin_kext;
            return str;
        }
    }

    if (kind)
        *kind = kind_unknown;
    if (offset)
        *offset = 0;
    return str;
}

void showkmodheader() {
    printf("kmod        address     hdr_size    size        id    refs     version  name\n");
}

void showkmodint(kmod_info_t &info) {
    printf("0x%08x  ", info.kmod);
    printf("0x%08x  ", info.address);
    printf("0x%08x  ", info.hdr_size);
    printf("0x%08x  ", info.size);
    printf("%3d  ", info.id);
    printf("%5d  ", info.reference_count);
    printf("%10s  ", info.version);
    printf("%s\n", info.name);
}

void showallkmods() {
    vector<kmod_info_t> kmod_infos = get_kmod_infos();
    showkmodheader();
    for (auto &info : kmod_infos) {
        showkmodint(info);
    }
}
