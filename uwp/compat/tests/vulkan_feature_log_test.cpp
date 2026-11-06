#include "../g64_vulkan_feature_log.h"
#include <cassert>
#include <string>

#ifdef NDEBUG
#error This test requires assertions to be enabled.
#endif

static std::string last_log;
static bool expect_eight = false;
static bool supported16 = true;
static bool supported8 = false;
extern "C" void gopher64_uwp_log_native(const char* message) { last_log = message; }

static void VKAPI_CALL fake_features(VkPhysicalDevice, VkPhysicalDeviceFeatures2* raw) {
    auto* storage16 = static_cast<VkPhysicalDevice16BitStorageFeatures*>(raw->pNext);
    assert(storage16 && storage16->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES);
    storage16->storageBuffer16BitAccess = supported16 ? VK_TRUE : VK_FALSE;
    raw->features.shaderInt16 = VK_TRUE;
    auto* storage8 = static_cast<VkPhysicalDevice8BitStorageFeatures*>(storage16->pNext);
    assert((storage8 != nullptr) == expect_eight);
    if (storage8) {
        assert(storage8->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES);
        storage8->storageBuffer8BitAccess = supported8 ? VK_TRUE : VK_FALSE;
    }
}

int main() {
    auto gpu = reinterpret_cast<VkPhysicalDevice>(1);
    expect_eight = true;
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_2, VK_STRUCTURE_TYPE_MAX_ENUM, VK_FALSE, VK_TRUE);
    assert(last_log == "Dozen storage: raw16=1 raw8=0 shaderInt16=1 granite16=0 granite8=1");
    expect_eight = false;
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_1, VK_STRUCTURE_TYPE_MAX_ENUM, VK_TRUE, VK_FALSE);
    assert(last_log == "Dozen storage: raw16=1 raw8=-1 shaderInt16=1 granite16=1 granite8=0");
    expect_eight = true;
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_1, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR, VK_TRUE, VK_FALSE);
    assert(last_log == "Dozen storage: raw16=1 raw8=0 shaderInt16=1 granite16=1 granite8=0");
    supported8 = true;
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_2, VK_STRUCTURE_TYPE_MAX_ENUM, VK_TRUE, VK_TRUE);
    assert(last_log == "Dozen storage: raw16=1 raw8=1 shaderInt16=1 granite16=1 granite8=1");
    supported8 = false;
    supported16 = false;
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_2, VK_STRUCTURE_TYPE_MAX_ENUM, VK_FALSE, VK_FALSE);
    assert(last_log == "Dozen storage: raw16=0 raw8=0 shaderInt16=1 granite16=0 granite8=0");
    last_log.clear();
    g64_uwp_log_storage_features(nullptr, gpu, VK_API_VERSION_1_2, VK_STRUCTURE_TYPE_MAX_ENUM, 0, 0);
    assert(last_log == "Dozen storage: raw feature query unavailable");
    last_log.clear();
    g64_uwp_log_storage_features(fake_features, nullptr, VK_API_VERSION_1_2, VK_STRUCTURE_TYPE_MAX_ENUM, 0, 0);
    assert(last_log == "Dozen storage: raw feature query unavailable");
    last_log.clear();
    g64_uwp_log_storage_features(fake_features, gpu, VK_API_VERSION_1_0, VK_STRUCTURE_TYPE_MAX_ENUM, 0, 0);
    assert(last_log == "Dozen storage: raw feature query unavailable");
}
