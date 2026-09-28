# W1 TESTS — what the unit tests became at the flip (was T0_LEFT_FOR_W1.md)

Branch `b3x-W1T` (on `b3x-W1`, the product flip). `make unit`: **919 tests, 0 failures** (916 before;
+3 in the new `tests/unit/dtor_check_test.cpp`). No product file was touched: no product seam was needed.
T0's per-file worklist (this file's previous content, `T0_LEFT_FOR_W1.md`) is in git history; each of
its categories now says what was done.

## The shared helper: `tests/unit/dtor_check.h`
What the retired slot-0 flags proved, proven the C++ way (B3X_RULES X4/X5):

| retail | now | how the test proves it |
|---|---|---|
| flags 1 (destroy, then MemFree `this`) | `delete p` → D0 (compiler slot 1) | destructor ran once AND a recording class-scope `operator delete` got `p` (`DTOR_CHECK_RECORDING_DELETE`, `dtor_check::Recording<T>` + `NewOnMalloc`); `HasClassDelete<T>` static_asserts D0 never reaches the global `operator delete` |
| flags 0 (destroy, no free) | `p->~T()` / D1 (compiler slot 0) | destructor ran once, the recording `operator delete` was NOT called |
| flags 3 (vector arm) | no Itanium counterpart; `delete[]` of `new T[n]` | `CHECK_ARRAY_DELETE(T, count, order)`: three destructors, DOWNWARD |
| raw slot 0 | `CallD1` / `CallD0` | D1 on a placement-new'd object (never destroyed again), D0 on a malloc'd one |

`dtor_check_test.cpp` (3 tests) shows each helper can FAIL (missing / upward destructor log, D1 vs D0,
`HasClassDelete` true/false).

## Per category (T0 counts → done)

| category | T0 | done |
|---|---:|---|
| `slot0` raw slot-0 reads/calls | 29 | all rewritten. The six stack-probe traps (audio, io_streams, io_vtable ×3, pathfinding) + the ones the forks found (ui_dialog 3, ui_element 2, ui_leaf_c3 1 — **not on T0's list** —, c4 2, ui_resources 1, graphics_* 4, video_* 3, object_external, object_typeinfo, object_vtable) now call D1 once on a placement-new'd probe and D0 on a heap probe with a recording operator delete. `video_data:256` (`MirrorDeviceData`, `*Data`) KEPT |
| `zt` `_ZTV…[2](p,1u)` | 8 | flags-1 calls → `[3]` typed `void(*)(void*)` (D0); the flags-0 call (rb_graphics SunLight) → `[2]` (D1); all kept RAW on purpose with a comment, plus `[2] != [3]` and a Recording<T> D0 to prove the free |
| `arm` vector arm | 42 | **41 converted** to `delete[]` (`CHECK_ARRAY_DELETE`, or a braced `new T[n]{…}` where the class has no default ctor), **0 retired**, 1 exempt kept (`b39_vid_data_test.cpp`, `*Data`). List below |
| `dd01` `DeletingDtor(0)/(1)` | 155 | `(0)` → `p->~T()` (qualified `p->Base::~Base()` where the old call was qualified: rb_engine ×3, rb_ui ×2); `(1)` → `delete p` + a recording free; b39_vid_data's 2 KEPT (exempt) |
| `decl` declarations / stand-in overrides | 116 | trivial `return this;` overrides deleted; recording ones → `~Probe() override { Note(…); }` (macros `C3_PROBE`, `C4_PROBE`, `STREAM_PROBE_SEVEN` edited); mirrors of flipped classes → `virtual ~Mirror()` (object_vtable, io_vtable `TestReaderWriter`/`TestStream` family, graphics_geometry/particle probes, `MirrorImageBuffer`, `MirrorShaderProgram`, `MirrorVertexLayout`, `MirrorCompiledExternal`); `*Data` mirrors KEPT (3, video_data) |
| `hvd` `has_virtual_destructor` | 197 | inverted on flipped classes; exempt lines unchanged (Thread family, `Engine::Loader`, `PathfinderThread`, Controller / `RootController` / `TableController`, `IO::Encoding`, `PlatformChannel`, `IPathInfo`, `*Data`, `IO::PersistenceWorker`) |
| `isd` `is_destructible` | 97 | inverted on flipped classes (destructors are public); exempt unchanged |
| `loop` whole-table loops / minted copies | 43 | bounds +1 (distinctness loops over n+1 entries, D1 != D0 asserted), minted copies one entry longer, patched index through `RetailSlot`; the `CheckDistinct*` helpers of io_streams / io_vtable take the retail count and translate it (ui_dialog's takes the compiler count) |
| `destroy` `Object::Destroy(` | 17 | kept as calls (it is `delete p` now); object_typeinfo's `Destroy(&stackObject)` → a heap object with a recording operator delete on the root; object_internal's `TestInternal` records in `~TestInternal()` and its own `operator delete`; object_external frees through `TypeTreeFree` in a recording operator delete |
| raw reads on exempt tables | — | untouched (T0's list at the end still applies) |

**Found beyond T0's list** (they failed only at run time):
- hand tables handed to BY-NAME product calls were still in retail order: `math_test.cpp` FakeSerializer
  (`Transform::Serialize` → `SerializeFloatArray`), `b39_io_classes_test.cpp` FakeStream (the real
  `BinaryReader`/`Writer` ctors call the stream by name), `io_vtable_abi_test.cpp` `ser_int64::g_table`
  (the real `Serializer::Serialize(int64&)`) and the direction-B table: now in the compiler's order, each
  entry placed through `RetailSlot<T>`. `io_vtable`'s `TestSerializer` over `minted::g_table` stays
  retail-order (a transcription of retail's raw reads, keyed on `MintedRwStorage`), as T0 said.
- `is_default_constructible` asserts that were false only because the destructor was protected
  (`UI::CharacterSet`, `Video::VertexLayout` ×2, `VertexProgram`, `PixelProgram`, `VideoProgram`): true now.
- io_vtable `TailReaderWriter`: its tail moved into `~TailReaderWriter()`; the objects moved from
  `HeaderEncoding` MEMBERS to raw storage (the product's `~Serializer` now `delete`s them, so as members
  they would have been destroyed twice); a no-free recording operator delete proves flags 1's "then free".

## Vector arms (retail address → replacement)
All `delete[]` of `new T[n]`, destructor order checked downward unless noted.
- audio: SoundBuffer 0x18000bef0, SoundEffect 0x180050820
- collision2d: Surface 0x180064ea0, CollisionMesh 0x1800656b0
- object: Base 0x1800065b0, External 0x180006640, CompiledExternal 0x18000b7e0, ExternalList 0x180007800
- pathfinding/picking: PointList 0x1800713b0, PickingMesh 0x180072050
- achievement: Goal 0x180001d70, GoalInstance 0x1800020c0, Achievement 0x180001680
- engine: LoaderLoop 0x18004d4f0 (braced initialisers, the public (next, delay) ctor)
- io: BinaryReader 0x180023a10 (final + braced: the two trailer reads are the observable, no order),
  Visitor 0x18002b710 (final + braced, `= default` dtor: runs and frees, nothing to order),
  VisitorStream 0x18002bf80, BinaryFileStream 0x180023d80, TextFileStream 0x18002ba90 (braced),
  MemoryStream 0x1800242a0, CompressedFileStream 0x180024100, PackageFile 0x1800259d0,
  CompressedMemoryStream 0x1800244a0 (on the `ArrLog<T>` stand-in whose destructor logs `this`)
- graphics: AmbientLight 0x180059080, Particle 0x180020160, ParticleSystem 0x180020a70,
  ReflectionImageBuffer 0x180022600, GeneratedImageBuffer 0x18001eee0, ParticleSystemInstance 0x180020ce0,
  ParticleInstance 0x180020310, EdgeMeshInstance 0x1800175d0, Geometry 0x180015e90, AnimationEvent
  0x180011800, AnimationGroup 0x1800167f0, Material 0x180011d20, GraphicsMesh 0x180012bd0
- video: VideoProgram 0x18004efb0, VertexLayout 0x18004e000, RenderState 0x18004b9d0, VertexProgram
  0x18004e810, PixelProgram 0x18004eb90
- ui: BorderElement 0x1800414f0, BorderDescription 0x180040ff0, ClipRegionElement 0x180038370,
  ClipRegionDescription 0x180037eb0, ButtonElement 0x1800485b0, ButtonDescription 0x1800481a0,
  ImageDescription 0x1800361f0, ImageElement 0x180036600, GroupDescription 0x180046af0, GroupElement
  0x180046e10, TextDescription 0x180041e30, TextElement 0x1800422f0, Edit/NumberEdit/Number
  Description, EditElement, NumberElement (no address in the file), ComboDialog 0x180074a10,
  ToolTipDialog 0x18004d270, Dialog 0x180038b20, DialogController 0x18003b800, DialogControllerConfig
  0x18003b600, CharacterSet 0x1800378a0, Cursor 0x18003b3a0, StringTable 0x180002740, Font 0x180032130,
  Element 0x180035c10 and ElementDescription 0x180035350 (rb_ui's flags-2 no-free arm), and the 26
  ui_leaf_c4/c5 classes through `B39CheckDestructorArms` (no per-class address in those files)
- retired with no counterpart: none. Kept (exempt): the `b39_vid_data_test.cpp` `*Data` arm.

## `DeletingDtor` left in tests/ (all on purpose)
- `object_vtable_abi_test.cpp` `FakeRetailShape`: the ONE control (the retired one-slot MSVC shape).
- `video_data_abi_test.cpp` `MirrorDeviceData`, `MirrorIndexBufferData`, `MirrorVertexLayoutData`;
  `b39_vid_data_test.cpp` (the twelve `Video::*Data`): exempt by decision X1.
- `slot_check.h` `HasDeletingDtor` / `DtorSlotOk` and `slot_check_test.cpp` `WithDeletingDtor`: the helper
  still serves the exempt `*Data` shape, and its test is a stand-in of that shape.
- comments naming the retired member (b39_sys_thread, video_data, object_vtable).

## Choices the rule sheet did not make
- Test NAMES kept even where they name the retired rule (`…_deletingdtor_…`, `…_has_no_virtual_destructor`,
  `a_virtual_destructor_would_shift_…`): each body states the new fact and says so.
- Constants renamed so the retired word leaves live code: `kDeletingDtor` → `kDtorSlot` / `kRetailDtor`,
  `kSlot0_DeletingDtor` → `kSlot0_Destructor`, `B39CheckDeletingDtorArms` → `B39CheckDestructorArms`.
- `final` dropped from three TEST stand-ins (`GoalLeaf`, `B39DcLeaf`, `B39CfgLeaf`) so `Recording<T>` can
  derive; a final PRODUCT class (`BinaryReader`, `Visitor`) keeps `HasClassDelete` + the real `delete`, and
  the recorded free is proven on io_vtable's `OrderProbe` (a ReaderWriter subclass).
- Braced `new T[n]{…}` for classes with no default constructor — the real public constructor, nothing
  invented.
