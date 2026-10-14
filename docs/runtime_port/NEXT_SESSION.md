# Next session (written 2026-10-11, evening: no phase open)

Endgame B closed at B9 (2026-10-10). Phase D closed 2026-10-11: D.1-D.3 + CMake (the restructure), D.4's four waves (the
portable layer: settings file, `Platform::` seams, SDL3 window / input / cursor / audio with the vendored WMAv2 decoder,
DXVK, the ABI audit; the first Linux game build boots to the menu and into a new game under WSL2 with the world and the
resave byte-identical), then the post-close audio hunt and the PORT GAP hunt, every merge gated at 0 problems. The user
looked at and listened to every group the same day and found no issue. **Nothing is in progress.** What follows is the
complete list of what is left, each item with its owner and the condition that closes it. There is no other list.

The history: `STATUS.md` (the close log, newest first), `phase_d/GATE_TABLE.md` (every gated merge), `phase_d/handins/`
(every block's hand-in), `docs/history/endgame_b/` (the B-phase rule sheets, kits and the old `NEXT_SESSION.md`).

## 1. Where it stands

| | |
|---|---|
| main | the night-of-2026-10-11 commits (CMake gate, Linux release / perf, housekeeping) and after |
| tree | `src/game` (the exe's logic), `src/engine` (the Runtime DLL), `src/platform/{win,posix,sdl,common}`, `src/render/dx11`, `src/app`, `src/diag`; `tests/`; **the build is CMake + Ninja through `tools/build.sh`** (exes in `build/cmake-<flavour>`; the Makefile is deleted) |
| `PORT GAP` lines | **1**: a comment that says "no PORT GAP" (`citizen_lifeevent_handler.cpp:180`). The device-lost pair was the last body and is ported (§2.4) |
| known_port_issues | **0 rows OPEN**; 1292 PARTLY FIXED (§2.1); 890 FIXED (§2.4); 1295 and 1162 RULED; 1277-1284, 1320-1322 RECORDED (platform / bed facts, not defects); every duplicate row collapsed |
| invariants (every gate) | new game `[ExtLoad] calls=5694 openFileFail=0`, tutorial 5773 / 12, typetable `965b527a`, resave 5/5 == golden `971aaec2cf95` (23,453,453 bytes), unit 1002 / 0 (1010 / 0 on Linux at wave 4), castmetric 2 (the two ruled casts), `tools/abi_audit.py` OK, `tools/acceptance/acceptance.sh` 0 off target, 0 new warnings |
| perf | sa-stress 139 fps, update p95 2.54 ms (baseline 2.40; recorded, not chased: the user's rule) |
| Linux | WSL2 Ubuntu 24.04 (WSL 3.0.1), lavapipe (CPU Vulkan: dzn / the host GPU is Vulkan 1.2, DXVK wants 1.3 -- row 1322), dxvk-native 3.1.1 (`tools/fetch_dxvk_native.sh`); source copy `~/banished-main` (rsync of main; ccache), builds `~/banished-main/build/linux-{release,perf}`, game dirs `~/banished_{release,perf}`, runs `~/lx_runs/`; **release and perf flavours boot, play and stress-test clean** (unit 1012 / 0 each); `tools/linux_run.sh` routes menu / newgame / world / resave / stress |
| worktrees / branches | none besides main (the 64 block branches and `oldbase` deleted 2026-10-11 at the user's word) |

## 2. What is left

### 2.1 Code: one item (an agent can take it; `behaviour:` commit, gated on main)

**Row 1292, the region build fold.** The map's region build is retail's one member at 0x14009e8c0, called from the map
create 0x14009ef20 (between `FillZones` 0x14009c480 and the shore-link loop 0x140088b80) and from `Map::Serialize`'s load
tail (0x14009f2a6). The tree spells it as two port-only free functions, `Pathfinder_Seed` (`src/game/Game/pathfinder_seed.cpp`)
and `Region_Seed` (`region_seed.cpp`), declared in `Game/exe_bodies.h`, each with an early return when its object exists
and a `[PfSeed]` / `[RegionSeed]` stderr line. The hunt already deleted `GameUpdateLoop::Load`'s post-load pair (DRAW-2)
and wired the map create; what is left is cosmetic: one Map member, no early return, no print, the two files and the two
declarations gone. Done when: the gate at 0 problems with the resave golden unchanged (the fold writes no state), and
`worldcmp.sh` differs from the last run by the `[PfSeed]` wording only.

### 2.2 By hand, the user: the checks no route or look has covered yet

The 2026-10-11 looks covered every group in `STATUS.md`'s hunt line. These rows still carry "look owed" in their cell
with a subject that list did not name; each closes with a sentence on its row (or a new row if it is wrong):

Seen good by the user on 2026-10-11 night: the sprite-capture button (row 1296), a bridge dragged over water (row
1180), a worker added and a worker removed at the town hall (rows 1141 / 1142), a drawn road (row 1240), livestock
sounds (cows). Still unseen: a look after a forced device loss (§2.4) and the three WSLg looks (§2.3).

The END click heard once on a debug exe is dropped from the list (the user, 2026-10-11 night): row 1289 proved every
sound tail clean in unit tests and the SDL pull harness; if it is ever heard again, the sound and the moment make a row.

### 2.3 Linux (rows 1277-1285 and 1320-1322 are RECORDED facts; these are the follow-ups)

Done 2026-10-11: the release and perf flavours (built, unit 1012 / 0, menu / new game / back-to-back boot / stress
clean); row 1282 re-tested clean 2 of 2 (stays RECORDED; no stall seen); the stress bed fixed (row 1320: GameAutoPause
off for the run) and dzn tried (row 1322: Vulkan 1.2, DXVK wants 1.3; the bed stays lavapipe). Left:

- **Stress numbers**: DONE the same night. The Windows baseline is re-cut from the CMake perf exe on a quiet machine
  (`perf/baselines/current.csv`: sim 0.17 ms / iteration, 0.19 before, 24820 entities); the quiet Linux perf exe reads
  0.46 (row 1321): 2.7x slower per sim iteration on the same tree and machine. Recorded, not chased; if the user ever
  wants it chased, the first read is the allocator (the standalone allocator's lock on glibc vs Win32) and the WSL2 VM,
  with `perf` under WSL2 on the perf exe. The Linux fps / draw columns are lavapipe's (and WSLg throttles an unfocused
  window to 5 fps) and never compare.
- **Fullscreen / alt-tab / a forced device loss on Linux** (one look each under WSLg, by the user at the WSLg window;
  the forced loss with `EXTRA_ENV=BANISHED_FORCE_DEVICE_LOST=12000` on a newgame route).
- **The POSIX diagnostics** (rows 1281 / 1283: BLOCKED_DIALOG, redzone, sampler, hw_watch are Windows-only): each is its
  own task, taken only if a Linux debugging session needs it.
- **A GPU-speed Linux bed** only if wanted: a native Linux boot, or dzn once it exposes Vulkan 1.3 (`LINUX_VK_ICD`
  in `linux_run.sh` is ready for it).

### 2.4 Device-lost: DONE (2026-10-11 night; re-opened by the user that evening, deferred since 2026-09-27)

Row 890 FIXED by merge `8b89fcfd5` (opus block d5-devlost, hand-in `phase_d/handins/DEVLOST.md`, gate row DEVLOST at
0 problems): `GameUpdateLoop::OnVideoError / OnVideoRestart` are retail 0x140095970 / 0x140095930 (the terrain's four
buffers, height image, mesh and water mesh; the minimap's two images), with the menu loop's twin pair and the engine's
`ImageBuffer::Release`. The forced route (`BANISHED_FORCE_DEVICE_LOST=12000`, `runs/DEVLOST1`) now runs ONE round trip,
no later device removal, working set flat at ~315 MB for the 220 s after it (was ~100 MB per frame), and the run quits
itself. What is left of device-lost: a look by the user after a forced loss (terrain, water and minimap drawn after the
trip -- the route's logs say the uploads happened, eyes say the picture is right), and device-lost / fullscreen /
alt-tab under WSLg (§2.3). The retail crash window (a loss between the loop becoming current and its first Draw,
`DEVLOST.md` section 5) is retail's and is not guarded.

### 2.5 Housekeeping

Done 2026-10-11 at the user's word: the 64 merged block branches and the `oldbase` worktree deleted (`worldcmp.sh`'s
`oldbase` comparison is gone with it; the Phase D world baseline `runs/POSTBLOCKS_worldcmp` is what it compares against
now). The ignored run logs (2+ GB under `docs/history/endgame_b/b*_session/runs/` and `phase_d/runs/`) stay, by the
user's choice; delete at will. The Makefile and its build directories were deleted the same night at the user's word.
Still the user's call:

- **Perf (2026-10-13 night close, `docs/extensions/STATUS.md` top entry):** the three next ideas are DONE: SORTMEMO shipped (the
  draw-item sort memo, draw -14% heavy / -7% compact), WALKGRID merged as the opt-in class B row `ext.walk_batch_grid` (speed-50
  everything-on fps +8..+20%), XFORM2 measured neutral and NOT merged (tag `legacy/perf-20261013-xform2-unmerged`; the world-matrix
  memo idea is closed). Faithful draw CPU: heavy 0.97 ms, compact 0.65 ms (morning 1.66 / 1.03). No perf idea is queued; the next
  collect would be a fresh IBS pass on the merged exe. Before any uncapped fps number: Afterburner, RTSS AND RTSSHooksLoader64
  stopped (they are stopped now; restart them). The hypervisor is still OFF (`bcdedit /store ... hypervisorlaunchtype auto` + reboot
  restores WSL2 / Docker).
- **Loading speed: DONE 2026-10-14 afternoon** (`docs/extensions/handins/LOADPLAN.md` §6, `docs/extensions/STATUS.md` top entry): new
  game 9.86 -> 3.23 s (P4 3.72 s), save load 4.07 -> 3.10 s (1.19 s with `ext.quick_screens`), boot 4.74 -> 4.46 s (2.89 s with it);
  every world byte-identical (`[WorldHash]`, kit `docs/extensions/handins/loadkit/`). Boot left as is by the user (the D3D device in the
  NVIDIA driver, the fades, the menu scene). Not queued: the start town's Perlin (~0.43 s; the noise is per tile and could be precomputed in parallel ahead of the
  RNG-ordered spawn), the blend AO pass (~0.22 s), `wg.paths` (150-320 ms by seed, never profiled), cold-cache loads (every number
  is warm; package read-ahead only pays there), a real 1025^2 field in the erosion bench. The census routes randomise the seed, so
  new-game times vary ~0.1 s by map. By hand, the user: `ext.quick_screens` on -- boot, a save load, an in-game save, quit to menu, the tutorial (no flash or hang).
- **Multithreading: DONE 2026-10-14** (`docs/extensions/handins/THREADPLAN.md`, hand-ins CPULIMIT / RENDERPIPE0 / CULLPAR / RENDERPIPE1,
  `docs/extensions/STATUS.md` top entry): `sys.cpu_limit` + the roster (`src/ext/cpu/`), `ext.render_pipe` (the render thread, class A) and
  `ext.gather_threads` (the parallel cull, class A) shipped at gate 0 with every shadow 0; 4-core heavy speed 10 +67%, speed 50 +35%;
  speed-50 close-out 752 / 618 / 425 fps everything on. **The user (2026-10-14): the in-game fps improvements are COMPLETE. The next
  phase is SPEEDING UP THE LOADING** (new game / load game / boot times): not yet planned; the first step is a timed census of the
  load route (`tools/boot_state.sh` stamps, the loader thread, `[ExtLoad] calls=5694`, the world build, `PaintTerrain`) before any design.
  Threading leftovers (by hand, the user): an alt-tab in exclusive fullscreen with `ext.render_pipe` on (DXGI presents off the window
  thread; a hang reads exit 4), a toggle off / on of `render_pipe` in one session, a road-tool drag with the row on; row 1328 (one
  pre-fix quit hang, not reproduced). Deferred: a ThreadSanitizer Linux flavour (needs the hypervisor back on).
- **The next phase: CHOSEN 2026-10-11 night -- fixes and extensions** (`docs/extensions/DESIGN.md`, status in
  `docs/extensions/STATUS.md`, the classified deviations in `docs/extensions/INVENTORY.md`; gate rows EXT1 / EXT2).
  The other candidates stay on record, not chosen: macOS through MoltenVK under DXVK (ENDGAME §E item 3);
  `docs/retail_bugs.md` as a "fixed" flavour; a Linux gate (the five routes + resave under WSL2 on every merge, as the
  Windows gate does).

### 2.6a Extensions in progress (owned by `docs/extensions/`)

**Ambient occlusion** (`ext.ambient_occlusion`, shipped OFF 2026-10-14 evening): the open list is `docs/extensions/AO.md`
§7 -- the user's look per tier first, then AOTUNE (the look ruling, GPU cost per tier with RTSS stopped, the live toggle
and device-lost with the row on, eye camera, Linux / DXVK, the `capture_arm.sh` fix).

### 2.6 Tracked, not tasks

The 28 `stays free` names of LEFT_FREE §A (typed overloads over one template core, the raw-triple holders, the harness
rows; acceptance 08 at 0 against them); row 1230 (the element-loop template's residue, unobservable); row 1295 (the
menu loop's `_loadBit = -1`, RULED kept with its reason); the comment debt of `docs/history/endgame_b/b9_session/handins/RT2.md`
(banners naming deleted helpers). None of these changes behaviour; each has its row or line.

## 3. Rules that hold (do not relax)

- CLAUDE.md's hard rules: no null guard on a faithful crash (find what handed us the null); no `getenv`, flags through
  `Opt()`; poll a run with `tools/boot_state.sh --wait 300`; read `standalone_crash.log` first; no pixel diffs
  (`tools/trace_oracle/`); no doors (`tools/acceptance/acceptance.sh` at target); wiring a body is `behaviour:` and the
  consumer is read first; never `find /`; never `pacman -S`.
- The build is `tools/build.sh` (CMake + Ninja); `--jobs 4` for the gate, 2 for agents; a warning baseline is cut only
  from a clean build (`tools/build.sh clean` first); park the `.pkm` mods before a gate (a mounted mod is +1 load).
- The block shape of `phase_d/D4_RULES.md` §4 / §5 and `phase_d/GAPHUNT_RULES.md`: one `behaviour:` commit per retail body,
  B11 per touched TU, hand-in at `phase_d/handins/<BLOCK>.md`; agents never run the game, `-j2`, at most four builders;
  the coordinator alone gates (`gate.sh` / `mergegate.sh` on the MERGED tree before the merge commit; the gate table
  committed first); `RESOLVE_CACHE_GONE` stays beside the kit; kill an orphan by PID, never `taskkill /IM`; a fresh
  worktree is reset to a named commit; `git stash` forbidden; any text with a backslash goes through the Write / Edit tool.
- A resave DIFF is a finding, never a flake; a golden re-cut needs the payload / table analysis the gate prints.
- Perf numbers are recorded, not chased (the user's rule): a regression is a row, not a bisect.
- Lessons that still bite: memory `feedback_gap_hunt_team_lessons`, `feedback_agent_team_coordination_lessons`,
  `reference_d4_kit_traps` (an agent idle "waiting for a build" never resumes by itself: check ListAgents, message it).
