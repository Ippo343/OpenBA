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
#include <new>          // ENDGAME B.3.1 step 2: placement new into the object body

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
    // ENDGAME B.3.1 step 2 LEAVES THIS AS A RAW POINTER, deliberately.
    // `Collection::Array<Point>` moves its elements with a bulk byte copy
    // (`PointArraySetSize`, by address), so a member with a real copy
    // constructor or destructor would be SILENTLY BYPASSED by every resize —
    // the hazard the banners in runtime/types/object/External.h and Internal.h
    // spell out. `ReleasePointsRange` below is this field's destructor, written
    // by hand exactly because the array will not run one.
    void*   name;      // +0x00  System::String (refcounted)
    float   pos[3];    // +0x08
    float   radius;    // +0x14
};
static_assert(sizeof(Point) == 0x18, "Pathfinding::PointList::Point is 0x18 bytes");

struct PointList {
    External                    base;      // +0x00
    // ENDGAME B.3.1 step 2: the comment became the type. Unlike `Point` above,
    // `PointList` itself is never bulk-copied — it is built by `ConstructBody`
    // and torn down by the destructor, both in this file — so these two carry
    // the real class.
    System::String              _name1;    // +0x20
    System::String              _name2;    // +0x28
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
    // Retail writes one null qword per field; the default constructor
    // (0x180006760) is that one store, placement-constructed because the
    // object body is raw memory at this point.
    new (&self->_name1) System::String();
    new (&self->_name2) System::String();
    self->_pointsA._data = nullptr; self->_pointsA._size = 0; self->_pointsA._capacity = 0;
    self->_pointsB._data = nullptr; self->_pointsB._size = 0; self->_pointsB._capacity = 0;
}

// FUN_180071890 (retail 0x180071890) -- release every Point's String in
// [0, count). Body B with the String at +0x00 and stride 0x18, the same rule
// runtime/src/collection/array_destroy.cpp files as kD_Str_18at00; kept here,
// where its two callers already live, so there is one body per address.
// REGISTERED as of batch D2 (2026-09-12): retail's 0x180071730 and 0x180071600
// both reach it by rel32, and at cutover that rel32 will not exist.
void ReleasePointsRange(Point* data, int count) {
    for (int i = 0; i < count; ++i) {
        // The release idiom IS `~String` (0x180006c80). `Point::name` keeps its
        // raw-pointer type (see the struct), so the destructor is spelled out
        // over the same eight bytes.
        reinterpret_cast<System::String*>(&data[i].name)->~String();
    }
}
extern "C" void RtPathfinding_PointArray_ReleaseRange(void* ignoredThis, void* data, int count) {
    (void)ignoredThis;   // retail spills RCX to the shadow space and never reads it
    ReleasePointsRange(static_cast<Point*>(data), count);
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
        if (i < oldSize) {
            // *** NOT a plain struct assignment. *** Retail's copy arm nulls
            // the destination's String slot, adopts the source's pointer and
            // BUMPS the shared refcount at [str-4], then copies four plain
            // dwords. This body used to do `*dst = oldData[i]`, which moved
            // the pointer WITHOUT the bump -- and since the realloc arm below
            // then calls ReleasePointsRange over the whole old buffer, every
            // grow of a named PointList dropped one reference per element and
            // could free a String the new buffer still points at. Fixed while
            // registering the address (batch D2, 2026-09-12).
            const Point* src = &oldData[i];
            dst->name = nullptr;
            if (void* s = src->name) {
                dst->name = s;
                ++*(reinterpret_cast<int32_t*>(static_cast<uint8_t*>(s) - 4));
            }
            dst->pos[0] = src->pos[0];
            dst->pos[1] = src->pos[1];
            dst->pos[2] = src->pos[2];
            dst->radius = src->radius;
        } else {
            // retail's else arm: THREE zero qwords over the whole 0x18 stride
            uint64_t* q = reinterpret_cast<uint64_t*>(dst);
            q[0] = 0; q[1] = 0; q[2] = 0;
        }
    }
    if (oldData == a->_data) {
        if (newSize < oldSize) ReleasePointsRange(oldData + newSize, oldSize - newSize);
    } else {
        ReleasePointsRange(oldData, oldSize);
        if (oldData != nullptr) MemFree(oldData);
    }
}

// FUN_180071600 (retail 0x180071600) -- `~Array<Point>`: the element destroy
// above over [0, size), then the plain `~Array` (free the buffer, zero the
// three header fields). The J2 shape, written here rather than in
// array_destroy.cpp because its element helper already lives in this file and
// a second copy of either is exactly the drift this project keeps paying for.
void PointArrayDtor(Collection::Array<Point>* a) {
    ReleasePointsRange(a->_data, a->_size);
    if (a->_data != nullptr) MemFree(a->_data);
    a->_data = nullptr;
    a->_size = 0;
    a->_capacity = 0;
}

}  // namespace

// ---- CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12) -----------------
// The three `Collection::Array<Pathfinding::Point>` bodies this file has always
// had and never registered. Retail's signatures exactly, so they can stand at
// their addresses for any caller.
extern "C" void RtPathfinding_PointArray_SetSize(void* a, int32_t n) {
    PointArraySetSize(static_cast<Collection::Array<Point>*>(a), n);
}
extern "C" void* RtPathfinding_PointArray_Dtor(void* a) {
    PointArrayDtor(static_cast<Collection::Array<Point>*>(a));
    return nullptr;
}

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
    // Retail CALLs 0x180071600 (~Array<Point>) on BOTH arrays at 0x18010f904
    // and 0x18010f90e -- NOT SetSize(0). The difference is load-bearing:
    // SetSize(0) keeps the buffer (its grow test can never fire with n == 0),
    // so the old spelling released every Point String and then LEAKED the
    // array buffer on every PointList teardown. Corrected while registering
    // 0x180071600 (batch D2, 2026-09-12).
    PointArrayDtor(&self->_pointsB);
    PointArrayDtor(&self->_pointsA);
    self->_name2.~String();
    self->_name1.~String();
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

    // ENDGAME B.3.1 step 2: `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`
    // (0x1800c9930) is a member now, so the name door is gone. The registry row
    // at that name registers the shim, and the shim is one line over this member.
    self->_name1.Serialize(&ser);
    self->_name2.Serialize(&ser);

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
            // `Point::name` keeps its raw-pointer type (see the struct), so the
            // member call is spelled over the same eight bytes.
            reinterpret_cast<System::String*>(&p->name)->Serialize(&ser);
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
    // ---- CUTOVER CLOSURE BATCH D2 "helpers-high" (2026-09-12) --------------
    // Unexported in both builds, so RegisterRuntimeAddr only. All three halves
    // of the pair are registered together: at cutover retail's rel32 from
    // 0x180071730 / 0x180071600 to 0x180071890 will not exist.
    RegisterRuntimeAddr  (0x180071730ull, (void*)&RtPathfinding_PointArray_SetSize);
    RegisterRuntimeAddr  (0x180071890ull, (void*)&RtPathfinding_PointArray_ReleaseRange);
    RegisterRuntimeAddr  (0x180071600ull, (void*)&RtPathfinding_PointArray_Dtor);
    return n;
}
