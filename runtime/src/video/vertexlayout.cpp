// ============================================================================
// runtime/src/video/vertexlayout.cpp — Video::VertexLayout, the whole class
// ============================================================================
//
// ??_7VertexLayout@Video@@6B@ (retail 0x180157ba0), nine slots:
//
//   0  deleting dtor        0x18004e000  runtime/src/object/deleting_dtor.cpp row 52
//   1  TypeName             0x18004dcd0  here
//   2  Type                 0x18004dcb0  here
//   3  GetFileVersion       0x1800f5e60  here  (returns 1)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f5e70  here
//   6  Reconstruct          0x18004de50  here
//   7  Destruct             0x18004de80  here
//   8  Construct            0x18004de70  here
//
// plus ??0VertexLayout@Video@@QEAA@XZ (0x1800f5d50), ~VertexLayout (0x1800f5df0)
// and the element serializer loop 0x1800563c0. GetElementCount / GetElement /
// GetStreamCount / Add / BeginAdd / EndAdd are non-virtual and not on the boot
// path; they are not ported here.
//
// Layout (sizeof 0x48, from the eh_vector iterator stride in the deleting dtor):
//   +0x00  Object::CompiledExternal (== External, 0x20; the ctor stamps
//          ??_7CompiledExternal@Object@@6B@ between External's and its own)
//   +0x20  Array<Element>    _elements   {data, size +0x28, cap +0x2c}
//   +0x30  VertexLayoutData* _data       (DeviceData slot 0x70 creates it)
//   +0x38  Array<uint8_t>    _platform   {data, size +0x40, cap +0x44}
//
// Element (0x20, collection/array.cpp kSpec_VertexElement):
//   +0x00 String name  +0x08 int stream  +0x0c int usage  +0x10 uint usageIndex
//   +0x14 int format   +0x18 uint offset +0x1c pad
//
// Serialize (0x1800f5e70): build the {stream, readerWriter} pair;
//   int n = _elements.size; rw->Int(&n); if reading SetSize(_elements, n) (0x180056240)
//   0x1800563c0(&ser, n, _elements.data)                 element loop, below
//   PlatformSerialize inlined: int m = _platform.size; rw->Int(&m);
//     if reading SetSize(_platform, m) (0x180029630); rw->UCharArray(m, _platform.data)
//   if reading: _data->vtable[2](_data)                  PlatformCreate
//   delete rw
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/system/String.h"

namespace {

struct VertexElement {
    void*    name;        // +0x00 System::String (refcounted)
    int32_t  stream;      // +0x08
    int32_t  usage;       // +0x0c
    uint32_t usageIndex;  // +0x10
    int32_t  format;      // +0x14
    uint32_t offset;      // +0x18
    uint32_t _pad1c;      // +0x1c
};
static_assert(sizeof(VertexElement) == 0x20, "VertexLayout::Element is 0x20 bytes");

struct VertexLayout {
    RtObject::External               base;       // +0x00
    Collection::Array<VertexElement> _elements;  // +0x20
    void*                            _data;      // +0x30  Video::VertexLayoutData*
    Collection::Array<uint8_t>       _platform;  // +0x38
};
static_assert(sizeof(VertexLayout) == 0x48, "Video::VertexLayout is 0x48 bytes");
static_assert(offsetof(VertexLayout, _elements) == 0x20, "+0x20");
static_assert(offsetof(VertexLayout, _data) == 0x30, "+0x30");
static_assert(offsetof(VertexLayout, _platform) == 0x38, "+0x38");

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;

void* BaseVTable()             { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()         { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }
void* VertexLayoutVTable()     { return RT_DATA_SLOT(void*, "??_7VertexLayout@Video@@6B@"); }
const uint8_t* VertexLayoutTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@VertexLayout@Video@@0VBaseTypeInfo@Object@@A");
}
void* DeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}
constexpr size_t kDeviceDataCreateVertexLayoutData = 0x70;   // DeviceData vtable slot 14

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}
template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

void PlatformConstruct(VertexLayout* self) {                // 0x1800f5f90
    void* dd = DeviceData();
    if (dd == nullptr) { self->_data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    self->_data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreateVertexLayoutData / 8])(dd, self);
}
void PlatformCreate(VertexLayout* self) {                   // 0x1800f6060: _data->vtable[2](_data)
    using Fn = void (*)(void*);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[2])(self->_data);
}
void PlatformDestruct(VertexLayout* self) {                 // 0x1800f5fd0
    if (self->_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[0])(self->_data, 1);
}

// ??0VertexLayout@Video@@QEAA@XZ (0x1800f5d50); Construct is the same with a null test.
void ConstructBody(VertexLayout* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;   // +0x08
    self->base._refs._data     = nullptr;   // +0x10
    self->base._refs._size     = 0;         // +0x18
    self->base._refs._capacity = 0;
    *vp = CompiledExternalVTable();
    *vp = VertexLayoutVTable();
    self->_elements._data     = nullptr;    // +0x20
    self->_elements._size     = 0;          // +0x28
    self->_elements._capacity = 0;
    self->_platform._data     = nullptr;    // +0x38
    self->_platform._size     = 0;          // +0x40
    self->_platform._capacity = 0;
    PlatformConstruct(self);                // +0x30
}

using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
using ArrayFn  = void (*)(ReaderWriter*, int, void*, const char*);

}  // namespace

extern "C" uint32_t RtVideo_VertexLayout_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, VertexLayoutTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtVideo_VertexLayout_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, VertexLayoutTypeInfo() + 8, sizeof v); return v;
}
// 0x1800f5e60 is `MOV EAX,3 ; RET`. This port said 1 until the 2026-09-12
// cutover walk: DLL-present the DLL's own vtable answered, so nothing noticed;
// DLL-absent every MeshLayout\*.rsc load failed External::Load's version
// check (badVer=65), every VideoProgram lost its VertexLayout, and
// Renderer::AddDrawItem silently dropped every draw item -- the black window.
extern "C" int32_t RtVideo_VertexLayout_GetFileVersion(const void*) { return 3; }

extern "C" void* RtVideo_VertexLayout_Ctor(void* selfv) {
    ConstructBody(static_cast<VertexLayout*>(selfv)); return selfv;
}
extern "C" void* RtVideo_VertexLayout_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<VertexLayout*>(selfv)); return selfv;
}
extern "C" void* RtVideo_VertexLayout_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<VertexLayout*>(selfv)); return selfv;
}
extern "C" void RtVideo_VertexLayout_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1VertexLayout@Video@@UEAA@XZ (0x1800f5df0)
extern "C" void RtVideo_VertexLayout_Dtor(void* selfv) {
    VertexLayout* self = static_cast<VertexLayout*>(selfv);
    void** vp = reinterpret_cast<void**>(self);
    *vp = VertexLayoutVTable();
    PlatformDestruct(self);
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x1800112a0ull)(&self->_platform);    // Array<u8>::~Array
    RtAddr<ArrayDtorFn>(0x18004fa30ull)(&self->_elements);    // Array<Element>::~Array (releases names)
    *vp = CompiledExternalVTable();
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// 0x1800563c0 — the per-element loop: (Serializer*, int count, Element* data)
extern "C" void RtVideo_VertexLayout_SerializeElements(Serializer* ser, int count, void* data) {
    ReaderWriter* rw = ser->readerWriter;
    VertexElement* e = static_cast<VertexElement*>(data);
    using StrSerFn = void (*)(void* str, Serializer* ser);
    for (int i = 0; i < count; ++i, ++e) {
        RT_EXPORT_ONCE(StrSerFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(&e->name, ser);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->stream,     nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->usage,      nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &e->usageIndex, nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &e->format,     nullptr);
        reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &e->offset,     nullptr);
    }
}

extern "C" bool RtVideo_VertexLayout_Serialize(void* selfv, void* streamv) {
    VertexLayout* self = static_cast<VertexLayout*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using SetSizeFn = void (*)(void*, int);
    int32_t n = self->_elements._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);
    if (ser.stream->direction == 0) RtAddr<SetSizeFn>(0x180056240ull)(&self->_elements, n);
    using ElemSerFn = void (*)(Serializer*, int, void*);
    RtAddr<ElemSerFn>(0x1800563c0ull)(&ser, n, self->_elements._data);

    // PlatformSerialize (0x1800f5ff0), inlined by retail
    int32_t m = self->_platform._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &m, nullptr);
    if (ser.stream->direction == 0) RtAddr<SetSizeFn>(0x180029630ull)(&self->_platform, m);
    reinterpret_cast<ArrayFn>(rw->vtable[rt::io::rw_vtbl::kArrayUChar / 8])(rw, m, self->_platform._data, nullptr);

    if (stream->direction == 0) PlatformCreate(self);       // retail derefs _data unguarded
    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtVideoVertexLayoutRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@VertexLayout@Video@@UEBAIXZ",           (void*)&RtVideo_VertexLayout_Type);           ++n;
    RegisterRuntimeAddr  (0x18004dcb0ull,                                (void*)&RtVideo_VertexLayout_Type);
    RegisterRuntimeExport("?TypeName@VertexLayout@Video@@UEBAPEBDXZ",    (void*)&RtVideo_VertexLayout_TypeName);       ++n;
    RegisterRuntimeAddr  (0x18004dcd0ull,                                (void*)&RtVideo_VertexLayout_TypeName);
    RegisterRuntimeExport("?GetFileVersion@VertexLayout@Video@@UEBAHXZ", (void*)&RtVideo_VertexLayout_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f5e60ull,                                (void*)&RtVideo_VertexLayout_GetFileVersion);
    RegisterRuntimeExport("??0VertexLayout@Video@@QEAA@XZ",              (void*)&RtVideo_VertexLayout_Ctor);           ++n;
    RegisterRuntimeAddr  (0x1800f5d50ull,                                (void*)&RtVideo_VertexLayout_Ctor);
    RegisterRuntimeExport("?Construct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtVideo_VertexLayout_Construct);   ++n;
    RegisterRuntimeAddr  (0x18004de70ull,                                                 (void*)&RtVideo_VertexLayout_Construct);
    RegisterRuntimeExport("?Reconstruct@VertexLayout@Video@@UEAAPEAVExternal@Object@@XZ", (void*)&RtVideo_VertexLayout_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004de50ull,                                                 (void*)&RtVideo_VertexLayout_Reconstruct);
    RegisterRuntimeExport("?Destruct@VertexLayout@Video@@UEAAXXZ",       (void*)&RtVideo_VertexLayout_Destruct);       ++n;
    RegisterRuntimeAddr  (0x18004de80ull,                                (void*)&RtVideo_VertexLayout_Destruct);
    RegisterRuntimeExport("??1VertexLayout@Video@@UEAA@XZ",              (void*)&RtVideo_VertexLayout_Dtor);           ++n;
    RegisterRuntimeAddr  (0x1800f5df0ull,                                (void*)&RtVideo_VertexLayout_Dtor);
    RegisterRuntimeExport("?Serialize@VertexLayout@Video@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtVideo_VertexLayout_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f5e70ull,                                           (void*)&RtVideo_VertexLayout_Serialize);
    RegisterRuntimeAddr  (0x1800563c0ull,                                (void*)&RtVideo_VertexLayout_SerializeElements); ++n;
    return n;
}
