# Runtime DLL Decompilation Goals

Tracking Runtime-x64.dll functions that need decompilation for current work.

## Infrastructure Status

- `runtime/types/` — binary-compatible struct headers (common.h, video/ImageBuffer.h, ui/Sprite.h, ui/SpriteSheet.h, ui/ImageElement.h, ui/LabelElement.h)
- `runtime/funcs/runtime_funcs.{h,cpp}` — internal function pointer resolution (placeholder, calls work)
- `runtime/src/ui/` — debug logging stubs (sprite_debug.cpp, spritesheet_debug.cpp, element_sprite_debug.cpp)
- `runtime/reimpl/` — hook wrappers (empty)
- **Makefile** — build rules for all runtime dirs already in place, include paths set (`-Iruntime/types -Iruntime/funcs`)

No additional toolchain or build setup needed. Files placed in `runtime/src/ui/` will compile automatically.

---

## COMPLETED: UI::Sprite Pipeline (2026-03-13)

All decompiled, type headers created, debug wrappers written. Build passes.

### Sprite (0x10 bytes) — `runtime/types/ui/Sprite.h`

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| +0x00 | void* | spriteSheet | ExternalPtr\<SpriteSheet\>, null = no sheet |
| +0x08 | int32 | spriteIndex | Index into sheet, -1 = invalid |
| +0x0C | int32 | pad0C | Padding |

- Main ctor (`0x18011b9c0`): stores sheet, calls GetSpriteIndexFromName to resolve name → index. Returns -1 if sheet is null.
- Default ctor (`0x18011ba10`): sheet=null, index=-1.
- By-index ctor (`0x18011ba30`): direct store, no name lookup.
- IsValid (`0x180030c40`): `index >= 0 AND sheet != nullptr`.

### SpriteSheet (0x58 bytes) — `runtime/types/ui/SpriteSheet.h`

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| +0x00 | void* | vtable | |
| +0x08 | char* | m_filename | Sheet resource path |
| +0x20 | SpriteInfo* | m_sprites | Dynamic array, 0x38 bytes per entry |
| +0x28 | int32 | m_spriteCount | |
| +0x2C | int32 | m_spriteCapacity | |
| +0x38 | char* | m_textureName | System::String |
| +0x40 | char* | m_groupName | System::String |

- **GetSpriteIndexFromName (`0x1800031f0`)**: LINEAR SCAN, case-sensitive, compares string length first then char-by-char. Returns -1 on not found.
- SpriteInfo entries are 0x38 bytes: name (+0x00), group (+0x08), x/y/w/h (+0x10..+0x1C), UV coords (+0x20..+0x2C).

### ImageElement (0xE0 bytes) — `runtime/types/ui/ImageElement.h`

- Inherits: Element (0xC0) → ImageElement
- Sprite at **+0xC0**
- SetSprite (`0x180118600`): compares old/new, copies 0x10 bytes, sets dirty flag `0x8000000` at +0x34, walks parent chain via +0x98.
- GetSprite (`0x1801182a0`): returns via hidden return pointer (member function ABI: RCX=this, RDX=retval).

### LabelElement (0x130 bytes) — `runtime/types/ui/LabelElement.h`

- Inherits: Element (0xC0) → TextElement (0x100) → LabelElement
- Sprite at **+0x100** (NOT +0xC0 — TextElement adds 0x40 bytes of text/font/color fields)
- SetSprite (`0x180042980`): same dirty-flag pattern as ImageElement.
- GetSprite (`0x1800429d0`): same hidden return pointer ABI.

### Debug Logging — `runtime/src/ui/`

- `sprite_debug.cpp` — wraps Sprite ctors and IsValid, logs sheet/index/validity
- `spritesheet_debug.cpp` — wraps GetSpriteIndexFromName (highlights failed lookups), includes `DumpAllSprites` for full sheet contents
- `element_sprite_debug.cpp` — wraps SetSprite/GetSprite for both element types, logs dirty flag changes

---

## COMPLETED: Component Layout at DAT_1401c7cb8 (2026-03-13)

**RESOLVED: DAT_1401c7cb8 = `RawMaterialDescription`** (not ResourceType, not CitizenComponent).

Type header: `engine/types/RawMaterialDescription.h` (0x60 bytes).
Full analysis: `notes/component_1401c7cb8_analysis.md`.

Key fields for sprite pipeline:
- +0x20: `_spriteSheet` (SpriteSheet*) — arg1 to fn_Sprite_Ctor
- +0x30: `_name` (String, e.g. "Log") — BuildUI sprite name
- +0x38: `_nameSmall` (String, e.g. "LogSmall") — BuildRequirementUI sprite name

### Rename TODO (see `notes/type_update_1401c7cb8.md`)

These wrong names need updating across the codebase:
- `g_TypeId_ResourceType` → `g_TypeId_RawMaterialDescription` (engine_funcs.h:583)
- `g_TypeId_CitizenComponent` → remove duplicate (engine_funcs.h:2714)
- `g_pBuildingDataTypeId` → `g_TypeId_RawMaterialDescription` (rawmaterialcomponent.cpp:65)
- Comments in buildui.cpp, buildrequirementui.cpp, debugui.cpp
- `Game_KillAllCitizens` → `Game_DestroyAllRawMaterials` (misnamed — destroys loose items, not citizens)

### Missing function pointer

- `fn_LabelElement_GetSprite` — ordinal 2927, address `0x1800429d0`, mangled: `?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ`

---

## Next Priorities

### 1. Integrate debug logging into BuildRequirementUI reimpl

Wire up the sprite debug wrappers in `code/Game/Dialog/buildrequirementui.cpp` to trace
the actual sprite pipeline at runtime. Replace direct `fn_Sprite_Ctor` calls with
`Debug_Sprite_Ctor` and `fn_ImageElement_SetSprite` with `Debug_ImageElement_SetSprite`
to see exactly what sprite data flows through.

### 2. Additional Runtime types (as needed)

| Type | Namespace | Priority | Notes |
|------|-----------|----------|-------|
| `Element` | `UI` | Medium | Full 0xC0-byte base class layout (dirty flags, parent chain, bounds) |
| `TextElement` | `UI` | Medium | 0x40 bytes of text/font/color fields between Element and LabelElement |
| `DialogInstance` | `UI` | Low | Container for element trees, needed for deeper UI reimpl |
| `String` | `System` | Low | Already partially understood (8-byte COW refcounted), full layout in SystemString.h |

### 3. Remaining runtime/src/ namespaces (future)

16 namespace directories exist but are empty. Fill as needed for exe reimpl work:
achievement, application, audio, collision, engine, graphics, io, map, math, network,
object, pathfinding, system, ui (started), video, volume.
