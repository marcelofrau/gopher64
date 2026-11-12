// UWP-PORT: Xbox bring-up diagnostics: frame heartbeat, hang watchdog and the R-02
// CPU-write visibility self-test (docs/xbox-port/04-vulkan-dozen.md M-01, 09-risks.md R-02).
// Everything logs through gopher64_uwp_log_native, the only sink a retail console has. (D-07)
#include "g64_diag.h"

#include "device.hpp"
#include "rdp_device.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

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

// ---- VI-distortion probe (docs/xbox-port/23-xbox-visual-distortion-plan.md) ----
namespace {
uint32_t vi_regs[unsigned(RDP::VIRegister::Count)] = {};
uint64_t vi_frame = 0;
bool vi_config_read = false;
unsigned vi_disable = 0; // bit per G64_VI_DISABLE token
std::vector<uint64_t> vi_dump_frames;

const char *const vi_tokens[] = {"aa", "scale", "serrate", "dither", "divot", "gamma"};

std::string env_utf8(const char *name) {
  char value[512] = {};
  const DWORD length = GetEnvironmentVariableA(name, value, sizeof(value));
  return length && length < sizeof(value) ? std::string(value) : std::string();
}

void vi_read_config() {
  vi_config_read = true;
  const std::string disable = env_utf8("G64_VI_DISABLE");
  for (unsigned i = 0; i < 6; i++)
    if (disable == "all" || ("," + disable + ",").find(std::string(",") + vi_tokens[i] + ",") != std::string::npos)
      vi_disable |= 1u << i;
  const std::string frames = env_utf8("G64_VI_DUMP_FRAMES");
  for (const char *p = frames.c_str(); *p;) {
    char *end = nullptr;
    const unsigned long long frame = std::strtoull(p, &end, 10);
    if (end == p) break;
    if (frame) vi_dump_frames.push_back(frame);
    p = *end ? end + 1 : end;
  }
  char message[256];
  std::snprintf(message, sizeof(message), "vi-probe: G64_VI_DISABLE=\"%s\" (mask=0x%x) G64_VI_DUMP_FRAMES=\"%s\" (%zu frames)",
                disable.c_str(), vi_disable, frames.c_str(), vi_dump_frames.size());
  gopher64_uwp_log_native(message);
}

// 24-bit bottom-up BMP; pixels are 0x00RRGGBB.
bool write_bmp(const std::string &path, unsigned width, unsigned height, const std::vector<uint32_t> &pixels) {
  FILE *file = nullptr;
  if (fopen_s(&file, path.c_str(), "wb") || !file) return false;
  const unsigned stride = (width * 3 + 3) & ~3u;
  const uint32_t data_size = stride * height, file_size = 54 + data_size;
  uint8_t header[54] = {'B', 'M'};
  auto put32 = [&](int at, uint32_t v) { std::memcpy(header + at, &v, 4); };
  put32(2, file_size);
  put32(10, 54);
  put32(14, 40);
  put32(18, width);
  put32(22, height);
  header[26] = 1;
  header[28] = 24;
  put32(34, data_size);
  std::fwrite(header, 1, sizeof(header), file);
  std::vector<uint8_t> row(stride);
  for (unsigned y = height; y-- > 0;) {
    for (unsigned x = 0; x < width; x++) {
      const uint32_t p = pixels[size_t(y) * width + x];
      row[x * 3 + 0] = uint8_t(p);
      row[x * 3 + 1] = uint8_t(p >> 8);
      row[x * 3 + 2] = uint8_t(p >> 16);
    }
    std::fwrite(row.data(), 1, stride, file);
  }
  return std::fclose(file) == 0;
}

uint32_t fnv1a(const std::vector<uint32_t> &pixels) {
  uint32_t h = 2166136261u;
  for (uint32_t p : pixels) h = (h ^ p) * 16777619u;
  return h;
}

void vi_dump(RDP::CommandProcessor &processor, const RDP::ScanoutOptions &options, const uint8_t *rdram,
             uint32_t rdram_size) {
  std::string dir = env_utf8("G64_LOCALSTATE");
  if (dir.empty()) {
    gopher64_uwp_log_native("vi-probe: G64_LOCALSTATE not set, dump skipped");
    return;
  }
  if (dir.back() != '\\' && dir.back() != '/') dir += '\\';
  const std::string prefix = dir + "g64-vi-f" + std::to_string(vi_frame);
  char message[512];

  // Stage 1: what the RDP rendered, as the CPU sees RDRAM after all GPU work has drained.
  processor.idle();
  const uint32_t control = vi_regs[unsigned(RDP::VIRegister::Control)];
  const uint32_t origin = vi_regs[unsigned(RDP::VIRegister::Origin)] & 0xffffff;
  const uint32_t width = vi_regs[unsigned(RDP::VIRegister::Width)] & 0xfff;
  const uint32_t v_start = vi_regs[unsigned(RDP::VIRegister::VStart)];
  const uint32_t y_scale = vi_regs[unsigned(RDP::VIRegister::YScale)] & 0xfff;
  const unsigned type = control & 3, bpp = type == 3 ? 4 : 2;
  uint32_t height = ((((v_start & 0x3ff) - ((v_start >> 16) & 0x3ff)) >> 1) * y_scale) >> 10;
  if (!height || height > 576) height = 240;
  if (width && type >= 2 && origin < rdram_size)
    height = std::min<uint32_t>(height, (rdram_size - origin) / (width * bpp));
  std::snprintf(message, sizeof(message),
                "vi-probe: frame %llu VI control=0x%08x origin=0x%06x width=%u v_start=0x%08x x_scale=0x%08x "
                "y_scale=0x%08x h_start=0x%08x -> %ux%u %s",
                (unsigned long long)vi_frame, control, origin, width, v_start,
                vi_regs[unsigned(RDP::VIRegister::XScale)], vi_regs[unsigned(RDP::VIRegister::YScale)],
                vi_regs[unsigned(RDP::VIRegister::HStart)], width, height,
                type == 3 ? "RGBA8888" : type == 2 ? "RGBA5551" : "blank");
  gopher64_uwp_log_native(message);
  if (rdram && width && height && type >= 2) {
    std::vector<uint32_t> pixels(size_t(width) * height);
    const auto *words = reinterpret_cast<const uint32_t *>(rdram);
    const auto *halves = reinterpret_cast<const uint16_t *>(rdram);
    for (uint32_t i = 0; i < width * height; i++) {
      if (type == 3) {
        const uint32_t w = words[(origin >> 2) + i];
        pixels[i] = (w >> 8) & 0xffffff;
      } else {
        // RDRAM is stored as host-endian 32-bit words, so a 16-bit pixel sits at halfword index ^ 1.
        const uint16_t p = halves[((origin >> 1) + i) ^ 1];
        pixels[i] = (((p >> 11) & 31u) << 19) | (((p >> 6) & 31u) << 11) | (((p >> 1) & 31u) << 3);
      }
    }
    const bool ok = write_bmp(prefix + "-rdram.bmp", width, height, pixels);
    std::snprintf(message, sizeof(message), "vi-probe: %s-rdram.bmp %ux%u fnv=0x%08x %s", prefix.c_str(), width,
                  height, fnv1a(pixels), ok ? "written" : "WRITE FAILED");
    gopher64_uwp_log_native(message);
  }

  // Stage 2: the full VI pipeline output (extract_vram -> fetch -> divot -> scale), read back to the CPU.
  std::vector<RDP::RGBA> colors;
  unsigned out_width = 0, out_height = 0;
  processor.scanout_sync(colors, out_width, out_height, options);
  if (out_width && out_height) {
    std::vector<uint32_t> pixels(colors.size());
    for (size_t i = 0; i < colors.size(); i++)
      pixels[i] = (uint32_t(colors[i].r) << 16) | (uint32_t(colors[i].g) << 8) | colors[i].b;
    const bool ok = write_bmp(prefix + "-scanout.bmp", out_width, out_height, pixels);
    std::snprintf(message, sizeof(message), "vi-probe: %s-scanout.bmp %ux%u fnv=0x%08x %s", prefix.c_str(),
                  out_width, out_height, fnv1a(pixels), ok ? "written" : "WRITE FAILED");
  } else {
    std::snprintf(message, sizeof(message), "vi-probe: frame %llu scanout_sync returned no image",
                  (unsigned long long)vi_frame);
  }
  gopher64_uwp_log_native(message);
}
} // namespace

void g64_uwp_vi_note_register(uint32_t reg, uint32_t value) {
  if (reg < unsigned(RDP::VIRegister::Count)) vi_regs[reg] = value;
}

void g64_uwp_vi_probe(RDP::CommandProcessor &processor, RDP::ScanoutOptions &options, const uint8_t *rdram,
                      uint32_t rdram_size) {
  if (!vi_config_read) vi_read_config();
  vi_frame++;
  if (vi_disable & 1) options.vi.aa = false;
  if (vi_disable & 2) options.vi.scale = false;
  if (vi_disable & 4) options.vi.serrate = false;
  if (vi_disable & 8) options.vi.dither_filter = false;
  if (vi_disable & 16) options.vi.divot_filter = false;
  if (vi_disable & 32) options.vi.gamma_dither = false;
  for (uint64_t frame : vi_dump_frames)
    if (frame == vi_frame) vi_dump(processor, options, rdram, rdram_size);
}
