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
// Slot4/Slot5 use function pointers from engine_funcs.h (resolved at startup).

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
// FinishTutorialInstance::Slot4 (vtable +0x20, 0x1400d7a90)
// ============================================================
// Startup/Load: allocates a TutorialInstance (0x38-byte Internal object),
// initializes its fields, stores it in self+0x20, copies the tutorial
// definition reference from the parent FinishTutorialGoal, then calls
// the tutorial tree insert helper to register the tutorial dialog,
// and sets game world flags to enable tutorial UI.
//
// Decompiled from disassembly:
//   1. Allocate 0x38 bytes, call Object::Internal::Internal()
//   2. Set vtable to TutorialInstance vtable (0x140160bf8)
//   3. Set field+0x10 = -1, IBP::Ctor(+0x18, NULL), IBP::Ctor(+0x20, NULL), +0x30 = 0
//   4. IBP::Ctor(&local, newObj), store in self+0x20
//   5. IsDerivedFrom(self+0x18, FinishTutorialGoal) -> get goal pointer
//   6. CheckPtr(self+0x20), copy goal->+0x30 to tutInst->+0x30, set tutInst->+0x10 = 0
//   7. Call FinishTutorialGoal_helper_01f0 (tree insert) with:
//      - gameWorld+0x120 (tutorial tree root)
//      - tutDef->+0x20 (tutorial external ID)
//      - param3 (forwarded)
//      - *(int*)(*(void**)singleton_exref + 4) - 0x1d0
//   8. OR gameWorld+0xd0 flags with 0x5f0
void FinishTutorialInstance_Slot4(FinishTutorialInstance* self, void* p2, void* p3) {
    uintptr_t s = (uintptr_t)self;

    // Step 1-3: Allocate and initialize TutorialInstance (0x38 bytes)
    void* newObj = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        newObj = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    }

    if (newObj != nullptr) {
        // Call Object::Internal::Internal constructor
        if (fn_ObjectInternal_Ctor) {
            fn_ObjectInternal_Ctor(newObj);
        }

        uintptr_t obj = (uintptr_t)newObj;

        // Set vtable to TutorialInstance vtable (0x140160bf8)
        if (g_TutorialInstanceVtable) {
            *(void**)obj = g_TutorialInstanceVtable;
        }

        // Set field+0x10 = -1 (0xFFFFFFFF)
        *(int32_t*)(obj + 0x10) = -1;

        // InternalBasePtr::Ctor on fields +0x18 and +0x20 with NULL
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor((void*)(obj + 0x18), nullptr);
            fn_InternalBasePtr_Ctor((void*)(obj + 0x20), nullptr);
        }

        // Set field+0x30 = 0
        *(uint64_t*)(obj + 0x30) = 0;
    } else {
        newObj = nullptr;  // allocation failed
    }

    // Step 4: Create IBP from newObj, store in self+0x20
    // Use a local IBP buffer (8 bytes), then copy to self+0x20
    uint64_t localIBP = 0;
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(&localIBP, newObj);
    }
    *(uint64_t*)(s + 0x20) = localIBP;

    // Step 5: IsDerivedFrom check on self+0x18 (goal pointer) against FinishTutorialGoal
    void* goalObj = *(void**)(s + 0x18);
    void* ftGoal = nullptr;
    if (fn_IsDerivedFrom && g_TypeId_FinishTutorialGoal && goalObj) {
        bool isDerived = fn_IsDerivedFrom(goalObj, *g_TypeId_FinishTutorialGoal);
        if (isDerived) {
            ftGoal = goalObj;
        }
    }

    // Step 6: CheckPtr on self+0x20, then copy goal fields to tutInst
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    }

    uintptr_t tutInst = *(uintptr_t*)(s + 0x20);
    if (tutInst && ftGoal) {
        // Copy tutorial definition reference: tutInst->+0x30 = goal->+0x30
        *(uint64_t*)(tutInst + 0x30) = *(uint64_t*)((uintptr_t)ftGoal + 0x30);
        // Reset current step to 0
        *(int32_t*)(tutInst + 0x10) = 0;
    }

    // Step 7: Call FinishTutorialGoal_helper_01f0 (tutorial tree insert)
    if (fn_TutorialTreeInsert && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        void* treeRoot = *(void**)(gameWorld + 0x120);

        // Read tutorial external ID from tutDef->+0x20
        uintptr_t tutDef = *(uintptr_t*)(tutInst + 0x30);
        uint64_t tutorialId = 0;
        if (tutDef) {
            tutorialId = *(uint64_t*)(tutDef + 0x20);
        }

        // Read screen height param from singleton exref
        int heightParam = 0;
        if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
            uintptr_t singletonObj = *(uintptr_t*)*g_pSingletonExref_151a80;
            if (singletonObj) {
                heightParam = *(int32_t*)(singletonObj + 4) - 0x1d0;
            }
        }

        fn_TutorialTreeInsert(treeRoot, tutorialId, p3, heightParam);
    }

    // Step 8: OR game world flags at +0xd0 with 0x5f0
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
        *(uint32_t*)(gameWorld + 0xd0) |= 0x5f0;
    }
}

// ============================================================
// FinishTutorialInstance::Slot5 (vtable +0x28, 0x1400d7a50)
// ============================================================
// Execute/Update: checks the InternalBasePtr at +0x20 (TutorialInstance),
// then delegates to the tutorial step execution helper.
//
// Decompiled from disassembly:
//   1. CheckPtr(&self->+0x20)
//   2. Tail-call FinishTutorialInstance_helper_1db0(*(self+0x20), p2, p3, p4)
//
// Note: R8 (p3) and R9 (p4) are implicitly forwarded through the tail JMP.
void FinishTutorialInstance_Slot5(FinishTutorialInstance* self, void* p2, void* p3, void* p4) {
    uintptr_t s = (uintptr_t)self;

    // CheckPtr on IBP at +0x20
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    }

    // Read the TutorialInstance pointer
    void* tutInst = *(void**)(s + 0x20);

    // Delegate to the tutorial step execution helper
    if (fn_TutorialStepExecute && tutInst) {
        fn_TutorialStepExecute(tutInst, p2, p3, (uint64_t)(uintptr_t)p4);
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
