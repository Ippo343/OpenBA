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

// One entry of a draw group's per-pass item lists, stride 0x38.
//
// WAVE 9 LANE GFX PINNED THIS. Lane F left it an opaque forward declaration
// ("an array-grow helper whose element stride lane F did not read"); the stride
// IS readable — the helper is 0x180063680 and it is the `Array<T>::Resize`
// instantiated for THIS element type, with `IMUL RCX,RCX,0x38` for the byte
// size and a seven-qword unrolled copy loop. 0x38 it is, and every field below
// is a store Renderer::AddDrawItem (0x1801057b0) makes into the stack record it
// hands that helper.
//
// TWO GAPS ARE REAL AND MUST STAY: AddDrawItem never writes `_pad24` (the four
// bytes between the int at +0x20 and the pointer at +0x28) or `_pad34` (the
// tail padding). The helper copies all seven qwords regardless, so retail
// stores UNINITIALISED STACK BYTES into the array at those two offsets on every
// append. A port must not zero them — and an oracle must not compare them
// (reference_oracle_must_not_compare_unwritten_bytes.md).
struct RendererDrawItem {
    const void* geometry;   // +0x00  the Geometry* argument
    const void* material;   // +0x08  MaterialInstance::material (+0x28), by value
    const void* instance;   // +0x10  the MaterialInstance* argument
    uint16_t    flags;      // +0x18  0x8000 when the 7th argument != 1.0f, OR'd
                            //        with the 4th argument (a 16-bit OR)
    uint16_t    arg5;       // +0x1a  the 5th argument
    float       sortKey;    // +0x1c  the 6th argument
    int32_t     arg8;       // +0x20  the 8th argument
    int32_t     _pad24;     // +0x24  NEVER WRITTEN — see the banner above
    const void* userData;   // +0x28  the 9th argument
    uint32_t    arg10;      // +0x30  the 10th argument
    int32_t     _pad34;     // +0x34  NEVER WRITTEN
};
static_assert(sizeof(RendererDrawItem) == 0x38, "Renderer draw item is 0x38");
static_assert(offsetof(RendererDrawItem, flags)    == 0x18, "drawItem::flags @ +0x18");
static_assert(offsetof(RendererDrawItem, sortKey)  == 0x1c, "drawItem::sortKey @ +0x1c");
static_assert(offsetof(RendererDrawItem, userData) == 0x28, "drawItem::userData @ +0x28");
static_assert(offsetof(RendererDrawItem, arg10)    == 0x30, "drawItem::arg10 @ +0x30");

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

// ############################################################################
// ENDGAME B.3 step 5 (block F) — `Graphics::Camera` and `Graphics::Renderer`,
// the REAL C++ classes, with the four element types of Renderer's three arrays
// ############################################################################
//
// `B35_RULES.md` §2.4/§3/§4/§6.1, plus SEVENTEEN NEW MEASUREMENTS block F made
// from the retail DLL (Runtime-x64.dll, 15290 functions, image base
// 0x180000000) this pass. Every one of the seventeen is an ACCESSOR whose whole
// body is one to six instructions, read as DISASSEMBLY (`disassemble_function`,
// never decompiled C), and between them they NAME eight fields the `RtGraphics`
// PODs above left as `field_NN` or as an opaque blob:
//
//   ?GetCurrentSceneGroup@Renderer@  0x180018030  MOV EAX,[RCX+0x30]
//   ?GetMaterialTypeFlags@Renderer@  0x180017ff0  group[cur] + 0x1bc
//   ?GetShader@Renderer@             0x180018010  group[cur] + 0x1b8
//   ?GetAlphaT@Renderer@             0x180018040  MOVSS XMM0,[RCX+0x40]
//   ?SetShaderDetail@Renderer@       0x180018050  MOV [RCX+0x3c],EDX
//   ?GetShaderDetail@Renderer@       0x180018060  MOV EAX,[RCX+0x3c]
//   ?GetDetailId@Renderer@           0x180018070  MOV EAX,[RCX+0x48]
//   ?GetFrameIndex@Renderer@         0x180018080  MOV EAX,[RCX+0x44]
//   ?GetSceneGroup@Renderer@         0x180018090  [RCX] + i*0x110
//   ?GetCamera@Renderer@ (XZ)        0x180017f70  drawGroups[cur] + 0x08
//   ?GetCamera@Renderer@ (H)         0x180017fb0  drawGroups[i]   + 0x08
//   ?GetCullCamera@Renderer@ (H)     0x180017fd0  drawGroups[i]   + 0xe0
//   ?BeginDrawGroup@Renderer@        0x180104fd0  (70 instructions, read whole)
//   ?GetCameraToWorld@Camera@ (XZ)   0x180017e70  12 dwords out of +0x64
//   ?GetCameraToWorld@Camera@ (M)    0x180017e10  IDENTICAL BODY, see below
//   ?GetProjection@Camera@           0x180017ed0  16 dwords out of +0x94
//   ?IsMirrored@Camera@              0x1800fe480  sign of det(cameraToWorld)
//   ?SetZUp@Camera@                  0x1800fe550  MOV [RCX+0xd4],DL
//   ??4Camera@                       0x180017c40  0x00..0x5f dwords, 0x60 BYTE,
//                                                 0x64..0xd0 dwords, 0xd4 BYTE
//
// -------------------------------------------------------------------------
// EIGHT CORRECTIONS TO THE PODs ABOVE. The POD field NAMES stay (they have
// live users in eight files outside block F's set, and B4b-1 only renames a POD
// that OCCUPIES the retail name — `RtGraphics` is a SIBLING of `Graphics`, so
// nothing has to move). The classes below carry the CORRECT names and every
// offset is cross-asserted against the POD, so the two cannot drift.
// -------------------------------------------------------------------------
//  1. `Renderer::contextSerial` (+0x44) is the FRAME INDEX.
//     `?GetFrameIndex@Renderer@Graphics@@QEBAHXZ` @0x180018080 is
//     `MOV EAX,[RCX+0x44]; RET`. The POD's comment ("Draw copies Context+0x08")
//     is not contradicted — it says what Context+0x08 IS.
//  2. `Renderer::field_48` is the DETAIL ID. `?GetDetailId@` @0x180018070.
//     `cull.cpp` already compares `geo->_detailId <= renderer->field_48`, which
//     is the same field in the same role from the other side.
//  3. `Renderer::field_3c` is the SHADER DETAIL, with BOTH accessors:
//     `?SetShaderDetail@` @0x180018050 writes it, `?GetShaderDetail@`
//     @0x180018060 reads it. It is still NOT WRITTEN BY THE CONSTRUCTOR
//     (RULE B5-13) — a settable-before-read field is exactly the shape that
//     licenses retail's gap, and the gap STAYS.
//  4. `Renderer::field_38` is DRAW GROUPS USED THIS FRAME — and that settles
//     B35_RULES §13 item 10, which recorded `standalone/scene_watch.h:73`'s
//     reading as "neither confirmed nor contradicted". `BeginDrawGroup`
//     (0x180104fd0) reads `[RCX+0x38]` as the index of the group it is about to
//     open, writes it to `currentDrawGroup` (+0x34), compares it against
//     `_drawGroups._size` (+0x18) to decide grow-versus-reuse, and then either
//     `INC dword ptr [RDI+0x38]` (grow) or `MOV [RDI+0x38], index+1` (reuse).
//     scene_watch.h was RIGHT.
//  5. `Renderer::frameAlpha` (+0x40) is retail's `alphaT`: `?GetAlphaT@`
//     @0x180018040 is `MOVSS XMM0,[RCX+0x40]` — a float, as the POD has it.
//  6. `Renderer::scenes` (+0x00) holds `Renderer::SceneGroup`, NOT
//     "SceneEntry". `?GetSceneGroup@Renderer@Graphics@@QEBAAEBUSceneGroup@12@H@Z`
//     @0x180018090 is `MOVSXD RAX,EDX; IMUL RAX,RAX,0x110; ADD RAX,[RCX]` —
//     the 0x110 stride and the +0x00 array together. The `@U` in the mangled
//     name makes `SceneGroup` a STRUCT nested in `Renderer@Graphics`. See the
//     naming note on `RendererSceneEntry` below.
//  7. `RendererDrawGroup::_head[0xe0]` IS NOT OPAQUE. It is
//     `{const Video::RenderBuffer* _renderBuffer; Camera _camera;}` — 8 + 0xd8
//     = 0xe0 exactly, with FOUR independent witnesses:
//       * `BeginDrawGroup` stores its `RenderBuffer*` argument as
//         `MOV qword ptr [RBX],RDX` and then runs `Camera::operator=`
//         (0x180017c40) on `RBX+0x08` and again on `RBX+0xe0`;
//       * `?GetCamera@Renderer@` returns `drawGroups[i] + 0x08` as a
//         `const Camera&` (0x180017f70 / 0x180017fb0);
//       * `closure_batch2b.cpp`'s draw-group copy constructor (0x18001be60)
//         copies +0x00 as one qword and copy-CONSTRUCTS a `Camera` at +0x08 and
//         at +0xe0 (CALL 0x180017a70 twice);
//       * `sun_camera.cpp`'s banner derives the same thing from
//         `GetSunCamera`'s reads of group+0x08 / group+0x6c.
//     So the 0x1f0 bytes are now FULLY accounted for and the blob is gone.
//  8. `RendererDrawGroup::passMask` (+0x1bc) is the group's MATERIAL TYPE
//     FLAGS: `?GetMaterialTypeFlags@Renderer@` @0x180017ff0 returns exactly
//     `drawGroups[currentDrawGroup] + 0x1bc`, and `Geometry::_materialTypeFlags`
//     (+0x30) is what `cull.cpp` and `Scene::AddGlobalDrawItems` AND it against.
//
// -------------------------------------------------------------------------
// A FINDING ON `Camera::GetCameraToWorld`, recorded because the two exports
// promise otherwise
// -------------------------------------------------------------------------
// `?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@M@Z` (0x180017e10)
// takes a `float` and IS INSTRUCTION FOR INSTRUCTION the no-argument overload
// (0x180017e70): twelve dwords out of +0x64 into the hidden return slot, then
// `MOV RAX,RDX`. The float NEVER APPEARS in the body. In a class with an
// interpolated camera the argument would be a lerp factor; `Graphics::Camera`
// has no previous-transform field for it to interpolate from, so the
// distinction the two signatures draw does not exist in the shipped binary —
// the same shape `renderer.cpp` already records for
// `SnapCameraToWorld` == `SetCameraToWorld`. Both are ported, separately, so a
// future divergence would show up instead of being assumed away.
//
// -------------------------------------------------------------------------
// RULE B5-4 — `Camera` IS CONCRETE, AND IT MUST STAY TRIVIALLY COPYABLE
// -------------------------------------------------------------------------
// `Graphics::Camera` appears BY VALUE three times, in TWO array element types:
// `RendererDrawGroup` twice (+0x08 `_camera`, +0xe0 `_cullCamera`) and
// `RendererSceneEntry` once (+0x10 `_camera`). Both element types live inside a
// `Collection::Array<T>` — `Renderer::_drawGroups` and `Renderer::_sceneGroups`
// — so B4b-3 applies: `Camera` is CONCRETE, its default constructor is
// `protected` and `= default`, and both holders are `friend`s.
//
// *** AND THE COPY CONSTRUCTOR AND `operator=` ARE `= default`, WHICH IS A
// DELIBERATE, RECORDED THREE-BYTE DEVIATION FROM RETAIL. *** Retail's two
// bodies are PARTIAL copies:
//
//   ??0Camera(const Camera&)  0x180017a70  53 dwords (+0x00..+0xd3) + the BYTE
//                                          at +0xd4. +0xd5..+0xd7 UNWRITTEN.
//   ??4Camera                 0x180017c40  dwords +0x00..+0x5f, the BYTE at
//                                          +0x60, dwords +0x64..+0xd0, the BYTE
//                                          at +0xd4. +0x61..+0x63 AND
//                                          +0xd5..+0xd7 UNWRITTEN.
//
// A compiler-generated copy moves all 0xd8 bytes, including those pads. The
// alternative — declaring both explicitly — would make the Camera-sized part of
// `RendererDrawGroup`'s and `RendererSceneEntry`'s memberwise copy a CALL rather
// than a blit, on the two types retail copies with a hand-written byte run
// (`SceneEntryAssign` 0x18001bdc0, the draw-group copy ctor 0x18001be60) and
// that `rtcoll::TAssign<T>` copies memberwise. A user copy constructor on a
// 0xd8 sub-object is a behaviour change waiting to happen; three never-read
// alignment pad bytes are not
// (`Volume.h`: "the pad bytes carry no meaning"; `Math.h`: `Frustum`'s
// +0x61..+0x63 exist only to satisfy alignment, and no retail call site reads
// past +0x60). So: `= default` here, and BOTH RETAIL BODIES KEEP THEIR
// REGISTERED DOORS — `RtGraphics_Camera_CopyCtor` (wave9_lane_gfxd.cpp) and the
// new `RtGraphics_Camera_Assign` (renderer.cpp) — so every copy the live engine
// makes is still retail's partial one. The deviation is reachable only from C++
// code that copies a `Graphics::Camera` directly, and nothing does.
//
// -------------------------------------------------------------------------
// NO REAL CONSTRUCTOR, NO REAL DESTRUCTOR (ENDGAME §B.1 decision 2, B4b-2,
// B35_RULES §11)
// -------------------------------------------------------------------------
// `??0Camera(const Frustum&, const Transform3&, bool)` (0x1800fe960),
// `??0Camera()` (0x1800fe7f0), `??0Renderer()` (0x180105180) and
// `??1Renderer()` (0x1800180a0) keep their doors — `fn_Graphics_Camera_Ctor`
// and `fn_Graphics_Renderer_Ctor` are named in B35_RULES §11's leave list. They
// are DECLARED below so no caller batch has to edit this header and left
// UNDEFINED; `Renderer`'s ctor additionally must not become an init-list,
// because RULE B5-13 requires +0x3c and +0x4c stay unwritten.
//
// RULE B5-7: nothing below adds a static initialiser, a lazy singleton or a
// non-atomic first-call cache. `Camera`, `Renderer` and `Scene` are constructed
// on the `Engine::Loader` thread (`code/Game/Game/gameupdateloop_load.cpp:570`,
// `:986`, `:1090`) while the main thread is inside `Draw`.
// ############################################################################

#include <type_traits>

// The classes other blocks own. Taken by pointer / reference only; the bridge,
// where one is needed, happens at the use site and never in this header.
namespace Video    { class Context; class RenderBuffer; }
namespace Graphics { class Geometry; class Light; class MaterialInstance; class Scene; }

namespace Graphics {

class RendererDrawGroup;
class RendererSceneEntry;
class Renderer;

// ============================================================
// Graphics::Camera — 0xd8 bytes, non-polymorphic, a ROOT (B35_RULES §2.1: no
// `??_7Camera@Graphics@@6B@`, no `_typeInfo`, every export `QEAA`/`QEBA`).
//
// Thirteen exports, ALL THIRTEEN NOW PORTED (B35_SLOTS.md's Camera block; the
// count corrects `renderer.cpp`'s old "all five of the class's exports", which
// `wave9_lane_gfxd.cpp` already flagged).
//
// The field names are the POD's, minus the leading-underscore convention, so a
// body moving out of a door reads `_frustum` where it read `self->frustum`.
// ============================================================
class Camera {
public:
    // Both copies are `= default` ON PURPOSE — see the banner's three-byte
    // deviation note. Retail's partial bodies stay behind their doors.
    Camera(const Camera& src) = default;                 // 0x180017a70  door: wave9_lane_gfxd.cpp
    Camera& operator=(const Camera& src) = default;       // 0x180017c40  door: renderer.cpp

    // `??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z`
    // @ 0x1800fe960 — PORTED, and it STAYS A DOOR (B4b-2, §11's leave list:
    // `fn_Graphics_Camera_Ctor` and its four call sites are not retired in
    // step 5). Declared so no caller batch has to edit this header, and
    // deliberately left UNDEFINED.
    Camera(const ::Frustum& frustum, const Math::Transform3& cameraToWorld, bool zUp);

    // ---- the ten non-constructor exports ----------------------------------
    // The four by-value returns are NON-VIRTUAL, so an ordinary C++ by-value
    // return is correct and safe: `Camera` has no vtable at all, both ends of
    // every member call are this same GCC, and each registered door keeps
    // retail's `(this, ret)` shape as a separate function that placement-news
    // the prvalue into the caller's slot. (A VIRTUAL by-value return would NOT
    // be safe — Geometry.h's B5-12 block, as amended by block E.)
    Math::Transform3 GetCameraToWorld() const;            // 0x180017e70  RETSLOT
    Math::Transform3 GetCameraToWorld(float t) const;     // 0x180017e10  RETSLOT, IGNORES t
    ::Frustum        GetFrustum() const;                  // 0x1800fe3e0  RETSLOT
    Math::Matrix     GetProjection() const;               // 0x180017ed0  RETSLOT
    bool             IsMirrored() const;                  // 0x1800fe480
    void             SetCameraToWorld(const Math::Transform3& cameraToWorld);   // 0x1800feb00
    void             SetFrustum(const ::Frustum& frustum);                      // 0x1800fe5c0
    void             SetZUp(bool zUp);                                          // 0x1800fe550
    void             SnapCameraToWorld(const Math::Transform3& cameraToWorld);  // 0x1800fe560

protected:
    // RULE B5-4 / B4b-3. `??0Camera@Graphics@@QEAA@XZ` (0x1800fe7f0) is a REAL
    // body — it seeds a unit-cube frustum, not zeros — and it keeps its door in
    // `wave9_lane_gfxd.cpp`. This `= default` is NOT that body: it exists only
    // so the three holders below can hold a `Camera` by value, exactly as
    // retail's own element initialiser (0x180062860) does by calling the door.
    // Nothing in the tree default-constructs a `Graphics::Camera` in C++.
    Camera() = default;
    friend class RendererDrawGroup;
    friend class RendererSceneEntry;

public:
    ::Frustum        _frustum;         // +0x00 .. +0x63
    Math::Transform3 _cameraToWorld;   // +0x64 .. +0x93
    Math::Matrix     _projection;      // +0x94 .. +0xd3
    // +0xd4 — `?SetZUp@Camera@Graphics@@QEAAX_N@Z` (0x1800fe550) is
    // `MOV byte ptr [RCX+0xd4],DL; RET`, so retail's own name for this byte is
    // Z-UP. The POD above calls it `swapZUpToYUp` and its long comment explains
    // WHAT it selects (whether `SetFrustum` post-multiplies the projection by
    // the fixed basis swap (x,y,z,w) -> (x,z,-y,w), i.e. a Z-up world into a
    // Y-up clip space) and why it is NOT "isPerspective". Both readings are the
    // same byte; the class takes retail's name and the POD's comment stands.
    uint8_t          _zUp;             // +0xd4
    uint8_t          _pad_d5[3];       // +0xd5..+0xd7 — NEVER WRITTEN by the
                                       // copy ctor, by `operator=` or by
                                       // `??0Camera()`. Alignment only.
};

static_assert(sizeof(Camera) == 0xd8, "Graphics::Camera is 0xd8 bytes");
static_assert(alignof(Camera) == 4, "Camera's widest member is a float");
static_assert(sizeof(Camera) == sizeof(RtGraphics::Camera),
              "the real class and the POD describe the same bytes");
// RULE B5-4's assert. What it buys, stated exactly: retail passes and returns a
// `Camera` as a plain 0xd8 blob (`GetSunCamera`/`GetSpotCamera` return one
// through a hidden return slot) and copies it with a partial dword/byte run, so
// the C++ class must carry no copy, assign or destroy step of its own.
//
// NOTE what is NOT claimed. `RendererDrawGroup` and `RendererSceneEntry` are NOT
// trivially copyable and never were, as PODs or as classes: each embeds a
// `Collection::Array<T>`, whose destructor is user-declared and whose
// `operator=` is retail's DEEP `ArrayAssignCore` (Collection.h:333 and its
// banner). `rtcoll::SpecFor<T>` therefore already derives `byteCopy = false` and
// a real `destroy` hook for both — which is exactly what renderer.cpp's
// `~Renderer` comment and `wave9_lane_gfxd.cpp`'s `kSpec_SceneEntry` rely on.
// The asserts below pin that half too, so an edit cannot quietly flip it.
static_assert(std::is_trivially_copyable_v<Camera>,
              "RULE B5-4: Camera is held BY VALUE in two Collection::Array "
              "element types; a non-trivial copy would change how array.cpp's "
              "trait-derived TAssign moves them");
static_assert(offsetof(Camera, _cameraToWorld) == offsetof(RtGraphics::Camera, cameraToWorld), "alias cameraToWorld");
static_assert(offsetof(Camera, _projection)    == offsetof(RtGraphics::Camera, projection),    "alias projection");
static_assert(offsetof(Camera, _zUp)           == offsetof(RtGraphics::Camera, swapZUpToYUp),  "alias swapZUpToYUp");
static_assert(offsetof(Camera, _cameraToWorld) == 0x64, "Camera::_cameraToWorld @ +0x64 (GetCameraToWorld 0x180017e70)");
static_assert(offsetof(Camera, _projection)    == 0x94, "Camera::_projection @ +0x94 (GetProjection 0x180017ed0)");
static_assert(offsetof(Camera, _zUp)           == 0xd4, "Camera::_zUp @ +0xd4 (SetZUp 0x1800fe550)");
static_assert(offsetof(Camera, _pad_d5)        == 0xd5, "Camera's three never-written pad bytes @ +0xd5");

// ============================================================
// Graphics::Renderer::Drawable — one entry of `Renderer::_drawables`, 0x10.
//
// `Renderer::AddLight` (0x1801051d0) writes TWELVE of the sixteen bytes —
// `_object` and `_value` — and the array's fill helper then copies all sixteen,
// so retail appends FOUR UNINITIALISED STACK BYTES at +0x0c on every append
// (wave29_lane_gfxbody.cpp's 12-of-16 note). `_pad0c` must never be zeroed and
// must never be compared.
// ============================================================
class RendererDrawable {
public:
    const void* _object;    // +0x00  the Geometry / Light the entry names
    int32_t     _value;     // +0x08  AddLight stores Renderer::_drawGroupsUsed
    int32_t     _pad0c;     // +0x0c  NEVER WRITTEN — see the banner
};
static_assert(sizeof(RendererDrawable) == 0x10, "Renderer::Drawable is 0x10");
static_assert(sizeof(RendererDrawable) == sizeof(RtGraphics::RendererDrawable), "alias sizeof");
static_assert(std::is_trivially_copyable_v<RendererDrawable>, "byteCopy element");
static_assert(offsetof(RendererDrawable, _value) == offsetof(RtGraphics::RendererDrawable, value), "alias value");
static_assert(offsetof(RendererDrawable, _pad0c) == 0x0c, "Drawable::_pad0c @ +0x0c");

// ============================================================
// Graphics::Renderer::DrawItem — one entry of a draw group's three per-pass
// item lists, 0x38. The POD's banner above is the evidence; TWO GAPS ARE REAL
// (RULE B5-13): `Renderer::AddDrawItem` (0x1801057b0) never writes `_pad24` or
// `_pad34`, and the fill helper copies all seven qwords regardless.
// ============================================================
class RendererDrawItem {
public:
    const void* _geometry;   // +0x00
    const void* _material;   // +0x08  MaterialInstance::material (+0x28), by value
    const void* _instance;   // +0x10  the MaterialInstance*
    uint16_t    _flags;      // +0x18
    uint16_t    _arg5;       // +0x1a
    float       _sortKey;    // +0x1c
    int32_t     _arg8;       // +0x20
    int32_t     _pad24;      // +0x24  NEVER WRITTEN
    const void* _userData;   // +0x28
    uint32_t    _arg10;      // +0x30
    int32_t     _pad34;      // +0x34  NEVER WRITTEN
};
static_assert(sizeof(RendererDrawItem) == 0x38, "Renderer::DrawItem is 0x38");
static_assert(sizeof(RendererDrawItem) == sizeof(RtGraphics::RendererDrawItem), "alias sizeof");
static_assert(std::is_trivially_copyable_v<RendererDrawItem>, "byteCopy element");
static_assert(offsetof(RendererDrawItem, _flags)    == offsetof(RtGraphics::RendererDrawItem, flags),    "alias flags");
static_assert(offsetof(RendererDrawItem, _sortKey)  == offsetof(RtGraphics::RendererDrawItem, sortKey),  "alias sortKey");
static_assert(offsetof(RendererDrawItem, _userData) == offsetof(RtGraphics::RendererDrawItem, userData), "alias userData");
static_assert(offsetof(RendererDrawItem, _arg10)    == offsetof(RtGraphics::RendererDrawItem, arg10),    "alias arg10");
static_assert(offsetof(RendererDrawItem, _pad24)    == 0x24, "DrawItem::_pad24 @ +0x24");
static_assert(offsetof(RendererDrawItem, _pad34)    == 0x34, "DrawItem::_pad34 @ +0x34");

// ============================================================
// Graphics::RendererDrawGroup — one draw group, 0x1f0, and the 0x1f0 bytes are
// now FULLY named (correction 7 in the banner).
//
// `BeginDrawGroup` (0x180104fd0) writes +0x00, then `Camera::operator=` over
// +0x08 and +0xe0, then +0x1b8 and +0x1bc. `RtGfxd_DrawGroupClearPasses`
// (0x1800626a0) clears the three item arrays and then zeroes +0x00..+0x07.
// NOTHING writes the two Cameras' own +0xd5..+0xd7 tails (at +0xdd..+0xdf and
// +0x1b5..+0x1b7), which is why they are pads on `Camera` and not fields.
// ============================================================
class RendererDrawGroup {
public:
    const Video::RenderBuffer* _renderBuffer;                 // +0x000
    Camera   _camera;                                         // +0x008  GetCamera(i)
    Camera   _cullCamera;                                     // +0x0e0  GetCullCamera(i)
    int32_t  _shader;                                         // +0x1b8  GetShader()
    uint32_t _materialTypeFlags;                              // +0x1bc  GetMaterialTypeFlags()
    Collection::Array<RendererDrawItem> _items[3];            // +0x1c0..+0x1ef
};
static_assert(sizeof(RendererDrawGroup) == 0x1f0, "Renderer draw group is 0x1f0");
static_assert(sizeof(RendererDrawGroup) == sizeof(RtGraphics::RendererDrawGroup), "alias sizeof");
// NOT trivially copyable, and never was — see Camera's assert block.
static_assert(!std::is_trivially_destructible_v<RendererDrawGroup>,
              "the three Collection::Array<RendererDrawItem> members are what "
              "SpecFor<T>'s destroy hook (and retail's 0x18001c8c0) free");
static_assert(offsetof(RendererDrawGroup, _camera)           == 0x008, "drawGroup::_camera @ +0x08 (BeginDrawGroup / GetCamera)");
static_assert(offsetof(RendererDrawGroup, _cullCamera)       == offsetof(RtGraphics::RendererDrawGroup, cullCamera), "alias cullCamera");
static_assert(offsetof(RendererDrawGroup, _shader)           == offsetof(RtGraphics::RendererDrawGroup, shader),     "alias shader");
static_assert(offsetof(RendererDrawGroup, _materialTypeFlags)== offsetof(RtGraphics::RendererDrawGroup, passMask),   "alias passMask");
static_assert(offsetof(RendererDrawGroup, _items)            == offsetof(RtGraphics::RendererDrawGroup, items),      "alias items");
static_assert(offsetof(RendererDrawGroup, _cullCamera)       == 0x0e0, "drawGroup::_cullCamera @ +0xe0 (GetCullCamera 0x180017fd0)");
static_assert(offsetof(RendererDrawGroup, _shader)           == 0x1b8, "drawGroup::_shader @ +0x1b8 (GetShader 0x180018010)");
static_assert(offsetof(RendererDrawGroup, _materialTypeFlags)== 0x1bc, "drawGroup::_materialTypeFlags @ +0x1bc (0x180017ff0)");
static_assert(offsetof(RendererDrawGroup, _items)            == 0x1c0, "drawGroup::_items @ +0x1c0");
// The two Cameras are exactly sizeof(Camera) apart, which is the embedding that
// pins sizeof(Camera) independently of Camera's own field list.
static_assert(offsetof(RendererDrawGroup, _cullCamera) - offsetof(RendererDrawGroup, _camera) == sizeof(Camera),
              "group+0x08 and group+0xe0 are two adjacent Cameras");

// ============================================================
// Graphics::RendererSceneEntry — one scene group, 0x110.
//
// *** RETAIL'S NAME FOR THIS TYPE IS `Graphics::Renderer::SceneGroup`. ***
// `?GetSceneGroup@Renderer@Graphics@@QEBAAEBUSceneGroup@12@H@Z` (0x180018090)
// returns `_sceneGroups._data + i*0x110` and the `@U` makes it a STRUCT nested
// in `Renderer@Graphics`. It is spelled at NAMESPACE scope here because the
// nesting cannot be expressed: `Camera` must `friend` this type (B5-4) and this
// type holds a `Camera` by value, so each would have to be complete before the
// other. `Renderer::SceneGroup` below is a member alias, so retail's spelling
// still names it. Recorded as a naming deviation, not an unknown.
//
// The three `RtGraphics` PODs and `wave9_lane_gfxd.cpp`'s ArraySpec label
// string ("Array<Graphics::Renderer::SceneEntry>") keep the old name: the label
// is debug output and the PODs have users outside block F.
//
// `SceneEntryInit` (0x180062860) and `SceneEntryAssign` (0x18001bdc0) leave
// +0xfc UNWRITTEN — plus the embedded Camera's +0xd5..+0xd7 tail, which lands
// at +0xe5..+0xe7. Both holes are real (wave9_lane_gfxd.cpp's banner).
// ============================================================
class RendererSceneEntry {
public:
    const Scene*               _scene;             // +0x000
    const Video::RenderBuffer* _renderBuffer;      // +0x008
    Camera                     _camera;            // +0x010
    // The five dwords `AddScene` (0x180105700) fills, in the order the header's
    // POD comment records them from `Renderer::Draw`'s reads. Named for the
    // Renderer field each is copied INTO, which is now an accessor-proved name
    // in every case: +0xe8 -> Renderer+0x3c (SetShaderDetail), +0xf8 ->
    // Renderer+0x48 (GetDetailId), and +0xec / +0xf0 are BeginDrawGroup's fifth
    // and sixth arguments, i.e. the group's `_shader` / `_materialTypeFlags`.
    int32_t                    _shaderDetail;      // +0x0e8  AddScene arg5
    int32_t                    _shader;            // +0x0ec  AddScene arg7
    uint32_t                   _materialTypeFlags; // +0x0f0  AddScene arg8
    int32_t                    _drawGroupIndex;    // +0x0f4  init -1; Draw writes it back
    int32_t                    _detailId;          // +0x0f8  AddScene arg6; init 0x40
    int32_t                    _pad_fc;            // +0x0fc  NEVER WRITTEN
    Collection::Array<int32_t> _drawableIds;       // +0x100..+0x10f
};
static_assert(sizeof(RendererSceneEntry) == 0x110, "Renderer::SceneGroup is 0x110");
static_assert(sizeof(RendererSceneEntry) == sizeof(RtGraphics::RendererSceneEntry), "alias sizeof");
static_assert(!std::is_trivially_destructible_v<RendererSceneEntry>,
              "_drawableIds is what kSpec_SceneEntry's destroy hook frees");
static_assert(offsetof(RendererSceneEntry, _camera)        == offsetof(RtGraphics::RendererSceneEntry, camera),         "alias camera");
static_assert(offsetof(RendererSceneEntry, _shaderDetail)  == offsetof(RtGraphics::RendererSceneEntry, field_e8),       "alias field_e8");
static_assert(offsetof(RendererSceneEntry, _drawGroupIndex)== offsetof(RtGraphics::RendererSceneEntry, drawGroupIndex), "alias drawGroupIndex");
static_assert(offsetof(RendererSceneEntry, _drawableIds)   == offsetof(RtGraphics::RendererSceneEntry, drawableIds),    "alias drawableIds");
static_assert(offsetof(RendererSceneEntry, _camera)        == 0x010, "sceneGroup::_camera @ +0x10");
static_assert(offsetof(RendererSceneEntry, _detailId)      == 0x0f8, "sceneGroup::_detailId @ +0xf8 (AddScene's out-of-order arg6)");
static_assert(offsetof(RendererSceneEntry, _pad_fc)        == 0x0fc, "sceneGroup::+0xfc is never written");
static_assert(offsetof(RendererSceneEntry, _drawableIds)   == 0x100, "sceneGroup::_drawableIds @ +0x100");
// The third Camera embedding, and the third independent pin on sizeof(Camera):
// Renderer::Draw reads the entry's next field at +0xe8 (Camera.h's own banner).
static_assert(offsetof(RendererSceneEntry, _camera) + sizeof(Camera) == 0x0e8,
              "0x10 + sizeof(Camera) == 0xe8");

// ============================================================
// Graphics::Renderer — 0x50 bytes, three arrays and eight scalars.
//
// Twenty-five exports (B35_SLOTS.md), and after this block TWENTY-THREE of them
// are ported. Where each body lives — checked against the tree, not assumed:
//
//   renderer.cpp             ctor, dtor, GetCullCamera×2, GetCamera×2,
//                            GetSceneGroup, GetShader, GetMaterialTypeFlags,
//                            GetCurrentSceneGroup, GetAlphaT, GetDetailId,
//                            GetFrameIndex, Get/SetShaderDetail   (block F's file)
//   wave9_lane_gfxd.cpp      Reset, AddScene                      (block F's file)
//   wave29_lane_gfxbody.cpp  AddLight, EndDrawGroup               (block F's file)
//   wave27_lane_gfxbig.cpp   Draw                                 NOT block F's
//   wave28_lane_gfxbody.cpp  BeginDrawGroup                       NOT block F's
//   wave9_lane_gfx.cpp       AddDrawItem                          NOT block F's
//   batchc_gfx_leaves.cpp    DrawItemList                         NOT block F's
//
// The four bodies in files block F does not own keep their doors untouched and are
// listed in its report; the caller batches and blocks G / the owner convert them.
// The only two exports with NO port anywhere are the copy constructor
// (0x1800180e0) and `operator=` (0x180018180), and neither has a caller: both are
// DECLARED below and deliberately left undefined.
//
// RULE B5-13: the constructor leaves +0x3c and +0x4c UNWRITTEN and this class
// gets NO constructor body that would "tidy" them.
// ============================================================
class Renderer {
public:
    // Retail's name for the 0x110 element type. See RendererSceneEntry's banner.
    using SceneGroup = RendererSceneEntry;
    using DrawGroup  = RendererDrawGroup;
    using DrawItem   = RendererDrawItem;
    using Drawable   = RendererDrawable;

    // ---- the four lifetime exports, ALL FOUR DOORS (B4b-2, §11) -----------
    Renderer();                                    // 0x180105180  door: renderer.cpp
    Renderer(const Renderer& src);                 // 0x1800180e0  not ported, no caller
    Renderer& operator=(const Renderer& src);      // 0x180018180  not ported, no caller
    ~Renderer();                                   // 0x1800180a0  door: renderer.cpp

    // ---- the four big bodies, ALL PORTED but NOT IN BLOCK F'S FILE SET -----
    // Declared here so no later block or caller batch has to edit this header,
    // and deliberately left UNDEFINED: each body is a door in a file block F may
    // not touch (see the banner's table), and whoever owns that file moves the
    // body onto the member. `Draw` is the whole frame.
    void AddDrawItem(const Geometry* geometry, const MaterialInstance* material,
                     uint16_t a, uint16_t b, float sortKey, float c, int d,
                     const void* userData, uint32_t e);                    // 0x1801057b0
    int  BeginDrawGroup(const Video::RenderBuffer* renderBuffer,
                        const Camera& camera, const Camera& cullCamera,
                        int shader, uint32_t materialTypeFlags);            // 0x180104fd0
    void Draw(Video::Context& context, float alphaT,
              const Scene& scene, const Camera& camera);                    // 0x180104bb0
    void DrawItemList(Video::Context& context, int a, int b, int c) const;  // 0x180105240

    // ---- the ported surface ------------------------------------------------
    void AddLight(const Light* light);                                      // 0x1801051d0
    void AddScene(const Scene* scene, const Video::RenderBuffer* renderBuffer,
                  const Camera& camera, int shaderDetail, int detailId,
                  int shader, uint32_t materialTypeFlags);                  // 0x180105700
    void EndDrawGroup(int group);                                           // 0x180104f50
    void Reset();                                                           // 0x180105110

    float                     GetAlphaT() const;                            // 0x180018040
    const Camera&             GetCamera() const;                            // 0x180017f70
    const Camera&             GetCamera(int group) const;                    // 0x180017fb0
    const Camera&             GetCullCamera() const;                         // 0x180017f90
    const Camera&             GetCullCamera(int group) const;                // 0x180017fd0
    int                       GetCurrentSceneGroup() const;                  // 0x180018030
    int                       GetDetailId() const;                           // 0x180018070
    int                       GetFrameIndex() const;                         // 0x180018080
    uint32_t                  GetMaterialTypeFlags() const;                  // 0x180017ff0
    const RendererSceneEntry& GetSceneGroup(int index) const;                // 0x180018090
    int                       GetShader() const;                             // 0x180018010
    int                       GetShaderDetail() const;                       // 0x180018060
    void                      SetShaderDetail(int detail);                   // 0x180018050

public:
    Collection::Array<RendererSceneEntry> _sceneGroups;  // +0x00  GetSceneGroup
    Collection::Array<RendererDrawGroup>  _drawGroups;   // +0x10
    Collection::Array<RendererDrawable>   _drawables;    // +0x20
    int32_t _currentSceneGroup;   // +0x30  GetCurrentSceneGroup; ctor -1
    int32_t _currentDrawGroup;    // +0x34  ctor -1
    int32_t _drawGroupsUsed;      // +0x38  BeginDrawGroup's cursor; ctor 0
                                  //        (B35_RULES §13 item 10, SETTLED)
    int32_t _shaderDetail;        // +0x3c  Get/SetShaderDetail. NOT WRITTEN BY
                                  //        THE CTOR — RULE B5-13.
    float   _alphaT;              // +0x40  GetAlphaT; ctor 0
    int32_t _frameIndex;          // +0x44  GetFrameIndex; ctor -1
    int32_t _detailId;            // +0x48  GetDetailId; ctor 0
    int32_t _pad_4c;              // +0x4c  NOT WRITTEN BY THE CTOR — B5-13.
};

static_assert(sizeof(Renderer) == 0x50, "Graphics::Renderer is 0x50 (80) bytes");
static_assert(alignof(Renderer) == 8, "Renderer's widest member is a pointer");
static_assert(sizeof(Renderer) == sizeof(RtGraphics::Renderer),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(Renderer, _drawGroups)        == offsetof(RtGraphics::Renderer, drawGroups),       "alias drawGroups");
static_assert(offsetof(Renderer, _drawables)         == offsetof(RtGraphics::Renderer, drawables),        "alias drawables");
static_assert(offsetof(Renderer, _currentSceneGroup) == offsetof(RtGraphics::Renderer, currentScene),     "alias currentScene");
static_assert(offsetof(Renderer, _currentDrawGroup)  == offsetof(RtGraphics::Renderer, currentDrawGroup), "alias currentDrawGroup");
static_assert(offsetof(Renderer, _drawGroupsUsed)    == offsetof(RtGraphics::Renderer, field_38),         "alias field_38");
static_assert(offsetof(Renderer, _shaderDetail)      == offsetof(RtGraphics::Renderer, field_3c),         "alias field_3c");
static_assert(offsetof(Renderer, _alphaT)            == offsetof(RtGraphics::Renderer, frameAlpha),       "alias frameAlpha");
static_assert(offsetof(Renderer, _frameIndex)        == offsetof(RtGraphics::Renderer, contextSerial),    "alias contextSerial");
static_assert(offsetof(Renderer, _detailId)          == offsetof(RtGraphics::Renderer, field_48),         "alias field_48");
static_assert(offsetof(Renderer, _pad_4c)            == offsetof(RtGraphics::Renderer, _pad_4c),          "alias _pad_4c");
// And against the retail bodies directly, so a POD edit cannot move both.
static_assert(offsetof(Renderer, _sceneGroups)       == 0x00, "Renderer::_sceneGroups @ +0x00 (GetSceneGroup 0x180018090)");
static_assert(offsetof(Renderer, _drawGroups)        == 0x10, "Renderer::_drawGroups @ +0x10 (GetCamera 0x180017f70)");
static_assert(offsetof(Renderer, _drawables)         == 0x20, "Renderer::_drawables @ +0x20");
static_assert(offsetof(Renderer, _currentSceneGroup) == 0x30, "Renderer::_currentSceneGroup @ +0x30 (0x180018030)");
static_assert(offsetof(Renderer, _currentDrawGroup)  == 0x34, "Renderer::_currentDrawGroup @ +0x34 (BeginDrawGroup 0x180104ffd)");
static_assert(offsetof(Renderer, _drawGroupsUsed)    == 0x38, "Renderer::_drawGroupsUsed @ +0x38 (BeginDrawGroup 0x18010509e INC)");
static_assert(offsetof(Renderer, _shaderDetail)      == 0x3c, "Renderer::_shaderDetail @ +0x3c (SetShaderDetail 0x180018050)");
static_assert(offsetof(Renderer, _alphaT)            == 0x40, "Renderer::_alphaT @ +0x40 (GetAlphaT 0x180018040, MOVSS)");
static_assert(offsetof(Renderer, _frameIndex)        == 0x44, "Renderer::_frameIndex @ +0x44 (GetFrameIndex 0x180018080)");
static_assert(offsetof(Renderer, _detailId)          == 0x48, "Renderer::_detailId @ +0x48 (GetDetailId 0x180018070)");
static_assert(offsetof(Renderer, _pad_4c)            == 0x4c, "Renderer::+0x4c is never written by the ctor (RULE B5-13)");

}  // namespace Graphics
