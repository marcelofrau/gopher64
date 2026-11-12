// UWP-PORT: Xbox bring-up diagnostics, see g64_diag.cpp. (D-07)
#pragma once

#include <cstdint>

namespace Vulkan {
class Device;
}
namespace RDP {
class CommandProcessor;
struct ScanoutOptions;
}

void g64_uwp_heartbeat_frame();
// VI-distortion probe, configured from LocalState/env.txt (docs/xbox-port/23-xbox-visual-distortion-plan.md):
// G64_VI_DISABLE=aa,scale,serrate,dither,divot,gamma turns VI stages off; G64_VI_DUMP_FRAMES=600,1500
// writes the RDRAM framebuffer and the VI output of those frames as BMPs to G64_LOCALSTATE.
void g64_uwp_vi_note_register(uint32_t reg, uint32_t value);
void g64_uwp_vi_probe(RDP::CommandProcessor &processor, RDP::ScanoutOptions &options, const uint8_t *rdram,
                      uint32_t rdram_size);
void g64_uwp_watchdog_start();
void g64_uwp_watchdog_stop();
bool g64_uwp_r02_selftest(Vulkan::Device &device);
