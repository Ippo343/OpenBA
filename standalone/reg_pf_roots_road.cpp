// standalone/reg_pf_roots_road.cpp
// Wave 3 (Action) — ROAD-edit ROOT callers registration. Maps the four top-level
// road/zone mutation entry points to their faithful 1:1 ports in
// code/Game/Action/action_pf_roots_road.cpp. The integrator wires
// RegisterPfRootsRoadBatch() into RegisterCodeFunctions() in address_registry.cpp.
//
// Closure: all sub-callees registered elsewhere (9c170/a6fe0 PF A* batch; a8c90/ab150
// PF road batch; 9f3e0 move/CNR; 1a6e0 Work BST) EXCEPT 0x1400af370 (road-mesh
// rebuild) which is DEFERRED — the ports call it via ResolveAddr so it is a safe no-op
// until its own pass registers it.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void Pf_ZoneRefresh_9c480(int*, void*, void*, int);                 // 0x14009c480
extern void Pf_SetTileRoadFlag_9f2f0(int*, int, uint32_t);                 // 0x14009f2f0
extern void Pf_RefreshRegionHeight_9e0e0(uint32_t*, int, int, int, int);   // 0x14009e0e0
extern void Pf_FinalizeRoadTile_9c640(uint32_t*, int, uint8_t, uint32_t);  // 0x14009c640

void RegisterPfRootsRoadBatch() {
    RegisterExeAddr(0x14009c480, reinterpret_cast<uintptr_t>(&Pf_ZoneRefresh_9c480));
    RegisterExeAddr(0x14009f2f0, reinterpret_cast<uintptr_t>(&Pf_SetTileRoadFlag_9f2f0));
    RegisterExeAddr(0x14009e0e0, reinterpret_cast<uintptr_t>(&Pf_RefreshRegionHeight_9e0e0));
    RegisterExeAddr(0x14009c640, reinterpret_cast<uintptr_t>(&Pf_FinalizeRoadTile_9c640));
}
#endif
