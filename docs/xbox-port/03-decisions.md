# 03 — Decision Records

Lightweight ADRs. Each record: **context → decision → alternatives → consequences**. Status values: `Proposed` (planned, not yet validated), `Accepted` (validated by a spike or implementation), `Superseded` (keep the record, link to the replacement).

| ID | Title | Status |
|----|-------|--------|
| [D-01](#d-01) | Scope: Xbox Series X\|S, x64, Dev Mode UWP | Proposed |
| [D-02](#d-02) | Integration: Rust staticlib + thin C++20 host exe | Proposed |
| [D-03](#d-03) | Toolchain: upstream-pinned rustc + `RUSTC_BOOTSTRAP=1` + `-Zbuild-std` | Proposed (spike ✔) |
| [D-04](#d-04) | Keep SDL3 (SDL3-uwp) for window, input, events, timers, TTF | Proposed |
| [D-05](#d-05) | Link a prebuilt SDL3-uwp DLL through Cargo `links` overrides | Proposed |
| [D-06](#d-06) | Run the emulator on the UI thread (initially) | Proposed |
| [D-07](#d-07) | Vulkan via Mesa Dozen, loaded directly as an ICD | Proposed (P0 gate) |
| [D-08](#d-08) | Audio via XAudio2 in the host, exposed to Rust via C ABI | Proposed |
| [D-09](#d-09) | Gate netplay, Discord, USB, update check with `cfg(target_vendor = "uwp")` | Proposed |
| [D-10](#d-10) | Storage root injected by the host; external drive first, LocalState fallback | Proposed |
| [D-11](#d-11) | TLS: rustls + ring + webpki-roots (the Android path) | Proposed |
| [D-12](#d-12) | Frontend: CLI-style boot first, then an ImGui gamepad shell in the host | Proposed |
| [D-13](#d-13) | Packaging with CMake's VS generator (WindowsStore), reusing Ymir | Proposed |
| [D-14](#d-14) | C/C++ in `build.rs`: clang-cl, no `-flto=thin` on UWP | Proposed |
| [D-15](#d-15) | Allocator: system heap on UWP (no mimalloc) | Proposed |
| [D-16](#d-16) | Branching: `uwp/main` on the fork, `main` mirrors upstream | Proposed |
| [D-17](#d-17) | Mesa base: SternXD mesa-uwp 26.2.2 + ported Xbox-hardening patch series | Proposed |
| [D-18](#d-18) | Presentation: Mesa CoreWindow WSI first, external-image D3D12 presenter as fallback | Proposed |

---

<a id="d-01"></a>
## D-01 — Scope: Xbox Series X|S, x64, Dev Mode UWP

**Context.** Xbox One (Jaguar + GCN) and Series (Zen 2 + RDNA2) both run Dev Mode. gopher64's interpreter needs strong single-thread performance and parallel-rdp needs a modern compute GPU. `.cargo/config.toml` already targets `x86-64-v3`, which Jaguar lacks (no AVX2).

**Decision.** Support Series X and Series S only, `x86_64` only, sideloaded via Dev Mode.

**Alternatives.** Xbox One support (would require dropping `x86-64-v3`, and Dozen performance on GCN would be poor). ARM64 Windows devices (out of scope).

**Consequences.** Keep `-C target-cpu=x86-64-v3` and `-march=x86-64-v3` (Ymir D16 confirms AVX2 on Series consoles).

---

<a id="d-02"></a>
## D-02 — Integration: Rust staticlib + thin C++20 host exe

**Context.** UWP apps need a CoreApplication entry point, an AppContainer link (`/APPCONTAINER`, `WindowsApp.lib`, App CRT), and an appx packaging step. gopher64 already supports being *called* instead of being `main`: Android exports `gopher64_sdl_main(argc, argv)` (`src/ui/android.rs:195-211`) which parses `Args` and calls `run()`.

**Decision.** Build gopher64 as a **staticlib** for `x86_64-uwp-windows-msvc` via `cargo rustc --lib --crate-type staticlib` (no `Cargo.toml` change). A ~50-line C++20 host (`uwp/src/main.cpp`) includes `<SDL3/SDL_main.h>` from SDL3-uwp, which supplies `WinMain → SDL_RunApp → CoreApplication`, and calls an exported `gopher64_uwp_main(argc, argv)`. The host is compiled without `/ZW` (SDL3-uwp waives the C++/CX requirement when `_MSVC_LANG >= 202002L`, as proven by Ymir decision D4).

**Alternatives.**

| Option | Why not |
|--------|---------|
| Pure Rust exe implementing `IFrameworkView` with `windows-rs` | Possible in theory, but no precedent anywhere in the workspace; it would duplicate what SDL3-uwp already does; packaging still needs MSBuild. |
| Rust `cdylib` loaded by the host via `LoadPackagedLibrary` | Two CRTs/allocators across a DLL boundary, more moving parts, no benefit. |
| Rewrite the frontend in C++ and call the Rust core | Throws away gopher64's working `run()` flow, input profiles, savestates UI glue. |

**Consequences.** The host owns: entry point, log redirection, storage root probe, XAudio2, later the launcher UI. Rust keeps everything else. The final link is done by MSBuild, so any native libraries Rust needs (`--print native-static-libs`) must be passed to the host's link line (see [05](05-rust-uwp-toolchain.md#final-link)).

---

<a id="d-03"></a>
## D-03 — Toolchain: upstream-pinned rustc + `RUSTC_BOOTSTRAP=1` + `-Zbuild-std`

**Context.** `x86_64-uwp-windows-msvc` is a Tier-3 target: rustup ships no prebuilt `std`, so `std` must be built from source with `-Zbuild-std`, a nightly-only flag. Upstream pins `channel = "1.99.1"` in `rust-toolchain.toml` and bumps it regularly.

**Decision.** Use the **same pinned toolchain as upstream** with `RUSTC_BOOTSTRAP=1` and the `rust-src` component, so the UWP build always uses exactly the compiler upstream tests with. Pinned nightly is the fallback.

**Evidence (2026-10-28).** A staticlib that spawns a thread with an 8 MiB stack and writes a file builds for `x86_64-uwp-windows-msvc`:

| Toolchain | Command | Result |
|-----------|---------|--------|
| `nightly-2026-09-08` (1.100.0) | `cargo +nightly build --release -Zbuild-std=std,panic_abort --target x86_64-uwp-windows-msvc` | ✔ 50 s |
| `1.99.1` + `rust-src` | `RUSTC_BOOTSTRAP=1 cargo +1.99.1 build …` (same flags) | ✔ 39 s |

`--print native-static-libs` → `kernel32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib /defaultlib:msvcrt`.

**Alternatives.** Pinned nightly (risk of nightly regressions; diverges from upstream's compiler). Stable `x86_64-pc-windows-msvc` std linked into an AppContainer exe (unsupported; pulls desktop-only imports).

**Consequences.** `RUSTC_BOOTSTRAP` is an escape hatch, not a supported configuration; it is scoped to the UWP build script only. When upstream bumps `rust-toolchain.toml`, CI must `rustup component add rust-src` for the new version.

---

<a id="d-04"></a>
## D-04 — Keep SDL3 (SDL3-uwp) for window, input, events, timers, TTF

**Context.** SDL is woven through gopher64: window creation (`src/ui/video.rs`), all input (`src/ui/input.rs`, `input_profile.rs`), the event watch with hotkeys, window events and lifecycle (`parallel-rdp/interface.cpp:160-322`), OSD timers (`SDL_AddTimer`), OSD text (SDL_ttf), `SDL_RunOnMainThread`. Upstream SDL3 removed WinRT; the SternXD fork **SDL3-uwp** (3.4.16) restores it and is proven by Ymir at 60 fps.

**Decision.** Keep SDL3 as-is and use SDL3-uwp. Replace only the SDL pieces known to be broken or missing on Xbox:

| SDL feature | Status on Xbox | Action |
|-------------|----------------|--------|
| Video (WinRT CoreWindow) | ✔ | Use; drop `SDL_WINDOW_VULKAN` flag |
| `SDL_Vulkan_*` | ✘ not implemented in `src/video/winrt/` | Bypass (D-07) |
| Joystick/Gamepad via WGI | ✔ with `SDL_HINT_JOYSTICK_WGI=1` | Set hint before `SDL_Init` |
| Audio (WASAPI WinRT) | ✘ deadlocks in `SDL_OpenAudioDeviceStream` | Replace (D-08) |
| Timers, events, `SDL_RunOnMainThread` | ✔ | Use |
| `SDL_SetCursor` / cursor | ✘ throws `0x80070714` | Avoid (gopher64 only hides the cursor — verify) |
| `SDL_GetPrefPath`, `SDL_GetCurrentDirectory` | may return NULL | Not used by gopher64; host computes paths |
| Dialogs | dummy in CMake build | Not used (gui feature off) |

**Alternatives.** Replace SDL with native CoreWindow + Windows.Gaming.Input + XAudio2 bindings in Rust (`windows-rs`). Large rewrite of `input.rs`, `input_profile.rs`, `interface.cpp`; diverges massively from upstream.

**Consequences.** Upstream merges stay trivial in input/video code. We inherit SDL3-uwp's quirks (documented in Ymir HANDOFF T-01..T-18).

---

<a id="d-05"></a>
## D-05 — Link a prebuilt SDL3-uwp DLL through Cargo `links` overrides

**Context.** `Cargo.toml` uses `sdl3-sys` with `build-from-source-static` and `sdl3-ttf-sys` with `build-static-vendored` on non-Android targets. Building SDL3-uwp statically would drag its internal C++/CX (`/ZW`) objects into our link (Ymir D3 avoided exactly this by building SDL3 as a DLL). `build.rs:77,80` reads `DEP_SDL3_OUT_DIR` / `DEP_SDL3_TTF_OUT_DIR` to locate headers.

**Decision.**

1. Build SDL3-uwp and SDL3_ttf as **DLLs** with CMake (`CMAKE_SYSTEM_NAME=WindowsStore`) into `out/uwp-deps` (Ymir `scripts/uwp/build-deps.ps1` recipe).
2. On the UWP target, depend on `sdl3-sys` / `sdl3-ttf-sys` **without** `build-from-source*` features (separate target-specific dependency table).
3. Override the build scripts via `.cargo/config.toml`:

```toml
[target.x86_64-uwp-windows-msvc.SDL3]
rustc-link-search = ["native=<repo>/out/uwp-deps/lib"]
rustc-link-lib = ["SDL3"]
OUT_DIR = "<repo>/out/uwp-deps"        # → DEP_SDL3_OUT_DIR, build.rs appends /include

[target.x86_64-uwp-windows-msvc.SDL3_ttf]
rustc-link-search = ["native=<repo>/out/uwp-deps/lib"]
rustc-link-lib = ["SDL3_ttf"]
OUT_DIR = "<repo>/out/uwp-deps"
```

(`<repo>` is the repository root. `scripts/uwp/build.ps1` resolves it at build time and writes this section into the git-ignored `.cargo/uwp-links.toml`. Cargo `links` overrides do not resolve relative paths, so the file is regenerated on every machine and never committed. Nothing machine-specific lands in git.)

**Alternatives.** `sdl3-sys` `build-from-source` pointed at the SDL3-uwp tree (would need CMake WindowsStore args passed through `cmake-rs`; fragile; static `/ZW` problem). `use-vcpkg` (no UWP SDL3 port).

**Consequences.** `SDL3.dll` + `SDL3_ttf.dll` ship in the package. Version skew: `sdl3-sys 0.7.2` targets SDL 3.4.18 headers, SDL3-uwp is 3.4.16 — we compile against SDL3-uwp's headers via `DEP_SDL3_OUT_DIR`, but the Rust bindings are generated for 3.4.18. Any symbol added between 3.4.16 and 3.4.18 and used by gopher64 would fail at link time (**R-08**; mitigation: rebase SDL3-uwp to 3.4.18 or pin `sdl3-sys` to a 3.4.16-matching release).

---

<a id="d-06"></a>
## D-06 — Run the emulator on the UI thread (initially)

**Context.** See [02 §3.1](02-architecture.md#31-threads). Mesa caches `CoreWindow::GetForCurrentThread()` on its first call; SDL3-uwp runs `SDL_main` on the UI thread; gopher64 pumps events every frame.

**Decision.** `gopher64_uwp_main` runs `run()` directly on the thread that receives `SDL_main`. The host exe links with `/STACK:8388608`.

**Alternatives.** Worker thread + patched Mesa (`MainView().CoreWindow()`); keep as fallback.

**Consequences.** Long blocking operations (ROM load from slow USB, shader compile on first boot) block CoreWindow event processing. Mitigate with an early "Loading…" frame and by keeping the CoreWindow activated before `run()` (SDL does this). Open question **Q-01**: is the main thread stack reserve honoured? If not → worker thread fallback.

---

<a id="d-07"></a>
## D-07 — Vulkan via Mesa Dozen, loaded directly as an ICD

**Context.** Xbox has no Vulkan driver. Every Xbox emulator in the workspace and on the EmulationRevival hub either uses native D3D11/D3D12 or OpenGL via Mesa's gallium d3d12. However, since October 2026 at least five public projects ship Vulkan-on-Dozen on Xbox (OnyxEMU, rpcs3-xbox, gow3_xbox, WWHD-UWP, a Bloodborne launcher — [11 §1](11-prior-art.md#1-headline-dozen-on-xbox-is-already-shipping-october-2026)), all loading `vulkan_dzn.dll` exactly as below. parallel-rdp is the core value of gopher64 and is Vulkan-only. Full analysis in [04](04-vulkan-dozen.md).

**Decision.** Ship `vulkan_dzn.dll` (built from our `mesa-uwp` fork with `meson-uwp`, `-D vulkan-drivers=microsoft-experimental`, `/D _XBOX_UWP`) and load it **without the Khronos loader**:

```cpp
HMODULE dzn = LoadPackagedLibrary(L"vulkan_dzn.dll", 0);
auto negotiate = (PFN_vkNegotiateLoaderICDInterfaceVersion)GetProcAddress(dzn, "vk_icdNegotiateLoaderICDInterfaceVersion");
uint32_t ver = 5; negotiate(&ver);
auto gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress(dzn, "vk_icdGetInstanceProcAddr");
Vulkan::Context::init_loader(gipa);
```

Surface: `VK_KHR_surface` + `VK_KHR_win32_surface`, `vkCreateWin32SurfaceKHR` with a dummy HWND; Mesa's `_XBOX_UWP` WSI ignores the HWND and calls `CreateSwapChainForCoreWindow`.

**Alternatives.**

| Option | Verdict |
|--------|---------|
| **Ashes** (Vulkan → D3D11) | ✘ SM 5.0 via SPIRV-Cross, no timeline semaphores, `shaderInt16` forced off, D3D11 only; cannot run parallel-rdp. |
| Khronos Vulkan loader + ICD JSON manifest | ✘ loader needs registry/env discovery that does not exist in AppContainer; extra DLL; no benefit over direct ICD loading. |
| Port parallel-rdp to native D3D12 / HLSL | ✘ months of work; loses upstream parallel-rdp updates; fallback of last resort. |
| RT64 (D3D12, as Starfox64Recomp-uwp) | ✘ HLE renderer, different accuracy model; would be a different emulator. |
| angrylion software RDP | ✘ far too slow for full speed on Zen 2 at 3.6 GHz while the interpreter also runs. |

**Consequences.** P0 must prove correctness on Dozen on a PC. Dozen is "experimental"/non-conformant; we own any Mesa patches in our fork (D-17). The PC cannot reproduce Xbox-only driver behaviour (CPU-write visibility, DXIL validator strictness, device removal rules), so P3 starts with targeted smoke tests for exactly those.

---

<a id="d-08"></a>
## D-08 — Audio via XAudio2 in the host, exposed to Rust via C ABI

**Context.** SDL3-uwp's WASAPI path deadlocks on Xbox (`SDL_wasapi_winrt.cpp:306/317`, Ymir D20). gopher64's audio surface is small (`src/ui/audio.rs`): `init`, `init_game_audio` (`SDL_OpenAudioDeviceStream`, S16 stereo at the AI frequency), `update_freq` (`SDL_SetAudioStreamFrequencyRatio` for drift control), `play_audio`, `pause/resume/close`, volume up/down.

**Decision.** Implement `g64_audio_{init,set_freq,set_ratio,queue,queued_bytes,pause,resume,set_volume,close}` in `uwp/src/xaudio2_backend.cpp`, adapted from Ymir's 259-line `apps/ymir-sdl3/src/app/xaudio2_backend.cpp`, and call it from a `#[cfg(target_vendor = "uwp")]` branch of `audio.rs`. `IXAudio2SourceVoice::SetFrequencyRatio` maps 1:1 onto `SDL_SetAudioStreamFrequencyRatio`. Buffer ring: 4 × ~12 ms (the "Xbox stability floor" measured in dosbox-pure-uwp).

**Alternatives.** Patch SDL3-uwp WASAPI (async activation without blocking wait) — attractive long-term fix upstream in SDL3-uwp, but riskier now. XAudio2 from Rust via `windows-rs` (adds a large crate; Ymir code reuse is cheaper).

**Consequences.** Rust staticlib has unresolved `g64_audio_*` symbols resolved at the host's final link — fine for static linking. The desktop build is unaffected.

---

<a id="d-09"></a>
## D-09 — Gate netplay, Discord, USB, update check with `cfg(target_vendor = "uwp")`

**Context.** Always-compiled deps that are useless or harmful on Xbox: `matchbox_socket` (→ `webrtc` → DTLS/ICE/mDNS, `ring`, `rcgen`, rustls 0.21), `ggrs`, `tokio-tungstenite`, `discord-rich-presence` (named pipes), `tokio` `process` feature, `src/ui/usb.rs` (TCP server on `localhost:48646`).

**Decision.** Use **target cfg**, not new Cargo features: move those deps into `[target.'cfg(not(target_vendor = "uwp"))'.dependencies]` and stub the call sites with `#[cfg(not(target_vendor = "uwp"))]`. The core references netplay through `Option<Netplay>` in `device.rs`, `vi.rs`, `savestates.rs`, `video.rs`; on UWP `Netplay` becomes an uninhabited/empty stub type with the same method signatures so call sites compile unchanged.

**Alternatives.** New default features `netplay`, `discord` (cleaner, and upstreamable, but changes upstream's feature surface and `cargo build` defaults; we can propose it upstream later).

**Consequences.** Cargo's feature resolver v2+ (edition 2024 defaults to resolver 3, a superset of v2) does not activate features from dependency tables of targets that are not being built, so target-specific dependency tables with different feature sets (e.g. `reqwest` with/without `system-proxy`) are reliable.

---

<a id="d-10"></a>
## D-10 — Storage root injected by the host; external drive first, LocalState fallback

**Context.** `get_dirs()` (`src/ui.rs:210-235`) uses `dirs::{config,data,cache}_dir()` or a portable mode next to the exe. On UWP the exe directory is read-only and `dirs` returns desktop paths. `StorageFolder` is unreliable on Xbox for arbitrary paths; plain Win32/`std::fs` works under `broadFileSystemAccess` + `runFullTrust` (Ymir, dosbox-pure, scummvm, x-files all rely on this).

**Decision.** The host probes `D:` … `Z:` for a writable `X:\gopher64` (real write test), else uses `ApplicationData::Current().LocalFolder()\gopher64`, and passes it via `gopher64_uwp_set_root(const char* utf8)`. On UWP `get_dirs()` returns `{root}\config`, `{root}\data`, `{root}\cache`. Everything else (`std::fs`, `tokio::fs`) works unchanged.

**Alternatives.** `GOPHER64_ROOT` env var (works, but env vars in UWP are process-local and invisible to users; a C ABI setter is explicit). Always LocalState (users cannot easily copy saves/ROMs; Device Portal file explorer is clumsy).

**Consequences.** Saves live on the external drive when present → survive app reinstall. A config toggle can force LocalState.

---

<a id="d-11"></a>
## D-11 — TLS: rustls + ring + webpki-roots (the Android path)

**Context.** `WEB_CLIENT` (`src/ui.rs:33-56`) uses `rustls-no-provider` + `system-proxy` and the platform verifier on desktop; on Android it uses `webpki_roots::TLS_SERVER_ROOTS` (`src/ui.rs:44-55`). `system-proxy` pulls `windows-registry`; the platform verifier uses CryptoAPI chain building (partition availability on Xbox unverified). `ring 0.17.14` is the only crypto provider (no `aws-lc-rs` in `Cargo.lock`).

**Decision.** On UWP: same code path as Android (`cfg(any(target_os = "android", target_vendor = "uwp"))`), reqwest without `system-proxy`, `webpki-roots` bundled. Only used for RetroAchievements (P6).

**Consequences.** Need `internetClient` capability. If `ring` fails to build for the UWP target (its build script special-cases `target_os`/`target_env`, not `target_vendor`; **Q-05**), RA can be gated out for the first release.

---

<a id="d-12"></a>
## D-12 — Frontend: CLI-style boot first, then an ImGui gamepad shell in the host

**Context.** The slint GUI cannot run (winit has no CoreWindow backend) and its game-launch model spawns processes. Ymir's UWP port has a proven gamepad-first ImGui "Shell" with a FromApp drive/folder browser (`apps/ymir-sdl3/src/platform/uwp/shell/`).

**Decision.**
- P2–P4: the host passes a ROM path in `argv` (hard-coded or from a text file `{root}\autoboot.txt`).
- P5: an ImGui shell in the host (C++), rendered with `SDL_Renderer` (SDL3-uwp's D3D11/D3D12 WinRT renderer) on the same SDL window **before** gopher64 creates its Vulkan swapchain, then torn down. It writes `config.json` through gopher64's own schema and calls `gopher64_uwp_main` with the selected ROM. Returning from the game returns to the shell.

**Alternatives.**

| Option | Notes |
|--------|-------|
| slint with a custom `Platform` + software renderer into an SDL texture | Reuses `appwindow.slint`, but `gui.rs` logic is desktop-bound (rfd, process spawn, update check); moderate work; keep as **future option**. |
| RmlUi (Starfox64Recomp) | Would need a renderer backend; no gopher64 synergy. |
| Draw the menu inside parallel-rdp's Vulkan output | Requires an ImGui Vulkan backend on Granite; complex; menu unavailable if Vulkan fails. |

**Consequences.** The shell keeps working even if Vulkan init fails (it can display the error). Transition shell → game requires destroying the SDL renderer before Dozen creates a swapchain on the same CoreWindow (**Q-06**).

---

<a id="d-13"></a>
## D-13 — Packaging with CMake's VS generator (WindowsStore), reusing Ymir

**Context.** Ymir already has `UWPPackaging.cmake`, `Package.appxmanifest.in`, `scripts/uwp/{common,build-deps,build,package,deploy-xbox}.ps1`, `xbox.env.example`, a CI workflow and a tag scheme `uwp-vX.Y.Z.R`.

**Decision.** Copy and adapt these into `uwp/` and `scripts/uwp/`. The VS generator builds the `.msix` directly; DLLs and assets are deployed via `VS_DEPLOYMENT_CONTENT`.

**Consequences.** Requires the VS 18 BuildTools UWP workload (present on this PC). See [08](08-packaging-deploy.md).

---

<a id="d-14"></a>
## D-14 — C/C++ in `build.rs`: clang-cl, no `-flto=thin` on UWP

**Context.** `build.rs:156-159` adds `-flto=thin` to every C/C++ object (volk, parallel-rdp, rcheevos). Upstream can do this because it links with `rust-lld`, which understands LLVM bitcode. Our final link is MSVC `link.exe` inside MSBuild, which **cannot read bitcode objects** bundled in the staticlib. `build.rs:121-134` adds `-march=x86-64-v3` unconditionally (`.flag`, not `flag_if_supported`). Upstream Windows CI compiles with `clang-cl` (`.github/workflows/build.yml:112-131`).

**Decision.** For `target_vendor == "uwp"`: compile with `clang-cl` (keeps `-march`, C++20, matches upstream Windows CI), **skip `-flto=thin`**, add `-DGOPHER64_UWP -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_XBOX_UWP`, skip `winresource`. Rust-side `lto = "thin"` stays (rustc resolves Rust LTO before writing the archive).

**Alternatives.** Use `lld-link` as the final linker in MSBuild (possible via `UseLldLink` / `LinkToolExe`, keeps cross-language LTO; revisit if profiling shows a gain). Use `cl.exe` (ignores `-march` with warning D9002 → loses AVX2 in C++ code paths).

**Consequences.** Minor perf loss in C++ (parallel-rdp CPU side is mostly command translation; the heavy work is on the GPU).

---

<a id="d-15"></a>
## D-15 — Allocator: system heap on UWP (no mimalloc)

**Context.** `mimalloc` is the `#[global_allocator]` only in `src/main.rs:3-4`, which the staticlib does not use, but `libmimalloc-sys` still compiles as a dependency and its OS layer uses `VirtualAlloc` APIs that are restricted in the UWP partition.

**Decision.** Move `mimalloc` to `[target.'cfg(not(target_vendor = "uwp"))'.dependencies]`.

**Consequences.** None functionally; the Windows heap is fine for gopher64's allocation pattern (large long-lived buffers).

---

<a id="d-16"></a>
## D-16 — Branching: `uwp/main` on the fork, `main` mirrors upstream

**Decision.** `main` stays a pure fast-forward mirror of `upstream/main`. All port work lives on `uwp/main` (feature branches `uwp/<topic>` merge into it). Upstream syncs: `git fetch upstream && git checkout main && git merge --ff-only upstream/main && git checkout uwp/main && git merge main`. Details in [12](12-upstream-sync.md).

---

<a id="d-17"></a>
## D-17 — Mesa base: SternXD mesa-uwp 26.2.2 + ported Xbox-hardening patch series

**Context.** Three Mesa lineages exist for Dozen on Xbox:

| Lineage | Has CoreWindow WSI | Xbox hardening | Notes |
|---------|-------------------|----------------|-------|
| SternXD `mesa-uwp` 26.2.2 (workspace) | ✔ (`_XBOX_UWP` in `wsi_common_win32.cpp`) | minimal (`SetViewInstanceMask`, debugger wait) | Builds with `meson-uwp`; prebuilt dzn already shipped in RetroArch |
| OnyxEMU 34-patch series on Mesa `a4a47003a1` | ✘ (Win32 WSI stubbed; presents via external images) | **extensive** (WC memory, DXIL self-sign, device-removal avoidance, PSO library, logging) | GPL-3.0 repo; proven at 60 fps on Series S |
| rpcs3-xbox: Mesa 26.2.5 + own patches, zig cross-build | ✘ (readback) | PSO library, UBO arrays, copy fixes | GPL-2.0 |

**Decision.** Fork from SternXD `mesa-uwp` (keeps the CoreWindow WSI and the meson-uwp build), add a branch `gopher64/dzn-xbox` and port the OnyxEMU patches relevant to compute workloads as a numbered series (M-01…M-11 in [04 §3.6](04-vulkan-dozen.md#36-mesa-patch-plan-for-our-mesa-uwp-fork)). For the very first console bring-up, RetroArch's prebuilt mesa-uwp `vulkan_dzn.dll` may be used to get a first frame, accepting that it lacks M-01/M-02.

**Alternatives.** Use OnyxEMU's tree directly (most hardened, but no CoreWindow WSI → forces D-18 option B immediately, and a different Mesa base than the rest of our stack). Use upstream Mesa + our own patches only (re-discovers every Xbox problem).

**Consequences.** We maintain a Mesa patch series. When the stack is stable, upstream generic fixes (M-03, M-04, M-08) to Mesa and offer M-01 to SternXD's mesa-uwp.

---

<a id="d-18"></a>
## D-18 — Presentation: Mesa CoreWindow WSI first, external-image D3D12 presenter as fallback

**Context.** Shipped projects present three ways ([11 §1.2](11-prior-art.md#12-how-they-present)). Our mesa-uwp already implements `CreateSwapChainForCoreWindow` behind the standard Vulkan WSI, so Granite's normal WSI path works with no extra code. The trade-offs are thread affinity of the CoreWindow lookup (D-06) and less control over pacing.

**Decision.** Option A (Mesa WSI) for P3. If A fails (no swapchain, pacing problems, lifecycle issues, HDR later), switch to option B: Granite external swapchain + `VK_KHR_external_memory_win32` images + a host-side D3D12 CoreWindow presenter (the OnyxEMU/gow3 design). Option C (CPU readback to an SDL texture) is a bring-up/debug tool only.

**Consequences.** Keep `interface.cpp` WSI changes behind a small abstraction (`g64_present_mode`) so swapping A↔B touches one file.
