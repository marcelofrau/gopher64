#include <windows.h>
#include <bcrypt.h>
#include <vulkan/vulkan.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

extern "C" void gopher64_uwp_log_native(const char* message);

namespace {
// Logs path, size and SHA-256 so every test run records exactly which driver/validator it used.
void log_file_identity(const wchar_t* path) {
    std::ifstream file(path, std::ios::binary);
    char message[512];
    if (!file) {
        std::snprintf(message, sizeof(message), "Dozen: %ls not found next to vulkan_dzn.dll", path);
        gopher64_uwp_log_native(message);
        return;
    }
    const std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    unsigned char hash[32] = {};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const bool hashed = BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
                        BCRYPT_SUCCESS(BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(data.data()),
                                                  static_cast<ULONG>(data.size()), hash, sizeof(hash)));
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    char hex[65] = {};
    for (int i = 0; i < 32; i++) std::snprintf(hex + i * 2, 3, "%02X", hash[i]);
    std::snprintf(message, sizeof(message), "Dozen: %ls size=%zu sha256=%s", path, data.size(),
                  hashed ? hex : "(hash failed)");
    gopher64_uwp_log_native(message);
}
} // namespace

extern "C" PFN_vkGetInstanceProcAddr g64_uwp_dozen_loader() {
    if (!SetEnvironmentVariableW(L"dzn_enable_8bit_loads_stores", L"true")) {
        char message[128];
        std::snprintf(message, sizeof(message), "Dozen: setting 8-bit storage failed: %lu", GetLastError());
        gopher64_uwp_log_native(message);
        return nullptr;
    }
    // shortcut: hardcoded DZN_DEBUG=no16bit so PC reproduces Xbox native16=0 during bring-up, remove after P3 validation
    if (!SetEnvironmentVariableW(L"DZN_DEBUG", L"no16bit")) {
        char message[128];
        std::snprintf(message, sizeof(message), "Dozen: setting DZN_DEBUG=no16bit failed: %lu", GetLastError());
        gopher64_uwp_log_native(message);
        return nullptr;
    }

    static HMODULE module = nullptr;
    if (!module) module = LoadPackagedLibrary(L"vulkan_dzn.dll", 0);
    if (!module) {
        char message[128];
        std::snprintf(message, sizeof(message), "Dozen: LoadPackagedLibrary failed: %lu", GetLastError());
        gopher64_uwp_log_native(message);
        return nullptr;
    }

    static bool identity_logged = false;
    if (!identity_logged) {
        identity_logged = true;
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        if (length && length < MAX_PATH) {
            log_file_identity(path);
            std::wstring dxil(path);
            dxil = dxil.substr(0, dxil.find_last_of(L"\\/") + 1) + L"dxil.dll";
            log_file_identity(dxil.c_str());
        }
    }

    using Negotiate = VkResult(VKAPI_PTR *)(uint32_t*);
    auto negotiate = reinterpret_cast<Negotiate>(GetProcAddress(module, "vk_icdNegotiateLoaderICDInterfaceVersion"));
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vk_icdGetInstanceProcAddr"));
    if (!negotiate || !gipa) {
        gopher64_uwp_log_native("Dozen: required ICD entry points are missing");
        return nullptr;
    }
    uint32_t version = 5;
    const VkResult result = negotiate(&version);
    char message[160];
    std::snprintf(message, sizeof(message), "Dozen: ICD negotiation result=%d, interface=%u", result, version);
    gopher64_uwp_log_native(message);
    if (result != VK_SUCCESS || version < 5) return nullptr;
    gopher64_uwp_log_native("Dozen: 8-bit storage and DZN_DEBUG=no16bit requested; UWP RDP host import forced off");
    return gipa;
}
