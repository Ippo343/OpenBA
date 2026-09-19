// code/Game/Component/marketcomponent.cpp
// Clean reimplementation of MarketComponent lifecycle methods from Application-x64.exe.
//
// MarketComponent is a ResourceProduceComponent subclass (0x50 bytes) with no
// additional fields beyond the parent. Only one non-trivial vtable override:
//   Shutdown (0x14004ead0) — calls ResourceProduceComponent_Shutdown + empty Serializer block
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/marketcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent_Shutdown (parent) at 0x14012b4a0
typedef void (*fn_ParentShutdown_t)(void* self, void* stream);
static fn_ParentShutdown_t s_fn_ParentShutdown = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ParentShutdown = reinterpret_cast<fn_ParentShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    s_initialized = true;
}

// ============================================================
// MarketComponent::Shutdown (vtable slot [3], 0x14004ead0)
// ============================================================
// Calls parent ResourceProduceComponent_Shutdown, then creates/destroys
// an empty Serializer block (version 0). This empty block exists for
// binary format compatibility (versioned sub-section with no fields).
//
// Original disassembly:
//   CALL ResourceProduceComponent_Shutdown (0x14012b4a0)
//   CALL Serializer_Ctor(local, stream, 0)
//   CALL Serializer_Dtor(local)

void MarketComponent_Shutdown(MarketComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    // Empty serializer block (version 0)
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MarketComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MarketComponent] Funcs resolved: ParentShutdown=%p\n",
           (void*)s_fn_ParentShutdown);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid MarketComponent: size 0x50 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x80 (from MarketComponent_Ctor) + zeroed body.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Hand-expanded from COMPONENT_INERT_FACTORY(MarketComponent, 0x50, 0x80) so vtable
// slots 13/14 — the panel command pair driving the "work" checkbox — carry real bodies
// instead of the inert stub (stub slot 14 = click is a no-op; stub slot 13 returns 0,
// which greys the checkbox every frame via ControlUI_Slot7).
//
// Byte-verified 2026-08-15 from vtable_MarketComponent (0x140159c40):
//   slot 13 = 0x14012b520   slot 14 = 0x14012b5a0
// — identical to vtable_ResourceProduceComponent (0x1401643f0), so Market declares no
// override and inherits the base pair verbatim. Both are already ported as
// ResourceProduceComponent_CommandMask / _Command. See the longer note in
// consumeproducecomponent.cpp for the Ghidra "HerbalistComponent_Slot13/14" naming trap.
//
// Opt-in until measured (BANISHED_LIVE_MARKET_CMD).
// ⚠ Wiring 13/14 alone is NOT enough here — see the long note in
// consumeproducecomponent.cpp. The base slot 13 reads the STATE MACHINE state and the
// base slot 14 posts to the component's own controller node, so with slots 16/17 stubbed
// the node is never linked and a click measured `mask 0x02 -> 0x02` (no change).
//
// Macro arguments, byte-verified from vtable_MarketComponent (0x140159c40):
//   Size 0x50 / Flags 0x80 — unchanged from the previous inert factory.
//   NodeOff 0x28  — slot 17 is the shared 0x140004080 `LEA RAX,[RCX+0x28]; RET`.
//   DefState 2    — slot 16 is the shared 0x1400026c0 `MOV EAX,2; RET`.
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uint8_t ResourceProduceComponent_CommandMask(void* self);  // 0x14012b520
extern "C" void    ResourceProduceComponent_Command(void* self);      // 0x14012b5a0
extern "C" uintptr_t* MarketComponent_CreateWork(void* self, uintptr_t* out);  // 0x14004ea40
                                                     // resourceproduce_creatework.cpp

// Slot 3 = Shutdown/Serialize. MarketComponent_Shutdown (above in this file) is the
// real 0x14004ead0 body and has been since it was ported — nothing ever wrote it into
// a vtable, so it was dead code. Dispatched as (self, stream).
static void MarketComponent_ShutdownVSlot(void* self, void* stream) {
    MarketComponent_Shutdown(reinterpret_cast<MarketComponent*>(self), stream);
}

// Slots 13/14/3/18 must be written from INSIDE InstallLive — FillTypedVtable erases
// static-init writes on the first factory call.
static void MarketCtrl_PopulateAndWireCmd();

CONTROLLER_LIVE_FACTORY_STATES(MarketComponent, 0x50, 0x80, 0x28, 2,
                               RPCtrl_NodeVtable, MarketCtrl_PopulateAndWireCmd)

static void MarketCtrl_PopulateAndWireCmd() {
    // Byte-verified 2026-08-22 from vtable_MarketComponent (0x140159c40):
    //   slot 3 = 0x14004ead0 (MarketComponent::Shutdown)
    //   slot 18 = 0x14004ea40 (MarketComponent::CreateWork — MarketWork ctor:
    //             MemAlloc(0x58,8), Work_Ctor25(owner IBP), vtable_MarketWork,
    //             +0x48 = 0, +0x50 = 0)
    // A live-probe slot census over a placed market read these two as the only
    // MISSING entries of the 22 (7 ported, 13 equivalent-noop). Both bodies existed
    // already; neither was reachable.
    MarketComponent_LiveVtable[3] =
        reinterpret_cast<void*>(&MarketComponent_ShutdownVSlot);
    MarketComponent_LiveVtable[18] =
        reinterpret_cast<void*>(&MarketComponent_CreateWork);
    MarketComponent_LiveVtable[13] =
        reinterpret_cast<void*>(&ResourceProduceComponent_CommandMask);
    MarketComponent_LiveVtable[14] =
        reinterpret_cast<void*>(&ResourceProduceComponent_Command);
    RPCtrl_PopulateTable();
}
REGISTER_TYPE(MarketComponent, "ResourceProduceComponent",
              &MarketComponent_Factory, &MarketComponent_CreateInPlace);
