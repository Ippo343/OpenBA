// ============================================================================
// runtime/src/engine/rt_engine_port.cpp — RtEnginePortInit()   Wave 8 lanes A+B
// ============================================================================
//   RT_ENGINE=0      leave the whole Engine namespace on the DLL
//   RT_ENGINELOOP=0  ... just the loop family (lane A):
//                                 UpdateLoop / Loader / LoaderLoop /
//                                 DialogLoopT / DialogLoaderLoopT
//   RT_ENGINECORE=0  ... just Engine::Engine + Engine::MainLoop (lane B)
//   RT_MAINLOOPRUN=0 ... just MainLoop::Run + MainLoop::Update (lane
//                                 ADDR). Its own switch because Run is the
//                                 process's ONE main loop and has no oracle:
//                                 a defect shows up as "the game does not run",
//                                 not as a divergence row.
//
// Two lanes, two register functions, one aggregator — the Wave 6 shape
// (runtime/src/video/rt_video_port.cpp), because Video likewise had lanes that
// shared a namespace switch.
//
// What is NOT here, and why, since a reader will come looking for it:
//
//   * Engine::Engine's ctor, dtor, Startup and Shutdown are NO LONGER held
//     back. Wave 8 lane B held them on the allocator NO-GO
//     (docs/runtime_port/WAVE2_ALLOCATOR.md); Wave 9 lanes E/E2/E3 discharged
//     that verdict and lane E4 armed the allocator at cutover, so the reason
//     is stale. They live in engine_lifecycle.cpp (lane ENG) and are what
//     cutover boot attempts 04-06 died on. Read WAVE9_LANE_ENG.md before
//     touching them: Engine::Startup is the ONLY place in the process an arena
//     is ever created, and Engine::Shutdown has calls=0 and is unverified.
//   * MainLoop::OnVideoError / OnVideoRestart are NO LONGER HELD BACK (Wave 9
//     lane VIDL, mainloop_video.cpp). Wave 8 lane B held them as "the first
//     functions in this whole port with a genuine DETOUR requirement", because
//     each has exactly one internal caller — the static thunks the MainLoop
//     ctor installs into Video::Device::_singleton +0x50/+0x58 — so a port that
//     registered the export and stopped would be dead code reporting
//     `calls=0 divergences=0`. THAT REASON IS DISCHARGED, and not by a detour:
//     lane VID replaced both thunks with our own, and lane ENGIO's
//     BANISHED_FORCE_DEVICE_LOST=<n> makes the device-lost event happen on
//     demand, so the pair now has a live driver and a non-zero calls=.
//     The reader should still expect calls=0 on an ORDINARY run — nothing
//     resets the device — which is why the lane doc insists the verification
//     run be the forced one.
//   * LoaderLoop::_loader is DATA and deliberately NOT registered: it is the
//     DLL's own static Engine::Loader, a System::Thread with a live OS thread
//     parked on its hStart. Handing callers a different object would stall the
//     loading screen forever. See WAVE8_LANE_A.md.
// ============================================================================

#include "../../funcs/rt_port.h"

// B3R RT-Core: every row the eight Engine register functions carried lost its
// last reader (the readers were converted to direct calls, here and in
// code/Game), so the functions went with their rows. This entry point
// registers nothing; it stays defined only because
// standalone/resolve_standalone.cpp's RuntimePortInit() still calls it (the
// coordinator removes that call, then this file).
void RtEnginePortInit() {
}
