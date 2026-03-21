#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// TutorialHighlightDialog::Startup (0x1400E0020)
// Finds "glowGroup", "glow", "arrow" elements by name and stores them
// at +0xC0, +0xC8, +0xD0 respectively.
void TutorialHighlightDialog_Startup(TutorialHighlightDialog* self) {
    uintptr_t s = (uintptr_t)self;

    struct { const char* name; uintptr_t offset; } elements[] = {
        { "glowGroup", 0xC0 },
        { "glow",      0xC8 },
        { "arrow",     0xD0 },
    };

    for (auto& e : elements) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, e.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        fn_String_Dtor(nameStr);

        // Store the found element pointer at the target offset
        void* elem = *(void**)retBuf;
        *(void**)(s + e.offset) = elem;
    }
}

// TutorialHighlightDialog::Update (0x1400dffe0) — vtable slot +0x30
// Dispatches to one of two exe-internal positioning helpers based on dataE8.
// The helpers are in-exe functions resolved by address from the reimpl layer.
// This wrapper exists for architecture consistency; the actual dispatch is in
// engine/reimpl/tutorialhighlightdialog.cpp (Reimpl_Update), which calls the
// exe helpers directly via resolved function pointers.
//
// If dataE8 != 0: uses FUN_1400dfda0 — positions arrow using a world-space rect
//   (stored in dataE8). Hides glowGroup (dataC0) when rect is off-screen.
// If dataE8 == 0: uses FUN_1400dfac0 — positions glowGroup (dataC0) and arrow
//   (dataD0) based on target UI element (dataE0), with rotation from dataD8.
void TutorialHighlightDialog_Update(TutorialHighlightDialog* /*self*/) {
    // Implementation lives in the reimpl layer which resolves exe helper addresses.
    // This declaration exists for the standalone exe target (future).
}

// TutorialHighlightDialog::Load (0x1400e0000) — vtable slot +0x38
// Original is 11 bytes: MOV RAX,[RDX]; ADD [RCX+0xB8],RAX; RET
// Accumulates a delta-time tick count (pointed to by param2) into dataB8,
// which is a System::Time accumulator used by Update for animation timing.
void TutorialHighlightDialog_Load(TutorialHighlightDialog* self, void* param2) {
    self->dataB8 += *(uint64_t*)param2;
}
