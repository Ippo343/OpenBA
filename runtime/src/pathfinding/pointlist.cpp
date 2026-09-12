// ============================================================================
// runtime/src/pathfinding/pointlist.cpp — Pathfinding::PointList, whole class
// ============================================================================
//
// Vtable batch 8. ??_7PointList@Pathfinding@@6B@ (retail), nine slots:
//
//   0  deleting dtor        0x1800713b0  deleting_dtor.cpp row (registers ~PointList here)
//   1  TypeName             0x180070e40  here
//   2  Type                 0x180070e20  here
//   3  GetFileVersion       0x18010f880  here  (returns 5)
//   4  Compile              0x18010f6d0  here  (returns 1, External::CompileResult::Ok)
//   5  Serialize            0x18010f6e0  here
//   6  Reconstruct          0x180071060  here
//   7  Destruct             0x180071150  here
//   8  Construct            0x1800710e0  here
//
// plus ~PointList (0x18010f8e0), registered here and appended to
// runtime/src/object/deleting_dtor.cpp's kSpecs.
//
// Layout (sizeof 0x50; Reconstruct/Construct zero +0x20..+0x48 as six qwords,
// factory step table runtime/src/pathfinding/rt_factories_pathfinding.inc agrees):
//   +0x00  Object::External              (0x20)
//   +0x20  System::String   _name1
//   +0x28  System::String   _name2
//   +0x30  Array<Point>     _pointsA     {data, size@0x38, cap@0x3c}
//   +0x40  Array<Point>     _pointsB     {data, size@0x48, cap@0x4c}
//
// Point (0x18, from Serialize 0x18010f6e0's element loop -- String at elem+0,
// Vector3 at elem+8 via kArrayFloat(3,ptr), float at elem+0x14 via kScalarFloat):
//   +0x00  System::String  _name
//   +0x08  Vector3         _pos      (3 floats, kArrayFloat n=3)
//   +0x14  float           _radius   (kScalarFloat)
//
// Serialize order (0x18010f6e0): _name1, _name2, then _pointsB (this+0x40) count
// at +0x48, THEN _pointsA (this+0x30) count at +0x38 -- retail serializes the
// SECOND array field before the first.
// Array growth: FUN_180071730 (0x180071730) -- generic Point-stride (0x18) grow,
// same MemAlloc/copy/zero-tail shape as every other Collection::Array<T> grow
// this port has already transcribed elsewhere; align 8 (`MemAlloc(n*0x18,8)`).
// Element release: FUN_180071890 releases each Point's String (refcount idiom)
// walking with stride 3 longlongs (0x18); ~PointList (0x10f8e0) frees each
// array's own String elements via the same walk before freeing the buffer.
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

struct Point {
    void*   name;      // +0x00  System::String (refcounted)
    float   pos[3];    // +0x08
    float   radius;    // +0x14
};
static_assert(sizeof(Point) == 0x18, "Pathfinding::PointList::Point is 0x18 bytes");

struct PointList {
    External                    base;      // +0x00
    void*                       _name1;    // +0x20  System::String
    void*                       _name2;    // +0x28  System::String
    Collection::Array<Point>    _pointsA;  // +0x30
    Collection::Array<Point>    _pointsB;  // +0x40
};
static_assert(sizeof(PointList) == 0x50, "Pathfinding::PointList is 0x50 bytes");
static_assert(offsetof(PointList, _name1)   == 0x20, "+0x20");
static_assert(offsetof(PointList, _name2)   == 0x28, "+0x28");
static_assert(offsetof(PointList, _pointsA) == 0x30, "+0x30");
static_assert(offsetof(PointList, _pointsB) == 0x40, "+0x40");

void* BaseVTable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* PointListVTable(){ return RT_DATA_SLOT(void*, "??_7PointList@Pathfinding@@6B@"); }
const uint8_t* PointListTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@PointList@Pathfinding@@0VBaseTypeInfo@Object@@A");
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

void ConstructBody(PointList* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = PointListVTable();
    self->_name1 = nullptr;
    self->_name2 = nullptr;
    self->_pointsA._data = nullptr; self->_pointsA._size = 0; self->_pointsA._capacity = 0;
    self->_pointsB._data = nullptr; self->_pointsB._size = 0; self->_pointsB._capacity = 0;
}

// FUN_180071890 -- release every Point's String in [0, count) (this file's ~PointList caller).
void ReleasePointsRange(Point* data, int count) {
    for (int i = 0; i < count; ++i) {
        RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&data[i].name));
    }
}

// FUN_180071730 -- Collection::Array<Point>::SetSize (retail 0x180071730), byte-faithful
// grow/shrink: reused old buffer's live elements are moved, new tail elements get a
// zeroed Point (default), shrink-tail elements have their String released before the
// buffer is (re)used or freed.
void PointArraySetSize(Collection::Array<Point>* a, int32_t newSize) {
    int32_t oldSize = a->_size;
    if (newSize == oldSize) return;
    Point* oldData = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->_capacity = newSize * 3;
        a->_data = static_cast<Point*>(MemAlloc(static_cast<uint64_t>(a->_capacity) * sizeof(Point), 8));
    }
    int32_t copyFrom = (oldData == a->_data) ? oldSize : 0;
    for (int32_t i = copyFrom; i < newSize; ++i) {
        Point* dst = &a->_data[i];
        if (i < oldSize) *dst = oldData[i];
        else { dst->name = nullptr; dst->pos[0] = dst->pos[1] = dst->pos[2] = 0.0f; dst->radius = 0.0f; }
    }
    if (oldData == a->_data) {
        if (newSize < oldSize) ReleasePointsRange(oldData + newSize, oldSize - newSize);
    } else {
        ReleasePointsRange(oldData, oldSize);
        if (oldData != nullptr) MemFree(oldData);
    }
}

}  // namespace

extern "C" uint32_t RtPathfinding_PointList_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, PointListTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtPathfinding_PointList_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, PointListTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtPathfinding_PointList_GetFileVersion(const void*) { return 5; }
extern "C" int32_t RtPathfinding_PointList_Compile(void*, const void*) { return 1; }

extern "C" void* RtPathfinding_PointList_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<PointList*>(selfv)); return selfv;
}
extern "C" void* RtPathfinding_PointList_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<PointList*>(selfv)); return selfv;
}
extern "C" void RtPathfinding_PointList_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1PointList@Pathfinding@@UEAA@XZ (0x18010f8e0)
extern "C" void RtPathfinding_PointList_Dtor(void* selfv) {
    PointList* self = static_cast<PointList*>(selfv);
    *reinterpret_cast<void**>(self) = PointListVTable();
    PointArraySetSize(&self->_pointsB, 0);
    PointArraySetSize(&self->_pointsA, 0);
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name2));
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&self->_name1));
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ?Serialize@PointList@Pathfinding@@UEAA_NAEAVStream@IO@@@Z (0x18010f6e0)
extern "C" bool RtPathfinding_PointList_Serialize(void* selfv, void* streamv) {
    PointList* self = static_cast<PointList*>(selfv);
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

    using ScalarIntFn   = void (*)(ReaderWriter*, int32_t*, const char*);
    using ScalarFloatFn = void (*)(ReaderWriter*, float*, const char*);
    using ArrayFloatFn  = void (*)(ReaderWriter*, int32_t, void*, const char*);
    auto scalarInt   = reinterpret_cast<ScalarIntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8]);
    auto scalarFloat = reinterpret_cast<ScalarFloatFn>(rw->vtable[rt::io::rw_vtbl::kScalarFloat / 8]);
    auto arrayFloat  = reinterpret_cast<ArrayFloatFn>(rw->vtable[rt::io::rw_vtbl::kArrayFloat / 8]);

    auto serializeArray = [&](Collection::Array<Point>* arr) {
        int32_t n = arr->_size;
        scalarInt(rw, &n, nullptr);
        if (stream->direction == 0) PointArraySetSize(arr, n);
        for (int32_t i = 0; i < n; ++i) {
            Point* p = &arr->_data[i];
            strSer(&p->name, &ser);
            arrayFloat(rw, 3, p->pos, nullptr);
            scalarFloat(rw, &p->radius, nullptr);
        }
    };

    // Retail serializes _pointsB (this+0x40) before _pointsA (this+0x30).
    serializeArray(&self->_pointsB);
    serializeArray(&self->_pointsA);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtPathfindingPointListRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@PointList@Pathfinding@@UEBAIXZ",           (void*)&RtPathfinding_PointList_Type);           ++n;
    RegisterRuntimeAddr  (0x180070e20ull,                                  (void*)&RtPathfinding_PointList_Type);
    RegisterRuntimeExport("?TypeName@PointList@Pathfinding@@UEBAPEBDXZ",    (void*)&RtPathfinding_PointList_TypeName);       ++n;
    RegisterRuntimeAddr  (0x180070e40ull,                                  (void*)&RtPathfinding_PointList_TypeName);
    RegisterRuntimeExport("?GetFileVersion@PointList@Pathfinding@@UEBAHXZ", (void*)&RtPathfinding_PointList_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x18010f880ull,                                  (void*)&RtPathfinding_PointList_GetFileVersion);
    RegisterRuntimeExport("?Compile@PointList@Pathfinding@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
                          (void*)&RtPathfinding_PointList_Compile); ++n;
    RegisterRuntimeAddr  (0x18010f6d0ull,                                  (void*)&RtPathfinding_PointList_Compile);
    RegisterRuntimeExport("?Construct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtPathfinding_PointList_Construct);   ++n;
    RegisterRuntimeAddr  (0x1800710e0ull,                                                    (void*)&RtPathfinding_PointList_Construct);
    RegisterRuntimeExport("?Reconstruct@PointList@Pathfinding@@UEAAPEAVExternal@Object@@XZ", (void*)&RtPathfinding_PointList_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180071060ull,                                                    (void*)&RtPathfinding_PointList_Reconstruct);
    RegisterRuntimeExport("?Destruct@PointList@Pathfinding@@UEAAXXZ",       (void*)&RtPathfinding_PointList_Destruct);       ++n;
    RegisterRuntimeAddr  (0x180071150ull,                                  (void*)&RtPathfinding_PointList_Destruct);
    RegisterRuntimeExport("??1PointList@Pathfinding@@UEAA@XZ",              (void*)&RtPathfinding_PointList_Dtor);           ++n;
    RegisterRuntimeAddr  (0x18010f8e0ull,                                  (void*)&RtPathfinding_PointList_Dtor);
    RegisterRuntimeExport("?Serialize@PointList@Pathfinding@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtPathfinding_PointList_Serialize); ++n;
    RegisterRuntimeAddr  (0x18010f6e0ull,                                                    (void*)&RtPathfinding_PointList_Serialize);
    return n;
}
