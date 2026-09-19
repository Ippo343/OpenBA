#pragma once
// ============================================================================
// runtime/types/graphics/Mesh.h — binary layouts for the Graphics mesh family
// ============================================================================
//
// Wave 6, lane E of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// §6 step 3). Covers `Graphics::Geometry` (the shared base), `MeshInstance`,
// `GraphicsMesh` (only the fields lane E reads), `EdgeMesh`, `EdgeMeshInstance`
// and the two nested `EdgeMesh` vertex structs.
//
// EVERY offset below was read out of a decompiled or disassembled RETAIL
// Runtime-x64.dll body, and the function plus the instruction that pins it is
// named next to the field. Nothing here is taken from the profile build: PLAN
// §6 warns that profile NAMES win but profile OFFSETS must not
// (reference_encoding_vtable_offsets_differ_between_builds.md).
//
// The full write-up is docs/runtime_port/WAVE6_LANE_E.md.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../../../engine/types/common.h"   // Vector3
#include "../math/Math.h"                   // Quaternion, Transform
#include "../volume/Volume.h"               // Box
#include "../collection/Collection.h"       // Collection::Array

// Graphics::Geometry is Wave 6 LANE D's type and lives in its header. lane E
// only embeds it (MeshInstance and EdgeMeshInstance both derive from it) and
// must not re-declare it: two definitions of RtGraphics::Geometry in one
// namespace across two translation units is an ODR violation, and the drift
// between them would be invisible until a field moved.
#include "Geometry.h"                       // RtGraphics::Geometry (lane D)

namespace RtGraphics {

// `RtGraphics::ExternalPtr` — the 8-byte handle — MOVED TO Geometry.h by
// ENDGAME B.3.5 block E, unchanged and with its whole banner. The real
// `Graphics::Geometry` class needs it for slot 2's `Draw` parameter, and
// Geometry.h is included ABOVE this point, so it cannot live here any more.
// Every user of `Mesh.h` still sees it, transitively, exactly as before.

// ============================================================================
// Graphics::InterpolatedState<Transform> — 0x38 bytes, two Transforms.
//
// Pinned by MeshInstance::UpdateHierarchy (retail 0x1800ff910): the node array
// walk advances 0x38 per node (`ADD RSI,0x38`), the first seven dwords are
// written from the geometry transform / the computed world transform, and the
// next seven are then copied verbatim from the first seven
// (`*(base+0x1c+k) = *(base+k)` for k in 0..6, decompiled at the tail of the
// loop). So the element is exactly two adjacent 0x1c-byte Transforms.
//
// WHICH HALF IS WHICH — SETTLED BY WAVE 9 LANE GFX, 2026-09-08. Lane E left
// this open ("do not read `_state[0]` as current without proving it") and named
// `InterpolatedState<T>::Lerp` as the function that would settle it. That
// function is 0x18001aff0, eleven instructions, and it settles it:
//
//     MOV R9,R8        ; R9  = the Scalar& argument (t)
//     MOV R8,RCX       ; R8  = this + 0x00        -> Transform::Lerp's `to`
//     ADD RCX,0x1c     ; RCX = this + 0x1c        -> Transform::Lerp's `this`
//     MOV RBX,RDX      ; RDX = the hidden return slot
//     CALL 0x18001d690 ; Math::Transform::Lerp(from, ret, to, t)
//
// `Math::Transform::Lerp` (0x18001d690, exported as ord 3193) computes
// `from + t*(to - from)` on the translation and `Quaternion::Lerp(from, to, t)`
// on the rotation — `this` is the FROM end and the argument is the TO end,
// pinned by the SUBSS/MULSS/ADDSS order at 0x18001d6d8..0x18001d6fa. So the
// interpolation runs FROM `_state[1]` (+0x1c) TO `_state[0]` (+0x00): +0x00 is
// the value the sim just produced (CURRENT) and +0x1c is the one it replaced
// (PREVIOUS), which is the ordinary render-interpolation arrangement and agrees
// with GetNodeToWorld reading the first one.
// ============================================================================
struct InterpolatedTransform {
    Transform _state[2];   // [0] = +0x00 CURRENT, [1] = +0x1c PREVIOUS
};
static_assert(sizeof(InterpolatedTransform) == 0x38,
              "InterpolatedState<Transform> must be 0x38 bytes");

// ============================================================================
// Graphics::Geometry — the base of MeshInstance and EdgeMeshInstance — is
// declared in runtime/types/graphics/Geometry.h by Wave 6 lane D, included
// above. Lane E confirmed the offsets it needs against the same retail bodies
// and agrees with lane D on all of them:
//   +0x2c _dirtyFlags     MeshInstance::SetMesh ORs kDirtyBounds (2) when the
//                         local AABB it copied out of the mesh differs.
//   +0x30 _materialTypeFlags  MeshInstance::SetMesh and EdgeMeshInstance::
//                         SetMesh both OR `1 << *(int*)(material + 0x20)`.
//   +0x34 _localBounds    MeshInstance::SetMesh copies GraphicsMesh+0x80..+0x94
//                         (six floats) here field by field.
//   +0x84 _transform      MeshInstance::UpdateHierarchy seeds node 0 from these
//                         seven floats; GetNodeToWorld reads the rotation at
//                         +0x84 and the translation at +0x94.
// Lane E deliberately uses lane D's NAMES (_detailId, _opacity, _opacityPrev)
// rather than the ones this lane would have guessed from AddDrawItems alone
// (_sortBias, _fadeA, _fadeB) — lane D's come from the exported getters
// ?GetDetailId@ / ?GetOpacity@, which is the stronger evidence.
// ============================================================================

// ============================================================================
// Graphics::AnimationState — 0x18 bytes as MeshInstance embeds it.
//
// Spelled `MeshAnimationState` here, not `AnimationState`. The class belongs to
// Wave 6 lane G (runtime/types/graphics/Animation.h); if that lane declares the
// real thing under its own name, two definitions of one name in RtGraphics
// would be an ODR violation the moment a header pulled both in. This is the
// three-field read-only VIEW lane E needs, named so it cannot collide.
//
// NOT PORTED BY THIS LANE (it is lane G's class). Declared for the three fields
// lane E's bodies read:
//   +0x00 a bool. ??0MeshInstance stores a single BYTE here (`MOV byte ptr
//         [RAX],CL` at 0x180100009, RAX = this+0xd8). GetNodeToWorld and
//         RootAnimationOffset branch on it: zero means "read the instance's own
//         node array / root offset", non-zero means "read the AnimationManager
//         instance pool".
//   +0x08 the layer array buffer, +0x10 its count. IsAnimating
//         (0x1800ff7d0) reads count at MeshInstance+0xe8 and buffer at +0xe0;
//         GetNodeToWorld reads `layers[count-1]` at stride 0x38, field +0x0c.
// The 0x38 layer stride and the field roles are documented in
// standalone/perf_experimental.cpp and are consistent with every read here.
// ============================================================================
struct MeshAnimationState {
    uint8_t _instanced;                     // +0x00
    uint8_t _pad01[7];                      // +0x01
    Collection::Array<uint8_t> _layers;     // +0x08 data, +0x10 size, +0x14 capacity
};
static_assert(sizeof(MeshAnimationState) == 0x18, "Graphics::AnimationState (as embedded) must be 0x18 bytes");
static_assert(offsetof(MeshAnimationState, _layers) == 0x08, "AnimationState::_layers @ +0x08");

// One entry of AnimationState's layer array. Only the pool id is read by lane E
// (GetNodeToWorld / RootAnimationOffset / AddDrawItems all read
// `layers[count-1] + 0x0c`, spelled in the decompile as
// `*(int*)(count*0x38 - 0x2c + buffer)`); the rest is named from
// standalone/perf_experimental.cpp's documented walk and is NOT pinned here.
struct MeshAnimationLayer {
    ExternalPtr _group;        // +0x00
    int32_t     _animIndex;    // +0x08
    int32_t     _poolId;       // +0x0c  <- the only field lane E reads
    uint8_t     _rest[0x28];   // +0x10 .. +0x37 (time at +0x18, blend at +0x20/+0x28)
};
static_assert(sizeof(MeshAnimationLayer) == 0x38, "AnimationState layer stride must be 0x38");
static_assert(offsetof(MeshAnimationLayer, _poolId) == 0x0c, "AnimationLayer::_poolId @ +0x0c");

// ============================================================================
// Graphics::MeshInstance — 0x120 bytes.
//
// Pinned field-by-field by ??0MeshInstance@Graphics@@QEAA@XZ (retail
// 0x1800fffc0, ordinal 275), disassembled: after the Geometry base ctor it
// writes qwords at +0xc0/+0xc8/+0xd0, a BYTE at +0xd8, qwords at +0xe0/+0xe8
// (through `LEA RAX,[RBX+0xd8]`), qwords at +0xf0/+0xf8/+0x100, a dword at
// +0x108, four 0x3f800000 dwords at +0x10c..+0x118 and a WORD at +0x11c. The
// last write being a word is what fixes the size at 0x120 rather than 0x11c.
//
// CROSS-CHECK: standalone/address_registry.cpp:1343 calls the array element
// StorageDisplayComponent reallocates "0x128-byte" and says it hand-copies
// "sceneId at +0x120". That is the EXE's array element (a MeshInstance plus a
// trailing int), not sizeof(MeshInstance); the two numbers agree.
// ============================================================================
struct MeshInstance {
    Geometry _base;                                   // +0x00 .. +0xbf
    ExternalPtr _mesh;                                // +0xc0  ExternalPtr<GraphicsMesh>
    Collection::Array<ExternalPtr> _materials;        // +0xc8 data, +0xd0 size, +0xd4 capacity
    MeshAnimationState _animation;                        // +0xd8 .. +0xef
    Collection::Array<InterpolatedTransform> _nodes;  // +0xf0 data, +0xf8 size, +0xfc capacity
    Vector3 _rootAnimationOffset;                     // +0x100 .. +0x10b
    float   _color[4];                                // +0x10c ctor: 1,1,1,1
    int16_t _sortBias;                                // +0x11c ctor: 0. AddDrawItems
                                                      //        adds it to Geometry::_sortBias.
    int16_t _pad11e;                                  // +0x11e
};
static_assert(sizeof(MeshInstance) == 0x120, "Graphics::MeshInstance must be 0x120 bytes");
static_assert(offsetof(MeshInstance, _mesh)                == 0xc0,  "MeshInstance::_mesh @ +0xc0");
static_assert(offsetof(MeshInstance, _materials)           == 0xc8,  "MeshInstance::_materials @ +0xc8");
static_assert(offsetof(MeshInstance, _animation)           == 0xd8,  "MeshInstance::_animation @ +0xd8");
static_assert(offsetof(MeshInstance, _nodes)               == 0xf0,  "MeshInstance::_nodes @ +0xf0");
static_assert(offsetof(MeshInstance, _rootAnimationOffset) == 0x100, "MeshInstance::_rootAnimationOffset @ +0x100");
static_assert(offsetof(MeshInstance, _color)               == 0x10c, "MeshInstance::_color @ +0x10c");
static_assert(offsetof(MeshInstance, _sortBias)            == 0x11c, "MeshInstance::_sortBias @ +0x11c");

// ============================================================================
// Graphics::GraphicsMesh::Node — 0x48 bytes.
//
// Stride pinned twice: GetNodeIndexFromName (0x1800fee90) advances its cursor
// by nine qwords per node (`puVar8 = puVar8 + 9`), and UpdateHierarchy
// (0x1800ff910) advances by 0x48 (`ADD RBP,0x48`).
//   +0x00 the node name, a plain `char*` — GetNodeIndexFromName strcmp's it
//         directly (it dereferences `*puVar8` as the first byte of a string).
//   +0x08 the parent node index — UpdateHierarchy reads
//         `*(int*)(nodes + 8 + 0x48*i)` and uses it to index _nodes.
//   +0x0c the node-local Transform. `LEA R8,[RCX+0xc]` at 0x1800ffa50 makes
//         R8 the third argument of Quaternion::operator* (the local rotation),
//         and the same R8 is read at +0x10/+0x14/+0x18 for the local
//         translation — i.e. a Transform at +0x0c whose translation sits at
//         +0x1c, exactly Math::Transform's own layout.
// +0x28 .. +0x47 is NOT pinned by this lane; no lane-E body touches it.
// ============================================================================
struct MeshNode {
    const char* _name;        // +0x00
    int32_t     _parent;      // +0x08
    Transform   _local;       // +0x0c .. +0x27
    uint8_t     _unpinned[0x20];  // +0x28 .. +0x47
};
static_assert(sizeof(MeshNode) == 0x48, "GraphicsMesh::Node must be 0x48 bytes");
static_assert(offsetof(MeshNode, _parent) == 0x08, "GraphicsMesh::Node::_parent @ +0x08");
static_assert(offsetof(MeshNode, _local)  == 0x0c, "GraphicsMesh::Node::_local @ +0x0c");

// ============================================================================
// Graphics::GraphicsMesh — PARTIAL. Only the fields lane E reads are declared;
// the class is far larger and no lane-E function constructs one, so this is a
// read-only overlay, not a definition. Do not `sizeof` it.
//
//   +0x50 Array<DetailLevel>, element 0xb0 — AddDrawItems (held back, see the
//         .cpp) indexes it by `level * 0xb0` and reads the level count at +0x58.
//   +0x60 Array<Node>, element 0x48 — GetNodeIndexFromName walks buffer +0x60
//         with count +0x68; MeshInstance::SetMesh resizes the instance node
//         array to `*(int*)(mesh+0x68)`; PlayAnimation passes `mesh + 0x60` as
//         the `const Array<Node>&` argument of AnimationState::PlayAnimation.
//   +0x70 the per-detail-level material Array. GetMaterialList(int)
//         (0x1800126c0) is exactly `*(mesh+0x70) + index*0x10` — index by the
//         0x10-byte Array header, so +0x70 is an ARRAY OF ARRAYS.
//
//         *** +0x70 IS A POINTER, +0x60 IS AN EMBEDDED ARRAY, AND THE TWO ARE
//         PASSED TO RUNTIME HELPERS DIFFERENTLY. *** `_nodes` at +0x60 is an
//         Array object living inside GraphicsMesh, so retail hands it over as
//         `mesh + 0x60` (its address). `_materialLists` at +0x70 is a pointer
//         TO such an array, so retail hands it over as `*(mesh + 0x70)` (the
//         loaded value). Confusing the two is silent whenever the garbage count
//         that gets read happens to be 0; it cost this lane its first
//         integration run (WAVE6_LANE_E.md §6.6). The field is declared as a
//         real pointer type here, not as a byte offset, so the compiler makes
//         the mistake impossible to repeat at this site.
//   +0x80 the mesh AABB — MeshInstance::SetMesh copies six floats from
//         +0x80..+0x94 into Geometry::_localBounds.
// ============================================================================
struct GraphicsMeshView {
    uint8_t _head[0x50];                        // +0x00 .. +0x4f (not pinned here)
    Collection::Array<uint8_t> _detailLevels;   // +0x50 data, +0x58 size, +0x5c capacity
    Collection::Array<MeshNode> _nodes;         // +0x60 data, +0x68 size, +0x6c capacity
    Collection::Array<ExternalPtr>* _materialLists;  // +0x70 -> Array<ExternalPtr>[levelCount]
    uint8_t _pad78[0x08];                       // +0x78 .. +0x7f
    Box         _aabb;                          // +0x80 .. +0x97
};
static_assert(offsetof(GraphicsMeshView, _detailLevels)  == 0x50, "GraphicsMesh detail levels @ +0x50");
static_assert(offsetof(GraphicsMeshView, _nodes)         == 0x60, "GraphicsMesh nodes @ +0x60");
static_assert(offsetof(GraphicsMeshView, _materialLists) == 0x70, "GraphicsMesh material lists @ +0x70");
static_assert(offsetof(GraphicsMeshView, _aabb)          == 0x80, "GraphicsMesh AABB @ +0x80");

// ============================================================================
// Graphics::EdgeMesh::TriangleVertex — 8 bytes, and EdgeMesh::EdgeVertex — 16.
//
// TriangleVertex ctor (0x1800a2f50, ordinal 462) writes three int16s from
// `Vector3 * 512.0f` (the multiplier is DAT_180161728, read out of the shipped
// DLL as 0x44000000 = 512.0f) and then a zero int16 at +6.
//
// EdgeVertex ctor (0x1800a2df0, ordinal 164) writes the first TriangleVertex's
// three shorts at +0/+2/+4, the int arg truncated to int16 at +6, the second
// TriangleVertex's three shorts at +8/+10/+12, and 0 at +14. So an EdgeVertex
// is two TriangleVertex-shaped halves, the first carrying the flags word the
// caller passed and the second a zero.
// ============================================================================
struct TriangleVertex {
    int16_t x, y, z;
    int16_t w;         // ctor: 0
};
static_assert(sizeof(TriangleVertex) == 8, "EdgeMesh::TriangleVertex must be 8 bytes");

struct EdgeVertex {
    TriangleVertex a;  // +0x00, a.w = the int argument truncated to int16
    TriangleVertex b;  // +0x08, b.w = 0
};
static_assert(sizeof(EdgeVertex) == 16, "EdgeMesh::EdgeVertex must be 16 bytes");

// The fixed-point scale the TriangleVertex ctor applies. Read from the shipped
// Runtime-x64.dll at 0x180161728 (bytes 00 00 00 44), not guessed.
static const float kEdgeMeshVertexScale = 512.0f;

// ============================================================================
// Graphics::EdgeMesh — 0x180 bytes, an Object::External subclass.
//
// Pinned by ??0EdgeMesh@Graphics@@QEAA@XZ (retail 0x1800a2830, ordinal 161).
// The ctor stamps ??_7Base@Object@@6B@ (0x180154788), then
// ??_7External@Object@@6B@ (0x1801547a0), zeroes +0x08/+0x10/+0x18 — that is
// Object::External's own ctor inlined — then stamps ??_7EdgeMesh@Graphics@@6B@
// (0x180154c70) and initialises its own fields.
//
// It then builds FOUR (VertexBuffer, IndexBuffer) pairs at 0x48/0x68, 0x88/0xa8,
// 0xc8/0xe8, 0x108/0x128. Each is 0x20 bytes: a dword 0 at +0x00, a byte 0 at
// +0x04, then Video::VertexBuffer::PlatformConstruct / IndexBuffer::
// PlatformConstruct fills the rest. EdgeMesh::Set reads the buffer OBJECT
// POINTERS at +0x50/+0x70, +0x90/+0xb0, +0xd0/+0xf0, +0x110/+0x130 — i.e. at
// pair+0x08, which is what PlatformConstruct writes (`*(this+8) = <device
// companion>` at 0x1800f57e0). The pair is therefore
// { int32 _size; uint8 _flags; pad; void* _deviceData; void* _prev; void* _next; }
// with the last two being the device's global intrusive buffer list.
// ============================================================================
struct VideoBufferHeader {
    int32_t _size;        // +0x00  ctor: 0
    uint8_t _flags;       // +0x04  ctor: 0
    uint8_t _pad05[3];    // +0x05
    void*   _deviceData;  // +0x08  written by PlatformConstruct
    void*   _listPrev;    // +0x10  device global buffer list
    void*   _listNext;    // +0x18
};
static_assert(sizeof(VideoBufferHeader) == 0x20, "Video vertex/index buffer header must be 0x20 bytes");
static_assert(offsetof(VideoBufferHeader, _deviceData) == 0x08, "Video buffer _deviceData @ +0x08");

struct EdgeMesh {
    void*   _vtable;                 // +0x00  ??_7EdgeMesh@Graphics@@6B@ (0x180154c70)
    void*   _externalA;              // +0x08  Object::External, ctor: 0
    void*   _externalB;              // +0x10  Object::External, ctor: 0
    void*   _externalC;              // +0x18  Object::External, ctor: 0
    void*   _reserved20;             // +0x20  ctor: 0 (no lane-E body reads it)
    ExternalPtr _material0;          // +0x28  SetMaterials arg 1
    ExternalPtr _material1;          // +0x30  SetMaterials arg 3  <-- see note
    ExternalPtr _material2;          // +0x38  SetMaterials arg 2  <-- see note
    ExternalPtr _material3;          // +0x40  SetMaterials arg 4
    VideoBufferHeader _triVerts;     // +0x48  vertex buffer
    VideoBufferHeader _triIndices;   // +0x68  index buffer
    VideoBufferHeader _edgeVerts;    // +0x88
    VideoBufferHeader _edgeIndices;  // +0xa8
    VideoBufferHeader _capVerts;     // +0xc8
    VideoBufferHeader _capIndices;   // +0xe8
    VideoBufferHeader _extraVerts;   // +0x108
    VideoBufferHeader _extraIndices; // +0x128
    Collection::Array<uint8_t> _triangleLists;  // +0x148 data, +0x150 size, +0x154 capacity
    Collection::Array<uint8_t> _edgeLists;      // +0x158 data, +0x160 size, +0x164 capacity
    Box         _bounds;             // +0x168  ctor: (+1e8 x3, -1e8 x3)
};
static_assert(sizeof(EdgeMesh) == 0x180, "Graphics::EdgeMesh must be 0x180 bytes");
static_assert(offsetof(EdgeMesh, _material0)     == 0x28,  "EdgeMesh material 0 @ +0x28");
static_assert(offsetof(EdgeMesh, _triVerts)      == 0x48,  "EdgeMesh triangle vertex buffer @ +0x48");
static_assert(offsetof(EdgeMesh, _triIndices)    == 0x68,  "EdgeMesh triangle index buffer @ +0x68");
static_assert(offsetof(EdgeMesh, _edgeVerts)     == 0x88,  "EdgeMesh edge vertex buffer @ +0x88");
static_assert(offsetof(EdgeMesh, _edgeIndices)   == 0xa8,  "EdgeMesh edge index buffer @ +0xa8");
static_assert(offsetof(EdgeMesh, _capVerts)      == 0xc8,  "EdgeMesh cap vertex buffer @ +0xc8");
static_assert(offsetof(EdgeMesh, _capIndices)    == 0xe8,  "EdgeMesh cap index buffer @ +0xe8");
static_assert(offsetof(EdgeMesh, _extraVerts)    == 0x108, "EdgeMesh extra vertex buffer @ +0x108");
static_assert(offsetof(EdgeMesh, _extraIndices)  == 0x128, "EdgeMesh extra index buffer @ +0x128");
static_assert(offsetof(EdgeMesh, _triangleLists) == 0x148, "EdgeMesh triangle lists @ +0x148");
static_assert(offsetof(EdgeMesh, _edgeLists)     == 0x158, "EdgeMesh edge lists @ +0x158");
static_assert(offsetof(EdgeMesh, _bounds)        == 0x168, "EdgeMesh bounds @ +0x168");

// NOTE on the material slot order, because it looks like a transcription bug
// and is not. `?SetMaterials@EdgeMesh@Graphics@@QEAAX...000@Z` (retail
// 0x1800a2760, ordinal 3910) stores its FOUR arguments in the order
//     arg1 -> +0x28, arg2 -> +0x38, arg3 -> +0x30, arg4 -> +0x40
// — the middle two are crossed. That is what the shipped code does (the four
// MOVs are emitted in the source order 0x28, 0x38, 0x40, 0x30 and the decompile
// preserves the operands), so the port reproduces it exactly rather than
// "fixing" it. EdgeMeshInstance::SetMesh reads the four slots back in the order
// +0x38, +0x28, +0x40, +0x30 when it ORs their material type flags.

// ============================================================================
// Graphics::EdgeMeshInstance — 0xd0 bytes.
//
// Pinned by ??0EdgeMeshInstance@Graphics@@QEAA@XZ (retail 0x1800a3ab0, ordinal
// 163) and its copy ctor (0x1800174f0, ordinal 162).
//
// *** AN UNINITIALISED FIELD IN THE SHIPPED DLL. *** The default ctor writes
// ONLY +0xc0 (`*(this+0xc0) = 0`) after the Geometry base and the vtable stamp.
// The copy ctor writes +0xc0 AND +0xc8. So a default-constructed
// EdgeMeshInstance leaves +0xc8 — the back-pointer to the MeshInstance the
// outline follows — holding whatever the allocator handed back, and only
// EdgeMeshInstance::SetMesh (0x1800a3a20) ever fills it in. This is the same
// class of finding as Wave 5's `RemoveMesh` leaving `Entry::_userValue2`
// uninitialised (reference_shadow_predicate_must_name_its_failing_term.md).
// The port MUST NOT zero it: doing so would make our default ctor differ from
// retail's on a field a copy ctor propagates, and PLAN §4.1 decision 5 requires
// both implementations to be able to operate on one shared object.
// ============================================================================
struct EdgeMeshInstance {
    Geometry    _base;          // +0x00 .. +0xbf
    ExternalPtr _mesh;          // +0xc0  ExternalPtr<EdgeMesh>; default ctor: 0
    MeshInstance* _followed;    // +0xc8  default ctor: LEFT UNINITIALISED (see above)
};
static_assert(sizeof(EdgeMeshInstance) == 0xd0, "Graphics::EdgeMeshInstance must be 0xd0 bytes");
static_assert(offsetof(EdgeMeshInstance, _mesh)     == 0xc0, "EdgeMeshInstance::_mesh @ +0xc0");
static_assert(offsetof(EdgeMeshInstance, _followed) == 0xc8, "EdgeMeshInstance::_followed @ +0xc8");

}  // namespace RtGraphics

// ############################################################################
// ENDGAME B.3 step 5 (block E) — `Graphics::MeshInstance` and
// `Graphics::EdgeMeshInstance`, the REAL C++ classes
// ############################################################################
//
// Both derive from the real `Graphics::Geometry` (Geometry.h). RULE B5-5: the
// `Geometry _base` embedded member of the two PODs above becomes a real base
// class in the same commit as `Geometry`, and it is the INHERITANCE that is
// real here — the PODs' `_base` was always "single inheritance written as
// embedding" (B35_RULES §3 says so in as many words).
//
// The PODs keep their names and their out-of-block users (`batchc_gfx_leaves.cpp`
// is not this block's file), so both descriptions coexist and every offset is
// cross-asserted. `GraphicsMeshView`, `EdgeMesh`, `MeshNode`, `TriangleVertex`,
// `EdgeVertex`, `VideoBufferHeader`, `MeshAnimationState`, `MeshAnimationLayer`
// and `InterpolatedTransform` above are NOT this block's classes —
// `Graphics::GraphicsMesh` and `Graphics::EdgeMesh` are block G's and
// `Graphics::AnimationState` is block H1's — and are left exactly as they are.
//
// ABSTRACT, per B34_BRIEFS §A.4's amended rule: nothing in the tree holds
// either class BY VALUE as a C++ member, and slot 0's body is a row in
// `runtime/src/object/deleting_dtor.cpp` (which B35_RULES §11 forbids retiring
// in step 5 and which is not this block's file), so slot 0 stays `= 0`.
// ############################################################################

namespace Graphics {

// ============================================================================
// Graphics::MeshInstance : Graphics::Geometry — 0x120, NINE slots.
//
// `??_7MeshInstance@Graphics@@6B@` @ 0x180154dc0 (B35_SLOTS.md):
//   [0] 0x180017330  (unexported) vector deleting dtor   deleting_dtor.cpp row
//   [1] 0x180100800  ?Update@…                  wave29_lane_slots.cpp
//   [2] 0x180100960  ?Draw@…MHHH@Z              meshinstance_draw.cpp
//   [3] 0x180100f20  ?AddDrawItems@…            mesh.cpp
//   [4] 0x1800a4020  Geometry::UpdateAttached            INHERITED
//   [5] 0x180101200  ?GetSubTransform@…         mesh.cpp        RETSLOT
//   [6] 0x180101280  ?GetInstanceTransform@…    mesh.cpp        RETSLOT
//   [7] 0x180101660  ?SetSharedInstanceData@…   wave29_lane_slots.cpp
//   [8] 0x1800a4420  Geometry::Clean                     INHERITED
//
// `MeshInstance` overrides BOTH retslot slots (5 and 6), where
// `EdgeMeshInstance` overrides only slot 6 (B35_RULES §2.3 item 6).
//
// THE TWO UNEXPORTED `.data` STATICS `MeshInstance::Update` READS STAY
// (B35_RULES §9, §6.1): the guard byte at 0x1801ff30c and the 48-entry identity
// `Transform` table at 0x1801ff310, minted as `EXTRA_STORAGE_BY_ADDR`. They are
// NOT in the cutover manifest and so are NOT protected by the manifest gate;
// `wave29_lane_slots.cpp` carries both `ResolveAddr` rows forward explicitly and
// `MeshInstance::Update` still declines the whole update when either resolves
// NULL. `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA` (0x1801fc70b) likewise
// stays an `RT_DATA_SLOT` read — RULE B4b-4 owes it a `MintStorageCell()`
// redirect at the class-static conversion, the END of B.3, not here.
// ============================================================================
class MeshInstance : public Geometry {
public:
    // `??0MeshInstance@Graphics@@QEAA@AEBV01@@Z` @ 0x1800170c0 — PORTED, body in
    // `runtime/src/graphics/mesh.cpp`. Declared, NOT `= default`: it zeroes each
    // of the three nested Array headers and then runs retail's deep
    // `Array<T>::operator=` over it, which a bitwise copy would not.
    MeshInstance(const MeshInstance& src);
    MeshInstance& operator=(const MeshInstance& src);   // 0x1800171e0  not ported

    // ---- the nine slots ---------------------------------------------------
    Geometry* DeletingDtor(unsigned flags) override = 0;                 // [0] 0x180017330
    void Update(const System::Time& time) override;                      // [1] 0x180100800
    void Draw(Video::Context& context, const Camera& camera,
              ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;         // [2] 0x180100960
    void AddDrawItems(const Scene& scene, Renderer& renderer) const override; // [3] 0x180100f20
    // Both retslot slots, in retail order — see Geometry.h's B5-12 block.
    Math::Transform* GetSubTransform(Math::Transform* ret,
                                     int node) const override;            // [5] 0x180101200 RETSLOT
    Math::Matrix* GetInstanceTransform(Math::Matrix* ret, int node,
                                       float t) const override;            // [6] 0x180101280 RETSLOT
    int SetSharedInstanceData(Video::Context& context,
                              int a, float t) const override;             // [7] 0x180101660
    // slots 4 and 8 are Geometry's own bodies, inherited unchanged.

    // ---- PORTED non-virtual members (bodies in this block's files) --------
    Math::Transform GetNodeToWorld(int node) const;                       // 0x180016a40 RETSLOT
    Math::Transform GetNodeToWorld(int node, float t) const;              // 0x180016c20 RETSLOT
    Math::Vector3   RootAnimationOffset() const;                          // 0x180016ef0 RETSLOT
    bool            IsAnimating() const;                                  // 0x1800ff7d0
    void            UpdateHierarchy();                                    // 0x1800ff910
    void            SetMaterialInstance(int index, ExternalPtrHandle value); // 0x1800ffe80
    void            SetMesh(ExternalPtrHandle mesh);                      // 0x1800ffea0
    void            PlayAnimation(ExternalPtrHandle group, int index,
                                  const void* startTime, const void* blendTime,
                                  float weight);                          // 0x1800ff840

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined -
    // Each is one to a dozen instructions in retail over offsets this header
    // already pins. Declared so no later block or caller batch edits this file.
    ExternalPtrHandle GetMesh() const;                       // 0x180016a20
    int      GetNodeCount() const;                           // 0x180016a30
    void     SetVisualScale(const Math::Vector4& scale);     // 0x180016fa0
    Math::Vector4 GetVisualScale() const;                    // 0x180016fd0 RETSLOT
    int      GetMaterialInstanceCount() const;               // 0x180017000
    ExternalPtrHandle GetMaterialInstance(int index) const;  // 0x180017010
    void     SetSortKey(int key);                            // 0x180017030
    void     Serialize(IO::Serializer& serializer);          // 0x1800ff680
    float    GetAnimationElapsed() const;                    // 0x1800ff700
    bool     IsCurrentAnimationLooping() const;              // 0x1800ff730
    void     SetCurrentAnimationTimeScale(float scale);      // 0x1800ff760
    float    GetCurrentAnimationTimeScale() const;           // 0x1800ff780
    int      GetCurrentAnimationIndex() const;               // 0x1800ff7b0
    void     ResetAnimationState();                          // 0x1800ff810
    void     CopyHierarchy(const MeshInstance& src);         // 0x1800ffd80
    int      GetMaterialIndex(ExternalPtrHandle material);   // 0x1800ffe30
    int      GetBones(float t, const Math::Transform& xf,
                      Math::Matrix* out) const;              // 0x180100070

    // The two CLASS STATICS over `?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA`
    // (0x1801fc70b). Exported, not ported, no caller as members — the flag
    // itself is read through `RT_DATA_SLOT` in `wave29_lane_slots.cpp` and owes
    // a `MintStorageCell()` redirect at the END of B.3 (RULE B4b-4).
    static void EnableRootOnlyUpdates(bool enable);          // 0x180017040
    static bool AreRootOnlyUpdateEnabled();                  // 0x180017050

    // `??2MeshInstance@Graphics@@SAPEAX_K@Z` (0x180016940),
    // `??2…@SAPEAX_KPEAX@Z` (0x1800169b0) and `??3…@SAXPEAX@Z` (0x1800169c0) are
    // the class's own `operator new` / `operator delete`. They are exported and
    // unported, and they are deliberately NOT declared: a declared-but-undefined
    // `operator new` would make any future `new MeshInstance` a link error at
    // best and a silently different allocator at worst, and nothing in the tree
    // news one as a C++ object (see the ABSTRACT banner above). Recorded here so
    // the omission is a decision, not a gap.

protected:
    // `??1MeshInstance@Graphics@@UEAA@XZ` @ 0x180017060, `UEAA` because retail's
    // is virtual — the non-virtual `QEAA` spelling is a name the DLL does not
    // contain (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). NON-VIRTUAL
    // here (ENDGAME decision 2). The body stays in its door in `mesh.cpp`: it
    // frees the three Array buffers through retail's own out-of-line
    // `Array<T>::~Array` and then runs `~Geometry`, and a compiler-emitted
    // member/base destructor chain on top of that would be retail's own
    // decomposition run twice. Declared, deliberately undefined.
    ~MeshInstance();

public:
    RtGraphics::ExternalPtr                        _mesh;         // +0xc0  ExternalPtr<GraphicsMesh>
    Collection::Array<RtGraphics::ExternalPtr>     _materials;    // +0xc8
    RtGraphics::MeshAnimationState                 _animation;    // +0xd8 .. +0xef
    Collection::Array<RtGraphics::InterpolatedTransform> _nodes;  // +0xf0
    Vector3                                        _rootAnimationOffset; // +0x100
    float                                          _color[4];     // +0x10c ctor 1,1,1,1
    int16_t                                        _sortBias;     // +0x11c ctor 0
    int16_t                                        _pad11e;       // +0x11e
};

static_assert(sizeof(MeshInstance) == 0x120,
              "Graphics::MeshInstance is 0x120 bytes (MOV EDX,0x120 in its own "
              "slot-0 vector deleting destructor 0x180017330)");
static_assert(alignof(MeshInstance) == 8, "qword aligned (the vptr)");
static_assert(sizeof(MeshInstance) == sizeof(RtGraphics::MeshInstance),
              "the real class and the POD describe the same bytes");

static_assert(offsetof(MeshInstance, _mesh)      == offsetof(RtGraphics::MeshInstance, _mesh),      "alias _mesh");
static_assert(offsetof(MeshInstance, _materials) == offsetof(RtGraphics::MeshInstance, _materials), "alias _materials");
static_assert(offsetof(MeshInstance, _animation) == offsetof(RtGraphics::MeshInstance, _animation), "alias _animation");
static_assert(offsetof(MeshInstance, _nodes)     == offsetof(RtGraphics::MeshInstance, _nodes),     "alias _nodes");
static_assert(offsetof(MeshInstance, _rootAnimationOffset) == offsetof(RtGraphics::MeshInstance, _rootAnimationOffset), "alias _rootAnimationOffset");
static_assert(offsetof(MeshInstance, _color)     == offsetof(RtGraphics::MeshInstance, _color),     "alias _color");
static_assert(offsetof(MeshInstance, _sortBias)  == offsetof(RtGraphics::MeshInstance, _sortBias),  "alias _sortBias");

static_assert(offsetof(MeshInstance, _mesh)                == 0xc0,  "MeshInstance::_mesh @ +0xc0 (??0 0x1800fffc0)");
static_assert(offsetof(MeshInstance, _materials)           == 0xc8,  "MeshInstance::_materials @ +0xc8");
static_assert(offsetof(MeshInstance, _animation)           == 0xd8,  "MeshInstance::_animation @ +0xd8 (the BYTE store at 0x180100009)");
static_assert(offsetof(MeshInstance, _nodes)               == 0xf0,  "MeshInstance::_nodes @ +0xf0");
static_assert(offsetof(MeshInstance, _rootAnimationOffset) == 0x100, "MeshInstance::_rootAnimationOffset @ +0x100");
static_assert(offsetof(MeshInstance, _color)               == 0x10c, "MeshInstance::_color @ +0x10c (four 0x3f800000 dwords)");
static_assert(offsetof(MeshInstance, _sortBias)            == 0x11c, "MeshInstance::_sortBias @ +0x11c (a WORD store, which is why sizeof is 0x120)");

// The base subobject must start at zero and the derived fields must start at
// exactly sizeof(Geometry) — no tail-padding reuse, no vptr duplication.
static_assert(offsetof(MeshInstance, _mesh) == sizeof(Geometry),
              "MeshInstance's own fields begin at the end of the Geometry base, "
              "with no gap: retail's ctor writes +0xc0 immediately after the base ctor");

// ============================================================================
// Graphics::EdgeMeshInstance : Graphics::Geometry — 0xd0, NINE slots.
//
// `??_7EdgeMeshInstance@Graphics@@6B@` @ 0x180154d70 (B35_SLOTS.md):
//   [0] 0x1800175d0  (unexported) vector deleting dtor   deleting_dtor.cpp row
//   [1] 0x1800a39b0  ?Update@…                  mesh.cpp
//   [2] 0x1800a3150  ?Draw@…MHHH@Z              meshinstance_draw.cpp
//   [3] 0x1800a2fd0  ?AddDrawItems@…            mesh.cpp
//   [4] 0x1800a4020  Geometry::UpdateAttached            INHERITED
//   [5] 0x1800a43c0  Geometry::GetSubTransform           INHERITED  (see B5-12)
//   [6] 0x1800a3840  ?GetInstanceTransform@…    mesh.cpp        RETSLOT
//   [7] 0x180015590  Geometry::SetSharedInstanceData     INHERITED
//   [8] 0x1800a4420  Geometry::Clean                     INHERITED
//
// *** IT OVERRIDES ONLY ONE OF THE TWO RETSLOT SLOTS. *** Slot 5 stays
// `Geometry::GetSubTransform` (0x1800a43c0, byte-identical target) and only slot
// 6 is its own. B35_RULES §2.3 item 6 records the same asymmetry; the class
// below therefore declares slot 6 and NOT slot 5.
//
// `_followed` (+0xc8) IS LEFT UNINITIALISED BY THE DEFAULT CONSTRUCTOR and the
// port must not zero it — see the POD's banner above. That is why there is no
// real default constructor here either (there would be none anyway: ENDGAME
// decision 2 and B35_RULES §11 keep the ctor door).
// ============================================================================
class EdgeMeshInstance : public Geometry {
public:
    // `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` @ 0x1800174f0 — PORTED,
    // body in `mesh.cpp`. It writes +0xc0 AND +0xc8, where the default ctor
    // writes only +0xc0.
    EdgeMeshInstance(const EdgeMeshInstance& src);
    EdgeMeshInstance& operator=(const EdgeMeshInstance& src);  // 0x180017550  not ported

    Geometry* DeletingDtor(unsigned flags) override = 0;                 // [0] 0x1800175d0
    void Update(const System::Time& time) override;                      // [1] 0x1800a39b0
    void Draw(Video::Context& context, const Camera& camera,
              ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;         // [2] 0x1800a3150
    void AddDrawItems(const Scene& scene, Renderer& renderer) const override; // [3] 0x1800a2fd0
    Math::Matrix* GetInstanceTransform(Math::Matrix* ret, int node,
                                       float t) const override;            // [6] 0x1800a3840 RETSLOT
    // slots 4, 5, 7 and 8 are Geometry's own bodies, inherited unchanged.

    // PORTED, body in `mesh.cpp`. `MeshInstance*` is the outline's back-pointer.
    void SetMesh(ExternalPtrHandle mesh, MeshInstance* followed);         // 0x1800a3a20

    // Exported, NOT PORTED, no caller: declared, deliberately undefined.
    ExternalPtrHandle GetMesh() const;                                    // 0x1800174c0

    // `??2`/`??3` operator new/delete (0x1800173e0 / 0x180017450 / 0x180017460)
    // are deliberately not declared; see MeshInstance's note above.

protected:
    ~EdgeMeshInstance();    // 0x1800174d0  exported, NOT PORTED, no caller

public:
    RtGraphics::ExternalPtr _mesh;      // +0xc0  ExternalPtr<EdgeMesh>; ctor 0
    MeshInstance*           _followed;  // +0xc8  default ctor LEAVES IT UNSET
};

static_assert(sizeof(EdgeMeshInstance) == 0xd0,
              "Graphics::EdgeMeshInstance is 0xd0 bytes (MOV EDX,0xd0 in its own "
              "slot-0 vector deleting destructor 0x1800175d0)");
static_assert(alignof(EdgeMeshInstance) == 8, "qword aligned (the vptr)");
static_assert(sizeof(EdgeMeshInstance) == sizeof(RtGraphics::EdgeMeshInstance),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(EdgeMeshInstance, _mesh)     == offsetof(RtGraphics::EdgeMeshInstance, _mesh),     "alias _mesh");
static_assert(offsetof(EdgeMeshInstance, _followed) == offsetof(RtGraphics::EdgeMeshInstance, _followed), "alias _followed");
static_assert(offsetof(EdgeMeshInstance, _mesh)     == 0xc0, "EdgeMeshInstance::_mesh @ +0xc0");
static_assert(offsetof(EdgeMeshInstance, _followed) == 0xc8, "EdgeMeshInstance::_followed @ +0xc8");
static_assert(offsetof(EdgeMeshInstance, _mesh) == sizeof(Geometry),
              "EdgeMeshInstance's own fields begin at the end of the Geometry base");

}  // namespace Graphics
