# B.4 (the cast metric) — close numbers

Step start `b2a5434c` (the pre-B.4 exe kept in the `oldbase` worktree) -> close `9903a0f1f` + the close
documents. Four sessions, 2026-09-28 .. 2026-10-05. The finishing session (S4) is `GATE_TABLE.md` rows 159-169.

## 1. The step

| | start | close |
|---|---:|---:|
| `castmetric.py code runtime standalone dx11 engine` (unmarked casts that are not tier I) | 46,404 (E0 base, no marker yet) | **336** |
| of it outside `dx11/` | — | **96**: 63 DEFERRED retail-difference task, 23 DEFERRED named prerequisite, 7 DEFERRED dx11 seam, 3 ruled to stay listed, **0 work** (`LEFT_FREE.md` section C, `leftfree_c.py --check`) |
| of it `dx11/` (the next step) | — | 240 |
| tier R (`// CAST-OK:`) / tier I (three shapes) | 0 / 329 | 745 of ceiling 764 in 18 categories / 447 |
| `Rt<Ns>::` POD mirrors, `pod_bridge.h`, `coll_bridge.h` | 48 + 2 headers | 0 |
| unit tests | 918 | 914, 0 failures (four tests of deleted PODs / the minted Thread table went with them) |
| gated merges | | 169 rows, the last 11 in S4; S4: 0 problems |

`git diff --shortstat b2a5434c 9903a0f1f -- code runtime standalone engine dx11 tests`: 1,535 files, 94,842
insertions, 126,334 deletions. S4 alone (`b6d3fdf62..`): 266 files, 1,432 / 1,748, 64 commits; in-scope casts
149 -> 96 (53 removed or ruled: 40 by typing, 14 marked on the floor, 1 dead overload pair).

## 2. Acceptance (`acceptance.sh`) at the close: 5 lines off target, none of them B.4 work

| line | value | why it is not 0 |
|---|---:|---|
| 01 | **0** | CLOSING FORM (ruled in S4): casts outside `dx11/` in no DEFERRED / RULED group. The PROD total is 336 |
| 02 / 03 | 0 / 0 | pass |
| 04 / 05 / 06 | 5 / 17 / 1 | all in `dx11/src/{contextdata,devicedata,imagebufferdata}.cpp`: the dx11 seam step |
| 07 | 57 | 51 in `dx11/`; 6 outside it, each on a DEFERRED cast that still exists (`citizenlist_serialize.cpp:604`, the two first-tick clamps, `documenttext.cpp:155 / :157 / :159`) |
| 08 / 09 | 0 / 0 | pass (names listed; sections A / B of `LEFT_FREE.md` were not regenerated in S4) |
| 10 | 21 | 19 template-class fields, 1 protected member (`InternalBasePtr::_ptr`), 1 miscount (`Device onLost`): ACCEPTED, the measure is a regex lower bound |
| 11 / 12 | 0 / 0 | pass |

## 3. The close checks

| check | result |
|---|---|
| `stepclose.sh` at `9903a0f1f` | mods route clean; menuquit / options / achievements exit 0; GoalDialog 1 OpenGoal line (killed at the timeout, expected); tool placement / road / removal exit 0; in-game save COMPLETE, crash log empty |
| `sa-stress` (recorded, never chased) | exit 0; update 1.67 ms/frame (p95 2.10), 139 fps, EntityList 1.42 ms |
| true release compile + `make clean` | exit 0, 939 files, exe 6,411,098 bytes |
| clean rebuild + gate `closeclean` | debug / perf / unit exit 0, 914 / 0, 0 new warnings, three routes + the -O2 route clean, typetable == baseline, resave 3/3, 0 NOT IMPLEMENTED stubs, stub set unchanged |
| `worldcmp.sh` (seed 12345 against `b2a5434c`) | identical, 118 lines — at `3e57f2ee` and at `b32b283f` (after the MapGenerator / TerrainGenerator ctor commit and the Camera copy ctor). NOT re-run at `9903a0f1f` (two merges later: RT2, LAST; neither touches world generation) |
| `devlost.sh` | COMPLETE at `d04813af` (219 round trips) and `b32b283f` (207), crash log empty. The run at the close was stopped by the harness for low memory: SEE section 4. NOT re-run at `9903a0f1f` |
| the removal tool's CalcPath by hand | **OWED: the user** (toolbar Remove button on a new game; the harness route skips CalcPath) |

## 4. Found on the way

- **The device-lost route eats 20 GB.** After the ONE injected reset every later Present reports
  0x887a0005 and the exe runs a full round trip per frame, about 100 MB each; at the 600 s kill the working
  set is 20.8 GB. This is what stopped the third session ("low system memory during the last merge's gate")
  and the S4 close. `known_port_issues` row 890; `devlost.sh` now stops after `DLMAX` (5) round trips. At the
  S4 start the same symptom had a second cause: VS Code's `cpptools` at 20 GB indexing 40 agent worktrees.
- **Three port bugs recorded, not fixed** (retail-difference task): row 861 (`GatherRawMaterials::Execute`
  hands the WorkPlaceComponent where retail hands `context` as the Time), row 820 (`ComponentEntity` serializes
  `_entityFlags` through the int overload, retail the uint one), row 860 (`ModelDescription::Serialize` checks
  the mesh group against the GraphicsMesh type). Also rows 810, 850, 880.
- **One blocker explains 13 of the 23 named-prerequisite casts**: retail open-codes `~String` / `~Array`
  (dec, MemFree, zero) where the tree's are out-of-line calls (`documenttext.cpp`, `modification.cpp`,
  `avl_trees.cpp`, `introsort.cpp:355`, `wave24_lane_gfxmat.cpp:363`). Making String's ctor / copy / release
  and `ArrayDtorImpl` inlinable as retail's unblocks all of them in one behaviour-neutral pass.
- `Vector3Slot` (a one-member union withholding Vector3's zeroing ctor) was ruled acceptable for the return
  slot of `InteractNodePosition` only (`FLOOR.md`, S4 section).

## 5. What passes but is not finished (unchanged from the third session's list unless said)

- 234 of the ~407 free functions of `exe_bodies.h` / `*_funcs.h` are members of a class in retail, still
  spelled as C functions (`LEFT_FREE.md` A). RULED in S4: a later quality pass, not B.4.
- 20 `void*`-first functions outside the allowed headers (10 tagged probes, 10 with a reason).
- New teardown behaviour only the gate has exercised (no long play session): rows 790, 791, 722, 760,
  770-775, 780, 805, 806, and S4's `behaviour:` commits (SunLight members, ParticleBoundsRecord, the 12-byte
  position slots, NamedExternalRef, Camera's partial copy, LevelArray's teardown).
- POS did not trace every `float*` callee that receives a former float[4] position (now 12 bytes): MoveTo,
  WorldToCell and the bf790 arms were read; retail's slot is 12 bytes at every one of those callers.
