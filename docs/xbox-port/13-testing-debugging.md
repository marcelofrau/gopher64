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

| Date | PC GPU / driver | Dozen build | Mode | Game | Result | Speed | Notes |
|------|-----------------|-------------|------|------|--------|-------|-------|
| | | | native Vulkan (baseline) | SM64 | | | |
| | | | Dozen, coherent | SM64 | | | |
| | | | Dozen, `ALLOW_EXTERNAL_HOST=0` | SM64 | | | |
| | | | Dozen, `SUBGROUP=0` | SM64 | | | |
| | | | Dozen, 8-bit on | SM64 | | | |
| | | | … | MK64 / OoT / GE | | | |

Feature dump (paste from log):

```text
(device name, apiVersion, driverID, subgroupSize, ops, int16/int8, 8/16-bit storage,
 timeline, ext-mem-host, memory heaps, parallel-rdp caps chosen)
```

## 5. Console results

| Date | Console | Build (tag/commit) | Phase | Test | Result | Notes |
|------|---------|--------------------|-------|------|--------|-------|
| | Series X | | P2 | tile launch ×10 | | |
| | Series X | | P3 | CPU-write visibility smoke test | | |
| | Series S | | P3 | pipeline compile (refused count) | | |
