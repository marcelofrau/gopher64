# Dozen storage feature investigation

## Observed on Xbox, 2026-11-06

- The frontend presented, staged the ROM and transitioned into the Dozen renderer.
- On the selected `SraKmd_arden` Vulkan 1.2.328 GPU, Dozen 25.3.3 reported
  `raw16=0 raw8=0 shaderInt16=0 granite16=0 granite8=0`.
- paraLLEl-RDP stopped at its explicit 16-bit SSBO storage requirement. The raw Vulkan and Granite
  results agree; this is not evidence that Granite lost an advertised feature.
- Audio, gameplay, gamepad and a rendered game frame on Xbox remain unverified.

## What this establishes

The Vulkan implementation used in this test does not advertise the required features. It does not
yet distinguish the D3D12 query failing from the driver successfully reporting that native 16-bit
operations are unavailable. Neither result proves what the physical silicon could do outside this
UWP driver. Do not bypass the renderer check or claim unsupported Vulkan features.

## Reference and working hypothesis

- [Mesa 25.3.3 Dozen](https://github.com/chaotic-cx/mesa-mirror/blob/mesa-25.3.3/src/microsoft/vulkan/dzn_device.c#L718)
  derives 16-bit storage from `Native16BitShaderOpsSupported`; its 8-bit storage additionally
  depends on a Dozen option. The packaged binary reports this version, but its source is unverified.
- [paraLLEl-RDP](https://github.com/Themaister/parallel-rdp) distinguishes arithmetic from storage:
  `SMALL_TYPES=0` does not remove its 8-/16-bit storage requirement.
- [smash4-xbox](https://github.com/jajdp/smash4-xbox/blob/main/patches/mesa-dozen-xbox.patch)
  reports a working Xbox Dozen port and fixes `lower_int16` for a GPU lacking native 16-bit ops.
  Its renderer differs and the patch does not expose 8-/16-bit Vulkan storage features; it is not
  evidence that paraLLEl-RDP can run unchanged.
- Mesa's [DXIL compiler](https://github.com/chaotic-cx/mesa-mirror/blob/mesa-25.3.3/src/microsoft/compiler/nir_to_dxil.c#L6247)
  contains a path to lower narrow memory accesses to 32 bits, including atomic treatment for
  unaligned stores. This is a plausible emulation mechanism, not proof of full Vulkan semantics.

## Incremental plan

1. Query D3D12 `OPTIONS4` and its HRESULT on the same adapter identified by the Vulkan device LUID.
   A successful query with native16 false supports a driver capability limit; a failed query needs
   separate investigation. Keep the existing renderer and features unchanged.
   [Microsoft documents](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)
   that a matching existing per-process D3D12 device is reused; the probe releases its COM references.
2. If emulation is needed, study a **separate** Dozen build that lowers narrow SSBO operations. Do
   not modify or replace the known PC-working pair; expose a feature only after the backend actually
   implements its semantics. The exact packaged binary's source and build provenance must be checked.
3. Before testing the emulator on Xbox, validate 8-/16-bit unaligned reads/writes, neighbouring-byte
   concurrency, bounds and representative RDP shader layouts against a reference on PC, using the
   smallest reproducible shader test. Include performance and failure reporting.
4. Only after those checks, ask the user to run the experimental build on Xbox and capture capability,
   shader compilation, first frame, correctness and performance. The user alone deploys/runs there.

No step above authorizes disabling validation, silently falling back to another renderer, modifying
the console, or treating a reported capability as successful gameplay.
