// UWP-PORT: translation-unit wrapper that compiles the UNMODIFIED submodule rdp_device.cpp with
// getenv redirected to a UWP-supported Win32 lookup (the UWP CRT has no getenv). The submodule is
// not edited; build.rs compiles this file instead of the submodule's on the uwp target. (D-07)
#include "rdp_environment.h"

// Match the validated P0 copy path and use UWP-supported environment access.
#define getenv(name) g64_uwp_rdp_getenv(name)
#include "../../parallel-rdp/parallel-rdp-standalone/parallel-rdp/rdp_device.cpp"
