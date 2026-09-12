// ============================================================================
// runtime/src/collision2d/surface.cpp — Collision2D::Surface, the whole class
// ============================================================================
//
// Vtable batch 8. ??_7Surface@Collision2D@@6B@ (retail), eight slots:
//
//   0  deleting dtor        0x180064ea0  deleting_dtor.cpp row (this file registers ~Surface)
//   1  TypeName             0x180064a70  here
//   2  Type                 0x180064a50  here
//   3  GetFileVersion       0x18010ca30  here  (returns 1)
//   4  Compile              -- NOT a Surface slot (retail's kStepsCollision2D_Surface size
//                              0x30 has no Compile row in the batch list; Surface inherits
//                              External::Compile, already ported elsewhere)
//   5  Serialize            0x18010ca40  here
//   6  Reconstruct          0x180064c70  here
//   7  Destruct             0x180064d40  here
//   8  Construct            0x180064ce0  here
//
// plus ~Surface (0x18010ca00), registered here and appended to
// runtime/src/object/deleting_dtor.cpp's kSpecs (row for 0x180064ea0).
//
// Layout (sizeof 0x30; factory step table runtime/src/collision2d/rt_factories_collision2d.inc
// zeroes +0x20 as one qword and +0x28 as one dword, matching Construct/Reconstruct below):
//   +0x00  Object::External           (0x20)
//   +0x20  uint32_t  _f20             Serialize order: _f20, _f28, _f24
//   +0x24  uint32_t  _f24
//   +0x28  uint32_t  _f28
//   +0x2c  uint32_t  _pad2c           never written; makes the struct 8-aligned to 0x30
// None of the three fields is named by any body this port read; Serialize is the only
// reader/writer and it treats them as three independent uint scalars.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"

namespace {

using RtObject::External;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

struct Surface {
    External base;      // +0x00
    uint32_t _f20;       // +0x20
    uint32_t _f24;       // +0x24
    uint32_t _f28;       // +0x28
    uint32_t _pad2c;      // +0x2c
};
static_assert(sizeof(Surface) == 0x30, "Collision2D::Surface is 0x30 bytes");
static_assert(offsetof(Surface, _f20) == 0x20, "+0x20");
static_assert(offsetof(Surface, _f24) == 0x24, "+0x24");
static_assert(offsetof(Surface, _f28) == 0x28, "+0x28");

void* BaseVTable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* SurfaceVTable()  { return RT_DATA_SLOT(void*, "??_7Surface@Collision2D@@6B@"); }
const uint8_t* SurfaceTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@Surface@Collision2D@@0VBaseTypeInfo@Object@@A");
}

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

void ConstructBody(Surface* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = SurfaceVTable();
    self->_f20 = 0;
    self->_f24 = 0;
    self->_f28 = 0;
}

}  // namespace

extern "C" uint32_t RtCollision2D_Surface_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, SurfaceTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtCollision2D_Surface_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, SurfaceTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtCollision2D_Surface_GetFileVersion(const void*) { return 1; }

extern "C" void* RtCollision2D_Surface_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    ConstructBody(static_cast<Surface*>(selfv)); return selfv;
}
extern "C" void* RtCollision2D_Surface_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<Surface*>(selfv)); return selfv;
}
extern "C" void RtCollision2D_Surface_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1Surface@Collision2D@@UEAA@XZ (0x18010ca00): restamp own vtable, chain to
// ~External. No owned resource of its own — three plain uint32 fields.
extern "C" void RtCollision2D_Surface_Dtor(void* selfv) {
    Surface* self = static_cast<Surface*>(selfv);
    *reinterpret_cast<void**>(self) = SurfaceVTable();
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ?Serialize@Surface@Collision2D@@UEAA_NAEAVStream@IO@@@Z (0x18010ca40)
extern "C" bool RtCollision2D_Surface_Serialize(void* selfv, void* streamv) {
    Surface* self = static_cast<Surface*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using ScalarFn = void (*)(ReaderWriter*, uint32_t*, const char*);
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &self->_f20, nullptr);
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &self->_f28, nullptr);
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &self->_f24, nullptr);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtCollision2DSurfaceRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@Surface@Collision2D@@UEBAIXZ",           (void*)&RtCollision2D_Surface_Type);           ++n;
    RegisterRuntimeAddr  (0x180064a50ull,                                 (void*)&RtCollision2D_Surface_Type);
    RegisterRuntimeExport("?TypeName@Surface@Collision2D@@UEBAPEBDXZ",    (void*)&RtCollision2D_Surface_TypeName);       ++n;
    RegisterRuntimeAddr  (0x180064a70ull,                                 (void*)&RtCollision2D_Surface_TypeName);
    RegisterRuntimeExport("?GetFileVersion@Surface@Collision2D@@UEBAHXZ", (void*)&RtCollision2D_Surface_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x18010ca30ull,                                 (void*)&RtCollision2D_Surface_GetFileVersion);
    RegisterRuntimeExport("?Construct@Surface@Collision2D@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtCollision2D_Surface_Construct);   ++n;
    RegisterRuntimeAddr  (0x180064ce0ull,                                                  (void*)&RtCollision2D_Surface_Construct);
    RegisterRuntimeExport("?Reconstruct@Surface@Collision2D@@UEAAPEAVExternal@Object@@XZ", (void*)&RtCollision2D_Surface_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180064c70ull,                                                  (void*)&RtCollision2D_Surface_Reconstruct);
    RegisterRuntimeExport("?Destruct@Surface@Collision2D@@UEAAXXZ",       (void*)&RtCollision2D_Surface_Destruct);       ++n;
    RegisterRuntimeAddr  (0x180064d40ull,                                 (void*)&RtCollision2D_Surface_Destruct);
    RegisterRuntimeExport("??1Surface@Collision2D@@UEAA@XZ",              (void*)&RtCollision2D_Surface_Dtor);           ++n;
    RegisterRuntimeAddr  (0x18010ca00ull,                                 (void*)&RtCollision2D_Surface_Dtor);
    RegisterRuntimeExport("?Serialize@Surface@Collision2D@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtCollision2D_Surface_Serialize); ++n;
    RegisterRuntimeAddr  (0x18010ca40ull,                                                    (void*)&RtCollision2D_Surface_Serialize);
    return n;
}
