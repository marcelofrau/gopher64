#include "../g64_d3d12_feature_log.h"
#include <cassert>
#include <string>
#include <iostream>

#ifdef NDEBUG
#error This test requires assertions.
#endif

static std::string last_log;
static bool luid_valid = false;
static LUID test_luid{};
extern "C" void gopher64_uwp_log_native(const char* message) { last_log = message; }

static void VKAPI_CALL fake_properties(VkPhysicalDevice, VkPhysicalDeviceProperties2* properties) {
    auto* id = static_cast<VkPhysicalDeviceIDProperties*>(properties->pNext);
    assert(id && id->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    id->deviceLUIDValid = luid_valid ? VK_TRUE : VK_FALSE;
    std::memcpy(id->deviceLUID, &test_luid, sizeof(test_luid));
}

int main() {
    auto gpu = reinterpret_cast<VkPhysicalDevice>(uintptr_t(1));
    g64_uwp_log_d3d12_storage(nullptr, gpu);
    assert(last_log == "D3D12 storage: Vulkan adapter identity unavailable");
    last_log.clear();
    g64_uwp_log_d3d12_storage(fake_properties, gpu);
    assert(last_log == "D3D12 storage: Vulkan adapter LUID unavailable");
    last_log.clear();
    luid_valid = true;
    test_luid.LowPart = 0xffffffff;
    test_luid.HighPart = 0x7fffffff;
    g64_uwp_log_d3d12_storage(fake_properties, gpu);
    assert(last_log.find("D3D12 storage: EnumAdapterByLuid failed hr=0x") == 0);
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    assert(SUCCEEDED(factory->EnumAdapters1(0, &adapter)));
    DXGI_ADAPTER_DESC1 desc{};
    assert(SUCCEEDED(adapter->GetDesc1(&desc)));
    test_luid = desc.AdapterLuid;
    last_log.clear();
    g64_uwp_log_d3d12_storage(fake_properties, gpu);
    std::cout << last_log << '\n';
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        assert(last_log.find("D3D12 storage: D3D12CreateDevice failed hr=0x") == 0);
        return 0;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options{};
    hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options, sizeof(options));
    if (FAILED(hr)) {
        assert(last_log.find("D3D12 storage: OPTIONS4 failed hr=0x") == 0);
        return 0;
    }
    assert(last_log == "D3D12 storage: OPTIONS4 hr=0x00000000 native16=" +
                       std::to_string(options.Native16BitShaderOpsSupported));
}
