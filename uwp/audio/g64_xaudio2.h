// UWP-PORT: XAudio2 audio backend for the Xbox/UWP target. (D-08)
//
// SDL3's WinRT WASAPI path deadlocks in SDL_OpenAudioDeviceStream: it calls
// ActivateAudioInterfaceAsync and waits on a semaphore with no timeout, and the CoreWindow
// dispatcher never services the completion when opened from the UI thread (proven on Xbox by
// the Ymir port, commit 071639c9). XAudio2 is synchronous and has no such activation step.
//
// This exposes a tiny C ABI that mirrors the slice of SDL_AudioStream gopher64 actually uses
// (src/ui/audio.rs): open, put S16LE stereo data, query queued bytes, gain, frequency ratio,
// pause/resume, close. The emulator pushes samples (play_audio); XAudio2 pulls from an internal
// ring buffer via IXAudio2VoiceCallback, applying the frequency ratio as linear resampling so
// the existing drift control keeps working unchanged.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a backend instance.
typedef struct G64Audio G64Audio;

// Open the default playback device for `channels` channels at `freq` Hz, S16LE. Returns NULL on
// failure (reason via g64_audio_last_error). Playback starts paused.
G64Audio *g64_audio_open(int freq, int channels);

// Push interleaved S16LE frames. `bytes` is the length in bytes (as SDL_PutAudioStreamData).
void g64_audio_put(G64Audio *audio, const void *data, int bytes);

// Bytes currently buffered (queued but not yet played), mirroring SDL_GetAudioStreamQueued.
int g64_audio_queued(G64Audio *audio);

// Change the S16LE format (freq/channels), mirroring SDL_SetAudioStreamFormat.
int g64_audio_set_format(G64Audio *audio, int freq, int channels);

// Linear output gain [0, 2]; and frequency ratio for drift control [0.995, 1.005].
void g64_audio_set_gain(G64Audio *audio, float gain);
float g64_audio_get_gain(G64Audio *audio);
void g64_audio_set_freq_ratio(G64Audio *audio, float ratio);
float g64_audio_get_freq_ratio(G64Audio *audio);

// Pause/resume playback, mirroring SDL_Pause/ResumeAudioStreamDevice.
void g64_audio_pause(G64Audio *audio);
void g64_audio_resume(G64Audio *audio);

// Stop playback and release every resource; the handle is invalid afterwards.
void g64_audio_close(G64Audio *audio);

// Last failure message for the current thread (never NULL).
const char *g64_audio_last_error(void);

#ifdef __cplusplus
}
#endif
