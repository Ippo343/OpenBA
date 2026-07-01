# Worldgen Generation Deviations — CLOSED REGISTER

Status: **CLOSED** (2026-07-01). Every remaining worldgen deviation has a decided
disposition below. None are bugs: all are consequences of the standalone build's
**inert-component architecture** — a deliberate project policy where ECS components
are instantiated + registered but never made live (no `Startup`, no per-frame
`Update`, no scene remove/re-add), the same gate applied to structures and citizens.
The authoritative, byte-level rationale for each lives **in-code** in
`code/Game/Game/starttown.cpp` (the port of the `FUN_1400a12b0` natural-resource
scatter); this file is the decision summary.

Dispositions:
- **ACCEPTED** — produces output net-identical to retail; not observable in the
  generated world. Permanent; nothing to revisit.
- **DEFERRED→LIVE** — moot in the default inert build (the divergent code path never
  executes). Only relevant if/when the live-component subsystem is built
  (`BANISHED_LIVE_GROWTH`); carries the exact revisit trigger so it is not lost.

(#1 and #2 were closed in earlier passes — RNG-alignment of the scatter stream via
`FUN_1400a12b0` prologue Perlins + `RandFloatTol` jitter/yaw; commits `1c7d7ac`,
`b4efaac`. This register tracks what remained.)

| # | Deviation | Root cause | Disposition | Anchor |
|---|-----------|-----------|-------------|--------|
| 3 | Variant mesh pick (`FUN_1401253d0`) runs **before** registration, so `OnAdded` does a single scene-add instead of the original's vtable+0x38/+0x30 scene **remove + re-add** (`ModelComponent_helper_2b70`). | The remove/re-add slots need a live `ModelComponent`; inert components never dispatch them. Running the variant pick pre-registration reaches the **same final scene contents** (correct growth-stage mesh present, exactly once). | **ACCEPTED** — net-identical scene state. | `ApplyResourceModelVariant`, starttown.cpp:1257–1283 |
| 4 | `ComponentEntity_impl_5cc0` (SpawnComponent initial sub-resource spawn) skipped entirely. | Recurses via `impl_5780` and reads `GrowthComponent+0x28`, valid only when `GrowthComponent` is live. Skipping omits recursively-spawned sub-resources — a live-only content path, consistent with the whole resource block being guarded off for inert components. | **DEFERRED→LIVE** — revisit under `BANISHED_LIVE_GROWTH` once `GrowthComponent::Startup` populates `+0x28`. | starttown.cpp:1289–1292 |
| 5 | Growth init (`FUN_140120db0`) is a no-op, and its `RandFloat` draw is latent-reordered vs the variant `Rand`. | Trees carry no `GrowthComponent` in the standalone factory (`FindComponent` returns null on every tree), so **neither** the original nor the port draws here — the reorder is unobservable today. It would only misalign the world RNG under a live build. | **DEFERRED→LIVE** — under `BANISHED_LIVE_GROWTH`, restore the `120db0` `RandFloat` draw to its original position in the scatter stream (before the variant `Rand`) to keep the seed byte-faithful. | `StartTown_SetGrowthProgress`/`ActCNR_SetGrowthProgress_120db0`, starttown.cpp:1563–1596 |
| 6 | Height/slope sampled via `fn_MapGetTerrainPos` instead of the `QuarryComponent_helper_aa20` picking ray. | `aa20` is null in standalone. `a12b0`'s picking ray is **vertical**, so a heightmap vertical sample returns the same height + slope — mathematically equivalent. | **ACCEPTED** — equivalent result. | starttown.cpp:1211–1214, 1361–1372 |
| 7 | `dee0` cell-grid terrain-type marking folded into placement, and `dc20`'s two-pass occupancy collapsed to a single cell+0x08 write. | `MapComponent::OnCreate`/`Draw` never fire on inert components, so the real `OnCreate → impl_4750 → dee0` and lazy `Draw → sub_41b0 → dc20` chains never run. The fold writes the **net end-state** both passes would leave (classified terrain type + owner-entity occupancy), which is what pathfinding + harvest-search read. | **ACCEPTED** — net-identical cell state; guarded notify side-effects target inert subsystems. | `StartTown_MarkResourceCellGrid`, starttown.cpp:1317–1520 |

## Closure summary
- **3 ACCEPTED** (#3, #6, #7): net-identical to retail output; permanent, no action.
- **2 DEFERRED→LIVE** (#4, #5): unobservable in the default inert build; each carries
  the exact `BANISHED_LIVE_GROWTH` revisit trigger and fix for a future live-component
  pass.
- No open/undecided deviations remain. Current scatter is A/B-verified crash-free
  (Rock ~890, Ore ~541, Tree ~8081; trees keep 3 variants, rock/ore default variant).
