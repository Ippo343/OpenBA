# Next session plan (written 2026-09-26, at the B.3.8 close)

State and counts live in `../STATUS.md` (the B.3.8 block) and `b38_session/CLOSE_NUMBERS.md`; this file is
only the plan. Delete it (or replace it) when the session it describes has started.

**What step 8 left behind, in one line:** every class the game instantiates now has a compiler-generated
vtable (`b38_session/CLOSE_NUMBERS.md` §3: 306 hand-built tables → 3, none serving an exe class; 310 of 310
reflected exe classes agree with retail). What is left of B.3 is the SCAFFOLDING the migration needed
while hand-built and compiler tables coexisted: the mint, the `_typeInfo` / `RT_DELDTOR` rows, the ctor /
dtor doors, the exe-address registrations, the type-id cells and the singleton storage. B.3's last step
retires them. After it, the normal-virtual cutover and the registry teardown are mechanical.

## 0. FIRST: finish the B.3.8 step close (owed)

The step-close run on `e24d1e7c` was stopped by the harness for low system memory (STATUS B.3.8 block,
`b38_session/GATE_TABLE.md` tail). Done: Mods, Quit, Options, Achievements, GoalDialog, the three tool
routes, the in-game save, `sa-stress`, the clean rebuild (+ its warning diff, unit, dispatch, manifest).
Owed: the true release compile from a clean obj dir (then `make clean`), the clean-built exe's gate routes +
resave, `rt_mint_gen --check`, the `NOT IMPLEMENTED` stub-set diff and the tutorial `[TownCensus]` line.
Close other memory-heavy programs, then run `stepclose.sh` from a scratchpad copy of `b38_session/`
(it needs `gate.sh`'s `runs/baseline` from the clean-rebuild baseline — cut it first, §1 below) and record
the result in STATUS and GATE_TABLE. A failure there is a step-8 finding, fixed before the next step.

## 1. Sync (5 minutes)

- `git log --oneline -8`; `git worktree list` (step 8's 24 agent worktrees were auto-created by the
  Agent tool under `.claude/worktrees/`: remove them with `git worktree remove` and prune the
  `worktree-agent-*` and `b38-*` branches with `git branch -d` — every one is merged into main;
  `git branch --no-merged main` must be empty).
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`; never `find /`.
- Ghidra: check the bridge FIRST (`mcp__ghidra-mcp__check_connection`; `tasklist` shows `javaw.exe`, not
  `java.exe`, while it is up). Pass `program="Application-x64.exe"` / `"Runtime-x64.dll"` on every call that
  takes it instead of `switch_program` (GLOBAL); step 8's agents did, and nobody collided.
- The gate is `b38_session/gate.sh` (it has the step-8 typetable rule: a diff made only of removed
  `ResolveTypeIdByNameLazy` lines is equal). Copy it, `reftable_diff.py`, `mods_route.sh`, `stepclose.sh`
  and `toolroutes.sh` into the session scratchpad, cut the warning baseline from a `make clean` rebuild
  FIRST (`gate.sh baseline 3 warnbaseline`, then `gate.sh baseline 3 skipbuild` for the typetable and
  route baselines). **Invariants changed in step 8** (LOOP, `758597e2`): new-game `calls=5683
  openFileFail=0`, tutorial `calls=5773 openFileFail=12` (Load Game floats 4758-4789); the resave golden
  md5 is `dd3893af7772e3f58a3bb51dd5c29a9f` (the `.bin` is git-ignored: if the golden is missing in a
  fresh checkout, run the resave gate twice — the first run cuts it, its md5 must equal the committed one).
- Agents: the `isolation: "worktree"` Agent option worked well in step 8 (24 worktrees, no collisions) —
  but a worktree is cut from HEAD, so COMMIT any doc an agent must read before launching it. Class blocks
  ran static-gate only; the coordinator alone takes the game-dir lock (the one exception: a block whose
  merge broke every route got the lock to bisect its own fix). Blocks that write helper scripts must use
  `C:/Users/USPC/AppData/Local/Temp/claude/<branch>/`, not the coordinator's scratchpad.

## 2. Carry-overs (each its own gated change; none blocks the next step unless marked)

From before step 8, still open (details in `B37_RULES.md` §12 and the old carry-over list in git history of
this file, `git show 1d7866d4:docs/runtime_port/endgame_b/NEXT_SESSION.md`):
1. `System::MemoryAllocator` (~1,146 sites) — deferred on purpose.
2. `System::Configuration::CurrentBuildId` stays a free door.
3. The 125 `ExternalPtr` exe-side door call sites; `Geometry.h`'s `Graphics::ExternalPtrHandle` alias.
4. The small list (receiver-unproved direction reads, `res_load_lock`, `Time` by name, `Camera.h` names,
   `scene_watch` raw reads, `boot_window.cpp`'s `Configuration+0x42`, `InterpolatedStateT`, the five int64
   `ReaderWriter` slot calls, `common.h` vs `ImageBuffer.h`).
5. **THE DEVICE-LOST / LOADER RACE NEEDS THE USER'S DECISION, NOT A PATCH**
   (`DEVICE_LOST_LOADER_RACE.md`).
6. dx11 `*Data` classes and the 84-slot seam (RULE B5-8); dx11's `appSingleton` data row.
7. Quit-during-splash exposure; `e353fb15`'s Mods-route timing.
8. UI (step 6): 76 by-address non-ctor UI calls inside `runtime/src/ui`, 103 exported UI members declared
   and not ported, four `RtUI::` PODs embedding `RtUI::Element base;`; the Options-change route (the menu
   driver still has no `OptionsDialog` token). **The `GoalDialog` route is CLOSED** (step 8: pause-menu
   id 10 in a tutorial scenario game, in `b38_session/stepclose.sh`).
9. Step 7: 105 exported members declared and not ported; `KeyMap`'s split fields; the 19 `rtpickgrid::`
   free functions; `Math::Vector3::operator==` (0x140152a70) unregistered (keeps `MoveEmitter` dead).

From step 8 (`B38_RULES.md` §10, sizes there):
10. **142 PORT GAP slots** (Component 104, Dialog 30 …), 38 of them leaking dtors; **49 STANDALONE
    DEVIATION** overrides (PNL). Each transcription is its own titled behaviour commit, gated with resave
    x5. This is B.4-adjacent quality work: it can run as parallel blocks by directory at any time.
11. **36 lazy `X_ResolveFuncs` resolvers (137 calls)** — B6-5 surface on the loader thread.
12. **238 raw dispatch sites** left unproved or excluded (75 candidates needing proof); **12 delegating
    members** (`CharacterComponent`'s 256-line body the largest).
13. The 3 remaining by-name concrete lookups (`Disease`, `Toolbar` ×2); 10 zero-reference `engine_funcs`
    globals not owned by a converted class + 8 type-id / ctor slot globals; 237 reader-less registry rows
    whose symbol names no converted class; the dead `ControllerLive::Dispatcher<Tag>` template.
14. **~270 `g_pGameWorldSingleton + off` raw reads outside `code/Game/Game`** (Component 96, Player 61,
    Dialog 45, Action 23 …; CB6's count) — `GameUpdateLoop` now declares those fields.
15. B38_RULES §6 still NOT VERIFIED: 2 (type-table order sensitivity — MEASURE IT FIRST in the next step,
    decision 4's `DEFINE_TYPE` fold depends on it), 4, 5, 7, 9.

## 3. B.3 step 9 — END of B.3: retire the migration scaffolding

`ENDGAME.md` B.3 is complete as a list of hierarchies; `B38_RULES.md` D10 wrote the retirement order. This
step is mostly DELETION, like phase A, but every deletion is gated because the scaffolding is live.
Acceptance: `rt_data_mint_tables.inc` has no `??_7` vtable row and no `?_typeInfo@` row;
`deleting_dtor.cpp`'s `RT_DELDTOR` table is empty; no `RegisterExeAddr` of a vtable address; no
`ResolveAddr(Module::Exe, 0x1401c…)` type-id cell read; the singleton storage lives in class statics; the
gate (routes + resave x5) passes. Then B.3 is CLOSED and the normal-virtual cutover (`DeletingDtor` →
`virtual ~T()`, `Object::Destroy` → `delete`) becomes legal — it is its own mechanical step, right after.

### 3a. What is already known (measured 2026-09-26 at the close)
- `rt_data_mint_tables.inc`: 152 minted vtables, 104 `BaseTypeInfo` records, 48 storage cells;
  `deleting_dtor.cpp` 137 `RT_DELDTOR` rows (+1 in `deleting_dtor_leaves.cpp`, which still has five
  separate dtor doors — RB's note); `rt_export_addrs.inc` 4,932 generated rows; `standalone/address_registry.cpp`
  1,617 lines; `engine_funcs.{h,cpp}` 4,292 + 2,576 lines; 145 `standalone/reg_*.cpp`; 58
  `CUTOVER_REQUIRED` manifest rows.
- Why the mint is still live: every Runtime class's ctor DOOR stamps its minted table (RB kept the doors
  for "unconverted subclasses"; after step 8 there are none in the exe), and the Runtime's own
  `Object::Factory` create/create-in-place functions build Runtime classes through those doors. A minted
  row retires when (a) no door stamps it — the door placement-news the real class instead — and (b) no
  `ResolveExport("??_7…")` / `RT_VTABLE_ONCE` / slot-copy site reads it. The six slot-copy sites of step 8
  are gone except the `Video::ImageBuffer` copy (0x14015c8c0) and `thumb_vtables.cpp`'s SpriteSheet /
  MaterialInstance copies (DLG left both: protected ctors) — those three go first.
- The exe vtable rows left (6): the four loop tables (read by 18 door-then-stamp loop builders in
  `mainmenudialog.cpp`, `newgamedialog.cpp`, `saveloaddialog.cpp`, `tutorialui.cpp`,
  `tutorial_next_scenario.cpp` and `engine_funcs.h`'s inline `MenuDialogLoopT_Ctor` — make them real loop
  ctors), `StatGoal` (read by `RunFactorySelfTest`), the ImageBuffer copy.
- Type-id cells: 100 `g_tid_*` + 111 `g_TypeId_*` names, 423 `ResolveAddr(Module::Exe, 0x1401c…/0x1401d…)`
  sites. Each becomes `X::TypeId()` (exe class) or the Runtime class's static; the `.data` rows retire.
- Reflection: exe classes are on `DEFINE_EXE_TYPE` (296) over the FactoryRegistration list, which feeds the
  DLL-shaped `BaseTypeInfo` chain; Runtime classes are on the 104 minted `_typeInfo` records. ENDGAME B.1
  decision 4 folds both into one `DEFINE_TYPE` — ONLY after the type-table order question (§6 item 2) is
  answered by a measurement (reorder registrations in a throwaway build, compare the pointer-free type
  table AND a resave).
- Singleton storage: 9 `CUTOVER_REQUIRED` System statics + 10 Video/Graphics + 9 step-7 rows +
  `?_singleton@SaveLoad@Save@@` (no manifest row) → class statics via the `MintStorageCell` redirect.

### 3b. Block 0 — the inventory and the question pass (read-only, then ONE opus owner into `B39_RULES.md`)
1. Per minted vtable row (152): the class, every stamper (its ctor door + any other), every reader
   (`ResolveExport("??_7…")`, `RT_VTABLE_ONCE`, copy sites, `Vtable()`-style comparisons), who creates the
   class (factory create / create-in-place / exe code / Runtime code), and whether it has a real C++ ctor
   today. Output: the retirement order per row (D10: copy sites → subclasses → exe registrations →
   `g_tid_*` → mint vt rows → `RT_DELDTOR` → `_typeInfo` → `Object::Base` last → singleton storage).
2. Per ctor/dtor door still registered (the "doors kept by design" lists of steps 6-8): callers by
   binding; which become `::new (p) X(…)` / `Object::Destroy` at the call site; which are reached only
   through the mint and die with it.
3. The `RT_DELDTOR` table: which rows a compiler `DeletingDtor` already supersedes (every class with a real
   slot 0) and which dispatches still reach a row.
4. Type-id cells and the reflection fold: the order measurement (§6 item 2); the `DEFINE_TYPE` shape for
   Runtime + exe classes together; `Factory::Startup`'s chain walk and the AVL tree.
5. Singleton storage: each cell's readers by binding; the class-static spelling; the manifest rows that go.
6. What stays in `rt_export_addrs.inc` / `address_registry.cpp` / `engine_funcs` after the step (the
   by-address doors the exe-phase code still calls, e.g. `ExternalPtr` doors, `MemoryAllocator`) — so the
   registry teardown that follows B.3 knows its size.
7. File ownership: the retirement is by BASE (per Runtime namespace), so blocks are namespace-disjoint
   (Object, System, IO, Collection, Video/Graphics, UI, Engine/Save/Achievement/Audio/Input/Pathfinding/
   Picking) plus one exe-side block (loop builders, type-id cells, the last copy sites) — `B39_FILEMAP.md`.

### 3c. Standing rules every prompt carries (B34B, B35 §11, B36 §8/10/14, B37 §8/10/14, B38 §4/§8/§11 hold)
- Step 8's additions: exe classes have REAL ctors; slot 0 is `DeletingDtor(unsigned)` with a protected
  non-virtual dtor (no virtual dtor until the cutover); a Runtime base's door and its real ctor share ONE
  body (`X::DoorBody`); `UI::DialogInstance::operator new(size_t, void*)` is declared but undefined — write
  `::new (p) X(…)`; a class's real table ENDS at its retail slot count — before deleting a padded table,
  grep the whole tree for dispatches past it (the CIT lesson); a teardown that now RUNS (a base dtor that a
  hand-built slot 0 used to skip) can free something another owner still uses — the LOOP lesson (the
  menu scene evicted by the splash's `ExternalFactory::Clean`): find the owner, never add a guard.
- Retail wins; every behaviour change is its own titled commit, gated with resave x5; a resave DIFF is
  localised (`reftable_diff.py`, then the object census) before any golden re-cut, and a re-cut is its own
  commit with the evidence in the message.

### 3d. The gate for every merge (coordinator only)
Unchanged from step 8 (`b38_session/gate.sh`): debug + release + perf build exit 0 · new-warning set diff
vs a clean-rebuild baseline · `make unit` · new game + pause-menu quit · tutorial · Load Game · resave x5 ·
pointer-free type table vs baseline (lazy-lookup-only diffs allowed) · `[RtResolve] 0 misses`, no
`[RtMint] TRAP`, 0 GATE WARN, new-game `calls=5683`, tutorial `calls=5773 openFileFail=12` ·
`dispatch_slot_map.py --check` 0 · cutover manifest gate · `rt_mint_gen --check` (this step CHANGES the
mint: regenerate with `rt_mint_gen.py`, never hand-edit) · `git diff --stat` against the agent's file set.
Retiring the mint also changes `[RtMint]` / `[RtResolve]` boot lines: re-cut the route expectations from
the first block's gate, in writing. At the step close: `b38_session/stepclose.sh` (Mods with `.pkm`,
Quit / Options / Achievements, GoalDialog from a tutorial game's pause menu, the three tool routes, an
in-game save, `sa-stress` recorded never chased, true release compile then `make clean`, clean rebuild +
full gate, the `NOT IMPLEMENTED` stub-set diff and the tutorial `[TownCensus]` line).
