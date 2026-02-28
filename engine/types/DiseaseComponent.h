#pragma once
// DiseaseComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: DiseaseComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140159290
//   +0x28..+0x40 = 0 (4 qwords), +0x48 = 0 (dword), +0x50 = 0 (qword)
//   flags |= 0x8B (Serialize | Update | bit3 | bit7)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace DiseaseComponentAddr {
    constexpr uintptr_t Vtable = 0x140159290;
}

struct DiseaseComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
    void*    field38;      // +0x38: zeroed
    void*    field40;      // +0x40: zeroed
    uint32_t field48;      // +0x48: zeroed (dword)
    uint8_t  _pad4C[4];    // +0x4C: padding
    void*    field50;      // +0x50: zeroed
};

static_assert(sizeof(DiseaseComponent) == 0x58, "DiseaseComponent size mismatch");
static_assert(offsetof(DiseaseComponent, vtable) == 0x00, "");
static_assert(offsetof(DiseaseComponent, flags) == 0x20, "");
static_assert(offsetof(DiseaseComponent, field28) == 0x28, "");
static_assert(offsetof(DiseaseComponent, field48) == 0x48, "");
static_assert(offsetof(DiseaseComponent, field50) == 0x50, "");
