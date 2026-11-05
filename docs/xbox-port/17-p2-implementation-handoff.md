# P2 implementation handoff

**Status: in progress; P2 exit criteria not met.** The host, package, and scripts exist and have
been exercised on a Windows PC. No Xbox installation, tile launch, F5 session, `rdp_init`, or game
rendering has been verified. This is a development package, not a release.

> This file records the earlier P2 baseline. The later experimental P3 Dozen integration reaches
> `rdp_init` on a PC, then hangs opening the SDL audio stream; see the current
> [work journal](memory-handoff.md) and [P3 plan](18-dozen-p3-work-plan.md). No game frame or Xbox
> execution has been verified.

## What exists

- `uwp/CMakeLists.txt`, `uwp/host/main.cpp` and `uwp/Package.appxmanifest.in` build a WindowsStore
  x64 C++ executable linked to the Rust staticlib, SDL3 and SDL_ttf. The package includes both SDL
  DLLs, assets derived from the existing project icon, and a minimal Dear ImGui status screen.
  ImGui is fetched from the same commit used by Ymir and checked against a SHA-256 hash.
- The host uses `LocalState` for the existing Rust config/data/cache paths. It logs host and explicitly
  instrumented Rust entry/video messages with timestamps to `LocalState/gopher64-host.log` and the
  debugger. This is **not yet capture of every SDL/RDP/native or Rust stderr line**.
- Without autoboot, the host opens the native WinRT ROM picker at startup and offers retry after
  cancellation or failure. It replaces the fixed `LocalState/gopher64-staging/gopher64-staged.rom`
  before passing its path to Rust; the original is not modified. This is one staging slot, not a
  persistent ROM library; an access-token/handle design is future work.
- An optional UTF-8 `LocalState/autoboot.txt` contains one absolute ROM path. No ROM is packaged.
  The host presents its status screen, tears down its SDL/ImGui resources, calls the existing Rust
  entry synchronously on that thread, and attempts to restore the status screen after return. It
  displays the Rust entry error there. Full game → menu lifecycle is not yet proven.
- `build-host.ps1` generates a VS solution and MSIX, calls the Rust build unless `-SkipRust` is
  requested, and runs an import-module audit against the **packaged** EXE and DLLs. The build uses a
  changing development package version so Windows can update the existing identity without uninstalling
  or deleting `LocalState`. `-PackageVersion` overrides it if a specific version is needed.
- `install-pc.ps1`, `open-vs.ps1`, `test-pc-probe.ps1`, and `test-pc-rom.ps1` cover the PC workflow.
  The user alone deploys to and launches on Xbox; no automated Xbox deployment script is provided.

## Results on this PC — 2026-11-04

| Check | Result and limit |
|---|---|
| VS 2026 Release and Debug builds | Both host configurations linked and generated MSIX. Build output included existing `parallel-rdp` deprecated CRT/signedness warnings, Debug `LNK4075`, and manifest `APPX0006` for `runFullTrust`. |
| PC installation | Development MSIX installed with existing trusted CMake test certificate and existing VCLibs; no certificate store or VCLibs changes were needed. `install-pc.ps1` was exercised. |
| ImGui status | SDL selected `direct3d11`; a status frame was presented. A PC VS F5 screenshot confirms the minimal status screen, not final visual QA. |
| Missing ROM | `test-pc-probe.ps1` presented a frame, invoked Rust, received an error rather than abort, and exited cleanly. |
| Real ROM autoboot | A temporary copy of Super Mario 64 was loaded from LocalState. SDL WinRT reported `Vulkan support is not configured in SDL`; the Rust error returned to the host, and a restored status frame was presented. `rdp_init` was **not reached**. The test deliberately exits nonzero for this unmet criterion. |
| Import audit | EXE, SDL3 and SDL_ttf import tables recorded; selected desktop modules and non-Store CRT DLLs not found. `ntdll.dll`, `crypt32.dll`, and `kernel32.dll` are present. Module names alone do **not** establish supported APIs; dynamic imports and Xbox availability remain unverified. |
| Desktop regression smoke | In a separate portable directory with a copied ROM, desktop Release initialized Vulkan on the RTX 4070 SUPER, created a 640×480 swapchain, stayed alive for 12 seconds and exited normally after a window-close request. No visual gameplay or real save/load was verified. |
| VS 2026 | A user F5 on PC initially failed with missing Debug CRT dependencies. After the fix, the user confirmed the status screen via PC F5. Xbox remote debugging remains unverified. |
| Rust tests | `cargo test --lib --locked`: 8 passed, covering ROM validation, raw/ZIP/7z input, config-write errors, RDRAM alignment and legacy postcard serialization. These do not prove Xbox gameplay. |
| Formatting and lint | `cargo fmt --all -- --check` and both Clippy configurations (`cargo clippy -- -Dwarnings` and `--no-default-features`) passed after fixing two new lint findings. Native C/C++ warnings remain. |

### PC F5 failure: Debug SDL with Release VCLibs (2026-11-04)

The user's VS local deployment exited with `0xc0000135` before host logging. `dumpbin` showed
`SDL3.dll` importing `vccorlib140d_app.dll`, `msvcp140d_app.dll`, `vcruntime140d_app.dll`,
`vcruntime140_1d_app.dll` and `ucrtbased.dll`; `SDL3_ttf.dll` also imported Debug CRT DLLs.
The package declared only `Microsoft.VCLibs.140.00`. In a separately registered temporary PC
package using the same binaries, adding `Microsoft.VCLibs.140.00.Debug` allowed the host to
present its first frame, proving the mismatch. The temporary registration was removed.

The normal build now uses Release SDL dependencies for both host configurations, matching the
host's `MultiThreadedDLL` Store CRT; Rust and host still use their selected Debug/Release profile.
The package import audit rejects Debug CRT imports, and `scripts/uwp/tests/audit-imports.tests.ps1`
checks both positive and negative cases. `build-host.ps1` also refreshes SDL DLLs in an existing
local VS `AppX` layout: installing an older-timestamp Release build otherwise left stale Debug
DLLs in that layout despite the newly generated MSIX being correct. On PC, the original registered
app then passed `test-pc-probe.ps1` and logged `First status frame presented` and the expected
missing-ROM error from Rust. This is not a new VS F5 or Xbox test; those remain user-operated.

## Diagnoses and fixes worth retaining

1. The original host died during its first `SDL_PollEvent`. CDB captured a WinRT
   `AccessDeniedException` in `SDL_winrtvideo.cpp:810`, querying `CoreWindow->PointerPosition` from a
   visibility callback. Running past the exception ended with `0xc000027b`. An isolated SDL copy that
   catches **only** `Platform::AccessDeniedException`, retains the prior mouse-focus flag and logs a
   warning advanced to the first presented frame. `scripts/uwp/SDL3-pointer-access.patch` is applied
   and reverse-checked on a generated, version-keyed copy of the clean `SDL3-uwp` Git worktree;
   the sibling repository is never modified. The build refuses a dirty source tree or a patch that
   does not apply instead of silently omitting changes. This fix is verified on PC, not Xbox.
2. The first automated Rust probe found a panic on a missing ROM. Shared ROM reading now reports an
   error for missing/unreadable files, invalid or short ROMs, and malformed archives instead of using
   unchecked `unwrap`/slices. Desktop unit tests cover missing paths, short data, byte order and
   malformed swapped lengths.
3. Returning from a real-ROM startup failure crashed in `RtlpHeapHandleError`. CDB with Debug symbols
   identified an 8 MiB RDRAM `Vec<u8>` being dropped. The old code allocated with 64 KiB alignment
   but handed that pointer to a `Vec<u8>`, whose deallocation layout has byte alignment; this violates
   the `Vec::from_raw_parts` contract. `AlignedRdram` now retains the original ordinary `Vec`
   allocation and exposes a 64 KiB-aligned slice inside it. Tests check 4/8 MiB alignment, clone and
   pointer-stable clone-from, and compatibility with the previous postcard representation. The same
   real-ROM error path then returned without the heap failure. This does not substitute for gameplay
   and save/load verification on actual targets.
4. An early automation attempt ran `git apply` inside a generated directory nested in this repository;
   Git returned success while skipping the intended file. The build now applies from the repository
   root with the generated path explicitly prefixed and checks the reverse patch. Do not remove that
   verification or treat a successful `git apply` exit alone as evidence.
5. Windows rejected a changed MSIX with the same identity and version. The build now generates a new
   development version. Do not uninstall the existing app merely to get around this: uninstalling can
   delete its `LocalState`.

## Commands

Run from the repository root in PowerShell 7. These use the already documented SDL3-uwp and SDL_ttf
workspaces and Rust prerequisites from [15](15-local-build-debug-workflow-plan.md).

The generated Visual Studio project builds the matching Rust static library before linking the host.
Cargo checks for changes incrementally on each build. Under **Tools → Options → Projects and Solutions
→ Build and Run**, set **On Run, when projects are out of date** to **Always build**, and **On Run,
when build or deployment errors occur** to **Do not launch**. Otherwise VS can run an old binary.
Regenerate older solutions with `open-vs.ps1` before relying on this. A command-line build makes the
MSIX but does not deploy it or necessarily refresh the local `Debug/AppX` layout; VS F5 deploys it.
`-SkipDependencies` reuses existing SDL dependencies. The explicit `build-host.ps1 -SkipRust`
option bypasses Rust and can use a stale library; never use it after changing core sources.
If the repository's `out` directory is a junction or symlink, the scripts use its resolved target
and generate a separate `uwp-host-vs<version>-direct` solution. Open that solution by the physical
path printed by the script: AppX deployment can reject a layout reached through an untrusted
junction. Existing generated solutions are preserved, but still use their original paths.

```powershell
# Full Release build, including SDL and Rust; prints the exact MSIX path.
pwsh -NoProfile -File scripts/uwp/build-host.ps1 -InitSubmodules

# Build Debug and open the generated solution in the detected Visual Studio.
pwsh -NoProfile -File scripts/uwp/open-vs.ps1 -Configuration Debug

# Optional: regenerate and open Debug using already built SDL dependencies.
pwsh -NoProfile -File scripts/uwp/open-vs.ps1 -Configuration Debug -SkipDependencies

# Replace <msix> with the exact path printed by build-host; no certificate is installed by this script.
pwsh -NoProfile -File scripts/uwp/install-pc.ps1 -Package '<msix>'
pwsh -NoProfile -File scripts/uwp/test-pc-probe.ps1
pwsh -NoProfile -File scripts/uwp/test-pc-rom.ps1 -Rom '<your ROM path>'

```

The ROM test refuses to overwrite an existing `autoboot.txt` or test marker, uses a temporary copy,
and retains it if the app remains running. It creates/updates files in this development package's
LocalState through normal app startup; do not treat its sandbox as a disposable copy of other apps'
data. The test currently reports failure because it does not reach `rdp_init`.

The PC installer uses an already-installed VCLibs dependency by default. `-InstallDependencies` is an
explicit opt-in because updating VCLibs can require other running apps to close. The target must trust
the signing certificate; the PC installer does not install or trust certificates. The default CMake test
certificate/identity is for local development only and must not be used for a public release.

## Remaining before calling P2 complete

- Resolve the SDL WinRT `SDL_WINDOW_VULKAN`/Dozen integration without pretending the PC ROM test reached
  `rdp_init`. This intersects the P3 graphics work; no real Vulkan surface or frame is proven here.
- The user must manually test the exact final MSIX and dependencies on Xbox, certificate trust, tile
  launch 10/10 and a separate VS 2026 remote-debug/F5 session. Agents must not connect to, deploy to,
  or launch on the user's Xbox. This workflow is not yet console-verified.
- Audit individual imported APIs with Windows App Certification Kit and investigate `ntdll`/`crypt32`,
  `APPX0006`, dynamic library loading, and Xbox API availability. `audit-imports.ps1` is a module
  screening aid, **not** Store certification. WACK was not run: Microsoft's CLI procedure requires an
  active administrator session and `appcert reset`, which can affect existing kit state.
- Verify foreground/background lifecycle, Game-mode classification, memory and stack questions, audio,
  controller navigation, persistent game saves and menu → game → menu on real targets. Audio's proven
  Ymir Xbox deadlock and XAudio2 direction are recorded in [16](16-p2-host-frontend-plan.md).
- Extend diagnostics to native SDL/RDP and other Rust stderr, not just the explicit host/Rust callback;
  make storage and fatal startup errors visible even if the status renderer cannot initialize.
