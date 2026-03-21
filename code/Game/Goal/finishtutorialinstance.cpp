// code/Game/Goal/finishtutorialinstance.cpp
// Clean reimplementation of FinishTutorialInstance vtable methods.
//
// FinishTutorialInstance (0x28 bytes) — goal instance for tutorial completion.
// Vtable: 0x140160478
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint32_t field08        — GoalInstance base (Internal base)
//   +0x0C: uint32_t field0C        — GoalInstance base
//   +0x10: int32_t  currentStep    — current tutorial step index
//   +0x18: void*    goalPtr        — InternalBasePtr to parent Goal
//   +0x20: void*    tutorialInst   — InternalBasePtr to TutorialInstance object
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Goal/finishtutorialinstance_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// No class-level resolved state needed — all resolved lazily in individual functions.

// ============================================================
// FinishTutorialInstance::Slot3 (vtable +0x18, 0x1400d7bf0)
// ============================================================
// Serialize/Persist: calls GoalInstance::Persist(base), creates Serializer,
// serializes InternalBasePtr at +0x20, then type-checks against TutorialInstance.
//
// Decompiled:
//   GoalInstance::Persist(this, stream);
//   Serializer local_20;
//   IO::Serializer::Serializer(local_20, stream, 0);
//   Object::InternalBasePtr::Serialize(&this->+0x20, local_20);
//   if (*(void**)(this+0x20) != NULL) {
//       if (!IsDerivedFrom(*(void**)(this+0x20), TypeInfo_TutorialInstance)) {
//           *(uint64_t*)(this+0x20) = 0;
//       }
//   }
//   IO::Serializer::~Serializer(local_20);
void FinishTutorialInstance_Slot3(FinishTutorialInstance* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Call GoalInstance base persist
    if (fn_GoalInstance_Persist) {
        fn_GoalInstance_Persist(self, stream);
    }

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize InternalBasePtr at +0x20
    if (fn_InternalBasePtr_Serialize) {
        fn_InternalBasePtr_Serialize((void*)(s + 0x20), serializer);
    }

    // Type-check: if deserialized object is not a TutorialInstance, null it out
    void* obj = *(void**)(s + 0x20);
    if (obj != nullptr) {
        // IsDerivedFrom check — use fn_IsDerivedFrom dispatch slot
        // The original uses TypeInfo_TutorialInstance for the check.
        // For safety, we read the fn_IsDerivedFrom from the dispatch slot.
        typedef bool (*fn_IsDerivedFrom_t)(void* obj, void* typeInfo);
        static fn_IsDerivedFrom_t s_fn_IsDerivedFrom = nullptr;
        static void* s_typeInfo = nullptr;
        if (!s_fn_IsDerivedFrom) {
            // fn_IsDerivedFrom is at a dispatch slot — resolve from exe .rdata
            void** slot = (void**)ResolveAddr(Module::Exe, 0x140151c08);
            if (slot && *slot) {
                s_fn_IsDerivedFrom = (fn_IsDerivedFrom_t)*slot;
            }
            // TypeInfo_TutorialInstance referenced in the original code
            // The decompilation references "TypeInfo_TutorialInstance" which is
            // a .rdata pointer. Need to find the actual address.
            void** tiSlot = (void**)ResolveAddr(Module::Exe, 0x1401c67e0);
            if (tiSlot) {
                s_typeInfo = tiSlot;
            }
        }
        if (s_fn_IsDerivedFrom && s_typeInfo) {
            bool isDerived = s_fn_IsDerivedFrom(obj, s_typeInfo);
            if (!isDerived) {
                *(uint64_t*)(s + 0x20) = 0;
            }
        }
    }

    // Destroy Serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// FinishTutorialInstance::Slot6 (vtable +0x30, 0x1400d7bb0)
// ============================================================
// Check/Shutdown: checks whether the tutorial is complete.
// Reads the TutorialInstance via IBP at +0x20, compares:
//   tutorial->+0x30 (External* to Tutorial definition)
//   tutorialDef->+0x30 (total step count)
//   vs tutorialInst->+0x10 (current step)
//
// Returns: currentStep >= totalSteps
//
// Decompiled:
//   fn_InternalBasePtr_CheckPtr(&this->+0x20);
//   lVar1 = *(this+0x20);  // TutorialInstance*
//   return *(int*)(*(int64_t*)(lVar1+0x30) + 0x30) <= *(int*)(lVar1+0x10);
int FinishTutorialInstance_Slot6(FinishTutorialInstance* self) {
    uintptr_t s = (uintptr_t)self;

    // CheckPtr on IBP at +0x20
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    }

    // Read the TutorialInstance pointer
    uintptr_t tutInst = *(uintptr_t*)(s + 0x20);
    if (tutInst == 0) return 0;

    // Read tutorial definition pointer at tutInst+0x30
    uintptr_t tutDef = *(uintptr_t*)(tutInst + 0x30);
    if (tutDef == 0) return 0;

    // Compare: totalSteps (tutDef+0x30) <= currentStep (tutInst+0x10)
    int totalSteps = *(int*)(tutDef + 0x30);
    int currentStep = *(int*)(tutInst + 0x10);

    return (totalSteps <= currentStep) ? 1 : 0;
}
