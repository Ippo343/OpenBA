# GATHERPF -- prefetch-ahead in the faithful scene gather (IBS_20261013 idea 4)

Block GATHERPF, 2026-10-13, branch `gatherpf` (base `23e418cd5`, main merged at `06a7c5886`). Faithful perf
work in the style of PREFETCH / PREFETCHCOV: hints only, the walk's visits, tests, visible list and draw items
unchanged, no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()` field, no `getenv`, no door.
**The game was not run by this block**; the coordinator runs the A/B (section 6).

**Short version.** Four `static constexpr bool` switches in `src/engine/graphics/gather_prefetch.h`, used by
`Graphics::Scene::AddCulledDrawItems` (`src/engine/graphics/cull.cpp`). Three ship ON (9 `prefetcht0` in the
perf object), one ships OFF. With all four off, the perf `cull.cpp` object of the arms commit (`bc0219737`) is
byte-identical (`objdump -dr`) to the same TU at `23e418cd5`. The `gather` COMPPROF row (commit `42dea67a6`)
adds two memory increments to the walk, in every leg alike.

| switch | default | what it prefetches | predicted heavy draw ms/frame |
|---|---|---|---|
| `kChain` (stride `kChainStride` = 1) | **ON** | ring object i+1's cull lines (2) and ring node i+2's line (1) | -0.03 (range -0.06 .. 0.00) |
| `kDispatch` (stride `kDispatchStride` = 1) | **ON** | visible entry i+2: vptr line, +0xc0 line; entry i+1 if a MeshInstance: +0x80, +0xd8, +0x11c | -0.02 (range -0.04 .. +0.01) |
| `kDispatchMaterials` (needs `kDispatch`) | **ON** | entry i+1's `_materials._data` first line | -0.005 (range -0.02 .. +0.01) |
| `kCellHead` | OFF | `&_cellHeads[cell]` before the node-box test | 0.00 +- 0.01 (not resolvable) |

## 1. Where the misses are, from the code

IBS_20261013 (heavy town, faithful, uncapped, main thread): `AddCulledDrawItems` 10.17% of ops, 25,958 load
samples, **40.62% L1 miss**, 4.31% miss latency; `MeshInstance::AddDrawItems` 4.16%, **19.05% miss**, 2.28%;
`PushVisible` 2.12%, 0.11% miss; `Renderer::AddDrawItem` 4.14%, 0.78% miss; `DrawItemArrayResize` 1.33%,
0.05% miss. So the misses are in the walk and in the slot-3 body it dispatches, not on the write side.

The walk has two phases. Allocations are 8-aligned (`MemAlloc(.., 8)`); "line" = 64 bytes.

### 1.1 Phase 1: the quadtree walk (per popped stack record, then per object of a surviving cell's ring)

| # | load | struct / offsets (static_asserts) | lines | depends on | miss? why |
|---|---|---|---|---|---|
| 1 | `st.rec[t]` (node, mask, box) | `CullStack` (cull.cpp), a 0x2430-byte local; record stride 0x24 | 1 | -- | no: the stack is L1 |
| 2 | `_cellCounts._data[cell]` | `SceneGrid` +0x50; `int32[0x5555]` = 87 KB | 1 | 1 | rarely: dense, L2-resident across the walks; the four children `4n+1..4n+4` share a line |
| 3 | `_cellHeads._data[cell]` | `SceneGrid` +0x30; `int32[0x5555]` = 87 KB | 1 | 1, read after the node-box frustum test | rarely, as 2 |
| 4 | `nodes[cur]._object`, `nodes[cur]._next` | `SceneNode` 0x18 bytes (+0x00 `_object`, +0x08 `_next`), `_nodes` +0x10, ~49k entries = 1.2 MB | 1 (the two words split in 1 of 8 entries: those at `24k % 64 == 56`) | 3 for the head; else the previous node's `_next` | **yes**: the ring is in insertion order (GridThread links at the tail), so consecutive objects are index-random in a 1.2 MB array; L2 or L3 |
| 5 | `geo->_materialTypeFlags`, `geo->_detailId` | `Geometry` +0x30, +0x28 | 1, or 2 when `geo % 64 == 0x10` (+0x28 at 0x38, +0x30 at 0x40) | 4 | **yes**: one scattered heap object per ring entry (~49k Geometry, ~10 MB with their arrays) |
| 6 | `geo->_worldBounds` (cull box; the shadow box is built from the same six floats) | `Geometry` +0x4c .. +0x63 | 1-2 | 4; only when 5 passes | **yes**, as 5 |
| 7 | `renderer->_detailId` | `Renderer` +0x48 | 1 | -- | no (hot) |
| 8 | `PushVisible`: `list.data[size]` store | the local `GeomList` | 1 | -- | no: sequential (0.11% miss) |

**The IBS note's "+0x28 and +0x4c", corrected.** The span the ring test reads is +0x28 .. +0x63, 60 bytes, so
it is on one line only when `geo % 64 == 0x18` and on two lines otherwise. The pair of points that names both
lines at every alignment is **+0x28 and +0x63** (or +0x60, the 4-aligned word `_worldBounds.max.z`). +0x4c is
on the second line only when `geo % 64` is 0x00 .. 0x10 or 0x38; at 0x20 / 0x28 / 0x30, +0x4c shares the
first line and the box's tail (+0x50 .. +0x63) is on the second, which a +0x4c prefetch would miss.

**Why 4-6 miss and the out-of-order core does not hide them.** It is a pointer chase: node i+1's index is only
known after node i's line arrives, and object i+1's address only after node i+1's. Between them sit the
per-object branches (flag test, detail test, frustum mask, the shadow arm), whose outcome is data-dependent;
each mispredict flushes the speculative loads of the next object, so the core rarely runs ahead by a whole
object. A prefetch issued at the top of object i's visit is older than all of those branches and survives
the flush.

The walk calls no virtual, so the Geometry vptr line (+0x00) is not read in phase 1 (it shares the +0x28 line
only when `geo % 64 < 0x18`).

### 1.2 Phase 2: the dispatch (per visible-list entry)

| # | load | offsets | lines | depends on | miss? why |
|---|---|---|---|---|---|
| 9 | `list.data[i]` | the local list | sequential | -- | no (hardware stream) |
| 10 | the vptr, then slot 3 of the vtable | `Geometry` +0x00; a handful of vtables | 1 + shared | 9 | **the vptr line yes**: not read by phase 1; the vtables are hot |
| 11 | `MeshInstance::AddDrawItems` (mesh.cpp:2138): `_mesh` +0xc0 (then the shared GraphicsMesh), `_worldBounds` / `_prevWorldBounds` +0x4c .. +0x7b (only when the mesh has more than one LOD level), `_animation._instanced` +0xd8 (and `_layers` +0xe0 .. when instanced), `_materials._data` +0xc8, `_sortBias` +0x11c, `_detailId` +0x28, `_opacity` / `_opacityPrev` +0x7c / +0x80 | `Mesh.h` static_asserts | every line of the 0x120-byte object except one wholly inside +0x84 .. +0xbf (the transform pair, read at emit, not here) | 10 | **yes**: the object's first touch since phase 1 touched +0x28 .. +0x63; between the two, the walk touched ~29k other objects (heavy, DRAWGATHER section 9), so even those lines are back in L3 |
| 12 | `_materials._data[lists[i]._f10]` | a per-instance heap array of 8-byte `ExternalPtr` | 1 | 11 (+0xc8) | **yes**: a second chase behind the object |
| 13 | `Renderer::AddDrawItem`: the group, the MaterialInstance / Material / VideoProgram chain, the item append | shared objects; `_items[pass]` sequential | -- | 12 | no: 0.78% / 0.05% miss in IBS |

## 2. The arms

All in `cull.cpp`; the helpers are `[[gnu::always_inline]]` (PREFETCH section 4: GCC classes an out-of-line
helper whose only effect is a prefetch as pure and deletes the call).

### Arm 1 `kChain` (ON): the ring, stride 1

At the top of object i's visit (before its branches): `ahead = node._next`; `an = nodes[ahead]`;
prefetch `&an._object->_detailId` (+0x28) and `&an._object->_worldBounds.max.z` (+0x60); prefetch
`&nodes[an._next]`. So object i+1's two Geometry lines and node i+2's line are in flight while object i is
tested, and node i+1's line was requested one visit earlier. This is PREFETCH's staging (one hop per stage,
stride 1) on a ring: the ring gives the next node for free and the stage before it pays for the one after.
`kChainStride` S > 1 hops S-1 more ring nodes (loads of nodes this cell's walk reads anyway); PREFETCH found
every stride above 1 worse (section 9.2: +3 / +4 / +8% at 2 / 4 / 8), so S stays 1 and is a constant only so
a sweep is repeatable on another CPU.

The first object of each cell is not covered (its node and object addresses come from `_cellHeads[cell]`,
read right before use; see arm 3 for why nothing earlier is allowed).

### Arm 2 `kDispatch` (ON): the visible list, stride 1, two stages

The list is an array of pointers, so lookahead costs no chase:

- **i+2S**: `list.data[i+2S]` -> prefetch the object (+0x00, the vptr line) and `geo + 1` (+0xc0: one past the
  0xC0-byte Geometry subobject, a pointer the language allows and that is never dereferenced; on a
  MeshInstance it is the line of `_mesh`, `_materials` and the head of `_animation`).
- **i+S**: `list.data[i+S]` -> load its vptr (the line was requested at i+2S; the virtual call loads the same
  word S iterations later) and compare it with the compiler's MeshInstance vtable address point
  (`_ZTVN8Graphics12MeshInstanceE` + 16, bound with `RT_ASM_LABEL`, as PREFETCH binds the components'
  tables). Equal means the dynamic type is exactly MeshInstance, so `static_cast` is exact; then prefetch
  `&_opacityPrev` (+0x80; +0x7c is on that line or on the box line phase 1 touched), `&_animation` (+0xd8),
  `&_sortBias` (+0x11c). Other types (lights, particles, status icons, WorldGeometry, EdgeMeshInstance) get
  only stage i+2S.

The two stages are one dispatch apart because a dispatch is long (MeshInstance::AddDrawItems + ~2.3
AddDrawItem calls per visible entry, DRAWGATHER's 10.8k visible vs DRAWSORT's 25.2k items per frame): one
stage of distance already exceeds an L3 latency. (DRAWGATHER's class A walk prefetches 4 entries ahead in
one stage; that was not A/B'd against 1.)

### Sub-arm `kDispatchMaterials` (ON): the materials array

At stage i+S, for a MeshInstance: load `_materials._data` (the +0xc8 line was requested at i+2S) and prefetch
its first line. Every triangle list of the entry indexes that array (row 12), and it is a separate heap
block. This is the shape of PREFETCHCOV's one winning arm (P: a dependent line behind a pointer in the object).
`_data` may be null (prefetching null is harmless); the element index needs the mesh's triangle lists, so
only the first line is named (meshes with more than 8 materials are rare).

### Arm 3 `kCellHead` (OFF)

After the faithful `_cellCounts[cell] >= 1` test, before the node-box test: prefetch `&_cellHeads._data[cell]`
(prefetch only; `_cellHeads._data` is a field of the live Scene the count test just indexed beside). OFF
because the two arrays are 87 KB each, stay in L2 across the ~3 walks per frame, siblings share a line, and
the head load already issues early whenever the box branch is predicted; there is nothing to hide on any
cache size. It exists so the coordinator can confirm that with one leg. Loading `_cellHeads[cell]` early to
reach the head NODE / OBJECT would be a load the faithful path performs only when the box test passes: not
built (the rule).

### Not built

- **The draw-item write side** (`Renderer::AddDrawItem`, `DrawItemArrayResize`): 0.78% / 0.05% L1 miss in IBS,
  a sequential append into a reused buffer that the hardware stream prefetcher covers.
- **The cull stack / `CullStackPushChildren`**: a local array, L1.
- **Scene::Clean** (one Geometry line per live node, 35% miss in IBS): not in the gather; a prefetch of
  `nodes[i+k]._object` there is the same pattern (DRAWGATHER's `ExtClean` does it at 8 ahead). A candidate for
  a follow-up block, not asked for here.

## 3. Exactness

- `prefetcht0` has no architectural effect and never faults. No value read for a prefetch reaches the walk,
  the visible list or a draw item.
- **Every load an arm adds is an earlier copy of a load the faithful code performs on the same address later in
  the same call**, on objects nothing frees during the call (the walk does not modify the Scene; the dispatch
  appends to the Renderer's lists only):
  - arm 1: `node._next` (read at the end of this visit), `nodes[next]._object` and `nodes[next]._next` (read on
    the next visit; when `next` wraps to the head they were read on the first). The ring is circular
    (GridThread), so every index reached is a ring member: no bound check is needed or added.
  - arm 2: `list.data[i+S]`, `list.data[i+2S]` (read S / 2S iterations later), bounded by the live
    `list.size` exactly as the loop is; the entry's vptr (the virtual call's load); `_materials._data` (read
    by MeshInstance::AddDrawItems whenever the mesh level has triangle lists; a field of the live object
    otherwise, never a fault).
  - arm 3: none (`_cellHeads._data` is a Scene field; the element is only prefetched).
- No faithful load or store is reordered, skipped or guarded; no null test was added on a faithful read.
- No shadow: there is nothing to compare (hints only). The existing COMPPROF=4 shadows (`FrustumCull`,
  `DrawItemSort`, the sim's) must stay at 0 mismatches in any leg, as before.

## 4. objdump evidence (perf flavour, `cull.cpp.obj`)

- **Arms off = the shipped code.** All four switches `false`, arms commit `bc0219737`: `objdump -dr` of the perf
  object vs the same TU built from `23e418cd5`'s `cull.cpp` with the identical command line: **0 lines differ**.
- **The row commit** (`42dea67a6`) adds, in every leg alike: `addq $1, g_gatherCells(%rip)` per walked cell,
  `addq $1, g_gatherChainObjects(%rip)` per ring object, one add per call for the visible count, register
  renames; stack references 158 vs HEAD's 158 (no new spills). The first version used local accumulators
  (FRUSTUMCULL's pattern) and cost 16 spill / reload instructions in the walk, so it was replaced. In
  `wave9_lane_gfx.cpp`, one `addq` per AddDrawItem append.
- **Defaults: 9 `prefetcht0`.** Arms on vs off, the only new instructions are the prefetches, their address
  arithmetic (`lea`, `movslq`) and these loads: in the walk `movslq 0x8(node)` (`_next`), `mov (nodes+24*next)`
  (`_object`), `movslq 0x8(nodes+24*next)` (`_next`); in the dispatch `list.data[i+2]`, `list.data[i+1]`,
  `cmp %r12,(entry)` (the vptr compare against the hoisted vtable address), `mov 0xc8(entry)` (`_materials._data`).
  The faithful `_next` read at the end of the visit is still there (not merged).
- **Per leg** (built and counted with `objdump -d cull.cpp.obj | grep -c prefetch`; every leg exit 0, 0 warnings
  in `cull.cpp`):

| leg | Chain | Dispatch | Materials | CellHead | prefetch count |
|---|---|---|---|---|---|
| off (the A/B base) | false | false | false | false | 0 |
| C | true | false | false | false | 3 |
| D (no materials) | false | true | false | false | 5 |
| DM | false | true | true | false | 6 |
| H | false | false | false | true | 1 |
| **default** (C + DM) | true | true | true | false | **9** |
| all | true | true | true | true | 10 |

The counts add exactly; `Materials` alone (with `Dispatch` false) compiles to nothing.

## 5. The row: `[COMPPROF]   gather (faithful walk)` (COMPPROF >= 1)

One line per window, printed after the `frustum cull` line, reset with the other counters, per DRAW frame:

```
[COMPPROF]   gather (faithful walk): cells walked/frame=N  chain objects/frame=N (x/cell)  visible/frame=N (x% of chain)  draw items appended/frame=N (x/visible)  passes/frame=x
```

- `cells walked` = cells whose ring was walked (head >= 0, after the box test); arm 1 runs once per chain
  object, arm 3 once per non-empty popped cell.
- `chain objects` = ring objects visited = Geometry headers the walk reads (arm 1's work count).
- `visible` = slot-3 dispatches (arm 2's work count).
- `draw items appended` = every `Renderer::AddDrawItem` append (all its callers are AddDrawItems overrides
  reached from a dispatch). It is the one counter that stays non-zero with `ext.draw_gather` ON.
- With `ext.draw_gather` ON the faithful body is never entered: cells / chain / visible read 0.

Every existing row and hook is unchanged. No shadow (hints only). No unit test: no helper worth testing was
added (the arms are a few inline prefetches; there is nothing to assert).

## 6. How to measure (coordinator)

- **Flip:** `src/engine/graphics/gather_prefetch.h`, the four `inline constexpr bool` lines; then
  `tools/build.sh build perf --jobs 2` (only `cull.cpp` recompiles) and check the prefetch count against
  section 4's table before copying the exe; record its md5.
- **Base leg = this branch with all four `false`**, not main: the row's two memory adds are in every leg.
- Faithful profile (`STRESS_EXT` unset), mods parked, idle machine (`machine load: CLEAN`; Afterburner, RTSS AND
  RTSSHooksLoader64 stopped, uncapped). Heavy = `--map large --citizens 1600 --sets 3 --spread --speed 10`;
  compact = the default town. Interleave `off C DM default H` twice per town (`D` only if `DM` and `C` disagree
  with `default`).
- **A/B column: draw ms/frame.** Also read fps. **Must not move:** sim ms/iter (no sim code touched), the resave
  golden (`tools/resave_gate/resave_gate.sh Gate`; nothing serialized is touched), COMPPROF=4 shadows at 0.
- One COMPPROF=1 run of the default leg (heavy) for the `gather` row: it sizes each arm's work (chain objects,
  visible) and lets a draw-ms delta be read per object.
- **One everything-on pair** (`STRESS_EXT` = the everything-on set, default vs off): `ext.draw_gather` replaces
  the whole walk at the top of `AddCulledDrawItems`, so **no prefetch of this block is reached**; the pair
  should be identical within noise, and the row's cells / chain / visible should read 0 (draw items non-zero).
- Keep rule (PREFETCH section 7 / PREFETCHCOV): keep an arm only if neutral-or-better in both towns; a
  same-sign regression in both pairs is a drop.
- If the co-runner is allowed (PREFETCH section 6, a 256 MB+ streaming process on two other cores, label
  `pressure`): `off` vs `default` under it is the only way this box shows the small-L3 case (section 8).

## 7. Prediction (draw ms/frame; heavy ~1.93-2.04, compact ~1.28-1.37 with the merged blocks)

The frame for it: `ext.draw_gather` removed ~60% of the walk's Geometry touches outright, prefetched the
dispatch, the Clean pass and dropped the per-call 128 KB alloc, for **-0.11 heavy / -0.07 compact** (DRAWGATHER
section 9). This block only overlaps misses, does not remove them, and adds work (the chain arm: 3 loads +
3 prefetches per ring object over ~29k objects per frame; the dispatch: ~3 loads + up to 6 prefetches per
visible entry over ~10.8k).

| leg | heavy | compact |
|---|---|---|
| C | -0.03 (-0.06 .. 0.00) | -0.02 (-0.04 .. +0.01) |
| D | -0.015 (-0.03 .. +0.01) | -0.01 (-0.02 .. +0.01) |
| DM | -0.02 (-0.04 .. +0.01) | -0.01 (-0.03 .. +0.01) |
| H | 0.00 +- 0.01 | 0.00 +- 0.01 |
| default (C + DM) | **-0.05 (-0.09 .. -0.01), about -2.5%** | **-0.03 (-0.05 .. 0.00)** |
| everything-on pair | 0.00 (not reached) | -- |

sim ms/iter: unchanged in every leg. Resave golden: unchanged.

## 8. Doubts

- **Fill-buffer pressure (the PREFETCHCOV lesson).** Arm C there regressed both towns by +3..+4% with ~8 extra
  prefetches per visit. Here the chain arm adds 3 per ring object and the dispatch up to 6 per visible entry;
  the ring object visit is short (a flag test and one or two box tests, ~30-60 ns), so three in-flight lines
  per visit is near what the fill buffers sustain beside the walk's own demand misses. If C regresses, a narrower
  C (only the object's two lines, no node i+2) is the next thing to try; it is not built.
- **The chain arm's dependent load.** `nodes[next]._object` must arrive before its prefetches can issue; if that
  node line is not yet in (it was only prefetched one visit earlier), the load stalls retirement exactly where
  the faithful next visit would have stalled anyway, so the expected cost is the extra instructions, not a new
  stall.
- **The small-L3 argument.** On this 96 MB X3D everything the gather touches stays L3-resident (~1.2 MB of nodes,
  ~6 MB of Geometry lines per walk), so every hint hides an ~11 ns L3 hit at best. On a 16-32 MB part with the
  sim's 5-10 MB set (PREFETCH section 5) the Geometry lines go to DRAM (70-90 ns); one ring visit of distance
  hides only part of that, and a stride of 2 might pay there while it costs here: the constant is kept for
  that sweep. Never assume this box's L3 for the verdict (`feedback_dont_assume_x3d_l3_for_perf_verdicts`).
- **The compact-town risk.** Fewer objects per ring and a smaller visible list: the per-cell head (not covered)
  is a larger share and the arms' fixed cost a larger fraction; compact is where a regression would show first.
- **Wasted dispatch fills.** `geo + 1` on a 0xC0- or 0xE0-byte geometry names the next heap block (a wasted
  fill); the MeshInstance stage does not fire for EdgeMeshInstance (roads, fields), whose own reads are not
  covered.
- **Nested gathers.** A dispatch can re-enter the gather (`MaterialInstance::RenderTextures`), which may evict
  the lines prefetched for i+1 / i+2; rare (render-to-texture materials only).
- **The row costs two memory adds per object in every build**, also in non-COMPPROF runs (as FRUSTUMCULL's
  counter does per call). ~29k + a few thousand adds per frame, no new spills; it is in every A/B leg alike.

## 9. Files

| file | change |
|---|---|
| `src/engine/graphics/gather_prefetch.h` | new: the four switches, the two strides, the counters' declarations |
| `src/engine/graphics/cull.cpp` | the arms (`PrefetchCullLines`, `PrefetchChainAhead`, `PrefetchDispatchFar`, `PrefetchDispatchNear`, all always_inline), the MeshInstance vtable binding, the counters' definitions and adds |
| `src/engine/graphics/wave9_lane_gfx.cpp` | `++g_gatherDrawItems` after the append in `Renderer::AddDrawItem` |
| `src/diag/stress_harness.cpp` | `GatherReport` and its reset |

## 10. Static gate (this block; on the merged tree, main `06a7c5886`)

- `tools/build.sh build perf --jobs 2`: exit 0 (defaults, and each leg of section 4); 0 warnings in the touched
  files.
- `tools/build.sh build debug --jobs 2`: exit 0; 0 warnings in the touched files.
- `tools/build.sh unit perf --jobs 2`: **1143 tests, 0 failures** (no test added).
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2** (one new cast, the vptr read, marked `CAST-OK: vslot` as
  PREFETCH's are).
- Not run: the game, the stress town, the resave gate, COMPPROF (the coordinator's).
