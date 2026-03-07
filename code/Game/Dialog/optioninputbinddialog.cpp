// code/Game/Dialog/optioninputbinddialog.cpp
// Clean reimplementation of OptionInputBindDialog methods.
//
// OptionInputBindDialog (0xC8 bytes): Key binding capture dialog.
// Extends MessageDialog (which extends DialogInstance at 0xB8 base).
//
// Layout:
//   +0xB8  (from MessageDialog, uninitialized)
//   +0xC0  keyIndex (int32): captured key/button index
//   +0xC4  inputType (int32): 0=none, 1=keyboard, 2=mouse, -1=initial
//
// The dialog captures a single keypress or mouse button click and stores
// the result. The parent OptionsDialog reads +0xC0 and +0xC4 after close.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// Helper: Find element by name in dialog instance
// ============================================================
static void* FindDialogElem(void* dialogInst, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_DialogInstance_FindElement(dialogInst, &result, nameStr);
    fn_String_Dtor(nameStr);
    return (void*)result;
}

// ============================================================
// Startup (vtable +0x20): Set up input bind dialog UI
// ============================================================
// Original: 0x14006d140
//
// Finds buttonCancel/Ok/Yes/No elements by name, sets element IDs 0-3.
// This is identical to MessageDialog::Slot4 — the base class setup.

void OptionInputBindDialog_Startup(OptionInputBindDialog* self) {
    void* cancelBtn = FindDialogElem(self, "buttonCancel");
    void* okBtn     = FindDialogElem(self, "buttonOk");
    void* yesBtn    = FindDialogElem(self, "buttonYes");
    void* noBtn     = FindDialogElem(self, "buttonNo");

    if (cancelBtn) fn_Element_SetElementId(cancelBtn, 0);
    if (okBtn)     fn_Element_SetElementId(okBtn, 1);
    if (yesBtn)    fn_Element_SetElementId(yesBtn, 2);
    if (noBtn)     fn_Element_SetElementId(noBtn, 3);
}

// ============================================================
// Load (vtable +0x38): Scan for key/mouse input releases
// ============================================================
// Original: 0x140071c80
//
// Polls Input::Device::Released for all keyboard keys (0..0x68)
// and mouse buttons (4..0xB). When a release is detected, stores
// the key index at +0xC0, input type (1=keyboard, 2=mouse) at +0xC4,
// and calls vtable[10] (OnOk) to close the dialog with result=1.

void OptionInputBindDialog_Load(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* inputDevice = *g_pInputDeviceSingleton;

    // Scan keyboard keys (InputDevice=1, keys 0..0x68)
    for (int key = 0; key < 0x69; key++) {
        if (fn_Input_Device_Released(inputDevice, 1, 0, key)) {
            *(int*)(s + 0xC4) = 1;  // keyboard
            *(int*)(s + 0xC0) = key;
            // Call vtable[10] = OnOk
            void** vtable = *(void***)self;
            typedef void (*fn_OnOk_t)(void*);
            ((fn_OnOk_t)vtable[10])(self);
            return;
        }
    }

    // Scan mouse buttons (InputDevice=2, buttons 4..0xB)
    for (int btn = 4; btn <= 0xB; btn++) {
        if (fn_Input_Device_Released(inputDevice, 2, 0, btn)) {
            *(int*)(s + 0xC4) = 2;  // mouse
            *(int*)(s + 0xC0) = btn;
            // Call vtable[10] = OnOk
            void** vtable = *(void***)self;
            typedef void (*fn_OnOk_t)(void*);
            ((fn_OnOk_t)vtable[10])(self);
            return;
        }
    }
}

// ============================================================
// OnOk (vtable +0x50): Set result value 1 (OK), close dialog
// ============================================================
// Original: 0x14006cff0

void OptionInputBindDialog_OnOk(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 1;
    uint64_t basePtr = 0;
    fn_InternalBasePtr_Ctor(&basePtr, self);
    void* ui = *(void**)(s + 0x10);
    fn_UI_CloseDialog(ui, &basePtr);
}

// ============================================================
// OnCancel (vtable +0x58): Set result value 0 (Cancel), close dialog
// ============================================================
// Original: 0x14006d030

void OptionInputBindDialog_OnCancel(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 0;
    uint64_t basePtr = 0;
    fn_InternalBasePtr_Ctor(&basePtr, self);
    void* ui = *(void**)(s + 0x10);
    fn_UI_CloseDialog(ui, &basePtr);
}

// ============================================================
// OnButton2 (vtable +0x60): Set result value 2, close dialog
// ============================================================
// Original: 0x14006cfb0

void OptionInputBindDialog_OnButton2(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 2;
    uint64_t basePtr = 0;
    fn_InternalBasePtr_Ctor(&basePtr, self);
    void* ui = *(void**)(s + 0x10);
    fn_UI_CloseDialog(ui, &basePtr);
}

// ============================================================
// OnButton3 (vtable +0x68): Set result value 3, close dialog
// ============================================================
// Original: 0x14006cf70

void OptionInputBindDialog_OnButton3(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 3;
    uint64_t basePtr = 0;
    fn_InternalBasePtr_Ctor(&basePtr, self);
    void* ui = *(void**)(s + 0x10);
    fn_UI_CloseDialog(ui, &basePtr);
}
