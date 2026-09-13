// ============================================================================
// runtime/src/video/vertexprogram.cpp — Video::VertexProgram, the whole class
// (original file: code\Runtime\Video\VertexProgram.h + ShaderProgram_win.cpp)
// ============================================================================
//
// ??_7VertexProgram@Video@@6B@ (retail 0x180157b10), nine slots:
//
//   0  deleting dtor        0x18004e810  runtime/src/object/deleting_dtor.cpp
//   1  TypeName             0x18004e560  here
//   2  Type                 0x18004e540  here
//   3  GetFileVersion       0x1800f6230  here  (returns 5)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f6240  here
//   6  Reconstruct          0x18004e6e0  here
//   7  Destruct             0x18004e710  here
//   8  Construct            0x18004e700  here
//
// plus ??0VertexProgram@Video@@QEAA@XZ (0x1800f6100) — the ctor the cutover
// gate died on, called from the factory step at 0x18004e6af
// (rt_factories_video.inc) — ~VertexProgram (0x1800f61b0), the three private
// Platform* helpers and the non-virtual GetData / Bind.
//
// Layout: runtime/types/video/ShaderProgram.h (ShaderProgram 0x50 + _data, 0x58).
//
// ---------------------------------------------------------------------------
// THE CTOR IS ShaderProgram's, INLINED — AND THE VTABLE ORDER MATTERS
// ---------------------------------------------------------------------------
// 0x1800f6100 stamps FIVE vtables in order: Base, External, CompiledExternal,
// **ShaderProgram** and only then VertexProgram. The fourth stamp is not
// decoration — an object that faults between the fourth and fifth store is
// destroyed as a ShaderProgram, and that is the shape retail's unwinder
// expects. Then PlatformConstruct: DeviceData slot +0x78.
//
// ---------------------------------------------------------------------------
// Serialize (0x1800f6240): THE BASE CALL, THEN A DEAD-ON-ARRIVAL v0 BLOCK
// ---------------------------------------------------------------------------
//     ShaderProgram::Serialize(this, stream)        0x1800f5110
//     rw = encoding->Create{Reader,Writer}(&ser, 1)
//     if (rw->GetVersion() == 0) { ...the legacy block... }
//     if (reading) _data->vtable[1](_data)          PlatformCreate
//     delete rw
//
// The legacy block reads FOUR temporary arrays and an ExternalPtr off the
// stream and then throws every one of them away without looking at a single
// field — it exists purely to advance a version-0 stream past data this build
// no longer stores on the object. It can only run on the READ path of a file
// written by a pre-v1 build: BinaryWriter stamps the version it was constructed
// with, which is the literal 1 above, so on the write path GetVersion() is 1
// and the block is skipped unconditionally.
//
// It is ported anyway, and it is ported as retail wrote it — including the
// ExternalPtr<VertexLayout> that is read and dropped. Five of the helpers it
// calls are unexported internals reached through ResolveAddr, which is the same
// door runtime/src/io/package_file_serialize.cpp already holds open for
// 0x180029bb0:
//     0x1800567d0  SetSize, stride 0x18 {String, String, uint, int}
//     0x180056960  ~Array of that element
//     0x180056af0  SetSize, the second legacy element type
//     0x180056db0  the per-element serialize loop for it
//     0x180056cb0  ~Array of it
//
// CORRECTION (cutover closure batch D1, 2026-09-12). This banner used to read
// "five ... UNPORTED internals ... with the DLL gone those five resolve to
// nothing, so a version-0 asset would call address zero". ALL FIVE ARE NOW
// PORTED AND REGISTERED — 0x1800567d0 / 0x180056af0 as `kSpec_VtxElement18` and
// `kSpec_VtxStream18` in runtime/src/collection/array.cpp, 0x180056960 and
// 0x180056cb0 in array_destroy.cpp, and 0x180056db0 at the bottom of THIS file.
// ResolveAddr is registry-first, so the five calls above already land on our
// bodies, and they will keep working with the DLL gone. What remains true is
// that the block is unreachable for any shipped asset (every one is version 1).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
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
using rt::video::VertexProgram;
namespace ddvt = rt::video::devicedata_vtbl;
namespace pdvt = rt::video::programdata_vtbl;

void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }
void* ShaderProgramVTable()    { return RT_DATA_SLOT(void*, "??_7ShaderProgram@Video@@6B@"); }
void* VertexProgramVTable()    { return RT_DATA_SLOT(void*, "??_7VertexProgram@Video@@6B@"); }
const uint8_t* VertexProgramTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@VertexProgram@Video@@0VBaseTypeInfo@Object@@A");
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

using ScalarFn  = void (*)(ReaderWriter*, void*, const char*);
using VersionFn = int  (*)(ReaderWriter*);
using SetSizeFn = void (*)(void*, int);
using LoopFn    = void (*)(Serializer* ser, int count, void* data);
using DestroyFn = void (*)(void* ignoredThis, void* base, int count);
using ExtPtrSerFn = void (*)(void* field, Serializer* ser);
using MemFreeFn = void (*)(void*);

inline void RwInt(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, p, nullptr);
}
inline void RwUInt(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p, nullptr);
}
inline void RwBool(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8])(rw, p, nullptr);
}

struct ArrayHdr { void* data; int32_t size; int32_t capacity; };

// Retail inlines EnterCriticalSection / Free / LeaveCriticalSection here; the
// exported ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0) is that
// sequence instruction for instruction, plus the null guard retail also does
// (runtime/src/collection/array.cpp "THE ALLOCATOR BOUNDARY").
void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

// ?PlatformConstruct@VertexProgram@Video@@AEAAXXZ (0x1800f65e0)
void PlatformConstruct(VertexProgram* self) {
    void* dd = DeviceData();
    if (dd == nullptr) { self->_data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    self->_data = reinterpret_cast<CreateFn>(vt[ddvt::kCreateVertexProgramData / 8])(dd, self);
}
// ?PlatformCreate@VertexProgram@Video@@AEAA_NXZ (0x1800f6640) — tail-jumps into
// the companion's slot 1, so it RETURNS whatever that returns.
bool PlatformCreate(VertexProgram* self) {
    using Fn = bool (*)(void*);
    void** vt = *reinterpret_cast<void***>(self->_data);
    return reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(self->_data);
}
// ?PlatformDestruct@VertexProgram@Video@@AEAAXXZ (0x1800f6620)
void PlatformDestruct(VertexProgram* self) {
    if (self->_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(self->_data, 1);
}

void ConstructBody(VertexProgram* self) {
    RtVideo_ShaderProgram_ConstructBody(self);          // through the ShaderProgram stamp
    *reinterpret_cast<void**>(self) = VertexProgramVTable();
    PlatformConstruct(self);                            // +0x50
}

// ---- the version-0 legacy block -------------------------------------------
// A version-0 file stored the vertex declaration on the program itself. Nothing
// here is kept; every array is destroyed and freed before the block returns.
void SerializeLegacyV0(Serializer* ser) {
    ReaderWriter* rw = ser->readerWriter;
    const bool reading = (ser->stream->direction == rt::io::kDirectionReading);

    void*    droppedLayout = nullptr;   // ExternalPtr<VertexLayout>, read and dropped
    ArrayHdr elements  = { nullptr, 0, 0 };   // stride 0x18 {String, String, uint, int}
    ArrayHdr streams   = { nullptr, 0, 0 };   // handled entirely by 0x180056db0
    ArrayHdr namesA    = { nullptr, 0, 0 };   // Array<System::String>
    ArrayHdr namesB    = { nullptr, 0, 0 };   // Array<System::String>
    int32_t  n = 0;

    RtAddr<ExtPtrSerFn>(0x180055090ull)(&droppedLayout, ser);   // ExternalPtr<VertexLayout>

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x1800567d0ull)(&elements, n);
    {
        uint8_t* e = static_cast<uint8_t*>(elements.data);
        for (int32_t i = 0; i < n; ++i, e += 0x18) {
            // B.3.1 bridge: untyped array storage, two System::Strings per element.
            reinterpret_cast<System::String*>(e + 0x00)->Serialize(ser);
            reinterpret_cast<System::String*>(e + 0x08)->Serialize(ser);
            RwUInt(rw, e + 0x10);
            RwInt (rw, e + 0x14);
        }
    }

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180056af0ull)(&streams, n);
    RtAddr<LoopFn>(0x180056db0ull)(ser, n, streams.data);

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180029a80ull)(&namesA, n);   // Array<String>::SetSize
    RtAddr<LoopFn>(0x180029bb0ull)(ser, n, namesA.data);          // Array<String> serialize loop

    n = 0;
    RwInt(rw, &n);
    if (reading) RtAddr<SetSizeFn>(0x180029a80ull)(&namesB, n);
    RtAddr<LoopFn>(0x180029bb0ull)(ser, n, namesB.data);

    uint8_t flag = 0;
    RwBool(rw, &flag);

    // Teardown, in retail's order: namesB, namesA, streams, elements.
    RtAddr<DestroyFn>(0x180011010ull)(nullptr, namesB.data, namesB.size);
    MemFree(namesB.data);   namesB  = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180011010ull)(nullptr, namesA.data, namesA.size);
    MemFree(namesA.data);   namesA  = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180056cb0ull)(nullptr, streams.data, streams.size);
    MemFree(streams.data);  streams = ArrayHdr{ nullptr, 0, 0 };
    RtAddr<DestroyFn>(0x180056960ull)(nullptr, elements.data, elements.size);
    MemFree(elements.data); elements = ArrayHdr{ nullptr, 0, 0 };
    (void)namesA; (void)namesB; (void)streams; (void)elements; (void)droppedLayout;
}

}  // namespace

extern "C" uint32_t RtVideo_VertexProgram_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, VertexProgramTypeInfo(), sizeof v); return v;
}
extern "C" uint32_t RtVideo_VertexProgram_TypeId() {
    uint32_t v = 0; std::memcpy(&v, VertexProgramTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtVideo_VertexProgram_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, VertexProgramTypeInfo() + 8, sizeof v); return v;
}
// .rdata literals at 0x180157a40 / 0x180157a28 / 0x180157a38.
extern "C" const char* RtVideo_VertexProgram_StaticTypeName() { return "VertexProgram"; }
extern "C" const char* RtVideo_VertexProgram_ParentTypeName() { return "ShaderProgram"; }
extern "C" const char* RtVideo_VertexProgram_FileExtension()  { return "srsl"; }
extern "C" int32_t     RtVideo_VertexProgram_GetFileVersion(const void*) { return 5; }

extern "C" void* RtVideo_VertexProgram_Ctor(void* selfv) {
    ConstructBody(static_cast<VertexProgram*>(selfv)); return selfv;
}
extern "C" void* RtVideo_VertexProgram_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<VertexProgram*>(selfv)); return selfv;
}
extern "C" void* RtVideo_VertexProgram_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<VertexProgram*>(selfv)); return selfv;
}
extern "C" void RtVideo_VertexProgram_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1VertexProgram@Video@@UEAA@XZ (0x1800f61b0) — ~ShaderProgram inlined.
extern "C" void RtVideo_VertexProgram_Dtor(void* selfv) {
    VertexProgram* self = static_cast<VertexProgram*>(selfv);
    void** vp = reinterpret_cast<void**>(self);
    *vp = VertexProgramVTable();
    PlatformDestruct(self);
    *vp = ShaderProgramVTable();
    RtVideo_ShaderProgram_DestroyArrays(self);
    *vp = CompiledExternalVTable();
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

extern "C" const void* RtVideo_VertexProgram_GetData(const void* selfv) {
    return static_cast<const VertexProgram*>(selfv)->_data;
}
// ?Bind@VertexProgram@Video@@QEBAXAEAVContext@2@H@Z (0x1800f6660): both
// arguments pass straight through to the companion's Bind slot.
extern "C" void RtVideo_VertexProgram_Bind(const void* selfv, void* context, int index) {
    void* data = static_cast<const VertexProgram*>(selfv)->_data;
    using Fn = void (*)(void*, void*, int);
    void** vt = *reinterpret_cast<void***>(data);
    reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(data, context, index);
}

// The three private Platform* helpers. `AEAA` means PRIVATE, not unexported —
// the DLL exports its private members (reference_aeaa_is_private_not_unexported.md)
// — so they get registrations of their own even though every caller in this
// file reaches the inlined copies above.
extern "C" void RtVideo_VertexProgram_PlatformConstructExport(void* selfv) {
    PlatformConstruct(static_cast<VertexProgram*>(selfv));
}
extern "C" bool RtVideo_VertexProgram_PlatformCreateExport(void* selfv) {
    return PlatformCreate(static_cast<VertexProgram*>(selfv));
}
extern "C" void RtVideo_VertexProgram_PlatformDestructExport(void* selfv) {
    PlatformDestruct(static_cast<VertexProgram*>(selfv));
}

// ?Serialize@VertexProgram@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f6240)
extern "C" bool RtVideo_VertexProgram_Serialize(void* selfv, void* streamv) {
    VertexProgram* self = static_cast<VertexProgram*>(selfv);
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

// ============================================================================
// The legacy Stream element serialize loop   0x180056db0   (batch D1)
// ============================================================================
// SerializeLegacyV0 above calls this by address. The element is
// {Collection::Array<Element> @+0x00, System::String* @+0x10}, stride 0x18 —
// the layout array.cpp's kSpec_VtxStream18 and array_destroy.cpp's
// 0x180056cb0 row independently agree on.
//
//   180056de0  n = elem->inner._size                      (e+0x08)
//   180056df6  kScalarInt(&n)                             — a LOCAL, not the field
//   180056dfc  if (ser->stream->direction == reading)
//   180056e0c      0x1800567d0(&elem->inner, n)           SetSize
//   180056e15  p = elem->inner._data                      (e+0x00)
//   180056e22  per inner element (stride 0x18):
//                 String::Serialize(p+0x00) / (p+0x08)
//                 kScalarUInt(p+0x10) / kScalarInt(p+0x14)
//   180056e6e  String::Serialize(elem+0x10)
//
// The count that drives the inner loop is the LOCAL read back at 0x180056e11,
// not `elem->inner._size` re-read after the SetSize. On the write path they are
// the same number; on the read path of a truncated stream they are not, and
// retail uses the local.
extern "C" void RtVideo_VertexProgram_SerializeStreams(void* serv, int count, void* basev) {
    if (count <= 0) return;                                  // 180056db0 TEST/JLE
    Serializer* ser = static_cast<Serializer*>(serv);
    uint8_t*    e   = static_cast<uint8_t*>(basev);

    for (int i = 0; i < count; ++i, e += 0x18) {
        int32_t n = *reinterpret_cast<int32_t*>(e + 0x08);   // 180056de0
        RwInt(ser->readerWriter, &n);                        // 180056df6

        if (ser->stream->direction == rt::io::kDirectionReading)   // 180056dfc
            RtAddr<SetSizeFn>(0x1800567d0ull)(e + 0x00, n);        // 180056e0c

        uint8_t* p = *reinterpret_cast<uint8_t**>(e + 0x00); // 180056e15
        for (int32_t k = 0; k < n; ++k, p += 0x18) {         // 180056e22
            // B.3.1 bridge: untyped array storage, two System::Strings per element.
            reinterpret_cast<System::String*>(p + 0x00)->Serialize(ser);
            reinterpret_cast<System::String*>(p + 0x08)->Serialize(ser);
            RwUInt(ser->readerWriter, p + 0x10);
            RwInt (ser->readerWriter, p + 0x14);
        }

        reinterpret_cast<System::String*>(e + 0x10)->Serialize(ser);   // B.3.1 bridge
    }
}

int RtVideoVertexProgramRegister() {
    int n = 0;

    // Batch D1: unexported in both builds, address door only.
    RegisterRuntimeAddr  (0x180056db0ull, (void*)&RtVideo_VertexProgram_SerializeStreams); ++n;

    RegisterRuntimeExport("?Type@VertexProgram@Video@@UEBAIXZ",        (void*)&RtVideo_VertexProgram_Type);     ++n;
    RegisterRuntimeAddr  (0x18004e540ull,                              (void*)&RtVideo_VertexProgram_Type);
    RegisterRuntimeExport("?TypeId@VertexProgram@Video@@SAIXZ",        (void*)&RtVideo_VertexProgram_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004e550ull,                              (void*)&RtVideo_VertexProgram_TypeId);
    RegisterRuntimeExport("?TypeName@VertexProgram@Video@@UEBAPEBDXZ", (void*)&RtVideo_VertexProgram_TypeName); ++n;
    RegisterRuntimeAddr  (0x18004e560ull,                              (void*)&RtVideo_VertexProgram_TypeName);
    RegisterRuntimeExport("?_TypeName@VertexProgram@Video@@CAPEBDXZ",       (void*)&RtVideo_VertexProgram_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e520ull,                                   (void*)&RtVideo_VertexProgram_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@VertexProgram@Video@@CAPEBDXZ", (void*)&RtVideo_VertexProgram_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e530ull,                                   (void*)&RtVideo_VertexProgram_ParentTypeName);
    RegisterRuntimeExport("?_FileExtension@VertexProgram@Video@@CAPEBDXZ",  (void*)&RtVideo_VertexProgram_FileExtension); ++n;
    RegisterRuntimeAddr  (0x18004e510ull,                                   (void*)&RtVideo_VertexProgram_FileExtension);
    RegisterRuntimeExport("?GetFileVersion@VertexProgram@Video@@UEBAHXZ",   (void*)&RtVideo_VertexProgram_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f6230ull,                                   (void*)&RtVideo_VertexProgram_GetFileVersion);

    RegisterRuntimeExport("??0VertexProgram@Video@@QEAA@XZ", (void*)&RtVideo_VertexProgram_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f6100ull,                    (void*)&RtVideo_VertexProgram_Ctor);
    RegisterRuntimeExport("??1VertexProgram@Video@@UEAA@XZ", (void*)&RtVideo_VertexProgram_Dtor); ++n;
    RegisterRuntimeAddr  (0x1800f61b0ull,                    (void*)&RtVideo_VertexProgram_Dtor);
    RegisterRuntimeExport("?Construct@VertexProgram@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_VertexProgram_Construct); ++n;
    RegisterRuntimeAddr  (0x18004e700ull, (void*)&RtVideo_VertexProgram_Construct);
    RegisterRuntimeExport("?Reconstruct@VertexProgram@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_VertexProgram_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004e6e0ull, (void*)&RtVideo_VertexProgram_Reconstruct);
    RegisterRuntimeExport("?Destruct@VertexProgram@Video@@UEAAXXZ", (void*)&RtVideo_VertexProgram_Destruct); ++n;
    RegisterRuntimeAddr  (0x18004e710ull,                           (void*)&RtVideo_VertexProgram_Destruct);
    RegisterRuntimeExport("?Serialize@VertexProgram@Video@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtVideo_VertexProgram_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f6240ull, (void*)&RtVideo_VertexProgram_Serialize);

    RegisterRuntimeExport("?GetData@VertexProgram@Video@@QEBAPEBVVertexProgramData@2@XZ",
                          (void*)&RtVideo_VertexProgram_GetData); ++n;
    RegisterRuntimeAddr  (0x18004e720ull, (void*)&RtVideo_VertexProgram_GetData);
    RegisterRuntimeExport("?Bind@VertexProgram@Video@@QEBAXAEAVContext@2@H@Z",
                          (void*)&RtVideo_VertexProgram_Bind); ++n;
    RegisterRuntimeAddr  (0x1800f6660ull, (void*)&RtVideo_VertexProgram_Bind);
    RegisterRuntimeExport("?PlatformConstruct@VertexProgram@Video@@AEAAXXZ",
                          (void*)&RtVideo_VertexProgram_PlatformConstructExport); ++n;
    RegisterRuntimeAddr  (0x1800f65e0ull, (void*)&RtVideo_VertexProgram_PlatformConstructExport);
    RegisterRuntimeExport("?PlatformCreate@VertexProgram@Video@@AEAA_NXZ",
                          (void*)&RtVideo_VertexProgram_PlatformCreateExport); ++n;
    RegisterRuntimeAddr  (0x1800f6640ull, (void*)&RtVideo_VertexProgram_PlatformCreateExport);
    RegisterRuntimeExport("?PlatformDestruct@VertexProgram@Video@@AEAAXXZ",
                          (void*)&RtVideo_VertexProgram_PlatformDestructExport); ++n;
    RegisterRuntimeAddr  (0x1800f6620ull, (void*)&RtVideo_VertexProgram_PlatformDestructExport);
    return n;
}
