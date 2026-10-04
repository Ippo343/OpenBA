# B.4 gate table

Every row: the coordinator's `gate.sh` on main after the merge (clean-rebuild warning baseline cut at
`b2a5434c`: debug 100, perf 201). Invariants: new game `calls=5683 openFileFail=0`, tutorial
`5773/12`, typetable 428 lines md5 `b983f144`, resave == golden, unit 0 failures, stub set 0.
`metric` is `castmetric.py code runtime standalone dx11 engine` (to remove; floor columns apart).

| # | label | main after | what | builds / unit | routes | resave | extra routes | problems |
|---:|---|---|---|---|---|---|---|---:|
| 0 | baseline | `b2a5434c` | clean rebuild, warning baseline | exit 0 · 918 / 0 | newgame 5683/0 · tutorial 5773/12 · loadgame 4766 · perf exe exit 0 | 3/3 | — | 0 |
| 1 | R0-EXE | `e738df25` | 49 unreferenced exe C bodies deleted; four grab-bag files split | exit 0 · 918 / 0 · 0 new warnings | 5683/0 · 5773/12 · 4766 · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 2 | E0 | `9fd39a60` | the metric knows the floor; 162 marker lines; `acceptance.sh`; `FLOOR.md` | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4758 · perf 0 | 5/5 | — | 0 |
| 3 | W0-H1 | `6d4ade58` | seven citizen-side headers: retail types, all asserted | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4758 · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 4 | R0-RT | `321e650c` | 697 unreferenced runtime bodies and doors deleted (merge `f41a352a` + the coordinator's `allocator.cpp` correction `321e650c`) | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4767 · perf 0 | 5/5 | ui ok · tools ok · devlost COMPLETE=103, crash log empty, killed at 600 s (as before) | 0 |

Coordinator errors, each fixed in its own commit:
- `f41a352a` resolved the `allocator.cpp` conflict at a comment banner of `=` characters (the
  script searched for the separator as a substring): the merge commit kept the region R0-RT deleted
  and a stray separator line, and does not build. `321e650c` is the file as R0-RT left it with E0's
  33 marker lines. **A bisect across `f41a352a` must skip it.** Conflict separators are matched as
  whole lines (`^=======$`).

| 5 | W0-H2 | `b6f73067` | GameUpdateLoop typed and asserted; GameSettings, GameContainer; five component headers; Map, PlayerController, PlayerToolManager; `behaviour:` sky mesh store order 0x140097380 | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4766 · perf 0 | 5/5 | ui ok · tools ok | 0 |

**W0 closed at `b6f73067`.** Metric 46,733 (all casts, block 0) = 46,226 to remove + 507 floor at the
base; at W0's close **45,357 to remove** (tier I 327, CAST-OK 180), bridge markers 6,849 -> 6,535.
`acceptance.sh`: 9 of 12 lines off target.

| 6 | W1-T | `be8b5258` | `ComponentEntity::FindComponent<T>()`, `FindComponentByTypeId`; `Component::_entity` / `_components` typed; 26 faithful find copies are `B4 FORWARD` forwarders | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | tools ok | 0 |
| 7 | W1-T-beh | `c7aedffa` | `behaviour:` 0x14000b6f0 finds ConsumeProduceComponent (cell 0x1401c7350) | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | tools ok | 0 |

Metric at `c7aedffa`: **45,262** to remove. The file map's build-directory exclusion swallowed the
twelve `build*.cpp` files (866 casts) until `76f22aff`.

| 8 | W1-R-UI-a | `6d2462aa` | runtime UI a (dialog, element, font, document text): 863 -> 135 in 97 files | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | ui ok · tools ok | 0 |

Coordinator error 2: the merge `18662af7` (R-UI-b) does not build — the two UI blocks ran in
parallel and each typed a field the other reads (`UserInterface::_toolTipDialog`). Fixed in the next
commit. **A bisect must skip `18662af7`.** `mergegate.sh` now builds the merged tree BEFORE the
merge commit is made.

| 9 | W1-R-UI-b | `19d81b4b` | runtime UI b (UserInterface family, sprite context / capture, description serializers): 862 -> 197 in 78 files (merge `18662af7` + the fix) | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 10 | W1-R-IO | `9da51f45` | runtime io / save / object: 955 -> 419 in its files; `FileInfo::winHandle` is a HANDLE | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | ui ok · tools ok (tool routes re-run after the lockup) | 0 |

Metric at `9da51f45`: **43,331** to remove.

**Machine lockup, 2026-09-29 ~00:57** (five agents building at -j3 while the coordinator ran routes):
the session died; the game-dir lock was left held by `coord` (released by hand); one file in an
agent worktree was written as NUL bytes (`runtime/types/ui/UserInterface.h` in COMP-b's worktree,
restored from its index); main and the object database were intact (`git fsck` clean). From here:
at most FOUR agents, building at **-j2**.

| 11 | W1-COMP-b | `4681d217` | the citizen family: CitizenComponent / CitizenAIComponent bodies are members: 3406 -> 2059 in its files | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | ui ok · tools ok | 0 |
| 12 | W1-R-GFX | `d5644a3f` | runtime graphics: 1409 -> 159 in 50 files | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · loadgame ok · perf 0 | 5/5 | ui ok · tools ok · devlost COMPLETE=221, crash log empty, killed at 600 s · `sa-stress` exit 0, crash log empty, update 1.91 ms/frame (p95 2.68; recorded, not chased) | 0 |
| 13 | W1-WORK-a | `58f05f12` | Work a-f and Entity: EntityList, CitizenBehavior, PathGraph members: 1697 -> 583 in 36 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 14 | W1-WORK-a-beh | `4c4c75c1` | `behaviour:` EntityList tree counts +0x70 / +0x80 are int32 (0x1400c3fe8 / 0x1400c3ff7) | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 15 | W1-COMP-a | `6fced53f` | Component a: 1383 -> 341 in 64 files; four `behaviour:` commits (Cemetery OnRemove 0x1400471c4, Boat Persist 0x140046635, AmbientEnvironmentDescription Serialize 0x140043a25, AutoPickup Startup 0x140044990) gated together | exit 0 · 918 / 0 · 0 | 5683/0 · 5773/12 · 4767 · perf 0 | 5/5 | tools ok | 0 |

Metric at `6fced53f`: **38,598** to remove.

Notes:
- Row 13: `build_actions` calls `FindComponent<CharacterComponent>()` where the tree's helper skipped
  a type id of 0 or -1 (a guard of the tree's, not retail's; block T). No value changes: the type is
  always registered. A guard removed without a `behaviour:` title.
- Row 15: the merged tree did not build (COMP-a typed `Component::_description` and split
  `AutoPickupComponent::field38`; WORK-a's code used both). `mergegate.sh` stopped BEFORE the merge
  commit; the fixes are in the merge. `componententity.cpp` carries two `static_cast<Description*>`
  marked LEFT: `ComponentDescription::_subDescs` is `void**` (COMP-c types it).
- Row 12: R-GFX's B11 attribution of its 154 differing functions was not handed in; two
  restructurings (`BeginDrawGroup`, `Render`) add zeroing stores retail does not have.

| 16 | W1-R-VIDEO | `612f0962` | runtime video: Device, Context, RenderState, the programs by name; `Device::deviceData` typed: 587 -> 41 in 44 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · devlost COMPLETE=194, crash log empty, killed at 600 s | 0 |

Metric at `612f0962`: **38,046** to remove (tier I 330, CAST-OK 237). Floor ceilings raised in
`FLOOR.md`: `crt` 5 -> 7 (the two `qsort` comparator parameters in `saveload_saveinfo.cpp`, R-IO),
`dx11_seam` 27 -> 39 (the twelve `DeletingDtor` vector arms in `CompanionData.h`, R-VIDEO).

| 17 | W1-DLG-e | `c6681a14` | Dialog e: TradeUI, TownHallUI, WorkPlaceUI, UpgradeUI: 2268 -> 275 in 20 files; three `behaviour:` commits (~TradeUI frees its arrays 0x140086a50, OnDismissButton posts event 10 0x1400819a9, OnProfessionSpinner 0x14007dd10) gated together | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · trade harness on the stress town: 17 trades through the converted checkout (+4,200 each), then exit 3 in `TradeWork::Validate` — **the same crash on the base `b2a5434c`** (19 trades): pre-existing, `known_port_issues` 13 | 0 |

Metric at `c6681a14`: **36,053** to remove.

| 18 | W1-DLG-a | `ae7b443e` | Dialog a: DebugUI, EntityUI, CitizenUI, BuildUI, DestroyUI, AchievementDialog and the smaller panels: 1762 -> 133 in 36 files; `behaviour:` CropUI::OnCreated calls SelectResourceUI's 0x140078be0 (0x1400b2b50); the two literal vtable-index calls of `destroyui` are gone (NEXT_SESSION §0.6) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `ae7b443e`: **34,424** to remove.

| 19 | W1-DLG-b | `ec3770d5` | Dialog b: EventNotifyDialog, ModDialog, ModDetailsDialog, HelpUI, MainMenuDialog, NewGameDialog, EventLogUI, GoalDialog …: 1733 -> 164 in 43 files; `behaviour:` FollowDialog title 0x140068f70, EventLogUI one flag byte per check box 0x140067640 (neither dialog is opened by a gate route) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 20 | W1-COMP-c | `d30aa937` | Component c: Disease, Destroy, Growth, CropField, Fire, ClearArea, Happiness, Decal, Floor, Fishing, Healer, Forester …: 1745 -> 405 in 121 files; `behaviour:` EnvironmentDescription::Serialize overloads 0x1400940f9 / 0x140094128 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, crash log empty, update 1.88 ms/frame (p95 2.71; recorded) | 0 |

Metric at `d30aa937`: **31,512** to remove.

Owed, found by the blocks (each a `behaviour:` commit of its own, not yet made):
- `CheckEducation` 0x14010f7a0 calls the StorageComponent finder 0x140006270 in retail; the tree
  calls the Residence finder and adds a null-guard bail (COMP-b).
- `AnimAudioEvent::Serialize` calls the StringTable serializer; retail calls 0x1400019a0 at
  0x140087efa (COMP-a).
- `PackageFile::GetFileInfo` returns a bitwise copy; retail uses the FileInfo copy ctor 0x18000c110
  (R-IO). `Graphics::AnimationState` is 0x18 in retail (ctor 0x1800fdaf0), 0x48 in the tree (R-GFX).
- HelpUI's history array (retail frees it at 0x140011cf0) and EventLogUI's three arrays
  (0x1400114b0) leak; `MainMenuDialog` OnOpened / OnClosed vs the Slot5 / Slot4 ports 0x1400b3c60 /
  0x1400b3cd0; `EventNotifyDialog::OnUpdate` vs 0x140067c30 (DLG-b).
- `UpgradeUIConfig::Serialize` validates against `ComponentEntity::TypeId()`; retail 0x140086fb0
  uses ComponentDescription (DLG-e). Retail's settings Save passes the raw flag byte 0x1400989d7
  (W0-H2).
- `NaturalResourceComponent` slot 8 0x140125c40 is NOT installed: it finds a ParticleComponent and
  reads its +0x28 as a MeshInstance (COMP-d asks for a ruling).
- `FindComponent<T>()` does not skip null component slots (retail does not either); the tree's old
  finders did. Safe while `ComponentEntity`'s ctor never stores a null slot (it skips a component
  that has no factory).

| 21 | W1-COMP-d | `34d15139` | Component d: Herd, Particle, Map, MultiObject, Immigration, Nomad, Orchard, Pasture, Model, NaturalResource …: 1934 -> 274 in 85 files; six `behaviour:` commits (Herd OnRemove 0x140121e20, MultiObject OnRemove 0x1401257c0, multiCount 0x140125880, MapComponent rect 0x1401243e0, pasture livestock position 0x140128670, ModelComponent's MeshInstance / Transform real members 0x140124d20 / 0x140124cd0); reconciled with COMP-c on its branch before the merge | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · devlost COMPLETE=188 · `sa-stress` exit 0, update 2.03 ms/frame, p95 3.33 (flagged; agents were building; recorded) | 0 |
| 22 | W1-COMP-e-shared | `26af0d62` | Component e, shared headers first: StateMachine, StorageComponent::Item, RawMaterial, Residence families and roster members, StatusIcon SetFlag: 1212 -> 728 in 22 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 23 | W1-GAME-a | `bf5c73dd` | Game a: every pointer member of GameUpdateLoop has a class; MapCell; six new headers: 1645 -> 526 in 33 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · devlost COMPLETE=219 · `sa-stress` exit 0, update 1.77 ms/frame, p95 2.57 | 0 |
| 24 | W1-DLG-c | `1d5b30a6` | Dialog c: Options, SaveLoad, ProfessionUI and seven panels: 1747 -> 170 in 24 files; four `behaviour:` commits (three destructors free what retail frees 0x1400774e0 / 0x1400132b0 / 0x1400b80f0; PastureUI calls ComputeCapacity 0x140128570) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `1d5b30a6`: **25,780** to remove. `acceptance.sh`: accessor definitions 97 -> 33, accessor
calls 951 -> 440, puns 253 -> 140, bridge lines 6,535 -> 3,140, C-body declarations 783 -> 619,
`void*`-first functions 1,204 -> 854.

Owed, added:
- Four differences of the save format the tree has and retail does not (GAME-a; each a
  `behaviour:` commit that re-cuts the resave golden): `ProfessionResourceEntry`'s Time is written
  at +0x14, retail 0x14008f760 uses +0x18; retail resizes the +0x150 array to 0x47 with a
  zero-extend fixup on load; on version 5 the tree clears the +0x160 tree, retail +0x140; the
  EventLog entry colour goes through the int overload, retail the uint one.
- `m_tutorial` holds a `WorkScheduler` and `m_imageCapture` the `Minimap`: rename in the second pass.
- `optionsdialog.cpp:745,748` store `_checked != 0` into `Input::Configuration`'s flags: raw byte
  copy or not, per retail (R-ENG settles the field type).
- `TickIdle`'s allocation-failure path was dropped (COMP-f) without reading retail.
- Commit messages with an UNVERIFIED sentence: COMP-d `9a0c288e` ("the next update re-grounds it
  either way"), COMP-f `a61be2e2` (the load-time recompute), DLG-b's GoalDialog commit ("46 -> 0":
  the base was 70).

| 25 | W1-COMP-f | `c544190a` | Component f: TradeComponent arrays and stock tree, TradeDescription merchants, WildAnimal, Well, Work, WorkPlace, Zone: 992 -> 146 in 39 files; `behaviour:` WorkComponent::OnAdded sets the worker count 0x14012e6f0 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.78 ms/frame, p95 2.53 · trade harness: 18 trades through the converted trade component, then the known crash (`known_port_issues` 13) | 0 |
| 26 | W1-COMP-e | `c7ec34a1` | Component e, the rest: Population, Spawn, StorageDisplay, Terraform, Quarry, Picking, Tombstone, Tracker, ResourceProduce: 1553 -> 207 in its files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.97 ms/frame, p95 3.04 (flagged; five agents were building; recorded) | 0 |

Metric at `c7ec34a1`: **24,792** to remove.

**Scope for this session, set by the user (2026-09-29):** the DANGEROUS casts go completely (any
spelling of a read by offset or of a `void*` / integer receiver reinterpreted as a class; the
metric's total); only safe casts stay (the `CAST-OK` floor, the three tier-I shapes, arithmetic and
same-hierarchy `static_cast`). The differences from retail (`known_port_issues` 14) are a separate
task for another session: blocks record them and do not schedule `behaviour:` commits for them. A
cast that can only be removed by changing behaviour is LEFT and listed with the row that blocks it.

`sa-stress` p95, recorded only (the 95th percentile moves with the machine's load; the mean does
not): base 2.40; COMP-c 2.71; COMP-d 3.33; GAME-a 2.57; COMP-f 2.53; COMP-e 3.04. Mean update:
1.92 / 1.88 / 2.03 / 1.77 / 1.78 / 1.97 ms.

| 27 | W1-R-ENG | `59975715` | runtime engine / application / achievement / input / audio: 590 -> 65 in 66 files; settles `known_port_issues` 14e from retail (the input flags are `bool`; the options dialog should copy the check box byte raw) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `59975715`: **24,258** to remove. Floor ceilings raised in `FLOOR.md` after the coordinator
read the marked lines: `win32` 34 -> 56 (Win32 handle parameters on NAMED fields: `HWND`,
`HINSTANCE`, `CRITICAL_SECTION*`, `WAVEFORMATEX*`; the class headers do not include `<windows.h>`),
`crt` 7 -> 9 (`qsort` comparator parameters).

| 28 | W1-WORK-c | `5beafd9a` | Work c: the Work base, the work_* leaves, accessors and action builders, TradeWork, PlantArea, PlantOrchard, TendArea: 1724 -> 300 in 33 files; `known_port_issues` 13 and 14 updated | exit 0 · 918 / 0 · 0 (one standing perf warning in `work_act_consume.cpp` changed its text when the type was named, `uintptr_t [0]` -> `ConsumeProduceRecipe* [0]`; the gate counted it as new) | ok | 5/5 | tools ok · `sa-stress` exit 0, update 1.80 ms/frame, p95 2.65 | 0 |

Metric at `5beafd9a`: **22,834** to remove.

| 29 | W1-DLG-d | `ee01e975` + `5c6051a0` | Dialog d: StatusBarUI, ScenarioDialog, ToolbarDialog, ToolUI, StorageUI, SelectResourceUI, SpriteCaptureUI, StatusIconUI, the TownHall helpers: 1499 -> 51 in 25 files; `behaviour:` TitleDescToolTipUI finds its texts through the dialog 0x14007b730, SpriteCaptureUI / NoiseUI slots run their bodies, the focus-entity button passes the entity 0x1400b92a0, `~StatusIconUI` frees its icons 0x140014370 | exit 0 · 918 / 0 · 0 (the merge alone had 4 unused-variable warnings in the perf build; `5c6051a0` marks the log-only locals) | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `5c6051a0`: **21,374** to remove.

| 30 | W1-WORK-b | `c8d6a8d6` | Work b: MarketWork, GatherRawMaterials, Forester, Pasture, Herbalist, Hunt, Pickup, LocationWork, HarvestArea, HarvestOrchard: 1784 -> 184 in 43 files; HarvestArea / HarvestOrchard +0x58 is an `Array<bool>` | exit 0 · 918 / 0 · 0 (the retyped `work_act_consume.cpp` warning of row 28 flagged again: the coordinator's baseline file was left unsorted) | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.67 ms/frame, p95 2.31 | 0 |

Metric at `c8d6a8d6`: **19,774** to remove.

| 31 | W1-ACT-a | `af21c719` | Action a: route, drop, walker, placement, herd, road, hunt, move, raw-material and complete bodies typed; `Action::_next`, `ActionStack`, the grid walker, road vertex and work-id node types; PathfinderThread and Map +0xA8 storage named: 2439 -> 331 in 21 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.64 ms/frame, p95 2.03 (no agents building) | 0 |

Metric at `af21c719`: **17,621** to remove. **B.4 PAUSED here by the user (2026-09-29): no agent is
running, no merge is in progress, every handed-in block is merged and gated.**

**Resumed 2026-10-02** (ACT-b, GAME-b, PLAYER, TUT). `ff3a8313`: `PlayerToolManager` +0x00 typed `UI::DialogInstance*` by the coordinator (asked by PLAYER; debug build, unit 918 / 0).

| 32 | W1-GAME-b (first part) | `e36cb873` | Game b: goals serialize, MenuDialogLoopT, MenuScene, the Game dtor, MapGenerator, Minimap paint, pathfinder seed, MusicList, NameDatabase, Profession / ProfessionList, Scenario; WorkScheduler root and nodes typed: 1526 -> 639 in 44 files (geometry and serialize files still to do) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.84 ms/frame, p95 2.47 (agents building) | 0 |

Metric at `e36cb873`: **16,728** to remove.

| 33 | W1-TUT | `c7e1120a` | Tutorial / Goal: every Goal and Tutorial class by name; `StatGoal::StatValue`, TutorialInstance and TutorialSetupDialog members; `NamedExternalRef`; the second CalcMaxWorkers port forwards to the member: 1320 -> 79 in 68 files (two by-name fixes in the merge for GAME-b's ProfessionList / Scenario typings) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok (tutorial and goal-dialog routes unchanged) · tools ok | 0 |

Metric at `c7e1120a`: **15,485** to remove.

| 34 | W1-ACT-b | `b3e86f74` | Action b: the 17 action classes by name, Persist through `Action::Persist`, typed ctors; `RegionGraph::Init`, `RegionPairCache::Expire` members; six storage lookups lose a null test retail lacks (14d): 1183 -> 90 in 38 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok · `sa-stress` exit 0, empty crash log, update 1.88 ms/frame, p95 2.79 (flagged; three agents building; recorded) | 0 |

Metric at `b3e86f74`: **14,368** to remove.

| 35 | W1-PLAYER (first part) | `0dd2b4c0` | Player: Tool, PlacementTool, RemovalTool, RoadTool, BridgeTool, DragPlacementTool, FencedTool, PathTool by name; `behaviour:` the tool overlay arrays grow as retail's inlined grow does 0x1400dc542 / 0x1400dd47c (first record capacity 1, the tree had 3): 2230 -> 935 in 49 files (eight files still to do; `838dc71f` inside the branch does not build: `git bisect skip`) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok (placement, road placed) · `sa-stress` exit 0, update 1.78 ms/frame, p95 2.44 (agents building) | 0 |

Metric at `0dd2b4c0`: **13,080** to remove.

| 36 | W1-GAME-b (second part) | `266bd17f` | Game b: the geometry and serialize bodies by name; the `TradeWork_impl_*` bodies are WorldGeometry members (`Serialize` 0x1400b04d0, `BuildTerrainMesh` 0x1400ac1e0, `UploadHeightImage` 0x1400ac780 ...); `WorldGeometryElements.h`; one shared typed external-ref serializer: 1526 -> 134 in 44 files (block finished) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 2.01 ms/frame, p95 3.31 (agents building); `devlost.sh` not run (owed at the close: geometry draw changed) | 0 |

Metric at `266bd17f`: **12575** to remove.

| 37 | W1-GAME-c (first part) | `bd70c96f` | Game c: weather, worldgen.cpp, the water mesh, the erosion and river stages by name; `WorldGeometry::BuildTerrain` 0x1400b0230, `BuildFromHeightfield` 0x1400ac4b0, `BuildWaterMesh` 0x1400ab260, `WeatherSimulation::Update`, `WeatherParticleGeometry::Configure` members; `TerrainHeightfield.h`; the first agent's rewritten water weld restored to the original: 1497 -> 1057 in 29 files (starttown and 13 files still to do) | exit 0 · 918 / 0 · 0 | ok | 5/5 | `sa-stress` exit 0, update 2.07 ms/frame, p95 2.97 (agents building) | 0 |

Metric at `bd70c96f`: **12135** to remove.

| 38 | W1-R-CORE | `06c62180` | runtime core: Array element operations by name through `AssignAs` / `InitAs` / `DestroyAs` (about 35 element types), the picking scene grid on typed arrays, Thread and RtSink typed: 941 -> 329 in 108 files; floor ceilings raised (FLOOR.md) | exit 0 · 918 / 0 · 0 | ok | 5/5 | see row 39 for tools / stress | 0 |

Metric at `06c62180`: **11,523** to remove.

R-CORE (row 38) routes at `06c62180`: ui ok · tools ok · `sa-stress` exit 0, update 1.89 ms/frame, p95 2.72 (three agents building).

| 39 | W2-STANDALONE (first part) | `7229ae9e` | standalone: the four game-logic files moved to `code/Game/Game`; autostart, modal_driver, mouse_inject, stress_harness, main and ten harnesses by name: 1746 -> 1286 in 78 files (tool_harness and 17 harnesses still to do) | exit 0 · 918 / 0 · 0 (the gate counted 10: the moved files' standing warnings under their new paths; baseline paths updated) | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.80 ms/frame, p95 2.41 (agents building) | 0 |

Metric at `7229ae9e`: **11,063** to remove.

| 40 | W1-PLAYER (second and third parts) | `558bbe9a` | Player: PathTool arrays are `Collection::Array` members (retail dtor 0x1400de950, ctor 0x1400de9e0, grows 0x140026ba0 / 0x1400de2f0 / 0x1400de400), `PathTool::SetSelected` 0x1400dd710, the 0x38-node AVL templates, edittool, tool_base_trade, fencedtool_calcpath, the removal CalcPath helpers are `WorkScheduler` / `DestroyComponent` members, `AddDecal`: 2230 -> 287 in 49 files (highlight tess and render left for the second pass) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 2.15 ms/frame, p95 3.24 (agents building) | 0 |

Metric at `558bbe9a`: **10415** to remove.

| 41 | W1-GAME-c (other files) | `2149b438` | Game c: the range and coastline stages, town storage seed, the workplace registry, `MusicPlayer::Update` 0x1400a40e0, `GameUpdateLoop::TakeScreenshot` 0x140095da0, tornado, uifade, toolbar, terraingenerator, worldsetup reload: 512 -> 71 in 20 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | routes with row 42 | 0 |
| 42 | W1-GAME-c (starttown) | `d27d421e` | Game c: starttown's start config, map, entity list, spawn passes, founding-house list, residence bring-up, storage wiring; MapGenerator pools `DynArrayHeader<T>` (retail dtor 0x14001aea0); eight doors replaced by their members: 545 -> 195 in starttown.cpp (citizen-side functions left for the second pass) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0, update 1.81 ms/frame, p95 2.34 (agents building) | 0 |

Metric at `d27d421e`: **9,619** to remove.

| 43 | W2-TESTS | `31b50bd5` | tests: InterpolatedState components by name, a real `IO::Stream` subclass for FakeStream, `Time` built by field, a literal copied instead of `const_cast`: tests row 813 -> 796 (the rest is the tests' subject, or waits on the runtime POD mirrors and the minted IO tables) | exit 0 · 918 / 0 · 0 | ok | 5/5 | — (tests only) | 0 |

| 44 | W2-STANDALONE (second part) | `6a3ccdca` | standalone: eighteen harnesses and diagnostics by name (food panel, scene watch, death, food production, orchard, market overstock, disaster, pasture, pause menu ...): 1286 -> 617 in 78 files (`tool_harness.cpp` 455 left for the second pass); floor ceilings `crash_walk` 86, `hash` 5 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok · `sa-stress` exit 0 on a QUIET machine (no agent building): update 1.68 ms/frame, p95 2.15 | 0 |

Metric at `6a3ccdca`: **8,950** to remove (tests row 796). **B.4 FIRST PASS DONE; the session stops
here by the user's decision (2026-10-02): no agent is running, no merge is in progress, every block
is merged and gated.** `sa-stress` during the pass, recorded only (base 1.92 / 2.40): mean 1.64 - 2.15,
p95 2.03 - 3.31 with agents building; the quiet run above is the comparable one.

## Second pass (2026-10-03) — blocks cut by blocker (`PASS2_FILEMAP.md`, `PASS2_BRIEF.md`)

The `runs/` baseline was re-cut from a clean rebuild of `0ba19d11` (0 problems; warning sets 50 debug / 120 perf; now tracked in git). Columns as above.

| 45 | P2-S2 | `02071596` | Work slots [4]-[7], [10] and Action [9] `GetTarget` typed from retail (dispatchers 0x1400f7b60 / 0x1400f8620 / 0x1400f8760, 0x1400eb910, 0x1400c5a50); 88 overrides; two dead forwarders deleted: 8950 -> 8867 | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 46 | P2-S3 | `527de776` | the controller state table's handlers are typed function pointers, table and entries templates on the owning component; 151 casts; `FindOrLoad` const LEFT (retail drops it in inlined template code) | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 47 | P2-T1 | `54bec6a8` | `tool_harness.cpp` by name, `EditTool_Harness*` typed: 484 -> 64 in four files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok (placement, road placed; same cell and candidate count as the pre-block harness, run twice each) | 0 |
| 48 | P2-S1 | `8196d1cf` | `External::Serialize`, `Internal::Persist`, `OnSerializeSettings` and every override take `IO::Stream&`; 325 body casts; seventeen null-stream guards retail lacks dropped (`known_port_issues` 125-126) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `8196d1cf`: **7,986** to remove (wave S done).

| 49 | P2-ENT (header point) | `b92eb128` | `CitizenBehavior.h` node types: `CitizenBucketNode<Key>` 0x38 (retail 0x140017460), `PathCostNode` 0x30, `PathCellPair` | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 50 | P2-ENT | `9b703063` | citizenbehavior, entitylist, componententity by name; `ComponentEntity::_entityList` typed, +0x20 is `System::Time _lastUpdateTime`; four dead forwarders deleted: 161 -> 41 in five files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 51 | P2-CIT-4 | `d8b79c39` | citizencomponent, _serialize, _lifecycle by name; four `CitizenComponent` timers are `System::Time`: 268 -> 50 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 52 | P2-GAME-2 | `0923dbed` | the game-world teardown bodies take typed receivers; load / serialize element operations typed: 286 -> 133 | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 53 | P2-R-COLL | `10db088d` | the generic AVL bodies take the Set; typed node structs; set_core without casts; hand-offs to the erased core marked `elem_stride`: 243 -> 137 in 13 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `10db088d`: **7,388** to remove.

| 54 | P2-CAI-6 | `98dec703` | citizenai_work_scheduler reads the scheduler trees by name (`known_port_issues` 200-201): 218 -> 17 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 55 | P2-CAI-4 | `9793505e` | `WorkCandidateNode` 0x30 / `WorkCandidateTree` (retail 0x1400f8000, 0x1400f79b0), citizenai_housing_idle by name, seven dead thunks deleted (rows 210-211): 202 -> 7 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 56 | P2-WORK-1 | `ba34cbd9` | ClearArea's set roots, BuildRoad's material / road type typed; cleararea, buildstructure, buildroad, clearstorage by name (rows 220-221): 243 -> 56 | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 57 | P2-R-OBJ | `24f32afa` | `TypeNode` balance / height two int16 (retail 0x180030540, row 100 CONFIRMED), the type tree and External's sets by name, `PathStackElement`: 200 -> 123 in 25 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `24f32afa`: **6,728** to remove.

| 58 | P2-CAI-5 | `7c5a44e1` | citizenai_leisure by name; `LeisureCandidateSet.h` (retail 0x14010a560); three dead trampolines deleted (rows 240-241): 162 -> 11 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 59 | P2-CAI-2 | `db11a004` | citizenai_executor, findhelpers, findtarget, gather by name; `CitizenAI_sub_78c0` typed (rows 250-252): 299 -> 38 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 60 | P2-WORK-2 | `07520c0d` | consumeproduce, fishingwork, firework, marketwork, pickup, pasturework, gatherrawmaterials by name (row 260): 309 -> 181 in 20 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 61 | P2-R-GFX | `3183f57f` | the 0xb0 mesh element is `GraphicsMesh::DetailLevel` (retail 0x1800fefc0, row 102 CONFIRMED); `AnimationGroup::Tag`, `VertexProgram::Element` / `Stream`; `behaviour:` `GraphicsMesh::_detailLevels` / `_skinMaterials` are `Array` members (retail ctor 0x1800ff170, dtor 0x1800ff270): 181 -> 145 in 45 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 62 | P2-CAI-3 (+ CIT-3) | `9732f3fe` | citizenai_growup, handlers 1-3, citizenaicomponent by name (row 280): 212 -> 66 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |

Metric at `9732f3fe`: **6,006** to remove.

| 63 | P2-CAI-1 | `29a0cc08` | citizenai_actionctors builds actions through their constructors (eleven forwarders bypassed), candidatelist on typed records (rows 290-291): 182 -> 33 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 64 | P2-WORK-3 | `84d4450f` | the Work action builders construct through the Action constructors (same order at -O0, checked by the coordinator), `CharacterComponent::field_0x58` is `InternalPtr<Action>`, eleven dead forwarders deleted, `SIG_WORK.tsv`: 261 -> 160 in 20 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 65 | P2-CIT-2 | `01d45efa` | citizen_updatehealth, citizen_reassign_workers by name; the radius query on `CharacterGridWalker` (retail 0x140092990) (rows 330-333): 151 -> 9 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 66 | P2-CIT-1 | `161eaec4` | citizen_employ_cascade, lifeevent_handler, assign_citizens by name (rows 310-311): 255 -> 43 in seven files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 67 | P2-R-IO | `33d41e34` | ReaderWriter fields typed, `ModArrayOf<T>`, `ModSetNode` int16 balance / height, FileSystem entry points take the real String / Filename; byte-level views marked `byte_io`: 215 -> 104 in 31 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `33d41e34`: **5,291** to remove.
