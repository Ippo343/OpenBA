#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// MessageDialog::Slot4 (0x14006D140) — Setup
// Finds buttonCancel, buttonOk, buttonYes, buttonNo elements by name.
// Sets element IDs: Cancel=0, Ok=1, Yes=2, No=3.
void MessageDialog_Slot4(MessageDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find and ID each button element
    struct { const char* name; int id; } buttons[] = {
        { "buttonCancel", 0 },
        { "buttonOk",     1 },
        { "buttonYes",    2 },
        { "buttonNo",     3 },
    };

    for (auto& btn : buttons) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, btn.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (elem) {
            fn_Element_SetElementId(elem, btn.id);
        }
    }
}

// ============================================================
// MessageDialog::Slot5 (vtable +0x28) — Shutdown
// ============================================================
// 3-byte no-op: XOR EAX,EAX; RET. Returns 0.
// Original: 0x14006D0C0.

int MessageDialog_Slot5(MessageDialog* /*self*/) {
    return 0;
}

// ============================================================
// MessageDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// Dispatches button clicks to the appropriate result handler.
// When msg==0x12 (button click):
//   id=0 → vtable[+0x58] (ButtonCancel/Slot11)
//   id=1 → vtable[+0x50] (ButtonOk/Slot10)
//   id=2 → vtable[+0x60] (ButtonYes/Slot12)
//   id=3 → vtable[+0x68] (ButtonNo/Slot13)
//
// Original: 0x14006D070.

void MessageDialog_Slot8(MessageDialog* self, void* /*param2*/, int param3, int param4) {
    if (param3 != 0x12)
        return;

    // Dispatch through vtable — allows subclass overrides
    uintptr_t* vtable = *(uintptr_t**)self;
    typedef void (*fn_Handler_t)(void*);

    switch (param4) {
    case 0:  // Cancel → vtable[+0x58 / 8 = 11]
        ((fn_Handler_t)vtable[0x58 / 8])(self);
        break;
    case 1:  // Ok → vtable[+0x50 / 8 = 10]
        ((fn_Handler_t)vtable[0x50 / 8])(self);
        break;
    case 2:  // Yes → vtable[+0x60 / 8 = 12]
        ((fn_Handler_t)vtable[0x60 / 8])(self);
        break;
    case 3:  // No → vtable[+0x68 / 8 = 13]
        ((fn_Handler_t)vtable[0x68 / 8])(self);
        break;
    }
}

// ============================================================
// MessageDialog::Slot10 (vtable +0x50) — ButtonOk
// ============================================================
// Sets result=1 at self+0x24, closes dialog.
// Original: 0x14006CFF0.

void MessageDialog_Slot10(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 1;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot11 (vtable +0x58) — ButtonCancel
// ============================================================
// Sets result=0 at self+0x24, closes dialog.
// Original: 0x14006D030.

void MessageDialog_Slot11(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 0;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot12 (vtable +0x60) — ButtonYes
// ============================================================
// Sets result=2 at self+0x24, closes dialog.
// Original: 0x14006CFB0.

void MessageDialog_Slot12(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 2;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot13 (vtable +0x68) — ButtonNo
// ============================================================
// Sets result=3 at self+0x24, closes dialog.
// Original: 0x14006CF70.

void MessageDialog_Slot13(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 3;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}
