#pragma once
// TendArea — Area tending work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: TendArea_Ctor (0x1400f5a00)
// Inherits: Object::Internal -> Work (0x48) -> TendArea
// Extends Work directly.
//
// BST node layout (simple, 0x20 bytes):
//   +0x00: key (InternalBasePtr, citizen entity)
//   +0x08: parent pointer
//   +0x10: left child
//   +0x18: right child

#include <cstdint>
#include <cstddef>

namespace TendAreaAddr {
    constexpr uintptr_t Ctor    = 0x1400f5a00;
    constexpr uintptr_t Vtable  = 0x140161990;
}

struct TendArea {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161990
    uint8_t  _workBase[0x40];   // +0x08: Work fields
    // --- TendArea fields ---
    void*    bstRoot;            // +0x48: BST root for assigned workers
    uint32_t workerCount;        // +0x50: number of workers in BST
    uint8_t  _pad54[4];          // +0x54: padding
    uint32_t tendMode;           // +0x58: 0=crop/area, nonzero=orchard mode (serialized int)
};

static_assert(sizeof(TendArea) == 0x60, "TendArea size mismatch");
static_assert(offsetof(TendArea, vtable) == 0x00, "");
static_assert(offsetof(TendArea, bstRoot) == 0x48, "");
static_assert(offsetof(TendArea, workerCount) == 0x50, "");
static_assert(offsetof(TendArea, tendMode) == 0x58, "");
