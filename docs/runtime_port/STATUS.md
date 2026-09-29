# Runtime port — status

Keep this to one screen. Update the closure block after every batch; never copy it into CLAUDE.md.

## Closure (2026-09-12 close, tools/cutover_closure, roots = census + every minted vtable slot)

| | |
|---|---|
| reachable functions from our call surface | 2,871 |
| unported | 128, all C runtime (our CRT covers them) |
| **engine functions to port (static lower bound)** | **0** (206 → 856 → 145 → 0 in one day) |
| minted vtables | 0 (ENDGAME B.3.9 retired all 152: every class has its compiler vtable); the mint is deleted (2026-09-28) |
| registry doors | NONE (2026-09-28, the registry teardown): nothing registers or resolves a body by retail name or address |

Worklist: `docs/runtime_port/closure/WORKLIST.md` (empty). Every function the DLL's call graph
reaches from our call surface OR from any minted vtable slot is ported, and every port is
reachable both by mangled name and by retail address. From here the closure cannot help:
what the gate finds next is state, not code — objects handed to our bodies by a route that
built them wrong, and callbacks stored in data.

## Endgame (2026-09-13)

- **B.4 (THE CAST METRIC) IN PROGRESS — paused 2026-09-29.** Scope set by the user: the dangerous
  casts go completely (any spelling of a read by offset or of a `void*` / integer receiver
  reinterpreted as a class), only safe casts stay; the differences from retail found on the way are a
  separate task (`docs/known_port_issues.md` 13, 14). Metric (`endgame_b/b4_session/castmetric.py`, every
  spelling summed): **46,226 to remove at block 0 -> 17,621** at `af21c719`; 31 gated merges, 0 problems;
  invariants unchanged. First pass merged: W0, T, runtime UI / IO / Graphics / Video, Component a-f,
  Dialog a-e, Work a-c, Game a, Action a, runtime Engine. Nothing is running or half merged. Not
  started: ACT-b, GAME-b, GAME-c, PLAYER, TUT, R-CORE, STANDALONE, tests; then the second pass, the
  closing wave (forwarders), the audit and the close. **How to resume: `endgame_b/NEXT_SESSION.md`.**
  Rule sheet `endgame_b/B4_RULES.md`, kit and gate table `endgame_b/b4_session/`.
- **REGISTRY TEARDOWN CLOSED (2026-09-28) — ENDGAME B.2 is finished:** nothing registers a body under a
  retail name or address and nothing resolves one. **Deleted:** `standalone/address_registry.{cpp,h}`, all
  127 `standalone/reg_*.cpp`, `resolve.h` / `resolve_standalone.cpp`, `runtime/funcs/rt_port.h` and
  `rt_resolve_cache.cpp`, `rt_export_addrs.inc` (4,926 rows), the mint and its generator, `engine/funcs/`
  (227 externs, `EngineFuncsInit`), the 51 dispatch slot cells, every `X_ResolveFuncs`, the registry
  instruments and their three flags, ten registry-only tools. A call is a C++ call through the declaring
  header (`code/Game/<Module>/exe_bodies.h` for the C bodies, with their retail address); a thing with no
  body is a `// PORT GAP 0x…` comment at its call site (189); the singleton cells are named objects
  (`GameUpdateLoop::_singleton`, `g_gameSettings`, `g_gameContainer`,
  `Input::Configuration::_singleton->_keyMap`); `System::MemoryAllocator` is a real class with one API and
  one instrument seat. What is left of boot init is `standalone/runtime_init.cpp` `RuntimeInit()` (five
  calls). Rule sheet `endgame_b/B3R_RULES.md` (R1-R11, run §7, what stays §7b, corrections §8), kit and
  documents `endgame_b/b3r_session/` (six INVENTORY files, BLOCK / W1 / STORAGE / W2 briefs, W1_AUDIT,
  GATE_TABLE, CLOSE_NUMBERS, `acceptance.sh`, the gate scripts). **Plan for the next step (B.4, the cast
  metric): `endgame_b/NEXT_SESSION.md`.** Known defects of our port, not yet fixed:
  `docs/known_port_issues.md`.
  **Shape.** Block 0 (six read-only inventories) → the rule sheet → W0 prerequisites (the mirror, the
  mint, one allocator API, declarations) → W1 "readers first": fourteen blocks by FILE plus a leftovers
  block converted every reader while the rows still existed → an independent read-only AUDIT → STORAGE →
  W2 "rows last", serial (runtime rows, `engine_funcs`, exe rows, the machinery) → close. **31 gate rows.**
  On every merge: new game `calls=5683 openFileFail=0`, tutorial `5773/12`, typetable == baseline
  (`b983f144`), resave == golden, 0 new warnings.
  **Numbers** (`b3r_session/CLOSE_NUMBERS.md`): 1,267 files, +25,701 / −85,391 lines; every door spelling
  → 0 (`RegisterRuntime*` 2,711, `RegisterExeAddr` 696, `ResolveAddr` 2,460, `ResolveExport` 205,
  `RT_EXPORT_ONCE` 242, `RtAddr<` 430); `NOT IMPLEMENTED` addresses 10 / 8 / 1 → **0** on every route;
  unit 919 → **918**, 0 failures; B.4 cast metric **29,064 → 26,488**; bridge markers 6,698 → 6,859;
  PORT GAP 149 → 189 (null arms made explicit; behaviour unchanged).
  **Behaviour changes, each titled** (list in CLOSE_NUMBERS): the largest visible one is that fire and
  disease now read the settings' auto-slow byte as retail does (they indexed the cell pointer), so an
  outbreak drops the game to 1x speed.
  **Found on the way, all fixed:** a deleted register function carried the `[ExtLoad]` exit hook (the
  gate now fails on a MISSING line); the audit found a false PORT GAP arm that stopped the KeyMap
  deleting destructor freeing its object; the step close's stress run crashed in
  `FindAvailableBuilding`, which read RAX after a `void` body through a cast (`known_port_issues` 12).
  **Step close:** RUN 2026-09-28: Mods / Quit / Options / Achievements exit 0; GoalDialog opens; tool
  routes exit 0; in-game save `SAVE OPERATION COMPLETE`; true release compile exit 0 (942 files) then
  `make clean`; clean rebuild exit 0, 0 new warnings, unit 918 / 0; gate on the clean-built exe 0
  problems; stub set 0; tutorial census citizen=16 herd=16; `sa-stress` CRASHED on the first run (above),
  exit 0 after the fix (update 1.81 ms/frame, recorded, not chased); `acceptance.sh` at target on all 17
  lines. Owed items: NEXT_SESSION §0.
- **NORMAL-VIRTUAL CUTOVER CLOSED (2026-09-28) — ENDGAME B.1 decision 2 is finished:** every class whose
  retail slot 0 is a deleting destructor declares `virtual ~T()` — **449 classes** under five roots
  (`Object::Base` 414, `Graphics::Geometry` 13, `IO::Stream` 9, `Engine::UpdateLoop` 9, `IO::ReaderWriter`
  4); deletion is `delete p`, `Object::Destroy` is `delete`, `Destruct` / `Reconstruct` call `this->~T()`;
  each root defines ONE class-scope `operator delete` (the global one is the C runtime's `free()`), the 41
  declared-but-undefined in-class ones are defined; the 127 dead vector arms are gone. GCC spends two slots
  on the destructor, so retail slot N >= 1 is compiler slot N + 1 — and **nothing in product code reads a
  flipping class's table by literal index** (~950 reads became by-name calls; `runtime/types/object/VSlot.h`
  for the few diagnostics that must look into a table). **Still on `DeletingDtor`, by decision: the twelve
  `Video::*Data` classes** (every live object wears dx11's table in retail order; they flip with the dx11
  seam). Exempt because retail has no destructor slot: the Thread family, `IO::Encoding`, the Controller
  interface, `PlatformChannel`, `IPathInfo`. Rule sheet `endgame_b/B3X_RULES.md` (X1-X6, run §7,
  corrections §8, left §9), kit and documents `endgame_b/b3x_session/` (five INVENTORY files, W0_AUDIT,
  W1_TESTS_DONE, GATE_TABLE, CLOSE_NUMBERS, ACCEPTANCE, `classes.py`, `flip.py`, `partc.py`,
  `acceptance.sh`, the gate scripts).
  **Shape.** Block 0 (five read-only inventories) → the rule sheet → W0 "by name first": seven blocks
  converted every slot-index read while nothing had shifted, then an independent audit (five reads no
  inventory had; 1,114 remaining sites classified; 0 wrong-root casts) → W1: one scripted flip, gated alone
  before the tests were rewritten, then the tests on top of it → one merge. **10 gated merges, 0
  problems**; on every one: new game `calls=5683 openFileFail=0`, tutorial `5773/12`, typetable == baseline
  (`b983f144`), resave 5/5 == golden, 0 new warnings. No behaviour change came from the flip itself.
  **Numbers** (`b3x_session/CLOSE_NUMBERS.md`): 959 files, +8,880 / −13,167 lines; unit 914 → **919**, 0
  failures (1,197 slot checks write RETAIL numbers through `tests/unit/slot_check.h`; the type decides the
  shift); B.4 cast metric **29,870 → 29,159**; bridge markers 6,116 → 6,397 (W0's casts on `void*`
  receivers); PORT GAP 165 → 149.
  **Behaviour changes, each titled:** SpriteCaptureUI appends the `".rsc"` literal (owed from B.3.9).
  Signatures corrected from retail's call sites: `Action::Enter(stack, time)`, `Work::Validate()`,
  `Work::Execute(citizen, context)`, `Work::Slot12(version)`; five call sites pass what retail passes where
  the raw call left a register unset.
  **Found on the way:** `partc.py` saw 190 of 1,205 runtime rows and a tenth of the tests; NEXT_SESSION's
  exempt list was partly wrong; Geometry objects were deleted through a cast to `Object::Base` (another
  root); `UI::DialogInstance::Update` passes `_dialog` to `Contains` where retail passes `_ui`
  (pre-existing, left for its own commit).
  **Step close:** RUN 2026-09-28 on `0419d0ce`: Mods / Quit / Options / Achievements exit 0; GoalDialog
  opens; tool routes exit 0; in-game save `SAVE OPERATION COMPLETE`; `sa-stress` exit 0 (update 1.73
  ms/frame, recorded, not chased); true release compile exit 0 (1,094 files) then `make clean`; clean
  rebuild exit 0, 0 new warnings, unit 919 / 0; gate on the clean-built exe 0 problems; stub set 0 new / 0
  gone; tutorial census citizen=16 herd=16; forced device loss COMPLETE with an empty crash log (killed at
  its time limit, as before — device-lost stays deferred). `acceptance.sh` at target on every line.
  Nothing owed.
- **B.3.9 CLOSED (2026-09-27) — B.3 is closed:** the migration scaffolding is gone. **The mint holds 0 vtables and
  0 `BaseTypeInfo`** (152 / 104 / 48 → 0 / 0 / 2): every Runtime class is a real C++ class with its COMPILER
  vtable, a concrete `DeletingDtor(unsigned)` with retail's arms (a `virtual ~T()` since the cutover above), a protected non-virtual dtor holding the old
  `??1` door's body and a real ctor holding the `??0` door's; every type record — the 104 Runtime ones and the
  310 exe ones — is a `DEFINE_TYPE` class static built at static init (chain **414**, ids identical,
  `b39_session/TYPETABLE_414.txt`). **Deleted:** the `RT_DELDTOR` table (135 rows) and its machinery; the three
  factory interpreters (FACTREST, FACTUI1, FACTUI2 — every class registers its own `CreateNewObject` /
  `CreateInPlaceObject` by retail name and VA); the 5 hand-built tables; every exe vtable `RegisterExeAddr`
  row; every type-id cell read (148 cells / 441 sites), 99 `g_TypeId_*` + 97 `g_tid_*` globals, 31 fill
  functions, 17 registrars; `InvokeAllRegistrations` / `REGISTER_TYPE` / `REGISTER_ABSTRACT_FACTORY` /
  `StandaloneFactoryTest`; 46 of 48 storage rows (class statics now), the D7 redirect, the cutover manifest and
  its tag file, the mint's vtable / type-info code. **Two storage rows stay minted**, as D10 said:
  `?_singleton@MemoryAllocator@System@@` (carry-over 1) and `?_disableHandler@Debug@System@@` (System::Debug
  unported). Rule sheet `endgame_b/B39_RULES.md` (D1-D10, B9-1..16; corrections §8, run log §9, what stays
  §10, NOT VERIFIED §11), file map `B39_FILEMAP.md`, kit `endgame_b/b39_session/` (BLOCK0 / BLOCK /
  CALLER_BATCH / CLOSING briefs, INVENTORY_A-D, ORDER_MEASUREMENT, GATE_TABLE, TYPETABLE_414, CLOSE_NUMBERS,
  gate.sh, mergegate.sh, finishmerge.sh, uiroutes.sh, toolroutes.sh, devlost.sh, stepclose.sh, acceptance.sh,
  codegrep.py, partc.py). Plan for the next step (the normal-virtual cutover): `endgame_b/NEXT_SESSION.md`.
  **Shape.** Block 0 (four read-only inventories: mint vtables, doors / `RT_DELDTOR`, type ids / reflection,
  storage; plus a MEASURED answer that the type table is order-independent — ids are `~CRC32(name)`) → one
  owner: rule sheet + retirement mechanism (`tools/rt_mint_retired/<block>.txt` fragments, the generator
  REFUSES a retired row that still has a reader) + pilot (four dead rows, PickingMesh, SunLight) → W0 COORD-0
  → W1 namespace blocks UI-A, UI-B, UI-D, IO, VID, GFX, ENG, SYS, OBJ, EXE-1 and caller batches C1-C5, CB →
  W2 IO2, GFX2, ENG2, RF (the exe fold, run alone), VS, caller batches CG1-CG3, storage sweeps SS1 / SS2,
  type-id residue TR → W3 TI-REST, TI-UI (the Runtime folds), OBJ3 (Object's four bases, `Base` last) → W4
  CLOSE. 31 merges, each gated by the coordinator (`b39_session/GATE_TABLE.md`): 0 problems after two
  typetable re-cuts made in writing (416 → 415, the duplicate `Internal`; the order-free 414-record format)
  and two coordinator errors fixed in their own commits (a stale generated manifest; a merge commit that did
  not build — the line-matching `deleting_dtor.cpp` resolver, replaced by a structural one). Invariants held
  on every merge: new game `calls=5683 openFileFail=0`, tutorial `5773/12`, resave 5/5 == golden, typetable ==
  baseline.
  **Numbers** (`b39_session/CLOSE_NUMBERS.md`): B.4 cast metric **32,057 → 29,870** (runtime −1,126, code −551);
  bridge markers 6,627 → 6,036; ctor / dtor doors registered by name 206 / 181 → 99 / 37; `rt_export_addrs.inc`
  rows with a reader 2,794 → 1,480 (of 4,926); `RegisterExeAddr` 1,007 → 694 (vtable range 6 → 0, data
  142 → 45); `address_registry.cpp` 1,617 → 1,243 lines; `reg_*.cpp` 145 → 127; `engine_funcs` 414 → 227
  externs (4,292 + 2,576 → 3,036 + 1,652 lines); `RT_DATA_SLOT` sites 422 → 4, `ResolveExport` 443 → 205,
  `RT_EXPORT_ONCE` 391 → 242, `ResolveAddr(Exe` 2,690 → 2,185, `ResolveAddr(Runtime` 386 → 275; literal
  `GetTypeIdByName("…")` 17 → 0 and `ResolveTypeIdByNameLazy("…")` 31 → 0; `DEFINE_EXE_VTABLE` 309 → 4 used;
  unit tests 736 → 914, 0 failures. The §6 acceptance (`b39_session/acceptance.sh`): every line 0 on code.
  **Behaviour changes, each titled and gated with resave ×5** (`git log --grep=behaviour ab1e0846..`): the
  pilot's one `~SunLight` body and the exe-built SunLight wearing its class table; the exe ImageBuffer's
  slots 1-8 running retail's members; Load Game's `GameUpdateLoop` name a null String as retail; IO slot-0
  `flags & 2` arms are retail's vector delete (FileStream family, readers / writers, MemoryStream,
  CompressedFileStream, CompressedMemoryStream — were aborts / table rows); manager-built
  AchievementInstances wear the class table; `~EdgeMeshInstance` ported (its row called a NULL `~Class`); the
  one `Internal` record and then `Base` / `External` / `CompiledExternal` / `ExternalList` / the two compiler
  records are the classes'; `Particle::_rng` seeded at CRT init; `Engine::Loader` built by its class at CRT
  init, not by the mint (gated alone); exe type records built at static init (ids read 0xFFFFFFFF, not 0,
  before `Factory::Startup` — no reader there, confirmed by a run) and the chain 414; `ExternalPtr<Toolbar>`
  validated against `Toolbar::TypeId()` (the global was a vtable slot); CB's retail-read sites: the
  citizen / follow panels find `CitizenAIComponent` (the Action line shows the activity), the pasture finds
  `AutoPickupComponent` and records new piles, the Healer / Well / Happiness and three Persist arrays run
  their IBP type check, five Action `+8` name reads latch.
  **Found on the way** (§8): `System::Thread` has no deleting dtor (two slots, `Run` first); INVENTORY_C was
  wrong on three by-name `g_tid_` cells (two never filled: their readers saw 0); the C1-C5 batches missed
  ~900 `g_TypeId_*` global reads (CG1-CG3 added); `g_TypeId_ToolbarExternal` was a vtable slot; SS1 missed
  seven by-address storage readers (the close converted them); `EXTRA_SIZES` of retired rows are NOT stale
  (they size the reader refusal); the acceptance's type-id grep could not work (fixed: `acceptance.sh`).
  **Device-lost, pre-existing and reproduced on the pre-step baseline** (GATE_TABLE note; the USER's decision,
  `DEVICE_LOST_LOADER_RACE.md`): a forced loss in the front-end faults in `MainLoop::OnVideoError` on a null
  `SaveLoad::_singleton`; during the world build it is the documented loader race; in game the round trip
  COMPLETEs but real `DXGI_ERROR_DEVICE_REMOVED` recoveries then repeat. Step 9's device-lost gate (Present
  #40000): COMPLETE, crash log 0, after every seam-touching merge.
  **Deliberately left** (B39_RULES §10): the normal-virtual cutover (next); the registry teardown
  (`rt_export_addrs.inc`, `address_registry.cpp`, `engine_funcs`, the 5 non-type-id cells, the two minted
  rows, `MemoryAllocator`); the dx11 seam; `ExternalPtr` exe door sites; the PORT GAP message targets
  `kCitizenAIMessageTargetTypeId` / `kHappinessMessageTargetTypeId` (receivers unported); AutoPickupComponent's
  uninstalled Startup; the EventNotify / AutoSave dialog construction skips; the `InitFields` seams the unit
  tests' recording stand-ins define; `spritecaptureui.cpp:200`'s raw read of retail `.rdata` 0x14015d424.
  **Step close:** RUN 2026-09-27 on a699884d + the flag commit. Mods route with `.pkm` exit 0; Quit / Options /
  Achievements exit 0; GoalDialog from a tutorial pause menu opens (1 `OpenGoal` line, killed at the timeout as
  designed); tool routes placement / road / removal exit 0; in-game save `SAVE OPERATION COMPLETE`, crash log 0;
  true release compile exit 0 (1,093 files) then `make clean`; clean rebuild of debug / perf / unit exit 0, 0 new
  warnings, unit 914 / 0 failures; full gate on the clean-built exe 0 problems (new game 5683/0, tutorial
  5773/12, Load Game 4758, resave 5/5, typetable == baseline 428 lines b983f144); `NOT IMPLEMENTED` stub set 0
  new / 0 gone; tutorial `[TownCensus] LOAD exit` citizen=16 herd=16; `acceptance.sh` at target on every line.
  **FOUND AND FIXED AT THE CLOSE:** `sa-stress` CRASHED (exit 2) -- the -O2 exe corrupted the heap in
  `~MeshInstance` at the menu shutdown, on the plain new-game route too; the -O0 exe never did, so no per-merge
  gate saw it. Cause: GCC's lifetime dead-store elimination deletes the field-nulling stores of a ported
  destructor (`ArrayDtorSited` -> `~Array`), and step 9's real classes free a member array in the destructor body
  and again in the compiler's member destructor. Fix: `-fno-lifetime-dse` in `SA_CXXFLAGS` / `UNIT_CXXFLAGS`
  (Makefile banner), MANDATORY like `-fno-strict-aliasing`. After it: `sa-stress` exit 0, 6,000 frames, clean quit
  (sim 0.19 ms/iteration, update 1.92 ms/frame; recorded, not chased), and the gate gained a `newgame_perf` route
  on the -O2 exe. **NOT RUN TO COMPLETION:** the forced device-lost route -- the harness killed it for low system
  memory with the game process at 6.6 GB (the repeated real device recoveries of `GATE_TABLE.md`); last clean
  round trip is the b39-TIUI merge. The user saw the terrain texture slide with the camera in that run; a plain
  new game does not show it (two screenshots at different pins). Both belong to the device-lost decision.
- **B.3.8 CLOSED (2026-09-26; step close completed 2026-09-27):** every exe-phase class in
  `code/Game` is a real C++ class with a COMPILER-GENERATED vtable, on the Runtime bases steps 3-7 made
  real: 310 of 310 reflected exe classes (components, descriptions, actions, works, tools, dialogs, panels,
  goals, scenario, tutorial, world externals) + the five loops, the controller members, the exe Geometry /
  Particle / Thread subclasses — each with a real ctor, `DeletingDtor(unsigned)` + protected dtor chaining
  its base, `DEFINE_EXE_TYPE` (literals exact), header `code/Game/<Dir>/<Class>.h`, all agreeing with the
  retail dump (parent, sizeof, slot count). **Hand-built vtables 306 → 3, none serving an exe class.** Rule
  sheet `endgame_b/B38_RULES.md` (D1-D10, B8-1..14, corrections §8/§11, run log §9, what stays §10), retail
  dump `B38_SLOTS.md`, file map `B38_FILEMAP.md`, kit `endgame_b/b38_session/` (BLOCK_BRIEF,
  CALLER_BATCH_BRIEF, CLOSING_BRIEF, INVENTORY_A/B, SEAM_AND_FILEMAP, CALLER_SURFACE, GATE_TABLE,
  CLOSE_NUMBERS, gate.sh, stepclose.sh, toolroutes.sh, `ghidra/Step8Appendix.java`). Plan for the last B.3
  step: `endgame_b/NEXT_SESSION.md`.
  **Found at block 0, retail-proved twice: there is NO multiple inheritance in the exe** (ENDGAME B.1
  decision 2 corrected): the "56 `ControllerComponent` types" are 29 classes embedding a controller MEMBER
  (9-slot interface 0x14015f070) at a per-class offset (+0x28 … +0x88); slot 17 returns its address.
  **Shape.** Block 0 (four read-only inventories + the Ghidra EXE dump) → one owner: rule sheet + a PILOT
  (Component, ControllerComponent, Description, Well leaves) → 14 class blocks on worktrees (RB Runtime
  bases' real ctors; K1 80 descriptions + ComponentEntity; ACT 18; WRK 23; TOOL 12; CIT 19; DLG 19; PNL 42;
  LOOP loops + 11 externals + Tornado/geometry/pathfinder; VIS 25; MI-A 13; MI-B 15; GOAL 27) → 7 caller +
  fold batches (CB1a/b, CB2-CB6: raw dispatches → virtuals, by-name type ids → `X::TypeId()`, delegating
  members' bodies moved in, loop fields named) → one closing owner. Every merge gated by the coordinator
  (`b38_session/GATE_TABLE.md`): 0 problems except four, each resolved — (1) CIT: the StateMachine lazy
  linker called controller slot 17 on every sibling, safe only while every component had a 48-slot padded
  table (fixed on main: ask `IsDerivedFrom(ControllerComponent)` first, as retail); (2) DLG: type table
  312→311 (AutoSaveDialog's duplicate registration removed, intended); (3) LOOP: every route crashed —
  the splash's newly-running base dtor evicted the menu scene the standalone had loaded under the splash's
  factory bit (fixed at the root: mark the menu loop's bit as `Loader::Run` would); merge undone, re-merged
  after the fix; (4) LOOP: resave DIFF — 128 main-menu UI objects no longer leaked (retail frees them), the
  golden re-cut `757e48a1` → `dd3893af` in its own commit (758597e2). New invariants: new-game `calls=5683
  openFileFail=0`, tutorial `5773/12`.
  **Numbers** (`b38_session/CLOSE_NUMBERS.md`): B.4 cast metric **36,450 → 32,057** (first step where it
  FELL: code −4,129) with 621 `// B.3.8 bridge`; `RegisterExeAddr` 1,390 → 1,007 (exe vtable rows 199 → 6);
  `REGISTER_TYPE` 289 → 3 / `DEFINE_EXE_TYPE` 296; `GetTypeIdByName("…")` 535 → 17; delegating members
  401 → 12; `engine/types` headers 147 → 28; unit tests 516 → 736, 0 failures; 142 PORT GAP + 49
  STANDALONE DEVIATION markers (the honest ceiling: retail bodies the tree still stubs, 38 of them leaking
  dtors). **Behaviour changes, each titled and gated with resave x5:** base dtors now run where hand-built
  slot 0s skipped them (panels, dialogs, loops, descriptions, PastureWork …); retail ctor defaults and
  dtor frees (CIT, ACT, MI-A, MI-B, DLG, TOOL); D9 port bugs (Action base-table stamp, ComponentEntity's
  own table, Tool/DragTool null restamps, EditTool/PathTool slot 13, the External triad, controller slot
  1/8, AbsorbResource / MarketLocation / MultiObject members, ClearArea/ConsumeProduce OnCancel, Nomad
  slot 7, TimeGoal meetable, Goal/Scenario null slots); `BANISHED_LIVE_TUTORIALINSTANCE_VT` retired.
  **Step close** (`b38_session/stepclose.sh` on `e24d1e7c`): Mods route with `.pkm` exit 0; front-end Quit,
  Options, Achievements exit 0, `[RtResolve] 0 misses`, 0 TRAP; **GoalDialog from a tutorial game's pause
  menu** (owed since B.3.6) opens with no crash; placement / road / removal tool routes exit 0; in-game save
  SAVE OPERATION COMPLETE, crash log 0; `sa-stress` exit 0 (update 1.85 ms/frame vs baseline 1.92, p95
  2.50 vs 2.40, draw 1.17 vs 1.38 — recorded, not chased); clean rebuild of debug / perf / unit from an
  empty obj dir exit 0, and on it 0 new warnings (debug, perf), 736 tests 0 failures, dispatch 0, manifest
  OK. **Step close COMPLETED 2026-09-27** (the first run was stopped by the harness for low system memory; the owed
  items re-run on `ab1e0846`, `b38_session/GATE_TABLE.md` tail): true release compile from a clean obj dir exit 0
  (1,124 files, 8.3 MB exe), `make clean`, clean rebuild of debug / perf / unit, then the full gate on the
  clean-built exe: 0 problems, 0 new warnings, 736 tests, dispatch 0, manifest OK, mint tables UP TO DATE
  (152/104/48), new game `calls=5683 openFileFail=0`, tutorial `5773/12`, Load Game 4758, `[RtResolve] 0 misses`,
  0 TRAP, resave 5/5; `NOT IMPLEMENTED` stub set identical to the step-8 baseline (23 hits); the tutorial
  `[TownCensus] LOAD exit` line prints (citizen=16 herd=16). Nothing owed.
- **B.3.7 CLOSED (2026-09-26):** `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`,
  `Picking` and `Application` are real C++ classes under retail's names — 15 polymorphic
  (`UpdateLoop`/`LoaderLoop`/`DialogLoopT`/`DialogLoaderLoopT` as their OWN root, `Loader` and
  `Audio::Stream` on the real `System::Thread`, `Achievement`/`Goal`/`SoundEffect`/`PointList`/
  `PickingMesh` on `Object::External`, `SoundBuffer` on `CompiledExternal`, `AchievementInstance`/
  `GoalInstance` on `Internal`, `IPathInfo`) + 17 non-polymorphic (`MainLoop` 0xa0, `Engine` 0x6f8
  PINNED, `Application` 0x30, `SaveLoad` 0x90, `AchievementManager`, `GoalList`, `Audio::Scene`/
  `Configuration`/`Device` 0x1298, `Input::KeyMap`/`Configuration`/`Device` 0x1200, `Picking::Scene`,
  `Pathfinding::Scene` 0x90 …) + 19 nested value types, in `runtime/types/<ns>/` (list with parent,
  slots, sizeof in `endgame_b/b37_session/CLOSE_NUMBERS.md` section 6: 14 of 14 checkable agree
  with the retail dump). Bodies moved onto members, callers converted, dead doors deleted. 14 merge
  gates + the close, each 0 problems (table in `B37_RULES.md` section 11); main `96b03474` ->
  `db990145` + the close docs. Rule sheet `endgame_b/B37_RULES.md` (B7-1..13, corrections in 10 and
  14, run log 11, what stays 12), retail dump `B37_SLOTS.md` (DLL + EXE modes), file map
  `B37_FILEMAP.md`, kit `endgame_b/b37_session/` (BLOCK_BRIEF, CALLER_BATCH_BRIEF, CLOSING_BRIEF,
  EXE_SUBCLASSES, CALLER_SURFACE, `ghidra/Step7Appendix.java`, CLOSE_NUMBERS, stepclose.sh). Plan
  for step 8: `endgame_b/NEXT_SESSION.md`.
  **Shape.** One question pass from the retail DLL and exe (30 classes / 618 exports; every vtable
  terminated on the next symbol, sizeof from slot 0, parents from literals and base-ctor calls,
  31 retslot exports, by-value-argument prologues, the five exe loop tables and the Goal family read
  in the exe) plus a file map (six disjoint blocks, 116 files, ONE fragment body) and an exe-subclass
  + caller-surface report (12 exe classes on 14 tables, 25 qualified-call sites, 451 call sites by
  binding); wave 1 blocks B1 Engine+Application, B2 Save, B3 Achievement (+ the exe Goal family's
  qualified calls), B6 Pathfinding+Picking; wave 2 B4 Audio, B5 Input, fragment F; caller round 1
  K1 (code/Game/Dialog), K2 (rest of code/Game) for the merged namespaces, round 2 K3 (Input),
  K4 (Audio), K5 (standalone/engine/runtime leftovers/dx11); one closing owner.
  **Numbers** (`CLOSE_NUMBERS.md`). `engine_funcs`: 92 globals bound to the eight namespaces BY
  INITIALISER (82 doors + 10 storage; the inventory's 63 was name-anchored) -> 17 (8 ctor/dtor
  doors still called + 9 storage globals; `g_pEngineSingleton` folded into `g_pMainLoopSingleton`,
  the same symbol; `g_pKeyMapObjectExref` renamed to what it is). Door CALLs 233 -> 30 (all
  ctor/dtor); `ResolveExport` sites 177 -> 49 (registry rows, storage, ctor/dtor doors);
  `RT_EXPORT_ONCE` 57 -> 13; exe import slots 47 -> 6 (storage exrefs); Runtime hex 21 -> 6; live
  hex resolve lines tree-wide 66 -> 12; hand-inlined vtable calls 2 -> 0. 433 `// B.3.7 bridge`
  (code 268, runtime 141). Census: 138 rows left `called` (214 -> 76) because member calls resolve
  nothing. Unit tests 462 -> 516, 0 failures. B.4 cast metric **36,449** lines (+224: code +261
  bridges, engine -93 deleted resolvers, runtime -33). Doors still registered: 285 names (238
  one-line qualified delegations, 24 dtors, 22 ctors, 1 `operator=`) + 16 mint, 8 `_typeInfo`, 12
  `RT_DELDTOR` rows and the 10 singleton storage slots (`SaveLoad::_singleton` has NO manifest row)
  — end of B.3. dx11's import table is down to the `appSingleton` data row.
  **Step-close routes:** merged-tree gate 0 problems (516 tests, 0 new warnings, newgame/tutorial/
  loadgame clean, typetable == baseline, resave 3/3; Save merged with resave 5/5); Mods route with
  `.pkm` mounted exit 0; front-end Quit, Options (id 4 -> Quit), Achievements (id 8 -> Quit) exit 0,
  all `[RtResolve] 0 misses`, 0 `[RtMint] TRAP`; an in-game save through `Save::SaveLoad` (resave
  gate file mode: SAVE OPERATION COMPLETE, 8.55 MB, crash log 0 — the whole-file diff is that
  mode's expected result); `make sa-stress` exit 0, clean quit (800 citizens x3, speed 10, 6000
  frames: update 2.01 ms/frame vs baseline 1.92, p95 2.87 vs 2.40 — recorded, not chased, per the
  B rule; the B.3.6 close read 1.97 / 2.79; draw 1.21 vs 1.38); the new-game log has the same
  `NOT IMPLEMENTED` stub SET as the baseline (39 -> 33 hits); `Audio::Device`/`Stream` construct on
  the plain new-game route (Stream ctor=1, Stream::Run calls=2, Scene::Update 799). True release compile from a clean obj dir exit 0 (1,148 files, 7.8 MB exe), `make clean`, clean rebuild of debug/perf/unit, then the full gate again on the clean-built exe: 0 problems, 0 new warnings, 516 tests, typetable == baseline, resave 3/3.
  **Found on the way, all retail-backed** (`B37_RULES.md` 10 and 14). (1) `Achievement::Achievement`
  is polymorphic (9 slots, 0x58, `??_7Achievement@0@6B@` — the back-reference the inventory's regex
  missed; 16 mint rows not 14) with the exe's `Scenario` on it; `GoalList` is a 0x10 value type.
  (2) The four loops are NOT `Object::Base` (slot 1 `Startup`); `Goal` slot 8 is `CheckGoalState`
  (0x18009f060 tail-calls `[RAX+0x40]`), not `Construct`; `GameUpdateLoop` has 12 slots, not 22;
  retail's Scenario table has 9 slots (tree 13), Goal tables 11 (tree arrays run into the next
  table). (3) `System::String` BY VALUE arrives by HIDDEN POINTER although it is 8 bytes;
  `ExternalPtr` and `System::Time` in a register (B7-5); every copy ctor read is DEEP. (4)
  `Application` has 27 exports (`@Application@1@`), not zero; `PointList` is LIVE
  (`InteractDescription::Serialize` loads one every game and read +0x30/+0x38 raw — now named
  fields); `Pathfinding::Scene`/`IPathInfo` dead in retail. (5) `Engine::Engine` 0x6f8 pinned by
  `??4Engine@0@`; `Audio::Device::PlatformChannel` is the "unnamed" voice-callback record;
  `SoundEffect` +0x20/+0x24/+0x28 and `SoundBuffer` +0x48/+0x50 named; `KeyMap` +0x00 is
  `_suspendCount`; `ModInfo` 0x20; `UpdateList` writes +0x2c (the old header said not). (6) Binding
  defects: `fn_KeyMap_Released` was bound ONLY to an unregistered exe slot (always null);
  `finishtutorialinstance.cpp` resolved `Audio::Configuration::Save`'s slot as `IsDerivedFrom`;
  `citizen_assignworkplace.cpp`'s five Audio emitter slots and `Vector3::operator==` (0x140152a70,
  still unregistered) were never registered; `g_pEngineSingleton` was bound to `MainLoop`'s
  singleton; five `MainLoop` members and `Input::Device::PlatformUpdate` were filed "not exported"
  while exported. (7) Tooling: `gate.sh`'s argument order is `<label> <n> [mode]`; the K1/K2 hex
  sweep dropped ordinary class-scoped names and K3's dropped back-references — anchor on the
  demangled OWNER; `warn_ab2.sh` misreports when a header loses a name (compile the base in its own
  worktree); three block agents went idle on background lock watchers (foreground only).
  **Behaviour changes, each gated and retail-faithful:** the drag tool's key-release paths run
  (K3); citizen emitter teardown/volume run (K4, own commit); the tutorial-instance type check runs
  (K2, per retail 0x1400d7bf0); MenuDLT `Shutdown` runs retail's full 12-step teardown (B1,
  carry-over 11 CLOSED); `PointList` and `Audio::Scene` allocate through the `rtcoll` door.
  **Deliberately NOT converted (reasons in `B37_RULES.md` 12):** no polymorphic class has a C++
  ctor; ctor/dtor doors, mint/`_typeInfo`/`RT_DELDTOR` rows and the singleton storage (end of B.3);
  the exe-phase subclasses (five loop tables, Goal family, Scenario, the unregistered
  `AchievementInstance` table) and the `RtEngine::` loop PODs they embed (95 `m_` uses in 7
  files) — step 8; 105 exported members declared and not ported (`Pathfinding::Scene` 15 dead,
  `MainLoop` 12, `Audio::Device` 11, `Input::Device` 10 …) + 8 undefined dtors; `KeyMap`'s split
  fields vs `Collection::Set`; the 19 `rtpickgrid::` free functions; four defined non-polymorphic
  dtor doors still called from `code/Game` (`Audio::Scene`, `Picking::Scene`, `SaveLoad`,
  `SavedState` — decide with their holders in step 8); the `GoalDialog` and Options-change routes
  still owed (the menu driver has no `OptionsDialog` token).
- **B.3.6 CLOSED (2026-09-26):** `UI` is real C++ classes under retail's names — 68 polymorphic
  classes (28 `*Description`, 28 `*Element`, the six-class Dialog family, `CharacterSet`, `Cursor`,
  `Font`, `SpriteCapture`, `SpriteSheet`, `StringTable`) plus the value types `UserInterface`
  (0x128), `SpriteContext` (0x88), `Sprite` and `String` (0x10 each) in `runtime/types/ui/` (list
  with parent, slot count and sizeof in `endgame_b/b36_session/CLOSE_NUMBERS.md` section 6: 68 of
  68 agree with the retail slot dump) — bodies moved onto members, callers converted, dead doors
  deleted. 110 commits (83 non-merge), main `dc647869` -> `0caf01c3` + the close docs; 312 source
  files, +31.1k / -18.3k lines. Rule sheet `endgame_b/B36_RULES.md` (B6-1..11 and the dated
  corrections in sections 10 and 14, the run log in 11, what stays in 12), retail slot / literal /
  ctor-chain dump `B36_SLOTS.md`, file ownership `B36_FILEMAP.md`, session kit
  `endgame_b/b36_session/` (BLOCK_BRIEF, CALLER_BATCH_BRIEF, EXE_SUBCLASSES, POD_TREE_AUDIT, the
  Ghidra dump script `ghidra/UiAppendix.java`, CLOSE_NUMBERS). Plan for step 7:
  `endgame_b/NEXT_SESSION.md`, map `endgame_b/STEP7_INVENTORY.md`.
  **Shape.** One question pass from the retail DLL (hierarchy from the 136 `_TypeName` /
  `_ParentTypeName` literals, all 68 vtables terminated on the next `??_7`/`??_R` symbol, 68
  `sizeof` from the slot-0 deleting dtors, the 76 by-value-returning exports, every ctor's stamp
  chain) plus a parallel file map, into a committed rule sheet; wave 1 blocks A (`Element` +
  `ElementDescription` bases), B (Dialog family), D (`Sprite`, `String`, the resources,
  `SpriteContext`); wave 2 the 27 leaf Description/Element pairs in five clusters C1-C5 (intermediate
  leaves first) and E (`UserInterface`); two fragment passes F2 (dialog side) and F1 (widget side);
  caller batches K1 (townhall/trade), K2 (the exe dialogs), K3 (the panel UIs), K4 (everything
  outside `code/Game/Dialog`), then round 2 (K1r2, K3r2) after the fragment passes defined the last
  members; one closing owner. Every merge gated by the coordinator: 17 gates + the close, each
  0 problems (table in `B36_RULES.md` section 11).
  **Numbers** (`CLOSE_NUMBERS.md`, every unit stated there). `engine_funcs`: 155 globals bound a
  `@UI@@` symbol BY INITIALISER (132 door pointers + 23 type-info globals; the inventory's
  name-anchored 118 missed exe-slot and duplicate bindings); 126 deleted whole, 7 door pointers
  left (six ctor/dtor doors + one `Save` door whose name carries `@UI@@` in a parameter) and 22
  type-info globals. Door calls through `engine_funcs` in `code/Game` 1,472 -> 65 (61 code-only,
  all ctor/dtor); `ResolveExport("…@UI@@…")` sites 185 -> 41 (registry rows, `_typeInfo`/`??_7`
  reads, ctor/dtor doors); `RT_EXPORT_ONCE` 79 -> 11; `RT_DATA_SLOT` 147 -> 70; exe import slots
  onto UI 59 -> 29; exact hand-inlined vtable calls in `code/Game/Dialog` 14 -> 9 (diagnostic slot
  reads, not calls). 2,083 `// B.3.6 bridge` casts (code 1,492, runtime 559). Census UI
  1625 total / 134 called / 78 ported (was 329 / 214: `called` counts what still resolves by name
  or address; member calls resolve nothing). Unit tests 358 -> 462, 0 failures. B.4 cast metric
  (lines): **36,225** (code 25,282 · runtime 6,885 · standalone 2,907 · engine 609 · dx11 168 ·
  tests 374), UP 1,400 from 34,825 while adding the 2,083 bridges; `engine` fell 147 and `runtime`
  124. Doors still registered: 809 `@UI@@` names (590 one-line qualified delegations, 67 ctors,
  69 dtors, 74 factory statics, 7 new/delete, 2 literals) + the 68 mint, 68 `_typeInfo` and 67+1
  `RT_DELDTOR` rows (end of B.3). Step-close routes: merged-tree gate 0 problems (462 tests, 0 new warnings,
  newgame/tutorial/loadgame clean, typetable == baseline, resave 3/3); Mods route with both `.pkm`
  mounted exit 0, front-end Quit, Options (id 4 -> Quit), Achievements (id 8 -> Quit) exit 0, all
  `[RtResolve] 0 misses`, 0 `[RtMint] TRAP`; the new-game run reaches TEN FEWER
  `NOT IMPLEMENTED` exe import-slot stubs than the baseline (UI doors now called as members) and
  no new one; `make sa-stress` exit 0, clean quit (800 citizens x3, speed 10, 6000 frames: update
  1.97 ms/frame vs baseline 1.92, p95 2.79 vs 2.40 — recorded, not chased, per the B rule; draw
  1.19 vs 1.38); true release compile from a clean obj dir exit 0 (1,148 files, 7.8 MB exe),
  `make clean`, clean rebuild of debug/perf/unit, then the full gate again on the clean-built exe: 0
  problems, 0 new warnings. NOT a finding: injecting front-end menu id 10 (Goals) faults in
  `GoalDialog` on a null game world — `buttonGoal` exists only in the pause-menu `.rsc`, so retail
  cannot reach that path from the front end (menu-driver skill corrected); the GoalDialog route is
  owed from a scenario game's pause menu.
  **Found on the way, all retail-backed.** (1) **An `InternalPtr<T>` ARGUMENT by value arrives
  by HIDDEN POINTER** (B6-11; `Element::Update`'s caller `LEA R9,[RBP+0x50]`), as does `UI::String`,
  `UI::Sprite` and every >=16-byte class; `ExternalPtr<T>` by value is a bare register pointer.
  `Element::AddChild` was the one declaration that missed it (0x180115df0 dereferences RDX) —
  fixed at the close. (2) An ABSTRACT class cannot be an alias target for an EMBEDDED POD: the
  `Element` POD moved to `RtUI::Element`, the engine dialog headers became aliases of the `RtUI`
  PODs, not the classes. (3) `SpriteContext` and `UserInterface` are NOT trivially copyable
  (`Array<T>` members with a deep copy ctor / destructor exported); `SpriteContext` is 0x88,
  `Element::UpdateState` 0x30, `EditElement` 0xf8, `LabelElement` fields +0x100..+0x12c; 42 layouts
  that block 0 had only sized were pinned from their ctors. (4) `ElementDescription` +0x40 is the
  ALIGNMENT seed, not an element type; `NumberElement` extends `Element` (the header said
  `TextElement`); `ImageDescription`'s applier, `ButtonDescription::CreateInstance`,
  `ComboElement::UpdateText` and the three `SpriteContext` members are EXPORTED (old banners said
  not); `??0CheckDescription`/`ComboDescription`/`ClipRegionDescription`/`NumberDescription` exist.
  (5) Retail slips transcribed faithfully: `GraphElement::SetYDivisions` writes the SAME field as
  `SetXDivisions` (+0xe4); the ported `ComboDialog::OnUpdate` passed `this` only where retail
  passes `(mx, my, _ui)` — a port bug, fixed. (6) `ToolTipDialog`'s parent literal is
  `"UI::DialogInstance"` (same namespace, still qualified: B6-7 — copy, never derive). (7) Six
  `g_TypeId_*` globals were bound to a DIFFERENT class than their name (`SliderElement` held
  SpinnerElement's id, `SpinnerElement` NumberElement's, `ObjectInternal` Element's,
  `UIElementBase` ButtonElement's, `ToolbarElement` CheckElement's, `g_pExternalTypeId` Dialog's);
  renamed to the bound class. (8) `element_common.h`'s `AtomicExport` froze pre-init resolves that
  `RtResolveCached` refuses to cache; fixed to the same shape. (9) mingw-w64 emits typeinfo as a
  COMDAT per TU, so a leaf's `TypeName` can be defined while the base's is a door; and
  `description_common.h` / `element_common.h` both define `enum class Module` (one per TU).
  (10) Loader-thread construction of UI forbids static-init / lazy caches in members (B6-5):
  `rtuielem::AtomicExport/AtomicAddr` slots instead.
  **Deliberately NOT converted (reasons in `B36_RULES.md` section 12):** no UI class has a C++
  constructor; dtors that would re-run retail teardown stay doors; the 68/68/68 mint, `_typeInfo`,
  `RT_DELDTOR` rows; the exe-phase subclasses (20 `DialogInstance`, 38 `DialogController`, 4
  `DialogControllerConfig`, 2 `Element`, 2 `ElementDescription`) keep their ctor doors and
  hand-built tables, with QUALIFIED calls inside their slot bodies (step 8); four `RtUI::` PODs
  still embed `RtUI::Element` (`Edit`, `Number`, `Progress`, `Spinner`); the four minted-table
  slot-copy sites; `TextDescription::CreateInstance` stays a door (reached by address from slot 8);
  103 exported UI members declared and defined nowhere (`DialogInstance` 22, `Dialog` 13,
  `UserInterface` 13, `Element` 10 …). **Largest carry-over, found only at the close:** 76 live
  by-address calls to NON-ctor UI members inside `runtime/src/ui` through the file-local
  `RtAddr<Fn>(0x18…)` helper (`SpriteContext::AddSprite` x27, `Dialog::CreateElement` x8,
  `UserInterface::Play` x7) — a binding spelling the inventory never grepped (B36_RULES 14.1);
  plus the pre-existing carry-overs in `NEXT_SESSION.md` section 1.
- **B.3.5 CLOSED (2026-09-19):** `Video` and `Graphics` are real classes under retail's names —
  27 in `Video`, 32 in `Graphics` (`runtime/types/video/`, `runtime/types/graphics/`; list in
  `endgame_b/b35_session/CLOSE_NUMBERS.md` section 7) — with their bodies moved onto members,
  their callers converted and the dead doors deleted. 103 commits (63 non-merge), main `bd483075`
  -> the close; 190 source files, +30.5k / -10.1k lines. Rule sheet `endgame_b/B35_RULES.md`
  (rules B5-1..17, with dated corrections from the blocks), raw retail slot / literal / member
  lists `B35_SLOTS.md`, file ownership `B35_FILEMAP.md`, the session's briefs, Ghidra dump
  scripts and measurements `endgame_b/b35_session/`. Plan for step 6 (UI):
  `endgame_b/NEXT_SESSION.md`, map `endgame_b/UI_INVENTORY.md`.
  **Shape.** One question pass from the retail DLL (hierarchy proved by the `_ParentTypeName`
  literals and shared slot targets; all 39 `sizeof` values from the slot-0 deleting dtors, 21 of
  21 existing header asserts agreed) plus a parallel file map; eleven class blocks on worktrees
  by DISJOINT FILE SET — A the twelve abstract `*Data` bases + `VertexLayout.h` authored, B
  program/shader family (three different parents), C `ImageBuffer` chain (crosses the namespace
  boundary) + `RenderState`, D1 `Context`/`Viewport`/`ConstantBuffer`, D2 `Device` + nested +
  `Configuration` + the buffers, E the Geometry tree (nine classes, one block), F
  `Camera`/`Renderer`/`Scene`, G `Material`/`MaterialInstance`/`GraphicsMesh`/`EdgeMesh`, H1
  animation, H2 particles, J dx11's import table + the seam dispatches; two fragment passes
  (bodies of a converted class that lived in another block's file); caller batches I1-I4 then
  K1-K2 after the fragment passes defined the last members; one closing owner. The coordinator
  gated every merge (19 gates).
  **Numbers.** Caller sites converted: about 190 in round 1 + 115 (K1) in `code/`, 29 + 32 in
  `runtime/src` outside the class blocks, 4 in `standalone/`, dx11's table 85 -> 38 live rows, 30
  seam dispatches -> virtual calls on the abstract `*Data` classes. `engine_funcs`: 76 globals
  bound a Video/Graphics symbol (about 20 spelled `fn_Video_*`/`fn_Graphics_*`, 3 bound only BY
  ADDRESS); 60 deleted with typedef, extern, initialiser and dump line; the 16 left are ctor doors
  kept by design. `ResolveExport` sites naming a Video/Graphics symbol 246 -> 108,
  `RT_EXPORT_ONCE` 147 -> 54 (what remains: ctor/dtor/new/delete names, static storage, vtable
  address reads), door call sites through `engine_funcs` 61 -> 0, hand-inlined slot calls in
  `runtime/src/{video,graphics}` 49 -> 11. Every duplicate view of a Runtime object in dx11 is
  collapsed onto the runtime header (nine views). Unit tests 260 -> 358, 0 failures (slot order
  read back from GCC's tables, the retslot signatures, the qualified-call rule, sizes). Census:
  Graphics 599 total / 92 called / 70 ported, Video 573 / 79 / 54. B.4 cast metric (lines):
  **34,825** (code 23,760 · runtime 7,009 · standalone 2,889 · engine 756 · dx11 168 · tests
  243), UP 769 from 34,056 while adding **911** `// B.3.5 bridge` casts (transitional: each dies
  with its receiver's conversion); `engine` fell 67 with the door globals.
  **Found on the way, all retail-backed.** (1) **GCC's ms_abi passes a hidden return pointer
  BEFORE `this`; MSVC passes `this` first** — a C++ by-value VIRTUAL writes its result through
  `this` when a retail-shaped caller calls it (block E's first unit test segfaulted so). Slots 5
  and 6 of the Geometry family are declared `T* M(T* ret, …)`; non-virtual by-value members sit
  behind explicit `(this, ret)` doors. The rule sheet had it backwards; amended. (2)
  `ExternalPtr<T>` getters ARE hidden-retslot in retail (ten prologues read); an `ExternalPtr<T>`
  ARGUMENT is a bare pointer in a register with no AddRef, so `EdgeMesh::SetMaterials` converts.
  (3) `vertexlayout.cpp`'s `VertexElement` field names were permuted (`format` is +0x18, +0x0c is
  the byte offset; the wire order was accidentally preserved); dx11's view was the right one.
  (4) 23 retail addresses in `ImageBuffer.h` named no `ImageBuffer` export at all;
  `0x180054080` had two bodies and the wrong owner; `0x180006a10`, registered as a Video buffer
  `operator delete`, is a shared free with ~100 callers. (5) `EdgeMesh::Set` has 25 parameters,
  not 22; `{Vertex,Index}Buffer::Set`'s two ints were named backwards in the header; `Adapter`
  +0x2c/+0x30 are the DXGI adapter / output indices; `GraphicsMesh` +0x50 is an
  `Array<DetailLevel>` (0xb0), not of `TriangleList`; three of `EdgeMesh`'s four buffer pairs were
  misnamed; `Geometry` slot 1 is `Update`, not `Draw`; eight `Camera.h` field names. (6) A latent
  ODR defect: two different `RtGraphics::InterpolatedTransform` definitions of the same size,
  harmless only while no TU included both — one definition now. (7) `Volume.h`'s global
  `Rectangle` is hidden by `<wingdi.h>`'s `Rectangle()` FUNCTION once `<windows.h>` is in, and
  the failure prints as a bogus layout assert (`1 == 16`); fixed at the source with elaborated
  specifiers. (8) `ParticleSystem` is the one class with `Construct` at slot 9 (behind the
  inherited pure `CreateSystem`); pinned by a test. (9) Added retail members that had never been
  ported: eleven `Renderer` accessors, five `Camera` members, `Scene::AddDrawItems`,
  `ConstantBuffer::GetNextUploadIndex`, `Viewport` `==`/`!=`, `Graphics::InterpolatedState<T>`
  with `Lerp`. (10) Dead instrument deleted: the `RT_GFX23_CORRUPT` seam (its probe tool does
  not exist) and five replay captures.
  **Deliberately NOT converted (reasons in `B35_RULES.md` section 11 and the headers):** no
  Video/Graphics class has a C++ constructor, and destructors that would re-run retail's
  out-of-line teardown are empty or stay doors (every live object wears the MINTED table; slot 0
  is declared pure because its body is a `deleting_dtor.cpp` row, so nearly every class is
  abstract); none of the 39 minted rows, `RT_DELDTOR` rows or `_typeInfo` rows retired (end of
  B.3); the ten `CUTOVER_REQUIRED` Video/Graphics statics stay `RT_DATA_SLOT` reads
  (`MintStorageCell` redirect owed, section 9; `Particle::_rng` is the double-seed case); the
  concrete dx11 `*Data` classes and the 84-slot seam (RULE B5-8: the seam's completeness check is
  what device-lost recovery stands on); `Device`'s `CRITICAL_SECTION` stays an opaque
  hand-rolled recursive lock (B5-6); `Device::{Adapter,DisplayMode,RefreshRate,GraphicsInterface}`
  and `ShaderProgram::Parameter` take no real special members (their `Array<T>` helpers destroy
  elements through open-coded copies: a double release); `sizeof(Video::Configuration)` cannot
  be measured (no allocation site; the header asserts a range); `Camera`'s copy/assign are
  defaulted for trivial copyability while retail's partial copies stay the doors (three pad bytes
  nothing reads); the exe-phase subclasses in `code/Game` (tornado, weather, status icons,
  geometry_draw, the NoiseUI image buffers) — only their calls INTO base members converted, and
  those are QUALIFIED; `enumerate_video_modes.cpp`'s SEH; at most 425 exported members are
  declared and defined nowhere (unported retail members with no caller: a caller that appears is
  a link error). Small leftovers are listed in `NEXT_SESSION.md` section 1 item 13.
  **Open defect, characterised, NOT fixed — needs a decision:** a device-lost that lands while
  the loader thread builds the world kills the loader thread (`BANISHED_FORCE_DEVICE_LOST=400`:
  pre-step exe 5/11 runs, step-5 exe 1/11; clean at `=2000` and on the plain route on both).
  PRE-EXISTING, and RETAIL HAS THE SAME WINDOW: all seven `Device::Lock` sites are reproduced,
  retail unlocks right after `Present` and runs the whole recovery unlocked, `Create` and the load
  arm of `Serialize` take no lock, `LoaderLoop::OnVideoError` is a bare RET.
  `endgame_b/DEVICE_LOST_LOADER_RACE.md` has the run tables, the lock map and the three possible
  fixes, each a deliberate divergence from retail. One run in this step died with the
  `Factory::IsDerivedFrom` near-null stack at frame 357, before its frame-400 injection: not
  explained; the coordinator's 57 route runs and 75 resaves never showed it.
  **Gate at the close** (coordinator, on a `make clean` rebuild of `8d72408b`, 1,148 compile
  lines per config): debug + perf + unit exit 0; distinct warnings 242 debug / 440 perf against
  the step's 284 / 482 clean-rebuild baseline, 0 new; `dispatch_slot_map --check` 0/109; cutover
  manifest OK (59 rows); mint tables UP TO DATE (152/104/48); new game + tutorial + Load Game
  routes exit 0, `calls=5686` / `5776 openFileFail=15`, `[RtResolve] 0 misses`, no TRAP, 0 GATE
  WARN; pointer-free type table == baseline; resave 5/5 PASS at 21,075,114 bytes; the forced
  device-lost round trip COMPLETE (blocks D2, VC, J; mandatory for anything touching the seam's
  callers). Mods route with both `.pkm` mounted (front-end Mods dialog, ModDetails, Apply): exit
  0, empty crash log, `[RtResolve] 0 misses`, files back in `_mods_off/`. One functional
  `sa-stress` (perf exe, label `b35_close`): exit 0, 6,000 sim frames, fps 139 -> 140, sim
  ms/iter -1.4% against the baseline (inside the 5% plan gate); the tool flagged PostSim ms +39%
  and private MB/min +52% and marked the machine BUSY (`make` ran during it) — recorded, not
  chased. A TRUE release compile (-O3, obj dir emptied first): exit 0, 1,148 files, 7.7 MB exe,
  followed by `make clean` and a debug rebuild (77 MB). Rendering, by command stream not pixels
  (`tools/trace_oracle`, `b35_session/RENDER_CHECK.md`): pre-step exe `bd483075` against the
  step's exe, two RenderDoc captures each (557 / 558 / 509 / 587 draws), selftest 19/19 caught;
  the verdict-bearing screens S3 (floored), S4 and S6 read **0** with the control pair taken from
  EITHER arm; the descriptive screens (S1 49 / 21, S2 3, S5 25 / 7) sit at or below the same-exe
  band measured the same day (47 / 3 / 28).
- **B.3.4 CLOSED (2026-09-19):** `IO` (Stream family, `Serializer`, `Encoding`/`ReaderWriter`,
  `Registry`, `Modification`, `PackageFile`, `FileSystem`, `Persistence`) and the `System` rest
  (`CriticalSection`, `Thread`, `Random`, `Time`, `Color`, `DateTime`, `Timer`, `CRC`, `Error`,
  `WideString`, `Compression`, `Configuration`) are real classes under retail's names, with
  their callers converted and the dead doors deleted. About 135 commits, main `523f8566` -> the close.
  Rule sheets: `endgame_b/B34_RULES.md` (IO, with two coordinator amendments),
  `B34_BRIEFS.md`, `B34B_RULES.md` (System — incl. the verdict table for all thirteen System
  classes and rules B4b-1..5). Plan for step 5: `endgame_b/NEXT_SESSION.md`.
  **Shape.** Phase 1 + blocks 0–3 by one core owner (Encoding 5 slots / ReaderWriter 26 slots /
  `IO::Serializer` 0x10 bytes, ctor consumes the 12-byte stream header via the Encoding factory,
  dtor patches 4 bytes — lifetime position is load-bearing); STREAMS 1–6 and PACKAGE+FS 1–6 in
  parallel on disjoint file sets (two owners each, handed over at commit boundaries); caller
  batches A (+residual), B, C, D, G, H, P; a four-commit closing pass that then SPECCED its own
  guard sweep (`endgame_b/b34_session/GUARD_SWEEP_SPEC.md`; that directory keeps the session's
  briefs, sweep scripts and the coordinator's gate)
  for five directory agents plus a last `engine_funcs`-only pass (2d); System: one owner for the
  rule sheet + nine classes, a second for three + the int64 overload, four caller batches, then
  the same guard-and-cache sweep in small (three directory agents + a last `engine_funcs` pass).
  **Numbers.** Serializer callers: A 852 (+8 residual), B 343, C 252, D 146, closing 17;
  `InternalBasePtr::Serialize` 84 (G); hand-inlined ReaderWriter slot calls / direction reads /
  Harvest door 125 (H); Modification/Persistence/FileSystem 29 (P); System callers 381.
  Deleted: 18 + 20 Serializer-family and 6 System door globals in `engine_funcs.{h,cpp}`, ~370 file-local door caches (typedef +
  static + resolver) and on the order of a thousand readiness-guard lines across `code/Game`, `SerializerLite`,
  `NoteSlotDispatch` and its counter family, `standalone/async_loader.{h,cpp}`.
  `engine_funcs` keeps 20 `fn_*Serialize*` pointers and NONE is an `IO::Serializer` door (10
  ExternalPtr exe doors, 3 exe array helpers, 7 other classes' bodies). System-family globals that
  remain are alive on purpose: the `Time` / `Random` ctor doors, four by-value `String` return
  doors, the allocator doors, the `g_Timer_*` data pointers and the `fn_Random_RandFloatTol` shim
  FUNCTION. Unit tests: 260, 0 failures (the step added ~45, incl. the first executable proof of the
  qualified-call rule and a behavioural CRC-32 check vector). Census: IO 407 total / 210 called
  / 203 ported, 96.7% called-covered; System 262 / 78 / 61 (no registration
  added or retired: `called` and `ported` each fell by 8 because eight ported doors lost their last
  by-name caller to a direct member call — the number measures the registry, not the code). Added retail members that had never been ported:
  `IO::Serializer::GetDirection` 0x180006210, `IO::Stream::GetDirection` 0x180006200,
  `IO::Serializer::Serialize(int64_t&)` 0x180035000, `Object::Internal::Clone` 0x1800bfb10,
  `ExternalFactory::SerializeReferenceTable(IO::Stream&)` (forwards; `external_reftable.cpp`
  untouched). B.4 cast metric: **34,052** (code 23,463 · runtime 6,607 · standalone 2,890 ·
  engine 823 · dx11 149 · tests 120), down 757 from 34,809 while ADDING 2,258
  `// B.3.4 bridge` casts (transitional: each dies with its receiver's conversion); B.3.3's
  1,048 are unchanged (counted on the final tree `1b5eb639`).
  **Deliberately NOT converted (each with its reason in the header or commit):** no stream class
  has a C++ constructor and none of the 19 minted IO rows / 14 deleting-dtor registrations
  retired (B34_RULES §4.3/§4.2 — 13 of 15 IO tables are stamped from outside `runtime/src/io`;
  retirement is the end of B.3); the compressed family's eleven non-virtual members and
  `TextFileStream`'s four non-vtable accessors (on the loader open/read path); `IO::
  CompressedCopyFileStream` (no live construction path anywhere — recorded, declined);
  `??0FileSystem` 0x1800ac7c0 and `??0Persistence` 0x1800b8020 (unported / builds the embedded
  Thread worker), every `PersistenceWorker_*` body incl. the by-address restamp of Thread vtable
  0x180155f20 (`persistence_worker_win.cpp:585-589`); `WriteSettingsBody`; `IO::Registry`'s
  destructor (three hand-inlined teardowns); `IO::TextCodec` — NOT A CLASS (four receiverless
  unexported free functions + `IO::SerializeStringArray`); ctor/dtor doors of `Random`, `Time`,
  `Color`, `DateTime`, `Thread`, `CriticalSection`, `Compression`, `Configuration` (B4b-2: a
  by-value holder seeded through the ctor door would double-seed — about 150 `Time` ctor call sites
  on slot 0x140152b00 stay doors by design); `Configuration::CurrentBuildId` 0x1800c4c60 (writes
  through an un-zeroed hidden return slot — needs its prologue read); eight by-value `String`
  returns into an already-constructed `String`; `System::MemoryAllocator` — **deferred past
  step 4 by decision** (~1,146 sites, assessment in `B34B_RULES.md` §6); `System::Debug` and
  `System::StackCriticalSection` found unported, nothing to convert; the eleven
  `CUTOVER_REQUIRED` System statics stay `RT_DATA_SLOT` reads, `MintStorageCell` redirect owed
  at the class-static conversion (tabled per row, §4.1; Timer's five together); the **125 sites
  through twelve `ExternalPtr` exe-side doors** — exe template instantiations ported as
  game-code bodies with their own guard set, NOT Runtime members (exe-phase, receiver-side;
  `fn_ExternalPtr_Serialize3` is the last reader of exe 0x140007140).
  **Found and fixed on the way, all retail-backed.** (1) **The two-loader defect** — our
  invented `standalone/async_loader` ran a second resource loader beside `Engine::Loader`, and a
  third main-thread loader (`ChunkedSceneLoader`) on the splash; every byte goes through
  `FileSystem::ReadFile`'s packed arm (`SetFilePointer`+`ReadFile` on ONE handle, no lock — and
  retail takes none, because retail has one loader: the exe does not import `Engine::Loader` at
  all). Torn reads gave NULL resources (`[GUL::Help] topics=-1`), bad magic, a
  `CompressedStream::Read` spin, an `ExternalFactory::FindNext` spin, and BOTH resave-gate flake
  signatures. Fix `96c17f47`: single loader, no lock, no guard, no timeout; new-game defects
  4/12 -> 0/24, `[GUL::Load] redundant dispatch` 12/12 -> 0/24, `calls` pinned at 5686.
  `endgame_b/LOADER_READ_SPIN.md` §9 + `LOADER_READ_SPIN_GHIDRA.md` Q5. **The resave gate has
  not DIFFed once since** (about 90 coordinator runs plus the agents' own); a count-field or resident-row DIFF is now a finding, and
  so is a `redundant dispatch` line. (2) `HarvestArea`/`HarvestOrchard`/`PlantArea`/
  `PlantOrchard::Serialize` aliased the four-argument bool-array door onto the three-argument
  `Serialize(int&)` slot — a guaranteed AV that had never executed on the gate save; retail
  calls 0x140151458. (3) `AmbientEmitterDescription` / `ToolbarDescription::Serialize` read
  `ReaderWriter+8` (always 0) as the direction, so `loading` was stuck TRUE; retail reads
  `serializer+0 -> +8`. (4) ~60 null guards of OURS at hand-inlined ReaderWriter calls dropped
  with a per-site argument (retail's `StatGoal::Serialize` 0x1400da8e2 has no test).
  (5) `completeroad.cpp`'s uchar door cache was permanently NULL (misspelled mangled fragment),
  hidden by its guard.
  **The five defects left open at the close — worked 2026-09-19 (merged, main `2788c108`; what is
  still exposed is listed in `endgame_b/NEXT_SESSION.md` §1.6):**
  (a) allocator `LockEnter` on a near-null lock: NOT reproduced, cause inferred. W2′ (unlocked
  free list) is not live — every door into the allocator is locked, as retail's inlined
  Enter/Allocate/Leave. W2 fits the stack; retail's only loader join ahead of `~Engine` is
  `LoaderLoop::Shutdown` (0x1800dc280, `WaitForSingleObject(hFinished, INFINITE)`), which our two
  LoaderLoop Shutdown stubs skipped — they now join (`a9873f47`). The recorded window was most
  likely already closed by `96c17f47`. Same exposure as retail remains for a quit during a splash
  loop; our `WinMain` tail after `~Engine` is longer than retail's. (b) `SaveLoadDialog_
  PopulateFileList` null = `tableSaveList` (+0x108) missing because one `.crs` loaded null under
  the two-loader race `96c17f47` removed; port matches retail, no change, 10 menu-Load runs (6
  under load) clean. Inferred, not reproduced on a pre-fix build. (c) FIXED by ordering, as
  retail: `MenuDLT::Shutdown` calls `Audio::Scene::StopAll(self+0xb30)` (exe 0x14009b41a) before
  the next loop remounts (`c4603875`); retail takes no lock on this path and keeps the same
  one-read residual window. Found with it: non-menu loops' `AllowsModChanges` returned true where
  retail's base (0x180031790) returns FALSE (`e353fb15`) — `_savingEnabled` now flips, `_savedMods`
  is snapshotted, game->menu re-runs `RefreshList`. (d) FIXED (`94258ffc`; retail `LEA RDX,
  [RAX+RCX*8]` at 0x14006ddee / 0x14006dfdb). (e) note corrected (`59ca40dd`); disposition
  unchanged (`?Get@Configuration@System@@` still unported), manifest check 59 rows identical.
  Merged-tree gate: new game reaches the world (`calls=5686` seen on the (c) branch, not re-read
  on the merge), resave PASS == golden (`calls=5691` is the load route's number), pause-menu
  Quit-to-Main-Menu returns to a live menu, crash log 0 bytes, unit 260/0, dispatch 0 mismatches.
  Tutorial route + pause-menu Quit: exit 0, `calls=5776 openFileFail=15` = the archived tutorial
  number. Mods (both `.pkm` moved in, back in `_mods_off/` after): front-end Mods route to Quit
  exit 0; new game -> Quit-to-Main-Menu live; in-game Mods toggle + Ok -> save-state world reload
  completes. (d) driven with a fabricated conflict (a duplicated `.pkm`) and a temporary trace:
  `conflictCount=1`, `Find` -> `cIdx=0`, valid `Mod*`. `sa-stress` (perf build): exit 0, clean
  quit, 140 fps, sim ms/iter +7.1% vs baseline under a BUSY machine — recorded, not chased.
  **Gate at the close** (coordinator; the final tree `1b5eb639` gated 0 problems with resave 5/5, and
  the numbers below are from the `make clean` run on `282cd55b` — 1,993 debug / 2,433 perf compile
  lines — before the System closing sweep, which was then gated per merge): debug + perf + unit exit 0; distinct warnings 284 debug / 482 perf
  against a 294 / 494 clean-rebuild baseline, 0 new; `dispatch_slot_map --check` 0/109; cutover
  manifest OK (59 rows); mint tables UP TO DATE (152/104/48); new game + tutorial + Load Game
  routes exit 0, `calls=5686`, `openCompFail=0`, `[RtResolve] 0 misses`, no TRAP, 0 GATE WARN;
  pointer-free type table == baseline (312 registered, 417/417); resave 5/5 PASS at 21,075,114
  bytes; a TRUE release compile (-O3, obj dir emptied first — `standalone` shares its obj dir
  with `standalone-debug`, so the usual release build compiles nothing) exit 0; Mods route with
  both `.pkm` mounted clean (run at the IO close); one functional `sa-stress`
  (perf exe, label `b34_close`): clean self-quit, no crash or hang log, fps 139 -> 140, sim ms/iter
  +3.6% against the baseline (inside the 5% plan gate; taken under load) — recorded, not chased.
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
  **Next: B.3 step 4 (IO). The plan for the next session is `endgame_b/NEXT_SESSION.md`;**
  rules so far: `endgame_b/B33_RULES.md`, `B32_RULES.md`, `B31_RULES.md`.
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
  **Coordinator gate on the merged main (2026-09-18):** every B.3.3 merge (steps 3, 4, 5,
  6A, 6B/C, 6D, closing) was gated by the coordinator: debug + perf build (release too from
  6B on), `make unit` (206/0 at the close), autostart + pause-menu quit, the tutorial route,
  the Load Game dialog route — all exit 0, empty crash log, no hang log — a live-boot type
  table compared against the pre-step-3 boot (312 registered, the same ten id/name pairs;
  the chain reads 417 since step 5's `DEFINE_TYPE(Internal)`, was 416), HelpTopics 66 /
  DiseaseList 12 identical, `[RtResolve] 0 misses`, no `[RtMint] TRAP`,
  `dispatch_slot_map.py --check` 0 (109 rows since 6D), 0 GATE WARN. At the close also the
  Mods route with both `.pkm` mounted (6 clicks, exit 0; mods back in `_mods_off/`) and the
  resave gate PASS 3/3. Resave across the step: PASS except the documented loader-race
  flake (count 5689 + ONE extra row — `CropField.rsc:resource` twice after 6A,
  `ToolbarRemoval.rsc:resource` once after 6B/C; never after 6D or the close) — see
  `tools/resave_gate/README.md`. One coordinator fix: a 6B include inside
  `#if defined(_DEBUG)` that broke only the perf build.
  `make sa-stress` is a FUNCTIONAL check while B is open (B33_RULES §10): 6000 frames, clean
  quit at every step close. Printed sim ms/iteration, recorded and NOT chased: step 3
  0.19 (-0.2%), step 4 0.19 (+0.4/+0.8%), step 5 0.21 (+9–10%), 6A 0.21, 6B/C 0.21,
  6D 0.20 (+4.6%), close 0.21 (+10.7%, `perf/results/20260918_175719_b33close`). Revisit
  after B closes; the per-step numbers are the trail to bisect from.

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
