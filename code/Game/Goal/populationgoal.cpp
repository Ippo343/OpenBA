// code/Game/Goal/populationgoal.cpp
// PopulationGoal (0x38 bytes) — achievement goal requiring a population count.
// ENDGAME B.3.8 block GOAL: a REAL class (Goal/PopulationGoal.h) with a
// compiler-generated vtable; the retail evidence per slot is in the header.
//
// Fields (from Ghidra decompilation):
//   +0x30: int32_t  targetPop        — required population count (default 100)
//
// The world population count is at game+0xF0+0xA8.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/PopulationGoal.h"
#include "engine_funcs.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7: QUALIFIED base calls
#include "engine/UpdateLoop.h"  // ENDGAME B.3.7 callers (K2): UpdateLoop::IsRunning
#include <new>

// ctor 0x1400d78f0: Goal(), the vptr, targetPop = 100.
PopulationGoal::PopulationGoal() : targetPop(100), _pad34{} {}

// [0] 0x140026000: ~PopulationGoal (empty) -> ~Goal -> ~External, MemFree on &1.
Object::Base* PopulationGoal::DeletingDtor(unsigned flags) {
    this->~PopulationGoal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// PopulationGoal::Serialize (vtable +0x28, 0x1400d7870)
// ============================================================
// Calls Goal::Serialize, then serializes targetPop (int32 at +0x30).
bool PopulationGoal::Serialize(void* stream) {
    // Achievement::Goal::Serialize (slot 5, 0x18009ee20) — QUALIFIED (B7-13).
    Achievement::Goal::Serialize(stream);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize targetPop at +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(targetPop, nullptr);   // B.3.4 bridge

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    return true;
}

// ============================================================
// PopulationGoal::Reconstruct [6] 0x140025fb0 / Destruct [7] 0x140001640
// ============================================================
// retail 0x140025fb0: vtable[0](this, 0) (DeletingDtor(0): destroy in place),
// then the ctor on `this`.
Object::External* PopulationGoal::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) PopulationGoal();
}
void PopulationGoal::Destruct() { DeletingDtor(0); }

// ============================================================
// PopulationGoal::CheckGoalState (vtable +0x40, 0x1400d7820)
// ============================================================
//   if (g_pGameWorldSingleton != NULL && IsRunning(g_pGameWorldSingleton))
//       return *(int*)(*(int64_t*)(g_pGameWorldSingleton+0xF0) + 0xA8) >= targetPop;
//   return false;
Achievement::Goal::GoalState PopulationGoal::CheckGoalState() const {
    if (g_pGameWorldSingleton == nullptr) return static_cast<GoalState>(0);
    void* gameWorld = *g_pGameWorldSingleton;
    if (gameWorld == nullptr) return static_cast<GoalState>(0);

    // Check IsRunning — Engine::UpdateLoop::IsRunning (import slot 0x140152850),
    // non-virtual. The door's null-`this` test is covered by the check above.
    bool running = reinterpret_cast<const Engine::UpdateLoop*>(gameWorld)->IsRunning();   // B.3.7 bridge
    if (!running) return static_cast<GoalState>(0);

    // Read world data pointer at game+0xF0
    uintptr_t worldData = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    if (worldData == 0) return static_cast<GoalState>(0);

    // Compare population at worldData+0xA8 against targetPop
    int currentPop = *(int*)(worldData + 0xA8);
    return static_cast<GoalState>((currentPop >= targetPop) ? 1 : 0);
}

// ============================================================
// PopulationGoal::Construct (vtable +0x50, 0x140025f60)
// ============================================================
Object::External* PopulationGoal::Construct() {
    return ::new (static_cast<void*>(this)) PopulationGoal();
}

// create 0x140025f00: MemAlloc(0x38, 8), then the (inlined) ctor.
void* PopulationGoal::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PopulationGoal), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) PopulationGoal());
}
// createInPlace 0x140025f60.
void* PopulationGoal::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) PopulationGoal());
}

DEFINE_EXE_VTABLE(PopulationGoal, "_ZTV14PopulationGoal")
DEFINE_EXE_TYPE(PopulationGoal, "Achievement::Goal")
