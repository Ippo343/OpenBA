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

void RtInputPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_INPUT")) {
        printf("[RtPort] BANISHED_RT_INPUT=0 -- Input namespace port disabled\n");
        return;
    }
    int n = RtInputRegister();
    n += RtInputWinRegister();
    n += RtInputKeyMapLoadRegister();
    if (RtWaveEnabled("BANISHED_RT_INPUTCFGSTART")) n += RtInputConfigStartupRegister();
    printf("[RtPort] Input: %d exports registered\n", n);
}
