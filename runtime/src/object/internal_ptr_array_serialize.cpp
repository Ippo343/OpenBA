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

#include "../../funcs/rt_port.h"
#include "../../types/object/Internal.h"

using RtObject::InternalBasePtr;
using RtObject::SerializerLite;

// Both are C++-linkage bodies in namespace RtObject (internal.cpp, type_system.cpp).
namespace RtObject {
void RtObject_InternalBasePtr_Serialize(InternalBasePtr* self, SerializerLite* ser);
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

extern "C" void RtObject_InternalPtrArray_Serialize(SerializerLite* ser, int count,
                                                    InternalBasePtr* elems, uint32_t typeId) {
    // TRANSITIONAL CAST (B31_RULES "not-yet-converted structs"): the callers
    // still spell the POD; it goes with the POD in step 2.
    using TypedPtr = ::Object::InternalPtr<RtObject::Internal>;
    TypedPtr::SerializeArray(*ser, count, reinterpret_cast<TypedPtr*>(elems), typeId);
}

#define INTPTR_ARRAY_THUNK(fnName, mangledTypeInfo)                             \
    extern "C" void fnName(SerializerLite* ser, int count, InternalBasePtr* elems) { \
        using TypeInfoPtr = const uint32_t*;                                    \
        const uint32_t* ti = RT_DATA_SLOT(TypeInfoPtr, mangledTypeInfo);        \
        if (ti == nullptr) {                                                    \
            static bool warned = false;                                         \
            if (!warned) {                                                      \
                warned = true;                                                  \
                std::fprintf(stderr, "[RtIntPtr] %s did not resolve -- this "   \
                             "instantiation cannot run its type check\n",       \
                             mangledTypeInfo);                                  \
            }                                                                   \
            return;                                                             \
        }                                                                       \
        RtObject_InternalPtrArray_Serialize(ser, count, elems, *ti);            \
    }

INTPTR_ARRAY_THUNK(RtObject_InternalPtrArraySerialize_AchievementInstance,
                   "?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A")
INTPTR_ARRAY_THUNK(RtObject_InternalPtrArraySerialize_GoalInstance,
                   "?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A")

#undef INTPTR_ARRAY_THUNK

int RtObjectInternalPtrArraySerializeRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x180009a20, (void*)&RtObject_InternalPtrArraySerialize_AchievementInstance); ++n;
    RegisterRuntimeAddr(0x180009fa0, (void*)&RtObject_InternalPtrArraySerialize_GoalInstance);        ++n;
    return n;
}
