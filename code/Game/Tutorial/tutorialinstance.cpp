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

#include "Tutorial/tutorialinstance_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Local function pointers (resolved lazily)
// ============================================================

// Internal::~Internal -- dispatch slot 0x140152770
typedef void (*fn_Internal_Dtor_dispatch_t)(void* self);
static fn_Internal_Dtor_dispatch_t s_fn_Internal_Dtor = nullptr;

// Serializer::Ctor -- dispatch slot 0x140152ad8
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// Serializer::Dtor -- dispatch slot 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Serializer::Serialize(int*) -- dispatch slot 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* serializer, void* intPtr, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    void** slot;

    // Internal::~Internal
    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152770));
    if (slot && *slot) s_fn_Internal_Dtor = reinterpret_cast<fn_Internal_Dtor_dispatch_t>(*slot);

    // Serializer slots
    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad8));
    if (slot && *slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad0));
    if (slot && *slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ac8));
    if (slot && *slot) s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(*slot);
}

// ============================================================
// TutorialInstance_Dtor -- Slot0 (vtable +0x00, 0x1400281a0)
// ============================================================
// Scalar destructor: sets vtable, calls Internal::~Internal,
// optionally frees memory.
//
// Disassembly:
//   LEA RAX,[0x140160bf8]   ; vtable
//   MOV [RCX],RAX           ; set vtable
//   CALL [0x140152770]      ; Internal::~Internal
//   TEST BL,0x1             ; check flags
//   JZ skip
//   CALL [0x140152b68]      ; MemFreeStatic
void* TutorialInstance_Dtor(void* self, uint32_t flags) {
    EnsureResolved();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Set vtable to TutorialInstance
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialInstanceAddr::Vtable);
    *reinterpret_cast<uintptr_t*>(s) = vtable;

    // Call Internal::~Internal
    if (s_fn_Internal_Dtor) {
        s_fn_Internal_Dtor(self);
    }

    // Conditionally free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// TutorialInstance_Serialize -- Slot3 (vtable +0x18, 0x1400e1520)
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
void TutorialInstance_Serialize(void* self, void* stream) {
    EnsureResolved();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    } else {
        return;
    }

    // 1. Serialize ExternalPtr<Tutorial> at +0x30
    if (fn_ExternalPtr_SerializeTutorial) {
        fn_ExternalPtr_SerializeTutorial(reinterpret_cast<void*>(s + 0x30), serializer);
    }

    // 2. Serialize int32_t currentStep at +0x10
    if (s_fn_Serializer_SerializeInt) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x10), nullptr);   // B.3.4 bridge
    }

    // 3. On load: adjust currentStep backwards if steps are not completed
    //    The serializer's encoding mode at *(serializer[0]+8) == 0 means "reading"
    uintptr_t serPtr = *reinterpret_cast<uintptr_t*>(serializer);
    if (*reinterpret_cast<int32_t*>(serPtr + 8) == 0) {
        // Loading from save -- check if current step needs adjustment
        void* tutDef = *reinterpret_cast<void**>(s + 0x30);
        int32_t step = *reinterpret_cast<int32_t*>(s + 0x10);

        if (tutDef != nullptr) {
            uintptr_t td = reinterpret_cast<uintptr_t>(tutDef);
            int32_t totalSteps = *reinterpret_cast<int32_t*>(td + 0x30);

            // Only adjust if step is within range and positive
            if (step < totalSteps && step > 0) {
                // Walk backwards: if the current step is not completed, go back
                // tutDef->+0x28 is the steps array, each element is a pointer
                // step[i]->+0x20 is the completed bool
                while (step > 0) {
                    void** stepsArray = *reinterpret_cast<void***>(td + 0x28);
                    void* currentStepObj = stepsArray[step];
                    uint8_t completed = *reinterpret_cast<uint8_t*>(
                        reinterpret_cast<uintptr_t>(currentStepObj) + 0x20);
                    if (completed != 0) break;
                    step--;
                    *reinterpret_cast<int32_t*>(s + 0x10) = step;
                }
            }
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// Standalone factory (factory cascade — concrete landmine removal)
// ============================================================
// Exe factory TutorialInstance_Factory (0x140028120): MemAlloc(0x38,8) ->
//   Object::Internal::Internal(p)
//   *(p)        = &vtable_TutorialInstance (0x140160bf8)
//   *(int p+0x10) = -1
//   InternalBasePtr::Ctor(p+0x18, null)
//   InternalBasePtr::Ctor(p+0x20, null)
//   *(p+0x30)   = 0
// Registered as the real factory so CreateObject("TutorialInstance") /
// External::Load no longer hits the NullFactory -> IsDerivedFrom(NULL) crash.
// DORMANT-SAFE: installs the exe TutorialInstance vtable only if it resolves in
// standalone (ResolveAddr != 0); otherwise keeps the Object::Internal base vtable
// that fn_ObjectInternal_Ctor installs. Nothing currently drives a TutorialInstance.
static void* TutorialInstance_Build(void* p) {
    if (!p) return nullptr;
    uint8_t* s = reinterpret_cast<uint8_t*>(p);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(p);
    uintptr_t vt = ResolveAddr(Module::Exe, TutorialInstanceAddr::Vtable);
    if (vt) *reinterpret_cast<uintptr_t*>(s) = vt;
    *reinterpret_cast<int32_t*>(s + 0x10) = -1;
    if (fn_InternalBasePtr_Ctor) {
        new (s + 0x18) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        new (s + 0x20) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    }
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    return p;
}
extern "C" void* TutorialInstance_CreateInPlace_SA(void* mem) {
    return TutorialInstance_Build(mem);
}
extern "C" void* TutorialInstance_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    return TutorialInstance_Build(p);
}
REGISTER_TYPE(TutorialInstance, "Object::Internal",
              &TutorialInstance_Factory_SA, &TutorialInstance_CreateInPlace_SA);
