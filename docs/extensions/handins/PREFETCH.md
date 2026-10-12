# PREFETCH -- a staged prefetch pipeline for the slot-8 update walk

Block PREFETCH, 2026-10-12. The user's question: "staged prefetch for node/tree walks" -- can a
software-pipelined prefetch of the per-iteration slot-8 walk's pointer chain buy anything? Faithful
perf work in the style of TREE_SNAPSHOT / WALKCACHE: hints only, the visit order and every tick
unchanged, no registry entry, no settings key, no flag. One compile-time constant chooses the
variant; the shipped value generates the walk exactly as before (section 4).

**Not run in the game by this block** (rules): the coordinator runs the stress A/B (section 6).

## 1. What is already there (this changes the question)

The snapshot walk (`src/game/Entity/entitylist.cpp`, `WalkUpdateTreeSnapshot`) has carried a
three-stage pipeline since TREE_SNAPSHOT (2026-09-05), narrowed by WALKCACHE:

| stage | distance | target |
|---|---|---|
| entity header | i+3 | `&e->_components` (+0x30) |
| component IBP array | i+2 | `e->_components._data` (first line) |
| component flag words | i+1 | `&c->_flags` (+0x20) of the listed components (WALKCACHE entry), or of the first six |

So the "staged" shape the question asks about already exists for the walk's OWN loads. The earlier
"no gain" attempts (`BANISHED_TREE_PREFETCH` 0.26 vs 0.25, two levels 0.27, a 3-deep prefetch
"neutral or worse") were on the AVL tree walk, before the snapshot; the shipped three-stage pipeline
was never A/B'd on its own. What it does not cover is what the TICKS read first, which is where a
deeper pipeline can still differ.

## 2. The per-entity load chain

One visit, heavy town (2,630 entities per iteration: ~2,000 characters, each with a ticking
CharacterComponent and ModelComponent; ~770 particle systems; ~620 building state machines; ~70
status icons). Allocations are 8- or 16-aligned, so line splits are by alignment, not by layout.

| # | load | depends on | lines | covered today |
|---|---|---|---|---|
| 1 | `arr[i]` (snapshot), `tl[i]` (TickList entry) | -- | sequential streams, 21 KB + 42 KB | hardware prefetcher |
| 2 | `e->_entityFlags` (+0x40), `e->_components` data / size (+0x30 / +0x38) | 1 | 1, or 2 when `e % 64` is 0x00 or 0x08 (+0x30..+0x3F and +0x40 straddle; a quarter of the entities) | the +0x30 line at i+3 |
| 3 | `comps[idx[k]]` (8-byte IBP slots) | 2 | 1-2 | first line only, i+2 |
| 4 | `c->_flags` (+0x20) | 3 | 1 | i+1 |
| 5 | `c->vptr` (+0x00), then the slot-8 entry in the vtable | 3 | +0x00 and +0x20 are on DIFFERENT lines for half the objects (`c % 64` >= 0x20, with 8- or 16-alignment) | no |
| 6 | the tick body's first reads (below) | 5 | | no |

What each tick reads first (all through `this`, then one or two dependent hops):

- **CharacterComponent** (0x80, `Tick` -> `CharacterComponent_MoveTick_1094c0`,
  `citizenai_executor.cpp`): `actionStack.current` (+0x60), `actionStack.actions._data` (+0x48)
  -> the current slot -> the **Action** object (`_status` +0x18, `_next` +0x10, vptr) -> its `_next`
  chain (2-3 nodes; MoveTo is 0x480 bytes, its Execute reads the path poll and step fields);
  then `modelComponent` (+0x68) -> the model's `transform` (+0x148..+0x163) and yaw;
  `characterDescription` (+0x78, shared per citizen type, always hot); `field_0x2C..0x3C`.
- **ModelComponent** (0x170, `ModelComponent::Tick`): the embedded `MeshInstance` (+0x28, 0x120
  bytes): `_transform` (+0x84 of the instance), `_animation` (+0xd8: `_layers._data` -> layer 0
  (stride 0x38) -> `_group` -> the clip, shared), `_nodes._data` (the pose array, stride 0x38; node
  0 on the root-only iterations, every node on the last), `_rootAnimationOffset` (+0x100);
  `IsAnimating` re-reads layer 0. So the model object is touched over 5-6 of its 6-7 lines.
- **ParticleComponent** (0x48): `emitters._data` -> each 0x18 slot -> `instance` -> the particle
  arrays. (PerfX's cull test first, off in the faithful profile.)
- **StateMachineComponent** (0x58): `stateMachine` (+0x48, 0x10 bytes) -> the current node and its
  state table entry.

The dependent hops in 6 (Action, layers, poses, emitters) are the misses no walk-level prefetch of
today's pipeline can reach; they are also the ones the out-of-order core cannot overlap across
entities, because the next entity's addresses are not known until the indirect call returns.

## 3. The staged pipeline (`kWalkPrefetch == 2`)

One entity apart per stage (an average visit is ~75 ns, about one DRAM latency and ~6 L3
latencies), five stages, each touching only lines the stage before pulled in:

| stage | distance | prefetches |
|---|---|---|
| S0 | i+5 | the entity header: the `_components` line AND the `_entityFlags` line (load 2's straddle) |
| S1 | i+4 | each IBP slot the visit will read: the listed indices (cached entry) or the first six |
| S2 | i+3 | each such component's first 0x80 bytes (`c`, `c+0x40`, `c+0x7F`): vptr + `_flags` (load 5's split), the whole CharacterComponent / ParticleComponent / StateMachineComponent, the ModelComponent's head |
| S3 | i+2 | by dynamic type (the vptr, resident since S2, compared with `CharacterComponent::Vtable()` / `ModelComponent::Vtable()` hoisted out of the loop): a character's current action IBP slot (`&actions._data[current]`, bounds-checked against the live size); a model's remaining lines (+0x80 .. +0x16F) |
| S4 | i+1 | a character's current Action object (`a`, `a+0x1F`: vptr, `_next`, `_status`); a model's `_animation._layers._data` and `_nodes._data` |

Rules kept:

- **Exact.** Prefetches never fault and never change a value. The reads a stage makes to compute an
  address (`_components`, a slot, a vptr, `actionStack.current` / `_data` / `_size`, a layer /
  pose array pointer) are plain loads of live objects: the snapshot's entities and their
  components cannot be freed during the walk (frees happen in `DrainRemovals`, outside it -- the
  same assumption the shipped pipeline makes), and every address is re-derived from live fields
  inside the stage that uses it: nothing is carried from one stage to the next except the TickList
  entry, whose indices are checked against the live count exactly as the shipped pipeline does. A
  stale hint costs a wasted prefetch, never a wrong read. The action slot is bounds-checked
  (`0 <= current < _size`) because an out-of-range read could cross a page; the null tests are on
  prefetch-only reads, not on any read the faithful code makes.
- **No toggle in the loop.** `kWalkPrefetch` is `static constexpr`; the variants are `if constexpr`
  arms, so the shipped value compiles the shipped code (section 4).
- **No new doors.** The vptrs come from `DEFINE_EXE_VTABLE` (the existing B38 D5 accessor for the
  compiler's own table, as StatGoal / GameUpdateLoop use it), added for CharacterComponent and
  ModelComponent; nothing is resolved by retail name or address.

Not staged (left for a follow-up if S2-S4 show anything): the Action `_next` chain beyond the first
node (needs a sixth stage), the particle emitter chain, the state machine's node, the
characterDescription (shared, always hot).

Cost per entity of variant 2: ~6-14 prefetches and ~4-8 extra loads (vptr compares, the action
stack fields, the layer / pose pointers), all to lines already resident. The shipped pipeline is
3-8 prefetches and 2-7 loads. Variant 2 therefore ADDS work to the walk; it has to win it back.

## 4. Files, and how to flip

| file | change |
|---|---|
| `src/game/Entity/entitylist.cpp` | `kWalkPrefetch` (0 / 1 / 2, shipped 1); `WalkVptrs`, `ForEachWalkCandidate`, `WalkPrefetchStaged` beside the snapshot walk; the shipped three-stage block wrapped in `if constexpr (kWalkPrefetch == 1)`; `#include "Component/CharacterComponent.h"` |
| `src/game/Component/charactercomponent.cpp` | `DEFINE_EXE_VTABLE(CharacterComponent, "_ZTV18CharacterComponent")` |
| `src/game/Component/modelcomponent.cpp` | `DEFINE_EXE_VTABLE(ModelComponent, "_ZTV14ModelComponent")` |

**Flip:** one constant, one file -- `src/game/Entity/entitylist.cpp`,
`static constexpr int kWalkPrefetch = 1;`

- `0` = no walk prefetch at all (what the shipped pipeline buys);
- `1` = shipped (TREE_SNAPSHOT / WALKCACHE three-stage);
- `2` = the staged pipeline of section 3.

Then `tools/build.sh build perf --jobs 2` (only `entitylist.cpp` recompiles).

**Shipped value = shipped code, checked.** The perf object of `entitylist.cpp` built with
`kWalkPrefetch = 1` was disassembled with relocations (`objdump -dr`) and diffed against the same
TU compiled from `HEAD` (62e766c06) with the same command line: 0 lines differ. The two
`DEFINE_EXE_VTABLE` lines add two unused 3-instruction accessors to their own objects and touch
no walk code. Variant 0 leaves only the AVL fallback walk's two prefetches (`g_treePrefetch`);
variant 2's object carries 34 prefetch instructions (the stages, in each of the walk's loop
copies) and the two `Vtable()` calls hoisted before the loop.

**A GCC trap found on the way (worth knowing for any prefetch work).** The first variant-2 build
had NO prefetch in it: GCC declares `__builtin_prefetch` as touching no memory, so a helper that
is not inlined early and whose only effects are prefetches is classed `pure` by IPA, and its
void call is deleted as dead. The whole stage set vanished silently. Every helper and stage
lambda is now `always_inline`; check `objdump -d ... | grep -c prefetch` after any change here.
This may also be why some of the 2026-09 "no gain" prefetch attempts measured nothing -- only if
they were written as out-of-line helpers; the shipped three-stage block is inline in the loop and
survives.

Gates on the shipped value: `tools/build.sh build perf --jobs 2` OK, `tools/build.sh unit perf
--jobs 2` 1090 tests / 0 failures, `tools/abi_audit.py` OK, `tools/acceptance/acceptance.sh`
0 lines off target. Variant 0 and 2 TUs compile clean (no new warnings).

## 5. Working set: for which caches the misses are real

Per iteration, the walk + ticks touch (estimates from the struct sizes above; poses, MoveTo and
particle arrays are the uncertain terms, given as ranges):

| what | per unit | units (heavy town) | per iteration |
|---|---|---|---|
| snapshot + TickList | 24 B | 2,630 | 63 KB |
| entity header lines | 64-128 B | 2,630 | 0.17-0.34 MB |
| IBP slot lines | 64-128 B | 2,630 | 0.17-0.34 MB |
| CharacterComponent | 128-192 B | 2,000 | 0.26-0.38 MB |
| ModelComponent (+ MeshInstance) | 384-448 B | 2,000 | 0.77-0.90 MB |
| action slot + Action chain (MoveTo is 0x480) | 192-448 B | 2,000 | 0.38-0.90 MB |
| animation layers + node-0 pose + instanced pool slot | 128-192 B | 2,000 | 0.26-0.38 MB |
| CharacterGrid / CharMgr slot | 64-128 B | 2,000 | 0.13-0.26 MB |
| particle systems (component, slots, instance 0xe0, particle arrays) | 0.5-2 KB | 770 | 0.4-1.5 MB |
| state machines (component, node, table entry) | 128-256 B | 620 | 0.08-0.16 MB |
| **intermediate iteration** | | | **~2.6-5.1 MB** (~1.1-1.6 KB per character) |
| last iteration adds the full pose (20-40 bones x 0x38) + terrain under each citizen | 1.1-2.2 KB | 2,000 | **+2.2-4.5 MB -> ~5-10 MB** |

Plus the code of the ticks and the shared animation data (~1-2 MB, read-only, shared).

Where it stops fitting:

- **L2** (512 KB Zen 3, 1 MB Zen 4/5, 1.25-2 MB Intel P-cores): never. Every iteration's walk is an
  L3 walk on every machine, which is why this box shows the cost as L3 latency (~45-50 cycles),
  not DRAM.
- **L3 <= 8 MB** (4-6 core laptops and older desktops, Ryzen 5 APUs, i5/i7 up to 9th gen): the
  last iteration alone is at or above it, the intermediate ones share it with the rest of the
  process and the driver: DRAM misses on every iteration.
- **L3 12-16 MB** (i5-12400 / 13400, Ryzen 7 5700U/7840U at 16 MB): the intermediate iterations fit;
  the first iteration after each Draw pays DRAM for whatever the render pass evicted (the draw
  lists and instance buffers for the same 2,000 models), and the last iteration's poses may spill.
- **L3 32 MB** (Ryzen 7 5800X / 7700X, i7/i9 with 30-36 MB): fits, with headroom, at this town
  size; it stops fitting around 3-4x the heavy town (6,000-8,000 citizens, by the per-character
  figure above).
- **96 MB X3D** (this box): fits at any population the game can reach.

So on this machine a removed miss is an L3 hit (~11 ns); on an 8-16 MB part a fraction of the same
misses are DRAM (~70-90 ns), where a stage distance of one entity is just enough to hide one.

## 6. Measurement plan (coordinator)

Three perf builds, same tree, `kWalkPrefetch` = 0, 1, 2. Faithful profile (every extension off,
the default stress profile). Idle machine (`machine load: CLEAN`), RTSS stopped, interleaved A B C
A B C, two runs each:

1. **Heavy town** (large map, 1600 citizens, spread; ELPROF's / WALKCACHE's town, speed 10):
   compare **sim ms/iter** (baseline 0.29), **EntityList ms/frame** (2.51) and its **tree walk**
   sub-phase (1.96, the COMPPROF=1 default). Noise on sim ms/iter is +-0.01 (= 3.4% here), on
   the tree walk ~+-0.03 ms/frame. A real effect needs **>= 0.02 ms/iter or >= 0.06 ms/frame on
   the tree walk, the same sign in both pairs**.
2. **Compact town** (default `tools/build.sh stress`, 0.16): the control; the same direction
   expected, smaller.
3. Optional, only if (1) shows a difference: one `BANISHED_STRESS_COMPPROF=2` run per variant for
   the `/tree` rows: the CharacterComponent and ModelComponent ns/call (57 / 12 in ELPROF) are
   where S2-S4 land; ParticleComponent / StateMachine should not move (only S0-S2 cover them).
   COMPPROF=2 adds ~0.03 ms/iter of its own, so compare those rows only with each other.

Reading the three:

- 0 vs 1 says whether the shipped pipeline buys anything here. If 0 == 1 within noise, the walk is
  not latency-bound on this box and 2 is not expected to differ either.
- 1 vs 2 is the question asked.

**Making it sensitive to a miss on this box.** The 9800X3D has one CCD and one 96 MB L3, so there is
no smaller-cache CCD to pin to, and no reachable population pushes the set past 96 MB (that needs
~60,000 citizens). What does work without touching the game:

- **A cache-pressure co-runner**: a process on two other cores that streams over a buffer of 256 MB+
  in a loop (any read-only sum loop) keeps evicting the shared L3, so the game's effective share
  drops to a few MB -- the 8-16 MB case above. Run all variants under the same co-runner,
  interleaved; label the runs `pressure`, not CLEAN (the co-runner IS the load). This is a tool
  outside the repo, so it is the coordinator's / user's call under "no new instruments"; it is
  the only way this machine can show the DRAM case.
- **Speed 1** runs one iteration per Draw, so every walk follows a render pass. On this box the
  render does not evict 96 MB, so it should NOT differ -- a useful negative control for the
  co-runner run.
- A bigger town (`--citizens 4000 --spread` on the large map, ~6-10 MB per iteration) makes the
  walk longer and the absolute differences larger, but still L3-resident here: it raises
  resolution, not miss cost.

## 7. Expectation, and the honest ceiling

Ceiling. The walk itself is ~13% of EntityList (ELPROF); after WALKCACHE the tree walk is 1.96 of
2.51 ms/frame, nearly all of it inside the ticks. What a prefetch can remove is the stall on lines a
tick reads before it can issue anything else: per character visit, at most the Action slot,
the Action node, 2-4 model / MeshInstance lines and the layer / pose arrays -- about 6-9 L3 hits,
of which the out-of-order core already overlaps the independent ones (the model lines among
themselves). Serially exposed, maybe 3-4 L3 hits = 35-45 ns per character in the worst case; the
measured character + model tick is ~69 ns per character, so the real exposed share is likely much
smaller. A realistic upper bound here: -5-10% of the tree walk = -0.1-0.2 ms/frame heavy = -0.01-0.02
ms/iter. The added work (~10 prefetches + ~6 loads + 2 compares per character entity, ~4-6 ns) eats
a third to a half of that.

Prediction: **on this box, variant 2 within noise of variant 1 (|delta| <= 0.01 ms/iter), possibly
+0.01 worse in the compact town; variant 0 within noise of 1 too.** The case for it is the DRAM case
(section 5): on an 8-16 MB L3 the same stages hide real 70-90 ns misses on the Action chain and the
model, where the shipped pipeline covers only the entity header, the slots and the flag words.

Keep / drop (the user's rule): keep variant 2 only if it is neutral-or-better here AND (true, by
construction) it covers misses a smaller cache pays AND it adds no work to the faithful walk. The
third clause is the catch: variant 2 is by construction MORE instructions per entity in the walk
(it is the faithful walk; there is no "off" path, the prefetch rides the shipped profile). So read
"adds no work" as "no measurable cost here": neutral-or-better within noise in both towns. A
measured regression in either town (>= 0.01 ms/iter, same sign in both pairs) is a drop. If the
co-runner run is allowed and shows a gain there while this box is neutral, that is the evidence
for keeping it.

## 8. Doubts

- The vptr compare assumes a CharacterComponent / ModelComponent object's vptr is the compiler
  table's address point (true for these `final` / single-inheritance classes; `DEFINE_EXE_VTABLE`
  is the project's own accessor for exactly that, and the linker checks the symbol). A wrong vptr
  only means the type-specific stages never fire.
- Stage reads of non-ticking components (an uncached entity's first six) read their vptr: these
  are live objects retail's own loop dereferences on the same visit, so no new fault is possible.
- The 5-stage pipeline loses its lead on short visits (entities that tick nothing cost a few ns),
  so the effective distance in a run of non-ticking entities is shorter than one DRAM latency per
  stage. WALKCACHE made those rare in the tree (bit 2 is required to be in it).
- Unit tests do not exercise the walk (it is not in the host unit build); the variant is exact by
  construction (prefetch-only), so the stress shadows (`TickList`, COMPPROF=4) still apply
  unchanged and should stay at 0 mismatches in a variant-2 run.
