# RENDERPIPE0 -- `ext.render_pipe` (class A): the split frame in the pipelined order, no thread

Block RENDERPIPE-0, 2026-10-14 (THREADPLAN.md section 2; THREADRESEARCH.md sections 1.5, 1.6, 5.1).
Static gate only: the game was NOT run by this block. The coordinator runs the COMPPROF=4 shadows
(heavy + compact, speed 10 + 50) and the uProf pass that measures the pass-1 share `a`.

## 0. Short version

- New registry row `ext.render_pipe` (Tier::Extension, Class::A, default OFF, formatVersion 0,
  "Render thread"). On, every game frame is split where `Renderer::Draw`'s pass 1 ends and run on
  the main thread in the PIPELINED ORDER: the main half of frame N+1 (GameUpdateLoop::Draw steps 1-6
  and pass 1, into the free one of two Renderers), then the held packet of frame N (pass 2, the
  post-scene draws, the post-draw callbacks, EndCommands, `++_frameIndex`, Present), then N+1 is
  held. Before any fixed step the held packet is emitted (the fence), so the sim never runs with
  render work pending. No thread, no event, no fence primitive.
- ONE engine seam: `Engine::MainLoop::_extFrame` (null by default), tested twice per Frame outside
  the step loop (before it, and at the tail). The faithful `Renderer::Draw`, `MainLoop::Draw`,
  `GameUpdateLoop::Draw` and `Weather::DrawUpdate` are not edited at all (section 7: their objects
  are identical); `MainLoop::Frame` differs by the two null tests.
- **Deviation from the brief, on purpose:** the split is selected at the MainLoop level (the Frame
  tail), not by an arm at `GameUpdateLoop::Draw`'s `Renderer::Draw` site. An arm there cannot defer
  steps 8-13, `MainLoop::Draw`'s tail (EndCommands / `++_frameIndex` / Finish) or record the
  prelude's context writes without arms at steps 2, 4, 5 and an early return; the ext instead holds
  a COPY of the prelude (as it holds a copy of `Renderer::Draw`'s two loops), so `GameUpdateLoop::Draw`
  is object-identical with no arm at all.
- Findings beyond THREADRESEARCH sections 1.2 / 1.3 (section 5): the quicksave / quickload
  screenshot flags are written by the emit side (`TakeScreenshot` clears them) and read by the next
  main half -> such a frame runs unpipelined; `_frameUpdateTime` is read at emit (not recorded) so
  SetTime sees what retail's sees; `RoadTool::Draw` runs `Map::FindRoadPath` on the emit side; the
  fix-5 rule must be "nothing held", not "first frame after a step" (a resize changes UpdateCamera's
  input without a step); `debugui.cpp:720` reads `_frameUpdateTime`.

## 1. The frame as built

Retail's frame (THREADRESEARCH 1.1) and where each statement goes with the row on:

| retail statement | where | with `ext.render_pipe` |
|---|---|---|
| Frame tail `_time` store + dirty 0x800 | `mainloop_frame.cpp` tail | **packet** (`frameTime`), replayed first |
| `BeginCommands` (device lock, resize path) | `loops.cpp` MainLoop::Draw | **emit**; failure runs retail's `DrawFailed` / `_contextDirty = 0` / `Reset` and the new packet is discarded |
| `_contextDirty` -> `ClearResources` | MainLoop::Draw | **emit**, read live (only steps and the emit write `_contextDirty`) |
| viewport reconcile | MainLoop::Draw | **emit**, read live (the default buffer's extent changes only in BeginCommands' resize path / device-lost, both emit-side) |
| RenderDoc `OnFrame` | GUL::Draw top | **emit** (brackets the GPU frame it belongs to) |
| step 1 pause -> alpha 1 | GUL::Draw | main half; `alpha` into the packet |
| step 2 `SetTime(game + frameUpdate x alpha)` | GUL::Draw | main records `m_gameTime`; **emit** computes with `_frameUpdateTime` read at emit (written by the previous emit, as retail's SetTime reads the previous Draw's) |
| step 3 `PaintTerrain` | GUL::Draw | never pipelined: that frame drains and runs faithful |
| step 4 `UpdateCamera` | GUL::Draw | main half, **only when nothing is held** (fix 5); else skipped (+ RenderPipeIdem) |
| step 4' `EyeCamera::ApplyFrame` | GUL::Draw | never pipelined (drain, faithful frame) |
| step 5 DrawUpdate: four floats + dirty 0x10800, `SetFog` | weather_sun.cpp | main computes (BlendLighting), **packet** values, replayed by the emit |
| step 5 DrawUpdate: `SunLight::Set` | weather_sun.cpp | main half, **only when nothing is held** (fix 4); else skipped (+ RenderPipeIdem) |
| step 5 snow diagnostic | GUL::Draw | **packet** (`forceSnow`, `snow`), replayed after the floats |
| step 6 `_shaderDetail` | GUL::Draw | main half, on the instance about to run pass 1 (fix 1); value into the packet |
| `SceneWatch_LodCensus` / `_DrawItemCensus` [diag] | GUL::Draw | main half (the census reads the pass-1 Renderer) |
| step 7 pass 1 | `Renderer::Draw` to :1030 | main half, `passes.cpp` `Pass1` (copy) with the main-owned frame index (fix 2) |
| step 7 pass 2 | `Renderer::Draw` from :1030 | **emit**, `passes.cpp` `Pass2` (copy) inside the SkinMemo scope (fix 6) |
| step 8 `SaveLoad::Draw` | GUL::Draw | receiver + material recorded; **emit** draws (`_captureFlags` is read and cleared inside it, emit-side) |
| step 9 / 13 quicksave / quickload `TakeScreenshot` | GUL::Draw | never pipelined (section 5, finding 1) |
| step 10 UI camera matrices | GUL::Draw | main computes `AsMatrix` + projection, **packet** values; emit `SetCamera` |
| step 11 `Tool::Draw` | GUL::Draw | tool pointer + flag recorded; **emit** draws |
| step 12 `UI::Draw` / `SharpUI::Draw` | GUL::Draw | `sharp_ui` recorded; **emit** draws |
| post-draw callbacks, `EndCommands`, `_frameUpdateTime`, `++_frameIndex`, `Finish` | MainLoop::Draw | **emit**, in that order |

The emit order is `packet.h`'s `Emit<Ops>` template (one function, the engine's `EngineOps` in
`render_pipe.cpp`, a recording `Ops` in the unit test that pins the order).

**Packet fields** (`src/ext/render_pipe/packet.h`): `game`, `renderer`, `slot`; `frameIndex`,
`modelIndex`, `alpha`, `shaderDetail`; `frameTime`, `gameTimeTicks`, `weather[4]`, `fogNear`,
`fogFar`, `fogColor[4]`, `forceSnow`, `snow`; `saveLoad`, `uiMaterial`, `view[16]`, `proj[16]`,
`tool`, `toolFlag`, `sharpUi`; `hashed`, `hash` (shadow).

## 2. Files

| file | what |
|---|---|
| `src/ext/render_pipe/packet.h` | the packet (values only) and `Emit<Ops>`, the second half's order |
| `src/ext/render_pipe/pipe_state.h` | `PipeState`: the drain state machine and the slot ping-pong (pure) |
| `src/ext/render_pipe/passes.cpp` | `Pass1` / `Pass2`: `Renderer::Draw`'s two loops, copied and cut |
| `src/ext/render_pipe/render_pipe.cpp` | the hook, the main half (copy of GUL::Draw steps 1-6 + post-scene recording), `EngineOps`, the drains, the Idem / Frame shadows, `Sync` |
| `src/ext/render_pipe/shadow.cpp` | `HashFrame`: the RenderPipe shadow's field-by-field hash |
| `src/ext/render_pipe/render_pipe.h`, `render_pipe_internal.h` | the public `Sync` / `Installed` / `TakeWindow`; the TU-shared declarations |
| `src/engine/engine/MainLoop.h`, `mainloop_frame.cpp` | THE seam (section 3) |
| `src/ext/registry.h`, `registry.cpp`, `registry_boot.cpp` | the row; `RenderPipe::Sync()` beside `DrawGather::Sync()` (boot and every toggle) |
| `src/diag/stress_harness.h/.cpp`, `tools/stress_report.py` | the split draw column (section 8) |
| `tests/unit/ext_render_pipe_test.cpp` | section 9 |

Not touched: `wave27_lane_gfxbig.cpp` (`Renderer::Draw`), `loops.cpp` (`MainLoop::Draw`),
`gameupdateloop.cpp` (`GameUpdateLoop::Draw`), `weather_sun.cpp` (`Weather::DrawUpdate`). No
private member blocked the copies (every Renderer / Context / MainLoop / GameUpdateLoop field the
copies use is public), so no friend or accessor was added.

## 3. The engine seam

`src/engine/engine/MainLoop.h` (after `_singleton`): `static ExtFrameFn _extFrame;` with
`using ExtFrameFn = void (*)(MainLoop&, int32_t point, float alpha, float time);` and the two
points `kExtBeforeSteps = 0`, `kExtFrameTail = 1`. Defined null in `mainloop_frame.cpp` (before
`Frame`). Two tests, both outside the step loop, once per Frame each:

- after `_quit = 0`, before `while (_accumulator >= _deltaTime)`:
  `if (_extFrame != nullptr && _accumulator >= _deltaTime._ticks) _extFrame(*this, kExtBeforeSteps, 0, 0);`
  -- the drain (only called when the Frame will step);
- the tail: `if (_extFrame != nullptr) _extFrame(*this, kExtFrameTail, alpha, v); else { the four
  _time stores, the dirty bit, Draw(alpha) }`.

The brief asked for "tested once per Frame"; a single test cannot serve both points (the drain
needs the post-accumulation accumulator, the tail replaces the `_time` stores), so the pointer is
read twice per Frame, never per step. Set by `RenderPipe::Sync()` when the row is on; cleared by
the hook itself at the first Frame tail after the row goes off (after draining).

## 4. The six fixes, where they live

| # | fix | where |
|---|---|---|
| 1 | two Renderer instances, ping-pong | slot 0 = `GameUpdateLoop::m_renderer`, slot 1 = `g_second` (`render_pipe.cpp`, `new Graphics::Renderer()` at install, deleted at uninstall). `PipeState::FreeSlot()` = the other slot from the held packet's, else the other from the last used (a faithful frame counts as slot 0), so each slot's lists are reused every other frame. `_shaderDetail` is set on the instance about to run pass 1 (MainHalf step 6), so `wave27_lane_gfxbig.cpp:399-406`'s first-frame read behaves as today on both. SORTMEMO: the memo is keyed by buffer pointer, 256 entries, age in sorts (`kMemoMaxAge` 1024, `draw_sort.h:94-96`); 2 x (groups x 3) lists, each meeting its own previous use two frames (~2 x 12 sorts) back -- far inside both bounds |
| 2 | main-owned frame index | `PipelinedTail`: `frameIndex = Context::_frameIndex + (a packet held ? 1 : 0)` -- the value the context holds when this packet's pass 2 runs (the held emit increments first). `Pass1` latches it into `Renderer::_frameIndex` (faithful: `context._frameIndex`), so `ReflectionImageBuffer::Render`'s `_generation` latch (pass 1) and `FrameCopyImageBuffer`'s (pass 2, `GenerateTextures(context, _frameIndex)`) see the sequential values. The emit keeps retail's `++_frameIndex` |
| 3 | context writes before pass 2 as values | packet fields, replayed by `Emit` in retail's order: `_time` (Frame tail), [BeginCommands, ClearResources, viewport: emit-owned, read live], SetTime, the four weather floats + 0x10800 + `SetFog`, the snow override |
| 4 | `SunLight::Set` step-only | `MainHalf` step 5: called when `plan.renderIdle` (nothing held), else skipped |
| 5 | `UpdateCamera` step-only | `MainHalf` step 4: same rule. While `EyeCamera::Active()` the frame is unpipelined |
| 6 | SkinMemo scope | `passes.cpp` `Pass2` opens `SkinMemo::Scope` around pass 2 only; pass 1 never opens it |

**Fix 4/5's rule as implemented is "nothing held", not "first frame after a step".** The two are
the same in the threaded design's steady state, but a resize (Device::settingsChanged, set inside a
step, consumed by `BeginCommands` one Frame LATER) changes `UpdateCamera`'s input (the back buffer
size) without a step: the resize frame runs unpipelined (it holds nothing afterwards), so the next
frame is "nothing held" and re-runs `UpdateCamera` with the new size. With "first frame after a
step" that frame would have skipped it and drawn with the old aspect until the next step.

## 5. Transitions (the drain table as implemented)

`PipeState::PlanTail` (`pipe_state.h`) + `Hook` (`render_pipe.cpp`):

| transition | test (read once per Frame tail) | action |
|---|---|---|
| first frame / toggle on | nothing held | pipelined, holds the first packet; presents nothing that Frame |
| a step | `kExtBeforeSteps` (only when `_accumulator >= _deltaTime`) | emit the held packet, then the steps run as today |
| toggle off | `!Ext::Ext().render_pipe` | drain, faithful tail, then `_extFrame = nullptr`, free slot 1 |
| loop change / not the built game | `_currentLoop != GameUpdateLoop::_singleton` or `m_game == nullptr` | drain, faithful tail (the menu loop is never pipelined) |
| quit | `MainLoop::_quit` set by this Frame's steps | drain, faithful tail (no packet left unpresented at exit) |
| resize | `_context->_device->settingsChanged` | drain, faithful tail (BeginCommands' resize path runs with nothing held) |
| `PaintTerrain` | `m_imageCapture->terrainImage == nullptr` | drain, faithful tail |
| eye camera | `Ext::Ext().eye_camera && EyeCamera::Active()` | drain, faithful tail |
| quicksave / quickload screenshot | `m_quickSavePressed || m_quickLoadPressed` | drain, faithful tail (finding 1) |
| a diagnostic that counts sequential frames | `Opt().captureIngame` set | every frame faithful (the faithful Draw's countdown) |
| device lost in an emit's Finish | `loop._context` changed across the emit | the NEW packet (gathered against the old context) is discarded; the next frame starts with nothing held |
| BeginCommands fails in an emit | (cannot happen: resize frames are unpipelined) | retail's DrawFailed path, the new packet discarded |

Single-threaded, every drain at a step is the emit of the frame gathered at the previous tail.
Saves / quicksaves / quickloads run from the UI pump inside a step (fenced); `GameUpdateLoop::DrawFailed`
is empty.

## 6. Readers / writers found beyond THREADRESEARCH 1.2 / 1.3

| # | what | file:line | handled |
|---|---|---|---|
| 1 | `m_quickSavePressed` / `m_quickLoadPressed`: set in Update (step), read by GUL::Draw steps 9 / 13, cleared ONLY by `TakeScreenshot` (`w2_takescreenshot.cpp:176-177`), i.e. on the emit side. Pipelined, frame N+1's main half would still see the flag N's emit has not yet cleared and record a second screenshot | `gameupdateloop.cpp:1449`, `:1518`; `w2_takescreenshot.cpp:176` | drain: a frame with either flag runs unpipelined (the flag is set in a step, so nothing is held and the faithful Draw takes and clears the screenshot in that same frame) |
| 2 | `MainLoop::_frameUpdateTime`: written by MainLoop::Draw's tail (the emit), read by the next SetTime | `loops.cpp:1106`, `gameupdateloop.cpp:1313` | read AT EMIT (not recorded): the emit of N follows the emit of N-1, exactly as retail's Draw N follows Draw N-1. Render-owned while pipelined. Also read by `debugui.cpp:720` (the debug UI, drawn / updated on the main side: a wall-clock display value) |
| 3 | `MainLoop::_contextDirty` and the default render buffer's extent | `loops.cpp:1063-1083` | emit-owned (written only in steps -- fenced -- and by the emit / device-lost path); read live by the emit, not recorded |
| 4 | `RoadTool::Draw` calls `Map::FindRoadPath` (writes the map's search scratch) on the emit side | `src/game/Player/roadtool.cpp` (Draw) | single-threaded: nothing on the main half pathfinds. **RENDERPIPE-1:** the render thread runs it while the main thread runs pass 1 -- no pass-1 reader of that scratch was found, but the path workers' / sim's users must be fenced (they are: steps) |
| 5 | `Save::SaveLoad::_captureFlags`: set in Update, read AND cleared by `SaveLoad::Draw` (`saveload_draw.cpp:369`, `:788`) | | emit-side only; the main half records the receiver pointer only |
| 6 | `UI::UserInterface::Draw` writes `_clipRegion` and the context colour / camera (`userinterface_draw.cpp:194-240`); `Tool::Draw` (seven tools) writes only the context and the tile-overlay records (`tool_base_tilerender.cpp`) and reads the live camera + mouse via `PlayerRay_ScreenToCell_101270` | | none of these are read by the main half (prelude or pass 1). The camera they read is written only in steps and by fix 5 (nothing held); the mouse only by the pump (steps) |
| 7 | `PerfX::BeginFrame(m_renderer)` reads slot 0's group camera as a fallback when no frustum was fed | `perfx.cpp:293` | runs in Update (a step: fenced); slot 0 may hold a frame two back -- it is a fallback only |
| 8 | `GameUpdateLoop::Draw`'s in-game capture arm (`BANISHED_CAPTURE_INGAME`) counts faithful Draw calls in a function static | `gameupdateloop.cpp:1283-1295` | while the diagnostic is set every frame runs faithful |

`Context::_frameIndex` readers: `wave27_lane_gfxbig.cpp:949` (pass 1, fix 2) and `sharp_ui.cpp:260`
(UI draw, emit side: sees N's value, as retail). No sim reader (re-checked).

## 7. Object proofs (WALKGRID section 3's method)

`tools/build.sh build perf --jobs 2` at the base (`eff2464a1`) before any edit; the objects of the
touched / adjacent TUs kept; the same command after. Per function, `objdump -d -r
--no-show-raw-insn -C`, display addresses dropped, branch targets as `<sym+off>`, relocations as
`RELOC type sym` (`scratchpad/rp0/cmpobj.py`, not committed).

| object | function | result |
|---|---|---|
| `wave27_lane_gfxbig.cpp` | `Renderer::Draw` and the other 23 functions | **identical** (file untouched) |
| `loops.cpp` | `MainLoop::Draw` and the other 36 functions | **identical** (recompiled: includes `MainLoop.h`) |
| `gameupdateloop.cpp` | `GameUpdateLoop::Draw`, `::Update` and the other 21 functions | **identical** (recompiled: includes `MainLoop.h`) |
| `weather_sun.cpp` | `WeatherSimulation::DrawUpdate`, `BlendLighting`, 5 others | **identical** (file untouched) |
| `gameupdateloop_load.cpp` | `PlayerController::UpdateCamera` and 20 others | **identical** |
| `mainloop_run.cpp`, `mainloop_video.cpp`, `engine.cpp` | `Run`, `Update`, `OnVideoError/Restart`, ... (30 functions) | **identical** (recompiled: include `MainLoop.h`) |
| `mainloop_frame.cpp` | 6 functions (`CurrentTimeTicksViaGlobals`, ...) | **identical** |
| `mainloop_frame.cpp` | `MainLoop::Frame` | **differs on purpose, 184 -> 197 insns**: (a) after `movb $0,0x19(%rbp)` (`_quit = 0`), one load of `_extFrame` + `test` + `je` past a cold block that compares the accumulator and makes the indirect call; (b) at the tail, the same load + `test`/`je`; the faithful branch's four `_time` stores, the `orl $0x800` and `call MainLoop::Draw` moved into an out-of-line block that jumps back to `call Platform::YieldThread`. The tail's float maths is the same instruction sequence (`cvtsi2sd` x3, `mulsd` x3, `cvtsd2ss` x3, `divss`, `mulss`, `addss`) on renamed registers. **The step loop's body (input update, the 64-channel audio sweep, the quit test, `Platform::Pump`, `Update`, the post-update callbacks, the accumulator updates) is instruction-for-instruction the same**, only its branch offsets moved |
| `registry.cpp`, `registry_boot.cpp`, `stress_harness.cpp` | -- | differ by design (the new row / `Sync` call / the split column; `Ext::State` grew a bool, so the table's offsets moved). Not faithful code |

Verdict: **the faithful `Renderer::Draw`, `MainLoop::Draw`, `GameUpdateLoop::Draw`,
`Weather::DrawUpdate` and `UpdateCamera` are the same code as the base**; the faithful frame pays
two loads + two not-taken branches per Frame in `MainLoop::Frame`, none inside the step loop.

## 8. The shadows (BANISHED_STRESS_COMPPROF=4; `[VERIFY] <name> checks= mismatches=` at quit)

| shadow | when | what it compares | expected |
|---|---|---|---|
| `RenderPipe` | every emit of a hashed packet | `HashFrame` at hand-off (end of the main half) vs right before the emit vs right after it (detail bit 0 = before differed, bit 1 = after). The hash, field by field (pads left out by construction: DrawItem +0x24/+0x34, Drawable +0x0c, SceneEntry +0xfc, Camera +0x61..+0x63/+0xd5..+0xd7, Geometry +0x14/+0xbc): every scene entry (scene, buffer, camera, scalars, drawable ids), every USED draw group (buffer, both cameras, shader, mask, the three item lists: every field but the pads), every drawable; per item its Geometry's header (+0x28..+0xbb: detail, dirty flags, mask, the three boxes, the opacity pair, the transform pair), a MeshInstance's `_nodes` pairs, root offset, colour, sort bias and -- when `_animation._instanced` -- the shared pose GetBones picks (AnimationManager instance state, mesh node count); a ParticleInstance's pool pointer, live count and live records; the SunLight's eleven Set dwords | 0. Single-threaded, a mismatch means something in frame N+1's main half (pass 1 or the prelude) wrote what frame N's pass 2 reads: the bug class this block exists to find |
| `RenderPipeFrame` | every emit, before pass 2 | `Renderer::_frameIndex == Context::_frameIndex` (the sequential order's latch) `== modelIndex` (the model: the context's index when the pipeline (re)starts, +1 per held packet), `_alphaT` bits == the recorded alpha, scene entry 0's `_shaderDetail` == the recorded shader detail. Detail = (renderer index << 32) | context index | 0 |
| `RenderPipeIdem` | every main half that SKIPS fixes 4 / 5 | detail 1: the eleven dwords `SunLight::Set` would store vs the live sun, bit for bit; detail 2: the camera's frustum planes + orthographic byte + projection after a REAL `UpdateCamera` vs before (the camera's bytes are restored after the compare, so a COMPPROF=4 run draws what a COMPPROF=0 run draws) | 0 |

Every existing shadow keeps running with the row on (DrawGather, SkinBones, DrawItemSort,
FrustumCull, ...); `SkinBones` now verifies inside pass 2's scope only.

## 9. The harness columns

- `src/diag/stress_harness.*`: on a pipelined frame the Draw bracket (`StressHarness_DrawBegin`,
  main half start) ends at the end of pass 1 (`StressHarness_PipeMainEnd`, which also counts the
  frame for fps); the packet's GUL::Draw share (pass 2 + steps 8-12: `PipePacketBegin/End`) is added
  to `draw_ms_mean` when the packet is emitted, so `draw_ms_mean` stays the same span as the faithful
  column. The whole emit (BeginCommands .. Present) is bracketed separately (`PipeEmitBegin/End`).
- CSV: three APPENDED columns `draw_main_ms`, `draw_packet_ms`, `draw_emit_ms` (per pipelined frame /
  per packet / per emit; 0 in a window with none). Every older reader is unaffected.
- stderr, per CSV row while installed: `[Stress::pipe] render_pipe=1 draw main=.. packet=.. emit=..
  (main share ..%) | window: pipelined= sequential= emits= drains step= other= discarded= idle
  writes= skips=`. **main share = `a`** (THREADRESEARCH 1.8, assumed 0.55).
- `tools/stress_report.py`: a `pipe` line under `render` (main / packet / emit ms, main share) when
  the CSV has the columns and a steady row is non-zero; nothing changes for an older CSV.

## 10. Unit tests

`tests/unit/ext_render_pipe_test.cpp`, 8 tests: the emit order against retail's (and the
BeginCommands failure path), the first frame holding nothing then the ping-pong, the fence, every
drain transition (and toggle-off's uninstall), the faithful-frame / discard slot choice, a 20,000-frame
scripted run of the state machine (never two packets held, pass 1 never fills the held slot, the
sim never runs with a packet held, renderIdle iff nothing held), the registry row.

`tools/build.sh unit debug --jobs 2`: **1161 tests, 0 failures** at the branch (1153 before + these 8); after merging main (CPULIMIT, +3 net) **1164 tests, 0 failures** (1156 + 8).

## 11. Gates

- `tools/build.sh build debug --jobs 2` and `build perf --jobs 2`: exit 0. Warnings: the perf build's
  normalised set (`grep 'warning:' | sed -E 's/:[0-9]+:[0-9]+: warning:/: warning:/' | sort -u`) against
  the base perf build's at `eff2464a1`: **0 new** (`comm -23` empty). Debug: no warning in any new or
  touched file except `mainloop_frame.cpp: multi-line comment` (line 230, pre-existing, also in the
  base perf log). The brief's baseline files `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt`
  do not exist in the tree (only `docs/history/endgame_b/*/runs/` copies), so the base build was the baseline.
- `python tools/castmetric.py src`: **2** (unchanged). `python tools/abi_audit.py`: **OK**.
  `bash tools/acceptance/acceptance.sh`: **0 line(s) off target**.
- After merging main (CPULIMIT; one include-block conflict in `stress_harness.cpp`, both kept):
  debug / perf / unit rebuilt, exit 0, 1164 / 0; the object proof of section 7 re-run on the merged
  tree: the same verdicts (only `MainLoop::Frame` differs among the faithful TUs); 0 new perf
  warnings; castmetric 2, abi_audit OK, acceptance 0 off target.
- Commit order note: the harness commit (`138327320`) declares `RenderPipe::TakeWindow` but the
  body arrives one commit later (`21d9183d2`); build the tip, not that intermediate commit.
- `tools/stress_report.py` run on `perf/baselines/current.csv` (no new columns: unchanged output) and on
  a copy with the three columns appended (prints the `pipe` line).

## 12. Open items for RENDERPIPE-1 (the thread)

- **What crosses.** The packet as built is the whole hand-off: every field is a value or a pointer
  to an object no step can free while a packet is held (the fence). The render thread needs, beyond
  the packet: the MainLoop (`_context`, `_contextDirty`, `_postDraw`, `_currentLoop->DrawFailed`,
  `_frameUpdateTime` / `_frameStartTicks`), `GameUpdateLoop::m_userInterface` (UI draw),
  `MainLoop::_singleton`. All are read by the emit only while pipelined (render-owned) or are
  written only in steps (fenced).
- **Where the lock moves.** `BeginCommands` / `Finish` are emit steps, so `Video::Device`'s
  recursive lock is taken and dropped on the render thread for the whole emit. The main half takes
  no device lock (pass 1 creates no GPU object; RenderTextures only appends scene entries). The
  main thread's Update-side `Device::Lock` users (the minimap upload, thumbnails, buffer creates)
  all run inside steps, after the fence, with the render thread idle.
- **The fence / hand-off.** `kExtBeforeSteps` becomes "wait until the render thread is idle"; the
  tail's "emit the held packet" becomes "wait for the render thread to finish N, then hand N+1
  over". Every `drain` in section 5 becomes a wait-idle. `PipeState` already encodes depth 1.
- **Device-lost re-entry.** Today (single thread) the emit's `Finish` runs retail's recovery on the
  main thread and the new packet is discarded (it names objects gathered against the old context).
  Threaded: the render thread detects `hr != 0` in Finish; the recovery (`OnVideoError` destroys
  Externals and the Context, `OnVideoRestart` rebuilds them) must run with the main thread parked at
  the hand-off (it is either parked or about to park: the main half never touches the device), and
  the parked packet is DISCARDED (pass 1 re-run on the next frame). The signal the thread can use is
  the one RENDERPIPE-0 uses: `MainLoop::_context` changed across the emit.
- **Fixes 4 / 5 on the thread.** "Nothing held" = "render thread idle": the writes are safe exactly
  then. The RenderPipeIdem camera check calls the REAL `UpdateCamera` and restores the bytes: with a
  render thread reading the camera (tools via `PlayerRay_ScreenToCell_101270`) that write races;
  RENDERPIPE-1 must compute it on a scratch copy (copy UpdateCamera's frustum maths) or only check on
  idle frames.
- **RoadTool::Draw's FindRoadPath** (finding 4) runs on the render thread: confirm no main-half
  reader of the map's search scratch, and that no path worker shares it.
- **The harness.** `StressHarness_PipePacket*` / `PipeEmit*` would run on the render thread: the
  window sums need atomics or a per-thread accumulator folded at `EmitRow` (today they are plain
  statics, single-threaded by construction). Add the hand-off wait counter and latency there.
- **The roster.** CPULIMIT's `Cpu::PathWorkers(cores, renderPipe)` / `CullHelpers` take the
  render-pipe bool; `path_workers.cpp:232` still passes false ("RENDERPIPE-1 passes
  Ext::Ext().render_pipe"). RENDERPIPE-0 runs no thread, so it leaves that call alone.
- **Stress-run expectations for the coordinator.** At COMPPROF=4 every `[VERIFY] RenderPipe*` line
  must read mismatches=0; `[Stress::pipe]` gives `a` directly; `idle writes` should equal the number
  of step frames + transitions; `discarded` 0 unless a device loss was injected.

## Results (coordinator, 2026-10-14)

- **Gate RENDERPIPE0 at 0** (merge `76bdc5718`; GATE_TABLE row): routes exit 0, resave 5/5 == golden `971aaec2cf95`, unit 1164 / 0, 0 new warnings, castmetric 2, abi_audit OK, mods parked. The gate runs the faithful profile (row OFF).
- **Shadows with the row ON** (`BANISHED_STRESS_COMPPROF=4 STRESS_EXT=render_pipe tools/stress_run.sh`, heavy = `--map large --citizens 1600 --sets 3 --spread`, compact = the default town; `perf/results/20261013_22{4556,4720,4826,4937}_rp0sh_*`), the last `[VERIFY]` line of each run:

| run | RenderPipe | RenderPipeFrame | RenderPipeIdem | discarded | `[Stress::pipe]` last window |
|---|---|---|---|---|---|
| heavy, speed 10, 3000 frames | 2932 / **0** | 2932 / **0** | 1612 / **0** | 0 | main 1.673 packet 0.556 emit 2.154 ms, main share 75.1%, pipelined 1032, drains step 599, skips 433 |
| heavy, speed 50, 1800 frames | 986 / **0** | 986 / **0** | (every frame steps: no skip) | 0 | main 1.818 packet 0.644 emit 0.745, share 73.9%, pipelined 328 = drains 328 |
| compact, speed 10, 3000 frames | 3743 / **0** | 3743 / **0** | 2682 / **0** | 0 | main 1.010 packet 0.458 emit 2.640, share 68.8%, pipelined 1253, drains 599, skips 654 |
| compact, speed 50, 1800 frames | 2323 / **0** | 2323 / **0** | 1068 / **0** | 0 | main 1.065 packet 0.523 emit 1.066, share 67.1%, pipelined 666, drains 596, skips 70 |

  Every shadow 0 in every run: nothing in frame N+1's main half writes what frame N's packet reads, the frame index / alpha / shader detail match the sequential order, and the skipped `SunLight::Set` / `UpdateCamera` would have written the same bytes. **The split is proven on one thread; RENDERPIPE-1 (the thread) dispatched on it.**
- **The pass-1 share `a` reads 67-75% under COMPPROF=4** (the hash at hand-off is inside the main bracket), against THREADRESEARCH's assumed 0.55. The COMPPROF=0 number on a CLEAN machine is still owed (below). The `emit` bracket (BeginCommands .. Present) exceeds the packet by 1-2 ms in the speed-10 runs: the Present side (GPU / driver) is inside it -- the render thread's bound, to be read at COMPPROF=0.
- **The pass-1 share `a` at COMPPROF=0, unrestricted mask, overlays stopped** (series ab4u, `perf/results/20261013_23*_ab4u_*`, 3000 frames at speed 10 / 1800 at speed 50, interleaved x2; `STRESS_EXT=none` vs `STRESS_EXT=render_pipe`; rep 1 of the heavy render_pipe leg ran BUSY (an agent's final build), every other leg CLEAN):

| leg | faithful fps / draw ms | + render_pipe (single-threaded split) fps / draw ms | main / packet / emit ms | **a** |
|---|---|---|---|---|
| heavy, speed 10 | 834 / 0.99, 819 / 1.01 | 780 / 1.04 (BUSY), 820 / 1.01 | 0.53 / 0.49 / 0.51 | **0.52** |
| compact, speed 10 | 1315 / 0.67, 1300 / 0.68 | 1326 / 0.67, 1331 / 0.67 | 0.30 / 0.36 / 0.38 | **0.45** |
| heavy, speed 50 | 159 / 1.10, 270 / 1.05 | 182 / 1.10, 187 / 1.07 | 0.59 / 0.50 / 0.52 | **0.54** |

  `a` = 0.45-0.54, inside THREADRESEARCH §1.8's assumed 0.55 (0.45-0.65); the COMPPROF=4 reading of 67-75% was the hash inside the main bracket. The single-threaded split is **neutral** (fps and draw ms within the pair spread: the packet machinery itself costs nothing measurable), which is what RENDERPIPE-0 had to show before the thread. The emit exceeds the packet by only ~20 us at COMPPROF=0 (Present does not wait on the GPU in these towns). The speed-50 heavy fps varies 159-270 between faithful reps (the town's state at 1800 frames): that leg needs longer runs or more reps before it says anything about the thread.
