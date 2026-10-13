# XFORM -- the per-draw-item world matrix computed once per key per Renderer::Draw emit pass

Block XFORM, 2026-10-13 (IBS_20261013 section 3, idea 1). This is faithful perf work in the style of SKINMEMO.
It adds no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()` field, no `getenv` and no door.
The kill switch is `static constexpr bool XformMemo::kEnabled = true` (`src/engine/graphics/xform_memo.h`).
**The game was not run by this block.** The coordinator runs the COMPPROF=4 shadow (`ItemTransform`),
the COMPPROF>=1 row (`/xform`) and the A/B.

**Short version.** Every per-item world transform the renderer emits is `AsMatrix` of a lerp of ONE
`InterpolatedState<Math::Transform>` pair: a Geometry's `_transform`, a MeshInstance's `_nodes[node]`, or a
shared instanced pose's `_nodes[node]`. Eight sites spell that arithmetic (section 1.1), and every spelling is
the same IEEE operations on the same operands. Inside the emit pass of one `Renderer::Draw` (pass 2: the
lights' `DrawShadowMap` / `DrawLight` -> `DrawItemList`), `t` is `Renderer::_alphaT` and nothing writes or
frees a pair (section 1.2). Yet one pair is asked for once per triangle list, once per pass (the sun's shadow
map, then `DrawLight` per light and list), once per instance sharing a pose, and once per terrain chunk item.
The memo keys the 64-byte matrix on (the pair's address, t's bits). Each site keeps its retail lerp +
AsMatrix verbatim inside a lambda; a miss runs the lambda into the slot, a hit copies the slot. The site then
does exactly what it did before with those 64 bytes (the column stores, `SetTransform`, `SetTransformScale`),
so the Context gets the same bytes in the same calls in the same order.

## 1. The proof

### 1.1 What the sites read, and the five spellings

Every site is a const member. What feeds the memo is the pair and `t`; everything else the site reads
(the mesh, the triangle list's `_f14`, `_animation`, `_color`, `_opacity`, the pool handle) is read by the
site exactly as before, around the memo, and is not part of the cached value.

| site (`XformMemo::Site`) | where | pair | retail spelling |
|---|---|---|---|
| `kMeshGit` | `MeshInstance::GetInstanceTransform` (`mesh.cpp:2123`, retail 0x180101280), all three arms | `&_transform` (node < 0), `&_nodes._data[node]`, or `&RtInstancedPoolSlot(this)->_nodes._data[node]` (the selection is retail's, unchanged) | `RtLerpTransformPair` then `xf.AsMatrix()`; then the 12 matrix stores + 4 `_color` stores, kept |
| `kEdgeGit` | `EdgeMeshInstance::GetInstanceTransform` (`mesh.cpp:2293`, 0x1800a3840), the own-transform arm | `&_transform` | `RtLerpTransformPair` + `AsMatrix`. The `GetNodeToWorld` arm is NOT memoised |
| `kMeshDraw` | `MeshInstance::Draw` pass == 1 pose block (`meshinstance_draw.cpp:302`, 0x180100960), both arms | `&_transform` (skinned) or `&base[boneNode]` (rigid; base = the instanced pool or `_nodes._data`, retail's selection) | `LerpTransform` (`dx = (c-p)*t; p + dx`) then `world.AsMatrix()` |
| `kEdgeDraw` | `EdgeMeshInstance::Draw` (`meshinstance_draw.cpp:427`, 0x1800a3150), the skinned and the standalone arms | `&_transform` | `LerpTransform` + `AsMatrix`. The `GetNodeToWorld` arm is NOT memoised |
| `kGeomGit` | `Geometry::GetInstanceTransform` (`scene.cpp:1273`, 0x1800154a0) | `&_transform` | `px + (cx - px) * t`, `RtQuaternionLerp` (= `from->Lerp(*to, *t)`), `RtTransformAsMatrix` (= `xf->AsMatrix()`) |
| `kPartGit` | `ParticleInstance::GetInstanceTransform` (`wave27_lane_gfxpart.cpp:356`, 0x1801047f0) | `&_transform` | `(c - p) * t + p`, `prv.rotation.Lerp(cur.rotation, t)`, `AsMatrix`; then the w-column stores, kept |
| `kPartDraw` | `ParticleInstance::Draw`, the `instances <= 1` block (`wave27_lane_gfxpart.cpp:747`) | `&_transform` | same as `kPartGit` |
| `kWorldDraw` | `WorldGeometry::Draw` (`src/game/Game/geometry_draw.cpp:350`, exe 0x1400AEA10), the terrain chunk items | `&_transform` | `_transform.Lerp(Scalar(t))` = `Transform::Lerp` (`ax + tv * (cx - ax)`) then `AsMatrix` |

**Claim A (one function).** For every site, the 64 bytes are `M(pair, t) = AsMatrix({ prev.rotation.Lerp(
cur.rotation, Scalar(t)), (p + (c - p) * t) per axis })`, bit for bit.

*Proof.* The rotation is the same out-of-line member call `Quaternion::Lerp` (`wave9_lane_p.cpp`) on the same
operands (`this` = the previous rotation, the argument = the current one) in every spelling, including the
two forwarders `RtQuaternionLerp` and `InterpolatedState<Transform>::Lerp` -> `Transform::Lerp`. The matrix is
the same out-of-line member `Transform::AsMatrix` (`transform.cpp:211`) everywhere (`RtTransformAsMatrix` is
`*ret = xf->AsMatrix()`). Each translation axis is one `subss`, one `mulss`, one `addss` on the same three
floats, each rounded once. The spellings differ only in operand order of the multiply (`tv * d` vs `d * t`)
and of the add (`p + x` vs `x + p`), and IEEE-754 multiply and add are commutative, exactly. The build passes
no `-ffast-math`, no `-march` / `-mfma` (`CMakeLists.txt`: x86-64 baseline SSE2), so GCC can neither
reassociate nor contract to FMA, at -O2 or -O3. MXCSR is fixed for the process (STILLPATH section 8). The
unit test `xform_memo_site_spellings_agree` runs all five spellings (four literal copies, the fifth the
real `InterpolatedState::Lerp`) on 80,000 cases including signed zeros, denormals, 1e30 magnitudes and
t = 0 / 1: 0 differences. The COMPPROF=4 shadow checks the live sites (section 4). ∎

**Claim B (the memo).** On a hit, the slot's 64 bytes are bit-identical to what the requesting site's retail
spelling would produce now.

*Proof.* A hit means the slot holds this exact key (a stored 16-byte compare: the pair address and t's bits,
not a hash match) claimed in the current epoch E. The epoch moves when pass 2 of `Renderer::Draw` begins and
when the body ends (`XformMemo::Scope`, `wave27_lane_gfxbig.cpp:1037`), so the claim and the hit fall inside
one emit pass. At the claim, some site's retail spelling ran on this pair's bytes and these t bits and wrote
the slot. The pair is the same object at the same address (1.3) and nothing wrote it in between (1.2), so its
56 bytes are unchanged; by claim A every site's spelling maps them to the same 64 bytes. ∎

### 1.2 The census: writers of the pairs, and why none runs inside the emit pass

| pair | writers (all of them, by grep of `_transform`, `SetTransform`, `_nodes`, `_states`, `_instances`, `Clean`, `UpdateAttached`) | where they run |
|---|---|---|
| `Geometry::_transform` (MeshInstance, EdgeMeshInstance, ParticleInstance, base, WorldGeometry) | `Geometry::SetTransform` (`scene.cpp:1054`; callers: the game's components, actions, tools, start town and the menu loop's load phases; `EdgeMeshInstance::Update`; `Attach`; ext.perfx `perfx.cpp:112`); `Geometry::UpdateAttached` (`scene.cpp:1110`, the child roll; from `MeshInstance::Update` and SetTransform); **`Geometry::Clean`** (`scene.cpp:1161`, rolls `_previous` on the bit-8 path; from `Scene::Clean` `scene.cpp:1424`, ext.draw_gather's `ExtClean` `draw_gather.cpp:100` which REPLACES Scene::Clean, and `Scene::AddObject` `scene.cpp:1321`); the Geometry ctor (`scene.cpp:557`); the copy at `closure_batch2b.cpp:121`; Serialize | the game update (`GameUpdateLoop::Update` step 15 calls Scene::Clean, `gameupdateloop.cpp:1193`), the menu loop's TickParticles, `minimap_paint.cpp:210` (before its own Renderer::Draw), entity create / attach, input events (HighlightComponent::Repaint), load. **None inside Renderer::Draw.** |
| a unique pose (`MeshInstance::_nodes`) and the instanced pool (`AnimationManager::_instances[..]._states[..]._nodes`) | SKINMEMO section 1.2, unchanged: `AnimationState::Update` / `CopyInstancedToUnique`, `UpdateHierarchy`, `SetMesh`, `PlayAnimation`, ctor / dtor, Serialize; `AnimationManager::Update` / `GetInstanceId` / Serialize / teardown | the game update, entity create / destroy, load |

**What runs inside the scope** (pass 2 of `Renderer::Draw`, `wave27_lane_gfxbig.cpp:1038` to the end of the
body), checked call by call:
- Per scene entry: `DrawShadowMap` (slot 9) of each drawable, the Context's render target / viewport /
  scissor / `Clear`, then `DrawLight` (slot 10) x 3 lists x each drawable. The drawables are the lights:
  `AmbientLight` / `SpotLight` / `SunLight` (`lights_vtable.cpp:288/328/471/508/566/625`). Their bodies set
  Context state (`SetImageBuffer`, `SetLight`, `WriteShadowMatrix`, `SetViewport`, `Clear`) and call
  `Renderer::DrawItemList`. Nothing else.
- `DrawItemList` (`batchc_gfx_leaves.cpp:689`): `MaterialInstance::GenerateTextures` (slot 10:
  `ReflectionImageBuffer::Generate` is an empty body, `FrameCopyImageBuffer::Generate` a GPU blit),
  `MaterialInstance::Setup`, and the Geometry slots `SetSharedInstanceData` (7), `GetInstanceTransform` (6)
  and `Draw` (2), then the Context setters.
- Slots 2, 6 and 7 are **const** in `Geometry.h:401-415`. The Draw overrides in the tree (Ambient / Spot / Sun
  light, MeshInstance, EdgeMeshInstance, ParticleInstance, StatusIconGeometry, WorldGeometry,
  WeatherParticleGeometry) are all const; there is no `const_cast` of a Geometry, an instance, a pose or the
  AnimationManager anywhere in `src/engine/graphics`, `src/game` or `src/ext`, and no `mutable` member in
  Geometry.h / Mesh.h / ParticleInstance.h / InterpolatedState.h. The only `const_cast` on that path is the
  `MaterialInstance` handle at `batchc_gfx_leaves.cpp:801`.
- `MeshInstance::SetSharedInstanceData` reaches SkinMemo / `GetBones` (pure, SKINMEMO section 1.1).

**Why the scope is pass 2 only, not the whole Draw.** Pass 1 (the gather) reads no world matrix, and keeping
it out of the scope means the proof does not have to follow every `AddDrawItems` and cull path. (For the
record: in this tree ext.draw_gather calls `Geometry::Clean` only from `ExtClean`, the Scene::Clean
replacement in the update phase; `ExtCulledGather` does not. SKINMEMO's section 1.2 note that the draw_gather
gather calls Clean is out of date. Either way it would be pass 1, outside this scope.)

### 1.3 Pointer keys: no reuse inside a scope

A pair's address is exact only if the pair cannot move or be freed within the scope. A Geometry is freed by
its owner's destroy (game update); a pose array by the 1.2 writers (SetSize / Assign / Dtor); none runs in
pass 2. So an address seen in a scope names one live pair for the whole scope. Across scopes the epoch empties
the table, so a freed pair whose address is reused can only miss. The two-renderer unit test rewrites 400
pairs in place between every pair of scopes on purpose (section 7).

### 1.4 Threads

Every `Renderer::Draw` runs on the thread that owns the `Video::Context` (the game Draw, the menu loop's Draw,
the minimap capture, ext.eye_camera). The scope state and table are plain globals, like SkinMemo's. The
pathfinding workers do not touch geometry. The loading-screen doubt is SKINMEMO's (section 8).

### 1.5 What stays exactly as before

- Outside the scope (a GetInstanceTransform or Draw reached from anything but pass 2) the memo counts a
  decline and runs the retail lambda: the same arithmetic as before.
- Each site's own reads happen in the same order: the mesh / list / node / pool selection before the memo,
  the colour / opacity / column stores after it, then the same Context call. The only reorder is in
  `MeshInstance::Draw` and `EdgeMeshInstance::Draw`'s skinned arms: retail lerps the pair before the bones and
  AsMatrix-es after; the lerp now runs (on a miss) with the AsMatrix, after the bones. Both are pure.
- No guard was added. The pair pointer is formed by the same unguarded reads retail makes, so a null crashes
  at the same place; the memo itself dereferences nothing of the pair on a hit.
- `Context::SetTransform`'s redundant-state compare (`Matrix::operator!=`) and `SetTransformScale` are
  untouched: they receive the same bytes.

## 2. The candidates

### 2.1 Pair-keyed world matrix: BUILT

Built as above. The multiplier is the repetition: a rigid building with three triangle lists on node -1 is one
key for all three lists; each item is drawn in the sun's shadow map and by two lights in each list it is in;
a tree batch shares the instanced pool's pose across instances; every terrain chunk item lerps the one
WorldGeometry `_transform`. The row's `req/key` per site is the real number.

### 2.2 Keying on (geometry, packed) and caching the whole GetInstanceTransform output: NOT built

That would also skip the mesh -> detail level -> triangle list -> `_f14` chain (part of
`GetInstanceTransform`'s own 17% miss) and the colour stores. But the key would then stand for everything the
override reads: the mesh handle and its triangle lists, `_animation._instanced`, the layer handle, `_nodes`'s
data pointer and `_color`. Each is another census row (`_color` writers alone are game-side), for a saving of
three dependent loads per hit on top of what 2.1 already saves. Not worth the proof surface now; the row
shows whether it is worth a second block (`mesh.git` hits x ~3 loads).

### 2.3 Across frames: CLOSED, not exact

SKINMEMO section 2.2 applies unchanged: `t` changes every frame, and `Quaternion::Lerp(a, a, t)` depends on t
in the last bits even for a still pair. A cross-frame key cannot be bit-exact.

### 2.4 Left on the retail arm

- The two `GetNodeToWorld` arms (EdgeMeshInstance's rigid follow, in `GetInstanceTransform` and `Draw`): they
  compose the followed instance's transform with its pose; a different function and a different census.
- `StatusIconGeometry::Draw` (`statusicon_geometry.cpp:97`): it keeps the lerped Transform (the billboard
  origin) for later, so only the AsMatrix could be memoised; not done.
- `WeatherParticleGeometry::Draw`: per-tile matrices, not a pair lerp.

## 3. Design

- `xform_memo.h` (header-only): `Key` (16 bytes: pair address, t bits, a zero dword; no padding),
  `HashKey` (splitmix64's finaliser over `std::hash<const void*>` of the address and the t bits, no cast:
  pairs sit at 0x38 strides and Geometry +0x84, so the low address bits alone would use 1/8 of the slots),
  `Table`: **16,384 direct-mapped slots** (`kSlots`), each the stored key, its epoch and the 64-byte matrix
  (88 bytes; 1.4 MB, allocated on first use). A different key on a live slot **evicts** it (counted).
  `g_epoch` / `g_depth` / `Scope` in SkinMemo's style (Begin and End both bump the epoch), the counters per
  site, and the harness hooks `g_prof` / `g_verify` / `g_report`.
- `XformMemo::World(pair, t, site, faithful)`: outside a scope -> `faithful()`. Inside: build the key, look
  up the slot; hit -> copy the slot (or, under the shadow, run `faithful()`, compare, return the reference);
  miss -> claim the slot and run `faithful()` into it.
- The eight sites: retail's lerp + AsMatrix moved verbatim into the lambda; nothing else in the site changed.
- `Renderer::Draw` holds `const XformMemo::Scope xformMemoScope;` from the start of pass 2 to the end of the
  body (no early return after it).

The faithful-path cost when `kEnabled = false`: none. `World` is `return faithful();` (a lambda the compiler
inlines) and the Scope is empty.

## 4. Kill switch, shadow and row

- **Kill switch**: `XformMemo::kEnabled` (`src/engine/graphics/xform_memo.h`). Set it to false for the A/B.
- **Shadow `ItemTransform`** (`BANISHED_STRESS_COMPPROF=4`): on every hit the requesting site's own retail
  lambda runs on the live pair, all 16 floats are compared bit for bit with the slot, and the game is handed
  the REFERENCE. detail = `(site << 16) | first differing float` (0xffff = none). **Expected: 0 mismatches.**
  A writer missing from the census (1.2) or a spelling that is not what claim A says would show here. Read it
  as `[VERIFY] ItemTransform checks=N mismatches=M`. The FrustumCull, SkinBones, DrawItemSort and other
  shadows must stay at 0.
- **Row `/xform`** (`BANISHED_STRESS_COMPPROF>=1`, after `/skin`), two lines:
  ```
  [COMPPROF]   /xform world-matrix memo (XFORM) requests/frame=R hits=H (h%) keys/frame=K (R/K req/key)
    scopes/frame=S declines/frame=D evictions/frame=E; miss X ms/frame Y ns/key; hit Z ms/frame W ns/call;
    est. saved M ms/frame
  [COMPPROF]   /xform per site: mesh.git R hits H keys (req/key); mesh.draw ...; edge.git ...; edge.draw ...;
    geom.git ...; part.git ...; part.draw ...; world.draw ...
  ```
  How to read it:
  - `req/key` is the multiplier. Per site, `keys` counts the keys that site claimed first; a site's hits can
    come from a key another site claimed (mesh.git and mesh.draw share a MeshInstance's `_transform`), so a
    per-site req/key can exceed its hits ratio.
  - `scopes/frame` should be ~1 per Renderer::Draw (~2.3 with the reflection / minimap, as SKINMEMO's row).
  - `declines/frame` counts requests outside an emit pass and should be ~0.
  - `evictions/frame` should be small next to `keys/frame`; if not, raise `kSlots`.
  - `est. saved` = (miss ns per key x hits) - the hits' own time; indicative only (RDTSC per call inside the
    profile).

## 5. Prediction

The IBS group: `GetInstanceTransform` 3.9 + `RtLerpTransformPair` 2.4 + `Transform::AsMatrix` 5.4 +
`Quaternion::Lerp` 4.8 = 16.5% of main-thread ops (SetTransform 2.2 + operator!= 1.0 are untouched: same
bytes, same calls). Assume k = requests / key in 2.5-3.5 (SKINMEMO's instanced 2.7-2.9 per key, plus the
multi-list and terrain sharing here). The memo removes (k-1)/k of the Lerp / AsMatrix / pair-read work and
adds per request a 2-multiply hash, a slot read (2 lines of a 1.4 MB table: L2 misses, L3 hits on this box)
and a 64-byte copy.

- **Main-thread op share** of the four memoised functions: 12.6% -> ~5-6%; `GetInstanceTransform` itself
  stays (it still selects the pair) at ~3%.
- **Faithful profile, draw ms/frame**: heavy **-3% to -6%**, compact **-3% to -6%** (SKINMEMO's heavy -1.2 /
  -2.1% came from a smaller group; this one is ~1.5x its op share). Est. saved in the row: ~0.06-0.12
  ms/frame.
- **Must not move**: sim ms / iteration (draw-side only), the resave golden (no state is written), every
  shadow at 0.
- If `req/key` comes out under ~1.5, expect no gain or a small loss: the kill switch takes it out.

## 6. Files

| file | change |
|---|---|
| `src/engine/graphics/xform_memo.h` (new) | kill switch, Key, Table, scope, per-site counters, hooks, `World` |
| `src/engine/graphics/wave27_lane_gfxbig.cpp` | `Renderer::Draw` holds the scope from pass 2 |
| `src/engine/graphics/mesh.cpp` | MeshInstance / EdgeMeshInstance `GetInstanceTransform` |
| `src/engine/graphics/meshinstance_draw.cpp` | MeshInstance / EdgeMeshInstance `Draw` pose blocks |
| `src/engine/graphics/scene.cpp` | `Geometry::GetInstanceTransform` |
| `src/engine/graphics/wave27_lane_gfxpart.cpp` | ParticleInstance `GetInstanceTransform` and `Draw` |
| `src/game/Game/geometry_draw.cpp` | `WorldGeometry::Draw` |
| `src/diag/stress_harness.cpp` | the `/xform` rows, their reset per window, the `ItemTransform` hook, `g_prof` |
| `tests/unit/xform_memo_test.cpp` (new) | 5 tests (globbed by the unit target; `sources.cmake` needs no entry: the memo is header-only and the spellings link `transform.cpp` / `wave9_lane_p.cpp`, already listed) |

## 7. Build results (this block, static gate only, after `git merge main` = CULLROUTE)

- `tools/build.sh build perf --jobs 2`: exit 0. `build debug --jobs 2`: exit 0. 0 new warnings in the touched
  files (the two present are older: `scene.cpp:249` multi-line comment, `wave27_lane_gfxbig.cpp:146` the
  log-stripped `Attend` parameter).
- `tools/build.sh unit perf --jobs 2`: **1149 tests, 0 failures** (5 new):
  - `xform_memo_table_slots_and_epochs`: the stored-key compare (keys that differ only in t's sign bit or
    only in the pair miss), the epoch empties the table with no clear, a collision evicts, 4,096 pairs at a
    0x38 stride spread over > 3,400 slots, a decline outside a scope runs the faithful body.
  - `xform_memo_site_spellings_agree`: 80,000 cases across the five spellings, 0 differences.
  - **`xform_memo_two_renderers_bit_identical`**: the real `XformMemo::World`, table, epoch and scope over
    30,000 pairs (more than `kSlots`), 400 pairs rewritten in place between every two scopes, 5,000 visible
    pairs and 9,000 requests per scope through randomly chosen spellings and sites, 3 seeds x 40 scopes:
    **1,080,000 requests, ~526,000 memo hits, ~111,000 evictions, 0 mismatches**.
  - `xform_memo_shadow_clean_on_faithful_run`: with the shadow on, every hit is checked, 0 reported bad.
  - **`xform_memo_write_inside_scope_diverges`**: a pair rewrite inside a scope (the census broken) diverges,
    and with the shadow on the shadow reports it and the game still gets the reference (0 mismatches).
- `python tools/abi_audit.py`: OK. `tools/acceptance/acceptance.sh`: 0 lines off target.
  `python tools/castmetric.py src`: **total 2** (no new cast).
- Not run: the game, the stress town, the shadow, the resave gate, the A/B.

## How to measure (coordinator)

1. Shadow: heavy town at `BANISHED_STRESS_COMPPROF=4`; expect `[VERIFY] ItemTransform checks=N mismatches=0`
   with N in the hundreds of thousands, and every other shadow at 0.
2. Row: COMPPROF=1; read `/xform` (requests, req/key per site, evictions, declines ~0, scopes/frame).
3. A/B: `XformMemo::kEnabled` false vs true (`src/engine/graphics/xform_memo.h`), unprofiled perf exe,
   interleaved pairs, heavy and compact, RTSS / Afterburner / RTSSHooksLoader64 stopped, machine CLEAN.
   **Should move: draw ms/frame** (down). **Must not move: sim ms / iteration** and the resave golden.
4. Gate run as usual (new game, resave gate).

## 8. Doubts

- **The census rests on pass 2 reaching only const geometry members.** A future draw-time writer (a Draw
  override that calls SetTransform, a `const_cast`) would break the claim silently in the build and show up
  only as an `ItemTransform` mismatch. The unit test shows such a writer does diverge.
- **Claim A rests on the build flags.** If anyone adds `-march=native`, `-mfma` or `-ffast-math`, GCC may
  contract `p + (c - p) * t` into an FMA in some spellings and not others, and cross-site hits would differ in
  the last bit. The shadow would catch it; the unit spelling test runs at -O0 and would not.
- **WorldGeometry is game-side** (`src/game`), included because it is the same pair lerp. No `SetTransform`
  in the tree targets it, so its `_transform` keeps the ctor's value; its only possible writers are the
  Geometry ones in 1.2 (SetTransform, Clean), all in the update phase.
- **The table is 1.4 MB** and a lookup touches two lines of it. On a part with a small L2 / L3 the hit can
  cost more than on this 96 MB-L3 box (feedback: do not assume X3D L3); `kSlots` = 8192 halves it at the cost
  of evictions. The row's `hit ns/call` vs `miss ns/key` says which.
- **The verdict is the A/B.** If `req/key` is close to 1 (shadows off, one light), the memo costs a hash and
  a lookup per request for nothing; the kill switch takes it out.
