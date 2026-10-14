# RENDERPIPE1 -- `ext.render_pipe` (class A): the packet on a render thread

Block RENDERPIPE-1, 2026-10-14 (THREADPLAN.md section 2; THREADRESEARCH.md sections 1.4, 1.5, 5.1, 7.4;
RENDERPIPE0.md section 12 was the work list). Static gate only: the game was NOT run by this block.

## 0. Short version

- With `ext.render_pipe` on and `Cpu::RenderThread(Cpu::Get().physical)` true (>= 3 physical cores), the
  packet (pass 2, the post-scene draws, the post-draw callbacks, EndCommands, the frame index, Present) runs
  on ONE persistent render thread. Fewer cores: RENDERPIPE-0's sequential split, unchanged, and the boot
  log says once `[RenderPipe] n physical cores: sequential split`.
- Depth 1. At a pipelined Frame tail the main thread runs the main half of N+1 (concurrent with N's emit),
  WAITS until N is presented, then publishes N+1. THE FENCE (`kExtBeforeSteps`) and every RENDERPIPE-0
  drain are "wait idle, then run the frame as today".
- The device lock moves with the emit (BeginCommands .. FinishPresent on the render thread). A failed
  Present is only REPORTED by the render thread; retail's recovery runs on the MAIN thread at the next
  join, and the packet gathered meanwhile is discarded.
- **Design changes beyond the brief (section 9):** `ContextData::Finish` had to be COPIED and cut in two
  (it runs the recovery inline); `FrameUpdateTime` takes the Frame start from the packet (the main thread
  rewrites the live field every Frame); `StressHarness_Verify` needed a lock; `FindRoadPath` does not
  touch a map scratch at all (it queues on the pathfinder and spins, section 6).
- Unit 1164 -> 1168 at the branch (+4); 1172 after merging main (CULLPAR's +4). Warnings 0 new (debug,
  perf). castmetric 2, abi_audit OK, acceptance 0 off target. No faithful TU changed (section 8).

## 1. The thread's life cycle

| when | what | where |
|---|---|---|
| install (`RenderPipe::Sync`, boot and every toggle, row on, not installed) | `g_second` Renderer; `g_driver.Reset()`; if `Cpu::RenderThread(physical)`: `new Handoff`, `g_driver.Attach`, `new RenderThread` (System::Thread subclass, 1 MB stack, NORMAL priority from the base ctor), `Start()`; boot line `[RenderPipe] render thread up (n physical cores)`. Else sequential, one line | `render_pipe.cpp:581` |
| between packets | `Run` = `Driver::WorkerLoop`: `Handoff::WaitJob` parks on a condition variable (no spin) | `pipe_driver.h:109`, `handoff.h:75` |
| toggle off | the next Frame tail: `PlanTail` drain (wait idle), faithful tail, `Uninstall` -> `StopThread`: `Handoff::Stop`, `delete g_thread` (~Thread signals hStop and joins), `delete g_handoff`; line `[RenderPipe] render thread joined` | `render_pipe.cpp:543` |
| process exit with the row on | the thread stays parked; every sync object is HEAP-allocated (no static destructor runs on a waited condition variable at exit) | |

## 2. Hand-off, fence, memory ordering

`handoff.h` (one job slot, `std::mutex` + two `std::condition_variable` + `std::atomic<bool> _busy`) and
`pipe_driver.h` (`Driver<Host>`, engine-free; `PipeState` unchanged).

| pair | release side | acquire side |
|---|---|---|
| publish | main writes `_held` (`pipe_driver.h:170`), then `Publish`: under the lock `_ready = true`, `_busy = true` (relaxed), **unlock = release** (`handoff.h:47-49`) | render `WaitJob`: **lock = acquire**, sees `_ready` (`handoff.h:76`) -> reads `_held` |
| idle | render: emit, Finish, `_result` written (`pipe_driver.h:111`), then `Done`: under the lock **`_busy.store(false, release)`** (`handoff.h:88-89`) | main `WaitIdle`: **`_busy.load(acquire)`** fast path, else lock + wait on the same predicate (`handoff.h:56-58`) -> `_result` and every emit write (the context, `_frameUpdateTime`, SaveLoad's capture flags, the road tool's state, the pathfinder's staged fields) happen-before what main runs next |
| stop | main (render idle) sets `_stop` under the lock (`handoff.h:67-68`) | `WaitJob` returns false |

The join sites, each followed by main-thread code that may write what the emit reads:

| site | wait | then |
|---|---|---|
| THE FENCE, `kExtBeforeSteps` (only when the Frame steps) | `Driver::BeforeSteps` -> `Join` (`pipe_driver.h:83`) | the steps |
| a transition drain (RENDERPIPE0 section 5: toggle off, loop change, quit, resize, PaintTerrain, eye camera, screenshot, diagnostic) | `Tail` -> `plan.drain` -> `Join` | the faithful tail (`MainLoop::Draw` on the main thread, device lock on the main thread, no emit alive) |
| THE HAND-OFF, a pipelined tail with a packet pending | after `MainHalf(N+1)`: `Join` (`pipe_driver.h:166`) | `AfterJoin` (camera shadow), publish N+1 |

Counted: `waits` (joins that blocked), `waitsFence` (of which not at the hand-off), `waitUs`, the packet
latency `latencyUs / latencyN` (publish -> `NowUs()` after Finish on the render thread).

## 3. The device lock

| thread | takes it |
|---|---|
| render | `BeginCommands` (`contextdata.cpp:421`, faithful) .. `FinishPresent`'s `Unlock` (`render_pipe_finish.cpp:69`) -- the whole emit |
| main, main half | none. Pass 1 creates no GPU object: re-checked `Renderer::AddDrawItem` -> `MaterialInstance::RenderTextures` -> `ReflectionImageBuffer::Render` (`imagebuffer_derived.cpp:609`: a camera mirror and one scene-entry append, no device call), `ParticleInstance::AddDrawItems`, `MeshInstance::AddDrawItems`, the game's `WorldGeometry` / `WeatherParticleGeometry` / `StatusIconGeometry::AddDrawItems`: no `Lock`, no `Create`, no Map |
| main, steps | the Update-side users (`minimap_upload.cpp`, `serialize_image_upload.cpp`, `moddialog.cpp`, `gameupdateloop.cpp:1206`): all inside steps, after the fence |
| main, faithful frames | `MainLoop::Draw`, render idle |
| loader thread | unchanged (it already shared the lock with the main thread) |

## 4. Device lost

Retail's `ContextData::Finish` runs `onVideoError` (MainLoop::OnVideoError: every video External
Destructed, `_currentLoop->OnVideoError`, the Context destroyed), the device recreate and `onVideoRestart`
INLINE. On the render thread that would destroy what the main thread's concurrent pass 1 is reading. So:

- `src/render/dx11/render_pipe_finish.cpp` (NOT RETAIL, the dx11 dir only because the D3D headers are
  visible there on Windows and dxvk-native): `FinishPresent` = Finish steps 1-4 (Present, Unlock, the
  forced-loss drive, the two DXGI tests, GetDeviceRemovedReason) -> 0 or the hr; `FinishRecover(device, hr)`
  = Finish step 5 from the `onLost` read, statement for statement, with the same `[DeviceLost]` lines
  (`device-lost round trip COMPLETE` included). The faithful `Finish` is untouched and still finishes every
  sequential frame and the whole sequential split.
- Render thread: `EngineOps::Finish` calls `FinishPresent`; a nonzero hr -> `EmitResult{lost, recover, hr}`.
- Main thread, at the next join (hand-off or fence): `Host::Recover` -> `FinishRecover` with the render
  thread idle, i.e. where retail runs it (MainLoop::Draw's tail on the main thread, nothing else in flight;
  `OnVideoError`'s Externals teardown runs on the thread that owns the loop). At the hand-off the packet
  gathered meanwhile (against the old context) is discarded (`PipeState::Discard`); at the fence nothing was
  gathered. The next frame has nothing pending (renderIdle), so fixes 4/5 re-run. The thread survives (it
  never left `WaitJob`'s loop).
- `BANISHED_FORCE_DEVICE_LOST=N`: the faithful counter is file-local to `contextdata.cpp`, so the copy keeps
  its own: it fires once, on the Nth PACKET Present (boot line `[DeviceLost] ARMED (render thread)`). With
  the row on from boot the faithful counter only counts the menu / load / transition frames and stays far
  below N; a run therefore makes one round trip (if the run is long enough for the faithful count to reach
  N as well, it would make a second: read the `INJECTING` lines).
- BeginCommands failure (settingsChanged): unchanged, retail's DrawFailed path on the emitting thread; the
  new packet is discarded (cannot happen: resize frames are unpipelined).

## 5. Fixes 4 / 5 and RenderPipeIdem

"Nothing held" = nothing pending = the render thread joined (the acquire of section 2): `SunLight::Set` and
`UpdateCamera` run in `MainHalf` only then. The camera half of RenderPipeIdem no longer runs inside the main
half: `Driver` calls `Host::AfterJoin` after the hand-off join on every kept skipping frame, and
`IdemCamera` runs the real `UpdateCamera` + byte restore there, render thread idle. Nothing between the main
half and the join writes UpdateCamera's inputs (camera transform: steps; back buffer: resize frames, never
pipelined). The sun half stays in MainHalf (it only reads the sun; the render thread only reads it too).

## 6. `RoadTool::Draw` -> `Map::FindRoadPath` on the render thread

Verdict: **no shared search scratch; safe by the fence.** `FindRoadPath` (`keystone_a_zone.cpp:248`) does
not search: it sets the pathfinder's staged step costs / search flags / passable types (each a compare + a
command queued under `pf->lock`), `RequestPath` (AllocResult under the lock), then spins on
`results[slot].ready` with `Application::YieldToThreads` (SwitchToThread) until the pathfinder thread or
the path-worker pool (`path_workers.cpp`: each worker its OWN `GridPathSearch` / `FastPaths::Searcher`
scratch, publish under the lock) answers. The staged fields and the results table are touched by the
requesting thread; the only other requester is the main thread, and it requests only inside steps (the sim,
the tools' Update, `roadtool_calcpath.cpp`), never in the main half -- so the two requesters are separated by
the fence. The road path stays an emit-side call (no value recording needed). Its wait is now on the
render thread (the main thread waits on it at the hand-off, as retail's main thread waited inside Draw).

## 7. Harness, roster

- `stress_harness.cpp`: `PipePacket*` / `PipeEmit*` window sums are atomics folded at `EmitRow` (the
  packet share joins `draw_ms_mean` there, the same span as before); `StressHarness_Verify` and the
  `[VERIFY]` report under a mutex (the render thread reports RenderPipe, RenderPipeFrame, SkinBones).
  Appended CSV columns `pipe_wait_us` (all blocking joins per draw frame) and `pipe_latency_us` (mean
  hand-off -> Present done); `[Stress::pipe]` gains `thread= wait=us/frame latency=us waits= (fence ..)
  recoveries=`. `tools/stress_report.py` prints a second `pipe` line.
- Roster: `path_workers.cpp` `SelectForGame` passes the ACTIVE `Ext::Ext().render_pipe` to
  `Cpu::PathWorkers`; the `[PathWorkers] selected` line prints `render_pipe=`. 8 physical: 4 either way;
  `sys.cpu_limit=0x55` (4 physical): 2 -> 1. Latched at game select (a mid-game toggle applies to the next
  game's pool).
- CULLPAR (merged meanwhile, coordinator note): `DrawGather::SyncThreads` passes the active
  `Ext::Ext().render_pipe` to `Cpu::CullHelpers` (8 physical: 2 either way; 6: 2 -> 1). `Sync` runs in
  `SetAndRecord` after `SetRequested` (the active value is already the new one) on every class A toggle, from
  the UI pump inside a step (render idle, no pass 1). The `_extDrawPass1End` park call stays at the end of
  `RenderPipe::Pass1`, on the main thread.

## 8. Proofs and gates

- Object proof (WALKGRID section 3's method, `objdump -d -r --no-show-raw-insn -C` per function, addresses
  dropped): perf built at the base `b9026f6ff` before the first edit, objects kept; after the edits the perf
  build recompiled 23 faithful TUs (all through `stress_harness.h`'s comment): **every one identical per
  function** (action_base_move, action_pf_astar, action_pf_region, action_pf_roots_{misc,road,route},
  keystone_a_zone, moveto, cemeterycomponent, citizen_lifeevent_handler, citizenai_executor,
  particlecomponent, pasturecomponent_states, quarrycomponent_deform, statemachinecomponent,
  gamegrid_serialize, entitylist, gameupdateloop, maptilegrid, terrain_sidecar, keystone_a_removalcalcpath,
  starttown, main). `contextdata.cpp`, `loops.cpp`, `mainloop_frame.cpp`, `wave27_lane_gfxbig.cpp` were not
  recompiled. The detector was checked on `render_pipe.cpp.obj` / `path_workers.cpp.obj` (differ, as they
  must). After merging main (CULLPAR) the diff against main touches no faithful file except that header
  comment.
- `tools/build.sh build debug / perf --jobs 2`: exit 0 before and after the merge; 0 new warnings against
  `warn_baseline_{debug,perf}.txt` (normalised, path prefix stripped). `unit debug`: 1168 / 0 at the branch
  (1164 + 4), **1172 / 0** after the merge. castmetric 2, abi_audit OK, acceptance 0 off target.

## 9. Findings / deviations

1. **Finish copied.** The brief's design (render thread reports, main recovers) is impossible without
   cutting `ContextData::Finish`: the recovery is inline. The copy is the ext's (as `Renderer::Draw`'s two
   loops are); the faithful Finish is untouched. Its forced-loss counter is separate (section 4).
2. **`_frameStartTicks` race.** `MainLoop::Run` writes `_frameStartTicks` at every Frame start, outside the
   hook, while the render thread's `FrameUpdateTime` read it. The threaded packet carries the start of the
   Frame that gathered it, so `_frameUpdateTime` = Present done - start of that Frame (retail's formula over
   the packet's own frame; it includes the one-frame latency, so it is larger than RENDERPIPE-0's
   start-of-the-next-Frame value). It only scales `SetTime`'s interpolation term (shader time). The
   sequential split keeps the live read.
3. **Fix 2 / modelIndex** derived from the pending packet, never from the context while the render thread
   may be incrementing it.
4. `StressHarness_Verify` was not thread-safe (now a mutex). Diagnostic counters written by pass 2
   (SkinMemo's /skin row, DRAWSORT's SetSizeCore counter, which counts only the marked draw thread) are now
   the render thread's: the /skin row's numbers are read unlocked at the report (64-bit aligned, diagnostic),
   and the SetSizeCore counter no longer sees pass 2.
5. `debugui.cpp:720` reads `_frameUpdateTime` on the main thread while the render thread may write it
   (a debug display value). `modal_driver.cpp` appends to `MainLoop::_postDraw` when it arms (boot, menu: the
   row is not pipelining then); a late arm with the render thread up would race the emit's iteration.

## 10. For the coordinator (could not be proven statically)

1. **Shadows with the thread on**, the same four runs as RENDERPIPE0's Results (heavy / compact, speed
   10 / 50, `BANISHED_STRESS_COMPPROF=4 STRESS_EXT=render_pipe`): RenderPipe, RenderPipeFrame,
   RenderPipeIdem and every existing shadow at 0; `[Stress::pipe] thread=1`; `discarded=0`,
   `recoveries=0`. RenderPipe's "pre" hash now runs on the render thread concurrently with pass 1 of N+1:
   a mismatch there is a real race (a finding, not noise).
2. **Forced device lost**: `BANISHED_FORCE_DEVICE_LOST=12000` with the row on: one `INJECTING ... render
   thread` line, `[RenderPipe] device lost in the render thread's Present`, one `round trip COMPLETE`,
   `recoveries=1`, `discarded` 0 or 1, the game keeps drawing (the thread survives), clean quit.
3. **A/B legs** (THREADPLAN section 4): faithful vs + render_pipe, everything on vs + render_pipe, heavy /
   compact, speed 10 / 50, at P4 / S4 / unrestricted; read `pipe_wait_us`, `pipe_latency_us`, main draw
   ms, sim ms per iteration (must not move). At P4 check `[PathWorkers] ... workers=1 ... render_pipe=1`.
   S4 = 2 physical: expect `sequential split`.
4. **Exclusive fullscreen** (unprovable here, a DXGI rule): Present now runs on a thread that does not own
   the window. In exclusive fullscreen DXGI may SEND messages to the window during Present (mode change,
   alt-tab); the main thread is then parked in the hand-off wait and does not pump -> a possible deadlock
   (the hang watchdog would report exit 4). Test alt-tab in and out of exclusive fullscreen with the row on.
   Windowed / borderless is the expected safe case. If it hangs, the fix is a message-aware wait on the main
   thread (dispatch sent messages only), not a change to the emit.
5. A toggle off -> on in one session (thread joined, a new one started); a return to menu and a load with
   the row on (the drain at the loop change; slot 1 survives); quick save / quick load frames (unpipelined).
6. The road tool dragged over a long path with the row on (FindRoadPath's spin now on the render thread;
   the hand-off wait absorbs it).
