# 01 — Context & Goals

## 1. Why this port

Gopher64 is one of the most accurate N64 emulators available: a cycle-aware interpreter core paired with **parallel-rdp**, a Vulkan compute-shader implementation of the N64 RDP that is bit-accurate with real hardware. On Xbox Series consoles in Developer Mode the N64 options today are RetroArch cores using GLideN64 (through Mesa GL or ANGLE) or the CPU-only angrylion plugin, plus static recompilations such as Starfox64Recomp. None of them offers parallel-rdp accuracy at native speed. Bringing gopher64 to the console fills that gap.

## 2. Goals

| ID | Goal | Measured by |
|----|------|-------------|
| G-1 | Boot and play commercial N64 ROMs on Xbox Series X|S Dev Mode at full speed (60/50 Hz VI) at 1× native resolution | Super Mario 64, Mario Kart 64, Zelda OoT, GoldenEye hold full speed with audio in sync |
| G-2 | Keep the emulator core and parallel-rdp **unmodified** in behaviour (same accuracy as desktop) | No `cfg` changes inside `src/device/**` beyond strictly necessary; parallel-rdp submodule untouched |
| G-3 | Gamepad-first experience on a TV (no keyboard/mouse required) | Every action reachable with an Xbox controller |
| G-4 | Saves, save states, config persist across launches and app updates | Files under LocalState / external drive survive reinstall-with-update |
| G-5 | Stay faithful to and mergeable with upstream `gopher64/gopher64` | Upstream syncs are routine merges with conflicts confined to a handful of `cfg`-gated hooks; enforced by the binding [upstream fidelity rules](00-upstream-fidelity-rules.md) (≤ 20 upstream files / ≤ 400 lines touched); process in [12](12-upstream-sync.md) |
| G-6 | Reproducible builds from a clean machine via scripts and CI | `scripts/uwp/build.ps1` + GitHub Actions produce a signed `.msix` bundle |
| G-7 | Open source, GPL-3.0 like upstream | All code we add is GPL-3.0-compatible; bundled Mesa is MIT, SDL is zlib |

## 3. Non-goals (for the first release)

- **Netplay.** Pulls in `matchbox_socket` → `webrtc` (DTLS/ICE/TURN), heavy and untested in AppContainer. Gated out; may return later (see [D-09](03-decisions.md#d-09)).
- **Discord rich presence.** Uses named pipes to a desktop Discord client that does not exist on Xbox. Gated out.
- **The slint desktop GUI.** winit has no CoreWindow backend; the GUI also launches games by re-spawning its own executable, which UWP forbids. Replaced by a gamepad launcher ([07](07-frontend.md)).
- **Update checker, `open` links, file dialogs from `rfd`.** Desktop-only.
- **SC64 USB emulation over TCP** (`src/ui/usb.rs`), VRU (voice recognition unit) microphone, Transfer Pak camera.
- **ARM64 / Xbox One.** Xbox One (GCN, Jaguar) has neither the GPU features nor the CPU headroom; only x86_64 Series consoles are targeted.
- **Retail Store distribution.** Dev Mode sideloading only.

## 4. The platform: Xbox Series Dev Mode

### 4.1 Hardware

| | Series X | Series S |
|---|---|---|
| CPU | 8× Zen 2 @ 3.8 GHz (3.66 with SMT) | 8× Zen 2 @ 3.6 GHz (3.4 with SMT) |
| ISA level | x86-64-v3 (AVX2, BMI2, FMA) | same |
| GPU | RDNA2, 52 CU, D3D12 FL 12_2 | RDNA2, 20 CU |
| Memory | 16 GB GDDR6 (shared) | 10 GB |

Gopher64's interpreter is single-thread heavy (CPU + RSP on the emulation thread), so per-core performance matters more than core count. Zen 2 at 3.6–3.8 GHz is comparable to a mid-range desktop from 2019–2020, where gopher64 runs at full speed.

### 4.2 Software constraints that shape the design

| Constraint | Consequence for gopher64 | Source |
|------------|--------------------------|--------|
| Only UWP apps (AppContainer, CoreWindow); no Win32 `HWND` | No winit, no `VK_KHR_win32_surface` with a real window; SDL3 must be the WinRT build | xbox-uwp-lab `docs/devmode-limitations.md` |
| Only D3D11/D3D12 GPU drivers | Vulkan needs a translation layer → Mesa Dozen | [04](04-vulkan-dozen.md) |
| DLLs must live in the package; `LoadPackagedLibrary` | Ship `vulkan_dzn.dll`, `dxil.dll`, `SDL3.dll` inside the msix | `devmode-limitations.md` |
| No process spawning | The desktop GUI's "launch game as child process" model is impossible; run the game in-process (as Android does) | `src/ui/gui.rs:582-640` |
| Restricted filesystem; `StorageFolder` unreliable on arbitrary paths; `*FromApp` Win32 APIs work under `broadFileSystemAccess` + `runFullTrust` | Use LocalState or a probed external drive (`X:\gopher64`) via plain `std::fs` | xbox-uwp-lab `docs/filesystem.md`, Ymir `uwp_platform.cpp` |
| "App" vs "Game" mode — App ≈ 1 GB RAM and a GPU slice; Game (needs `expandedResources` + set as Game in Dev Home) ≈ 5 GB (field report, unmeasured) | Must ship `expandedResources` and run as **Game** | Ymir `AGENTS.md`, `devmode-limitations.md:7-35` |
| No `timeBeginPeriod`; `Sleep` granularity ≈ 15.6 ms | Frame limiter must use high-resolution waitable timers / spin | dosbox-pure-uwp pacing notes, Ymir HANDOFF |
| Only Xbox-compatible controllers; Windows.Gaming.Input (WGI); no XInput/DirectInput/HIDAPI | SDL hint `SDL_JOYSTICK_WGI=1` is mandatory, otherwise **zero** controllers enumerate | Ymir commit `1f098a91` |
| SDL3 WASAPI deadlocks on Xbox in `SDL_OpenAudioDeviceStream` | Audio via XAudio2 | Ymir `docs/uwp/README.md` D20 |
| Tile-launch watchdog kills apps that do not activate the CoreWindow promptly; unguarded `OutputDebugString` can throw without a debugger | Activate early, boot asynchronously, log to file | scummvm-uwp `docs/XBOX-TILE-LAUNCH-KILL.md` |
| CoreWindow logical size is 1920×1080 even on a 4K TV | Query real HDMI mode (SDL3-uwp commit `4df9707d5`, Mesa `uwp_get_width/height`) | `devmode-limitations.md`, `mesa-uwp/src/util/uwp_util.cpp` |
| Visual Studio remote debugging is unreliable and changes behaviour (memory limits, watchdog) | Log-file-first debugging, TCP log sidecar optional | [13](13-testing-debugging.md) |

## 5. Success criteria per milestone

| Milestone | Definition of done |
|-----------|--------------------|
| **M0 — Dozen proof** | Desktop gopher64 renders SM64 correctly with Dozen as the only Vulkan driver, with and without `VK_EXT_external_memory_host` |
| **M1 — Rust builds for UWP** | `gopher64.lib` builds for `x86_64-uwp-windows-msvc` with zero desktop-only symbols in the final import table |
| **M2 — Boots on console** | msix installs, tile launch survives the watchdog, log file shows emulator init |
| **M3 — First frame** | SM64 title screen visible on the TV through Dozen |
| **M4 — Playable** | Audio, controller, saves, save states, full speed on SM64 |
| **M5 — Usable** | Gamepad launcher: browse ROMs, change settings, no hard-coded paths |
| **M6 — Release** | CI-built signed bundle, README, known-issues list, compatibility table |
