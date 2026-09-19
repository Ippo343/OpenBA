# ENDGAME B.3 step 5 — `Video` and `Graphics` design inventory (read-only, 2026-09-19)

> **A MAP, NOT EVIDENCE.** Written by a read-only sonnet pass at the B.3.4 close; NOTHING in it was
> verified by the step coordinator. Its own last section lists what it verified by opening a file and what
> it inferred from a name. Step 5's question pass (`NEXT_SESSION.md` §2b) re-derives every structural claim
> from the RETAIL DLL before a rule sheet cites it — B.3.4's two inventories each needed nine-plus corrections.

Scope: namespaces `Video` and `Graphics` per `docs/runtime_port/ENDGAME.md` §B.3 step 5
("`Video` and `Graphics` (dx11/ stays the plugin, implemented in-tree)"). Format follows
`docs/runtime_port/endgame_b/IO_INVENTORY.md` / `IO_CALLER_INVENTORY.md`. Read
`B34_RULES.md` §4 (vtables/mint rows/deleting dtors) and §6.5 (caller-surface sizing
rules) first; this document assumes those rules and does not restate them except where
Video/Graphics disagrees. No tree file was edited to produce this. Every number below is
a grep/wc run against the live tree at `C:\code\GIT\untitled_rev_eng_project`
(`standalone/build*/`, `tests/unit/build/` not excluded explicitly this pass unless noted
— none of the counts below hit those paths since they are source-tree scoped). Anything
not read directly is marked NOT VERIFIED / INFERRED.

This is a MAP, not evidence: it tells the step-5 planner where to look, not the final
per-class design (that is what B35_RULES.md-equivalent will need to do the way
`B34_RULES.md` did for IO).

---

## 1. Class list

### 1.1 `Video` — `runtime/types/video/*.h` (7 headers, 1,341 lines, VERIFIED read)

| header | lines | static_assert | class(es) declared |
|---|---|---|---|
| `Buffers.h` | 65 | 12 | `VertexBuffer`, `IndexBuffer`, `RenderBuffer`, `ConstantBuffer` (POD family; VERIFIED by header read) |
| `CompanionData.h` | 78 | 5 | the `*Data` companion-object shapes dx11/ owns (`VertexBufferData` etc. layout prefix) |
| `Context.h` | 414 | 61 | `Video::Context` (the big one — `Viewport`, `Vector4Raw`, `MatrixRaw`, `RenderBufferPrefix`, `ConstantBufferOpaque` helper structs plus `Context` itself, ~0x1f80+ bytes, holds `ConstantBufferOpaque _materialConstants/_pixelConstants/_globalConstants` and `MatrixRaw`/`Vector4Raw` fields **by value** — see §5 hazards) |
| `Device.h` | 333 | 86 | `Video::Device` (largest static_assert count in the namespace — big POD) |
| `ImageBuffer.h` | 157 | 22 | `Video::ImageBuffer` |
| `RenderState.h` | 129 | 15 | `Video::RenderState` (+ `Stencil`/`Depth`/`AlphaBlend` per `runtime/src/video/README.md`) |
| `ShaderProgram.h` | 135 | 9 | `Video::ShaderProgram` (abstract base — Serialize + `PlatformSerialize`'s three device-interface arms + 9 accessors, per README) |

Video vtable classes NOT yet in a dedicated `runtime/types/video/*.h` (their layout is
presumably folded into `Buffers.h`/`Device.h`/`CompanionData.h` or is undocumented —
NOT VERIFIED which): `VertexLayout`, `VertexProgram`, `PixelProgram`, `VideoProgram`
(these four DO have `.cpp` bodies — see §1.3 file table below — and DO have `?_typeInfo@`
mint rows, §1.4).

### 1.2 `Graphics` — `runtime/types/graphics/*.h` (10 headers, 1,798 lines, VERIFIED read)

| header | lines | static_assert | class(es) declared |
|---|---|---|---|
| `Animation.h` | 138 | 8 | `Graphics::Animation`/`AnimationEvent`(?) — NOT VERIFIED exact class split, header not fully read line-by-line this pass |
| `AnimationManager.h` | 183 | 11 | `Graphics::AnimationManager` (has a `_singleton` per the cutover-manifest hit `?_singleton@AnimationManager@Graphics@@0PEAV12@EA`, HARMFUL/DLL-PRESENT storage row) |
| `AnimationState.h` | 270 | 26 | `Graphics::AnimationState` (highest static_assert count after Device/Context) |
| `Camera.h` | 216 | 27 | `Graphics::Camera` |
| `FrameCopyImageBuffer.h` | 92 | 5 | `Graphics::FrameCopyImageBuffer` |
| `GeneratedImageBuffer.h` | 115 | 10 | `Graphics::GeneratedImageBuffer` |
| `Geometry.h` | 175 | 14 | `Graphics::Geometry` |
| `Material.h` | 118 | 13 | `Graphics::Material`, `MaterialInstance` (both have separate vtable rows, §1.4) |
| `Mesh.h` | 392 | 41 | `Graphics::GraphicsMesh`, `MeshInstance`, `EdgeMesh`, `EdgeMeshInstance` (biggest header, likely holds the whole mesh family — NOT individually confirmed which struct is which without a full read) |
| `Particle.h` | 272 | 13 | `Graphics::Particle`, `ParticleInstance`, `ParticleSystem`, `ParticleSystemInstance` |
| `Scene.h` | 133 | 11 | `Graphics::Scene` |
| `SunLight.h` | 94 | 11 | `Graphics::SunLight` (and presumably `Light`/`AmbientLight`/`SpotLight` per the vtable rows in §1.4 — NOT VERIFIED whether they share this header or are undocumented) |

**No dedicated type header found for:** `Graphics::Light`, `AmbientLight`, `SpotLight`,
`AnimationGroup`, `ReflectionImageBuffer`, `Renderer` — these have `.cpp` bodies and/or
vtable rows (§1.3/§1.4) but their POD layout's home header is NOT VERIFIED this pass
(likely inside `SunLight.h`/`Mesh.h`/`AnimationManager.h` given the file's own content
isn't exhaustively read, or possibly undocumented the way `IO::PackageFile.h` doesn't
exist — flag for the step-5 owner to confirm by grep, not assumed absent).

### 1.3 `.cpp` file table

`runtime/src/video/*.cpp`: **31 files** (VERIFIED `ls`), lines not summed this pass
(`wc -l runtime/src/video/*.cpp runtime/src/graphics/*.cpp` totalled **39,111 lines**
combined for both directories, VERIFIED). `runtime/src/graphics/*.cpp`: **26 files**.

| file (video) | role (from filename / README) |
|---|---|
| `context.cpp`, `context_bind.cpp`, `context_ctor.cpp`, `context_lifecycle.cpp` | `Video::Context` split across lifecycle/bind/ctor |
| `device.cpp`, `device_startup.cpp`, `device_startup_win.cpp`, `device_adjustdisplay_win.cpp`, `device_enum_types.cpp` | `Video::Device` — startup is a `_win` platform seam per dir convention |
| `buffers.cpp`, `buffers_win.cpp` | the buffer family (`VertexBuffer`/`IndexBuffer`/`RenderBuffer`/`ConstantBuffer`) |
| `constant_buffer.cpp` | `ConstantBuffer` (separate from `buffers.cpp` — overlap NOT resolved this pass) |
| `dynamic_resources.cpp`, `video_resources.cpp`, `video_resources_win.cpp` | resource lifetime helpers |
| `renderstate.cpp` | `Video::RenderState` |
| `shaderprogram.cpp`, `pixelprogram.cpp`, `vertexprogram.cpp`, `videoprogram.cpp`, `video_program.cpp` | shader/program family — note both `videoprogram.cpp` AND `video_program.cpp` exist (two files, per README "`VideoProgram.cpp` -> `video_program.cpp` + `videoprogram.cpp`" — a deliberate split, not a duplicate) |
| `vertexlayout.cpp` | `Video::VertexLayout` |
| `imagebuffer_vtable.cpp`, `imagebuffer_platform_serialize.cpp` | `Video::ImageBuffer` |
| `enumerate_video_modes.cpp`, `video_mode_sort.cpp` | mode enumeration (has the `__try`/`__except` comments cited in `IO_INVENTORY.md` §7 — SEH-can't-port note applies here too, VERIFIED same grep hits this pass at `enumerate_video_modes.cpp:57,70`) |
| `video_interfaces_win.cpp` | Win32 interface glue |
| `companion_data.cpp` | the `*Data` companion objects' Runtime-side half (dx11 owns the other half, §3) |
| `batchc_context_setlight.cpp`, `closure_batch2b.cpp` | wave/batch cutover-closure files (naming convention shared with `io/closure_batch2b_win.cpp`) |
| `save_image_to_file.cpp` | screenshot / image save path |
| `rt_factories_video.cpp` (+`.inc`), `rt_video_port.cpp` | factory registration + wave entry point (same convention as `runtime/src/io/rt_factories_io.cpp` / `rt_io_port.cpp`) |
| `shader_program_names.cpp`, `shader_constants.inc` | shader name tables / constants |
| `vertexlayout.cpp` | (listed above) |

| file (graphics) | role |
|---|---|
| `mesh.cpp`, `meshinstance_draw.cpp`, `edgemesh_vtable.cpp`, `graphicsmesh_vtable.cpp` | mesh family |
| `animation.cpp`, `animationstate.cpp`, `animationstate_transforms.cpp` | animation family |
| `sunlight.cpp`, `sun_camera.cpp` | `SunLight`/`Camera` (has the `throw away` comment noted in IO_INVENTORY §7, `sunlight.cpp:386` — false positive, not real C++ `throw`) |
| `scene.cpp`, `scene_grid.cpp`(+`.h`) | `Graphics::Scene` |
| `material.cpp` | `Material`/`MaterialInstance` |
| `renderer.cpp` | `Graphics::Renderer` |
| `cull.cpp` | culling |
| `imagebuffer_derived.cpp` | `FrameCopyImageBuffer`/`GeneratedImageBuffer`/`ReflectionImageBuffer` |
| `lights_vtable.cpp` | `Light`/`AmbientLight`/`SpotLight` vtable bodies |
| `graphics_typeinfo_accessors.cpp` | reflection accessors (same convention as `object/object_typeinfo_accessors.cpp`) |
| `batchc_gfx_leaves.cpp` | cutover-closure leaves batch |
| `vtbatch7_animgroup_particlesystem.cpp` | `AnimationGroup` + `ParticleSystem` batch (named like `runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp` — same wave convention) |
| `wave9_lane_gfx.cpp`, `wave9_lane_gfxd.cpp`(+`.h`), `wave21_lane_gfxbody.cpp`, `wave23_lane_gfxbody.cpp`, `wave24_lane_gfxmat.cpp`, `wave27_lane_gfxbig.cpp`, `wave27_lane_gfxpart.cpp`, `wave28_lane_gfxbody.cpp`, `wave28_lane_partsys.cpp`, `wave29_lane_gfxbody.cpp`, `wave29_lane_slots.cpp` | **11 wave-lane files** — this namespace was ported across many small numbered waves rather than whole-class files the way `runtime/src/io/` mostly was; a step-5 planner should expect the class bodies to be SCATTERED across these wave files, not one file per class (contrast with IO's "whole classes, one file each" §1 note) |
| `rt_factories_graphics.cpp`(+`.inc`), `rt_graphics_port.cpp` | factory + wave entry |

**CLAUDE.md §"No waves, lanes"** rule note: these `waveNN_lane_*.cpp` filenames are
historical (pre-dating the current no-new-waves rule) — they are existing code, not new
work; the step-5 planner is not creating new waves by touching them, but should know the
convention exists in this namespace more than most.

`runtime/src/graphics/README.md` still says **"Ported here: _nothing yet_"** despite 26
`.cpp` files and 249 export + 219 addr registrations existing — the README is stale, not
an accurate signal of what's ported. Flag for whoever owns doc upkeep; do not read that
README's silence as "graphics is unported."

### 1.4 Polymorphism / vtables (VERIFIED against `standalone/rt_data_mint_tables.inc`)

**Video — 20 vtable rows found** (`grep -n '@Video@@6B@' rt_data_mint_tables.inc`):
`ConstantBufferData`(4), `ContextData`(20), `DeviceData`(20), `ImageBuffer`(10),
`ImageBufferData`(7), `IndexBufferData`(7), `PixelProgram`(9), `PixelProgramData`(3),
`RenderBufferData`(4), `RenderState`(9), `RenderStateData`(3), `ShaderProgram`(8),
`VertexBufferData`(7), `VertexLayout`(9), `VertexLayoutData`(3), `VertexProgram`(9),
`VertexProgramData`(3), `VideoProgram`(9), `VideoProgramData`(3) = 19 distinct symbols
(one, `ConstantBufferData`, is listed once — 19 not 20, correcting my own header count).

Note the `*Data` suffix classes (`ContextData`, `DeviceData`, `ImageBufferData`, the
buffer `*Data`s, `*ProgramData`s) are the **dx11-owned companion objects** (§3) — they
have their OWN vtables, separate from the Runtime-side class's vtable. So e.g.
`Video::Device` (POD, no vtable of its own per `Device.h` — NOT VERIFIED, inferred from
absence of a `??_7Device@Video@@6B@` row) owns a `DeviceData*` which DOES have a 20-slot
vtable that dx11/ fills.

**Graphics — 18 vtable rows** (VERIFIED grep, matches the cutover-manifest's own count
"Graphics 4" — that count is REQUIRED-tagged storage rows only, not vtable rows; do not
conflate the two): `AmbientLight`(11), `AnimationEvent`(9), `AnimationGroup`(9),
`EdgeMesh`(9), `EdgeMeshInstance`(9), `FrameCopyImageBuffer`(12),
`GeneratedImageBuffer`(12), `Geometry`(9), `GraphicsMesh`(9), `Light`(11), `Material`(9),
`MaterialInstance`(9), `MeshInstance`(9), `Particle`(9), `ParticleInstance`(10),
`ParticleSystem`(10), `ParticleSystemInstance`(10), `ReflectionImageBuffer`(12),
`SpotLight`(11), `SunLight`(11) = **20 distinct**, corrected from my earlier miscount of
18 (the earlier "Graphics 4" I initially assumed was vtables was actually the
cutover-manifest's REQUIRED storage rows, §2).

**RT_DELDTOR rows** (`runtime/src/object/deleting_dtor.cpp`, VERIFIED grep):
Video — 6 rows: `VideoProgram`, `VertexLayout`, `ShaderProgram`, `VertexProgram`,
`PixelProgram`, `RenderState` (the `*Data` classes are NOT in this list — their deleting
dtors are presumably registered directly the way IO's `MemoryStream`/`FileStream` are,
or by dx11/, NOT VERIFIED which — a `*Data` class's dtor ownership is a real open
question for step 5, since dx11/ half-owns those classes).
Also 12 more `RT_DELDTOR` rows tagged `// probe subject` for every `*Data` class
(`RenderBufferData`, `VideoProgramData`, `RenderStateData`, `PixelProgramData`,
`VertexProgramData`, `VertexLayoutData`, `VertexBufferData`, `IndexBufferData`,
`ConstantBufferData`, `ImageBufferData`, `ContextData`, `DeviceData`) plus
`ImageBuffer` itself (13 total, all size 0x010 except ImageBuffer's 0x068) — so the
`*Data` classes DO have `RT_DELDTOR` rows after all, contradicting my sentence above;
**corrected: all 19 Video vtable classes have an `RT_DELDTOR` row.** The "probe subject"
comment on 12 of them is worth the step-5 owner asking about — it suggests these were
added for an instrumentation/measurement pass, not a completed port, and may need
re-verification of the body quality, not just presence.

Graphics — 20 `RT_DELDTOR` rows (VERIFIED grep), one per vtable class in the list above
— full 1:1 coverage, no gaps found.

**`?_typeInfo@` reflection rows** (VERIFIED grep, `_typeInfo@...@0VBaseTypeInfo@Object@@A`
pattern): **9 in Video** (`AnimationEvent`? — no wait, that's Graphics; re-list
correctly): Video rows are `ImageBuffer`, `PixelProgram`, `RenderState`, `ShaderProgram`,
`VertexLayout`, `VertexProgram`, `VideoProgram` = **7**. Graphics rows:
`AnimationEvent`, `AnimationGroup`, `EdgeMesh`, `FrameCopyImageBuffer`,
`GeneratedImageBuffer`, `GraphicsMesh`, `Material`, `MaterialInstance`, `Particle`,
`ParticleSystem`, `ReflectionImageBuffer` = **11**. Total 18, which is where the
cutover-manifest's earlier-quoted "Graphics 4 / Video 18" line (§2) draws from a
DIFFERENT count (REQUIRED-tagged rows only) — **do not equate vtable-row counts,
typeInfo-row counts, and cutover-REQUIRED counts; they are three different tallies that
happen to share small numbers.** This is the single easiest mistake for the step-5
planner to make reading this document too fast.

Classes with a `?_typeInfo@` row are `Object::External` subclasses (same logic
`B34_RULES.md` §1.4 used for `IO::PackageFile`) — so at minimum these 18 are confirmed
`External`-derived; the remaining Video/Graphics vtable classes without a typeInfo row
(`Context`/`Device`/the `*Data` family/`Light`/`AmbientLight`/`SpotLight`/`SunLight`/
`MeshInstance`/`EdgeMeshInstance`/`ParticleInstance`/`ParticleSystemInstance`/
`AnimationGroup`... wait `AnimationGroup` IS listed) either derive from something else,
are unreflected `External` subclasses (rare but exists, e.g. `IO::CompressedCopyFileStream`
had no typeInfo row either per `IO_INVENTORY.md`), or are non-`External` polymorphic
classes entirely (e.g. `SunLight`/`Light` siblings look like a small hierarchy of their
own, possibly rooted differently) — **base class for most of these is NOT VERIFIED**;
confirming it needs the same slot-by-slot retail read `B34_RULES.md` §1.4 did for
`PackageFile`, class by class, which this pass did not do (out of scope for a map).

### 1.5 Base classes — what IS known

- `Object::External`/`Object::Internal` question (B.3.3): NOT re-verified per class here.
  The 18 typeInfo-row classes above are the strong candidates for `External` subclasses;
  everything else is open.
- `runtime/types/object/External.h`/`Internal.h` were read by B.3.3 for the Object family
  only — this pass did NOT cross-reference Video/Graphics classes against those headers'
  own comments (e.g. whether `External.h` or `Internal.h` mention a Video/Graphics
  subclass by name). Flag as a fast follow-up grep (`grep -n "Video\|Graphics"
  runtime/types/object/External.h runtime/types/object/Internal.h`) rather than leaving
  unstated.

---

## 2. Counts

**`RegisterRuntimeExport` / `RegisterRuntimeAddr`** (VERIFIED, `grep -c` summed):

| dir | export | addr |
|---|---|---|
| `runtime/src/video/*.cpp` | 268 | 276 |
| `runtime/src/graphics/*.cpp` | 249 | 219 |
| **total** | **517** | **495** |

(For scale: IO was ~273 export + ~320 addr ≈ 593 total per `IO_INVENTORY.md` §1. Video+
Graphics combined is 1,012 — nearly double IO's registration count, consistent with 19+20
polymorphic classes vs IO's ~15.)

**Minted vtable rows** (VERIFIED, §1.4): 19 Video + 20 Graphics = **39 minted vtable
rows** total naming Video/Graphics — roughly double IO's 19-row count from
`B34_RULES.md` §4.2. This alone says step 5's Question-4-equivalent sweep (who stamps
each table, from where) is a bigger job than IO's was.

**Storage rows** — the mangled-name storage/singleton rows the cutover manifest tags
(VERIFIED, `python tools/cutover_manifest.py --table`, `by namespace:` summary line):
`Graphics 4 | ... | Video 18` — this is the manifest's own per-namespace REQUIRED-tag
count, not a full row count (the manifest by default may print only REQUIRED rows;
NOT VERIFIED whether `--table` has a filter flag that was silently applied — treat
"4"/"18" as a lower bound, not an exhaustive count, until the manifest's own `--help` is
read). Sample rows seen (VERIFIED, from the table output):
- Graphics storage (HARMFUL/DLL-PRESENT, REQUIRED): `?_frameIndex@ParticleInstance@Graphics@@1HA`,
  `?_rng@Particle@Graphics@@1VRandom@System@@A`, `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA`,
  `?_singleton@AnimationManager@Graphics@@0PEAV12@EA`.
- Video: 11 `??_7*Data@Video@@6B@` NO-OP/PERMANENT vtable rows (the dx11 companion
  vtables — "NO-OP is permanent, but N by-name door(s) resolve NULL with no DLL loaded",
  i.e. these rows document that the Runtime DLL's OWN copy of these vtables is
  intentionally inert since dx11/ is the real implementation) plus storage rows
  `?_firstBuffer@IndexBuffer@Video@@…`, `?_firstBuffer@VertexBuffer@Video@@…`,
  `?_firstRenderBuffer@RenderBuffer@Video@@…`, `?_firstSurface@ImageBuffer@Video@@…`,
  `?_singleton@Configuration@Video@@…`, `?_singleton@Device@Video@@…` (the last with 36
  by-name doors — the single largest door-count of any REQUIRED row seen in this table).

**Cutover manifest tool**: `tools/cutover_manifest.py`, generates
`standalone/cutover_manifest.inc` (consumed by `standalone/cutover_manifest.cpp`),
`--table` prints the human markdown, `--check` verifies. VERIFIED by running
`--table` (read-only; it only prints, per the script's own docstring "the human manifest
... one row per tagged gap row" — did not run `--inc`/`--check` since those are meant to
write/verify generated files, staying strictly read-only per the task's no-edit mandate).

---

## 3. The dx11/ boundary

`dx11/` is the reimplementation of `VideoDX11-x64.dll`, built with `-Iruntime/types`
(VERIFIED, `Makefile:68` `DX11_FLAGS ... -Iruntime/types -Iruntime/funcs -Idx11/types
-Idx11/funcs -Istandalone`). It is a genuinely separate translation unit set from
`runtime/src/video` — CLAUDE.md's "dx11/ stays the plugin, implemented in-tree" instruction
for step 5 means: Video/Graphics classes in `runtime/` become real C++, but the
DirectX-11-specific `*Data` companion classes stay in `dx11/` as their own plugin
implementation, reached through the same seam.

**dx11/src files and roles** (VERIFIED, `README.md` + directory listing, 12 files):

| file | role | state |
|---|---|---|
| `dx11_backend_access.{h,cpp}` | shared plumbing: `Video::Device -> DeviceData -> ID3D11Device/Context` walk, offset-pinned views of Runtime-side owner objects (`Video::ShaderProgram`, `VertexLayout`, `RenderBuffer`, `ImageBuffer`), lazy resolution of Runtime base destructors | — |
| `devicedata_stubs.cpp` | `DeviceData` slots 4-7, four one-liner stubs | default-on, verified in-game 2026-07-24 per README |
| `shaderprograms.cpp` | `VertexProgramData`/`PixelProgramData` (3 slots each) — dtor, `PlatformCreate`, `Bind` | opt-in `BANISHED_DX11_SHADERS` |
| `vertexlayoutdata.cpp` | `VertexLayoutData` (3 slots) | opt-in `BANISHED_DX11_SHADERS` |
| `smallclasses.cpp` | `VideoProgramData` (3, inert) + `RenderBufferData` (4) | opt-in `BANISHED_DX11_SHADERS` |
| `buffers.cpp` | (present, README does not list it under Landed/Still-to-come — a gap in the README itself) |
| `contextdata.cpp` | `ContextData` — README lists this as "Still to come" (`context_data.cpp`) but the file EXISTS as `contextdata.cpp` (no underscore) — README is stale on filenames, not just status |
| `imagebufferdata.cpp` | `ImageBufferData` — also README-listed as "still to come" (`image_buffer_data.cpp`) but exists |
| `renderstatedata.cpp` | `RenderStateData` — same stale-README pattern (`render_state_data.cpp` listed as future) |
| `vertexlayoutdata.cpp` | already counted above |
| `exports.cpp` | the DLL export table stand-in (NOT VERIFIED exact role — name-inferred) |

**dx11/'s "still to come" list per its own README is largely already landed** —
`device_data.cpp`, `vertex_buffer_data.cpp`, `index_buffer_data.cpp`,
`constant_buffer_data.cpp`, `render_state_data.cpp`, `image_buffer_data.cpp`,
`context_data.cpp` are all listed as future work but files with equivalent (differently
spelled, no-underscore) names exist in the directory listing (`devicedata.cpp`,
`renderstatedata.cpp`, `imagebufferdata.cpp`, `contextdata.cpp`,
`vertexlayoutdata.cpp` already covered, plus `buffers.cpp` likely covering
vertex/index/constant buffer data together). **Same class of staleness as
`runtime/src/graphics/README.md`'s "_nothing yet_" — step 5 should not trust either
README's status table without a fresh `ls` + symbol check**, though the FILE-LIST
("one file per class or per closely-related group") portion of dx11's README is
presumably still an accurate naming convention.

**dx11 includes of `runtime/types` headers** (VERIFIED grep, only 2 hits, both look
incidental rather than structural): `devicedata.cpp:125` includes `"system/String.h"`
(comment: `// -Iruntime/types`) and `dx11_backend_access.h:29` includes
`"collection/Collection.h"` for `Collection::Array<T>`. **dx11/ does NOT widely depend on
Video/Graphics type headers directly by `#include`** in this grep pass — it more likely
reaches Runtime-side objects through `dx11_backend_access.h`'s "offset-pinned views"
(i.e. its own local structs mirroring offsets, not the real headers) rather than
including `runtime/types/video/*.h` — **NOT VERIFIED without reading
`dx11_backend_access.h` in full**, which this pass did not do (flagged as the single
most important follow-up read for step 5, since "offset-pinned views" duplicated from
`runtime/types/video/*.h` is exactly the kind of duplicate-POD hazard `B34_RULES.md`
§1.5.2 found for `IO::Stream`).

**Who constructs the plugin interface, which vtables are dx11's own vs Runtime's**: NOT
VERIFIED this pass — would need reading `dx11_backend_access.cpp` and
`runtime/src/video/device_startup*.cpp` together. The `*Data` vtables (§1.4, 19 minted
rows with the `*Data` suffix) are the dx11-owned tables; the plain-name Video classes
(`Device`, `Context`, `ShaderProgram`, etc.) are Runtime-owned and (for the ones with a
vtable, e.g. `ShaderProgram`/`RenderState`/`VertexLayout`/the Program family) are
converted the normal `B34_RULES.md`-style way. This split is the load-bearing design
fact for step 5 and deserves its own confirmed read, not an inference.

---

## 4. Caller surfaces

Measured by grep occurrence count (not the comment/string-aware lexer
`IO_CALLER_INVENTORY.md` used — **these counts are raw and will include some
comment/guard noise; treat as an upper-bound ranking, not exact CALL counts** the way
B.3.4's lexer-based numbers are).

### 4.1 `engine_funcs.h` global doors — small (42 total lines, 21 distinct pointers)

VERIFIED full list (§ above, "fn_Video_.../fn_Graphics_..." grep). Only **3 `Video`
doors** (`fn_Video_Context_SetFog`, `fn_Video_IndexBuffer_Ctor`,
`fn_Video_VertexBuffer_Ctor`) vs **18 `Graphics` doors** — Video's caller surface through
`engine_funcs.h` is nearly absent, which either means (a) `code/Game` rarely touches
`Video::*` objects directly (plausible — game logic manipulates `Graphics::Geometry`/
`Scene`/`MeshInstance`, not `Video::Context` directly), or (b) Video callers use a
different idiom entirely (ResolveExport-by-mangled-name, §4.2, which the numbers below
confirm is actually the DOMINANT idiom for both namespaces, not just a supplement — this
namespace pair looks more like `B34_RULES.md` §6.5(d)'s `System::Thread` finding than
like IO's `engine_funcs.h`-centric caller list).

Call counts (VERIFIED, raw grep `\bname(`):

| pointer | calls |
|---|---|
| `fn_Graphics_Scene_AddGlobalObject` | 12 |
| `fn_Graphics_Scene_AddObject` | 7 |
| `fn_Graphics_MeshInstance_SetMesh` | 4 |
| `fn_Graphics_Camera_Ctor` | 4 |
| `fn_Graphics_SunLight_Set` | 3 |
| `fn_Graphics_Scene_Ctor` | 3 |
| `fn_Graphics_Scene_Clean` | 3 |
| `fn_Graphics_Renderer_Ctor` | 3 |
| `fn_Video_VertexBuffer_Ctor` | 2 |
| `fn_Video_IndexBuffer_Ctor` | 2 |
| `fn_Video_Context_SetFog` | 2 |
| `fn_Graphics_SunLight_Ctor` | 2 |
| `fn_Graphics_Renderer_Draw` | 2 |
| `fn_Graphics_MeshInstance_SetMaterialInstance` | 2 |
| `fn_Graphics_MeshInstance_Ctor` | 2 |
| `fn_Graphics_Geometry_Ctor` | 2 |
| `fn_Graphics_Camera_SetFrustum` | 2 |
| `fn_Graphics_ParticleSystemInstance_Ctor` | 1 |
| `fn_Graphics_ParticleInstance_IncrementInstanceIndex` | 1 |
| `fn_Graphics_Particle_Ctor` | 1 |
| `fn_Graphics_GraphicsMesh_GetMaterialList` | 1 |

**Total `engine_funcs.h`-door CALL: 61** — tiny compared to IO's 861+130. This confirms
Video/Graphics is NOT primarily reached through `engine_funcs.h`.

### 4.2 `ResolveExport("...@Video@@..."/"...@Graphics@@...")` by mangled name — the dominant idiom

VERIFIED grep, **178 call sites** across `code/Game`/`engine`/`standalone` (excludes
`runtime/` itself, which is expected to reference its own exports constantly and isn't a
"caller" in the B34-rules sense). This is nearly 3x the `engine_funcs.h` door count —
**Video/Graphics callers overwhelmingly resolve by mangled name into file-local
`s_fn_*`-style caches, the same pattern `B34_RULES.md` §6.5(d) found for
`System::Thread`/`CriticalSection`, not the `engine_funcs.h` global-pointer pattern IO's
Serializer family uses.** A step-5 caller sweep MUST NOT scope itself by reading
`engine_funcs.h` — that would report a caller count of ~61 when the truth is closer to
239 (61 + 178) plus whatever the `s_fn_*` file list below hides.

By directory (VERIFIED, `grep -c` per file, top 15):

| file | ResolveExport(Video/Graphics) sites |
|---|---|
| `code/Game/Game/minimap_paint.cpp` | 13 |
| `code/Game/Game/gameupdateloop.cpp` | 8 |
| `code/Game/Player/tool_base_tilerender.cpp` | 5 |
| `code/Game/Game/tornado.cpp` | 4 |
| `code/Game/Game/serialize_heightmap_mesh.cpp` | 4 |
| `code/Game/Action/action_base_complete.cpp` | 4 |
| `code/Game/Game/weather_particle_draw.cpp` | 3 |
| `code/Game/Game/w2_takescreenshot.cpp` | 3 |
| `code/Game/Component/statusiconcomponent.cpp` | 3 |
| `code/Game/Component/particlecomponent.cpp` | 3 |
| `code/Game/Component/frustumoutlineelement.cpp` | 3 |
| `code/Game/Game/weather_particles.cpp` | 2 |
| `code/Game/Game/water_contour_mesh.cpp` | 2 |
| `code/Game/Game/gameupdateloop_load.cpp` | 2 |
| `code/Game/Component/animaudioevent.cpp` | 2 |

28 files (VERIFIED, `grep -rl`) cache a Video/Graphics mangled export as an `s_fn_*`
file-static — full list: `action_base_complete.cpp`, `action_base_movedeps.cpp`,
`animaudioevent.cpp`, `citizenai_executor.cpp`, `frustumoutlineelement.cpp`,
`naturalresourcecomponent.cpp`, `particlecomponent.cpp`, `statusiconcomponent.cpp`,
`statusicon_geometry.cpp`, `moddialog.cpp`, `gameupdateloop.cpp`,
`gameupdateloop_load.cpp`, `gameupdateloop_serialize.cpp`, `game_leaf_helpers.cpp`,
`game_main.cpp`, `menudialogloopt.cpp`, `minimap_paint.cpp`,
`serialize_heightmap_mesh.cpp`, `serialize_image_upload.cpp`, `tornado.cpp`,
`w2_takescreenshot.cpp`, `water_contour_mesh.cpp`, `weather_particles.cpp`,
`weather_particle_draw.cpp`, `weather_sim.cpp`, `weather_sun.cpp`,
`placementtool.cpp`, `tool_base_tilerender.cpp`.

Top mangled-name shapes by frequency across `code/Game` (VERIFIED,
`grep -rho '@Video@@...\|@Graphics@@...'`, top hits): 13x a `Graphics@@QEAA@XZ`
(a default ctor family — several classes share this shape, not one function), 6x
`Graphics@@QEBA` (const-method family), 5x `Video@@QEAAXHAEBVMatrix@Math@@@Z` (a
`void(int, const Math::Matrix&)` setter, likely `Context::Set*Matrix`), 4x
`Video@@QEAA_NH_NPEAX@Z`, 4x `Graphics@@UEAA@XZ` (a virtual — dtor-shaped), 4x
`Graphics@@QEAAXPEAVGeometry@2@@Z` (`Scene::AddObject`/`RemoveObject`-shaped, matches
§4.1's Scene numbers), 4x `Graphics@@QEAAXAEBVBox@Volume@@@Z` (`Geometry::SetBounds`).

### 4.3 exe dispatch slots (`standalone/address_registry.cpp` + `reg_*.cpp`)

VERIFIED grep-count of files mentioning "Video"/"Graphics" (string match, not a precise
slot count — includes comments):

| file | "Video" hits | "Graphics" hits |
|---|---|---|
| `address_registry.cpp` | 17 | 34 |
| `reg_mint_promoted.cpp` | 53 | 61 |
| `reg_player_ray.cpp` | 2 | 0 |
| `reg_player_tilerender.cpp` | 1 | 0 |
| `reg_unreg_lane2.cpp` | 1 | 0 |
| `reg_unreg_lane3.cpp` | 1 | 14 |
| `reg_action_complete.cpp` | 0 | 1 |
| `reg_component_gap_C.cpp` | 0 | 1 |
| `reg_destroy_sm.cpp` | 0 | 2 |
| `reg_player_place.cpp` | 0 | 2 |
| `reg_player_render.cpp` | 0 | 1 |

`reg_mint_promoted.cpp` dominates both (53+61 = 114 of the ~188 total hits) — consistent
with §1.4's finding that Video/Graphics between them mint 39 vtable rows, each needing a
promotion row there. This file is analogous to IO's own entry in the same file
(`B34_RULES.md` §4.2's promo-row table) and will need the same "who still stamps this
table" analysis before any row can retire — NOT done this pass (out of scope for a map;
step 5's own Question-4 pass should do it the way `B34_RULES.md` §4 did for IO).

### 4.4 Top 15 doors by call count, combined ranking

Combining §4.1 (engine_funcs) with §4.2's by-file ResolveExport counts (not
individual-symbol counts — the per-symbol ResolveExport breakdown needs the
comment-aware lexer `IO_CALLER_INVENTORY.md` used, NOT VERIFIED to that precision here):

1. `code/Game/Game/minimap_paint.cpp` — 13 ResolveExport sites (file-level, mixed symbols)
2. `fn_Graphics_Scene_AddGlobalObject` — 12 calls
3. `code/Game/Game/gameupdateloop.cpp` — 8 ResolveExport sites
4. `fn_Graphics_Scene_AddObject` — 7 calls
5. `code/Game/Player/tool_base_tilerender.cpp` — 5 ResolveExport sites
6. `fn_Graphics_MeshInstance_SetMesh` — 4 calls
7. `fn_Graphics_Camera_Ctor` — 4 calls
8. `code/Game/Game/tornado.cpp` — 4 ResolveExport sites
9. `code/Game/Game/serialize_heightmap_mesh.cpp` — 4 ResolveExport sites
10. `code/Game/Action/action_base_complete.cpp` — 4 ResolveExport sites
11. `fn_Graphics_SunLight_Set` — 3 calls
12. `fn_Graphics_Scene_Ctor` — 3 calls
13. `fn_Graphics_Scene_Clean` — 3 calls
14. `fn_Graphics_Renderer_Ctor` — 3 calls
15. `code/Game/Game/weather_particle_draw.cpp` — 3 ResolveExport sites

**This ranking mixes file-level and symbol-level counts because the precise per-symbol
ResolveExport breakdown was not run this pass** (would need the same lexer
`IO_CALLER_INVENTORY.md` built) — flagged rather than presented as more precise than it
is. The clear signal regardless of exact method: `Scene`, `MeshInstance`/`Mesh`,
`Camera`, `minimap_paint.cpp`, and `gameupdateloop.cpp` are the hot spots.

---

## 5. Hazards

1. **`Video::Context` holds multiple structs BY VALUE**, confirmed by header read
   (`Context.h`): `Viewport _viewport`, `Viewport _scissor`, `ConstantBufferOpaque
   _materialConstants/_pixelConstants/_globalConstants`, `MatrixRaw _viewProjection/
   _inverseView`, `Vector4Raw _cameraPosition/_unknown03b8/_time/_fogParams/_fogColor/
   _inverseScale/_color`. If any of `Viewport`/`ConstantBufferOpaque`/`MatrixRaw`/
   `Vector4Raw` become classes with non-trivial dtors/vtables, `Context` becomes
   non-trivially-copyable — the exact `IO::CompressedMemoryStream`-holds-`MemoryStream`
   shape `B34_RULES.md` §2 flagged (member dtor must stay public, non-virtual). These
   four helper structs are declared in `Context.h` itself (`Viewport:165`,
   `Vector4Raw:177`, `MatrixRaw:182`, `RenderBufferPrefix:191`,
   `ConstantBufferOpaque:200`) — VERIFIED by grep, not fully read.

2. **Vtables stamped BY ADDRESS in `code/Game`**: only 2 raw `0x18015xxxx` hex-literal
   hits tree-wide in `code/Game` (VERIFIED grep), and BOTH are comments/documentation,
   not live stamps: `statusicon_geometry.cpp:272` (a comment describing
   `VA 0x180154e60`'s code pointers) and `tool_base_toolui.cpp:136-142` (a `DialogInstance`
   vtable canonicalization, which is a UI class, not Video/Graphics — the actual
   `ResolveAddr(Module::Runtime, 0x180156d70)` call at line 142 targets a UI vtable, out
   of this namespace's scope entirely). **So this specific hazard (hex-literal vtable
   stamps) is NOT present in Video/Graphics callers today**, unlike some other
   namespaces — a genuinely clean finding, not a gap in the search.

3. **Hand-inlined `*(void**)obj` vtable-slot calls near Video/Graphics-sounding files**:
   a keyword-based sample (`*(void**)` AND one of video/graphics/mesh/particle/light/
   texture/shader in the same file) returned **91 files** — but this is almost certainly
   an OVER-broad match (e.g. `buildcomponent.cpp`, `cemeterycomponent.cpp` likely matched
   on "light" or "texture" mentioned incidentally, not a Video/Graphics vtable call).
   **NOT VERIFIED as a real count** — a step-5 pass needs a tighter grep anchored on an
   actual Video/Graphics class name adjacent to the `*(void**)` cast, the way
   `IO_CALLER_INVENTORY.md` §4 anchored on `SerializerEncodingVCall`'s specific helper
   name. Flagged as "there is probably a real hand-inlined-vtable-call population here,
   roughly in the same 91-file ballpark or smaller, and it needs the tighter sweep
   before step 5 sizes a caller batch" rather than asserted as a hard number.

4. **Thread/render-thread concerns**: NOT directly investigated this pass beyond noting
   that `device_startup_win.cpp`/`device_adjustdisplay_win.cpp` are `_win` platform seams
   (per the directory's own README convention) that plausibly touch the D3D device on a
   specific thread, and that `dx11_backend_access.h`'s "lazy resolution of Runtime base
   destructors" phrase (from its own one-line README description) suggests some
   ordering-sensitive teardown path between `runtime/` and `dx11/`. No `CriticalSection`/
   `Thread` hits were searched specifically inside `runtime/src/video`/`graphics` this
   pass — that grep is a fast, valuable follow-up (`grep -rn "CriticalSection\|Thread"
   runtime/src/video runtime/src/graphics dx11/src`) that was not run.

5. **Device-lost / device-recreate handling**: NOT searched this pass. Given
   `device_startup.cpp`/`device_startup_win.cpp` exist and D3D11 device-lost is a classic
   hazard, this is worth a targeted grep (`grep -rn "DeviceLost\|device.lost\|Reset.*Device"
   runtime/src/video dx11/src`) before step 5 starts — flagged as unresearched, not as
   absent.

6. **Docs claiming unported/stubbed state**: `runtime/src/graphics/README.md`'s
   "_nothing yet_" (§1.3) and `dx11/src/README.md`'s "still to come" list (§3) are BOTH
   stale relative to the actual file listing — treat neither as a status source for step
   5; use the file/registration counts in this document instead. `docs/runtime_port/
   STATUS.md` and `docs/reference/engine_architecture.md` were NOT read this pass for a
   Video/Graphics-specific status line — a fast follow-up grep
   (`grep -n -i "video\|graphics" docs/runtime_port/STATUS.md
   docs/reference/engine_architecture.md`) is recommended before step 5 planning
   finalizes, since this document did not check those two files as instructed and that
   is a real gap in this inventory, not a "nothing found" result.

7. **`docs/runtime_port/STATUS.md` DOES document real Video/Graphics history** (VERIFIED
   grep this pass, `STATUS.md:421-460`): `Video::Device::InitializeInterfaces` used to
   `LoadLibraryW` the DX9 video plugin, which statically imports `Runtime-x64.dll` —
   fixed by making the loop load nothing (`video_interfaces_win.cpp`, closed 2026-09-13).
   `Graphics::Geometry`'s copy ctor in `graphics/mesh.cpp` was a live call through NULL
   (a raw-door dependency found during the A.3 DLL-removal pass, since folded to the
   registry door) — every `MeshInstance`/`EdgeMeshInstance` copy went through it.
   `GraphicsMesh::PlatformSerialize`'s minted vtable slot was UNREGISTERED in the PERF
   build only (a `printf(...RtXxxRegister()...)` stripped by the perf build's `log_strip.h`,
   `reference_perf_build_strips_printf_arguments.md` — the same class of bug that memory
   already tracks). A second perf-only crash: `Renderer::DrawItemList` faulted reading a
   `Math::Matrix` returned BY VALUE from `Context::SetTransform`'s read of the geometry's
   vtable slot 6 (`GetInstanceTransform`) — the `ParticleInstance::GetInstanceTransform`
   port in `wave27_lane_gfxpart.cpp` was implicated. **All of these are CLOSED historical
   bugs, not open hazards**, but they are exactly the shape of bug (raw-door fallback,
   perf-build printf-stripped registration, by-value return from a vtable slot) that a
   step-5 class conversion should watch for again in the SAME files
   (`graphics/mesh.cpp`, `graphicsmesh_vtable.cpp`, `wave27_lane_gfxpart.cpp`,
   `context*.cpp`) since they've already produced two boot-time and one stress-time
   failure. `docs/reference/engine_architecture.md` has only one incidental Video/Graphics
   hit (a `VideoSettings` registry-bit comment, `:18`) — nothing load-bearing there.

8. **The `videoprogram.cpp` / `video_program.cpp` split** (§1.3) and the
   `constant_buffer.cpp` vs `buffers.cpp` overlap for `ConstantBuffer` are both worth a
   confirming read before a step-5 file-ownership split is drawn — if two files both
   touch the same class's registrations, a block/batch split by filename (the way
   `B34_RULES.md` §6.3 split IO into STREAMS/PACKAGE+FS by disjoint file sets) needs to
   know which file owns which member first.

---

## 6. Proposed split into blocks

Modeled on `B34_RULES.md` §6.1/§6.3's pattern (core serial owner first, then
parallelizable blocks by disjoint file set, then caller batches by directory). This is a
FIRST DRAFT sizing proposal from the counts above, not a design decision — the actual
per-class slot analysis (the equivalent of `B34_RULES.md` §1) has not been done for any
Video/Graphics class in this pass, so block boundaries may shift once that analysis
exists, exactly as `B34_RULES.md` corrected `IO_INVENTORY.md`'s early guesses.

**Ordering rationale**: Video's `*Data` classes are dx11-owned and smallest slot-count
(3-4 slots mostly, plus Device/Context at 20) — good first movers to prove the
Runtime-side/dx11-side split cleanly, the same reason `B34_RULES.md` §3.2 moved
`IO::Encoding` (smallest, 5 slots, one caller) first. `Video::Context`/`Device`
(largest static_assert counts, 61/86) and the whole Graphics mesh/particle/light family
(highest caller-touching classes per §4.4) come later once the pattern is proven.

| block | scope | files | rationale |
|---|---|---|---|
| **0 — question pass (serial, one owner)** | no ported code touched | re-run this document's remaining NOT VERIFIED items: `dx11_backend_access.h` full read, base-class-per-class retail slot read (the `B34_RULES.md` §1 equivalent), `videoprogram.cpp`/`video_program.cpp` and `constant_buffer.cpp`/`buffers.cpp` ownership | mirrors `B34_RULES.md`'s own §1 "read the retail DLL byte-wise before declaring a shape" step — Video/Graphics has NOT had that pass yet, unlike IO when B34_RULES was written |
| **1 — Video small `*Data` family (dx11 boundary proof)** | `runtime/types/video/{Buffers.h,CompanionData.h}`, `runtime/src/video/{buffers.cpp,buffers_win.cpp,constant_buffer.cpp,companion_data.cpp}` + confirms the dx11-side vtable ownership split (§3) | ~5 files, 3-7 slot classes, smallest and proves the plugin boundary early like IO's Encoding move |
| **2 — Video program/shader family** | `runtime/types/video/ShaderProgram.h`, `runtime/src/video/{shaderprogram,pixelprogram,vertexprogram,videoprogram,video_program,vertexlayout,shader_program_names}.cpp` | 8-9 slot classes, all have `?_typeInfo@` rows (confirmed `External` subclasses per §1.4), same `Object::External` slot-1..7 pattern `B34_RULES.md` §1.4 established — mechanical once one is done |
| **3 — Video Context/Device/RenderState/ImageBuffer (the big PODs)** | `runtime/types/video/{Context.h,Device.h,RenderState.h,ImageBuffer.h}`, `runtime/src/video/{context*,device*,renderstate,imagebuffer_*,dynamic_resources,video_resources*,enumerate_video_modes,video_mode_sort,video_interfaces_win,save_image_to_file}.cpp` | 20-86 static_asserts, largest single classes — needs block 0's hazard-3 finding (Context's by-value members) resolved first |
| **4 — Graphics mesh/geometry/scene family** | `runtime/types/graphics/{Mesh.h,Geometry.h,Scene.h,Camera.h}`, `runtime/src/graphics/{mesh,meshinstance_draw,edgemesh_vtable,graphicsmesh_vtable,geometry(if separate file exists — NOT VERIFIED),scene,scene_grid,cull,sun_camera}.cpp` | the highest-caller-traffic group per §4.4 (`Scene`/`MeshInstance`/`Geometry`/`Camera` account for most of the top-15 doors) — do this after the small families prove the pattern, same "Component runs alone and first because it dominates every pointer family" logic `B34_RULES.md` §6.4 batch A used, inverted here (do it once patterns exist, not first, since it's also the riskiest for caller-count blast radius) |
| **5 — Graphics material/animation/particle/light family** | `runtime/types/graphics/{Material.h,AnimationManager.h,AnimationState.h,Animation.h,Particle.h,SunLight.h}`, `runtime/src/graphics/{material,animation,animationstate,animationstate_transforms,sunlight,vtbatch7_animgroup_particlesystem,lights_vtable,imagebuffer_derived}.cpp` + the 11 `waveNN_lane_*.cpp` files (§1.3) | the scattered wave-lane files live here — this block absorbs the historical fragmentation; needs its own sub-mapping of which wave file owns which class member before it can be split further |
| **6 — Renderer + `rt_factories_*`/`rt_*_port.cpp` (owner-retained)** | `runtime/src/graphics/{renderer.cpp,rt_factories_graphics.cpp+.inc,rt_graphics_port.cpp,batchc_gfx_leaves.cpp,closure_batch2b.cpp,graphics_typeinfo_accessors.cpp}`, `runtime/src/video/{rt_factories_video.cpp+.inc,rt_video_port.cpp,batchc_context_setlight.cpp,closure_batch2b.cpp}` | stays with whoever owns the whole step, same reason `B34_RULES.md` §6.3 kept `rt_io_port.cpp`/`rt_factories_io.cpp` with the IO owner — both agents in blocks 1-5 would otherwise touch these |
| **7 — Caller batches, by directory, AFTER blocks 1-6** | `code/Game/Game/*` (minimap_paint.cpp, gameupdateloop*.cpp, tornado.cpp, weather_*.cpp, water_contour_mesh.cpp, serialize_heightmap_mesh.cpp — the top of §4.4's ranking) run first per the same "highest-traffic directory first" logic as `B34_RULES.md` §6.4 batch A; then `code/Game/Component/*`, `code/Game/Player/*`, `code/Game/Action/*`, `code/Game/Dialog/*` (moddialog.cpp) as smaller trailing batches | mirrors `IO_CALLER_INVENTORY.md` §7's directory-partition pattern; **unlike IO, Video/Graphics callers are dominated by `ResolveExport`-by-name (§4.2), not `engine_funcs.h` globals, so batch conversion is "give it a real member call" not "retype a global pointer" — a different mechanical shape per `B34_RULES.md` §6.5(a)'s distinction** |

Blocks 1-6 are NOT confirmed disjoint by file the way `B34_RULES.md` §6.3 confirmed
STREAMS/PACKAGE+FS — the wave-lane files in particular (block 5) may cut across class
boundaries that blocks 1-4 also touch, since the `waveNN_lane_gfx*.cpp` naming suggests
opportunistic batching rather than one-class-per-file. **Confirming disjointness is
block 0's job**, not assumed here.

---

## Summary of what is VERIFIED vs NOT VERIFIED / INFERRED

VERIFIED this pass: file lists and line counts (`ls`, `wc -l`), registration counts
(`grep -c` on `RegisterRuntimeExport`/`RegisterRuntimeAddr`), all vtable/RT_DELDTOR/
typeInfo rows (grepped directly from `rt_data_mint_tables.inc` and `deleting_dtor.cpp`),
the cutover manifest's own `--table` output, `engine_funcs.h`'s Video/Graphics door
names and raw call counts, `ResolveExport` mangled-name call counts and file lists,
dx11/src file listing and its own README text, the two `_win`/SEH-comment cross-checks
against `IO_INVENTORY.md`'s findings.

Also VERIFIED (added after first draft): `docs/runtime_port/STATUS.md`'s Video/Graphics
history (§5 item 7 — three closed historical bugs, worth re-reading before step 5) and
`docs/reference/engine_architecture.md`'s single incidental hit (nothing load-bearing).

NOT VERIFIED / INFERRED (explicit list, all called out inline above too): exact
per-class base-class identity beyond the 18 typeInfo-row classes; `dx11_backend_access.h`
full contents; which `*.h` in `runtime/types/graphics` documents `Light`/`AmbientLight`/
`SpotLight`/`AnimationGroup`/`ReflectionImageBuffer`/`Renderer`'s layout; thread/
device-lost handling; the true (non-keyword-noisy) count of hand-inlined vtable slot
calls.
