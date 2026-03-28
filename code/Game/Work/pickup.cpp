// code/Game/Work/pickup.cpp
// Clean C++ reimplementation of Pickup -- all methods.
// Source: Game/Work/Pickup.cpp (from profile build debug strings)
//
// Pickup (0x60 bytes) -- Item pickup work action.
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Vtable: 0x140161810
//
// Pickup differs from most Work classes:
//   - Has its OWN Execute override (re-targets when citizen changes workplace)
//   - Fields: status (+0x48), citizenIBP (+0x50), candidateList (+0x58)
//   - Status values: 0=unassigned, 1=assigned, 2=completing
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/pickup_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers (resolved once by Pickup_ResolveFuncs)
// ============================================================

// All typedefs prefixed with PiW_ to avoid conflicts

// Work::Serialize
typedef void  (*PiW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor/Serialize/GetVersion
typedef void* (*PiW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*PiW_Serializer_Dtor_t)(void* self);
typedef void  (*PiW_Serializer_Serialize_t)(void* self, int* value, const char* name);
typedef int   (*PiW_Serializer_GetVersion_t)(void* self);
// Object::InternalBasePtr::Serialize
typedef void  (*PiW_InternalBasePtr_Serialize_t)(void* basePtr, void* serializer);
// FUN_140018020 -- array/list serialization helper
typedef void  (*PiW_ArraySerialize_t)(void* arr, void* serializer);
// FUN_1400f7100 -- validate work entity
typedef bool  (*PiW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// FUN_1400c5400 -- cancel all work items
typedef void  (*PiW_CancelAllWork_t)(void* actionStack);
// Object::Internal::~Internal (dtor tail call)
typedef void  (*PiW_Internal_Dtor_t)(void* self);
// fn_MemFreeStatic
typedef void  (*PiW_MemFreeStatic_t)(void* ptr);
// Slot4/CreateWork -- original function (called through for complex logic)
typedef int   (*PiW_Slot4_Original_t)(void* self, void* entityIBP, uint64_t param3, uint64_t param4);

static PiW_Work_Serialize_t            s_fn_Work_Serialize = nullptr;
static PiW_Serializer_Ctor_t           s_fn_Serializer_Ctor = nullptr;
static PiW_Serializer_Dtor_t           s_fn_Serializer_Dtor = nullptr;
static PiW_Serializer_Serialize_t      s_fn_Serializer_Serialize = nullptr;
static PiW_Serializer_GetVersion_t     s_fn_Serializer_GetVersion = nullptr;
static PiW_InternalBasePtr_Serialize_t s_fn_InternalBasePtr_Serialize = nullptr;
static PiW_ArraySerialize_t            s_fn_ArraySerialize = nullptr;
static PiW_ValidateEntity_t            s_fn_ValidateEntity = nullptr;
static PiW_CancelAllWork_t             s_fn_CancelAllWork = nullptr;
static PiW_MemFreeStatic_t             s_fn_MemFreeStatic = nullptr;
static PiW_Slot4_Original_t            s_fn_Slot4_Original = nullptr;

// Type ID for validation (Pickup type at DAT_1401c7158)
static uint32_t s_typeId_Pickup = 0;

// Type index constant at DAT_1401c6be8 (for Slot1)
static uint64_t* s_typeIndex_Pickup = nullptr;

// Vtable pointers for dtor chain
static void** s_vtable_Pickup = nullptr;  // 0x140161810
static void** s_vtable_Work = nullptr;    // 0x140161a80

static bool s_pickup_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool Pickup_ResolveFuncs() {
    if (s_pickup_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<PiW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ArraySerialize = reinterpret_cast<PiW_ArraySerialize_t>(
        ResolveAddr(Module::Exe, 0x140018020));
    s_fn_ValidateEntity = reinterpret_cast<PiW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_CancelAllWork = reinterpret_cast<PiW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_Slot4_Original = reinterpret_cast<PiW_Slot4_Original_t>(
        ResolveAddr(Module::Exe, 0x1400f2f00));

    // Dispatch slots (indirect through .rdata)
    // Serializer ctor/dtor -- use same slots as Pickup's Serialize (0x140152ad8/0x140152ad0)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<PiW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<PiW_Serializer_Dtor_t*>(iatSerDtor);

    // Serializer::Serialize
    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer)
        s_fn_Serializer_Serialize = *reinterpret_cast<PiW_Serializer_Serialize_t*>(iatSer);

    // Serializer::GetVersion (correct IAT slot verified from original disasm)
    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer)
        s_fn_Serializer_GetVersion = *reinterpret_cast<PiW_Serializer_GetVersion_t*>(iatGetVer);

    // InternalBasePtr::Serialize
    uintptr_t iatBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatBPSer)
        s_fn_InternalBasePtr_Serialize = *reinterpret_cast<PiW_InternalBasePtr_Serialize_t*>(iatBPSer);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<PiW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID -- read uint32 value at 0x1401c7158
    uintptr_t typeIdAddr = ResolveAddr(Module::Exe, 0x1401c7158);
    if (typeIdAddr)
        s_typeId_Pickup = *reinterpret_cast<uint32_t*>(typeIdAddr);

    // Type index constant at DAT_1401c6be8 (for Slot1)
    s_typeIndex_Pickup = reinterpret_cast<uint64_t*>(
        ResolveAddr(Module::Exe, 0x1401c6be8));

    // Vtable pointers for dtor
    s_vtable_Pickup = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161810));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_Serialize && s_fn_Serializer_GetVersion &&
              s_fn_InternalBasePtr_Serialize && s_fn_ArraySerialize &&
              s_fn_ValidateEntity && s_fn_CancelAllWork &&
              s_fn_Slot4_Original &&
              fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton &&
              s_vtable_Pickup && s_vtable_Work;

    s_pickup_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[Pickup] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_Serialize) printf("  MISSING: Serializer_Serialize\n");
        if (!s_fn_Serializer_GetVersion) printf("  MISSING: Serializer_GetVersion\n");
        if (!s_fn_InternalBasePtr_Serialize) printf("  MISSING: InternalBasePtr_Serialize\n");
        if (!s_fn_ArraySerialize) printf("  MISSING: ArraySerialize\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_Slot4_Original) printf("  MISSING: Slot4_Original\n");
        if (!fn_InternalBasePtr_CheckPtr) printf("  MISSING: InternalBasePtr_CheckPtr\n");
        if (!fn_InternalBasePtr_Ctor) printf("  MISSING: InternalBasePtr_Ctor\n");
        if (!fn_IsDerivedFrom) printf("  MISSING: IsDerivedFrom\n");
        if (!g_pGameWorldSingleton) printf("  MISSING: g_pGameWorldSingleton\n");
    }
#endif

    return ok;
}


// ============================================================
// Pickup::Serialize (0x1400f3aa0)
// Pattern: Work_Serialize + Serializer(v2)
//   + Serializer::Serialize(status at +0x48)
//   + InternalBasePtr::Serialize(citizenIBP at +0x50) + IsDerivedFrom validate
//   + if version > 0: ArraySerialize(candidateList at +0x58)
// ============================================================
void Pickup_Serialize_Impl(Pickup* self, void* stream) {
    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version=2)
    uint8_t serializerBuf[24] = {0};
    s_fn_Serializer_Ctor(serializerBuf, stream, 2);

    // Serialize uint32 status at +0x48
    s_fn_Serializer_Serialize(serializerBuf, reinterpret_cast<int*>(&self->status), nullptr);

    // Serialize InternalBasePtr at +0x50
    s_fn_InternalBasePtr_Serialize(&self->citizenIBP, serializerBuf);

    // Validate the deserialized ptr against Pickup type
    if (self->citizenIBP != nullptr) {
        if (!fn_IsDerivedFrom(self->citizenIBP, s_typeId_Pickup)) {
            self->citizenIBP = nullptr;
        }
    }

    // Version >= 1: serialize candidate list at +0x58
    int version = s_fn_Serializer_GetVersion(serializerBuf);
    if (version > 0) {
        s_fn_ArraySerialize(&self->candidateList, serializerBuf);
    }

    s_fn_Serializer_Dtor(serializerBuf);
}


// ============================================================
// Pickup::Slot4 / CreateWork (0x1400f2f00)
// Extremely complex pickup target assignment logic (~400 lines).
// Searches for valid pickup targets across all entities, sorts
// candidates by distance, creates PerformWork actions.
//
// Due to the enormous number of helper functions and complex control
// flow, this calls through to the original function. For standalone
// exe, this resolved address will be set at build time.
// ============================================================
int Pickup_Slot4_Impl(Pickup* self, void* entityIBP, uint64_t param3, uint64_t param4) {
    if (s_fn_Slot4_Original) {
        return s_fn_Slot4_Original(self, entityIBP, param3, param4);
    }
    return 1;  // No work available (fallback)
}


// ============================================================
// Pickup::Execute (Slot5, 0x1400f38f0)
// UNIQUE to Pickup -- handles re-targeting when citizen changes
// workplace during an active pickup.
//
// Algorithm:
// 1. If status == 0, return false (no active work)
// 2. If citizenIBP is null, return false
// 3. Check if citizen's current workplace matches citizenIBP's workplace
// 4. If different: cancel old work, re-run Slot4 for both entities
// 5. Return false (keep executing)
// ============================================================
int Pickup_Execute_Impl(Pickup* self, void* entityIBP, void* param3) {
    // Check status != 0 and citizenIBP != null
    if (self->status == 0 || self->citizenIBP == nullptr) {
        return 0;
    }

    // Check citizenIBP (+0x50)
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)(*(uintptr_t*)entityIBP + 0x1f0);

    fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
    uintptr_t storedCitizen = (uintptr_t)self->citizenIBP;

    if (citizenEntity != *(uintptr_t*)(storedCitizen + 0x1f0)) {
        // Citizen changed workplace -- cancel old work and re-target
        fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
        uintptr_t oldCitizen = (uintptr_t)self->citizenIBP;

        fn_InternalBasePtr_CheckPtr((void*)(oldCitizen + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(oldCitizen + 0x278) + 0x48));

        // Re-run Slot4 with the new citizen
        uintptr_t newCitizenRef = *(uintptr_t*)entityIBP;
        uintptr_t local_buf[1] = { newCitizenRef };

        // Call Slot4 via vtable (guard against freed self after CancelAllWork)
        uintptr_t vtable = *(uintptr_t*)self;
        if (vtable == 0) {
            return 0;  // self was freed during CancelAllWork
        }
        typedef int (*Slot4_fn)(void*, void*, void*);
        auto slot4 = *reinterpret_cast<Slot4_fn*>(vtable + 0x20);
        int result = slot4(self, local_buf, param3);

        if (result == 2) {
            return 1;  // Successfully re-targeted
        }

        // Try again with stored citizen
        uintptr_t storedBuf[1] = { storedCitizen };
        slot4(self, storedBuf, param3);
    }

    return 0;
}


// ============================================================
// Pickup::Slot6 / OnComplete (0x1400f2ec0)
// Resets citizenIBP to null, sets status to 2 (completing).
// Returns false.
// ============================================================
bool Pickup_Slot6_Impl(Pickup* self) {
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    self->citizenIBP = (void*)nullIBP;
    self->status = 2;
    return false;
}


// ============================================================
// Pickup::Slot7 / OnCancel (0x1400f2e80)
// Resets citizenIBP to null, sets status to 0 (unassigned).
// ============================================================
int Pickup_Slot7_Impl(Pickup* self) {
    uintptr_t nullIBP = 0;
    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
    self->citizenIBP = (void*)nullIBP;
    self->status = 0;
    return 0;
}


// ============================================================
// Pickup::Slot8 / CancelAll (0x1400f39f0)
// If status == 1 (assigned), cancels the citizen's active work.
// ============================================================
int Pickup_Slot8_Impl(Pickup* self) {
    if (self->status == 1) {
        fn_InternalBasePtr_CheckPtr(&self->citizenIBP);
        uintptr_t citizenEntity = (uintptr_t)self->citizenIBP;

        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// Pickup::Slot9 / Validate (0x1400f2e20)
// Validates the assigned citizen entity.
// If invalid, resets citizenIBP to null and status to 0.
// Returns true if valid (or no citizen assigned).
// ============================================================
bool Pickup_Slot9_Impl(Pickup* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t citizenRef = (uintptr_t)self->citizenIBP;

    if (citizenRef == 0) {
        return true;  // No citizen assigned -- nothing to validate
    }

    // Validate entity via FUN_1400f7100
    uintptr_t entityIBP[1] = { citizenRef };
    bool valid = s_fn_ValidateEntity(selfAddr, param2, entityIBP, nullptr);

    if (!valid) {
        // Entity no longer valid -- reset
        uintptr_t nullIBP = 0;
        fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
        self->citizenIBP = (void*)nullIBP;
        self->status = 0;
        return false;
    }

    return true;
}


// ============================================================
// Pickup::Slot10 / GetTarget (0x1400f3a50)
// Returns the global entity from GameWorld+0xb0+0x30.
// This is a global target, not per-instance.
// ============================================================
void* Pickup_Slot10_Impl(Pickup* /*self*/, void* outPtr) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)outPtr = *(uintptr_t*)(entityList + 0x30);
    return outPtr;
}


// ============================================================
// Pickup::Slot11 / HasWorker (0x14002c000)
// Returns true if citizenIBP is non-null (worker assigned).
// 10 bytes, unhookable.
// ============================================================
bool Pickup_Slot11_Impl(Pickup* self) {
    return self->citizenIBP != nullptr;
}


// ============================================================
// Pickup::Slot1 / GetTypeIndex (0x14002bfb0)
// Returns type index constant from DAT_1401c6be8.
// Tiny, not hooked.
// ============================================================
uint64_t Pickup_Slot1_Impl(Pickup* /*self*/) {
    if (s_typeIndex_Pickup) {
        return *s_typeIndex_Pickup;
    }
    return 0;
}


// ============================================================
// Pickup::Slot2 / GetTypeId (0x14002bfa0)
// Returns Pickup type ID constant (TypeInfo_Pickup).
// Tiny, not hooked.
// ============================================================
uint32_t Pickup_Slot2_Impl(Pickup* /*self*/) {
    return s_typeId_Pickup;
}


// ============================================================
// Pickup::DtorBody
// Sets vtable to Pickup, then Work, calls Internal dtor.
// Simpler than HuntWork -- no BST tree to free (Pickup doesn't
// have a BST, just simple fields).
// ============================================================
void Pickup_DtorBody(Pickup* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to Pickup
    *(void**)selfAddr = s_vtable_Pickup;

    // Set vtable to Work (dtor chain)
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// Pickup::ScalarDtor (0x14002c010)
// Calls dtor body, then conditional free based on flags.
// ============================================================
void* Pickup_ScalarDtor_Impl(Pickup* self, uint32_t flags) {
    Pickup_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}
