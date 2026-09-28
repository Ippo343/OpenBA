// ============================================================================
// runtime/src/picking/pickingmesh.cpp — Picking::PickingMesh, whole class
// ============================================================================
//
// Vtable batch 8. ??_7PickingMesh@Picking@@6B@ (retail), nine slots:
//
//   0  deleting dtor        0x180072050  deleting_dtor.cpp row (registers ~PickingMesh here)
//   1  TypeName             0x180071ab0  here
//   2  Type                 0x180071a90  here
//   3  GetFileVersion       0x180122940  here  (returns 9)
//   4  Compile              0x180122950  here  (returns 1)
//   5  Serialize            0x180122960  here
//   6  Reconstruct          0x180071cd0  here
//   7  Destruct             0x180071db0  here
//   8  Construct            0x180071d40  here
//
// plus ~PickingMesh (0x1801228a0), registered here and appended to
// runtime/src/object/deleting_dtor.cpp's kSpecs.
//
// PickingMesh itself (sizeof 0x48; Reconstruct/Construct zero +0x20/+0x28 as
// qwords, +0x30 as one byte, +0x38/+0x40 as qwords):
//   +0x00  Object::External           (0x20)
//   +0x20  System::String   _name1
//   +0x28  System::String   _name2
//   +0x30  bool             _flag        Serialize: kScalarBool
//   +0x38  SubMesh*         _nodes       (runtime/types/picking/Scene.h)
//   +0x40  int32_t          _nodeCount
// _nodes/_nodeCount were already pinned by Wave 5 lane D (scene.cpp); this
// port adds Serialize, which walks the SubMesh array and — in doing so —
// pins the LAST two fields Scene.h left opaque (_name at SubMesh+0x00,
// _uvs Array<UV16> at SubMesh+0x70). See the edit at the top of Scene.h.
//
// Serialize order (0x122960): _name1, _name2, _flag (kScalarBool), then the
// SubMesh array (count @ +0x40, resize FUN_18007aa40, stride 0x80).
//
// Per-SubMesh loop (FUN_18007ac20, disassembled -- Ghidra's decompile drops
// several call arguments here, same pitfall as CollisionMesh's Face loop):
//   _name (String::Serialize), _parent (kScalarInt),
//   _local.quaternion (kArrayFloat n=4), _local.translation (kArrayFloat n=3),
//   _aabb.min (kArrayFloat n=3), _aabb.max (kArrayFloat n=3),
//   _vertices (count, resize FUN_18007a4d0 stride 0xc align4, kArrayFloat n=3 per elem),
//   _normals   (same shape, same resize function, reused),
//   _triangles (count, resize FUN_18007a790 stride 0xc align2,
//               per elem: kArrayUShort(n=3) TWICE -- elem+0x00 and elem+0x06),
//   _uvs       (count, resize FUN_18007a900 stride 4 align2, kArrayUShort(n=2) per elem).
//
// ENDGAME B.3 step 7, block B6: `Picking::PickingMesh` is a real
// `Object::External` class now (runtime/types/picking/PickingMesh.h). The
// anonymous-namespace `PickingMesh` this file used to declare (full Array
// header, capacity at +0x44) and runtime/types/picking/Scene.h's POD (bare
// `SubMesh* _nodes; int32 _nodeCount`) FOLD into that one class (RULE B7-6).
// The slot bodies are its members; every registered door below is a one-line
// QUALIFIED delegation. The fields are named by offset (_string20, _string28,
// _flag30): no getter names them (B6-8).
//
// RULE B6-5: the four Array SetSize bodies and ~Array<SubMesh> are reached by
// ADDRESS (registry-first) through namespace-scope atomic slots — this class
// is deserialized on the LOADER thread, so the function-local first-call
// caches these used to keep are gone. The two unused by-address
// `MemAlloc`/`MemFree` helpers are deleted.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
// ResolveAddr, declared locally rather than via standalone/resolve.h -- that header
// pulls in <windows.h>, which collides with names this file (or a header it
// includes) uses -- see runtime/src/collection/array_internal.h for the same fix.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/system/String.h"
#include "../../types/picking/PickingMesh.h"
#include "../system/string.h"

namespace {

using RtSystem::String;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
using SubMesh  = Picking::PickingMesh::SubMesh;
using Triangle = Picking::PickingMesh::Triangle;
using UV16     = Picking::PickingMesh::UV16;

// ENDGAME B.3.9 PILOT: `InlineCtorStores` (Base / External / PickingMesh vptr
// stamps from the MINT plus the zero stores) and the file's `CallSlot0` are
// gone. The stores are the real constructor now (PickingMesh.h), the stamps
// the compiler's, and slot 0 is a virtual call on the class.

// ---- the Collection::Array<T> instantiations, by ADDRESS -------------------
//
// CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12). This file used to carry
// a private `GenericArraySetSize` template plus a hand-written
// `SubMeshArraySetSize`, written before the Array family was parameterised.
// THEY WERE NOT BYTE-FAITHFUL, and the divergences were not cosmetic:
//
//   * `GenericArraySetSize` zero-filled the new tail for EVERY element type.
//     Retail's 0x18007a4d0 (_vertices/_normals) does zero it; 0x18007a790
//     (_triangles) and 0x18007a900 (_uvs) have NO `else` arm at all and leave
//     the new tail holding whatever the allocation contained. A template
//     cannot carry "which arm exists" per instantiation.
//   * `SubMeshArraySetSize` did a SHALLOW struct copy on the grow arm where
//     retail CALLs the copy ctor 0x1800734a0 -- which adopts the element's
//     System::String with a refcount bump and runs `operator=` on all four
//     inner Arrays, allocating fresh buffers.
//   * It then skipped retail's per-element destroy on BOTH arms: 0x18007aa40
//     CALLs 0x180073ee0 on the shrink tail AND on the whole old buffer before
//     freeing it.
//
// Each body is ported ONCE, in runtime/src/collection/array{,_destroy}.cpp with
// the rest of its family, and reached here by address -- registry-first, so
// these call OUR bodies from the moment RtCollectionPortInit runs.
//
// RULE B6-5: one namespace-scope atomic slot per address; a miss is retried,
// a null is never cached (the element_common.h `AtomicAddr` contract).
void* AtomicAddr(std::atomic<void*>& slot, uintptr_t ghidra) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        p = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, ghidra));
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_vecSetSize{nullptr};      // 0x18007a4d0
std::atomic<void*> g_triSetSize{nullptr};      // 0x18007a790
std::atomic<void*> g_uvSetSize{nullptr};       // 0x18007a900
std::atomic<void*> g_subMeshSetSize{nullptr};  // 0x18007aa40
std::atomic<void*> g_subMeshDtor{nullptr};     // 0x180073720

using ArraySetSizeFn = void (*)(void*, int32_t);

// 0x18007a4d0  Array<Vector3>::SetSize   (_vertices AND _normals: retail shares
//              one body, stride 0xc, align 4, tail zeroed)
void VecArraySetSize(Collection::Array<Vector3>* a, int32_t n) {
    reinterpret_cast<ArraySetSizeFn>(AtomicAddr(g_vecSetSize, 0x18007a4d0ull))(a, n);
}
// 0x18007a790  Array<Triangle>::SetSize  (stride 0xc, align 2, tail UNTOUCHED)
void TriArraySetSize(Collection::Array<Triangle>* a, int32_t n) {
    reinterpret_cast<ArraySetSizeFn>(AtomicAddr(g_triSetSize, 0x18007a790ull))(a, n);
}
// 0x18007a900  Array<UV16>::SetSize      (stride 4, align 2, tail UNTOUCHED)
void UvArraySetSize(Collection::Array<UV16>* a, int32_t n) {
    reinterpret_cast<ArraySetSizeFn>(AtomicAddr(g_uvSetSize, 0x18007a900ull))(a, n);
}
// 0x18007aa40  Array<SubMesh>::SetSize. NON-TRIVIAL: element copy ctor
// 0x1800734a0, per-element destroy 0x180073ee0 on the shrink tail and on the
// abandoned buffer.
void SubMeshArraySetSize(Collection::Array<SubMesh>* a, int32_t n) {
    reinterpret_cast<ArraySetSizeFn>(AtomicAddr(g_subMeshSetSize, 0x18007aa40ull))(a, n);
}
// 0x180073720  ~Array<SubMesh>: the element destroy 0x180073ee0, then the
// plain `~Array`. Retail's ~PickingMesh CALLs exactly this at 0x1801228c4.
void SubMeshArrayDtor(Collection::Array<SubMesh>* a) {
    using Fn = void* (*)(void*);
    reinterpret_cast<Fn>(AtomicAddr(g_subMeshDtor, 0x180073720ull))(a);
}

// ---- the two Serialize halves retail keeps out of line --------------------
//
// 0x18007a350(Array<Triangle>*, Serializer*) -- the triangle sub-walk. Retail
// reads the count out of the array header (`[RCX+8]`), serializes it with
// kScalarInt, grows the array when reading, and only then walks TWO
// kArrayUShort(n=3) per element, at +0x00 and +0x06. Its argument order is
// (RCX = the Array, RDX = the Serializer) -- the OPPOSITE of 0x18007ac20's.
void SerializeTrianglesBody(Collection::Array<Triangle>* arr, Serializer* ser) {
    using ScalarIntFn   = void (*)(ReaderWriter*, int32_t*, const char*);
    using ArrayUShortFn = void (*)(ReaderWriter*, int32_t, void*, const char*);
    ReaderWriter* rw = ser->readerWriter;
    int32_t count = arr->_size;
    reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &count, nullptr);
    if (ser->stream->direction == 0) TriArraySetSize(arr, count);
    auto arrayUShort = reinterpret_cast<ArrayUShortFn>(
        rw->vtable[rt::io::rw_vtbl::kArrayUShort / 8]);
    for (int32_t i = 0; i < count; ++i) {
        arrayUShort(rw, 3, &arr->_data[i].i0, nullptr);
        arrayUShort(rw, 3, &arr->_data[i].j0, nullptr);
    }
}

// 0x18007ac20(Serializer*, int count, SubMesh* base) -- the per-SubMesh walk.
// Read from the raw disassembly rather than the pseudo-C, which drops the
// third argument on most of these indirect calls.
void SerializeSubMeshesBody(Serializer* ser, int32_t count, SubMesh* base) {
    if (count <= 0) return;
    using ScalarIntFn   = void (*)(ReaderWriter*, int32_t*, const char*);
    using ArrayFloatFn  = void (*)(ReaderWriter*, int32_t, void*, const char*);
    using ArrayUShortFn = void (*)(ReaderWriter*, int32_t, void*, const char*);
    ReaderWriter* rw = ser->readerWriter;
    auto scalarInt   = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    auto arrayFloat  = reinterpret_cast<ArrayFloatFn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8]);
    auto arrayUShort = reinterpret_cast<ArrayUShortFn>(rw->vtable[rt::io::rw_vtbl::kArrayUShort / 8]);
    const bool reading = (ser->stream->direction == 0);

    for (int32_t i = 0; i < count; ++i) {
        SubMesh* s = &base[i];
        // B.3.1 bridge: SubMesh::_name is a raw slot (PickingMesh.h says why).
        reinterpret_cast<System::String&>(s->_name).Serialize(ser);
        scalarInt(rw, &s->_parent, nullptr);
        arrayFloat(rw, 4, &s->_local.rotation, nullptr);
        arrayFloat(rw, 3, &s->_local.translation, nullptr);
        arrayFloat(rw, 3, &s->_aabb.min, nullptr);
        arrayFloat(rw, 3, &s->_aabb.max, nullptr);

        int32_t vCount = s->_vertices._size;
        scalarInt(rw, &vCount, nullptr);
        if (reading) VecArraySetSize(&s->_vertices, vCount);
        for (int32_t v = 0; v < vCount; ++v) arrayFloat(rw, 3, &s->_vertices._data[v], nullptr);

        int32_t nCount = s->_normals._size;
        scalarInt(rw, &nCount, nullptr);
        if (reading) VecArraySetSize(&s->_normals, nCount);
        for (int32_t v = 0; v < nCount; ++v) arrayFloat(rw, 3, &s->_normals._data[v], nullptr);

        SerializeTrianglesBody(&s->_triangles, ser);

        int32_t uvCount = s->_uvs._size;
        scalarInt(rw, &uvCount, nullptr);
        if (reading) UvArraySetSize(&s->_uvs, uvCount);
        for (int32_t v = 0; v < uvCount; ++v) arrayUShort(rw, 2, &s->_uvs._data[v], nullptr);
    }
}

}  // namespace

// ############################################################################
// Picking::PickingMesh's member bodies (ENDGAME B.3.7 block B6).
// ############################################################################
namespace Picking {

// [1] 0x180071ab0 / [2] 0x180071a90 / TypeId 0x180071aa0: DECLARE_TYPE's
// `MOV RAX,[_typeInfo+8]` / `MOV EAX,[_typeInfo]` over the class's own record
// (picking_type_records.cpp; ENDGAME B.3.9 TI-REST).
// [3] 0x180122940: `MOV EAX,9 ; RET`.
int32_t PickingMesh::GetFileVersion() const { return 9; }
// [4] 0x180122950: returns 1.
Object::External::CompileResult PickingMesh::Compile(const System::Filename&) { return 1; }

// [8] 0x180071d40 — the inlined constructor, i.e. placement construction.
// (Retail's `TEST RCX,RCX` null test sat in the retired door; a virtual call
// always has a `this`.) `::new`: the class declares its own placement new.
Object::External* PickingMesh::Construct() {
    return ::new (this) PickingMesh();
}
// [6] 0x180071cd0: slot0(this, 0) dispatched VIRTUALLY, then the same stores.
Object::External* PickingMesh::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) PickingMesh();
}
// [7] 0x180071db0: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void PickingMesh::Destruct() { DeletingDtor(0); }

// ?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ  0x180071ba0
// The FACTREST row this replaces (rt_factories_picking.inc, 0x180071ba0..c3f):
// `?MemAlloc@MemoryAllocator@System@@` (0x1800c7a50, the LOCKED wrapper --
// the same function Object::detail::InternalMemAlloc reaches) of 0x48 bytes,
// align 8; retail's null branch; then the inlined constructor.
Object::Base* PickingMesh::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PickingMesh), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) PickingMesh();
}
// ?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z  0x180071c60
// retail's null test, then the same inlined constructor.
Object::Base* PickingMesh::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) PickingMesh();
}

// ============================================================================
// [5] ?Serialize@PickingMesh@Picking@@UEAA_NAEAVStream@IO@@@Z  0x180122960
// ============================================================================
// Order: _string20, _string28, _flag30 (kScalarBool), then the SubMesh array
// (count @ +0x40, resize 0x18007aa40, walk 0x18007ac20).
bool PickingMesh::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    // B.3.1 bridge: the two Strings are the POD spelling (PickingMesh.h).
    reinterpret_cast<System::String&>(_string20).Serialize(&ser);
    reinterpret_cast<System::String&>(_string28).Serialize(&ser);

    using ScalarBoolFn   = void (*)(ReaderWriter*, bool*, const char*);
    using ScalarIntFn    = void (*)(ReaderWriter*, int32_t*, const char*);
    auto scalarBool   = reinterpret_cast<ScalarBoolFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8]);
    auto scalarInt    = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    const bool reading = (stream->direction == 0);

    scalarBool(rw, &_flag30, nullptr);

    int32_t nodeCount = _nodes._size;
    scalarInt(rw, &nodeCount, nullptr);
    if (reading) SubMeshArraySetSize(&_nodes, nodeCount);

    // Retail CALLs 0x18007ac20 here rather than inlining the walk; so does
    // this, through the ported body (batch D2, 2026-09-12).
    SerializeSubMeshesBody(&ser, nodeCount, _nodes._data);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

// ??1PickingMesh@Picking@@UEAA@XZ  0x1801228a0 — retail's body, moved here
// from the retired `RtPicking_PickingMesh_Dtor` door (ENDGAME B.3.9 PILOT).
// The entry restamp is the compiler's; so is the tail into ~External. Retail
// CALLs 0x180073720 at 0x1801228c4: the element destroy 0x180073ee0 over every
// SubMesh -- four inner buffers DESCENDING from +0x70, then the String at
// +0x00 -- followed by the plain `~Array`, which frees and ZEROES the header.
// The compiler-emitted `_nodes.~Array()` that runs after this body therefore
// sees an empty header: no element destroy (`_size` 0) and MemFree(null),
// which retail's allocator tests inside (array.cpp ArrayDtorImpl) -- a no-op.
PickingMesh::~PickingMesh() {
    SubMeshArrayDtor(&_nodes);
    RtSystem::RtString_ReleaseInline(&_string28);
    RtSystem::RtString_ReleaseInline(&_string20);
}

}  // namespace Picking

// ============================================================================
// ENDGAME B.3.9 PILOT: the eight slot doors (`RtPicking_PickingMesh_Type` ..
// `_Serialize`) and the `??1PickingMesh` door are gone. Their only reader was
// the minted `??_7PickingMesh@Picking@@6B@` (and, for the ??1 door, its
// RT_DELDTOR row): every live PickingMesh now wears the compiler's table.
// ============================================================================

// ---- CUTOVER CLOSURE BATCH D2: the two registered Serialize halves --------
// Retail's own signatures, so these can stand at their addresses for any
// caller. Note the argument ORDER differs between them: 0x18007a350 takes the
// Array first, 0x18007ac20 the Serializer.
extern "C" void RtPicking_PickingMesh_SerializeTriangles(void* arr, void* ser) {
    SerializeTrianglesBody(static_cast<Collection::Array<Triangle>*>(arr),
                           static_cast<Serializer*>(ser));
}
extern "C" void RtPicking_PickingMesh_SerializeSubMeshes(void* ser, int32_t count, void* base) {
    SerializeSubMeshesBody(static_cast<Serializer*>(ser), count, static_cast<SubMesh*>(base));
}

int RtPickingPickingMeshRegister() {
    int n = 0;
    // ---- the factory pair (B39_RULES D2): the class's OWN statics, both
    // doors, replacing the FACTREST row. The class's own `DEFINE_TYPE` record
    // (picking_type_records.cpp) carries both as its factoryFn /
    // inPlaceFactoryFn, so `Object::Factory` -- External::Load's
    // create-by-type-id -- builds a real PickingMesh.
    RegisterRuntimeExport("?CreateNewObject@PickingMesh@Picking@@SAPEAVBase@Object@@XZ",
                          (void*)&Picking::PickingMesh::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x180071ba0ull, (void*)&Picking::PickingMesh::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@PickingMesh@Picking@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&Picking::PickingMesh::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x180071c60ull, (void*)&Picking::PickingMesh::CreateInPlaceObject);
    // ---- CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12) --------------
    // The two out-of-line halves of Serialize. Unexported in both builds, so
    // RegisterRuntimeAddr only. Their Array callees (0x18007a4d0, 0x18007a790,
    // 0x18007a900, 0x18007aa40, 0x180073720, 0x180073ee0) are registered by
    // runtime/src/collection/array{,_destroy}.cpp in the same commit.
    RegisterRuntimeAddr  (0x18007a350ull, (void*)&RtPicking_PickingMesh_SerializeTriangles);
    RegisterRuntimeAddr  (0x18007ac20ull, (void*)&RtPicking_PickingMesh_SerializeSubMeshes);
    return n;
}
