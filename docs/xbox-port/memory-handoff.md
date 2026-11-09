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
- **W11-4NSYSB4** (the user calls it "Opus"; identified by `hostname` on 2026-11-09): the 2026-11-05
  Opus entries below were written here. VS 18 Build Tools with the WindowsStore v180 toolset,
  meson-uwp and Python 3.14 are present; on 2026-11-09 it configured and built Mesa Dozen. The
  ImGui download failed on 2026-11-05 and a complete host MSIX build has not been established here.
  No Xbox access. The `../mesa-uwp` checkout here has `origin` = `marcelofrau/mesa-uwp` (the fork).

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

## 2026-11-05 - YODA: Release audio investigation

- The user reports 30 FPS / 60 VI/s in Release, but periodic audio stutters and high pitch.
- The UWP source voice kept its open-time rate while the resampler ignored later N64 input-rate
  changes; unlike SDL stream conversion, this can play the source at the wrong speed and drain it.
  The shim now uses `input_rate / voice_rate * frequency_ratio`, validates stereo rates, and reports
  input/voice rates plus aggregate underrun, overflow and submit-failure counters every ~5 seconds.
  Formatting and logging are outside the audio lock and callback; the first interval includes startup.
- Queued samples still use the latest rate rather than a rate per segment. SDL's get-callback shim is
  also a no-op, and Rust may discard data before the shim: these are pending, not proven causes.
- A PC-only C++ test exercises the actual resampler, rate changes, invalid formats and counters;
  an isolated mutation restoring the old step failed the test as expected. Release packaging passed
  with warnings. Import-module audit passed but is not Store/API-level or Xbox certification.
- Found and fixed a Cargo dependency omission: `uwp/audio` was not tracked, so native-only changes
  could reuse a stale Rust library. An actual Release build after touching the header and again after
  touching the C++ file rebuilt the library and relinked the host both times. Structural test passed.
- **Pending:** user reruns Release/x64 using the same experimental solution, listens for pitch and
  stutters, and shares several `audio: input=` lines after startup. No real playback was run by the
  agent; output DLLs and renderer are unchanged, and DXIL 1.9 remains experimental on Xbox.

## 2026-11-05 - YODA: user confirms PC audio; sharing state

- The user reports that the audio issue is resolved in the PC Release test after the resampling fix.
  No new diagnostic counters or audio recording were supplied; this is a subjective user result for
  the tested SM64 run, not proof for all games, audio-rate transitions or Xbox.
- The working PC experiment combines Dozen 25.3.3 SHA-256
  `972C65469440AF3C7834A84CD2B55C87457E7DAA2E1BC660AC26180C01C2D272` with Microsoft SDK DXIL
  1.9.2602.17 SHA-256 `DB108FFEF7BCECB984BDCCF2B4725719D65B2CE7F1EBC40019DBE6C5A7016E09`.
  The DXIL file was locally installed and Microsoft-signed; no binaries or generated build trees
  are committed. The SDK file's redistribution rights and Xbox compatibility remain unverified.
- **Cross-machine build caveat:** the experimental VS solution and paired DLL directory exist only
  in YODA's build output. Default scripts can select the RetroArch pair with the older crashing DXIL;
  a Git pull alone does not recreate the working experiment. For a PC-only reconstruction, obtain
  the two inputs separately, verify the hashes above, set `GOPHER64_DOZEN_DIR` to their local
  directory and run `scripts/uwp/open-vs.ps1 -Configuration Release` (use `-SkipDependencies` only
  after dependencies are built). This generates a new solution; its runtime has not been reverified.
- The user authorized committing and pushing this work and indicated there is another issue to
  resolve before Xbox testing; its details have not yet been provided. No automated Xbox access or
  deployment is authorized.

## 2026-11-05 - YODA: UWP gamepad investigation

- The user reports that gamepad input does not reach the emulated game on the PC; Xbox is not tested.
- Ymir enables WGI before SDL initialization and opens gamepads on connection. The vendored SDL
  WGI driver defaults off, and `SDL_Quit()` clears hints between the Gopher64 frontend and game.
- Gopher64 defaults to no controller assignment; the frontend has no assignment UI. SDL events
  are already pumped, so the change does not alter rendering or frame scheduling.
- Read-only inspection of YODA's active UWP config confirmed player 1 enabled, default gamepad
  profile, and zero assigned slots. No config, ROM or save data was modified by this inspection.
- WGI and system-back hints are enabled before each SDL initialization. The first recognized
  gamepad binds to player 1 only when no explicit assignment exists; configuration is not saved.
- The UWP event watch handles mapped-gamepad additions; logs report enumeration, binding,
  disconnection, and Start presses with the mapped N64 Start bit.
- Release build, standalone policy tests, and import-module audit passed. The first build failed on
  missing UWP-only controller fields; all four initializers were fixed and the build passed.
- The virtual SDL enumeration, Start and hotplug test passed on desktop SDL. The UWP DLL did not
  load outside its AppX runtime; this test does not validate UWP, WGI, real input or Xbox.
- Pending: user tests the experimental Release on PC, reports input logs, Start, other controls,
  reconnect, graphics and audio; only the user performs Xbox deployment when ready.

## 2026-11-05 - YODA: consolidated primary solution

- The user requested one primary solution for tomorrow's Xbox test and clarification of DXIL.
  The direct VS 2026 host solution is the primary one; the old A/B solutions remain untouched.
- The tested pair was copied into the generated `out/uwp-dozen` directory and hashes verified.
  Build selection preserves explicit overrides, prefers complete local pairs, and warns before
  falling back to the legacy source. Incomplete local pairs are never silently selected.
- The entire generated `out/` tree is now ignored to prevent accidental commits of caches,
  binaries and local proof scripts. Existing contents were preserved, not deleted or modified.
- `open-vs.ps1` now builds Release by default; the user must still select Release in VS. Import
  audit requires both Dozen DLLs. The main Release MSIX passed the audit and its DLL hashes match
  the PC-tested pair; VCLibs and a development certificate are packaged alongside it.
- The Release deployment layout has not yet been created by F5; an older Debug layout still has
  the old DXIL. Neither layout was deleted or altered; select Release and let VS deploy it.
- The build still reports `APPX0006` for `runFullTrust`; import-module checks do not prove Xbox
  API, dynamic-load, certificate trust, Store, or console deployment compatibility.
- Fidelity review found new gamepad implementation in upstream files and historical budget
  violations. Recent logic was moved to port-owned modules with marked hooks; the historical
  exceptions remain. The unpushed gamepad subject was corrected; published history was not rewritten.
- Final staged-diff review passed for new changes; the historical fidelity budget remains exceeded
  against local `main`, and an authoritative `upstream/main` comparison is unavailable.
- Reviewer also noted a potential resampler phase edge case after underrun at step above one.
  The user reported working audio; no regression was established, so this is not changed here.
- After the relay refactor, the desktop SDL virtual gamepad test and a simulated scheduling-failure
  ownership/logging test passed. Neither proves WGI, real input, final PC playback or Xbox behavior.
  See [21](21-first-xbox-smoke-test.md) for the next manual test and log checklist.

## 2026-11-05 - YODA: publication requested

- The user authorized pushing the consolidated input and Xbox smoke-build commits to `uwp/main`.
  The remote was fetched and had no commits ahead of the local branch. Console testing remains pending.
- Fidelity review notes that the hook inventory still lacks `src/device.rs` and `src/ui/config.rs`;
  the historical over-budget exception remains open against the local `main` baseline.

## 2026-11-06 - YODA: preparing the first manual Xbox run

- The user returned with Visual Studio closed and requested instructions for remote debug/deployment.
  The primary direct solution and both local Dozen/DXIL inputs exist; the DXIL signature is valid.
- Microsoft's Xbox UWP setup specifies the C++ Remote Machine target, Universal (Unencrypted
  Protocol), and a console PIN on first pairing. Use the protocol only on a trusted network.
- No build, application launch, console access or deployment was performed. Actual controller,
  console rendering, audio, performance and deployment remain untested.

## 2026-11-06 - YODA: primary solution location

- The user asked why the primary solution is on H: rather than in the F: checkout. The checkout's
  `out` is a junction to H:, and opening the generated solution through that junction previously
  caused AppX error DEP0700 on the PC. The physical build path avoids that traversed junction.
- No build or console access was performed. A separate checkout entry point could be considered
  if requested, while keeping generated package paths physical; it has not been implemented.

## 2026-11-06 - YODA: portable generated solution attempt (superseded below)

- The user questioned portability and requested a junction-compatible workflow.
- Correction to the preceding entry: the observed failure was the path to the AppX layout/manifest,
  not proof that opening the solution via a junction alone fails.
- A PC-only synthetic package with its own identity registered through the physical path but failed
  through the checkout's `out` junction and a new junction with `0x800701C0`.
- The temporary test package was removed; its scratch files were retained in ignored `out`. No
  production package was registered, no security settings were changed, and no console was accessed.
- The primary solution is now generated under `.uwp-vs/vs2026-<checkout-key>` in the physical checkout; runtime
  output and the AppX layout use the physical target of `out`. Paths are resolved per machine,
  including ancestor junctions; the key also prevents reuse of generated caches after moving a
  checkout or copying it to another machine. The prior solutions were preserved.
- Release built and passed the package import audit; the VS recipe points its layout at physical
  output and the MSIX contains the same PC-tested Dozen/DXIL hashes. `-SkipRust` preserved the
  existing Rust library. Path, nested-junction, Dozen-selection and build-integration tests passed.
- Follow-up review found potential junctions inside generated directories, PowerShell 7.0 API
  incompatibilities and an integration-test gap; the scripts now check those paths and use older APIs.
- Tests covered relative symbolic links, generated-directory junction rejection and a bad recipe in an
  explicit build directory. Actual local PowerShell testing used 7.6.9; 7.0 was not available.
- Both Debug and Release compiled with the new split paths; neither was launched through VS yet.
- The new VS solution has not been run with F5. Real gamepad, console deployment, rendering and
  audio remain unverified; `APPX0006` and the experimental Xbox status of the DLL pair remain.

## 2026-11-06 - YODA: tracked root solution, approved simplification

- The user stopped the expanding cache/portability work and approved a normal versioned VS entry
  point. Canonical solution: `Gopher64UWP.slnx` at the repository root, referencing only the tracked
  port-owned `uwp/gopher64-uwp.vcxproj`. Both contain relative paths, no machine/drive assignments.
- The project runs `prepare-host.ps1` and the existing configuration-matched Rust build before host
  compilation/link. Fast up-to-date skipping is disabled; Rust failure blocks the host. The optional
  `build-host.ps1` and `open-vs.ps1` now build/open this same solution, not another generated one.
- Normal clones use normal `out`/`target` directories. YODA's existing junctions remain untouched.
  Build output is `out/uwp-host`; only MSBuild output/layout/manifest paths are resolved physically
  for AppX. No Windows security changes, console access, installation, or runtime launch occurred.
- Removed the uncommitted machine/checkout hash scheme and dependency-cache isolation changes.
  Prior generated solutions and scratch files remain preserved but are not active entry points.
  The experimental `uwp/CMakeLists.txt` is retained and explicitly marked legacy, not the VS workflow.
- The discarded dependency-cache probe failed during CMake's signed try-compile packaging with a
  269-character path. The exact cause was not established; no speculative workaround was applied,
  and the known-working dependency DLLs were not replaced.
- Native Debug and Release builds, MSIX generation and import audits passed on PC. Both recipes
  target physical output/layout/manifest paths. Rust compiled in both configurations; existing
  native/Rust warnings and `APPX0006` remain visible (Debug reported 484 warnings, zero errors).
- An isolated MSBuild failure test under a path containing spaces/apostrophe confirmed Rust failure
  stops host output. Path tests cover ordinary clone directories, ancestor junctions and unsafe
  AppX junction rejection. The project/recipe test checks both configurations and tracked entry point.
- The first static project test used an outdated target name and was corrected; the first failure
  harness used the wrong VS install-property name and was corrected. Neither was a product failure.
- Unrelated untracked `docs/assets/exported/` and `docs/assets/fonts/` appeared during this work and
  were left untouched. No renderer/audio/input behavior changes are part of this consolidation.
- Next manual action: open the canonical root solution, choose **Release | x64**, set remote Xbox
  debugging as described in [21](21-first-xbox-smoke-test.md), and let the user perform F5.
  F5, physical gamepad and Xbox compatibility remain unverified for this new project.
- Final read-only review found one portability mismatch: VS 2022 was still selectable while the
  native project required v145. The project now selects v143 under VS 2022 and retains v145 under
  VS 2026; the project contract test covers this selection. VS 2022 runtime/build is not claimed.
- MSBuild property evaluation on the actual installed VS 2026 and VS 2022 returned v145 and v143
  respectively. The final VS 2026 Release rebuild/package import audit also passed after this fix.
- A newer Rust library timestamp forced native host relinking. A junction-guard mutation was rejected
  by tests in an isolated copy, without changing production files. Both package configurations contain
  the exact same Dozen/DXIL hashes as the PC-tested pair. No other new review blockers were reported;
  the accumulated historical upstream-fidelity budget exceptions remain open.

## 2026-11-06 - YODA: restore ordinary local build directories

- The user reported another junction-related deployment problem and explicitly requested ordinary
  `out/` and `target/` directories inside the checkout after freeing local disk space. No new failure
  log was supplied; the exact remaining junction failure was not diagnosed or claimed fixed.
- After the user closed VS, the agent confirmed no VS/build/game processes remained. Copied 19,537
  output files (9,300,423,018 bytes) and 26,809 target files (17,608,356,973 bytes) to local staging.
  Robocopy reported zero failed files; a second read-only comparison found no size/timestamp/count
  differences. SHA-256 matched both Rust libraries, both host executables and the SDL/Dozen/DXIL DLLs.
- Four directory junctions inside discarded experiments were excluded from the copy. They and all
  original files remain untouched in the previous storage location. No source data was deleted.
  Removed only the two verified checkout junction entries and moved the verified copies into place;
  both trees now contain ordinary directories, with no directory reparse points. Copy logs remain
  in the local staging area. Approximately 9 GiB remained free after the builds.
- Debug and Release Rust/host builds, MSIX generation and import audits passed on PC. Both deployment
  recipes now point to the checkout's physical local output and manifest, not the former storage
  volume. Existing warnings, including APPX0006, remain. No app was installed or run on PC or Xbox.
- Preserved the user's VS-normalized root solution and unrelated assets without staging them.
  The static project test was too strict about two explicit Deploy elements; it now accepts VS's
  equivalent unconditional Deploy element while checking each configuration is enabled.
- The sole entry point remains the tracked root `Gopher64UWP.slnx`. The user must open it via the
  physical checkout path, select Release/x64 and retry their manual remote deployment. A separate
  workspace alias still exists outside the checkout; it was not changed. F5/Xbox success is pending.

## 2026-11-06 - YODA: user reports the physical-folder VS run appears successful

- The user reports they ran the project in VS after replacing `out/` and `target/` junctions with
  physical checkout directories and that it appears to have worked. No output log or exact
  run/deploy/game/controller details accompanied the report; record it as user-observed success,
  not as independent verification of ROM, input, audio, or full Xbox behavior.
- Opening the root solution in VS normalized `Gopher64UWP.slnx`: VS added its project ID and changed
  explicit per-configuration Deploy entries into one unconditional `<Deploy />`. Preserve that
  generated normalization in Git; the project integration test now confirms deploy remains enabled
  for Debug and Release. No unrelated asset files are included.
- Fetched `origin`; it remained exactly four commits behind local before publication. The remaining
  four local commits contain the tracked root solution, VS native host project, physical-directory
  deployment recipe and supporting docs/scripts. After validation, publish them together.
- After that push, the user explicitly authorized tracking the previously untracked GUI assets under
  `docs/assets/exported/` and `docs/assets/fonts/`; they were prepared in parallel for the future GUI.
  Inventory: 14 PNG assets (including eight navigation icons), two TTF fonts and one OTF font. Commit
  and push those assets separately; no asset contents were edited.

## 2026-11-06 - YODA: first Xbox RDP capability failure

- The user manually deployed and ran the Release app on Xbox. The frontend presented, the picker
  staged a ROM, Rust entered `rdp_init`, Dozen loaded and negotiated interface 5, and Vulkan found
  `Microsoft Direct3D12 (SraKmd_arden)` plus a software device. The swapchain was created before
  paraLLEl-RDP reported `16-bit storage for SSBOs is not supported` and the debugger broke inside
  `Renderer::init_caps`. The user continued and supplied a further stack, not a successful game frame.
- Surface diagnostics reported 1920x1080 while the swapchain capabilities reported 3840x2160;
  the created swapchain was 1920x1080. The relevance of this mismatch is unproven.
- Upstream Mesa 25.3.3 `src/microsoft/vulkan/dzn_device.c` queries D3D12 OPTIONS4 and gates Vulkan
  `storageBuffer16BitAccess` and `shaderInt16` on `Native16BitShaderOpsSupported`; 8-bit storage
  additionally depends on its 8-bit option. The packaged Dozen reports version 25.3.3, but its
  exact source and Xbox D3D12 query result have not been independently established. Do not equate
  the error with a proven hardware limitation or force unsupported Vulkan features.
- Independent reference [smash4-xbox](https://github.com/jajdp/smash4-xbox) reports a Series X
  where native 16-bit shader operations were unavailable, and its Mesa 26.2.x patch lowers shader
  arithmetic in that case. Its renderer and Mesa version differ from this port; lowering arithmetic
  alone does not establish Vulkan 8-/16-bit SSBO storage support for paraLLEl-RDP.
- Added a one-time UWP-only diagnostic before RDP construction, comparing raw Vulkan 16-/8-bit
  storage against Granite's selected-device values and logging raw `shaderInt16`. It does not change
  rendering or enable features. A fake Vulkan query test covers Vulkan 1.1 with/without 8-bit storage,
  Vulkan 1.2 and missing function handling. The user stopped their debug session before the PC build.
- The first Vulkan 1.1-extension test caught a diagnostic-formatting error (`raw8=-1` despite an
  extension query); fixed it and reran the test. The tests now have a dedicated PowerShell runner.
- Follow-up fidelity review moved the extension decision into port-owned code and tightened the
  tests for feature structure types, integration wiring and independently emitted fallback logs.
- Next: the user runs the root solution in Release on Xbox and supplies the `Dozen storage:` line
  and any subsequent error. If raw and Granite differ, inspect capability propagation; if raw is
  unsupported, investigate the D3D12 OPTIONS4 result and proven alternative implementations before
  deciding whether a different Dozen/RDP approach is required. No Xbox access by the agent.

## 2026-11-06 - YODA: Xbox raw feature result and D3D12 probe

- The user tested the diagnostic on Xbox: `raw16=0 raw8=0 shaderInt16=0 granite16=0 granite8=0`.
  The swapchain initialized, then RDP hit its explicit 16-bit SSBO requirement. Dozen and Granite
  agree, but this alone cannot distinguish failed D3D12 capability queries from a genuine driver limit.
- The same run's ROM staging took about 17 seconds; the file and storage conditions are unknown, so
  no regression is inferred. The console's audio, gamepad, gameplay and rendering remain unverified.
- Added a plan in [22](22-dozen-storage-investigation.md). Mesa 25.3.3's DXIL backend includes a
  32-bit narrow-access lowering pass, but its full RDP semantics are not yet proven. The separate
  smash4-xbox patch lowers 16-bit arithmetic; it does not establish SSBO storage support.
- Added a one-time D3D12 OPTIONS4 probe for the Vulkan device's LUID. It logs the failing stage and
  HRESULT or the successful native16 value, without enabling features. The user stopped debugging
  before the PC build; the agent did not access, install or run anything on the console.
- The first MSBuild failed because Cargo's library also links the new D3D12/DXGI calls; added the
  corresponding UWP link directives. A second build exposed a pre-existing clean-build issue:
  MSBuild supplied the Store CRT in `LIB` to the desktop Rust build script, causing unresolved
  `RoInitialize`, while direct Cargo succeeded. The Cargo child now uses the desktop CRT search
  directory and restores `LIB`; the final MSBuild host still uses the UWP CRT.
- After forcing `build.rs` to rebuild, both Release and Debug builds and package/module audits passed.
  The PC D3D12 diagnostic returned `OPTIONS4 hr=0x00000000 native16=1` for its selected adapter;
  mocked tests cover missing LUID and adapter lookup failure. Release contains the new diagnostic.
  No claim is made about API-level certification, Xbox support or runtime behavior yet.
- Import inspection shows the existing SDL3 already imports `CreateDXGIFactory1` and D3D12 ordinal
  101, as does the new host probe; SDL3 loaded successfully in the user's previous Xbox run. This
  does not prove that the new API calls or DXGI LUID lookup will succeed on Xbox.

## 2026-11-06 - YODA: Xbox D3D12 capability confirmed

- The user ran the diagnostic on Xbox: `raw16=0 raw8=0 shaderInt16=0 granite16=0 granite8=0`.
  The selected adapter's D3D12 OPTIONS4 query succeeded with `hr=0x00000000 native16=0`; RDP then
  stopped at its explicit 16-bit SSBO requirement.
- The independent D3D12 query succeeded and the tested Xbox UWP driver reported no native 16-bit
  shader operations. The Vulkan and Granite values agree. This does not reveal the HRESULT of the
  separate query inside Dozen or characterize the physical silicon outside that driver interface.
- Updated plan [22](22-dozen-storage-investigation.md) and reviewed the local Mesa UWP source without
  changing it. The checkout is clean on 26.2.0, with a remote 25.3.3 branch available. In both,
  the DXIL lowering mechanism is a candidate for 32-bit emulation but is not proven for RDP memory
  semantics. The independent Xbox reference changes arithmetic lowering, not the storage feature.
- Potential follow-up: build a separate experimental Dozen and validate narrow/aliased SSBO accesses,
  concurrent neighbouring writes and boundaries against native storage on PC before user Xbox testing.
  The current driver/DXIL pair, existing source checkout and user backups were not replaced.

## 2026-11-06 - YODA: early Xbox storage candidate

- The user preferred a direct Xbox test over a longer validation phase. The regular root solution
  remains the sole entry point; no alternate solution or automatic Xbox deployment was made.
- Built candidate Dozen 25.3.3 from Mesa UWP commit `f1ef4e6e645f364323b09f9cebbbde780347c5dc`.
  The exact patch is retained in the H: experiment directory; R4 requires a Mesa fork for integration.
- The candidate exposes narrow SSBO storage and forces Mesa's existing 32-bit lowering, including
  on the PC. It does not expose unsupported narrow arithmetic on the Xbox.
- Source, isolated build, Python environment, Khronos glslang and smoke tests live under the
  H: build storage. The existing Mesa 26.2.0 checkout was not modified.
- Failed attempts: solution target names containing `@` were rejected by MSBuild; direct project
  build then required PCH disabled and the source's `_XBOX_UWP` define. These changes are confined
  to the generated experimental build. The candidate was built with shader cache disabled.
- The first atomic replacement attempt failed because PowerShell passed an empty backup path; hash
  checks proved the original unchanged. A second atomic replacement succeeded with a local backup,
  after the user stopped debugging; the original is also independently backed up on H:.
- Original Dozen SHA-256: `972C65469440AF3C7834A84CD2B55C87457E7DAA2E1BC660AC26180C01C2D272`.
  Candidate SHA-256: `97306A5310B4FEEDA2B894800E5DBF5B6A2E8FB1095719118C8E072AC79CC0B1`.
- DXIL SHA-256 remains `DB108FFEF7BCECB984BDCCF2B4725719D65B2CE7F1EBC40019DBE6C5A7016E09`.
  The original Dozen is `out/uwp-dozen/vulkan_dzn.before-storage-test.bak` and in the H: backup.
- On the PC NVIDIA adapter, original and candidate each passed five separate-buffer and five
  aliased-buffer compute runs: 64 concurrent narrow writes, peer-thread reads and guard bytes.
- The shader did not enable `shaderInt8`/`shaderInt16`; incorrect writes and reads were rejected.
  The PC loader required the runtime directory in PATH. Tests do not establish full conformance.
- Release build and module-level audit passed; the generated MSIX contains the candidate hash.
  The old loose AppX layout still had the prior DLL before VS deploy; F5 must refresh the layout.
- Expected Xbox diagnostic if the candidate is loaded: `raw16=1 raw8=1`, while the independent
  D3D12 query may still show `native16=0`. First frame, correctness, saves and speed are untested.
- Fidelity review flagged the Mesa patch in the application repo as violating R4. It was moved
  back to H:; publishing it requires a Mesa fork and has not been done.
- The standalone PC test source, shader and runner are tracked in `scripts/uwp/tests/dozen-storage*`.
  The runner repeated both drivers and independently rejected wrong 8- and 16-bit outputs; the exact
  source/version, configuration and compiler commands are recorded in [22](22-dozen-storage-investigation.md).
- Follow-up review found the initial smoke test did not verify loaded module paths or force
  peer-thread reads. The test was corrected and repeated against both drivers; wrong shader results
  and wrong expected Dozen/DXIL module paths were rejected.

## 2026-11-06 - YODA: candidate Dozen compute exception on PC

- The user supplied a run with NVIDIA RTX 4070 SUPER and `native16=1`, therefore this log is from
  PC, not the earlier Xbox adapter. The runtime reported raw/granite 8/16 storage as available,
  initialized RDP, audio and input, and entered the emulated CPU loop. It then reported a first-chance
  `0xC0000005` read at address `0x30` inside `vulkan_dzn.dll`, during compute pipeline creation
  from `resolve_coherency_host_to_gpu`. The snippet does not establish whether the exception was
  unhandled, nor definitively identify the loaded DLL hash or the exact shader.
- The candidate binary has no PDB debug directory; no Mesa source location can be resolved from the
  pasted absolute addresses alone. The RDP function contains two compute dispatches, for
  `masked_rdram_resolve` and `clear_write_mask`; neither may be assumed to be the failing one.
- Extracted the exact baked SPIR-V for both shaders and created a temporary PC-only pipeline probe
  with matching specialization values (256, 256). Both shaders compiled successfully with both
  the original and the candidate drivers; a separate candidate run also enabled the reported
  8/16-bit arithmetic features. Initial original-driver probe loaded the wrong DLL because the
  executable directory took precedence; it rejected the mismatch, and was rerun from the correct
  directory. These tests do not reproduce the emulator's full device/pipeline state or prove a fix.
- Pending: confirm whether continuing the exception terminates or advances the game, obtain complete
  Output and module identity, then reproduce with real pipeline settings and/or produce symbols.
  Xbox validation of the candidate is still pending. No DLL was replaced and no Xbox was accessed.

## 2026-11-06 - YODA: user preparing Xbox candidate test

- The user intends to test the experimental candidate on Xbox despite the PC compute exception.
  Advised using a ROM without important saves and collecting the complete Output and any call stack.
  The current candidate and original backup hashes remain unchanged. The Xbox result is pending;
  the agent did not connect, deploy or launch anything on the console.

## 2026-11-06 - YODA: candidate tested by user on Xbox

- The user ran the candidate on Xbox. The Vulkan adapter was SraKmd_arden and reported
  `raw16=1 raw8=1 shaderInt16=0 granite16=1 granite8=1`; D3D12 OPTIONS4 still returned
  `hr=0 native16=0`. This is the experimental emulated storage exposure, not native support.
- The RDP initialized, audio and input initialized, one gamepad was detected and opened, and the
  emulated CPU loop started. The log does not establish a rendered game frame, audible audio or
  working physical buttons. A compute pipeline failed in `submit_span_setup_jobs` with pipeline
  hash `2f444097f40d7813`; the resulting RDP breakpoint stopped the session.
- The source identifies the shader as `span_setup` with four storage buffers and a uniform texel
  buffer. The candidate Dozen maps several failures to a generic Vulkan error: NIR-to-DXIL failure,
  DXIL validation failure and D3D12 CreatePipelineState failure may all map to out-of-host-memory.
  No cause can be inferred without the underlying stage/result.
- Extracted the exact baked `span_setup` SPIR-V and compiled it on the PC with the candidate,
  descriptor layout and non-upscaled specialization; pipeline creation succeeded. This does not
  reproduce the Xbox driver or the full emulator state and does not prove execution correctness.
  No changes to the app, deployed DLLs, or console were made during this investigation.
- After the user stopped debugging, built a diagnostic-only variation of the candidate. It logs
  failures for SPIR-V-to-NIR, NIR-to-DXIL, DXIL validation (including its message), and D3D12
  CreatePipelineState (including HRESULT) with the prefix `G64-DZN-DIAG`. Windows Mesa logging
  defaults to OutputDebugString, but emission of these failure messages on Xbox remains untested.
- The diagnostic candidate passed the PC narrow-storage smoke test; the Release package and
  module audit passed with 484 warnings and zero errors. The packaged Dozen SHA-256 is
  `820615746E8D53D0BB0F83BE680272EEC4CFE1AA2A5307D98A7206BFD4B5F93F`; DXIL is unchanged.
  The previously Xbox-tested candidate is preserved as `out/uwp-dozen/vulkan_dzn.before-stage-diagnostic.bak`;
  the original driver backup is also untouched. No Xbox deployment was performed by the agent.
- Next: the user deploys the root solution in Release and provides the complete Output around
  the failure, including `G64-DZN-DIAG` and `VALIDATION ERROR` if present. This build is diagnostic,
  not a fix. Continue to treat game rendering, controller buttons, audio and saves as unverified.

## 2026-11-06 - YODA: Xbox DXIL validation failure

- The user tested the stage diagnostic on Xbox. Storage was reported enabled, D3D12 still reported
  native16=0, RDP initialization succeeded, audio and input initialized, and the emulated CPU
  started. `span_setup` compute pipeline `2f444097f40d7813` then failed with
  `G64-DZN-DIAG: DXIL validation failed: no validator message`. No first game frame was confirmed.
- In Mesa 25.3.3 `dxil_validator.cpp`, a failed `GetStatus` result is not checked and resolving
  the error text requires optional `dxcompiler.dll`/`IDxcLibrary`; that module was not observed
  in the supplied partial log. A null validator, failed API call or unsuccessful validation must
  be distinguished. The Xbox failure is not proven to be the earlier PC access violation.
- In the isolated Mesa experiment, added diagnostics for validator null, `Validate` and `GetStatus`
  HRESULTs, validation status and bounded UTF-8 error-buffer output when available. API failure and
  null result are now handled without dereference; validation itself remains enabled. This is not
  a Mesa fork integration and the scratch sources and previous source backups remain on YODA.
- The revised Dozen SHA-256 is `CAAAF54269C1A9591C3DC4792327D6FC950A58FBFB0299CCA09497346E9BC65A`.
  After the user stopped debugging it was placed in the host input, with the prior diagnostic DLL
  preserved as `out/uwp-dozen/vulkan_dzn.before-validator-diagnostic.bak`. The experimental
  storage candidate and original Dozen backups were also preserved; DXIL was not changed.
- Release built with 484 warnings and zero errors; the module-level import audit passed, and the
  generated MSIX contains the new driver hash and the same DXIL hash. PC Vulkan storage smoke
  tests passed, including negative controls; the diagnostic failure branches and Xbox are untested.
  The user alone will run Xbox F5 and provide `G64-DXIL-DIAG` and `G64-DZN-DIAG` lines.

## 2026-11-06 - YODA: PSV wave-size mismatch and candidate fix

- The user tested the diagnostic build on Xbox. The validator returned `0x80aa0013` and reported
  a `PSVRuntimeInfo` mismatch: one side had minimum 0 / maximum 4294967295, the other `WaveSize=64`.
  The UTF-8 error buffer had 898 bytes; the diagnostic logged only the first 800. The validator
  rejected the shader; the failure was not caused merely by missing dxcompiler.
- The public DirectXShaderCompiler v1.8.2505.1 validation source prints regenerated module state
  as the PSV0 argument and the actual PSV as the module argument in this mismatch diagnostic. The
  exact source of the installed validator is unverified; its displayed labels need not identify origin.
- Mesa 25.3.3 emits explicit wave-size metadata only with DXIL minor version 6 or higher, but its
  PSV builder sets fixed lane counts without that guard. On YODA a temporary test-only hook forced
  minor version 5 and subgroup 64; the validator reproduced the same mismatch. Adding the missing
  `minor_version >= 6` guard to PSV generation made that PC pipeline compile. This simulation did
  not independently establish the Xbox shader model or prove correct shader execution.
- Removed the test-only forcing hook before the final build. The candidate retains validation
  diagnostics and adds the guard; its source and build remain in the local Mesa experiment, not the
  application repo per R4. Dozen SHA-256 `CFD9F03407EFEA60E2BFE09DCB9D327A070150A3D3129A8B444548AFA9450781`
  is in the host input; the prior diagnostic is in `out/uwp-dozen/vulkan_dzn.before-psv-fix.bak`.
  DXIL remains unchanged. The user stopped debugging before the replacement.
- The final candidate passed the PC storage smoke test and negative controls. Release package and
  module audit passed; the package's Dozen hash matches. The build had 484 warnings and zero errors.
  The user alone can perform the next F5 on Xbox; rendering, correctness, audio, input, saves,
  performance and full Vulkan semantics remain unverified. No console access by the agent.

## 2026-11-06 - YODA: subgroup size incompatibility

- The user tested the PSV candidate on Xbox: the previous `span_setup` compute pipeline compiled
  (`2f444097f40d7813`, `success: yes`), then tile binning stopped with `Subgroup size configuration
  not supported`. No game image, sound, physical control behavior, or performance was confirmed.
- In the pinned paraLLEl-RDP source, `supports_subgroup_size_control` checks `computeFullSubgroups`
  and size properties but not Granite's `subgroupSizeControl` feature or force-disable quirk. The
  subsequent Granite `supports_subgroup_size_log2` checks those additional conditions. Mesa 25.3.3
  reports `computeFullSubgroups=true` while gating `subgroupSizeControl` on shader model >= 6.6 and
  WaveOps; the exact value and rejecting condition in this Xbox run were not directly logged.
- Added a UWP-only pre-RDP policy: query Granite's actual support for the range requested by tile
  binning and select the already-baked upstream `PARALLEL_RDP_SUBGROUP=0` shader variants when
  unsupported. This disables subgroup optimization, not Vulkan, Dozen or the ubershader. The app
  logs the choice explicitly; it does not advertise any additional GPU features.
- Tests verified supported vs unsupported 32/64 sizes, resetting the policy across reinitialization,
  environment overrides and sharing across translation units. Release and package/module audit
  passed with 484 warnings and zero errors; the MSIX contains the policy and the same PSV candidate
  Dozen hash `CFD9F03407EFEA60E2BFE09DCB9D327A070150A3D3129A8B444548AFA9450781`.
- The user stopped debugging before the build. Only the user will deploy and run the next Xbox F5.
  Logs, visual correctness, performance, actual audio, controls and save integrity are unverified;
  fallback selection may be slower. Existing unrelated asset edits were not staged.

## 2026-11-06 - YODA: end-of-day Xbox black-screen handoff

- The user manually tested the latest Release on Xbox, then asked to stop for the day, publish all
  changes including their icon, and leave the other agent enough context to investigate. The user
  reported no debugger stop or visible game progress; the game window remained black. The log still
  contains first-chance `_com_error` messages and system HRESULTs; there is no evidence they caused
  the later black screen and no unhandled exception is reported in the supplied excerpt.
- The console run reports `raw16=1 raw8=1`, `shaderInt16=0`, D3D12 `native16=0`, followed by
  `subgroup size control unavailable; using RDP non-subgroup shaders` and `Allow subgroups = 0`.
  RDP initialization succeeded, audio initialized, one SDL gamepad was detected and opened, and
  the emulated CPU loop was entered. Compute shader hashes `2f444097f40d7813` and
  `eb43f456d4421d5b` compiled successfully; the last log is the second compile success at 17:56:02.
- This does not establish a rendered game frame, responsive gameplay, audible sound, working
  controller buttons, a deadlock or its cause. Both warnings are synchronous successful shader
  compilations. The NVIDIA PC test of the earlier storage candidate stopped with an access violation
  in Dozen; it was not proven equivalent to this console black screen.
- Next investigator: inspect the full sequence in [22](22-dozen-storage-investigation.md), identify
  whether CPU execution, GPU submission, first scanout/present or the event pump progresses, and use
  bounded diagnostic logging and user-collected debugger thread stacks. Do not assume which wait or
  thread is responsible. Only the user may deploy to or run on Xbox.
- The application repository includes the tracked root `Gopher64UWP.slnx`, Rust build integration,
  UWP subgroup policy and PC storage smoke tests. The experimental Dozen source changes and the
  tested DLL remain **local to YODA**, not versioned or uploaded: R4 requires a Mesa fork, and no
  user-owned Mesa fork is configured. The second machine needs that fork/artifact before it can
  reproduce the exact candidate. Source changes and build recipe are described in
  [22](22-dozen-storage-investigation.md).
- The active Dozen SHA-256 is `CFD9F03407EFEA60E2BFE09DCB9D327A070150A3D3129A8B444548AFA9450781`;
  DXIL remains `DB108FFEF7BCECB984BDCCF2B4725719D65B2CE7F1EBC40019DBE6C5A7016E09`. Keep the prior
  candidate and original driver backups; do not delete or silently replace them. The YODA build
  drive had approximately 8.86 GiB free when this handoff was written.
- The user explicitly requested including their modified 24x24 unplated app icon. No additional build,
  application launch or Xbox test was started during this closeout.

## 2026-11-09 - W11-4NSYSB4: Xbox hang review, Mesa fork series, bring-up diagnostics

Host `W11-4NSYSB4` ("Opus"). No Xbox access; YODA was unavailable, so YODA-only steps are listed
as pending at the end. Reviewed YODA's 2026-11-06 work after the Xbox black-screen run.

**Reading of the last Xbox run.** The user waited, heard no sound and the app looked frozen or
extremely slow; in an earlier session an exception appeared only after a long wait. The XAudio2
counters (`audio: input=`) are logged every ~5 s from `g64_audio_put`, i.e. the emulator thread;
none appeared. So this is most likely a hang or a crawl, not "renders black while running". M-01
alone would leave sound playing. Treat both as hypotheses until the diagnostics below report.

**Do we need Mesa? Yes, for paraLLEl-RDP.** Research this session:
- paraLLEl-RDP hard-requires `storageBuffer8BitAccess`/`storageBuffer16BitAccess`
  (`parallel-rdp/.../rdp_renderer.cpp:168-178`), no fallback; `shaderInt8/16` are optional.
- Xbox reports `native16=0`; stock Dozen ties storage to it (`../mesa-uwp/src/microsoft/vulkan/dzn_device.c`).
- No workspace sibling runs Vulkan on Xbox (Cemu, XBSX2, dolphin, xenia, xemu: native D3D12;
  Azahar and scummvm: Mesa for GL only). Every external Dozen-on-Xbox project patches Mesa.
- The only paraLLEl-RDP-on-Dozen success found is desktop "Vkon12", fixed once 8/16-bit storage was
  emulated (Themaister/parallel-rdp issue #56). No public paraLLEl-RDP-on-Xbox evidence was found.
- So YODA's direction (emulate narrow storage in Dozen) is right. Gaps found in its execution:
  it diverged from D-17 (local 25.3.3 patch, no fork, not reproducible: R4), went to Xbox with an
  unexplained PC access violation in the same lowering path, and never applied M-01.

**Likely root cause of both the PC crash and the Xbox hang (inferred, not yet proven).** Dozen calls
`nir_to_dxil(nir, &opts, NULL, ...)`. With a NULL logger, nir_to_dxil's default logger prints to
stderr and runs `UNREACHABLE()`, which in MSVC release is `__assume(0)`
(`src/util/macros.h:131-136`): undefined behaviour on any unsupported instruction, instead of a
clean failure. Narrow-storage lowering is exactly the kind of change that hits such paths. Also,
Dozen's `lower_int16` only turned on when the app enabled shaderInt16/Float16/float16_int8; RDP
enables none on Xbox, so 16-bit storage types went unlowered (same trap smash4-xbox documented).

**Mesa fork series** — `marcelofrau/mesa-uwp`, branch `gopher64/dzn-xbox`, from `uwp-26.2.2`
(`34732bbc207`) per D-17. Local commits on W11-4NSYSB4, **not pushed yet** (push needs the user's OK):
1. `e0b7050c266` dzn: real logger for nir_to_dxil (no UNREACHABLE UB); log NIR-to-DXIL stage,
   silent validation failures, and CreatePipelineState HRESULTs.
2. `c0b607c9d6c` util/log: honor `MESA_LOG_FILE` on Windows (minimal M-06).
3. `0d32c663299` dzn: `DZN_DEBUG=no16bit` clears Native16BitShaderOpsSupported, so a desktop GPU
   reproduces the Xbox caps exactly (unlike YODA's "force lowering on all GPUs", which still
   exposed native 16-bit arithmetic on PC).
4. `fe2d56c90ca` dzn: expose storageBuffer16BitAccess / storageBuffer8BitAccess without native
   16-bit ops; enabling either turns on `lower_int16`. Arithmetic, UBO and push-constant narrow
   access stay tied to native support. Reimplemented, not copied (Cosmo's tree is unlicensed).
5. `d6e68972f0a` compiler: PSV fixed wave lane count only for DXIL 1.6+ (YODA's validated fix).
6. `364c8b1e0a9` dzn: under `_XBOX_UWP`, plain host-visible memory stays WRITE_COMBINE; only
   HOST_CACHED (readback) gets WRITE_BACK (M-01, OnyxEMU 0014 idea, reimplemented).
Not ported from YODA: its extra dxil_validator HRESULT diagnostics (its source is on YODA only).

**gopher64 diagnostics (this repo).** New port-owned `uwp/compat/g64_diag.{h,cpp}`, hooked only in
existing `#ifdef GOPHER64_UWP` blocks of `parallel-rdp/interface.cpp` plus one gated line in
`rdp_close`, compiled from the `uwp` block of `build.rs`:
- `R-02 cpu-write visibility: ok|FAIL|SKIPPED` — once after WSI init: CPU writes a pattern through a
  Host-domain mapping, GPU copies it Device and back to CachedHost, compared on the CPU; fence wait
  bounded to 5 s so a GPU hang is reported, not inherited.
- `heartbeat: first frame presented`, then `heartbeat: frames=N (+n in the last ~2 s)`.
- `watchdog: no frame presented yet after N s` / `...for N s` from a background thread, every 5 s.
- Host (`uwp/host/main.cpp`) sets `MESA_LOG_FILE` to `LocalState/gopher64-mesa.log` before Dozen
  loads; it only fills with a Dozen built from the fork (patch 2).
UWP staticlib `cargo check` passed on W11-4NSYSB4. No PC F5, no Xbox run.

**Dozen built from the fork on W11-4NSYSB4.** Release `vulkan_dzn.dll` + `vulkan_dzn.pdb` from branch
`gopher64/dzn-xbox` at `364c8b1e0a9`, SHA-256
`B8E5FA4EE0FD8F8DAF2FF0905D62B52F2F9B5FB94121D8E4C7F91593E90D58DD` (local build output, not
committed, not shipped). Recipe = doc 22 §Reconstructing, plus two build-environment defines in the
generated `Directory.Build.targets`: `_SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS` (MSVC
14.51 turns STL1011 in `uwputil`'s `<experimental/coroutine>` into an error) and PDB generation
(`DebugInformationFormat=ProgramDatabase`, `GenerateDebugInformation=true`) so crashes symbolize.
Python deps went into an isolated venv under `out/` (mako 1.3.10, PyYAML 6.0.3, packaging 26.0,
PLY 3.11). `dumpbin`: exports the three `vk_icd*` entry points; imports only App CRT (`*_APP.dll`)
and UWP API sets (plus ole32/oleaut32); no user32/gdi32. No `dxil.dll` is built: keep the
Microsoft SDK DXIL 1.9.2602.17 pair already used on YODA. Not run on any GPU yet.

### Pending for YODA (user will relay; do in order)

1. `git pull` gopher64 `uwp/main`, and in `../mesa-uwp` fetch `origin` (`marcelofrau/mesa-uwp`) and
   check out `gopher64/dzn-xbox` (after the user approves pushing it from W11-4NSYSB4).
2. Push YODA's local experimental Mesa patch (25.3.3, incl. the dxil_validator HRESULT diagnostics)
   to a temp branch of the fork, e.g. `yoda/storage-experiment-25.3.3`, so it is versioned (R4).
   Do not rebase the fork series onto it; compare and port anything still missing.
3. Build `vulkan_dzn.dll` + `.pdb` from `gopher64/dzn-xbox` (doc 22 recipe + the two defines above)
   and put it with the existing DXIL 1.9 in `out/uwp-dozen/` (back up the current candidate as
   `vulkan_dzn.before-fork.bak`; keep all older backups).
4. **PC first, with `DZN_DEBUG=no16bit`** (process env, e.g. VS Debugging > Environment, or a
   temporary `SetEnvironmentVariableW` before `LoadPackagedLibrary`): SM64 Release F5 on the NVIDIA PC.
   Expected: `raw16=1 raw8=1 shaderInt16=0`, `R-02 cpu-write visibility: ok`, heartbeat lines.
   Collect `gopher64-host.log` and `gopher64-mesa.log` from LocalState. If it crashes, the PDB now
   gives a symbolized `vulkan_dzn` stack; if a pipeline fails, `gopher64-mesa.log` should contain
   `nir_to_dxil:` / `CreatePipelineState failed` lines. Do not go to Xbox until the PC with
   `no16bit` reaches the SM64 title screen.
5. Only then: the user runs the same Release on Xbox (no `DZN_DEBUG` needed there; native16 is
   already 0). Collect both logs. If it still hangs: VS **Break All**, copy all threads' stacks.
   Read the result: `R-02 FAIL` -> M-01 incomplete; `nir_to_dxil:`/PSO lines -> a lowering gap;
   heartbeat advancing but slow -> M-04/M-05 (PSO stalls, pipeline library) and re-check the forced
   ubershader; no heartbeat + watchdog lines -> hang, use the stacks; device removed -> M-03.
6. Record results here with the host name, and fill the P3 checklist (M-01/M-06/M-09 items).

## 2026-11-09 - W11-4NSYSB4: unified logging and crash capture (supersedes parts of the entry above)

The user asked for maximum diagnostic information for YODA's tests, in a file and in the VS Output
window. Review found that on UWP the 86 upstream Rust `println!`/`eprintln!` calls, 12 C/C++
`printf`/`stderr` calls in the renderer glue, Granite's fallback on non-RDP threads and the default
SDL and Mesa stderr loggers were all lost. There was no crash capture, and the log only grew.

**Logging framework.** A Rust `log`/`tracing` crate would have meant editing the 86 upstream prints,
which breaks fidelity. spdlog would have been a new dependency to do what the host already did.
So there is no new framework. Instead, `uwp/host/host_log.{h,cpp}` (port-owned) implements the
doc 06 §6 plan, with the redirect technique taken from Ymir:
- one sink: file, plus VS Output only when a debugger is attached;
- `[t<tid>] [tag]` lines;
- rotation to `gopher64-host.prev.log`;
- stdout/stderr redirected through a Win32 pipe with `_dup2` **and** `SetStdHandle`. Rust std writes
  via `GetStdHandle`, so `_dup2` alone would still lose `println!`;
- an unhandled-exception filter that writes `[crash] code, address, module+offset` with a raw
  `WriteFile`. `AddVectoredExceptionHandler` is not in the UWP API partition; the APP-family
  compile check caught that;
- `LocalState/env.txt` KEY=VALUE overrides.

Other changes:
- `uwp/host/main.cpp` uses the sink and logs package version, device family and host config.
  It no longer sets `MESA_LOG_FILE`: Mesa's stderr now lands in `gopher64-host.log` under
  `[stdio]`, on the same timeline. `MESA_LOG_FILE` stays available through env.txt.
- `src/ui/uwp/entry.rs` stops duplicating lines (`eprintln!` only without a host callback) and
  logs `GIT_DESCRIBE`.
- `uwp/compat/dozen_loader.cpp` logs path, size and SHA-256 of the loaded `vulkan_dzn.dll` and
  `dxil.dll`.
- The log reading guide is in [13 §2](13-testing-debugging.md#2-logging).

**Verified on W11-4NSYSB4.**
- `host_log.cpp` compiles with `WINAPI_FAMILY_APP`.
- A desktop test exe checked CRT stdout/stderr, Win32 `GetStdHandle` writes (Rust's path), a
  worker thread, a line without newline, env.txt parsing, rotation, and an unhandled access
  violation. That last one logged as `[crash] unhandled exception 0xC0000005 at ... (gopher64-uwp.exe+0xf9d8)`.
- The tracked runner `scripts/uwp/tests/host-log.tests.ps1` needs `Get-UwpVisualStudio`, which this
  machine lacks, so the same steps were run here with VsDevCmd directly.
- UWP staticlib check, `cargo fmt --check` and desktop `clippy --no-default-features -Dwarnings`
  pass.
- Not built into the MSIX here (no UWP VS component). YODA's first build will be the first
  full-host compile of `main.cpp` with these changes.

**Corrections to the YODA list above.**
- In step 4, set `DZN_DEBUG=no16bit` with a `LocalState/env.txt` line instead of VS settings.
- Mesa/Dozen errors (`nir_to_dxil:`, `CreatePipelineState failed`, validation) are in
  `gopher64-host.log` as `[stdio]` lines. A separate `gopher64-mesa.log` only exists if
  `MESA_LOG_FILE` is set in env.txt.
- Also run `pwsh -NoProfile -File scripts/uwp/tests/host-log.tests.ps1` on YODA.
