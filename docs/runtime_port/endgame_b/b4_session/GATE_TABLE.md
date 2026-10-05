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

| 68 | P2-ACT-1 | `7e297bb2` | the pool elements are `GeometryBatchVertex` / `GeometryBatch` (retail 0x14001ee40, 0x14001fa80), `ActionStack::field_0x10` is `InternalPtr<Action>`, 36 dead forwarders deleted, `SIG_ACT.tsv`: 296 -> 129 | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 69 | P2-COMP-8 | `951339e8` | workcomponent, workplacecomponent, zonecomponent, tradecomponent by name; `WorkScheduler::field_0x20` typed (rows 360-361): 111 -> 40 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 70 | P2-COMP-1 | `c85bf00a` | boat route, build, animal, attendance, absorb, ambient components by name (rows 350-352; two never-firing guards dropped, recorded): 202 -> 70 in 25 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 71 | P2-R-UI | `7d9c0eb0` | `Element::_border`, `DialogInstancePtr`, `Sprite::spriteSheet`, `UI::String::table`, `SpriteSheet::m_field30` typed; `behaviour:` Progress / Slider / Tab descriptions hold `System::String` members (retail ctors 0x18011a210 / 0x1800d7b60 / 0x1800daa00) (rows 370-371): 295 -> 146 in 90 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 72 | P2-PLAYER | `66cf85e3` | the highlight tessellation on `GeometryBatch`, `HighlightComponent::fieldF8` is `EdgeMesh*`, `EditTool::entityRef` / `lastEntityRef` typed, `PlayerToolManager` +0x08 / +0x0C two floats (row 380): 282 -> 189 in 22 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok · ui ok | 0 |
| 73 | P2-COMP-4+5 | `360a066f` | growth, herd, floor, immigration, map, nomad components by name; five dead forwarders deleted (row 400): 135 -> 47 in 49 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |

Metric at `360a066f`: **4,584** to remove.

| 74 | P2-COMP-2+3 | `c40923a2` | disease, destroy, cemetery, crop field, clear area, char manager, state table by name; `StateMachine` fully asserted (retail ctor 0x14012ba30); `CemeteryDescription::_ref20` typed (rows 390-392): 295 -> 89 in 50 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 75 | P2-DLG-3 | `616fdf55` | tradeui, townhallui, the trade leaves / sort / checkout by name; `TradeStockEntry` (retail 0x14000e1c0), `UpgradeUIConfig::_target` typed (row 420): 267 -> 37 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 76 | P2-GAME-1 | `c079adec` | game_leaf_helpers, gamegrid / citizenlist serialize, citizen names, event log by name; element operations through `AssignAs` / `InitAs` / `DestroyAs`; `behaviour:` `Disease::_name`, `ClimateGenerator::_field28` are `System::String` members (retail 0x140019040 / 0x1400930c0, dtor 0x140018f60) (row 410): 256 -> 109 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 77 | P2-COMP-6+7 | `c8ad99b8` | residence, storage, orchard, pasture, terraform, tornado, state machine components by name; nine dead forwarders and doors deleted (row 430): 239 -> 114 in 48 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |

Metric at `c8ad99b8`: **3,876** to remove.

| 78 | P2-GAME-3 | `230aeaa7` | `behaviour:` MapGenerator's `ExternalRef::strPtr` is a `System::String`, its pools `DynArrayHeader` members with destructors (retail ctor 0x1400a3030, dtor 0x14001aea0); the menu loop's tick / draw helpers typed (row 470): 114 -> 80 in 25 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 79 | P2-DLG-2 | `2e525ea1` | professionui, residenceui, productionui, the toolbar dialogs by name; `ToolRegistryNode.h` (0x38, retail 0x14002df10), `TitleDescToolTipData` (rows 450-452): 252 -> 104 in 19 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 80 | P2-GAME-4 | `0b76abe0` | starttown by name (`StartTown_SpawnColonistRetail` returns the entity and the citizen; the founding-house / template / demo-entity globals typed); `behaviour:` TerrainGenerator's four Strings are members (retail ctor 0x1400b1810, dtor 0x140020630): 262 -> 69 in 20 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok | 0 |
| 81 | P2-DLG-1 | `46fbb17e` | `Map::regionPairCache` typed, `Map::field_0xA2` `uint8_t`, `TrackerComponent::EntryArray`; debugui, eventnotify, follow, citizen, build dialogs by name; `behaviour:` EventLogUI, CitizenUI, HelpUI, BuildUI hold `Collection::Array` members whose dtors free them as retail (rows 440-447): 233 -> 59 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `46fbb17e`: **3,312** to remove.

| 82 | P2-ACT-2 + TUT | `b77b4460` | fourteen dead Action forwarders deleted, the goal / tutorial bucket walks on `ProfessionBucket` / `TemplateBucket`; `behaviour:` TutorialSetupDialog's two String arrays are `Array` members (retail ctor 0x1400e2cc0, dtor 0x1400e2bf0): 115 -> 78 in 54 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 83 | P2-SA | `4b4f2d26` | the standalone harnesses and boot code by name; `EditTool_Harness*` declared once in `edittool_funcs.h`, local externs deleted; floor casts marked: 187 -> 97 in 25 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok · ui ok | 0 |
| 84 | P2-R-SYS | `3b606e4b` | Configuration's settings map root and platform / language / build dwords typed, `MemoryAllocator::freeLists` typed, four Serialize bodies build the real `IO::Serializer`; `behaviour:` `PointList::Point::name` is a `System::String`, `pos` a `Vector3` (retail ctor 0x18010f980, dtor 0x18010f8e0): 149 -> 107 in 45 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `3b606e4b`: **3,136** to remove (of which `dx11/` 240). **Waves S, L and R of the second pass are done: every block of `PASS2_FILEMAP.md` is merged (40 gated merges, rows 45-84, 0 problems each).**

`sa-stress` at `3b606e4b`, QUIET machine (no agent building): exit 0, update 1.73 ms/frame, p95 2.21, draw 1.25 (base 1.92 / 2.40 / 1.38). Recorded, not chased.

### Wave X (declaration families; inventory `data/pass2x/`, brief `PASS2X_BRIEF.md`)

| 85 | X-RL | `e09a8631` | runtime lines blocked by nothing outside their file: the `GraphicsMesh::DetailLevel`, `MaterialConstant`, `AnimationGroup::Tag`, `VertexProgram` element bodies by name through `AssignAs` / `InitAs` / `DestroyAs`; `EdgeMeshInstance::_mesh` an `ExternalPtr<EdgeMesh>`, `Video::Device` +0x50 typed: 137 -> 69 in 13 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 86 | X-GS | `044e8fce` | the virtual-slot parameters tree-wide: Controller slots and state handlers take `StateMachine*`, Component / Controller / Action / EntityList ticks `const System::Time*`, Action AbstractImpl / Execute and the executor `ActionStack*`, the ValidateCell / FindCell / CalcDistance trio `Map*` (rows 510-511): 587 -> 513 in 175 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 87 | X-R1 | `bc8b7989` | the ExternalPtr serialize thunks are typed wrappers (`SerializeExternal<T>`, `ExternalPtr<T>::Serialize` / `SerializeArray`) over the one core `Object::ExternalPtrSerializeBody`; 30 per-type thunks, the C shims and the caller bridges deleted; `ExternalList::Serialize` builds the real `IO::Serializer`; `RtObject::ExternalPtr` / `ExternalList` mirrors deleted: 207 -> 81 in 60 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `bc8b7989`: **2,868** to remove.

| 88 | X-RH | `3240a8ac` | `behaviour:` six runtime classes hold their Strings as members in retail's construction / destruction order: `IO::FileStream::_filename` (0x1800aaa10 / 0x1800aaab0), `UI::CharacterSet`, `UI::NumberElement::_cachedText`, `UI::Dialog::_spriteName`, `Video::ImageBuffer::_sourceName`, `Picking::PickingMesh`: 105 -> 96 in 22 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 89 | X-G1-2 | `e68c1f0b` | the work scheduler entry points typed; the AVL rotate / rebalance / insert forwarders and three placement-new forwarders deleted; twelve one-line forwarders replaced by the member; EventLog, ClearArea, fire, PerfX entry points typed (rows 560-561): 536 -> 404 in 90 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 90 | X-G1-3 | `6b09325e` | `StorageComponent_DynArrayResize` is the one SetSize core (retail 0x14000f090), the ambient / toolbar resizes typed by element; fifteen pure forwarders deleted; pathing, citizen-list, serialize, weather, tool and loop entry points typed; `ExternalPtrSerializeGeneric<T>` (rows 570-572): 524 -> 324 in 90 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 91 | X-R2 | `6f9dbc5d` | the External / ExternalFactory / resource-set entry points and `FileSystem::Exists` / `CreatePath` / `ExistsInPackfile` take `System::Filename`, eleven WideString holders are scoped `System::WideString` objects, `MaterialInstance::AddTexture` takes `System::String`; three callerless doors deleted (rows 580-582): 118 -> 72 in 42 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |

Metric at `6f9dbc5d`: **2,475** to remove.

| 92 | X-G1-1 | `1b134ba9` | the citizen find / target helpers, the nine CitizenAI action builders, the work action constructors, WorkLeaf helpers and WalkablePos, the StateCandidate list, dialog ctor entries, TradeUI price functions typed; about 25 forwarders and 7 callerless functions deleted; `FishingDescription::_fish`, `ForesterDescription::_ref20` typed (rows 550-551): 702 -> 398 in 90 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `1b134ba9`: **2,163** to remove.

| 93 | X-G1-3b | `75c80222` | `TradeWork_impl_4b40`'s callers on `Avl38Retrace`, the UIFade entry points take `UI::UserInterface*`, `EntityUI_SelectEntity_Impl` typed, `TerrainGenerator::_field20`, HighlightDescription's refs typed; `behaviour:` `HighlightComponent::transform` is a `Math::Transform` member (retail ctor 0x14004c930) (rows 573-575): 324 -> 296 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 94 | X-R3 | `8229a704` | `behaviour:` `IO::FileSystem`'s directories are `System::Filename` members (0x1800ac7c0 / 0x1800ac6d0), `Object::External::_path` released after `_refs` as retail (0x1800bc950), `IO::PackageFile`'s Strings and String arrays (0x1800b4440 / 0x1800b4540), `UI::Font`'s Strings (0x180031ce0 / 0x1801175a0) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok | 0 |
| 95 | X-G1-2b | `bda542a1` | the TownHall helpers typed (`TownHall_AvlRebalance28` deleted for the template), ProfessionNode / AgricultureNode / PathCostNode balance + height two int16 (rows 560-561 confirmed), ActMv / Pf_VectorResize14 / WorldGen / Region entry points, `GridTile.h`, entity_finders_disaster.cpp deleted: 404 -> 318 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 96 | X-G1-1b | `c57419d5` | `CitizenAIComponent::leisureTimer` a `System::Time`, dialog ctor entries take `const UI::Dialog*`, the tool-registry teardown on `ToolRegistryNode` (row 550 confirmed, rows 552-553): 398 -> 366 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `c57419d5`: **2,009** to remove (of which `dx11/` 240). Wave X round 1 done: 12 gated merges (rows 85-96), 0 problems each.

| 97 | X-G2-2 | `47b3b9be` | six pure forwarders and one callerless finder deleted, the factory dtors, CullStack helpers, `MeshInstance::PlayAnimation`, the WorkBST serializer parameter typed; stale-tag lines converted: 460 -> 421 in 126 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 98 | X-G2-3 | `adc83c33` | the seat cascade and `TownRegistry_RegisterResidence` typed, four storage forwarders and four callerless functions deleted, `StartTown_SpawnStartingHerd` takes `MapGenerator*`: 441 -> 368 in 126 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 99 | X-G2-4 | `57dde4a1` | `CitizenAI_Reg_helper_e360` one template over `LegacyKeyIntNode`, `Pf_VectorResize60`, `ResidenceComponent_impl_95a0`, `CitizenComp_Infect` typed, slot 18 `CreateWork` takes `InternalPtr<Work>*`; three pure forwarders deleted: 395 -> 339 in 126 files | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 100 | X-G2-1 | `f522f4eb` | `WorkAct_PerformWorkCtor` / `GatherWorkCtor` typed, four forwarders deleted, NameDB / region-pair-cache / AdjustDelimiters / WorkCellBST serializer entry points typed, the erased cores' element copies marked `elem_stride`: 484 -> 428 in 127 files (one conflict in `Game/exe_bodies.h`: two agents typed `GetLoadingSoundString` identically; main's side kept) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `f522f4eb`: **1,785** to remove (of which `dx11/` 240). Wave X round 2 done; every remaining cast outside `dx11/` carries a `// LEFT:` reason or a `// CAST-OK:` marker on its line.

| 101 | X3-WORK | `fd338ae7` | `behaviour:` eleven Work leaves hold their citizen / cell / entity sets as `Collection::Set` members in retail's order (FireWork 0x1400e9bd0 / 0x1400e9ac0: its dtor now frees the nodes it leaked; Forester, Herbalist, Hunt, Pasture, TendArea, HarvestArea, HarvestOrchard, PlantArea, PlantOrchard, ClearArea); WorkBST / WorkCellBST entry points typed wrappers over the one core (rows 640-641): 282 -> 223 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 102 | X3-GAME | `00b296c8` | `WorkComponent_CalcMaxWorkers` typed (21 callers), `Pf_AvlRebalanceA` / `TradeWork_impl_4b40` / `Pf_ZoneRefresh` and five more forwarders deleted, `WorldSetup_LoadFromSave` typed; `behaviour:` Toolbar's two Strings (0x1400d2130 / 0x140024c10) (rows 660-662): 1097 -> 1022 in code + standalone | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 103 | X3-COMP | `73a029e5` | the `System::Time` fields typed in CitizenAIComponent, CitizenComponent, CitizenBehavior (readers identical), 22 `fn_FindComponent_*` finders typed over one core, `CitizenBucketNode` balance + height two int16, nine forwarders deleted; `behaviour:` `CitizenComponent::name` a `System::String` released by the dtor as retail (0x140114770 / 0x1401146f0): 224 -> 187 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 104 | X3-RT | `2ba6e08f` | `Serialize` of Vector2/3/4, Transform, Audio::Scene, String takes `IO::Serializer&` tree-wide (90 call lines), `MainLoopCallback` one element type per array, `InternalFactory::_random` a `System::Random`; `behaviour:` the unreachable by-retail-address launcher lookup deleted; two conflicts with the typed String members resolved by hand: 1245 -> 1228 in runtime + engine + tests | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `2ba6e08f`: **1,588** to remove (of which `dx11/` 240). Wave X round 3 done.

`sa-stress` at `2ba6e08f`, QUIET machine: exit 0, update p95 2.29 ms, draw 1.25 (base 2.40 / 1.38). Recorded, not chased.

| 105 | X4-GAME | `cf521d31` | the standalone diagnostics read MeshInstance / GraphicsMesh / AnimationState by name, `g_StandaloneNewGameDialog` and the bridge harness globals typed, `EntityUI_OpenPauseMenu` deleted, the `EntityUI_*_SA` entries take `PlayerToolManager*`; vague tags restated: 541 -> 498 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 106 | X4-WORK | `ef292b04` | `behaviour:` GatherRawMaterials, BuildStructure, ClearStorage hold their arrays and citizen set as members in retail's teardown order (0x1400ec650 / 0x1400e5920 / 0x1400e73b0); five signatures typed, three forwarders deleted (rows 642-643; one conflict in workplaceui.cpp resolved to the member call): 204 -> 167 | exit 0 · 918 / 0 · 0 | ok | 5/5 | tools ok (ui with row 107) | 0 |
| 107 | X4-COMP | `8ac8fef0` | `behaviour:` EntityList is a real class: arrays `Collection::Array<ComponentEntity*>`, trees `Collection::Set`, ctor / dtor in retail's sequence (0x1400c3f90 / 0x1400c3e60), the memset construct and hand-written teardown deleted; three forwarders deleted, `ComponentEntity_CtorFromDescription` typed (rows 650-651): 173 -> 163 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `8ac8fef0`: **1,479** to remove (of which `dx11/` 240).

| 108 | X4-RT | `HEAD` | `Graphics::Camera(Frustum, Transform3, bool)` a defined member (retail 0x1800fe960); `behaviour:` `GetSunCamera` builds in the return slot as retail; `InternalBasePtr::_ptr` is `Object::Internal*`, `ResourceEntry::external` `Object::External*`; `behaviour:` `EditElement::_text` / `_filter` String members (0x1800d0f10 / 0x1800d0e50); two leftover `RtSystem` overloads deleted: 1228 -> 1204 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Wave X round 4 done (rows 105-108).
Metric after round 4: # castmetric total: 1451

| 109 | X5-GAME | `64532756` | `behaviour:` `PlayerController::camera` a `Graphics::Camera` member built last in both ctors as retail (0x1400985b0 / 0x140098800); WorldGeometry's nine arrays named members; `ForceWork::DriveWhenIdle`, `TradeWork_impl_ace40`, `LoaderDialog_FromSplashDLT` typed (rows 663-665): 485 -> 447 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 110 | X5-WORK | `0a4f7874` | `Work::field_10` / `field_18` `InternalPtr` members (47 readers); `CharacterComponent` +0x48 an embedded `ActionStack` of `InternalPtr<Action>` (retail 0x1400c54e0): Action Enter / Exit, executor callers, TaskListCleanup take `ActionStack*`: 167 -> 152 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 111 | X5-COMP | `a10fdd6f` | `behaviour:` CitizenBehavior is a real class (sixteen lists, nested / record arrays, both bucket trees as `Collection::Set<CitizenBucket<Key>>`) with retail's ctor 0x140090a30 and dtor 0x1400907b0; memset construct, trivial assert and hand-written teardown deleted; 719 reader lines in 104 files (rows 652-654) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 112 | X5-RT | `a424a694` | `SettingsMapNode` / `SettingsMap` declared once, `StringMapInsert` / `rtsetclear` / `rtpathid` typed, `GeneratedImageBuffer::Render` typed, `String::Serialize(void*)` deleted (reference table and compiled-resource load build the real `IO::Serializer`): 1204 -> 1171 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `a424a694`: **1,341** to remove (of which `dx11/` 240). Wave X round 5 done.

| 113 | X6-GAME-B | `4bbda0a5` | the highlight tessellation's stack blocks typed (36 -> 13, same stores), `Toolbar_ShowGroup_Impl` deleted, four entry points typed; `behaviour:` AchievementDialog's entries and BuildRequirementUI's arrays are Array members freed as retail (0x1400103f0, 0x140010860: row 441's leak ends) (rows 690-691): 196 -> 153 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 114 | X6-RT | `c113c856` | `MeshInstance::_mesh` an `ExternalPtr<GraphicsMesh>`; `SetMesh` / `SetMaterialInstance` / `PlayAnimation` / `SetParticle` take `ExternalPtr<T>` as retail's exports; `Context::_pixelConstantStaging` a named struct; the Persistence forwarders and the Camera door deleted: 376 -> 343 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 115 | X6-GAME-A | `33df7a1a` | GameUpdateLoop's ctor takes `const InternalPtr<AchievementInstance>&` (row 663 settled); `FindAvailableBuilding` and the town registry walk the typed bucket Set; `behaviour:` `m_scenarioInstance`, `PlayerToolManager::cameraViews` (0x1401025c0 / 0x1401024d0), WeatherSimulation's Vector4 and Arrays (0x140093f60 / 0x140093eb0: the arrays' zero stores now precede the PerlinNoise ctor call where retail's follow it); six dead helpers deleted: 242 -> 206 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 116 | X6-CW | `07d3aa4a` | `PickBestDropTarget` takes `CitizenBehavior*`; the `InternalPtr` members of Pickup, BuildRoad, LocationWork, the set records, OrchardSlot, Nomad / Picking / Character components typed (30 serialize bridges gone); seven forwarders deleted: 290 -> 220 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `07d3aa4a`: **1,142** to remove (of which `dx11/` 240). Wave X round 6 done.

`sa-stress` at `07d3aa4a`, QUIET machine: exit 0, update 1.70 ms/frame, p95 2.13 (base 1.92 / 2.40). Recorded, not chased.

| 117 | X7-GAME-B | `8e01ca83` | every remaining cast line in Dialog + Player + standalone read; `MainMenuDialog_FromMenuDLT`, `StartNewGame`'s saved-state argument, `PathTool::GetSelected` typed, `Dialog_FindComponent_d6c0` deleted, the tool-activation mirrors replaced by `ToolRegistryNode` (row 692): 147 -> 139 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 118 | X7-GAME-A | `17a89e05` | MapGenerator's string pools `Collection::Array`s over retail's rows; `behaviour:` TerrainGenerator's size table and material refs (0x140020630), three tutorial Strings, `Tutorial::steps`; the `Reimpl_ExternalPtr_Serialize_*` family nine typed cores under typed templates; tutorial update slots take `const System::Time*`: 199 -> 185 (the power went out during its tool routes; gate complete, ui ok; tools re-run after the restart: ok) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 119 | X7-RT | `bfbdd239` | `behaviour:` `IO::Persistence` holds its settings map, worker and `_savePath` as members in retail's order (0x1800b8020 / 0x1800b7fb0; the `RtIo::Persistence` mirror deleted), `IO::PackageFile`'s icon / preview arrays, `UI::TextElement`'s `_params` and `_cachedText`, `UI::SpriteCapture`'s three arrays, `UI::SpriteSheet`'s two Strings; two dead erased dtors deleted: 25 casts in 16 files | exit 0 · 918 / 0 · **1** | ok | 5/5 | ui ok · tools ok (run after the fix) | **1: a new perf warning — the file-local `MemFree` helper of spritesheet_new.cpp lost its last caller with the String members; deleted in `a2044ca6`, perf rebuild clean** |
| 120 | X7-CW | `23956761` | `ResidenceComponent_impl_7230` typed under `InsertCitizenBucket<Key>`, the 16-byte material-array family the exported bodies under typed wrappers, the road mesh on `GeometryBatchVertex` (row 662 settled from retail's serializer 0x14001f9c0), MarketWork's claim tree a POD; `behaviour:` TrackerComponent's lists, `AmbientEmitterComponent::slots`, `AmbientEnvironmentDescription::_entries` are Array members freed as retail — three leaks end (rows 700-704): 215 -> 176 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `23956761`: **1,052** to remove (of which `dx11/` 240). Wave X round 7 done: 76 gated merges this pass (rows 45-120), one problem (row 119, fixed).

`sa-stress` at `23956761` (end of round 7; Ghidra and four read-only inventory agents running, no builds): exit 0, update 1.77 ms/frame, p95 2.51, draw 1.29 (base 1.92 / 2.40 / 1.38). Recorded, not chased (p95 +4.5% on a machine running Ghidra).

### Wave Y (the re-inventory by class, `data/pass2y/`; round 1 interrupted)

| 121 | Y1-RT prerequisites | `91709c75` | `Audio::Scene` has a real `Scene()` (retail 0x1800e0160) and `Picking::Scene` a real `Scene()` delegating to `Scene(float)` (0x1801258e0 / 0x18007aef0); the two doors forward through placement new (the machine went down during this gate: re-run complete as `y1_RTpre2`) | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |

Metric at `91709c75`: **1,051** to remove (of which `dx11/` 240). **Session closed here by the user (2026-10-05).**
77 gated merges this session (rows 45-121); one problem (row 119). The per-merge evidence (`gate_*.out`, the
merge / premerge logs, `runs/<label>/`) was deleted at the close on the user's request; `runs/baseline` and the
warning baselines are kept (the gate compares against them). This table is the record.

### Wave Y, rounds 2 onward (the following session; baseline gate `s3_base` on `e4e5a78a`: 0 problems, invariants as above)

| 122 | Y2-GAMEA | `f0d89817` | `behaviour:` TutorialStep (0x1400e3480 / 0x1400e33f0; the `b4y1-GAMEA` takeover, verified), MenuDialogLoopT `_audioScene` + three arrays (0x14009bb00 / 0x14001a310; the Scene member is built before the body seeds `_random`, retail seeds first: disjoint bytes), MenuScene's tree records (0x14001b860), `Map::pickingScene` (0x14009ee70 / 0x14009ed60); Audio::Device teardown by name, NameDatabase, ClimateGenerator: 1051 -> 1023 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 123 | Y2-RT | `5b2932b8` | `Video::Context` `_renderBuffer` / `_imageBuffers` / `_dynamicIndexCursor` typed, `ReloadEntry` declared; `behaviour:` `ComboElement::_strings` (0x1801123f0 / 0x180112490), Text / Label / Border / Scroll description names, `Audio::Stream::_buffers` (0x1800a0e30 / 0x1800a0d90); `EdgeMesh::_reserved20`, `RendererDrawable::_object`: -> 990 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 124 | Y2-CW | `836d88de` | the `b4y1-CW` takeover (scratch slots as typed locals, verified); `behaviour:` `CitizenComponent::meshInstance` + `consumedTypes` (0x140114770 / 0x1401146f0: row 650 fixed by row 720; the null-vptr probes replaced by retail's unconditional `Update`), `ToolbarDescription::_entries` (0x140039490 / 0x14012db70, row 721); FruitDescription, DecalDescription, ComponentEntity's category byte, `CitizenComp_DeferredInit_117080` typed; row 705 recorded | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 125 | Y2-GAMEB | `c85074f1` | `behaviour:` HighlightComponent holds a real `EdgeMeshInstance` (0x14004c930 / 0x14004c8c0; row 573 fixed by row 740: the outline mesh is freed with the component); PlayerToolManager's two set headers (`PinnedToolNode`; row 741 recorded), `Avl38SetInsert` / `Remove` / `FindRemove` typed wrappers over one core, the tess vertex copy, `Dialog_AutoSaveHandler` deleted | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `c85074f1`: **911** to remove (of which `dx11/` 240). Wave Y round 2 done.

| 126 | Y3-GAMEA | `73032137` | `behaviour:` `NamedExternalRef::name` / `TutorialGoalCount::elementName` real Strings over eight owner classes (dtors 0x140026540, 0x140027820, 0x140027590, 0x140027d70, 0x140028090, 0x1400e2bf0, 0x140028bb0, 0x140025e80), BuildingGoal's ref array; `MapTileGrid_Build` / `Pathfinder_Seed` take `Map*`, the Stride20 resize deleted, tutorial SerOne / SerArray typed, the diagnostic probes real objects; row 730 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 127 | Y3-RT | `c91c129a` | `behaviour:` GraphicsMesh names + `NodeArray` (0x1800ff170 / 0x1800ff270), AnimationGroup clips / tags with Animation and Tag Strings (0x1800fb5a0 / 0x1800fb620), TextFileStream's names (0x1800ba340 / 0x1800ba490), `FileSystem::_packfiles` (0x1800ac6d0): -> 885 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 128 | Y3-GAMEB | `ba2a1604` | `behaviour:` TownHallUI's eleven containers (0x140081280 / 0x1400810d0; `OwnedTree`, `kSpec_ProfessionRow`); `PlayerToolManager::currentTool` an `InternalPtr<Tool>`, `PlayerController::target` a Vector3, `ToolRegistryNode::toolSetting`, `RawArraySetSize` / `RawArrayFree` | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 129 | Y4-RTUI | `deb19cb2` | `behaviour:` `UI::Dialog` (0x180112db0 / 0x180112e40), DocumentTextElement (0x1800cf410 / 0x1800cf320), DocumentTextDescription (0x1800cfc30 / 0x1800cfb30), `ElementDescription::_textName` (0x180115a40 / 0x180116100), Edit / Graph / Image / List description Strings; `StringTable::TextEntry` | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 130 | Y3-CW | `04d3fa3a` | `DriveWhenIdle` and its drivers typed, the ActHunt forwarders deleted, `EntityList_RegisterEntity`, `Work(ComponentEntity* const*)` through every leaf, PathGraph's distance cache and WorkScheduler's tree one `AvlTreeHead` each, `RoadTool_DecalStore` typed (identical -O2 code) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `04d3fa3a`: **838** to remove (of which `dx11/` 240). Wave Y round 3 done.

### Wave Y rounds 4-5 and the closing wave Z (the audit's rulings: `AUDIT.md`)

| 131 | Y4-RTGFX | `d247c1d0` | `behaviour:` real `~IndexBuffer` / `~VertexBuffer` (0x1800f3480 / 0x1800f55e0; WorldGeometry's buffers are destroyed after its tree frees until that class is typed: row 715), Renderer (0x180105180 / 0x1800180a0), AnimationManager (0x1800fc430 / 0x1800fc400), Scene dtor (0x1801026c0), `ShaderProgram::Parameter`, `VertexLayout::Element`, `PickingMesh::SubMesh` names; `SetVertexBuffer`, `EdgeMesh::Set`, the vertex ctors typed: 838 -> 814 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| — | castmetric | `557c1b2d` | the audit's tool rulings: the tier-I allocation shape grows, four categories added, the `elem_stride` `ArrayHdr` exception: 814 -> 779 (no code change) | — | — | — | — | — |
| 132 | Y5-RTUI | `df9ea594` | `behaviour:` `LayoutDescription::Entry` (0x1800d3a90 / 0x1800d3a10), `SpriteInfo` (AddSpriteInfo 0x180003390), `CaptureEntry::name`, StringTable's entry array (0x18011d890 / 0x18011d8f0), `SpriteContext(Video::Context&)` (0x18011d1e0), `Element::UpdateState` (0x180115600); raw String locals, `StringSerialize(char**)`, the AddDialog door deleted | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 133 | Z-CONST | `c90d4a3f` | `FindOrLoadAs` / `FindAs` / `FindNextAs` typed wrappers (`CAST-OK: const_api`, 3 lines), all 48 const_find sites through them; `_generation` mutable: -> 708 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 134 | Y4-STORAGE | `15c99436` | `behaviour:` Pasture animals (0x1401280e0 / 0x140128070), MultiObject children (0x140125a70 / 0x140125a00), `StorageComponent::storageSlots` (0x14012c8d0 / 0x14012c870: the item array is freed as retail, row 25 fixed by 722), Residence families (0x14012a2a0 / 0x14012a230), Minimap and CharacterGrid real ctors / dtors in place of the loader memsets (0x140097276 / 0x1400a4fc0; 0x1400182b0 / 0x140018220); `ArrayAt` deleted; row 723 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 135 | Y4-MAP | `959eea1e` | `behaviour:` `Map::cells` / `cellQueue` with retail's ctor stores (0x14009ee70 / 0x14009ed60), the fenced tool's 12-byte normal local (0x14009f3e0; rows 730-733); Map +0xE8 one `AvlTreeHead`, MapCell's 9-byte block, `TrackedEntityNode::key`, the Avl38 cores with a key accessor (TradeWork's tree typed, the PlayerTrade forwarders deleted), `PathSegmentNode`, `Reimpl_MapGetTerrainPos` typed | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `959eea1e`: **676** to remove (of which `dx11/` 240; 436 in scope).

| 136 | Z-RT | `ea50ba91` | `BaseTypeInfo` factory functions typed, one `MessageCallbackEntry` and `rtsetavl::ElementOf`, four bool fields in place of byte puns, `Random::_index` / `ElementDescription::_id` uint32 as retail reads them, `SaveInfo::sortKey` a DateTime, `Array::DestroyAndFree` | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 137 | Z-CODEA | `f5e8b3ed` | `behaviour:` WorldGeometry +0xD8..+0x220 real members in retail's release order (0x1400b00e0 / 0x1400aed50; row 715 fixed by 734), NameDatabase's char array (0x1400a45e0); generator bool flags, the tool registry one `AvlTreeHead`, the twin path-vector row unified (row 735) | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 138 | Z-CODEB | `dfea96f4` | `behaviour:` TradeComponent's arrays and stock tree (0x14005c430 / 0x14005c340), StateMachine Persist through `Serialize(signed char&)` (0x14012b940), the shared Avl38 find-remove core walks as retail 0x14002c690 (row 731 fixed by 742), `TrackManagerPinEntity` as 0x140102e30 (row 743); bool chains, `WorkSchedulerTree` wrappers: -> 621 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 139 | Z-RT2 | `476b6d19` | `AnimationStateArray` and the Mod arrays typed headers with typed doors, the `RtIO::Modification` mirror deleted; `behaviour:` `AnimatedImageDescription::_frameNames` (0x1800cb6f0 / 0x1800cb6b0); every remaining runtime line tagged with its audit disposition: -> 597 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 140 | Z-MIRROR | `6c51e8ce` | `behaviour:` `ResourceEntry` / `ReferenceEntry` hold a real Filename (0x18002ca00 / 0x18002cb90), `PathStackEntry` (0x1800bd177 / 0x18002ccb0), `External::_refs` and `~External` as retail 0x1800bc950; `ModSetNode::key`; two mirrors deleted; the Filename IS-a String ruling recorded | GATE INTERRUPTED: an agent killed every compiler on the machine during the perf build (no compile error); covered by row 141's gate of the tree that contains it | — | — | — | (see 141) |
| 141 | Z-SWEEP | `ad55bf56` | `behaviour:` EventLog's entry array with retail's inline construction (0x14009700c; row 723 fixed by 736), MapCell blocker / occupant InternalPtrs with the cell array built by `SetSize` as retail 0x14009ef20 (row 737); `DECLARE_EXE_TYPE` factories return `Object::Base*`, `Component::_entity`, `CitizenAIComponent::citizen`, CitizenComponent's twelve members and the animal-list members typed InternalPtr: -> 573 | exit 0 · 918 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `ad55bf56`: **573** to remove (of which `dx11/` 240; 333 in scope).

| 142 | Z-MARKERS | `706400dc` | the audit's floor rulings as `CAST-OK` markers (81 casts), eleven wrong / mis-filed markers removed, FLOOR.md re-cut with measured ceilings; comment-only (every function IDENTICAL); then `736fd75c`: the 16 macro-line casts by block-comment marker: 573 -> 486 | exit 0 · 918 / 0 · 0 | ok | 5/5 | — | 0 |
| 143 | Z-MIRROR2-STR | `0d3f9fa6` | `behaviour:` `Filename : public String` as retail (0x180006220 / 0x18000c780 / 0x180006980), the WideString slots real locals released last (rows 580-582 settled by 706; the POD deleted), `ReloadEntry::file` with retail's GetPath-before-Type order (0x1800af130, row 707), `FilenamePair::key`; unit 918 -> 917 (the deleted POD's test) | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 144 | Z-MIRROR2-GFX | `dc2fae28` | `behaviour:` retail's 0x18-byte `Graphics::AnimationState` (0x1800fdaf0 / 0x1800fe380), `MeshInstance::_animation` that member (0x1800fffc0 / 0x180017060; the copy ctor gains one dead byte store); the layer / interpolated-transform arrays on retail's rows; seven `RtGraphics` mirrors deleted: -> 458 | exit 0 · 917 / 0 · 1 | ok | 5/5 | ui ok · tools ok | 1: the standing scene_grid memset warning's wording follows the SceneNode rename (baseline line updated, `e5c33ea2`) |
| 145 | Z-LAST-CODE | `242eb864` | AddDecal's five float parameters as retail's MOVSS stores (the `float_bits` puns gone); `behaviour:` `SpawnDescription::_entries` (0x14000c1d0 / 0x14000c2e0), `EnvironmentDescription::_weatherArray` (0x1400941c0 / 0x140019190); the panel harnesses hand a real InternalPtr (row 155): -> 445 | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 146 | Z-LAST-IO | `4ea28051` | `behaviour:` `IO::FileSystem::FileInfo` a real class (0x18000c110 / 0x18000c200 / 0x18000c180; row 708: GetFileInfo's missing reference), PackageFile's entry array and `PackageFileEntry::name` (0x1800b4440 / 0x1800b4540, 0x1800b46b0); the `RtIO::FileSystem` POD, `PackageFileView`, the String `Sys()` and both `SysFile()` bridges, 19 dead forwarders deleted; row 709: -> 434 | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 147 | ACC asserts + bridge comments | `60dc218a` | 548 compiler-derived `offsetof` asserts in 185 `code/Game` headers (acceptance line 10: 1231 -> 682); 175 stale bridge comments removed in `code/` + `standalone/`, comment-only, objects IDENTICAL (line 07: 331 -> 156). The assert merge `7a6c17be` was committed without its own pre-merge build; this gate covers both | exit 0 · 917 / 0 · 0 | ok | 5/5 | — | 0 |

Metric at `60dc218a`: **434** to remove (of which `dx11/` 240; 194 in scope).

### The raw array triples, the last mirrors and the acceptance passes

| 148 | H5-A | `90a4f4ed` | `behaviour:` AutoPickup (0x140044ae0 / 0x140044a80; freed as retail, row 760), Cemetery plots, CreatePlacedDescription, CitizenDescription refs, Boat route, Attendance queue, ConsumeProduceComponent rates, BuildComponent's two arrays as real Array members | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 149 | H5-C | `ebd84ccc` | `behaviour:` the crop / orchard work leaves' per-slot bool arrays, FishingWork's two arrays (0x1400eabe0 / 0x1400eaa50; freed as retail, row 780), ConsumeProduce's two arrays, `WorldGeometry::_batchCounters` (one intermediate commit does not build the unit tests; the tip does) | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 150 | H5-B | `e5a850af` | `behaviour:` Terraform records, Interact locations, Herd members, WorkPlace workers, Immigration group, UIComponent data, Tornado path points, Population cells / herds; six arrays the tree never freed are freed as retail (rows 770-775) | exit 0 · 917 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 151 | ACC-MIRROR11 | `98b5dbd6` | acceptance line 11: 48 -> 2 (44 unused Rt mirrors deleted, `RtObject::Factory` and `RtSystem::Thread` replaced: `ThreadFunc` 0x1800ca440 by virtual calls; one `PackfileIndexNode` over a two-dword value); `behaviour:` GetSaveDirectory's WideString released last (0x1800acfd0); rows 750-751; unit 917 -> 914 (three tests of the deleted minted Thread table): -> 420 | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 152 | H5-P1 | `d3f30d7b` | `behaviour:` `~ActionStack` ported (0x1400c5150 from `~CharacterComponent` 0x14002faa0: the stack's Actions deleted, the buffer freed; row 790; the PORT GAP gone), `~ParticleComponent` (0x140053310: emitter instances deleted, both arrays freed; row 791); the dying-entity audit re-fires `ParticleComponent::OnRemove` (tree-only, row 792) | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 153 | ACC asserts RT | `c87a2d2b` | 596 compiler-derived `offsetof` asserts in 101 runtime / dx11 / engine headers (line 10: 650 -> 54) | exit 0 · 914 / 0 · 0 | ok | 5/5 | — | 0 |
| 154 | H5-P2 (+ ACC bridge RT `7b15ce62`) | `4e3fef95` | `behaviour:` `GeometryBatch` a real class (0x14001f250 / 0x14001f5e0 / 0x14001fb80) and its owning arrays on `kSpec_GeometryBatch` through the erased core (row 795; eight hand-written helpers deleted); EntityList's update buckets hold InternalPtr elements; row 796 recorded open; 26 stale runtime bridge comments removed | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 155 | ACC-MOD | `018f53d5` | acceptance line 11: 2 -> 0. `behaviour:` `IO::Modification::Mod` a real class (0x1800101e0 / 0x180026ef0 / 0x180011460) on `kSpec_Mod` (row 752), `MaterialConstant::name` a real String (0x1800616c0; row 753); `RtSystem::String`, `RtSystem::Filename`, `pod_bridge.h` deleted. Mods route with two `.pkm` mounted (list, select, details, apply): exit 0, no crash | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok · mods ok | 0 |
| 156 | FIN-F2 | `239c7591` | `behaviour:` `CreateRemovalDescription::_refs` (0x140032b70 / 0x140032ca0), `SpawnComponent::timers` (0x140056030 / 0x14000c350; freed as retail, row 805), the citizen bucket-tree load through a real local Array (0x140017a40 / 0x140017cd0, row 806); `TradeMerchantOrder::goods` typed | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 157 | FIN-F1 | `8a67fc68` | `behaviour:` `ComponentEntity::_components` a real Array of InternalPtr (0x1400c3620 / 0x1400c3720 / 0x1400c3550), the WorkScheduler serializer passes retail's root-and-count headers (0x1400f97c5 / 0x1400f989a; row 796 fixed by 800); EntityList's master array InternalPtr elements | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |
| 158 | FIN-RT | `12b982db` | `SceneGrid` (0x18005e4a0) a member and `Scene()` a real ctor (0x180102730), the persist-state Serializer and Clone's MemoryStreams real locals, offset asserts for the last four headers, stale mirror comments fixed | exit 0 · 914 / 0 · 0 | ok | 5/5 | ui ok · tools ok | 0 |

Metric at `4f8da1d4` (main): **389** to remove (of which `dx11/` 240; **149 in scope**). `4f8da1d4` itself adds one floor marker with a
parameter rename (`runtime/src/graphics/closure_batch2b.cpp`: `self` -> `dst`) that was committed WITHOUT a build: build it first.

Close checks run at `12b982db`: `worldcmp.sh` (seed 12345, against the pre-B.4 exe `b2a5434c`): all 118 pointer-free world lines
identical once pointer print widths are normalised; `devlost.sh` alone: exit 0, INJECT=1, round trip COMPLETE, no crash / hang.

**NOT MERGED: `b4fin-VOID` (`eed39411`).** Its squash-merge was staged and its pre-merge build was running when the harness stopped the
command for low system memory; the main checkout was reset to `4f8da1d4` and nothing of it is on main. The branch builds and passes
unit at its tip by its own hand-in (62 `void*`-first functions: 27 typed, 15 deleted, 10 tagged probes, 10 left; `coll_bridge.h`
deleted; metric 389 -> 386). Its first commit `6f1c444e` swept in a stale index and `026a2101` reverses it: merge it SQUASHED
(`git merge --squash b4fin-VOID`), build, gate with `ui tools`.
