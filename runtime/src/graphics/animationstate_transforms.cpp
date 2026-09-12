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
#include <vector>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "../../types/graphics/AnimationManager.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using RtGraphics::AnimationManagerView;

namespace {

using ArrayHdr = Collection::Array<uint8_t>;

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

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
// The neighbours these bodies call. Every one goes through ResolveAddr or
// ResolveExport so that OUR port answers where one exists
// (reference_resolveaddr_consults_port_registry_first.md).
// ---------------------------------------------------------------------------
using SetSizeFn      = void  (*)(void* a, int n);
using SetSizeFillFn  = void  (*)(void* a, int n, const void* fill);
using ArrayAssignFn  = void* (*)(void* dst, const void* src);
using ArrayDtorFn    = void* (*)(void* a);

SetSizeFn SetSizeNodes()  { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005aa30); return f; }
SetSizeFn SetSizeStates() { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005b1e0); return f; }
SetSizeFn SetSizeLayers() { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005c8a0); return f; }
SetSizeFn SetSizeInterp() { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005ac50); return f; }
SetSizeFn SetSizePose()   { static SetSizeFn f = RtAddr<SetSizeFn>(0x180059140); return f; }

// Array<AnimationManager::Instance>::SetSize, the THREE-argument form whose new
// tail elements are copy-assigned from `fill` (array_graphics_anim.cpp).
SetSizeFillFn SetSizeInstances() {
    static SetSizeFillFn f = RtAddr<SetSizeFillFn>(0x18005be30);
    return f;
}

ArrayAssignFn AssignNodes()  { static ArrayAssignFn f = RtAddr<ArrayAssignFn>(0x180013960); return f; }
ArrayAssignFn AssignStates() { static ArrayAssignFn f = RtAddr<ArrayAssignFn>(0x18005b130); return f; }

ArrayDtorFn DtorNodes()  { static ArrayDtorFn f = RtAddr<ArrayDtorFn>(0x180014090); return f; }
ArrayDtorFn DtorStates() { static ArrayDtorFn f = RtAddr<ArrayDtorFn>(0x18005bbb0); return f; }

// Math. All five carry a hidden return slot; see the banner.
using QuatMulFn    = const float* (*)(const void* self, void* ret, const void* rhs);
using QuatRotateFn = const float* (*)(const void* self, void* ret, const void* v);
using QuatLerpFn   = const float* (*)(const void* from, void* ret, const void* to, const float* t);
using XformFn      = const float* (*)(const void* self, void* ret);

QuatMulFn QuaternionMul() {
    return RT_EXPORT_ONCE(QuatMulFn, "??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z");
}
QuatRotateFn QuaternionRotate() {
    return RT_EXPORT_ONCE(QuatRotateFn, "?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z");
}
QuatLerpFn QuaternionLerp() {
    return RT_EXPORT_ONCE(QuatLerpFn, "?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z");
}
XformFn TransformInverse() {
    return RT_EXPORT_ONCE(XformFn, "?Inverse@Transform@Math@@QEBA?AV12@XZ");
}
XformFn TransformAsMatrix() {
    return RT_EXPORT_ONCE(XformFn, "?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ");
}

// The three cluster bodies these five call. By ADDRESS, because that is where
// animationstate.cpp registers them and because retail's call is a direct one.
using GetPoseFn       = void (*)(const void* mgr, int handle, void* out, int count);
using GetPoseAtTimeFn = void (*)(const void* clip, const int64_t* time,
                                 const int64_t* delta, void* out, int count);
using UpdateFn        = bool (*)(void* self, const int64_t* dt, const void* nodes,
                                 const void* root, void* interp, void* pose,
                                 float* rootOffset, bool a, bool b);

GetPoseFn GetPose()             { static GetPoseFn f = RtAddr<GetPoseFn>(0x1800fb8e0); return f; }
GetPoseAtTimeFn GetPoseAtTime() { static GetPoseAtTimeFn f = RtAddr<GetPoseAtTimeFn>(0x1800faca0); return f; }
// ?Update@AnimationState@Graphics@@ — retail 0x1800fdb20, NOT ported (it is the
// body runtime/types/graphics/AnimationState.h was read out of). Reached by
// address so the day it IS ported this call site follows without an edit.
UpdateFn AnimationStateUpdate() { static UpdateFn f = RtAddr<UpdateFn>(0x1800fdb20); return f; }

// ?_singleton@AnimationManager@Graphics@@0PEAV12@EA — the slot at 0x1801fe7b0.
// ONE deref gives the manager; its +0x00 is the instance array header.
AnimationManagerView* AnimationManagerSingleton() {
    AnimationManagerView** slot =
        RT_DATA_SLOT(AnimationManagerView**, "?_singleton@AnimationManager@Graphics@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ?_secondsPerTick@Timer@System@@0NA — a DOUBLE at 0x1801fe728. Seconds->ticks
// is a DIVIDE by it here (`DIVSD`), where GetPoseAtTime next door multiplies;
// both spellings are retail's, in the direction each needs.
double SecondsPerTick() {
    double* p = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
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
extern "C" void RtGraphics_AnimationState_PlayAnimation(
    void* selfv, void* group, int animIndex, const void* nodesv,
    const int64_t* startTime, const int64_t* /*fadeTime, unread*/,
    float /*speed, unread*/, bool instanced) {
    RT_DIFF_BEGIN("Graphics::AnimationState::PlayAnimation");
    // NOT DIFFABLE against the DLL. The body mutates the layer array AND the
    // AnimationManager's shared instance pool (reference counts, SetSize);
    // running the original a second time on the same inputs would double every
    // one of those. rt_diff.h names this case explicitly. The site is still
    // registered so the summary carries its `entered` count.

    uint8_t* self = static_cast<uint8_t*>(selfv);
    ArrayHdr* layers = reinterpret_cast<ArrayHdr*>(self + 0x08);

    if (animIndex < 0) {
        for (int32_t i = 0; i < *reinterpret_cast<const int32_t*>(self + 0x10); ++i) {
            const int32_t handle =
                *reinterpret_cast<const int32_t*>(layers->_data + i * kLayerStride + 0x0c);
            if (handle < 0) continue;                       // TEST ECX,ECX / JS

            AnimationManagerView* mgr = AnimationManagerSingleton();
            uint8_t* inst = mgr->_instances._data +
                            static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride;
            if (--*reinterpret_cast<int32_t*>(inst + 0x0c) != 0) continue;

            *reinterpret_cast<uint64_t*>(inst + 0x00) = 0;   // _group
            *reinterpret_cast<int32_t*>(inst + 0x08)  = -1;  // _animIndex
            SetSizeNodes()(inst + 0x10, 0);                  // CALL 0x18005aa30
            SetSizeStates()(inst + 0x20, 0);                 // CALL 0x18005b1e0
        }
        SetSizeLayers()(layers, 0);                          // CALL 0x18005c8a0
        return;
    }

    const int32_t count = *reinterpret_cast<const int32_t*>(self + 0x10);

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
        SetSizeLayers()(layers, 1);
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

            SetSizeLayers()(layers, *reinterpret_cast<const int32_t*>(self + 0x10) + 1);

            // Shift layers[k] -> layers[k+1] for k = oldCount-1 .. 0, as seven
            // qword moves per record, re-reading the buffer each iteration
            // exactly as retail does.
            int64_t n = *reinterpret_cast<const int32_t*>(self + 0x10) - 1;
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
    *reinterpret_cast<int64_t*>(l0 + 0x18) = *startTime;

    const uint8_t* clips =
        *reinterpret_cast<const uint8_t* const*>(static_cast<uint8_t*>(group) +
                                                 RtGraphics::anim_group::kClips);
    if (clips[static_cast<int64_t>(animIndex) * kClipStride +
              RtGraphics::anim_clip::kInstanced] != 0 && instanced) {
        // CALL 0x1800fb9d0 — GetInstanceId, the body below. Through the address
        // registry rather than as a direct C call so the seam stays one door.
        using GetInstanceIdFn = int (*)(void*, void*, int, const void*);
        *reinterpret_cast<int32_t*>(l0 + 0x0c) =
            RtAddr<GetInstanceIdFn>(0x1800fb9d0)(
                AnimationManagerSingleton(), group, animIndex, nodesv);
    } else {
        *reinterpret_cast<int32_t*>(l0 + 0x0c) = -1;
    }
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
extern "C" int RtGraphics_AnimationManager_GetInstanceId(
    void* selfv, void* group, int animIndex, const void* nodesv) {
    RT_DIFF_BEGIN("Graphics::AnimationManager::GetInstanceId");
    // NOT DIFFABLE: the body allocates instances, bakes poses into them and
    // moves reference counts. A second run against the DLL would create a
    // second instance and double every count.

    AnimationManagerView* self = static_cast<AnimationManagerView*>(selfv);
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
            AssignNodes()(inst + 0x10, tmp + 0x10);            // CALL 0x180013960
            AssignStates()(inst + 0x20, tmp + 0x20);           // CALL 0x18005b130
        } else {
            idx = count;
            SetSizeInstances()(self, self->_instances._size + 1, tmp);   // CALL 0x18005be30
        }
        DtorStates()(tmp + 0x20);                              // CALL 0x18005bbb0
        DtorNodes()(tmp + 0x10);                               // CALL 0x180014090
    }

    if (!found) {
        uint8_t* inst = self->_instances._data + static_cast<int64_t>(idx) * kInstanceStride;
        uint8_t* instNodes  = inst + 0x10;
        uint8_t* instStates = inst + 0x20;

        AssignNodes()(instNodes, nodesv);                      // CALL 0x180013960

        const uint8_t* clip =
            *reinterpret_cast<const uint8_t* const*>(static_cast<uint8_t*>(group) +
                                                     RtGraphics::anim_group::kClips) +
            static_cast<int64_t>(animIndex) * kClipStride;

        // *** THE STATE COUNT IS clip+0x28, NOT clip+0x50. *** +0x50 is the
        // clip's keyframe count (AnimationManager.h); +0x28 is how many baked
        // samples an instanced clip carries, and it is the bound of every loop
        // below and of the final smallest-time sweep.
        SetSizeStates()(instStates, *reinterpret_cast<const int32_t*>(clip + 0x28));

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

                SetSizeInterp()(st + 0x38, nodes->_size);      // CALL 0x18005ac50
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

                SetSizePose()(st + 0x28, nodes->_size);        // CALL 0x180059140

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
                        const float* q = QuaternionMul()(parentXform, qret, nd + nodeOff + 0x0c);

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
                using PlayFn = void (*)(void*, void*, int, const void*,
                                        const int64_t*, const int64_t*, float, bool);
                RtAddr<PlayFn>(0x1800fdf50)(st, group, animIndex, nodesv,
                                            &at, &zero, 1.0f, false);

                for (int rep = 0; rep < 2; ++rep) {
                    const int64_t z = SecondsToTicks(0.0);
                    const float ident[7] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
                    AnimationStateUpdate()(st, &z, instNodes, ident,
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
extern "C" void RtGraphics_AnimationState_CopyInstancedToUnique(
    const void* /*self, unread*/, const void* nodesv, const void* rootv,
    void* interpv, int handle) {
    RT_DIFF_BEGIN("Graphics::AnimationState::CopyInstancedToUnique");

    const ArrayHdr* nodes = static_cast<const ArrayHdr*>(nodesv);
    const uint8_t* root = static_cast<const uint8_t*>(rootv);
    uint8_t* interp = static_cast<uint8_t*>(interpv);

    // Full oracle: the only thing this body writes is the caller's interp
    // array, so snapshot it, run ours live, then run the DLL's body on the copy
    // and compare. Nothing else in the process is touched.
    std::vector<uint8_t> replay;
    const size_t replayBytes =
        static_cast<size_t>(nodes->_size > 0 ? nodes->_size : 0) * kInterpStride;
    if (rt_diff_on__ && replayBytes != 0) {
        replay.assign(interp, interp + replayBytes);
    }

    const uint8_t* instData = AnimationManagerSingleton()->_instances._data;
    const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(
        instData + static_cast<int64_t>((handle >> 8) & 0xff) * kInstanceStride + 0x20);
    const uint8_t* pose = *reinterpret_cast<const uint8_t* const*>(
        states + static_cast<int64_t>(handle & 0xff) * kStateStride + 0x28);

    // ---- node 0, through the two real calls ----
    float rotated[3];
    const float* rot = QuaternionRotate()(root, rotated, pose + 0x10);

    float ry = *reinterpret_cast<const float*>(root + 0x14);
    float rz = *reinterpret_cast<const float*>(root + 0x18);
    float rx = rot[0];
    ry = ry + rot[1];
    rz = rz + rot[2];
    rx = rx + *reinterpret_cast<const float*>(root + 0x10);

    float q0ret[4];
    const float* q0 = QuaternionMul()(root, q0ret, pose);

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
        const float* q = QuaternionMul()(parentXform, qret, posei);

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

    if (rt_diff_on__ && replayBytes != 0) {
        using OrigFn = void (*)(const void*, const void*, const void*, void*, int);
        RT_ORIGINAL_ONCE(OrigFn,
            "?CopyInstancedToUnique@AnimationState@Graphics@@AEBAXAEBV?$Array@UNode@"
            "GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV"
            "?$InterpolatedState@VTransform@Math@@@2@H@Z")(
                nullptr, nodesv, rootv, replay.data(), handle);
        RT_DIFF_BYTES(interp, replay.data(), replayBytes);
    }
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
extern "C" void RtGraphics_AnimationState_UpdateNodes(
    void* selfv, float speed, const void* nodesv, const void* rootv,
    void* interpv, void* posev, float* rootOffset) {
    RT_DIFF_BEGIN("Graphics::AnimationState::UpdateNodes");

    uint8_t* self = static_cast<uint8_t*>(selfv);
    const ArrayHdr* nodes = static_cast<const ArrayHdr*>(nodesv);
    const uint8_t* root = static_cast<const uint8_t*>(rootv);
    uint8_t* interp = static_cast<uint8_t*>(interpv);
    uint8_t* pose = static_cast<uint8_t*>(posev);

    const int64_t nodeCount = nodes->_size;

    // Full oracle: the three out-parameters are the whole of this body's
    // effect. `this` is only read (no store to it exists in the disassembly)
    // and neither GetPose nor GetPoseAtTime mutates the manager.
    std::vector<uint8_t> replayPose, replayInterp;
    float replayOffset[3] = { 0.0f, 0.0f, 0.0f };
    const size_t poseBytes   = static_cast<size_t>(nodeCount > 0 ? nodeCount : 0) * kTransformStride;
    const size_t interpBytes = static_cast<size_t>(nodeCount > 0 ? nodeCount : 0) * kInterpStride;
    if (rt_diff_on__ && poseBytes != 0) {
        replayPose.assign(pose, pose + poseBytes);
        replayInterp.assign(interp, interp + interpBytes);
        std::memcpy(replayOffset, rootOffset, sizeof(replayOffset));
    }

    // ---- phase 1: the last layer ----
    {
        const int32_t last = *reinterpret_cast<const int32_t*>(self + 0x10) - 1;
        uint8_t* layer = *reinterpret_cast<uint8_t* const*>(self + 0x08) +
                         static_cast<int64_t>(last) * kLayerStride;
        const uint8_t* group = *reinterpret_cast<const uint8_t* const*>(layer);
        const uint8_t* clip =
            *reinterpret_cast<const uint8_t* const*>(group + RtGraphics::anim_group::kClips) +
            static_cast<int64_t>(*reinterpret_cast<const int32_t*>(layer + 0x08)) * kClipStride;
        const int32_t handle = *reinterpret_cast<const int32_t*>(layer + 0x0c);
        if (handle >= 0) {
            GetPose()(AnimationManagerSingleton(), handle, pose,
                      static_cast<int>(nodeCount));
        } else {
            float scaled = speed;
            scaled = scaled * *reinterpret_cast<const float*>(layer + 0x10);
            const int64_t delta = SecondsToTicks(static_cast<double>(scaled));
            GetPoseAtTime()(clip, reinterpret_cast<const int64_t*>(layer + 0x18), &delta,
                            pose, static_cast<int>(nodeCount));
        }
    }

    // ---- phase 2: every earlier layer, blended in backwards ----
    float scratch[48][7];
    for (int32_t i = *reinterpret_cast<const int32_t*>(self + 0x10) - 2; i >= 0; --i) {
        for (int s = 0; s < 48; ++s) {
            scratch[s][0] = 0.0f; scratch[s][1] = 0.0f; scratch[s][2] = 0.0f;
            scratch[s][3] = 1.0f;
            scratch[s][4] = 0.0f; scratch[s][5] = 0.0f; scratch[s][6] = 0.0f;
        }

        uint8_t* layers = *reinterpret_cast<uint8_t* const*>(self + 0x08);
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
            GetPoseAtTime()(clip, reinterpret_cast<const int64_t*>(layer + 0x18), &delta,
                            &scratch[0][0], static_cast<int>(nodeCount));
        }

        // t = layer[i+1].fadeRemaining / layer[i+1].fadeLength, both converted
        // to seconds as doubles first and narrowed before the DIVSS.
        const double spt = SecondsPerTick();
        uint8_t* lbase = *reinterpret_cast<uint8_t* const*>(self + 0x08);
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
                const float* q = QuaternionLerp()(from, qret, dst, &t);
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
        const float* q = QuaternionMul()(root, qret, pose);

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
        const float* q = QuaternionMul()(parentXform, qret, posek);

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

    if (rt_diff_on__ && poseBytes != 0) {
        using OrigFn = void (*)(void*, float, const void*, const void*, void*, void*, float*);
        RT_ORIGINAL_ONCE(OrigFn,
            "?UpdateNodes@AnimationState@Graphics@@AEAAXMAEBV?$Array@UNode@GraphicsMesh@"
            "Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@"
            "VTransform@Math@@@2@PEAV56@AEAVVector3@6@@Z")(
                selfv, speed, nodesv, rootv, replayInterp.data(), replayPose.data(),
                replayOffset);
        RT_DIFF_BYTES(pose, replayPose.data(), poseBytes);
        RT_DIFF_BYTES(interp, replayInterp.data(), interpBytes);
        RT_DIFF_BYTES(rootOffset, replayOffset, sizeof(replayOffset));
    }
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
extern "C" int RtGraphics_MeshInstance_GetBones(
    const void* selfv, float t, const void* rootv, void* outv) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::GetBones");

    const uint8_t* self = static_cast<const uint8_t*>(selfv);
    const uint8_t* mesh = *reinterpret_cast<const uint8_t* const*>(self + 0xc0);
    const int32_t boneCount = *reinterpret_cast<const int32_t*>(mesh + 0x68);
    uint8_t* out = static_cast<uint8_t*>(outv);

    // Full oracle: the body writes only the caller's matrix buffer and returns
    // the bone count. Nothing it reads is modified.
    std::vector<uint8_t> replay;
    const size_t replayBytes = static_cast<size_t>(boneCount > 0 ? boneCount : 0) * 0x40;
    if (rt_diff_on__ && replayBytes != 0) replay.assign(replayBytes, 0);

    float identity[7];
    float inverse[7];
    const float* rootT;
    if (self[0xd8] != 0) {
        identity[0] = 0.0f; identity[1] = 0.0f; identity[2] = 0.0f;
        identity[3] = 1.0f;
        identity[4] = 0.0f; identity[5] = 0.0f; identity[6] = 0.0f;
        rootT = identity;
    } else {
        rootT = TransformInverse()(rootv, inverse);
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
        const float* lq = QuaternionLerp()(ip + kTransformStride, lqret, ip, &t);

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
        const float* q1 = QuaternionMul()(lerped, q1ret, bind);
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
        const float* q2 = QuaternionMul()(rootXform, q2ret, composedQ);

        const float finalXform[7] = { q2[0], q2[1], q2[2], q2[3], Rx, Ry, Rz };
        float matrix[16];
        const float* m = TransformAsMatrix()(finalXform, matrix);

        uint8_t* dst = out + k * 0x40;
        for (int w = 0; w < 16; ++w)
            reinterpret_cast<uint32_t*>(dst)[w] = reinterpret_cast<const uint32_t*>(m)[w];
    }

    if (rt_diff_on__ && replayBytes != 0) {
        using OrigFn = int (*)(const void*, float, const void*, void*);
        const int theirs = RT_ORIGINAL_ONCE(OrigFn,
            "?GetBones@MeshInstance@Graphics@@QEBAHMAEBVTransform@Math@@PEAVMatrix@4@@Z")(
                selfv, t, rootv, replay.data());
        RT_DIFF_SCALAR(boneCount, theirs);
        RT_DIFF_BYTES(out, replay.data(), replayBytes);
    }

    return boneCount;
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

    return n;
}
