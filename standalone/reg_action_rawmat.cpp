// standalone/reg_action_rawmat.cpp
// Wave 3 (Action) — raw-material cluster (batch E) registration (closure-safe leaves).
//
// Registers the six closure-safe leaf sub-helpers referenced from
// code/Game/Action/{consumerawmaterial,pickuprawmaterial,pickupstoredrawmaterial,
// createrawmaterial}.cpp to their reimplementations in
// code/Game/Action/action_base_rawmat.cpp. The integrator wires
// RegisterActionRawMatBatch() into RegisterCodeFunctions() in address_registry.cpp.
//
// Safety: every address is a real function ENTRY (Ghidra Entry == address) and a
// faithful 1:1 port whose closure is satisfied — pure array ops (d150), MemAlloc/MemFree
// (d000/da70/44810), or engine_funcs export-resolved pointers (88f90 IsDerivedFrom;
// 223c0 CheckPtr/IsDerivedFrom/IBP_Ctor).
//
// DEFERRED (NOT registered — deeper/unregistered closure, see action_base_rawmat.cpp):
//   0x14009bd40 FindPlacementCell (intricate spiral; own pass),
//   0x140129760 PositionEntity (Geometry render-state mutation + Entity_ClearPathQueue dep),
//   0x140088ef0 RegisterRawMaterial (deps FUN_140013320 + CreateRawMaterial_helper_8fd0 unreg).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void  ActRm_StorageSubtract_d150(uintptr_t, int64_t, int);        // 0x14012d150
extern void  ActRm_StorageAdd_d000(uintptr_t, int64_t, int);             // 0x14012d000
extern void  ActRm_ArrayResize16_da70(int64_t*, int);                    // 0x14000da70
extern void  ActRm_DynArrayPush4_44810(uintptr_t, uint32_t);             // 0x140044810
extern int   ActRm_GetResourceCategory_88f90(void*, uintptr_t);          // 0x140088f90
extern void* ActRm_FindComponent_223c0(uintptr_t, void*);                // 0x1400223c0

void RegisterActionRawMatBatch() {
    RegisterExeAddr(0x14012d150, reinterpret_cast<uintptr_t>(&ActRm_StorageSubtract_d150));
    RegisterExeAddr(0x14012d000, reinterpret_cast<uintptr_t>(&ActRm_StorageAdd_d000));
    RegisterExeAddr(0x14000da70, reinterpret_cast<uintptr_t>(&ActRm_ArrayResize16_da70));
    RegisterExeAddr(0x140044810, reinterpret_cast<uintptr_t>(&ActRm_DynArrayPush4_44810));
    RegisterExeAddr(0x140088f90, reinterpret_cast<uintptr_t>(&ActRm_GetResourceCategory_88f90));
    RegisterExeAddr(0x1400223c0, reinterpret_cast<uintptr_t>(&ActRm_FindComponent_223c0));
}
#endif
