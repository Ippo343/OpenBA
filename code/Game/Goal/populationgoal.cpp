// code/Game/Goal/populationgoal.cpp
// Clean reimplementation of PopulationGoal vtable methods.
//
// PopulationGoal (0x38 bytes) — achievement goal requiring population count.
// Vtable: 0x140160370
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint8_t  _goalBase[0x28]  — Goal base fields
//   +0x30: int32_t  targetPop        — required population count (default 100)
//   +0x34: uint8_t  _pad34[4]        — padding
//
// The world population count is at game+0xF0+0xA8.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/populationgoal_funcs.h"
#include "engine_funcs.h"
#include "Goal/goal_exe_helpers.h"
#include "resolve.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// Vtable pointer for PopulationGoal (resolved at runtime)
static void* s_vtable_PopulationGoal = nullptr;
static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_PopulationGoal = (void*)ResolveAddr(Module::Exe, 0x140160370);
    s_resolved = true;
}

// ============================================================
// PopulationGoal::Dtor (vtable +0x00, 0x140026000)
// ============================================================
// Scalar destructor: sets vtable, calls Goal::~Goal, optionally frees.
void* PopulationGoal_Dtor(PopulationGoal* self, uint32_t flags) {
    EnsureResolved();

    // Set vtable to PopulationGoal
    *(void**)self = s_vtable_PopulationGoal;

    // Call Goal base destructor
    if (fn_Goal_BaseDtor) {
        fn_Goal_BaseDtor(self);
    }

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// PopulationGoal::Slot5 (vtable +0x28, 0x1400d7870)
// ============================================================
// Serialize/Load: calls Goal::Serialize base, then serializes
// targetPop (int32 at +0x30) via Serializer.
int PopulationGoal_Slot5(PopulationGoal* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Call Goal base serialize
    if (fn_Goal_BaseSerialize) {
        fn_Goal_BaseSerialize(self, stream);
    }

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize targetPop at +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    return 1;
}

// ============================================================
// PopulationGoal::Slot6 (vtable +0x30, 0x140025fb0)
// ============================================================
// Factory constructor: calls Goal::Goal() base ctor, sets vtable,
// initializes targetPop to 100.
// Returns null if input is null.
PopulationGoal* PopulationGoal_Slot6(PopulationGoal* self) {
    EnsureResolved();

    if (self == nullptr) {
        return nullptr;
    }

    // Call Goal base default constructor
    if (fn_Goal_BaseCtor) {
        fn_Goal_BaseCtor(self);
    }

    // Set vtable to PopulationGoal
    *(void**)self = s_vtable_PopulationGoal;

    // Initialize targetPop to 100
    self->targetPop = 100;

    return self;
}

// ============================================================
// PopulationGoal::Slot8 (vtable +0x40, 0x1400d7820)
// ============================================================
// Check goal completion: compares world population against targetPop.
//
// Decompiled:
//   if (g_pGameWorldSingleton != NULL) {
//       if (IsRunning(g_pGameWorldSingleton)) {
//           return *(int*)(*(int64_t*)(g_pGameWorldSingleton+0xF0) + 0xA8) >= self->targetPop;
//       }
//   }
//   return false;
//
// NOTE: Hook is DISABLED due to RIP-relative MOV in prologue.
// This clean reimplementation works regardless of hook status.
int PopulationGoal_Slot8(PopulationGoal* self) {
    uintptr_t s = (uintptr_t)self;

    if (g_pGameWorldSingleton == nullptr) return 0;
    void* gameWorld = *g_pGameWorldSingleton;
    if (gameWorld == nullptr) return 0;

    // Check IsRunning
    if (fn_IsRunning) {
        bool running = fn_IsRunning(gameWorld);
        if (!running) return 0;
    }

    // Read world data pointer at game+0xF0
    uintptr_t worldData = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    if (worldData == 0) return 0;

    // Compare population at worldData+0xA8 against targetPop at self+0x30
    int currentPop = *(int*)(worldData + 0xA8);
    int targetPop = *(int*)(s + 0x30);

    return (currentPop >= targetPop) ? 1 : 0;
}

// ============================================================
// PopulationGoal::CreateInstance (vtable +0x50, 0x140025f60)
// ============================================================
// Same as Slot6 — constructs a new PopulationGoal in-place.
PopulationGoal* PopulationGoal_CreateInstance(PopulationGoal* self) {
    return PopulationGoal_Slot6(self);
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// ============================================================
// PopulationGoal vtable (exe addr 0x140160370, 11 slots)
// ============================================================
static uint32_t PopulationGoal_GetTypeId_Std(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PopulationGoal");
    return cached;
}

static int PopulationGoal_Slot3_Trampoline(PopulationGoal* self) { return PopulationGoal_Slot3(self); }

static void* g_PopulationGoal_Vtable[11] = {
    reinterpret_cast<void*>(&PopulationGoal_Dtor),                // +0x00 dtor
    nullptr,                                                      // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&PopulationGoal_GetTypeId_Std),       // +0x10 GetTypeId
    reinterpret_cast<void*>(&PopulationGoal_Slot3_Trampoline),    // +0x18 Slot3 (returns 1)
    nullptr,                                                      // +0x20 Slot4 (Goal JMP thunk)
    reinterpret_cast<void*>(&PopulationGoal_Slot5),               // +0x28 Slot5 / Load
    reinterpret_cast<void*>(&PopulationGoal_Slot6),               // +0x30 Slot6 / Cleanup
    nullptr,                                                      // +0x38 Slot7 (base stub)
    reinterpret_cast<void*>(&PopulationGoal_Slot8),               // +0x40 Slot8 / Check
    nullptr,                                                      // +0x48 Slot9 (base stub)
    reinterpret_cast<void*>(&PopulationGoal_CreateInstance),      // +0x50 Slot10 / CreateInstance
};

extern "C" void RegisterPopulationGoalVtable() {
    Goal_FillInheritedVtableSlots(g_PopulationGoal_Vtable, "PopulationGoal");
    RegisterExeAddr(0x140160370, reinterpret_cast<uintptr_t>(g_PopulationGoal_Vtable));
}

extern "C" void* PopulationGoal_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return PopulationGoal_Slot6(reinterpret_cast<PopulationGoal*>(p));
}

extern "C" void* PopulationGoal_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return PopulationGoal_Slot6(reinterpret_cast<PopulationGoal*>(memory));
}

REGISTER_TYPE(PopulationGoal, "Achievement::Goal",
              &PopulationGoal_Factory, &PopulationGoal_CreateInPlace);
