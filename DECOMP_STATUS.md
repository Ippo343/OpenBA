# Decompilation Status & Type Inventory

Status key: `-` = not started, `~` = partially decompiled, `x` = fully decompiled & documented, `R` = fully reimplemented & hooked in-game

## Binary Summary

| Binary | Functions | Classes | Role |
|--------|-----------|---------|------|
| Application-x64.exe | 10,476 | ~67 ECS components + game logic | Game logic, UI controllers, ECS |
| Runtime-x64.dll | 15,290 | 100+ across 18 namespaces | Full engine runtime |
| VideoDX11-x64.dll | 576 (~86 rendering) | ~25 Video::*Data classes | DX11 renderer plugin |

---

## Application-x64.exe (Game Logic)

### Engine Core

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

### ECS - Game Components (all ComponentEntity children)

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

### ECS - Tool/Creation Descriptions

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
| ~ | `EnvironmentDescription` | Object::External | 0x1b8 (440B) | Ctor 0x1400941c0, factory 0x140019120. TypeInfo DAT_1401c5980. 12×0x20-byte blocks of ARGB colors + float params (lighting/environment presets). |
| ~ | `CaptureBoxDescription` | ElementDescription | 0x78 (120B) | Ctor inlined, vtable 0x14015d4a0, factory 0x140013d90. TypeInfo DAT_1401c5600. Screenshot capture. |
| ~ | `FrustumOutlineDescription` | ElementDescription | 0x80 (128B) | Ctor inlined, vtable 0x14015c300, factory 0x1400122c0. TypeInfo DAT_1401c5168. Sentinel -1 at +0x78. Debug frustum display. |

---

## Runtime-x64.dll (Engine Core)

15,290 functions, ~4,800+ exports, 18 namespaces. Every exported class follows a consistent pattern: `_TypeName`, `_ParentTypeName`, `Type`, `TypeId`, `TypeName`, `operator_new`, `operator_delete`, `CreateNewObject`, `CreateInPlaceObject`, `Reconstruct`, `Construct`, `Destruct`, copy-ctor, `operator=`.

### System (Core Types) - 508 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `String` | ctor, operator=, concatenation, comparison, conversion |
| - | `WideString` | StrLength, AsUTF8, dtor |
| - | `Filename` | GetPathFilename, GetPathIdentifier |
| - | `Color` | RGBA (uint32-backed) |
| - | `Time` | AsSeconds (int64 ticks) |
| - | `Timer` | timer utility |
| - | `DateTime` | from FILETIME |
| - | `Random` | Rand, RandTol, RandFloat, RandFloatTol |
| - | `CriticalSection` | mutex wrapper |
| - | `StackCriticalSection` | scoped lock |
| - | `Thread` | Start, WaitForFinish, OnCompleted (vtable) |
| - | `Compression` | compression utility |
| R | `Configuration` | **10 methods hooked**: CurrentBuildId, CurrentVersion, GetVersionString, GetPlatformName, GetLanguageName, GetBuildName, GetCommandLineString, SetCommandLine (8 full reimpl), Ctor, Dtor (2 passthrough — lifecycle). 3 too-small documented. Uses Map::Find/Insert, ParseCommand/ParseValue. |
| - | `MemoryAllocator` | MemAlloc, MemFree |
| - | `Debug` | debug output |
| - | `Error` | SetText, Display |
| - | `CRC` | CRC hash |

### Math - 384 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Scalar` | float wrapper (SSE float1) |
| - | `Vector2` | Scalar, ctor (float2) |
| - | `Vector3` | Scalar, X, Y, Dot, Normalize, Length, Heading (float3) |
| - | `Vector4` | float4 |
| - | `Matrix` | Identity (4x4) |
| - | `Quaternion` | FromYawPitch, FromYawPitchOffset |
| - | `Transform` | SetTransform, GetTransform, Identity, operator* (Quat + Vec3) |
| - | `Transform2` | 2D transform (rotation + translation) |
| - | `Transform3` | Identity (axis vectors + origin) |

### Volume (Bounding Primitives) - 93 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Box` | Extend (AABB, min/max Vector3) |
| - | `Circle` | center + radius |
| - | `Rectangle` | min/max Vector2 |
| - | `Frustum` | 6 planes, perspective or ortho |
| - | `Plane` | normal + offset |

### Object System - 806 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Base` | TypeName, Type, IsA, Construct, Destruct, Reconstruct, Serialize, operator_delete |
| - | `BaseTypeInfo` | RTTI descriptors |
| - | `Type` | type registry (uint32) |
| - | `Internal` | internal (owned) object |
| - | `External` | external (resource) object, GetPath |
| - | `InternalBasePtr` | IsPtrValid, CheckPtr |
| - | `InternalPtr<T>` | smart pointer for internal objects |
| - | `ExternalPtr<T>` | smart pointer for external objects |
| - | `Factory` | object creation |
| - | `InternalFactory` | internal object creation |
| - | `ExternalFactory` | FindOrLoad, GetPathForReferenceId, IsReferenceModeEnabled |
| - | `ExternalList` | external resource lists |
| - | `ExternalCompiler` | asset compilation |
| - | `ExtensionCompiler` | extension compilation |
| - | `CompiledExternal` | compiled resource |

### IO (File System & Serialization) - 550 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Stream` | base stream (vtable) |
| - | `FileStream` | file-backed stream |
| - | `MemoryStream` | memory-backed stream |
| - | `CompressedStream` | compressed stream |
| - | `CompressedFileStream` | compressed file I/O |
| - | `CompressedMemoryStream` | compressed memory I/O |
| - | `CompressedCopyFileStream` | compressed copy stream |
| - | `BinaryFileStream` | binary file stream |
| - | `TextFileStream` | text file I/O |
| - | `TextFile` | text file |
| - | `VisitorStream` | visitor-pattern stream |
| - | `Visitor` | visitor pattern |
| - | `Serializer` | serialization engine |
| - | `BinaryReader` | typed binary reader |
| - | `BinaryWriter` | typed binary writer |
| - | `ReaderWriter` | bidirectional reader/writer |
| - | `FileSystem` | virtual file system (packages + mods) |
| - | `PackageFile` | package archive |
| R | `Modification` | **8 methods hooked** (see Engine Core above): Find, Enable, DisableAll, Swap, SaveList (5 full reimpl) + EnableSavingSettings, RefreshList, SetupFileSystem (3 passthrough). Uses Persistence SetBool/SetInt, Filename::GetPathFilename, ModArrayCopy. 6 too-small documented. |
| - | `Persistence` | GetSavePath, save state |
| - | `Registry` | Windows registry access |

### Graphics / Rendering - 641 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Scene` | scene graph root |
| - | `Renderer` | Draw, AddDrawItem, GetShader, GetCullCamera |
| - | `Camera` | Frustum + Transform3 |
| - | `Geometry` | UpdateAttached, AABB |
| - | `GraphicsMesh` | Node, TriangleList |
| - | `MeshInstance` | Update, UpdateHierarchy, AddDrawItems, SetMesh, SetMaterials, IsAnimating |
| - | `EdgeMesh` | Vertices, ctor, dtor |
| - | `EdgeMeshInstance` | AddObject, RemoveObject, SetMesh |
| - | `EdgeVertex` | Set |
| - | `TriangleVertex` | Set |
| - | `Material` | material definitions |
| - | `MaterialInstance` | per-object materials, BuildParameters |
| - | `Light` | base light |
| - | `AmbientLight` | ambient light |
| - | `SpotLight` | position, cone, color, shadow map |
| - | `SunLight` | Update, Draw, DrawShadowMap, AddDrawItems |
| - | `Animation` | animation data |
| - | `AnimationGroup` | animation sets (external resource) |
| - | `AnimationEvent` | animation callbacks |
| - | `AnimationManager` | Update, Serialize |
| - | `AnimationState` | playback state |
| - | `Particle` | single particle |
| - | `ParticleInstance` | Update, AddDrawItems |
| - | `ParticleSystem` | particle system def |
| - | `ParticleSystemInstance` | Update |
| - | `InterpolatedState<T>` | smooth interpolation (Transform, Vector3, Box, float, double) |
| - | `FrameCopyImageBuffer` | Render |
| - | `GeneratedImageBuffer` | generated textures |
| - | `ReflectionImageBuffer` | reflection rendering |
| - | `Sprite` | 2D sprite |
| - | `SpriteSheet` | sprite atlas |
| - | `SpriteCapture` | sprite rendering |
| - | `SpriteContext` | sprite batch state |

### Video (GPU Abstraction) - 669 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Device` | Lock, Unlock, ReleaseDynamic, AdjustDisplay, singleton |
| - | `Device::Adapter` | GPU adapter info |
| - | `Device::DisplayMode` | display mode |
| - | `Device::GraphicsInterface` | graphics API interface |
| - | `Device::RefreshRate` | refresh rate |
| - | `DeviceData` | platform-specific device data |
| - | `Context` | SetRenderBuffer, SetDefaultRenderBuffer |
| - | `ContextData` | platform-specific context data |
| - | `RenderBuffer` | Create, ctor, dtor |
| - | `RenderBufferData` | Bind |
| - | `ImageBuffer` | Width, Height, IsCompressed, GetLevelCount, CopyFromTexture |
| - | `ImageBufferData` | platform-specific texture data |
| - | `VertexBuffer` | Lock, Unlock |
| - | `VertexBufferData` | platform vertex data |
| - | `IndexBuffer` | Lock, Unlock |
| - | `IndexBufferData` | platform index data |
| - | `ConstantBuffer` | Bind (context, data, size) |
| - | `ConstantBufferData` | platform constant buffer |
| - | `VertexLayout` | vertex input layout |
| - | `VertexLayoutData` | IAInputLayout |
| - | `VertexProgram` | vertex shader |
| - | `VertexProgramData` | platform vertex shader |
| - | `PixelProgram` | pixel shader |
| - | `PixelProgramData` | platform pixel shader |
| - | `VideoProgram` | combined shader program |
| - | `VideoProgramData` | platform shader program |
| - | `ShaderProgram` | shader with Parameter constants |
| - | `RenderState` | render state management |
| - | `RenderState::AlphaBlend` | alpha blend state |
| - | `RenderState::Depth` | depth state |
| - | `RenderState::Stencil` | stencil state |
| - | `RenderStateData` | platform render state |
| - | `Viewport` | viewport rect |
| - | `Configuration` | video configuration, singleton |

### UI System - 1,766 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `UserInterface` | Update, PausesGame |
| - | `Dialog` | GetTemplate, CreateElement |
| - | `DialogInstance` | OnUpdate, OnUpdateLayout, AddController, GetController, FindElement, SetElementId, SetChild |
| - | `DialogController` | OnUpdate, OnUpdateLayout, OnOpened, OnClosed, OnCreated, OnElementEvent, OnSerializeSettings, GetContainer, CreateInPlaceObject |
| - | `DialogControllerConfig` | ctor, dtor |
| - | `Element` | Update, SetFlagsOnOff, FindNext |
| - | `ElementDescription` | GetFileVersion, Serialize, CreateInstance |
| - | `Font` | font rendering |
| - | `CharacterSet` | character set data |
| - | `StringTable` | localized string lookup |
| - | `Cursor` | mouse cursor |
| - | `ComboDialog` | dropdown dialog |
| - | `ToolTipDialog` | tooltip dialog |

**UI Element Types** (all under `UI` namespace, each has matching `*Description`):

| Status | Element | Purpose |
|--------|---------|---------|
| - | `AnimatedImageElement` | Animated image display |
| - | `BorderElement` | Border/frame |
| - | `ButtonElement` | Clickable button |
| - | `CheckElement` | Checkbox |
| - | `ClipRegionElement` | Clip region |
| - | `ComboElement` | Dropdown combo box |
| - | `ContainerElement` | Generic container |
| - | `DocumentTextElement` | Rich text display |
| - | `EditElement` | Text input field |
| - | `GraphElement` | Data graph/chart |
| - | `GroupElement` | Grouping container |
| - | `ImageElement` | Image display |
| - | `LabelElement` | Text label |
| - | `LayoutElement` | Layout container |
| - | `ListElement` | List display |
| - | `NumberElement` | Number display |
| - | `NumberEditElement` | Number input |
| - | `PageElement` | Tab page content |
| - | `ProgressElement` | Progress bar |
| - | `RadioElement` | Radio button |
| - | `RibbonElement` | Ribbon UI |
| - | `ScrollElement` | Scroll container |
| - | `SliderElement` | Slider control |
| - | `SpinnerElement` | Numeric spinner |
| - | `TabElement` | Tab container (AddTab) |
| - | `TableElement` | Data table |
| - | `TextElement` | Text block (SetParameter) |

### Collision2D - 81 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `CollisionMesh` | edges, triangles, vertices, normals |
| - | `Surface` | mask, color, report-only |
| - | `Scene` | AddCircle, AddMesh, Collide, Intersect (swept circles, static meshes) |

### Picking (3D) - 52 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `PickingMesh` | SubMesh, AABB, triangles, vertices, normals |
| - | `Scene` | AddMesh, Clip, Intersect, GetTransform |

### Pathfinding - 49 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `PointList` | navigation point list (external resource) |
| - | `Scene` | AddNode, Build, GeneratePath, SmoothPath, RemoveNode (A* on 2D grids) |
| - | `IPathInfo` | path info interface |

### Input - 69 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Device` | GetValue, Pressed, Released, PressedOrRepeated (keyboard/mouse/gamepad, singleton) |
| - | `Configuration` | mouse clipping/display, legacy mode (singleton) |
| - | `KeyMap` | RegisterInput, GetValue, Pressed, Released, ClearAll, Save, Load |

### Achievement - 102 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Achievement` | title, description, goals, sprite |
| - | `AchievementInstance` | Update, IsMet, HasFailed |
| - | `AchievementManager` | GetAchievementCount, SetAchievementMet, ReadAchievements (singleton) |
| - | `Goal` | text |
| - | `GoalInstance` | Update, IsMet, HasFailed, CheckGoalState |
| - | `GoalList` | AreGoalsMet, AnyGoalFailed |

### Save / Persistence - 39 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `SaveLoad` | SetOperation, Update, Draw, LoadMetaData, SaveMetaData (singleton) |
| - | `SavedState` | Store, Retrieve, Discard |

### Collection (Containers) - 41 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `Array<T>` | dynamic array |
| - | `LinkedList<T>` | doubly-linked list |
| - | `Set<T>` | sorted set (AVL tree) |

### Utility - 20 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `PerlinNoise` | noise generation |
| - | `ImageTools` | image resize, color swap, load from memory |
| - | `HashGrid<T>` | spatial hash grid (used by Collision2D, Pathfinding) |

### Debug - 8 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| - | `AnimationDebug` | draw animation skeleton |
| - | `Collision2DDebug` | draw collision shapes |
| - | `PathfindingDebug` | draw pathfinding graph |
| - | `PickingDebug` | draw picking meshes |

---

## Reimplementation Dependencies & Blockers

All engine core loop classes (MainLoop, UpdateLoop, LoaderLoop, DialogLoaderLoopT, DialogLoopT) are now fully reimplemented (R). Resolved infrastructure: ExternalFactory (FindOrLoad, Clean, IsDerivedFrom), String lifecycle (ctor/dtor/copy), Filename::AdjustDelimiters.

All engine core classes promoted to R. Configuration fully reimplemented (8/10 methods, remaining 2 are lifecycle passthroughs). Modification at 5/8 full reimpl, remaining 3 passthroughs need deeper I/O subsystem support.

Resolved dependencies (used by Configuration and Modification reimplementations):
- `Map::Find` (0x18002ace0) — AVL tree lookup for command-line map
- `Map::Insert` (0x18002a090) — AVL tree insert for command-line map
- `ParseCommand` (0x1800c4a00) — extract "/command" token from CLI string
- `ParseValue` (0x1800c4930) — extract value token after command
- `Persistence::_singleton` (0x1801fc6b8) — key/value settings store
- `Persistence::SetBool` (0x1800b8380) — store bool via "prefix + suffix" key format
- `Persistence::SetInt` (0x1800b80c0) — store int via "prefix + suffix" key format
- `Filename::GetPathFilename` (0x1800c6740) — extract base filename, strips path/extension
- `ModArrayCopy` (0x180010110) — deep copy Mod array (0x80-byte elements)

Remaining passthrough wrappers (blocked on subsystem infrastructure):

| Passthrough Function | Dependency | What's Needed |
|---------------------|------------|---------------|
| `Modification::RefreshList` | `FileSystem`, `PackageFile`, `Persistence` | ~3646 bytes, scans filesystem for .pkm packages, rebuilds mod list |
| `Modification::SetupFileSystem` | `FileSystem`, `PackageFile`, `ExternalFactory` | ~2617 bytes, mounts mod packages into engine VFS |
| `Modification::EnableSavingSettings` | Calls `RefreshList` | Thin wrapper, blocked by RefreshList |
| `Configuration::Ctor` | Map init, field zeroing | 62 bytes, lifecycle — low priority |
| `Configuration::Dtor` | Map teardown, list cleanup | 49 bytes, lifecycle — low priority |

### Hook Infrastructure Notes

- **Total hooks**: 57 (all verified in-game)
- **Trampoline safety**: Some hooks have RIP-relative instructions in their prologues. Trampolines for these are broken (wrong displacement after relocation). Currently safe because no reimplementations call through trampolines. If A/B testing via trampoline is needed later, `InstallHook` must detect and relocate RIP-relative instructions.
- **Too-small functions**: Functions under 12 bytes cannot receive the inline jump patch. These are documented as "R (too small to hook)" with full reimplementation in source comments.

---

## VideoDX11-x64.dll (DX11 Renderer Plugin)

576 functions (~86 rendering code, rest CRT). Plugin-style DLL with minimal export surface.

### Exports

| Export | Address | Purpose |
|--------|---------|---------|
| `CreateDeviceData` | `0x1800011a0` | Factory - creates DX11 device backend |
| `GetDeviceName` | `0x180001230` | Returns renderer name |

### Classes (all `*Data` companion classes for engine abstractions)

| Status | Class | Purpose |
|--------|-------|---------|
| - | `DeviceData` | DX11 device implementation |
| - | `ContextData` | DX11 context bindings |
| - | `VertexBufferData` | DX11 vertex buffer |
| - | `IndexBufferData` | DX11 index buffer |
| - | `ConstantBufferData` | DX11 constant buffer |
| - | `RenderBufferData` | DX11 render target |
| - | `ImageBufferData` | DX11 texture |
| - | `VertexProgramData` | DX11 vertex shader |
| - | `PixelProgramData` | DX11 pixel shader |
| - | `VideoProgramData` | DX11 combined shader program |
| - | `RenderStateData` | DX11 rasterizer/blend/depth state |
| - | `VertexLayoutData` | DX11 input layout |

### Rendering Specs
- Backbuffer: `DXGI_FORMAT_B8G8R8A8_UNORM`
- Depth/Stencil: `DXGI_FORMAT_D24_UNORM_S8_UINT`
- Min feature level: `D3D_FEATURE_LEVEL_10_0`
- MSAA support, device lost/reset handling
- Imports: `d3d11.dll` (D3D11CreateDevice), `dxgi.dll` (CreateDXGIFactory1), DINPUT8
