#pragma once
// UI::DialogInstance -- binary-compatible base struct layout.
// Runtime-x64.dll export. Size = 0xB8 (estimated from subclass field starts).
//
// Base class for all 22 Dialog Controllers in the exe.
// Constructor is UI::DialogInstance::DialogInstance() in Runtime-x64.dll.
// Subclass fields begin at +0xB8.
//
// Vtable varies per subclass (set after base ctor returns).

#include <cstdint>
#include <cstddef>

// Inherits Object::Internal (+0x00..+0x0F: vtable, internalIndex, randomId)
// Constructor: UI::DialogInstance::DialogInstance (Runtime-x64.dll @ 0x180114ca0)
// Destructor:  UI::DialogInstance::~DialogInstance (Runtime-x64.dll @ 0x180114d40)
struct DialogInstance {
    void*    vtable;           // +0x00: vptr (set by subclass after base ctor)
    int32_t  internalIndex;    // +0x08: Object::Internal index in factory
    uint32_t randomId;         // +0x0C: Object::Internal random id
    uint64_t data10;           // +0x10: zeroed
    uint64_t data18;           // +0x18: zeroed
    uint64_t data20;           // +0x20: zeroed
    uint64_t data28;           // +0x28: zeroed
    uint64_t data30;           // +0x30: not initialized by ctor (implicit zero / skipped)
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint32_t data48;           // +0x48: zeroed
    uint8_t  _pad4C[4];        // +0x4C: gap
    uint64_t data50;           // +0x50: zeroed (ptr, cleaned in dtor via vcall)
    uint64_t data58;           // +0x58: zeroed
    int32_t  data60;           // +0x60: initialized to -1 (0xFFFFFFFF)
    uint8_t  _pad64[4];        // +0x64: gap
    uint64_t data68;           // +0x68: array ptr (freed in dtor)
    uint64_t data70;           // +0x70: array count/capacity
    uint64_t data78;           // +0x78: array ptr (freed in dtor)
    uint64_t data80;           // +0x80: array count
    uint64_t data88;           // +0x88: zeroed
    int64_t  data90;           // +0x90: initialized to -1 (0xFFFFFFFFFFFFFFFF)
    uint8_t  data98;           // +0x98: zeroed (byte)
    uint8_t  _pad99[7];        // +0x99: gap
    uint64_t dataA0;           // +0xA0: zeroed
    uint64_t dataA8;           // +0xA8: zeroed
    uint8_t  _padB0[8];        // +0xB0: gap to reach 0xB8
};

static_assert(sizeof(DialogInstance) == 0xB8, "DialogInstance size mismatch");
static_assert(offsetof(DialogInstance, vtable) == 0x00, "");
static_assert(offsetof(DialogInstance, internalIndex) == 0x08, "");
static_assert(offsetof(DialogInstance, data60) == 0x60, "");
static_assert(offsetof(DialogInstance, data90) == 0x90, "");
