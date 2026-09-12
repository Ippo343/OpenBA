// ============================================================================
// runtime/src/video/videoprogram.cpp — Video::VideoProgram, the whole class
// ============================================================================
//
// ??_7VideoProgram@Video@@6B@ (retail 0x180157a70), nine slots:
//
//   0  deleting dtor        0x18004efb0  runtime/src/object/deleting_dtor.cpp row 51
//   1  TypeName             0x18004ec50  here
//   2  Type                 0x18004ec30  here
//   3  GetFileVersion       0x1800f46a0  here  (returns 1)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f46b0  here
//   6  Reconstruct          0x18004edd0  here
//   7  Destruct             0x18004ee00  here
//   8  Construct            0x18004edf0  here
//
// plus the default ctor ??0VideoProgram@Video@@QEAA@XZ (0x1800f45b0), which the
// factory steps call (rt_factories_video.inc), ~VideoProgram (0x1800f4650) and
// the three private Platform* helpers.
//
// Layout (sizeof 0x48, from CreateNewObject's Allocate(0x48, 8)):
//   +0x00  Object::External             (0x20)
//   +0x20  ExternalPtr<VertexLayout>    _vertexLayout
//   +0x28  ExternalPtr<VertexProgram>   _vertexProgram
//   +0x30  ExternalPtr<PixelProgram>    _pixelProgram
//   +0x38  ExternalPtr<RenderState>     _renderState
//   +0x40  VideoProgramData*            _data   (dx11/: our DeviceData slot 0x90 creates it)
//
// ctor == Construct minus the null test: three vtable stamps, zero +0x08..+0x3f,
// then PlatformConstruct: `_data = Device::_singleton->deviceData ? deviceData->
// vtable[0x90/8](deviceData, this) : nullptr`.
// Serialize: build the {stream, readerWriter} pair, serialize the four
// ExternalPtrs (four 748-byte instantiations, object/external_ptr_serialize.cpp),
// then if any of them is non-null and we are reading, PlatformCreate =
// `_data->vtable[1](_data)`; finally delete the reader/writer.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"

namespace {

struct VideoProgram {
    RtObject::External base;            // +0x00
    void* _vertexLayout;                // +0x20
    void* _vertexProgram;               // +0x28
    void* _pixelProgram;                // +0x30
    void* _renderState;                 // +0x38
    void* _data;                        // +0x40  Video::VideoProgramData*
};
static_assert(sizeof(VideoProgram) == 0x48, "Video::VideoProgram is 0x48 bytes");
static_assert(offsetof(VideoProgram, _vertexLayout) == 0x20, "+0x20");
static_assert(offsetof(VideoProgram, _data) == 0x40, "+0x40");

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;

void* BaseVTable()         { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()     { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VideoProgramVTable() { return RT_DATA_SLOT(void*, "??_7VideoProgram@Video@@6B@"); }
const uint8_t* VideoProgramTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@VideoProgram@Video@@0VBaseTypeInfo@Object@@A");
}
// `?_singleton@Device@Video@@` is the ADDRESS of the Device* static; +0x100 of
// the Device is its DeviceData* (runtime/types/video/Device.h, dx11/src).
void* DeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}
constexpr size_t kDeviceDataCreateVideoProgramData = 0x90;   // DeviceData vtable slot 18

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

// ?PlatformConstruct@VideoProgram@Video@@AEAAXXZ (0x1800f47a0), also the tail
// of the ctor and of Construct.
void PlatformConstruct(VideoProgram* self) {
    void* dd = DeviceData();
    if (dd == nullptr) { self->_data = nullptr; return; }
    using CreateFn = void* (*)(void* deviceData, void* program);
    void** vt = *reinterpret_cast<void***>(dd);
    self->_data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreateVideoProgramData / 8])(dd, self);
}
// ?PlatformCreate@VideoProgram@Video@@AEAAXXZ (0x1800f4810): _data->vtable[1](_data)
void PlatformCreate(VideoProgram* self) {
    using Fn = void (*)(void*);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[1])(self->_data);
}
// ?PlatformDestruct@VideoProgram@Video@@AEAAXXZ (0x1800f47f0): _data->vtable[0](_data, 1)
void PlatformDestruct(VideoProgram* self) {
    if (self->_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[0])(self->_data, 1);
}

void ConstructBody(VideoProgram* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;   // +0x08
    self->base._refs._data     = nullptr;   // +0x10
    self->base._refs._size     = 0;         // +0x18
    self->base._refs._capacity = 0;
    *vp = VideoProgramVTable();
    self->_vertexLayout  = nullptr;         // +0x20
    self->_vertexProgram = nullptr;         // +0x28
    self->_pixelProgram  = nullptr;         // +0x30
    self->_renderState   = nullptr;         // +0x38
    PlatformConstruct(self);                // +0x40
}

}  // namespace

extern "C" uint32_t RtVideo_VideoProgram_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, VideoProgramTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtVideo_VideoProgram_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, VideoProgramTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtVideo_VideoProgram_GetFileVersion(const void*) { return 1; }

extern "C" void* RtVideo_VideoProgram_Ctor(void* selfv) {
    ConstructBody(static_cast<VideoProgram*>(selfv));
    return selfv;
}
extern "C" void* RtVideo_VideoProgram_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<VideoProgram*>(selfv));
    return selfv;
}
extern "C" void* RtVideo_VideoProgram_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<VideoProgram*>(selfv));
    return selfv;
}
extern "C" void RtVideo_VideoProgram_Destruct(void* selfv) {
    CallSlot0(selfv, 0);
}
extern "C" void RtVideo_VideoProgram_Dtor(void* selfv) {
    VideoProgram* self = static_cast<VideoProgram*>(selfv);
    *reinterpret_cast<void**>(self) = VideoProgramVTable();
    PlatformDestruct(self);
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

extern "C" bool RtVideo_VideoProgram_Serialize(void* selfv, void* streamv) {
    VideoProgram* self = static_cast<VideoProgram*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using ExtPtrSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<ExtPtrSerFn>(0x180055090ull)(&self->_vertexLayout,  &ser);   // ExternalPtr<VertexLayout>
    RtAddr<ExtPtrSerFn>(0x1800542a0ull)(&self->_vertexProgram, &ser);   // ExternalPtr<VertexProgram>
    RtAddr<ExtPtrSerFn>(0x180055380ull)(&self->_pixelProgram,  &ser);   // ExternalPtr<PixelProgram>
    RtAddr<ExtPtrSerFn>(0x180055670ull)(&self->_renderState,   &ser);   // ExternalPtr<RenderState>

    if ((self->_vertexLayout || self->_vertexProgram || self->_pixelProgram || self->_renderState)
        && stream->direction == 0) {
        PlatformCreate(self);                       // retail derefs _data unguarded
    }
    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtVideoProgramRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@VideoProgram@Video@@UEBAIXZ",          (void*)&RtVideo_VideoProgram_Type);           ++n;
    RegisterRuntimeAddr  (0x18004ec30ull,                               (void*)&RtVideo_VideoProgram_Type);
    RegisterRuntimeExport("?TypeName@VideoProgram@Video@@UEBAPEBDXZ",   (void*)&RtVideo_VideoProgram_TypeName);       ++n;
    RegisterRuntimeAddr  (0x18004ec50ull,                               (void*)&RtVideo_VideoProgram_TypeName);
    RegisterRuntimeExport("?GetFileVersion@VideoProgram@Video@@UEBAHXZ",(void*)&RtVideo_VideoProgram_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f46a0ull,                               (void*)&RtVideo_VideoProgram_GetFileVersion);
    RegisterRuntimeExport("??0VideoProgram@Video@@QEAA@XZ",             (void*)&RtVideo_VideoProgram_Ctor);           ++n;
    RegisterRuntimeAddr  (0x1800f45b0ull,                               (void*)&RtVideo_VideoProgram_Ctor);
    RegisterRuntimeExport("?Construct@VideoProgram@Video@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtVideo_VideoProgram_Construct);   ++n;
    RegisterRuntimeAddr  (0x18004edf0ull,                                                 (void*)&RtVideo_VideoProgram_Construct);
    RegisterRuntimeExport("?Reconstruct@VideoProgram@Video@@UEAAPEAVExternal@Object@@XZ", (void*)&RtVideo_VideoProgram_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004edd0ull,                                                 (void*)&RtVideo_VideoProgram_Reconstruct);
    RegisterRuntimeExport("?Destruct@VideoProgram@Video@@UEAAXXZ",      (void*)&RtVideo_VideoProgram_Destruct);       ++n;
    RegisterRuntimeAddr  (0x18004ee00ull,                               (void*)&RtVideo_VideoProgram_Destruct);
    RegisterRuntimeExport("??1VideoProgram@Video@@UEAA@XZ",             (void*)&RtVideo_VideoProgram_Dtor);           ++n;
    RegisterRuntimeAddr  (0x1800f4650ull,                               (void*)&RtVideo_VideoProgram_Dtor);
    RegisterRuntimeExport("?Serialize@VideoProgram@Video@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtVideo_VideoProgram_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f46b0ull,                                           (void*)&RtVideo_VideoProgram_Serialize);
    return n;
}
