# Cross-machine work journal

This is the shared handoff for agents working on the Xbox port. Read this file,
`README.md`, and the detailed [P2 implementation handoff](17-p2-implementation-handoff.md)
before continuing. Append an English dated entry after **each interaction**, including the machine
name from `$env:COMPUTERNAME` (PowerShell) or `hostname` (elsewhere): intent, changes or
decisions, evidence and failed attempts, tests with actual outcomes, and remaining work. Do not
include credentials, console addresses, commercial ROMs, or machine-specific absolute paths.
Commit the agent's own work in logical increments; do not stage unrelated user files or push
unless requested.

**Non-negotiable:** agents never connect to, deploy to, or launch on the user's Xbox. Only the
user performs console deployment or VS F5. Agent execution and automated tests stay on the PC.
Never uninstall the existing app merely to refresh a package: its `LocalState` can contain data.

## Machines and evidence

Identify the current host at the start of work with `$env:COMPUTERNAME` in PowerShell, or
`hostname` elsewhere. Use that name in new machine-specific notes, memories and handoff entries;
if it cannot be determined, say **unknown**. A separate `.env` is unnecessary for identification.
Tool availability and test results are dated observations, not guarantees; recheck when needed.
Agent/model names are not machine names and do not determine their abilities or access.

- **YODA:** current host identified by `COMPUTERNAME` on 2026-11-05. VS 2026 host builds were
  verified here; the user identifies it as the Xbox-access environment. Only the user may deploy
  to or launch the Xbox, including VS F5.
- **YODA storage, 2026-11-05:** repository `out` and `target` are junctions to H:. Full pre-switch
  originals remain in the `gopher64-build-backup-20261105` directory on F:. Do not delete them
  without user approval; they still occupy F: space. H: must be available to build.
- **W11-4NSYSB4** (the user calls it "Opus"; identified by `hostname` on 2026-11-09): the 2026-11-05
  Opus entries below were written here. VS 18 Build Tools with the WindowsStore v180 toolset,
  meson-uwp and Python 3.14 are present; on 2026-11-09 it configured and built Mesa Dozen. The
  ImGui download failed on 2026-11-05 and a complete host MSIX build has not been established here.
  No Xbox access. The `../mesa-uwp` checkout here has `origin` = `marcelofrau/mesa-uwp` (the fork).

Older entries using "this PC", "Opus machine", "console machine" or agent labels are historical
context, not reliable host names. Do not assign them to YODA or another name without evidence.
Record future checks with the host name and distinguish PC evidence from user-performed Xbox tests.

**Compaction policy (2026-11-12).** Keep this file short. Once it grows past about 400 lines, move
entries older than the two most recent working days into `handoff-archive/`. Move them verbatim and
replace absolute paths with relative descriptions. Then update **Current state** and **History**
below. Never delete history.

## Current state (2026-11-12)

- **PC (YODA, NVIDIA RTX 4070 SUPER, UWP Release | x64):** SM64 runs correctly with the Mesa fork
  Dozen, using bindless + subgroups (`DZN_DEBUG=no16bit`, DLL `0D3AB0AE...`).
  - The Xbox-parity configuration `no16bit,nobindless` (DLL `3F06E897...`, commit `797c123548e`) has
    **not** been run on PC yet.
  - The `legacycaps` parity from `781f4d9` / Mesa `d0b7c8827ab` is also untested on PC.
- **Xbox (`SraKmd_arden`):** boot works; all shaders compile; it runs at 60 VI/s; XAudio2 audio
  plays.
  - **Open blocker:** the image is deterministically distorted. Vertical placement is right, but
    each scanline is scrambled horizontally into coloured bands.
  - The full investigation plan is [23 — Xbox visual distortion plan](23-xbox-visual-distortion-plan.md).
- **Mesa fork:** `marcelofrau/mesa-uwp`, branch `gopher64/dzn-xbox`.
  - Published up to `797c123548e`.
  - `d0b7c8827ab` (`G64-DZN-CAPS` log + `DZN_DEBUG=legacycaps`) exists only on W11-4NSYSB4, waiting
    for the user to approve a push.
  - Build recipe: [22 §Reconstructing](22-dozen-storage-investigation.md#reconstructing-the-experimental-driver).
- **gopher64 `uwp/main`:** the latest local commits (`b9049ac`, `781f4d9`, plus this compaction) are
  not pushed yet.
- **Machine rules:**
  - W11-4NSYSB4 is corporate. Never launch the emulator, a ROM or any game window there; only builds
    and headless checks.
  - Only the user deploys to or runs anything on the Xbox.

## History (summaries; full entries in `handoff-archive/`)

- **2026-11-04 to 2026-11-05** ([archive](handoff-archive/2026-11-04-05.md)):
  - P2 host built: C++/SDL3/ImGui UWP host, MSIX, VS solution, LocalState, ROM picker.
  - CRT mismatch `0xc0000135` fixed.
  - Audio hang in `SDL_OpenAudioDeviceStream` solved with the XAudio2 backend (D-08).
  - DXIL A/B experiments; the ubershader improved the SM64 image on PC.
  - Gamepad investigation; the root `Gopher64UWP.slnx` was consolidated.
  - Rendering research, cited by [19](19-rendering-bringup-xbox.md).
- **2026-11-06** ([archive](handoff-archive/2026-11-06.md)):
  - First Xbox runs. RDP refused to start (`16-bit storage for SSBOs is not supported`). D3D12
    `OPTIONS4 native16=0` confirmed on Xbox.
  - YODA wrote a local 25.3.3 storage-emulation Dozen.
  - Fixed DXIL validation (PSV wave size); selected the non-subgroup RDP variant.
  - The day ended with an Xbox black screen / hang after two compute compiles.
- **2026-11-09, W11-4NSYSB4** ([archive](handoff-archive/2026-11-09.md)):
  - Created the Mesa fork series on 26.2.2: logger, `no16bit`, narrow storage, PSV guard, M-01.
  - Added gopher64 diagnostics: R-02 self-test, heartbeat, watchdog.
  - Unified `gopher64-host.log`: stdio capture, crash filter, `env.txt`.
- **2026-11-11, YODA** ([archive](handoff-archive/2026-11-11.md)):
  - Root-caused and fixed three Dozen bugs:
    - an MSVC `/O2` miscompile of `nir_remove_dead_variables`;
    - `merge_ubos_and_ssbos` aliasing and a null `var`;
    - an `optimize_nir` infinite loop from `support_16bit_alu` (this was the Xbox black-screen
      hang).
  - Added the Xbox UMA `WRITE_COMBINE` / `DISCRETE_GPU` change; it had no effect on the distortion.
  - First Xbox audio and 60 VI/s. The distortion remained, pixel-identical across DLLs.
  - The headless `rdp_scanout_probe` and the `sm64.rdp` trace are on YODA scratch storage.

## Recent entries

## 2026-11-12 - W11-4NSYSB4: review of YODA's 2026-11-11 work, Xbox distortion triage

- Read-only review on `W11-4NSYSB4` of commits `62ffa30..d34e03c` and the YODA entries above.
  Fetched `marcelofrau/mesa-uwp`; `origin/gopher64/dzn-xbox` is at `797c123548e` (the local
  checkout here is still at `364c8b1e0a9`; not updated). `out/uwp-dozen/` does not exist here.
- Gaps found in the 2026-11-11 evidence:
  - The headless `rdp_scanout_probe` used two flat colours (`top`/`bot` samples). A flat image
    cannot reveal horizontal scrambling, so it does not clear the VI path. Rerun it with a
    per-pixel X/Y gradient and compare every pixel.
  - `DZN_DEBUG=no16bit,nobindless` (`1b6d9c4`, DLL `3F06E897...`) has only run on Xbox. The PC
    success used bindless + subgroups. PC F5 with the current build is still the first
    discriminator.
- Triage notes for the next run:
  - The screenshots show correct vertical placement and per-scanline horizontal smearing. That
    fits `vi_scale.frag` reading wrong `x_start`/`x_add` from `uHorizontalInfo` (an
    `R32G32B32A32_SINT` texel buffer). Non-bindless Dozen binds that as a typed SRV buffer,
    unlike the bindless PC path.
  - Other candidates: the `R8G8B8A8_UINT` UAV/SRV pair between `extract_vram` and `vi_fetch`
    (M-03), and the 2-layer `uFetchCache`/`uDivotOutput` render targets.
  - Check whether the OSD text (`create_message_image`) renders cleanly on Xbox. If it does,
    the final blit and swapchain path are fine.
- No build, PC launch or Xbox action was run. No code changed.

## 2026-11-12 - W11-4NSYSB4: always-on FPS OSD, D3D12 caps log, PC legacy-caps parity

- New lead. Dozen chooses different code paths from D3D12 caps:
  - `options12.EnhancedBarriersSupported`: barrier model (`dzn_cmd_buffer.c:678`).
  - `options12.RelaxedFormatCastingSupported`: typeless `MUTABLE_FORMAT` images (`dzn_image.c:76,207`).
  - `options3.CastingFullyTypedFormatSupported`: view casting (`dzn_image.c:750`).
  - `options13.UnrestrictedBufferTextureCopyPitchSupported`: copy pitch (`dzn_device.c:3287`).
- A modern PC reports all four as 1. The Xbox UWP runtime is assumed, but not proven, to report
  0, so PC and Xbox would run different paths through the same DLL.
- `../mesa-uwp` `gopher64/dzn-xbox` commit `d0b7c8827ab`, local and not pushed:
  - Logs one raw `G64-DZN-CAPS:` line per physical device to stderr, before any override.
  - Adds `DZN_DEBUG=legacycaps`, which clears those four caps.
  - Built here: `out/mesa-build/src/microsoft/vulkan/vulkan_dzn.dll`, SHA-256
    `903BCDDEEFFBD4A4E98577E4C04AE76979BC28136603A2BA417DA6804207DCBB`.
  - `vulkaninfo --summary` with this DLL printed the line for NVIDIA RTX 3500 Ada, Intel Arc and
    WARP. All showed `enhanced_barriers=1 relaxed_casting=1 unrestricted_pitch=1 cast_fully_typed=1`
    and `sm=0x68`, and `legacycaps=1` parsed.
- gopher64 changes:
  - `parallel-rdp/interface.cpp`: a `GOPHER64_UWP` hook sets `display_fps = true` in `rdp_init`,
    because the Xbox pad has no F1.
  - `uwp/host/main.cpp`: on a non-`Windows.Xbox` device family, sets `DZN_DEBUG=legacycaps`
    before `env.txt` is applied.
  - `uwp/compat/dozen_loader.cpp`: appends any existing `DZN_DEBUG` to `no16bit,nobindless`
    instead of replacing it, and logs the final string. On Xbox, `env.txt` can add flags this way.
- Verification here: Dozen MSBuild Release passed; the UWP staticlib `cargo rustc` passed (it
  compiles `interface.cpp` and `dozen_loader.cpp`).
- Not verified here: the change to `uwp/host/main.cpp`, because the host MSIX build is not set up
  on this machine. No gopher64 run and no Xbox action.
- Next, on YODA (by the user):
  1. Pull both repos and rebuild `vulkan_dzn.dll` from `d0b7c8827ab` (doc 22 recipe).
  2. Run PC F5. Expect `PC parity: DZN_DEBUG=legacycaps`, `DZN_DEBUG=no16bit,nobindless,legacycaps`
     and `legacycaps=1`. Check whether SM64 distorts like the Xbox.
  3. Run on Xbox. Record the `G64-DZN-CAPS` line for `SraKmd_arden`, and whether the FPS text
     renders cleanly.
- Reading the results:
  - PC distorts with `legacycaps`: the bug is in a Dozen legacy path. Bisect by clearing one cap at
    a time on PC.
  - The Xbox reports any of the four caps as 1: make `legacycaps` match the real Xbox values.
  - The FPS text is clean on Xbox: the final blit and swapchain path are fine.
- User rule (2026-11-12): on `W11-4NSYSB4` (corporate), never launch the emulator, a ROM or anything that opens a game window. Only builds and headless checks are allowed there.
