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

#include <core/hostevents.h>
#include <core/coresignal.h>
#include <cpu/ppc/ppcemu.h>
#include <devices/common/adb/adbkeyboard.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/viacuda.h>
#include <loguru.hpp>
#include <SDL3/SDL.h>

namespace loguru {
    enum : Verbosity {
        Verbosity_HOSTEVENTS = loguru::Verbosity_9
    };
}

EventManager* EventManager::event_manager;

static int get_sdl_event_key_code(const SDL_KeyboardEvent& event, uint32_t kbd_locale);

constexpr SDL_Keymod KMOD_ALL = (SDL_Keymod)(
    SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT | SDL_KMOD_LCTRL | SDL_KMOD_RCTRL |
    SDL_KMOD_LALT | SDL_KMOD_RALT | SDL_KMOD_LGUI | SDL_KMOD_RGUI
);

static const char * get_event_name(int32_t x) {
    switch (x) {
        #define oneevent(x) case x: return #x ;
        oneevent(SDL_EVENT_FIRST)
        oneevent(SDL_EVENT_QUIT)
        oneevent(SDL_EVENT_TERMINATING)
        oneevent(SDL_EVENT_LOW_MEMORY)
        oneevent(SDL_EVENT_WILL_ENTER_BACKGROUND)
        oneevent(SDL_EVENT_DID_ENTER_BACKGROUND)
        oneevent(SDL_EVENT_WILL_ENTER_FOREGROUND)
        oneevent(SDL_EVENT_DID_ENTER_FOREGROUND)
        oneevent(SDL_EVENT_LOCALE_CHANGED)
        oneevent(SDL_EVENT_SYSTEM_THEME_CHANGED)
        case 0x150: return "SDL_DISPLAYEVENT";
        oneevent(SDL_EVENT_DISPLAY_ORIENTATION)
        oneevent(SDL_EVENT_DISPLAY_ADDED)
        oneevent(SDL_EVENT_DISPLAY_REMOVED)
        oneevent(SDL_EVENT_DISPLAY_MOVED)
        oneevent(SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED)
        oneevent(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED)
        oneevent(SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)
        case 0x158: return "SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED";
        case 0x200: return "SDL_WINDOWEVENT";
        case 0x201: return "SDL_SYSWMEVENT";
        oneevent(SDL_EVENT_WINDOW_SHOWN)
        oneevent(SDL_EVENT_WINDOW_HIDDEN)
        oneevent(SDL_EVENT_WINDOW_EXPOSED)
        oneevent(SDL_EVENT_WINDOW_MOVED)
        oneevent(SDL_EVENT_WINDOW_RESIZED)
        oneevent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        oneevent(SDL_EVENT_WINDOW_METAL_VIEW_RESIZED)
        oneevent(SDL_EVENT_WINDOW_MINIMIZED)
        oneevent(SDL_EVENT_WINDOW_MAXIMIZED)
        oneevent(SDL_EVENT_WINDOW_RESTORED)
        oneevent(SDL_EVENT_WINDOW_MOUSE_ENTER)
        oneevent(SDL_EVENT_WINDOW_MOUSE_LEAVE)
        oneevent(SDL_EVENT_WINDOW_FOCUS_GAINED)
        oneevent(SDL_EVENT_WINDOW_FOCUS_LOST)
        oneevent(SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        oneevent(SDL_EVENT_WINDOW_HIT_TEST)
        oneevent(SDL_EVENT_WINDOW_ICCPROF_CHANGED)
        oneevent(SDL_EVENT_WINDOW_DISPLAY_CHANGED)
        oneevent(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
        oneevent(SDL_EVENT_WINDOW_SAFE_AREA_CHANGED)
        oneevent(SDL_EVENT_WINDOW_OCCLUDED)
        oneevent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)
        oneevent(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
        oneevent(SDL_EVENT_WINDOW_DESTROYED)
        oneevent(SDL_EVENT_WINDOW_HDR_STATE_CHANGED)
        oneevent(SDL_EVENT_KEY_DOWN)
        oneevent(SDL_EVENT_KEY_UP)
        oneevent(SDL_EVENT_TEXT_EDITING)
        oneevent(SDL_EVENT_TEXT_INPUT)
        oneevent(SDL_EVENT_KEYMAP_CHANGED)
        oneevent(SDL_EVENT_KEYBOARD_ADDED)
        oneevent(SDL_EVENT_KEYBOARD_REMOVED)
        oneevent(SDL_EVENT_TEXT_EDITING_CANDIDATES)
        case 0x308: return "SDL_EVENT_SCREEN_KEYBOARD_SHOWN";
        case 0x309: return "SDL_EVENT_SCREEN_KEYBOARD_HIDDEN";
        oneevent(SDL_EVENT_MOUSE_MOTION)
        oneevent(SDL_EVENT_MOUSE_BUTTON_DOWN)
        oneevent(SDL_EVENT_MOUSE_BUTTON_UP)
        oneevent(SDL_EVENT_MOUSE_WHEEL)
        oneevent(SDL_EVENT_MOUSE_ADDED)
        oneevent(SDL_EVENT_MOUSE_REMOVED)
        oneevent(SDL_EVENT_JOYSTICK_AXIS_MOTION)
        oneevent(SDL_EVENT_JOYSTICK_BALL_MOTION)
        oneevent(SDL_EVENT_JOYSTICK_HAT_MOTION)
        oneevent(SDL_EVENT_JOYSTICK_BUTTON_DOWN)
        oneevent(SDL_EVENT_JOYSTICK_BUTTON_UP)
        oneevent(SDL_EVENT_JOYSTICK_ADDED)
        oneevent(SDL_EVENT_JOYSTICK_REMOVED)
        oneevent(SDL_EVENT_JOYSTICK_BATTERY_UPDATED)
        oneevent(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE)
        oneevent(SDL_EVENT_GAMEPAD_AXIS_MOTION)
        oneevent(SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        oneevent(SDL_EVENT_GAMEPAD_BUTTON_UP)
        oneevent(SDL_EVENT_GAMEPAD_ADDED)
        oneevent(SDL_EVENT_GAMEPAD_REMOVED)
        oneevent(SDL_EVENT_GAMEPAD_REMAPPED)
        oneevent(SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN)
        oneevent(SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION)
        oneevent(SDL_EVENT_GAMEPAD_TOUCHPAD_UP)
        oneevent(SDL_EVENT_GAMEPAD_SENSOR_UPDATE)
        oneevent(SDL_EVENT_GAMEPAD_UPDATE_COMPLETE)
        oneevent(SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED)
        oneevent(SDL_EVENT_FINGER_DOWN)
        oneevent(SDL_EVENT_FINGER_UP)
        oneevent(SDL_EVENT_FINGER_MOTION)
        oneevent(SDL_EVENT_FINGER_CANCELED)
        case 0x710: return "SDL_EVENT_PINCH_BEGIN";
        case 0x711: return "SDL_EVENT_PINCH_UPDATE";
        case 0x712: return "SDL_EVENT_PINCH_END";
        oneevent(SDL_EVENT_CLIPBOARD_UPDATE)
        oneevent(SDL_EVENT_DROP_FILE)
        oneevent(SDL_EVENT_DROP_TEXT)
        oneevent(SDL_EVENT_DROP_BEGIN)
        oneevent(SDL_EVENT_DROP_COMPLETE)
        oneevent(SDL_EVENT_DROP_POSITION)
        oneevent(SDL_EVENT_AUDIO_DEVICE_ADDED)
        oneevent(SDL_EVENT_AUDIO_DEVICE_REMOVED)
        oneevent(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED)
        oneevent(SDL_EVENT_SENSOR_UPDATE)
        oneevent(SDL_EVENT_PEN_PROXIMITY_IN)
        oneevent(SDL_EVENT_PEN_PROXIMITY_OUT)
        oneevent(SDL_EVENT_PEN_DOWN)
        oneevent(SDL_EVENT_PEN_UP)
        oneevent(SDL_EVENT_PEN_BUTTON_DOWN)
        oneevent(SDL_EVENT_PEN_BUTTON_UP)
        oneevent(SDL_EVENT_PEN_MOTION)
        oneevent(SDL_EVENT_PEN_AXIS)
        oneevent(SDL_EVENT_CAMERA_DEVICE_ADDED)
        oneevent(SDL_EVENT_CAMERA_DEVICE_REMOVED)
        oneevent(SDL_EVENT_CAMERA_DEVICE_APPROVED)
        oneevent(SDL_EVENT_CAMERA_DEVICE_DENIED)
        oneevent(SDL_EVENT_RENDER_TARGETS_RESET)
        oneevent(SDL_EVENT_RENDER_DEVICE_RESET)
        oneevent(SDL_EVENT_RENDER_DEVICE_LOST)
        oneevent(SDL_EVENT_PRIVATE0)
        oneevent(SDL_EVENT_PRIVATE1)
        oneevent(SDL_EVENT_PRIVATE2)
        oneevent(SDL_EVENT_PRIVATE3)
        oneevent(SDL_EVENT_POLL_SENTINEL)
        oneevent(SDL_EVENT_USER)
        oneevent(SDL_EVENT_LAST)
        oneevent(SDL_EVENT_ENUM_PADDING)
        #undef oneevent
        default: return "unknown";
    }
}

bool g_swap_command_option = false;

static AdbKey swap_command_option(AdbKey key)
{
    if (!g_swap_command_option)
        return key;
    switch (key) {
    case AdbKey_Option:      return AdbKey_Command;
    case AdbKey_RightOption: return AdbKey_Command;
    case AdbKey_Command:     return AdbKey_Option;
    default:                 return key;
    }
}

void EventManager::set_keyboard_locale(uint32_t keyboard_id) {
    this->kbd_locale = keyboard_id;
}

void EventManager::poll_events() {
    SDL_Event event;
    bool host_input = false; // set when an input event is delivered to the guest

    while (SDL_PollEvent(&event)) {
        events_captured++;

        switch (event.type) {
        case SDL_EVENT_QUIT:
            LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
            power_off(po_quit);
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s key:%s repeat:%d",
                    event.type, get_event_name(event.type), SDL_GetScancodeName(event.key.scancode), event.key.repeat);
                if (event.key.repeat)
                    break;

                // Internal shortcuts, intentionally not sent to the host.
                // Control-G: mouse grab
                if (event.key.key == SDLK_G && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we;
                        we.sub_type  = DPPC_WINDOWEVENT_MOUSE_GRAB_TOGGLE;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                        we.sub_type  = DPPC_WINDOWEVENT_MOUSE_GRAB_CHANGED;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-S: scale quality
                if (event.key.key == SDLK_S && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_SCALE_QUALITY_TOGGLE;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-F: fullscreen
                if (event.key.key == SDLK_F && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-Shift-F: fullscreen reverse
                if (event.key.key == SDLK_F && (event.key.mod & KMOD_ALL) == (SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT)) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE_REVERSE;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-+: bigger
                if (event.key.key == SDLK_EQUALS && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_BIGGER;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control--: smaller
                if (event.key.key == SDLK_MINUS && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_SMALLER;
                        we.window_id = event.key.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-Alt-Shift-+: increase instruction period
                if (event.key.key == SDLK_EQUALS &&
                    (event.key.mod & KMOD_ALL) == (SDL_KMOD_LCTRL | SDL_KMOD_LALT | SDL_KMOD_LSHIFT)
                ) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        uint64_t instruction_period = increment_instruction_period();
                        LOG_F(INFO, "Incremented instruction period: %d", (int)instruction_period);
                    }
                    return;
                }
                // Control-Alt-Shift--: decrease instruction period
                if (event.key.key == SDLK_MINUS &&
                    (event.key.mod & KMOD_ALL) == (SDL_KMOD_LCTRL | SDL_KMOD_LALT | SDL_KMOD_LSHIFT)
                ) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        uint64_t instruction_period = decrement_instruction_period();
                        LOG_F(INFO, "Decremented instruction period: %d", (int)instruction_period);
                    }
                    return;
                }

                // Control-Alt-R: g_realtime toggle
                if (event.key.key == SDLK_R && (event.key.mod & KMOD_ALL) == (SDL_KMOD_LCTRL | SDL_KMOD_LALT)) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        bool g_realtime_status = toggle_g_realtime();
                        LOG_F(INFO, "g_realtime: %s", g_realtime_status ? "enabled" : "disabled");
                    }
                }

                // Control-L: log toggle
                if (event.key.key == SDLK_L && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        loguru::Verbosity new_verbosity = loguru::g_stderr_verbosity;
                        if (new_verbosity < loguru::Verbosity_INFO)
                            new_verbosity = loguru::Verbosity_INFO;
                        else if (new_verbosity < loguru::Verbosity_MAX)
                            new_verbosity = loguru::Verbosity_MAX;
                        else
                            new_verbosity = loguru::Verbosity_OFF;
                        loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
                        LOG_F(INFO, "g_stderr_verbosity: %d", new_verbosity);
                        loguru::g_stderr_verbosity = new_verbosity;
                    }
                    return;
                }
                // Control-D: debugger
                if (event.key.key == SDLK_D && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        power_off(po_enter_debugger);
                    }
                    return;
                }
                // Control-I: cuda interrupt
                if (event.key.key == SDLK_I && (event.key.mod & KMOD_ALL) == SDL_KMOD_LCTRL) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        LOG_F(INFO, "CUDA interrupt");
                        InterruptCtrl* int_ctrl = dynamic_cast<InterruptCtrl*>(
                            gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
                        int_ctrl->ack_int(int_ctrl->register_int(IntSrc::VIA_CUDA), 1);
                    }
                    return;
                }
                if (event.key.key == SDLK_I && (event.key.mod & KMOD_ALL) == (SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT)) {
                    if (event.type == SDL_EVENT_KEY_UP) {
                        LOG_F(INFO, "CUDA SR interrupt");
                        ViaCuda* via_obj = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));
                        if (via_obj)
                            via_obj->schedule_sr_int(0);
                    }
                    return;
                }
                // Ralt+delete => ctrl+alt+del
                if (event.key.key == SDLK_DELETE && ((event.key.mod & KMOD_ALL) == SDL_KMOD_RALT) != 0) {
                    KeyboardEvent ke{};
                    ke.key = AdbKey_Control;

                    if (event.type == SDL_EVENT_KEY_DOWN) {
                        ke.flags = KEYBOARD_EVENT_DOWN;
                        key_downs++;
                    } else {
                        ke.flags = KEYBOARD_EVENT_UP;
                        key_ups++;
                    }

                    host_input = true;
                    this->_keyboard_signal.emit(ke);
                    ke.key = AdbKey_Delete;
                    this->_keyboard_signal.emit(ke);
                    return;
                }
                int key_code = get_sdl_event_key_code(event.key, this->kbd_locale);
                if (key_code != -1) {
                    KeyboardEvent ke{};
                    ke.key = key_code;
                    if (event.type == SDL_EVENT_KEY_DOWN) {
                        ke.flags = KEYBOARD_EVENT_DOWN;
                        key_downs++;
                    } else {
                        ke.flags = KEYBOARD_EVENT_UP;
                        key_ups++;
                    }
                    // Caps Lock is a special case, since it's a toggle key
                    if (ke.key == AdbKey_CapsLock) {
                        ke.flags = event.key.mod & SDL_KMOD_CAPS ?
                            KEYBOARD_EVENT_DOWN : KEYBOARD_EVENT_UP;
                    }
                    host_input = true;
                    this->_keyboard_signal.emit(ke);
                } else {
                    LOG_F(WARNING, "Unknown key 0x%X pressed", event.key.key);
                }
            }
            break;

        case SDL_EVENT_MOUSE_MOTION: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
                MouseEvent me{};
                me.xrel  = (int32_t)event.motion.xrel;
                me.yrel  = (int32_t)event.motion.yrel;
                me.xabs  = (uint32_t)event.motion.x;
                me.yabs  = (uint32_t)event.motion.y;
                me.flags = MOUSE_EVENT_MOTION;
                host_input = true;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
                MouseEvent me{};
                uint8_t adb_button;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT   : adb_button = 0; break;
                    case SDL_BUTTON_MIDDLE : adb_button = 2; break;
                    case SDL_BUTTON_RIGHT  : adb_button = 1; break;
                    default                : adb_button = event.button.button - 1;
                }
                me.buttons_state = (this->buttons_state |= (1 << adb_button));
                me.xabs  = (uint32_t)event.button.x;
                me.yabs  = (uint32_t)event.button.y;
                me.flags = MOUSE_EVENT_BUTTON;
                host_input = true;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
                MouseEvent me{};
                uint8_t adb_button;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT   : adb_button = 0; break;
                    case SDL_BUTTON_MIDDLE : adb_button = 2; break;
                    case SDL_BUTTON_RIGHT  : adb_button = 1; break;
                    default                : adb_button = event.button.button - 1;
                }
                me.buttons_state = (this->buttons_state &= ~(1 << adb_button));
                me.xabs  = (uint32_t)event.button.x;
                me.yabs  = (uint32_t)event.button.y;
                me.flags = MOUSE_EVENT_BUTTON;
                host_input = true;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
                GamepadEvent ge{};
                switch (event.gbutton.button) {
                    case SDL_GAMEPAD_BUTTON_BACK:           ge.button = GamepadButton::FrontLeft;    break;
                    case SDL_GAMEPAD_BUTTON_GUIDE:          ge.button = GamepadButton::FrontMiddle;  break;
                    case SDL_GAMEPAD_BUTTON_START:          ge.button = GamepadButton::FrontRight;   break;
                    case SDL_GAMEPAD_BUTTON_NORTH:          ge.button = GamepadButton::Blue;         break;
                    case SDL_GAMEPAD_BUTTON_WEST:           ge.button = GamepadButton::Yellow;       break;
                    case SDL_GAMEPAD_BUTTON_DPAD_UP:        ge.button = GamepadButton::Up;           break;
                    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      ge.button = GamepadButton::Left;         break;
                    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     ge.button = GamepadButton::Right;        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      ge.button = GamepadButton::Down;         break;
                    case SDL_GAMEPAD_BUTTON_SOUTH:          ge.button = GamepadButton::Red;          break;
                    case SDL_GAMEPAD_BUTTON_EAST:           ge.button = GamepadButton::Green;        break;
                    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: ge.button = GamepadButton::RightTrigger; break;
                    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  ge.button = GamepadButton::LeftTrigger;  break;
                    default: break;
                }
                ge.gamepad_id = event.gbutton.which;
                ge.flags = GAMEPAD_EVENT_DOWN;
                host_input = true;
                this->_gamepad_signal.emit(ge);
            }
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
                GamepadEvent ge{};
                switch (event.gbutton.button) {
                    case SDL_GAMEPAD_BUTTON_BACK:           ge.button = GamepadButton::FrontLeft;    break;
                    case SDL_GAMEPAD_BUTTON_GUIDE:          ge.button = GamepadButton::FrontMiddle;  break;
                    case SDL_GAMEPAD_BUTTON_START:          ge.button = GamepadButton::FrontRight;   break;
                    case SDL_GAMEPAD_BUTTON_NORTH:          ge.button = GamepadButton::Blue;         break;
                    case SDL_GAMEPAD_BUTTON_WEST:           ge.button = GamepadButton::Yellow;       break;
                    case SDL_GAMEPAD_BUTTON_DPAD_UP:        ge.button = GamepadButton::Up;           break;
                    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      ge.button = GamepadButton::Left;         break;
                    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     ge.button = GamepadButton::Right;        break;
                    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      ge.button = GamepadButton::Down;         break;
                    case SDL_GAMEPAD_BUTTON_SOUTH:          ge.button = GamepadButton::Red;          break;
                    case SDL_GAMEPAD_BUTTON_EAST:           ge.button = GamepadButton::Green;        break;
                    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: ge.button = GamepadButton::RightTrigger; break;
                    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  ge.button = GamepadButton::LeftTrigger;  break;
                    default: break;
                }
                ge.gamepad_id = event.gbutton.which;
                ge.flags = GAMEPAD_EVENT_UP;
                host_input = true;
                this->_gamepad_signal.emit(ge);
            }
            break;

        case SDL_EVENT_DROP_FILE: {
                const char* path = event.drop.data;
                // TODO: Distinguish CD-ROM and floppy images once dynamic
                // floppy insertion is supported.
                CdromImageEvent cdrom_event{};
                cdrom_event.image_path = path;
                this->post_cdrom_event(cdrom_event);

                switch (cdrom_event.result) {
                case CdromInsertionResult::SUCCESS:
                    LOG_F(INFO, "Inserted CD-ROM image: %s", path);
                    break;
                case CdromInsertionResult::NO_DRIVE:
                    LOG_F(ERROR, "Cannot insert CD-ROM image; no CD-ROM drive is available: %s",
                          path);
                    break;
                case CdromInsertionResult::MEDIA_PRESENT:
                    LOG_F(ERROR, "Cannot insert CD-ROM image; eject the current media first: %s",
                          path);
                    break;
                case CdromInsertionResult::IMAGE_OPEN_FAILED:
                    LOG_F(ERROR, "Cannot insert CD-ROM image: %s", path);
                    break;
                }
            }
            break;

        default:
            LOG_F(HOSTEVENTS, "event: 0x%X = %s", event.type, get_event_name(event.type));
            if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
                WindowEvent we{};
                we.sub_type = event.type;
                we.window_id = event.window.windowID;
                this->_window_signal.emit(we);
                break;
            }
            unhandled_events++;
        }
    }

    // perform post-processing
    this->_post_signal.emit();

    if (host_input)
        mark_host_input();
}

void EventManager::post_keyboard_state_events() {
    int count;
    int numkeys;
    const bool *states = SDL_GetKeyboardState(&numkeys);
    SDL_Keymod modstate = SDL_GetModState();

    SDL_KeyboardEvent keyevent = { .type = SDL_EVENT_KEY_DOWN };
    SDL_Scancode scancode;
    KeyboardEvent ke{};

    typedef struct {
        SDL_Scancode scancode;
        SDL_Keymod   keymod;
        AdbKey       adbkey;
    } Modifier_t;

    static Modifier_t modifiers[] = {
        { SDL_SCANCODE_LSHIFT       , SDL_KMOD_LSHIFT , AdbKey_Shift        },
        { SDL_SCANCODE_RSHIFT       , SDL_KMOD_RSHIFT , AdbKey_RightShift   },
        { SDL_SCANCODE_LCTRL        , SDL_KMOD_LCTRL  , AdbKey_Control      },
        { SDL_SCANCODE_RCTRL        , SDL_KMOD_RCTRL  , AdbKey_RightControl },
        { SDL_SCANCODE_LALT         , SDL_KMOD_LALT   , AdbKey_Option       },
        { SDL_SCANCODE_RALT         , SDL_KMOD_RALT   , AdbKey_RightOption  },
        { SDL_SCANCODE_LGUI         , SDL_KMOD_LGUI   , AdbKey_Command      },
        { SDL_SCANCODE_RGUI         , SDL_KMOD_RGUI   , AdbKey_Command      },
//      { SDL_SCANCODE_NUMLOCKCLEAR , SDL_KMOD_NUM    , AdbKey_KeypadClear  },
        { SDL_SCANCODE_CAPSLOCK     , SDL_KMOD_CAPS   , AdbKey_CapsLock     },
//      { SDL_SCANCODE_MODE         , SDL_KMOD_MODE   , AdbKey_????         },
//      { SDL_SCANCODE_SCROLLLOCK   , SDL_KMOD_SCROLL , AdbKey_F14          },
        { SDL_SCANCODE_UNKNOWN      , SDL_KMOD_NONE   , (AdbKey)0           }
    };

    LOG_F(INFO, "Current keyboard state:");

    LOG_F(INFO, "    Modifiers:");
    count = 0;
    for (Modifier_t *mod = modifiers; mod->scancode != SDL_SCANCODE_UNKNOWN; mod++) {
        if (!(modstate & mod->keymod))
            continue;
        LOG_F(INFO, "        Modifier: %s", SDL_GetScancodeName(mod->scancode));
        count++;
        ke.key = swap_command_option(mod->adbkey);
        ke.flags = KEYBOARD_EVENT_DOWN;
        this->_keyboard_signal.emit(ke);
    }
    if (!count)
        LOG_F(INFO, "        (none)");

    LOG_F(INFO, "    Keys and Modifiers:");
    count = 0;
    for (int i = 0; i < numkeys; i++) {
        if (!states[i])
            continue;

        count++;
        scancode = (SDL_Scancode)i;

        Modifier_t *mod = modifiers;
        for (; mod->scancode != SDL_SCANCODE_UNKNOWN && mod->scancode != scancode; mod++);
        if (mod->scancode == scancode) {
            LOG_F(INFO, "        Modifier: %s", SDL_GetScancodeName(scancode));
            continue;
        }

        LOG_F(INFO, "        Key: %s", SDL_GetScancodeName(scancode));
        keyevent.scancode = scancode;
        keyevent.key = SDL_GetKeyFromScancode(scancode, modstate, false);
        keyevent.mod = modstate;

        int key_code = get_sdl_event_key_code(keyevent, this->kbd_locale);
        if (key_code != -1) {
            ke.key = key_code;
            ke.flags = KEYBOARD_EVENT_DOWN;
            this->_keyboard_signal.emit(ke);
        } else {
            LOG_F(WARNING, "        Unknown key 0x%X pressed", keyevent.key);
        }
    }
    if (!count)
        LOG_F(INFO, "        (none)");
}

static int get_sdl_event_key_code(const SDL_KeyboardEvent &event, uint32_t kbd_locale)
{
    switch (event.key) {
    case SDLK_A:            return AdbKey_A;
    case SDLK_B:            return AdbKey_B;
    case SDLK_C:            return AdbKey_C;
    case SDLK_D:            return AdbKey_D;
    case SDLK_E:            return AdbKey_E;
    case SDLK_F:            return AdbKey_F;
    case SDLK_G:            return AdbKey_G;
    case SDLK_H:            return AdbKey_H;
    case SDLK_I:            return AdbKey_I;
    case SDLK_J:            return AdbKey_J;
    case SDLK_K:            return AdbKey_K;
    case SDLK_L:            return AdbKey_L;
    case SDLK_M:            return AdbKey_M;
    case SDLK_N:            return AdbKey_N;
    case SDLK_O:            return AdbKey_O;
    case SDLK_P:            return AdbKey_P;
    case SDLK_Q:            return AdbKey_Q;
    case SDLK_R:            return AdbKey_R;
    case SDLK_S:            return AdbKey_S;
    case SDLK_T:            return AdbKey_T;
    case SDLK_U:            return AdbKey_U;
    case SDLK_V:            return AdbKey_V;
    case SDLK_W:            return AdbKey_W;
    case SDLK_X:            return AdbKey_X;
    case SDLK_Y:            return AdbKey_Y;
    case SDLK_Z:            return AdbKey_Z;

    case SDLK_1:            return AdbKey_1;
    case SDLK_2:            return AdbKey_2;
    case SDLK_3:            return AdbKey_3;
    case SDLK_4:            return AdbKey_4;
    case SDLK_5:            return AdbKey_5;
    case SDLK_6:            return AdbKey_6;
    case SDLK_7:            return AdbKey_7;
    case SDLK_8:            return AdbKey_8;
    case SDLK_9:            return AdbKey_9;
    case SDLK_0:            return AdbKey_0;

    case SDLK_ESCAPE:       return AdbKey_Escape;
    case SDLK_GRAVE:        return AdbKey_Grave;
    case SDLK_MINUS:        return AdbKey_Minus;
    case SDLK_EQUALS:       return AdbKey_Equal;
    case SDLK_LEFTBRACKET:  return AdbKey_LeftBracket;
    case SDLK_RIGHTBRACKET: return AdbKey_RightBracket;
    case SDLK_BACKSLASH:    return AdbKey_Backslash;
    case SDLK_SEMICOLON:    return AdbKey_Semicolon;
    case SDLK_APOSTROPHE:   return AdbKey_Quote;
    case SDLK_COMMA:        return AdbKey_Comma;
    case SDLK_PERIOD:       return AdbKey_Period;
    case SDLK_SLASH:        return AdbKey_Slash;

    // Convert shifted variants to unshifted
    case SDLK_EXCLAIM:      return AdbKey_1;
    case SDLK_AT:           return AdbKey_2;
    case SDLK_HASH:         return AdbKey_3;
    case SDLK_DOLLAR:       return AdbKey_4;
    case SDLK_UNDERSCORE:   return AdbKey_Minus;
    case SDLK_PLUS:         return AdbKey_Equal;
    case SDLK_COLON:        return AdbKey_Semicolon;
    case SDLK_DBLAPOSTROPHE: return AdbKey_Quote;
    case SDLK_LESS:         return AdbKey_Comma;
    case SDLK_GREATER:      return AdbKey_Period;
    case SDLK_QUESTION:     return AdbKey_Slash;

    case SDLK_TAB:          return AdbKey_Tab;
    case SDLK_RETURN:       return AdbKey_Return;
    case SDLK_SPACE:        return AdbKey_Space;
    case SDLK_BACKSPACE:    return AdbKey_Delete;

    case SDLK_DELETE:       return AdbKey_ForwardDelete;
    case SDLK_INSERT:       return AdbKey_Help;
    case SDLK_HOME:         return AdbKey_Home;
    case SDLK_HELP:         return AdbKey_Home;
    case SDLK_END:          return AdbKey_End;
    case SDLK_PAGEUP:       return AdbKey_PageUp;
    case SDLK_PAGEDOWN:     return AdbKey_PageDown;

    case SDLK_LCTRL:        return AdbKey_Control;
    case SDLK_RCTRL:        return AdbKey_RightControl;
    case SDLK_LSHIFT:       return AdbKey_Shift;
    case SDLK_RSHIFT:       return AdbKey_RightShift;
    case SDLK_LALT:         return swap_command_option(AdbKey_Option);
    case SDLK_RALT:         return swap_command_option(AdbKey_RightOption);
    case SDLK_LGUI:         return swap_command_option(AdbKey_Command);
    case SDLK_RGUI:         return swap_command_option(AdbKey_Command);
    case SDLK_MENU:         return AdbKey_Grave;
    case SDLK_CAPSLOCK:     return AdbKey_CapsLock;

    case SDLK_UP:           return AdbKey_ArrowUp;
    case SDLK_DOWN:         return AdbKey_ArrowDown;
    case SDLK_LEFT:         return AdbKey_ArrowLeft;
    case SDLK_RIGHT:        return AdbKey_ArrowRight;

    case SDLK_KP_0:         return AdbKey_Keypad0;
    case SDLK_KP_1:         return AdbKey_Keypad1;
    case SDLK_KP_2:         return AdbKey_Keypad2;
    case SDLK_KP_3:         return AdbKey_Keypad3;
    case SDLK_KP_4:         return AdbKey_Keypad4;
    case SDLK_KP_5:         return AdbKey_Keypad5;
    case SDLK_KP_6:         return AdbKey_Keypad6;
    case SDLK_KP_7:         return AdbKey_Keypad7;
    case SDLK_KP_9:         return AdbKey_Keypad9;
    case SDLK_KP_8:         return AdbKey_Keypad8;
    case SDLK_KP_PERIOD:    return AdbKey_KeypadDecimal;
    case SDLK_KP_PLUS:      return AdbKey_KeypadPlus;
    case SDLK_KP_MINUS:     return AdbKey_KeypadMinus;
    case SDLK_KP_MULTIPLY:  return AdbKey_KeypadMultiply;
    case SDLK_KP_DIVIDE:    return AdbKey_KeypadDivide;
    case SDLK_KP_ENTER:     return AdbKey_KeypadEnter;
    case SDLK_KP_EQUALS:    return AdbKey_KeypadEquals;
    case SDLK_NUMLOCKCLEAR: return AdbKey_KeypadClear;

    case SDLK_F1:           return AdbKey_F1;
    case SDLK_F2:           return AdbKey_F2;
    case SDLK_F3:           return AdbKey_F3;
    case SDLK_F4:           return AdbKey_F4;
    case SDLK_F5:           return AdbKey_F5;
    case SDLK_F6:           return AdbKey_F6;
    case SDLK_F7:           return AdbKey_F7;
    case SDLK_F8:           return AdbKey_F8;
    case SDLK_F9:           return AdbKey_F9;
    case SDLK_F10:          return AdbKey_F10;
    case SDLK_F11:          return AdbKey_F11;
    case SDLK_F12:          return AdbKey_F12;
    case SDLK_PRINTSCREEN:  return AdbKey_F13;
    case SDLK_SCROLLLOCK:   return AdbKey_F14;
    case SDLK_PAUSE:        return AdbKey_F15;
    }

    // International keyboard support - check by scancode
    switch (event.scancode) {
    // Japanese keyboard
    case SDL_SCANCODE_INTERNATIONAL3:
        if (kbd_locale == Jpn_JPN)
            return AdbKey_JIS_Yen;
        else
            return -1;
    case SDL_SCANCODE_INTERNATIONAL1:
        return AdbKey_JIS_Underscore;
    case SDL_SCANCODE_INTERNATIONAL2:
        return AdbKey_JIS_Kana;
    default:
        break;
    }

    // Non-standard keycodes for international characters
    switch (event.key) {
    case 0XBC:
        return AdbKey_JIS_KP_Comma;
    case 0X89:
        return AdbKey_JIS_Eisu;

    // German keyboard
    case 0XB4:        return AdbKey_Slash;
    case 0X5E:        return AdbKey_ISO1;
    case 0XDF:        return AdbKey_Minus;       // Eszett
    case 0XE4:        return AdbKey_LeftBracket; // A-umlaut
    case 0XF6:        return AdbKey_Semicolon;   // O-umlaut
    case 0XFC:        return AdbKey_LeftBracket; // U-umlaut

    // French keyboard
    case 0X29:        return AdbKey_Minus;             // Right parenthesis
    case 0X43:        return AdbKey_KeypadMultiply;    // Star/Mu
    // 0XB2 is superscript 2. Which Mac key should this one map to?
    case 0XF9:        return AdbKey_Quote;             // U-grave

    // Italian keyboard
    case 0XE0:        return AdbKey_9;              // A-grave
    case 0XE8:        return AdbKey_6;              // E-grave
    case 0XEC:        return AdbKey_LeftBracket;    // I-grave
    case 0XF2:        return AdbKey_KeypadMultiply; // O-grave

    // Spanish keyboard
    case 0XA1:        return AdbKey_Comma;        // Inverted question mark
    case 0XBA:        return AdbKey_6;            // Backslash
    case 0XE7:        return AdbKey_Slash;        // C-cedilla
    case 0XF1:        return AdbKey_Semicolon;    // N-tilde
    case 0X4000002f:
        return AdbKey_LeftBracket;    // Acute
    case 0X40000034:
        return AdbKey_Semicolon;    // Acute
    }
    return -1;
}
