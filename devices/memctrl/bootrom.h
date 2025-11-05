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

/** BootRom definitions. */

#ifndef BOOTROM_H
#define BOOTROM_H

#include <devices/common/mmiodevice.h>
#include <devices/memctrl/memctrlbase.h>
#include <array>

namespace Flash {

    enum State : uint8_t {
        ReadMemory,
        ReadAutoSelect,
        EraseSetup,
        EraseWrite,
        EraseVerify,
        ProgramSetup,
        Program,
        ProgramVerify,
        Reset,
        EmbeddedEraseSetup,
        EmbeddedEraseWrite,
        EmbeddedProgramSetup,
        EmbeddedProgram,
    };

} // namespace Hammerhead

class FlashController;

class FlashChip : virtual public HWComponent {
public:
    FlashChip(const std::string &dev_name) : HWComponent(dev_name) {}
    virtual ~FlashChip() = default;

    // FlashChip methods
    virtual void set_controller(FlashController* controller);
    virtual uint16_t read(uint32_t addr) = 0;
    virtual void write(uint32_t addr, uint16_t value) = 0;

    FlashController *controller = nullptr;
};

class FlashController : virtual public HWComponent {
public:
    FlashController(const std::string &dev_name) : HWComponent(dev_name) {}
    ~FlashController() = default;

    // FlashController methods
    virtual uint16_t rom_read(FlashChip *chip, uint32_t addr) = 0;
    virtual void rom_write(FlashChip *chip, uint32_t addr, uint16_t value) = 0;
};

class Am28F020 : public FlashChip {
public:
    Am28F020(const std::string &dev_name);
    virtual ~Am28F020() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<Am28F020>(new Am28F020(dev_name));
    }

    // FlashChip methods
    virtual uint16_t read(uint32_t addr);
    virtual void write(uint32_t addr, uint16_t value);

private:
#if 1
    uint8_t         vendor_id  = 0x01; // AMD
    uint8_t         device_id  = 0x2A; // Am28F020
#else
    uint8_t         vendor_id  = 0x89; // Intel
    uint8_t         device_id  = 0xBD; // 28F020
#endif
    Flash::State    state = Flash::ReadMemory;
    uint32_t        EA;
    uint32_t        PA;

    bool            use_intel_hack = true; // return 0xDB for flash chip index >= 8
    bool            supports_embedded = true;
    bool            supports_non_embedded = false;
};

class Mt28F008B1 : public FlashChip {
public:
    Mt28F008B1(const std::string &dev_name);
    virtual ~Mt28F008B1() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<Mt28F008B1>(new Mt28F008B1(dev_name));
    }

    // FlashChip methods
    virtual uint16_t read(uint32_t addr);
    virtual void write(uint32_t addr, uint16_t value);

private:
    uint8_t         vendor_id  = 0x89;  // Micron
    uint8_t         device_id  = 0x98;  // Mt28F008B1

    Flash::State    state = Flash::ReadMemory;
    uint32_t        EA;
    uint32_t        PA;
};

class BootRom : public MMIODevice, public FlashController {
public:
    BootRom(const std::string &dev_name, uint32_t size);
    ~BootRom() = default;

    // HWComponent methods
    virtual HWComponent* add_device(int32_t unit_address, HWComponent *dev_obj, const std::string &name = "") override;
    virtual PostInitResultType device_postinit() override;

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override = 0;
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override = 0;

    // BootRom methods
    virtual void set_rom_write_enable(const bool enable);
    virtual int set_data(const uint8_t* data, uint32_t size);
    virtual uint8_t* get_data() { return rom_entry->mem_ptr; };
    virtual void identify_rom();

private:
    bool                rom_we = false;
    bool                has_flash = true;
    AddressMapEntry*    rom_entry;
    uint32_t            rom_addr;
    uint32_t            rom_size;
};

class BootRomOW : public BootRom {
public:
    BootRomOW(const std::string &dev_name) : BootRom(dev_name, 0x400000), HWComponent(dev_name) {}
    ~BootRomOW() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<BootRomOW>(new BootRomOW(dev_name));
    }

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // FlashController methods
    virtual uint16_t rom_read(FlashChip *chip, uint32_t addr) override;
    virtual void rom_write(FlashChip *chip, uint32_t addr, uint16_t value) override;
};

class BootRomNW : public BootRom {
public:
    BootRomNW(const std::string &dev_name) : BootRom(dev_name, 0x100000), HWComponent(dev_name) {}
    ~BootRomNW() = default;

    static std::unique_ptr<HWComponent> create(const std::string &dev_name) {
        return std::unique_ptr<BootRomNW>(new BootRomNW(dev_name));
    }

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // FlashController methods
    virtual uint16_t rom_read(FlashChip *chip, uint32_t addr) override;
    virtual void rom_write(FlashChip *chip, uint32_t addr, uint16_t value) override;

};

#endif // BOOTROM_H
