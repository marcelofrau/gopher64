# First Xbox smoke test

## Status

On 2026-11-05, the user observed correct-looking Super Mario 64 frames and 30 FPS / 60 VI/s
on YODA in a PC UWP Release test. The user later reported that the audio issue was resolved.
The newer gamepad changes and final consolidated build have **not** been tested with a real
controller. No part of this application has yet been run on Xbox.

The agent must not access the console, deploy to it, or launch it. All Xbox steps below are manual
actions for the user.

## One primary solution

Open the **tracked `Gopher64UWP.slnx` at the repository root**. It references the tracked
`uwp/gopher64-uwp.vcxproj`; neither file needs generating or contains a machine-specific drive.
On YODA the canonical path is `F:\workspace\gopher64\Gopher64UWP.slnx`.

A normal clone uses ordinary `out/` and `target/` directories. YODA's junctions to H: are optional
local storage choices, not prerequisites. MSBuild calls the matching Rust build before compiling
and linking the host. A failed Rust build stops the host; VS must be configured not to launch an
older executable after a build or deployment error.

For a fresh checkout, install the prerequisites from [15](15-local-build-debug-workflow-plan.md),
initialize the submodules, run `scripts/uwp/build-deps.ps1`, and provide the Dozen/DXIL pair below.
The host build fetches the same hash-pinned ImGui sources used by the earlier CMake host and
prepares its local manifest and development certificate. The solution/project themselves are
already present. Do not copy build caches to a new PC. Existing junctions copied with an entire
working directory still need to point to valid local storage; scripts do not replace them.

The host output is `out/uwp-host/`. Its physical deployment path is resolved for AppX;
this keeps junctions usable without hard-coded drives or changes to Windows security. Junctions
inside the configuration/AppX layout are rejected rather than silently traversed.

On YODA, a temporary PC package with its own identity registered through the physical path but
failed with `0x800701C0` through both the checkout's `out` junction and a new non-elevated junction.
The temporary package was removed, and no security setting was changed. This is consistent with
[Microsoft's redirection-trust policy](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-process-mitigation-redirection-trust-policy).
The tracked project builds and generates the intended VS deployment recipe;
its first VS F5 deployment still needs user verification.

Double-click `Gopher64UWP.slnx`. Optionally, to rebuild and open it from a terminal:

```powershell
pwsh -NoProfile -File scripts/uwp/open-vs.ps1 -SkipDependencies
```

The optional script builds Release by default and opens Visual Studio on the PC only. In Visual Studio explicitly
select **Release | x64**, because its selected configuration is remembered independently. F5
builds the matching Rust library before linking; configure VS to build out-of-date projects and not
launch when build or deployment fails. The alternative A/B solutions are historical, not the
primary workflow; do not delete them without the user's approval. This includes the former
`.uwp-vs` solutions: the root solution supersedes them. No regeneration is needed for normal use.
The first F5 still needs to deploy the Release layout. Older solutions and their layouts may contain
the old DXIL and must not be used as evidence of what the new Release package contains.

## Dozen and DXIL inputs

Place `vulkan_dzn.dll` and `dxil.dll` in `out/uwp-dozen/`, or set `GOPHER64_DOZEN_DIR` before starting
Visual Studio. Both files are required. The optional `build-host.ps1 -DozenDir ...` also supports
an explicit one-build override and the historical warned RetroArch fallback. For consistent direct
F5 builds, use the local pair or environment variable rather than relying on that fallback.

The PC test used Dozen 25.3.3 and a Microsoft-signed SDK DXIL validator 1.9.2602.17:

| File | Tested SHA-256 |
|---|---|
| `vulkan_dzn.dll` | `972C65469440AF3C7834A84CD2B55C87457E7DAA2E1BC660AC26180C01C2D272` |
| `dxil.dll` | `DB108FFEF7BCECB984BDCCF2B4725719D65B2CE7F1EBC40019DBE6C5A7016E09` |

These binaries are local inputs, not committed to Git. The previous DXIL version caused an
unhandled validator exception on this PC. The newer one eliminated that exception in the PC test;
its Xbox compatibility and redistribution rights remain unverified. A different machine must
provide its own pair and verify its hashes before assuming it reproduces the test. A nonmatching
pair may be tested intentionally, but is not the validated PC combination.

The generated MSIX contains both DLLs and an x64 VCLibs dependency; the module-level import audit
passed. This is not API-level Store certification, a warranty of console compatibility, or proof
that the development certificate is trusted on a console.
The build also emits `APPX0006` for the manifest's `runFullTrust` capability. Its effect on the
first console deployment remains unverified; capture any deployment error rather than suppressing it.

## Manual console run

1. The user starts Xbox Developer Mode, prepares the console connection in Visual Studio, and
   selects the console as the deployment/debug target. Follow the [Microsoft Xbox UWP setup
   instructions](https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/getting-started) for the
   current Visual Studio UI; the agent does not connect to the device.
2. Select **Release | x64**, start with F5, and load the same Mario 64 ROM when prompted. Keep the
   Output window open. The first shader compilations may cause temporary pauses; measure only after
   they settle.
3. Check that the frontend appears, the ROM loads, the Mario logo/menu renders correctly, and the
   audio plays without pitch changes or periodic stutters. Record FPS and VI/s without assuming the
   console will match the PC.
4. With the gamepad connected, test Menu/Start, analog stick, and buttons. In the default profile,
   Xbox A maps to N64 A and Xbox X to N64 B. Also test disconnect/reconnect if the first run works.
5. On a failure, capture the full Output and any exception stack, the last on-screen state, and
   whether it happened on the PC or Xbox. Do not treat successful package registration as proof of
   successful game rendering or audio.

## Useful log markers

- `input: detected ...` reports SDL enumeration; `automatically opened gamepad` reports player 1
  auto-binding. Auto-binding is intentionally disabled if any controller assignment is saved.
- `input: gamepad Start pressed; N64 Start=true` shows that Start reached the mapping. A detected
  gamepad alone does not prove that the emulated game received its buttons.
- `rdp_init succeeded` only confirms graphics initialization. A first visible game frame is a
  separate milestone. A `Stalled compile` warning ending in `success: yes` reports compilation time,
  not by itself a failed shader.
- `audio: input=... voice=...` reports input/output rates and five-second aggregate underrun,
  overflow and submission counters. The first interval can include startup; Rust-side dropped
  packets are not counted by these shim counters.

Keep the full log for diagnosis. The PC experiment does not validate the Xbox's CoreWindow/WSI,
shader validator, runtime API availability, gamepad handling, audio or performance.
