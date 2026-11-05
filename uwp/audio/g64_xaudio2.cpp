// UWP-PORT: XAudio2 audio backend implementation. See g64_xaudio2.h. (D-08)
//
// Architecture (adapted from Ymir's xaudio2_backend.cpp, commit 071639c9, to gopher64's push
// model): the emulator pushes S16LE stereo frames into a lock-guarded ring buffer. XAudio2 owns
// a source voice with N recycled buffers; its IXAudio2VoiceCallback::OnBufferEnd thread refills
// the next buffer from the ring, applying the input/output rates and ratio as linear resampling. If the ring
// runs dry the tail is zero-filled (no click-free hold yet; matches SDL's underrun silence).
// The producer and audio callback share a mutex; diagnostic output runs after it is released.

#include "g64_xaudio2.h"

#include <windows.h>
#include <xaudio2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

extern "C" void gopher64_uwp_log_native(const char *message);

namespace {

constexpr size_t kBufferCount = 4;      // recycled XAudio2 buffers in flight
constexpr size_t kFramesPerBuffer = 512; // stereo frames per buffer (matches Ymir's low-latency choice)

thread_local char g_last_error[256] = {0};

void set_error(const char *msg) {
    std::strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

} // namespace

struct G64Audio {
    IXAudio2 *engine = nullptr;
    IXAudio2MasteringVoice *mastering = nullptr;
    IXAudio2SourceVoice *source = nullptr;

    std::mutex mutex;
    int freq = 44100;
    int output_freq = 44100;
    int channels = 2;
    float gain = 1.0f;
    float freq_ratio = 1.0f;
    std::atomic<bool> running{false};

    // Ring buffer of interleaved S16 samples (not frames).
    std::vector<int16_t> ring;
    size_t ring_head = 0; // read position
    size_t ring_tail = 0; // write position
    size_t ring_count = 0; // samples currently stored

    // Fractional read cursor for linear resampling.
    double resample_pos = 0.0;

    // Recycled output buffers.
    std::vector<std::vector<int16_t>> buffers;
    std::array<bool, kBufferCount> in_flight{};
    size_t next_buffer = 0;
    uint64_t underrun_frames = 0;
    uint64_t overflow_samples = 0;
    uint64_t submit_failures = 0;
    HRESULT last_submit_error = S_OK;
    uint64_t last_report_ms = 0;

    struct VoiceCallback;
    VoiceCallback *callback = nullptr;

    void refill_locked();
    size_t resample_locked(int16_t *out, size_t frames);
    void release_locked();
};

// --- ring helpers (call with mutex held) -----------------------------------
namespace {

void ring_push(G64Audio *a, const int16_t *src, size_t samples) {
    const size_t cap = a->ring.size();
    if (cap == 0) {
        return;
    }
    // Drop oldest if overflowing, to bound latency (the emulator also limits via queued()).
    if (samples > cap) {
        a->overflow_samples += samples - cap;
        src += (samples - cap);
        samples = cap;
    }
    if (samples > cap - a->ring_count) {
        a->overflow_samples += samples - (cap - a->ring_count);
    }
    for (size_t i = 0; i < samples; ++i) {
        a->ring[a->ring_tail] = src[i];
        a->ring_tail = (a->ring_tail + 1) % cap;
        if (a->ring_count == cap) {
            a->ring_head = (a->ring_head + 1) % cap; // overwrite oldest
        } else {
            a->ring_count++;
        }
    }
}

// Read one interleaved stereo frame (2 samples) at integer offset `frame` from head, without
// consuming. Returns false if not enough data.
bool ring_peek_frame(const G64Audio *a, size_t frame, int16_t &l, int16_t &r) {
    const size_t cap = a->ring.size();
    const size_t sample_index = frame * 2;
    if (sample_index + 1 >= a->ring_count) {
        return false;
    }
    l = a->ring[(a->ring_head + sample_index) % cap];
    r = a->ring[(a->ring_head + sample_index + 1) % cap];
    return true;
}

// Consume `frames` stereo frames from the head.
void ring_consume_frames(G64Audio *a, size_t frames) {
    const size_t cap = a->ring.size();
    size_t samples = frames * 2;
    if (samples > a->ring_count) {
        samples = a->ring_count;
    }
    a->ring_head = (a->ring_head + samples) % cap;
    a->ring_count -= samples;
}

} // namespace

// --- XAudio2 voice callback -------------------------------------------------
struct G64Audio::VoiceCallback final : public IXAudio2VoiceCallback {
    explicit VoiceCallback(G64Audio &owner) noexcept : owner(owner) {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() noexcept override {}
    void STDMETHODCALLTYPE OnStreamEnd() noexcept override {}
    void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT) noexcept override {}
    void STDMETHODCALLTYPE OnBufferStart(void *) noexcept override {}
    void STDMETHODCALLTYPE OnLoopEnd(void *) noexcept override {}
    void STDMETHODCALLTYPE OnBufferEnd(void *context) noexcept override {
        const size_t index = reinterpret_cast<size_t>(context);
        std::lock_guard<std::mutex> lock(owner.mutex);
        if (index < kBufferCount) {
            owner.in_flight[index] = false;
        }
        if (owner.running.load(std::memory_order_relaxed)) {
            owner.refill_locked();
        }
    }
    G64Audio &owner;
};

size_t G64Audio::resample_locked(int16_t *out, size_t frames) {
    // shortcut: queued samples use the latest input rate, track rate segments if games switch mid-buffer.
    const double step = static_cast<double>(freq_ratio) * freq / output_freq;
    size_t produced = 0;
    for (size_t f = 0; f < frames; ++f) {
        const size_t base = static_cast<size_t>(resample_pos);
        int16_t l0, r0, l1, r1;
        if (!ring_peek_frame(this, base, l0, r0)) {
            break; // ran dry
        }
        // Linear interpolation toward the next frame when available.
        const double frac = resample_pos - static_cast<double>(base);
        if (frac > 0.0 && ring_peek_frame(this, base + 1, l1, r1)) {
            out[f * 2] = static_cast<int16_t>(l0 + (l1 - l0) * frac);
            out[f * 2 + 1] = static_cast<int16_t>(r0 + (r1 - r0) * frac);
        } else {
            out[f * 2] = l0;
            out[f * 2 + 1] = r0;
        }
        resample_pos += step;
        produced++;
    }

    // Advance the ring by the whole frames we consumed, keep the fractional remainder.
    const size_t consumed = static_cast<size_t>(resample_pos);
    ring_consume_frames(this, consumed);
    resample_pos -= static_cast<double>(consumed);

    // Zero-fill the tail on underrun.
    if (produced < frames) {
        std::memset(out + produced * 2, 0, (frames - produced) * 2 * sizeof(int16_t));
    }
    underrun_frames += frames - produced;
    return produced;
}

void G64Audio::refill_locked() {
    if (source == nullptr) {
        return;
    }
    // Find a free buffer slot.
    size_t slot = kBufferCount;
    for (size_t i = 0; i < kBufferCount; ++i) {
        const size_t candidate = (next_buffer + i) % kBufferCount;
        if (!in_flight[candidate]) {
            slot = candidate;
            break;
        }
    }
    if (slot == kBufferCount) {
        return; // all queued; wait for the next OnBufferEnd
    }

    std::vector<int16_t> &out = buffers[slot];
    resample_locked(out.data(), kFramesPerBuffer);

    XAUDIO2_BUFFER xbuf{};
    xbuf.AudioBytes = static_cast<UINT32>(kFramesPerBuffer * 2 * sizeof(int16_t));
    xbuf.pAudioData = reinterpret_cast<const BYTE *>(out.data());
    xbuf.pContext = reinterpret_cast<void *>(slot);
    const HRESULT result = source->SubmitSourceBuffer(&xbuf);
    if (FAILED(result)) {
        ++submit_failures;
        last_submit_error = result;
        return;
    }
    in_flight[slot] = true;
    next_buffer = (slot + 1) % kBufferCount;
}

void G64Audio::release_locked() {
    running.store(false, std::memory_order_relaxed);
    if (source != nullptr) {
        source->Stop(0);
        source->FlushSourceBuffers();
        source->DestroyVoice();
        source = nullptr;
    }
    if (mastering != nullptr) {
        mastering->DestroyVoice();
        mastering = nullptr;
    }
    if (engine != nullptr) {
        engine->Release();
        engine = nullptr;
    }
    in_flight.fill(false);
    next_buffer = 0;
}

// --- C ABI ------------------------------------------------------------------
extern "C" {

G64Audio *g64_audio_open(int freq, int channels) {
    if (freq <= 0 || channels != 2) {
        set_error("invalid audio format");
        return nullptr;
    }
    auto *a = new (std::nothrow) G64Audio();
    if (a == nullptr) {
        set_error("out of memory");
        return nullptr;
    }
    a->freq = freq;
    a->output_freq = freq;
    a->channels = channels;
    a->callback = new G64Audio::VoiceCallback(*a);

    // ~0.5 s of stereo samples; bounds latency and absorbs jitter.
    a->ring.assign(static_cast<size_t>(freq) * channels / 2, 0);
    a->buffers.resize(kBufferCount);
    for (auto &b : a->buffers) {
        b.assign(kFramesPerBuffer * 2, 0);
    }

    std::lock_guard<std::mutex> lock(a->mutex);
    if (FAILED(XAudio2Create(&a->engine, 0, XAUDIO2_DEFAULT_PROCESSOR)) || a->engine == nullptr) {
        set_error("XAudio2Create failed");
        a->release_locked();
        delete a->callback;
        delete a;
        return nullptr;
    }
    // This SDK's CreateMasteringVoice has no channel-mask argument (7 params).
    if (FAILED(a->engine->CreateMasteringVoice(&a->mastering, static_cast<UINT32>(channels),
                                               static_cast<UINT32>(freq), 0, nullptr, nullptr,
                                               AudioCategory_GameEffects)) ||
        a->mastering == nullptr) {
        set_error("CreateMasteringVoice failed");
        a->release_locked();
        delete a->callback;
        delete a;
        return nullptr;
    }
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = static_cast<WORD>(channels);
    fmt.nSamplesPerSec = static_cast<DWORD>(freq);
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = static_cast<WORD>(fmt.nChannels * (fmt.wBitsPerSample / 8));
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.cbSize = 0;
    if (FAILED(a->engine->CreateSourceVoice(&a->source, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                            a->callback)) ||
        a->source == nullptr) {
        set_error("CreateSourceVoice failed");
        a->release_locked();
        delete a->callback;
        delete a;
        return nullptr;
    }
    a->source->SetVolume(a->gain);
    return a;
}

void g64_audio_put(G64Audio *audio, const void *data, int bytes) {
    if (audio == nullptr || data == nullptr || bytes <= 0) {
        return;
    }
    struct {
        int freq, output_freq;
        float ratio;
        uint64_t queued, underrun, overflow, failures, span_ms;
        HRESULT last_error;
    } snapshot{};
    {
        std::lock_guard<std::mutex> lock(audio->mutex);
        ring_push(audio, static_cast<const int16_t *>(data), static_cast<size_t>(bytes) / sizeof(int16_t));
        const uint64_t now = GetTickCount64();
        if (audio->last_report_ms == 0) {
            audio->last_report_ms = now;
        } else if (now - audio->last_report_ms >= 5000) {
            snapshot = {audio->freq, audio->output_freq, audio->freq_ratio, audio->ring_count / 2,
                        audio->underrun_frames, audio->overflow_samples, audio->submit_failures,
                        now - audio->last_report_ms, audio->last_submit_error};
            audio->underrun_frames = 0;
            audio->overflow_samples = 0;
            audio->submit_failures = 0;
            audio->last_report_ms = now;
        }
    }
    if (snapshot.span_ms != 0) {
        char report[384];
        std::snprintf(report, sizeof(report),
                      "audio: input=%dHz voice=%dHz ratio=%.6f queued=%llu frames underrun=%llu frames "
                      "overflow=%llu samples submit_failures=%llu last_submit=0x%08lX span=%llums",
                      snapshot.freq, snapshot.output_freq, snapshot.ratio,
                      static_cast<unsigned long long>(snapshot.queued),
                      static_cast<unsigned long long>(snapshot.underrun),
                      static_cast<unsigned long long>(snapshot.overflow),
                      static_cast<unsigned long long>(snapshot.failures),
                      static_cast<unsigned long>(snapshot.last_error),
                      static_cast<unsigned long long>(snapshot.span_ms));
        gopher64_uwp_log_native(report);
    }
}

int g64_audio_queued(G64Audio *audio) {
    if (audio == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    return static_cast<int>(audio->ring_count * sizeof(int16_t));
}

int g64_audio_set_format(G64Audio *audio, int freq, int channels) {
    if (audio == nullptr || freq <= 0 || channels != 2) {
        set_error("invalid audio format");
        gopher64_uwp_log_native("audio: rejected invalid input format");
        return 0;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    audio->freq = freq;
    audio->channels = channels;
    return 1;
}

void g64_audio_set_gain(G64Audio *audio, float gain) {
    if (audio == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    audio->gain = gain;
    if (audio->source != nullptr) {
        audio->source->SetVolume(gain);
    }
}

float g64_audio_get_gain(G64Audio *audio) {
    if (audio == nullptr) {
        return 1.0f;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    return audio->gain;
}

void g64_audio_set_freq_ratio(G64Audio *audio, float ratio) {
    if (audio == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    audio->freq_ratio = ratio;
}

float g64_audio_get_freq_ratio(G64Audio *audio) {
    if (audio == nullptr) {
        return 1.0f;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    return audio->freq_ratio;
}

void g64_audio_pause(G64Audio *audio) {
    if (audio == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    if (audio->source != nullptr) {
        audio->source->Stop(0);
    }
    audio->running.store(false, std::memory_order_relaxed);
}

void g64_audio_resume(G64Audio *audio) {
    if (audio == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(audio->mutex);
    if (audio->source == nullptr) {
        return;
    }
    if (FAILED(audio->source->Start(0))) {
        set_error("source->Start failed");
        return;
    }
    audio->running.store(true, std::memory_order_relaxed);
    for (size_t i = 0; i < kBufferCount; ++i) {
        audio->refill_locked();
    }
}

void g64_audio_close(G64Audio *audio) {
    if (audio == nullptr) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(audio->mutex);
        audio->release_locked();
    }
    delete audio->callback;
    delete audio;
}

const char *g64_audio_last_error(void) {
    return g_last_error;
}

} // extern "C"
