# 19 — Rendering bring-up on Xbox: test script and failure-mode guide

Audience: the console machine (VS 2026 + Xbox). This condenses what the proven Xbox ports in the
workspace (Ymir D3D12 on CoreWindow; scummvm-uwp) and the mesa-uwp Dozen WSI tell us about getting
an image on screen, so the first-frame work is directed instead of trial-and-error. Evidence and
file:line references are in the [2026-11-04/05 journal archive](handoff-archive/2026-11-04-05.md) (2026-11-05 research entries). PC agents cannot test
any of this; it is console-only.

## 0. Mental model

"`rdp_init` returned" and "Vulkan initialized" do **not** mean a frame is on screen. On UWP the
CoreWindow only becomes visible once `CoreWindow::Activate()` has run, and that only happens when the
event loop is pumped. A correctly rendered first frame before activation is invisible and looks
exactly like a hang. Ymir lost a full debug cycle to this.

## 1. The three failure modes and how to tell them apart from the log

The code is already instrumented; `gopher64-host.log` now carries `[rdp]` (Granite + parallel-rdp,
via the logging bridge) and `[sdl]` lines. Map the log to the failure:

| Symptom in the log | Failure mode | Where / fix |
|---|---|---|
| `Dozen: CoreWindow or Vulkan surface entry point is unavailable` | CoreWindow handle null | surface created before the window exists / wrong thread (`wsi_platform.cpp:18`) |
| `Dozen: vkCreateWin32SurfaceKHR result=<non-zero>` | surface creation failed in the driver | ICD/driver problem; capture Mesa log (M-06, below) |
| `Dozen: surface extent 0x0` or a wrong size | extent 0×0 / stale | CoreWindow not activated or HDMI mode unknown when queried; see §2 and the `uwp_get_width/height` caching caveat (Q-08) |
| `Dozen: initial WSI begin_frame failed` / `WSI end_frame failed` | swapchain acquire/present failed | driver/swapchain; Mesa log needed |
| `end_frame` succeeds but **never** `first touched swapchain frame completed end_frame` | submitting frames that draw nothing (black screen) | the RDP isn't drawing to the backbuffer, or a present/compositor issue — not a crash |
| nothing after `entering rdp_init` | hang (often the CoreDispatcher busy-wait in `uwp_get_width/height` off the UI thread), or activation never pumped | see §2 |

## 2. Do — the activation / present pattern proven on Xbox

- Create the Vulkan surface/swapchain on the thread that owns the CoreWindow (already the design;
  SDL_WinRTApp runs the entry point synchronously on the CoreApplication thread, and gopher64's
  tokio runtime is on a *separate* thread, so `run()`/`rdp_init` stay on the CoreWindow thread).
- Pump SDL events (`SDL_PollEvent`) **early and repeatedly** during boot so `CoreWindow::Activate()`
  fires and the splash is dismissed. Do not gate the first pump behind a fully-finished init.
- If any init step (ROM load, Dozen device creation) can exceed ~1 s, keep it off the UI thread and
  keep pumping + presenting something on the UI thread meanwhile (scummvm's `BootWorker` +
  `PresentBlank` pattern — for PLM responsiveness, not to force visibility).
- Trust Mesa's CoreWindow swapchain choices (they are inherited for free via D-07): it already uses
  `CreateSwapChainForCoreWindow`, `DXGI_SCALING_STRETCH`, extent from the real HDMI mode, no
  frame-latency waitable object, no tearing. Do **not** override these from gopher64's side.

## 3. Do NOT

- Don't treat a successful `rdp_init`/Vulkan init as "something is on screen".
- Don't run two swapchains/renderers on the same CoreWindow at once. The host's D3D11 status screen
  and the Dozen game swapchain must never be live simultaneously — scummvm-uwp proved this deadlocks
  (`WrongThreadException` between the two graphics stacks). This is the single biggest risk (Q-06).
- Don't use the frame-latency waitable object, `SetMaximumFrameLatency`, or tearing on the
  CoreWindow swapchain — no Xbox port in the workspace does.
- Don't trust "works under F5" as proof — the debugger changes activation/suspend timing. Verify
  from a cold tile launch too.

## 4. First console test, in order (do this isolated, before anything else in P3/P5)

1. **Status frame alone.** Tile-launch with no ROM. Confirm the ImGui/D3D11 status screen appears
   (not just "presented" in the log — actually visible on the TV). This proves activation + present.
2. **D3D11 → Dozen hand-over (Q-06).** The highest-risk step. Before `gopher64_uwp_main`/`rdp_init`
   runs, the host must **fully** tear down its D3D11 status renderer: `SDL_DestroyRenderer` (and the
   window if it recreates one), and confirm the DXGI swapchain/device references are actually
   released — not just "destroy issued". Only then let Dozen touch the CoreWindow. If this deadlocks,
   the documented fallback (07 §2.2) is a Vulkan-rendered shell, not trying to interleave two stacks.
3. **Autoboot a ROM (SM64).** Watch the `[rdp]` sequence: surface result, extent (non-zero, matches
   the TV, e.g. 3840×2160 or 1920×1080), `initial WSI begin_frame completed`, then
   `first touched swapchain frame completed end_frame`. The last line is the proof a real frame
   reached the swapchain.
4. **Series S as well** — stricter validator, less memory; and confirm the extent matches its output.

## 5. What is still missing for full diagnosis (console/VS work)

- **M-06 — Mesa/Dozen driver-internal log hook.** Granite and SDL logs now reach the host log, but
  the driver's own internal logging (`mesa_log*`/`vk_log` inside `vulkan_dzn.dll`) does not. When a
  surface/swapchain call fails *inside* the driver, the reason is still invisible. Implement M-06 in
  the mesa-uwp fork (route Mesa logging to a callback or to a file the host reads). High priority —
  the roadmap already lists it first for P3.
- **Q-08 extent caching.** `uwp_get_width/height` in mesa-uwp caches the first result forever with no
  invalidation. If it is queried before the HDMI mode is known, the whole run is stuck at the wrong
  size. Confirm the first query happens after activation; consider a re-probe if supporting a
  mid-session mode/resolution change or both console models from one build.

## 6. PC-side support already in place (so the console agent isn't blocked)

- XAudio2 audio backend replaces the SDL WASAPI deadlock (D-08); host links `xaudio2`.
- `[rdp]` (Granite) and `[sdl]` logging bridges into `gopher64-host.log` (this doc §1 depends on them).
- Instrumentation around surface creation, extent, and first-touched-frame in
  `parallel-rdp/wsi_platform.cpp` and `interface.cpp`.
- Config C ABI for the shell: `gopher64_uwp_config_get` / `gopher64_uwp_config_set` /
  `gopher64_uwp_last_error` / `gopher64_uwp_string_free` (07 §2.2 Settings page can be built on these
  now; they are plain file I/O, independent of the renderer).
