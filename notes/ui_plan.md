# UI Deep Reimplementation Plan

## Current State

- **67 UI classes** (39 panels, 22 dialogs, plus abstract bases)
- **11 panels touched** in Batches 1-3: 10 dtors reimplemented, 1 Serialize (TitleDescToolTipUI)
- **All game logic methods still passthrough** — blocked by exe-internal dependencies
- 1 hook disabled (CropUI::Slot9 — relative branch in prologue)

## The 4 Walls

Every UI game logic method hits the same dependency categories. Until these are resolved as infrastructure in `engine/funcs/`, game logic can't move to `engine/src/ui/`.

### Wall 1: Runtime DLL UI Widget Functions (HIGH priority)

Every panel calls Runtime-x64.dll widget methods. We only have 6 resolved; we need ~20 more.

**Already resolved:**
```
fn_FindElement              — find element by name
fn_TextElement_SetParameter — set text param
fn_Element_GetUserData      — get user data ptr
fn_DialogController_Dtor    — base destructor
fn_InternalBasePtr_Ctor     — ptr wrapper ctor
fn_IsDerivedFrom            — type check
```

**Needed (used across most panels):**
```
UI::Element::SetFlagsOnOff(this, uint hideFlags, uint showFlags)
UI::Element::SetElementId(this, int id)
UI::Element::SetVisible(this, bool)
UI::Element::SetEnabled(this, bool)
UI::Element::SetName(this, String const&)
UI::Element::SetToolTip(this, void* sprite)
UI::Element::Child(this, int index) -> InternalPtr<Element>
UI::Element::ChildCount(this) -> int

UI::DialogInstance::FindElement(this, String const&) -> InternalPtr<Element>
UI::DialogInstance::GetController(this, retval) -> InternalPtr<DialogController>

UI::ComboElement::AddItem(this, Sprite*, String*, void*)
UI::ComboElement::SetCurrentItem(this, int index)
UI::ComboElement::GetItemCount(this) -> int

UI::ProgressElement::SetProgressTextParameter(this, int, String const&)

UI::TableElement::SetSize(this, int rows, int cols)
UI::TableElement::SetChild(this, elem, int col, int row)

UI::NumberEditElement::SetValue(this, int)
UI::NumberEditElement::SetMinMax(this, int min, int max)
UI::NumberElement::SetValue(this, int)

UI::SpinnerElement::SetElementId(this, int)
UI::ImageElement::SetSprite(this, Sprite*)
UI::LabelElement::SetSprite(this, Sprite*)
UI::TextElement::SetText(this, void* sprite)
```

**How to resolve:** All are MSVC-mangled exports in Runtime-x64.dll. Use Ghidra `search_functions` to find exact mangled names, add to `engine_funcs.h/.cpp` with `ResolveExport()`. Batch process: search Ghidra for each class name, resolve all exports for that class at once.

**Estimated work:** ~25 function pointers. 1 session to look up mangled names + add to engine_funcs.

### Wall 2: Exe Type IDs (MEDIUM priority)

`IsDerivedFrom` checks need type IDs stored in exe .rdata. These appear as `DAT_1401cXXXX` or `TypeInfo_XXX` in Ghidra.

**Common type IDs needed:**
```
TypeInfo_ComponentEntity     — already named in Ghidra (used everywhere)
DAT_1401c7318               — BuildComponent type ID (BuildRequirementUI)
DAT_1401c7388               — ConsumeProduceComponent type ID
DAT_1401c4870               — RadiusComponent type ID
DAT_1401c7f58               — BuildRequirementComponent type ID
DAT_1401c7040               — ResourceComponent type ID
DAT_1401c7cb8               — ResourceType (sprite lookup)
DAT_1401c56e0               — DialogController type ID
DAT_1401c4f00               — PageToggleUI type ID
DAT_1401c7e40               — MeshInstanceComponent type ID
DAT_1401c7628               — DestroyComponent internal type
DAT_1401c80e0               — WorkplaceComponent type ID
DAT_1401c8070               — StorageLocationComponent type ID
```

**How to resolve:** Each is a `uint32_t*` pointer into exe .rdata. Resolve with `ResolveAddr(Module::Exe, addr)`. Add as globals in `engine_funcs.h`:
```cpp
extern uint32_t* g_TypeId_ComponentEntity;     // DAT at 0x1401cXXXX
extern uint32_t* g_TypeId_BuildComponent;      // etc.
```

**Estimated work:** ~15 pointer declarations. Quick — just need addresses from Ghidra.

### Wall 3: Exe Game Singletons (MEDIUM priority)

Several exe globals are game state singletons accessed by UI panels.

**Most common:**
```
DAT_1401c3dd8  — Game/World singleton (used by ~20 UI methods)
                 +0xB0  → ProfessionList (profession count, names)
                 +0xF0  → CitizenList/PopulationData
                 +0xF8  → WorldGrid (map dimensions)
                 +0x100 → ResourceTypeMap (BST of resource types)
                 +0x110 → RadiusDisplayManager

DAT_1401c3de8  — Input::KeyMap singleton (HotKeyToolTipUI)
```

**How to resolve:** `ResolveAddr(Module::Exe, 0x1401c3dd8)` to get runtime pointer. Then field offsets are fixed.

**Estimated work:** 2-3 singleton pointers. The sub-field semantics need Ghidra analysis.

### Wall 4: Exe-Internal Functions (HIGH priority, ongoing)

Functions inside Application-x64.exe that aren't DLL exports. Can't be resolved by name — must be resolved by RVA or reimplemented.

**Common patterns:**
```
Entity_FindComponent_StateMachine(entity, &result)   — 0x14000b6f0?
Entity_FindComponent_MeshInstance(entity, &result)    — varies
FUN_140004680(entity, &result)                        — FindComponent variant
FUN_140005780(entity, &result)                        — FindComponent (DestroyComponent?)
FUN_1400032e0(arrayBase, newCount)                    — Array resize
FUN_140003600(arrayBase, newCount, element)            — Array push
FUN_1400021d0(arrayBase, newCount)                    — Array resize variant
StorageComponent_QueryCount(storageComp)              — 0x14012cf10
FUN_140055430(component, tileIndex)                   — Radius calculation
FUN_1400b0d00(radiusMgr, index)                       — Remove radius overlay
FUN_140078830(cropUI)                                 — CropUI helper (populate combo)
FUN_140078a40(cropUI)                                 — CropUI helper (get expected count)
FUN_14011e690(component) -> bool                      — DestroyComponent query
FUN_14011e600(component) -> bool                      — DestroyComponent query
FUN_14011e400(component)                              — Reclaim action
FUN_14011e470(component)                              — Rebuild action
FUN_14011e4d0(component)                              — Destroy action
FUN_140119470(component) -> int                       — Get active product index
FUN_14012e5c0(component) -> int                       — Storage count query
```

**How to resolve:** Two paths:
1. **Resolve by RVA** — `ResolveAddr(Module::Exe, addr)` and call as function pointer. Works but ties us to this exe build.
2. **Reimplement** — Write clean C++ that replaces the exe function. Preferred for standalone exe goal, but requires deeper decompilation.

**Strategy:** Resolve the most-used ones (FindComponent variants, array ops) as function pointers first. Reimplement later when tackling the component module.

---

## Unblocking Strategy

### Phase A: Infrastructure Build (prerequisite for all game logic)

1. **Resolve Runtime UI widget functions** (~25 exports)
   - Search Ghidra for each UI element class
   - Get mangled export names
   - Add to `engine_funcs.h/.cpp`
   - One session, mechanical work

2. **Resolve exe type IDs** (~15 pointers)
   - Read addresses from Ghidra for each `DAT_1401cXXXX`
   - Add as `uint32_t*` globals
   - Quick, batch process

3. **Resolve game singletons** (2-3 pointers)
   - `DAT_1401c3dd8` (World/Game state)
   - Document sub-field layout from Ghidra

4. **Resolve common exe functions** (~10 function pointers)
   - Entity FindComponent variants
   - Array resize/push helpers
   - StorageComponent_QueryCount

### Phase B: Simple Panel Game Logic (after infrastructure)

With the infrastructure resolved, these panels become straightforward data-binding reimpl:

| Panel | Methods to Reimpl | Key Dependencies |
|-------|-------------------|------------------|
| DestroyUI::Slot4 | FindElement x8, SetElementId x3, SetFlagsOnOff | All in Phase A |
| StorageCapacityUI::Serialize | FindElementType, InternalBasePtr | Phase A + 1 type ID |
| StorageCapacityUI::Startup | FindComponent, QueryCount, SetProgress | Phase A |
| ConsumeProduceUI::Slot7 | Progress bar update | Phase A + float constants |
| ConsumeProduceUI::Slot9 | Combo selection → component write | Phase A |
| StatusIconUI::Serialize | FindElement, ChildCount, Child | Phase A |
| RadiusUI::Slot6 | Radius display remove | Phase A + singleton |

### Phase C: Complex Panel Game Logic

After Phase B proves the infrastructure works:

| Panel | Complexity | Key Challenge |
|-------|-----------|---------------|
| ConsumeProduceUI::Slot4 | High | ComboElement population with sprites |
| ProfessionUI::Slot5 | Very High | TableElement, tree structure, dynamic arrays |
| DestroyUI::Slot7 | High | Complex visibility state machine |
| BuildRequirementUI::Slot4 | Very High | Dynamic label arrays, sprite setup |
| CropUI::Slot4 | Medium | ComboElement population |
| StatusIconUI::Startup | High | MeshInstance component, bitfield visibility |

### Phase D: Remaining Panels (mechanical sweep)

All remaining panels follow the same patterns. Once Phases A-C prove the infrastructure, these are mostly copy-paste with different element names and component types.

32 panels remaining, most with 1-3 game logic methods each.

### Phase E: Dialogs

22 dialog classes. Similar pattern but based on DialogInstance (0xB8 base) instead of DialogController (0x30 base). Dialogs tend to be more self-contained (menu screens, load/save, options).

---

## Dtor Sweep (parallel track, no blockers)

All remaining panel/dialog dtors can be reimplemented NOW without any infrastructure. The pattern is mechanical:

```cpp
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    // 1. Set vtable
    ((DialogController*)self)->vtable = resolved_vtable;
    // 2. Free any dynamic arrays (check Ghidra decompilation)
    // 3. Call base dtor
    fn_DialogController_Dtor(self);
    // 4. Conditional free
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}
```

**Exceptions:** ProfessionUI dtor has recursive tree cleanup — needs its own helper or stays passthrough.

**Scope:** ~50 more dtors across all panels + dialogs. Can batch-process in one session.

---

## File Organization

```
engine/funcs/
  engine_funcs.h          — Add: UI widget funcs, type IDs, singletons, exe funcs
  engine_funcs.cpp         — Add: Resolution calls for all new pointers

engine/src/ui/
  ui_panels.h              — Function declarations for all reimpl'd panel methods
  ui_common.h              — Shared UI helpers (element lookup wrappers, etc.)
  destroyui.cpp            — DestroyUI game logic (Phase B)
  storagecapacityui.cpp    — StorageCapacityUI game logic (Phase B)
  consumeproduceui.cpp     — ConsumeProduceUI game logic (Phase B-C)
  statusiconui.cpp         — StatusIconUI game logic (Phase B)
  professionui.cpp         — ProfessionUI game logic (Phase C)
  buildrequirementui.cpp   — BuildRequirementUI game logic (Phase C)
  ...                      — One file per panel as methods get reimplemented
```

## Success Metrics

- **Phase A complete:** 25+ Runtime exports resolved, 15+ type IDs resolved, 3+ singletons resolved
- **Phase B complete:** 7+ game logic methods moved to `engine/src/ui/`, visually verified in-game
- **Phase C complete:** Complex panels (Profession, Destroy, BuildRequirement) have game logic in src
- **Phase D complete:** All 39 panel controllers have all reimplementable methods in src
- **Phase E complete:** All 22 dialog controllers covered
- **End state:** `engine/src/ui/` compiles standalone (no hook/trampoline deps), all UI logic is clean C++
