#include "rdp_environment.h"

// Match the validated P0 copy path and use UWP-supported environment access.
#define getenv(name) g64_uwp_rdp_getenv(name)
#include "../../parallel-rdp/parallel-rdp-standalone/parallel-rdp/rdp_device.cpp"
