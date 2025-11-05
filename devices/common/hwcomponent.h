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

#ifndef HW_COMPONENT_H
#define HW_COMPONENT_H

#include <devices/deviceregistry.h>
#include <cinttypes>
#include <string>
#include <map>
#include <loguru.hpp>

/** types of different HW components */
enum HWCompType : uint64_t {
    UNKNOWN     = 0ULL,       // unknown component type
    MEM_CTRL    = 1ULL << 0,  // memory controller
    NVRAM       = 1ULL << 1,  // non-volatile random access memory
    ROM         = 1ULL << 2,  // read-only memory
    RAM         = 1ULL << 3,  // random access memory
    MMIO_DEV    = 1ULL << 4,  // memory mapped I/O device
    PCI_HOST    = 1ULL << 5,  // PCI host
    PCI_DEV     = 1ULL << 6,  // PCI device
    NUBUS_DEV   = 1ULL << 7,  // Nubus device
    I2C_HOST    = 1ULL << 8,  // I2C host
    I2C_DEV     = 1ULL << 9,  // I2C device
    ADB_HOST    = 1ULL << 12, // ADB host
    ADB_DEV     = 1ULL << 13, // ADB device
    IOBUS_HOST  = 1ULL << 14, // IOBus host
    IOBUS_DEV   = 1ULL << 15, // IOBus device
    INT_CTRL    = 1ULL << 16, // interrupt controller
    SCSI_BUS    = 1ULL << 20, // SCSI bus
    SCSI_HOST   = 1ULL << 21, // SCSI host adapter
    SCSI_DEV    = 1ULL << 22, // SCSI device
    IDE_BUS     = 1ULL << 23, // IDE bus
    IDE_HOST    = 1ULL << 24, // IDE host controller
    IDE_DEV     = 1ULL << 25, // IDE device
    SND_CODEC   = 1ULL << 30, // sound codec
    SND_SERVER  = 1ULL << 31, // host sound server
    FLOPPY_CTRL = 1ULL << 32, // floppy disk controller
    FLOPPY_DRV  = 1ULL << 33, // floppy disk drive
    ETHER_MAC   = 1ULL << 40, // Ethernet media access controller
    MACHINE     = 1ULL << 41, // machine root
    VIDEO_CTRL  = 1ULL << 42, // video controller
    DISPLAY     = 1ULL << 43, // display
    PRAM        = 1ULL << 44, // parameter RAM
    FLASH_CTRL  = 1ULL << 45, // flash chip controller
    FLASH       = 1ULL << 46, // flash chip
};

extern std::map<HWCompType, HWCompType> MapBusDev;

enum PostInitResultType : int {
    PI_SUCCESS = 0,
    PI_FAIL    = -1,
    PI_RETRY   = 1,
};

/** Base class for HW components. */
class HWComponent {
friend class MachineFactory;
public:
    HWComponent(const std::string name);
    virtual ~HWComponent();

    virtual std::string get_name(void) {
        return this->name;
    }
    virtual void set_name(const std::string name) {
        this->name = name;
    }

    virtual bool supports_type(HWCompType type) {
        return !!(this->supported_types & type);
    }

    virtual void supports_types(uint64_t types) {
        this->supported_types = types;
    }

    virtual PostInitResultType device_postinit() {
        return PI_SUCCESS;
    }

    virtual bool is_ready_for_machine() {
        return true;
    }

    virtual void change_unit_address(int32_t unit_address);
    virtual void set_unit_address(int32_t unit_address) {
        this->unit_address = unit_address;
    }
    int32_t get_unit_address() {
        return this->unit_address;
    }

    virtual HWComponent* get_parent() {
        return this->parent;
    }
    virtual void set_parent(HWComponent* parent) {
        this->parent = parent;
    }

    void clear_devices();
    virtual HWComponent* add_device(int32_t unit_address, HWComponent* dev_obj, const std::string &name = "");
    virtual void move_device(HWComponent* new_parent);
    virtual bool remove_device(int32_t unit_address);
    HWComponent* get_comp_by_name(const std::string name, bool optional = false);
    HWComponent* get_comp_by_name_optional(const std::string name);
    HWComponent* get_comp_by_type(HWCompType type);
    PostInitResultType postinit_devices();

    std::string get_path();
    HWComponent *find_path(
        std::string path, int match_types = 1, bool allow_partial_match = true,
        bool *is_leaf_match = nullptr, int32_t *unit_address = nullptr
    );
    std::string get_name_and_unit_address();
    virtual int32_t parse_self_unit_address_string(const std::string unit_address_string);
    virtual int32_t parse_child_unit_address_string(const std::string unit_address_string, HWComponent*& hwc);
    virtual std::string get_child_unit_address_string(int32_t unit_address);
    virtual std::string get_self_unit_address_string(int32_t unit_address);
    std::string get_self_unit_address_string();
    static std::string extract_device_name(const std::string name);
    static std::string extract_unit_address(const std::string name);

    virtual HWComponent* set_property(const std::string &property, const std::string &value, int32_t unit_address = -1) {
        return nullptr;
    }
    bool override_property(const std::string &property, const std::string &value);
    bool can_property_be_overriden(const std::string &property);
    std::string get_property_str(const std::string &property);
    int         get_property_int(const std::string &property);
    bool        get_property_bin(const std::string &property);

    void dump_devices(int indent = 0);
    void dump_paths();
    bool path_match(std::string path, bool allow_partial_match);

    bool iterate(const std::function<bool(HWComponent *it, int depth)> &func, int depth = 0);

protected:
    std::string name;
    uint64_t    supported_types = HWCompType::UNKNOWN;

    int32_t unit_address = -1;
    std::map<int32_t, std::unique_ptr<HWComponent>> children;
    HWComponent* parent = nullptr;

private:
    void move_children(HWComponent* dst);
    PostInitResultType postinit_devices(int &devices_inited, int &devices_skipped);
    PostInitResultType postinit_device(int &devices_inited, int &devices_skipped);
    void init_device_settings(const DeviceDescription &dev);
    bool postinitialized = false;
    const DeviceDescription* device_description = nullptr;
    std::map<std::string, std::unique_ptr<Setting>> device_settings;
};

extern std::unique_ptr<HWComponent> gMachineObj;

#endif // HW_COMPONENT_H
