# Inventory E — `engine/funcs/` (NEXT_SESSION §3b question 5)

Table: `data/E_engine_funcs.tsv` (313 rows). Script: `scripts/E_engine_funcs.py` (re-runs from the
repo root with no arguments in about 70 s; `--dump FILE` also writes every counted use line).
Read-only: no product file was edited.

## 1. The answer

- `engine_funcs.h` declares **228 `extern`s**: 227 globals (217 function pointers + 10 data pointers)
  and one `extern "C"` function declaration. The 3,035 lines also hold **52 plain function
  declarations**, **12 inline functions**, **1 alias** (`fn_SM_CancelWork`), **2 forward class
  declarations**, **227 typedef lines / 226 names** (`fn_ExternalPtr_Serialize_t` is typedef'd
  twice, lines 1776 and 2792), 0 macros. 313 table rows (typedefs used by an extern ride on its row).
- **Externs by class: DIRECT 214, STORAGE 10, STUB 1, DEAD 3, CALLBACK 0.** Every DIRECT
  pointer already points at OUR body: 178 exe-address rows at a registered C-shaped port (2 with a
  never-taken fallback), 31 export rows + 1 runtime-address row at a member of a class that exists in
  `runtime/types` (via an `Rt*` wrapper), 3 bound directly to our function in the .cpp, and the
  `extern "C"` declaration.
- Non-extern rows: 51 function declarations + 11 inline functions + 2 forward declarations are
  DIRECT (they move to a header); 1 declaration, 1 inline, the alias and 18 typedefs are DEAD.
- **4,890 code lines use the externs** (2,152 calls, 1,765 readiness guards) in **460 files**
  (429 `code/Game`, 29 `standalone`, 2 `engine`; 0 runtime, 0 dx11, 0 tests). **Ten rows carry
  3,772 of those lines in 426 files**: `g_pGameWorldSingleton` 832, `fn_IsDerivedFrom` 895,
  `fn_MemAlloc` 487, `fn_MemFreeStatic` 460, `g_pMemAllocSingleton` 457, `fn_InternalBasePtr_Ctor`
  401, `fn_Time_Ctor_Float` 143, `fn_InternalBasePtr_DeletePtr` 41, `fn_MemAllocStatic` 31,
  `g_pGameSingleton` 25. The other 204 externs: 1,118 lines in 205 files.
- The only real STUB is `fn_Entity_FindAudioComponent` (0x140002280, registered nowhere). No export
  row resolves null. 19 globals are misnamed against their binding; 16 addresses are bound by two
  or three globals each (35 globals). 6 member rows carry a hidden return slot (not a rename), 2 exe
  rows have an arity mismatch against their registered body, 3 globals are rewritten at run time.

## 2. How the table was built

1. **Header**: `codegrep.strip()` (comments out, strings kept), then a depth-0 statement walk:
   `typedef R (*N)(A);`, `extern T N;`, `extern "C"`, `class N;`, `static inline auto& N = M;`,
   `inline … N(…) {…}`, `R N(…);`. Nothing is left unparsed.
2. **Binding by INITIALISER** (never by name): each statement of `EngineFuncsInit()` that assigns a
   global is read for `ResolveAddr(Module::Exe|Runtime, 0x…)`, `ResolveExport("…")`, a `dispatch`
   local that re-reads a PE import slot (`exe_slot`), `&Function`, or a lambda. `if (!X)` fallbacks
   are kept as second bindings. 230 bindings for 227 globals (exe_addr 188, export 31, exe_slot 3,
   rt_addr 2, other 6).
3. **What the binding resolves to**, the way `resolve_standalone.cpp` answers at boot:
   exe address → the `RegisterExeAddr` / `RegisterDispatchSlot` / `RegisterLane3Slot` /
   `RegisterPlacePlayerSlot` row (table rows `{0x…,` too); a slot whose value is
   `ResolveExport("…")` is chased to the export; export name → `RegisterRuntimeExport("…")` or
   `RegisterRuntimeAddr(addr)` with the address `rt_export_addrs.inc` pairs with the name; runtime
   address → the mint table or `RegisterRuntimeAddr`. The registered symbol's definition is found
   (registering file first, namespace-qualified) and read: line count, parameter count, and any
   `NOT IMPLEMENTED` / `PORT GAP` / `NOT PORTED` marker. A marked body of 12 lines or fewer is a STUB;
   a longer marked body is a real port with a hole (noted `PARTIAL`, stays DIRECT).
4. **Uses**: every code line (strings blanked, per the brief) naming the item as a whole word in
   `code runtime standalone engine dx11 tests tools`, excluding the `standalone/build*`,
   `tests/unit/build`, `.claude/worktrees`, `/build/`, `/build-` paths. Not counted: the item's own
   declaration, its `= nullptr` definition, its binding statement, the `_DEBUG` dump printfs inside
   `EngineFuncsInit`, an inline function's own body, a declared function's own definition line, and
   files that declare their OWN symbol of the same name (a shadow; listed in `notes`). An `extern`
   redeclaration elsewhere is a use (it names the same symbol). A multi-line call (`fn_X` then `(`
   on the next line) is a call; `fn_X` then `?` on the next line is a guard.
5. **Columns.** `bound_by`/`bound_to`: door and literal from the initialiser. `resolves_to`:
   `file:function` of the registered body, `MINT …`, or `NULL`. `uses_*`: code lines per directory
   (`uses_code_game` = `code/Game`). `guards`: readiness tests (`if (fn_X)`, `fn_X &&`, `!fn_X`,
   ternary). Extra columns after the requested ones: `calls`, `writes` (the global reassigned),
   `copies` (value cached into another variable or passed as a callback), `addr_taken` (`(void*)fn_X`
   for a printf or a readiness table), `other_uses`, `other_readers_same_binding` (code lines OUTSIDE
   `engine/funcs` that resolve the SAME address or name themselves: the registry row cannot die until
   those convert too; they are other inventories' rows), `header_line`, `use_sites` (truncated at
   1,500 characters; `--dump` has all of them), `notes` (`READ:` = read by hand, see §4).
6. The `class` rule: no use → DEAD; data pointer → STORAGE; binding resolves NULL or to a stub body →
   STUB; otherwise DIRECT. `READ:` notes may override (two did: `fn_Entity_FindAudioComponent` STUB,
   `fn_TutorialStepExecute` DIRECT). CALLBACK was searched for (a pointer written into a table or a
   struct and called later) and none exists — see §4.

## 3. Counts per class, per directory

| class | rows | of which externs | code/Game lines | standalone | engine | runtime / dx11 / tests |
|---|---:|---:|---:|---:|---:|---:|
| DIRECT | 278 | 214 | 3,647 | 67 | 35 | 0 |
| STORAGE | 10 | 10 | 1,394 | 83 | 8 | 0 |
| STUB | 1 | 1 | 2 | 0 | 0 | 0 |
| DEAD | 24 | 3 | 0 | 0 | 0 | 0 |

Files with a use, by `code/Game` subdirectory (all rows): Component 163 files / 1,494 lines / 50
rows; Game 63 / 813 / 59; Dialog 62 / 1,223 / 164; Work 46 / 524 / 64; Action 37 / 320 / 13;
Player 31 / 330 / 31; Tutorial 20 / 174 / 36; Entity 4 / 123 / 27; Goal 4 / 42 / 8;
`standalone` 29 / 150 / 20; `engine` 2 / 43 / 14.

DEAD (24): externs `fn_SceneAddObject`, `fn_SceneRemoveObject`, `fn_MeshInstance_UpdateHierarchy`
(bound through dispatch-slot reads; the only mentions are comments in `tool_base_place.cpp`);
declaration `fn_FindComponent_Residence` (defined, never called); inline `Storage_AddItem`; alias
`fn_SM_CancelWork`; 18 typedefs with no extern (`fn_FindComponent_t`, `fn_ToolbarSort_t`,
`fn_SetEngineNextLoop_t`, `fn_Create{Bridge,Drag,Fenced}Description_Ctor_t`, the three
`fn_CitizenBehavior_*_t`, `fn_ExternalPtr_Serialize_Generic_t`, …). Also dead but not a row: four
`dispatch` blocks in `EngineFuncsInit` that resolve 0x140151288 / 0x140151a00 / 0x140151a08 /
0x140151a20 and assign nothing (engine_funcs.cpp:897-905, 952-963).

Typedefs: 208 names ride on an extern; 204 die with it. Used elsewhere: `fn_MemAlloc_t` and
`fn_MemFreeStatic_t` (`standalone/redzone_alloc.cpp:57-58`), `fn_IsDerivedFrom_t` (the .cpp's own
trampoline), `fn_ExternalPtr_Serialize_t` (its own duplicate typedef).

STORAGE (10), what each cell really is:

| global | cell | holds | writers | end state |
|---|---|---|---|---|
| `g_pMemAllocSingleton` | mint `?_singleton@MemoryAllocator@System@@` | a SNAPSHOT: `EngineFuncsInit` copies the cell (NULL at that time), `standalone/main.cpp:686` refreshes it once | main.cpp:686 | the class static (question 4); callers read it live |
| `g_pGameWorldSingleton`, `g_pGameSingleton` | 0x1401c3dd8 → `g_storage_GameWorldSingleton` (address_registry.cpp:1149) | the GameUpdateLoop | gameupdateloop_load.cpp:914, gameupdateloop.cpp:2679 | one class static on the loop class |
| `g_pKeyMapSingleton` | 0x1401c3de8 → `g_storage_KeyMapSingleton` | the Game object (the header says "NOT the KeyMap") | game.cpp:39, game_main.cpp:776 | a class static on `Game` |
| `g_pGameSettingsPtr`, `g_pNewGameSettings` | 0x1401c3de0 → `g_storage_GameSettingsPtr` | the settings block (`int32_t**` in one, `void**` in the other) | game_main.cpp:565, :771 | one typed static |
| `g_pInputConfigSingletonExref` | import slot 0x140151738 → `&g_StandaloneKeyMapMid` | a hand-built 3-level chain ending in the KeyMap | address_registry.cpp:48 | `Input::Configuration::_singleton` |
| `g_pAgeMultiplier`, `g_pNotifyTimerDuration`, `g_fUVScale` | 0x140172198 / 0x1401721cc / 0x14017214c | constants 5.0f, 30.0f, 1.0f | citizenui.cpp:197, residenceui.cpp:83, eventnotifydialog.cpp:933 install LOCAL fallbacks when null (never taken) | `constexpr float` |

## 4. Blind spots, and what was read by hand

- **Registration timing.** A binding is cached ONCE in `EngineFuncsInit`. The table assumes every
  registration runs before it. Read: every registering function behind these rows (88) has a caller in
  `AddressRegistryInit()` or a `RuntimePortInit()` chain (both before `EngineFuncsInit`,
  main.cpp:1333-1344; the chains were not traced further), except `reg_closeout_lane_d.cpp`, which registers from a **static initializer** (before `main`) —
  a registration spelling not on the brief's list.
- **Last-writer-wins.** When an address is registered twice the resolver keeps the last one; the
  script reports every registration line and resolves to the last in file order, not in call order.
  None of the 174 distinct bound addresses is registered twice (`notes` would say `registered N times`).
- **Other readers of the same binding** (`other_readers_same_binding`): 105 rows, ~970 lines in
  `code/Game` and `runtime` resolve the same address or name themselves (`ResolveAddr`, `RtAddr<>`,
  `ResolveExport`, `RT_EXPORT_ONCE`, a `constexpr` on the same line). They are counted, not
  classified: they belong to inventories A/B. A `constexpr` address declared on a different line
  from its use, a `k…Addr[]` table, or `ByAddr`-style wrappers named differently are not seen.
- **CALLBACK search.** Read every `copy`/`addr`/`other` use of a function pointer (list via `--dump`).
  None stores a pointer into data that is called later by address: the copies are per-file caches
  (`s_fn_MemFree = fn_MemFreeStatic` in 11 Work files, `s_fn_IBP_DeletePtr`, `s_fn_IsDerived`,
  `s_fn_IBPCtor`, `s_fn_IBP_Ctor`), one callback ARGUMENT (`fn_ExternalPtr_BuildSerialize` into
  `SerializeCompRefTree`, citizenlist_serialize.cpp:661, convertible to `&Reimpl_…`), and diagnostic
  tables / printfs (`townhallui.cpp:2076-2087`, `performwork.cpp:160`, `HO_MISS`). The caches are
  secondary doors (spelling 5): they die with the conversion of their file.
- **Stub markers are a grep.** Five registered bodies contain `PORT GAP` / `NOT PORTED`
  (`WorkAct_PerformWorkCtor_bffd0`, `…_c1eb0`, `WorkAct_GatherRawMatCtor_bf790`,
  `BuildStructure_SetupBuildActions`, `TutWave_StepExecute`). Read: all five are real ports; the
  first four carry the `kCitizenAIMessageTargetTypeId` PORT GAP constant, the last has one NOT PORTED
  leg (tutorial completion). They are DIRECT; the holes are other inventories' STUB rows.
- **ABI.** The retslot flag keys on parameter names (`retval`, `retbuf`, `retStr`, `outIBP`, …) and
  the arity check compares typedef and body parameter counts. Read the 32 member-target rows by hand
  for by-value returns. A retslot typedef whose parameter is named otherwise would be missed.
- **Includes** are resolved with the Makefile's `SA_INCLUDES` order; `#include <…>` is not followed.
  Only one header includes `engine_funcs.h` (`code/Game/Dialog/citizen_icon_helpers.h`), so direct
  includers ≈ translation units.
- **Name collisions** handled by the shadow rule: `FindComponentByTypeId` (the header's 2-argument
  inline vs a 3-argument overload in ~10 files), `g_fUVScale` (a local `const float` in
  weather_particle_draw.cpp), `fn_Random_RandFloatTol` (its defining file).
- **Hand-read facts** are the `READ:` notes (58 rows), kept in the script's `HAND` table so a re-run
  reproduces them.

## 5. What surprised me (code contradicting §3a or a comment)

1. **§3a's "228 externs" is 227 globals + 1 function declaration**, and the header is not mostly
   externs: 52 plain function declarations and 12 inline bodies are the part that must MOVE, not be
   converted.
2. **Three globals are rewritten at run time.** `standalone/redzone_alloc.cpp:340-341` swaps
   `fn_MemAlloc` / `fn_MemFreeStatic` for `ShimAlloc` / `ShimFree` (BANISHED_REDZONE). A direct
   call bypasses the redzone. `EngineFuncsInit` itself swaps `fn_IsDerivedFrom` for
   `IsDerivedFrom_NullSafe` (null → false), a documented SA deviation: `p->IsDerivedFrom(id)` on the
   real class drops it. Both are BEHAVIOUR decisions, not renames.
3. **`fn_MemAlloc` is not `?MemAlloc@`**: it is `?Allocate@MemoryAllocator` (the singleton-first
   member), whose registered value is `SwapAllocate` in an anonymous namespace of
   `standalone/alloc_cutover_arm.cpp`, reached only through a pointer the armer hands back. There is
   no C++ name to convert to yet.
4. **`g_pMemAllocSingleton` is a stale snapshot**: cached NULL at `EngineFuncsInit` (the comments in
   `game_main.cpp:477` and `main.cpp:680` say so) and patched once by `RunStartupSelfTests`.
5. **The IBPTree fallback never fires, and the comment explaining it is wrong.** engine_funcs.cpp:1460
   says 0x140021a30 / 0x14001a6e0 are "unmapped" so the EntityList falls back to
   `IBPTreeInsert_CppImpl`; both are registered (`RegisterWorkBstBatch`, in `AddressRegistryInit`),
   so the EntityList tree runs `WorkBST_RegisterWork_1a30` and `IBPTreeInsert_CppImpl` /
   `IBPTreeRemove_CppImpl` (entitylist.cpp:277, :310) are dead code.
6. **Arity mismatches against the registered body** (the "Ghidra drops args on forwarders" shape):
   `fn_Map_Invalidate` is typedef'd with 1 parameter; `Pf_ZoneRefresh_9c480` takes 4 and passes
   `param_4` to the flood fill (9c170) as its seed — the one caller (gamegrid_serialize.cpp:318)
   leaves R9 undefined. `fn_Map_CalcPathDist` 3 vs `Pf_RegionPathLength_8ba0` 4 (`param_4` goes to
   0x14001dc00). A direct call will not compile until someone decides what retail passes: own commit.
7. **Guard-only doors.** `fn_InternalBasePtr_Ctor` (393 guards) and `fn_InternalBasePtr_DeletePtr`
   (38) are never called: every use is `if (fn_X) new (p) Object::InternalBasePtr(…)` or a guard
   around the member `DeletePtr()`. Conversion is deleting a test that is always true.
8. **19 misnamed globals** (binding disagrees with the name): `fn_Game_DestroyEntity` (header says
   FUN_1400c3ec0; initialiser 0x140091e60 = `RawMaterialComponent_RegisterRawMaterial`),
   `fn_AudioObjectGroup_Ctor` (= ComponentEntity ctor), `fn_AudioScene_RegisterObject`
   (= EntityList register), `fn_StorageComponent_GetCapacity` / `_SpawnItem` (pasture),
   `fn_Building_AddBuyToStorage` (livestock purchase), `fn_CitizenList_RequestWorker`
   (`FindAvailableBuilding`), `fn_RadiusCalc` (highlight add), `fn_RadiusDisplay_Remove` (decal
   remove), `fn_SetEditMode` (= `Toolbar_ShowGroup_Impl`; the header ALSO calls 0x140102b60
   "SetEditMode" and ports it a third time as `EntityUI_SetEditMode_Impl`), `fn_SetFollowBuilding`,
   `fn_CalcMaxBuilders` (= `WorkComponent_CalcMaxWorkers`), `fn_ResidenceComponent_GetOccupantCount` /
   `_GetSpawnPoint`, `fn_Entity_FindComponent_Position` (finds InteractComponent),
   `fn_FindComponent_Position` (finds BuildComponent), `fn_FindComponent_Build` (MapComponent),
   `fn_Entity_FindComponent_MatDesc` / `fn_FindStorageLocationComp` (WorkComponent).
9. **Two bodies for one retail function**: 0x14012e5c0 is registered as
   `WorkComponent_CalcMaxWorkers` (tutorial_base.cpp) while this header declares
   `fn_StorageCountQuery` (workcomponent.cpp:321) for the same function; three exe functions
   (0x14000bd30, 0x14000bf40, 0x140002340) share ONE registered body,
   `Reimpl_ExternalPtr_Serialize_StringTable` ("stream-identical", address_registry.cpp:1195).
10. **`g_pInputConfigSingletonExref`**: the header (line 238) says it "was named
    g_pInputConfigSingletonExref until the B.3.7 close"; it still is.
11. **`food_stock_census.cpp:29` redeclares `extern "C" void** g_pGameWorldSingleton;`**, a C
    linkage spelling of a C++ global (it links because GCC does not mangle namespace-scope variables).
12. **The header's header comment is out of date**: "These are runtime-resolved pointers to functions
    in Runtime-x64.dll" — 188 of 230 bindings are exe addresses, all resolved to our own bodies.
13. **170 of the 180 exe-bound DIRECT targets have no header declaring them** (they are reached
    only through the registry and ad-hoc `extern` lines in `standalone/reg_*.cpp`), so "convert to a
    direct call" needs those declarations first.

## 6. `EngineFuncsInit()` and the rest of `engine/`

**What it does besides binding** (called once, `standalone/main.cpp:1344`, after `RuntimePortInit`
and `AddressRegistryInit`, before `RuntimeFuncsInit`; always returns `true`):
installs the `IsDerivedFrom_NullSafe` trampoline; assigns the `fn_MemFree` lambda;
binds `fn_ScenarioDialog_Ctor` / `fn_ModDialog_ArrayPush` to our functions directly (the second is
`fn_ModDialog_ArrayPush58_CppImpl`, DEFINED in engine_funcs.cpp, as is
`Entity_FindComponentByExeTypeId`); runs the two IBPTree fallbacks; runs four resolves whose result
is discarded; prints 56 `printf` lines (`[EngineFuncs] …`) under `_DEBUG` (the debug gate build defines
it). **Boot-log effect**: the resolve of the unregistered 0x140002280 prints
`[STANDALONE] NOT IMPLEMENTED: exe function at 0x140002280` — one member of the gate's NOT
IMPLEMENTED stub set comes from here and leaves with the row (question 6). Its two `ResolveAddr
(Module::Runtime…)` reads add 2 to the `[RtPort]` hit counter; no export row misses (0
`[RtUnresolved]` from this file).

**`engine/` in full** (line count, direct includers resolved with `SA_INCLUDES`):

| file | lines | includers | what it is |
|---|---:|---:|---|
| `funcs/engine_funcs.h` | 3,035 | 510 (474 code/Game, 35 standalone, 1 engine) | this inventory |
| `funcs/engine_funcs.cpp` | 1,651 | — | definitions, `EngineFuncsInit`, 3 function bodies |
| `types/common.h` | 59 | 12 (10 runtime, 2 standalone) | shared primitive typedefs |
| `types/ui/DialogInstance.h` | 11 | 24 (21 code/Game, 2 runtime, 1 engine) | alias of the runtime class |
| `types/ui/DialogController.h` | 11 | 5 | alias |
| `types/ui/Dialogs.h` | 213 | 4 (3 code/Game, 1 standalone) | exe dialog layouts |
| `types/ui/Panels.h` | 300 | 3 (code/Game) | exe panel layouts |
| `types/Component.h` | 9 | 3 (engine) | forwarder to `Component/Component.h` |
| `types/FireComponent.h` | 71 | 2 | layout |
| `types/DiseaseComponent.h` | 46 | 2 (1 code/Game, 1 tests) | layout |
| `types/CitizenBehavior.h` | 225 | 1 | layout |
| `types/EntityList.h` | 92 | 1 | layout |
| `types/TornadoComponent.h` | 58 | 1 | layout |
| `types/RawMaterialDescription.h` | 70 | 1 (tests) | layout |
| `types/{DialogLoaderLoopT,GameUpdateLoop,LoaderLoop,MainLoop,UpdateLoop}.h` | 10-11 each | 1 each (tests) | `using` aliases / forwarders (B.3.8) |
| `types/{ClimateGenerator,MapGenerator,MenuScene,Profession,ProfessionList,TerrainGenerator,Toolbar,Weather}.h` | 7-9 each | **0** | forwarders to `code/Game/Game/*.h`, unused |
| `types/Configuration.h` | 102 | **0** | unused (the includers resolve to `runtime/types`) |
| `types/Modification.h` | 120 | **0** | unused |
| `types/io_serializer.h` | 203 | **0** | unused |

11 `engine/types` headers have no includer and can go now; the aliases and layouts go with their
users (other inventories).

## 7. Proposed blocks (disjoint FILE sets)

The ten heavy rows touch 426 files, so blocks cannot own rows: every block converts every row that
appears in ITS files, following one rule sheet. Order:

**E0 — declarations first (no behaviour change; must land before any caller block).**
Files: `engine/funcs/engine_funcs.h` plus NEW headers only. Create a header declaring each registered exe body
that has none (170 rows, 153 distinct bodies in 82 defining `.cpp` files; only new `.h` files are
written), move the 51 function declarations, 11 inline bodies, the `extern "C"`
declaration and the 2 forward declarations out of `engine_funcs.h` into them, and have
`engine_funcs.h` include the new headers so nothing else changes. Delete the 24 DEAD rows and the 4
discarded resolves (engine_funcs.cpp joins this block). ~95 rows, 2 existing files + one new header per defining file or directory.

**Behaviour commits (separate titled commits, gated with resave x5, owned by E0's file set plus the
named file):** (a) a hookable named allocator entry so `redzone_alloc.cpp` keeps working, then
`fn_MemAlloc`/`fn_MemFreeStatic` can convert; (b) the `IsDerivedFrom` null-safety (keep a free
null-safe helper or prove the sites); (c) `fn_Map_Invalidate` / `fn_Map_CalcPathDist` arity
(`gamegrid_serialize.cpp`, `action_pf_roots_route.cpp`); (d) `g_pMemAllocSingleton` read live
(removes main.cpp:680-690 refresh); (e) `fn_Entity_FindAudioComponent` (changes the NOT IMPLEMENTED
set). The STORAGE class statics (`GameUpdateLoop`, `Game`, settings, `Input::Configuration`) are
added in the class headers and the registry cells pointed at them — no behaviour change, but they
touch `address_registry.cpp` (coordinate with inventories A/C).

**Caller blocks (parallel after E0; each converts every extern use in its files):**

| block | files it owns | files | rows touched, extern use lines |
|---|---|---:|---:|
| E1 | `code/Game/Component/` | 163 | 40 externs / 50 rows, 1,474 lines |
| E2 | `code/Game/Dialog/` | 62 | 109 / 164, 992 lines |
| E3 | `code/Game/Game/`, `Entity/`, `Goal/` | 70 | 71 / 78, 966 lines |
| E4 | `code/Game/Work/`, `Action/` | 83 | 57 / 64, 818 lines |
| E5 | `code/Game/Player/`, `Tutorial/` | 51 | 44 / 56, 481 lines |
| E6 | `standalone/` (incl. `redzone_alloc.cpp`, `main.cpp`) | 29 | 17 / 20, 135 lines |

E1 is the largest by files but mostly the ten heavy rows (mechanical); E2 has the most distinct
rows. Split E1 alphabetically if a block must stay under ~80 files.

**EZ — close (after all caller blocks):** delete `engine/funcs/engine_funcs.{h,cpp}`, the
`EngineFuncsInit()` call and its Makefile source line; the 11 unused `engine/types` headers go in
E0. Then the registry rows these globals read are DEAD only if `other_readers_same_binding` is also
zero (inventories A/B own those readers).
