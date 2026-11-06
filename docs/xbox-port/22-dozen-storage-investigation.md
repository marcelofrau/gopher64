# Dozen storage feature investigation

## Observed on Xbox, 2026-11-06

- The frontend presented, staged the ROM and transitioned into the Dozen renderer.
- On the selected `SraKmd_arden` Vulkan 1.2.328 GPU, Dozen 25.3.3 reported
  `raw16=0 raw8=0 shaderInt16=0 granite16=0 granite8=0`.
- paraLLEl-RDP stopped at its explicit 16-bit SSBO storage requirement. The raw Vulkan and Granite
  results agree; this is not evidence that Granite lost an advertised feature.
- A direct D3D12 query on the same adapter succeeded: `OPTIONS4 hr=0x00000000 native16=0`.
- Audio, gameplay, gamepad and a rendered game frame on Xbox remain unverified.

## What this establishes

The D3D12 query succeeded, and the Xbox UWP driver reports native 16-bit operations unavailable.
Dozen's Vulkan report matches it. This establishes a limitation of the tested UWP driver interface,
not what the physical silicon could do outside that interface. Do not bypass the renderer check or
claim unsupported Vulkan features.

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
- The local Mesa UWP source checkout is on 26.2.0, with a 25.3.3 remote branch available; neither
  was modified. In both, `dzn_pipeline.c` requires native16 to be absent **and** at least one of the
  enabled `KHR_shader_float16_int8` extension, `shaderFloat16`, or `shaderInt16` features. The log
  reports raw `shaderInt16=0`, but does not establish the other two enabled values or the condition's
  final result in this run.
- The reference's patch changes that condition when native16 is absent; the compiler's lowering pass
  then targets 32-bit memory operations. Narrow SSBO stores may become an atomic AND followed by OR.
  Correctness under concurrent access, aliasing and descriptor boundaries must be established; a
  successful arithmetic conversion alone is not enough.

## Incremental plan

1. **Done:** query D3D12 `OPTIONS4` and its HRESULT on the adapter identified by the Vulkan LUID.
   It succeeded with native16 false. The probe released its COM references; no capabilities were
   changed. [Microsoft documents](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)
   reuse of an existing device for the same adapter in a process.
2. Build an **isolated experimental** Dozen from Mesa UWP source without altering the existing
   checkout or known PC-working DLLs. Reuse the proven lowering condition as a starting point.
   A private test build may expose candidate features only to exercise the implementation; it must
   not be shipped as supported before semantics are validated. The packaged binary's exact source
   and build provenance remain unverified.
3. Before testing the emulator on Xbox, compare 8-/16-bit reads/writes against native storage on PC:
   neighbouring-byte concurrency, boundaries, unaligned accesses, aliasing through 32-bit views and
   representative RDP shader layouts. Measure the lowering cost and surface failures explicitly.
4. Only after those checks, ask the user to run the experimental build on Xbox and capture capability,
   shader compilation, first frame, correctness and performance. The user alone deploys/runs there.

No step above authorizes disabling validation, silently falling back to another renderer, modifying
the console, or treating a reported capability as successful gameplay.
