// Tornado wave — address registry for the TornadoComponent lane.
//
// Registers the exe addresses of everything ported in
// code/Game/Component/tornadocomponent.cpp, so ResolveAddr(Module::Exe, ...) finds
// our code. The integrator wires
// RegisterTornadoLane() into address_registry.cpp.
//
// The vtable at 0x14015aa80 is 16 slots; only 3, 4 and 8 are overridden in retail
// and the other 13 are base-Component no-ops. Slot indices are from the vtable
// BYTES, not from Ghidra's names — Ghidra calls slot 3 "_Shutdown", slot 4
// "_Serialize" and slot 8 "_OnRemove", and all three are off by one.
//
// Slot 8 (+0x40) is the per-frame tick because the ctor ORs flags 0x84 (bit 2 =
// the full-rate EntityList update-TREE walk). Slot 9 (+0x48) is the throttled
// bucket slot and stays a no-op, exactly as in retail.
//
// NOT registered here: 0x14011e690 / 0x14011e7b0 / 0x14011ea10 (FireComponent
// ignite) belong to the fire lane. tornadocomponent.cpp resolves them and reports
// the building arm as INERT while they are null, rather than skipping silently.

#include "resolve.h"
#include "address_registry.h"
#include <cstdint>

#include "Component/tornadocomponent_funcs.h"
#include "Object/factory_registration.h"
#include <cstdio>
#include "Component/ParticleComponent.h"

// ENDGAME B.3.8 CIT: the 16-slot copy of the retail vtable that used to live here
// (g_TornadoComponent_Vtable, registered at 0x14015aa80) is gone. TornadoComponent
// is a real class with a compiler vtable (code/Game/Component/TornadoComponent.h),
// and nothing in the tree reads the retail table address back (B38 D5). The rows
// for its three overridden slots (0x140058b80 / 0x1400585a0 / 0x140057cd0) went too
// (caller batch CB5): nothing read those addresses back either.

extern uint32_t g_tid_ParticleComponent;

extern "C" void RegisterTornadoLane() {

    // --- leaves owned by other classes that had no port yet ---
    // 0x14000cf40 is the Vector3 sibling of the already-registered 0x14000d030
    // resize; without it every waypoint push is dropped and the path stays empty.
    // ParticleComponent / AudioComponent leaves. First port of each — grepped for
    // 0x140053720 and 0x140042960 across the tree before writing them, so this is
    // not the reference_one_retail_fn_ported_twice_diverges situation.
    RegisterExeAddr(0x140053720, reinterpret_cast<uintptr_t>(&Tornado_ParticleReverse_53720));
    RegisterExeAddr(0x140042960, reinterpret_cast<uintptr_t>(&Tornado_AudioMoveEmitters_42960));

    // --- ParticleComponent type-id .rdata global (see below) ---
    RegisterExeAddr(0x1401c4720, reinterpret_cast<uintptr_t>(&g_tid_ParticleComponent));
}

// ============================================================
// ParticleComponent type-id global — 0x1401c4720
// ============================================================
// ROOT CAUSE OF THE 2026-08-22 INTEGRATED-SOAK CRASH (frame 2400, ACCESS_VIOLATION
// reading 0x18 at GetParticleDesc+0x14, RCX=0).
//
// In the original exe this .data slot is the Object::BaseTypeInfo written by
// ParticleComponent_RegisterType (0x14014b500). In standalone WE provide the
// storage and fill it after Engine::Startup, because Object::GetTypeIdByName
// returns the 0xFFFFFFFF ctor sentinel until Object::Factory::Startup has run.
//
// WHY IT CRASHED. The shared component finder 0x140005200 is registered to
// NaturalResourceComponent_FindComponentByType (reg_component_nature.cpp:127),
// whose body is:
//     s_TypeId_1401c4720 = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c4720);
//     uint32_t wantId = s_TypeId_1401c4720 ? *s_TypeId_1401c4720 : 0;
// With no storage registered, wantId could never equal ParticleComponent's real
// CRC id, so the scan matched NO component on EVERY entity, forever. The chain:
//     DisasterHarness ForceFire -> FireComponent_Ignite_610 (0x140049610)
//       -> finder returns null -> ParticleComponent_StartEmitterFade(NULL)
//         -> GetParticleDesc(NULL) -> deref NULL+0x18 -> boom.
// Retail 0x140049610 derefs that finder result unguarded too, so the fire lane's
// port is FAITHFUL and must not gain a null check — the null was upstream, here.
//
// Same class as reference_null_typeid_global_fakes_ordering_bug and the instance
// documented at reg_component_fruit.cpp:44 ("registered NOWHERE, so the shared
// sibling finder missed on EVERY entity forever").
//
// This ALSO unblocks the tornado lane: TornadoComponent::Update uses the same
// finder for the same purpose and derefs the result exactly as retail does, so
// the tornado would have crashed identically at frame 2700.
//
// NOTE for whoever next touches naturalresourcecomponent.cpp: unlike its sibling
// readers (particlecomponent.cpp's PC_DescTypeId falls back to
// Object::GetTypeIdByName), that finder has NO fallback — it trusts the global or
// yields 0. Registering the global fixes it; giving the finder the same fallback
// would make it robust against a future unregistered id. Not done here: the file
// is outside this lane and the registration is sufficient.
uint32_t g_tid_ParticleComponent = 0;   // storage for 0x1401c4720

// Deferred fill — called from game_main.cpp after Object::Factory::Startup.
// GUARDS THE VALUE, not just the registration (reference_registered_but_filled_with_zero):
// a slot that is registered but filled with 0 or the 0xFFFFFFFF sentinel produces
// the EXACT same silent whole-entity miss as no registration at all, so the value
// is logged once and shouted about if it is unusable.
extern "C" void FillTypeIdGlobals_tornado() {
    g_tid_ParticleComponent = ParticleComponent::TypeId();

    if (g_tid_ParticleComponent == 0 || g_tid_ParticleComponent == 0xFFFFFFFFu) {
        std::fprintf(stderr,
            "[Tornado] *** ParticleComponent type-id global 0x1401c4720 filled with %s "
            "-- the shared finder 0x140005200 will MISS ON EVERY ENTITY and "
            "FireComponent::Ignite / TornadoComponent::Update will fault on the null. "
            "Called too early (before Object::Factory::Startup), or the type was never "
            "registered.\n",
            g_tid_ParticleComponent == 0 ? "0" : "the 0xFFFFFFFF sentinel");
    } else {
        std::fprintf(stderr,
            "[Tornado] ParticleComponent type-id global 0x1401c4720 = 0x%08X (ok)\n",
            g_tid_ParticleComponent);
    }
    std::fflush(stderr);
}
