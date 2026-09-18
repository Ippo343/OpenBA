# Next session plan (written 2026-09-18, at the B.3.2 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list`, `git branch --no-merged main` (expect none).
- ~19 merged agent worktrees sit under `.claude/worktrees/` (two are `locked`). They are
  all merged; prune them (`git worktree remove`, `unlock` first for the locked two) —
  they make every unscoped grep 20x slower and have produced false counts.
- Any grep over `standalone/` or `tests/` must exclude `standalone/build*/` and
  `tests/unit/build/`: object files match as text and inflate counts ~40x.

## 1. Carried over from B.3.2 (small, do first, each is its own gated change)

1. **The `0x1400021d0` tail question.** ~20 step-2 sites (pathfinder, gatherrawmaterials,
   cemetery, autopickup, resourceproduce, consumeproduce, fishingwork, …) cite exe
   `0x1400021d0` — whose retail body leaves a grown tail UNINITIALISED — but use the zeroing
   `GameColl::I32`. The faithful change is `I32NoInit`. It turns zeroes into allocator
   garbage across the sim, so: one agent (opus), per site prove the tail is written before
   it is read (or find the read), flip, then the coordinator runs the resave gate AND
   `make sa-stress` (perf exe, 6000 frames) — not one boot. Documented in
   `code/Game/Component/coll_bridge.h`.
2. **Mods-route coverage.** `Set<System::Filename>::Insert` (width-8 `AvlOps`,
   `runtime/src/io/modification_refresh.cpp`) has run ZERO live iterations: this box has no
   `.pkm` in `WinData/`. Move the two mods out of `WinData/_mods_off/`, run the Mods route
   (`BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6
   BANISHED_MODAL_ANSWER=1`), move them back (the achievements harness needs them out).
3. ~~`make sa-stress` on the closed tree~~ DONE at the close: +3.1% vs baseline, the
   allocator door's per-call slot re-read is not visible. Nothing to do.
4. **`BANISHED_AUTOTUTORIAL=1` alone never self-quits** (ran healthy to frame 85k and had
   to be killed). Always pair it with `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`
   in agent prompts — the 2d gate line below already does.
5. The rest of STATUS.md's "B.3.2 CLOSED → Left, and why" is by design and needs no action:
   241 collection rows stay (81 by name, 160 by address), 36 collection `fn_*` pointers
   stay until their callers' own §B.2 conversion.

**Sweep rules learned at this close (put them in every delete-pass prompt):** a hit in
`docs/` or a `// 0x18…` comment or a printf/static_assert string is NOT a caller;
"by address" includes the per-file wrappers (`RtAddr<Fn>`, `RtAddrFn`, `ArrayDtor(addr,…)`,
`ArraySetSize`, `SetSizeWithFill`, `PlainArrayDtor`, `ByAddr`, named `constexpr`), not just
`ResolveAddr`; an exclusion like `\build` also swallows `buildinggoal.cpp` — anchor
exclusions on a path separator; a dead row that was live is an EXEC-of-0 at runtime, never
a link error, so the routes are the proof.

## 2. B.3.3 — `Object::Base`, `TypeInfo`, `Factory`, reflection macro

Input: `OBJECT_INVENTORY.md` (this directory; measured 2026-09-18, sonnet, NOT yet
spot-checked — verify the numbers a decision rests on before using them).
ENDGAME §B.3 step 3: **one owner, serial, real design work — not a batch.**

### 2a. Already done / out of scope
- `ExternalPtr<T>`, `InternalBasePtr`/`InternalPtr<T>` are real classes since B.3.1.
- `IO::Serializer` is B.3 step 4. Object's `Serialize` bodies keep going through
  `RtObject::SerializerLite` and the RETAIL Encoding vtable offsets (they differ in the
  profile build — never cross-check a Serialize body against the profile disassembly).
- The `ExternalFactory::Add` loader race is its own investigation. The class conversion
  must not add locking that changes timing, and must not "fix" it by accident.

### 2b. Questions the owner answers first (write them into `B33_RULES.md` before any code)
1. **Base slot 1**: name it from the profile DLL / export table (Ghidra, profile program),
   or commit to `Slot1()`. Every subclass inherits the answer.
2. **One TypeInfo record.** `RtObject::BaseTypeInfo` (runtime/types/object/TypeInfo.h) and
   `BaseTypeInfoStorage` (code/Game/Object/factory_registration.h) describe the same 0x38
   bytes with no assert against each other. `DECLARE_TYPE/DEFINE_TYPE` (decision 4) must
   subsume both; decide how the exe-side staging list folds in now that there is no DLL
   ctor-order problem.
3. **`Factory::root`**: is `type_system.cpp` still chasing raw `TypeNode*`? If so, moving it
   onto `Collection::Set<Pair<u32,TypeRecord>>` is B.3.3 scope. Check before planning.
4. **Coexistence proof for decision 2.** Before converting anything real: a unit test with a
   GCC class (`virtual Base* DeletingDtor(unsigned)`, slots in retail order, no virtual
   dtor) whose vtable is compared slot-for-slot with a hand-built array, plus one
   two-base class asserting the second vptr at +0x28. This is the whole premise of
   per-class migration; prove it in `tests/unit/` first.
5. **Vtable identity compares** (`audio_stream_win.cpp` ThreadBaseVtable; `goal.cpp`
   Base→External→Goal stamping): list every site that reads a vptr for identity; a
   compiler-generated vtable has a different address than the minted one, so each site needs
   a decision (type-id check instead) BEFORE its class flips.
6. **`deleting_dtor.cpp` (135 rows)**: state the rule — a class's row dies in the same
   commit that gives the class a real `DeletingDtor`. Row indices are table positions;
   re-count on every merge.

### 2c. Order of work
1. Inventory spot-check + the six answers → `B33_RULES.md` (owner, opus; Ghidra open).
2. The coexistence unit test (2b.4).
3. `Object::TypeInfo` + the macro, with BOTH old structs kept as layout-asserted aliases;
   `Factory` onto it; `X::TypeId()` available. Gate.
4. `Object::Base` as a real class; `Destroy(p)` helper; Base's own `RT_DELDTOR` row retired.
   Gate (all routes + resave — every object in the game derives from this).
5. Then, and only then, fan out (opus, worktrees, disjoint files, the B31 hygiene block):
   - `External` / `ExternalFactory` / `ExternalList` (`external*.cpp`, ~5.9k lines)
   - `Internal` / `InternalFactory` (`internal*.cpp`, ~1.5k lines)
6. Caller batches by `code/Game/<Module>` directory: 654 `GetTypeIdByName("X")` →
   `X::TypeId()`, ~194 `DeletePtr`/slot-0 calls → `Destroy`, the ~106 `fn_Object*`
   pointers. Sonnet is enough for the `GetTypeIdByName` sweep once the pattern is set;
   opus for the delete paths.
7. One closing agent: pointers/shims/rows at zero callers by name AND address, regenerate
   the `.inc` files, STATUS + B.4 table.

### 2d. The gate for every B.3.3 merge (coordinator only)
build exit 0 (debug AND perf — deletions near init printfs break only the perf exe) ·
`make unit` · `sa-run` + pause-menu quit · `BANISHED_AUTOTUTORIAL=1` (+ the two PAUSEMENU
vars, or it never exits) · Load Game dialog route · `tools/resave_gate/resave_gate.sh Gate`
PASS (a ONE-byte DIFF at +0x18 is the README's known flake: re-run twice before reading it)
· `dispatch_slot_map.py --check` 0 · `git diff --stat` against the agent's file set before
merging · `make sa-stress` at each step close.

## 3. How the 2026-09-18 session ran (what to repeat, what to change)

- Two opus worktree agents on disjoint file sets + one sonnet read-only inventory, then one
  opus final agent. Coordinator merged and gated each. Worked; repeat.
- Sub-workers spawned by an agent report to the coordinator too. Harmless, but the
  coordinator should not act on them — the owning agent integrates.
- One agent burned turns polling `git diff --stat` while waiting on sub-workers. Prompt
  text for next time: "block on sub-workers; do not poll".
- Ghidra was not open at launch. Open it BEFORE launching any agent that needs it.
