// ============================================================================
// runtime/src/engine/rt_engine_port.cpp — RtEnginePortInit()   Wave 8 lanes A+B
// ============================================================================
//   BANISHED_RT_ENGINE=0      leave the whole Engine namespace on the DLL
//   BANISHED_RT_ENGINELOOP=0  ... just the loop family (lane A):
//                                 UpdateLoop / Loader / LoaderLoop /
//                                 DialogLoopT / DialogLoaderLoopT
//   BANISHED_RT_ENGINECORE=0  ... just Engine::Engine + Engine::MainLoop (lane B)
//   BANISHED_RT_MAINLOOPRUN=0 ... just MainLoop::Run + MainLoop::Update (lane
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

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtEngineLoopsRegister();     // loops.cpp            (lane A)  — 12
extern int RtEngineCoreRegister();      // engine.cpp           (lane B)  — 3
extern int RtEngineLifecycleRegister(); // engine_lifecycle.cpp (lane ENG) — 8
extern int RtEngineMainLoopRunRegister();// mainloop_run.cpp    (lane ADDR)— 2
extern int RtEngineMainLoopVideoRegister();// mainloop_video.cpp (lane VIDL)— 2

void RtEnginePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_ENGINE")) {
        printf("[RtPort] BANISHED_RT_ENGINE=0 -- Engine namespace port disabled\n");
        return;
    }
    int loops = 0, core = 0, life = 0, mlrun = 0, mlvid = 0;
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
    // Wave 9 lane ADDR. MainLoop::Run + MainLoop::Update, on their OWN switch
    // because Run is the process's single main loop: it is called once from
    // game_main.cpp:176, never returns until quit, and CANNOT have a diff
    // oracle. If it is wrong the symptom is "the game does not run", with no
    // divergence row to read, so the integrator needs to be able to A/B this
    // one function without also giving up the Engine lifecycle ports.
    if (RtWaveEnabled("BANISHED_RT_MAINLOOPRUN")) mlrun = RtEngineMainLoopRunRegister();
    // Wave 9 lane VIDL. MainLoop::OnVideoError + OnVideoRestart, the
    // device-lost pair, on their OWN switch for the same reason lane ADDR has
    // one: they run only after a real (or forced) device reset, so a defect
    // here is invisible until the day a driver resets — and being able to A/B
    // exactly these two against the DLL's own bodies, with the rest of the
    // Engine port left alone, is the only way to attribute a device-lost
    // regression. They also register the two Collection instantiations they
    // need (0x18004f620 SetSize and 0x180034440 elem-destroy), so turning this
    // switch off returns BOTH of those to the DLL as well.
    if (RtWaveEnabled("BANISHED_RT_MAINLOOPVIDEO")) mlvid = RtEngineMainLoopVideoRegister();
    printf("[RtPort]   Engine loop family (lane A): %d exports registered "
           "(LoadResources held back -- no oracle, see WAVE8_LANE_A.md)\n", loops);
    printf("[RtPort]   Engine::Engine + MainLoop (lane B): %d exports registered "
           "(2 held back: both detour-required, plus _singleton which is DATA -- "
           "see WAVE8_LANE_B.md; Run is NO LONGER among them, see lane ADDR below)\n", core);
    printf("[RtPort]   MainLoop::Run + Update (lane ADDR): %d exports registered "
           "(Run closes the ADDRESS door at game_main.cpp:176 and opens two NAME "
           "doors -- Input::Device::Update and MainLoop::Frame; Run has NO ORACLE "
           "by construction -- see WAVE9_LANE_ADDR.md)\n", mlrun);
    printf("[RtPort]   Engine lifecycle (lane ENG): %d exports registered "
           "(ctor/dtor/Startup/Shutdown + 4 helpers; Shutdown is UNVERIFIED, "
           "calls=0 by construction -- see WAVE9_LANE_ENG.md)\n", life);
    printf("[RtPort]   MainLoop device-lost pair (lane VIDL): %d exports registered "
           "(OnVideoError + OnVideoRestart, plus Array<VideoResource>::SetSize "
           "0x18004f620 and elem-destroy 0x180034440 by ADDRESS; OnVideoRestart "
           "OPENS THREE NAME DOORS -- Load@External, GetCompiledPath@ExternalFactory, "
           "OnVideoRestart@SaveLoad -- see WAVE9_LANE_VIDL.md §2)\n", mlvid);
    printf("[RtPort] Engine: %d exports registered\n",
           loops + core + life + mlrun + mlvid);
}
