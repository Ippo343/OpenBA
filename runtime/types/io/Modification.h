#pragma once
// ============================================================================
// runtime/types/io/Modification.h — IO::Modification binary layout
// ============================================================================
// Wave 4, lane D modelled only the three fields
// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z (retail 0x1800aea10)
// touches. WAVE 9 LANE MOD completes the record, because the mod-entry
// lifecycle triple (copy ctor 0x1800101e0, operator= 0x180026ef0, dtor
// 0x180011460) walks EVERY member of Mod and a port of them needs the whole
// 0x80 laid out, not just the stride.
//
// Every offset below was read out of one of those three bodies, which is the
// strongest source available for a layout: the copy ctor visits the members in
// declaration order and its element helper tells you the type of each.
//
//   +0x00/08/10  three refcounted System::String   (adopt/release idiom)
//   +0x18..0x2c  six int32 copied field by field
//   +0x30        Collection::Array<int32>          (op= 0x180011110, stride 4)
//   +0x40        Collection::Array<uint8>          (op= 0x180011220, stride 1)
//   +0x50        bool enabled                      (MOVZX/MOV byte)
//   +0x58        System::Filename                  (ctor-copy 0x1800067d0)
//   +0x60        Collection::Set<Filename> root+count  (clone 0x1800104f0)
//   +0x70        Collection::Array<Filename>       (op= 0x1800091f0, stride 8)
//
// The two Modification header flags come from their own accessors, which is
// why they are named rather than guessed:
//   +0x00 _dirty          — ?IsDirty@Modification@IO@@QEBA_NXZ is `MOVZX EAX,
//                           byte ptr [RCX]; RET`.
//   +0x01 _savingSettings — ?EnableSavingSettings@ (0x1800aead0) is the only
//                           writer; the ctor (0x1800b04b0) opens with
//                           `MOV word ptr [RCX],0x101`, i.e. BOTH start at 1.
//                           That is why SaveList/RefreshList take their
//                           registry branch on a normal boot.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../system/Filename.h"

namespace RtIO {

// Collection::Array<T> header, 16 bytes and T-independent (Collection.h in the
// Wave 2 tree static_asserts the same thing). Spelled locally so this header
// stays includable from engine/ without dragging the collection tree in.
struct ModArray {
    void*   data;       // +0x00
    int32_t size;       // +0x08
    int32_t capacity;   // +0x0c
};
static_assert(sizeof(ModArray) == 0x10, "Collection::Array is 16 bytes");

// One node of the Filename-keyed set at Mod+0x60. Allocated at 0x180010570
// (`MOV EDX,0x28` size, `LEA R8D,[RDX-0x20]` align 8) and cloned at 0x1800104f0.
struct ModSetNode {
    RtSystem::Filename key;      // +0x00 — copy-constructed by 0x1800067d0
    ModSetNode*        parent;   // +0x08
    ModSetNode*        left;     // +0x10
    ModSetNode*        right;    // +0x18
    uint16_t           w20;      // +0x20 — copied verbatim by the clone
    uint16_t           w22;      // +0x22 — ditto
    uint32_t           _pad24;   // +0x24
};
static_assert(sizeof(ModSetNode) == 0x28, "node alloc at 0x180010570 asks for 0x28");
static_assert(offsetof(ModSetNode, parent) == 0x08, "0x1800105e8 MOV [RDI+8],RSI");
static_assert(offsetof(ModSetNode, left)   == 0x10, "0x180010535 MOV [RBX+0x10],RAX");
static_assert(offsetof(ModSetNode, right)  == 0x18, "0x18001054e MOV [RBX+0x18],RAX");

struct ModSet {
    ModSetNode* root;    // +0x00
    int32_t     count;   // +0x08
    int32_t     _pad0c;
};
static_assert(sizeof(ModSet) == 0x10, "CopyCtor zeroes [RSI] and [RSI+8] then strides 0x10");

// One entry of Modification's mod array.
struct Mod {
    char*    name;          // +0x00 — refcounted String
    char*    author;        // +0x08 — refcounted String
    char*    description;   // +0x10 — refcounted String

    int32_t  i18;           // +0x18
    int32_t  i1c;           // +0x1c
    int32_t  order;         // +0x20 — SaveList writes the loop index here
    int32_t  i24;           // +0x24
    int32_t  i28;           // +0x28
    int32_t  i2c;           // +0x2c

    ModArray ints;          // +0x30 — Array<int32>
    ModArray bytes;         // +0x40 — Array<uint8>

    uint8_t  enabled;       // +0x50 — Enable() writes this byte
    uint8_t  _pad51[7];

    RtSystem::Filename file;   // +0x58 — the mod's .pkm path; Find()'s key

    ModSet   set;           // +0x60 — Filename-keyed set
    ModArray files;         // +0x70 — Array<Filename>
};
static_assert(sizeof(Mod) == 0x80,
              "Find() 0x1800aea10: `lVar3 = lVar3 + 0x80` strides the array by this");
static_assert(offsetof(Mod, order)   == 0x20, "SaveList 0x1800b0654");
static_assert(offsetof(Mod, ints)    == 0x30, "CopyCtor 0x180010265 ADD RCX,0x30");
static_assert(offsetof(Mod, bytes)   == 0x40, "CopyCtor 0x180010283 LEA RCX,[RBX+0x40]");
static_assert(offsetof(Mod, enabled) == 0x50, "Enable 0x1800aeabe MOV [RDX+RAX+0x50],R8B");
static_assert(offsetof(Mod, file)    == 0x58,
              "Find() 0x1800aea10: `*(char**)(*(this+8) + 0x58 + lVar3)`");
static_assert(offsetof(Mod, set)     == 0x60, "CopyCtor 0x1800102ae LEA R14,[RDI+0x60]");
static_assert(offsetof(Mod, files)   == 0x70, "CopyCtor 0x1800102e0 LEA RCX,[RBX+0x70]");

struct Modification {
    uint8_t  dirty;           // +0x00 — IsDirty 0x18000daa0
    uint8_t  savingSettings;  // +0x01 — EnableSavingSettings 0x1800aead0
    uint8_t  _pad02[6];
    Mod*     mods;    // +0x08 — Find()'s `*(longlong*)(this + 8)`
    int32_t  count;   // +0x10 — Find()'s `*(int*)(this + 0x10)`
    int32_t  capacity;// +0x14
    Mod*     savedMods;     // +0x18 — the backup EnableSavingSettings(false) makes
    int32_t  savedCount;    // +0x20
    int32_t  savedCapacity; // +0x24
};
static_assert(sizeof(Modification) == 0x28, "ctor 0x1800b04b0 zeroes +0x08..+0x27");
static_assert(offsetof(Modification, mods)  == 0x08, "see field comment");
static_assert(offsetof(Modification, count) == 0x10, "see field comment");
static_assert(offsetof(Modification, savedMods) == 0x18,
              "SaveList 0x1800b0619: ModArrayCopy(this+0x18, this+0x08)");

}  // namespace RtIO
