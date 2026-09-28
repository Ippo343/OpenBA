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

// The five unexported `Collection::Set` AVL bodies for the 0x18-byte element
// instantiation (0x180023060/130/170/570/6a0), registered from here because
// Input::KeyMap was their only caller.
extern int RtCollectionSetAvlE24Register();       // collection/set_avl_e24.cpp

void RtInputPortInit() {
    // B3R (registry teardown, W1 RT-Core): the eight Input register functions
    // registered only rows no code read (or read only by a site converted to a
    // direct call); they went with their rows.
    const int n = RtCollectionSetAvlE24Register();
    printf("[RtPort] Input: %d exports registered\n", n);
}
