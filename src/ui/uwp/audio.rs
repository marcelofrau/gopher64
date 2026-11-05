// UWP-PORT: SDL_AudioStream-compatible shim over the XAudio2 backend (uwp/audio/g64_xaudio2.*).
//
// SDL3's WinRT WASAPI SDL_OpenAudioDeviceStream deadlocks on Xbox (see g64_xaudio2.h, D-08).
// This module mirrors the exact names and signatures of the `sdl3_sys::audio` functions that
// src/ui/audio.rs calls, so that file only needs a gated `use ... as` alias at the top and its
// call sites stay unchanged. XAudio2 is synchronous and has no async activation step. (D-08)
#![allow(
    non_upper_case_globals,
    non_camel_case_types,
    non_snake_case,
    dead_code
)]

use std::ffi::{c_char, c_float, c_int, c_void};

mod ffi {
    #![allow(
        non_upper_case_globals,
        non_camel_case_types,
        non_snake_case,
        dead_code
    )]
    include!(concat!(env!("OUT_DIR"), "/g64_xaudio2_bindings.rs"));
}

/// Opaque stream handle; the Ui struct stores `*mut SDL_AudioStream`, which on UWP is this.
pub type SDL_AudioStream = ffi::G64Audio;

// Format constant used by callers; we only ever produce S16LE stereo so the value is cosmetic.
pub const SDL_AUDIO_S16LE: u32 = 0x8010;
pub type SDL_AudioDeviceID = u32;
pub const SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK: SDL_AudioDeviceID = 0xFFFF_FFFF;

/// Matches the layout the callers build: `{ format, freq, channels }`.
#[repr(C)]
pub struct SDL_AudioSpec {
    pub format: u32,
    pub freq: c_int,
    pub channels: c_int,
}

/// SDL's callback type; accepted and ignored (XAudio2 handles underruns by zero-filling).
pub type SDL_AudioStreamCallback =
    Option<unsafe extern "C" fn(*mut c_void, *mut SDL_AudioStream, c_int, c_int)>;

pub unsafe fn SDL_OpenAudioDeviceStream(
    _devid: SDL_AudioDeviceID,
    spec: *const SDL_AudioSpec,
    _callback: SDL_AudioStreamCallback,
    _userdata: *mut c_void,
) -> *mut SDL_AudioStream {
    let (freq, channels) = if spec.is_null() {
        (44100, 2)
    } else {
        unsafe { ((*spec).freq, (*spec).channels) }
    };
    unsafe { ffi::g64_audio_open(freq, channels) }
}

pub unsafe fn SDL_PutAudioStreamData(
    stream: *mut SDL_AudioStream,
    buf: *const c_void,
    len: c_int,
) -> bool {
    unsafe { ffi::g64_audio_put(stream, buf, len) };
    true
}

pub unsafe fn SDL_GetAudioStreamQueued(stream: *mut SDL_AudioStream) -> c_int {
    unsafe { ffi::g64_audio_queued(stream) }
}

pub unsafe fn SDL_SetAudioStreamFormat(
    stream: *mut SDL_AudioStream,
    src_spec: *const SDL_AudioSpec,
    _dst_spec: *const SDL_AudioSpec,
) -> bool {
    if !src_spec.is_null() {
        return unsafe {
            ffi::g64_audio_set_format(stream, (*src_spec).freq, (*src_spec).channels) != 0
        };
    }
    true
}

pub unsafe fn SDL_SetAudioStreamGain(stream: *mut SDL_AudioStream, gain: c_float) -> bool {
    unsafe { ffi::g64_audio_set_gain(stream, gain) };
    true
}

pub unsafe fn SDL_GetAudioStreamGain(stream: *mut SDL_AudioStream) -> c_float {
    unsafe { ffi::g64_audio_get_gain(stream) }
}

pub unsafe fn SDL_SetAudioStreamFrequencyRatio(
    stream: *mut SDL_AudioStream,
    ratio: c_float,
) -> bool {
    unsafe { ffi::g64_audio_set_freq_ratio(stream, ratio) };
    true
}

pub unsafe fn SDL_GetAudioStreamFrequencyRatio(stream: *mut SDL_AudioStream) -> c_float {
    unsafe { ffi::g64_audio_get_freq_ratio(stream) }
}

pub unsafe fn SDL_SetAudioStreamGetCallback(
    _stream: *mut SDL_AudioStream,
    _callback: SDL_AudioStreamCallback,
    _userdata: *mut c_void,
) -> bool {
    // XAudio2 drives refills from its own voice-callback thread; no SDL-style pull callback.
    true
}

pub unsafe fn SDL_ResumeAudioStreamDevice(stream: *mut SDL_AudioStream) -> bool {
    unsafe { ffi::g64_audio_resume(stream) };
    true
}

pub unsafe fn SDL_PauseAudioStreamDevice(stream: *mut SDL_AudioStream) -> bool {
    unsafe { ffi::g64_audio_pause(stream) };
    true
}

pub unsafe fn SDL_DestroyAudioStream(stream: *mut SDL_AudioStream) {
    unsafe { ffi::g64_audio_close(stream) };
}

/// Last error from the backend (for parity with SDL_GetError in diagnostics).
pub fn last_error() -> String {
    unsafe {
        std::ffi::CStr::from_ptr(ffi::g64_audio_last_error() as *const c_char)
            .to_string_lossy()
            .into_owned()
    }
}
