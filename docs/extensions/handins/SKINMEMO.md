# SKINMEMO -- the bone matrices computed once per key per Renderer::Draw

Block SKINMEMO, 2026-10-13. This is faithful perf work in the style of FRUSTUMCULL / STILLTERRAIN. It adds
no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()` field, no `getenv` and no door.
The kill switch is `static constexpr bool SkinMemo::kEnabled = true` (`src/engine/graphics/skin_memo.h`).
**The game was not run by this block.** The coordinator runs the COMPPROF=4 shadow (`SkinBones`),
the COMPPROF>=1 row (`/skin`) and the A/B.

**Short version.** The renderer asks `MeshInstance::GetBones` for the same bones several times
per frame, with bit-identical inputs. It asks once per triangle list of the mesh, once per
pass, and once per instancing batch. The passes are the sun's shadow map, then `DrawLight` for
each light (ambient, sun) in each of the three lists, plus the water reflection's scene
group when one is visible. Inside one `Renderer::Draw` nothing writes any of GetBones' inputs.
The memo keys every input by identity and bits: the pose array, the bind array, count,
instanced, the root bits and the t bits. A fixed slot table holds each key's `count` matrices.
`SetBones` is handed the slot, and it copies exactly `count` matrices, so the Context gets the
same bytes in the same calls in the same order. The key covers both candidate 1 (instances
that share one instanced state and mesh) and the repeat of one instance across passes.
Candidates 2 and 4 stop at the analysis (sections 2.2, 2.4). Candidate 3 (the zeroing) comes
free on the memo path (section 2.3).

## 1. The proof

### 1.1 What GetBones reads (`animationstate_transforms.cpp:701`, retail 0x180100070)

| input | where | in the key |
|---|---|---|
| `_mesh.Get()->GetNodes()`: `_size` (= count) and each node's `_bind` | :703-705, the loop | `binds` = the node array's `_data` pointer, `count` |
| `_animation._instanced` | :711, :731 | `instanced` |
| the root: identity when instanced (:712-715), else `rootRef.Inverse()` (:717, pure, `transform.cpp:274`) | | `root[7]` = the caller's root bits when not instanced; all 0 when instanced (GetBones ignores it there) |
| the pose array: `AnimationManager::_singleton->_instances[(h>>8)&0xff]._states[h&0xff]._nodes._data` when instanced (:732-736, `h` = the last layer's `_handle`), else `_nodes._data` (:738), and its first `count` records | | `interp` = that pointer (`skin_memo.cpp` `PoseArray` is a literal copy of the selection) |
| `t` | the lerp, `Quaternion::Lerp` | `t` bits |
| MXCSR | all the arithmetic | fixed for the process (STILLPATH section 8) |

Everything else is pure. `Quaternion::Lerp` (`wave9_lane_p.cpp:79`), `operator*`, `Transform::AsMatrix`
(`transform.cpp:211`) and `Inverse` read only their operands and have no static state. GetBones writes
only `out[0 .. count)` (:903) and returns `count`.

**Claim.** On a hit, the matrices in the slot are bit-identical to what GetBones would write now.

*Proof.* A hit means the slot holds this exact key (a stored 56-byte compare, not a hash
match) and was claimed in the current epoch E. The epoch moves at every `Renderer::Draw` entry
and exit (`SkinMemo::Scope`, `wave27_lane_gfxbig.cpp:976`), so the claim and the hit fall inside
one `Renderer::Draw`. At the claim, GetBones ran on the same pointers, the same count, the same
instanced byte, the same root bits and the same t bits, and wrote the slot's matrices. The bind
and pose arrays are the same objects at the same addresses (1.3), and nothing wrote their
contents in between (1.2). So the inputs are bit-identical and the outputs are too. ∎

### 1.2 The census: writers of the inputs, and why none runs inside a Renderer::Draw

| input | writers (all of them, by grep of `_nodes`, `_states`, `_instances`, the Array SetSize / Assign / Dtor instantiations) | where they run |
|---|---|---|
| a unique pose array (`MeshInstance::_nodes`, `Mesh.h:494`) | `AnimationState::Update` -> `UpdateNodes` / `CopyInstancedToUnique` (`wave27_lane_gfxbig.cpp:800/807`), called from `MeshInstance::Update` (`wave29_lane_slots.cpp:231`); `MeshInstance::UpdateHierarchy` (`mesh.cpp:1937`, from Update and SetMesh); `SetMesh` (`mesh.cpp:2013`, SetSize :2030); `PlayAnimation` (`mesh.cpp:2055`); ctor / copy ctor / dtor (`mesh.cpp:685/759`); `Serialize` (`wave24_lane_gfxmat.cpp:486`) | the game update (component ticks, the model / citizen tick, render_culling's pause flush), entity create / destroy, load. All of these are non-const members. Every Geometry slot reached from `Renderer::Draw` is const: AddDrawItems, GetInstanceTransform, SetSharedInstanceData and Draw. |
| a shared instanced state (`InstancedAnimationState::_nodes`) and the `_instances` / `_states` arrays | `AnimationManager::Update` (`wave23_lane_gfxbody.cpp:489`, per sim tick); `AnimationManager::GetInstanceId` (`animationstate_transforms.cpp:924`: grows `_instances`, bakes states; reached only from `AnimationState::PlayAnimation` :271); `AnimationManager::Serialize` (`wave24_lane_gfxmat.cpp:543`); `animationstate.cpp:459` (teardown) | the game update and load. Never reached from the render. |
| the bind array (`GraphicsMesh::_nodes`, `GraphicsMesh.h:548`) | `GraphicsMesh::Serialize` (`graphicsmesh_vtable.cpp:522`), the dtor (:269) | resource load / unload. Nothing reloads a resource inside a draw. |
| `_animation._instanced`, the layers and their handle | `AnimationState::PlayAnimation` / `Update` / Serialize | as above |

**What runs inside `Renderer::Draw`** (`wave27_lane_gfxbig.cpp:963`), checked call by call:
- `Reset`, `AddScene`, `BeginDrawGroup`.
- Each global Geometry's `AddDrawItems` (const) and `Scene::AddCulledDrawItems` (`cull.cpp`). The
  ext.draw_gather variant also calls `Geometry::Clean` (`scene.cpp:1160`, the only override),
  which writes bounds and scene-grid state, never animation state.
- `Renderer::AddDrawItem` -> **`MaterialInstance::RenderTextures`** (`wave9_lane_gfx.cpp:295/406`)
  -> `GeneratedImageBuffer::Render` slot 11. Its two overrides are
  `FrameCopyImageBuffer::Render` (empty, `imagebuffer_derived.cpp:290`) and
  `ReflectionImageBuffer::Render` (:609). The latter only appends a `RendererSceneEntry` (the
  mirrored camera) to `_sceneGroups`, which adds more draws of the same instances (more hits),
  not a writer.
- The draw-item sort.
- Pass 2: the lights' `DrawShadowMap` / `DrawLight` (`lights_vtable.cpp:338/482/535/592/660`) ->
  `DrawItemList` (`batchc_gfx_leaves.cpp:689`). That runs `MaterialInstance::GenerateTextures`
  (slot 10: `ReflectionImageBuffer::Generate` is an empty body, `FrameCopyImageBuffer::Generate`
  is a GPU blit), `MaterialInstance::Setup`, Geometry `SetSharedInstanceData` (:757),
  `GetInstanceTransform` and `Draw` (:798), and the Context setters.

None of these reaches a writer in the table, allocates, or frees a pose, state or bind array.
The coordinator's condition 1 is answered: RenderTextures is in the list above. No draw path
calls `AnimationState::Update`, `MeshInstance::Update` or `CopyInstancedToUnique`; their only
callers are `MeshInstance::Update` (slot 1, game update), `AnimationManager::Update` and
`GetInstanceId` / `PlayAnimation`.

### 1.3 Pointer keys: no reuse inside a scope

A pointer key is exact only if the pointee cannot change or be reused within the scope. Arrays
are freed or reallocated only by the writers in 1.2 (SetSize / Assign / Dtor of `_nodes`,
`_states` and `_instances`, the mesh and instance dtors), and none runs in a scope. So a pose
or bind address seen in a scope names one live array for the whole scope. Across scopes the
epoch empties the table, so a freed array whose address is reused next frame can only miss.
The unit test reuses addresses between scopes on purpose (section 6).

### 1.4 Threads

Every `Renderer::Draw` caller is a Draw-phase function on the thread that owns the
`Video::Context`: `GameUpdateLoop::Draw`, the menu loop's Draw, the minimap capture
(`minimap_paint.cpp:211`, terrain only) and ext.eye_camera. The scope state is plain globals,
like FRUSTUMCULL's counters. One doubt (section 8): a loader thread writing animation state
while the main thread draws the loading screen would race in retail too.

### 1.5 What stays exactly as before

- Outside a scope, with count <= 0, or with count above the caller's buffer (48 in the two
  Draws, 47 in SetSharedInstanceData), the memo declines and retail's arm runs: the zeroed
  stack buffer, `GetBones`, `SetBones`. A count above the buffer overflows it in retail, and it
  still does.
- The call order is unchanged: `LerpTransform`, then the bones, then `SetBones`, then
  `SetTransformScale`. GetBones has no side effects, so computing it at the claim instead of at
  each request is invisible.
- No guard was added. The key reads `_mesh.Get()->GetNodes()` and the handle chain exactly as
  GetBones does, so a null crashes at the same place.

## 2. The candidates

### 2.1 Candidate 1 (instanced-state sharing): BUILT, together with the per-instance repeat

`t` is `Renderer::_alphaT`, written once at `Renderer::Draw` entry (`wave27_lane_gfxbig.cpp`, after
`Reset`) and read by every `DrawItemList` (`batchc_gfx_leaves.cpp:758/771/802`). That includes the
shadow passes, which run `DrawItemList` on the same Renderer inside the same Draw. So within a
scope every draw has the same `t`, and the key's t bits are a formality. When the instance is
instanced, GetBones uses the identity root and the shared state, so instances that share
(handle, mesh) share a key. The batching path (`SetSharedInstanceData`, identity root) and the
single-item path (`Draw` with the instance's own root, which GetBones ignores) also share it.

The bigger multiplier is not sharing between instances. It is **repetition**. `Draw` is
called with `pass` = the batch count, and the pose block runs when that is 1 (see the banner in
`meshinstance_draw.cpp`). A unique citizen is therefore posed once per triangle list per
`DrawItemList` that lists it: the sun's shadow map, then `DrawLight` x 3 lists x 2 lights
wherever its material has those passes, plus the reflection group. The `/skin` row shows the
real ratio (`req/key` per class).

### 2.2 Candidate 2 (still unique instances across frames): CLOSED, not exact

When `_previous == _current` bit for bit, the translation lerp `s + (f - s) * t` is `s` for every
finite t >= +0, except that -0 becomes +0. **The rotation is not**: `Quaternion::Lerp(a, a, t)`
(`wave9_lane_p.cpp:79`) computes `r = a*(1-t) + a*t` per component and then divides by `|r|`. Both
the sum and the norm round differently for different t, so the bones depend on t, and t
(the frame's interpolation factor) changes every frame. A cross-frame memo keyed on the node
state therefore cannot be bit-exact. `skin_memo_still_lerp_depends_on_t` finds such an `a`
with the real Lerp. A class A version (reuse last frame's bones for a still pose) would change
the uploaded bytes by ULPs and is not built here. For the record, the `/skin` row's
`all-still keys/frame` counts unique pose arrays that are all-still at their miss (profile only).

### 2.3 Candidate 3 (the 48-matrix zeroing): removed on the memo path, exact

Retail zeroes its stack buffer: `Matrix()` x 48 in `Draw` (the `dec/jns` loop, see the banner),
x 48 in EdgeMeshInstance::Draw, and x 47 in SetSharedInstanceData. `Context::SetBones`
(`context.cpp:389`) copies exactly `count` matrices into `_boneAndProjectionStaging`, and
nothing else reads the stack buffer. The GPU upload (`contextdata.cpp:774`) binds the whole
0xcc0-byte staging area, but SetBones writes only `[0, count)` of it, the same range as before.
The zeroed tail never reaches the Context. On the memo path (hit or miss) there is no stack
buffer at all: GetBones writes straight into the slot. On a decline, retail's arm with its
zeroing runs unchanged. I kept that arm literal rather than add a second, unzeroed retail arm.

### 2.4 Candidate 4 (SIMD of the per-bone arithmetic): not built

It could be exact. `CMakeLists.txt` passes no `-ffp-contract` and no `-mfma`, so x86-64 baseline
GCC cannot contract. Packing four bones into the four lanes and replaying GetBones' scalar
schedule lane-wise (the same `mulss` / `addss` order, `sqrtss` -> `sqrtps`, `divss` -> `divps`)
gives the same bits per lane. But after the memo only the misses run GetBones, one per key per
frame (section 4). The body is ~200 hand-scheduled float statements across four different
schedules, which is easy to get wrong. The ceiling is the miss cost (the row's `miss ms/frame`),
and with the coordinator's sampled share that is well under 0.1 ms/frame. Not worth it.

## 3. Design

- `skin_memo.h`: `Key` (56 bytes, no padding: interp, binds, count, instanced, t bits, root bits),
  `HashKey` (`std::hash<const void*>` for the pointers, no cast), `Table`: **4096 direct-mapped
  slots** (`kSlots`), each holding a stored key, its epoch and a fixed region of `kMaxBones` = 48
  matrices. A different key that hashes to a live slot **evicts** it (counted). That bounds the
  memory: 4096 x 72 B of keys plus 4096 x 3 KB of regions = 12.9 MB reserved on first use, of
  which only `count` matrices per claimed slot are ever touched. This is the coordinator's
  condition 2. `g_epoch` / `g_depth`, `Scope` (RAII: Begin at entry, End at exit, both bump the
  epoch), the counters, and the harness hooks `g_prof` / `g_verify` / `g_report`.
- `skin_memo.cpp`: `SkinMemo::Bones(src, t, root, capacity, &count)`. Decline -> nullptr. Else
  it builds the key and looks up the slot: on a hit it returns the region; on a miss it claims
  the slot and calls `src.MeshInstance::GetBones(t, root, region)`.
- The callers (`meshinstance_draw.cpp:275`, `:384`; `wave29_lane_slots.cpp:435`):
  `memo ? SetBones(count, memo) : <retail arm>`.
- `Renderer::Draw` holds `const SkinMemo::Scope skinMemoScope;` for its whole body
  (`wave27_lane_gfxbig.cpp:976`). The body has no early return. Nested draws are not known,
  but they would be exact anyway: every Begin and End bumps the epoch.

The faithful-path cost when `kEnabled = false`: none. The Scope is empty and the callers
compile to retail's arm.

## 4. Kill switch, shadow and row

- **Kill switch**: `SkinMemo::kEnabled` (`skin_memo.h`). Set it to false for the A/B.
- **Shadow `SkinBones`** (`BANISHED_STRESS_COMPPROF=4`): on every hit, GetBones runs with the
  faithful body on the live inputs into a scratch buffer, and **all `count` matrices** are
  compared with the slot's bit for bit. The game is then handed the reference. detail =
  `(count differs) << 16 | first differing bone` (0xffff = none). **Expected: 0 mismatches.**
  A writer missing from the census (1.2) would show up here. Read it as
  `[VERIFY] SkinBones checks=N mismatches=M`. The FrustumCull, StillTerrain and other shadows
  must stay at 0.
- **Row `/skin`** (`BANISHED_STRESS_COMPPROF>=1`, after `/part`):
  ```
  [COMPPROF]   /skin bone memo (SKINMEMO) requests/frame=R hits=H (h%) scopes/frame=S declines/frame=D;
    unique: Ru req Ku keys (Ru/Ku req/key, all-still A keys/frame); instanced: Ri req Ki keys (Ri/Ki req/key);
    evictions/frame=E; bones computed/frame=B saved/frame=V; miss X ms/frame Y ns/bone; hit Z ms/frame W ns/call;
    est. saved M ms/frame
  ```
  How to read it:
  - `req/key` per class is the candidate's real multiplier. `instanced` keys are the distinct
    (shared state, mesh) pairs, which is the count the brief asked for.
  - `evictions/frame` should be small next to the keys. If it is not, raise `kSlots`.
  - `declines/frame` counts requests outside a scope or over capacity, and should be ~0.
  - `est. saved` = (miss ns per bone x bones saved) - the hits' own time. It is indicative,
    because RDTSC per call is inside the profile.
  - The authoritative number is the unprofiled A/B on draw ms/frame with RTSS stopped:
    `kEnabled` false vs true, interleaved, heavy and compact.

## 5. Prediction

The coordinator's sample (heavy, HEAD perf exe, 139 fps) puts Lerp + AsMatrix + GetBones self at
~2.1% of all samples (~4-5% of the frame's real CPU). The 15.5% IBS op share overstates it.
Assume a request multiplicity of k = requests / keys. From the pass structure, k >= 3 for a
unique citizen (shadow map + ambient + sun) and higher with several triangle lists or a visible
reflection. The memo removes (k-1)/k of the GetBones work and adds per hit a key build, a
56-byte compare and a count x 64-byte copy in SetBones (the copy was already there). It also
removes the 3 KB stack zeroing from every memo-path request, hits and misses alike.

- **Faithful profile (render_culling off), heavy town: ~0.08-0.15 ms/frame of the 2.2 ms draw
  CPU (4-7% of draw, ~1-2% of the frame).** That is 2/3 of the ~0.15 ms skinning, plus the
  zeroing (~30-60 ns per request x the request count; the row's `requests/frame` gives the
  count).
- **Main-thread op share**: the skinning group's 15.5% of IBS ops should drop to ~5-6%,
  because only the misses remain.
- **Everything-on**: the same per drawn instance. render_culling only skips off-screen
  updates, not draws.
- Candidate 1 alone (instanced sharing across instances): its share is the `instanced` part of
  the row. If trees / crops are not instanced-animated, or are batched already
  (`SetSharedInstanceData` poses once per batch), it is small. The repeat across passes is
  most of the gain.
- Candidate 2: 0 (closed). Candidate 3: included above. Candidate 4: not built (< 0.05
  ms/frame left).

## 6. Files

| file | change |
|---|---|
| `src/engine/graphics/skin_memo.h` (new) | kill switch, Key, Table, scope, counters, hooks |
| `src/engine/graphics/skin_memo.cpp` (new) | `SkinMemo::Bones` (the memo entry; the shadow; the still census) |
| `src/engine/graphics/meshinstance_draw.cpp` | the skinned arms of `MeshInstance::Draw` and `EdgeMeshInstance::Draw` |
| `src/engine/graphics/wave29_lane_slots.cpp` | `MeshInstance::SetSharedInstanceData` |
| `src/engine/graphics/wave27_lane_gfxbig.cpp` | `Renderer::Draw` holds the scope |
| `src/diag/stress_harness.cpp` | the `/skin` row, its reset per window, the `SkinBones` hook, `g_prof` |
| `tests/unit/skin_memo_test.cpp` (new) | 4 tests |

## 7. Build results (this block, static gate only)

- `tools/build.sh build perf --jobs 2`: OK. `build debug --jobs 2`: OK. 0 warnings in the touched
  files (the perf build's `Attend` unused-parameter warning at `wave27_lane_gfxbig.cpp:144` was
  there before: log stripping).
- `tools/build.sh unit perf --jobs 2`: **1134 tests, 0 failures** (4 new):
  - `skin_memo_table_slots_and_epochs`: the stored-key compare (a key that differs only in a
    -0.0 root bit misses), the scope epoch empties the table with no clear, a collision evicts.
  - **`skin_memo_two_renderers_bit_identical`**: two scripted renderers over a fixed pool of
    buffers. Arrays are rewritten and reused **at the same address** between scopes, instances
    are re-pointed and change instanced state, the root varies three ways per instance, and the
    effective table is cut to 64 slots to force evictions. 6 seeds x 400 scopes: 720,000
    requests, 400,333 memo hits, 225,744 evictions, **0 mismatches** over all `count` matrices.
  - **`skin_memo_write_inside_scope_diverges`**: a pose write inside a scope (the census claim
    broken) diverges, so the test can tell.
  - `skin_memo_still_lerp_depends_on_t`: candidate 2's counterexample with the real Lerp.
- `python tools/abi_audit.py`: OK. `tools/acceptance/acceptance.sh`: 0 lines off target.
  `python tools/castmetric.py src`: **total 2** (the first draft hashed the pointers with
  `reinterpret_cast`; that became `std::hash<const void*>`).
- Not run: the game, the stress town, the shadow, the resave gate. The worktree needed
  `git submodule update --init third_party/SDL` before CMake would configure.

## 8. Doubts

- **The census rests on the render paths reading animation state through const members.** A
  future draw-time writer would break the claim silently in the build, and show up only as a
  `SkinBones` mismatch. The unit test shows such a writer does diverge.
- **The loading screen.** If a loader thread runs `PlayAnimation` / `GetInstanceId` (which grows
  `_instances`) while the main thread draws a scene with instanced meshes, retail races already.
  The memo could carry a torn value to the rest of that one Draw. Not seen; noted.
- **The key copies GetBones' pose-array selection** (`PoseArray`). If GetBones' selection
  changes, the copy must change too. The shadow would catch a difference (a hit whose bytes
  differ).
- **Memory**: 12.9 MB reserved on first skinned draw (the regions; the vector value-initialises
  them, so they are committed). `kSlots` = 2048 would halve it at the cost of more evictions.
  The row says which.
- **The verdict is the coordinator's A/B.** If `req/key` is close to 1 (one pass per instance,
  for example with shadows off), the memo costs a key build and a lookup per request for
  nothing. The row shows that, and the kill switch takes it out.
