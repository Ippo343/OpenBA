#pragma once
// Engine::DialogLoaderLoopT — loading screen with UI dialog overlay.
// Runtime-x64.dll, constructor at 0x1800DB0D0, vtable at 0x1801578D0.
// Inherits LoaderLoop (0x48 bytes). Struct size: 0x188 bytes.
//
// Adds a dialog (e.g., splash screen, loading progress) over LoaderLoop's
// background resource loading. The dialog is created by the pure virtual
// CreateDialog() (vtable slot 12) — concrete subclasses in the exe provide
// the specific dialog widget.
//
// Contains an embedded UI::UserInterface (0x128 bytes) at offset +0x60.
// Update() calls UI::UserInterface::Update, Draw() calls UI::UserInterface::Draw.
//
// Concrete subclasses in the exe:
//   - Shining Rock splash (Dialog/ShiningRock.rsc)
//   - Loading screen (Dialog/Loading.rsc)

// ENDGAME B.3.7 (block B1): the REAL class is `Engine::DialogLoaderLoopT`
// (runtime/types/engine/UpdateLoop.h). This exe-side mirror STAYS a POD with its `m_`
// spellings until the caller round renames the users outside block B1's file
// set (code/Game/{Dialog,Game,Goal,Tutorial}, standalone/autostart.cpp,
// engine/funcs/engine_funcs.h); it then becomes `using DialogLoaderLoopT = RtEngine::DialogLoaderLoopT;` (B7-6). Every offset here is
// pinned against the class by tests/unit/engine_loop_abi_test.cpp.

#include "LoaderLoop.h"

struct DialogLoaderLoopT {
    // --- UpdateLoop base (0x00 - 0x27) ---
    UpdateLoopVtable* vtable;   // +0x00: vptr (points to DialogLoaderLoopT vtable)
    void* m_resourceList;       // +0x08
    void* m_internalListA;      // +0x10
    void* m_internalListB;      // +0x18
    int32_t m_loadBit;          // +0x20
    int32_t m_loadProgress;     // +0x24

    // --- LoaderLoop fields (0x28 - 0x47) ---
    UpdateLoop* m_nextLoop;     // +0x28: loop to transition to after loading
    int64_t m_elapsedTicks;     // +0x30: accumulated time since Startup
    int64_t m_delayTicks;       // +0x38: minimum display duration
    uint8_t m_autoAdvance;      // +0x40: auto-advance gate, ctor sets 1 — see
                                //        the correction note in LoaderLoop.h
    uint8_t _pad41[7];          // +0x41: padding

    // --- DialogLoaderLoopT fields (0x48 - 0x5F) ---
    void* m_dialogFilename;     // +0x48: System::String (refcounted wchar_t*, dialog .rsc path)
    void* m_dialogExternal;     // +0x50: Object::External* (loaded dialog resource)
    void* m_dialogInstance;     // +0x58: UI::DialogInstance* (open dialog)

    // --- Embedded UI::UserInterface (0x60 - 0x187) ---
    // Opaque 0x128-byte embedded object. Key sub-offsets (relative to +0x60):
    //   +0x00/+0x08: dialog linked list head/tail
    //   +0x10: dialog count
    //   +0x28: pending-open dialog array {data*, count, capacity}
    //   +0x38: pending-close dialog array
    //   +0xDC: UI scale factor (float, 1.0f)
    uint8_t m_userInterface[0x128]; // +0x60: UI::UserInterface (treated as opaque)
};
static_assert(sizeof(DialogLoaderLoopT) == 0x188, "DialogLoaderLoopT must be 0x188 bytes");
static_assert(offsetof(DialogLoaderLoopT, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(DialogLoaderLoopT, m_nextLoop) == 0x28, "m_nextLoop at +0x28");
static_assert(offsetof(DialogLoaderLoopT, m_dialogFilename) == 0x48, "m_dialogFilename at +0x48");
static_assert(offsetof(DialogLoaderLoopT, m_dialogExternal) == 0x50, "m_dialogExternal at +0x50");
static_assert(offsetof(DialogLoaderLoopT, m_dialogInstance) == 0x58, "m_dialogInstance at +0x58");
static_assert(offsetof(DialogLoaderLoopT, m_userInterface) == 0x60, "m_userInterface at +0x60");
