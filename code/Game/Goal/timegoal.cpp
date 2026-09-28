// code/Game/Goal/timegoal.cpp
// TimeGoal (0x38 bytes) — achievement goal requiring elapsed time.
// ENDGAME B.3.8 block GOAL: a REAL class (Goal/TimeGoal.h) with a
// compiler-generated vtable; the retail evidence per slot is in the header.
//
// Fields:
//   +0x30: int32  requiredYears         — years to survive (multiplied by 0xe10=3600 for seconds)
//   +0x34: bool   requireOtherGoalsMet  — if set, time only counts when other goals are met
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/TimeGoal.h"
#include "Goal/TimeGoalInstance.h"
#include "engine_funcs.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7: QUALIFIED base calls
#include <new>

// ctor 0x1400dabb0 (inlined in create 0x1400266f0): Goal(), the vptr,
// requiredYears = 1, requireOtherGoalsMet = 0.
TimeGoal::TimeGoal() : requiredYears(1), requireOtherGoalsMet(0), _pad35{} {}

// [0] 0x140026850: ~TimeGoal (empty) -> ~Goal -> ~External, MemFree on &1.
Object::Base* TimeGoal::DeletingDtor(unsigned flags) {
    this->~TimeGoal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// TimeGoal::Serialize (vtable +0x28, 0x1400dab20)
// ============================================================
// Calls base Goal::Serialize, then requiredYears (int) and
// requireOtherGoalsMet (bool).
bool TimeGoal::Serialize(void* stream) {
    // Achievement::Goal::Serialize (slot 5, 0x18009ee20) — QUALIFIED (B7-13).
    Achievement::Goal::Serialize(stream);

    // Create serializer
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize requiredYears (+0x30)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(requiredYears, nullptr);   // B.3.4 bridge

    // Serialize requireOtherGoalsMet (+0x34)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<bool*>(&requireOtherGoalsMet), nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// TimeGoal::Reconstruct [6] 0x1400267a0 / Destruct [7] 0x140001640
// ============================================================
// DeletingDtor(0) (destroy in place), then the ctor on `this`.
Object::External* TimeGoal::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) TimeGoal();
}
void TimeGoal::Destruct() { DeletingDtor(0); }

// ============================================================
// TimeGoal::GetInstanceType [9] 0x1400267f0
// ============================================================
// `MOV EAX,[TypeInfo_TimeGoalInstance]; MOV [RDX],EAX; MOV RAX,RDX; RET` —
// the B7-4 retslot shape of Achievement::Goal::GetInstanceType.
Object::Type* TimeGoal::GetInstanceType(Object::Type* ret) const {
    ret->_type = TimeGoalInstance::TypeId();
    return ret;
}

// ============================================================
// TimeGoal::Construct (vtable +0x50, 0x140026750)
// ============================================================
Object::External* TimeGoal::Construct() {
    return ::new (static_cast<void*>(this)) TimeGoal();
}

// create 0x1400266f0: MemAlloc(0x38, 8), then the (inlined) ctor.
void* TimeGoal::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TimeGoal), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TimeGoal());
}
// createInPlace 0x140026750.
void* TimeGoal::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TimeGoal());
}

DEFINE_EXE_TYPE(TimeGoal, "Achievement::Goal")
