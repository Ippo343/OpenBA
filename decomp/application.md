# Application-x64.exe (Game Logic)

## Engine Core

| Status | Class | Namespace | Known Methods |
|--------|-------|-----------|---------------|
| R | `MainLoop` | Engine | **26/26 methods reimplemented** (`engine/reimpl/mainloop.cpp`): Run, Frame, Update, Draw, SetNextLoop, Get, IsRunning, GetContext, GetCurrentLoop, GetDeltaTime, GetFrameTime, GetFrameStartTime, GetFrameUpdateTime, MarkFrameUpdateTime, AddPostUpdate, AddPostDraw, RemovePostUpdate, RemovePostDraw, OnVideoError, OnVideoRestart, OnVideoError\_s, OnVideoRestart\_s, CreateVideoContext, ReleaseVideoContext, ctor, dtor. 9 getters too small to hook (4-8 bytes), documented in source. |
| R | `UpdateLoop` | Engine | **All methods reimplemented** (`engine/reimpl/updateloop.cpp`): IsRunning, ctor, dtor (hooked). SetLoadProgress, GetLoadProgress, GetLoadBit (4 bytes each, documented). LoadResources resolved as callable fn ptr. Base virtuals (Update, Draw, Startup, Shutdown, etc.) overridden by subclasses. |
| R | `LoaderLoop` | Engine | **All methods reimplemented** (`engine/reimpl/loaderloop.cpp`): Startup, Shutdown, Update, Draw, Exit, IsFinished, OnVideoError (7 hooked). Load is 3-byte no-op stub (documented). |
| R | `DialogLoaderLoopT` | Engine | **5/5 methods reimplemented & hooked** (`engine/reimpl/dialogloaderloop.cpp`): Startup, Shutdown, Update, Draw, Load |
| R | `DialogLoopT` | Engine | **5/5 methods reimplemented & hooked** (`engine/reimpl/dialogloop.cpp`): Startup, Shutdown, Update, Draw, Load |
| R | `Application` | System | YieldToThreads (trivial thunk → `SwitchToThread()`, 7 bytes at `0x18009f920`, no hook needed) |
| R | `Configuration` | System | **10 methods hooked** (`engine/reimpl/configuration.cpp`): CurrentBuildId, CurrentVersion, GetVersionString, GetPlatformName, GetLanguageName, GetBuildName, GetCommandLineString, SetCommandLine (8 full reimpl), Ctor, Dtor (2 passthrough wrappers — lifecycle only). AddCommandLine (4b thunk), GetResourceParameter (10b), GetResourceParameterCount (3b) — too small to hook, documented. |
| R | `Modification` | IO | **8 methods hooked** (`engine/reimpl/modification.cpp`): Find, Enable, DisableAll, Swap, SaveList (5 full reimpl), EnableSavingSettings, RefreshList, SetupFileSystem (3 passthrough wrappers — RefreshList/SetupFileSystem need FileSystem+PackageFile subsystem). Get/GetCount/IsDirty/GetModification (too small, 3-12 bytes, documented). AreExternalModsUpToDate/UpdateExternalMods (stubs, 3 bytes). |

## ECS - Game Components (all ComponentEntity children)

| Status | Component | Category | Purpose |
|--------|-----------|----------|---------|
| ~ | `CitizenAIComponent` | AI | 0xD0 bytes, 16 overrides. **Full AI state machine mapped**: 16 states with priority-weighted selection, 16 cooldown timers at +0x40-0xBF. BuildCandidateStateList evaluates conditions, shuffles, sorts by priority. All 14 state handlers decompiled and named. Update=150 lines. Startup resolves CitizenComponent ref at +0x38. ~262 functions renamed total. Ctor 0x14010fb70, vtable 0x140162470. |
| ~ | `CitizenComponent` | AI | 0x290 bytes (largest), 18 overrides. **Update fully decompiled (393 lines)**: aging system (3 age tracks: real/adult/elder), education-dependent aging rates, food/fuel consumption timers, death conditions (3 types), workplace/housing assignment, family management. Config table at +0x288 with ~25 parameters. Key fields: age(+0x3C), adultAge(+0x44), elderAge(+0x48), residence(+0x68), workplace(+0x228/+0x238), family(+0x278), education(+0x1E4). Startup resolves family(+0x278), mesh(+0x280), config(+0x288). Ctor 0x140114770, vtable 0x140162580. |
| ~ | `StateMachineComponent` | AI | 0x58 bytes, 16 overrides. **Update = 1-line delegator** to StateMachine_Tick(+0x48, dt, true, true). Back-ptrs at +0x40/+0x48. Ctor 0x14012ba30, vtable 0x140164520. |
| ~ | `CharacterComponent` | AI | 0x80 bytes, 16 overrides. **Update decompiled**: delegates to Character_UpdateAnimation via array at +0x48 indexed by +0x60. Ctor 0x140109d20, vtable 0x140162370. |
| ~ | `AnimalComponent` | AI | 0x58 bytes, 14 overrides. **Update fully decompiled**: aging (+0x44 += dt*rate), death when exceeding threshold, random movement within bounds (short[4] at +0x38-0x3E), grid validation (terrain type 0x02), pathfinding creation (0x480 bytes). 100-attempt destination search. Ctor 0x140106fc0, vtable 0x140162110. |
| ~ | `WildAnimalComponent` | AI | 0x30 bytes (minimal), 10 overrides. No Update override. Ctor 0x14012e3c0, vtable 0x140164af0. |
| ~ | `HerdComponent` | AI | 0x60 bytes, 13 overrides. **Update decompiled**: timed food check → AdjustSize, two-phase grazing/migration system (flag at +0x48), grid-based terrain validation for migration destinations. Ctor 0x140122150, vtable 0x140163680. |
| ~ | `LivestockComponent` | AI | 0x38 bytes, 13 overrides. **Update decompiled**: simple reproduction timer at +0x30, increments herd birth counter (+0x28→+0x44) on interval. Only active if herd ref and reproduction flag set. Ctor 0x140123ee0, vtable 0x1401639b0. |
| ~ | `BoatComponent` | AI | 0x98 bytes, parent=ControllerComponent. No own Update (uses base). Secondary vtable at +0x78. Own GetDescription. Ctor 0x140046800, vtable 0x140158f40. |
| ~ | `ControllerComponent` | Base | **ABSTRACT**, parent=Component. Intermediate base for 20+ components. Factory returns NULL (0x140003730). TypeInfo DAT_1401c73c0. RegFunc 0x14014eb50. |
| ~ | `ResourceProduceComponent` | Base | **ABSTRACT**, parent=ControllerComponent. Base for production chains. Factory returns NULL (0x140003730). Ctor 0x14012b3d0. TypeInfo DAT_1401c7d60. RegFunc 0x14014f8d0. |
| ~ | `WorkComponent` | Economy | 0x38 bytes, parent=Component. Job execution logic. Ctor 0x14012e820, factory 0x14003a570. TypeInfo DAT_1401c8070. |
| ~ | `WorkPlaceComponent` | Economy | 0x68 bytes, parent=ControllerComponent. Building work assignment. Ctor 0x14012f2e0, factory 0x14003a030. TypeInfo DAT_1401c80e0. |
| ~ | `StorageComponent` | Economy | 0x40 bytes, parent=Component. Resource storage management. Ctor 0x14012c8d0. TypeInfo DAT_1401c7e78 (used by CitizenAI for storage queries). |
| ~ | `StorageLocationComponent` | Economy | 0x48 bytes, parent=ControllerComponent. Physical storage positions. Ctor 0x14012d360, factory 0x140038df0. TypeInfo DAT_1401c7ee8. |
| ~ | `StorageDisplayComponent` | Economy | 0x38 bytes, parent=Component. Visual storage display. Ctor 0x140056a00. |
| ~ | `MarketComponent` | Economy | 0x50 bytes, parent=ResourceProduceComponent. Market resource distribution. Uses ResourceProduceComponent_Ctor + vtable override. |
| ~ | `MarketLocationComponent` | Economy | 0x48 bytes, parent=ControllerComponent. Market building positions. Ctor 0x14004f060, factory 0x1400086c0. TypeInfo DAT_1401c4560. |
| ~ | `ConsumeProduceComponent` | Economy | 0x70 bytes, parent=ResourceProduceComponent. Production chain logic. Ctor 0x140119050. |
| ~ | `RawMaterialComponent` | Economy | 0x40 bytes, parent=Component. Raw material definitions. Ctor 0x140129400, factory 0x140037270. TypeInfo DAT_1401c7c80. |
| ~ | `AbsorbResourceComponent` | Economy | 0x48 bytes, parent=ControllerComponent. Resource absorption. Ctor 0x140105cc0, factory 0x14002ea20. TypeInfo DAT_1401c6f28. |
| ~ | `TradeComponent` | Economy | **0xa8 bytes** (largest ControllerComponent subclass), parent=ControllerComponent. Trading post logic. Ctor 0x14005c430, factory 0x14000db60. TypeInfo DAT_1401c4b80. |
| ~ | `ResidenceComponent` | Population | 0x60 bytes, parent=ControllerComponent. Housing assignment/capacity. Ctor 0x14012a2a0, factory 0x140037440. TypeInfo DAT_1401c7cf0. |
| ~ | `PopulationComponent` | Population | 0x70 bytes, parent=Component. **Update decompiled**: 2D density smoothing (5x5 weighted kernel) + per-cell habitability scanning. Grid at +0x38, dim at +0x30. Ctor 0x1400546d0, vtable 0x14015a360. |
| ~ | `HappinessComponent` | Population | 0x50 bytes, parent=ControllerComponent. Citizen happiness calculation. Ctor 0x14004ab60, factory 0x140005ba0. TypeInfo DAT_1401c42c0. |
| ~ | `DiseaseComponent` | Population | 0x58 bytes, parent=Component. **Update decompiled (~100 lines)**: two-state disease sim — inactive: seasonal infection probability roll; active: countdown timer, cure checks (healer building), periodic health damage + neighbor spread. Ctor 0x140048db0, vtable 0x140159290. |
| ~ | `ImmigrationComponent` | Population | 0x68 bytes, parent=ControllerComponent. Nomad/immigration events. Ctor 0x14004e640, factory 0x140007a20. TypeInfo DAT_1401c4480. |
| ~ | `NomadComponent` | Population | 0x78 bytes, parent=ControllerComponent. Nomad arrival handling. Ctor 0x14004fdb0, factory 0x140009020. TypeInfo DAT_1401c4640. |
| ~ | `AttendanceComponent` | Population | 0x58 bytes, parent=ControllerComponent. **Update decompiled**: processes attendance queue when building idle — iterates queued citizens, checks building assignments (+0x208/+0x228/+0x230), marks attended. Ctor 0x140044580, vtable 0x140158ce0. |
| ~ | `HealerComponent` | Population | 0x48 bytes, parent=ControllerComponent. Healer building logic. Ctor 0x14004af70, factory 0x140006490. TypeInfo DAT_1401c4330. |
| ~ | `HerbalistComponent` | Population | 0x50 bytes, parent=ResourceProduceComponent. Herbalist building logic. Inline ctor (RPCtor+vtable). Factory 0x140006b40. TypeInfo DAT_1401c43a0. |
| ~ | `CropFieldComponent` | Production | 0x68 bytes, parent=ControllerComponent. Crop field farming. Ctor 0x14011afd0, factory 0x140032ed0. TypeInfo DAT_1401c7510. |
| ~ | `ForesterComponent` | Production | 0x58 bytes, parent=ResourceProduceComponent. Forester lodge tree management. Inline ctor. Factory 0x140034870. TypeInfo DAT_1401c7740. |
| ~ | `FishingComponent` | Production | 0x58 bytes, parent=ResourceProduceComponent. Fishing dock logic. Inline ctor. Factory 0x140034350. TypeInfo DAT_1401c7660. |
| ~ | `OrchardComponent` | Production | 0x68 bytes, parent=ControllerComponent. Orchard tree management. Ctor 0x140051f40, factory 0x140009870. TypeInfo DAT_1401c46b0. |
| ~ | `PastureComponent` | Production | 0x68 bytes, parent=ControllerComponent. Pasture animal grazing. Ctor 0x1401280e0, factory 0x140036730. TypeInfo DAT_1401c7ba0. |
| ~ | `MineComponent` | Production | 0x28 bytes (smallest), parent=Component. Mine resource extraction. Ctor 0x14004f2e0, factory 0x140008dd0. TypeInfo DAT_1401c45d0. |
| ~ | `QuarryComponent` | Production | 0x28 bytes (smallest), parent=Component. Quarry stone extraction. Ctor 0x1400552b0, factory 0x14000b3d0. TypeInfo DAT_1401c4800. |
| ~ | `WellComponent` | Production | 0x48 bytes, parent=ControllerComponent. Well water supply. Ctor 0x14005d060, factory 0x14000fc80. TypeInfo DAT_1401c4c28. |
| ~ | `HuntComponent` | Production | 0x58 bytes, parent=ResourceProduceComponent. **Update decompiled**: time accumulator scaled by terrain speed ratio — hunt progress timer. Ctor 0x1401228e0, factory 0x140034f30, vtable 0x140163770. |
| ~ | `FruitComponent` | Production | 0x38 bytes, parent=Component. **Update decompiled**: fruit ripeness simulation — quadratic temperature sensitivity, seasonal growth factor, clamped [0,1]. Ctor 0x14004a160, vtable 0x140159450. |
| ~ | `GrowthComponent` | Simulation | 0x50 bytes, parent=Component. **Update decompiled (complex)**: tree/crop growth with seasonal temp modulation, harvest threshold, sub-resource spawning. Most complex production Update. Ctor 0x1401208f0, vtable 0x140163590. |
| ~ | `NaturalResourceComponent` | Simulation | 0x30 bytes, parent=Component. Natural resource spawning. Inline ctor. Factory 0x140036550. TypeInfo DAT_1401c7b30. |
| ~ | `FireComponent` | Simulation | 0x48 bytes, parent=Component. **Update decompiled**: fire spread timer + random ignition. Two states: active (countdown + spread to neighbors + extinguish) vs inactive (random fire-start probability). Ctor 0x140049b80, vtable 0x140159380. |
| ~ | `TornadoComponent` | Simulation | 0x68 bytes, parent=Component. Tornado disaster. Ctor 0x140058cf0, factory 0x14000d500. TypeInfo DAT_1401c4ad8. |
| ~ | `AmbientEmitterComponent` | Simulation | 0x48 bytes, parent=Component. Ambient sound emitter. Ctor 0x1400430c0, factory 0x1400014b0. TypeInfo DAT_1401c3e60. |
| ~ | `AmbientEnvironmentComponent` | Simulation | 0x48 bytes, parent=Component. **Update decompiled**: terrain grid sampling for ambient sound selection — identifies dominant biome around entity, blends day/night sound sets based on time-of-day. Ctor 0x1400438d0, vtable 0x140158b70. |
| ~ | `CemeteryComponent` | Building | 0x58 bytes, parent=ControllerComponent. Cemetery management. Ctor 0x1400476f0, factory 0x140004040. TypeInfo DAT_1401c4090. |
| ~ | `TombstoneComponent` | Building | 0x30 bytes, parent=Component. Individual grave tracking. Ctor 0x1400579a0, factory 0x14000d280. TypeInfo DAT_1401c4a30. |
| ~ | `SpawnComponent` | Building | 0x38 bytes, parent=Component. **Update decompiled**: periodic entity spawning — iterates spawn entries, accumulates time, spawns when interval exceeded. Ctor 0x140056030, vtable 0x14015a670. |
| ~ | `TerraformComponent` | Building | 0x48 bytes, parent=Component. Terrain modification. Ctor 0x140057620, factory 0x14000cc90. TypeInfo DAT_1401c49c0. |
| ~ | `FloorComponent` | Building | 0x30 bytes, parent=Component. Floor/foundation placement. Ctor 0x14011f600, factory 0x140034600. TypeInfo DAT_1401c76d0. |
| ~ | `ZoneComponent` | Building | 0x28 bytes (smallest), parent=Component. Zone area definitions. Ctor 0x1401303c0, factory 0x14003a6a0. TypeInfo DAT_1401c8150. |
| ~ | `BuildComponent` | Building | 0x78 bytes, parent=ControllerComponent. Construction progress. Ctor 0x140108fc0, factory 0x14002f2b0. TypeInfo DAT_1401c7008. |
| ~ | `DestroyComponent` | Building | 0x68 bytes, parent=ControllerComponent. Demolition handling. Ctor 0x14011dc80, factory 0x140033bc0. TypeInfo DAT_1401c75f0. |
| ~ | `ClearAreaComponent` | Building | 0x50 bytes, parent=ControllerComponent. Area clearing for construction. Ctor 0x140117fa0, factory 0x140030ea0. TypeInfo DAT_1401c71c8. |
| ~ | `ClearResourceComponent` | Building | 0x48 bytes, parent=ControllerComponent. Resource removal from area. Ctor 0x1401186a0, factory 0x1400315a0. TypeInfo DAT_1401c7238. |
| ~ | `MultiObjectComponent` | Building | 0x60 bytes, parent=ControllerComponent. Multi-tile building logic. Ctor 0x140125a70, factory 0x140035ef0. TypeInfo DAT_1401c7ac0. |
| ~ | `MapComponent` | World | 0x38 bytes, parent=Component. World map data. Ctor 0x1401244d0, factory 0x140035700. TypeInfo DAT_1401c79e0. |
| ~ | `TrackerComponent` | World | 0x50 bytes, parent=Component. Entity tracking/indexing. Inline ctor. Factory 0x14000d7a0. TypeInfo DAT_1401c4b10. |
| ~ | `InteractComponent` | Gameplay | 0x38 bytes, parent=Component. Player interaction handling. Ctor 0x140123480, factory 0x1400351a0. TypeInfo DAT_1401c7900. |
| ~ | `AutoPickupComponent` | Gameplay | 0x40 bytes, parent=Component. Auto-resource collection. Ctor 0x140044ae0, factory 0x1400033c0. TypeInfo DAT_1401c3fb0. |
| ~ | `PickingComponent` | Gameplay | 0x38 bytes, parent=Component. Mouse picking/selection. Ctor 0x140128c80, factory 0x140036da0. TypeInfo DAT_1401c7c10. |
| ~ | `ModelComponent` | Rendering | **0x170 bytes** (largest Component overall), parent=Component. 3D model reference. Ctor 0x140124d20, factory 0x1400358d0. TypeInfo DAT_1401c7a50. |
| ~ | `DecalComponent` | Rendering | 0x38 bytes, parent=Component. Terrain decal. Ctor 0x14011b8f0, factory 0x1400337c0. TypeInfo DAT_1401c7580. |
| ~ | `ParticleComponent` | Rendering | 0x48 bytes, parent=Component. Particle effect. Ctor 0x1400533d0, factory 0x14000a360. TypeInfo DAT_1401c4720. |
| ~ | `HighlightComponent` | Rendering | **0x128 bytes** (2nd largest), parent=Component. Selection highlight. Ctor 0x14004c930, factory 0x140006e40. TypeInfo DAT_1401c4410. |
| ~ | `StatusIconComponent` | Rendering | **0x108 bytes** (3rd largest), parent=Component. Overhead status icons. Ctor 0x14012c0c0, factory 0x1400388c0. TypeInfo DAT_1401c7e08. |
| ~ | `UIComponent` | UI | 0x38 bytes, parent=Component. In-world UI element. Ctor 0x14012de00, factory 0x140039550. TypeInfo DAT_1401c7fc8. |

## ECS - Tool/Creation Descriptions

| Status | Description | Parent | Size | Details |
|--------|-------------|--------|------|---------|
| ~ | `CreateBridgeDescription` | Description | 0x60 (96B) | Ctor 0x1401196f0, factory 0x140032880. TypeInfo DAT_1401c73f8. Bridge placement tool. |
| ~ | `CreateDragDescription` | Description | 0x40 (64B) | Ctor 0x140119860, factory 0x140032970. TypeInfo DAT_1401c7430. Drag-to-build tool. |
| ~ | `CreateFencedDescription` | Description | 0x78 (120B) | Ctor 0x140047a10, factory 0x1400048e0. TypeInfo DAT_1401c4100. Fenced area tool. |
| ~ | `CreatePathDescription` | Description | 0x20 (32B) | Ctor inlined, vtable 0x140159210, factory 0x140004c30. TypeInfo DAT_1401c4138. Path/road tool. |
| ~ | `CreatePlacedDescription` | Description | 0x58 (88B) | Ctor 0x140119a60, factory 0x140032a60. TypeInfo DAT_1401c7468. Single-placement tool. |
| ~ | `CreateRemovalDescription` | Description | 0x48 (72B) | Ctor inlined, vtable 0x140162d80, factory 0x140032b70. TypeInfo DAT_1401c74a0. Removal tool. |
| ~ | `CreateRoadDescription` | Description | 0x30 (48B) | Ctor inlined, vtable 0x140162df0, factory 0x140032d30. TypeInfo DAT_1401c74d8. Road building tool. |
| ~ | `RadiusDescription` | Description | 0x30 (48B) | Ctor inlined, vtable 0x14015a580, factory 0x14000b9e0. TypeInfo DAT_1401c4870. Default radius=30 at +0x28. |
| ~ | `SelectDescription` | Description | 0x48 (72B) | Ctor inlined, vtable 0x14015a5f0, factory 0x14000bb90. TypeInfo DAT_1401c48a8. Selection tool. |
| ~ | `ValueDescription` | Description | 0x50 (80B) | Ctor inlined, vtable 0x14015add0, factory 0x14000faa0. TypeInfo DAT_1401c4bf0. Default value=100 at +0x44. |
| ~ | `ToolbarDescription` | Description | 0x68 (104B) | Ctor 0x14012dc30, factory 0x140039490. TypeInfo DAT_1401c7f58. Sentinel -1 at +0x60. Toolbar layout. |
| ~ | `EnvironmentDescription` | Object::External | 0x1b8 (440B) | Ctor 0x1400941c0, factory 0x140019190. TypeInfo DAT_1401c5980. 12×0x20-byte blocks of ARGB colors + float params (lighting/environment presets). |
| ~ | `CaptureBoxDescription` | ElementDescription | 0x78 (120B) | Ctor inlined, vtable 0x14015d4a0, factory 0x140013d90. TypeInfo DAT_1401c5600. Screenshot capture. |
| ~ | `FrustumOutlineDescription` | ElementDescription | 0x80 (128B) | Ctor inlined, vtable 0x14015c300, factory 0x1400122c0. TypeInfo DAT_1401c5168. Sentinel -1 at +0x78. Debug frustum display. |

## Game UI — Dialog Controllers (22 classes)

Registered types inheriting from `UI::DialogInstance`. Each manages a specific game dialog/screen.

| Status | Class | Size | Vtable | Factory | Parent | Purpose |
|--------|-------|------|--------|---------|--------|---------|
| ~ | `AchievementDialog` | 0xd0 (208B) | `0x14015b020` | `0x140010320` | UI::DialogInstance | Achievement display |
| ~ | `AutoSaveDialog` | 0xd0 (208B) | `0x14015b120` | `0x1400106c0` | UI::DialogInstance | Autosave notification |
| ~ | `EventNotifyDialog` | 0xf0 (240B) | `0x14015bca0` | `0x140011500` | UI::DialogInstance | Event notification banner. Ctor 0x140068b60 |
| ~ | `EventPopupDialog` | 0xc0 (192B) | `0x14015bd80` | `0x140011920` | UI::DialogInstance | Event popup window |
| ~ | `FollowDialog` | 0xd8 (216B) | `0x14015be00` | `0x140011a20` | UI::DialogInstance | Camera follow UI |
| ~ | `GoalDialog` | 0xc8 (200B) | `0x14015beb0` | `0x140011b60` | UI::DialogInstance | Goal/objective display |
| ~ | `LoaderDialog` | 0xc8 (200B) | `0x14015f300` | `0x140020bb0` | UI::DialogInstance | Loading screen |
| ~ | `MainMenuDialog` | 0xc8 (200B) | `0x14015f550` | `0x140020d10` | UI::DialogInstance | Main menu |
| ~ | `MapParamsDialog` | 0xf8 (248B) | `0x14015c180` | `0x140012100` | UI::DialogInstance | Map generation parameters. Ctor 0x14006be20 |
| ~ | `MessageDialog` | 0xc0 (192B) | `0x14015c480` | `0x1400125d0` | UI::DialogInstance | Message popup |
| ~ | `ModDetailsDialog` | 0xc8 (200B) | `0x14015c670` | `0x140012790` | UI::DialogInstance | Mod details view |
| ~ | `ModDialog` | 0xd8 (216B) | `0x14015c870` | `0x140012900` | UI::DialogInstance | Mod browser/list |
| ~ | `ModUploadDialog` | 0xb8 (184B) | `0x14015c6c0` | `0x140012bc0` | UI::DialogInstance | Mod upload (Steam Workshop) |
| ~ | `NewGameDialog` | 0xae8 (2792B) | `0x14015c920` | `0x140012c70` | UI::DialogInstance | New game setup (largest). Ctor 0x140071120, has Random@+0x118 |
| ~ | `OptionsDialog` | 0x1c0 (448B) | `0x14015ceb0` | `0x140012df0` | UI::DialogInstance | Options/settings menu. Ctor 0x140075280 |
| ~ | `OptionInputBindDialog` | 0xc8 (200B) | `0x14015c480` | `0x140012e60` | **MessageDialog** | Key binding (extends MessageDialog) |
| ~ | `SaveLoadDialog` | 0x160 (352B) | `0x14015f900` | `0x140021160` | UI::DialogInstance | Save/load screen. Ctor 0x1400b81c0, Sprite@+0xD0 |
| ~ | `ScenarioDialog` | 0xe8 (232B) | `0x14015d2d0` | `0x140013970` | UI::DialogInstance | Scenario selection |
| ~ | `ToolbarDialog` | 0x108 (264B) | `0x14015fa00` | `0x140021710` | UI::DialogInstance | Main toolbar. Ctor 0x1400b9890 |
| ~ | `TutorialGoalDialog` | 0x40 (64B) | `0x1401607a0` | `0x140027440` | **TutorialGoal** | Tutorial goal display |
| ~ | `TutorialHighlightDialog` | 0xf0 (240B) | `0x1401609e0` | `0x1400278c0` | UI::DialogInstance | Tutorial highlight overlay. Ctor 0x1400e0290 |
| ~ | `TutorialSetupDialog` | 0x78 (120B) | `0x140160d20` | `0x1400285b0` | **Object::External** | Tutorial setup. Ctor 0x1400e2cc0, base vtable 0x140160c30 |

## Game UI — Panel Controllers (39 classes)

Registered types inheriting from `UI::DialogController`. Each manages a specific in-game panel/HUD element. Some have paired `*Config` types.

Inheritance note: `SelectResourceUI` is an abstract base class (placeholder factory). `CropUI`, `OrchardUI`, and `PastureUI` extend it. `OrchardUI` and `PastureUI` share the same vtable (`0x14015d330`).

| Status | Class | Size | Vtable | Factory | Parent | Purpose |
|--------|-------|------|--------|---------|--------|---------|
| ~ | `AttendanceUI` | 0x38 (56B) | `0x14015b070` | `0x1400104b0` | UI::DialogController | Attendance panel |
| ~ | `BuildRequirementUI` | 0x60 (96B) | `0x14015b1d0` | `0x1400107f0` | UI::DialogController | Build requirements display |
| ~ | `BuildUI` | 0x50 (80B) | `0x14015f210` | `0x1400209e0` | UI::DialogController | Build/construction panel |
| ~ | `CemeteryUI` | 0x38 (56B) | `0x14015b240` | `0x140010900` | UI::DialogController | Cemetery panel |
| ~ | `CitizenUI` | 0xe8 (232B) | `0x14015ba00` | `0x140010f80` | UI::DialogController | Citizen info panel. Ctor 0x140065a40, InternalBasePtr@+0xE0 |
| ~ | `ConsumeProduceUI` | 0x40 (64B) | `0x14015b2e0` | `0x140010ae0` | UI::DialogController | Production chain panel |
| ~ | `ControlUI` | 0x40 (64B) | `0x14015b380` | `0x140010c00` | UI::DialogController | Control panel (+`ControlUIConfig`) |
| ~ | `CropUI` | 0x60 (96B) | `0x14015f270` | `0x140020b10` | **SelectResourceUI** | Crop field panel. Ctor 0x1400b2c90, base vtable 0x14015d330 |
| ~ | `DebugUI` | 0x130 (304B) | `0x14015b870` | `0x140010ec0` | DialogController | Debug overlay. Ctor 0x140064630 |
| ~ | `DestroyUI` | 0x78 (120B) | `0x14015bb00` | `0x140011130` | UI::DialogController | Demolition panel |
| ~ | `EntityUI` | 0x48 (72B) | `0x14015bb90` | `0x140011270` | UI::DialogController | Entity info panel |
| ~ | `EventLogUI` | 0x80 (128B) | `0x14015bc40` | `0x1400113e0` | UI::DialogController | Event log |
| ~ | `HelpUI` | 0x78 (120B) | `0x14015bf30` | `0x140011ca0` | UI::DialogController | Help panel. Ctor 0x14006a920 |
| ~ | `HotKeyToolTipUI` | 0x30 (48B) | `0x14015bfc0` | `0x140011e20` | UI::DialogController | Hotkey tooltip |
| ~ | `ImmigrationUI` | 0x50 (80B) | `0x14015c080` | `0x140011ee0` | UI::DialogController | Immigration panel |
| ~ | `MapUI` | 0x50 (80B) | `0x14015c350` | `0x140012240` | UI::DialogController | Minimap |
| ~ | `ModBrowseUI` | 0x30 (48B) | `0x14015c500` | `0x1400126d0` | UI::DialogController | Mod browser panel |
| ~ | `NoiseUI` | 0x70 (112B) | `0x14015ca80` | `0x140012d30` | UI::DialogController | Noise/debug display |
| ~ | `OrchardUI` | 0x48 (72B) | `0x14015d330` | `0x140012ff0` | **SelectResourceUI** | Orchard panel |
| ~ | `PageToggleUI` | 0x48 (72B) | `0x14015cfb0` | `0x140013160` | UI::DialogController | Page toggle |
| ~ | `PastureUI` | 0x40 (64B) | `0x14015d330` | `0x140020e10` | **SelectResourceUI** | Pasture panel |
| ~ | `ProductionUI` | 0x50 (80B) | `0x14015d070` | `0x140013230` | UI::DialogController | Production overview |
| ~ | `ProfessionUI` | 0x78 (120B) | `0x14015d0f0` | `0x140013400` | UI::DialogController | Profession assignment. Ctor 0x1400775a0 |
| ~ | `RadiusUI` | 0x38 (56B) | `0x14015d160` | `0x140013590` | UI::DialogController | Radius display |
| ~ | `ResidenceUI` | 0x40 (64B) | `0x14015f6d0` | `0x140020f60` | UI::DialogController | Residence panel |
| ~ | `ResourceLimitUI` | 0x40 (64B) | `0x14015d1f0` | `0x1400136b0` | UI::DialogController | Resource limits (+`ResourceLimitUIConfig`) |
| ~ | `SelectResourceUI` | abstract | — | (abstract) | UI::DialogController | Resource selection base class |
| ~ | `SpriteCaptureUI` | 0x70 (112B) | `0x14015d4f0` | `0x140013d10` | UI::DialogController | Screenshot capture |
| ~ | `StatusBarUI` | 0x1d0 (464B) | `0x14015d810` | `0x1400140e0` | UI::DialogController | Status bar. Ctor 0x14007af20 |
| ~ | `StatusIconUI` | 0x40 (64B) | `0x14015d8a0` | `0x140014180` | UI::DialogController | Status icons (+`StatusIconUIConfig`) |
| ~ | `StorageCapacityUI` | 0x38 (56B) | `0x14015d950` | `0x1400143f0` | UI::DialogController | Storage capacity display |
| ~ | `StorageUI` | 0x40 (64B) | `0x14015f970` | `0x140021330` | UI::DialogController | Storage panel |
| ~ | `TitleDescToolTipUI` | 0x30 (48B) | `0x14015d9f0` | `0x1400144c0` | UI::DialogController | Title/description tooltip |
| ~ | `ToolUI` | 0x58 (88B) | `0x14015da70` | `0x140014580` | UI::DialogController | Tool panel |
| ~ | `TownHallUI` | 0x1e8 (488B) | `0x14015dce0` | `0x140014660` | UI::DialogController | Town hall panel. Ctor 0x140081280 |
| ~ | `TradeUI` | 0x148 (328B) | `0x14015de80` | `0x140015f80` | UI::DialogController | Trade post panel. Ctor 0x140086b80 |
| ~ | `TutorialUI` | 0x60 (96B) | `0x140160f30` | `0x140029400` | UI::DialogController | Tutorial panel |
| ~ | `UpgradeUI` | 0x38 (56B) | `0x14015df00` | `0x140016b30` | UI::DialogController | Upgrade panel (+`UpgradeUIConfig`) |
| ~ | `WorkPlaceUI` | 0x68 (104B) | `0x14015e030` | `0x140016dc0` | UI::DialogController | Workplace panel |

## Game UI — Exe-Specific Elements (2 classes)

UI Element types registered in the exe (beyond the 22+ base elements in Runtime-x64.dll).

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `CaptureBoxElement` | `0x14015d3c8` | Screenshot capture box element |
| - | `FrustumOutlineElement` | `0x14015c218` | Debug frustum outline element |

## Game Systems — Tools & Placement (11 classes)

Registered types for building/creation tools used by the placement system.

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `Toolbar` | `0x140160110` | Tool toolbar container |
| - | `ToolSetting` | `0x140160700` | Tool configuration |
| - | `BridgeTool` | `0x140161ae8` | Bridge placement |
| - | `DragTool` | `0x140160648` | Drag-to-build |
| - | `DragPlacementTool` | `0x140161b78` | Drag placement variant |
| - | `EditTool` | `0x140161c88` | Edit/modify tool |
| - | `FencedTool` | `0x140160658` | Fenced area tool |
| - | `PathTool` | `0x140160670` | Path creation |
| - | `PlacementTool` | `0x140161d10` | Single building placement |
| - | `RemovalTool` | `0x140161dd0` | Demolition/removal |
| - | `RoadTool` | `0x140161e58` | Road building |

## Game Systems — State Machine Actions (32 classes)

Registered types used by `StateMachineComponent` to drive citizen AI behavior. Each action is a discrete work step.

| Status | Class | String Address | Category |
|--------|-------|---------------|----------|
| - | `AnimateNaturalResource` | `0x14015faf0` | Resource |
| - | `AnimAudioEvent` | `0x14015e088` | Audio |
| - | `AutoPickupWork` | `0x140160f88` | Work |
| - | `BuildRoad` | `0x140161008` | Construction |
| - | `BuildStructure` | `0x140161088` | Construction |
| - | `CompleteRoad` | `0x14015fb60` | Construction |
| - | `CompleteTask` | `0x14015fbc0` | General |
| - | `ConsumeProduce` | `0x140161208` | Production |
| - | `ConsumeRawMaterial` | `0x14015fc70` | Resource |
| - | `CreateNaturalResource` | `0x14015fc88` | Resource |
| - | `CreateRawMaterial` | `0x14015fcf0` | Resource |
| - | `DropOffRawMaterial` | `0x14015fd60` | Resource |
| - | `FireWork` | `0x140161288` | Emergency |
| - | `FishingWork` | `0x140161308` | Production |
| - | `ForesterWork` | `0x140161388` | Production |
| - | `GatherRawMaterials` | `0x140161408` | Resource |
| - | `HerbalistWork` | `0x140161588` | Production |
| - | `HuntWork` | `0x140161608` | Production |
| - | `LocationWork` | `0x140161688` | Work |
| - | `MarketWork` | `0x140161778` | Economy |
| - | `MoveTo` | `0x14015fe30` | Navigation |
| - | `PastureWork` | `0x140161788` | Production |
| - | `PerformWork` | `0x14015fe90` | Work |
| - | `Pickup` | `0x140161808` | Resource |
| - | `PickupRawMaterial` | `0x14015fef0` | Resource |
| - | `PickupStoredRawMaterial` | `0x14015ff60` | Resource |
| - | `PlayAnimation` | `0x140158938` | Animation |
| - | `RemoveAnimal` | `0x14015ffd0` | Resource |
| - | `RemoveNaturalResource` | `0x140160030` | Resource |
| - | `SendComponentMessage` | `0x1401600a0` | Messaging |
| - | `ShowTool` | `0x1401589a0` | UI |
| - | `TradeWork` | `0x1401619f8` | Economy |

## Game Systems — Tutorials (16 classes)

Registered types for the tutorial system. Hierarchical: Tutorial contains TutorialSteps, each step has Setup/Goal/Image sub-types.

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `Tutorial` | `0x140160730` | Tutorial definition (External) |
| - | `TutorialInstance` | `0x140160be0` | Runtime tutorial state |
| - | `TutorialStep` | `0x140160e80` | Individual tutorial step |
| - | `TutorialGoal` | `0x140160788` | Goal condition base |
| - | `TutorialGoalButton` | `0x1401607e8` | Goal: click a button |
| - | `TutorialGoalCount` | `0x140160930` | Goal: reach a count |
| - | `TutorialGoalGameSpeed` | `0x1401608c0` | Goal: set game speed |
| - | `TutorialImage` | `0x140160a30` | Tutorial image base |
| - | `TutorialImageHotKey` | `0x140160a90` | Image with hotkey overlay |
| - | `TutorialImageInputKey` | `0x140160b00` | Image with input key |
| - | `TutorialImageToolbarIcon` | `0x140160b70` | Image with toolbar icon |
| - | `TutorialSetup` | `0x140160c18` | Setup action base |
| - | `TutorialSetupCamera` | `0x140160c88` | Setup: position camera |
| - | `TutorialSetupDialog` | `0x140160d00` | Setup: open dialog |
| - | `TutorialSetupGameSpeed` | `0x140160d80` | Setup: set game speed |
| - | `TutorialSetupRestrictPlayer` | `0x140160e00` | Setup: restrict input |

## Game Systems — Achievement Goals (7 exe-specific subtypes)

Goal subtypes registered in the exe. Base types (Achievement, AchievementInstance, AchievementManager, Goal, GoalInstance, GoalList) are in Runtime-x64.dll.

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `BuildingGoal` | `0x1401602f0` | Goal: build N buildings |
| - | `PopulationGoal` | `0x140160358` | Goal: reach population N |
| - | `StatGoal` | `0x1401604b0` | Goal: reach statistic N |
| - | `TimeGoal` | `0x140160530` | Goal: survive N years |
| - | `TimeGoalInstance` | `0x140160518` | Runtime state for TimeGoal |
| - | `FinishTutorialGoal` | `0x140160400` | Goal: complete tutorial |
| - | `FinishTutorialInstance` | `0x1401603e8` | Runtime state for FinishTutorialGoal |

## Game Systems — World Generation (3 classes)

Map and terrain generation types.

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `MapGenerator` | `0x14015ed68` | Top-level map generation |
| - | `TerrainGenerator` | `0x14015f158` | Terrain heightmap/biome generation |
| - | `ClimateGenerator` | `0x14015e128` | Climate/weather pattern generation |

## Game Systems — Miscellaneous Types (9 classes)

Other registered types in the exe that don't fit the above categories.

| Status | Class | String Address | Purpose |
|--------|-------|---------------|---------|
| - | `ComponentEntity` | `0x14015fa50` | Entity that holds components (ECS container) |
| - | `Profession` | `0x14015ef28` | Profession definition (External) |
| - | `ProfessionList` | `0x14015ef38` | Collection of professions |
| - | `Scenario` | `0x14015efe8` | Scenario definition (External) |
| - | `Weather` | `0x140160230` | Weather system type |
| - | `MenuScene` | `0x14015edc8` | Main menu 3D scene |
| - | `ResourceTip` | `0x14015f200` | Resource tooltip type |
| - | `SaveLoadEmpty` | `0x14015ebc0` | Empty save state placeholder |
| - | `AutosaveInterval` | `0x14015e548` | Autosave timing config |

## Key Standalone Functions

Major game-logic functions identified but not yet part of any class decomposition.

| Status | Address | Name | XRefs | Purpose |
|--------|---------|------|-------|---------|
| - | `0x140041ef0` | `GameMain` | 3 | Top-level startup — creates Engine, MainLoop, all loop types |
| - | `0x140096380` | (unnamed) | — | **Main gameplay tick** — calls ComponentEntity update, AchievementManager, AnimationManager, AudioScene, Camera, SaveLoad, UI |
| - | `0x1400a9660` | `StateMachine_Tick` | 62 | State machine update (all StateMachineComponents) |
| - | `0x140001740` | (unnamed) | 745 | Vector/array free (most-called function in binary) |
| - | `0x1400c54e0` | (unnamed) | 143 | Vector push_back |
| - | `0x1400a6a30` | (unnamed) | 88 | Entity component message dispatch |
| - | `0x14009f3e0` | (unnamed) | 102 | Terrain grid query with pathfinding |
| - | `0x1400bf790` | `Pathfinding_FindPath` | 12 | A* pathfinding |
| - | `0x14012cf10` | `StorageComponent_QueryCount` | 44 | Storage query — O(entries×components), critical hotspot |
