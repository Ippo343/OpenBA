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
// ============================================================================
// ENDGAME B.3.5 BLOCK B — THE BODIES MOVED INTO THE MEMBERS, AND THIS FILE'S
// LOCAL `struct VideoProgram` IS GONE
// ============================================================================
// `Video::VideoProgram` is a real C++ class in the NEW
// runtime/types/video/VideoProgram.h, which this block had to AUTHOR: B35_RULES
// 13.3 listed this layout as one where the size was verified and the FIELDS were
// not. The header carries the retail evidence for all five offsets — the four
// exported getters, the ctor's five stores, SetupShaders's context/dirty-bit
// pairing and Serialize's four per-T instantiations, four independent agreements.
//
// *** ITS PARENT IS `Object::External` DIRECTLY — NOT CompiledExternal, NOT
// ShaderProgram. *** That is B35_RULES 2.1's headline trap and the reason
// VideoProgram converts LAST and on its own (section 10 move 4). The ctor at
// 0x1800f45b0 stamps Base, External and VideoProgram and NEVER
// ??_7CompiledExternal@Object@@6B@, which ??0ShaderProgram and ??0VertexLayout
// both do; `?_ParentTypeName@VideoProgram@Video@@` says "Object::External".
//
// Each body below is a member, defined in the `namespace Video` block at the
// bottom (B34_BRIEFS A.2), and each registered DOOR is a one-line QUALIFIED
// non-virtual delegation through a `reinterpret_cast` bridge — qualified because a
// live VideoProgram wears the MINTED table whose slots resolve back to these very
// doors (B34_BRIEFS A.1). Nothing is unregistered; the ctor door stays a ctor door
// (B4b-2: it writes the MINTED vptr).
//
// One runtime-internal binding retires:
// `RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self)` — the base
// destructor is the destructor CHAIN now, which is what retail's tail-JMP to
// 0x1800bc950 at the end of 0x1800f4650 is (spelling 3).
//
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
#include "../../types/video/VideoProgram.h"

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using Video::VideoProgram;

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

// ??0VideoProgram@Video@@QEAA@XZ (0x1800f45b0) — the ctor DOOR's body, which
// `Construct` (0x18004edf0) repeats behind a null test. THREE vtable stamps: Base,
// External, VideoProgram. There is no CompiledExternal stamp and adding one would
// be the trap this class is famous for.
void ConstructBody(VideoProgram* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->_path.data      = nullptr;   // +0x08
    self->_refs._data     = nullptr;   // +0x10
    self->_refs._size     = 0;         // +0x18
    self->_refs._capacity = 0;
    *vp = VideoProgramVTable();
    self->_vertexLayout  = nullptr;         // +0x20
    self->_vertexProgram = nullptr;         // +0x28
    self->_pixelProgram  = nullptr;         // +0x30
    self->_renderState   = nullptr;         // +0x38
    self->Video::VideoProgram::PlatformConstruct();   // +0x40
}

}  // namespace

// ---------------------------------------------------------------------------
// The registered doors. Each is one line over the member of the same name.
// ---------------------------------------------------------------------------
extern "C" uint32_t RtVideo_VideoProgram_Type(const void* selfv) {
    return reinterpret_cast<const VideoProgram*>(selfv)
               ->Video::VideoProgram::Type();                      // B.3.5 bridge
}
extern "C" const char* RtVideo_VideoProgram_TypeName(const void* selfv) {
    return reinterpret_cast<const VideoProgram*>(selfv)
               ->Video::VideoProgram::TypeName();                  // B.3.5 bridge
}
extern "C" int32_t RtVideo_VideoProgram_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const VideoProgram*>(selfv)
               ->Video::VideoProgram::GetFileVersion();            // B.3.5 bridge
}

extern "C" void* RtVideo_VideoProgram_Ctor(void* selfv) {
    ConstructBody(reinterpret_cast<VideoProgram*>(selfv));         // B.3.5 bridge
    return selfv;
}
extern "C" void* RtVideo_VideoProgram_Construct(void* selfv) {
    // RETAIL'S NULL TEST STAYS IN THE DOOR (BLOCK_BRIEF "the shape"): `this` is
    // `nonnull` to the compiler, so the same test inside the member is both a
    // -Wnonnull-compare warning and an optimisation GCC may delete.
    if (selfv == nullptr) return nullptr;
    return reinterpret_cast<VideoProgram*>(selfv)
               ->Video::VideoProgram::Construct();                 // B.3.5 bridge
}
extern "C" void* RtVideo_VideoProgram_Reconstruct(void* selfv) {
    return reinterpret_cast<VideoProgram*>(selfv)
               ->Video::VideoProgram::Reconstruct();               // B.3.5 bridge
}
extern "C" void RtVideo_VideoProgram_Destruct(void* selfv) {
    reinterpret_cast<VideoProgram*>(selfv)
        ->Video::VideoProgram::Destruct();                         // B.3.5 bridge
}
// ??1VideoProgram@Video@@UEAA@XZ (0x1800f4650)
extern "C" void RtVideo_VideoProgram_Dtor(void* selfv) {
    reinterpret_cast<VideoProgram*>(selfv)
        ->Video::VideoProgram::~VideoProgram();                    // B.3.5 bridge
}
extern "C" bool RtVideo_VideoProgram_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<VideoProgram*>(selfv)
               ->Video::VideoProgram::Serialize(streamv);          // B.3.5 bridge
}

// ============================================================================
// THE MEMBER BODIES — `Video::VideoProgram::*`. At `namespace Video` scope and
// AFTER the anonymous namespace (B34_BRIEFS A.2).
// ============================================================================
namespace Video {

// slot 1 — the `_typeInfo` record's +0x08 (`BaseTypeInfo::name`, CONTENT-only).
const char* VideoProgram::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, VideoProgramTypeInfo() + 8, sizeof v); return v;
}
// slot 2 — the record's +0x00, 0xFFFFFFFF until Factory::Startup.
uint32_t VideoProgram::Type() const {
    uint32_t v = 0; std::memcpy(&v, VideoProgramTypeInfo(), sizeof v); return v;
}
// slot 3 — 0x1800f46a0 is `MOV EAX,1 ; RET`.
int32_t VideoProgram::GetFileVersion() const { return 1; }

// slot 8 — 0x18004edf0, the ctor body; retail's leading `TEST RCX,RCX / JZ` lives
// in the DOOR. `virtual`, not `override`: this branch introduces the slot (B5-16).
Object::External* VideoProgram::Construct() {
    ConstructBody(this);
    return this;
}

// slot 6 — 0x18004edd0. On why the slot-0 call is `CallSlot0` and not
// `DeletingDtor(0)`: our slot 0 is declared PURE (the body is deleting_dtor.cpp's
// table-driven one, registered by address and kept by B35_RULES 11), so the C++
// spelling would be a virtual call on an abstract class that GCC may fold to
// `__cxa_pure_virtual`. `CallSlot0` reads the object's ACTUAL — minted — table.
Object::External* VideoProgram::Reconstruct() {
    CallSlot0(this, 0);
    ConstructBody(this);
    return this;
}
// slot 7 — 0x18004ee00.
void VideoProgram::Destruct() { CallSlot0(this, 0); }

// ??1VideoProgram@Video@@UEAA@XZ (0x1800f4650), read as disassembly:
//   1800f4666  LEA RAX,[0x180157a70] / MOV [RCX],RAX     the vtable stamp
//   1800f4670  MOV RCX,[RCX+0x40] / TEST / JE            PlatformDestruct, inlined
//   1800f4681  CALL [RAX]  with EDX=1                      _data slot 0, flags=1
//   ...        tail-JMP ~External (0x1800bc950)
// The stamp is written out by hand, naming the MINTED address, because GCC's own
// vptr store at the top of a polymorphic destructor names GCC's table while the
// mint rows and the rest of the tree expect the minted one. The tail-JMP is the
// destructor CHAIN: `~External()` is a real member
// (runtime/src/object/external_class.cpp). There are no arrays of our own to tear
// down — the four fields are trivially destructible `ExternalPtr<T>`s and retail
// does not release them here either.
VideoProgram::~VideoProgram() {
    *reinterpret_cast<void**>(this) = VideoProgramVTable();
    this->Video::VideoProgram::PlatformDestruct();
}

// The four getters, 0x18004ee10 .. 0x18004ee40, each `MOV RAX,[RCX+off]` into the
// caller's return slot. On why none of them has a door, and what a door would have
// to do about the hidden slot, see the header.
Object::ExternalPtr<VertexLayout>  VideoProgram::GetVertexLayout() const  { return _vertexLayout; }
Object::ExternalPtr<VertexProgram> VideoProgram::GetVertexProgram() const { return _vertexProgram; }
Object::ExternalPtr<PixelProgram>  VideoProgram::GetPixelProgram() const  { return _pixelProgram; }
Object::ExternalPtr<RenderState>   VideoProgram::GetRenderState() const   { return _renderState; }
// 0x18004ee50 — `MOV RAX,[RCX+0x40]; RET`, no return slot.
const void* VideoProgram::GetData() const { return _data; }

// ?PlatformConstruct@VideoProgram@Video@@AEAAXXZ (0x1800f47a0), also the tail of
// the ctor and of Construct. DeviceData slot 18 (+0x90).
void VideoProgram::PlatformConstruct() {
    void* dd = DeviceData();
    if (dd == nullptr) { _data = nullptr; return; }
    using CreateFn = void* (*)(void* deviceData, void* program);
    void** vt = *reinterpret_cast<void***>(dd);
    _data = reinterpret_cast<CreateFn>(vt[kDeviceDataCreateVideoProgramData / 8])(dd, this);
}
// ?PlatformCreate@VideoProgram@Video@@AEAAXXZ (0x1800f4810): _data->vtable[1](_data)
void VideoProgram::PlatformCreate() {
    using Fn = void (*)(void*);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[1])(_data);
}
// ?PlatformDestruct@VideoProgram@Video@@AEAAXXZ (0x1800f47f0): _data->vtable[0](_data, 1)
void VideoProgram::PlatformDestruct() {
    if (_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(_data);
    reinterpret_cast<Fn>(vt[0])(_data, 1);
}

// slot 5 — ?Serialize@VideoProgram@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f46b0).
//
// The four per-T `ExternalPtr<T>::Serialize` instantiations are reached by ADDRESS
// because each is its own 748-byte body in retail and ours are ported in
// object/external_ptr_serialize.cpp under those addresses. The ORDER is the field
// order and the four addresses are NOT interchangeable: each one loads its own
// `_typeInfo`, so swapping two would look up a VertexProgram resource under
// VertexLayout's type id.
bool VideoProgram::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using ExtPtrSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<ExtPtrSerFn>(0x180055090ull)(&_vertexLayout,  &ser);   // ExternalPtr<VertexLayout>
    RtAddr<ExtPtrSerFn>(0x1800542a0ull)(&_vertexProgram, &ser);   // ExternalPtr<VertexProgram>
    RtAddr<ExtPtrSerFn>(0x180055380ull)(&_pixelProgram,  &ser);   // ExternalPtr<PixelProgram>
    RtAddr<ExtPtrSerFn>(0x180055670ull)(&_renderState,   &ser);   // ExternalPtr<RenderState>

    if ((_vertexLayout.Get() || _vertexProgram.Get() || _pixelProgram.Get() || _renderState.Get())
        && stream->direction == 0) {
        this->Video::VideoProgram::PlatformCreate();   // retail derefs _data unguarded
    }
    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

}  // namespace Video

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
