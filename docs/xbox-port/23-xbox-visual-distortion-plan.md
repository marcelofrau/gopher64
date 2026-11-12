# 23 — Xbox visual distortion: investigation and fix plan

**Status (2026-11-12, written on W11-4NSYSB4):**
- Two Dozen fixes, five diagnostic tools and a PC-parity mode are committed. None of them has run
  on a GPU yet.
- This document is the step-by-step plan for the next agent. Follow it in order and record every
  result in [memory-handoff](memory-handoff.md).
- Decision: D-07 (Dozen). Phase: P3. Mitigations: M-03 and M-11 in
  [04 §3.6](04-vulkan-dozen.md).

## 0. Rules that apply to every step

- **Xbox:**
  - Never connect to, deploy to, or launch on the Xbox.
  - The user deploys, presses F5, edits `env.txt` through Device Portal, and downloads logs and
    BMPs.
  - Your job is to prepare builds and exact instructions, then read the results.
- **W11-4NSYSB4 is a corporate machine:**
  - Never launch gopher64, a ROM, or anything that opens a window there.
  - Builds, `vulkaninfo --summary`, and headless console probes are allowed.
- **YODA:**
  - PC game runs (VS F5, Local Machine) are done by the user.
  - You may run headless probes and the test scripts.
- **Fork rules** ([00](00-upstream-fidelity-rules.md)):
  - Port code goes only in port-owned paths.
  - Upstream files get gated `UWP-PORT` hooks only.
  - Never touch the `parallel-rdp/parallel-rdp-standalone` submodule. The VI shaders and Granite
    are upstream code, so fixes go in Dozen (`../mesa-uwp`, branch `gopher64/dzn-xbox`) or in
    port-owned files.
  - Commit messages use the `uwp:` prefix plus the decision ID. In the Mesa fork, use the `dzn:`
    prefix.
  - Never push without the user's approval.
  - Run the `fidelity-reviewer` agent before every gopher64 commit.
- **Records:**
  - No absolute paths in docs. Name sibling repos as `../mesa-uwp`.
  - Record the SHA-256 of every DLL you build and every DLL a run loaded. The log prints it in the
    `Dozen: ... sha256=` line.

## 1. Symptom and established facts

The facts below come from the 2026-11-11 Xbox runs ([archive](handoff-archive/2026-11-11.md)).

- **Xbox state:** Series console, device `SraKmd_arden`, UWP Release | x64.
  - Boot works, every compute and graphics pipeline compiles (`success: yes`), and the game runs
    at a steady 60 VI/s.
  - XAudio2 plays with no underruns.
- **Image:** wrong.
  - The 4:3 viewport inside the 1920x1080 swapchain has the right size and position.
  - Vertically, the colour regions match the real SM64 scene.
  - Horizontally, every scanline is scrambled or smeared into coloured bands.
  - On the title screen, the ~14-line copyright banner becomes ~14 repeating vertical columns.
- **The corruption is deterministic.**
  - Screenshots `2026-11-11-14-42-02-a054.png` (DLL `0D3AB0AE...`) and
    `2026-11-11-15-17-21-7575.png` (DLL `3F06E897...`, commit `797c123548e`) are pixel-for-pixel
    identical.
  - Between those two runs the memory types changed: `WRITE_BACK` became `WRITE_COMBINE`, and the
    device type changed from `INTEGRATED` to `DISCRETE`.
  - So the cause is not CPU cache coherency and not a CPU/GPU race. It is a deterministic wrong
    state, a wrong descriptor, or a miscompile.
- **PC state:** YODA, RTX 4070 SUPER, same UWP build.
  - Renders correctly with `DZN_DEBUG=no16bit` (bindless + subgroups).
  - The exact Xbox-parity configuration has **never** run on PC.

## 2. What is ruled out, and why

| Candidate | Status | Evidence |
|---|---|---|
| CPU cache / WRITE_BACK visibility (M-01) | Ruled out for this symptom | Memory type change did not change a single pixel. `R-02 cpu-write visibility: ok` on Xbox. |
| Swapchain row pitch / DXGI present | Unlikely | The viewport rectangle and its edges are sharp and correct. Also checked by step X1 below: `scanout.bmp` vs screenshot. |
| Black-screen hang / shader compile | Fixed | Mesa `f476404419f` (`optimize_nir` loop). 60 VI/s on Xbox. |
| Typed UAV *loads* missing for 32bpp formats (Azahar / OnyxEMU) | Unlikely | No paraLLEl-RDP shader calls `imageLoad`. The only storage image is `extract_vram.comp:28`, which is `writeonly` (typed **store** only). Check the `G64-DZN-FMT` line anyway. |
| Texel-buffer offset truncation | Ruled out | `dzn_buffer_view_create` (`dzn_image.c`) uses `FirstElement = offset / blksz`. paraLLEl-RDP always uses offset 0 and gives every `VkBuffer` its own placed resource. |

## 3. Pipeline map: where the image can break

The frame flows through these stages:

```
CPU emulation ──► RDP command stream ──► Dozen compute (span_setup, binning, ubershader)
                                           writes RDRAM SSBO (8/16-bit stores emulated in 32-bit)
                                                   │
                     ┌─────────────── GPU→CPU copy │ (non-coherent path, staging_readback)
                     ▼                             ▼
          gfx_info.RDRAM (CPU) ◄──── stage A: what the RDP rendered
                                                   │
VI (parallel-rdp video_interface.cpp):             ▼
  extract_vram.comp  (async compute queue) RDRAM16/32 SSBO ─► vram_image R8G8B8A8_UINT (imageStore)
  vi_fetch.frag      utexture2D vram_image ─► aa_image   R8G8B8A8_UINT 2D_ARRAY (1–2 layers), RTV per layer
  vi_divot.frag      utexture2DArray aa    ─► divot_image R8G8B8A8_UINT 2D_ARRAY (1–2 layers)
  vi_scale.frag      utexture2DArray divot + itextureBuffer HorizontalInfo (R32G32B32A32_SINT)
                     + utextureBuffer gamma LUT (R8_UINT) ─► scale_image R8G8B8A8_UNORM (MUTABLE_FORMAT)
                                                   │
                                                   ▼  stage B: VI output (scanout_sync readback)
interface.cpp render_frame: blit shader samples scale_image ─► swapchain (CoreWindow WSI)
                                                   │
                                                   ▼  stage C: what the TV shows (screenshot)
```

Resource facts, with sources in `parallel-rdp/parallel-rdp-standalone`:
- **vram_image** (`video_interface.cpp:405-510`):
  - Usage `STORAGE|SAMPLED`, `SHARING_MODE_CONCURRENT`, written on the async compute queue and
    sampled on the graphics queue.
  - Dozen creates it `ALLOW_SIMULTANEOUS_ACCESS`.
- **aa_image / divot_image** (`:512-698`):
  - Created with `FORCE_ARRAY`, so the default view is `VK_IMAGE_VIEW_TYPE_2D_ARRAY`.
  - They have 2 layers only when FETCH_BUG is active (`:398-403`, `init_y_add < 1024 && scaling == 1`).
  - SM64 at 240p normally has 1 layer.
  - Each layer is rendered through an explicit 2D RTV.
  - `vi_divot.frag:32` and `vi_scale.frag:30` declare `utexture2DArray`.
- **HorizontalInfo** (`:700-717`): a new 9216-byte `LinkedDeviceHost` buffer every frame, read at
  `2*line+{0,1}` (`vi_scale.frag:83-84`).
- **scale_image** (`:719-999`):
  - Format `R8G8B8A8_UNORM`, created with `MUTABLE_FORMAT` (UNORM/SRGB list).
  - Render pass loadOp is CLEAR.
- **Granite enables `synchronization2`** (`vulkan/context.cpp:1673,2058`).
  - Every Granite render pass then uses `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` and
    `READ_ONLY_OPTIMAL` (`vulkan/render_pass.cpp:534,572,599,622,646`).

## 4. PC vs Xbox: every difference we know of

| Area | PC (YODA / W11-4NSYSB4) | Xbox `SraKmd_arden` | Parity tool |
|---|---|---|---|
| Native 16-bit ops | yes | no (`native16=0`, proven) | `DZN_DEBUG=no16bit` (hardcoded in `dozen_loader.cpp`) |
| Bindless / BDA | yes | no (proven by `Allow subgroups = 0`, non-bindless log) | `DZN_DEBUG=nobindless` (hardcoded) |
| Subgroup size control | yes | no | cleared by `no16bit` in Mesa `797c123548e` |
| Enhanced barriers | yes (`enhanced_barriers=1`) | **assumed no** | `DZN_DEBUG=legacycaps` (auto on PC since `781f4d9`) |
| Relaxed format casting | yes | **no** per OnyxEMU `0011` ("no Agility SDK on Xbox") | `legacycaps` |
| CastingFullyTypedFormat | yes | assumed no | `legacycaps` |
| Unrestricted copy pitch | yes | assumed no | `legacycaps` |
| Shader model | 6.8 (`sm=0x68`) | **unknown** | `DZN_G64_MAX_SHADER_MODEL=0x6N` |
| Wave size | NVIDIA 32 | RDNA2: 32 or 64, unknown | none (no subgroup ops used) |
| UMA | no | yes | `_XBOX_UWP` memory types (`797c123548e`) |
| D3D12 runtime + shader compiler | Windows 11 + NVIDIA | GameOS + AMD | none: the remaining difference after parity |
| Debug layer behaviour | NVIDIA tolerates invalid states/descriptors | AMD acts on them (DCC/compression, descriptor layout) | none |

The "assumed" rows become facts as soon as the Xbox prints its `G64-DZN-CAPS` line (step X1).

## 5. Root-cause candidates found on 2026-11-12, both already fixed in the Mesa fork

### F1 — Undefined D3D12 resource state on the legacy-barrier path (prime suspect)

- **Code:** `../mesa-uwp/src/microsoft/vulkan/dzn_image.c`, `dzn_image_layout_to_state`.
  - Before the fix, the switch had no case for `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL`,
    `READ_ONLY_OPTIMAL`, `STENCIL_READ_ONLY_OPTIMAL` or `SHARED_PRESENT_KHR`.
  - It fell to `default: UNREACHABLE("not implemented")`.
  - On MSVC release builds that macro is `assert` + `__assume(0)` (`src/util/macros.h:131-136`).
    That is undefined behaviour. In practice the compiler deletes the default branch and returns
    whatever the last compared case leaves, which is a **deterministic** wrong state.
- **Who calls it:** only the legacy (non-enhanced) barrier path calls this function:
  - `dzn_cmd_buffer_queue_image_range_layout_transition` (`dzn_cmd_buffer.c:196-251`), from the
    legacy `vkCmdPipelineBarrier2` image loop (`:1144-1163`);
  - `dzn_cmd_buffer_clear_attachment` (legacy branch ~`:2218-2237`), used by the scale pass CLEAR;
  - resolve (~`:4772`).
  - The enhanced path uses `dzn_vk_layout_to_d3d_layout`, which already handles
    `ATTACHMENT_OPTIMAL` (`dzn_image.c:716-718`). That is why the PC, with enhanced barriers,
    renders correctly.
- **Why it matches the symptom:**
  - The render targets (`aa_image`, `divot_image`, `scale_image`) would end up in a wrong state
    while being rendered to, or sampled from, on AMD hardware.
  - Rendering into or sampling from an RGBA8 surface in the wrong state skips the colour
    decompression (DCC) and metadata transitions. The result is deterministic garbage that keeps
    the frame geometry (row and band structure) but scrambles pixels along each row. NVIDIA
    tolerates the same mistake.
  - This is an inference. Steps X1–X3 prove or disprove it.
- **Fix:** Mesa `c0ea943b3b1` maps the sync2 layouts like the enhanced path does:
  - colour `ATTACHMENT_OPTIMAL` becomes `RENDER_TARGET`;
  - `READ_ONLY_OPTIMAL` becomes the shader-resource states.
  - Any layout still unhandled now logs `G64-DZN-WARN: dzn_image_layout_to_state: unhandled
    layout N, using COMMON` (first 16 times) instead of UB.
- **Toggle:** `DZN_G64_DISABLE_FIX=layouts` restores the old behaviour, for A/B testing.

### F2 — 1-layer `2D_ARRAY` views bound as `TEXTURE2D`

- **Code:** `dzn_image.c`, `dzn_image_view_prepare_srv_desc` and `..._uav_desc`.
  - `use_array` was `base_array_layer > 0 || layer_count > 1`.
  - So `aa_image` and `divot_image` (view type `2D_ARRAY`, 1 layer when FETCH_BUG is off) became
    a `D3D12_SRV_DIMENSION_TEXTURE2D` SRV.
  - `vi_divot.frag` and `vi_scale.frag` declare `utexture2DArray`, which in DXIL is a
    Texture2DArray resource (`dxil_module.c:2091-2092`).
  - A dimension mismatch between the shader and the descriptor is undefined in D3D12. The debug
    layer reports it; NVIDIA happens to work; AMD descriptor layouts differ.
- **Fix:** in `c0ea943b3b1`, `*_ARRAY` view types are always arrays.
- **Toggle:** `DZN_G64_DISABLE_FIX=arrayviews`.

Both fixes are in one DLL. `DZN_G64_DISABLE_FIX=layouts,arrayviews` comes close to the pre-fix
behaviour, but not exactly:
- `arrayviews` off is exactly the old code.
- `layouts` off no longer runs into UB. The sync2 layouts become `COMMON` and each one logs a
  `G64-DZN-WARN ... unhandled layout 1000314001` (ATTACHMENT_OPTIMAL) or `1000314000`
  (READ_ONLY_OPTIMAL) line, up to 16 lines.
  - Those lines alone prove the legacy path really runs on that machine.
- The exact old behaviour is the previous DLL `3F06E897...`, already known to distort on Xbox.

## 6. Tools now available (all controlled from `LocalState/env.txt`, no rebuild)

`env.txt` holds one `KEY=VALUE` per line; `#` starts a comment. It is applied by
`host_log_apply_env_file` before Dozen loads.

- **`DZN_DEBUG` on Xbox:** the loader *appends* the value to the hardcoded `no16bit,nobindless`.
- **`DZN_DEBUG` on PC:** `uwp/host/main.cpp` first sets `DZN_DEBUG=legacycaps`, and an `env.txt`
  `DZN_DEBUG` line *replaces* that value. So on PC, write `DZN_DEBUG=legacycaps,<more>`.
- The final string is logged as `Dozen: 8-bit storage and DZN_DEBUG=... requested`.

| Key / line | Where | What it does / prints |
|---|---|---|
| (always) `G64-DZN-CAPS: "<GPU>" warp= fl= sm= uma= ccuma= heap_tier= typed_uav_load_ext= wave=min-max native16= view_inst_tier= cast_fully_typed= enhanced_barriers= relaxed_casting= unrestricted_pitch= legacycaps=` | Mesa `d0b7c8827ab` | **Raw** caps per GPU, logged before any override. The Xbox truth table. |
| (always) `G64-DZN-FMT: "<GPU>" dxgi=N hr= support1= support2=` | Mesa `99ebfcfba5a` | Format support for DXGI 30 `R8G8B8A8_UINT`, 28 `R8G8B8A8_UNORM`, 27 `R8G8B8A8_TYPELESS`, 87 `B8G8R8A8_UNORM`, 4 `R32G32B32A32_SINT`, 42 `R32_UINT`, 57 `R16_UINT`, 62 `R8_UINT`. Support2 bits: 0x40 UAV typed load, 0x80 UAV typed store. Support1: 0x1 buffer, 0x100 shader load, 0x4000 render target, 0x2000000 typed UAV. PC NVIDIA baseline for `R8G8B8A8_UINT`: support1=0x027051f3, support2=0x3c0. |
| `DZN_DEBUG=legacycaps` | Mesa `d0b7c8827ab` | Clears EnhancedBarriers, RelaxedFormatCasting, CastingFullyTypedFormat and UnrestrictedCopyPitch. **Auto-set on PC** by the host. |
| `DZN_G64_MAX_SHADER_MODEL=0x65` | Mesa `0d5764304ac` | Clamps the shader model, which lowers the DXIL version. Use the Xbox `sm=` value. Logs `G64-DZN-CAPS: DZN_G64_MAX_SHADER_MODEL=... -> sm=`. |
| `DZN_G64_DISABLE_FIX=layouts,arrayviews` | Mesa `c0ea943b3b1` | Turns F1 and/or F2 off for A/B. |
| `G64_VI_DISABLE=aa,scale,serrate,dither,divot,gamma` or `all` | gopher64 `2218981` | Sets `ScanoutOptions.vi.*` to false, which skips VI stages or variants (`video_interface.hpp:66-74`). Logs `vi-probe: G64_VI_DISABLE=...`. |
| `G64_VI_DUMP_FRAMES=300,600,1200` | gopher64 `2218981` | At those `render_frame` counts (~1 per VI frame), it: (1) calls `processor->idle()` and writes the CPU-side RDRAM framebuffer at `VI_ORIGIN` as `g64-vi-f<N>-rdram.bmp` (stage A); (2) calls `scanout_sync` and writes `g64-vi-f<N>-scanout.bmp` (stage B). It logs the VI registers, the size and an FNV hash. |
| (always on UWP) FPS OSD | gopher64 `781f4d9` | `FPS: x VI/S: y` text at the bottom. It is uploaded from the CPU and drawn by the same blit as the game image. |

**Retrieving files:** in Xbox Device Portal, open File explorer, then LocalAppData, then
`Gopher64.UWP.Dev_<publisher hash>`, then `LocalState`. Download `gopher64-host.log` (and
`gopher64-host.prev.log`) and `g64-vi-*.bmp`. Upload `env.txt` there. On PC the same folder is
`%LOCALAPPDATA%\Packages\Gopher64.UWP.Dev_<hash>\LocalState`.

**Viewing BMPs as an agent:** the Read tool shows PNG/JPG, not BMP. Convert first (tested on
2026-11-12):
```powershell
Add-Type -AssemblyName System.Drawing
Get-ChildItem <folder>\g64-vi-*.bmp | ForEach-Object {
  $i = [System.Drawing.Image]::FromFile($_.FullName); $i.Save("$($_.FullName).png", [System.Drawing.Imaging.ImageFormat]::Png); $i.Dispose() }
```
- Check the PC P1 `rdram.bmp` before trusting any Xbox comparison. It must look like the SM64
  frame. If colours are swapped or pixels pair-swapped, the probe's RGBA5551 / `^1` decode in
  `g64_diag.cpp` is wrong. That code has no unit test.
- Also ask the user what they see. Their description of the FPS text and of the screen is part of
  the evidence.

## 7. Procedure

### Step 0 — Sync and build (agent on YODA)

1. Run `git pull` in gopher64 on `uwp/main`. You need at least `2218981` plus the doc commits.
2. In `../mesa-uwp`, run `git fetch origin` and check out `gopher64/dzn-xbox` at `c0ea943b3b1` or
   later.
3. Build `vulkan_dzn.dll` and its `.pdb` with the
   [22 §Reconstructing](22-dozen-storage-investigation.md#reconstructing-the-experimental-driver)
   recipe.
   - Add the two `Directory.Build.targets` defines recorded in the
     [2026-11-09 archive](handoff-archive/2026-11-09.md): `_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS`
     and PDB generation.
   - Then rebuild only the `vulkan_dzn` vcxproj with MSBuild (`Configuration=release`, `Platform=x64`).
4. Back up the current `out/uwp-dozen/vulkan_dzn.dll` as `vulkan_dzn.3F06E897.bak`; never delete
   backups. Then copy the new DLL and PDB into `out/uwp-dozen/`. Keep `dxil.dll` `DB108FFE...`.
5. Run `scripts/uwp/build-host.ps1 -Configuration Release -SkipDependencies`.
   - Expect: module audit passed, and the MSIX `vulkan_dzn.dll` hash equals the new hash.
6. Run the regression tests:
   - `scripts/uwp/tests/dozen-storage.tests.ps1` with `DZN_DEBUG=no16bit,nobindless,legacycaps`
     must PASS;
   - `scripts/uwp/tests/host-log.tests.ps1` must PASS;
   - the `uber_probe.exe` from 2026-11-11, if it is still on YODA scratch, with
     `DZN_DEBUG=no16bit,nobindless,legacycaps`, must PASS.
7. Optional but valuable: check whether the D3D12 debug layer exists with
   `Test-Path $env:windir\System32\d3d12SDKLayers.dll`.
   - If it is missing, it comes from the Windows optional feature "Graphics Tools". Ask the user;
     do not install it yourself.

### Step P1 — PC F5 with parity (user presses F5, Local Machine, Release | x64)

1. Put `G64_VI_DUMP_FRAMES=300,600,900,1200,1800` in the PC `LocalState/env.txt`. Do not add
   `DZN_DEBUG`: the host sets `legacycaps`.
2. Check the log. Expected lines:
   - `PC parity: DZN_DEBUG=legacycaps ...`
   - `Dozen: ... sha256=<new hash>`
   - `G64-DZN-CAPS: "NVIDIA GeForce RTX 4070 SUPER" ... enhanced_barriers=1 ... legacycaps=1`
   - `G64-DZN-FMT` × 8
   - `vi-probe: G64_VI_DISABLE="" (mask=0x0) G64_VI_DUMP_FRAMES="300,..." (5 frames)`
   - five pairs of `vi-probe: ...-rdram.bmp ... written` / `...-scanout.bmp ... written`
3. Collect every `G64-DZN-WARN` line. There should be none; any line names a layout the fix missed.
4. Expected image: correct, because the PC has F1 and F2 fixes plus legacy caps. The BMPs are the
   reference set for the Xbox comparison.
5. **P1b, PC repro attempt:** the same run with these two `env.txt` lines added. Keep `legacycaps`
   in `DZN_DEBUG`, because an `env.txt` `DZN_DEBUG` replaces the host default:
   ```
   DZN_DEBUG=legacycaps
   DZN_G64_DISABLE_FIX=layouts,arrayviews
   ```
   - If the PC now shows the Xbox-style distortion, F1/F2 are confirmed on PC.
   - Then bisect with `=layouts` alone and `=arrayviews` alone, and record which one matters.
   - If the PC still renders cleanly, NVIDIA tolerates both bugs. That is expected and does not
     disprove them; the Xbox run X1 decides.
6. **P1c, only if the debug layer is installed:** `DZN_DEBUG=legacycaps,d3d12` with
   `DZN_G64_DISABLE_FIX=layouts,arrayviews`.
   - VS Output should show `D3D12 ERROR` lines about `ResourceBarrier` before/after state mismatch
     and/or SRV dimension mismatch.
   - Then run again without `DZN_G64_DISABLE_FIX`; those errors should be gone.
   - Paste the first 20 distinct errors into the handoff.

### Step X1 — Xbox run with both fixes (user only)

`env.txt` on the Xbox:
```
G64_VI_DUMP_FRAMES=300,600,900,1200,1800
```

Collect: `gopher64-host.log`, every `g64-vi-*.bmp`, a Dev Portal screenshot at the title screen
and at the castle intro, and the user's description of the FPS text (clean or distorted).

Record in the handoff, verbatim:
- the `G64-DZN-CAPS` line(s) and all `G64-DZN-FMT` lines for `SraKmd_arden`;
- `Dozen: ... sha256=`;
- any `G64-DZN-WARN`, `nir_to_dxil:`, `CreatePipelineState`, or `device removed` lines;
- the heartbeat rate.

**Reading X1:**
- **Image correct:** F1 and/or F2 was the bug. Go to X2 for attribution, then section 9 (cleanup).
- **Still distorted:** go to the decision tree in section 8, using the BMPs.
- **Update the parity table (section 4)** with the real caps. If any of the four legacy caps is 1
  on Xbox, change `legacycaps` in `dzn_device.c` to clear only the caps that are 0 on Xbox. If
  `sm` is below 0x68, use `DZN_G64_MAX_SHADER_MODEL=<that value>` in every later PC run.

### Step X2 — Attribution (user only, if X1 fixed it)

Run twice, each time with exactly one line in `env.txt`:
1. `DZN_G64_DISABLE_FIX=layouts`
2. `DZN_G64_DISABLE_FIX=arrayviews`

Whichever run brings the distortion back is the root cause. If both do, both bugs matter. Record
the results, then write the upstreaming *proposal* text in [12 §4](12-upstream-sync.md). Never
open an upstream MR or issue yourself; the fork maintainer writes them by hand.

## 8. Decision tree if X1 is still distorted

First compare the BMPs from the same frame numbers on PC (P1) and Xbox (X1). Frame numbers can
drift by a few frames, so compare content, not hashes.

| `rdram.bmp` on Xbox | `scanout.bmp` on Xbox | Screen | Conclusion | Go to |
|---|---|---|---|---|
| wrong | wrong | wrong | The RDP renders wrong into RDRAM (or the GPU→CPU readback is wrong) | 8.1 |
| correct | wrong | wrong | The VI stages are wrong | 8.2 |
| correct | correct | wrong | The final blit, swapchain or WSI is wrong | 8.3 |
| no `vi-probe` lines | | | The env var did not reach the process | Check `env.txt: G64_VI_DUMP_FRAMES=...` in the log and the file encoding (UTF-8, no BOM issues) |

### 8.1 RDP stage wrong

1. Use the FPS OSD as a control. If it is clean, the blit path is fine, which confirms the problem
   is upstream of it.
2. Candidates, most likely first:
   1. **Narrow-storage emulation miscompiled by the Xbox shader compiler.** The 8/16-bit SSBO
      loads and stores are lowered to 32-bit load, shift, mask and atomic ops in DXIL. The same
      DXIL works on NVIDIA.
      - Test on PC with `DZN_G64_MAX_SHADER_MODEL=<Xbox sm>`; this checks for a DXIL-version
        difference.
      - Then diff the DXIL. Use `DZN_DEBUG=legacycaps,dxil` plus `MESA_LOG_FILE=<LocalState>\mesa.log`
        in `env.txt` to dump DXIL on both machines, and compare the ubershader module.
   2. **TMEM / hidden RDRAM host-visible buffers.** These are `CachedCoherentHostPreferCoherent`,
      `WRITE_COMBINE` on Xbox since `797c123548e`. GPU reads of WC memory on UMA should be fine.
      Check `R-02` again.
   3. **Wave64.** If the caps say `wave=64-64`, look for any shader that assumes 32 lanes.
      paraLLEl-RDP runs `SUBGROUP=0`, so this is low probability.
3. Tool: the YODA `rdp_scanout_probe` + `sm64.rdp` trace (2026-11-11 archive) replays the same RDP
   stream headless. Extend it to dump RDRAM after replay on PC with `legacycaps` + SM clamp.

### 8.2 VI stage wrong

Run one Xbox run per `env.txt` line, keeping `G64_VI_DUMP_FRAMES=600`:

| Run | `G64_VI_DISABLE=` | What changes |
|---|---|---|
| V1 | `all` | Simplest VI: no AA, no scale filter, no divot, no dither or gamma dither, no serrate |
| V2 | `scale` | `vi_scale` without bilinear (`SCALE_AA=0`, no `texelFetchOffset`) |
| V3 | `divot` | skips the divot pass (`vi_divot`) |
| V4 | `aa` | skips the AA fetch filter (`vi_fetch` variant) |
| V5 | `dither,gamma` | dither and gamma variants |

- If V1 is clean, add stages back one at a time; the first one that breaks the image is the
  culprit shader.
- If V1 is still wrong, suspect `extract_vram.comp`, the `HorizontalInfo` texel buffer, or the
  `vram_image` cross-queue use.

Known candidates:
1. **`HorizontalInfo` texel buffer** (`R32G32B32A32_SINT` typed Buffer SRV, `LinkedDeviceHost`,
   new every frame).
   - Check `G64-DZN-FMT dxgi=4 support1` bit 0x1 (buffer).
   - If per-line `x_start`/`x_add` reads come back wrong, each line smears exactly like the
     screenshots.
   - Fix direction in Dozen: when the format lacks buffer support, lower typed buffer loads to raw
     `ByteAddressBuffer` loads.
2. **`vram_image` cross-queue use.** It is written on the async compute queue and sampled on the
   graphics queue, with `ALLOW_SIMULTANEOUS_ACCESS` and `COMMON` promotion.
   - Test: force Granite to a single queue. That needs a port-owned env hook; Granite reads
     `GRANITE_*` env vars, so grep `getenv` in `parallel-rdp-standalone/vulkan` first.
3. **Out-of-family view → null descriptor** (OnyxEMU `0011`, M-03).
   - Without relaxed casting, `scale_image` (MUTABLE UNORM/SRGB) is created TYPELESS. Every view
     must stay in the `R8G8B8A8` family.
   - Add a log in `dzn_image_view_prepare_*_desc` when a view format's typeless family differs
     from the resource's.
4. **Integer texture sampling.** OnyxEMU `0011` also lowers integer-texture *sampling* to `txf`.
   The VI shaders already use `texelFetch` only, so this is low.

### 8.3 Blit / swapchain / WSI wrong

- If `scanout.bmp` is clean but the screen is not, look at the blit in `interface.cpp`
  `render_frame` and the Dozen WSI for CoreWindow (`need_swapchain_blits = true`: Dozen copies its
  own image into the DXGI back buffer).
- Compare with the OSD. If the OSD text is also scrambled, the swapchain copy or present is at
  fault. If the OSD is clean, the blit of `scale_image` (sampled `R8G8B8A8_UNORM`, MUTABLE →
  TYPELESS) is at fault. Check F1 coverage (`G64-DZN-WARN`) and the SRV format.

## 9. After the fix: cleanup list (do not forget)

- Remove the shortcuts once the Xbox image is right:
  - the always-on FPS OSD hook in `parallel-rdp/interface.cpp`;
  - the VI-probe hooks there (`g64_uwp_vi_probe`, `g64_uwp_vi_note_register`) and their code in
    `uwp/compat/g64_diag.*`, or keep them behind `G64_VI_*` env only;
  - the PC `legacycaps` default in `uwp/host/main.cpp`. Keep it only if the user wants permanent
    PC parity.
- Update the hook-inventory row for `parallel-rdp/interface.cpp` in
  [00](00-upstream-fidelity-rules.md) every time a hook is added or removed.
- Mark M-03 and M-11 in [10](10-roadmap-checklist.md) and [04](04-vulkan-dozen.md) with the
  actual finding.
- Write the upstreaming proposal (Mesa) as text only.

## 10. References

Local:
- `../mesa-uwp`, branch `gopher64/dzn-xbox`:
  - `src/microsoft/vulkan/dzn_image.c`: `dzn_image_layout_to_state`, `dzn_vk_layout_to_d3d_layout`
    and the SRV/UAV/RTV `prepare_*_desc` functions;
  - `dzn_cmd_buffer.c`: the legacy barriers at `:196-251`, `:1144-1163` and `:2213-2237`, and
    `enhanced_barriers` at `:678`;
  - `dzn_device.c`: `dzn_physical_device_cache_caps`, the `G64-DZN-*` logs, and the overrides;
  - `src/util/macros.h:131-136` (`UNREACHABLE`).
- `parallel-rdp/parallel-rdp-standalone/parallel-rdp`: `video_interface.cpp`, `video_interface.hpp`
  (`ScanoutOptions`), and `shaders/extract_vram.comp`, `vi_fetch.frag`, `vi_divot.frag`,
  `vi_scale.frag`.
- `parallel-rdp/parallel-rdp-standalone/vulkan`: `context.cpp` (sync2), `render_pass.cpp`
  (sync2 layouts), and `device.cpp` (`FORCE_ARRAY`, `MUTABLE_SRGB`).
- gopher64: `parallel-rdp/interface.cpp` (`render_frame`, `rdp_set_vi_register`),
  `uwp/compat/g64_diag.cpp` (VI probe), `uwp/compat/dozen_loader.cpp` (`DZN_DEBUG` composition),
  and `uwp/host/main.cpp` (PC parity, `G64_LOCALSTATE`).

External prior art (read for ideas; reimplement and check licences):
- **OnyxEMU** (`oledz-i/OnyxEMU`, GPL-3.0), `patches/mesa/`:
  - `0011`: no relaxed casting on Xbox → null descriptors for out-of-family views; integer-texture
    sampling → `txf`; `DENY_SHADER_RESOURCE` only with depth;
  - `0014`: WRITE_BACK invisible to the GPU (our M-01);
  - `0015`: D16 typeless and `AlphaBlendFactor`;
  - `0034`: the console lacks typed UAV loads for some formats and relaxed casting.
- **Azahar UWP**: `externals/uwp/mesa/azahar-xbox-uav-cast.patch` (typed store-only UAV for
  RGBA8-family typeless resources). `STATUS.md` notes that R32_UINT storage views of RGBA8 images
  are null on Xbox.
- **`rodrigoandrigo/Mesa-UWP`** (no licence; ideas only): raw-image lowering for storage images
  without typed loads (`59652f206f`), later disabled by default (`a7dadb2c3e`). It also logs
  `enhanced_barriers` and `shader_model`.
- **Themaister/parallel-rdp #56**: paraLLEl-RDP on desktop Dozen worked once 8/16-bit storage was
  emulated.
- No public project runs paraLLEl-RDP on Xbox (searched 2026-11-12); we are first.
