#pragma once
// UI::DialogController -- binary-compatible base struct layout.
// Runtime-x64.dll export. Size = 0x30 (estimated from subclass field starts).
//
// Base class for all 39 Panel Controllers in the exe.
// Constructor is UI::DialogController::DialogController() in Runtime-x64.dll.
// Subclass fields begin at +0x30.
//
// Vtable varies per subclass (set after base ctor returns).

#include <cstdint>
#include <cstddef>

// Inherits Object::Internal (+0x00..+0x0F: vtable, internalIndex, randomId)
// Constructor: UI::DialogController::DialogController (Runtime-x64.dll @ 0x1800cd320)
// Destructor:  UI::DialogController::~DialogController (Runtime-x64.dll @ 0x1800cd2f0)
struct DialogController {
    void*    vtable;           // +0x00: vptr (set by subclass after base ctor)
    int32_t  internalIndex;    // +0x08: Object::Internal index in factory
    uint32_t randomId;         // +0x0C: Object::Internal random id
    uint64_t data10;           // +0x10: zeroed
    uint64_t data18;           // +0x18: zeroed
    uint64_t data20;           // +0x20: zeroed
    uint64_t data28;           // +0x28: (copy ctor copies this field)
};

static_assert(sizeof(DialogController) == 0x30, "DialogController size mismatch");
static_assert(offsetof(DialogController, vtable) == 0x00, "");
static_assert(offsetof(DialogController, internalIndex) == 0x08, "");
static_assert(offsetof(DialogController, data10) == 0x10, "");
