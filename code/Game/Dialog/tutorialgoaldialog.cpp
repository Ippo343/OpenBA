// code/Game/Dialog/tutorialgoaldialog.cpp
// Clean reimplementation of TutorialGoalDialog vtable slot stubs.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
//
// These are small unhookable slots needed for the standalone exe target.

#include "Dialog/ui_dialogs.h"
#include <cstdint>

// ============================================================
// Slot3 (vtable +0x18, exe 0x1400df080): MOV EAX,1; RET
// ============================================================
// Returns 1 — indicates this External subclass type (constant).
int TutorialGoalDialog_Slot3(void* self) {
    (void)self;
    return 1;
}

// ============================================================
// Slot7 (vtable +0x38, exe 0x140001640): Delegator
// ============================================================
// MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]
// Calls vtable slot 0 (scalar dtor) with flags=0 (destruct without freeing).
void TutorialGoalDialog_Slot7(void* self) {
    typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
    void** vtbl = *(void***)self;
    fn_VDtor_t vdtor = (fn_VDtor_t)vtbl[0];
    vdtor(self, 0);
}
