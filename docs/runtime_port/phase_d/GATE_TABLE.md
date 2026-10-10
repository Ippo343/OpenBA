# Phase D gate table (coordinator; one row per gated merge / move, written BEFORE the gate runs)

Kit: copied from `b9_session` at the Phase D start (`gate.sh`, `mergegate.sh`, `stepclose.sh`, `worldcmp.sh`, the
route scripts, `keepboth.py`, `reftable_diff.py`, `RESOLVE_CACHE_GONE`); `runs/baseline` = B8's close run (as B9),
warning baselines `runs/warn_baseline_{debug,perf}.txt` (B9's). `runs/` is gitignored. B11 base for the blocks:
`b9_session/runs/b11base` (main at the B9 close; Phase D's first commits are docs / Makefile-rule only).

Invariants (B9 close): new game 5683 / 0, tutorial 5773 / 12, typetable `965b527a`, resave 5/5 == golden
`994eb184a8e88db9372b5b9cd13a9ed9`, unit 920 / 0, 0 new warnings, castmetric 2.

| row | branch / step | merge commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | castmetric | routes | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| SLOTS | d0-slots (tip `9f1f1038f`) | `570b1ee31` | 0 | 5683 / 0 | 5773 / 12 | `965b527a` | 5/5 PASS (23452937 bytes) | 920 / 0 | 0 / 0 | 2 | tools (placement / road / removal as B9); orchard / stress / disaster run after the ENGTYPES merge (`runs/POSTBLOCKS_*`) | rows 1202 / 1203 / 1204 FIXED, 1270 (no decal race: six port-only early returns in OnAdded) FIXED, 1271 / 1272 LEFT; starttown's 0x100000 pre-sets and MarkBuildingCells gone; slot 12 now paints building minimap dots |
| ENGTYPES | d0-engtypes (tip `fb5faef68`) | `789295190` (the coordinator's plan-files commit landed mid-merge and became the merge commit; message amended, content = tip + the 4 plans) | 0 | 5683 / 0 | 5773 / 12 | `965b527a` | 5/5 PASS (23452937) | 920 / 0 | 0 / 0 | 2 | tools (placement / road / removal as B9) | engine/types dissolved: 8 forwarders deleted, common.h -> src/engine/global_types.h (runtime names; accepted deviation), ui/Dialogs.h / Panels.h -> src/game/Dialog/, 4 loop aliases + their 4 unit CHECKs gone (count 920 unchanged); B11 vs a rebuilt base at 2c1840ad4: 16944 functions IDENTICAL, 934 / 935 objects byte-identical stripped |

**Post-block routes (after ENGTYPES, main `c603c98dd`, `runs/POSTBLOCKS_*`):** orchardpanel exit 0 crash 0 (OrchardDiag
counts live), disaster exit 0 (2 houses ignited), sa-stress 139 fps / p95 2.42 ms (B9: 139 / 2.42), worldcmp vs B9's
CLOSE: DIFFERS as row 1203 predicts -- the 8 `[startTown::pickmap] marked` lines are gone (MarkBuildingCells deleted),
tree / pile cells carry dc20's Obstacle stamp so `[startTown::DIAG] buildable(2)` 73809 -> 49461 and mtnLo 12013 ->
10974, and 101 fewer scattered resources spawn (25687 -> 25586: cells claimed by AddToMap at RegisterEntity are
occupied for later pools). `runs/POSTBLOCKS_worldcmp/new.world.txt` (md5 4ff6354d) is the Phase D world baseline.
B11 base for the moves: `runs/b11base_premove/perf` = main's -O2 objects at `789295190` (ENGTYPES' gate build).

## Move steps (one top-level directory per commit; a pure move: B11 IDENTICAL on every object, names mapped)

| step | move | commit | gate problems | new game | tutorial | typetable | resave | unit | warnings | B11 (vs b11base_premove, names mapped) | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | code/Game -> src/game (922 files; 0 include lines rewritten -- the tree is addressed module-relatively; 1115 text files re-pointed) | (this commit) | 2 = the warning check keyed on file path: 0 new vs the PATH-MAPPED baseline (9 / 17 baseline entries gone = bodies the two blocks deleted); baselines now mapped | 5683 / 0 | 5773 / 12 | `965b527a` | 3/3 PASS | 920 / 0 | 0 / 0 (mapped) | IDENTICAL 10752 functions, 0 DIFF / SAME / ONLY (`runs/STEP1_game/b11_mapped.txt`) | castmetric 2; stale old-name objects deleted from the obj dirs |
| 2 | runtime -> src/engine (types + src folded per namespace, funcs at the root; 4 private headers renamed *_internal.h) + the 35 *_win.cpp -> src/platform/win (547 files; 2324 of 12922 include lines rewritten; 874 text files re-pointed) | (this commit) | 2 = the path-keyed warning check: 0 new vs the exactly-mapped baseline (8 / 16 gone) | 5683 / 0 | 5773 / 12 | `965b527a` | 3/3 PASS | 920 / 0 | 0 / 0 (mapped) | IDENTICAL on all 935 objects, 16865 functions, 0 DIFF / SAME / ONLY (`runs/STEP2_engine/b11_mapped.txt`) | castmetric 2; the step's text rewrite first ran with step 1's move map (a `git checkout -- tools/restructure/` restored it): the 35 seams' and 4 headers' references re-pointed in the same commit |
| 3 | dx11 -> src/render/dx11 (17 files, src + types folded; dx11/src/README.md -> SOURCES.md; 0 include lines rewritten; 179 text files re-pointed) | (this commit) | 2 = the path-keyed warning check: 0 new vs the mapped baseline | 5683 / 0 | 5773 / 12 | `965b527a` | 3/3 PASS | 920 / 0 | 0 / 0 (mapped) | IDENTICAL on all 935 objects, 16865 functions (`runs/STEP3_dx11/b11_mapped.txt`) | castmetric 2 |
| 4 | standalone -> src/app (the launcher: main, options, runtime_init, video_interface, boot_window, frame_limiter, engine_loader_gate, res_load_lock, perf_experimental, icon.rc, log_strip.h) + src/diag (56 files: watchdog, hw_watch, redzone, sampler, autostart / menu driver, the harnesses); build output standalone/build* and tests/unit/build -> build/{debug,og,perf,unit} (75 files; 2325 include lines rewritten; 439 text files re-pointed) | (this commit) | 1 = the path-keyed warning check: 0 new vs the exactly-mapped baseline | 5683 / 0 | 5773 / 12 | `965b527a` | 3/3 PASS | 920 / 0 | 0 / 0 (mapped) | IDENTICAL on 16934 functions, only unpaired object the renamed icon resource (`runs/STEP4_app/b11_mapped.txt`) | castmetric 2; icon.rc's relative .ico path fixed for the new depth (the first gate run failed at windres); tools/boot_state*.sh and cutover_run.sh re-pointed by hand (`../standalone` escapes the rewriter's boundary) |
| 5 | CMakeLists.txt + tests/unit/sources.cmake (generated from sources.mk); the docs/history paths re-pointed in 169 source comments; CLAUDE.md / STATUS.md / ENDGAME.md E / NEXT_SESSION.md §2.1 updated | (this commit) | n/a (no gate: the Makefile build is unchanged; make debug + unit rebuilt after the comment rewrite) | CMake debug exe: new game 5683 / 0, exit 0, crash 0 (`runs/CMAKE_newgame`) | | | | CMake `unit`: 920 / 0 | 113 (= the standing set) | n/a | cmake 4.4.4 + ninja 1.13 via pip; 934 TUs; build/cmake-debug |
