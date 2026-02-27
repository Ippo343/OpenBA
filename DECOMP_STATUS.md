# Decompilation Status & Type Inventory

Status key: `-` = not started, `~` = partially decompiled, `x` = fully decompiled & documented, `R` = reimplemented & hooked in-game

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
| R | `MainLoop` | Engine | Run, Frame, Update, Draw, SetNextLoop (**reimpl: `engine/reimpl/mainloop.cpp`**), Get, IsRunning, GetContext, GetCurrentLoop, GetDeltaTime, GetFrameTime, GetFrameStartTime, GetFrameUpdateTime, MarkFrameUpdateTime, AddPostUpdate, AddPostDraw, RemovePostUpdate, RemovePostDraw, OnVideoError, OnVideoRestart, CreateVideoContext, ReleaseVideoContext, ctor, dtor, singleton |
| ~ | `UpdateLoop` | Engine | Update, Draw, Startup, Shutdown, SaveRestore, DrawFailed, OnVideoRestart, AllowsModChanges, GetResourceFilename, IsRunning (**reimpl: `engine/reimpl/updateloop.cpp`**), LoadResources, SetLoadProgress, GetLoadProgress, GetLoadBit, ctor, dtor |
| R | `LoaderLoop` | Engine | Update, Draw, Load, Startup, Shutdown, OnVideoError, IsFinished, Exit (**reimpl: `engine/reimpl/loaderloop.cpp`**), ctor, dtor |
| - | `DialogLoaderLoopT` | Engine | Update, Draw, Load, Startup, ctor, dtor |
| - | `DialogLoopT` | Engine | Update, Draw, Load, Startup, Shutdown, ctor, dtor |
| - | `Application` | System | YieldToThreads |
| ~ | `Configuration` | System | CurrentBuildId (static), CurrentVersion (static), GetVersionString (static) |
| - | `Modification` | Engine | (mod system) |

### ECS - Game Components (all ComponentEntity children)

| Status | Component | Category | Purpose |
|--------|-----------|----------|---------|
| - | `CitizenAIComponent` | AI | Citizen behavior/decision making |
| - | `CitizenComponent` | AI | Citizen state/attributes |
| - | `StateMachineComponent` | AI | Generic AI state machines |
| - | `CharacterComponent` | AI | Character traits/properties |
| - | `AnimalComponent` | AI | Animal behavior base |
| - | `WildAnimalComponent` | AI | Wild animal spawning/AI |
| - | `HerdComponent` | AI | Animal herding logic |
| - | `LivestockComponent` | AI | Domesticated animal management |
| - | `BoatComponent` | AI | Boat/water transport |
| - | `WorkComponent` | Economy | Job execution logic |
| - | `WorkPlaceComponent` | Economy | Building work assignment |
| - | `StorageComponent` | Economy | Resource storage management |
| - | `StorageLocationComponent` | Economy | Physical storage positions |
| - | `StorageDisplayComponent` | Economy | Visual storage display |
| - | `MarketComponent` | Economy | Market resource distribution |
| - | `MarketLocationComponent` | Economy | Market building positions |
| - | `ConsumeProduceComponent` | Economy | Production chain logic |
| - | `ResourceProduceComponent` | Economy | Raw resource production |
| - | `RawMaterialComponent` | Economy | Raw material definitions |
| - | `AbsorbResourceComponent` | Economy | Resource absorption |
| - | `TradeComponent` | Economy | Trading post logic |
| - | `ResidenceComponent` | Population | Housing assignment/capacity |
| - | `PopulationComponent` | Population | Population tracking/stats |
| - | `HappinessComponent` | Population | Citizen happiness calculation |
| - | `DiseaseComponent` | Population | Disease spread/management |
| - | `ImmigrationComponent` | Population | Nomad/immigration events |
| - | `NomadComponent` | Population | Nomad arrival handling |
| - | `AttendanceComponent` | Population | Building attendance tracking |
| - | `HealerComponent` | Population | Healer building logic |
| - | `HerbalistComponent` | Population | Herbalist building logic |
| - | `CropFieldComponent` | Production | Crop field farming |
| - | `ForesterComponent` | Production | Forester lodge tree management |
| - | `FishingComponent` | Production | Fishing dock logic |
| - | `OrchardComponent` | Production | Orchard tree management |
| - | `PastureComponent` | Production | Pasture animal grazing |
| - | `MineComponent` | Production | Mine resource extraction |
| - | `QuarryComponent` | Production | Quarry stone extraction |
| - | `WellComponent` | Production | Well water supply |
| - | `HuntComponent` | Production | Hunting cabin logic |
| - | `FruitComponent` | Production | Fruit tree harvesting |
| - | `GrowthComponent` | Simulation | Plant/tree growth over time |
| - | `NaturalResourceComponent` | Simulation | Natural resource spawning |
| - | `FireComponent` | Simulation | Fire spread/damage |
| - | `TornadoComponent` | Simulation | Tornado disaster |
| - | `CemeteryComponent` | Building | Cemetery management |
| - | `TombstoneComponent` | Building | Individual grave tracking |
| - | `SpawnComponent` | Building | Entity spawn point |
| - | `TerraformComponent` | Building | Terrain modification |
| - | `FloorComponent` | Building | Floor/foundation placement |
| - | `ZoneComponent` | Building | Zone area definitions |
| - | `BuildComponent` | Building | Construction progress |
| - | `DestroyComponent` | Building | Demolition handling |
| - | `ClearAreaComponent` | Building | Area clearing for construction |
| - | `ClearResourceComponent` | Building | Resource removal from area |
| - | `MultiObjectComponent` | Building | Multi-tile building logic |
| - | `MapComponent` | World | World map data |
| - | `TrackerComponent` | World | Entity tracking/indexing |
| - | `InteractComponent` | Gameplay | Player interaction handling |
| - | `AutoPickupComponent` | Gameplay | Auto-resource collection |
| - | `PickingComponent` | Gameplay | Mouse picking/selection |
| - | `ControllerComponent` | Gameplay | Generic controller |
| - | `ModelComponent` | Rendering | 3D model reference |
| - | `DecalComponent` | Rendering | Terrain decal |
| - | `ParticleComponent` | Rendering | Particle effect |
| - | `HighlightComponent` | Rendering | Selection highlight |
| - | `StatusIconComponent` | Rendering | Overhead status icons |
| - | `UIComponent` | UI | In-world UI element |

### ECS - Tool/Creation Descriptions

| Status | Description | Purpose |
|--------|-------------|---------|
| - | `CreateBridgeDescription` | Bridge placement tool |
| - | `CreateDragDescription` | Drag-to-build tool |
| - | `CreateFencedDescription` | Fenced area tool |
| - | `CreatePathDescription` | Path/road tool |
| - | `CreatePlacedDescription` | Single-placement tool |
| - | `CreateRemovalDescription` | Removal tool |
| - | `CreateRoadDescription` | Road building tool |
| - | `RadiusDescription` | Radius-based placement |
| - | `SelectDescription` | Selection tool |
| - | `ValueDescription` | Value display |
| - | `ToolbarDescription` | Toolbar layout |
| - | `EnvironmentDescription` | Environment settings |
| - | `CaptureBoxDescription` | Screenshot capture |
| - | `FrustumOutlineDescription` | Debug frustum display |

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
| ~ | `Configuration` | CurrentBuildId, CurrentVersion, GetVersionString |
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
| - | `Modification` | mod support |
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

### Engine (Main Loop) - 86 symbols

| Status | Class | Known Methods |
|--------|-------|---------------|
| R | `MainLoop` | Get, Run, Frame, Draw, Update, SetNextLoop (**reimpl: `engine/reimpl/mainloop.cpp`**), GetCurrentLoop, GetDeltaTime, GetFrameTime, GetFrameStartTime, GetFrameUpdateTime, MarkFrameUpdateTime, IsRunning, GetContext, AddPostDraw, AddPostUpdate, RemovePostDraw, RemovePostUpdate, OnVideoError, OnVideoRestart, OnVideoError_, OnVideoRestart_, CreateVideoContext, ReleaseVideoContext, ctor, dtor (singleton, 26 methods total) |
| ~ | `UpdateLoop` | Load, Startup, Update, Draw, Shutdown, SaveRestore, IsRunning (**reimpl: `engine/reimpl/updateloop.cpp`**), LoadResources, SetLoadProgress, GetLoadProgress, GetLoadBit (virtual base, 0x28 bytes) |
| R | `LoaderLoop` | Startup, Shutdown, Update, Draw, Load, Exit, IsFinished, OnVideoError (**reimpl: `engine/reimpl/loaderloop.cpp`**) (0x48 bytes) |
| - | `Loader` | resource loader (singleton) |
| - | `DialogLoaderLoopT` | Update, Draw, Load, Startup |
| - | `DialogLoopT` | GetUserInterface |
| - | `Application` | top-level application class |

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
