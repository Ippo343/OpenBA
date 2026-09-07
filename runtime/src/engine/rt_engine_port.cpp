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
//   * Engine::Engine's ctor, dtor, Startup and Shutdown are HELD BACK. The
//     MemoryAllocator is embedded at offset 0 of Engine::Engine
//     (GetMemoryAllocator is a bare `MOV RAX,RCX; RET`) and the allocator port
//     carries a NO-GO verdict (docs/runtime_port/WAVE2_ALLOCATOR.md). Lane B
//     found the NO-GO reaches two functions further than PLAN.md predicted:
//     Startup writes three allocator fields directly and Shutdown calls the
//     allocator's own shutdown. See WAVE8_LANE_B.md.
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

extern int RtEngineLoopsRegister();   // loops.cpp  (lane A) — 12
extern int RtEngineCoreRegister();    // engine.cpp (lane B) — 3

void RtEnginePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_ENGINE")) {
        printf("[RtPort] BANISHED_RT_ENGINE=0 -- Engine namespace port disabled\n");
        return;
    }
    int loops = 0, core = 0;
    if (RtWaveEnabled("BANISHED_RT_ENGINELOOP")) loops = RtEngineLoopsRegister();
    if (RtWaveEnabled("BANISHED_RT_ENGINECORE")) core  = RtEngineCoreRegister();
    printf("[RtPort]   Engine loop family (lane A): %d exports registered "
           "(LoadResources held back -- no oracle, see WAVE8_LANE_A.md)\n", loops);
    printf("[RtPort]   Engine::Engine + MainLoop (lane B): %d exports registered "
           "(8 held back: 4 allocator NO-GO, 2 detour-required, Run, _singleton -- "
           "see WAVE8_LANE_B.md)\n", core);
    printf("[RtPort] Engine: %d exports registered\n", loops + core);
}
