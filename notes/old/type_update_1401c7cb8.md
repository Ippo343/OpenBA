# Type Update Needed: DAT_1401c7cb8 = RawMaterialDescription

## Problem

The type ID at `0x1401c7cb8` is mislabeled throughout the codebase. It is currently
called `g_TypeId_ResourceType` in one place and `g_TypeId_CitizenComponent` in another,
but the registration function at `0x14014f790` proves it is `RawMaterialDescription`.

## Changes Required

### 1. engine/funcs/engine_funcs.h

**Rename global variable** (line 583):
```cpp
// BEFORE:
extern uint32_t* g_TypeId_ResourceType;           // DAT_1401c7cb8

// AFTER:
extern uint32_t* g_TypeId_RawMaterialDescription; // DAT_1401c7cb8
```

**Remove duplicate** (line ~2714-2719):
```cpp
// DELETE these lines — they create a wrong alias:
// CitizenComponent type ID — DAT_1401c7cb8 (used by KillAllCitizens)
// NOTE: same as g_TypeId_ResourceType; this is the CitizenComponent (not ResourceType)
// Ghidra uses DAT_1401c7cb8 in KillAllCitizens to find citizen components on entities
// fn_FindComponent_Citizen — searches entity for CitizenComponent (DAT_1401c7cb8)
```

Also remove `g_TypeId_CitizenComponent` and `fn_FindComponent_Citizen` declarations
if they reference this same address.

**Update all inline helpers** that use `g_TypeId_ResourceType`:
- `FindComponent_ResourceType` helper functions (search for usage patterns)
- Rename to `FindComponent_RawMaterialDescription` or similar

### 2. engine/funcs/engine_funcs.cpp

**Rename variable definition** (line 243):
```cpp
// BEFORE:
uint32_t* g_TypeId_ResourceType = nullptr;

// AFTER:
uint32_t* g_TypeId_RawMaterialDescription = nullptr;
```

**Rename first init** (line 1731):
```cpp
// BEFORE:
g_TypeId_ResourceType = reinterpret_cast<uint32_t*>(
    ResolveAddr(Module::Exe, 0x1401c7cb8));

// AFTER:
g_TypeId_RawMaterialDescription = reinterpret_cast<uint32_t*>(
    ResolveAddr(Module::Exe, 0x1401c7cb8));
```

**Remove duplicate init** (line 3031-3032):
```cpp
// DELETE:
g_TypeId_CitizenComponent = reinterpret_cast<uint32_t*>(
    ResolveAddr(Module::Exe, 0x1401c7cb8));
```

### 3. code/Game/Dialog/buildui.cpp

Update all comments and variable names that say "ResourceType" to say
"RawMaterialDescription":
- Line 253: `g_TypeId_ResourceType` -> `g_TypeId_RawMaterialDescription`
- Line 254: same
- Comments like "Find ResourceType in entry's component list" -> "Find RawMaterialDescription"

### 4. code/Game/Dialog/buildrequirementui.cpp

- Line 266: `g_TypeId_ResourceType` -> `g_TypeId_RawMaterialDescription`
- Comment at line 265: "Find ResourceType component" -> "Find RawMaterialDescription"

### 5. code/Game/Dialog/debugui.cpp

- Line 343: Comment says "CitizenComponent (DAT_1401c7cb8)" -> should say
  "RawMaterialDescription (DAT_1401c7cb8)"

### 6. engine/reimpl/rawmaterialcomponent.cpp

- Line 64-65: Change comment and variable name:
  ```cpp
  // BEFORE:
  // DAT_1401c7cb8 — type ID for building/structure data
  static uint32_t* g_pBuildingDataTypeId = nullptr;

  // AFTER:
  // DAT_1401c7cb8 — type ID for RawMaterialDescription
  static uint32_t* g_pRawMaterialDescTypeId = nullptr;
  ```
- Line 286: update reference accordingly

### 7. Ghidra Rename

Rename the function at `0x140060010` (`Game_KillAllCitizens`) to
`Game_DestroyAllRawMaterials` — it destroys entities with RawMaterialDescription,
not citizens.

## New Type Header

A new file `engine/types/RawMaterialDescription.h` should be created with the struct
definition. See the analysis document for the full field layout (0x60 bytes).
