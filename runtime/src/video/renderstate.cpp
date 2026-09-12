// ============================================================================
// runtime/src/video/renderstate.cpp — Video::RenderState, the whole class
// (original file: code\Runtime\Video\RenderState.h)
// ============================================================================
//
// ??_7RenderState@Video@@6B@ (retail 0x180157720), nine slots:
//
//   0  deleting dtor        0x18004b9d0  runtime/src/object/deleting_dtor.cpp
//   1  TypeName             0x18004b570  here
//   2  Type                 0x18004b550  here
//   3  GetFileVersion       0x1800f4c80  here  (returns 3)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f4c90  here
//   6  Reconstruct          0x18004b6f0  here
//   7  Destruct             0x18004b720  here
//   8  Construct            0x18004b710  here
//
// plus ??0RenderState@Video@@QEAA@XZ (0x1800f4b60, the factory step at
// 0x18004b6bf), ~RenderState (0x1800f4c30), Initialize, IsWireframe, Bind, the
// three Platform* helpers, and the three sub-struct ctors and serializers:
//     ??0Stencil    0x1800f49d0   ?Serialize@Stencil     0x1800f48e0
//     ??0Depth      0x1800f4a80   ?Serialize@Depth       0x1800f4a00
//     ??0AlphaBlend 0x1800f4b30   ?Serialize@AlphaBlend  0x1800f4aa0
//
// Layout and the "derives from External, NOT CompiledExternal" finding:
// runtime/types/video/RenderState.h.
//
// ---------------------------------------------------------------------------
// Serialize (0x1800f4c90) INLINES TWO OF THE THREE SUB-SERIALIZERS
// ---------------------------------------------------------------------------
// AlphaBlend's five fields and Depth's five are emitted straight into the body;
// only Stencil is a real `CALL 0x1800f48e0`. The two inlined runs are field for
// field identical to the exported sub-serializers they came from — checked
// against 0x1800f4aa0 and 0x1800f4a00 — so this file expresses them as calls to
// the same three functions it registers. That is the same code; what it is NOT
// is an invitation to "simplify" Stencil into the other two, because Stencil
// serializes its write mask as a UChar (+0x08) among eight UInts, and a UInt
// there would move the next eight fields by three bytes.
//
// The reader/writer is built with version 0 (`XOR R8D,R8D` at 0x1800f4cc1) —
// unlike the shader programs above, which pass 1.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/video/RenderState.h"
#include "../../types/video/ShaderProgram.h"   // devicedata_vtbl / programdata_vtbl

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
using rt::video::RenderState;
using rt::video::AlphaBlend;
using rt::video::Depth;
using rt::video::Stencil;
namespace ddvt = rt::video::devicedata_vtbl;
namespace pdvt = rt::video::programdata_vtbl;

void* BaseVTable()        { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()    { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* RenderStateVTable() { return RT_DATA_SLOT(void*, "??_7RenderState@Video@@6B@"); }
const uint8_t* RenderStateTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A");
}
void* DeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100);
}

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
inline void RwBool (ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool  / 8])(rw, p, nullptr);
}
inline void RwUInt (ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt  / 8])(rw, p, nullptr);
}
inline void RwUChar(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUChar / 8])(rw, p, nullptr);
}
inline void RwFloat(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarFloat / 8])(rw, p, nullptr);
}

// ?PlatformConstruct@RenderState@Video@@AEAAXXZ (0x1800f4e30)
void PlatformConstruct(RenderState* self) {
    void* dd = DeviceData();
    if (dd == nullptr) { self->_data = nullptr; return; }
    using CreateFn = void* (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(dd);
    self->_data = reinterpret_cast<CreateFn>(vt[ddvt::kCreateRenderStateData / 8])(dd, self);
}
// ?PlatformCreate@RenderState@Video@@AEAAXXZ (0x1800f4ea0) — returns void here,
// unlike the shader programs' `_N` (bool) form. The mangling says so and so
// does the tail-jump's discarded result.
void PlatformCreate(RenderState* self) {
    using Fn = void (*)(void*);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[pdvt::kCreate / 8])(self->_data);
}
// ?PlatformDestruct@RenderState@Video@@AEAAXXZ (0x1800f4e80)
void PlatformDestruct(RenderState* self) {
    if (self->_data == nullptr) return;
    using Fn = void (*)(void*, int);
    void** vt = *reinterpret_cast<void***>(self->_data);
    reinterpret_cast<Fn>(vt[pdvt::kDeletingDtor / 8])(self->_data, 1);
}

// ??0RenderState@Video@@QEAA@XZ (0x1800f4b60). The defaults are the three
// sub-struct ctors inlined, in field order.
void ConstructBody(RenderState* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;   // +0x08
    self->base._refs._data     = nullptr;   // +0x10
    self->base._refs._size     = 0;         // +0x18
    self->base._refs._capacity = 0;
    *vp = RenderStateVTable();              // no CompiledExternal stamp — see the header

    self->_alphaBlend.enabled   = 0;        // +0x20  (a BYTE store; +0x21..+0x23 untouched)
    self->_alphaBlend.srcBlend  = 1;        // +0x24
    self->_alphaBlend.dstBlend  = 0;        // +0x28
    self->_alphaBlend.blendOp   = 0;        // +0x2c
    self->_alphaBlend.writeMask = 0xf;      // +0x30
    self->_depth.test           = 1;        // +0x34  (a WORD store of 0x0101)
    self->_depth.write          = 1;        // +0x35
    self->_depth.func           = 4;        // +0x38
    self->_depth.bias           = 0.0f;     // +0x3c
    self->_depth.slopeBias      = 0.0f;     // +0x40
    self->_stencil.enabled      = 0;        // +0x44  (a WORD store of 0)
    self->_stencil.twoSided     = 0;        // +0x45
    self->_stencil.readMask     = 0;        // +0x48
    self->_stencil.writeMask    = 0;        // +0x4c  (a BYTE store)
    self->_stencil.func         = 0;        // +0x50
    self->_stencil.failOp       = 0;        // +0x54
    self->_stencil.depthFailOp  = 0;        // +0x58
    self->_stencil.passOp       = 0;        // +0x5c
    self->_stencil.backFunc     = 0;        // +0x60
    self->_stencil.reference    = 0;        // +0x64
    self->_cullMode             = 1;        // +0x68
    self->_fillMode             = 0;        // +0x6c
    self->_flag70               = 0;        // +0x70
    PlatformConstruct(self);                // +0x78
}

}  // namespace

// ---- the three sub-structs, all exported ----------------------------------
extern "C" void* RtVideo_RenderState_AlphaBlend_Ctor(void* p) {   // 0x1800f4b30
    AlphaBlend* a = static_cast<AlphaBlend*>(p);
    a->enabled = 0; a->srcBlend = 1; a->dstBlend = 0; a->blendOp = 0; a->writeMask = 0xf;
    return p;
}
extern "C" void* RtVideo_RenderState_Depth_Ctor(void* p) {        // 0x1800f4a80
    Depth* d = static_cast<Depth*>(p);
    d->test = 1; d->write = 1; d->func = 4; d->bias = 0.0f; d->slopeBias = 0.0f;
    return p;
}
extern "C" void* RtVideo_RenderState_Stencil_Ctor(void* p) {      // 0x1800f49d0
    Stencil* s = static_cast<Stencil*>(p);
    s->enabled = 0; s->twoSided = 0; s->readMask = 0; s->writeMask = 0;
    s->func = 0; s->failOp = 0; s->depthFailOp = 0; s->passOp = 0;
    s->backFunc = 0; s->reference = 0;
    return p;
}

extern "C" void RtVideo_RenderState_AlphaBlend_Serialize(void* p, void* serv) {  // 0x1800f4aa0
    AlphaBlend* a = static_cast<AlphaBlend*>(p);
    ReaderWriter* rw = static_cast<Serializer*>(serv)->readerWriter;
    RwBool(rw, &a->enabled);
    RwUInt(rw, &a->srcBlend);
    RwUInt(rw, &a->dstBlend);
    RwUInt(rw, &a->blendOp);
    RwUInt(rw, &a->writeMask);
}
extern "C" void RtVideo_RenderState_Depth_Serialize(void* p, void* serv) {       // 0x1800f4a00
    Depth* d = static_cast<Depth*>(p);
    ReaderWriter* rw = static_cast<Serializer*>(serv)->readerWriter;
    RwBool (rw, &d->test);
    RwBool (rw, &d->write);
    RwUInt (rw, &d->func);
    RwFloat(rw, &d->bias);
    RwFloat(rw, &d->slopeBias);
}
extern "C" void RtVideo_RenderState_Stencil_Serialize(void* p, void* serv) {     // 0x1800f48e0
    Stencil* s = static_cast<Stencil*>(p);
    ReaderWriter* rw = static_cast<Serializer*>(serv)->readerWriter;
    RwBool (rw, &s->enabled);
    RwBool (rw, &s->twoSided);
    RwUInt (rw, &s->readMask);
    RwUChar(rw, &s->writeMask);     // UChar, not UInt — see this file's banner
    RwUInt (rw, &s->func);
    RwUInt (rw, &s->failOp);
    RwUInt (rw, &s->depthFailOp);
    RwUInt (rw, &s->passOp);
    RwUInt (rw, &s->backFunc);
    RwUInt (rw, &s->reference);
}

extern "C" uint32_t RtVideo_RenderState_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, RenderStateTypeInfo(), sizeof v); return v;
}
extern "C" uint32_t RtVideo_RenderState_TypeId() {
    uint32_t v = 0; std::memcpy(&v, RenderStateTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtVideo_RenderState_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, RenderStateTypeInfo() + 8, sizeof v); return v;
}
// .rdata literals at 0x180157710 / 0x1801544f0. The parent really is
// "Object::External" and not "Object::CompiledExternal"; see the header.
extern "C" const char* RtVideo_RenderState_StaticTypeName() { return "RenderState"; }
extern "C" const char* RtVideo_RenderState_ParentTypeName() { return "Object::External"; }
extern "C" int32_t     RtVideo_RenderState_GetFileVersion(const void*) { return 3; }

extern "C" void* RtVideo_RenderState_Ctor(void* selfv) {
    ConstructBody(static_cast<RenderState*>(selfv)); return selfv;
}
extern "C" void* RtVideo_RenderState_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<RenderState*>(selfv)); return selfv;
}
extern "C" void* RtVideo_RenderState_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<RenderState*>(selfv)); return selfv;
}
extern "C" void RtVideo_RenderState_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1RenderState@Video@@UEAA@XZ (0x1800f4c30)
extern "C" void RtVideo_RenderState_Dtor(void* selfv) {
    RenderState* self = static_cast<RenderState*>(selfv);
    *reinterpret_cast<void**>(self) = RenderStateVTable();
    PlatformDestruct(self);
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

extern "C" bool RtVideo_RenderState_IsWireframe(const void* selfv) {
    return static_cast<const RenderState*>(selfv)->_fillMode == 1;
}
// ?Bind@RenderState@Video@@QEBAXAEAVContext@2@@Z (0x1800f4ec0)
extern "C" void RtVideo_RenderState_Bind(const void* selfv, void* context) {
    void* data = static_cast<const RenderState*>(selfv)->_data;
    using Fn = void (*)(void*, void*);
    void** vt = *reinterpret_cast<void***>(data);
    reinterpret_cast<Fn>(vt[pdvt::kBind / 8])(data, context);
}
// ?Initialize@RenderState@Video@@ (0x1800f4840): a field-by-field copy, then
// PlatformCreate. Retail copies dword by dword and therefore also copies the
// three padding bytes after AlphaBlend::enabled and the two after Depth::test;
// the struct assignments below copy the same 0x14 / 0x10 / 0x24 byte extents.
extern "C" void RtVideo_RenderState_Initialize(void* selfv, const void* ab, const void* dp,
                                               const void* st, uint32_t cull, uint32_t fill,
                                               bool flag) {
    RenderState* self = static_cast<RenderState*>(selfv);
    std::memcpy(&self->_alphaBlend, ab, sizeof(AlphaBlend));
    std::memcpy(&self->_depth,      dp, sizeof(Depth));
    std::memcpy(&self->_stencil,    st, sizeof(Stencil));
    self->_fillMode = fill;     // retail stores fillMode BEFORE cullMode
    self->_cullMode = cull;
    self->_flag70   = flag ? 1 : 0;
    PlatformCreate(self);       // retail derefs _data unguarded
}

extern "C" void RtVideo_RenderState_PlatformConstructExport(void* selfv) {
    PlatformConstruct(static_cast<RenderState*>(selfv));
}
extern "C" void RtVideo_RenderState_PlatformCreateExport(void* selfv) {
    PlatformCreate(static_cast<RenderState*>(selfv));
}
extern "C" void RtVideo_RenderState_PlatformDestructExport(void* selfv) {
    PlatformDestruct(static_cast<RenderState*>(selfv));
}

// ?Serialize@RenderState@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f4c90)
extern "C" bool RtVideo_RenderState_Serialize(void* selfv, void* streamv) {
    RenderState* self = static_cast<RenderState*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    RtVideo_RenderState_AlphaBlend_Serialize(&self->_alphaBlend, &ser);   // inlined by retail
    RtVideo_RenderState_Depth_Serialize     (&self->_depth,      &ser);   // inlined by retail
    RtVideo_RenderState_Stencil_Serialize   (&self->_stencil,    &ser);   // a real CALL
    RwUInt(rw, &self->_cullMode);
    RwUInt(rw, &self->_fillMode);
    RwBool(rw, &self->_flag70);

    if (stream->direction == rt::io::kDirectionReading)
        PlatformCreate(self);          // retail derefs _data unguarded

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtVideoRenderStateRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RenderState@Video@@UEBAIXZ",        (void*)&RtVideo_RenderState_Type);     ++n;
    RegisterRuntimeAddr  (0x18004b550ull,                            (void*)&RtVideo_RenderState_Type);
    RegisterRuntimeExport("?TypeId@RenderState@Video@@SAIXZ",        (void*)&RtVideo_RenderState_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004b560ull,                            (void*)&RtVideo_RenderState_TypeId);
    RegisterRuntimeExport("?TypeName@RenderState@Video@@UEBAPEBDXZ", (void*)&RtVideo_RenderState_TypeName); ++n;
    RegisterRuntimeAddr  (0x18004b570ull,                            (void*)&RtVideo_RenderState_TypeName);
    RegisterRuntimeExport("?_TypeName@RenderState@Video@@CAPEBDXZ",       (void*)&RtVideo_RenderState_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004b530ull,                                 (void*)&RtVideo_RenderState_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@RenderState@Video@@CAPEBDXZ", (void*)&RtVideo_RenderState_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004b540ull,                                 (void*)&RtVideo_RenderState_ParentTypeName);
    RegisterRuntimeExport("?GetFileVersion@RenderState@Video@@UEBAHXZ",   (void*)&RtVideo_RenderState_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800f4c80ull,                                 (void*)&RtVideo_RenderState_GetFileVersion);

    RegisterRuntimeExport("??0RenderState@Video@@QEAA@XZ", (void*)&RtVideo_RenderState_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f4b60ull,                  (void*)&RtVideo_RenderState_Ctor);
    RegisterRuntimeExport("??1RenderState@Video@@UEAA@XZ", (void*)&RtVideo_RenderState_Dtor); ++n;
    RegisterRuntimeAddr  (0x1800f4c30ull,                  (void*)&RtVideo_RenderState_Dtor);
    RegisterRuntimeExport("?Construct@RenderState@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_RenderState_Construct); ++n;
    RegisterRuntimeAddr  (0x18004b710ull, (void*)&RtVideo_RenderState_Construct);
    RegisterRuntimeExport("?Reconstruct@RenderState@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtVideo_RenderState_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18004b6f0ull, (void*)&RtVideo_RenderState_Reconstruct);
    RegisterRuntimeExport("?Destruct@RenderState@Video@@UEAAXXZ", (void*)&RtVideo_RenderState_Destruct); ++n;
    RegisterRuntimeAddr  (0x18004b720ull,                         (void*)&RtVideo_RenderState_Destruct);
    RegisterRuntimeExport("?Serialize@RenderState@Video@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtVideo_RenderState_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f4c90ull, (void*)&RtVideo_RenderState_Serialize);

    RegisterRuntimeExport("??0Stencil@RenderState@Video@@QEAA@XZ",    (void*)&RtVideo_RenderState_Stencil_Ctor);    ++n;
    RegisterRuntimeAddr  (0x1800f49d0ull,                             (void*)&RtVideo_RenderState_Stencil_Ctor);
    RegisterRuntimeExport("??0Depth@RenderState@Video@@QEAA@XZ",      (void*)&RtVideo_RenderState_Depth_Ctor);      ++n;
    RegisterRuntimeAddr  (0x1800f4a80ull,                             (void*)&RtVideo_RenderState_Depth_Ctor);
    RegisterRuntimeExport("??0AlphaBlend@RenderState@Video@@QEAA@XZ", (void*)&RtVideo_RenderState_AlphaBlend_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f4b30ull,                             (void*)&RtVideo_RenderState_AlphaBlend_Ctor);
    RegisterRuntimeExport("?Serialize@Stencil@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_Stencil_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f48e0ull, (void*)&RtVideo_RenderState_Stencil_Serialize);
    RegisterRuntimeExport("?Serialize@Depth@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_Depth_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f4a00ull, (void*)&RtVideo_RenderState_Depth_Serialize);
    RegisterRuntimeExport("?Serialize@AlphaBlend@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_AlphaBlend_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f4aa0ull, (void*)&RtVideo_RenderState_AlphaBlend_Serialize);

    RegisterRuntimeExport("?IsWireframe@RenderState@Video@@QEBA_NXZ", (void*)&RtVideo_RenderState_IsWireframe); ++n;
    RegisterRuntimeAddr  (0x18004b730ull,                             (void*)&RtVideo_RenderState_IsWireframe);
    RegisterRuntimeExport("?Bind@RenderState@Video@@QEBAXAEAVContext@2@@Z",
                          (void*)&RtVideo_RenderState_Bind); ++n;
    RegisterRuntimeAddr  (0x1800f4ec0ull, (void*)&RtVideo_RenderState_Bind);
    RegisterRuntimeExport("?Initialize@RenderState@Video@@QEAAXAEBUAlphaBlend@12@AEBUDepth@12@"
                          "AEBUStencil@12@W4CullMode@12@W4FillMode@12@_N@Z",
                          (void*)&RtVideo_RenderState_Initialize); ++n;
    RegisterRuntimeAddr  (0x1800f4840ull, (void*)&RtVideo_RenderState_Initialize);
    RegisterRuntimeExport("?PlatformConstruct@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformConstructExport); ++n;
    RegisterRuntimeAddr  (0x1800f4e30ull, (void*)&RtVideo_RenderState_PlatformConstructExport);
    RegisterRuntimeExport("?PlatformCreate@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformCreateExport); ++n;
    RegisterRuntimeAddr  (0x1800f4ea0ull, (void*)&RtVideo_RenderState_PlatformCreateExport);
    RegisterRuntimeExport("?PlatformDestruct@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformDestructExport); ++n;
    RegisterRuntimeAddr  (0x1800f4e80ull, (void*)&RtVideo_RenderState_PlatformDestructExport);
    return n;
}
