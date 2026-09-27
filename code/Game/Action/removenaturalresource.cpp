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

#include "RemoveNaturalResource.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CitizenComponent.h"
#include "Component/NaturalResourceDescription.h"

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — RemoveNaturalResource-specific helpers)
// ============================================================


// Object::InternalBasePtr::DeletePtr — clears entity ref during dtor
static void (*s_fn_IBP_DeletePtr)(void* self) = nullptr;

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

// TypeInfo for BaseDtor (slot 2)
static uint32_t* s_typeInfo_RemoveNaturalResource = nullptr;  // DAT_1401c60c0

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool RemoveNaturalResource_ResolveFuncs() {
    if (s_funcs_resolved) return true;


    // InternalBasePtr::DeletePtr
    s_fn_IBP_DeletePtr = (void(*)(void*))fn_InternalBasePtr_DeletePtr;

    // FUN_1400052c0 — FindComponent (NaturalResourceComponent)
    s_fn_FindNRComp = (s_fn_FindNRComp_t)ResolveAddr(Module::Exe, 0x1400052c0);

    // FUN_1400c3470 — notify entity changed
    s_fn_NotifyEntityChanged = (void(*)(void*))ResolveAddr(Module::Exe, 0x1400c3470);

    // FUN_1401260f0 — remove natural resource
    s_fn_RemoveNatRes = (s_fn_RemoveNatRes_t)ResolveAddr(Module::Exe, 0x1401260f0);

    // TypeInfo for BaseDtor
    s_typeInfo_RemoveNaturalResource = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c60c0));

    bool ok = s_fn_IBP_DeletePtr &&
              s_fn_FindNRComp &&
              s_fn_NotifyEntityChanged && s_fn_RemoveNatRes &&
              g_TypeId_ComponentEntity &&
              s_typeInfo_RemoveNaturalResource &&
              fn_IsDerivedFrom &&
              fn_MemFreeStatic && g_pGameWorldSingleton;

#ifdef _DEBUG
    printf("[RemoveNaturalResource] ResolveFuncs: IBPDel=%p FindNR=%p\n",
           (void*)s_fn_IBP_DeletePtr,
           (void*)s_fn_FindNRComp);
    printf("[RemoveNaturalResource]   NotifyChanged=%p RemoveNR=%p\n",
           (void*)s_fn_NotifyEntityChanged, (void*)s_fn_RemoveNatRes);
    if (!ok) printf("[RemoveNaturalResource] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (0x1400cac50)
// Serializes: status(+0x18), entity(+0x20) with type check, entity(+0x28) with type check, bool(+0x30)
// ============================================================
void RemoveNaturalResource::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    RemoveNaturalResource_ResolveFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status byte (+0x18)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(_status, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs and harvest flag
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize citizen entity ref (+0x20)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field20 != nullptr) {
        if (!fn_IsDerivedFrom(_field20, CitizenComponent::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Serialize resource entity ref (+0x28)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field28 != nullptr) {
        if (!fn_IsDerivedFrom(_field28, *g_TypeId_ComponentEntity)) {
            _field28 = nullptr;
        }
    }

    // Serialize harvest flag (+0x30)
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<bool*>(s + 0x30), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400cab10) — slot 4
// Checks resource ownership, sets harvest flags, calls removal helper.
// Returns 0 (action complete).
// ============================================================
int  RemoveNaturalResource::AbstractImpl(void* actionStack) {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    void* nrCompIBP = nullptr;

    // Get NaturalResourceComponent from resource entity at +0x28
    s_fn_FindNRComp(_field28, &nrCompIBP);

    // Standalone-safety: the NR component find can resolve null when the action's target
    // (+0x28) is not wired to a live tree entity. In retail it is always a valid natural
    // resource, so the original derefs local_res8+0x18 unconditionally. Guard so the
    // organic forester cut sequence (which now reaches here — the MoveTo/arrival keystone
    // makes the citizen actually arrive at the tree) is crash-safe rather than dereffing
    // null+0x18. The diagnostic reports the target so the cut-sequence target wiring (the
    // remaining economy gap, PerformWork_Action_Ctor3) can be closed. Returning 0 completes
    // the action so the citizen isn't wedged. The live-world path (non-null) is unchanged.
    if (nrCompIBP == nullptr) {
        return 0;
    }
    uintptr_t nrComp = (uintptr_t)nrCompIBP;
    void* descObj = *(void**)(nrComp + 0x18);

    // Check if description is NaturalResourceDescription type
    bool isNatResDesc = fn_IsDerivedFrom(descObj, NaturalResourceDescription::TypeId());
    void* natResDesc = isNatResDesc ? descObj : nullptr;

    // Check if the resource has an owner and it's not the town owner
    uintptr_t ownerRef = *(uintptr_t*)((uintptr_t)natResDesc + 0x78);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapPtr = *(uintptr_t*)(gameWorld + 0xb0);
    uintptr_t townOwner = *(uintptr_t*)(mapPtr + 0x30);

    if (ownerRef != 0 && ownerRef != townOwner) {
        // Check if the citizen's current action matches the owner
        uintptr_t citizenEntity = *(uintptr_t*)(s + 0x20);
        uintptr_t citizenAction = *(uintptr_t*)(citizenEntity + 0x1f0);

        if (citizenAction != ownerRef) {
            // Clear the harvestable flag on NR comp
            nrComp = (uintptr_t)nrCompIBP;
            *(uint8_t*)(nrComp + 0x2d) = 0;
        }
    }

    // Compare citizen's growth value with component threshold
    uintptr_t citizenEntity = *(uintptr_t*)(s + 0x20);
    uintptr_t comp288 = *(uintptr_t*)(citizenEntity + 0x288);
    int threshold = *(int*)(comp288 + 0x118);
    int growthValue = *(int*)(citizenEntity + 0x210);

    nrComp = (uintptr_t)nrCompIBP;
    *(bool*)(nrComp + 0x2c) = (threshold <= growthValue);

    // Get citizen's tool/work entity ref at +0x248
    citizenEntity = *(uintptr_t*)(s + 0x20);
    void* workEntityIBP = *(void**)(citizenEntity + 0x248);

    // Call the removal helper
    nrComp = (uintptr_t)nrCompIBP;
    s_fn_RemoveNatRes((void*)nrComp, &workEntityIBP, *(char*)(s + 0x30));

    return 0;
}

// ============================================================
// CheckEntity (0x1400caaa0) — slot 8
// Clears "in use" bit (bit 20) on entity's model component at +0x28.
// ============================================================
void RemoveNaturalResource::CheckEntity() {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    void* localIBP[4];

    s_fn_FindNRComp(_field28, localIBP);

    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    if (!modelComp) return;   // [standalone] NR component not found -> nothing to clear
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);
    uint32_t flagsNew = flagsOld & 0xFFEFFFFF;  // clear bit 20
    *(uint32_t*)(modelComp + 0x20) = flagsNew;

    if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
        void* parent = *(void**)(modelComp + 0x10);
        if (parent != nullptr) {
            s_fn_NotifyEntityChanged(*(void**)(modelComp + 0x10));
        }
    }
}

// ============================================================
// GetTarget (0x1400caa20) — slot 9
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
bool RemoveNaturalResource::GetTarget(void* param) {
    RemoveNaturalResource_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    void* entity28 = _field28;

    if (*(void**)param == entity28) {
        return true;
    }

    void* parentEntity = *(void**)(*(uint8_t**)(s + 0x20) + 0x10);
    return parentEntity == *(void**)param;
}

// ============================================================
// class RemoveNaturalResource (RemoveNaturalResource.h) — ENDGAME B.3.8 ACT.
// The hand-built g_RemoveNaturalResource_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140022f70 (inlined ctor). The tree's factory zeroed every field above
// +0x10; kept (not a behaviour change of this step).
RemoveNaturalResource::RemoveNaturalResource()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(0),
      _pad31{} {}

// [0] 0x140022fe0
Object::Base* RemoveNaturalResource::DeletingDtor(unsigned flags) {
    this->~RemoveNaturalResource();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400cac50
// [4] 0x1400cab10
// [8] 0x1400caaa0
// [9] 0x1400caa20

void* RemoveNaturalResource::CreateNewObject() {
    RemoveNaturalResource_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(RemoveNaturalResource), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RemoveNaturalResource());
}
void* RemoveNaturalResource::CreateInPlaceObject(void* storage) {
    RemoveNaturalResource_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RemoveNaturalResource());
}

DEFINE_EXE_VTABLE(RemoveNaturalResource, "_ZTV21RemoveNaturalResource")

DEFINE_EXE_TYPE(RemoveNaturalResource, "Action")
