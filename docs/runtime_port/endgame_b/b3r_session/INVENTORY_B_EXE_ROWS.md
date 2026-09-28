# Inventory B — EXE_ROWS (NEXT_SESSION.md §3b question 1, the exe half)

Table: `data/B_exe_rows.tsv` (778 rows, one per exe address that is registered OR read).
Script: `scripts/B_exe_rows.py` (re-runs from any directory, no arguments, ~7 s; reads the
retail exe's PE import and section tables through `pefile`, read-only, like
`tools/dispatch_slot_map.py`).

## 1. The answer

- **760 exe addresses are registered** (765 registration sites: 688 literal `RegisterExeAddr`, 1 named
  `BuildRoadAddr::CtorParams`, 51 slot-cell wrappers, 25 rows of the `.rdata` float table; 5 addresses
  registered twice). **18 more are read and never registered** (they resolve to 0 through the
  `NOT IMPLEMENTED` path).
- **Classes: DIRECT 635 · STORAGE 63 · DEAD 45 · CALLBACK 17 · STUB 18** (all 18 STUB are the
  unregistered ones; no registered row points at a stub body).
- **2,375 reader occurrences**: 2,146 `ResolveAddr(Module::Exe, 0x…)` literal lines + 212 calls of 16
  file-local helpers + 17 table/switch rows. 48 of them sit in functions nothing reaches (dead readers).
- **The split that decides the plan: exe FUNCTION addresses dominate.** 1,961 occurrences (650 addresses)
  are exe functions, 252 (69) are `.rdata`/`.data` cells, and only **162 (59) are PE import slots**
  (Runtime symbols). Import slots are 6.8 % of the readers, not the majority.
- DIRECT exe_fn rows: 588 rows → 584 distinct C++ bodies, all extern-linkage free functions defined in
  `code/Game/*`; **547 of them are declared in no header** (their only prototypes are `extern` lines in
  `standalone/reg_*.cpp` and the readers' `fn_…_t` typedefs). 526 have a reader outside the defining file.

## 2. How the table was built

Code lines only: every file under `code/ engine/ runtime/ standalone/ dx11/ tests/` (excluding
`standalone/build*`, `tests/unit/build`, `.claude/worktrees`, `/build/`, `/build-`) is passed through
`b3x_session/codegrep.strip` (comments removed) and then string contents are blanked, so a provenance
comment, a printf or a static_assert string is never a hit.

1. **Every `0x14xxxxxxx` literal** is found, and every use of a named address
   (`constexpr/const uintptr_t NAME = 0x14…`; a namespaced one only when spelled `Ns::NAME`, a
   namespace-less one only in its own file). 264 named addresses exist, **2 are used** (below).
2. Each occurrence is classified by the innermost call whose parentheses hold it (backward paren scan,
   argument index checked):
   - registration: `RegisterExeAddr` (arg 0), `RegisterDispatchSlot` (two definitions:
     `address_registry.cpp:762` file-static, `reg_unreg_lane1.cpp:75` anon-namespace),
     `RegisterLane3Slot` (`reg_unreg_lane3.cpp:35`, skips a null value), `RegisterPlacePlayerSlot`
     (`reg_player_place.cpp:37`), and the `g_consts[]` table looped by `RegisterRdataFloatConsts`
     (`reg_rdata_float_consts.cpp`). The four slot wrappers store the VALUE in a map and register the
     ADDRESS of the map cell. `RegisterExeAddrs`/`ExeAddrEntry` have **no caller**.
   - reader: `ResolveAddr(Module::Exe, …)`; a helper call (below); a table/switch row.
   - neither: image-base arithmetic (`redzone_alloc.cpp`, `sampler.cpp`, one diagnostic in
     `citizen_assignworkplace.cpp:386`), the `g_modules` base, `tests/unit/alloc_door_test.cpp`'s fake.
3. **Helper spellings found** (each read by hand; every one ends in `ResolveAddr(Module::Exe, param)`):
   `R<T>` (9 files, 125 calls), `slot` lambda (7 files, 18), `ResolveSlot` (4 files, 17 — in
   `serialize_struct60b_serialize.cpp` it takes `(addr, cache, tried)`), `code` lambda (pickup.cpp, 14),
   `Slot` (boatcomponent_states.cpp, alloc_door.cpp; 12), `Flt` (7), `RF::Get` (quarry, 6; writes only
   when non-null), `RSlot` (3 files, 4), `resolveIAT` (edittool, placementtool; 4), `RExe` (2),
   `SlotFn` (keystone_a_removalcalcpath, 2), `ReadDispatchSlot` (1). Not on the brief's list:
   `code`, `Flt`, `RF::Get`, `RExe`, `SlotFn`, `ReadDispatchSlot`, `resolveIAT`, the named constant
   `kSlot_Vector2Ser` passed to `geometry_serialize.cpp`'s `ResolveSlot`, and the two data spellings
   (§4). Three helper definitions have **no caller at all**: `SlotFn<T>` in `boatcomponent_route.cpp:87`,
   `ResolveSlotPtr<T>` in `tornadocomponent.cpp:156`, `RSlot` in `diseasecomponent.cpp:147` — each is one
   of the 39 non-literal `ResolveAddr(Module::Exe` lines. `ReadGlobal`/`WriteGlobal` have no caller.
   `loadExref`, `ResolveFn<T>`, `RtAddr`, `RSlot` with `Module::Runtime` etc. do not touch exe addresses.
4. **Liveness ("is the reader itself still called?")** — a reachability pass over every function body in
   the tree (13,961 bodies): roots are members (`Class::M`, reached by vtable/header), `main`/`WinMain`,
   and any function named on a namespace-scope line that is not a prototype (tables, initialisers);
   an edge runs from a body to each function it names; a same-name file-local function binds to its own
   file. A reader is live when its function is reachable AND, if the statement stores into a
   namespace-scope pointer (`g_x = …ResolveAddr(…)`, the engine_funcs initialiser binding, 177 of the
   195 engine_funcs.cpp readers), that pointer is named again by a live function. 415 bodies are
   unreachable tree-wide.
5. **Class rules**: no registration → STUB; no live reader → DEAD; a reader that is a table/switch row
   → CALLBACK; target an `.rdata`/`.data` cell or a data import (`?_singleton@…`) → STORAGE;
   otherwise DIRECT. `addr_kind` comes from the PE: `import_slot` = an import-table cell,
   `exe_fn` = `.text`, `data` = `.rdata` (non-import) / `.data`.

Columns: `key_addr, addr_kind, runtime_symbol_if_slot, reg_file, reg_line, target` (the registered value
as written; `ResolveExport:<name>` for a slot cell), `readers` (occurrences), `reader_files` (≤8, then
`(+n)`), `class`, `direct_replacement` (DIRECT: the C++ name and its header, or `no declaration exists
yet [defined <file>]`; import slots: the demangled Runtime member and its `runtime/types` header;
STORAGE: the storage object; CALLBACK: the target), then five extra: `n_regs`, `live_readers`, `doors`,
`reader_funcs` (`func>global`, `[unref]` = dead), `reg_fn` (the Register function holding the row; all
are reachable).

## 3. Counts

Rows by class and registration directory:

| reg dir | DEAD | DIRECT | STORAGE | STUB | CALLBACK | total |
|---|---:|---:|---:|---:|---:|---:|
| standalone (address_registry.cpp + 121 reg_*.cpp) | 24 | 598 | 61 | 0 | 17 | 700 |
| code/Game/Game (10 files) | 21 | 18 | 2 | 0 | 0 | 41 |
| code/Game/Component (6 files) | 0 | 11 | 0 | 0 | 0 | 11 |
| code/Game/Dialog (5) / Player (1) / Tutorial (1) / Work (1) | 0 | 8 | 0 | 0 | 0 | 8 |
| unregistered (read only) | 0 | 0 | 0 | 18 | 0 | 18 |
| **total** | **45** | **635** | **63** | **18** | **17** | **778** |

By address kind: DIRECT 588 exe_fn + 47 import_slot; STORAGE 62 data + 1 import slot
(`?_singleton@Configuration@Input@@`, 0x140151738); DEAD 38 exe_fn + 4 import + 3 data; STUB 7 exe_fn +
7 import + 4 data; CALLBACK 17 exe_fn. DIRECT import slots by Runtime namespace: Math 19, System 8, IO 8,
Object 6, Graphics 4, Volume 2.

Readers per directory (`RA` = `ResolveAddr(Module::Exe, 0x…)` code lines; `helper` = helper-call and
table lines; `addrs` = distinct addresses slot/fn/data; the last two columns split the OCCURRENCES by
kind, RA door then helper door):

| dir | RA lines | helper | addrs slot/fn/data | RA occ slot/fn/data | helper occ slot/fn/data |
|---|---:|---:|---|---|---|
| code/Game/Component | 883 | 126 | 26/289/48 | 50/710/123 | 21/89/19 |
| code/Game/Work | 538 | 16 | 11/132/14 | 36/462/40 | 2/14/0 |
| code/Game/Action | 234 | 36 | 8/137/27 | 3/190/41 | 6/25/5 |
| engine (engine_funcs.cpp only) | 195 | 0 | 8/162/6 | 8/179/8 | 0/0/0 |
| code/Game/Player | 138 | 17 | 12/82/2 | 1/134/3 | 17/0/0 |
| code/Game/Game | 89 | 26 | 10/83/7 | 1/81/7 | 10/16/0 |
| code/Game/Tutorial | 23 | 3 | 0/20/0 | 0/23/0 | 0/3/0 |
| standalone (harnesses, redzone_alloc) | 22 | 0 | 3/11/2 | 3/17/2 | 0/0/0 |
| code/Game/Dialog | 14 | 0 | 2/11/0 | 2/12/0 | 0/0/0 |
| code/Game/Entity | 8 | 0 | 0/6/2 | 0/6/2 | 0/0/0 |
| code/Game/Goal | 2 | 0 | 0/0/2 | 0/0/2 | 0/0/0 |
| runtime/src/collection (alloc_door.cpp) | 0 | 2 | 2/0/0 | 0/0/0 | 2/0/0 |
| **total** | **2,146** | **229** | 59/650/69 (tree) | 104/1,814/228 | 58/147/24 |

2,146 + the 39 non-literal helper-body lines = the 2,185 of §3a exactly. dx11/, runtime/ (other than
alloc_door) and tests/ read no exe address.

Reader occurrences by the class of the row they read: DIRECT 2,048 · STORAGE 227 · CALLBACK 29 ·
STUB 23 · dead readers 48 (Component 29, Action 7, Work 7, Player 4, Game 1).

The 36 `X_ResolveFuncs` (all reachable; 138 call sites plus 24 prototypes; all in Action/Work):

| resolver | file:line | exe addrs | calls | caller files |
|---|---|---:|---:|---:|
| AnimateNaturalResource | Action/animatenaturalresource.cpp:44 | 3 | 5 | 1 |
| CompleteRoad | Action/completeroad.cpp:36 | 2 | 4 | 1 |
| CompleteTask | Action/completetask.cpp:49 | 4 | 6 | 1 |
| ConsumeRawMaterial | Action/consumerawmaterial.cpp:66 | 3 | 6 | 1 |
| CreateNaturalResource | Action/createnaturalresource.cpp:71 | 12 | 5 | 1 |
| CreateRawMaterial | Action/createrawmaterial.cpp:84 | 8 | 5 | 1 |
| DropOffRawMaterial | Action/dropoffrawmaterial.cpp:74 | 2 | 6 | 1 |
| HuntAnimal | Action/huntanimal.cpp:111 | 11 | 5 | 1 |
| MoveTo | Action/moveto.cpp:120 | 12 | 9 | 1 |
| PerformWork | Action/performwork.cpp:91 | 8 | 8 | 1 |
| PickupRawMaterial | Action/pickuprawmaterial.cpp:71 | 3 | 4 | 1 |
| PickupStoredRawMaterial | Action/pickupstoredrawmaterial.cpp:67 | 3 | 6 | 1 |
| PlayAnimation | Action/playanimation.cpp:42 | 3 | 6 | 1 |
| RemoveAnimal | Action/removeanimal.cpp:67 | 3 | 6 | 1 |
| RemoveNaturalResource | Action/removenaturalresource.cpp:73 | 3 | 6 | 1 |
| SendComponentMessage | Action/sendcomponentmessage.cpp:39 | 1 | 5 | 1 |
| ShowTool | Action/showtool.cpp:91 | 2 | 6 | 1 |
| ConsumeProduce | Work/consumeproduce.cpp:105 | 9 | 2 | 2 |
| FireWork | Work/firework.cpp:100 | 8 | 2 | 1 |
| FishingWork | Work/fishingwork.cpp:100 | 9 | 1 | 1 |
| ForesterWork | Work/foresterwork.cpp:159 | 35 | 2 | 1 |
| GatherRawMaterials | Work/gatherrawmaterials.cpp:157 | 21 | 3 | 1 |
| HarvestArea | Work/harvestarea.cpp:142 | 19 | 2 | 2 |
| HarvestOrchard | Work/harvestorchard.cpp:122 | 21 | 2 | 2 |
| HerbalistWork | Work/herbalistwork.cpp:139 | 28 | 1 | 1 |
| HuntWork | Work/huntwork.cpp:133 | 25 | 1 | 1 |
| LocationWork | Work/locationwork.cpp:108 | 11 | 3 | 2 |
| MarketBst_b290 | Work/marketwork_bst_b290.cpp:79 | 0 | 2 | 1 |
| MarketWork | Work/marketwork.cpp:125 | 10 | 1 | 1 |
| MarketWork_Slot4 | Work/marketwork.cpp:616 | 23 | 1 | 1 |
| PastureWork | Work/pasturework.cpp:171 | 34 | 3 | 1 |
| Pickup | Work/pickup.cpp:66 | 3 | 4 | 1 |
| PlantArea | Work/plantarea.cpp:132 | 18 | 2 | 2 |
| PlantOrchard | Work/plantorchard.cpp:126 | 17 | 2 | 2 |
| TendArea | Work/tendarea.cpp:123 | 20 | 3 | 3 |
| TradeWork | Work/tradework.cpp:86 | 7 | 3 | 1 |

(`MarketBst_b290_ResolveFuncs` resolves no exe address — only names.)

The 18 read-but-unregistered addresses (STUB, or a latent bug):

| addr | kind / what | readers | state |
|---|---|---|---|
| 0x140002280 | exe fn (Entity_FindAudioComponent) | engine_funcs `fn_Entity_FindAudioComponent`, citizen_assignworkplace, firecomponent_ignite, tornadocomponent | null; 4 readers |
| 0x140022480 | exe fn | action_base_cnr `s_fn_022480` | null |
| 0x1400938d0 | exe fn Weather DrawUpdate | gameupdateloop.cpp:513 | guarded `[GUARD-M5]` |
| 0x14009e8c0 | exe fn RegionSystem build | gamegrid_serialize.cpp:96 | null |
| 0x1400fcc90 | exe fn | edittool.cpp:153 | null |
| 0x140101ee0 / 0x1401026a0 | EntityUI::Startup / Update | gameupdateloop.cpp:473/454 | guarded; camera driven directly instead (gameupdateloop.cpp:1305) |
| 0x1401513f8 | slot `Volume::Box::Box()` | dragtool.cpp:135 | null |
| 0x140151ab8 | slot `InternalBasePtr::DeletePtr` | work_act_clear/path/perform/rawmat, nomadcomponent (dead) | null |
| 0x140152160 | slot `ExternalList::Find` | gameupdateloop.cpp:552 | fallback after a name resolve |
| 0x140152778 | slot `Factory::CreateObject` | nomadcomponent.cpp:103 (dead fn) | dead reader |
| 0x1401527e8 | slot = DATA `Graphics::Particle::_rng` | gatherrawmaterials.cpp:219 **as MemAlloc** | wrong-name fallback, works only while null |
| 0x1401527f0 | slot `Math::Vector3::Lerp` | gatherrawmaterials.cpp:225 **as MemFree** | wrong-name fallback, works only while null |
| 0x140152a70 | slot `Vector3::operator==` | citizen_assignworkplace.cpp:123 | null |
| 0x140172130 / 0x140172274 | .rdata floats | quarrycomponent_deform.cpp:101/102 via `RF::Get` | hard-coded defaults 0.85f / -0.25f used, not checked against the binary |
| 0x1401c22d0 / 0x1401c23d0 | .data LUTs | action_pf_geom.cpp:279, action_pf_road.cpp:324 | local copy used as fallback |

## 4. Blind spots

- **What reads BY ADDRESS from data (CALLBACK, 17 rows):** `CitizenAI_ValidateAndEnterState`
  (`citizenai_candidatelist.cpp:663`, a `switch(state)` that yields an address, then one
  `ResolveAddr(Module::Exe, addr)`; 14 addresses) and `TutorialStep::Serialize` (`tutorialstep.cpp:129`,
  a local `ArrayLeg legs[3]` of serialize-helper addresses fed to `SerializePtrArrayFn(exeAddr)`; 3). Both
  are greppable by literal; I found them because they were the only literals in a brace/statement
  context. No serialized function id, no state-machine table and no `DEFINE_EXE_VTABLE` holds an exe
  address (`exe_type.h:122` resolves the compiler's vtable by symbol). An address built by arithmetic
  (base + index) would be invisible; the only `0x140000000` arithmetic is diagnostic.
- **Liveness is by name.** An overloaded or same-named free function in another file is conservatively
  live; members are all roots; a function whose only reference is its own `RegisterExeAddr(&F)` row is
  live through the registration function. Reachability is one pass over names, not a call graph, so it
  undercounts dead code (never overcounts it, with the one exception that a function called only
  through a function pointer stored in a namespace-scope initialiser is still found as a root).
- **Slot VALUES are not checked:** a slot cell registered with `ResolveExport(name)` holds null if the
  name is not ported; `RegisterLane3Slot` then does not register at all. The boot log's
  `[RtUnresolved]` line answers that, not this script. `tools/dispatch_slot_map.py --check` covers the
  names.
- **Registration order:** 5 floats are registered twice with the same value (below); the table's
  `reg_file` names one site, `n_regs` = 2.
- **Signatures are not compared.** The reader's `fn_…_t` typedef and the registered body's prototype can
  disagree (this project's hidden-retslot and dropped-arg bugs); a direct call will make the compiler
  check every one. Read by hand: none — too many (588).
- Hand-read to cover the method: the 39 non-literal `ResolveAddr(Module::Exe` lines (all helpers), the
  5 variable-address `RegisterExeAddr` lines, every brace/statement-context literal, all 45 DEAD rows
  (each address appears nowhere else in code), all 18 STUB rows, and the 5 reader functions the
  reachability pass found unreachable (`AutoPickupComponent_InitFuncs`, `NaturalResourceComponent_InitFuncs`,
  nomadcomponent `EnsureB60`, storagelocationcomponent `EnsureHelpers`, `WorkAcc_ZoneRefresh_9c480`: their
  callers `AutoPickupComponent_Serialize`, `NomadComp_RebuildController_b60`,
  `StorageLocationComponent_OnCreate` have no caller; the other 43 dead readers are dead statics).

## 5. What surprised me (code vs NEXT_SESSION.md §3a / comments)

1. **Import slots do not dominate.** §3b's note ("exe import slots dominate code/Game") is wrong for the
   exe registry: 162 of 2,375 reader occurrences (59 of 778 addresses). The bulk is 1,961 reads of our
   own exe-function bodies by retail address.
2. `RegisterExeAddr(` is **698** code lines, not 696 (688 literal calls + 1 named + 4 slot-wrapper bodies
   + the float loop + 4 declaration/definition lines). `X_ResolveFuncs`: 36 definitions and **138 calls**
   (+24 prototypes), not 154.
3. `RegisterExeAddrs`, `ExeAddrEntry`, `ReadGlobal`, `WriteGlobal`: declared and defined, never called.
4. 264 named exe addresses (`XAddr` namespaces in `engine/types/ui/Panels.h` 84, `Dialogs.h` 54, the
   Work/Tool headers) are documentation: 2 are used (`BuildRoadAddr::CtorParams` → the registration at
   `buildroad.cpp:519`; `kSlot_Vector2Ser` → a reader). `Tool.h:80` even says "(documentation)".
5. Two wrong-name import-slot readers (0x1401527e8 read as MemAlloc is a DATA import, 0x1401527f0 read
   as MemFree is `Vector3::Lerp`) — exactly the "works only while null" case `.claude/rules/address-registry.md`
   warns about. Registering either slot would break GatherRawMaterials. They are dead fallbacks behind a
   `ResolveExport` primary: delete, never register.
6. 5 `.rdata` floats have two storage cells: 0x14017210c (reg_boat_ctrl + float table), 0x1401722c0
   (reg_fire + float table), 0x14017219c / 0x1401721dc / 0x140172240 (gameupdateloop_load.cpp + float
   table). Values agree; the later `Register*` call wins.
7. Four DEAD rows are dead only because their readers' pointers are never used: `EnsureTailInit` in
   `citizenai_executor.cpp` still resolves 0x140131090 / 0x140131c90 / slot 0x140151e78 /
   0x140172148 into statics nothing reads (the comment at :543 says the slot was replaced by `InlScalarMax`).
8. Two vtable slots marked **PORT GAP** jump to bodies that ARE ported and registered:
   CitizenAIComponent [6] 0x14010f880 → `JMP 0x14010f890` (`CitizenComp_CitizenAIComponent_CheckAge`,
   `reg_component_people.cpp:79`), PickingComponent [11] 0x140128ba0 → `JMP 0x140128f50`
   (`PickingComponent_RegisterMesh`, `reg_closeout_lane_d.cpp:60`). The gap may be one line each.
9. 21 of the 45 DEAD rows are in `code/Game/Game/*_serialize.cpp`/`gameupdateloop_teardown.cpp`/
   `terraingenerator.cpp` Register functions — the "serialize helper" batches registered bodies whose
   readers were later converted to direct calls, and the rows stayed.
10. 0x140115e60 (CitizenAI LongTermPlan) is both a switch row (CALLBACK) and read directly by 12 readers
    in Work/engine; it is classed CALLBACK, the direct readers convert like DIRECT ones.

## 6. Proposed blocks (readers first, rows last; blocks own FILES)

Principle (§3c): convert every READER to a direct reference while the rows still exist (behaviour
identical: every DIRECT row is non-null at run time, so each `if (fn)` guard is already always-true);
delete rows only when the table shows 0 readers. No reader runs at namespace scope (checked), so no
conversion changes static-init order.

**P0 — declarations first (serial, new files only).** 547 DIRECT bodies have no header; 584 distinct
bodies are the targets. One owner writes one new declaration header per defining module
(`code/Game/<Module>/<module>_exe_bodies.h` or similar: Action 142 bodies, Component 176, Game 74, Work
74, Player 51, Dialog 42, Tutorial 17, Entity 10, Goal 1, standalone 1), copying each prototype from its
DEFINITION (linkage included: 321 of the reg files' prototypes are `extern "C"`). Also declares the 63
STORAGE objects (they live today as file-statics in 18 registering files, 22 of them in the float table; making
them visible means moving each constant into the header as `inline constexpr`/a class static — that
edits the reg files, so it belongs to block Z, and P0 only adds the declarations). Touches only new files.

**Reader blocks (parallel after P0; each owns only its reader files; rows untouched):**

| block | files | occurrences | addresses | notes |
|---|---:|---:|---:|---|
| R-Work | 43 | 554 | 157 | 19 `X_ResolveFuncs`; the 2 wrong-name fallbacks; 7 dead readers |
| R-Comp-citizen (`Component/citizen*`) | 25 | 343 | — | includes the CALLBACK switch (`citizenai_candidatelist.cpp`) and `citizenai_executor.cpp`'s dead `EnsureTailInit` statics |
| R-Comp-a–f (`Component/[a-f]*` minus citizen) | 37 | 246 | — | 3 dead helper definitions (boat route, disease) |
| R-Comp-g–z | 52 | 423 | — | tornado dead helper; quarry floats (STUB) |
| R-Action | 38 | 270 | 172 | 17 `X_ResolveFuncs` |
| R-Player | 26 | 155 | 96 | `resolveIAT`, `SlotFn` |
| R-Game+Tutorial+Dialog+Entity+Goal | 39 | 165 | — | tutorialstep CALLBACK table; gameupdateloop STUB guards stay |
| R-engine (`engine/funcs/engine_funcs.cpp` only) | 1 | 195 | 176 | initialisers become `fn_X = &X`; removing the globals is question 5's inventory |
| R-harness (`standalone/*_harness.cpp`, `redzone_alloc.cpp`, `runtime/src/collection/alloc_door.cpp`) | 11 | 24 | 18 | the alloc door reads slots 0x140152b60/b68 (MemAlloc/MemFree) |

(Component split by file prefix so each block is ~250–420 occurrences. The 24 `code/Game` files that
hold Register functions (Game 10, Component 6, Dialog 5, Player/Tutorial/Work 1 each) belong to their
directory's R block; their ROWS stay until Z.)

**Z0 — DEAD rows (can run in parallel with the reader blocks):** the 24 DEAD rows in 8 standalone files
(`address_registry.cpp` 5, `reg_citizenai_housing_idle.cpp` 7, `reg_citizenai_leisure.cpp` 3,
`reg_rdata_float_consts.cpp` 3, `reg_code_lane4.cpp` 2, `reg_animal_serialize.cpp`, `reg_component_people.cpp`,
`reg_herd_live.cpp`, `reg_immigration.cpp` 1 each). The 21 DEAD rows in `code/Game/Game` go with R-Game
(same files). Behaviour-neutral: no reader, so no resolve changes.

**Z — row deletion (serial, last, one owner, after every R block merged):** the remaining 715 rows
(720 registration sites) in `address_registry.cpp`, 121 `reg_*.cpp` and the 24 `code/Game` files; the
four slot-cell wrappers; `RegisterExeAddr`/`RegisterExeAddrs`/the exe half of `resolve_standalone.cpp`.
Moves the STORAGE constants into the P0 headers. What stays: the 18 STUB addresses, listed by name
(their readers keep a null-returning door, or are converted to an explicit "not ported" null — a
behaviour-neutral rename).

**Separate titled commits (behaviour changes, gate with resave x5):**
- deleting the 48 dead readers and the dead fallbacks at 0x1401527e8/0x1401527f0 is neutral, but
  deleting a dead reader FUNCTION (`AutoPickupComponent_Serialize`, `NomadComp_RebuildController_b60`,
  `StorageLocationComponent_OnCreate`, …) is a code deletion — its own commit;
- the PORT GAP slots in surprise 8 (wiring a ported body into a vtable slot changes behaviour);
- any STUB that gets a body (0x140002280 FindAudioComponent, EntityUI Update/Startup, the quarry floats);
- any typedef/prototype mismatch the compiler finds when a reader becomes a direct call.
