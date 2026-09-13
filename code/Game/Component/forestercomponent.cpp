// code/Game/Component/forestercomponent.cpp
// Clean reimplementation of ForesterComponent lifecycle methods from Application-x64.exe.
//
// ForesterComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint16 foresterFlags (two packed bytes, each serialized as bool)
//
// Vtable at 0x140163460. One non-trivial hookable override:
//   Shutdown (0x14011f960) — ResourceProduceComponent::Shutdown + own serializer
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/forestercomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent::Shutdown (base) at 0x14012b4a0
typedef void (*fn_ResourceProduceShutdown_t)(void* self, void* stream);
static fn_ResourceProduceShutdown_t s_fn_ResourceProduceShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ResourceProduceShutdown = reinterpret_cast<fn_ResourceProduceShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    s_initialized = true;
}

// ============================================================
// ForesterComponent::Shutdown (vtable slot [3], 0x14011f960)
// ============================================================
// ResourceProduceComponent::Shutdown(self, stream)
// Serializer s(stream, 0)
// s.Serialize(&self[0x50], null)  // bool at +0x50 (foresterFlags low byte)
// s.Serialize(&self[0x51], null)  // bool at +0x51 (foresterFlags high byte)
// ~Serializer()

void ForesterComponent_Shutdown(ForesterComponent* self, void* stream) {
    EnsureHelpers();

    // Call base ResourceProduceComponent::Shutdown
    if (s_fn_ResourceProduceShutdown) {
        s_fn_ResourceProduceShutdown(self, stream);
    }

    // Serialize ForesterComponent's own fields
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // foresterFlags at +0x50 is uint16, serialized as two individual bools
    if (s_fn_Serializer_SerializeBool) {
        bool* flagLow  = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x50);
        bool* flagHigh = reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(self) + 0x51);
        s_fn_Serializer_SerializeBool(serializer, flagLow, nullptr);
        s_fn_Serializer_SerializeBool(serializer, flagHigh, nullptr);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ForesterComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ForesterComponent] Funcs resolved: ResourceProduceShutdown=%p "
           "Serializer_Ctor=%p SerializeBool=%p\n",
           (void*)s_fn_ResourceProduceShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_SerializeBool);
#endif
}

// ============================================================
// Vtable slots 13/14 — the panel COMMAND pair
// ============================================================
// Unlike ConsumeProduce/Market (which inherit the ResourceProduce base pair verbatim),
// ForesterComponent OVERRIDES both. Byte-verified from vtable_ForesterComponent
// (0x140163460): slot 13 = 0x14011f920, slot 14 = 0x14011f890.
//
extern "C" void ResourceProduceComponent_Command(void* self);  // base slot 14, 0x14012b5a0

// The forester lodge has TWO independent work toggles, not one:
//   self+0x50 = "plant trees"   (command id 1)
//   self+0x51 = "cut trees"     (command id 2)
// The base pair has no concept of that, which is why this class overrides.

// ── slot 13 — retail 0x14011f920 — per-command availability mask ──────────────────
// Retail body:
//     if (cmd == 1) return -(*(char*)(self+0x50) != 0) & 2 | 1;
//     if (cmd == 2) return -(*(char*)(self+0x51) != 0) & 2 | 1;
//     return 1;
// `-(x) & 2 | 1` is the compiler's branchless form of `x ? 3 : 1`: bit 0 = "this command
// is offered", bit 1 = "it is currently ON". So an unknown command id still returns 1
// (offered, off) rather than 0 — returning 0 is what greys a ControlUI checkbox, and
// retail deliberately never does that here.
//
// ⚠ Unlike the ResourceProduce base slot 13, this one DOES read its command-id argument.
extern "C" uint8_t ForesterComponent_CommandMask(void* self, int cmd) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (cmd == 1) return *reinterpret_cast<char*>(comp + 0x50) ? 3 : 1;
    if (cmd == 2) return *reinterpret_cast<char*>(comp + 0x51) ? 3 : 1;
    return 1;
}

// ── slot 14 — retail 0x14011f890 — toggle one flag, notify only on edge ───────────
// Retail snapshots "is any flag set" BEFORE the toggle, flips the flag the command
// names, recomputes "is any flag set" AFTER, and calls the base slot-14 body only when
// that aggregate CHANGED (none->some or some->none). Toggling "cut" while "plant" is
// already on leaves the lodge working, so no state-machine event is posted — the edge
// test is the whole point of the override, not an optimisation.
//
// The tail call Ghidra prints as `HerbalistComponent_Slot14(param_1)` is address
// 0x14012b5a0 — the ResourceProduce BASE slot-14 body, already ported as
// ResourceProduceComponent_Command. The "Herbalist" name is a subclass label Ghidra
// attached to a base-vtable body; it is not the herbalist's function.
extern "C" void ForesterComponent_Command(void* self, int cmd) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    char* const plant = reinterpret_cast<char*>(comp + 0x50);
    char* const cut   = reinterpret_cast<char*>(comp + 0x51);

    const char cutBefore = *cut;
    const bool wasIdle   = (cutBefore == 0 && *plant == 0);
    const bool wasActive = !wasIdle;

    if (cmd == 1)      *plant = (*plant == 0) ? 1 : 0;
    else if (cmd == 2) *cut   = (cutBefore == 0) ? 1 : 0;

    const bool nowIdle   = (*cut == 0 && *plant == 0);
    const bool nowActive = !nowIdle;

    if ((wasIdle && nowActive) || (wasActive && nowIdle))
        ResourceProduceComponent_Command(self);
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

// Promoted 2026-08-15 from the hand-expanded inert factory (which reached only slots
// 13/14) to a full live ControllerComponent.
//
// The previous wave made the two work checkboxes clickable, but the lodge still did
// nothing: ForesterComponent is a ResourceProduceComponent subclass, so all of its
// production runs off the SHARED ResourceProduce state machine (idle 2 -> producing 13
// -> paused 14), and that machine was never linked because slots 16/17
// (GetDefaultState / CreateState) still held the inert stub. A clickable checkbox on a
// state machine that does not exist is exactly the "live command pair, inert
// controller" shape the coverage harness reported as 17/22.
//
// Size 0x58 / Flags 0x80 / NodeOff 0x28 / DefState 2 all come from
// ForesterComponent_Ctor (0x14011fa40) -> ResourceProduceComponent_Ctor, the same
// derivation as fishingcomponent.cpp.
//
// ⚠ NOT the macro. ForesterComponent_Ctor's own body is two stores:
//       *(ComponentVtable**)self = &vtable_ForesterComponent;
//       *(uint16*)(self + 0x50)  = 0x101;
// and that second store is behaviour, not padding: it is what makes a freshly-built
// lodge default to "plant AND cut both ON". Our factories zero-fill the body, so under
// CONTROLLER_LIVE_FACTORY_STATES every lodge in the game would be born with both work
// flags OFF — slot 13 would grey nothing but slot 14's edge test would see an idle
// lodge, and CreateWork's re-arm (see resourceproduce_creatework.cpp) would be the only
// thing ever turning them on. CONTROLLER_LIVE_FACTORY_STATES' PopulateFn takes no
// arguments, so it cannot reach the instance; the macro is hand-expanded here purely to
// get `mem` in scope for the ctor default.
extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uintptr_t* ForesterComponent_CreateWork(void*, uintptr_t*);   // slot 18

static uint32_t ForesterComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ForesterComponent");
    return cached;
}
static void* ForesterComponent_LiveGetTypeInfo(void*) {
    static void* cached = nullptr;
    if (!cached) cached = ::Object::GetTypeInfoByName("ForesterComponent");
    return cached;
}
static int   ForesterComponent_GetDefaultState(void*) { return 2; }      // slot 16
static void* ForesterComponent_CreateState(void* self) {                 // slot 17
    return reinterpret_cast<uint8_t*>(self) + 0x28;
}
static void* ForesterComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

// Must run AFTER InitInertTyped/MakeInertTyped — FillTypedVtable erases static-init
// writes, so every real slot is (re)installed from in here on every instance creation.
static void ForesterComponent_InstallLive(void* mem) {
    // Slot 3 is the STREAM SERIALIZER, not a gameplay slot: the save encoding
    // does not care whether this component's live feature is enabled. Wired
    // UNCONDITIONALLY, ahead of the gameplay slots.
    // See reference_vtable_slot3_serialize_never_installed.
    ForesterComponent_LiveVtable[3]  = reinterpret_cast<void*>(&ForesterComponent_Shutdown);
    ForesterComponent_LiveVtable[1]  = reinterpret_cast<void*>(&ForesterComponent_LiveGetTypeInfo);
    ForesterComponent_LiveVtable[13] = reinterpret_cast<void*>(&ForesterComponent_CommandMask);
    ForesterComponent_LiveVtable[14] = reinterpret_cast<void*>(&ForesterComponent_Command);
    ForesterComponent_LiveVtable[16] = reinterpret_cast<void*>(&ForesterComponent_GetDefaultState);
    ForesterComponent_LiveVtable[17] = reinterpret_cast<void*>(&ForesterComponent_CreateState);
    ForesterComponent_LiveVtable[18] = reinterpret_cast<void*>(&ForesterComponent_CreateWork);
    RPCtrl_PopulateTable();
    if (!mem) return;
    uint8_t* const p = reinterpret_cast<uint8_t*>(mem);
    // Embedded state node at +0x28: node vtable at +0x00, owning component at +0x18.
    *reinterpret_cast<void**>(p + 0x28)        = reinterpret_cast<void*>(RPCtrl_NodeVtable());
    *reinterpret_cast<void**>(p + 0x28 + 0x18) = mem;
    // ForesterComponent_Ctor's own two-store body (see the block comment above).
    *reinterpret_cast<uint16_t*>(p + 0x50) = 0x101;
}
extern "C" void* ForesterComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x58, 0x80,
        ForesterComponent_LiveVtable,
        reinterpret_cast<void*>(&ForesterComponent_LiveGetTypeId));
    ForesterComponent_InstallLive(p);
    return p;
}
static void* ForesterComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x58, 0x80,
        ForesterComponent_LiveVtable,
        reinterpret_cast<void*>(&ForesterComponent_LiveGetTypeId));
    ForesterComponent_InstallLive(mem);
    return mem;
}
REGISTER_TYPE(ForesterComponent, "ResourceProduceComponent",
              &ForesterComponent_Factory, &ForesterComponent_CreateInPlace);
