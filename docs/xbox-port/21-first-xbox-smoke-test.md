# First Xbox smoke test

## Status

On 2026-11-05, the user observed correct-looking Super Mario 64 frames and 30 FPS / 60 VI/s
on YODA in a PC UWP Release test. The user later reported that the audio issue was resolved.
The newer gamepad changes and final consolidated build have **not** been tested with a real
controller. No part of this application has yet been run on Xbox.

The agent must not access the console, deploy to it, or launch it. All Xbox steps below are manual
actions for the user.

## One primary solution

The generated `Gopher64UWP.slnx` printed by `build-host.ps1` is the primary solution. If `out` is a
junction, open it from the physical build drive, not through the junction: AppX registration can
reject the latter as an untrusted mount point. Do not copy the generated solution to the repo root;
its project and package paths are generated together.

To rebuild and open the primary solution on a configured PC:

```powershell
pwsh -NoProfile -File scripts/uwp/open-vs.ps1 -SkipDependencies
```

This builds Release by default and opens Visual Studio on the PC only. In Visual Studio explicitly
select **Release | x64**, because its selected configuration is remembered independently. F5
builds the matching Rust library before linking; configure VS to build out-of-date projects and not
launch when build or deployment fails. The alternative A/B solutions are historical, not the
primary workflow; do not delete them without the user's approval.
The first F5 still needs to deploy the Release layout. An older Debug layout may contain the old
DXIL and must not be used as evidence of what the Release package contains.

## Dozen and DXIL inputs

The host uses a local directory containing `vulkan_dzn.dll` and `dxil.dll`. Selection order is an
explicit `-DozenDir` to `build-host.ps1` or `GOPHER64_DOZEN_DIR`, then a complete local `uwp-dozen`
directory beside the generated host build, then the historical RetroArch source as a warned
fallback. An incomplete local pair is reported and skipped. The build prints
the selected directory and both SHA-256 hashes, and fails if a selected file is missing.

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
