# B5 independent audit (the dx11 seam)

**Range:** `d14517b71` to `b5-CLOSE` (`f57caa085`). Every line number below is a line of `f57caa085` (`git show f57caa085:<path>`); the branch ref is gone, and the merge `020a4e3ae` has an identical tree under dx11/, runtime/ and standalone/. The audit was read-only: nothing was built or run. The old free functions were compared against the new members statement by statement, using the per-file `git diff` plus side-by-side extracts of both versions.

**Summary:** 88 OK, 13 FINDING, 2 QUESTION.

- **No behaviour finding.** All 84 slot bodies, the 12 destructors, the 12 constructors, the 19 `delete` sites and the Adapter / GraphicsInterface members reproduce the old code. Each has the same statements in the same order, the same D3D calls and arguments, the same guards, and the same releases.
- **All 13 FINDINGs are text.** Twelve are comments that now say something false, mostly about the seam, `void*` fields or hand tables. One is a dead struct (`VideoDX11::DataBase`). One acceptance sentence in B5_RULES §7 cannot be met as written.
- **Two QUESTIONs:** whether "the ONE downcast" should cover `Dx11ImageBackend`, and the bool-typed RenderState fields that dx11 now reads.
- **Residue:** no seam machinery is left in code (`DX11_REGISTER_SLOT`, `Dx11VtableAddress`, `Dx11SeamInstall`, `Dx11LiveVtable`, `AllocData`, `self->vptr` all have 0 code hits). No Video companion `DeletingDtor` call remains.

---

## 1. Slot bodies are the same code

| # | class / slot(s) | b5-CLOSE location | verdict | evidence |
|---|---|---|---|---|
| 1.1 | DeviceData::Shutdown [3] | dx11/src/devicedata.cpp:1020-1051 | OK | Same order as before: unregister 0x20, then 6, DestroyCursor, fullscreen restore, release depth / colour / context, `SetFullscreenState(FALSE)` + release of the swap chain, release device. Only `self->` and the casts were removed. |
| 1.2 | DeviceData::CreateDevice [2] | devicedata.cpp:1103-1270 | OK | Steps are identical. The `Field<>` reads became named Device / Configuration members of the same width (uint8 `fullscreen` / `field42`, int32 window / multisample). The out-params are typed now. The `Orig<>(2)` arm became `return false` (D9, ruled). No guard was added or dropped. |
| 1.3 | Resize (0x180004340) + OnActivate, OnSetCursor, ResolveMultisampleMode, FillSwapChainDesc, CreateAttachment | devicedata.cpp:403-920 | OK | Every statement is kept in order. `settingsChanged` (+0x9c) is uint8, as the old `Field<uint8_t>` was. The new callback signatures have the same x64 ABI. |
| 1.4 | DeviceData::SetImageBuffer [8] | devicedata.cpp:1301-1353 | OK | Same identity test, DestroyCursor, Lock / CopyFromTexture / Unlock, CreateCursor and stores. The local was renamed `cursor` to `built` because it now collides with the member. |
| 1.5 | EnumerateInterfaces [1] (DX11 and DX9 placeholder) | devicedata.cpp:1734-1736, :1885-1890; dx11/src/dx9_placeholder.cpp:45-47 | OK | Both go through `Dx11EnumerateDxgiInterfaces`, which does the same import null test and the same walk. The `Orig<>(1)` arm became `return false` (D9). |
| 1.6 | Stubs [4]-[7] | dx11/src/devicedata_stubs.cpp:42-57 | OK | `ret` / `0.0f` / `false` / `true`, unchanged. |
| 1.7 | The 11 factories [9]-[19] | devicedata.cpp:1797-1871 | OK | Each is `return new XData(owner)`. The `if (!p) return nullptr` arm was dropped (D2, ruled). |
| 1.8 | ContextData::BeginCommands [1] | dx11/src/contextdata.cpp:405-434 | OK | The four counters are zeroed first. Then `settingsChanged` leads to `Dx11_DeviceData_Resize` and `return false`. Then Lock, `SetRenderBuffer(_defaultRenderBuffer)`, viewport `{0,0,w,h}`, `d3dContext = dd->d3dContext`. Same order and widths as old :482-510. |
| 1.9 | EndCommands [2] | contextdata.cpp:458-474 | OK | AddRef(colour), `GetBuffer(0, IID, &bb)` with no hr check, Resolve(…, 0x57), Release(colour), Release(bb). Identical. |
| 1.10 | Finish [3]: Present / hr handling | contextdata.cpp:502-528 | OK | vsync is Configuration+0x41 (u8). Present, then Unlock, then the injection latch (unchanged). Then: return on `hr == 0`, the REMOVED / RESET test, the `GetDeviceRemovedReason` query, return on `hr == 0`. Same as old :631-655. |
| 1.11 | Finish: callbacks, release / recreate order | contextdata.cpp:538-557, :576-583 | OK | The owner is `device->deviceData->_owner`, the same object and field as the old `dd->device`. Then `onLost()`, ReleaseDynamic, **`dd->d3dDevice = nullptr` before Shutdown** (kept), Shutdown, CreateDevice (now virtual calls through the base, i.e. retail slots 3 / 2 rather than a hand-table index). The failure branch is identical. Then RecreateDynamic and onRestored. |
| 1.12 | Finish: the new diagnostic (B5_RULES §6a) | contextdata.cpp:545-549 | OK (note) | One added `fprintf` of `d3dDevice->GetDeviceRemovedReason()`. It sits after the onLost print and before `onLost()` and the null store. It only queries; no state changes. Note: this is the one new deref in the step (the old lost path never dereferenced `d3dDevice`). The pointer cannot be null there: Present just ran on the swap chain of that device, and the null store comes later. |
| 1.13 | Clear [4] | contextdata.cpp:636-669 | OK | Byte test on dirty bit 0x01, then u32 tests on 0x200 / 0x400. The BindRenderBuffer / BindViewport calls are qualified and non-virtual, as before. Mask 0xfffff9fe. RTV / DSV clears are identical. |
| 1.14 | BindRenderBuffer / BindViewport / BindScissor / BindTransforms [5]-[8] | contextdata.cpp:694-780, helper :597-605 | OK | Same re-reads, same operand order (`w+x`, `h+y`). The offsets 0x2008 / 0x2050 / 0x10d8 / 0x418 are asserted in Context.h. |
| 1.15 | [9]-[13] (TextureNone, IndexNone, VertexNone, DrawIndexed, GetMaxDynamicInstances) | contextdata.cpp:791-900 | OK | Identical D3D calls and arguments. Topology is forced to 4. `0x34` is returned. |
| 1.16 | Vertex paths [14]-[19] | contextdata.cpp:922-1152 | OK | Signed size compare; `Lock(n, _unknown2100)` takes an `int&` on the int32 at +0x2100. The retail capacity bug is kept. Unlock order is V then I. Slot 16 is the same qualified call to slot 18 with `instanceCount = 1`. Slot 17 is a qualified call to `EndInstancedIndexedVertices`; before, it was registered to the same function directly, so the same body runs. |
| 1.17 | ImageBufferData::AttachColorBuffer / AttachDepthBuffer [1]/[2] | dx11/src/imagebufferdata.cpp:410-475 | OK | The `IbSet` raw-offset stores became named `Video::ImageBuffer` int32 fields (`_width` 0x20 … `_filter` 0x44, all `int32_t` / `uint32_t`, asserted at ImageBuffer.h:434-443) in the **same store order** (0x40, 0x3c, 0x38, 0x44, then flags 0x34 last). Then AddRef, the texture store, the RTV / DSV create, and the tail TrackImageBuffer. |
| 1.18 | ImageBufferData::Create [3] | imagebufferdata.cpp:482-610 | OK | Same degenerate early return, track test (bit 6 only), ReleaseInterfaces, desc fields, level walk (`levels->GetSize()` = count at +8; the element's `GetData()` = element +0x00), SRV / RTV / DSV ladder, and unconditional CreateSamplerState + MemFree. `_kind` is cast to `uint32_t` explicitly, matching the old `IbU32`. `Dx11Device()` re-walks where the old code reused the cached `dd`: same object, no write in between. |
| 1.19 | ImageBufferData::Bind / Update / Read [4]-[6] | imagebufferdata.cpp:617-700 | OK | Sampler, then SRV. Map / memcpy / Unmap or UpdateSubresource. Staging create (hr ignored), CopyResource, Map READ_WRITE, the row loop, Unmap, Release. The Read loop's `out` / `in` locals are byte views of the same pointers. |
| 1.20 | CreateSamplerState / ReleaseInterfaces (private) | imagebufferdata.cpp:306-375 | OK | Release order is unchanged: +0x28, +0x20, +0x30, +0x10, +0x18, then five nulls. Configuration+0x28 and Device+0x98 are int32 named members (Device.h:590/739, asserted :945/:976). |
| 1.21 | ConstantBufferData::Create [1] | dx11/src/buffers.cpp:689-748 | OK | The device is still taken from the OWNER (`Dx11DeviceDataOf(owner->_device)`, i.e. +0x38, then +0x100, then +0x20). `ArraySetLength` takes the three fields by reference: same growth, same MemAlloc(…, 8), same copy loop, same MemFree. `hr = -1` seed and loop unchanged. |
| 1.22 | ConstantBufferData::Bind [2] | buffers.cpp:760-783 | OK | Same index formula, same `activeIndex` compare / store, PS-vs-VS on `_bindToVertex`, then `UpdateSubresource(buffers[activeIndex], 0, nullptr, src, 0, 0)`. |
| 1.23 | ConstantBufferData::SlotIndexPlusOne [3] | buffers.cpp:791-793 | OK | `_owner->_uploadIndex + 1`. |
| 1.24 | Index / VertexBufferData::Create / SetData [1]/[2] | buffers.cpp:350-385, 473-500 (helpers); 518-527, 577-590 (members) | OK | `GpuBuffer_Create` / `GpuBuffer_SetData` are templates over the owner type with the same body. Bind flags 1 / 2. `dstSubresource` 0 vs `offsetBytes` (the retail bug is kept). |
| 1.25 | Shared Lock / Unlock [3]/[4] | buffers.cpp:304-335 (templates), members :531-539 / :593-600 | OK | One template body each, called from both classes' members (D3, no cross-class cast). DISCARD vs NO_OVERWRITE test, `locked = 1`, cursor advance `((n+1)/2)*2`, Unmap then `locked = 0`. Unchanged. |
| 1.26 | Index / VertexBufferData::Bind [5] | buffers.cpp:545-553, 622-624 | OK | `IASetVertexBuffers(startSlot, 1, &buffer, &stride, &offset)` / `IASetIndexBuffer(buffer, R16_UINT, 0)`. |
| 1.27 | Index / VertexBufferData::Serialize [6] | buffers.cpp:395-465, members :556 / :627 | OK | The encoding gate `vtable[0](encoding)` became `GetEncoding().IsBinary()`. `IO::Encoding` declares `IsBinary` first with no virtual dtor (src/engine/io/Serializer.h:379-381), so this is the same slot 0. `owner->dynamic` is `bool` (Buffers.h:144), so `Serialize(bool&)` picks the same overload the old `reinterpret_cast<bool&>` did. Same order: dynamic, then size. The re-create callback is `self->Create` on a `final` class, the same direct call the old function pointer made. |
| 1.28 | RenderStateData::Create / Bind [1]/[2] | dx11/src/renderstatedata.cpp:321-455 | OK | The null-`dd` arms return `true` / return. The old `Dx11OriginalSlot` lookup was always null, so the result is the same (D9). The `VideoRenderState` view became the named `Video::RenderState` fields at the same offsets (RenderState.h:325-369). Call order is Blend, Rasterizer, DepthStencil. The stencil ref is a byte (`sizeof == 1` asserted). |
| 1.29 | RenderStateData::Create: bool-typed fields | renderstatedata.cpp:337-410 | QUESTION | See Q2. The old view read `uint8_t blendEnabled / depthEnabled / depthWriteEnabled / stencilEnabled / stencilTwoSided / scissorEnabled`. The real class types them `bool` (B.4 R-VIDEO, H4), so GCC may test bit 0 instead of `!= 0`. This is identical for every 0 / 1 byte. |
| 1.30 | VertexLayoutData::Bind / PlatformCreate [1]/[2] | dx11/src/vertexlayoutdata.cpp:123-216 | OK | `IASetInputLayout(inputLayout)` (unchecked, as before). Same element build and `CreateInputLayout(…, &inputLayout)`. |
| 1.31 | Vertex / PixelProgramData::PlatformCreate / Bind [1]/[2] | dx11/src/shaderprograms.cpp:122-231 | OK | Same `i >= _byteCode.GetSize()` skip, Create*Shader into `&variants[i]`, `return false` on hr. Bind picks `variants[p] ? p : 0`, then VSSetShader / PSSetShader. |
| 1.32 | VideoProgramData [1]/[2], RenderBufferData [1]-[3] | dx11/src/smallclasses.cpp:83-208 | OK | `true`, empty, `true`. Bind: the RTV / DSV of `_owner`'s two ImageBuffers, then OMSetRenderTargets. Resolve: `ResolveSubresource(dst tex, 0, src tex, 0, 0x57)`, unguarded as before. |

**Item 1 verdict: 31 OK, 1 QUESTION.**

## 2. Destructors

INVENTORY_A §3 records what each old slot-0 body did: re-seat the vptr, release, call the qualified base dtor, then `MemFree` on `flags & 1`. In each `~XData()` only the releases remain, in the old order. Nothing was added.

| class | b5-CLOSE | releases (order) | verdict |
|---|---|---|---|
| DeviceData | devicedata.cpp:982-987 | `MemFree(trackedImages)`, then three stores (`nullptr`, 0, 0). No COM release, as before (that is slot 3's job). | OK |
| ContextData | contextdata.cpp:384-385 | Nothing (`d3dContext` is borrowed). | OK |
| ImageBufferData | imagebufferdata.cpp:392-400 | Untrack (bits `IsGenerated` / 0x70 / `IsRendered`), then `ReleaseInterfaces()` (+0x28, +0x20, +0x30, +0x10, +0x18, five nulls). | OK |
| ConstantBufferData | buffers.cpp:660-676 | `Dx11UntrackConstantBuffer(this)`, Release `[0, count)`, `MemFree(buffers)` (unconditional), three clears. The compiler's vptr store now comes BEFORE the untrack instead of after. The untrack makes no virtual call, which the comment states. | OK |
| IndexBufferData / VertexBufferData | buffers.cpp:567-572 / 509-514 | Release `buffer` if set, then `buffer = nullptr`, `writeCursor = 0`, `locked = 0`. | OK (x2) |
| VertexLayoutData | vertexlayoutdata.cpp:105-107 | Release `inputLayout` if set; no null store. The old `Dx11Release` was `if (p) ((IUnknown*)p)->Release()`, so this is identical. | OK |
| Vertex / PixelProgramData | shaderprograms.cpp:86-89 / 186-189 | Release `variants[0..3]` if set; no null stores. | OK (x2) |
| RenderStateData | renderstatedata.cpp:300-310 | Release blend, then depthStencil, then rasterizer (field order); no null stores. | OK |
| VideoProgramData / RenderBufferData | smallclasses.cpp:71-72 / 100-101 | Empty. | OK (x2) |
| Dx9PlaceholderDeviceData | dx9_placeholder.cpp:37-61; src/engine/video/enumerate_video_modes.cpp:141 | `final`, size 0x10, implicit virtual dtor. It is built with `new` and freed with `delete` through the base's class-scope operators (MemAlloc / MemFree). The old hand object used global new / delete in both directions, so allocation and free are still a matched pair. `CreateDevice() == false` means it is never the active device, so its inherited `_purecall` slots 4-19 cannot be reached (device_startup.cpp:445-466 deletes it). | OK |

**Item 2 verdict: 13 OK.**

## 3. Constructors write exactly the factory's stores

Each was compared with `Factory_XData` / `Dx11_CreateDeviceData` at `d14517b71`. All are user-provided constructors with no member initialisers and no `{}` value-init, so `new T(owner)` leaves the bytes it does not name untouched, as `MemAlloc` + stores did.

| class | b5-CLOSE | stores | verdict |
|---|---|---|---|
| DeviceData | devicedata.cpp:950-958 (exports.cpp:66-68 calls it) | The same 14 stores. `wasFullscreen` is a byte; `pad41` is not written; `cursorHotspotX / Y = -1`. | OK |
| ContextData | dx11/types/VideoDX11Data.h:248-253 | `topology = 0; pad14 = 0; d3dContext = nullptr`. | OK |
| ImageBufferData | VideoDX11Data.h:344-347 | Five `nullptr`s. | OK |
| ConstantBufferData | buffers.cpp:640-648 | `activeIndex = -1`, `pad14 = 0`, `buffers = nullptr`, `0`, `0`, then `Dx11TrackConstantBuffer(this)`, all in the factory's order. | OK |
| Index / VertexBufferData | VideoDX11Data.h:454-459 / 493-498 | `buffer = nullptr`, `writeCursor = 0` (dword), `locked = 0` (byte). `pad1d` is not written. | OK |
| VertexLayoutData | VideoDX11Data.h:563-564 | `inputLayout = nullptr`. | OK |
| Vertex / PixelProgramData | VideoDX11Data.h:604-607 / 638-641 | `variants[0..3] = nullptr`. | OK |
| RenderStateData | VideoDX11Data.h:681-683 | Three `nullptr`s. | OK |
| VideoProgramData / RenderBufferData | VideoDX11Data.h:726-727 / 764-765 | None (base ctor only). | OK |

All twelve `sizeof` asserts equal the retail `MemAlloc` size (VideoDX11Data.h:183-771).

**Item 3 verdict: 9 OK.**

## 4. Every `delete` site

There are 19 sites: the 18 listed in B5_RULES C1 (the `dynamic_resources.cpp` template had 4 users), plus `video_resources.cpp`'s Device dtor. Every one replaces `X->DeletingDtor(1)` at the same point with the same guard and the same null store (or the same absence of one).

| site(s) | guard kept | verdict |
|---|---|---|
| src/engine/video/buffers.cpp ImageBuffer / Index / Vertex / RenderBuffer `PlatformDestruct` | `if (… companion = field) delete companion;`. Index / Vertex null the field; ImageBuffer / RenderBuffer do not, as before. | OK |
| buffers.cpp `Video_ConstantBuffer_Dtor` | `if (companion)`. | OK |
| context_lifecycle.cpp (2), incl. `RtVideo_Context_Dtor` | `if (self->_contextData)`. | OK |
| device_startup.cpp:374 / :465 / :544 | Re-read then `if (dd)` (:374 / :544). The `d2` local in the failure path. | OK |
| video_resources.cpp `Video_Device_Dtor` | `if (dd) delete dd; deviceData = nullptr;`. | OK |
| pixelprogram / vertexprogram / vertexlayout / videoprogram / renderstate `PlatformDestruct` | `if (_data == nullptr) return;`. `_data` is not nulled, as before. | OK |
| enumerate_video_modes.cpp:141 (the DX9 placeholder and DX11 probes) | `if (data != nullptr)`. | OK |
| dynamic_resources.cpp: the four `ReleaseDynamic` walkers | The template is inlined as guard, `delete`, null the field: the template's three statements. | OK |
| engine/mainloop_video.cpp:545, save/saveload_update.cpp:1703 | `if (payload / data != nullptr)`, then the field nulled. | OK |
| `git grep DeletingDtor f57caa085 -- runtime dx11 standalone code tests` | 0 Video companion hits. The remaining hits are `RtInput_KeyMap_DeletingDtor`, `RtEngine_Application_ScalarDeletingDtor`, slot_check tests and READMEs. | OK |
| dynamic_resources.cpp:356, :403 comment | Says the template "stays as is for the classes still on DeletingDtor", but the template is deleted. | **FINDING F7** |

**Item 4 verdict: 10 OK, 1 FINDING.**

## 5. Signatures

| what | evidence | verdict |
|---|---|---|
| `Index/VertexBufferData::Serialize(IO::Serializer*)` (CompanionData.h:438 / :477) | `?Serialize@IndexBuffer@Video@@QEAAXAEAVSerializer@IO@@@Z` passes its reference through. Every caller passes `&ser` (graphicsmesh_vtable.cpp:376-379, edgemesh_vtable.cpp:123/126, batchc_gfx_leaves.cpp:481/484). | OK |
| `ImageBufferData::Create(const Collection::Array<Collection::Array<uint8_t>>*)` (CompanionData.h:371) | `ImageBuffer::Create` passes its `LevelArray* levels` through (src/engine/video/buffers.cpp:339). The mangled parameter is cited, and the dx11 reads (outer data / size, element +0x00) fit the layout. | OK |
| `RenderBufferData::Resolve(void*, RenderBuffer*, bool)` (CompanionData.h:560) | The mangling `?Resolve@RenderBuffer@Video@@QEBAXAEAVContext@2@PEAV12@_N@Z` is cited. The caller passes `_sourceTarget` (wave23_lane_gfxbody.cpp:616). | OK |
| `DeviceData::SetImageBuffer(ImageBuffer*, int, int)` (CompanionData.h:260) | The only caller, userinterface.cpp:692, passes a `Video::ImageBuffer*`. | OK |
| Typed `_owner` and `Create*Data(Video::X*)` | Each `_owner` is at +0x08 (asserted per class). The eleven factories take the owner type the runtime callers pass. | OK |
| `Device::onVideoError / onVideoRestart` (was `gap50[2]`) | Device.h:583-584, `void(*)()` at +0x50 / +0x58, asserted :938-939. The engine.cpp:484-487 static_asserts were rebased onto the names. singleton_ctors.cpp:360-361 writes two `nullptr`s, the same 16 bytes as the old memset. dx11 Finish reads both with the same null checks. | OK |
| Context.h +0x2100..+0x212f split | Was 4 × u64; now `i32 +0x2100, i32 +0x2104, void* +0x2108`, and the same at +0x2120 (Context.h:581-589, asserts :683-689). These match the old dx11 `kDyn*` constants exactly. The only other writer, context_ctor.cpp:346-353, zeroes the same 32 bytes. | OK |
| D8 accessor removal (`Field<T>` / `FieldPtr` / `Ctx::` / `Dev::`) | Every replacement member was checked against its old constant for offset and width (Context `_dirty` u32 0x10 … 0x215c; Device +4 / +8 / +0x9c / +0x100). All are asserted. | OK |
| Context.h's two `const void*` forwarders | Removed. They only `static_cast` into the typed members dx11 now calls, and they had no other callers. | OK |
| `RenderState::Stencil` renaming (RES side change) | Old `readMask +4 / writeMask +8 / func +c … backFunc +1c / reference +20` became `func +4 / reference +8 (u8) / failOp +c … backPassOp +20`. `Stencil::Serialize` (renderstate.cpp:164-177) serializes the same 8 offsets in the same order at the same widths. The ctor zeroes the same bytes. No other users. | OK |
| `IO::Encoding::IsBinary` by name (BUF) | Slot 0, declared first, no virtual dtor (Serializer.h:301-381). | OK |
| `Device::onVideoError` types match dx11's reads | `void(*)()`, the same as the old `Dev::kOnDeviceLost / kOnDeviceRestored` function-pointer reads. | OK |
| Device.h:580 comment | Says "The ctor zeroes both (singleton_ctors.cpp's memset)", but the memset is gone. | **FINDING F8** |

**Item 5 verdict: 12 OK, 1 FINDING.**

## 6. Adapter / GraphicsInterface (DEV-B)

| what | b5-CLOSE | verdict |
|---|---|---|
| Members against the old `RtVideo_*` doors | src/engine/video/device_enum_types.cpp:554-647. ctor: the same stores, `field34` untouched. Copy ctor: name share, zero + assign modes, zero + assign msaa, 3 dwords, same order. dtor: arrays first, then the name through the member dtor after the body (retail's order). `String()` / copy / `~String` match the old StringShare / StringRelease step for step (system/string.cpp:132-151, :308-318). `operator=` is deleted and asserted (Device.h:888-895). | OK |
| Element-destroy loops: `e->name.~String()` replaces StringReleaseInLoop | device_enum_types.cpp:369, :396. The extra JZ only matters when the base is 0 (name == 8), which faults on the refcount decrement before either spelling reaches the test; the reasoning is at :334-350. | OK |
| The 7 array helpers moved from device_startup.cpp | device_enum_types.cpp:143-332. All 88 code lines are verbatim. device_startup.cpp's only other code change is the 3 `delete` sites. | OK |
| Array growers | devicedata.cpp:1373-1453. Placement-new copy and explicit `~T()` at the old points. Same `MemAlloc(…, 8)` / `MemFree` and the same `if (!dst) continue`. | OK |
| `EnumerateDxgiInterfaceInto` locals | devicedata.cpp:1531-1725. `gi` is destroyed at both returns (:1548, :1724). `rec` is destroyed at the `break` (:1590) and at the end of each iteration (:1711). The only `continue` (:1643) is in the inner loop. No explicit dtor call is left, so there is no double release. The WideString locals end at the old points. | OK |
| Former door callers | `git grep RtVideo_(Adapter\|GraphicsInterface)_` = 0. optionsdialog.cpp:491 / :518 read `.name`. | OK |

**Item 6 verdict: 6 OK.**

## 7. Allocation and deletion correctness

| what | evidence | verdict |
|---|---|---|
| `operator new` / `operator delete` on all twelve bases, defined once | CompanionData.h has 12 of each declaration. src/engine/video/companion_operators.cpp defines each once (`MemAlloc(n, 4)` / `MemFree`, malloc / free under `BANISHED_UNIT_TEST`). 12 `has_virtual_destructor` asserts at CompanionData.h:758-780. | OK |
| The DX9 placeholder uses them | It derives from `Video::DeviceData`, so it inherits the class-scope operators. | OK |
| No companion `MemAlloc` / `MemFree(this)` left in dx11/ | Every remaining MemAlloc / MemFree is an array buffer or a scratch block (tracked images, CB buffers, subresource array, Serialize scratch, Adapter / GI arrays). | OK |
| Base-to-derived downcasts | Only `Dx11DeviceDataOf` (dx11_backend_access.h:113-116) and `Dx11ImageBackend` (:351-354) exist. Both reach objects built by dx11's own factories. The DX9 placeholder is never the active `deviceData` when a DX11 body runs, and it creates no ImageBuffer companions. | OK |
| D7 `Dx11SwapChain` | dx11_backend_access.cpp:20-22. standalone/main.cpp:873 / :944 do the same +0x18 read through the same singleton → deviceData path. | OK |
| "the ONE downcast" wording | dx11_backend_access.h:106 and :348 both say so. | **QUESTION Q1** |

**Item 7 verdict: 5 OK, 1 QUESTION.**

## 8. Residue

`git grep` at f57caa085 in `*.cpp *.h *.mk Makefile` under dx11 / standalone / runtime / code / tests, for `DX11_REGISTER_SLOT`, `Dx11VtableAddress`, `Dx11SeamInstall`, `Dx11LiveVtable`, `dx11_seam`, `AllocData`, `self->vptr`, `Dx11OriginalSlot`, and `B5 .* bridge`:

| hit | kind | verdict |
|---|---|---|
| Code (not comments) | 0 hits | OK |
| dx11/README.md:20 onward ("Everything below … is HISTORY"), dx11/src/README.md:9-10, devicedata_stubs.cpp:21, VideoDX11Data.h:779-780, renderstatedata.cpp:252 / :324 / :442, imagebufferdata.cpp:184, video_interface.cpp:111, buffers.cpp:137, contextdata.cpp:536 / :762, devicedata.cpp:299 | Historical: they say the thing is gone or what it was. | OK |
| dx11/src/exports.cpp:6-17 | Says it is false that a seam detour replaces the exports, and points at the deleted `dx11_seam.h`. | **FINDING F2** |
| dx11/src/dx11_backend_access.h:369-376 | States in the present tense that the slot-0 destructors call MemFree on `flags & 1` and their base dtors by qualified name. | **FINDING F1** |
| dx11_backend_access.h:379-381; devicedata.cpp:1871-1873 | "the seam cannot redirect it" / "the seam has no way to redirect it". | **FINDING F5** |
| standalone/video_interface.cpp:34 | "the same reason the DX11 seam is the vtables". | **FINDING F10** |
| src/engine/video/context.cpp:635-639; context_bind.cpp:222-225 | "that is the seam's business" (present tense). The slot indexes are said to be checked against "`DX11_REGISTER_SLOT` rows" that no longer exist. | **FINDING F11** |
| dx11/types/VideoDX11Data.h:45-53 | `struct DataBase { void* vptr; void* owner; }` and 3 asserts. Dead: 0 users in dx11 / tests. It describes a hand vptr that no longer exists. | **FINDING F3** |

**Item 8 verdict: 2 OK, 6 FINDING.**

## 9. Anything else

| what | verdict |
|---|---|
| imagebufferdata.cpp:429 / :459 say "DeviceData::colorBuffer / depthBuffer is still DEV's `void*` field", but both are `ID3D11Texture2D*` (VideoDX11Data.h:144-145). The `static_cast<ID3D11Texture2D*>` on :430 / :460 is now an identity cast. | **FINDING F4** |
| smallclasses.cpp:198-199 says "The base slot declares `other` as `void*`", but CompanionData.h:560 declares `RenderBuffer*` (B5 CLOSE). | **FINDING F6** |
| contextdata.cpp:530-534 says "reaches the same entries on the hand table today and the compiler's table once DeviceData is a real subclass". There is no hand table, and DeviceData is a subclass. | **FINDING F9** |
| Device.h:916-917 says "the object our ports and dx11 reach into by raw offset". dx11 no longer reads Device by raw offset (D8). Related but pre-existing: userinterface.cpp:678 says `deviceData` is `void*`; it was already `DeviceData*` at d14517b71. | **FINDING F12** |
| B5_RULES.md §7 requires `grep -rn "DeletingDtor\|…" code runtime standalone dx11 engine tests = 0`. That grep cannot reach 0: KeyMap's `RtInput_KeyMap_DeletingDtor`, `RtEngine_Application_ScalarDeletingDtor`, and slot_check / object_vtable tests are other roots, out of B5 scope. The Video companion part is 0. | **FINDING F13** (doc) |

**Item 9 verdict: 5 FINDING.**

---

## FINDINGS (ranked by severity; none changes behaviour)

1. **F1 (medium: a false statement that invites someone to restore the removed code).** dx11/src/dx11_backend_access.h:373-376. Current text: "The slot-0 destructors call `System::MemoryAllocator::MemFree` (…) when (flags & 1), and their base destructors by qualified name (RULE B5-3)." Correct text: "Since B5 the destructors are `~XData() override`: the compiler chains to the base dtor, and `delete` frees through the base's class-scope `operator delete` (MemFree, src/engine/video/companion_operators.cpp)."
2. **F2 (medium: points at a deleted file).** dx11/src/exports.cpp:6-17, the paragraphs "So they are replaced at the function entry instead, by a seam detour — see the 'Detours' section of dx11/funcs/dx11_seam.h" and "allocates the DeviceData whose vtable the rest of the seam then patches … the vtable patching is unaffected". Correct text: "Runtime reaches both through `Video::Device::dllSlots[1]`, which standalone/video_interface.cpp fills from inside the ported InitializeInterfaces. CreateDeviceData runs on every boot and allocates the `VideoDX11::DeviceData` subclass, which uses the compiler's vtable."
3. **F3 (low: dead code with a false layout claim).** dx11/types/VideoDX11Data.h:45-53. Delete `struct DataBase` and its three asserts. The per-class `offsetof(X, _owner) == 0x8` asserts already pin the `{compiler vptr, _owner}` = 0x10 prefix.
4. **F4 (low).** dx11/src/imagebufferdata.cpp:429 and :459. Correct text: "DeviceData::colorBuffer (depthBuffer) is the `ID3D11Texture2D*` at +0x38 (+0x30)." The `static_cast` on :430 / :460 is now an identity cast and can be dropped.
5. **F5 (low).** dx11/src/dx11_backend_access.h:379-381 and dx11/src/devicedata.cpp:1871-1873. Correct text: "0x180004340 is a near CALL from ContextData::BeginCommands, not a vtable slot, so it needs a declaration with external linkage."
6. **F6 (low).** dx11/src/smallclasses.cpp:198-199. Correct text: "`other` is typed `Video::RenderBuffer*` from retail's owner-side mangling (CompanionData.h, B5 CLOSE)."
7. **F7 (low).** src/engine/video/dynamic_resources.cpp:355-356 and :402-403. Drop the trailing "(the template stays as is for the classes still on DeletingDtor)". The template was deleted at :165-169.
8. **F8 (low).** src/engine/video/Device.h:580. Correct text: "The ctor zeroes both (singleton_ctors.cpp: two `nullptr` stores = retail's two qword stores)."
9. **F9 (low).** dx11/src/contextdata.cpp:533-534. Correct text: "…so the by-name call reaches retail slots 3 and 2 of the compiler's DeviceData table (B5 DEV-A: DeviceData is a real subclass)."
10. **F10 (low).** standalone/video_interface.cpp:34. Drop "which is the same reason the DX11 seam is the vtables and not Device+0xd0/+0xd8".
11. **F11 (low).** src/engine/video/context.cpp:635. Replace "that is the seam's business, not this file's" with "dx11 declares slot 17 as a forwarder to slot 19". Also, context.cpp:637-639 and context_bind.cpp:222-225: "checked … against the `DX11_REGISTER_SLOT` rows (B.3.5; since B5 the `override` declarations in dx11/types/VideoDX11Data.h)".
12. **F12 (low).** src/engine/video/Device.h:916-917. Correct text: "…so the object our ports and dx11 read by named member is still exactly retail's bytes." Also (pre-existing) src/engine/ui/userinterface.cpp:678: drop "`Video::Device::deviceData` (+0x100) is `void*`".
13. **F13 (low, doc).** docs/runtime_port/endgame_b/B5_RULES.md §7. Correct the grep to: "no `DeletingDtor` call on a `Video::*Data` / `VideoDX11::*` object, and `DX11_REGISTER_SLOT\|Dx11VtableAddress\|dx11_seam` = 0 outside historical comments." The literal grep cannot reach 0 because the KeyMap, Application and test roots are out of scope.

## QUESTIONS

- **Q1.** dx11_backend_access.h:106 ("the ONE downcast dx11 makes") and :348 ("the one downcast dx11 may make") each claim to be the only downcast, but there are two (`Dx11DeviceDataOf`, `Dx11ImageBackend`), and B5_RULES D7 names only the DeviceData one. Both are sound (dx11 built both objects). Should D7 be read as "downcasts of objects dx11 built", with both comments saying "one of the two"?
- **Q2.** The old dx11 `VideoRenderState` view read the RenderState enable flags as `uint8_t` and tested `!= 0`. Through the real class they are `bool` (typed in B.4 R-VIDEO, H4), so GCC may test only bit 0. These are `_alphaBlend.enabled`, `_depth.test / write`, `_stencil.enabled / twoSided` and `_flag70`. The result is identical whenever the byte is 0 or 1, which every writer guarantees (the ctor, `Serialize(bool&)`). Is relying on that acceptable, or should it be recorded in known_port_issues as a typing assumption?
