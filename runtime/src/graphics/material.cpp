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
//   4  External::Compile    0x1800066e0  already ported (external.cpp), INHERITED
//   5  Serialize            0x180102a50  here
//   6  Reconstruct          0x180011ae0  here
//   7  Destruct             0x180011bc0  here
//   8  Construct            0x180011b50  here
//
// plus ~Material (0x180102a00, ??1Material@Graphics@@UEAA@XZ), which slot 0
// reaches through the port-aware address door.
//
// ENDGAME B.3 step 5 (block G): the file-local `struct Material` that used to
// carry the layout here MOVED to `runtime/types/graphics/Material.h` as the real
// `Graphics::Material` class (an `Object::External` subclass — read from
// `?_ParentTypeName@Material@Graphics@@` @0x1800118a0, not inferred), and the
// seven slot bodies below MOVED ONTO ITS MEMBERS. Every registered door keeps
// its mangled name AND its retail address and is now a one-line QUALIFIED
// non-virtual delegation. THE QUALIFICATION IS LOAD-BEARING (B34_BRIEFS A.1): a
// live `Material` wears the MINTED `??_7Material@Graphics@@6B@`, whose slots
// resolve back to these very doors, so an unqualified virtual call inside a door
// would dispatch into itself instead of delegating.
//
// Layout (sizeof 0x38, from the eh_vector_destructor_iterator call in slot 0),
// now asserted in the header rather than here:
//   +0x00  Object::External                 (0x20)
//   +0x20  int32_t   _type                  ?GetType@ 0x180011bd0; Serialize's uint
//   +0x28  Array<ExternalPtr<VideoProgram>> _passes  {data +0x28, size +0x30, cap +0x34}
//
// Serialize (0x180102a50) builds the IO::Serializer pair on its own stack
// exactly as MaterialInstance::Serialize does — {stream, readerWriter} — asks
// the stream's Encoding for the reader (slot +0x18) or writer (+0x20), then:
//   rw->SerializeUInt(&_type);  rw->SerializeInt(&n)
//   if reading: 0x1800604e0(&_passes, n)                Array<ptr>::SetSize (zero-filled)
//   0x1800605e0(&ser, n, _passes._data)                 ExternalPtr<VideoProgram>[] serialize
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
#include "../../types/graphics/Material.h"   // the real Graphics::Material

namespace {

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

// The three inlined base constructors, in retail's order (0x1801029a0, which
// ?Construct@ at 0x180011b50 tail-jumps to). IT IS NOT A CALL TO A BASE
// CONSTRUCTOR: retail open-codes the whole chain as three vptr stamps plus three
// zero stores, and the stamps name the MINTED tables, which is exactly right for
// the objects that reach this body. A member constructor here would stamp GCC's
// tables into a minted object.
void ConstructBody(Graphics::Material* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();                       // 0x1801029b1
    *vp = ExternalVTable();                   // 0x1801029bb
    self->_path.data      = nullptr;          // +0x08
    self->_refs._data     = nullptr;          // +0x10
    self->_refs._size     = 0;                // +0x18 (one qword with capacity)
    self->_refs._capacity = 0;
    *vp = MaterialVTable();                   // 0x1801029db
    self->_type           = 0;                // +0x20 dword
    self->_passes._data     = nullptr;        // +0x28
    self->_passes._size     = 0;              // +0x30 (one qword with capacity)
    self->_passes._capacity = 0;
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

}  // namespace

// ############################################################################
// `Graphics::Material`'s member bodies. Their own `namespace Graphics { }`
// block, after the anonymous namespace has closed, because a member definition
// has to appear in a namespace ENCLOSING the class (B34_BRIEFS A.2). The
// anonymous namespace's helpers are reachable unqualified from here: they are
// members of the global namespace's unnamed namespace, in this same TU.
// ############################################################################
namespace Graphics {

// [2] ?Type@Material@Graphics@@UEBAIXZ  0x1800118b0 — BaseTypeInfo +0x00.
uint32_t Material::Type() const {
    uint32_t v = 0;
    std::memcpy(&v, MaterialTypeInfo(), sizeof v);
    return v;
}

// [1] ?TypeName@Material@Graphics@@UEBAPEBDXZ  0x1800118d0 — BaseTypeInfo +0x08.
const char* Material::TypeName() const {
    const char* v = nullptr;
    std::memcpy(&v, MaterialTypeInfo() + 8, sizeof v);
    return v;
}

// [3] ?GetFileVersion@Material@Graphics@@UEBAHXZ  0x180102a40 — `MOV EAX,5; RET`.
// A LITERAL. B35_RULES section 6.1: `VertexLayout::GetFileVersion` returning 1
// instead of retail's 3 silently dropped every draw item once, so this is never
// derived from anything in our tree.
int32_t Material::GetFileVersion() const { return 5; }

// [8] ?Construct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ  0x180011b50
//     `TEST RCX,RCX ; JNZ 0x1801029a0 ; XOR EAX,EAX` — retail's own null test.
//     The test lives in the DOOR, which is where retail put it; by the time a
//     member runs, `this` is already non-null by construction.
Object::External* Material::Construct() {
    ConstructBody(this);
    return this;
}

// [6] ?Reconstruct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ  0x180011ae0
//     Slot 0 with flag 0 (destroy IN PLACE, not free), then the ctor on the same
//     storage. Slot 0 is dispatched VIRTUALLY on purpose — a subclass's deleting
//     destructor is what has to run.
Object::External* Material::Reconstruct() {
    CallSlot0(this, 0);
    ConstructBody(this);
    return this;
}

// [7] ?Destruct@Material@Graphics@@UEAAXXZ  0x180011bc0
void Material::Destruct() { CallSlot0(this, 0); }

// [5] ?Serialize@Material@Graphics@@UEAA_NAEAVStream@IO@@@Z  0x180102a50
bool Material::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };                // [RSP+0x20], [RSP+0x28]
    void**  encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                              : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void* enc, Serializer* ser, int version);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, &ser, 0);
    ser.readerWriter = rw;

    using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &_type, nullptr);
    int32_t n = _passes._size;
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->direction == 0) {
        using SetSizeFn = void (*)(void*, int);
        RtAddr<SetSizeFn>(0x1800604e0ull)(&_passes, n);
    }
    using ElemSerFn = void (*)(Serializer*, int, void*);
    RtAddr<ElemSerFn>(0x1800605e0ull)(&ser, n, _passes._data);

    if (rw != nullptr) {
        using DeleteFn = void (*)(ReaderWriter*, int);
        reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
    }
    return true;
}

// ??1Material@Graphics@@UEAA@XZ  0x180102a00 — EMPTY, AND THE DOOR BELOW KEEPS
// ITS HAND-WRITTEN BODY. The door stamps the MINTED table and releases `_passes`
// through retail's own out-of-line `~Array` at 0x180013da0, which is right for
// the minted objects that are the only ones alive today. A real destructor chain
// emits stamps naming GCC's tables and would run `~Array` a second time — the
// `Object::External::_refs` discussion in External.h, one level in. The two
// spellings coexist and each half of retail's decomposition runs exactly once.
Material::~Material() {}

}  // namespace Graphics

// ============================================================================
// THE DOORS. Each keeps its registered name and address and delegates with a
// QUALIFIED non-virtual call.
// ============================================================================

// ---- slots 2, 1, 3 ---------------------------------------------------------
extern "C" uint32_t RtGraphics_Material_Type(const void* selfv) {
    return reinterpret_cast<const Graphics::Material*>(selfv)      // B.3.5 bridge
               ->Graphics::Material::Type();
}
extern "C" const char* RtGraphics_Material_TypeName(const void* selfv) {
    return reinterpret_cast<const Graphics::Material*>(selfv)      // B.3.5 bridge
               ->Graphics::Material::TypeName();
}
extern "C" int32_t RtGraphics_Material_GetFileVersion(const void* selfv) {
    return reinterpret_cast<const Graphics::Material*>(selfv)      // B.3.5 bridge
               ->Graphics::Material::GetFileVersion();
}

// ---- slots 8, 6, 7 and the destructor ---------------------------------------
extern "C" void* RtGraphics_Material_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;               // retail's own null test
    return reinterpret_cast<Graphics::Material*>(selfv)            // B.3.5 bridge
               ->Graphics::Material::Construct();
}
extern "C" void* RtGraphics_Material_Reconstruct(void* selfv) {
    return reinterpret_cast<Graphics::Material*>(selfv)            // B.3.5 bridge
               ->Graphics::Material::Reconstruct();
}
extern "C" void RtGraphics_Material_Destruct(void* selfv) {
    reinterpret_cast<Graphics::Material*>(selfv)                   // B.3.5 bridge
        ->Graphics::Material::Destruct();
}
extern "C" void RtGraphics_Material_Dtor(void* selfv) {
    Graphics::Material* self = reinterpret_cast<Graphics::Material*>(selfv);  // B.3.5 bridge
    *reinterpret_cast<void**>(self) = MaterialVTable();
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180013da0ull)(&self->_passes);
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self);
}

// ---- slot 5 ------------------------------------------------------------------
extern "C" bool RtGraphics_Material_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<Graphics::Material*>(selfv)            // B.3.5 bridge
               ->Graphics::Material::Serialize(streamv);
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
