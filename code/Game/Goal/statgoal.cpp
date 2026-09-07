// code/Game/Goal/statgoal.cpp
// Clean reimplementation of StatGoal vtable methods.
//
// StatGoal (0x80 bytes) — achievement goal requiring stat comparison.
// Vtable: 0x1401604c0
//
// Field layout (from StatGoal_Ctor at 0x1400daa80 and Slot5 at 0x1400da8b0):
//   +0x30: int32  statTypeA           — stat type enum for first operand (default 5)
//   +0x34: int32  statTypeB           — stat type enum for second operand (default 5)
//   +0x38: int32  comparisonType      — comparison enum (0-11: EQ/NE/LT/LE/GT/GE, even=A, odd=B)
//   +0x3c: int32  fixedValue          — fixed comparison value when not using statB (default 1)
//   +0x40: bool   requireOtherGoalsMet — adds 1 to result if true
//   +0x48: String statAString         — resource name for stat A
//   +0x50: int32  statAGeneration     — ExternalFactory gen counter
//   +0x58: void*  statAExternal       — resolved External* for stat A
//   +0x60: String statBString         — resource name for stat B
//   +0x68: int32  statBGeneration     — ExternalFactory gen counter
//   +0x70: void*  statBExternal       — resolved External* for stat B
//   +0x78: int32  statFlags           — flags/category (default 0x20)
//   +0x7c: int32  statSubType         — sub-type index (default 1)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Goal/statgoal_funcs.h"
#include "engine_funcs.h"
#include "Goal/goal_exe_helpers.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Resolved helpers
// ============================================================

// Achievement::Goal::Serialize — dispatch slot at 0x140152840
typedef void (*fn_GoalSerialize_t)(void* self, void* stream);
static fn_GoalSerialize_t s_fn_GoalSerialize = nullptr;

// Achievement::Goal::Goal — constructor
typedef void* (*fn_GoalCtor_t)(void* self);

// Achievement::Goal::~Goal — destructor
typedef void (*fn_GoalDtor_t)(void* self);

// FUN_1400265b0 — resolve ExternalPtr (refresh External* from string name)
typedef void (*fn_ResolveExternalPtr_t)(void* stringField);
static fn_ResolveExternalPtr_t s_fn_ResolveExternalPtr = nullptr;

// FUN_1400da4e0 — stat value dispatch (giant switch on statTypeId)
typedef int (*fn_StatValueDispatch_t)(void* self, int statTypeId, void* externalPtr);
static fn_StatValueDispatch_t s_fn_StatValueDispatch = nullptr;

// System::MemoryAllocator::MemFree (static wrapper)
// Already declared in engine_funcs.h as fn_MemFreeStatic

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    // Goal::Serialize is a dispatch slot — read the function pointer at runtime
    uintptr_t slotAddr = ResolveAddr(Module::Exe, 0x140152840);
    if (slotAddr) s_fn_GoalSerialize = *reinterpret_cast<fn_GoalSerialize_t*>(slotAddr);

    s_fn_ResolveExternalPtr = (fn_ResolveExternalPtr_t)ResolveAddr(Module::Exe, 0x1400265b0);
    s_fn_StatValueDispatch = (fn_StatValueDispatch_t)ResolveAddr(Module::Exe, 0x1400da4e0);

    s_helpersResolved = true;
}

// ============================================================
// StatGoal::Slot5/Load (vtable +0x28, 0x1400da8b0)
// ============================================================
// Serializes all stat goal fields to/from a stream.
// Calls base Goal::Serialize first, then serializes stat-specific fields.
// The serializer vtable +0x98 dispatch is used for enum-typed int fields.

void StatGoal_Slot5(StatGoal* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Call base class serialize
    if (s_fn_GoalSerialize) {
        s_fn_GoalSerialize(self, stream);
    }

    // Create serializer (stack-local, 24 bytes: [0]=?, [8]=serializerPtr)
    uint8_t serializerBuf[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializerBuf, stream, 0);
    }

    // Get the serializer's vtable dispatch for enum serialization
    // The serializer object is at serializerBuf+8 (pointer to actual serializer)
    void* serializerObj = *(void**)(serializerBuf + 8);

    if (serializerObj) {
        // Serialize statTypeA, statTypeB, comparisonType via vtable +0x98
        // Signature: void serialize_enum(this, void* field, const char* name)
        typedef void (*fn_SerializeEnum_t)(void* self, void* field, const char* name);
        void* vtable = *(void**)serializerObj;
        fn_SerializeEnum_t serializeEnum = *(fn_SerializeEnum_t*)((uintptr_t)vtable + 0x98);

        if (serializeEnum) {
            serializeEnum(serializerObj, (void*)(s + 0x30), nullptr);  // statTypeA
            serializeEnum(serializerObj, (void*)(s + 0x34), nullptr);  // statTypeB
            serializeEnum(serializerObj, (void*)(s + 0x38), nullptr);  // comparisonType
        }
    }

    // Serialize fixedValue (+0x3c) via Serialize_Int
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializerBuf, (void*)(s + 0x3c), nullptr);
    }

    // Serialize requireOtherGoalsMet (+0x40) via Serialize_Bool
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(serializerBuf, (void*)(s + 0x40), nullptr);
    }

    // Serialize statA string (+0x48) via String::Serialize
    // First construct a Filename (stack temp)
    typedef void (*fn_FilenameCtor_t)(void* self);
    typedef void (*fn_FilenameDtor_t)(void* self);
    fn_FilenameCtor_t filenameCtor = nullptr;
    fn_FilenameDtor_t filenameDtor = nullptr;

    uintptr_t fnCtorSlot = ResolveAddr(Module::Exe, 0x140151d40);
    if (fnCtorSlot) filenameCtor = *reinterpret_cast<fn_FilenameCtor_t*>(fnCtorSlot);
    uintptr_t fnDtorSlot = ResolveAddr(Module::Exe, 0x1401529e8);
    if (fnDtorSlot) filenameDtor = *reinterpret_cast<fn_FilenameDtor_t*>(fnDtorSlot);

    if (g_fn_String_Serialize) {
        uint8_t filenameBuf[8];
        if (filenameCtor) filenameCtor(filenameBuf);
        g_fn_String_Serialize((void*)(s + 0x48), serializerBuf);

        // If loading (mode == 0), resolve the ExternalPtr.
        // Original (0x1400da960): RAX = *(serializer+0) [the reader/writer], then
        // check *(int*)(RAX+8) == 0. serializerBuf+0 is the reader, NOT serializerBuf+8
        // (that is the field-dispatch object). Reading the wrong field mis-gates the
        // External resolve.
        void* reader = *(void**)(serializerBuf + 0);
        if (reader && *(int*)((uintptr_t)reader + 0x8) == 0) {
            if (s_fn_ResolveExternalPtr) {
                s_fn_ResolveExternalPtr((void*)(s + 0x48));
            }
        }
        if (filenameDtor) filenameDtor(filenameBuf);

        // Serialize statB string (+0x60)
        if (filenameCtor) filenameCtor(filenameBuf);
        g_fn_String_Serialize((void*)(s + 0x60), serializerBuf);

        reader = *(void**)(serializerBuf + 0);
        if (reader && *(int*)((uintptr_t)reader + 0x8) == 0) {
            if (s_fn_ResolveExternalPtr) {
                s_fn_ResolveExternalPtr((void*)(s + 0x60));
            }
        }
        if (filenameDtor) filenameDtor(filenameBuf);
    }

    // Serialize statFlags (+0x78) via vtable +0x98
    if (serializerObj) {
        typedef void (*fn_SerializeEnum_t)(void* self, void* field, const char* name);
        void* vtable = *(void**)serializerObj;
        fn_SerializeEnum_t serializeEnum = *(fn_SerializeEnum_t*)((uintptr_t)vtable + 0x98);
        if (serializeEnum) {
            serializeEnum(serializerObj, (void*)(s + 0x78), nullptr);
        }
    }

    // Serialize statSubType (+0x7c) via Serialize_Int
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializerBuf, (void*)(s + 0x7c), nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// StatGoal::Slot6/Cleanup (vtable +0x30, 0x140026520)
// ============================================================
// Resets the goal to its default constructed state.
// Calls the scalar dtor with flags=0 (cleanup without dealloc),
// then re-constructs in place.

void StatGoal_Slot6(StatGoal* self) {
    // Call scalar dtor via vtable +0x00 with flags=0
    typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
    void** vtable = *(void***)self;
    fn_ScalarDtor_t scalarDtor = (fn_ScalarDtor_t)vtable[0];
    if (scalarDtor) {
        scalarDtor(self, 0);
    }

    // Re-construct in place
    StatGoal_Ctor_Impl(self);
}

// ============================================================
// StatGoal::Slot8/Check (vtable +0x40, 0x1400da720)
// ============================================================
// Checks whether the stat comparison goal is met.
//
// 1. Copies statA string, resolves ExternalPtr, looks up stat value via dispatch
// 2. If comparisonType is odd, copies statB string, resolves, looks up second value
//    Otherwise uses fixedValue (+0x3c) as the second operand
// 3. Switch on comparisonType (even/odd pairs share comparison):
//    0,1 = EQ; 2,3 = NE; 4,5 = LT; 6,7 = LE; 8,9 = GT; 10,11 = GE
// 4. If requireOtherGoalsMet, result is: comparisonMet ? 2 : 1
//    Otherwise: comparisonMet ? 1 : 0

// Operand computation, split out of Slot8 unchanged so a diagnostic can read the
// two numbers the comparison is about to use. Retail has no such split; the
// instruction order inside is untouched.
static void StatGoal_ComputeOperands(StatGoal* self, int& statValueA, int& comparisonValue) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Resolve dispatch slots for String copy ctor and dtor
    typedef void* (*fn_StringCopyCtor_t)(void* dst, void* src);
    typedef void (*fn_StringDtor_dispatch_t)(void* str);
    static fn_StringCopyCtor_t s_fn_StringCopyCtor = nullptr;
    static fn_StringDtor_dispatch_t s_fn_StringDtor_dispatch = nullptr;
    static bool s_copyResolved = false;
    if (!s_copyResolved) {
        uintptr_t slot;
        slot = ResolveAddr(Module::Exe, 0x140151428);
        if (slot) s_fn_StringCopyCtor = *reinterpret_cast<fn_StringCopyCtor_t*>(slot);
        slot = ResolveAddr(Module::Exe, 0x140152af0);
        if (slot) s_fn_StringDtor_dispatch = *reinterpret_cast<fn_StringDtor_dispatch_t*>(slot);
        // SA fallback: these two dispatch slots (0x140151428 copy ctor / 0x140152af0 dtor)
        // are unregistered in the standalone exe → null. Unlike the tools' cosmetic cost
        // string, here the copy feeds the local ExternalPtr that ResolveExternalPtr uses to
        // look up the stat value, so a null ctor would silently break the comparison. Fall
        // back to the export-resolved System::String ctor/dtor (same pattern as dragtool).
        if (!s_fn_StringCopyCtor && fn_String_Ctor_Copy)
            s_fn_StringCopyCtor = reinterpret_cast<fn_StringCopyCtor_t>(fn_String_Ctor_Copy);
        if (!s_fn_StringDtor_dispatch && fn_String_Dtor)
            s_fn_StringDtor_dispatch = reinterpret_cast<fn_StringDtor_dispatch_t>(fn_String_Dtor);
        s_copyResolved = true;
    }

    // Local ExternalPtr struct layout matching original:
    //   [0x00..0x07]: String (8 bytes, copy)
    //   [0x08..0x0B]: int32 generation
    //   [0x0C..0x0F]: padding
    //   [0x10..0x17]: void* external ptr
    //   ...
    //   [0x28..0x2F]: parent ptr (points back to start)

    // --- Resolve statA ---
    // Copy string from self+0x48 into local struct
    uint8_t localA[0x30];
    memset(localA, 0, sizeof(localA));
    *(uintptr_t*)(localA + 0x28) = (uintptr_t)localA;  // parent ptr

    if (s_fn_StringCopyCtor) {
        s_fn_StringCopyCtor(localA, (void*)(s + 0x48));
    }
    *(int32_t*)(localA + 0x08) = *(int32_t*)(s + 0x50);   // generation
    *(void**)(localA + 0x10)   = *(void**)(s + 0x58);      // external ptr
    *(uintptr_t*)(localA + 0x28) = (uintptr_t)localA;      // parent ptr

    // Resolve ExternalPtr
    if (s_fn_ResolveExternalPtr) {
        s_fn_ResolveExternalPtr(localA);
    }
    void* extA = *(void**)(localA + 0x10);

    // Destroy string copy
    if (s_fn_StringDtor_dispatch) {
        s_fn_StringDtor_dispatch(localA);
    }

    // Look up stat value A
    statValueA = 0;
    if (s_fn_StatValueDispatch) {
        statValueA = s_fn_StatValueDispatch(self, self->statTypeA, extA);
    }

    // --- Determine comparison value ---
    comparisonValue = 0;
    if ((self->comparisonType & 1) != 0) {
        // Odd comparisonType: use statB
        uint8_t localB[0x30];
        memset(localB, 0, sizeof(localB));
        *(uintptr_t*)(localB + 0x28) = (uintptr_t)localB;

        if (s_fn_StringCopyCtor) {
            s_fn_StringCopyCtor(localB, (void*)(s + 0x60));
        }
        *(int32_t*)(localB + 0x08) = *(int32_t*)(s + 0x68);
        *(void**)(localB + 0x10)   = *(void**)(s + 0x70);
        *(uintptr_t*)(localB + 0x28) = (uintptr_t)localB;

        if (s_fn_ResolveExternalPtr) {
            s_fn_ResolveExternalPtr(localB);
        }
        void* extB = *(void**)(localB + 0x10);

        if (s_fn_StringDtor_dispatch) {
            s_fn_StringDtor_dispatch(localB);
        }

        if (s_fn_StatValueDispatch) {
            comparisonValue = s_fn_StatValueDispatch(self, self->statTypeB, extB);
        }
    } else {
        // Even comparisonType: use fixedValue
        comparisonValue = self->fixedValue;
    }

}

int StatGoal_Slot8(StatGoal* self) {
    int statValueA = 0;
    int comparisonValue = 0;
    StatGoal_ComputeOperands(self, statValueA, comparisonValue);

    // --- Evaluate comparison ---
    bool comparisonMet = false;
    int cmpType = self->comparisonType;
    if (cmpType >= 0 && cmpType <= 11) {
        switch (cmpType) {
            case 0: case 1:   comparisonMet = (statValueA == comparisonValue); break;
            case 2: case 3:   comparisonMet = (statValueA != comparisonValue); break;
            case 4: case 5:   comparisonMet = (statValueA <  comparisonValue); break;
            case 6: case 7:   comparisonMet = (statValueA <= comparisonValue); break;
            case 8: case 9:   comparisonMet = (statValueA >  comparisonValue); break;
            case 10: case 11: comparisonMet = (statValueA >= comparisonValue); break;
        }
    }

    // Apply requireOtherGoalsMet modifier
    if (self->requireOtherGoalsMet != 0) {
        // requireOtherGoalsMet: 1 if not met, 2 if met
        return comparisonMet ? 2 : 1;
    }

    // Normal: 0 if not met, 1 if met
    return comparisonMet ? 1 : 0;
}

// ============================================================
// StatGoal_ProbeOperands — diagnostic accessor (not retail)
// ============================================================
// Returns the two numbers Check() compares, so a census can print
// "have N, need M" per goal instead of a bare met/not-met bit. Uses the same
// helper Check does, so it can never disagree with the verdict.
extern "C" void StatGoal_ProbeOperands(void* goal, int* outA, int* outB) {
    int a = 0, b = 0;
    if (goal) StatGoal_ComputeOperands(reinterpret_cast<StatGoal*>(goal), a, b);
    if (outA) *outA = a;
    if (outB) *outB = b;
}

// ============================================================
// StatGoal::Slot10/CreateInstance (vtable +0x50, 0x140026510)
// ============================================================
// Construct a StatGoal in pre-allocated memory.
// If memory is NULL, return NULL.

void* StatGoal_Slot10(void* memory) {
    if (!memory) return nullptr;
    return StatGoal_Ctor_Impl(memory);
}

// ============================================================
// StatGoal::Dtor (vtable +0x00, 0x140026540)
// ============================================================
// Destructor: set vtable, destroy String fields, call base ~Goal.
// If flags & 1, free memory.

void* StatGoal_Dtor(StatGoal* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to StatGoal (for correct virtual dispatch during destruction)
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x1401604c0);
    if (vtableAddr) {
        *(void**)self = (void*)vtableAddr;
    }

    // Destroy String fields
    if (fn_String_Dtor) {
        fn_String_Dtor((void*)(s + 0x60));  // statBString
        fn_String_Dtor((void*)(s + 0x48));  // statAString
    }

    // Call base Achievement::Goal::~Goal.
    // This used to say "the base dtor is at a dispatch slot; we call it via the
    // parent vtable". It is neither: it is a plain Runtime export (ordinal 600)
    // resolved by name, with no slot and no vtable in the path.
    typedef void (*fn_GoalDtor_t)(void* self);
    static fn_GoalDtor_t s_fn_GoalDtor = nullptr;
    static bool s_dtorResolved = false;
    if (!s_dtorResolved) {
        // Achievement::Goal::~Goal, retail 0x18009edb0.
        //
        // The mangling is UEAA, not QEAA: Goal is polymorphic, so MSVC exports
        // the VIRTUAL destructor spelling. This resolved
        // "??1Goal@Achievement@@QEAA@XZ" until 2026-09-06 (Wave 5 lane A) --
        // a name the shipped DLL has never contained -- with a fallback to
        // "?~Goal@Achievement@@QEAA@XZ", which is not a valid MSVC mangling in
        // any build. Both returned null silently, the guard below skipped the
        // call, and the caller went on to MemFree the object anyway: every
        // StatGoal destruction leaked its text String (refcounted at +0x28) and
        // skipped Object::External::~External. Control case that proves the
        // rule: GoalList really IS non-virtual and really DOES export
        // "??1GoalList@Achievement@@QEAA@XZ".
        s_fn_GoalDtor = (fn_GoalDtor_t)ResolveExport(
            "??1Goal@Achievement@@UEAA@XZ");
        s_dtorResolved = true;
    }
    if (s_fn_GoalDtor) {
        s_fn_GoalDtor(self);
    }

    // Free memory if requested
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// StatGoal_Factory / StatGoal_CreateInPlace
// ============================================================
// Object::Factory entry points. Called from Runtime DLL's External::Load
// after the typeId tree-walk locates our BaseTypeInfo.
//
// Factory: allocate sizeof(StatGoal)=0x80 with 8-byte alignment, then
// run the in-place constructor. Mirrors the original exe's
// StatGoal_Factory at 0x140026500 (MemAlloc + StatGoal_Ctor).
//
// CreateInPlace: just the constructor (memory provided by caller).
// Equivalent to the existing StatGoal_Slot10 vtable entry — kept as a
// separate name because the BaseTypeInfo's inPlaceCreateFn slot is
// distinct from the vtable slot in the original layout.


#include "Object/factory_registration.h"

// ============================================================
// StatGoal vtable (exe addr 0x1401604c0, 11 slots)
// Slots we've reimplemented are wired up; inherited Achievement::Goal
// slots are nullptr placeholders — calling them will crash, but the
// loader's instantiation path only needs Dtor/Slot2/Slot5/Slot10. Fill the
// inherited slots when something virtual-calls them.
// ============================================================
// Forward decls — these live below in this same TU with C++ linkage.
static int StatGoal_Slot3_Trampoline(StatGoal* self) { return StatGoal_Slot3(self); }

// Slot +0x10: Object::Base::GetTypeId() — virtual that returns the runtime
// CRC32 typeId. Original exe reads from StatGoal's static BaseTypeInfo at
// 0x1401C62B0; we read from our REGISTER_TYPE storage instead (same layout,
// typeId at offset 0). Cached after first lookup.
static uint32_t StatGoal_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) {
        cached = ::Object::GetTypeIdByName("StatGoal");
    }
    return cached;
}

static void* g_StatGoal_Vtable[11] = {
    reinterpret_cast<void*>(&StatGoal_Dtor),               // +0x00 dtor
    nullptr,                                               // +0x08 Slot1 (Goal inherited)
    reinterpret_cast<void*>(&StatGoal_GetTypeId),          // +0x10 Slot2 / GetTypeId
    reinterpret_cast<void*>(&StatGoal_Slot3_Trampoline),   // +0x18 Slot3 (returns 1)
    nullptr,                                               // +0x20 Slot4 (Goal JMP thunk)
    reinterpret_cast<void*>(&StatGoal_Slot5),              // +0x28 Slot5 / Load
    reinterpret_cast<void*>(&StatGoal_Slot6),              // +0x30 Slot6 / Cleanup
    nullptr,                                               // +0x38 Slot7 (Goal stub)
    reinterpret_cast<void*>(&StatGoal_Slot8),              // +0x40 Slot8 / Check
    nullptr,                                               // +0x48 Slot9 (Goal stub)
    reinterpret_cast<void*>(&StatGoal_Slot10),             // +0x50 Slot10 / CreateInstance
};

#include "address_registry.h"

// Called from address_registry.cpp's RegisterVtablesAndTypeIds() so the
// exe address 0x1401604c0 resolves to our synthesized vtable.
extern "C" void RegisterStatGoalVtable() {
    Goal_FillInheritedVtableSlots(g_StatGoal_Vtable, "StatGoal");
    RegisterExeAddr(0x1401604c0, reinterpret_cast<uintptr_t>(g_StatGoal_Vtable));
}

extern "C" void* StatGoal_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(StatGoal), 8);
    if (!p) return nullptr;
    return StatGoal_Ctor_Impl(p);
}

extern "C" void* StatGoal_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return StatGoal_Ctor_Impl(memory);
}

REGISTER_TYPE(StatGoal, "Achievement::Goal",
              &StatGoal_Factory, &StatGoal_CreateInPlace);

// ============================================================
// StatGoal_Ctor_Impl (0x1400daa80)
// ============================================================
// Construct a StatGoal object in pre-allocated memory.
// Calls Achievement::Goal::Goal base constructor, sets vtable,
// initializes all stat-specific fields to defaults.

void* StatGoal_Ctor_Impl(void* memory) {
    // Call base constructor Achievement::Goal::Goal
    typedef void* (*fn_GoalCtor_t)(void* self);
    static fn_GoalCtor_t s_fn_GoalCtor = nullptr;
    static bool s_ctorResolved = false;
    if (!s_ctorResolved) {
        s_fn_GoalCtor = (fn_GoalCtor_t)ResolveExport(
            "??0Goal@Achievement@@QEAA@XZ");
        s_ctorResolved = true;
    }
    if (s_fn_GoalCtor) {
        s_fn_GoalCtor(memory);
    }

    uintptr_t s = (uintptr_t)memory;

    // Set vtable to StatGoal
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x1401604c0);
    if (vtableAddr) {
        *(void**)memory = (void*)vtableAddr;
    }

    // Initialize stat-specific fields (from StatGoal_Ctor at 0x1400daa80)
    *(int32_t*)(s + 0x30) = 5;           // statTypeA (default 5)
    *(int32_t*)(s + 0x34) = 5;           // statTypeB (default 5)
    *(int32_t*)(s + 0x38) = 10;          // comparisonType (default 10 = GE)
    *(int32_t*)(s + 0x3c) = 1;           // fixedValue (default 1)
    *(uint8_t*)(s + 0x40) = 0;           // requireOtherGoalsMet (default false)
    *(uint64_t*)(s + 0x48) = 0;          // statAString (zeroed)
    *(int32_t*)(s + 0x50) = -1;          // statAGeneration (0xFFFFFFFF)
    *(uint64_t*)(s + 0x58) = 0;          // statAExternal (null)
    *(uint64_t*)(s + 0x60) = 0;          // statBString (zeroed)
    *(int32_t*)(s + 0x68) = -1;          // statBGeneration (0xFFFFFFFF)
    *(uint64_t*)(s + 0x70) = 0;          // statBExternal (null)
    *(int32_t*)(s + 0x78) = 0x20;        // statFlags (default 0x20)
    *(int32_t*)(s + 0x7c) = 1;           // statSubType (default 1)

    return memory;
}
