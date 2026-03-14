#pragma once
// RawMaterialDescription — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Description type for raw material items (logs, stone, iron, food, etc.).
// Registered as "RawMaterialDescription" inheriting from "Description".
//
// Registration: FUN_14014f790 -> BaseTypeInfo at DAT_1401c7cb8
// Factory:      FUN_1400372d0 -> MemAlloc(0x60, 8)
// Constructor:  FUN_1401296a0
// Vtable:       0x1401641f0
// Type ID addr: 0x1401c7cb8 (g_TypeId_RawMaterialDescription in engine_funcs.h)
//
// ToolKit .rsc definition (e.g., RawMaterialWood.rsc):
//   RawMaterialDescription rawmaterial {
//       SpriteSheet _spriteSheet; StringTable _stringTable;
//       String _name; String _nameSmall;
//       int _lowCreateCount; int _highCreateCount;
//       int _weight; int _createChance;
//       RawMaterialFlags _flags; float _carryScale;
//   }
//
// Used by: BuildUI (sprite from +0x20/+0x30), BuildRequirementUI (sprite from
//   +0x20/+0x38), StorageComponent_QueryCount (+0x48 weight), various AI state
//   functions (+0x5C flags), Game_DestroyAllRawMaterials (entity lookup).

#include <cstdint>
#include <cstddef>

namespace RawMaterialDescriptionAddr {
    constexpr uintptr_t RegisterType = 0x14014f790;
    constexpr uintptr_t Factory      = 0x1400372d0;
    constexpr uintptr_t Ctor         = 0x1401296a0;
    constexpr uintptr_t Vtable       = 0x1401641f0;
    constexpr uintptr_t TypeIdGlobal = 0x1401c7cb8;  // DAT_1401c7cb8
}

// RawMaterialFlags bitfield (from ToolKit .rsc enum)
// Used at field +0x5C, tested via bitwise AND.
namespace RawMaterialFlags {
    constexpr uint32_t Edible    = 0x00000001;
    constexpr uint32_t Protein   = 0x00000002;
    constexpr uint32_t Fruit     = 0x00000004;
    constexpr uint32_t Vegetable = 0x00000008;
    constexpr uint32_t Grain     = 0x00000010;
    constexpr uint32_t Fuel      = 0x00000020;
    constexpr uint32_t Tool      = 0x00000040;
    constexpr uint32_t Wood      = 0x00000080;
    constexpr uint32_t Stone     = 0x00000100;
    constexpr uint32_t Iron      = 0x00000200;
    constexpr uint32_t Health    = 0x00000400;
    constexpr uint32_t Clothing  = 0x00000800;
    constexpr uint32_t Textile   = 0x00001000;
    constexpr uint32_t Alcohol   = 0x00002000;
    // Custom0-Custom9: 0x00004000 - 0x00800000 (10 bits for modding)
}

struct RawMaterialDescription {
    // Object::External base (0x20 bytes)
    void*    vtable;           // +0x00: -> 0x1401641f0
    uint8_t  _extBase[0x18];   // +0x08: Object::External inherited fields

    // RawMaterialDescription own fields
    void*    _spriteSheet;     // +0x20: SpriteSheet* — .rsc: _spriteSheet
                               //   Used as arg1 to fn_Sprite_Ctor (BuildUI, BuildReqUI)
    void*    _stringTable;     // +0x28: StringTable* — .rsc: _stringTable
                               //   Used as arg1 to fn_UIString_Ctor
    uint8_t  _name[8];         // +0x30: System::String — .rsc: _name (e.g., "Log")
                               //   Full-size sprite name; BuildUI uses for fn_Sprite_Ctor arg2
    uint8_t  _nameSmall[8];    // +0x38: System::String — .rsc: _nameSmall (e.g., "LogSmall")
                               //   Small sprite name; BuildRequirementUI uses for fn_Sprite_Ctor arg2
    int32_t  _lowCreateCount;  // +0x40: .rsc default = 1 (ctor), e.g., 2 for wood
    int32_t  _highCreateCount; // +0x44: .rsc default = 1 (ctor), e.g., 3 for wood
    int32_t  _weight;          // +0x48: .rsc default = 1 (ctor), e.g., 11 for wood
                               //   Used by StorageComponent_QueryCount (0x14012cf10)
    int32_t  _createChance;    // +0x4C: .rsc default = 0 (not set by ctor), e.g., 1
    float    _carryScale;      // +0x50: .rsc default = 0.33f (0x3ea8f5c3)
    uint32_t _pad54;           // +0x54: zeroed (from 8-byte write at +0x50)
    uint8_t  _unknown58;       // +0x58: bool? default = 0 (false)
    uint8_t  _pad59[3];        // +0x59: padding
    uint32_t _flags;           // +0x5C: RawMaterialFlags bitfield — .rsc: _flags
                               //   Tested via bitwise AND in FUN_14004a550, FUN_14004a780,
                               //   CitizenAI_State_GatherResources, etc.
};

static_assert(sizeof(RawMaterialDescription) == 0x60, "RawMaterialDescription size mismatch");
static_assert(offsetof(RawMaterialDescription, vtable) == 0x00, "");
static_assert(offsetof(RawMaterialDescription, _spriteSheet) == 0x20, "");
static_assert(offsetof(RawMaterialDescription, _stringTable) == 0x28, "");
static_assert(offsetof(RawMaterialDescription, _name) == 0x30, "");
static_assert(offsetof(RawMaterialDescription, _nameSmall) == 0x38, "");
static_assert(offsetof(RawMaterialDescription, _lowCreateCount) == 0x40, "");
static_assert(offsetof(RawMaterialDescription, _highCreateCount) == 0x44, "");
static_assert(offsetof(RawMaterialDescription, _weight) == 0x48, "");
static_assert(offsetof(RawMaterialDescription, _createChance) == 0x4C, "");
static_assert(offsetof(RawMaterialDescription, _carryScale) == 0x50, "");
static_assert(offsetof(RawMaterialDescription, _unknown58) == 0x58, "");
static_assert(offsetof(RawMaterialDescription, _flags) == 0x5C, "");
