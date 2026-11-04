#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include "../compat/rdp_environment.h"

using LogCallback = void (*)(const char*);

int g64_dozen_probe(LogCallback log, bool enable_8bit, void* core_window) {
    if (enable_8bit) {
        if (!SetEnvironmentVariableW(L"PARALLEL_RDP_ALLOW_EXTERNAL_HOST", L"1") ||
            std::strcmp(g64_uwp_rdp_getenv("PARALLEL_RDP_ALLOW_EXTERNAL_HOST"), "0") != 0 ||
            !SetEnvironmentVariableW(L"G64_RDP_ENV_PROBE", L"123") ||
            !g64_uwp_rdp_getenv("G64_RDP_ENV_PROBE") ||
            std::strcmp(g64_uwp_rdp_getenv("G64_RDP_ENV_PROBE"), "123") != 0) {
            log("UWP RDP environment self-check failed");
            return 1;
        }
        SetEnvironmentVariableW(L"G64_RDP_ENV_PROBE", nullptr);
        log("UWP RDP environment self-check passed: external host forced off");
        if (!SetEnvironmentVariableW(L"dzn_enable_8bit_loads_stores", L"true")) {
            char error[128];
            std::snprintf(error, sizeof(error), "SetEnvironmentVariableW(dzn_enable_8bit_loads_stores) failed: %lu", GetLastError());
            log(error);
            return 1;
        }
        log("Set dzn_enable_8bit_loads_stores=true for this process");
    }
    HMODULE module = LoadPackagedLibrary(L"vulkan_dzn.dll", 0);
    if (!module) {
        char message[128];
        std::snprintf(message, sizeof(message), "LoadPackagedLibrary(vulkan_dzn.dll) failed: %lu", GetLastError());
        log(message);
        return 1;
    }
    auto negotiate = reinterpret_cast<VkResult (VKAPI_PTR *)(uint32_t*)>(
        GetProcAddress(module, "vk_icdNegotiateLoaderICDInterfaceVersion"));
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(module, "vk_icdGetInstanceProcAddr"));
    if (!negotiate || !gipa) {
        log("Dozen ICD negotiation or instance-proc-address export is missing.");
        return 1;
    }
    uint32_t interface_version = 5;
    VkResult result = negotiate(&interface_version);
    char message[512];
    std::snprintf(message, sizeof(message), "ICD negotiation result=%d, interface=%u", result, interface_version);
    log(message);
    if (result != VK_SUCCESS || interface_version < 5) return 1;

    auto create = reinterpret_cast<PFN_vkCreateInstance>(gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create) {
        log("Dozen did not provide vkCreateInstance.");
        return 1;
    }
    VkApplicationInfo application{};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName = "Gopher64 UWP Dozen probe";
    application.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &application;
    const char* surface_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    if (core_window) {
        info.enabledExtensionCount = 2;
        info.ppEnabledExtensionNames = surface_extensions;
    }
    VkInstance instance = VK_NULL_HANDLE;
    result = create(&info, nullptr, &instance);
    std::snprintf(message, sizeof(message), "vkCreateInstance result=%d", result);
    log(message);
    if (result != VK_SUCCESS) return 1;

    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(gipa(instance, "vkDestroyInstance"));
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(gipa(instance, "vkEnumeratePhysicalDevices"));
    auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(gipa(instance, "vkGetPhysicalDeviceProperties"));
    auto features2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(gipa(instance, "vkGetPhysicalDeviceFeatures2"));
    auto device_extensions = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(gipa(instance, "vkEnumerateDeviceExtensionProperties"));
    if (!destroy || !enumerate || !properties || !features2 || !device_extensions) {
        log("Dozen did not provide required Vulkan 1.1 instance functions.");
        if (destroy) destroy(instance, nullptr);
        return 1;
    }
    uint32_t count = 0;
    result = enumerate(instance, &count, nullptr);
    std::snprintf(message, sizeof(message), "vkEnumeratePhysicalDevices result=%d, count=%u", result, count);
    log(message);
    if (result != VK_SUCCESS || count == 0) {
        destroy(instance, nullptr);
        return 1;
    }
    std::vector<VkPhysicalDevice> devices(count);
    result = enumerate(instance, &count, devices.data());
    if (result != VK_SUCCESS) {
        std::snprintf(message, sizeof(message), "Physical-device enumeration failed: %d", result);
        log(message);
        destroy(instance, nullptr);
        return 1;
    }
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    PFN_vkDestroySurfaceKHR destroy_surface = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR surface_capabilities = nullptr;
    if (core_window) {
        auto create_surface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(gipa(instance, "vkCreateWin32SurfaceKHR"));
        destroy_surface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(gipa(instance, "vkDestroySurfaceKHR"));
        surface_capabilities = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            gipa(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        if (!create_surface || !destroy_surface || !surface_capabilities) {
            log("Dozen did not provide the required Win32 surface functions");
            destroy(instance, nullptr);
            return 1;
        }
        // Mesa's _XBOX_UWP WSI uses the current CoreWindow instead of a desktop HWND.
        VkWin32SurfaceCreateInfoKHR surface_info{};
        surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surface_info.hwnd = reinterpret_cast<HWND>(core_window);
        result = create_surface(instance, &surface_info, nullptr, &surface);
        std::snprintf(message, sizeof(message), "vkCreateWin32SurfaceKHR result=%d", result);
        log(message);
        if (result != VK_SUCCESS) {
            destroy(instance, nullptr);
            return 1;
        }
    }
    bool compatible_gpu = false;
    bool compatible_surface = !core_window;
    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props{};
        properties(devices[i], &props);
        VkPhysicalDevice8BitStorageFeatures storage{};
        storage.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &storage;
        features2(devices[i], &features);
        uint32_t extension_count = 0;
        result = device_extensions(devices[i], nullptr, &extension_count, nullptr);
        if (result != VK_SUCCESS) {
            std::snprintf(message, sizeof(message), "Device extension count failed: %d", result);
            log(message);
            if (surface) destroy_surface(instance, surface, nullptr);
            destroy(instance, nullptr);
            return 1;
        }
        std::vector<VkExtensionProperties> extensions(extension_count);
        result = device_extensions(devices[i], nullptr, &extension_count, extensions.data());
        if (result != VK_SUCCESS) {
            std::snprintf(message, sizeof(message), "Device extension enumeration failed: %d", result);
            log(message);
            if (surface) destroy_surface(instance, surface, nullptr);
            destroy(instance, nullptr);
            return 1;
        }
        bool external_host = false;
        for (const auto& extension : extensions) {
            if (std::strcmp(extension.extensionName, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME) == 0) external_host = true;
        }
        if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU && storage.storageBuffer8BitAccess) compatible_gpu = true;
        std::snprintf(message, sizeof(message),
            "GPU[%u]=%s vendor=0x%04x device=0x%04x Vulkan=%u.%u.%u driver=%u 8bitStorage=%u uniform8bit=%u externalHost=%u",
            i, props.deviceName, props.vendorID, props.deviceID,
            VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion),
            props.driverVersion, storage.storageBuffer8BitAccess, storage.uniformAndStorageBuffer8BitAccess, external_host);
        log(message);
        if (surface) {
            VkSurfaceCapabilitiesKHR capabilities{};
            result = surface_capabilities(devices[i], surface, &capabilities);
            std::snprintf(message, sizeof(message), "GPU[%u] surface capabilities result=%d extent=%ux%u",
                          i, result, capabilities.currentExtent.width, capabilities.currentExtent.height);
            log(message);
            if (result == VK_SUCCESS && props.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU && storage.storageBuffer8BitAccess) {
                compatible_surface = true;
            }
        }
    }
    if (surface) destroy_surface(instance, surface, nullptr);
    destroy(instance, nullptr);
    if (enable_8bit && !compatible_gpu) {
        log("No non-CPU GPU exposes the required storageBuffer8BitAccess feature");
        return 2;
    }
    if (!compatible_surface) {
        log("No compatible hardware GPU supports the UWP surface");
        return 2;
    }
    return 0;
}
