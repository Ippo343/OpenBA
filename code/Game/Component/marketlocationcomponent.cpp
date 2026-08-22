// code/Game/Component/marketlocationcomponent.cpp
// Clean reimplementation of MarketLocationComponent lifecycle methods from Application-x64.exe.
//
// MarketLocationComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. Two non-trivial vtable overrides:
//   Shutdown (0x14004efa0) — Component_Serialize + two empty Serializer blocks
//   OnCreate (0x14004ef60) — slot 7, the REMOVAL hook: erase-by-value of the owner
//                             entity from the town's market-location list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/marketlocationcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// FUN_140002cc0 = DynArray8_EraseByValue — linear scan of an 8-byte-stride DynArray
// (+0x00 data, +0x08 count, +0x0C capacity), shift-down removing EVERY slot equal to
// *value. It is the UNREGISTER primitive, not the append one (append counterpart:
// DynArray8_ResizeFill 0x140003110). Naming it "RegisterEntity" is the exact trap that
// cost a day on the residence registry — see reference_ghidra_oncreate_onremove_mislabel.
typedef void (*fn_ListEraseByValue_t)(void* listPtr, void* entityPtr);
static fn_ListEraseByValue_t s_fn_ListEraseByValue = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_fn_ListEraseByValue = reinterpret_cast<fn_ListEraseByValue_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

// ============================================================
// MarketLocationComponent::Shutdown (vtable slot [3], 0x14004efa0)
// ============================================================
// Calls Component_Serialize (base), then creates/destroys two empty
// Serializer blocks (version 0 each). These exist for binary format
// compatibility (versioned sub-sections with no fields).
//
// Original disassembly:
//   CALL Component_Serialize (0x1401189b0)
//   CALL Serializer_Ctor(local1, stream, 0)
//   CALL Serializer_Dtor(local1)
//   CALL Serializer_Ctor(local2, stream, 0)
//   CALL Serializer_Dtor(local2)

void MarketLocationComponent_Shutdown(MarketLocationComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Two empty serializer blocks (version 0)
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        uint8_t serializerBuf1[24];
        s_fn_Serializer_Ctor(serializerBuf1, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf1);

        uint8_t serializerBuf2[24];
        s_fn_Serializer_Ctor(serializerBuf2, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf2);
    }
}

// ============================================================
// MarketLocationComponent::OnCreate (vtable slot [7], 0x14004ef60)
// ============================================================
// ⚠ The Ghidra name lies. Slot 7 on this component family is the REMOVAL hook, not
// OnAdded: entity mark-removal (FUN_1400c2ea0) broadcasts vtable+0x38 to every
// component whose capability flags (+0x20) carry bit 1 — MarketLocationComponent's
// 0x82 qualifies. The body is a single DynArray8_EraseByValue, i.e. it UNREGISTERS
// the owning entity from the town's market-location list at (gameState+0xf0)+0x10.
// The function name is kept because it is the name in marketlocationcomponent_funcs.h
// (out of this wave's file scope); the behaviour is the erase.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   LEA RDX,[RSP+0x30]          ; stack InternalBasePtr
//   MOV [RSP+0x30],RAX          ; store entity ptr (the needle)
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; town / world manager
//   ADD RCX,0x10                ; market location entity list
//   CALL FUN_140002cc0           ; erase-by-value

void MarketLocationComponent_OnCreate(MarketLocationComponent* self) {
    EnsureHelpers();

    if (!s_fn_ListEraseByValue || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    // Build an InternalBasePtr on the stack with the owner entity
    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    // Get the world manager from game state
    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Erase the owner entity from the market-location list at worldMgr + 0x10
    s_fn_ListEraseByValue((void*)(worldMgr + 0x10), &entityPtr);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MarketLocationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MarketLocationComponent] Funcs resolved: Component_Serialize=%p "
           "Ctor=%p Dtor=%p RegisterEntity=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_ListEraseByValue);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x48 (MarketLocationComponent_Factory MemAlloc), flags 0x82 (Ctor
// 0x14004f060: Component base sets +0x20=0x80, leaf OR 0x2 => 0x82).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Hand-expanded from COMPONENT_INERT_FACTORY(MarketLocationComponent, 0x48, 0x82) so
// slots 3 and 7 carry the two real bodies above instead of the shared inert stub. Both
// have been ported since this file was written and were referenced by NOTHING outside
// marketlocationcomponent_funcs.h — never registered, never in a vtable, dead code.
//
// The class needs no state machine, so this is the InteractComponent/PickingComponent
// shape (a hand-expanded inert factory + FinishVtable), NOT CONTROLLER_LIVE_FACTORY*:
// retail's slots 16/17 here are the shared 0x1400026c0 / 0x140004080 constants, but
// nothing on the market-location entity links this node, and inventing a live node
// would be a mechanism retail does not exercise.
//
// Byte-verified 2026-08-22 from vtable_MarketLocationComponent (0x140159dd0) — the
// bytes, not the source comments, are the authority here because Ghidra mislabels
// OnCreate/OnRemove off by one on this family:
//   slot 3 = 0x14004efa0   MarketLocationComponent::Shutdown  (serialize)
//   slot 7 = 0x14004ef60   the removal hook (Ghidra: "OnCreate")
// The comments happened to have the indices right; they had the slot-7 SEMANTICS
// backwards. See the note on the body above.
//
// Opt-in until measured (BANISHED_LIVE_MARKETLOC). Default (env off) => byte-for-byte
// the previous inert factory.
static uint32_t MarketLocationComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MarketLocationComponent");
    return cached;
}
static void* MarketLocationComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void MarketLocationComponent_ShutdownVSlot(void* self, void* stream) {
    MarketLocationComponent_Shutdown(
        reinterpret_cast<MarketLocationComponent*>(self), stream);
}
static void MarketLocationComponent_RemoveVSlot(void* self) {
    MarketLocationComponent_OnCreate(reinterpret_cast<MarketLocationComponent*>(self));
}
// Must run after every MakeInertTyped/InitInertTyped — those re-stub the shared
// per-type vtable on each instance creation, so a one-shot install would be erased
// by the next component created.
static void MarketLocationComponent_FinishVtable() {
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_MARKETLOC")) return;
    MarketLocationComponent_InertVtable[3] =
        reinterpret_cast<void*>(&MarketLocationComponent_ShutdownVSlot);
    MarketLocationComponent_InertVtable[7] =
        reinterpret_cast<void*>(&MarketLocationComponent_RemoveVSlot);
}
extern "C" void* MarketLocationComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x48, 0x82,
        MarketLocationComponent_InertVtable,
        reinterpret_cast<void*>(&MarketLocationComponent_InertGetTypeId));
    MarketLocationComponent_FinishVtable();
    return p;
}
static void* MarketLocationComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x48, 0x82,
        MarketLocationComponent_InertVtable,
        reinterpret_cast<void*>(&MarketLocationComponent_InertGetTypeId));
    MarketLocationComponent_FinishVtable();
    return mem;
}
REGISTER_TYPE(MarketLocationComponent, "ControllerComponent",
              &MarketLocationComponent_Factory, &MarketLocationComponent_CreateInPlace);
