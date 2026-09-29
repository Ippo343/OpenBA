# Next session plan (written 2026-09-28, at the close of the registry teardown)

State and counts live in `../STATUS.md` (the B3R block) and `b3r_session/CLOSE_NUMBERS.md`; this file is
only the plan. Replace it when the session it describes has started.

**What the teardown left behind, in one line:** nothing registers a body under a retail name or address
and nothing resolves one — `standalone/address_registry.*`, the 127 `reg_*.cpp`, `resolve*.cpp`,
`rt_port.h`, the mint and `engine/funcs/` are deleted; a call is a C++ call; a thing with no body is a
`// PORT GAP 0x…` comment at its call site. ENDGAME B.2 is finished. What is left of B is the CAST
METRIC (B.4): the bodies are still C-shaped — `void*` receivers, raw offset reads — and that is the
next step.

## 0. Owed from the teardown (each its own titled commit; none blocks the step)

1. **`scene_watch.cpp`'s LOD census matches no object** (before the teardown and after): `IsMeshInstance`
   compares a vtable slot with `&RtGraphics_MeshInstance_AddDrawItems`, an `extern "C"` wrapper; the
   class's table holds the MEMBER. Compare through `runtime/types/object/VSlot.h`, or delete the census.
2. **Cell-test guards that now hide a fault** (`b3r_session/STORAGE_BRIEF.md` decision 4): where old code
   tested only the CELL pointer and then read the OBJECT unguarded (`edittool.cpp`'s KeyMap read;
   pickup's `if (!cell) return 1`; ~25 sites; find them in the commits
   `git log --grep='B3R STORAGE' --oneline` lists), a null object now returns early. Retail has no test there.
   Read retail per site; delete the guard where retail has none (that is restoring behaviour, a
   `behaviour:` commit each).
3. **Bodies with no reference** (kept as provenance; decide per body: make it the member B.4 needs, or
   delete): 13 exe bodies (`b3r_session/data/W2_EXE_bodies_unreferenced.txt`), 39 Runtime C-door bodies
   (GATE_TABLE row 29's hand-in list: `vector.cpp` 14, `transform.cpp` 7, `string.cpp` 4 …), 25 from
   W2-RT (10 IO, 3 Video, 12 companion `*Data` ctors), W2-EZ's list, `WorkAcc_ZoneRefresh_9c480`,
   `TabElement_UpdateContainers_Body`, the losing `RtGraphics_InterpolatedStateTransform_Lerp`,
   `RtLaneXEnsureAllocatorPlatform`. W1 already deleted ~175 delegations and three real transcriptions
   (`NomadComp_RebuildController_b60`, `NomadComponent_OnCreate`, `StorageLocationComponent_OnCreate`):
   history has them (`git log -S`).
4. **`docs/known_port_issues.md`** entries 1-6, 8, 10, 11 are open (the no-op abs mask of `helper_fe40`;
   Orchard's by-address time; growth sub-resources never spawn, 0x14009f5d0; the door / body width table;
   misleading body names; wrong bindings in Dialog; 0x140172248; `ToInteger` / `memmove` not exercised).
   Each is a `behaviour:` commit with its retail address. Entry 3 and 8 change what the player sees.
5. **16 harness files are compile-checked only** (no gate route sets their flag; list with flags in
   GATE_TABLE row 18's hand-in). Either give each a route in `toolroutes.sh` or delete the harness.
6. **Two literal vtable index uses** survive in `code/Game` (`destroyui` / `immigrationui` typedefs;
   `work_leaf_hunt` `vtable+0x8`): by-name calls, as the cutover's rule says.
7. **History comments** across `runtime/` still name `*PortInit`, waves, lanes, the mint and "registers
   under". They describe deleted machinery. Strip them per directory as B.4 touches the file — not in a
   sweep of their own.
8. **Tools that read archived census CSVs** (`tools/rt_data_row_refs.py`, `video_data_refs.py`,
   `rt_data_harvest.py`) have no producer any more; `b3r_session/scripts/` A, C, D, E read deleted files
   (they reproduce only at `eb6f8600`). Delete the three tools; leave the session scripts as a record.
9. Two W2 commits do not build alone (`a093a549`, `0ab4cbb9`: deletions staged a commit early). A bisect
   across them must skip: `git bisect skip`.

## 1. Sync (5 minutes)

- `git log --oneline -8`; `git worktree list` must show main only; `git branch --no-merged main` empty.
- Greps exclude `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`; never `find /`. Count
  CODE lines with `b3r_session/codegrep.py` (comments stripped) — a raw grep counts history comments.
- The kit is `b3r_session/`: `gate.sh`, `mergegate.sh`, `finishmerge.sh`, `uiroutes.sh`, `toolroutes.sh`,
  `devlost.sh`, `stepclose.sh`, `mods_route.sh`, `acceptance.sh`, `codegrep.py`, `reftable_diff.py`.
  Copy the WHOLE directory into the session scratchpad, **marker files included**
  (`RESOLVE_CACHE_GONE`), and cut the warning baseline from a `make clean` rebuild FIRST
  (`gate.sh baseline 3`, then `gate.sh baseline 3 warnbaseline`).
- **Memory:** agents build at **-j3**, at most **~5 agents at once**, the gate at -j4. The Makefile no
  longer overrides a `-j` given on the command line.
- **ONE game-running command at a time.** The game-dir lock is re-entrant for the holder `coord`: two
  coordinator runs at once splice each other's logs.
- **Invariants:** new game `calls=5683 openFileFail=0`; tutorial `calls=5773 openFileFail=12` (Load Game
  floats 4758-4787, recorded only); resave golden md5 `dd3893af7772e3f58a3bb51dd5c29a9f`; typetable 428
  lines md5 `b983f144`; unit **918 tests, 0 failures**; `NOT IMPLEMENTED` lines **0** on every route;
  no `[RtPort]` / `[AddressRegistry]` / `[RtResolve]` line exists. `gate.sh` fails a route whose
  `[ExtLoad]` line is MISSING.
- Ghidra: `check_connection` first; pass `program=` on every call, never `switch_program`.
- **Worktree agents** are cut from an OLD commit: every brief starts with
  `git checkout -b <branch> main` and the expected hash. Agents stage a file deletion in the commit
  that removes its last user (every commit builds).

## 2. Carry-overs (each its own gated change)

1. `System::MemoryAllocator` — **closed** (a real class, `_singleton` a class static, one API; the
   REDZONE / ALLOC_STATS seat is `System::g_memoryAllocatorInstrument`).
2. `System::Configuration::CurrentBuildId` free door — **closed** (no doors).
3. `ExternalPtr` exe door sites — **closed** as doors; their `void*` shape is B.4's.
4. `scene_watch.cpp` 0x180100f20 — **closed** as a row; see §0.1.
5. **Device-lost / loader race — DEFERRED by the user (2026-09-27) until ALL endgame work is finished.
   Do not fix, guard or investigate it; `devlost.sh` stays a crash check on Video / Graphics / dx11
   merges.** (The run is killed at its 600 s limit with the round trip COMPLETE.)
6. **dx11 `*Data` seam** (84 slots) — **open**: §4.1. The twelve `Video::*Data` classes are the last
   `DeletingDtor`.
7. Quit-during-splash; Mods-route timing — **open**, not re-measured.
8. UI leftovers — **partly open** (four `RtUI::Element base;` PODs; the Options-change route has no menu
   token).
9. `Math::Vector3::operator==` 0x140152a70 — a PORT GAP arm (`same = 1`) in
   `citizen_assignworkplace.cpp`: the operator EXISTS in `runtime/`; wiring it is a `behaviour:` commit.
   The same holds for the other six unfilled-slot arms (`Box` ctor, `DeletePtr`, `ExternalList::Find`,
   `Factory::CreateObject`, `Particle::_rng`, `Vector3::Lerp`) and `Entity::FindAudioComponent`
   0x140002280 (town sounds never play).
10. PORT GAP / STANDALONE DEVIATION — **open**: 189 PORT GAP lines, 56 deviations (W1 made the null arms
    explicit: the count rose, the behaviour did not change).
11. Lazy `X_ResolveFuncs` — **closed** (0). 99 `Ensure*` latches remain: type-id caches and resource
    loads, not doors.
12. (closed earlier)
13. `GameUpdateLoop::_singleton + 0x…` raw reads (≈880 lines, all of `code/Game`) — **open**: the largest
    single family of B.4. The object is a real class static now; its MEMBERS are not named.
14. PORT GAP message targets (`CitizenAIComponent::OnMessage` 0x14010f340,
    `HappinessComponent::OnMessage` 0x14004a780) — **open**.
15. `AutoPickupComponent`'s Startup (retail slot [4] 0x140044990) — **open**.
16. `EventNotifyDialog` / `AutoSaveDialog` standalone `dialogRes != nullptr` skips — **open**.
17. The `InitFields` / `ReleaseFields` link seams for the unit tests — **open**.
18. The four `DEFINE_EXE_VTABLE` lines — **open**.
19. Protected destructors of `Graphics::Light`, `Object::ExtensionCompiler`, `Object::ExternalCompiler`;
    undefined in-class `operator new` declarations — **open**.
20. **From the teardown:** `g_gameSettings` (`int32_t*`, the 0x34-byte block of 0x1401c3de0) and
    `g_gameContainer` (`void*`, 0x1401c3de8, "the Game container, NOT the KeyMap") are namespace-scope
    pointers because retail's types for them are not C++ classes in the tree. B.4 gives each a struct.

## 3. The step: B.4, the cast metric

**Goal.** The bodies read like C++: a function that retail compiled as a member IS a member of its
class, its receiver is `this`, a field is read by NAME. `reinterpret_cast` and the `// B.3.x bridge`
markers fall to what is genuinely a reinterpretation (serialized bytes, the dx11 seam, allocator
headers). This is ENDGAME B.4; the phase plan's acceptance number is in `../ENDGAME.md`.

### 3a. Measured at the teardown's close (code lines, `codegrep.py`)

| what | count |
|---|---:|
| `reinterpret_cast`, all | 26,488 (29,064 at the cutover's close) |
| … `code/Game` | 19,542: Component 7,790 · Game 2,630 · Dialog 2,530 · Work 2,071 · Action 2,013 · Player 1,321 · Tutorial 535 · Goal 355 · Entity 288 |
| … `runtime/src` + `runtime/types` | 4,772: ui 1,368 · graphics 1,160 · collection 455 · video 402 · io 324 |
| … `standalone` / `dx11` / `engine` / `tests` | 1,509 / 134 / 5 / 526 |
| casts of the shape `reinterpret_cast<T>(p + 0x…)` in `code/Game` | 7,097 |
| files in `code/Game` with 50 casts or more | 141 files hold 15,890 of 22,169 raw occurrences |
| `// B.3.x bridge` markers (raw lines) | 6,859 |
| `void*` first parameters in `code/Game` | ~2,970 lines |
| `code/Game/<Module>/exe_bodies.h` declarations (the C bodies, with their retail address) | Action 145 · Work 83 · Component 172 · Game 67 · Player 51 · Dialog 39 · Tutorial 20 |

Expect these to undercount or miscount until block 0 has read the code (the teardown's §3a was wrong
on five lines).

### 3b. Block 0 — the questions (read-only, parallel; then ONE owner writes the rule sheet)
1. **Which class does each C body belong to?** For every declaration in the seven `exe_bodies.h` and
   every `Rt<Ns>_<Class>_<Member>` body in `runtime/src`: the class (from retail's `this` use, the
   vtable that holds it, the mangled name where one exists), whether the class is already a real C++
   class with a header, and whether the member is already DECLARED there (the audit found declared but
   undefined members: `~Scene`, `~Renderer`, `SaveLoad()`, `Audio::Scene()`; an inventory's "member
   exists" column must be checked against a DEFINITION).
2. **Which offsets have a name?** Per class: the offsets the bodies read (`p + 0x…`), against the
   fields its type header declares. Three sets: named and asserted (convert), read but not declared
   (the header grows a field, with `static_assert`, from retail), declared under a guess name
   (`field_0x48`: keep the read typed, name later).
3. **The big objects.** `GameUpdateLoop` (≈880 reads through `_singleton`), `CitizenComponent` (654
   casts in one file), the settings block, the Game container: what is the full layout, and who owns
   the header?
4. **What is a legitimate cast?** Serialized byte streams, allocator block headers, the dx11 seam,
   `Collection::Array<T>` storage, hashing, vtable diagnostics through `VSlot.h`. These stay and are
   the denominator's floor: list them by file so the acceptance can name them.
5. **Order and conflict.** A header is shared by every body of the class: blocks own CLASSES' headers
   plus the files that define their members, not directories. Which classes are read from many
   modules (their header must be finished FIRST, alone)?
6. **The tests.** 526 casts in `tests/unit`, many of them deliberate layout probes: which are the
   test's subject and stay.

### 3c. Shape
Read-only inventories (parallel) → ONE rule sheet + class/file map, committed → **W0: headers first**
(every field a body reads is declared and asserted; bodies untouched; the compiler proves nothing
moved: `static_assert(offsetof)` and `sizeof`) → **W1: bodies by class** on worktrees with disjoint
file sets (a body becomes a member; an offset read becomes a field read; callers change from
`Body(p, …)` to `p->Member(…)`) → the coordinator merges and gates each → an independent AUDIT
before the C declarations in `exe_bodies.h` are deleted → close.
Lessons of the teardown that bind here:
- **Make the dangerous change checkable first.** A field read is checkable: W0 adds
  `static_assert(offsetof(Class, field) == 0x…)` for every offset a body uses, BEFORE any body
  changes. W1 is then a rename the compiler checks.
- **Read the whole body before deleting or moving a function.** Register functions carried `atexit`
  hooks and seam installs; C bodies carry counters, latches and prints.
- **A premise is checked at the base commit**, not assumed (the false `PORT GAP 0x180006a10` arm).
- **An independent read-only audit before the irreversible wave** found the one real defect of this
  step. Keep it.
- **No-behaviour-change is provable per object file**: compare the debug objects function by function
  with main's (W2-EZ did: 1,078 of 1,086 identical). A W1 block hands in that comparison.

### 3d. Standing rules
- Retail wins; a behaviour change is its own commit whose title STARTS with `behaviour:`, gated with
  resave x5. Widths and signedness are part of behaviour: a field read keeps the width the offset
  read had unless retail says otherwise (then it is a `behaviour:` commit; `known_port_issues` 5).
- Never "fix" a faithful crash with a null check; never `getenv`; no new flag or instrument.
- Nothing reads a vtable by literal index. Deletion is `delete p`. By-value virtuals are declared
  `T* M(T* ret, …)` (GCC ms_abi passes the return slot before `this`).
- When retail leaves a register unset, the site is LEFT and listed (B3R_RULES §8.7).
- Type headers: `.claude/rules/type-headers.md`. A new field is `static_assert`ed.
- No C-style umbrella header; a caller includes the class's header.

### 3e. The gate for every merge (coordinator only)
`b3r_session/gate.sh`; `uiroutes.sh` on Dialog / UI merges; `toolroutes.sh` on Player / Action merges;
`devlost.sh` on Video / Graphics / dx11 merges; `acceptance.sh` stays at target (a door spelling that
reappears is a regression).

### 3f. Acceptance
Every C body is a member of its class or is listed by name with its reason; `exe_bodies.h` headers
hold only what has no class; every offset a body reads has a declared, asserted field;
`reinterpret_cast` in PROD is at the floor block 0 listed (by file); bridge markers 0 outside that
floor; the gate passes; `stepclose.sh`.

## 4. After it

1. **The dx11 seam** (carry-over 6): dx11's companion types become subclasses of `Video::*Data`; the
   twelve classes flip to `virtual ~T()`; `DeletingDtor` and `Dx9Placeholder` leave the tree.
2. **ENDGAME B.1 decision 7**: exceptions (~25 files on `setjmp` / `longjmp` / SEH).
3. **The PORT GAPs** (carry-overs 9, 14, 15 and the 189 marker lines): port what has no body, wire what
   has one; each a `behaviour:` commit.
4. Then, and only then, the deferred device-lost work.
