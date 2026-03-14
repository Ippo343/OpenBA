# Runtime DLL Decompilation Goals

Tracking Runtime-x64.dll functions that need decompilation for current work.

## Infrastructure Status

- `runtime/types/` — binary-compatible struct headers (common.h, video/ImageBuffer.h, ui/Sprite.h, ui/SpriteSheet.h, ui/ImageElement.h, ui/LabelElement.h)
- `runtime/funcs/runtime_funcs.{h,cpp}` — internal function pointer resolution (placeholder, calls work)
- `runtime/src/ui/` — debug logging stubs + headers (sprite_debug.{h,cpp}, spritesheet_debug.{h,cpp}, element_sprite_debug.{h,cpp})
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

### COMPLETED: Renames (2026-03-13)

All wrong names for DAT_1401c7cb8 fixed in commit `cd8b5a5`:
- `g_TypeId_ResourceType` → `g_TypeId_RawMaterialDescription` (12 files, including consumeproduceui, tradeui, townhallui, storageui, productionui)
- `g_TypeId_CitizenComponent` duplicate removed, replaced with `fn_FindComponent_RawMaterial` helper
- `g_pBuildingDataTypeId` local static removed from rawmaterialcomponent.cpp (uses global instead)
- `Game_KillAllCitizens` → `Game_DestroyAllRawMaterials` (engine_funcs + debugui)

### COMPLETED: fn_LabelElement_GetSprite (2026-03-13)

Added to engine_funcs — ordinal 2927, address `0x1800429d0`, resolves via mangled name with address fallback. `element_sprite_debug.cpp` updated to use it.

### COMPLETED: Debug wrapper headers (2026-03-13)

Created `.h` files for all debug wrappers so they can be included from other translation units:
- `runtime/src/ui/sprite_debug.h`
- `runtime/src/ui/element_sprite_debug.h`
- `runtime/src/ui/spritesheet_debug.h`

### COMPLETED: Debug integration into BuildRequirementUI + BuildUI (2026-03-13)

8 sprite call sites wrapped with `#ifdef _DEBUG` / Debug_ wrappers in:
- `code/Game/Dialog/buildrequirementui.cpp` (4 sites: 2x Sprite_Ctor, 1x ImageElement_SetSprite, 1x LabelElement_SetSprite)
- `code/Game/Dialog/buildui.cpp` (4 sites: 1x Sprite_Ctor, 1x Sprite_DefaultCtor, 2x LabelElement_SetSprite)

Debug builds now log all sprite pipeline operations. Deploy and check `patch_debug.log`.

---

## Next Priorities

### 1. Deploy and diagnose sprite display issue

Build with `mingw32-make deploy`, launch game, start a new game, place a building.
Check `patch_debug.log` for:
- `[Sprite::Ctor]` lines showing sheet ptr and resolved index
- `[SpriteSheet] LOOKUP FAILED` lines indicating wrong sprite names
- `[ImageElement::SetSprite]` / `[LabelElement::SetSprite]` showing dirty flag changes

If sprites still don't display, use `SpriteSheet_Debug_DumpAllSprites` to dump the full
sheet contents and compare against the names being looked up.

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
