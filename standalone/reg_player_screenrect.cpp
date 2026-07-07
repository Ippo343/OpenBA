// standalone/reg_player_screenrect.cpp
// Player wave (batch "screenrect") — edittool.cpp deep tier round 2d.
//
// Registers the four-function closure for 0x140100380 (Entity_impl_0380, entity
// screen bounding-rectangle, resolved from code/Game/Player/edittool.cpp
// EditTool_Startup) to the reimplementations in
// code/Game/Player/tool_base_screenrect.cpp. The integrator wires
// RegisterPlayerScreenRectBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Closure (registered bottom-up; every member resolves):
//   0x1400fec70 BuildCorners   -> CharacterComponent_impl_f3e0 (0x14009f3e0, REG)
//   0x140098210 WorldToScreen  -> Math dispatch slots (registered locally below) +
//                                 g_pSingletonExref_151a80 (display) + g_fUVScale
//   0x1400fe960 AccumulateRect -> 0x140098210 + Vector2 Min/Max/*= slots +
//                                 g_pGameSettingsPtr + display
//   0x140100380 Entity_0380    -> 0x1400fec70, 0x1400fe960,
//                                 Entity_ClearPathQueue (0x140003f60, REG),
//                                 FUN_140004680 (0x140004680, REG),
//                                 fn_InternalBasePtr_CheckPtr_slot (engine_funcs) +
//                                 Volume/Vector3/Transform/Box dispatch slots.
//
// Every value-returning Math op is a pure Runtime export decoded from the exe dispatch
// slot name table (slot RVA + 2-byte hint -> mangled string). Verdict: REGISTER.
// Member struct-return ABI (RCX=this, RDX=&retbuf, R8.. = args) verified from disasm.
//
// The reimpl bodies in tool_base_screenrect.cpp resolve every Math export DIRECTLY via
// ResolveExport, so the chain works even if the .rdata slots are never filled. We ALSO
// register each slot here (a local slot-storage registrar mirroring the file-static
// RegisterDispatchSlot in address_registry.cpp) so any OTHER standalone caller that reads
// these .rdata dispatch slots gets a valid pointer too.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" uint32_t PlayerScreenRect_BuildCorners_ec70(uint64_t, int, int, int, int, float*);
extern "C" uint8_t  PlayerScreenRect_WorldToScreen_098210(uintptr_t, float*, float*);
extern "C" uint8_t  PlayerScreenRect_AccumulateRect_fe960(uint64_t, uint32_t, float*, void*);
extern "C" uint8_t  PlayerScreenRect_Entity_0380(uintptr_t, void*, uint64_t*);

// NOTE: the 14 value-return Math ops (Vector2/3/4, Matrix, Transform3, Box, Rectangle)
// are NOT registered as dispatch slots here. The reimpl bodies in tool_base_screenrect.cpp
// resolve every one DIRECTLY via ResolveExport (EnsureScreenRectExports), so the chain is
// self-contained without touching the .rdata slots. Registering the slots globally was
// dropped on purpose: slot 0x140152780 (verifiably Vector2::Max) is MISLABELED as
// "iatMemAlloc" in code/Game/Work/harvestarea.cpp + plantarea.cpp — registering it would
// make their ResolveAddr(0x140152780) return Vector2::Max and be called as MemAlloc
// (a latent Work-batch bug; flagged for a separate fix). Leaving the slots unregistered
// keeps those Work paths at their current safe no-op.

void RegisterPlayerScreenRectBatch() {
    // Code-address reimplementations (registered bottom-up: leaves first). The Math
    // exports they call resolve at runtime via ResolveExport (see tool_base_screenrect.cpp).
    RegisterExeAddr(0x1400fec70, reinterpret_cast<uintptr_t>(&PlayerScreenRect_BuildCorners_ec70));
    RegisterExeAddr(0x140098210, reinterpret_cast<uintptr_t>(&PlayerScreenRect_WorldToScreen_098210));
    RegisterExeAddr(0x1400fe960, reinterpret_cast<uintptr_t>(&PlayerScreenRect_AccumulateRect_fe960));
    RegisterExeAddr(0x140100380, reinterpret_cast<uintptr_t>(&PlayerScreenRect_Entity_0380));
}
#endif // STANDALONE_EXE
