# P3 Dozen integration — active work plan

Status: implementation in progress; P3 is not complete. The user authorized this work on
2026-11-04. All agent-run deployment and runtime tests are PC-only; the user alone operates Xbox.

**2026-11-04 checkpoint:** steps 1–3 reached ICD/device and CoreWindow surface on PC; step 4
reached `rdp_init` but hung in `SDL_OpenAudioDeviceStream` before any game frame.

**2026-11-05 update:** on YODA, the user observed correct SM64 screens and 30 FPS / 60 VI/s in an
experimental UWP Release build, then reported audio fixed in a later run. This uses a local DXIL 1.9
DLL and the ubershader; it is
not a validated standard package or Xbox result. See the [work journal](memory-handoff.md) for
the failed attempts, exact component hashes, and remaining diagnostics and validation.

## Baselines and constraints

- The P0 desktop checkpoint used Dozen 26.2.4 and established the game/renderer baseline described
  in `14-p0-handoff.md`. It did not validate the UWP ICD or Xbox.
- The local RetroArch UWP package contains a 25.3.3 `vulkan_dzn.dll`, not the P0 driver. It exports
  the ICD negotiation/GIPA functions and contains the 8-bit storage configuration option. The
  sibling `mesa-uwp` source is 26.2.0. Treat this older binary as a diagnostic candidate, not a
  proven drop-in replacement; record exact versions, hashes, features, and limitations.
- Keep the working desktop/Android paths untouched; no changes inside Mesa/SDL submodules. Do not
  commit prebuilt driver binaries. Explicitly document where build inputs come from and their license.
- The CMake packaging warning `APPX0006` and API-level Store validation remain open.

## Incremental checks

1. **Package and probe:** make the UWP ICD/DXIL build inputs explicit, verify they are actually in
   the MSIX, and run a PC-only probe of packaged-library loading, ICD negotiation, instance/device
   enumeration, version, extensions, and the 8-bit storage features. Stop and surface any failure;
   do not claim the driver can render merely because it loaded.
2. **Core integration:** replace the SDL Vulkan loader path only for UWP with the verified ICD
   entry point; retain detailed errors and the P0-relevant configuration. Preserve desktop behavior.
3. **Window/surface:** create the UWP SDL window without its unsupported Vulkan flag and integrate
   the Mesa-UWP CoreWindow-backed WSI, grounded in the actual driver source and references. Verify
   the exact ICD's behavior, rather than assuming a dummy HWND is generally valid.
4. **PC game milestone:** with a real ROM, capture loader/instance/device/surface/`rdp_init` and
   first-frame evidence; separately record compatibility and feature gaps versus the P0 driver.
   Save-state coherency and repeated menu/game transitions remain separate checks.
5. **User-run Xbox:** only after PC evidence and at the user's discretion, provide build/package
   instructions and logs. No agent connection, deployment, execution, or data retrieval on Xbox.

The single-slot ROM staging change is tracked separately in `memory-handoff.md`; direct brokered
access and a persistent ROM library remain later work.

## Local candidate selection

`scripts/uwp/build-host.ps1` accepts `-DozenDir` or the `GOPHER64_DOZEN_DIR` environment variable.
If neither is set, it looks for the existing ScummVM/RetroArch UWP Mesa x64 directory in the
workspace. The directory must contain both `vulkan_dzn.dll` and `dxil.dll`; the build reports their
SHA-256 hashes and packages copies, never the originals. If no candidate is available, the host
build fails with instructions instead of silently packaging an app without its graphics driver.
The known local Mesa 25.3.3 binary is a diagnostic candidate, not an asserted match to the P0
26.2.4 desktop baseline.
