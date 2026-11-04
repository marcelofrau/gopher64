#pragma once

#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <string>

inline const char* g64_uwp_rdp_getenv(const char* name) {
    if (std::strcmp(name, "PARALLEL_RDP_ALLOW_EXTERNAL_HOST") == 0)
        return "0";

    const DWORD required = GetEnvironmentVariableA(name, nullptr, 0);
    if (!required) return nullptr;
    // shortcut: the returned pointer lasts until the next call on this thread; review if upstream stores it.
    static thread_local std::string value;
    value.resize(required);
    const DWORD length = GetEnvironmentVariableA(name, value.data(), required);
    if (!length || length >= required) return nullptr;
    value.resize(length);
    return value.c_str();
}
