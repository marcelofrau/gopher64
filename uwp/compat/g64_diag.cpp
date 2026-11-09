// UWP-PORT: Xbox bring-up diagnostics: frame heartbeat, hang watchdog and the R-02
// CPU-write visibility self-test (docs/xbox-port/04-vulkan-dozen.md M-01, 09-risks.md R-02).
// Everything logs through gopher64_uwp_log_native, the only sink a retail console has. (D-07)
#include "g64_diag.h"

#include "device.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

extern "C" void gopher64_uwp_log_native(const char *message);

namespace {
std::atomic<uint64_t> frame_count{0};
std::atomic<uint64_t> last_frame_ms{0};
std::atomic<bool> watchdog_running{false};
std::thread watchdog_thread;
// Emulator-thread only; reset per game in g64_uwp_watchdog_start.
uint64_t report_ms = 0;
uint64_t report_frames = 0;

void log_fmt(const char *format, unsigned long long a, unsigned long long b) {
  char message[160];
  std::snprintf(message, sizeof(message), format, a, b);
  gopher64_uwp_log_native(message);
}
} // namespace

void g64_uwp_heartbeat_frame() {
  // Called only from the emulator thread (rdp_update_screen).
  const uint64_t now = GetTickCount64();
  const uint64_t frames = frame_count.fetch_add(1) + 1;
  last_frame_ms.store(now);
  if (report_ms == 0) {
    report_ms = now;
    report_frames = frames;
    gopher64_uwp_log_native("heartbeat: first frame presented");
  } else if (now - report_ms >= 2000) {
    log_fmt("heartbeat: frames=%llu (+%llu in the last ~2 s)", frames, frames - report_frames);
    report_ms = now;
    report_frames = frames;
  }
}

void g64_uwp_watchdog_start() {
  if (watchdog_running.exchange(true)) return;
  frame_count.store(0);
  last_frame_ms.store(0);
  report_ms = 0;
  report_frames = 0;
  const uint64_t start_ms = GetTickCount64();
  watchdog_thread = std::thread([start_ms] {
    uint64_t warned_ms = 0;
    while (watchdog_running.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      const uint64_t now = GetTickCount64();
      const uint64_t last = last_frame_ms.load();
      const uint64_t idle = now - (last ? last : start_ms);
      if (idle >= 5000 && now - warned_ms >= 5000) {
        warned_ms = now;
        log_fmt(last ? "watchdog: no frame presented for %llu s (frames=%llu)"
                     : "watchdog: no frame presented yet after %llu s (frames=%llu)",
                idle / 1000, frame_count.load());
      }
    }
  });
}

void g64_uwp_watchdog_stop() {
  if (!watchdog_running.exchange(false)) return;
  if (watchdog_thread.joinable()) watchdog_thread.join();
}

bool g64_uwp_r02_selftest(Vulkan::Device &device) {
  constexpr VkDeviceSize size = 64 * 1024;
  constexpr uint32_t words = size / sizeof(uint32_t);

  Vulkan::BufferCreateInfo info = {};
  info.size = size;
  info.domain = Vulkan::BufferDomain::Host; // same domain the RDP uploads RDRAM through
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  auto upload = device.create_buffer(info);
  info.domain = Vulkan::BufferDomain::Device;
  info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  auto gpu = device.create_buffer(info);
  info.domain = Vulkan::BufferDomain::CachedHost;
  info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  auto readback = device.create_buffer(info);
  if (!upload || !gpu || !readback) {
    gopher64_uwp_log_native("R-02 cpu-write visibility: SKIPPED (buffer allocation failed)");
    return false;
  }

  // CPU writes through the host mapping: the pattern Xbox Series loses with WRITE_BACK memory.
  auto *dst = static_cast<uint32_t *>(device.map_host_buffer(*upload, Vulkan::MEMORY_ACCESS_WRITE_BIT));
  if (!dst) {
    gopher64_uwp_log_native("R-02 cpu-write visibility: SKIPPED (map failed)");
    return false;
  }
  for (uint32_t i = 0; i < words; i++) dst[i] = 0x9E3779B9u * (i + 1);
  device.unmap_host_buffer(*upload, Vulkan::MEMORY_ACCESS_WRITE_BIT);

  auto cmd = device.request_command_buffer();
  cmd->copy_buffer(*gpu, *upload);
  cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
  cmd->copy_buffer(*readback, *gpu);
  cmd->barrier(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
  Vulkan::Fence fence;
  device.submit(cmd, &fence);
  if (!fence || !fence->wait_timeout(5000000000ull)) {
    gopher64_uwp_log_native("R-02 cpu-write visibility: FAIL (GPU copy did not finish within 5 s)");
    device.wait_idle(); // don't release buffers the GPU may still be copying
    return false;
  }

  auto *src = static_cast<const uint32_t *>(device.map_host_buffer(*readback, Vulkan::MEMORY_ACCESS_READ_BIT));
  if (!src) {
    gopher64_uwp_log_native("R-02 cpu-write visibility: SKIPPED (readback map failed)");
    return false;
  }
  uint32_t mismatches = 0, first = 0, got = 0;
  for (uint32_t i = 0; i < words; i++) {
    if (src[i] != 0x9E3779B9u * (i + 1)) {
      if (!mismatches++) {
        first = i;
        got = src[i];
      }
    }
  }
  device.unmap_host_buffer(*readback, Vulkan::MEMORY_ACCESS_READ_BIT);

  char message[192];
  if (!mismatches) {
    std::snprintf(message, sizeof(message), "R-02 cpu-write visibility: ok (%u words)", words);
  } else {
    std::snprintf(message, sizeof(message),
                  "R-02 cpu-write visibility: FAIL (%u/%u words differ; first at %u got 0x%08x expected 0x%08x)",
                  mismatches, words, first, got, 0x9E3779B9u * (first + 1));
  }
  gopher64_uwp_log_native(message);
  return mismatches == 0;
}
