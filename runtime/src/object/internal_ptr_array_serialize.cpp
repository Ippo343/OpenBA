// ============================================================================
// runtime/src/object/internal_ptr_array_serialize.cpp
//
// The array form of InternalPtr<T>::Serialize — 180 bytes, two instantiations
// reached by the cutover closure (docs/runtime_port/closure/WORKLIST.md):
//
//     0x180009a20  T = Achievement::AchievementInstance
//     0x180009fa0  T = Achievement::GoalInstance
//
//     void (Serializer* ser, int count, InternalBasePtr* elems)   RCX, EDX, R8
//
// Retail, per element:
//     InternalBasePtr::Serialize(&elems[i], ser);
//     obj = elems[i].ptr;
//     if (obj && obj->GetTypeId() != T::typeId) {
//         rec = Factory::_singleton->FindNode(obj->GetTypeId()) + 8;   // unguarded
//         while (rec->typeId != T::typeId) { rec = rec->parent; if (!rec) { elems[i].ptr = 0; break; } }
//     }
//
// The walk after the load is exactly Object::Base::IsDerivedFrom (type_system.cpp),
// including the unguarded FindNode result, so the port calls that body rather
// than transcribing the walk a third time. T's typeId is the first dword of its
// `?_typeInfo@` node, read through the name door so it follows a mint promotion.
// ============================================================================
#include <cstdint>
#include <cstdio>

#include "../../types/object/Internal.h"
#include "../../types/achievement/AchievementManager.h"   // ENDGAME B.3.9 TI-REST: T::TypeId()
#include "../../types/achievement/Goal.h"

using RtObject::InternalBasePtr;

// Both are C++-linkage bodies in namespace RtObject (internal.cpp, type_system.cpp).
// B.3.4 block 3: the serializer parameter is the REAL `IO::Serializer`; it was
// `RtObject::SerializerLite`, which died with this block. Spelled `::IO::` in
// full so the declaration cannot drift from internal.cpp's definition — a
// mismatch here would be a link error, not a silent one, but naming it exactly
// is cheaper than proving that each time.
namespace RtObject {
bool RtObject_Base_IsDerivedFrom(const void* self, uint32_t typeId);
}

// ---------------------------------------------------------------------------
// ENDGAME B.3.1 step 1. The body is now
// `Object::InternalPtr<T>::SerializeArray` (runtime/types/object/Internal.h),
// one template body for both instantiations — re-verified 2026-09-13,
// function-size-bounded, at identical=55 / position-only=2 / REAL=1, the one
// REAL instruction being the `_typeInfo` load. `T`'s type id is still supplied
// by the caller through the mangled-name door, because none of the two `T`s is
// a C++ class in this tree yet.
//
// The `IsDerivedFrom` customization point the class declares: `Object::Base` is
// unconverted (SMARTPTR_INVENTORY.md §4), so it stays reached through the same
// ported body the C form called.
// ---------------------------------------------------------------------------
namespace Object {
bool InternalPtrIsDerivedFrom(const void* obj, uint32_t typeId) {
    return RtObject::RtObject_Base_IsDerivedFrom(obj, typeId);
}
}  // namespace Object

extern "C" void RtObject_InternalPtrArray_Serialize(::IO::Serializer* ser, int count,
                                                    InternalBasePtr* elems, uint32_t typeId) {
    // TRANSITIONAL CAST (B31_RULES "not-yet-converted structs"): the callers
    // still spell the POD; it goes with the POD in step 2.
    using TypedPtr = ::Object::InternalPtr<RtObject::Internal>;
    TypedPtr::SerializeArray(*ser, count, reinterpret_cast<TypedPtr*>(elems), typeId);  // LEFT: H10 RtObject:: POD mirror of the Object:: class (named in other blocks' files; wave X)
}

// ENDGAME B.3.9 TI-REST (B39_RULES D6): T's type id is the class's own
// `DEFINE_TYPE` record's, read fresh per call (retail: the first dword of
// ?_typeInfo@T@Achievement@@, 0x1801f98d0 / 0x1801f9940). The by-name thunk
// macro over the minted records went with them.
extern "C" void RtObject_InternalPtrArraySerialize_AchievementInstance(
        ::IO::Serializer* ser, int count, InternalBasePtr* elems) {
    RtObject_InternalPtrArray_Serialize(ser, count, elems, Achievement::AchievementInstance::TypeId());
}
extern "C" void RtObject_InternalPtrArraySerialize_GoalInstance(
        ::IO::Serializer* ser, int count, InternalBasePtr* elems) {
    RtObject_InternalPtrArray_Serialize(ser, count, elems, Achievement::GoalInstance::TypeId());
}
