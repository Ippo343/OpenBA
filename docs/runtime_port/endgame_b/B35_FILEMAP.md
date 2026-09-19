# B35 file ownership map — Video / Graphics class refactor

Scope: `runtime/src/video/`, `runtime/src/graphics/`, `dx11/src/`,
`runtime/types/video/*.h`, `runtime/types/graphics/*.h`, plus any file
elsewhere in `runtime/src/`, `standalone/`, `engine/`, `dx11/` that
registers or defines a body for a `@Video@@`/`@Graphics@@` mangled name.
All counts below come from a regex scan
(`RegisterRuntimeExport("...",&fn)` / `RegisterRuntimeAddr(0x...,&fn)`,
tolerant of the call split across two lines, which the first cut of the
scan missed) over 923 candidate `.cpp/.h/.inc` files under
`runtime/`, `standalone/`, `engine/`, `dx11/` (build dirs excluded).
Script: `scratchpad/filemap/scan.py` (temp dir, not checked in). Anything
not backed by a grep/script line is marked **INFERRED**.

**Demangling note:** class = second-to-last `@`-separated segment of the
name-chain before `@@`, namespace = last segment. `[byaddr]ClassName` in
the raw data means the row only had a `RegisterRuntimeAddr` whose function
name matched `Video_ClassName_Method` / `RtVideo_ClassName_Method` /
`Graphics_ClassName_Method` / `RtGfx_ClassName_Method` — i.e. inferred from
the function's own naming convention, not from a mangled string on that
line. All `[byaddr]`-only rows are folded into the same class below;
they never introduce a class the export-string data didn't already show,
**except** the dx11-side `*Data` companion classes (`ImageBufferData`,
`DeviceData`, `ContextData`, `RenderStateData`, `VertexLayoutData`,
`VertexProgramData`, `PixelProgramData`, `VideoProgramData`,
`ConstantBufferData`, `IndexBufferData`, `VertexBufferData`,
`RenderBufferData`) which are real, separate dx11-side classes (see §3f).

**Printf-wrapped registrations (perf-build bug shape):** zero found. No
`RegisterRuntimeExport`/`RegisterRuntimeAddr` call in the scope sits as an
argument to `printf`/`TRACE`/`fprintf`/`sprintf`/`LOG*`.

**Known scan limitation (verified against 2 spot-checks, both fixed
inline below):** the demangler takes the second-to-last `@`-segment as
"the class." For a **nested** class (`?ctor@Node@GraphicsMesh@Graphics@@`
= `GraphicsMesh::Node`, not top-level `GraphicsMesh`), this attributes
the row to the outer class, which is directionally right but not
literally a `GraphicsMesh` member — flagged where it occurs
(`animationstate.cpp`). The regex-based per-class counts on `renderer.cpp`
were also off by one for `MaterialInstance` (raw regex saw 2 of the 3
lines because one export spans a longer wrapped line); corrected by
direct read. Given the volume of call sites (≈500 across the tree),
per-file counts elsewhere are regex-exact but not all individually
re-verified by hand; treat single-digit count discrepancies as possible
rather than treat every number as hand-verified.

---

## 1. Per-file table (files with ≥1 registration)

Only files with at least one `Video`/`Graphics`-relevant
`RegisterRuntimeExport`/`RegisterRuntimeAddr` call are listed (62 files).
"Classes (count)" excludes `[byaddr]`-only duplicate counts of an
already-named class; those are folded into the same class's total.

### `runtime/src/video/`

| File | Lines | Export calls | Addr calls | Classes registered (count) |
|---|---:|---:|---:|---|
| batchc_context_setlight.cpp | 200 | 1 | 0 | Context (1) |
| buffers.cpp | 1046 | 28 | 29 | ImageBuffer (13), IndexBuffer (4), VertexBuffer (4), RenderBuffer (2) |
| companion_data.cpp | 520 | 29 | 29 | IndexBuffer (2), VertexBuffer (2), ConstantBuffer (1), Device (2) + 10 `*Data` companion classes by address (see §3f) |
| constant_buffer.cpp | 793 | 5 | 5 | ConstantBuffer (4), Context (1 — `AssignConstants`) |
| context.cpp | 848 | 25 | 25 | Context (25) |
| context_bind.cpp | 895 | 2 | 2 | Context (2) |
| context_ctor.cpp | 410 | 3 | 7 | Context/ImageBuffer/RenderBuffer/"Ctx" by address only — class unknown from export string; **INFERRED** from fn-name prefixes only |
| context_lifecycle.cpp | 621 | 5 | 6 | Context (1) + ConstantBuffer/RenderBuffer/Buffer by address |
| device.cpp | 713 | 10 | 1 | Device (6), RenderBuffer (1), Configuration (2) |
| device_enum_types.cpp | 639 | 9 | 9 | Device (9) |
| device_startup.cpp | 862 | 9 | 9 | Device (9) |
| dynamic_resources.cpp | 587 | 13 | 13 | Device (4), ImageBuffer (2), IndexBuffer (2), VertexBuffer (2), RenderBuffer (3) |
| enumerate_video_modes.cpp | 198 | 0 | 1 | Device — by-address only, **INFERRED** |
| imagebuffer_platform_serialize.cpp | 350 | 1 | 1 | ImageBuffer (1) |
| imagebuffer_vtable.cpp | 531 | 7 | 7 | ImageBuffer (7) — hand-built vtable, see §3e |
| pixelprogram.cpp | 347 | 18 | 0 | PixelProgram (16) — local `struct ArrayHdr` only, no type header |
| renderstate.cpp | 392 | 23 | 0 | RenderState (21) |
| save_image_to_file.cpp | 402 | 1 | 1 | ImageBuffer (1) |
| shader_program_names.cpp | 258 | 2 | 0 | ShaderProgram (2) |
| shaderprogram.cpp | 388 | 16 | 0 | ShaderProgram (14) |
| vertexlayout.cpp | 259 | 9 | 0 | VertexLayout (7) — local `struct VertexLayout`/`VertexElement`, no type header |
| vertexprogram.cpp | 437 | 18 | 0 | VertexProgram (16) — local `struct ArrayHdr` only, no type header |
| video_interfaces_win.cpp | 287 | 0 | 2 | Device — by-address only, **INFERRED** |
| video_resources.cpp | 574 | 6 | 6 | RenderBuffer (1), ImageBuffer (1), Configuration/Device by address |
| video_program.cpp | 215 | 0 | 0 (see text) | VideoProgram — 1 export (`SetupShaders`), see §3a |
| videoprogram.cpp | 215 | 9 | 0 | VideoProgram (7) — local `struct VideoProgram`, no type header |
| closure_batch2b.cpp | 139 | 0 | 2 | `RtClosure2bVideoRegister()`, by-address only, class unknown — **INFERRED none**, treat as OWNER (thin sub-dispatcher of `system/closure_batch2b.cpp`) |
| buffers_win.cpp | 139 | 0 | 0 | none — Win32 recursive-lock helper for `buffers.cpp`'s 4 `Platform{Construct,Destruct}` bodies, no registrations of its own |
| device_adjustdisplay_win.cpp, device_startup_win.cpp, video_mode_sort.cpp, video_resources_win.cpp, shader_constants.inc | — | 0 | 0 | helpers/data tables, no registrations |
| rt_factories_video.cpp / rt_factories_video.inc | 25 / 176 | 0 | 0 | OWNER — factory table, calls `RtFactoryRegisterAll(kRegs,...)`, not per-symbol registration |
| rt_video_port.cpp | 168 | 0 | 0 | OWNER — dispatcher; declares `extern int RtVideo*Register()` and sums their return counts, one line per lane file |

### `runtime/src/graphics/`

| File | Lines | Export calls | Addr calls | Classes registered (count) |
|---|---:|---:|---:|---|
| animation.cpp | 610 | 9 | 2 | ParticleInstance (2) (+7 export-only rows not resolved to a listed class — **INFERRED gap**, likely `Animation`/`AnimationEvent` bodies; re-run with `Animation` added to demangle allowlist before partitioning) |
| animationstate.cpp | 613 | 5 | 0 | GraphicsMesh (1 — actually `GraphicsMesh::Node` copy-ctor, a **nested** class, see caveat below), AnimationState (2), AnimationManager (1), Animation (1) |
| animationstate_transforms.cpp | 1513 | 1 | 0 | MeshInstance (1) |
| batchc_gfx_leaves.cpp | 1122 | 10 | 0 | MaterialInstance (1), SpotLight (1), GraphicsMesh (1), Renderer (1) (+6 rows not attributed — **INFERRED gap**) |
| closure_batch2b.cpp | 244 | 1 | 0 | Geometry (1 — `Geometry::CopyCtor`, `RtClosure2bGraphicsRegister`); also 2 by-address rows for `DrawGroup::CopyCtor` and one more unlabeled (class unknown, **INFERRED**) |
| cull.cpp | 470 | 1 | 0 | Scene (1) |
| edgemesh_vtable.cpp | 576 | 8 | 0 | EdgeMesh (8) — hand-built vtable, see §3e |
| graphics_typeinfo_accessors.cpp | 261 | 8 | 0 | AnimationEvent (2), MaterialInstance (3), Particle (3) |
| graphicsmesh_vtable.cpp | 702 | 9 | 0 | GraphicsMesh (7) — hand-built vtable (`RtGraphicsMeshVtableBatch6Register`), see §3e |
| imagebuffer_derived.cpp | 897 | 25 | 0 | GeneratedImageBuffer (4), FrameCopyImageBuffer (8), ReflectionImageBuffer (9) (+4 unattributed) |
| lights_vtable.cpp | 1016 | 16 | 0 | AmbientLight (5), SpotLight (5), SunLight (2) — hand-built vtable (`RtGraphicsLightsVtableRegister`), see §3e |
| material.cpp | 186 | 8 | 0 | Material (7) |
| mesh.cpp | 2714 | 20 | 1 | EdgeMeshInstance (3), MeshInstance (8), GraphicsMesh (1), EdgeMesh (2) |
| meshinstance_draw.cpp | 532 | 2 | 0 | MeshInstance (1), EdgeMeshInstance (1) |
| renderer.cpp | 665 | 12 | 8 | Camera (4), Renderer (2), MaterialInstance (3 — verified by line read: `MaterialInstance::Ctor`, `SetMaterial`, `AddTexture` at lines 652/656/660; corrects the regex's raw count of 2) |
| scene.cpp | 1376 | 19 | 15 | Geometry (10), Scene (5) |
| sun_camera.cpp | 334 | 1 | 0 | SunLight (1) |
| sunlight.cpp | 474 | 3 | 3 | SunLight (1) |
| vtbatch7_animgroup_particlesystem.cpp | 732 | 17 | 0 | AnimationGroup (8), ParticleSystem (8) — both classes' `struct` defined locally in this file, no type header |
| wave21_lane_gfxbody.cpp | 355 | 3 | 0 | MaterialInstance (1) |
| wave23_lane_gfxbody.cpp | 700 | 4 | 0 | FrameCopyImageBuffer (1), AnimationManager (1) |
| wave24_lane_gfxmat.cpp | 761 | 6 | 0 | MaterialInstance (4), MeshInstance (1), AnimationManager (1) |
| wave27_lane_gfxbig.cpp | 1263 | 1 | 0 | MaterialInstance (1) |
| wave27_lane_gfxpart.cpp | 1274 | 3 | 0 | Particle (1), ParticleInstance (2) |
| wave28_lane_gfxbody.cpp | 939 | 6 | 0 | Material (2), MaterialInstance (2), AnimationState (1) |
| wave28_lane_partsys.cpp | 1093 | 4 | 0 | ParticleSystemInstance (1), ParticleInstance (1), SunLight (2) |
| wave29_lane_gfxbody.cpp | 758 | 3 | 0 | Scene (1), Renderer (2) |
| wave29_lane_slots.cpp | 504 | 2 | 0 | MeshInstance (2) |
| wave9_lane_gfx.cpp | 448 | 3 | 3 | MaterialInstance (1), Renderer (1), `Math::VTransform` (1, cross-namespace helper) |
| wave9_lane_gfxd.cpp | 610 | 6 | 6 | Renderer (2: `Reset`, `AddScene`), Camera (2: copy-ctor, default-ctor) — under a local `rtgfxd::` namespace, missed by the fn-name regex but confirmed by grep of the export strings directly |
| scene_grid.cpp / scene_grid.h | — | 0 | 0 | Scene-adjacent spatial grid helper, no registrations |
| rt_factories_graphics.cpp / .inc | 25 / 338 | 0 | 0 | OWNER — factory table |
| rt_graphics_port.cpp | 353 | 0 | 0 | OWNER — dispatcher; declares/sums 20 `extern int RtGraphics*Register()` lanes |

### `dx11/src/`

None of these files call `RegisterRuntimeExport`/`RegisterRuntimeAddr` —
dx11 is the *consumer* side. It resolves already-registered Runtime
exports via `Dx11ResolveRuntimeImport(...)` / the `Exp<T>("...")` helper
in `exports.cpp`, and implements the platform (`*Data`) companion
objects the Runtime-side classes hold a pointer to.

| File | Lines | Mangled names referenced (Video/Graphics) | Local structs defined |
|---|---:|---|---|
| buffers.cpp | 1025 | `Device::Lock/Unlock`, `VertexBufferData/IndexBufferData/ConstantBufferData` dtors | `RuntimeDeps`, `GpuBufferOwner`, `ConstantBufferOwner`, `PtrArray` |
| contextdata.cpp | 1396 | Context/ContextData exports | `Imports`, `Viewport` (**mirrors `runtime/types/video/Context.h::Viewport`**, see §3f) |
| devicedata.cpp | 2193 | DeviceData ctor, Device exports | `Imports`, `DisplayMode`, `RefreshRate`, `Adapter`, `GraphicsInterface`, `WideStringStorage`, `Dx9Placeholder` — **`DisplayMode`/`RefreshRate`/`Adapter`/`GraphicsInterface` are byte-for-byte name+layout mirrors of `runtime/types/video/Device.h`'s structs, each with its own `static_assert`s duplicating the Runtime-side ones** (see §3f) |
| devicedata_stubs.cpp | 69 | — | none |
| dx11_backend_access.cpp / .h | 32 / 387 | includes `runtime/types` | `ShaderProgramOwner`, `VertexLayoutOwner`, `VertexLayoutElement`, `RenderBufferOwner`, `ImageBufferOwner` |
| exports.cpp | 143 | `DeviceData` ctor, `Device::Lock/Unlock`, `*BufferData` dtors | `ExportImports` |
| imagebufferdata.cpp | 818 | ImageBufferData | `Imports`, `ImageLevel`, `ImageLevelArray`, `SubresArray` |
| renderstatedata.cpp | 561 | RenderStateData | `VideoRenderState`, `RuntimeDeps` |
| shaderprograms.cpp | 254 | ShaderProgram(Data) | none listed |
| smallclasses.cpp | 226 | misc small `*Data` companions | none listed |
| vertexlayoutdata.cpp | 225 | VertexLayoutData | none listed |

### `runtime/types/video/*.h` and `runtime/types/graphics/*.h`

Declaration-only; zero registrations by construction. Struct inventory
is in §3d.

### Files elsewhere that register a `@Video@@`/`@Graphics@@` name

| File | Registers | Note |
|---|---|---|
| `runtime/src/object/external_ptr_serialize.cpp` | `_typeInfo` static-member thunks (`EXTPTR_THUNK`) for `Video::ImageBuffer`, `Graphics::Material`, `Graphics::MaterialInstance`, `Graphics::GraphicsMesh`, `Graphics::AnimationGroup` | Object-serialization infrastructure shared across every namespace, not Video/Graphics-owned code — **OWNER** |
| `runtime/src/system/closure_batch2b.cpp` | `Graphics::MaterialInstance::operator delete` (0x1800028a0), `Video::ImageBuffer::operator delete` (0x18001e990), plus dispatches to `RtClosure2bGraphicsRegister()`/`RtClosure2bVideoRegister()` (the graphics/video `closure_batch2b.cpp` files above) | Cross-namespace operator-delete hub + dispatcher — **OWNER** |
| `runtime/src/collection/array_graphics_mesh.cpp` | Array<T> container instantiated over `Graphics::GraphicsMesh*`/mesh-family pointee types (template mangled name contains `@Video@@`/`@Graphics@@` as a nested type argument, not as the registered class itself) | Container plumbing, not a Graphics class body — **OWNER** |
| `runtime/src/collection/array.cpp`, `runtime/src/collection/set_avl.cpp`, `runtime/src/object/deleting_dtor.cpp`, `runtime/src/object/deleting_dtor_leaves.cpp`, `runtime/src/object/class_operator_delete.cpp` | Comments/vtable-ordinal tables and generic deleting-dtor thunks that *reference* Video/Graphics classes as the pointee of a template or vtable slot | No class-body registration; mentions only — excluded from the partition |
| `standalone/reg_mint_promoted.cpp`, `standalone/rt_export_addrs.inc`, `standalone/rt_data_mint.cpp`, `standalone/rt_data_mint_tables.inc`, `standalone/address_registry.cpp`, `standalone/cutover_manifest.inc`, `standalone/scene_watch.cpp`, `standalone/resolve_standalone.cpp`, `standalone/video_interface.cpp`, `standalone/perf_experimental.cpp` | Address-table / mint-machinery entries and debug hooks that reference Video/Graphics mangled names as data, not registration call sites in the `RegisterRuntime*` sense | **OWNER** — none of these define a class member body |
| `engine/funcs/engine_funcs.h`, `engine/funcs/engine_funcs.cpp` | `fn_*` typedefs / dependency declarations for calling into Video/Graphics exports | **OWNER** — shared engine-func plumbing |
| `runtime/src/engine/*.cpp` (loops.cpp, engine.cpp, engine_lifecycle.cpp, engine_platform.cpp, mainloop_video.cpp, singleton_ctors.cpp) | `ResolveExport(...)` calls into Video/Graphics exports, plus 2 singleton-ctor registrations resolving to Device (by fn-name only) | Call sites / cross-module glue — **OWNER**, not part of the Video/Graphics class-body partition |
| `runtime/src/ui/*.cpp` (dialog*.cpp, element*.cpp, userinterface*.cpp, spritecontext*.cpp, combodialog_ctor.cpp) | `ResolveExport("?Draw@UserInterface@UI@@...AEAVContext@Video@@@Z")` and similar — Context is a *parameter type*, not the registered class | Call sites — excluded; two real registrations do exist (`UI::UserInterface` in userinterface_draw.cpp / userinterface_standalone.cpp, `UI::SpriteContext` in spritecontext.cpp) but those are UI-namespace classes' own members, out of scope |
| `runtime/src/save/saveload*.cpp`, `runtime/src/io/file_stream.cpp`, `runtime/src/io/rt_io_port.cpp`, `runtime/src/application/*.cpp`, `runtime/src/audio/audio_voice_callback_win.cpp`, `runtime/src/math/transform.cpp` | Reference Video/Graphics types in comments/params/serialization glue; `saveload_draw.cpp` has one real `Save::SaveLoad` registration (not Video/Graphics) | Excluded — no Video/Graphics class-body registration found |

---

## 2. Per-class inverse table

All Video and Graphics classes found registering at least one member, with
the files that register them. **Bold** = split across >1 file.

### Video

| Class | Total regs | Files (count each) |
|---|---:|---|
| **Context** | 30 | context.cpp (25), context_bind.cpp (2), batchc_context_setlight.cpp (1), constant_buffer.cpp (1), context_lifecycle.cpp (1) |
| **Device** | 30 | device_enum_types.cpp (9), device_startup.cpp (9), device.cpp (6), dynamic_resources.cpp (4), companion_data.cpp (2) |
| **ImageBuffer** | 25 | buffers.cpp (13), imagebuffer_vtable.cpp (7), dynamic_resources.cpp (2), imagebuffer_platform_serialize.cpp (1), save_image_to_file.cpp (1), video_resources.cpp (1) |
| RenderState | 21 | renderstate.cpp (21) |
| PixelProgram | 16 | pixelprogram.cpp (16) |
| **ShaderProgram** | 16 | shaderprogram.cpp (14), shader_program_names.cpp (2) |
| VertexProgram | 16 | vertexprogram.cpp (16) |
| **IndexBuffer** | 8 | buffers.cpp (4), companion_data.cpp (2), dynamic_resources.cpp (2) |
| **VertexBuffer** | 8 | buffers.cpp (4), companion_data.cpp (2), dynamic_resources.cpp (2) |
| VertexLayout | 7 | vertexlayout.cpp (7) |
| VideoProgram | 7 | videoprogram.cpp (7) — plus 1 unregistered-by-export member (`SetupShaders`) in `video_program.cpp`, see §3a |
| **RenderBuffer** | 7 | dynamic_resources.cpp (3), buffers.cpp (2), device.cpp (1), video_resources.cpp (1) |
| **ConstantBuffer** | 5 | constant_buffer.cpp (4), companion_data.cpp (1) |
| Configuration | 2 | device.cpp (2) |

### Graphics

| Class | Total regs | Files (count each) |
|---|---:|---|
| **MaterialInstance** | 15 | wave24_lane_gfxmat.cpp (4), graphics_typeinfo_accessors.cpp (3), renderer.cpp (2), wave28_lane_gfxbody.cpp (2), batchc_gfx_leaves.cpp (1), wave21_lane_gfxbody.cpp (1), wave27_lane_gfxbig.cpp (1), wave9_lane_gfx.cpp (1) |
| **MeshInstance** | 13 | mesh.cpp (8), wave29_lane_slots.cpp (2), animationstate_transforms.cpp (1), meshinstance_draw.cpp (1), wave24_lane_gfxmat.cpp (1) |
| **GraphicsMesh** | 10 | graphicsmesh_vtable.cpp (7), animationstate.cpp (1), batchc_gfx_leaves.cpp (1), mesh.cpp (1) |
| **EdgeMesh** | 10 | edgemesh_vtable.cpp (8), mesh.cpp (2) |
| Geometry | 10 | scene.cpp (10) |
| **Material** | 9 | material.cpp (7), wave28_lane_gfxbody.cpp (2) |
| **ReflectionImageBuffer** | 9 | imagebuffer_derived.cpp (9) |
| AnimationGroup | 8 | vtbatch7_animgroup_particlesystem.cpp (8) |
| ParticleSystem | 8 | vtbatch7_animgroup_particlesystem.cpp (8) |
| **FrameCopyImageBuffer** | 9 | imagebuffer_derived.cpp (8), wave23_lane_gfxbody.cpp (1) |
| **Scene** | 7 | scene.cpp (5), wave29_lane_gfxbody.cpp (1), cull.cpp (1) |
| **Renderer** | 6 | renderer.cpp (2), wave29_lane_gfxbody.cpp (2), batchc_gfx_leaves.cpp (1), wave9_lane_gfx.cpp (1) |
| **SunLight** | 6 | lights_vtable.cpp (2), wave28_lane_partsys.cpp (2), sun_camera.cpp (1), sunlight.cpp (1) |
| **SpotLight** | 6 | lights_vtable.cpp (5), batchc_gfx_leaves.cpp (1) |
| AmbientLight | 5 | lights_vtable.cpp (5) |
| **ParticleInstance** | 5 | wave27_lane_gfxpart.cpp (2), animation.cpp (2), wave28_lane_partsys.cpp (1) |
| Camera | 4 | renderer.cpp (4) — plus 2 more (copy-ctor/default-ctor) in wave9_lane_gfxd.cpp under `rtgfxd::` namespace |
| GeneratedImageBuffer | 4 | imagebuffer_derived.cpp (4) |
| **EdgeMeshInstance** | 4 | mesh.cpp (3), meshinstance_draw.cpp (1) |
| **Particle** | 4 | graphics_typeinfo_accessors.cpp (3), wave27_lane_gfxpart.cpp (1) |
| **AnimationState** | 3 | animationstate.cpp (2), wave28_lane_gfxbody.cpp (1) |
| **AnimationManager** | 3 | animationstate.cpp (1), wave23_lane_gfxbody.cpp (1), wave24_lane_gfxmat.cpp (1) |
| AnimationEvent | 2 | graphics_typeinfo_accessors.cpp (2) |
| Animation | 1 | animationstate.cpp (1) |
| ParticleSystemInstance | 1 | wave28_lane_partsys.cpp (1) |

**Classes whose members are split over more than one file:** every
**bold** row above — 14 of 14 Video classes with >1 registration site
have exactly one single-file class except RenderState, PixelProgram,
VertexProgram, VertexLayout, VideoProgram, Configuration, AmbientLight,
AnimationGroup, ParticleSystem, Geometry, GeneratedImageBuffer,
AnimationEvent, Animation, ParticleSystemInstance (14 single-file
classes total); the remaining 20 classes (11 Video, 16 Graphics, with
overlap on the total-30 line) are split.

**Files serving more than one class:** `buffers.cpp` (4: ImageBuffer,
IndexBuffer, VertexBuffer, RenderBuffer), `companion_data.cpp` (4:
IndexBuffer, VertexBuffer, ConstantBuffer, Device — plus the 10 `*Data`
companion classes by address), `dynamic_resources.cpp` (4: Device,
ImageBuffer, IndexBuffer, VertexBuffer, RenderBuffer — actually 5),
`device.cpp` (3: Device, RenderBuffer, Configuration),
`mesh.cpp` (4: EdgeMeshInstance, MeshInstance, GraphicsMesh, EdgeMesh),
`batchc_gfx_leaves.cpp` (4: MaterialInstance, SpotLight, GraphicsMesh,
Renderer), `lights_vtable.cpp` (3: AmbientLight, SpotLight, SunLight),
`wave24_lane_gfxmat.cpp` (3: MaterialInstance, MeshInstance,
AnimationManager), `wave28_lane_gfxbody.cpp` (3: Material,
MaterialInstance, AnimationState), `wave28_lane_partsys.cpp` (3:
ParticleSystemInstance, ParticleInstance, SunLight), `wave9_lane_gfx.cpp`
(3: MaterialInstance, Renderer, `Math::VTransform`), `renderer.cpp` (3:
Camera, Renderer, MaterialInstance), `context_lifecycle.cpp` (Context +
3 by-address-only buffer classes), `context_ctor.cpp` (Context,
ImageBuffer, RenderBuffer by address only), `wave9_lane_gfxd.cpp`
(Renderer, Camera), `imagebuffer_derived.cpp` (3: GeneratedImageBuffer,
FrameCopyImageBuffer, ReflectionImageBuffer), `animationstate.cpp` (4:
GraphicsMesh, AnimationState, AnimationManager, Animation).

---

## 3. Specific questions

### (a) `videoprogram.cpp` vs `video_program.cpp`

No overlap. `videoprogram.cpp` (215 lines) owns `VideoProgram`'s object
lifecycle: `Ctor`, `Dtor`, `Construct`, `Destruct`, `Reconstruct`,
`GetFileVersion`, `Serialize`, `Type`, `TypeName` (7 `RegisterRuntime*`
calls covering 9 method bodies) and defines the local `struct
VideoProgram` used by both files. `video_program.cpp` (215 lines) owns
exactly one exported member, `VideoProgram::SetupShaders`
(`Video_VideoProgram_SetupShaders`), plus unexported local helpers
(`struct VideoProgramPartial`, `struct Touched`) that snapshot/restore
Context state around the call — it was split out because `SetupShaders`
is large and touches `Video::Context` internals, not because it's a
different class. Verdict: **same class, disjoint method sets, safe to
assign to the same family (V2, program/shader).**

### (b) `constant_buffer.cpp` vs `buffers.cpp` vs `buffers_win.cpp`

No member overlap either.
- `constant_buffer.cpp` owns `Video::ConstantBuffer` (`BeginConstants`,
  `AddConstant`, `EndConstants`, `PlatformCreate` — 4 regs) plus one
  `Video::Context` member, `AssignConstants` (1 reg, because it iterates
  the seven constant slots `ConstantBuffer` fills), and an internal
  `ConstEntryArray` helper (not itself a registered class).
- `buffers.cpp` owns `Video::ImageBuffer`, `Video::IndexBuffer`,
  `Video::VertexBuffer`, `Video::RenderBuffer` — the four "buffer" family
  classes' `Ctor`/`Create`/`Set`/`PlatformConstruct`/`PlatformDestruct`
  members (28 exports / 29 addrs total). Does not touch `ConstantBuffer`.
- `buffers_win.cpp` (139 lines) registers **nothing**. It's a Win32-only
  helper providing the recursive `Device` critical-section lock/unlock
  used inline by the four `Platform{Construct,Destruct}` bodies in
  `buffers.cpp` (kept out of `buffers.cpp` per PLAN.md §4.1's rule that
  `runtime/src/` stays free of `<windows.h>` outside a `_win` twin).

Verdict: `constant_buffer.cpp` has one foreign-class touch
(`Context::AssignConstants`) that a disjoint partition must account for
— either move that one function or accept `constant_buffer.cpp` needing
read access to `Context`'s definition.

### (c) The eleven `waveNN_lane_gfx*.cpp` files, `vtbatch7_*`, `batchc_*`, `closure_batch2b.cpp` (both dirs)

| File | Classes |
|---|---|
| wave9_lane_gfx.cpp | MaterialInstance, Renderer, `Math::VTransform` (cross-namespace) |
| wave9_lane_gfxd.cpp | Renderer, Camera |
| wave21_lane_gfxbody.cpp | MaterialInstance |
| wave23_lane_gfxbody.cpp | FrameCopyImageBuffer, AnimationManager |
| wave24_lane_gfxmat.cpp | MaterialInstance, MeshInstance, AnimationManager |
| wave27_lane_gfxbig.cpp | MaterialInstance |
| wave27_lane_gfxpart.cpp | Particle, ParticleInstance |
| wave28_lane_gfxbody.cpp | Material, MaterialInstance, AnimationState |
| wave28_lane_partsys.cpp | ParticleSystemInstance, ParticleInstance, SunLight |
| wave29_lane_gfxbody.cpp | Scene, Renderer |
| wave29_lane_slots.cpp | MeshInstance |
| vtbatch7_animgroup_particlesystem.cpp | AnimationGroup, ParticleSystem (both classes' structs defined locally in this file) |
| batchc_gfx_leaves.cpp | MaterialInstance, SpotLight, GraphicsMesh, Renderer |
| batchc_context_setlight.cpp (video) | Context |
| closure_batch2b.cpp (graphics) | Geometry (+ `DrawGroup` copy-ctor, by address) |
| closure_batch2b.cpp (video) | by-address only, 2 addrs, class not resolvable from the export string on this line — **INFERRED: none confidently attributable**; treat as OWNER-adjacent (thin per-namespace sub-registrant called from `system/closure_batch2b.cpp`) |

None of these 14 "wave"/"batch" files is single-class; every one of them
straddles at least 2 of the proposed V/G families below (they were
written as historical "lanes" that grabbed whatever functions were
reachable that session, per DIRECTION.md's no-more-lanes rule — this is
exactly the fragmentation the refactor is meant to undo).

### (d) Type headers: which class, which namespace spelling, which classes have none

| Header | Structs declared | Namespace spelling used |
|---|---|---|
| `runtime/types/video/Buffers.h` | `IndexBuffer`, `VertexBuffer` | bare (no namespace wrapper; comments say `Video`) |
| `runtime/types/video/CompanionData.h` | `CompanionData`, `ConstantBuffer` | bare |
| `runtime/types/video/Context.h` | `Viewport`, `Vector4Raw`, `MatrixRaw`, `RenderBufferPrefix`, `ConstantBufferOpaque`, `Context` | bare |
| `runtime/types/video/Device.h` | `RefreshRate`, `DisplayMode`, `Adapter`, `GraphicsInterface`, `InterfaceSlot`, `Device`, `Configuration`, `RenderBuffer` | bare |
| `runtime/types/video/ImageBuffer.h` | `ImageBuffer`, `ImageBufferData` | bare |
| `runtime/types/video/RenderState.h` | `AlphaBlend`, `Depth`, `Stencil`, `RenderState` | bare |
| `runtime/types/video/ShaderProgram.h` | `ShaderParameter`, `ShaderProgram`, `ShaderProgramWithData` | bare |
| `runtime/types/graphics/Animation.h` | `AnimationEvent`, `AnimationManager` | bare |
| `runtime/types/graphics/AnimationManager.h` | `AnimStateRec`, `AnimInstanceRec`, `AnimationManagerView` | bare |
| `runtime/types/graphics/AnimationState.h` | `AnimationLayer`, `AnimationClipPrefix`, `AnimationState`, `InterpolatedTransform` | bare |
| `runtime/types/graphics/Camera.h` | `Camera`, `RendererDrawable`, `RendererDrawItem`, `RendererDrawGroup`, `RendererSceneEntry`, `Renderer` | bare |
| `runtime/types/graphics/FrameCopyImageBuffer.h` | `FrameCopyImageBuffer` | bare |
| `runtime/types/graphics/GeneratedImageBuffer.h` | `GeneratedImageBuffer`, `ReflectionImageBuffer` | bare |
| `runtime/types/graphics/Geometry.h` | `Geometry` | bare |
| `runtime/types/graphics/Material.h` | `MaterialTexture`, `MaterialBinding`, `MaterialParameterSet`, `MaterialConstant`, `MaterialInstance` | bare |
| `runtime/types/graphics/Mesh.h` | `ExternalPtr`, `InterpolatedTransform`, `MeshAnimationState`, `MeshAnimationLayer`, `MeshInstance`, `MeshNode`, `GraphicsMeshView`, `TriangleVertex`, `EdgeVertex`, `VideoBufferHeader`, `EdgeMesh`, `EdgeMeshInstance` | bare |
| `runtime/types/graphics/Particle.h` | `Particle`, `ParticleBoundsRecord`, `ParticleResource`, `GeometryBase`, `ParticleInstance`, `ParticleSystemInstance` | bare |
| `runtime/types/graphics/Scene.h` | `SceneNode`, `Scene` | bare |
| `runtime/types/graphics/SunLight.h` | `SunLight` | bare |

**Correction from an initial (wrong) pass:** the struct bodies
themselves are declared at file scope with plain names, but every
header does wrap accessor/helper code in a C++ namespace, and the
spelling is **not consistent**:
- `RtVideo` — `CompanionData.h`, `Context.h`, `Device.h`
- `rt::video` (nested) — `RenderState.h`, `ShaderProgram.h` (which also
  has two more nested namespaces, `devicedata_vtbl` and
  `programdata_vtbl`, for vtable-pointer tables)
- `RtGraphics` — every graphics header (`Animation.h`,
  `AnimationManager.h` — plus its own `anim_clip`/`anim_group` nested
  namespaces —, `AnimationState.h`, `Camera.h`, `FrameCopyImageBuffer.h`,
  `GeneratedImageBuffer.h`, `Geometry.h`, `Material.h`, `Mesh.h`,
  `Particle.h`, `Scene.h`, `SunLight.h`)

So Video is split `RtVideo` vs `rt::video` depending on the file
(3 headers vs 2), while every Graphics header agrees on `RtGraphics`.
`Buffers.h` and `ImageBuffer.h` (video) declare their structs with no
namespace wrapper at all (verified: neither file has a `namespace`
line) — a fourth spelling, "none". A disjoint-file partition that also
normalizes namespace usage will need to pick one spelling per family and
touch `Buffers.h`/`ImageBuffer.h`/`RenderState.h`/`ShaderProgram.h` to
converge on it.

**Classes registering members but with NO type header** (struct is
either local to the one `.cpp` that owns it, or doesn't exist at all as
a named layout):
- `Video::VertexLayout` — local `struct VertexLayout`/`VertexElement` in `vertexlayout.cpp`
- `Video::VideoProgram` — local `struct VideoProgram` in `videoprogram.cpp`
- `Video::PixelProgram` — no struct anywhere; only a local `struct ArrayHdr` helper in `pixelprogram.cpp` (grep confirms no `struct PixelProgram` in the tree)
- `Video::VertexProgram` — same as PixelProgram, no struct exists (only `ArrayHdr`)
- `Graphics::GraphicsMesh` — no `struct GraphicsMesh` exists anywhere; `Mesh.h` only has `GraphicsMeshView` (a partial/opaque view struct); `graphicsmesh_vtable.cpp` uses offset comments instead of a struct, explicitly noting a struct there "would duplicate `array_graphics_mesh.cpp`'s"
- `Graphics::AmbientLight`, `Graphics::SpotLight` — no struct anywhere; only `SunLight.h` exists for the lights family; `lights_vtable.cpp` has an anonymous local `struct { void* light; int32_t tag; int32_t _pad; } pair;` but nothing named `AmbientLight`/`SpotLight`
- `Graphics::AnimationGroup`, `Graphics::ParticleSystem` — both local `struct`s in `vtbatch7_animgroup_particlesystem.cpp`, no header

### (e) Hand-built vtable arrays / `Register*Vtable` functions

| File:line | Function | Class |
|---|---|---|
| `runtime/src/video/imagebuffer_vtable.cpp:492` | `RtVideoImageBufferVtableRegister()` | Video::ImageBuffer |
| `runtime/src/graphics/edgemesh_vtable.cpp:540` | `RtGraphicsEdgeMeshVtableRegister()` | Graphics::EdgeMesh |
| `runtime/src/graphics/graphicsmesh_vtable.cpp:659` | `RtGraphicsMeshVtableBatch6Register()` | Graphics::GraphicsMesh |
| `runtime/src/graphics/lights_vtable.cpp:940` | `RtGraphicsLightsVtableRegister()` | Graphics::AmbientLight, Graphics::SpotLight, Graphics::SunLight (one function, three classes — a fifth split point beyond the per-class table above: this file's vtable registrant itself straddles 3 classes) |

### (f) dx11/src: runtime/types includes and local mirror structs

**Files including `runtime/types`:** `dx11/src/contextdata.cpp`,
`dx11/src/devicedata.cpp`, `dx11/src/dx11_backend_access.h` (grep of
`#include.*runtime[\\/]types` across `dx11/src/*`; the other 9 dx11
files do not include `runtime/types` at all — they only reach the
Runtime side through `Dx11ResolveRuntimeImport`/exported function
pointers, never a shared struct).

**Local mirror structs of Runtime-side objects** (name AND layout
duplicated, each with its own `static_assert`, independently of the
Runtime-side header):
- `dx11/src/devicedata.cpp:442` `struct DisplayMode` — mirrors `runtime/types/video/Device.h:59 struct DisplayMode`
- `dx11/src/devicedata.cpp:451` `struct RefreshRate` — mirrors `runtime/types/video/Device.h:43 struct RefreshRate`
- `dx11/src/devicedata.cpp:469` `struct Adapter` — mirrors `runtime/types/video/Device.h:81 struct Adapter`
- `dx11/src/devicedata.cpp:491` `struct GraphicsInterface` — mirrors `runtime/types/video/Device.h:115 struct GraphicsInterface`
- `dx11/src/contextdata.cpp:425` `struct Viewport` — mirrors `runtime/types/video/Context.h:165 struct Viewport`

These five are genuine name-for-name duplicates verified by reading both
sides (matching field names, offsets, and `static_assert` sizes) — a
disjoint-file partition that changes `Device.h`/`Context.h` layout must
also touch these five structs in dx11, which is outside the
`runtime/src/` and `runtime/types/` tree the partition otherwise covers.
Every other dx11 local struct listed in §1's dx11 table (`RuntimeDeps`,
`GpuBufferOwner`, `ConstantBufferOwner`, `PtrArray`, `Imports` ×4,
`ShaderProgramOwner`, `VertexLayoutOwner`, `VertexLayoutElement`,
`RenderBufferOwner`, `ImageBufferOwner`, `ImageLevel`,
`ImageLevelArray`, `SubresArray`, `VideoRenderState`,
`Dx9Placeholder`, `WideStringStorage`, `ExportImports`) is dx11-only —
no Runtime-side counterpart with the same name — and is **not** a
mirror; it's the DX11 backend's own private state.

---

## 4. Proposed partition into disjoint file sets

**A fully disjoint partition does NOT exist as the code is currently
laid out.** Too many files register members of ≥2 classes that the
family boundaries below would place in different families (see the
"files serving more than one class" list in §2, and every wave/batch
file in §3c). Below is the closest achievable split, naming every file
that needs a line-range split or an explicit shared-file exception.

### V1 — Video buffers + `*Data` companions
`buffers.cpp` (ImageBuffer/IndexBuffer/VertexBuffer/RenderBuffer bodies),
`buffers_win.cpp` (Win32 lock helper, no registrations — follows
buffers.cpp), `companion_data.cpp` (12-class `CompanionData`/
`ConstantBuffer` + the 10 `*Data` vtable stamps — **shared with V2/V3**,
see below), `constant_buffer.cpp` (**needs split**: `ConstantBuffer`
members stay in V1; the one `Context::AssignConstants` function at
lines 783–786 belongs to V3), `imagebuffer_vtable.cpp`,
`imagebuffer_platform_serialize.cpp`, `save_image_to_file.cpp`,
`dynamic_resources.cpp` (**needs split**: `ImageBuffer`/`IndexBuffer`/
`VertexBuffer`/`RenderBuffer` sections stay V1, its `Device` section —
4 regs — belongs to V3), `video_resources.cpp` (**needs split** the same
way: `ImageBuffer`/`RenderBuffer` → V1, `Device`/`Configuration` → V3),
`video_resources_win.cpp`, `video_mode_sort.cpp`,
`enumerate_video_modes.cpp`, `context_ctor.cpp` (**by-address only,
straddles Context/ImageBuffer/RenderBuffer — needs manual re-attribution
before it can be split**), dx11: `buffers.cpp`, `imagebufferdata.cpp`.

### V2 — Video program/shader/VertexLayout family
`shaderprogram.cpp`, `shader_program_names.cpp`, `pixelprogram.cpp`,
`vertexprogram.cpp`, `vertexlayout.cpp`, `videoprogram.cpp`,
`video_program.cpp`, `shader_constants.inc`. No cross-class overlap
found inside this set — **this family alone is already disjoint from
V1/V3**. dx11: `shaderprograms.cpp`, `vertexlayoutdata.cpp`.

### V3 — Video Context/Device/RenderState/ImageBuffer-lifecycle
`context.cpp`, `context_bind.cpp`, `context_lifecycle.cpp`
(**by-address-only rows besides its 1 real `Context` export — same
re-attribution caveat as context_ctor.cpp**), `batchc_context_setlight.cpp`,
`device.cpp`, `device_enum_types.cpp`, `device_startup.cpp`,
`device_startup_win.cpp`, `device_adjustdisplay_win.cpp`,
`video_interfaces_win.cpp`, `renderstate.cpp`. Plus the `Context`
fragment of `constant_buffer.cpp` and the `Device` fragments of
`dynamic_resources.cpp`/`video_resources.cpp` noted above. dx11:
`contextdata.cpp`, `devicedata.cpp`, `devicedata_stubs.cpp`,
`renderstatedata.cpp`, `exports.cpp`, `dx11_backend_access.cpp/.h`,
`smallclasses.cpp`.

### G1 — Graphics mesh/geometry/scene/camera/cull
`mesh.cpp`, `meshinstance_draw.cpp`, `edgemesh_vtable.cpp`,
`graphicsmesh_vtable.cpp`, `scene.cpp`, `scene_grid.cpp/.h`,
`cull.cpp`, `renderer.cpp` (**straddles G1/G2**: `Camera`/`Renderer`
sections → G1, `MaterialInstance` section (lines with
`RtGraphics_MaterialInstance_*`, 2 regs) → G2), `wave9_lane_gfxd.cpp`
(Renderer/Camera, clean fit here), `wave29_lane_gfxbody.cpp`
(Scene/Renderer, clean fit).

### G2 — Graphics material/animation/particle/light/derived imagebuffers
`material.cpp`, `imagebuffer_derived.cpp`, `animation.cpp`,
`animationstate.cpp` (**straddles G1/G2**: its 1 `GraphicsMesh` reg
belongs to G1, the other 3 — AnimationState/AnimationManager/Animation —
stay G2), `animationstate_transforms.cpp` (**its `MeshInstance` reg is
a G1 class** — split point), `graphics_typeinfo_accessors.cpp`,
`lights_vtable.cpp`, `sun_camera.cpp`, `sunlight.cpp`,
`vtbatch7_animgroup_particlesystem.cpp`.

**Every one of the 11 wave*/batch* lane files below is a shared-file
exception; none can be assigned to a single family without a manual
line-level split first:**
- `wave9_lane_gfx.cpp` — G2 (MaterialInstance) + G1 (Renderer) + OWNER (`Math::VTransform`)
- `wave21_lane_gfxbody.cpp` — G2 only (MaterialInstance) — clean, assign to G2
- `wave23_lane_gfxbody.cpp` — G2 only (FrameCopyImageBuffer, AnimationManager) — clean, assign to G2
- `wave24_lane_gfxmat.cpp` — G2 (MaterialInstance, AnimationManager) + G1 (MeshInstance, 1 reg)
- `wave27_lane_gfxbig.cpp` — G2 only (MaterialInstance) — clean
- `wave27_lane_gfxpart.cpp` — G2 only (Particle, ParticleInstance) — clean
- `wave28_lane_gfxbody.cpp` — G2 only (Material, MaterialInstance, AnimationState) — clean
- `wave28_lane_partsys.cpp` — G2 only (ParticleSystemInstance, ParticleInstance, SunLight) — clean
- `wave29_lane_slots.cpp` — G1 only (MeshInstance) — clean, assign to G1
- `batchc_gfx_leaves.cpp` — G1 (GraphicsMesh, Renderer) + G2 (MaterialInstance, SpotLight)
- `closure_batch2b.cpp` (graphics) — G1 (Geometry, DrawGroup) — clean, assign to G1

Net: 6 of the 11 wave/batch files are actually clean once checked
individually; `wave9_lane_gfx.cpp`, `wave24_lane_gfxmat.cpp`,
`batchc_gfx_leaves.cpp` are the true 3-way straddlers needing a
line-level split before parallel agents can each own a disjoint file.

### OWNER
`rt_video_port.cpp`, `rt_graphics_port.cpp`, `rt_factories_video.cpp`/
`.inc`, `rt_factories_graphics.cpp`/`.inc`, `runtime/src/graphics/
closure_batch2b.cpp`'s address-only tail two rows / `runtime/src/video/
closure_batch2b.cpp` (both — class unresolved, thin dispatchers),
`runtime/src/system/closure_batch2b.cpp`, `runtime/src/object/
external_ptr_serialize.cpp`, `runtime/src/object/deleting_dtor.cpp` +
`deleting_dtor_leaves.cpp`, `runtime/src/object/
class_operator_delete.cpp`, `runtime/src/collection/array.cpp` +
`array_graphics_mesh.cpp` + `set_avl.cpp`, `engine/funcs/
engine_funcs.h/.cpp`, all of `runtime/src/engine/*.cpp` that call
`ResolveExport` into Video/Graphics, all of `standalone/*` reg/mint/
address files, `runtime/src/ui/*.cpp` (Context/UI cross-calls),
`runtime/src/save/*.cpp`, `runtime/src/io/*`, `runtime/src/
application/*`, `runtime/src/audio/audio_voice_callback_win.cpp`,
`runtime/src/math/transform.cpp`. None of these define a Video/Graphics
class's own member body; they call into or serialize/allocate around
one.

### Bottom line

A clean disjoint split exists for **V2** outright, and for most of
**V1**/**V3**/**G1**/**G2** once four named cross-class fragments are
manually re-attributed (`constant_buffer.cpp`'s `AssignConstants`,
`dynamic_resources.cpp` and `video_resources.cpp`'s `Device`/
`Configuration` sections, `renderer.cpp`'s `MaterialInstance` section,
`animationstate.cpp`'s `GraphicsMesh` reg, `animationstate_transforms.cpp`'s
`MeshInstance` reg) and three wave/batch files are line-split
(`wave9_lane_gfx.cpp`, `wave24_lane_gfxmat.cpp`, `batchc_gfx_leaves.cpp`).
`context_ctor.cpp` and `context_lifecycle.cpp`'s by-address-only rows
need their class re-confirmed against the retail addresses (not just
the fn-name heuristic) before they can be assigned with confidence.
`vertexlayout.cpp`/`videoprogram.cpp`/`vertexprogram.cpp`/
`pixelprogram.cpp`/`AnimationGroup`+`ParticleSystem`'s host file/
`GraphicsMesh`/`AmbientLight`/`SpotLight` all lack a `runtime/types/`
header and would need one written (or an explicit decision not to)
before their family's agent starts.

---

## 5. Unit tests touching Video/Graphics

None. `tests/unit/array_test.cpp` and `tests/unit/list_test.cpp` test
the generic container templates (which Video/Graphics types happen to
instantiate) with no Video/Graphics-specific assertions; the only hit
is a comment in `list_test.cpp:439` noting `Graphics::RendererDrawItem`
and `IO::Mod` as example instantiating types. `tests/unit/
rt_port_stub.cpp` has no Video/Graphics references at all. No dedicated
Video or Graphics unit test file exists in `tests/unit/`.
