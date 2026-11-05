use std::sync::atomic::{AtomicBool, Ordering};

use crate::ui;

static START_PRESSED: AtomicBool = AtomicBool::new(false);

pub(crate) fn configure_hints() {
    unsafe {
        sdl3_sys::hints::SDL_SetHint(sdl3_sys::hints::SDL_HINT_JOYSTICK_WGI, c"1".as_ptr());
        sdl3_sys::hints::SDL_SetHint(c"SDL_WINRT_HANDLE_BACK_BUTTON".as_ptr(), c"1".as_ptr());
        if !sdl3_sys::hints::SDL_GetHintBoolean(sdl3_sys::hints::SDL_HINT_JOYSTICK_WGI, false) {
            super::entry::log("input: Windows Gaming Input is disabled by an SDL hint override");
        }
    }
}

pub(crate) fn auto_bind_player_one(ui: &mut ui::Ui, id: sdl3_sys::joystick::SDL_JoystickID) {
    let config = &ui.config.input;
    let controller = &ui.input.controllers[0];
    let gamepad_profile = config
        .input_profiles
        .get(&config.input_profile_binding[0])
        .is_some_and(|profile| !profile.dinput);
    if !super::gamepad_policy::can_auto_assign(
        config.controller_enabled[0],
        config.controller_assignment.iter().any(Option::is_some),
        gamepad_profile,
        !controller.game_controller.is_null() || !controller.joystick.is_null(),
    ) || !unsafe { sdl3_sys::gamepad::SDL_IsGamepad(id) }
        || !unsafe { sdl3_sys::joystick::SDL_GetJoystickFromID(id) }.is_null()
        || !unsafe { sdl3_sys::gamepad::SDL_GetGamepadFromID(id) }.is_null()
    {
        return;
    }
    let gamepad = unsafe { sdl3_sys::gamepad::SDL_OpenGamepad(id) };
    if gamepad.is_null() {
        let error = unsafe { std::ffi::CStr::from_ptr(sdl3_sys::error::SDL_GetError()) };
        super::entry::log(&format!(
            "input: player 1 gamepad open failed: {}",
            error.to_string_lossy()
        ));
        return;
    }
    ui.input.controllers[0].game_controller = gamepad;
    ui.input.controllers[0].guid = unsafe { sdl3_sys::gamepad::SDL_GetGamepadGUIDForID(id) };
    super::entry::log(&format!(
        "input: player 1 automatically opened gamepad {}",
        u32::from(id)
    ));
}

pub(crate) fn initialize(ui: &mut ui::Ui) {
    START_PRESSED.store(false, Ordering::Relaxed);
    let joysticks = ui::input::get_joysticks();
    let gamepads = joysticks
        .iter()
        .filter(|id| unsafe { sdl3_sys::gamepad::SDL_IsGamepad(**id) })
        .count();
    super::entry::log(&format!(
        "input: detected {} joystick(s), {gamepads} SDL gamepad(s)",
        joysticks.len()
    ));
    for id in joysticks {
        auto_bind_player_one(ui, id);
    }
}

pub(crate) fn disconnected(channel: usize) {
    if channel == 0 {
        START_PRESSED.store(false, Ordering::Relaxed);
        super::entry::log("input: player 1 gamepad disconnected");
    }
}

pub(crate) fn trace_start(
    channel: usize,
    controller: *mut sdl3_sys::gamepad::SDL_Gamepad,
    keys: u32,
) {
    if channel != 0 {
        return;
    }
    let pressed = !controller.is_null()
        && unsafe {
            sdl3_sys::gamepad::SDL_GetGamepadButton(
                controller,
                sdl3_sys::gamepad::SDL_GAMEPAD_BUTTON_START,
            )
        };
    if pressed && !START_PRESSED.swap(pressed, Ordering::Relaxed) {
        super::entry::log(&format!(
            "input: gamepad Start pressed; N64 Start={}",
            keys & (1 << ui::input_profile::START_BUTTON) != 0
        ));
    } else if !pressed {
        START_PRESSED.store(false, Ordering::Relaxed);
    }
}
