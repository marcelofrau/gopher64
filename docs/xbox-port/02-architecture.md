# 02 — Architecture

This document describes the target architecture of the UWP build. The guiding principle is **wrap, don't fork**: the emulator core, parallel-rdp and almost all of gopher64's Rust code compile unchanged. A thin C++ host owns the UWP entry point, and a few `cfg(target_vendor = "uwp")` branches replace desktop-only platform code.

## 1. Component view

```mermaid
flowchart TB
    subgraph HOST["UWP host — uwp/ (C++20, no /ZW)"]
        MAIN["main.cpp<br/>SDL_main → gopher64_uwp_main()"]
        PLAT["uwp_platform.cpp<br/>paths · log redirect · drive probe"]
        XA2["xaudio2_backend.cpp<br/>C ABI: g64_audio_*"]
        SHELL["launcher/ (P5)<br/>ImGui gamepad shell"]
    end

    subgraph RUST["gopher64.lib — Rust staticlib (x86_64-uwp-windows-msvc)"]
        direction TB
        LIBRS["lib.rs: run(Args)<br/>+ uwp entry (C ABI)"]
        CORE["src/device/**<br/>CPU · RSP · RDRAM · PI/SI/AI/VI<br/>(unchanged)"]
        UI["src/ui/**<br/>video · input · storage · config<br/>(cfg-gated where needed)"]
        AUDIO_RS["src/ui/audio.rs<br/>uwp → g64_audio_* FFI"]
        RA["retroachievements (C, rcheevos)"]
        subgraph CPP["C++ compiled by build.rs (cc)"]
            IFACE["parallel-rdp/interface.cpp<br/>OSD · hotkeys · present"]
            WSI["wsi_platform.cpp<br/>SDL_WSIPlatform / UWP_WSIPlatform"]
            PRDP["parallel-rdp + Granite + volk<br/>(submodule, unchanged)"]
        end
    end

    subgraph DEPS["Packaged third-party DLLs"]
        SDL["SDL3.dll (SDL3-uwp 3.4.x)<br/>WinRT video · WGI joystick · events · timers"]
        TTF["SDL3_ttf.dll (+FreeType)"]
        DZN["vulkan_dzn.dll (Mesa Dozen)<br/>Vulkan 1.2 → D3D12"]
        DXIL["dxil.dll<br/>(DXIL validator/signing)"]
    end

    subgraph OS["Xbox OS (UWP partition)"]
        CW["CoreWindow / CoreApplication"]
        D3D["D3D12 + DXGI"]
        XAUD["XAudio2"]
        WGI["Windows.Gaming.Input"]
        FS["LocalState · E:/F: drives"]
    end

    MAIN --> LIBRS
    MAIN --> PLAT
    SHELL -. P5 .-> LIBRS
    LIBRS --> CORE
    LIBRS --> UI
    CORE --> UI
    UI --> AUDIO_RS --> XA2 --> XAUD
    UI --> IFACE --> PRDP
    IFACE --> WSI
    UI --> SDL
    IFACE --> SDL
    IFACE --> TTF --> SDL
    PRDP -->|"volk ← vk_icdGetInstanceProcAddr"| DZN
    WSI -->|"vkCreateWin32SurfaceKHR (dummy HWND)"| DZN
    DZN --> DXIL
    DZN --> D3D
    DZN -->|"CreateSwapChainForCoreWindow"| CW
    SDL --> CW
    SDL --> WGI
    PLAT --> FS
    CORE -->|"std::fs (saves/states)"| FS
```

### 1.1 Module disposition

| Module | Desktop | UWP | Change type |
|--------|---------|-----|-------------|
| `src/device/**` (CPU, RSP, RDP glue, PIF, carts) | ✔ | ✔ | **None** |
| `src/savestates.rs`, `src/cheats.rs` | ✔ | ✔ | None |
| `src/netplay.rs` + `src/ui/netplay.rs` | ✔ | ✘ | `cfg` stub (`Netplay` type kept, `init` returns `None`) |
| `src/retroachievements.rs` | ✔ | ✔ (P6) | Discord part gated; HTTP via reqwest/rustls/webpki-roots |
| `src/ui/gui.rs`, `gui/*.slint`, `cheats.rs`, `vru.rs` (gui feature) | ✔ | ✘ | Excluded by `--no-default-features` |
| `src/ui/video.rs` | SDL window + Vulkan flag | SDL window **without** `SDL_WINDOW_VULKAN`, always fullscreen | `cfg` branch |
| `parallel-rdp/interface.cpp` | `SDL_Vulkan_GetVkGetInstanceProcAddr` | Dozen GIPA via `LoadPackagedLibrary` | `#ifdef GOPHER64_UWP` |
| `parallel-rdp/wsi_platform.cpp` | `SDL_Vulkan_CreateSurface` | `vkCreateWin32SurfaceKHR` with dummy HWND | `#ifdef` / subclass |
| `src/ui/audio.rs` | SDL audio stream | XAudio2 via C ABI in the host | `cfg` branch |
| `src/ui/input.rs`, `input_profile.rs` | SDL gamepad/joystick | Same (WGI under the hood) + default Xbox profile | Hint + defaults |
| `src/ui.rs` `get_dirs()` | `dirs` crate / portable | Root path injected by the host | `cfg` branch |
| `src/ui.rs` `WEB_CLIENT` | rustls + platform verifier + system proxy | rustls + webpki-roots (Android path) | `cfg(any(android, uwp))` |
| `src/main.rs` | binary, mimalloc | Not used (staticlib entry instead) | None |
| `build.rs` | winresource, `-march`, LTO, git describe | Skip winresource; define `GOPHER64_UWP`, `WINAPI_FAMILY=WINAPI_FAMILY_APP` | Small `if target_vendor == "uwp"` |

## 2. Build pipeline

```mermaid
flowchart LR
    subgraph S1["1 · build-deps.ps1 (cached)"]
        A1["SDL3-uwp<br/>CMake, WindowsStore,<br/>VS 18 generator, SHARED"] --> P1[("out/uwp-deps<br/>SDL3.dll/.lib + include")]
        A2["SDL3_ttf 3.2.x<br/>CMake, WindowsStore,<br/>vendored FreeType,<br/>SDL3_DIR=out/uwp-deps"] --> P1
        A3["mesa-uwp<br/>meson-uwp --uwp,<br/>-Dvulkan-drivers=microsoft-experimental,<br/>/D _XBOX_UWP"] --> P2[("out/uwp-deps/mesa<br/>vulkan_dzn.dll · dxil.dll")]
    end

    subgraph S2["2 · cargo (Rust staticlib)"]
        B1["cargo +nightly rustc --lib<br/>--crate-type staticlib<br/>--target x86_64-uwp-windows-msvc<br/>-Zbuild-std=std,panic_abort<br/>--no-default-features"]
        B2[".cargo links override:<br/>[target.x86_64-uwp-windows-msvc.SDL3]<br/>OUT_DIR = out/uwp-deps"]
        B2 --> B1
        P1 --> B2
        B1 --> L1[("gopher64.lib<br/>+ native-static-libs list")]
    end

    subgraph S3["3 · CMake WindowsStore host"]
        C1["uwp/CMakeLists.txt<br/>VS 18 2026 generator<br/>CMAKE_SYSTEM_NAME=WindowsStore"]
        L1 --> C1
        P1 --> C1
        P2 --> C1
        C1 --> X1[("gopher64_uwp.exe")]
        C1 --> M1[("AppxManifest + Assets<br/>VS_DEPLOYMENT_CONTENT DLLs")]
    end

    subgraph S4["4 · package.ps1 / deploy-xbox.ps1"]
        X1 --> D1["MSBuild AppxPackage<br/>→ .msix + VCLibs dep"]
        M1 --> D1
        D1 --> D2["signtool (self-signed .pfx)"]
        D2 --> D3["zip: msix + cer +<br/>Dependencies + Install.ps1"]
        D2 --> D4["Device Portal REST<br/>:11443 install + launch"]
    end
```

Notes:

- **Why `cargo rustc --crate-type staticlib`** instead of adding `staticlib` to `Cargo.toml`: keeps upstream's `crate-type = ["cdylib", "rlib"]` untouched (no extra artifact for desktop builds, no merge conflicts). `cargo rustc --crate-type` is stable.
- **Why the `links` override**: `sdl3-sys` declares `links = "SDL3"`, so Cargo lets `.cargo/config.toml` replace its build script output per target. Setting the `OUT_DIR` metadata key makes `DEP_SDL3_OUT_DIR` point at our prebuilt prefix, which is exactly what `build.rs:77` reads to find SDL headers. **Zero changes to `build.rs` for SDL include paths.** Same for `SDL3_ttf` (`links = "SDL3_ttf"`, `build.rs:80`).
- **Why the final link happens in CMake/MSBuild**, not cargo: the UWP CRT swap (`vccorlib.lib`, `msvcrt.lib`, `WindowsApp.lib`, `/APPCONTAINER`, `/NODEFAULTLIB:kernel32.lib …`) and appx packaging are solved problems in the VS generator (Ymir `UWPPackaging.cmake`). Cargo's link step is not involved for a staticlib.
- Details: [05 — Rust on UWP](05-rust-uwp-toolchain.md), [08 — Packaging](08-packaging-deploy.md).

## 3. Runtime view

### 3.1 Threads

```mermaid
flowchart TB
    subgraph UI["UI thread (ASTA) — owns CoreWindow"]
        T1["SDL_RunApp → SDL_main (host)"]
        T2["gopher64_uwp_main → run(Args)"]
        T3["device::run_game → cpu::run loop<br/>(CPU + RSP interpreter, VI timing)"]
        T4["rdp_update_screen: SDL_PumpEvents,<br/>wsi end_frame/begin_frame → DXGI Present"]
        T1 --> T2 --> T3 --> T4 --> T3
    end
    subgraph RDP["parallel-rdp workers"]
        W1["timeline_worker<br/>(fence/timeline waits)"]
        W2["pipeline_worker<br/>(deferred PSO compiles)"]
    end
    subgraph TOK["tokio runtime thread(s)"]
        K1["save writes (tokio::fs)"]
        K2["RA HTTP (reqwest)"]
        K3["FPS counter"]
    end
    subgraph AUD["XAudio2 voice thread"]
        A1["OnBufferEnd → refill from ring"]
    end
    subgraph SDLT["SDL internal"]
        S1["timer thread (OSD timers)"]
        S2["WGI gamepad callbacks"]
    end
    subgraph DZN["Dozen / D3D12 internal"]
        Z1["queue submission · shader compile (SPIR-V→NIR→DXIL)"]
    end
    T3 -->|"RDP commands"| RDP
    T3 -->|"play_audio()"| AUD
    T3 -->|"spawn"| TOK
    RDP --> DZN
```

**Decision ([D-06](03-decisions.md#d-06)): the emulator runs on the UI thread** for the first milestones. Reasons:

1. SDL3-uwp runs `SDL_main` on the UI (ASTA) thread — Ymir works this way at 60 fps.
2. Mesa's `uwp_get_window_reference()` caches `CoreWindow::GetForCurrentThread()` on first call (`mesa-uwp/src/util/uwp_util.cpp:25-31`); creating the swapchain on any other thread yields a null window and `CreateSwapChainForCoreWindow` fails.
3. gopher64 already pumps SDL events once per frame (`parallel-rdp/interface.cpp:735`), which drives `CoreDispatcher::ProcessEvents` inside SDL's WinRT event pump, so the window stays responsive.
4. gopher64 needs an 8 MiB stack (`.cargo/config.toml`: `/STACK:8388608`). The host exe sets `/STACK:8388608`; whether the UI thread honours the PE stack reserve under CoreApplication is **Q-01** (verify in P2; fallback below).

Fallback if Q-01 fails or UI-thread blocking causes lifecycle problems: patch mesa-uwp `uwp_get_corewindow()` to use `CoreApplication::MainView().CoreWindow()` (it already does that for the dispatcher in `uwp_get_width/height`), and move `run()` to a worker thread spawned with an explicit 8 MiB stack.

### 3.2 Boot sequence

```mermaid
sequenceDiagram
    autonumber
    participant OS as Xbox shell
    participant SDL as SDL3.dll (WinRT)
    participant Host as gopher64_uwp.exe
    participant Rust as gopher64.lib
    participant RDP as interface.cpp / Granite
    participant DZN as vulkan_dzn.dll

    OS->>Host: wWinMain (SDL_main.h)
    Host->>SDL: SDL_RunApp(SDL_main)
    SDL->>SDL: CoreApplication::Run, IFrameworkView
    SDL->>OS: CoreWindow.Activate()  (early — watchdog)
    SDL->>Host: SDL_main(argc, argv) on UI thread
    Host->>Host: redirect stdout/stderr → LocalState\gopher64.log
    Host->>Host: probe writable root (X:\gopher64 or LocalState)
    Host->>SDL: SDL_SetHint(JOYSTICK_WGI=1, WINRT_HANDLE_BACK_BUTTON=1)
    Host->>Rust: gopher64_uwp_set_root(path)
    Host->>Rust: gopher64_uwp_main(argc, argv)  [ROM path from launcher]
    Rust->>Rust: create_runtime() (tokio thread), get_dirs(), sdl_hints()
    Rust->>Rust: load ROM, config, saves
    Rust->>SDL: SDL_CreateWindow(FULLSCREEN, no VULKAN flag)
    Rust->>RDP: rdp_init(window, gfx_info, font)
    RDP->>DZN: LoadPackagedLibrary("vulkan_dzn.dll")
    RDP->>DZN: vk_icdNegotiateLoaderICDInterfaceVersion(5)
    RDP->>RDP: Context::init_loader(vk_icdGetInstanceProcAddr) → volk
    RDP->>DZN: vkCreateInstance(VK_KHR_surface, VK_KHR_win32_surface)
    RDP->>DZN: vkCreateWin32SurfaceKHR(hwnd = dummy)
    RDP->>DZN: vkCreateDevice / vkCreateSwapchainKHR
    DZN->>OS: CreateSwapChainForCoreWindow(GetForCurrentThread())
    RDP->>RDP: CommandProcessor (RDRAM import or incoherent fallback)
    Rust->>Host: g64_audio_init(freq) → XAudio2 source voice
    Rust->>Rust: cpu::run() — emulation loop
```

### 3.3 Frame loop

```mermaid
sequenceDiagram
    participant CPU as cpu::run (UI thread)
    participant RDP as parallel-rdp
    participant VI as vi.rs / interface.cpp
    participant DZN as Dozen
    participant XA as XAudio2

    loop every VI interrupt (~16.7 ms NTSC)
        CPU->>RDP: rdp_process_commands (DP_START/END)
        RDP->>DZN: compute dispatches (tile binning, raster, combiner)
        CPU->>XA: play_audio(AI DMA samples) → ring buffer
        CPU->>VI: vi interrupt → rdp_update_screen()
        VI->>VI: SDL_PumpEvents (CoreDispatcher.ProcessEvents)
        VI->>RDP: scanout(VI regs) → blit pipeline + OSD text
        VI->>DZN: wsi end_frame → vkQueuePresentKHR → IDXGISwapChain::Present
        VI->>CPU: speed limiter (spin_sleep / waitable timer)
        CPU->>CPU: input::get(): SDL gamepad state (WGI)
    end
```

## 4. Repository layout (planned additions)

```text
gopher64/
├─ docs/xbox-port/            ← this documentation
├─ uwp/                       ← NEW: UWP host project (never built by upstream CI)
│  ├─ CMakeLists.txt          WindowsStore project; links gopher64.lib + SDL3; packaging
│  ├─ UWPPackaging.cmake      adapted from Ymir
│  ├─ Package.appxmanifest.in
│  ├─ Assets/                 tiles, splash, store logo (full set — APPX1619 otherwise)
│  └─ src/
│     ├─ main.cpp             SDL_main → gopher64_uwp_main
│     ├─ uwp_platform.{hpp,cpp}  log redirect, root probe, UI helpers
│     ├─ xaudio2_backend.{hpp,cpp}  C ABI used by src/ui/audio.rs
│     └─ launcher/            (P5) ImGui shell, file browser
├─ scripts/uwp/               ← NEW: build-deps, build, package, deploy-xbox (from Ymir)
│  └─ xbox.env.example
├─ .cargo/config.toml         ← + [target.x86_64-uwp-windows-msvc] section & links overrides
├─ .github/workflows/uwp.yml  ← NEW (fork only)
├─ build.rs                   ← + small `target_vendor == "uwp"` block
├─ Cargo.toml                 ← + target-specific dependency tables
├─ parallel-rdp/interface.cpp ← + #ifdef GOPHER64_UWP loader branch
├─ parallel-rdp/wsi_platform.{hpp,cpp} ← + UWP surface creation
└─ src/
   ├─ lib.rs                  ← + uwp C-ABI entry module
   ├─ ui.rs                   ← get_dirs / WEB_CLIENT cfg branches
   ├─ ui/audio.rs             ← uwp branch → FFI
   ├─ ui/video.rs             ← uwp window flags
   └─ netplay.rs              ← cfg stub on uwp
```

## 5. On-device layout

### 5.1 Package (read-only install location on the console, `%ProgramFiles%\WindowsApps\<pkg>\` equivalent)

```text
gopher64_uwp.exe
gopher64_uwp.pri / resources.pri
AppxManifest.xml
Assets\*.png
SDL3.dll
SDL3_ttf.dll
vulkan_dzn.dll
dxil.dll
data\…                       (only if any runtime data is needed; fonts are embedded via include_bytes!)
```

### 5.2 Writable data (root chosen at startup)

```text
<root>\                      = X:\gopher64  (first writable external drive)  or  LocalState\gopher64
├─ config\
│  ├─ config.json            (src/ui/config.rs)
│  ├─ cheats.json
│  └─ retroachievements.json
├─ data\
│  ├─ saves\<name>-<hash>.{eep,sra,fla,mpk}
│  └─ states\<name>-<hash>.state<N>
├─ cache\
├─ roms\                     (suggested location; launcher browses any drive)
└─ logs\gopher64.log         (stdout/stderr redirected at fd level)
```

The three directories map 1:1 onto gopher64's existing `Dirs { config_dir, data_dir, cache_dir }` (`src/ui.rs:210-235`), so nothing downstream changes.
