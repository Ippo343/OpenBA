// ============================================================================
// runtime/src/graphics/animationstate_transforms.cpp
// The five Graphics animation bodies that carry hand-scheduled floating point
// (cutover closure batch 3b, 2026-09-12).
// ============================================================================
//
// WHAT IS IN HERE, with retail addresses and the export name each registers:
//
//   0x1800fdf50  ?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@
//                VAnimationGroup@Graphics@@@Object@@HAEBV?$Array@UNode@
//                GraphicsMesh@Graphics@@@Collection@@AEBVTime@System@@2M_N@Z
//   0x1800fb9d0  ?GetInstanceId@AnimationManager@Graphics@@QEAAHV?$ExternalPtr@
//                VAnimationGroup@Graphics@@@Object@@HAEBV?$Array@UNode@
//                GraphicsMesh@Graphics@@@Collection@@@Z
//   0x1800fd4c0  ?CopyInstancedToUnique@AnimationState@Graphics@@AEBAXAEBV
//                ?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBV
//                Transform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@H@Z
//   0x1800fcc20  ?UpdateNodes@AnimationState@Graphics@@AEAAXMAEBV?$Array@UNode@
//                GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV
//                ?$InterpolatedState@VTransform@Math@@@2@PEAV56@AEAVVector3@6@@Z
//   0x180100070  ?GetBones@MeshInstance@Graphics@@QEBAHMAEBVTransform@Math@@
//                PEAVMatrix@4@@Z
//
// The serializable half of the same cluster is next door in animationstate.cpp;
// its Array<T> instantiations are in runtime/src/collection/array_graphics_anim.cpp
// and the record layouts are pinned in runtime/types/graphics/AnimationManager.h.
// Nothing here re-derives a layout those three already assert.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A SEPARATE FILE, AND WHY THE ARITHMETIC IS WRITTEN OUT LONGHAND
// ---------------------------------------------------------------------------
// Four of the five inline a quaternion multiply of a PURE quaternion (the
// vector to rotate, with w == 0) by a parent rotation, then multiply the result
// by the parent's conjugate — the classic `q * v * q^-1` rotate — and MSVC
// hand-scheduled each of those four DIFFERENTLY. Single-precision addition is
// not associative, so `a + b + c` and `a + (b + c)` are different floats, and a
// pose that is one ULP off on a bone propagates down the skeleton.
//
// So every `mulss` / `addss` / `subss` in the instruction stream is one C
// statement on a `float` here, in the instruction stream's order, with no fused
// expressions and no reassociation. The `xorps reg,[0x1801617c0]` sign flips
// (that constant is four -0.0f, read out of the DLL) are written as unary
// minus on a float, which is the same bit operation. The compiler is not asked
// to contract anything: the project builds with no `-ffp-contract` override and
// no `-mfma`, so the x86-64 baseline has no FMA to contract into, and this file
// adds no flags of its own (Makefile SA_CXXFLAGS, unchanged).
//
// A shared helper was considered and rejected on purpose: the four schedules
// are NOT the same order, so one helper would silently impose one of them on
// the other three. runtime/src/math/transform.cpp already holds the project's
// ported `Math::Transform::operator*` with its OWN order, and it is likewise
// not reused here for the inlined copies — only for the real `CALL`s retail
// makes, which go through the port registry so our own body answers.
//
// ---------------------------------------------------------------------------
// THE INSTANCE HANDLE
// ---------------------------------------------------------------------------
// Same packed pair animationstate.cpp documents: `instanceIndex << 8 |
// frameIndex`, negative meaning "not instanced". GetInstanceId's last two
// instructions build it (`SHL ESI,0x8 / OR ESI,R14D`); PlayAnimation,
// CopyInstancedToUnique, UpdateNodes and GetBones all take it apart with
// `(h >> 8) & 0xff` and `h & 0xff`.
//
// ---------------------------------------------------------------------------
// HIDDEN RETURN SLOTS
// ---------------------------------------------------------------------------
// Four Math entry points here return a value by reference in RDX, which shifts
// the real arguments right (reference_hidden_retslot_wild_write.md):
//   ??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z   (this, ret, rhs)
//   ?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z  (this, ret, v)
//   ?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z (from, ret, to, &t)
//   ?Inverse@Transform@Math@@QEBA?AV12@XZ     (this, ret)
//   ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ (this, ret)
// Each returns the slot pointer in RAX, and retail reads the result back
// through RAX rather than through the buffer it passed; that is reproduced.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/math/Math.h"
#include "../../types/collection/Collection.h"
#include "../../types/graphics/AnimationManager.h"
#include "../../types/graphics/AnimationState.h"   // ENDGAME B.3.5 block GC
#include "../../types/graphics/Mesh.h"             // Graphics::MeshInstance, ditto
#include "../../types/graphics/InterpolatedState.h"  // ENDGAME B.3.5 closing pass:
                                                     // Graphics::InterpolatedState<T>
#include <new>                                       // placement new, for the
                                                     // hidden-retslot door below
#include "../../types/system/Value.h"                // System::Time, ditto
#include "../../types/system/Timer.h"   // ENDGAME B.3.9 SS1: Timer::_secondsPerTick
#include "../../types/graphics/Animation.h"   // Graphics::Animation::GetPoseAtTime

using RtGraphics::AnimationManagerView;

namespace {

using ArrayHdr = Collection::Array<uint8_t>;


// ---------------------------------------------------------------------------
// Record strides and field offsets, all of them already asserted next door.
// ---------------------------------------------------------------------------
constexpr int kInstanceStride  = 0x30;   // AnimationManager::Instance
constexpr int kStateStride     = 0x48;   // AnimationState
constexpr int kLayerStride     = 0x38;   // AnimationState::Layer
constexpr int kNodeStride      = 0x48;   // GraphicsMesh::Node
constexpr int kInterpStride    = 0x38;   // InterpolatedState<Math::Transform>
constexpr int kTransformStride = 0x1c;   // Math::Transform
constexpr int kClipStride      = 0x70;   // Graphics::Animation

// ---------------------------------------------------------------------------
// The neighbours these bodies call, by name (B3R): the Collection::Array<T>
// instantiations are rtcoll::Rt_SetSize_* / Rt_ArrayAssign_* / Rt_ArrayDtor_*
// (array_graphics_anim.cpp, declared in AnimationState.h) and
// rtcoll::RtColl_ArraySetSize_180059140 (Array<Math::Transform>::SetSize).
// 0x18005be30 is Array<AnimationManager::Instance>::SetSize, the THREE-argument
// form whose new tail elements are copy-assigned from `fill`.
// ---------------------------------------------------------------------------

// Math. ENDGAME B.3.1 step 2: all five are members of the real Math classes
// now (runtime/types/math/Math.h). They keep the `(self, ret, …) -> const
// float*` SHAPE, because every call site in this file works in raw float
// pointers into the untyped pose / node / interpolated-state storage that
// Graphics::AnimationState keeps; those B.3.1 bridges go when THAT class is
// converted (B.2), not here. The hidden return slot the banner describes is
// gone — each member returns by value and the assignment below is the store
// retail made through RDX.
inline const float* QuaternionMul(const void* self, void* ret, const void* rhs) {
    *static_cast<Math::Quaternion*>(ret) =
        *static_cast<const Math::Quaternion*>(self) *
        *static_cast<const Math::Quaternion*>(rhs);
    return static_cast<const float*>(ret);
}
inline const float* QuaternionRotate(const void* self, void* ret, const void* v) {
    *static_cast<Math::Vector3*>(ret) =
        static_cast<const Math::Quaternion*>(self)->Rotate(
            *static_cast<const Math::Vector3*>(v));
    return static_cast<const float*>(ret);
}
inline const float* QuaternionLerp(const void* from, void* ret, const void* to,
                                   const float* t) {
    *static_cast<Math::Quaternion*>(ret) =
        static_cast<const Math::Quaternion*>(from)->Lerp(
            *static_cast<const Math::Quaternion*>(to),
            *reinterpret_cast<const Math::Scalar*>(t));
    return static_cast<const float*>(ret);
}
inline const float* TransformInverse(const void* self, void* ret) {
    *static_cast<Math::Transform*>(ret) =
        static_cast<const Math::Transform*>(self)->Inverse();
    return static_cast<const float*>(ret);
}
inline const float* TransformAsMatrix(const void* self, void* ret) {
    *static_cast<Math::Matrix*>(ret) =
        static_cast<const Math::Transform*>(self)->AsMatrix();
    return static_cast<const float*>(ret);
}

// The three cluster bodies these five call, by name (B3R):
// Graphics::AnimationManager::GetPose (0x1800fb8e0) and
// Graphics::Animation::GetPoseAtTime (0x1800faca0) are members;
// ?Update@AnimationState@Graphics@@ (0x1800fdb20) is reached through its door
// RtGraphics_AnimationState_Update (wave27_lane_gfxbig.cpp), which counts the
// call for the [GFXBIG27] line before it calls the member.

// ?_singleton@AnimationManager@Graphics@@0PEAV12@EA — the slot at 0x1801fe7b0.
// ONE deref gives the manager; its +0x00 is the instance array header.
// ENDGAME B.3.9 GFX: the class static, not the mint's slot (D7 row A44).
AnimationManagerView* AnimationManagerSingleton() {
    return reinterpret_cast<AnimationManagerView*>(
        Graphics::AnimationManager::_singleton);   // B.3.9 bridge (the POD view)
}

// ?_secondsPerTick@Timer@System@@0NA — a DOUBLE at 0x1801fe728. Seconds->ticks
// is a DIVIDE by it here (`DIVSD`), where GetPoseAtTime next door multiplies;
// both spellings are retail's, in the direction each needs.
double SecondsPerTick() {
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    return ::System::Timer::_secondsPerTick;
}

// `CVTTSD2SI` after a `DIVSD` — seconds to System::Time ticks, truncating.
int64_t SecondsToTicks(double seconds) {
    return static_cast<int64_t>(seconds / SecondsPerTick());
}

void Copy7Dwords(void* dst, const void* src) {
    for (int w = 0; w < 7; ++w)
        static_cast<uint32_t*>(dst)[w] = static_cast<const uint32_t*>(src)[w];
}

}  // namespace

// ============================================================================
// ?PlayAnimation@AnimationState@Graphics@@  (0x1800fdf50)
// ============================================================================
//
// Push a clip onto the front of the layer list, cross-fading out whatever was
// playing. Four arms, and the interesting part is that they are chosen by the
// state of layer 0, not by the caller:
//
//   animIndex < 0   stop everything: drop every layer's reference on its shared
//                   instance and free the layer array. This is
//                   ResetAnimationState's body inlined, except that retail
//                   calls Array<Layer>::SetSize(0) here instead of inlining
//                   ~Array as ResetAnimationState does.
//   same LOOPING clip already on layer 0   nothing to do but reset the speed
//                   to 1.0f and return.
//   layer 0 has NOT restarted and there is more than one layer   re-arm the
//                   fade on layer 1 and overwrite layer 0 in place; no layer is
//                   inserted.
//   otherwise       arm the fade on layer 0, grow the array by one and shift
//                   every layer up so the new clip becomes layer 0.
//
// *** THE 6th AND 7th PARAMETERS ARE NEVER READ. *** The fade length comes from
// the group's own table (or its default at +0x30), never from the `Time&` the
// caller passes, and the new layer's speed is the literal 1.0f, never the
// caller's `float`. Both stay in the signature because retail's callers push
// them and the registered entry point must have retail's arity.
// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::AnimationState::PlayAnimation`, in this file's `namespace Graphics`
// block at the bottom. The door keeps its name, its registration and retail's
// arity, including the two parameters retail never reads.
extern "C" void RtGraphics_AnimationState_PlayAnimation(
    void* selfv, void* group, int animIndex, const void* nodesv,
    const int64_t* startTime, const int64_t* fadeTime,
    float speed, bool instanced) {
    reinterpret_cast<Graphics::AnimationState*>(selfv)                  // B.3.5 bridge
        ->Graphics::AnimationState::PlayAnimation(
            Object::ExternalPtr<Graphics::AnimationGroup>(
                reinterpret_cast<Graphics::AnimationGroup*>(group)),    // B.3.5 bridge
            animIndex,
            *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv),
            *reinterpret_cast<const System::Time*>(startTime),
            *reinterpret_cast<const System::Time*>(fadeTime),
            speed, instanced);
}

// ============================================================================
// ?GetInstanceId@AnimationManager@Graphics@@  (0x1800fb9d0)
// ============================================================================
//
// Find — or bake — the shared instance for (group, clip) and return its packed
// handle. Baking is the expensive half: for every frame of the clip it builds
// the skeleton's world pose once and stores it, so that a hundred citizens
// playing the same walk cycle sample a table instead of each running the
// hierarchy.
//
// THE SEARCH PASS DOES TWO THINGS AT ONCE. It looks for a live entry whose
// group and clip index match, and in the same sweep remembers the FIRST entry
// whose group is null — the slot ResetAnimationState/PlayAnimation blanked when
// its last reference went away. A hit returns immediately; a miss reuses that
// blank slot, and only if there is none does the array grow by one.
//
// THE SECOND RETURN COMPONENT IS NOT ALWAYS ZERO. After bumping the reference
// count the body sweeps the instance's baked states looking for the one whose
// layer 0 has the SMALLEST playback time (seeded with 10000.0f from
// 0x180161734), and packs that state's index into the low byte. A state with no
// layers scores 0.0f, which wins outright.
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::AnimationManager::GetInstanceId`, in this file's `namespace Graphics`
// block. Block H1 left it a door for the "NOT DIFFABLE" scaffolding reason; the
// NOT-DIFFABLE narrative is documentation and travelled with the body, and there
// was no oracle machinery here to move out from under (the replay captures this
// file carried went in block GC commit 2). The door keeps its name and its
// registration.
extern "C" int RtGraphics_AnimationManager_GetInstanceId(
    void* selfv, void* group, int animIndex, const void* nodesv) {
    return reinterpret_cast<Graphics::AnimationManager*>(selfv)          // B.3.5 bridge
        ->Graphics::AnimationManager::GetInstanceId(
            Object::ExternalPtr<Graphics::AnimationGroup>(
                reinterpret_cast<Graphics::AnimationGroup*>(group)),      // B.3.5 bridge
            animIndex,
            *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv));
}

// ============================================================================
// ?CopyInstancedToUnique@AnimationState@Graphics@@  (0x1800fd4c0)
// ============================================================================
//
// Re-root a baked, shared pose onto one instance's own node array: walk the
// skeleton once, composing each node's baked local transform with its parent's
// freshly computed world transform, and write the result into the caller's
// InterpolatedState array (shifting each entry's current value into its
// previous half first, which is what keeps interpolation continuous).
//
// *** `this` IS NEVER READ. *** RCX is overwritten with the root transform four
// instructions in. The method is declared const and private and takes
// everything it needs as arguments; the parameter stays for the ABI.
//
// Node 0 is done with the two real Math CALLs retail makes (Quaternion::Rotate
// then operator*); nodes 1..n-1 have the rotate INLINED, and that inlined form
// is the one whose schedule is transcribed below.
// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::AnimationState::CopyInstancedToUnique`, at the bottom of this file.
// The door keeps its name, its registration and retail's `this` parameter, which
// the body still never reads.
extern "C" void RtGraphics_AnimationState_CopyInstancedToUnique(
    const void* selfv, const void* nodesv, const void* rootv,
    void* interpv, int handle) {
    reinterpret_cast<const Graphics::AnimationState*>(selfv)            // B.3.5 bridge
        ->Graphics::AnimationState::CopyInstancedToUnique(
            *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv),
            *reinterpret_cast<const Math::Transform*>(rootv),
            interpv, handle);
}

// ============================================================================
// ?UpdateNodes@AnimationState@Graphics@@  (0x1800fcc20)
// ============================================================================
//
// Resolve every layer of a playing animation into one pose, then push that pose
// through the skeleton.
//
// Three phases, in order:
//   1. Sample the LAST layer into `pose`, either from its shared baked instance
//      (GetPose) or by sampling the clip directly (GetPoseAtTime).
//   2. Walk the remaining layers BACKWARDS, from count-2 down to 0. Each one is
//      sampled into a 48-entry scratch and `pose` is lerped FROM that scratch
//      TOWARDS its current value by `fadeRemaining / fadeLength` of the layer
//      ABOVE it — so as that layer's fade runs out the blend lands on the lower
//      layer's pose.
//   3. Hand pose[0]'s translation back as the root offset, reset pose[0] to the
//      identity, and compose every node's pose with its parent's, writing the
//      hierarchy into `interp`.
//
// *** THE SCRATCH IS 48 TRANSFORMS AND THE BOUND IS NEVER CHECKED. *** Retail
// re-initialises exactly 48 identity transforms at the top of every layer
// iteration and then fills `nodeCount` of them. A mesh with more than 48 nodes
// overruns it in retail too; the array here is the same 48 so that a build with
// more nodes fails the same way rather than differently.
// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::AnimationState::UpdateNodes`, at the bottom of this file. The door
// keeps its name, its registration and retail's argument order.
extern "C" void RtGraphics_AnimationState_UpdateNodes(
    void* selfv, float speed, const void* nodesv, const void* rootv,
    void* interpv, void* posev, float* rootOffset) {
    reinterpret_cast<Graphics::AnimationState*>(selfv)                  // B.3.5 bridge
        ->Graphics::AnimationState::UpdateNodes(
            speed,
            *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv),
            *reinterpret_cast<const Math::Transform*>(rootv),
            interpv, posev,
            *reinterpret_cast<Math::Vector3*>(rootOffset));             // B.3.5 bridge
}

// ============================================================================
// ?GetBones@MeshInstance@Graphics@@  (0x180100070)
// ============================================================================
//
// Produce one skinning matrix per bone and return the bone count.
//
// Per bone: interpolate between the InterpolatedState's two halves by `t`,
// compose that with the node's BIND transform, compose THAT with the instance's
// root transform, and convert to a Matrix.
//
// THE TWO BRANCHES ON MeshInstance+0xd8 ARE THE SAME QUESTION ASKED TWICE.
// Non-zero means the instance is playing a shared, instanced clip: the node
// states come out of the AnimationManager's pool through the last layer's
// packed handle, and the root transform used is the IDENTITY. Zero means the
// instance owns its nodes (+0xf0) and the root transform is the INVERSE of the
// one the caller passed. Retail tests the byte three separate times rather than
// keeping the answer; reproduced.
//
// The root quaternion's four `q * 0.0f` products are hoisted out of the loop by
// MSVC and are kept hoisted here, because they are what the second rotate's
// accumulators are SEEDED with.
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::MeshInstance::GetBones` (block E's class; this file is not block
// E's). The door keeps its name and its registration.
extern "C" int RtGraphics_MeshInstance_GetBones(
    const void* selfv, float t, const void* rootv, void* outv) {
    return reinterpret_cast<const Graphics::MeshInstance*>(selfv)        // B.3.5 bridge
        ->Graphics::MeshInstance::GetBones(
            t, *reinterpret_cast<const Math::Transform*>(rootv),
            static_cast<Math::Matrix*>(outv));                           // B.3.5 bridge
}

// ============================================================================
// ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@
//  AEBVScalar@4@@Z   retail 0x18001aff0 — PORTED by ENDGAME B.3.5's closing pass.
//
// Never ported before: three caller batches reported the class as "no such class
// in runtime/types/graphics at all" and left two call sites on a resolved pointer.
// `runtime/types/graphics/InterpolatedState.h` now declares it, with the layout
// read from retail three ways and this body's own eleven instructions quoted.
//
// HIDDEN RETSLOT (`?AVTransform@Math@@` right after `QEBA`), so the door's
// signature is `(this, ret, args)` and it placement-constructs the member's
// prvalue into the caller's slot and returns the slot — B5-12's shape. Retail does
// the same thing by handing RDX straight to `Math::Transform::Lerp` and returning
// it in RAX; ours differs only in that the construction is spelled in C++.
// ============================================================================
extern "C" void* RtGraphics_InterpolatedState_Transform_Lerp(
    const void* selfv, void* ret, const void* tv) {
    const Graphics::InterpolatedState<Math::Transform>* self =            // B.3.5 bridge
        reinterpret_cast<const Graphics::InterpolatedState<Math::Transform>*>(selfv);
    ::new (ret) Math::Transform(
        self->Graphics::InterpolatedState<Math::Transform>::Lerp(
            *reinterpret_cast<const Math::Scalar*>(tv)));
    return ret;
}

// ============================================================================
// Registration. Unconditional (DIRECTION.md §2: a port registers, no flag).
// ============================================================================
int RtGraphicsAnimationTransformsRegister() {
    int n = 0;

    RegisterRuntimeExport(
        "?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup@"
        "Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@"
        "AEBVTime@System@@2M_N@Z",
        (void*)&RtGraphics_AnimationState_PlayAnimation);                       ++n;
    RegisterRuntimeAddr(0x1800fdf50ull, (void*)&RtGraphics_AnimationState_PlayAnimation);

    RegisterRuntimeExport(
        "?GetInstanceId@AnimationManager@Graphics@@QEAAHV?$ExternalPtr@VAnimationGroup@"
        "Graphics@@@Object@@HAEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@@Z",
        (void*)&RtGraphics_AnimationManager_GetInstanceId);                     ++n;
    RegisterRuntimeAddr(0x1800fb9d0ull, (void*)&RtGraphics_AnimationManager_GetInstanceId);

    RegisterRuntimeExport(
        "?CopyInstancedToUnique@AnimationState@Graphics@@AEBAXAEBV?$Array@UNode@"
        "GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@"
        "VTransform@Math@@@2@H@Z",
        (void*)&RtGraphics_AnimationState_CopyInstancedToUnique);               ++n;
    RegisterRuntimeAddr(0x1800fd4c0ull, (void*)&RtGraphics_AnimationState_CopyInstancedToUnique);

    RegisterRuntimeExport(
        "?UpdateNodes@AnimationState@Graphics@@AEAAXMAEBV?$Array@UNode@GraphicsMesh@"
        "Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@"
        "Math@@@2@PEAV56@AEAVVector3@6@@Z",
        (void*)&RtGraphics_AnimationState_UpdateNodes);                         ++n;
    RegisterRuntimeAddr(0x1800fcc20ull, (void*)&RtGraphics_AnimationState_UpdateNodes);

    RegisterRuntimeExport(
        "?GetBones@MeshInstance@Graphics@@QEBAHMAEBVTransform@Math@@PEAVMatrix@4@@Z",
        (void*)&RtGraphics_MeshInstance_GetBones);                              ++n;
    RegisterRuntimeAddr(0x180100070ull, (void*)&RtGraphics_MeshInstance_GetBones);

    RegisterRuntimeExport(
        "?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@"
        "AEBVScalar@4@@Z",
        (void*)&RtGraphics_InterpolatedState_Transform_Lerp);                   ++n;
    RegisterRuntimeAddr(0x18001aff0ull,
                        (void*)&RtGraphics_InterpolatedState_Transform_Lerp);

    return n;
}

// ############################################################################
// ENDGAME B.3.5 step 5, block GC — the `Graphics::AnimationState` member bodies
// whose live doors are the `extern "C"` functions above.
//
// Their own `namespace Graphics { }` block: a member definition has to appear in
// a namespace ENCLOSING the class, and `Graphics` / `RtGraphics` are siblings
// (B34_BRIEFS A.2). Every helper this file defines sits in the GLOBAL unnamed
// namespace, so unqualified lookup from here reaches all of them.
//
// Block H1 left these three bodies where they were because each carried its
// lane's diff-oracle scaffolding and "moving a body out from under its own
// scaffolding is a second change riding on this one". THE SCAFFOLDING IS GONE,
// and that is why the move is now a move and not two changes: the oracle
// captures (`replay`, `replayBytes`, `replayPose`, `replayInterp`,
// `replayOffset`, `poseBytes`, `interpBytes`) had NO READER anywhere in the tree
// — they were this file's five -Wunused-variable warnings — so they are deleted
// with the `<vector>` include they needed. The narrative paragraphs that explain
// WHY a replay against the DLL would be destructive are kept, because that is
// still true of the members.
// ############################################################################

namespace Graphics {

// ---------------------------------------------------------------------------
// `Graphics::InterpolatedState<Math::Transform>::Lerp` (retail 0x18001aff0) is a
// TEMPLATE member, so its explicit specialisation is defined `inline` in
// `runtime/types/graphics/InterpolatedState.h`, next to the class -- where a
// template's members belong, and where
// `tests/unit/graphics_interpolatedstate_abi_test.cpp` can reach it without
// linking this file's whole animation surface. The eleven retail instructions it
// was read from are quoted at the definition. The registered DOOR stays HERE
// (`RtGraphics_InterpolatedState_Transform_Lerp`, above), because this is the file
// that owns the animation-transform registrations.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ?PlayAnimation@AnimationState@Graphics@@  0x1800fdf50 — ENDGAME B.3.5 block GC.
// The four arms, the two unread parameters and the store-order asymmetry between
// arm A and arm B are all documented in the banner beside the door above.
//
// `group` is retail's `Object::ExternalPtr<AnimationGroup>` BY VALUE (8 bytes,
// trivially copyable, so it arrives in a register); the body reads it as the raw
// pointer retail does, because `Graphics::AnimationGroup`'s own fields are
// reached here through `RtGraphics::anim_group::k*`, not as members.
// ---------------------------------------------------------------------------
void AnimationState::PlayAnimation(Object::ExternalPtr<AnimationGroup> groupHandle,
                                   int animIndex,
                                   const Collection::Array<uint8_t>& nodesRef,
                                   const System::Time& startTime,
                                   const System::Time& /*fadeTime, unread*/,
                                   float /*speed, unread*/, bool instanced) {
    // NOT DIFFABLE against the DLL. The body mutates the layer array AND the
    // AnimationManager's shared instance pool (reference counts, SetSize);
    // running the original a second time on the same inputs would double every
    // one of those. the diff-oracle header names this case explicitly. The site is still
    // registered so the summary carries its `entered` count.

    void* const group = groupHandle.Get();
    const void* const nodesv = &nodesRef;
    ArrayHdr* layers = &_layers;              // self + 0x08

    if (animIndex < 0) {
        for (int32_t i = 0; i < _layers._size; ++i) {
            const int32_t handle =
                *reinterpret_cast<const int32_t*>(layers->_data + i * kLayerStride + 0x0c);
            if (handle < 0) continue;                       // TEST ECX,ECX / JS

            AnimationManagerView* mgr = AnimationManagerSingleton();
            uint8_t* inst = mgr->_instances._data +
                            static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride;
            if (--*reinterpret_cast<int32_t*>(inst + 0x0c) != 0) continue;

            *reinterpret_cast<uint64_t*>(inst + 0x00) = 0;   // _group
            *reinterpret_cast<int32_t*>(inst + 0x08)  = -1;  // _animIndex
            rtcoll::Rt_SetSize_18005aa30(inst + 0x10, 0);                  // CALL 0x18005aa30
            rtcoll::Rt_SetSize_18005b1e0(inst + 0x20, 0);                 // CALL 0x18005b1e0
        }
        rtcoll::Rt_SetSize_18005c8a0(layers, 0);                          // CALL 0x18005c8a0
        return;
    }

    const int32_t count = _layers._size;

    if (count > 0) {
        uint8_t* l0 = layers->_data;
        if (*reinterpret_cast<void* const*>(l0) == group) {
            const int32_t clip0 = *reinterpret_cast<const int32_t*>(l0 + 0x08);
            if (clip0 == animIndex) {
                const uint8_t* clips =
                    *reinterpret_cast<const uint8_t* const*>(static_cast<uint8_t*>(group) +
                                                             RtGraphics::anim_group::kClips);
                if (clips[static_cast<int64_t>(clip0) * kClipStride +
                          RtGraphics::anim_clip::kLooping] != 0) {
                    // speed = 1.0f, stored as the raw dword retail stores.
                    *reinterpret_cast<uint32_t*>(l0 + 0x10) = 0x3f800000u;
                    return;
                }
            }
        }
    }

    if (count == 0) {
        rtcoll::Rt_SetSize_18005c8a0(layers, 1);
    } else {
        uint8_t* l = layers->_data;
        const bool armLayer0 = (l[0x30] != 0) || (count == 1);

        if (!armLayer0) {
            // Arm A: re-arm the fade on layer 1. Note the STORE ORDER —
            // fadeRemaining (+0x28) before fadeLength (+0x20) — which is the
            // reverse of arm B below. Both are retail's.
            float fade;
            const uint8_t* g = static_cast<const uint8_t*>(group);
            const int32_t clip1 = *reinterpret_cast<const int32_t*>(l + kLayerStride + 0x08);
            if (*reinterpret_cast<void* const*>(l + kLayerStride) == group && clip1 >= 0) {
                const int64_t clipCount =
                    *reinterpret_cast<const int32_t*>(g + RtGraphics::anim_group::kClipCount);
                const float* table =
                    *reinterpret_cast<const float* const*>(g + RtGraphics::anim_group::kFadeTable);
                fade = table[clipCount * animIndex + clip1];
            } else {
                fade = *reinterpret_cast<const float*>(g + RtGraphics::anim_group::kDefaultFade);
            }
            const int64_t ticks = SecondsToTicks(static_cast<double>(fade));
            *reinterpret_cast<int64_t*>(l + kLayerStride + 0x28) = ticks;
            *reinterpret_cast<int64_t*>(layers->_data + kLayerStride + 0x20) = ticks;
        } else {
            // Arm B: arm the fade on layer 0, then insert a new layer 0.
            float fade;
            const uint8_t* g = static_cast<const uint8_t*>(group);
            const int32_t clip0 = *reinterpret_cast<const int32_t*>(l + 0x08);
            if (*reinterpret_cast<void* const*>(l) == group && clip0 >= 0) {
                const int64_t clipCount =
                    *reinterpret_cast<const int32_t*>(g + RtGraphics::anim_group::kClipCount);
                const float* table =
                    *reinterpret_cast<const float* const*>(g + RtGraphics::anim_group::kFadeTable);
                fade = table[clipCount * animIndex + clip0];
            } else {
                fade = *reinterpret_cast<const float*>(g + RtGraphics::anim_group::kDefaultFade);
            }
            const int64_t ticks = SecondsToTicks(static_cast<double>(fade));
            *reinterpret_cast<int64_t*>(l + 0x20) = ticks;
            *reinterpret_cast<int64_t*>(layers->_data + 0x28) = ticks;

            rtcoll::Rt_SetSize_18005c8a0(layers, _layers._size + 1);

            // Shift layers[k] -> layers[k+1] for k = oldCount-1 .. 0, as seven
            // qword moves per record, re-reading the buffer each iteration
            // exactly as retail does.
            int64_t n = _layers._size - 1;
            if (n >= 1) {
                int64_t off = n * kLayerStride;
                do {
                    uint8_t* base = layers->_data;
                    off -= kLayerStride;
                    for (int q = 0; q < 7; ++q) {
                        *reinterpret_cast<uint64_t*>(base + off + kLayerStride + q * 8) =
                            *reinterpret_cast<const uint64_t*>(base + off + q * 8);
                    }
                    --n;
                } while (n != 0);
            }
        }
    }

    // The common tail: write the new clip into layer 0.
    uint8_t* l0 = layers->_data;
    *reinterpret_cast<void**>(l0 + 0x00) = group;
    *reinterpret_cast<int32_t*>(l0 + 0x08) = animIndex;
    // Retail divides a literal 0.0 by _secondsPerTick TWICE rather than storing
    // a zero twice. Reproduced; it is two `DIVSD`/`CVTTSD2SI` pairs.
    *reinterpret_cast<int64_t*>(l0 + 0x20) = SecondsToTicks(0.0);
    *reinterpret_cast<int64_t*>(l0 + 0x28) = SecondsToTicks(0.0);
    *reinterpret_cast<uint32_t*>(l0 + 0x10) = 0x3f800000u;   // speed = 1.0f
    l0[0x30] = 0;                                            // restarted = false
    *reinterpret_cast<int64_t*>(l0 + 0x18) = startTime._ticks;

    const uint8_t* clips =
        *reinterpret_cast<const uint8_t* const*>(static_cast<uint8_t*>(group) +
                                                 RtGraphics::anim_group::kClips);
    if (clips[static_cast<int64_t>(animIndex) * kClipStride +
              RtGraphics::anim_clip::kInstanced] != 0 && instanced) {
        // CALL 0x1800fb9d0 — GetInstanceId. ENDGAME B.3.5 block GC item 6: this was
        // `RtAddr<GetInstanceIdFn>(0x1800fb9d0)`, an ADDRESS door onto the body in
        // this very file, kept that way "so the seam stays one door".
        // `Graphics::AnimationManager::GetInstanceId` is a MEMBER now, so this is a
        // QUALIFIED non-virtual member call — retail's own direct CALL — and the
        // registered door still serves every caller outside this file.
        *reinterpret_cast<int32_t*>(l0 + 0x0c) =
            reinterpret_cast<Graphics::AnimationManager*>(              // B.3.5 bridge
                AnimationManagerSingleton())
                ->Graphics::AnimationManager::GetInstanceId(
                    Object::ExternalPtr<Graphics::AnimationGroup>(
                        reinterpret_cast<Graphics::AnimationGroup*>(group)),
                    animIndex,
                    *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv));
    } else {
        *reinterpret_cast<int32_t*>(l0 + 0x0c) = -1;
    }
}

// ---------------------------------------------------------------------------
// ?CopyInstancedToUnique@AnimationState@Graphics@@  0x1800fd4c0 — block GC.
//
// *** `this` IS STILL NEVER READ. *** RCX is overwritten with the root transform
// four instructions in. The member is `const` and takes everything it needs as
// arguments; retail's receiver stays in the signature for the ABI and for the
// door, and the body below does not touch a single field of this class.
// The node-0-versus-node-n schedule difference is in the door's banner.
// ---------------------------------------------------------------------------
void AnimationState::CopyInstancedToUnique(const Collection::Array<uint8_t>& nodesRef,
                                           const Math::Transform& rootRef,
                                           void* interpv, int handle) const {
    const ArrayHdr* nodes = &nodesRef;
    const uint8_t* root = reinterpret_cast<const uint8_t*>(&rootRef);
    uint8_t* interp = static_cast<uint8_t*>(interpv);

    const uint8_t* instData = AnimationManagerSingleton()->_instances._data;
    const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(
        instData + static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride + 0x20);
    const uint8_t* pose = *reinterpret_cast<const uint8_t* const*>(
        states + static_cast<int64_t>(handle & 0xff) * kStateStride + 0x28);

    // ---- node 0, through the two real calls ----
    float rotated[3];
    const float* rot = QuaternionRotate(root, rotated, pose + 0x10);

    float ry = *reinterpret_cast<const float*>(root + 0x14);
    float rz = *reinterpret_cast<const float*>(root + 0x18);
    float rx = rot[0];
    ry = ry + rot[1];
    rz = rz + rot[2];
    rx = rx + *reinterpret_cast<const float*>(root + 0x10);

    float q0ret[4];
    const float* q0 = QuaternionMul(root, q0ret, pose);

    Copy7Dwords(interp + kTransformStride, interp);
    {
        float* dst = reinterpret_cast<float*>(interp);
        dst[0] = q0[0];
        dst[1] = q0[1];
        dst[2] = q0[2];
        dst[3] = q0[3];
        dst[4] = rx;
        dst[5] = ry;
        dst[6] = rz;
    }

    // ---- nodes 1..n-1, rotate inlined ----
    for (int32_t i = 1; i < nodes->_size; ++i) {
        const uint8_t* posei = pose + static_cast<int64_t>(i) * kTransformStride;
        const int64_t nodeOff = static_cast<int64_t>(i) * kNodeStride;

        const float vy = *reinterpret_cast<const float*>(posei + 0x14);
        const float vx = *reinterpret_cast<const float*>(posei + 0x10);
        const float vz = *reinterpret_cast<const float*>(posei + 0x18);

        const int32_t parent =
            *reinterpret_cast<const int32_t*>(nodes->_data + nodeOff + 0x08);
        const uint8_t* P = interp + static_cast<int64_t>(parent) * kInterpStride;

        const float px = *reinterpret_cast<const float*>(P + 0x00);
        const float pw = *reinterpret_cast<const float*>(P + 0x0c);
        const float pz = *reinterpret_cast<const float*>(P + 0x08);
        const float py = *reinterpret_cast<const float*>(P + 0x04);

        const float mpx = -px;
        const float mpz = -pz;
        const float mpy = -py;

        const float tx = *reinterpret_cast<const float*>(P + 0x10);
        const float ty = *reinterpret_cast<const float*>(P + 0x14);
        const float tz = *reinterpret_cast<const float*>(P + 0x18);

        float parentXform[7] = { px, py, pz, pw, tx, ty, tz };

        float A = pw * vx;
        float C = py * vx;
        float B = py * 0.0f;
        float k1 = py * vz;
        float k2 = py * vy;
        float k3 = px * 0.0f;
        A = A + k3;
        float k4 = pz * vy;
        A = A + k4;
        A = A - k1;
        float k5 = pz * vx;
        float k6 = pw * vy;
        B = B - k5;
        float k7 = px * vz;
        B = B + k6;
        float k8 = pw * vz;
        B = B + k7;
        float k9 = pz * 0.0f;
        float k10 = pz * vz;
        C = C + k9;
        float k11 = px * vy;
        float k12 = px * vx;
        C = C - k11;
        C = C + k8;

        float rxi = pw * A;
        float D = pw * 0.0f;
        float u1 = C * mpy;
        D = D - k12;
        D = D - k2;
        D = D - k10;
        float u0 = D * mpx;
        rxi = rxi + u0;
        rxi = rxi + u1;
        float ryi = pw * B;
        float u2 = B * mpz;
        float rzi = pw * C;
        float u6 = B * mpx;
        rxi = rxi - u2;
        rzi = rzi + u6;
        float u3 = C * mpx;
        float u4 = D * mpy;
        float u5 = D * mpz;
        float wx = tx;
        wx = wx + rxi;
        ryi = ryi - u3;
        float u7 = A * mpz;
        ryi = ryi + u4;
        float u8 = A * mpy;
        ryi = ryi + u7;
        rzi = rzi - u8;
        rzi = rzi + u5;
        float wy = ty;
        wy = wy + ryi;
        float wz = tz;
        wz = wz + rzi;

        float qret[4];
        const float* q = QuaternionMul(parentXform, qret, posei);

        uint8_t* slot = interp + static_cast<int64_t>(i) * kInterpStride;
        Copy7Dwords(slot + kTransformStride, slot);
        float* dst = reinterpret_cast<float*>(slot);
        dst[0] = q[0];
        dst[1] = q[1];
        dst[2] = q[2];
        dst[3] = q[3];
        dst[4] = wx;
        dst[5] = wy;
        dst[6] = wz;
    }
}

// ---------------------------------------------------------------------------
// ?UpdateNodes@AnimationState@Graphics@@  0x1800fcc20 — ENDGAME B.3.5 block GC.
// The two phases, the backward blend, the 48-entry scratch that retail also
// overruns, and the next-layer fade weight are documented beside the door.
//
// `_layers._data` and `_layers._size` are the POD's `layers` (+0x08) and
// `layerCount` (+0x10); both are RE-READ at every use here exactly as they were
// through `self`, because retail reloads them and the callees could grow the
// array.
// ---------------------------------------------------------------------------
void AnimationState::UpdateNodes(float speed,
                                 const Collection::Array<uint8_t>& nodesRef,
                                 const Math::Transform& rootRef,
                                 void* interpv, void* posev,
                                 Math::Vector3& rootOffsetOut) {
    const ArrayHdr* nodes = &nodesRef;
    const uint8_t* root = reinterpret_cast<const uint8_t*>(&rootRef);
    float* const rootOffset = reinterpret_cast<float*>(&rootOffsetOut);
    uint8_t* interp = static_cast<uint8_t*>(interpv);
    uint8_t* pose = static_cast<uint8_t*>(posev);

    const int64_t nodeCount = nodes->_size;

    // ---- phase 1: the last layer ----
    {
        const int32_t last = _layers._size - 1;
        uint8_t* layer = _layers._data +
                         static_cast<int64_t>(last) * kLayerStride;
        const uint8_t* group = *reinterpret_cast<const uint8_t* const*>(layer);
        const uint8_t* clip =
            *reinterpret_cast<const uint8_t* const*>(group + RtGraphics::anim_group::kClips) +
            static_cast<int64_t>(*reinterpret_cast<const int32_t*>(layer + 0x08)) * kClipStride;
        const int32_t handle = *reinterpret_cast<const int32_t*>(layer + 0x0c);
        if (handle >= 0) {
            // B.3.x bridge: the POD view is the manager; `pose` is Transform storage.
            reinterpret_cast<const Graphics::AnimationManager*>(AnimationManagerSingleton())
                ->GetPose(handle, reinterpret_cast<Math::Transform*>(pose),
                          static_cast<int>(nodeCount));
        } else {
            float scaled = speed;
            scaled = scaled * *reinterpret_cast<const float*>(layer + 0x10);
            const int64_t delta = SecondsToTicks(static_cast<double>(scaled));
            reinterpret_cast<const Graphics::Animation*>(clip)->GetPoseAtTime(   // B.3.x bridge
                *reinterpret_cast<const System::Time*>(layer + 0x18),
                *reinterpret_cast<const System::Time*>(&delta),
                reinterpret_cast<Math::Transform*>(pose), static_cast<int>(nodeCount));
        }
    }

    // ---- phase 2: every earlier layer, blended in backwards ----
    float scratch[48][7];
    for (int32_t i = _layers._size - 2; i >= 0; --i) {
        for (int s = 0; s < 48; ++s) {
            scratch[s][0] = 0.0f; scratch[s][1] = 0.0f; scratch[s][2] = 0.0f;
            scratch[s][3] = 1.0f;
            scratch[s][4] = 0.0f; scratch[s][5] = 0.0f; scratch[s][6] = 0.0f;
        }

        uint8_t* layers = _layers._data;
        uint8_t* layer = layers + static_cast<int64_t>(i) * kLayerStride;
        const uint8_t* group = *reinterpret_cast<const uint8_t* const*>(layer);
        const uint8_t* clip =
            *reinterpret_cast<const uint8_t* const*>(group + RtGraphics::anim_group::kClips) +
            static_cast<int64_t>(*reinterpret_cast<const int32_t*>(layer + 0x08)) * kClipStride;
        const int32_t handle = *reinterpret_cast<const int32_t*>(layer + 0x0c);

        if (handle >= 0) {
            // Inlined GetPose, straight out of the instance pool into the
            // scratch: retail does NOT call 0x1800fb8e0 on this arm.
            const uint8_t* instData = AnimationManagerSingleton()->_instances._data;
            const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(
                instData + static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride + 0x20);
            if (nodeCount > 0) {
                const uint8_t* src = *reinterpret_cast<const uint8_t* const*>(
                    states + static_cast<int64_t>(handle & 0xff) * kStateStride + 0x28);
                for (int64_t k = 0; k < nodeCount; ++k) {
                    const uint32_t* s = reinterpret_cast<const uint32_t*>(
                        src + k * kTransformStride);
                    uint32_t* d = reinterpret_cast<uint32_t*>(&scratch[k][0]);
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
                    d[4] = s[4]; d[5] = s[5]; d[6] = s[6];
                }
            }
        } else {
            float scaled = speed;
            scaled = scaled * *reinterpret_cast<const float*>(layer + 0x10);
            const int64_t delta = SecondsToTicks(static_cast<double>(scaled));
            reinterpret_cast<const Graphics::Animation*>(clip)->GetPoseAtTime(   // B.3.x bridge
                *reinterpret_cast<const System::Time*>(layer + 0x18),
                *reinterpret_cast<const System::Time*>(&delta),
                reinterpret_cast<Math::Transform*>(&scratch[0][0]),
                static_cast<int>(nodeCount));
        }

        // t = layer[i+1].fadeRemaining / layer[i+1].fadeLength, both converted
        // to seconds as doubles first and narrowed before the DIVSS.
        const double spt = SecondsPerTick();
        uint8_t* lbase = _layers._data;
        const double remSec =
            static_cast<double>(*reinterpret_cast<const int64_t*>(
                lbase + static_cast<int64_t>(i) * kLayerStride + 0x60)) * spt;
        const double lenSec =
            static_cast<double>(*reinterpret_cast<const int64_t*>(
                lbase + static_cast<int64_t>(i) * kLayerStride + 0x58)) * spt;
        float t = static_cast<float>(remSec);
        const float len = static_cast<float>(lenSec);
        t = t / len;

        if (nodeCount > 0) {
            for (int64_t k = 0; k < nodeCount; ++k) {
                uint8_t* dst = pose + k * kTransformStride;
                const float* from = &scratch[k][0];

                float bx = *reinterpret_cast<const float*>(dst + 0x10);
                float by = *reinterpret_cast<const float*>(dst + 0x14);
                float bz = *reinterpret_cast<const float*>(dst + 0x18);
                bx = bx - from[4];
                by = by - from[5];
                bz = bz - from[6];
                bx = bx * t;
                by = by * t;
                bz = bz * t;
                bx = bx + from[4];
                by = by + from[5];
                bz = bz + from[6];

                float qret[4];
                const float* q = QuaternionLerp(from, qret, dst, &t);
                reinterpret_cast<uint32_t*>(dst)[0] = reinterpret_cast<const uint32_t*>(q)[0];
                reinterpret_cast<uint32_t*>(dst)[1] = reinterpret_cast<const uint32_t*>(q)[1];
                reinterpret_cast<uint32_t*>(dst)[2] = reinterpret_cast<const uint32_t*>(q)[2];
                reinterpret_cast<uint32_t*>(dst)[3] = reinterpret_cast<const uint32_t*>(q)[3];
                *reinterpret_cast<float*>(dst + 0x10) = bx;
                *reinterpret_cast<float*>(dst + 0x14) = by;
                *reinterpret_cast<float*>(dst + 0x18) = bz;
            }
        }
    }

    // ---- phase 3: hand out the root offset, then build the hierarchy ----
    rootOffset[0] = *reinterpret_cast<const float*>(pose + 0x10);
    rootOffset[1] = *reinterpret_cast<const float*>(pose + 0x14);
    rootOffset[2] = *reinterpret_cast<const float*>(pose + 0x18);

    *reinterpret_cast<float*>(pose + 0x00) = 0.0f;
    *reinterpret_cast<float*>(pose + 0x04) = 0.0f;
    *reinterpret_cast<float*>(pose + 0x08) = 0.0f;
    *reinterpret_cast<float*>(pose + 0x0c) = 1.0f;
    *reinterpret_cast<float*>(pose + 0x10) = 0.0f;
    *reinterpret_cast<float*>(pose + 0x14) = 0.0f;
    *reinterpret_cast<float*>(pose + 0x18) = 0.0f;

    {
        // Node 0. Its translation was JUST zeroed, so MSVC folded the vector
        // into its zero register — every `* 0.0f` below is a real MULSS in the
        // instruction stream and is kept rather than folded away, because the
        // sign of a zero product still reaches the sums.
        const float qx = *reinterpret_cast<const float*>(root + 0x00);
        const float qy = *reinterpret_cast<const float*>(root + 0x04);
        const float qz = *reinterpret_cast<const float*>(root + 0x08);
        const float qw = *reinterpret_cast<const float*>(root + 0x0c);
        const float mqx = -qx;
        const float mqy = -qy;
        const float mqz = -qz;

        float ka = 0.0f * qy;
        float A = 0.0f * qw;
        float C = 0.0f * qy;
        float B = qy * 0.0f;
        float kb = qx * 0.0f;
        float D = qw * 0.0f;
        A = A + kb;
        float kc = 0.0f * qz;
        A = A + kc;
        A = A - ka;
        float kd = 0.0f * qz;
        float ke = 0.0f * qw;
        B = B - kd;
        float kf = 0.0f * qx;
        float rx = A;
        B = B + ke;
        rx = rx * qw;
        B = B + kf;
        float kg = 0.0f * qw;
        float kh = qz * 0.0f;
        C = C + kh;
        float ki = 0.0f * qx;
        C = C - ki;
        C = C + kg;
        float kj = 0.0f * qx;
        float kk = 0.0f * qy;
        D = D - kj;
        float kl = 0.0f * qz;
        D = D - kk;
        float u1 = C * mqy;
        D = D - kl;
        float u0 = D * mqx;
        rx = rx + u0;
        rx = rx + u1;
        float u2 = B * mqz;
        float ry = B;
        ry = ry * qw;
        float u3 = D * mqy;
        float u4 = D * mqz;
        rx = rx - u2;
        rx = rx + *reinterpret_cast<const float*>(root + 0x10);
        float u5 = C * mqx;
        float rz = C;
        rz = rz * qw;
        float u6 = B * mqx;
        ry = ry - u5;
        rz = rz + u6;
        ry = ry + u3;
        float u7 = A * mqz;
        float u8 = A * mqy;
        rz = rz - u8;
        ry = ry + u7;
        ry = ry + *reinterpret_cast<const float*>(root + 0x14);
        rz = rz + u4;
        rz = rz + *reinterpret_cast<const float*>(root + 0x18);

        float qret[4];
        const float* q = QuaternionMul(root, qret, pose);

        Copy7Dwords(interp + kTransformStride, interp);
        float* dst = reinterpret_cast<float*>(interp);
        dst[0] = q[0];
        dst[1] = q[1];
        dst[2] = q[2];
        dst[3] = q[3];
        dst[4] = rx;
        dst[5] = ry;
        dst[6] = rz;
    }

    for (int64_t k = 1; k < nodeCount; ++k) {
        const uint8_t* posek = pose + k * kTransformStride;
        const int64_t nodeOff = k * kNodeStride;

        const float vx = *reinterpret_cast<const float*>(posek + 0x10);
        const float vy = *reinterpret_cast<const float*>(posek + 0x14);
        const float vz = *reinterpret_cast<const float*>(posek + 0x18);

        const int32_t parent =
            *reinterpret_cast<const int32_t*>(nodes->_data + nodeOff + 0x08);
        const uint8_t* P = interp + static_cast<int64_t>(parent) * kInterpStride;

        const float py = *reinterpret_cast<const float*>(P + 0x04);
        const float pz = *reinterpret_cast<const float*>(P + 0x08);
        const float px = *reinterpret_cast<const float*>(P + 0x00);
        const float pw = *reinterpret_cast<const float*>(P + 0x0c);
        const float tx = *reinterpret_cast<const float*>(P + 0x10);
        const float ty = *reinterpret_cast<const float*>(P + 0x14);
        const float tz = *reinterpret_cast<const float*>(P + 0x18);

        const float mpy = -py;
        const float mpx = -px;
        const float mpz = -pz;

        float parentXform[7] = { px, py, pz, pw, tx, ty, tz };

        float k1 = vz * py;
        float B = py * 0.0f;
        float A = vx * pw;
        float k2 = px * 0.0f;
        float C = vx * py;
        A = A + k2;
        float k3 = vy * pz;
        A = A + k3;
        float k4 = vx * px;
        float k5 = vx * pz;
        A = A - k1;
        B = B - k5;
        float k6 = vy * pw;
        float k7 = vz * px;
        B = B + k6;
        B = B + k7;
        float k8 = vz * pw;
        float k9 = vz * pz;
        float k10 = pz * 0.0f;
        C = C + k10;
        float k11 = vy * py;
        float k12 = vy * px;
        float rx = A;
        rx = rx * pw;
        C = C - k12;
        C = C + k8;
        float D = pw * 0.0f;
        float u1 = C * mpy;
        D = D - k4;
        D = D - k11;
        D = D - k9;
        float u0 = D * mpx;
        rx = rx + u0;
        rx = rx + u1;
        float u2 = B * mpz;
        float ry = B;
        ry = ry * pw;
        float u3 = D * mpy;
        float u4 = D * mpz;
        rx = rx - u2;
        float u5 = C * mpx;
        rx = rx + tx;
        float rz = C;
        rz = rz * pw;
        float u6 = B * mpx;
        ry = ry - u5;
        rz = rz + u6;
        ry = ry + u3;
        float u7 = A * mpz;
        float u8 = A * mpy;
        rz = rz - u8;
        ry = ry + u7;
        ry = ry + ty;
        rz = rz + u4;
        rz = rz + tz;

        float qret[4];
        const float* q = QuaternionMul(parentXform, qret, posek);

        uint8_t* slot = interp + k * kInterpStride;
        Copy7Dwords(slot + kTransformStride, slot);
        float* dst = reinterpret_cast<float*>(slot);
        dst[0] = q[0];
        dst[1] = q[1];
        dst[2] = q[2];
        dst[3] = q[3];
        dst[4] = rx;
        dst[5] = ry;
        dst[6] = rz;
    }
}

// ---------------------------------------------------------------------------
// ?GetBones@MeshInstance@Graphics@@  0x180100070 -- ENDGAME B.3.5 block GC.
// The +0xd8 byte tested three separate times, the identity-versus-inverse root
// choice and MSVC's hoisted `q * 0.0f` seeds are all in the door's banner.
//
// The MeshInstance fields are still reached at their raw offsets through one
// bridge local rather than through Mesh.h's names: this body reads +0xc0, +0xd8,
// +0xe0, +0xe8 and +0xf0, and every one of them is an offset the surrounding
// helpers in this file also use. Renaming them is a separate mechanical pass.
// ---------------------------------------------------------------------------
int MeshInstance::GetBones(float t, const Math::Transform& rootRef,
                           Math::Matrix* out_) const {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);  // B.3.5 bridge
    const void* const rootv = &rootRef;
    void* const outv = out_;
    const uint8_t* mesh = *reinterpret_cast<const uint8_t* const*>(self + 0xc0);
    const int32_t boneCount = *reinterpret_cast<const int32_t*>(mesh + 0x68);
    uint8_t* out = static_cast<uint8_t*>(outv);

    // The body writes only the caller's matrix buffer and returns the bone count;
    // nothing it reads is modified. (The two dead oracle captures that used to
    // sit here went with the rest of this file's replay scaffolding — block GC.)

    float identity[7];
    float inverse[7];
    const float* rootT;
    if (self[0xd8] != 0) {
        identity[0] = 0.0f; identity[1] = 0.0f; identity[2] = 0.0f;
        identity[3] = 1.0f;
        identity[4] = 0.0f; identity[5] = 0.0f; identity[6] = 0.0f;
        rootT = identity;
    } else {
        rootT = TransformInverse(rootv, inverse);
    }

    const float rqx = rootT[0];
    const float rqy = rootT[1];
    const float rqz = rootT[2];
    const float rqw = rootT[3];
    const float rtx = rootT[4];
    const float rty = rootT[5];
    const float rtz = rootT[6];

    // The root transform, rebuilt as the `this` retail hands to operator*.
    const float rootXform[7] = { rqx, rqy, rqz, rqw, rtx, rty, rtz };

    const uint8_t* interp;
    if (self[0xd8] != 0) {
        const uint8_t* layers = *reinterpret_cast<const uint8_t* const*>(self + 0xe0);
        const int64_t lcount = *reinterpret_cast<const int32_t*>(self + 0xe8);
        const int32_t handle =
            *reinterpret_cast<const int32_t*>(layers + lcount * kLayerStride - 0x2c);
        const uint8_t* instData = AnimationManagerSingleton()->_instances._data;
        const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(
            instData + static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride + 0x20);
        interp = *reinterpret_cast<const uint8_t* const*>(
            states + static_cast<int64_t>(handle & 0xff) * kStateStride + 0x38);
    } else {
        interp = *reinterpret_cast<const uint8_t* const*>(self + 0xf0);
    }

    if (boneCount <= 0) return boneCount;

    const float rqx0 = rqx * 0.0f;
    const float rqy0 = rqy * 0.0f;
    const float rqz0 = rqz * 0.0f;
    const float rqw0 = rqw * 0.0f;
    const float mrqx = -rqx;
    const float mrqy = -rqy;
    const float mrqz = -rqz;

    for (int64_t k = 0; k < boneCount; ++k) {
        const uint8_t* ip = interp + k * kInterpStride;

        // Lerp the node's two stored transforms by `t`.
        float sx = *reinterpret_cast<const float*>(ip + 0x2c);
        float fx = *reinterpret_cast<const float*>(ip + 0x10);
        float sy = *reinterpret_cast<const float*>(ip + 0x30);
        float fy = *reinterpret_cast<const float*>(ip + 0x14);
        float sz = *reinterpret_cast<const float*>(ip + 0x34);
        float fz = *reinterpret_cast<const float*>(ip + 0x18);
        fx = fx - sx;
        fy = fy - sy;
        fz = fz - sz;
        fx = fx * t;
        fy = fy * t;
        fz = fz * t;
        sx = sx + fx;
        sy = sy + fy;
        sz = sz + fz;

        float lqret[4];
        const float* lq = QuaternionLerp(ip + kTransformStride, lqret, ip, &t);

        const uint8_t* bind =
            *reinterpret_cast<const uint8_t* const*>(mesh + 0x60) + k * kNodeStride + 0x28;

        // ---- stage 1: rotate the bind translation by the lerped rotation ----
        const float lqx = lq[0];
        const float lqw = lq[3];
        const float lqz = lq[2];
        const float lqy = lq[1];
        const float bvx = *reinterpret_cast<const float*>(bind + 0x10);
        const float bvy = *reinterpret_cast<const float*>(bind + 0x14);
        const float bvz = *reinterpret_cast<const float*>(bind + 0x18);
        const float mlqx = -lqx;
        const float mlqy = -lqy;
        const float mlqz = -lqz;

        const float ka = lqx * 0.0f;
        float B = lqy * 0.0f;
        float A = bvx * lqw;
        const float kb = bvz * lqy;
        float C = bvx * lqy;
        A = A + ka;
        const float kc = bvy * lqz;
        A = A + kc;
        const float kd = bvx * lqz;
        A = A - kb;
        const float ke = bvy * lqw;
        B = B - kd;
        const float kf = bvz * lqx;
        B = B + ke;
        const float kg = bvz * lqw;
        B = B + kf;
        const float kh = lqz * 0.0f;
        float D = lqw * 0.0f;
        C = C + kh;
        const float ki = bvy * lqx;
        C = C - ki;
        C = C + kg;
        const float kj = bvy * lqy;
        const float kk = bvz * lqz;
        float rx = A * lqw;
        const float u1 = C * mlqy;
        float ry = B * lqw;
        const float kl = bvx * lqx;
        D = D - kl;
        D = D - kj;
        D = D - kk;
        const float u0 = D * mlqx;
        rx = u0 + rx;
        const float u6 = B * mlqx;
        const float u2 = B * mlqz;
        rx = rx + u1;
        rx = rx - u2;
        const float u3 = D * mlqy;
        const float u4 = D * mlqz;
        const float u5 = C * mlqx;
        rx = rx + sx;
        float rz = C * lqw;
        ry = ry - u5;
        rz = u6 + rz;
        const float u7 = A * mlqz;
        const float u8 = A * mlqy;
        ry = ry + u3;
        rz = rz - u8;
        rz = rz + u4;
        ry = ry + u7;
        rz = rz + sz;
        ry = ry + sy;

        const float lerped[7] = { lqx, lqy, lqz, lqw, sx, sy, sz };
        float q1ret[4];
        const float* q1 = QuaternionMul(lerped, q1ret, bind);
        const float composedQ[4] = { q1[0], q1[1], q1[2], q1[3] };

        // ---- stage 2: the same shape again against the root rotation. The
        // four accumulators are SEEDED with the hoisted `rq * 0.0f` products,
        // which is why those are computed once outside the loop. ----
        float A2 = rx * rqw;
        A2 = A2 + rqx0;
        float C2 = rx * rqy;
        float B2 = rqy0;
        C2 = C2 + rqz0;
        const float m0 = ry * rqz;
        A2 = A2 + m0;
        const float m1 = rz * rqy;
        const float m2 = rx * rqz;
        A2 = A2 - m1;
        const float m3 = ry * rqw;
        B2 = B2 - m2;
        const float m4 = rz * rqx;
        B2 = B2 + m3;
        B2 = B2 + m4;
        const float m5 = ry * rqx;
        float D2 = rqw0;
        C2 = C2 - m5;
        const float m6 = rx * rqx;
        const float m7 = ry * rqy;
        D2 = D2 - m6;
        float Rx = A2 * rqw;
        const float m8 = rz * rqw;
        float Ry = B2 * rqw;
        C2 = C2 + m8;
        D2 = D2 - m7;
        const float m9 = rz * rqz;
        const float v1 = C2 * mrqy;
        D2 = D2 - m9;
        const float v0 = D2 * mrqx;
        Rx = v0 + Rx;
        const float v2 = B2 * mrqz;
        const float v6 = B2 * mrqx;
        Rx = Rx + v1;
        const float v3 = D2 * mrqy;
        Rx = Rx - v2;
        const float v4 = D2 * mrqz;
        const float v5 = C2 * mrqx;
        Rx = Rx + rtx;
        float Rz = C2 * rqw;
        Ry = Ry - v5;
        Rz = v6 + Rz;
        const float v7 = A2 * mrqz;
        const float v8 = A2 * mrqy;
        Ry = Ry + v3;
        Rz = Rz - v8;
        Rz = Rz + v4;
        Ry = Ry + v7;
        Ry = Ry + rty;
        Rz = Rz + rtz;

        float q2ret[4];
        const float* q2 = QuaternionMul(rootXform, q2ret, composedQ);

        const float finalXform[7] = { q2[0], q2[1], q2[2], q2[3], Rx, Ry, Rz };
        float matrix[16];
        const float* m = TransformAsMatrix(finalXform, matrix);

        uint8_t* dst = out + k * 0x40;
        for (int w = 0; w < 16; ++w)
            reinterpret_cast<uint32_t*>(dst)[w] = reinterpret_cast<const uint32_t*>(m)[w];
    }

    return boneCount;
}

// ---------------------------------------------------------------------------
// ?GetInstanceId@AnimationManager@Graphics@@  0x1800fb9d0 -- block GC.
// The two-jobs-at-once search pass, the blank-slot reuse and the 10000.0f-seeded
// smallest-playback-time sweep that picks the packed low byte are all documented
// in the banner beside the door.
//
// The manager is still reached through `RtGraphics::AnimationManagerView` -- the
// POD whose `_instances` is the same `Array<Instance>` at +0x00 that
// `AnimationManager.h` names, cross-asserted there -- because every helper this
// body shares with `PlayAnimation` takes that view, and B4b-1 keeps the POD until
// its last user converts. `group` is retail's `ExternalPtr<AnimationGroup>` BY
// VALUE: 8 bytes, trivially copyable, so it arrives in a register.
// ---------------------------------------------------------------------------
int32_t AnimationManager::GetInstanceId(Object::ExternalPtr<AnimationGroup> groupHandle,
                                        int animIndex,
                                        const Collection::Array<uint8_t>& nodesRef) {
    // NOT DIFFABLE: the body allocates instances, bakes poses into them and
    // moves reference counts. A second run against the DLL would create a
    // second instance and double every count.

    AnimationManagerView* const self =
        reinterpret_cast<AnimationManagerView*>(this);   // B.3.5 bridge
    void* const group = groupHandle.Get();
    const void* const nodesv = &nodesRef;
    const ArrayHdr* nodes = static_cast<const ArrayHdr*>(nodesv);

    int32_t freeSlot = -1;
    int32_t idx = 0;
    bool found = false;

    const int32_t count = self->_instances._size;
    if (count > 0) {
        uint8_t* p = self->_instances._data;
        for (;;) {
            void* g = *reinterpret_cast<void* const*>(p);
            if (g == nullptr && freeSlot < 0) freeSlot = idx;   // CMOVS EAX,ESI
            if (g == group && *reinterpret_cast<const int32_t*>(p + 0x08) == animIndex) {
                found = true;
                break;
            }
            ++idx;
            p += kInstanceStride;
            if (idx >= count) break;
        }
    }

    if (!found) {
        // A zeroed Instance on the stack, built the same way in both arms: the
        // group and clip index filled in, the reference count 0 and both Array
        // headers null. It is the FILL for the grow arm and the SOURCE of the
        // two empty arrays for the reuse arm.
        uint8_t tmp[kInstanceStride];
        *reinterpret_cast<void**>(tmp + 0x00)   = group;
        *reinterpret_cast<int32_t*>(tmp + 0x08) = animIndex;
        *reinterpret_cast<int32_t*>(tmp + 0x0c) = 0;
        *reinterpret_cast<uint64_t*>(tmp + 0x10) = 0;
        *reinterpret_cast<uint64_t*>(tmp + 0x18) = 0;
        *reinterpret_cast<uint64_t*>(tmp + 0x20) = 0;
        *reinterpret_cast<uint64_t*>(tmp + 0x28) = 0;

        if (count > 0 && freeSlot >= 0) {
            idx = freeSlot;
            uint8_t* inst = self->_instances._data +
                            static_cast<int64_t>(freeSlot) * kInstanceStride;
            *reinterpret_cast<void**>(inst + 0x00)   = group;
            *reinterpret_cast<int32_t*>(inst + 0x08) = animIndex;
            *reinterpret_cast<int32_t*>(inst + 0x0c) = 0;
            rtcoll::Rt_ArrayAssign_180013960(inst + 0x10, tmp + 0x10);            // CALL 0x180013960
            rtcoll::Rt_ArrayAssign_18005b130(inst + 0x20, tmp + 0x20);           // CALL 0x18005b130
        } else {
            idx = count;
            rtcoll::Rt_SetSize_18005be30(self, self->_instances._size + 1, tmp);   // CALL 0x18005be30
        }
        rtcoll::Rt_ArrayDtor_18005bbb0(tmp + 0x20);                              // CALL 0x18005bbb0
        rtcoll::Rt_ArrayDtor_180014090(tmp + 0x10);                               // CALL 0x180014090
    }

    if (!found) {
        uint8_t* inst = self->_instances._data + static_cast<int64_t>(idx) * kInstanceStride;
        uint8_t* instNodes  = inst + 0x10;
        uint8_t* instStates = inst + 0x20;

        rtcoll::Rt_ArrayAssign_180013960(instNodes, nodesv);                      // CALL 0x180013960

        const uint8_t* clip =
            *reinterpret_cast<const uint8_t* const*>(static_cast<uint8_t*>(group) +
                                                     RtGraphics::anim_group::kClips) +
            static_cast<int64_t>(animIndex) * kClipStride;

        // *** THE STATE COUNT IS clip+0x28, NOT clip+0x50. *** +0x50 is the
        // clip's keyframe count (AnimationManager.h); +0x28 is how many baked
        // samples an instanced clip carries, and it is the bound of every loop
        // below and of the final smallest-time sweep.
        rtcoll::Rt_SetSize_18005b1e0(instStates, *reinterpret_cast<const int32_t*>(clip + 0x28));

        const int32_t frames = *reinterpret_cast<const int32_t*>(clip + 0x28);

        // step = clipDurationSeconds * (1.0f / frames). Written as the three
        // SSE ops retail emits — a CVTPD2PS of the double product, a reciprocal
        // by DIVSS, then one MULSS — not as `dur / frames`.
        const double durSec =
            static_cast<double>(*reinterpret_cast<const int64_t*>(
                clip + RtGraphics::anim_clip::kDuration)) * SecondsPerTick();
        float dur = static_cast<float>(durSec);
        const float fFrames = static_cast<float>(frames);
        float inv = 1.0f;
        inv = inv / fFrames;
        float step = dur;
        step = step * inv;

        float t = 0.0f;
        int32_t frame = 0;

        if (frames > 0) {
            int64_t stateOff = 0;
            for (;;) {
                uint8_t* st = *reinterpret_cast<uint8_t* const*>(instStates) + stateOff;

                rtcoll::Rt_SetSize_18005ac50(st + 0x38, nodes->_size);      // CALL 0x18005ac50
                uint8_t* ip = *reinterpret_cast<uint8_t* const*>(st + 0x38);
                // Node 0 is the identity transform, written field by field, and
                // then copied over its own second half.
                reinterpret_cast<float*>(ip)[0] = 0.0f;
                reinterpret_cast<float*>(ip)[1] = 0.0f;
                reinterpret_cast<float*>(ip)[2] = 0.0f;
                reinterpret_cast<float*>(ip)[3] = 1.0f;
                reinterpret_cast<float*>(ip)[4] = 0.0f;
                reinterpret_cast<float*>(ip)[5] = 0.0f;
                reinterpret_cast<float*>(ip)[6] = 0.0f;
                Copy7Dwords(ip + kTransformStride, ip);

                rtcoll::RtColl_ArraySetSize_180059140(st + 0x28, nodes->_size);        // CALL 0x180059140

                if (*reinterpret_cast<const int32_t*>(st + 0x40) > 1) {
                    int64_t nodeOff   = kNodeStride;
                    int64_t interpOff = kInterpStride;
                    int32_t j = 1;
                    for (;;) {
                        const uint8_t* nd = nodes->_data;
                        uint8_t* ipArr = *reinterpret_cast<uint8_t* const*>(st + 0x38);

                        const int32_t parent =
                            *reinterpret_cast<const int32_t*>(nd + nodeOff + 0x08);
                        const uint8_t* P = ipArr + static_cast<int64_t>(parent) * kInterpStride;

                        const float px = *reinterpret_cast<const float*>(P + 0x00);
                        const float py = *reinterpret_cast<const float*>(P + 0x04);
                        const float pz = *reinterpret_cast<const float*>(P + 0x08);
                        const float pw = *reinterpret_cast<const float*>(P + 0x0c);
                        const float tx = *reinterpret_cast<const float*>(P + 0x10);
                        const float ty = *reinterpret_cast<const float*>(P + 0x14);
                        const float tz = *reinterpret_cast<const float*>(P + 0x18);

                        // The parent Transform, rebuilt on the stack because
                        // that is the `this` retail hands to operator*.
                        float parentXform[7] = { px, py, pz, pw, tx, ty, tz };

                        const float* local =
                            reinterpret_cast<const float*>(nd + nodeOff + 0x0c);
                        const float vx = local[4];
                        const float vy = local[5];
                        const float vz = local[6];

                        const float mpx = -px;   // XORPS with four -0.0f
                        const float mpy = -py;
                        const float mpz = -pz;

                        // q * v, with v.w == 0 supplied as a literal 0.0f
                        // wherever retail multiplies by its zero register.
                        float A = pw * vx;
                        float s0 = px * 0.0f;
                        A = A + s0;
                        float s1 = pz * vy;
                        A = A + s1;
                        float s2 = py * vz;
                        A = A - s2;

                        float B = py * 0.0f;
                        float s3 = pz * vx;
                        B = B - s3;
                        float s4 = pw * vy;
                        B = B + s4;
                        float s5 = px * vz;
                        B = B + s5;

                        float C = py * vx;
                        float s6 = pz * 0.0f;
                        C = C + s6;
                        float s7 = px * vy;
                        C = C - s7;
                        float s8 = pw * vz;
                        C = C + s8;

                        float D = pw * 0.0f;
                        float s9 = px * vx;
                        D = D - s9;
                        float s10 = py * vy;
                        D = D - s10;
                        float s11 = pz * vz;
                        D = D - s11;

                        // (A,B,C,D) * conj(q)
                        float rx = pw * A;
                        float u0 = D * mpx;
                        rx = rx + u0;
                        float u1 = C * mpy;
                        rx = rx + u1;
                        float u2 = B * mpz;
                        rx = rx - u2;

                        float ry = pw * B;
                        float u3 = C * mpx;
                        ry = ry - u3;
                        float u4 = D * mpy;
                        ry = ry + u4;
                        float u5 = A * mpz;
                        ry = ry + u5;

                        float rz = pw * C;
                        float u6 = B * mpx;
                        rz = rz + u6;
                        float u7 = A * mpy;
                        rz = rz - u7;
                        float u8 = D * mpz;
                        rz = rz + u8;

                        float wx = tx;
                        wx = wx + rx;
                        float wy = ty;
                        wy = wy + ry;
                        float wz = tz;
                        wz = wz + rz;

                        float qret[4];
                        const float* q = QuaternionMul(parentXform, qret, nd + nodeOff + 0x0c);

                        float* dst = reinterpret_cast<float*>(ipArr + interpOff);
                        dst[0] = q[0];
                        dst[1] = q[1];
                        dst[2] = q[2];
                        dst[3] = q[3];
                        dst[4] = wx;
                        dst[5] = wy;
                        dst[6] = wz;
                        Copy7Dwords(ipArr + interpOff + kTransformStride, ipArr + interpOff);

                        ++j;
                        nodeOff   += kNodeStride;
                        interpOff += kInterpStride;
                        if (j >= *reinterpret_cast<const int32_t*>(st + 0x40)) break;
                    }
                }

                // Seed the state: play the clip at `t` seconds with no fade,
                // then run two zero-length Updates so both halves of every
                // InterpolatedState hold the settled pose.
                const int64_t zero = SecondsToTicks(0.0);
                const int64_t at   = SecondsToTicks(static_cast<double>(t));
                // Graphics::AnimationState::PlayAnimation (0x1800fdf50). B.3.x
                // bridge: `st` is AnimationState storage; the ExternalPtr is the
                // bare pointer the door wrapped (a trivial wrapper, no refcount).
                reinterpret_cast<Graphics::AnimationState*>(st)->PlayAnimation(
                    Object::ExternalPtr<Graphics::AnimationGroup>(
                        reinterpret_cast<Graphics::AnimationGroup*>(group)),
                    animIndex,
                    *reinterpret_cast<const Collection::Array<uint8_t>*>(nodesv),
                    *reinterpret_cast<const System::Time*>(&at),
                    *reinterpret_cast<const System::Time*>(&zero),
                    1.0f, false);

                for (int rep = 0; rep < 2; ++rep) {
                    const int64_t z = SecondsToTicks(0.0);
                    const float ident[7] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
                    RtGraphics_AnimationState_Update(st, &z, instNodes, ident,
                                           *reinterpret_cast<void* const*>(st + 0x38),
                                           *reinterpret_cast<void* const*>(st + 0x28),
                                           reinterpret_cast<float*>(st + 0x18),
                                           false, false);
                }

                ++frame;
                stateOff += kStateStride;
                t = t + step;
                if (frame >= *reinterpret_cast<const int32_t*>(clip + 0x28)) break;
            }
        }
    }

    // ---- reference count and the smallest-playback-time sweep ----
    const int64_t instOff = static_cast<int64_t>(idx) * kInstanceStride;
    ++*reinterpret_cast<int32_t*>(self->_instances._data + instOff + 0x0c);

    float best = 10000.0f;          // DAT_180161734
    int32_t bestIdx = 0;
    int32_t j = 0;
    uint8_t* base = self->_instances._data;
    if (*reinterpret_cast<const int32_t*>(base + instOff + 0x28) > 0) {
        const int32_t n = *reinterpret_cast<const int32_t*>(base + instOff + 0x28);
        const uint8_t* sp = *reinterpret_cast<uint8_t* const*>(base + instOff + 0x20) + 0x08;
        const double spt = SecondsPerTick();
        do {
            float v;
            if (*reinterpret_cast<const int32_t*>(sp + 0x08) == 0) {
                v = 0.0f;
            } else {
                const uint8_t* ld = *reinterpret_cast<const uint8_t* const*>(sp);
                v = static_cast<float>(
                        static_cast<double>(*reinterpret_cast<const int64_t*>(ld + 0x18)) * spt);
            }
            // COMISS/JBE: an unordered compare skips, so a NaN never wins.
            if (best > v) { bestIdx = j; best = v; }
            ++j;
            sp += kStateStride;
        } while (j < n);
    }

    return (idx << 8) | bestIdx;
}

}  // namespace Graphics
