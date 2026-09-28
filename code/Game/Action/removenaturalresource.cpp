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
#include "Action/exe_bodies.h"   // ActCNR_*, ActHunt_* helpers
#include "Work/exe_bodies.h"     // WorkBase_FindComponent_52c0
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/CitizenComponent.h"
#include "Component/NaturalResourceDescription.h"
#include "Entity/ComponentEntity.h"

// ============================================================
// Helpers, called by name (B3R)
//   0x1400052c0 WorkBase_FindComponent_52c0 -- FindComponent by type DAT_1401c7b30
//               (NaturalResourceComponent); RCX=entity, RDX=outIBP
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470 -- notify entity changed (visual update)
//   0x1401260f0 ActCNR_RemoveNatRes_1260f0 -- remove natural resource helper
//               (natResComp, citizenEntity IBP, harvestFlag)
//
// ENDGAME B.3.9 CB: retail reads this record field ONLY in the class's own TypeName
// slot (0x1401c60c0 = the NAME field (+8) of RemoveNaturalResource's record, read by
// RemoveNaturalResource_Slot1 0x140022f60); DECLARE_EXE_TYPE answers that slot, so the tree's copy of it is gone.
// ============================================================

// ============================================================
// Serialize (0x1400cac50)
// Serializes: status(+0x18), entity(+0x20) with type check, entity(+0x28) with type check, bool(+0x30)
// ============================================================
void RemoveNaturalResource::Persist(void* stream) {
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
        if (!::Object::IsDerivedFromNullSafe(_field20, CitizenComponent::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Serialize resource entity ref (+0x28)
    reinterpret_cast<Object::InternalBasePtr*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (_field28 != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field28, ComponentEntity::TypeId())) {
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
    (void)actionStack;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    void* nrCompIBP = nullptr;

    // Get NaturalResourceComponent from resource entity at +0x28
    WorkBase_FindComponent_52c0(reinterpret_cast<uintptr_t>(_field28), &nrCompIBP);   // 0x1400052c0

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
    bool isNatResDesc = ::Object::IsDerivedFromNullSafe(descObj, NaturalResourceDescription::TypeId());
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
    ActCNR_RemoveNatRes_1260f0((int64_t)nrComp, &workEntityIBP, *(char*)(s + 0x30));   // 0x1401260f0

    return 0;
}

// ============================================================
// CheckEntity (0x1400caaa0) — slot 8
// Clears "in use" bit (bit 20) on entity's model component at +0x28.
// ============================================================
void RemoveNaturalResource::CheckEntity() {
    void* localIBP[4];

    WorkBase_FindComponent_52c0(reinterpret_cast<uintptr_t>(_field28), localIBP);   // 0x1400052c0

    uint8_t* modelComp = (uint8_t*)(localIBP[0]);
    if (!modelComp) return;   // [standalone] NR component not found -> nothing to clear
    uint32_t flagsOld = *(uint32_t*)(modelComp + 0x20);
    uint32_t flagsNew = flagsOld & 0xFFEFFFFF;  // clear bit 20
    *(uint32_t*)(modelComp + 0x20) = flagsNew;

    if (((flagsNew ^ flagsOld) & 0xFFFF) != 0) {
        void* parent = *(void**)(modelComp + 0x10);
        if (parent != nullptr) {
            ActHunt_NotifyEntityChanged_c3470(*(void**)(modelComp + 0x10));   // 0x1400c3470
        }
    }
}

// ============================================================
// GetTarget (0x1400caa20) — slot 9
// Returns true if param matches entity at +0x28 or entity at +0x20's parent.
// ============================================================
bool RemoveNaturalResource::GetTarget(void* param) {
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

// [3] 0x1400cac50
// [4] 0x1400cab10
// [8] 0x1400caaa0
// [9] 0x1400caa20

void* RemoveNaturalResource::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RemoveNaturalResource), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) RemoveNaturalResource());
}
void* RemoveNaturalResource::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) RemoveNaturalResource());
}


DEFINE_EXE_TYPE(RemoveNaturalResource, "Action")
