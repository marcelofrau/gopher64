# P0 — Validate parallel-rdp on Dozen (Windows PC)

> Historical starting plan. For the 2026-10-30 results, working configuration and
> current next steps, use `docs/xbox-port/14-p0-handoff.md`. P0 remains open.

## Context
The Xbox port plan (`docs/xbox-port/`) is done and committed (`1d0394a`, already on `origin/uwp/main`).
The next step in the roadmap is **P0**: prove that parallel-rdp renders correctly on Mesa Dozen
(Vulkan→D3D12) on the PC, **without changing gopher64 code**. This is the project's go/no-go (risk R-01).

Current state (checked 2026-10-29):
- Submodules `retroachievements/rcheevos`, `src/compat/sse2neon`, `android-project/libadrenotools` not initialised.
- No `clang`/`clang++`/`llvm-ar` on PATH (VS BuildTools only has clang-format/tidy). rustc 1.99.1 = LLVM 23.1.1 → need LLVM 23.x (`-flto=thin` + `rust-lld`).
- `GOPHER64_ROMS` not set; no ROMs in the workspace.
- No desktop Dozen. `../RetroArch/pkg/msvc-uwp/.../MESA/x64/vulkan_dzn.dll` is a UWP build (App CRT), not suitable for the Khronos loader on desktop.
- GPU: NVIDIA RTX 3500 Ada (native Vulkan 1.4) + Intel Arc → native baseline available.
- `out/` is **not** git-ignored (doc 04 §7 says it is). Use `.git/info/exclude` (local, does not touch upstream `.gitignore`).

## Steps
1. **Environment**
   - `git submodule update --init --recursive` (only initialises the pinned pointers, does not change them).
   - Install LLVM 23.x (user's choice: scoop `llvm` or the official installer); check `clang --version` and `llvm-ar --version`.
   - Add `out/` to `.git/info/exclude`.
   - Set `GOPHER64_ROMS` (user env var).
2. **Baseline build**: `cargo build --release`, plus `cargo fmt --check` and `cargo clippy -- -Dwarnings` to confirm the toolchain.
3. **Native baseline**: run SM64, MK64, OoT and GE on NVIDIA. Take screenshots and note FPS (F1/OSD).
4. **Desktop Dozen** → `out/dzn-desktop/`, from mesa-dist-win (pal1000, MIT): `vulkan_dzn.dll` + `dzn_icd.x86_64.json`.
   Fallback: build `../mesa-uwp` for the desktop target with `../meson-uwp`.
5. `VK_DRIVER_FILES=out/dzn-desktop/.../dzn_icd.x86_64.json vulkaninfo --summary` → only `Microsoft Direct3D12 (…)`, `driverID = MESA_DOZEN`.
6. **Dozen runs** (no code change), per doc 04 §7:
   coherent → `PARALLEL_RDP_ALLOW_EXTERNAL_HOST=0` → `PARALLEL_RDP_SUBGROUP=0` → `dzn_enable_8bit_loads_stores=true`;
   savestate round trip; upscale 2×/4×. SM64 first, then MK64, OoT and GE.
7. **Feature dump**: collect it via `VK_LOADER_DEBUG`/`vulkaninfo` (full, Dozen) + parallel-rdp logs. A temporary log patch only if needed, on a throwaway local branch, never committed to `uwp/main`.
8. **Record**: fill in `docs/xbox-port/13-testing-debugging.md §4`, tick P0 in `10-roadmap-checklist.md` (including the already completed item "Create branch uwp/main"), and fix the `out/` note in 04 §7.
   Commit `uwp: record P0 Dozen results (D-xx)` after `fidelity-reviewer`. Push only with approval.

## Files touched (port-owned only)
- `docs/xbox-port/13-testing-debugging.md`, `10-roadmap-checklist.md`, `04-vulkan-dozen.md`
- `.git/info/exclude` (local, not versioned)

## Verification
- `vulkaninfo --summary` with Dozen shows only MESA_DOZEN.
- gopher64 log/OSD shows the Dozen device; visual parity with the native screenshots; speed ≥ 100%.
- `git diff main...uwp/main --stat` contains only `docs/xbox-port/`; `cargo fmt --check` + both clippy runs green.
- Exit: GO → P1; NO-GO → triage doc 04 §7 / 09 §3.
