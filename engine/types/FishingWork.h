#pragma once
// FishingWork — Fishing work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: FishingWork_Ctor (0x1400eabe0), FishingWork_Factory (0x14002a280)
// Inherits: Object::Internal -> Work (0x48) -> FishingWork
// NOTE: Extends Work directly (sets Work vtable then overwrites to FishingWork vtable).

#include <cstdint>
#include <cstddef>

namespace FishingWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400eabe0;
    constexpr uintptr_t Factory = 0x14002a280;
    constexpr uintptr_t Vtable  = 0x140161320;
}

struct FishingWork {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161320
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    _field10;           // +0x10
    void*    _field18;           // +0x18
    int32_t  workplaceId;        // +0x20: -1
    uint32_t _field24;           // +0x24
    int32_t  _field28;           // +0x28: -1
    uint32_t _field2C;           // +0x2C
    void*    _field30;           // +0x30
    void*    _field38;           // +0x38
    uint32_t _field40;           // +0x40
    uint8_t  _pad44[4];          // +0x44

    // --- FishingWork fields ---
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
    uint32_t _field68;           // +0x68: zeroed
    uint8_t  _pad6C[4];          // +0x6C: padding
};

static_assert(sizeof(FishingWork) == 0x70, "FishingWork size mismatch");
static_assert(offsetof(FishingWork, vtable) == 0x00, "");
