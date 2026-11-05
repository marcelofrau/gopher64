# Cross-machine work journal

This is the shared, append-only handoff for agents working on the Xbox port. Read this file,
`README.md`, and the detailed [P2 implementation handoff](17-p2-implementation-handoff.md)
before continuing. Append an English dated entry after **each interaction**, including the machine
name from `$env:COMPUTERNAME` (PowerShell) or `hostname` (elsewhere): intent, changes or
decisions, evidence and failed attempts, tests with actual outcomes, and remaining work. Do not
include credentials, console addresses, commercial ROMs, or machine-specific absolute paths.
Commit the agent's own work in logical increments; do not stage unrelated user files or push
unless requested.

**Non-negotiable:** agents never connect to, deploy to, or launch on the user's Xbox. Only the
user performs console deployment or VS F5. Agent execution and automated tests stay on the PC.
Never uninstall the existing app merely to refresh a package: its `LocalState` can contain data.

## Machines and evidence

Identify the current host at the start of work with `$env:COMPUTERNAME` in PowerShell, or
`hostname` elsewhere. Use that name in new machine-specific notes, memories and handoff entries;
if it cannot be determined, say **unknown**. A separate `.env` is unnecessary for identification.
Tool availability and test results are dated observations, not guarantees; recheck when needed.
Agent/model names are not machine names and do not determine their abilities or access.

- **YODA:** current host identified by `COMPUTERNAME` on 2026-11-05. VS 2026 host builds were
  verified here; the user identifies it as the Xbox-access environment. Only the user may deploy
  to or launch the Xbox, including VS F5.
- **YODA storage, 2026-11-05:** repository `out` and `target` are junctions to H:. Full pre-switch
  originals remain in the `gopher64-build-backup-20261105` directory on F:. Do not delete them
  without user approval; they still occupy F: space. H: must be available to build.
- **Other host, name unknown:** the 2026-11-05 Opus entries below were written on another PC. Its
  reported Rust/clang tests and host-build investigation apply to that PC only. The later probe
  found VS 18 Build Tools and a WindowsStore v180 toolset, but its ImGui download failed and a
  complete host MSIX build was not established. Earlier statements that it had no VS 2026 are
  superseded by that probe; do not assume the IDE or final packaging is available there.

Older entries using "this PC", "Opus machine", "console machine" or agent labels are historical
context, not reliable host names. Do not assign them to YODA or another name without evidence.
Record future checks with the host name and distinguish PC evidence from user-performed Xbox tests.

## Historical state — 2026-11-04

- Branch: `uwp/main`, tracking `origin/uwp/main`. The original P1 work was already on this
  branch; the staticlib was not a complete installable Xbox app.
- A C++/SDL3/ImGui UWP host, VS 2026 solution generation, MSIX, timestamped stage logs, LocalState
  setup, Rust C ABI calls, test-ROM autoboot, and PC build/test scripts now exist. ImGui is only a
  minimal status/error screen, not the finished game library or settings frontend.
- The user verified the minimal status screen and ROM picker by VS F5 on PC. The original SDL
  Vulkan error was bypassed using the experimental packaged Dozen ICD and CoreWindow WSI. An
  isolated PC ROM test now reaches and successfully returns from `rdp_init`, but hangs in
  `SDL_OpenAudioDeviceStream` before any game frame or CPU loop has been observed. The integrated
  P3 build has been compiled in Debug only; it is not proven functional or ready for gameplay.
- The user's first local VS F5 failed with `0xc0000135` due to a proven Debug/Release CRT mismatch;
  the later local F5 succeeded. The user has not yet run the current experimental P3 build via F5.
- P2/P3 remain **in progress, not complete**. No Xbox deployment, tile launch, F5, gameplay, audio, input,
  lifecycle, or save verification has been performed. The user alone performs future Xbox tests.
- `build-host.ps1` now requires an experimental UWP Dozen ICD and `dxil.dll`, from
  `GOPHER64_DOZEN_DIR`/`-DozenDir` or the sibling ScummVM RetroArch checkout. The currently tested
  candidate is Mesa 25.3.3, not the 26.2.4 desktop baseline; binaries are not committed.
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

### Temporary ROM copies and next priority

The user asked whether the same temporary copy is reused and requested a follow-up to eliminate
copying after higher-priority issues. **The directory is shared per app, but copies are not reused:**
`StorageFile.CopyAsync` uses `GenerateUniqueName`, creating another full copy on every selection,
including repeated selections of the same ROM. The app does not explicitly remove these files;
Windows may clear temporary storage, but there is no guaranteed cleanup schedule. On this PC, the
development app's `TempState` contained one ROM-like file totaling 32 MiB at the time checked. No
files were deleted. Later, replace staging with proven brokered/stream/handle access as appropriate,
while protecting original ROMs and saves and addressing temporary paths in `recent_roms`; do not
remove unrelated user files. This is intentionally deferred, not forgotten.

The recommended next priority is the Dozen integration described above: use the P0 proven desktop
settings as a baseline, study working UWP reference implementations, establish package/loader and
SDL/CoreWindow surface integration with logs, and test the earliest verifiable graphics milestone on
PC before the user chooses whether to deploy and test on Xbox. Do not treat the documentation's
sketches or desktop P0 as proof of the UWP implementation.

### Single-slot staging and Dozen integration authorized — 2026-11-04

The user explicitly requested one fixed file in LocalState, overwritten on each new selection,
and authorized continuing with Dozen integration. Direct/brokered ROM access remains later work.

- The host now copies selected files to `LocalState/gopher64-staging/gopher64-staged.rom` using
  `ReplaceExisting`; it reuses an already-selected staging file rather than copying it over itself.
  The original selected file is never overwritten. Existing files in the old TempState are not
  removed. The fixed slot means recent-ROM entries cannot represent a real persistent library.
- Because the fixed name no longer contains the original extension, the UWP-specific Rust path
  detects ZIP and 7z signatures for that reserved basename; raw ROM byte-order handling remains
  unchanged. A unit test overwrites one slot with valid raw, ZIP, and 7z fixtures and checks each.
- The Debug UWP host and Rust staticlib built and passed module-level import audit. A local isolated
  WinRT picker loaded a real ROM from this fixed path to the known SDL Vulkan blocker. An attempted
  second UI-automation run did not complete as intended; do not treat it as a pass. Independently,
  the same WinRT `StorageFile.CopyAsync(... ReplaceExisting)` API was tested with two real ROMs in
  a separate scratch directory: each copy matched the source SHA-256, only one destination file
  remained, and both originals were unchanged. This API test is not Xbox validation.
- The P3 investigation found a UWP `vulkan_dzn.dll` in the local RetroArch tree. Its embedded
  version is Mesa 25.3.3 (`f1ef4e6e64`), not the P0's desktop Mesa 26.2.4; it exports the ICD
  entry points and contains the `dzn_enable_8bit_loads_stores` option. The sibling `mesa-uwp`
  checkout is 26.2.0. Treat the older DLL only as a diagnostic candidate until actual device
  features and compatibility are proven; do not claim P0 parity.

### P3 ICD baseline probe — 2026-11-04

- Added an opt-in PC probe using `p3-dozen-probe.txt`: load the packaged driver, negotiate ICD v5,
  create a Vulkan instance, enumerate GPUs and query their 8-bit storage features. It does not
  initialize parallel-rdp, create a Vulkan surface or prove gameplay. The build obtains two DLLs
  from an explicit `GOPHER64_DOZEN_DIR` or the known local RetroArch sibling and logs SHA-256;
  binaries are not committed. The candidate is Mesa 25.3.3, not the 26.2.4 P0 binary.
- An initially broken audit referred to an unset extraction variable; fixed and reran it. Auditing
  every packaged root DLL found `dxil.dll` importing `advapi32.dll`. Inspection found only
  `EventRegister`, `EventUnregister` and `EventWriteTransfer`. Microsoft documents all three for
  UWP, so the module check now permits only this exact ETW set; a missing/unparseable import list,
  any other symbol, and ordinal imports fail closed. Added positive/negative parser tests. This is
  not full API-level certification; `APPX0006` and dynamic import risks remain.
- Isolated PC package with the ICD deliberately removed produced explicit error 126. With the DLL
  restored, ICD negotiation succeeded (interface 5), instance creation succeeded and two devices
  were enumerated: RTX 4070 SUPER and Microsoft Basic Render Driver. Both reported
  `storageBuffer8BitAccess=0` and `uniformAndStorageBuffer8BitAccess=0` with default options.
  parallel-rdp checks `storageBuffer8BitAccess` and P0 required `dzn_enable_8bit_loads_stores=true`;
  therefore ICD load/enumeration alone is not compatibility or a first frame.
- Follow-up hypothesis: Microsoft documents `SetEnvironmentVariableW` for UWP; the reference
  `ashes-vk-uwp/uwp/main.cpp` uses it and `mesa-uwp`'s `origin/uwp-25.3.3:src/util/os_misc.c`
  reads options with `GetEnvironmentVariableA`, not the CRT `getenv` snapshot. An opt-in probe
  with that P0 flag is being prepared; whether the GPU advertises 8-bit storage remains unproven.
  This affects only the process and does not change system/user environment variables.
- The follow-up isolated UWP PC probe confirmed the hypothesis: after setting the process-only flag
  with `SetEnvironmentVariableW`, both enumerated devices reported `storageBuffer8BitAccess=1` and
  `uniformAndStorageBuffer8BitAccess=1`; ICD negotiation and instance creation still succeeded.
  This proves one P0 prerequisite for the candidate on the tested PC, not shader correctness,
  surface/swapchain behavior, P0 version parity, performance or Xbox support.
- The candidate advertises `VK_EXT_external_memory_host` on both GPUs, so the P0 requirement to
  disable imported host RDRAM cannot be assumed automatically; verify the C runtime environment
  and implement the P0-safe behavior before claiming game compatibility.
- A separate SDL WinRT window/ICD probe enabled `VK_KHR_surface` and `VK_KHR_win32_surface`,
  created the Mesa-specific CoreWindow surface, and obtained successful GPU surface capabilities
  (`1200x480`) on the PC. This is not a swapchain or a rendered frame. The first automated check
  incorrectly expected the text `surface caps` instead of `surface capabilities`; after correcting
  the test, it passed. The driver reports external-host support, so a UWP-only compilation wrapper
  is being tested to force the P0 copy path and use `GetEnvironmentVariableA` instead of unsupported
  UWP CRT `getenv` in `rdp_device.cpp`; the submodule itself is untouched.

### User-supplied UWP artwork — 2026-11-04

- The user supplied Windows asset variants and asked for manifest/package integration using the
  ScummVM UWP project as reference. Added wide, small, and large tile and lock-screen metadata;
  packaged the supplied scaled splash, tile, app-list, store, badge, and wide variants. Retained
  the legacy unscaled square images in the repository but did not package them, preventing them
  from taking precedence over the new art. The user's ScummVM reference images and full-resolution
  artwork remain untouched and are not shipped in the package.
- The user's 620x620 large-tile PNG exceeded the UWP 204,800-byte limit (`APPX3207`). Created a
  separate OptiPNG-derived image; it is 200,882 bytes and ImageMagick pixel absolute-error count
  is zero. The original was not overwritten. The package built successfully with the derived
  image; visual appearance on an installed PC tile and on Xbox remains untested.
- While work was underway the user added base splash/store and 200%-wide variants; they are also
  included. The `runFullTrust` `APPX0006` warning remains.

### P3 integrated ROM smoke and end-of-day handoff — 2026-11-04

- The UWP game path now loads the packaged ICD, requests the P0 8-bit option using the documented
  process-level Win32 environment API, creates a Mesa-specific CoreWindow Vulkan surface and
  initializes Granite WSI. The UWP-only RDP environment wrapper forces external RDRAM host imports
  off and reads other device options via Win32; an isolated UWP self-check verified the override.
- A PC-only Super Mario 64 smoke reached ICD negotiation, created two surfaces, initialized WSI,
  completed the initial `begin_frame`, and returned from `rdp_init`. No game frame appeared during
  the subsequent 60 seconds. A further 25-second repeat logged `FPS counter task started`,
  `audio: SDL audio subsystem initialized; opening stream`, and then no return from
  `SDL_OpenAudioDeviceStream`; the isolated proof process was terminated after timeout. This is a
  reproducible hang, **not** a successful game test, and no screen contents were verified.
  Timestamped excerpts with machine paths omitted are in
  [the PC evidence log](evidence/2026-11-04-dozen-pc.md).
- SDL's WinRT WASAPI source uses semaphore waits around `ActivateAudioInterfaceAsync`; this is a
  lead, not a proven stack or root cause. Ymir calls the same SDL stream API, and SDL documents it
  as thread-safe. Tomorrow, capture the stuck process's real stack and compare COM/dispatcher and
  SDL lifetime with Ymir before choosing a supported fix. Do not silently disable audio or abandon
  the Dozen goal. A first CDB observer started too late and missed the log; a second was deliberately
  left without a target after the user requested a stop. No stack has been captured.
- An earlier smoke script prematurely stopped its test because `ReadAllText` could not share the
  actively written log; switching to the existing shared-reader fixed the harness. A surface probe
  had a separate false-negative due to `caps` versus `capabilities`; its corrected rerun passed.
- The user offered to run F5 on PC, then explicitly asked to stop and resume tomorrow. Do not
  access, deploy to, or launch on Xbox. The current P3 code is experimental and may hang at audio.
- The user explicitly authorized versioning the previously untracked local asset sources, including
  PSDs, the social preview, and reference images, for the other machine. These are source assets;
  generated `out/` builds, copied third-party DLLs, ROMs, and installed proof-package data are not
  source and must not be added to Git.
- The build-module audit checks the ICD and DXIL DLLs, and permits only three documented UWP ETW
  functions from `advapi32.dll`; unknown symbols and ordinals remain rejected. API-level Store
  validation and `APPX0006` remain open. No Store/Xbox certification is claimed.
- End-of-day checks: UWP Debug host/package and import-module audit passed; desktop `cargo test
  --lib --locked` passed all 9 tests; desktop Clippy with and without default features passed with
  `-Dwarnings`; formatting, PowerShell parse and the Dozen/audit script self-checks passed. Existing
  native C/C++ deprecated-API and signedness warnings, Debug `LNK4075`, and `APPX0006` remain. No
  latest P3 Release UWP build, desktop gameplay/save test, visual game frame, or Xbox test was run.

### P3 audio deadlock resolved with XAudio2 backend — 2026-11-05

- Diagnosed the reproducible PC hang in `SDL_OpenAudioDeviceStream` (logged `opening stream`,
  never returned). Root cause is the one Ymir already hit: SDL3 WinRT WASAPI calls
  `ActivateAudioInterfaceAsync` and waits on a semaphore with no timeout; the CoreWindow
  dispatcher never services the completion from the UI thread. Not fixable in SDL (submodule).
- Implemented decision D-08 (XAudio2 backend). New port-owned files:
  `uwp/audio/g64_xaudio2.{h,cpp}` (synchronous XAudio2: engine → mastering voice → source voice,
  4×512-frame recycled buffers fed from a lock-guarded ring buffer by `IXAudio2VoiceCallback`,
  with linear resampling driven by the frequency ratio so the existing drift control keeps
  working; never blocks the emulator). Architecture adapted from Ymir commit `071639c9`, but to
  gopher64's push model instead of Ymir's pull model. Rust shim `src/ui/uwp/audio.rs` mirrors the
  `sdl3_sys::audio` names/signatures.
- Upstream hook is minimal and gated: `src/ui/audio.rs` gets a single `use … as aud` alias at the
  top (SDL on desktop/Android, the shim on UWP) and a uniform `aud::` prefix on the call sites —
  no per-call-site `cfg`. `src/ui.rs` gates the `Audio.audio_stream` field type. `build.rs`
  compiles the backend, links `xaudio2`, and bindgens its header inside the existing `uwp` block.
  Removed the 5 earlier UWP-only diagnostic log lines in `audio.rs` now that the hang is gone.
- Compiled on a Windows PC by Claude (Opus): the UWP staticlib links and exports `g64_audio_*`;
  desktop `cargo fmt --check`, both Clippy profiles (`-Dwarnings`), and `cargo test --lib`
  (9 passed) are green. **Not tested on Xbox hardware** — the deadlock is avoided because the SDL
  audio path is no longer called on UWP, but real playback, drift control and underrun behavior
  need console validation (P4). `fidelity-reviewer` PASS for this increment (3 upstream files,
  ~96 lines), after fixing two process gaps: the code referenced a nonexistent D-19 (corrected to
  the existing D-08) and the hook inventory / D-08 status note were updated.
- Also produced (not acted on yet, per user) a fidelity report on the earlier P2/P3 commits
  (2953b68..ad49f2b): the cumulative upstream diff is ~713 lines (over the 400 budget) with
  non-gated behavior changes in `device.rs` (run_game→Result, swap_rom, get_rom_contents),
  `rdram.rs` (AlignedRdram, savestate format preserved with a byte-exact test), `config.rs`
  (write_json refactor), and a submodule source file swapped via `uwp/compat/rdp_device_uwp.cpp`.
  Some fix real bugs and are upstream-PR candidates. These are already flagged in the doc's
  "Fidelity deviations requiring ongoing review" section; a dedicated cleanup pass is deferred.

### Fidelity cleanup pass and agent roster — 2026-11-05

- Added an "Agents and machines" section at the top of this file: the Opus deep-analysis machine
  (no VS 2026, no Xbox; builds the Rust staticlib + clang-cl C/C++, runs cargo, reasons about logs)
  vs the secondary VS-2026/console machine (builds the host MSIX, can ask the user to F5/deploy on
  Xbox and return logs). Match tasks accordingly.
- Reviewed the earlier P2/P3 commits for fidelity and did a low-risk cleanup, per the user's choice
  to keep P3 instrumentation and keep genuine bug fixes rather than revert:
  - Added the missing `UWP-PORT` markers to every previously unmarked hook:
    `parallel-rdp/interface.cpp` (9 Dozen WSI/log blocks), `parallel-rdp/wsi_platform.cpp` (4
    CoreWindow-surface blocks), `src/ui/video.rs` (bring-up diagnostics + error-vs-panic branches),
    `src/device.rs` (bring-up logs + the staged-ROM sniff block), `src/ui/config.rs`
    (write-error sink), `uwp/compat/rdp_device_uwp.cpp`.
  - Tagged the non-gated bug fixes in-code as upstream-PR candidates (not reverted):
    `src/device/rdram.rs` `AlignedRdram` (real heap-UB fix, savestate bytes preserved by test) and
    `src/device.rs` `swap_rom`/`get_rom_contents`/`run_game` (panic -> None/Result on bad input).
  - Clarified a prior mischaracterization: `uwp/compat/rdp_device_uwp.cpp` does NOT modify the RDP
    submodule; it `#define`s `getenv` and then `#include`s the unmodified submodule `rdp_device.cpp`,
    so R4 is not violated. Updated the deviations section in 00-upstream-fidelity-rules.md.
  - Kept the `video.rs`/`device.rs` host-log lines on purpose: they are the console machine's P3
    Dozen instrumentation and should stay until P3 is validated on Xbox, then be consolidated.
  - Still open (owned by the VS/console machine): `scripts/uwp/SDL3-pointer-access.patch` should be
    upstreamed to the SDL fork rather than kept as a generated-snapshot patch.
- No logic changed in this pass (markers/comments/doc only). Verified on PC by the Opus agent:
  `cargo fmt --check` clean, desktop `cargo check --no-default-features` and the UWP staticlib
  `cargo check` both exit 0. No Xbox test (cannot, this machine). The XAudio2 audio backend from the
  same day (commit 0953db8) is pushed to origin/uwp/main and still needs console playback validation.

### PC-side P2/P3 de-risking: logging bridges, import audit, WSI review — 2026-11-05

Opus agent, PC only (no VS2026/Xbox). Four PC-doable workstreams to unblock the console agent:

- **Host link fix (committed 5bb7a94):** `uwp/CMakeLists.txt` did not list `xaudio2`; the XAudio2
  backend's `rustc-link-lib=xaudio2` is only a request a staticlib records, so the host MSIX link
  would fail with unresolved XAudio2 symbols. Added `xaudio2` to the host link line.
- **Logging bridges (this commit).** The host only captures what flows through the Rust log
  callback; on a retail Xbox stderr has no sink and OutputDebugString needs a debugger, so Granite
  `LOGE/LOGW/LOGI`, SDL category logging, and Mesa-internal logs were all being LOST. Fixed the two
  PC-doable ones:
  - Granite/parallel-rdp: new `uwp/compat/g64_granite_log.cpp` installs a `Util::LoggingInterface`
    that forwards to `gopher64_uwp_log_native` (→ host log, `[rdp]` prefix). Installed at the top of
    `rdp_init` (interface.cpp). Caveat: Granite's hook is thread-local; this covers the rdp_init /
    command thread (single-threaded RDP), not Granite's internal worker threads.
  - SDL: `entry.rs` now calls `SDL_SetLogOutputFunction` (sdl_log_output → host log, `[sdl]` prefix)
    at startup. Previously SDL's own AUDIO/VIDEO logs went to stderr and were lost.
  - STILL OPEN (needs the mesa-uwp fork, console agent's territory): M-06 Mesa/Dozen driver-internal
    log hook. `dozen_loader.cpp` already logs the loader's own diagnostics, but not Mesa internals.
- **Static import audit (PC).** `llvm-nm` on the UWP staticlib (37 MB): no forbidden desktop DLLs
  (user32/gdi32/winmm/shell32/...); `winmm`/`timeBeginPeriod` absent (precise_sleep replaced
  spin_sleep successfully); only `LoadLibraryA` imported (the known benign Q-11 case, never called);
  `XAudio2Create` present (needs the host xaudio2 link above). The staticlib introduces nothing the
  Xbox rejects. The FINAL exe/DLL import-table audit still belongs to the console agent (needs the
  built MSIX + `scripts/uwp/audit-imports.ps1`).
- **Dozen WSI static review (vs mesa-uwp source).** No corruption bugs. Verified SOUND: ICD
  negotiation (version 5 path), 8-bit-storage env-var name/timing, external-host-off timing,
  begin/end_frame error handling, surface + extension list. Residual console-only risks the review
  could not rule out:
  1. `CreateSwapChainForCoreWindow` inside Mesa uses `CoreWindow::GetForCurrentThread()`; requires
     rdp_init on the CoreWindow-owning thread AFTER Activate(). The host DOES activate and present
     its own status frames (main.cpp:293,403) before calling run_rust (main.cpp:438) → rdp_init, and
     SDL_WinRTApp runs the entry point synchronously on the CoreApplication thread, so the ordering
     looks correct — but only real Dozen on console confirms it.
  2. Q-06: the host creates a D3D11 SDL status window first, then rdp_init creates the Dozen
     CoreWindow swapchain. Whether Dozen can create the CoreWindow swapchain after SDL's D3D11
     renderer released its own is unproven on console.
  3. `uwp_get_width/height()` dispatches on the CoreDispatcher and can hang/return 0x0 if the view
     is not yet visible. Instrument around `wsi->init_simple` / the swapchain create on Xbox.
  The new `[rdp]`/`[sdl]` log bridges are exactly what lets the console agent tell these three
  failure modes apart from the log.
- Verified on PC: UWP staticlib `cargo check` exit 0, desktop `cargo check --no-default-features`
  exit 0, `cargo fmt --check` clean. No Xbox test (cannot).

### Rendering bring-up guide, swapchain instrumentation, config C ABI — 2026-11-05

Opus agent, PC only. Prep so the console agent can attack first-frame rendering (the recurring
"initializes but nothing on screen" problem) with direction instead of trial-and-error.

- **New doc `19-rendering-bringup-xbox.md`** (linked from the P3 checklist): the three first-frame
  failure modes mapped to exact `[rdp]`/`[sdl]` log lines, the Xbox-proven activation/present rules
  (pump events early so CoreWindow::Activate fires; never run two swapchains on one CoreWindow;
  trust Mesa's STRETCH/HDMI-extent/no-waitable/no-tearing choices), and an ordered first-console
  test that does the D3D11→Dozen hand-over (Q-06) isolated and FIRST. Distilled from Ymir (D3D12
  CoreWindow, proven on Series X) and scummvm-uwp (documented the two-renderer deadlock as a
  "do not"). Note: Ymir does NOT use Vulkan/Dozen on Xbox (native D3D12), so it is an analogue, not
  a Dozen proof.
- **Swapchain instrumentation** (`parallel-rdp/wsi_platform.cpp`): log the surface extent on UWP
  (`Dozen: surface extent WxH`) — a 0x0/stale value is failure mode #3. Surface-create result and
  first-touched-frame logging already existed; together they cover all three modes.
- **Config C ABI for the P5 shell** (`src/ui/uwp/entry.rs`): `gopher64_uwp_config_get` (returns
  config.json text, "{}" if absent), `gopher64_uwp_config_set` (validates JSON then writes),
  `gopher64_uwp_last_error`, `gopher64_uwp_string_free`. Plain file I/O under the injected root,
  renderer-independent — the Settings page of the shell can be built on these now. Schema stays
  owned by src/ui/config.rs (it parses the same file at boot).
- **GUI recommendation:** the shell's logic is isolated from the emulator core, but its PRESENTATION
  is not — it shares the CoreWindow with Dozen, and the D3D11→Dozen hand-over (Q-06) is the proven
  failure mode. So the PC agent provided the renderer-independent piece (config C ABI) and the
  guide; the visual ImGui shell is best done on the console machine (VS2026 + reuse Ymir's shell:
  commits 9a42e339 "Add the Xbox shell, gamepad-first", bc575c0a gamepad nav, plus its file picker).
  Do NOT build the shell's game hand-over before Q-06 is validated on console.
- Verified on PC: UWP staticlib + desktop cargo check and fmt green. Config symbols compile; no
  Xbox test. No logic in the emulator core changed.

### Host build validation attempt from the Opus/PC machine — 2026-11-05

Tried to run the UWP host build (`scripts/uwp/build-host.ps1` path) on the Opus machine to catch
link/packaging errors before the console. Findings:

- **The WindowsStore v180 C++ toolset IS present** here (VS 18 BuildTools, `VC.Tools.x86.x64`):
  `cmake -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore` reaches MSBuild and
  processes the generated `.vcxproj`. So the host's toolchain gating is not the blocker here. (The
  dedicated `UWP.VC.v143` component query returns empty, but the v180 WindowsStore toolset covers it.)
- **The host CMake configure fails at the ImGui `FetchContent`** (`uwp/CMakeLists.txt:17-23`): it
  downloads ImGui from a github.com tarball URL, and that download fails on this machine
  (`download-imgui-populate.cmake:163 … downloading '…imgui…tar.gz' failed`). This is an environment
  network limitation on the Opus machine, not a code defect — the CMakeLists is syntactically fine up
  to that point. For a reliable/offline build, the console machine should ensure ImGui is cached or
  vendored (e.g. pre-populate `FetchContent` cache, or vendor the pinned ImGui under `uwp/extern/`),
  otherwise `build-host.ps1` needs working outbound network at configure time.
- Net: the Opus machine cannot complete the host MSIX build (ImGui fetch + final appx packaging are
  the gaps), so host build/package/F5 stays with the console machine. No code change resulted; this
  is a recorded environment finding. Probe build dirs (`out/uwp-host-probe*`) were deleted.

### Machine identity and handoff clarification — 2026-11-05 — YODA

- The user asked for machine-based context after pulling the latest Opus work. PowerShell's
  `COMPUTERNAME` reports `YODA`; the user identifies it as the VS 2026/Xbox-access environment.
  The other host's name is not recorded, so it remains unknown rather than being guessed.
- Updated `AGENTS.md`, `CLAUDE.md` and this journal to identify the host at the start of work and
  name it in future machine-specific memories and entries. The operating system already provides
  the name, so no additional `.env` or local configuration was introduced. Clarified that agent
  identity does not determine machine capabilities, and labeled the README toolchain observation
  and older journal state as historical. The later Opus probe found VS Build Tools despite earlier
  claims of no VS; its ImGui network failure and unverified MSIX packaging remain distinct.
- Reviewed the latest handoff, rendering bring-up guide and P5 design vision. XAudio2 and graphics
  logging/instrumentation are in place, but real audio, first frame, D3D11-to-Dozen hand-over and
  Xbox execution remain unverified. The P5 shell is a design, not an implementation. The user alone
  deploys and runs on Xbox; no console interaction was attempted.
- Verification: `COMPUTERNAME` identified YODA; the known local ROM directory exists, but its files
  were not inspected. `git diff --check` passed for the documentation changes. No build, VS toolchain
  check, game or console test was run; this was a documentation-only update. An existing untracked
  `out/` directory was left untouched. Recheck machine capabilities before relying on them and
  record the other host's actual name when observed there.

### User PC F5 Mario 64 hang and stale Rust artifact — 2026-11-05 — YODA

- The user selected Mario 64 on PC through VS 2026 F5 and reported a blank/stuck game window.
  The supplied VS Output shows timestamped host/Rust logs, so that debugger output route works in
  this run. The picker staged the ROM, the SDL frontend exited, Dozen initialized a 640x480
  swapchain, `rdp_init` succeeded, and the final application marker was `audio: SDL audio
  subsystem initialized; opening stream`. There is no subsequent application marker in the paste;
  this identifies the last observed stage, not proof of the precise blocking call or its cause.
  Gaming Input and Game Bar HRESULT diagnostics appear too, but do not establish this hang's cause.
- The shipped Debug EXE was built on Nov 5, while the Debug Rust staticlib it links was dated
  Nov 4. A byte scan of the actual AppX EXE found the old `audio: SDL audio subsystem initialized;
  opening stream` marker; it did not find the new `[rdp] ` or `XAudio2Create` strings. Current
  sources instead use the UWP XAudio2 shim and do not contain the old audio marker. The observed
  F5 run therefore exercised a stale Rust core, not the latest Opus audio/logging changes. VS's
  generated host project links the prebuilt staticlib; a VS-only build/F5 does not rebuild it.
- Added the required pre-F5 rebuild procedure to the P2 workflow. Stop debugging and run
  `pwsh -NoProfile -File scripts/uwp/open-vs.ps1 -Configuration Debug -SkipDependencies` from the
  repo root, then use the opened solution for the user's next PC F5 test. `-SkipDependencies`
  reuses installed SDL dependencies but does not skip Rust. If the build fails, retain and inspect
  its complete output before retrying. Do not uninstall the existing app or delete its LocalState.
- Verification here was read-only inspection of sources, scripts, artifact timestamps and the
  packaged EXE's byte strings; no rebuild, new F5, ROM execution or Xbox test was performed by
  the agent. The updated XAudio2 runtime, complete SDL/Granite log routing and first game frame
  remain unverified. Note: in the current code the XAudio2 shim has no detailed open-stage logs and
  a failed open panics with a generic message, so more targeted instrumentation may still be needed
  if a fresh build stalls or fails there. Preserve the user's untracked `out/` contents.

### VS builds Rust and YODA build storage moved to H: — 2026-11-05 — YODA

- User requested F5 builds the latest Rust automatically and approved junctions on H: to avoid F:
  running out of space. No agent launch or deployment to Xbox or PC was performed.
- Added a CMake VS project dependency that runs the UWP Rust build with `-SkipDependencies` before
  linking the host; Cargo handles incremental builds. Unsupported configurations fail the build.
  Explicit `build-host.ps1 -SkipRust` remains available by disabling project references for that
  already-configured command-line build; do not use it to test changed Rust. On VS itself, set
  **Always build** on Run and **Do not launch** on build/deployment errors (see the P2 handoff).
  These IDE settings are user-controlled and have not yet been verified here.
- The first isolated Debug build correctly blocked the host on a Rust failure: F: had only ~0.38 GiB
  free and staticlib creation failed with `No space left on device`. After user approval and closing
  VS, copied both `out` and `target` to H: and switched the original paths to junctions. No elevation
  was needed. Copies were compared by file size/timestamp; the two Rust libraries, VS AppX EXE and
  CMake cache also matched SHA-256. One copied incremental metadata file had a different timestamp
  but the same SHA-256; corrected its copied timestamp before comparison. Some VS index files
  disappeared from the source as VS closed during the first copy; their extra copies were preserved
  separately on H: under `gopher64-copy-extras` rather than deleted.
- The complete originals remain in `gopher64-build-backup-20261105` on F: and have not been
  deleted. F: reported about 10.66 GiB free after the work, up from ~0.38 GiB at the failure; the
  reason for that change was not established. The backups still occupy F: space; ask the user before
  removing them. H: is now required for builds.
- Validation on PC: Debug and Release Rust/host/MSIX builds passed in a separate build directory;
  an unsupported `RelWithDebInfo` build failed before the host; a repeated Debug build reran Cargo
  incrementally. Touching a Rust source timestamp rebuilt `gopher64.lib` and relinked the Debug host.
  The normal Debug `build-host.ps1 -SkipDependencies` succeeded with import audit, as did explicit
  `-SkipRust` without invoking Rust. The normal MSIX EXE SHA-256 matched the linked EXE and contains
  XAudio2 and `[rdp]` markers, not the old SDL audio-opening string. Builds emitted existing C/C++
  warnings, `LNK4075` and `APPX0006`; the import audit still warns Store/API and Xbox validation are
  outstanding. The new generated-project integration checks and existing import-audit checks passed;
  `git diff --check` passed. Build test logs are on H: under `gopher64-*-verification.log`.
- The command-line build did not deploy or refresh the existing `Debug/AppX` EXE. This is not proof
  that F5 deploy fails; layout refresh and the new runtime behavior remain for the user's manual PC
  F5 test. Reopen the regenerated `out/uwp-host-vs2026/Gopher64UWP.slnx`, check the two IDE settings,
  and capture the new VS Output through the last line after loading Mario 64. Do not infer the old
  hang is fixed until that test. The prior advice to rerun `open-vs.ps1` before every F5 is superseded
  by the project dependency; regeneration remains needed after generator/build-configuration changes.

### AppX rejected the new out junction — 2026-11-05 — YODA

- The user's F5 reported `DEP0700`, `0x80073CF0` and `0x800701C0`: opening `AppxManifest.xml`
  failed because the path contains an untrusted mount point. This occurred before game execution.
  The previously introduced repository `out` junction is in the existing solution's layout path.
- Microsoft's [RedirectionGuard explanation](https://www.microsoft.com/en-us/msrc/blog/2025/06/redirectionguard-mitigating-unsafe-junction-traversal-in-windows)
  states that AppX Deployment Service uses the mitigation and untrusted junction traversal can be
  blocked. This supports the diagnosis; the direct-path F5 is still needed to verify it here.
- `build-host.ps1` and `open-vs.ps1` now share `Get-UwpHostBuildDir`: when `out` itself is a link,
  use its resolved target and a separate `uwp-host-vs<version>-direct` build directory, preserving
  existing CMake caches and data. Otherwise retain the original output location. This does not
  resolve other links in ancestor directories; investigate those separately if encountered. The
  repository `out` and `target` junctions and F: backup were not modified; no security policy or
  junction trust was changed.
- Generated a fresh solution directly on H:, using the existing ImGui source cache, and successfully
  built the Debug host and MSIX with the normal script. Its generated Debug `OutDir` uses the physical
  H: path; each existing directory from the drive root to that output was checked and is not a link.
  Host path resolver tests (missing, normal, junction), generated VS dependency tests, import-audit
  tests and `git diff --check` passed. Existing compile/link/packaging warnings remain. One attempt to
  capture the build log failed with a PowerShell parser error before execution; the normal build
  command then succeeded. No package registration or app launch was attempted by the agent.
- Pending: the user must close the old solution and open the new `-direct` solution using the physical
  H: path, then F5 on PC. Only a successful registration can confirm the workaround; runtime ROM,
  audio and Xbox results remain pending.

### PC F5 reaches first scanout and DXIL exception — 2026-11-05 — YODA

- The user ran the new physical-path solution on PC: AppX registration and launch succeeded, so
  the preceding untrusted-junction failure is resolved for this path. The current XAudio2 and
  timestamped `[rdp]` logging were present; Mario 64 was selected and staged successfully.
- Evidence in order: D3D11 status frame presented; frontend exited; Dozen 25.3.3 detected RTX 4070
  SUPER; surface/swapchain and RDP initialization succeeded; audio and input initialized; emulated
  CPU loop entered. Compute-shader stall warnings reported `success: yes`. Then the VS debugger
  reported `0xC0000005` reading memory inside `dxil.dll`, called via `vulkan_dzn.dll`, from
  `vkCreateGraphicsPipelines` during `RDP::VideoInterface::aa_fetch_stage` / `scanout` / `render_frame`.
  The selected graphics program is `fullscreen.vert` plus `vi_fetch.frag` (the embedded fetch-bug
  variant is not yet known). This stack establishes where the exception surfaced, not which shader
  stage or component caused the invalid access. The pasted output says `Exception thrown`, not
  `unhandled`; whether continuing in the debugger recovers or terminates is pending user feedback.
  No successful game frame or audible game output was reported.
- The packaged validator reports file version `101.7.2308.24` and SHA-256 prefix `0DAF173C`; the
  paired ICD reports Mesa 25.3.3. A newer Microsoft-signed SDK validator is present locally
  (`1.9.2602.17`), but UWP compatibility and effect on this failure have **not** been tested; no DLL
  was swapped. The local Mesa source tree is not proven to match the supplied 25.3.3 binary, so its
  validation code is informative only, not a symbol-level identification of the fault.
- Reference: P0's four-game desktop success used Dozen 26.2.4, ubershader enabled and NVIDIA RTX 3500
  Ada (`14-p0-handoff.md`); a different desktop 26.2.6 failed. This UWP run reports neither an
  ubershader override nor a successful image. The renderer defaults ubershader off and the normal
  renderer source reads the CRT environment; differences from P0 are hypotheses, not a proven cause
  of this scanout graphics-pipeline exception. Do not silently disable VI filtering or abandon Dozen.
- Next diagnostic: if VS is paused on the exception, the user can continue once and report whether it
  is handled, repeats, or terminates. Only then choose a controlled PC A/B (one variable at a time,
  starting from the same ROM and renderer); explain the expected outcome before running it. No Xbox
  test and no runtime-code change were performed by the agent.

## 2026-11-05 - YODA: fatal scanout exception and isolated DXIL A/B

- The user continued in Visual Studio: the window was black and the exception became an unhandled
  `0xC0000005` at the same `dxil.dll -> vulkan_dzn.dll -> vkCreateGraphicsPipelines ->
  aa_fetch_stage` stack. No game image appeared. The old gray-window stall is not the current state.
- Hypothesis, not a diagnosis: the older validator may fail on DXIL emitted by Dozen. Prepared a
  separate **PC-only** Debug solution using Microsoft's signed Windows SDK `dxil.dll` 1.9.2602.17;
  the baseline solution and its DLL were not changed. Its UWP and Xbox compatibility is unverified.
- Experiment artifacts are under YODA's H: build area: the `dxil-1.9` experiment contains the
  candidate DLL and the same Dozen binary; `uwp-host-dxil-1.9-ab` contains the separate VS solution.
  The package uses the same development identity and version as baseline and shares LocalState;
  do not uninstall the existing app. Restore baseline by opening and running its direct solution.
- Checked: the experimental MSIX built successfully; Dozen and SDL DLL hashes match baseline and
  DXIL differs. The executable `.text` hashes match; complete executable and `.rdata` do not, so the
  builds are not byte-identical. The VS Rust dependency structural test passed. The candidate's
  PE imports include ETW-only `ADVAPI32` and release UCRT; this is not full API certification.
- The first configure attempt passed an unquoted PowerShell value `10.0`, cached it as `10` and
  failed. Left that agent-created directory untouched and configured a new `-ab` directory with
  the value quoted. The successful build emitted existing `getenv`, `LNK4075` and `APPX0006` warnings.
- **Pending:** the user should open the experimental solution, F5 the same Mario 64 ROM and paste
  output and any exception stack. The experiment is not a permanent fix and must not go to Xbox.

## 2026-11-05 - YODA: DXIL A/B passes crash; ubershader experiment

- The user tested the isolated DXIL 1.9 PC solution with the same Dozen: the prior access violation
  did not recur in the supplied log, the game reached its menu, but screenshots show severe tiled
  corruption and the user reports slowness. This is not a functional graphics or performance pass.
  The log confirms successful synchronous shader compilations, including graphics after the point
  where the old DLL crashed; it does not establish sustained FPS, cause of slowness, or audio output.
- Prior P0 evidence in `14-p0-handoff.md`: Dozen 26.2.4 default produced corruption; on NVIDIA,
  `PARALLEL_RDP_UBERSHADER=1` rendered SM64 correctly and passed 237 reduced RDP suites. Intel still
  corrupted and UWP currently uses Dozen 25.3.3, so those results do not prove this configuration.
- The UWP renderer defaulted ubershader to false and its log did not show an override. The UWP
  translation-unit wrapper now routes renderer environment lookups through the existing UWP shim,
  which forces the P0 ubershader setting; other platforms' renderer build is unchanged. This is an
  unverified workaround, not a root-cause claim. Check the runtime log for `Overriding ubershader = 1`.
- Prepared a separate PC-only `uwp-host-dxil-1.9-uber` solution in the H: build area. Its Debug
  MSIX built successfully; Dozen, DXIL 1.9 and SDL hashes match the preceding A/B package. The UWP
  shim's standalone Windows C++ assertions and VS dependency structural test passed. Build warnings
  remain, including `LNK4075` and `APPX0006`. No manual app or Xbox test was run by the agent.
- All generated solutions rebuild from the same current source and Rust library; reopening the old
  A/B solution and building now also picks up the ubershader change. Existing MSIX artifacts remain
  as historical evidence, but an old solution is not a stable way to rerun the previous baseline.
- **Pending:** user F5 with the new solution and the same ROM; verify the override log, image, audio,
  and whether speed improves after initial compilation. The candidate DXIL remains experimental;
  neither it nor the ubershader workaround is validated on Xbox.

## 2026-11-05 - YODA: ubershader improves SM64 image; Release comparison pending

- The user tested the isolated DXIL 1.9 + ubershader Debug build on PC. Screenshots of the SM64 logo
  and Mario title screen no longer show the prior severe corruption. The user reports 5–15 FPS and
  about 15–16 VI/s. This validates only those displayed scenes; no full rendering, audio or Xbox pass
  has been established. The user did not supply the ubershader run's Output log in this interaction.
- Prior P0 desktop evidence: a Release build with Dozen 26.2.4 and ubershader on NVIDIA sustained
  30 FPS / 60 VI/s in SM64's courtyard (`14-p0-handoff.md`). That was different hardware, Dozen build
  and desktop host; it is a useful benchmark, not evidence that the present UWP port must match it.
- The observed UWP run used Debug: `scripts/uwp/build.ps1` uses Cargo `dev` for Debug and `--release`
  for Release. Prepared Release in the same experimental solution, without changing source or DLLs;
  the build succeeded with warnings, including APPX0006. Debug and Release MSIX checksums match for
  Dozen, DXIL and both SDL DLLs. Cargo reported the Release profile as optimized.
- **Pending:** user selects Release/x64 in the same VS solution, reruns the same ROM and scene after
  initial shader compilation, and reports sustained FPS/VI/s, image, audio and relevant Output logs.
  If still slow, profile before attributing cost to CPU, GPU, logging, Dozen or the ubershader. No
  performance fix or Xbox validation is claimed; the DXIL DLL remains a PC-only experiment.

## 2026-11-05 - YODA: Visual Studio solution location clarified

- For the pending Release comparison, use the `uwp-host-dxil-1.9-uber` experimental solution under
  the physical H: build output, not the normal `uwp-host-vs2026-direct` solution with the old DXIL.
- The source repository remains on F:; `out` and `target` are junctions to H:. CMake generates the
  solution in the build tree. Opening the generated solution through the F: `out` junction caused
  AppX registration error DEP0700 / 0x800701C0; opening its physical H: location succeeded on PC.
- The user asked why the solution is not in the project folder. A future adjustment could place the
  generated solution in a non-junction repository build directory while keeping heavy artifacts and
  the registered AppX layout on physical H:, but that split has not been implemented or verified.
