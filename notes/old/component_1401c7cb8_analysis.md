# Component Type ID at DAT_1401c7cb8 — Analysis

## Summary

**DAT_1401c7cb8 is the type ID for `RawMaterialDescription`**, a Description class
(inherits from `Object::External` -> `Description`). It is NOT `ResourceType`,
NOT `CitizenComponent`, and NOT a Component class at all.

### Evidence

The registration function at `0x14014f790` definitively identifies the class:

```c
// FUN_14014f790 — RegisterType for RawMaterialDescription
void FUN_14014f790(void) {
    Object::BaseTypeInfo::BaseTypeInfo(
        (BaseTypeInfo *)&DAT_1401c7cb8,
        "RawMaterialDescription",    // <-- class name
        "Description",               // <-- parent class
        (char *)0x0,
        FUN_1400372d0,               // factory
        &LAB_140037310               // createInPlace
    );
}
```

The factory at `0x1400372d0` allocates **0x60 bytes** (96 bytes).

---

## Existing Naming Errors

The codebase has THREE conflicting names for this address:

| Location | Name Used | Correct? |
|----------|-----------|----------|
| `engine_funcs.h:583` | `g_TypeId_ResourceType` | WRONG |
| `engine_funcs.h:2714` | `g_TypeId_CitizenComponent` | WRONG |
| `rawmaterialcomponent.cpp:65` | `g_pBuildingDataTypeId` | WRONG |
| `code/Game/Dialog/buildui.cpp` | comments say "ResourceType" | WRONG |
| `code/Game/Dialog/buildrequirementui.cpp` | comments say "ResourceType" | WRONG |
| `code/Game/Dialog/debugui.cpp:343` | comments say "CitizenComponent" | WRONG |

**All should be renamed to `g_TypeId_RawMaterialDescription`.**

Additionally, `Game_KillAllCitizens` (Ghidra name) does NOT kill citizens. It destroys
all entities that have a `RawMaterialDescription` component (loose raw material items
like logs, stone, iron on the ground). Should be renamed to `Game_DestroyAllRawMaterials`.

---

## Field Layout (0x60 bytes)

Derived from constructor at `0x1401296a0`, .rsc definition in ToolKit, and xref analysis.

| Offset | Size | Type | Field Name | Default | Evidence |
|--------|------|------|------------|---------|----------|
| +0x00 | 8 | void* | vtable | `0x1401641f0` | Constructor |
| +0x08 | 24 | | _extBase (Object::External) | | Inherited |
| +0x20 | 8 | void* | _spriteSheet | 0 (nullptr) | .rsc: `SpriteSheet _spriteSheet`; BuildUI/BuildReqUI pass to `fn_Sprite_Ctor` arg1 |
| +0x28 | 8 | void* | _stringTable | 0 (nullptr) | .rsc: `StringTable _stringTable`; BuildReqUI::Slot4 uses for `fn_UIString_Ctor` |
[retail data elided]
[retail data elided]
[retail data elided]
[retail data elided]
[retail data elided]
[retail data elided]
[retail data elided]
| +0x54 | 4 | | (padding) | 0 | Zero from 8-byte write at +0x50 |
| +0x58 | 1 | bool | _unknown58 | false | Purpose unknown |
| +0x59 | 3 | | (padding) | | |
| +0x5C | 4 | uint32 | _flags (RawMaterialFlags) | 0 | .rsc: `RawMaterialFlags _flags = Wood`; bitwise AND at `FUN_14004a550` |

**Total size: 0x60 (96 bytes)** — confirmed by factory MemAlloc.

---

## Key Xref Evidence

### Field +0x48 (_weight) — StorageComponent_QueryCount (0x14012cf10)
```c
iVar9 = iVar9 + *(int *)(pBVar6 + 0x48) * *(int *)(lVar1 + -8 + lVar11);
```
Multiplies `_weight` by stored quantity to compute total weight. Confirms +0x48 = _weight.

### Field +0x5C (_flags) — FUN_14004a550 (0x14004a550)
```c
bVar4 = (bool)(bVar4 | (*(uint *)(pBVar12 + 0x28) & *(uint *)(pBVar9 + 0x5c)) != 0);
```
Bitwise AND of a mask against +0x5C. Confirms +0x5C = RawMaterialFlags.

### Fields +0x20, +0x30 — BuildUI::Slot4 (0x14005e7c0, Ghidra decompilation)
```c
UI::Sprite::Sprite(sprite, *(undefined8 *)(pBVar11 + 0x20), pBVar11 + 0x28);
// Note: BuildUI reimpl code says +0x30 for sprite name, but Ghidra shows +0x28
// The Ghidra pBVar11 is actually a BuildRequirementComponent, not RawMaterialDescription
```

### Fields +0x20, +0x38 — BuildRequirementUI::Slot4 (0x14005e7c0, part 2)
```c
UI::Sprite::Sprite(local_58, *(undefined8 *)(pBVar15 + 0x20), pBVar15 + 0x38);
```
Uses +0x20 (SpriteSheet) and +0x38 (_nameSmall) for small icons. This is confirmed
from the Ghidra decompilation of `FUN_14005e7c0` at line ~270 of the decompilation.

### Game_KillAllCitizens (0x140060010) — actually destroys raw material entities
Iterates all ComponentEntity objects, searches for RawMaterialDescription component
via `DAT_1401c7cb8`, and calls `Game_DestroyEntity` on matches. This destroys loose
resource items, not citizens.

---

## Relationship to Other Description Types

RawMaterialDescription and ValueDescription both appear on resource entities:

```
// From RawMaterialWood.rsc:
ComponentDescription resource {
    [retail data elided]
}

ValueDescription value {
    SpriteSheet _spriteSheet; String _spriteName;     // +0x20, +0x28
    StringTable _stringTable; String _stringName;     // +0x30, +0x38
    int _baseValue; int _stackCount;                  // +0x40, +0x44
}

RawMaterialDescription rawmaterial {
    SpriteSheet _spriteSheet; StringTable _stringTable;  // +0x20, +0x28  (NOTE: different order!)
    String _name; String _nameSmall;                     // +0x30, +0x38
    int _lowCreateCount; int _highCreateCount; int _weight; int _createChance;
    float _carryScale;
    RawMaterialFlags _flags;
}
```

Note the field order difference at +0x28/+0x30:
- **ValueDescription**: +0x28 = _spriteName (String), +0x30 = _stringTable (ptr)
- **RawMaterialDescription**: +0x28 = _stringTable (ptr), +0x30 = _name (String)

Both have SpriteSheet at +0x20 and a String at +0x38, but the types at +0x28 and +0x30
are swapped.
