// ============================================================================
// runtime/src/collection/array_graphics_anim.cpp
// The seventeen Collection::Array<T> instantiations underneath the Graphics
// animation / skeleton cluster (cutover closure batch 3, 2026-09-12).
// ============================================================================
//
// WHY A NEW FILE AND NOT MORE ROWS IN array.cpp / array_destroy.cpp
// -----------------------------------------------------------------
// It is the same two algorithms those files already own, and this file calls
// THEIR bodies — `rtcoll::SetSizeCore`, `rtcoll::ArrayDtorImpl`,
// `rtcoll::FreeInnerArraysCore`, `rtcoll::ReleaseStringsCore` — through
// array_internal.h / array_destroy_internal.h rather than restating any of
// them. Nothing here is a second copy of a Collection body; what is new is
// seventeen (address, spec) pairs and one job the other two files do not have
// an entry shape for, `Array<T>::operator=` for a NON-pointer element type.
//
// array_destroy.cpp's banner asks, in as many words, that an address be
// grepped across both files before a Collection helper is ported here. All
// seventeen were; none is registered in either. Three of them are ALREADY
// SPEC'D there and unregistered ("the eight ported but deliberately
// unregistered ... register it the day a caller appears"), and this batch is
// that day for two of them:
//     0x18005bce0  kD_InnerArr3_48   the AnimationState element destroy
//     0x180014a40  kD_Str_48         the GraphicsMesh::Node element destroy
// so they are reached here through the SPEC ROWS those files export, not
// through a re-derivation.
//
// ---------------------------------------------------------------------------
// THE FOUR RECORD TYPES, AND WHERE EACH LAYOUT COMES FROM
// ---------------------------------------------------------------------------
// runtime/types/graphics/AnimationManager.h, pinned by the six serializers in
// runtime/src/graphics/animationstate.cpp. In one line each:
//
//   AnimationManager::Instance   0x30  {ExternalPtr, int, int, Array, Array}
//   AnimationState               0x48  {bool, Array, Vector3, Array, Array}
//   AnimationState::Layer        0x38  {ExternalPtr, int, int, float, 3x Time, bool}
//   GraphicsMesh::Node           0x48  {char*, int, Transform, Transform, int}
//   InterpolatedState<Transform> 0x38  two 0x1c Math::Transform
//   Math::Transform              0x1c  (already array.cpp's kSpec_Transform)
//
// ---------------------------------------------------------------------------
// THE ONE THING IN HERE THAT LOOKS LIKE A TRANSCRIPTION ERROR AND IS NOT
// ---------------------------------------------------------------------------
// `kInitMeshNode` does NOT write +0x08 (the parent index) or +0x44, and
// `kInitAnimLayer` does not write +0x10 (the playback speed) or +0x31..+0x37.
// Those are retail's holes: the `else` arm of each fill loop stores exactly the
// qwords listed beside it below and nothing more. Helpfully zeroing them would
// be unfaithful AND would make an oracle that compared them fail 100% of calls
// (reference_oracle_must_not_compare_unwritten_bytes.md). Every caller in this
// cluster overwrites both fields before reading them — AnimationState::
// PlayAnimation writes layer speed on the line after it grows the array, and
// GetInstanceId copies the node array in over the top — which is presumably why
// MSVC was allowed to elide the stores.
//
// ALLOCATOR BOUNDARY: none of its own. `MemAlloc` / `MemFree` are array.cpp's
// two accessors, and the lane-E dependency its banner states applies here
// unchanged.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "array_internal.h"
#include "array_destroy_internal.h"
#include "../../types/graphics/AnimationState.h"   // B3R RT-VG: the Rt_* declarations

// The three element copy constructors live with the class they belong to, in
// runtime/src/graphics/animationstate.cpp. They are reached by name and not by
// address so that this file has no dependency on registration ORDER.
extern "C" void* RtGraphics_MeshNode_CopyCtor(void* dst, const void* src);
extern "C" void* RtGraphics_AnimState_CopyCtor(void* dst, const void* src);
extern "C" void* RtGraphics_AnimInstance_CopyCtor(void* dst, const void* src);

namespace rtcoll {
namespace gfxanim {

// ---------------------------------------------------------------------------
// Element assignment
// ---------------------------------------------------------------------------
// The two byte-copy forms are written as the exact qword / dword sequences
// retail emits rather than as a memcpy, because the COUNT is the evidence for
// the stride and a memcpy would erase it from the source.

// 0x18001b700's inner loop and 0x18005c8a0's grow arm: seven qwords.
void Assign38Q(uint8_t* d, const uint8_t* s) {
    uint64_t* dq = reinterpret_cast<uint64_t*>(d);
    const uint64_t* sq = reinterpret_cast<const uint64_t*>(s);
    for (int i = 0; i < 7; ++i) dq[i] = sq[i];
}

// 0x18001b810's inner loop and 0x18005ac50's grow arm: fourteen dwords. The
// SAME 0x38 stride as Assign38Q above and a DIFFERENT instantiation — the
// element is two 0x1c Math::Transforms, whose alignment is 4, and retail
// asks the allocator for 4 here and for 8 there. Merging the two by stride
// would change the alignment argument of every allocation this array makes.
void Assign38D(uint8_t* d, const uint8_t* s) {
    uint32_t* dd = reinterpret_cast<uint32_t*>(d);
    const uint32_t* sd = reinterpret_cast<const uint32_t*>(s);
    for (int i = 0; i < 14; ++i) dd[i] = sd[i];
}

void AssignMeshNode(uint8_t* d, const uint8_t* s)     { RtGraphics_MeshNode_CopyCtor(d, s); }
void AssignAnimState(uint8_t* d, const uint8_t* s)    { RtGraphics_AnimState_CopyCtor(d, s); }
void AssignAnimInstance(uint8_t* d, const uint8_t* s) { RtGraphics_AnimInstance_CopyCtor(d, s); }

// ---------------------------------------------------------------------------
// New-tail initialisation — one function per grow arm, each transcribing the
// stores of the `else` limb of that address's fill loop, in retail's order.
// ---------------------------------------------------------------------------

// 0x18005aa30. Two identity Math::Transforms; +0x08 and +0x44 NOT written.
void InitMeshNode(uint8_t* p) {
    *reinterpret_cast<uint64_t*>(p + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x0c) = 0;
    *reinterpret_cast<uint32_t*>(p + 0x14) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0x3f800000ull;   // +0x18 = 1.0f, +0x1c = 0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    *reinterpret_cast<uint32_t*>(p + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x34) = 0x3f800000ull;   // +0x34 = 1.0f, +0x38 = 0
    *reinterpret_cast<uint64_t*>(p + 0x3c) = 0;
}

// 0x18005b1e0. A zeroed AnimationState: the bool, then the three Array headers
// and the Vector3 between them, all zero. +0x01..+0x07 not written.
void InitAnimState(uint8_t* p) {
    p[0] = 0;
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *reinterpret_cast<uint32_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x38) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x40) = 0;
}

// 0x18005c8a0. The -1 is ONE qword covering BOTH ints at +0x08 and +0x0c, i.e.
// clipIndex = -1 and handle = -1. +0x10 (speed) and +0x31..+0x37 not written.
void InitAnimLayer(uint8_t* p) {
    *reinterpret_cast<uint64_t*>(p + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0xffffffffffffffffull;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    p[0x30] = 0;
}

// 0x18005c3d0 / 0x18005be30's no-fill arm. Covers the whole 0x30.
void InitAnimInstance(uint8_t* p) {
    *reinterpret_cast<uint64_t*>(p + 0x00) = 0;
    *reinterpret_cast<int32_t*>(p + 0x08)  = -1;
    *reinterpret_cast<int32_t*>(p + 0x0c)  = 0;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
}

// 0x18005ac50. Two identity Math::Transforms back to back; covers the whole
// 0x38.
void InitInterpTransform(uint8_t* p) {
    *reinterpret_cast<uint64_t*>(p + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(p + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x0c) = 0x3f800000ull;   // +0x0c = 1.0f, +0x10 = 0
    *reinterpret_cast<uint64_t*>(p + 0x14) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x1c) = 0;
    *reinterpret_cast<uint32_t*>(p + 0x24) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0x3f800000ull;   // +0x28 = 1.0f, +0x2c = 0
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;
}

// ---------------------------------------------------------------------------
// Element destruction
// ---------------------------------------------------------------------------
// Two of the three are array_destroy.cpp's own rows, used through its Core
// bodies. The third, 0x18005c550, is the "eleventh helper" its banner names as
// "a RECURSIVE destroy that calls one of each body before freeing" — the only
// one of the eleven that has never had a caller until this batch.

void DestroyMeshNodeRange(void*, uint8_t* base, int count) {
    ReleaseStringsCore(base, count, &kD_Str_48);        // 0x180014a40
}

void DestroyAnimStateRange(void*, uint8_t* base, int count) {
    FreeInnerArraysCore(base, count, &kD_InnerArr3_48); // 0x18005bce0
}

// 0x18005c550 — Array<AnimationManager::Instance>'s element destroy. Per
// element, in RETAIL'S ORDER (the states array BEFORE the nodes array, which
// is the same out-of-order visit the serializer makes):
//
//   FUN_18005bce0(elem->_states._data, elem->_states._size);   ; per-state
//   MemFree(elem->_states._data); zero its 16-byte header
//   FUN_180014a40(elem->_nodes._data,  elem->_nodes._size);    ; per-node
//   MemFree(elem->_nodes._data);  zero its 16-byte header
//
// The zeroing is FOUR dword stores over the header, unconditional, including
// when the buffer was already null — exactly Body A's shape one level up.
void DestroyInstanceRange(void*, uint8_t* base, int count) {
    if (count <= 0) return;
    const int64_t n = static_cast<int64_t>(count);
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = base + i * 0x30;

        ArrayHdr* states = reinterpret_cast<ArrayHdr*>(e + 0x20);
        FreeInnerArraysCore(states->_data, states->_size, &kD_InnerArr3_48);
        if (states->_data) MemFree(states->_data);
        states->_data = nullptr; states->_size = 0; states->_capacity = 0;

        ArrayHdr* nodes = reinterpret_cast<ArrayHdr*>(e + 0x10);
        ReleaseStringsCore(nodes->_data, nodes->_size, &kD_Str_48);
        if (nodes->_data) MemFree(nodes->_data);
        nodes->_data = nullptr; nodes->_size = 0; nodes->_capacity = 0;
    }
}

// ---------------------------------------------------------------------------
// The specs
// ---------------------------------------------------------------------------
// Field order is array_internal.h's:
//   name, stride, align, assign, init, destroy, byteCopy,
//   assignUnwritten{Lo,Hi}, initUnwritten{Lo,Hi}, seedZero{Lo,Hi}
//
// The `initUnwritten` window can hold ONE hole and two of these rows have two.
// Where that happens the LARGER hole is declared and the other is named in the
// comment; nothing reads these fields today (no probe case is added by this
// batch), and the honest record is the comment either way.

constexpr ArraySpec kSpec_MeshNode = {
    "Array<GraphicsMesh::Node>", 0x48, 8,
    &AssignMeshNode, &InitMeshNode, &DestroyMeshNodeRange, false,
    0, 0,          // the copy ctor writes +0x00..+0x44 (it skips +0x45..+0x47)
    0x08, 0x0c,    // init skips the parent index; it ALSO skips +0x44..+0x47
    0x00, 0x08 };  // the name slot is refcounted — a probe must seed it null

constexpr ArraySpec kSpec_AnimState = {
    "Array<Graphics::AnimationState>", 0x48, 8,
    &AssignAnimState, &InitAnimState, &DestroyAnimStateRange, false,
    0, 0,
    0x01, 0x08,
    0x08, 0x48 };  // three inner Array headers; every one is dereferenced

constexpr ArraySpec kSpec_AnimLayer = {
    "Array<Graphics::AnimationState::Layer>", 0x38, 8,
    &Assign38Q, &InitAnimLayer, nullptr, true,
    0, 0,
    0x10, 0x18,    // init skips the speed; it ALSO skips +0x31..+0x37
    0, 0 };

constexpr ArraySpec kSpec_AnimInstance = {
    "Array<Graphics::AnimationManager::Instance>", 0x30, 8,
    &AssignAnimInstance, &InitAnimInstance, &DestroyInstanceRange, false,
    0, 0,
    0, 0,
    0x10, 0x30 };  // two inner Array headers

constexpr ArraySpec kSpec_InterpTransform = {
    "Array<Graphics::InterpolatedState<Math::Transform>>", 0x38, 4,
    &Assign38D, &InitInterpTransform, nullptr, true,
    0, 0,
    0, 0,
    0, 0 };

// Compile-time checks on the transcription, in array_destroy.cpp's spirit: an
// init hole has to lie inside its own stride, or the window names bytes of the
// NEXT element.
static_assert(kSpec_MeshNode.initUnwrittenHi  <= kSpec_MeshNode.stride,  "MeshNode hole");
static_assert(kSpec_AnimState.initUnwrittenHi <= kSpec_AnimState.stride, "AnimState hole");
static_assert(kSpec_AnimLayer.initUnwrittenHi <= kSpec_AnimLayer.stride, "AnimLayer hole");
static_assert(kSpec_MeshNode.seedZeroHi       <= kSpec_MeshNode.stride,  "MeshNode seed");
static_assert(kSpec_AnimState.seedZeroHi      <= kSpec_AnimState.stride, "AnimState seed");
static_assert(kSpec_AnimInstance.seedZeroHi   <= kSpec_AnimInstance.stride, "Instance seed");

// ---------------------------------------------------------------------------
// J4 — Collection::Array<T>::operator= for a NON-pointer element type
// ---------------------------------------------------------------------------
// array_destroy.cpp already owns this algorithm for `Array<T*>` (0x180003970)
// and array.cpp owns it for `Array<int32>` (0x18001cdf0). The four addresses
// below are the same body with a per-element copy CONSTRUCTOR instead of an
// 8-byte move, so the shape is factored here once:
//
//     if (dst == &src) return dst;
//     Clear(dst);                       ; the matching ~Array — see the table
//     n   = src._size;
//     old = dst._size;                  ; READ AFTER the clear, so ALWAYS 0
//     dst._size = n;
//     if (n > dst._capacity) {
//         dst._capacity = n;
//         if (n == old + 1 && old != 0) dst._capacity = n * 3;   ; DEAD, kept
//         dst._data = MemAlloc((int64)dst._capacity * stride, align);
//     }
//     for (i = 0; i < dst._size; ++i) { ; bound and base RE-READ every pass
//         p = dst._data + i*stride;
//         if (p) ElemCopyCtor(p, src._data + i*stride);
//     }
//     return dst;
//
// BOTH of the notes those two files attach to this body apply verbatim and are
// not repeated in full: `old` is always zero because the clear ran first, so
// the treble-growth arm is unreachable in every one of these instantiations and
// is transcribed anyway; and there is no "copy from the old buffer" limb
// because the clear already freed it.
//
// The element copy is a CONSTRUCTOR into raw memory, not an assignment. For
// kSpec_MeshNode that matters: 0x1800127f0 opens by storing 0 over the name
// slot before taking the source's reference, which is only correct because the
// destination is either fresh MemAlloc memory or memory the clear just
// released.
using ClearFn = void (*)(ArrayHdr*);

void* ArrayAssignCore(ArrayHdr* dst, const ArrayHdr* src,
                      uint32_t stride, int align, ClearFn clear, ElemAssign assign) {
    if (dst == src) return dst;
    clear(dst);

    const int32_t n   = src->_size;
    const int32_t old = dst->_size;          // always 0; see above
    dst->_size = n;
    if (n > dst->_capacity) {                // signed 32-bit compare
        dst->_capacity = n;
        if (n == static_cast<int32_t>(old + 1) && old != 0)
            dst->_capacity = static_cast<int32_t>(n * 3);   // dead; kept faithful
        dst->_data = static_cast<uint8_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(dst->_capacity)) * stride,
                     align));
    }
    for (int32_t i = 0; i < dst->_size; ++i) {
        uint8_t* p = dst->_data + static_cast<int64_t>(i) * stride;
        if (p == nullptr) continue;          // retail tests the COMPUTED address
        assign(p, src->_data + static_cast<int64_t>(i) * stride);
    }
    return dst;
}

// ---- the four clear functions, i.e. the four ~Array bodies -----------------
// Two are lane Q's J1 (free + zero the header) and two are lane D2's J2
// (per-element destroy, then J1). None of the four is a new algorithm.

void Clear_18001c480(ArrayHdr* a) { ArrayDtorImpl(a); }           // Array<Layer>
void Clear_18001c550(ArrayHdr* a) { ArrayDtorImpl(a); }           // Array<InterpolatedState>

void Clear_180014090(ArrayHdr* a) {                                // Array<Node>
    ReleaseStringsCore(a->_data, a->_size, &kD_Str_48);            // CALL 0x180014a40
    ArrayDtorImpl(a);
}

void Clear_18005bbb0(ArrayHdr* a) {                                // Array<AnimationState>
    FreeInnerArraysCore(a->_data, a->_size, &kD_InnerArr3_48);     // CALL 0x18005bce0
    ArrayDtorImpl(a);
}

}  // namespace gfxanim
}  // namespace rtcoll

// ============================================================================
// The registered entry points. One free function per ADDRESS; the address is in
// the name so a crash log or a census line names it directly.
// ============================================================================
// B3R RT-VG: namespace rtcoll (external linkage), was an anonymous namespace --
// the graphics readers call these by name (types/graphics/AnimationState.h).
namespace rtcoll {

using namespace rtcoll::gfxanim;

// ---- J1 / J2: ~Array ------------------------------------------------------
void* Rt_ArrayDtor_18001c480(void* a) { rtcoll::AsArray<&kSpec_AnimLayer>(a)->~Array(); return nullptr; }
void* Rt_ArrayDtor_18001c550(void* a) { rtcoll::AsArray<&kSpec_InterpTransform>(a)->~Array(); return nullptr; }
void* Rt_ArrayDtor_180014090(void* a) { Clear_180014090(static_cast<ArrayHdr*>(a)); return nullptr; }
void* Rt_ArrayDtor_18005bbb0(void* a) { Clear_18005bbb0(static_cast<ArrayHdr*>(a)); return nullptr; }

// ---- the three per-element destroy helpers --------------------------------
// Retail's arity, including the `this` every one of them spills to the shadow
// space and never reads (array_destroy.cpp's "ignoredThis" note applies).

// ---- J3: SetSize ----------------------------------------------------------
void Rt_SetSize_18005aa30(void* a, int n) {   // Array<GraphicsMesh::Node>
    rtcoll::AsArray<&kSpec_MeshNode>(a)->SetSize(n);
}
void Rt_SetSize_18005b1e0(void* a, int n) {   // Array<AnimationState>
    rtcoll::AsArray<&kSpec_AnimState>(a)->SetSize(n);
}
void Rt_SetSize_18005c8a0(void* a, int n) {   // Array<AnimationState::Layer>
    rtcoll::AsArray<&kSpec_AnimLayer>(a)->SetSize(n);
}
void Rt_SetSize_18005ac50(void* a, int n) {   // Array<InterpolatedState<Transform>>
    rtcoll::AsArray<&kSpec_InterpTransform>(a)->SetSize(n);
}
void Rt_SetSize_18005c3d0(void* a, int n) {   // Array<Instance>, no fill
    rtcoll::AsArray<&kSpec_AnimInstance>(a)->SetSize(n);
}
// The one THREE-argument row in this batch. Its new elements are copy-assigned
// from `fill` instead of default-constructed, which is what distinguishes it
// from 0x18005c3d0 above; the rest of the body is the same instantiation.
void Rt_SetSize_18005be30(void* a, int n, const void* fill) {
    rtcoll::AsArray<&kSpec_AnimInstance>(a)->SetSizeFill(n, fill);
}

// ---- J4: operator= --------------------------------------------------------
void* Rt_ArrayAssign_180013960(void* d, const void* s) {   // Array<GraphicsMesh::Node>
    return ArrayAssignCore(static_cast<ArrayHdr*>(d), static_cast<const ArrayHdr*>(s),
                           0x48, 8, &Clear_180014090, &AssignMeshNode);
}
void* Rt_ArrayAssign_18005b130(void* d, const void* s) {   // Array<AnimationState>
    return ArrayAssignCore(static_cast<ArrayHdr*>(d), static_cast<const ArrayHdr*>(s),
                           0x48, 8, &Clear_18005bbb0, &AssignAnimState);
}
void* Rt_ArrayAssign_18001b700(void* d, const void* s) {   // Array<AnimationState::Layer>
    return ArrayAssignCore(static_cast<ArrayHdr*>(d), static_cast<const ArrayHdr*>(s),
                           0x38, 8, &Clear_18001c480, &Assign38Q);
}
void* Rt_ArrayAssign_18001b810(void* d, const void* s) {   // Array<InterpolatedState<Transform>>
    return ArrayAssignCore(static_cast<ArrayHdr*>(d), static_cast<const ArrayHdr*>(s),
                           0x38, 4, &Clear_18001c550, &Assign38D);
}

}  // namespace rtcoll

// Every one of the seventeen is a template instantiation with NO export name in
// either shipped build, so all seventeen are RegisterRuntimeAddr and none is a
// RegisterRuntimeExport. (array.cpp's banner establishes that for the family:
// the profile DLL's 5,182-name export directory has no `Array@Collection` row.)
// ENDGAME B.3.2 closing pass (2026-09-18): the three per-element destroy
// wrappers (0x180014a40 nodes, 0x18005bce0 states, 0x18005c550 instances) and
// their rows were removed at zero callers — nothing in the tree resolves those
// addresses in any form, and their CORES (DestroyMeshNodeRange /
// DestroyAnimStateRange / DestroyInstanceRange) are still called from this
// file's own dtor bodies, so only the retail-arity forwarders went.
int RtCollectionGraphicsAnimRegister() {
    // B3R RT-Core: every row this registered lost its last reader. Kept
    // defined, returning 0, only because runtime/src/graphics/animationstate.cpp (RT-VG) still calls it.
    return 0;
}
