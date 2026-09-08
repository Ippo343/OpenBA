// ============================================================================
// runtime/src/engine/rt_engine_port.cpp — RtEnginePortInit()   Wave 8 lanes A+B
// ============================================================================
//   BANISHED_RT_ENGINE=0      leave the whole Engine namespace on the DLL
//   BANISHED_RT_ENGINELOOP=0  ... just the loop family (lane A):
//                                 UpdateLoop / Loader / LoaderLoop /
//                                 DialogLoopT / DialogLoaderLoopT
//   BANISHED_RT_ENGINECORE=0  ... just Engine::Engine + Engine::MainLoop (lane B)
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
//   * MainLoop::OnVideoError / OnVideoRestart are HELD BACK, and they are the
//     first functions in this whole port with a genuine DETOUR requirement:
//     each has exactly one internal caller — the static thunks the MainLoop
//     ctor installs into Video::Device::_singleton +0x50/+0x58 — so a port that
//     registered the export and stopped would be dead code reporting
//     `calls=0 divergences=0`. The ported ctor keeps installing the DLL's own
//     thunks. See WAVE8_LANE_B.md.
//   * LoaderLoop::_loader is DATA and deliberately NOT registered: it is the
//     DLL's own static Engine::Loader, a System::Thread with a live OS thread
//     parked on its hStart. Handing callers a different object would stall the
//     loading screen forever. See WAVE8_LANE_A.md.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtEngineLoopsRegister();     // loops.cpp            (lane A)  — 12
extern int RtEngineCoreRegister();      // engine.cpp           (lane B)  — 3
extern int RtEngineLifecycleRegister(); // engine_lifecycle.cpp (lane ENG) — 8

void RtEnginePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_ENGINE")) {
        printf("[RtPort] BANISHED_RT_ENGINE=0 -- Engine namespace port disabled\n");
        return;
    }
    int loops = 0, core = 0, life = 0;
    if (RtWaveEnabled("BANISHED_RT_ENGINELOOP")) loops = RtEngineLoopsRegister();
    if (RtWaveEnabled("BANISHED_RT_ENGINECORE")) core  = RtEngineCoreRegister();
    // Wave 9 lane ENG. Kept as its OWN switch rather than folded into
    // ENGINECORE: these four are the first ports in this project that sit on
    // the process's one boot path, and the integrator needs to be able to A/B
    // them without also giving up MainLoop's ctor/dtor. It must also be turned
    // OFF for any allocator-shadow run -- RtAllocShadowInstall arms by
    // detouring the DLL's Engine::Startup, and our port never executes those
    // bytes, so the shadow would report skipped_pre_startup and call the run
    // INVALID. See docs/runtime_port/WAVE9_LANE_ENG.md §5.
    if (RtWaveEnabled("BANISHED_RT_ENGINELIFE")) life = RtEngineLifecycleRegister();
    printf("[RtPort]   Engine loop family (lane A): %d exports registered "
           "(LoadResources held back -- no oracle, see WAVE8_LANE_A.md)\n", loops);
    printf("[RtPort]   Engine::Engine + MainLoop (lane B): %d exports registered "
           "(3 held back: 2 detour-required + Run, plus _singleton which is DATA -- "
           "see WAVE8_LANE_B.md)\n", core);
    printf("[RtPort]   Engine lifecycle (lane ENG): %d exports registered "
           "(ctor/dtor/Startup/Shutdown + 4 helpers; Shutdown is UNVERIFIED, "
           "calls=0 by construction -- see WAVE9_LANE_ENG.md)\n", life);
    printf("[RtPort] Engine: %d exports registered\n", loops + core + life);
}
