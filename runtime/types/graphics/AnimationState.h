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
    int32_t  frameRate;       // +0x50  read as an INT and cvtdq2ps'd to float
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
static_assert(offsetof(AnimationClipPrefix, frameRate)  == 0x50, "clip::frameRate @ +0x50");
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

    // +0x28 / +0x38  the two InterpolatedState<Math::Transform>* the caller
    //        passes as Update's 4th and 5th parameters (note the caller passes
    //        +0x38 FIRST). INHERITED, not measured here.
    void*    interpB;          // +0x28
    uint8_t  _unpinned_30[8];  // +0x30
    void*    interpA;          // +0x38
    uint8_t  _unpinned_40[8];  // +0x40
};
static_assert(sizeof(AnimationState) == 0x48,
              "Graphics::AnimationState stride is 0x48 — INHERITED from WAVE 23 "
              "lane GFXBODY's read of AnimationManager::Update, not measured by "
              "lane GFXBIG");
static_assert(offsetof(AnimationState, layers)        == 0x08, "AnimationState::layers @ +0x08");
static_assert(offsetof(AnimationState, layerCount)    == 0x10, "AnimationState::layerCount @ +0x10");
static_assert(offsetof(AnimationState, layerCapacity) == 0x14, "AnimationState::layerCapacity @ +0x14");
static_assert(offsetof(AnimationState, rootOffset)    == 0x18, "AnimationState::rootOffset @ +0x18");
static_assert(offsetof(AnimationState, interpB)       == 0x28, "AnimationState::interpB @ +0x28");
static_assert(offsetof(AnimationState, interpA)       == 0x38, "AnimationState::interpA @ +0x38");

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
struct InterpolatedTransform {
    float first[7];    // +0x00  Math::Transform — the SOURCE of the dt==0 copy
    float second[7];   // +0x1c  Math::Transform — the DESTINATION
};
static_assert(sizeof(InterpolatedTransform) == 0x38,
              "InterpolatedState<Math::Transform> is two 0x1c Transforms");
static_assert(offsetof(InterpolatedTransform, second) == 0x1c,
              "InterpolatedTransform::second @ +0x1c");

}  // namespace RtGraphics
