| merge | unit | result |
|---|---:|---|
| baseline (1d7866d4) | 516 | 0 problems |
| pilot b38-rules (addb4dfd) | 523 | 0 problems, resave 3/3 |
| RB part 1 UI bases (3f090079) | 533 | 0 problems, resave 3/3 |
| CIT (e61cae1a) | 537 | 9 problems: newgame EXEC past 16-slot table (StateMachine lazy linker); fixed on main | 
| CIT + linker fix | 537 | 0 problems, resave 5/5 |
| ACT (45b8cc2e) | 557 | 0 problems, resave 5/5 |
| TOOL (bb0ee047) | 567 | 0 problems, resave 5/5 |
| RB (65eafdc0) | 594 | 0 problems, resave 5/5 |
| PNL (03e6799f) | 634 | 0 problems, resave 5/5 |
| K1 (fc92ee69) | 645 | 0 problems, resave 5/5 |
| WRK (3eb4c02b) | 670 | 0 problems, resave 5/5 |
| MI-A (95128b08) | 676 | 0 problems, resave 5/5; tutorial TownCensus entA=1 |
| DLG (877f68c3) | 691 | 1 problem = typetable 312->311 registered / 417->416 chain: AutoSaveDialog duplicate removed (D4, intended; baseline re-cut); resave 5/5; Mods route exit 0 |
| VIS (e3f09481) | 699 | 0 problems, resave 5/5 |
| MI-B (73142f29) | 709 | 0 problems, resave 5/5 |
| LOOP (7bdc92fd) | 722 | 18 problems: every route exit 3 at ~7s, EXEC 0x3 in Graphics::Scene::Clean geo->Clean() slot 8 from MenuDialogLoopT_TickParticles; merge UNDONE (main reset to 73142f29), sent back to LOOP |
| GOAL (9fb05e19) | 723 | 0 problems, resave 5/5 |
| LOOP (a9937675, fix 723b0232) | 736 | 1 problem = resave DIFF: 128 leaked menu UI objects now freed (retail); golden re-cut 758597e2 (757e48a1 -> dd3893af); resave 5/5 on new golden; new-game calls 5686->5683 openFileFail 3->0, tutorial 5776/15 -> 5773/12 |
| CB3 (69c0bf3c) | 736 | 0 problems, resave 5/5 |
| CB1b (98edc88f) | 736 | 0 problems, resave 5/5 |
| CB4 (f3bec4e4) | 736 | 1 problem = typetable minus 15 ResolveTypeIdByNameLazy log lines (lookups now X::TypeId(); counts/chain/ids unchanged) -> gate taught; resave 5/5 |
| CB4 tool routes | - | placement/road/removal harness exit 0, crash 0 |
| CB6 (f7f7181f) | 736 | 0 problems, resave 5/5, Mods route exit 0 |
| CB5 (bda29cbd) | 736 | 0 problems, resave 5/5 |
| CB1a (8b282a76) | 736 | 0 problems, resave 5/5 |
| CB2 (abae0ac2) | 736 | 0 problems, resave 5/5, Mods route exit 0 |
| close (e24d1e7c) | 736 | 0 problems, resave 5/5 |

## Step close (b38_session/stepclose.sh on e24d1e7c) — PARTIAL, stopped by the harness for low system memory

```
== stepclose @ e24d1e7c 22:18:57
mods route: exit=0 crashlog bytes: 0 hang: no  [RtResolve] 0 misses — every cached resolve in this run found its symbol.
route menuquit: exit 0 crashlog=0 hang=no killed=no (8s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: [AutoStart] AUTOMENU_BUTTON ENABLED - will click menu id=6 after 60 menu frames and stop there|
route options: exit 0 crashlog=0 hang=no killed=no (9s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: [AutoStart] AUTOMENU_CLICKS queued: m6|[AutoStart] AUTOMENU_BUTTON ENABLED - will click menu id=4 after 60 menu frames and stop there|[AutoStart] AUTOMENU_CLICKS: MainMenuDialog=000001f2b587bcb0 id=6|
route achievements: exit 0 crashlog=0 hang=no killed=no (9s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: [AutoStart] AUTOMENU_CLICKS queued: m6|[AutoStart] AUTOMENU_BUTTON ENABLED - will click menu id=8 after 60 menu frames and stop there|[AutoStart] AUTOMENU_CLICKS: MainMenuDialog=000002300082bcb0 id=6|
route goaldialog: exit 1 crashlog=0 hang=no killed=YES (200s) TRAP=0 
  automenu: 
  goaldialog: 1 OpenGoal line(s)
route tool_placement: exit 0 crashlog=0 hang=no killed=no (36s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: 
route tool_road: exit 0 crashlog=0 hang=no killed=no (36s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: 
route tool_removal: exit 0 crashlog=0 hang=no killed=no (36s) TRAP=0 [RtResolve] 0 misses — every cached resolve in this run found its symbol.
  automenu: 
  tools: [ToolHarness] PlacementTool CREATE -> 1 (1=building materialized) template="Template\WoodHouse.rsc" cell=(123,123) after 2 candidate(s) around (124,124)|[ToolHarness] PlacementTool sequence complete.|[ToolHarness] RemovalTool sequence complete.|[ToolHarness][road] CalcPath (commit) -> 1 (1=road placed)|[ToolHarness][road] sequence complete: 1/1 span(s) placed, ~6 tiles (BANISHED_ROAD_RUNS=1 len=5)
in-game save (resave file mode): exit 1 [SaveTest] frame 661: SAVE OPERATION COMPLETE (61 frames elapsed) [gate] crash log bytes: 0 
sa-stress: exit 0
     AnimalComponent                       0.001 ms/frame   0.32%  calls/frame=     0.2  7.2740 us/call
     FireComponent                         0.001 ms/frame   0.24%  calls/frame=    44.2  0.0218 us/call
  citizens=800 sets=3 speed=10 frames=6000 interval=600 seed=31337 map=medium opt=on
  fps                           139        748  +438.9%  improved
  cpu%      : median 26, max 45
  free_mb   : median 10559, max 10726
  compiler  : median 0, max 0   (cc1plus/gcc/g++/ld/as/collect2)
  make      : median 1, max 1
  python    : median 5, max 5
  reimpl    : median 1, max 1   (this run is 1 of these)
  other_game: median 0, max 0   (Application-x64 / Tools-x64)
== done: /c/code/GIT/untitled_rev_eng_project/perf/results/20260926_222600_run
true release compile: exit 2, 13 files compiled, exe  bytes, warnings 0
clean rebuild standalone-debug: exit 0
clean rebuild standalone-perf: exit 0
clean rebuild unit: exit 0
== gate closeclean (skipbuild) ==
true release compile: NOT COMPLETED (compiler processes failed to start, Error -1073741502 = 0xC0000142, during memory pressure; 0 compile errors / 0 warnings in the 13 files it reached)
closeclean gate (skipbuild) reached: new warnings debug 0 / perf 0, unit 736 tests 0 failures, dispatch 0, manifest OK; then STOPPED — its routes/resave, mintgen, the stub-set diff and the tutorial census NOT RUN (owed)
```
