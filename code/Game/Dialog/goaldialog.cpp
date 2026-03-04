// code/Game/Dialog/goaldialog.cpp
// Clean reimplementation of GoalDialog game logic.
//
// GoalDialog (0xC8 bytes) — achievement goal progress display.
// Extends DialogInstance (0xB8 base).
// +0xB8 = goal entry array (stride 0x18), +0xC0 = goal count.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// GoalDialog::Slot6 (vtable +0x30) — Update goal entry visibility
// ============================================================
// Gets the GoalList from game world (+0x150), iterates each goal entry.
// For each goal: checks IsMet/HasFailed, sets visibility flags on
// corresponding UI elements (entry+0 for met, entry+8 for failed).
//
// Entry layout at +0xB8 (stride 0x18 per entry):
//   +0x00: Element* metIndicator (shown when goal is met)
//   +0x08: Element* failedIndicator (shown when goal has failed)
//   +0x10: (unused in Slot6)
//
// Original: 0x140069640.

void GoalDialog_Slot6(GoalDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get GoalList from game world singleton at +0x150
    void* world = *g_pGameWorldSingleton;
    uintptr_t goalListObj = *(uintptr_t*)((uintptr_t)world + 0x150);

    int count = *(int*)(s + 0xC0);
    uintptr_t entryArray = *(uintptr_t*)(s + 0xB8);

    for (int i = 0; i < count; i++) {
        uintptr_t entryOffset = (uintptr_t)i * 0x18;

        // Get goal at index i from the GoalList
        uint64_t goalInst = 0;
        fn_GoalList_GetGoal((void*)(goalListObj + 0x10), &goalInst, i);

        // Check if goal is met and if it has failed
        bool isMet = fn_GoalInstance_IsMet((void*)goalInst);
        bool hasFailed = fn_GoalInstance_HasFailed((void*)goalInst);

        // Met indicator: show if met, hide if not
        void* metElem = *(void**)(entryArray + entryOffset);
        fn_Element_SetFlagsOnOff(metElem, isMet ? 0 : 1, isMet ? 1 : 0);

        // Failed indicator: show if failed, hide if not
        void* failedElem = *(void**)(entryArray + 8 + entryOffset);
        fn_Element_SetFlagsOnOff(failedElem, hasFailed ? 0 : 1, hasFailed ? 1 : 0);
    }
}
