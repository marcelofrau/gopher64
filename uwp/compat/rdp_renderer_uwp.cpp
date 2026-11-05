// UWP-PORT: use the same environment shim as the unmodified RDP device.
#include "rdp_environment.h"
#define getenv(name) g64_uwp_rdp_getenv(name)
#include "../../parallel-rdp/parallel-rdp-standalone/parallel-rdp/rdp_renderer.cpp"
