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

#include <cinttypes>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <set>

#ifndef MACHINE_PROPERTIES_H
#define MACHINE_PROPERTIES_H

/** Property types. */
enum PropType : int {
    PROP_TYPE_UNKNOWN   = 0,
    PROP_TYPE_STRING    = 1,
    PROP_TYPE_INTEGER   = 2,
    PROP_TYPE_BINARY    = 3, // binary aka on/off switch
};

/** Check types for property values. */
enum CheckType : int {
    CHECK_TYPE_NONE     = 0,
    CHECK_TYPE_RANGE    = 1,
    CHECK_TYPE_LIST     = 2,
};

/** Abstract base class for properties. */
class BasicProperty {
public:
    BasicProperty(PropType type, std::string val) {
        this->type = type;
        set_string(val);
    }

    virtual ~BasicProperty() = default;

    /* Clone method for copying derived property objects. */
    virtual BasicProperty* clone() const = 0;

    virtual std::string get_string() {
        return this->val;
    }

    virtual void set_string(std::string str) {
        this->val = str;
    }

    virtual PropType get_type() {
        return this->type;
    }

protected:
    PropType         type;
    std::string      val;
};


/** Property class that holds a string value. */
class StrProperty : public BasicProperty {
public:
    StrProperty(std::string str)
        : BasicProperty(PROP_TYPE_STRING, str)
    {
        this->check_type = CHECK_TYPE_NONE;
        this->vec.clear();
    }

    /* construct a string property with a list of valid values. */
    StrProperty(std::string str, std::vector<std::string> vec)
        : BasicProperty(PROP_TYPE_STRING, str)
    {
        this->check_type = CHECK_TYPE_LIST;
        this->vec = vec;
    }

    BasicProperty* clone() const { return new StrProperty(*this); }

    /* override BasicProperty::set_string() and perform checks */
    void set_string(std::string str);

    std::string get_valid_values_as_str();

protected:
    bool check_val(std::string str);

private:
    CheckType                 check_type;
    std::vector<std::string>  vec;
};

/** Property class that holds an integer value. */
class IntProperty : public BasicProperty {
public:
    /* construct an integer property without value check. */
    IntProperty(uint32_t val)
        : BasicProperty(PROP_TYPE_INTEGER, std::to_string(val))
    {
        this->int_val    = val;
        this->min        = std::numeric_limits<uint32_t>::min();
        this->max        = std::numeric_limits<uint32_t>::max();
        this->check_type = CHECK_TYPE_NONE;
        this->vec.clear();
    }

    /* construct an integer property with a predefined range. */
    IntProperty(uint32_t val, uint32_t min, uint32_t max)
        : BasicProperty(PROP_TYPE_INTEGER, std::to_string(val))
    {
        this->int_val    = val;
        this->min        = min;
        this->max        = max;
        this->check_type = CHECK_TYPE_RANGE;
        this->vec.clear();
    }

    /* construct an integer property with a list of valid values. */
    IntProperty(uint32_t val, std::vector<uint32_t> vec)
        : BasicProperty(PROP_TYPE_INTEGER, std::to_string(val))
    {
        this->int_val    = val;
        this->min        = std::numeric_limits<uint32_t>::min();
        this->max        = std::numeric_limits<uint32_t>::max();
        this->check_type = CHECK_TYPE_LIST;
        this->vec = vec;
    }

    BasicProperty* clone() const { return new IntProperty(*this); }

    uint32_t get_int();

    std::string get_valid_values_as_str();

protected:
    bool check_val(uint32_t val);

private:
    uint32_t                 int_val;
    CheckType                check_type;
    uint32_t                 min;
    uint32_t                 max;
    std::vector<uint32_t>    vec;
};

/** Property class that holds a binary value. */
class BinProperty : public BasicProperty {
public:
    BinProperty(int val)
        : BasicProperty(PROP_TYPE_BINARY, (val & 1) ? "on" : "off")
    {
        this->bin_val = val & 1;
    }

    BasicProperty* clone() const { return new BinProperty(*this); }

    // override BasicProperty::set_string() and perform checks
    void set_string(std::string str);

    std::string get_valid_values_as_str();

    int get_val() { return this->bin_val; }

private:
    int     bin_val;
};

/** Special map type for specifying machine presets. */
typedef std::map<std::string, BasicProperty*> PropMap;

/** Global map that holds settings for the running machine. */

class Setting {
public:
    Setting() = default;
    ~Setting() = default;
    void set_property_info(BasicProperty *p) {
        this->value_default = p->get_string();
        this->property = std::unique_ptr<BasicProperty>(p->clone());
        this->property->set_string(
            this->value_commandline == this->value_not_inited ?
                this->value_default
            :
                this->value_commandline
        );
    }
    std::string value_default = this->value_not_inited;
    std::string value_commandline = this->value_not_inited;
    std::unique_ptr<BasicProperty> property = nullptr;
    static const char *value_not_inited;
    static const char *value_defaulted;
    static std::set<BasicProperty*> loaded_properties;
};

extern std::map<std::string, std::unique_ptr<Setting>> gMachineSettings;

/** Conveniency macros to hide complex casts. */
#define GET_STR_PROP(name) \
    dynamic_cast<StrProperty*>(gMachineSettings.at(name)->property.get())->get_string()

#define GET_INT_PROP(name) \
    dynamic_cast<IntProperty*>(gMachineSettings.at(name)->property.get())->get_int()

#define GET_BIN_PROP(name) \
    dynamic_cast<BinProperty*>(gMachineSettings.at(name)->property.get())->get_val()

#endif // MACHINE_PROPERTIES_H
