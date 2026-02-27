#pragma once
// Engine::DialogLoopT — menu/dialog loop with UI overlay (no background loading).
// Runtime-x64.dll, constructor at 0x1800DB3F0, vtable at 0x1801579A0.
// Inherits UpdateLoop (0x28 bytes). Struct size: 0x168 bytes.
//
// Simpler sibling of DialogLoaderLoopT: shows a dialog with UI interaction
// but does NOT use Engine::Loader for background resource loading.
// Like DialogLoaderLoopT, CreateDialog() (vtable slot 12) is pure virtual —
// concrete subclasses in the exe provide the dialog widget.
//
// Contains an embedded UI::UserInterface (0x128 bytes) at offset +0x40.
// Update() calls UI::UserInterface::Update, Draw() calls UI::UserInterface::Draw.
//
// Key differences from DialogLoaderLoopT:
//   - No LoaderLoop base (no m_elapsedTicks, m_delayTicks, m_finished)
//   - No background loading (no Loader::Load, no WaitForSingleObject)
//   - Update: just UI::Update — no sleep, no transition check
//   - Draw: clears to gray (0xFF808080), no time uniforms
//   - Shutdown: just CloseDialog — no loader wait

#include "UpdateLoop.h"

struct DialogLoopT {
    // --- UpdateLoop base (0x00 - 0x27) ---
    UpdateLoopVtable* vtable;   // +0x00: vptr (points to DialogLoopT vtable at 0x1801579A0)
    void* m_resourceList;       // +0x08
    void* m_internalListA;      // +0x10
    void* m_internalListB;      // +0x18
    int32_t m_loadBit;          // +0x20
    int32_t m_loadProgress;     // +0x24

    // --- DialogLoopT fields (0x28 - 0x3F) ---
    void* m_dialogFilename;     // +0x28: System::String (refcounted wchar_t*, dialog .rsc path)
    void* m_dialogExternal;     // +0x30: Object::External* (loaded dialog resource)
    void* m_dialogInstance;     // +0x38: UI::DialogInstance* (open dialog)

    // --- Embedded UI::UserInterface (0x40 - 0x167) ---
    // Opaque 0x128-byte embedded object. Key sub-offsets (relative to +0x40):
    //   +0x00/+0x08: dialog linked list head/tail
    //   +0x10: dialog count
    //   +0x28: pending-open dialog array {data*, count, capacity}
    //   +0x38: pending-close dialog array
    //   +0xDC: UI scale factor (float, 1.0f)
    uint8_t m_userInterface[0x128]; // +0x40: UI::UserInterface (treated as opaque)
};
static_assert(sizeof(DialogLoopT) == 0x168, "DialogLoopT must be 0x168 bytes");
static_assert(offsetof(DialogLoopT, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(DialogLoopT, m_resourceList) == 0x08, "m_resourceList at +0x08");
static_assert(offsetof(DialogLoopT, m_loadBit) == 0x20, "m_loadBit at +0x20");
static_assert(offsetof(DialogLoopT, m_dialogFilename) == 0x28, "m_dialogFilename at +0x28");
static_assert(offsetof(DialogLoopT, m_dialogExternal) == 0x30, "m_dialogExternal at +0x30");
static_assert(offsetof(DialogLoopT, m_dialogInstance) == 0x38, "m_dialogInstance at +0x38");
static_assert(offsetof(DialogLoopT, m_userInterface) == 0x40, "m_userInterface at +0x40");
