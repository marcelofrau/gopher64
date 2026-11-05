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

## Agents and machines

This port is worked on from more than one machine by more than one agent. Their environments and
strengths differ, so match the task to the agent. The user coordinates between them via this file
and git; agents do not talk to each other directly.

- **Opus agent (this one, "deep-analysis" machine).** Strong at multi-file investigation, fidelity
  review, root-cause work and keeping the upstream diff minimal/gated. Environment limits: **no
  Visual Studio 2026 and no Xbox access** here — it can build the Rust staticlib, the SDL/XAudio2
  C/C++ via clang-cl, run `cargo` (fmt/clippy/test) and reason about logs, but it **cannot build
  the UWP host MSIX via MSBuild/VS, cannot F5, and cannot run anything on the console**. Use it for
  design, fidelity cleanups, tricky bugs, and anything provable on the PC with cargo/clang.
- **Secondary agent ("build-and-console" machine, e.g. a lighter GPT model).** Shallower analysis,
  but has **Visual Studio 2026** (builds/packages the host MSIX, runs `scripts/uwp/build-host.ps1`,
  opens the VS solution) and can **ask the user to deploy and F5 on the Xbox and bring back console
  logs and runtime feedback**. Use it for host/packaging work, VS-specific issues, and for getting
  real Xbox execution evidence that the Opus machine cannot produce.
- **Division of labour in practice:** Opus lands minimal, gated, fidelity-clean code and proves it
  on the PC (link + desktop regression); the secondary agent takes that, builds the MSIX, and
  drives console validation with the user. When Opus writes something that only the console can
  confirm (audio playback, Dozen frames, lifecycle), it says so explicitly and leaves it for the
  console machine to verify — never claims an Xbox result it could not have observed.

## Current state — 2026-11-04

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
