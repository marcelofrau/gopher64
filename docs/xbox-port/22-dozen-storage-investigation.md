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

### Latest result: black screen on Xbox, 2026-11-06

The user's last manual Xbox run selected the non-subgroup RDP variant (`Allow subgroups = 0`),
initialized the swapchain and RDP, opened one detected gamepad, initialized audio, and entered the
emulated CPU loop. Compute pipelines `2f444097f40d7813` and `eb43f456d4421d5b` compiled with
`success: yes`. The log ends immediately after the second compilation; the user saw a black window
with no visible progress and no debugger stop. These logs do **not** show a game frame, successful
audio playback, working controller buttons, a deadlock, or the cause of the black screen. Earlier
`_com_error` first-chance messages and system HRESULTs appear in the log; no causal link is proven.

The `PSVRuntimeInfo` validation mismatch and the immediate subgroup configuration error were passed
in this run, but correctness of the experimental storage emulation remains unproven. The earlier PC
run with the storage candidate produced an access violation in Dozen; its cause also remains open.
For the next investigator, first establish whether the CPU, GPU submission, first scanout, present,
or event pump is making progress, using bounded diagnostics and debugger thread stacks. These are
hypotheses to measure, not conclusions; do not make further assumptions or deploy to the console.

**2026-11-06 experiment:** at the user's request, proceed to an early Xbox test
after a small PC GPU smoke test rather than waiting for exhaustive conformance tests.
The candidate is experimental, not a conformant general-purpose Vulkan driver.
It uses Mesa UWP 25.3.3 at `f1ef4e6e645f364323b09f9cebbbde780347c5dc`. The candidate patch
is retained with the experimental source on YODA, not committed to gopher64: R4 requires Mesa
changes to live in a Mesa fork before integration.
The patch exposes only the narrow SSBO storage capabilities and forces the existing Mesa DXIL
lowering path on all GPUs for the experiment; it does not claim native 8/16-bit arithmetic.
On YODA, a real Vulkan compute shader passed against both the original and candidate drivers:
64 concurrent 8/16-bit writes and peer-thread reads, separate and aliased buffers, five runs each.
The alias case uses adjacent non-overlapping regions; it does not prove overlapping-access semantics.
Guard bytes stayed unchanged and deliberately wrong shaders were rejected. The shader did not
enable `shaderInt8` or `shaderInt16`; this limited smoke test is not Vulkan conformance.

1. **Done:** query D3D12 `OPTIONS4` and its HRESULT on the adapter identified by the Vulkan LUID.
   It succeeded with native16 false. The probe released its COM references; no capabilities were
   changed. [Microsoft documents](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice)
   reuse of an existing device for the same adapter in a process.
2. **Done:** build an experimental Dozen without modifying the existing Mesa checkout. The prior
   DLL is preserved in verified local and external backups; the candidate is in the host's input.
   A private test build may expose candidate features only to exercise the implementation; it must
   not be shipped as supported before semantics are validated. The original prebuilt binary's exact
   source and build provenance remain unverified.
3. **Partially done:** the smoke test compares 8-/16-bit accesses with the original driver.
   Boundaries, unaligned accesses, 32-bit aliasing, representative RDP shaders and performance
   remain unvalidated and must be revisited before a production claim.
4. **Partially done:** the user tested on Xbox; initialization and two shaders succeeded, but the
   window remained black. First frame, correctness, audio, input and performance remain unresolved.
   Only the user deploys/runs on the console.

No step above authorizes disabling validation, silently falling back to another renderer, modifying
the console, or treating a reported capability as successful gameplay.

## Reproducing the PC smoke test

The probe and shader are tracked under `scripts/uwp/tests/dozen-storage/`. They run on the PC only;
they do not deploy, register or launch the emulator. They require a Vulkan loader, x64 VCLibs,
the [UWP VS/CMake toolchain](17-p2-implementation-handoff.md) and `glslang` 16.6.0. Supply an existing
Dozen/DXIL directory:

```powershell
pwsh -NoProfile -File scripts/uwp/tests/dozen-storage.tests.ps1 `
  -DriverDirectory <dozen-directory> -Glslang <path-to-glslang.exe>
```
Run once with the original driver and once with the candidate; compare the printed GPU names.
It uses the first enumerated adapter, checks loaded Dozen/DXIL paths and hashes the selected files, repeats
separate/aliased cases five times and checks incorrect writes, reads and module paths. The tests do
not establish Vulkan conformance.
The YODA experiment used [Khronos glslang 16.6.0](https://github.com/KhronosGroup/glslang/releases/tag/16.6.0);
the Windows release archive SHA-256 was `82BF434E69B9BB4829DE7E2B4BC2C5E7A7861E53D66CF75E5CC70F5F694A8D9B`.

## Reconstructing the experimental driver

This is developer reference, not a second application solution. The patch, diagnostic source, and
DLL are local to YODA and are **not included in this repository or its remote**. They need a Mesa
fork in accordance with R4; the other machine cannot reproduce the exact build from this repo alone.

The tested candidate modifies Mesa's `dzn_device.c` to expose `KHR_8bit_storage`,
`KHR_16bit_storage`, `storageBuffer8BitAccess` and `storageBuffer16BitAccess` experimentally;
`dzn_pipeline.c` forces `lower_int16` to exercise 32-bit lowering; `nir_to_dxil.c` gates the PSV
fixed-wave fields on DXIL minor version >= 6, matching the metadata emission guard. Diagnostics in
`dzn_pipeline.c` and `dxil_validator.cpp` log the validation stage and HRESULTs. Unsupported native
narrow arithmetic was not forced on Xbox. These changes do not prove SSBO semantics or Vulkan conformance.

1. Archive Mesa UWP commit `f1ef4e6e645f364323b09f9cebbbde780347c5dc` into a new directory and
   apply the saved experimental patch there. Do not edit the existing Mesa checkout.
2. Use `meson-uwp` commit `0379d79b0c4d1bd907fd8e1a5f5ae987a62266b8` (1.11.2) with the x64 VS
   developer environment. YODA used Python 3.14.10 in a local virtual environment with Mako 1.3.10,
   PyYAML 6.0.3, packaging 26.0 and PLY 3.11; no global packages were changed.
3. Configure the Mesa snapshot using:

```powershell
$env:CC = 'cl'
$env:CXX = 'cl'
& <venv-python> <meson-uwp>/meson.py setup <build> <source> `
  --backend vs2026 --uwp --buildtype release `
  -Dvulkan-drivers=microsoft-experimental -Dgallium-drivers= -Dopengl=false `
  -Dgles1=disabled -Dgles2=disabled -Degl=disabled -Dglx=disabled `
  -Dplatforms=windows -Dllvm=disabled -Dzlib=disabled -Dzstd=disabled `
  -Dexpat=disabled -Dbuild-tests=false -Dmicrosoft-clc=disabled `
  -Dspirv-to-dxil=false -Dshader-cache=disabled -Db_vscrt=md
```
4. The generated UWP projects expected a precompiled header and lacked `_XBOX_UWP`. Add this
   `Directory.Build.targets` at the build root before building the `vulkan_dzn` project:

```xml
<Project>
  <ItemDefinitionGroup><ClCompile>
    <PrecompiledHeader>NotUsing</PrecompiledHeader>
    <PreprocessorDefinitions>_XBOX_UWP;%(PreprocessorDefinitions)</PreprocessorDefinitions>
  </ClCompile></ItemDefinitionGroup>
</Project>
```
5. Build the generated `src/microsoft/vulkan/*vulkan_dzn*vcxproj` with MSBuild using
   `Configuration=release` and `Platform=x64`. Building the solution target by name failed because
   its generated name contains `@`; building the project directly succeeded with project references.
