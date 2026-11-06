#pragma once

#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <string>

inline bool g64_uwp_disable_subgroup = false;

inline bool g64_uwp_configure_rdp_subgroup(unsigned size, bool supports_control) {
    g64_uwp_disable_subgroup = (size == 32 || size == 64) && !supports_control;
    return g64_uwp_disable_subgroup;
}

inline const char* g64_uwp_rdp_getenv(const char* name) {
    // P0 NVIDIA Dozen rendered correctly with ubershader; Xbox remains unverified.
    if (std::strcmp(name, "PARALLEL_RDP_UBERSHADER") == 0) return "1";
    if (g64_uwp_disable_subgroup && std::strcmp(name, "PARALLEL_RDP_SUBGROUP") == 0) return "0";
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
