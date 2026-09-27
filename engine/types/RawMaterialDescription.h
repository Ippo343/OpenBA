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
// Type ID addr: 0x1401c7cb8 (TypeId_RawMaterialDescription in engine_funcs.h)
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

// RawMaterialFlags bitfield — field +0x5C, tested via bitwise AND.
//
// [CORRECTED 2026-07-19 — binary-verified] The old table here naively followed the
// ToolKit .rsc enum listing order (Edible=bit0, Fuel=bit5, ...) and was WRONG.
// Ground truth recovered from the exe:
//   * Eat loop (0x1401152e0) ORs flag bits {0,1,2,4} into the citizen's diet mask
//     (+0x54) — those are the four diet groups; bit 3 is edible-without-group (ale).
//   * Storage_GetResourceCount (0x14012a9d0) sums rows with bit 5  -> 0x20 = EDIBLE.
//   * Storage_GetResourceCount2 (0x14012ab00) + the residence heat tick
//     (0x140129970) key on bit 6                                   -> 0x40 = FUEL.
//   * Entity_impl_ef40 (0x14008ef40) category map: cat0->bit5, cat1->bit6,
//     cat2->bit9, cat3->bit13, cat4->bit14.
//   * Start-resource seed masks (0x1400a1f00): 0x20 = starting food,
//     0x40 = starting firewood.
namespace RawMaterialFlags {
    // Diet groups (bits 0/1/2/4; exact name->bit order among the four unverified):
    constexpr uint32_t DietGroup0 = 0x00000001;
    constexpr uint32_t DietGroup1 = 0x00000002;
    constexpr uint32_t DietGroup2 = 0x00000004;
    constexpr uint32_t EdibleNoGroup = 0x00000008;  // bit 3: edible w/o diet group (ale)
    constexpr uint32_t DietGroup3 = 0x00000010;
    constexpr uint32_t Edible     = 0x00000020;  // bit 5 — EDIBLE master flag (verified)
    constexpr uint32_t Fuel       = 0x00000040;  // bit 6 — FIREWOOD (verified)
    // Bits 7+ — names below are UNVERIFIED toolkit-order guesses shifted to match
    // the recovered anchors; treat with caution until each is binary-confirmed.
    constexpr uint32_t Wood       = 0x00000080;  // bit 7  (CountResources entry +0x60)
    constexpr uint32_t Stone      = 0x00000100;  // bit 8
    constexpr uint32_t Iron       = 0x00000200;  // bit 9  (ef40 cat2)
    constexpr uint32_t Tool       = 0x00000400;  // bit 10 \  0x1c40 (bits 6,10,11,12)
    constexpr uint32_t Health     = 0x00000800;  // bit 11  } = the "durable goods"
    constexpr uint32_t Clothing   = 0x00001000;  // bit 12 /  notify-summary split
    constexpr uint32_t Alcohol    = 0x00002000;  // bit 13 (ef40 cat3)
    // bit 14 (0x4000): ef40 cat4 / start-seed mask member (tools or clothing class)
    // Custom flags continue upward (modding).
}

// ENDGAME B.3.8 K1: `RawMaterialDescription` is a real class now (B8-4, one
// definition per name) — code/Game/Component/RawMaterialDescription.h, with the
// field names this POD gave (+0x54 `_pad54` is `_i54`: the ctor zeroes it and
// Serialize writes it as an int). The address and flag namespaces above stay.
#include "Component/RawMaterialDescription.h"
