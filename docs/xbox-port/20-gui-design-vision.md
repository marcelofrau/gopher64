# 20 — GUI design vision: a gamepad-first N64 shell for Xbox

Status: **design vision — documentation only, nothing implemented.** This captures creative and
scope decisions made with the user on 2026-11-05, before any P5 shell code is written. It refines
the P5 shell sketched in [07-frontend.md §2.2](07-frontend.md#22-p5-gamepad-shell-imgui-in-the-host)
with a concrete visual identity, menu scope and asset strategy.

## 1. Philosophy: the same recipe as Ymir's shell, a different era

The user's Ymir UWP shell (`../Ymir/apps/ymir-sdl3/src/platform/uwp/shell/`) is the bar: a
full-screen, controller-only interface themed after the Sega Saturn BIOS — a starfield in "warp", a
cockpit panel, faceted spheres as menu options, a dot-matrix LCD readout, scanlines, a chromatic
split on focused text. Every visual choice in it has a historical reason ("this is what the VDP1
drew", "this is the LCD font of 90s hardware"), and the whole thing is built from procedural
widgets with a graceful placeholder fallback when art is missing — the "soul" of the design is code
simulating period hardware artifacts, not a static art budget.

gopher64's shell should follow the **same recipe** — procedural depth, graceful fallback, a theme
manifest overridable without rebuild, canvas + TV-safe-area layout, a native-gamepad ImGui
navigation model — but the **identity is the Nintendo 64**, not the Saturn: the console, the
cartridge, the controller, not a spaceship cockpit.

Architectural decisions worth reapplying as-is (Ymir `docs/uwp/shell/README.md` §5, S1–S17):

- Shell lives in its own module, separate from the unstable, frequently-upstream-touched desktop
  GUI code (S1) — for gopher64 this means the shell is entirely host-side C++ (`uwp/`), consuming
  the Rust core only through the C ABI, never touching `src/ui/gui.rs`/`*.slint`.
- A fixed reference canvas (1920×1080) with a configurable TV safe area, independent of
  ImGui/WinRT, so the layout math is testable without a console.
- Procedural widgets (geometry, not shaders — see §3) with a placeholder fallback when an asset is
  missing, so the shell never has a visual hole.
- A theme/asset manifest that the console can override without rebuilding (Device Portal file
  drop), analogous to Ymir's `theme.toml` + `<profile>/shell-theme/` override path.
- An own small bitmap font for display accents, if chosen (see §6) — never a third-party font
  embedded without a clear license to distribute.
- Native ImGui gamepad navigation (`ImGuiConfigFlags_NavEnableGamepad`) over hand-drawn
  `InvisibleButton` items, no virtual mouse cursor, each widget drawing its own focus effect.
- A dedicated button to open/close the shell (Ymir uses **View**, since the Saturn pad has no
  equivalent button to lose; the N64 controller has no View-equivalent either, so the same choice
  is free here too).

## 2. Visual identity: Nintendo 64, not Saturn

- **Palette: fixed, authentic, not user-selectable (confirmed decision).** Base: the classic N64
  console body color — charcoal gray / black. Accents: the real N64 controller button colors —
  A, B, the four C-buttons, and Start. These exact hex values are **not yet confirmed against a
  reference photo**; the user is supplying assets/references, and the palette should be finalized
  against those before implementation, not from memory alone.
- **No shader, no post-processing — anywhere in the shell, and not reused from the gameplay CRT
  toggle either.** This is an explicit, non-negotiable user decision: nothing in the shell or the
  emulator's main screen gets a new rendering effect. The existing optional `video.crt` toggle
  (`parallel-rdp/interface.cpp:422-424,684,713`, config key `video.crt`, default `false`) is
  **unchanged** — it stays exactly as it is today, a user-facing gameplay setting exposed in
  Settings (07 §2.3) — it is simply never used as a "visual identity bridge" to the shell, and the
  shell never applies anything like it to itself.
- **Where the "era" depth comes from instead:** hand-drawn ImGui geometry (not shaders), color,
  motion (rotation, float, fade-in/out — the same toolkit Ymir used for its spheres, just without
  any pixel-shader post-processing step), and assets. A recurring N64-flavored motif — e.g. a
  cartridge silhouette (label area, the gold edge-connector teeth), the control stick's distinctive
  shape, the trident grip outline — can play the role Ymir's faceted spheres played for Saturn:
  one geometric module reused everywhere for consistency and low art cost.

## 3. Layout: the sidebar + grid sketch, refined

The user's concept sketch (`../assets/concept-design.png`, formalized by Gemini into a layout
JSON) is the starting skeleton, not the final word:

- **Fixed left sidebar** (~18% width): logo/wordmark, vertical menu list with icon + label, footer
  with version string.
- **Main content, right of the sidebar**: a grid of game tiles.
- **N64 branding** anchored in a corner.
- Both areas live inside the **1920×1080 reference canvas + TV safe area** model (Ymir
  `layout.hpp`), so the design scales correctly to 1080p and 4K Xbox output without per-resolution
  special-casing.

Existing assets that already cover part of the sidebar icon set (`data/ui/icons/*.svg`):
controller, settings, cheats, achievements, about, local_game. Missing and to be created/sourced:
Browse/Open, Quit — unless one of the existing SVGs is repurposed.

## 4. Menu scope, v1 (confirmed)

| Item | Backed by (headless, verified in code) | Notes |
|---|---|---|
| Browse / Open | `ui::get_dirs()` (`src/ui.rs:217-241`), `ui.rom_dir`, `ui.recent_roms` (`src/ui/config.rs:77-78`, max 5, `src/lib.rs:173-175`) | No existing cover-art/metadata infra beyond the No-Intro name dump (`data/ui/Nintendo...xml`) — see §5 |
| Settings | Config C ABI (`gopher64_uwp_config_get/set`, `src/ui/uwp/entry.rs`, shipped 2026-11-05) | Schema stays owned by `src/ui/config.rs`; see 07 §2.3 for the setting list |
| Input | `ui::input_profile`, `ui::input` — not `gui`-gated, callable directly; rumble is real (`SDL_RumbleGamepad`/`SDL_RumbleJoystick`, `src/ui/input.rs:221-234`) | Reuses the existing CLI-equivalent flow (`--configure-input-profile`), no new binding logic |
| Cheats | `src/cheats.rs` (always compiled), `data/cheats.json` keyed by CRC (`ui::storage::get_game_crc`), user selection in `ui::config::Cheats` (`src/ui/config.rs:70-119`) | Only the slint window (`src/ui/cheats.rs`) is `gui`-gated and unusable; the engine and config are not |
| RetroAchievements | `src/retroachievements.rs` (always compiled), login/hardcore headless (`src/lib.rs:216-248`) | Only the slint window (`src/ui/retroachievements.rs`) is `gui`-gated; **included in v1** per user decision |
| About | static | version, build info |
| Quit | host-side | |

**Netplay is deliberately absent from v1 — not even as a disabled/"coming soon" item.** It is
confirmed dead on the UWP target: the `cfg` swap in `src/lib.rs` routes to
`src/ui/uwp/netplay_stub.rs:58-65`, whose `init()` always returns `None`; the netplay window
(`src/ui/netplay.rs`) is additionally `cfg(feature = "gui")` and excluded from the UWP build
entirely. There is nothing for a menu item to do.

## 5. Box art: placeholder now, TheGamesDB later (confirmed decision)

There is **no existing cover-art infrastructure** — confirmed by an empty grep for
box/cover/gamelist/scraper across `src/ui/gui.rs` and `src/ui/storage.rs`. The only game metadata
today is the No-Intro XML dump used purely to show a pretty ROM name.

- **v1:** each game tile falls back to a **procedural placeholder** when no cover art exists —
  accent color + game title + the recurring cartridge motif from §2. No network dependency, no
  licensing question, works from day one.
- **Future (v2+, not implemented, not specified in detail here):** TheGamesDB integration for real
  box art. When that work starts, it needs: an API key, network access (the project already links
  `reqwest` for RetroAchievements, so the dependency exists), a local image cache, respecting the
  API's rate limits and terms of service, and — critically — **never bundling or distributing cover
  images in the package**. This mirrors how Ymir handles music and covers: always user-supplied or
  fetched at runtime into the user's own data directory, never shipped. This document intentionally
  leaves the implementation open; it only reserves the slot.

## 6. Gamepad navigation and feedback

Mirrors Ymir's pattern directly:

- Menu/tile items are ImGui `InvisibleButton`s drawn by hand in the draw list; ImGui's native 2D
  "nearest item in that direction" navigation moves focus with the d-pad/left stick; A activates.
- No virtual cursor — each widget draws its own focus effect (the exact visual is TBD per §2, but
  it is geometry/color/motion, not a shader).
- First item in a screen gets keyboard-equivalent focus automatically on open.
- B is "go back" within the shell; the shell's dedicated open/close button follows Ymir's choice of
  **View** (no N64 controller equivalent to preserve either, so nothing is lost).
- Light rumble on focus change / confirm, using the already-working
  `set_rumble`/`SDL_RumbleGamepad` path (`src/ui/input.rs:221-234`) — no new API needed.

## 7. Typography — open decision

`data/ui/RobotoMono-Regular.ttf` is already embedded (used today for the OSD) and costs nothing new
to reuse for the shell's general text. Given the user's choice of Ymir-level thematic depth, it is
worth considering an own small bitmap/pixel font (no third-party license to manage, same reasoning
as Ymir's 5×7 dot-matrix font) for titles/accents only. **Not decided yet** — flagged here as a
decision to make before implementation, not resolved by this document.

## 8. Architecture and fidelity footprint

The shell is entirely host-side C++ (ImGui, port-owned under `uwp/`), talking to the Rust core only
through the C ABI that already exists or is a natural extension of it (config get/set today;
cheats/RetroAchievements equivalents would follow the same pattern if/when needed). This keeps the
upstream diff footprint minimal, consistent with
[00-upstream-fidelity-rules.md](00-upstream-fidelity-rules.md).

This is P5 work ([10-roadmap-checklist.md](10-roadmap-checklist.md#p5)). It depends on the
D3D11→Dozen hand-over (Q-06) being validated on real hardware first — see
[19-rendering-bringup-xbox.md §4](19-rendering-bringup-xbox.md#4-first-console-test-in-order-do-this-isolated-before-anything-else-in-p3p5).
Building the shell↔game swap before that is validated would mean building on top of the exact
failure mode `19` warns about.

## 9. Explicitly deferred / open questions

- **Swappable color themes** (e.g. a Funtastic-colorway picker) — considered, not chosen for v1;
  the palette is fixed and authentic (§2).
- **TheGamesDB integration** — reserved slot, not specified (§5).
- **Own pixel font vs. reusing RobotoMono** — not decided (§7).
- **Exact asset list** (icons, cartridge motif art, reference photos for the authentic palette) —
  to be supplied by the user; this document does not invent asset content, only where it plugs in.
