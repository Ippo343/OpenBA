// ============================================================================
// runtime/src/collision2d/collisionmesh.cpp — Collision2D::CollisionMesh, whole class
// ============================================================================
//
// Vtable batch 8. ??_7CollisionMesh@Collision2D@@6B@ (retail), nine slots:
//
//   0  deleting dtor        0x1800656b0  deleting_dtor.cpp row (registers ~CollisionMesh here)
//   1  TypeName             0x180064f70  here
//   2  Type                 0x180064f50  here
//   3  GetFileVersion       0x180108cc0  here  (returns 3)
//   4  Compile              0x180108cb0  here  (returns 1)
//   5  Serialize            0x180108cd0  here
//   6  Reconstruct          0x1800651b0  here
//   7  Destruct             0x1800652c0  here
//   8  Construct            0x180065240  here
//
// plus ~CollisionMesh (0x180065400), registered here and appended to
// runtime/src/object/deleting_dtor.cpp's kSpecs.
//
// Layout (sizeof 0x70; Reconstruct/Construct zero +0x20..+0x68 as ten qwords,
// factory step table runtime/src/collision2d/rt_factories_collision2d.inc agrees):
//   +0x00  Object::External                (0x20)
//   +0x20  System::String              _name1
//   +0x28  System::String              _name2
//   +0x30  Array<Vec2>                 _ptsA    {data,size@0x38,cap@0x3c}  align4
//   +0x40  Array<Vec2>                 _ptsB    {data,size@0x48,cap@0x4c}  align8
//   +0x50  Array<Face>                 _faces   {data,size@0x58,cap@0x5c}  align8
//   +0x60  Array<ExternalPtr<Surface>> _surfaces{data,size@0x68,cap@0x6c}  align8
//
// Face (0x28, from FUN_1800665c0 0x1800665c0 disassembled at instruction level --
// Ghidra's decompile of this one drops several call arguments, a known project
// pitfall, so this shape came from the raw disassembly, not the pseudo-C):
//   +0x00  int32_t                field0      kScalarInt
//   +0x04  (pad)
//   +0x08  Array<Triple3>         _triples    {data,size@0x10,cap@0x14}  align2
//   +0x18  Array<Quad4>           _quads      {data,size@0x20,cap@0x24}  align2
// Serialize order per face: field0, THEN _quads (count, resize, 4x kScalarUShort
// per element), THEN _triples (count, resize, kArrayUShort(n=3) per element).
// Grow: FUN_180066350 (Face array, element assign FUN_180065b70 deep-clones both
// nested arrays via FUN_180066de0/FUN_180066b30, shrink-tail destroy FUN_1800664c0
// frees both nested buffers). Nested array grows: FUN_180066e80 (_triples,
// stride 6) and FUN_180066bc0 (_quads, stride 8), both plain zero-fill-tail, no
// per-element dtor.
//
// _ptsA/_ptsB: Vec2 (2 floats), serialized as kArrayFloat(2, ptr) per element.
// Grows FUN_1800660f0 (_ptsA, align4) and FUN_1800666e0 (_ptsB, align8) are both
// plain zero-fill-tail, no per-element dtor -- align differs only in the MemAlloc
// call, transcribed for fidelity though it has no observable effect.
//
// _surfaces: ExternalPtr<Collision2D::Surface> (a bare, non-owning External*).
// Grow reuses FUN_1800666e0 (same stride 8/align 8 as _ptsB -- retail happens to
// share the body). Serialize is Object::ExternalPtr<T>::Serialize (Wave 10 lane
// EXTPTR, runtime/src/object/external_ptr_serialize.cpp), confirmed by reading
// FUN_1800667f0's typeInfo global (0x1801fa820) against Surface::Type's own body
// (0x180064a50: `MOV EAX,[0x1801fa820]; RET`) -- same address, so T == Surface.
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
#include "../system/string.h"

namespace {

using RtObject::External;
using RtSystem::String;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// The generic Object::ExternalPtr<T>::Serialize body (Wave 10 lane EXTPTR).
extern "C" void RtObject_ExternalPtr_Serialize(External** self, Serializer* ser, const void* typeInfo);

struct Vec2 { float x, y; };
static_assert(sizeof(Vec2) == 8, "Vec2 is 8 bytes");

struct Triple3 { uint16_t a, b, c; };
static_assert(sizeof(Triple3) == 6, "Triple3 is 6 bytes");

struct Quad4 { uint16_t a, b, c, d; };
static_assert(sizeof(Quad4) == 8, "Quad4 is 8 bytes");

struct Face {
    int32_t                   field0;    // +0x00
    int32_t                   _pad04;    // +0x04
    Collection::Array<Triple3> triples;  // +0x08
    Collection::Array<Quad4>   quads;    // +0x18
};
static_assert(sizeof(Face) == 0x28, "CollisionMesh::Face is 0x28 bytes");
static_assert(offsetof(Face, triples) == 0x08, "+0x08");
static_assert(offsetof(Face, quads)   == 0x18, "+0x18");

struct CollisionMesh {
    External                    base;       // +0x00
    void*                       _name1;     // +0x20  System::String
    void*                       _name2;     // +0x28  System::String
    Collection::Array<Vec2>     _ptsA;      // +0x30
    Collection::Array<Vec2>     _ptsB;      // +0x40
    Collection::Array<Face>     _faces;     // +0x50
    Collection::Array<void*>    _surfaces;  // +0x60  ExternalPtr<Surface>
};
static_assert(sizeof(CollisionMesh) == 0x70, "Collision2D::CollisionMesh is 0x70 bytes");
static_assert(offsetof(CollisionMesh, _name1)    == 0x20, "+0x20");
static_assert(offsetof(CollisionMesh, _name2)    == 0x28, "+0x28");
static_assert(offsetof(CollisionMesh, _ptsA)     == 0x30, "+0x30");
static_assert(offsetof(CollisionMesh, _ptsB)     == 0x40, "+0x40");
static_assert(offsetof(CollisionMesh, _faces)    == 0x50, "+0x50");
static_assert(offsetof(CollisionMesh, _surfaces) == 0x60, "+0x60");

void* BaseVTable()          { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()      { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* CollisionMeshVTable() { return RT_DATA_SLOT(void*, "??_7CollisionMesh@Collision2D@@6B@"); }
const uint8_t* CollisionMeshTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@CollisionMesh@Collision2D@@0VBaseTypeInfo@Object@@A");
}
const void* SurfaceTypeInfo() {
    return RT_DATA_SLOT(const void*, "?_typeInfo@Surface@Collision2D@@0VBaseTypeInfo@Object@@A");
}

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

// ---- allocator boundary, by address (array.cpp's banner: heap cannot be half-ours) --
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

void ConstructBody(CollisionMesh* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = CollisionMeshVTable();
    self->_name1 = nullptr;
    self->_name2 = nullptr;
    self->_ptsA._data = nullptr;     self->_ptsA._size = 0;     self->_ptsA._capacity = 0;
    self->_ptsB._data = nullptr;     self->_ptsB._size = 0;     self->_ptsB._capacity = 0;
    self->_faces._data = nullptr;    self->_faces._size = 0;    self->_faces._capacity = 0;
    self->_surfaces._data = nullptr; self->_surfaces._size = 0; self->_surfaces._capacity = 0;
}

// ---- generic byte-copy array SetSize: zero-fill tail, no per-element dtor. ----
// FUN_1800660f0 (_ptsA, align 4), FUN_1800666e0 (_ptsB and _surfaces, align 8).
template <typename T>
void GenericArraySetSize(Collection::Array<T>* a, int32_t newSize, int align) {
    int32_t oldSize = a->_size;
    if (newSize == oldSize) return;
    T* oldData = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->_capacity = newSize * 3;
        a->_data = static_cast<T*>(MemAlloc(static_cast<uint64_t>(a->_capacity) * sizeof(T), align));
    }
    int32_t copyFrom = (oldData == a->_data) ? oldSize : 0;
    for (int32_t i = copyFrom; i < newSize; ++i) {
        if (i < oldSize) a->_data[i] = oldData[i];
        else a->_data[i] = T{};
    }
    if (oldData != a->_data) {
        if (oldData != nullptr) MemFree(oldData);
    }
}

// ---- Face's two nested POD arrays: plain zero-fill-tail grows. ----
// FUN_180066e80 (_triples, stride 6), FUN_180066bc0 (_quads, stride 8).
void TripleArraySetSize(Collection::Array<Triple3>* a, int32_t n) { GenericArraySetSize(a, n, 2); }
void QuadArraySetSize(Collection::Array<Quad4>* a, int32_t n)     { GenericArraySetSize(a, n, 2); }

// FUN_180066de0 / FUN_180066b30 — deep-clone a Face's nested array into a fresh buffer.
void CloneTriples(Collection::Array<Triple3>* dst, const Collection::Array<Triple3>* src) {
    dst->_data = nullptr; dst->_size = 0; dst->_capacity = 0;
    int32_t n = src->_size;
    if (n <= 0) return;
    dst->_size = n; dst->_capacity = n;
    dst->_data = static_cast<Triple3*>(MemAlloc(static_cast<uint64_t>(n) * sizeof(Triple3), 2));
    for (int32_t i = 0; i < n; ++i) dst->_data[i] = src->_data[i];
}
void CloneQuads(Collection::Array<Quad4>* dst, const Collection::Array<Quad4>* src) {
    dst->_data = nullptr; dst->_size = 0; dst->_capacity = 0;
    int32_t n = src->_size;
    if (n <= 0) return;
    dst->_size = n; dst->_capacity = n;
    dst->_data = static_cast<Quad4*>(MemAlloc(static_cast<uint64_t>(n) * sizeof(Quad4), 2));
    for (int32_t i = 0; i < n; ++i) dst->_data[i] = src->_data[i];
}
// FUN_1800664c0 — release a Face's two nested buffers over [data, data+count).
void FreeFacesRange(Face* data, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        if (data[i].quads._data)    MemFree(data[i].quads._data);
        data[i].quads._data = nullptr; data[i].quads._size = 0; data[i].quads._capacity = 0;
        if (data[i].triples._data)  MemFree(data[i].triples._data);
        data[i].triples._data = nullptr; data[i].triples._size = 0; data[i].triples._capacity = 0;
    }
}
// FUN_180066350 — Collection::Array<Face>::SetSize.
void FaceArraySetSize(Collection::Array<Face>* a, int32_t newSize) {
    int32_t oldSize = a->_size;
    if (newSize == oldSize) return;
    Face* oldData = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->_capacity = newSize * 3;
        a->_data = static_cast<Face*>(MemAlloc(static_cast<uint64_t>(a->_capacity) * sizeof(Face), 8));
    }
    int32_t copyFrom = (oldData == a->_data) ? oldSize : 0;
    for (int32_t i = copyFrom; i < newSize; ++i) {
        Face* dst = &a->_data[i];
        if (i < oldSize) {
            dst->field0 = oldData[i].field0;
            dst->_pad04 = 0;
            CloneTriples(&dst->triples, &oldData[i].triples);
            CloneQuads(&dst->quads, &oldData[i].quads);
        } else {
            dst->field0 = 0; dst->_pad04 = 0;
            dst->triples = { nullptr, 0, 0 };
            dst->quads   = { nullptr, 0, 0 };
        }
    }
    if (oldData == a->_data) {
        if (newSize < oldSize) FreeFacesRange(oldData + newSize, oldSize - newSize);
    } else {
        FreeFacesRange(oldData, oldSize);
        if (oldData != nullptr) MemFree(oldData);
    }
}

}  // namespace

extern "C" uint32_t RtCollision2D_CollisionMesh_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, CollisionMeshTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtCollision2D_CollisionMesh_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, CollisionMeshTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtCollision2D_CollisionMesh_GetFileVersion(const void*) { return 3; }
extern "C" int32_t RtCollision2D_CollisionMesh_Compile(void*, const void*) { return 1; }

extern "C" void* RtCollision2D_CollisionMesh_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<CollisionMesh*>(selfv)); return selfv;
}
extern "C" void* RtCollision2D_CollisionMesh_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<CollisionMesh*>(selfv)); return selfv;
}
extern "C" void RtCollision2D_CollisionMesh_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1CollisionMesh@Collision2D@@UEAA@XZ (0x180065400)
extern "C" void RtCollision2D_CollisionMesh_Dtor(void* selfv) {
    CollisionMesh* self = static_cast<CollisionMesh*>(selfv);
    // _surfaces (ExternalPtr<Surface>, bare pointers -- just free the buffer).
    if (self->_surfaces._data) MemFree(self->_surfaces._data);
    self->_surfaces = { nullptr, 0, 0 };
    // _faces -- release each element's nested arrays, then the buffer.
    FreeFacesRange(self->_faces._data, self->_faces._size);
    if (self->_faces._data) MemFree(self->_faces._data);
    self->_faces = { nullptr, 0, 0 };
    // _ptsB / _ptsA -- plain buffers, no per-element dtor.
    if (self->_ptsB._data) MemFree(self->_ptsB._data);
    self->_ptsB = { nullptr, 0, 0 };
    if (self->_ptsA._data) MemFree(self->_ptsA._data);
    self->_ptsA = { nullptr, 0, 0 };
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name2));
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name1));
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ?Serialize@CollisionMesh@Collision2D@@UEAA_NAEAVStream@IO@@@Z (0x180108cd0)
extern "C" bool RtCollision2D_CollisionMesh_Serialize(void* selfv, void* streamv) {
    CollisionMesh* self = static_cast<CollisionMesh*>(selfv);
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

    using ScalarIntFn    = void (*)(ReaderWriter*, int32_t*, const char*);
    using ScalarUShortFn = void (*)(ReaderWriter*, uint16_t*, const char*);
    using ArrayFloatFn   = void (*)(ReaderWriter*, int32_t, void*, const char*);
    using ArrayUShortFn  = void (*)(ReaderWriter*, int32_t, void*, const char*);
    auto scalarInt    = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    auto scalarUShort = reinterpret_cast<ScalarUShortFn>(rw->vtable[rt::io::rw_vtbl::kScalarUShort / 8]);
    auto arrayFloat   = reinterpret_cast<ArrayFloatFn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8]);
    auto arrayUShort  = reinterpret_cast<ArrayUShortFn>(rw->vtable[rt::io::rw_vtbl::kArrayUShort / 8]);
    const bool reading = (stream->direction == 0);

    // _ptsA
    int32_t nA = self->_ptsA._size;
    scalarInt(rw, &nA, nullptr);
    if (reading) GenericArraySetSize(&self->_ptsA, nA, 4);
    for (int32_t i = 0; i < nA; ++i) arrayFloat(rw, 2, &self->_ptsA._data[i], nullptr);

    // _ptsB
    int32_t nB = self->_ptsB._size;
    scalarInt(rw, &nB, nullptr);
    if (reading) GenericArraySetSize(&self->_ptsB, nB, 8);
    for (int32_t i = 0; i < nB; ++i) arrayFloat(rw, 2, &self->_ptsB._data[i], nullptr);

    // _faces
    int32_t nF = self->_faces._size;
    scalarInt(rw, &nF, nullptr);
    if (reading) FaceArraySetSize(&self->_faces, nF);
    for (int32_t i = 0; i < nF; ++i) {
        Face* f = &self->_faces._data[i];
        scalarInt(rw, &f->field0, nullptr);

        int32_t quadCount = f->quads._size;
        scalarInt(rw, &quadCount, nullptr);
        if (reading) QuadArraySetSize(&f->quads, quadCount);
        for (int32_t q = 0; q < quadCount; ++q) {
            scalarUShort(rw, &f->quads._data[q].a, nullptr);
            scalarUShort(rw, &f->quads._data[q].b, nullptr);
            scalarUShort(rw, &f->quads._data[q].c, nullptr);
            scalarUShort(rw, &f->quads._data[q].d, nullptr);
        }

        int32_t tripleCount = f->triples._size;
        scalarInt(rw, &tripleCount, nullptr);
        if (reading) TripleArraySetSize(&f->triples, tripleCount);
        for (int32_t t = 0; t < tripleCount; ++t) {
            arrayUShort(rw, 3, &f->triples._data[t], nullptr);
        }
    }

    // _surfaces (ExternalPtr<Collision2D::Surface>)
    int32_t nS = self->_surfaces._size;
    scalarInt(rw, &nS, nullptr);
    if (reading) GenericArraySetSize(&self->_surfaces, nS, 8);
    const void* surfaceTypeInfo = SurfaceTypeInfo();
    for (int32_t i = 0; i < nS; ++i) {
        RtObject_ExternalPtr_Serialize(
            reinterpret_cast<External**>(&self->_surfaces._data[i]), &ser, surfaceTypeInfo);
    }

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtCollision2DCollisionMeshRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@CollisionMesh@Collision2D@@UEBAIXZ",           (void*)&RtCollision2D_CollisionMesh_Type);           ++n;
    RegisterRuntimeAddr  (0x180064f50ull,                                      (void*)&RtCollision2D_CollisionMesh_Type);
    RegisterRuntimeExport("?TypeName@CollisionMesh@Collision2D@@UEBAPEBDXZ",    (void*)&RtCollision2D_CollisionMesh_TypeName);       ++n;
    RegisterRuntimeAddr  (0x180064f70ull,                                      (void*)&RtCollision2D_CollisionMesh_TypeName);
    RegisterRuntimeExport("?GetFileVersion@CollisionMesh@Collision2D@@UEBAHXZ", (void*)&RtCollision2D_CollisionMesh_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x180108cc0ull,                                      (void*)&RtCollision2D_CollisionMesh_GetFileVersion);
    RegisterRuntimeExport("?Compile@CollisionMesh@Collision2D@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
                          (void*)&RtCollision2D_CollisionMesh_Compile); ++n;
    RegisterRuntimeAddr  (0x180108cb0ull,                                      (void*)&RtCollision2D_CollisionMesh_Compile);
    RegisterRuntimeExport("?Construct@CollisionMesh@Collision2D@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtCollision2D_CollisionMesh_Construct);   ++n;
    RegisterRuntimeAddr  (0x180065240ull,                                                        (void*)&RtCollision2D_CollisionMesh_Construct);
    RegisterRuntimeExport("?Reconstruct@CollisionMesh@Collision2D@@UEAAPEAVExternal@Object@@XZ", (void*)&RtCollision2D_CollisionMesh_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x1800651b0ull,                                                        (void*)&RtCollision2D_CollisionMesh_Reconstruct);
    RegisterRuntimeExport("?Destruct@CollisionMesh@Collision2D@@UEAAXXZ",       (void*)&RtCollision2D_CollisionMesh_Destruct);       ++n;
    RegisterRuntimeAddr  (0x1800652c0ull,                                      (void*)&RtCollision2D_CollisionMesh_Destruct);
    RegisterRuntimeExport("??1CollisionMesh@Collision2D@@UEAA@XZ",              (void*)&RtCollision2D_CollisionMesh_Dtor);           ++n;
    RegisterRuntimeAddr  (0x180065400ull,                                      (void*)&RtCollision2D_CollisionMesh_Dtor);
    RegisterRuntimeExport("?Serialize@CollisionMesh@Collision2D@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtCollision2D_CollisionMesh_Serialize); ++n;
    RegisterRuntimeAddr  (0x180108cd0ull,                                                       (void*)&RtCollision2D_CollisionMesh_Serialize);
    return n;
}
