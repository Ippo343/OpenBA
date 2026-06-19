// standalone/reg_action_cnr.cpp
// Wave 3 (Action) — CreateNaturalResource / AnimateNaturalResource /
// RemoveNaturalResource shared exe sub-helper registration.
//
// Registers the exe addresses of the NR action helpers (resolved from the
// per-class Action .cpp call sites) to their reimplementations in
// code/Game/Action/action_base_cnr.cpp. The integrator wires
// RegisterActionCnrBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// FindComponent family (member ABI: RCX=entity, RDX=retIBP) -> void*(uintptr_t, void*)
extern void* ActCNR_FindComponent_3f60(uintptr_t, void*);   // 0x140003f60 type 0x1401c7a50
extern void* ActCNR_FindComponent_c460(uintptr_t, void*);   // 0x14000c460 type 0x1401c77b0

// Cell terrain availability check -> uint64_t(void*, int)  (low byte = bool)
extern uint64_t ActCNR_CheckTerrain_cb00(void*, int);       // 0x14009cb00

// GrowthComponent::SetGrowthProgress(progress) -> void(int64_t, float)
extern void ActCNR_SetGrowthProgress_120db0(int64_t, float); // 0x140120db0

// NaturalResource variant/sub-variant accessors + setter
extern uint32_t ActCNR_NR_GetVariantCount_1252f0(int64_t);        // 0x1401252f0
extern uint32_t ActCNR_NR_GetSubVariantCount_125320(int64_t, int);// 0x140125320
extern void     ActCNR_NR_SetVariant_1253d0(int64_t*, uint16_t, int16_t); // 0x1401253d0

// AnimateNaturalResource SetAnimIndex -> void(int64_t, uint32_t)
extern void ActCNR_SetAnimIndex_126440(int64_t, uint32_t);  // 0x140126440

// CreateNaturalResource model placement -> uint64_t(int64_t, int)
extern uint64_t ActCNR_SetupModelAtCell_126540(int64_t, int); // 0x140126540

// RemoveNaturalResource core removal/harvest -> void(int64_t, void*, char)
extern void ActCNR_RemoveNatRes_1260f0(int64_t, void*, char); // 0x1401260f0

void RegisterActionCnrBatch() {
    RegisterExeAddr(0x140003f60, reinterpret_cast<uintptr_t>(&ActCNR_FindComponent_3f60));
    RegisterExeAddr(0x14000c460, reinterpret_cast<uintptr_t>(&ActCNR_FindComponent_c460));
    RegisterExeAddr(0x14009cb00, reinterpret_cast<uintptr_t>(&ActCNR_CheckTerrain_cb00));
    RegisterExeAddr(0x140120db0, reinterpret_cast<uintptr_t>(&ActCNR_SetGrowthProgress_120db0));
    RegisterExeAddr(0x1401252f0, reinterpret_cast<uintptr_t>(&ActCNR_NR_GetVariantCount_1252f0));
    RegisterExeAddr(0x140125320, reinterpret_cast<uintptr_t>(&ActCNR_NR_GetSubVariantCount_125320));
    RegisterExeAddr(0x1401253d0, reinterpret_cast<uintptr_t>(&ActCNR_NR_SetVariant_1253d0));
    RegisterExeAddr(0x140126440, reinterpret_cast<uintptr_t>(&ActCNR_SetAnimIndex_126440));
    RegisterExeAddr(0x140126540, reinterpret_cast<uintptr_t>(&ActCNR_SetupModelAtCell_126540));
    RegisterExeAddr(0x1401260f0, reinterpret_cast<uintptr_t>(&ActCNR_RemoveNatRes_1260f0));
}
#endif
