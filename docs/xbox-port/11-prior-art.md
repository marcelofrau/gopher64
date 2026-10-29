# 11 — Prior Art

What others already solved, and what we reuse. Sources: the workspace ports (sibling repositories in `../`, i.e. one level above this repo), the EmulationRevival hub (<https://emulationrevival.github.io/>), and GitHub. Survey date: **2026-10-28**.

## 1. Headline: Dozen-on-Xbox is already shipping (October 2026)

An earlier draft of this plan assumed gopher64 would be the first Xbox port to render through Mesa Dozen. **That is false.** At least five public projects shipped installable Dev Mode packages that render Vulkan through `vulkan_dzn.dll` during October 2026. None of them is listed on the EmulationRevival hub yet.

| Project | What | First Dozen release | License | Repo |
|---------|------|---------------------|---------|------|
| **OnyxEMU** (ex-ONYX3DS) | Azahar (3DS) libretro core + host | v2.0.1 — 2026-10-04 | GPL-3.0 | <https://github.com/oledz-i/OnyxEMU> |
| **gow3_xbox** | shadPS4-based God of War III launcher | 2026-10-19 | GPL-2.0 | <https://github.com/Cosmo/gow3_xbox> |
| **WWHD-UWP** | Wind Waker HD recomp | v0.1.2 — 2026-10-21 | MPL-2.0 | <https://github.com/JYRS20/WWHD-UWP> |
| **rpcs3-xbox** | RPCS3 (PS3) | v0.1.34.71 — 2026-10-24 | GPL-2.0 | <https://github.com/emelin2004/rpcs3-xbox> |
| Bloodborne launcher | shadPS4-based | 2026-10-26 | — | <https://github.com/youdedend/Test-project> |

Not shipped / not rendering with Dozen: `oledz-i/Vita3K-UWP` (pre-alpha, no releases), `Edem180599/rpcs3-uwp` (no releases), `veroxsity/JavaUWP` (preloads dzn but renders GL), `Cosmo/KytyPS5` (native D3D12 using Mesa `spirv_to_dxil`). Earlier experiment: `worleydl/mesa-uwp` branch `unleash_the_vulkan` (2025-01).

Infrastructure: SternXD/XboxEmulationHub `mesa-uwp` release zips have contained `bin/vulkan_dzn.dll` + `share/vulkan/icd.d/dzn_icd.x86_64.json` since **25.1.6 (2025-07-19)**. RetroArch UWP (XboxEmulationHub) and RetroArch 3DS **bundle** `vulkan_dzn.dll` but do not use it (no `HAVE_VULKAN` in the UWP vcxproj).

**What is still new with gopher64:** no N64 emulator, and no parallel-rdp user, runs on Xbox through Dozen (no gopher64 / mupen64plus / simple64 / paraLLEl-N64 Xbox port found). parallel-rdp is compute-only and has a different stress profile from the graphics-pipeline-heavy projects above.

### 1.1 How they load Dozen (all identical — and identical to our plan)

`LoadPackagedLibrary(L"vulkan_dzn.dll")` → `vk_icdNegotiateLoaderICDInterfaceVersion(5)` → `vk_icdGetInstanceProcAddr`, no Khronos loader:

- OnyxEMU `app/ONYX3DS/Emu/VulkanHost.cpp` `DozenDriver::Load` (verified: negotiates v5 first, then fetches GIPA; also installs a Mesa log hook `onyx_set_log_hook` exported by its patched Dozen).
- rpcs3-xbox `uwp/vkshim/vkshim.cpp:31-35`.
- gow3_xbox `xbox/graphics/present_bridge.cpp:28-31`.

### 1.2 How they present

| Strategy | Who | Mechanism | Pros | Cons |
|----------|-----|-----------|------|------|
| **A. Mesa WSI on CoreWindow** | WWHD-UWP (own patch); our `mesa-uwp` already has it (`wsi_common_win32.cpp` `_XBOX_UWP`) | `vkCreate*Surface` → `CreateSwapChainForCoreWindow` inside Dozen | No app-side D3D code; Granite WSI unchanged | CoreWindow thread affinity; swapchain behaviour owned by Mesa |
| **B. External image → own D3D12 swapchain** | OnyxEMU (XAML `SwapChainPanel`, `CreateSwapChainForComposition`), gow3 / Bloodborne (CoreWindow flip swapchain) | Render into Vulkan images exported with `VK_KHR_external_memory_win32`, sync via shared timeline semaphore/fence, `OpenSharedHandle` in D3D12, present ourselves | Full control of present/pacing/HDR; no Mesa WSI dependency | More code (D3D12 presenter, sync); needs Granite external-swapchain path |
| **C. CPU readback** | rpcs3-xbox (into a D3D11 composition swapchain), OnyxEMU fallback | Copy frame to host, upload to D3D11 | Simplest, robust | Extra copy per frame |

We start with **A** (zero extra code with our mesa-uwp) and keep **B** as the planned fallback — see [D-17](03-decisions.md#d-17).

### 1.3 Xbox-specific Dozen problems they hit (and fixed)

From OnyxEMU's 34-patch Mesa stack (`patches/mesa/`, base commit `a4a47003a1`, not part of our mesa-uwp history) — verified by reading the patch headers:

| Patch | Problem on Xbox | Relevance to parallel-rdp |
|-------|-----------------|---------------------------|
| `0001` build Dozen for UWP | No `LoadLibrary` / desktop Win32; D3D12/DXGI entry points from import libs; `LoadPackagedLibrary` for `DXIL.dll`; `CreateFileMappingFromApp`; skip dbghelp/console | High — compare with mesa-uwp's own UWP commit `34732bbc207` |
| `0008`, `0016` DXIL validator / self-signing (`ONYX_DXIL_SELFSIGN=1`) | **Series S's validator refuses Dozen-generated DXIL**; they sign modules regardless | **High** — any refused compute shader = missing RDP stage |
| `0011` avoid calls the Xbox runtime removes the device for | Views outside typeless family, sampling integer textures, integer border colours, `DENY_SHADER_RESOURCE` on mapped buffers → **device removed** | Medium — parallel-rdp mostly uses SSBOs, but check storage views |
| `0014` **write-combined memory for CPU-written data** | "On Xbox Series CPU writes through WRITE_BACK mappings did not reach the GPU (every hardware frame came out black)" | **Critical** — parallel-rdp's incoherent RDRAM mode uploads host data through host-visible buffers |
| `0017` fewer stalls | Driver PSO compile takes **0.4–0.7 s** on Xbox; per-texture heaps → kernel calls; `GetDeviceRemovedReason` is a kernel call | **High** — parallel-rdp has many compute variants |
| `0025`–`0028` D3D12 pipeline library as PSO cache | Persist compiled PSOs across launches | **High** |
| `0009`, `0020` descriptor pool heap sizing | Heap limits/fragmentation | Medium |
| `0019` check fence values while waiting | Fence-trust issues | Medium (timeline-heavy Granite) |
| `0005`–`0007`, `0010`, `0012`, `0022`, `0024` logging hooks | Diagnosability on a console without debugger | High (adopt the log hook) |

OnyxEMU status notes: v2.0 "loses the GPU device on Series S"; later builds run NSMB2 at 60 fps on Series S; v3.1.1 "Mario Kart 7 runs a steady 60 fps"; known periodic ~500 ms stalls (VMA image destroy → D3D12 kernel call, PSO builds).

Other projects:

- **rpcs3-xbox**: Mesa **26.2.5** cross-built from Linux with zig (`uwp/toolchain/build-mesa-dzn.sh`); patches for unsized UBO arrays, typeless sampled depth, buffer pool, PSO library, copy fixes; ships Microsoft `dxil.dll` + `dxcompiler.dll` (Microsoft.Direct3D.DXC). Recurring "Out of host memory" when "the Xbox's graphics driver rejects one frame". Bakes dynamic state into PSOs because Dozen otherwise builds a PSO per new value at draw time (0.1–4 s hitches).
- **gow3_xbox / Bloodborne**: Mesa from `Cosmo/mesa_ai_slop_version` (branch `dozen-update`, no license file): extended dynamic state 1/2, maintenance4, inline uniform blocks, Vulkan memory model, 1.3 on SM6.6, **8/16-bit storage without native 16-bit ops**, **narrow subgroup lowering**, constant-alpha emulation. Bloodborne precompiles PSOs into Dozen's `driver.vkcache`. ~45 fps @4K (GoW3), 25–30 fps on Series S (Bloodborne).
- **WWHD-UWP**: exports `wwhd_dzn_create_core_window_surface(VkInstance, IUnknown*)`, CoreWindow swapchain with `FLIP_DISCARD`, `currentExtent = UINT32_MAX`; lowers DXIL validator target to **1.7**; stops advertising stencil BLIT_DST without `PSSpecifiedStencilRefSupported`; `DZN_DEBUG` via `LocalState/dzn-debug.txt`.

> Licensing note: OnyxEMU's patches live in a GPL-3.0 repository; rpcs3-xbox and gow3 are GPL-2.0; `Cosmo/mesa_ai_slop_version` has **no license** (do not copy code from it; read only for ideas, or ask the author). Our port is GPL-3.0, so taking OnyxEMU's patches into our mesa-uwp fork is compatible as long as the combined Mesa build we ship is distributed under GPL-3.0-compatible terms with sources available.

## 2. EmulationRevival hub inventory (2026-10-28)

The hub pages are static HTML (no JSON feed). Counts: emulators 21, frontends 3, game ports 76, apps 4, experimental 2, media 7, utilities 6. **No hub entry renders through Dozen.**

| Name | System | Source | Xbox graphics backend |
|------|--------|--------|-----------------------|
| Azahar | 3DS | danprice142/Azahar-UWP | OpenGL via Mesa GL-on-D3D12 (mesa-uwp 26.1.3 + 5 patches) |
| Cemu | Wii U | danprice142/Cemu-UWP | New native D3D12 renderer |
| Dolphin | GC/Wii | danprice142/dolphin-uwp | D3D12 / D3D11 |
| DOSBox Pure UWP | DOS | marcelofrau/dosbox-pure-uwp | D3D11 |
| Flycast | Dreamcast | flyinghead/flycast | D3D11 |
| GZDoom / Raze | Doom / Build | aerisarn/*-uwp | GL via Mesa |
| Ikemen GO | MUGEN | danprice142/Ikemen-GO-dev-uwp | GL via Mesa |
| PPSSPP | PSP | hrydgard/ppsspp | D3D11 |
| RetroArch | multi | XboxEmulationHub/RetroArch | D3D11/D3D12/GL (bundles unused `vulkan_dzn.dll`) |
| RetroArch 3DS | 3DS | release mirror | GL via Mesa (bundles unused dzn) |
| ScummVM | SCUMM | marcelofrau/scummvm-uwp | D3D11 + Mesa GL |
| Supermodel | Model 3 | danprice142/Supermodel-uwp-gl | Mesa GL |
| XBSX2 | PS2 | XboxEmulationHub/XBSX2 | D3D12 / D3D11 |
| xemu | Xbox | danprice142/xemu-uwp | New native D3D12 NV2A renderer |
| Xenia Canary | X360 | danprice142/xenia-canary-uwp | D3D12 |
| Ymir | Saturn | marcelofrau/Ymir | Native D3D12 VDP renderer |
| (76 game ports) | — | various | Mesa GL, ANGLE, native D3D12 (RT64/plume, Dawn), SDL2/D3D11 — none Dozen |

## 3. Workspace ports — reusable assets

| Source | Asset | Use in gopher64 |
|--------|-------|-----------------|
| **Ymir** (`uwp/main`, ~38 `uwp:` commits) | `docs/uwp/README.md` (D1–D24), `HANDOFF.md` (T-01…T-18), `TESTING.md` | Gotcha catalogue; read before P2 |
| Ymir | `scripts/uwp/{common,build-deps,build,package,deploy-xbox,open-vs}.ps1`, `xbox.env.example` | Copy → `scripts/uwp/` |
| Ymir | `apps/ymir-sdl3/uwp/{UWPPackaging.cmake,Package.appxmanifest.in,ShellAssets.cmake,Assets/}` | Copy → `uwp/` |
| Ymir | `apps/ymir-sdl3/src/platform/uwp/uwp_platform.cpp` (log redirect, `FindExternalProfilePath`, pickers, `UseFullCoreWindowBounds`), `stall_watchdog.cpp` | Copy/adapt → `uwp/src/uwp_platform.cpp` |
| Ymir | `apps/ymir-sdl3/src/app/xaudio2_backend.cpp` (259 lines) | Adapt → `uwp/src/xaudio2_backend.cpp` |
| Ymir | `src/platform/uwp/shell/` (ImGui gamepad shell, FromApp file browser) | P5 launcher |
| Ymir | `.github/workflows/uwp.yml` (windows-2025, vcpkg + deps cache, tag `uwp-vX.Y.Z.R`) | P6 CI |
| scummvm-uwp | `docs/XBOX-TILE-LAUNCH-KILL.md` (OutputDebugString VEH, early activation, App 1 GB vs Game cap) | Mandatory reading before P2 |
| dosbox-pure-uwp | QPC + high-res waitable timer pacer; XAudio2 4×12 ms ring; "never `.get()` WinRT async on STA"; `uwp-xray-depot` TCP log | Timing, audio, debug sidecar |
| x-files-uwp | `scripts/package.ps1` (self-signed cert), `tools/liberate-loopback.ps1`, `docs/PORTAL-APPDATA.md` | Packaging, debugging |
| Starfox64Recomp-uwp | Game as static lib + separate WindowsStore CMake project (`uwp/CMakeLists.txt`, `uwp/main.cpp`) | Validates our D-02 structure |
| SDL3-uwp | WinRT backend for SDL 3.4.16 | D-04/D-05 |
| mesa-uwp + meson-uwp | Dozen + CoreWindow WSI for UWP | D-07 |
| RetroArch (XboxEmulationHub) | Prebuilt mesa-uwp `vulkan_dzn.dll`, `dxil.dll`, `spirv_to_dxil.dll` (`pkg/msvc-uwp/RetroArch-msvcUWP/MESA/x64/`, commit `68768fcff0`) | Quick start for P3 before our own Mesa build |
| xbox-uwp-lab | `docs/{environment,install-guide,deploy,filesystem,devmode-limitations,port-recipe,stack-inventory,rust-uwp}.md`, `docs/targets/gopher64.md` | Background; **partly outdated** vs. Ymir (e.g. "Rust/C++ cannot be the entry point" was disproved by Ymir D4) |

## 4. Rust on UWP precedent

None found in the workspace or among the shipped Dozen projects (all C/C++). The only `build-std` precedent in the workspace is `ask/ask-dos/.cargo/config.toml` (DOS target, `core` only). gopher64 would be, as far as this survey found, the first Rust-hosted emulator on Xbox Dev Mode — hence the early P1 spike.
