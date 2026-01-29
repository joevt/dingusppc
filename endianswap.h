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

/** @file Macros for performing byte swapping for quick endian conversion.

    It will try to use the fast built-in machine instructions first
    and fall back to the slow conversion if none were found.
 */

#ifndef ENDIAN_SWAP_H
#define ENDIAN_SWAP_H

#include <core/bitops.h>

#ifdef __GNUG__ /* GCC, ICC and Clang */

#   define BYTESWAP_16(x) (__builtin_bswap16 (x))
#   define BYTESWAP_32(x) (__builtin_bswap32 (x))
#   define BYTESWAP_64(x) (__builtin_bswap64 (x))

#elif _MSC_VER   /* MSVC */

#   include <stdlib.h>

#   define BYTESWAP_16(x) (_byteswap_ushort (x))
#   define BYTESWAP_32(x) (_byteswap_ulong (x))
#   define BYTESWAP_64(x) (_byteswap_uint64 (x))

#elif __has_include(<byteswap.h>) /* Many unixes */

#   include <byteswap.h>

#   define BYTESWAP_16(x) (bswap_16(x))
#   define BYTESWAP_32(x) (bswap_32(x))
#   define BYTESWAP_64(x) (bswap_64(x))

// TODO: C++23 has std::byteswap

#else

// #warning is not standard until C++23:
//
// #   warning "Unknown byte swapping built-ins (do it the slow way)!"

#   include <cstdint>

// Most optimizing compilers will translate these functions directly
// into their native instructions (e.g. bswap on x86).
inline std::uint16_t byteswap_16_impl(std::uint16_t x)
{
    return (x >> 8) | (x << 8);
}

inline std::uint32_t byteswap_32_impl(std::uint32_t x)
{
    return (
          ((x & UINT32_C(0x000000FF)) << 24)
        | ((x & UINT32_C(0x0000FF00)) << 8)
        | ((x & UINT32_C(0x00FF0000)) >> 8)
        | ((x & UINT32_C(0xFF000000)) >> 24)
        );
}

inline std::uint64_t byteswap_64_impl(std::uint64_t x)
{
    return (
          ((x & UINT64_C(0x00000000000000FF)) << 56)
        | ((x & UINT64_C(0x000000000000FF00)) << 40)
        | ((x & UINT64_C(0x0000000000FF0000)) << 24)
        | ((x & UINT64_C(0x00000000FF000000)) << 8)
        | ((x & UINT64_C(0x000000FF00000000)) >> 8)
        | ((x & UINT64_C(0x0000FF0000000000)) >> 24)
        | ((x & UINT64_C(0x00FF000000000000)) >> 40)
        | ((x & UINT64_C(0xFF00000000000000)) >> 56)
        );
}

#   define BYTESWAP_16(x) (byteswap_16_impl(x))
#   define BYTESWAP_32(x) (byteswap_32_impl(x))
#   define BYTESWAP_64(x) (byteswap_64_impl(x))

#endif

#define BYTESWAP_SIZED(val, size) \
    ((size) == 2 ? BYTESWAP_16(uint16_t(val)) : (size) == 4 ? BYTESWAP_32(val) : (val))

enum {
    PCI_CONFIG_DIRECTION    = 0x0100,
        PCI_CONFIG_READ     = 0x0000,
        PCI_CONFIG_WRITE    = 0x0100,

    PCI_CONFIG_TYPE         = 0x1000,
        PCI_CONFIG_TYPE_0   = 0x0000,
        PCI_CONFIG_TYPE_1   = 0x1000,
}; // PCIAccessFlags

/** PCI config space access details */
typedef uint32_t AccessDetails;
#define ACCESSDETAILS_SET(details, size, offset, flags) (details = (((offset) & 3) | ((size)<<2) | (flags)))
#define ACCESSDETAILS_SIZE(details) ((details >> 2) & 7)
#define ACCESSDETAILS_OFFSET(details) (details & 3)
#define ACCESSDETAILS_SIZE_OFFSET(details) (details & 0x1F)
#define ACCESSDETAILS_FLAGS(details) details
#define ACCESSDETAILS_FLAGS_SET(details, flags) (details |= flags)

/**
    Perform size dependent endian swapping for value that is dword from PCI config or any other dword little endian register.

    Unaligned data is handled properly by using bytes from the next dword.
 */
inline uint32_t conv_rd_data(uint32_t value, uint32_t value2, const AccessDetails details) {
    switch (ACCESSDETAILS_SIZE_OFFSET(details)) {
    // Bytes
    case 0x04:
        return value & 0xFF;            // 0
    case 0x05:
        return (value >>  8) & 0xFF;    // 1
    case 0x06:
        return (value >> 16) & 0xFF;    // 2
    case 0x07:
        return (value >> 24) & 0xFF;    // 3

    // Words
    case 0x08:
        return BYTESWAP_16(uint16_t(value));                // 0 1
    case 0x09:
        return BYTESWAP_16((value >>  8) & 0xFFFFU);        // 1 2
    case 0x0A:
        return BYTESWAP_16((value >> 16) & 0xFFFFU);        // 2 3
    case 0x0B:
        return ((value >> 16) & 0xFF00) | (value2 & 0xFF);  // 3 4

    // Dwords
    case 0x10:
        return BYTESWAP_32(value);                          // 0 1 2 3
    case 0x11:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >>  8);
        return BYTESWAP_32(value);                          // 1 2 3 4
    case 0x12:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >> 16);
        return BYTESWAP_32(value);                          // 2 3 4 5
    case 0x13:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >> 24);
        return BYTESWAP_32(value);                          // 3 4 5 6
    default:
        return 0xFFFFFFFFUL;
    }
}

/**
    Perform size dependent endian swapping for v2, then merge v2 with v1 under
    control of a mask generated according with the size parameter.

    Unaligned data is handled properly by wrapping around if needed.
 */
inline uint32_t conv_wr_data(uint32_t v1, uint32_t v2, const AccessDetails details)
{
    switch (ACCESSDETAILS_SIZE_OFFSET(details)) {
    // Bytes
    case 0x04:
        return (v1 & ~0xFF)      |  (v2 & 0xFF);        //  3  2  1 d0
    case 0x05:
        return (v1 & ~0xFF00)    | ((v2 & 0xFF) << 8);  //  3  2 d0  0
    case 0x06:
        return (v1 & ~0xFF0000)  | ((v2 & 0xFF) << 16); //  3 d0  1  0
    case 0x07:
        return (v1 & 0x00FFFFFF) | ((v2 & 0xFF) << 24); // d0  2  1  0

    // Words
    case 0x08:
        return (v1 & ~0xFFFF)    |  BYTESWAP_16(uint16_t(v2));          //  3  2 d1 d0
    case 0x09:
        return (v1 & ~0xFFFF00)  | (BYTESWAP_16(uint16_t(v2)) << 8);    //  3 d1 d0  0
    case 0x0a:
        return (v1 & 0x0000FFFF) | (BYTESWAP_16(uint16_t(v2)) << 16);   // d1 d0  1  0
    case 0x0b:
        return (v1 & 0x00FFFF00) | ((v2 & 0xFF00) << 16) | (v2 & 0xFF); // d0  2  1 d1

    // Dwords
    case 0x10:
        return BYTESWAP_32(v2);              // d3 d2 d1 d0
    case 0x11:
        return ROTL_32(BYTESWAP_32(v2), 8);  // d2 d1 d0 d3
    case 0x12:
        return ROTL_32(BYTESWAP_32(v2), 16); // d1 d0 d3 d2
    case 0x13:
        return ROTR_32(BYTESWAP_32(v2), 8);  // d0 d3 d2 d1

    default:
        return 0xFFFFFFFFUL;
    }
}

#endif /* ENDIAN_SWAP_H */
