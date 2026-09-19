# B.3 step 5 — `Video` and `Graphics` become real C++ classes: the rule sheet

Written 2026-09-19 by block 0 (the question pass) on main `bd483075`. This is the sheet six-plus
conversion agents are handed. It answers `NEXT_SESSION.md` §2b's five questions from the RETAIL
`Runtime-x64.dll` (15290 functions, image base 0x180000000) and from the tree, and it supersedes
`VIDEO_GRAPHICS_INVENTORY.md` wherever the two disagree — the inventory is a map, this is the
evidence. §13 lists what is still NOT VERIFIED; nothing outside §13 was taken on trust.

Standing rules from `B34B_RULES.md` §2d (class shape, name-lookup traps, the eight — now NINE,
see §8.1 — caller spellings, process) apply unchanged and are not repeated. `ENDGAME.md` §B.1/§B.2
decisions apply unchanged: retail layout kept, slots in retail order, **no virtual destructor**
(slot 0 is `DeletingDtor(unsigned)`), no real ctor on a polymorphic class, doors delegate with a
QUALIFIED non-virtual call.

## 0. How this sheet was measured

| fact | instrument |
|---|---|
| vtable addresses, slot count, each slot's target + retail name | `read_memory` over `.rdata` via a Ghidra Java script, one qword at a time, terminating on the next `??_7` symbol / on non-code / on a rodata string. NOT decompiled C. |
| `sizeof` of every polymorphic class | the `MOV EDX,0x??` immediate in its own slot-0 vector deleting destructor (the arg MSVC hands `??_ehvec_dtor`), read as DISASSEMBLY |
| base class | `?_ParentTypeName@X@Ns@@CAPEBDXZ`'s returned string literal, read byte-wise (§2.1), CROSS-CHECKED against shared slot targets and against `sizeof` |
| who stamps a vtable | Ghidra `getReferencesTo` on the vtable address, resolved to the containing function's mangled name |
| by-value returns | every Video/Graphics export whose mangled return type is `?AV`/`?AU` (class by value → hidden retslot); `?AW4` (enum, returned in EAX) excluded |
| our tree | `Grep`/`Glob` scoped to the worktree, build dirs excluded |

Counts below say TOKENS or CALL SITES explicitly. Where a number came from a subagent's regex
rather than from bytes it is labelled.

**The raw data is `B35_SLOTS.md`** — every slot of all 39 vtables and the five parents with full
mangled names and `inherited-from` markers, all 36 `_TypeName`/`_ParentTypeName` literals, the 60
hidden-retslot exports, and every exported member of the fourteen non-polymorphic classes. Copy
from it rather than from the tables here; the scripts that produced it are in
`b35_session/ghidra/`.

---

## 1. Q1 — THE dx11 BOUNDARY

### 1.1 Verdict

**The abstract/concrete split is REAL, it is exactly the shape step 5 should model, and it is
IN SCOPE — but only the Runtime half of it.**

Confirmed byte-wise: for all twelve `Video::*Data` classes, Runtime-x64.dll contains an
**abstract base** whose vtable has a concrete slot 0 (a deleting dtor in Runtime's
`0x180058590..0x180058b10` band) and `_purecall` in **every other slot**, and whose object is
exactly **0x10 bytes** (`vtable`, `owner`). VideoDX11 contains the **concrete** override set.
Two independent sources agree on the slot count of all twelve:

| class | Runtime abstract vtable | slots (bytes) | dx11 concrete vtable (`dx11/funcs/dx11_seam.h`) | agree |
|---|---|---|---|---|
| `Video::DeviceData` | 0x180158c30 | 20 | `Vt::DeviceData`, "20 slots" | YES |
| `Video::ContextData` | 0x180158b90 | 20 | `Vt::ContextData`, "20 slots" | YES |
| `Video::ImageBufferData` | 0x180158b50 | 7 | `Vt::ImageBufferData`, "7 slots" | YES |
| `Video::IndexBufferData` | 0x180158af8 | 7 | 7 | YES |
| `Video::VertexBufferData` | 0x180158ac0 | 7 | 7 | YES |
| `Video::ConstantBufferData` | 0x180158b30 | 4 | 4 | YES |
| `Video::RenderBufferData` | 0x180158a28 | 4 | 4 | YES |
| `Video::VertexLayoutData` | 0x180158aa8 | 3 | 3 | YES |
| `Video::VertexProgramData` | 0x180158a90 | 3 | 3 | YES |
| `Video::PixelProgramData` | 0x180158a78 | 3 | 3 | YES |
| `Video::RenderStateData` | 0x180158a60 | 3 | 3 | YES |
| `Video::VideoProgramData` | 0x180158a48 | 3 | 3 | YES |

12/12. That is not a coincidence of two guesses; the dx11 numbers were read from the shipped
plugin's `.rdata` in an earlier wave and the Runtime numbers were read from bytes this pass.

**RULE B5-1 (the boundary).**
1. `runtime/types/video/` owns ONE real abstract class per `*Data`: slot 0
   `virtual XData* DeletingDtor(unsigned)`, every other slot `= 0` in retail order, two data
   members (`vtable` is the implicit vptr, `owner` is the `X*`), `static_assert(sizeof == 0x10)`
   **is wrong to write** — 0x10 is a LOWER bound (`CompanionData.h` already says so and is
   right); assert the two offsets, not the size.
2. `dx11/` owns ONE concrete subclass per `*Data`, in `dx11/types/`. It keeps its own
   `sizeof` asserts (e.g. `DeviceData` is **0x70**, from `MemAlloc(0x70, 4)` in
   `Dx11_CreateDeviceData`, `dx11/src/exports.cpp`).
3. **The vtables belong to dx11**, not Runtime: `Dx11SeamInstall()` assembles all twelve in the
   exe from `DX11_REGISTER_SLOT` static registrations and refuses a hole. Runtime's twelve
   `??_7XData@Video@@6B@` addresses are minted only so the *abstract base's* ctor/dtor can stamp
   them (`runtime/src/video/companion_data.cpp` — one file, all twelve).
4. **The concrete dx11 side is a DECLARED LEAVE for step 5** (see §11). Converting 84 seam slots
   into twelve C++ classes changes how `Dx11SeamInstall`'s completeness check works, and that
   check is the only thing standing between us and a null vtable slot. Step 5 converts the
   Runtime half and the *callers*; the seam stays. Say so in every dx11-touching commit.

### 1.2 dx11 MAY include `runtime/types/video/*.h` — it already includes four runtime headers

There is no `DX11_FLAGS` variable in the Makefile (the inventory and the brief both imply one;
they are wrong). `dx11/funcs/*.cpp` and `dx11/src/*.cpp` are folded into `SA_SRCS` (Makefile:41-42,
76-77) and compile with the single shared include set at Makefile:68, which contains
`-Iruntime/types -Iruntime/funcs`. So **no Makefile change is needed**, and the precedent is
already set four times:

```
dx11/src/dx11_backend_access.h:29  #include "collection/Collection.h"   // Collection::Array<T>
dx11/src/devicedata.cpp:125        #include "system/String.h"
dx11/src/devicedata.cpp:126        #include "system/Error.h"       // ENDGAME B.3.4b
dx11/src/devicedata.cpp:127        #include "system/WideString.h"  // ENDGAME B.3.4b
dx11/src/contextdata.cpp:146       #include "system/Error.h"       // ENDGAME B.3.4b
```

`Collection.h` is the exact precedent that matters: B.3.2 step 2 deleted dx11's private `RtArray`
POD and replaced it with the real `Collection::Array<T>` by including the Runtime header. Do the
same here.

### 1.3 The duplicate-POD hazard — five views, ONE of them wrong, ONE with no twin at all

`dx11/src/dx11_backend_access.h` keeps "partial, offset-pinned views of the *Runtime-side* owner
objects". Every one is a duplicate POD in `B34_RULES.md` §1.5.2's sense. Measured:

| dx11 view (`dx11_backend_access.h`) | what it pins | Runtime twin | verdict |
|---|---|---|---|
| `RtByteCodeBlob` / `RtByteCodeList` (l.134) | — | **is** `Collection::Array<T>` | NOT a duplicate. Already fixed in B.3.2. |
| `ShaderProgramOwner` (l.161) `opaque00[0x40]` + `byteCode`@0x40, count@0x48 | +0x40, +0x48 | `rt::video::ShaderProgram` (`ShaderProgram.h:86`) `_byteCode`@0x40, sizeof 0x50 | **AGREE.** Delete the view, include the header. |
| `ImageBufferOwner` (l.300) `opaque00[0x60]` + `data`@0x60 | +0x60 | `RtVideo::ImageBuffer` (`ImageBuffer.h:57`) `platformData`@0x60, sizeof 0x68 | **AGREE.** Delete the view, include the header. |
| `RenderBufferOwner` (l.280) `color`@0, `depth`@8, `static_assert(sizeof == 0x10)` | +0x00, +0x08 | `RtVideo::RenderBuffer` (`Device.h:302`) `colorBuffer`@0, `depthBuffer`@8, **sizeof 0x30**; and a THIRD view `RtVideo::RenderBufferPrefix` (`Context.h:191`) pinning width@0x10/height@0x14 | **FIELDS AGREE, THE SIZE ASSERT IS FALSE.** `sizeof(RenderBufferOwner)==0x10` asserts that a 0x30-byte class is 0x10. It cannot fail (it describes only the view) and it is the exact shape that hides a layout error. **Three views of one class across three files.** |
| `VertexLayoutOwner` (l.200) + `VertexLayoutElement` (l.230) | elements@0x20/count@0x28, signature@0x38/len@0x40; Element stride 0x20, stream@8, byteOffset@0xc, format@0x18 | **NONE.** There is no `struct VertexLayout` anywhere in `runtime/types/`. | **NO TWIN.** dx11 is today the ONLY home of `Video::VertexLayout`'s layout, and it is a partial one. |
| `kVideoDevice_ActiveDeviceData = 0x100` (l.68) | Device+0x100 | `RtVideo::Device::deviceData`@0x100 (`Device.h:204`, whose comment already says "dx11/ reads this at exactly this offset") | **AGREE.** |
| `kRt_VideoDeviceSingleton = 0x1801fe7a8` (l.71) | — | `?_singleton@Device@Video@@0PEAV12@EA` @ **0x1801fe7a8** (`cutover_manifest.inc:129`) | **AGREE.** |

**RULE B5-2 (collapse the duplicates, in this order).**
1. In the commit that makes `Video::ImageBuffer` and `Video::ShaderProgram` real classes, DELETE
   `ImageBufferOwner` and `ShaderProgramOwner` from `dx11_backend_access.h` and include the
   Runtime headers. `Dx11ImageTexture/Rtv/Dsv` keep their signatures and lose their casts.
2. `Video::VertexLayout` gets a REAL `runtime/types/video/VertexLayout.h` **before** any block
   touches it: 0x48 bytes (§2.2), `Object::CompiledExternal` base (§2.1), with the four offsets
   `VertexLayoutOwner` pins folded in AND `VertexLayout::Element` named (retail exports
   `?GetElement@VertexLayout@Video@@QEBAAEBUElement@12@H@Z`, so `Element` is a `struct`, `@U`).
   Then delete both dx11 views. This is the one place step 5 must AUTHOR a header, not port one.
3. **Delete `static_assert(sizeof(RenderBufferOwner) == 0x10)` on sight.** A size assert on a
   prefix view is a false witness. Collapse `RenderBufferOwner` and `Context.h`'s
   `RenderBufferPrefix` into `RtVideo::RenderBuffer` (Device.h) in the `RenderBuffer` commit;
   that removes two of the three views and leaves the one with the true 0x30.

### 1.4 Who constructs what, and who stamps what

- **The twelve abstract bases' ctors/dtors are Runtime's**, all in
  `runtime/src/video/companion_data.cpp` (24 bodies; the per-class `??_7` LEA operand is tabulated
  in its banner and every one matches the address in §1.1's table).
- **The derived `*Data` objects are allocated and constructed by dx11**, which calls the Runtime
  base ctor by mangled name and then overwrites the vptr with its own. `Dx11_CreateDeviceData`
  (`dx11/src/exports.cpp`) is the readable example; the other eleven are created through
  `DeviceData`'s own `Create*Data` slots.
- dx11 stamps only `Dx11LiveVtable(Vt)`. It NEVER stamps a `??_7...@Video@@6B@` address: the
  comment at `dx11_backend_access.h:373-379` says explicitly not to reintroduce that.
- **Nothing in `dx11/` constructs a Runtime-side object.** It reads them (`ImageBuffer`,
  `ShaderProgram`, `VertexLayout`, `RenderBuffer`, `Device`, `Configuration`) and calls their
  members by name.

**RULE B5-3.** A dx11 destructor's `Dx11VtableAddress(v)` re-seat and its call to the Runtime base
dtor are the MSVC derived-dtor idiom. When the Runtime base becomes a real class, the base-dtor
call becomes `self->Video::XData::~XData()` — a **qualified non-virtual** call, per §B.1 decision 2
and `B34B_RULES` §2d. It must NOT become `DeletingDtor(0)`: that re-enters the derived slot 0.

---

## 2. Q2 — PER CLASS, FROM RETAIL BYTES

### 2.1 The hierarchy, proved

Every edge below is proved by `?_ParentTypeName@` (18 classes carry one; the string was read
byte-wise) or, where no `_ParentTypeName` exists, by **shared slot targets plus `sizeof`**. The
`_typeInfo` column is `?_typeInfo@X@Ns@@` present in the export table.

```
Object::Base            (B.3.3 — runtime/types/object/TypeInfo.h:309)
└─ Object::External     (B.3.3 — runtime/types/object/External.h:601, slots 0..7)
   ├─ Video::RenderState                 _ParentTypeName "Object::External"
   ├─ Video::VideoProgram                _ParentTypeName "Object::External"
   ├─ Graphics::Material                 _ParentTypeName "Object::External"
   ├─ Graphics::MaterialInstance         _ParentTypeName "Object::External"
   ├─ Graphics::AnimationEvent           _ParentTypeName "Object::External"   (ABSTRACT)
   ├─ Graphics::AnimationGroup           _ParentTypeName "Object::External"
   ├─ Graphics::EdgeMesh                 _ParentTypeName "Object::External"
   ├─ Graphics::Particle                 _ParentTypeName "Object::External"   (ABSTRACT)
   │  └─ Graphics::ParticleSystem        _ParentTypeName "Particle"
   └─ Object::CompiledExternal (B.3.3 — External.h:692, slots 0..7)
      ├─ Graphics::GraphicsMesh          _ParentTypeName "Object::CompiledExternal"
      ├─ Video::VertexLayout             _ParentTypeName "Object::CompiledExternal"
      ├─ Video::ShaderProgram            _ParentTypeName "Object::CompiledExternal" (ABSTRACT)
      │  ├─ Video::VertexProgram         _ParentTypeName "ShaderProgram"
      │  └─ Video::PixelProgram          _ParentTypeName "ShaderProgram"
      └─ Video::ImageBuffer              _ParentTypeName "Object::CompiledExternal"
         └─ Graphics::GeneratedImageBuffer  _ParentTypeName "Video::ImageBuffer"  (ABSTRACT)
            ├─ Graphics::FrameCopyImageBuffer   _ParentTypeName "GeneratedImageBuffer"
            └─ Graphics::ReflectionImageBuffer  _ParentTypeName "GeneratedImageBuffer"

Graphics::Geometry   — A ROOT. Not an Object::Base subclass: its slot 0 (0x180015e90) and slots
│                      1/2 are its own, and share NOTHING with ??_7Base@Object@@6B@ (0x180154788,
│                      whose slot 2 is ?Type@Base@Object@@). No _typeInfo, no TypeName/Type.
│                      ABSTRACT (slots 1,2,3 = _purecall). 0xC0.
├─ Graphics::MeshInstance            slots 4,8 identical to Geometry's; Mesh.h:172 `Geometry _base`@+0
├─ Graphics::EdgeMeshInstance        slots 4,5,7,8 identical to Geometry's; Mesh.h:384 same
├─ Graphics::ParticleInstance        slots 4,5,7,8 identical to Geometry's (ABSTRACT, slot 1 pure)
│  └─ Graphics::ParticleSystemInstance  slots 2,3,6 identical to ParticleInstance's
└─ Graphics::Light                   slots 4,5,6,7,8 ALL identical to Geometry's AND sizeof ==
   │                                 Geometry's 0xC0 (Light adds no fields — SunLight.h:62 says
   │                                 the same independently). ABSTRACT (1,2,3,9,10 pure).
   ├─ Graphics::AmbientLight         0xd8
   ├─ Graphics::SpotLight            0x168
   └─ Graphics::SunLight             0x108

Video::{Device,Context,Configuration,ConstantBuffer,IndexBuffer,VertexBuffer,RenderBuffer,Viewport}
Graphics::{Camera,Scene,Renderer,Animation,AnimationManager,AnimationState}
   — NON-POLYMORPHIC. No ??_7 export, no _typeInfo. Roots.

Video::{DeviceData,ContextData,ImageBufferData,ConstantBufferData,IndexBufferData,
        VertexBufferData,VertexLayoutData,VertexProgramData,PixelProgramData,
        RenderStateData,VideoProgramData,RenderBufferData}
   — twelve independent ABSTRACT roots, 0x10 each, no _typeInfo. See §1.
```

**Findings that contradict `VIDEO_GRAPHICS_INVENTORY.md`:**
- §2a says the Video program family are "8-9 slots, all `External` subclasses — mechanical after
  one". **Three different parents in that family**: `ShaderProgram`/`VertexLayout` are
  `CompiledExternal`, `VideoProgram` is direct `External`, `VertexProgram`/`PixelProgram` are
  `ShaderProgram`. Converting `VideoProgram` on `ShaderProgram`'s pattern gives it a
  `CompiledExternal` vtable stamp it must not have. Exactly the mistake `RenderState.h`'s banner
  was written to prevent for `RenderState`.
- §2a says "the Light siblings … have UNCONFIRMED bases". Confirmed here: `Light : Geometry`,
  adding **no fields**, and `Ambient/Spot/Sun : Light`.
- §2a says the `*Data` family has unconfirmed bases. Confirmed: twelve independent roots.

### 2.2 The 39 minted-vtable classes — slot count, `sizeof`, verdict

`sizeof` is the `MOV EDX,imm` in the class's own slot-0 vector deleting dtor. "hdr" is the
existing type header, or `—` if the layout is undocumented (§2.4). Verdicts per `B34B_RULES` §2d
and B4b-2/3.

| class | vtable | slots | sizeof | hdr asserts it | verdict |
|---|---|---|---|---|---|
| `Video::ImageBuffer` | 0x180154fc0 | 10 | 0x68 | ImageBuffer.h ✓ | concrete, no real ctor |
| `Video::RenderState` | 0x180157720 | 9 | 0x80 | RenderState.h ✓ | concrete, no real ctor |
| `Video::ShaderProgram` | 0x180157b60 | 8 | 0x50 | ShaderProgram.h ✓ | **ABSTRACT** (3,6,7 pure) |
| `Video::VertexProgram` | 0x180157b10 | 9 | 0x58 | ShaderProgram.h ✓ | concrete, no real ctor |
| `Video::PixelProgram` | 0x180157ac0 | 9 | 0x58 | ShaderProgram.h ✓ | concrete, no real ctor |
| `Video::VideoProgram` | 0x180157a70 | 9 | 0x48 | — | concrete, no real ctor |
| `Video::VertexLayout` | 0x180157ba0 | 9 | 0x48 | — (dx11 partial only) | concrete, no real ctor |
| `Video::DeviceData` | 0x180158c30 | 20 | 0x10 | CompanionData.h (shared) | **ABSTRACT** (1..19 pure) |
| `Video::ContextData` | 0x180158b90 | 20 | 0x10 | CompanionData.h | **ABSTRACT** (1..19 pure) |
| `Video::ImageBufferData` | 0x180158b50 | 7 | 0x10 | ImageBuffer.h ✓ 0x10 | **ABSTRACT** |
| `Video::IndexBufferData` | 0x180158af8 | 7 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::VertexBufferData` | 0x180158ac0 | 7 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::ConstantBufferData` | 0x180158b30 | 4 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::RenderBufferData` | 0x180158a28 | 4 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::VertexLayoutData` | 0x180158aa8 | 3 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::VertexProgramData` | 0x180158a90 | 3 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::PixelProgramData` | 0x180158a78 | 3 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::RenderStateData` | 0x180158a60 | 3 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Video::VideoProgramData` | 0x180158a48 | 3 | 0x10 | CompanionData.h | **ABSTRACT** |
| `Graphics::Geometry` | 0x180154e60 | 9 | 0xC0 | Geometry.h ✓ | **ABSTRACT** (1,2,3 pure); **held BY VALUE** — see §3 |
| `Graphics::MeshInstance` | 0x180154dc0 | 9 | 0x120 | Mesh.h ✓ | concrete, no real ctor |
| `Graphics::EdgeMeshInstance` | 0x180154d70 | 9 | 0xd0 | Mesh.h ✓ | concrete, no real ctor |
| `Graphics::EdgeMesh` | 0x180154c70 | 9 | 0x180 | Mesh.h ✓ | concrete, no real ctor |
| `Graphics::GraphicsMesh` | 0x180154cc0 | 9 | 0x98 | — | concrete, no real ctor |
| `Graphics::Material` | 0x180154d10 | 9 | 0x38 | — | concrete, no real ctor |
| `Graphics::MaterialInstance` | 0x1801545e0 | 9 | 0x60 | Material.h ✓ | concrete, no real ctor |
| `Graphics::AnimationEvent` | 0x180154bf0 | 9 | 0x20 | Animation.h ✓ | **ABSTRACT** (3,5,6,7,8 pure) |
| `Graphics::AnimationGroup` | 0x180154e10 | 9 | 0x58 | — | concrete, no real ctor |
| `Graphics::Particle` | 0x180155130 | 9 | 0x48 | Particle.h ✓ | **ABSTRACT** (6,7,8 pure) |
| `Graphics::ParticleSystem` | 0x180155090 | 10 | 0x108 | — | concrete; **slot order trap, §2.3** |
| `Graphics::ParticleInstance` | 0x1801550e0 | 10 | 0xd8 | Particle.h ✓ | **ABSTRACT** (slot 1 pure) |
| `Graphics::ParticleSystemInstance` | 0x180155040 | 10 | 0xe0 | Particle.h ✓ | concrete, no real ctor |
| `Graphics::GeneratedImageBuffer` | 0x180154f60 | 12 | 0x70 | GeneratedImageBuffer.h ✓ | **ABSTRACT** (10,11 pure) |
| `Graphics::FrameCopyImageBuffer` | 0x180154f00 | 12 | 0x88 | FrameCopyImageBuffer.h ✓ | concrete, no real ctor |
| `Graphics::ReflectionImageBuffer` | 0x180155190 | 12 | 0x90 | GeneratedImageBuffer.h ✓ | concrete, no real ctor |
| `Graphics::Light` | 0x180158d30 | 11 | 0xC0 | — (SunLight.h notes it) | **ABSTRACT** (1,2,3,9,10 pure) |
| `Graphics::AmbientLight` | 0x180158cd0 | 11 | 0xd8 | — | concrete, no real ctor |
| `Graphics::SpotLight` | 0x180158db0 | 11 | 0x168 | — | concrete, no real ctor |
| `Graphics::SunLight` | 0x180158e10 | 11 | 0x108 | SunLight.h ✓ | concrete, no real ctor |

Every `sizeof` in the "hdr asserts it" column matched the dtor immediate exactly — 21 independent
agreements, zero disagreements. That is the strongest single result of this pass: the existing
headers are trustworthy on size.

### 2.3 Slot-order traps read from bytes

1. **`Construct` is NOT inherited.** `Object::External`'s real class (`External.h:601`) declares
   slots 0..7 only; `virtual External* Construct()` appears at `External.h:725`, on
   `ExternalList`. Retail agrees: `??_7External@Object@@6B@` and
   `??_7CompiledExternal@Object@@6B@` are 8 entries each. Every Video/Graphics external
   introduces `Construct` itself at slot 8, and the `ImageBuffer` / `ShaderProgram` /
   `Particle` chains then override it. So: `virtual`, not `override`, on the first class of each
   branch.
2. **`Graphics::ParticleSystem` is the one class where `Construct` is NOT slot 8.**
   Read from bytes: `[8] 0x1800a5140 ?CreateSystem@ParticleSystem@Graphics@@UEBAPEAVParticleInstance@2@XZ`,
   `[9] 0x180020580 ?Construct@ParticleSystem@Graphics@@UEAAPEAVExternal@Object@@XZ`.
   `Particle` (its base) introduces `CreateSystem` as PURE at slot 8, and `Construct` enters the
   branch only at `ParticleSystem`, slot 9. **Copying "Construct is slot 8" onto ParticleSystem
   swaps two slots and produces a `CreateSystem` that returns an `External*`.**
3. **`Graphics::Light` adds no fields but adds two slots.** `sizeof(Light) == sizeof(Geometry) ==
   0xC0`, yet Light's vtable is 11 entries against Geometry's 9: it introduces
   `DrawShadowMap` (9) and the four-int `Draw` (10) as PURE. `AmbientLight`/`SpotLight`/`SunLight`
   override both. A `Light` class with a data member would break all three subclasses' offsets.
4. **Two `Draw` overloads on the Light branch, at slots 2 and 10.** Slot 2 is
   `Draw(Context&, const Camera&, ExternalPtr<MaterialInstance>, float, int, int, int)`
   (inherited shape from Geometry); slot 10 is `Draw(Context&, Renderer&, int, int, int, int)`.
   `ENDGAME.md` §B.1.2 says give overloaded virtuals distinct names — **do it here**, because MSVC
   grouped-and-reversed overloads are exactly what §B.1.2 is dodging, and these two are 8 slots
   apart so a mistake is silent.
5. `Video::ImageBuffer` slot 9 is `Create`; `GeneratedImageBuffer` adds `Generate`(10) and
   `Render`(11) as PURE. `FrameCopy`/`Reflection` override 6,7,8,10,11 but **inherit slot 9**
   (`?Create@ImageBuffer@Video@@`) — `GeneratedImageBuffer` also inherits 6,7,8 from `ImageBuffer`.
6. `Graphics::EdgeMeshInstance` overrides slot 6 only of the two retslot slots; slot 5
   (`GetSubTransform`) stays `Geometry`'s. `MeshInstance` overrides both.

### 2.4 The NON-polymorphic classes

`sizeof` here is NOT from a deleting dtor (there is none); it is whatever the existing header
asserts, with the header's own cited evidence. Where a header has no size assert, the size is
marked INFERRED and goes on the §13 list.

| class | sizeof | header | real ctor/dtor allowed? |
|---|---|---|---|
| `Video::Context` | 0x2168 | Context.h:387 ✓ | NO — `??0Context` takes `Device*` / `(Device*,int,int,int,RenderBuffer*)`; there is no default ctor. Holds four types BY VALUE (§3). |
| `Video::Device` | 0x108 | Device.h:206 ✓ | NO — no `??0Device@Video@@QEAA@XZ` exists; only `(DeviceType)` and the copy ctor. Owns a `CRITICAL_SECTION` at +0x60 (§5). |
| `Video::Configuration` | **0x44 INFERRED** | Device.h:245, offsets only | see §13 |
| `Video::RenderBuffer` | 0x30 | Device.h:311 ✓ | NO — ctor/dtor maintain the `_firstRenderBuffer` intrusive list |
| `Video::IndexBuffer` | 0x20 | Buffers.h:42 ✓ | NO — `_firstBuffer` intrusive list |
| `Video::VertexBuffer` | 0x20 | Buffers.h:60 ✓ | NO — `_firstBuffer` intrusive list |
| `Video::ConstantBuffer` | 0x48 | Context.h:203 as `ConstantBufferOpaque` ✓; CompanionData.h:70 partial, no size | NO — ctor calls `Device` vtable +0x58. Held BY VALUE ×7 in `Context` (§3). |
| `Video::Viewport` | 0x10 | Context.h:171 ✓ | **YES** — trivial four ints, and retail exports both `??0Viewport@Video@@QEAA@XZ` and `??0Viewport@Video@@QEAA@HHHH@Z`. Held by value in `Context` and returned by value (§2.5). |
| `Graphics::Camera` | 0xd8 | Camera.h:72 ✓ | NO — **held BY VALUE inside two array element types** (§3) |
| `Graphics::Scene` | 0x78 | Scene.h:116 ✓ | NO (by-value holder unproven; see §13) |
| `Graphics::Renderer` | 0x50 | Camera.h:208 ✓ | NO — **the ctor leaves +0x3c and +0x4c UNWRITTEN** (§6 rule B5-9) |
| `Graphics::Animation` | **undocumented** | — | see §13 |
| `Graphics::AnimationManager` | 0x9d8 | Animation.h:131 ✓ | NO — singleton (`?_singleton@AnimationManager@Graphics@@`) |
| `Graphics::AnimationState` | 0x48 | AnimationState.h:235 ✓ | NO — a 0x18-byte prefix of it is embedded in `MeshInstance` as `MeshAnimationState` (Mesh.h:139) |

Nested value types with their own ctors, all already documented and all candidates for a real
ctor/dtor: `Device::{RefreshRate 0x08, DisplayMode 0x18, Adapter 0x38, GraphicsInterface 0x18}`,
`Device::InterfaceSlot 0x18`, `RenderState::{AlphaBlend 0x14, Depth 0x10, Stencil 0x24}`,
`ShaderProgram::Parameter 0x10`, `GraphicsMesh::{Node 0x48, TriangleList}`,
`EdgeMesh::{TriangleVertex 8, EdgeVertex 16}`, `Scene::Node 0x18`.

### 2.5 Hidden return slots — 60 exports, 9 of them THROUGH A VTABLE SLOT

Every Video/Graphics export whose mangled return type is a class by value (`?AV`/`?AU`) uses the
MSVC hidden return slot: RDX is the caller's storage and **RAX must carry it back at every exit**.
63 symbols matched; three are `?AW4` (enum → EAX, NOT a retslot) and are excluded:
`?GetFormat@ImageBuffer@Video@@`, `?Compile@EdgeMesh@Graphics@@`,
`?Compile@AnimationGroup@Graphics@@`. **60 true hidden-retslot exports.**

The dangerous subset — reached through a **vtable slot**, so the slot typedef and the hand-built
row must both carry the retslot:

| slot | class | export | returns |
|---|---|---|---|
| 5 | `Graphics::Geometry` | `?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z` @0x1800a43c0 | `Math::Transform` |
| 5 | `Graphics::MeshInstance` | `?GetSubTransform@MeshInstance@…` @0x180101200 | `Math::Transform` |
| 6 | `Graphics::Geometry` | `?GetInstanceTransform@Geometry@…` @0x1800154a0 | `Math::Matrix` |
| 6 | `Graphics::MeshInstance` | `?GetInstanceTransform@MeshInstance@…` @0x180101280 | `Math::Matrix` |
| 6 | `Graphics::EdgeMeshInstance` | `?GetInstanceTransform@EdgeMeshInstance@…` @0x1800a3840 | `Math::Matrix` |
| 6 | `Graphics::ParticleInstance` | `?GetInstanceTransform@ParticleInstance@…` @0x1801047f0 | `Math::Matrix` |

Slots 5 and 6 are also live on `Light`/`AmbientLight`/`SpotLight`/`SunLight` and on
`ParticleSystemInstance`, inherited from `Geometry`/`ParticleInstance` (same target addresses) —
so **nine (class, slot) pairs** in total, on **two slot indices**. Those two indices are the whole
of the vtable retslot surface in Video and Graphics. The 2026-09-13 perf-only crash
(`1a4200c3`, §7 defect 4) was exactly one of them.

High-traffic non-slot retslot doors a caller batch will meet:
`?GetTransform@Geometry@…` (Transform), `?LocalAABB@Geometry@…` (Volume::Box),
`?GetCameraToWorld@Camera@…` ×2 and `?GetProjection@Camera@…` (Transform3 / Matrix),
`?GetFrustum@Camera@…` (Volume::Frustum), `?GetViewport@Context@Video@@` (Viewport),
`?GetSunCamera@SunLight@…` and `?GetSpotCamera@SpotLight@…` (**`Graphics::Camera` — 0xd8 — by
value**), `?GetDisplayMode@Device@Video@@` (DisplayMode, called from dx11),
`?GetMeshName@GraphicsMesh@…` / `?GetTextureName@Material@…` / `?GetConstantName@…` (String),
`?GetLength@Animation@…` / `?GetBlendTime@AnimationGroup@…` (System::Time), and eighteen
`ExternalPtr<T>` getters (8 bytes but **non-trivially copyable**, so still a retslot).

---

## 3. By-value holdings — the B34 §2 rule, instantiated

| holder | holds by value | evidence | consequence |
|---|---|---|---|
| `Video::Context` | `Viewport ×2`, `ConstantBufferOpaque ×7`, `MatrixRaw ×2`, `Vector4Raw ×7` | Context.h:257-305 | `ConstantBuffer` and `Viewport` must stay trivially default-constructible, or `Context` gains a ctor it must not have |
| `RendererDrawGroup` | **`Graphics::Camera cullCamera`** @+0xe0 | Camera.h:139 | inside `Collection::Array<RendererDrawGroup>` (Renderer+0x10) → `Array<T>::Resize` default-constructs and bulk-copies it |
| `RendererSceneEntry` | **`Graphics::Camera camera`** @+0x010 | Camera.h:156 | inside `Collection::Array<RendererSceneEntry>` (Renderer+0x00) → same |
| `Graphics::MeshInstance` | `Geometry _base` @+0x00 | Mesh.h:172 | this is INHERITANCE, expressed as embedding in the POD mirror |
| `Graphics::EdgeMeshInstance` | `Geometry _base` @+0x00 | Mesh.h:384 | ditto |
| `Graphics::ParticleInstance` | `GeometryBase base` @+0x00 | Particle.h:239 | ditto |
| `Graphics::SunLight` | `Geometry _base` @+0x00 | SunLight.h:62 | ditto (through `Light`, which adds nothing) |
| `Graphics::Geometry` | `Math::Transform _transform`, `_prevTransform` | Geometry.h:118-119 | Math is already a real class (B.3.1) — this is the WORKING case |
| `Graphics::MeshInstance` | `MeshAnimationState` (a 0x18 prefix of `AnimationState`) | Mesh.h:134-139 | a PREFIX view, not the class: §13 |

**RULE B5-4 (by-value instances).** `Graphics::Camera` is a by-value member of two element types
that live in `Collection::Array<T>`. Therefore `Camera` is **concrete with a `protected`
`= default` constructor and `friend RendererDrawGroup; friend RendererSceneEntry;`** (B4b-3), and
`static_assert(std::is_trivially_copyable_v<Camera>)` goes next to its size assert. The same for
`Video::Viewport` and `Video::ConstantBuffer` with respect to `Video::Context`. `Camera` and
`Renderer` therefore **convert in the same commit** (§4).

**RULE B5-5.** `Geometry` is the base of seven classes and is spelled as an embedded member in
four POD mirrors. When `Geometry` becomes a class, those four `_base` members become real base
classes in the same commit; a POD mirror that keeps `Geometry _base` while `Geometry` is a class
with a protected dtor will not compile, and one that keeps a *copy* of the layout is a new
duplicate POD. `Geometry`, `MeshInstance`, `EdgeMeshInstance`, `ParticleInstance`,
`ParticleSystemInstance`, `Light`, `AmbientLight`, `SpotLight`, `SunLight` are **one block**.

---

## 4. Q3 — classes that MUST travel together

Not the file map (that is `B35_FILEMAP.md`) — the *class* sets that inheritance or by-value
holding welds together. A block may not split one of these.

| set | why |
|---|---|
| `Geometry` + `MeshInstance` + `EdgeMeshInstance` + `ParticleInstance` + `ParticleSystemInstance` + `Light` + `AmbientLight` + `SpotLight` + `SunLight` | one inheritance tree, and `Geometry` is embedded by value in four mirrors (§3) |
| `Camera` + `Renderer` (+ `RendererDrawGroup`/`SceneEntry`/`DrawItem`/`Drawable`) | `Camera` held by value in two of Renderer's element types; **all five layouts live in one file, `runtime/types/graphics/Camera.h`** |
| `Particle` + `ParticleSystem` + `ParticleInstance` + `ParticleSystemInstance` | `ParticleSystem : Particle`; `ParticleSystem::CreateSystem` returns `ParticleInstance*`; slot-order trap §2.3.2 |
| `ImageBuffer` + `GeneratedImageBuffer` + `FrameCopyImageBuffer` + `ReflectionImageBuffer` | three-level chain crossing the Video/Graphics namespace boundary (`GeneratedImageBuffer`'s parent is `Video::ImageBuffer`) |
| `ShaderProgram` + `VertexProgram` + `PixelProgram` | chain |
| all twelve `*Data` | one Runtime file (`companion_data.cpp`) holds all 24 ctor/dtor bodies, and one dx11 seam holds all 84 concrete slots |
| `Context` + `Viewport` + `ConstantBuffer` | by value, ×9 members |
| `Device` + `Device::{Adapter,DisplayMode,RefreshRate,GraphicsInterface,InterfaceSlot}` + `Configuration` | `Device`'s arrays are of these; `Configuration` is `Device`'s singleton companion and dx11 reads both singletons |
| `Material` + `MaterialInstance` | `MaterialInstance` reads `Material`'s render state; `RendererDrawItem::material` is `MaterialInstance+0x28` by value |
| `VertexLayout` + `VideoProgram` | `?GetVertexLayout@VideoProgram@` returns `ExternalPtr<VertexLayout>`; VertexLayout's header does not exist yet (§1.3.2) |

**Cross-cutting files that are NOT any block's** — each stamps vtables of classes in different
sets, so the OWNER handles them, not a block (addresses grepped from the tree this pass):

- `runtime/src/object/deleting_dtor.cpp` — EdgeMesh, EdgeMeshInstance, MeshInstance, Light, SunLight
- `runtime/src/graphics/animation.cpp` — AnimationEvent **and** Particle, ParticleInstance,
  ParticleSystemInstance (so `animation.cpp` is NOT disjoint from the particle family)
- `runtime/src/graphics/mesh.cpp` — EdgeMesh, EdgeMeshInstance, MeshInstance
- `runtime/src/graphics/renderer.cpp` + `wave24_lane_gfxmat.cpp` — MaterialInstance
- `runtime/src/graphics/closure_batch2b.cpp` + `scene.cpp` — Geometry
- `code/Game/Component/statusicon_geometry.cpp` — **a GAME file that names `Geometry`'s vtable
  address 0x180154e60**; the only cross-tree vtable reference found
- `standalone/reg_mint_promoted.cpp` — MaterialInstance, AnimationEvent, Geometry, ImageBuffer

The clean ones: `imagebuffer_derived.cpp` (exactly the three derived image buffers),
`lights_vtable.cpp` (exactly Ambient/Light/Spot), `companion_data.cpp` (exactly the twelve
`*Data`), `graphicsmesh_vtable.cpp`, `edgemesh_vtable.cpp`, `material.cpp`,
`vtbatch7_animgroup_particlesystem.cpp` (ParticleSystem + Particle).

---

## 5. Q4 — threading and device-lost

### 5.1 The Device critical section is RETAIL and it is LOAD-BEARING

`RtVideo::Device` owns a real `CRITICAL_SECTION` at **+0x60**, with `lockOwnerThreadId` at +0x88
and `lockRecursionCount` at +0x8C (`Device.h:190-191`). `runtime/src/video/buffers_win.cpp:21-87`
reproduces retail's hand-rolled recursive lock, re-reading `*_singleton` on unlock; its banner
records that four independent decompiles agree byte for byte. Four call sites take it:
`{Image,Index,Vertex,Render}Buffer::Platform{Construct,Destruct}` — the intrusive
`_first*` lists at `?_firstBuffer@…`, `?_firstRenderBuffer@…`, `?_firstSurface@…` are what it
protects. `runtime/src/video/context_lifecycle.cpp:68` takes the same lock in the singleton-reading
form. dx11 takes it too, by name (`?Lock@Device@Video@@QEAAXXZ` /
`?Unlock@…`, `dx11/src/contextdata.cpp:276,508`).

**RULE B5-6.** `Video::Device`'s lock stays a byte-faithful opaque `uint8_t lock[0x28]` with the
hand-rolled recursive acquire. Do NOT replace it with `std::recursive_mutex` or a scoped guard:
the owner-tid and depth fields are at PINNED offsets that dx11 and four Platform bodies read, and
`Device.h`'s banner records that the header is included from `runtime/src/`, which PLAN §4.1
decision 1 keeps free of `<windows.h>`. `Device` therefore also keeps **no real ctor/dtor**
(§2.4) — `InitializeCriticalSection` is retail's `Device::Startup`, not a constructor.

### 5.2 What runs off the main thread

One background thread: `Engine::Loader`, spawned in `standalone/loader_loop_stubs.cpp:143-149`
and joined by `WaitForEngineLoader()` / `LoaderLoopShutdownWait()`. `standalone/async_loader.*` —
a second loader — was DELETED in `96c17f47`; `Engine::Loader` is now the sole dispatcher, which is
the fix the two unreproduced open defects (a)/(b) are attributed to.

That thread constructs Graphics objects directly, in `code/Game/Game/gameupdateloop_load.cpp`:
`Camera` ctor (:570), `MeshInstance` ctor + `SetMesh` (:802), `Renderer` ctor (:986), `Scene` ctor
(:1090), `SunLight` ctor + `Set` (:1136), `Scene::AddGlobalObject` (:1654). The main thread's
`Draw` reads `std::atomic<void*> s_worldBuiltLoop` (:885) to avoid rendering a half-built world.
The only lock those ctors take is the **global `MemoryAllocator` critical section** at
`_singleton+0x640`, which retail inlines into `Scene::AddGlobalObject` and the particle paths
(`runtime/src/graphics/scene.cpp:234-238`, `wave27_lane_gfxpart.cpp:105-108`).

**RULE B5-7.** The conversion order is constrained by this: `Camera`, `Renderer`, `Scene`,
`SunLight`, `MeshInstance` and `Graphics::Scene::AddGlobalObject` are constructed on the loader
thread while the main thread is in `Draw`. A real constructor on any of them runs off the main
thread. Since none of them gets a real ctor anyway (§2.4, B4b-2), this is a reason the rule holds
rather than a new hazard — but any conversion that adds a static initialiser, a lazy singleton or
a first-call cache to those classes introduces a data race that did not exist. Caches added to
these six files must be `std::atomic` or must not exist.

### 5.3 Device-lost DOES exist, and it is entirely dx11's

`dx11/src/contextdata.cpp:564-720` (`ContextData_Finish`, retail `0x1800017c0`) calls
`IDXGISwapChain::Present` (:651), checks the HRESULT, and on
`DXGI_ERROR_DEVICE_REMOVED`/`_RESET` (or `GetDeviceRemovedReason()`, :666) runs a full recovery:
notify `onLost` → `?ReleaseDynamic@Device@Video@@` → null `d3dDevice` → `DeviceData` vtable
shutdown+create → `?RecreateDynamic@Device@Video@@` → notify `onRestored`, or
`System::Error::SetText/Display` on failure. `ForceDeviceLostThisFrame()` (:617-637) injects a
synthetic reset under `Opt().forceDeviceLost`, so the path is TESTABLE.

Two things hang off it:
- `dx11/src/buffers.cpp:940-977` holds a `std::mutex CbRegistryLock()` over a
  `std::vector<ConstantBufferData*>`, touched only on create/destroy and once per device reset.
  Its own comment says it fixes a **port-introduced** dangling-binding bug after `ClearState()`;
  there is no `// retail` near it. It is the only non-retail lock in the Video/Graphics port.
- `dx11/src/contextdata.cpp:537-542` deliberately does not check `GetBuffer`'s HRESULT, because
  retail does not either and does not init the out-pointer.

**RULE B5-8.** Device-lost recovery is the reason the `*Data` concrete side is a declared leave
(§11): `ContextData_Finish` reaches `DeviceData`'s vtable slots by index to shut down and recreate
the device, and the seam's completeness check is what guarantees those slots are non-null.
Converting the concrete `*Data` classes means re-deriving that guarantee. Anyone who touches
`contextdata.cpp` in step 5 must run the `BANISHED_FORCE_DEVICE_LOST` route, not just `sa-run`.
`Dx11TrackConstantBuffer` / `Dx11Untrack…` / `Dx11InvalidateConstantBufferBindings` and their
mutex are **kept as-is**, with their comment, when `ConstantBufferData` converts.

### 5.4 `res_load_lock` — the carry-over note is RIGHT, and a subagent's contradiction was wrong

`NEXT_SESSION.md` §1 item 10 says "`standalone/res_load_lock.{h,cpp}`'s header rule is followed by
exactly one body". Verified repo-wide this pass (`ResLoadGuard|res_load_lock`, build dirs
excluded): **exactly one body**, and it is not the one the header's example names. It is
`ExpandRelativePath` at **`runtime/src/object/external_path_win.cpp:356`** (`ResLoadGuard resGuard;`,
with the header included at :63 and the reason at :34). `Object::ExternalFactory::FindOrLoad` and
`Object::External::Load` — the two functions the header declares the rule FOR — do **not** take it.
A block-0 subagent scoped its grep to `runtime/src/object/external*.cpp` and reported "zero
bodies"; that is a false negative. Nothing in Video or Graphics takes the guard. The live/dead
decision stays a carry-over, not step-5 work.

### 5.5 dx11 has NO IO dependency, on purpose

`dx11/src/buffers.cpp:253-279` (`SerializerStreamMode`) reads a stream direction at raw offsets
rather than calling `IO::Serializer::GetDirection()` (retail 0x180006210), and its comment says
why: the translation unit includes no `runtime/types` header and pulling
`runtime/types/io/Serializer.h` into dx11 for one expression "is a module-boundary decision, not a
caller batch's". **§1.2 settles that decision: dx11 already includes four `runtime/types` headers,
so the boundary argument no longer holds.** Convert this site in the `ConstantBufferData` /
`buffers.cpp` commit and retire the carry-over. It is one expression and it removes a fifth
duplicate offset view.

---

## 6. Q5 — the four closed defects, as rules

### Defect 1 — `Video::Device::InitializeInterfaces` and `LoadLibraryW` (fixed `c1fdd7f6`)
Retail `LoadLibraryW`'d `VideoDX9-x64.dll`, which statically imports `Runtime-x64.dll`, mapping
the DLL behind `ResolveInit`'s back; the first heap call in the mapped image faulted. Fix:
`runtime/src/video/video_interfaces_win.cpp`'s per-slot
`LoadLibraryW`/`GetProcAddress`/`FreeLibrary` block (retail VA 0x1800f7a44..0x1800f7ac9) is
deleted; the loop still resolves the plugin name via `Device::GetDLLForInterface` and loads
nothing, so `loaded`/`rejected` stay 0.

> **RULE B5-9.** `Video::Device::InitializeInterfaces` keeps its deleted `LoadLibraryW` arm when it
> becomes a member of the real `Video::Device`, with the retail VA range in the comment. A real
> class must not "restore" the faithful body. Files: `runtime/src/video/video_interfaces_win.cpp`,
> `standalone/video_interface.cpp` (which writes our two function pointers into Device+0xd0/+0xd8),
> `dx11/src/exports.cpp`. `?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z` is a
> hidden-retslot door (§2.5) — it is in the same function.

### Defect 2 — `Graphics::Geometry` copy ctor through a NULL raw door (fixed in `28162092` / `5084c732`)
`RtGeometryCopyCtor()` in `runtime/src/graphics/mesh.cpp` was still on `RT_ORIGINAL_ONCE` while
every sibling resolver in the file had moved to `RT_EXPORT_ONCE`; after A.1 it resolved NULL and
every `MeshInstance`/`EdgeMeshInstance` copy called through null.
`??0Geometry@Graphics@@QEAA@AEBV01@@Z` @0x180015b60 is registered in `closure_batch2b.cpp`, and
Ghidra confirms that address is one of the eight referencers of `??_7Geometry@Graphics@@6B@`
(as `Graphics_Geometry_CopyCtor`).

> **RULE B5-10.** `Graphics::Geometry`'s copy constructor is a REAL, REGISTERED, EXPORTED body —
> not a compiler default. When `Geometry` becomes a class it must declare
> `Geometry(const Geometry&)` explicitly with that body, because four mirrors embed it by value
> (§3) and `Collection::Array` copies of the derived classes go through it. `= default` here
> silently replaces refcount handling with a bitwise copy. Files: `runtime/src/graphics/mesh.cpp`,
> `closure_batch2b.cpp`, `runtime/types/graphics/Geometry.h`, `Mesh.h`, `Particle.h`, `SunLight.h`.
> Also: `grep -n RT_ORIGINAL_ONCE` must return 0 in every file a block touches — STATUS.md:809-811
> records two mesh lines that silently bypassed ported bodies for a month.

### Defect 3 — a registration stripped from the PERF build because it sat inside a `printf` (fixed `1a4200c3`)
`printf("[RtPort] BATCH C: %d graphics leaves registered\n", RtGraphicsBatchCLeavesRegister());`
— the perf build's `standalone/log_strip.h` compiles `printf` to `((void)0)` and drops the
argument, so `?PlatformSerialize@GraphicsMesh@` was never registered in any perf exe and boot
died on a VCALL-of-0. Seven sites, in `rt_graphics_port.cpp`, `rt_video_port.cpp`,
`rt_audio_port.cpp`, `rt_achievement_port.cpp`, `rt_math_port.cpp`. A block-0 sweep of the whole
tree (`printf|fprintf|TRACE` with an `Rt*Register()` call in the argument list, multiline) found
**zero remaining sites**, agreeing with STATUS.md:578.

> **RULE B5-11.** Never put a `Register*()` call — or any call with a side effect — inside a
> `printf`/`fprintf`/`TRACE` argument list, anywhere under `runtime/src/`, `dx11/`, `standalone/`.
> Hoist to a named local on its own statement, then log the local. This is a **gate item, not a
> review item**: §2e already requires a perf build, and the warning A/B must run with the PERF
> flag set (a hoisted local that is then only logged becomes an `unused variable` warning in the
> perf build — that is the expected new warning, not a defect). Watch
> `runtime/src/video/rt_video_port.cpp` and `runtime/src/graphics/rt_graphics_port.cpp`: they are
> step 5's own registration files and every block edits them.

### Defect 4 — a `Math::Matrix` returned BY VALUE from a vtable slot (fixed `1a4200c3`)
`RtGraphics_ParticleInstance_GetInstanceTransform` (`wave27_lane_gfxpart.cpp`) was declared
`void`; the hidden return slot arrives in RDX and must be returned in RAX at every exit. At `-O0`
RAX happened to still hold it; at `-O2` it held garbage and `Renderer::DrawItemList` →
`Context::SetTransform` dereferenced it. Fix: return type `Matrix*`, `return out;` on every arm
including the early return. The commit audited all 175 registered by-value-returning exports and
found no other `void` port.

> **RULE B5-12.** The two retslot slot indices are **5 (`GetSubTransform` → `Math::Transform`)
> and 6 (`GetInstanceTransform` → `Math::Matrix`)**, live on nine (class, slot) pairs (§2.5).
> When the Geometry block declares those virtuals, they return the value type BY VALUE in C++ —
> GCC's Microsoft-ABI lowering produces the same hidden slot and the same RAX return, which is the
> point of using a real class here. Every remaining `fn_*` typedef and hand-built row for those
> slots must keep `Matrix*`/`Transform*` as its return type until the row is deleted, and must
> never be "simplified" to `void`. **Verify at `-O2`, not `-O0`** — this defect is invisible in a
> debug build. The wider rule: a door for any of the 60 hidden-retslot exports (§2.5)
> placement-news the member's prvalue into the caller's slot and returns that pointer; a caller
> converts only as "the door body moved to the call site" (`B34B_RULES` §2d).
>
> **COORDINATOR AMENDMENT (block E, measured 2026-09-19): the "BY VALUE in C++" sentence above is
> WRONG for VIRTUALS.** GCC's ms_abi passes the hidden return pointer FIRST (RCX) and `this`
> second (RDX) for a member function; MSVC — and every minted row — passes `this` in RCX and the
> return slot in RDX. A by-value virtual therefore writes the result THROUGH `this` when called
> from a retail-shaped caller (block E's first unit test segfaulted exactly so). **A virtual that
> retail returns by value is declared with the out-parameter spelled in retail order:
> `T* M(T* ret, args…) const`** (`Geometry.h` B5-12 block; `tests/unit/graphics_geometry_abi_test.cpp`
> PART C pins both halves). NON-virtual by-value members are unaffected as long as they are only
> reached by C++ callers or through a door that placement-news the prvalue. Also from block E:
> §2.5's "eighteen `ExternalPtr<T>` getters are retslot" is DOUBTFUL — the diffed ports pass
> `ExternalPtr` in a register, i.e. MSVC treats it as trivially copyable, and an 8-byte trivially
> copyable class returns in RAX. Settle per getter from its retail prologue before writing a door.

### 6.1 Also carried forward (from STATUS.md and the inventory, same sweep)

- **`Renderer`'s ctor leaves +0x3c and +0x4c unwritten**, and `RendererDrawItem` leaves `_pad24`
  and `_pad34` unwritten on every append (Camera.h's two "TWO GAPS ARE REAL" banners). **RULE
  B5-13:** a real `Graphics::Renderer` must not member-initialise `field_3c` or `_pad_4c`, and a
  real `RendererDrawItem` must not zero its two pads. A constructor with an init-list that covers
  every member is a divergence here. `AnimationEvent`'s ctor oracle already trips the
  released-pointer detector for the same class of reason (STATUS.md:774-775) — make it NO-ORACLE.
- **`AnimationGroup::Serialize` field order**: emitting the +0x30 float before the tag array made
  `CompressedStream::Read` HANG, not crash (STATUS.md:697-699, closed). A Serialize field-order
  mistake in step 5 presents as a hang; `tools/boot_state.sh --wait 300` and exit 4 +
  `standalone_hang.log` are how it shows.
- **19 `GetFileVersion` ports written as a constant were audited; ports that return through a
  variable were NOT** (STATUS.md:731-732). Every external in §2.2 has a `GetFileVersion` at slot
  3. Audit the variable-returning ones as they convert. `VertexLayout::GetFileVersion` returning 1
  instead of retail's 3 is what silently dropped every draw item once (STATUS.md:710-713).
- **`MeshInstance::Update` reads two UNEXPORTED `.data` statics by address** — guard 0x1801ff30c
  and a 48-entry identity `Transform` table 0x1801ff310 — and declines the whole update if they
  resolve NULL (STATUS.md:715-718, closed by `EXTRA_STORAGE_BY_ADDR`). Those two rows must survive
  the `MeshInstance` conversion; they are not in the cutover manifest and so are not protected by
  the manifest gate.
- **`runtime/src/video/enumerate_video_modes.cpp` has `__try`/`__except`** (B34B_RULES:80) —
  §B.1.7 puts SEH last. Leave the file's exception structure alone in step 5.
- **`StatusIconComponent` slot 11 calls `Scene::AddObject` BEFORE `Geometry::SetTransform`**, and
  `Geometry::Clean` is two-phase (`docs/retail_bugs.md:18-23`). Both are retail ordering to
  PRESERVE. `code/Game/Component/statusicon_geometry.cpp` is also the one game file naming a
  Runtime vtable by address (§4).
- **`runtime/src/graphics/README.md` says "Ported here: nothing yet" and `dx11/src/README.md`
  lists files "still to come" that already exist.** Both are STALE. Fix them in the block that
  first touches the directory; do not trust either as a status source.
- **Grep hygiene:** `System::` matches inside `ParticleSystem` / `ParticleSystemInstance`
  (B34B_RULES:20-21), and `sunlight.cpp:386`'s "throw away" is not a `throw`.

---

## 7. Q6 — the caller surface, measured

All counts exclude `standalone/build*/`, `tests/unit/build/`, `dx11/build*/`, `runtime/build*/`,
`.claude/worktrees/`, `docs/`. Unit is stated per row. (a)-(h) were measured by a subagent with
Python regexes over 4,948 files; §7.1 is this pass's own correction to them.

| binding | unit | total | code/Game | engine | standalone | runtime/src | dx11/src |
|---|---|---|---|---|---|---|---|
| (a) `fn_Video_*`/`fn_Graphics_*` doors | CALL SITES | **61** | 61 | 0 | 0 | 0 | 0 |
| (b) `ResolveExport("…@Video@@/@Graphics@@…")`, wrapped literals handled | CALL SITES | **246** | 78 | 74 | 36 | 45 | 13 |
| (c) `RT_EXPORT_ONCE(…"@Video@@/@Graphics@@"…)` | CALL SITES | **147** | 0 | 0 | 0 | 147 | 0 |
| (d) `ResolveAddr(Module::Runtime, 0x…)` onto the V/G address set | CALL SITES | **56** | 7 | 5 | 7 | 34 | 3 |
| (e) hand-inlined `(*(void***)p)[N]` on a plausibly-V/G receiver | CALL SITES | **118** | 69 | — | — | 49 (video 27, graphics 22) | 0 |
| (f) files caching V/G doors into file-local statics | FILES / CACHES | **26 / 94** | 15 / 46 | 0 | 0 | 11 / 48 | 0 |

- `engine/funcs/engine_funcs.h` declares exactly **21** `fn_Video_*`/`fn_Graphics_*` typedefs and
  **21** matching externs — no mismatch. All 61 call sites are in `code/Game`. So an
  `engine_funcs.h`-anchored sweep sees 61 of ~630 bindings: the inventory's "would undercount by
  4x" is itself an undercount — it is **~10x**.
- Wrapped string literals accounted for **15** of (b): a naive line-based grep reports 231.
- Runtime-internal: of (b)+(c) = 393 sites, **205** are inside `runtime/src/**` or `dx11/src/**`
  (these convert inside the class blocks) and **188** outside (code 78, engine 74, standalone 36).
  runtime/src breakdown: graphics 111, video 34, engine 17, save 16, object 6, ui 4, collection 4.
- Top doors by call sites: `fn_Graphics_Scene_AddGlobalObject` 12, `fn_Graphics_Scene_AddObject` 7,
  `fn_Graphics_MeshInstance_SetMesh` 4, `fn_Graphics_Camera_Ctor` 4, `fn_Graphics_Scene_Clean` 3,
  `fn_Graphics_Renderer_Ctor` 3, `fn_Graphics_Scene_Ctor` 3, `fn_Graphics_SunLight_Set` 3.
- Heaviest `code/Game` files: `menudialogloopt.cpp` 27, `minimap_paint.cpp` 21,
  `gameupdateloop_load.cpp` 12, `gameupdateloop.cpp` 11, `tornado.cpp` 7,
  `statusicon_geometry.cpp` 6, then a long tail of 3-5.
- Heaviest file-local caches: `runtime/src/collection/array.cpp` 26 (26 distinct `Array<T>`
  helper addresses — legitimate), `code/Game/Game/gameupdateloop.cpp` 9,
  `code/Game/Component/statusicon_geometry.cpp` 6, `runtime/src/graphics/wave27_lane_gfxpart.cpp` 5,
  `code/Game/Player/tool_base_tilerender.cpp` 5.

### 7.1 A NINTH caller spelling: dx11's table-driven import struct

`NEXT_SESSION.md` §2d lists eight binding spellings. dx11 uses a ninth, and **it is why (b)
reports only 13 sites for `dx11/src`**:

```c
struct Entry { void** dst; const char* mangled; uintptr_t iat; };
const Entry table[] = {
    { (void**)&g_im.CtorContextData, "??0ContextData@Video@@QEAA@PEAVContext@1@@Z", 0x180013478 },
    ...
};   // then a loop calling Dx11ResolveRuntimeImport(e.mangled, e.iat)
```

Measured: `dx11/src` has **13** `Dx11ResolveRuntimeImport(` call sites, **18** `ResolveExport(`
sites, **4** `ResolveAddr(` sites — and **76 distinct mangled Runtime symbol literals**. The
literals are DATA in a table; a grep anchored on `ResolveExport(` or on a call shape finds the
loop, not the 76 bindings.

> **RULE B5-14.** Spelling 9: **a mangled name held as a `const char*` in an initialised table or
> struct, resolved positionally by a loop.** `dx11/src/devicedata.cpp` (the largest, ~40 rows),
> `contextdata.cpp`, `buffers.cpp`, `imagebufferdata.cpp`, `shaderprograms.cpp`,
> `smallclasses.cpp`, `vertexlayoutdata.cpp`, `renderstatedata.cpp`, `exports.cpp`. Size a dx11
> caller batch by the **literal count (76)**, never by the call-site count (13). The
> `uintptr_t iat` third column is dead documentation of a shipped IAT slot — it is not a binding
> and must not be counted as one.

### 7.2 Which Video/Graphics members dx11 actually calls

From the 76 literals: the twelve `??0*Data` base ctors and their `??1` dtors; `Device::{Lock,
Unlock, ReleaseDynamic, RecreateDynamic, AdjustDisplay, GetDisplayMode, GetMultisampleModeCount,
IsMultisampleModeSupported}`; `Device::{Adapter,GraphicsInterface,DisplayMode}` ctors/dtors and
`Adapter::FindOrAddMode`; `Context::{SetViewport, SetRenderBuffer, SetImageBuffer,
SetVertexBuffer, BindTextures, BindResources}`; `RenderBuffer::Bind`; `ConstantBuffer::Bind`;
`{Vertex,Index}Buffer::{Lock,Unlock}`; `ImageBuffer::{Width, Height, IsDynamic, IsCompressed,
IsRenderBuffer, IsDepthBuffer, IsRendered, IsGenerated, GetLevelCount, CopyFromTexture}`; the two
singletons `?_singleton@Device@Video@@` and `?_singleton@Configuration@Video@@`; and from
System/IO/Collection: `MemoryAllocator::{MemAlloc,MemFree}`, `WideString::AsUTF8`,
`Serializer::{GetEncoding,Serialize×2,SerializeBytes}`.

That is the dx11 caller batch's whole scope, and it is a clean cut: **dx11 calls `Device`,
`Context`, `ImageBuffer`, `RenderBuffer`, `ConstantBuffer`, the two vertex/index buffers, the
twelve `*Data` bases and `Configuration` — and no Graphics class at all.**

---

## 8. The rules, collected

| # | rule | §|
|---|---|---|
| B5-1 | Runtime owns ONE abstract class per `*Data` (0x10, slot 0 concrete, rest pure); dx11 owns the concrete subclass and the vtables; assert the two offsets, NOT `sizeof` | 1.1 |
| B5-2 | Collapse the five duplicate views in `dx11_backend_access.h`: delete `ImageBufferOwner`/`ShaderProgramOwner` and include the Runtime headers; AUTHOR `runtime/types/video/VertexLayout.h` before any block touches VertexLayout; delete `static_assert(sizeof(RenderBufferOwner)==0x10)` on sight and collapse it with `Context.h`'s `RenderBufferPrefix` into `Device.h`'s `RenderBuffer` | 1.3 |
| B5-3 | A dx11 dtor's base-dtor call becomes a QUALIFIED non-virtual `~XData()`, never `DeletingDtor(0)` | 1.4 |
| B5-4 | `Camera`, `Viewport`, `ConstantBuffer`: concrete + protected `= default` + `friend` the holder + `is_trivially_copyable` assert. `Camera` and `Renderer` convert in ONE commit | 3 |
| B5-5 | `Geometry` and its seven subclasses and four by-value mirrors are ONE block | 3 |
| B5-6 | `Video::Device`'s `CRITICAL_SECTION` stays an opaque `uint8_t lock[0x28]` with the hand-rolled recursive acquire; no `std::mutex`, no `<windows.h>` in the header, no real ctor/dtor | 5.1 |
| B5-7 | No static initialiser, lazy singleton or non-atomic first-call cache in `Camera`, `Renderer`, `Scene`, `SunLight`, `MeshInstance`, `Scene::AddGlobalObject` — the loader thread constructs them while `Draw` runs | 5.2 |
| B5-8 | The concrete `*Data` side and the seam stay; anyone touching `contextdata.cpp` runs the `BANISHED_FORCE_DEVICE_LOST` route; keep the `CbRegistry` mutex and its comment | 5.3 |
| B5-9 | `InitializeInterfaces` keeps its deleted `LoadLibraryW` arm | 6 |
| B5-10 | `Geometry`'s copy ctor is explicit, never `= default`; `grep RT_ORIGINAL_ONCE` == 0 in every touched file | 6 |
| B5-11 | No side-effecting call inside a `printf`/`TRACE` argument list; perf build is a gate item | 6 |
| B5-12 | Slots 5 and 6 of the Geometry family are hidden-retslot; keep `Transform*`/`Matrix*` on every surviving typedef; verify at `-O2` | 6 |
| B5-13 | Keep retail's unwritten bytes: `Renderer` +0x3c/+0x4c, `RendererDrawItem` `_pad24`/`_pad34`. No all-members init-list | 6.1 |
| B5-14 | Spelling 9 exists (a mangled name in a table, resolved by a loop). Size dx11 batches by the 76 literals, not the 13 call sites | 7.1 |
| B5-15 | `DEFINE_TYPE`'s name string is the EXACT retail literal, copied not derived — see below | 8.1 |
| B5-16 | `Construct` is introduced per branch at slot 8, EXCEPT `ParticleSystem` where it is slot 9 behind `CreateSystem`. `virtual`, not `override`, on each branch's first class | 2.3 |
| B5-17 | Name the two `Draw` overloads on the Light branch distinctly (slots 2 and 10) | 2.3 |

### 8.1 RULE B5-15, in full

Retail's `TypeInfo` name strings are **not uniformly namespaced**, and the type id is the CRC of
the exact string, which saves depend on (`ENDGAME.md` §B.1.4, and
`reference_basetypeinfo_name_is_content_only.md`). Read byte-wise this pass:

```
?_TypeName@GeneratedImageBuffer@Graphics@@        -> "GeneratedImageBuffer"     (no namespace)
?_ParentTypeName@GeneratedImageBuffer@Graphics@@  -> "Video::ImageBuffer"       (namespaced)
?_ParentTypeName@FrameCopyImageBuffer@Graphics@@  -> "GeneratedImageBuffer"     (no namespace)
?_ParentTypeName@PixelProgram@Video@@             -> "ShaderProgram"            (no namespace)
?_ParentTypeName@RenderState@Video@@              -> "Object::External"         (namespaced)
?_ParentTypeName@ParticleSystem@Graphics@@        -> "Particle"                 (no namespace)
```

The pattern is "namespace-qualified iff the parent is in a DIFFERENT namespace", but it is a
pattern, not a guarantee. `DEFINE_TYPE(Class, Parent, Ext)` must be handed the literal that was
read out of the binary for that class, per class, and a block's diff must show the read. A
`#Parent` stringisation of a C++ base name produces `Object::External` for RenderState (right) and
`Graphics::Particle` for ParticleSystem (**wrong** — the CRC changes and every save that names a
ParticleSystem stops loading). All 36 literals are recorded in this pass's scratch output; re-read
them, do not reconstruct them.

## 9. The REQUIRED storage rows — `MintStorageCell` redirect owed, not dropped

`python tools/cutover_manifest.py --table` → 59 tagged gap rows, `REQUIRED 58 | WITHHELD 1`, of
which **Video 18 / Graphics 4 = 22, all REQUIRED, none withheld**. Twelve are the `??_7*Data`
vtables of §1.1 and are answered by the mint; the other ten are statics. Per `B34B_RULES` §4.1 and
**RULE B4b-4**, each static row below is OWED a `MintStorageCell()` redirect **at the class-static
conversion, the END of B.3** — not in step 5. Step 5 converts the FUNCTIONS and leaves these as
`RT_DATA_SLOT` reads, each with a one-line comment naming this rule.

| manifest line | row | DLL va | becomes | redirect owed |
|---|---|---|---|---|
| 129 | `?_singleton@Device@Video@@0PEAV12@EA` | 0x1801fe7a8 | `static Device* Video::Device::_singleton` | YES — **36 by-name doors**, the highest in the manifest; dx11 also reads it by address (`kRt_VideoDeviceSingleton`) |
| 127 | `?_singleton@Configuration@Video@@0PEAV12@EA` | 0x1801fe7a0 | `static Configuration* Video::Configuration::_singleton` | YES — 16 doors; dx11 reads it by name |
| 119 | `?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA` | 0x1801fe788 | `static IndexBuffer* …::_firstBuffer` | YES — 3 doors; the intrusive list head guarded by Device+0x60 |
| 121 | `?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA` | 0x1801fe798 | `static VertexBuffer* …::_firstBuffer` | YES — 3 |
| 123 | `?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA` | 0x1801fe790 | `static RenderBuffer* …::_firstRenderBuffer` | YES — 3 |
| 125 | `?_firstSurface@ImageBuffer@Video@@0PEAV12@EA` | 0x1801fe780 | `static ImageBuffer* …::_firstSurface` | YES — 3 |
| 33 | `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` | 0x1801fe7b0 | `static AnimationManager* …::_singleton` | YES — 6 |
| 31 | `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` | 0x1801fc70b | `static bool Graphics::MeshInstance::_rootOnlyUpdates` | YES — 4 |
| 27 | `?_frameIndex@ParticleInstance@Graphics@@1HA` | 0x1801fe77c | `static int Graphics::ParticleInstance::_frameIndex` | YES — 4 (`1HA` = protected) |
| 29 | `?_rng@Particle@Graphics@@1VRandom@System@@A` | 0x1801fe7f0 | `static System::Random Graphics::Particle::_rng` | YES — 4. **`System::Random` is a by-value holder seeded through a ctor DOOR** — B4b-2's double-seed case applies verbatim |
| 95-117 | the twelve `??_7*Data@Video@@6B@` | see §1.1 | stay minted while the seam stays | n/a — §11 |

Not in the manifest and therefore NOT protected by the manifest gate, but load-bearing:
`MeshInstance::Update`'s two unexported `.data` reads at **0x1801ff30c** (a guard byte) and
**0x1801ff310** (a 48-entry identity `Transform` table), minted as `EXTRA_STORAGE_BY_ADDR`
(§6.1). The `MeshInstance` block must carry them forward explicitly.

## 10. Order of moves inside step 5

1. Author `runtime/types/video/VertexLayout.h` (B5-2.2). Nothing else can start on VertexLayout.
2. The twelve `*Data` abstract bases (one file, `companion_data.cpp`) — this proves B5-1 and B5-3
   on 3-slot classes before the 20-slot ones matter.
3. Collapse the duplicate views (B5-2) as each owner class converts.
4. The Video program/shader family, **`ShaderProgram`/`VertexProgram`/`PixelProgram` first, then
   `VertexLayout`, then `VideoProgram` LAST and on its own** (three different parents, §2.1).
5. `Video::ImageBuffer` → `GeneratedImageBuffer` → `FrameCopy`/`Reflection` (one chain, and it
   crosses the namespace boundary).
6. `Video::RenderState` (the one direct-`External` Video class with a documented header).
7. `Video::Context` + `Viewport` + `ConstantBuffer` (the 0x2168 POD, nine by-value members).
8. `Video::Device` + its four nested value types + `Configuration` + `RenderBuffer` + the two
   buffers (B5-6; touches the lock and the four intrusive lists).
9. The Geometry tree (B5-5) — biggest, highest caller traffic, and the retslot slots.
10. `Camera` + `Renderer` in one commit (B5-4).
11. `Material` + `MaterialInstance`; `GraphicsMesh` + `EdgeMesh`; `Scene`.
12. `Particle` + `ParticleSystem` + the two instances (B5-16's slot trap).
13. `Animation` + `AnimationGroup` + `AnimationState` + `AnimationManager` + `AnimationEvent`.
14. Caller batches by directory, `code/Game/Game` first (the four heaviest files are there).
15. The dx11 caller batch (76 literals, B5-14) — after every Runtime class it names is real.
16. The cross-cutting files (§4) and the two stale READMEs — the owner, at the close.

## 11. Deliberately NOT converted in step 5

| left alone | why |
|---|---|
| the twelve **concrete** `*Data` classes in `dx11/` and the 84-slot seam | `Dx11SeamInstall()`'s completeness check is the only guarantee those slots are non-null, and device-lost recovery dispatches through them by index (B5-8). Converting them means re-deriving that guarantee. The abstract Runtime half converts; the concrete side is a step-6+ item. |
| the twelve `??_7*Data@Video@@6B@` mint rows and `??_7X` rows generally | mint rows and deleting-dtor rows retire at the END of B.3, per `ENDGAME.md` §A.6 / §B.2.5 and B34B_RULES §4. Regenerate with `rt_mint_gen.py`, never hand-edit. |
| the ten `CUTOVER_REQUIRED` statics of §9 | `MintStorageCell` redirect owed at the class-static conversion, end of B.3 (RULE B4b-4, coordinator's 2026-09-19 ruling). |
| ctor DOORS for classes with no real ctor (`fn_Graphics_Camera_Ctor`, `_Renderer_Ctor`, `_Scene_Ctor`, `_SunLight_Ctor`, `_MeshInstance_Ctor`, `_Geometry_Ctor`) | B4b-2: no real constructor on a polymorphic class, nor where a by-value holder is seeded through a ctor door. The doors stay; the call sites stay. `?_rng@Particle@Graphics@@` is the double-seed case (§9). |
| `System::MemoryAllocator` | deferred past step 4 on purpose (~1,146 sites, `NEXT_SESSION` §1 item 5). dx11 and every Graphics ctor path call `MemAlloc`/`MemFree` by name; those stay doors. |
| `enumerate_video_modes.cpp`'s SEH | §B.1.7 puts exceptions last. |
| `rw_vtbl` constants in the seven Graphics and six Video files | B34_RULES §3.3: retired by the class that owns each body, not by a sweep. |
| the nine hand-inlined int64 `ReaderWriter*` slot calls in `animationstate.cpp:329-331` and `vtbatch7_animgroup_particlesystem.cpp:324/616/638/639` | receiver-side work; `IO::Serializer::Serialize(int64_t&)` exists, so these convert with their own receiver, not with Graphics. |
| `standalone/res_load_lock`'s live/dead decision | §5.4 — a carry-over, one body, nothing Video/Graphics. |

## 12. Proposed blocks (class sets; file sets are `B35_FILEMAP.md`'s to prove disjoint)

| block | classes | rough caller load | notes |
|---|---|---|---|
| **A** (owner) | `runtime/types/video/VertexLayout.h` authored; the twelve `*Data` abstract bases; the duplicate-view collapse | runtime-internal + the dx11 76 | Proves B5-1/2/3. Must land before B and D. |
| **B** | `ShaderProgram`, `VertexProgram`, `PixelProgram`, `VertexLayout`, `VideoProgram` | modest | mechanical after one, EXCEPT `VideoProgram`'s parent (§2.1) |
| **C** | `ImageBuffer`, `GeneratedImageBuffer`, `FrameCopyImageBuffer`, `ReflectionImageBuffer`, `RenderState` | modest | one clean file, `imagebuffer_derived.cpp`; crosses the namespace boundary |
| **D** | `Context`, `Viewport`, `ConstantBuffer`, `Device` + nested, `Configuration`, `RenderBuffer`, `IndexBuffer`, `VertexBuffer` | heavy in dx11 and `minimap_paint.cpp` | the big PODs and the lock (B5-6) |
| **E** (owner or senior) | `Geometry`, `MeshInstance`, `EdgeMeshInstance`, `ParticleInstance`, `ParticleSystemInstance`, `Light`, `AmbientLight`, `SpotLight`, `SunLight` | **heaviest** | B5-5, B5-12, B5-17; `deleting_dtor.cpp` and `animation.cpp` are the owner's |
| **F** | `Camera`, `Renderer`, `Scene` | heavy (`gameupdateloop*`, `minimap_paint`) | one commit for Camera+Renderer (B5-4) |
| **G** | `Material`, `MaterialInstance`, `GraphicsMesh`, `EdgeMesh` | moderate | `wave24_lane_gfxmat.cpp` overlaps E's `renderer.cpp` — coordinate |
| **H** | `Particle`, `ParticleSystem`, `Animation`, `AnimationGroup`, `AnimationState`, `AnimationManager`, `AnimationEvent` | moderate | absorbs the wave-lane files; B5-16's slot trap; needs E's `ParticleInstance` first |
| **I** | caller batches by directory: `code/Game/Game` (188 outside-runtime bindings, heaviest four files), then `code/Game/{Component,Player,Dialog,Action,Entity,Tutorial}`, then `engine` (74), then `standalone` (36) | — | after the class blocks the batch names |
| **J** | the dx11 caller batch — 76 literals (B5-14) | — | LAST; every Runtime class it names must be real |

Blocks B and C are parallel after A. D, E parallel after A. F after E. G, H after E.

## 13. NOT VERIFIED — read this before citing anything above

Read from RETAIL BYTES this pass: all 39 vtable addresses and full slot lists; all 39 `sizeof`
values; all 36 `_TypeName`/`_ParentTypeName` literals; the five parent vtables
(`Base`/`External`/`CompiledExternal`/`Internal`/`ExternalList`); every vtable's referencing
functions; the 63 by-value-returning exports; the ctor/dtor export set of every Video and Graphics
class. Read from the TREE this pass: the eight Q1 files in full, all seven
`runtime/types/video/*.h` and all twelve `runtime/types/graphics/*.h` size and member lines, the
Makefile include set, `cutover_manifest.inc`'s 22 V/G rows, the vtable-address grep of §4.

**NOT verified — each is a gap a block must close, not a fact:**

1. **`sizeof(Video::Configuration)` is 0x44 INFERRED**, never measured. `Device.h:240-243` says so
   itself: no body that lane read allocates one, which is why there is no `static_assert`. The
   `Device`/`Configuration` block must find an allocation site or a `MOV EDX` and add the assert.
2. **`Graphics::Animation`'s layout is undocumented.** No `struct Animation`, no size. It has 22
   exports and a non-virtual ctor/dtor at 0x1800fb1d0/0x1800fb290. Block H must author it.
3. **Six more layouts are undocumented** (no `struct`, no `sizeof` assert), though this pass read
   their sizes from the deleting dtor: `Graphics::Light` 0xC0, `AmbientLight` 0xd8, `SpotLight`
   0x168, `AnimationGroup` 0x58, `Material` 0x38, `GraphicsMesh` 0x98, `ParticleSystem` 0x108,
   `Video::VertexLayout` 0x48, `VideoProgram` 0x48. The SIZE is verified; the FIELDS are not.
4. **`Video::ConstantBuffer`'s interior is deliberately opaque** (`ConstantBufferOpaque` is
   `uint8_t[0x48]`; `CompanionData.h:70` names only +0x38 and +0x40). 0x48 is a measured size but
   not a measured *total* — `CompanionData.h` says the ctor's last write is +0x40, so anything
   between +0x41 and +0x47 is unproven.
5. **Whether anything holds `Graphics::Scene` or `Video::Context` by value is NOT established.**
   I found no by-value holder of either; that is an absence of evidence from a grep over
   `runtime/types` + `engine/types` only, not a proof. A block converting `Scene` or `Context`
   must repeat the search over `code/` and `standalone/`.
6. **`ParticleInstance`'s and `Particle`'s slot 6/7/8 identities are INFERRED**, not read:
   Particle's slots 6,7,8 are `_purecall`, and I assigned them `Reconstruct`/`Destruct`/
   `CreateSystem` from `ParticleSystem`'s concrete overrides at the same indices. Sound, but it is
   an inference; `ParticleSystem`'s slot list is the byte-read part.
7. **`Object::External`'s and `CompiledExternal`'s slot-8 NULL.** Both retail arrays are 8 entries
   and External's ninth qword reads 0. I did not establish whether that null is an MSVC artifact
   or a ninth declared-but-unimplemented slot. It does not change step 5 (each Video/Graphics leaf
   introduces `Construct` itself, which is what the real `External.h` already encodes) but it is
   not proved either way.
8. **The `ContextData`/`DeviceData` 20-slot count is bounded above by the next `.rdata` symbol.**
   All 20 entries are `_purecall` and `dx11_seam.h` independently says 20, so I treat it as exact
   — but a 21st slot that happened to be null and to sit under the next symbol would be invisible
   to both readers.
9. **Counts (a)-(h) in §7 are a subagent's regexes**, not my own reads. Units are as labelled;
   (e) in `code/Game` is over a keyword-selected file set (335 files) and so is a plausible-receiver
   count, not a proven-receiver count — the inventory's §5.3 flagged the same measurement as
   over-broad. The subagent's (f) was refined down from 514 to 94 by target verification and is
   still regex-based. §7.1's dx11 numbers ARE my own reads.
10. **`Graphics::Renderer`'s `field_38`** is read by `standalone/scene_watch.h:73` as "draw groups
    used this frame"; `Camera.h:208`'s banner says no ported function touches it, so that reading
    is neither confirmed nor contradicted.
11. **`External`'s +0x2c** is `_capacity` by plausibility only (`External.h:318`) — B.3.3's gap,
    inherited by every Video/Graphics external.
12. **I did not build, run or gate anything**, per the brief. No claim here is backed by an
    execution. `dispatch_slot_map.py --check`, `rt_mint_gen --check` and the cutover gate were not
    run; the `cutover_manifest.py --table` output in §9 was produced by a subagent and I
    cross-checked all 22 rows against `standalone/cutover_manifest.inc` by line number.
13. **No Ghidra renames or annotations were made.** Five read-only Java scripts were written to
    `C:\Users\USPC\ghidra_scripts\` (`VgDump`, `VgDump2`, `VgDump3`, `VgRetslot`, `VgParents`);
    raw output is in the session scratchpad under `block0/`. Re-run them rather than trusting a
    transcription. `run_script_inline` writes `.java` regardless of content and this Ghidra has no
    Python provider, so Jython/PyGhidra scripts fail — write Java.
