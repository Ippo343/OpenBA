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

// ENDGAME B.3.8 CIT: the 16-slot copy of the retail vtable that used to live here
// (g_TornadoComponent_Vtable, registered at 0x14015aa80) is gone. TornadoComponent
// is a real class with a compiler vtable (code/Game/Component/TornadoComponent.h),
// and nothing in the tree reads the retail table address back (B38 D5). The rows
// for its three overridden slots (0x140058b80 / 0x1400585a0 / 0x140057cd0) went too
// (caller batch CB5): nothing read those addresses back either.

extern "C" void RegisterTornadoLane() {

    // --- leaves owned by other classes that had no port yet ---
    // 0x14000cf40 is the Vector3 sibling of the already-registered 0x14000d030
    // resize; without it every waypoint push is dropped and the path stays empty.
    // ParticleComponent / AudioComponent leaves. First port of each — grepped for
    // 0x140053720 and 0x140042960 across the tree before writing them, so this is
    // not the reference_one_retail_fn_ported_twice_diverges situation.
    RegisterExeAddr(0x140053720, reinterpret_cast<uintptr_t>(&Tornado_ParticleReverse_53720));
    RegisterExeAddr(0x140042960, reinterpret_cast<uintptr_t>(&Tornado_AudioMoveEmitters_42960));
}

// ENDGAME B.3.9 COORD-0: the ParticleComponent type-id cell 0x1401c4720 (its
// storage, registration and FillTypeIdGlobals_tornado) is gone. Its one reader,
// the shared finder behind 0x140005200, reads ParticleComponent::TypeId()
// (code/Game/Component/naturalresourcecomponent.cpp); the history of the
// 2026-08-22 fire-arm crash it fixed is in git.
