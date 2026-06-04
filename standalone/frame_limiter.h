// standalone/frame_limiter.h
// Per-frame CPU frame limiter for the standalone exe.
//
// The engine main loop (Runtime MainLoop::Frame, which standalone runs via
// MainLoop::Run @ Runtime 0x1800dd3c0) is uncapped — its only per-frame yield
// is Application::YieldToThreads (a thunk to SwitchToThread, which does NOT
// sleep). With nothing else runnable on the core, the menu loop spins at
// thousands of FPS and pegs one CPU core to 100%. The only natural throttle
// is Present blocking on vsync, which the standalone never enables.
//
// FrameLimiter::Tick() is called once per rendered frame (from the loop's Draw
// vtable slot — the only per-frame chokepoint we own in standalone, since the
// outer Frame loop lives in the Runtime DLL). It paces the loop to an UPPER
// ceiling of (display refresh + headroom) FPS.
//
// The ceiling sits ABOVE the refresh rate on purpose: it exists only to kill
// the multi-thousand-FPS spin, never to be the real limiter. Any external
// pacing the user has — G-Sync/FreeSync, driver/in-game vsync, RTSS, a driver
// frame cap — runs at or below refresh and therefore engages first; our
// limiter then measures elapsed >= interval and sleeps zero. So external frame
// limiting always works and is never fought.
//
// Tuning (env vars, read once at first Tick):
//   BANISHED_FPS_CAP=<n>        Absolute ceiling in FPS. 0 / "off" = disabled
//                              (fully uncapped). Overrides the auto ceiling.
//   BANISHED_FPS_HEADROOM=<p>   Percent above display refresh for the auto
//                              ceiling. Default 50 (=> refresh * 1.5).

#pragma once

namespace FrameLimiter {

// Pace the calling thread so successive Tick() calls are at least one frame
// interval apart. Cheap no-op when the frame already took longer than the
// ceiling interval (i.e. when something else is already limiting the rate).
// Call exactly once per rendered frame, at frame start (before draw work) for
// lowest added latency. First call only primes the timestamp.
void Tick();

// Most recent measured frame rate (frames/sec), updated ~once per second.
// 0 until enough frames have elapsed. Intended for HUD/log readout.
double GetFps();

// The active ceiling in FPS (0 if the limiter is disabled). Valid after the
// first Tick().
double GetCapFps();

} // namespace FrameLimiter
