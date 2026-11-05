#include <SDL3/SDL.h>
#include <cstdio>

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "%s: %s\n", #expr, SDL_GetError()); return 1; } } while (0)

struct Events { unsigned joystick_added = 0, gamepad_added = 0, joystick_removed = 0; };

static bool SDLCALL watch(void* data, SDL_Event* event) {
    auto& events = *static_cast<Events*>(data);
    if (event->type == SDL_EVENT_JOYSTICK_ADDED) ++events.joystick_added;
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) ++events.gamepad_added;
    if (event->type == SDL_EVENT_JOYSTICK_REMOVED) ++events.joystick_removed;
    return true;
}

int main() {
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));
    Events events;
    CHECK(SDL_AddEventWatch(watch, &events));
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    auto id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id != 0);
    SDL_PumpEvents();
    CHECK(events.joystick_added > 0 && events.gamepad_added > 0);
    CHECK(SDL_IsGamepad(id));
    auto* gamepad = SDL_OpenGamepad(id);
    CHECK(gamepad != nullptr);
    auto* joystick = SDL_GetGamepadJoystick(gamepad);
    CHECK(joystick != nullptr);
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, true));
    SDL_PumpEvents();
    CHECK(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START));
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_START, false));
    SDL_PumpEvents();
    CHECK(!SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START));
    CHECK(SDL_DetachVirtualJoystick(id));
    SDL_PumpEvents();
    CHECK(events.joystick_removed > 0);
    SDL_CloseGamepad(gamepad);
    SDL_RemoveEventWatch(watch, &events);
    SDL_Quit();
    std::puts("PASS: virtual SDL gamepad enumeration, Start and hotplug");
}
