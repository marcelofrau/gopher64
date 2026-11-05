use crate::device;
use crate::netplay;
use crate::ui;

// UWP-PORT: route the audio stream through the XAudio2 shim on UWP (SDL's WinRT WASAPI path
// deadlocks in SDL_OpenAudioDeviceStream). The shim mirrors sdl3_sys::audio's names and
// signatures so every call site below is an unchanged `aud::` call. (D-08)
#[cfg(target_vendor = "uwp")]
use crate::ui::uwp::audio as aud;
#[cfg(not(target_vendor = "uwp"))]
use sdl3_sys::audio as aud;

static ADJUST_LOCKED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

pub fn init(device: &mut device::Device) {
    ui::sdl_init(sdl3_sys::init::SDL_INIT_AUDIO);
    init_game_audio(device);
}

unsafe extern "C" fn audio_callback(
    _userdata: *mut std::ffi::c_void,
    audio_stream: *mut aud::SDL_AudioStream,
    additional_amount: i32,
    _total_amount: i32,
) {
    if additional_amount > 0 {
        unsafe {
            aud::SDL_PutAudioStreamData(
                audio_stream,
                vec![0; additional_amount as usize].as_ptr() as *const std::ffi::c_void,
                additional_amount,
            )
        };

        adjust_audio_frequency(audio_stream, -0.0005);
    }
}

pub fn update_freq(device: &mut device::Device) {
    let game_audio_spec = aud::SDL_AudioSpec {
        format: aud::SDL_AUDIO_S16LE,
        freq: device.ai.freq as i32,
        channels: 2,
    };
    unsafe {
        aud::SDL_SetAudioStreamFormat(
            device.ui.audio.audio_stream,
            &game_audio_spec,
            std::ptr::null(),
        );
    }

    if device.ai.regs[device::ai::AI_STATUS_REG] & device::ai::AI_STATUS_BUSY != 0 {
        resume_game_audio(&mut device.ui);
    } else {
        pause_game_audio(&mut device.ui);
    }
}

pub fn init_game_audio(device: &mut device::Device) {
    let game_audio_spec = aud::SDL_AudioSpec {
        format: aud::SDL_AUDIO_S16LE,
        freq: device.ai.freq as i32,
        channels: 2,
    };

    device.ui.audio.audio_stream = unsafe {
        aud::SDL_OpenAudioDeviceStream(
            aud::SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &game_audio_spec,
            None,
            std::ptr::null_mut(),
        )
    };
    if device.ui.audio.audio_stream.is_null() {
        panic!("Could not create audio stream");
    }
    if !unsafe {
        aud::SDL_SetAudioStreamGain(device.ui.audio.audio_stream, device.ui.audio.gain)
            && aud::SDL_SetAudioStreamGetCallback(
                device.ui.audio.audio_stream,
                Some(audio_callback),
                std::ptr::null_mut(),
            )
    } {
        panic!("Could not initialize audio stream");
    }
    if device.ai.regs[device::ai::AI_STATUS_REG] & device::ai::AI_STATUS_BUSY != 0 {
        resume_game_audio(&mut device.ui);
    } else {
        pause_game_audio(&mut device.ui);
    }
}

pub fn close(ui: &mut ui::Ui) {
    close_game_audio(ui);
}

pub fn resume_game_audio(ui: &mut ui::Ui) {
    unsafe {
        aud::SDL_ResumeAudioStreamDevice(ui.audio.audio_stream);
    }
}

pub fn pause_game_audio(ui: &mut ui::Ui) {
    unsafe {
        aud::SDL_PauseAudioStreamDevice(ui.audio.audio_stream);
    }
}

pub fn close_game_audio(ui: &mut ui::Ui) {
    unsafe {
        if !ui.audio.audio_stream.is_null() {
            aud::SDL_DestroyAudioStream(ui.audio.audio_stream);
            ui.audio.audio_stream = std::ptr::null_mut();
        }
    }
}

pub fn lower_audio_volume(ui: &mut ui::Ui) {
    unsafe {
        ui.audio.gain = aud::SDL_GetAudioStreamGain(ui.audio.audio_stream) - 0.05;
        if ui.audio.gain < 0.0 {
            ui.audio.gain = 0.0;
        }
        aud::SDL_SetAudioStreamGain(ui.audio.audio_stream, ui.audio.gain);
    }
    ui::video::onscreen_message(
        &format!("Audio volume: {:.0}%", ui.audio.gain * 100.0),
        ui::video::MESSAGE_LENGTH_MESSAGE_VERY_SHORT,
    );
}

pub fn raise_audio_volume(ui: &mut ui::Ui) {
    unsafe {
        ui.audio.gain = aud::SDL_GetAudioStreamGain(ui.audio.audio_stream) + 0.05;
        if ui.audio.gain > 2.0 {
            ui.audio.gain = 2.0;
        }
        aud::SDL_SetAudioStreamGain(ui.audio.audio_stream, ui.audio.gain);
    }
    ui::video::onscreen_message(
        &format!("Audio volume: {:.0}%", ui.audio.gain * 100.0),
        ui::video::MESSAGE_LENGTH_MESSAGE_VERY_SHORT,
    );
}

unsafe extern "C" fn lock_callback(
    _userdata: *mut std::ffi::c_void,
    _id: sdl3_sys::timer::SDL_TimerID,
    _interval: u32,
) -> u32 {
    ADJUST_LOCKED.store(false, std::sync::atomic::Ordering::Relaxed);
    0
}

fn adjust_audio_frequency(audio_stream: *mut aud::SDL_AudioStream, frequency: f32) {
    if ADJUST_LOCKED.load(std::sync::atomic::Ordering::Relaxed) {
        return;
    }

    ADJUST_LOCKED.store(true, std::sync::atomic::Ordering::Relaxed);

    unsafe {
        sdl3_sys::timer::SDL_AddTimer(1000, Some(lock_callback), std::ptr::null_mut());
        let current_ratio = aud::SDL_GetAudioStreamFrequencyRatio(audio_stream);
        aud::SDL_SetAudioStreamFrequencyRatio(
            audio_stream,
            (current_ratio + frequency).clamp(0.995, 1.005),
        );

        /*
        println!(
            "Adjusted audio frequency ratio to {}",
            aud::SDL_GetAudioStreamFrequencyRatio(audio_stream)
        );
        */
    }
}

pub fn play_audio(device: &device::Device, dram_addr: usize, length: u64) {
    if device.ui.audio.audio_stream.is_null() {
        return;
    }

    if netplay::in_rollback(device.netplay.as_ref()) {
        return;
    }

    let mut primary_buffer: Vec<i16> = vec![0; length as usize / 2];
    let mut i = 0;
    while i < length as usize / 2 {
        // Left channel
        primary_buffer[i] = *device.rdram.mem.get(dram_addr + (i * 2) + 2).unwrap_or(&0) as i16
            | ((*device.rdram.mem.get(dram_addr + (i * 2) + 3).unwrap_or(&0) as i16) << 8);

        // Right channel
        primary_buffer[i + 1] = *device.rdram.mem.get(dram_addr + (i * 2)).unwrap_or(&0) as i16
            | ((*device.rdram.mem.get(dram_addr + (i * 2) + 1).unwrap_or(&0) as i16) << 8);
        i += 2;
    }

    let audio_queued =
        unsafe { aud::SDL_GetAudioStreamQueued(device.ui.audio.audio_stream) } as f64;
    let samples_per_frame = device.ai.freq as f64 * device.vi.frame_time * 4.0;
    let max_latency = samples_per_frame * 8.0;
    if audio_queued < max_latency {
        unsafe {
            aud::SDL_PutAudioStreamData(
                device.ui.audio.audio_stream,
                primary_buffer.as_ptr() as *const std::ffi::c_void,
                primary_buffer.len() as i32 * 2,
            )
        };
    } else if device.ui.speed_limiter.enabled {
        adjust_audio_frequency(device.ui.audio.audio_stream, 0.0005);
    }
}
