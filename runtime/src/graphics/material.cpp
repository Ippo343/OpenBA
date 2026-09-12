// ============================================================================
// runtime/src/graphics/material.cpp — Graphics::Material, the whole vtable
// ============================================================================
//
// ??_7Material@Graphics@@6B@ (retail 0x180154d10), nine slots:
//
//   0  deleting dtor        0x180011d20  runtime/src/object/deleting_dtor.cpp row 50
//   1  TypeName             0x1800118d0  here
//   2  Type                 0x1800118b0  here
//   3  GetFileVersion       0x180102a40  here  (returns 5)
//   4  External::Compile    0x1800066e0  already ported (external.cpp)
//   5  Serialize            0x180102a50  here
//   6  Reconstruct          0x180011ae0  here
//   7  Destruct             0x180011bc0  here
//   8  Construct            0x180011b50  here
//
// plus ~Material (0x180102a00, ??1Material@Graphics@@UEAA@XZ), which slot 0
// reaches through the port-aware address door.
//
// Layout (sizeof 0x38, from the eh_vector_destructor_iterator call in slot 0):
//   +0x00  Object::External                 (0x20)
//   +0x20  uint32_t  _shaderType            slot 0x98 (uint) in Serialize
//   +0x28  Array<ExternalPtr<VideoProgram>> _programs  {data +0x28, size +0x30, cap +0x34}
//
// Serialize (0x180102a50) builds the IO::Serializer pair on its own stack
// exactly as MaterialInstance::Serialize does — {stream, readerWriter} — asks
// the stream's Encoding for the reader (slot +0x18) or writer (+0x20), then:
//   rw->SerializeUInt(&_shaderType);  rw->SerializeInt(&n)
//   if reading: 0x1800604e0(&_programs, n)              Array<ptr>::SetSize (zero-filled)
//   0x1800605e0(&ser, n, _programs._data)               ExternalPtr<VideoProgram>[] serialize
//   if (rw) rw->slot0(rw, 1)                            delete the reader/writer
// Both helpers are ported (collection/array.cpp, object/external_ptr_serialize.cpp)
// and are reached through the same address doors retail's CALLs name.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"

namespace {

struct Material {
    RtObject::External        base;         // +0x00
    uint32_t                  _shaderType;  // +0x20
    uint32_t                  _pad24;       // +0x24
    Collection::Array<void*>  _programs;    // +0x28  ExternalPtr<Video::VideoProgram>
};
static_assert(sizeof(Material) == 0x38, "Graphics::Material is 0x38 bytes");
static_assert(offsetof(Material, _shaderType) == 0x20, "Material::_shaderType @ +0x20");
static_assert(offsetof(Material, _programs)   == 0x28, "Material::_programs @ +0x28");

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;

void* BaseVTable()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* MaterialVTable() { return RT_DATA_SLOT(void*, "??_7Material@Graphics@@6B@"); }
const uint8_t* MaterialTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A");
}

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

// The three inlined base constructors, in retail's order (0x180011b50).
void ConstructBody(Material* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data     = nullptr;      // +0x08
    self->base._refs._data    = nullptr;      // +0x10
    self->base._refs._size    = 0;            // +0x18 (one qword with capacity)
    self->base._refs._capacity = 0;
    *vp = MaterialVTable();
    self->_shaderType         = 0;            // +0x20 dword
    self->_programs._data     = nullptr;      // +0x28
    self->_programs._size     = 0;            // +0x30 (one qword with capacity)
    self->_programs._capacity = 0;
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

}  // namespace

// ---- slots 2, 1, 3 ---------------------------------------------------------
extern "C" uint32_t RtGraphics_Material_Type(const void*) {
    uint32_t v = 0;
    std::memcpy(&v, MaterialTypeInfo(), sizeof v);      // BaseTypeInfo +0x00
    return v;
}
extern "C" const char* RtGraphics_Material_TypeName(const void*) {
    const char* v = nullptr;
    std::memcpy(&v, MaterialTypeInfo() + 8, sizeof v);  // BaseTypeInfo +0x08
    return v;
}
extern "C" int32_t RtGraphics_Material_GetFileVersion(const void*) {
    return 5;                                           // MOV EAX,5 ; RET
}

// ---- slots 8, 6, 7 and the destructor ---------------------------------------
extern "C" void* RtGraphics_Material_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;               // retail's own null test
    ConstructBody(static_cast<Material*>(selfv));
    return selfv;
}
extern "C" void* RtGraphics_Material_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                // destroy in place
    ConstructBody(static_cast<Material*>(selfv));
    return selfv;
}
extern "C" void RtGraphics_Material_Destruct(void* selfv) {
    CallSlot0(selfv, 0);
}
extern "C" void RtGraphics_Material_Dtor(void* selfv) {
    Material* self = static_cast<Material*>(selfv);
    *reinterpret_cast<void**>(self) = MaterialVTable();
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180013da0ull)(&self->_programs);
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ---- slot 5 ------------------------------------------------------------------
extern "C" bool RtGraphics_Material_Serialize(void* selfv, void* streamv) {
    Material* self   = static_cast<Material*>(selfv);
    Stream*   stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                // [RSP+0x20], [RSP+0x28]
    void**  encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void* enc, Serializer* ser, int version);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &self->_shaderType, nullptr);
    int32_t n = self->_programs._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->direction == 0) {
        using SetSizeFn = void (*)(void*, int);
        RtAddr<SetSizeFn>(0x1800604e0ull)(&self->_programs, n);
    }
    using ElemSerFn = void (*)(Serializer*, int, void*);
    RtAddr<ElemSerFn>(0x1800605e0ull)(&ser, n, self->_programs._data);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

int RtGraphicsMaterialRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@Material@Graphics@@UEBAIXZ",            (void*)&RtGraphics_Material_Type);           ++n;
    RegisterRuntimeAddr  (0x1800118b0ull,                                 (void*)&RtGraphics_Material_Type);
    RegisterRuntimeExport("?TypeName@Material@Graphics@@UEBAPEBDXZ",      (void*)&RtGraphics_Material_TypeName);       ++n;
    RegisterRuntimeAddr  (0x1800118d0ull,                                 (void*)&RtGraphics_Material_TypeName);
    RegisterRuntimeExport("?GetFileVersion@Material@Graphics@@UEBAHXZ",   (void*)&RtGraphics_Material_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x180102a40ull,                                 (void*)&RtGraphics_Material_GetFileVersion);
    RegisterRuntimeExport("?Construct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ",   (void*)&RtGraphics_Material_Construct);   ++n;
    RegisterRuntimeAddr  (0x180011b50ull,                                                (void*)&RtGraphics_Material_Construct);
    RegisterRuntimeExport("?Reconstruct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ", (void*)&RtGraphics_Material_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180011ae0ull,                                                (void*)&RtGraphics_Material_Reconstruct);
    RegisterRuntimeExport("?Destruct@Material@Graphics@@UEAAXXZ",         (void*)&RtGraphics_Material_Destruct);       ++n;
    RegisterRuntimeAddr  (0x180011bc0ull,                                 (void*)&RtGraphics_Material_Destruct);
    RegisterRuntimeExport("??1Material@Graphics@@UEAA@XZ",                (void*)&RtGraphics_Material_Dtor);           ++n;
    RegisterRuntimeAddr  (0x180102a00ull,                                 (void*)&RtGraphics_Material_Dtor);
    RegisterRuntimeExport("?Serialize@Material@Graphics@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtGraphics_Material_Serialize); ++n;
    RegisterRuntimeAddr  (0x180102a50ull,                                          (void*)&RtGraphics_Material_Serialize);
    return n;
}
