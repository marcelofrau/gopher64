# P3 Dozen integration — active work plan

Status: implementation in progress; P3 is not complete. The user authorized this work on
2026-11-04. All agent-run deployment and runtime tests are PC-only; the user alone operates Xbox.

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
