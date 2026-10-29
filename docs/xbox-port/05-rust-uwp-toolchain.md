# 05 — Rust on UWP: Toolchain, Crates, cfg Gating

## 1. The target

| Property | Value |
|----------|-------|
| Target triple | `x86_64-uwp-windows-msvc` |
| Tier | 3 (no prebuilt `std` from rustup; `-Zbuild-std` required) |
| `cfg` values | `target_os = "windows"`, `target_family = "windows"`, `target_env = "msvc"`, **`target_vendor = "uwp"`**, `target_abi = "uwp"` (as seen by `cc-rs`, `cc-1.8.0/src/target/parser.rs:435`) |
| Rust std behaviour | Uses UWP-safe code paths where std has them (e.g. no `GetUserProfileDirectoryW` fallback for `home_dir`); still requests `kernel32 ntdll userenv ws2_32 dbghelp` + `msvcrt` (see §3) |

> `cfg(windows)` is **true** on UWP. Any code that must differ uses `cfg(target_vendor = "uwp")`. In `build.rs`, read `CARGO_CFG_TARGET_VENDOR`.

## 2. Spike evidence (2026-10-28, this PC)

Spike crate: staticlib exporting `spike_main()` that spawns a thread with an 8 MiB stack and writes to `temp_dir()`.

```text
$ cargo +nightly build --release -Zbuild-std=std,panic_abort --target x86_64-uwp-windows-msvc
   Finished `release` profile [optimized] target(s) in 50.51s       (rustc 1.100.0-nightly 2026-09-08)

$ RUSTC_BOOTSTRAP=1 cargo +1.99.1 build --release -Zbuild-std=std,panic_abort --target x86_64-uwp-windows-msvc
   Finished `release` profile [optimized] target(s) in 39.25s       (pinned upstream toolchain + rust-src)

$ cargo +nightly rustc … -- --print native-static-libs
note: native-static-libs: kernel32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib /defaultlib:msvcrt
```

Local toolchain inventory:

| Component | Present | Notes |
|-----------|---------|-------|
| rustup 1.29.2, toolchains `1.99.1-msvc` (active, pinned by `rust-toolchain.toml`), `nightly-msvc`, `stable-msvc`, `stable-gnu` (default) | ✔ | Default toolchain is `-gnu`; the repo's `rust-toolchain.toml` overrides it to `1.99.1` MSVC |
| `rust-src` for `1.99.1` and nightly | ✔ | Installed 2026-10-28 for the spike |
| VS 18 BuildTools, MSVC 14.51.36231, `lib/x64/store` (App CRT) | ✔ | Located with `vswhere -latest -products * -requires Microsoft.VisualStudio.Workload.UniversalBuildTools` (default install: `%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools`). Scripts must use `vswhere`, never a hard-coded path |
| Windows SDK 10.0.26100 with `WindowsApp.lib`, `OneCoreUAP.lib` | ✔ | |
| CMake 4.3.2 | ✔ | |
| LLVM / clang-cl matching rustc's LLVM | ? | Needed for D-14; install via the same method as upstream CI (`.github/workflows/build.yml:112-131`) |
| Vulkan SDK (`vulkaninfo`) | ✘ | Needed for P0 only |
| Python 3 + Meson deps for mesa-uwp | ? | Needed for P3 (mesa build) |

## 3. Final link

The staticlib's object files plus the libraries reported by `--print native-static-libs` are linked by MSBuild into `gopher64_uwp.exe`. The UWP CRT swap is applied there:

```text
link.exe /APPCONTAINER /SUBSYSTEM:WINDOWS /STACK:8388608
         gopher64.lib SDL3.lib SDL3_ttf.lib
         WindowsApp.lib                       ← umbrella import lib (api-ms-win-* sets)
         vccorlib.lib msvcrt.lib vcruntime.lib ucrt.lib  (store variants, lib/x64/store)
         /NODEFAULTLIB:kernel32.lib /NODEFAULTLIB:user32.lib /NODEFAULTLIB:ole32.lib …
         + remaining natives from --print native-static-libs, resolved via WindowsApp.lib where possible
```

Rules:

1. Every symbol Rust std asks from `kernel32.lib`/`ws2_32.lib` must resolve through `WindowsApp.lib` (most do: file, thread, sync, Winsock).
2. `ntdll.lib`, `userenv.lib`, `dbghelp.lib` are **not** part of the UWP partition. They are referenced by std for `NtCreateFile` (`remove_dir_all`), `GetUserProfileDirectoryW`, and backtraces. Link them, then **audit the final import table** (`dumpbin /imports gopher64_uwp.exe`) and make sure every imported DLL exists on the console. `panic = "abort"` + `RUST_BACKTRACE` unset avoids dbghelp at runtime; it is loaded lazily by std.
3. Fail the build if the import table contains `winmm.dll`, `user32.dll`, `gdi32.dll`, `shell32.dll`, `advapi32.dll` registry functions, `dwmapi.dll`, `uxtheme.dll` (desktop-only). The audit is a script step (`scripts/uwp/audit-imports.ps1`, P1).

## 4. Crate compatibility matrix

Dependency set from `Cargo.toml` (v1.1.42) as resolved in `Cargo.lock`, evaluated for `x86_64-uwp-windows-msvc` with `--no-default-features`.

Legend: ✔ expected to work · ⚠ needs change/verification · ✘ must be excluded on UWP

| Crate | Pulled by | Status | Issue | Action |
|-------|-----------|--------|-------|--------|
| `slint`, `slint-build`, `rfd`, `open` | `gui` feature | ✘ | winit/wgpu/COM dialogs/ShellExecute | Excluded by `--no-default-features` |
| `sdl3-sys 0.7.2` (`build-from-source-static`) | always | ⚠ | Upstream SDL3 has no WinRT backend; static `/ZW` | UWP table without build features + `links` override (D-05) |
| `sdl3-ttf-sys 0.7.1` (`build-static-vendored`) | always | ⚠ | Builds against SDL3 found via `DEP_SDL3_CMAKE_DIR` | Prebuilt SDL3_ttf DLL + `links` override |
| `spin_sleep 1.3.3` | `src/device/vi.rs:223` | ✘ | Imports `timeBeginPeriod`/`timeEndPeriod`/`timeGetDevCaps` (winmm) (`spin_sleep-1.3.3/src/windows.rs:4-17`) — not in the UWP partition | Replace on UWP with a local `precise_sleep()` (waitable timer `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` + spin tail) |
| `dirs 7` / `dirs-sys 0.5` | `src/ui.rs:227-229` | ✘ | `SHGetKnownFolderPath` (shell32) | Gate out; host-injected root (D-10) |
| `mimalloc` / `libmimalloc-sys` | `src/main.rs` only | ✘ | C build with `VirtualAlloc`-family OS layer | Gate out (D-15) |
| `matchbox_socket 0.14` (→ `webrtc 0.17`, `async-tls`, rustls 0.21, `rcgen`) | netplay | ✘ | Heavy, untested in AppContainer | Gate out (D-09) |
| `ggrs 0.13` | netplay | ✘ | Only useful with matchbox | Gate out |
| `tokio-tungstenite 0.30` | netplay lobby | ✘ | — | Gate out |
| `discord-rich-presence 1.1` | RA rich presence | ✘ | Named pipe to desktop Discord | Gate out |
| `tokio 1.53` (`rt-multi-thread`, `fs`, `process`) | always | ⚠ | `process` unneeded; IO driver uses `mio` → AFD via `NtDeviceIoControlFile` | UWP table without `process`; verify IO driver init on console (**Q-10**) |
| `reqwest 0.13` (`system-proxy`, platform verifier) | RA HTTP, update check | ⚠ | `system-proxy` → `windows-registry`; `rustls-platform-verifier` → CryptoAPI | UWP table without `system-proxy`; webpki-roots path (D-11) |
| `rustls 0.23` + `ring 0.17.14` | TLS | ⚠ | ring build: pregenerated NASM objects for Windows x86_64 (`ring-0.17.14/build.rs:237-258`), C via clang-cl; keys on `target_os`, so UWP is treated as Windows | Expected ✔; verify (**Q-05**) |
| `getrandom 0.2/0.3/0.4` | rand, ring, … | ⚠ | 0.3/0.4 use `ProcessPrng` (bcryptprimitives); 0.2 uses `BCryptGenRandom` | Expected ✔ (both in UWP partition / present on Xbox); verify in import audit |
| `zip 9` (→ `zstd-sys` C, `zlib-rs`, `libbz2-rs-sys`, `lzma-rust2`, `ppmd-rust`) | ROM loading | ✔ | `zstd-sys` compiles C with cc (clang-cl) | none |
| `sevenz-rust2 0.24` | ROM loading | ✔ | pure Rust | none |
| `serde*`, `postcard`, `serde-big-array`, `sha2`, `chrono`, `encoding_rs`, `regex`, `quick-xml`, `semver`, `rustc-hash`, `futures`, `rand`, `fatfs`, `obfstr`, `clap` | core | ✔ | pure Rust | none (`chrono` local-time APIs expected OK — confirm in the import audit) |
| `bindgen 0.73` (build-dep) | build host | ✔ | needs host libclang | host-only |
| `winresource` (build-dep) | `build.rs:140-143` | ⚠ | Compiles `.rc` with rc.exe; icon irrelevant in appx | Skip when `CARGO_CFG_TARGET_VENDOR == "uwp"` |
| `cc 1.8` (build-dep) | build | ✔ | Maps `x86_64-uwp-windows-msvc` → `x86_64-pc-windows-msvc` for clang (`cc-1.8.0/src/target/generated.rs:339`); does **not** add `WINAPI_FAMILY` | Add `-DWINAPI_FAMILY=WINAPI_FAMILY_APP` ourselves (D-14) |

## 5. Planned `Cargo.toml` changes

```toml
# Desktop (unchanged behaviour): everything that is not Android and not UWP
[target.'cfg(not(any(target_os = "android", target_vendor = "uwp")))'.dependencies]
slint = { version = "1.16", default-features = false, features = [ …as today… ], optional = true }
rfd = { version = "0.17", optional = true }
dirs = "7.0"
open = { version = "5.3", optional = true }
sdl3-sys = { version = "0.7", features = ["build-from-source-static"] }
sdl3-ttf-sys = { version = "0.7", features = ["build-static-vendored", "no-sdlttf-harfbuzz", "no-sdlttf-plutosvg"] }

# Everything that is not UWP (desktop + Android): netplay, discord, allocator, timers
[target.'cfg(not(target_vendor = "uwp"))'.dependencies]
mimalloc = "0.1"
spin_sleep = "1.3"
matchbox_socket = "0.14"
ggrs = { version = "0.13", features = ["sync-send"] }
tokio-tungstenite = { version = "0.30", features = ["rustls-tls-webpki-roots"] }
discord-rich-presence = "1.1"
reqwest = { version = "0.13", default-features = false, features = ["rustls-no-provider", "charset", "http2", "system-proxy", "json", "query"] }
tokio = { version = "1.46", features = ["rt-multi-thread", "fs", "process"] }

# UWP
[target.'cfg(target_vendor = "uwp")'.dependencies]
sdl3-sys = "0.7"                 # no build-from-source: links override in .cargo/config.toml
sdl3-ttf-sys = { version = "0.7", features = ["no-sdlttf-harfbuzz", "no-sdlttf-plutosvg"] }
webpki-roots = "1.0"
reqwest = { version = "0.13", default-features = false, features = ["rustls-no-provider", "charset", "http2", "json", "query"] }
tokio = { version = "1.46", features = ["rt-multi-thread", "fs"] }
```

The shared `[dependencies]` table loses `mimalloc`, `spin_sleep`, `matchbox_socket`, `ggrs`, `tokio-tungstenite`, `discord-rich-presence`, `reqwest`, `tokio` (moved into the two target tables). `main.rs` (desktop only) keeps `mimalloc`.

> Conflict surface for upstream merges: these tables. Keep the order and comments identical to upstream where possible; see [12](12-upstream-sync.md).

## 6. Planned `.cargo/config.toml` additions

```toml
[target.x86_64-uwp-windows-msvc]
# no `linker = "rust-lld"`: the final link is done by MSBuild (staticlib)
rustflags = ["-C", "target-cpu=x86-64-v3"]

[env]
# Upstream sets CC=clang/CXX=clang++/AR=llvm-ar globally; target-specific vars take precedence in cc-rs.
CC_x86_64_uwp_windows_msvc  = "clang-cl"
CXX_x86_64_uwp_windows_msvc = "clang-cl"
AR_x86_64_uwp_windows_msvc  = "llvm-lib"

# Generated by scripts/uwp/build.ps1 into a git-ignored include file (absolute paths):
# [target.x86_64-uwp-windows-msvc.SDL3]      rustc-link-search, rustc-link-lib, OUT_DIR
# [target.x86_64-uwp-windows-msvc.SDL3_ttf]  rustc-link-search, rustc-link-lib, OUT_DIR
```

Cargo's `[target.'cfg(…)']` sections in the existing config also match UWP: `cfg(any(target_os = "windows", target_os = "macos"))` sets `linker = "rust-lld"` and `cfg(target_os = "windows")` adds `/STACK`. For a staticlib the linker is never invoked, so they are harmless, but the build script passes `--config` overrides to make the intent explicit.

## 7. Planned `build.rs` changes

```rust
let vendor = std::env::var("CARGO_CFG_TARGET_VENDOR").unwrap_or_default();
let uwp = vendor == "uwp";

for b in [&mut volk_build, &mut rdp_build, &mut simd_build, &mut retroachievements_build] {
    b.flag(opt_flag);
    if uwp {
        b.define("GOPHER64_UWP", None);
        b.define("WINAPI_FAMILY", "WINAPI_FAMILY_APP");
        b.define("_XBOX_UWP", None);           // same macro mesa-uwp uses, for shared helpers
    } else {
        b.flag("-flto=thin");                  // bitcode only when rust-lld links (D-14)
    }
}

if os == "windows" {
    volk_build.flag("-DVK_USE_PLATFORM_WIN32_KHR");
    rdp_build.flag("-DVK_USE_PLATFORM_WIN32_KHR");
    if !uwp {
        winresource::WindowsResource::new().set_icon("data/icon/icon.ico").compile().unwrap();
    }
}
```

Also: `git describe` (`build.rs:284-301`) panics without git — fine for our CI (full clone), noted for source-tarball builds.

### 7.1 C/C++ sources under `WINAPI_FAMILY_APP` — the compat header

Defining `WINAPI_FAMILY=WINAPI_FAMILY_APP` hides desktop-only declarations in the Windows SDK headers. A scan of the 44 parallel-rdp/Granite/volk files compiled by `build.rs` found these desktop-only uses (all on code paths gopher64 never executes on UWP, but they must still compile and link):

| File:line | API | Partition | Executed on UWP? |
|-----------|-----|-----------|------------------|
| `volk/volk.c:30-31` | declares `__declspec(dllimport) LoadLibraryA/GetProcAddress` itself | `LoadLibraryA` desktop-only | No — `volkInitialize()` is never called; Granite uses `volkInitializeCustom` |
| `volk/volk.c:87` | `LoadLibraryA("vulkan-1.dll")` | desktop | No |
| `vulkan/context.cpp:249,255` | `LoadLibraryA("vulkan-1.dll")`, `GetProcAddress` | desktop | No — only when `init_loader(nullptr)`; we always pass Dozen's GIPA |
| `vulkan/breadcrumbs.cpp:709` | `MessageBoxA` (user32) | desktop — **user32 does not exist on Xbox** | Only on device-lost post-mortem |
| `vulkan/device.cpp:2905` | `QueryPerformanceFrequency` | app ✔ | yes |
| `vulkan/context.cpp:1381-1393`, `memory_allocator.cpp`, `semaphore.cpp` | `VK_KHR_external_{memory,semaphore}_win32` structs | Vulkan headers only | only if Dozen exposes them |

Solution without touching the submodule ([G-2](01-context-and-goals.md#2-goals)): `uwp/compat/g64_uwp_compat.h`, force-included by `build.rs` into the volk and parallel-rdp builds on UWP (`/FI` for clang-cl):

```c
// uwp/compat/g64_uwp_compat.h — force-included (/FI) into volk + parallel-rdp on UWP only.
#pragma once
#if defined(GOPHER64_UWP)
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
// Desktop-only in the SDK headers under WINAPI_FAMILY_APP. Re-declare so code compiles;
// resolved at link time via OneCoreUAP.lib/kernel32.lib. Never called on UWP (see 05 §7.1).
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
__declspec(dllimport) HMODULE WINAPI LoadLibraryA(LPCSTR lpLibFileName);
#endif
#ifdef __cplusplus
}
#endif
// user32 does not exist on Xbox: route Granite's post-mortem dialog to the log (stderr is
// redirected to the log file by the host). No OutputDebugString: it can raise without a
// debugger attached (scummvm-uwp docs/XBOX-TILE-LAUNCH-KILL.md).
#include <stdio.h>
#define MessageBoxA(hwnd, text, caption, type) (fprintf(stderr, "%s: %s\n", (caption), (text)), 0)
#endif
```

```rust
// build.rs, UWP only
volk_build.flag("/FIuwp/compat/g64_uwp_compat.h");
rdp_build.flag("/FIuwp/compat/g64_uwp_compat.h");
```

Verification: the import audit ([§3](#3-final-link)) must show **no** `user32.dll`. `LoadLibraryA` may appear as an import from `api-ms-win-core-libraryloader-*`/`kernel32.dll`; it exists at runtime on the console (scummvm-uwp calls `LoadLibrary` under `runFullTrust`) but is never called by us (**Q-11**: confirm import resolves at load time on the console).

## 8. Rust source changes (cfg map)

| File | Change | Lines (approx.) |
|------|--------|-----------------|
| `src/lib.rs` | `#[cfg(target_vendor = "uwp")] pub mod uwp;` exporting `gopher64_uwp_main(argc, argv)` and `gopher64_uwp_set_root(*const c_char)` (copy of `android::gopher64_sdl_main`, `src/ui/android.rs:195-211`) | +40 |
| `src/ui.rs` | `get_dirs()` UWP branch; `WEB_CLIENT` builder: `cfg(any(android, uwp))` for webpki-roots; `sdl_hints()`: add `SDL_HINT_JOYSTICK_WGI=1`, `SDL_HINT_WINRT_HANDLE_BACK_BUTTON=1` on UWP | ~+25 |
| `src/ui/audio.rs` | `#[cfg(target_vendor = "uwp")]` implementations calling `g64_audio_*` | ~+80 |
| `src/ui/video.rs` | Window flags: no `SDL_WINDOW_VULKAN`, always fullscreen on UWP | ~+6 |
| `src/device/vi.rs:223` | `#[cfg(target_vendor = "uwp")]` call to `crate::ui::uwp::time::precise_sleep(dur)`; the upstream `spin_sleep::sleep(dur)` line stays, gated `not(uwp)` (rule R3) | ~+3 (+ new 30-line module in `src/ui/uwp/time.rs`) |
| `src/netplay.rs`, `src/ui/netplay.rs` | UWP stub module with identical public API | ~+40 |
| `src/retroachievements.rs` | Discord functions → no-ops on UWP | ~+10 |
| `src/ui/usb.rs` | No-op on UWP (or keep; default off) | ~+5 |

Total expected Rust diff: ~250 lines, almost all additive and `cfg`-isolated.

## 9. Build command (what `scripts/uwp/build.ps1` will run)

```powershell
$env:RUSTC_BOOTSTRAP = "1"
cargo rustc --lib --release `
    --crate-type staticlib `
    --target x86_64-uwp-windows-msvc `
    --no-default-features `
    -Z build-std=std,panic_abort `
    --config "profile.release.panic='abort'" `
    --config .cargo/uwp-links.toml          # generated: SDL3 / SDL3_ttf overrides
# → target/x86_64-uwp-windows-msvc/release/gopher64.lib
```

`panic = "abort"` on UWP: unwinding across the C++ host/SDL boundary is UB anyway, and it removes the need for `dbghelp`-backed unwinding at runtime. A panic hook writes the message to the log file before aborting.
