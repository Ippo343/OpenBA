// ============================================================================
// runtime/src/input/rt_input_port.cpp — RtInputPortInit()  Wave 8 lane D
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_INPUT=0   leave the whole Input namespace on the DLL
//
// Mirrors runtime/src/video/rt_video_port.cpp. This lane is a single file
// (input.cpp), so there is only one namespace-level switch, not a per-lane
// one — a future lane splitting KeyMap/Device/Configuration further can add
// BANISHED_RT_INPUT_KEYMAP etc. the way Wave 6's Video did.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"

extern int RtInputRegister();     // input.cpp
// Wave 9 lane IOA: Input::Device::EnableMouseClipping and its one (unexported)
// callee PlatformUpdate. Their own .cpp because they are the Win32 half of
// Input::Device and input.cpp must stay free of <windows.h>.
extern int RtInputWinRegister();  // input_win.cpp
// Wave 9 lane IOA: Input::KeyMap::Load — its own .cpp because it is the one
// KeyMap member that reaches into IO::Persistence's settings map.
extern int RtInputKeyMapLoadRegister();  // keymap_load.cpp
// Wave 9 lane ENG: Input::Configuration::Startup + the unexported Load it
// calls — Engine::Startup step 14, whose whole closure is now ported (lane
// IOA's KeyMap::Load was the last piece). Its own switch because it is on the
// process's single boot path.
extern int RtInputConfigStartupRegister();  // input_config_startup.cpp
// Wave 9 lane INPUT: Input::KeyMap::Save (0x1800a7520) and the unexported
// KeyMap deleting destructor (0x180053440) — the last two by-address doors the
// Input cluster left open.
extern int RtInputKeyMapSaveRegister();     // keymap_save.cpp
// Wave 9 lane INPUT: the five unexported `Collection::Set` AVL bodies for the
// 0x18-byte element instantiation (0x180023060/130/170/570/6a0). They live in
// runtime/src/collection/ because they are element-type-agnostic within that
// instantiation, but they are registered HERE because Input::KeyMap is their
// only caller in our tree today and this is the switch that governs it.
extern int RtCollectionSetAvlE24Register();       // collection/set_avl_e24.cpp
extern "C" void RtCollectionSetAvlE24ProbeArm(bool on);

void RtInputPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_INPUT")) {
        printf("[RtPort] BANISHED_RT_INPUT=0 -- Input namespace port disabled\n");
        return;
    }
    int n = RtInputRegister();
    n += RtInputWinRegister();
    n += RtInputKeyMapLoadRegister();
    if (RtWaveEnabled("BANISHED_RT_INPUTCFGSTART")) n += RtInputConfigStartupRegister();
    n += RtInputKeyMapSaveRegister();
    const int avl = RtCollectionSetAvlE24Register();
    n += avl;

    const char* p = std::getenv("BANISHED_RT_SETAVL24_PROBE");
    RtCollectionSetAvlE24ProbeArm(p && *p && std::strcmp(p, "0") != 0);

    // The identity line this lane gates its runs on. `tools/game_dir_lock.sh`
    // is advisory and six lanes were running when this landed: the on-disk md5
    // says which exe is on disk, not which one RAN, so the acceptance test is a
    // string only this build emits, read out of the run's own log.
    printf("[RtPort] Input: %d exports registered (lane INPUT: +2 KeyMap doors, "
           "+%d Set<E24> AVL addresses)\n", n, avl);
}
