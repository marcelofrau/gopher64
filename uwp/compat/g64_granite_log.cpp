// UWP-PORT: route Granite/parallel-rdp LOGE/LOGW/LOGI into the host log on UWP. (D-07)
//
// On a retail Xbox there is no stderr sink and OutputDebugStringA is only visible under an
// attached debugger, so Granite's default logging (util/logging.cpp fallback) is invisible there.
// Granite exposes a thread-local LoggingInterface hook (util/logging.hpp): when one is installed,
// LOGx calls go through it instead of the stderr/OutputDebugString fallback. We install an
// interface that formats the message and forwards it to gopher64_uwp_log_native(), which the host
// captures into gopher64-host.log.
//
// Limitation: set_thread_logging_interface is THREAD-LOCAL. Installing it on the rdp_init thread
// covers initialization and the command-processing path (where P3 bring-up fails). Logs emitted
// from Granite's own internal worker threads are not covered and still hit the lost fallback.
// gopher64 runs parallel-rdp single-threaded (PARALLEL_RDP_SINGLE_THREADED_COMMAND), so the main
// RDP path is on this thread; present/WSI helper threads, if any, are the residual gap.

#include "../../parallel-rdp/parallel-rdp-standalone/util/logging.hpp"

#include <cstdarg>
#include <cstdio>

extern "C" void gopher64_uwp_log_native(const char *message);

namespace {

class HostLoggingInterface final : public Util::LoggingInterface {
public:
    bool log(const char *tag, const char *fmt, va_list va) override {
        char buffer[1024];
        int tag_len = std::snprintf(buffer, sizeof(buffer), "[rdp] %s", tag ? tag : "");
        if (tag_len < 0) {
            tag_len = 0;
        }
        if (static_cast<size_t>(tag_len) < sizeof(buffer)) {
            std::vsnprintf(buffer + tag_len, sizeof(buffer) - static_cast<size_t>(tag_len), fmt, va);
        }
        // Trim one trailing newline so the host does not log a blank line after each message.
        size_t len = 0;
        while (buffer[len] != '\0') {
            ++len;
        }
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        gopher64_uwp_log_native(buffer);
        return true; // handled; skip Granite's stderr fallback
    }
};

HostLoggingInterface g_host_logging_interface;

} // namespace

// Install the host logging interface on the calling thread. Call early in rdp_init.
extern "C" void g64_uwp_install_granite_logger() {
    Util::set_thread_logging_interface(&g_host_logging_interface);
}
