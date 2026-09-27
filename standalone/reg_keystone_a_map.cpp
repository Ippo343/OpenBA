// standalone/reg_keystone_a_map.cpp
// Registration for the KEYSTONE-A MAP batch (code/Game/Component/keystone_a_map.cpp).
//
// Maps each ported exe helper's Ghidra address to our reimplementation so that, in
// standalone mode, ResolveAddr(Module::Exe, 0xADDR) returns our function instead of 0.
//
// Closure: every exe sub-callee of these five is already registered elsewhere
//   0x14009f3e0 GridToWorld (mapquery), 0x1400a4d30 (reg_component_visuals),
//   0x1400c3470 / 0x1400c2ea0 / 0x140003110 (reg_action_hunt),
//   0x14009f2f0 (reg_pf_roots_road) — or is one of the five here (e2c0).
// The externals (IsDerivedFrom / CheckPtr / InternalBasePtr::Ctor slots) come from
// engine_funcs and need no exe registration.
//


#include "resolve.h"
#include <cstdint>

// Forward declarations of the ported helpers (defined in code/Game/Component/keystone_a_map.cpp).
void MapComponent_sub_41b0(int64_t param_1);
void CreateNaturalResource_impl_4750(int64_t param_1);

void RegisterKeystoneAMapBatch() {
    RegisterExeAddr(0x1401241b0, reinterpret_cast<uintptr_t>(&MapComponent_sub_41b0));
    // 0x140124750 is ported twice and the two bodies DIVERGE: this file's
    // CreateNaturalResource_impl_4750 is the strict transcription (IsDerivedFrom guard +
    // the registered dee0), while mapcomponent.cpp's MapComponent_DetachOccupancy_4750
    // deliberately drops the IsDerivedFrom guard (it rejects valid descriptions in
    // standalone's partial factory tree) and routes the grid write through
    // StartTown_UnmarkResourceCellGrid, which is kept in lockstep with the spawn-side
    // mark. address_registry.cpp registers that one later, so it is what every soak run
    // has actually exercised — it stays the owner. Duplicate registration removed here
    // 2026-08-30; the body above is retained as the reference transcription.
}

