# Cross-machine work journal

This is the shared, append-only handoff for agents working on the Xbox port. Read this file,
`README.md`, and the detailed [P2 implementation handoff](17-p2-implementation-handoff.md)
before continuing. Append an English dated entry after **each interaction**: intent, changes or
decisions, evidence and failed attempts, tests with actual outcomes, and remaining work. Do not
include credentials, console addresses, commercial ROMs, or machine-specific absolute paths.
Commit the agent's own work in logical increments; do not stage unrelated user files or push
unless requested.

**Non-negotiable:** agents never connect to, deploy to, or launch on the user's Xbox. Only the
user performs console deployment or VS F5. Agent execution and automated tests stay on the PC.
Never uninstall the existing app merely to refresh a package: its `LocalState` can contain data.

## Current state — 2026-11-04

- Branch: `uwp/main`, tracking `origin/uwp/main`. The original P1 work was already on this
  branch; the staticlib was not a complete installable Xbox app.
- A C++/SDL3/ImGui UWP host, VS 2026 solution generation, MSIX, timestamped stage logs, LocalState
  setup, Rust C ABI calls, test-ROM autoboot, and PC build/test scripts now exist. ImGui is only a
  minimal status/error screen, not the finished game library or settings frontend.
- Release and Debug host packages build on PC. The registered PC app presents its status frame,
  calls Rust, and reports a missing ROM. A copied real ROM reaches SDL video initialization, where
  `Vulkan support is not configured in SDL`; `rdp_init` and game rendering are **not reached**.
- The user's first local VS F5 failed with `0xc0000135`. A Debug/Release CRT mismatch was proven and
  corrected; the local registered app worked after the fix. **The user has not yet repeated VS F5.**
- P2 is **in progress, not complete**. No Xbox deployment, tile launch, F5, gameplay, audio, input,
  lifecycle, or save verification has been performed. The user alone performs future Xbox tests.
- API-level Store validation, Xbox module availability, dynamic imports, signing/trust and
  `APPX0006` (`runFullTrust`) remain open. The import-module audit is not certification.
- The fixes to ROM error handling and RDRAM allocation touch shared upstream files, and the SDL
  pointer-access patch is carried in this repository. These depart from the original fidelity rules;
  they are explicitly recorded for review in [00](00-upstream-fidelity-rules.md) and
  [12](12-upstream-sync.md). The user was informed; do not describe them as UWP-only hooks.
- Android was already present upstream; it is **not a target of this fork** and was not newly added.
  Shared changes could affect it, but it has not been tested.

## 2026-11-04 — Branch discovery, P1 reproduction and planning

1. Inspected the fork without initially switching branches: `origin/uwp/main` contained seven
   commits beyond `origin/main`, including Rust UWP staticlib changes and C ABI exports. Switched
   the clean worktree to local `uwp/main` tracking that branch. The previously untracked
   `docs/assets/` files belonged to the user and were left untouched.
2. Reproduced the P1 build on PC. The old dependency script selected the first listed CMake
   generator (VS 2022), which failed despite VS 2026 being installed. Updated selection to match an
   installed Visual Studio with its generator and keep caches separate by VS version. Initialized
   the two emulator submodules required for the build; their Git pointers were not changed.
3. The first Rust attempt could not find `rust-src` because the `cargo` on PATH was a separate
   installation, not the rustup-managed toolchain. The build wrapper now uses the project's pinned
   toolchain via `rustup run`. Release and Debug `gopher64.lib` built; `dumpbin` confirmed
   `gopher64_uwp_main` and `gopher64_uwp_set_root`. The desktop Release build also passed.
4. Added a reusable PowerShell entry point and generated local-only Cargo link settings; narrowed
   ignores and any cleaning to generated UWP outputs. Updated stale planning/checklist documents.
   A historical P0 checkpoint named in older notes was not present in this checkout; its earlier
   reported PC results are not a fresh reproduction or Xbox evidence.
5. Inspected Gopher64 and `../Ymir` before implementing P2. Gopher64 currently owns its game window,
   SDL lifecycle and blocking event/session flow; Ymir's separate emulator-thread model cannot be
   copied blindly. P2 was scoped to a minimal status screen, host and package; a full frontend,
   stable game-to-menu flow and actual Vulkan rendering remain later work. SDL audio compilation
   does not establish Xbox runtime behavior; Ymir's documented WASAPI deadlock makes XAudio2 an
   evidence-backed later direction, not something implemented or proven here.

## 2026-11-04 — P2 implementation and PC investigations

1. Added `uwp/CMakeLists.txt`, the C++ host, package manifest and derived logo assets, plus scripts
   to build dependencies and Rust, generate the VS solution, package, audit imports, install/test
   locally, and open Visual Studio. The host logs staged startup, presents a status/error screen,
   passes a writable LocalState root to Rust and supports an optional user-supplied ROM path.
   The host relinquishes SDL/ImGui resources before invoking the existing blocking Rust entry and
   attempts to recreate the status UI afterwards. Full lifecycle and repeatability remain unproven.
   CMake fetches Dear ImGui at a pinned revision with a SHA-256 integrity check; initial setup needs
   network access or an existing CMake download cache. The host currently uses C++17 and `/ZW`.
2. First host startup died at the first SDL event poll. CDB captured a WinRT
   `AccessDeniedException` querying `CoreWindow->PointerPosition`; continuing reached `0xc000027b`.
   A narrow `Platform::AccessDeniedException` catch in an isolated SDL3-uwp copy retained the prior
   mouse-focus state and allowed the first frame on PC. The generated-source patch is recorded in
   `scripts/uwp/SDL3-pointer-access.patch`; the sibling Git tree is not edited. An earlier
   `git apply` falsely succeeded while skipping the intended generated file; the script now uses an
   explicit repository-root path and reverse-checks the patch. Xbox behavior is untested.
3. A missing-ROM probe initially panicked. Changed ROM input and archive error paths to surface
   errors rather than abort, and propagated initialization errors back to the status UI. Tests cover
   missing/short ROMs, malformed archives and byte-swapped input. Configuration write failures are
   now surfaced rather than silently ignored.
4. Returning from a real-ROM startup failure then crashed in `RtlpHeapHandleError`. CDB identified an
   8 MiB RDRAM allocation whose `Vec<u8>` was reconstructed from a 64 KiB-aligned allocation with an
   incompatible deallocation layout. The fix keeps the original `Vec` allocation and uses an aligned
   internal slice. Unit tests cover 4/8 MiB alignment, clone/clone-from and compatibility with the
   prior postcard representation. The same PC ROM failure then returned to the status screen without
   that heap crash. Real gameplay and save/load compatibility have not been proven.
5. A repeated MSIX with the same package identity and version was rejected; development packages
   now receive a new version, avoiding an uninstall and preserving existing LocalState. The PC
   installer does not install a signing certificate and does not update VCLibs unless explicitly
   requested. Module-level import audit passed on the final packaged EXE and SDL DLLs; it does not
   validate individual APIs or dynamically loaded dependencies.
6. Validation: PC Release and Debug host builds passed; missing-ROM probe passed; a real ROM failed
   as expected at SDL Vulkan window creation before `rdp_init`. Desktop Release initialized Vulkan
   and remained alive for a 12-second isolated smoke test; no visual gameplay or real save/load was
   verified. `cargo test --lib --locked` passed 8 tests, both Clippy configurations passed, and
   formatting and PowerShell parsing passed. Native C/C++ deprecated-CRT/signedness warnings,
   Debug `LNK4075`, and package `APPX0006` were not hidden.
7. The user explicitly prohibited automatic Xbox access/deployment. An unneeded Xbox deployment
   script and its mock test were removed before use; no console connection was made. This rule is
   recorded in `AGENTS.md`, `CLAUDE.md`, and the port documentation.

## 2026-11-04 — User's local VS F5 failure and fix

1. The user ran F5 locally. VS logged the host and SDL DLLs loading, then exited with `0xc0000135`
   (`A dependent dll was not found`) before the host's first log. The screenshot showed a Windows
   Store app activation failure. The same failure was reproduced locally outside VS.
2. `dumpbin` showed Debug SDL DLLs importing the Debug VCLibs runtime while the manifest declared
   the Release VCLibs package. A separately registered temporary PC package using the same binaries
   and adding `Microsoft.VCLibs.140.00.Debug` reached the first status frame. The temporary package
   was unregistered and its generated files removed. This establishes the mismatch, not Xbox support.
3. `build.ps1` now builds Release SDL dependencies for both host configurations, matching the host's
   Release Store CRT while keeping host and Rust Debug builds. The import audit rejects Debug CRT
   imports; its positive/negative checks pass in `scripts/uwp/tests/audit-imports.tests.ps1`.
4. A newly generated correct MSIX did not replace the older Debug SDL DLLs in an existing VS local
   `AppX` layout because their timestamps were newer. `build-host.ps1` now refreshes only mismatched
   SDL DLLs in that generated layout. The original locally registered app then passed the missing-ROM
   probe and logged its first frame and Rust return. This is **not** a fresh VS F5; the user was asked
   to reopen the solution and retry locally. Xbox was not accessed.

## 2026-11-04 — Shared handoff and commit workflow

The user requested an English diary usable by the other agent on another machine and regular
commits of agent-owned changes. Created this journal and repository-wide `AGENTS.md` instructions,
and linked it from the port README. Before committing, inspect the index and working tree, preserve
the pre-existing untracked `docs/assets/`, verify the configured Git identity, and include only
agent-owned work. Do not push without the user's request. Future agents must append an entry after
every interaction, including when a task is blocked or only a decision was made.

Before committing, reran the eight Rust library tests, Rust formatting check, PowerShell script
parsing, CRT import self-check, and `git diff --check`; all passed. Native compiler warnings remain.
