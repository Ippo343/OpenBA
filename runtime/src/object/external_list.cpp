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

#include "../../types/object/External.h"
#include "../../types/object/Internal.h"   // Object::detail::InternalMemAlloc
#include "../../types/collection/Collection.h"   // B3R: rtcoll::RtColl_ArraySetSize_18002f070

#include "../../types/io/Serializer.h"   // B.4 P2 X: the real IO::Serializer pair
#include "../../types/io/Stream.h"

namespace Object {

// ---- slot 5 — ?Serialize@ExternalList@Object@@UEAA_NAEAVStream@IO@@@Z  0x1800bfa20
// The body the retired `RtObject_ExternalList_Serialize` door carried, moved
// here unchanged (ENDGAME B.3.9 OBJ). B.4 P2 wave X: the hand-built pair, its
// file-local `Stream` / `Serializer` shadows and the raw encoding-factory slot
// call are the real `IO::Serializer` — its ctor (0x1800b9960) is the same
// `{stream, nullptr}` + `CreateReader` / `CreateWriter` (+0x18 / +0x20, by
// direction, version 0), and its dtor (0x1800b99b0) the same `if (rw) slot 0,
// flags 1` at the end of the body.
bool ExternalList::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);

    int32_t n = _count;
    ser._readerWriter->SerializeInt(n, nullptr);   // +0xa0

    if (stream._direction == IO::kReading) {
        rtcoll::RtColl_ArraySetSize_18002f070(&_resources, n);   // 0x18002f070
    }

    // ExternalPtr<Object::External>'s array form, an unexported internal:
    // 0x18002f170.
    SerializeExternalArray<External>(ser, n, _resources);

    return true;   // ~Serializer: `if (rw) delete rw` (slot 0, flags 1)
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
