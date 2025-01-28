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

/** @file Dingus video output definitions. */

#ifndef DINGUS_VIDEO_REGS_H
#define DINGUS_VIDEO_REGS_H

// Memory-mapped registers.

namespace DingusVideoRegsMeta {

    enum DingusVideoMetaRegIndex : int {
        NUM_DISPLAYS = 0,
        MAX_REG
    };

};

namespace DingusVideoRegsDisplay {

    enum DingusVideoDisplayRegIndex : int {
        HACTIVE = 0,
        HFRONT,
        HBACK,
        HTOTAL,
        VACTIVE,
        VFRONT,
        VBACK,
        VTOTAL,
        MON_SENSE,
        TIMING_FLAGS,
        IMMEDIATE_FLAGS,
        PIXEL_CLOCK,
        PIXEL_DEPTH,
        FRAMEBUFFER_BASE,
        FRAMEBUFFER_ROWBYTES,
        INT_ENABLE,
        INT_STATUS,
        HWCURSOR_BASE,
        HWCURSOR_WIDTH,
        HWCURSOR_POS,
        COLOR_INDEX,
        COLOR_DATA,
        MAX_REG
    };

};

// Bit definitions.
enum {
    // TIMING_FLAGS
    INTERLACED      = 1 <<  0, // 0 - interlaced, 1 - progressive
    VSYNC_POLARITY  = 1 <<  1, // 0 - negative, 1 - positive
    HSYNC_POLARITY  = 1 <<  2, // 0 - negative, 1 - positive
    HWCURSOR_24     = 1 <<  3, // 0 - indexed color - 24 bit color cursor

    // IMMEDIATE_FLAGS
    HWCURSOR_ENABLE = 1 <<  0, // 1 - enable hw cursor, 0 - disable it
    DISABLE_TIMING  = 1 <<  1, // 0 - enable video timing, 1 - disable it
    VSYNC_DISABLE   = 1 <<  2, // 0 - enable vertical sync, 1 - disable it
    HSYNC_DISABLE   = 1 <<  3, // 0 - enable horizontal sync, 1 - disable it
    CSYNC_DISABLE   = 1 <<  4, // 0 - enable composite sync, 1 - disable it
    BLANK_DISABLE   = 1 <<  5, // 0 - enable blanking, 1 - disable it
    DO_LATCH        = 1 <<  6, // 1 - do latch all regs

    // INT_ENABLE
    VBL_IRQ_CLR  = 1 << 0, // VBL interrupt clear  bit
    VBL_IRQ_EN   = 1 << 1, // VBL interrupt enable bit

    // INT_STATUS
    VBL_IRQ_STAT = 1 << 0, // VBL interrupt status bit (INT_STATUS)
};

#endif // DINGUS_VIDEO_REGS_H
