// standalone/boot_window.h
// Hold the engine window hidden until there is a real frame to put in it.
//
// The problem, measured (see [BootTime] lines in standalone_debug.log):
//
//   t=  70ms  Engine::Startup begins
//   t= 535ms  ... Runtime platform/filesystem work (package mount, disk-bound)
//   t= 625ms  VideoDX11 CreateDevice step 6: ShowWindow  <-- window on screen
//   t= 750ms  D3D11CreateDevice + swap chain + color/depth buffers
//   t= 940ms  rest of Engine::Startup (audio, input)
//   t=1070ms  our post-Startup boot (sprite sheet, splash .rsc preloads)
//   t=1100ms  first splash frame is finally presented
//
// So the window is up and empty for ~450ms of every boot. Nothing in that
// window is wasted work — it is all load the game genuinely has to do — but
// there is no reason for the user to watch an empty frame while it happens.
// Retail hides this behind the same ShowWindow simply because it reaches its
// first Present sooner.
//
// The fix uses the engine's OWN mechanism rather than a new one. Video::
// Configuration+0x42 ("skipShow") is a non-persisted flag that the shipped
// CreateDevice already honours:
//
//     if (!Configuration+0x42) ShowWindow(Device+0x90, SW_SHOWDEFAULT)
//
// We set that flag before CreateDevice reads it, so the shipped branch skips
// its own ShowWindow with no change to the ported code path, and we take
// responsibility for showing the window once the splash has actually presented
// a frame. Net effect: the window appears already displaying the Shining Rock
// splash instead of appearing empty and filling in half a second later.
//
// Boot wall-clock is unchanged — this trades a visible empty window for a
// slightly later, already-populated one.
//
// Kill-switch: BANISHED_DEFER_WINDOW_SHOW=0 restores the shipped behaviour
// (window appears as soon as CreateDevice shows it).

#pragma once

#include <windows.h>

namespace BootWindow {

// Called from VideoDX11 CreateDevice just before step 6. Sets the engine's
// skipShow flag and records the HWND so ShowNow can reveal it later.
// Returns true if the deferral was armed; false means the shipped ShowWindow
// runs as normal (kill-switch set, or no config/hwnd available).
bool ArmDeferredShow(void* videoConfig, HWND hwnd);

// Reveal the window. Idempotent, and a no-op when nothing was armed.
// `reason` is logged so the boot trace shows which path revealed it.
void ShowNow(const char* reason);

// True while the window is armed but still hidden.
bool IsPending();

// Per-frame safety net. If something goes wrong on the splash path and nobody
// ever calls ShowNow, this reveals the window anyway after a timeout so the
// game can never end up running with no visible window at all.
void PumpWatchdog();

} // namespace BootWindow
