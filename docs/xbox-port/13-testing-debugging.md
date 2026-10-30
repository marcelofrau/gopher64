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

**2026-10-30 matrix result:** all four reference games rendered correctly in the
user's short desktop tests on NVIDIA with Dozen 26.2.4 + ubershader + 8-bit storage
enabled + external host import disabled + single-threaded commands. Reported
speed matched native. SM64 passed at 1×/2×/4×; MK64, OoT and GoldenEye passed
save/load round trips. Recommendation: scoped GO for P1 with this fallback.
Coherent Dozen mode, Intel correctness and Xbox behavior remain unresolved;
this is not completion of the original coherent-and-incoherent exit criterion.

2026-10-30 preliminary setup: the prebuilt [mmozeiko/build-mesa 26.2.6](https://github.com/mmozeiko/build-mesa/releases/tag/26.2.6) `mesa-dzn-x64` archive passed its published SHA-256 check. With `VK_DRIVER_FILES` set to its manifest, `vulkaninfo --summary` reported Dozen 26.2.6 for NVIDIA RTX 3500 Ada, Intel Arc and Microsoft Basic Render Driver. The NVIDIA device exposes `VK_EXT_external_memory_host`; actual coherent-mode use remains to be verified in the renderer. No game result is implied by this setup check.

| Date | PC GPU / driver | Dozen build | Mode | Game | Result | Speed | Notes |
|------|-----------------|-------------|------|------|--------|-------|-------|
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | native Vulkan (baseline) | SM64 | User reports image and audio working | not measured | portable data directory |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | native Vulkan, `ALLOW_EXTERNAL_HOST=0` | SM64 | User reports correct rendering, no visible faults | not measured | Control for the incoherent RDRAM path; normal exit |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | native Vulkan, external host disabled, single-threaded commands, `BENCH=1` | SM64 | Correct rendering; castle courtyard screenshot | Opening 20 FPS, menu/courtyard 30 FPS; 60 VI/s throughout (user observed) | Full-speed reference; native state saved for Dozen comparison |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 1×, 8-bit storage on, external host disabled, single-threaded commands, ubershader, `BENCH=1` | SM64 | Native savestate loaded with F7; image remains correct | 30 FPS / 60 VI/s sustained; brief 29/59 then 31/61 (user observed) | Normal exit; no sustained slowdown observed in courtyard |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Same working Dozen configuration, 2× upscale | SM64 | User confirms perfect rendering | Stable 30 FPS / 60 VI/s (user observed) | Normal exit; log `out/p0-performance/dozen-2x-20261030-111307-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Same working Dozen configuration, 4× upscale | SM64 | User confirms perfect rendering | Stable 30 FPS / 60 VI/s (user observed) | Normal exit; log `out/p0-performance/dozen-4x-20261030-111354-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SUBGROUP=0` | SM64 | Severe visual corruption persists | not measured | User screenshots |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | Mesa Dozen 26.2.6 | Dozen, default 8-bit off | SM64 | Fails to initialise: 8-bit storage required | n/a | `storageBuffer8BitAccess` is mandatory |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | Mesa Dozen 26.2.6 | Dozen, 8-bit on, external host disabled | SM64 | Access violation during startup | n/a | Also fails with 26.2.4 `dxil.dll` directory on PATH; no visual result |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled | SM64 | Severe visual corruption in menus and gameplay | not measured | User screenshots; process exited normally |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SMALL_TYPES=0` | SM64 | Severe visual corruption persists | not measured | User screenshots |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SINGLE_THREADED_COMMAND=1` inherited at startup | SM64 | Severe visual corruption persists | not measured | C++ log confirms single-threaded command processing; normal exit |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `SINGLE_THREADED_COMMAND=1`, `UBERSHADER=1` | SM64 | User confirms perfect rendering | not measured | Normal exit (0); first visually successful Dozen configuration |
| 2026-10-30 | Intel Arc Pro / 101.8860 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled | SM64 | Visual corruption persists | not measured | User reports improved 3D but worse 2D; screenshots |
| 2026-10-30 | Intel Arc Pro / 101.8860 | mesa-dist-win 26.2.4 | Dozen, 8-bit on, external host disabled, `UBERSHADER=1` | SM64 | Visual corruption persists | not measured | User reports similar result to previous Intel test |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | Native Vulkan, 1×, external host disabled, single-threaded commands, `BENCH=1` | MK64 | User confirms correct image/audio and save/load round trip | Stable 30 FPS / 60 VI/s (user observed) | Normal exit; log `out/p0-performance/mk64-native-20261030-111719-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Working Dozen configuration, 1×, `BENCH=1` | MK64 | User confirms correct image/audio, native state load and new Dozen save/load round trip | Stable 30 FPS / 60 VI/s (user confirmed) | Normal exit; log `out/p0-performance/mk64-dozen-20261030-111835-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | Native Vulkan, 1×, external host disabled, single-threaded commands, `BENCH=1` | OoT (U, V1.2) | User confirms correct image/audio and save/load round trip | 20 FPS / 60 VI/s (user observed) | Normal exit; log `out/p0-performance/oot-native-20261030-111921-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Working Dozen configuration, 1×, `BENCH=1` | OoT (U, V1.2) | User confirms same correct image/audio and new save/load round trip | 20 FPS / 60 VI/s (user confirmed) | Normal exit; log `out/p0-performance/oot-dozen-20261030-112131-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | n/a | Native Vulkan, 1×, external host disabled, single-threaded commands, `BENCH=1` | GoldenEye (U) | User reports good rendering; savestate file present | 23–32 FPS; VI/s almost always 60 (user observed) | Normal exit; log `out/p0-performance/ge-native-20261030-112315-stderr.log` |
| 2026-10-30 | NVIDIA RTX 3500 Ada / 596.72 | mesa-dist-win 26.2.4 | Working Dozen configuration, 1×, `BENCH=1` | GoldenEye (U) | User confirms same result as native, including image/audio and requested save/load check | Same reported 23–32 FPS range; VI/s almost always 60 | Normal exit; log `out/p0-performance/ge-dozen-20261030-112527-stderr.log` |

### Working driver feature dump

Source: `out/p0-performance/vulkaninfo-dozen-26.2.4-8bit.txt`, GPU0;
captured with the working manifest and `dzn_enable_8bit_loads_stores=true`.
These are advertised capabilities, not proof every renderer path used them.

```text
deviceName: Microsoft Direct3D12 (NVIDIA RTX 3500 Ada Generation Laptop GPU)
apiVersion: 1.2.354; deviceType: DISCRETE_GPU
driverID: MESA_DOZEN; driverInfo: Mesa 26.2.4 (git-9a0169f2a7)
subgroupSize: 32; supported stages include COMPUTE
subgroup operations: BASIC, VOTE, ARITHMETIC, BALLOT, SHUFFLE, SHUFFLE_RELATIVE, QUAD
shaderInt16 / shaderInt8: true / true
storageBuffer16BitAccess / storageBuffer8BitAccess: true / true
timelineSemaphore: true
VK_EXT_external_memory_host: advertised; minImportedHostPointerAlignment: 65536
heap 0: 17.93 GiB, non-device-local; heap 1: 11.73 GiB, device-local
type 0: heap 0, flags none
type 1: heap 0, HOST_VISIBLE | HOST_COHERENT
type 2: heap 0, HOST_VISIBLE | HOST_COHERENT | HOST_CACHED
type 3: heap 1, DEVICE_LOCAL
```

Renderer logs confirm: incoherent RDRAM fallback, small integer arithmetic enabled,
ubershader enabled and single-threaded command processing. Swapchain: 640×480,
format enum 44 (B8G8R8A8_UNORM), present mode enum 1 (MAILBOX), two images.
The exact selected subgroup tile-binning/depth-blend flags were not logged;
do not infer them from device capabilities. Budgets/heaps are desktop measurements.

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
The subsequent 1× courtyard comparison sustained 30 FPS / 60 VI/s on both native
and Dozen; loading the native savestate on Dozen preserved correct rendering.
Follow-up 2× and 4× tests also rendered correctly at 30 FPS / 60 VI/s in SM64.
MK64, OoT and GoldenEye subsequently matched their native baselines and passed
user-observed Dozen save/load checks. This supports a scoped P1 GO recommendation,
not an Xbox correctness/performance claim or a bit-exact savestate comparison.

Performance logs are in local `out/p0-performance/`: native run
`20261030-110801` and Dozen run `20261030-111036`. Whole-session GPU timestamp
averages for `render-pass` were 0.721 and 9.486 ms/frame-context respectively.
These are not total frame times or a scene-controlled benchmark; the sessions
include startup and different dwell times. Dozen costs more GPU time but did not
show sustained emulation slowdown in the observed scene. The native savestate
was backed up in `native-states-20261030-111036/` before loading it on Dozen.

See [P0 handoff](14-p0-handoff.md) for reproduction commands, preserved local
artifacts and the exact next steps.

## 5. Console results

| Date | Console | Build (tag/commit) | Phase | Test | Result | Notes |
|------|---------|--------------------|-------|------|--------|-------|
| | Series X | | P2 | tile launch ×10 | | |
| | Series X | | P3 | CPU-write visibility smoke test | | |
| | Series S | | P3 | pipeline compile (refused count) | | |
