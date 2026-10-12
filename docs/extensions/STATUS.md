# Fixes and extensions -- status (newest first)

## 2026-10-12 evening -- WALKCACHE: the slot-8 walk visits only the components that tick (`handins/WALKCACHE.md`)

- Per-entity side table (16 B, parallel to the TREE_SNAPSHOT array) of the components with flag bit 2, staled by `RefreshEntityFlags` (every post-ctor flag writer reaches it) and keyed on the component array; retail's loop takes over mid-entity on a stale, 0x400 still breaks after the same tick; 11 unit tests (retail vs cached logs identical under mutation), COMPPROF=4 shadow `TickList`: 155.7M live checks in the heavy town, 0 mismatches. Gate ELPERF 0 (both merged, `aea9b6834`). **Benchmark (CLEAN)**: heavy town sim 0.32 -> 0.29 ms/iter (-9.7%; tree walk -4.7%, PathDistance 0.44 -> 0.17 ms/frame, CitizenList 0.21 -> 0.08), compact 0.17 -> 0.16 (-5.5%). Both are faithful replacements (same results), not extensions -- the user's question answered that way.

## 2026-10-12 evening -- PATHDIST: the region search on a bucket open set (`handins/PATHDIST.md`)

- `RegionGraph::FindPath` (Map::PathDistance's region A*) pops from a LIFO bucket set in the AVL's exact (f asc, seq desc) order, faithful and default: unit 0 mismatches over 12,805 queries (1063 / 0); the COMPPROF=4 shadow (`StressHarness_Verify "PathDistance"`, retail's AVL body kept as `FindPathAvl`): 127,594 live region searches in the heavy town, 0 mismatches (`20261012_143402_pathdist_verify`).

## 2026-10-12 afternoon -- ELPROF: the EntityList profile (`handins/ELPROF.md`)

- Heavy town, speed 10, COMPPROF=2 + the sampler (two runs, CLEAN). EntityList = tree walk 85%
  (2,630 entities per iteration at 78 ns each, no hot spot) + bucket phase 15% (4x that in the
  first two windows). Tree walk by type: CharacterComponent move tick + action executor 49%,
  ModelComponent 10% (PerfX bookkeeping), particles 9%, state machines 6%, the walk itself ~13%.
  Bucket phase: `Map::PathDistance` 46% (the region search on the same AVL open set as retail's
  cell search), `FindBestTarget` 26%.
- Threading data (Amdahl, upper bounds): the entity-local ticks are 45% of EntityList -> Update
  -29%, speed-50 heavy windows ~60 -> ~85 fps; everything beyond that threads the citizen AI's
  shared-state transitions (class C, non-deterministic sim). Serial candidates with faithful
  results: PathDistance on the fast_paths bucket queue, PerfX dense side table, cached slot-8
  component lists (~-20% of EntityList together). The user's call on what to build.

## 2026-10-12 afternoon -- E2E split: the 15 ms tail is the 60 Hz Update step, not pathfinding

- Four pf_* columns (`pf_ready_us_*`, `pf_poll_us_*`, `pf_frames{0,1,2,3p}_pct`; `PfProf_Ready` at
  both publish sites). Heavy town, both extensions: enqueue->ready p95 745 us, ready->taken p95
  14.3 ms, 12.6% of requests cross one Update, none two. `MainLoop::Frame` runs Update on a fixed
  1/60 s accumulator with the speed iterations back to back; a late request waits for the next
  Update. Wall latency only (one sim iteration in sim time); no bearing on fps.
- fps at speed 50 (heavy town, rows 1-3): Update 12.7-15.5 ms = the whole step, EntityList ~88% of
  it, fps 48-67 in the windows where Updates pile up before a Draw. The limiter is the main-thread
  component tick; pathfinding's main-thread share is PathDistance ~0.3 ms/Update.
- Facts recorded (memory too): the user's RTSS limiter capped every earlier fps column at ~140
  (uncapped: 390-430 fps at speed 10; stop Afterburner first, it respawns RTSS); the heavy stress
  town starves to 0 citizens at ~110k-200k iterations at any speed (speed-invariant sim), so speed
  50 steady-state averages are only valid for rows 1-3. Hand-in: `handins/PFBENCH.md` (E2E split).

## 2026-10-12 -- `ext.path_workers` + the extension load notice merged (gates at 0); the heavy benchmark

- The user LOOKED at `ext.fast_paths`: "looks fine in game". Decisions: build the worker pool; DROP the GPU
  replay prototype for now; PAUSE the gameplay items (road costs, cost-aware PathDistance); the `ext:` load
  notice must say "extension".
- EXTNOTICE (`4c1c9bbe4`, gate EXTNOTICE 0): three MessageBox dialogs inside `Extensions.rsc` (not in build /
  newer / turned off) with their strings, `src/ext/load_notice` picks one in `SaveLoadDialog::DoLoad`; real
  mods untouched; the kit compiled them with 0 problems. Owed: the user's look (save with fast_paths on,
  turn it off, load).
- PATHWORKERS (merge + `f4bf88986`, gate PATHWORKERS 0): `ext.path_workers` (class B, formatVersion 1, off)
  -- retail's worker thread becomes a dispatcher; N = hardware threads - 2 in [1, 4] pool workers
  (`System::Thread`), each with its own search scratch (0x18 bytes per cell per worker: 24 MB at N = 4 on
  a large map), commands 2 / 3 as a barrier, results published under the lock; composes with
  `ext.fast_paths` (per-worker fast scratch). 118k randomised requests identical per slot vs the serial
  path. Not built: a priority lane for the road tool / boats; shadow mode does not run under the pool.
  `pf_worker_busy_pct` is the SUM over workers under the pool.
- **HEAVY BENCHMARK** (every run `machine load: CLEAN`; the 800-citizen towns never back the queue up, this
  one does: retail's single worker is 59% busy and a citizen waits 0.8 s at the median for a path):

| run (large map, 1600 citizens, spread, speed 10) | search us p50 / p95 / max | wait p50 / p95 | e2e p95 | worker busy | req / sim s | sim ms/iter |
|---|---|---|---|---|---|---|
| faithful (`20261012_105902_pfh_faithful`) | 1223 / 8887 / 30304 | 826 ms / 947 ms | 961 ms | 58.9% | 25.0 | 0.32 |
| ext.path_workers, 4 workers (`20261012_110144_pfh_workers`) | 36 / 3699 / 18071 | 18 us / 1.0 ms | 18.8 ms | 22.9% (sum of 4) | 41.0 | 0.31 |
| ext.fast_paths (`20261012_110422_pfh_fast`) | 4.1 / 674 / 4118 | 6.8 us / 1.8 ms | 15.1 ms | 4.6% | 40.5 | 0.31 |
| both (`20261012_110655_pfh_both`) | 4.8 / 780 / 4865 | 15 us / 39 us | 14.7 ms | 5.5% | 41.2 | 0.31 |

  The pool removes the queue (wait p95 947 ms -> 1 ms); the fast search removes most of the work (search
  p95 8.9 -> 0.7 ms, busy 59% -> 4.6%); together the wait p95 is 39 us. Requests per sim second rise 60%+
  with either, because citizens are no longer standing still waiting (path len mean 46 -> 30 for the same
  reason: the sims diverge). Sim ms / iteration is unchanged within noise in all four.
- Also merged earlier today (see the entry below): PATHRESEARCH, EXTSAVE, PFBENCH, `ext.fast_paths`.
- Open: the user's looks (the notice; `ext.path_workers` in play), one run per cell (the plan wanted three
  per side; the paired shadow and the two-order-of-magnitude deltas make repeats a formality), the two
  EXTSAVE doubts (old saves keep the requested set; Yes on a fatal notice still leads to the error box).

## 2026-10-12 -- PATHFINDING: research, the class B machinery, the benchmark, `ext.fast_paths` (gates at 0)

- The user opened the pathfinding block with an opus team ("research first, then implement; benchmark GPU A*
  in the stress town"). Rule recorded: **benchmarks only on an idle machine**, between agent waves, and a
  stress report counts only with `machine load: CLEAN`.
- PATHRESEARCH (`d62630584`, `handins/PATHRESEARCH.md`): retail's heuristic prices a step at 10 while the
  cheapest real step costs 30 (road) / 40 (ground), so A* expands a blob (verified in `action_pf_astar.cpp`
  :340 / :375); an AVL open set and ~48 divisions per expansion. GPU A*: searches are 10^3-10^4 nodes with a
  citizen waiting on each, a dispatch + readback costs more than the CPU search, vendor tie-breaking is
  non-deterministic -- a benchmark-only prototype later. The user's decisions: equal-cost paths that differ in
  shape are acceptable in a locked game; GPU = benchmark-only prototype; class B.
- EXTSAVE (`defef548a`, gate EXTSAVE 0): the class B / C save machinery -- pseudo-mods `ext:<id>` through
  `Save::SaveLoad::_extExtraMods` (null under faithful), the block-1 tail manifest (zero bytes when no B / C
  extension is active) locking the set on load (`Ext::LockForGameWithSet`), the `ext:` load notice, 7 tests.
  Open: an old save without a manifest keeps the requested set; the notice wording says "mod".
- PFBENCH (`dab117dc1`, gate PFBENCH 0): pathfinding columns in the stress CSV (worker search time,
  expansions, queue wait / depth, e2e, PathDistance / FillZones / road spin; shadow columns), the spread town
  (`--spread` = negative sets), the PATHFINDING report section; harness cost within noise (sim 0.17 = 0.17).
  Shadow switch = `BANISHED_STRESS_COMPPROF=3` (level 1 + shadow). `STRESS_EXT=<id>` runs a single-extension
  A/B through settings.cfg.
- FASTPATHS (`b0b308590` + `44b14ff4b`, gate FASTPATHS 0): `ext.fast_paths` (class B, formatVersion 1,
  default off) -- heuristic scaled to the cheapest step type present on the map (type 11 is passable and costs
  1, so the minimum is over the types the grid holds, counted at the case-2 seam), LIFO bucket queue that
  reproduces retail's (f, LIFO) pop order, division-free neighbours; selection read in `GameUpdateLoop::Load`
  after the lock is final; 4000-grid unit tests: identical cells at scale 1, equal cost at the real scale.
- **BENCHMARK (spread town, 800 citizens, speed 10, medium map; all runs CLEAN):** faithful vs
  `ext.fast_paths` alone: search p95 2008 -> 717 us, p50 153 -> 16 us, max 6.5 -> 3.6 ms, worker busy 4.9% ->
  1.6%, queue wait p95 4.3 -> 0.7 ms, end-to-end p95 21.3 -> 15.8 ms, sim 0.19 = 0.19 ms / iteration.
  **Shadow (paired, 13,160 requests): equal cost 100%, worse 0, same cells 46.7%, speedup 8.25x.** One run
  per side so far (the paired shadow is the decisive measure); the compact town's worker is under 6% busy
  either way, so the player-visible gain is per-request latency. Results in `perf/results/20261012_09*_pf_*`.
- PATHWORKERS (`ad3369633` + `34e2fbad9`, `handins/PATHWORKERS.md`; not yet gated or benchmarked):
  `ext.path_workers` (class B, formatVersion 1, default off). The pathfinder thread becomes a dispatcher:
  commands 0 / 1 / 4 apply in order, 5 becomes a job with a parameter snapshot, 2 / 3 are a barrier. N =
  hardware threads - 2, clamped to [1, 4], `System::Thread` pool workers, each with its own GridPathSearch
  (retail's Run) or FastPaths::Searcher; results published under the lock; drained and joined in
  `~PathfinderThread`. Unit tests: 3000 streams (118k requests, 47k edits) at N = 1..4 equal the serial path
  per slot. Faithful cost: one byte load per command. Expect a gain only where the queue backs up
  (`--citizens 1600 --map large --spread`); `pf_worker_busy_pct` is the sum over workers.
- Next candidates (PATHRESEARCH §4): `ext.path_workers` (a worker pool, retail's cells, 3-4 agent-days), the
  GPU batch prototype as an offline replay (5-8 agent-days, benchmark only), the gameplay items in §6 Q6
  (speed-proportional road costs, cost-aware PathDistance) only if the user wants them. Owed: the user's look
  at `ext.fast_paths` in play (toggle it in Extensions before a new game; the save then carries `ext:fast_paths`).

## 2026-10-12 -- SHARPUI2 LOOKED (good); the fifth fix `fix.ui_scale_layout`

- SHARPUI2 (`6e22720b1`) + the loose-variant check fix (`252a6c01c`: `PlatformExists` on
  `GetCompiledPath`'s full path with `:resource`; `FileSystem::Exists` is gated off during play).
  **LOOKED (the user, 2026-10-12)**: "overall looks good at all ui scales and in game the tooltips and
  render look good". One defect reported: on Apply of a UI-scale change the dialogs jump to a corner for an
  instant (upper left going 75 -> 150, lower right going 150 -> 75).
- Traced FIRST (EYECAM §11 lesson): a frame-timing line in `SharpUI::Draw` for the change frame and the three
  after it. The variant load is 1-20 ms, every frame around the change is 2-12 ms, and the user reproduced
  the jump at the menu, in game, and with every extension OFF. Not the extension's: it is retail's
  `UserInterface::Update` reading `_uiScale` for the layout size BEFORE the events that Apply runs in
  (`retail_bugs.md` §5, row 1325). The trace was removed again (numbers in `handins/SHARPUI.md` §12.2).
- Fifth fix `fix.ui_scale_layout` (default on): the layout loop uses `backBuffer * _uiScale` when the events
  changed the scale. First ENGINE-side fix seam: `UI::UserInterface::_extFixScaleLayout`, a null-by-default
  `const bool*` that `Ext::InitFromSettings` points at `Fix().ui_scale_layout` (hook rule 5; the menu toggle
  stays live through the pointer). Unit 1032/0. Gate UISCALE in `phase_d/GATE_TABLE.md`.
- Still owed: the next extension (class B pathfinding, `INVENTORY.md` §5.2), route 2 (port the compiler),
  the hard-link hazard in `ext_resources.sh`, row 1324, the 18 RESTORE items.
- `ext.fast_paths` (class B, default off, formatVersion 1; block FASTPATHS, `handins/FASTPATHS.md`): PATHRESEARCH
  §4.1 (a)-(d) as one search (`src/ext/fast_paths/`), selected once per game in `GameUpdateLoop::Load` (after the
  lock and again after step 31), dispatched in `ApplyCommand` case 5 (retail's `grid.Run` on the faithful arm),
  heuristic scaled by the cheapest PRESENT passable type (type 11 sits in 0xe1f at cost 1, so case 2 keeps type
  counts). Unit: scale 1 identical to retail on 4000 grids; real scale cost-exact, expansions 0.625 overall,
  0.35 with roads, 0.01 on open ground. Not run in game; the first live exercise of EXTSAVE §4.3.

## 2026-10-12 -- SHARPUI + EXTRSC merged (gate SHARPUI at 0); the stress false alarm

- `ext.sharp_ui` (class A, default off, `dee1fd743`): when on, the effective UI scale snaps to an integer
  device-pixel ratio -- `ceil(100 / slider%)`, so 75-99% = 2x, 100-150% = 1x, capped at the largest ratio
  that leaves an 800x600 layout (1080p stays 1x, `clamped=1` in the trace; 1440p and 4K get 2x); the slider
  value is never written. Pixel-snapped quads and per-batch point sampling through
  `UI::SpriteContext::_extSharp` / `Video::ImageBufferData::_extPointSampling` (rule 5 shape); retail's
  `UserInterface::Draw` runs unchanged inside the wrapper. NOTE the slider's sense: 75% is the BIGGER UI.
  Sharp text (c) is a costed plan in `handins/SHARPUI.md` §11, not code. `[SharpUI] on ...` trace line.
- EXTRSC (`404aede4d`): the Extensions dialog, button, 19 strings and 5 placeholder sprites are our own
  resources under `src/ext/ui/`, compiled by `ToolKit/bin/x64/Tools-x64.exe` through `tools/ext_resources.sh`
  (`/build <T> /pathres <res> /pathdat <WinData> /pathpkg <WinData>`, the kit's `resource/` overlaid with ours),
  28 `.crs` deployed loose into WinData/Dialog by `tools/build.sh deploy`. Deploy now NEEDS the kit. The
  script hard-links the game's `.pkg` into the build dir (mtimes intact, but a hazard: make it a copy or a
  read-only reference). A failed compile pops a modal Assert box (unattended hazard; 900 s timeout per call).
- **False alarm, two hours**: the first stress pair read 0.01 ms per iteration. Not the loose files (the
  correlation was coincidence): the game window came up UNFOCUSED while the user typed elsewhere,
  `Application::IsPaused` + `GameAutoPause=true` made `PlayerToolManager::Update` (retail 0x1401026a0) open
  the pause menu at frame 1, and a pause menu pauses the sim. `tools/stress_run.sh` now switches
  GameAutoPause off for the run and restores settings.cfg at exit, as `linux_run.sh` already did
  (`8f15c5936`). Lesson: before bisecting a stress stall, check for `[MainMenu] OnOpened ... game=<world>` at
  frame 1 in the error log.
- **LOOKED (the user, 2026-10-12)**: the Extensions dialog on compiled resources works, glyphs show. The
  integer snap at 75% = 2x: crisp but "blows it up to the point of being unusable" (screenshot 100 vs 75).
  VERDICT: no scale snap; the slider means what it always meant. The kit's Font/*.rsc name an installed
  typeface + pixel height and the compiler rasterizes the sheet, so sharp text at a non-integer scale =
  scale-matched font atlases with metrics divided back to layout units. Block SHARPUI2 dispatched: snap
  removed, compiled font variants under src/ext/ui/Font/, a 2x sprite-sheet lookup convention (no art).

## 2026-10-11 night -- decisions: resources for extensions, the toolkit compiler

- **Now (route 1)**: the official mod kit's `Tools-x64.exe` compiles the sources under `src/ext/<name>/ui/`
  offline; a build step deploys the compiled `.crs` loose into WinData (loose loads first); the Extensions
  dialog moves from its Mod.rsc clone to its own `Dialog/Extensions.rsc` + icon sprite. The compiler is a
  developer-machine dependency (not redistributable, not in the repo); Linux consumes the compiled outputs.
  NOT blocked after all: the kit is in the repo at `ToolKit/` (git-ignored; `bin/x64/Tools-x64.exe`, the
  profile binaries, `resource/`, the example mods) -- EXTSCAFFOLD searched only outside the repo. Block EXTRSC
  dispatched: the compile step + the dialog swap.
- **Later (route 2)**: port the compiler -- the text Encoding reader (stripped from the shipped DLL, present
  in Runtime-x64-profile.dll) + the Visitor read path over the existing Serialize bodies + the source-tree
  driver -- as `tools/rsc_compile` in CMake, cross-platform. The start of our own modding toolkit. A planned
  block once two or three extensions need resources.
- SHARPUI in flight with the user's snap decision: when on, the effective UI scale snaps to an integer
  device-pixel ratio (slider untouched), fit-checked for the largest dialogs, clamped to 1x below the fit.

## 2026-10-11 night -- ext.eye_camera DONE: the user's look clean

- The idle drop's OBSERVED cause (`handins/EYECAM.md` §11): EditTool::Update's retail off-display rule
  (0x1400fcebc) closed the citizen panel, and with it the FollowDialog, because the followed citizen
  projects to no screen rect at eye level. One seam in `edittool.cpp` exempts the eye's own citizen
  (`EyeCamera::FollowedEntity()`), `a39611200`. The user: "lgtm no issues" (follow through stops and
  buildings, the four fixes, the starting town on the ground).
- Lesson for the next extension: when a drop or close is intermittent, put the trace in FIRST and let the
  user's run name the cause; EYECAM2 and EYECAM3 fixed real defects but inferred the wrong one twice.
- Next in the user's order: class B pathfinding (`INVENTORY.md` §5.2), then class A sharp UI at high scale
  (§5.3).

## 2026-10-11 night -- EYECAM3 + the fourth fix merged (gate EYECAM3 at 0)

- BARNZ: the floating starter barn is RETAIL's (0x1400a1fc0 samples the corners before the flatten; the
  player path 0x1400fda00 stores z = 0): retail_bugs §4, row 1323, and the fourth fix `fix.start_barn_height`
  (`e696a0633`, default on; applies to every start-town structure).
- EYECAM3 (`fe72a743e`): the followed ENTITY is the identity, read each step from the frontmost
  FollowDialog; hand-back only on no dialog / no citizen / entity removed; `[EyeCam] engage / disengage
  reason=` lines in standalone_error.log. Cause inferred, not reproduced: if the view drops again the
  trace names the test. Row 1324 (CitizenUI::OnClosed skips the FollowDialog close on a marked citizen).
- **Owed**: the user's look (idle follow in a busy town for minutes, then grep `[EyeCam]`; the starting
  town sitting on the ground, no building sunk).

## 2026-10-11 late -- EYECAM2 merged and LOOKED; EYECAM3 + BARNZ dispatched

- EYECAM2 (`10aeeb57c`, gate row EYECAM2 at 0): child eye height from the age scale (confirmed good), shadows
  at eye level through `SunLight::_extShadowFitView` (confirmed good; DESIGN hook rule 5 names that shape),
  the slot-reuse weak ref. The idle drop is only MOSTLY fixed: standing still SOMETIMES still hands the view
  back to retail's follow -- block EYECAM3 enumerates every exit condition, adds an `[EyeCam] disengage
  reason=` stderr trace and fixes the live one.
- The user also saw the STARTER STORAGE BARN floating above uneven terrain on a new game: a faithful-port bug,
  not the extension's -- block BARNZ (root cause against retail's placement order; known_port_issues row).

## 2026-10-11 late -- EYECAM merged: the first extension

- `ext.eye_camera` (class A, default off) merged `a67c8b445`: `src/ext/eye_camera/` (eye_math pure + the
  game glue), three seams (Update step 6, Draw step 4 at the renderer's alpha + near 0.1, the zoom in
  camera_input.cpp), six unit tests; PlayerController's saved fields never written. Gate EYECAM at 0;
  stress faithful vs `/profile:all` identical (0.17 ms / iteration, 140 fps). `tools/stress_run.sh` takes
  `STRESS_PROFILE` (default faithful).
- **LOOKED (the user, 2026-10-11 late)**: engage, the three exit paths, the return to retail's camera and the
  zoom all good. Three defects, block EYECAM2 dispatched: children get the adult eye height; shadows do not
  render at the default distance 0.9 (back at ~3 units); an idle citizen kicks the view back to retail's
  follow until they walk again.
- Was owed: the user's look (`handins/EYECAM.md` §7: 13 checks -- eye height 0.88 units, the follow
  engage / disengage, first person, hidden / removed citizen, faithful load of a save made while following).

The design is `DESIGN.md`; the classified deviation list is `INVENTORY.md`; every block's hand-in is
`handins/<BLOCK>.md`. The gate table for this phase is `docs/runtime_port/phase_d/GATE_TABLE.md`
(rows EXT1 / EXT2).

## 2026-10-11 night -- the phase opened; scaffold, three fix pilots, verification, inventory

- **Merged on main**: EXTSCAFFOLD (`src/ext/registry.*`, `Ext::Fix()` / `Ext::Ext()`, settings keys
  `fix.<id>` / `ext.<id>` / `profile=faithful|all`, the class lock, the pseudo-mod provider type, the
  Extensions dialog + button beside Mods in the front end and the pause menu, 11 unit tests),
  FIXPILOT (`fix.icon_slide` at slot 11 AND `SetFlag`, `fix.residence_goto`, `fix.tavern_counter`;
  default on), SAVETAIL (`handins/SAVETAIL.md`, five verdicts VERIFIED), EXTINVENTORY
  (`INVENTORY.md`: RESTORE 18, FIX 7, EXT-A 1, KEEP 7).
- **Gate EXT2 at 0 problems** (faithful profile forced by `/profile:faithful` on the exe's command
  line: `tools/build.sh run`, `resave_gate.sh`, `phase_d/gate.sh`): new game 5694 / 0, tutorial
  5773 / 12, loadgame 3436 / 0, typetable `965b527a`, resave 3/3 == golden (23,453,453 bytes), unit
  1013 / 0, castmetric 2, acceptance 0 off target, 0 new warnings. EXT1 (one problem, castmetric 6)
  was the dialog's three casts and the residence faithful arm's; fixed in `e2d8be2f4`.
- **Design corrections from SAVETAIL**: a class-C chunk lives at the tail of save block 1, not after
  the file (retail checks the exact file size); every B/C extension writes a manifest there; the
  pseudo-mod's format version goes in ModInfo+0x18 (+0x10 is the package CRC); both write seams emit
  zero bytes under the faithful profile.
- **Facts learned**: a loose text `.rsc` is NOT loaded by the shipped loader and `Tools-x64.exe` is
  not on this machine, so the Extensions dialog reuses `Dialog/Mod.rsc` with its texts replaced and
  draws its icon in code (EXTSCAFFOLD hand-in). The button's tooltip still reads the Mods text.
- **LOOKED at by the user (2026-10-11 night)**: the Extensions dialog's name / author / description
  rows and the enable / disable toggles work in the front end and in game; the row icon column is empty
  (the user will author programmer art later; the slot stays). The three fixes seen working: icon_slide,
  residence_goto, tavern_counter. Fixes are global and live, NOT recorded in the save (a disable + save +
  re-enable + load shows the fix enabled): confirmed as intended by the user.
- **Next**: the first extension, class A eye-level camera (seam in `INVENTORY.md` §2).
