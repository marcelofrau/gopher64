# 13 — Testing & Debugging

## 1. Test matrix

| Environment | Purpose | Phases |
|-------------|---------|--------|
| Windows PC, native Vulkan driver | Baseline (reference screenshots, FPS) | P0 |
| Windows PC, Dozen via Khronos loader | parallel-rdp logic on Dozen | P0, regression after submodule bumps |
| Windows PC, UWP sideload (same msix) | Packaging, AppContainer FS, imports, lifecycle — fast iteration | P2+ |
| **Xbox Series X**, Dev Mode, App type = Game | Primary target | P2+ |
| **Xbox Series S**, Dev Mode, App type = Game | Stricter driver/validator, less memory and GPU | P3+ (every phase) |

### 1.1 Reference games

| Game | Why |
|------|-----|
| Super Mario 64 | Baseline, common microcode, sanity |
| Mario Kart 64 | Framebuffer effects, 4-player split screen (multiple viewports) |
| The Legend of Zelda: Ocarina of Time | CPU framebuffer reads (incoherent-RDRAM path), heavy combiner use |
| GoldenEye 007 | Performance stress, Rare microcode |
| (P4+) Banjo-Kazooie, Perfect Dark, Conker, Star Fox 64, F-Zero X | Expansion Pak, high-res modes, Rumble Pak, 60 fps |

### 1.2 Per-phase checks

| Check | How | Pass |
|-------|-----|------|
| Visual parity | Screenshot at fixed frame (savestate + frame advance `/`) vs PC native baseline | Identical or explainable differences |
| Speed | gopher64 FPS counter (F1 / OSD), VI/s | ≥ 100 % (60/50 VI/s) |
| Frame time split | `PARALLEL_RDP_BENCH=1` + host QPC timestamps around `cpu::run` frame, `rdp_update_screen`, present | Logged per 600 frames |
| Audio | 10 min play, underrun counter from XAudio2 backend | 0 audible crackles; underruns < 1/min |
| Saves | Save in-game, quit, relaunch, reinstall (external root) | Save present |
| Save states | Save → load → compare next frame | Identical |
| Lifecycle | Guide button, go to dashboard and back, launch another app and back | Resumes, no black screen |
| Memory | Log `GlobalMemoryStatusEx`-equivalent / `MemoryManager::AppMemoryUsageLimit` at boot and every 60 s | Stable, under limit |
| Stability | 60 min soak on Series S | No device removal, no crash |

## 2. Logging

- Everything goes to `{root}\logs\gopher64.log` (fd-level redirect, [06 §6](06-platform-layer.md#6-logging--crash-capture)).
- Prefixes: `[host]`, `[rust]`, `[rdp]` (parallel-rdp `LOGI/LOGW/LOGE`), `[dzn]` (Mesa log hook, M-06), `[sdl]` (`SDL_SetLogOutputFunction`).
- Boot banner: app version, `GIT_DESCRIBE`, SDL3-uwp / Mesa commit hashes, console model if detectable, root path, memory limit.
- Retrieve via Device Portal file explorer (LocalState) or from the external drive directly.

## 3. Debugging tools

| Tool | Use | Caveat |
|------|-----|--------|
| Log file | Primary | — |
| Device Portal: Performance, Processes, Crash dumps (if enabled) | CPU/GPU/memory usage, kill hung app | — |
| TCP log sidecar (`uwp-xray-depot`, ports 9000-9009) | Live log streaming (debug builds) | Needs loopback/LAN capability |
| `DZN_DEBUG` via `{root}\config\dzn-debug.txt` (WWHD idea) | Mesa debug flags without rebuilding | Set with `_putenv_s` before `LoadPackagedLibrary` |
| `RDP_DEBUG`, `PARALLEL_RDP_*` knobs via `{root}\config\env.txt` | Renderer triage on console | same |
| Visual Studio remote debugger | Last resort | Changes memory limits & watchdog behaviour; unreliable on Xbox (Ymir, scummvm notes) |
| PC UWP sideload + WinDbg | Reproduce non-GPU crashes locally | Not representative for driver issues |

## 4. P0 results

<a id="p0-results"></a>

Fill in during P0.

2026-10-30 preliminary setup: the prebuilt [mmozeiko/build-mesa 26.2.6](https://github.com/mmozeiko/build-mesa/releases/tag/26.2.6) `mesa-dzn-x64` archive passed its published SHA-256 check. With `VK_DRIVER_FILES` set to its manifest, `vulkaninfo --summary` reported Dozen 26.2.6 for NVIDIA RTX 3500 Ada, Intel Arc and Microsoft Basic Render Driver. The NVIDIA device exposes `VK_EXT_external_memory_host`; actual coherent-mode use remains to be verified in the renderer. No game result is implied by this setup check.

| Date | PC GPU / driver | Dozen build | Mode | Game | Result | Speed | Notes |
|------|-----------------|-------------|------|------|--------|-------|-------|
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | native Vulkan (baseline) | SM64 | User reports image and audio working | not measured | portable data directory |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | native Vulkan, `ALLOW_EXTERNAL_HOST=0` | SM64 | User reports correct rendering, no visible faults | not measured | Control for the incoherent RDRAM path; normal exit |
| | | | Dozen, coherent | SM64 | | | |
| | | | Dozen, `ALLOW_EXTERNAL_HOST=0` | SM64 | | | |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SUBGROUP=0` | SM64 | Severe visual corruption persists | not measured | User screenshots |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | Mesa Dozen 26.2.6 | Dozen, default 8-bit off | SM64 | Fails to initialise: 8-bit storage required | n/a | `storageBuffer8BitAccess` is mandatory |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | Mesa Dozen 26.2.6 | Dozen, 8-bit on, external host disabled | SM64 | Access violation during startup | n/a | Also fails with 26.2.4 `dxil.dll` directory on PATH; no visual result |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled | SM64 | Severe visual corruption in menus and gameplay | not measured | User screenshots; process exited normally |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SMALL_TYPES=0` | SM64 | Severe visual corruption persists | not measured | User screenshots |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SINGLE_THREADED_COMMAND=1` inherited at startup | SM64 | Severe visual corruption persists | not measured | C++ log confirms single-threaded command processing; normal exit |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SINGLE_THREADED_COMMAND=1`, `UBERSHADER=1` | SM64 | User confirms perfect rendering | not measured | Normal exit (0); first visually successful Dozen configuration |
| 2026-10-30 | Intel Arc Pro / 101.8860 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled | SM64 | Visual corruption persists | not measured | User reports improved 3D but worse 2D; screenshots |
| 2026-10-30 | Intel Arc Pro / 101.8860 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `UBERSHADER=1` | SM64 | Visual corruption persists | not measured | User reports similar result to previous Intel test |
| | | | … | MK64 / OoT / GE | | | |

Feature dump (paste from log):

```text
(device name, apiVersion, driverID, subgroupSize, ops, int16/int8, 8/16-bit storage,
 timeline, ext-mem-host, memory heaps, parallel-rdp caps chosen)
```

### P0 automated renderer investigation (2026-10-30)

Built upstream paraLLEl-RDP `395da8f0b896a3158987dcdcdeabfefbad59366d`
(the standalone tree's `COMMIT`) with its pinned Granite and Angrylion submodules.
`rdp_renderer.cpp`, `rdp_device.cpp` and `video_interface.cpp` match the emulator's
copies byte-for-byte. The suite compiles shaders at runtime, whereas gopher64 uses
embedded SPIR-V, so passing here still requires an emulator check.

On NVIDIA, with `PARALLEL_RDP_ALLOW_EXTERNAL_HOST=0` and
`PARALLEL_RDP_SINGLE_THREADED_COMMAND=1` inherited at startup:

| Test | Native Vulkan | mesa-dist-win Dozen 26.2.4, 8-bit storage enabled |
|------|---------------|-----------------------------------------------|
| `rdp-conformance --suite fill-16 --range 0 10` | Pass | Fails at iteration 0: RDRAM dword 3888, expected `0`, got `0x10001` |
| Same failing case with `SUBGROUP=0`, `SMALL_TYPES=0`, both together, or `DZN_DEBUG=nobindless` | not run | Same first mismatch |
| `fill-16`, `fill-8`, `fill-32`, `rasterization-noaa`, `tex-rect`, range 0–10, `FORCE_SYNC_SHADER=1` | not run | All fail |
| Same five suites, range 0–10, `UBERSHADER=1` | not run | All pass |
| `texture-load-tile-16` and `texture-load-tile-8`, default shader path | not run | Pass |
| `rdp-conformance --range 0 10`, `UBERSHADER=1` | not run | All 237 suites pass (reduced iteration count) |
| `vi-conformance --suite aa-none-rgba5551 --range 0 10`, `UBERSHADER=1` | not run | Pass |

This reproduces incorrect rendering without gopher64, ROM input or presentation.
The split rasterization/depth-blend path fails while the combined ubershader path
passes these tests. This does **not** yet identify the faulty compiler pass or
barrier: ubershader changes both shader specialization and the dispatch structure.
The user confirmed perfect SM64 rendering with NVIDIA + ubershader in gopher64;
the process exited normally. Intel already showed corruption with ubershader.
Performance, other games, savestates and upscaling remain unverified. No full P0
or Xbox GO decision follows from this reduced test run.

See [P0 handoff](14-p0-handoff.md) for reproduction commands, preserved local
artifacts and the exact next steps.

## 5. Console results

| Date | Console | Build (tag/commit) | Phase | Test | Result | Notes |
|------|---------|--------------------|-------|------|--------|-------|
| | Series X | | P2 | tile launch ×10 | | |
| | Series X | | P3 | CPU-write visibility smoke test | | |
| | Series S | | P3 | pipeline compile (refused count) | | |
