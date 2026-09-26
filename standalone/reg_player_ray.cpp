// standalone/reg_player_ray.cpp
// Wave 5 (Player) round 2 — ray / screen-to-cell sub-batch registration.
//
// Registers the exe addresses of the Player terrain-raycast chain (resolved from
// several code/Game/Player/*.cpp call sites) to their reimplementations in
// code/Game/Player/tool_base_ray.cpp. The integrator wires RegisterPlayerRayBatch()
// into RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Closure (all satisfied, registered bottom-up):
//   0x1400aa530 TerrainMarch  -> {Vector2::Normalize slot 0x1401526b0 (reg),
//                                 QuarryComponent_helper_aa20 0x1400aaa20 (reg),
//                                 g_fUVScale, DAT_1401722c0 sign mask}
//   0x1401005a0 GetMouseRay   -> {KeyMap exref (g_pInputConfigSingletonExref), VideoDevice
//                                 singleton (g_pVideoDeviceSingleton), fn_KeyMap_GetValue,
//                                 fn_Camera_GetFrustum, + 4 Runtime exports resolved
//                                 locally and registered as dispatch slots below}
//   0x140101270 ScreenToCell  -> {0x1401005a0 (this batch), 0x1400aa530 (this batch)}
//   0x1400fda00 PlacementPos  -> {0x140101270 (this batch),
//                                 CharacterComponent_impl_f3e0 0x14009f3e0 (reg),
//                                 fn_InternalBasePtr_CheckPtr, DAT_140172204/172114}
//
// Safety: every code address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
#include "resolve.h"
#include <cstdint>

extern "C" uint8_t PlayerRay_TerrainMarch_aa530(uintptr_t, float*, float*, float*, float*, float*);
extern "C" uint8_t PlayerRay_GetMouseRay_1005a0(void*, float*, float*);
extern "C" int     PlayerRay_ScreenToCell_101270(uintptr_t);
extern "C" uint8_t PlayerRay_PlacementPosCalc_fda00(uintptr_t, int*, int*, int*, int*, float*, float*);

// NOTE on GetMouseRay (1005a0)'s four Math/Volume member functions:
//   ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z          (Vector3(const Scalar&))  slot 0x140151320
//   ??DTransform3@Math@@QEBA?AVVector3@1@AEBV21@@Z (Transform3::operator*)   slot 0x140152908
//   ?GetScreenDirection@Frustum@Volume@@QEBA?AVVector3@Math@@MM@Z           slot 0x140152918
//   ?Rotate@Transform3@Math@@QEBA?AVVector3@2@AEBV32@@Z (Transform3::Rotate) slot 0x140151860
// These are pure Runtime exports (member functions returning Math::Vector3 by value:
// RCX=this, RDX=&retbuf, R8/XMM2.. = args). The reimpl body resolves them DIRECTLY via
// ResolveExport (tool_base_ray.cpp EnsureRayExports), so no slot registration is needed
// for this chain. If any OTHER standalone caller reads those four .rdata dispatch slots,
// the lead may optionally add RegisterDispatchSlot(slot, ResolveExport(name)) for each in
// address_registry.cpp (RegisterDispatchSlot is file-static there).

void RegisterPlayerRayBatch() {
    // Code-address reimplementations (registered bottom-up: leaves first).
    RegisterExeAddr(0x1400aa530, reinterpret_cast<uintptr_t>(&PlayerRay_TerrainMarch_aa530));
    RegisterExeAddr(0x1401005a0, reinterpret_cast<uintptr_t>(&PlayerRay_GetMouseRay_1005a0));
    RegisterExeAddr(0x140101270, reinterpret_cast<uintptr_t>(&PlayerRay_ScreenToCell_101270));
    RegisterExeAddr(0x1400fda00, reinterpret_cast<uintptr_t>(&PlayerRay_PlacementPosCalc_fda00));
}
