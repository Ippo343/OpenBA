// code/Game/Goal/tutorialgoalbutton.cpp
// Clean reimplementation of TutorialGoalButton vtable methods.
//
// TutorialGoalButton (0x30 bytes) — tutorial goal that waits for a UI element state.
// Vtable: 0x140160800
// Extends TutorialGoal -> Object::External
//
// Fields (from Ghidra decompilation):
//   +0x20: System::String elementPath — dotted path to UI element (e.g. "DialogType.ElementName")
//   +0x28: bool invertCheck — selects which bit of element flags to check
//
// Check logic (Slot8):
//   Finds UI element via FUN_1400e1870 path lookup.
//   If found, wraps in InternalBasePtr, checks IsDerivedFrom(Object::Internal).
//   Reads flags at element+0x34:
//     invertCheck=true:  returns ~(flags >> 23) & 1  (bit 23 NOT set = success)
//     invertCheck=false: returns (flags >> 22) & 1   (bit 22 set = success)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/tutorialgoalbutton_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// TutorialGoalButton::Dtor (vtable +0x00, 0x1400273b0)
// ============================================================
// 1. Set vtable to TutorialGoalButton
// 2. Destroy String at +0x20
// 3. Set vtable to TutorialGoal (base class)
// 4. Call External::~External
// 5. Optionally free memory
void* TutorialGoalButton_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalButton
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialGoalButtonAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Destroy String at +0x20
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x20));
    }

    // Set vtable to TutorialGoal (base)
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialGoalButton::Serialize (vtable +0x28, 0x1400deda0)
// ============================================================
// 1. Call base TutorialGoal::Serialize (creates/destroys empty serializer)
// 2. Create a new Serializer from stream
// 3. Serialize String at +0x20 (elementPath)
// 4. Serialize bool at +0x28 (invertCheck)
// 5. Destroy serializer
void TutorialGoalButton_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Base class serialize (empty — just creates/destroys serializer)
    uint8_t ser0[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser0, stream, 0);
    }
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser0);
    }

    // Create serializer for our fields
    uint8_t ser[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser, stream, 0);
    }

    // Serialize elementPath (+0x20)
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize((void*)(s + 0x20), ser);
    }

    // Serialize invertCheck (+0x28) — bool
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(ser, (void*)(s + 0x28), nullptr);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser);
    }
}

// ============================================================
// TutorialGoalButton::Cleanup (vtable +0x30, 0x140027350)
// ============================================================
// Calls ScalarDtor(self, 0) via vtable slot 0, then re-initializes.
// The dtor destroys the string, the re-init zeroes fields.
// Equivalent to: self->vtable[0](self, 0); External::External(self);
//                set vtable chain; zero fields.
void TutorialGoalButton_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable — this destroys the object
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize via External::External
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalButtonAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Zero fields
    *(uint64_t*)(s + 0x20) = 0;  // elementPath = empty string
    *(uint8_t*)(s + 0x28) = 0;   // invertCheck = false
}

// ============================================================
// TutorialGoalButton::Check (vtable +0x40, 0x1400dece0)
// ============================================================
// Finds UI element by path string, checks element visibility flags.
int TutorialGoalButton_Check(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Find UI element by elementPath (+0x20)
    uintptr_t elementIBP = 0;
    if (fn_FindUIElementByPath) {
        fn_FindUIElementByPath(&elementIBP, (void*)(s + 0x20), nullptr);
    }

    if (elementIBP == 0) {
        return 0;
    }

    // CheckPtr on the found element
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr(&elementIBP);
    }

    // Check if element IsDerivedFrom Object::Internal
    uintptr_t element = elementIBP;
    if (!fn_IsDerivedFrom || !g_TypeId_ObjectInternal) {
        return 0;
    }

    bool isDerived = fn_IsDerivedFrom((void*)element, *g_TypeId_ObjectInternal);
    void* validElement = isDerived ? (void*)element : nullptr;

    // Wrap in another InternalBasePtr
    uintptr_t wrappedIBP = 0;
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(&wrappedIBP, validElement);
    }

    if (wrappedIBP == 0) {
        return 0;
    }

    // Read flags from element+0x34
    bool invertCheck = *(uint8_t*)(s + 0x28) != 0;
    if (fn_InternalBasePtr_CheckPtr) {
        fn_InternalBasePtr_CheckPtr(&wrappedIBP);
    }

    uintptr_t elem = wrappedIBP;
    uint32_t flags = *(uint32_t*)(elem + 0x34);

    if (invertCheck) {
        // Bit 23 NOT set = success
        return (~(flags >> 23)) & 1;
    } else {
        // Bit 22 set = success
        return (flags >> 22) & 1;
    }
}

// ============================================================
// TutorialGoalButton::Init (vtable +0x48, 0x140027300)
// ============================================================
// Constructor/init called by factory after allocation.
void* TutorialGoalButton_Init(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialGoal -> TutorialGoalButton
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalButtonAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Zero fields
    *(uint64_t*)(s + 0x20) = 0;  // elementPath = null string
    *(uint8_t*)(s + 0x28) = 0;   // invertCheck = false

    return self;
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160800 (9 slots, Object::External base), size 0x30, parent "TutorialGoal".
// GetFileVersion (+0x18) = 2. MusicList pattern (musiclist.cpp).
// ============================================================
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialGoalButton_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialGoalButton");
    return c;
}
static int TutorialGoalButton_GetFileVersion(void*) { return 2; }

static void* g_TutorialGoalButton_Vtable[9] = {
    (void*)&TutorialGoalButton_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialGoalButton_GetTypeId,       // +0x10 GetTypeId
    (void*)&TutorialGoalButton_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialGoalButton_Serialize,       // +0x28 Serialize
    (void*)&TutorialGoalButton_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialGoalButton_Init,              // +0x40 Ctor/CreateInPlace
};

extern "C" void RegisterTutorialGoalButtonVtable() {
    RegisterExeAddr(0x140160800, (uintptr_t)g_TutorialGoalButton_Vtable);
}
extern "C" void* TutorialGoalButton_CreateInPlace(void* mem) {
    return TutorialGoalButton_Init(mem);
}
extern "C" void* TutorialGoalButton_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    return p ? TutorialGoalButton_Init(p) : nullptr;
}
REGISTER_TYPE(TutorialGoalButton, "TutorialGoal", &TutorialGoalButton_Factory, &TutorialGoalButton_CreateInPlace);
#endif // STANDALONE_EXE
