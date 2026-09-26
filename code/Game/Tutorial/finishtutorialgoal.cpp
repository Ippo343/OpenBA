// code/Game/Goal/finishtutorialgoal.cpp
// Clean reimplementation of FinishTutorialGoal vtable methods.
//
// FinishTutorialGoal (0x38 bytes) — achievement goal for completing tutorial.
// Vtable: 0x140160420
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint8_t  _goalBase[0x28]  — Goal base fields
//   +0x30: void*    tutorialRef      — External* to Tutorial object
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/finishtutorialgoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "achievement/Goal.h"   // ENDGAME B.3.7 callers (K2): QUALIFIED base calls
#include <new>

// Vtable pointer for FinishTutorialGoal (resolved at runtime)
static void* s_vtable_FinishTutorialGoal = nullptr;
// Vtable pointer for FinishTutorialInstance (resolved at runtime)
static void* s_vtable_FinishTutorialInstance = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_FinishTutorialGoal = (void*)ResolveAddr(Module::Exe, 0x140160420);
    s_vtable_FinishTutorialInstance = (void*)ResolveAddr(Module::Exe, 0x140160478);
    s_resolved = true;
}

// ============================================================
// FinishTutorialGoal::Dtor (vtable +0x00, 0x140026260)
// ============================================================
// Scalar destructor: sets vtable, calls Goal::~Goal, optionally frees.
void* FinishTutorialGoal_Dtor(FinishTutorialGoal* self, uint32_t flags) {
    EnsureResolved();

    // Set vtable to FinishTutorialGoal (for correct virtual dispatch during destruction)
    *(void**)self = s_vtable_FinishTutorialGoal;

    // Achievement::Goal::~Goal (0x18009edb0) — a QUALIFIED member call (B7-13).
    reinterpret_cast<Achievement::Goal*>(self)->Achievement::Goal::~Goal();   // B.3.7 bridge

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// FinishTutorialGoal::Slot5 (vtable +0x28, 0x1400d7970)
// ============================================================
// Serialize/Load: calls Goal::Serialize base, then serializes
// the tutorial External* at +0x30 using the tutorial-specific
// External serializer.
int FinishTutorialGoal_Slot5(FinishTutorialGoal* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Achievement::Goal::Serialize (slot 5, 0x18009ee20) — QUALIFIED (B7-13): an
    // unqualified call would dispatch through this exe table back into this slot.
    reinterpret_cast<Achievement::Goal*>(self)->Achievement::Goal::Serialize(stream);   // B.3.7 bridge

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize the tutorial External* at +0x30
    if (fn_ExternalPtr_SerializeTutorial) {
        fn_ExternalPtr_SerializeTutorial((void*)(s + 0x30), serializer);
    }

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge

    return 1;
}

// ============================================================
// FinishTutorialGoal::Slot6 (vtable +0x30, 0x1400261a0)
// ============================================================
// Factory constructor: calls Goal::Goal() base ctor, sets vtable,
// zeroes the tutorial reference at +0x30.
// Returns null if input is null.
FinishTutorialGoal* FinishTutorialGoal_Slot6(FinishTutorialGoal* self) {
    EnsureResolved();

    if (self == nullptr) {
        return nullptr;
    }

    // Call Goal base default constructor
    if (fn_Goal_BaseCtor) {
        fn_Goal_BaseCtor(self);
    }

    // Set vtable to FinishTutorialGoal
    *(void**)self = s_vtable_FinishTutorialGoal;

    // Zero the tutorial reference
    *(uint64_t*)((uintptr_t)self + 0x30) = 0;

    return self;
}

// ============================================================
// FinishTutorialGoal::CreateInstance (vtable +0x50, 0x140026150)
// ============================================================
// Same as Slot6 — constructs a new FinishTutorialGoal in-place.
FinishTutorialGoal* FinishTutorialGoal_CreateInstance(FinishTutorialGoal* self) {
    return FinishTutorialGoal_Slot6(self);
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// ============================================================
// FinishTutorialGoal vtable (exe addr 0x140160420, 12 slots)
// ============================================================
static uint32_t FinishTutorialGoal_GetTypeId_Std(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FinishTutorialGoal");
    return cached;
}

// Slot +0x48: the exe OVERRIDE of Achievement::Goal::GetInstanceType (retail
// 0x1400261f0): returns FinishTutorialInstance's type id BY VALUE.
// Original: MOV EDX,[TypeInfo_FinishTutorialInstance]; MOV [RDX_retval],EDX; MOV RAX,RDX; RET.
// RULE B7-4: the override takes the base's retslot shape
// `Object::Type* (const Goal* this, Object::Type* ret)` — `this` first, the
// hidden return slot second, the slot handed back in RAX — exactly what
// `virtual Object::Type* Goal::GetInstanceType(Object::Type* ret) const`
// compiles to (the TimeGoal override has the same shape).
static Object::Type* FinishTutorialGoal_Slot9_Standalone(const Achievement::Goal* /*self*/, Object::Type* retval) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FinishTutorialInstance");
    if (retval) retval->_type = cached;
    return retval;
}

// Trampolines for inline-defined helpers in finishtutorialgoal_funcs.h.
static int FinishTutorialGoal_Slot3_Trampoline(FinishTutorialGoal* self) {
    return FinishTutorialGoal_Slot3(self);
}
static int FinishTutorialGoal_Slot8_Trampoline(FinishTutorialGoal* self) {
    return FinishTutorialGoal_Slot8(self);
}

static void* g_FinishTutorialGoal_Vtable[12] = {
    reinterpret_cast<void*>(&FinishTutorialGoal_Dtor),                  // +0x00 dtor
    nullptr,                                                            // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&FinishTutorialGoal_GetTypeId_Std),         // +0x10 GetTypeId
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot3_Trampoline),      // +0x18 Slot3 (returns 1)
    nullptr,                                                            // +0x20 Slot4 (Goal JMP thunk)
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot5),                 // +0x28 Slot5 / Load
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot6),                 // +0x30 Slot6 / Cleanup
    nullptr,                                                            // +0x38 Slot7 (base stub)
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot8_Trampoline),      // +0x40 Slot8 (returns 0)
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot9_Standalone),      // +0x48 Slot9 / GetInstanceTypeId
    reinterpret_cast<void*>(&FinishTutorialGoal_CreateInstance),        // +0x50 Slot10 / CreateInstance
    reinterpret_cast<void*>(&FinishTutorialGoal_Slot11),                // +0x58 Slot11 / SharedDtor
};

extern "C" void RegisterFinishTutorialGoalVtable() {
    RegisterExeAddr(0x140160420, reinterpret_cast<uintptr_t>(g_FinishTutorialGoal_Vtable));
}

extern "C" void* FinishTutorialGoal_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);  // size = 0x38
    if (!p) return nullptr;
    return FinishTutorialGoal_Slot6(reinterpret_cast<FinishTutorialGoal*>(p));
}

extern "C" void* FinishTutorialGoal_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return FinishTutorialGoal_Slot6(reinterpret_cast<FinishTutorialGoal*>(memory));
}

REGISTER_TYPE(FinishTutorialGoal, "Achievement::Goal",
              &FinishTutorialGoal_Factory, &FinishTutorialGoal_CreateInPlace);

// ============================================================
// FinishTutorialGoal::Slot11 / FinishTutorialInstance::Dtor
// (vtable +0x58, 0x140026200)
// ============================================================
// Scalar destructor for FinishTutorialInstance:
//   1. Sets vtable to FinishTutorialInstance
//   2. Deletes InternalBasePtr at +0x20
//   3. Calls GoalInstance::~GoalInstance
//   4. Optionally frees memory
void* FinishTutorialGoal_Slot11(void* self, uint32_t flags) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Set vtable to FinishTutorialInstance
    *(void**)s = s_vtable_FinishTutorialInstance;

    // Delete InternalBasePtr at +0x20
    if (fn_InternalBasePtr_DeletePtr) {
        reinterpret_cast< ::Object::InternalBasePtr*>((void*)(s + 0x20))->DeletePtr();   // B.3.3 bridge
    }

    // Achievement::GoalInstance::~GoalInstance (retail 0x18009ef40) — a
    // QUALIFIED member call (B7-13): re-stamp + ~Internal (the InternalFactory
    // swap-remove).
    reinterpret_cast<Achievement::GoalInstance*>(self)->Achievement::GoalInstance::~GoalInstance();   // B.3.7 bridge

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}
