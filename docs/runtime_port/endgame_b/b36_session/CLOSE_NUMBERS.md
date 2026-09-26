# B.3.6 CLOSE NUMBERS — the step's measurements, for the STATUS block

Measured by `b36-close` on its own branch at the end of the closing pass: the
tree that carries every class block (A, B, D, C1–C5, E), both fragment passes
(F1, F2), every caller batch (K1–K4, rounds 1 and 2) and this pass's commits
1–5. "Start of step" is `dc647869` (the launch of B.3 step 6), measured the same
way in a throwaway `git worktree` at that commit, then removed. The scripts are
in `C:/Users/USPC/AppData/Local/Temp/claude/b36-close/` (`numbers.py`,
`sweep.py`, `sites.py`, `classes.py`, `crosscheck.py`, `doors.py`). Every number
says what it counts, because several are only comparable to the start-of-step
figures when the unit is the same.

---

## 1. `reinterpret_cast` — LINE counts per directory (the B.4 metric)

`grep -c` semantics (LINES containing the token, summed) over `*.cpp *.h *.inc`,
`build*/` and `obj/` excluded. A conversion that bridges a `void*` or POD
receiver onto a real class MUST spell it `reinterpret_cast`, so this RISES during
B.3 and falls only in B.4 when the receivers become typed.

| directory | start of step (`dc647869`) | now | delta |
|---|---:|---:|---:|
| `code` | 23,760 | **25,282** | +1,522 |
| `runtime` | 7,009 | **6,885** | −124 |
| `standalone` | 2,889 | **2,907** | +18 |
| `engine` | 756 | **609** | −147 |
| `dx11` | 168 | **168** | 0 |
| `tests` | 243 | **374** | +131 |
| **TOTAL** | **34,825** | **36,225** | **+1,400** |

`code` rose with the caller batches' bridges (1,492 of them, §2). `engine` FELL
147: this pass deleted 126 `engine_funcs` globals, each with a
`reinterpret_cast<fn_X_t>(…)` resolver line. `runtime` fell because the class
blocks replaced raw-offset casts inside `runtime/src/ui` with member access.
`tests` rose with the eleven new UI ABI test files.

## 2. `// B.3.6 bridge` — LINE counts per directory

| directory | lines |
|---|---:|
| `code` | 1,492 |
| `runtime` | 559 |
| `standalone` | 20 |
| `engine` | 5 |
| `dx11` | 0 |
| `tests` | 7 |
| **TOTAL** | **2,083** |

`engine`'s 5 are this pass's (the three `engine_funcs.h` inline bodies:
`SetEngineNextLoop` ×2, `SetDialogLoaderNextLoop`, `FindDialogByName_Impl`,
`Toolbar_CenterOnEntity_Impl`). Start of step: 0 (the marker did not exist).

## 3. The caller surface

### 3a. `engine_funcs` globals bound to a `@UI@@` symbol

Found BY INITIALISER (`sweep.py`: a positional statement walk over
`engine_funcs.cpp`; `ResolveExport` literals joined across wraps,
`ResolveAddr(Module::Exe, slot)` mapped through the PE import table,
`ResolveAddr(Module::Runtime, a)` through `rt_export_addrs.inc`, and the
two-statement `void** d = ResolveAddr(..); if (d) fn_X = *d;` form).

| | start of step | now |
|---|---:|---:|
| globals bound to a `@UI@@` symbol | **155** | **29** |
| — of which door (function) pointers | 132 | **7** |
| — of which `?_typeInfo@…@UI@@` data globals | 23 | 22 |

The brief's "112" and the inventory's "118" are narrower spellings of the same
population (name-anchored, or `ResolveExport`-only); the 132 includes the
exe-slot bindings and duplicate bindings of one export (three default-`Sprite`
ctor pointers, `fn_Sprite_Sprite`, the two `EditElement::SetText` pointers).
The 7 left: `fn_DialogInstance_Ctor` (29 calls), `fn_DialogInstance_Dtor` (20),
`fn_SpriteSheet_Ctor` (4), `fn_ElementDescription_Ctor`/`_Dtor` (2 each),
`fn_Element_Ctor` (2) — ctor/dtor doors, B36_RULES 7 — and
`fn_SaveLoad_SetSaveSprite` (2), a `Save::SaveLoad` door whose mangled name
contains `@UI@@` only in a parameter type. **Zero UI-bound globals at zero
references.**

### 3b. Door CALL sites through those globals in `code/Game`

Unit: the inventory's grep, raw `\bNAME(` occurrences over the
initialiser-derived name list (comments included), in `code/Game`.

| | start of step | now |
|---|---:|---:|
| raw `NAME(` occurrences | **1,472** over 118 pointers | **65** over 7 pointers |
| code-only CALLs (comments/strings stripped) | 1,464 | **61** |

The inventory's 1,434 is the same grep over a partly name-selected list; 1,472
is this pass's re-measurement with the initialiser-derived list. All 61 calls
left are the ctor/dtor doors and the Save door above.

### 3c. `ResolveExport` / `RT_EXPORT_ONCE` / exe-slot sites naming a `@UI@@` symbol

Unit: CALL SITES (one per call; literals joined across line wraps; comments
stripped). `sites.py`.

| binding | directory | start of step | now |
|---|---|---:|---:|
| `ResolveExport("…@UI@@…")` | `code/Game` | 18 | **3** |
| | `engine` | 113 | **3** |
| | `standalone` | 25 | **22** |
| | `runtime/src` | 29 | **13** |
| | **total** | **185** | **41** |
| `RT_EXPORT_ONCE(T, "…@UI@@…")` (literal-resolved) | `runtime/src` | 79 | **11** |
| `RT_EXPORT_ONCE(` raw occurrences (incl. stringized macro names) | `runtime/src/ui` | 174 | **57** |
| `RT_DATA_SLOT(T, "…@UI@@…")` (static-storage reads) | `runtime/src` | 147 | **70** |
| `ResolveAddr(Module::Exe, 0x14015…)` → UI import (`dispatch_slot_map.py`) | `code/Game` | 8 | **3** |
| | `engine` | 51 | **26** |
| | **total** | **59** | **29** |
| `RtAddr<…>(0x18…)` / `ResolveAddr(Module::Runtime, 0x18…)` → UI export | `runtime/src` | 235 | **146** |
| | `engine` + `code/Game` | 7 | **3** |

What the survivors are (commit 2's message quotes each): `standalone`'s 22 are
`RegisterDispatchSlot(slot, ResolveExport(…))` rows and `?_typeInfo` reads —
never retired; `engine`'s 3 are the kept ctor/dtor doors; `code/Game`'s 3 are the
two `UserInterface` ctor/dtor doors (`gameupdateloop_load.cpp`, `gameupdateloop.cpp`)
and one `?_typeInfo@ImageElement` read (`tool_base_createbar.cpp`); `runtime/src`'s `ResolveExport` 13 are `??_7`/ctor/dtor/`_typeInfo`. Of the
146 by-address `runtime/src` sites, **69 are ctor/dtor/copy-ctor doors and 76 are
live calls to NON-ctor UI members through the file-local `RtAddr<Fn>(0x18…)`
helper** (`SpriteContext::AddSprite` ×27, `Dialog::CreateElement` ×8,
`UserInterface::Play` ×7, …) — a spelling the step-6 inventory did not grep.
They are the step's largest carry-over (B36_RULES 12).

### 3d. Hand-inlined `(*(void***)p)[N]` in `code/Game/Dialog`

| unit | start of step | now |
|---|---:|---:|
| exact token `(*(void***)` | 14 | **9** |
| broad token `(*(void**…)` / `(*(uintptr_t**…)` (any double-pointer deref cast) | 425 in 39 files | **431** in 39 files |

The inventory's 417 is not reproducible byte-for-byte on `dc647869`; the broad
token is the closest grep (425, with `townhallui.cpp` at exactly the
inventory's 95). That token counts every double-pointer DEREF, not just vtable
calls, which is why it does not fall: the caller batches replaced vtable-slot
calls with member calls but left (and in bridges added) raw field reads. The
exact vtable-call token fell 14 → 9; the 9 left are diagnostic slot READS
(`workplaceui.cpp:472-475`) and exe-side table fills, not calls.

## 4. `tools/runtime_census.py --namespace UI`

Verbatim rows:

```
start (dc647869):  UI  1625  329  214  0  65.0%  99.4%     population 1288 rows
now:               UI  1625  134   78  0  58.2%  98.5%     population 1092 rows
                   namespace total called ported detour called-covered ceiling
```

`called` counts distinct functions the tree RESOLVES (by name or address). A
member call does not resolve anything, so converting callers REMOVES rows from
`called` — 195 UI rows left the board because nothing resolves them any more,
not because anything was unported. The census measures the door surface; it
is not a coverage measure for a namespace whose callers became C++.

## 5. Unit tests

`UNIT_TEST(` registrations, and the runner's own count:

| | start of step | now |
|---|---:|---:|
| tests | **358** | **462** (0 failures) |
| test files in `tests/unit` | 33 | 44 |

## 6. The class list — from the headers, cross-checked against retail

`classes.py` parses every `runtime/types/ui/*.h` at `namespace UI` scope; parent
and `sizeof` are the header's (`class X : public Y`, `static_assert(sizeof(X)
== …)`); slot count is the retail vtable length from `B36_SLOTS.md` §1 (the
headers annotate only the slots each class itself declares, so a header count
is not the vtable length). `crosscheck.py`: **68 of 68 polymorphic classes agree
with `B36_SLOTS.md` on parent AND sizeof.**

| class | parent | slots | sizeof |
|---|---|---:|---:|
| `AnimatedImageDescription` | ImageDescription | 10 | 0x98 |
| `AnimatedImageElement` | ImageElement | 12 | 0x100 |
| `BorderDescription` | ElementDescription | 10 | 0xb8 |
| `BorderElement` | Element | 12 | 0x150 |
| `ButtonDescription` | ElementDescription | 10 | 0xa0 |
| `ButtonElement` | Element | 12 | 0xd8 |
| `CharacterSet` | Object::External | 9 | 0x28 |
| `CheckDescription` | ButtonDescription | 10 | 0xa8 |
| `CheckElement` | ButtonElement | 12 | 0xe0 |
| `ClipRegionDescription` | ElementDescription | 10 | 0x78 |
| `ClipRegionElement` | Element | 12 | 0xd8 |
| `ComboDescription` | ButtonDescription | 10 | 0xb8 |
| `ComboDialog` | DialogInstance | 10 | 0xd0 |
| `ComboElement` | ButtonElement | 12 | 0x128 |
| `ContainerDescription` | ElementDescription | 10 | 0x90 |
| `ContainerElement` | Element | 12 | 0xd8 |
| `Cursor` | Object::External | 9 | 0x30 |
| `Dialog` | Object::External | 9 | 0x88 |
| `DialogController` | Object::Internal | 11 | 0x30 |
| `DialogControllerConfig` | Object::External | 8 | 0x20 |
| `DialogInstance` | Object::Internal | 10 | 0xb8 |
| `DocumentTextDescription` | ElementDescription | 10 | 0xe8 |
| `DocumentTextElement` | Element | 12 | 0x1c8 |
| `EditDescription` | ElementDescription | 10 | 0x88 |
| `EditElement` | Element | 13 | 0xf8 |
| `Element` | Object::Internal | 12 | 0xc0 |
| `ElementDescription` | Object::External | 9 | 0x68 |
| `Font` | Object::External | 9 | 0x90 |
| `GraphDescription` | ElementDescription | 10 | 0x88 |
| `GraphElement` | Element | 12 | 0x100 |
| `GroupDescription` | ElementDescription | 10 | 0x78 |
| `GroupElement` | Element | 12 | 0xc0 |
| `ImageDescription` | ElementDescription | 10 | 0x80 |
| `ImageElement` | Element | 12 | 0xe0 |
| `LabelDescription` | TextDescription | 10 | 0xb8 |
| `LabelElement` | TextElement | 12 | 0x130 |
| `LayoutDescription` | ElementDescription | 10 | 0x78 |
| `LayoutElement` | Element | 12 | 0xd0 |
| `ListDescription` | ElementDescription | 10 | 0x90 |
| `ListElement` | Element | 12 | 0x100 |
| `NumberDescription` | ElementDescription | 10 | 0x88 |
| `NumberEditDescription` | ElementDescription | 10 | 0x88 |
| `NumberEditElement` | EditElement | 13 | 0x100 |
| `NumberElement` | Element | 12 | 0xe8 |
| `PageDescription` | GroupDescription | 10 | 0x80 |
| `PageElement` | GroupElement | 12 | 0xc8 |
| `ProgressDescription` | ElementDescription | 10 | 0xb0 |
| `ProgressElement` | Element | 12 | 0x130 |
| `RadioDescription` | GroupDescription | 10 | 0x80 |
| `RadioElement` | GroupElement | 12 | 0xd8 |
| `RibbonDescription` | ElementDescription | 10 | 0x80 |
| `RibbonElement` | Element | 12 | 0xd8 |
| `ScrollDescription` | ElementDescription | 10 | 0xc8 |
| `ScrollElement` | Element | 12 | 0x178 |
| `SliderDescription` | ElementDescription | 10 | 0xb8 |
| `SliderElement` | Element | 12 | 0x130 |
| `SpinnerDescription` | ElementDescription | 10 | 0x88 |
| `SpinnerElement` | Element | 12 | 0xc8 |
| `SpriteCapture` | Object::External | 9 | 0x58 |
| `SpriteSheet` | Object::External | 9 | 0x58 |
| `StringTable` | Object::External | 9 | 0x30 |
| `TabDescription` | ElementDescription | 10 | 0xb8 |
| `TabElement` | Element | 12 | 0x108 |
| `TableDescription` | ElementDescription | 10 | 0x98 |
| `TableElement` | Element | 12 | 0xf0 |
| `TextDescription` | ElementDescription | 10 | 0x90 |
| `TextElement` | Element | 12 | 0x100 |
| `ToolTipDialog` | DialogInstance | 10 | 0xc0 |
Non-polymorphic (5 with retail exports): `UserInterface` 0x128, `SpriteContext`
0x88, `Sprite` 0x10, `String` 0x10, `Element::UpdateState` 0x30 (nested in
`Element.h`). The headers also declare six layout-only value types at `UI`
scope (`CaptureArray` 0x10, `CaptureEntry` 0x28, `ClipRegion` 0x10, `Event`
0x30, `SpriteDraw` 0x28, `SpriteInfo` 0x38) with no exports of their own.

## 7. Declared but NOT defined (the unported-member ceiling)

Class scope by brace depth; a declaration with an inline body, `= 0`,
`= default` or `= delete` is not counted; "defined" = a `Class::Member(…) {`
definition under `runtime/src`. **1,253 member declarations across 72 classes;
625 have no definition.** By family:

| family | undefined | why |
|---|---:|---|
| factory statics (`CreateNewObject`/`CreateInPlaceObject`), `TypeId`, `_TypeName`/`_ParentTypeName` | 324 | registered doors (factory tables, B36_RULES 7) or literals recorded not used (B6-7) |
| `operator new` / `delete` / `operator=` | 127 | allocator doors, deferred on purpose |
| constructors incl. copy ctors | 71 | no real ctor on a polymorphic class (B.1/B.2); ctor doors |
| **other members — the honest ceiling** | **103** | exported, declared, not ported |

The 103 by class: `DialogInstance` 22, `Dialog` 13, `UserInterface` 13, `Element`
10, `SpriteContext` 8, `ClipRegionElement` 5, `ListElement` 5, `SpriteSheet` 5,
`ButtonElement` 3, and 1–2 each in `CheckElement`, `ComboElement`,
`DocumentTextElement`, `LayoutElement`, `RibbonElement`, `ScrollElement`,
`ContainerElement`, `DialogController`, `ProgressElement`, `SliderElement`,
`SpinnerElement`, `Sprite` (`Serialize`), `ToolTipDialog`.

CAVEATS, stated rather than buried: the matcher compares member NAMES, so an
overload defined under one signature counts its sibling as defined (e.g.
`UserInterface::SetClipRegion` ×2 is listed once); a member defined through a
qualified name the regex does not see (a macro-generated body) counts as
undefined. **625 / 103 are ceilings, right to within those rows.**

## 8. Doors kept by design

Distinct `@UI@@` mangled names registered by `RegisterRuntimeExport` under
`runtime/src` (`doors.py`), by family:

| family | names |
|---|---:|
| member doors — each a one-line QUALIFIED delegation to a C++ member | 590 |
| factory statics `CreateNewObject` / `CreateInPlaceObject` | 74 |
| dtors `??1` | 69 |
| ctors `??0` incl. copy ctors | 67 |
| `operator new/delete` `??2`/`??3` | 7 |
| `_TypeName` / `TypeId` | 2 |
| **total** | **809** |

Plus, retired only at the END of B.3 (B35_RULES 11, B36_RULES 7): **68**
`??_7X@UI@@6B@` mint rows and **68** `?_typeInfo@X@UI@@` rows in
`standalone/rt_data_mint_tables.inc`; **67** `RT_DELDTOR` rows naming a UI class
in `runtime/src/object/deleting_dtor*.cpp` + the 68th, `StringTable`'s inline
`RtUiStringTable_ScalarDelete` (`stringtable_factory.cpp`).

## 9. Gate and runtime, at the close

See the closing-pass report (the coordinator gates the merged tree): per
commit, `standalone-debug` 0, `standalone-perf` 0, `unit` 0 (462 / 0 failures),
`dispatch_slot_map --check` 0 (109 registrations, 0 mismatches),
`cutover_manifest --gate` OK (59 rows; no warning after commit 4),
`rt_mint_gen --check` UP TO DATE (152 vtables, 104 BaseTypeInfo, 48 storage).
