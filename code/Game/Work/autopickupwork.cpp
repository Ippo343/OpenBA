// code/Game/Work/autopickupwork.cpp
// Clean reimplementation of AutoPickupWork base class methods.
//
// AutoPickupWork (0x48 bytes) — Abstract base for auto-pickup work actions.
// Inherits: Object::Internal -> Work -> AutoPickupWork
// Vtable at 0x140160fa0 (12 slots, most are _purecall).
//
// Concrete derived types: HarvestArea, HarvestOrchard, ForesterWork,
// HerbalistWork, HuntWork, PastureWork, FishingWork, etc.
//
// Decompiled from Application-x64.exe.

#include "Work/autopickupwork_funcs.h"
#include "Work/work_base_standalone.h"     // Work_Slot1 / Work_Slot12 / Work_PureCall
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Resolved function pointers (local to this translation unit)
// ============================================================

// Object::Internal::Internal — dispatch slot at 0x140152768
typedef void* (*fn_ObjectInternal_Ctor_t)(void* self);
static fn_ObjectInternal_Ctor_t s_fn_ObjectInternal_Ctor = nullptr;

// Object::Internal::~Internal — dispatch slot at 0x140152770
typedef void (*fn_ObjectInternal_Dtor_t)(void* self);
static fn_ObjectInternal_Dtor_t s_fn_ObjectInternal_Dtor = nullptr;

// Work::Serialize (Work_Shutdown) — exe address 0x1400F7400
typedef void (*fn_Work_Serialize_t)(void* self, void* stream);
static fn_Work_Serialize_t s_fn_Work_Serialize = nullptr;

// FUN_140022520 — finds AutoPickupComponent on entity, returns InternalBasePtr
// Signature: (entity*, outPtr*) -> outPtr*
typedef void* (*fn_FindAutoPickupComponent_t)(void* entity, void* outPtr);
static fn_FindAutoPickupComponent_t s_fn_FindAutoPickupComponent = nullptr;

// Cached vtable addresses
static void* s_vtable_Work = nullptr;
static void* s_vtable_AutoPickupWork = nullptr;

// ============================================================
// Init — resolve all function pointers (called once)
// ============================================================
static bool s_initialized = false;

static void EnsureInit() {
    if (s_initialized) return;
    s_initialized = true;

    // Object::Internal::Internal — indirect through dispatch slot
    s_fn_ObjectInternal_Ctor = reinterpret_cast<fn_ObjectInternal_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152768)));

    // Object::Internal::~Internal — indirect through dispatch slot
    s_fn_ObjectInternal_Dtor = reinterpret_cast<fn_ObjectInternal_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152770)));

    // Work::Serialize — direct exe address
    s_fn_Work_Serialize = reinterpret_cast<fn_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400F7400));

    // FUN_140022520 — find AutoPickupComponent on entity
    s_fn_FindAutoPickupComponent = reinterpret_cast<fn_FindAutoPickupComponent_t>(
        ResolveAddr(Module::Exe, 0x140022520));

    // Vtables
    s_vtable_Work = (void*)ResolveAddr(Module::Exe, WorkAddr::Vtable);
    s_vtable_AutoPickupWork = (void*)ResolveAddr(Module::Exe, AutoPickupWorkAddr::Vtable);
}

// ============================================================
// The AutoPickupWork BASE vtable — exe 0x140160fa0 (AutoPickupWorkAddr::Vtable)
// ============================================================
// Registered NOWHERE until now, so every ResolveAddr of it returned 0 and both
// s_vtable_AutoPickupWork writes below stored NULL. The slot values are the retail
// bytes at 0x140160fa0 and they match this file's own per-slot comments exactly:
//   +0x00 0x1400294e0 ScalarDtor · +0x18 0x1400e3f80 Serialize ·
//   +0x28 0x1400f7540 Execute (`return 0`) · +0x40 0x1400e3f30 OnRemove ·
//   +0x60 0x140006b10 nop · everything else 0x14003addc (_purecall).
// Slots 1 and 2 (0x1400294d0 dtor thunk, 0x1400294c0 GetTypeId) are unported;
// they get the shared stand-ins, as the concrete Work types do.
//
// ⚠ NOT 0x140161380 — that address is the RTTI type descriptor for ForesterWork
// ("ForesterWork\0" in the bytes), which harvestorchard.cpp/harvestarea.cpp had
// mistaken for this vtable. See the note in work_base_standalone.cpp.
static uint32_t AutoPickupWork_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("AutoPickupWork");
    return c;
}

static void* g_AutoPickupWork_Vtable[13] = {
    (void*)&AutoPickupWork_ScalarDtor,  // +0x00 (0x1400294e0, PORTED)
    (void*)&Work_Slot1,                 // +0x08 dtor thunk (0x1400294d0, unported)
    (void*)&AutoPickupWork_GetTypeId,   // +0x10 GetTypeId  (0x1400294c0, unported)
    (void*)&AutoPickupWork_Serialize,   // +0x18 (0x1400e3f80, PORTED)
    (void*)&Work_PureCall,              // +0x20 _purecall
    (void*)&AutoPickupWork_Execute,     // +0x28 (0x1400f7540, PORTED — `return 0`)
    (void*)&Work_PureCall,              // +0x30 _purecall
    (void*)&Work_PureCall,              // +0x38 _purecall
    (void*)&AutoPickupWork_OnRemove,    // +0x40 (0x1400e3f30, PORTED)
    (void*)&Work_PureCall,              // +0x48 _purecall
    (void*)&Work_PureCall,              // +0x50 _purecall
    (void*)&Work_PureCall,              // +0x58 _purecall
    (void*)&Work_Slot12,                // +0x60 void nop (0x140006b10)
};

extern "C" void RegisterAutoPickupWorkVtable() {
    RegisterExeAddr(AutoPickupWorkAddr::Vtable, (uintptr_t)g_AutoPickupWork_Vtable);  // 0x140160fa0
}

// ============================================================
// AutoPickupWork::Ctor (0x1400E4050)
// ============================================================
// Calls Object::Internal::Internal, initializes all Work base fields,
// then overwrites vtable to AutoPickupWork vtable.
void* AutoPickupWork_Ctor(AutoPickupWork* self) {
    EnsureInit();

    // Initialize Internal base
    s_fn_ObjectInternal_Ctor(self);

    // Set Work vtable (intermediate step, matches original)
    self->vtable = s_vtable_Work;

    // Initialize Work base fields
    self->_field10 = nullptr;
    self->_field18 = nullptr;
    self->workplaceId = -1;
    self->_field24 = 0;
    self->_field28 = -1;
    self->_field2C = 0;
    self->_field30 = nullptr;
    self->_field38 = nullptr;
    self->_field40 = 0;

    // Set final AutoPickupWork vtable
    self->vtable = s_vtable_AutoPickupWork;

    return self;
}

// ============================================================
// AutoPickupWork::ScalarDtor (vtable +0x00, 0x1400294E0)
// ============================================================
// Resets vtable chain (AutoPickupWork -> Work), destroys Internal base,
// conditionally frees memory.
void* AutoPickupWork_ScalarDtor(AutoPickupWork* self, uint32_t flags) {
    EnsureInit();

    // Reset vtable chain (mirrors original dtor sequence)
    self->vtable = s_vtable_AutoPickupWork;
    self->vtable = s_vtable_Work;

    // Destroy Internal base
    s_fn_ObjectInternal_Dtor(self);

    // Free memory if requested
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// AutoPickupWork::Serialize (vtable +0x18, 0x1400E3F80)
// ============================================================
// Calls Work::Serialize for base fields, then creates an empty
// serializer block (version 0) for AutoPickupWork-specific data.
// AutoPickupWork itself has no extra serialized fields — derived
// types override this to add their own.
void AutoPickupWork_Serialize(AutoPickupWork* self, void* stream) {
    EnsureInit();

    // Serialize Work base fields
    s_fn_Work_Serialize(self, stream);

    // Create and immediately destroy a serializer block (version 0)
    // This establishes the versioned block for derived types to extend
    uint8_t serializer[24]; // IO::Serializer is ~16-24 bytes
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// AutoPickupWork::Execute (vtable +0x28, 0x1400F7540)
// ============================================================
// Trivial — always returns 0 (false). This is the base implementation;
// derived types override with actual work logic.
int AutoPickupWork_Execute(AutoPickupWork* self) {
    (void)self;   // the only read was a CheckPtr, deleted in B.3.3 step 6D
    return 0;
}

// ============================================================
// AutoPickupWork::OnRemove (vtable +0x40, Slot8, 0x1400E3F30)
// ============================================================
// Finds the AutoPickupComponent on the entity at self+0x10,
// then frees its item list (component+0x30/+0x38).
void AutoPickupWork_OnRemove(AutoPickupWork* self) {
    EnsureInit();

    // CheckPtr on the entity InternalBasePtr at self+0x10

    // Find AutoPickupComponent on the entity
    uintptr_t componentPtr[4] = {0}; // InternalBasePtr result buffer
    s_fn_FindAutoPickupComponent(self->_field10, componentPtr);

    // CheckPtr on the found component

    // Free the item list at component+0x30
    void* component = (void*)componentPtr[0];
    void* itemArray = *(void**)((uintptr_t)component + 0x30);
    fn_MemFreeStatic(itemArray);

    // Zero out the item list fields
    *(void**)((uintptr_t)component + 0x30) = nullptr;
    *(void**)((uintptr_t)component + 0x38) = nullptr;
}
