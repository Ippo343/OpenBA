# Next session plan (written 2026-09-26, at the B.3.7 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list` (only the main tree: step 7's fourteen agent
  worktrees were removed as each merged; every `b37-*` branch is merged into main — prune them
  with `git branch -d`), `git branch --no-merged main` (expect none). Old branches that are NOT
  this plan's: `endgame-a3`, `wave29/base`, `defects-open-5` (merged; can go, local and `origin`).
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`;
  never `find /`.
- Ghidra: check the bridge FIRST (`mcp__ghidra-mcp__check_connection`; `tasklist` shows NO
  `java.exe` while Ghidra is up — memory `reference_ghidra_running_check`); only if it is down,
  `nohup "C:/tools/ghidra_12.0.3_PUBLIC/ghidraRun.bat" Banished_RE.gpr &` from the repo root
  (~60 s). `switch_program` is GLOBAL: every agent asserts `get_current_program_info` before each
  read batch (retail `Runtime-x64.dll` 15290 functions; profile 16002 is names only;
  `Application-x64.exe` 11473) and switches BACK. Scripts are JAVA; the dual-mode
  `b37_session/ghidra/Step7Appendix.java` (DLL mode: vtables terminated on the next `??_7`/`??_R`
  symbol, sizeof from slot 0, parents from the literals, retslot exports, by-value-argument
  prologues, ctor stamp chains; EXE mode: exe-side tables and import stubs) is the model — step 8 is
  almost entirely EXE mode.
- The gate is `b34_session/gate.sh` (copy it, `reftable_diff.py`, `b37_session/{mergegate.sh
  shape, stepclose.sh, mods_route.sh}` into the session scratchpad; `<label> <n resaves>
  [skipbuild|resaveonly|warnbaseline]` — the ARGUMENT ORDER matters: a mode in the count slot
  runs a full gate); cut the warning baseline from a `make clean` rebuild FIRST
  (`gate.sh baseline 3 warnbaseline` after building with logs into `runs/baseline/`); the compiler
  is behind `ccache`, so a clean rebuild takes ~25 s and replays cached warnings honestly;
  `rt_mint_gen --check` rewrites `standalone/rt_export_addrs.inc` line endings — `git checkout --`
  it after every gate.
- Agents: EVERY prompt says the lock acquire, the run and the `boot_state.sh --wait` poll happen in
  the FOREGROUND. Three of step 7's four wave-1 blocks went idle waiting on a background watcher
  that never fired and held the game-dir lock idle for 10+ minutes; the coordinator checks
  `tools/game_dir_lock.sh status` (empty `exe_pids` + old `last_write` = an idle holder) and a
  message resumes the idle agent. Caller batches are static-gate only. Two batches editing the same
  file's include block conflict trivially (keep both sides).

## 1. Carry-overs (each its own gated change; none blocks step 8)

Unchanged from before (details in the B.3.4 / B.3.5 / B.3.6 blocks of `STATUS.md`,
`B34B_RULES.md`, `B35_RULES.md` section 11, `B36_RULES.md` section 12):
1. The IAT-cached `fn_InternalBasePtr_Ctor` / `_DeletePtr` callers in ten files.
2. One `?_typeInfo@` record per name (`Base`, `External`, `ExternalList`, `CompiledExternal`,
   `Internal`): owner task, `rt_data_mint.cpp` + `rt_mint_gen.py` + a live type-table comparison.
3. `ExternalList::Find`'s second parameter is `Object::Type` by value (`runtime/types/object/Type.h`
   now exists — B3 added it).
4. `System::MemoryAllocator` — deferred on purpose (~1,146 sites); the `iatMemAlloc` fallback in
   `gatherrawmaterials.cpp` / `foresterwork.cpp`; `singleton_ctors.cpp` stamps its ctor too.
5. The singleton STORAGE: eleven `CUTOVER_REQUIRED` System statics + ten Video/Graphics + the nine
   step-7 rows (`AchievementManager`, `Application`, `Audio::Configuration`/`Device`,
   `LoaderLoop::_loader`, `Engine`, `MainLoop`, `Input::Configuration`/`Device`) plus
   `?_singleton@SaveLoad@Save@@` (0x1801fc6f0, read through `RT_DATA_SLOT`, NO manifest row):
   `MintStorageCell` redirect at the class-static conversion, the END of B.3 (after step 8).
6. `System::Configuration::CurrentBuildId` stays a free door.
7. The 125 call sites through the twelve `ExternalPtr` exe-side doors; `Geometry.h`'s
   `Graphics::ExternalPtrHandle` alias.
8. Small: eight direction reads with an unproved receiver (`d6ee7bfc`); `standalone/res_load_lock.{h,cpp}`
   live-or-dead; `Time::AsSeconds` / the `Time` ctor reach `System::Timer` by name; `Camera.h`'s
   eight wrong POD field names; `scene_watch.{h,cpp}`'s ~60 raw-offset reads; `boot_window.cpp`'s
   `Configuration+0x42` write; `InterpolatedStateT` vs `InterpolatedState<T>`; the nine int64
   `ReaderWriter` slot calls (five left); `runtime/types/common.h`'s global forward declarations
   clash with `video/ImageBuffer.h`'s global `using` (B1 dropped the include from `MainLoop.h`).
9. **THE DEVICE-LOST / LOADER RACE NEEDS THE USER'S DECISION, NOT A PATCH**
   (`endgame_b/DEVICE_LOST_LOADER_RACE.md`; retail has the same window; step 7 moved no call in
   `Loader::Run`'s bracket).
10. The concrete dx11 `*Data` classes and the 84-slot seam stay (RULE B5-8). dx11's import table
    is down to the `appSingleton` data row for `Application` (K5 converted the three function rows).
11. Quit-during-splash exposure; `e353fb15`'s Mods-route timing. (MenuDLT `Stub_Shutdown`'s leak,
    the old carry-over 11, is CLOSED: B1 transcribed retail's 12-step teardown.)
12. UI (step 6): 76 live by-address calls to NON-ctor UI members inside `runtime/src/ui`
    (`SpriteContext::AddSprite` x27, `Dialog::CreateElement` x8, `UserInterface::Play` x7 …) plus
    69 ctor/dtor ones; 103 exported UI members declared and not ported; four `RtUI::` PODs still
    embed `RtUI::Element base;`; `g_TypeId_Element`'s lazy fallback; `TextDescription::CreateInstance`
    stays a door; the owed routes (a `GoalDialog` route from a scenario game's PAUSE menu; an Options
    route that changes a setting and saves — the menu driver has no token for `OptionsDialog`
    elements, so this needs a driver token first, a small deliberate instrument exception or a
    manual run).
13. Step 7 (`B37_RULES.md` section 12, `b37_session/CLOSE_NUMBERS.md`): 105 exported members
    declared and not ported (`Pathfinding::Scene` 15 dead in retail, `MainLoop` 12, `Audio::Device`
    11, `Input::Device` 10 …) and 8 undefined dtors that stay doors (`~Engine`, `~Application`,
    `Input::~Configuration`, `~KeyMap`, `Audio::~Device`); `KeyMap`'s two split fields vs a
    `Collection::Set<Binding>` (needs a byte-key compare policy); the 19 `rtpickgrid::` free
    functions; the 38 address-documentation constants in `runtime/types/engine/`;
    `Math::Vector3::operator==` (exe slot 0x140152a70) still unregistered, which keeps the
    `MoveEmitter` loop in `citizen_assignworkplace.cpp` dead; `gameupdateloop_load.cpp`'s
    `BuildSkyMesh_Step28` two loader-thread lazy resolves (Graphics/System symbols); the four
    defined non-polymorphic dtor doors still called from `code/Game` (`Audio::Scene`, `Picking::Scene`,
    `SaveLoad`, `SavedState` — the calls replicate exe import calls on exe-held objects: decide with
    the holders in step 8).
14. All mint / `_typeInfo` / `RT_DELDTOR` rows (300 / 104 / 138 tree-wide today) retire at the END
    of B.3 with the rest; the four minted-table slot-copy sites of step 6 must convert first.

## 2. B.3 step 8 — `code/Game`: the exe-phase classes become real C++ subclasses

`ENDGAME.md` B.3 line 8: "`code/Game`: components and dialogs, the 56 multiple-inheritance types
last." This is the last conversion step of B.3; after it the END-of-B.3 retirement (mint rows,
`_typeInfo`, `RT_DELDTOR`, singleton storage → class statics, the normal-virtual cutover) is its
own closing step. Same shape that has now worked nine times: read-only inventory -> committed rule
sheet `B38_RULES.md` (+ `B38_SLOTS.md` from the EXE, `B38_FILEMAP.md`) -> ONE owner for the
question pass -> class blocks by DISJOINT FILE SET on worktrees -> coordinator gates each merge ->
fragment pass(es) -> caller batches -> one closing owner -> step close. Session kit: `b37_session/`
(`BLOCK_BRIEF.md`, `CALLER_BATCH_BRIEF.md`, `CLOSING_BRIEF.md`, the dual-mode Ghidra script,
`CLOSE_NUMBERS.md` as the measurement template, `stepclose.sh`), `b36_session/EXE_SUBCLASSES.md`
and `b37_session/EXE_SUBCLASSES.md` (the two enumerations of what step 8 converts).

### 2a. What is already known (both `EXE_SUBCLASSES.md` files; sizes measured 2026-09-26)
- **The Runtime bases are all real now**: `Object::{Base,External,CompiledExternal,Internal}`,
  the 68 `UI::*` classes, `Engine::{UpdateLoop,LoaderLoop,DialogLoopT,DialogLoaderLoopT}`,
  `Achievement::{Achievement,AchievementInstance,Goal,GoalInstance}` — each abstract-by-policy
  with slot 0 `= 0`, a ctor door, and QUALIFIED calls already in place inside the exe slot
  bodies (30 UI sites, 25 step-7 sites). Step 8 replaces the hand-built tables with
  compiler-generated vtables by deriving real C++ classes from those bases.
- **The exe population**: `code/Game` 543 `.cpp`; **188 hand-built vtable arrays** in `code/Game`
  (+ the `standalone/*_stubs.cpp` loop tables); 1,401 `RegisterExeAddr` calls; `engine/types` 143
  headers / 248 structs (the exe-phase PODs — `Dialogs.h`, `Panels.h`, `Elements.h`,
  `Descriptions.h`, `AchievementGoals.h`, `TutorialGoals.h`, `GameUpdateLoop.h`, the component
  headers); 164 `standalone/reg_*.cpp`. Known subclass sets: 20 `UI::DialogInstance`, 38-39
  `UI::DialogController`, 4 `UI::DialogControllerConfig`, 2 `UI::Element`, 2 `UI::ElementDescription`;
  `GameUpdateLoop`, `MenuDialogLoopT`, the splash loop, two `LoaderLoop` tables; 5 `Goal` + 2
  `GoalInstance` subclasses, `Scenario` (`Achievement::Achievement`), the never-registered exe
  `AchievementInstance` table 0x14015e350; and the big one, the **components**: `Component`
  subclasses with the 56 secondary-vptr (`ControllerComponent`) types — no inventory exists yet.
- **Retail facts already read** that constrain the shape: the retail Scenario table has 9 slots (the
  tree's has 13), the retail Goal tables 11 (the tree's arrays run into the next table);
  `GameUpdateLoop` has 12 slots; slot 1 (`TypeName`) is null in every hand-built Goal table and
  `FinishTutorialGoal` leaves 4 and 7 null; five slot-0 bodies skip their base dtor (MenuDLT, splash,
  two LoaderLoops, `TutorialHighlightDialog`); `Dialogs.h:23` and `Panels.h:22` both claim
  0x14015b070; `::TutorialSetupDialog` is defined twice (`engine/types/ui/Dialogs.h:468` vs
  `engine/types/TutorialSetups.h:90` — a live ODR violation); the `AchievementDialog` / `ModDialog`
  multiple-inheritance hint; the four minted-table slot-copy sites (`tool_base_toolui.cpp`,
  `frustumoutlineelement.cpp`, `thumb_vtables.cpp`, `dialog_controller_vtable.cpp`); no
  `DialogController` subclass runs `~DialogController` today.
- **The RtEngine loop PODs** stay until the loop subclasses convert: 95 `m_` field uses in 7 files
  (`gameupdateloop.cpp` 44, `splash_loop_stubs.cpp` 31, `loader_loop_stubs.cpp` 12, …).

### 2b. Block 0 — the inventory and the question pass (read-only, then ONE opus owner into `B38_RULES.md`)
1. **The exe class inventory** (no map exists): every hand-built vtable array in `code/Game` and
   `standalone/`, its retail exe vtable address (`RegisterExeAddr`), slot count, the Runtime base it
   derives from (or the exe base — `Component`, `ControllerComponent`, `Tool`, `Dialog*` intermediates),
   its `engine/types` POD, alloc size, ctor door, how many slots are own bodies vs base vs nop; the
   exe's own hierarchy read from `Application-x64.exe` (the `_ParentTypeName` literals the exe
   emits, the `??_7` tables with their slot lists terminated on the next symbol — EXE mode of
   `Step7Appendix.java`), including which tables carry a SECONDARY vptr (the 56
   `ControllerComponent` types: `class X : public Component, public ControllerComponent` — GCC
   lays non-virtual bases out in declaration order and emits the this-adjusting thunks retail
   hand-wrote; ENDGAME B.1 decision 2) — read every exe ctor's stamp chain for a store at a non-zero
   `this` offset.
2. **Which exe classes are reflected** (`DECLARE_TYPE`-shaped: a `_typeInfo` record, `TypeName`/
   `GetTypeId`, a factory registration keyed by the CRC of the class name — saves depend on the
   NAME, so the literal is copied from the exe) vs plain subclasses.
3. **Order of conversion**: bases first (`Component` and the exe intermediates), then leaves; the
   dialogs/controllers cluster (UI bases), the Goal/Scenario cluster (Achievement bases), the loop
   cluster (Engine bases — this retires the `RtEngine::` PODs and the five stub tables), the
   components (largest; the 56 multiple-inheritance types LAST, each pinned by a unit test that
   reads the secondary vptr offset and the thunk adjust).
4. **What the hand-built tables do that a compiler vtable will not**: null slots (Goal slot 1),
   print-once nop stubs, tables sized past retail's (Scenario 13 vs 9), slot-0 bodies that skip
   the base dtor, tables filled at init from a Runtime object (`FillBaseSlots`, `Apply()`, the four
   slot-copy sites) — each is a behaviour decision to write down before a block starts (retail's
   behaviour wins unless the standalone genuinely differs, per CLAUDE.md).
5. **The `RegisterExeAddr` seam**: an exe function the exe reaches by address today keeps its
   registration as a door delegating to the member (the step-6/7 shape); which registrations are
   reached ONLY through the hand-built table (and so die with it) vs by address from elsewhere.
6. **Threading**: the loader thread constructs `GameUpdateLoop`'s world (components, dialogs) while
   the menu draws; B6-5 applies to every exe member.
7. **File ownership** (`B38_FILEMAP.md`): `code/Game/<Dir>/` is mostly one class per file, but
   `engine/types/*.h` PODs are shared headers (`Dialogs.h` 20 classes, `Panels.h` 38) — the B6-4.4
   rule (leaf blocks author per-class headers; a shared POD header becomes aliases in the closing
   pass) decides.
8. **The END-of-B.3 retirement plan**, written now so step 8's blocks do not fight it: when a class
   has a compiler-generated vtable, its mint row, `RT_DELDTOR` row and `_typeInfo` row can retire
   ONLY after the four slot-copy sites and every by-address reader of the table are gone; the order
   of retirements per class is part of the rule sheet.

### 2c. Standing rules every prompt carries (B34B_RULES, B35_RULES section 11, B36_RULES sections 8/10/14, B37_RULES sections 8/10/14 hold; these are step 7's additions)
- The four Engine loops are their OWN root (slot 1 `Startup`), not `Object::Base` (B7-1); `Goal` slot
  8 is `CheckGoalState`, not `Construct` (B37_RULES 10); `Goal::GetInstanceType` is the one virtual
  retslot in the batch — `Object::Type* M(Object::Type* ret) const` on every override (B7-4).
- By-value ARGUMENTS per type (B7-5): `ExternalPtr` and `System::Time` in a REGISTER; `InternalPtr`,
  `Collection::Array`, `UI::Sprite` and **`System::String` (8 bytes!)** by HIDDEN POINTER. Read the
  prologue per member.
- Every copy ctor read in these namespaces was DEEP; no class was trivially copyable — assert
  `!is_trivially_copyable`, never the opposite, until read (B7-8).
- A POD that holds a polymorphic POD by value (`Audio::Device` → `Stream`) keeps the POD twin
  cross-asserted beside the class (B7-12); an abstract class is never an alias target of an
  embedded POD (B36_RULES 10).
- Name sweeps match back-references (`@Achievement@1@`, `@Application@1@`, `@Engine@1@`,
  `??_7Achievement@0@6B@`); hex sweeps anchor on the DEMANGLED OWNER of the name, not on `@NS@`
  (B37_RULES 14.6 — K1/K2's sweep missed ordinary class-scoped names, K3's missed back-references;
  the close's `hexall.py` shape is right).
- A warning A/B across a header change compiles the BASE in its own worktree (B37_RULES 14.7).
- A door that never resolves in standalone (an unregistered exe import slot) is a PORT GAP, not a
  design choice: converting it to the member makes retail's path live — do it, in its own commit
  titled as a behaviour change, and gate it (step 7: dragtool key release, citizen emitter teardown,
  the tutorial-instance type check).
- Before writing "not exported", check `rt_export_addrs.inc` (B37_RULES 14.4: five `MainLoop`
  members and `Input::Device::PlatformUpdate` were filed as unexported while exported).

### 2d. The gate for every merge (coordinator only)
Unchanged: debug + release + perf build exit 0 · new-warning set diff vs a clean-rebuild baseline ·
`make unit` · new game + pause-menu quit · tutorial · Load Game dialog · resave gate PASS x3 (x5 for
anything on a serialize or load path — the components ARE that path in step 8) · pointer-free type
table vs baseline · `[RtResolve] 0 misses`, no `[RtMint] TRAP`, 0 GATE WARN, new-game `calls=5686`,
tutorial `calls=5776 openFileFail=15` (Load Game floats 4760-4789) · `dispatch_slot_map.py --check`
0 · cutover manifest gate · `rt_mint_gen --check` · `git diff --stat` against the agent's file set.
At the step close (`b37_session/stepclose.sh`: Mods route with `.pkm` mounted, front-end Quit /
Options / Achievements routes, an in-game save through `Save::SaveLoad` (resave gate file mode),
one `make sa-stress` recorded never chased, true release compile from a clean obj dir FOLLOWED by
`make clean`, clean rebuild of debug/perf/unit, the full gate again on the clean-built exe, the
`Audio::Device` construction check and the `NOT IMPLEMENTED` stub-set diff on the new-game log).
For step 8 add: a component-heavy route (the tutorial save's 5,354 entities load every component
type — `BANISHED_AUTOTUTORIAL` already does; check the entity/component counts in the log against
the baseline), the `GoalDialog` route from a scenario game's pause menu, and per multiple-inheritance
class a unit test that reads the secondary vptr through the base pointer.
