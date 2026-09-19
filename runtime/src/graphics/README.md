# runtime/src/graphics — original `code\Runtime\Graphics\`

Port target for the Runtime-x64.dll `Graphics` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/graphics/<file>.cpp`, with binary-compatible types in
`runtime/types/graphics/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (18 cpp, 10 h)

- `Animation.cpp`
- `AnimationGroup.h`
- `AnimationGroup_compile.cpp`
- `AnimationManager.cpp`
- `AnimationManager.h`
- `AnimationState.cpp`
- `EdgeMesh.h`
- `EdgeMesh_compile.cpp`
- `FrameCopyImageBuffer.cpp`
- `FrameCopyImageBuffer.h`
- `Geometry.cpp`
- `GraphicsMesh.h`
- `GraphicsMesh_pc.cpp`
- `GraphicsMeshInstance.cpp`
- `GraphicsScene.cpp`
- `GraphicsScene.h`
- `Material.h`
- `MaterialInstance.h`
- `Particle.cpp`
- `ParticleInstance.cpp`
- `ParticleSystem.cpp`
- `ParticleSystem.h`
- `ParticleSystemInstance.cpp`
- `ReflectionImageBuffer.cpp`
- `ReflectionImageBuffer.h`
- `Renderer.cpp`
- `SpotLight.cpp`
- `SunLight.cpp`

## Ported here

"Ported here: nothing yet" was this section for a long time and it was badly wrong —
`B35_RULES.md` §6.1 flags it as STALE and tells whoever first touches this directory to
fix it. Rewritten by ENDGAME B.3.5 block GC, 2026-09-19. Do not read the table below as
a completeness claim: it says which real C++ class each header declares and which files
in this directory hold that class's member bodies. Unported members are declared in the
header and left undefined, each with its own note there; ctor/dtor/`operator=` are
mostly still registered DOORS on purpose (B4b-2, `B35_RULES.md` §11).

| class (retail name) | header in `runtime/types/graphics/` | member bodies in this directory |
|---|---|---|
| `Geometry` | `Geometry.h` | `mesh.cpp`, `closure_batch2b.cpp`, `scene.cpp`, `cull.cpp` |
| `MeshInstance` | `Mesh.h` | `mesh.cpp`, `meshinstance_draw.cpp`, `animationstate_transforms.cpp`, `wave28_lane_gfxbody.cpp` |
| `EdgeMeshInstance` | `Mesh.h` | `mesh.cpp`, `edgemesh_vtable.cpp` |
| `EdgeMesh` | `Mesh.h` | `edgemesh_vtable.cpp` |
| `GraphicsMesh` | `GraphicsMesh.h` | `graphicsmesh_vtable.cpp`, `batchc_gfx_leaves.cpp` |
| `Material`, `MaterialInstance` | `Material.h` | `material.cpp`, `wave24_lane_gfxmat.cpp`, `wave28_lane_gfxbody.cpp`, `wave21_lane_gfxbody.cpp`, `wave27_lane_gfxbig.cpp`, `wave9_lane_gfx.cpp`, `batchc_gfx_leaves.cpp`, `renderer.cpp` |
| `Camera`, `Renderer` | `Camera.h` | `renderer.cpp`, `wave9_lane_gfxd.cpp`, `wave29_lane_gfxbody.cpp`, `wave28_lane_gfxbody.cpp`, `wave27_lane_gfxbig.cpp`, `wave9_lane_gfx.cpp`, `batchc_gfx_leaves.cpp` |
| `Scene` | `Scene.h` | `scene.cpp`, `scene_grid.cpp`, `wave29_lane_gfxbody.cpp` |
| `Light`, `AmbientLight`, `SpotLight` | `Light.h` | `lights_vtable.cpp`, `batchc_gfx_leaves.cpp` |
| `SunLight` | `SunLight.h` | `sunlight.cpp`, `sun_camera.cpp` |
| `Particle`, `ParticleSystem` | `Particle.h` | `vtbatch7_animgroup_particlesystem.cpp`, `wave27_lane_gfxpart.cpp`, `animation.cpp`, `graphics_typeinfo_accessors.cpp` |
| `ParticleInstance`, `ParticleSystemInstance` | `ParticleInstance.h` | `wave27_lane_gfxpart.cpp`, `wave28_lane_partsys.cpp`, `batchc_gfx_leaves.cpp` |
| `Animation`, `AnimationEvent` | `Animation.h` | `animation.cpp`, `animationstate.cpp`, `wave28_lane_gfxbody.cpp`, `wave29_lane_gfxbody.cpp`, `graphics_typeinfo_accessors.cpp` |
| `AnimationGroup` | `AnimationGroup.h` | `vtbatch7_animgroup_particlesystem.cpp` |
| `AnimationState` | `AnimationState.h` | `animationstate.cpp`, `animationstate_transforms.cpp`, `wave27_lane_gfxbig.cpp`, `wave28_lane_gfxbody.cpp` |
| `AnimationManager` | `AnimationManager.h` | `animationstate.cpp`, `wave23_lane_gfxbody.cpp`, `wave24_lane_gfxmat.cpp` |
| `GeneratedImageBuffer`, `ReflectionImageBuffer` | `GeneratedImageBuffer.h` | `imagebuffer_derived.cpp` |
| `FrameCopyImageBuffer` | `FrameCopyImageBuffer.h` | `imagebuffer_derived.cpp`, `wave23_lane_gfxbody.cpp` |

`rt_graphics_port.cpp` is this directory's registration file and holds no bodies;
`rt_factories_graphics.{cpp,inc}` are the reflection factory rows.

THE `wave*`/`batchc*`/`closure*`/`vtbatch*` FILE NAMES ARE NOT ORIGINAL SOURCE FILES.
They are per-lane porting batches from before B.3, and each one now holds members of
whatever class its bodies belonged to — which is why several classes above list five or
six files. Consolidating them onto the original `Renderer.cpp` / `Material.h` shape at
the top of this README is a later, purely mechanical move; nothing depends on the
current names except the Makefile's wildcard.
