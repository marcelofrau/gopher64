#pragma once

#include <vulkan/vulkan.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>

extern "C" void gopher64_uwp_log_native(const char* message);

inline void g64_uwp_log_d3d12_error(const char* stage, HRESULT hr) {
    char message[128];
    std::snprintf(message, sizeof(message), "D3D12 storage: %s failed hr=0x%08lx", stage,
                  static_cast<unsigned long>(hr));
    gopher64_uwp_log_native(message);
}

inline void g64_uwp_log_d3d12_storage(PFN_vkGetPhysicalDeviceProperties2 get_properties,
                                         VkPhysicalDevice gpu) {
    if (!get_properties || !gpu) {
        gopher64_uwp_log_native("D3D12 storage: Vulkan adapter identity unavailable");
        return;
    }
    VkPhysicalDeviceIDProperties id{};
    id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &id;
    get_properties(gpu, &properties);
    if (!id.deviceLUIDValid) {
        gopher64_uwp_log_native("D3D12 storage: Vulkan adapter LUID unavailable");
        return;
    }
    static_assert(sizeof(LUID) == VK_LUID_SIZE, "Vulkan/DXGI LUID size mismatch");
    LUID luid{};
    std::memcpy(&luid, id.deviceLUID, sizeof(luid));
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { g64_uwp_log_d3d12_error("CreateDXGIFactory1", hr); return; }
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter));
    if (FAILED(hr)) { g64_uwp_log_d3d12_error("EnumAdapterByLuid", hr); return; }
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) { g64_uwp_log_d3d12_error("D3D12CreateDevice", hr); return; }
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options{};
    hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options, sizeof(options));
    if (FAILED(hr)) { g64_uwp_log_d3d12_error("OPTIONS4", hr); return; }
    char message[128];
    std::snprintf(message, sizeof(message), "D3D12 storage: OPTIONS4 hr=0x%08lx native16=%d",
                  static_cast<unsigned long>(hr), options.Native16BitShaderOpsSupported);
    gopher64_uwp_log_native(message);
}
