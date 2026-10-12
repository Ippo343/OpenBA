# PERFXEXT -- PerfX becomes extensions (`ext.render_culling` A, `ext.anim_batch` B)

Block PERFXEXT, 2026-10-12. The user's decision (ELPERF2 close): PerfX -- a perf-build-only,
env-gated set of non-retail optimisations in `src/app/perf_experimental.cpp` -- becomes proper
extensions in `src/ext`, compiled into every flavour, default OFF; the `BANISHED_PERF_*` flags and
the `-DBANISHED_PERF_EXPERIMENTAL` master are retired. Static gate only: the game was NOT run by
this block (the coordinator runs the gate, the COMPPROF=4 shadows and the stress A/B).

## 1. Classification (written first)

Class A = no sim effect, no serialized state, live toggle. Class B = sim-divergent, format-neutral,
locked per game (DESIGN.md §2). "Faithful" = same results as retail's code by proof; not an
extension, unconditional code (as TREE_SNAPSHOT / WALKCACHE / BESTTARGET / PERFXTBL already are).

| knob | what it changes | can the sim (or the save) observe it? | verdict |
|---|---|---|---|
| `animCull` | last speed iteration: an off-screen model's `MeshInstance::Update` runs root-only (`_rootOnlyUpdates` = 1 for that one call), as retail's own intermediate iterations do | no. Root-only advances the clock and the root-motion offset exactly as the full update (it is retail's path for N-1 of N iterations). The sim's reads of a MeshInstance are `IsAnimating` (clock vs duration, layer 0; `mesh.cpp` 0x1800ff7d0), `RootAnimationOffset` (move tick) and `PlayAnimation`; the node pose is read only by presentation (`GetNodeToWorld`: particle attach `particlecomponent.cpp:216/719`, highlight, `tool_base_render.cpp`; the carried tool via `MeshInstance::Attach`, `action_base_complete.cpp:374`). The pose is not serialized. Pause flush re-poses with a zero-time update | **A** (`ext.render_culling`) |
| `geomXfLastOnly` | move tick: `Geometry::SetTransform` (render scene transform, bounds, children) skipped on intermediate iterations | no. `model->transform` (+0x148, the gameplay copy) is still written every tick. Game code never queries the graphics scene spatially (only Add/Remove/Clean, `grep scene->`); `meshInstance._transform` is read by the tutorial highlight and ext.eye_camera (presentation) only | **A** |
| `geomCull` | last iteration: the same push skipped for an off-screen citizen; pushed in the pause flush | no (as above) | **A** |
| `iconLastOnly` / `iconCull` | status icon: the positioning tail (`StatusIcon_SetIconTransform`: bounds / `GetTerrainPos` reads, `Geometry::SetTransform`) skipped on intermediate iterations / for an off-screen owner; replayed on pause | no. The scene add/remove and opacity part of the tick runs before the skip, unchanged; the tail writes only the icon geometry | **A** |
| `pickCull` | last iteration: `Picking::Scene::SetTransform` of an off-screen citizen deferred; replayed on pause | no. That scene (`GameUpdateLoop+0x70`) is queried only by the player tools (input); the map's own picking scene that the sim ray-casts for heights (`mapquery.cpp:133`, `Map+8`) is a different scene and is not touched | **A** |
| `particleCull` (as shipped in PerfX) | an off-screen ParticleComponent is not ticked at all (frozen) | **yes, as shipped.** The tick's fade integration is what calls `StopEmitter` (opacity reaching 0): that writes `emitters[i].sceneId = -1`, `active[i] = false` (serialized, `ParticleComponent::Persist` 0x140053270) and clears flag bit 2 (`RefreshEntityFlags`, update-tree membership). `TornadoComponent::Tick` reads `emitters[0].sceneId >= 0` to decide whether the tornado lives (`tornadocomponent.cpp:819`): a frozen off-screen funnel would keep moving and damaging | **A after a fix (§3.1)**: a component is frozen only while every live emitter has `fadeRate == 0` and `opacity > 0`. Then retail's tick provably changes only presentation state (node-follow transform, the ParticleInstance simulation); a fade starting while frozen unfreezes it on its next tick. Shadow `PerfXSim` (§5) |
| `animBatchAll` | speed N: the N-1 intermediate root-only updates of a model whose root offset "cannot differ" are skipped, the last iteration advances it by `(skipped+1) x tick` | **yes.** (1) The clock lags by up to N-1 ticks inside the frame and three sim reads compare it with the duration: `NaturalResourceComponent::Tick` (every iteration; on "not animating" it clears its bit and calls `RemoveResource`, `naturalresourcecomponent.cpp:53`), `AnimateNaturalResource::Execute` (a citizen action completes on `!IsAnimating`, `animatenaturalresource.cpp:81`) and the tornado's tree knock-down test (`tornadocomponent.cpp:375`). A falling tree batched at speed N finishes up to N-1 ticks late, so the felling citizen's next action starts late. (2) A `PlayAnimation` mid-frame switches the model to per-tick, but the last iteration still applies `(skipped+1) x tick`, the skipped ticks of the OLD animation, to the NEW one: its clock runs ahead, and for a non-instanced root-motion clip `RootAnimationOffset` is computed with that dt (the "exact walk speed" claim holds only for frames without a clip change). Animation events (`AnimAudioEvent`, the only `AnimationEvent` subclass) are audio only | **B** (`ext.anim_batch`, its own extension, formatVersion 1). Shadow `AnimBatchSim` (§5) counts the IsAnimating answers that differ |
| `animBatchCulled` | the same, only for off-screen models | yes (as above), and the divergence would depend on where the camera looks | **dropped** (no measurable gain on its own, perf/README iteration 12; `anim_batch` batches every eligible model) |
| `ambientThrottle` | `AmbientEnvironmentComponent::Update`: the biome histogram reused for 7 of 8 updates per emitter | **yes (the save).** The dominant biome selects `AmbientEmitterComponent` slot sounds (`SetSlot`), which `AmbientEmitterComponent::Persist` serializes (the sound ExternalPtr per slot); a map change shows up to 7 updates late. No gameplay read. Also: its cache (`unordered_map` keyed on `this`) is never pruned, so a new component at a freed address inherits a stale biome | **dropped** (retail's body restored). A class B lock for an ambient-audio crossfade is not worth a save entry; its gain was never measured alone (one of four in the 0.245 -> 0.231 step) |
| `treeSnapshot` | the update-tree walk from a flat in-order snapshot | no: the same visit order (WALKCACHE §3, PERFXTBL) | **faithful perf work**: unconditional (was default-on only in the perf build); a `constexpr` kill switch in `entitylist.cpp` |
| `terrainSidecar` | the move tick's terrain-type byte read from a 1-byte mirror | must be the same byte. It was NOT in every case: two live writers of `MapCell::terrainType` never told the mirror (`Cemetery_SpawnTombstone_6a90`, `citizen_lifeevent_handler.cpp:228`, and the pasture cell retype, `pasturecomponent_states.cpp:446`, both `= 2`), so after a burial or a pasture retype the move tick could read the cell's old type (the old verify run, 800x10 3000 frames, had neither). Fixed here (both now call the note). With the writer list complete it is the same byte | **faithful perf work**: unconditional; `BANISHED_PERF_TERRAIN_VERIFY` becomes the COMPPROF=4 shadow `TerrainSidecar` |
| `cullMargin` | the 6-unit frustum margin (16 for particles) | -- | `constexpr` (`PerfX::kCullMargin`) |
| `trace` | the 600-frame census on stderr | -- | **removed** (with `PerfX::Count` and its counters) |

**PERFXTBL / WALKCACHE when the extensions are off.** WALKCACHE and the snapshot are faithful and
stay on. PERFXTBL's side table serves only `anim_batch` (batch counts) and `render_culling`'s
particle verdicts: with both off, `PerfX::OnWalkSnapshot` builds nothing (one call per snapshot
rebuild that leaves the side unusable). `g_walkIndex` stays one unconditional store per entity in
the walk (a branch on the extension there would cost as much as the store). When either extension
turns on, `BeginFrame` marks the walk snapshot dirty so the side table is built with the next walk.

## 2. The extensions

| id | class | display name | the knobs it owns |
|---|---|---|---|
| `render_culling` | A (live) | "Skip unseen presentation work" | animCull, geomXfLastOnly, geomCull, iconLastOnly, iconCull, pickCull, particleCull (exact form) |
| `anim_batch` | B (locked per game, pseudo-mod `ext:anim_batch`, formatVersion 1) | "Batched animation at high speed" | animBatchAll |

One toggle each; the dialog has no sub-options (rows come from the registry table,
`extensionsdialog.cpp:189`), so the per-knob granularity is `PerfX::kRenderCulling`, a `constexpr`
struct the extension owns. No `.rsc` change: the dialog lists registry rows, so no kit run.
