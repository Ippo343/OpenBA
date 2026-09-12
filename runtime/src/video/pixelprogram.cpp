// ============================================================================
// runtime/src/video/pixelprogram.cpp — Video::PixelProgram, the whole class
// (original file: code\Runtime\Video\PixelProgram.h + ShaderProgram_win.cpp)
// ============================================================================
//
// ??_7PixelProgram@Video@@6B@ (retail 0x180157ac0), nine slots:
//
//   0  deleting dtor        0x18004eb90  runtime/src/object/deleting_dtor.cpp
//   1  TypeName             0x18004e8e0  here
//   2  Type                 0x18004e8c0  here
//   3  GetFileVersion       0x1800f3a80  here  (returns 5)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f3a90  here
//   6  Reconstruct          0x18004ea60  here
//   7  Destruct             0x18004ea90  here
//   8  Construct            0x18004ea80  here
//
// plus ??0PixelProgram@Video@@QEAA@XZ (0x1800f3950, the factory step at
// 0x18004ea2f), ~PixelProgram (0x1800f3a00), the three Platform* helpers and
// GetData / Bind.
//
// PixelProgram is VertexProgram's twin: same 0x58 layout, same five-vtable
// ctor, same "base Serialize then a dead v0 block" shape. The THREE things
// that differ, and they are the three that would be silently wrong if this file
// were written by copying vertexprogram.cpp:
//
//   1. the DeviceData slot is +0x80 (CreatePixelProgramData), not +0x78;
//   2. the ExternalPtr the v0 block reads and drops is
//      ExternalPtr<VertexProgram> (0x1800542a0), not ExternalPtr<VertexLayout>;
//   3. the v0 block's four temporaries are different types in a different
//      order — see SerializeLegacyV0 below.
//
// Everything said in vertexprogram.cpp about the v0 block applies here too:
// it can only run when reading a pre-v1 file, it keeps nothing, and the two
// unexported helpers it needs (0x1800547e0 / 0x180054920 for the first array,
// 0x180054a20 / 0x180054ba0 for the fourth) are reached through ResolveAddr and
// resolve to nothing once the DLL is gone.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/video/ShaderProgram.h"

// standalone/resolve.h is DELIBERATELY NOT INCLUDED — it drags in <windows.h>,
// and <wingdi.h> declares a function called `AlphaBlend`, which would make
// Video::RenderState::AlphaBlend ambiguous at every use. Same local declaration
// runtime/src/collection/array.cpp and runtime/src/object/deleting_dtor.cpp
// carry, for the same PLAN.md §4.1 reason; the linker checks the two spellings.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

extern "C" void  RtVideo_ShaderProgram_ConstructBody(void* self);   // shaderprogram.cpp
extern "C" void  RtVideo_ShaderProgram_DestroyArrays(void* self);   // shaderprogram.cpp
extern "C" bool  RtVideo_ShaderProgram_Serialize(void* self, void* stream);

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using rt::video::PixelProgram;
namespace ddvt = rt::video::devicedata_vtbl;
namespace pdvt = rt::video::programdata_vtbl;

void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }
void* ShaderProgramVTable()    { return RT_DATA_SLOT(void*, "??_7ShaderProgram@Video@@6B@"); }
void* PixelProgramVTable()     { return RT_DATA_SLOT(void*, "??_7PixelProgram@Video@@6B@"); }
const uint8_t* PixelProgramTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@PixelProgram@Video@@0VBaseTypeInfo@Object@@A");
}
void* DeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

using ScalarFn    = void (*)(ReaderWriter*, void*, const char*);
using VersionFn   = int  (*)(ReaderWriter*);
using SetSizeFn   = void (*)(void*, int);
using StrSerFn    = void (*)(void* str, Serializer* ser);
using LoopFn      = void (*)(Serializer* ser, int count, void* data);
using DestroyFn   = void (*)(void* ignoredThis, void* base, int count);
using ExtPtrSerFn = void (*)(void* field, Serializer* ser);
using MemFreeFn   = void (*)(void*);

inline void RwInt(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, p, nullptr);
}
inline void RwBool(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8])(rw, p, nullptr);
}

struct ArrayHdr { void* data; int32_t size; int32_t capacity; };

void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

// ?PlatformConstruct@PixelProgram@Video@@AEAAXXZ (0x1800f3e40)
void PlatformConstruct(PixelProgram* self) {
    void* dd = DeviceData();
    if (dd == nullptr) { self->_data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    self->_data = reinterpret_cast<CreateFn>(vt[ddvt::kCreatePixelProgramData / 8])(dd, self);
}
// ?PlatformCreate@PixelProgram@Video@@AEAA_NXZ (0x1800f3eb0)
bool PlatformCreate(PixelProgram* self) {
    using Fn = bool (*)(void*);
    void** vt = *reinterpret_cast<void***>(self->_data);
    return reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(self->_data);
}
// ?PlatformDestruct@PixelProgram@Video@@AEAAXXZ (0x1800f3e90)
void PlatformDestruct(PixelProgram* self) {
    if (self->_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(self->_data, 1);
}

void ConstructBody(PixelProgram* self) {
    RtVideo_ShaderProgram_ConstructBody(self);
    *reinterpret_cast<void**>(self) = PixelProgramVTable();
    PlatformConstruct(self);                            // +0x50
}

// ---- the version-0 legacy block -------------------------------------------
void SerializeLegacyV0(Serializer* ser) {
    ReaderWriter* rw = ser->readerWriter;
    const bool reading = (ser->stream->direction == rt::io::kDirectionReading);

    void*    droppedVertexProgram = nullptr;   // ExternalPtr<VertexProgram>
    ArrayHdr strings8 = { nullptr, 0, 0 };     // stride 8, one String each
    ArrayHdr namesA   = { nullptr, 0, 0 };     // Array<System::String>
    ArrayHdr namesB   = { nullptr, 0, 0 };     // Array<System::String>
    ArrayHdr samplers = { nullptr, 0, 0 };     // stride 0x10 {String, int, 3 bools}
    int32_t  n = 0;

    RtAddr<ExtPtrSerFn>(0x1800542a0ull)(&droppedVertexProgram, ser);

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x1800547e0ull)(&strings8, n);
    {
        uint8_t* e = static_cast<uint8_t*>(strings8.data);
        for (int32_t i = 0; i < n; ++i, e += 8)
            RT_EXPORT_ONCE(StrSerFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(e, ser);
    }

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180029a80ull)(&namesA, n);
    RtAddr<LoopFn>(0x180029bb0ull)(ser, n, namesA.data);

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180029a80ull)(&namesB, n);
    RtAddr<LoopFn>(0x180029bb0ull)(ser, n, namesB.data);

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180054a20ull)(&samplers, n);
    {
        uint8_t* e = static_cast<uint8_t*>(samplers.data);
        for (int32_t i = 0; i < n; ++i, e += 0x10) {
            RT_EXPORT_ONCE(StrSerFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(e + 0x00, ser);
            RwInt (rw, e + 0x08);
            RwBool(rw, e + 0x0c);
            RwBool(rw, e + 0x0d);
            RwBool(rw, e + 0x0e);
        }
    }

    // Teardown, in retail's order: samplers, namesB, namesA, strings8.
    RtAddr<DestroyFn>(0x180054ba0ull)(nullptr, samplers.data, samplers.size);
    MemFree(samplers.data); samplers = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180011010ull)(nullptr, namesB.data, namesB.size);
    MemFree(namesB.data);   namesB   = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180011010ull)(nullptr, namesA.data, namesA.size);
    MemFree(namesA.data);   namesA   = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180054920ull)(nullptr, strings8.data, strings8.size);
    MemFree(strings8.data); strings8 = ArrayHdr{ nullptr, 0, 0 };
    (void)samplers; (void)namesA; (void)namesB; (void)strings8; (void)droppedVertexProgram;
}

}  // namespace

extern "C" uint32_t RtVideo_PixelProgram_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, PixelProgramTypeInfo(), sizeof v); return v;
}
extern "C" uint32_t RtVideo_PixelProgram_TypeId() {
    uint32_t v = 0; std::memcpy(&v, PixelProgramTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtVideo_PixelProgram_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, PixelProgramTypeInfo() + 8, sizeof v); return v;
}
// .rdata literals at 0x180157a50 / 0x180157a28 / 0x180157a38.
extern "C" const char* RtVideo_PixelProgram_StaticTypeName() { return "PixelProgram"; }
extern "C" const char* RtVideo_PixelProgram_ParentTypeName() { return "ShaderProgram"; }
extern "C" const char* RtVideo_PixelProgram_FileExtension()  { return "srsl"; }
extern "C" int32_t     RtVideo_PixelProgram_GetFileVersion(const void*) { return 5; }

extern "C" void* RtVideo_PixelProgram_Ctor(void* selfv) {
    ConstructBody(static_cast<PixelProgram*>(selfv)); return selfv;
}
extern "C" void* RtVideo_PixelProgram_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<PixelProgram*>(selfv)); return selfv;
}
extern "C" void* RtVideo_PixelProgram_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<PixelProgram*>(selfv)); return selfv;
}
extern "C" void RtVideo_PixelProgram_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1PixelProgram@Video@@UEAA@XZ (0x1800f3a00) — ~ShaderProgram inlined.
extern "C" void RtVideo_PixelProgram_Dtor(void* selfv) {
    PixelProgram* self = static_cast<PixelProgram*>(selfv);
    void** vp = reinterpret_cast<void**>(self);
    *vp = PixelProgramVTable();
    PlatformDestruct(self);
    *vp = ShaderProgramVTable();
    RtVideo_ShaderProgram_DestroyArrays(self);
    *vp = CompiledExternalVTable();
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

extern "C" const void* RtVideo_PixelProgram_GetData(const void* selfv) {
    return static_cast<const PixelProgram*>(selfv)->_data;
}
// ?Bind@PixelProgram@Video@@QEBAXAEAVContext@2@H@Z (0x1800f3ed0)
extern "C" void RtVideo_PixelProgram_Bind(const void* selfv, void* context, int index) {
    void* data = static_cast<const PixelProgram*>(selfv)->_data;
    using Fn = void (*)(void*, void*, int);
    void** vt = *reinterpret_cast<void***>(data);
    reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(data, context, index);
}

// See vertexprogram.cpp: `AEAA` is private, not unexported.
extern "C" void RtVideo_PixelProgram_PlatformConstructExport(void* selfv) {
    PlatformConstruct(static_cast<PixelProgram*>(selfv));
}
extern "C" bool RtVideo_PixelProgram_PlatformCreateExport(void* selfv) {
    return PlatformCreate(static_cast<PixelProgram*>(selfv));
}
extern "C" void RtVideo_PixelProgram_PlatformDestructExport(void* selfv) {
    PlatformDestruct(static_cast<PixelProgram*>(selfv));
}

// ?Serialize@PixelProgram@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f3a90)
extern "C" bool RtVideo_PixelProgram_Serialize(void* selfv, void* streamv) {
    PixelProgram* self = static_cast<PixelProgram*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    RtVideo_ShaderProgram_Serialize(self, stream);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 1);
    ser.readerWriter = rw;

    if (reinterpret_cast<VersionFn>(rw->vtable[rt::io::rw_vtbl::kGetVersion / 8])(rw) == 0)
        SerializeLegacyV0(&ser);

    if (stream->direction == rt::io::kDirectionReading)
        PlatformCreate(self);          // retail derefs _data unguarded

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtVideoPixelProgramRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@PixelProgram@Video@@UEBAIXZ",        (void*)&RtVideo_PixelProgram_Type);     ++n;
    RegisterRuntimeAddr  (0x18004e8c0ull,                             (void*)&RtVideo_PixelProgram_Type);
    RegisterRuntimeExport("?TypeId@PixelProgram@Video@@SAIXZ",        (void*)&RtVideo_PixelProgram_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004e8d0ull,                             (void*)&RtVideo_PixelProgram_TypeId);
    RegisterRuntimeExport("?TypeName@PixelProgram@Video@@UEBAPEBDXZ", (void*)&RtVideo_PixelProgram_TypeName); ++n;
    RegisterRuntimeAddr  (0x18004e8e0ull,                             (void*)&RtVideo_PixelProgram_TypeName);
    RegisterRuntimeExport("?_TypeName@PixelProgram@Video@@CAPEBDXZ",       (void*)&RtVideo_PixelProgram_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e8a0ull,                                  (void*)&RtVideo_PixelProgram_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@PixelProgram@Video@@CAPEBDXZ", (void*)&RtVideo_PixelProgram_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e8b0ull,                                  (void*)&RtVideo_PixelProgram_ParentTypeName);
    RegisterRuntimeExport("?_FileExtension@PixelProgram@Video@@CAPEBDXZ",  (void*)&RtVideo_PixelProgram_FileExtension); ++n;
    RegisterRuntimeAddr  (0x18004e890ull,                                  (void*)&RtVideo_PixelProgram_FileExtension);
    RegisterRuntimeExport("?GetFileVersion@PixelProgram@Video@@UEBAHXZ",   (void*)&RtVideo_PixelProgram_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f3a80ull,                                  (void*)&RtVideo_PixelProgram_GetFileVersion);

    RegisterRuntimeExport("??0PixelProgram@Video@@QEAA@XZ", (void*)&RtVideo_PixelProgram_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f3950ull,                   (void*)&RtVideo_PixelProgram_Ctor);
    RegisterRuntimeExport("??1PixelProgram@Video@@UEAA@XZ", (void*)&RtVideo_PixelProgram_Dtor); ++n;
    RegisterRuntimeAddr  (0x1800f3a00ull,                   (void*)&RtVideo_PixelProgram_Dtor);
    RegisterRuntimeExport("?Construct@PixelProgram@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_PixelProgram_Construct); ++n;
    RegisterRuntimeAddr  (0x18004ea80ull, (void*)&RtVideo_PixelProgram_Construct);
    RegisterRuntimeExport("?Reconstruct@PixelProgram@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_PixelProgram_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004ea60ull, (void*)&RtVideo_PixelProgram_Reconstruct);
    RegisterRuntimeExport("?Destruct@PixelProgram@Video@@UEAAXXZ", (void*)&RtVideo_PixelProgram_Destruct); ++n;
    RegisterRuntimeAddr  (0x18004ea90ull,                          (void*)&RtVideo_PixelProgram_Destruct);
    RegisterRuntimeExport("?Serialize@PixelProgram@Video@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtVideo_PixelProgram_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f3a90ull, (void*)&RtVideo_PixelProgram_Serialize);

    RegisterRuntimeExport("?GetData@PixelProgram@Video@@QEBAPEBVPixelProgramData@2@XZ",
                          (void*)&RtVideo_PixelProgram_GetData); ++n;
    RegisterRuntimeAddr  (0x18004eaa0ull, (void*)&RtVideo_PixelProgram_GetData);
    RegisterRuntimeExport("?Bind@PixelProgram@Video@@QEBAXAEAVContext@2@H@Z",
                          (void*)&RtVideo_PixelProgram_Bind); ++n;
    RegisterRuntimeAddr  (0x1800f3ed0ull, (void*)&RtVideo_PixelProgram_Bind);
    RegisterRuntimeExport("?PlatformConstruct@PixelProgram@Video@@AEAAXXZ",
                          (void*)&RtVideo_PixelProgram_PlatformConstructExport); ++n;
    RegisterRuntimeAddr  (0x1800f3e40ull, (void*)&RtVideo_PixelProgram_PlatformConstructExport);
    RegisterRuntimeExport("?PlatformCreate@PixelProgram@Video@@AEAA_NXZ",
                          (void*)&RtVideo_PixelProgram_PlatformCreateExport); ++n;
    RegisterRuntimeAddr  (0x1800f3eb0ull, (void*)&RtVideo_PixelProgram_PlatformCreateExport);
    RegisterRuntimeExport("?PlatformDestruct@PixelProgram@Video@@AEAAXXZ",
                          (void*)&RtVideo_PixelProgram_PlatformDestructExport); ++n;
    RegisterRuntimeAddr  (0x1800f3e90ull, (void*)&RtVideo_PixelProgram_PlatformDestructExport);
    return n;
}
