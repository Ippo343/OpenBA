# Next session plan (written 2026-09-28, at the close of the normal-virtual cutover)

State and counts live in `../STATUS.md` (the B3X block) and `b3x_session/CLOSE_NUMBERS.md`; this file is only
the plan. Replace it when the session it describes has started.

**What the cutover left behind, in one line:** every class whose retail slot 0 is a deleting destructor has
`virtual ~T()` (449 classes, five roots), deletion is `delete p`, nothing in product code reads a flipping
class's table by literal index, and the twelve `Video::*Data` classes are the one exception, by decision
(`B3X_RULES.md` X1). ENDGAME B.1 decision 2 is finished. What is left of B is the REGISTRY (the doors by name
and by address that real classes no longer need) and then the cast metric (B.4). This file plans the registry
teardown.

## 0. Owed from the cutover (each its own titled commit; none blocks the step)

1. **`UI::DialogInstance::Update` passes the wrong fourth argument to `Contains`** —
   `runtime/src/ui/dialog.cpp:1646` passes `_dialog` (+0x18); retail passes `_ui` (+0x10)
   (`0x180113bb3 MOV R9,[RBX+0x10]`, then `CALL [RAX+0x48]`). A BEHAVIOUR change: own commit, resave x5 and
   `uiroutes.sh`. The call then becomes by-name (today the slot is taken from the member with
   `Object::SlotAddressOf`, which is shift-proof but is not a call by name).
2. **`Graphics::AnimationEvent::Construct` (retail slot 8) is declared without arguments**; retail passes the
   transform. `runtime/src/graphics/wave28_lane_gfxbody.cpp:1174` dispatches through `SlotAddressOf` with
   retail's two arguments. Give the member its retail signature (the unit tests' overrides pin the empty
   list: they change in the same commit), then call by name.
3. **`Video::DeviceData::Slot4()` is declared without the bool retail passes in DL**
   (`runtime/src/engine/loops.cpp:1111/1131`). An exempt class; the declaration is still wrong.
4. **Dead code found on the way:** `StateMachine_Tick_Reimpl` and the `BehaviorNode_*` helpers
   (`code/Game/Game/statemachine.cpp:55-100`, declaration `statemachine_funcs.h:83`) have no caller;
   `factory_vtbl` (`engine/types/io_serializer.h:205`) has no user. Delete both.
5. **A stale comment:** `tests/unit/video_imagebuffer_abi_test.cpp` PART D says `saveload_draw.cpp` walks
   `+0x50` by hand; `runtime/src/save/saveload_draw.cpp:468` calls `Generate` by name.
6. **`partc.py` still misses four shapes** (`b3x_session/W0_AUDIT.md` §6: a `static_cast` byte offset on the
   vptr, a POD field named `_vftable`, a typed function-pointer table indexed with `[N]`, a typed slot-0
   read with no index). The audit read every product site by hand, so nothing is known to be missed; add
   the regexes before the script is used as a gate again.
7. The standalone harnesses' `OnElementEvent` calls now pass `0 / 0 / 0 / nullptr` where the raw calls left
   four registers unset (env-gated harness code only; `b3x_session/GATE_TABLE.md` row 7).

## 1. Sync (5 minutes)

- `git log --oneline -8`; `git worktree list` must show main only; `git branch --no-merged main` empty.
- Greps exclude `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`; never `find /`. For "is this
  still read" use `b3x_session/codegrep.py` (comment-stripped) — a raw grep counts history comments.
- The kit is `b3x_session/`: `gate.sh`, `mergegate.sh`, `finishmerge.sh`, `uiroutes.sh`, `toolroutes.sh`,
  `devlost.sh`, `stepclose.sh`, `mods_route.sh`, `acceptance.sh`, `partc.py`, `classes.py`, `flip.py`,
  `codegrep.py`, `reftable_diff.py`. Copy it into the session scratchpad (its `runs/` directory is not
  committed) and cut the warning baseline from a `make clean` rebuild FIRST
  (`gate.sh baseline 3`, then `gate.sh baseline 3 warnbaseline`).
- **Memory:** agents build at **-j3**, at most **~5 agents at once**, the gate at -j4.
- **Invariants:** new game `calls=5683 openFileFail=0`; tutorial `calls=5773 openFileFail=12` (Load Game floats
  4758-4787); resave golden md5 `dd3893af7772e3f58a3bb51dd5c29a9f`; typetable 428 lines md5 `b983f144`;
  unit **919 tests, 0 failures**.
- Ghidra: `check_connection` first; pass `program=` on every call, never `switch_program`.
- **Worktree agents:** `isolation: "worktree"` cuts from HEAD, and once in this step a worktree was cut from
  an OLD commit (the audit's, from `a699884d`). Every brief says: `git log --oneline -1` first, and
  `git reset --hard main` / `git merge main` if the tip is not main's. A long-running agent that builds in the
  background sends interim "waiting for the build" notices: they are not hand-ins.
- An owner that must work on top of another owner's unmerged branch: `git checkout -b <new> <that branch>`
  in its own worktree (the tests owner did this on `b3x-W1`). The coordinator can gate an unmerged branch
  with `git checkout --detach <branch>`, a build, and `gate.sh <label> 5 skipbuild`.

## 2. Carry-overs (each its own gated change; numbers are the B.3.9 close's unless marked)

1. `System::MemoryAllocator` — **open**: its storage row is still minted; 289 code lines name it
   (re-measured). Part of the step below.
2. `System::Configuration::CurrentBuildId` free door — **open**.
3. `ExternalPtr` exe door sites — **open** (D10's 125).
4. `scene_watch.cpp` reads 0x180100f20 as an identity token — **open**.
5. **Device-lost / loader race — DEFERRED by the user (2026-09-27) until ALL endgame work is finished. Do not
   fix, guard or investigate it; `devlost.sh` stays a crash check on Video / Graphics / dx11 merges.**
   (`DEVICE_LOST_LOADER_RACE.md`; the run is killed at its time limit with the round trip COMPLETE.)
6. **dx11 `*Data` seam** (84 slots) — **open**, and now ALSO the last `DeletingDtor`: the twelve
   `Video::*Data` classes flip when dx11's types become real subclasses of them (then the seam's tables are
   the compiler's and no re-index is needed). `static_assert(!has_virtual_destructor)` on each pins today's
   state; 8 `DeletingDtor(1)` calls and `Dx9Placeholder` (CRT heap, `dx11/src/devicedata.cpp:1932`) ride on it.
7. Quit-during-splash; Mods-route timing — **open**, not re-measured.
8. UI leftovers — **partly open** (`ResolveAddr(Module::Runtime` lines in `runtime/src/ui`, four
   `RtUI::Element base;` PODs, the Options-change route has no menu token).
9. `Math::Vector3::operator==` 0x140152a70 read through an import slot (`citizen_assignworkplace.cpp:123`).
10. PORT GAP / STANDALONE DEVIATION — **open**: 149 PORT GAP lines (165 at the B.3.9 close), 49 deviations.
11. Lazy `X_ResolveFuncs` — **open**: 36 definitions, 154 calls (re-measured, unchanged).
12. By-name concrete lookups, dead `Dispatcher<Tag>` members — **closed**.
13. `g_pGameWorldSingleton + 0x…` raw reads outside `code/Game/Game` — **open**, not re-measured.
14. The PORT GAP message targets `kCitizenAIMessageTargetTypeId` / `kHappinessMessageTargetTypeId` stay 0
    until `CitizenAIComponent::OnMessage` 0x14010f340 and `HappinessComponent::OnMessage` 0x14004a780 are ported.
15. `AutoPickupComponent`'s Startup (retail slot [4] 0x140044990) is not installed.
16. `EventNotifyDialog` / `AutoSaveDialog` construction keeps standalone `dialogRes != nullptr` skips.
17. The `InitFields` / `ReleaseFields` seams kept for the unit tests' recording stand-ins.
18. The four `DEFINE_EXE_VTABLE` lines (GameUpdateLoop, MenuDialogLoopT, GameLoadDialogLoopT, StatGoal): their
    readers print or compare WHOLE vptrs (unaffected by the flip); they go when the reader is a typed check.
19. **From the cutover:** `Graphics::Light`, `Object::ExtensionCompiler`, `Object::ExternalCompiler` keep
    protected destructors (they declare none of their own slot 0); in-class `operator new` declarations that
    nothing references stay undefined; test NAMES that mention the retired rule were kept (their bodies state
    the new fact); 6,397 `// B.3.x bridge` lines (W0 added ~280: by-name calls on `void*` receivers).

## 3. The step: the registry teardown

**Goal.** A real class needs no door. Every row that registers one of OUR bodies under a retail name or a
retail address, and every resolve that reads such a row, is either converted to a direct C++ reference or
deleted; what is left is the list of things that genuinely have no C++ name yet. End state (ENDGAME B.2
last paragraph): `standalone/address_registry.cpp`, `rt_data_mint*.inc` and `rt_export_addrs.inc` empty and
deleted with `standalone/resolve*.cpp`; `engine/funcs/` dissolved into `code/Game` headers.

### 3a. Measured at the cutover's close (code lines, `codegrep.py`)

| what | count |
|---|---:|
| `standalone/rt_export_addrs.inc` rows (GENERATED by `tools/rt_mint_gen.py`) | 4,926 (1,480 with a reader at the B.3.9 close) |
| `RegisterRuntimeExport(` / `RegisterRuntimeAddr(` | 1,257 / 1,452 |
| `RegisterExeAddr(` | 696 |
| `standalone/address_registry.cpp` | 1,243 lines |
| `standalone/reg_*.cpp` | 127 files |
| `ResolveAddr(Module::Exe` / `ResolveAddr(Module::Runtime` | 2,185 / 275 |
| `ResolveExport(` / `RT_EXPORT_ONCE(` | 204 / 242 |
| `engine/funcs/engine_funcs.h` / `.cpp` | 3,035 / 1,651 lines, 228 externs |
| `RT_DATA_SLOT` | 5 |
| minted storage rows | 2 (`?_singleton@MemoryAllocator@System@@`, `?_disableHandler@Debug@System@@`) |
| `X_ResolveFuncs` | 36 definitions, 154 calls |

### 3b. Block 0 — the questions (read-only, then ONE owner writes the rule sheet)
1. **Readers per row, by BINDING** (`feedback_sweep_callers_by_binding`: ten spellings — exe import slots
   dominate code/Game; engine_funcs globals bind by initialiser). For every registered name and address:
   who resolves it, through which door, and is the resolver itself still called? Classify each row:
   `DEAD` (no reader: delete), `DIRECT` (the reader can name the C++ function or member: convert the reader,
   then the row is DEAD), `STORAGE` (a data cell: becomes a class static or a namespace-scope object),
   `STUB` (the target is a `NOT IMPLEMENTED` stub or a PORT GAP: stays, listed), `CALLBACK` (an address
   stored in DATA and called later — what `STATUS.md`'s closure block warns about: state, not code).
2. **What reads BY ADDRESS at run time?** With no retail binary loaded, an address is only a key. List
   every place a retail address reaches a resolver from DATA (serialized function ids, tables of addresses,
   state-machine tables, `DEFINE_EXE_VTABLE`), because those rows cannot be found by grepping names.
3. **Order.** Rows die at zero readers by name AND by address; generated `.inc` files are regenerated, never
   hand-edited (the generator REFUSES a retired row that still has a reader — keep that mechanism,
   `tools/rt_mint_retired/`). Which resolvers are lazy caches on the loader thread (B6-5 atomics)?
4. **`System::MemoryAllocator`** (carry-over 1) and **`System::Debug::_disableHandler`**: what it takes to
   make them class statics, so the mint (`rt_data_mint.*`, `rt_mint_gen.py`, `RT_DATA_SLOT`) can be deleted.
   The five root `operator delete`s of `runtime/src/object/root_operator_delete.cpp` and the 41 in-class ones
   free through `Object::detail::InternalMemFree` (an `RT_EXPORT_ONCE` door): they become direct calls here.
5. **`engine_funcs`**: per extern, its readers by binding; which externs are typed function pointers to OUR
   bodies (convert to a declaration of the function), which are storage, which are stubs.
6. **The boot log is part of the gate.** `[RtResolve] 0 misses`, `[RtMint] registered … / minted …`,
   `[RtPort] … exports + … addresses registered`, the `NOT IMPLEMENTED` stub set: deleting rows changes these
   lines. Decide which lines are invariants and which are re-cut in writing (as the typetable was).

### 3c. Shape (what worked nine times, and once more in the cutover)
Read-only inventories (parallel) → ONE rule sheet + file map, committed → blocks on worktrees with DISJOINT
FILE SETS (blocks own files, not classes) → the coordinator merges and gates each → an independent AUDIT
before anything irreversible → one closing owner → step close. Two lessons from the cutover:
- **Make the dangerous change checkable first.** W0 turned every index read into a by-name call while
  nothing had shifted, so the flip itself was a rename the compiler checked and its gate showed no diff.
  The registry's equivalent: convert READERS to direct references first (rows stay, behaviour identical),
  delete rows only when the generator and a link prove nothing reads them.
- **The measure undercounts until block 0 has read the code.** `partc.py` saw 190 of 1,205 runtime rows
  and a tenth of the tests. Expect the same of any grep-built number in §3a.

### 3d. Standing rules (B34B … B39, B3X hold)
- Retail wins; a behaviour change is its own titled commit gated with resave x5.
- Never "fix" a faithful crash with a null check; never `getenv`; no new flag or instrument; no
  `Register*()` inside a `printf`; generated `.inc` files are regenerated, never hand-edited.
- Nothing reads a vtable by literal index: a call is `p->Member(…)`; a diagnostic takes the slot from the
  member (`runtime/types/object/VSlot.h`). Tests write RETAIL slot numbers through `tests/unit/slot_check.h`.
- Deletion is `delete p`; in-place destruction of a stack / embedded / placement object is the QUALIFIED
  `p->T::~T()`; `Destruct` / `Reconstruct` call the UNQUALIFIED `this->~T()`. A cast before `delete` names a
  class of the object's OWN hierarchy (`Object::Base`, `Graphics::Geometry`, `IO::Stream`,
  `IO::ReaderWriter`, `Engine::UpdateLoop` are five different roots).
- A new polymorphic class under one of those roots inherits its root's `operator delete`; a class outside
  them that is ever `delete`d needs its own (the global one is the C runtime's `free()`).

### 3e. The gate for every merge (coordinator only)
`b3x_session/gate.sh` unchanged; `uiroutes.sh` / `toolroutes.sh` on UI / tool merges; `devlost.sh` on Video /
Graphics / dx11 merges; `acceptance.sh` stays at target (a `DeletingDtor` that reappears is a regression).

### 3f. Acceptance
Every row classified; 0 `DEAD` rows left; 0 readers of a `DIRECT` row; the mint deleted or down to rows
listed by name with the reason; `engine_funcs` externs = the `STUB` / not-yet-ported list; the gate passes;
`stepclose.sh`.

## 4. After it

1. **B.4, the cast metric**: `reinterpret_cast` lines 29,159 (29,870 at the B.3.9 close) and the bridge
   markers. Most remaining casts are the C-body shape (`void*` receivers, raw offset reads): they go with
   typed members and typed parameters, per directory.
2. **The dx11 seam** (carry-over 6): dx11's companion types become subclasses of `Video::*Data`; the twelve
   classes flip; `DeletingDtor` leaves the tree.
3. ENDGAME B.1 decision 7 (exceptions: ~25 files on setjmp / longjmp / SEH).
4. Then, and only then, the deferred device-lost work.
