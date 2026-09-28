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
| b39-IO2 (W2: every IO vtable row retired; PersistenceWorker a real class; FileSystem / Modification / Persistence class statics; 1 behaviour commit) | 204b5776 | ok | 0 | 896 | 5683/0 · 5773/12 · 4758 | 5/5 | 21/103/34 | 18 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; in-game save COMPLETE, crash log 0 | 0 |
| b39-GFX2 (W2: no Graphics vtable minted; MeshInstance copy ctor, EdgeMesh x2, particle instances, Geometry; Particle::_rng class static (behaviour)) | 253a8693 | ok | 0 | 901 | 5683/0 · 5773/12 · 4758 | 5/5 | 12/103/33 | 9 | tools x3 exit 0 | 0 |
| b39-ENG2 to ed104779 (W2: four Engine loops, Audio::Stream, PathfinderThread on real ctors; Engine / Audio::Device singletons retired) | 0169cfa9 | ok | 0 | 904 | 5683/0 · 5773/12 · 4787 | 5/5 | 7/103/31 | 5 | — | 0 |
| b39-ENG2 7800a669 ALONE (behaviour: Engine::Loader constructed by the class at static init, _loader a class static; the mint constructs nothing) | d260cac9 | ok | 0 | 905 | 5683/0 · 5773/12 · 4766 | 5/5 | 6/103/30 | 5 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; forced device-lost in game: round trip COMPLETE, crash log 0 | 0 |
| b39-CG3 (g_TypeId_* / g_tid_* reads: Player / Tutorial / Entity / Component / Work / Action / Game) | 2472cb06 | ok | 0 | 905 | clean (5683 / 5773/12) | 5/5 | 6/103/30 | 5 | tools x3 exit 0 | 0 |
| b39-CG1 (g_TypeId_* reads: code/Game/Dialog a-o, 338 sites) | ca6bfa7b | ok | 0 | 905 | clean | 5/5 | 6/103/30 | 5 | — | 0 |
| b39-CG2 (g_TypeId_* reads: code/Game/Dialog p-z) | fa7beb85 | ok | 0 | 905 | clean | 5/5 | 6/103/30 | 5 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0 | 0 |
| b39-RF ALONE (behaviour: exe type records are class statics built at static init; abstract bases have TypeId(); chain 414; order-free dump; boot check VerifyExeTypeInfos) | efc4fb72 | ok | 0 | 905 | 5683/0 · 5773/12 · 4766 | 5/5 | 6/103/30 | 5 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0 | 0 after the re-cut below |

**Typetable baseline RE-CUT (2026-09-27, with the RF merge efc4fb72), in writing.** The gate reported 1
problem, the designed one: the type table's FORMAT changed (B39_RULES D6/D9). Old table 32 lines, new 433
(md5 728a74b9; committed beside this file as `TYPETABLE_414.txt`). Line by line:
- `[FactoryReg] Registered 311 types into Runtime _firstTypeInfo` → `Registered 310 exe type records in
  _firstTypeInfo` (the non-retail `StandaloneFactoryTest` is deleted);
- `Post-Startup _firstTypeInfo: 415 entries, 415 …` → `414 entries, 414 have non-FF typeId`;
- the 10 first-ten lines `[GameMain]   typeId=0x%08X name=X` are replaced by 414 order-free lines
  `typeId=%08X name=X parent=P`, one per chain record. Checked: all 10 old id/name pairs are present with
  the SAME id; 414 distinct ids, 414 distinct names, 0 `FFFFFFFF`;
- three `ResolveTypeIdByNameLazy` lines gone (`"StringTable"` — batch CG; `"Tool"`, `"ToolSetting"` — RF:
  the readers are `X::TypeId()` now); the other 15 lazy lines are byte-identical;
- the `[GUL::DiseaseList]` / `[GUL::Help]` lines are unchanged.
Resave 5/5 == golden and `calls=5683` / `5773 openFileFail=12` unchanged: ids are `~CRC32(name)` and no name
changed. B39_RULES §5 item 1 (no reader of an exe type id before `Factory::Startup`) is now CONFIRMED BY A
RUN: new game, tutorial, Load Game, Mods, the front-end routes and the tool routes are clean and
`VerifyExeTypeInfos` (fatal in every build) did not fire.
| b39-VS (VID W2 + SYS W3: Video::ImageBuffer and System::Thread rows retired) | a99bb175 | ok | 0 | 906 | 5683/0 · 5773/12 · 4766 | 5/5 | 4/103/30 | 4 | in-game save COMPLETE, crash log 0; forced device-lost in game: round trip COMPLETE, crash log 0 | 0 |
| b39-SS1 (storage sweep, Runtime side: Timer x5, Video::Device / Configuration, the Object / IO / Input / Engine singletons — no reader under runtime/ goes through a storage door) | 8a9400dc | ok | 0 | 906 | 5683/0 · 5773/12 · 4766 | 5/5 | 4/103/30 | 4 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; forced device-lost in game: round trip COMPLETE, crash log 0 | 0 |
| b39-SS2 (storage sweep, exe side: 20 storage rows retired; 20 engine_funcs singleton globals, 9 import-slot rows deleted) | b3b3b45c | ok | 0 | 906 | 5683/0 · 5773/12 · 4758 | 5/5 | 4/103/10 | 4 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0; in-game save COMPLETE; forced device-lost in game COMPLETE, crash log 0 | 0 |
| b39-TR (type-id residue: 99 type-id globals, 97 g_tid_ storage globals, 31 fill functions, 157 registry rows, 17 registrars deleted; 1 behaviour commit: ExternalPtr<Toolbar>::Serialize) | 8bbea516 | ok | 0 | 906 | 5683/0 · 5773/12 · 4758 | 5/5 | 4/103/10 | 4 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0 | 0 |

Typetable after TR: 428 lines (md5 b983f144) = the 433-line table minus 5 `ResolveTypeIdByNameLazy` lines whose
readers are `X::TypeId()` now (the gate's lazy-lookup rule: equal). Baseline and `TYPETABLE_414.txt` refreshed to
it so later comparisons are strict. `NOT IMPLEMENTED` hits on the new-game route: 23 → 11 (rows of deleted
readers; none new).
| b39-TIR (W3: 29 Runtime type records folded into DEFINE_TYPE — Graphics 11, Video 7, Achievement 4, Collision2D 2, Audio 2, Picking, Pathfinding, IO) | b0835c71 | ok | 0 | 909 | 5683/0 · 5773/12 · 4766 | 5/5 | 4/74/10 | 4 | tools x3 exit 0; typetable == baseline (428 lines, identical ids) | 0 |
| b39-TIUI (W3: the 68 UI type records folded into DEFINE_TYPE) | 251c9c9a | ok | 0 | 912 | 5683/0 · 5773/12 · 4766 | 5/5 | 4/6/10 | 4 | Mods, Quit / Options / Achievements exit 0; GoalDialog opens; tools x3 exit 0; forced device-lost in game COMPLETE, crash log 0; typetable == baseline (identical ids) | 0 |
| b39-OBJ3 (W3: the four Object base tables are the compiler's, Base last; Object's 6 type records folded; RT_DELDTOR machinery and FACTREST deleted; 5 behaviour commits) | 851adb15 | ok | 5 (all in standalone/rt_data_mint.cpp: two trap helpers unused, one -Warray-bounds on the empty type-info table — the mint's DEAD paths, deleted by CLOSE) | 919 | 5683/0 · 5773/12 · 4787 | 5/5 | 0/0/10 | table deleted | Mods, Quit / Options / Achievements, GoalDialog, tools x3, in-game save, forced device-lost in game: see the run lines below | 0 functional; the 5 warnings are owed to CLOSE |

| b39-close (W4 closing pass) + `-fno-lifetime-dse` | a699884d + flag commit | ok | 0 | 914 | 5683/0 · 5773/12 · 4758 | 5/5 | 0/0/2 | — | acceptance at target; `newgame_perf` (-O2 exe) exit 0 |

## Step close (2026-09-27)
Every route of `stepclose.sh` clean (see STATUS, B.3.9 block) EXCEPT `sa-stress`, which crashed on the -O2 exe:
heap corruption in `~MeshInstance` at the menu shutdown (third member-destructor free, `_materials`). Reproduced
twice, and on the plain new-game route with the perf exe; the debug exe passes both. Disassembly of
`rtcoll::ArrayDtorImpl` inlined through `a->~Array()` showed the `_data = nullptr` stores deleted (lifetime DSE).
Fixed by `-fno-lifetime-dse`; clean rebuild + full gate 0 problems; `sa-stress` exit 0.
**Coordinator error 3:** no per-merge gate ran the optimised exe, so the merge that introduced the double free
(a GFX block: real `~MeshInstance` with `Collection::Array` members) is not identified. `gate.sh` now runs
`newgame_perf`.
Device-lost at the close: killed by the harness for low memory (game process 6.6 GB) -- NOT a pass, NOT a
failure; the user reported the terrain texture sliding with the camera during that run. Not reproduced on a
plain new game.
