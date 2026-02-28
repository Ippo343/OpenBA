#pragma once
// CitizenComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x290.
//
// Decompiled from: CitizenComponent_Ctor (0x140114770)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Largest AI component -- full citizen state (health, happiness, inventory, mesh, etc).
// Contains a Graphics::MeshInstance sub-object at +0xB8 (0x120 bytes).
// Flags: base | 0xCB
//
// Ctor init pattern:
//   *(u64)(+0x28) = 0
//   *(u32)(+0x30) = 0xFFFFFFFF
//   Zero block: +0x34..+0x84 (various u64/u32 writes)
//   *(u32)(+0x88) = 0
//   Zero: +0x90..+0xB4
//   *(u32)(+0xA8) = 0xFFFFFFFF
//   Graphics::MeshInstance::MeshInstance at +0xB8 (0x120 bytes)
//   Post-mesh fields: +0x1D8..+0x288

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace CitizenComponentAddr {
    constexpr uintptr_t Ctor       = 0x140114770;
    constexpr uintptr_t Vtable     = 0x140162580;
    constexpr uintptr_t ScalarDtor = 0x140030270; // vtable[0]
    constexpr uintptr_t Startup    = 0x1400300f0; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140114160; // vtable[3]
    constexpr uintptr_t Serialize  = 0x140114050; // vtable[4]
    constexpr uintptr_t Draw       = 0x140114000; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140113ca0; // vtable[7]
    constexpr uintptr_t Update     = 0x140112e70; // vtable[9]
    constexpr uintptr_t OnUpdate2  = 0x1401140f0; // vtable[11]
}

struct CitizenComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // CitizenComponent fields -- pre-mesh block
    uint64_t data28;       // +0x28: zeroed
    int32_t  data30;       // +0x30: initialized to -1 (0xFFFFFFFF)
    uint32_t data34;       // +0x34: zeroed (from u64 write at 0x34)
    uint32_t data38;       // +0x38: zeroed
    uint32_t data3C;       // +0x3C: zeroed (from u64 write at 0x3C)
    uint32_t data40;       // +0x40: zeroed
    uint32_t data44;       // +0x44: zeroed (from u64 write at 0x44)
    uint32_t data48;       // +0x48: zeroed
    uint32_t data4C;       // +0x4C: zeroed (from u64 write at 0x4C)
    uint32_t data50;       // +0x50: zeroed
    uint32_t data54;       // +0x54: zeroed (from u32 write at 0x54)
    uint64_t data58;       // +0x58: zeroed
    uint64_t data60;       // +0x60: zeroed
    uint64_t data68;       // +0x68: zeroed
    uint64_t data70;       // +0x70: zeroed
    uint64_t data78;       // +0x78: zeroed
    uint32_t data80;       // +0x80: zeroed (from u32 write at 0x80)
    uint8_t  _pad84[4];    // +0x84: gap
    uint32_t data88;       // +0x88: zeroed (from u32 write at 0x88)
    uint8_t  _pad8C[4];    // +0x8C: gap
    uint64_t data90;       // +0x90: zeroed
    uint64_t data98;       // +0x98: zeroed
    uint64_t dataA0;       // +0xA0: zeroed
    int32_t  dataA8;       // +0xA8: initialized to -1 (0xFFFFFFFF)
    uint32_t dataAC;       // +0xAC: zeroed (from u64 write at 0xAC)
    uint32_t dataB0;       // +0xB0: zeroed (upper of u64 at 0xAC)
    uint8_t  _padB4[4];    // +0xB4: gap

    // Graphics::MeshInstance sub-object (0x120 bytes)
    uint8_t  meshInstance[0x120]; // +0xB8: initialized by MeshInstance::MeshInstance()

    // Post-mesh fields
    uint64_t data1D8;      // +0x1D8: set to 0xFFFFFFFFFFFFFFFF (-1)
    uint64_t data1E0;      // +0x1E0: set to 4
    uint64_t data1E8;      // +0x1E8: zeroed
    uint64_t data1F0;      // +0x1F0: zeroed
    uint64_t data1F8;      // +0x1F8: zeroed
    uint64_t data200;      // +0x200: zeroed
    uint64_t data208;      // +0x208: zeroed
    uint32_t data210;      // +0x210: zeroed
    uint8_t  _pad214[4];   // +0x214: gap
    uint64_t data218;      // +0x218: zeroed
    uint8_t  data220;      // +0x220: zeroed (byte write)
    uint8_t  _pad221[7];   // +0x221: gap
    uint64_t data228;      // +0x228: zeroed
    uint64_t data230;      // +0x230: zeroed
    uint64_t data238;      // +0x238: zeroed
    uint8_t  _pad240[8];   // +0x240: gap (not initialized by ctor)
    uint64_t data248;      // +0x248: zeroed
    uint64_t data250;      // +0x250: zeroed
    uint64_t data258;      // +0x258: zeroed (hex 600 = 0x258)
    uint64_t data260;      // +0x260: zeroed
    uint64_t data268;      // +0x268: zeroed
    uint64_t data270;      // +0x270: zeroed
    uint64_t data278;      // +0x278: zeroed
    uint64_t data280;      // +0x280: zeroed
    uint64_t data288;      // +0x288: zeroed
};

static_assert(sizeof(CitizenComponent) == 0x290, "CitizenComponent size mismatch");
static_assert(offsetof(CitizenComponent, vtable)       == 0x00, "");
static_assert(offsetof(CitizenComponent, next)         == 0x08, "");
static_assert(offsetof(CitizenComponent, owner)        == 0x10, "");
static_assert(offsetof(CitizenComponent, flags)        == 0x20, "");
static_assert(offsetof(CitizenComponent, data28)       == 0x28, "");
static_assert(offsetof(CitizenComponent, data30)       == 0x30, "");
static_assert(offsetof(CitizenComponent, data58)       == 0x58, "");
static_assert(offsetof(CitizenComponent, data88)       == 0x88, "");
static_assert(offsetof(CitizenComponent, dataA8)       == 0xA8, "");
static_assert(offsetof(CitizenComponent, meshInstance)  == 0xB8, "");
static_assert(offsetof(CitizenComponent, data1D8)      == 0x1D8, "");
static_assert(offsetof(CitizenComponent, data1E0)      == 0x1E0, "");
static_assert(offsetof(CitizenComponent, data220)      == 0x220, "");
static_assert(offsetof(CitizenComponent, data248)      == 0x248, "");
static_assert(offsetof(CitizenComponent, data288)      == 0x288, "");
