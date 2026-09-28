// ============================================================================
// runtime/src/video/pixelprogram.cpp — Video::PixelProgram, the whole class
// (original file: code\Runtime\Video\PixelProgram.h + ShaderProgram_win.cpp)
// ============================================================================
//
// ??_7PixelProgram@Video@@6B@ (retail 0x180157ac0), nine slots:
//
//   0  deleting dtor        0x18004eb90  ShaderProgram.h (inline, B.3.9)
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
// it can only run when reading a pre-v1 file and it keeps nothing.
//
// ============================================================================
// ENDGAME B.3.5 BLOCK B — THE BODIES MOVED INTO THE MEMBERS.
// ============================================================================
// Read vertexprogram.cpp's banner of the same name: `Video::PixelProgram` is the
// twin conversion, member for member. The bodies are members of the real class
// (runtime/types/video/ShaderProgram.h), defined in the `namespace Video` block
// at the bottom of this file; each registered door is a one-line QUALIFIED
// non-virtual delegation through a `reinterpret_cast` bridge; ~PixelProgram
// writes only the minted ??_7PixelProgram stamp and PlatformDestruct, and
// ~ShaderProgram / ~CompiledExternal / ~External are the destructor CHAIN, which
// is exactly retail's inlined teardown at 0x1800f3a00. The
// `RtVideo_ShaderProgram_DestroyArrays` and `??1External@Object@@UEAA@XZ`
// bindings are retired here too. ENDGAME B.3.9 VID: the ctor is REAL now
// (ShaderProgram.h); the ctor / dtor / seven slot doors retired with
// ??_7PixelProgram@Video@@6B@ and the factory pair is the class's own.
//
// CORRECTION (cutover closure batch D1, 2026-09-12). This banner used to end
// "...are reached through ResolveAddr and RESOLVE TO NOTHING ONCE THE DLL IS
// GONE". All four are now ported and registered: 0x1800547e0 and 0x180054a20
// as `kSpec_StringRefZ_PP` / `kSpec_PpSampler10` in
// runtime/src/collection/array.cpp, 0x180054920 and 0x180054ba0 as the
// `kD_Str_08` / `kD_Str_10` Body-B rows in array_destroy.cpp. ResolveAddr is
// registry-first, so the four calls below already land on our bodies.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"   // ENDGAME B.3.9 VID: Video::Device::_singleton
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

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using Video::PixelProgram;
namespace ddvt = rt::video::devicedata_vtbl;
namespace pdvt = rt::video::programdata_vtbl;

void* ActiveDeviceDataPtr() {
    void** slot = reinterpret_cast<void**>(&::Video::Device::_singleton);
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using ScalarFn    = void (*)(ReaderWriter*, void*, const char*);
using VersionFn   = int  (*)(ReaderWriter*);
using SetSizeFn   = void (*)(void*, int);
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
        // B.3.1 bridge: `strings8` is untyped array storage, one System::String per
        // 8-byte slot; the cast dies when Collection::Array<T> is a template (B.3.2).
        for (int32_t i = 0; i < n; ++i, e += 8)
            reinterpret_cast<System::String*>(e)->Serialize(ser);
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
            reinterpret_cast<System::String*>(e + 0x00)->Serialize(ser);  // B.3.1 bridge
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

// ---------------------------------------------------------------------------
// The registered doors. Each is one line over the member of the same name.
// ---------------------------------------------------------------------------
extern "C" uint32_t RtVideo_PixelProgram_TypeId() {
    return Video::PixelProgram::TypeId();
}
// .rdata literals at 0x180157a50 / 0x180157a28 / 0x180157a38. RULE B5-15: each
// is the EXACT retail literal, copied, never derived from the C++ base name.
// ENDGAME B.3.9 TI-REST: `_TypeName` / `_ParentTypeName` are DECLARE_TYPE's
// private statics (retail's `CA`), which an `extern "C"` door cannot reach, so
// the two doors repeat the literals -- renderstate.cpp's precedent -- rather
// than widen the access. The record (video_type_records.cpp) is built from the
// class's own accessors.
extern "C" const char* RtVideo_PixelProgram_StaticTypeName() { return "PixelProgram"; }
extern "C" const char* RtVideo_PixelProgram_ParentTypeName() { return "ShaderProgram"; }
extern "C" const char* RtVideo_PixelProgram_FileExtension()  { return Video::PixelProgram::_FileExtension(); }
extern "C" const void* RtVideo_PixelProgram_GetData(const void* selfv) {
    return reinterpret_cast<const PixelProgram*>(selfv)
               ->Video::PixelProgram::GetData();                      // B.3.5 bridge
}
extern "C" void RtVideo_PixelProgram_Bind(const void* selfv, void* context, int index) {
    reinterpret_cast<const PixelProgram*>(selfv)
        ->Video::PixelProgram::Bind(context, index);                  // B.3.5 bridge
}

// See vertexprogram.cpp: `AEAA` is private, not unexported.
extern "C" void RtVideo_PixelProgram_PlatformConstructExport(void* selfv) {
    reinterpret_cast<PixelProgram*>(selfv)
        ->Video::PixelProgram::PlatformConstruct();                   // B.3.5 bridge
}
extern "C" bool RtVideo_PixelProgram_PlatformCreateExport(void* selfv) {
    return reinterpret_cast<PixelProgram*>(selfv)
               ->Video::PixelProgram::PlatformCreate();               // B.3.5 bridge
}
extern "C" void RtVideo_PixelProgram_PlatformDestructExport(void* selfv) {
    reinterpret_cast<PixelProgram*>(selfv)
        ->Video::PixelProgram::PlatformDestruct();                    // B.3.5 bridge
}

// ============================================================================
// THE MEMBER BODIES — `Video::PixelProgram::*`. At `namespace Video` scope and
// AFTER the anonymous namespace (B34_BRIEFS A.2).
// ============================================================================
namespace Video {

// TypeName / TypeId / Type / `_TypeName` / `_ParentTypeName`: DECLARE_TYPE /
// DEFINE_TYPE's (video_type_records.cpp; ENDGAME B.3.9 TI-REST).
// RULE B5-15: read out of .rdata, not derived from the C++ base name.
const char* PixelProgram::_FileExtension()  { return "srsl"; }

// slot 3 — 0x1800f3a80 is `MOV EAX,5 ; RET`.
int32_t PixelProgram::GetFileVersion() const { return 5; }

// slot 8 — ?Construct@: `TEST RCX,RCX / JZ` then the ctor (`this` is never null
// here). `virtual`, not `override`: this branch introduces the slot (RULE
// B5-16). `::new` because the class declares its own (unported) placement new.
Object::External* PixelProgram::Construct() {
    return ::new (this) PixelProgram();
}

// slot 6 — ?Reconstruct@: slot 0 with flags 0 (destroy in place, no free), then
// the ctor over the same storage.
Object::External* PixelProgram::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) PixelProgram();
}

// slot 7 — ?Destruct@: `XOR EDX,EDX ; JMP [RAX]` — slot 0, flags 0.
void PixelProgram::Destruct() { DeletingDtor(0); }

// ??1PixelProgram@Video@@UEAA@XZ — retail: stamp ??_7PixelProgram, PlatformDestruct, then
// ~ShaderProgram inlined and ~External. The stamps and everything after
// PlatformDestruct are the destructor chain's.
PixelProgram::~PixelProgram() {
    this->Video::PixelProgram::PlatformDestruct();
}

// ?CreateNewObject@PixelProgram@Video@@SAPEAVBase@Object@@XZ  0x18004e9d0:
// MemAlloc(0x58, 8), null branch, the ctor (B39_RULES D2).
Object::Base* PixelProgram::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PixelProgram), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) PixelProgram();
}
// ?CreateInPlaceObject@PixelProgram@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004ea50
Object::Base* PixelProgram::CreateInPlaceObject(void* storage) {
    return storage ? ::new (storage) PixelProgram() : nullptr;
}

// ?GetData@PixelProgram@Video@@QEBAPEBVPixelProgramData@2@XZ (0x18004eaa0)
const void* PixelProgram::GetData() const { return _data; }

// ?Bind@PixelProgram@Video@@QEBAXAEAVContext@2@H@Z (0x1800f3ed0)
void PixelProgram::Bind(void* context, int32_t permutation) const {
    using Fn = void (*)(void*, void*, int);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(_data, context, permutation);
}

// ?PlatformConstruct@PixelProgram@Video@@AEAAXXZ (0x1800f3e40). THE SLOT IS
// +0x80, NOT +0x78: CreatePixelProgramData, not CreateVertexProgramData. Swapping
// the two produces a PixelProgramData behind a VertexProgram's vtable, which
// fails only on the first Bind.
void PixelProgram::PlatformConstruct() {
    void* dd = ActiveDeviceDataPtr();
    if (dd == nullptr) { _data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    _data = reinterpret_cast<CreateFn>(vt[ddvt::kCreatePixelProgramData / 8])(dd, this);
}
// ?PlatformCreate@PixelProgram@Video@@AEAA_NXZ (0x1800f3eb0)
bool PixelProgram::PlatformCreate() {
    using Fn = bool (*)(void*);
    void** vt = *reinterpret_cast<void***>(_data);
    return reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(_data);
}
// ?PlatformDestruct@PixelProgram@Video@@AEAAXXZ (0x1800f3e90)
void PixelProgram::PlatformDestruct() {
    if (_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(_data, 1);
}

// slot 5 — ?Serialize@PixelProgram@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f3a90)
bool PixelProgram::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    // Retail CALLs 0x1800f5110 here; the QUALIFIED spelling is that call.
    this->ShaderProgram::Serialize(stream);

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
        this->Video::PixelProgram::PlatformCreate();   // retail derefs _data unguarded

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

}  // namespace Video

int RtVideoPixelProgramRegister() {
    int n = 0;
    // ENDGAME B.3.9 VID: the factory pair by retail NAME and VA (B39_RULES D2).
    RegisterRuntimeExport("?CreateNewObject@PixelProgram@Video@@SAPEAVBase@Object@@XZ",
                          (void*)&Video::PixelProgram::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x18004e9d0ull, (void*)&Video::PixelProgram::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@PixelProgram@Video@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&Video::PixelProgram::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x18004ea50ull, (void*)&Video::PixelProgram::CreateInPlaceObject);
    RegisterRuntimeExport("?TypeId@PixelProgram@Video@@SAIXZ",        (void*)&RtVideo_PixelProgram_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004e8d0ull,                             (void*)&RtVideo_PixelProgram_TypeId);
    RegisterRuntimeExport("?_TypeName@PixelProgram@Video@@CAPEBDXZ",       (void*)&RtVideo_PixelProgram_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e8a0ull,                                  (void*)&RtVideo_PixelProgram_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@PixelProgram@Video@@CAPEBDXZ", (void*)&RtVideo_PixelProgram_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e8b0ull,                                  (void*)&RtVideo_PixelProgram_ParentTypeName);
    RegisterRuntimeExport("?_FileExtension@PixelProgram@Video@@CAPEBDXZ",  (void*)&RtVideo_PixelProgram_FileExtension); ++n;
    RegisterRuntimeAddr  (0x18004e890ull,                                  (void*)&RtVideo_PixelProgram_FileExtension);


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
