#pragma once
// ============================================================================
// runtime/types/graphics/AnimationState.h — Wave 27 lane GFXBIG
// Graphics::AnimationState and the two records it walks.
// ============================================================================
//
// EVERY OFFSET HERE WAS READ OUT OF ONE BODY: `?Update@AnimationState@
// Graphics@@` (retail 0x1800fdb20, ordinal 4474), disassembled with
// `objdump -d -M intel` over the shipped C:\GOG Games\Banished\Runtime-x64.dll.
//
// *** IT WAS NOT READ IN GHIDRA, AND THAT WAS NOT A PREFERENCE. ***
// Ghidra holds TWO OVERLAPPING function bodies at this address — `FUN_1800fd9a0`
// claiming 0x1800fd9a0-0x1800fdb58 and `Update` claiming
// 0x1800fdb20-0x1800fdf45 — and the ghidra-mcp `disassemble_function` endpoint
// resolves an address to the first body containing it. Asked for 0x1800fdb20 it
// returned 126 instructions of the OTHER function, printed under the addresses
// that had been asked for, with no warning. It reported the instruction at
// 0x1800fdb20 as `LEA RAX,[RBX + -0x8]`; the bytes in the file are
// `4c 89 4c 24 20 ...` = `MOV [RSP+0x20],R9`, a prologue.
// See docs/runtime_port/WAVE27_LANE_GFXBIG.md "INSTRUMENT DEFECT OBSERVED,
// NOT FIXED".
//
// WHAT IS MEASURED AND WHAT IS INHERITED
// --------------------------------------
// MEASURED HERE: every field this file marks with an instruction address.
// INHERITED (measured by WAVE 23 lane GFXBODY from the CALLER side, in
// `?Update@AnimationManager@Graphics@@` 0x1800fc240, and recorded in
// runtime/src/graphics/wave23_lane_gfxbody.cpp §5.1): the 0x48 STRIDE of
// AnimationState and the meaning of +0x18 / +0x28 / +0x38. `Update` itself
// never touches those three, so this header cannot and does not claim to have
// re-derived them; it names their source instead
// (reference_a_measured_number_sitting_beside_an_asserted_one.md is exactly the
// defect being avoided).
//
// NOT PINNED, and left as explicit holes rather than invented: AnimationState
// +0x24, +0x30..+0x37, +0x40..+0x47; AnimationLayer +0x14, +0x31..+0x37;
// AnimationClip everything not listed. No body this lane read touches them.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../collection/Collection.h"
#include "../object/External.h"   // Object::ExternalPtr<T>, for PlayAnimation
#include "../math/Math.h"          // B.4: Math::Transform / Vector3, the typed members
#include "InterpolatedState.h"     // B.4: Array<InterpolatedState<Math::Transform>>
#include "GraphicsMesh.h"          // B.4: Array<GraphicsMesh::Node>, retail's parameter type
#include "../system/Value.h"       // B.4: System::Time, the three Layer times

// Forward declarations only — see the `namespace Graphics` block at the bottom.
// Tags match the real declarations (RULE B4b-5).
namespace System { class Time; }
namespace Math { class Transform; class Vector3; }
namespace IO { class Serializer; }
namespace Graphics { class AnimationGroup; }

namespace RtGraphics {

// ============================================================================
// One playing animation layer, stride 0x38.
//
// The stride is MEASURED three independent ways inside Update: the iteration
// step `ADD R14,0x38` (0x1800fdef9), the erase-shift `ADD RCX,0x38` with a
// seven-qword copy from `+0x38` (0x1800fdeda / 0x1800fde9c..0x1800fded6), and
// the last-element address `(count-1) * 0x38` via `IMUL RDX,RDX,0x38`
// (0x1800fdbbc).
// ============================================================================
struct AnimationLayer {
    // +0x00  Graphics::AnimationGroup* — the resource the clip lives in.
    //        `MOV RAX,QWORD PTR [R14+R15*1]` at 0x1800fdce9, then
    //        `ADD RBP,QWORD PTR [RAX+0x20]` — so +0x20 of THIS is the clip array.
    void*   group;

    // +0x08  index of the clip within group->clips.
    //        `MOVSXD RBP,DWORD PTR [R14+R15*1+0x8]` at 0x1800fdce4, scaled by
    //        0x70 (`IMUL RBP,RBP,0x70` at 0x1800fdcf4).
    int32_t clipIndex;

    // +0x0c  packed handle into AnimationManager's group array. NEGATIVE means
    //        "no handle" (`TEST ECX,ECX / JS` at 0x1800fde35). When present,
    //        bits 8..15 are the group index: `SAR ECX,0x8` then
    //        `MOVZX EAX,CL` at 0x1800fde39.
    int32_t handle;

    // +0x10  playback rate. Compared against 1.0f (`UCOMISS XMM0,XMM8` at
    //        0x1800fdcfc, XMM8 loaded from 0x18016169c == 1.0f) to take a
    //        fast path that uses the frame delta verbatim.
    float   speed;

    int32_t _unpinned_14;   // +0x14  no reader in Update

    // +0x18  accumulated playback position in System::Time ticks.
    //        Read/written at 0x1800fdd49, 0x1800fdd80, 0x1800fdd98,
    //        0x1800fddec, 0x1800fddf7.
    int64_t time;

    // +0x20  total fade-out length in ticks. `> 0` is what ARMS the countdown
    //        (`CMP QWORD PTR [R14+R15*1+0x20],0x0 / JLE` at 0x1800fddfb).
    int64_t fadeLength;

    // +0x28  remaining fade ticks; decremented by the frame delta and the layer
    //        is REMOVED when it reaches <= 0 (0x1800fde1b / 0x1800fde20).
    int64_t fadeRemaining;

    // +0x30  set to 1 on layer[0] on every entry into Update
    //        (`MOV BYTE PTR [RAX+0x30],0x1` at 0x1800fdb84).
    uint8_t restarted;

    uint8_t _unpinned_31[7];   // +0x31  no reader in Update
};
static_assert(sizeof(AnimationLayer) == 0x38, "Graphics AnimationLayer stride is 0x38");
static_assert(offsetof(AnimationLayer, clipIndex)     == 0x08, "AnimationLayer::clipIndex @ +0x08");
static_assert(offsetof(AnimationLayer, handle)        == 0x0c, "AnimationLayer::handle @ +0x0c");
static_assert(offsetof(AnimationLayer, speed)         == 0x10, "AnimationLayer::speed @ +0x10");
static_assert(offsetof(AnimationLayer, time)          == 0x18, "AnimationLayer::time @ +0x18");
static_assert(offsetof(AnimationLayer, fadeLength)    == 0x20, "AnimationLayer::fadeLength @ +0x20");
static_assert(offsetof(AnimationLayer, fadeRemaining) == 0x28, "AnimationLayer::fadeRemaining @ +0x28");
static_assert(offsetof(AnimationLayer, restarted)     == 0x30, "AnimationLayer::restarted @ +0x30");

// ============================================================================
// One animation clip, stride 0x70 (`IMUL RBP,RBP,0x70` at 0x1800fdcf4).
//
// ONLY the eight fields Update reads are named. The record is 0x70 bytes and
// this header claims nothing about the other 0x48 of it.
// ============================================================================
struct AnimationClipPrefix {
    uint8_t  _head_00[0x08];  // +0x00  no reader in Update

    // +0x08  event count. `> 0` (together with the caller's 8th bool and
    //        layer index 0) is what gates the Animation::ApplyEvents call
    //        (`CMP DWORD PTR [RBP+0x8],R12D / JLE` at 0x1800fdd61).
    int32_t  eventCount;

    uint8_t  _head_0c[0x24];  // +0x0c  no reader in Update

    int32_t  frameFirst;      // +0x30  subtrahend in the loop-restart fraction
    int32_t  frameLast;       // +0x34  frameLast - frameFirst is the denominator
    int32_t  _unpinned_38;    // +0x38  no reader in Update

    // +0x3c  looping flag. Non-zero => wrap `time` modulo `duration` and add a
    //        restart offset; zero => clamp `time` to `duration`
    //        (`CMP BYTE PTR [RBP+0x3c],0x0 / JE` at 0x1800fdd84).
    uint8_t  looping;
    uint8_t  _unpinned_3d[3]; // +0x3d

    int32_t  loopFrame;       // +0x40  ZERO short-circuits the whole fraction to 0
    int32_t  _unpinned_44;    // +0x44
    float    loopScale;       // +0x48  final multiplier
    int32_t  _unpinned_4c;    // +0x4c
    // +0x50  read as an INT and cvtdq2ps'd to float.
    //
    // *** THIS IS THE CLIP'S FRAME COUNT, NOT ITS FRAME RATE. Corrected
    // 2026-09-12 (cutover closure batch 3). *** The name here was a guess from
    // the cvtdq2ps; `?GetPoseAtTime@Animation@Graphics@@` (0x1800faca0) settles
    // it by using BOTH fields in one expression — it multiplies elapsed seconds
    // by +0x44 to get a frame cursor, then compares that cursor against +0x50
    // to decide whether the clip has run out and must clamp or wrap. A rate
    // cannot be the bound of a frame index. So +0x44 is the frames-per-second
    // and +0x50 is the number of keyframes; `loopScale` at +0x48 above has no
    // reader in GetPoseAtTime and is left as this lane named it.
    int32_t  frameCount;      // +0x50
    int32_t  _unpinned_54;    // +0x54

    // +0x58  clip duration in System::Time ticks (int64, compared against
    //        `layer.time` with a SIGNED 64-bit CMP at 0x1800fdd8a).
    int64_t  duration;
};
static_assert(offsetof(AnimationClipPrefix, eventCount) == 0x08, "clip::eventCount @ +0x08");
static_assert(offsetof(AnimationClipPrefix, frameFirst) == 0x30, "clip::frameFirst @ +0x30");
static_assert(offsetof(AnimationClipPrefix, frameLast)  == 0x34, "clip::frameLast @ +0x34");
static_assert(offsetof(AnimationClipPrefix, looping)    == 0x3c, "clip::looping @ +0x3c");
static_assert(offsetof(AnimationClipPrefix, loopFrame)  == 0x40, "clip::loopFrame @ +0x40");
static_assert(offsetof(AnimationClipPrefix, loopScale)  == 0x48, "clip::loopScale @ +0x48");
static_assert(offsetof(AnimationClipPrefix, frameCount) == 0x50, "clip::frameCount @ +0x50");
static_assert(offsetof(AnimationClipPrefix, duration)   == 0x58, "clip::duration @ +0x58");
static_assert(sizeof(AnimationClipPrefix) == 0x60,
              "the PREFIX of an animation clip Update reads ends at +0x60; the "
              "record itself is 0x70 and this struct deliberately covers only "
              "the part that has a named reader");

// ============================================================================
// Graphics::AnimationState — 0x48 bytes.
//
// THE THREE FIELDS UPDATE ACTUALLY READS ARE +0x00, +0x08 and +0x10, and those
// are the three this lane MEASURED:
//
//   0x1800fdb60  CMP DWORD PTR [RCX+0x10],0x0   ; layer count, early return
//   0x1800fdb6d  MOV RAX,QWORD PTR [RCX+0x8]    ; layer array base
//   0x1800fdb84  MOV BYTE PTR [RAX+0x30],0x1    ; layers[0].restarted
//   0x1800fdbcf  MOV BYTE PTR [RSI],CL          ; +0x00 = 1
//   0x1800fdbd3  CMP BYTE PTR [RSI],0x0         ; +0x00 tested
//   0x1800fdbf8  MOV BYTE PTR [RSI],0x0         ; +0x00 cleared
//   0x1800fdee4  DEC DWORD PTR [RSI+0x10]       ; layer count decremented
//
// +0x08/+0x10/+0x14 are a `Collection::Array<AnimationLayer>` triple. The
// evidence is the same shape lane GFXBODY used for AnimationManager: +0x10 is a
// LOOP BOUND that is also DECREMENTED on removal, and +0x08 is an ARRAY BASE
// indexed by a 0x38 stride. `_layerCapacity` has NO reader in this body and is
// named by position in the triple, not by a measurement — stated plainly.
//
// +0x18 / +0x28 / +0x38 and the 0x48 stride are INHERITED, not measured here:
// see this file's banner.
// ============================================================================
struct AnimationState {
    // +0x00  "the unique node set is stale". Set to 1 when there is exactly one
    //        layer whose handle is >= 0; when non-zero, Update calls
    //        CopyInstancedToUnique and clears it.
    uint8_t  needsUniqueCopy;
    uint8_t  _pad_01[7];

    AnimationLayer* layers;    // +0x08  Array<AnimationLayer>::_data
    int32_t         layerCount;      // +0x10  ::_size
    int32_t         layerCapacity;   // +0x14  ::_capacity — NO reader in Update

    // +0x18  Math::Vector3 the caller hands back in as Update's 6th parameter.
    //        INHERITED from wave23_lane_gfxbody.cpp §5.1, not measured here.
    float    rootOffset[3];
    int32_t  _unpinned_24;

    // +0x28 / +0x38  the two buffers the caller passes as Update's 4th and 5th
    //        parameters (note the caller passes +0x38 FIRST).
    //
    //        *** CORRECTED 2026-09-12 (cutover closure batch 3). *** Each of
    //        these is a 16-byte `Collection::Array` HEADER, not a bare
    //        pointer: +0x28 is `Array<Math::Transform>` (the baked pose) and
    //        +0x38 is `Array<InterpolatedState<Transform>>` (the per-node
    //        interpolation state). What this header used to call `interpB` /
    //        `interpA` is the Array's `_data` field in each case, so the
    //        POINTER VALUE it described was right and the TYPE was wrong.
    //
    //        Three independent bodies say so, and all three were read for the
    //        correction rather than inferred from each other:
    //          * `Array<AnimationState>::Serialize` (0x18005c2c0) resizes
    //            `this+0x38` through `Array<InterpolatedState>::SetSize`
    //            (0x18005ac50) with the count it reads at `this+0x40`, and
    //            hands `this+0x28` whole to `Array<Transform>::Serialize`
    //            (0x180059820).
    //          * `?GetPose@AnimationManager@Graphics@@` (0x1800fb8e0) loads
    //            `state+0x28` as a BUFFER BASE and walks it at stride 0x1c.
    //          * the element-destroy helper 0x18005bce0 — array_destroy.cpp's
    //            `kD_InnerArr3_48` — frees three inner Array buffers per 0x48
    //            record, at offsets {0x38, 0x28, 0x08}.
    //        The full declaration is runtime/types/graphics/AnimationManager.h.
    Collection::Array<uint8_t> pose;   // +0x28  Array<Math::Transform>
    Collection::Array<uint8_t> nodes;  // +0x38  Array<InterpolatedState<Transform>>
};
static_assert(sizeof(AnimationState) == 0x48,
              "Graphics::AnimationState stride is 0x48 — INHERITED from WAVE 23 "
              "lane GFXBODY's read of AnimationManager::Update, not measured by "
              "lane GFXBIG");
static_assert(offsetof(AnimationState, layers)        == 0x08, "AnimationState::layers @ +0x08");
static_assert(offsetof(AnimationState, layerCount)    == 0x10, "AnimationState::layerCount @ +0x10");
static_assert(offsetof(AnimationState, layerCapacity) == 0x14, "AnimationState::layerCapacity @ +0x14");
static_assert(offsetof(AnimationState, rootOffset)    == 0x18, "AnimationState::rootOffset @ +0x18");
static_assert(offsetof(AnimationState, pose)          == 0x28, "AnimationState::pose @ +0x28");
static_assert(offsetof(AnimationState, nodes)         == 0x38, "AnimationState::nodes @ +0x38");

// ============================================================================
// Math::InterpolatedState<Math::Transform> — 0x38 bytes, two Math::Transform.
//
// MEASURED from Update's dt==0 arm (0x1800fdc50..0x1800fdcb1): the iteration
// step is `LEA RBX,[RBX+0x38]` and the arm copies the seven dwords at
// `base+0x00..base+0x1b` to `base+0x1c..base+0x37`. A Math::Transform is 0x1c
// bytes (quaternion then Vector3) — already asserted in wave23_lane_gfxbody.cpp
// — and 2 * 0x1c == 0x38 exactly.
// ============================================================================
// The two members are named BY POSITION (`first` / `second`), not by role. The
// only thing Update's dt==0 arm proves is the COPY DIRECTION — +0x00 is the
// source and +0x1c the destination. Calling them "previous" and "current" would
// be an inference about which one the interpolator samples, and no body this
// lane read establishes that (reference_a_writer_beats_readers_agreeing_on_a_
// sign.md: a single writer does not name a field's meaning either).
// *** THIS IS NOW THE TREE'S ONE AND ONLY DEFINITION. *** ENDGAME B.3.5 block GC
// collapsed a LATENT ODR DEFECT: `Mesh.h` carried a SECOND
// `RtGraphics::InterpolatedTransform`, same 0x38 size and same offsets but spelled
// `Transform _state[2]`, with neither guarding against the other. The two
// coexisted only because no translation unit had ever included both headers;
// `GraphicsMesh.h`'s banner records block G hitting the redefinition and routing
// around it rather than editing two other blocks' files. Block GC hit it for real
// — `animationstate_transforms.cpp` needs `Mesh.h` for `Graphics::MeshInstance`
// AND this header for `Graphics::AnimationState` — so `Mesh.h` now INCLUDES this
// header instead of redeclaring the type.
//
// THE FIELD NAMES ARE THESE ONES AND THE SEMANTICS ARE Mesh.h's, which is the
// stronger read of the two. `?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@`
// (0x18001aff0, eleven instructions) calls `Math::Transform::Lerp` with `this+0x1c`
// as the RECEIVER and `this+0x00` as the argument, and `Transform::Lerp`'s
// SUBSS/MULSS/ADDSS order (0x18001d6d8..0x18001d6fa) makes the receiver the FROM
// end. So the interpolation runs FROM +0x1c TO +0x00: **+0x00 is CURRENT and +0x1c
// is PREVIOUS**, the ordinary render-interpolation arrangement, and it agrees with
// `GetNodeToWorld` reading the first one. The POSITIONAL names are kept because
// they are the live spelling in `wave27_lane_gfxbig.cpp` and `mesh.cpp` (nothing
// ever used `_state[...]` as a member access — only comments named it), and the
// meaning lives in the comments, where the evidence travels with it.
struct InterpolatedTransform {
    float first[7];    // +0x00  Math::Transform — CURRENT (the dt==0 copy's SOURCE)
    float second[7];   // +0x1c  Math::Transform — PREVIOUS (its DESTINATION)
};
static_assert(sizeof(InterpolatedTransform) == 0x38,
              "InterpolatedState<Math::Transform> is two 0x1c Transforms");
static_assert(offsetof(InterpolatedTransform, second) == 0x1c,
              "InterpolatedTransform::second @ +0x1c");

}  // namespace RtGraphics

// ############################################################################
// ENDGAME B.3.5 step 5, block H1 — the REAL `Graphics::AnimationState`.
//
// The three `RtGraphics::` PODs that describe the same 0x48 bytes all KEEP
// LIVING (B4b-1): `RtGraphics::AnimationState` above (lane GFXBIG's, the view
// `Update` reads), `RtGraphics::AnimStateRec` in AnimationManager.h (the
// serializer's view) and `RtGraphics::AnimationLayer` above (the nested record).
// `tests/unit/graphics_animation_abi_test.cpp` cross-asserts the class against
// all three so none of them can drift.
//
// *** THE 0x18-BYTE PREFIX RELATIONSHIP, RECORDED HERE BECAUSE THE OTHER END OF
// IT IS NOT THIS BLOCK'S TO TOUCH. *** `runtime/types/graphics/Mesh.h:134-139`
// embeds a 0x18-byte prefix of this record inside `Graphics::MeshInstance` as
// `MeshAnimationState` — the bool at +0x00, the `Array<Layer>` header at +0x08
// and nothing past +0x18. That header belongs to block b35-E (the Geometry
// tree) and block H1 does not edit it. The consequence a later block must
// honour: `MeshAnimationState` is a PREFIX VIEW, not the class. It is NOT a
// duplicate POD in B34_RULES section 1.5.2's sense that could be collapsed by
// including this header, because it genuinely describes fewer bytes — a
// `MeshInstance` carries the first 0x18 of an `AnimationState` inline and the
// rest of the record lives in the AnimationManager's instance pool. B35_RULES
// section 13's line about it ("a PREFIX view, not the class") stands, and the
// unit test pins `offsetof(_rootOffset) == 0x18` so the boundary cannot move
// without failing.
//
// NOTHING IS RETIRED. There is no `??_7AnimationState@Graphics@@6B@`, no
// `_typeInfo`, no deleting-dtor row — the class is NON-POLYMORPHIC (both its
// constructors and its destructor are `QEAA`). The `?_singleton@AnimationManager
// @Graphics@@` cell three of these bodies read stays an `RT_DATA_SLOT` read per
// B35_RULES section 9 / RULE B4b-4.
//
// GREP HAZARD: inside a `namespace Graphics {` block a bare `AnimationState` or
// `AnimationLayer` now binds the CLASS. Spell the PODs `RtGraphics::` there.
// ############################################################################

namespace Graphics {

// ============================================================================
// Graphics::AnimationState — 0x48 bytes, NO vtable.
//
// NO REAL CONSTRUCTOR OR DESTRUCTOR IS DEFINED, and that is not B4b-2's
// vtable argument (there is no vtable to get wrong) — it is the weaker and
// sufficient one: `??0AnimationState@Graphics@@QEAA@XZ` (0x1800fdaf0),
// `??0…@AEBV01@@Z` (0x1800168d0) and `??1…@QEAA@XZ` (0x1800fe380) are UNPORTED
// and UNCALLED in this tree, and an uncalled port is an unverified port. Every
// live one is an element of a `Collection::Array<AnimationState>` whose
// construction, copy and destruction run through the REGISTERED rows at retail's
// own addresses (`0x18005b660` copy-ctor and `kD_InnerArr3_48` destroy, both in
// this block's files). A C++ constructor or destructor here would add a SECOND
// element lifecycle beside the ported one. They are declared for the record and
// deliberately left undefined.
// ============================================================================
class AnimationState {
public:
    // ------------------------------------------------------------------------
    // Graphics::AnimationState::Layer — one playing layer, stride 0x38.
    //
    // Retail's own nested name: `Array<AnimationState::Layer>::Serialize` is
    // 0x18005cb40 and `…::SetSize` is 0x18005c8a0. The stride is MEASURED three
    // independent ways inside `Update` — see `RtGraphics::AnimationLayer`'s
    // banner above, which this declaration mirrors field for field.
    // ------------------------------------------------------------------------
    struct Layer {
        AnimationGroup* _group;  // +0x00  Graphics::AnimationGroup* (a raw copy;
                                 //        the reference is the layer's, not the
                                 //        instance cache entry's)
        int32_t _clipIndex;      // +0x08  index into _group->_clips, scaled 0x70
        int32_t _handle;         // +0x0c  packed (instance << 8 | frame), or < 0
        float   _speed;          // +0x10
        int32_t _unpinned_14;    // +0x14  no reader in any body read
        // B.4: System::Time, retail's type -- `GetPoseAtTime` takes `+0x18` as
        // its `const Time&` (0x1800fcc20) and the fade weight reads the other two
        // as tick counts.
        System::Time _time;          // +0x18
        System::Time _fadeLength;    // +0x20  > 0 is what ARMS the countdown
        System::Time _fadeRemaining; // +0x28  <= 0 removes the layer
        bool    _restarted;      // +0x30  bool: SerializeBool (0x18005cb40)
        uint8_t _unpinned_31[7]; // +0x31
    };

    // ---- the members retail exports ----------------------------------------
    // ??0AnimationState@Graphics@@QEAA@XZ        0x1800fdaf0  NOT PORTED
    // ??0AnimationState@Graphics@@QEAA@AEBV01@@Z 0x1800168d0  NOT PORTED
    //   (the UNEXPORTED array-element copy constructor at 0x18005b660 IS ported,
    //    as `RtGraphics_AnimState_CopyCtor` in animationstate.cpp — a different
    //    address and a different function)
    // ??1AnimationState@Graphics@@QEAA@XZ        0x1800fe380  NOT PORTED
    // ??4AnimationState@Graphics@@QEAAAEAV01@AEBV01@@Z 0x180016910 NOT PORTED
    AnimationState();
    AnimationState(const AnimationState&);
    ~AnimationState();
    AnimationState& operator=(const AnimationState&);

    // ?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z 0x1800fe250
    //   PORTED HERE — the body moved out of animationstate.cpp's
    //   `RtGraphics_AnimationState_Serialize`.
    //
    // *** IT SERIALIZES ONLY THE LAYERS AND THE BOOL. *** `_pose`, `_nodes` and
    // `_rootOffset` are derived state that `Update` recomputes next tick.
    void Serialize(IO::Serializer& s);

    // ?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ  0x1800fd930
    //   PORTED HERE — the body moved out of animationstate.cpp.
    void ResetAnimationState();

    // ?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup
    // @Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@
    // AEBVTime@System@@2M_N@Z   0x1800fdf50
    //
    // PORTED — the body IS this member, in animationstate_transforms.cpp's
    // `namespace Graphics` block, and the registered door delegates to it
    // (ENDGAME B.3.5 block GC). This note used to say the body stayed put
    // because it carried its lane's diff-oracle scaffolding and "moving a body
    // out from under its own scaffolding is a second change riding on this one".
    // THAT SCAFFOLDING IS GONE: the five replay captures in that file had no
    // reader anywhere in the tree — they were its five -Wunused-variable
    // warnings — so the move is a move again. The narrative that explains WHY a
    // replay against the DLL would be destructive is kept at the body.
    //
    // THE 5th AND 6th PARAMETERS ARE NEVER READ (the fade comes from the group's
    // own table, the speed is the literal 1.0f). Kept for retail's arity.
    //
    // `nodes` is retail's `const Collection::Array<GraphicsMesh::Node>&`, stride
    // 0x48. `GraphicsMesh` is block G's class and is deliberately NOT named here
    // (the H1 brief: forward-declare, do not convert), so the parameter is the
    // raw 16-byte Array header every ported body in this cluster already uses.
    void PlayAnimation(Object::ExternalPtr<AnimationGroup> group, int animIndex,
                       const Collection::Array<GraphicsMesh::Node>& nodes,
                       const System::Time& startTime, const System::Time& fadeTime,
                       float speed, bool instanced);

    // ?Update@AnimationState@Graphics@@QEAA_N…   0x1800fdb20
    //   PORTED — the body IS this member, in wave27_lane_gfxbig.cpp's
    //   `namespace Graphics` block (ENDGAME B.3.5 block GC). Its three calls onto
    //   this class's own `CopyInstancedToUnique` / `UpdateRootOnly` /
    //   `UpdateNodes` are QUALIFIED member calls now, not three resolver hits.
    // B.4: retail's parameter types (the mangled name's `PEAV?$InterpolatedState@
    // VTransform@Math@@@2@PEAV78@`): the node transforms, then the pose buffer.
    bool Update(const System::Time& dt, const Collection::Array<GraphicsMesh::Node>& nodes,
                const Math::Transform& root, InterpolatedState<Math::Transform>* interp,
                Math::Transform* pose, Math::Vector3& rootOffsetOut, bool rootOnly,
                bool flag8);

    // ?GetAnimationElapsed@          0x1800fe220  NOT PORTED
    float GetAnimationElapsed() const;
    // ?GetCurrentAnimationIndex@     0x1800fda90  NOT PORTED
    int32_t GetCurrentAnimationIndex() const;
    // ?GetCurrentAnimationTimeScale@ 0x1800fda70  NOT PORTED
    float GetCurrentAnimationTimeScale() const;
    // ?SetCurrentAnimationTimeScale@ 0x1800fda50  NOT PORTED
    void SetCurrentAnimationTimeScale(float scale);
    // ?GetInstanceId@                0x1800168b0  NOT PORTED
    int32_t GetInstanceId() const;
    // ?GetInstanceTransforms@        0x1800fe2d0  NOT PORTED
    //   retail returns `const InterpolatedState<Transform>*`
    const void* GetInstanceTransforms() const;
    // ?InstancedRootAnimationOffset@ 0x1800fe320  NOT PORTED — hidden retslot
    Math::Vector3 InstancedRootAnimationOffset() const;
    // ?IsAnimating@                  0x1800fdab0  NOT PORTED
    bool IsAnimating() const;
    // ?IsCurrentAnimationLooping@    0x1800fda20  NOT PORTED
    bool IsCurrentAnimationLooping() const;
    // ?IsInstanced@                  0x1800168a0  NOT PORTED
    bool IsInstanced() const;

    // The three `AEAA`/`AEBA` members — retail PRIVATE, and the mangling says so
    // (`A` is the private access code). This DLL exports its private members
    // (reference_aeaa_is_private_not_unexported.md).
    //
    // THEY ARE PUBLIC HERE, not private, and block GC changed that when it moved
    // their bodies onto the members: each one's registered door is an
    // `extern "C"` FREE function that has to make the qualified call, and a free
    // function cannot be a friend of this class without a declaration this
    // header does not need. That is exactly the reason and the wording block G
    // recorded for `MaterialInstance::FindConstant` / `FindTexture`
    // (`wave28_lane_gfxbody.cpp`), and retail's own access code is kept in the
    // comment rather than in the C++.
    //
    // ALL THREE BODIES ARE NOW MEMBERS. `CopyInstancedToUnique` and
    // `UpdateNodes` are in `animationstate_transforms.cpp`, `UpdateRootOnly` in
    // `wave28_lane_gfxbody.cpp` — each in that file's `namespace Graphics` block,
    // with its lane's attendance counters left in the door.
    //
    // ?CopyInstancedToUnique@ 0x1800fd4c0 — `this` IS NEVER READ (RCX is
    //   overwritten four instructions in); the parameter stays for the ABI.
    void CopyInstancedToUnique(const Collection::Array<GraphicsMesh::Node>& nodes,
                               const Math::Transform& root,
                               InterpolatedState<Math::Transform>* interp,
                               int handle) const;
    // ?UpdateNodes@    0x1800fcc20
    void UpdateNodes(float speed, const Collection::Array<GraphicsMesh::Node>& nodes,
                     const Math::Transform& root,
                     InterpolatedState<Math::Transform>* interp, Math::Transform* pose,
                     Math::Vector3& rootOffsetOut);
    // ?UpdateRootOnly@ 0x1800fc870 — body in wave28_lane_gfxbody.cpp
    void UpdateRootOnly(float speed, Math::Vector3& rootOffsetOut);

    // ---- the 0x48 bytes ----------------------------------------------------
    // PUBLIC for the same reason `IO::Serializer`'s are: unconverted bodies
    // across `runtime/src/graphics/` reach them at these offsets through a cast.
    //
    // The three Array members are the RAW 16-byte header, not `Array<Layer>` /
    // `Array<Math::Transform>` / `Array<InterpolatedState<Transform>>`, for the
    // reason `AnimStateRec` already spells them that way: element lifecycle for
    // all three runs through the REGISTERED `kSpec_*` / `kD_InnerArr3_48` rows at
    // retail's addresses, and the typed form would make the compiler emit a
    // second one beside the ported one.

    // +0x00  "the unique node set is stale". Set to 1 when there is exactly one
    //        layer whose handle is >= 0; `Update` then calls
    //        `CopyInstancedToUnique` and clears it.
    bool     _needsUniqueCopy;   // B.4: bool -- retail's SerializeBool (0x1800fe250)
    uint8_t  _pad01[7];
    //
    // B.4: the three Arrays carry their ELEMENT TYPE now. No lifecycle changed:
    // this class's ctors and dtor are declared and never defined, so no member
    // ctor / dtor of these Arrays is ever instantiated through it; element
    // construction, copy and destruction still run through the registered
    // `rtcoll::Rt_*` rows at retail's addresses (array_graphics_anim.cpp).
    Collection::Array<Layer> _layers;    // +0x08  stride 0x38
    Math::Vector3 _rootOffset;           // +0x18
    int32_t  _pad24;                     // +0x24  the Array triple's _capacity half
    Collection::Array<Math::Transform> _pose;   // +0x28  stride 0x1c
    Collection::Array<InterpolatedState<Math::Transform>> _nodes;  // +0x38  stride 0x38
};
static_assert(sizeof(AnimationState) == 0x48,
              "Graphics::AnimationState is 0x48 bytes -- the stride AnimationManager"
              "::Update walks and the record kD_InnerArr3_48 destroys");
static_assert(alignof(AnimationState) == 8, "Graphics::AnimationState aligns to 8");
static_assert(sizeof(AnimationState::Layer) == 0x38,
              "Graphics::AnimationState::Layer stride is 0x38");
static_assert(offsetof(AnimationState, _layers)     == 0x08, "AnimationState::_layers @ +0x08");
static_assert(offsetof(AnimationState, _rootOffset) == 0x18, "AnimationState::_rootOffset @ +0x18");
static_assert(offsetof(AnimationState, _pose)       == 0x28, "AnimationState::_pose @ +0x28");
static_assert(offsetof(AnimationState, _nodes)      == 0x38, "AnimationState::_nodes @ +0x38");
static_assert(offsetof(AnimationState::Layer, _clipIndex)     == 0x08, "Layer::_clipIndex @ +0x08");
static_assert(offsetof(AnimationState::Layer, _handle)        == 0x0c, "Layer::_handle @ +0x0c");
static_assert(offsetof(AnimationState::Layer, _speed)         == 0x10, "Layer::_speed @ +0x10");
static_assert(offsetof(AnimationState::Layer, _time)          == 0x18, "Layer::_time @ +0x18");
static_assert(offsetof(AnimationState::Layer, _fadeLength)    == 0x20, "Layer::_fadeLength @ +0x20");
static_assert(offsetof(AnimationState::Layer, _fadeRemaining) == 0x28, "Layer::_fadeRemaining @ +0x28");
static_assert(offsetof(AnimationState::Layer, _restarted)     == 0x30, "Layer::_restarted @ +0x30");
// The class and all three PODs describe the same bytes.
static_assert(sizeof(AnimationState) == sizeof(RtGraphics::AnimationState), "alias GFXBIG's POD");
static_assert(sizeof(AnimationState::Layer) == sizeof(RtGraphics::AnimationLayer), "alias Layer");
static_assert(offsetof(AnimationState, _layers) ==
              offsetof(RtGraphics::AnimationState, layers), "alias layers");
static_assert(offsetof(AnimationState, _rootOffset) ==
              offsetof(RtGraphics::AnimationState, rootOffset), "alias rootOffset");
static_assert(!std::is_polymorphic<AnimationState>::value,
              "Graphics::AnimationState must never acquire a vtable: retail exports "
              "no ??_7AnimationState@Graphics@@6B@ and its ctor/dtor are QEAA");
static_assert(std::is_standard_layout<AnimationState>::value,
              "the ported doors reach these fields through a reinterpret_cast");

}  // namespace Graphics

// ==== B3R W0-P0c: Runtime bodies reached by retail address ====
// ENDGAME B.3R (B3R_RULES R5): each body below is registered at the retail
// address in its trailing comment and read through the registry today. It is
// declared here, copied from its definition (linkage, namespace, types), so the
// reader can call it by name. Nothing calls it through this declaration yet.
namespace rt { namespace io { struct Serializer; } }   // io/Serializer.h

// ---- graphics/animationstate.cpp: three `Array<T>::Serialize` instantiations
// (Collection::Array has no Serialize member -- Collection.h's banner), typed
// by the element retail's instantiation names (B.4).
void RtGraphics_SerializeMeshNodes(IO::Serializer& ser, int count,
                                   Graphics::GraphicsMesh::Node* base);  // 0x18005c120
void RtGraphics_SerializeTransformArray(Collection::Array<Math::Transform>* a,
                                        IO::Serializer& ser);  // 0x180059820
// ==== end B3R W0-P0c ====

// ==== B3R RT-VG: the graphics-anim Collection::Array instantiations ====
// Defined in runtime/src/collection/array_graphics_anim.cpp (given external
// linkage for their direct readers in runtime/src/graphics/).
namespace rtcoll {
void* Rt_ArrayDtor_18001c480(void* a);  // 0x18001c480 ~Array<AnimationState::Layer>
void* Rt_ArrayDtor_18001c550(void* a);  // 0x18001c550 ~Array<InterpolatedState<Transform>>
void* Rt_ArrayDtor_180014090(void* a);  // 0x180014090 ~Array<GraphicsMesh::Node>
void* Rt_ArrayDtor_18005bbb0(void* a);  // 0x18005bbb0 ~Array<AnimationState>
void Rt_SetSize_18005aa30(void* a, int n);  // 0x18005aa30 Array<GraphicsMesh::Node>
void Rt_SetSize_18005b1e0(void* a, int n);  // 0x18005b1e0 Array<AnimationState>
void Rt_SetSize_18005c8a0(void* a, int n);  // 0x18005c8a0 Array<AnimationState::Layer>
void Rt_SetSize_18005ac50(void* a, int n);  // 0x18005ac50 Array<InterpolatedState<Transform>>
void Rt_SetSize_18005c3d0(void* a, int n);  // 0x18005c3d0 Array<Instance>
void Rt_SetSize_18005be30(void* a, int n, const void* fill);  // 0x18005be30 Array<Instance>, fill
void* Rt_ArrayAssign_180013960(void* d, const void* s);  // 0x180013960 Array<GraphicsMesh::Node>
void* Rt_ArrayAssign_18005b130(void* d, const void* s);  // 0x18005b130 Array<AnimationState>
void* Rt_ArrayAssign_18001b700(void* d, const void* s);  // 0x18001b700 Array<AnimationState::Layer>
void* Rt_ArrayAssign_18001b810(void* d, const void* s);  // 0x18001b810 Array<InterpolatedState<Transform>>
}  // namespace rtcoll
// ---- graphics/wave27_lane_gfxbig.cpp: the Update door (it counts its calls
// for the [GFXBIG27] line, then calls Graphics::AnimationState::Update)
extern "C" bool RtGraphics_AnimationState_Update(
        Graphics::AnimationState* self, const System::Time* time,
        const Collection::Array<Graphics::GraphicsMesh::Node>* nodes,
        const Math::Transform* baseTransform,
        Graphics::InterpolatedState<Math::Transform>* interp, Math::Transform* pose,
        Math::Vector3* outVec3, bool rootOnly, bool applyEvents);  // 0x1800fdb20
// ==== end B3R RT-VG ====
