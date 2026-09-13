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
// ============================================================================
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
#include "../../types/picking/Scene.h"
#include "../system/string.h"

namespace {

using RtObject::External;
using RtSystem::String;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
using RtPicking::SubMesh;
using RtPicking::Triangle;
using RtPicking::UV16;

// +0x38 is a FULL Collection::Array<SubMesh> header (data/size/capacity), not
// a bare pointer+count: FUN_18007aa40 (the resize this class's Serialize
// calls) is the same SetSize shape as every other Collection::Array<T> this
// port has transcribed, reading/writing `param_1[1]` (size) and
// `*(int*)(param_1+0xc)` (capacity) -- and Construct/Reconstruct zero
// `this+0x40` as ONE qword, which is exactly "zero an Array's size+capacity
// together". runtime/types/picking/Scene.h's PickingMesh (Wave 5 lane D) reads
// the same two fields as a bare `SubMesh* _nodes; int32_t _nodeCount;` because
// that lane never needed the capacity int32 at +0x44 -- byte-compatible with
// this, just one field short.
struct PickingMesh {
    External                    base;    // +0x00
    void*                       _name1;  // +0x20
    void*                       _name2;  // +0x28
    bool                        _flag;   // +0x30
    uint8_t                     _pad31[7];  // +0x31
    Collection::Array<SubMesh>  _nodes;  // +0x38  data, size@0x40, cap@0x44
};
static_assert(sizeof(PickingMesh) == 0x48, "Picking::PickingMesh is 0x48 bytes");
static_assert(offsetof(PickingMesh, _name1) == 0x20, "+0x20");
static_assert(offsetof(PickingMesh, _name2) == 0x28, "+0x28");
static_assert(offsetof(PickingMesh, _flag)  == 0x30, "+0x30");
static_assert(offsetof(PickingMesh, _nodes) == 0x38, "+0x38");

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

void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50ull));
    return fn(bytes, align);
}
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0ull));
    fn(p);
}

void ConstructBody(PickingMesh* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = PickingMeshVTable();
    self->_name1 = nullptr;
    self->_name2 = nullptr;
    self->_flag = false;
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
//   * It then skipped retail's per-element destroy on BOTH arms. The comment
//     it carried -- "retail's shrink-tail path here does not free the
//     truncated SubMeshes' nested arrays (FUN_18007aa40 has no such call on
//     the reused-buffer arm)" -- IS WRONG, and is corrected here at its source
//     per PLAN.md section 6 step 8: 0x18007aa40 CALLs 0x180073ee0 on the
//     shrink tail AND on the whole old buffer before freeing it. Together with
//     the shallow copy that made the old body a double-owner on grow and a
//     leak on shrink.
//
// Those copies are gone. Each body is ported ONCE, in
// runtime/src/collection/array{,_destroy}.cpp with the rest of its family, and
// reached here by address -- registry-first, so these call OUR bodies from the
// moment RtCollectionPortInit runs, and at cutover they are all that exists.
template <typename Fn>
static Fn RtAddrFn(uintptr_t a) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, a)); }

using ArraySetSizeFn = void (*)(void*, int32_t);

// 0x18007a4d0  Array<Vector3>::SetSize   (_vertices AND _normals: retail shares
//              one body, stride 0xc, align 4, tail zeroed)
void VecArraySetSize(Collection::Array<Vector3>* a, int32_t n) {
    static ArraySetSizeFn fn = nullptr;
    if (!fn) fn = RtAddrFn<ArraySetSizeFn>(0x18007a4d0ull);
    fn(a, n);
}
// 0x18007a790  Array<Triangle>::SetSize  (stride 0xc, align 2, tail UNTOUCHED)
void TriArraySetSize(Collection::Array<Triangle>* a, int32_t n) {
    static ArraySetSizeFn fn = nullptr;
    if (!fn) fn = RtAddrFn<ArraySetSizeFn>(0x18007a790ull);
    fn(a, n);
}
// 0x18007a900  Array<UV16>::SetSize      (stride 4, align 2, tail UNTOUCHED)
void UvArraySetSize(Collection::Array<UV16>* a, int32_t n) {
    static ArraySetSizeFn fn = nullptr;
    if (!fn) fn = RtAddrFn<ArraySetSizeFn>(0x18007a900ull);
    fn(a, n);
}

// 0x18007aa40  Array<SubMesh>::SetSize. NON-TRIVIAL: element copy ctor
// 0x1800734a0, per-element destroy 0x180073ee0 on the shrink tail and on the
// abandoned buffer. See the banner above for what the old private copy got
// wrong.
void SubMeshArraySetSize(Collection::Array<SubMesh>* a, int32_t n) {
    static ArraySetSizeFn fn = nullptr;
    if (!fn) fn = RtAddrFn<ArraySetSizeFn>(0x18007aa40ull);
    fn(a, n);
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
    using StrSerFn      = void (*)(void* str, Serializer* ser);
    using ScalarIntFn   = void (*)(ReaderWriter*, int32_t*, const char*);
    using ArrayFloatFn  = void (*)(ReaderWriter*, int32_t, void*, const char*);
    using ArrayUShortFn = void (*)(ReaderWriter*, int32_t, void*, const char*);
    ReaderWriter* rw = ser->readerWriter;
    auto strSer = RT_EXPORT_ONCE(StrSerFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z");
    auto scalarInt   = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    auto arrayFloat  = reinterpret_cast<ArrayFloatFn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8]);
    auto arrayUShort = reinterpret_cast<ArrayUShortFn>(rw->vtable[rt::io::rw_vtbl::kArrayUShort / 8]);
    const bool reading = (ser->stream->direction == 0);

    for (int32_t i = 0; i < count; ++i) {
        SubMesh* s = &base[i];
        strSer(&s->_name, ser);
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

// 0x180073720  ~Array<SubMesh>: the element destroy 0x180073ee0, then the
// plain `~Array`. Retail's ~PickingMesh CALLs exactly this at 0x1801228c4.
void SubMeshArrayDtor(Collection::Array<SubMesh>* a) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = RtAddrFn<Fn>(0x180073720ull);
    fn(a);
}

}  // namespace

extern "C" uint32_t RtPicking_PickingMesh_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, PickingMeshTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtPicking_PickingMesh_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, PickingMeshTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtPicking_PickingMesh_GetFileVersion(const void*) { return 9; }
extern "C" int32_t RtPicking_PickingMesh_Compile(void*, const void*) { return 1; }

extern "C" void* RtPicking_PickingMesh_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<PickingMesh*>(selfv)); return selfv;
}
extern "C" void* RtPicking_PickingMesh_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<PickingMesh*>(selfv)); return selfv;
}
extern "C" void RtPicking_PickingMesh_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1PickingMesh@Picking@@UEAA@XZ (0x1801228a0)
extern "C" void RtPicking_PickingMesh_Dtor(void* selfv) {
    PickingMesh* self = static_cast<PickingMesh*>(selfv);
    *reinterpret_cast<void**>(self) = PickingMeshVTable();
    // Retail CALLs 0x180073720 here (at 0x1801228c4): the element destroy
    // 0x180073ee0 over every SubMesh -- four inner buffers DESCENDING from
    // +0x70 and then the String at +0x00 -- followed by the plain `~Array`.
    // This used to be written out inline, with the four frees in the wrong
    // order relative to retail and no header clear per element; it now goes
    // through the ported bodies (batch D2, 2026-09-12).
    SubMeshArrayDtor(&self->_nodes);
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name2));
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name1));
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ?Serialize@PickingMesh@Picking@@UEAA_NAEAVStream@IO@@@Z (0x180122960)
extern "C" bool RtPicking_PickingMesh_Serialize(void* selfv, void* streamv) {
    PickingMesh* self = static_cast<PickingMesh*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using StrSerFn = void (*)(void* str, Serializer* ser);
    auto strSer = RT_EXPORT_ONCE(StrSerFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z");
    strSer(&self->_name1, &ser);
    strSer(&self->_name2, &ser);

    using ScalarBoolFn   = void (*)(ReaderWriter*, bool*, const char*);
    using ScalarIntFn    = void (*)(ReaderWriter*, int32_t*, const char*);
    auto scalarBool   = reinterpret_cast<ScalarBoolFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8]);
    auto scalarInt    = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    const bool reading = (stream->direction == 0);

    scalarBool(rw, &self->_flag, nullptr);

    int32_t nodeCount = self->_nodes._size;
    scalarInt(rw, &nodeCount, nullptr);
    if (reading) SubMeshArraySetSize(&self->_nodes, nodeCount);

    // Retail CALLs 0x18007ac20 here rather than inlining the walk; so does
    // this, through the ported body, so the registered function is the one
    // that runs (batch D2, 2026-09-12).
    SerializeSubMeshesBody(&ser, nodeCount, self->_nodes._data);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
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
