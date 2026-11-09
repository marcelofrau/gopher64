// UWP-PORT: Xbox bring-up diagnostics, see g64_diag.cpp. (D-07)
#pragma once

namespace Vulkan {
class Device;
}

void g64_uwp_heartbeat_frame();
void g64_uwp_watchdog_start();
void g64_uwp_watchdog_stop();
bool g64_uwp_r02_selftest(Vulkan::Device &device);
