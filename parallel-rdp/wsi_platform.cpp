#include "wsi_platform.hpp"
#include <SDL3/SDL_vulkan.h>
// UWP-PORT: CoreWindow Vulkan surface for Dozen WSI (D-07)
#ifdef GOPHER64_UWP
#include <SDL3/SDL_properties.h>
#include <cstdio>
extern "C" void gopher64_uwp_log_native(const char *message);
#endif

VkSurfaceKHR SDL_WSIPlatform::create_surface(VkInstance instance,
                                             VkPhysicalDevice gpu) {
// UWP-PORT: CoreWindow Vulkan surface for Dozen WSI (D-07)
#ifdef GOPHER64_UWP
  auto core_window = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                             SDL_PROP_WINDOW_WINRT_WINDOW_POINTER,
                                             nullptr);
  if (!core_window || !vkCreateWin32SurfaceKHR) {
    gopher64_uwp_log_native("Dozen: CoreWindow or Vulkan surface entry point is unavailable");
    surface_valid = false;
    return VK_NULL_HANDLE;
  }
  VkWin32SurfaceCreateInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  // Mesa's UWP WSI uses the current CoreWindow instead of interpreting this as an HWND.
  info.hwnd = reinterpret_cast<HWND>(core_window);
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const VkResult result = vkCreateWin32SurfaceKHR(instance, &info, nullptr, &surface);
  char message[128];
  std::snprintf(message, sizeof(message), "Dozen: vkCreateWin32SurfaceKHR result=%d", result);
  gopher64_uwp_log_native(message);
  surface_valid = result == VK_SUCCESS;
  return surface_valid ? surface : VK_NULL_HANDLE;
#else
  VkSurfaceKHR surface = nullptr;
  bool result = SDL_Vulkan_CreateSurface(window, instance, NULL, &surface);
  if (result != true) {
    printf("Error creating surface\n");
    surface_valid = false;
  } else {
    surface_valid = true;
  }
  return surface;
#endif
}

void SDL_WSIPlatform::destroy_surface(VkInstance instance,
                                      VkSurfaceKHR surface) {
  surface_valid = false;
// UWP-PORT: CoreWindow Vulkan surface for Dozen WSI (D-07)
#ifdef GOPHER64_UWP
  if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
#else
  SDL_Vulkan_DestroySurface(instance, surface, NULL);
#endif
}

std::vector<const char *> SDL_WSIPlatform::get_instance_extensions() {
// UWP-PORT: CoreWindow Vulkan surface for Dozen WSI (D-07)
#ifdef GOPHER64_UWP
  return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
#else

  unsigned int extensionCount = 0;
  char const *const *extensions =
      SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  if (extensions == NULL) {
    printf("Error getting instance extensions\n");
  }

  std::vector<const char *> extensionNames;
  for (unsigned int i = 0; i < extensionCount; ++i) {
    extensionNames.push_back(extensions[i]);
  }
  return extensionNames;
#endif
}

uint32_t SDL_WSIPlatform::get_surface_width() {
  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);
// UWP-PORT: log the extent Granite sees; a 0-size or stale value here is a known Xbox
// first-frame failure mode (CoreWindow not yet activated / HDMI mode unknown). (D-07)
#ifdef GOPHER64_UWP
  char message[96];
  std::snprintf(message, sizeof(message), "Dozen: surface extent %dx%d", w, h);
  gopher64_uwp_log_native(message);
#endif
  return w;
}

uint32_t SDL_WSIPlatform::get_surface_height() {
  int w, h;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  return h;
}

bool SDL_WSIPlatform::alive(Vulkan::WSI &wsi) { return true; }

void SDL_WSIPlatform::poll_input() { SDL_PumpEvents(); }

void SDL_WSIPlatform::poll_input_async(Granite::InputTrackerHandler *handler) {}

void SDL_WSIPlatform::set_window(SDL_Window *_window) { window = _window; }

void SDL_WSIPlatform::do_resize() { resize = true; }
