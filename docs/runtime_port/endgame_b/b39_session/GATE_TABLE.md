# B.3.9 gate table (coordinator). One block per merge; full summaries are the coordinator's scratch kit runs.

| merge | main | builds | warnings new | unit | routes (new game / tutorial / load) | resave | mint (vt/ti/storage) | RT_DELDTOR | extra routes | problems |
|---|---|---|---|---|---|---|---|---|---|---|
| baseline (clean rebuild, = B.3.8 step close) | ab1e0846 | ok | — | 736 | 5683/0 · 5773/12 · 4758 | 5/5 | 152/104/48 | 135 | — | 0 |
| b39-rules (mechanism + pilot: 4 dead rows, PickingMesh, SunLight) | 08e9b580 | ok (-j4; the -j12 run was stopped by the harness for low memory) | 0 | 740 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | — | 0 |
| b39-EXA (EXE-1: loop builders, copy sites, exe builders, dead scaffolding) | 41b05317 | ok | 0 | 744 | 5683/0 · 5773/12 · 4758 | 5/5 | 146/104/48 | 130 | Mods (.pkm) exit 0; tool placement / road / removal exit 0 | 0 |
| b39-COORD0 (W0: storage redirect, promoted-file strip, 5 storage cells, 21 globals, dead type-id cells, 286 reader-less registry rows) | cf9fc8e7 | ok | 0 | 749 | 5683/0 · 5773/12 · 4766 | 5/5 | 146/104/43 | 130 | — | 0 |
| b39-UIB (W1: 12 widget pairs concrete) | a1750148 | ok | 0 | 764 | 5683/0 · 5773/12 · 4758 | 5/5 | 122/104/43 | 106 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens, crash log 0 | 0 |
| b39-UIA (W1: 15 widget pairs concrete) | e564f680 | ok | 0 | 786 | 5683/0 · 5773/12 · 4767 | 5/5 | 92/104/43 | 76 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens, crash log 0; tool placement / road / removal exit 0 | 0 |
| b39-IO (W1: encodings, streams, PackageFile concrete; 3 titled behaviour commits) | d0acb883 | ok | 0 | 799 | 5683/0 · 5773/12 · 4758 | 5/5 | 78/104/43 | 72 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens; in-game save COMPLETE, crash log 0 | 0 |
| b39-C3 (caller batch: code/Game/Work type-id cells, ~100 sites) | 96cba89a | ok | 0 | 799 | 5683/0 · 5773/12 · 4763 | 5/5 | 78/104/43 | 72 | tool placement / road / removal exit 0 | 0 |
| b39-ENG (W1: Achievement x4, Audio, Collision2D, PointList concrete; 9 singletons class statics via redirect; 1 behaviour commit) | 9e3e765e | ok | 0 | 821 | 5683/0 · 5773/12 · 4766 | 5/5 | 68/104/43 | 63 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens, crash log 0 | 0 |
| b39-C2 (caller batch: code/Game/Component i-z type-id cells) | 3557ba2f | ok | 0 | 821 | 5683/0 · 5773/12 · 4787 | 5/5 | 68/104/43 | 63 | tool placement / road / removal exit 0 | 0 |
| b39-C1 (caller batch: code/Game/Component a-h type-id cells) | a2abb993 | ok | 0 | 821 | 5683/0 · 5773/12 · 4766 | 5/5 | 68/104/43 | 63 | tool placement / road / removal exit 0 | 0 |
| b39-GFX (W1: 10 Graphics classes concrete, ~EdgeMeshInstance ported (behaviour), 5 storage cells retired) | 5ca67302 | ok | 0 | 832 | 5683/0 · 5773/12 · 4766 | 5/5 | 58/104/38 | 53 | tool placement / road / removal exit 0; forced device-lost in game (Present #40000): round trip COMPLETE, crash log 0 — see note | 0 |

**Device-lost note (2026-09-27, coordinator; `devlost.sh`, `devlost_at.sh`).** Three behaviours of the forced
device-lost route, each reproduced IDENTICALLY on the pre-step-9 baseline `ab1e0846` built in a throwaway
worktree — none is a step-9 finding, all belong to `DEVICE_LOST_LOADER_RACE.md` (the user's decision):
(1) `BANISHED_FORCE_DEVICE_LOST=2000` fires in the front-end menu (Present #2000 is ~8 s in): exit 3,
`Engine::MainLoop::OnVideoError` (`mainloop_video.cpp:606`) reads `SaveLoad::_singleton + 0x58` with the
singleton null — baseline 2/2 runs the same fault. (2) `=12000` fires during the world build: exit 3 on the
loader path (`ModelComponent::Startup` → `MeshInstance::SetMesh` → `ArrayAssignCore`), the documented race.
(3) `=40000` fires in the running game: the round trip COMPLETEs, crash log 0, then REAL
`DXGI_ERROR_DEVICE_REMOVED` (0x887a0005) recoveries repeat (main 76 in 600 s, baseline 34 in 420 s) and the
scripted pause-menu quit is never reached on either. The step-9 gate for seam-touching merges is (3):
INJECT=1, COMPLETE>=1, crash log 0.

| merge | main | builds | warnings new | unit | routes (new game / tutorial / load) | resave | mint (vt/ti/storage) | RT_DELDTOR | extra routes | problems |
|---|---|---|---|---|---|---|---|---|---|---|
| b39-SYS (W1: System::Thread real ctor/dtor; Timer x5, Error x3, Configuration class statics) | 27b8ca2a | ok | 0 | 836 | 5683/0 · 5773/12 · 4766 | 5/5 | 58/104/38 | 53 | — | 0 |
| b39-C45 (caller batches C4+C5: Action / Goal / Entity / Dialog / Player / Tutorial / Game type-id cells) | 5f13d9b1 | ok | 0 | 836 | 5683/0 · 5773/12 · 4758 | 5/5 | 58/104/38 | 53 | tools x3 exit 0; Mods, Quit / Options / Achievements exit 0; GoalDialog opens | 0 |
| b39-VID (W1: 18 Video classes concrete incl. the 12 *Data; Device / Configuration class statics; 4 list heads retired) | a7868c1e | ok | 0 | 867 | 5683/0 · 5773/12 · 4767 | 5/5 | 40/104/34 | 35 | forced device-lost in game (Present #40000): round trip COMPLETE, crash log 0 | 0 (1 coordinator error: stale generated manifest after the merge, regenerated in a7868c1e) |
| b39-UID (W1+W2: UI resources, dialog family, SpriteSheet, DialogInstance, Element bases; FACTUI1 / FACTUI2 deleted) | 8fd88b17 | ok | 0 | 888 | 5683/0 · 5773/12 · 4763 | 5/5 | 26/104/34 | 22 | Mods exit 0; Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0 | 0 (1 coordinator error: the merge commit 760d51c2 did not build — a line-matching conflict resolver broke deleting_dtor.cpp; repaired structurally in 8fd88b17, resolver replaced) |
| b39-OBJ (W1: ExternalList concrete; the ONE Internal type record is the class's (behaviour); ExternalFactory / InternalFactory / Factory singletons + compiler-module handle class statics) | 37533866 | ok | 0 | 894 | 5683/0 · 5773/12 · 4787 | 5/5 | 25/103/34 | 21 | — | 0 after the re-cut below |

**Typetable baseline RE-CUT (2026-09-27, with the OBJ merge 37533866), in writing.** The gate reported 1
problem: the pointer-free type table differed from the baseline in exactly ONE line,
`[GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have non-FF typeId` → `415 entries, 415 …`. That is
the retired duplicate: the minted `?_typeInfo@Internal@Object@@` record is gone and
`DEFINE_TYPE(Object::Internal)` is the one `Internal` record (OBJ commit 14769fe2, B39_RULES D6;
`ORDER_MEASUREMENT.md` variant C measured that either winner gives the same tree and resave). Every id/name
line, the registered count (311) and the resave (5/5) are unchanged. New baseline: 32 lines, md5 201d7aa7.
| b39-CB (behaviour type-id sites: 9 commits, each read against its retail caller; CitizenAI / Happiness message targets stay 0 behind named PORT GAP constants) | 020c883e | ok | 0 | 894 | 5683/0 · 5773/12 · 4787 | 5/5 | 25/103/34 | 21 | tools x3 exit 0; Mods, Quit / Options / Achievements exit 0; GoalDialog opens | 0 |
