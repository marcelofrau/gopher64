# UWP local build/debug workflow

> Historical P1 baseline. The current P2 implementation is in [17](17-p2-implementation-handoff.md); the user alone deploys to or launches on Xbox.

## Status

P1 local static-library workflow implemented and validated in Release and Debug. P2 Xbox app host/debug deployment is not implemented.

## Findings

- The workspace already has Visual Studio Community 2026 (18.11), the UWP C++ component, Windows SDKs, CMake 4.4.4, LLVM/`clang-cl`, and Rust 1.99.1 with `rust-src`.
- The sibling `../SDL3-uwp` exists. The existing `scripts/uwp/build-deps.ps1` can build SDL3 and SDL_ttf into `out/uwp-deps`; `gen-links.ps1` then creates the machine-local Cargo links file.
- At the start, the required Rust submodules were not initialized, and neither the SDL output nor `.cargo/uwp-links.toml` existed. The build entry point now initializes only the two required submodules when passed `-InitSubmodules`.
- Sibling Ymir has the closest useful pattern: PowerShell entry points, dynamic VS/CMake detection, explicit configurations, and no cleanup unless requested. ScummVM provides useful MSBuild/package examples.
- There is no UWP C++ host, CMake project, Visual Studio solution, manifest, or package output under `uwp/`; the solutions found in the repository are unrelated `rcheevos` tests. A VS project or `run`/package/deploy wrappers cannot honestly build or debug an Xbox app yet.
- The older P0 handoff mentions `out/p0-2026-10-30`, but that path is absent in this checkout. Build outputs and `-Clean` are scoped to `out/uwp-deps*` so any future checkpoint under `out/` is preserved.
- First dependency-build attempt selected the VS 2022 generator simply because it appeared first in `cmake --help`, then failed while probing `VCTargetsPath`. The build script must match the installed VS instance to its CMake generator and keep per-version build caches.
- The first wrapper run initialized the needed submodules, then exposed a preflight bug: PowerShell `-notmatch` returned every installed component except `rust-src`. The check now tests for the exact `rust-src` entry.
- The next run exposed two Rust installations on PATH: standalone `cargo` was not using the repository's rustup-managed pinned toolchain, whose sysroot contains `rust-src`. The build entry point now invokes `rustup run` and validates `Cargo.lock` in that toolchain's standard-library sources.
- The first Rust compile completed successfully, but the wrapper looked for the `staticlib` beside Cargo's primary crate outputs. `cargo rustc -- --crate-type staticlib` places the extra crate type under `target/<triple>/<profile>/deps`; verify and report that artifact path.

## Decisions

1. Keep the current pass in P1: automate prerequisites and build `gopher64.lib` only.
2. Keep scripts under `scripts/uwp/`, follow the Ymir conventions, pin the repo's Rust toolchain, and never clean generated output by default.
3. Ignore only generated `out/uwp-deps*` and `.cargo/uwp-links.toml`; leave unrelated untracked user files untouched.
4. Do not add a placeholder `.sln` or no-op `run.ps1`. Add the CMake/VS project, package, Xbox deploy, and real launch/debug flow when the P2 host exists. Device Portal launch and Visual Studio attached debugging are separate flows.

## P1 implementation and verification

`scripts/uwp/build.ps1` checks the pinned Rust source component, `clang-cl`, and required submodules; builds the SDL dependencies; generates the local Cargo links config; and builds the UWP `staticlib`. Pass `-InitSubmodules` only when the two target-required submodules are absent. `-SkipDependencies` reuses already-built SDL libraries.

Verified on VS Community 2026 (18.11), Windows SDK 10.0.28000.0, CMake 4.4.4, Rust 1.99.1 / LLVM 23.1.1, and clang-cl 23.1.4:

| Check | Result |
|---|---|
| `pwsh -NoProfile -File scripts/uwp/build.ps1` | Pass; SDL3/SDL_ttf and Release staticlib built through the single entry point |
| `pwsh -NoProfile -File scripts/uwp/build.ps1 -SkipDependencies -Configuration Debug` | Pass; Debug staticlib built |
| `rustup run 1.99.1 cargo build --release --locked` | Pass; desktop Release still compiles |
| `dumpbin /linkermember:2 .../gopher64.lib` | Both `gopher64_uwp_main` and `gopher64_uwp_set_root` are present in Debug and Release archives |
| `--print native-static-libs` | `SDL3_ttf.lib SDL3.lib bcrypt.lib kernel32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib /defaultlib:msvcrt` |
| `cargo fmt --all -- --check`; `git diff --check` | Pass |

The native-library line is evidence for the next MSBuild step, not a clean UWP import audit: P2 must select the Store CRT and resolve UWP-safe system libraries. There is no host executable yet, so packaging, launching, Xbox deployment, and attached Visual Studio debugging remain untested. C/C++ warnings from `parallel-rdp` (deprecated CRT calls and signed/unsigned comparisons) are non-fatal and were not changed here.

## P2 follow-up (not part of this pass)

- Add the SDL3 UWP host and CMake project/presets for VS 2026.
- Generate/open the VS solution and configure Xbox package/debug deployment.
- Add package, install/launch, and attached-debug scripts only after there is an app to package and launch.
