# Next session plan (written 2026-09-26, at the B.3.6 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list` (only the main tree: step 6's 18 agent worktrees and
  every `b36-*` / `worktree-agent-*` branch were pruned at the close, 2026-09-26),
  `git branch --no-merged main` (expect none). Old branches that are NOT this plan's:
  `endgame-a3`, `wave29/base`, `defects-open-5` (merged; can go, local and `origin`).
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`;
  never `find /`.
- Ghidra: `nohup "C:/tools/ghidra_12.0.3_PUBLIC/ghidraRun.bat" Banished_RE.gpr &` from the repo
  root brings the MCP up alone in ~60 s (memory `reference_ghidra_unattended_launch`).
  `switch_program` is GLOBAL: every agent asserts `get_current_program_info` before each read
  batch (retail `Runtime-x64.dll` 15290 functions; the profile DLL 16002 shares the image base
  and is never a source of offsets; `Application-x64.exe` 11473) and switches BACK. Scripts are
  JAVA; `b36_session/ghidra/UiAppendix.java` (vtables terminated on the next `??_7`/`??_R`
  symbol, sizeof from slot 0, parents from the literals, retslot exports, ctor stamp chains) is
  the model — parameterise its namespace list. Oversized script output is read from the saved
  tool-result JSON.
- The gate is still `b34_session/gate.sh` (copied per session into the scratchpad; `<label> <n
  resaves> [skipbuild]`); `rt_mint_gen --check` rewrites `standalone/rt_export_addrs.inc` line
  endings — `git checkout --` it after every gate.

## 1. Carry-overs (each its own gated change; none blocks step 7)

Unchanged from before (details in the B.3.4 / B.3.5 blocks of `STATUS.md`, `B34B_RULES.md`,
`B35_RULES.md` section 11):
1. The IAT-cached `fn_InternalBasePtr_Ctor` / `_DeletePtr` callers in ten files.
2. One `?_typeInfo@` record per name (`Base`, `External`, `ExternalList`, `CompiledExternal`,
   `Internal`): owner task, `rt_data_mint.cpp` + `rt_mint_gen.py` + a live type-table comparison.
3. `ExternalList::Find`'s second parameter is `Object::Type` by value, not `uint32_t`.
4. `System::MemoryAllocator` — deferred on purpose (~1,146 sites); the `iatMemAlloc` fallback in
   `gatherrawmaterials.cpp` / `foresterwork.cpp` that resolves a DATA import goes with it.
   `singleton_ctors.cpp` (Engine-filed) stamps its ctor too — see 2c.
5. The eleven `CUTOVER_REQUIRED` System statics + ten Video/Graphics + (step 7 adds) the nine
   Engine/Audio/Input/Achievement/Application singleton rows: `MintStorageCell` redirect owed at
   the class-static conversion, the END of B.3.
6. `System::Configuration::CurrentBuildId` stays a free door.
7. The 125 call sites through the twelve `ExternalPtr` exe-side doors; `Geometry.h`'s
   `Graphics::ExternalPtrHandle` alias (~30 uses, six headers).
8. Small: eight direction reads with an unproved receiver (`d6ee7bfc`);
   `standalone/res_load_lock.{h,cpp}` live-or-dead; `Time::AsSeconds` / the `Time` ctor reach
   `System::Timer` by name; `Camera.h`'s eight wrong POD field names; `scene_watch.{h,cpp}`'s
   ~60 raw-offset reads; `boot_window.cpp`'s `Configuration+0x42` write; `InterpolatedStateT`
   vs `InterpolatedState<T>`; the nine int64 `ReaderWriter` slot calls (five left).
9. **THE DEVICE-LOST / LOADER RACE NEEDS THE USER'S DECISION, NOT A PATCH**
   (`endgame_b/DEVICE_LOST_LOADER_RACE.md`; retail has the same window; any fix diverges).
   Cheap non-divergent move: print `Device::_singleton`, `activeDeviceData`, `d3dDevice`, lock
   owner tid/depth in the crash dump.
10. The concrete dx11 `*Data` classes and the 84-slot seam stay (RULE B5-8). dx11's import table
    is at 38 live rows: allocator doors, ctor rows, the two singletons, and FOUR `Application`
    rows that wait for `Application` to become a class — step 7, block 1.
11. Quit-during-splash exposure; `MenuDLT` `Stub_Shutdown` skipping the rest of retail's Shutdown
    (a leak; step 7's Engine/Audio loop work owns it); `e353fb15`'s Mods-route timing.

New from step 6 (details in `STATUS.md`'s B.3.6 block, `B36_RULES.md` sections 12 and 14):
12. **76 live by-address calls to NON-ctor UI members inside `runtime/src/ui`** through the
    file-local `RtAddr<Fn>(0x18…)` helper (`SpriteContext::AddSprite` x27, `Dialog::CreateElement`
    x8, `UserInterface::Play` x7, ~30 others), plus 69 ctor/dtor ones. A binding spelling no
    inventory grepped. Each needs its receiver typed: a UI caller batch 3, or B.4. **Every step-7
    sweep scans hex literals against `rt_export_addrs.inc` and the PE import table from day one**
    (the closing scratchpad's `slotlits.py` shape; `STEP7_INVENTORY.md` section 3.5 already did it:
    8 sites).
13. 103 exported UI members declared and not ported (`DialogInstance` 22, `Dialog` 13,
    `UserInterface` 13, `Element` 10, `SpriteContext` 8 …): a caller that appears is a link error.
14. Four `RtUI::` PODs still embed `RtUI::Element base;` (`EditElement.h`, `NumberElement.h`,
    `ProgressElement.h`, `SpinnerElement.h`); `RtUI::Element` lives until the last goes.
15. `g_TypeId_Element`'s lazy fallback fills it with `Object::Internal`'s id when the exe slot is
    unfilled (the default); `g_TypeId_CheckElement` and `g_TypeId_CheckboxElement` bind one slot.
16. Step 8 material, enumerated in `b36_session/EXE_SUBCLASSES.md`: 20 `DialogInstance`, 38
    `DialogController`, 4 `DialogControllerConfig`, 2 `Element`, 2 `ElementDescription` exe
    subclasses on ctor doors + hand-built tables; the thirty QUALIFIED-call sites; the
    `0x14015b070` double claim (`Dialogs.h:23` vs `Panels.h:22`); the `::TutorialSetupDialog` ODR
    violation; the `AchievementDialog` / `ModDialog` multiple-inheritance hint; the four minted-
    table slot-copy sites (`tool_base_toolui.cpp`, `frustumoutlineelement.cpp`, `thumb_vtables.cpp`,
    `dialog_controller_vtable.cpp`) that must convert before the mint rows retire;
    `DialogController`'s dtor behaviour change noted in block B.
17. `TextDescription::CreateInstance` stays a door (reached by address from slot 8).
18. Owed routes: the `GoalDialog` route from a scenario game's PAUSE menu (front-end id 10 is not
    a button — see the menu-driver skill); an Options route that changes a setting and saves
    (`optionsdialog.cpp` has build coverage only; the close drove Options -> Quit).
19. All 68 UI mint / `_typeInfo` / `RT_DELDTOR` rows retire at the END of B.3 with the rest.

## 2. B.3 step 7 — `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`, `Picking` (+ `Application`)

Same shape that has now worked eight times: read-only inventory -> committed rule sheet
`B37_RULES.md` (+ `B37_SLOTS.md`, `B37_FILEMAP.md`) -> ONE owner for the question pass ->
class blocks by DISJOINT FILE SET on worktrees -> coordinator gates each merge -> fragment
pass(es) -> caller batches by directory (two rounds) -> one closing owner -> step close. Session
kit: `b36_session/` (`BLOCK_BRIEF.md`, `CALLER_BATCH_BRIEF.md`, the Ghidra script,
`CLOSE_NUMBERS.md` as the measurement template, `EXE_SUBCLASSES.md` as the model for the exe
loop subclasses), `b34_session/gate.sh`.

### 2a. What is already measured (`STEP7_INVENTORY.md` — a MAP, NOT EVIDENCE; nothing in it was verified by the coordinator)
- Eight namespaces, 628 registrations in 74 files (32k lines): the whole batch is IO-sized
  (~593), not UI-sized (1,663). Header dirs use ONE spelling each (`RtEngine::`, `RtSave::`, …);
  `Pathfinding` has no header at all (`PointList` is defined in its `.cpp`); `Audio`'s
  `SoundBuffer`/`SoundEffect`/`Stream` have no header either.
- Polymorphism is thin: 14 mint rows in all — `Engine` 5 (`UpdateLoop` 12, `LoaderLoop` 12,
  `DialogLoopT` 13, `DialogLoaderLoopT` 13, `Loader` 2), `Achievement` 3 (`AchievementInstance`
  4, `Goal` 10, `GoalInstance` 7), `Audio` 3 (9 each), `Pathfinding` 2 (`IPathInfo` 2,
  `PointList` 9), `Picking` 1 (`PickingMesh` 9); `Save`, `Input`, `Application` none.
  `Engine::MainLoop` has NO mint row despite 25 exports and a singleton manifest row — settle it
  first. `Achievement::Achievement` and `GoalList` have an `RT_DELDTOR` row but no vtable.
  `Engine` has 0 of 6 `_typeInfo` rows (UI had 68/68): read before assuming any Engine class is
  an `Object::External`.
- Census (verbatim rows in the inventory): `Save` 37/29/29 (the ONLY namespace at 100% called
  coverage — start there), `Input` at ceiling, `Pathfinding` 45 rows / 2 called / 0 ported (ask
  whether any live route reaches it before spending a block).
- Callers: 63 `engine_funcs` globals bound to these namespaces with 166 `code/Game` call sites;
  `Picking` is called EXCLUSIVELY through exe dispatch slots (spelling 10); 8 by-address
  `RtAddr<>` sites (spelling 3, the one step 6 missed); the four `Application` rows in
  `dx11/src/devicedata.cpp`.
- Cross-namespace file: `runtime/src/engine/singleton_ctors.cpp` (928 lines) stamps ctors for
  `Audio::{Configuration,Device}`, `Input::{Configuration,Device}`, `Video::{Configuration,Device}`,
  the three Object factories, three IO classes and `System::MemoryAllocator` — owner-retained, not
  split by namespace. Three same-named `Scene` classes (`RtAudio`, `RtPicking`, Graphics):
  always qualify.
- Exe-phase subclasses of the Engine loops: `GameUpdateLoop`, `MenuDLT`, the splash loop, two
  `LoaderLoop` subclasses — five hand-built vtables (`gameupdateloop.cpp`,
  `menudialogloopt_stubs.cpp`, `splash_loop_stubs.cpp`, `loader_loop_stubs.cpp`). This is the
  step's `DialogInstance` constraint: those bases stay abstract with ctor doors, QUALIFIED calls
  inside their slot bodies (B6-3).
- One virtual hidden-retslot export in the batch: `Goal::GetInstanceType` (declare
  `T* M(T* ret, …)` per B5-12 / B6-2). A second thread: `audio_stream_win.cpp`'s
  `_beginthreadex` worker (B6-5-style: no static-init / lazy caches in `Stream` members).

### 2b. Block 0 — the question pass (ONE opus owner, no ported code touched, into `B37_RULES.md`)
1. The class list and hierarchy from the RETAIL DLL byte-wise for all eight namespaces: every
   `??_7` in the export directory, its slot list terminated on the next `??_7`/`??_R` symbol,
   `sizeof` from each slot-0 deleting dtor's `MOV EDX,imm`, the `_TypeName`/`_ParentTypeName`
   literals where they exist, who stamps what (`singleton_ctors.cpp` is the map). Decide
   `Engine::MainLoop`'s polymorphism from the bytes; decide whether `Achievement`/`GoalList` are
   value types with an out-of-line dtor.
2. The `engine/types` mirrors (`AchievementGoals.h`, `PickingComponent.h`, the loop PODs in
   `engine/types/*Loop*.h` if any survive) vs `runtime/types/`: ONE definition per layout; the
   B6-4 rule (an abstract class cannot be the alias target of an embedded POD) decides which.
3. The exe-side subclasses: all five loop vtables, the seven-class `Goal` family in
   `code/Game/Goal/*` (`engine/types/AchievementGoals.h`), `PickingComponent`. Enumerate every
   QUALIFIED-call site they will need (the B6-3 shape), into `b37_session/EXE_SUBCLASSES.md`.
4. `Application`: not polymorphic, zero named exports, four dx11 import rows waiting — is step 7
   the step that makes it a real class (recommended: yes, in block 1, so dx11's table can drop
   the rows), and what does `Engine::Startup`'s ordering need from it.
5. Threading: the loader thread (constructs Save/Achievement objects while the menu draws), the
   audio worker thread, the main thread — which classes are touched from two threads and where
   B6-5's "no static-init / lazy cache in a member" applies.
6. By-value holdings and hidden retslots per class (`String`, `Filename`, `Array`, `ExternalPtr`
   members; `SaveInfo`/`ModInfo` returned by value?); read the prologues, never assume (B6-11).
7. File ownership: which of the 74 files serve more than one class or namespace
   (`singleton_ctors.cpp`, `loops.cpp` holds four Engine classes, the three distinct
   `closure_batch2b.cpp`).
8. `Pathfinding`: is any of it reachable from a live route (census says 2 called / 0 ported)?
   If not, the block is a header + declarations only, and the rule sheet says so.

### 2c. Proposed blocks after block 0 (inventory section 5 — confirm disjointness first)
1. `Engine` (`MainLoop`, `UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`)
   + `Application` — serial, one owner: the highest exe-subclass blast radius (five hand-built
   vtables), and it owns `singleton_ctors.cpp` and `loops.cpp`. `MenuDLT`'s `Stub_Shutdown` leak
   (carry-over 11) is fixed here if the retail body says so.
2. `Save` (`SaveLoad`, `SaveInfo`, `ModInfo`, `SavedState`; 9 files) — the cleanest block; run it
   FIRST in parallel with block 1 to bank the model case. Anything on a serialize/load path gets
   resave x5 at its gate.
3. `Achievement` (`AchievementManager`, `Achievement`, `AchievementInstance`, `Goal`,
   `GoalInstance`, `GoalList`; 11 files) + the exe `Goal` family's QUALIFIED calls.
4. `Audio` (`Channel`, `EmitterInstance`, `EffectSlot`, `Scene`, `Configuration`, `Device`,
   `SoundBuffer`, `SoundEffect`, `Stream`; 15 files) — after block 1 settles
   `singleton_ctors.cpp`; the worker thread read first.
5. `Input` (`KeyMap`, `Binding`, `BindingNode`, `Device`, `Configuration`; 9 files) — after block
   1; nearly mechanical.
6. `Pathfinding` + `Picking` (`PointList`, `IPathInfo`; `Scene`, `PickingMesh`, `SubMesh`, `Cell`,
   `Link`, `Entry`; 8 files) — `Picking`'s callers are all exe dispatch slots: the caller batch
   greps hex literals from the start.
7. Fragment pass (bodies of a converted class in another block's file), then caller batches by
   directory: `code/Game/Game` (Engine/Save/Achievement, mixed bindings), `code/Game/Player` +
   `Component` (Picking's dispatch-slot calls), `code/Game/Goal`, `code/Game/Tutorial` (Audio),
   `standalone/` (the loop stubs, `boot_*`, autostart), `dx11/` (the `Application` rows),
   `runtime/src` outside the blocks; a second round; one closing owner (`engine_funcs` sweep BY
   BINDING including `engine_funcs.h`'s inline bodies — B36_RULES 14.2; dead caches; hex-literal
   sweep; numbers per `b36_session/CLOSE_NUMBERS.md`).

### 2d. Standing rules every prompt carries (B34B_RULES, B35_RULES section 11, B36_RULES sections 8, 10, 14 still hold; these are the ones step 6 added or sharpened)
Class shape
- A by-value return through a VIRTUAL is `T* M(T* ret, args…)` in retail order (GCC ms_abi puts
  the hidden return pointer BEFORE `this`). Non-virtual by-value members sit behind a
  `(this, ret)` door that placement-news the prvalue.
- **An `InternalPtr<T>`, `UI::String`, or any >=16-byte class ARGUMENT by value arrives by HIDDEN
  POINTER; an `ExternalPtr<T>` by value is a bare register pointer** (B6-11, eleven prologues).
  Read the prologue per member; `Element::AddChild` was the one that slipped.
- Slot 0 `DeletingDtor(unsigned) = 0` while its body is a `deleting_dtor.cpp` row; no virtual
  dtor, no real ctor on a polymorphic class; ctor/dtor doors stay (B36_RULES 7); a concrete class
  only when something holds it BY VALUE (protected `= default` ctor + `friend` the holder).
  Trivial copyability is a CLAIM to verify per class: `SpriteContext` and `UserInterface` both
  had exported deep copy ctors / destructors over `Array<T>` members.
- An ABSTRACT class cannot be the alias target of an EMBEDDED POD: the POD moves to `Rt<NS>::`
  and is cross-asserted field for field; the engine-side header becomes a one-line alias of the
  POD, not of the class (B6-4 correction).
- `Construct`/`On*` slot indices are read from the slot list per class, never assumed (B6-1:
  `DialogInstance` `On*` at 4..9 vs `DialogController` 4..10; `Construct` at 9 on leaf
  Descriptions, 8 on resources, absent on the base). `virtual`, not `override`, on the branch's
  first class.
- Type-name literals are copied from the binary per class (`ToolTipDialog`'s parent literal is
  `"UI::DialogInstance"` — same namespace, still qualified). Declare EVERY exported member up
  front (`// not ported`, defined nowhere).
- A converted call INSIDE a slot body of a hand-built exe-phase subclass is QUALIFIED
  (`->Engine::UpdateLoop::Update()`): the five loop subclasses are exactly this shape.
- No static-init or function-local lazy cache in a member of a class constructed on another
  thread: a namespace-scope atomic slot with the `RtResolveCached` shape (stores only what
  `RtResolveCached` itself would cache — B36_RULES 14.4).
- Toolchain: mingw-w64 emits typeinfo as a COMDAT per TU (a leaf's `TypeName` can be defined
  while the base's is a door); two `*_common.h` files defining the same `enum class` cannot share
  a TU; an unqualified call inside a member binds to the class's own undefined member first.
- Retail slips are transcribed, not fixed (`GraphElement::SetYDivisions`); a PORT bug found on
  the way is fixed faithfully and recorded (`ComboDialog::OnUpdate`).
Finding callers — by BINDING; ELEVEN spellings now
- The ten in B35's list, plus 11: **a file-local by-address helper taking a hex literal**
  (`RtAddr<Fn>(0x18…)`, `resolveIAT()`, `RSlot()`): scan every `0x18…`/`0x14015…` literal
  against `rt_export_addrs.inc` and the PE import table. Step 6 missed 235 sites this way.
- `engine_funcs` globals by their INITIALISER (155 UI-bound by initialiser vs 118 by name:
  exe-slot bindings and duplicate bindings of one export are part of the population), and the
  sweep INCLUDES `engine_funcs.h`'s own inline bodies (four live UI door calls sat there).
- A readiness guard, `Ready()` conjunct, `DiagPrint` row or `(void*)fn_X` printf of a door nobody
  calls is dead code and goes with the global. Docs, comments and strings are not callers.
- A per-door "converted" tally can read complete when it is not: finish with a line-by-line
  re-sweep of the final tree.
Process
- Blocks own FILES, not classes; foreign-class bodies stay doors and go on the fragment list;
  callers in two rounds; blocks do not touch `tests/unit/sources.mk`; helper scripts under
  `scratchpad/<batch>/<worker>/`; `git diff --stat` against the file set before every merge;
  commit messages and scripts as FILES (heredocs eat backslashes); no `git stash`/amend/rebase.
- More than three agents that run the game starve the game dir: caller batches are static-gate
  only; class blocks run one route at the end under `tools/game_dir_lock.sh`; the coordinator
  gates every merge and the coordinator alone runs the step close.
- The cutover manifest prints a COSMETIC `stale` warning when by-name door counts move; refresh
  `standalone/cutover_manifest.inc` with `python tools/cutover_manifest.py --inc` at the close
  (never hand-edit).
- `[AutoStart]` and `[BootState]` are in `standalone_error.log`; a crash is `standalone_crash.log`
  first, symbolised with `python tools/symbolize.py <exe> <offsets…>` against the SAME exe
  (copy it aside before a clean rebuild); a `NOT IMPLEMENTED` stub-line set is compared to the
  baseline run's set (step 6's new-game run reaches ten FEWER than the baseline: a good sign, and
  a new one is a finding).

### 2e. The gate for every merge (coordinator only)
Unchanged: debug + release + perf build exit 0 · new-warning set diff vs a clean-rebuild
baseline · `make unit` · new game + pause-menu quit · tutorial · Load Game dialog · resave gate
PASS x3 (x5 for anything on a serialize or load path — `Save` is that block) · pointer-free type
table vs baseline · `[RtResolve] 0 misses`, no `[RtMint] TRAP`, 0 GATE WARN, new-game
`calls=5686`, tutorial `calls=5776 openFileFail=15` (Load Game floats 4760-4789) ·
`dispatch_slot_map.py --check` 0 · cutover manifest gate · `rt_mint_gen --check` · `git diff
--stat` against the agent's file set. At the step close (the B.3.6 script `b36_session/stepclose.sh` = Mods
route with `.pkm` mounted, front-end Quit / Options / Achievements routes, one `make sa-stress` recorded never chased, true release compile from a
clean obj dir FOLLOWED by `make clean`, clean rebuild of debug/perf/unit, then the full gate again
on the clean-built exe). For step 7 add: an Achievements route that opens a goal (block 3), an
Options route that changes and saves a setting (`Input`/`Audio` configuration), a save-then-load
round trip through the pause menu (`Save`), and a run with the audio device present (the stress
run is silent by default — check `Audio::Device` is constructed on the plain new-game route).
