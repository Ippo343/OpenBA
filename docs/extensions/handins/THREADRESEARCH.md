# THREADRESEARCH hand-in (2026-10-13)

Research only: no code, no build, no run, no sub-agent. The ask (coordinator, for the user): a multithreading research
pass over the frame -- renderer / sim overlap, parallel cull + gather, parallel batched walking, the snapshot walk -- with
censuses, designs and a ranked recommendation to act on next session. Base: main `30ce2f4bd`.

**Every time figure below is an ESTIMATE** reasoned from the code and from the two uProf passes in the tree:
`IBS_20261013_S50B.md` (speed 50, everything on + `walk_batch_grid`, heavy spread town; tables in
`perf/uprof/results_20261013_s50close/`) and `IBS_20261013.md` (speed 10, faithful, heavy). IBS op shares are used as a
time proxy (cycles per op differ between rows; the error is stated where it matters). Nothing was measured for this
hand-in. The box: Ryzen 7 9800X3D, 8 cores / 16 threads, 96 MB L3.

## 0. Short version

**Design target (the user, via the coordinator): a 4-core / 4-thread machine, main thread + 3 helpers, with
`ext.path_workers` already holding threads.** This box (8 cores / 16 threads) is the second column, for measurement
only. No Amdahl extrapolation: every candidate gets a break-even (work it moves per frame against a Windows fork-join
cost), a cap, and the expected result on 4 cores with the path workers running (§7, the binding section for the target; §5 per candidate).

| rank | candidate | class | exact? | helpers to pay | 4-core result (path workers running) | this box | size |
|---|---|---|---|---|---|---|---|
| 1 | **Pipelined emit**: pass 2 + the post-scene draws + Present on a render thread, overlapped with the NEXT frame's pass 1; a full fence before every sim step | A | yes for the sim (§1.6) | **1** (cap 1) | **pays**, if the render thread outranks the path workers (§5.1): CPU-bound gain of the order of this box's, zero where the GPU is already the bound; worst case = today + ~30 us per frame | speed 10 heavy 847 -> ~1,300-1,850 fps; speed 50 +45..+117% (GPU permitting) | large |
| 2 | **Parallel cull inside `ext.draw_gather`'s walk**, subtree tasks concatenated in DFS order | A | yes (§2.3) | 1 SPINNING helper (event-parked helpers lose at any count) ; cap 2 helpers | **same or worse** with rank 1 on (no free core during pass 1); ~0.10 ms per frame with rank 1 off; a preempted helper is a frame hitch | ~0.10 / ~0.14 ms per frame with 1 / 2 helpers | medium |
| 3 | Async sort on one helper | A | yes (§2.6) | 1 spinning helper; cap 1 | **worse** (no core; <= 0.1 ms to win) | <= ~0.1 ms | small |
| 4 | Parallel batched walking (`walk_batch_grid`) | **B at best** | **no** (§3.2) | 1 spinning helper for the whole step; cap 1-2 | **same or worse** (the walkers' lines migrate back every iteration) | ~0 to +7% fps at speed 50 | medium + a B row |
| -- | The snapshot walk (21%) | -- | **impossible** exactly (§4) | -- | -- | -- | -- |
| -- | Full sim / render decoupling, D3D11 deferred contexts | A | exact / driver-dependent | -- | -- | -- | very large (§1.7) |

**Only rank 1 pays on the 4-core target.** Ranks 2-4 need a helper that spins while the main thread works, i.e. a free
core beyond main + render + path workers; they are 6- and 8-core features at best and default to 0 helpers on 4 cores.
The path workers' current default on a 4-thread machine is 2 (`hardware_concurrency - 2`, `pool.cpp:10`); with rank 1 on
it should be 1 (§7.4).

**The biggest surprise:** the frame is NOT "sim then draw" every frame. `MainLoop::Frame` runs the sim in fixed 1/60 s
steps and draws uncapped (`mainloop_frame.cpp:361`, `:466`), and the input pump is INSIDE the step loop (`:408`). At
speed 10 heavy, ~14 of every 15 frames run no sim step and no input at all; at speed 50 w1, ~6 of 7. So "overlap the
renderer with the sim" never has to run the sim beside the renderer: overlapping a frame's emit with the next frame's
gather, and draining before each step, is enough -- and that makes the class A argument nearly mechanical, and needs one
helper, which is why it is the one candidate that survives the 4-core budget. The flip side, stated plainly: at speed 50
each step is ONE 8.5-13 ms block (50 iterations x 0.17-0.26 ms) every 16.7 ms, during which nothing is presented. No
exact scheme in this hand-in removes that hitch; it raises the average fps between hitches.

## 1. Renderer / sim overlap

### 1.1 The frame as built

| order | what | where | thread |
|---|---|---|---|
| per Frame | `while (_accumulator >= _deltaTime)` -- 0..4 fixed steps of 1/60 s (`_maxAccumulator` 1/15 s) | `mainloop_frame.cpp:361`, ctor values `:95`, `engine.cpp:287` | main |
| per step | input, audio sweep, **the message pump** (`Platform::Pump` -> `WindowProc`), `MainLoop::Update` -> the loop's `Update`, the post-update callbacks | `mainloop_frame.cpp:367-433` | main |
| `GameUpdateLoop::Update` | `PerfX::BeginFrame`, `WalkBatch::BeginFrame`; the sim loop `do { RegionPairCache, AnimationManager::Update(rootOnly), EntityList::Update*, calendar, CitizenList::Update } while (iteration < speedMultiplier)`; achievements; `PostUpdate`; camera transform; music; WorldGeometry::Update; **UI::Update**; cursor; audio scene; quicksave / SaveLoad::Update; **`Scene::Clean` (or `ExtClean`)**; minimap upload under `Device::Lock`; next-loop check | `gameupdateloop.cpp:525`, `:559-560`, `:767-844`, `:1000`, `:1197`, `:1204-1213` | main |
| per Frame tail | `context->_time` stores, then `MainLoop::Draw(alpha)`, then `SwitchToThread` | `mainloop_frame.cpp:457`, `:466`, `:468` | main |
| `MainLoop::Draw` | `BeginCommands` (**takes the device lock**), `ClearResources` if dirty, viewport reconcile, the loop's `Draw`, post-draw callbacks, `EndCommands` (resolve), `_frameUpdateTime`, **`++context._frameIndex`**, `Finish` (**Present, drops the lock**, device-lost recovery) | `loops.cpp:1046-1111`; lock `contextdata.cpp:421` / `:509` | main |
| `GameUpdateLoop::Draw` | stress bracket, pause -> alpha 1, `SetTime`, `PaintTerrain` (one-shot, first frame), `UpdateCamera` / `EyeCamera::ApplyFrame`, `Weather::DrawUpdate` (context + `SunLight::Set`), `_shaderDetail`, **`Renderer::Draw`** (or `EyeCamera::DrawScene`), `SaveLoad::Draw`, screenshot, camera matrices for the UI, `Tool::Draw`, `UI::Draw` / `SharpUI::Draw` | `gameupdateloop.cpp:1257-1525`; Renderer::Draw call `:1414-1430` | main |
| `Renderer::Draw` | `SkinMemo::Scope`; `Reset`; `_alphaT`, `_frameIndex = context._frameIndex`; `AddScene`; **pass 1** (per scene entry: `BeginDrawGroup`, global `AddDrawItems` -- the lights open their own groups and gathers -- `AddCulledDrawItems`, 3 sorts); **pass 2** backward (per entry: `DrawShadowMap` per light, render target / viewport / scissor / `Clear`, `DrawLight` x 3 lists x lights -> `DrawItemList`) | `wave27_lane_gfxbig.cpp:932-1097`; pass 2 starts `:1030` | main |

Frames per step: speed 10 heavy faithful 847 fps / 60 = ~14; speed 50 w1 412 / 60 = ~7 (SORTMEMO Results ab3;
IBS_20261013_S50B §0). A frame with no step executes `GameUpdateLoop::Draw` and nothing that writes sim state.

The wall model that fits every measured row: per second, `S + F x D ~= 1000 ms`, where S = 60 x speed x (ms per
iteration) and D = draw ms. Speed 10 heavy: 162 + 847 x 0.97 = 984; speed 50 w1: 510 + 412 x 1.2 = 1004; w3: 630 +
282 x 1.2 = 968.

### 1.2 What Draw reads that the sim writes

| state read by Draw | read in | written by (sim side) | written when |
|---|---|---|---|
| Geometry `_transform` pair, `_nodes` pairs, instanced pool states (`GetInstanceTransform`, `GetBones`) | pass 2: `DrawItemList` `batchc_gfx_leaves.cpp:757`, `:771`; `mesh.cpp:2093` | `SetTransform`, `UpdateAttached`, `Geometry::Clean` roll, AnimationManager / AnimationState (full census XFORM §1.2, SKINMEMO §1.2) | steps only (Update step 3 and step 15), entity create, load, the menu loop, `minimap_paint.cpp:210` (inside `PaintTerrain`, a Draw step) |
| `_worldBounds` / `_prevWorldBounds`, `_opacity` pair, `_materialTypeFlags`, `_detailId`, scene grid (cells, chains) | pass 1 (cull, LOD pick `mesh.cpp:2138`) | `Clean` / `GridReindex`, `AddObject` / `RemoveObject` | steps (Clean at `gameupdateloop.cpp:1197`); the loader thread's `AddObject` (pre-existing race, `draw_gather.cpp:66` comment) |
| particle pools (`ParticleInstance::Draw`, `wave27_lane_gfxpart.cpp:724`) | pass 2 | `ParticleInstance::Update`, ParticleComponent ticks | steps |
| material instance parameters, textures (`MaterialInstance::Setup`) | pass 2 | game components (highlight, construction) | steps |
| terrain GPU buffers (`WorldGeometry`) | pass 2 | `WorldGeometry::Update` (`gameupdateloop.cpp:1020`), terrain edits | steps |
| SunLight direction / colours | pass 1 (`GetSunCamera`, `wave28_lane_partsys.cpp:889`) and pass 2 (`SunLight::DrawLight`, `lights_vtable.cpp:566`) | **`Weather::DrawUpdate` -> `SunLight::Set`** (`weather_sun.cpp:110`) | **every frame, in Draw**, but idempotent between steps: `BlendLighting` is const over the calendar's year time, the effects' intensities and `showWeather` (`weather_sun.cpp:56-84`), all step-written |
| `PlayerController::camera` | pass 1 (`AddScene` copies it); post-scene camera matrices `gameupdateloop.cpp:1455-1477`; tools (`tool_base_ray.cpp:203`) | Update step 6 `SetCameraToWorld` (`:1000`); Draw step 4 `UpdateCamera` (`gameupdateloop_load.cpp:531`, idempotent: backbuffer size + constants) or **`EyeCamera::ApplyFrame` (alpha-dependent, NOT idempotent)** (`gameupdateloop.cpp:1350`) | steps; every frame |
| UI / tool state (`UI::Draw`, `Tool::Draw`) | post-scene | `UI::Update`, the pump's input events | steps only (the pump is inside the step loop) |

The rule this table gives: **between two steps, nothing writes what Draw reads, except Draw itself.**

### 1.3 What Draw writes, and who reads it

| written by Draw | where | read by the sim? | read by the next frame's pre-pass-2 work? |
|---|---|---|---|
| `Video::Context` state (dirty bits, constants, staging, `_time`, `_frameIndex`) | everywhere in pass 2 and post-scene; `SetTime` `gameupdateloop.cpp:1317`; DrawUpdate's half `weather_sun.cpp:92-104`; Frame tail `mainloop_frame.cpp:457`; `++_frameIndex` `loops.cpp:1110` | `ParticleInstance::_frameIndex` is a DIFFERENT counter (sim-owned, `gameupdateloop.cpp:839`); no sim read of `Context::_frameIndex` found (grep of `src/game`, `src/ext`: only `sharp_ui.cpp:260`, a UI draw) | **yes**: pass 1 copies `context._frameIndex` into `Renderer::_frameIndex` (`wave27_lane_gfxbig.cpp:949`), and `ReflectionImageBuffer::Render` (pass 1, `imagebuffer_derived.cpp:609-657`) latches on it. Read stale, the reflection scene entry is skipped for the frame (`_generation == generation` early-out): a visible bug, not a race to tolerate |
| Renderer state: `_sceneGroups`, `_drawGroups` + item lists, `_drawables`, `_alphaT`, `_frameIndex`, `_shaderDetail` (also written in pass 2, `:1036`) | `Renderer::Draw` | `PerfX::BeginFrame(m_renderer)` reads the group camera only as a fallback (`perfx.cpp:293`); `SetWorldFrustum` is fed from pass 1 (`perfx.cpp:301`) | yes: pass 1 resets and refills the same lists pass 2 iterates |
| latches on image buffers: `ReflectionImageBuffer::_generation` (pass 1), `FrameCopyImageBuffer::_generation` (pass 2, `wave23_lane_gfxbody.cpp:618`) | | no | each latch is touched by one pass only |
| SkinMemo scope / table, XformMemo (shipped OFF), DrawSort memo + scratch (`draw_sort.cpp:108-121`, `:444`) | pass 2 / pass 1 | no | process globals, "main thread only" by comment; SkinMemo's scope opens at `Renderer::Draw` entry (`wave27_lane_gfxbig.cpp:944`) but is used in pass 2 |
| Geometry objects | **nothing**: slots 2 / 6 / 7 are const, no `mutable`, no `const_cast` of a Geometry on the path (XFORM §1.2) | -- | -- |
| ext.draw_gather side records, counters (`draw_gather.cpp:389`, `:386`, `:407`) | pass 1 | no | pass 1 only |
| `MainLoop::_frameUpdateTime` (`loops.cpp:1106`) | Draw tail | no | read by the next `SetTime` (`gameupdateloop.cpp:1313`) -- a wall-clock value, nondeterministic already |
| one-shot: `PaintTerrain` removes / re-adds the sun and terrain globals and draws the minimap terrain (`gameupdateloop.cpp:1331-1340`) | Draw step 3, first frame only | -- | needs the context and the scene exclusively |
| counters (`g_drawCalls`, `g_addDrawItemsMesh`, `g_gatherDrawItems`, FrustumCull sites ...) | both passes | stress harness only | -- |

**No sim read of anything Draw writes was found.** The class A claim of §1.6 rests on this table plus §1.2.

### 1.4 Context and device-lock threading today

- One `Video::Context` over the D3D11 **immediate** context (`contextdata.cpp:432`); no deferred context anywhere in
  `src/render/dx11/` (grep `Deferred`, `FinishCommandList`: none). The device is created with flags 0 (not
  single-threaded, `devicedata.cpp:1104`), so resource creation is free-threaded at the D3D level.
- The engine guards the immediate context with `Video::Device`'s recursive lock: `BeginCommands` takes it
  (`contextdata.cpp:421`) and `Finish` drops it after Present (`:509`), so **the whole Draw holds it**. Every GPU
  resource create / destroy takes it too (`buffers.cpp:345-433`, `RtVideoDeviceLock`), as do the minimap upload in
  Update (`gameupdateloop.cpp:1206`), `minimap_upload.cpp:69`, `serialize_image_upload.cpp:191`, the save thumbnails
  (`saveload_update.cpp:238`) and the mods dialog (`moddialog.cpp:199`).
- Consequence for any overlap: a sim step that creates or destroys a mesh, a texture or a buffer would block on a render
  thread that holds the lock for a whole frame. The fence of §1.5 removes the contention by construction (the render
  thread has dropped the lock at Present before any step starts).
- Draw callers of `Renderer::Draw`: the game Draw, `EyeCamera::DrawScene`, `PaintTerrain` (`minimap_paint.cpp:211`), the
  menu loop (SKINMEMO §1.4). All on the main thread.

### 1.5 The design: pipelined emit, fenced before every step

**Split.** Frame N's work after `Renderer::Draw` pass 1 -- pass 2, `SaveLoad::Draw`, screenshots, the UI camera
matrices, `Tool::Draw`, `UI::Draw`, the post-draw callbacks, `EndCommands`, `++_frameIndex`, `Finish` -- runs on a
render thread, while the main thread runs frame N+1's pre-pass-2 work (`GameUpdateLoop::Draw` steps 1-6 and pass 1).
Pass 1 stays on the main thread: it is where the scene is read under the same thread as every scene writer.

**Hand-off and back-pressure.** One packet in flight. The main thread finishes pass 1 of N+1, then waits for the render
thread to finish N, then hands over N+1. Depth 1: one frame of extra latency (0.6-1.7 ms at these rates).

**The fence.** Before the first step of a Frame (the top of `while (_accumulator >= _deltaTime)`), the main thread waits
until the render thread is idle (frame N presented, the device lock dropped). Every sim step, every input event, every
UI update, every loop transition, every save and every `Device::Lock` in Update then runs exactly as today, with no
render work alive. The render thread holds no state the step can see.

**What has to move, be double-buffered, or be owned by one side (the six fixes):**

| # | item | today | in the pipeline |
|---|---|---|---|
| 1 | `Renderer` frame state (`_sceneGroups`, `_drawGroups` + three item lists each, `_drawables`, `_alphaT`, `_frameIndex`, `_shaderDetail`) | one `m_renderer` | **two Renderer instances**, ping-pong per frame. Step 6 (`gameupdateloop.cpp:1397`) sets `_shaderDetail` on the instance about to run pass 1, so the "uninitialised on the first frame" quirk (`wave27_lane_gfxbig.cpp:399-406`) behaves as today on both. Lists are already per Renderer (`Reset` keeps capacity, `wave9_lane_gfxd.cpp:542`); the SORTMEMO table is keyed by buffer pointer, so it holds 8 entries instead of 4 and each list hits against its own previous use two frames back; `kMemoMaxAge` is 1024 sorts (`draw_sort.h:95`), far above 2 frames |
| 2 | `Context::_frameIndex` read by pass 1 | incremented at the end of the previous Draw | the main thread keeps the counter for pass 1 (`frames handed off`), equal by construction to what the context holds when that frame's pass 2 runs; the render thread keeps retail's `++` for the UI side. Fixes the reflection-latch bug of §1.3 |
| 3 | Context writes made on the main thread before pass 2 (Frame tail `_time` `mainloop_frame.cpp:457`, `ClearResources` / viewport `loops.cpp:1063-1083`, `SetTime` `gameupdateloop.cpp:1317`, DrawUpdate's context half `weather_sun.cpp:92-104`, the snow diag) | inline | recorded in the packet as values and replayed in retail's order by the render thread before pass 2 |
| 4 | `SunLight::Set` from `Weather::DrawUpdate` (read by pass 2 of N while pass 1 of N+1 runs) | every frame | run only on the first frame after a step (render idle, nothing to race); between steps it would rewrite the same bytes (§1.2), so skipping is exact. The first block proves the idempotence with a shadow (§5) before relying on it |
| 5 | `UpdateCamera`'s frustum (read by the render-side tools) | every frame | same rule as 4 (idempotent: backbuffer size and constants). **While `EyeCamera::Active()` the frame runs unpipelined** (`ApplyFrame` is alpha-dependent and the tools read the live camera, `tool_base_ray.cpp:203`) |
| 6 | SkinMemo scope (`wave27_lane_gfxbig.cpp:944`) | around the whole `Renderer::Draw` | around pass 2 on the render thread; SkinMemo, XformMemo and every pass-2 counter become render-thread-only, the DrawSort memo / scratch and the gather state stay main-only |

**Transitions** (each one is "drain, then run the frame exactly as today on the main thread"):

| transition | rule |
|---|---|
| first frame / first frame after a toggle on | nothing in flight; the packet starts empty |
| a step | the fence above |
| `PaintTerrain` (minimap terrain, `terrainImage == nullptr`) | drain, run unpipelined |
| resize (`device->settingsChanged`, set only from the options UI or `WindowProc`, both inside a step) | the main thread tests it at hand-off; set -> drain and run that frame unpipelined so `BeginCommands`' resize path (`contextdata.cpp:414-418`) runs with no pass 1 alive |
| device lost (Present fails inside `Finish` on the render thread) | the recovery destroys Externals and recreates the context (`gameupdateloop.cpp:2314-2331`, `contextdata.cpp:541-583`): the render thread waits until the main thread is parked at the hand-off, runs retail's recovery, and the parked frame's packet is DISCARDED and its pass 1 re-run (its draw items may name freed Externals) |
| `GameUpdateLoop::DrawFailed` | empty (`GameUpdateLoop.h:88`): nothing to marshal |
| save / quicksave / quickload | saves run from the UI pump inside a step: fenced. The screenshot steps are post-scene draws: render thread, in order |
| loop change (`SetNextLoop`, quit, `_quit`) | happens in a step or at the Frame boundary: drain; the menu loop's Draw is never pipelined |
| toggle off (class A, live) | drain at a Frame boundary, free the second Renderer |

**Size.** `src/ext/render_pipe/` (the thread, the packet, the fence, the pass-1 / pass-2 split as a COPY of
`Renderer::Draw`'s two loops -- the faithful body stays untouched, as `ExtCulledGather` copies the walk); one engine seam
(a null-by-default static pointer in `MainLoop::Frame` / `MainLoop::Draw`, DESIGN hook rule 5) so the Frame tail and
`MainLoop::Draw` can hand over; a `GameUpdateLoop::Draw` ext arm at the existing `Renderer::Draw` seam
(`gameupdateloop.cpp:1414`, next to `EyeCamera::DrawScene`); the stress harness's draw column split into main ms and
render ms. Files touched outside `src/ext`: `mainloop_frame.cpp`, `loops.cpp`, `gameupdateloop.cpp`, `weather_sun.cpp`
(the step-only rule is an ext arm at the call site, not an edit), the harness.

### 1.6 Class and exactness

**Class A.** The sim's inputs are unchanged: no sim step ever runs while render work is alive (the fence), and the sim
reads nothing the render thread writes (§1.3; the one main-side reader of render-written state, `_frameUpdateTime`, is
presentation and wall-clock). The sequence of sim states is therefore the faithful one, step for step, independent of
thread count and timing; what already varies between two faithful runs (pathfinding completion times, PATHRESEARCH
§1.5; the step count per Frame) varies the same way. The presentation of frame N is the same function of (scene state,
camera, alpha, frame time, frame index) as today; alpha and frame time are wall-clock inputs that already differ run to
run. **The faithful path** gets one null test per Frame at the seam and is otherwise object-identical (WALKGRID §3's
objdump method).

**What it does not do.** It does not shorten a step. At speed 50 the step frame still costs the fence wait (up to one
pass 2) + 8.5-13 ms of sim + pass 1; the hitch stays (§0).

### 1.7 Rejected

- **Sim on its own thread, drawing the previous state meanwhile.** Needs every row of §1.2 double-buffered or
  copied per step (transform pairs, poses, pools, bounds, the grid, particles, material parameters, GPU buffers) plus
  deferred destruction of every Geometry, Mesh, Material and GPU object the sim frees while a draw holds it. The heavy
  town's ~55k visible objects per frame (GATHERPF Results) and 0x120-byte MeshInstances put the copy alone at
  megabytes per step; in practice it is a render-proxy rewrite of the engine. Exact and the only thing that removes the
  speed-50 hitch, but not a block-sized job.
- **D3D11 deferred contexts for a parallel pass 2.** `Video::Context` is a CPU-side state cache bound one-to-one to the
  immediate context (`contextdata.cpp:95-130`); several contexts would duplicate the cache and the dirty-bit protocol,
  and deferred-context gains are driver-dependent. Not before rank 1 is measured.

### 1.8 Estimate

Let pass 1 be a fraction `a` of D (the main thread keeps it) and pass 2 + post-scene + the D3D calls `1 - a` (the render
thread). The main thread per second: `S + F x a x D + B`, with `B <= min(60, F') x (1 - a) x D` (at most one fence wait per frame that holds a step; below 60 fps every frame holds one and the pipeline collapses to the sequential frame); the
render thread: `F x (1 - a) x D <= 1000`. So `F' = (1000 - S - B) / (a x D)`.

`a` from the profiles: at speed 10 faithful (IBS_20261013 §1) gather 22% + cull 23% + sort 15% against per-item
transforms 20% + the rest of the draw 9%, i.e. pass 1 ~67% before SORTMEMO and CULLROUTE took ~14% and ~2-8% out of
pass 1; at speed 50 everything-on (S50B §1) the pass-1 rows (`BoxPasses`, `ShadowCasterBox`, `FrustumCull_ContainsFast`,
the SSE intrinsics, `Walk`, `AddDrawItem(s)`) sum to ~17% of the main thread and the pass-2 rows (`AsMatrix`, `Lerp`,
`ParticleInstance::Draw`, `SetTransform`, the lerp pair) ~11%, with ~20% of the draw in rows under 0.5%. Taken: **a =
0.55, range 0.45-0.65.** The first block measures it (§5).

| leg | today: fps / frame ms | S ms/s | D | F' at a = 0.45 / 0.55 / 0.65 | render-thread bound 1000 / ((1-a) D) |
|---|---|---|---|---|---|
| speed 10 heavy faithful (SORTMEMO ab3) | 847 / 1.18 | 162 | 0.97 | 1,844 / 1,520 / 1,296 | 1,873 / 2,358 / 2,941 |
| speed 50 w1, everything + grid | 412 / 2.43 | 510 | 1.2 | 834 / 693 / 596 | 1,515 / 1,852 / 2,381 |
| speed 50 w3, everything + grid | 282 / 3.55 | 630 | 1.2 | 612 / 511 / 442 | same |

That is **+53..+118% at speed 10 and +45..+117% at speed 50** in the CPU-bound regime. Two caveats: nothing in the
tree says whether the GPU keeps up past ~1,400 fps (the compact town has run 1,368 fps, SORTMEMO ab3; the heavy town
never has); and the step-frame hitch at speed 50 is unchanged.
These are THIS BOX's numbers; the 4-core budget's, with break-even and worst case, are in §5.1.

## 2. Parallel cull + gather

### 2.1 The shape per frame

- The faithful walk: `Scene::AddCulledDrawItems` (`cull.cpp:411`), the seam at `:416`, the quadtree DFS `:468`, the
  visible list `PushVisible` (`:517`, `:556`), the serial dispatch `geo->AddDrawItems` (`:586`).
- The extension's: `ExtCulledGather` (`draw_gather.cpp:420`) = `Walk<SideTest>` into `g_lists[depth]` (`:322`, `:451`),
  then the same serial dispatch (`:483`). The walk and the dispatch are already separate phases: the walk produces a
  list, the dispatch consumes it in order.
- It runs once per draw group: 18.07 passes per frame heavy, 30.46 compact (GATHERPF Results), because
  `SunLight::AddDrawItems` / `SpotLight::AddDrawItems` open their own groups and gather (`wave28_lane_partsys.cpp:889-929`,
  CULLROUTE §1). Heavy: 2,059 cells, 149,553 chain objects, 54,691 visible, 54,977 items appended per frame.
- Cost: at speed 50 everything-on the fast cull + walk rows are ~15% of the main thread (S50B §1: `BoxPasses` 3.74,
  `ShadowCasterBox` 2.00, `ContainsFast` 1.69, intrinsics ~6.8, `Walk` 0.73) = ~150 ms/s = **~0.36 ms per frame, ~30% of
  the 1.2 ms draw**, ~20 us per pass on average. The per-frame cost does not depend on speed (it is per Draw).

### 2.2 Census per box / per object (ext walk)

| access | what | during the walk |
|---|---|---|
| read | scene grid: `_cellCounts`, `_cellHeads`, `_nodes[]._object/_next`, `_cellSize`, `_levelCount` | immutable (written by `Clean` / `AddObject`, steps only) |
| read | the `Frame`: two prepared plane tables, `passMask`, `rendererDetail`, `notShadow`, extension vector (`draw_gather.cpp:285-299`) | built before the walk, `const Frame&` |
| read | per object: the side record, or the Geometry's `_materialTypeFlags`, `_detailId`, `_worldBounds` | immutable |
| write | `f.boxes` (`mutable`, `:298`) | per-walk counter |
| write | side records via `StoreFull` on an invalid record (`:389`); **`SideArray::Store` GROWS the vectors** (`gather_side.cpp:16-21`) | per node id, but a growth reallocates for everyone |
| write | `g_counters.nodesTested / geoTouched / refreshes` (`:386`, `:407`, `:94`) | globals |
| write | the visible list (`out.push_back`) | one vector |
| not touched by the walk | `Renderer` item lists, `AddDrawItem`, `RenderTextures`, any Geometry field | dispatch only |

### 2.3 Design: subtree tasks, concatenated in DFS order

The DFS order is fixed by `CullStackPushChildren` (`draw_gather.cpp:193`): children are written 1..4 and child 4 is
popped first, so the order is pre-order: a cell's own chain, then subtree 4, 3, 2, 1. The coordinator walks the top `d`
levels itself (d = 2 or 3: 16 or 64 frontier records with their inherited masks), emitting **segments** in pre-order:
the objects of each interior cell it visits, and one placeholder per frontier subtree. Workers walk the frontier
subtrees (each from its own `CullRecord`, a private `CullStack`, a private output vector, private counters); the
coordinator concatenates segments in order. Each subtree's result is a function of its root record and immutable data,
so **the list is byte-identical to the sequential walk for any K and any timing.**

Required: `side.Store` must never grow during the walk -- size the side array to `_grid._nodes.GetSize()` before the walk
(one resize on the coordinator); records of distinct node ids are disjoint writes. Counters: per task, summed after the
join. The COMPPROF=4 `DrawGather` shadow (list vs retail's reads, `draw_gather.cpp:454-466`) checks the result as it
stands.

**The dispatch stays serial.** `AddDrawItem` appends to one shared `Array` per pass (`wave9_lane_gfx.cpp:397-399`),
reads `_currentDrawGroup`, and on a rendered texture calls `RenderTextures` -> `ReflectionImageBuffer::Render`, which
APPENDS a scene entry and latches `_generation` (`imagebuffer_derived.cpp:655-657`): order-dependent side effects.
Per-thread item arrays concatenated in order would reproduce the non-pad bytes (the pads are stack garbage already,
SORTMEMO §1.4, and the memo masks them), but the dispatch is ~2.6% of the main thread (S50B: `AddDrawItem` 1.15,
`AddDrawItems` 0.68, LOD) -- not worth the reflection-entry re-ordering proof.

### 2.4 Which pool, and what a helper costs

`ext.path_workers`' pool cannot carry frame tasks: one FIFO of path `Job`s that run for milliseconds
(`pool.h:114`), a condition-variable wake (`pool.cpp:45`), a `Barrier()` that waits for EVERY queued job before a grid
edit (`pool.h:94`, `:131`) -- frame tasks would wait behind searches and grid edits would wait behind frame tasks -- and
`kMaxWorkers = 4` (`pool.h:39`). Frame helpers need their own persistent threads (created once, as path_workers' are).

The cost of using a persistent helper parked on an event, per fork-join, on Windows (typical figures, not measured
here): the wake 5-30 us (event or condition variable set -> the parked thread running), the join another 5-30 us when
the main thread finishes its share first and parks, plus the cache migration of what the helper wrote. A helper that
SPINS on an atomic task index instead hands off in ~0.2-1 us, but burns its core for the whole window it spins in.

### 2.5 Break-even and cap (no Amdahl)

**The work.** ~0.36 ms per frame of walk + box tests in the heavy town on this box (S50B §1, ~15% of the main thread at
412 fps), split over **18 passes** (30 in the compact town): **~20 us per pass on average**, the main view pass larger,
the spot / cascade passes smaller. A 4-core part with a lower clock and a 6-16 MB L3 runs the same walk perhaps 1.3-2x
slower (the side array of the heavy town, ~40 B x ~150k nodes = ~6 MB, stops fitting in L3 below ~8 MB), so ~25-40 us
per pass there.

**Event-parked helpers lose at any count.** Wake + join = 10-60 us per pass against 20-40 us of work per pass: the
break-even is not reached for any K. A per-frame wake with spinning inside pass 1 is the only shape that can pay: one
5-30 us wake per frame, then ~1 us hand-offs per pass, the helper spinning through pass 1 (~0.4-0.8 ms per frame).

**Minimum chunk.** A task must carry >= ~5 us of tests (~1 us hand-off, the output segment of `Geometry*` -- <= 8 KB --
and the side records it refreshed migrating back to the main thread, whose `ExtClean` touches them next step): ~200 chain
objects. At ~20 us per pass that allows 4 tasks, so **at most 2 helpers** beside the main thread can be kept busy; a
third waits on the slowest task (the join is per pass).

| helpers (spinning) | saving per frame, this box | notes |
|---|---|---|
| 1 | 0.36 x 0.9 x 1/2 - 18 x ~1 us - one wake ~15 us - imbalance ~20% = **~0.10 ms** (8% of the 1.2 ms draw) | |
| 2 | 0.36 x 0.9 x 2/3 - same overheads = **~0.14 ms** (12%) | the cap |
| 3+ | no further gain | tasks under ~5 us |

**The 4-core answer.** With rank 1 on, pass 1 is exactly when the render thread is busy (that is the overlap), so a
spinning cull helper is the fourth busy thread beside main, render and a path worker. Any other runnable thread (the
D3D driver's own thread, audio, the loader) preempting the helper while it holds a claimed task stalls that pass's join
for up to a scheduler quantum (milliseconds): a frame hitch far larger than the 0.10 ms won. **Expected: same or worse.
Default 0 helpers on 4 cores; 1 on 6; 2 on 8.** With rank 1 off, 1 helper on 4 cores could win ~0.10 ms per frame with
the same preemption risk.

### 2.6 The async sort (rank 3; break-even and cap in §5.3)

The pass-1 sorts (`wave27_lane_gfxbig.cpp:1004-1022`, `EndDrawGroup` `wave29_lane_gfxbody.cpp:656`) are pure functions
of the list bytes and DAZ (SORTMEMO §1.1). Run them all on ONE helper thread in submission order: the memo, its sweep
sequence and every scratch vector (`draw_sort.cpp:108-121`, `:444`) stay single-threaded and see the same sequence, so
the output is identical. Join before pass 2 and before the one event that moves a list: **`BeginDrawGroup` growing
`_drawGroups`** (`wave28_lane_gfxbody.cpp:848`, `SetSizeFill` copies the groups and their arrays) -- rare after
warm-up, but it would free a buffer a pending sort holds. Saving: SORTMEMO took the sort down to ~2.8 MB compared per
frame; what is left is likely under 0.1 ms. Only worth building inside rank 1's split copy, and only if pass 1 is then
the bound.

## 3. Parallel batched walking (`ext.walk_batch_grid`)

### 3.1 Census per batched tick (`walk_batch.cpp`)

| access | what | where |
|---|---|---|
| read (own) | `actionStack.current`, `characterIndex`, the top action's vptr, `_status`, `_next`, `character`; MoveTo `pathIndex`, `pathCount`, `path[]`, `destination`, `description`, `speedMultiplier`, `field_0x474` | `Eligible` `:87-116` |
| read (own) | model translation, yaw; character `+0x2C`, `+0x3C`, `+0x38` | `Step` `:137-145` |
| read (shared, immutable in the iteration?) | `Map::size`, the terrain sidecar byte of the walker's cell | `:108`, `:126` -- **NO**: terrain bytes are written by other components' ticks in the same walk (`SetCellTerrain` writers, PATHRESEARCH §1.1) |
| read (shared) | `RootAnimationOffset()`: the instance's own `+0x100` or the instanced pool slot (`mesh.cpp:1919`) | `:142` -- refreshed by `AnimationManager::Update` before the walk (`gameupdateloop.cpp:779-785`); a `ModelComponent::Tick` in the same walk can (re)assign animation and grow the manager's instance table |
| read (shared) | `GetTerrainPos` in `UpdateFacing`: heights, `floorCount`, the picking scene (`mapquery.cpp:167-210`) | `:133` |
| read (const) | character description fields | `:140` |
| write (own) | model translation, yaw; character `+0x38`, `+0x28`, `+0x2C`, `+0x3C`; MoveTo `pathIndex` | `:129`, `:132-133` (via `UpdateFacing`, `action_base_move.cpp:85`), `:147-150` |
| write (shared) | `g_batchedTicks` | `:253` |
| not reached | the grid (`Place`, kNoPlace), the rotation, the Geometry push, STILLPATH | `:153`, WALKBATCH §1.4 |

So a batched tick writes only its own walker -- but it reads state that other ticks of the same iteration write.

### 3.2 Who reads a walker's position inside the same iteration

WALKBATCH §1.6 and OFFSCREEN §4.1: ~20 reader families read `transform.translation` of another character during the
tick walk -- disease proximity (`citizen_updatehealth.cpp:413`, `:639`), the herd box, hunts, `FindBestTarget`, every
Work start-cell search after a `Clear()`, drop targets, fire ignition, child spawn. Retail's answer for a reader at tick
position r is "walker w's position after w's tick this iteration if w < r, else before". Any scheme that advances
batched walkers outside their slot changes that answer for every reader between the old and the new slot. Add the
terrain bytes and the shared RNG (`GameUpdateLoop::m_random`, consumed in tick order, e.g. `action_base_cnr.cpp:257`):
a walker's `Clear()` by another component earlier in the iteration declines its tick in retail and would not in a
pre-phase.

### 3.3 Verdict

- **Exact: impossible.** The line that breaks it is the reader list of §3.2: walker positions are read inside the same
  iteration by components whose tick order interleaves with the walkers'.
- **Concurrent with the serial walk: rejected** (a data race on positions and terrain; not even deterministic).
- **Class B, deterministic: possible.** A parallel phase at the start of each intermediate iteration that steps every
  walker `Eligible` at that moment (each walker's step a pure function of its own fields + state nothing in the phase
  writes), marking them so the snapshot walk skips their slot. Thread-count-invariant, so two runs with one seed and one
  setting agree; sim-divergent from faithful (readers see positions one slot-order earlier, a `Clear()` in the
  iteration lands one tick later, terrain read at iteration start). It would ride on `walk_batch_grid` (already class B)
  or be a second B row.
- **Money:** the batched rows at speed 50 w1 are ~9% of the main thread (`TickGrid` 2.53, `Eligible` 2.19,
  `WorldToCell` 1.84, `Step` 1.40, `TickCharacterGrid` 1.02) = ~30 us per intermediate iteration. §5.4 sets that against
  a realistic fork-join and the walkers' cache lines migrating back to the main thread each iteration: no break-even can
  be shown, ~0 to +7% fps at speed 50 at best, same or worse on 4 cores. A new phase, a new skip bit in the snapshot
  walk, a new class B semantics to explain in the menu. Not recommended.

## 4. The snapshot walk (~21%)

The ordered tick dispatch IS the sim: every component tick may read and write any other entity (positions, action
stacks, storage, the map) and draws from one shared generator (`GameUpdateLoop::m_random`) in tick order, so the outcome
is a function of the order; any concurrency reorders those reads, writes and draws. Its pure sub-parts are the snapshot
rebuild (an in-order tree copy, only when the tree is dirty), the `TickList` key filter and the staged prefetch
(`entitylist.cpp:504-517`, `:547-601`, `:683`); none is worth a thread: the rebuild is rare, the key filter must read
flags at tick time (a tick can change another component's flags), and a prefetch issued from another core lands in
that core's L1 / L2, not the main thread's, on a thread that is not memory-bound (0.19% miss latency, S50B §1).

## 5. Ranked recommendation (4-core budget first)

### 5.0 The thread budget

The roster on 4 logical cores, the path-worker sizing (today 2 workers on 4 threads, `pool.cpp:10-15`), the per-core
defaults and the sequential fallback are in §7, which is the binding section for the target machine. §5.1-5.4 give each
candidate's detail, 4-core figures first.

### 5.1 Rank 1 -- pipelined emit (`ext.render_pipe`, class A): the only candidate that pays on 4 cores

- **Exactness:** §1.6.
- **(a) Break-even.** It moves pass 2 + post-scene + Present, `p2 = (1 - a) x D` per frame: 0.44 ms (0.34-0.53) at speed
  10 heavy, 0.54 ms (0.42-0.66) at speed 50, ~0.29 ms in the compact town at speed 10 (D 0.65), all on this box. The
  cost per frame: one hand-off (an event set; the render thread's 5-30 us wake is off the main thread's path); the main
  thread's wait for "frame N presented" before handing over N+1 (0 when the render thread finished first, else a 5-30 us
  wake); and migration: the pass-1 lists (~55k items x 56 B = ~3 MB per frame, heavy) are written on the main core and
  read on the render core. They exceed either core's L2, so pass 2 reads them from L3 today as well; the next pass 1
  writes the OTHER Renderer instance, so nothing ping-pongs. Estimate <= ~30 us. Per step, one fence wait of at most the
  unfinished part of a pass 2, never more than the sequential frame would have spent on it.
  **Break-even: p2 >= ~0.06 ms per frame.** Every measured town is 5-10x above it; the menu loop (below it) is never
  pipelined.
- **(b) Cap: 1 helper.** Pass 2 is serial on the one immediate context (§1.4), and the UI after it too; a second helper
  has nothing to do (deferred contexts, §1.7, would be the only way past 1).
- **(c) 4 cores, path workers running.** Busy demand: main 1.0 + render ~0.4-0.7 + path ~0.6-1.0 (one worker, proposed)
  + the driver's own thread: ~2.4-2.8 of 4 cores. It fits, with the path workers below normal. If the render thread is
  starved anyway, the main thread waits at the hand-off and that frame costs today's time + one wake: **worst case =
  today + ~30 us per frame.** The fence term is really `B = min(60, F') x p2` (§1.8 uses 60, valid above 60 fps), so near
  sim saturation (F' < 60, a step in every frame) the pipeline collapses to the sequential frame: **same as now, not
  worse.**
- **(d) Numbers.** 4-core column: this box's rows scaled 1.5x slower per thread (S and D both; a guess for a current
  4-core part, stated as such), a = 0.55, GPU not the bound:

| leg | 4 cores: today -> pipelined fps | this box: today -> pipelined fps |
|---|---|---|
| speed 10 heavy faithful | 518 -> ~900 (+73%) | 847 -> ~1,520 (+80%) |
| speed 50 w1 everything + grid | 131 -> ~188 (+44%) | 412 -> ~693 (+68%) |
| speed 50 w3 everything + grid | ~31 -> ~31 (same: the sim fills the main thread, a step in every frame) | 282 -> ~511 (+81%) |

  The speed-10 gain on a 4-core PC exists only where that PC's GPU is not already the bound at ~500-900 fps; often it
  will be, and then the gain is 0. The speed-50 gain is CPU-side and survives a weaker GPU, until the sim alone fills the
  main thread.
- **Persistent threads:** one render thread created when the row turns on, parked on an event between frames. Nothing
  more.
- **Risks:** a writer missed by §1.2 / §1.3 (`UI::Draw` and `Tool::Draw` were checked for context and camera use, not
  field by field); device-lost re-entry; the device lock moving to the render thread; one frame of latency.
- **Verification:** (a) faithful object identity of every touched TU except the seam's null test; (b) a COMPPROF=4 shadow
  `RenderPipe`: the main thread hashes, at hand-off, the packet's lists (non-pad bytes) and every transform pair / pose /
  particle pool / SunLight record the items reference, and the render thread re-hashes them after pass 2 (a difference
  names a concurrent writer); `RenderPipeFrame` (frame index, alpha, shader detail equal the sequential order's); the
  idempotence of fixes 4-5 (the skipped `SunLight::Set` / `UpdateCamera` would have written the same bytes); (c) the
  existing shadows with the row on; (d) the trace oracle on one frame, on vs off, against its noise floor; (e) the A/B of
  §7 on the 4-CPU mask first, then unrestricted; sim ms / iteration must not move; resave golden unchanged.
- **First block: RENDERPIPE-0, no thread.** The split, the packet, the second Renderer and the ownership rules, run on
  the main thread in the PIPELINED ORDER (pass 1 of N+1, then pass 2 of N; drain before a step). Ordering bugs show in
  the shadows with no race to hide them, and a uProf pass on it measures `a`. RENDERPIPE-1 moves the second half onto
  the thread, together with the path-worker rule and the affinity setting of §7.

### 5.2 Rank 2 -- parallel cull inside ext.draw_gather (class A): 6 cores and up

- Exactness: §2.3. Break-even, cap and the 4-core answer: §2.5. Event-parked helpers lose at any count; one spinning
  helper wins ~0.10 ms per frame on this box, two ~0.14 ms; cap 2; **4 cores with rank 1 on: same or worse, default 0.**
- Persistent threads: MORE than an event-parked pool. The helpers must spin through pass 1 (one wake per frame, then
  spin), ~0.4-0.8 ms of a core per frame each.
- Verification: the `DrawGather` / `DrawGatherSide` shadows; a unit test of the walk on random grids for 1-3 threads
  (list equality); the §7 A/B on 6- and 8-CPU masks, and on the 4-CPU mask to confirm the default 0.

### 5.3 Rank 3 -- async sort (class A): not on 4 or 6 cores

- (a) <= ~0.1 ms per frame left after SORTMEMO, spread over 17-41 sorts (`draw_sort.h:89`): 2-6 us per sort. An event
  per sort (5-30 us) loses outright; only a helper spinning through pass 1 can pay, and it then rewrites the memoised
  lists on its core (the render thread reads them from L3 either way). (b) Cap 1: the memo and its scratch are
  single-threaded by design (`draw_sort.cpp:108-121`, `:444`). (c) 4 cores: worse (no core). Default 0 on 4 and 6;
  build only if an 8-core A/B on top of ranks 1-2 shows pass 1 still the bound. Needs a spinning helper.

### 5.4 Rank 4 -- parallel walk batch (class B): not recommended

- (a) ~30 us of batched ticks per intermediate iteration on this box (~45 us on a slower part), 49 iterations per step
  at speed 50. Event-parked: 10-60 us of fork-join per iteration against ~15-22 us it could move: loses. Spinning
  through the whole step would use the render thread's core, which is idle during a fenced step, so the core exists even
  on 4 cores. But each batched walker's character / model / MoveTo lines (~4 written lines x ~1,000 walkers = ~250 KB
  per iteration) are written on the helper and re-read by the main thread later in the same iteration (the snapshot
  walk, the position readers of §3.2, the last iteration's real tick). That migration is of the same order as the work
  moved, so **no break-even can be shown.** (b) Cap 1 helper. (c) 4 cores: same or worse. Default 0 everywhere; and it
  is class B with no exact form (§3.3).

### 5.5 Not candidates

The snapshot walk (§4); full decoupling and deferred contexts (§1.7).

## 6. Open questions for the coordinator

1. **Rank 1's class.** Presentation-only and sim-exact, so class A (live toggle, drained at a Frame boundary) as
   proposed, or does the user want a new threading feature locked per game regardless?
2. **One frame of extra latency** (0.6-1.7 ms here, ~1-8 ms on a 4-core part at its frame rates): acceptable?
3. **The speed-50 hitch.** No exact parallel scheme removes the 8.5-13 ms step block (more on 4 cores). The options are
   the render-proxy decoupling (§1.7, very large) or drawing between sim iterations (a presentation change, sim
   untouched). Does the user want either researched?
4. **Thread counts.** Registry rows are bools (`registry.h:68-85`). The §7.4 rules can stay derived (no setting), or
   become numeric `settings.cfg` keys with the §7.4 defaults. Which?
5. **Path workers on 4 cores.** Proposed: count the affinity mask instead of `hardware_concurrency`, one fewer worker
   when the render thread is on (2 -> 1 on 4 cores), BELOW_NORMAL priority. It changes only when path results land
   (class B timing). Agreed?
6. **Logical vs physical.** The target is 4C / 4T. On a 4C / 8T part the rules would count 8; should they count physical
   cores (`GetLogicalProcessorInformation`) so a helper never shares a core with the main or render thread?
7. **A ThreadSanitizer flavour of the Linux build** (CMake already builds `reimpl-x64` there) would check rank 1's race
   freedom in the stress town. A build-system addition: allowed?
8. **GPU headroom.** Rank 1's speed-10 gain depends on the GPU keeping up; the 4-core target's typical GPU probably does
   not at 500-900 fps. Is one RenderDoc pass-timing capture (existing tool) enough to bound it on this box, and is the
   speed-50 (CPU-side) gain the headline for the user?

## 7. The 4-core budget

The design target is a 4-core / 4-thread machine (the user, via the coordinator). The rule taken from the user: a scheme
does the same as now or worse with fewer cores (thread overhead), and is capped with more; no Amdahl extrapolation. n
below is the popcount of the process affinity mask, not `std::thread::hardware_concurrency()` (which ignores affinity).

### 7.1 The thread roster on 4 logical cores

`ext.path_workers` today: `ChooseWorkers(hardware_concurrency)` = n - 2 clamped to [1, `kMaxWorkers` = 4]
(`pool.cpp:10-15`, `pool.h:39`, called once per game at `path_workers.cpp:223`). On 4 threads that is **2 workers**, not 4:
the clamp alone does not oversubscribe a 4-core machine. Workers park on the pool's condition variable between jobs
(`pool.cpp:45`); the dispatcher (the retail `PathfinderThread`) parks when its queue is empty. All `System::Thread`s run at
NORMAL priority (`thread.cpp:146`).

| configuration | threads that can be busy during a frame | spin | park | busy-capable on 4 cores |
|---|---|---|---|---|
| faithful | main; the retail pathfinder worker | -- | pathfinder (empty queue), loader | 2 |
| everything on today | main; 2 path workers | -- | dispatcher, workers between jobs, loader | 3 |
| + rank 1 as first designed | main; **render**; 2 path workers | -- | render between frames (event) | **4 + the driver's thread: oversubscribed in a path burst** |
| + rank 1, proposed | main; render; **1** path worker at BELOW_NORMAL | -- | as above | 3 |
| + rank 2 (cull helpers) | + 1-2 helpers spinning through pass 1, i.e. while the render thread is busy | helpers | helpers between frames | 4-5: **no core left** |
| + ranks 3 / 4 | + 1 spinning helper | helper | | no core left |

So on 4 cores only rank 1 fits, and only with one path worker (or with two at lower priority).

### 7.2 Break-even, cap and the 4-core outcome per candidate

Fork-join cost assumed on Windows (typical, not measured): event / condition-variable wake 5-30 us, the join the same
when the main thread finishes first and parks, plus the migration of what the helper wrote. 4-core numbers scale this
box's by 1.5x per thread (a guess, stated as one); they are CPU-bound figures.

| candidate | work it moves per frame | break-even | cap (threads) | 4 cores, path workers running |
|---|---|---|---|---|
| 1. render pipe | pass 2 + UI + Present: 0.29-0.54 ms here (0.45-0.8 ms on 4 cores) | ~30 us per frame (one wake on the critical path, ~3 MB of lists read from L3 as today): pays above **p2 ~0.06 ms** | **1** (one immediate context) | **better**: speed 50 w1 ~131 -> ~188 fps (+44%); speed 10 heavy ~518 -> ~900 (+73%) only if the GPU is not the bound (often it will be: then 0). In a frame where a path burst takes the render thread's core: today's cost + one wake. Near sim saturation (a step every frame): **same as now** (§5.1) |
| 2. parallel cull | ~0.36 ms per frame in 18 passes = ~20 us per pass (~30 us on 4 cores) | event-parked helpers: 10-60 us per pass > the work: **never**. One wake per frame + spinning: tasks >= ~5 us | **2 helpers** (a pass splits into <= 4 tasks of >= 5 us) | with rank 1 on: **worse** (the spinner takes the render thread's or the path worker's core; a preempted helper stalls the pass join by a scheduler quantum, milliseconds). Rank 1 off, 1 path worker: ~0.15 ms per frame (~10% of draw) at best, same preemption risk |
| 3. async sort | <= 0.1 ms over 17-41 sorts (2-6 us each) | event per sort: never; spinning helper only | 1 | **worse** (no core) |
| 4. parallel walk batch (class B) | ~30-45 us per intermediate iteration | fork-join per iteration loses; spinning on the render thread's idle step core is possible, but ~250 KB of walker lines per iteration migrate back to the main thread: **no break-even shown** | 1 | **same or worse** |

**Re-rank for 4 cores:** 1 render pipe (pays); then nothing. Ranks 2 and 3 are 6- and 8-core features and run the
sequential path below that; rank 4 is not recommended anywhere. The 8-core order is the same, but on 4 cores rank 1 must
come with the path-worker change, or it competes with a worker for a core in every path burst.

### 7.3 Measuring a 4-core machine on this box

**A settings row, not `start /affinity`.** `stress_run.sh` launches the exe directly from bash (`stress_run.sh:279`) and
`tools/build.sh run` does likewise; `start /affinity` would need a `cmd` wrapper in each script, and, worse, it leaves
`ChooseWorkers` reading 16 from `hardware_concurrency`, so the path-workers leg would run 4 workers on 4 CPUs, a machine no
player has. A `settings.cfg` key (e.g. `sys.cpu_limit`), read once at boot like the `ext.*` keys, applies
`SetProcessAffinityMask` in platform startup before any thread exists (loader, audio, pathfinder, workers, render
thread), makes every thread-count rule of §7.4 read the mask, and is printed in the stress report header and the
`[BootState]` line so each leg proves its mask. It also works on Linux (`sched_setaffinity`). Not a `BANISHED_*` flag;
never `getenv`.

**Two 4-logical masks, both measured:** P4 = four physical cores, one thread each (CPUs 0, 2, 4, 6 if the SMT siblings
are adjacent, as Windows usually numbers them; the block checks with `GetLogicalProcessorInformation`): the 4C / 4T
target. S4 = two physical cores with both SMT threads (CPUs 0-3): a 2C / 4T machine, where the render thread shares a
core with the main thread and the gain should be smaller. The 9800X3D has one CCD, so every mask shares the L3.

**Legs** (CLEAN machine, overlays stopped, interleaved x2 as usual): faithful vs faithful + `render_pipe`, and
everything on vs everything on + `render_pipe`; heavy and compact towns; speed 10 and 50; at P4, S4, then unrestricted.
Read fps, main-thread draw ms, render-thread ms, the hand-off waits per frame (a counter in the existing report), and sim
ms / iteration (must not move with `render_pipe`). A rank-2 build adds its helper count (0 / 1 / 2) as a dimension, and
the P4 legs must confirm that 0 is the right default there.

**What the mask cannot emulate,** to be stated next to every masked number: the clock (faster here per thread), the L3
(96 MB here against 6-16 MB on a 4-core part; the heavy town's pass-1 lists ~3 MB plus the draw_gather side array ~6 MB
stop fitting below ~8-12 MB, so miss and migration costs read low here), and the GPU (stronger here: the speed-10 gain
reads high).

### 7.4 Default thread-count rows per core count

| row (settings key) | rule | n = 4 | n = 6 | n = 8 |
|---|---|---|---|---|
| `ext.render_pipe` (bool registry row, class A, default OFF per DESIGN §2) | when on and n >= 3: one render thread; n < 3: the sequential path | 1 | 1 | 1 |
| path workers (count) | clamp(n - 2 - (render_pipe ? 1 : 0), 1, 4); BELOW_NORMAL priority | 1 (2 with render_pipe off) | 3 (4) | 4 (4) |
| cull helpers (count, rank 2) | clamp(n - 5, 0, 2) with render_pipe on, clamp(n - 4, 0, 2) off; 0 = the sequential walk | 0 | 1 | 2 |
| sort / walk helpers | not built | 0 | 0 | 0 |

**The fallback rule:** a machine with fewer logical CPUs than a feature's roster needs gets that feature's sequential
path -- no thread is created and the faithful-shaped body runs -- and the boot log says so once. Whether the counts are
derived (no key) or numeric `settings.cfg` keys with these defaults is open question 4; whether n should count physical
cores is open question 6.
