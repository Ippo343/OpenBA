// code/Game/Tutorial/tutorialinstance.cpp
// Clean reimplementation of TutorialInstance vtable methods.
//
// TutorialInstance (0x38 bytes) -- tutorial runtime instance.
// Vtable: 0x140160bf8, extends Object::Internal (0x10 bytes base).
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint8_t  _intBase[8]   -- Internal base fields
//   +0x10: int32_t  currentStep   -- current tutorial step index
//   +0x30: void*    tutorialDef   -- ExternalPtr<Tutorial> definition
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialInstance.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B; InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// The ctor inlined in create 0x140028120 and in FinishTutorialInstance's
// Startup (0x1400d7a90): Internal() (the base's real ctor), the stamp (the
// compiler's), then the fields. +0x28 is not written (retail's).
TutorialInstance::TutorialInstance()
    : currentStep(-1), ibpField18(nullptr), ibpField20(nullptr), tutorialDef(nullptr) {
    ::new (static_cast<void*>(&ibpField18)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    ::new (static_cast<void*>(&ibpField20)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
}

// ============================================================
// TutorialInstance::Persist -- Slot3 (vtable +0x18, 0x1400e1520)
// ============================================================
// Serializes the tutorial instance:
//   1. ExternalPtr<Tutorial> at +0x30 via ExternalPtr_SerializeTutorial
//   2. int32_t currentStep at +0x10
//   3. On load: adjusts currentStep backwards if steps are not completed
//      (walks backwards through tutorial steps checking the completed flag)
//
// Disassembly:
//   CALL [0x140152ad8]  ; Serializer::Ctor(stack, stream, 0)
//   CALL 0x1400262b0    ; ExternalPtr<Tutorial>::Serialize(&self+0x30, serializer)
//   CALL [0x140152ac8]  ; Serializer::Serialize(&self+0x10, int)
//   CMP [serializer+8],0  ; check if loading
//   ...step adjustment loop...
//   CALL [0x140152ad0]  ; Serializer::~Serializer
void TutorialInstance::Persist(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // 1. Serialize ExternalPtr<Tutorial> at +0x30
    if (fn_ExternalPtr_SerializeTutorial) {
        fn_ExternalPtr_SerializeTutorial(reinterpret_cast<void*>(s + 0x30), serializer);
    }

    // 2. Serialize int32_t currentStep at +0x10
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(currentStep, nullptr);   // B.3.4 bridge

    // 3. On load: adjust currentStep backwards if steps are not completed
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        void* tutDef = tutorialDef;
        int32_t step = currentStep;

        if (tutDef != nullptr) {
            uintptr_t td = reinterpret_cast<uintptr_t>(tutDef);
            int32_t totalSteps = *reinterpret_cast<int32_t*>(td + 0x30);

            // Only adjust if step is within range and positive
            if (step < totalSteps && step > 0) {
                // tutDef->+0x28 is the steps array; step[i]->+0x20 the completed bool
                while (step > 0) {
                    void** stepsArray = *reinterpret_cast<void***>(td + 0x28);
                    void* currentStepObj = stepsArray[step];
                    uint8_t completed = *reinterpret_cast<uint8_t*>(
                        reinterpret_cast<uintptr_t>(currentStepObj) + 0x20);
                    if (completed != 0) break;
                    step--;
                    currentStep = step;
                }
            }
        }
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Reflection (ENDGAME B.3.8)
// ============================================================
// create 0x140028120: MemAlloc(0x38, 8), then the (inlined) ctor.
void* TutorialInstance::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialInstance), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialInstance());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (null);
// the tree's in-place build is kept (B38_RULES B8-13).
void* TutorialInstance::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialInstance());
}

DEFINE_EXE_TYPE(TutorialInstance, "Object::Internal")
