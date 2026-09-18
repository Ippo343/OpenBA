# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 close, tools/cutover_closure, roots = census + every minted vtable slot)

| | |
|---|---|
| reachable functions from our call surface | 2,871 |
| unported | 128, all C runtime (our CRT covers them) |
| **engine functions to port (static lower bound)** | **0** (206 → 856 → 145 → 0 in one day) |
| minted vtables complete | 152 of 152 (the two unexported IO::Encoding tables joined 2026-09-12 evening) |
| registry doors | name and address are ONE door (`standalone/rt_export_addrs.inc`) |

Worklist: `docs/runtime_port/closure/WORKLIST.md` (empty). Every function the DLL's call graph
reaches from our call surface OR from any minted vtable slot is ported, and every port is
reachable both by mangled name and by retail address. From here the closure cannot help:
what the gate finds next is state, not code — objects handed to our bodies by a route that
built them wrong, and callbacks stored in data.

## Endgame (2026-09-13)

- **B.3.2 CLOSED (2026-09-18):** `Collection::Array<T>`, `Set<T, Compare>` and
  `LinkedList<T>` are real templates under retail's names
  (`runtime/types/collection/Collection.h`) wrapping the ported cores; three core
  agents, four caller batches by directory, two closing batches (template side
  f82ff0c1, game side 89b9cf9b) and this final pass. What the closing pass added to
  the templates: ONE allocator door for both the tree and the array
  (`rtcoll::DoorAlloc`/`DoorFree`, `runtime/src/collection/alloc_door.cpp`, so
  template allocations are visible to `BANISHED_REDZONE`); the width-8 `AvlOps`
  specialisation the `Set<System::Filename>` path set needs; the deep
  `Array::operator=` (`rtcoll::ArrayAssignCore`); `Set::InsertInPlace`/`Emplace`;
  and one game-side bridge header — `code/Game/Component/coll_bridge.h` — with
  `Dialog/dialog_array_bridge.h` deleted.
  **Deleted here:** 14 `fn_*Array*`/`fn_*Set*`/LinkedList pointers in
  `engine/funcs/engine_funcs.{h,cpp}` with zero callers tree-wide, taking 5 typedefs,
  5 hand-written C bodies (`fn_ExeArrayResize/2/3/Push` and
  `ExeArrayResizeFill_CppImpl`) and their init + `_DEBUG` printf lines with them;
  `fn_UI_GetFirstDialog` went with its last consumer, the inline dialog walk in
  engine_funcs.h, which now runs on `ui->_dialogs.GetFirst()` — the same
  CheckPtr/GetController order, and that walk IS retail's GetFirstDialog body
  (`*retval = this->_dialogs._head`). 36 collection pointers remain, every one with a
  live caller. `runtime/src/io/modification.cpp`'s `RtColl_SetDestroySubtree_180010470`
  is folded onto `Collection::Set<Filename>::DestroySubtree` (same post-order walk,
  same element dtor, same free door) and its private `RtColl_SetNodeFree_180009c30`
  copy went with it; the clone twin `RtColl_SetCloneSubtree_1800104f0` has no template
  analogue and stays.
  **Rows:** of the 323 `RegisterRuntimeAddr` rows in `runtime/src/collection/`, **82 are
  deleted with their shims** (array.cpp 67, array_destroy.cpp 10, array_graphics_anim.cpp 3,
  array_graphics_mesh.cpp 2 — all trivial per-instantiation `~Array`/`SetSize`/element-destroy
  bodies) and **241 stay**: 81 reached by NAME, 160 by ADDRESS. The address form is not just
  `ResolveAddr(Module::Runtime, 0x18…)` — 27 of those 160 go through per-file wrappers
  (`RtAddr<Fn>(0x…)`, `RtAddrFn`, `ArrayDtor(0x…, p)`, `ArraySetSize(…)`, `SetSizeWithFill`,
  `PlainArrayDtor`, `ByAddr(0x…)`, one address held in a named `constexpr`), which a sweep
  that greps only for `ResolveAddr` will miss. A comment or a `printf`/`static_assert` string
  citing an address is NOT a caller; 79 of the 82 had only that. The 82 were additionally
  cleared of bare-hex/table references (no `k…Addr[]` entry, no arithmetic) and ZERO
  collection address appears in `rt_export_addrs.inc` or `rt_data_mint_tables.inc` — so
  regenerating both with `tools/rt_mint_gen.py --check` produced no change, as expected.
  Nothing broke: link, unit and all three routes are clean, which is the honest test for a
  row (a dead row that was live shows up as EXEC-of-0 at runtime, never as a link error).
  `make unit`: **167 tests, 0 failures**. Debug and perf exes both build clean (the
  perf build is the one that catches a deleted init printf). Routes, all exit 0, no
  crash/hang log: autostart + pause-menu quit, the tutorial route, the front-end
  menu route. `tools/dispatch_slot_map.py --check`: 0 mismatches before and after —
  no registration changed, and slot 0x140151ca0 (GetFirstDialog) was never registered.
  An `Array::operator= = delete` probe over the whole merged tree found **four**
  copy-assignment sites, all of them the deliberate coverage in
  `tests/unit/array_test.cpp`: no production code copy-assigns an Array, so the
  operator becoming a DEEP copy in this pass changed nothing live.
  `reinterpret_cast` (ENDGAME §B.4 metric):

  | | code | engine | runtime | standalone | dx11 |
  |---|---|---|---|---|---|
  | B.3.1 close | 23,784 | 906 | 6,788 | 3,055 | 148 |
  | B.3.2 close | 23,661 | 899 | 6,669 | 3,052 | 148 |

  Same pattern as B.3.1 and for the same reason: a `fn_SomeArrayResize(hdr, n)` call
  becomes `reinterpret_cast<GameColl::I32*>(field)->SetSize(n)`, so one cast shape is
  traded for another. The drop is real but small (−242 overall) and comes from the
  pointers and shims that disappeared outright, not from the conversions. These casts
  are the B.3.2 bridge casts, one per use of a raw array header inside a struct that
  is not a class yet; each disappears with that struct's own §B.2 conversion.

  **Left, and why:**
  - ~~The ~20 sites that cite exe **0x1400021d0** but use the zeroing `GameColl::I32`.~~
    CLOSED 2026-09-18 (merge 36c36cf4): 21 sites in 13 files flipped to `I32NoInit` —
    9 are `SetSize(0)`/shrinks, 12 grow, and every grow is followed at once by a loop or
    a bulk stream read that writes the whole tail (per-site proof in each comment; no
    read-before-write found in retail). Two `SetSize(n, fill)` appends stay on `I32`
    (`createrawmaterial.cpp`, `toolbardialog.cpp`: neither calls 0x1400021d0/0x140003600).
    Gate: debug + perf build, unit 167/0, autostart + pause-menu quit, tutorial, Load Game
    dialog all exit 0, resave gate PASS, dispatch check 0, `make sa-stress` 6000 frames
    clean quit at sim 0.20 ms/iteration (+5.9% printed; the close run printed +3.1% at the
    same 0.20 — spread, `perf/results/20260918_131903_i32noinit_clean`).
  - Not converted, by design (documented in-file): `EntityList`'s update/post-update
    trees (an unbalanced BST standing in for retail's AVL, entangled with the tree
    guards), exe helpers with non-zero or partial tail init or per-element deep copy,
    the OOM-rollback appends in tradeui, the packfile introsort,
    `Configuration`/`Persistence` settings maps (no type header / POD String node).
  - The 241 collection rows kept because they are reached BY ADDRESS or BY NAME
    (counts above). A row reached by address is a live door; deleting it is an
    EXEC-of-0 at runtime, not a link error — so the wrapper call forms have to be in
    the sweep's vocabulary before any row is deleted.
  - The deep `Array::operator=` has **unit coverage only**: no production caller
    copy-assigns an Array. `Set<Filename>` insert (width-8 `AvlOps`) was in the same
    position at the close because `WinData/` holds no `.pkm`; it has since run live —
    2026-09-18, main f215e93f, both mods moved in from `WinData/_mods_off/`, the Mods
    route to Quit: exit 0, empty crash log, no hang log, the mods-Ok reload completed.
    The mods are back in `_mods_off/`. (`set_test.cpp`'s width-8 block drives
    `AvlGeneric` on the host, not the ported bodies — the real run verifies those.)
  - 36 collection `fn_*` pointers and `fn_Array_Resize_Stride02/08/14/16B/20`,
    `fn_BST_*`, `fn_DynArray_Clear_*` and friends stay: each still has at least one
    caller in `code/Game`, and each disappears with its caller's own conversion.

  **Coordinator gate on the merged main (2026-09-18):** build, unit 167/0, autostart +
  pause-menu quit and the Load Game dialog route exit 0 after every one of the three
  merges; resave gate PASS after each (the last merge read the README's known one-byte
  `count` flake at +0x18 once — 5689 vs 5688, sizes identical — then PASS twice on the same
  exe). `make sa-stress` (perf exe, 6000 frames, clean quit,
  `perf/results/20260918_124928_b32close`): sim **0.20 ms/iteration, +3.1%** against the
  2026-09-05 baseline (B.3.1 closed at +5.5%), so the allocator door's per-call slot
  re-read is not visible.

- **B.3.3 CLOSED (2026-09-18):** `Object::Base`, `Object::BaseTypeInfo`, `Object::Factory`,
  `Object::External` / `ExternalList` / `CompiledExternal` / `ExternalFactory`,
  `Object::Internal` / `InternalFactory` are real classes under retail's names, over a real
  `Base` with the three measured slots and no virtual destructor;
  `DECLARE_TYPE`/`DEFINE_TYPE` exist; `Factory` is `Collection::Set<TypeEntry,…>` wrapping
  the ported AVL core. One owner through step 5, four caller batches (6A External, 6B
  Internal, 6C Factory, 6D CheckPtr), one closing pass. History of steps 1–6D is in the
  per-step notes below, unchanged.
  **What the closing pass deleted.** Twelve `fn_*` pointers in
  `engine/funcs/engine_funcs.{h,cpp}` with their typedefs, externs, definitions,
  `ResolveExport`/`ResolveAddr` initialisers and `_DEBUG` value prints:
  `fn_ExternalFactory_{FindOrLoad,Find,FindNext,IsReferenceModeEnabled,GetReferenceId,`
  `GetPathForReferenceId,Clean}`, `fn_External_{GetPath,SaveText}`,
  `fn_Factory_{GetTypeIdFromName,CreateObject}`, `fn_ExternalPtr_Serialize_Typed_7140`.
  Ten of them were freed by converting the last **10** call sites batches 6A–6C did not
  own, all inline bodies in `engine_funcs.{h,cpp}` (1 `Find`, 2 `FindNext`,
  2 `GetTypeIdFromName`, 5 `InternalBasePtr_Ctor`); the other two were already at zero.
  Then **117** guard conjuncts across 36 files plus 9 hand edits (4 `if (P) {…}` blocks
  unwrapped, 2 `P ? call : nullptr` ternaries collapsed, 3 printf/DLOG `%p` value arguments
  dropped). Removing a guard is safe here because each pointer resolved unconditionally
  through a REGISTERED Runtime export — `[RtResolve] 0 misses` on every boot since B.3.1 is
  the standing proof that each guard was always true — and the call behind it is now our own
  compiled member, not a resolved pointer. Also gone: `ThreadBaseVtable()`
  (`audio_stream_win.cpp`, zero callers by name, and 0x180155f20 STAYS — persistence_worker
  and both generated tables cite it); `rtavl::TypeSetInsert` with its declaration and its
  `RegisterRuntimeAddr(0x180030140, …)`, swept clean of `ResolveAddr`, of every per-file
  wrapper form (`RtAddr`, `RtAddrFn`, `ByAddr`, `RT_DATA_SLOT`, `RT_EXPORT_ONCE`,
  `RtResolveCached`, `RtPortLookupAddr`, named `constexpr`), of `RT_MINT_UNNAMED` slots in
  `rt_data_mint_tables.inc`, of `rt_export_addrs.inc` and of the exe dispatch slots.
  `runtime/types/object/internal_class.h` is merged back into `Internal.h` (234 lines, no
  declaration edited) and `External.h`'s inert second `#include "TypeInfo.h"` — the reason
  the split ever existed — is deleted in the same commit.
  **Added:** `Object::Factory::Shutdown()` (retail `?Shutdown@Factory@Object@@QEAAXXZ`,
  0x1800c0600), ported from the disassembly and registered under both doors.
  **B33_RULES §9 step 7 was wrong about it** and the correction matters: `Shutdown` is eight
  instructions that FreeLibrary the compiler-plugin HMODULE at the unexported 0x1801fc6d0
  (the cell `UnloadCompilers` and `Engine::Shutdown` also release). The type-tree walk is
  `??1Factory@Object@@` (0x1800071a0 → 0x180008110), ported since Wave 11 as
  `RtObject_Factory_Dtor`. So `Shutdown` is **not** a prerequisite for moving
  `?_singleton@Factory@Object@@` off the mint, and that cell is untouched.
  **Left, and why.**
  · `fn_ExternalList_GetResourceCount` — two REAL calls remain, both inside fprintf
  argument lists (`ui_dialog_stub_factories.cpp`, `gameupdateloop_load.cpp`). The perf build
  strips printf ARGUMENTS, so 6A deliberately did not convert them; the pointer and its five
  guards stay with them.
  · `fn_InternalBasePtr_Ctor` / `fn_InternalBasePtr_DeletePtr` — NOT at zero. Ten files
  cache them in file-static function pointers behind an IAT-slot preference
  (`s_fn_IBP_Ctor = resolveIAT(0x140152a90)`, then fall back to the global) and call through
  the cache: `placementtool.cpp`, `edittool.cpp`, `removenaturalresource.cpp`,
  `performwork.cpp`, `dropoffrawmaterial.cpp`, `work_act_perform.cpp`, `work_act_path.cpp`,
  `work_act_clear.cpp`, `pickup_gate.cpp` and `work_base_reimpl.cpp`'s helpers. Converting
  them means deciding the IAT-vs-export preference per site — a caller batch, not a sweep.
  · `tools/rt_mint_gen.py` was NOT re-run and no `.inc` was regenerated: `--check` reports
  the committed `rt_data_mint_tables.inc` UP TO DATE (152 vtables, 104 BaseTypeInfo, 48
  storage). Dropping the `?_firstTypeInfo@BaseTypeInfo@Object@@` row would remove a LIVE
  door rather than inert bytes — `standalone/main.cpp` reads 0x1801fc6e0 by ADDRESS twice
  (the boot probe and the factory self-test, i.e. the source of the
  `[GameMain] Post-Startup _firstTypeInfo` line) and `cutover_manifest.inc:57` carries the
  name as `CUTOVER_REQUIRED`. Only the row's 8 bytes in `kStorageBytes` are inert;
  `MintStorageCell()`'s redirect is what makes the name door, the address door and the class
  static one cell, so it stays.
  · The five `?_typeInfo@` rows (`Base` 0x1801f9c50, `External` 0x1801f9b38, `ExternalList`
  0x1801f9be0, `CompiledExternal` 0x1801f9b00, `Internal` 0x1801f9c18) stay minted and the
  three External classes stay without `DEFINE_TYPE`, for the reason External.h's closing
  banner states, re-measured here: ~20 live by-name `RT_DATA_SLOT` readers, two
  `CUTOVER_REQUIRED` rows, two by-address readers, `reg_mint_promoted.cpp`'s promotion rows
  — and no way to show the post-Startup type ids are unchanged without a boot the closing
  agent cannot run. `Object::Internal` already carries `DEFINE_TYPE` beside its minted
  record, which is why the chain reads 417 and not 416; the closing pass adds and removes no
  chain record, so **417 is still the expected count on a live boot**.
  · Everything B33_RULES §9 step 7 lists as STAY is untouched: the `RT_DELDTOR` rows for
  `Base` / `External` / `CompiledExternal` / `Internal`, `RtObject_Base_Dtor`, every minted
  vtable, `fn_External_External` / `fn_External_Dtor` / `fn_ObjectInternal_Ctor` / `_Dtor`,
  `fn_ExternalList_Find` (signature disagreement recorded, not fixed: `External.h` spells
  the 2nd parameter `uint32_t`, retail's is `Object::Type` by value), the serializer
  pointers, `CallSlot0`, every `GetTypeIdByName` site, the exe-side `FactoryRegistration`
  staging list, the runtime port of the empty `CheckPtr` member, and the
  `ExternalFactory::Add` path. The three dispatch-slot rows 0x140151218 / 0x140151220 /
  0x140151228 stay registered although they now have no exe-side reader — they are the exe's
  own PE import slots and the Runtime ports behind them are unchanged.
  · §6.3's wrong reason is deleted where it was written down: `InternalBasePtr::DeletePtr`'s
  raw slot-0 call stays raw because the pointee is `RtObject::Internal*`, the POD, so the
  member spelling adds a bridge cast and removes no indirection — NOT because it "may be
  handed a minted-vtable object", which `object_vtable_abi_test.cpp` disproves.
  **Casts (B.4 metric), measured here, build dirs excluded:** 34,809 `reinterpret_cast`
  — code 23,971, runtime 6,685, standalone 3,054, engine 888, dx11 148, tests 63 (2026-09-13
  baseline ~35,700). B.3.3 added **1,045** `// B.3.3 bridge` markers, each of which dies
  with its receiver's own conversion.
  **Closing-pass checks:** `standalone-debug`, `standalone`, `standalone-perf` and `unit`
  all exit 0; unit **206 tests, 0 failures**; `tools/dispatch_slot_map.py --check` 109 rows,
  **0 mismatches**; `tools/runtime_census.py` clean (every remaining registration a LITERAL
  address); `tools/cutover_manifest.py --gate` **OK, 59 rows, 0 GATE WARN**;
  `tools/rt_mint_gen.py --check` UP TO DATE; zero new warnings on any line changed.
  The boot lines that carry each risky deletion: `[RtResolve] 0 misses` and the absence of
  `[RtMint] TRAP` (the guard removals), the type-table dump plus
  `[GameMain] Post-Startup _firstTypeInfo: 417 entries` (the `TypeSetInsert` retirement —
  Startup still builds the same tree through `Collection::Set::Insert`), and
  `[RtPort]   Object::factory dtors:` now reading **4** exports instead of 3
  (`Factory::Shutdown`).
  **Next (B.3 step 4, IO):** the 159 `fn_ExternalPtr_*Serialize*` /
  `fn_InternalBasePtr_Serialize` / `fn_ExternalRefArray_Serialize` sites over 9 pointers,
  still untouched; `fn_ExternalPtr_Serialize3` is the last reader of exe address
  0x140007140; `Internal::Clone` and `SerializeReferenceTable` as a member;
  `runtime/src/io/file_stream.cpp:393`'s `NoteSlotDispatch` vptr counter dies with
  `IO::BinaryFileStream`; `RtObject::SerializerLite` and the retail `Encoding` vtable offsets
  are what every converted `Serialize` body still goes through.
  **Coordinator gate on the merged main:** (coordinator fills this in)

- **B.3.3 step detail** (`Object::Base` / `BaseTypeInfo` / `Factory` + reflection macro,
  single owner). Rules and the order of work: `endgame_b/B33_RULES.md`; measured input:
  `endgame_b/OBJECT_INVENTORY.md` (spot-checked and corrected 2026-09-18).
  Steps 1–2 done (merge of `b33-owner`): the six design answers — Base has exactly three
  slots (`DeletingDtor`, `TypeName()`, `Type()`), the one record is `Object::BaseTypeInfo`
  and the exe-side staging list folds away, `Factory` (0x10 bytes, 0x50-byte nodes) moves
  onto `Collection::Set` inside step 3 — and `tests/unit/object_vtable_abi_test.cpp`
  (7 tests; unit now 174/0) proving a GCC class without a virtual dtor is slot-for-slot a
  retail vtable, with the second vptr at +0x28 for two bases.
  Step 3 done (merge of `13bcc143`): `Object::BaseTypeInfo` and `Object::Factory` are real
  classes (`Factory` = 0x10 bytes on `Collection::Set` over the 0x50-byte node, wrapping the
  ported rtavl core), `DECLARE_TYPE`/`DEFINE_TYPE` exist, `_firstTypeInfo` is a plain static
  that both mint doors publish (the generated row's bytes are inert until the closing pass
  regenerates). All seven `RtObject_*` shims and the exe-side staging list STAY: the 305
  `REGISTER_TYPE` sites name PODs, so the list folds only as those classes convert.
  Gate: debug + perf build, unit 185/0, a live boot with the type table byte-for-byte the
  same as the pre-merge boot (312 registered, 416/416 ids, same ten id/name pairs),
  autostart + pause-menu quit, tutorial, Load Game dialog exit 0, resave PASS, dispatch 0,
  `make sa-stress` -0.2% (`perf/results/*_b33step3`).
  Step 4 done (merge of `62926d44`): `Object::Base` is a real class — three virtuals in
  retail order (`DeletingDtor`, `TypeName()`, `Type()`), non-virtual `~Base()`,
  `Object::Destroy(p)`, `IsA`/`IsDerivedFrom` members; `DECLARE_TYPE` emits `Type()` /
  `TypeName()`. The three `RtObject_Base_*` shims delegate to it. Base's `RT_DELDTOR` row,
  `RtObject_Base_Dtor` and the minted `??_7Base@Object@@6B@` table all STAY: 64 by-name
  doors, 22 factory-replay `Vptr` rows and 59 `.cpp` readers still stamp that table, and
  its minted slot 0 is filled FROM the row (B33_RULES §6.1: check `rt_data_mint_tables.inc`
  for a `RT_MINT_UNNAMED` slot before retiring any row). They retire at the end of the
  fan-out. Gate: debug + perf build, unit 188/0, live type table identical to the
  pre-step-3 boot, `[RtResolve] 0 misses`, no `[RtMint] TRAP`, three routes exit 0, resave
  PASS, dispatch 0, `make sa-stress` +0.4% / +0.8% on two quiet runs (a first run straight
  after the resave gate printed +10.1% and did not reproduce).
  `-Wno-invalid-offsetof` is in `SA_CXXFLAGS` and `UNIT_CXXFLAGS` (every build variant
  expands `SA_CXXFLAGS`) ahead of step 5, where the first polymorphic classes WITH fields
  land.
  Step 5 done (merges of `3593efd3` External, `7302ecb3` Internal): `Object::External`
  (8 slots; slot 4 is `Compile()`, not nameless), `CompiledExternal`, `ExternalList`
  (9 slots), `ExternalFactory`, `Object::Internal` (4 slots: dtor, `TypeName`, `Type`,
  `Persist`) and `InternalFactory` are real classes over `Object::Base`; the `RtObject::*`
  PODs stay as layout-asserted trivially-destructible aliases, every `RtObject_*` door and
  registration is unchanged, no `RT_DELDTOR` row or minted vtable retired, nothing on the
  `ExternalFactory::Add` path reordered. `Internal` uses `DEFINE_TYPE` (a second chain
  record beside the minted one: same id in pass 1, no tree node, no child — the chain reads
  417 entries, was 416); the three External classes do NOT, until their minted
  `?_typeInfo@` rows are redirected the way `_firstTypeInfo` was (needs
  `standalone/rt_data_mint.cpp` + `tools/rt_mint_gen.py` — closing pass). Left for B.3
  step 4 (IO): `Internal::Clone`, `SerializeReferenceTable` as a member.
  Gate: debug + perf build, unit 206/0, 0 GATE WARN, three routes exit 0, same ten
  id/name pairs, `[RtResolve] 0 misses`, no `[RtMint] TRAP`, resave PASS with the persist
  pair live (`clearRuns=2 resetRuns=2`), dispatch 0. `make sa-stress`: clean quit, sim
  0.21 ms/iteration (+9–10% printed; External-only read +7%). **Recorded, not chased** —
  perf is revisited after B closes (B33_RULES §10).
  Step 6 RE-SCOPED (B33_RULES §9.6): none of the 545 literal `GetTypeIdByName` sites names
  a class that is real yet (287 game/engine names), so that sweep moves into each class's
  own conversion (B.3 steps 5–8); the 124 `CallSlot0` sites wait for their classes; 159
  serializer-pointer sites wait for `IO::Serializer`. What B.3.3 does convert: batch A
  External callers, B Internal callers, C Factory callers, D the 4,700 no-op `CheckPtr`
  calls.
  Batch 6A done (merge of `5d6a6eeb`): 158 of 380 real call sites of `fn_ExternalFactory_*`
  / `fn_External_GetPath` / `fn_ExternalList_GetResourceCount` / `fn_External_SaveText` are
  member calls on the real classes (53 files, 141 `// B.3.3 bridge` casts, every guard on
  a no-longer-called pointer kept for the closing pass). NOT converted, by design:
  `fn_External_External` (113) and `fn_External_Dtor` (107) — `Object::External` is
  abstract and a real ctor/dtor chain would stamp GCC's vtable where live objects wear the
  minted one; they convert with their receivers' classes. `fn_ExternalList_Find` (4):
  `External.h` spells its second parameter `uint32_t`, retail's is `Object::Type` by value.
  Two `GetResourceCount` calls inside fprintf argument lists stay (perf build strips them).
  Gate: debug + perf build, unit 206/0, three routes exit 0, HelpTopics 66 / DiseaseList 12
  identical to the pre-merge boot, dispatch 0, stress clean quit (0.21 ms/iter, recorded).
  Resave gate: 8 PASS / 2 DIFF in 10 runs (0/9 DIFF on the commit before) — the known
  loader-race flake one step further, one extra `CropField.rsc:resource` row at count 5689;
  documented in `tools/resave_gate/README.md`, no semantic cause in the batch.
  Batches 6B/6C done (merge of `ad54c8a4` + one coordinator fix): 876 real call sites in
  225 files — `fn_InternalBasePtr_Ctor` 815 (placement-new of `::Object::InternalBasePtr`
  into the SAME raw storage, which is literally what the registered door runs; the class is
  static_asserted trivially destructible and copyable, so no second destructor exists),
  `fn_InternalBasePtr_DeletePtr` 47, `fn_Factory_GetTypeIdFromName` 10,
  `fn_Factory_CreateObject` 4. NOT converted, by design: all 61 `fn_ObjectInternal_Ctor` /
  `_Dtor` sites (the door stamps the MINTED vtables on purpose; every receiver is a derived
  exe POD — they convert with `Object::Component`), `fn_BaseTypeInfo_Ctor` (staging path).
  One `runtime/` edit, a real bug fix: `runtime/types/system/Value.h` no longer duplicates
  the global `struct Time` that `engine/types/common.h` owns — the redefinition is why no
  exe-side file could include `object/Internal.h` before (`mouse_inject.cpp`'s file-local
  `InputDevice()` renamed `InputDeviceSingleton()` as the one collateral).
  Coordinator fix: `buildrequirementui.cpp`'s new include had landed inside
  `#if defined(_DEBUG)` — debug built, the PERF build did not. All 245 step-6 includes
  scanned, that was the only one. (Agents must build `standalone-perf` too, not just
  `standalone`.) Gate: debug + release + perf build, unit 206/0, three routes exit 0, type
  table and list counts identical, `[RtResolve] 0 misses`, dispatch 0, resave 2 PASS + 1
  flake of the documented signature (count 5689, one extra row, this time
  `ToolbarRemoval.rsc:resource`), stress clean quit (0.21 ms/iter, recorded). The 6C
  `GetTypeIdFromName` sites in Options/NewGame dialogs have build coverage only on these
  routes.
  Batch 6D done (merge of `b3364b8e`): all 4,670 `InternalBasePtr::CheckPtr` calls deleted
  (3,373 through the macro, 1,297 through aliases/wrappers — 110 of those through wrappers
  whose names do not contain "CheckPtr"), 299 files, −5,463 lines. Retail's body is
  `C2 00 00` (`RET 0`) in the shipped AND the profile DLL and the exe has no import for it.
  Side-effect audit first: 1,302 distinct argument expressions, none with a side effect;
  1,402 `if (<pointer>) call;` statements deleted whole, none was the sole body of a
  loop/else; 11 comma-operator operands rewritten keeping the survivor. Gone with them: the
  macro (both forms), typedef, extern, `g_checkPtrIsNop` and its byte-sniff block, the
  `BANISHED_CHECKPTR_NOP` option, `-DBANISHED_CHECKPTR_STATIC_NOP`, the exe dispatch-slot
  row 0x140152ab0 (dispatch check now 109 rows, 0 mismatches). The runtime port of the
  empty member STAYS (it is the real class's ABI door; a unit test calls it).
  Gate: debug + release + perf (clean dir) build, unit 206/0, three routes exit 0, type
  table and list counts identical, no `[CheckPtr]` boot line, `[RtResolve] 0 misses`,
  resave PASS 3/3, stress clean quit (0.20 ms/iter, recorded).
  Step 7 done (the closing pass) — see the "B.3.3 CLOSED" block above for what it deleted,
  what it left and why.

- **B.3.1 CLOSED (2026-09-13 night):** `System::String`, `System::Filename` and the nine
  `Math` value types are real classes under retail's names (`runtime/types/system/*.h`,
  `runtime/types/math/Math.h`); three cores, five caller batches by directory, one closing
  pass. The C seam between callers and these types is gone: every `fn_String_*`,
  `fn_Filename_*`, `fn_Vector*`, `fn_Quaternion*`, `fn_Transform*`, `fn_Matrix*` pointer in
  `engine/funcs/engine_funcs.{h,cpp}` is deleted, `engine/types/SystemString.h` is deleted,
  and 36 zero-consumer `RtMath_*`/`RtSystem_*` shims went with their 44 registry rows.
  `standalone/filename_dtor_check.cpp` and `BANISHED_FNDTOR_CHECK` are gone with the
  `fn_Filename_Dtor` pointer they interposed (options.h is still the whole flag inventory).
  The 21 smallest Math members are now `inline` in Math.h (bodies moved verbatim).
  `make unit`: 154 tests, 0 failures. Routes, all exit 0, no crash/hang log: autostart +
  pause-menu quit, the Mods route, the tutorial route, the Load Game dialog.
  `tools/dispatch_slot_map.py --check`: 0 mismatches. `make sa-stress` reads sim ms/iter
  **+5.5% against the 2026-09-05 baseline**, of which +4.4% is already there with the
  inlining reverted — the inlining itself is inside the run-to-run spread.
  `reinterpret_cast` (ENDGAME §B.4 metric), start -> now:

  | | code | engine | runtime | standalone | dx11 |
  |---|---|---|---|---|---|
  | start (main 1d97579c) | 23,657 | 962 | 6,826 | 3,066 | 145 |
  | now | 23,784 | 906 | 6,788 | 3,055 | 148 |

  It moved barely at all, and `code/` went UP, because the conversions traded one cast
  shape for another: a `fn_String_Set(field, s)` call became
  `reinterpret_cast<System::String*>(field)->Set(s)`. Those are the B.3.1 bridge casts
  (`// B.3.1 bridge`), one per use of a POD member of a struct that is not a class yet;
  each disappears with that struct's own §B.2 conversion, not before.

  **Left, and why** — all of it belongs to a later §B.2 class conversion, not to B.3.1:
  - Bodies still in C shape over these types: `IO::FileSystem::ReadPackfile`,
    `WriteSettingsBody`, the RtSink-routed `runtime/src/save/saveload_update.cpp`, and
    `UI::WideString` (never converted; it is its own type, not System::String).
  - `RtSystem::String` / `RtSystem::Filename` PODs and `runtime/src/system/pod_bridge.h`
    stay while any struct still declares one as a MEMBER: `IO::FileSystem` (5 fields),
    `IO::PackageFile` (5), `IO::Persistence` (5), `Achievement::Achievement` (3),
    `Achievement::Goal` (1), `Engine::UpdateLoop`'s two loop subclasses (2),
    `IO::Registry` (1), `IO::Modification` (2, `Filename`), plus
    `runtime/types/object/External.h`'s `using RtSystem::Filename`.
  - Shims still reached by name or address from an unconverted caller stay: String's
    Set/Dtor/Assign/SubString/Format/Serialize/InsertAt/RemoveAt/operator+/StrCompare,
    Filename's ctor/dtor/assign/compare family, and the Vector/Quaternion/Transform
    members the graphics bodies call.
  - The global aliases (`Scalar`, `Quaternion`, `Matrix`, `Transform`, `Transform2`,
    `Transform3` in Math.h; `Vector2/3/4` in engine/types/common.h) all still have
    hundreds of users and stay. `RtMath::` is already gone.

- **CLOSED (2026-09-13):** the stress-town teardown crash/hang was `FishingWork::OnCancel`
  ported with three parameters where retail forwards a fourth (the packed slot word) by
  register passthrough — Slot6 then stored a `2` and a null past two 4-entry arrays on every
  cancelled fishing action. One-line fix in `code/Game/Work/fishingwork.cpp`; root cause,
  measurement and what the earlier analysis got wrong: `TEARDOWN_WILD_WRITE.md` §0. Verified:
  debug exe reproduced 1/1 before the fix with the writer named, 0/2 after (both teardown
  brackets clean, clean exit). The loader race in `ExternalFactory::Add` was not reached.

The plan for what follows the gate is `ENDGAME.md`: A remove the DLL, C remove the flags,
B fold the C bodies into real classes, in that order. **2026-09-13: A.1 and A.2 are done.**
Tag `dll-present-last` (commit `86d28a73`) is the last commit where the exe still loaded
Runtime-x64.dll; the exe never loads it now. `make sa-run` is the DLL-absent route and
`make cutover` is kept as an alias for it.
- The flip exposed one bug the old gate could never see: `Video::Device::InitializeInterfaces`
  still `LoadLibraryW`'d the video plugins, and VideoDX9-x64.dll statically imports
  Runtime-x64.dll, so the DLL was mapped behind `ResolveInit`'s back and its first heap call
  faulted. The move-aside had made that LoadLibrary fail for the wrong reason. The loop now
  loads nothing (`runtime/src/video/video_interfaces_win.cpp`); `ResolveInit` fails fast if the
  DLL is ever found mapped. Verified 2026-09-13 with the DLL on disk: sa-run + pause-menu
  quit exit 0, `BANISHED_AUTOQUIT`, and the resave gate PASS.

- **2026-09-13 later: A.3, A.4 and C.1 are done** (commit `5084c732` on `endgame-a3`; five
  parallel worktree batches by directory plus one serial pass over the shared definitions;
  493 files, +1.9k/-83k lines). Gone: the raw door (`RtOriginal`, `RT_ORIGINAL_ONCE`,
  `ResolveExportRaw/OrdinalRaw/RuntimeRaw`, the ordinal door), the diff oracle (`rt_diff`),
  the predictive oracle (`rt_predict`; `RtSink` survives as `runtime/funcs/rt_sink.h`, the live
  sink the save/ui bodies are written against), `io/save_trace`, 46 `*_probe.cpp`, the alloc
  and mint shadows, the 11 detour/hook installers and `hde/`, the byte-pins and `*_sweep`
  make targets, `tools/attic`, and every `BANISHED_RT_*` read (`runtime/` getenv 133 -> 7).
  `allocator_lock.cpp` (a byte patch on the DLL's Allocate/Free) is reduced to
  `standalone/alloc_stats.cpp`.
  - **Five live dependencies on the raw door were found**, each a call through NULL since
    A.1 that no gate had caught: `Graphics::Geometry` copy ctor in `graphics/mesh.cpp`
    (every MeshInstance / EdgeMeshInstance copy), `MemFree` in `ui/sprite.cpp`'s
    `SpriteSheetAssignString` (every refcount-to-zero string release), `MemAlloc/MemFree`
    in `system/string.cpp` and `object/internal.cpp` (raw first, registry fallback), and
    `_beginthreadex` in `system/thread_win.cpp`. All folded to the registry door.
  - A.5 routes, DLL-absent, all exit 0 with an empty crash log: `make sa-run` +
    pause-menu quit (town built, App::Quit), `BANISHED_AUTOQUIT`, the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2`), the Mods route (0 rows, no .pkm installed), and the
    resave gate PASS (byte-identical to the golden, same as the baseline on `c1fdd7f6`).
  - **The first `make sa-stress` died in boot with a VCALL-of-0** on `GraphicsMesh`'s minted
    vtable: `?PlatformSerialize@GraphicsMesh@` was unregistered in the PERF build only.
    Cause (pre-existing, 2026-09-12): seven registrations were written as
    `printf("... %d", RtXxxRegister())`, and the perf build compiles `printf` to `((void)0)`
    (`standalone/log_strip.h`), arguments included. Batch C graphics/achievement/audio/video
    leaves and three math rows never registered in any perf exe. Hoisted; sweep with
    `grep -rnPzo '(?s)printf\s*\([^;]*?Rt[A-Za-z0-9_]*Register\s*\(\)[^;]*?\);'` (0).
  - **Second perf crash, also perf-only and also pre-existing:** with the leaves registered,
    `Renderer::DrawItemList` faulted in `Context::SetTransform` reading the matrix the
    geometry's vtable slot 6 (`GetInstanceTransform`, returns `Math::Matrix` BY VALUE) had
    returned. Our `ParticleInstance::GetInstanceTransform` port (wave27_lane_gfxpart.cpp) was
    declared `void`: the hidden return slot arrives in RDX and its address must go back in
    RAX; at -O0 RAX happened to still hold it, at -O2 it was garbage. Now returns the slot.
    Audit over all 175 registered by-value-returning exports: no other port declared void
    (address-only registrations not covered). Same debug-exe town ran 1200 frames clean.
  - `make sa-stress` on the perf exe then completes 6000 frames with a clean quit. Sim cost
    0.202 ms/iteration vs the 2026-09-05 DLL-present baseline's 0.192 (+5.0%, `ph_postsim`
    0.069 -> 0.097 ms/frame is most of it); a second run with a compile going on beside it
    read +10.3%, so the run-to-run noise on this box is of the same size as the delta. The
    perf gate was deferred to after cutover by the user (Wave 4); recorded, not chased here.
    A clean A/B needs the machine idle and the baseline re-cut on this build.
  - The debug exe's stress town (`sa-stress-debug`, 1200 frames) simulates clean but CRASHES
    IN TEARDOWN after the quit click: READ fault inside `rtalloc::Free` from `MemFree` in
    `ComponentEntity_Dtor` <- `InternalBasePtr::DeletePtr` <- `EntityList_RemoveEntity` <-
    `EntityList_Shutdown` <- `GameUpdateLoop_Shutdown`. The normal-size town's pause-menu
    Quit exits 0; this is the teardown defect already listed above (seen once on 09-13
    morning), now reproducible with the 800-citizen town on the debug exe.
- **Serializer gate exists and passes**: `tools/resave_gate/resave_gate.sh Gate` (README
  there). Load a save DLL-absent, serialize the restored world again before the first tick,
  compare the raw bytes against a golden. Golden cut against `Gate.sav`
  md5 `68f99e06e6cdce6c17a8fb06e867b2c8`; two pinned runs byte-identical.
- Its first run found the last DLL-absent SAVE blocker: `CompressedStream::SetWindow`'s
  write-side grow path was still delegated to the DLL (the cutover gate never saved).
  Ported; the DLL-absent autosave now completes.
- Seen once each, pre-existing, not fixed: a loader race crash in `ExternalFactory::Add`
  (AVL rotate on a null node, `[GUL::Load] redundant dispatch` just before) and a hang in
  teardown after the pause-menu Quit returned.

- **2026-09-13: C.3 — the options struct exists and `standalone/` is at zero `getenv`.**
  `standalone/options.{h,cpp}`: one `Options` struct, filled once by `OptionsInit()` at the
  top of `WinMain` (before `HarnessGuardUnattendedBoot`, before any other reader), read
  everywhere else as `Opt().field`. Every field carries a comment naming its reader — a
  Makefile target, a `tools/` script, `perf/`, or a route documented in CLAUDE.md — and the
  ones nothing outside the tree sets are marked `[no tool reader]`, which is the C.4 delete
  list ready-made. The header also declares the tier-3 fields the `code/` sites will move
  onto (world setup, the stress/perf knobs, the `BUILD_DEMO_*` founding-town flags,
  `MENU_SCENE`, `SAVE_TEST`, `MODAL_ANSWER`), so it is the whole inventory, not just
  standalone's half.
  - **Both mid-run environment WRITES are gone.** `stress_harness.cpp`'s `SetIfUnset` bundle
    (autostart, map/seed/difficulty/climate, camera pin, the demo buildings, the in-game
    quit) is now `ApplyStressDefaults` inside `OptionsInit`, and
    `construction_force_harness.cpp`'s `_putenv_s("BANISHED_BUILD_SYNC_DELIVERED")` — which
    raced `GatherRawMaterials::Execute`'s own cached read — is the derived field
    `Options::buildSyncDelivered`. `OptionsInit` still `_putenv_s`es the stress bundle for
    the ~600 not-yet-migrated `code/` readers; that half goes with them, and `env_cache.h`
    stays until then.
  - **Deleted with their flag** (no reader in Makefile / tools / perf / .vscode / CLAUDE.md /
    STATUS.md): `BANISHED_W17_ATTEND` and all of `standalone/wave17_door_attend.cpp` (the
    Wave 17 door-attendance census, a per-`ResolveExport` strcmp loop),
    `BANISHED_MOUSE_DRAGSKEW` (existed only to skew what the A.4-deleted diff oracle could
    see), `BANISHED_AUTOSAVE_TRACE`, `BANISHED_LANE4_TRACE`, `BANISHED_W2LANE3_TRACE` (wave
    traces), and a dead `EnvOn` in `rt_data_mint.cpp`. Nothing else was deleted: the
    gameplay harnesses are wired into `code/Game/Game/gameupdateloop.cpp`, `starttown.cpp`
    and a dozen components, so retiring them belongs with the `code/` pass, not this one.
  - Verified: `standalone-debug` and `standalone-perf` both build clean, and all three
    routes exit 0 with an empty `standalone_crash.log` — `sa-run` with
    `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` (town built, pause menu opened at
    frame 601, Quit clicked at 721), `BANISHED_AUTOQUIT`, and the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2 BANISHED_AUTOMENU_CLICKS=m6`).

- **2026-09-13: C.2 and C.4 are closed — the whole tree is at zero `getenv` and there are
  no live-feature gates left.** Four directory batches plus a closing pass.
  - **Gates:** the 38 remaining `kDefaultOnFlags` entries, `LiveEnabled`, `EnvOn` and the
    `BANISHED_LIVE_COMPONENTS` master read are deleted; the `EnvVar` parameter is gone from
    every `COMPONENT_LIVE_FACTORY*` / `CONTROLLER_LIVE_FACTORY*` macro and its 14
    invocations. The twelve flags the batches left undecided were proven in one forced-on
    run (all twelve set: `sa-run` + pause-menu quit exit 0, empty crash log, resave gate
    PASS twice, byte-identical to the golden) and graduated: `LIVE_AUTOSAVE`,
    `LIVE_BUILD_ONREMOVE`, `LIVE_CEMETERY`, `LIVE_CITIZEN_FAMILY`, `LIVE_CROPDISEASE`,
    `LIVE_FOUNDING_SPAWN`, `LIVE_HAPPINESS`, `LIVE_HEALER`, `LIVE_MARKETLOC`,
    `LIVE_TOMBSTONE`, plus `CLEAR_FOOTPRINT` and `LIVE_ANIMALS`, whose readers the batches
    had already removed. Graduating `LIVE_FOUNDING_SPAWN` deleted its off arm: the
    hand-rolled `SpawnOneCitizen` family loop in `starttown.cpp` and its four helpers
    (~230 lines) are gone, so the retail `sub_1040` port is now the only founding spawner.
    Three standalone opt-ins that were OFF on every default run went with their flags
    (`LIVE_CITIZENAI_TIDS` / `_HAPPYID` / `_AIID`: the `c42c0` and `c70e8` type-id fills).
  - **`getenv` is zero** in `code/`, `runtime/`, `dx11/`, `engine/` and `standalone/` — the
    only call sites left are in `standalone/options.cpp`. 69 reads moved onto `Opt()`
    (code 66, dx11 1, engine 2), with ~40 new typed/raw fields in `options.h`.
    `standalone/env_cache.{h,cpp}` and the `-include standalone/env_cache.h` in the
    Makefile are deleted, and `ApplyStressDefaults` no longer writes the environment: it
    sets the struct fields, so `OptionsInit` is now read-only with respect to the CRT
    environment block.
  - **Shims deleted with their call sites:** `MapQuery_FloorTraceOn` (+10 guarded traces),
    the empty `StartTown_DriveProduction` (and `production_harness.cpp`),
    `WorkPlaceComponent_ControllerLive`, `ObjAudit::Enabled` and the diagnostic half of
    `internal_factory_audit` (`Scan`, `ReportLeaks`, `NoteUnregisteredFree`;
    `IsRegistered`/`Count`/`Factory` stay — they are load-bearing on the free path), and
    the six empty probe stubs `WorkSchedProbe_Report`, `DumpSlot3Inventory`,
    `ModProbe_DumpModList`, `ModProbe_StartWatcher`, `TradeUI_PriceProbeReport`,
    `TradeUI_SortSelfTest`. `Options::buildSyncDelivered` lost its last reader and went too.
  - Verified on both exes: `standalone-debug` and `standalone-perf` build with exit 0 and no
    new warnings, and every route exits 0 with an empty `standalone_crash.log` — `sa-run` +
    pause-menu quit, `BANISHED_AUTOQUIT`, the Load Game dialog
    (`BANISHED_AUTOMENU_BUTTON=2 BANISHED_AUTOMENU_CLICKS=m6`), and `make sa-stress` on the
    perf exe (6000 frames, clean self-quit).

- **2026-09-13: frozen-exe detector.** The boot-state watchdog now carries the main loop's
  frame counter; a stalled counter publishes `STATE=HUNG` (exit 5 from `tools/boot_state.sh`),
  writes `standalone_hang.log` with all-thread stacks, and on unattended runs terminates the
  process with exit code 4 — so a hang costs ~30 s instead of the caller's whole timeout.

## Cutover gate

`make sa-run` (`make cutover` is the same thing under its legacy name): deploys the debug
exe and boots it — the DLL is never loaded, so there is nothing left to move aside. Run it
under `tools/game_dir_lock.sh`. Symbolize frames with
`addr2line -f -C -e standalone/build/reimpl-x64.exe 0x14xxxxxxx`. The history below (walls,
"DLL-absent" language) predates A.1 and describes the old DLL-present/DLL-absent procedure,
recoverable at the `dll-present-last` tag.

Walls passed 2026-09-12: the Video shader family → the UI Description/Element family (53
classes) → `Font::Serialize`'s two by-address callees → (evening walk, one session, serial)
the two unminted `IO::Encoding` tables (Visitor 0x1801559f8, Text 0x1801559d0; both minted
by address, ten slot bodies in `io/encoding_visitor_text.cpp`) → the two unminted
`GraphicsMesh::Serialize` statics (0x1801fc69c/0x1801fc6a0, `EXTRA_STORAGE_BY_ADDR`) → a HANG,
not a crash: `AnimationGroup::Serialize` emitted the +0x30 float BEFORE the tag array (retail:
after), so the tag count came out of the float's bytes and `CompressedStream::Read` spun on a
zero-length chunk past EOF (found with `gdb -p`, not the crash log) → **the gate then booted to
the in-game world** (24 citizens, 21k entities at frame 7200) → the quit path: eleven scalar
destructors the `RT_DELDTOR` table still resolved to NULL (`object/deleting_dtor_leaves.cpp`:
SoundEffect, MaterialInstance, AnimationGroup, ToolTipDialog, ReaderWriter, Light, SunLight,
AchievementInstance, Base, Stream, ParticleInstance) → one `RT_ORIGINAL_ONCE` on a live path
(`video/buffers.cpp` MemFree) → **`make cutover` with `BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` exits 0 with an empty crash log and the DLL absent.**

**The gate's "done" condition is met: boot to the world, quit cleanly, DLL absent.** Then the
render walk (same evening): the user saw a black window. RenderDoc on a DLL-absent frame 150
showed 6 draws (DLL-present: 453); `Renderer::AddDrawItem` dropped every item on its
"VideoProgram+0x20 null" arm because every `MeshLayout\*.rsc` load failed External::Load's
version check (`[ExtLoad] badVer=65`, 0 DLL-present): `VertexLayout::GetFileVersion` returned
1, retail returns 3. Fixed; the DLL-absent frame is now 460 actions like DLL-present, and the
menu, title and terrain draw. **The three render defects from the side-by-side captures are fixed (2026-09-12 late):**
- no building or tree meshes: `MeshInstance::Update` reads two UNEXPORTED `.data` statics by
  address (guard 0x1801ff30c, 48-entry identity Transform table 0x1801ff310) and declined the
  whole update when they resolved NULL. Both minted (`EXTRA_STORAGE_BY_ADDR`). The tell was one
  `[SLOTS29] ... declining` line in the DLL-absent log that the DLL-present log did not have.
- first glyph only, and the wrong glyph: `Font::ComputeLineLayout` stored the codepoint in the
  layout entry where retail stores the GLYPH index (0x180116d99, EDX from the table load at
  0x180116d1a). AddFont uses it as the sheet sprite index; the sheet has 96 sprites.
- the grey rectangle went away with the glyph fix (a label's measured width, presumably).
Menu (town, market, text) and the in-game world (trees, rocks, shadows, toolbar) now look
right DLL-absent by screenshot (`tools/capture_window.ps1`). **The DLL-present build never
enters the ported text path** (a trace in our `AddFont` fired 0 times with the DLL present),
so a text-path port can only be seen DLL-absent; assume the same of any path a DLL-internal
caller reaches.
Method that worked twice tonight and should be the default for render walks: capture under
RenderDoc DLL-absent, look at the frame overview and one draw's state, then put a
count-limited `fprintf` on the silent-drop arms of the body the numbers point at, one gate
run, remove the trace. Also audited: the 19 `GetFileVersion` ports written as a constant all
match the DLL bytes now; ports that return through a variable were not covered.
Raw-door triage (2026-09-12 late): `BANISHED_RT_STRICT=1` on the gate prints `[RtRawResidue]`,
the raw-door requests the run actually made. 42 DLL-absent (each once). All but one are oracles,
probes, byte-pins, hook installers or `if (!g_portOn)` fall-throughs that degrade to our body or
print "not checked"; the one live dependency was `userinterface_hover.cpp`'s string release in
BeginToolTip (MemFree through the raw door), now the registry door. The other 600-odd sites were
not reached by this route; re-run the strict gate on any new route and read the table.
Load Game route, DLL-absent (2026-09-12, `BANISHED_AUTOMENU_BUTTON=2`): two defects, both fixed.
- EXEC-of-0 in `SaveLoad::UpdateList` state 1: the save-list sort calls the DLL's own CRT
  `qsort` by ADDRESS (0x18007f050) and every lane before had filed it "CRT, out of scope";
  the closure report still counts it among the CRT rows "our CRT already provides", but no
  body sat behind the retail address. Ported with `shortsort` (0x18007f340) in
  `runtime/src/crt/msvc_qsort.cpp` (the MSVC algorithm, not `::qsort`, because the
  comparator is not a strict weak order and the algorithm decides the tie order).
- Load/Cancel/Delete greyed: `UI::Element::Enable`'s port had the argument sense inverted,
  under a banner claiming the inversion was retail's. The DLL-present build never runs that
  body outside the diff oracle (dialogs reach slot 8 through the DLL's vtables), so the
  screenshot pair present/absent was the first thing to show it. Same lesson as the text path
  above: a body a DLL-internal caller reaches can only be seen DLL-absent.
The dialog now matches the DLL-present capture. Clicking Load in that dialog (the actual
save load, DLL-absent) has not been driven yet; there is no unattended click into
SaveLoadDialog.
Pressing Load (attended, DLL-absent, same evening): the save deserialized up to
`InternalBasePtr::Serialize`, whose PersistObject door was `RtOriginal` (null with no DLL);
now the registry door. After that the save loads and plays.
Picking parity, DLL-absent vs DLL-present (2026-09-12, reported as "click-to-highlight a
touch off"): a seeded, camera-pinned new game (`BANISHED_SEED=12345 BANISHED_CAMERA_PIN=town
BANISHED_SELECT_DIAG=1`) with the same 54 synthetic clicks (`SetCursorPos` + `mouse_event` at
fixed client coordinates) in both builds gave IDENTICAL terrain hit positions, cells, picking
mesh ids and selected entities on every click, and the same house highlighted. Also checked
against retail bytes: `Frustum::GetScreenDirection`, `Cursor::Serialize`, the cursor block of
`UserInterface::Draw`, `PickingComponent::OnAdded`'s AddMesh transform; window/client/backbuffer
sizes are equal in both logs. Whatever the offset is, it is not DLL-absent-specific; the
cursor is the OS cursor in both builds (the sprite-cursor arm of `UserInterface::Draw` is not
taken at the menu). Next step if it persists: the same click grid against retail's own
Application-x64.exe on one shared save.

## DLL-present build

`make sa-run` boots to the in-game world and pause-menu Quit works (re-verified 2026-09-12
at close, 975 UI exports registered, 135 deleting-dtor rows: `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120`, exit 0, empty crash log). Ten divergent sites, all the
pre-existing negative controls and vtable-identity artefacts (the `AnimationEvent()` ctor
oracle still trips the released-pointer detector; make it NO-ORACLE when someone is there).
The two that appeared when the doors were unified were harness artefacts, resolved 2026-09-12
late: `SetupShaders` compared four padding bytes of an uninitialised snapshot; `Flush`
compared the live dirty mask AFTER the real draw had consumed it. Both now 0.
Fixed the same evening, DLL-present: hovering any dialog moved it (the pause menu followed the
cursor) — `DialogInstance::Update` wrote the tooltip-settle mouse position into `_upperLeftX/Y`
(+0x88) where retail uses +0x90/+0x94; found with a hardware write watchpoint from the
pause-menu harness plus a scripted mouse (`tools/capture_window.ps1` to look).
Drag-tool flashing (2026-09-12 late, both builds): `DialogInstance::Update`'s port read
Element flags bit2 (`NoMouseCapture`, what `Dialog/CreateBar.rsc` sets) as "always hit"; retail
(0x180113b9a) skips the capture block on it. The cursor-following cost panel captured the mouse
every frame, UserInterface+0xd9 went to 1, the tool tick passed OnKey key=0, OnKey closed the
panel, the next frame reopened it. Fixed from the bytes; the autostart + pause-menu-quit route
still exits 0. Not yet re-driven with an attended drag.
The unattended Mods route (`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,
4001,101,m6 BANISHED_MODAL_ANSWER=1`) runs to Quit, exit 0. It needs at least two `.pkm` in
`WinData/` (both live in `WinData/_mods_off/` because the achievements harness needs them
out); with none, the driver now skips per-row tokens with a `[AutoStart] WARN`. ToggleMod
was also wrong (read the model and inverted; retail pushes the checkbox's own state).

## Held back, with reasons

- `System::MemoryAllocator` (7 bodies) armed only under `BANISHED_RT_ALLOC`, which
  `BANISHED_RT_NO_DLL` now implies. Needs a DLL-present A/B before it goes default-on there.
- Perf gate breached +7.7% in Wave 4; the user deferred it to after cutover.
- `VertexLayout::Add` / `GetStreamCount` / `GetElement*` are non-virtual and unported.
- Ports reached only through the DLL's own vtables (every `UI::*Description` / `*Element`
  body, the ShaderProgram family, Audio::Stream::Run) show no oracle attendance DLL-present;
  only the gate exercises them. The DLL-present run proves link/register/boot, nothing more.
- `deleting_dtor.cpp` row indices are table positions (135 rows now). Every parallel batch
  collided on them; append after the last row and re-count before registering.
- `tools/rt_deleting_dtor_verify.py` flags the two `Object::*Compiler` rows BAD (their dtors
  restamp two vtables; the reader knows one) and eight rows UNRECOGNISED (bare tail-jump
  dtors). None is a port defect; each is explained at its row.
- Two mesh-code lines reached the DLL through `RT_ORIGINAL_ONCE`, the raw door that skips
  the registry, and silently bypassed two ported bodies until 2026-09-12. A bypassed port
  never runs, so the oracle cannot see it. Grep for that macro before trusting attendance.
