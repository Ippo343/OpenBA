// code/Game/Goal/tutorialgoalcount.cpp
// Clean reimplementation of TutorialGoalCount vtable methods.
//
// TutorialGoalCount (0x50 bytes) — tutorial goal counting items/buildings/citizens.
// Vtable: 0x140160950
// Extends TutorialGoal -> Object::External
//
// The Check function (Slot8) is a large switch on countMode (+0x20) with 11 cases.
// Each case counts a different game metric and compares against targetCount (+0x24).
// If greaterOrEqual (+0x28), checks count >= target; else checks count == target.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Goal/tutorialgoalcount_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Exe-internal helpers resolved at runtime
// ============================================================

// FUN_1400265b0 — resolve External reference from String (lazy load)
typedef void (*fn_ResolveExternalRef_t)(void* stringField);
static fn_ResolveExternalRef_t s_fn_ResolveExternalRef = nullptr;

// FUN_140092010 — count buildings of a type in the world BST
// RCX=worldData, RDX=buildingTypePtr -> returns uint count
typedef uint32_t (*fn_CountBuildingsByType_t)(uintptr_t worldData, uintptr_t typePtr);
static fn_CountBuildingsByType_t s_fn_CountBuildingsByType = nullptr;

// FUN_1400910a0 — count professions of a type
// RCX=worldData, RDX=professionTypePtr -> returns uint count
typedef uint32_t (*fn_CountProfessionsByType_t)(uintptr_t worldData, uintptr_t typePtr);
static fn_CountProfessionsByType_t s_fn_CountProfessionsByType = nullptr;

// FUN_14012fbb0 — get stored resource count for a building entity
typedef int (*fn_GetStoredCount_t)(uintptr_t entity);
static fn_GetStoredCount_t s_fn_GetStoredCount = nullptr;

// FUN_140015140 — FindBuildingComponent on entity
typedef void* (*fn_FindBuildingComp_t)(uintptr_t entity, void* outIBP);
static fn_FindBuildingComp_t s_fn_FindBuildingComp = nullptr;

// FUN_140006270 — FindStorageComponent on entity
typedef void* (*fn_FindStorageComp_t)(uintptr_t entity, void* outIBP);
static fn_FindStorageComp_t s_fn_FindStorageComp = nullptr;

// FUN_140013920 — get dialog controller from InternalBasePtr
typedef void* (*fn_GetDialogController_t)(void* element);
static fn_GetDialogController_t s_fn_GetDialogController = nullptr;

// TypeInfo_Profession at 0x1401c7318 (TypeId used in IsDerivedFrom checks)
static uint32_t* s_TypeId_Profession = nullptr;
// TypeId for building descriptions (DWORD_1401c7318 in some cases)
static uint32_t* s_TypeId_BuildingDesc = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;
    s_fn_ResolveExternalRef = (fn_ResolveExternalRef_t)ResolveAddr(Module::Exe, 0x1400265b0);
    s_fn_CountBuildingsByType = (fn_CountBuildingsByType_t)ResolveAddr(Module::Exe, 0x140092010);
    s_fn_CountProfessionsByType = (fn_CountProfessionsByType_t)ResolveAddr(Module::Exe, 0x1400910a0);
    s_fn_GetStoredCount = (fn_GetStoredCount_t)ResolveAddr(Module::Exe, 0x14012fbb0);
    s_fn_FindBuildingComp = (fn_FindBuildingComp_t)ResolveAddr(Module::Exe, 0x140015140);
    s_fn_FindStorageComp = (fn_FindStorageComp_t)ResolveAddr(Module::Exe, 0x140006270);
    s_fn_GetDialogController = (fn_GetDialogController_t)ResolveAddr(Module::Exe, 0x140013920);

    // TypeId for Profession/BuildingDescription — both at 0x1401c7318
    // In the decompilation, different cases use different type IDs.
    // Cases 0/7/10 use DWORD_1401c7318 for building type checks.
    // Cases 1/3/6 use TypeInfo_Profession (also 0x1401c7318) for profession checks.
    // These may actually be the same type ID used for different subtype checks.
    s_TypeId_BuildingDesc = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7318);
    s_TypeId_Profession = s_TypeId_BuildingDesc;

    s_helpersResolved = true;
}

// ============================================================
// TutorialGoalCount::Dtor (vtable +0x00, 0x140027820)
// ============================================================
// 1. Set vtable to TutorialGoalCount
// 2. Destroy String at +0x48
// 3. Destroy String at +0x30
// 4. Set vtable to TutorialGoal
// 5. Call External::~External
// 6. Optionally free memory
void* TutorialGoalCount_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalCount
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalCountAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Destroy String at +0x48 (elementName)
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x48));
    }

    // Destroy String at +0x30 (typeName)
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x30));
    }

    // Set vtable to TutorialGoal base
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
// TutorialGoalCount::Serialize (vtable +0x28, 0x1400df900)
// ============================================================
// Serializes all fields. The ExternalPtr at +0x20 is serialized via the
// serializer's vtable (+0x98), which handles ExternalPtr serialization.
// After loading, if version==0 (load mode), resolves the external ref at +0x30.
void TutorialGoalCount_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    EnsureHelpers();

    // Base class serialize (empty)
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

    // Serialize ExternalPtr at +0x20 via serializer vtable +0x98
    // The serializer object has a pointer to its vtable-like structure at offset +0x08
    uintptr_t serObj = *(uintptr_t*)(ser + 0x08);
    if (serObj != 0) {
        uintptr_t serVtable = *(uintptr_t*)serObj;
        typedef void (*SerializeExternalPtr_t)(uintptr_t serObj, void* fieldPtr);
        SerializeExternalPtr_t serExtPtr = (SerializeExternalPtr_t)(*(uintptr_t*)(serVtable + 0x98));
        if (serExtPtr) {
            serExtPtr(serObj, (void*)(s + 0x20));
        }
    }

    // Serialize targetCount (+0x24) as int
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser, (void*)(s + 0x24), nullptr);
    }

    // Serialize typeName (+0x30) as String
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize((void*)(s + 0x30), ser);
    }

    // If loading (serializer version == 0), resolve the external ref
    if (g_fn_Serializer_GetVersion) {
        int version = g_fn_Serializer_GetVersion(ser);
        if (version == 0 && s_fn_ResolveExternalRef) {
            s_fn_ResolveExternalRef((void*)(s + 0x30));
        }
    } else {
        // Check raw field: ser+0x08 -> version at +0x08
        uintptr_t serData = *(uintptr_t*)(ser + 0x00);
        if (serData != 0) {
            int version = *(int*)(serData + 0x08);
            if (version == 0 && s_fn_ResolveExternalRef) {
                s_fn_ResolveExternalRef((void*)(s + 0x30));
            }
        }
    }

    // Serialize greaterOrEqual (+0x28) as bool
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(ser, (void*)(s + 0x28), nullptr);
    }

    // Serialize elementName (+0x48) as String
    if (g_fn_String_Serialize) {
        g_fn_String_Serialize((void*)(s + 0x48), ser);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser);
    }
}

// ============================================================
// TutorialGoalCount::Cleanup (vtable +0x30, 0x140027800)
// ============================================================
// Calls ScalarDtor(self, 0) via vtable, then re-initializes via Init.
void TutorialGoalCount_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize via TutorialGoalCount_Init (the real ctor body at 0x1400dfa60)
    TutorialGoalCount_Init(self);
}

// ============================================================
// TutorialGoalCount::Check (vtable +0x40, 0x1400df290)
// ============================================================
// Large switch on countMode, counts various game metrics.
// Returns 1 if goal met, 0 if not.
//
// Due to the extreme complexity (11 cases, ~1500 bytes of code),
// this is implemented as a call to the original function via
// resolved address. The function is too entangled with internal
// game state (BST traversals, entity iteration, profession lookups)
// to safely reimplement without risk of subtle bugs.
//
// The function will be fully reimplemented once all helper functions
// (FUN_140092010, FUN_1400910a0, etc.) are individually decompiled
// and available as clean code.
//
// For now, we use a resolved function pointer to the original.
int TutorialGoalCount_Check(void* self, void* param2, void* param3) {
    EnsureHelpers();

    // Call original implementation via resolved address
    typedef int (*fn_Check_t)(void*, void*, void*);
    static fn_Check_t s_fn_Check = nullptr;
    if (!s_fn_Check) {
        s_fn_Check = (fn_Check_t)ResolveAddr(Module::Exe, TutorialGoalCountAddr::Check);
    }
    if (s_fn_Check) {
        return s_fn_Check(self, param2, param3);
    }
    return 0;
}

// ============================================================
// TutorialGoalCount::Init (vtable +0x48, 0x1400dfa60)
// ============================================================
// Constructor body. Called from vtable slot 9 (wrapper at 0x1400277f0
// which null-checks self then jumps here).
void* TutorialGoalCount_Init(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialGoal -> TutorialGoalCount
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalCountAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Zero/init all fields
    *(uint64_t*)(s + 0x20) = 0;          // countMode=0, targetCount=0 (packed)
    *(uint8_t*)(s + 0x28) = 0;           // greaterOrEqual = false
    *(uint64_t*)(s + 0x30) = 0;          // typeName = null string
    *(int32_t*)(s + 0x38) = -1;          // sentinel
    *(uint64_t*)(s + 0x40) = 0;          // ExternalPtr = null
    *(uint64_t*)(s + 0x48) = 0;          // elementName = null string

    return self;
}
