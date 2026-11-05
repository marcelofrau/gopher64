#pragma once

#include "../../parallel-rdp/interface.hpp"
#include <SDL3/SDL.h>
#include <new>

extern "C" void gopher64_uwp_log_native(const char* message);

inline void g64_uwp_queue_gamepad_event(SDL_JoystickID id, SDL_MainThreadCallback callback) {
    auto* joystick_event = new (std::nothrow) JoystickEvent{id, true};
    if (!joystick_event) {
        gopher64_uwp_log_native("input: unable to allocate gamepad event");
    } else if (!SDL_RunOnMainThread(callback, joystick_event, false)) {
        delete joystick_event;
        gopher64_uwp_log_native("input: unable to schedule gamepad event");
    }
}
