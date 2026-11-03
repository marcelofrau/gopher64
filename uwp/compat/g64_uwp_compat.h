// uwp/compat/g64_uwp_compat.h — force-included (/FI) into volk + parallel-rdp on UWP only.
//
// UWP-PORT: parallel-rdp/Granite/volk compile under WINAPI_FAMILY_APP on the UWP target,
// which hides a few desktop-only SDK declarations. We re-declare/redirect them so the code
// still compiles and links; none of these paths execute on UWP (see docs/xbox-port/05 §7.1).
// This header is port-owned and never touches the submodule (G-2). (D-14)
#pragma once
#if defined(GOPHER64_UWP)
// Force-included before any TU's own includes: keep <windows.h>'s min/max macros out of the
// way so parallel-rdp's std::min/std::max (interface.cpp) compile. Desktop gets NOMINMAX via
// the cc crate's defaults; on UWP we add it here since we pull in windows.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
// LoadLibraryA is desktop-only in the SDK headers under WINAPI_FAMILY_APP. Re-declare so
// volk.c / context.cpp compile; resolved at link via the UWP umbrella import libs. Never
// called on UWP: volkInitialize() is unused and init_loader() always gets Dozen's GIPA.
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
__declspec(dllimport) HMODULE WINAPI LoadLibraryA(LPCSTR lpLibFileName);
#endif
#ifdef __cplusplus
}
#endif
// user32 does not exist on Xbox: route Granite's post-mortem dialog to the log (stderr is
// redirected to the log file by the host). No OutputDebugString: it can raise without a
// debugger attached (scummvm-uwp docs/XBOX-TILE-LAUNCH-KILL.md).
#include <stdio.h>
#define MessageBoxA(hwnd, text, caption, type) (fprintf(stderr, "%s: %s\n", (caption), (text)), 0)
#endif
