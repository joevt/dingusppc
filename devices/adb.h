/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#ifndef ADB_H
#define ADB_H

#include <array>
#include <cinttypes>
#include <thirdparty/SDL2/include/SDL.h>
#include <thirdparty/SDL2/include/SDL_events.h>
#include "adbkeybd.h"
#include "adbmouse.h"

class ADB_Bus
{
public:
    ADB_Bus();
    ~ADB_Bus();

    void add_adb_device(int type);
    bool adb_verify_listen(int device, int reg);
    bool adb_verify_talk(int device, int reg);

    uint8_t get_input_byte(int offset);
    uint8_t get_output_byte(int offset);

    int get_input_len();
    int get_output_len();

private:
    ADB_Keybd* keyboard;
    ADB_Mouse* mouse;

    uint8_t input_data_stream[16]; //temp buffer
    int input_stream_len;
    uint8_t output_data_stream[16]; //temp buffer
    int output_stream_len;
};

#endif /* ADB_H */