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
// Guarded by STANDALONE_EXE — in DLL mode these addresses resolve to the original exe code.


#include "resolve.h"
#include <cstdint>

// Forward declarations of the ported helpers (defined in code/Game/Component/keystone_a_map.cpp).
int  CreateNaturalResource_helper_e2c0(int* param_1, int param_2);
void MapComponent_overlay_dc20(int* param_1, int64_t* param_2, uint8_t param_3,
                               int param_4, int param_5, int param_6, int param_7,
                               int64_t* param_8, int param_9);
void CreateNaturalResource_impl_dee0(int* param_1, void* param_2, int param_3,
                                     int param_4, int param_5, int param_6,
                                     int64_t* param_7, int param_8);
void MapComponent_sub_41b0(int64_t param_1);
void CreateNaturalResource_impl_4750(int64_t param_1);

void RegisterKeystoneAMapBatch() {
    RegisterExeAddr(0x14009e2c0, reinterpret_cast<uintptr_t>(&CreateNaturalResource_helper_e2c0));
    RegisterExeAddr(0x14009dc20, reinterpret_cast<uintptr_t>(&MapComponent_overlay_dc20));
    RegisterExeAddr(0x14009dee0, reinterpret_cast<uintptr_t>(&CreateNaturalResource_impl_dee0));
    RegisterExeAddr(0x1401241b0, reinterpret_cast<uintptr_t>(&MapComponent_sub_41b0));
    RegisterExeAddr(0x140124750, reinterpret_cast<uintptr_t>(&CreateNaturalResource_impl_4750));
}

