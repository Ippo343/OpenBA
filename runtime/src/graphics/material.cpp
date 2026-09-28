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
// and are called by their body names (B3R).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>


#include "../../types/collection/Collection.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/graphics/Material.h"   // the real Graphics::Material

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;

// ENDGAME B.3.9 block GFX: `ConstructBody` (retail 0x1801029a0 open-coded as
// three MINTED vptr stamps plus the zero stores) and `CallSlot0` are gone: the
// stores are the real `Material()` (Material.h), the stamps the compiler's, and
// slot 0 is a virtual call on the class.

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
// ENDGAME B.3.9 TI-REST: Type / TypeName / TypeId are DECLARE_TYPE's bodies over the class's own
// record (graphics_type_records.cpp).

// [1] ?TypeName@Material@Graphics@@UEBAPEBDXZ  0x1800118d0 — BaseTypeInfo +0x08.

// [3] ?GetFileVersion@Material@Graphics@@UEBAHXZ  0x180102a40 — `MOV EAX,5; RET`.
// A LITERAL. B35_RULES section 6.1: `VertexLayout::GetFileVersion` returning 1
// instead of retail's 3 silently dropped every draw item once, so this is never
// derived from anything in our tree.
int32_t Material::GetFileVersion() const { return 5; }

// [8] ?Construct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ  0x180011b50
//     `TEST RCX,RCX ; JNZ 0x1801029a0 ; XOR EAX,EAX` — retail's own null test.
//     The test lives in the DOOR, which is where retail put it; by the time a
//     member runs, `this` is already non-null by construction.
//     (ENDGAME B.3.9 GFX: the null test sat in the retired door.) `::new`: the
//     class declares its own, undefined, placement operator new.
Object::External* Material::Construct() {
    return ::new (this) Material();
}

// [6] ?Reconstruct@Material@Graphics@@UEAAPEAVExternal@Object@@XZ  0x180011ae0
//     Slot 0 with flag 0 (destroy IN PLACE, not free), then the ctor on the same
//     storage. Slot 0 is dispatched VIRTUALLY on purpose — a subclass's deleting
//     destructor is what has to run.
Object::External* Material::Reconstruct() {
    this->~Material();
    return ::new (this) Material();
}

// [7] ?Destruct@Material@Graphics@@UEAAXXZ  0x180011bc0
void Material::Destruct() { this->~Material(); }

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

    // B3X W0: by name on the real class. `_type` goes through +0x98
    // `SerializeUInt` (retail's slot), so it is viewed as the uint32 it is.
    IO::ReaderWriter* const irw = reinterpret_cast<IO::ReaderWriter*>(rw);  // B.3.x bridge
    irw->SerializeUInt(*reinterpret_cast<uint32_t*>(&_type), nullptr);
    int32_t n = _passes._size;
    irw->SerializeInt(n, nullptr);

    if (ser.stream->direction == 0) {
        rtcoll::RtColl_ArraySetSize_1800604e0(&_passes, n);             // 0x1800604e0
    }
    RtObject_ExternalPtrArraySerialize_VideoProgram(                     // 0x1800605e0
        &ser, n, reinterpret_cast<RtObject::External**>(_passes._data));

    if (rw != nullptr) {
        delete irw;
    }
    return true;
}

// ??1Material@Graphics@@UEAA@XZ  0x180102a00 — retail's body, moved here from
// the retired `RtGraphics_Material_Dtor` door (ENDGAME B.3.9 block GFX): the
// restamp (the compiler's), `_passes` through retail's own out-of-line ~Array at
// 0x180013da0 (by body name), then ~External (the compiler's
// chain). 0x180013da0 frees AND zeroes the header, so the member's implicit
// ~Array that runs after this body sees an empty header: no element release,
// MemFree(null) -- `~Array` does not run a second time on live data.
Material::~Material() {
    rtcoll::RtColl_ArrayDtor_180013da0(&_passes);
}

// ?TypeId@Material@Graphics@@SAIXZ 0x1800118c0 -- the static twin of slot 2:
// the minted record's +0x00 until the W3 fold (B39_RULES D6 step 1).

// ?CreateNewObject@Material@Graphics@@SAPEAVBase@Object@@XZ 0x1800119c0 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x38, align 8; retail's null
// branch; the inlined ctor. Replaces the FACTREST row (B39_RULES D2).
Object::Base* Material::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Material), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) Material();
}
// ?CreateInPlaceObject@Material@Graphics@@SAPEAVBase@Object@@PEAX@Z 0x180011a70
Object::Base* Material::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) Material();
}

}  // namespace Graphics

// ============================================================================
// ENDGAME B.3.9 block GFX: the slot doors (Type, TypeName, GetFileVersion,
// Construct, Reconstruct, Destruct, Serialize) and the ??1 door are gone --
// their only readers were the minted `??_7Material@Graphics@@6B@` and its
// RT_DELDTOR row. Every live Material wears the compiler's table.
// ============================================================================
