// code/Game/Component/quarrycomponent.cpp
// Clean reimplementation of QuarryComponent lifecycle methods from Application-x64.exe.
//
// QuarryComponent (0x28 bytes) — minimal component, same as MineComponent.
// Vtable at 0x14015a490. Only Shutdown has real logic.
//

#include "Component/quarrycomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// QuarryComponent::Shutdown (vtable slot [3], 0x140055230)
// ============================================================

void QuarryComponent_Shutdown(QuarryComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor)
        s_fn_Serializer_Ctor(serializer, stream, 0);
    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void QuarryComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Size 0x28 + Object::Internal base + capability flags +0x20=0x83 (from
// QuarryComponent_Ctor) + zeroed body.
//
// This was COMPONENT_INERT_FACTORY until the quarry-carve wave: every instance carried
// the shared 48-slot stub vtable, so the Shutdown body above was unreachable and the
// terrain deformation had no port at all. The pit a quarry digs is a heightmap carve
// driven from two vtable slots — bodies in quarrycomponent_deform.cpp:
//   [3]  Shutdown  0x140055230 (this file)
//   [12] Message   0x1400550f0 — initial carve at _initialDepth on msg 4 (build done)
//   [15] Message   0x140054b80 — progressive carve + terrain conversion, sent by
//                                ConsumeProduce on every completed production cycle
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

extern "C" void QuarryComponent_Slot12(void* self, int msgId);
extern "C" void QuarryComponent_Slot15(void* self);

static uint32_t QuarryComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("QuarryComponent");
    return cached;
}
static void* QuarryComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void QuarryComponent_ShutdownVSlot(void* s, void* stream) {
    QuarryComponent_InitFuncs();
    QuarryComponent_Shutdown(reinterpret_cast<QuarryComponent*>(s), stream);
}

// Re-run per instance: InitInertTyped re-stubs the shared vtable on every spawn, so the
// live slots must be (re)written here rather than at static-init time.
static void QuarryComponent_InstallLive() {
    // Slot 3 is the STREAM SERIALIZER, not a gameplay slot: the save encoding
    // does not care whether this component's live feature is enabled. Wired
    // UNCONDITIONALLY, ahead of the gate -- behind it, a disabled flag left the
    // class falling through to base-only Component_Serialize, which skips this
    // body's bytes and desyncs the stream for EVERY object after it.
    // See reference_vtable_slot3_serialize_never_installed.
    QuarryComponent_LiveVtable[3]  = reinterpret_cast<void*>(&QuarryComponent_ShutdownVSlot);
    QuarryComponent_LiveVtable[12] = reinterpret_cast<void*>(&QuarryComponent_Slot12);
    QuarryComponent_LiveVtable[15] = reinterpret_cast<void*>(&QuarryComponent_Slot15);
}

extern "C" void* QuarryComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x28, 0x83,
        QuarryComponent_LiveVtable,
        reinterpret_cast<void*>(&QuarryComponent_LiveGetTypeId));
    QuarryComponent_InstallLive();
    return p;
}
static void* QuarryComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x28, 0x83,
        QuarryComponent_LiveVtable,
        reinterpret_cast<void*>(&QuarryComponent_LiveGetTypeId));
    QuarryComponent_InstallLive();
    return mem;
}
REGISTER_TYPE(QuarryComponent, "Component",
              &QuarryComponent_Factory, &QuarryComponent_CreateInPlace);
