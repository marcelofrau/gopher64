#include "../g64_xaudio2.cpp"
#include <cassert>

#ifdef NDEBUG
#error This test requires assertions to be enabled.
#endif

extern "C" void gopher64_uwp_log_native(const char *) {}

int main() {
    G64Audio audio;
    audio.output_freq = 33600;
    audio.ring.assign(8192, 0);
    std::vector<int16_t> input(4096);
    for (size_t i = 0; i < input.size() / 2; ++i) {
        input[i * 2] = static_cast<int16_t>(i * 10);
        input[i * 2 + 1] = static_cast<int16_t>(-static_cast<int>(i * 10));
    }
    std::vector<int16_t> output(1024);
    ring_push(&audio, input.data(), input.size());
    assert(g64_audio_set_format(&audio, 32000, 2) == 1);
    assert(audio.resample_locked(output.data(), 512) == 512);
    assert(output[2] == 9 && output[3] == -9);
    assert(output[1022] == 4866 && output[1023] == -4866);
    assert(audio.ring_count == (2048 - 487) * 2);
    assert(g64_audio_set_format(&audio, 0, 2) == 0 && audio.freq == 32000);
    assert(g64_audio_set_format(&audio, 48000, 1) == 0 && audio.freq == 32000);

    audio.ring_head = audio.ring_tail = audio.ring_count = 0;
    audio.resample_pos = 0;
    ring_push(&audio, input.data(), input.size());
    assert(g64_audio_set_format(&audio, 48000, 2) == 1);
    assert(audio.resample_locked(output.data(), 512) == 512);
    assert(output[2] == 14 && output[3] == -14);
    assert(audio.ring_count == (2048 - 731) * 2);

    audio.ring_head = audio.ring_tail = audio.ring_count = 0;
    audio.resample_pos = 0;
    assert(audio.resample_locked(output.data(), 512) == 0);
    assert(audio.underrun_frames == 512 && output[0] == 0 && output.back() == 0);

    audio.underrun_frames = 0;
    audio.resample_pos = 0;
    audio.freq_ratio = 1.005f;
    assert(g64_audio_set_format(&audio, 33600, 2) == 1);
    ring_push(&audio, input.data(), input.size());
    assert(audio.resample_locked(output.data(), 512) == 512);
    assert(audio.ring_count == (2048 - 514) * 2);

    G64Audio small_ring;
    small_ring.ring.assign(4, 0);
    ring_push(&small_ring, input.data(), 6);
    ring_push(&small_ring, input.data(), 2);
    assert(small_ring.overflow_samples == 4 && small_ring.ring_count == 4);
}
