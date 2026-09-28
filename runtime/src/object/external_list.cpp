// ============================================================================
// runtime/src/object/external_list.cpp — VTABLE BATCH 9, ENDGAME B.3.9 OBJ
//   Object::ExternalList: Serialize (slot 5) and retail's factory pair.
// ============================================================================
//
// Layout: runtime/types/object/External.h pins ExternalList as External at
// +0x00 plus `External** _resources` at +0x20, `int32_t _count` at +0x28 — the
// plain Array<External*> shape, sizeof 0x30 (the deleting destructor's own
// `MOV EDX,0x30`).
//
// ENDGAME B.3.9 OBJ (B39_RULES D1/D2): the class is CONCRETE and every live
// ExternalList wears the COMPILER's table. The minted `??_7ExternalList@Object@@6B@`
// (0x180154800), its RT_DELDTOR row 0x180007800, the FACTREST row
// (rt_factories_object.inc) and the eight slot doors that used to live here
// (TypeName / Type / GetFileVersion / Construct / Reconstruct / Destruct /
// Serialize / ??1) are retired: the minted table was their only reader. The
// slot bodies are members now:
//
//   0  DeletingDtor      0x180007800  External.h (both arms, element 0x30)
//   1  TypeName          0x180007370  external_class.cpp (the record accessor)
//   2  Type              0x180007350  external_class.cpp
//   3  GetFileVersion    0x1800bfa10  external_class_doors.cpp (returns 3)
//   4  External::Compile 0x1800066e0  inherited
//   5  Serialize         0x1800bfa20  HERE
//   6  Reconstruct       0x180007570  external_class_doors.cpp
//   7  Destruct          0x180007640  external_class_doors.cpp
//   8  Construct         0x1800075e0  external_class_doors.cpp
//   ~ExternalList        0x1800076c0  external_class_doors.cpp
//
// ---------------------------------------------------------------------------
// ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z   0x1800bfa20
// ---------------------------------------------------------------------------
// Same Serializer-pair shape every ported ElementDescription's Serialize uses
// (runtime/src/ui/groupdescription.cpp documents the pair and the retail
// encoding-vtable offsets at length; not re-derived here):
//
//   pair = { stream, nullptr }
//   rw   = stream->mode == 0 ? factory->vt[0x18](factory,&pair,0)
//                            : factory->vt[0x20](factory,&pair,0)
//   n    = this->_count
//   rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) SetSize(&_resources, n)     0x18002f070
//   ExternalPtrArraySerialize_External(&pair, n, _resources) 0x18002f170
//   if (rw) rw->vt[0](rw, 1)
//   return true
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/object/External.h"
#include "../../types/object/Internal.h"   // Object::detail::InternalMemAlloc

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// Same Serializer-pair shape as runtime/src/ui/groupdescription.cpp; retail
// vtable offsets into Encoding/ReaderWriter, RETAIL not profile
// (reference_encoding_vtable_offsets_differ_between_builds.md).
struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;             // +0x08  0 == reading
    int32_t _pad0c;
    void*   encodingFactory;  // +0x10
};
struct Serializer {
    Stream* stream;   // +0x00
    void*   readerWriter;  // +0x08
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

constexpr int kEncCreateReader = 0x18;   // CALL [RAX+0x18]
constexpr int kEncCreateWriter = 0x20;   // CALL [RAX+0x20]
// B3X W0: the ReaderWriter's [RAX+0xa0] (SerializeInt) and [RAX] (slot 0,
// the deleting destructor) are called by name below.

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

// The ExternalPtrArraySerialize_External thunk (runtime/src/object/
// external_ptr_serialize.cpp), reached by address since it is an unexported
// internal.
using ArraySerFn = void (*)(Serializer* ser, int count, Object::External** elems);

}  // namespace

namespace Object {

// ---- slot 5 — ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z  0x1800bfa20
// The body the retired `RtObject_ExternalList_Serialize` door carried, moved
// here unchanged (ENDGAME B.3.9 OBJ).
bool ExternalList::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    Serializer ser = { stream, nullptr };

    void* factory = stream->encodingFactory;
    void** fvt    = VTableOf(factory);
    const int which = (stream->mode == 0) ? kEncCreateReader : kEncCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;

    int32_t n = _count;
    reinterpret_cast<::IO::ReaderWriter*>(rw)->SerializeInt(n, nullptr);   // +0xa0 (B.3.x bridge)

    if (ser.stream->mode == 0) {
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x18002f070ull)(&_resources, n);
    }

    RtAddr<ArraySerFn>(0x18002f170ull)(&ser, n, _resources);

    if (rw != nullptr) {
        reinterpret_cast<::IO::ReaderWriter*>(rw)->DeletingDtor(1);        // slot 0 (B.3.x bridge)
    }
    return true;
}

// ?CreateNewObject@ExternalList@Object@@SAPEAVBase@2@XZ  0x180007460
// The FACTREST row this replaces (rt_factories_object.inc, 0x1800074c3..f4):
// `?MemAlloc@MemoryAllocator@System@@` of 0x30 bytes, align 8 (the same locked
// wrapper Object::detail::InternalMemAlloc reaches); retail's null branch; then
// the inlined constructor.
Base* ExternalList::CreateNewObject() {
    void* p = detail::InternalMemAlloc(sizeof(ExternalList), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ExternalList();
}

// ?CreateInPlaceObject@ExternalList@Object@@SAPEAVBase@2@PEAX@Z  0x180007510
// retail's null test, then the same inlined constructor.
Base* ExternalList::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ExternalList();
}

}  // namespace Object

// ============================================================================
// Registration — the factory pair (B39_RULES D2): the class's OWN statics by
// retail name AND VA. The class record (DEFINE_TYPE, external_class.cpp)
// names them directly.
// ============================================================================
int RtObjectExternalListRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@ExternalList@Object@@SAPEAVBase@2@XZ",
                          (void*)&Object::ExternalList::CreateNewObject); ++n;
    RegisterRuntimeAddr(0x180007460ull, (void*)&Object::ExternalList::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ExternalList@Object@@SAPEAVBase@2@PEAX@Z",
                          (void*)&Object::ExternalList::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr(0x180007510ull, (void*)&Object::ExternalList::CreateInPlaceObject);
    return n;
}
