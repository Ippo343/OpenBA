# B5 inventory B: everything OUTSIDE dx11/ that the dx11 seam step touches

Read-only inventory. Paths are repo-relative; `file:line` on every row. Generated build
dependency files (`standalone/build*/obj/*.d`, `tests/unit/build/*.d`) list `dx11_seam.h` /
`VideoDX11Data.h` as dependencies of main.cpp, video_interface.cpp and the dx11 TUs; they are
regenerated and are not rows below.

## 1. Seam machinery users outside dx11/

No hit at all outside dx11/ (code/ runtime/ standalone/ engine/ tests/ tools/ Makefile) for:
`Dx11SeamReady`, `Dx11ForwardCount`, `Dx11LiveVtable`, `kSlots::`, `DX11_REGISTER_SLOT`,
`Dx11VtableAddress`, and tools/ has no reference to any of the 16 names. engine/ has none.

| # | file:line | name | what it does with it |
|---|---|---|---|
| 1 | standalone/main.cpp:18 | `dx11_seam.h` | `#include "dx11_seam.h"` (the only code include of the header outside dx11/ besides row 5) |
| 2 | standalone/main.cpp:547 | `VideoDX11::Dx11SeamReport()` | report call in the crash-handler tail, just before `CrashLogf("=== END CRASH ===\n")` |
| 3 | standalone/main.cpp:1249 | `VideoDX11::Dx11SeamInstall()` | THE install call, boot step 4b, "must run before Engine::Startup" |
| 4 | standalone/main.cpp:1243-1255 | `Dx11SeamInstall`, `VideoInterfaceFillDx11Slot`, "seam" | comment block around row 3 (`VideoDX11-x64.dll is never loaded, so the seam's vtables are our own`); `StallDebug::Mark("dx11 seam")` at :1255 |
| 5 | standalone/video_interface.cpp:50 | `dx11_seam.h` | include |
| 6 | standalone/video_interface.cpp:56-64 | `VideoDX11::Dx11_CreateDeviceData`, `Dx11_GetDeviceName`, `Dx9Placeholder_CreateDeviceData`, `Dx9Placeholder_GetDeviceName`, `struct VideoDX11::DeviceData` fwd decl | local forward declarations of dx11/src/exports.cpp and devicedata.cpp functions (a local `extern` in the CLAUDE.md sense; `DeviceData` here is a forward decl of an incomplete `VideoDX11::DeviceData`) |
| 7 | standalone/video_interface.cpp:110 | `VideoInterfaceFillDx11Slot` | definition, forwards to `FillDx11Slot` |
| 8 | standalone/video_interface.cpp:116-123 | `VideoInterfaceFillDx9Placeholder`, `Dx9Placeholder_GetDeviceName` (:120), `Dx9Placeholder_CreateDeviceData` (:121) | definition: writes Device `dllSlots[kInterfaceDX9]` (module = sentinel, the two fn pointers); :118/:119/:121 carry `CAST-OK: dx11_seam` |
| 9 | standalone/video_interface.cpp:129-141 | `Dx11_GetDeviceName` (:132, :140), `Dx11_CreateDeviceData` (:133, :141) | `FillDx11Slot` body: writes `dllSlots[kInterfaceDX11]`; :130/:131/:133 carry `CAST-OK: dx11_seam`; prints the pointers |
| 10 | standalone/video_interface.cpp:148 | `VideoInterfacePrefilled` | definition, returns `g_filled` |
| 11 | standalone/video_interface.h:6 | `VideoInterfacePrefilled` | declaration |
| 12 | standalone/video_interface.h:15 | `VideoInterfaceFillDx11Slot` | declaration |
| 13 | standalone/video_interface.h:18 | `VideoInterfaceFillDx9Placeholder` | declaration |
| 14 | runtime/src/video/video_interfaces_win.cpp:41 | `VideoInterfaceFillDx11Slot` | banner comment (ported `Video::Device::InitializeInterfaces` calls it) |
| 15 | runtime/src/video/video_interfaces_win.cpp:73-74 | `VideoInterfaceFillDx9Placeholder` | file-scope `void VideoInterfaceFillDx9Placeholder(void* device);` declaration (a local extern: file-scope, not in a header; comment at :71-73 explains why not block scope) |
| 16 | runtime/src/video/video_interfaces_win.cpp:223 | `VideoInterfaceFillDx11Slot(this)` | first statement of `Video::Device::InitializeInterfaces`; declaration comes via standalone/video_interface.h |
| 17 | runtime/src/video/video_interfaces_win.cpp:270-274 | `::VideoInterfaceFillDx9Placeholder(this)` | CUTOVER-ONLY: called when `dllSlots[0].module == nullptr`; comment at :264-269 |
| 18 | runtime/src/video/context.cpp:189, :627 | `VideoDX11::ContextData` | comments only ("the object behind the pointer is a `VideoDX11::ContextData` wearing dx11's vtable"; `Backend(this)` forwarders) |
| 19 | runtime/types/video/CompanionData.h:46, :139, :144, :164-165, :790 | `VideoDX11Data.h`, `dx11_seam.h` (84-slot table), `Dx11LiveVtable`, `DX11_REGISTER_SLOT`, `Dx11SeamInstall`, `dx11_seam.cpp` | comments only (banner + the B3X X1 exemption comment above the `has_virtual_destructor` asserts) |
| 20 | runtime/types/video/Buffers.h:11 | `VideoDX11Data.h` | comment |
| 21 | runtime/types/video/VertexLayout.h:11, :55 | `dx11_backend_access.h` (`VertexLayoutOwner`) | comments (layout evidence) |
| 22 | runtime/types/video/ImageBuffer.h:426 | `dx11_backend_access.h` (`ImageBufferOwner`) | comment |
| 23 | runtime/types/video/Device.h:747 | `dx11_backend_access.h` (`RenderBufferOwner`) | comment |
| 24 | runtime/src/video/companion_data.cpp:175 | `VideoDX11Data.h` | comment (Lock/Unlock trampolines reach `dx11::IndexBufferData` / `VertexBufferData`) |
| 25 | runtime/src/video/buffers.cpp:15 | `VideoDX11Data.h` | comment |
| 26 | runtime/src/save/saveload_draw.cpp:770 | `VideoDX11Data.h` | comment ("slot INDEX 6, which dx11/types/VideoDX11Data.h already names") |
| 27 | tests/unit/video_imagebuffer_abi_test.cpp:16, :89, :114 | `dx11_backend_access.h`, "`_data` at +0x60 ... dx11 reads" | comments on the `ImageBuffer::_data` offset pin (the pin itself is :120-122, see section 2) |
| 28 | Makefile:43-45 | `DX11_FUNC_SRCS = $(wildcard dx11/funcs/*.cpp)`, `DX11_SRC_SRCS = $(wildcard dx11/src/*.cpp)` | wildcards: deleting dx11_seam.cpp needs no edit |
| 29 | Makefile:80 | `-Idx11/types -Idx11/funcs -Istandalone` | include path: `dx11/funcs` becomes an empty dir if the seam dir goes; main.cpp/video_interface.cpp need `dx11_seam.h` gone first |
| 30 | Makefile:270-274 | `$(SA_OBJ_DIR)/dx11__funcs__%.o`, `dx11__src__%.o` | pattern rules (`dx11__funcs__dx11_seam.o` is the seam object) |
| 31 | tests/unit/sources.mk:144-148, :236-240, :250-258 | comments | say the twelve `Video::*Data` bodies / `ContextData` forwarders "stand on dx11's live vtables" and so no tree source links (video_data_abi_test, video_device_abi_test, video_context_abi_test) |

`CAST-OK: dx11_seam` markers outside dx11/ (the FLOOR category `dx11_seam`, section 6):
12 in runtime/types/video/CompanionData.h (:247, :307, :362, :416, :468, :508, :562, :595,
:629, :661, :693, :729), 2 in runtime/types/video/Context.h (:322, :329), 6 in
standalone/video_interface.cpp (:118, :119, :121, :130, :131, :133). 20 in all, which is the
FLOOR ceiling for the category.

Context.h:318-329 also holds the two `void*` overloads "B4 FORWARD: dx11/src/contextdata.cpp
hands its dynamic vertex buffer / default render buffer as a `void*`":
`SetVertexBuffer(const void* vb, ...)` (:321) and `SetRenderBuffer(const void* rb)` (:328),
each a `static_cast` back to the typed pointer. They are called from dx11/ only and exist
because dx11's structs are not the typed classes.

## 2. DeletingDtor / exemption pins on the twelve Video::*Data

### 2a. Declarations and static_asserts (runtime/types/video/CompanionData.h)

| file:line | what |
|---|---|
| CompanionData.h:180-195 | banner "NO VIRTUAL DESTRUCTOR (ENDGAME B.1 decision 2)": `~XData()` PUBLIC non-virtual, "must NOT become `DeletingDtor(0)`", vector arm marked `dx11_seam` "goes with the seam, plan 4.1" |
| CompanionData.h:244, :304, :359, :413, :465, :505, :559, :592, :626, :658, :690, :726 | the twelve `virtual void* DeletingDtor(unsigned flags)` slot-0 bodies, in class order Device(235), Context(295), ImageBuffer(350), IndexBuffer(404), VertexBuffer(456), ConstantBuffer(496), RenderBuffer(550), VertexLayout(583), VertexProgram(617), PixelProgram(649), RenderState(681), VideoProgram(717). Each has a scalar arm (`this->~XData()`, `InternalMemFree(this)`) and a vector arm (flags & 2: element count read from the cookie at `this - 8`, `this[i].~XData()` downward, free the cookie block) |
| CompanionData.h:247, :307, :362, :416, :468, :508, :562, :595, :629, :661, :693, :729 | the twelve `static_cast<int32_t*>(block) - 2  // CAST-OK: dx11_seam` cookie casts (the vector arm only exists for the hand slot 0) |
| CompanionData.h:288, :343, :397, :449, :489, :543, :576, :610, :642, :674, :710, :757 | the twelve PUBLIC non-virtual `~XData()` declarations (`~DeviceData()` :288, ... `~VideoProgramData()` :757) |
| CompanionData.h:258-286 | DeviceData slots 1-19 (EnumerateInterfaces ... CreateRenderBufferData), each a `detail::CompanionPureCall()` body. RetailSlot n >= 1 moves to compiler slot n + 1 at the flip |
| CompanionData.h:757-785 | `RT_VIDEO_COMPANION_PINS` x12 (`sizeof == 0x10`, `offsetof(_owner) == 0x08`). The 0x10 size survives the flip (the vptr is still the only polymorphic word) |
| CompanionData.h:787-828 | THE exemption pins: twelve `static_assert(!std::has_virtual_destructor<XData>::value, "... exempt from the normal-virtual cutover ...")` at :793 DeviceData, :796 ContextData, :799 ImageBufferData, :802 IndexBufferData, :805 VertexBufferData, :808 ConstantBufferData, :811 RenderBufferData, :814 VertexLayoutData, :817 VertexProgramData, :820 PixelProgramData, :823 RenderStateData, :826 VideoProgramData. The only `has_virtual_destructor` asserts on them anywhere |

### 2b. DeletingDtor mentions naming Video / companion classes (tree-wide)

| file:line | role |
|---|---|
| runtime/src/video/video_resources.cpp:261, :283 | `~Device` step: `if (dd) (void)dd->DeletingDtor(1)` on the `DeviceData` (section 3 row) |
| runtime/src/video/videoprogram.cpp:160 | `_data->DeletingDtor(1)` |
| runtime/src/video/vertexprogram.cpp:328 | `_data->DeletingDtor(1)` |
| runtime/src/video/vertexlayout.cpp:226 | `_data->DeletingDtor(1)` |
| runtime/src/video/pixelprogram.cpp:226 | `_data->DeletingDtor(1)` |
| runtime/src/video/renderstate.cpp:40-41, :192 | comment on the virtual spelling; `_data->DeletingDtor(1)` (:192) |
| runtime/src/video/enumerate_video_modes.cpp:109, :138 | `(void)data->DeletingDtor(1)` on the temporary DeviceData |
| runtime/src/video/dynamic_resources.cpp:168 | `(void)companion->DeletingDtor(1)` |
| runtime/src/video/device_startup.cpp:531, :536, :606, :627, :706 | `dd->DeletingDtor(1)` x3 (:536, :627, :706) plus comments |
| runtime/src/video/context_lifecycle.cpp:107, :221, :338 | comment; `companion->DeletingDtor(1)` (:221); `self->_contextData->DeletingDtor(1)` (:338) |
| runtime/src/video/buffers.cpp:355, :375, :392, :423 | `companion->DeletingDtor(1)` x4 (ImageBuffer/IndexBuffer/VertexBuffer/RenderBuffer failure paths) |
| runtime/src/engine/mainloop_video.cpp:543 | `payload->DeletingDtor(1u)` on `ImageBuffer::_data` (OnVideoError) |
| runtime/src/save/saveload_update.cpp:1703 | `data->DeletingDtor(1u)` on `ImageBuffer::_data` (SaveLoad::OnVideoError) |
| runtime/src/video/README.md:77 | prose: "`DeletingDtor(unsigned)`, the class's own member (ENDGAME B.3.9 deleted the ...)" |
| runtime/types/video/ShaderProgram.h:504 | `constexpr size_t kDeletingDtor = 0x00;` in the `programdata_vtbl` namespace (a byte-offset name; check whether any caller is left) |
| runtime/types/video/RenderState.h:78, :91 | comments naming `DeletingDtor()` on `RenderStateData` and `vtable for Video::RenderState` slot 0 |
| runtime/types/video/ImageBuffer.h:74 | comment, `vtable for Video::ImageBuffer` slot 0 (`DeletingDtor`) is a table row (the OWNER class, not the companion) |
| tests/unit/b39_vid_data_test.cpp:53, :61, :72 | `d->DeletingDtor(0)`, `p->DeletingDtor(1)`, `e[0]->DeletingDtor(3)` (section 2c) |
| tests/unit/video_data_abi_test.cpp:122, :139, :194, :224, :310 | prose + the three Mirror classes' `virtual ... DeletingDtor(unsigned)` (section 2c) |
| dx11/src/** | NOT outside dx11/: the 12 `*_ScalarDeletingDtor` bodies, the `DX11_REGISTER_SLOT(..., 0, ...)` rows (imagebufferdata.cpp:754, renderstatedata.cpp:540, buffers.cpp:911/926/941), `Dx9P_DeletingDtor` (devicedata.cpp:1779), the "must NOT be `DeletingDtor(0)`" comments (inventory A covers them) |

NOT OURS (one line): `DeletingDtor` also names Toolbar/TerrainGenerator/Profession/ProfessionList/MapGenerator/NameDatabase/MusicList/MenuScene/ClimateGenerator (code/Game/Game), the Controller `ScalarDeletingDtor` rows (code/Game/Component/*), Material/Surface/CollisionMesh/ExternalList/Stream/Serializer/IO::Binary* classes, Input::KeyMap (`RtInput_KeyMap_DeletingDtor`), Engine::Application, Object::Internal/External, Thread (none), PickingMesh/PointList/IPathInfo notes, and tests/unit/object_vtable_abi_test.cpp:19/138/262/322 (`FakeRetailShape`, a class-shape probe for the Object family).

### 2c. Tests that pin a Video::*Data slot index, size or vtable

| file:line | asserts | what must change when slot 0 becomes `virtual ~XData()` |
|---|---|---|
| tests/unit/b39_vid_data_test.cpp:6-10, :41 | `CHECK(!std::has_virtual_destructor<T>::value)` for all 12 via `CheckData<T>()` (:96-107) | INVERT to `has_virtual_destructor == true`; header comment (:6, "no virtual destructor, the destructor PUBLIC ... RULE B5-3") rewritten |
| b39_vid_data_test.cpp:42 | `std::is_destructible<T>` "PUBLIC: RULE B5-3" | stays true (public virtual dtor) but the rationale (qualified `Video::XData::~XData()` from a non-derived dx11 struct) is gone |
| b39_vid_data_test.cpp:44 | `CHECK_DTOR_SLOT(T)` -> `DtorSlotOk`: `HasDeletingDtor<T>` branch requires `CompilerSlot(&T::DeletingDtor) == 0 && !has_virtual_destructor` | After the flip `&T::DeletingDtor` no longer exists, so `DtorSlotOk` takes the `else` arm and requires `has_virtual_destructor` (slot_check.h:111-118): no edit to the macro, it follows the type |
| b39_vid_data_test.cpp:47-77 | scalar/vector arms of `DeletingDtor(0/1/3)` over placement-new'd objects, a `g_dtorCount` marker dtor defined in the test (:81-94) | The test calls `d->DeletingDtor(...)`, which disappears. Needs re-expression through `dtor_check.h` (`CallD0`, `NewOnMalloc`, `g_frees`; used by video_data_abi_test.cpp:41, :449-458) or `delete`/placement-dtor; the `flags & 2` vector arm and the `this - 8` cookie have no virtual-dtor counterpart (MSVC vector dtor is compiler array-delete on the real class) and this test's flags 3 case goes away |
| b39_vid_data_test.cpp:82-93 | out-of-line `~XData()` bodies defined as markers in the test (companion_data.cpp cannot link into the host test) | Stay needed if the dtor stays out of line in companion_data.cpp (or become inline defaults) |
| b39_vid_data_test.cpp:112-127 | `CHECK_SLOT(D, EnumerateInterfaces, 0x08/8)`, `SetImageBuffer 0x40/8`, `CreateContextData 0x48/8`, `CreateConstantBufferData 0x58/8`, `CreateVertexLayoutData 0x70/8`, `CreateVertexProgramData 0x78/8`, `CreatePixelProgramData 0x80/8`, `CreateRenderStateData 0x88/8`, `CreateVideoProgramData 0x90/8`, `CreateRenderBufferData 0x98/8`; `CHECK_SLOT(C, BeginCommands, 0x08/8)`, `DrawIndexed 0x60/8`, `EndInstancedIndexedVertices 0x98/8` | NO edit to the numbers: `CHECK_SLOT` is written in retail numbers and `RetailSlotOfPmf` adds 1 when `has_virtual_destructor` and n >= 1 (slot_check.h:49-51, :88-93). They start passing only because the type flips; they fail if the base flips and a dx11 class does not match |
| tests/unit/video_data_abi_test.cpp:75-114 | `sizeof(Video::XData) == sizeof(void*) * 2` x12 (:84-95), `offsetof(XData, _owner) == 0x08` x12 (:97-108), `sizeof(RtVideo::CompanionData) == sizeof(Video::DeviceData)` (:113) | Unchanged (a virtual dtor adds no data member). `offsetof` on a polymorphic class is already used |
| video_data_abi_test.cpp:116-132 | prose: "Slot 0 is `DeletingDtor(unsigned)` ... ordinary virtual, NOT a destructor ... B3X W1 kept this shape ... exempt by decision" | Rewrite |
| video_data_abi_test.cpp:137-163, :192-205, :222-231 | `MirrorDeviceData` (20 slots), `MirrorIndexBufferData` (7), `MirrorVertexLayoutData` (3): slot 0 `virtual MirrorX* DeletingDtor(unsigned)`, `protected: ~MirrorX() = default` NON-virtual; `ProbeX final : MirrorX` | These mirror the CLASS SHAPE. After the flip each mirror's slot 0 becomes `virtual ~MirrorX()` (protected) and every raw `SlotAt<...>(d, 0xNN)` offset below must go through `RetailByteOffset<T>` / `RetailEntry` instead of the literal `0x08..0x98` |
| video_data_abi_test.cpp:243-299 | `video_companion_data_slot_order`: `sizeof(Mirror*) == sizeof(Video::*)` (:247-249), `SlotAt<..., MirrorDeviceData>(d, 0x00)(d, 1)` slot 0 call with flags (:256), slots 1-8 at +0x08..+0x40 (:258-267), the eleven factories `slot * 8` for 9..19 (:272-276), IndexBufferData +0x18/+0x20/+0x28/+0x30 (:287-292), VertexLayoutData +0x10 (:297) | Slot 0: a deleting dtor reached by raw byte offset 0 is no longer the `D0(flags)` shape (D1 at compiler slot 0, D0 at 1, `dtor_check.h` reads it). Every other offset shifts +8. The test already includes `dtor_check.h` (:41) and uses `RetailSlotFn`-style helpers elsewhere in the file |
| video_data_abi_test.cpp:301-330 | PART C: qualified non-virtual call (`MirrorQualBase::BaseOnly`, `p->Base::Method()` on a derived overriding it); comment :303-311 ties it to RULE B5-3 / `~XData()` | Keep only as a codegen proof if wanted; the dx11 destructor reason disappears |
| tests/unit/slot_check.h:11-14, :111-118 | header comment says the `Video::*Data` seam classes "has no virtual destructor and is not shifted"; `HasDeletingDtor` / `DtorSlotOk` handle both shapes | No logic change (decides by `has_virtual_destructor`); update comment at :11-14. `HasDeletingDtor` branch becomes dead for the tree |
| tests/unit/slot_check_test.cpp:6-7, :27-34, :48, :53-93 | `WithDeletingDtor` / `DeletingDtorLeaf` MIRROR shape ("the exempt *Data shape") proving `RetailSlot` does not shift without a virtual dtor | Test is of the helper, not the tree; stays valid. Header comment :6-7 ("since B3X W1 that is only the twelve Video::*Data classes") becomes false |
| tests/unit/object_vtable_abi_test.cpp:19, :138, :262, :322 | `FakeRetailShape` has `DeletingDtor(unsigned)` and no virtual dtor; `CHECK_EQ(CompilerSlot(&FakeRetailShape::DeletingDtor), 0L)` | Object-family shape probe, not a Video class; independent |
| tests/unit/video_imagebuffer_abi_test.cpp:120-122 | `is_same<decltype(Video::ImageBuffer::_data), Video::ImageBufferData*>` | unchanged |
| video_imagebuffer_abi_test.cpp:407-409 | `is_same<decltype(Video::RenderState::_data), Video::RenderStateData*>` | unchanged |
| video_imagebuffer_abi_test.cpp:434-435 | `has_virtual_destructor<Video::RenderState>` / `<Video::ImageBuffer>` == true | owners, not companions: unchanged |
| tests/unit/rb_video_abi_test.cpp:127, :159 | `has_virtual_destructor<Video::ImageBuffer>` | owner; unchanged |
| tests/unit/video_program_abi_test.cpp:379-383 | `has_virtual_destructor` for ShaderProgram, VertexProgram, PixelProgram, VertexLayout, VideoProgram | owners; unchanged |
| tests/unit/video_device_abi_test.cpp:249 | `d->deviceData = reinterpret_cast<Video::DeviceData*>(0xDEADBEEF)` writes a Device field | unchanged (pointer type) |
| tests/unit/video_context_abi_test.cpp:15, :71, :181, :297 | comments only about dx11 `Ctx::` / `ConstantBufferOwner` offsets | none |
| tests/unit/graphics_geometry_abi_test.cpp:718-724, tests/unit/graphics_particle_abi_test.cpp:483-490 | comments "RULE B5-3's `self->Video::XData::~XData()` is the same shape" for qualified-base calls | comment cleanup only |
| tests/unit/sources.mk:144-148, :236-240, :250-258 | explain the three video ABI tests link no tree source because the bodies "stand on dx11's live vtables" | if `dx11/` classes derive from the Runtime base, the host test still cannot link dx11 (D3D11, <windows.h>); sentences may need a wording pass |

## 3. Runtime callers of the companion virtuals (and creation / destruction)

All runtime/src/video unless noted. Members are on the Runtime abstract base; today the call goes through dx11's hand table.

| file:line | receiver | member called |
|---|---|---|
| video_resources.cpp:196 | `platformData` (RenderBuffer) | `Bind(&context)` slot 2 |
| video_resources.cpp:230 | `dd` (`Device::deviceData`) | `CreateImageBufferData(this)` (slot 10) |
| video_resources.cpp:283 | `dd` | `DeletingDtor(1)` (~Device) |
| videoprogram.cpp:150, :155, :160 | `dd`, `_data` | `CreateVideoProgramData(this)`; `_data->Slot1()`; `_data->DeletingDtor(1)` |
| vertexprogram.cpp:309, :318, :323, :328 | `_data`, `dd` | `Bind(context, permutation)`; `CreateVertexProgramData`; `PlatformCreate()`; `DeletingDtor(1)` |
| vertexlayout.cpp:215, :221, :226 | `dd`, `_data` | `CreateVertexLayoutData`; `PlatformCreate()`; `DeletingDtor(1)` |
| pixelprogram.cpp:206, :217, :221, :226 | `_data`, `dd` | `Bind(context, permutation)`; `CreatePixelProgramData`; `PlatformCreate()`; `DeletingDtor(1)` |
| renderstate.cpp:110, :179, :182, :187, :192, :252 | `ActiveDeviceData()` (:110 = `Device::_singleton->deviceData`), `_data` | `CreateRenderStateData(this)`; `Create()`; `DeletingDtor(1)`; `Bind(context)` |
| context_bind.cpp:254, :257, :292, :304, :308, :317, :322, :335, :340, :351, :359, :361, :367, :371, :373, :392 | `img->_data`, `_contextData`/`cd`, `_vertexProgram->_data`, `_pixelProgram->_data`, `vb->platformData`, `_vertexLayout->_data`, `ib->platformData`, `_renderState->_data`, `u.buf->_platformData` | `ImageBufferData::Bind(this, i)`; `ContextData::BindTextureNone(i)`, `BindRenderBuffer`, `BindTransforms`, `BindVertexNone(i)`, `BindIndexNone`, `BindViewport`, `BindScissor`; `VertexProgramData/PixelProgramData::Bind(this, detail)`; `VertexBufferData::Bind(this, ...)`; `VertexLayoutData::Bind(this)`; `IndexBufferData::Bind(this)`; `RenderStateData::Bind(this)`; `ConstantBufferData::Bind(this, src, 0, len)` |
| constant_buffer.cpp:389, :397, :424, :455 | `companion` / `_platformData` | `Create()` (tail JMP +0x08); `Create()`; `SlotIndexPlusOne()` (slot 3); `Bind(&context, data, first, count)` |
| context_ctor.cpp:207, :230, :373, :422, :423 | `factory` = `device->deviceData`, `p->platformData`, `color->_data`, `depth->_data` | `CreateConstantBufferData(buf)`; `IndexBufferData::Create(bytes, 1, nullptr)`; `CreateContextData(self)`; `AttachColorBuffer()`, `AttachDepthBuffer()` |
| context_lifecycle.cpp:221, :338 | `companion`, `self->_contextData` | `DeletingDtor(1)` (both) |
| context.cpp:199, :648-676 | `Backend(this)` = `_contextData` | `DrawIndexed`, `GetMaxDynamicInstances`, `BeginVertices`, `EndVertices`, `BeginIndexedVertices`, `EndIndexedVertices`, `BeginInstancedIndexedVertices`, `EndInstancedIndexedVertices` |
| device.cpp:355 | `platformData` | `Slot1()` (RenderBufferData) |
| device_startup.cpp:533, :536, :609, :625, :627, :703, :706 | `deviceData`/`dd`/`d2` | `Shutdown()`, `DeletingDtor(1)`, `CreateDevice()` |
| enumerate_video_modes.cpp:123, :131, :138 | `data = create(this)` (the plugin's `CreateDeviceData` fn pointer) | `EnumerateInterfaces()`, `DeletingDtor(1)`. THE ONLY place a DeviceData is created and destroyed by Runtime code without keeping it |
| buffers.cpp:313, :317, :339, :347, :355, :367, :375, :384, :392, :415, :423, :465, :472, :476, :481 | `_data` (ImageBuffer), `platformData` (Index/Vertex/RenderBuffer), `dd` | `ImageBufferData::Update/Read/Create`; `CreateImageBufferData`, `CreateIndexBufferData`, `CreateVertexBufferData`, `CreateRenderBufferData`; `DeletingDtor(1)` x4; `Create(count, ...)`, `SetData(...)` |
| companion_data.cpp:191, :195, :199, :203 | `platformData` | `Lock(sizeBytes, &outByteOffset)` / `Unlock()` (Index/VertexBuffer, slots 3/4) |
| dynamic_resources.cpp:168, :254-266, :326-330, :366-370, :419-423 | `companion`, `dd` | `DeletingDtor(1)`; `CreateImageBufferData` then `AttachColorBuffer()` / `AttachDepthBuffer()` / `Create(nullptr)`; `CreateIndexBufferData` then `Create(size, 1, nullptr)`; `CreateVertexBufferData` then `Create(size, 1, nullptr)`; `CreateRenderBufferData` then `Slot1()` |
| engine/mainloop_video.cpp:541-545 | `obj->_data` (ImageBuffer) | `DeletingDtor(1u)`, then `obj->_data = nullptr` |
| save/saveload_update.cpp:1699-1705 | `img->_data` | `DeletingDtor(1u)`, then `_data = nullptr` |
| save/saveload_draw.cpp:615-618, :776 | `ctx->_contextData`, `target->_data` | `ContextData::Clear(1u, 0xffff6000u, 1.0f, 0)`; `ImageBufferData::Read(buffer, w, h)` |
| save/saveload_saveinfo.cpp:237 | `img->_data` | `Update(pixels, w, h)` |
| graphics/wave23_lane_gfxbody.cpp:261, :574, :616 | `image->_data`, `_data`, an `->Resolve(context, _sourceTarget, true)` receiver | `AttachColorBuffer()`; `Create(levels)`; `RenderBufferData::Resolve` |
| graphics/edgemesh_vtable.cpp:123, :126; graphics/graphicsmesh_vtable.cpp:376-379 | `buf.platformData`, `level->_indexBuffer/_vertexBuffer/_skinIndexBuffer/_skinVertexBuffer.platformData` | `Serialize(&ser)` x6 |
| graphics/sunlight.cpp:143; graphics/renderer.cpp:206; graphics/lights_vtable.cpp:574 | `Video::Device::_singleton->deviceData` | `Slot6()`, `Slot7()`, `Slot7()` |
| graphics/lights_vtable.cpp:459 (comment), graphics/meshinstance_draw.cpp:203 (comment), graphics/wave27_lane_gfxpart.cpp:669-678 (comment) | `_contextData` | slot 4 `Clear`, slot 12 `DrawIndexed`, slots 18/19 instanced draw; documented as raw vtable reads in comments (check for live raw reads: the 3 hits are comments) |
| engine/loops.cpp:710, :992-993, :1011-1012, :1051, :1099, :1111 | `ctx->_contextData` (`cd`, `cd2`, `cd3`), `owner->deviceData` | `Slot4(true)` / `Slot4(false)` on DeviceData; `BeginCommands()`, `EndCommands()`, `Finish()` on ContextData |
| ui/userinterface.cpp:684-692 | `Video::Device::_singleton->deviceData` | `SetImageBuffer(image, hotX, hotY)` (cursor) |
| ui/spritecontext_flush.cpp:294-305, :336, :382-383, :504-506 | `device->deviceData`, `_context->_contextData` (`cd`, `cd2`) | `Slot5()` (half pixel, returns 0.0f); `BeginVertices(n * vertsPer, 0x20)`; `EndVertices(prim)` |
| code/Game/Player/tool_base_tilerender.cpp:209, :293 | `ctx->BeginVertices` / `ctx->EndVertices` | these are `Video::Context` members (owner class), not the companion; they forward through `Backend(this)` |

### CREATED

| file:line | what |
|---|---|
| runtime/src/video/enumerate_video_modes.cpp:123 | `DeviceData* data = create(this)`: `create` is `InterfaceSlot::createDeviceData`, i.e. dx11's `Dx11_CreateDeviceData` (via standalone/video_interface.cpp:133 / :121); the only plugin-factory call besides CreateVideoInterface |
| runtime/src/video/device_startup.cpp (CreateVideoInterface; `deviceData = ...createDeviceData(this)`) | the live DeviceData; destroyed at :536, :627, :706 |
| runtime/src/video/video_resources.cpp:230 | `dd->CreateImageBufferData(this)` (ImageBuffer init) |
| runtime/src/video/buffers.cpp:347, :367, :384, :415 | `Create{ImageBuffer,IndexBuffer,VertexBuffer,RenderBuffer}Data(this)` |
| runtime/src/video/videoprogram.cpp:150; vertexprogram.cpp:318; vertexlayout.cpp:215; pixelprogram.cpp:217; renderstate.cpp:182 | `Create{VideoProgram,VertexProgram,VertexLayout,PixelProgram,RenderState}Data(this)` |
| runtime/src/video/context_ctor.cpp:207, :373 | `CreateConstantBufferData(buf)`, `CreateContextData(self)` |
| runtime/src/video/dynamic_resources.cpp:256, :327, :367, :420 | `CreateImageBufferData(p)`, `CreateIndexBufferData(p)`, `CreateVertexBufferData(p)`, `CreateRenderBufferData(this)` (device-lost Recreate paths) |
| runtime/src/video/companion_data.cpp:115-133 | the twelve ctor DOORS `Video_XData_Ctor(CompanionData* self, void* owner)` (`::new (self) Video::XData(owner)`), `RT_COMPANION_CTOR` x12; dx11 calls them through its import struct (AllocData in devicedata.cpp, exports.cpp). Nothing in runtime/ or code/ calls them; no `delete` / placement-new of a `Video::*Data` anywhere in code/ or runtime/ outside this file |

No `CreateXData` caller exists in code/ or standalone/. No `delete` of a companion exists outside the DeletingDtor calls above.

### DESTROYED (all `DeletingDtor(1)`)

video_resources.cpp:283; videoprogram.cpp:160; vertexprogram.cpp:328; vertexlayout.cpp:226; pixelprogram.cpp:226; renderstate.cpp:192; enumerate_video_modes.cpp:138; dynamic_resources.cpp:168; device_startup.cpp:536/:627/:706; context_lifecycle.cpp:221/:338; buffers.cpp:355/:375/:392/:423; engine/mainloop_video.cpp:543; save/saveload_update.cpp:1703. Eighteen call sites; every one is a flag-1 SCALAR delete (no flags 2 / 3 caller exists in the tree, so the VECTOR arm in CompanionData.h and in the three tests is reached by nothing but b39_vid_data_test.cpp). Nulling after: mainloop_video.cpp:545, saveload_update.cpp:1705, device_startup.cpp (`deviceData = nullptr`).

### companion_data.cpp twelve `~XData()` bodies (runtime/src/video/companion_data.cpp:150-161)

All twelve are empty `{}`: `RenderBufferData`, `VideoProgramData`, `PixelProgramData`, `VertexProgramData`, `VertexLayoutData`, `VertexBufferData`, `IndexBufferData`, `ConstantBufferData`, `ContextData`, `DeviceData`, `ImageBufferData`, `RenderStateData`. They do nothing (the retail body is only the base-vptr re-stamp, which is the compiler's entry store; `_owner` at +0x08 deliberately left). The block comment :135-147 states the base "owns no memory; the derived dx11 object's slot 0 does the MemFree and calls this as its base-destructor step (RULE B5-3)". The tests (b39_vid_data_test.cpp:81-94) define their own marker versions because companion_data.cpp does not link into the host test.

## 4. Raw reach-ins into dx11 state from outside dx11/

The 7 casts (LEFT_FREE.md section C counts four in main.cpp because two casts each on :879 and :951 are listed twice):

| # | file:line | exact text |
|---|---|---|
| 1 | standalone/main.cpp:879 | `    IDXGISwapChain* swapChain = *reinterpret_cast<IDXGISwapChain**>((uintptr_t)activeDD + 0x18);   // LEFT: OTHER dx11 seam: the swap chain is a field (+0x18) of the DX11 DeviceData subclass, not of Video::DeviceData (plan section 4.1)` (two casts: `reinterpret_cast` and `(uintptr_t)`). Context: `SwapProbe` function, `::Video::DeviceData* activeDD = device->deviceData;   // +0x100` at :872; comment :878 "the dx11 seam (FLOOR dx11_seam, plan section 4.1)"; the error message at :881 reads `swapChain NULL (activeDD+0x18)` |
| 2 | standalone/main.cpp:951 | identical text to row 1, in `CaptureBootFullscreenMode()`; `activeDD` at :947; comment :950 |
| 3 | standalone/boot_window.cpp:49 | `    static_cast< ::Video::Configuration*>(videoConfig)->field42 = 1;   // +0x42 (kCfgSkipShow)  // LEFT: dx11 seam (devicedata.cpp hands its cfg as void*)` (comment line above it, :48: `// LEFT: dx11/src/devicedata.cpp (dx11 seam) hands the configuration as void*.`). This is the argument of `ArmDeferredShow(void* videoConfig, HWND hwnd)` (boot_window.cpp:38), called from dx11/src/devicedata.cpp |
| 4 | code/Game/Dialog/optionsdialog.cpp:492 | `        comboAdapter->AddItem(sprite, *reinterpret_cast<const System::String*>(&gfxIface.adapterData[i].name), itemData);   // LEFT: dx11 seam: Device::Adapter::name as System::String gives the record an implicit String ctor / dtor, and dx11/src/devicedata.cpp builds these records by hand through the RtVideo_Adapter / RtVideo_GraphicsInterface Ctor / Dtor doors (its `GraphicsInterface gi;` local, :1548-1560, would release the name twice) -- the doors become the structs' C++ ctor / dtor with that dx11 edit` (the tool output elides the end; read from `sed -n 492p`). Comment :489 `(LEFT: Adapter::name is declared as the String's data pointer, H5)` |
| 5 | code/Game/Dialog/optionsdialog.cpp:520 | `        comboRenderer->AddItem(sprite, *reinterpret_cast<const System::String*>(&gfxIface.name), itemData);   // LEFT: dx11 seam: as the adapter line above (Device::GraphicsInterface::name; dx11/src/devicedata.cpp :1548-1560 builds the record through RtVideo_GraphicsInterface_Ctor / _Dtor)`; comment :516 |
| 6 | (counted twice in LEFT_FREE.md) standalone/main.cpp:879 second cast | `(uintptr_t)activeDD` |
| 7 | (counted twice) standalone/main.cpp:951 second cast | `(uintptr_t)activeDD` |

Other reads of a companion / dx11 private field by offset from outside dx11/ (all non-cast or already typed):

| file:line | text / note |
|---|---|
| standalone/main.cpp:762 | `        ::Video::DeviceData* activeDD = device->deviceData;       // +0x100` (VideoProbe) |
| standalone/main.cpp:769, :775-776 | `printf("[VideoProbe]   +0x100 active DeviceData = %p\n", (void*)activeDD);` and `void* vtbl = *reinterpret_cast<void**>(activeDD);   // the vtable word: logged, then its module looked up  // CAST-OK: crash_walk`. The module-lookup / `expected = "reimpl-x64.exe"` check (:775-800) is the "THE DX11 PORT IS NOT RUNNING" guard: it reads the VPTR of the DeviceData and tests it lives in the exe. With compiler vtables it still lives in the exe, but the meaning of "the seam is installed" changes (no Dx11SeamInstall) |
| standalone/main.cpp:872, :947 | `::Video::DeviceData* activeDD = device->deviceData;   // +0x100` x2 |
| runtime/src/video/renderstate.cpp:110 | `Video::DeviceData* ActiveDeviceData()` returns `Video::Device::_singleton->deviceData` (typed) |
| runtime/types/video/Device.h:90, :583 | `DeviceData* deviceData;          // +0x100 dx11/ reads this at exactly this offset`; comment :90 (+0xf8 active interface index, +0x100 DeviceData*) |
| runtime/types/video/Device.h:539-566 | the `interfaceData` (+0x40) / `interfaceCount` (+0x48) / `interfaceCapacity` (+0x4C) fields that dx11 writes by byte offset (`Dev::kInterfaces` / `kInterfaceCount`, devicedata.cpp:274-275, :1739-1741 via `GfxInterfaceArraySetLength(arrayBase, ...)`) |
| runtime/src/engine/engine.cpp:487-489 | `callbacks = ::Video::Device::_singleton->gap50; callbacks[0] = &RtEngine_MainLoop_OnVideoError_static; // +0x50 ... callbacks[1] = ... // +0x58`: the Device error/restart callback slots dx11's `ContextData_Finish` reads by offset (`Dev::kOnDeviceLost = 0x050`, `kOnDeviceRestored = 0x058`, dx11/src/contextdata.cpp:341-342) |

`ActiveDeviceData` / `kVideoDevice_ActiveDeviceData` / `_platformData` / `_data` reads outside dx11/: no offset-based reads. `kVideoDevice_ActiveDeviceData` does not exist anywhere; `activeDD` appears only in standalone/main.cpp. Every other read of a companion from outside dx11/ goes through the typed abstract base (section 3). dx11's own `Dev::kActiveDeviceData = 0x100` is at dx11/src/contextdata.cpp:344 and devicedata.cpp:290. The `Dx11_DeviceData_Resize` call at contextdata.cpp:494 is dx11-internal.

## 5. GraphicsInterface / Adapter lifetime tree-wide

### Declarations (runtime/types/video/Device.h)

| file:line | text |
|---|---|
| Device.h:253-268 | Adapter banner: "Video::Device::Adapter -- 0x38 bytes", "DOORS, NOT MEMBERS" table of retail ctor/copy/dtor addresses |
| Device.h:263 | `??0Adapter@Device@Video@@QEAA@XZ                 0x1800f7100  RtVideo_Adapter_Ctor` |
| Device.h:264 | `??0Adapter@...QEAA@AEBU012@@Z                   0x18000cc70  RtVideo_Adapter_CopyCtor` |
| Device.h:265 | `??1Adapter@Device@Video@@QEAA@XZ                 0x18000cc10  RtVideo_Adapter_Dtor` |
| Device.h:266 | `??4Adapter@...                                   0x18000cd00  UNPORTED` (operator=) |
| Device.h:269 | `struct Adapter {` (members: `FindDisplayMode` :272, `FindOrAddMode` :275) |
| Device.h:277 | `        void*        name;              // +0x00  System::String (data pointer); ctor zeroes it` |
| Device.h:278-299 | `modeData` +0x08, `modeCount` +0x10, `modeCapacity` +0x14, `multisampleData` +0x18, `multisampleCount` +0x20, `multisampleCap` +0x24, `maxFilterLevel` +0x28 (ctor -1), `dxgiAdapterIndex` +0x2C (-1), `dxgiOutputIndex` +0x30 (-1), `field34` +0x34 (not written by the ctor) |
| Device.h:302-317 | GraphicsInterface banner, "DOORS, NOT MEMBERS": `??0GraphicsInterface` 0x18000cd90 `RtVideo_GraphicsInterface_Ctor` (:311); copy ctor 0x18000ce10 (:312); `??1GraphicsInterface` 0x18000cdc0 `RtVideo_GraphicsInterface_Dtor` (:313); `??4GraphicsInterface` 0x18000ce60 UNPORTED (:314). :307 cites "code/Game/Dialog/optionsdialog.cpp:462" (stale: the reads are now at :484-520) |
| Device.h:318-323 | `struct GraphicsInterface {  void* name;            // +0x00  System::String data pointer -- the renderer name shown in the combo` / `Adapter* adapterData;     // +0x08` / `int32_t adapterCount;    // +0x10` / `int32_t adapterCapacity; // +0x14` |
| Device.h:402 | `const GraphicsInterface& GetInterface(int index) const;` |
| Device.h:557-559 | `Device::interfaceData` +0x40, `interfaceCount` +0x48, `interfaceCapacity` +0x4C |
| Device.h:851-872 | `static_assert(sizeof(Adapter) == 0x38)`, `alignof == 8`, `offsetof(name/modeData/modeCount/modeCapacity/multisampleData/.../field34)`; `sizeof(GraphicsInterface) == 0x18`, `alignof == 8`, `offsetof(name/adapterData/adapterCount/adapterCapacity)`. `offsetof` on a type with a non-trivial `System::String` member is only "conditionally supported" unless the class stays standard-layout |
| Device.h:991-992 | `using Adapter = ::Video::Device::Adapter;` / `using GraphicsInterface = ...` (namespace RtVideo aliases) |
| Device.h:1023-1031 | `extern "C"` door declarations: `RtVideo_GraphicsInterface_Ctor` (:1023), `RtVideo_GraphicsInterface_CopyCtor` (:1025), `RtVideo_GraphicsInterface_Dtor` (:1027), `RtVideo_Adapter_Ctor` (:1028), `RtVideo_Adapter_CopyCtor` (:1029), `RtVideo_Adapter_Dtor` (:1031) |
| Device.h:1062-1070 | `RtVideo_ArrayAdapterElemDestroy` (0x180010da0), `RtVideo_ArrayAdapterDtor`, `RtVideo_ArrayInterfaceElemDestroy`, `RtVideo_ArrayAdapterAssign` |
| Device.h:62 | banner reference to `RtVideo_ArrayAdapterElemDestroy` |

### Door bodies (runtime/src/video/device_enum_types.cpp), transcribed

String helpers (:145-188, anonymous namespace; header at `-4` refcount, `-8` allocation base; `name` stays `void*`):

```cpp
// Spelling A (standalone dtors 0x18000cde0 / 0x18000cc3a)            :145
void StringRelease(void*& slot) {
    void* p = slot; if (!p) return;
    int32_t* rc = static_cast<int32_t*>(p) - 1;          // CAST-OK: string_header
    if (--*rc == 0) { slot = static_cast<uint8_t*>(slot) - 8;  // CAST-OK: string_header
                      System::MemoryAllocator::MemFree(slot); }
    else            { slot = nullptr; }
}
// Spelling B (the copy INSIDE the Array<Adapter> element-destroy loop, 0x180010e85)   :166
void StringReleaseInLoop(void*& slot) {   // same, but frees only if (slot-8) != 0, leaves slot = base on the free path
    ...
}
// the inlined String share (both copy ctors)                            :181
void StringShare(void*& dst, void* const& src) { dst = nullptr; if (src) { dst = src; ++*(int32_t*)src[-4]; } }
```

Bodies:

```cpp
// :390  ??0GraphicsInterface 0x18000cd90
extern "C" GraphicsInterface* RtVideo_GraphicsInterface_Ctor(GraphicsInterface* self) {
    self->name = nullptr; self->adapterData = nullptr; self->adapterCount = 0; self->adapterCapacity = 0; return self; }
// :401  ??0GraphicsInterface(const&) 0x18000ce10
extern "C" GraphicsInterface* RtVideo_GraphicsInterface_CopyCtor(GraphicsInterface* dst, const GraphicsInterface* src) {
    StringShare(dst->name, src->name);
    dst->adapterData = nullptr; dst->adapterCount = 0; dst->adapterCapacity = 0;
    RtVideo_ArrayAdapterAssign(dst->adapterData, dst->adapterCount, dst->adapterCapacity, src->adapterData, src->adapterCount);
    return dst; }
// :422  ??1GraphicsInterface 0x18000cdc0
extern "C" void RtVideo_GraphicsInterface_Dtor(GraphicsInterface* self) {
    RtVideo_ArrayAdapterDtor(self->adapterData, self->adapterCount, self->adapterCapacity);
    StringRelease(self->name); }
// :438  ??0Adapter 0x1800f7100
extern "C" Adapter* RtVideo_Adapter_Ctor(Adapter* self) {
    self->name = nullptr; self->modeData = nullptr; self->modeCount = 0; self->modeCapacity = 0;
    self->multisampleData = nullptr; self->multisampleCount = 0; self->multisampleCap = 0;
    self->maxFilterLevel = -1; self->dxgiAdapterIndex = -1; self->dxgiOutputIndex = -1;   // +0x34 untouched
    return self; }
// :458  ??0Adapter(const&) 0x18000cc70
extern "C" Adapter* RtVideo_Adapter_CopyCtor(Adapter* dst, const Adapter* src) {
    StringShare(dst->name, src->name);
    dst->modeData = nullptr; dst->modeCount = 0; dst->modeCapacity = 0;
    RtVideo_ArrayModeAssign(dst->modeData, dst->modeCount, dst->modeCapacity, src->modeData, src->modeCount);
    dst->multisampleData = nullptr; dst->multisampleCount = 0; dst->multisampleCap = 0;
    RtVideo_ArrayBoolAssign(dst->multisampleData, dst->multisampleCount, dst->multisampleCap, src->multisampleData, src->multisampleCount);
    dst->maxFilterLevel = src->maxFilterLevel; dst->dxgiAdapterIndex = src->dxgiAdapterIndex; dst->dxgiOutputIndex = src->dxgiOutputIndex;
    return dst; }
// :486  ??1Adapter 0x18000cc10   (order: msaa, modes, name)
extern "C" void RtVideo_Adapter_Dtor(Adapter* self) {
    RtVideo_ArrayBoolDtor(self->multisampleData, self->multisampleCount, self->multisampleCap);
    RtVideo_ArrayModeDtor(self->modeData, self->modeCount, self->modeCapacity);
    StringRelease(self->name); }
```

Array helpers in the same file that destroy names INLINE with spelling B (no door call): `RtVideo_ArrayAdapterElemDestroy` (:206-232, `StringReleaseInLoop(e->name)` at :228), `RtVideo_ArrayInterfaceElemDestroy` (:245-258, `StringReleaseInLoop(g->name)` at :255), `RtVideo_ArrayAdapterDtor` (:263-269, calls ElemDestroy then frees), `RtVideo_ArrayAdapterAssign` (:287-303, ArrayAdapterDtor then `RtVideo_Adapter_CopyCtor` per element).

### Construction / destruction / copy / growth sites

| file:line | kind | what |
|---|---|---|
| dx11/src/devicedata.cpp:1548-1549 | CONSTRUCT | `GraphicsInterface gi;` (trivial default init) then `RtVideo_GraphicsInterface_Ctor(&gi)` |
| dx11/src/devicedata.cpp:1550-1555 | NAME STORE | `const System::String name(interfaceName); reinterpret_cast<System::String&>(gi) = name;` (comment :1551 "B.3.1 bridge: GraphicsInterface::name is a POD slot ... not converted yet") |
| dx11/src/devicedata.cpp:1559 | DESTROY (early out) | `RtVideo_GraphicsInterface_Dtor(&gi)` when `createFactory` fails |
| dx11/src/devicedata.cpp:1745 | DESTROY | `RtVideo_GraphicsInterface_Dtor(&gi)` at the end of `EnumerateDxgiInterfaceInto` (both `return false` at :1560 and `return any` at :1746 are reached with `gi` live) |
| dx11/src/devicedata.cpp:1573-1574 | CONSTRUCT | `Adapter rec;` then `RtVideo_Adapter_Ctor(&rec)` (inside the per-output loop) |
| dx11/src/devicedata.cpp:1577-1590 | NAME STORE | `reinterpret_cast<System::String&>(rec) = utf8;  // B.3.1 bridge` after `AsUTF8` of the DXGI description |
| dx11/src/devicedata.cpp:1602 | DESTROY (break) | `RtVideo_Adapter_Dtor(&rec)` when `EnumOutputs` returns NOT_FOUND |
| dx11/src/devicedata.cpp:1729 | DESTROY | `RtVideo_Adapter_Dtor(&rec)` at the bottom of each output iteration |
| dx11/src/devicedata.cpp:1724 | COPY + GROW | `AdapterArraySetLength(&gi, gi.adapterCount + 1, &rec)` (proto copy-constructed into the new slot) |
| dx11/src/devicedata.cpp:1432-1468 | GROW (Adapter) | `AdapterArraySetLength`: new capacity `n` or `3n` (when `n == old + 1 && old != 0`); `MemAlloc(cap * sizeof(Adapter), 8)`; `RtVideo_Adapter_CopyCtor(dst, src)` per element (:1457: proto for i >= oldCount, old element otherwise); old block elements `RtVideo_Adapter_Dtor` (:1462) then `MemFree(oldBase)`; non-moved shrink dtors the tail (:1466) |
| dx11/src/devicedata.cpp:1740-1742 | GROW (GraphicsInterface) | `GfxInterfaceArraySetLength(arrayBase, Field<int32_t>(dev, Dev::kInterfaceCount) + 1, &gi)` where `arrayBase = dev + Dev::kInterfaces (0x40)` |
| dx11/src/devicedata.cpp:1387-1427 | GROW (GraphicsInterface) | `GfxInterfaceArraySetLength` copy-constructs through `RtVideo_GraphicsInterface_CopyCtor` (:1416), dtors old via `RtVideo_GraphicsInterface_Dtor` (:1421, :1425), `MemFree(oldBase)` |
| dx11/src/devicedata.cpp:1779-1804, :1782-1791 | (second caller) | `Dx9Placeholder::EnumerateInterfaces` calls the same `EnumerateDxgiInterfaceInto(self->device, "DirectX 9", ...)`, so the name flows are executed twice per boot (once "DirectX 11", once "DirectX 9") |
| runtime/src/video/video_resources.cpp:295-296 | DESTROY | `~Device`: `RtVideo_ArrayInterfaceElemDestroy(self->interfaceData, self->interfaceCount); FreeTriple(self->interfaceData, ...)` (inline Spelling-B name release per GraphicsInterface and per Adapter) |
| runtime/src/video/device_enum_types.cpp:245-258, :206-232 | DESTROY (inline) | the two element-destroy loops above |
| runtime/src/video/device_enum_types.cpp:287-303 | COPY | `ArrayAdapterAssign` -> `RtVideo_Adapter_CopyCtor` per element |
| runtime/src/video/device_enum_types.cpp:401-415 | COPY | `RtVideo_GraphicsInterface_CopyCtor` |
| runtime/src/video/device_enum_types.cpp:518 | MUTATE | `Video::Device::Adapter::FindOrAddMode(const DisplayMode&, const RefreshRate&)` (grows `modeData` / refresh arrays; not the name) |
| runtime/src/video/device.cpp:109, :126-127, :220-226 | READ | `GetInterfaceCount()` (:109 `return interfaceCount`), `GetInterface(int)` (:126-127 `return interfaceData[index]`), `ValidateConfiguration` reads `interfaceData + renderer` / `iface->adapterData + adapterIdx` (:220-226) |
| runtime/src/video/device_startup.cpp:327, :344, :349, :439 | READ | `ifaceTotal = interfaceCount`, `interfaceData + ifaceIdx`, `iface->adapterData + adapterIdx` |
| runtime/src/video/enumerate_video_modes.cpp:149-152 | READ | `interfaceData + gi`, `iface->adapterData + ai` |
| runtime/src/engine/singleton_ctors.cpp:357-359 | INIT | `Device` ctor sets `interfaceData = nullptr; interfaceCount = 0; interfaceCapacity = 0` |
| code/Game/Dialog/optionsdialog.cpp:394-395, :440-441, :484, :492, :517-520, :901-902, :1185-1215 | READ (const refs, no copy) | `const Video::Device::GraphicsInterface& gfxIface = vidDev->GetInterface(...)`; `const Video::Device::Adapter& adapter = gfxIface.adapterData[...]`; `reinterpret_cast<const System::String*>(&....name)` at :492 and :520. No by-value copy of either type |
| runtime/src/video/video_interfaces_win.cpp:264-274 | (placeholder) | the DX9 placeholder is the second producer of a GraphicsInterface entry (:268 comment "zero-adapter placeholder" is stale; the placeholder lists the same adapters, devicedata.cpp:1786-1790) |
| tests/unit/video_device_abi_test.cpp:134, :313-314 | LAYOUT PIN | `CHECK_EQ(offsetof(Dev::GraphicsInterface, adapterData), 0x08u)`; `is_same<RtVideo::Adapter, Video::Device::Adapter>`, `is_same<RtVideo::GraphicsInterface, ...>`; no `is_trivially_*` / `is_standard_layout` pin on either struct |

No `std::vector` / `Array<GraphicsInterface>` template use exists; the Array is the raw `{data, count, capacity}` triple on `Device` and on `GraphicsInterface`. No other `Adapter` / `GraphicsInterface` local or by-value copy exists tree-wide (grep `GraphicsInterface gi`, `Adapter rec`, `Device::Adapter`, `Device::GraphicsInterface` over runtime/ code/ dx11/ standalone/ tests/).

### Which sites would DOUBLE-RELEASE a `System::String name` member if the struct gained a real dtor while the door is still called

1. dx11/src/devicedata.cpp:1548 `GraphicsInterface gi;` + :1559 and :1745 explicit `RtVideo_GraphicsInterface_Dtor(&gi)`: the compiler-emitted `~GraphicsInterface` at scope exit would run after the door's `StringRelease(self->name)`. On the free path the door leaves `name` pointing at the freed allocation base (`slot = base - 8`, not nulled), so the implicit dtor decrements a refcount in freed memory: use-after-free / double free. (This is exactly what the optionsdialog.cpp:492 comment says.)
2. dx11/src/devicedata.cpp:1573 `Adapter rec;` + :1602 and :1729 explicit `RtVideo_Adapter_Dtor(&rec)`: same, once per output iteration. Also the early `break` at :1602 and the loop-iteration end at :1729 would each be followed by the implicit dtor.
3. The default-initialisation `GraphicsInterface gi;` / `Adapter rec;` then the `_Ctor` door: with a `System::String name` member the implicit default ctor runs first (name = null), then the door writes `name = nullptr` without releasing, harmless; but the door's `maxFilterLevel` / `-1` stores would be redundant with a real ctor.
4. `reinterpret_cast<System::String&>(gi) = name;` (:1554) and `...(rec) = utf8;` (:1588): operator= on the aliased POD slot works today (the slot is a String-sized pointer); with a real member this is a plain assignment and the cast goes away.
5. NOT double-releases (verify and keep in mind): the array growers' `RtVideo_*_Dtor(oldBase + j)` calls (devicedata.cpp:1421, :1425, :1462, :1466) act on raw `MemAlloc`'d storage the compiler never destroys; the Runtime inline element-destroy loops (device_enum_types.cpp:228, :255) and `~Device` (video_resources.cpp:295) act on raw arrays. They stay single-release as long as the array is not turned into a compiler-managed container and the door-vs-member choice is made once. If the struct gets a real dtor and the grower keeps calling the door on raw storage, nothing is double-released but the door and the member dtor are then two implementations of the same dtor.
6. `RtVideo_*_CopyCtor` doors (`StringShare` nulls the destination first): fine on raw storage; with a real copy ctor and a `String` member, `StringShare` would overwrite a constructed String without releasing (leak, not double release) if run on a constructed object.

## 6. Floor / metric / acceptance plumbing (docs/runtime_port/endgame_b/b4_session/)

| file:line | text |
|---|---|
| FLOOR.md:21 | `and the tree metric is **502** (code runtime standalone engine: 262; dx11/ is the next step's` (continues :22 `scope).`) |
| FLOOR.md:83 | `| gpu_map | 0 | 0 | memory returned by a GPU buffer map | nothing marked (dx11/ is the next step) |` |
| FLOOR.md:88 | `| dx11_seam | 20 | 20 | deferred: the VideoDX11 seam, removed by plan §4.1 (CompanionData.h's 12 DeletingDtor arms, Context.h, video_interface.cpp) | 39 -> 20 measured: dx11/ itself carries no marker yet |` (`acceptance.sh` reads this row as the ceiling: regex `^\|\s*`([a-z0-9_]+)`\s*\|\s*(\d+)\s*\|`) |
| FLOOR.md:156 | `| code/Game/Dialog/optionsdialog.cpp | ptr_int 4 | E estimate |` (the two seam casts are not marker-counted) |
| FLOOR.md:170-178 | per-file rows for dx11/funcs/dx11_seam.h (`dx11_seam 1`), dx11/src/buffers.cpp (`dx11_seam 3, gpu_map 1`), contextdata.cpp (`com_iid 1, dx11_seam 2`), devicedata.cpp (`com_iid 1, dx11_seam 6, win32 1`), imagebufferdata.cpp (`dx11_seam 1`), renderstatedata.cpp (`dx11_seam 3`), shaderprograms.cpp (`dx11_seam 2`), smallclasses.cpp (`dx11_seam 2`), vertexlayoutdata.cpp (`dx11_seam 1`) |
| FLOOR.md:212 | `| standalone/video_interface.cpp | dx11_seam 6 | E estimate |` |
| castmetric.py:61-63 | `INDEX_ROOTS = ('code', 'runtime', 'standalone', 'dx11', 'engine', 'tests')` |
| castmetric.py:70-72 | `CATEGORIES = (... 'vslot', 'dx11_seam', ...)`: the category is a tuple member; removing the category means deleting `'dx11_seam'` here, the FLOOR.md :88 row, and the markers |
| acceptance.sh:12 | `#   §6.3  07     bridge markers only on CAST-OK dx11_seam lines` |
| acceptance.sh:55-59 | the "S4 closing form" comment (below) |
| acceptance.sh:62-63 | `_dx = sum(c['total'] for rel, c, sc, hits in rows if rel.startswith('dx11/'))` and `line('01', 'unmarked casts outside dx11/ in no DEFERRED / RULED group (leftfree_c.py WORK)', len(_c[LC.G_WK]), 0, note=f'PROD total {tot["total"]} = dx11 {_dx} + ' + ...` |
| acceptance.sh:111-113 | `if CM._RE_BRIDGE.search(ln) and not re.search(r'CAST-OK:\s*dx11_seam\b', ln): nb += 1` and `line('07', 'bridge-marker lines outside CAST-OK: dx11_seam lines, PROD', nb, 0)` |
| acceptance.sh:233 | `if not rel.endswith(('.h', '.hpp')) or not rel.startswith(('runtime/types/', 'engine/types/', 'code/Game/', 'dx11/types/')):` (type-header scope check includes dx11/types/) |
| leftfree_c.py:2, :7, :16, :26-27 | `"""Section C of LEFT_FREE.md: every counted cast outside dx11/, ...`; `G_DX = 'DEFERRED: dx11 seam step'`; `if re.search(r'dx11 seam', t): return G_DX` (a cast tagged with the words "dx11 seam" lands in the group; the 7 casts all contain those words) |
| leftfree_c.py:40, :75 | `"""{group: [(rel, line, tag)]} for every counted cast outside dx11/."""`; `print('%d counted casts outside `dx11/` ...')` |
| LEFT_FREE.md:649 | `96 counted casts outside dx11/ (one row per cast; a line with two casts appears twice).` |
| LEFT_FREE.md:655 | `| DEFERRED: dx11 seam step | 7 |` |
| LEFT_FREE.md:755-765 | section `### DEFERRED: dx11 seam step (7)`: rows optionsdialog.cpp:492, :520, boot_window.cpp:49, main.cpp:879 (x2), main.cpp:951 (x2) |

The "S4 comment" in acceptance.sh (lines 55-59), verbatim:

```
# S4 closing form (NEXT_SESSION 2.2, ruled 2026-10-05): the PROD total cannot reach 0 inside B.4
# (dx11/ is the next step's, the retail-difference rows a separate task). Line 01 is the casts
# outside dx11/ that leftfree_c.py (LEFT_FREE.md section C) files under NO DEFERRED / RULED group.
# The dx11 seam step restores the PROD-total form when it closes.
```

How line 01 returns to the PROD-total form: replace the `line('01', ...)` call (acceptance.sh:62-68; value `len(_c[LC.G_WK])`) with the PROD-total count (`tot["total"]` = unmarked casts across PROD, including dx11/, target 0), drop the `import leftfree_c as LC` / `_c = LC.collect()` / `_dx = ...` lines, and delete the `S4 closing form` comment. The line-01 note string already prints `PROD total {tot["total"]} = dx11 {_dx} + <groups>` so the number to drive to 0 is visible today. For this to read 0 the 7 casts (section 4) must go (7 DEFERRED: dx11 seam), the 12+2+6 `CAST-OK: dx11_seam` markers disappear with their casts, and every dx11/ cast must be gone or marked. Line 07 (`acceptance.sh:113`) has a `dx11_seam` carve-out that becomes a plain "zero bridge markers" with the category removed.

## 7. Docs that describe the seam (will need a sentence at the close)

| file:line | content |
|---|---|
| dx11/README.md headings | :1 `# dx11/ -- VideoDX11-x64.dll reimplementation`; :22 `## Why this is a tractable target`; :35 `## The replacement seam`; :65 `### The seam that works: own the vtables`; :112 `## One driver: the port replaces the DLL outright`; :120 `### AB mode, and why it is gone (2026-07-24)`; :163 `### How the port stops Runtime loading the DLL`; :208 `## Is the shipped DLL actually unreachable? (verification pass, 2026-07-24)`; :232 `### How it is measured, not argued`; :256 `### Result`; :272 `### What is irreducible, and what is still untested`; :291 `### Superseded -- this whole section is history`; :302 `## Per-draw pipeline-state equality (2026-07-24) -- the claim AB mode existed for`; :312 `### Making two captures comparable at all`; :321 `### The method: let a control decide what counts as noise`; :338 `### Result 1 -- per-field diff over aligned draws`; :368 `### Result 2 -- pipeline signatures, with no alignment step`; :404 `### Result 3 -- resource sets, with --opt-ref-all-resources`; :425 `### What this does and does not establish`; :437 `## Verification is unusually good here`; :453 `## Suggested porting order`; :471 `### Known issue to keep in view`; :482 `## Porting lanes`; :507 `### Integration protocol (historical -- it depended on AB mode)`; :531 `## Status board`; :536 `### Exports`; :557 `### DeviceData vtable (20 slots @ 0x180018bb0)`; :589 `### Runtime-import IAT base -- 0x180013218`; :600 `### Resource class vtables`; :635 `### RenderDoc A/B result`; :675 `### L5 A/B (2026-07-24) -- 68/84 slots`; :702 `### Function roster -- all 85 attributed`; :739/:748/:752 `#### Shared / infrastructure (4)` / `Exports (2)` / `Per-class attribution`; :773 `#### DeviceData helper roles (recovered)`; :789 `## Workflow (per function)`; :808 `### Ghidra note` |
| dx11/src/README.md:4-5 | `Each file registers its own vtable slots with DX11_REGISTER_SLOT (see ../funcs/dx11_seam.h), so a new class needs no edit` |
| dx11/src/README.md:11-12 | `Slot counts are grep -c DX11_REGISTER_SLOT per file; the total is 84, which is the number Dx11SeamInstall()'s completeness check requires (it refuses a hole).` |
| dx11/src/README.md:34-52 | section `## What is deliberately NOT here (ENDGAME B.3.5, RULE B5-8)`: "The twelve concrete *Data classes stay C-style structs with a DX11_REGISTER_SLOT table ... and the 84-slot seam stays. Dx11SeamInstall()'s completeness check is the only guarantee those slots are non-null, and ContextData_Finish's device-lost recovery dispatches through DeviceData's slots by index"; the qualified `self->Video::XData::~XData()` bridge note (:45-48); the `Imports` / `g_deps` rows note (:49-52) |
| dx11/src/README.md:36 | the `exports.cpp` row: "`Dx11_CreateDeviceData` (the readable example of how dx11 allocates a *Data and overwrites the base vptr)" |
| docs/runtime_port/PLAN.md:111 | `Dx11ResolveRuntimeImport(mangled, iat)` row: "dx11/'s own door" |
| docs/runtime_port/PLAN.md:122 | `**VideoDX11-x64.dll is already fully replaced** (dx11/src/, 13 files, plus dx11/funcs/dx11_seam.cpp ...` |
| docs/runtime_port/PLAN.md:350, :392, :393, :680 | mentions `dx11/` (renderer is DX11; Wave 9 cutover "VideoDX11 is already ours"; the census defect note) |
| docs/runtime_port/ENDGAME.md:81, :167 | :81 `What stays after A: the registry seam (RegisterRuntimeExport/Addr, ResolveExport)`; :167 `[DONE 2026-09-28, endgame_b/B3X_RULES.md: 449 classes ... the twelve Video::*Data classes stay on DeletingDtor until the dx11 seam goes.]` |
| CLAUDE.md:11 | `Runtime-x64.dll is ported (runtime/), VideoDX11-x64.dll is replaced (dx11/), game logic lives in code/Game/.` (no seam wording; probably no edit) |
| docs/reference/engine_architecture.md | no hit for `dx11_seam|DX11_REGISTER_SLOT|Dx11SeamInstall|hand-built|VideoDX11|dx11` |
| .claude/skills/*, .claude/rules/* | no hit for any of the patterns |
| runtime/types/video/CompanionData.h:124-196, :787-792 | the banner table `class / ??_7 / slots / dx11 concrete vtable` (`Vt::DeviceData` ...) and the B3X X1 exemption comment (sections 1 and 2) |
| runtime/src/video/README.md:77 | `DeletingDtor(unsigned)`, the class's own member (ENDGAME B.3.9 deleted the ...) |
| tests/unit/sources.mk:144-148, :236-240, :250-258 | seam-linking explanations (section 2c) |
| Makefile:43 | `# VideoDX11-x64.dll reimplementation sources (see dx11/README.md)` |

## 8. Device-lost route plumbing (facts)

| fact | file:line |
|---|---|
| Option field declared | standalone/options.h:180 `const char* forceDeviceLost      = nullptr;// BANISHED_FORCE_DEVICE_LOST -- dx11/src/contextdata.cpp` |
| Option read once at boot | standalone/options.cpp:284 `o.forceDeviceLost        = Raw("BANISHED_FORCE_DEVICE_LOST");` |
| Option consumer | dx11/src/contextdata.cpp:615 `const char* v = Opt().forceDeviceLost;` inside `ForceDeviceLostThisFrame()` (:608-632); `wanted = atoi(v)` (frame number); "ARMED" line at :618-621 |
| Injection print | dx11/src/contextdata.cpp:644 `std::fprintf(stderr, "[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x%08lx)\n", ...)` then `hr = DXGI_ERROR_DEVICE_RESET;` (:648); injection site is `ContextData_Finish` after `swapChain->Present(sync, 0)` (:638) |
| Round-trip complete print | dx11/src/contextdata.cpp:700 `std::fprintf(stderr, "[DeviceLost] device-lost round trip COMPLETE\n");` (stderr, so standalone_error.log; docs/runtime_port/endgame_b/b4_session/devlost.sh:22 greps both logs for it, DLMAX default 5 at :19/:22) |
| Other prints on the route | dx11/src/contextdata.cpp:662 `[DeviceLost] hr=0x%08lx onLost=%p -> ...`; :696 `[DeviceLost] recreate OK; onRestored=%p -> ...` |
| `_resetPending` | NO HIT anywhere in code/ runtime/ standalone/ dx11/ engine/ (neither the name nor `resetPending`) |
| `OnDeviceLost` / `OnDeviceRestored` | no member of those names. dx11 names the two Device fields `kOnDeviceLost = 0x050`, `kOnDeviceRestored = 0x058` (dx11/src/contextdata.cpp:341-342) and reads them as `DeviceNotifyFn` at :659 and :694, calling each if non-null (:665, :703); Runtime-side they are `Video::Device::gap50[2]` (+0x50, +0x58) |
| Writers of the two callbacks | runtime/src/engine/engine.cpp:487-489 `auto* const callbacks = ::Video::Device::_singleton->gap50; callbacks[0] = &RtEngine_MainLoop_OnVideoError_static; callbacks[1] = &RtEngine_MainLoop_OnVideoRestart_static;` with static_asserts at :484-485 (`MainLoopAddr::DeviceErrorFuncSlot` / `DeviceRestartFuncSlot` == `offsetof(Video::Device, gap50)` (+8)); the long note :466-480 explains the old null-pointer defect; thunk bodies runtime/src/engine/engine.cpp:401-414 (`OnVideoError_`, `OnVideoRestart_`, and the `extern "C"` statics at :409/:413) |
| `OnVideoError` body | runtime/src/engine/mainloop_video.cpp:419 (`Engine::MainLoop::OnVideoError()`), calls `_currentLoop->OnVideoError()` at :550, per-record destroy loop with `payload->DeletingDtor(1u)` at :541-545 |
| `OnVideoRestart` body | runtime/src/engine/mainloop_video.cpp:637 (`OnVideoRestart`), `_currentLoop->OnVideoRestart()` at :649, `Save::SaveLoad::_singleton->OnVideoRestart()` at :651 (comment :520 names `Save::SaveLoad::OnVideoRestart` 0x1800c0f10) |
| `SaveLoad::OnVideoError` | runtime/src/save/saveload_update.cpp:1693 (exported, no caller; the DeletingDtor loop is at :1699-1705; MainLoop::OnVideoError inlines its own copy) |
| Loop-level `OnVideoError` overrides (slot 8 / +0x40) | runtime/src/engine/loops.cpp:635 (UpdateLoop), :789 (LoaderLoop); code/Game/Game/gameupdateloop.cpp:2348 (GameUpdateLoop, `{}`), code/Game/Game/menudialogloopt_stubs.cpp:760 (MenuDialogLoopT, prints `[MenuDLT::OnVideoError]`); tests pin slot 8: tests/unit/engine_loop_abi_test.cpp:111, :121, :126; tests/unit/loop_abi_test.cpp:159, :176 |
| Resource-release / recreate on the route | `Video::Device::ReleaseDynamic` / `RecreateDynamic`: runtime/src/video/dynamic_resources.cpp:490-494 / :483-487 (per-class at :247 ImageBuffer::RecreateDynamic, :285 ReleaseDynamic, :318/:340 IndexBuffer, :358/:380 VertexBuffer, :442/:456 RenderBuffer). dx11 calls them at contextdata.cpp `DevOf(owner)->ReleaseDynamic()` (:668 region) and `DevOf(owner)->RecreateDynamic()` (:691). The Options dialog drives the same pair on a renderer/adapter change: code/Game/Dialog/optionsdialog.cpp:959-962 `mainLoop->OnVideoError(); vidDev->ReleaseDynamic(); vidDev->Shutdown();` and :993-996 `vidDev->RecreateDynamic(); ... mainLoop->OnVideoRestart();` |
| Shutdown / CreateDevice by slot INDEX inside the route | dx11/src/contextdata.cpp:673-676 `ddVtable[kSlot_Shutdown]` / `ddVtable[kSlot_CreateDevice]` through `dd->vptr` reinterpret-casts: the dispatch-by-index the README says the seam guards (this block must become `dd->Shutdown()` / `dd->CreateDevice()` as virtual calls on the real class) |
| Runner scripts | docs/runtime_port/endgame_b/b4_session/devlost.sh:15 `env BANISHED_AUTOSTART=1 BANISHED_FORCE_DEVICE_LOST=${DLFRAME:-12000} BANISHED_PAUSEMENU=... ./reimpl-x64.exe`; devlost_at.sh:23 same with `$F`; endgame_b/b5_session/devlost.sh exists (copy) |

## Surprises

1. **A `Video::DeviceData` is created and destroyed by Runtime without ever being a real Device companion**: runtime/src/video/enumerate_video_modes.cpp:123-138 calls `create(this)` (the plugin factory) then `EnumerateInterfaces()` then `DeletingDtor(1)`. With the DX9 placeholder, `create` returns a `new Dx9Placeholder{ g_dx9PlaceholderVtable, device }` (devicedata.cpp:1777-1804): a 16-byte struct with a four-entry hand table, NOT a `Video::DeviceData` and NOT derived from it. After the flip, `data->DeletingDtor(1)` becomes a virtual-destructor call through slot 0/1 of whatever vptr that struct wears. The placeholder needs a real class deriving from `Video::DeviceData` (or the placeholder goes), and its `Dx9P_DeletingDtor(self, flag)` / `delete self` (devicedata.cpp:1779-1781) disappears. The placeholder is also the sole "second producer" of a GraphicsInterface entry and is installed by the cutover-only `VideoInterfaceFillDx9Placeholder`.
2. **Nothing in the tree calls `DeletingDtor` with flags 2 or 3.** All 18 destruction sites pass 1. The VECTOR arm in CompanionData.h (twelve times, plus the twelve `CAST-OK: dx11_seam` cookie casts) and the flags-2/3 test arms in b39_vid_data_test.cpp are reachable by nothing live. They go with the flip.
3. **`_resetPending`, `OnDeviceLost`, `OnDeviceRestored` do not exist as names.** The route is `Device::gap50[0..1]` (+0x50 error, +0x58 restart), written once in `Engine::MainLoop`'s ctor (engine.cpp:487-489) and read by offset in dx11's `ContextData_Finish`. `gap50` is still a `gap` array, not typed `void(*)()` members.
4. **dx11 dispatches the device-lost recovery by vtable INDEX** (contextdata.cpp:673-676, `ddVtable[kSlot_Shutdown]`, `ddVtable[kSlot_CreateDevice]`): the one place where the hand table is read raw inside dx11. It must become virtual calls on the typed base, or it will silently call the wrong slot after slot N becomes N + 1 for N >= 1 (`Shutdown` is retail 3 -> compiler 4). That is the contextdata.cpp the README names as the reason the seam stays.
5. **Seam installation is boot-ordered and reported on the crash path.** `Dx11SeamInstall()` is called at boot step 4b (main.cpp:1249) and `Dx11SeamReport()` from the crash handler (main.cpp:547); with compiler vtables neither exists, and the "THE DX11 PORT IS NOT RUNNING" guard at main.cpp:775-800 (reads the DeviceData vptr and checks it lives in `reimpl-x64.exe`) needs a different proof (the module of the vptr still works, but the "seam not installed" message wording becomes wrong).
6. **optionsdialog.cpp:492's comment names a bug the stale layout hides**: Device.h:307 cites "optionsdialog.cpp:462" for the by-hand String read (now :484-520). Device.h:277 / :319 `void* name` is a pointer to a refcounted buffer whose refcount lives at `-4` and base at `-8`; the free path of the door leaves the slot pointing at the freed base (device_enum_types.cpp:145-155), which is why the real-dtor flip double-releases at devicedata.cpp:1559/1602/1729/1745 (section 5).
7. **`CAST-OK: dx11_seam` has exactly 20 markers outside dx11/ and the ceiling is 20** (FLOOR.md:88): 12 CompanionData.h, 2 Context.h (the two `const void*` overloads that exist only for dx11's callers, Context.h:321/:328), 6 standalone/video_interface.cpp. Removing the category also removes `Context.h`'s two `void*` overloads; dx11 callers pass `void*` at contextdata.cpp (:499 area) and must pass typed pointers first.
8. **Two seam casts in main.cpp count as four** in LEFT_FREE.md/leftfree_c.py (`reinterpret_cast` + `(uintptr_t)` on each of :879 and :951); the "7" is 2 optionsdialog + 1 boot_window + 4 main.cpp.
9. **`swapChain` has a typed home**: dx11/types/VideoDX11Data.h:66 `void* swapChain; // +0x18` and `static_assert(offsetof(DeviceData, swapChain) == 0x18)` (:114). main.cpp:879/:951 could use `VideoDX11::DeviceData::swapChain` once the class is a real subclass visible to standalone (it includes `dx11_seam.h` already for the install call) or via an accessor on the dx11 side; the base `Video::DeviceData` has no such field.
10. **boot_window.cpp:49 is not about a companion at all**: `ArmDeferredShow(void* videoConfig, HWND hwnd)` receives a `void*` Configuration from dx11/src/devicedata.cpp; the fix is to give the call the typed `Video::Configuration*` (dx11 already reads `::Video::Configuration::_singleton` as a class static at contextdata.cpp:636), independent of the vtable work.
11. **Makefile needs no edit to delete `dx11/funcs/`** (wildcards at :44-45 and the pattern rules :270-274), except `-Idx11/funcs` at :80 which becomes an include of a missing directory (harmless in GCC, but a `-Idx11/funcs` of a deleted dir should be removed).
12. **`runtime/types/video/ShaderProgram.h:504 constexpr size_t kDeletingDtor = 0x00;`** is a byte-offset constant in the `programdata_vtbl` namespace next to `kCreate`/`kBind` (renderstate.cpp:40-41 comment says the file stopped indexing by offset); probably dead, check for readers before the flip.
13. **video_data_abi_test.cpp already depends on `dtor_check.h`** (:41, :449-458 for `MirrorVertexLayout` flavour of the flipped shape), so the helper for a virtual-dtor mirror exists; the `Mirror*Data` classes at :137-231 are the only test mirrors still on the exempt slot-0-body shape.
