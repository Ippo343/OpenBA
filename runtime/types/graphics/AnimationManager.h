#pragma once
// ============================================================================
// runtime/types/graphics/AnimationManager.h
// Graphics::AnimationManager and the two records its arrays hold, plus the
// three Array<T> instantiations underneath them.
// ============================================================================
//
// SOURCE OF EVERY OFFSET BELOW. Unlike AnimationState.h next door — which had
// to read one body's disassembly because Ghidra holds two overlapping function
// bodies around 0x1800fdb20 — this file's layout is pinned by a SERIALIZER,
// which is the strongest evidence this project gets for a record layout: a
// serializer names the TYPE of every field it touches (through the
// IO::ReaderWriter vtable slot it calls) and visits them in declaration order.
//
//   0x18005c660   Array<AnimationManager::Instance>::Serialize  stride 0x30
//   0x18005c2c0   Array<AnimationState>::Serialize              stride 0x48
//   0x18005cb40   Array<AnimationState::Layer>::Serialize       stride 0x38
//   0x18005c120   Array<GraphicsMesh::Node>::Serialize          stride 0x48
//   0x18005c230   Array<InterpolatedState<Transform>>::Serialize stride 0x38
//   0x180059820   Array<Math::Transform>::Serialize             stride 0x1c
//
// Cross-checked against four independent readers of the same records:
//   * ?GetPose@AnimationManager@Graphics@@ (0x1800fb8e0) walks
//     `manager->_instances._data + ((id >> 8) & 0xff) * 0x30`, loads the
//     instance's +0x20 field as a buffer and indexes it by
//     `(id & 0xff) * 0x48`, then reads THAT record's +0x28 as a buffer of
//     0x1c-byte Transforms. That fixes the Instance stride (0x30), the
//     AnimationState stride (0x48), Instance::_states at +0x20 and
//     AnimationState::_pose at +0x28 all at once.
//   * ?ResetAnimationState@AnimationState@Graphics@@ (0x1800fd930) and
//     ?PlayAnimation@AnimationState@Graphics@@ (0x1800fdf50) both decrement
//     `instance + 0x0c` and, when it reaches zero, store 0 at +0x00,
//     -1 at +0x08 and SetSize(instance+0x10, 0) / SetSize(instance+0x20, 0).
//   * the per-element destroy helper 0x18005bce0 is already a row in
//     runtime/src/collection/array_destroy.cpp as `kD_InnerArr3_48` with
//     offsets {0x38, 0x28, 0x08} at stride 0x48 — three inner Arrays inside a
//     0x48 record, which is exactly the AnimationState declared below.
//   * the copy constructors 0x18005b700 (Instance) and 0x18005b660
//     (AnimationState) write the same fields in the same order.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE CORRECTS IN AnimationState.h
// ---------------------------------------------------------------------------
// AnimationState.h (Wave 27 lane GFXBIG) declares AnimationState +0x28 and
// +0x38 as two `void*` named `interpB` / `interpA`, "the two
// InterpolatedState<Math::Transform>* the caller passes as Update's 4th and
// 5th parameters". The POINTER VALUE it describes is right and the TYPE is
// wrong: both are 16-byte `Collection::Array` headers, so what the caller
// passes is the address of an Array, not an InterpolatedState. See the
// correction note left in AnimationState.h itself.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"

namespace RtGraphics {

// ---------------------------------------------------------------------------
// Graphics::AnimationState — 0x48 bytes, the full layout.
//
// AnimationState.h declares the same record from AnimationState::Update's side
// and names the fields Update reads; this declaration is the SERIALIZER's view
// and is the one the Array<AnimationState> machinery in
// runtime/src/collection/array_graphics_anim.cpp uses. They agree on every
// offset; the two differences are that this one knows +0x28 / +0x38 are Array
// headers and that it does not claim to know what +0x24 / +0x30 / +0x40 are
// (they are the `_capacity` halves of the two Array triples).
//
// Serialized by 0x18005c2c0 in this order: the layer array (count as int, then
// the 0x38 records), +0x00 as a bool, the node array (count as int, then the
// 0x38 InterpolatedState records), the Vector3 at +0x18 as three floats, then
// the pose array through 0x180059820.
// ---------------------------------------------------------------------------
struct AnimStateRec {
    uint8_t  _needsUniqueCopy;                   // +0x00
    uint8_t  _pad01[7];                          // +0x01
    Collection::Array<uint8_t> _layers;          // +0x08  stride 0x38
    float    _rootOffset[3];                     // +0x18  Math::Vector3
    int32_t  _pad24;                             // +0x24
    Collection::Array<uint8_t> _pose;            // +0x28  Array<Math::Transform>, stride 0x1c
    Collection::Array<uint8_t> _nodes;           // +0x38  Array<InterpolatedState<Transform>>, 0x38
};
static_assert(sizeof(AnimStateRec) == 0x48, "Graphics::AnimationState is 0x48 bytes");
static_assert(offsetof(AnimStateRec, _layers)     == 0x08, "AnimationState::_layers @ +0x08");
static_assert(offsetof(AnimStateRec, _rootOffset) == 0x18, "AnimationState::_rootOffset @ +0x18");
static_assert(offsetof(AnimStateRec, _pose)       == 0x28, "AnimationState::_pose @ +0x28");
static_assert(offsetof(AnimStateRec, _nodes)      == 0x38, "AnimationState::_nodes @ +0x38");

// ---------------------------------------------------------------------------
// Graphics::AnimationManager::Instance — 0x30 bytes.
//
// One cached, shared skeleton animation: the group/clip it plays, a reference
// count, the bind-pose node list it was built for, and one AnimationState per
// frame of the clip (SetSize(instance->_states, clip->_frameCount)).
//
// Serialized by 0x18005c660: ExternalPtr at +0x00, two ints at +0x08 / +0x0c,
// then the STATES array (+0x20 / count +0x28) BEFORE the nodes array
// (+0x10 / count +0x18). The out-of-order visit is retail's and is reproduced.
// ---------------------------------------------------------------------------
struct AnimInstanceRec {
    void*    _group;                             // +0x00  ExternalPtr<AnimationGroup>
    int32_t  _animIndex;                         // +0x08  init'd to -1 by SetSize's new-tail arm
    int32_t  _refCount;                          // +0x0c  GetInstanceId ++, ResetAnimationState --
    Collection::Array<uint8_t> _nodes;           // +0x10  Array<GraphicsMesh::Node>, stride 0x48
    Collection::Array<uint8_t> _states;          // +0x20  Array<AnimationState>,     stride 0x48
};
static_assert(sizeof(AnimInstanceRec) == 0x30, "AnimationManager::Instance is 0x30 bytes");
static_assert(offsetof(AnimInstanceRec, _animIndex) == 0x08, "Instance::_animIndex @ +0x08");
static_assert(offsetof(AnimInstanceRec, _refCount)  == 0x0c, "Instance::_refCount @ +0x0c");
static_assert(offsetof(AnimInstanceRec, _nodes)     == 0x10, "Instance::_nodes @ +0x10");
static_assert(offsetof(AnimInstanceRec, _states)    == 0x20, "Instance::_states @ +0x20");

// ---------------------------------------------------------------------------
// Graphics::AnimationManager — only the one field every body here reads.
//
// `?GetPose@AnimationManager@Graphics@@` opens with
// `MOV RAX,[RCX]` / `... + 0x20 + idx*0x30`, and GetInstanceId reads the count
// at `this + 0x08` and the buffer at `this + 0x00`. The class is larger; this
// is a read-only overlay of its head, not a definition.
// ---------------------------------------------------------------------------
struct AnimationManagerView {
    Collection::Array<uint8_t> _instances;       // +0x00  Array<Instance>, stride 0x30
};
static_assert(offsetof(AnimationManagerView, _instances) == 0x00,
              "AnimationManager::_instances @ +0x00");

// ---------------------------------------------------------------------------
// Graphics::Animation — the clip record, stride 0x70.
//
// Only the fields `?GetPoseAtTime@Animation@Graphics@@` (0x1800faca0) reads are
// named, plus the two `?PlayAnimation@AnimationState@Graphics@@` reads. The
// record is 0x70 bytes and nothing else about it is claimed here.
//
// *** THIS DISAGREES WITH AnimationState.h's `AnimationClipPrefix` ON TWO
// NAMES, and GetPoseAtTime is the body that settles both. *** That header calls
// +0x48 `loopScale` and +0x50 `frameRate`. GetPoseAtTime multiplies the elapsed
// SECONDS by +0x44 to get a frame cursor (so +0x44 is the rate, in frames per
// second) and compares that cursor against +0x50 to decide whether the clip has
// run out (so +0x50 is the FRAME COUNT, not a rate). AnimationState.h's own
// note that +0x50 is "read as an INT and cvtdq2ps'd to float" is consistent
// with a count and not with a stored rate.
// ---------------------------------------------------------------------------
// Declared as OFFSET CONSTANTS rather than as a struct: the record is 0x70
// bytes, only these ten fields have a named reader, and a struct would have to
// invent names for the holes between them.
//   +0x2c  bool  "this clip is instanced"  (PlayAnimation gates GetInstanceId)
//   +0x30  int   first frame of the loop fraction's numerator
//   +0x34  int   last frame  (frameLast - frameFirst is the denominator)
//   +0x3c  bool  looping
//   +0x40  int   loop start frame; 0 short-circuits the restart offset to 0
//   +0x44  float frames per second
//   +0x4c  int   bone count       (`MOV ECX,[RBX+0x4c]` at 0x1800fad9b)
//   +0x50  int   frame count      (`MOV ...,[RBX+0x50]`)
//   +0x58  int64 duration in ticks
//   +0x60  Math::Transform*       the key buffer, boneCount * frameCount keys
//                                 of 0x1c bytes (`MOV RAX,[RBX+0x60]` 0x1800fade4)
namespace anim_clip {
    constexpr int kInstanced   = 0x2c;
    constexpr int kFrameFirst  = 0x30;
    constexpr int kFrameLast   = 0x34;
    constexpr int kLooping     = 0x3c;
    constexpr int kLoopFrame   = 0x40;
    constexpr int kFrameRate   = 0x44;
    constexpr int kBoneCount   = 0x4c;
    constexpr int kFrameCount  = 0x50;
    constexpr int kDuration    = 0x58;
    constexpr int kKeys        = 0x60;
    constexpr int kStride      = 0x70;
}

// Graphics::AnimationGroup — the two fields PlayAnimation and GetInstanceId
// read. `group + 0x20` is the clip buffer and `group + 0x28` its count, indexed
// by `animIndex * 0x70`.
namespace anim_group {
    constexpr int kClips       = 0x20;
    constexpr int kClipCount   = 0x28;
    constexpr int kDefaultFade = 0x30;   // float, seconds
    constexpr int kFadeTable   = 0x48;   // float*, [fromClip * clipCount + toClip]
}

}  // namespace RtGraphics
