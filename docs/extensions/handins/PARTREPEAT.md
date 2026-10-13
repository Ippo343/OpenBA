# PARTREPEAT -- the particle repeat-tick skip at speed N (NOT BUILT: not exact)

Block PARTREPEAT, 2026-10-13. Candidate #3 of `IDEAS_20261012.md` (§2.3). Step 0 was run: one
heavy-town COMPPROF=2 run. **The time bar was met (repeat ticks ~3.4% of EntityList), but the
exactness premise of §2.3 is false, so the skip was not built.** What this block leaves is the
Step 0 instrument (the `[COMPPROF] /part` row) and the verdict below. No game code was changed.

**Short version.** §2.3 assumes `ParticleInstance::_frameIndex` is constant within a render frame,
so a component's later ticks in the frame find every pool already stamped and do nothing.
That is not the case. `GameUpdateLoop::Update` step 3h (`gameupdateloop.cpp:835`, retail's)
advances `_frameIndex` **once per sim iteration**. So in every iteration, each pool is simulated
again by the first instance that touches it. The pool's bounds are rebuilt from particles that
moved. Every other instance on that pool then takes the "early exit", which is
`Geometry::SetBounds(pool->_bounds)`, and that copies the new bounds into its own
`_localBounds` and sets dirty bit 2. A "quiet" repeat tick is therefore a real write on
essentially every iteration. A skip would leave the instance's local bounds one or more
iterations stale. Those bounds are what the scene cull and the draw read.

## 0. Step 0: the numbers and the verdict

**Run.** `STRESS_ARGS="--label partrepeat_c2 --spread --citizens 1600 --frames 6000 --map large"
BANISHED_STRESS_COMPPROF=2 tools/build.sh stress`. Heavy town: large map, 1,600 citizens, spread
layout, speed 10, seed 31337. Perf exe at 64259ef5a (main efa4a8dbf + the /part row). Mods
parked; game-dir lock held. Exit 0, no crash.
Result directory: `perf/results/20261013_085556_partrepeat_c2/` (not committed; `perf/results`
is ignored).
**machine load: NOT CLEAN** (the stress script's own verdict was BUSY: median CPU 79%, other agents
compiling). Read the counts and the shares. Do not quote the ms values as a timing.

**The instrument.** `CharProf_Begin` token 4 (`src/diag/stress_harness.cpp`, `PartWindow`,
`PartReport`) splits every ParticleComponent slot-8 tick in two:
- **first**: the component's first tick of the render frame. A profiler-side seen table, keyed
  by component and `GameUpdateLoop_CurrentFrame()`, decides this.
- **repeat**: any later tick in the same frame.

It also counts the **early-exit** repeats. These are ticks where, before the tick:
- every emitter with an instance has `nodeIndex < 0`;
- every live emitter has `fadeRate == 0` and `opacity > 0`;
- every live emitter's pool is stamped (resource active, `frameTag == _frameIndex`).

So Update would take its SetBounds exit for every emitter. (The row first called these
"quiet". That label was wrong, for the reason in §1.)

| window (frame) | ticks/frame | first /frame, ns/call | repeat /frame | repeat ms/frame | repeat ns/call | repeat % of EntityList | early-exit % of repeats | early-exit ms/frame, ns/call | live emitters / tick |
|---|---|---|---|---|---|---|---|---|---|
| 600 | 6,794 | 679, 25.3 | 6,114 | 0.095 | 15.5 | 3.11% | 98.6% | 0.085, 14.2 | 1.00 |
| 1200 | 7,149 | 715, 26.5 | 6,434 | 0.106 | 16.4 | 3.39% | 99.3% | 0.091, 14.2 | 1.01 |
| 2400 | 7,722 | 772, 27.6 | 6,949 | 0.116 | 16.7 | 3.64% | 99.2% | 0.100, 14.5 | 1.02 |
| 3600 | 7,649 | 765, 25.7 | 6,884 | 0.113 | 16.4 | 3.43% | 99.3% | 0.098, 14.3 | 1.02 |
| 4800 | 7,675 | 768, 30.4 | 6,907 | 0.127 | 18.4 | 3.31% | 99.1% | 0.105, 15.3 | 1.02 |
| 6000 | 7,658 | 766, 29.7 | 6,892 | 0.124 | 18.0 | 2.95% | 99.2% | 0.107, 15.6 | 1.02 |

Other windows are in the same ranges. Context rows from the same run:
- `/tree ParticleComponent`: 0.125-0.151 ms/frame, ~7,660 calls/frame, 17-20 ns/call.
- `/sm`: a skipped StateMachine tick costs 7.3 ns under the profiler. This is the floor of a
  bracketed tick that does almost nothing.
- EntityList, profiled and under load: 3.0-4.2 ms/frame. The clean unprofiled figure is 2.34.

**Against the bar.** The repeat ticks are 2.95-3.64% of EntityList, mean ~3.4%, so the bar of
"at least ~3%" is met, narrowly. The part a skip could have removed is the early-exit repeats
less the profiler floor: ~6,850 x (15 - 7.3) ns, about 0.053 ms/frame. That is ~2.3% of the
clean 2.34 ms EntityList.

**Verdict: not built.** The bar was met. The proof required by step 2 of the brief fails (§1),
so this is the brief's "exactness fails: say exactly what and stop" case. The 0.8% of repeats
that are not early-exit are ~55 ticks/frame, ~6 per iteration. That matches §1: the first
toucher of each pool, in every iteration.

## 1. The proof, and where it fails

Claim §2.3 needed: on a repeat tick in the same render frame, with every live emitter quiet
(`fadeRate == 0`, `opacity > 0`, `nodeIndex < 0`), the faithful body writes nothing.

Taking the body piece by piece (`ParticleComponent::Tick`, `particlecomponent.cpp:706`):

1. `PC_Culled`: off in the faithful profile. With ext.render_culling on it decides on its own
   and returns first. Not the problem.
2. `PC_UnstampOwnEmitters`: on a repeat tick, the component is already in `s_compTbl` for this
   frame token. The probe finds it and returns without writing: every slot before it on the
   probe path has `gen == frame`, so none is claimed. Not the problem.
3. The emitter loop. Node follow is skipped (`nodeIndex < 0`). The fade arm writes nothing
   (`fadeRate == 0`, `op > 0`, so `op == g->_opacity` and there is no store). Then
   `ParticleSystemInstance::Update(elapsed)` runs (`wave28_lane_partsys.cpp:383`):
   - `_boundsPtr = _resource->_records._data[_index]`: the same value, because the records are
     fixed after the resource loads.
   - `pool->frameTag == frame`? `frame` is `*FrameIndexPtr()` = `ParticleInstance::_frameIndex`.
   - If yes: `Geometry::SetBounds(pool->_bounds)` (`scene.cpp:1214`). It writes
     `_localBounds = box; _dirtyFlags |= 2` whenever any of the six floats differs.
   - If no: the full simulate (spawn, age, rebuild `pool->_bounds`, SetBounds inlined, compact),
     which stamps `frameTag = frame`.

**The step that fails: is `pool->_bounds` unchanged since this instance's previous tick?**
`gameupdateloop.cpp:835` (step 3h, inside the `do { ... } while (iteration < speedMultiplier)`
loop):
```
::Graphics::ParticleInstance::_frameIndex = ::Graphics::ParticleInstance::_frameIndex + 1;
```
So `_frameIndex` is a per-iteration token, not a per-render-frame one. Take iteration k, with
`_frameIndex = fi_k`. A pool P carries the stamp `fi_{k-1}` from the previous iteration. The
first instance to reach P in the walk (any component sharing P; pools are shared through the
resource's `_records`, indexed by a random `_index` chosen in `SetParticle`) finds
`frameTag != fi_k`. It simulates P, rebuilds `P->_bounds` from the moved particles, and stamps
`fi_k`. Every other instance on P then takes the early exit, and its `SetBounds` compares its
`_localBounds` with bounds from iteration k-1. Those differ as soon as one particle at an
extreme of the box moved. Smoke rises, so the box differs on essentially every iteration. Only an
empty pool, whose box is zero-filled both times, compares equal.

Two consequences:
- **No fast path can keep the "first toucher" tick.** That tick does the simulate, so it is never a
  no-op. Which component is the first toucher depends on walk order, not on the component's
  own state.
- **The other repeat ticks are not no-ops either.** They each publish the new bounds into their
  own Geometry. Skipping them leaves `_localBounds` stale: the scene cull and the draw then use
  an old box, and `_dirtyFlags` bit 2 is not raised.

The memo of §2.3, `{component, frame, unstampEpoch}`, cannot see this. The write is driven by
another instance's simulate of a shared pool, inside the same frame, with no un-stamp involved.
Adding the pool's bounds or `frameTag` to the key would make the hit test chase the same lines
the tick chases: the emitter slot, the instance, the resource, the records, the pool. That
removes the point of the skip.

**Why §2.3 believed it.** The comment above `PC_UnstampOwnEmitters` describes a `_frameIndex`
that never advances ("without it ParticleSystemInstance::Update dedups forever ... and the sim
freezes"). Step 3h advances it every iteration, which the ideas file did not account for.

## 2. Design

Not built. For the record, the design the brief asked for would have been:
- the memo in `Component::_pad24` (+0x24). Every component has these 4 bytes; retail never
  writes them and nothing reads them. They sit on the component's first line, next to
  `_flags`, which the walk already reads.
- a global epoch, bumped at the frame boundary and on every un-stamp lowering.
- clears in every ParticleComponent member that writes an emitter slot (`Setup`, `Place`,
  `StartEmitter`, `StartEmitterFade`, `StopEmitter`, `OnAdded`, `OnRemove`, `OnEvent`,
  `Slot11`, `Persist`, `StopEmitterFade` in `diseasecomponent.cpp`, `FadeOutEmitter` in
  `tornadocomponent.cpp`).

It is exact only if pool bounds cannot change within an epoch. Step 3h makes that false (§1),
and the per-iteration simulate does not lower any stamp, so the epoch never sees it.

## 3. Decline rules

n/a (not built).

## 4. Shadow

n/a. Had it been built, the `ParticleRepeat` shadow would have reported mismatches on
`_localBounds` and dirty bit 2 for almost every hit on a non-empty pool.

## 5. Counter row: `[COMPPROF] /part` (kept)

`BANISHED_STRESS_COMPPROF=2` prints, after `/sm`:
```
[COMPPROF]   /part ParticleComponent slot-8 (PARTREPEAT step 0) ticks/frame=N: first F/frame x ms/frame y ns/call; repeat R/frame ... (= P% of EntityList); early-exit repeat Q/frame (q% of repeats) ... L live emitters/tick (= P2% of EntityList; EntityList E ms/frame)
```
It costs nothing outside COMPPROF=2. Under COMPPROF=2 it adds one hash probe per particle tick,
plus a walk of the emitters on repeat ticks, both outside the timed bracket. It stays useful as
the particle split for rank 1 of the ideas file (prefetch coverage II), whose particle arm attacks
the same lines.

## 6. Prediction

n/a. The ceiling, had the skip been exact, was ~0.05 ms/frame (~2.3% of EntityList, §0).

## 7. Files

| file | change |
|---|---|
| `src/diag/stress_harness.cpp` | `PartWindow` / `s_partWin` / `s_tidPart` / `s_partSeen`, token 4 in `CharProf_Begin` / `End`, `PartReport` (the /part row), reset per window. Includes `ParticleComponent.h`, `graphics/ParticleInstance.h`, `<functional>`. |
| `src/game/Game/GameUpdateLoop.h` | declares `extern "C" int GameUpdateLoop_CurrentFrame()` (defined in `gameupdateloop.cpp`; the harness reads it; `particlecomponent.cpp` still has its own local declaration, untouched) |
| `docs/extensions/handins/PARTREPEAT.md` | this file |

## 8. Doubts and follow-ups

- **The un-stamp under step 3h.** With `_frameIndex` advancing every iteration,
  `PC_UnstampOwnEmitters` lowers a pool only when that pool was already stamped *this iteration*
  before any ParticleComponent's un-stamp saw it this frame. That can happen through
  `StartEmitter`'s `Update(Time 0)`, a prewarm, or a non-component instance sharing the
  resource's records. The lowering then makes a second simulate of that pool in the same
  iteration. In steady state it seems never to fire. Its comment, though, describes a world
  without step 3h. That deserves its own look (is the helper still needed at all?). Not touched
  here.
- **A smaller exact candidate is still on the table.** The un-stamp's per-tick component probe
  into `s_compTbl` (a 128 KB static, one random line per particle tick, ~7,660 ticks/frame) could
  be replaced by a per-component frame stamp in `Component::_pad24`. The table answers only
  "has this component un-stamped this frame", which a stamp in the object answers without the
  random line. It is exact by construction, because the table is port-only scaffolding, not a
  retail body. The ceiling is perhaps 3-8 ns x 7,660, about 0.02-0.06 ms/frame, and rank 1's
  `&s_compTbl[slotOf(c)]` prefetch competes for the same miss. Not built: outside this brief.
- **The /sm row's `est. saved`** reads absurd values (13 ms, 430% of EntityList) in windows with a
  single full tick of ~2 us. It extrapolates a rare heavy transition over 6,174 skips. SMMEMO §5
  already calls it indicative and an overstatement. With fewer than ~10 full ticks in a window it
  is noise.
- Step 0's ms figures were taken under load (`machine load: NOT CLEAN`). The shares and counts
  are what this verdict rests on, and the verdict does not depend on them at all (§1 is a code
  argument).

## 9. Build results (this block)

- `tools/build.sh build perf --jobs 2`: OK. No new warning; the touched files compile clean.
- `tools/build.sh unit perf --jobs 2`: **1126 tests, 0 failures** (no test added; nothing was built to test).
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2**. The first draft of the /part row hashed the pointer
  with a C-style `(uintptr_t)` cast (total 3). It now uses `std::hash<const void*>`, like
  `PC_UnstampOwnEmitters`.
- Not run: the resave gate, `tools/build.sh run` (the coordinator's). The game code is
  unchanged, so neither is affected.

## 10. Results (coordinator, 2026-10-13 morning) -- CLOSED, NOT EXACT; the /part row ships

The coordinator read the claim at its source: `gameupdateloop.cpp` step 3h advances
`ParticleInstance::_frameIndex` inside the per-iteration loop, so the pool is re-simulated once per
sim iteration, not once per render frame, and the early exit's `SetBounds` copies fresh bounds. The
ideas file's premise was wrong; the verdict stands. The /part row on a CLEAN heavy run (COMPPROF=2,
`20261013_..._st_row_heavy`, windows 9 / 10): 7,666 / 7,642 particle ticks per frame, 767 / 764
first-of-frame at 23 ns, 6,899 / 6,878 repeats at 16 ns = 0.110 / 0.109 ms/frame (3.1% / 2.9% of
EntityList), 99.2% of repeats take the early exit. Candidate closed; the smaller exact one (the
un-stamp's s_compTbl probe replaced by a per-component frame stamp, section 8 doubt 2) stays open
for the rank-1 prefetch session.
