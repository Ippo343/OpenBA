# INVENTORY A — src/render/dx11/ by class (B5, the dx11 seam, Block 0)

READ-ONLY inventory. Written against the working tree at 2026-10-05 ~19:15, which was NOT clean while this was
written: another party had uncommitted edits in `src/render/dx11/funcs/dx11_seam.cpp`, `src/render/dx11/devicedata.cpp`,
`src/render/dx11/exports.cpp`, `src/engine/video/CompanionData.h`, `src/engine/video/companion_data.cpp`,
`src/app/video_interface.cpp`, `tests/unit/b39_vid_data_test.cpp`, `tests/unit/sources.mk`, and (appearing mid-run) `src/render/dx11/contextdata.cpp` (+6 lines at :670) and `docs/.../B5_RULES.md` (a "B5 W0" change:
typed `_owner`, `using Owner = X`, `static operator new/delete` on the twelve bases, typed owner on the eleven
factories and on `Dx11_CreateDeviceData`, and a "zero registrations for a whole table = flipped, skip it" arm in
`Dx11SeamInstall`). Where W0 changes a fact below, both states are given. Line numbers are of the working tree
(`devicedata.cpp` and `exports.cpp` kept their line counts under the W0 edit; `dx11_seam.cpp` grew 216 -> 228 and
`CompanionData.h` 832 -> 941, so cite those two by the lines given here, not by older notes).
`castmetric.py dx11 --detail`: **240** on the HEAD+first-W0-edits tree, **238** after the contextdata.cpp W0 edit (the three by-index hand-vtable casts at the old :670/:671/:673 are gone, one `Video::DeviceData*` bridge cast at :677 is new). The section-4 table and counts below are the 238 state; contextdata.cpp lines after :674 are +6 against older notes.

Paths are relative to `src/render/dx11/` unless they start with `runtime/`, `standalone/` or `tests/`.
"BASE" = `src/engine/video/CompanionData.h` (cited as `CD.h:line`).

---------------------------------------------------------------------------------------------------

## 1. Slot bodies vs base signatures

Verdict rules: the free function's first parameter is `self` (becomes `this`); compare the rest. Return type must be
identical for an `override` (covariant only for pointer-to-derived-class returns, and only once the derived class
really derives). `void*` vs a typed pointer = MISMATCH noted. "ignored" = dx11 names the parameter in a comment or
never reads it.

**MISMATCH TOTAL = 17 rows**: 5 slot-0 return types (`DeviceData*`/`ConstantBufferData*`/`IndexBufferData*`/
`VertexBufferData*`/`RenderStateData*` instead of `void*`), 1 `ImageBufferData::Create` parameter
(`const ImageLevelArray*` vs `const void*`), 11 factory returns (`void*` vs the typed `XData*`). The 11 factory
OWNER parameters were `void*` at HEAD (mismatch against `X* owner`) and are typed in the uncommitted W0 edit, so they
are MATCH in the working tree. Everything else (67 of 84 slots) is MATCH. Nothing differs in arity anywhere.

Cross-cutting facts that apply to every class (not repeated per row):
* Every dx11 slot-0 body handles only `flags & 1` (scalar deleting dtor) and returns `self`; the base's
  `DeletingDtor` also has the `flags & 2` VECTOR arm (CD.h:254 etc.). An `override` of slot 0 has to decide what to do
  with bit 2 (retail's dx11 slot 0 never sees it; nothing in the tree vector-news a `*Data`).
* Every dx11 struct duplicates the base's two fields: `vptr` (+0x00, dies when the compiler owns the vptr) and the
  owner (+0x08: `device`, `context`, `imageBuffer`, `constantBuffer`, `indexBuffer`, `vertexBuffer`, `vertexLayout`,
  `vertexProgram`, `pixelProgram`, `renderState`, `videoProgram`, `renderBuffer`) which is the base's `_owner`
  (`Owner*` after W0). All of those must be removed/renamed to `_owner` in `types/VideoDX11Data.h`; uses are listed in
  section 7.
* Free functions are in an anonymous namespace (`namespace VideoDX11 { namespace {`) except where "static" is shown;
  `devicedata_stubs.cpp`, `shaderprograms.cpp`, `smallclasses.cpp`, `vertexlayoutdata.cpp` use `static`.

### 1.1 DeviceData — base `Video::DeviceData` CD.h:236, 20 slots, `Vt::DeviceData`, vtable 0x180018bb0

| slot | DX11_REGISTER_SLOT | dx11 free function (file:line) and FULL signature | base virtual (CD.h:line) and signature | verdict |
|---|---|---|---|---|
| 0 | devicedata.cpp:2020 | `DeviceData_Dtor` devicedata.cpp:984 `DeviceData* DeviceData_Dtor(DeviceData* self, unsigned flags)` | :254 `virtual void* DeletingDtor(unsigned flags)` | MISMATCH (return `DeviceData*` vs `void*`); bit-2 arm absent in dx11 |
| 1 | devicedata.cpp:2022 | `DeviceData_EnumerateInterfaces` :1749 `bool DeviceData_EnumerateInterfaces(DeviceData* self)` | :267 `virtual bool EnumerateInterfaces()` | MATCH (dead `Orig<bool(*)(DeviceData*)>(1)` arm at :1752) |
| 2 | devicedata.cpp:2024 | `DeviceData_CreateDevice` :1123 `bool DeviceData_CreateDevice(DeviceData* self)` | :268 `virtual bool CreateDevice()` | MATCH (dead `Orig<>(2)` arm :1126) |
| 3 | devicedata.cpp:2026 | `DeviceData_Shutdown` :1040 `void DeviceData_Shutdown(DeviceData* self)` | :269 `virtual void Shutdown()` | MATCH |
| 4 | devicedata_stubs.cpp:64 | `DeviceData_Stub4_Void` devicedata_stubs.cpp:41 `static void DeviceData_Stub4_Void(void* /*self*/, bool /*loading*/)` | :274 `virtual void Slot4(bool loading)` | MATCH; `loading` ignored; `self` is `void*` |
| 5 | devicedata_stubs.cpp:66 | `DeviceData_Stub5_ZeroFloat` :45 `static float DeviceData_Stub5_ZeroFloat(void* /*self*/)` | :275 `virtual float Slot5()` | MATCH (returns 0.0f) |
| 6 | devicedata_stubs.cpp:68 | `DeviceData_Stub6_False` :50 `static bool DeviceData_Stub6_False(void* /*self*/)` | :276 `virtual bool Slot6()` | MATCH (returns false) |
| 7 | devicedata_stubs.cpp:70 | `DeviceData_Stub7_True` :55 `static bool DeviceData_Stub7_True(void* /*self*/)` | :277 `virtual bool Slot7()` | MATCH (returns true) |
| 8 | devicedata.cpp:2027 | `DeviceData_SetImageBuffer` :1321 `void DeviceData_SetImageBuffer(DeviceData* self, void* imageBuffer, int hotX, int hotY)` | :279 `virtual void SetImageBuffer(void* imageBuffer, int hotX, int hotY)` | MATCH (`void* imageBuffer` both sides; dx11 casts it at devicedata.cpp:319) |
| 9 | devicedata.cpp:2029 | `Factory_ContextData` :1869 `void* Factory_ContextData(DeviceData* /*self*/, ::Video::Context* owner)` (HEAD: `void* owner`) | :286 `virtual ContextData* CreateContextData(Context* owner)` | MISMATCH (return `void*`) |
| 10 | devicedata.cpp:2031 | `Factory_ImageBufferData` :1879 `void* ...(DeviceData*, ::Video::ImageBuffer* owner)` | :287 `virtual ImageBufferData* CreateImageBufferData(ImageBuffer* owner)` | MISMATCH (return) |
| 11 | devicedata.cpp:2033 | `Factory_ConstantBufferData` :1891 `void* ...(DeviceData*, ::Video::ConstantBuffer* owner)` | :288 `virtual ConstantBufferData* CreateConstantBufferData(ConstantBuffer* owner)` | MISMATCH (return) |
| 12 | devicedata.cpp:2035 | `Factory_IndexBufferData` :1907 `void* ...(DeviceData*, ::Video::IndexBuffer* owner)` | :289 `virtual IndexBufferData* CreateIndexBufferData(IndexBuffer* owner)` | MISMATCH (return) |
| 13 | devicedata.cpp:2037 | `Factory_VertexBufferData` :1917 `void* ...(DeviceData*, ::Video::VertexBuffer* owner)` | :290 `virtual VertexBufferData* CreateVertexBufferData(VertexBuffer* owner)` | MISMATCH (return) |
| 14 | devicedata.cpp:2039 | `Factory_VertexLayoutData` :1927 `void* ...(DeviceData*, ::Video::VertexLayout* owner)` | :291 `virtual VertexLayoutData* CreateVertexLayoutData(VertexLayout* owner)` | MISMATCH (return) |
| 15 | devicedata.cpp:2041 | `Factory_VertexProgramData` :1935 `void* ...(DeviceData*, ::Video::VertexProgram* owner)` | :292 `virtual VertexProgramData* CreateVertexProgramData(VertexProgram* owner)` | MISMATCH (return) |
| 16 | devicedata.cpp:2043 | `Factory_PixelProgramData` :1943 `void* ...(DeviceData*, ::Video::PixelProgram* owner)` | :293 `virtual PixelProgramData* CreatePixelProgramData(PixelProgram* owner)` | MISMATCH (return) |
| 17 | devicedata.cpp:2045 | `Factory_RenderStateData` :1951 `void* ...(DeviceData*, ::Video::RenderState* owner)` | :294 `virtual RenderStateData* CreateRenderStateData(RenderState* owner)` | MISMATCH (return) |
| 18 | devicedata.cpp:2047 | `Factory_VideoProgramData` :1961 `void* ...(DeviceData*, ::Video::VideoProgram* owner)` | :295 `virtual VideoProgramData* CreateVideoProgramData(VideoProgram* owner)` | MISMATCH (return) |
| 19 | devicedata.cpp:2049 | `Factory_RenderBufferData` :1966 `void* ...(DeviceData*, ::Video::RenderBuffer* owner)` | :296 `virtual RenderBufferData* CreateRenderBufferData(RenderBuffer* owner)` | MISMATCH (return) |

Notes: all 11 factories ignore their `DeviceData* /*self*/`. The stubs 4-7 are the "DeviceData stubs" shared at the
binary level with VideoProgramData[2] (0x180001190 = Stub4) and RenderBufferData[1]/VideoProgramData[1]/DeviceData[7]
(0x180001060 = `return true`) but dx11 has SEPARATE source functions for each (`DeviceData_Stub7_True`,
`VideoProgramData_StubTrue`, `RenderBufferData_StubTrue`; `DeviceData_Stub4_Void` vs `VideoProgramData_StubVoid`), so
nothing is shared in source. A covariant override of the factories needs `VideoDX11::XData` complete and derived from
`Video::XData` at the declaration. The Dx9 placeholder is NOT a registered table (section 2.3).

### 1.2 ContextData — base `Video::ContextData` CD.h:305, 20 slots, `Vt::ContextData`, vtable 0x1800186f0

All in contextdata.cpp (anonymous namespace). `self->context` is the owner `void*`.

| slot | REG | dx11 free function (line) and signature | base virtual (CD.h) | verdict |
|---|---|---|---|---|
| 0 | :1336 | :439 `void* ContextData_Dtor(ContextData* self, unsigned flags)` | :323 `virtual void* DeletingDtor(unsigned flags)` | MATCH |
| 1 | :1338 | :482 `bool ContextData_BeginCommands(ContextData* self)` | :336 `virtual bool BeginCommands()` | MATCH |
| 2 | :1340 | :534 `void ContextData_EndCommands(ContextData* self)` | :337 `virtual void EndCommands()` | MATCH |
| 3 | :1342 | :631 `void ContextData_Finish(ContextData* self)` | :338 `virtual void Finish()` | MATCH; Finish's device-lost recovery (:~660-690) calls DeviceData slots 3 and 2 BY NAME through a `Video::DeviceData*` bridge cast (B5 W0, :677-679); at HEAD it read `dd->vptr` and indexed `kSlot_Shutdown`/`kSlot_CreateDevice` |
| 4 | :1343 | :763 `void ContextData_Clear(ContextData* self, unsigned flags, unsigned color, float depth, int stencil)` | :339 `virtual void Clear(unsigned flags, unsigned color, float depth, int stencil)` | MATCH |
| 5 | :1344 | :831 `void ContextData_BindRenderBuffer(ContextData* self)` (fwd decl :719) | :341 `virtual void BindRenderBuffer()` | MATCH |
| 6 | :1346 | :858 `void ContextData_BindViewport(ContextData* self)` (fwd :720) | :342 `virtual void BindViewport()` | MATCH |
| 7 | :1348 | :875 `void ContextData_BindScissor(ContextData* self)` | :343 `virtual void BindScissor()` | MATCH |
| 8 | :1350 | :908 `void ContextData_BindTransforms(ContextData* self)` | :344 `virtual void BindTransforms()` | MATCH |
| 9 | :1352 | :943 `void ContextData_BindTextureNone(ContextData* self, int slot)` | :345 `virtual void BindTextureNone(int slot)` | MATCH |
| 10 | :1354 | :962 `void ContextData_BindIndexNone(ContextData* self)` | :346 `virtual void BindIndexNone()` | MATCH |
| 11 | :1356 | :974 `void ContextData_BindVertexNone(ContextData* self, int slot)` | :347 `virtual void BindVertexNone(int slot)` | MATCH |
| 12 | :1358 | :1010 `void ContextData_DrawIndexed(ContextData* self, int primitiveType, int baseVertex, int vertexCount, int startIndex, int indexCount, int instanceCount)` | :348 `virtual void DrawIndexed(int primitiveType, int baseVertex, int vertexCount, int startIndex, int indexCount, int instanceCount)` | MATCH; `primitiveType` unread (`(void)primitiveType` :1011) |
| 13 | :1360 | :1052 `int ContextData_GetMaxDynamicInstances(ContextData* /*self*/, int /*a*/, int /*b*/, int /*c*/)` | :351 `virtual int GetMaxDynamicInstances(int a, int b, int c)` | MATCH; all four params ignored (returns 0x34) |
| 14 | :1362 | :1078 `bool ContextData_BeginVertices(ContextData* self, int vertexCount, int vertexStride)` | :352 `virtual bool BeginVertices(int vertexCount, int vertexStride)` | MATCH |
| 15 | :1364 | :1134 `void ContextData_EndVertices(ContextData* self, int primitiveType)` | :353 `virtual void EndVertices(int primitiveType)` | MATCH |
| 16 | :1366 | :1174 `bool ContextData_BeginIndexedVertices(ContextData* self, int vertexCount, int vertexStride, int indexCount)` (calls :1204 with instanceCount 1) | :354 `virtual bool BeginIndexedVertices(int vertexCount, int vertexStride, int indexCount)` | MATCH |
| 17 | :1368 | **registered to `ContextData_EndInstancedIndexedVertices`** (:1265) `void ...(ContextData* self, int primitiveType)` | :356 `virtual void EndIndexedVertices(int primitiveType)` | MATCH signature; SHARED BODY with slot 19 (retail 0x180002020 is `jmp 0x180002150`); the override needs a one-line forwarder |
| 18 | :1370 | :1204 `bool ContextData_BeginInstancedIndexedVertices(ContextData* self, int vertexCount, int vertexStride, int indexCount, int instanceCount)` (fwd :1161) | :357 `virtual bool BeginInstancedIndexedVertices(int vertexCount, int vertexStride, int indexCount, int instanceCount)` | MATCH |
| 19 | :1372 | :1265 `void ContextData_EndInstancedIndexedVertices(ContextData* self, int primitiveType)` | :360 `virtual void EndInstancedIndexedVertices(int primitiveType)` | MATCH |

### 1.3 ImageBufferData — base CD.h:369, 7 slots, vtable 0x180018790. All in imagebufferdata.cpp

| slot | REG | dx11 free function and signature | base virtual | verdict |
|---|---|---|---|---|
| 0 | :754 | :391 `void* ImageBufferData_ScalarDeletingDtor(ImageBufferData* self, unsigned int flags)` | CD.h:387 `virtual void* DeletingDtor(unsigned flags)` | MATCH |
| 1 | :756 | :405 `void ImageBufferData_AttachColorBuffer(ImageBufferData* self)` | :400 `virtual void AttachColorBuffer()` | MATCH |
| 2 | :758 | :436 `void ImageBufferData_AttachDepthBuffer(ImageBufferData* self)` | :401 `virtual void AttachDepthBuffer()` | MATCH |
| 3 | :760 | :520 `bool ImageBufferData_Create(ImageBufferData* self, const ImageLevelArray* levels)` (`struct ImageLevelArray {void* data; int32_t count; int32_t capacity;}` :479, file-local) | :404 `virtual bool Create(const void* levels)` | MISMATCH (`const ImageLevelArray*` vs `const void*`) |
| 4 | :762 | :662 `void ImageBufferData_Bind(ImageBufferData* self, void* unknownArg, unsigned int slot)` | :416 `virtual void Bind(void* context, unsigned slot)` | MATCH; `unknownArg` (stale name, it is the Context) is ignored `(void)unknownArg` :663 |
| 5 | :764 | :677 `bool ImageBufferData_Update(ImageBufferData* self, void* data, int width, int height)` | :417 `virtual bool Update(void* data, int width, int height)` | MATCH |
| 6 | :766 | :703 `bool ImageBufferData_Read(ImageBufferData* self, void* dst, int width, int height)` | :418 `virtual bool Read(void* dst, int width, int height)` | MATCH |

### 1.4 ConstantBufferData — base CD.h:542, 4 slots, vtable 0x1800185d0. All in buffers.cpp

| slot | REG | dx11 free function and signature | base virtual | verdict |
|---|---|---|---|---|
| 0 | :941 | :718 `ConstantBufferData* ConstantBufferData_ScalarDeletingDtor(ConstantBufferData* self, unsigned flags)` (calls `X) | CD.h:560 `virtual void* DeletingDtor(unsigned flags)` | MISMATCH (return) |
| 1 | :943 | :739 `bool ConstantBufferData_Create(ConstantBufferData* self)` | :573 `virtual bool Create()` | MATCH |
| 2 | :945 | :811 `void ConstantBufferData_Bind(ConstantBufferData* self, void* /*context, never read*/, const void* srcData, int32_t /*first, never read*/, int32_t sizeBytes)` | :594 `virtual void Bind(void* context, const void* srcData, int32_t first, int32_t sizeBytes)` | MATCH; `context` and `first` ignored |
| 3 | :947 | :846 `int32_t ConstantBufferData_SlotIndexPlusOne(ConstantBufferData* self)` | :596 `virtual int32_t SlotIndexPlusOne()` | MATCH |

### 1.5 IndexBufferData (base CD.h:432, vtable 0x1800187c8) and VertexBufferData (base CD.h:493, vtable 0x180018830) — buffers.cpp

7 slots each. Slots 3 and 4 are ONE body registered to BOTH tables (`BufferData_Lock_Entry` / `BufferData_Unlock_Entry`,
typed on `IndexBufferData*`, legal because `VideoDX11Data.h:389-396` asserts the layouts identical).

| slot | Index REG | Vertex REG | dx11 free function and signature (Index form; Vertex identical with `VertexBufferData*`) | base virtual (Index CD.h / Vertex CD.h) | verdict |
|---|---|---|---|---|---|
| 0 | :926 | :911 | Index :613 `IndexBufferData* IndexBufferData_ScalarDeletingDtor(IndexBufferData* self, unsigned flags)`; Vertex :552 `VertexBufferData* VertexBufferData_ScalarDeletingDtor(VertexBufferData* self, unsigned flags)` | :450 / :511 `virtual void* DeletingDtor(unsigned flags)` | MISMATCH x2 (return) |
| 1 | :928 | :913 | Index :629 `bool IndexBufferData_Create(void* selfV, uint32_t sizeBytes, uint8_t dynamicByte, const void* data)` (fwd decl :610); Vertex :570 `bool VertexBufferData_Create(void* selfV, uint32_t sizeBytes, uint8_t dynamicByte, const void* data)` (fwd :549) | :463 / :524 `virtual bool Create(uint32_t sizeBytes, uint8_t dynamic, const void* data)` | MATCH x2; `self` is `void*` (re-cast at :630 / :571), the first parameter becomes `this` |
| 2 | :930 | :915 | Index :639 `bool IndexBufferData_SetData(IndexBufferData* self, int32_t sizeBytes, int32_t offsetBytes, const void* src)`; Vertex :578 same with `VertexBufferData*` | :465 / :526 `virtual bool SetData(int32_t sizeBytes, int32_t offsetBytes, const void* src)` | MATCH x2 |
| 3 | :932 | :917 | SHARED :685 `void* BufferData_Lock_Entry(IndexBufferData* self, int32_t sizeBytes, int32_t* outByteOffset)` (-> `BufferData_Lock` :347) | :469 / :528 `virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset)` | MATCH x2; one body, two classes |
| 4 | :934 | :919 | SHARED :689 `void BufferData_Unlock_Entry(IndexBufferData* self)` (-> `BufferData_Unlock`) | :470 / :529 `virtual void Unlock()` | MATCH x2; shared |
| 5 | :936 | :921 | Index :666 `void IndexBufferData_Bind(IndexBufferData* self, void* /*context, never read*/)`; Vertex :589 `void VertexBufferData_Bind(VertexBufferData* self, void* /*context, unused*/, uint32_t startSlot, uint32_t offsetBytes, uint32_t strideBytes)` | Index :483 `virtual void Bind(void* context)`; Vertex :531 `virtual void Bind(void* context, uint32_t startSlot, uint32_t offsetBytes, uint32_t strideBytes)` | MATCH x2; `context` ignored in both |
| 6 | :938 | :923 | Index :672 `void IndexBufferData_Serialize(IndexBufferData* self, void* serializer)`; Vertex :601 same | :484 / :533 `virtual void Serialize(void* serializer)` | MATCH x2 |

### 1.6 VertexLayoutData (base CD.h:647, vtable 0x180018878) — vertexlayoutdata.cpp

| slot | REG | dx11 | base | verdict |
|---|---|---|---|---|
| 0 | :242 | :104 `static void* VertexLayoutData_Dtor(VertexLayoutData* self, unsigned flags)` | CD.h:665 `virtual void* DeletingDtor(unsigned flags)` | MATCH |
| 1 | :244 | :143 `static void VertexLayoutData_Bind(VertexLayoutData* self, void* /*Video::Context& — ignored*/)` | :678 `virtual void Bind(void* context)` | MATCH; context ignored |
| 2 | :246 | :185 `static void VertexLayoutData_PlatformCreate(VertexLayoutData* self)` | :681 `virtual void PlatformCreate()` | MATCH (void, unlike the shader classes) |

### 1.7 VertexProgramData (base CD.h:690, vtable 0x180018890) and PixelProgramData (base CD.h:731, vtable 0x180018800) — shaderprograms.cpp

| slot | Vertex REG / Pixel REG | dx11 (Vertex :line / Pixel :line) | base (Vertex / Pixel) | verdict |
|---|---|---|---|---|
| 0 | :266 / :273 | :85 `static void* VertexProgramData_Dtor(VertexProgramData* self, unsigned flags)` / :206 `static void* PixelProgramData_Dtor(PixelProgramData* self, unsigned flags)` | CD.h:708 / :749 `virtual void* DeletingDtor(unsigned flags)` | MATCH x2 |
| 1 | :268 / :275 | :144 `static bool VertexProgramData_PlatformCreate(VertexProgramData* self)` / :226 `static bool PixelProgramData_PlatformCreate(PixelProgramData* self)` | :721 / :762 `virtual bool PlatformCreate()` | MATCH x2 |
| 2 | :270 / :277 | :190 `static void VertexProgramData_Bind(VertexProgramData* self, void* /*Video::Context& — ignored, see header*/, int permutation)` / :252 `static void PixelProgramData_Bind(PixelProgramData* self, void* /*Video::Context& — ignored*/, int permutation)` | :722 / :763 `virtual void Bind(void* context, int permutation)` | MATCH x2; context ignored |

### 1.8 RenderStateData (base CD.h:772, vtable 0x180018818) — renderstatedata.cpp

| slot | REG | dx11 | base | verdict |
|---|---|---|---|---|
| 0 | :540 | :352 `RenderStateData* RenderStateData_ScalarDeletingDtor(RenderStateData* self, unsigned int flags)` | CD.h:790 `virtual void* DeletingDtor(unsigned flags)` | MISMATCH (return) |
| 1 | :542 | :394 `bool RenderStateData_Create(RenderStateData* self)` (dead `Dx11OriginalSlot(Vt::RenderStateData, 1)` arm :398) | :803 `virtual bool Create()` | MATCH |
| 2 | :544 | :513 `void RenderStateData_Bind(RenderStateData* self, void* /*videoContext — unused, as in the original*/)` (dead arm :517) | :804 `virtual void Bind(void* context)` | MATCH; context ignored |

### 1.9 VideoProgramData (base CD.h:817, vtable 0x1800185a8) and RenderBufferData (base CD.h:605, vtable 0x180018588) — smallclasses.cpp

| class.slot | REG | dx11 | base | verdict |
|---|---|---|---|---|
| VideoProgramData.0 | :248 | :76 `static void* VideoProgramData_Dtor(VideoProgramData* self, unsigned flags)` | CD.h:835 `virtual void* DeletingDtor(unsigned flags)` | MATCH |
| VideoProgramData.1 | :250 | :100 `static bool VideoProgramData_StubTrue(void* /*self*/)` | :848 `virtual bool Slot1()` | MATCH (returns true) |
| VideoProgramData.2 | :252 | :112 `static void VideoProgramData_StubVoid(void* /*self*/, void* /*context*/, int /*detail*/)` | :864 `virtual void Bind(void* context, int detail)` | MATCH; both params ignored. NB the base parameter is NAMED `detail`, the same word as `namespace detail` used in its own body (`detail::CompanionPureCall()`); it compiles today |
| RenderBufferData.0 | :255 | :131 `static void* RenderBufferData_Dtor(RenderBufferData* self, unsigned flags)` | :623 `virtual void* DeletingDtor(unsigned flags)` | MATCH |
| RenderBufferData.1 | :257 | :150 `static bool RenderBufferData_StubTrue(void* /*self*/)` | :636 `virtual bool Slot1()` | MATCH |
| RenderBufferData.2 | :259 | :184 `static void RenderBufferData_Bind(RenderBufferData* self, void* /*Video::Context& — ignored*/)` | :637 `virtual void Bind(void* context)` | MATCH; context ignored |
| RenderBufferData.3 | :261 | :228 `static void RenderBufferData_Resolve(RenderBufferData* self, void* /*Video::Context& — ignored*/, void* other, bool /*flag — ignored by this backend*/)` | :638 `virtual void Resolve(void* context, void* other, bool flag)` | MATCH; `context`, `flag` ignored; `other` cast to `RenderBufferOwner` at :235 |

Registration totals check: `grep -c DX11_REGISTER_SLOT` = buffers 18 + contextdata 20 + devicedata 16 + stubs 4 +
imagebufferdata 7 + renderstate 3 + shaderprograms 6 + smallclasses 7 + vertexlayout 3 = 84.

---------------------------------------------------------------------------------------------------

## 2. Factories

### 2.1 The eleven `Factory_XData` (devicedata.cpp) and the shared `AllocData`

`AllocData<T, Base>` is at devicedata.cpp:1861-1868 (working tree: `T* AllocData(size_t size, typename Base::Owner* owner, Vt vt)`;
HEAD: `void* owner`): `p = MemAlloc(size, 4)` (alignment ARGUMENT is 4 for all eleven even though the structs hold
pointers; matches retail), `if (!p) return nullptr`, `::new (static_cast<void*>(p)) Base(owner)` (the Runtime base ctor
= `owner` at +0x08 and the base's vptr), then `p->vptr = Dx11VtableAddress(vt)` (:1866), then the caller zeroes the
private tail. The retail address/size table is the comment at devicedata.cpp:1827-1839; the two non-zero values are
documented at :1841-1850.

| slot | function (line) | size | retail factory addr | retail vtable | initial values written after `AllocData` | registry / side effect |
|---|---|---|---|---|---|---|
| 9 | `Factory_ContextData` :1869 | 0x20 | 0x180005650 | 0x1800186f0 | `topology=0`, `pad14=0`, `d3dContext=nullptr` | none |
| 10 | `Factory_ImageBufferData` :1879 | 0x38 | 0x1800056c0 | 0x180018790 | `sampler/texture/renderTargetView/depthStencilView/shaderResourceView = nullptr` | none (tracking into DeviceData happens later, in AttachColorBuffer/AttachDepthBuffer/Create, via `TrackImageBuffer` imagebufferdata.cpp:228) |
| 11 | `Factory_ConstantBufferData` :1891 | 0x28 | 0x180005730 | 0x1800185d0 | **`activeIndex = -1`** (non-zero), `pad14=0`, `buffers=nullptr`, `bufferCount=0`, `bufferCapacity=0` | **`Dx11TrackConstantBuffer(p)` :1903** (buffers.cpp:881; process-global `std::mutex` + `std::vector<ConstantBufferData*>`; DELIBERATE deviation from retail, see buffers.cpp:858-880; paired with `Dx11UntrackConstantBuffer(self)` in the dtor, buffers.cpp:699) |
| 12 | `Factory_IndexBufferData` :1907 | 0x20 | 0x1800057a0 | 0x1800187c8 | `buffer=nullptr`, `writeCursor=0` (dword), `locked=0` (BYTE); `pad1d[3]` left as MemAlloc leaves it | none |
| 13 | `Factory_VertexBufferData` :1917 | 0x20 | 0x180005810 | 0x180018830 | same as Index | none |
| 14 | `Factory_VertexLayoutData` :1927 | 0x18 | 0x180005880 | 0x180018878 | `inputLayout=nullptr` | none |
| 15 | `Factory_VertexProgramData` :1935 | 0x30 | 0x1800058f0 | 0x180018890 | `variants[0..3]=nullptr` (loop) | none |
| 16 | `Factory_PixelProgramData` :1943 | 0x30 | 0x180005960 | 0x180018800 | `variants[0..3]=nullptr` | none |
| 17 | `Factory_RenderStateData` :1951 | 0x28 | 0x1800059d0 | 0x180018818 | `blendState/depthStencilState/rasterizerState=nullptr` | none |
| 18 | `Factory_VideoProgramData` :1961 | 0x10 | 0x180005a40 | 0x1800185a8 | none (`return AllocData(...)` directly) | none |
| 19 | `Factory_RenderBufferData` :1966 | 0x10 | 0x180005aa0 | 0x180018588 | none | none |

Sizes cross-check with `types/VideoDX11Data.h` `static_assert(sizeof(...))` (0x20/0x38/0x28/0x20/0x20/0x18/0x30/0x30/0x28/0x10/0x10).
Once a class derives from the base (which already has `vptr` + `_owner` = 0x10), the factory's `MemAlloc(size, 4)` +
placement-new `Base(owner)` + `p->vptr = ...` triple becomes one `new (MemAlloc-backed operator new) XData(owner)`;
W0 added `static operator new/delete` on every base (CD.h, implemented in `src/engine/video/companion_operators.cpp`) for exactly that.

### 2.2 The plugin exports (exports.cpp)

| function | file:line | what it does |
|---|---|---|
| `Dx11_CreateDeviceData` (retail `CreateDeviceData` 0x1800011a0) | exports.cpp:64 `DeviceData* Dx11_CreateDeviceData(::Video::Device* device)` (HEAD: `void* device`) | `MemAlloc(0x70, 4)` :65, `::new (p) ::Video::DeviceData(device)` :68, `p->vptr = Dx11VtableAddress(Vt::DeviceData)` :69, then zeroes `unk10, swapChain, d3dDevice, d3dContext, depthBuffer, colorBuffer`, `wasFullscreen = 0` (BYTE only; `pad41` untouched), `cursor, cursorImage = nullptr`, **`cursorHotspotX = cursorHotspotY = -1`** (the retail qword -1 at +0x58), `trackedImages = nullptr`, `trackedCount = trackedCapacity = 0` (:78-85). No registry side effect. Called only through `Video::Device::dllSlots[].createDeviceData` (src/app/video_interface.cpp:133) |
| `Dx11_GetDeviceName` (retail 0x180001230) | exports.cpp:101 `const char* Dx11_GetDeviceName()` | returns the literal `"DirectX 11"` |

NB these two are declared by a LOCAL `extern` in `src/app/video_interface.cpp:56-64` (`namespace VideoDX11 { struct DeviceData; DeviceData* Dx11_CreateDeviceData(::Video::Device*); const char* Dx11_GetDeviceName(); void* Dx9Placeholder_CreateDeviceData(void*); const char* Dx9Placeholder_GetDeviceName(); }`) — no dx11 header declares them (CLAUDE.md "never a local extern").

### 2.3 The Dx9Placeholder block (devicedata.cpp)

Comment block :1757-1776 ("the DX9 PLACEHOLDER slot -- cutover only"). Code inside an anonymous namespace :1777-1801:
`struct Dx9Placeholder { void* vptr; void* device; }` :1777; `void Dx9P_DeletingDtor(Dx9Placeholder* self, int flag)` :1779
(`if (flag & 1) delete self;` — a plain C++ `new`/`delete`, NOT MemAlloc); `bool Dx9P_EnumerateInterfaces(Dx9Placeholder* self)` :1782
(re-uses `EnumerateDxgiInterfaceInto(self->device, "DirectX 9", ...)`); `bool Dx9P_Create(Dx9Placeholder*)` :1792 (returns false);
`void Dx9P_Destroy(Dx9Placeholder*)` :1793; a hand `void* g_dx9PlaceholderVtable[4]` :1795-1800 (the four functions through
`reinterpret_cast<void*>`, 4 of the casts in section 4). External-linkage wrappers (file scope, after the namespace):
`void* Dx9P_CreateImpl(void* device)` :1803 (`new Dx9Placeholder{ g_dx9PlaceholderVtable, device }`), `const char* Dx9P_NameImpl()` :1806, and the two
entry points near the end of the file, inside `namespace VideoDX11`: `void* Dx9Placeholder_CreateDeviceData(void* device)` :2055 and
`const char* Dx9Placeholder_GetDeviceName()` :2056. Only caller: `VideoInterfaceFillDx9Placeholder`, src/app/video_interface.cpp:116-121.
It is NOT one of the twelve classes, derives from nothing, and is unaffected by B5 — but it is a second hand vtable
inside src/render/dx11/ that the seam deletion does not remove.

---------------------------------------------------------------------------------------------------

## 3. Destructors (the twelve slot-0 bodies)

Common shape, every class: (1) `self->vptr = Dx11VtableAddress(Vt::X)` or `Dx11LiveVtable(Vt::X)` (re-seat the dx11 table before
the base dtor — MSVC's derived-dtor idiom); (2) release / free this class's private state; (3)
`reinterpret_cast<Video::XData*>(self)->Video::XData::~XData()` — QUALIFIED non-virtual base-dtor call through a
`// B.3.5 bridge` cast (never `DeletingDtor(0)`, that re-enters slot 0); (4) `if (flags & 1) System::MemoryAllocator::MemFree(self)`; (5) return `self`.

| class | slot-0 body (file:line) | what it releases, in order | vptr re-seat line | qualified base-dtor call | MemFree arm | extra work |
|---|---|---|---|---|---|---|
| DeviceData | devicedata.cpp:984-1006 | `MemFree(trackedImages)` :987, then `trackedImages=nullptr`, `trackedCount=0`, `trackedCapacity=0` :988-990. Releases NO COM object (swap chain, device, context, buffers, cursor are slot 3's job; documented :960-982) | :985 `Dx11VtableAddress(Vt::DeviceData)` | :1000-1001 | :1003-1004 | frees the tracked-image array only |
| ContextData | contextdata.cpp:439-461 | nothing (the cached `d3dContext` is borrowed, never AddRef'd) | :440 `Dx11VtableAddress(Vt::ContextData)` | :456-457 | :459 | none |
| ImageBufferData | imagebufferdata.cpp:391-395 -> `Destructor()` :358-386 | **untracks first** :368-370 (`IsGenerated() || (flags&0x70) || IsRendered()` -> `UntrackImageBuffer(ActiveDeviceData(VideoDevice()), ib)` :263-273, a stable shift-erase of DeviceData::trackedImages), then `ReleaseInterfaces` :280-292 in order `depthStencilView, renderTargetView, shaderResourceView, sampler, texture` (+0x28,+0x20,+0x30,+0x10,+0x18) and nulls all five | :363 `Dx11LiveVtable(Vt::ImageBufferData)` | :384-385 | :393 | **reaches into DeviceData** (removes itself from the reset registry); the slot-0 wrapper and the private `Destructor` are two functions |
| ConstantBufferData | buffers.cpp:718-723 -> `ConstantBufferData_Destruct` :698-716 | **`Dx11UntrackConstantBuffer(self)` first** :699 (removes from the CbRegistry, swap-remove), vptr :700, `Release()` on `buffers[0..bufferCount)` :701-704, `MemFree(self->buffers)` :707, `buffers=nullptr`, `bufferCount=bufferCapacity=0` :708-711 | :700 `Dx11LiveVtable(Vt::ConstantBufferData)` (AFTER the untrack) | :713-714 | :721 | **untracks from the buffers.cpp CbRegistry** (std::mutex); ORDER matters: untrack, vptr, releases |
| IndexBufferData | buffers.cpp:613-624 | `buffer->Release()` if non-null :615; `buffer=nullptr`, `writeCursor=0`, `locked=0` :616-618 | :614 `Dx11LiveVtable(Vt::IndexBufferData)` | :620-621 | :622 | none |
| VertexBufferData | buffers.cpp:552-566 | same as Index (:554-557) | :553 `Dx11LiveVtable(Vt::VertexBufferData)` | :562-563 | :564 | none |
| VertexLayoutData | vertexlayoutdata.cpp:104-126 | `Dx11Release(inputLayout)` :107 (no nulling) | :105 `Dx11VtableAddress(Vt::VertexLayoutData)` | :121-122 | :124 | none |
| VertexProgramData | shaderprograms.cpp:85-108 | `Dx11Release(variants[i])` for i in 0..3 :88-89 (no nulling) | :86 `Dx11VtableAddress(Vt::VertexProgramData)` | :103-104 | :106 | none |
| PixelProgramData | shaderprograms.cpp:206-220 | `Dx11Release(variants[i])` i in 0..3 :209-210 | :207 `Dx11VtableAddress(Vt::PixelProgramData)` | :215-216 | :218 | none |
| RenderStateData | renderstatedata.cpp:352-382 | `blendState->Release()` :355-357, `depthStencilState->Release()` :358-360, `rasterizerState->Release()` :361-363 (field order, deliberately not creation order); no nulling | :353 `Dx11LiveVtable(Vt::RenderStateData)` | :376-377 | :379-381 | none |
| VideoProgramData | smallclasses.cpp:76-97 | nothing (inert class) | :77 `Dx11VtableAddress(Vt::VideoProgramData)` | :92-93 | :95 | none |
| RenderBufferData | smallclasses.cpp:131-146 | nothing (the views belong to the ImageBufferData of each ImageBuffer) | :132 `Dx11VtableAddress(Vt::RenderBufferData)` | :141-142 | :144 | none |

Five slot-0 bodies call `Dx11LiveVtable` directly (buffers.cpp x3, imagebufferdata.cpp, renderstatedata.cpp — the older
lanes) and seven call `Dx11VtableAddress`, an inline wrapper over `Dx11LiveVtable` (dx11_backend_access.h:374-380), so
they are the same operation. Return types of the free
functions are in section 1 (`DeviceData*`, `ConstantBufferData*`, `IndexBufferData*`, `VertexBufferData*`,
`RenderStateData*` vs the base's `void*`). The destructors that do extra work are ImageBufferData (DeviceData tracked list),
ConstantBufferData (CbRegistry) and DeviceData (frees the array that ImageBufferData maintains).

---------------------------------------------------------------------------------------------------

## 4. Every cast in src/render/dx11/

Source: `python docs/runtime_port/endgame_b/b4_session/castmetric.py dx11 --detail` (one row per OCCURRENCE; a
`static_cast<T*>` whose operand is `void*` is a `static_void` row; `tier_i:alloc_result` rows are listed but are
NOT in the 240). Columns: `file:line:col` | cast text (trimmed) | castmetric kind | classification | FROM -> TO | note.

Classification key: **(a)** `void*` COM field -> `ID3D11X*`/`IDXGI*`/`IUnknown*` (dies when the field is typed);
**(b)** `self`/companion `VideoDX11::XData*` <-> `Video::XData*` bridge (dies with inheritance);
**(c)** owner `void*` -> `Video::X*` (dies when `_owner` is typed in the base — W0 already types it as `Owner*`; the
dx11 struct's own owner field is what still erases it);
**(d)** offset accessor / raw offset onto a Video::Context / Device / ImageBuffer / Configuration field
(the runtime member is named in the note);
**(e)** COM IID / QueryInterface / `void**` out-param; **(f)** Win32 handle; **(g)** function-pointer registration
(`DX11_REGISTER_SLOT`, `Orig<>`, `Dx11OriginalSlot`, hand-vtable reads, message callbacks);
**(h)** GraphicsInterface/Adapter `name` String storage; **(i)** WideStringStorage placement; **(j)** other (described).

| file:line:col | cast text | metric kind | class | FROM -> TO | note |
|---|---|---|---|---|---|
| funcs/dx11_seam.h:135:42 | `reinterpret_cast<void*>(impl),     \` | reinterpret | g | slot fn ptr (impl) -> void* | DX11_REGISTER_SLOT macro body; dies with the table |
| src/buffers.cpp:132:23 | `reinterpret_cast<uint8_t*>(::Video::Device::_singleton);` | reinterpret | d | Video::Device* (_singleton) -> uint8_t* | base for Device::deviceData @0x100 |
| src/buffers.cpp:133:13 | `reinterpret_cast<DeviceData**>(device + 0x100);` | reinterpret | d | uint8_t* +0x100 -> VideoDX11::DeviceData** | Video::Device::deviceData (0x100); also a (b) bridge Video::DeviceData*->VideoDX11::DeviceData* |
| src/buffers.cpp:136:12 | `reinterpret_cast<ID3D11DeviceContext*>(ActiveDeviceData()->d3dConte...` | reinterpret | a | void* DeviceData::d3dContext/d3dDevice -> ID3D11DeviceContext*/ID3D11Device* |  |
| src/buffers.cpp:139:12 | `reinterpret_cast<ID3D11Device*>(ActiveDeviceData()->d3dDevice);` | reinterpret | a | void* DeviceData::d3dContext/d3dDevice -> ID3D11DeviceContext*/ID3D11Device* |  |
| src/buffers.cpp:148:12 | `reinterpret_cast< ::Video::Device*>(VideoDeviceSingleton());` | reinterpret | j | void* (singleton) -> Video::Device* | tagged B.3.5 bridge; Device::_singleton is already typed, VideoDeviceSingleton() erases it |
| src/buffers.cpp:154:12 | `reinterpret_cast< ::IO::Serializer*>(serializer);` | reinterpret | j | void* serializer -> IO::Serializer* | slot arg `void* serializer` (base declares void*) |
| src/buffers.cpp:263:9 | `reinterpret_cast< ::IO::Serializer*>(serializer)->GetDirection());` | reinterpret | j | void* serializer -> IO::Serializer* | slot arg `void* serializer` (base declares void*) |
| src/buffers.cpp:299:12 | `reinterpret_cast<PtrArray*>(&self->buffers);` | reinterpret | j | &ConstantBufferData::buffers -> PtrArray* | array view over buffers/bufferCount/bufferCapacity; dies when buffers is typed |
| src/buffers.cpp:323:13 | `reinterpret_cast<void**>(a->data)[i] = reinterpret_cast<void**>(old...` | reinterpret | j | void* (PtrArray data / oldData) -> void** | element copy loop (2 casts) |
| src/buffers.cpp:323:52 | `reinterpret_cast<void**>(oldData)[i];` | reinterpret | j | void* (PtrArray data / oldData) -> void** | element copy loop (2 casts) |
| src/buffers.cpp:348:35 | `reinterpret_cast<const GpuBufferOwner*>(self->indexBuffer);` | reinterpret | c | void* owner (IndexBufferData::indexBuffer / ownerPtr) -> const Video::IndexBuffer* (GpuBufferOwner) | also used for VertexBufferData (field-identical) |
| src/buffers.cpp:360:9 | `reinterpret_cast<ID3D11Buffer*>(self->buffer), 0, mapType, 0, &mapp...` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:370:12 | `reinterpret_cast<uint8_t*>(mapped.pData) + *outByteOffset;` | reinterpret | j | void* mapped.pData -> uint8_t* | Map result byte arithmetic |
| src/buffers.cpp:375:28 | `reinterpret_cast<ID3D11Buffer*>(self->buffer), 0);` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:396:29 | `reinterpret_cast<GpuBufferOwner*>(ownerPtr);` | reinterpret | c | void* owner (IndexBufferData::indexBuffer / ownerPtr) -> const Video::IndexBuffer* (GpuBufferOwner) | also used for VertexBufferData (field-identical) |
| src/buffers.cpp:426:9 | `reinterpret_cast<ID3D11Buffer**>(outBuffer));` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:447:30 | `reinterpret_cast<EncodingQuery_t*>(` | reinterpret | g | void* encoding (vtable word 0) -> EncodingQuery_t | calls a vtable fn of an IO encoding blindly (GpuBuffer_Serialize) |
| src/buffers.cpp:448:10 | `reinterpret_cast<void* const*>(encoding));` | reinterpret | g | void* encoding (vtable word 0) -> EncodingQuery_t | calls a vtable fn of an IO encoding blindly (GpuBuffer_Serialize) |
| src/buffers.cpp:451:29 | `reinterpret_cast<GpuBufferOwner*>(ownerPtr);` | reinterpret | c | void* owner (IndexBufferData::indexBuffer / ownerPtr) -> const Video::IndexBuffer* (GpuBufferOwner) | also used for VertexBufferData (field-identical) |
| src/buffers.cpp:454:34 | `reinterpret_cast<bool&>(owner->dynamic), nullptr);` | reinterpret | j | uint8_t owner->dynamic -> bool& | pun: Video::IndexBuffer::dynamic is a byte, Serialize wants bool& |
| src/buffers.cpp:493:36 | `reinterpret_cast<ID3D11Buffer*>(*bufferSlot));` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:523:35 | `reinterpret_cast<const GpuBufferOwner*>(ownerPtr);` | reinterpret | c | void* owner (IndexBufferData::indexBuffer / ownerPtr) -> const Video::IndexBuffer* (GpuBufferOwner) | also used for VertexBufferData (field-identical) |
| src/buffers.cpp:538:28 | `reinterpret_cast<ID3D11Buffer*>(buffer), dstSubresource, &box,` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:554:23 | `reinterpret_cast<ID3D11Buffer*>(self->buffer)->Release();` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:562:5 | `reinterpret_cast<Video::VertexBufferData*>(self)` | reinterpret | b | VideoDX11::{Vertex,Index,Constant}BufferData* self -> Video::{...}BufferData* | qualified base-dtor call |
| src/buffers.cpp:571:30 | `reinterpret_cast<VertexBufferData*>(selfV);` | reinterpret | j | void* selfV -> {Vertex,Index}BufferData* | Create declared with void* self (forward decl at 549/610) |
| src/buffers.cpp:597:23 | `reinterpret_cast<ID3D11Buffer**>(&self->buffer), &stride, &offset);` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:615:23 | `reinterpret_cast<ID3D11Buffer*>(self->buffer)->Release();` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:620:5 | `reinterpret_cast<Video::IndexBufferData*>(self)` | reinterpret | b | VideoDX11::{Vertex,Index,Constant}BufferData* self -> Video::{...}BufferData* | qualified base-dtor call |
| src/buffers.cpp:630:29 | `reinterpret_cast<IndexBufferData*>(selfV);` | reinterpret | j | void* selfV -> {Vertex,Index}BufferData* | Create declared with void* self (forward decl at 549/610) |
| src/buffers.cpp:668:9 | `reinterpret_cast<ID3D11Buffer*>(self->buffer), DXGI_FORMAT_R16_UINT...` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:702:27 | `reinterpret_cast<ID3D11Buffer**>(self->buffers)[i];` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:713:5 | `reinterpret_cast<Video::ConstantBufferData*>(self)` | reinterpret | b | VideoDX11::{Vertex,Index,Constant}BufferData* self -> Video::{...}BufferData* | qualified base-dtor call |
| src/buffers.cpp:741:9 | `reinterpret_cast<ConstantBufferOwner*>(self->constantBuffer);` | reinterpret | c | void* ConstantBufferData::constantBuffer -> (const) Video::ConstantBuffer* (ConstantBufferOwner) |  |
| src/buffers.cpp:754:28 | `reinterpret_cast<uint8_t*>(owner->_device);` | reinterpret | d | Video::Device* ConstantBuffer::_device -> uint8_t* | base for Device::deviceData @0x100 |
| src/buffers.cpp:755:23 | `reinterpret_cast<DeviceData**>(ownerDevice + 0x100);` | reinterpret | d | uint8_t* +0x100 -> VideoDX11::DeviceData** | Video::Device::deviceData (0x100); also (b) |
| src/buffers.cpp:756:25 | `reinterpret_cast<ID3D11Device*>(dd->d3dDevice);` | reinterpret | a | void* d3dDevice -> ID3D11Device* |  |
| src/buffers.cpp:773:32 | `reinterpret_cast<ID3D11Buffer**>(self->buffers));` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:790:36 | `reinterpret_cast<ID3D11Buffer**>(self->buffers) + i);` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:815:9 | `reinterpret_cast<ConstantBufferOwner*>(self->constantBuffer);` | reinterpret | c | void* ConstantBufferData::constantBuffer -> (const) Video::ConstantBuffer* (ConstantBufferOwner) |  |
| src/buffers.cpp:822:31 | `reinterpret_cast<ID3D11Buffer**>(self->buffers) + index;` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:831:9 | `reinterpret_cast<ID3D11Buffer**>(self->buffers)[self->activeIndex],` | reinterpret | a | void* buffer / out-param -> ID3D11Buffer* / ID3D11Buffer** | IndexBufferData/VertexBufferData::buffer, ConstantBufferData::buffers[] |
| src/buffers.cpp:849:9 | `reinterpret_cast<const ConstantBufferOwner*>(self->constantBuffer);` | reinterpret | c | void* ConstantBufferData::constantBuffer -> (const) Video::ConstantBuffer* (ConstantBufferOwner) |  |
| src/contextdata.cpp:310:13 | `reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);` | reinterpret | d | void* base + off (Field<T> body) -> T* | Field<T> definition: reinterpret_cast + static_cast<uint8_t*> (2 rows each) |
| src/contextdata.cpp:310:34 | `static_cast<uint8_t*>(base) + off);` | static_void | d | void* base + off (Field<T> body) -> T* | Field<T> definition: reinterpret_cast + static_cast<uint8_t*> (2 rows each) |
| src/contextdata.cpp:314:13 | `reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + off);` | reinterpret | d | void* base + off (Field<T> body) -> T* | Field<T> definition: reinterpret_cast + static_cast<uint8_t*> (2 rows each) |
| src/contextdata.cpp:314:40 | `static_cast<const uint8_t*>(base) + off);` | static_void | d | void* base + off (Field<T> body) -> T* | Field<T> definition: reinterpret_cast + static_cast<uint8_t*> (2 rows each) |
| src/contextdata.cpp:317:12 | `static_cast<uint8_t*>(base) + off;` | static_void | d | void* base + off (FieldPtr body) -> uint8_t* | FieldPtr definition |
| src/contextdata.cpp:371:12 | `reinterpret_cast< ::Video::Device*>(device);` | reinterpret | c | void* (Context::_device) -> Video::Device* | DevOf; Context::_device @0x00 |
| src/contextdata.cpp:374:12 | `reinterpret_cast< ::Video::Context*>(context);` | reinterpret | c | void* ContextData::context -> Video::Context* | CtxOf |
| src/contextdata.cpp:378:12 | `reinterpret_cast< ::Video::VertexBuffer*>(vb);` | reinterpret | d | void* Field<void*>(ctx,kDynVertexBuffer) -> Video::VertexBuffer* | Context::_dynamicVertexBuffer @0x20f8 (already typed there) |
| src/contextdata.cpp:381:12 | `reinterpret_cast< ::Video::IndexBuffer*>(ib);` | reinterpret | d | void* Field<void*>(ctx,kDynIndexBuffer) -> Video::IndexBuffer* | Context::_dynamicIndexBuffer @0x2118 (already typed there) |
| src/contextdata.cpp:387:12 | `static_cast<ID3D11DeviceContext*>(self->d3dContext);` | static_void | a | void* d3dContext -> ID3D11DeviceContext* | ContextData::d3dContext |
| src/contextdata.cpp:424:12 | `reinterpret_cast<Fn>(Dx11OriginalSlot(Vt::ContextData, slot));` | reinterpret | g | Dx11OriginalSlot() void* -> Fn | Orig<> template (dead fallback) |
| src/contextdata.cpp:456:5 | `reinterpret_cast< ::Video::ContextData*>(self)` | reinterpret | b | VideoDX11::ContextData* self -> Video::ContextData* | qualified base-dtor call |
| src/contextdata.cpp:537:23 | `static_cast<IUnknown*>(dd->colorBuffer);` | static_void | a | void* dd->colorBuffer -> IUnknown* |  |
| src/contextdata.cpp:541:5 | `static_cast<IDXGISwapChain*>(dd->swapChain)->GetBuffer(` | static_void | a | void* dd->swapChain/colorBuffer/d3dDevice -> IDXGISwapChain*/ID3D11Resource*/ID3D11Device* |  |
| src/contextdata.cpp:542:34 | `reinterpret_cast<void**>(&backBuffer));` | reinterpret | e | IUnknown**/ID3D11Texture2D** &backBuffer -> void** | IDXGISwapChain::GetBuffer out-param with kIID_ID3D11Texture2D |
| src/contextdata.cpp:545:37 | `static_cast<ID3D11Resource*>(dd->colorBuffer), 0,` | static_void | a | void* dd->swapChain/colorBuffer/d3dDevice -> IDXGISwapChain*/ID3D11Resource*/ID3D11Device* |  |
| src/contextdata.cpp:638:18 | `static_cast<IDXGISwapChain*>(dd->swapChain)->Present(sync, 0);` | static_void | a | void* dd->swapChain/colorBuffer/d3dDevice -> IDXGISwapChain*/ID3D11Resource*/ID3D11Device* |  |
| src/contextdata.cpp:654:14 | `static_cast<ID3D11Device*>(dd->d3dDevice)->GetDeviceRemovedReason();` | static_void | a | void* dd->swapChain/colorBuffer/d3dDevice -> IDXGISwapChain*/ID3D11Resource*/ID3D11Device* |  |
| src/contextdata.cpp:677:41 | `reinterpret_cast< ::Video::DeviceData*>(dd);` | reinterpret | b | VideoDX11::DeviceData* dd -> Video::DeviceData* | B5 W0 bridge so Finish() calls Shutdown()/CreateDevice() by name; replaces the by-index hand-vtable reads |
| src/contextdata.cpp:790:26 | `static_cast<const RenderBufferOwner*>(rb)->colorBuffer), rgba);` | static_void | d | void* Field<void*>(ctx,kRenderBuffer) -> const Video::RenderBuffer* (RenderBufferOwner) | Context::_renderBuffer @0x28 (typed const RenderBuffer* there) |
| src/contextdata.cpp:799:26 | `static_cast<const RenderBufferOwner*>(rb)->depthBuffer),` | static_void | d | void* Field<void*>(ctx,kRenderBuffer) -> const Video::RenderBuffer* (RenderBufferOwner) | Context::_renderBuffer @0x28 (typed const RenderBuffer* there) |
| src/contextdata.cpp:912:9 | `reinterpret_cast< ::Video::ConstantBuffer*>(` | reinterpret | d | void* FieldPtr(ctx,kTransformConstantBuffer / kBoneConstantBuffer) -> Video::ConstantBuffer* | Context::_transformConstants @0x2050 / _boneConstants @0x2008 (embedded objects) |
| src/contextdata.cpp:923:9 | `reinterpret_cast< ::Video::ConstantBuffer*>(` | reinterpret | d | void* FieldPtr(ctx,kTransformConstantBuffer / kBoneConstantBuffer) -> Video::ConstantBuffer* | Context::_transformConstants @0x2050 / _boneConstants @0x2008 (embedded objects) |
| src/contextdata.cpp:1081:10 | `static_cast<const int32_t*>(vb) <= vertexCount * vertexStride)` | static_void | d | void* Field<void*>(ctx,kDynVertexBuffer) -> const int32_t* | reads Video::VertexBuffer::size @+0x00 raw |
| src/contextdata.cpp:1209:37 | `static_cast<const int32_t*>(vb);` | static_void | d | void* Field<void*>(ctx,kDynVertexBuffer) -> const int32_t* | reads Video::VertexBuffer::size @+0x00 raw |
| src/devicedata.cpp:208:24 | `reinterpret_cast<void*>(GetProcAddress(m, name)))` | reinterpret | j | FARPROC / void* (GetProcAddress) -> void* / CreateDXGIFactory1_t / D3D11CreateDevice_t | dynamic-import fn pointers (legit loader floor) |
| src/devicedata.cpp:222:38 | `reinterpret_cast<CreateDXGIFactory1_t>(` | reinterpret | j | FARPROC / void* (GetProcAddress) -> void* / CreateDXGIFactory1_t / D3D11CreateDevice_t | dynamic-import fn pointers (legit loader floor) |
| src/devicedata.cpp:227:37 | `reinterpret_cast<D3D11CreateDevice_t>(` | reinterpret | j | FARPROC / void* (GetProcAddress) -> void* / CreateDXGIFactory1_t / D3D11CreateDevice_t | dynamic-import fn pointers (legit loader floor) |
| src/devicedata.cpp:302:13 | `reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);` | reinterpret | d | void* base + off (Field<T> body) -> T* | Field<T> definition (2 rows: reinterpret + static_cast<uint8_t*>) |
| src/devicedata.cpp:302:34 | `static_cast<uint8_t*>(base) + off);` | static_void | d | void* base + off (Field<T> body) -> T* | Field<T> definition (2 rows: reinterpret + static_cast<uint8_t*>) |
| src/devicedata.cpp:305:13 | `reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + off);` | reinterpret | d | const void* base + off (Field<T> const body) -> const T* | Field<T> const definition (2 rows) |
| src/devicedata.cpp:305:40 | `static_cast<const uint8_t*>(base) + off);` | static_void | d | const void* base + off (Field<T> const body) -> const T* | Field<T> const definition (2 rows) |
| src/devicedata.cpp:316:12 | `reinterpret_cast< ::Video::Device*>(device);` | reinterpret | c | void* DeviceData::device -> Video::Device* | VDev |
| src/devicedata.cpp:319:12 | `reinterpret_cast< ::Video::ImageBuffer*>(imageBuffer);` | reinterpret | c | void* imageBuffer (SetImageBuffer arg) -> Video::ImageBuffer* | VImg; base declares `void* imageBuffer` |
| src/devicedata.cpp:410:12 | `reinterpret_cast<Fn>(Dx11OriginalSlot(Vt::DeviceData, slot));` | reinterpret | g | Dx11OriginalSlot() void* -> Fn | Orig<> template (dead fallback) |
| src/devicedata.cpp:551:25 | `static_cast<ID3D11Device*>(self->d3dDevice);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:553:39 | `reinterpret_cast<ID3D11Texture2D**>(slot));` | reinterpret | a | void** slot (&depthBuffer/&colorBuffer) -> ID3D11Texture2D** | CreateTexture2D out-param via helper `void** slot` |
| src/devicedata.cpp:752:32 | `static_cast<ID3D11DeviceContext*>(self->d3dContext);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:854:26 | `static_cast<IDXGISwapChain*>(self->swapChain);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:958:15 | `static_cast<HCURSOR>(dd->cursor));` | static_void | f | void* DeviceData::cursor -> HCURSOR | cursor handle stored as void* |
| src/devicedata.cpp:1000:5 | `reinterpret_cast<Video::DeviceData*>(self)` | reinterpret | b | VideoDX11::DeviceData* self -> Video::DeviceData* | qualified base-dtor call |
| src/devicedata.cpp:1042:5 | `reinterpret_cast< ::Application::Application*>(app)->UnregisterMess...` | reinterpret | j | void* AppOf() (Application::_singleton) -> Application::Application* | B.3.7 bridge; AppOf() erases the singleton type |
| src/devicedata.cpp:1043:5 | `reinterpret_cast< ::Application::Application*>(app)->UnregisterMess...` | reinterpret | j | void* AppOf() (Application::_singleton) -> Application::Application* | B.3.7 bridge; AppOf() erases the singleton type |
| src/devicedata.cpp:1046:23 | `static_cast<HCURSOR>(self->cursor));` | static_void | f | void* DeviceData::cursor -> HCURSOR | cursor handle stored as void* |
| src/devicedata.cpp:1062:30 | `static_cast<IDXGISwapChain*>(self->swapChain);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:1135:30 | `static_cast<IDXGIFactory1*>(self->unk10);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:1214:22 | `reinterpret_cast<ID3D11Device**>(&self->d3dDevice), &granted,` | reinterpret | a | void** &self->d3dDevice/d3dContext/swapChain -> ID3D11Device**/ID3D11DeviceContext**/IDXGISwapChain** | D3D11CreateDevice / CreateSwapChain out-params |
| src/devicedata.cpp:1215:22 | `reinterpret_cast<ID3D11DeviceContext**>(&self->d3dContext)) != 0) {` | reinterpret | a | void** &self->d3dDevice/d3dContext/swapChain -> ID3D11Device**/ID3D11DeviceContext**/IDXGISwapChain** | D3D11CreateDevice / CreateSwapChain out-params |
| src/devicedata.cpp:1223:25 | `static_cast<ID3D11Device*>(self->d3dDevice);` | static_void | a | void* DeviceData::d3dDevice/d3dContext/swapChain/unk10 -> ID3D11Device*/ID3D11DeviceContext*/IDXGISwapChain*/IDXGIFactory1* |  |
| src/devicedata.cpp:1248:34 | `reinterpret_cast<IDXGISwapChain**>(&self->swapChain)) != 0) {` | reinterpret | a | void** &self->d3dDevice/d3dContext/swapChain -> ID3D11Device**/ID3D11DeviceContext**/IDXGISwapChain** | D3D11CreateDevice / CreateSwapChain out-params |
| src/devicedata.cpp:1285:5 | `reinterpret_cast< ::Application::Application*>(app)->RegisterMessag...` | reinterpret | j | void* AppOf() (Application::_singleton) -> Application::Application* | B.3.7 bridge; AppOf() erases the singleton type |
| src/devicedata.cpp:1286:15 | `reinterpret_cast< ::Application::Application::MessageCallback>(&Dev...` | reinterpret | g | &DeviceData_OnSetCursor / &DeviceData_OnActivate -> Application::Application::MessageCallback | WM_SETCURSOR/WM_ACTIVATE callback registration |
| src/devicedata.cpp:1287:5 | `reinterpret_cast< ::Application::Application*>(app)->RegisterMessag...` | reinterpret | j | void* AppOf() (Application::_singleton) -> Application::Application* | B.3.7 bridge; AppOf() erases the singleton type |
| src/devicedata.cpp:1288:12 | `reinterpret_cast< ::Application::Application::MessageCallback>(&Dev...` | reinterpret | g | &DeviceData_OnSetCursor / &DeviceData_OnActivate -> Application::Application::MessageCallback | WM_SETCURSOR/WM_ACTIVATE callback registration |
| src/devicedata.cpp:1328:23 | `static_cast<HCURSOR>(self->cursor));` | static_void | f | void* DeviceData::cursor -> HCURSOR | cursor handle stored as void* |
| src/devicedata.cpp:1363:13 | `reinterpret_cast<const ::Application::Application*>(AppOf())->GetIn...` | reinterpret | j | void* AppOf() (Application::_singleton) -> Application::Application* | B.3.7 bridge; AppOf() erases the singleton type |
| src/devicedata.cpp:1389:33 | `reinterpret_cast<GraphicsInterface**>(arrayBase);` | reinterpret | d | void* arrayBase (&Device::interfaceData) -> GraphicsInterface** | Video::Device::interfaceData @0x40 |
| src/devicedata.cpp:1390:23 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(arrayBase) + 8);` | reinterpret | d | uint8_t* arrayBase + 8 (2 rows) -> int32_t* | Video::Device::interfaceCount @0x48 |
| src/devicedata.cpp:1390:50 | `static_cast<uint8_t*>(arrayBase) + 8);` | static_void | d | uint8_t* arrayBase + 8 (2 rows) -> int32_t* | Video::Device::interfaceCount @0x48 |
| src/devicedata.cpp:1391:23 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(arrayBase) + 12);` | reinterpret | d | uint8_t* arrayBase + 12 (2 rows) -> int32_t* | Video::Device::interfaceCapacity @0x4C |
| src/devicedata.cpp:1391:50 | `static_cast<uint8_t*>(arrayBase) + 12);` | static_void | d | uint8_t* arrayBase + 12 (2 rows) -> int32_t* | Video::Device::interfaceCapacity @0x4C |
| src/devicedata.cpp:1406:18 | `static_cast<GraphicsInterface*>(` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/devicedata.cpp:1447:27 | `static_cast<Adapter*>(` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/devicedata.cpp:1554:9 | `reinterpret_cast<System::String&>(gi) = name;` | reinterpret | h | GraphicsInterface (gi) -> System::String& | GraphicsInterface::name @0x00 stored as String (B.3.1 bridge) |
| src/devicedata.cpp:1558:43 | `reinterpret_cast<void**>(&factory)) != 0) {` | reinterpret | e | IDXGIFactory1** &factory -> void** | CreateDXGIFactory1(IID, void**) |
| src/devicedata.cpp:1587:25 | `reinterpret_cast<const System::WideString*>(&w)->AsUTF8();` | reinterpret | i | WideStringStorage w / tailW -> const System::WideString* | AsUTF8 on placement-storage (B.3.4 bridge) |
| src/devicedata.cpp:1588:21 | `reinterpret_cast<System::String&>(rec) = utf8;` | reinterpret | h | Adapter rec -> System::String& | Adapter::name @0x00 String assign / += (B.3.1 bridge) |
| src/devicedata.cpp:1590:17 | `reinterpret_cast<System::WideString*>(&w)->~WideString();` | reinterpret | i | WideStringStorage w / tailW -> System::WideString* | explicit ~WideString on storage |
| src/devicedata.cpp:1632:29 | `reinterpret_cast<const System::WideString*>(&tailW)->AsUTF8();` | reinterpret | i | WideStringStorage w / tailW -> const System::WideString* | AsUTF8 on placement-storage (B.3.4 bridge) |
| src/devicedata.cpp:1639:34 | `reinterpret_cast<System::String&>(rec) += t2);` | reinterpret | h | Adapter rec -> System::String& | Adapter::name @0x00 String assign / += (B.3.1 bridge) |
| src/devicedata.cpp:1643:21 | `reinterpret_cast<System::WideString*>(&tailW)->~WideString();` | reinterpret | i | WideStringStorage w / tailW -> System::WideString* | explicit ~WideString on storage |
| src/devicedata.cpp:1654:29 | `static_cast<DXGI_MODE_DESC*>(` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/devicedata.cpp:1700:51 | `static_cast<uint8_t*>(` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/devicedata.cpp:1739:27 | `static_cast<uint8_t*>(dev) + Dev::kInterfaces;` | static_void | d | Video::Device* dev + Dev::kInterfaces -> uint8_t* (void* arrayBase) | Video::Device::interfaceData array header @0x40 |
| src/devicedata.cpp:1796:5 | `reinterpret_cast<void*>(&Dx9P_DeletingDtor),` | reinterpret | g | &Dx9P_DeletingDtor -> void* | Dx9 placeholder hand vtable entry 0 |
| src/devicedata.cpp:1797:5 | `reinterpret_cast<void*>(&Dx9P_EnumerateInterfaces),` | reinterpret | g | &Dx9P_EnumerateInterfaces -> void* | Dx9 placeholder hand vtable entry 1 |
| src/devicedata.cpp:1798:5 | `reinterpret_cast<void*>(&Dx9P_Create),` | reinterpret | g | &Dx9P_Create -> void* | Dx9 placeholder hand vtable entry 2 |
| src/devicedata.cpp:1799:5 | `reinterpret_cast<void*>(&Dx9P_Destroy),` | reinterpret | g | &Dx9P_Destroy -> void* | Dx9 placeholder hand vtable entry 3 |
| src/devicedata.cpp:1863:12 | `static_cast<T*>(System::MemoryAllocator::MemAlloc(size, 4));` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/devicedata.cpp:1980:30 | `static_cast<DeviceData*>(deviceData));` | static_void | b | void* deviceData (Dx11_DeviceData_Resize arg) -> VideoDX11::DeviceData* | seam-boundary void*; caller passes Device::deviceData (Video::DeviceData*) |
| src/dx11_backend_access.h:99:12 | `reinterpret_cast<void**>(&::Video::Device::_singleton);` | reinterpret | j | &Video::Device::_singleton -> void** | Dx11VideoDeviceSingletonSlot |
| src/dx11_backend_access.h:109:20 | `static_cast<uint8_t*>(Dx11VideoDevice());` | static_void | d | void* Dx11VideoDevice() -> uint8_t* | base for Device::deviceData @0x100 |
| src/dx11_backend_access.h:110:13 | `reinterpret_cast<DeviceData**>(dev + kVideoDevice_ActiveDeviceData);` | reinterpret | d | uint8_t* + kVideoDevice_ActiveDeviceData (0x100) -> VideoDX11::DeviceData** | Video::Device::deviceData; also (b) |
| src/dx11_backend_access.h:114:12 | `static_cast<ID3D11Device*>(Dx11DeviceData()->d3dDevice);` | static_void | a | void* d3dDevice / d3dContext -> ID3D11Device* / ID3D11DeviceContext* | Dx11Device()/Dx11Context() |
| src/dx11_backend_access.h:118:12 | `static_cast<ID3D11DeviceContext*>(Dx11DeviceData()->d3dContext);` | static_void | a | void* d3dDevice / d3dContext -> ID3D11Device* / ID3D11DeviceContext* | Dx11Device()/Dx11Context() |
| src/dx11_backend_access.h:334:37 | `static_cast<const Video::ImageBuffer*>(imageBuffer);` | static_void | c | const void* imageBuffer -> const Video::ImageBuffer* | Dx11ImageBackend param is const void* |
| src/dx11_backend_access.h:335:12 | `reinterpret_cast<ImageBufferData*>(img->_data);` | reinterpret | b | Video::ImageBufferData* img->_data -> VideoDX11::ImageBufferData* | Video::ImageBuffer::_data @0x60 |
| src/dx11_backend_access.h:338:12 | `static_cast<ID3D11Resource*>(Dx11ImageBackend(imageBuffer)->texture);` | static_void | a | void* ImageBufferData::texture/renderTargetView/depthStencilView -> ID3D11Resource*/ID3D11RenderTargetView*/ID3D11DepthStencilView* |  |
| src/dx11_backend_access.h:341:12 | `static_cast<ID3D11RenderTargetView*>(` | static_void | a | void* ImageBufferData::texture/renderTargetView/depthStencilView -> ID3D11Resource*/ID3D11RenderTargetView*/ID3D11DepthStencilView* |  |
| src/dx11_backend_access.h:345:12 | `static_cast<ID3D11DepthStencilView*>(` | static_void | a | void* ImageBufferData::texture/renderTargetView/depthStencilView -> ID3D11Resource*/ID3D11RenderTargetView*/ID3D11DepthStencilView* |  |
| src/dx11_backend_access.h:385:20 | `static_cast<IUnknown*>(comObject)->Release();` | static_void | a | void* comObject -> IUnknown* | Dx11Release helper (also the generic COM floor) |
| src/exports.cpp:65:21 | `static_cast<DeviceData*>(System::MemoryAllocator::MemAlloc(0x70, 4));` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/imagebufferdata.cpp:188:55 | `reinterpret_cast<int32_t*> (static_cast<uint8_t*>(ib) + off); }` | reinterpret | d | void* ib + off (IbI32 body) -> int32_t* | IbI32 definition (2 rows) |
| src/imagebufferdata.cpp:188:83 | `static_cast<uint8_t*>(ib) + off); }` | static_void | d | void* ib + off (IbI32 body) -> int32_t* | IbI32 definition (2 rows) |
| src/imagebufferdata.cpp:189:55 | `reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ib) + off); }` | reinterpret | d | void* ib + off (IbU32 body) -> uint32_t* | IbU32 definition (2 rows) |
| src/imagebufferdata.cpp:189:83 | `static_cast<uint8_t*>(ib) + off); }` | static_void | d | void* ib + off (IbU32 body) -> uint32_t* | IbU32 definition (2 rows) |
| src/imagebufferdata.cpp:190:59 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ib) + off) = v; }` | reinterpret | d | void* ib + off (IbSet body) -> int32_t* | IbSet definition (2 rows) |
| src/imagebufferdata.cpp:190:86 | `static_cast<uint8_t*>(ib) + off) = v; }` | static_void | d | void* ib + off (IbSet body) -> int32_t* | IbSet definition (2 rows) |
| src/imagebufferdata.cpp:198:12 | `reinterpret_cast<const ::Video::ImageBuffer*>(imageBuffer);` | reinterpret | c | void* ImageBufferData::imageBuffer -> const Video::ImageBuffer* | ImgOf |
| src/imagebufferdata.cpp:209:13 | `reinterpret_cast<DeviceData**>(static_cast<uint8_t*>(device) + 0x100);` | reinterpret | d | void* device + 0x100 -> VideoDX11::DeviceData** | Video::Device::deviceData; also (b) |
| src/imagebufferdata.cpp:209:44 | `static_cast<uint8_t*>(device) + 0x100);` | static_void | d | void* device + 0x100 -> VideoDX11::DeviceData** | Video::Device::deviceData; also (b) |
| src/imagebufferdata.cpp:212:65 | `static_cast<ID3D11Device*>(dd->d3dDevice); }` | static_void | a | void* d3dDevice/d3dContext -> ID3D11Device*/ID3D11DeviceContext* |  |
| src/imagebufferdata.cpp:213:65 | `static_cast<ID3D11DeviceContext*>(dd->d3dContext); }` | static_void | a | void* d3dDevice/d3dContext -> ID3D11Device*/ID3D11DeviceContext* |  |
| src/imagebufferdata.cpp:235:20 | `static_cast<void**>(oldData);` | static_void | j | void* DeviceData::trackedImages -> void** (Video::ImageBuffer*[]) | dies when trackedImages is typed Video::ImageBuffer** |
| src/imagebufferdata.cpp:253:22 | `static_cast<void**>(dd->trackedImages) + i;` | static_void | j | void* DeviceData::trackedImages -> void** (Video::ImageBuffer*[]) | dies when trackedImages is typed Video::ImageBuffer** |
| src/imagebufferdata.cpp:255:37 | `static_cast<void**>(oldData)[i] : imageBuffer;` | static_void | j | void* DeviceData::trackedImages -> void** (Video::ImageBuffer*[]) | dies when trackedImages is typed Video::ImageBuffer** |
| src/imagebufferdata.cpp:265:13 | `static_cast<void**>(dd->trackedImages)[i] != imageBuffer) continue;` | static_void | j | void* DeviceData::trackedImages -> void** (Video::ImageBuffer*[]) | dies when trackedImages is typed Video::ImageBuffer** |
| src/imagebufferdata.cpp:267:27 | `static_cast<void**>(dd->trackedImages) + (j - 1);` | static_void | j | void* DeviceData::trackedImages -> void** (Video::ImageBuffer*[]) | dies when trackedImages is typed Video::ImageBuffer** |
| src/imagebufferdata.cpp:281:35 | `static_cast<IUnknown*>(self->depthStencilView)->Release();` | static_void | a | void* depthStencilView/renderTargetView/shaderResourceView/sampler/texture -> IUnknown* | ReleaseInterfaces |
| src/imagebufferdata.cpp:282:35 | `static_cast<IUnknown*>(self->renderTargetView)->Release();` | static_void | a | void* depthStencilView/renderTargetView/shaderResourceView/sampler/texture -> IUnknown* | ReleaseInterfaces |
| src/imagebufferdata.cpp:283:35 | `static_cast<IUnknown*>(self->shaderResourceView)->Release();` | static_void | a | void* depthStencilView/renderTargetView/shaderResourceView/sampler/texture -> IUnknown* | ReleaseInterfaces |
| src/imagebufferdata.cpp:284:35 | `static_cast<IUnknown*>(self->sampler)->Release();` | static_void | a | void* depthStencilView/renderTargetView/shaderResourceView/sampler/texture -> IUnknown* | ReleaseInterfaces |
| src/imagebufferdata.cpp:285:35 | `static_cast<IUnknown*>(self->texture)->Release();` | static_void | a | void* depthStencilView/renderTargetView/shaderResourceView/sampler/texture -> IUnknown* | ReleaseInterfaces |
| src/imagebufferdata.cpp:309:18 | `reinterpret_cast<int*>(` | reinterpret | d | uint8_t* Configuration::_singleton + 0x28 -> int* | Video::Configuration::textureFilter @0x28 |
| src/imagebufferdata.cpp:310:9 | `reinterpret_cast<uint8_t*>(::Video::Configuration::_singleton) + 0x...` | reinterpret | d | Configuration* _singleton -> uint8_t* | base for textureFilter read |
| src/imagebufferdata.cpp:311:28 | `IbU32(ib, 0x34);` | accessor | d | IbU32(ib,0x34) -> uint32_t | Video::ImageBuffer::_flags @0x34 |
| src/imagebufferdata.cpp:331:27 | `reinterpret_cast<int*>(static_cast<uint8_t*>(device) + 0x98);` | reinterpret | d | void* device + 0x98 -> int* | Video::Device::maxAnisotropy @0x98 |
| src/imagebufferdata.cpp:331:50 | `static_cast<uint8_t*>(device) + 0x98);` | static_void | d | void* device + 0x98 -> int* | Video::Device::maxAnisotropy @0x98 |
| src/imagebufferdata.cpp:335:65 | `IbI32(ib, 0x44)]);` | accessor | d | IbI32(ib,0x44) -> int32_t | Video::ImageBuffer::_filter @0x44 |
| src/imagebufferdata.cpp:336:80 | `IbI32(ib, 0x38)]);` | accessor | d | IbI32(ib,0x38) -> int32_t | Video::ImageBuffer::_addressU @0x38 |
| src/imagebufferdata.cpp:337:80 | `IbI32(ib, 0x3c)]);` | accessor | d | IbI32(ib,0x3c) -> int32_t | Video::ImageBuffer::_addressV @0x3C |
| src/imagebufferdata.cpp:338:80 | `IbI32(ib, 0x40)]);` | accessor | d | IbI32(ib,0x40) -> int32_t | Video::ImageBuffer::_addressW @0x40 |
| src/imagebufferdata.cpp:352:37 | `reinterpret_cast<ID3D11SamplerState**>(&self->sampler));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:368:38 | `IbU32(ib, 0x34) & 0x70) != 0 \|\| ImgOf(ib)->IsRendered())` | accessor | d | IbU32(ib,0x34) -> uint32_t | Video::ImageBuffer::_flags @0x34 |
| src/imagebufferdata.cpp:384:5 | `reinterpret_cast< ::Video::ImageBufferData*>(self)` | reinterpret | b | VideoDX11::ImageBufferData* self -> Video::ImageBufferData* | qualified base-dtor call |
| src/imagebufferdata.cpp:412:22 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x04));` | reinterpret | d | void* device + 0x04 -> int32_t* | Video::Device::backBufferWidth @0x04 (2 rows each) |
| src/imagebufferdata.cpp:412:49 | `static_cast<uint8_t*>(device) + 0x04));` | static_void | d | void* device + 0x04 -> int32_t* | Video::Device::backBufferWidth @0x04 (2 rows each) |
| src/imagebufferdata.cpp:413:22 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x08));` | reinterpret | d | void* device + 0x08 -> int32_t* | Video::Device::backBufferHeight @0x08 (2 rows each) |
| src/imagebufferdata.cpp:413:49 | `static_cast<uint8_t*>(device) + 0x08));` | static_void | d | void* device + 0x08 -> int32_t* | Video::Device::backBufferHeight @0x08 (2 rows each) |
| src/imagebufferdata.cpp:424:5 | `static_cast<IUnknown*>(dd->colorBuffer)->AddRef();` | static_void | a | void* dd->colorBuffer / depthBuffer -> IUnknown* | AddRef of DeviceData buffer |
| src/imagebufferdata.cpp:426:33 | `static_cast<ID3D11Resource*>(self->texture), nullptr,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:427:33 | `reinterpret_cast<ID3D11RenderTargetView**>(&self->renderTargetView));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:441:22 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x04));` | reinterpret | d | void* device + 0x04 -> int32_t* | Video::Device::backBufferWidth @0x04 (2 rows each) |
| src/imagebufferdata.cpp:441:49 | `static_cast<uint8_t*>(device) + 0x04));` | static_void | d | void* device + 0x04 -> int32_t* | Video::Device::backBufferWidth @0x04 (2 rows each) |
| src/imagebufferdata.cpp:442:22 | `reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x08));` | reinterpret | d | void* device + 0x08 -> int32_t* | Video::Device::backBufferHeight @0x08 (2 rows each) |
| src/imagebufferdata.cpp:442:49 | `static_cast<uint8_t*>(device) + 0x08));` | static_void | d | void* device + 0x08 -> int32_t* | Video::Device::backBufferHeight @0x08 (2 rows each) |
| src/imagebufferdata.cpp:453:5 | `static_cast<IUnknown*>(dd->depthBuffer)->AddRef();` | static_void | a | void* dd->colorBuffer / depthBuffer -> IUnknown* | AddRef of DeviceData buffer |
| src/imagebufferdata.cpp:458:33 | `static_cast<ID3D11Resource*>(self->texture), nullptr,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:459:33 | `reinterpret_cast<ID3D11DepthStencilView**>(&self->depthStencilView));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:506:21 | `static_cast<D3D11_SUBRESOURCE_DATA*>(` | tier_i:alloc_result | tier_i | MemAlloc result (void*) -> typed pointer | alloc_result: NOT in the 240 |
| src/imagebufferdata.cpp:532:66 | `IbU32(ib, 0x34) >> 6) & 1) != 0)` | accessor | d | IbU32(ib,0x34) -> uint32_t | Video::ImageBuffer::_flags @0x34 |
| src/imagebufferdata.cpp:538:27 | `IbU32(ib, 0x2c);` | accessor | d | IbU32(ib,0x2c) -> uint32_t | Video::ImageBuffer::_kind @0x2C |
| src/imagebufferdata.cpp:550:63 | `IbU32(ib, 0x34))) & 1);` | accessor | d | IbU32(ib,0x34) -> uint32_t | Video::ImageBuffer::_flags @0x34 |
| src/imagebufferdata.cpp:551:23 | `IbI32(ib, 0x28) == 0) ? 1 : 6;` | accessor | d | IbI32(ib,0x28) -> int32_t | Video::ImageBuffer::_dimension @0x28 |
| src/imagebufferdata.cpp:552:58 | `IbI32(ib, 0x30)]);` | accessor | d | IbI32(ib,0x30) -> int32_t | Video::ImageBuffer::_format @0x30 |
| src/imagebufferdata.cpp:560:28 | `IbI32(ib, 0x28) != 0) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;` | accessor | d | IbI32(ib,0x28) -> int32_t | Video::ImageBuffer::_dimension @0x28 |
| src/imagebufferdata.cpp:565:26 | `IbI32(ib, 0x20);` | accessor | d | IbI32(ib,0x20) -> int32_t | Video::ImageBuffer::_width @0x20 |
| src/imagebufferdata.cpp:570:17 | `reinterpret_cast<const ImageLevel*>(static_cast<uint8_t*>(levels->d...` | reinterpret | j | void* levels->data + i*0x10 -> const ImageLevel* | ImageLevelArray view (2 rows) |
| src/imagebufferdata.cpp:570:53 | `static_cast<uint8_t*>(levels->data) + i * 0x10);` | static_void | j | void* levels->data + i*0x10 -> const ImageLevel* | ImageLevelArray view (2 rows) |
| src/imagebufferdata.cpp:579:69 | `IbI32(ib, 0x30)] * rowUnits);` | accessor | d | IbI32(ib,0x30) -> int32_t | Video::ImageBuffer::_format @0x30 |
| src/imagebufferdata.cpp:590:30 | `IbI32(ib, 0x20);` | accessor | d | IbI32(ib,0x20) -> int32_t | Video::ImageBuffer::_width @0x20 |
| src/imagebufferdata.cpp:597:29 | `reinterpret_cast<ID3D11Texture2D**>(&self->texture));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:614:20 | `IbI32(ib, 0x28) == 1) {` | accessor | d | IbI32(ib,0x28) -> int32_t | Video::ImageBuffer::_dimension @0x28 |
| src/imagebufferdata.cpp:615:90 | `IbI32(ib, 0x30)]);` | accessor | d | IbI32(ib,0x30) -> int32_t | Video::ImageBuffer::_format @0x30 |
| src/imagebufferdata.cpp:624:13 | `static_cast<ID3D11Resource*>(self->texture), srvDescPtr,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:625:13 | `reinterpret_cast<ID3D11ShaderResourceView**>(&self->shaderResourceV...` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:637:21 | `static_cast<ID3D11Resource*>(self->texture), &dsvDesc,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:638:21 | `reinterpret_cast<ID3D11DepthStencilView**>(&self->depthStencilView));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:641:21 | `static_cast<ID3D11Resource*>(self->texture), nullptr,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:642:21 | `reinterpret_cast<ID3D11RenderTargetView**>(&self->renderTargetView));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:666:33 | `reinterpret_cast<ID3D11SamplerState**>(&self->sampler));` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:667:40 | `reinterpret_cast<ID3D11ShaderResourceView**>(&self->shaderResourceV...` | reinterpret | a | void** &self->sampler/shaderResourceView/renderTargetView/depthStencilView/texture -> ID3D11SamplerState**/ID3D11ShaderResourceView**/ID3D11RenderTargetView**/ID3D11DepthStencilView**/ID3D11Texture2D** | out-param address of void* field |
| src/imagebufferdata.cpp:682:18 | `static_cast<ID3D11Resource*>(self->texture), 0, D3D11_MAP_WRITE_DIS...` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:686:20 | `static_cast<ID3D11Resource*>(self->texture), 0);` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:688:32 | `static_cast<ID3D11Resource*>(self->texture), 0, nullptr,` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:723:32 | `static_cast<ID3D11Resource*>(self->texture));` | static_void | a | void* self->texture -> ID3D11Resource* |  |
| src/imagebufferdata.cpp:736:9 | `std::memcpy(static_cast<uint8_t*>(dst) + dstOffset,` | pun | j | void* dst / mapped.pData -> uint8_t* | memcpy row copy (pun + static_cast rows) |
| src/imagebufferdata.cpp:736:21 | `static_cast<uint8_t*>(dst) + dstOffset,` | static_void | j | void* dst / mapped.pData -> uint8_t* | memcpy row copy (pun + static_cast rows) |
| src/imagebufferdata.cpp:737:21 | `static_cast<uint8_t*>(mapped.pData) + srcOffset,` | static_void | j | void* dst / mapped.pData -> uint8_t* | memcpy row copy (pun + static_cast rows) |
| src/renderstatedata.cpp:319:13 | `reinterpret_cast<DeviceData**>(` | reinterpret | d | uint8_t* device + 0x100 -> VideoDX11::DeviceData** | Video::Device::deviceData; also (b) |
| src/renderstatedata.cpp:320:9 | `reinterpret_cast<uint8_t*>(device) + 0x100);` | reinterpret | d | Video::Device* _singleton -> uint8_t* | base for Device::deviceData @0x100 |
| src/renderstatedata.cpp:324:12 | `reinterpret_cast<VideoRenderState*>(self->renderState);` | reinterpret | c | void* RenderStateData::renderState -> VideoRenderState* (local 0x80 mirror of Video::RenderState) | local duplicate view still present |
| src/renderstatedata.cpp:356:9 | `static_cast<ID3D11BlendState*>(self->blendState)->Release();` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:359:9 | `static_cast<ID3D11DepthStencilState*>(self->depthStencilState)->Rel...` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:362:9 | `static_cast<ID3D11RasterizerState*>(self->rasterizerState)->Release();` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:376:5 | `reinterpret_cast< ::Video::RenderStateData*>(self)` | reinterpret | b | VideoDX11::RenderStateData* self -> Video::RenderStateData* | qualified base-dtor call |
| src/renderstatedata.cpp:398:23 | `reinterpret_cast<orig_t>(` | reinterpret | g | Dx11OriginalSlot() void* -> orig_t | dead fallback arm |
| src/renderstatedata.cpp:402:25 | `static_cast<ID3D11Device*>(dd->d3dDevice);` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:488:27 | `reinterpret_cast<ID3D11BlendState**>(&self->blendState));` | reinterpret | a | void** &self->blendState/rasterizerState/depthStencilState -> ID3D11BlendState**/ID3D11RasterizerState**/ID3D11DepthStencilState** | Create*State out-params |
| src/renderstatedata.cpp:490:27 | `reinterpret_cast<ID3D11RasterizerState**>(&self->rasterizerState));` | reinterpret | a | void** &self->blendState/rasterizerState/depthStencilState -> ID3D11BlendState**/ID3D11RasterizerState**/ID3D11DepthStencilState** | Create*State out-params |
| src/renderstatedata.cpp:492:27 | `reinterpret_cast<ID3D11DepthStencilState**>(&self->depthStencilStat...` | reinterpret | a | void** &self->blendState/rasterizerState/depthStencilState -> ID3D11BlendState**/ID3D11RasterizerState**/ID3D11DepthStencilState** | Create*State out-params |
| src/renderstatedata.cpp:517:23 | `reinterpret_cast<orig_t>(` | reinterpret | g | Dx11OriginalSlot() void* -> orig_t | dead fallback arm |
| src/renderstatedata.cpp:522:32 | `static_cast<ID3D11DeviceContext*>(dd->d3dContext);` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:526:26 | `static_cast<ID3D11BlendState*>(self->blendState),` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:529:9 | `static_cast<ID3D11DepthStencilState*>(self->depthStencilState),` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/renderstatedata.cpp:531:21 | `static_cast<ID3D11RasterizerState*>(self->rasterizerState));` | static_void | a | void* blendState/depthStencilState/rasterizerState/d3dDevice/d3dContext -> ID3D11BlendState*/ID3D11DepthStencilState*/ID3D11RasterizerState*/ID3D11Device*/ID3D11DeviceContext* |  |
| src/shaderprograms.cpp:103:5 | `reinterpret_cast< ::Video::VertexProgramData*>(self)` | reinterpret | b | VideoDX11::{Vertex,Pixel}ProgramData* self -> Video::{Vertex,Pixel}ProgramData* | qualified base-dtor call |
| src/shaderprograms.cpp:154:13 | `static_cast<const Video::ShaderProgram*>(self->vertexProgram);` | static_void | c | void* vertexProgram / pixelProgram -> const Video::ShaderProgram* |  |
| src/shaderprograms.cpp:165:13 | `reinterpret_cast<ID3D11VertexShader**>(&self->variants[i]));` | reinterpret | a | void** &self->variants[i] -> ID3D11VertexShader**/ID3D11PixelShader** | Create*Shader out-param |
| src/shaderprograms.cpp:215:5 | `reinterpret_cast< ::Video::PixelProgramData*>(self)` | reinterpret | b | VideoDX11::{Vertex,Pixel}ProgramData* self -> Video::{Vertex,Pixel}ProgramData* | qualified base-dtor call |
| src/shaderprograms.cpp:232:13 | `static_cast<const Video::ShaderProgram*>(self->pixelProgram);` | static_void | c | void* vertexProgram / pixelProgram -> const Video::ShaderProgram* |  |
| src/shaderprograms.cpp:243:13 | `reinterpret_cast<ID3D11PixelShader**>(&self->variants[i]));` | reinterpret | a | void** &self->variants[i] -> ID3D11VertexShader**/ID3D11PixelShader** | Create*Shader out-param |
| src/smallclasses.cpp:92:5 | `reinterpret_cast< ::Video::VideoProgramData*>(self)` | reinterpret | b | VideoDX11::{VideoProgram,RenderBuffer}Data* self -> Video::{VideoProgram,RenderBuffer}Data* | qualified base-dtor call |
| src/smallclasses.cpp:141:5 | `reinterpret_cast<Video::RenderBufferData*>(self)` | reinterpret | b | VideoDX11::{VideoProgram,RenderBuffer}Data* self -> Video::{VideoProgram,RenderBuffer}Data* | qualified base-dtor call |
| src/smallclasses.cpp:189:9 | `static_cast<const RenderBufferOwner*>(self->renderBuffer);` | static_void | c | void* RenderBufferData::renderBuffer -> const Video::RenderBuffer* (RenderBufferOwner) |  |
| src/smallclasses.cpp:233:9 | `static_cast<const RenderBufferOwner*>(self->renderBuffer);` | static_void | c | void* RenderBufferData::renderBuffer -> const Video::RenderBuffer* (RenderBufferOwner) |  |
| src/smallclasses.cpp:235:9 | `static_cast<const RenderBufferOwner*>(other);` | static_void | c | void* other (Resolve arg) -> const Video::RenderBuffer* | base declares `void* other` |
| src/vertexlayoutdata.cpp:121:5 | `reinterpret_cast< ::Video::VertexLayoutData*>(self)` | reinterpret | b | VideoDX11::VertexLayoutData* self -> Video::VertexLayoutData* | qualified base-dtor call |
| src/vertexlayoutdata.cpp:146:9 | `static_cast<ID3D11InputLayout*>(self->inputLayout));` | static_void | a | void* inputLayout -> ID3D11InputLayout* |  |
| src/vertexlayoutdata.cpp:190:9 | `static_cast<const Video::VertexLayout*>(self->vertexLayout);` | static_void | c | void* VertexLayoutData::vertexLayout -> const Video::VertexLayout* |  |
| src/vertexlayoutdata.cpp:235:9 | `reinterpret_cast<ID3D11InputLayout**>(&self->inputLayout));` | reinterpret | a | void** &self->inputLayout -> ID3D11InputLayout** | CreateInputLayout out-param |

### 4.1 Counts per classification (the 238 counted rows; 7 `tier_i:alloc_result` rows are extra)

| class | rows | meaning |
|---|---|---|
| (a) `void*` COM field -> `ID3D11X*` | **81** | dies when the 40-odd COM fields in `VideoDX11Data.h` are typed (`d3dDevice`, `d3dContext`, `swapChain`, `unk10`, `depthBuffer`, `colorBuffer`, `sampler`, `texture`, `renderTargetView`, `depthStencilView`, `shaderResourceView`, `buffer`, `buffers[]`, `inputLayout`, `variants[4]`, `blendState`, `depthStencilState`, `rasterizerState`) and the `reinterpret_cast<ID3D11X**>(&field)` out-params follow. Includes `Dx11Release(void*)`'s `IUnknown*` cast (dx11_backend_access.h:385) |
| (b) `VideoDX11::XData*` <-> `Video::XData*` bridge | **15** | the 12 qualified base-dtor casts (buffers.cpp:562/620/713, contextdata.cpp:456, devicedata.cpp:1000, imagebufferdata.cpp:384, renderstatedata.cpp:376, shaderprograms.cpp:103/215, smallclasses.cpp:92/141, vertexlayoutdata.cpp:121) + `Dx11ImageBackend` (dx11_backend_access.h:335, `Video::ImageBuffer::_data` @0x60 -> `VideoDX11::ImageBufferData*`) + `Dx11_DeviceData_Resize`'s `void*` arg (devicedata.cpp:1980) + the new W0 `Finish()` bridge (contextdata.cpp:677). All die with inheritance. FIVE more rows are `(d)` raw-offset reads of `Video::Device::deviceData` (@0x100, typed `Video::DeviceData*`) into `VideoDX11::DeviceData**` and are ALSO (b) bridges: buffers.cpp:133, buffers.cpp:755, dx11_backend_access.h:110, imagebufferdata.cpp:209 (x2 rows), renderstatedata.cpp:319 |
| (c) owner `void*` -> `Video::X*` | **20** | dies when the dx11 struct's own owner field (`device`, `context`, `imageBuffer`, `constantBuffer`, `indexBuffer`, `vertexBuffer`, `vertexLayout`, `vertexProgram`, `pixelProgram`, `renderState`, `renderBuffer`) is replaced by the base's `_owner` (typed `Owner*` by W0), plus the `void*` arguments `imageBuffer` (devicedata.cpp:319), `other` (smallclasses.cpp:235), `const void* imageBuffer` (dx11_backend_access.h:334) |
| (d) offset accessor / raw offset | **68** | 15 are the accessor DEFINITIONS (contextdata.cpp:310x2,314x2,317; devicedata.cpp:302x2,305x2; imagebufferdata.cpp:188x2,189x2,190x2) and 53 are use sites (Ib* x17 counted by the metric, Device/Configuration raw offsets, Context fields behind `VbOf/IbOf/ConstantBuffer*/RenderBufferOwner`, Device::interfaceData array header). The metric counts only the `IbI32/IbU32` calls (17) as `accessor`; the ~230 `Field<T>(base, kConst)` / `IbSet(ib, 0x..)` uses with NAMED/literal offsets are NOT castmetric rows (the cast is inside the definitions) — they are indexed in section 5 |
| (e) COM IID / `void**` out-param | **2** | contextdata.cpp:542 (`GetBuffer`), devicedata.cpp:1558 (`CreateDXGIFactory1`). The only genuine IID floor |
| (f) Win32 handle | **3** | `static_cast<HCURSOR>(cursor)` devicedata.cpp:958/1046/1328 (dies if `cursor` is typed `HCURSOR`) |
| (g) function-pointer registration / hand vtable | **13** | seam macro (dx11_seam.h:135), `Orig<>` x2 (contextdata.cpp:424 never instantiated; devicedata.cpp:410), `Dx11OriginalSlot` casts renderstatedata.cpp:398/517, message callbacks devicedata.cpp:1286/1288, Dx9 vtable x4 (1796-1799), `EncodingQuery_t` x2 buffers.cpp:447/448. (The three hand-vtable reads contextdata.cpp:670/671/673 that were here at HEAD are gone with the W0 edit.) |
| (h) GraphicsInterface / Adapter `name` String | **3** | devicedata.cpp:1554, 1588, 1639 |
| (i) WideStringStorage placement | **4** | devicedata.cpp:1587, 1590, 1632, 1643 |
| (j) other | **29** | GetProcAddress/fn-ptr x3 (devicedata.cpp:208/222/227), `AppOf()` -> `Application*` x5 (1042/1043/1285/1287/1363), `Video::Device::_singleton` helpers (buffers.cpp:148, dx11_backend_access.h:99), serializer `void*` x2 (buffers.cpp:154/263), `PtrArray` view (buffers.cpp:299) and copy loop x2 (323), Map result bytes (buffers.cpp:370; imagebufferdata.cpp:736/737x2), `bool&` pun (buffers.cpp:454), `void* selfV` x2 (buffers.cpp:571/630), `trackedImages` `void**` x5 (imagebufferdata.cpp:235/253/255/265/267), `ImageLevel` view x2 (570), `Dx11VideoDeviceSingletonSlot` |
| **total** | **238** | = 81 + 15 + 20 + 68 + 2 + 3 + 13 + 3 + 4 + 29 |
| tier_i (alloc_result), not counted | 7 | devicedata.cpp:1406, 1447, 1654, 1700, 1863; exports.cpp:65; imagebufferdata.cpp:506 |

Per-file totals (castmetric `--by-file`, 238 state): dx11_seam.h 1; buffers.cpp 43; contextdata.cpp 25; devicedata.cpp 50
(+tier_i 5); dx11_backend_access.h 11; exports.cpp 0 (+tier_i 1); imagebufferdata.cpp 76 (+tier_i 1); renderstatedata.cpp 17;
shaderprograms.cpp 6; smallclasses.cpp 5; vertexlayoutdata.cpp 4; devicedata_stubs.cpp 0; dx11_seam.cpp 0 (two
void-pointer prints at :191 `static_cast<void*>` and :213 `(void*)` that the metric does not count).

---------------------------------------------------------------------------------------------------

## 5. Offset accessor definitions and their constant tables

### 5.1 Definitions

| helper | file:line | signature / body |
|---|---|---|
| `Field<T>(void* base, size_t off)` | contextdata.cpp:308-311 | `template <typename T> inline T& Field(void* base, size_t off) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off); }` |
| `Field<T>(const void* base, size_t off)` | contextdata.cpp:312-315 | `template <typename T> inline T Field(const void*, size_t)` returning by value |
| `FieldPtr(void* base, size_t off)` | contextdata.cpp:316-318 | `inline void* FieldPtr(void* base, size_t off) { return static_cast<uint8_t*>(base) + off; }` |
| `Field<T>` (a SECOND, identical copy) | devicedata.cpp:301-306 | same two overloads, own anonymous-namespace copy (`T&` at :301-303, `T` const at :304-306); no `FieldPtr` here |
| `IbI32(void* ib, size_t off)` | imagebufferdata.cpp:188 | `inline int32_t IbI32(void* ib, size_t off) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ib) + off); }` |
| `IbU32(void* ib, size_t off)` | imagebufferdata.cpp:189 | uint32_t equivalent |
| `IbSet(void* ib, size_t off, int32_t v)` | imagebufferdata.cpp:190 | store (the task list did not name it; it is the writer of the same family, 20 use sites, not a castmetric `accessor`) |
| `ImgOf(void* imageBuffer)` | imagebufferdata.cpp:197-199 | `reinterpret_cast<const ::Video::ImageBuffer*>` bridge (typed access to what the Ib* helpers read raw) |

No `Field`/`FieldPtr` anywhere else; buffers.cpp, renderstatedata.cpp, shaderprograms.cpp, smallclasses.cpp, vertexlayoutdata.cpp
hard-code the single `+0x100` (`Video::Device::deviceData`) with a literal.

### 5.2 `Ctx::` — offsets onto `Video::Context` (contextdata.cpp:220-306)

Runtime header: `src/engine/video/Context.h` (asserts at :604-676). `self->context` is passed as `void*`.

| constant (line) | value | `Video::Context` member (Context.h) |
|---|---|---|
| `Ctx::kDevice` (225) | 0x0000 | `Device* _device` |
| `Ctx::kDirtyFlags` (235) | 0x0010 | `uint32_t _dirty` |
| `Ctx::kTransformCount` (240) | 0x0020 | `_transformCount` (int32) |
| `Ctx::kBoneCount` (241) | 0x0024 | `int32_t _unknown0024` ("BindResources' second counter") |
| `Ctx::kRenderBuffer` (245) | 0x0028 | `const RenderBuffer* _renderBuffer` |
| `Ctx::kTextures` (250) | 0x0030 | `const ImageBuffer* _imageBuffers[16]` |
| `Ctx::kTextureSlotCount` (251) | 16 (int) | length of `_imageBuffers` |
| `Ctx::kBoneData` (256) | 0x0418 | `_boneAndProjectionStaging` |
| `Ctx::kBoneDataBytes` (257) | 0x0cc0 (int) | byte length of the bone payload |
| `Ctx::kTransformData` (258) | 0x10d8 | `_transforms` |
| `Ctx::kBoundIndexBuffer` (263) | 0x01b8 | `const IndexBuffer* _indexBuffer` |
| `Ctx::kBoundIndexOffset` (264) | 0x01c0 | `_indexOffset` |
| `Ctx::kViewport` (269) | 0x01d0 | `Viewport _viewport` (`Video::Viewport`: x,y,width,height) |
| `Ctx::kScissor` (273) | 0x01e0 | `_scissor` (4 x int32) |
| `Ctx::kBoneConstantBuffer` (278) | 0x2008 | `ConstantBuffer _boneConstants` (embedded object) |
| `Ctx::kTransformConstantBuffer` (279) | 0x2050 | `ConstantBuffer _transformConstants` (embedded) |
| `Ctx::kDynVertexCursor` (284) | 0x20e0 | `_dynamicVertexCursor` |
| `Ctx::kDynVertexStride` (285) | 0x20e8 | `_dynamicVertexStride` |
| `Ctx::kDynIndexCursor` (286) | 0x20f0 | `_dynamicIndexCursor` |
| `Ctx::kDynVertexBuffer` (287) | 0x20f8 | `VertexBuffer* _dynamicVertexBuffer` |
| `Ctx::kDynVertexOffset` (288) | 0x2100 | low dword of `uint64_t _unknown2100` |
| `Ctx::kDynVertexCount` (289) | 0x2104 | HIGH dword of `_unknown2100` (no named member) |
| `Ctx::kDynVertexBase` (290) | 0x2108 | `uint64_t _unknown2108` |
| `Ctx::kDynInstanceCount` (291) | 0x2110 | `_instanceCount` |
| `Ctx::kDynIndexBuffer` (292) | 0x2118 | `IndexBuffer* _dynamicIndexBuffer` |
| `Ctx::kDynIndexOffset` (293) | 0x2120 | low dword of `uint64_t _unknown2120` |
| `Ctx::kDynIndexCount` (294) | 0x2124 | HIGH dword of `_unknown2120` (no named member) |
| `Ctx::kDynIndexBase` (295) | 0x2128 | `uint64_t _unknown2128` |
| `Ctx::kDefaultRenderBuffer` (299) | 0x2138 | `RenderBuffer* _defaultRenderBuffer` |
| `Ctx::kTrianglesSubmitted` (302) | 0x2150 | `_trianglesSubmitted` |
| `Ctx::kVerticesSubmitted` (303) | 0x2154 | `_verticesSubmitted` |
| `Ctx::kDrawsSubmitted` (304) | 0x2158 | `_drawsSubmitted` |
| `Ctx::kObjectsSubmitted` (305) | 0x215c | `_objectsSubmitted` |

Context.h has NO named member for 0x2100/0x2104/0x2120/0x2124 individually (the `_unknownNNNN` qwords contain two ints each) — a
Block-CTX prerequisite to naming them; the dx11 file's names are the better ones.

### 5.3 `Dev::` — offsets onto `Video::Device`

Two independent `namespace Dev` tables (contextdata.cpp:338-345 and devicedata.cpp:247-291). Runtime header `src/engine/video/Device.h` (asserts :892-923).

| constant (file:line) | value | `Video::Device` member |
|---|---|---|
| `Dev::kWidth` (contextdata.cpp:339, devicedata.cpp:252) | 0x004 | `backBufferWidth` |
| `Dev::kHeight` (contextdata.cpp:340, devicedata.cpp:253) | 0x008 | `backBufferHeight` |
| `Dev::kModeIndex` (devicedata.cpp:257) | 0x00c | `displayModeIndex` |
| `Dev::kRefreshIndex` (devicedata.cpp:261) | 0x010 | `refreshRateIndex` |
| `Dev::kAdapterIndex` (devicedata.cpp:265) | 0x014 | `dxgiAdapterIndex` |
| `Dev::kOutputIndex` (devicedata.cpp:266) | 0x018 | `dxgiOutputIndex` |
| `Dev::kModeTable` (devicedata.cpp:269) | 0x020 | `modeData` (array base) |
| `Dev::kInterfaces` (devicedata.cpp:274) | 0x040 | `interfaceData` (array header: data @0x40, `interfaceCount` @0x48, `interfaceCapacity` @0x4C) |
| `Dev::kInterfaceCount` (devicedata.cpp:275) | 0x048 | `interfaceCount` |
| `Dev::kOnDeviceLost` (contextdata.cpp:341) | 0x050 | `void (*gap50[2])()` element 0 |
| `Dev::kOnDeviceRestored` (contextdata.cpp:342) | 0x058 | `gap50[1]` |
| `Dev::kHwnd` (devicedata.cpp:279) | 0x090 | `windowHandle` |
| `Dev::kResetPending` (contextdata.cpp:343, devicedata.cpp:284) | 0x09c | `settingsChanged` |
| `Dev::kMonitor` (devicedata.cpp:287) | 0x0a0 | `monitor` |
| `Dev::kActiveDeviceData` (contextdata.cpp:344, devicedata.cpp:290) and `kVideoDevice_ActiveDeviceData` (dx11_backend_access.h:90, `uintptr_t`) | 0x100 | `Video::DeviceData* deviceData` (typed in Device.h; the dx11 side reads it as `void*`/`DeviceData**`) |

Other raw `Video::Device` offsets spelled as literals: `+0x98` `maxAnisotropy` (imagebufferdata.cpp:331), `+0x04/+0x08` `backBufferWidth/Height` (imagebufferdata.cpp:412/413/441/442), `+0x100` `deviceData` (buffers.cpp:133/755, imagebufferdata.cpp:209, renderstatedata.cpp:320). Constant also named but unused: `kDx11Iat_VideoDeviceSingleton = 0x1800134b8` (dx11_backend_access.h:94, documentation only).

### 5.4 `Cfg::` and `kConfigVSync` — `Video::Configuration` (Device.h:937-955)

| constant (file:line) | value | `Video::Configuration` member |
|---|---|---|
| `Cfg::kWindowWidth` (devicedata.cpp:294) | 0x1c | `windowWidth` |
| `Cfg::kWindowHeight` (devicedata.cpp:295) | 0x20 | `windowHeight` |
| `Cfg::kMultisample` (devicedata.cpp:296) | 0x24 | `multisample` |
| `Cfg::kFullscreen` (devicedata.cpp:297) | 0x40 | `fullscreen` |
| `Cfg::kSkipShow` (devicedata.cpp:298) | 0x42 | `field42` |
| `kConfigVSync` (contextdata.cpp:352) | 0x41 | `vsync` |
| literal `+0x28` (imagebufferdata.cpp:309-310) | 0x28 | `textureFilter` |

### 5.5 `Ib*` — offsets onto `Video::ImageBuffer` (`ImageBuffer.h:434-444`), imagebufferdata.cpp

Raw literals (no named constants): 0x20 `_width`, 0x24 `_height`, 0x28 `_dimension`, 0x2c `_kind`, 0x30 `_format`, 0x34 `_flags`, 0x38 `_addressU`, 0x3c `_addressV`, 0x40 `_addressW`, 0x44 `_filter`.

### 5.6 Use-site indexes (generated by script from the current files; lines exclude the `constexpr` definitions and comments)

### Ctx:: use sites (contextdata.cpp, excluding the definitions)

| constant | n | lines |
|---|---|---|
| Ctx::kDevice | 2 | 360, 489 |
| Ctx::kDirtyFlags | 8 | 767, 770, 774, 777, 842, 844, 910, 1283 |
| Ctx::kTransformCount | 1 | 917 |
| Ctx::kBoneCount | 1 | 921 |
| Ctx::kRenderBuffer | 4 | 786, 797, 836, 848 |
| Ctx::kTextures | 1 | 834 |
| Ctx::kBoneData | 2 | 926, 931 |
| Ctx::kTransformData | 2 | 915, 931 |
| Ctx::kBoundIndexBuffer | 2 | 1281, 1284 |
| Ctx::kBoundIndexOffset | 2 | 1282, 1285 |
| Ctx::kViewport | 4 | 862, 863, 864, 865 |
| Ctx::kScissor | 6 | 728, 729, 730, 731, 732, 733 |
| Ctx::kBoneConstantBuffer | 1 | 924 |
| Ctx::kTransformConstantBuffer | 1 | 913 |
| Ctx::kDynVertexCursor | 8 | 1092, 1094, 1096, 1135, 1223, 1225, 1227, 1266 |
| Ctx::kDynVertexStride | 4 | 1084, 1142, 1214, 1276 |
| Ctx::kDynIndexCursor | 4 | 1236, 1238, 1240, 1269 |
| Ctx::kDynVertexBuffer | 8 | 1080, 1088, 1136, 1139, 1208, 1219, 1267, 1273 |
| Ctx::kDynVertexOffset | 4 | 1090, 1141, 1221, 1275 |
| Ctx::kDynVertexCount | 6 | 1085, 1145, 1148, 1157, 1215, 1296 |
| Ctx::kDynVertexBase | 2 | 1093, 1224 |
| Ctx::kDynInstanceCount | 6 | 1216, 1290, 1295, 1299, 1310, 1317 |
| Ctx::kDynIndexBuffer | 3 | 1232, 1270, 1279 |
| Ctx::kDynIndexOffset | 3 | 1234, 1280, 1308 |
| Ctx::kDynIndexCount | 4 | 1229, 1289, 1312, 1316 |
| Ctx::kDynIndexBase | 1 | 1237 |
| Ctx::kDefaultRenderBuffer | 1 | 499 |
| Ctx::kTrianglesSubmitted | 4 | 484, 1016, 1146, 1291 |
| Ctx::kVerticesSubmitted | 4 | 485, 1017, 1147, 1294 |
| Ctx::kDrawsSubmitted | 4 | 486, 1018, 1149, 1297 |
| Ctx::kObjectsSubmitted | 4 | 487, 1019, 1150, 1298 |
| Ctx::kTextureSlotCount | 1 | 832 |
| Ctx::kBoneDataBytes | 2 | 928, 931 |

### contextdata.cpp Dev:: / kConfigVSync use sites

| constant | n | lines |
|---|---|---|
| Dev::kWidth | 1 | 504 |
| Dev::kHeight | 1 | 505 |
| Dev::kOnDeviceLost | 1 | 659 |
| Dev::kOnDeviceRestored | 1 | 700 |
| Dev::kResetPending | 1 | 490 |
| Dev::kActiveDeviceData | 3 | 384, 494, 633 |
| kConfigVSync | 1 | 636 |

### devicedata.cpp Dev:: / Cfg:: use sites

| constant | n | lines |
|---|---|---|
| Dev::kWidth | 16 | 489, 539, 663, 779, 793, 803, 807, 826, 828, 839, 846, 875, 1159, 1165, 1265, 1275 |
| Dev::kHeight | 16 | 490, 540, 663, 780, 794, 804, 808, 827, 829, 840, 847, 876, 1160, 1167, 1266, 1276 |
| Dev::kModeIndex | 3 | 485, 776, 1158 |
| Dev::kRefreshIndex | 2 | 495, 843 |
| Dev::kAdapterIndex | 1 | 1138 |
| Dev::kOutputIndex | 1 | 1145 |
| Dev::kModeTable | 1 | 1157 |
| Dev::kInterfaces | 1 | 1739 |
| Dev::kInterfaceCount | 1 | 1741 |
| Dev::kHwnd | 7 | 508, 652, 741, 749, 1178, 1184, 1243 |
| Dev::kResetPending | 2 | 739, 884 |
| Dev::kMonitor | 1 | 1195 |
| Dev::kActiveDeviceData | 2 | 926, 956 |
| Cfg::kWindowWidth | 5 | 644, 664, 831, 833, 1164 |
| Cfg::kWindowHeight | 5 | 645, 665, 832, 834, 1166 |
| Cfg::kMultisample | 3 | 434, 441, 442 |
| Cfg::kFullscreen | 9 | 509, 778, 879, 928, 933, 940, 1050, 1063, 1163 |
| Cfg::kSkipShow | 1 | 1183 |

### imagebufferdata.cpp Ib* accessor use sites by raw offset (excluding definitions)

| offset | accessor | n | lines |
|---|---|---|---|
| 0x20 | IbI32 | 2 | 565, 590 |
| 0x20 | IbSet | 2 | 412, 441 |
| 0x24 | IbSet | 2 | 413, 442 |
| 0x28 | IbI32 | 4 | 551, 560, 614, 629 |
| 0x28 | IbSet | 2 | 414, 443 |
| 0x2c | IbSet | 2 | 415, 444 |
| 0x2c | IbU32 | 1 | 538 |
| 0x30 | IbI32 | 3 | 552, 579, 615 |
| 0x30 | IbSet | 2 | 416, 445 |
| 0x34 | IbSet | 2 | 421, 450 |
| 0x34 | IbU32 | 4 | 311, 368, 532, 550 |
| 0x38 | IbI32 | 1 | 336 |
| 0x38 | IbSet | 2 | 419, 448 |
| 0x3c | IbI32 | 1 | 337 |
| 0x3c | IbSet | 2 | 418, 447 |
| 0x40 | IbI32 | 1 | 338 |
| 0x40 | IbSet | 2 | 417, 446 |
| 0x44 | IbI32 | 1 | 335 |
| 0x44 | IbSet | 2 | 420, 449 |

---------------------------------------------------------------------------------------------------

## 6. Bridge-marker lines in src/render/dx11/

Searched `//.*(bridge|LEFT:|CAST-OK:)` over `src/render/dx11/funcs`, `src/render/dx11/src` (cpp + h) and `src/render/dx11/types`. Results: **zero** `LEFT:`, `CAST-OK:` or
`// B4 ...` markers anywhere in src/render/dx11/ (the B.4 pass left src/render/dx11/ untouched; its `CAST-OK: dx11_seam` tags are on the
RUNTIME side: `CompanionData.h` `DeletingDtor` vector arms x12, `src/app/video_interface.cpp:121/133` etc.). Only
bridge markers exist: `// B.3.5 bridge` (Runtime owner/base bridges), `// B.3.4 bridge` (System::Error/WideString —
mostly `Error::SetText(...)` calls, NOT casts), `// B.3.7 bridge` (`Application*`), `// B.3.1 bridge` (String), and the
new `// B5 W0 bridge` (contextdata.cpp:677). Rows below are every match; `(comment-only line)` rows are banner comments that mention
a bridge, not a marked statement. castmetric's own `bridge` column reads 51 for src/render/dx11/src (code lines carrying a marker
that is also a counted cast).

| file:line | code on the line | marker text |
|---|---|---|
| src/buffers.cpp:143 | `(comment-only line)` | // ENDGAME B.3.5 block J — the bridge cast the converted `Video::Device::Lock` / |
| src/buffers.cpp:148 | `return reinterpret_cast< ::Video::Device*>(VideoDeviceSingleton());` | // B.3.5 bridge |
| src/buffers.cpp:154 | `return reinterpret_cast< ::IO::Serializer*>(serializer);` | // B.3.5 bridge |
| src/buffers.cpp:260 | `(comment-only line)` | // B.3.5 bridge — the slot's registered ABI hands the serializer across the |
| src/buffers.cpp:560 | `(comment-only line)` | // cast is the bridge the declared leave (RULE B5-8) requires: this class |
| src/buffers.cpp:563 | `->Video::VertexBufferData::~VertexBufferData();` | // B.3.5 bridge |
| src/buffers.cpp:621 | `->Video::IndexBufferData::~IndexBufferData();` | // B.3.5 bridge |
| src/buffers.cpp:714 | `->Video::ConstantBufferData::~ConstantBufferData();` | // B.3.5 bridge |
| src/contextdata.cpp:183 | `(comment-only line)` | // `// B.3.5 bridge` cast where the receiver is still an untyped pointer this |
| src/contextdata.cpp:363 | `(comment-only line)` | // ENDGAME B.3.5 block J — the two bridge casts every converted call goes through. |
| src/contextdata.cpp:371 | `return reinterpret_cast< ::Video::Device*>(device);` | // B.3.5 bridge |
| src/contextdata.cpp:374 | `return reinterpret_cast< ::Video::Context*>(context);` | // B.3.5 bridge |
| src/contextdata.cpp:378 | `return reinterpret_cast< ::Video::VertexBuffer*>(vb);` | // B.3.5 bridge |
| src/contextdata.cpp:381 | `return reinterpret_cast< ::Video::IndexBuffer*>(ib);` | // B.3.5 bridge |
| src/contextdata.cpp:448 | `(comment-only line)` | // reached through a bridge cast because `VideoDX11::ContextData` is still the |
| src/contextdata.cpp:457 | `-> ::Video::ContextData::~ContextData();` | // B.3.5 bridge |
| src/contextdata.cpp:676 | `(comment-only line)` | // `virtual ~DeviceData()`). The bridge cast is the CTX block's to remove. |
| src/contextdata.cpp:677 | `::Video::DeviceData* const ddBase = reinterpret_cast< ::Video::DeviceData*>(dd` | // B5 W0 bridge |
| src/contextdata.cpp:693 | `System::Error::SetText(msg, true);` | // B.3.4 bridge |
| src/contextdata.cpp:694 | `System::Error::Display();` | // B.3.4 bridge |
| src/contextdata.cpp:905 | `(comment-only line)` | // `FieldPtr` — so they keep the raw-offset spelling and take the bridge cast. The |
| src/contextdata.cpp:911 | `(comment-only line)` | // B.3.5 bridge — an embedded ConstantBuffer reached by offset. |
| src/contextdata.cpp:922 | `(comment-only line)` | // B.3.5 bridge — the other embedded ConstantBuffer. |
| src/devicedata.cpp:310 | `(comment-only line)` | // ENDGAME B.3.5 block J — the two bridge casts every converted call goes through. |
| src/devicedata.cpp:316 | `return reinterpret_cast< ::Video::Device*>(device);` | // B.3.5 bridge |
| src/devicedata.cpp:319 | `return reinterpret_cast< ::Video::ImageBuffer*>(imageBuffer);` | // B.3.5 bridge |
| src/devicedata.cpp:995 | `(comment-only line)` | // re-enter this slot. The cast is the bridge the declared leave (RULE B5-8) |
| src/devicedata.cpp:1001 | `->Video::DeviceData::~DeviceData();` | // B.3.5 bridge |
| src/devicedata.cpp:1042 | `reinterpret_cast< ::Application::Application*>(app)->UnregisterMessageCallback` | // B.3.7 bridge |
| src/devicedata.cpp:1043 | `reinterpret_cast< ::Application::Application*>(app)->UnregisterMessageCallback` | // B.3.7 bridge |
| src/devicedata.cpp:1132 | `System::Error::SetText("VideoDX11: Couldn't create a dxgi factory!", false);` | // B.3.4 bridge |
| src/devicedata.cpp:1140 | `System::Error::SetText("VideoDX11: Unable to query adapter for dx11 interface!` | // B.3.4 bridge |
| src/devicedata.cpp:1147 | `System::Error::SetText("VideoDX11: Unable to query output for dx11 display!", ` | // B.3.4 bridge |
| src/devicedata.cpp:1217 | `"D3D_FEATURE_LEVEL_10_0 is required!", false);` | // B.3.4 bridge |
| src/devicedata.cpp:1230 | `"DXGI_FORMAT_B8G8R8A8_UNORM as a render target format!", false);` | // B.3.4 bridge |
| src/devicedata.cpp:1238 | `"DXGI_FORMAT_D24_UNORM_S8_UINT as a depth stencil format!", false);` | // B.3.4 bridge |
| src/devicedata.cpp:1249 | `System::Error::SetText("VideoDX11: Could not initialize the swap chain!", fals` | // B.3.4 bridge |
| src/devicedata.cpp:1267 | `System::Error::SetText(msg._data, false);` | // B.3.4 bridge |
| src/devicedata.cpp:1277 | `System::Error::SetText(msg._data, false);` | // B.3.4 bridge |
| src/devicedata.cpp:1285 | `reinterpret_cast< ::Application::Application*>(app)->RegisterMessageCallback(` | // B.3.7 bridge |
| src/devicedata.cpp:1287 | `reinterpret_cast< ::Application::Application*>(app)->RegisterMessageCallback(` | // B.3.7 bridge |
| src/devicedata.cpp:1363 | `reinterpret_cast<const ::Application::Application*>(AppOf())->GetInstance()),` | // B.3.7 bridge |
| src/devicedata.cpp:1551 | `(comment-only line)` | // B.3.1 bridge: GraphicsInterface::name is a POD slot on a struct this |
| src/devicedata.cpp:1577 | `::new (static_cast<void*>(&w)) System::WideString(adapterDesc.Description);` | // B.3.4 bridge |
| src/devicedata.cpp:1587 | `reinterpret_cast<const System::WideString*>(&w)->AsUTF8();` | // B.3.4 bridge |
| src/devicedata.cpp:1588 | `reinterpret_cast<System::String&>(rec) = utf8;` | // B.3.1 bridge |
| src/devicedata.cpp:1590 | `reinterpret_cast<System::WideString*>(&w)->~WideString();` | // B.3.4 bridge |
| src/devicedata.cpp:1626 | `const int len = System::WideString::StrLength(mi.szDevice);` | // B.3.4 bridge |
| src/devicedata.cpp:1628 | `::new (static_cast<void*>(&tailW)) System::WideString(mi.szDevice + (len - 1))` | // B.3.4 bridge |
| src/devicedata.cpp:1632 | `reinterpret_cast<const System::WideString*>(&tailW)->AsUTF8();` | // B.3.4 bridge |
| src/devicedata.cpp:1643 | `reinterpret_cast<System::WideString*>(&tailW)->~WideString();` | // B.3.4 bridge |
| src/dx11_backend_access.h:335 | `return reinterpret_cast<ImageBufferData*>(img->_data);` | // B.3.5 bridge |
| src/imagebufferdata.cpp:192 | `(comment-only line)` | // ENDGAME B.3.5 block J — the bridge cast the nine converted `Video::ImageBuffer` |
| src/imagebufferdata.cpp:198 | `return reinterpret_cast<const ::Video::ImageBuffer*>(imageBuffer);` | // B.3.5 bridge |
| src/imagebufferdata.cpp:378 | `(comment-only line)` | // bridge cast because `VideoDX11::ImageBufferData` is still the declared leave |
| src/imagebufferdata.cpp:385 | `-> ::Video::ImageBufferData::~ImageBufferData();` | // B.3.5 bridge |
| src/renderstatedata.cpp:369 | `(comment-only line)` | // base subobject, reached through the bridge cast because |
| src/renderstatedata.cpp:377 | `-> ::Video::RenderStateData::~RenderStateData();` | // B.3.5 bridge |
| src/shaderprograms.cpp:96 | `(comment-only line)` | // non-virtual call on the base subobject, reached through the bridge cast |
| src/shaderprograms.cpp:104 | `-> ::Video::VertexProgramData::~VertexProgramData();` | // B.3.5 bridge |
| src/shaderprograms.cpp:216 | `-> ::Video::PixelProgramData::~PixelProgramData();` | // B.3.5 bridge |
| src/smallclasses.cpp:84 | `(comment-only line)` | // non-virtual call on the base subobject, reached through the bridge cast |
| src/smallclasses.cpp:93 | `-> ::Video::VideoProgramData::~VideoProgramData();` | // B.3.5 bridge |
| src/smallclasses.cpp:138 | `(comment-only line)` | // `reinterpret_cast` is the bridge the declared leave (RULE B5-8) requires: |
| src/smallclasses.cpp:142 | `->Video::RenderBufferData::~RenderBufferData();` | // B.3.5 bridge |
| src/vertexlayoutdata.cpp:114 | `(comment-only line)` | // non-virtual call on the base subobject, reached through the bridge cast |
| src/vertexlayoutdata.cpp:122 | `-> ::Video::VertexLayoutData::~VertexLayoutData();` | // B.3.5 bridge |

---------------------------------------------------------------------------------------------------

## 7. Cross-class reach-ins inside src/render/dx11/

### 7.1 One class reading/writing another companion's fields

| from (file:line) | reaches into | what | helper |
|---|---|---|---|
| buffers.cpp:131-140, 360, 375, 426, 493, 538, 666-668 | DeviceData `d3dDevice`/`d3dContext` (+0x20/+0x28) via `Video::Device::_singleton` -> +0x100 | every Index/Vertex buffer op fetches the context/device | file-local `ActiveDeviceData()/ActiveContext()/ActiveDevice()` (a THIRD private copy of the walk) |
| buffers.cpp:754-756 | DeviceData `d3dDevice` via `owner->_device` (the ConstantBuffer's own Device, not the singleton) +0x100 | `ConstantBufferData_Create` | inline `reinterpret_cast`s (cast rows 754/755/756) |
| buffers.cpp:699, 881-893 | process-global `CbRegistry` (std::mutex + std::vector) | `Dx11UntrackConstantBuffer(self)` in the ConstantBuffer dtor; `Dx11TrackConstantBuffer` from the DeviceData factory (devicedata.cpp:1903) | decls dx11_backend_access.h:276-278 |
| devicedata.cpp:762 -> buffers.cpp:896-900 | **every `ConstantBufferData::activeIndex`** (writes -1) | `DeviceData_Resize` calls `Dx11InvalidateConstantBufferBindings()` after `ClearState()` | declared dx11_backend_access.h:278 |
| devicedata.cpp:1903 | ConstantBufferData registry | `Factory_ConstantBufferData` | `Dx11TrackConstantBuffer(p)` |
| contextdata.cpp:383-385, 508, 535-545, 633-654, ~669 | DeviceData `colorBuffer`, `swapChain`, `d3dDevice` (read), `d3dDevice = nullptr` (WRITE, :~669), `d3dContext` (read at :508 and cached into `self->d3dContext`) | BeginCommands / EndCommands / Finish | `OwnerDeviceData(self)` (Field<DeviceData*>(OwnerDevice(self), Dev::kActiveDeviceData)) and `Field<DeviceData*>(device, Dev::kActiveDeviceData)` :633 |
| contextdata.cpp:494 -> devicedata.cpp:1979 | DeviceData swap-chain resize | `Dx11_DeviceData_Resize(Field<void*>(device, Dev::kActiveDeviceData))`; the ONE external-linkage function of the DeviceData TU besides the slots | decl dx11_backend_access.h:368; def devicedata.cpp:1979-1981 (-> anonymous `DeviceData_Resize` :~739-890) |
| contextdata.cpp:677-679 | DeviceData slots 3 (`Shutdown`) and 2 (`CreateDevice`) | device-lost recovery in `Finish`; W0: by name through `::Video::DeviceData*` (HEAD: by raw vtable index via `dd->vptr`) | bridge cast :677 |
| contextdata.cpp:790, 799 | ImageBufferData `renderTargetView` / `depthStencilView` of the render buffer's images | `Clear` | `Dx11ImageRtv/Dsv(Video::ImageBuffer*)` dx11_backend_access.h:340-347 -> `Dx11ImageBackend` :333-336 (`Video::ImageBuffer::_data` @0x60) |
| smallclasses.cpp:192, 195, 197, 237-238 | ImageBufferData `renderTargetView` / `depthStencilView` / `texture` of both render buffers; DeviceData `d3dContext` | `RenderBufferData::Bind` / `::Resolve` | `Dx11ImageRtv/Dsv/Texture`, `Dx11Context()` |
| shaderprograms.cpp:145, 193, 227, 255; vertexlayoutdata.cpp:145, 227; smallclasses.cpp:197, 237 | DeviceData `d3dDevice` / `d3dContext` | all Create/Bind in those classes | `Dx11Device()/Dx11Context()` (dx11_backend_access.h:113-120) over `Dx11DeviceData()` :108-111 |
| imagebufferdata.cpp:208-213, 351, 369, 407, 438, 529, 665, 678, 717 | DeviceData `d3dDevice`/`d3dContext`/`colorBuffer`/`depthBuffer` | all Create/Bind/Update/Read/Attach | file-local `ActiveDeviceData(void* device)`, `D3DDevice`, `D3DContext`, `VideoDevice()` (a FOURTH private copy) |
| imagebufferdata.cpp:228-259, 263-273 (called 369, 429, 461, 533) | DeviceData `trackedImages` / `trackedCount` / `trackedCapacity` (+0x60/+0x68/+0x6c): **grows and shift-erases** the registry the DeviceData dtor frees (devicedata.cpp:987-990) | `TrackImageBuffer` (retail 0x180002300, "shared with DeviceData", file comment says to hoist it when DeviceData lands — never hoisted) and `UntrackImageBuffer` (inlined ~ImageBufferData) | file-local; NOTHING in src/render/dx11/ ever walks the registry (device-reset release/recreate is not implemented here) |
| imagebufferdata.cpp:424, 453 | DeviceData `colorBuffer` / `depthBuffer` | `AddRef` into the ImageBuffer's own `texture` | static_cast<IUnknown*> |
| renderstatedata.cpp:316-321, 395, 514 | DeviceData `d3dDevice` / `d3dContext` | Create / Bind (with a dead `Dx11OriginalSlot` arm when `dd` or the field is null) | file-local `ActiveDeviceData()` (a FIFTH copy; null-checks, unlike the others) |
| exports.cpp:64-86 | DeviceData all 14 private fields | `Dx11_CreateDeviceData` zero-init | n/a |
| devicedata.cpp:1869-1970 | every other class's private tail (writes the initial values) | the eleven factories | n/a |

There are FIVE separate implementations of "Video::Device::_singleton -> +0x100 -> d3dDevice/d3dContext": `Dx11DeviceData()`
(dx11_backend_access.h:108), `ActiveDeviceData()` in buffers.cpp:131, `ActiveDeviceData(void*)` in imagebufferdata.cpp:208, `ActiveDeviceData()`
in renderstatedata.cpp:316, and `OwnerDeviceData` in contextdata.cpp:383 (which walks `context->_device` instead of the singleton).

### 7.2 Seam / vtable machinery uses (file:line)

| symbol | defined | uses |
|---|---|---|
| `Vt` enum, `VtName/VtSlotCount/VtGhidraAddr` | dx11_seam.h:61 (`enum class Vt`), :~76-79 / dx11_seam.cpp:~35-48 | dx11_seam.cpp only (+ `Vt::X` arguments in the uses below) |
| `Dx11LiveVtable(Vt)` | dx11_seam.h:90 decl; dx11_seam.cpp:77 | buffers.cpp:553, 614, 700; imagebufferdata.cpp:363; renderstatedata.cpp:353; dx11_backend_access.h:379 (inside `Dx11VtableAddress`); dx11_seam.cpp:213 (report) |
| `Dx11VtableAddress(Vt)` (inline over the above) | dx11_backend_access.h:374-380 | contextdata.cpp:440; devicedata.cpp:985, 1866 (`AllocData`, the shared factory re-seat); exports.cpp:69; shaderprograms.cpp:86, 207; smallclasses.cpp:77, 132; vertexlayoutdata.cpp:105 |
| `Dx11SeamInstall()` | dx11_seam.h:147; dx11_seam.cpp:143 | **src/app/main.cpp:1249** (Init step 4b) — only caller; has the new W0 zero-registration skip arm (:150-160) |
| `Dx11SeamReady()` | dx11_seam.h:150; dx11_seam.cpp:84 | none (dead; only defined/declared) |
| `Dx11SeamReport()` | dx11_seam.h:153; dx11_seam.cpp:204 | `std::atexit` registration dx11_seam.cpp:199; **src/app/main.cpp:547** |
| `Dx11OriginalSlot(Vt,int)` + `Dx11ForwardCount()` | dx11_seam.h:102 / :105; dx11_seam.cpp:101 / :99 | `Orig<>` templates: contextdata.cpp:424 (template only — NO instantiation anywhere in contextdata.cpp), devicedata.cpp:410 (instantiated at :1126 slot 2, :1752 slot 1); direct: renderstatedata.cpp:398-399 (slot 1), :517-518 (slot 2). `Dx11ForwardCount()` has no caller. Every one is a "Runtime import failed to resolve" arm that cannot fire |
| `Orig<Fn>(slot)` | contextdata.cpp:423-425; devicedata.cpp:409-411 | see above |
| `Dx11RegisterSlot` / `DX11_REGISTER_SLOT` | dx11_seam.h:120 / 132-138; dx11_seam.cpp:118 | the 84 registrations (section 1) |
| `Dx11_DeviceData_Resize` | dx11_backend_access.h:368 decl; devicedata.cpp:1979 | contextdata.cpp:494 |
| `Dx11TrackConstantBuffer / Untrack / InvalidateConstantBufferBindings` | buffers.cpp:881 / 887 / 896; decl dx11_backend_access.h:276-278 | devicedata.cpp:1903 / buffers.cpp:699 / devicedata.cpp:762 |
| `kSlot_*`, `kCtx_*`, `kImg_*`, `kRS_*` enums, `kSlots::*`, `DataBase` | types/VideoDX11Data.h:27-33, 136-166, 225-247, 293-302, 517-522, 579-599 | **no users in the tree now** (the only two, `kSlot_Shutdown`/`kSlot_CreateDevice` at contextdata.cpp HEAD:671/673, went with the W0 edit). `kSlots::kTotal` has its own `static_assert == 84` |
| `Dx11VerifyDeviceSingleton()` | dx11_backend_access.cpp:11 (decl dx11_backend_access.h:127) | none (dead; the whole .cpp is 19 lines) |
| `Dx9Placeholder_CreateDeviceData / GetDeviceName` | devicedata.cpp:2055 / 2056 | src/app/video_interface.cpp:116-121 (via local extern :62-63) |
| `Dx11_CreateDeviceData / Dx11_GetDeviceName` | exports.cpp:64 / 101 | src/app/video_interface.cpp:132-141 (via local extern :56-60) |

Outside src/render/dx11/ nothing includes `VideoDX11Data.h` except in comments (src/engine/save/saveload_draw.cpp:770, src/engine/video/buffers.cpp:15-18,
src/engine/video/companion_data.cpp:157, src/engine/video/Buffers.h:11, CompanionData.h:46/144) and `tests/unit/video_data_abi_test.cpp:356,379`
(comments); `tests/unit/b39_vid_data_test.cpp` is under W0 edit.

---------------------------------------------------------------------------------------------------

## 8. Per-file line counts and proposed partition

### 8.1 Line counts (working tree, `wc -l`)

| file | lines | | file | lines |
|---|---|---|---|---|
| src/devicedata.cpp | 2057 | | src/vertexlayoutdata.cpp | 247 |
| src/contextdata.cpp | 1373 | | funcs/dx11_seam.h | 155 |
| src/buffers.cpp | 948 | | funcs/dx11_seam.cpp | 228 |
| src/imagebufferdata.cpp | 767 | | src/exports.cpp | 111 |
| types/VideoDX11Data.h | 601 | | src/devicedata_stubs.cpp | 71 |
| src/renderstatedata.cpp | 545 | | src/dx11_backend_access.cpp | 19 |
| src/dx11_backend_access.h | 388 | | **total (15 files)** | **8050** |
| src/shaderprograms.cpp | 278 | | | |
| src/smallclasses.cpp | 262 | | | |

(READMEs excluded: src/render/dx11/README.md, src/render/dx11/SOURCES.md. Both are STALE, see "surprises".)

### 8.2 What the shared headers contain, by section (so blocks can cite ranges)

`types/VideoDX11Data.h` (601): `DataBase` 27-33 (unused); `DeviceData` struct+asserts 51-125, slot enum 136-166; `ContextData` 200-212,
enum 225-247; `ImageBufferData` 272-288, enum 293-302; `ConstantBufferData` 316-332; `IndexBufferData` 348-362; `VertexBufferData`
366-380; shared Index/Vertex asserts 389-396; `VertexLayoutData` 415-423; `VertexProgramData` 446-455; `PixelProgramData` 468-477;
`RenderStateData` 498-510, enum 517-522; `VideoProgramData` 537-543; `RenderBufferData` 566-572; `kSlots` 579-599.

`src/engine/video/CompanionData.h` (941): `RtVideo::CompanionData` 49-54 (+ `ConstantBuffer` alias :84); bases begin at
`Device` fwd 205; `DeviceData` 236, `ContextData` 305, `ImageBufferData` 369, `IndexBufferData` 432, `VertexBufferData` 493,
`ConstantBufferData` 542, `RenderBufferData` 605, `VertexLayoutData` 647, `VertexProgramData` 690, `PixelProgramData` 731,
`RenderStateData` 772, `VideoProgramData` 817; `RT_VIDEO_COMPANION_PINS` 875-894; the twelve
`!std::has_virtual_destructor` asserts :896-937 (these must flip when each class derives).
`src/engine/video/companion_data.cpp` and `companion_operators.cpp` own the ctors/dtors and the W0 `operator new/delete`.

### 8.3 Proposed blocks

Constraint analysis. (1) `VideoDX11Data.h` holds all twelve structs and `devicedata.cpp` holds all eleven factories, so any
per-class block touches TWO shared files. (2) The seam's W0 arm already tolerates a flipped class (zero registrations =
skipped table), so classes can flip one at a time; but `AllocData` (devicedata.cpp:1866) and every slot-0 re-seat write
`vptr = Dx11VtableAddress(...)`, and `Dx11LiveVtable` for a flipped class returns a pointer to a NULL-filled array, not
nullptr — a leftover re-seat after a flip silently installs a null table. (3) DeviceData (the factory table and the
`Dx9Placeholder`) depends on all eleven classes being complete types for covariant returns, so it goes LAST; ContextData needs
DeviceData's fields (`d3dContext`, ...) only, which the W0 bridge already isolates.

Recommended prelude (mechanical, one agent, no behaviour change, before any class block): **P0** — (i) split
`VideoDX11Data.h` into one header per class (`src/render/dx11/<Class>Data.h`) with `VideoDX11Data.h` as an umbrella include, so
the per-class blocks stop sharing a file; (ii) move each `Factory_X` function and its
`DX11_REGISTER_SLOT(DeviceData, 9..19, ...)` row out of `devicedata.cpp` into the class's own .cpp (the registration is by
slot number from any TU), and `AllocData` into a shared header. After P0 the blocks below are file-disjoint. P0 touches:
VideoDX11Data.h, devicedata.cpp (factories 1861-1970 + registrations 2029-2049), plus one new header per class.

| block | files owned (lines) | shared headers it edits | depends on | notes |
|---|---|---|---|---|
| **L1 VertexLayoutData** | vertexlayoutdata.cpp (247) + its header | VertexLayoutData header; CD.h `VertexLayoutData` 647-688 only if a signature changes (none needed: 3/3 MATCH) | P0 | smallest; 3 slots; one (b), one (c), two (a) casts; the pattern block for the others |
| **L2 Vertex/PixelProgramData** | shaderprograms.cpp (278) + 2 headers | header sections 446-477; `dx11_backend_access.h` only for `kShaderVariantCount` (read-only) | P0 | 6/6 MATCH, `variants[4]` -> `ID3D11VertexShader*[4]` / `ID3D11PixelShader*[4]` |
| **L3 RenderStateData** | renderstatedata.cpp (545) + header | header 498-522; CD.h `RenderStateData` 772-815 | P0 | slot 0 return is a MISMATCH; also owns the last duplicate view (`VideoRenderState` :124-166 vs real `Video::RenderState`); removes the dead `Dx11OriginalSlot` arms (:398, :517) |
| **L4 VideoProgramData + RenderBufferData** | smallclasses.cpp (262) + 2 headers | header 537-543, 566-572; reads `Dx11ImageRtv/Dsv/Texture` (dx11_backend_access.h:340-347) | P0; the `Dx11ImageBackend` bridge dies with L6 | trivially small; RenderBufferData consumes ImageBufferData fields |
| **L5 Index/Vertex/ConstantBufferData** | buffers.cpp (948) + 3 headers | header 316-396 (incl. the shared-layout asserts); `dx11_backend_access.h:276-278` (registry decls) | P0 | 3 slot-0 MISMATCHES; the shared `Lock/Unlock` body must become a template/free helper called by both overrides; owns the `CbRegistry` (devicedata.cpp:762 calls `Dx11InvalidateConstantBufferBindings`, keep the signature) |
| **L6 ImageBufferData** | imagebufferdata.cpp (767) + header | header 272-302; `dx11_backend_access.h:333-347` (`Dx11ImageBackend` + 3 accessors, which L2/L4/L7 call); CD.h `ImageBufferData::Create(const void*)` or retype | P0; hosts `TrackImageBuffer`/`UntrackImageBuffer` (touch DeviceData `trackedImages` fields, so the field types are shared with L8) | the heaviest per-class cast load (76 rows, 17 `Ib*` accessors, 20 `IbSet`); `Create` is the one non-slot-0 MISMATCH |
| **L7 ContextData** | contextdata.cpp (1373) + header | header 200-247; none else | P0; reads DeviceData fields; calls `Dx11_DeviceData_Resize` (L8's) | 20/20 MATCH; owns the ~115 `Field<T>/FieldPtr` lines -> typed `Video::Context` members (`_dirty`, `_renderBuffer`, ... see 5.2); two unnamed halves (0x2104/0x2124) |
| **L8 DeviceData** (LAST) | devicedata.cpp (2057) + devicedata_stubs.cpp (71) + exports.cpp (111) + header | header 51-166; `dx11_backend_access.h:90-120` (`Dx11DeviceData/Dx11Device/Dx11Context` walk), `:368` (Resize decl) | L1-L7 all landed (needs complete derived types for the 11 covariant factory returns), or keep the factories registered in the hand table until then | slot 0 MISMATCH; 11 factory returns; stubs 4-7 become inline overrides; `Dx9Placeholder` is orthogonal (own hand vtable) but lives here; `DeviceData::device` -> `_owner`, `vptr` field removed; typed COM members; `Dx11_CreateDeviceData` already typed by W0 |
| **CLOSE** (coordinator) | dx11_seam.{h,cpp}, dx11_backend_access.{h,cpp}, README(s), src/app/main.cpp:547/1249, src/app/video_interface.cpp:56-64 | CD.h: 12 `!has_virtual_destructor` asserts (896-937) flip, `RT_VIDEO_COMPANION_PINS` | all | deletes `Dx11OriginalSlot`/`Orig<>`, `Dx11RegisterSlot`, `DX11_REGISTER_SLOT`, `Vt`, `kSlots`, `kSlot_*` enums, `DataBase`, `Dx11SeamReady/Report/ForwardCount`, `Dx11VerifyDeviceSingleton`; adds a real header for the two exports (kills the local externs) |

Order suggestion: P0, then L1 (pattern), then L2/L3/L4/L5 in parallel (disjoint files), then L6, then L7, then L8, then CLOSE.
L6 before L4/L7 only matters for the `Dx11ImageBackend` bridge, not for correctness. At most 4 agents in parallel (memory note).

---------------------------------------------------------------------------------------------------

## Things that surprised me

1. **The tree moved under the inventory.** Another party's uncommitted "B5 W0" edits landed in src/render/dx11/, `CompanionData.h`,
   `companion_data.cpp`, `video_interface.cpp`, tests and `B5_RULES.md` while this was written (contextdata.cpp changed
   mid-run: 240 -> 238 casts, +6 lines after :670, the by-index vtable reads of DeviceData slots 3/2 became by-name calls).
   The 11 factory OWNER parameters are typed in the working tree but `void*` at HEAD; the typed returns are still `void*`.
2. **Stale worktree copy**: `.claude/worktrees/oldbase/` holds a complete older copy of `src/render/dx11/` (it shows up in any repo-wide grep for
   `Dx11LiveVtable`, `Dx11SeamInstall`, ...). Scope greps to `src/render/dx11/`, `standalone/`, `runtime/`.
3. **A fifth private copy of the Device->DeviceData->D3D walk**, plus `Field<>` defined twice (contextdata.cpp:308, devicedata.cpp:301),
   plus literal `0x100` in four files, although `Video::Device::deviceData` is a typed, asserted member (Device.h:923).
4. **`renderstatedata.cpp:124-166` still defines a local `VideoRenderState` (0x80)** — the one duplicate view of a Runtime class that
   survived the B.3.5 collapse, although `Video::RenderState` is a real class (src/engine/video/RenderState.h:111).
5. **ContextData slot 17 is registered to the slot-19 body** (retail's `jmp`), and Index/Vertex `Lock`/`Unlock` are one body typed on
   `IndexBufferData*` registered in both tables: an `override` conversion needs forwarders/a template in both places.
6. **Dx9Placeholder is a second hand-built vtable inside src/render/dx11/** (4 entries, plain `new`/`delete`, devicedata.cpp:1777-1806) that the seam
   deletion does not touch, and it is reached by `src/app/video_interface.cpp` through LOCAL `extern` declarations (:56-64) — the
   two plugin exports have no header.
7. **`Dx11LiveVtable` for a flipped class returns a pointer to a null-filled array, not nullptr** (dx11_seam.cpp:77-82 + the W0 skip arm),
   so a forgotten `self->vptr = Dx11VtableAddress(...)` after a flip installs a null table with no diagnostic.
8. **`TrackImageBuffer`'s registry is write-only inside src/render/dx11/**: `DeviceData::trackedImages` is appended/erased by ImageBufferData and freed by
   the DeviceData dtor, but nothing in src/render/dx11/ ever iterates it (`DeviceData_Resize` only calls `ClearState` + resizes + `Dx11InvalidateConstantBufferBindings`).
   The CbRegistry (std::mutex + vector) is a deliberate non-retail deviation and the only `std::` container state in src/render/dx11/.
9. **Base param named `detail` shadows `namespace detail`** in `VideoProgramData::Bind(void*, int detail)` (CD.h:864) whose body is `detail::CompanionPureCall()`.
   It compiles (nested-name-specifier lookup skips non-namespaces) but a rename would be safer.
10. **`Dx11SeamReady`, `Dx11ForwardCount`, `Dx11VerifyDeviceSingleton`, `DataBase`, all `k*Slot` enums and `kSlots` have no remaining users**;
    the `Orig<>` template in contextdata.cpp:423 is never instantiated.
11. **READMEs are stale**: `src/render/dx11/SOURCES.md:35-50` is titled "What is deliberately NOT here (RULE B5-8)" and says the concrete classes stay
    C structs and that "Imports / g_deps rows stay" (those rows are gone since B.3R DX) — the opposite of B5. `src/render/dx11/VideoDX11Data.h`
    header comments still say "Fields are void* so this shared header does not force <d3d11.h>" which is the (a)-row count's root cause.
12. **Slot-0 semantic gap**: base `DeletingDtor(unsigned)` implements the `flags & 2` vector arm; no dx11 slot-0 body does (only `& 1`).
    Also dx11's slot-0 free functions return the concrete type in five classes (the 5 return MISMATCHES), `void*` in the other seven.
13. **Alignment**: all factories pass alignment 4 to `MemAlloc` for 8-aligned structs (retail's value); the W0 base `operator new` reproduces it.
14. **`ImageBufferData_Bind`'s parameter is still named `unknownArg`** though CD.h:416 (block J) proved it is the Context.
15. `castmetric` does not see the ~230 `Field<T>(base, kConst)` / `IbSet` uses (the cast is inside the definition); the 240/238 figure
    understates the raw-offset traffic ~4x. Section 5.6 indexes them.
