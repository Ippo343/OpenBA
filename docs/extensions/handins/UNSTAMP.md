# UNSTAMP -- the particle un-stamp's "already ran this frame" test as a stamp in the component

Block UNSTAMP, 2026-10-13. PARTREPEAT.md section 8, doubt 2, built as faithful perf work in the
style of SMMEMO / STILLTERRAIN. There is no registry entry, no settings key, no `BANISHED_*` flag,
no `Opt()` field, no `getenv` and no door.
`static constexpr bool kUnstampStamp = true` (`src/game/Component/particlecomponent.cpp`) is the
kill switch. **The game was not run by this block.** The coordinator runs the COMPPROF=4 shadow,
the COMPPROF=2 row and the A/B.

**Short version.** `PC_UnstampOwnEmitters` runs before every ParticleComponent tick. Its first
step asked a 128 KB static open-addressing table `s_compTbl` (8192 slots x 16 bytes, keyed by the
component pointer, generation = the render-frame token `GameUpdateLoop_CurrentFrame()`): "has
this component already run its un-stamp this render frame?". That costs a hash and one random
line of the table per particle tick, ~7,660 ticks per frame in the heavy town. The question is now
answered by the frame token stored in the component itself, in `Component::_pad24` (+0x24). That
is a retail pad that the ctor zeroes and that nothing else reads or writes. The test is one
load from a line the tick touches anyway, plus a compare. The table is port-only scaffolding,
not a retail body. The stamp gives the same answer for the same inputs (section 1). The one
exception is a freed component's address reused within one render frame: there the table is
wrong and the stamp is right (section 1.3), and the output can only differ through a lowering
that seems never to fire (section 8). The table path stays compiled as the kill switch's false
arm. It is also the COMPPROF=4 shadow's reference.

## 1. The proof

### 1.1 The question and the two answers

The table path (unchanged, now `PC_CompSeenTable(self, frame)`): the probe starts at
`PC_SlotOf(self)` and walks forward. The first slot whose `gen != frame` is claimed
(`gen = frame, key = self`) and the answer is "not seen". A slot with `gen == frame && key == self`
answers "seen". If every one of the 8192 slots has `gen == frame` and another key, the answer is
"not seen" and nothing is claimed.

The stamp path: `seen = frame != 0 && self->_pad24 == (uint32_t)frame`. If not seen, it stores
`_pad24 = (uint32_t)frame`.

**Claim.** For each ParticleComponent object X and render frame F, both paths answer "not seen"
at X's first un-culled tick in F and "seen" at every later un-culled tick of X in F. The two
exceptions are listed in section 1.3.

*Table.* At X's first tick in F, no slot holds `{X, F}`. Claims happen only in this function,
with the key that probes, so a slot holding `{X, F}` would mean X already probed in F. The
probe therefore reaches a slot with `gen != F` (fewer than 8192 components tick in a frame; ~770
in the heavy town), claims it and answers "not seen". A slot claimed in F keeps `gen == F` until
the token changes, so X's probe path is the same at its next tick in F: every slot before X's slot
has `gen == F` (it was claimed in F, or X would have claimed it) and is skipped. X's slot then
answers "seen". Within F the slots ahead of X's can only be claimed, never released, so the path
stays the same.

*Stamp.* `_pad24` is written only by this store and by the ctor (zero), per the census in section
1.2. At X's first tick in F with F != 0, `_pad24` holds either 0 (the ctor) or the token of an
earlier frame F' < F. Neither equals F (section 1.4 covers the wrap), so the answer is "not seen"
and `_pad24 = F`. Every later tick in F reads F, so the answer is "seen". ∎

**Frame token 0 (the initial state, task item 1).** `s_frameNum` (`gameupdateloop.cpp:336`) is a
process-wide static `int`, initialised to 0 and incremented once per `GameUpdateLoop::Update`
(`:550`). Nothing resets it: not a new game, not a load, not a teardown. The increment comes
before step 3, the sim loop that runs the EntityList walk, so every particle tick sees a token >= 1.
A fresh component (stamp 0) therefore reads "not seen" on its first tick. Token 0 is reserved
anyway, so that the two paths agree even at token 0. The static table's slots start at
`{nullptr, 0}`, so at token 0 every slot reads as claimed: the table never answers "seen" and never
claims (8192 probes, then "not seen"). The stamp test is `frame != 0 && ...`, so it also never
answers "seen" at token 0. The store of 0 it then makes is harmless.

### 1.2 Census of `Component::_pad24`

Grep of `_pad24` over `src/` and `tests/`. Every other `_pad24` belongs to another class
(RendererDrawItem, Material, AnimStateRec, UI::Event, the Description classes, PathAvl, ...).
For `Component`:

| site | what |
|---|---|
| `src/game/Component/Component.h:102` | the declaration (public, before `protected:`), `static_assert` at +0x24 |
| `src/game/Component/component.cpp:30` | the ctor's member init `_pad24(0)`. Every Component, and so every ParticleComponent (`ParticleComponent()` chains `Component()`; both create paths, `particlecomponent.cpp:873/881`, are placement-new of the ctor), starts at 0 |
| `src/game/Component/particlecomponent.cpp` (this block) | the stamp test and store in `PC_UnstampOwnEmitters` |
| `src/diag/stress_harness.cpp` (this block) | the /part row reads it before and after a profiled tick (COMPPROF=2 only, a read) |

**Not serialized.** `Component::Persist` (`component.cpp`) serializes `Internal`, `_entity`, `_flags`
and `_description`, and nothing else. `ParticleComponent::Persist` chains to it and adds the
emitter state. No `*_serialize.cpp` names the field. **Not copied.** Components are created only by
their ctors (no copy ctor use, no memcpy of a component: `sizeof(ParticleComponent)` appears only
in the allocator call). **Not compared, not read by any shadow:** no other hit. No raw +0x24 read
of a Component exists. `castmetric` total is 2, so no offset cast hides one, and the +0x24 grep
over `src/game`, `src/ext` and `src/diag` finds only other classes' fields. Retail: the ctor at
0x1401188b0 does not write +0x24 (the header's own note). No retail body reads it, since it is
a pad.

### 1.3 Where the table and the stamp differ

| case | table | stamp | can it happen in a tick? |
|---|---|---|---|
| **Address reuse within one render frame**: X ticks in F, X is freed, a new component Y is constructed at X's address and ticks in F | Y reads "seen" (the slot holds `{addr, F}`): Y's un-stamp is skipped for the rest of F | Y's ctor zeroed `_pad24`, so Y reads "not seen" and runs its un-stamp | **Possible at speed >= 3, rare.** Components are freed only in `EntityList::DrainRemovals` (`RemoveEntity`), which runs at the top of every `EntityList::Update`, i.e. once per sim **iteration**: N drains per render frame at speed N, not one. The sequence: X ticks in iteration i and its entity is marked; the drain at the top of iteration i+1 frees it; a new entity with a ParticleComponent is created during iteration i+1 at the same address; it is staged and inserted by `PreUpdate` at iteration i+2, and ticks there. **The stamp is strictly more correct.** The answer should not depend on allocator addresses. |
| **More than 8192 distinct components tick in one frame** | the 8193rd and later never claim, and read "not seen" on every tick | exact, no capacity | No: ~770 per frame in the heavy town. |
| **Token 0** | never "seen" | never "seen" (reserved) | No tick runs at token 0 (section 1.1). Identical anyway. |
| **Token wrap** (`s_frameNum` is `int`; 2^31 frames is ~177 days at 140 fps; signed overflow, formally UB, is the same for both paths) | a slot from frame F reads "seen" again only if the token returns to F, i.e. 2^32 frames later with nothing claiming that slot in between | a stamp from frame F reads "seen" again only if the token returns to F, i.e. 2^32 frames later with the component not ticking in between | Not reachable in practice. Same exposure for both paths. |
| **Cost** | hash, then 1 to 8192 probes (1 in the common case), a random line of a 128 KB table, a store on first-of-frame | one load and one compare, a store on first-of-frame | n/a |

**What the address-reuse case changes in the output.** On a "not seen" answer, the un-stamp marks
each of Y's live emits in `s_seenTbl` (per-emit, per-frame). It lowers `frameTag` to `fi - 1` only
when `frameTag == fi` and the emit was not yet seen this frame. Two effects of the stamp path's
extra un-stamp are possible: (a) a lowering on one of Y's emits, and (b) an emit marked seen
earlier than the table path would mark it, which moves a later lowering from one component to
another. Both need `frameTag == fi` at the un-stamp, and that seems never to happen in steady
state (section 8, doubt 1; the row now counts lowerings). So the output is expected to be
identical. If the shadow ever reports a mismatch with **detail 2** (table seen, stamp not seen), it
is this case, and the stamp is the address-independent answer. Any other detail (1 = the stamp says
seen, the table does not; 3 cannot occur) is a bug in this block.

**Other things checked (task item 2):**
- **A component ticked by more than one walk.** `ParticleComponent::Tick` is reached only through
  slot 8 from the EntityList walk: `UpdateImpl<false>`, `UpdateImpl<true>` (ext.walk_batch, which
  offers only characters to WalkBatch; a particle component goes through the same slot call), the
  WALKCACHE TickList, the COMPPROF profiled dispatch (a bracket around the same slot call) and
  DIRECTTICK (off, the same function). All of them call the same `Tick`, which calls the helper once.
  Both paths count ticks per (object, frame), whichever walk made them.
- **`PC_Culled` returning before the un-stamp.** It returns before `PC_UnstampOwnEmitters` in both
  paths. A culled tick neither probes the table nor touches the stamp. A component culled on
  iteration 1 and drawn on iteration 2 runs its first un-stamp on iteration 2 in both paths.
- **The COMPPROF path.** The profiler's bracket (`CharProf_Begin` / `End`) only reads. The /part row's
  own `s_partSeen` table is separate from `s_compTbl` and unchanged.
- **Load / new game / teardown.** The token never resets. New components start at stamp 0, and an
  old world's table slots hold earlier generations. Both read "not seen". A load inside one
  `Update` before the walk is covered by the address-reuse row: the table can again only err
  towards "seen".
- **Threads.** Particle ticks run on the main thread only, so the stamp needs no atomics (the
  table had none either).

## 2. Design

```
const int frame = GameUpdateLoop_CurrentFrame();
if constexpr (kUnstampStamp) {
    const uint32_t token = static_cast<uint32_t>(frame);
    const bool seen = frame != 0 && self->_pad24 == token;
    if (g_verifyOn) { tbl = PC_CompSeenTable(self, frame); Verify("UnstampStamp", seen == tbl, ...); }
    if (seen) return;
    self->_pad24 = token;
} else {
    if (PC_CompSeenTable(self, frame)) return;
}
... the emit loop, unchanged (s_seenTbl dedup, frameTag = fi - 1)
```

- `PC_SlotOf` (the pointer hash) and `PC_CompSeenTable` (the old probe, byte for byte) were lifted
  out of the helper's lambdas into two file statics. The per-emit `s_seenTbl` still uses
  `PC_SlotOf`.
- **Where the stamp sits.** +0x24 is the 4-byte pad after `_flags` (+0x20) on the component's first
  16 bytes after `_description`. The ParticleComponent is 0x48 bytes and 8- or 16-aligned. The
  tick already reads the vptr (+0x00, for the slot-8 dispatch) and `emitters` (+0x38..+0x47,
  right after the test). +0x24 always shares a 64-byte line with one of the two: with the
  object at line offset 0 or 16 it is on the vptr's line, at 32 or 48 it is on the emitters'
  line. The test never brings in a line of its own.
- **Perf exe as built** (`objdump` of `ParticleComponent::Tick`, the un-culled path):
  ```
  mov  _frameIndex,%ebx ; call GameUpdateLoop_CurrentFrame ; mov %eax,%r12d
  mov  .refptr.g_verifyOn,%rax
  test %r12d,%r12d ; je  <first-of-frame / shadow arm>
  cmp  0x24(%rsi),%r12d ; jne <first-of-frame / shadow arm>     ; the stamp
  mov  (%rax),%r9d ; test %r9d,%r9d ; jne <shadow>               ; g_verifyOn
  <the tick body>
  ```
  `GameUpdateLoop_CurrentFrame` is a cross-TU call (it was before as well). Making it inline would
  save a few more cycles, but it needs `s_frameNum` exported. Not done; it is outside this block.

## 3. Decline rules

There is no decline: the stamp answers every un-culled tick. With `kUnstampStamp = false` the table
answers, exactly as before (the shadow and the stamp columns of the row are then silent / zero).

## 4. Shadow (BANISHED_STRESS_COMPPROF=4): `UnstampStamp`

On every un-culled particle tick with `g_verifyOn`, `PC_CompSeenTable(self, frame)` runs beside
the stamp test and the two answers are compared. **Expected mismatches: 0.** Detail: bit 0 = the
stamp says seen, bit 1 = the table says seen.

**The shadow does not change the live path.** The live decision is the stamp's. The table is
written only by its own probe, and with the shadow on it is read only by that probe. It is
probed exactly once per un-culled tick, the same sequence of `(key, frame)` probes the kill
switch's false arm makes. Its contents, and so its answers, are therefore the old path's
answers. The probe's writes (slot claims) feed only later shadow probes. Nothing else reads
`s_compTbl`, and the emit loop's `s_seenTbl` is a different table.

`kVerifySlots` in `stress_harness.cpp` went from 16 to 32. There were already 17 distinct
`StressHarness_Verify` names in the tree, and a name that arrives after the 16th is dropped
silently (`if (s_verifyN >= kVerifySlots) return;`). Most ext-only shadows do not fire in the faithful
heavy town, so the old limit had not yet been hit there. `UnstampStamp` is the 18th name, and
PREFETCHCOV may add more.

Read: `[VERIFY] UnstampStamp checks=N mismatches=M` in the [COMPPROF] report /
`standalone_error.log`. Expect `checks` ~ the /part row's ticks per frame x frames.

## 5. Counter row: the `/part` row extended (BANISHED_STRESS_COMPPROF=2)

The existing PARTREPEAT row gains a tail:
```
...; UNSTAMP stamp hits H/frame first F/frame none Z/frame, lowerings L/frame
```
- **hits**: the stamp held this frame's token before the tick, so the un-stamp returned at once.
  It should equal the row's `repeat`/frame (the profiler's own seen table asks the same question),
  ~6,880 heavy.
- **first**: the tick wrote the stamp (first-of-frame), ~765 heavy. It should equal `first`/frame.
- **none**: neither. `PC_Culled` returned before the un-stamp (ext.render_culling only), or
  `kUnstampStamp` is false. 0 in the faithful town.
- **lowerings**: `frameTag = fi - 1` stores per frame (`PartUnstamp::g_lowered`, counted in both
  arms). It is the measurement doubt 1 needs: 0 in steady state is the prediction.

The harness reads `_pad24` in `CharProf_Begin` (token 4) and again in `CharProf_End`, outside the
timed bracket. Outside COMPPROF=2 it costs nothing. The lowering counter is an increment on a
branch that seems never to be taken.

## 6. Prediction

Removed per tick: the pointer hash (shift, `imul`, mask), the load of the table slot's 16 bytes (a
random line of a 128 KB table: an L2 hit when the table stays resident, an L3 hit when the
walk's working set evicts it), and two compares. Added: one load from a line the
tick already touches, and one compare. On first-of-frame ticks (~765 / frame) the table slot store
becomes a store into the component line.
- ~7,660 ticks per frame heavy (the /part row, CLEAN run).
- ~2-5 ns per tick (L2 latency ~3-4 ns plus the hash; more when the table's lines are cold).
- **Predicted: ~0.015-0.04 ms/frame heavy, i.e. ~0.6-1.6% of the clean 2.34 ms EntityList.**
  That is the bottom of the brief's 0.02-0.06 ceiling, and at or under the A/B noise edge (the
  STILLTERRAIN A/B resolved ~1.8%). The compact town has fewer particle ticks. Expect a similar
  share, smaller in ms.
- 128 KB less in the hot set also relieves L2 for the rest of the walk. That is not
  predictable from here, and on a 96 MB L3 box the reviewer should not expect it to show (see
  the memory note on X3D parts). On a smaller-cache part it is a free gain if anything.
- **Interaction with PREFETCHCOV:** the ideas file's particle prefetch arm included
  `&s_compTbl[slotOf(c)]`. That agent was told to omit it because of this block. With the stamp
  there is nothing to prefetch for this test: the line is the component's own. If UNSTAMP were
  rolled back (`kUnstampStamp = false`), that prefetch arm would be worth reconsidering.

## 7. Files

| file | change |
|---|---|
| `src/game/Component/particlecomponent.cpp` | `kUnstampStamp`, `PC_SlotOf`, `PC_CompSeenTable` (the old probe, out of the helper), the stamp test / store, the `UnstampStamp` shadow, `++PartUnstamp::g_lowered` at the lowering, a dated note on the helper's stale banner |
| `src/game/Component/ParticleComponent.h` | `namespace PartUnstamp { inline long long g_lowered; }` |
| `src/game/Component/Component.h` | the `_pad24` comment names its port-only use |
| `src/diag/stress_harness.cpp` | `PartWindow` stamp / lowering counters, the Begin / End reads, the row's tail; `kVerifySlots` 16 -> 32 |
| `docs/extensions/handins/UNSTAMP.md` | this file |

`entitylist.cpp` is untouched (PREFETCHCOV's file).

## 8. Doubts

- **Doubt 1 of PARTREPEAT section 8: what the helper still does (analysis only, nothing changed but
  the counter).**
  - *The banner is stale.* It says the standalone "still lacks the per-render-frame advance of
    `_frameIndex` that retail's Renderer::Render performs" and that "without it ... the sim
    freezes". The helper came in with `ada555477` (2026-07-07). At that commit, step 3h read
    `if (g_pp_frameIndex && *g_pp_frameIndex) **g_pp_frameIndex += 1;`, with `g_pp_frameIndex =
    ResolveAddr(Module::Exe, 0x140151f38)`, an exe import slot. The banner's own claim (the sim
    froze) says that this slot did not resolve to a live cell in the standalone. So step 3h was dead,
    and the helper stood in for it. ENDGAME B.3.9 SS2 (`38737c9fd` / `cbf2349e2`, 2026-09-27) made
    step 3h write the class static `Graphics::ParticleInstance::_frameIndex` directly
    (`gameupdateloop.cpp:835`). Since then `_frameIndex` advances once per **sim iteration**, as in
    retail's own `GameUpdateLoop::Update`. Each pool is simulated once per iteration by its first
    toucher (PARTREPEAT section 1). Whether retail's Renderer also advances it is not needed here.
    The only other caller of `IncrementInstanceIndex` in the tree is the menu loop
    (`menudialogloopt.cpp:412`), which does not run in game. The status-icon remark in the banner
    describes the July state too.
  - *What the emit loop does in steady state.* At a component's first un-culled tick of render frame
    F (iteration k, token `fi_k`), take each live emit P not yet in `s_seenTbl` this frame.
    `frameTag(P)` is the token of the last simulate of P. If any ParticleComponent sharing P ticked
    earlier in F, its un-stamp already put P in `s_seenTbl`, so the loop skips P. Otherwise no
    component simulated P in iteration k before now, so `frameTag(P) <= fi_k - 1` and the test
    `frameTag == fi` fails: no lowering. So the lowering fires only when **something other than a
    ParticleComponent tick** stamped P with `fi_k` earlier in the same iteration, before the first
    component sharing P ran its un-stamp in F. The candidates: `StartEmitter`'s
    `instance->Update(Time 0)` or a prewarm in another component's tick / OnAdded / message earlier
    in the iteration, a TornadoComponent's emitter work, or a non-component ParticleInstance sharing
    the resource's records. When it fires, the component's tick then simulates P a **second** time in
    iteration k with the real elapsed time. Retail would take the SetBounds early exit there. So the
    lowering is a small divergence from retail, not a fix. In steady state it should be 0. The /part
    row's `lowerings/frame` now measures it.
  - *Can the helper be retired?* Probably, as a later `behaviour:` block. Remove the whole helper
    (both tables, the stamp and the emit loop). The tick then becomes retail's
    `ParticleComponent::Tick` with step 3h as the only advance. Evidence needed first: (1) this
    block's row on the heavy and compact towns, plus a town that starts and stops emitters (fires,
    disease, tornado, a new house's chimney), reading `lowerings/frame` = 0 or near it. (2) A
    by-hand look at smoke and status icons with the helper removed, at speeds 1 and 10. (3) The
    resave gate. If lowerings are exactly 0 everywhere, the retirement changes no output at all, and
    it removes ~770 first-of-frame emit-table probes per frame plus this block's stamp. Not done
    here: this block replaces the probe only.
- **The address-reuse difference** (section 1.3) makes the stamp path not formally bit-identical
  to the table path in one rare case. It is the table that depends on allocator addresses. A detail-2
  shadow mismatch would be this case. It can change the output only through a lowering (see
  doubt 1). I think the stamp is the one to keep either way.
- **Writing a retail pad.** `_pad24` is part of the retail object layout, and the port now writes a
  value there. Retail code never runs in this exe (`reimpl-x64.exe` loads no retail binary), the save
  does not carry it, and no shadow or tool compares raw component bytes (the census). If a future
  `Component` port finds a use of +0x24 that the header missed, this block must move the stamp
  (a side array keyed by the component's walk index, for instance).
- **Speed 1.** At speed 1 every tick is first-of-frame, so the stamp saves the table probe and adds
  a store (as the table did). Neutral to slightly positive.
- Not run in the game by this block. The coordinator should run, in order: COMPPROF=4 heavy
  (`UnstampStamp` 0 mismatches, the other shadows still 0; `UnstampStamp` must appear at all, which
  it now can with 32 slots), COMPPROF=2 heavy (the /part tail: hits == repeat, first == first,
  none 0, lowerings ~0), the resave gate / `tools/build.sh run`, then the A/B
  (`kUnstampStamp` false vs true, interleaved, heavy and compact, CLEAN).

## 9. Build results (this block)

- `tools/build.sh build perf --jobs 2`: OK. 0 warnings in the touched files (183 elsewhere in the
  fresh tree, pre-existing).
- `tools/build.sh build debug --jobs 2`: OK, 0 warnings in the touched files.
- `tools/build.sh unit perf --jobs 2`: **1130 tests, 0 failures**. No test was added. The helper is
  a file static of `particlecomponent.cpp`, which the unit binary does not link, and a test of it
  would need the game loop (the frame token, the walk, the emitters' resources). The COMPPROF=4
  shadow is the check, on the live walk.
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2** (integer `static_cast<uint32_t>` only).
- The worktree needed `git submodule update --init` (third_party/SDL) before the first configure.
- Not run: the game, the stress town, the shadow, the row, the A/B, the resave gate (the
  coordinator's).
