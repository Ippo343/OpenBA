# B3X W0 audit — did the seven W0 blocks miss a slot-index read?

Written 2026-09-28 by the W0 audit agent on branch `b3x-AUDIT`, cut from `main` @ fbf8f6dc (all seven W0
merges in). Scope: PRODUCT code only — `code/`, `runtime/`, `standalone/`, `engine/`, `dx11/`
(`tests/` is W1's). Rules: `../B3X_RULES.md` X1 / X3 / §3.

## 1. Result

**Five sites were still reading a FLIPPING class's table by index (or by byte offset). All five are fixed
on this branch; none changes behaviour.** Nothing else in product code indexes a flipping table. Every
by-name delete that goes through a cast names the object's own hierarchy. The spot check of the W0
conversions found no member whose compiler slot differs from the slot the old raw code read.

| bucket | sites | stays? |
|---|---:|---|
| MISSED (flipping class, index / byte offset) | **5** — all fixed (§2) | converted |
| EXEMPT class (X1: IO::Encoding, Controller, Thread, IPathInfo, COM, the twelve `Video::*Data` incl. dx11 seam + Dx9Placeholder) | 397 | yes |
| NOT A VTABLE (IAT / dispatch slot, data table, array or field load, string, vertex buffer, CRC table) | 533 | yes |
| BY NAME already (a member *named* `SlotN`, by-name helpers such as `DestroyReaderWriter`, `VCall_Element_*`, `CallGetTypeId`) | 140 | yes |
| SLOT FROM THE MEMBER (`Object::CompilerSlotOf` / `SlotAddressOf`) | 25 | yes |
| whole-vptr identity / raw dump / vptr loaded only to print or null-test | 19 | yes |
| dead slot constant / typedef on a flipping class | 1 namespace, test-only users — LEFT (§5) | yes |
| **total sites examined** (union, de-duplicated by `file:line`) | **1,114** | |

Sources of the 1,114 (a site can come from several): `partc.py --list` for the eleven pattern names in the
brief, non-tests lines = 355 sites; independent sweep S1 (`scan.py`: `R(**)(`, `void***`-style casts,
`->vtable`/`->vptr` POD fields, any `[..]` / `+ k` on a `vt`/`vtbl`/`vtable`/`fns`/`methods`/`tbl` name,
integer-vptr `*(uintptr_t*)p + 0x..`, helper-returned tables, suspicious `*Slot*` helpers, `mem*` on
vtables, slot compares) = 867; independent sweep S2 (`scan2.py`: every statement that loads a pointer out of
`*obj` (+ offset / `[i]`) AND calls it through a function-pointer type — 2,247 fn typedef names collected
repo-wide — plus every local loaded from `*obj` and later indexed / offset within 80 lines) = 202. Extra
hand greps: `static_cast<uint8_t*>(*…) + off`, `(*reinterpret_cast<Fn* const*>(x))[N]`, `(**(Fn**)x)(…)`,
`*reinterpret_cast<Fn*>(vt)` (slot 0, no index), `(***)`, `_vftable`/`__vptr`/`_vfptr`, POD
`struct …Vtbl`, `#define` bodies, `VirtualProtect` (none), `memcpy` of a table (none). The scratch scripts
are in `scratchpad/b3x/AUDIT/`.

After this branch the `partc.py` slot-index patterns only fell: `raw vt[N>=1]` 83 → 82, the rest unchanged
(the other four fixed reads were in shapes no partc pattern sees); `->DeletingDtor(1)` rose 149 → 150 by
the converted `DeleteImageBuffer` (W1 turns it into `delete`).

## 2. MISSED sites (all fixed on `b3x-AUDIT`)

| # | site (before) | class (flips) | read | now |
|---|---|---|---|---|
| 1 | `runtime/src/object/external.cpp:407` `RtObject_ExternalFactory_FindNext` | `Object::External` (root `Object::Base`) | `*(GetTypeIdFn*)((uint8_t*)candidate->_vftable + 0x10)` = slot 2 `Type()` | `reinterpret_cast<const ::Object::Base*>(candidate)->Type()` — the bridge `mainloop_video.cpp:380` already uses for the same walk |
| 2 | `runtime/src/graphics/sunlight.cpp:197` `ImageBufferCreate` | `Video::ImageBuffer` (root `Object::Base`) | `*(ImageCreateFn*)((uint8_t*)*(void**)ib + 0x48)` = slot 9 `Create` | `reinterpret_cast<Video::ImageBuffer*>(ib)->Create(type, usage, format, flags, w, h, filter, addrU, addrV, addrW, (LevelArray*)data)` — eleven arguments, same order and types as `ImageBuffer.h:310` (all five enums are `int32_t` aliases); the dead `ImageCreateFn` typedef went with it |
| 3 | `runtime/src/graphics/sunlight.cpp:272` `DeleteImageBuffer` | `Video::ImageBuffer` | `(*(DeletingDtorFn*)*(void**)ib)(ib, 1)` = slot 0 DELETE | `reinterpret_cast<Video::ImageBuffer*>(ib)->DeletingDtor(1)` (own hierarchy); the dead `DeletingDtorFn` typedef deleted |
| 4 | `runtime/src/graphics/wave28_lane_gfxbody.cpp:1174` `Animation::ApplyEvents` | `Graphics::AnimationEvent` (root `Object::Base`; the live objects are the exe's `AnimAudioEvent`) | `(*(EventFireFn* const*)e)[8](e, transform)` | `((EventFireFn)Object::SlotAddressOf(e, &Graphics::AnimationEvent::Construct))(e, transform)` — see below |
| 5 | `runtime/src/ui/dialog.cpp:1646` `DialogInstance` hit test | `UI::Element` (root `Object::Base`) | `((Fn)vt[9])(root, x, y, _dialog)` | `((Fn)Object::SlotAddressOf(root, &UI::Element::Contains))(root, x, y, _dialog)` — see below |

Sites 1–3 were in no inventory (INVENTORY_B1 has no row for `sunlight.cpp`, `wave28_lane_gfxbody.cpp` or
`object/external.cpp`), so no W0 block owned them. Their shapes: a `static_cast` (not `reinterpret_cast`)
byte offset on `*(void**)obj`, a POD field named `_vftable`, and a typed `Fn* const*` table indexed with
`[8]` — none matched by partc or by the inventories' regexes.

**Sites 4 and 5 keep a hand-typed call, with the slot taken from the member.** Both have an argument list
that disagrees with the header, so a by-name call is not possible without a header change that is not this
audit's to make:
- #4: slot 8 is declared `virtual Object::External* Construct() = 0` (`Animation.h`), but retail's
  `CALL [RAX+0x40]` passes RDX = the transform (0x1800fa9a0, read off the instruction — banner in
  `wave28_lane_gfxbody.cpp`), and the exe body 0x140087e40 is the audio trigger that reads it
  (`AnimAudioEvent.h`). The header's empty list is pinned by test overrides
  (`graphics_animation_abi_test.cpp:111/270`, `rb_graphics_abi_test.cpp`), which this audit may not edit.
  **For W1 (with the tests):** give slot 8 its retail signature (e.g. `virtual void Fire(const
  Math::Transform&) = 0`, `AnimAudioEvent` overrides it) and make the call by name.
- #5: R-UI LEFT this read on purpose (its comment: retail passes `_ui` (+0x10), 0x180113bb3
  `MOV R9,[RBX+0x10]`, the tree passes `_dialog` (+0x18)). It was still a literal `vt[9]` that the flip would
  have turned into a call of `Enable`'s neighbour. The divergence is untouched (a behaviour change is its
  own titled commit); only the index is now member-derived. **Open:** fix the 4th argument to `_ui` in a
  titled behaviour-change commit, then call `Contains` by name.

`Object::SlotAddressOf` resolves to the compiler's slot of the named member (`CompilerSlotOf` = 8 / 9
today, verified with the probe in §4), so both calls survive the flip unchanged.

## 3. By-name deletes and the wrong-root question

All 497 `DeletingDtor(0|0u|1|1u)` / `Object::Destroy(` lines in product code were listed
(`deletes.py`). 439 have a typed receiver (compiler-checked) or go through a typed helper (`AsRw(…)` ×15,
`Rw(…)` ×3 — both return `IO::ReaderWriter*`). 58 cast the receiver; every cast target was checked against
the object's real class:

| cast target | root | sites |
|---|---|---|
| `IO::ReaderWriter` | ReaderWriter | 10 (encoding factory results) |
| `Video::ImageBuffer`, `Graphics::FrameCopyImageBuffer` | Object::Base | 10 + 2 |
| `Graphics::MaterialInstance`, `UI::SpriteSheet`, `Graphics::EdgeMesh`, `::Object::External`, `::IO::PackageFile`, `UI::DialogInstance`, `UI::Element`/`Element`, `Object::Internal` | Object::Base | 3, 2, 2, 2, 1, 1, 4, 1 |
| `Graphics::Geometry`, `Graphics::MeshInstance`, `Graphics::SunLight`, `WorldGeometry` | Geometry | 3, 3, 1, 2 (weather geometry, held item / placement meshes, sky mesh, sun) |
| `IO::FileStream::DeletingDtor(0)`, `IO::MemoryStream::DeletingDtor(0)` (qualified, stack / embedded) | Stream | 2, 2 |
| `Video::DeviceData`, `ContextData`, `ConstantBufferData` | own (exempt) | 5, 1, 1 |

**Wrong-root casts: 0.** The one the inventory found (`menudialogloopt_stubs.cpp:447`, Geometry objects
deleted as `Object::Base`) is already `static_cast<Graphics::Geometry*>(o)->DeletingDtor(1)` (:451, R-REST).
No `reinterpret_cast<Object::Base|Internal|External*>` in product code feeds a delete or a member call on a
Geometry / Stream / ReaderWriter / UpdateLoop / `*Data` object (searched all such casts; the rest are
`static_cast` upcasts the compiler checks, or `Type()` / `GetPath()` on real Base-family objects).

## 4. Spot check of the W0 by-name conversions

Method: every hunk of the six W0 merge diffs (`git diff M^1 M` for c445c813 R-GFX, e7890cb1 R-UI,
d85f0fb7 R-IO, 3d380b91 R-REST, bc10e6df G-A, 76c634c1 G-B) was scanned for a removed raw read (literal
`[N]`, `+ 0xNN`, `k… / 8`) paired with an added bridge call `cast<Class*>(p)->Member(`; the member's
COMPILER slot was then taken from GCC itself — a probe TU declaring `const auto pmf = &Class::Member;`
for every pair, compiled with the project's include flags to assembly, the Itanium PMF word read back
(slot = (ptr − 1) / 8). Pairs where the extractor mis-read the old offset were settled by hand.

- **157 converted call sites** across code/Game/Component 28, Action 26, Work 24, Dialog 11, Game 11,
  Player 10, Entity 1, Tutorial 1; runtime/src/ui 14, graphics 7, video 5, io 4, object 1, save 1,
  audio 1, engine 1; standalone 3; engine/funcs 2 — 50 distinct `Class::Member`s. **157 of 157 agree**
  (151 automatically; 6 by hand: `citizenai_housing_idle.cpp:505` old `vt[0x20/8]` = `Work::Execute` 4;
  four `SerializeFloat` from `rw_vtbl::kScalarFloat` = 0x80 = slot 16; `dialog_create.cpp:423` old `vt[8]` =
  `ElementDescription::CreateInstance` `[8]`).
- **Plus 45 member-table checks** of the by-name surfaces the R-IO / R-REST conversions go through: all 25
  `IO::ReaderWriter` members against the retired `rw_vtbl::k*` constants (0x08 … 0xC0), `IO::Stream`
  Write/Read/SetCurrentPos/GetCurrentPos/GetSize (2–6), `MemoryStream::Close` 7, `CompressedStream::PreCommit`
  8, `External` GetFileVersion/Serialize/Reconstruct/Destruct (3/5/6/7), `Base::TypeName` 1,
  `UpdateLoop::SaveRestore` 7, `Geometry::Draw` 2 / `Clean` 8, `DialogController` OnOpened 5 / OnClosed 6 /
  OnSerializeSettings 10, and this branch's `AnimationEvent::Construct` 8, `ImageBuffer::Create` 9,
  `Element::Contains` 9. **45 of 45 agree.**
- The two swapped-name traps of B3X_RULES X3 were followed by OFFSET: `external_load.cpp` `SerU32` (+0xa0)
  now calls `SerializeInt`, `SerI32` (+0x98) `SerializeUInt`; `stringtable_factory.cpp:184` +0xa0 →
  `SerializeInt`.

**Mismatches: 0.**

## 5. Dead slot constants / typedefs

- `runtime/types/io/Serializer.h:180-207` `namespace rw_vtbl` (IO::ReaderWriter — flips): no product user
  besides its own three `static_assert`s (:538-540); **45 test lines use it** → LEFT for W1 (tests own it).
- `engine/types/io_serializer.h:205` `namespace factory_vtbl` has zero users anywhere, but it describes
  `IO::Encoding` (exempt), so it is not a flip hazard; left (not this audit's rule to delete).
- Every other `k…Slot…` / `…_vtbl` / `k…Vtbl…` constant still in product code names an exempt table
  (`encoding_vtbl`, `devicedata_vtbl`/`programdata_vtbl`, `kDeviceDataCreate*`, `kVideoBufferSerialize`,
  `kFactoryMake*Encoding`, `kEnc*`, `kVoice*`/`kCom*` COM offsets, dx11 `kSlot_*` / `kSlots::*`) or is not a
  vtable slot at all (buffer / array / hash-table sizes).

## 6. Shapes `partc.py` still misses (for the coordinator)

1. `static_cast` byte offset on the vptr: `static_cast<uint8_t*>(*reinterpret_cast<void**>(x)) + 0x48` —
   `(?:static_cast|reinterpret_cast)\s*<\s*(?:const\s+)?(?:uint8_t|char)\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*(?:const\s+)?void\s*\*\s*(?:const\s*)?\*\s*>\s*\([^)]*\)\s*\)\s*\+`
   (today: 4 hits, all exempt — `sunlight.cpp:178` DeviceData, `buffers.cpp:282` DeviceData,
   `saveload_draw.cpp:806` ImageBufferData, + the `serializer + 8` data reads).
2. A POD vptr field spelled `_vftable`: `\b_vftable\b` (0 code users left).
3. A typed function-pointer table indexed: `\(\s*\*\s*reinterpret_cast\s*<\s*[\w:]+\s*\*\s*(?:const\s*)?\*\s*>\s*\([^)]*\)\s*\)\s*\[`
4. Slot 0 with no index through a typed pointer: `\*\s*reinterpret_cast\s*<\s*\w+\s*\*\s*>\s*\(\s*\*\s*reinterpret_cast\s*<\s*void\s*\*\*\s*>\s*\(\w+\)\s*\)`
   (today: `buffers.cpp:266`, ContextData companion — exempt).

The statement-level S2 sweep (`scan2.py`: fn-pointer-typed call of anything loaded from `*obj`) is the
shape-independent net that found MISSED #2 and #4 (S1's `vft\w*` name found #1; #3 sat next to #2; #5 was
R-UI's known LEFT row); it could replace partc's regex zoo for the close.

## 7. Builds (this branch)

`/c/msys64/ucrt64/bin/mingw32-make.exe -j3 standalone-debug` exit 0; `-j3 standalone-perf` exit 0;
`-j3 unit` exit 0, **916 tests, 0 failures**. No warning on any line this branch touched (the perf build's
`wave28_lane_gfxbody.cpp:97` unused-parameter `what` is pre-existing - the perf build strips the printf that
reads it; the line moved by one because of the added include).

## 8. The per-site list (acceptance evidence)

Every examined site, grouped by file, one line each: `:line` **bucket** reason — code.
Buckets: **EX** exempt class (X1) · **NV** not a vtable · **BN** already by name / a member named SlotN ·
**SM** slot taken from the member (`CompilerSlotOf` / `SlotAddressOf`) · **ID** whole-vptr identity, raw
dump, or vptr loaded only to print / null-test.

#### `code/Game/Action/action_base_cnr.cpp`
- :109 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `static inline void* RSlot(uintptr_t slotAddr) {`
- :140 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_fn_Vec4 = reinterpret_cast<fn_Vec4_t>(RSlot(0x1401529`
- :141 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_fn_Time = reinterpret_cast<fn_Time_t>(RSlot(0x140152b`
- :549 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t cit28 = *reinterpret_cast<int64_t*>(*reinterpre`
- :562 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (p) *reinterpret_cast<uint32_t*>(*reinterpret_cast<i`
- :566 **NV** array / pointer FIELD load at a data offset (no vptr) — `if (s_fn_c48a0) s_fn_c48a0(*reinterpret_cast<void***>(g`
- :616 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t v = p ? *reinterpret_cast<uint32_t*>(*reinterp`

#### `code/Game/Action/action_base_complete.cpp`
- :479 **NV** array / pointer FIELD load at a data offset (no vptr) — `meshPtr = **reinterpret_cast<void***>(reinterpret_cast<`

#### `code/Game/Action/action_base_move.cpp`
- :118 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (enc) encType = (*reinterpret_cast<int(**)(void*)>(*`

#### `code/Game/Action/action_pf_ctors.cpp`
- :102 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<char*>(obj + 0x18) != '\0') {`
- :107 **NV** IBP / owner pointer deref then a FIELD read (no call) — `lVar1 = *reinterpret_cast<int64_t*>(*reinterpret_cast<i`
- :283 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>`

#### `code/Game/Action/action_pf_geom.cpp`
- :51 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_memAlloc = reinterpret_cast<fn_MemAllocStatic_t>(slot`
- :54 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_memFree = reinterpret_cast<fn_MemFreeStatic2_t>(slot(`

#### `code/Game/Action/action_pf_roots_drop.cpp`
- :268 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** pplVar3 = *reinterpret_cast<void***>(reinterpret`
- :344 **NV** IBP / owner pointer deref then a FIELD read (no call) — `local_78 = *reinterpret_cast<int64_t*>(*reinterpret_cas`
- :351 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(lVar3 + 0x58) != 0) {`
- :361 **NV** IBP / owner pointer deref then a FIELD read (no call) — `local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret`
- :373 **NV** IBP / owner pointer deref then a FIELD read (no call) — `local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret`
- :381 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterp`
- :402 **NV** IBP / owner pointer deref then a FIELD read (no call) — `local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret`
- :426 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t uVar4 = *reinterpret_cast<uint64_t*>(*reinterp`
- :459 **NV** IBP / owner pointer deref then a FIELD read (no call) — `local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret`

#### `code/Game/Action/action_pf_roots_misc.cpp`
- :103 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_boxisect = reinterpret_cast<fn_BoxIntersect_t>(slot(0`

#### `code/Game/Action/createrawmaterial.cpp`
- :439 **NV** array / pointer FIELD load at a data offset (no vptr) — `s_fn_RegisterEntity(*(void***)(gameWorld + 0xe8), &enti`

#### `code/Game/Action/moveto.cpp`
- :141 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `if (iat) s_fn_IsDerivedFrom = *reinterpret_cast<bool(**`

#### `code/Game/Action/removenaturalresource.cpp`
- :259 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* parentEntity = *(void**)(*(uint8_t**)(s + 0x20) +`

#### `code/Game/Action/sendcomponentmessage.cpp`
- :44 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `if (slotTypeSer) s_fn_Type_Serialize = *reinterpret_cas`

#### `code/Game/Action/showtool.cpp`
- :216 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** heldVt = *(void***)(e + 0xb8);`
- :250 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** meshVt = *(void***)(e + 0xb8);`

#### `code/Game/Component/AnimalComponent.h`
- :39 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/BuildComponent.h`
- :50 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override {}`

#### `code/Game/Component/CitizenComponent.h`
- :53 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/Component.h`
- :53 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `virtual void Slot5();`
- :59 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `virtual void Slot11();`

#### `code/Game/Component/DecalComponent.h`
- :48 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/FloorComponent.h`
- :38 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/GrowthComponent.h`
- :42 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/ParticleComponent.h`
- :45 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/PickingComponent.h`
- :39 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/PopulationComponent.h`
- :43 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/StateMachineComponent.h`
- :77 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/StatusIconComponent.h`
- :80 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/StorageDisplayComponent.h`
- :42 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot11() override;`

#### `code/Game/Component/ambient_externalptr_serialize.cpp`
- :42 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** vt = *reinterpret_cast<void***>(enc);`
- :43 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `return reinterpret_cast<int(*)(void*)>(vt[2])(enc);`

#### `code/Game/Component/ambientenvironmentcomponent.cpp`
- :129 **BN** emitter sound slot setter, no vtable — `void SetSlot(void* comp, int slot, uint64_t soundRef, f`
- :196 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** list = *reinterpret_cast<void***>(reinterpret_ca`
- :430 **BN** emitter sound slot setter, no vtable — `SetSlot(emitter, 0, daySound, f);`
- :431 **BN** emitter sound slot setter, no vtable — `SetSlot(emitter, 1, nightSound, unit - f);`

#### `code/Game/Component/animalcomponent.cpp`
- :870 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void AnimalComponent::Slot11() {}`

#### `code/Game/Component/animaldescription.cpp`
- :33 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :34 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/boatcomponent_route.cpp`
- :86 **NV** IAT slot helper (read by hand) — `template <typename T> T SlotFn(uintptr_t slotAddr) {`
- :87 **NV** IAT slot helper (read by hand) — `T SlotFn(uintptr_t slotAddr) {`
- :144 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const int n = table ? *reinterpret_cast<int*>(table + 0`
- :146 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const uintptr_t entry = *reinterpret_cast<uintptr_t*>(t`
- :191 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const uintptr_t entry = *reinterpret_cast<uintptr_t*>(t`
- :401 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `Release_t release = *reinterpret_cast<Release_t*>(*rein`

#### `code/Game/Component/boatcomponent_states.cpp`
- :142 **NV** IAT slot helper — `inline void* Slot(uintptr_t addr) {`
- :166 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3Ctor3f = reinterpret_cast<fn_V3Ctor3f_t>(Slot(0x140`
- :167 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3CtorS = reinterpret_cast<fn_V3CtorS_t>(Slot(0x14015`
- :168 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3Length = reinterpret_cast<fn_V3Length_t>(Slot(0x140`
- :169 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3Dot = reinterpret_cast<fn_V3Dot_t>(Slot(0x140151298`
- :170 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3Normalize = reinterpret_cast<fn_V3Normalize_t>(Slot`
- :171 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3NormalizeLen = reinterpret_cast<fn_V3NormalizeLen_t`
- :172 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_V3Heading = reinterpret_cast<fn_V3Heading_t>(Slot(0x1`
- :173 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_TimeCtor = reinterpret_cast<fn_TimeCtor_t>(Slot(0x140`
- :174 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_XformFromYPO = reinterpret_cast<fn_XformFromYPO_t>(Sl`
- :175 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_XformAssign = reinterpret_cast<fn_XformAssign_t>(Slot`
- :273 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = *reinterpret_cast<void***>(node);`
- :274 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `if (!vt // !vt[2]) return;`
- :277 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `reinterpret_cast<PostEvent_t>(vt[2])(node, sm, curIdx, `

#### `code/Game/Component/buildcomponent.cpp`
- :484 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr1 = *(void***)(s + 0x58);`
- :512 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr2 = *(void***)(s + 0x68);`

#### `code/Game/Component/buildcomponent_state1.cpp`
- :468 **NV** FIELD read/write through an object pointer (no call) — `if (builderMarker != 0) *reinterpret_cast<uint64_t*>(re`

#### `code/Game/Component/charactercomponent.cpp`
- :76 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*`
- :83 **NV** IBP / owner pointer deref then a FIELD read (no call) — `node = *reinterpret_cast<int64_t*>(*reinterpret_cast<in`
- :356 **NV** array FIELD load (no vptr) — `void** objArray = *reinterpret_cast<void***>(&data48);`

#### `code/Game/Component/charmanager.cpp`
- :81 **NV** IBP / owner pointer deref then a FIELD read (no call) — `float r = *reinterpret_cast<float*>( *reinterpret_cast<`

#### `code/Game/Component/citizen_assignworkplace.cpp`
- :102 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `inline void* RSlot(uintptr_t slotAddr) {`
- :123 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_slot_Vec3Eq = reinterpret_cast<fn_Vec3Eq_t>(RSlot(0x1`

#### `code/Game/Component/citizen_lifeevent_handler.cpp`
- :116 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** comps = *reinterpret_cast<void***>(entity + 0x30`
- :312 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int rot = (resIBPp && *reinterpret_cast<int64_t*>(resIB`

#### `code/Game/Component/citizen_reassign_workers.cpp`
- :164 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* ownerEntity = *reinterpret_cast<void**>( *reinter`

#### `code/Game/Component/citizen_updatehealth.cpp`
- :94 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `static inline void* RSlot(uintptr_t slotAddr) {`
- :109 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_boxIntersect = reinterpret_cast<fn_BoxIntersect_t>(RS`
- :321 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(int64_t*)(citizen + 0x68) != 0) return 1;`
- :672 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entity = *(uintptr_t*)(*(uintptr_t*)slot + 0x`

#### `code/Game/Component/citizenai_actionctors.cpp`
- :204 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :222 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t entityVal = *reinterpret_cast<int64_t*>(*reinte`
- :420 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :484 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :529 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :585 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :657 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :669 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :752 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :794 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :841 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :868 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`

#### `code/Game/Component/citizenai_candidatelist.cpp`
- :403 **NV** FIELD read through an object / IBP pointer (no call) — `if (cz != 0 && *reinterpret_cast<int64_t*>(cz + 0x68) !`
- :405 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x228) != 0) {`
- :446 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {`
- :454 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(cz + 0x1e4) == 0) AppendCan`
- :461 **NV** FIELD read through an object / IBP pointer (no call) — `if ((work50 <= fv && fv != work50) && ((*reinterpret_ca`
- :512 **NV** FIELD read through an object / IBP pointer (no call) — `if ((age < ageThresh) // (*reinterpret_cast<int64_t*>(c`
- :515 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x208) == 0) {`
- :518 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {`
- :532 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64`
- :539 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x238) != 0) {`
- :545 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {`
- :546 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t resv = *reinterpret_cast<int64_t*>( *reinterpre`
- :565 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {`
- :566 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t resv = *reinterpret_cast<int64_t*>( *reinterpre`
- :583 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {`

#### `code/Game/Component/citizenai_gather.cpp`
- :239 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(mi + 0xf0) == 0) {`
- :253 **NV** FIELD read through an object / IBP pointer (no call) — `if (settings && reinterpret_cast<char*>(settings)[0xc] `
- :269 **NV** FIELD read through an object / IBP pointer (no call) — `if ((changed & 0xffff) != 0 && *reinterpret_cast<int64_`

#### `code/Game/Component/citizenai_handlers.cpp`
- :123 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :145 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*rein`
- :166 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t entityVal = *reinterpret_cast<int64_t*>(*reinte`

#### `code/Game/Component/citizenai_handlers2.cpp`
- :295 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(mi + 0xf0) == 0) {`
- :309 **NV** FIELD read through an object / IBP pointer (no call) — `if (settings && reinterpret_cast<char*>(settings)[0xc] `
- :326 **NV** FIELD read through an object / IBP pointer (no call) — `if ((changed & 0xffff) != 0 && *reinterpret_cast<int64_`

#### `code/Game/Component/citizenai_housing_idle.cpp`
- :272 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int sv = *reinterpret_cast<int*>(*reinterpret_cast<int6`
- :278 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int sv2 = *reinterpret_cast<int*>(*reinterpret_cast<int`
- :418 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int sv = *reinterpret_cast<int*>(*reinterpret_cast<int6`
- :424 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int sv2 = *reinterpret_cast<int*>(*reinterpret_cast<int`
- :505 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(payload + 0x2c) != *reinter`
- :515 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(cur`
- :596 **NV** FIELD read through an object / IBP pointer (no call) — `if (local_res20 == *reinterpret_cast<int64_t*>(citizen `
- :658 **NV** IBP / owner pointer deref then a FIELD read (no call) — `bool homeless = (*reinterpret_cast<int64_t*>(*reinterpr`
- :728 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t ownerKey = *reinterpret_cast<uint64_t*>(*reint`

#### `code/Game/Component/citizenaicomponent.cpp`
- :591 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t res10 = *reinterpret_cast<int64_t*>(*reinterpre`
- :598 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t profList = *reinterpret_cast<int64_t*>(*reinter`
- :622 **NV** FIELD read through an object / IBP pointer (no call) — `if (ageThresh <= age && *reinterpret_cast<int64_t*>(d +`
- :626 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64`

#### `code/Game/Component/citizencomponent.cpp`
- :620 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(healthCompVal + 0x60) + 1 <`
- :726 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(partnerComp + 0x238) !=`
- :753 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (isChildOrStudent // (*reinterpret_cast<int*>(*reint`
- :889 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*reinterpret_cast<int64_t*>( *reinterpret_cast<int64_t*`
- :1293 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(int32_t*)(slc + 0x30) == 0) {`
- :2157 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>`
- :2555 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (st && *reinterpret_cast<int*>(*reinterpret_cast<int`

#### `code/Game/Component/citizencomponent_lifecycle.cpp`
- :348 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void CitizenComponent::Slot11() {`

#### `code/Game/Component/clearareacomponent.cpp`
- :187 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** comps = *reinterpret_cast<void***>(e + 0x30);`

#### `code/Game/Component/component.cpp`
- :86 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Component::Slot5() {}`
- :92 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Component::Slot11() {}`

#### `code/Game/Component/componentdescription.cpp`
- :38 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :39 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :68 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `EncFn getState = reinterpret_cast<EncFn>( *reinterpret_`
- :69 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*r`

#### `code/Game/Component/consumeproducecomponent.cpp`
- :227 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(descRoot + 0x28);`

#### `code/Game/Component/consumeproducedescription.cpp`
- :59 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :60 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/controller_state_table.h`
- :303 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `RT_t rt = *reinterpret_cast<RT_t*>(reinterpret_cast<uin`

#### `code/Game/Component/cropfieldcomponent.cpp`
- :462 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** nvt = *reinterpret_cast<void***>(node);`
- :463 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `if (!nvt // !nvt[2]) return;`
- :466 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `reinterpret_cast<char (*)(void*, void*, int, int)>(nvt[`

#### `code/Game/Component/cropfieldcomponent_states.cpp`
- :188 **BN** crop-field plot array helpers (data) — `uint8_t* SlotAt(uintptr_t comp, int i) {`
- :192 **BN** crop-field plot array helpers (data) — `int SlotCount(uintptr_t comp) {`
- :198 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** items = *reinterpret_cast<void***>(list + 0x28);`
- :309 **BN** crop-field plot array helpers (data) — `int CountSlots(uintptr_t comp, int state) {`
- :311 **BN** crop-field plot array helpers (data) — `const int total = SlotCount(comp);`
- :313 **BN** crop-field plot array helpers (data) — `uint8_t* s = SlotAt(comp, i);`
- :324 **BN** crop-field plot array helpers (data) — `const int total = SlotCount(comp);`
- :330 **BN** crop-field plot array helpers (data) — `uint8_t* s = SlotAt(comp, i);`
- :432 **BN** crop-field plot array helpers (data) — `uint8_t* s = SlotAt(comp, row * w + col);`
- :445 **BN** crop-field plot array helpers (data) — `const int count = SlotCount(comp);`
- :485 **BN** crop-field plot array helpers (data) — `uint8_t* s = SlotAt(comp, i);`
- :507 **BN** crop-field plot array helpers (data) — `const int total = SlotCount(comp);`
- :508 **BN** crop-field plot array helpers (data) — `const int growing = CountSlots(comp, 1);`
- :509 **BN** crop-field plot array helpers (data) — `const int done = CountSlots(comp, 2);`
- :564 **BN** crop-field plot array helpers (data) — `const int total = SlotCount(comp);`
- :565 **BN** crop-field plot array helpers (data) — `if (CountSlots(comp, 2) == total) ::ControllerLive::Req`
- :611 **BN** crop-field plot array helpers (data) — `if (CountSlots(comp, 2) == SlotCount(comp)) {`

#### `code/Game/Component/decalcomponent.cpp`
- :451 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void DecalComponent::Slot11() {}`

#### `code/Game/Component/destroycomponent_state6.cpp`
- :204 **NV** Collection::Array data pointer (items / material list) — `void** items = *reinterpret_cast<void***>(st);`
- :320 **NV** Collection::Array data pointer (items / material list) — `void* const* data = *reinterpret_cast<void* const* cons`

#### `code/Game/Component/destroycomponent_states.cpp`
- :240 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vt = *reinterpret_cast<void***>(node);`

#### `code/Game/Component/diseasecomponent.cpp`
- :147 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `static inline void* RSlot(uintptr_t slotAddr) {`

#### `code/Game/Component/firecomponent_ignite.cpp`
- :443 **NV** FIELD read through an object / IBP pointer (no call) (read by hand) — `if (evt) *reinterpret_cast<uint32_t*>(evt + 0x34) = 1;`

#### `code/Game/Component/firecomponent_smstate.cpp`
- :109 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = *reinterpret_cast<void***>(n);`
- :110 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `int r = reinterpret_cast<fn_FindTrans_t>(vt[1])( n, sta`

#### `code/Game/Component/fishingcomponent_produce.cpp`
- :60 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** items = *reinterpret_cast<void***>(list + 0x28);`

#### `code/Game/Component/floorcomponent.cpp`
- :463 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void FloorComponent::Slot11() {`

#### `code/Game/Component/growthcomponent.cpp`
- :822 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void GrowthComponent::Slot11() {}`

#### `code/Game/Component/herbalistdescription.cpp`
- :32 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :33 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/herdcomponent.cpp`
- :551 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t entity = *(uint64_t*)(*(int64_t*)animalIbp + 0`

#### `code/Game/Component/immigrationcomponent_states.cpp`
- :202 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = *reinterpret_cast<void***>(node);`
- :204 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `reinterpret_cast<PostEvent_t>(vt[2])(node, reinterpret_`

#### `code/Game/Component/immigrationdescription.cpp`
- :43 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :44 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/keystone_a_restree.cpp`
- :63 **NV** IAT slot helper — `inline uintptr_t slot(uintptr_t a) {`
- :70 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x`
- :77 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x14`

#### `code/Game/Component/marketdescription.cpp`
- :33 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :34 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/multiobjectcomponent.cpp`
- :50 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** array = *(void***)((uintptr_t)self + 0x48);`

#### `code/Game/Component/particlecomponent.cpp`
- :473 **NV** string literal text (read by hand) — `"with nullResource==0 means the resource resolved but i`
- :751 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(float*)(geoObj + 0x7C) != 0.0f) {`
- :909 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void ParticleComponent::Slot11() {`

#### `code/Game/Component/pasturecomponent.cpp`
- :266 **NV** FIELD read through an object / IBP pointer (no call) — `if (((newFlags ^ oldFlags) & 0xffff) != 0 && *(int64_t*`

#### `code/Game/Component/pasturecomponent_spawn.cpp`
- :194 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t parentOwner = *reinterpret_cast<int64_t*>( *rei`

#### `code/Game/Component/pasturecomponent_states.cpp`
- :708 **NV** IBP / owner pointer deref then a FIELD read (no call) — `const uint32_t v = *reinterpret_cast<uint32_t*>( *reint`

#### `code/Game/Component/pickingcomponent.cpp`
- :310 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void PickingComponent::Slot11() {}`

#### `code/Game/Component/populationcomponent.cpp`
- :442 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `Slot11();`
- :617 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void PopulationComponent::Slot11() {}`

#### `code/Game/Component/quarrycomponent_deform.cpp`
- :254 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `if (!desc) { QuarryBail("Slot12(msg4)", "no QuarryDescr`

#### `code/Game/Component/resourceproduce_creatework.cpp`
- :353 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const arr = *reinterpret_cast<void***>(descRoot `

#### `code/Game/Component/spawndescription.cpp`
- :36 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :37 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/statemachinecomponent.cpp`
- :302 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((VFn_onstate_t)(*(uintptr_t*)(*(int64_t*)sm[0] + 0x28)`
- :750 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void StateMachineComponent::Slot11() {}`

#### `code/Game/Component/statusicon_geometry.cpp`
- :198 **NV** vertex array / buffer named vtx* — `SIVertex vtx[4];`
- :206 **NV** vertex array / buffer named vtx* — `vtx[1].x = center[0] + right[0] + up[0];`
- :207 **NV** vertex array / buffer named vtx* — `vtx[1].y = center[1] + right[1] + up[1];`
- :208 **NV** vertex array / buffer named vtx* — `vtx[1].z = center[2] + right[2] + up[2];`
- :209 **NV** vertex array / buffer named vtx* — `vtx[1].color = 0xffffffffu; vtx[1].u = u1; vtx[1].v = v`
- :211 **NV** vertex array / buffer named vtx* — `vtx[2].x = center[0] + right[0] - up[0];`
- :212 **NV** vertex array / buffer named vtx* — `vtx[2].y = center[1] + right[1] - up[1];`
- :213 **NV** vertex array / buffer named vtx* — `vtx[2].z = center[2] + right[2] - up[2];`
- :214 **NV** vertex array / buffer named vtx* — `vtx[2].color = 0xffffffffu; vtx[2].u = u1; vtx[2].v = v`
- :216 **NV** vertex array / buffer named vtx* — `vtx[3].x = center[0] - right[0] - up[0];`
- :217 **NV** vertex array / buffer named vtx* — `vtx[3].y = center[1] - right[1] - up[1];`
- :218 **NV** vertex array / buffer named vtx* — `vtx[3].z = center[2] - right[2] - up[2];`
- :219 **NV** vertex array / buffer named vtx* — `vtx[3].color = 0xffffffffu; vtx[3].u = u0; vtx[3].v = v`

#### `code/Game/Component/statusiconcomponent.cpp`
- :659 **NV** string literal text (read by hand) — `"(geom vtable +0x28 == 0, sceneId=%d) � icon refresh sk`
- :769 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void StatusIconComponent::Slot11() {}`

#### `code/Game/Component/storagedisplaycomponent.cpp`
- :672 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void StorageDisplayComponent::Slot11() {`

#### `code/Game/Component/tornadocomponent.cpp`
- :613 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t ownerEntity = *reinterpret_cast<int64_t*>(*rein`

#### `code/Game/Component/tradecomponent_dock.cpp`
- :163 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** objs = *reinterpret_cast<void***>(owner + 0x28);`
- :396 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** src = *reinterpret_cast<void***>(rec + 0x10);`

#### `code/Game/Component/tradedescription.cpp`
- :54 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `return *reinterpret_cast<int*>( reinterpret_cast<uint8_`
- :55 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&s`

#### `code/Game/Component/zonecomponent.cpp`
- :77 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t key = *reinterpret_cast<uint64_t*>( *reinterpr`

#### `code/Game/Dialog/buildrequirementui.cpp`
- :82 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vtable = *(void***)comp;`
- :83 **SM** slot taken from the member (object/VSlot.h) — `if (!SaneHeapPtr(vtable) // Object::SlotAddressOf(comp,`

#### `code/Game/Dialog/controlui.cpp`
- :89 **BN** string / trace helper named SlotN — `void TraceSlot7(void* self) {`
- :276 **BN** string / trace helper named SlotN — `controlui_sa::TraceSlot7(this);`
- :286 **BN** string / trace helper named SlotN — `if (param3 == 0x12 && param4 == 50000) controlui_sa::Tr`

#### `code/Game/Dialog/debugui.cpp`
- :75 **NV** singleton pointer chain (data) — `void** weatherListPtr = *(void***)((uintptr_t)(*g_pGame`
- :102 **NV** singleton pointer chain (data) — `void** weatherListPtr = *(void***)((uintptr_t)(*g_pGame`
- :153 **NV** singleton pointer chain (data) — `void** weatherListPtr = *(void***)((uintptr_t)(*g_pGame`
- :321 **NV** FIELD read through an object / IBP pointer (no call) — `bool posEqual = zeroPos == *reinterpret_cast<const Math`
- :401 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(e + 0x28);`
- :1050 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** citizenList = *(void***)((uintptr_t)game + 0xF0)`
- :1124 **NV** FIELD read through an object / IBP pointer (no call) — `bool checked = *(uint8_t*)((uintptr_t)element + 0xD8) !`
- :1441 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** weatherListPtr = *(void***)((uintptr_t)gameWorld`
- :1493 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** weatherLive = *(void***)((uintptr_t)gameWorld + `

#### `code/Game/Dialog/destroyui.cpp`
- :134 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** smVtable = *(void***)smPtr;`
- :137 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (`
- :153 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** smVtable = *(void***)smPtr;`
- :155 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (`
- :172 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** smVtable = *(void***)smPtr;`
- :174 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (`
- :188 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** smVtable = *(void***)smPtr;`
- :190 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (`

#### `code/Game/Dialog/entityui.cpp`
- :80 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)((uintptr_t)entityDesc + 0`
- :381 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(uint8_t*)((uintptr_t)toolPtr + 0x40) == 0 && *(ui`
- :532 **NV** singleton pointer chain (data) — `void* keyMap = ***(void****)g_pInputConfigSingletonExre`
- :765 **NV** FIELD read through an object / IBP pointer (no call) — `if (pinState == 0 && *(char*)((uintptr_t)toolPtr + 0x41`

#### `code/Game/Dialog/eventnotifydialog.cpp`
- :554 **NV** singleton pointer chain (data) — `uint64_t soundEffect = *(uint64_t*)(*(uintptr_t*)g_pKey`

#### `code/Game/Dialog/followdialog.cpp`
- :337 **NV** singleton pointer chain (data) — `void* keyMap = g_pInputConfigSingletonExref ? ***(void*`

#### `code/Game/Dialog/immigrationui.cpp`
- :262 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** smVtable = *(void***)smObj;`
- :267 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_RequestAction_t)smVtable[2])(smObj, (void*)(bsPtr `

#### `code/Game/Dialog/moddetailsdialog.cpp`
- :221 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** conflictArray = *(void***)(m + 0x70);`
- :282 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** cArray = *(void***)(m + 0x70);`

#### `code/Game/Dialog/moddialog.cpp`
- :249 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uint32_t*)(*(uintptr_t*)elemBuf + 0xC0) = 8;`
- :338 **NV** vertex array / buffer named vtx* — `uint8_t vtChildBuf[8] = {};`

#### `code/Game/Dialog/newgamedialog.cpp`
- :108 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ptrArray = *(void***)((uintptr_t)externalList + `
- :220 **NV** array / pointer FIELD load at a data offset (no vptr) — `int maxVal = *(int*)((uintptr_t)*(void**)editSeedPtr + `
- :280 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ptrArray = *(void***)((uintptr_t)self->mapGenera`
- :341 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ptrArray = *(void***)((uintptr_t)list + 0x20);`
- :629 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** mapPtrArray = *(void***)((uintptr_t)self->mapGen`
- :673 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** climatePtrArray = *(void***)((uintptr_t)self->cl`
- :679 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** terrainPtrArray = *(void***)((uintptr_t)self->te`

#### `code/Game/Dialog/orchardui.cpp`
- :296 **BN** string / trace helper named SlotN — `orchardui_sa::Trace("Slot9(event)", this);`

#### `code/Game/Dialog/pastureui.cpp`
- :221 **NV** FIELD read through an object / IBP pointer (no call) — `bool enableCombo = (itemCount > 0) && (*(int32_t*)(comp`
- :336 **BN** string / trace helper named SlotN — `pastureui_sa::Trace("Slot9(event)", this);`

#### `code/Game/Dialog/professionui.cpp`
- :368 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** factory = *(void***)((uintptr_t)firstChild + 0xB`

#### `code/Game/Dialog/statusbarui.cpp`
- :93 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(uintptr_t*)(gw + 0x128) == 0) return false;`
- :94 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(uintptr_t*)(gw + 0x130) == 0) return false;`
- :336 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(entity + 0x30);`
- :625 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(compEntity + 0x30);`

#### `code/Game/Dialog/toolbardialog.cpp`
- :245 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** buttonArray = *(void***)(s + 0xC0);`
- :252 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** buttonArray = *(void***)(s + 0xC0);`
- :733 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** kids = *reinterpret_cast<void***>(reinterpret_ca`
- :776 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`
- :1692 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* subRoot2 = *(void**)((uintptr_t)*(void**)subDialo`

#### `code/Game/Dialog/toolui.cpp`
- :470 **NV** singleton pointer chain (data) — `void* keyMap = ***(void****)g_pInputConfigSingletonExre`
- :510 **NV** singleton pointer chain (data) — `void* keyMap = ***(void****)g_pInputConfigSingletonExre`

#### `code/Game/Dialog/townhall_helpers.cpp`
- :480 **NV** FIELD read through an object / IBP pointer (no call) — `if (comp && *(uintptr_t*)((uintptr_t)comp + 0x58) != 0)`

#### `code/Game/Dialog/townhallui.cpp`
- :242 **NV** singleton pointer chain (data) — `uintptr_t profList = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :848 **NV** singleton pointer chain (data) — `self->cachedGameTick = *(int*)(*(uintptr_t*)(*(uintptr_`
- :913 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :946 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uint32_t*)(*(uintptr_t*)labelPtr + 0x128) = colors[g]`
- :949 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t lblST = *(uint64_t*)(*(uintptr_t*)labelPtr + 0`
- :987 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :1033 **NV** singleton pointer chain (data) — `int year = **(int**)(*(uintptr_t*)g_pGameSingleton + 0x`
- :1037 **NV** singleton pointer chain (data) — `float seconds = reinterpret_cast<const System::Time*>((`
- :1124 **NV** singleton pointer chain (data) — `int* timePtr = *(int**)(*(uintptr_t*)g_pGameSingleton +`
- :1133 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :1253 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** compArray = *(void***)(entity + 0x28);`
- :1269 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vtable = *(void***)comp;`
- :1307 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vtable = *(void***)comp;`
- :1504 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :1647 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :1648 **NV** singleton pointer chain (data) — `uintptr_t profListBase = *(uintptr_t*)(*(uintptr_t*)g_p`
- :1810 **NV** singleton pointer chain (data) — `uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGame`
- :1927 **NV** singleton pointer chain (data) — `uintptr_t entityController = *(uintptr_t*)(*(uintptr_t*`
- :1955 **NV** singleton pointer chain (data) — `void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingl`
- :1956 **NV** singleton pointer chain (data) — `uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_`
- :1993 **NV** singleton pointer chain (data) — `void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingl`
- :1994 **NV** singleton pointer chain (data) — `uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_`
- :2046 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(uintptr_t*)(gw + 0x128) == 0) return false;`
- :2075 **NV** a local diagnostic name/pointer table — `const N fns[] = {`

#### `code/Game/Dialog/tradeui.cpp`
- :322 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *(void***)((uintptr_t)descEntity + 0x28);`
- :371 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `static inline void VCall_Element_SetEnabled(void* elem,`
- :382 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `static inline void VCall_Element_Slot10(void* elem, int`
- :456 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** descArray = *(void***)(resEntity + 0x28);`
- :504 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArr = *(void***)(entityPtr[0] + 0x30);`
- :577 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(spinner, spinnerEnabled);`
- :580 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + `
- :581 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + `
- :603 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_Slot10(*(void**)sellUI, color);`
- :604 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_Slot10(*(void**)((uintptr_t)sellUI + 0x08`
- :605 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_Slot10(*(void**)((uintptr_t)sellUI + 0x10`
- :631 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :633 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :635 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :801 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0`
- :802 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0`
- :803 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0`
- :832 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :834 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :836 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + `
- :864 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(comboElem, comboEnabled);`
- :913 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)purchaseBtn, true);`
- :924 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uint8_t*)(*(uintptr_t*)purchaseBtnPtr + 0xd8) = 1;`
- :975 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(btn, false);`
- :978 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uint8_t*)(*(uintptr_t*)purchaseUI + 0xd8) = 0;`
- :1001 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** innerPtr = *(void***)(sp + 0xa0);`
- :1189 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)((uintptr_t)buildComp + 0x`
- :1455 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(entity + 0x28);`
- :2148 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vtbl = *(void***)obj;`
- :2150 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((StateCall_t)(vtbl[2]))(obj);`
- :2289 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** itemList = *(void***)(storageComp + 0x28);`
- :2365 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** itemList = *(void***)(storageComp + 0x28);`
- :2500 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)(uiEntry), false);`
- :2503 **BN** VCall_Element_* helpers are by-name UI::Element::Enable / SetColor (B.3.6) — `VCall_Element_SetEnabled(*(void**)(uiEntry + 0x08), fal`
- :2532 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)((uintptr_t)descEntity + 0`

#### `code/Game/Dialog/tradeui_buyentries.cpp`
- :123 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* const* arr = *reinterpret_cast<void* const* const`
- :162 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* const* goods = *reinterpret_cast<void* const* con`

#### `code/Game/Dialog/tradeui_checkout.cpp`
- :118 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** objs = *reinterpret_cast<void***>(entity + 0x28)`

#### `code/Game/Dialog/tradeui_price.cpp`
- :73 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* const* arr = *reinterpret_cast<void* const* const`

#### `code/Game/Dialog/workplaceui.cpp`
- :800 **SM** slot taken from the member (object/VSlot.h) — `return b ? Object::SlotAddressOf(b, &UI::Element::Enabl`

#### `code/Game/Entity/citizenbehavior.cpp`
- :911 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *reinterpret_cast<void***>(entity + `
- :1693 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(building + 0x30);`
- :1701 **NV** array / pointer FIELD load at a data offset (no vptr) — `compArray = *(void***)(building + 0x30);`
- :1922 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(school + 0x30);`
- :1930 **NV** array / pointer FIELD load at a data offset (no vptr) — `compArray = *(void***)(school + 0x30);`
- :2002 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)(school + 0x30);`
- :2008 **NV** array / pointer FIELD load at a data offset (no vptr) — `compArray = *(void***)(school + 0x30);`

#### `code/Game/Entity/componententity.cpp`
- :305 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `reinterpret_cast<Component*>(compPtr)->Slot5();`
- :741 **NV** FIELD read through an object / IBP pointer (no call) — `const bool armed = (*(uint8_t*)(compPtr + 0x20) & 1) !=`

#### `code/Game/Entity/entitylist.cpp`
- :905 **NV** FIELD read through an object / IBP pointer (no call) — `if (((*reinterpret_cast<uint32_t*>(ent + 0x40) >> 10) &`
- :1247 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(int32_t*)(*(int64_t*)swappedIBP + 0x18) = entityMaste`

#### `code/Game/Game/force_work.cpp`
- :93 **SM** slot taken from the member (object/VSlot.h) — `const long typeSlot = ::Object::CompilerSlotOf(&::Objec`
- :96 **SM** slot taken from the member (object/VSlot.h) — `if (!Executable(::Object::SlotAddressOf(obj, &::Object:`

#### `code/Game/Game/game_leaf_helpers.cpp`
- :226 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *reinterpret_cast<void***>(b + 0x50);`

#### `code/Game/Game/game_main.cpp`
- :241 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(sub1);`
- :242 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<void(*)(void*)>(vt[0x90/8])(sub1);`
- :246 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(sub2);`
- :247 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<void(*)(void*)>(vt[0x10/8])(sub2);`

#### `code/Game/Game/gameupdateloop.cpp`
- :1399 **NV** singleton pointer chain (data) — `void* keyMap = ***(void****)g_pInputConfigSingletonExre`
- :2321 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** citizenArray = *(void***)(citizenList + 0xa0);`

#### `code/Game/Game/gameupdateloop_load.cpp`
- :673 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr0 = *(void***)(p + 0x50);`
- :1603 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** data = resList ? *(void***)((uintptr_t)resList +`

#### `code/Game/Game/gameupdateloop_serialize.cpp`
- :618 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `reinterpret_cast<Component*>(comp)->Slot11();`
- :661 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `if (*(int*)((uintptr_t)*(void**)serializer + 8) == 0) {`

#### `code/Game/Game/goals_serialize.cpp`
- :667 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `reinterpret_cast<Work*>(entity)->Slot12(slot12Ver);`
- :672 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t hi = *reinterpret_cast<uint32_t*>(*reinterpret`
- :694 **NV** FIELD read/write through an object pointer (no call) — `if (*reinterpret_cast<int64_t*>(reinterpret_cast<uint8_`

#### `code/Game/Game/menudialogloopt.cpp`
- :600 **NV** array FIELD load (no vptr) — `void** arr = *reinterpret_cast<void***>(Loop(self)->_pa`

#### `code/Game/Game/serialize_arrays.cpp`
- :466 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int serializerInner = *reinterpret_cast<int*>( reinterp`
- :467 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :486 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :487 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :510 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :511 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`

#### `code/Game/Game/serialize_externals.cpp`
- :134 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int serializerInner = *reinterpret_cast<int*>( reinterp`
- :135 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :152 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :153 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :175 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :176 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`

#### `code/Game/Game/serialize_externals_typed.cpp`
- :118 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int serializerInner = *reinterpret_cast<int*>( reinterp`
- :119 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :136 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :137 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :159 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :160 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`

#### `code/Game/Game/serialize_struct60b_serialize.cpp`
- :149 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :150 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :315 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :316 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :427 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int serializerInner = *reinterpret_cast<int*>( reinterp`
- :428 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :445 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :446 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`
- :468 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `int innerCheck = *reinterpret_cast<int*>( reinterpret_c`
- :469 **NV** IO::Serializer +0 is the Stream*; Stream+8 is its direction word (data) — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(se`

#### `code/Game/Game/starttown.cpp`
- :632 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *(void***)(townMgr + 0x120);`
- :2473 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const int aCount = tbl ? *(int32_t*)(tbl + 0x38) : 0;`
- :2475 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const uintptr_t e = *(uintptr_t*)(tbl + 0x30) + (uintpt`
- :3399 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** comps = *(void** const*)((const uint8_t*)owner +`

#### `code/Game/Game/statemachine.cpp`
- :54 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :55 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `return ((fn_t)vt[0])(node, stateIndex);`
- :63 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :64 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `return ((fn_t)vt[3])(node, ctx) != 0;`
- :71 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :72 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `return ((fn_t)vt[4])(node, ctx) != 0;`
- :81 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :82 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `((fn_t)vt[5])(node, ctx, fromSubState, toExitState);`
- :90 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :91 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `return ((fn_t)vt[6])(node, ctx, deltaTime) != 0;`
- :99 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = (void**)node->vtable;`
- :100 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `return ((fn_t)vt[7])(node, ctx, deltaTime) != 0;`

#### `code/Game/Game/town_workplace_registry.cpp`
- :296 **NV** IBP / owner pointer deref then a FIELD read (no call) — `accepting = *reinterpret_cast<char*>(*reinterpret_cast<`

#### `code/Game/Game/weather_particle_draw.cpp`
- :378 **NV** vertex array / buffer named vtx* — `std::memcpy(vtxCur, staging, vtxBytes);`
- :379 **NV** vertex array / buffer named vtx* — `vtxCur = reinterpret_cast<uint8_t*>(vtxCur) + vtxBytes;`

#### `code/Game/Game/weather_particles.cpp`
- :345 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** activeArray = *reinterpret_cast<void***>(s + 0x5`

#### `code/Game/Game/weather_sim.cpp`
- :234 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** activeArray = *reinterpret_cast<void***>(w + 0x5`
- :311 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *reinterpret_cast<void***>(w + 0x50);`

#### `code/Game/Game/weather_sun.cpp`
- :154 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const EnvKeyframe& a = table[kf];`
- :155 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const EnvKeyframe& b = table[kfNext];`
- :182 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *reinterpret_cast<void***>(ws + 0x50);`

#### `code/Game/Goal/goal_exe_helpers.cpp`
- :82 **NV** string characters — `++k) normalized[k] = (name[k] == '\\') ? '/' : name[k];`

#### `code/Game/Object/internal_factory_audit.cpp`
- :38 **NV** Object factory registration table (data) — `static bool Table(void*** outTable, int* outCount) {`
- :42 **NV** Object factory registration table (data) — `void** tbl = *reinterpret_cast<void***>(fb);`
- :65 **NV** Object factory registration table (data) — `void** tbl = *reinterpret_cast<void***>(fb);`
- :70 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `return tbl[idx] == obj;`

#### `code/Game/Player/bridgetool.cpp`
- :65 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`

#### `code/Game/Player/dragplacementtool.cpp`
- :57 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`

#### `code/Game/Player/dragtool.cpp`
- :164 **NV** singleton pointer chain (data) — `return ***(void****)g_pInputConfigSingletonExref;`

#### `code/Game/Player/edittool.cpp`
- :907 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** kids = *(void***)(e + 0xa0);`

#### `code/Game/Player/fencedtool.cpp`
- :92 **NV** singleton pointer chain (data) — `return ***(void****)g_pInputConfigSingletonExref;`
- :395 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityTypeRef = *(uintptr_t*)(*(uintptr_t*)ce`
- :601 **NV** singleton pointer chain (data) — `startCell = s_fn_GetCursorCell((uint64_t)***(void****)g`
- :667 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityTypeRef = *(uintptr_t*)(*(uintptr_t*)ce`
- :745 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`
- :854 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** modelData = *(void***)(modelArray + 0x08);`

#### `code/Game/Player/keystone_a_removalcalcpath.cpp`
- :120 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `static inline void* SlotFn(uintptr_t slotAddr) {`
- :144 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_memAlloc = reinterpret_cast<fn_memAllocStatic_t>(Slot`
- :145 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `s_timeCtor = reinterpret_cast<fn_timeCtor_t>(SlotFn(0x1`
- :274 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `(*reinterpret_cast<void(**)(int64_t*, void*, int, int)>`
- :303 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `while (((it < 0) // ((*reinterpret_cast<char(**)(int64_`
- :306 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))( p`
- :315 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `(*reinterpret_cast<void(**)(int64_t*, int64_t*, int, in`
- :333 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `char st = (*reinterpret_cast<char(**)(int64_t, int)>(*r`
- :339 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `st = (*reinterpret_cast<char(**)(int64_t*, int)>(*nxt))`
- :348 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `while (((it < 0) // ((*reinterpret_cast<char(**)(int64_`
- :351 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))( p`
- :364 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `while (((it < 0) // ((*reinterpret_cast<char(**)(int64_`
- :368 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))( p`
- :460 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `int iVar8 = (*reinterpret_cast<int(**)(int64_t*, int, i`
- :925 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `(*reinterpret_cast<void(**)(int64_t*, void*, int, int)>`

#### `code/Game/Player/pathtool.cpp`
- :78 **NV** singleton pointer chain (data) — `return (uint64_t)***(void****)g_pInputConfigSingletonEx`

#### `code/Game/Player/pathtool_setselected.cpp`
- :319 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t workplace = *(int64_t*)(*(int64_t*)param_2 + 0x`

#### `code/Game/Player/placementtool.cpp`
- :182 **NV** singleton pointer chain (data) — `return ***(void****)g_pInputConfigSingletonExref;`
- :882 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`
- :1063 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** comps = *(void***)(entity + 0x28);`

#### `code/Game/Player/removaltool.cpp`
- :64 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`

#### `code/Game/Player/roadtool.cpp`
- :128 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** components = *(void***)(entity + 0x28);`
- :172 **NV** singleton pointer chain (data) — `uint64_t keyMap = (uint64_t)***(void****)g_pInputConfig`

#### `code/Game/Player/roadtool_calcpath.cpp`
- :103 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** comps = *(void***)(entity + 0x28);`

#### `code/Game/Player/tool_base_highlight.cpp`
- :57 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x`
- :60 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x14`

#### `code/Game/Player/tool_base_highlight_tess.cpp`
- :69 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x`
- :72 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x14`
- :116 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `opVec2Ctor = reinterpret_cast<Vec2Ctor_t>(slot(0x140151`
- :117 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `opT2Scalar = reinterpret_cast<T2FromScalarVec_t>(slot(0`
- :118 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `opT2YawOff = reinterpret_cast<T2FromYaw_t>(slot(0x14015`
- :119 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `opT2MulT2 = reinterpret_cast<T2MulT2_t>(slot(0x14015264`
- :120 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `opT2MulVec = reinterpret_cast<T2MulVec_t>(slot(0x140152`
- :338 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ppvVar9 = *reinterpret_cast<void***>(lVar16 + 0x`

#### `code/Game/Player/tool_base_path.cpp`
- :184 **NV** FIELD read through an object / IBP pointer (no call) — `ownOk = (*reinterpret_cast<int64_t*>(owner + 0x28) == p`
- :237 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(entity + 0x28) != *rein`
- :279 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(entity + 0x28) == *rein`
- :357 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (lv == *reinterpret_cast<int64_t*>(*reinterpret_cast`

#### `code/Game/Player/tool_base_ray.cpp`
- :227 **NV** singleton pointer chain (data) — `void**** p = reinterpret_cast<void****>(g_pInputConfigS`

#### `code/Game/Player/tool_base_render.cpp`
- :249 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `return (float)(int)tbl[(int64_t)dim * cb + ca] * hscale`
- :425 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** oldMesh = *reinterpret_cast<void***>(self + 0xf8`
- :532 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** m = *reinterpret_cast<void***>(self + 0xf8);`
- :708 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vt = *reinterpret_cast<void***>(self);`
- :730 **SM** slot taken from the member (object/VSlot.h) — `if (vt && ::Object::SlotAddressOf(reinterpret_cast<void`

#### `code/Game/Player/tool_base_tilerender.cpp`
- :277 **NV** vertex array / buffer named vtx* — `float vtx[6];`
- :278 **NV** vertex array / buffer named vtx* — `vtx[0] = quad[k][0]; vtx[1] = quad[k][1]; vtx[2] = quad`
- :279 **NV** vertex array / buffer named vtx* — `vtx[3] = ((vt - half) * sinA + (ut - half) * cosA + hal`
- :280 **NV** vertex array / buffer named vtx* — `vtx[4] = ((vt - half) * cosA - (ut - half) * sinA) + ha`
- :281 **NV** vertex array / buffer named vtx* — `std::memcpy(&vtx[5], &rec[3], 4);`
- :299 **NV** vertex array / buffer named vtx* — `float vtx[6];`
- :301 **NV** vertex array / buffer named vtx* — `vtx[1] = corners[k * 3 + 1];`
- :302 **NV** vertex array / buffer named vtx* — `vtx[2] = corners[k * 3 + 2];`
- :303 **NV** vertex array / buffer named vtx* — `vtx[3] = ((vt - half) * sinA + (ut - half) * cosA + hal`
- :304 **NV** vertex array / buffer named vtx* — `vtx[4] = ((vt - half) * cosA - (ut - half) * sinA) + ha`
- :305 **NV** vertex array / buffer named vtx* — `std::memcpy(&vtx[5], &rec[3], 4);`

#### `code/Game/Player/tool_base_trade.cpp`
- :57 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x`
- :60 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x14`

#### `code/Game/Tutorial/tutorial_base.cpp`
- :133 **NV** string characters — `++k) normalized[k] = (name[k] == '\\') ? '/' : name[k];`
- :270 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int*>(comp + 0x30) == 0) {`

#### `code/Game/Tutorial/tutorialhighlightdialog.cpp`
- :298 **SM** slot taken from the member (object/VSlot.h) — `if (!::Object::SlotAddressOf(reinterpret_cast<void*>(ob`

#### `code/Game/Tutorial/tutorialinstance.cpp`
- :88 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** stepsArray = *reinterpret_cast<void***>(td + 0x2`

#### `code/Game/Tutorial/tutorialsetupcamera.cpp`
- :81 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** gridPtr = *reinterpret_cast<void***>(gameWorld +`

#### `code/Game/Tutorial/tutorialsetuprestrictplayer.cpp`
- :87 **NV** string characters — `++k) normalized[k] = (name[k] == '\\') ? '/' : name[k];`

#### `code/Game/Work/ConsumeProduce.h`
- :55 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot12(int version) override;`

#### `code/Game/Work/LocationWork.h`
- :57 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Slot12(int version) override;`

#### `code/Game/Work/Pickup.h`
- :47 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `int Slot5(void* entityIBP, void* context) override;`

#### `code/Game/Work/Work.h`
- :63 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `virtual int Slot5(void* citizen, void* context);`
- :80 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `virtual void Slot12(int version);`

#### `code/Game/Work/buildroad.cpp`
- :288 **NV** FIELD read through an object / IBP pointer (no call) — `if (actionPtr == *(uint64_t*)(citizenEntity + 0x1f0) &&`
- :417 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t homeEntity = *(uint64_t*)(*(uintptr_t*)entityI`
- :447 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** citizenList = *(void***)(gameWorld + 0xf0);`

#### `code/Game/Work/buildstructure.cpp`
- :525 **NV** FIELD read through an object / IBP pointer (no call) — `if (targetVal == *(uintptr_t*)(bldgEntity + 0x1f0) && p`
- :612 **NV** FIELD read through an object / IBP pointer (no call) — `if (*(int*)(taskPtr + 0x30) == self->field_20) {`

#### `code/Game/Work/cleararea.cpp`
- :761 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity2 == *(uint64_t*)((uintptr_t)citizenObj +`
- :860 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity2 == *(uint64_t*)((uintptr_t)citizenObj +`

#### `code/Game/Work/clearstorage.cpp`
- :317 **NV** FIELD read through an object / IBP pointer (no call) — `if (resultVal == *(uintptr_t*)(citizenPtr + 0x1f0) && p`
- :355 **NV** FIELD read through an object / IBP pointer (no call) — `while (*(uintptr_t*)(node + 0x10) != 0) {`
- :413 **NV** FIELD read through an object / IBP pointer (no call) — `while (*(uintptr_t*)(node + 0x10) != 0) {`

#### `code/Game/Work/consumeproduce.cpp`
- :110 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable (read by hand) — `if (iatTypeSer) s_fn_Type_Serialize = *reinterpret_cast`
- :383 **NV** FIELD read through an object / IBP pointer (no call) — `if (actionPtr == *(uint64_t*)(citizenEntity + 0x1f0) &&`
- :403 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** worldEntityList = *(void***)(gameWorld + 0xf0);`
- :1257 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void ConsumeProduce::Slot12(int ) {}`

#### `code/Game/Work/firework.cpp`
- :204 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* citizenObj = *(void**)(*(uintptr_t*)citizenIBP + `
- :321 **NV** FIELD read through an object / IBP pointer (no call) — `if (targetVal == *(uintptr_t*)(citizenVal + 0x1f0) && p`

#### `code/Game/Work/fishingwork.cpp`
- :460 **NV** FIELD read through an object / IBP pointer (no call) — `if (actionVal == *(uint64_t*)(ce + 0x1f0) && priority >`
- :486 **NV** FIELD read through an object / IBP pointer (no call) — `if (actionVal == *(uint64_t*)(ce + 0x1f0) && priority >`

#### `code/Game/Work/foresterwork.cpp`
- :417 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity == *(uintptr_t*)(citizen + 0x1f0) && pri`
- :738 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* natResDescObj = *(void**)(*(uintptr_t*)natResComp`
- :757 **NV** IBP / owner pointer deref then a FIELD read (no call) — `float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + `
- :761 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResComp`
- :892 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && pr`

#### `code/Game/Work/gatherrawmaterials.cpp`
- :426 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (*(char*)(*(uintptr_t*)wpExtIBP + 0x50) == '\0') {`
- :639 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity == *(uintptr_t*)(citizenFinal + 0x1f0) &`

#### `code/Game/Work/harvestarea.cpp`
- :462 **NV** FIELD read through an object / IBP pointer (no call) — `if (natResIBP2 != 0 && (*(uint32_t*)(natResIBP2 + 0x20)`
- :514 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`
- :825 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCom`

#### `code/Game/Work/harvestorchard.cpp`
- :490 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`
- :745 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCom`

#### `code/Game/Work/herbalistwork.cpp`
- :334 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity == *(uintptr_t*)(citizen + 0x1f0) && pri`
- :550 **NV** IBP / owner pointer deref then a FIELD read (no call) — `float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + `
- :563 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResComp`
- :652 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && pr`

#### `code/Game/Work/huntwork.cpp`
- :312 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* herdObj = *(void**)(*(uintptr_t*)herdIBP + 0x18);`
- :509 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`

#### `code/Game/Work/locationwork.cpp`
- :321 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEntity == *(uintptr_t*)(citEntPost + 0x1f0) && `
- :437 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void LocationWork::Slot12(int ) {}`

#### `code/Game/Work/marketwork.cpp`
- :1348 **NV** FIELD read through an object / IBP pointer (no call) — `if (tgtVal == *reinterpret_cast<int64_t*>(citizen3 + 0x`
- :1701 **NV** FIELD read through an object / IBP pointer (no call) — `if (tgtVal2 == *reinterpret_cast<int64_t*>(citizen5 + 0`

#### `code/Game/Work/pasturework.cpp`
- :512 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`
- :561 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint64_t overflowResult = s_fn_StorageOverflowCheck( st`
- :654 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`

#### `code/Game/Work/pickup.cpp`
- :302 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_x_memAlloc = reinterpret_cast<PiX_MemAlloc_t>(slot(0x`
- :305 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `s_x_memFree = reinterpret_cast<PiX_MemFree_t>(slot(0x14`
- :439 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (!s_x_wantsRes(townMgr, *reinterpret_cast<int64_t*>(`
- :459 **NV** IBP / owner pointer deref then a FIELD read (no call) — `s_x_findStoreRes(townMgr, &storeOut, static_cast<uint32`
- :532 **NV** IBP / owner pointer deref then a FIELD read (no call) — `const int nTypeId = *reinterpret_cast<int*>( *reinterpr`
- :539 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int* node = *reinterpret_cast<int**>(*reinterpret_cast<`
- :620 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int* node = *reinterpret_cast<int**>(*reinterpret_cast<`
- :660 **NV** array / pointer FIELD load at a data offset (no vptr) — `if (targetOut == *reinterpret_cast<void**>(citEntity + `
- :682 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `int Pickup::Slot5(void* entityIBP, void* param3) {`
- :689 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t citizenEntity = *(uintptr_t*)(*(uintptr_t*)en`

#### `code/Game/Work/pickup_gate.cpp`
- :128 **NV** IBP / owner pointer deref then a FIELD read (no call) — `mask = *reinterpret_cast<int*>(*reinterpret_cast<uintpt`

#### `code/Game/Work/plantarea.cpp`
- :418 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`
- :764 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)result`

#### `code/Game/Work/plantorchard.cpp`
- :372 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && pri`
- :617 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)result`

#### `code/Game/Work/tendarea.cpp`
- :380 **NV** FIELD read through an object / IBP pointer (no call) — `if (workEnt == *(uintptr_t*)(citEnt + 0x1f0) && priorit`

#### `code/Game/Work/tradework.cpp`
- :318 **NV** FIELD read through an object / IBP pointer (no call) — `if (slot10Ent == *reinterpret_cast<int64_t*>(citizen + `

#### `code/Game/Work/work_act_clear.cpp`
- :218 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t firstEntityInner = *reinterpret_cast<uintptr_`
- :223 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* targetComp = *reinterpret_cast<void**>(*reinterpr`
- :247 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :289 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t innerR4 = *reinterpret_cast<uintptr_t*>(*rein`
- :312 **NV** FIELD read through an object / IBP pointer (no call) — `if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 && `
- :370 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`

#### `code/Game/Work/work_act_consume.cpp`
- :273 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :301 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :305 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterp`
- :354 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t listBase1 = *reinterpret_cast<uintptr_t*>(*re`
- :444 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`

#### `code/Game/Work/work_act_hunt.cpp`
- :175 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :203 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :243 **NV** IBP / owner pointer deref then a FIELD read (no call) — `*reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr`
- :288 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :381 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`

#### `code/Game/Work/work_act_path.cpp`
- :216 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :237 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :346 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>( `
- :445 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>( `

#### `code/Game/Work/work_act_perform.cpp`
- :218 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :310 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :360 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :413 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :467 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :500 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :582 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`

#### `code/Game/Work/work_act_rawmat.cpp`
- :184 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :221 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*rei`
- :226 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (storageIBP) p7 = *reinterpret_cast<uint32_t*>(*rein`
- :231 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entField248 = *reinterpret_cast<uintptr_t*>(*`
- :264 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*rei`
- :311 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :362 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*rei`
- :367 **NV** IBP / owner pointer deref then a FIELD read (no call) — `if (storageIBP) p7 = *reinterpret_cast<uint32_t*>(*rein`
- :412 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*rei`

#### `code/Game/Work/work_base_actionctor.cpp`
- :216 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* targetComp = *reinterpret_cast<void**>(*reinterpr`
- :221 **NV** IBP / owner pointer deref then a FIELD read (no call) — `int64_t targetInner = *reinterpret_cast<int64_t*>(*rein`
- :244 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :357 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :384 **NV** FIELD read through an object / IBP pointer (no call) — `if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 && `
- :475 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :562 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :611 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :639 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :682 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :752 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`
- :807 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* entityInner = *reinterpret_cast<void**>(*reinterp`
- :881 **NV** IBP / owner pointer deref then a FIELD read (no call) — `uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*`

#### `code/Game/Work/work_base_grid.cpp`
- :93 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `uint64_t assignR = static_cast<uint32_t>(reinterpret_ca`

#### `code/Game/Work/work_base_standalone.cpp`
- :44 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `DEFINE_EXE_ABSTRACT_TYPE(Work, "") int Work::Slot5(void`
- :47 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `int Work::Slot5(void*, void*) { return 0; }`
- :49 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `void Work::Slot12(int ) {}`

#### `code/Game/Work/work_base_walkpos.cpp`
- :167 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(target + 0x1f0) != gate`
- :169 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<int64_t*>(target + 0x248) != 0) {`
- :177 **NV** IBP / owner pointer deref then a FIELD read (no call) — `void* obj = *reinterpret_cast<void**>(*reinterpret_cast`

#### `code/Game/Work/work_leaf_hunt.cpp`
- :325 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `SMQuery_t query = *reinterpret_cast<SMQuery_t*>(*reinte`

#### `dx11/funcs/dx11_seam.cpp`
- :25 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `static const VtDesc kVtables[static_cast<int>(Vt::kVtCo`
- :41 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `int VtSlotCount(Vt v) { return kVtables[static_cast<int`
- :58 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `static SlotEntry s_slots[static_cast<int>(Vt::kVtCount)`
- :75 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `static OurVtable s_ourVt[static_cast<int>(Vt::kVtCount)`
- :80 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `if (vi < 0 // vi >= static_cast<int>(Vt::kVtCount)) ret`
- :81 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `return s_ourVt[vi].slot;`
- :86 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `static bool ValidSlot(Vt v, int slot) {`
- :88 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `return vi >= 0 && vi < static_cast<int>(Vt::kVtCount) &`
- :96 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `static unsigned s_forward[static_cast<int>(Vt::kVtCount`
- :102 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `if (!ValidSlot(v, slot)) return nullptr;`
- :120 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `fprintf(stderr, "", ValidSlot(v, slot) ? VtName(v) : ""`
- :124 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `if (!ValidSlot(v, slot) // !impl) {`
- :148 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); +`
- :149 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `s_ourVt[vi].rtti = nullptr;`
- :161 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `s_ourVt[vi].slot[slot] = e.impl;`
- :176 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `total, total, static_cast<int>(Vt::kVtCount));`
- :177 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); +`
- :178 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `printf("", kVtables[vi].name, static_cast<void*>(s_ourV`
- :198 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `for (int vi = 0; vi < static_cast<int>(Vt::kVtCount); +`

#### `dx11/funcs/dx11_seam.h`
- :74 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `kVtCount`
- :80 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `int VtSlotCount(Vt v);`

#### `dx11/src/buffers.cpp`
- :627 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = g_deps.vtVertexBufferData;`
- :716 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = g_deps.vtIndexBufferData;`
- :833 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = g_deps.vtConstantBufferData;`

#### `dx11/src/contextdata.cpp`
- :440 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::ContextData);`
- :670 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `void** ddVtable = static_cast<void**>(dd->vptr);`
- :671 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `reinterpret_cast<void (*)(DeviceData*)>(ddVtable[kSlot_`
- :672 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `const bool created = reinterpret_cast<bool (*)(DeviceDa`

#### `dx11/src/devicedata.cpp`
- :1127 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::DeviceData);`
- :1951 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `void* g_dx9PlaceholderVtable[4] = {`
- :2015 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `p->vptr = Dx11VtableAddress(vt);`

#### `dx11/src/exports.cpp`
- :101 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `p->vptr = Dx11VtableAddress(Vt::DeviceData);`

#### `dx11/src/imagebufferdata.cpp`
- :402 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11LiveVtable(Vt::ImageBufferData);`

#### `dx11/src/renderstatedata.cpp`
- :391 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11LiveVtable(Vt::RenderStateData);`

#### `dx11/src/shaderprograms.cpp`
- :86 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::VertexProgramData);`
- :207 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::PixelProgramData);`

#### `dx11/src/smallclasses.cpp`
- :77 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::VideoProgramData);`
- :132 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::RenderBufferData);`

#### `dx11/src/vertexlayoutdata.cpp`
- :105 **EX** dx11 seam: the twelve Video::*Data hand tables + Dx9Placeholder (X1 by decision) — `self->vptr = Dx11VtableAddress(Vt::VertexLayoutData);`

#### `engine/funcs/engine_funcs.cpp`
- :483 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** compArray = *(void***)((uintptr_t)entity + 0x30)`

#### `runtime/src/achievement/achievement_serialize.cpp`
- :114 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = enc->vtable;`
- :120 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[sl`

#### `runtime/src/achievement/achievement_vtable.cpp`
- :193 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = enc->vtable;`
- :199 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[sl`

#### `runtime/src/achievement/goal_serialize.cpp`
- :163 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = enc->vtable;`
- :169 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[sl`

#### `runtime/src/audio/audio.cpp`
- :696 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVtable = *reinterpret_cast<void***>(st->encod`
- :699 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `auto factory = reinterpret_cast<FactoryFn>(encVtable[fa`

#### `runtime/src/audio/audio_device.cpp`
- :166 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vtable = *reinterpret_cast<void***>(voice);`
- :167 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<void (*)(void*)>(vtable[kVoiceReleaseS`
- :286 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vtable = *reinterpret_cast<void***>(voice);`
- :287 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<void (*)(void*, void*, uint32_t)>( vta`
- :288 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `vtable[kVoiceStopSlot / 8])(voice, nullptr, 0);`

#### `runtime/src/audio/audio_platform_startup_win.cpp`
- :81 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `return *reinterpret_cast<void* const* const*>(obj);`
- :84 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `template <typename Fn> inline Fn Slot(void* obj, size_t`
- :85 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `inline Fn Slot(void* obj, size_t byteOffset) {`
- :86 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `return reinterpret_cast<Fn>(VtblOf(obj)[byteOffset / si`
- :113 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const long hr2 = Slot<InitFn>(obj, 0x28)(obj, 0, 0xFFFF`
- :116 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `Slot<ReleaseFn>(obj, 0x10)(obj);`
- :149 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const long hrMv = Slot<CreateMasteringVoiceFn>(xa2, 0x5`
- :154 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `Slot<ReleaseFn>(*xaSlot, 0x10)(*xaSlot);`

#### `runtime/src/audio/audio_play_platform.cpp`
- :384 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** engineVtbl = *reinterpret_cast<void***>(engine);`
- :385 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto createSourceVoice = reinterpret_cast<CreateSourceV`
- :420 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** voiceVtbl = *reinterpret_cast<void***>(voice);`
- :428 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto destroy = reinterpret_cast<DestroyVoiceFn>(voiceVt`
- :434 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto submit = reinterpret_cast<SubmitSourceBufferFn>( v`
- :443 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto setVolume = reinterpret_cast<SetFloatOpFn>(voiceVt`
- :451 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto setFreqRatio = reinterpret_cast<SetFloatOpFn>(voic`
- :458 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto start = reinterpret_cast<StartFn>(voiceVtbl[kVoice`

#### `runtime/src/audio/audio_scene_update.cpp`
- :598 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(pc);`
- :599 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `auto setVol = reinterpret_cast<SetVolumeFn>(vt[0x60 / 8`

#### `runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp`
- :159 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :163 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`
- :167 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `void DestroyReaderWriter(ReaderWriter* rw) {`
- :256 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DestroyReaderWriter(ser.readerWriter);`
- :442 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DestroyReaderWriter(ser.readerWriter);`

#### `runtime/src/audio/vtbatch7b_stream_run_win.cpp`
- :413 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<GetStateFn>( (*reinterpret_cast<void**`
- :414 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `(*reinterpret_cast<void***>(voice))[kVoiceGetState / 8]`
- :426 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<GetStateFn>( (*reinterpret_cast<void**`
- :427 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `(*reinterpret_cast<void***>(voice))[kVoiceGetState / 8]`
- :456 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<SubmitFn>( (*reinterpret_cast<void***>`
- :457 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `(*reinterpret_cast<void***>(voice))[kVoiceSubmitSourceB`
- :493 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<GetStateFn>( (*reinterpret_cast<void**`
- :494 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `(*reinterpret_cast<void***>(voice))[kVoiceGetState / 8]`
- :509 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<GetStateFn>( (*reinterpret_cast<void**`
- :510 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `(*reinterpret_cast<void***>(voice))[kVoiceGetState / 8]`

#### `runtime/src/collection/alloc_door.cpp`
- :87 **NV** IAT slot helper — `inline void** Slot(uintptr_t exeAddr, void*** cache) {`
- :101 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `if (void** const s = Slot(0x140152b60, &slot)) {`
- :115 **NV** IAT / dispatch-slot read (ResolveAddr), not a vtable — `if (void** const s = Slot(0x140152b68, &slot)) {`

#### `runtime/src/collision2d/collisionmesh.cpp`
- :302 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :306 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/collision2d/surface.cpp`
- :92 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :96 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/engine/engine_lifecycle.cpp`
- :803 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(a);`
- :804 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<VoidThisFn>(vt[0x90 / 8])(a);`
- :808 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(b);`
- :809 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<VoidThisFn>(vt[0x10 / 8])(b);`

#### `runtime/src/engine/loops.cpp`
- :1111 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<SetModeFn>((*reinterpret_cast<void***>`
- :1131 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<SetModeFn>((*reinterpret_cast<void***>`

#### `runtime/src/engine/mainloop_frame.cpp`
- :477 **NV** post-update callback ARRAY (data), not a vtable (read by hand) — `auto fn = *reinterpret_cast<void (**)(const void*, void`

#### `runtime/src/engine/mainloop_video.cpp`
- :379 **BN** CallGetTypeId body is ::Object::Base::Type() by name — `inline uint32_t CallGetTypeId(const RtObject::External*`
- :554 **BN** CallGetTypeId body is ::Object::Base::Type() by name — `if (CallGetTypeId(e) == wanted) {`
- :564 **BN** CallGetTypeId body is ::Object::Base::Type() by name — `rec.typeId = static_cast<int32_t>(CallGetTypeId(e));`
- :622 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(payload);`
- :624 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<void (*)(void*, int)>(vt[0])(payload, `
- :759 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const typeFns = *reinterpret_cast<void***>(node `

#### `runtime/src/graphics/animation.cpp`
- :653 **NV** array / pointer FIELD load at a data offset (no vptr) — `uint8_t* const rec = (*reinterpret_cast<uint8_t* const*`
- :654 **NV** array / pointer FIELD load at a data offset (no vptr) — `(*reinterpret_cast<uint8_t* const* const*>(self + 0x38)`

#### `runtime/src/graphics/batchc_gfx_leaves.cpp`
- :574 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `void DeleteRw(ReaderWriter* rw) {`
- :582 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :586 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`
- :606 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void SerializeBufferSlot(void* obj, Serializer* sub) {`
- :607 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(obj);`
- :608 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<BufSerFn>(vt[0x30 / 8])(obj, sub);`
- :677 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(outer->stream`
- :679 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (!reinterpret_cast<CarriesPlatformDataFn>(encVt[0])(`
- :694 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DeleteRw(rw);`
- :739 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `SerializeBufferSlot(lvl2->_indexBuffer._deviceData, &su`
- :740 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `SerializeBufferSlot(lvl2->_vertexBuffer._deviceData, &s`
- :741 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `SerializeBufferSlot(lvl2->_skinIndexBuffer._deviceData,`
- :742 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `SerializeBufferSlot(lvl2->_skinVertexBuffer._deviceData`
- :749 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DeleteRw(rw);`
- :878 **NV** FIELD / array load (no vptr) — `const uint8_t* const* const programs = *reinterpret_cas`

#### `runtime/src/graphics/edgemesh_vtable.cpp`
- :363 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** const fvt = *reinterpret_cast<void***>(factory);`
- :367 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `MakeEncFn make = *reinterpret_cast<MakeEncFn*>( reinter`
- :368 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<uint8_t*>(fvt) + off);`
- :425 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const bvt = *reinterpret_cast<void***>(buf);`
- :426 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `BufSerializeFn f = *reinterpret_cast<BufSerializeFn*>( `
- :427 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<uint8_t*>(bvt) + kVideoBufferSerialize`

#### `runtime/src/graphics/graphicsmesh_vtable.cpp`
- :223 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = enc->vtable;`
- :228 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[sl`
- :471 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(sub);`
- :472 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<void (*)(void*, Serializer*)>(vt[0x30 `
- :487 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (!reinterpret_cast<EncQueryFn>(enc->vtable[0])(enc))`
- :489 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (!reinterpret_cast<EncQueryFn>(enc->vtable[0])(enc))`

#### `runtime/src/graphics/lights_vtable.cpp`
- :350 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void RenderStateSlot4(void* ctx) {`
- :352 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(obj);`
- :354 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[4])(obj, 6, 0, AsFloat(kBits_on`
- :749 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `RenderStateSlot4(ctx);`
- :864 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(obj);`
- :865 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `modern = reinterpret_cast<bool (*)(void*)>(vt[7])(obj);`
- :960 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `RenderStateSlot4(ctx);`

#### `runtime/src/graphics/material.cpp`
- :126 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :130 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/graphics/mesh.cpp`
- :3018 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(buf);`
- :3019 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `BufCreateFn create = reinterpret_cast<BufCreateFn>(vt[1`
- :3020 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `BufCreateFn fill = reinterpret_cast<BufCreateFn>(vt[2])`

#### `runtime/src/graphics/meshinstance_draw.cpp`
- :289 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(obj);`
- :291 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[12])(obj, 0, *reinterpret_cast<`

#### `runtime/src/graphics/renderer.cpp`
- :259 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vtable = *reinterpret_cast<void***>(iface);`
- :261 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return reinterpret_cast<Fn>(vtable[7])(iface);`

#### `runtime/src/graphics/sunlight.cpp`
- :178 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `CapFn slot6 = *reinterpret_cast<CapFn*>( static_cast<ui`
- :180 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return slot6(dd);`

#### `runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp`
- :212 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :216 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`
- :220 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `void DestroyReaderWriter(ReaderWriter* rw) {`
- :459 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DestroyReaderWriter(ser.readerWriter);`
- :840 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DestroyReaderWriter(ser.readerWriter);`

#### `runtime/src/graphics/wave23_lane_gfxbody.cpp`
- :275 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(data);`
- :276 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<void (*)(void*)>(vt[1])(data);`
- :689 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(data);`
- :690 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `const bool ok = reinterpret_cast<bool (*)(void*, void*)`
- :733 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** const vt = *reinterpret_cast<void***>(data);`
- :734 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<void (*)(void*, void*, void*, bool)>(v`

#### `runtime/src/graphics/wave24_lane_gfxmat.cpp`
- :400 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(obj);`
- :401 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `void* VSlot(void* obj, size_t byteOffset) {`
- :402 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return VTableOf(obj)[byteOffset / sizeof(void*)];`
- :418 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void* enc = reinterpret_cast<MakeEncodingFn>(VSlot(fact`

#### `runtime/src/graphics/wave27_lane_gfxbig.cpp`
- :448 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return *reinterpret_cast<void***>(obj);`
- :892 **NV** FIELD / array load (no vptr) — `acc24 = static_cast<uint8_t>( acc24 / (*reinterpret_cas`
- :897 **NV** FIELD / array load (no vptr) — `acc25 = static_cast<uint8_t>( acc25 / (*reinterpret_cas`
- :1371 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<rtgfxbig::ClearFn>(rtgfxbig::VTable(cd`

#### `runtime/src/graphics/wave27_lane_gfxpart.cpp`
- :170 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return *reinterpret_cast<void* const*>(obj) ? *reinterp`
- :172 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void* VSlot(const void* obj, size_t byteOffset) {`
- :173 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return VTableOf(obj)[byteOffset / sizeof(void*)];`
- :296 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(enc->vta`
- :297 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc,`
- :768 **NV** FIELD / array load (no vptr) — `void** base = *reinterpret_cast<void***>(p + kParticle_`
- :912 **NV** FIELD / array load (no vptr) — `void* const* table = *reinterpret_cast<void* const* con`
- :1092 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `if (!reinterpret_cast<BeginFn>(VSlot(ctxData, 0x90))( c`
- :1169 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<EndFn>(VSlot(ctxData, 0x98))(ctxData, `

#### `runtime/src/graphics/wave28_lane_gfxbody.cpp`
- :1183 **SM** slot taken from the member (object/VSlot.h) — `reinterpret_cast<rtgfxbody28::EventFireFn>( ::Object::S`

#### `runtime/src/graphics/wave28_lane_partsys.cpp`
- :582 **NV** array / pointer FIELD load at a data offset (no vptr) — `uint8_t** boxes = *reinterpret_cast<uint8_t***>(res + k`

#### `runtime/src/graphics/wave9_lane_gfx.cpp`
- :460 **NV** array / pointer FIELD load at a data offset (no vptr) — `const uint8_t* const* programs = *reinterpret_cast<cons`

#### `runtime/src/input/input_device_update.cpp`
- :277 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `inline void* ComSlot(void* com, int byteOffset) {`
- :278 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vtbl = *reinterpret_cast<void***>(com);`
- :279 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `return vtbl[byteOffset / 8];`
- :447 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const uint32_t hr = reinterpret_cast<GetStateFn>( ComSl`
- :448 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComSlot(keyboard, kComGetDeviceState))(keyboard, 0x100,`
- :457 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const uint32_t a = reinterpret_cast<AcquireFn>(ComSlot(`
- :458 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<AcquireFn>(ComSlot(dev, kComAcquire))(`
- :570 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const uint32_t hr = reinterpret_cast<GetStateFn>( ComSl`
- :571 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComSlot(mouse, kComGetDeviceState))(mouse, 0x14, &st);`
- :576 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `const uint32_t a = reinterpret_cast<AcquireFn>(ComSlot(`
- :577 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<AcquireFn>(ComSlot(dev, kComAcquire))(`

#### `runtime/src/input/input_platform_shutdown_win.cpp`
- :233 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `inline void ComCall(void* obj, int slotByteOffset) {`
- :234 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vtbl = *reinterpret_cast<void***>(obj);`
- :236 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `reinterpret_cast<Fn>(vtbl[slotByteOffset / 8])(obj);`
- :271 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComCall(mouse, kComUnacquire);`
- :275 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComCall(_mouseDevice, kComRelease);`
- :282 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComCall(keyboard, kComUnacquire);`
- :283 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComCall(_keyboardDevice, kComRelease);`
- :291 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `ComCall(dinput, kComRelease);`

#### `runtime/src/input/input_platform_startup_win.cpp`
- :131 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `return *reinterpret_cast<void* const* const*>(obj);`
- :133 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `template <typename Fn> inline Fn Slot(void* obj, size_t`
- :134 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `inline Fn Slot(void* obj, size_t byteOffset) {`
- :135 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `return reinterpret_cast<Fn>(VtblOf(obj)[byteOffset / si`
- :286 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<CreateDeviceFn>(di8, 0x18)(di8, GUID_SysKeybo`
- :292 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<SetDataFormatFn>(kbd, 0x58)(kbd, &c_dfDIKeybo`
- :299 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<SetCoopFn>(kbd, 0x68)(kbd, hwnd, kCoopLevel);`
- :305 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<CreateDeviceFn>(di8, 0x18)(di8, GUID_SysMouse`
- :311 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<SetDataFormatFn>(mouse, 0x58)(mouse, &c_dfDIM`
- :316 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `hr = Slot<SetCoopFn>(mouse, 0x68)(mouse, hwnd, kCoopLev`
- :345 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `Slot<AcquireFn>(kbd, 0x38)(kbd);`
- :347 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `Slot<AcquireFn>(mouse, 0x38)(mouse);`

#### `runtime/src/io/compressed_stream_release.cpp`
- :76 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const data = *reinterpret_cast<void***>(base + 0`
- :96 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const comp = *reinterpret_cast<void***>(base + 0`

#### `runtime/src/io/package_file_serialize.cpp`
- :209 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(enc->vta`
- :210 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc,`

#### `runtime/src/io/persistence_saveinfo_job.cpp`
- :187 **NV** CRC table — `static uint32_t table[256];`

#### `runtime/src/io/persistence_worker_win.cpp`
- :171 **NV** CRC table — `static uint32_t table[256];`

#### `runtime/src/io/serializer_class.cpp`
- :141 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** const fvt = *reinterpret_cast<void** const*>(fac`
- :146 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `_readerWriter = reinterpret_cast<MakeRw>( *reinterpret_`
- :148 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<const uint8_t*>(fvt) + slot))(factory,`

#### `runtime/src/object/external_class_doors.cpp`
- :128 **BN** retslot helper, no vtable — `template <class Fn> inline System::Filename ByHiddenSlo`
- :205 **BN** retslot helper, no vtable — `return ByHiddenSlot([self, id](RtSystem::Filename* slot`
- :212 **BN** retslot helper, no vtable — `return ByHiddenSlot([self](RtSystem::Filename* slot) {`
- :219 **BN** retslot helper, no vtable — `return ByHiddenSlot([self](RtSystem::Filename* slot) {`
- :228 **BN** retslot helper, no vtable — `return ByHiddenSlot([self, p, appendIdentifier](RtSyste`
- :306 **BN** retslot helper, no vtable — `return ByHiddenSlot([p](RtSystem::Filename* slot) {`

#### `runtime/src/object/external_list.cpp`
- :86 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :109 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(f`

#### `runtime/src/object/external_load.cpp`
- :293 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(enc);`
- :295 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ser.impl = reinterpret_cast<SerFactoryFn>(encVt[slot / `

#### `runtime/src/object/external_ptr_serialize.cpp`
- :273 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `return reinterpret_cast<GetTypeFn>(enc->vtable[0x10 / 8`

#### `runtime/src/object/external_reftable.cpp`
- :149 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `inline void* VSlot(void* obj, int byteOffset) {`
- :150 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `void** vt = *reinterpret_cast<void***>(obj);`
- :151 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return vt[byteOffset / 8];`
- :223 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<EncFactoryFn>(VSlot`
- :224 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<EncFactoryFn>(VSlot(enc, factorySlot))`

#### `runtime/src/pathfinding/pointlist.cpp`
- :228 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :232 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/picking/pickingmesh.cpp`
- :283 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :287 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/save/savedstate_setupload.cpp`
- :136 **BN** persistence size map (data) — `uint32_t LookupSlotSize(const uint8_t* persistence, int`
- :172 **BN** persistence size map (data) — `const uint32_t size = LookupSlotSize(persistence, stati`

#### `runtime/src/save/saveload_draw.cpp`
- :805 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `(*reinterpret_cast<ReadBackFn*>( reinterpret_cast<uint8`
- :806 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(pl`

#### `runtime/src/save/saveload_loadmeta.cpp`
- :239 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `template <typename Fn> inline Fn VSlot(const void* obj,`
- :240 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `inline Fn VSlot(const void* obj, int byteOffset) {`
- :241 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `const void* const* vtbl = *reinterpret_cast<const void*`
- :242 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return reinterpret_cast<Fn>( const_cast<void*>(vtbl[byt`
- :320 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ser.readerWriter = VSlot<CreateCodecFn>(enc, slot)(enc,`

#### `runtime/src/save/saveload_meta.cpp`
- :345 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `inline void* ReadVtableSlot(const void* obj, int byteOf`
- :346 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `void* const* vtbl = *reinterpret_cast<void* const* cons`
- :348 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `reinterpret_cast<const uint8_t*>(vtbl) + byteOffset);`
- :399 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ser.readerWriter = reinterpret_cast<MakeCodecFn>(ReadVt`

#### `runtime/src/save/saveload_update.cpp`
- :1161 **SM** slot taken from the member (object/VSlot.h) — `const void* saveRestore = ::Object::SlotAddressOf(loop,`
- :1166 **SM** oracle label; the slot address comes from SlotAddressOf(&UpdateLoop::SaveRestore) — `if (out.Call("Engine::UpdateLoop::SaveRestore [vtbl+0x3`
- :2061 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<DeletingDtorFn>((*reinterpret_cast<voi`

#### `runtime/src/system/crc.cpp`
- :48 **NV** CRC table — `static uint32_t table[256];`

#### `runtime/src/system/thread_win.cpp`
- :331 **EX** System::Thread (slot 0 Run) - X1 — `void** vt = static_cast<void**>(self->vtable);`
- :333 **EX** System::Thread (slot 0 Run) - X1 — `reinterpret_cast<WorkFn>(vt[0])(self);`
- :335 **EX** System::Thread (slot 0 Run) - X1 — `void** vtAfter = static_cast<void**>(self->vtable);`
- :336 **EX** System::Thread (slot 0 Run) - X1 — `reinterpret_cast<WorkFn>(vtAfter[1])(self);`

#### `runtime/src/ui/animatedimagedescription.cpp`
- :161 **BN** a local InternalPtr named slot — `Object::InternalPtr<ImageElement> slot(elem);`

#### `runtime/src/ui/buttondescription.cpp`
- :197 **BN** a local InternalPtr named slot — `Object::InternalPtr<ButtonElement> slot(elem);`

#### `runtime/src/ui/characterset.cpp`
- :66 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :130 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/checkdescription.cpp`
- :119 **BN** a local InternalPtr named slot — `Object::InternalPtr<ButtonElement> slot(elem);`

#### `runtime/src/ui/combodescription.cpp`
- :173 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `if (*reinterpret_cast<const int32_t*>(table + 0x28) > 0`
- :211 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `} while (i < *reinterpret_cast<const int32_t*>(table + `

#### `runtime/src/ui/cursor.cpp`
- :76 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :138 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/description_common.h`
- :96 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *const_cast<void***>(reinterpret_cast<void** con`
- :131 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(f`
- :162 **NV** array / pointer FIELD load at a data offset (no vptr) — `if (child != nullptr) *reinterpret_cast<void**>(child +`
- :165 **NV** array append helper call (data) (read by hand) — `RtAddr<PushFn>(0x180076900ull)( parent + kElemChildren,`

#### `runtime/src/ui/dialog.cpp`
- :1448 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* const* const children = *reinterpret_cast<void* c`
- :1524 **NV** IBP / owner pointer deref then a FIELD read (no call) — `? *reinterpret_cast<float*>(*reinterpret_cast<uint8_t**`
- :1649 **SM** slot taken from the member (object/VSlot.h) — `hit = reinterpret_cast<Fn>( ::Object::SlotAddressOf(roo`
- :1746 **NV** array / pointer FIELD load at a data offset (no vptr) — `void* child = (*reinterpret_cast<void** const*>(r + 0xa`

#### `runtime/src/ui/dialog_create.cpp`
- :633 **NV** array FIELD load (no vptr) — `void** kids = *reinterpret_cast<void***>(B(insertion, E`

#### `runtime/src/ui/dialog_vtable.cpp`
- :411 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(enc->vta`
- :412 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc,`

#### `runtime/src/ui/documenttextdescription.cpp`
- :311 **BN** a local InternalPtr named slot — `Object::InternalPtr<DocumentTextElement> slot(elem);`

#### `runtime/src/ui/element_vtable.cpp`
- :345 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void* const* const encVt = *reinterpret_cast<void* cons`
- :349 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ScopeFn open = reinterpret_cast<ScopeFn>( encVt[(mode =`

#### `runtime/src/ui/elementdescription_serialize.cpp`
- :175 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(enc->vta`
- :176 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc,`

#### `runtime/src/ui/font.cpp`
- :102 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :303 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`
- :401 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (reinterpret_cast<FactoryFlagFn>(VTableOf(f)[0])(f))`
- :509 **NV** array append helper call (data) (read by hand) — `RtAddr<AppendFn>(0x180076d10ull)( layout + 0x08, *reint`
- :665 **NV** array append helper call (data) (read by hand) — `RtAddr<AppendIntFn>(0x180023830ull)( layout + 0x18, *re`

#### `runtime/src/ui/imagedescription.cpp`
- :204 **BN** a local InternalPtr named slot — `Object::InternalPtr<ImageElement> slot(elem);`

#### `runtime/src/ui/layoutdescription.cpp`
- :114 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :226 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/listdescription.cpp`
- :98 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :161 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/numberdescription.cpp`
- :76 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :119 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/numbereditdescription.cpp`
- :81 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :157 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/progressdescription.cpp`
- :66 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :137 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/ribbondescription.cpp`
- :83 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :143 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/scrolldescription.cpp`
- :87 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :137 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/sliderdescription.cpp`
- :76 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :159 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/spinnerdescription.cpp`
- :74 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :135 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/spritecapture_class.cpp`
- :78 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :179 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`
- :359 **NV** FIELD read through an object / IBP pointer (no call) — `if (*reinterpret_cast<const int32_t*>(q + 0x6c) != 1) {`

#### `runtime/src/ui/spritecontext_flush.cpp`
- :325 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `return reinterpret_cast<Video::DeviceData*>(deviceData)`

#### `runtime/src/ui/spritesheet_vtable.cpp`
- :338 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<FactoryFn>(enc->vta`
- :339 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc,`

#### `runtime/src/ui/stringtable_factory.cpp`
- :130 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `void* VSlotOf(const void* obj, size_t byteOffset) {`
- :131 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `void** vtbl = *reinterpret_cast<void** const*>(obj);`
- :132 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return vtbl[byteOffset / sizeof(void*)];`
- :180 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ser.impl = reinterpret_cast<SerFactoryFn>(VSlotOf(enc, `

#### `runtime/src/ui/tabdescription.cpp`
- :76 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :178 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/tabledescription.cpp`
- :96 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :151 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`

#### `runtime/src/ui/textdescription.cpp`
- :81 **EX** vtable helper whose only users read the IO::Encoding factory - X1 — `return *reinterpret_cast<void***>(o);`
- :158 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[w`
- :177 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `emitTail = !reinterpret_cast<EncPredFn>(evt[0])(stream-`

#### `runtime/src/ui/userinterface.cpp`
- :769 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vtbl = *reinterpret_cast<void***>(backend);`
- :779 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<void (*)(void*, void*, int32_t, int32_`
- :1461 **NV** array / pointer FIELD load at a data offset (no vptr) — `if (*reinterpret_cast<void**>(dlg + 0xa8) != nullptr) {`
- :1487 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(enc);`
- :1488 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `UiCreateRwFn make = reinterpret_cast<UiCreateRwFn>( st-`
- :1519 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(enc);`
- :1520 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `UiCreateRwFn make2 = reinterpret_cast<UiCreateRwFn>( st`

#### `runtime/src/ui/userinterface_close.cpp`
- :323 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** p = *reinterpret_cast<void***>(S + 0x18);`
- :389 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** p = *reinterpret_cast<void***>(S + 0x18);`

#### `runtime/src/ui/userinterface_draw.cpp`
- :632 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const int32_t sh = *reinterpret_cast<int32_t*>(tbl + kS`
- :633 **NV** a data table named table/tbl (entries, attach points, sprite table, StringTable count) — `const int32_t sw = *reinterpret_cast<int32_t*>(tbl + kS`

#### `runtime/src/ui/userinterface_frame.cpp`
- :317 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const addData = *reinterpret_cast<void***>(S + 0`
- :352 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** const reload = *reinterpret_cast<void***>(S + 0x`

#### `runtime/src/ui/widgets_input.cpp`
- :654 **BN** a local InternalPtr named slot — `Object::InternalPtr<DialogInstance> dialogSlot( reinter`
- :850 **BN** a local InternalPtr named slot — `Object::InternalPtr<Element> slot(this);`

#### `runtime/src/ui/widgets_layout.cpp`
- :228 **SM** slot taken from the member (object/VSlot.h) — `const void* const slot = Object::SlotAddressOf(child, &`

#### `runtime/src/video/buffers.cpp`
- :267 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `slot0(companion, 1);`
- :282 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `FactoryFn make = *reinterpret_cast<FactoryFn*>( static_`

#### `runtime/src/video/constant_buffer.cpp`
- :488 **BN** Video::ConstantBufferData member by name (exempt class) — `return Companion(companion)->SlotIndexPlusOne();`

#### `runtime/src/video/context_bind.cpp`
- :698 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `template <int32_t TAG, int N> void FakeSlot(void* self,`
- :706 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `template <int32_t TAG> struct Vt { static void* table[k`
- :707 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `template <int32_t TAG> void* Vt<TAG>::table[kFakeSlots]`

#### `runtime/src/video/device.cpp`
- :464 **BN** Video::*Data member called by name (exempt class) — `return reinterpret_cast<Video::RenderBufferData*>(platf`

#### `runtime/src/video/dynamic_resources.cpp`
- :209 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `template <typename Fn> Fn CompanionSlot(void* companion`
- :210 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `Fn CompanionSlot(void* companion, size_t byteOffset) {`
- :211 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vtable = *reinterpret_cast<void***>(companion);`
- :212 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return reinterpret_cast<Fn>( *reinterpret_cast<void**>(`
- :213 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt`
- :224 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return CompanionSlot<Fn>(deviceData, deviceDataSlot)(de`
- :234 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `CompanionSlot<Fn>(companion, 0)(companion, 1);`
- :422 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `CompanionSlot<Fn>(companion, 0x08)(companion, p->size, `
- :508 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `CompanionSlot<Fn>(companion, 0x08)(companion);`

#### `runtime/src/video/enumerate_video_modes.cpp`
- :103 **BN** Device interface-slot array (data) — `inline InterfaceSlot* SlotAt(Device* self, int i) {`

#### `runtime/src/video/pixelprogram.cpp`
- :279 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :280 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(_data, contex`
- :291 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(dd);`
- :292 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `_data = reinterpret_cast<CreateFn>(vt[ddvt::kCreatePixe`
- :297 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :298 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(_dat`
- :304 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :305 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(_data`
- :316 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :321 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/video/shaderprogram.cpp`
- :194 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `inline void DeleteRw(ReaderWriter* rw) {`
- :203 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :208 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`
- :258 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DeleteRw(rw);`
- :381 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(ser->stream->`
- :383 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `if (!reinterpret_cast<CarriesPlatformDataFn>(encVt[0])(`
- :403 **BN** helper body is AsRw(rw)->DeletingDtor(1): by name, IO::ReaderWriter root — `DeleteRw(rw);`

#### `runtime/src/video/vertexlayout.cpp`
- :233 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(dd);`
- :234 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `_data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreate`
- :241 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :242 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[2])(_data);`
- :248 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :249 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[0])(_data, 1);`
- :257 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :261 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/video/vertexprogram.cpp`
- :331 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :332 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(_data, contex`
- :340 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(dd);`
- :341 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `_data = reinterpret_cast<CreateFn>(vt[ddvt::kCreateVert`
- :347 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :348 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `return reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(_dat`
- :354 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :355 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(_data`
- :367 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :372 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/src/video/video_interfaces_win.cpp`
- :96 **BN** Device interface-slot array (data) — `inline InterfaceSlot* SlotAt(Device* self, int i) {`

#### `runtime/src/video/videoprogram.cpp`
- :171 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(dd);`
- :172 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `_data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreate`
- :177 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :178 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[1])(_data);`
- :184 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `void** vt = *reinterpret_cast<void***>(_data);`
- :185 **EX** Video::*Data companion table (DeviceData / ContextData / *BufferData / *ProgramData ...) - X1 by decision — `reinterpret_cast<Fn>(vt[0])(_data, 1);`
- :200 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `void** encVt = *reinterpret_cast<void***>(stream->encod`
- :204 **EX** IO::Encoding (slot 0 IsBinary; factory CreateReader/Writer +0x18/+0x20, GetKind +0x10) - X1 — `ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slo`

#### `runtime/types/object/ExternalCompiler.h`
- :38 **BN** a member NAMED SlotN in a declaration — `DECLARE_TYPE(ExtensionCompiler) public: virtual void Sl`
- :41 **BN** a member NAMED SlotN in a declaration — `virtual void Slot3() = 0;`
- :50 **BN** a member NAMED SlotN in a declaration — `DECLARE_TYPE(ExternalCompiler) public: virtual void Slo`
- :53 **BN** a member NAMED SlotN in a declaration — `virtual void Slot3() = 0;`
- :54 **BN** a member NAMED SlotN in a declaration — `virtual void Slot4() = 0;`

#### `runtime/types/object/VSlot.h`
- :23 **SM** slot taken from the member (object/VSlot.h) — `template <typename PMF> inline long CompilerSlotOf(PMF `
- :34 **SM** slot taken from the member (object/VSlot.h) — `template <typename PMF> inline void* SlotAddressOf(cons`
- :36 **SM** slot taken from the member (object/VSlot.h) — `const long slot = CompilerSlotOf(pmf);`
- :38 **SM** VSlot.h itself: SlotAddressOf body — `return (*reinterpret_cast<void** const*>(obj))[slot];`

#### `runtime/types/pathfinding/IPathInfo.h`
- :33 **BN** a member NAMED SlotN in a declaration — `public: virtual void Slot0() = 0;`
- :36 **BN** a member NAMED SlotN in a declaration — `virtual void Slot0() = 0;`
- :37 **BN** a member NAMED SlotN in a declaration — `virtual void Slot1() = 0;`

#### `runtime/types/video/CompanionData.h`
- :235 **BN** a member NAMED SlotN in a declaration — `virtual void Slot4() { detail::CompanionPureCall(); }`
- :236 **BN** a member NAMED SlotN (Component/Work), called or declared by name — `virtual float Slot5() { detail::CompanionPureCall(); }`
- :237 **BN** a member NAMED SlotN in a declaration — `virtual bool Slot6() { detail::CompanionPureCall(); }`
- :238 **BN** a member NAMED SlotN in a declaration — `virtual bool Slot7() { detail::CompanionPureCall(); }`
- :506 **BN** a member NAMED SlotN in a declaration — `virtual int32_t SlotIndexPlusOne() {`
- :536 **BN** a member NAMED SlotN in a declaration — `virtual bool Slot1() { detail::CompanionPureCall(); }`
- :698 **BN** a member NAMED SlotN in a declaration — `virtual bool Slot1() { detail::CompanionPureCall(); }`

#### `standalone/cropfield_force_start.cpp`
- :87 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `void** vt = *reinterpret_cast<void***>(node);`
- :88 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `if (!vt // !vt[2]) return;`
- :90 **EX** ControllerComponent::Controller state-node chain (slot 0 FindState) - X1 (statemachine.cpp: dead code) — `reinterpret_cast<char (*)(void*, void*, int, int)>(vt[2`
- :136 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** types = town ? *reinterpret_cast<void***>(town +`
- :156 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** subs = *reinterpret_cast<void***>(e + 0x28);`

#### `standalone/disaster_harness.cpp`
- :203 **NV** harness registry array (data) — `bool Registry(uintptr_t arrOff, uintptr_t cntOff, void*`
- :206 **NV** array / pointer FIELD load at a data offset (no vptr) — `*outArr = *reinterpret_cast<void***>(town + arrOff);`

#### `standalone/dlgctrl_route.cpp`
- :176 **SM** slot taken from the member (object/VSlot.h) — `long SpecSlot(int s) {`
- :178 **SM** slot taken from the member (object/VSlot.h) — `case 0: return ::Object::CompilerSlotOf(&UI::DialogCont`
- :179 **SM** slot taken from the member (object/VSlot.h) — `case 1: return ::Object::CompilerSlotOf(&UI::DialogCont`
- :180 **SM** slot taken from the member (object/VSlot.h) — `default: return ::Object::CompilerSlotOf(&UI::DialogCon`
- :272 **ID** whole-table stub-mode scan / patch at SpecSlot() (member-derived) — `void** vt = *reinterpret_cast<void***>(obj);`
- :284 **ID** whole-table stub-mode scan / patch at SpecSlot() (member-derived) — `++j) if (vt[j] == vt[i]) ++c;`
- :285 **ID** whole-table stub-mode scan / patch at SpecSlot() (member-derived) — `stub = vt[i];`
- :303 **SM** slot taken from the member (object/VSlot.h) — `const long slot = SpecSlot(s);`
- :304 **ID** whole-table stub-mode scan / patch at SpecSlot() (member-derived) — `const bool isStub = (vt[slot] == stub);`
- :310 **ID** whole-table stub-mode scan / patch at SpecSlot() (member-derived) — `if (g_patchMode) vt[slot] = kThunk[s];`

#### `standalone/entity_comp_validator.cpp`
- :93 **SM** slot taken from the member (object/VSlot.h) — `const long typeSlot = ::Object::CompilerSlotOf(&::Objec`
- :97 **SM** slot taken from the member (object/VSlot.h) — `if (!::Object::SlotAddressOf(obj, &::Object::Base::Type`

#### `standalone/food_panel_harness.cpp`
- :270 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`

#### `standalone/food_placement_harness.cpp`
- :240 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vt = *reinterpret_cast<void***>(comp);`
- :247 **SM** slot taken from the member (object/VSlot.h) — `const long shift = ::Object::CompilerSlotOf(&::Object::`
- :254 **SM** slot taken from the member (object/VSlot.h) — `const bool isStub = (vt[compilerSlot(i)] == nullptr);`

#### `standalone/food_production_diag.cpp`
- :213 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** da = *reinterpret_cast<void***>(dlist + 0x28);`

#### `standalone/main.cpp`
- :969 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(swapChain);`
- :976 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `long hr1 = ((GetFullscreenState_t)vt[11])(swapChain, &f`
- :979 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `long hr2 = ((GetDesc_t)vt[12])(swapChain, &desc);`
- :1039 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `void** vt = *reinterpret_cast<void***>(swapChain);`
- :1042 **EX** COM interface (XAudio2 / DirectInput / DXGI swap chain) - not ours — `if (((GetDesc_t)vt[12])(swapChain, &desc) == 0) {`

#### `standalone/menudialogloopt_stubs.cpp`
- :478 **NV** array FIELD load (no vptr) — `const uint32_t id = (*reinterpret_cast<const uint32_t* `
- :480 **NV** array FIELD load (no vptr) — `if (void* o = (*reinterpret_cast<void* const* const*>(_`

#### `standalone/orchard_force_start.cpp`
- :98 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`

#### `standalone/panel_staffing_driver.cpp`
- :154 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`

#### `standalone/scene_watch.cpp`
- :454 **ID** vptr loaded only to print / null-test the object; the call is by name — `void** vt = *reinterpret_cast<void***>(obj);`
- :458 **SM** slot taken from the member (object/VSlot.h) — `const uintptr_t addDrawItems = reinterpret_cast<uintptr`
- :659 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** matInsts = *reinterpret_cast<void***>(g + 0xc8);`
- :672 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *reinterpret_cast<void***>(mt + 0x28);`

#### `standalone/stress_harness.cpp`
- :264 **ID** whole vptr used as a hash key — `++i) if (s_comp[i].vtbl && s_comp[i].calls) {`
- :572 **ID** whole vptr used as a hash key — `if (s.vtbl == vtbl && s.kind == kind) {`
- :573 **ID** whole vptr used as a hash key — `if (!s.vtbl) {`
- :574 **ID** whole vptr used as a hash key — `s.vtbl = vtbl;`

#### `standalone/tool_harness.cpp`
- :1000 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** arr = *reinterpret_cast<void***>(entity + 0x30);`
- :1682 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`
- :1806 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`
- :1913 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`

#### `standalone/trade_buy_harness.cpp`
- :156 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** ctrlArr = *reinterpret_cast<void***>(reinterpret`
- :218 **NV** array / pointer FIELD load at a data offset (no vptr) — `void** tmpls = *reinterpret_cast<void***>(townMgr + 0x1`
