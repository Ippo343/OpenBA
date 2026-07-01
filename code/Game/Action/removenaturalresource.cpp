// code/Game/Action/removenaturalresource.cpp
// Clean C++ reimplementation of RemoveNaturalResource action methods.
// Source: Game/Action/RemoveNaturalResource.cpp (from profile build debug strings)
//
// RemoveNaturalResource — 0x38 bytes. Removes a natural resource entity.
// Inherits: Object::Internal -> Action (0x20) -> RemoveNaturalResource
// Factory: 0x140022f70, Vtable: 0x140160050
//
// Vtable (10 slots):
//   [0] +0x00 ScalarDtor    = 0x140022fe0
//   [1] +0x08 VectorDtor    = 0x140022f60 (returns type ptr, unhookable)
//   [2] +0x10 BaseDtor      = 0x140022f50 (returns TypeInfo, unhookable)
//   [3] +0x18 Serialize     = 0x1400cac50
//   [4] +0x20 AbstractImpl  = 0x1400cab10
//   [5] +0x28 Execute       = 0x1400c4a90 (base Action, returns 0)
//   [6] +0x30 Enter         = 0x1400c4b90 (base Action, void nop)
//   [7] +0x38 Exit          = 0x1400c4ba0 (base Action, void nop)
//   [8] +0x40 CheckEntity   = 0x1400caaa0
//   [9] +0x48 GetTarget     = 0x1400caa20
//
// Field layout (0x38 bytes):
//   +0x00-0x1F: Action base (vtable, Internal base, entity ptr, status byte)
//   +0x20: void*    citizenEntity  (InternalBasePtr, type DAT_1401c7158)
//   +0x28: void*    resourceEntity (InternalBasePtr, ComponentEntity type)
//   +0x30: uint8_t  harvestFlag
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "RemoveNaturalResource.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — RemoveNaturalResource-specific helpers)
// ============================================================

// Object::Internal::~Internal(this) — via IAT at 0x140152770
static void (*s_fn_ObjectInternal_Dtor)(void* self) = nullptr;

// Object::InternalBasePtr::DeletePtr — clears entity ref during dtor
static void (*s_fn_IBP_DeletePtr)(void* self) = nullptr;

// Object::InternalBasePtr::Serialize(this, serializer) — via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* basePtr, void* serializer) = nullptr;

// FUN_1400052c0 — FindComponent by type DAT_1401c7b30 (NaturalResourceComponent)
// RCX=entity, RDX=outIBP
typedef void* (*s_fn_FindNRComp_t)(void* entity, void* outIBP);
static s_fn_FindNRComp_t s_fn_FindNRComp = nullptr;

// FUN_1400c3470 — notify entity changed (triggers visual update)
static void (*s_fn_NotifyEntityChanged)(void* entity) = nullptr;

// FUN_1401260f0 — remove natural resource helper
// Params: natResComp, citizenEntity IBP, harvestFlag
typedef void (*s_fn_RemoveNatRes_t)(void* natResComp, void* citizenIBP, char harvestFlag);
static s_fn_RemoveNatRes_t s_fn_RemoveNatRes = nullptr;

// Type IDs for IsDerivedFrom checks
static uint32_t* s_typeId_1401c7158 = nullptr;  // entity type (CitizenAI/entity ref)
static uint32_t* s_typeId_NatResDescription = nullptr;  // DAT_1401c7b68

// Vtable addresses for dtor chain
static uintptr_t s_vtable_RemoveNaturalResource = 0;
static uintptr_t s_vtable_Action = 0;

// TypeInfo for BaseDtor (slot 2)
static uint32_t* s_typeInfo_RemoveNaturalResource = nullptr;  // DAT_1401c60c0

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool RemoveNaturalResource_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Internal dtor via IAT
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<void(**)(void*)>(iatDtor);

    // InternalBasePtr::DeletePtr
    s_fn_IBP_DeletePtr = (void(*)(void*))fn_InternalBasePtr_DeletePtr;

    // InternalBasePtr::Serialize via IAT at 0x140152aa0
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // FUN_1400052c0 — FindComponent (NaturalResourceComponent)
    s_fn_FindNRComp = (s_fn_FindNRComp_t)ResolveAddr(Module::Exe, 0x1400052c0);

    // FUN_1400c3470 — notify entity changed
    s_fn_NotifyEntityChanged = (void(*)(void*))ResolveAddr(Module::Exe, 0x1400c3470);

    // FUN_1401260f0 — remove natural resource
    s_fn_RemoveNatRes = (s_fn_RemoveNatRes_t)ResolveAddr(Module::Exe, 0x1401260f0);

    // Type IDs
    s_typeId_1401c7158 = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7158));
    s_typeId_NatResDescription = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7b68));

    // Vtable addresses
    s_vtable_RemoveNaturalResource = ResolveAddr(Module::Exe, 0x140160050);
    s_vtable_Action = ResolveAddr(Module::Exe, 0x14015faa0);  // Action base vtable

    // TypeInfo for BaseDtor
    s_typeInfo_RemoveNaturalResource = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c60c0));

    bool ok = s_fn_ObjectInternal_Dtor && s_fn_IBP_DeletePtr &&
              s_fn_IBP_Serialize && s_fn_FindNRComp &&
              s_fn_NotifyEntityChanged && s_fn_RemoveNatRes &&
              s_typeId_1401c7158 && g_TypeId_ComponentEntity &&
              s_typeId_NatResDescription &&
              s_vtable_RemoveNaturalResource && s_vtable_Action &&
              s_typeInfo_RemoveNaturalResource &&
              fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              fn_MemFreeStatic && g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[RemoveNaturalResource] ResolveFuncs: ObjDtor=%p IBPDel=%p IBPSer=%p FindNR=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_IBP_DeletePtr,
           (void*)s_fn_IBP_Serialize, (void*)s_fn_FindNRComp);
    printf("[RemoveNaturalResource]   NotifyChanged=%p RemoveNR=%p typeId7158=%p NRDesc=%p\n",
           (void*)s_fn_NotifyEntityChanged, (void*)s_fn_RemoveNatRes,
           (void*)s_typeId_1401c7158, (void*)s_typeId_NatResDescription);
    if (!ok) printf("[RemoveNaturalResource] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (0x140022fe0)
// Set vtables to RemoveNaturalResource then Action, cleanup entity ref, call ~Internal, optionally free.
// ============================================================
void* RemoveNaturalResource_ScalarDtor_Impl(RemoveNaturalResource* self, uint32_t flags) {
    uint8_t* s = (uint8_t*)self;

    // Set RemoveNaturalResource vtable (for correct virtual dispatch during destruction)
    *(uintptr_t*)s = s_vtable_RemoveNaturalResource;
    // Set Action vtable (base class destructor phase)
    *(uintptr_t*)s = s_vtable_Action;

    // If entity ref at +0x10 is non-null, delete it
    if (*(void**)(s + 0x10) != nullptr) {
        s_fn_IBP_DeletePtr(s + 0x10);
    }

    // Call Object::Internal::~Internal
    s_fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// BaseDtor (0x140022f50) — slot 2
// Returns TypeInfo for RemoveNaturalResource (tiny, unhookable).
// ============================================================
uint32_t RemoveNaturalResource_BaseDtor_Impl() {
    return *s_typeInfo_RemoveNaturalResource;
}

// ============================================================
// Serialize (0x1400cac50)
// Serializes: status(+0x18), entity(+0x20) with type check, entity(+0x28) with type check, bool(+0x30)
// ============================================================
void RemoveNaturalResource_Serialize_Impl(RemoveNaturalResource* self, void* stream) {
    uint8_t* s = (uint8_t*)self;
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status byte (+0x18)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, s + 0x18, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs and harvest flag
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Serialize citizen entity ref (+0x20)
    s_fn_IBP_Serialize(s + 0x20, ser2);
    if (*(void**)(s + 0x20) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x20), *s_typeId_1401c7158)) {
            *(void**)(s + 0x20) = nullptr;
        }
    }

    // Serialize resource entity ref (+0x28)
    s_fn_IBP_Serialize(s + 0x28, ser2);
    if (*(void**)(s + 0x28) != nullptr) {
        if (!fn_IsDerivedFrom(*(void**)(s + 0x28), *g_TypeId_ComponentEntity)) {
            *(void**)(s + 0x28) = nullptr;
        }
    }

    // Serialize harvest flag (+0x30)
    g_fn_Serialize_Bool(ser2, s + 0x30, nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// AbstractImpl (0x1400cab10) — slot 4
// Checks resource ownership, sets harvest flags, calls removal helper.
// Returns 0 (action complete).
// ============================================================
int RemoveNaturalResource_AbstractImpl_Impl(RemoveNaturalResource* self, void* actionStack) {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    if (std::getenv("BANISHED_FORESTER_SCAN_TRACE")) {
        fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNaturalResource::Main REACHED (self=%p, RemoveNatRes=%p)\n",
                (void*)self, (void*)s_fn_RemoveNatRes);
        fflush(stderr);
    }
    uint8_t* s = (uint8_t*)self;
    void* nrCompIBP = nullptr;

    // Get NaturalResourceComponent from resource entity at +0x28
    fn_InternalBasePtr_CheckPtr(s + 0x28);
    s_fn_FindNRComp(*(void**)(s + 0x28), &nrCompIBP);

    // Standalone-safety: the NR component find can resolve null when the action's target
    // (+0x28) is not wired to a live tree entity. In retail it is always a valid natural
    // resource, so the original derefs local_res8+0x18 unconditionally. Guard so the
    // organic forester cut sequence (which now reaches here — the MoveTo/arrival keystone
    // makes the citizen actually arrive at the tree) is crash-safe rather than dereffing
    // null+0x18. The diagnostic reports the target so the cut-sequence target wiring (the
    // remaining economy gap, PerformWork_Action_Ctor3) can be closed. Returning 0 completes
    // the action so the citizen isn't wedged. Live/DLL mode (non-null) is unchanged.
    fn_InternalBasePtr_CheckPtr(&nrCompIBP);
    if (nrCompIBP == nullptr) {
        if (std::getenv("BANISHED_FORESTER_SCAN_TRACE")) {
            fprintf(stderr, "[ORGANIC-PRODUCE] RemoveNaturalResource: NR-comp null "
                    "(target +0x28=%p) — cut-sequence target not wired to a tree; skipping "
                    "produce (crash-safe). Movement keystone reached this point organically.\n",
                    *(void**)(s + 0x28));
            fflush(stderr);
        }
        return 0;
    }
    uintptr_t nrComp = (uintptr_t)nrCompIBP;
    void* descObj = *(void**)(nrComp + 0x18);

    // Check if description is NaturalResourceDescription type
    bool isNatResDesc = fn_IsDerivedFrom(descObj, *s_typeId_NatResDescription);
    void* natResDesc = isNatResDesc ? descObj : nullptr;

    // Check if the resource has an owner and it's not the town owner
    uintptr_t ownerRef = *(uintptr_t*)((uintptr_t)natResDesc + 0x78);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapPtr = *(uintptr_t*)(gameWorld + 0xb0);
    uintptr_t townOwner = *(uintptr_t*)(mapPtr + 0x30);

    if (ownerRef != 0 && ownerRef != townOwner) {
        // Check if the citizen's current action matches the owner
        fn_InternalBasePtr_CheckPtr(s + 0x20);
        uintptr_t citizenEntity = *(uintptr_t*)(s + 0x20);
        uintptr_t citizenAction = *(uintptr_t*)(citizenEntity + 0x1f0);

        if (citizenAction != ownerRef) {
            // Clear the harvestable flag on NR comp
            fn_InternalBasePtr_CheckPtr(&nrCompIBP);
            nrComp = (uintptr_t)nrCompIBP;
            *(uint8_t*)(nrComp + 0x2d) = 0;
        }
    }

    // Compare citizen's growth value with component threshold
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    uintptr_t citizenEntity = *(uintptr_t*)(s + 0x20);
    uintptr_t comp288 = *(uintptr_t*)(citizenEntity + 0x288);
    int threshold = *(int*)(comp288 + 0x118);
    int growthValue = *(int*)(citizenEntity + 0x210);

    fn_InternalBasePtr_CheckPtr(&nrCompIBP);
    nrComp = (uintptr_t)nrCompIBP;
    *(bool*)(nrComp + 0x2c) = (threshold <= growthValue);

    // Get citizen's tool/work entity ref at +0x248
    fn_InternalBasePtr_CheckPtr(s + 0x20);
    citizenEntity = *(uintptr_t*)(s + 0x20);
    void* workEntityIBP = *(void**)(citizenEntity + 0x248);

    // Call the removal helper
    fn_InternalBasePtr_CheckPtr(&nrCompIBP);
    nrComp = (uintptr_t)nrCompIBP;
    s_fn_RemoveNatRes((void*)nrComp, &workEntityIBP, *(char*)(s + 0x30));

    return 0;
}

// ============================================================
// CheckEntity (0x1400caaa0) — slot 8
// Clears "in use" bit (bit 20) on entity's model component at +0x28.
// ============================================================
void RemoveNaturalResource_CheckEntity_Impl(RemoveNaturalResource* self) {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;
    void* localIBP[4];

    fn_InternalBasePtr_CheckPtr(s + 0x28);
    s_fn_FindNRComp(*(void**)(s + 0x28), localIBP);

    fn_InternalBasePtr_CheckPtr(localIBP);
    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    if (!modelComp) return;   // [standalone] NR component not found -> nothing to clear
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);
    uint32_t flagsNew = flagsOld & 0xFFEFFFFF;  // clear bit 20
    *(uint32_t*)(modelComp + 0x20) = flagsNew;

    if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
        void* parent = *(void**)(modelComp + 0x10);
        if (parent != nullptr) {
            fn_InternalBasePtr_CheckPtr(modelComp + 0x10);
            s_fn_NotifyEntityChanged(*(void**)(modelComp + 0x10));
        }
    }
}

// ============================================================
// GetTarget (0x1400caa20) — slot 9
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
bool RemoveNaturalResource_GetTarget_Impl(RemoveNaturalResource* self, void* param) {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = (uint8_t*)self;

    fn_InternalBasePtr_CheckPtr(s + 0x28);
    void* entity28 = *(void**)(s + 0x28);

    if (*(void**)param == entity28) {
        return true;
    }

    fn_InternalBasePtr_CheckPtr(s + 0x20);
    void* parentEntity = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    fn_InternalBasePtr_CheckPtr(param);
    return parentEntity == *(void**)param;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x140160050 (10 slots), size 0x38, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RemoveNaturalResource_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("RemoveNaturalResource");
    return c;
}

static void* g_RemoveNaturalResource_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&RemoveNaturalResource_GetTypeId,           // +0x10 GetTypeId
    (void*)&RemoveNaturalResource_Serialize_Impl,      // +0x18 Serialize
    (void*)&RemoveNaturalResource_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&RemoveNaturalResource_CheckEntity_Impl,  // +0x40 CheckEntity
    (void*)&RemoveNaturalResource_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterRemoveNaturalResourceVtable() {
    RegisterExeAddr(0x140160050, (uintptr_t)g_RemoveNaturalResource_Vtable);
}
extern "C" void* RemoveNaturalResource_CreateInPlace(void* mem) {
    RemoveNaturalResource_ResolveFuncs();
    return Action_InitInPlace(mem, 0x38, g_RemoveNaturalResource_Vtable);
}
extern "C" void* RemoveNaturalResource_Factory(void) {
    RemoveNaturalResource_ResolveFuncs();
    return Action_Alloc(0x38, g_RemoveNaturalResource_Vtable);
}
REGISTER_TYPE(RemoveNaturalResource, "Action", &RemoveNaturalResource_Factory, &RemoveNaturalResource_CreateInPlace);
#endif // STANDALONE_EXE
