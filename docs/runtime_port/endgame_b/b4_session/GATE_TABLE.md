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
