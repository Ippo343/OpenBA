# SMMEMO -- the exact no-op skip of the state machine's slot-8 tick

Block SMMEMO, 2026-10-12. Candidate #2 of `IDEAS_20261012.md` (§2.2), built as faithful perf
work in the style of WALKCACHE / STILLPATH: no registry entry, no settings key, no `BANISHED_*`
flag, no `Opt()` field, no `getenv`, no door. `static constexpr bool SmMemo::kEnabled = true`
(`src/game/Component/sm_memo.h`) is the kill switch. **The game was not run by this block**; the
coordinator runs the COMPPROF=4 shadow, the COMPPROF=2 row and the A/B.

**Short version.** A building's state machine with nothing pending spends its slot-8 tick
walking its controller chain to null trans30 handlers and writing nothing. The memo is the
machine's own four state bytes, recorded (inverted) in the StateMachine's tail padding at the end
of a full slot-8 tick that called no trans30 handler. When the live bytes equal the memo, the
tick returns. The check is one dword compare on the line the full tick reads first. The memo is
keyed on every byte the walk reads from the machine, so any write to them (a request, a
transition, a load) makes it decline. The rest of what the walk depends on (the chain, the node
classes, the state tables) is fixed for the machine's life once it is in the update tree (§1.3).
**Design choice versus the ideas file:** the memo lives *inside the object*, not in a vector
parallel to the walk snapshot (§2).

## 1. The proof

### 1.1 What the slot-8 tick does

`StateMachineComponent::Tick` (slot 8, 0x14012b640) = `StateMachine::Tick(t, useAlt 0, run 1)`
(0x1400a9660, `statemachinecomponent.cpp`). With `pendingState < 0` it jumps to `LAB_transition`:

```
if (run && currentInput >= 0) {
    i = activeState; node = current;
    while ((i < 0 || node->Tick(this, t, i) == 0) && (node = node->parent) != nullptr)
        i = node->FindState(currentInput);
}
} while (pendingState >= 0);
```

- **Node slot 6 (`Tick`)** has exactly two overriders in the tree: `ControllerLive::TableController
  <Tag>::Tick` -> `Dispatcher<Tag>::Trans30` (`controller_state_table.h`), and
  `StateMachineComponent::RootController::Tick`, which returns 0. `Controller` has no other
  subclass (grep of `public ControllerComponent::Controller` / `::ControllerComponent::Controller`).
- **`Trans30`**: `if (idx < 0 || idx >= table.count) return 0; h = entries[idx].trans30Handler;
  if (!h) return 0; return h(...)`. Without a handler it reads the static table and writes nothing.
- **Who registers a trans30 handler.** All 23 controller tables are filled by
  `TableRegisterState(t, id, enter, trans30, trans38, update)` in their populate functions.
  The third argument is non-null only in `boatcomponent_states.cpp` (states 0, 1, 3, 4:
  `BoatState0Tick`, `1Tick`, `3Tick`, `4Tick`). Every other table passes `nullptr` there,
  checked call by call: AbsorbResource, Attendance, Build, Cemetery, ClearArea, ClearResource,
  CropField, Destroy (its `State4Tick` / `5Tick` / `6Tick` are trans38), Happiness, Healer,
  Immigration, MarketLocation, MultiObject, Nomad, Orchard, Pasture, Residence, ResourceProduce,
  StorageLocation, Trade (`retail: 0` comments), Well, WorkPlace. `trans30Handler` is written only
  by `TableRegisterState` (grep).
- **`FindState`** = `TableFindIndex` over the static table, or the root's `id == 0 ? 0 : -1`.
  It is pure.

So a slot-8 tick with `pendingState < 0` either (a) calls at least one non-null trans30 handler,
or (b) visits the nodes `current, current->parent, ...` (stopping at the root's null parent),
computes their indices with `FindState(currentInput)`, writes nothing, and returns. Nothing in
(b) can raise `pendingState`, so the outer `while` exits. **Which of (a) / (b) happens, and the
exact sequence of reads in (b), is a function of: `current`, `activeState`, `currentInput`, the
parent links from `current` up, each node's class (its vptr), and the static tables.**

### 1.2 The memo

`StateMachine::tickMemo`: the 4 bytes at StateMachine +0x0C (component +0x54..+0x57). These are
retail's tail padding. Retail's ctor never writes them, `Persist` never serializes them, and no
code reads them (grep of `_pad0C` before the rename: only the static_assert). The port's ctor
value-initialises them to 0 (`stateMachine{..., {}}`).

- **The state word** `W = StateWord(currentInput, activeState, pendingState, transitionState)`: the
  four signed bytes at +0x08..+0x0B as one little-endian dword.
- **Record** (end of every full slot-8 tick, `SmMemo::Record`): `tickMemo = ~W_after` if
  `g_trans30Calls` did not change during the tick and `W_after`'s pending byte is negative, else
  `kNone` (0). `g_trans30Calls` is incremented in `Dispatcher::Trans30`'s handler branch only.
- **Hit** (start of every slot-8 tick, `SmMemo::Hit`): `~W_live == tickMemo` -> `++g_skipped;
  return`.
- **Why inverted.** A new or cleared memo is 0. Stored plainly, 0 would match the real word
  (0, 0, 0, 0): input 0, active 0, **pending 0**, transition 0. That is a pending transition, and
  skipping it would be wrong. The unit test caught this. Inverted, 0 matches only
  `W = 0xffffffff`: everything -1, where the full tick is a no-op anyway (nothing pending, so no
  transition phase; input < 0, so no run phase).

**Claim.** On a hit, the full tick would call no handler and write nothing.

*Proof.* A hit means `~W_live == tickMemo`. Either the memo is kNone and W_live = 0xffffffff
(the no-op word, above), or the memo was stored by Record at the end of a full slot-8 tick T0
of **this object** with `W_T0 = W_live`. Record requires a negative pending
byte, so `pendingState < 0` now: the full tick would run the run phase only. At T0's end the
state was also `pendingState < 0` with the same bytes, and T0's run phase called no trans30
handler. T0's run phase was the last thing T0 did, and it ran with T0's final `current`, the same
`activeState` / `currentInput` (the run phase does not write them), the chain and the tables.
By §1.1, the run phase now repeats exactly T0's walk, case (b), **provided `current`, the chain,
the node classes and the tables are the same as at T0**. §1.3 shows they are. ∎

### 1.3 Every writer of every input

| input | writers | how the memo sees it |
|---|---|---|
| `currentInput`, `activeState`, `pendingState`, `transitionState` (+0x08..+0x0B) | `StateMachine::Tick` transition phase; the inlined copy `SM_RunPhase` (`keystone_a_removalcalcpath.cpp:206`, DestroyComponent::RunStateMachine); `ControllerLive::RequestStateTransition` (`controller_state_table.h`, pending + transition); `firecomponent_smstate.cpp:85-86`; `orchardcomponent_states.cpp:146-147`; `Startup` (pending = the first controller's default); `starttown.cpp:1651` / `:1718` (pending = 2); `finish_builds_harness.cpp:120` (diag, pending = 8); `Persist` (load, a new object); the ctor | **in the key**: read live on every tick, so any change fails the compare. A request between two slot-8 ticks sets `pendingState >= 0`, and the next slot-8 tick runs the transition, as in retail. Nothing to enumerate for exactness; the list is for the record. |
| `current` (+0x00) | the ctor (= the root, with input -1); the transition phase of `StateMachine::Tick`; `SM_RunPhase`; `RelinkAndResume` (Slot11) | **not in the key**. Every writer leaves `current = G(chain, currentInput)` whenever `currentInput >= 0`, where G = "go to the chain's tail, then climb while `FindState(currentInput) < 0`". The transition phase and `SM_RunPhase` do exactly this (tail via `+0x10`, `FindState(pending)` then `FindState(currentInput)` on the climb, with `currentInput = pending` just stored). `RelinkAndResume` does the same climb. The ctor's root goes with input -1, where no run phase happens. With the chain fixed (next row), `current` is therefore a function of `currentInput`, which is in the key. No other writer exists (grep of `current =` / `->current =` / `.current =` over src: the other hits are `ActionStack::current`, an unrelated int). |
| the chain: node `+0x08` parent, `+0x10` child | `StateMachineComponent::Startup` and `RelinkAndResume`, nothing else (grep of `_data08 =` / `_data10 =`) | **both clear the memo** (`tickMemo = kNone`, NOT RETAIL, the only lines added to faithful bodies). Both also run before the entity is in the update tree: Startup from `StartupComponents` at create, before `RegisterEntity`; Slot11 from the post-load pass `EntityList_PostLoad_c4730`, before the entity is enrolled in `stagingUpdate`. So no slot-8 tick ever sees a chain change between a record and a hit. The clears are a belt for the day that changes. |
| the node objects and their classes | members of the entity's components (`TableController<Tag>` at each leaf's retail offset, `RootController` at SM+0x28). Constructed with the component, never re-stamped. Components never leave an entity in the tree (WALKCACHE §2, "The `_components` array"). | fixed for the object's life |
| the state tables | `Dispatcher<Tag>::table`, filled once by the 23 `*_PopulateTable` / `*_PopulateStateTable` functions, each called from a namespace-scope `static const bool s_... = (Populate(), true)` initializer (verified for all 23), i.e. during static init, before `main`. The root's table is a constant. | static after static init |
| `g_trans30Calls` | `Dispatcher<Tag>::Trans30`, the handler branch (`if constexpr (SmMemo::kEnabled)`) | the only path to a handler. A handler reached through `SM_RunPhase` or another machine's tick during our tick also bumps it, which can only make us decline. |

**The memo is per object**, so there is no identity problem. A freed and reallocated machine is
a new object with `tickMemo = 0`. There is no index to keep in step with a snapshot, and no
pointer key that could alias (§2).

### 1.4 What stays exactly as before

- The bucket `Update` (slot 9, `Tick(t, 1, 1)`, the trans38 column), `OnAdded`, the nested ticks of
  `RequestStateTransition`, `SM_RunPhase`: untouched, never skipped.
- Boat: in states 0, 1, 3 and 4 its slot-8 tick calls a handler, so it never records and never
  skips. In state 2 (no trans30 handler) it is skipped like any other machine.
- Retail's crash behaviour: the hit dereferences nothing at all. The full tick, on a miss, is the
  unchanged body. No guard was added.

## 2. Why not the ideas file's side vector

§2.2 sketched a memo in a vector parallel to the walk snapshot (like `s_tickList`), keyed
`{sm, current, activeState, currentInput}` and indexed by `g_walkIndex`. I built it in the object
instead, for four reasons:

1. **Zero extra lines.** The full tick's first load is `pendingState`, on the line that holds
   +0x48..+0x57 (the S2 prefetch's `c + 0x40`). The memo sits in the same 16 bytes. A side
   vector costs a line per four machines, and a second stream in the walk.
2. **No identity hazard.** A pointer key in a side table survives the object. A key checked
   against a reused address, or against `g_walkIndex` while another entity's tick runs a
   nested slot-8 call, needs care. A field of the object dies with it.
3. **No coupling.** The side vector would tie `statemachinecomponent.cpp` to the snapshot, to
   `PerfX::g_walkIndex` (src/ext) and to the snapshot's rebuild. The tree-walk fallback would
   also need its own rule.
4. **`current` is not needed in the key** (§1.3: it is G(chain, currentInput)). Without it the
   key fits in the padding.

Cost of this choice: a retail padding field gets a port meaning. It is invisible to saves, the
ABI asserts still hold (`offsetof(StateMachine, tickMemo) == 0x0C`, `sizeof == 0x10`), and
`tools/abi_audit.py` passes (a `uint32_t`).

## 3. Decline rules (the full tick runs)

- any byte of the state word differs from the memo: a pending request, a transition run by the
  bucket `Update` or an event since the last slot-8 tick, a different transition byte
- the last full slot-8 tick called a trans30 handler (Boat in states 0, 1, 3, 4), or ended with a
  pending state (impossible: `Tick` returns only with `pendingState < 0`; the test is a belt)
- the first slot-8 tick after construction, `Startup` or `RelinkAndResume` (memo 0)

## 4. The shadow `SmTickNoop` (BANISHED_STRESS_COMPPROF=4)

On a hit with `g_verifyOn` set, the tick does not return. It keeps the claim (the state word, the
trans30 count, `current`), runs the **full tick on the live object**, and reports
`StressHarness_Verify("SmTickNoop", diff == 0, diff)`. Detail bit 0 = a trans30 handler was
called, bit 1 = the state word changed, bit 2 = `current` moved. It then re-records from the
full tick's result, which equals the memo when the claim held. The sim under the shadow is the
faithful sim. Off, the shadow costs one load and branch of `g_verifyOn`, only on a hit (the
STILLPATH precedent).

What it does not compare: the nodes' other fields and the handlers' targets. By §1.1, the walk
can write nothing except through a handler, and a handler call is bit 0.

Read: `[VERIFY] SmTickNoop checks=N mismatches=M` in the [COMPPROF] report / standalone_error.log.
**Expected M = 0.** N is the number of skipped ticks the run would have taken. Run:
`BANISHED_STRESS_COMPPROF=4 tools/build.sh stress` (heavy town, as STILLPATH).

## 5. The counter: the `[COMPPROF] /sm` row (BANISHED_STRESS_COMPPROF=2)

`SmMemo::g_skipped` is incremented once per skipped tick (one global RMW). `CharProf_Begin`
returns token 3 for a StateMachineComponent slot-8 tick and notes `g_skipped`. `CharProf_End`
charges the tick to `SmMemo::ProfWindow` as skipped or full. The row is printed after the /char
rows:

```
[COMPPROF]   /sm slot-8 memo (SMMEMO) skipped/frame=  N of  M StateMachine slot-8 ticks (a%); skipped X ms/frame Y ns/call; full F/frame Z ms/frame W ns/call; est. saved S ms/frame = P% of EntityList (EntityList E ms/frame)
```

How to read it:
- `a%` is the hit rate. Expect > 95%: buildings sit in one state for thousands of ticks, and only
  Boat's handler states, transitions and the first tick after one run in full.
- `Y ns/call` is what a skipped tick still costs under the profiler: the walk's dispatch, the
  compare and the counter, plus the profiler's own Begin/End. `W ns/call` is the full ticks'
  mean. That population is the transitions plus Boat's handler ticks, so it is heavier than a
  plain no-op walk.
- `est. saved` = skips x (W - Y). **Indicative only, an overstatement**, for the reason above.
  The `/tree` row `StateMachineComponent` (ms/frame, ns/call) before vs after is the better
  profiled figure: ELPROF measured 0.137 ms/frame, 6,180 calls/frame, 22 ns/call.
- **The authoritative number is the unprofiled A/B**: `kEnabled` false vs true, interleaved pairs,
  heavy and compact, CLEAN machine, treeWalk / EntityList ms/frame (the PREFETCH / STILLPATH
  protocol).

## 6. Prediction

Removed per skipped tick: the call into `StateMachine::Tick`, and the walk over 2-3 controller
nodes, each a virtual `Tick` plus a virtual `FindState` on the parent, each node inside another
component of the entity, so typically one miss per node (ELPROF: 22 ns/call profiled;
`StateMachine::Tick` 4.6% self / 6.1% incl of the tree phase). Kept: the walk's dispatch to slot
8 and one compare on an already-resident line (~2-3 ns).

- Hit rate ~97% of ~620 machines x 10 iterations.
- Unprofiled saving ~0.08-0.11 ms/frame in the heavy town, from 6.1% x 2.05 ms tree phase x ~0.9
  of it removed.
- **Predicted: -3..-4% of EntityList** (heavy, speed 10). That is the upper half of the ideas
  file's 2-4%, because the in-object memo adds no line.
- If rank 1's state-machine prefetch arm (`IDEAS` §2.1) lands first, it hides part of the same
  node misses, and SMMEMO's own share drops to ~2-3%. Still, skipping the work beats prefetching
  for it. With SMMEMO on, rank 1's SM arm should be re-measured; it may become a net loss (it
  would prefetch nodes nobody reads).
- The compact town has a higher share of buildings per citizen, so its percentage should be
  similar or higher.

## 7. Files

| file | change |
|---|---|
| `src/game/Component/sm_memo.h` (new) | `kEnabled`, `g_trans30Calls`, `g_skipped` (C++17 inline variables), `StateWord`, `PendingOf`, `kNone`, `Record`, `Hit`, `ShadowDiff`, `ProfWindow`. Header-only, no game types. |
| `src/game/Component/StateMachineComponent.h` | `_pad0C[4]` -> `uint32_t tickMemo` (same offset, comment, static_assert renamed) |
| `src/game/Component/statemachinecomponent.cpp` | `StateMachineComponent::Tick`: the hit test inline, the full tick + record (`SmMemoFullTick`) and the shadow (`SmMemoShadowTick`) out of line. `Startup` and `RelinkAndResume`: `tickMemo = kNone` first. Includes `sm_memo.h`, `stress_harness.h`. `StateMachine::Tick` itself is unchanged. |
| `src/game/Component/controller_state_table.h` | `Dispatcher::Trans30`: `++SmMemo::g_trans30Calls` in the handler branch (Boat only) |
| `src/diag/stress_harness.cpp` | `s_smWin` / `s_tidSM` / `s_smSkip0`, token 3 in `CharProf_Begin` / `End`, `SmMemoReport` (the /sm row), reset per window |
| `tests/unit/sm_memo_test.cpp` (new) | 6 tests (below) |

Unit tests (`tools/build.sh unit perf`): see the results line in §9.
- `sm_memo_state_word`
- `sm_memo_record_and_hit`: the inverted store; kNone against every pending word, the all-zero
  one included; kNone matches only the all -1 no-op word
- `sm_memo_shadow_diff`
- `sm_memo_prof_window`
- **`sm_memo_two_worlds_bit_identical`**: a literal copy of `StateMachine::Tick` over plain
  structs. A root owning state 0, table nodes dispatching like `Dispatcher` (bounds test, null ->
  0, the count in the handler branch), chains linked like `Startup` (first default pending, the
  OnAdded tick) and relinked like `RelinkAndResume`. Five chain shapes, modelled on ClearArea /
  Build / WorkPlace / Residence / Destroy and on Boat (trans30 handlers in states 0, 1, 3, 4). 60
  machines x 3,000 steps. Each step, per machine: an outside request, maybe; a bucket Update
  (trans38 column), maybe; a save / load round trip (current = root, Relink), maybe; a direct
  pending stamp (the start-town idiom), maybe; then 1-10 slot-8 ticks. Every handler logs, returns
  0 or 1, and requests a transition ~16% of the time (the `RequestStateTransition` idiom, nested
  tick included). The machines (current node, four bytes) and the handler logs must be identical
  after every step. Result: identical; 976,629 skipped and 14,463 full slot-8 ticks; 9,978
  trans30 handler calls.
- **`sm_memo_broken_variants_diverge`**: the same world with a memo that ignores the handler count,
  and with one whose key drops the pending byte. Both must diverge, and they do: the test can tell.

## 8. Doubts

- **The padding field.** The port now writes 4 bytes retail leaves as garbage. Nothing reads them
  (no save, no ABI consumer, no diff tool that dumps raw component bytes was found). If an
  object-comparison tool ever compares raw `StateMachineComponent` bytes across builds, it
  will see the memo; it differs between `kEnabled` on and off, never between two runs of one build.
- **`current` out of the key** rests on §1.3's G invariant. If a future port adds a writer of
  `current` that does not climb from the tail (e.g. a hand-written "jump to node"), the memo could
  pass with the wrong `current`. The shadow's bit 2 would catch it. A comment at the field says
  where the memo lives.
- **A second StateMachineComponent on one entity** would link the same controller nodes under two
  roots (retail's own behaviour). Both link at create, before any slot-8 tick, so the chain is
  still fixed by the time a memo is recorded. No shipped template has two.
- **`g_trans30Calls` false declines.** A Boat handler reached during our tick through another
  machine (an Enter handler calling `DestroyComponent::RunStateMachine` on another entity, for
  example) makes us decline once. Safe, and rare.
- **Profiler overhead.** Under COMPPROF=2 every tick is bracketed, so the skipped ticks' ns/call
  is mostly profiler. Use the unprofiled A/B (§5).
- Not run in the game by this block. First verification is the coordinator's COMPPROF=4 heavy run
  (`SmTickNoop` mismatches = 0). Then the resave gate / `tools/build.sh run` (the sim is claimed
  unchanged, so the resave golden must hold). Then the COMPPROF=2 row and the A/B.

## 9. Build results (this block)

- **The hit path as built** (perf exe, `objdump` of `StateMachineComponent::Tick`). It is a leaf
  with no frame, and the two full-tick arms are out-of-line `noinline` helpers reached by tail
  jump:
  ```
  mov 0x50(%rcx),%r8d ; mov %r8d,%eax ; not %eax ; cmp %eax,0x54(%rcx) ; je hit
  add $0x48,%rcx ; jmp SmMemoFullTick                       <- miss
  hit: ++g_skipped ; load g_verifyOn ; test ; jne shadow ; ret
  ```
  GCC merges the four state bytes into one dword load. The miss path adds two loads of
  `g_trans30Calls`, the compare, and one store of the memo around the unchanged
  `StateMachine::Tick`. With `kEnabled = false` the body is the old one-line call.
- `tools/build.sh build perf --jobs 2`: OK, no warning in the touched files.
- `tools/build.sh unit perf --jobs 2`: **1106 tests, 0 failures** (6 new, §7).
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2**. No casts added; the integer `static_cast`s in
  `StateWord` are not counted.
- Not run: the game, the stress town, the resave gate (the coordinator's).
