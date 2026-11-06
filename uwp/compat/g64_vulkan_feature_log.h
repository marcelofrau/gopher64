#pragma once

#include <vulkan/vulkan.h>
#include <cstdio>

extern "C" void gopher64_uwp_log_native(const char* message);

inline void g64_uwp_log_storage_features(PFN_vkGetPhysicalDeviceFeatures2 get_features,
                                          VkPhysicalDevice gpu, uint32_t api_version,
                                          VkStructureType storage8_type,
                                          VkBool32 granite16, VkBool32 granite8) {
    if (!get_features || !gpu || api_version < VK_API_VERSION_1_1) {
        gopher64_uwp_log_native("Dozen storage: raw feature query unavailable");
        return;
    }
    VkPhysicalDeviceFeatures2 raw{};
    raw.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDevice16BitStorageFeatures storage16{};
    storage16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    raw.pNext = &storage16;
    VkPhysicalDevice8BitStorageFeatures storage8{};
    storage8.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
    if (api_version >= VK_API_VERSION_1_2 ||
        storage8_type == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR) storage16.pNext = &storage8;
    get_features(gpu, &raw);

    char message[192];
    std::snprintf(message, sizeof(message),
                  "Dozen storage: raw16=%u raw8=%d shaderInt16=%u granite16=%u granite8=%u",
                  storage16.storageBuffer16BitAccess,
                  storage16.pNext ? int(storage8.storageBuffer8BitAccess) : -1,
                  raw.features.shaderInt16, granite16, granite8);
    gopher64_uwp_log_native(message);
}
