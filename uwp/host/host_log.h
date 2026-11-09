// UWP-PORT: single log sink for the UWP host (docs/xbox-port/06-platform-layer.md §6). (D-02)
//
// Every line goes to <dir>/gopher64-host.log and, when a debugger is attached, to the VS Output
// window. stdout/stderr of the whole process (Rust println!/eprintln!, C fprintf(stderr), Granite,
// SDL and Mesa default loggers) are redirected into the same sink, and fatal SEH exceptions are
// logged with module + offset before the process dies.
#pragma once

#include <string>

// Opens the log (previous run kept as gopher64-host.prev.log), redirects stdio and installs the
// crash logger. Returns false if the log file could not be opened; logging then goes only to the
// debugger.
bool host_log_open(const std::wstring& dir);

// Thread-safe. `tag` is a short source label such as "host" or "rust".
void host_log(const char* tag, const std::string& message);

// Applies KEY=VALUE lines from `path` to the process environment ('#' starts a comment).
// Missing file is not an error. Each applied variable is logged.
void host_log_apply_env_file(const std::wstring& path);

// Drains the stdio pipe and closes the log. Call once before the process exits.
void host_log_close();
