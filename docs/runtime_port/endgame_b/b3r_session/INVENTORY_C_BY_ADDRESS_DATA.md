# Inventory C — BY_ADDRESS_DATA (block 0 of the registry teardown)

Questions: NEXT_SESSION.md §3b **2** (what reads by address / by name from DATA at run time) and
**3** (which resolvers are lazy caches, on which thread; the boot order; the generator's retirement
mechanism). Table: `data/C_by_address_data.tsv` (144 rows). Script:
`scripts/C_by_address_data.py` (no arguments; re-runs from any cwd, rewrites the table
byte-identically, prints every number quoted here).

## 1. The answer

1. **Keys that reach a door through a variable are few: the table's 35 read rows (registration
   tables, dispatch slots and engine_funcs excluded) keep 66 rows.** 120 wrappers
   take the key as a parameter. Their 3,558 literal call sites are by-literal readers, and the
   A/B inventories count those. Once each wrapper was traced to its callers, 28 sites remain
   where the key is neither a literal nor a parameter. They sit in 20 files; the table lists each
   one with the rows it keeps. There are **no serialized function ids (0)** and **no computed
   addresses (0)**.
2. **The real CALLBACK shapes:**
   - one **state-machine table**: `CitizenAI_ValidateAndEnterState`, 14 handler addresses in a switch;
   - **51 dispatch slots** whose value is resolved once and **stored** (51 exe rows + 42 name rows);
   - **228 `engine_funcs` globals** bound at boot (208 rows; question 5 owns them);
   - **two dx11 handle structs**: `devicedata.cpp` walks a table of 21 names in a loop, and
     `buffers.cpp` fills `g_deps`.
3. **`DEFINE_EXE_VTABLE` keeps no rows (0).** The macro binds the compiler's `_ZTV…` symbol. No
   retail vtable address of those four classes is registered.
4. **Rows registered from data:** 5 sites register 204 rows through a loop or a named key, so a
   `Register*(0x` grep cannot see them:
   - `kRows` ×2: 88 addresses + 88 names;
   - `g_consts`: 25 exe STORAGE rows;
   - the mint loop: 2 rows;
   - `BuildRoadAddr::CtorParams`: 1 row.

   7 dispatch slots go through differently-named helpers that `dispatch_slot_map.py --check`
   does not parse.
5. **Lazy caches: 12 spellings, 1,335 sites.**
   - Only the `std::atomic` spelling is B6-5-safe (83 uses, 91 slots), and 12 of its copies
     freeze pre-init hits.
   - The main non-atomic spellings: `RT_EXPORT_ONCE` 242, hand-written `if (!x) x = …` 197,
     function-local static 103, bool-flag `Ensure*()` 195, `X_ResolveFuncs`/`_InitFuncs` 89 defs
     + 191 calls, and 79 guards that set the flag BEFORE filling.
   - Converting to a direct reference leaves no cache at all.
6. **Nothing binds before registration, and a direct reference binds earlier still.** Registration
   happens at link time instead of in `RuntimePortInit()`. No namespace-scope initialiser resolves
   (0). The only order-sensitive values are the dispatch-slot values, frozen at
   `AddressRegistryInit`, and the engine_funcs globals, frozen at `EngineFuncsInit`. Direct
   references remove both freezes.
7. **`rt_export_addrs.inc` is the mirror's table, and only the mirror reads it.** Of its 4,926
   rows, **16 are needed** (10 when "reader" means a literal at a resolver or wrapper call):
   - 1,034 rows have both keys registered explicitly;
   - 3,591 rows have neither key registered;
   - 301 rows have exactly one key registered, and only those 16 have a reader of the other key.

   Recommendation: don't retire its rows. Register those 16 explicitly (or convert their
   readers), then delete the table, its generator step and the mirror (§6 block C8).

## 2. How the table was built

- **Files:** `code engine standalone runtime dx11 tests/unit`, comments stripped with
  `b3x_session/codegrep.strip`, the codegrep `EXCL` build paths skipped, and the two generated
  `.inc` files skipped as readers.
- **Doors:** `ResolveAddr`, `ResolveExport`, `RtPortLookupAddr/Export`, `RtResolveCached`,
  `ReadGlobal`, `WriteGlobal`, `RuntimeExportIsRegistered`.
- **Wrappers (fixpoint):**
  - A door call whose key argument is an identifier naming a PARAMETER of the enclosing
    function, lambda or `#define` makes that callable a wrapper. Its call sites are then scanned
    the same way.
  - A call binds to a same-file definition first, then to a definition in a header it includes
    (transitively, by basename).
  - This found 120 wrappers. Among them are `RT_EXPORT_ONCE`, `RT_DATA_SLOT`, all the
    `AtomicExport/AtomicAddr` copies, `RtAddr` ×25 files, `RSlot`, `ResolveSlot`, `Exp<T>`,
    `Dx11ResolveRuntimeImport`, `SerializePtrArrayFn`, `ByAddr*` and `PtrSerialize`.
  - Brief spellings that are not on its list: `AtomicByName`/`AtomicByAddr`
    (userinterface_showdialog), `AtomicResolve` (combo/page descriptions), `Door`
    (element_vtable), `RuntimeAddr` (achievement ×3), `MemberDtor`, `Flt`, `RF::Get`,
    `SerializeParameters` (a wrapper of `RtAddr`).
  - The same helper also collects **registrar wrappers**: `RegisterPlacePlayerSlot` and
    `RegisterLane3Slot`.
- **Data site:** a door or wrapper call whose key is neither a literal nor a parameter. Named
  constants (`kX = "…"` / `kX = 0x…`, looked up in the file and its includes) are resolved to
  their value.
- **Tables:** array initialisers holding at least 2 retail addresses or mangled names (9 found).
- **Registry rows:**
  - literal `Register*(` keys;
  - `kRows` tables;
  - `g_consts`;
  - `k`-constant registrations (24/24 resolved);
  - registrar-wrapper literals;
  - the mint table.

  Totals: exe 759, runtime addresses 1,532, runtime names 1,340. `rows_kept_alive` counts the
  keys a site holds that one of these serves. The count includes the export row when only the
  MIRROR serves the key.
- **Hand-read facts** (source of values, thread, class) are the `CURATED` list in the script.
  Each entry is anchored by a regex, so line numbers are recomputed on every run.
- **Columns:**
  - `site_file`, `site_line` (0 = a per-directory cache census row);
  - `kind`;
  - `module`;
  - `distinct_addresses` (distinct keys, addresses AND names);
  - `source_of_values`;
  - `rows_kept_alive`;
  - `thread`;
  - `note`, which starts with `class=` (next section).

## 3. Rows per class, per directory (non-cache rows)

| class | rows (dir) |
|---|---|
| `CALLBACK` (8) | code/Game/Component 1 (the state switch), dx11 1 (`g_deps`), engine 1 (EngineFuncsInit), runtime/src/engine 1 (MainLoop device callbacks, 0 registry rows), standalone 4 (the four dispatch-slot helpers) |
| `DIRECT` (29) | runtime/src/ui 16, runtime/src/io 3, standalone 3 (`dlgctrl_route`, `modal_driver`, `scene_watch`), code/Game/Game 1, code/Game/Tutorial 1, code/Game/Work 1, dx11 1 (`devicedata`), runtime/src/graphics 1, runtime/src/object 1 (`kRows`) |
| `STORAGE` (2) | standalone 2 (`g_consts` 25 rows, the mint loop 2 rows) |
| `DEAD` (2) | code/Game/Dialog 1 (`tradeui` Dep list), dx11 1 (`dx11_seam` kVtables: VideoDX11 addresses, resolve nothing) |
| none (5) | the 4 `DEFINE_EXE_VTABLE` lines, the `splash_loop_stubs` vptr compare |
| cache census | 99 rows, one per spelling per directory (§5.2) |

`STUB` is 0. All 14 handler addresses in the state switch, all 3 `legs` and the geometry slot are
registered, so today no data-driven key resolves to a missing body.

### 3.1 The data-driven read sites (true data flow, by hand)

| site | values | keys / rows | thread |
|---|---|---|---|
| `citizenai_candidatelist.cpp:656` `CitizenAI_ValidateAndEnterState` | `switch(state)` → `addr` → `ResolveAddr(Exe, addr)` → call; a null skips the state | 14 / 14 | main (sim) |
| `tutorialstep.cpp:128` `legs[3]` | `{dataOff,countOff,serializeAddr}` → `SerializePtrArrayFn(leg.serializeAddr)` | 3 / 3 | loader (save load) |
| `geometry_serialize.cpp:95` `kSlot_Vector2Ser` | `ResolveSlot(kSlot,cache,tried,tag)` → `*(fn*)ResolveAddr(Exe)` | 1 / 1 | loader |
| `dlgctrl_route.cpp:169` `kSpecs[3]` | `ResolveAddr(Runtime, kSpecs[s].dllAddr)`, stored in `g_target[]`, printed; `Opt().dlgCtrlRoute` instrument | 3 / 3 | main |
| `modal_driver.cpp:118` `kUpdateStandAloneUI` | compared with the fn ptrs `ShowDialog` stores in MainLoop+0x90 | 1 / 1 | main |
| `scene_watch.cpp:451` `RtPortLookupAddr(0x180100f20)` | cached in a function-local static; compared with the object's `AddDrawItems` slot | 1 / 1 | main |
| `dx11/src/devicedata.cpp:251` `Entry table[21]` | a loop → `Dx11ResolveRuntimeImport(e.mangled, e.iat)`, stored in `g_im` | 21 names / 21 | main (first call at device startup) |
| `dx11/src/buffers.cpp:166` `EnsureDeps` | `g_deps` handle struct via `Exp<T>("…")`, plain-bool guard | 2 / 2 | both (buffers are built on the loader) |
| `wave24_lane_gfxmat.cpp:519/541` | `RT_GFXMAT_CORRUPT(1) ? A : B` negative-control swap at the door | 2 / 2 | both |
| `filesystem_win.cpp:171/179/186` | `RT_EXPORT_ONCE(Fn, RtIO::kName…)` named constants | 3 / 3 | both |
| `dialog.cpp:311/313` | `kElementFindElement[Type]` → `ResolveExport` | 2 / 2 | both |
| userinterface_{close,draw,frame,hover,showdialog}.cpp, element_vtable.cpp (11 sites) | `kMemAlloc/kMemFree/kCameraCtor/k*SetSize*Addr/kDynArrayPushAddr` → atomic caches | 11 / 11 | both (B6-5) |
| dispatch slots: address_registry.cpp:762 + reg_player_place / reg_unreg_lane1 / reg_unreg_lane3 | `RegisterDispatchSlot(0x14015…, ResolveExport("…"))`: the value is frozen in slot storage; readers do `*(fn_t*)ResolveAddr(Exe, slot)` | 51 slots / 93 | main (filled), both (read) |

### 3.2 Asked about, and not data

- **Serialized function ids:** 0. Save files carry factory type ids and names, never a registry
  key.
- **Addresses printed by diagnostics:** 0 resolves inline in a `printf` argument list, 0 inline
  `==` compares. Every compare goes through a stored variable (`modal_driver`, `scene_watch`,
  `splash_loop_stubs`). Printed LITERALS (`dx11_seam`, `tradeui` strings,
  `geometry_serialize`/`Dx11ResolveRuntimeImport` failure lines) read no registry row.
- **`engine.cpp:502`:** stores OUR thunks into `Video::Device`. It is a callback, but by C++
  address: 0 rows.

## 4. Blind spots, and what was read by hand to cover them

1. **Names or addresses built at run time.** The only builders found are the token-pasting
   `J1_ENTRY(180013da0)` macros in runtime/src/collection. They build FUNCTION NAMES, not keys;
   the registrations beside them use literals. The `snprintf`/concat shapes were read: 0.
2. **Wrapper binding is by name plus include closure, not by overload resolution.** Two helpers
   with the same name in one TU would merge. None was found, but the `Atomic*` helpers have 21
   definitions: 6 in 3 headers and 15 file-local copies in 14 files.
3. **Enclosing-callable detection is textual.** Six wrappers were read to have ZERO callers, so
   they are dead code, not readers:
   - `RSlot` (diseasecomponent)
   - `SlotFn` (boatcomponent_route)
   - `ResolveSlotPtr` (tornadocomponent)
   - `ResolveFn` (game_main)
   - `RtDataExport` and `RtSaveDetail::AtomicExport` (saveload_internal.h; only `using`
     declarations name them)
4. **The `thread` column is per directory, not per call graph.** "both" means the directory is
   reached from `GameUpdateLoop::Load` on `Engine::Loader`: SaveRestoreOut runs every
   Serialize and factory `CreateNewObject` there, and so do the world build, UI construction and
   resource loads (B35_RULES 5.2, B36_RULES 5). The loader and the sim do not overlap: the sim
   starts after `s_worldBuiltLoop`. So a real CONCURRENT race needs code that main's `Draw`/UI
   also runs while the loader builds: runtime/src/{graphics,ui,video,collection}, dx11 buffers,
   the allocator doors. `code/Game` caches are reached by both threads, but in sequence.
5. **Mirror counts use literal keys.** A key reachable only through an unscanned spelling would
   move a row from "neither" into "one key". Every variable-key registration (§1.4) and every
   variable-key read (§3.1) was read by hand.
6. **The cache census regexes undercount wrapper-based fills.** `if (!s_x) s_x = R(…)` through a
   file-local lambda is counted only when the lambda is one of the 120 known wrappers.

## 5. What surprised me (code contradicting NEXT_SESSION §3a, a brief or a comment)

1. **`DEFINE_EXE_VTABLE` is not by-address.** The brief lists it as a data reader. It binds the
   compiler's `_ZTV…` label, and no retail vtable of GameUpdateLoop, MenuDialogLoopT,
   GameLoadDialogLoopT or StatGoal is registered. The readers are 2 printfs, 1 vptr compare and
   1 self-test.
2. **"1,480 rows with a reader" (§3a) is not a count of mirror readers.**
   - The table is read ONLY inside `RegisterRuntimeExport`/`RegisterRuntimeAddr`
     (`resolve_standalone.cpp:243,255`), at boot.
   - 1,335 rows have a registered key; 530 have a literal of either key anywhere in code; 16 are
     served by the mirror.
   - Deleting the mirror drops the `[RtPort] runtime registry: N exports, M addrs` line by 28
     exports and 273 addrs. That line needs a boot-log re-cut (question 6).
3. **`tools/rt_mint_gen.py --check` writes a file.** `main()` calls `write_export_addrs(pe)`
   before it looks at `--check`. `gate.sh:88` runs `--check` and does not restore the file.
   finishmerge/mergegate/stepclose run `git checkout -- standalone/rt_export_addrs.inc` after
   the generator. So the .inc is effectively frozen at commit 5ab628b5, and any hand retirement
   there would be reverted by the gate scripts.
4. **12 `AtomicExport` copies store every hit, pre-init hits included:**
   - combo, documenttext, edit, layout, list, page, progress, scroll, slider, tab and table
     descriptions (`store(p)`, not `store(scratch)`);
   - `AtomicByName` in userinterface_showdialog, which calls `ResolveExport` directly.

   Their comments claim element_common.h's contract, and B36_RULES §14.4 says that contract was
   fixed. Every `AtomicAddr` also stores a pre-init address hit: the address door has no §6
   rule, as rt_resolve_cache.cpp itself admits.
5. **`enum class Module` is declared in 150 files as `{Exe, Runtime, VideoDX11 = 2, COUNT = 3}`.**
   resolve.h declares `{Exe, Runtime, COUNT = 2}`. element_common.h:50 says "must stay
   byte-identical … the linker checks the two", but the linker does not check enums. This is an
   ODR mismatch, and it is also 150 private `ResolveAddr` declarations that do not include
   resolve.h. A grep of `#include "resolve.h"` misses them.
6. **`scene_watch.cpp:~435`** says the `AddDrawItems` body "is not ported … this lane does NOT
   register it". But `runtime/src/graphics/mesh.cpp:2048` registers
   `RtGraphics_MeshInstance_AddDrawItems` at 0x180100f20. So `IsMeshInstance` does answer.
7. **Seven dispatch slots escape `dispatch_slot_map.py --check`:**
   - `RegisterPlacePlayerSlot` ×1 and `RegisterLane3Slot` ×6. The tool's regex only knows the
     `RegisterDispatchSlot(` spelling.
   - The rule file says `--check` "validates every registration".
8. **79 guard functions set the flag BEFORE filling** (`if (f) return; f = true; …fills…`).
   `gameupdateloop_serialize.cpp:136` is one of them, and it runs on the loader. A second
   concurrent caller would read the statics while they are still null. This is benign only
   because the loader and the sim are sequential.
9. **The exe door itself is not thread-safe on a miss.** `NotImplementedStub_impl` mutates a
   static `unordered_map` (and the census map). `RecordResolveMiss` mutates a `std::map`, and
   `g_rtHits++` is a plain increment. Loader and main can both miss.
10. **`X_ResolveFuncs`:** 36 definitions (§3a is right). There are also 53 `*_InitFuncs` with the
    same shape; §3a does not count them.
11. **`CitizenAI_ValidateAndEnterState` null-skips any unregistered handler.** All 14 are
    registered today. A conversion to direct calls deletes the skip, and that is a behaviour
    change only for a handler that is still a stub (none is).

## 5.1 Question 3: caches, threads, boot order, the generator

**Spellings** (sites; how many are in loader-reachable dirs; defined where; thread-safety):

| spelling | sites | in loader dirs | defined | safety | replaced by |
|---|---:|---:|---|---|---|
| `RT_EXPORT_ONCE` | 242 (io 67, engine 57, save 29, graphics 27, object 23, ui 14) | 163 | rt_port.h:129 | lambda-local `static void*` (constant-init, no guard) → `RtResolveCached`: plain pointer store (a formal race), never caches null, never freezes a pre-init hit | a direct call; no cache |
| `RT_DATA_SLOT` | 4 | 1 | rt_port.h:144 | same, for STORAGE | a class static (question 4) |
| `RtResolveCached` direct | 35 (io 19, ui 12) | 33 | rt_resolve_cache.cpp:316 | same | direct |
| `std::atomic` Atomic* | 83 uses / 91 slots (ui 54) | 64 | element_common.h:75/84, mainloop_internal.h:49, saveload_internal.h:191/205, 15 file-local copies in 14 files | acquire/release, B6-5-safe; 12 copies freeze pre-init hits (§5.4) | direct |
| function-local `static T = resolve(…)` | 103 (graphics 41, io 18, collection 12) | 96 | — | magic static (thread-safe init: no `-fno-threadsafe-statics`); FREEZES the first answer, null included | direct |
| `if (!x)` / `x == nullptr` lazy fill | 197 (collection 46, io 32, Player 21, graphics 19) | 154 | — | plain pointer, retries a null, racy | direct |
| `X_ResolveFuncs` + `_InitFuncs` | 89 defs / 191 calls | all (code/Game) | per file | file statics + plain bool; lazily called from `CreateNewObject`, `Persist`, `Execute` (loader on a load, then main) | direct |
| `Ensure*()` bool guard | 195 | 147 | per file | same | direct |
| guard set before fill | 79 | 58 | per file | worst shape (§5.8) | direct |
| handle-struct fill `g_x.f = resolve` | 26 (Component 20, dx11 3) | 26 | — | plain | direct |
| EngineFuncsInit globals | 228 externs | — | engine_funcs.cpp:603 | bound once on main before any thread exists: no race | a declaration of the function |
| dispatch-slot storage | 51 | — | address_registry.cpp:762 + 3 copies | filled once on main at boot | the slot reader calls the function |

**Boot order** (`standalone/main.cpp:1310-1359`):

1. `OptionsInit`, then `ResolveInit`, then `RtSystemAllocatorSetCutoverArmer`.
2. **`RuntimePortInit`** (resolve_standalone.cpp:276): every `*PortInit`, then `RtDataMintInit`,
   then `RtResolveMarkPortInitDone`.
3. **`AddressRegistryInit`**: exe rows, plus dispatch-slot VALUES `ResolveExport`ed. It must come
   after step 2, or a slot freezes a null.
4. **`EngineFuncsInit`**: it reads exe slots and runtime doors, so it must come after both steps
   2 and 3.
5. `RuntimeFuncsInit`, then `Dx11SeamInstall`.

Other boot facts:
- Static initialisers resolve nothing: 0 namespace-scope initialisers call a door. The CRT-init
  work (`DEFINE_EXE_TYPE` records, `DX11_REGISTER_SLOT`) uses C++ addresses only.
- The resolves that happen before registration completes all run INSIDE `RuntimePortInit`:
  lane X's `LANEX_PROBE` through the caching door, and closure batch 2b's lazy Collection
  addresses. `RtResolveNoteEarly` and §6 exist for those.
- **A direct reference binds at link time, so nothing binds LATER than today.** The frozen
  dispatch-slot values, the EngineFuncsInit globals and the §6 early-hit problem disappear.
- The one behaviour hazard is a readiness test on a resolved pointer that is null TODAY:
  `tradeui_sa::DepsReady()`, `if (fn_Time_Ctor_Float)` in game_main, and the state switch's
  null-skip. Converting a null (STUB) target is impossible, and converting a non-null one
  changes nothing. So each such test becomes a titled commit only if its target is a stub.
- `RtResolveEarlySummary`, `RtResolveSlotAudit` and the `[RtResolve] 0 misses` line (which
  `uiroutes.sh` greps) die with `RtResolveCached`. Question 6 must re-cut them.

**The generator's retirement mechanism** (`tools/rt_mint_gen.py`):
- **Declaring a retired row:** a line `<mangled | 0xVA> <owner> [proof]` in any
  `tools/rt_mint_retired/*.txt`. There are 15 fragments, 479 lines. `#` starts a comment, and a
  key may appear only once.
- **Population check:** a key the population rules do not produce (not exported DATA) is an
  ERROR. Retirement is subtracted LAST (`:874-941`).
- **Refusal:** `scan_readers` (`:566`) over code/ runtime/ standalone/ engine/ dx11/
  tests/unit, comments stripped, only the two generated `.inc` files skipped. It refuses (exit 2,
  also under `--check`) on any of these:
  - `DATA_LITERAL` `"(\?\?_7|\?_)…"`, i.e. data-shaped names only;
  - a stringize-macro expansion of the one `"prefix" #param "suffix"` shape;
  - any hex literal (string contents blanked) inside the row's extent, where the extent is
    vtable length, 0x38 for a type record, 8 for a singleton, or `EXTRA_SIZES`;
  - a `runtime_census.py` consumer.
- **What `--check` verifies:** that `rt_data_mint_tables.inc` would regenerate byte-identical
  (exit 3 with the ENTERED/LEFT names), plus the refusals.
- **Blind spots:**
  - function names are not matched at all;
  - the hex-range test also counts VideoDX11 addresses in dx11/ (a false positive, the safe
    side);
  - names built by other macro shapes or at run time are missed;
  - `#if 0` code counts;
  - it needs the retail DLL at `C:\GOG Games\Banished\Runtime-x64.dll` and `pefile`.
- **For `rt_export_addrs.inc` at scale, four things would change:**
  1. `write_export_addrs` would take the retired set (today it dumps every named export, even
     under `--check`).
  2. Retired keys would be allowed to be FUNCTION exports (today that is an error).
  3. `DATA_LITERAL` would be widened to any mangled name, and the hex test narrowed to the exact
     entry address.
  4. The gate scripts' `git checkout` of the file would go.

  None of this is worth building. The table's only reader is the mirror, which needs 16 rows,
  so delete the mirror instead (block C8).

## 6. Proposal: blocks with disjoint file sets

The rows in §3.1 share files with inventories A/B (literal readers). Each block below OWNS its
files, so the rule sheet should merge any other inventory's rows in the same file into that
block.

| block | files | rows | first / notes |
|---|---|---:|---|
| **C1 game data tables** | `code/Game/Component/citizenai_candidatelist.cpp`, `code/Game/Tutorial/tutorialstep.cpp`, `code/Game/Game/geometry_serialize.cpp` | 18 | none; the state switch becomes direct calls. It is behaviour-neutral while all 14 handlers are registered; a stub handler would make it a titled commit |
| **C2 UI named constants** | `runtime/src/ui/{dialog, userinterface_close, userinterface_draw, userinterface_frame, userinterface_hover, userinterface_showdialog, element_vtable}.cpp` | 13 | merge with the UI literal-reader block; the Array `SetSize` addresses become Collection template calls |
| **C3 io named constants** | `runtime/src/io/filesystem_win.cpp` | 3 | — |
| **C4 dx11 imports** | `dx11/src/{devicedata, buffers, dx11_backend_access}.cpp`, `dx11/src/dx11_backend_access.h` | 23 | `Imports`/`RuntimeDeps` become direct calls; `Dx11ResolveRuntimeImport` dies |
| **C5 standalone instruments** | `standalone/{dlgctrl_route, modal_driver, scene_watch}.cpp` | 5 | decide delete vs direct per instrument; the `gfxmat` corrupt probe goes with its graphics-file owner |
| **C6 dispatch slots** | `standalone/{address_registry, reg_player_place, reg_unreg_lane1, reg_unreg_lane3}.cpp`, `tools/dispatch_slot_map.py` | 93 | LAST: only after every `*(fn*)ResolveAddr(Exe, 0x14015…)` reader (A/B, engine_funcs) is converted; each slot row then dies with its name row |
| **C7 registration tables** | `runtime/src/object/class_operator_delete.cpp`, `runtime/src/ui/ui_operator_delete.cpp`, `standalone/reg_rdata_float_consts.cpp`, `code/Game/Work/buildroad.cpp` (`:519`) | 202 | after their readers convert (question 4 owns the root operator deletes; the 25 floats become `constexpr` read directly by code/Game); the mint loop is question 4's |
| **C8 the mirror** | `standalone/resolve_standalone.cpp` (mirror half), `standalone/rt_export_addrs.inc` (delete), `tools/rt_mint_gen.py` (`write_export_addrs`, `--export-addrs-only`), `b3x_session/{finishmerge,mergegate,stepclose}.sh` (checkout lines) | 16 | FIRST, and TITLED: register the 16 missing doors explicitly (or convert their readers), then delete. The `[RtPort]` counts change by −28 / −273, which needs a boot-log re-cut |
| cache spellings | — | — | no block: each cache dies with the reader that owns it. Only if a cache must outlive its block should §5.4's 12 copies be fixed, as a separate titled commit. The `enum class Module` duplicates (150 files) go when the last `ResolveAddr` declaration in a file goes: a closing sweep |

The 16 rows C8 must keep serving:

| direction | keys |
|---|---|
| name → addr | `StringTable::GetIndexFromName`, `SpriteSheet::GetSpriteIndexFromName`, `UI::String` conversion `??B`, `Configuration::CurrentBuildId`, `Configuration::CurrentVersion`, `Configuration::GetVersionString`, `MemoryAllocator::AddNewMemoryRange`, `??0MemoryAllocator`, `NumberEditElement::SetValue`, `SpriteContext::AddLine`, `SpriteContext::SetScissor` |
| addr → name | `ExternalList::Find`, `InternalFactory::PersistObject`, `SpriteContext::Flush`, `SpriteContext::AddSpriteDraw`, `SpriteContext::Begin` |

The script prints them with their addresses.
