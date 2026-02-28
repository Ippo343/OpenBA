# Runtime-x64.dll (Engine Core)

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
