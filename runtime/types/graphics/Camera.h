#pragma once
// ============================================================================
// runtime/types/graphics/Camera.h — Graphics::Camera and Graphics::Renderer
// ============================================================================
//
// Wave 6 lane F of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Every offset below was read out of the shipped Runtime-x64.dll in Ghidra; the
// function that pins it is named next to it. Nothing here is inferred from a
// header name or from the profile build's source-file list.
//
// The lane write-up, including everything that is deliberately NOT pinned, is
// docs/runtime_port/WAVE6_LANE_F.md.
//
// ABI NOTE — inherited from runtime/types/math/Math.h and repeated because it
// bites in this file: `Camera::GetFrustum` returns a `Volume::Frustum` BY VALUE,
// so MSVC passes a hidden return-slot pointer in RDX after `this` in RCX and
// returns that same pointer in RAX. A typedef that omits the slot is a wild
// write that still returns the right answer
// (reference_hidden_retslot_wild_write.md). Ports in renderer.cpp spell the slot
// out as an explicit parameter.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../math/Math.h"                 // Matrix, Transform3
#include "../volume/Volume.h"             // Frustum
#include "../collection/Collection.h"     // Collection::Array

namespace RtGraphics {

// ============================================================
// Graphics::Camera — a frustum, a camera-to-world transform, the projection
// matrix derived from the two, and one flag.
//
// Pinned by three functions, which between them name every byte:
//   ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z
//     (0x1800fe960) copies 25 dwords from the Frustum argument to +0x00..+0x63,
//     12 dwords from the Transform3 argument to +0x64..+0x93, zeroes the 16
//     dwords +0x94..+0xd3, and stores the bool argument at +0xd4.
//   ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z (0x1800fe5c0)
//     writes the projection matrix into +0x94..+0xd3 and branches on +0xd4.
//   ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ (0x1800fe3e0)
//     copies exactly 25 dwords out of +0x00.
//
// sizeof is pinned INDEPENDENTLY of the field list, by two embeddings:
//   * Renderer::Draw (0x180104bb0) treats a scene entry's +0x10 as a Camera and
//     reads the entry's own next field at +0xe8 — so 0x10 + sizeof == 0xe8.
//   * ?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ (0x180017f90)
//     returns drawGroup+0xe0 as a Camera and ?GetShader@Renderer@Graphics@@QEBAHXZ
//     (0x180018010) reads the group's next field at +0x1b8 — 0xe0 + sizeof == 0x1b8.
// Both give sizeof(Camera) == 0xd8, which is also what natural alignment of the
// field list produces (0xd5 rounded up to 4).
// ============================================================
struct Camera {
    Frustum    frustum;         // +0x00 .. +0x63
    Transform3 cameraToWorld;   // +0x64 .. +0x93
    Matrix     projection;      // +0x94 .. +0xd3  (frustum projection, already
                                //                  post-multiplied when the
                                //                  flag below is set)
    // +0xd4 — the ctor's third argument. NOT "isPerspective", which is what
    // engine/funcs/engine_funcs.h:3174 calls it: `Frustum` carries its own
    // orthographic/perspective flag at its +0x60 and SetFrustum never looks at
    // this byte for that purpose. What it actually selects is whether the
    // projection matrix gets post-multiplied by a fixed basis swap
    //     (x, y, z, w) -> (x, z, -y, w)
    // i.e. a Z-up world into a Y-up clip space. See kZUpToYUp in renderer.cpp,
    // whose four rows are built inline in SetFrustum at 0x1800fe5f0..0x1800fe6a3.
    uint8_t    swapZUpToYUp;    // +0xd4
    uint8_t    _pad_d5[3];      // +0xd5..+0xd7, alignment only; never read.
};
static_assert(sizeof(Camera) == 0xd8, "Graphics::Camera must be 0xd8 (216) bytes");
static_assert(offsetof(Camera, cameraToWorld) == 0x64, "Camera::cameraToWorld @ +0x64");
static_assert(offsetof(Camera, projection)    == 0x94, "Camera::projection @ +0x94");
static_assert(offsetof(Camera, swapZUpToYUp)  == 0xd4, "Camera::swapZUpToYUp @ +0xd4");

// ============================================================
// The three element types the Renderer's arrays hold. Lane F ports only the
// Renderer ctor, dtor and the two accessors, so these are pinned only as far as
// those four functions and `Draw`'s decompilation require. Where a field is not
// named by a function this lane read, it stays an explicit blob rather than a
// guess — a wrong field name is worse than no field name (PLAN §6 step 8).
// ============================================================

// One entry of Renderer::_drawables, stride 0x10.
// Pinned by Renderer::Draw (0x180104bb0): `*(void**)(base + idx*0x10)` is
// dispatched through as an object with virtuals at +0x48 and +0x50, and
// `*(int*)(base + idx*0x10 + 8)` is passed alongside it as an index.
struct RendererDrawable {
    void*   object;    // +0x00  the Geometry-side drawable
    int32_t value;     // +0x08
    int32_t _pad0c;    // +0x0c
};
static_assert(sizeof(RendererDrawable) == 0x10, "Renderer drawable entry is 0x10");

// One entry of a draw group's per-pass item lists. NOT pinned by this lane:
// Renderer::AddDrawItem (0x1801057b0) builds a 0x38-byte stack record and hands
// it to an array-grow helper whose element stride lane F did not read, and
// AddDrawItem is HELD BACK (see WAVE6_LANE_F.md), so nothing here depends on it.
struct RendererDrawItem;

// One draw group, stride 0x1f0. Pinned by GetCullCamera (0x180017f90),
// GetShader (0x180018010), AddDrawItem (0x1801057b0, which indexes
// `group + 0x1c0 + passIndex*0x10` with passIndex from MaterialInstance+0x20)
// and the group-array destructor at 0x18001c8c0, which runs a 3-element vector
// destructor over `group + 0x1c0` with stride 0x10 and then frees the buffer.
struct RendererDrawGroup {
    uint8_t  _head[0xe0];                             // +0x000  not read by lane F
    Camera   cullCamera;                              // +0x0e0
    int32_t  shader;                                  // +0x1b8
    uint32_t passMask;                                // +0x1bc
    Collection::Array<RendererDrawItem> items[3];     // +0x1c0..+0x1ef
};
static_assert(sizeof(RendererDrawGroup) == 0x1f0, "Renderer draw group is 0x1f0");
static_assert(offsetof(RendererDrawGroup, cullCamera) == 0x0e0, "drawGroup::cullCamera @ +0xe0");
static_assert(offsetof(RendererDrawGroup, shader)     == 0x1b8, "drawGroup::shader @ +0x1b8");
static_assert(offsetof(RendererDrawGroup, passMask)   == 0x1bc, "drawGroup::passMask @ +0x1bc");
static_assert(offsetof(RendererDrawGroup, items)      == 0x1c0, "drawGroup::items @ +0x1c0");

// One scene entry, stride 0x110. Pinned by Renderer::Draw (0x180104bb0), which
// reads every one of these fields, and by AddDrawItem, which reads +0x00 and
// +0x10 as the scene and its camera.
struct RendererSceneEntry {
    void*    scene;                            // +0x000  Graphics::Scene*
    void*    renderBuffer;                     // +0x008  Video::RenderBuffer*
    Camera   camera;                           // +0x010
    int32_t  field_e8;                         // +0x0e8  copied to Renderer+0x3c
    int32_t  field_ec;                         // +0x0ec  BeginDrawGroup arg
    uint32_t field_f0;                         // +0x0f0  BeginDrawGroup arg
    int32_t  drawGroupIndex;                   // +0x0f4  written back from Renderer+0x34
    int32_t  field_f8;                         // +0x0f8  copied to Renderer+0x48
    int32_t  _pad_fc;                          // +0x0fc
    Collection::Array<int32_t> drawableIds;    // +0x100..+0x10f
};
static_assert(sizeof(RendererSceneEntry) == 0x110, "Renderer scene entry is 0x110");
static_assert(offsetof(RendererSceneEntry, camera)         == 0x010, "sceneEntry::camera @ +0x10");
static_assert(offsetof(RendererSceneEntry, field_e8)       == 0x0e8, "sceneEntry::field_e8 @ +0xe8");
static_assert(offsetof(RendererSceneEntry, drawGroupIndex) == 0x0f4, "sceneEntry::drawGroupIndex @ +0xf4");
static_assert(offsetof(RendererSceneEntry, drawableIds)    == 0x100, "sceneEntry::drawableIds @ +0x100");

// ============================================================
// Graphics::Renderer — three arrays and eight scalars, 0x50 bytes.
//
// Pinned by ??0Renderer@Graphics@@QEAA@XZ (0x180105180), which writes
// +0x00..+0x2f as zero qwords, 0xffffffffffffffff over +0x30/+0x34, 0 at +0x38,
// 0 at +0x40, -1 at +0x44 and 0 at +0x48; by ??1Renderer@Graphics@@QEAA@XZ
// (0x1800180a0), which destroys the three 0x10-byte array triples at +0x20,
// +0x10 and +0x00 in that order; and by Renderer::Draw and the two accessors
// for the scalars.
//
// TWO GAPS ARE REAL, not modelling laziness: the ctor never writes +0x3c or
// +0x4c. `Draw` assigns +0x3c from a scene entry before anything reads it, so
// the uninitialised dword is harmless in retail — but a port must not
// "helpfully" zero it, because then a byte comparison of a freshly constructed
// Renderer against the DLL's would diverge on whatever the caller's memory
// happened to hold. renderer.cpp reproduces the gap exactly.
//
// sizeof 0x50 also matches the pre-existing note at
// engine/funcs/engine_funcs.h:3081 ("Object size 0x50, alignment 4").
// ============================================================
struct Renderer {
    Collection::Array<RendererSceneEntry> scenes;      // +0x00..+0x0f
    Collection::Array<RendererDrawGroup>  drawGroups;  // +0x10..+0x1f
    Collection::Array<RendererDrawable>   drawables;   // +0x20..+0x2f
    int32_t currentScene;      // +0x30  ctor: -1
    int32_t currentDrawGroup;  // +0x34  ctor: -1
    int32_t field_38;          // +0x38  ctor: 0. standalone/scene_watch.h:73 reads
                               //        this as "draw groups used this frame";
                               //        lane F ported no function that touches it,
                               //        so that reading is neither confirmed nor
                               //        contradicted here.
    int32_t field_3c;          // +0x3c  NOT written by the ctor
    float   frameAlpha;        // +0x40  ctor: 0    (Draw's `float` argument)
    int32_t contextSerial;     // +0x44  ctor: -1   (Draw copies Context+0x08)
    int32_t field_48;          // +0x48  ctor: 0
    int32_t _pad_4c;           // +0x4c  NOT written by the ctor
};
static_assert(sizeof(Renderer) == 0x50, "Graphics::Renderer must be 0x50 (80) bytes");
static_assert(offsetof(Renderer, drawGroups)       == 0x10, "Renderer::drawGroups @ +0x10");
static_assert(offsetof(Renderer, drawables)        == 0x20, "Renderer::drawables @ +0x20");
static_assert(offsetof(Renderer, currentScene)     == 0x30, "Renderer::currentScene @ +0x30");
static_assert(offsetof(Renderer, currentDrawGroup) == 0x34, "Renderer::currentDrawGroup @ +0x34");
static_assert(offsetof(Renderer, frameAlpha)       == 0x40, "Renderer::frameAlpha @ +0x40");
static_assert(offsetof(Renderer, contextSerial)    == 0x44, "Renderer::contextSerial @ +0x44");

}  // namespace RtGraphics
