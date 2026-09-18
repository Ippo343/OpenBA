# ENDGAME B.3.3 — `Object::Base`/reflection/factory design inventory (read-only, 2026-09-18)

Scope: `Object::Base`, `Object::TypeInfo`/`BaseTypeInfo`, `Object::Factory`, `Object::External`/
`ExternalFactory`, `Object::Internal`, `ExternalPtr<T>`, `InternalBasePtr`/`InternalPtr<T>`.
No tree file edited. Format matches `SMARTPTR_INVENTORY.md` and `COLLECTION_INVENTORY.md`;
this document does not repeat what SMARTPTR_INVENTORY already covers in depth — it notes
what changed since 2026-09-13 and adds the parts SMARTPTR_INVENTORY explicitly deferred
(`Object::Base`, `TypeInfo`/`Factory` reflection, the whole-tree vtable census, the
`deleting_dtor` mechanism).

**Headline change since SMARTPTR_INVENTORY (2026-09-13): B.3.1 is already done.**
`RtObject::Internal.h`/`External.h` now contain real C++ classes — `Object::InternalBasePtr`,
`Object::InternalPtr<T>` (Internal.h:276-412) and `Object::ExternalPtr<T>` (External.h:373-509)
— exactly as SMARTPTR_INVENTORY §6 recommended: thin, trivially-copyable/-destructible
wrappers with the old `RtObject::ExternalPtr<T>`/`InternalBasePtr` PODs kept alive
side-by-side for not-yet-converted callers (B31_RULES step-1 pattern). So this document's
new-design surface is narrower than ENDGAME §B.3 step 3 implies: only `Object::Base`,
`TypeInfo`/`BaseTypeInfo`, `Factory`, `External`, `Internal` (the classes, not their pointer
wrappers) remain PODs/free-function collections.

## 1. Type headers — where each type is declared today

| type | file:lines | size | offset asserts | status |
|---|---|---|---|---|
| `RtObject::Base` | `runtime/types/object/TypeInfo.h:58-61` | 8 (vtable ptr only) | 1 (`vtable`@0x00) | POD, "only the vtable pointer is pinned" (comment says rest belongs to "lanes B/C") |
| `RtObject::BaseTypeInfo` | `runtime/types/object/TypeInfo.h:77-129` | 0x38 | 7 fields + sizeof | POD, fully pinned, field-by-field cited to the ctor + 4 readers |
| `RtObject::TypeRecord` | `TypeInfo.h:148-163` | 0x28 | 6 fields + sizeof | POD, the AVL value half |
| `RtObject::TypeNode` | `TypeInfo.h:201-216` | 0x50 (asserted in `avl_trees.cpp`, not here) | key/value/parent/left/right pinned; `balance` at +0x48 is two i16 fields documented as a correction of an earlier int32 guess | POD |
| `RtObject::Factory` | `TypeInfo.h:231-234` | only `root`@0x00 pinned | 1 | POD, "only the tree root is pinned" — count/allocator/compiler-list unmodelled |
| `RtObject::Internal` | `runtime/types/object/Internal.h:72-79` | 0x10 | 3 (factoryIndex, generation, sizeof) | POD, vtable is the exported `??_7Internal@Object@@6B@` (4 slots documented at :59-70), never a hand-built array |
| `RtObject::InternalBasePtr` (legacy POD) | `Internal.h:108-111` | 8 | sizeof only | POD, kept alive for not-yet-migrated callers (`runtime/src/achievement/goal.cpp:411` per the file's own banner) |
| `Object::InternalBasePtr` (real class) | `Internal.h:289-360` | 8 | sizeof + `is_trivially_copyable`/`is_trivially_destructible` | **REAL CLASS**, non-virtual, single member `_ptr` |
| `Object::InternalPtr<T>` (real class) | `Internal.h:373-410` | 8 (adds nothing) | sizeof + trivial-trait asserts | **REAL CLASS**, `: public InternalBasePtr` |
| `RtObject::InternalFactory` | `Internal.h:155-164` | 0x9e8 | 3 fields + sizeof | POD, embeds `Collection::Array<Internal*>` x2 + `::Random` (0x9c4 bytes) |
| `RtObject::SerializerLite` | `Internal.h:199-203` | 0x10 | sizeof only | **Deliberate stand-in POD** — comment says "reproduced here rather than taken from a Wave 4 header, which does not exist yet"; real `IO::Serializer` is B.3 step 4, after Object |
| `RtObject::ExternalPtr<T>` (legacy POD) | `runtime/types/object/External.h:50-53` | 8 | sizeof only | POD, kept for not-yet-converted callers |
| `Object::ExternalPtr<T>` (real class) | `External.h:433-509` | 8 | sizeof + trivial-trait asserts, two instantiations checked (`RtObject::External`, a forward-declared probe type) | **REAL CLASS** |
| `RtObject::ExternalArrayElement` | `External.h:103-116` | 0x10 | 2 fields + sizeof | POD |
| `RtObject::External` | `External.h:118-126` | 0x20 | 3 fields + sizeof | POD — vtable ptr, `Filename _path`, `Array<ExternalArrayElement> _refs` |
| `RtObject::ExternalFactory` | `External.h:267-288` | not sizeof-asserted (only field offsets) | 7 field asserts including a cross-struct one for the path-stack `_size` | POD, 0x43+ bytes; +0x30/+0x38 undocumented-by-a-reader-beyond-the-path-stack |
| `RtObject::ResourceEntry` / `ReferenceEntry` | `External.h:180-201` | 0x18 / 0x10 | full | POD, the two AVL-tree row types (see COLLECTION_INVENTORY §1.1/§5.4 "two node layouts") |
| `RtObject::ResourceSet` / `ReferenceSet` | `External.h:212-223` | `Collection::Set<...>::Node` 0x38 / 0x30 | full | type aliases over the now-templated `Collection::Set` (COLLECTION_INVENTORY closed; `Collection::Set` is itself already a class per the `External.h:208-211` comment: "ENDGAME B.3.2 step 1: `Collection::Set` is now a class") |
| `RtObject::PathStackEntry` | `External.h:258-261` | 0x10 | 2 | POD |

**Duplicate PODs describing the same bytes**: none found for the Object family itself — unlike
`Collection::Array`/`Set` (which had per-instantiation C bodies), each Object-namespace struct
above has exactly one declaration. The closest thing to duplication is the two-tier pattern
now established twice (`RtObject::ExternalPtr<T>` + `Object::ExternalPtr<T>`;
`RtObject::InternalBasePtr`/`InternalPtr` + `Object::InternalBasePtr`/`InternalPtr<T>`) —
intentional, per B31_RULES step 1, not a layout bug. No duplicate `Object::Base`/`External`/
`Internal`/`Factory`/`BaseTypeInfo` struct was found under `engine/types/` (`grep -rn
"struct.*Base\b" engine/types` — 0 hits; `code/Game/Object/factory_registration.h:41-60`
declares its own **parallel** `BaseTypeInfoStorage`/`FactoryRegistration` for the exe-side
static-init list, which is a deliberate second representation — see §4).

## 2. Bodies — `runtime/src/object/*.cpp`

29 files (28 `.cpp` + `rt_factories_object.inc`), **14,030 lines total** including headers;
**13,233 lines** across the 26 `.cpp` files alone (`wc -l runtime/src/object/*.cpp`). This is
3x `SMARTPTR_INVENTORY`'s implicit scope and is not covered by `COLLECTION_INVENTORY` at all.

| file | lines | role |
|---|---|---|
| `deleting_dtor.cpp` | 2011 | The `RT_DELDTOR` table (135 rows, §6) + `DeletingDtorCore`/`EhVectorDtor` |
| `internal.cpp` | 1145 | `Object::Internal`/`InternalFactory` ctor/dtor/AddObject/RemoveObject, `InternalBasePtrSerializeBody`, `InternalPtrIsDerivedFrom` |
| `type_system.cpp` | 872 | `Factory::Startup` (line 789, `RtObject_Factory_Startup`), `TypeNameCRC`, the AVL insert/walk for the type tree |
| `external_set.cpp` | 933 | One of the two AVL keyed-instantiation bodies for `ExternalFactory`'s trees |
| `external_ptr_serialize.cpp` | 796 | The single `ExternalPtr<T>::Serialize` body (single + array forms), all 26/8 registered addresses |
| `external.cpp` | 737 | `Object::External` ctor/dtor/GetPath/Load dependency-replay |
| `external_path_win.cpp` | 625 | Path resolution (Windows-specific half) |
| `factory_dtors.cpp` | 544 | Deleting-dtor rows for Factory-adjacent classes (separate from `deleting_dtor.cpp`'s main table — confirm relationship before assuming disjoint; not verified this pass) |
| `reference_set.cpp` | 525 | `ReferenceEntry`/`ReferenceSet` body |
| `external_load.cpp` | 517 | `External::Load`, dependency array replay |
| `external_reftable.cpp` | 470 | Reference-id table serialize |
| `external_path_stack.cpp` | 422 | `PushPath`/`GetPathStack*` |
| `external_savetext_win.cpp` | 407 | `SaveText` (text-format resource save) |
| `external_compiled.cpp` + `.h` | 365+? | `Object::CompiledExternal` (also has its own `RT_DELDTOR` row, `deleting_dtor.cpp:297`) |
| `external_add.cpp` | 382 | `ExternalFactory::Add` (the loader-thread-race site, §7) |
| `object_typeinfo_accessors.cpp` | 316 | Accessors reading `BaseTypeInfo`/`TypeRecord` fields |
| `rt_factories_core.cpp` | 271 | Core factory dispatch glue |
| `internalfactory_persist.cpp` | 252 | `InternalFactory::PersistObject` |
| `external_find.cpp` | 221 | `ExternalList::Find`/`FindNext` |
| `deleting_dtor_leaves.cpp` | 204 | Leaf-class deleting dtors kept out of the main table |
| `factory_file_extension.cpp` | 233 | `GetTypeIdForFileExtension` |
| `rt_object_port.cpp` | 199 | Port init glue (`RtObjectDeletingDtorPortInit` etc.) |
| `external_forceunload.cpp` | 193 | Force-unload path |
| `external_list.cpp` | 255 | `Object::ExternalList` |
| `ext_compilers.cpp` | 131 | Compiler list (`LoadCompilers`/`UnloadCompilers`, per `Factory`'s comment at `TypeInfo.h:224`) |
| `internal_ptr_array_serialize.cpp` | 96 | `InternalPtr<T>` array-serialize, both retail instantiations |
| `class_operator_delete.cpp` | 111 | Shared `operator delete` plumbing for deleting dtors |

**Registry rows** (`RegisterRuntimeExport` + `RegisterRuntimeAddr`, pattern
`grep -c "RegisterRuntimeExport\|RegisterRuntimeAddr" runtime/src/object/*.cpp`):
**388 total** (97 `RegisterRuntimeExport` + 291 `RegisterRuntimeAddr`). By file, top five:
`deleting_dtor.cpp` 139, `external_ptr_serialize.cpp` 32, `external.cpp` 24, `internal.cpp` 23,
`deleting_dtor_leaves.cpp` 22. Not broken down "by class" beyond this — the table rows are
per-address, and one class (e.g. `Object::External`) spans several files' rows.

## 3. Vtables — hand-built arrays and the whole-tree census

**`Object::Base`'s own vtable is NOT a hand-built array** — `TypeInfo.h:58-61`'s `Base`
struct carries only the vtable pointer field; no `g_Base_Vtable[]` exists anywhere in the
tree (`grep -rE "g_\w*[Vv]table" code/ engine/ runtime/ standalone/` finds 229 distinct
identifiers, none matching `Base` alone — Base always appears as part of a derived class's
stamped chain, e.g. `BaseVtable()`/`ExternalVtable()` helper functions in
`runtime/src/achievement/goal.cpp:115-127` that return the DLL's own exported vtable
addresses for stamping, not locally-built slot arrays).

**Slot order of `Object::Base`, as the code documents it today** (`TypeInfo.h:47-61`,
`Internal.h:59-70`):

| slot | byte offset | name | retail address | evidence |
|---|---|---|---|---|
| 0 | +0x00 | scalar deleting destructor | class-specific (e.g. `0x1800c0360` for Base itself, per `deleting_dtor.cpp:295`) | `DeletingDtorSpec` table row "Object::Base ... probe subject" |
| 1 | +0x08 | *unnamed* — no reader found in this file set | — | `Internal.h`'s vtable dump lists Internal's slot 1 as `TypeName()`; whether Base's own slot 1 is the same is NOT confirmed by anything this pass re-read |
| 2 | +0x10 | `GetTypeId() const` | pinned via `Base::IsA` (retail `0x1800491c0`) | `TypeInfo.h:49-54`: "`(**(code**)(*(longlong*)this+0x10))()`... vtable slot 2 is `GetTypeId`" |
| — | — | (Internal adds a 4th slot at +0x18, `Persist`, empty in both builds) | `0x1800bfda0` | `Internal.h:63-65` — this is `Internal`'s OWN vtable, not proof Base has 4 slots; Internal's vtable is a *different*, longer, exported array (`??_7Internal@Object@@6B@`) than Base's (`??_7Base@Object@@6B@`) |

So only 2 of `Object::Base`'s own slots (0 and 2) are independently pinned by a reader in
this file set; slot 1's name is UNKNOWN here (decision 2's worked example in ENDGAME §B.1.2
guesses "Slot1()" with a "retail name once known" comment, matching this gap).

**Classes deriving from Base via a hand-built/exported-vtable-stamp mechanism, tree-wide**:
`grep -rl "Base@Object" .` (mangled `??_7...Base@Object@@6B@` occurrences) matches **169
files**. A tighter proxy — distinct mangled vtable symbols the closure census tracks as
`mint-vtable:` rows — gives **209 rows / 23 distinct classes** in
`docs/runtime_port/closure/census.csv` (a partial, point-in-time sample of classes the
closure tool currently tracks as open work, e.g. `MaterialInstance`, `SpriteSheet`,
`BinaryReader/Writer`, `Visitor`, `Element`, `DialogInstance`, `ToolTipDialog` — not the
whole census). The exe side (`code/Game`) separately hand-builds **197-229** `g_*Vtable`-
style arrays (`grep -rlE "g_\w*[Vv]table"` = 197 files, 229 distinct identifiers) for
`Component`/`ControllerComponent`/`Description` classes — the current-day count behind
ENDGAME's 2026-09-13 snapshot of "161 hand-built vtable arrays ... 360 `Register*Vtable`
functions" (grown since; no `Register*Vtable(` function-name pattern was found in
`runtime/` this pass, so that half of the ENDGAME figure may be an exe-side/`code/`
convention only — flag as UNKNOWN provenance, not re-derived).

**Size of the thing decision 2 has to coexist with**: every one of the ~209 mint-vtable rows
plus the ~229 exe-side hand-built arrays is, transitively, a class whose vtable slot 0 must
match `Object::Base::DeletingDtor`'s retail slot order the moment `Base` becomes a real
class with a compiler-generated vtable — i.e. potentially **400+ classes** whose vtable
layout decision 2 constrains, though only the ~22 `ControllerComponent` multiple-inheritance
types (§7) and the direct `Object::Base`/`Internal`/`External` chain are B.3.3's own scope.

## 4. Reflection — how type registration works today

**Two parallel registration lists**, confirmed by re-reading both sides:

1. **Runtime-DLL side** (`RtObject::BaseTypeInfo`, `TypeInfo.h:64-129`): a `static` per
   registered class, intrusively linked through `_firstTypeInfo` (ctor at
   `??0BaseTypeInfo@Object@@...` retail `0x180006320`). `Factory::Startup`
   (`type_system.cpp:789`, ported as `RtObject_Factory_Startup`) walks the chain ONCE, CRCs
   each name (`TypeNameCRC`, reflected CRC-32, poly `0xEDB88320`, over the UTF-16LE-widened
   name), inserts a `TypeNode` into the `Collection::Set<Pair<u32,TypeRecord>>` AVL tree at
   `Factory::root`, and on a second pass fills `TypeRecord::parent`/`children`.
2. **Exe side** (`code/Game/Object/factory_registration.h`/`.cpp`, 247+189=436 lines): a
   PARALLEL, hand-written `FactoryRegistration` struct (`factory_registration.h:49-60`) that
   embeds its own `BaseTypeInfoStorage { uint8_t bytes[0x38] }` — a byte-array mirror of the
   SAME 0x38-byte layout `TypeInfo.h` already documents field-by-field, duplicated because
   the exe-side static-init list must build before the Runtime DLL's ctor can run
   (`factory_registration.h:1-18` explains the ordering: `InvokeAllRegistrations()` calls the
   DLL's real `BaseTypeInfo` ctor for each pending entry, so this parallel struct is a
   staging buffer, not a second source of truth for the layout).

**`REGISTER_TYPE`/`REGISTER_TYPE_FULL` call sites**: **300 + 5 = 305** (`grep -rc
"REGISTER_TYPE(" / "REGISTER_TYPE_FULL("` across `code/ engine/ runtime/ standalone/ dx11/`).

**`COMPONENT_LIVE_FACTORY*` / `CONTROLLER_LIVE_FACTORY*` macro uses**: 36 + 27 = **63**
(`component_factory_base.h` defines 6 macro variants, `controller_live_base.h` defines 2).

**`GetTypeIdByName("…")`-style lookups**: **654** (`grep -rn "GetTypeIdByName("` across
`code/ engine/ runtime/ standalone/ dx11/`, all 654 in `code/` (476) and `standalone/` (216)
— zero in `engine/`/`runtime/`/`dx11/`). ENDGAME §B.1.4 states 639 (2026-09-13); this is a
re-measure with the same grep shape, 15 higher — consistent with ongoing REGISTER_TYPE
growth, not a discrepancy in method.

**Distinct call shapes found, one example each**:
- **Direct literal lookup**, no caching: `::Object::GetTypeIdByName("CharacterComponent")`
  (`code/Game/Action/action_base_move.cpp:71`).
- **Cached function-local static** (the dominant idiom the header's own comment at
  `factory_registration.h:76-77` recommends — "caching the result in a function-local static
  after first hit"): `if (s_tidModelDesc == 0) s_tidModelDesc =
  ::Object::GetTypeIdByName("ModelDescription");` (`action_base_movedeps.cpp:129`).
- **Indirect through `WriteComponentTypeId`** (the Description-vtable-slot-+0x40 contract,
  `factory_registration.h:101-114`): `::Object::WriteComponentTypeId(out,
  "AbsorbResourceComponent")` (`code/Game/Component/absorbresourcedescription.cpp:91`), one
  per Description class that maps to a Component.
- **`IsTypeDerivedFromByName`** (registry-based substitute for the engine's
  `IsDerivedFrom` when the question is about exe-side classes,
  `factory_registration.h:144-153`) — not individually counted this pass.

`IsDerivedFrom` occurrences (any form, name lookup or call): **code 1613, engine 64, runtime
187, standalone 111** (`grep -rc "IsDerivedFrom"`, source files only — this pass's first
attempt at this count over `standalone/` returned 4841 before excluding `standalone/build*/`
object files, which is a trap worth flagging: **always exclude `standalone/build*/` and
`tests/unit/build/` explicitly**, `--include=*.cpp --include=*.h --include=*.inc` is not
enough on its own since a bare `grep -r` still descends into `.o` files that happen to be
under a source-looking path).

## 5. Caller vocabulary

- **`fn_*` typedefs in `engine_funcs.h` naming Object/Base/External/Internal/Factory**:
  **106** matches of `fn_(Object|Base|External|Internal|Factory)` out of **623** total
  `typedef ... fn_*` lines in the file — i.e. ~17% of every resolved-function-pointer
  typedef in the engine belongs to this one namespace family.
- **`@Object@@` mangled-name occurrences** (files, `grep -rc`): code 49, engine 92, runtime
  1722, standalone 1607, dx11 0. Runtime and standalone dominate — expected, since the class
  bodies and the by-address registry rows both live there; `code/`'s 49 are almost entirely
  `ResolveExport`/`ResolveAddr` string literals (same pattern SMARTPTR_INVENTORY §2 already
  found for `ExternalPtr`/`InternalPtr` specifically).
- **Slot-0 deleting-dtor call shapes**:
  - `CallSlot0`: 124 in `runtime/`, 0 elsewhere.
  - `DeletePtr`: code 167, engine 9, runtime 13, standalone 5 (total 194) — the exe-side 167
    is `Object::InternalBasePtr::DeletePtr()`/`Object::ExternalPtr<T>` equivalents being
    called from game logic, i.e. this is the ALREADY-CONVERTED-class call path, not raw
    vtable dispatch.
  - Raw `reinterpret_cast<void***>(...)` (the shape `InternalBasePtr::DeletePtr` itself uses
    internally, `Internal.h:317`): code 111, engine 6, runtime 249, standalone 76 (total
    442) — the better tree-wide proxy for "raw vtable-slot dispatch", since the literal
    `(*(void***)obj)[N]` spelling from the ENDGAME banner is not how this codebase's actual
    C bodies are written (they alias through a named `void** vtbl` local, e.g.
    `Internal.h:317-318`).
  - A literal `vtbl[0xNN]` indexed-call idiom was searched for tree-wide and found only
    **2 occurrences** (`vtbl[0xa0]`, both in `runtime/`) — this pattern is rare; most vtable
    dispatch in the ported bodies goes through a named function-pointer cast at the call
    site instead of an indexed array read, which is a maintainability note for B.3.3's own
    recipe (a real class's method call replaces the WHOLE cast-and-call idiom, not just an
    index).
- **Raw vtable-slot dispatch by N**: not resolvable to a clean per-N table this pass — the
  `reinterpret_cast<void***>` idiom's index literal is usually computed inline
  (`vtbl[0]`, `vtbl[2]`, or a named `enum` constant like `kInternalVtblType = 0x10`,
  `Internal.h:224`) rather than collected in one place; a full by-N breakdown would need a
  dedicated script, not a grep, and is marked **UNKNOWN / left to the eventual implementer**.

## 6. `deleting_dtor.cpp` / the `RT_DELDTOR` mechanism

`runtime/src/object/deleting_dtor.cpp` (2011 lines) plus its header
`deleting_dtor_internal.h` (107 lines, `rtdtor` namespace) is the ONE oracle for MSVC's
vector-deleting-destructor pattern across the whole Object hierarchy: one 8-byte-aligned
`DelDtorSpec{ addr, dtorVa, elemSize, name }` row per class, built with the macro
`RT_DELDTOR(ADDR, DTOR, SIZE, NAME)` (`deleting_dtor.cpp:285`). **135 rows**
(`grep -c "RT_DELDTOR(0x"`), matching the header's own `kSpecCount = 135` constant
(`deleting_dtor_internal.h:66`) exactly — the file's static_assert guards this count against
silent drift, and the header's own banner documents a real merge hazard already paid for
once (three Wave 27 lanes appending at the same table index, §"MERGE HAZARD" comment,
`deleting_dtor_internal.h:86-96`).

**`Object::Base` itself has a row**: `RT_DELDTOR(0x1800065b0, 0x1800c0360, 0x008,
"Object::Base")` — annotated `// probe subject` (`deleting_dtor.cpp:295`), size 8 (matching
`RtObject::Base`'s vtable-pointer-only layout), meaning the table ALREADY calls through to
Base's real `~Base` at `0x1800c0360` today.

**Interaction with decision 2's `DeletingDtor(unsigned flags)`**: the table's shape —
address-in, `(self, flags)`-signature dispatch — is EXACTLY the shape ENDGAME §B.1 decision
2 asks for (`virtual Base* DeletingDtor(unsigned flags)` at retail slot 0). The 135-row table
is therefore not something B.3.3 needs to redesign; it is the reference data an eventual
"replace `DeletingDtorCore`'s address-indexed dispatch with a real virtual call" pass reads
row-by-row, one row retiring per class converted (mirrors COLLECTION_INVENTORY's "339
registered rows collapse into ~27 template methods" shape, but for dtors instead of
`SetSize`). No row references `Object::External`/`Internal`/`CompiledExternal` by anything
other than by-address dispatch today (`deleting_dtor.cpp:294-297`), so those three classes'
`~Class` bodies stay reachable exactly as retail's own vtable would reach them once the
class is real — the table is a stand-in vtable-slot-0 array, not a separate mechanism decision
2 has to reconcile with a second design.

## 7. Risks and open design questions

1. **`ExternalFactory::Add`'s AVL-rotate loader-thread race** (STATUS.md:104,173; resave
   gate README:59-61): "one run in ~12 crashed in `ExternalFactory::Add`'s AVL rotate during
   the loader, with a `[GUL::Load] redundant dispatch` line just before it (two loader
   dispatchers racing)". Pre-existing, intermittent, NOT fixed, and sits directly in
   `ExternalFactory`'s `_resources`/`_references` tree mutation path — a real
   `ExternalFactory` class must not paper over this with new locking that changes
   observable timing; it needs its own investigation, out of scope for a mechanical
   POD-to-class pass.
2. **`Object::Base` slot 1 is unnamed** (§3) — decision 2's own worked example in ENDGAME
   already anticipates this ("retail name once known"); B.3.3 will need to name it (or leave
   it `Slot1()`) before the class can compile with retail's full vtable, since GCC requires
   every slot declared even if unused by any ported caller.
3. **`ControllerComponent`'s +0x28 second vptr, 22 multiple-inheritance types**
   (`code/Game/Component/controller_live_base.h:6,26,190` — "the 22 building-behaviour
   components: BuildComponent, ..." with slot 5 at retail +0x28 being `OnState`). ENDGAME's
   "56 exe types carry a secondary vptr" figure (§0) is broader than just
   `ControllerComponent`; this pass confirms only the `ControllerComponent` family's count
   (22) and its file set (`code/Game/Component/{controller_live_base,
   controller_state_table, marketlocationcomponent_funcs, residencecomponent_funcs,
   statemachinecomponent_funcs, storagelocationcomponent_funcs,
   tradecomponent_funcs}.h`, 7 files) — the other ~34 types are UNKNOWN to this pass (not
   re-derived; ENDGAME's number stands unverified here).
4. **Places that compare vtable pointers for identity**: found at least one — 
   `runtime/src/audio/audio_stream_win.cpp:303-389`, `ThreadBaseVtable()` compared with
   `!=` against a live object's stamped vtable pointer to distinguish `System::Thread`-base
   objects from `Stream`-derived ones during teardown. Not Object-namespace itself, but the
   SAME pattern (`BaseVtable()`/`ExternalVtable()`/`GoalVtable()` in
   `runtime/src/achievement/goal.cpp:115-127`) is how the Object family's own multi-level
   stamping works today — `goal.cpp:194` writes `*reinterpret_cast<void**>(self) =
   BaseVtable();` before overwriting with the more-derived vtable, i.e. retail's own ctor
   chain literally stamps Base's vtable and then overwrites it, which a real C++ constructor
   chain reproduces for free but which any code that reads a vtable pointer MID-CONSTRUCTION
   (rare, but `goal.cpp` proves the pattern exists in this codebase) must not break.
5. **`RtObject::SerializerLite` is an intentional stand-in POD**, not a class, and
   `Object::Base`/`External`/`Internal`'s Serialize paths all go through it. B.3.3 converting
   `Object::Base` does NOT get to also convert `IO::Serializer` (that's B.3 step 4, after
   Object) — every new class's `Serialize` method keeps calling through
   `SerializerLite`/the raw `Encoding` vtable byte offsets (`Internal.h:214-218`,
   `kEncSerializeBool/Uint/Int`, which DIFFER between retail and profile builds — "a port
   MUST use the retail numbers... copying them out of a profile decompile writes the right
   values through the wrong slots", `Internal.h:209-213`). This is a live footgun for anyone
   who cross-checks a Serialize body against the profile-build disassembly.
6. **`Factory::root`'s Set is not yet reconciled with `Collection::Set` being a class**
   (§1, `External.h:208-211`'s comment marks `Collection::Set` as already templated per
   COLLECTION_INVENTORY's own B.3.2 closure) — `RtObject::Factory` (`TypeInfo.h:231-234`)
   still declares only a raw `TypeNode* root`, not a `Collection::Set<Pair<u32,TypeRecord>>`
   member, so `Factory::Startup`'s AVL manipulation (`type_system.cpp`) may or may not
   already be using the real `Collection::Set` API — NOT independently confirmed this pass;
   worth checking before assuming `Factory` "just" needs a `class` keyword, since if
   `type_system.cpp` still does raw node pointer-chasing that is itself B.3.3 scope, not
   already-done B.3.2 scope.
7. **Two parallel `BaseTypeInfo`-shaped structs** (§4: `RtObject::BaseTypeInfo` and
   `Object::FactoryRegistration::BaseTypeInfoStorage`) must stay byte-identical by
   convention, not by a shared type — nothing statically asserts they describe the same 0x38
   bytes against EACH OTHER (each is asserted against its own understanding of the retail
   layout independently). A real `Object::TypeInfo` class (decision 4's `DECLARE_TYPE`/
   `DEFINE_TYPE` macro) is the natural place to collapse these two into one, which is
   explicitly decision 4's stated goal — flag this as the clearest "this design decision
   already has its target shape sketched in ENDGAME, verify it subsumes
   `factory_registration.h`'s staging struct too" item.
8. **No sighting this pass of a `TypeInfo*` or vtable address stored in SAVE data** (the
   ENDGAME risk prompt's specific worry) — `TypeInfo.h` explicitly notes "`BaseTypeInfo::name`
   is content-only" (a binary fact, memory `reference_basetypeinfo_name_is_content_only.md`)
   and saves key on the CRC'd name string, not a pointer (ENDGAME §0: "Saves and the object
   factory key on class-name strings hashed by CRC ... reference ids and persist ids are
   positional"). This is REASSURING, not a risk, but stated as UNKNOWN-turned-CONFIRMED
   rather than asserted without a check.

## 8. Proposed split

**Must be one serial owner's work** (ENDGAME §B.3 step 3's own framing, confirmed by this
inventory): converting `Object::Base`, `BaseTypeInfo`/`TypeInfo`, `Factory` and the
`DECLARE_TYPE`/`DEFINE_TYPE` reflection macro (decision 4) into their final shape. Reasons
specific to what this inventory found: (a) `Object::Base`'s slot 1 name is still unknown and
naming it is a one-time archaeology task that gates every subclass; (b) the two parallel
`BaseTypeInfo` structs (§7 risk 7) need one person deciding how `factory_registration.h`'s
exe-side staging list folds into the new macro, since it currently exists ONLY because the
Runtime DLL's ctor can't run before CRT-init order is settled — a design call, not a
mechanical port; (c) `Factory::root`'s relationship to the now-real `Collection::Set` (risk 6)
needs resolving before ANY class can register through the new tree without either double
templating or a raw-pointer regression.

**Could fan out by directory once the core three classes have their final shape**, mirroring
COLLECTION_INVENTORY's "family-serial, caller-parallel" split:
- `Object::External`/`ExternalFactory`/`ExternalList` + the AVL two-node-layout trees
  (`external*.cpp`, 11 files, ~5,900 lines) — one agent, since `External` is a direct `Base`
  subclass and depends on the core three landing first.
- `Object::Internal`/`InternalFactory`/`InternalPtr<T>` (`internal*.cpp`, 3 files, ~1,500
  lines) — a second agent, largely independent of External's tree work (different AVL
  instances, different factory).
- `deleting_dtor.cpp`'s 135-row table retirement (§6) — a THIRD, purely mechanical, ongoing
  task that can run continuously alongside both (each class conversion retires its own row;
  no single agent owns the whole table, the table is just where progress is visible).
- Caller-side migration (the 106 `fn_Object*`/`fn_Base*`/... typedefs in `engine_funcs.h`,
  the 654 `GetTypeIdByName` call sites, the 194 `DeletePtr` call sites) — fully parallel by
  `code/Game/<Module>` directory, same as COLLECTION_INVENTORY's "Step 2 callers" pattern,
  and should NOT start until the core three classes and at least `External`/`Internal` have
  landed, since every caller-side change needs `X::TypeId()` (decision 4) to exist first.

Every count above was produced by a grep/wc run in this session against the live tree
(`C:\code\GIT\untitled_rev_eng_project`, `.claude/worktrees/`, `standalone/build*/` and
`tests/unit/build/` excluded throughout); the pattern is stated next to each number. Anything
without a pattern citation in this document is explicitly marked UNKNOWN.
