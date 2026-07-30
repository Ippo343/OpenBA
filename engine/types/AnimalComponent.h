#pragma once
// AnimalComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: AnimalComponent_Ctor (0x140106fc0)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Flags: base | 0xC9
//
// Ctor init pattern:
//   *(u32)(+0x28)=0, *(u64)(+0x30)=0, *(u64)(+0x38)=0,
//   byte(+0x40)=0, *(u64)(+0x44)=0, *(u64)(+0x50)=0

#include <cstdint>
#include <cstddef>
#include "Component.h"

// Vtable slots, read back from .rdata at 0x140162110 (2026-07-28). Slots 4 and 6
// were previously labelled "Serialize" and "Draw"; Ghidra and the decompiled
// bodies say otherwise — 0x140106d50 rolls the death age off the description
// (Startup) and 0x140106c20 is the register-time OnAdded one-shot (the project's
// established +0x30 / index 6 mapping). Every slot not listed is a shared
// Component base stub in 0x140118xxx.
namespace AnimalComponentAddr {
    constexpr uintptr_t Ctor       = 0x140106fc0;
    constexpr uintptr_t Vtable     = 0x140162110;
    constexpr uintptr_t ScalarDtor = 0x14002f170; // vtable[0]
    constexpr uintptr_t BaseInit   = 0x14002f000; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140106e40; // vtable[3]
    constexpr uintptr_t Startup    = 0x140106d50; // vtable[4]  (was "Serialize")
    constexpr uintptr_t OnAdded    = 0x140106c20; // vtable[6]  (was "Draw")
    constexpr uintptr_t Update     = 0x140106420; // vtable[9]
    constexpr uintptr_t OnUpdate2  = 0x140106dd0; // vtable[11] (not ported)

    // Private helpers of Update, all reimplemented in animalcomponent.cpp.
    constexpr uintptr_t SetBounds      = 0x140105e20; // ported as PastureComponent_AnimalSetBounds
    constexpr uintptr_t FindHabitat    = 0x140106020;
    constexpr uintptr_t SetWanderTimer = 0x140106250;
    constexpr uintptr_t UpdateBehavior = 0x140107320;
}

// Field meanings recovered from the Update/Startup/OnAdded bodies. Names are
// deliberately left as dataNN so sibling lanes that already reference them keep
// compiling; animalcomponent.cpp accesses them by byte offset.
struct AnimalComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10  Entity InternalBasePtr
    void*    _unk18;       // +0x18  AnimalDescription (validated, tid 0x1401c6fd0)
    uint32_t flags;        // +0x20  bit 20 penned, 21 graze latch,
                           //        22 revalidate habitat, 23 grazing enabled
    uint8_t  _pad24[4];    // +0x24

    // AnimalComponent fields
    uint32_t data28;       // +0x28  habitat cell index (y * mapDim + x)
    uint8_t  _pad2C[4];    // +0x2C: padding/gap
    uint64_t data30;       // +0x30  System::Time move timer
    uint64_t data38;       // +0x38  wander box: short x0, y0, width, height
    uint8_t  data40;       // +0x40  non-zero once this animal grazes
    uint8_t  _pad41[3];    // +0x41: gap
    uint32_t data44;       // +0x44  float age, in hours
    uint32_t data48;       // +0x48  float age of death (rolled by Startup)
    uint8_t  _pad4C[4];    // +0x4C: gap
    uint64_t data50;       // +0x50  System::Time graze timer
};

static_assert(sizeof(AnimalComponent) == 0x58, "AnimalComponent size mismatch");
static_assert(offsetof(AnimalComponent, vtable) == 0x00, "");
static_assert(offsetof(AnimalComponent, next)   == 0x08, "");
static_assert(offsetof(AnimalComponent, owner)  == 0x10, "");
static_assert(offsetof(AnimalComponent, flags)  == 0x20, "");
static_assert(offsetof(AnimalComponent, data28) == 0x28, "");
static_assert(offsetof(AnimalComponent, data30) == 0x30, "");
static_assert(offsetof(AnimalComponent, data38) == 0x38, "");
static_assert(offsetof(AnimalComponent, data40) == 0x40, "");
static_assert(offsetof(AnimalComponent, data44) == 0x44, "");
static_assert(offsetof(AnimalComponent, data48) == 0x48, "");
static_assert(offsetof(AnimalComponent, data50) == 0x50, "");
