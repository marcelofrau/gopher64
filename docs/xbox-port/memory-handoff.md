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

## 2026-11-04 — Visual Studio F5 observation and native ROM picker

- The user ran Debug / x64 locally via VS 2026 after the CRT fix. Their debugger log shows the app,
  SDL3 and Store Release CRT loading, `SDL renderer: direct3d11`, `Status screen initialized`, and
  `First status frame presented`; their screenshot shows the minimal ImGui status screen. This
  confirms the previously failing **PC F5** startup. It does not validate game rendering or Xbox.
- The log also contained `Windows.Gaming.Input` HRESULTs `8685C003` and `80070057`, Game Bar
  `80004005`, and injected Windhawk modules. The app nevertheless displayed its first frame. Their
  causes and impact on gamepad behavior are unproven; do not dismiss or attribute them without tests.
- The user requested an automatic ROM picker at startup on PC and asked to consult Ymir's original
  UWP picker before its custom explorer. Ymir commit `512a00a5`,
  `apps/ymir-sdl3/src/platform/uwp/uwp_platform.cpp`, uses WinRT `FileOpenPicker` on the UI thread
  with asynchronous completion; it catches errors when setting a suggested start folder because
  some locations are unavailable on Xbox. This host uses the same system picker via existing
  C++/CX/PPL rather than Ymir's C++/WinRT coroutine, without forcing a start location. Ymir's
  `docs/uwp/TESTING.md` also warns that receiving `StorageFile.Path` does not prove direct sandbox
  access and recommends copying with WinRT if direct opening fails.
- The first implementation opened a picker in an isolated PC test package; cancellation returned to
  the status UI. The user's own PC F5 selection returned `Could not read ROM file`. Outside the app,
  the selected file exists and its V64 header and size are valid. Because Rust discards the original
  I/O error, a sandbox/path restriction is likely but not yet proved by an OS error code.
- Implemented asynchronous `StorageFile.CopyAsync` into the app's `TemporaryFolder`, using a unique
  name and never overwriting or modifying the source. The UI shows progress/error; cancellation
  offers retry. The existing autoboot and missing-ROM test markers remain available. An interactive
  PC test with a separately registered proof package selected a V64, copied it, read it through
  Rust and reached `creating SDL Vulkan game window`; it then hit the expected WinRT Vulkan error
  and restored the status UI. This proves selected-file reading in this PC test, not gameplay or
  Xbox access. Temporary copies persist until Windows clears them, and UWP `recent_roms` may
  contain temporary paths; the eventual library must handle this explicitly, preferably with
  persistent brokered access rather than pretending temporary paths are durable. Existing save
  filenames derive from the ROM header/hash, not the staging filename.
- The host and MSIX compiled in both Debug and Release in an isolated build; both import audits
  passed, while `APPX0006` and the Debug linker's `LNK4075` remain warnings. The actual VS Debug
  host was also rebuilt. The missing-ROM probe was verified to bypass the new picker and exit as
  expected. Its first temporary test incorrectly read the previous launch's appended log and
  reported a false failure; restricting the check to the latest launch passed.
- An isolated build directory is used while the user's VS instance is running. Do not terminate or
  overwrite their running session for testing. Picker behavior and path access on Xbox remain
  untested and must be tested by the user. Even after ROM loading, the known SDL WinRT Vulkan
  blocker remains.

### Dozen/P0 clarification

The user asked whether the existing P0 Dozen work was being considered. **Yes: Dozen remains the
graphics target**, not the Direct3D 11 renderer currently used by the temporary ImGui status screen.
P0 tested parallel-rdp on desktop Dozen 26.2.4 with the specific 8-bit storage, incoherent RDRAM,
single-threaded command, and ubershader settings documented in `14-p0-handoff.md`; it did not
validate the UWP package or Xbox. Preserve those results and their limitations as the P3 baseline.

The current UWP game code still requests `SDL_WINDOW_VULKAN` in `src/ui/video.rs` and uses
`SDL_Vulkan_CreateSurface` in `parallel-rdp/wsi_platform.cpp`. WinRT SDL rejects the window before
`rdp_init`, and the current package does not contain `vulkan_dzn.dll`. Thus the latest ROM test
did not exercise Dozen at all. The planned P3 work in `04-vulkan-dozen.md` and the roadmap must
provide the UWP Dozen ICD, loader, surface/CoreWindow integration, suitable feature flags and
diagnostics, then verify the actual results; the documented sketches are not implemented or proved.
No change to the chosen graphics direction was made, and Xbox deployment remains user-operated.

### User PC F5 ROM test — 21:31

The user ran the current Debug build in VS 2026 on Windows and selected **Banjo-Tooie (.v64)**.
Their log shows the picker opening, the ROM copied to the app's temporary storage, the frontend
shut down, and Rust entered. At `21:31:55.407`, Rust logged `creating SDL Vulkan game window`
and immediately reported `Vulkan support is either not configured in SDL or not available in
current SDL video driver (winrt) or platform`. It shut down services, returned `1`, and the host
restored and presented the status screen. This verifies selected-ROM reading and error recovery in
this PC run, not `rdp_init`, Dozen, gameplay, or Xbox behavior.

The debugger also emitted Windows Gaming Input HRESULTs `8685C003` and `80070057` and Game Bar
`80004005`; the log does not establish their cause or gameplay impact. Windhawk modules were loaded
into this PC process; no causal connection to the observed Vulkan failure has been established.
SDL3 DLLs are Release builds without symbols, as intended by the corrected Store CRT build workflow.
The next graphics work remains the planned Dozen/P3 integration, subject to the user's direction.
