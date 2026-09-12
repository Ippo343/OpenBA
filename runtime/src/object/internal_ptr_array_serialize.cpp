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

extern "C" void RtObject_InternalPtrArray_Serialize(SerializerLite* ser, int count,
                                                    InternalBasePtr* elems, uint32_t typeId) {
    for (int i = 0; i < count; ++i) {
        RtObject::RtObject_InternalBasePtr_Serialize(&elems[i], ser);
        void* obj = *reinterpret_cast<void**>(&elems[i]);
        if (obj != nullptr && !RtObject::RtObject_Base_IsDerivedFrom(obj, typeId))
            *reinterpret_cast<void**>(&elems[i]) = nullptr;
    }
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
