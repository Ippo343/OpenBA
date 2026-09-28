# Next session plan (written 2026-09-27, at the B.3.9 close)

State and counts live in `../STATUS.md` (the B.3.9 block) and `b39_session/CLOSE_NUMBERS.md`; this file is only
the plan. Replace it when the session it describes has started.

**What step 9 left behind, in one line:** B.3 is closed — no minted vtable, no minted type record, no `RT_DELDTOR`
row, no hand-built table, no factory interpreter, no exe vtable registration, no type-id cell read; the mint keeps
two storage rows. Every class's vtable is the COMPILER's. That is what makes ENDGAME B.1 decision 2 legal now:
`DeletingDtor(unsigned)` → `virtual ~T()`, `Object::Destroy` → `delete`. This file plans that step.

## 0. Owed from step 9

1. **The step close is DONE** (STATUS, B.3.9 block) except the forced device-lost route, which the harness
   killed for low memory with the game process at 6.6 GB. It found one real fault: the -O2 exe corrupted the heap
   (lifetime dead-store elimination deleted a ported destructor's field-nulling stores). `-fno-lifetime-dse` is
   now MANDATORY in the Makefile and `gate.sh` runs a `newgame_perf` route on the optimised exe — keep both.
   In that run the terrain texture slid with the camera (user's report) — filed with the deferred device-lost
   bugs (§2 item 5).
2. `code/Game/Dialog/spritecaptureui.cpp:200` reads retail `.rdata` 0x14015d424 as a raw `const char*` (`".rsc"`):
   in our exe that address is our own image. One-line fix (`".rsc"`), its own titled behaviour commit.
3. The four `DEFINE_EXE_VTABLE` lines left (GameUpdateLoop, MenuDialogLoopT, GameLoadDialogLoopT, StatGoal) each
   have a reader; they go when the reader is a `dynamic_cast`/typeid-free equivalent — not urgent.

## 1. Sync (5 minutes)

- `git log --oneline -8`; `git worktree list`. Step 9's worktrees live under `.claude/worktrees/`: remove them
  (`git worktree remove`), then prune every `b39-*` and `worktree-agent-*` branch with `git branch -d` — all are
  merged; `git branch --no-merged main` must be empty.
- Greps exclude `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`; never `find /`. For "is this
  still read", use `b39_session/codegrep.py` (comment-stripped) — a raw grep counts history comments.
- The gate kit is `b39_session/`: `gate.sh` (at -j4), `mergegate.sh`, `finishmerge.sh`, `uiroutes.sh`,
  `toolroutes.sh`, `devlost.sh` (injection at Present #40000: INJECT=1, COMPLETE ≥ 1, crash log 0),
  `stepclose.sh`, `acceptance.sh`, `partc.py`. The close removed the manifest step from `gate.sh` and the
  manifest regeneration from `mergegate.sh` / `finishmerge.sh`; copy the kit into the session scratchpad from
  main, cut the warning baseline from a `make clean` rebuild FIRST (`gate.sh baseline 3 warnbaseline`).
- **Memory:** the machine ran out at -j12 with several agents building. Agents build at **-j3**, at most **~5
  agents at once**, the gate at -j4. (The perf sub-make forces -j16 inside the Makefile; it has not failed alone.)
- **Invariants:** new game `calls=5683 openFileFail=0`; tutorial `calls=5773 openFileFail=12` (Load Game floats
  4758-4787); resave golden md5 `dd3893af7772e3f58a3bb51dd5c29a9f`; typetable 428 lines md5 `b983f144`
  (`b39_session/TYPETABLE_414.txt`), chain 414 records, 414 distinct ids.
- Ghidra: check the bridge first (`check_connection`); pass `program=` on every call, never `switch_program`.

## 2. Carry-overs (re-checked against the tree at the B.3.9 close; each its own gated change)

The old list (`git show ab1e0846:docs/runtime_port/endgame_b/NEXT_SESSION.md` §2):
1. `System::MemoryAllocator` — **open**: its storage row is still minted; 289 code lines name `MemoryAllocator`
   (3,599 with `MemAlloc`/`MemFree`); `RT_DATA_SLOT` ×2, by-address ×4. Registry teardown.
2. `System::Configuration::CurrentBuildId` free door — **open** (11 code lines).
3. `ExternalPtr` exe door sites — **open** (D10's 125; 185 code lines in `code/` name `ExternalPtr<` /
   `ExternalPtrHandle`, 62 the alias).
4. The small list — **open**, not re-measured (`scene_watch.cpp` still reads 0x180100f20 as an identity token).
5. **Device-lost / loader race — DEFERRED by the user (2026-09-27): a known bug, worked on AFTER all endgame
   work is finished. Do not fix, guard or investigate it during the endgame; `devlost.sh` stays a crash check on
   Video / Graphics / dx11 merges only.** Background: (`DEVICE_LOST_LOADER_RACE.md`) plus the three pre-existing
   device-lost findings of `b39_session/GATE_TABLE.md` (front-end fault in `MainLoop::OnVideoError` on a null
   `SaveLoad::_singleton`; the world-build race; repeated real `DXGI_ERROR_DEVICE_REMOVED` recoveries in game).
6. dx11 `*Data` seam (84 slots, RULE B5-8) — **open** and it MATTERS to the next step (§3c); `appSingleton` data
   row — **closed** (0 lines).
7. Quit-during-splash; Mods-route timing — **open**, not re-measured.
8. UI leftovers — **partly open**: 45 `ResolveAddr(Module::Runtime` lines in `runtime/src/ui` (was 76 by-address
   non-ctor calls); four `RtUI::Element base;` PODs remain; the Options-change route still has no menu token.
9. Step 7 leftovers — **open**: `Math::Vector3::operator==` 0x140152a70 still read through an import slot
   (`citizen_assignworkplace.cpp:123`).
10. PORT GAP / STANDALONE DEVIATION — **open**: 165 PORT GAP markers (142 at step 8's close) and 49 deviations.
11. Lazy `X_ResolveFuncs` — **open**: 36 definitions, 154 calls.
12. Raw dispatch sites / delegating members — **open**, not re-measured as such; `partc.py` measures the raw
    slot reads the next step must convert anyway (§3a).
13. By-name concrete lookups — **closed** (0 literal `GetTypeIdByName` / `ResolveTypeIdByNameLazy`); the dead
    `Dispatcher<Tag>` members — **closed** (COORD-0); reader-less registry rows — **mostly closed** (COORD-0 286,
    TR 157 deleted), the rest is registry teardown.
14. `g_pGameWorldSingleton + 0x…` raw reads outside `code/Game/Game` — **open**: 230 lines.
15. B38 §6 NOT VERIFIED item 2 (type-table order) — **closed** (MEASURED order-independent, `b39_session/
    ORDER_MEASUREMENT.md`); items 4, 5, 7, 9 as B38 wrote them.

From step 9 (`B39_RULES.md` §10):
16. The PORT GAP message targets `kCitizenAIMessageTargetTypeId` / `kHappinessMessageTargetTypeId` (19 uses) stay 0
    until `CitizenAIComponent::OnMessage` 0x14010f340 and `HappinessComponent::OnMessage` 0x14004a780 are ported.
17. `AutoPickupComponent`'s Startup (retail slot [4] 0x140044990) is not installed; its ported body with
    `AutoPickupDescription::TypeId()` / `isEnabled` runs nowhere.
18. `EventNotifyDialog` / `AutoSaveDialog` construction keeps standalone `dialogRes != nullptr` skips; retail
    constructs unconditionally.
19. The `InitFields` / `ReleaseFields` seams kept for the unit tests' recording stand-ins (UpdateLoop, LoaderLoop,
    DialogLoopT, DialogLoaderLoopT, Particle, SunLight, Thread, DialogInstance, Element, ElementDescription,
    ImageBuffer).
20. The non-polymorphic ctor / dtor doors (99 ctor + 37 dtor registered by name, incl. the 12 `Video::*Data`
    doors dx11 calls) — registry teardown.

## 3. The step: the normal-virtual cutover (ENDGAME B.1 decision 2)

**Goal.** Every class with a destructor in its retail slot 0 declares `virtual ~T()` instead of
`virtual T* DeletingDtor(unsigned)`; `Object::Destroy(p)` and `p->DeletingDtor(1)` become `delete p`; the
`DeletingDtor(0)` self-calls of `Destruct` / `Reconstruct` become `this->~T()`. Decision 2's premise — "changes
nothing observable because every vtable is compiler-generated and every delete goes through the helper" — holds
for DISPATCH BY NAME only. It does not hold for anything that reads a vtable BY INDEX:

**GCC puts TWO destructor slots where MSVC puts ONE.** Itanium's `virtual ~T()` is a complete-object dtor AND a
deleting dtor, slots 0 and 1; MSVC's scalar/vector deleting dtor is one slot. So the cutover SHIFTS every later
virtual of every such class by one slot, and slot 0 stops being "delete me": `vt[0](p, 1)` would run the complete
dtor and ignore the flag (a silent leak), `vt[N]` would call the neighbour. The list below — not the declaration
count — is the size of the step.

### 3a. Measured at the close (`python docs/runtime_port/endgame_b/b39_session/partc.py`; `--list <name>` prints sites)

| what (code lines) | count | where |
|---|---:|---|
| `DeletingDtor(unsigned)` declarations / definitions | 902 | code/Game 634 (Component 314, Dialog 122, Work 46, Tutorial 42, Game 38, Action 36, Player 24, …), runtime/types 142 (ui 68, graphics 19, video 19, io 17, …), tests 119, standalone 4, runtime/src 3 |
| `DeletingDtor(0)` self-calls (`Destruct` / `Reconstruct`) | 466 | Component 166, tests 109, runtime/src/ui 74, Game 29, Tutorial 27, video 11, io 9 |
| `DeletingDtor(1)` | 53 | tests 32, video 10, ui 5, … |
| `Object::Destroy(` | 19 | tests 17, achievement 2 |
| `CallSlot0(` | 4 | video 3, ui 1 |
| explicit `~T()` calls (`->~X()` / `.~X()`) | 1,051 | every one on a class that turns virtual must be re-read: qualified `X::~X()` stays non-virtual, unqualified starts dispatching |
| **raw slot-0 dispatch** `vt[0](p, …)` | **56** | 45 outside tests; most are deletes (`(p, 1)`) → `delete p`; NOT all: `statemachine.cpp:55` (a state handler), `thread_win.cpp:333` (`Thread::Run`), `graphicsmesh_vtable.cpp:486/488` and `edgemesh_vtable.cpp:452` (IO::Encoding tables), `font.cpp:410` (a factory flag), the controller `Step` tests — tables whose slot 0 is not a dtor and must NOT shift |
| **raw `vt[N≥1]` reads** | **160** | tests 33, Component 26, standalone 18, Player 17, Dialog 14, graphics 11, ui 11, Game 9 |
| **vtable byte offsets** (`vt + 0x..`) | **27** | Component 16, ui 3, Game 2, standalone 2 |
| **slot helpers** (`VSlot`, `SlotOf`, `VCall*`, `CallVSlot`, …) | **116** | tests 24, Dialog 22, ui 21, object 13, graphics 12, save 12 |
| **PMF-order unit checks** (`PmfSlot`) | **92** | tests only — they pin today's order and will all move |
| **dx11 seam slot indices** | 23 lines, **84 slots** | `dx11/funcs/dx11_seam.cpp` builds the twelve `*Data` tables by RETAIL slot number |

**Classes whose retail slot 0 is NOT a deleting destructor** (they must keep their layout; retail has no
destructor slot to become virtual): `System::Thread` (two slots, `Run`, `OnCompleted`) and its subclasses
`Engine::Loader`, `Audio::Stream`, `IO::PersistenceWorker`, the exe's `PathfinderThread`; the IO::Encoding
policy tables (unexported, 5 slots); the controller member interface (9 slots at 0x14015f070, slot 0 `Step`) and
the state-machine handler tables; `UI::Font`'s factory-flag slot. The four Engine loops DO have slot 0 =
`DeletingDtor` (`UpdateLoop.h:319`; `Startup` is slot 1) and are ordinary cutover classes.

### 3b. Block 0 — the questions (read-only, then ONE owner writes `B3X_RULES.md`)
1. Per raw slot read above (`partc.py --list`): which class's table it indexes; does that class turn virtual;
   is the read a delete (→ `delete p` / `p->~T()` / a member call), a named member (→ a virtual call), or a
   non-dtor slot 0 (stays, and the class is exempt). The exempt list is derived, never assumed.
2. The dx11 seam: dx11 stamps its own twelve tables, indexed by retail slot, onto `Video::*Data` objects the
   Runtime then dispatches by COMPILER slot. Either the twelve `*Data` classes stay on `DeletingDtor` (exempt,
   carry-over 6 unchanged) or the seam re-indexes — decide before any VID block.
3. `delete p` needs an accessible destructor and the class's `operator delete` (class_operator_delete.cpp,
   ui_operator_delete.cpp route to `MemoryAllocator::MemFree`): list every class whose protected dtor must become
   public or whose `operator delete` is missing; prove no caller passes `flags & 2` (the vector arm — B8-2 said
   none; re-grep).
4. The 1,051 explicit dtor calls: which are unqualified calls on a class that turns virtual (they start to
   dispatch — the `companion_data.cpp:218` reason for qualifying).
5. Order: leaves before bases is NOT enough — a base and all its subclasses must turn virtual in ONE commit (a
   virtual dtor in the base shifts every subclass's slots), so blocks are HIERARCHIES (Object::Base's whole tree
   is one), and the exe tree hangs off Runtime bases. Measure the hierarchy sizes and decide whether the step is
   one mechanical owner with a script (likely) rather than parallel blocks.
6. Tests: the 92 `PmfSlot` checks and 119 `DeletingDtor` test lines are rewritten, not deleted — they are what
   proves the new order.

### 3c. Standing rules (B34B, B35 §11, B36-B38 §8/10/14, B39 D1-D10 / B9-1..16 hold)
- Retail wins; a behaviour change is its own titled commit gated with resave ×5. This step should have NONE: a
  resave or typetable diff is a slot-shift bug, localised before anything else.
- Never "fix" a faithful crash with a null check; never `getenv`; no new flag or instrument; no `Register*()`
  inside a `printf`; generated `.inc` files are regenerated, never hand-edited.

### 3d. The gate for every merge (coordinator only)
`b39_session/gate.sh` unchanged: debug + release + perf build · new-warning set vs a clean-rebuild baseline ·
`make unit` · new game + pause-menu quit · tutorial · Load Game · resave ×5 · typetable == baseline ·
`[RtResolve] 0 misses`, 0 TRAP, 0 GATE WARN, `calls=5683` / `5773 openFileFail=12` · `dispatch_slot_map --check`
0 · `rt_mint_gen --check` UP TO DATE (0/0/2); `uiroutes.sh` and `toolroutes.sh` on every merge that touches UI or
a tool; `devlost.sh` on every merge that touches Video / Graphics / dx11. Plus: `partc.py`'s numbers only fall.

### 3e. Acceptance
0 `DeletingDtor` declarations outside the derived exempt list; 0 `DeletingDtor(0)` / `DeletingDtor(1)` calls;
`Object::Destroy` is `delete` (or gone); every raw slot read of §3a re-derived and either converted or proven to
index an exempt table; the PMF tests pin the new order; the gate passes; `stepclose.sh`.

## 4. After it

1. **The registry teardown**: `rt_export_addrs.inc` (4,926 rows, 1,480 with a reader), `address_registry.cpp`
   (1,243 lines, 694 `RegisterExeAddr` statements, 642 function rows), 127 `standalone/reg_*.cpp`, `engine_funcs`
   (227 externs; 3,036 + 1,652 lines), `ResolveExport(` 205 / `RT_EXPORT_ONCE(` 242 / `ResolveAddr(Module::Exe`
   2,185 / `ResolveAddr(Module::Runtime` 275 code lines, the non-polymorphic doors, the five non-type-id cells,
   the mint's last two storage rows (and with them `rt_data_mint.*`, `rt_mint_gen.py` and `RT_DATA_SLOT`), and
   `System::MemoryAllocator` (carry-over 1).
2. **B.4, the cast metric**: `reinterpret_cast` lines 29,870 (code 20,863, runtime 5,773, standalone 2,213,
   engine 259, dx11 173, tests 589) and 6,036 `// B.3.x bridge` markers.
