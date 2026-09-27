// ============================================================================
// runtime/src/input/rt_input_port.cpp — RtInputPortInit()  Wave 8 lane D
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   RT_INPUT=0   leave the whole Input namespace on the DLL
//
// Mirrors runtime/src/video/rt_video_port.cpp. This lane is a single file
// (input.cpp), so there is only one namespace-level switch, not a per-lane
// one — a future lane splitting KeyMap/Device/Configuration further can add
// RT_INPUT_KEYMAP etc. the way Wave 6's Video did.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/input/Device.h"   // ENDGAME B.3.9 ENG: the storage redirect
#include "../../types/input/Input.h"   // ENDGAME B.3.9 ENG: the storage redirect

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
// Wave 9 lane ENGIO: Input::Device::Update (0x1800E1490) and the three
// unexported bodies it calls -- UpdateState (0x1800E0FC0),
// PlatformGetKeyboardState (0x1800E1C60) and PlatformGetMouseState
// (0x1800E1E10). The fourth helper, PlatformUpdate, was already lane IOA's.
// This is WAVE9.md row 16, the row two earlier lanes sized and declined.
extern int RtInputDeviceUpdateRegister();         // input_device_update.cpp
// Wave 24 lane SMALLBODY: Input::Device::PlatformShutdown -- the Win32/
// DirectInput teardown half. Its own .cpp for the same <windows.h> seam
// input_win.cpp sits behind, and its own kill switch.
extern int RtInputPlatformShutdownRegister();     // input_platform_shutdown_win.cpp

void RtInputPortInit() {
    // ENDGAME B.3.9 ENG (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish).
    RtMintRedirectStorage("?_singleton@Device@Input@@0PEAV12@EA", &::Input::Device::_singleton);
    // ENDGAME B.3.9 ENG (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish).
    RtMintRedirectStorage("?_singleton@Configuration@Input@@0PEAV12@EA", &::Input::Configuration::_singleton);
    int n = RtInputRegister();
    n += RtInputWinRegister();
    n += RtInputKeyMapLoadRegister();
    n += RtInputConfigStartupRegister();
    n += RtInputKeyMapSaveRegister();
    const int avl = RtCollectionSetAvlE24Register();
    n += avl;
    const int upd = RtInputDeviceUpdateRegister();
    n += upd;
    // Wave 24 lane SMALLBODY: Input::Device::PlatformShutdown (0x1800e1b50).
    // Own switch: it is on the process SHUTDOWN path and should be
    // bisectable without moving Device::Update and the KeyMap bodies.
    n += RtInputPlatformShutdownRegister();

    // The identity line this lane gates its runs on. `tools/game_dir_lock.sh`
    // is advisory and six lanes were running when this landed: the on-disk md5
    // says which exe is on disk, not which one RAN, so the acceptance test is a
    // string only this build emits, read out of the run's own log.
    printf("[RtPort] Input: %d exports registered (lane INPUT: +2 KeyMap doors, "
           "+%d Set<E24> AVL addresses; lane ENGIO: +%d Device::Update bodies)\n",
           n, avl, upd);
}
