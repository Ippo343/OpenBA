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

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/collection/Collection.h"
#include "../../types/graphics/AnimationManager.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
namespace rw_vtbl = rt::io::rw_vtbl;

using RtGraphics::AnimInstanceRec;
using RtGraphics::AnimStateRec;
using RtGraphics::AnimationManagerView;
namespace anim_clip = RtGraphics::anim_clip;

namespace {

using ArrayHdr = Collection::Array<uint8_t>;

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// ---------------------------------------------------------------------------
// The neighbours these bodies call.
// ---------------------------------------------------------------------------

// The four Collection::Array<T> instantiations, by address — they have no name
// in either shipped build. Each goes through ResolveAddr so that our own port
// in array_graphics_anim.cpp is what answers (ResolveAddr consults the port
// registry first) and so tools/runtime_census.py sees the edge.
using SetSizeFn   = void  (*)(void* a, int n);
using ArrayAssign = void* (*)(void* dst, const void* src);

SetSizeFn SetSizeNodes()   { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005aa30); return f; }
SetSizeFn SetSizeStates()  { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005b1e0); return f; }
SetSizeFn SetSizeLayers()  { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005c8a0); return f; }
SetSizeFn SetSizeInterp()  { static SetSizeFn f = RtAddr<SetSizeFn>(0x18005ac50); return f; }
SetSizeFn SetSizePose()    { static SetSizeFn f = RtAddr<SetSizeFn>(0x180059140); return f; }

ArrayAssign AssignNodes()  { static ArrayAssign f = RtAddr<ArrayAssign>(0x180013960); return f; }
ArrayAssign AssignStates() { static ArrayAssign f = RtAddr<ArrayAssign>(0x18005b130); return f; }
ArrayAssign AssignLayers() { static ArrayAssign f = RtAddr<ArrayAssign>(0x18001b700); return f; }
ArrayAssign AssignInterp() { static ArrayAssign f = RtAddr<ArrayAssign>(0x18001b810); return f; }
// Array<Math::Transform>::operator=, retail 0x18001b360. NOT in this batch and
// not ported: it is the only element operation of AnimationState's copy ctor
// that has no other caller in the cluster, so it stays on the DLL until a batch
// reaches it. Named here rather than left as a bare number.
ArrayAssign AssignPose()   { static ArrayAssign f = RtAddr<ArrayAssign>(0x18001b360); return f; }

// Object::ExternalPtr<Graphics::AnimationGroup>::Serialize, retail 0x18005b840
// — already ported, in runtime/src/object/external_ptr_serialize.cpp. Reached
// by address because that is where it is registered and it has no export name.
using ExtPtrSerFn = void (*)(void* slot, Serializer* ser);
ExtPtrSerFn SerializeGroupPtr() {
    static ExtPtrSerFn f = RtAddr<ExtPtrSerFn>(0x18005b840);
    return f;
}

// Math::Quaternion::Lerp — HIDDEN RETURN SLOT in RDX. See the banner.
using QuatLerpFn = void* (*)(const void* from, void* ret, const void* to, const float* t);
QuatLerpFn QuaternionLerp() { static QuatLerpFn f = RtAddr<QuatLerpFn>(0x18001d500); return f; }

// ?_singleton@AnimationManager@Graphics@@0PEAV12@EA — the slot at 0x1801fe7b0.
// ONE deref of the slot gives the manager; the manager's +0x00 is the instance
// buffer. `MOV RAX,[0x1801fe7b0]` / `ADD RBX,[RAX]` at 0x1800fd97a is the pair.
AnimationManagerView* AnimationManagerSingleton() {
    AnimationManagerView** slot =
        RT_DATA_SLOT(AnimationManagerView**, "?_singleton@AnimationManager@Graphics@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ?_secondsPerTick@Timer@System@@0NA — a DOUBLE. Every tick<->seconds
// conversion in this file goes through it, exactly as retail's inlined
// `CVTSI2SD / MULSD [rip+_secondsPerTick]` does.
double SecondsPerTick() {
    double* p = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
}

// ---------------------------------------------------------------------------
// ReaderWriter call shapes. Two, and only two, appear in this cluster.
// ---------------------------------------------------------------------------
using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
using ArrayFn  = void (*)(ReaderWriter*, int, void*, const char*);

void RwScalar(ReaderWriter* rw, int slot, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[slot / 8])(rw, p, nullptr);
}
void RwFloats(ReaderWriter* rw, int count, void* p) {
    reinterpret_cast<ArrayFn>(rw->vtable[rw_vtbl::kArrayFloat / 8])(rw, count, p, nullptr);
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
// *** +0x45..+0x47 IS NOT COPIED. *** The last store is a dword at +0x40; the
// record is 0x48 bytes. Zeroing the tail here would be unfaithful and would
// make a byte comparator fail on bytes neither side writes.
extern "C" void* RtGraphics_MeshNode_CopyCtor(void* dstv, const void* srcv) {
    uint8_t* d = static_cast<uint8_t*>(dstv);
    const uint8_t* s = static_cast<const uint8_t*>(srcv);

    *reinterpret_cast<void**>(d) = nullptr;
    uint8_t* name = *reinterpret_cast<uint8_t* const*>(s);
    if (name != nullptr) {
        *reinterpret_cast<uint8_t**>(d) = name;
        ++*reinterpret_cast<int32_t*>(name - 4);       // System::String refcount at str-4
    }
    for (int off = 0x08; off <= 0x40; off += 4)
        *reinterpret_cast<uint32_t*>(d + off) = *reinterpret_cast<const uint32_t*>(s + off);
    return d;
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
    AssignLayers()(d + 0x08, s + 0x08);                            // CALL 0x18001b700

    *reinterpret_cast<uint32_t*>(d + 0x18) =                       // _rootOffset
        *reinterpret_cast<const uint32_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x1c) =
        *reinterpret_cast<const uint32_t*>(s + 0x1c);
    *reinterpret_cast<uint32_t*>(d + 0x20) =
        *reinterpret_cast<const uint32_t*>(s + 0x20);

    *reinterpret_cast<uint64_t*>(d + 0x28) = 0;                    // _pose
    *reinterpret_cast<uint64_t*>(d + 0x30) = 0;
    AssignPose()(d + 0x28, s + 0x28);                              // CALL 0x18001b360

    *reinterpret_cast<uint64_t*>(d + 0x38) = 0;                    // _nodes
    *reinterpret_cast<uint64_t*>(d + 0x40) = 0;
    AssignInterp()(d + 0x38, s + 0x38);                            // CALL 0x18001b810
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
    AssignNodes()(d + 0x10, s + 0x10);                             // CALL 0x180013960

    *reinterpret_cast<uint64_t*>(d + 0x20) = 0;                    // _states
    *reinterpret_cast<uint64_t*>(d + 0x28) = 0;
    AssignStates()(d + 0x20, s + 0x20);                            // CALL 0x18005b130
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
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(ser)) SetSizePose()(a, n);        // CALL 0x180059140

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
        RwScalar(rw, rw_vtbl::kScalarInt, p + 0x08);
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
        SerializeGroupPtr()(p + 0x00, ser);                  // ExternalPtr<AnimationGroup>
        RwScalar(rw, rw_vtbl::kScalarInt,   p + 0x08);       // clip index
        RwScalar(rw, rw_vtbl::kScalarFloat, p + 0x10);       // speed / weight
        RwScalar(rw, rw_vtbl::kScalarInt64, p + 0x18);       // time
        RwScalar(rw, rw_vtbl::kScalarInt64, p + 0x20);       // fade length
        RwScalar(rw, rw_vtbl::kScalarInt64, p + 0x28);       // fade remaining
        RwScalar(rw, rw_vtbl::kScalarInt,   p + 0x0c);       // packed instance handle
        RwScalar(rw, rw_vtbl::kScalarBool,  p + 0x30);       // restarted
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
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser)) SetSizeLayers()(p + 0x08, n);            // CALL 0x18005c8a0
        RtGraphics_SerializeAnimLayers(ser, n, *reinterpret_cast<void* const*>(p + 0x08));

        RwScalar(rw, rw_vtbl::kScalarBool, p + 0x00);

        n = *reinterpret_cast<const int32_t*>(p + 0x40);             // _nodes._size
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser)) SetSizeInterp()(p + 0x38, n);            // CALL 0x18005ac50
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
        SerializeGroupPtr()(p + 0x00, ser);
        RwScalar(rw, rw_vtbl::kScalarInt, p + 0x08);                 // clip index
        RwScalar(rw, rw_vtbl::kScalarInt, p + 0x0c);                 // reference count

        int32_t n = *reinterpret_cast<const int32_t*>(p + 0x28);     // _states._size
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser)) SetSizeStates()(p + 0x20, n);            // CALL 0x18005b1e0
        RtGraphics_SerializeAnimStates(ser, n, *reinterpret_cast<void* const*>(p + 0x20));

        n = *reinterpret_cast<const int32_t*>(p + 0x18);             // _nodes._size
        RwScalar(rw, rw_vtbl::kScalarInt, &n);
        if (IsReading(ser)) SetSizeNodes()(p + 0x10, n);             // CALL 0x18005aa30
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
extern "C" void RtGraphics_AnimationState_Serialize(void* selfv, Serializer* ser) {
    AnimStateRec* self = static_cast<AnimStateRec*>(selfv);
    ReaderWriter* rw = ser->readerWriter;

    int32_t n = self->_layers._size;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(ser)) SetSizeLayers()(&self->_layers, n);
    RtGraphics_SerializeAnimLayers(ser, n, self->_layers._data);
    RwScalar(rw, rw_vtbl::kScalarBool, self);
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
extern "C" void RtGraphics_AnimationState_Reset(void* selfv) {
    AnimStateRec* self = static_cast<AnimStateRec*>(selfv);

    for (int32_t i = 0; i < self->_layers._size; ++i) {
        const int32_t handle =
            *reinterpret_cast<const int32_t*>(self->_layers._data + i * 0x38 + 0x0c);
        if (handle < 0) continue;                        // TEST ECX,ECX / JS

        AnimationManagerView* mgr = AnimationManagerSingleton();
        uint8_t* inst = mgr->_instances._data +
                        static_cast<int64_t>((handle >> 8) & 0xff) * 0x30;
        if (--*reinterpret_cast<int32_t*>(inst + 0x0c) != 0) continue;

        *reinterpret_cast<uint64_t*>(inst + 0x00) = 0;   // _group
        *reinterpret_cast<int32_t*>(inst + 0x08)  = -1;  // _animIndex
        SetSizeNodes()(inst + 0x10, 0);                  // CALL 0x18005aa30
        SetSizeStates()(inst + 0x20, 0);                 // CALL 0x18005b1e0
    }

    // The inlined ~Array. `MemFree` is null-guarded inside 0x1800c7ad0, which is
    // instruction for instruction what retail inlines here.
    using MemFreeFn = void (*)(void*);
    if (self->_layers._data != nullptr)
        RtAddr<MemFreeFn>(0x1800c7ad0)(self->_layers._data);
    self->_layers._data     = nullptr;
    self->_layers._size     = 0;
    self->_layers._capacity = 0;
}

// ============================================================================
// ?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z (0x1800fb8e0)
// ============================================================================
//
// Copy `count` baked Math::Transforms out of the cached instance the packed
// handle names. Unguarded on every step — a bad handle indexes off the end of
// the instance table — and that is retail's, so it stays that way.
extern "C" void RtGraphics_AnimationManager_GetPose(const void* selfv, int handle,
                                                    void* outv, int count) {
    const AnimationManagerView* self = static_cast<const AnimationManagerView*>(selfv);

    const uint8_t* inst = self->_instances._data +
                          static_cast<int64_t>((handle >> 8) & 0xff) * 0x30;
    const uint8_t* states = *reinterpret_cast<const uint8_t* const*>(inst + 0x20);
    const uint8_t* pose = *reinterpret_cast<const uint8_t* const*>(
        states + static_cast<int64_t>(handle & 0xff) * 0x48 + 0x28);

    uint8_t* out = static_cast<uint8_t*>(outv);
    for (int i = 0; i < count; ++i, out += 0x1c, pose += 0x1c)
        for (int w = 0; w < 7; ++w)
            reinterpret_cast<uint32_t*>(out)[w] = reinterpret_cast<const uint32_t*>(pose)[w];
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
extern "C" void RtGraphics_Animation_GetPoseAtTime(const void* selfv,
                                                   const int64_t* time,
                                                   const int64_t* delta,
                                                   void* outv, int /*unused*/) {
    const uint8_t* clip = static_cast<const uint8_t*>(selfv);
    uint8_t* out = static_cast<uint8_t*>(outv);

    const int32_t loopFrame  = *reinterpret_cast<const int32_t*>(clip + anim_clip::kLoopFrame);
    const int32_t frameFirst = *reinterpret_cast<const int32_t*>(clip + anim_clip::kFrameFirst);
    const int32_t frameLast  = *reinterpret_cast<const int32_t*>(clip + anim_clip::kFrameLast);
    const int32_t frameCount = *reinterpret_cast<const int32_t*>(clip + anim_clip::kFrameCount);
    const int32_t boneCount  = *reinterpret_cast<const int32_t*>(clip + anim_clip::kBoneCount);
    const uint8_t  looping   = clip[anim_clip::kLooping];
    const float    rate      = *reinterpret_cast<const float*>(clip + anim_clip::kFrameRate);
    const uint8_t* keys      = *reinterpret_cast<const uint8_t* const*>(clip + anim_clip::kKeys);

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

            float q[4];
            const float* r = static_cast<const float*>(QuaternionLerp()(a, q, b, &frac));
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

// ============================================================================
// Registration. Unconditional (DIRECTION.md §2: a port registers, no flag).
// ============================================================================
int RtCollectionGraphicsAnimRegister();

int RtGraphicsAnimationStateRegister() {
    int n = RtCollectionGraphicsAnimRegister();

    RegisterRuntimeExport("??0Node@GraphicsMesh@Graphics@@QEAA@AEBU012@@Z",
                          (void*)&RtGraphics_MeshNode_CopyCtor);                        ++n;
    RegisterRuntimeAddr  (0x1800127f0ull, (void*)&RtGraphics_MeshNode_CopyCtor);
    RegisterRuntimeAddr  (0x18005b660ull, (void*)&RtGraphics_AnimState_CopyCtor);       ++n;
    RegisterRuntimeAddr  (0x18005b700ull, (void*)&RtGraphics_AnimInstance_CopyCtor);    ++n;

    RegisterRuntimeAddr  (0x180059820ull, (void*)&RtGraphics_SerializeTransformArray);  ++n;
    RegisterRuntimeAddr  (0x18005c230ull, (void*)&RtGraphics_SerializeInterpTransforms);++n;
    RegisterRuntimeAddr  (0x18005c120ull, (void*)&RtGraphics_SerializeMeshNodes);       ++n;
    RegisterRuntimeAddr  (0x18005cb40ull, (void*)&RtGraphics_SerializeAnimLayers);      ++n;
    RegisterRuntimeAddr  (0x18005c2c0ull, (void*)&RtGraphics_SerializeAnimStates);      ++n;
    RegisterRuntimeAddr  (0x18005c660ull, (void*)&RtGraphics_SerializeAnimInstances);   ++n;

    RegisterRuntimeExport("?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtGraphics_AnimationState_Serialize);                 ++n;
    RegisterRuntimeAddr  (0x1800fe250ull, (void*)&RtGraphics_AnimationState_Serialize);
    RegisterRuntimeExport("?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ",
                          (void*)&RtGraphics_AnimationState_Reset);                     ++n;
    RegisterRuntimeAddr  (0x1800fd930ull, (void*)&RtGraphics_AnimationState_Reset);
    RegisterRuntimeExport("?GetPose@AnimationManager@Graphics@@QEBAXHPEAVTransform@Math@@H@Z",
                          (void*)&RtGraphics_AnimationManager_GetPose);                 ++n;
    RegisterRuntimeAddr  (0x1800fb8e0ull, (void*)&RtGraphics_AnimationManager_GetPose);
    RegisterRuntimeExport(
        "?GetPoseAtTime@Animation@Graphics@@QEBAXAEBVTime@System@@0PEAVTransform@Math@@H@Z",
        (void*)&RtGraphics_Animation_GetPoseAtTime);                                    ++n;
    RegisterRuntimeAddr  (0x1800faca0ull, (void*)&RtGraphics_Animation_GetPoseAtTime);
    return n;
}
