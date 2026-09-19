// code/Game/Component/destroycomponent.cpp
// Clean reimplementation of DestroyComponent lifecycle methods from Application-x64.exe.
//
// DestroyComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages building demolition/destruction process.
// Fields: field48 (Time), destroyFlags (uint16 +0x50), field51 (bool),
//         field54 (uint32), field58 (ptr), field60 (entity ref / mesh ID).
//
// Vtable at 0x140163120. Two non-trivial overrides:
//   Shutdown  (0x14011DB50) -- serialize all destroy state fields
//   Serialize (0x14011DB00) -- OnCreate: copy mesh ref from building data
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/destroycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// System::Time::Serialize -- dispatch slot at 0x140152aa8
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// FUN_140002340 -- serialize InternalBasePtr (field60)
typedef void (*fn_SerializeInternalBasePtr_t)(void* ptrField, void* serializer);
static fn_SerializeInternalBasePtr_t s_fn_SerializeInternalBasePtr = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// Type ID for building component type (DAT_1401c7628)
static uint32_t* s_pBuildingCompTypeId = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_SerializeInternalBasePtr = reinterpret_cast<fn_SerializeInternalBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_pBuildingCompTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// DestroyComponent::Shutdown (vtable slot [3], 0x14011DB50)
// ============================================================
// Full serialization of destroy state:
//   1. Component_Serialize
//   2. Empty serializer block (controller compat)
//   3. Serializer(version=0): field54 (int), field58 (int),
//      field51 (bool), field60 (InternalBasePtr via FUN_140002340),
//      destroyFlags at +0x50 (bool), field48 (Time)

void DestroyComponent_Shutdown(DestroyComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller format compat)
    alignas(8) uint8_t serializer1[24];
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Serialize destroy state fields
    alignas(8) uint8_t serializer2[24];
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>(&self->field54), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((uintptr_t)self + 0x58), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<bool*>((uintptr_t)self + 0x51), nullptr);   // B.3.4 bridge

    if (s_fn_SerializeInternalBasePtr) {
        s_fn_SerializeInternalBasePtr((void*)((uintptr_t)self + 0x60), serializer2);
    }

    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<bool*>(&self->destroyFlags), nullptr);   // B.3.4 bridge

    if (s_fn_Time_Serialize) {
        reinterpret_cast<System::Time*>((void*)&self->field48)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer2));   // B.3.4b bridge
    }

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// DestroyComponent::Serialize (vtable slot [4], 0x14011DB00)
// ============================================================
// Copies mesh reference from building data. Reads _unk18,
// does IsDerivedFrom check, copies 8 bytes from building+0x20
// into field60.

void DestroyComponent_Serialize(DestroyComponent* self) {
    EnsureHelpers();

    void* baseObj = self->_unk18;
    if (!s_fn_IsDerivedFrom_d || !s_pBuildingCompTypeId) return;

    bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pBuildingCompTypeId);
    void* buildingData = isDerived ? baseObj : nullptr;

    if (buildingData) {
        // Copy mesh reference from building data +0x20 into field60
        *(uint64_t*)&self->field60 = *(uint64_t*)((uintptr_t)buildingData + 0x20);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DestroyComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[DestroyComponent] Funcs resolved: Serialize=%p IsDerivedFrom=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IsDerivedFrom_d);
#endif
}

// ===== Standalone factory (Foundation D → upgrade-path live states) =====
// BANISHED_LIVE_DESTROY is DEFAULT-ON via the default-on table (graduated 2026-07-25 once
// the upgrade path was verified end-to-end); the env var is now a kill-switch (=0),
// which reverts this component to inert. It graduates:
//   * vtable index 4 (+0x20, 0x14011DB00 Serialize/Startup) — copies the
//     DestroyDescription's default target (desc+0x20) into +0x60, dispatched at
//     spawn by ComponentEntity_StartupComponents (flags 0x80 → bit 7 set).
//   * vtable index 3 (+0x18, 0x14011DB50 Shutdown) — save serialization.
//   * vtable index 16/17 (GetDefaultState / CreateState=&node@+0x28) + the REAL
//     state table (destroycomponent_states.cpp) — the retail mechanism that
//     lets the entity's StateMachineComponent link + tick the destroy node, and
//     the 0x14011dfa0 pump (UpgradeUI click) drive states 3→4→5 (demolish →
//     haul-away → refund + stone-house spawn).
// GetDefaultState: Ghidra-verified — retail vtable slot 16 (0x1401631a0)
// points at 0x140033c00 = `mov eax,4 ; ret`, and slot 17 (0x1401631a8) at
// 0x140004080 = `lea rax,[rcx+0x28] ; ret`. We return 4 to match.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_state_table.h"

extern "C" void** DestroyCtrl_NodeVtable();     // destroycomponent_states.cpp
extern "C" void   DestroyCtrl_PopulateTable();

static uint32_t DestroyComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DestroyComponent");
    return cached;
}
static void* DestroyComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void DestroyComponent_ShutdownVSlot(void* s, void* stream) {   // idx 3 (+0x18)
    DestroyComponent_Shutdown(reinterpret_cast<DestroyComponent*>(s), stream);
}
static void DestroyComponent_SerializeVSlot(void* s) {                // idx 4 (+0x20)
    DestroyComponent_Serialize(reinterpret_cast<DestroyComponent*>(s));
}
// Retail slot 16 returns 4 (Ghidra-verified). That is only safe under retail's
// first-controller consult order, where the build controller precedes destroy
// in every building template. Our SM startup adopts the default from the FIRST
// controller with a non-null CreateState, and the build controller is live, so
// the faithful 4 (haul-away) applies — this is also what gives the starting
// supply cart its haul-away-then-vanish behaviour.
static int DestroyComponent_GetDefaultState(void*) {
    return 4;
}
static void* DestroyComponent_CreateState(void* self) {               // idx 17
    return reinterpret_cast<uint8_t*>(self) + 0x28;
}
// Vtable slot 15 (+0x78 "OnMessage") — retail DestroyComponent_Slot15 0x14011daf0:
//   if (msgCode == 0) ++*(int*)(self + 0x54);
// The demolition-progress credit, destroy-side twin of BuildComponent_MsgReceive
// (buildcomponent.cpp). The demolition action chain (BuildStructure_SetupBuildActions,
// altAnim=1) pushes a SendComponentMessage carrying the DestroyComponent type-id
// (DAT_1401c75f0); each completed PerformWork routes here and advances +0x54 toward
// the +0x58 target that gates the state-5 tick.
static void DestroyComponent_MsgReceive(void* self, int msgCode,
                                        void* /*srcEntityRef*/,
                                        uint32_t /*p1*/, uint32_t /*p2*/) {
    if (msgCode == 0)
        ++*reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(self) + 0x54);
}

static void DestroyComponent_FinishLiveVtable(void* mem) {
    // Slot 3 is the STREAM SERIALIZER, wired ahead of the gameplay slots.
    // See reference_vtable_slot3_serialize_never_installed.
    DestroyComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&DestroyComponent_ShutdownVSlot);
    DestroyComponent_LiveVtable[4] =
        reinterpret_cast<void*>(&DestroyComponent_SerializeVSlot);
    DestroyComponent_LiveVtable[15] =
        reinterpret_cast<void*>(&DestroyComponent_MsgReceive);
    DestroyComponent_LiveVtable[16] =
        reinterpret_cast<void*>(&DestroyComponent_GetDefaultState);
    DestroyComponent_LiveVtable[17] =
        reinterpret_cast<void*>(&DestroyComponent_CreateState);
    DestroyCtrl_PopulateTable();
    if (mem) {
        // Embedded node at +0x28: dispatcher vtable + node+0x18 back-pointer.
        // node+0x18 == comp+0x40, exactly the selfPtr the retail ctor writes —
        // the tail-call handlers receive it as their component argument.
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28) =
            reinterpret_cast<void*>(DestroyCtrl_NodeVtable());
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + 0x28 + 0x18) = mem;
    }
}

extern "C" void* DestroyComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x68, 0x80,
        DestroyComponent_LiveVtable,
        reinterpret_cast<void*>(&DestroyComponent_LiveGetTypeId));
    DestroyComponent_FinishLiveVtable(p);
    return p;
}
static void* DestroyComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x68, 0x80,
        DestroyComponent_LiveVtable,
        reinterpret_cast<void*>(&DestroyComponent_LiveGetTypeId));
    DestroyComponent_FinishLiveVtable(mem);
    return mem;
}

REGISTER_TYPE(DestroyComponent, "ControllerComponent",
              &DestroyComponent_Factory, &DestroyComponent_CreateInPlace);
