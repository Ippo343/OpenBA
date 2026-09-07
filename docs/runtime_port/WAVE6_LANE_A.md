# Wave 6 — Lane A — `Video::Context`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/video/context.cpp`, `runtime/types/video/Context.h`, this document.

**Result: 23 of 27 ported, 4 held back with written reasons.**
`RtVideoContextRegister()` returns **23**. `-fsyntax-only` with the wave's exact
command line is **warning-clean**. `tools/export_name_audit.py` is back to its
9 pre-existing phantoms with **none in this lane**.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a *prediction of
what the oracle will report*, not a measurement — see §7.

---

## 1. The function table

Retail addresses and ordinals were read out of the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with a pefile-style reader **before any
body was written** (`reference_resolveexport_phantom_names.md`). All 27 names
exist in the shipped DLL.

| Export | Retail | Ord | Ported | Oracle | Diff site name |
|---|---|---|---|---|---|
| `??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z` | 0x1800f1820 | 125 | **no** | — | — |
| `??1Context@Video@@QEAA@XZ` | 0x1800f16e0 | 562 | **no** | — | — |
| `??3Context@Video@@SAXPEAX@Z` | 0x180018500 | 945 | **no** | — | — |
| `?AddVertex@Context@Video@@QEAAXPEBX@Z` | 0x18001ad40 | 1592 | yes | replay | `Video::Context::AddVertex` |
| `?AddVertices@Context@Video@@QEAAXHPEBX@Z` | 0x18001ad70 | 1593 | yes | replay | `Video::Context::AddVertices` |
| `?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z` | 0x1800f2170 | 1644 | yes | thunk | `Video::Context::BeginIndexedVertices[slot]` |
| `?BeginInstancedIndexedVertices@Context@Video@@QEAA_NHHHH@Z` | 0x1800f2130 | 1645 | yes | thunk | `Video::Context::BeginInstancedIndexedVertices[slot]` |
| `?BeginVertices@Context@Video@@QEAA_NHH@Z` | 0x1800f21b0 | 1651 | yes | thunk | `Video::Context::BeginVertices[slot]` |
| `?Clear@Context@Video@@QEAAXIIMH@Z` | 0x1800f23f0 | 1705 | yes | thunk | `Video::Context::Clear[slot]` |
| `?ClearResources@Context@Video@@QEAAXXZ` | 0x1800f2090 | 1712 | **no** | — | — |
| `?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z` | 0x1800f2410 | 2278 | yes | thunk | `Video::Context::DrawIndexed[slot]` |
| `?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z` | 0x1800f2150 | 2312 | yes | thunk | `Video::Context::EndIndexedVertices[slot]` |
| `?EndInstancedIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z` | 0x1800f2110 | 2313 | yes | thunk | `Video::Context::EndInstancedIndexedVertices[slot]` |
| `?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z` | 0x1800f2190 | 2318 | yes | thunk | `Video::Context::EndVertices[slot]` |
| `?GetMaxDynamicInstances@Context@Video@@QEBAHHHH@Z` | 0x1800f21d0 | 2774 | yes | thunk | `Video::Context::GetMaxDynamicInstances[slot]` |
| `?Reset@Context@Video@@QEAAXXZ` | 0x1800f1e50 | 3573 | yes | replay | `Video::Context::Reset` |
| `?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z` | 0x18001a150 | 3837 | yes | replay | `Video::Context::SetCamera` |
| `?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z` | 0x18001a8e0 | 3846 | yes | replay | `Video::Context::SetColor` |
| `?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z` | 0x1800ef3c0 | 3866 | yes | replay | `Video::Context::SetDefaultRenderBuffer` |
| `?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z` | 0x18001a360 | 3884 | yes | replay | `Video::Context::SetFog` |
| `?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z` | 0x180019ed0 | 3893 | yes | replay | `Video::Context::SetImageBuffer` |
| `?SetInverseScale@Context@Video@@QEAAXMMMM@Z` | 0x18001a880 | 3898 | yes | replay | `Video::Context::SetInverseScale` |
| `?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z` | 0x1800f2100 | 3950 | yes | replay | `Video::Context::SetRenderBuffer` |
| `?SetTime@Context@Video@@QEAAXM@Z` | 0x18001a330 | 3988 | yes | replay | `Video::Context::SetTime` |
| `?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z` | 0x18001a6b0 | 3991 | yes | replay | `Video::Context::SetTransform` |
| `?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z` | 0x180019f20 | 4006 | yes | replay | `Video::Context::SetVertexBuffer` |
| `?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z` | 0x18001a060 | 4011 | yes | replay | `Video::Context::SetViewport` |

Every one of the 23 is registered **twice**, under its mangled name and under
its retail address, even though today all live call sites use the name form
(§5). The address registration costs nothing and closes the Wave-5-lane-D
failure mode where one live object is split between two implementations because
a later call site used the other door.

---

## 2. The four hold-backs

### `??0Context` (ctor), `??1Context` (dtor), `??3Context` (operator delete)

**Ownership of the object is entirely inside the DLL, and so is its memory.**
`Engine::MainLoop::CreateVideoContext` (0x1800dd500, ordinal 2139) does the
whole lifecycle:

```
pCtx = MemoryAllocator::Allocate(_singleton, 0x2168, 8);      // under the allocator's CS
Video::Context::Context(pCtx, Video::Device::_singleton, 0x800000, 0x400000, 0x100000, nullptr);
```

and the dtor is reached from `CreateVideoContext`, `ReleaseVideoContext`,
`~MainLoop` and `OnVideoError` — all `Engine::MainLoop`, which is Wave 8.

The ctor additionally allocates **five sub-objects** (a dynamic `VertexBuffer`,
a dynamic `IndexBuffer`, an instance `VertexBuffer`, a `RenderBuffer` and two
`ImageBuffer`s) through each class's own `operator new`, and runs each one's
`PlatformConstruct`; the dtor frees exactly those five and destroys the seven
embedded `ConstantBuffer`s. Porting either half means **our** code allocates
objects the **DLL's** other half frees. That is the split-ownership hazard
WAVE6.md names, and it is not mitigable by a diff oracle: there is nothing to
compare, only two heaps' worth of bookkeeping to get wrong.

`operator delete` is three lines and every one is out of scope: it takes
`MemoryAllocator::_singleton + 0x640` as a Win32 `CRITICAL_SECTION` and calls
`MemoryAllocator::Free`. `EnterCriticalSection` cannot appear in a non-`_win`
file (PLAN §4.1 decision 1), and `MemFree` is Wave 2's allocator port, which
shipped **opt-in with a NO-GO verdict** (`WAVE2_ALLOCATOR.md`). A `Video::Context`
lane has no business being the thing that finally arms it.

All three also have **zero live callers in our tree** — see the dead-resolve
finding in §6.

### `?ClearResources@`

Pure, self-contained, fully reversed, and **held back because it can never be
verified**. `fn_VideoCtx_ClearResources` (`engine/funcs/engine_funcs.cpp:1567`)
is resolved and never invoked, and `get_xrefs_to` on 0x1800f2090 shows no
internal DLL caller either. A registered port on a path nothing executes reports
`calls=0 divergences=0` forever, which *reads* as a pass —
`reference_uncalled_port_is_not_verified.md` is exactly this trap (a wrong
`Quaternion::operator*` survived all of Wave 1 that way). The reversed body, for
whoever lands it the day something calls it:

```
for i in 0..15:                                   # image slots, +0x30 + i*8
    if _imageBuffers[i]: _dirty |= 0x2; _imageBufferDirty |= 1<<i; _imageBuffers[i] = 0
for i in 0..15:                                   # vertex slots, +0xb8 + i*8
    if _vertexBuffers[i]:
        _dirty |= 0x8; _vertexBufferDirty |= 1<<i
        _vertexBuffers[i] = 0; _vertexOffsets[i] = 0; _vertexStrides[i] = 0
```

Note the asymmetry with `Reset`, which is not a transcription slip: `Reset`'s
vertex test also fires when only the offset or the stride is non-zero;
`ClearResources`' looks at the pointer alone.

---

## 3. Layout recovered — `runtime/types/video/Context.h`

`sizeof(Video::Context) == 0x2168`, alignment 8. **Proven, not inferred**: it is
the literal size argument in `MainLoop::CreateVideoContext`'s
`MemoryAllocator::Allocate(_singleton, 0x2168, 8)`. Every offset the lane uses
is `static_assert`ed.

**The class has no vtable.** The ctor's first store is `*(Device**)this = device`
and no Context method is virtual. This is the single most load-bearing fact in
the lane: PLAN §4.3 risk 3 (vtable ownership) simply does not arise, which is
what lets a partial port coexist with the DLL's own callers.

Field evidence — a one-line exported accessor is the strongest kind, because it
gives the offset *and* the field's real name:

| off | field | proven by | retail |
|---|---|---|---|
| 0x0000 | `_device` | ctor's first store | 0x1800f1820 |
| 0x0008 | `_frameIndex` | `?GetFrameIndex@` | 0x18001ae50 |
| 0x000c | `_shaderDetail` | `?GetShaderDetail@` | 0x18001ae30 |
| 0x0010 | `_dirty` | every setter ORs into it | |
| 0x0018 | `_imageBufferDirty` | `?SetImageBuffer@` (bit per slot) | 0x180019ed0 |
| 0x001c | `_vertexBufferDirty` | `?SetVertexBuffer@` (bit per slot) | 0x180019f20 |
| 0x0020 | `_transformCount` | `?SetTransform@` high-water mark | 0x18001a6b0 |
| 0x0028 | `_renderBuffer` | `?GetRenderBuffer@` | 0x180019ec0 |
| 0x0030 | `_imageBuffers[16]` | `?SetImageBuffer@` | 0x180019ed0 |
| 0x00b0 | `_vertexLayout` | `?SetVertexLayout@` | 0x180019f00 |
| 0x00b8 | `_vertexBuffers[16]` | `?SetVertexBuffer@` / `?GetVertexBuffer@` | 0x180019f80 |
| 0x0138 | `_vertexOffsets[16]` | `?GetVertexBufferOffset@` | 0x180019fa0 |
| 0x0178 | `_vertexStrides[16]` | `?GetVertexBufferStride@` | 0x180019f90 |
| 0x01b8 | `_indexBuffer` | `?SetIndexBuffer@` | 0x180019fb0 |
| 0x01c0 | `_indexOffset` | `?SetIndexBuffer@` 2nd arg | 0x180019fb0 |
| 0x01c8 | `_renderState` | `?SetRenderState@` | 0x180019fe0 |
| 0x01d0 | `_viewport` | `?GetViewport@` / `?SetViewport@` | 0x18001a0c0 |
| 0x01e0 | `_scissor` | `?SetScissor@` | 0x18001a0f0 |
| 0x01f0 | `_vertexProgram` | `?SetVertexProgram@` | 0x18001a000 |
| 0x01f8 | `_pixelProgram` | `?SetPixelProgram@` | 0x18001a020 |
| 0x0258 | fog colour, 2nd copy | `?SetFog@` | 0x18001a360 |
| 0x0298 | `_materialConstants` | `?GetMaterialConstantBuffer@` | 0x18001aef0 |
| 0x02e0 | `_pixelConstants` | `?GetPixelConstantBuffer@` | 0x18001af00 |
| 0x0328 | `_viewProjection` | `?SetCamera@` | 0x18001a150 |
| 0x0368 | `_inverseView` | `?SetCamera@` | 0x18001a150 |
| 0x03a8 | `_cameraPosition` | `?SetCamera@` (view row 3) | 0x18001a150 |
| 0x03c8 | `_time` | `?SetTime@` | 0x18001a330 |
| 0x03d8 | `_fogParams` | `?SetFog@` | 0x18001a360 |
| 0x03e8 | `_fogColor` | `?SetFog@` | 0x18001a360 |
| 0x03f8 | `_inverseScale` | `?SetInverseScale@` | 0x18001a880 |
| 0x0408 | `_color` | `?SetColor@` | 0x18001a8e0 |
| 0x10d8 | `_transforms[52]` | `?SetTransform@` (`this + i*0x40 + 0x10d8`) | 0x18001a6b0 |
| 0x1f78 | `_globalConstants` | `?GetGlobalConstantBuffer@` | 0x18001aea0 |
| 0x1fc0 | `_instanceConstants` | `?GetInstanceConstantBuffer@` | 0x18001aed0 |
| 0x2008 | `_boneConstants` | `?GetBoneConstantBuffer@` | 0x18001aec0 |
| 0x2050 | `_transformConstants` | `?GetTransformConstantBuffer@` | 0x18001aeb0 |
| 0x2098 | `_lightConstants` | `?GetLightConstantBuffer@` | 0x18001aee0 |
| 0x20e0 | `_dynamicVertexCursor` | `?AddVertex@` | 0x18001ad40 |
| 0x20e8 | `_dynamicVertexStride` | `?AddVertex@` | 0x18001ad40 |
| 0x20f8 | `_dynamicVertexBuffer` | ctor, `param2 > 0` branch | 0x1800f1820 |
| 0x2110 | `_instanceCount` | `dx11/src/contextdata.cpp` slots 16/18 | |
| 0x2118 | `_dynamicIndexBuffer` | ctor, `param3 > 0` branch | 0x1800f1820 |
| 0x2130 | `_dynamicInstanceBuffer` | ctor, `param4 > 0` branch | 0x1800f1820 |
| 0x2138 | `_defaultRenderBuffer` | `?GetDefaultRenderBuffer@` | 0x18001ae40 |
| 0x2140 / 0x2148 | default colour / depth `ImageBuffer` | ctor, `param5 == nullptr` branch | 0x1800f1820 |
| 0x2150..0x215c | triangles / vertices / draws / objects submitted | `?GetTrianglesSubmitted@` … `?GetObjectsSubmitted@` | 0x18001ae60..90 |
| 0x2160 | `_contextData` | every backend thunk's first load | |

`sizeof(Video::ConstantBuffer) == 0x48`, from the ctor's two identical
initialisation sequences at +0x298 and +0x2e0 and the same stride between the
five at +0x1f78 / +0x1fc0 / +0x2008 / +0x2050 / +0x2098. **Its interior is
deliberately opaque** in the header: nothing in lane A reads a field of one, and
inventing a layout we have not proven is how a shared struct corrupts silently
(PLAN §4.3 risk 1).

**Independent cross-check.** `dx11/src/contextdata.cpp` recovered +0x0028,
+0x01d0, +0x2110, +0x2138, +0x2150..+0x215c, +0x20e0, +0x20e8 and +0x2160 from
the *DX11* side before this lane existed. Every one agrees.

### The dirty mask (+0x0010)

Each bit named by the setter that sets it:

| bit | meaning | setter |
|---|---|---|
| 0x00000001 | render buffer | `?SetRenderBuffer@` 0x1800f2100 |
| 0x00000002 | image buffers | `?SetImageBuffer@` 0x180019ed0 |
| 0x00000004 | vertex layout | `?SetVertexLayout@` 0x180019f00 |
| 0x00000008 | vertex buffers | `?SetVertexBuffer@` 0x180019f20 |
| 0x00000010 | index buffer | `?SetIndexBuffer@` 0x180019fb0 |
| 0x00000020 | render state | `?SetRenderState@` 0x180019fe0 |
| 0x00000040 | vertex program | `?SetVertexProgram@` 0x18001a000 |
| 0x00000080 | pixel program | `?SetPixelProgram@` 0x18001a020 |
| 0x00000200 | viewport | `?SetViewport@` 0x18001a060 |
| 0x00000400 | scissor | `?SetScissor@` 0x18001a0f0 |
| 0x00000800 | global constants | `?SetCamera@`, `?SetTime@` |
| 0x00001000 | material constants | `?SetColor@`, `?SetInverseScale@` |
| 0x00004000 | transform constants | `?SetTransform@` |
| 0x00010000 | pixel constants | `?SetFog@` (which ORs 0x10800) |

`Reset` **assigns** 0x1fe00 (bits 9..16, the constant-buffer group) and then ORs
individual binding bits back in as it finds non-null bindings. 0x100, 0x2000 and
0x8000 fall inside that group but no lane-A function names them; they are not
guessed in the header.

### `Video::ContextData` vtable byte offsets

Nine methods are three-instruction tail jumps
(`MOV RCX,[RCX+0x2160] / MOV RAX,[RCX] / JMP [RAX+off]`):

`Clear` 0x20 · `DrawIndexed` 0x60 · `GetMaxDynamicInstances` 0x68 ·
`BeginVertices` 0x70 · `EndVertices` 0x78 · `BeginIndexedVertices` 0x80 ·
`EndIndexedVertices` 0x88 · `BeginInstancedIndexedVertices` 0x90 ·
`EndInstancedIndexedVertices` 0x98.

These match `dx11/src/contextdata.cpp`'s independently-recovered 20-slot table
exactly — and the port re-derives them from the shipped bytes at runtime rather
than trusting either source (§4b).

---

## 4. Two oracles

### (a) Replay — the fourteen state mutators

Snapshot the live 0x2168-byte Context into a private scratch copy **before** our
body runs; run our body on the real object; run the DLL's body on the copy;
`memcmp` all 0x2168 bytes. `RtDiffReportBytes` names the first differing offset,
and §3's table turns that offset straight into a field name.

This is a **full** oracle, not a spot check. It catches a wrong offset, a missed
dirty bit, a reassociated float, an early-out taken when retail would not have
taken it, and a stray write into padding — including into the ~5 KB of the
object this lane never models.

It is also what makes the two-implementations-on-one-object situation a
non-event: it asserts, on every diffed call, that our body leaves the object in
exactly the state the DLL's body would have.

One shared file-scope scratch buffer is used rather than an 8.5 KB stack frame
per function. Safe because rendering is single-threaded and no ported body calls
another ported body (`SetCamera` is the only one that calls out at all, into
`Math::Matrix`, which touches no Context). Written down in the source so that if
either premise changes, the next reader knows what to fix.

**One caveat, stated plainly.** For `AddVertex` / `AddVertices` the replayed
original *does* write outside the scratch copy: it memcpy's the same source
bytes to the same mapped vertex-buffer address, because the scratch Context's
cursor is the value ours started from. The second write is byte-for-byte
identical to the first, so it is idempotent rather than a second, different
write into live state. The alternative was no oracle at all for those two.

### (b) Thunk — the nine backend forwarders

Forwarders cannot be replayed; running them twice submits the draw twice. But a
forwarder has exactly one thing that can be wrong: **which vtable slot it jumps
to**, and getting it wrong calls a different backend method with this method's
arguments.

So at registration time the port decodes the shipped three-instruction body out
of memory — the `MOV RCX,[RCX+disp32]` displacement and the `JMP [reg+disp8/32]`
displacement, handling all the REX / mod / rm forms the nine actually use — and
reports through a normal diff site whether the DLL's `+0x2160` and `+slot` are
the ones the port uses. Each appears in `RtDiffSummary()` as one call.

This turns nine functions that would otherwise have been honest "no oracle"
rows into verified ones, and it verifies against the **shipped binary** rather
than against a table a human transcribed.

---

## 5. Registration form, and why name-only was still checked

`wave6_lanes.txt` marks all 27 rows `kind = export`. A grep across `code/`,
`engine/`, `standalone/`, `runtime/` and `dx11/` confirms every **ported**
method's live call sites resolve by mangled name:
`engine/funcs/engine_funcs.cpp`, `code/Game/Component/statusicon_geometry.cpp`,
`code/Game/Game/minimap_paint.cpp`, `code/Game/Game/gameupdateloop.cpp`,
`code/Game/Game/weather_sun.cpp`, `code/Game/Game/weather_particle_draw.cpp`,
`code/Game/Player/tool_base_tilerender.cpp`. The address form is registered
anyway, defensively.

The four exe PE-import slots that carry Context methods (0x1401525e0
`SetTransform`, 0x140152930 `BeginVertices`, 0x140152938 `EndVertices`,
0x140152948 `AddVertex`, per the comments in `tool_base_tilerender.cpp`) are
filled by `RegisterDispatchSlot(slot, ResolveExport(name))` in
`standalone/address_registry.cpp`, and **`AddressRegistryInit()` runs after
`RuntimePortInit()`** (`standalone/main.cpp:1614` then `:1626`), so those slots
pick up this port too. Checked, because a slot filled before the port registered
would have kept the DLL pointer and quietly halved the lane.

---

## 6. Stale comments this lane disproved

Lane A may only edit its own three files, so these are for the wave lead to
apply at their source (PLAN §6 step 8).

**(1) `docs/runtime_port/wave6_lanes.txt` — three lane-A rows have the wrong
`kind`.** It lists all 27 as `export`. Three are resolved by **address**:

- `engine/funcs/engine_funcs.cpp:1620` — ctor via `ResolveAddr(Module::Runtime, 0x1800f1820)`
- `engine/funcs/engine_funcs.cpp:1628` — `operator delete` via `ResolveAddr(Module::Runtime, 0x180018500)`
- `engine/funcs/engine_funcs.cpp:1622-1626` — dtor by name, falling back to `ResolveAddr(Module::Runtime, 0x1800f16e0)`

Harmless here only because all three are held back; a name-only port of any of
them would have registered something nobody calls, and the census would still
have counted it.

**(2) `engine/funcs/engine_funcs.h:50-51` and `:95-104` — four dead resolves.**
`fn_VideoCtx_Ctor`, `fn_VideoCtx_Dtor`, `fn_VideoCtx_Delete` and
`fn_VideoCtx_ClearResources` are assigned in `EngineFuncsInit` and the only
other mention of any of them anywhere in `code/`, `engine/`, `standalone/`,
`runtime/` or `dx11/` is the diagnostic printf at
`engine/funcs/engine_funcs.cpp:4123-4141`. **Nothing calls them.**
`fn_VideoCtx_Reset` is the only member of that group with a live caller
(`standalone/main.cpp:1321`).

Suggested correction, at the header:

```
// Video::Context lifecycle. RESOLVED, NEVER CALLED — the Context is created
// and destroyed entirely inside Runtime's Engine::MainLoop
// (CreateVideoContext 0x1800dd500 / ReleaseVideoContext / ~MainLoop). These
// four pointers exist only for the EngineFuncsDump() diagnostic; fn_VideoCtx_Reset,
// below, is the one member of this group with a live caller (standalone/main.cpp:1321).
```

The comment as written ("Video::Context lifecycle") is accurate about what they
*are* and silent about the fact that nothing drives them, which is what cost
this lane a grep and could cost a future lane a port.

**(3) `docs/runtime_port/WAVE6.md` scope line "Video::Context (27)".** Accurate
as a call-site count; worth annotating that four of the 27 have no live caller
at all (the three lifecycle functions and `ClearResources`), so the achievable
denominator for this lane is 23.

---

## 7. What this lane could NOT verify

Stated plainly, because a prediction is not a measurement:

1. **No run happened.** Per the lane brief, this lane did not build, deploy or
   run anything. `RtVideoContextRegister()` returning 23 and the oracle design
   are both source-level facts; **zero diffed calls have executed.** Until the
   wave lead runs `BANISHED_RT_DIFF=1`, every row in §1 is "expected clean", not
   "clean". The kill switch is `BANISHED_RT_VIDCONTEXT=0`.

2. **The thunk decoder has never decoded a live byte.** It was written against
   the nine bodies read out of Ghidra (`48 8b 89 60 21 00 00` + a 3-byte load +
   a `JMP [reg+disp]` in both disp8 and disp32 forms). If the loaded image
   differs from the on-disk image at those addresses — it should not, these are
   not relocated sites — the check reports `unrecognised thunk shape` rather
   than silently passing, which is the right failure direction but is still an
   untested path.

3. **`GetMaxDynamicInstances` is `const` in retail (`QEBA`) and the port takes a
   non-const `Context*`.** That is a source-level convenience only — the ABI is
   identical and the body does not write — but it is a deviation from the
   declared signature and worth knowing.

4. **`Math::Matrix::Inverse` is resolved through `ResolveExport`**, so `SetCamera`
   uses whichever implementation is registered. Today neither `Inverse`
   (`?Inverse@Matrix@Math@@QEBA?AV12@XZ`, 0x1800bb310, ord 3082) nor
   `operator!=` is ported, and Wave 1 *did* port `operator*`. If a later wave
   ports `Inverse` and gets it wrong, this lane's `SetCamera` diff is where it
   will surface — correctly, but it will look like a lane-A regression. The
   replay compares our `SetCamera` (which may call a ported `Inverse`) against
   the DLL's `SetCamera` (which always calls the DLL's `Inverse`).

5. **`Math::Matrix::operator!=` is inlined rather than called.** Retail calls the
   export at 0x18001d850. The inlined version reproduces the same sixteen `==`
   comparisons with the same short-circuit shape and the same NaN behaviour, and
   the replay oracle covers any disagreement — but it is a deviation from
   "call what retail called", taken to keep a resolve off the hottest setter in
   the class.

6. **Nothing here was measured for performance.** The replay oracle is inside
   `if (rt_diff_on__)` and costs nothing in a normal run; the forwarders add one
   indirect call versus retail's tail jump. The PLAN 5% gate is deferred
   (PLAN §5) and this lane did not touch it.

7. **The ~5 KB of the Context this lane does not model** (the staging blocks at
   +0x0200, +0x0418 and +0x1dd8, and the seven `ConstantBuffer` interiors) is
   *compared* byte-for-byte by the replay oracle but is not *named*. If a
   divergence ever lands inside one of those ranges, the reported offset will
   not resolve to a field name and the next reader will have to reverse the
   owning setter (`?SetLight@` 0x18001a950, `?SetBones@` 0x18001a780,
   `?SetShadowProjection@` 0x18001ac20, `?SetTextureProjection@` 0x18001ab80,
   `?SetMaterialParameter@` 0x18001acb0, `?SetUserPosition@` 0x18001a2e0).

---

## 8. Notable faithfulness details

Things a "tidied" port would silently drop, each preserved and commented in the
source:

- **`SetTransform` raises `_transformCount` outside the change test.** The
  high-water mark advances even when the matrix is identical.
- **`SetTime` and `SetFog` do not compare first.** They write and OR
  unconditionally, unlike every other setter in the class.
- **`SetFog` negates via a sign-bit XOR** (`XORPS XMM3,[0x1801617c0]`, a 16-byte
  `0x80000000` quad), not `0 - nearZ`; reproduced bitwise so a fog plane at
  `0.0f` yields `-0.0f`, as retail does. The other constant, `DAT_18016169c`,
  is `0x3f800000` = `1.0f`, so `_fogParams.y = 1.0f / (farZ - nearZ)`.
- **`SetFog` stores the colour twice**, at +0x3e8 and again at +0x258. Not a
  decompiler artefact — the second store is in the disassembly at 0x18001a3c4.
- **`SetCamera`'s multiply reads the field it just wrote.** It is
  `_viewProjection = _inverseView * projection`, with the *field* as the left
  operand, not the local `Inverse` result.
- **`Reset` assigns then ORs.** `_dirty` gets 0x1fe00 and then picks up binding
  bits only for bindings that were actually non-null, so a `Reset` on a clean
  Context leaves a different `_dirty` from a `Reset` on a dirty one.
- **`Reset`'s two block clears start at +0x208 and +0x248**, eight bytes past
  `_pixelProgram` and inside the pixel-constant staging block, and stop short of
  `_materialConstants` at +0x298 — which `Reset` does **not** clear. Written as
  raw offsets in the port for exactly that reason.
- **`Reset` re-reads the render buffer's width/height** between the viewport and
  scissor tests rather than reusing the values.
- **`Reset` dereferences `_defaultRenderBuffer` unguarded**, as retail does.
  CLAUDE.md's rule: no null check retail lacks.
- **`SetDefaultRenderBuffer` overwrites the pointer without freeing it.** The
  caller owns the buffer it hands in; do not add a delete.
- **`SetImageBuffer` / `SetVertexBuffer` mask the slot with `& 0x1f`** before
  shifting. With 16 slots it can never wrap, but the AND is in the shipped code.
- **`AddVertices` computes `count * stride` in 32-bit** and then widens, so an
  overflowing count wraps the same way in both implementations.

---

## 9. Ghidra

Everything in this lane was read with `program="Runtime-x64.dll"` on every call.
`switch_program` was never invoked. No function was renamed, so no other lane's
view of the shared project changed.
