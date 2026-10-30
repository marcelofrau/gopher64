# P0 handoff — 2026-10-30

## Resume here

**First working configuration found.** The user confirmed perfect Super Mario 64
rendering on desktop **mesa-dist-win Dozen 26.2.4 + NVIDIA RTX 3500 Ada** with:

```text
dzn_enable_8bit_loads_stores=true
PARALLEL_RDP_ALLOW_EXTERNAL_HOST=0
PARALLEL_RDP_SINGLE_THREADED_COMMAND=1
PARALLEL_RDP_UBERSHADER=1
GRANITE_VULKAN_DEVICE_INDEX=0
```

The process exited with code 0. No gopher64 or renderer source was modified.
This is a working configuration, **not a root-cause fix or full P0 GO**.
FPS/speed, saves, upscaling and the other reference games remain to be tested.
Do not start P1 as though P0 had passed. Validate the next stage with the user.

## Local artifacts

- Working branch: `uwp/main`. This checkpoint is the basis for tag `uwp-v0.0.1`.
- `.claude/plans/pode-dar-uma-olhada-hashed-puffin.md` was already staged before
  this work. Preserve it; it predates the findings below.
- Desktop executable: `target/release/gopher64.exe`, built with
  `cargo build --release --locked` (Rust 1.99.1; Scoop LLVM 23.1.4).
- All pinned project submodules were initialized without changing their pointers.
- Original scratch directory:
  `%LOCALAPPDATA%\Temp\opencode\gopher64-p0-53e6ef25889b4b9e98785a839d7a1f5b`.
- **Preserved local checkpoint:** `out/p0-2026-10-30/`, excluded only through
  `.git/info/exclude`. Contains top-level logs/feature dumps, both downloaded
  Dozen archives, extracted drivers and the isolated portable emulator directory.
  The portable copy contains test data; do not overwrite it when resuming.
  All **66 copied files were SHA-256 verified** against their originals;
  `SHA256.csv` records the hashes. This checkpoint is local and is not in Git.
- ROM used: `D:\tmp\Super Mario 64 (U) [!].v64` (not copied into the checkpoint).
- Test source/build stays under scratch `parallel-rdp-tests/`; it can be rebuilt
  from the pinned revision below if temporary files disappear.
- No global Vulkan driver registration or replacement was performed. Driver and
  renderer environment variables were set only in launched PowerShell processes.

Dozen archive provenance (SHA-256 verified during download):

| Package | URL | SHA-256 |
|---------|-----|---------|
| Working distribution | https://github.com/pal1000/mesa-dist-win/releases/download/26.2.4/mesa3d-26.2.4-release-msvc.7z | `351fc8c8b695878ffb3eaa044b3ead08672a48b1a045e3c3e3975811df0f6695` |
| Crashing distribution | https://github.com/mmozeiko/build-mesa/releases/download/26.2.6/mesa-dzn-x64-26.2.6.7z | `71a358a714d91175847ce0fdb2f9f3d797e58fbf74f8f39eda4555db429d0e57` |

The working extracted driver directory has `vulkan_dzn.dll`,
`dzn_icd.x86_64.json`, and `dxil.dll`. No installer is needed.

## Reproduce the successful game test

Run in a **fresh PowerShell** from the repository root; close that shell afterward
to discard its environment settings. The portable marker isolates config/saves
from the user's normal gopher64 data.

```powershell
$base = (Resolve-Path 'out\p0-2026-10-30').Path
$test = Join-Path $base 'gopher64-portable'
$dzn = Join-Path $base 'mesa-dist-win-26.2.4\x64'
$env:VK_DRIVER_FILES = Join-Path $dzn 'dzn_icd.x86_64.json'
$env:PATH = "$dzn;$env:PATH"
$env:dzn_enable_8bit_loads_stores = 'true'
$env:PARALLEL_RDP_ALLOW_EXTERNAL_HOST = '0'
$env:PARALLEL_RDP_SINGLE_THREADED_COMMAND = '1'
$env:PARALLEL_RDP_UBERSHADER = '1'
$env:GRANITE_VULKAN_DEVICE_INDEX = '0'
Remove-Item Env:DZN_DEBUG,Env:VK_ICD_FILENAMES,Env:PARALLEL_RDP_SUBGROUP,Env:PARALLEL_RDP_SMALL_TYPES,Env:PARALLEL_RDP_FORCE_SYNC_SHADER -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $test 'portable.txt'))) { throw 'Portable marker missing' }
& (Join-Path $test 'gopher64.exe') 'D:\tmp\Super Mario 64 (U) [!].v64'
```

Index 0 was NVIDIA in this machine's Dozen enumeration. Verify enumeration again
after a driver/hardware change. Original success log: `dozen-nvidia-uber-stderr.log`.
It confirms `Overriding ubershader = 1` and single-threaded command processing.

## What we learned / already tried

1. Native NVIDIA Vulkan renders SM64 correctly both normally and with
   `ALLOW_EXTERNAL_HOST=0`. The incoherent path alone is not enough to reproduce.
2. Dozen 26.2.4 starts but produces severe tiled corruption by default. Disabling
   subgroups, disabling small arithmetic, or forcing single-threaded commands
   individually did not fix NVIDIA gameplay.
3. Intel selection changed the corruption, but did not solve it; Intel with
   ubershader also remained visibly corrupted. Do not generalize NVIDIA success.
4. Dozen 26.2.6 (mmozeiko) crashes with access violation `0xC0000005` after renderer
   initialization. Disabling external import, disabling subgroups/small arithmetic,
   and adding the 26.2.4 `dxil.dll` directory to PATH did not solve it. Loading that
   DLL was not independently confirmed. This is separate from the 26.2.4 corruption.
5. LLDB launch of the graphical emulator timed out; no usable crash stack obtained.
6. **Plan correction:** 8/16-bit storage buffer access is mandatory
   (`rdp_renderer.cpp:168–177`). `SMALL_TYPES=0` affects arithmetic only.
   `dzn_enable_8bit_loads_stores=true` is required here, not a performance tweak.
   M-09 was promoted to a P3 requirement in the docs.
7. C++ did not log single-threaded processing when Rust alone called `set_var`.
   Supplying the setting before process launch makes the log appear, but does not
   itself repair corruption. Investigate Rust/CRT environment handling separately
   (Q-07); do not mislabel it as the graphics root cause.
8. External host import was advertised on NVIDIA but failed with
   `No compatible host pointer types are available`; no coherent Dozen run passed.

Full manual and automated results: [13 — Testing](13-testing-debugging.md#p0-results).
Screenshots are in the conversation; text results and logs are preserved locally.

## Automated reproducer — no ROM or window required

Upstream source: `https://github.com/Themaister/parallel-rdp.git`, commit
`395da8f0b896a3158987dcdcdeabfefbad59366d`, from the standalone `COMMIT` file.
Pinned Granite: `53090429539bf3404250d18fe58db971f0907c04`.
Pinned Angrylion: `31bdb1f0a79dd726017a38432540c6b5db0fa117`.
Three main RDP implementation files match gopher64 byte-for-byte; the test build
compiles shaders at runtime, while gopher64 embeds SPIR-V.

Build using a short writable path for `$repo`:

```powershell
git clone --no-checkout https://github.com/Themaister/parallel-rdp.git $repo
git -C $repo fetch origin 395da8f0b896a3158987dcdcdeabfefbad59366d
git -C $repo checkout --detach FETCH_HEAD
git -c core.longpaths=true -C $repo submodule update --init --recursive
cmake -S $repo -B "$repo\build" -G 'Visual Studio 18 2026' -A x64 '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
cmake --build "$repo\build" --config Release --target rdp-conformance vi-conformance --parallel 8
```

Lessons: Git required `core.longpaths=true`; quote the entire CMake `-D` argument
in PowerShell or `3.5` may be passed incorrectly. These are preparation issues,
not rendering failures. The tests successfully built with MSVC 19.51 / SDK 26100.

Use the driver/environment settings above with these executables:

```powershell
# Reproduces incorrect RDRAM at the first iteration:
Remove-Item Env:PARALLEL_RDP_UBERSHADER -ErrorAction SilentlyContinue
& "$repo\build\Release\rdp-conformance.exe" --suite fill-16 --range 0 10 --verbose

# Working reduced verification on NVIDIA:
$env:PARALLEL_RDP_UBERSHADER = '1'
& "$repo\build\Release\rdp-conformance.exe" --range 0 10
& "$repo\build\Release\vi-conformance.exe" --suite aa-none-rgba5551 --range 0 10
```

Results:
- Native `fill-16`: pass, 11 iterations.
- Dozen default `fill-16`: fails iteration 0, RDRAM dword 3888,
  expected `0`, got `0x10001`.
- Same mismatch with `nobindless`, no small types, no subgroups, or both disabled.
- Synchronous compilation does not fix it: five basic suites fail.
- Ubershader: **237/237 RDP suites pass** with `--range 0 10`; texture-load tests
  use their own internal case loops. One VI suite also passes. Not the full
  1000-iteration CTest run, not all VI suites, and not a performance benchmark.
- Logs: `conformance-*.log`, especially `conformance-nvidia-uber-all.log`.

The evidence narrows the issue to the split rasterization/depth-blend path versus
combined ubershader execution. It does **not** prove specialization constants are
the culprit: ubershader also bypasses the intermediate buffers and indirect
rasterization dispatch. Relevant code: `rdp_renderer.cpp:2296–2340`,
`submit_rasterization`, `shaders/rasterizer.comp`, Granite barriers/indirect dispatch,
Mesa `src/microsoft/vulkan/dzn_pipeline.c` and DXIL/NIR lowering.

## Next steps, in order

1. Keep the successful NVIDIA/26.2.4 configuration as the reproducible baseline.
   Measure speed and test savestate round trip and 2×/4× upscale using only test data.
2. Test MK64, OoT and GoldenEye against native baselines as ROMs become available.
   Complete the P0 feature/caps dump; do not assume advertised features were used.
3. If continuing root-cause work, use the failing headless `fill-16` case. Compare
   split versus combined execution, intermediate buffers, indirect dispatch and
   barriers; obtain shader/validation evidence before patching Mesa or Granite.
   Avoid another open-ended cycle of manual SM64 option toggles.
4. Expand automated coverage where it informs the decision (VI suites, stronger
   iteration counts, Intel diagnosis). Preserve native controls for any failing case.
5. Decide with the user whether ubershader is an acceptable initial port fallback
   once performance is measured. Xbox memory visibility, DXIL validation and Series S
   behavior remain separate P3 risks; desktop success does not resolve them.

Checks already passed: desktop release build, `cargo fmt --check`,
`git diff --check`. Clippy was not run. No source fix was made. The user authorized
committing the documentation checkpoint, tagging `uwp-v0.0.1` and pushing it.
The next task is to measure a suspected framerate reduction, not assume full speed.
