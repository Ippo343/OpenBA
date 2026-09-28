// ============================================================================
// runtime/src/graphics/animationstate.cpp
// Graphics::AnimationState / AnimationManager / Animation — the serializable
// half of the skeleton-animation cluster (cutover closure batch 3, 2026-09-12).
// ============================================================================
//
// WHAT IS IN HERE, with retail addresses. Four are exported; the rest are
// template instantiations and per-record helpers with no name in either build,
// registered by address.
//
//   0x1800127f0  ??0Node@GraphicsMesh@Graphics@@QEAA@AEBU012@@Z   copy ctor
//   0x18005b660  (unexported)  Graphics::AnimationState copy ctor
//   0x18005b700  (unexported)  AnimationManager::Instance copy ctor
//
//   0x180059820  (unexported)  Array<Math::Transform>::Serialize
//   0x18005c230  (unexported)  Array<InterpolatedState<Transform>>::Serialize
//   0x18005c120  (unexported)  Array<GraphicsMesh::Node>::Serialize
//   0x18005cb40  (unexported)  Array<AnimationState::Layer>::Serialize
//   0x18005c2c0  (unexported)  Array<AnimationState>::Serialize
//   0x18005c660  (unexported)  Array<AnimationManager::Instance>::Serialize
//
//   0x1800fe250  ?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z
//   0x1800fd930  ?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ
//   0x1800fb8e0  ?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z
//   0x1800faca0  ?GetPoseAtTime@Animation@Graphics@@QEBAXAEBVTime@System@@0PEAVTransform@Math@@H@Z
//
// The Array<T> instantiations these bodies call live in
// runtime/src/collection/array_graphics_anim.cpp, with the Collection family
// they belong to.
//
// ---------------------------------------------------------------------------
// THE INSTANCE HANDLE, because three bodies here decode it and none of them
// says what it is
// ---------------------------------------------------------------------------
// `AnimationState::Layer::handle` (+0x0c) is a packed pair, or -1 for "this
// layer is not instanced". `GetInstanceId` builds it as `instanceIndex << 8 |
// frameIndex` on its last line; `GetPose` and `ResetAnimationState` take it
// apart with `(h >> 8) & 0xff` and `h & 0xff`. So there is a hard ceiling of
// 256 cached instances and 256 frames per instanced clip, and the layer stores
// no more than one byte of each. That ceiling is retail's, not ours.
//
// ---------------------------------------------------------------------------
// THE HIDDEN RETURN SLOT, and the one place it applies here
// ---------------------------------------------------------------------------
// `Math::Quaternion::Lerp` (0x18001d500) returns a Quaternion BY VALUE, so
// MSVC passes the destination buffer in RDX and shifts the real arguments
// right: RCX = the FROM quaternion, RDX = the return buffer, R8 = the TO
// quaternion, R9 = &t. The typedef below carries that slot. Getting it wrong
// is a wild write into whatever the caller had in RDX
// (reference_hidden_retslot_wild_write.md); it is spelled the same way
// runtime/src/graphics/mesh.cpp:1801 and scene.cpp:366 already spell it, so
// there is one shape to get right and not three.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>                 // placement new, for the Node copy-ctor door

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/collection/Collection.h"
#include "../../types/graphics/AnimationManager.h"
#include "../../types/graphics/GraphicsMesh.h"   // ENDGAME B.3.5 block G
// ENDGAME B.3.5 block H1: the real `Graphics::Animation` / `AnimationState` /
// `AnimationManager` classes, and `System::Time` for the two tick parameters
// `Animation::GetPoseAtTime` takes by reference. Value.h is what drags the ONE
// global `struct Time` (engine/types/common.h) into this translation unit; no
// other header here declares one, so there is nothing for it to collide with.
#include "../../types/graphics/Animation.h"
#include "../../types/graphics/AnimationState.h"
#include "../../types/system/Value.h"
#include "../../types/system/Timer.h"   // ENDGAME B.3.9 SS1: Timer::_secondsPerTick
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"         // RtObject_ExternalPtrSerialize_AnimationGroup
#include "../../types/math/Math.h"               // Math::Quaternion::Lerp
#include "../../types/system/MemoryAllocator.h"
#include "../picking/scene_grid.h"               // rtpickgrid::TransformArrayAssign

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

using RtGraphics::AnimInstanceRec;
using RtGraphics::AnimStateRec;
using RtGraphics::AnimationManagerView;
namespace anim_clip = RtGraphics::anim_clip;

namespace {

using ArrayHdr = Collection::Array<uint8_t>;

// ---------------------------------------------------------------------------
// The neighbours these bodies call, by name (B3R). The Collection::Array<T>
// instantiations have no name in either shipped build: rtcoll::Rt_SetSize_* /
// Rt_ArrayAssign_* (array_graphics_anim.cpp, declared in AnimationState.h),
// rtcoll::RtColl_ArraySetSize_180059140 (Array<Math::Transform>::SetSize),
// rtpickgrid::TransformArrayAssign (0x18001b360, Array<Math::Transform>::
// operator=). ExternalPtr<AnimationGroup>::Serialize is
// RtObject_ExternalPtrSerialize_AnimationGroup (0x18005b840) and
// Math::Quaternion::Lerp (0x18001d500) is the member.
// ---------------------------------------------------------------------------
using rtpickgrid::TransformArrayAssign;

// ?_singleton@AnimationManager@Graphics@@0PEAV12@EA — the slot at 0x1801fe7b0.
// ONE deref of the slot gives the manager; the manager's +0x00 is the instance
// buffer. `MOV RAX,[0x1801fe7b0]` / `ADD RBX,[RAX]` at 0x1800fd97a is the pair.
// ENDGAME B.3.9 GFX: the class static, not the mint's slot (D7 row A44).
AnimationManagerView* AnimationManagerSingleton() {
    return reinterpret_cast<AnimationManagerView*>(
        Graphics::AnimationManager::_singleton);   // B.3.9 bridge (the POD view)
}

// ?_secondsPerTick@Timer@System@@0NA — a DOUBLE. Every tick<->seconds
// conversion in this file goes through it, exactly as retail's inlined
// `CVTSI2SD / MULSD [rip+_secondsPerTick]` does.
double SecondsPerTick() {
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    return ::System::Timer::_secondsPerTick;
}

// ---------------------------------------------------------------------------
// ReaderWriter calls. Four members, and only four, appear in this cluster:
// SerializeInt (+0xA0), SerializeFloat (+0x80), SerializeBool (+0x78) and
// SerializeFloatArray (+0x28). B3X W0: by name on the real class; the
// `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
// interchangeable in Serializer.h. `name` is nullptr, as retail passes.
// ---------------------------------------------------------------------------
IO::ReaderWriter* AsRw(ReaderWriter* rw) {
    return reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}
void RwInt(ReaderWriter* rw, void* p)   { AsRw(rw)->SerializeInt(*static_cast<int32_t*>(p), nullptr); }
void RwFloat(ReaderWriter* rw, void* p) { AsRw(rw)->SerializeFloat(*static_cast<float*>(p), nullptr); }
void RwBool(ReaderWriter* rw, void* p)  { AsRw(rw)->SerializeBool(*static_cast<bool*>(p), nullptr); }
void RwFloats(ReaderWriter* rw, int count, void* p) {
    AsRw(rw)->SerializeFloatArray(count, static_cast<float*>(p), nullptr);
}

// `stream->direction == 0` is IO::Stream::Direction::Reading. Every "if we are
// loading, resize the destination first" guard below is retail's
// `MOV RAX,[RDI]; CMP dword ptr [RAX+8],0; JNZ`.
bool IsReading(const Serializer* ser) {
    return ser->stream->direction == rt::io::kDirectionReading;
}

}  // namespace

// ============================================================================
// The three element copy constructors
// ============================================================================

// 0x1800127f0 — ??0Node@GraphicsMesh@Graphics@@QEAA@AEBU012@@Z.
//
// A refcounting copy of the node name followed by a flat fifteen-dword copy of
// everything from +0x08 to +0x44. The name slot is stored ZERO first and only
// then given the source's pointer, which is what makes this safe to run into
// raw MemAlloc memory — and is why Array<Node>::operator= can call it directly
// on a buffer it has just allocated.
//
// *** +0x44..+0x47 IS NOT COPIED. *** The last dword the loop writes is at
// +0x40, so the copy covers +0x08..+0x43 and the tail dword at +0x44 keeps
// whatever the destination held. Zeroing it here would be unfaithful and would
// make a byte comparator fail on bytes neither side writes.
//
// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::GraphicsMesh::Node`'s copy
// constructor, at the bottom of this file's `namespace Graphics` block. The
// fifteen-dword flat copy became three member assignments — `_parent` (4) +
// `_local` (0x1c) + `_bind` (0x1c) is EXACTLY +0x08..+0x43 — which is also why
// `_pad44` is left alone without a comment having to promise it.
extern "C" void* RtGraphics_MeshNode_CopyCtor(void* dstv, const void* srcv) {
    return new (dstv) Graphics::GraphicsMesh::Node(                  // B.3.5 bridge
        *reinterpret_cast<const Graphics::GraphicsMesh::Node*>(srcv));
}

// 0x18005b660 — Graphics::AnimationState's copy constructor.
//
// The three Array members are ZEROED and then copy-ASSIGNED, in that order.
// The zeroing is not tidiness: `operator=` opens by destroying the
// destination, so handing it the garbage that is in raw MemAlloc memory would
// free a garbage pointer. (Same reasoning array.cpp records for the
// GraphSeries element assign.)
extern "C" void* RtGraphics_AnimState_CopyCtor(void* dstv, const void* srcv) {
    uint8_t* d = static_cast<uint8_t*>(dstv);
    const uint8_t* s = static_cast<const uint8_t*>(srcv);

    d[0] = s[0];                                                   // _needsUniqueCopy

    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;                    // _layers
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    rtcoll::Rt_ArrayAssign_18001b700(d + 0x08, s + 0x08);                            // CALL 0x18001b700

    *reinterpret_cast<uint32_t*>(d + 0x18) =                       // _rootOffset
        *reinterpret_cast<const uint32_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x1c) =
        *reinterpret_cast<const uint32_t*>(s + 0x1c);
    *reinterpret_cast<uint32_t*>(d + 0x20) =
        *reinterpret_cast<const uint32_t*>(s + 0x20);

    *reinterpret_cast<uint64_t*>(d + 0x28) = 0;                    // _pose
    *reinterpret_cast<uint64_t*>(d + 0x30) = 0;
    TransformArrayAssign(d + 0x28, s + 0x28);                              // CALL 0x18001b360

    *reinterpret_cast<uint64_t*>(d + 0x38) = 0;                    // _nodes
    *reinterpret_cast<uint64_t*>(d + 0x40) = 0;
    rtcoll::Rt_ArrayAssign_18001b810(d + 0x38, s + 0x38);                            // CALL 0x18001b810
    return d;
}

// 0x18005b700 — AnimationManager::Instance's copy constructor.
//
// The ExternalPtr at +0x00 is copied as a RAW QWORD with no reference taken.
// That is retail's, and it is consistent with the rest of the class: the
// instance table is a cache keyed by the pointer, and the reference that keeps
// the group alive is the LAYER's, not the cache entry's.
extern "C" void* RtGraphics_AnimInstance_CopyCtor(void* dstv, const void* srcv) {
    uint8_t* d = static_cast<uint8_t*>(dstv);
    const uint8_t* s = static_cast<const uint8_t*>(srcv);

    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint32_t*>(d + 0x0c) = *reinterpret_cast<const uint32_t*>(s + 0x0c);

    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;                    // _nodes
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
    rtcoll::Rt_ArrayAssign_180013960(d + 0x10, s + 0x10);                             // CALL 0x180013960

    *reinterpret_cast<uint64_t*>(d + 0x20) = 0;                    // _states
    *reinterpret_cast<uint64_t*>(d + 0x28) = 0;
    rtcoll::Rt_ArrayAssign_18005b130(d + 0x20, s + 0x20);                            // CALL 0x18005b130
    return d;
}

// ============================================================================
// The six serializers
// ============================================================================
//
// Five take retail's `(Serializer*, int count, void* base)` element-loop shape;
// the sixth (0x180059820) takes `(Array*, Serializer*)` and does the count and
// the resize itself. The asymmetry is retail's and is reproduced.
//
// Every one of them reads the DESTINATION sizes back out of the array header
// AFTER the resize, never from the local it just deserialized — which is why
// `count` is passed on rather than re-read.

// 0x180059820 — Array<Math::Transform>::Serialize. Element 0x1c: a quaternion
// (four floats at +0x00) and a Vector3 (three floats at +0x10).
extern "C" void RtGraphics_SerializeTransformArray(void* arrayv, Serializer* ser) {
    ArrayHdr* a = static_cast<ArrayHdr*>(arrayv);
    ReaderWriter* rw = ser->readerWriter;

    int32_t n = a->_size;
    RwInt(rw, &n);
    if (IsReading(ser)) rtcoll::RtColl_ArraySetSize_180059140(a, n);        // CALL 0x180059140

    uint8_t* p = a->_data;
    for (int32_t i = 0; i < n; ++i, p += 0x1c) {
        RwFloats(rw, 4, p);
        RwFloats(rw, 3, p + 0x10);
    }
}

// 0x18005c230 — Array<InterpolatedState<Transform>>::Serialize's element loop.
// Two 0x1c Math::Transforms back to back.
extern "C" void RtGraphics_SerializeInterpTransforms(Serializer* ser, int count, void* basev) {
    if (count <= 0) return;
    ReaderWriter* rw = ser->readerWriter;
    uint8_t* p = static_cast<uint8_t*>(basev);
    for (int i = 0; i < count; ++i, p += 0x38) {
        RwFloats(rw, 4, p + 0x00);
        RwFloats(rw, 3, p + 0x10);
        RwFloats(rw, 4, p + 0x1c);
        RwFloats(rw, 3, p + 0x2c);
    }
}

// 0x18005c120 — Array<GraphicsMesh::Node>::Serialize's element loop.
// {String name, int parent, Transform local, Transform bind}, stride 0x48.
// The last three floats are at +0x38 (read out of `MOV R8,RBX` at 0x18005c1ad,
// where RBX is elem+0x38 — Ghidra's decompiler drops that operand).
extern "C" void RtGraphics_SerializeMeshNodes(Serializer* ser, int count, void* basev) {
    if (count <= 0) return;
    ReaderWriter* rw = ser->readerWriter;
    uint8_t* p = static_cast<uint8_t*>(basev);
    for (int i = 0; i < count; ++i, p += 0x48) {
        // B.3.1 bridge: GraphicsMesh::Node::_name, untyped node storage.
        reinterpret_cast<System::String*>(p + 0x00)->Serialize(ser);
        RwInt(rw, p + 0x08);
        RwFloats(rw, 4, p + 0x0c);
        RwFloats(rw, 3, p + 0x1c);
        RwFloats(rw, 4, p + 0x28);
        RwFloats(rw, 3, p + 0x38);
    }
}

// 0x18005cb40 — Array<AnimationState::Layer>::Serialize's element loop.
//
// NOTE THE ORDER: the int at +0x0c (the packed instance handle) is written
// SIXTH, after the three Times, not second next to the clip index. Reordering
// it to match the declaration would change the byte layout of every save.
extern "C" void RtGraphics_SerializeAnimLayers(Serializer* ser, int count, void* basev) {
    if (count <= 0) return;
    ReaderWriter* rw = ser->readerWriter;
    uint8_t* p = static_cast<uint8_t*>(basev);
    for (int i = 0; i < count; ++i, p += 0x38) {
        RtObject_ExternalPtrSerialize_AnimationGroup(reinterpret_cast<RtObject::External**>(p + 0x00), ser);                  // ExternalPtr<AnimationGroup>
        RwInt(rw, p + 0x08);       // clip index
        RwFloat(rw, p + 0x10);       // speed / weight
        // ENDGAME B.3.5 block H1 — THE THREE HAND-INLINED int64 SLOT CALLS
        // B35_RULES section 11 listed as "receiver-side work". Block H1 owns the
        // receiver (`Graphics::AnimationState::Layer`), and
        // `IO::Serializer::Serialize(int64_t&, const char*)`
        // (`?Serialize@Serializer@IO@@QEAAXAEA_JPEBD@Z` @ 0x180035000) is a real
        // member, so they are spelled as member calls. The member's own body
        // still calls the ReaderWriter's +0x90 slot (Serializer.h's section 3.1
        // banner: the 22 members KEEP their raw slot calls until the minted
        // tables retire), so this is a spelling change and nothing else — and it
        // removes the last three places a Graphics body spells `0x90` by hand.
        // The `rt::io::Serializer` POD and `IO::Serializer` are asserted
        // layout-identical in runtime/types/io/Serializer.h.
        IO::Serializer* const sc = reinterpret_cast<IO::Serializer*>(ser);  // B.3.5 bridge
        sc->Serialize(*reinterpret_cast<int64_t*>(p + 0x18), nullptr);      // time
        sc->Serialize(*reinterpret_cast<int64_t*>(p + 0x20), nullptr);      // fade length
        sc->Serialize(*reinterpret_cast<int64_t*>(p + 0x28), nullptr);      // fade remaining
        RwInt(rw, p + 0x0c);       // packed instance handle
        RwBool(rw, p + 0x30);       // restarted
    }
}

// 0x18005c2c0 — Array<AnimationState>::Serialize's element loop.
//
// Visits the layer array, the bool, the node array, the root offset and the
// pose array, in that order. The bool at +0x00 lands BETWEEN the two arrays,
// which is why it is not hoisted to the top.
extern "C" void RtGraphics_SerializeAnimStates(Serializer* ser, int count, void* basev) {
    if (count <= 0) return;
    ReaderWriter* rw = ser->readerWriter;
    uint8_t* p = static_cast<uint8_t*>(basev);
    for (int i = 0; i < count; ++i, p += 0x48) {
        int32_t n = *reinterpret_cast<const int32_t*>(p + 0x10);     // _layers._size
        RwInt(rw, &n);
        if (IsReading(ser)) rtcoll::Rt_SetSize_18005c8a0(p + 0x08, n);            // CALL 0x18005c8a0
        RtGraphics_SerializeAnimLayers(ser, n, *reinterpret_cast<void* const*>(p + 0x08));

        RwBool(rw, p + 0x00);

        n = *reinterpret_cast<const int32_t*>(p + 0x40);             // _nodes._size
        RwInt(rw, &n);
        if (IsReading(ser)) rtcoll::Rt_SetSize_18005ac50(p + 0x38, n);            // CALL 0x18005ac50
        RtGraphics_SerializeInterpTransforms(ser, n, *reinterpret_cast<void* const*>(p + 0x38));

        RwFloats(rw, 3, p + 0x18);                                   // _rootOffset

        RtGraphics_SerializeTransformArray(p + 0x28, ser);           // CALL 0x180059820
    }
}

// 0x18005c660 — Array<AnimationManager::Instance>::Serialize's element loop.
// The STATES array is visited before the NODES array; see AnimationManager.h.
extern "C" void RtGraphics_SerializeAnimInstances(Serializer* ser, int count, void* basev) {
    if (count <= 0) return;
    ReaderWriter* rw = ser->readerWriter;
    uint8_t* p = static_cast<uint8_t*>(basev);
    for (int i = 0; i < count; ++i, p += 0x30) {
        RtObject_ExternalPtrSerialize_AnimationGroup(reinterpret_cast<RtObject::External**>(p + 0x00), ser);
        RwInt(rw, p + 0x08);                 // clip index
        RwInt(rw, p + 0x0c);                 // reference count

        int32_t n = *reinterpret_cast<const int32_t*>(p + 0x28);     // _states._size
        RwInt(rw, &n);
        if (IsReading(ser)) rtcoll::Rt_SetSize_18005b1e0(p + 0x20, n);            // CALL 0x18005b1e0
        RtGraphics_SerializeAnimStates(ser, n, *reinterpret_cast<void* const*>(p + 0x20));

        n = *reinterpret_cast<const int32_t*>(p + 0x18);             // _nodes._size
        RwInt(rw, &n);
        if (IsReading(ser)) rtcoll::Rt_SetSize_18005aa30(p + 0x10, n);             // CALL 0x18005aa30
        RtGraphics_SerializeMeshNodes(ser, n, *reinterpret_cast<void* const*>(p + 0x10));
    }
}

// ============================================================================
// ?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z  (0x1800fe250)
// ============================================================================
//
// *** IT SERIALIZES ONLY THE LAYERS AND THE BOOL. *** The pose, node and root
// offset members that the ARRAY element serializer above writes are NOT written
// here. That is retail's and it is consistent: those three are derived state —
// AnimationState::Update recomputes them from the layers on the next tick — and
// this entry point is the one a MeshInstance's own Serialize reaches. The array
// form is the AnimationManager's instance CACHE, which has to come back whole
// because nothing rebuilds it.
//
// The last call is a TAIL JUMP through the ReaderWriter vtable in retail
// (Ghidra prints "Could not recover jumptable ... Treating indirect jump as
// call" at 0x1800fe2c3); a call is the same thing without the tail merge.
// ENDGAME B.3.5 block H1: THE BODY MOVED INTO `Graphics::AnimationState::
// Serialize` (this file's `namespace Graphics` block). The door keeps its
// mangled name and its retail address and is a one-line qualified call.
extern "C" void RtGraphics_AnimationState_Serialize(void* selfv, Serializer* ser) {
    reinterpret_cast<Graphics::AnimationState*>(selfv)                      // B.3.5 bridge
        ->Graphics::AnimationState::Serialize(*reinterpret_cast<IO::Serializer*>(ser));
}

// ============================================================================
// ?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ  (0x1800fd930)
// ============================================================================
//
// Drop every layer's reference on the shared instance it was using, then free
// the layer array outright. The free is the J1 `~Array` body inlined — retail
// does the Enter/Free/Leave itself here rather than calling 0x18001c480 — so
// this is written out rather than routed through the Collection port, which is
// what makes it byte-faithful and not merely equivalent.
//
// A REFERENCE THAT REACHES ZERO DOES NOT FREE THE INSTANCE RECORD. It blanks
// it in place (group = null, clip index = -1, both arrays resized to 0) and
// leaves it in the table, which is what lets GetInstanceId reuse the slot: its
// first pass looks for `group == 0` and remembers the first such index.
// ENDGAME B.3.5 block H1: THE BODY MOVED INTO
// `Graphics::AnimationState::ResetAnimationState`.
extern "C" void RtGraphics_AnimationState_Reset(void* selfv) {
    reinterpret_cast<Graphics::AnimationState*>(selfv)                       // B.3.5 bridge
        ->Graphics::AnimationState::ResetAnimationState();
}

// ============================================================================
// ?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z (0x1800fb8e0)
// ============================================================================
//
// Copy `count` baked Math::Transforms out of the cached instance the packed
// handle names. Unguarded on every step — a bad handle indexes off the end of
// the instance table — and that is retail's, so it stays that way.
// ENDGAME B.3.5 block H1: THE BODY MOVED INTO
// `Graphics::AnimationManager::GetPose(int, Math::Transform*, int) const`.
extern "C" void RtGraphics_AnimationManager_GetPose(const void* selfv, int handle,
                                                    void* outv, int count) {
    reinterpret_cast<const Graphics::AnimationManager*>(selfv)                // B.3.5 bridge
        ->Graphics::AnimationManager::GetPose(
            handle, static_cast<Math::Transform*>(outv), count);
}

// ============================================================================
// ?GetPoseAtTime@Animation@Graphics@@QEBAXAEBVTime@System@@0PEAVTransform@Math@@H@Z
// (0x1800faca0)
// ============================================================================
//
// Sample the clip at `time`, writing one Math::Transform per bone into `out`,
// then overwrite bone 0 with an identity rotation and a translation scaled by
// `delta`. That last step is root-motion extraction: the caller wants bone 0 to
// carry how far the root moved over `delta` and no rotation at all.
//
// THE FOURTH ARGUMENT (`int`) IS NEVER READ. The bone count comes from the
// clip's own +0x4c, not from the caller. It is kept in the signature because
// retail's callers pass it and the registered entry point must have retail's
// arity, not a tidied one.
//
// THE NaN CASE IS NOT AN ACCIDENT. Retail's test is
// `UCOMISS XMM9,XMM10 / JP taken / JZ` — the parity jump sends an unordered
// compare to the INTERPOLATING arm, so `frac != frac` takes the lerp. Written
// as `f0 == f1 || frac == 0.0f` in C, which has exactly that behaviour.
// ENDGAME B.3.5 block H1: THE BODY MOVED INTO `Graphics::Animation::
// GetPoseAtTime` (below, in this file's `namespace Graphics` block). The
// registered door keeps its mangled name AND its retail address and is now a
// one-line QUALIFIED, non-virtual call. `Graphics::Animation` has no vtable at
// all, so the qualification is not load-bearing the way it is on a polymorphic
// receiver (B34_BRIEFS A.1) — it is spelled anyway so the shape is uniform.
//
// The two `System::Time` parameters arrive as `const int64_t*` here because
// that is retail's ABI for a `const System::Time&` (one tick count, 8 bytes),
// and `System::Time` is layout-identical to it (`Value.h`, `_ticks` @ +0x00).
extern "C" void RtGraphics_Animation_GetPoseAtTime(const void* selfv,
                                                   const int64_t* time,
                                                   const int64_t* delta,
                                                   void* outv, int unused) {
    // B.3.5 bridge
    reinterpret_cast<const Graphics::Animation*>(selfv)->Graphics::Animation::GetPoseAtTime(
        *reinterpret_cast<const System::Time*>(time),
        *reinterpret_cast<const System::Time*>(delta),
        static_cast<Math::Transform*>(outv), unused);
}

// ############################################################################
// ENDGAME B.3.5 step 5, block H1 — the member bodies.
//
// Their own `namespace Graphics { }` block, after this file's anonymous
// namespace has closed, because a member definition has to appear in a namespace
// ENCLOSING the class and `Graphics` / `RtGraphics` are siblings (B34_BRIEFS
// section A.2). The anonymous namespace's helpers (`RwInt`, `IsReading`,
// `SetSize*`, `RtAddr`, `AnimationManagerSingleton`, `SecondsPerTick`,
// `QuaternionLerp`) are reachable unqualified: they are members of the global
// namespace's unnamed namespace, in this same translation unit.
//
// FOUR BODIES MOVED HERE, and the doors above keep their mangled names, their
// retail addresses and their registrations. None of these three classes is
// polymorphic, so the `Class::` on each delegation is uniformity rather than
// recursion-avoidance (B34_BRIEFS A.1) — it is spelled anyway.
//
// GREP HAZARD: a bare `AnimationState` / `AnimationManager` / `Animation` in
// here binds the CLASS. The PODs this file also uses — `AnimStateRec`,
// `AnimInstanceRec`, `AnimationManagerView` — are spelled `RtGraphics::` inside
// this block.
// ############################################################################

namespace Graphics {

// ============================================================================
// ?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z  (0x1800fe250)
//
// *** IT SERIALIZES ONLY THE LAYERS AND THE BOOL. *** `_pose`, `_nodes` and
// `_rootOffset` are NOT written here. That is retail's and it is consistent:
// those three are derived state that `Update` recomputes on the next tick, and
// this entry point is the one a `MeshInstance`'s own Serialize reaches. The
// ARRAY form (`RtGraphics_SerializeAnimStates`, 0x18005c2c0) is the
// AnimationManager's instance CACHE, which has to come back whole because
// nothing rebuilds it.
//
// The last call is a TAIL JUMP through the ReaderWriter vtable in retail
// (Ghidra prints "Could not recover jumptable" at 0x1800fe2c3); a call is the
// same thing without the tail merge.
//
// The int count and the bool go through the ReaderWriter's SerializeInt (+0xA0)
// and SerializeBool (+0x78), by name since B3X W0.
// ============================================================================
void AnimationState::Serialize(IO::Serializer& s) {
    // The POD view of the same 16 bytes — Serializer.h asserts the two
    // interchangeable. Used because the helpers above take the POD.
    Serializer* ser = reinterpret_cast<Serializer*>(&s);   // B.3.5 bridge
    ReaderWriter* rw = ser->readerWriter;

    int32_t n = _layers._size;
    RwInt(rw, &n);
    if (IsReading(ser)) rtcoll::Rt_SetSize_18005c8a0(&_layers, n);
    RtGraphics_SerializeAnimLayers(ser, n, _layers._data);
    RwBool(rw, &_needsUniqueCopy);
}

// ============================================================================
// ?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ  (0x1800fd930)
//
// Drop every layer's reference on the shared instance it was using, then free
// the layer array outright. The free is the J1 `~Array` body INLINED — retail
// does the Enter/Free/Leave itself here rather than calling 0x18001c480 — so it
// is written out rather than routed through the Collection port, which is what
// makes it byte-faithful and not merely equivalent.
//
// A REFERENCE THAT REACHES ZERO DOES NOT FREE THE INSTANCE RECORD. It blanks it
// in place (group = null, clip index = -1, both arrays resized to 0) and leaves
// it in the table, which is what lets `GetInstanceId` reuse the slot: its first
// pass looks for `group == 0` and remembers the first such index.
//
// THE SINGLETON STAYS AN `RT_DATA_SLOT` READ. `?_singleton@AnimationManager@
// Graphics@@0PEAV12@EA` @ 0x1801fe7b0 is a `CUTOVER_REQUIRED` row with six
// by-name doors; per B35_RULES section 9 / RULE B4b-4 its `MintStorageCell()`
// redirect is OWED at the class-static conversion, the END of B.3, not here.
//
// Every deref is UNGUARDED, exactly as retail's is.
// ============================================================================
void AnimationState::ResetAnimationState() {
    for (int32_t i = 0; i < _layers._size; ++i) {
        const int32_t handle =
            *reinterpret_cast<const int32_t*>(_layers._data + i * 0x38 + 0x0c);
        if (handle < 0) continue;                        // TEST ECX,ECX / JS

        RtGraphics::AnimationManagerView* mgr = AnimationManagerSingleton();
        uint8_t* inst = mgr->_instances._data +
                        static_cast<int64_t>((handle >> 8) & 0xff) * 0x30;
        if (--*reinterpret_cast<int32_t*>(inst + 0x0c) != 0) continue;

        *reinterpret_cast<uint64_t*>(inst + 0x00) = 0;   // _group
        *reinterpret_cast<int32_t*>(inst + 0x08)  = -1;  // _animIndex
        rtcoll::Rt_SetSize_18005aa30(inst + 0x10, 0);                  // CALL 0x18005aa30
        rtcoll::Rt_SetSize_18005b1e0(inst + 0x20, 0);                 // CALL 0x18005b1e0
    }

    // The inlined ~Array. `MemFree` is null-guarded inside 0x1800c7ad0, which is
    // instruction for instruction what retail inlines here.
    if (_layers._data != nullptr)
        System::MemoryAllocator::MemFree(_layers._data);
    _layers._data     = nullptr;
    _layers._size     = 0;
    _layers._capacity = 0;
}

// ============================================================================
// ?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z (0x1800fb8e0)
//
// Copy `count` baked `Math::Transform`s out of the cached instance the packed
// handle names. `(handle >> 8) & 0xff` is the instance index and `handle & 0xff`
// the frame — the 256/256 ceiling is retail's, not ours.
//
// UNGUARDED ON EVERY STEP: a bad handle indexes off the end of the instance
// table. That is retail's, so it stays that way (CLAUDE.md: retail's unguarded
// derefs are load-bearing).
//
// The copy is SEVEN 32-BIT MOVES per Transform rather than a float assignment,
// for the reason wave21 records: an integer move of a float is bit-exact where a
// float assignment is only value-exact, and the two differ on a signalling NaN
// or a negative zero. The nested records are still reached at raw offsets
// (`+0x20` = `Instance::_states._data`, `+0x28` = `AnimationState::_pose._data`)
// because the class's own Array members are the RAW header type; the offsets are
// the ones the headers assert.
// ============================================================================
void AnimationManager::GetPose(int handle, Math::Transform* outv, int count) const {
    const uint8_t* inst = _instances._data +
                          static_cast<int64_t>((handle >> 8) & 0xff) * 0x30;
    const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(inst + 0x20);
    const uint8_t* pose = *reinterpret_cast<const uint8_t* const*>(
        states + static_cast<int64_t>(handle & 0xff) * 0x48 + 0x28);

    uint8_t* out = reinterpret_cast<uint8_t*>(outv);
    for (int i = 0; i < count; ++i, out += 0x1c, pose += 0x1c)
        for (int w = 0; w < 7; ++w)
            reinterpret_cast<uint32_t*>(out)[w] = reinterpret_cast<const uint32_t*>(pose)[w];
}

// ?GetPoseAtTime@Animation@Graphics@@QEBAXAEBVTime@System@@0PEAVTransform@Math@@H@Z
// retail 0x1800faca0. The banner above the door carries the porting argument;
// what changed in this commit is the SPELLING — every `clip + anim_clip::kX`
// offset became a named member of `Graphics::Animation`, whose layout is pinned
// in runtime/types/graphics/Animation.h against the same instructions.
void Animation::GetPoseAtTime(const System::Time& timeRef, const System::Time& deltaRef,
                              Math::Transform* outv, int /*unused*/) const {
    const int64_t* const time  = &timeRef._ticks;
    const int64_t* const delta = &deltaRef._ticks;
    uint8_t* out = reinterpret_cast<uint8_t*>(outv);

    const int32_t loopFrame  = _loopFrame;
    const int32_t frameFirst = _startFrame;
    const int32_t frameLast  = _endFrame;
    const int32_t frameCount = _frameCount;
    const int32_t boneCount  = _boneCount;
    const uint8_t  looping   = _looping ? 1u : 0u;
    const float    rate      = _frameRate;
    const uint8_t* keys      = _keys._data;

    // Where a loop restarts, as a frame index. Zero when the clip has no loop
    // frame at all — and note that THIS is the short-circuit, not a test on the
    // looping flag: a looping clip with loopFrame == 0 restarts at frame 0.
    int32_t loopStart = 0;
    if (loopFrame != 0) {
        loopStart = static_cast<int32_t>(
            (static_cast<float>(loopFrame - frameFirst) /
             static_cast<float>(frameLast - frameFirst)) * static_cast<float>(frameCount));
    }

    const float cursor = static_cast<float>(static_cast<double>(*time) * SecondsPerTick()) * rate;
    const int32_t whole = static_cast<int32_t>(cursor);

    int32_t f0 = whole;
    if (f0 >= frameCount)
        f0 = looping ? (loopStart + whole % (frameCount - loopStart)) : (frameCount - 1);
    int32_t f1 = f0 + 1;
    if (f1 >= frameCount)
        f1 = looping ? (f1 % (frameCount - loopStart) + loopStart) : (frameCount - 1);

    const float frac = cursor - static_cast<float>(whole);

    if (f0 == f1 || frac == 0.0f) {
        const uint8_t* src = keys + static_cast<int64_t>(boneCount * f0) * 0x1c;
        uint8_t* dst = out;
        for (int32_t i = 0; i < boneCount; ++i, src += 0x1c, dst += 0x1c)
            for (int w = 0; w < 7; ++w)
                reinterpret_cast<uint32_t*>(dst)[w] = reinterpret_cast<const uint32_t*>(src)[w];
    } else {
        const uint8_t* a = keys + static_cast<int64_t>(boneCount * f0) * 0x1c;
        const uint8_t* b = keys + static_cast<int64_t>(boneCount * f1) * 0x1c;
        uint8_t* dst = out;
        for (int32_t i = 0; i < boneCount; ++i, a += 0x1c, b += 0x1c, dst += 0x1c) {
            const float ax = *reinterpret_cast<const float*>(a + 0x10);
            const float ay = *reinterpret_cast<const float*>(a + 0x14);
            const float az = *reinterpret_cast<const float*>(a + 0x18);
            const float bx = *reinterpret_cast<const float*>(b + 0x10);
            const float by = *reinterpret_cast<const float*>(b + 0x14);
            const float bz = *reinterpret_cast<const float*>(b + 0x18);

            // Math::Quaternion::Lerp (0x18001d500) by value; `q` is the slot.
            float q[4];
            *reinterpret_cast<Math::Quaternion*>(q) =
                reinterpret_cast<const Math::Quaternion*>(a)->Lerp(
                    *reinterpret_cast<const Math::Quaternion*>(b),
                    *reinterpret_cast<const Math::Scalar*>(&frac));
            const float* r = q;
            reinterpret_cast<uint32_t*>(dst)[0] = reinterpret_cast<const uint32_t*>(r)[0];
            reinterpret_cast<uint32_t*>(dst)[1] = reinterpret_cast<const uint32_t*>(r)[1];
            reinterpret_cast<uint32_t*>(dst)[2] = reinterpret_cast<const uint32_t*>(r)[2];
            reinterpret_cast<uint32_t*>(dst)[3] = reinterpret_cast<const uint32_t*>(r)[3];

            *reinterpret_cast<float*>(dst + 0x10) = ax + (bx - ax) * frac;
            *reinterpret_cast<float*>(dst + 0x14) = ay + (by - ay) * frac;
            *reinterpret_cast<float*>(dst + 0x18) = az + (bz - az) * frac;
        }
    }

    const float scale = static_cast<float>(static_cast<double>(*delta) * SecondsPerTick()) * rate;
    *reinterpret_cast<uint32_t*>(out + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(out + 0x04) = 0;
    *reinterpret_cast<uint32_t*>(out + 0x08) = 0;
    *reinterpret_cast<float*>(out + 0x0c) = 1.0f;              // DAT_18016169c
    *reinterpret_cast<float*>(out + 0x10) *= scale;
    *reinterpret_cast<float*>(out + 0x14) *= scale;
    *reinterpret_cast<float*>(out + 0x18) *= scale;
}

// ---------------------------------------------------------------------------
// ENDGAME B.3.5 step 5, block G - `Graphics::GraphicsMesh::Node`'s copy
// constructor, the ONE `GraphicsMesh` fragment in this file (B35_FILEMAP.md:97).
//   ??0Node@GraphicsMesh@Graphics@@QEAA@AEBU012@@Z      retail 0x1800127f0
//
// The name slot is stored ZERO FIRST and only then given the source's pointer:
// that is what makes this safe to run into raw `MemAlloc` memory, and it is why
// retail's `Array<Node>::operator=` calls it directly on a buffer it has just
// allocated. A member-initialiser list would do the same thing, but the two
// stores are written out in retail's order so the sequence stays visible.
//
// THE THREE ASSIGNMENTS ARE RETAIL'S FIFTEEN-DWORD FLAT COPY, exactly:
// `_parent` (4) + `_local` (0x1c) + `_bind` (0x1c) = 0x3c bytes spanning
// +0x08..+0x43, which is `for (off = 0x08; off <= 0x40; off += 4)`. `_pad44` is
// NOT assigned, which is retail's own behaviour and not an omission.
// ---------------------------------------------------------------------------
GraphicsMesh::Node::Node(const Node& src) {
    _name.data = nullptr;
    if (src._name.data != nullptr) {
        _name.data = src._name.data;
        ++RtSystem::StringRefCount(_name.data);   // System::String refcount at str-4
    }
    _parent = src._parent;
    _local  = src._local;
    _bind   = src._bind;
}

}  // namespace Graphics

// ============================================================================
// Registration. Unconditional (DIRECTION.md §2: a port registers, no flag).
// ============================================================================
int RtCollectionGraphicsAnimRegister();

int RtGraphicsAnimationStateRegister() {
    int n = RtCollectionGraphicsAnimRegister();

    return n;
}
