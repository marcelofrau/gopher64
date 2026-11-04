#include <windows.h>
#include <vulkan/vulkan.h>

#include <cstdio>

extern "C" void gopher64_uwp_log_native(const char* message);

extern "C" PFN_vkGetInstanceProcAddr g64_uwp_dozen_loader() {
    if (!SetEnvironmentVariableW(L"dzn_enable_8bit_loads_stores", L"true")) {
        char message[128];
        std::snprintf(message, sizeof(message), "Dozen: setting 8-bit storage failed: %lu", GetLastError());
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
    gopher64_uwp_log_native("Dozen: 8-bit storage requested; UWP RDP host import forced off");
    return gipa;
}
