# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

This is a fork of [gopher64/gopher64](https://github.com/gopher64/gopher64), a cross-platform N64 emulator written in Rust, being ported to Xbox Series Dev Mode (UWP). The remotes are:

- `origin`: `marcelofrau/gopher64`
- `upstream`: `gopher64/gopher64`

The port's plan, decisions, risks and checklist live in `docs/xbox-port/`. Start with `docs/xbox-port/README.md`. The phase checklist is in `docs/xbox-port/10-roadmap-checklist.md`.

## Binding rules for the fork

`docs/xbox-port/00-upstream-fidelity-rules.md` is mandatory. It has the full rules plus a hook inventory you must keep up to date. The essentials:

- **Branches.** `main` is a fast-forward-only mirror of `upstream/main`. Port work goes on `uwp/main` or on `uwp/<topic>` branches.
- **Port-owned paths.** Put port code in `uwp/`, `scripts/uwp/`, `src/ui/uwp/`, `docs/xbox-port/` and `.github/workflows/uwp.yml`. `src/compat/` belongs to upstream; don't add files there.
- **Upstream files get hooks only.** Each hook is a minimal change gated with `cfg(target_vendor = "uwp")`, `#ifdef GOPHER64_UWP`, or `CARGO_CFG_TARGET_VENDOR == "uwp"` in `build.rs`. It delegates to port-owned code and carries a `// UWP-PORT: <reason> (D-xx)` marker.
- **Leave upstream code alone.** Never refactor, reformat, rename or reorder it. There is no `.clang-format` upstream, so never run clang-format on upstream C/C++.
- **Submodules.** Never change submodule pointers yourself; only take them through upstream merges. The submodules are `parallel-rdp/parallel-rdp-standalone`, `retroachievements/rcheevos`, `src/compat/sse2neon` and `android-project/libadrenotools`.
- **Desktop and Android behaviour is unchanged.** Config, save and savestate formats stay identical.
- **Dependencies.** Dependencies, `Cargo.lock` and `rust-toolchain.toml` follow upstream. UWP-only dependency differences go in a `[target.'cfg(target_vendor = "uwp")'.dependencies]` table.
- **Syncing.** Sync with `git merge main` into `uwp/main`. Never rebase or force-push `uwp/main`.
- **Diff budget.** Stay within 20 upstream files and 400 lines.
- **Commit messages.** Prefix port commits with `uwp:` and reference the decision ID they implement.
- **No upstream PRs.** Upstream's README forbids PRs that are vibe-coded or co-authored by AI agents. Never open PRs or issues against `gopher64/gopher64` from agent-written code. Upstreaming candidates are only *proposals*, and the maintainer of this fork writes them by hand.
- **No absolute machine paths.** Docs, scripts, agents and config never contain absolute paths. The workspace is the parent directory, so refer to sibling repos as `../Ymir`, `../mesa-uwp`, `../SDL3-uwp`, and so on. Paths inside this repo are relative to the repo root.
  - Locate tools at runtime: `vswhere`, `PATH`, `rustup`.
  - Machine-specific locations come from environment variables, e.g. `GOPHER64_ROMS`.
  - Anything that needs absolute paths, such as Cargo `links` overrides, is generated at build time into a git-ignored file.
- **Git identity.** Commits must be authored and committed as `Marcelo Frau <marcelofrau@gmail.com>`. Never use a corporate identity. A PreToolUse hook enforces this (`.claude/hooks/check-git-identity.sh`). Check `git config user.name` and `git config user.email` before committing.

## Project agents (`.claude/agents/`)

- `fidelity-reviewer`: read-only check of a diff against the fidelity rules. Run it before every commit or PR to `uwp/main`, and after every sync.
- `upstream-sync`: fetches, fast-forwards `main`, merges into `uwp/main` and runs the checks. It never pushes; it hands you the push commands for approval.
- `port-researcher`: finds how a UWP/Xbox/Dozen problem was already solved, in the workspace reference ports (Ymir, SDL3-uwp, mesa-uwp, …) or in public Dozen-on-Xbox projects.

## Commands

Upstream CI (`.github/workflows/lint.yml`) runs these checks, and they must keep passing on `uwp/main`:

```bash
git submodule update --init --recursive        # required before the first build
cargo build --release                          # desktop build (default features = slint GUI)
cargo fmt --check
cargo clippy -- -Dwarnings
cargo clippy --no-default-features -- -Dwarnings
./target/release/gopher64 /path/to/rom.z64     # run a ROM directly (CLI mode)
```

- There is no test suite: the repository has no `#[test]` functions and no `tests/` directory. Validation is done by running ROMs. The reference games and the test matrix are in `docs/xbox-port/13-testing-debugging.md`.
- `--no-default-features` builds without the slint GUI (CLI only). This is the configuration the UWP build uses.
- Build requirements:
  - `clang`/`llvm`: `.cargo/config.toml` sets `CC=clang`, `CXX=clang++`, `AR=llvm-ar`. Windows CI uses `clang-cl` with an LLVM version matching rustc's.
  - libclang for `bindgen`.
  - git, because `build.rs` runs `git describe` and panics without it.
- The toolchain is pinned by `rust-toolchain.toml` (currently 1.99.1).
- UWP build (planned; see `docs/xbox-port/05-rust-uwp-toolchain.md`):

  ```bash
  RUSTC_BOOTSTRAP=1 cargo rustc --lib --release --crate-type staticlib \
    --target x86_64-uwp-windows-msvc --no-default-features -Zbuild-std=std,panic_abort
  ```

  This needs `rustup component add rust-src` for the pinned toolchain. The final link and msix packaging are done by the CMake/MSBuild host in `uwp/` and the scripts in `scripts/uwp/` (not created yet).

## Architecture (big picture)

### Entry and run modes

`src/main.rs` (desktop binary, mimalloc) calls `create_runtime()`, which starts a tokio runtime on its own thread, then calls `run(Args)` in `src/lib.rs`. `run()` creates the config, data and cache dirs (`ui::get_dirs()`), sets SDL hints, and then branches:

- **ROM argument:** load the ROM (z64/n64/v64, zip, 7z), config, cheats, saves, RetroAchievements and optional netplay, then call `device::run_game`.
- **CLI tools:** `--configure-input-profile`, `--list-controllers`, `--assign-controller`, `--bind-input-profile` and `--clear-input-bindings`.
- **No arguments with the `gui` feature:** slint `AppWindow`. The desktop GUI launches games by **re-spawning its own executable** with CLI args (`tokio::process::Command` in `src/ui/gui.rs`), so the GUI and the emulator run in separate processes.
- **Android:** `android_main` hosts only the slint launcher. Games run through the exported `gopher64_sdl_main(argc, argv)` (`src/ui/android.rs`), called by SDL's Activity in the same process. The UWP port reuses this "embedder calls `run()` in-process" model.

### Emulator core (`src/device/**`)

- One big `Device` struct (`src/device.rs`) owns all hardware state:
  - VR4300 CPU, COP0/1, TLB, cache
  - RSP (scalar plus vector unit, using SSE intrinsics; NEON via sse2neon on aarch64)
  - PI, SI, AI, VI, MI, RI
  - PIF, cartridge and controller paks
- It is serialised with serde/postcard for savestates (`src/savestates.rs`). Non-serialisable parts are marked `#[serde(skip)]`.
- **Pure interpreter.**
  - CPU and RSP instructions dispatch through function-pointer tables (`cpu.rs`, `cpu_instructions.rs`, `rsp_*`).
  - Memory accesses dispatch through per-64 KiB function-pointer tables (`memory.rs`: `memory_map_read`, `memory_map_write`, `fast_read`).
  - There is no JIT and no executable memory.
- **Timing.** An event scheduler (`events.rs`: `create_event`, `trigger_event`) drives interrupts and DMA completion against the CPU cycle count. `vi.rs` paces frames with `spin_sleep`.

### Renderer (C++ via FFI)

- The RDP is **parallel-rdp** (a Vulkan compute-shader RDP), with Granite's Vulkan backend and volk. All of it is compiled by `build.rs` with `cc`.
- The glue is `parallel-rdp/interface.cpp` (repo-owned, not the submodule). It handles:
  - Vulkan init: `Context::init_loader(SDL_Vulkan_GetVkGetInstanceProcAddr())` and `SDL_WSIPlatform`
  - creating the `RDP::CommandProcessor` over gopher64's host RDRAM pointer
  - the OSD (SDL_ttf), present, and CRT/plain blit shaders (`spirv*.hpp`)
  - an SDL event watch implementing keyboard hotkeys and window/lifecycle events
- Rust calls it through bindgen-generated bindings of `parallel-rdp/interface.hpp`. The Rust side is `src/ui/video.rs`, which creates the SDL window, calls `rdp_init`, and handles framebuffer sync via `rdp_check_framebuffers`.
- RetroAchievements is rcheevos C (`retroachievements/`), also wrapped with bindgen. Its HTTP goes through Rust's `reqwest` (`src/retroachievements.rs`).

### Platform layer (`src/ui/**`)

- Everything uses SDL3 through `sdl3-sys`, built from source and statically linked on desktop:
  - window (`video.rs`)
  - audio stream with drift control via the frequency ratio (`audio.rs`)
  - gamepad and joystick input and profiles, with gamepad hotkeys while the Back/View button is held (`input.rs`, `input_profile.rs`)
  - saves, written asynchronously with `tokio::fs` (`storage.rs`)
  - `config.json` (`config.rs`)
- The slint GUI (`gui.rs`, `gui/*.slint`) and its helpers sit behind the `gui` feature.
- Netplay is always compiled: `src/netplay.rs` (ggrs over a matchbox WebRTC socket) and the lobby in `src/ui/netplay.rs` (tokio-tungstenite).

### Build script (`build.rs`)

- Compiles the slint UI (gui feature only), volk, parallel-rdp/Granite and rcheevos.
- Adds `-march=x86-64-v3` (or the ARM equivalent) and `-flto=thin` to all C/C++ code. LTO bitcode only works because the Windows/macOS linker is `rust-lld`.
- Adds `VK_USE_PLATFORM_WIN32_KHR` and the winresource icon on Windows.
- Runs bindgen and sets `GIT_DESCRIBE`.
- SDL headers are found through `DEP_SDL3_OUT_DIR` / `DEP_SDL3_TTF_OUT_DIR`, which the `sdl3-sys` / `sdl3-ttf-sys` build scripts export (`links = "SDL3"`). The UWP plan overrides these via Cargo `links` overrides instead of editing `build.rs`.
