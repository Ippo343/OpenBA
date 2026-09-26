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

void* BaseVTable()         { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()     { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* PickingMeshVTable()  { return RT_DATA_SLOT(void*, "??_7PickingMesh@Picking@@6B@"); }
const uint8_t* PickingMeshTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@PickingMesh@Picking@@0VBaseTypeInfo@Object@@A");
}

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

// The stores retail inlines into Construct (0x180071d40) and Reconstruct
// (0x180071cd0): +0x20/+0x28 as qwords, +0x30 as ONE byte, +0x38/+0x40 as
// qwords (the last covers _size and _capacity together).
void InlineCtorStores(Picking::PickingMesh* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->_path.data      = nullptr;
    self->_refs._data     = nullptr;
    self->_refs._size     = 0;
    self->_refs._capacity = 0;
    *vp = PickingMeshVTable();
    self->_string20.data = nullptr;
    self->_string28.data = nullptr;
    self->_flag30 = false;
    self->_nodes._data = nullptr;
    self->_nodes._size = 0;
    self->_nodes._capacity = 0;
}

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

// [2] 0x180071a90: `MOV EAX,[_typeInfo] ; RET`.
uint32_t PickingMesh::Type() const {
    uint32_t v = 0; std::memcpy(&v, PickingMeshTypeInfo(), sizeof v); return v;
}
// [1] 0x180071ab0: the same node at +8.
const char* PickingMesh::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, PickingMeshTypeInfo() + 8, sizeof v); return v;
}
// [3] 0x180122940: `MOV EAX,9 ; RET`.
int32_t PickingMesh::GetFileVersion() const { return 9; }
// [4] 0x180122950: returns 1.
Object::External::CompileResult PickingMesh::Compile(const System::Filename&) { return 1; }

// [8] 0x180071d40 — retail's null test is in the DOOR.
Object::External* PickingMesh::Construct() {
    InlineCtorStores(this);
    return this;
}
// [6] 0x180071cd0: slot0(this, 0) dispatched VIRTUALLY, then the same stores.
Object::External* PickingMesh::Reconstruct() {
    CallSlot0(this, 0);
    InlineCtorStores(this);
    return this;
}
// [7] 0x180071db0: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void PickingMesh::Destruct() { CallSlot0(this, 0); }

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

// ~PickingMesh: NON-VIRTUAL and EMPTY; the registered dtor door does retail's work.
PickingMesh::~PickingMesh() {}

}  // namespace Picking

// ============================================================================
// THE DOORS — qualified delegation (a live PickingMesh wears the MINTED table).
// ============================================================================
extern "C" uint32_t RtPicking_PickingMesh_Type(const void* selfv) {
    return reinterpret_cast<const Picking::PickingMesh*>(selfv)->Picking::PickingMesh::Type();           // B.3.7 bridge
}
extern "C" const char* RtPicking_PickingMesh_TypeName(const void* selfv) {
    return reinterpret_cast<const Picking::PickingMesh*>(selfv)->Picking::PickingMesh::TypeName();       // B.3.7 bridge
}
extern "C" int32_t RtPicking_PickingMesh_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const Picking::PickingMesh*>(selfv)->Picking::PickingMesh::GetFileVersion(); // B.3.7 bridge
}
extern "C" int32_t RtPicking_PickingMesh_Compile(void* selfv, const void* pathv) {
    return static_cast<int32_t>(reinterpret_cast<Picking::PickingMesh*>(selfv)                            // B.3.7 bridge
        ->Picking::PickingMesh::Compile(*static_cast<const System::Filename*>(pathv)));
}
extern "C" void* RtPicking_PickingMesh_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                 // retail's null test
    return reinterpret_cast<Picking::PickingMesh*>(selfv)->Picking::PickingMesh::Construct();          // B.3.7 bridge
}
extern "C" void* RtPicking_PickingMesh_Reconstruct(void* selfv) {
    return reinterpret_cast<Picking::PickingMesh*>(selfv)->Picking::PickingMesh::Reconstruct();        // B.3.7 bridge
}
extern "C" void RtPicking_PickingMesh_Destruct(void* selfv) {
    reinterpret_cast<Picking::PickingMesh*>(selfv)->Picking::PickingMesh::Destruct();                  // B.3.7 bridge
}
extern "C" bool RtPicking_PickingMesh_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<Picking::PickingMesh*>(selfv)->Picking::PickingMesh::Serialize(streamv);   // B.3.7 bridge
}

// ============================================================================
// ??1PickingMesh@Picking@@UEAA@XZ (0x1801228a0) — the dtor DOOR keeps its
// body; the member destructor is empty (PickingMesh.h).
// ============================================================================
extern "C" void RtPicking_PickingMesh_Dtor(void* selfv) {
    Picking::PickingMesh* self = reinterpret_cast<Picking::PickingMesh*>(selfv);   // B.3.7 bridge
    *reinterpret_cast<void**>(self) = PickingMeshVTable();
    // Retail CALLs 0x180073720 here (at 0x1801228c4): the element destroy
    // 0x180073ee0 over every SubMesh -- four inner buffers DESCENDING from
    // +0x70 and then the String at +0x00 -- followed by the plain `~Array`.
    SubMeshArrayDtor(&self->_nodes);
    RtSystem::RtString_ReleaseInline(&self->_string28);
    RtSystem::RtString_ReleaseInline(&self->_string20);
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

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
    RegisterRuntimeExport("?Type@PickingMesh@Picking@@UEBAIXZ",           (void*)&RtPicking_PickingMesh_Type);           ++n;
    RegisterRuntimeAddr  (0x180071a90ull,                                (void*)&RtPicking_PickingMesh_Type);
    RegisterRuntimeExport("?TypeName@PickingMesh@Picking@@UEBAPEBDXZ",    (void*)&RtPicking_PickingMesh_TypeName);       ++n;
    RegisterRuntimeAddr  (0x180071ab0ull,                                (void*)&RtPicking_PickingMesh_TypeName);
    RegisterRuntimeExport("?GetFileVersion@PickingMesh@Picking@@UEBAHXZ", (void*)&RtPicking_PickingMesh_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x180122940ull,                                (void*)&RtPicking_PickingMesh_GetFileVersion);
    RegisterRuntimeExport("?Compile@PickingMesh@Picking@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
                          (void*)&RtPicking_PickingMesh_Compile); ++n;
    RegisterRuntimeAddr  (0x180122950ull,                                (void*)&RtPicking_PickingMesh_Compile);
    RegisterRuntimeExport("?Construct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtPicking_PickingMesh_Construct);   ++n;
    RegisterRuntimeAddr  (0x180071d40ull,                                                  (void*)&RtPicking_PickingMesh_Construct);
    RegisterRuntimeExport("?Reconstruct@PickingMesh@Picking@@UEAAPEAVExternal@Object@@XZ", (void*)&RtPicking_PickingMesh_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180071cd0ull,                                                  (void*)&RtPicking_PickingMesh_Reconstruct);
    RegisterRuntimeExport("?Destruct@PickingMesh@Picking@@UEAAXXZ",       (void*)&RtPicking_PickingMesh_Destruct);       ++n;
    RegisterRuntimeAddr  (0x180071db0ull,                                (void*)&RtPicking_PickingMesh_Destruct);
    RegisterRuntimeExport("??1PickingMesh@Picking@@UEAA@XZ",              (void*)&RtPicking_PickingMesh_Dtor);           ++n;
    RegisterRuntimeAddr  (0x1801228a0ull,                                (void*)&RtPicking_PickingMesh_Dtor);
    RegisterRuntimeExport("?Serialize@PickingMesh@Picking@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtPicking_PickingMesh_Serialize); ++n;
    RegisterRuntimeAddr  (0x180122960ull,                                                  (void*)&RtPicking_PickingMesh_Serialize);
    // ---- CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12) --------------
    // The two out-of-line halves of Serialize. Unexported in both builds, so
    // RegisterRuntimeAddr only. Their Array callees (0x18007a4d0, 0x18007a790,
    // 0x18007a900, 0x18007aa40, 0x180073720, 0x180073ee0) are registered by
    // runtime/src/collection/array{,_destroy}.cpp in the same commit.
    RegisterRuntimeAddr  (0x18007a350ull, (void*)&RtPicking_PickingMesh_SerializeTriangles);
    RegisterRuntimeAddr  (0x18007ac20ull, (void*)&RtPicking_PickingMesh_SerializeSubMeshes);
    return n;
}
