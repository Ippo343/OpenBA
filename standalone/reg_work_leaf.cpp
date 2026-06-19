// standalone/reg_work_leaf.cpp
// Registration for the Work-gap LEAF wave — closure-safe per-class job helpers ported by the
// 4-batch opus fan-out (bodies in code/Game/Work/work_leaf_{clear,market,gather,hunt}.cpp).
//
// Each address below was decompiled, closure-checked against notes/_reg_snapshot.txt, and the
// port lead-verified offset-by-offset against the Ghidra disassembly before registration.
// The DEFERRED addresses in the same files (action-construction ctors, pathfinder/copy-ctor
// closures, the MarketWork Slot4 + caller-bug BST block) are NOT registered — their exe
// sub-callee closures are incomplete, so a port would build a half-null/drifted structure
// (silent corruption, strictly worse than the safe no-op skip). See the batch reports.
//
// Guarded by STANDALONE_EXE — in DLL mode these addresses resolve to the original exe code.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include <cstdint>

// ---- work_leaf_clear.cpp (cleararea / harvestarea / plantarea) ----
void  WorkLeaf_BstRemoveByIBP_298a0(void* treeRootSlot, void* keyIBP);
void* WorkLeaf_BstInsertByIBP_29920(void* treeRootSlot, void* outNode, void* insIBP);
void  WorkLeaf_BstSerializeIBPpair_29c10(void* treeRootSlot, void* ser);
void  WorkLeaf_SortTilesByDistance_9d7c0(int* mapGrid, void* tileArray, int refCell);
void  WorkLeaf_GetTilesInArea_9fc00(int* grid, int x, int y, int w, int h, void* outArr, char collectEmpty);

// ---- work_leaf_market.cpp (marketwork / tradework) ----
void  WorkLeaf_MarketTreeCleanup_2af40(void* ctx, void* node);
void  WorkLeaf_TradeTreeCleanup_14b00(void* ctx, void* node);

// ---- work_leaf_gather.cpp (gatherrawmaterials / foresterwork) ----
int       WorkLeaf_FreeStorageCapacity_116540(intptr_t self);
void*     WorkLeaf_FindGrowthDescription_c3c0(intptr_t entity, void* retOut);
long long WorkLeaf_FootprintOwnerCheck_120c30(intptr_t self, int col, int row);

// ---- work_leaf_hunt.cpp (huntwork / harvestorchard / pasturework / consumeproduce) ----
void* WorkLeaf_FindComp_a730(uintptr_t entity, void* retIBP);
void* WorkLeaf_FindComp_a0b0(uintptr_t entity, void* retIBP);
void* WorkLeaf_FindComp_4fe0(uintptr_t entity, void* retIBP);
void* WorkLeaf_FindComp_63d0(uintptr_t entity, void** retval);
bool  WorkLeaf_ConsumeProduce_93a0(uintptr_t self);
int   WorkLeaf_Pasture_8350(uintptr_t self);
void  WorkLeaf_ConsumeProduce_b310(uintptr_t self);

void RegisterWorkLeafBatch() {
    // clear
    RegisterExeAddr(0x1400298a0, reinterpret_cast<uintptr_t>(&WorkLeaf_BstRemoveByIBP_298a0));
    RegisterExeAddr(0x140029920, reinterpret_cast<uintptr_t>(&WorkLeaf_BstInsertByIBP_29920));
    RegisterExeAddr(0x140029c10, reinterpret_cast<uintptr_t>(&WorkLeaf_BstSerializeIBPpair_29c10));
    RegisterExeAddr(0x14009d7c0, reinterpret_cast<uintptr_t>(&WorkLeaf_SortTilesByDistance_9d7c0));
    RegisterExeAddr(0x14009fc00, reinterpret_cast<uintptr_t>(&WorkLeaf_GetTilesInArea_9fc00));
    // market
    RegisterExeAddr(0x14002af40, reinterpret_cast<uintptr_t>(&WorkLeaf_MarketTreeCleanup_2af40));
    RegisterExeAddr(0x140014b00, reinterpret_cast<uintptr_t>(&WorkLeaf_TradeTreeCleanup_14b00));
    // gather
    RegisterExeAddr(0x140116540, reinterpret_cast<uintptr_t>(&WorkLeaf_FreeStorageCapacity_116540));
    RegisterExeAddr(0x14000c3c0, reinterpret_cast<uintptr_t>(&WorkLeaf_FindGrowthDescription_c3c0));
    RegisterExeAddr(0x140120c30, reinterpret_cast<uintptr_t>(&WorkLeaf_FootprintOwnerCheck_120c30));
    // hunt
    RegisterExeAddr(0x14002a730, reinterpret_cast<uintptr_t>(&WorkLeaf_FindComp_a730));
    RegisterExeAddr(0x14000a0b0, reinterpret_cast<uintptr_t>(&WorkLeaf_FindComp_a0b0));
    RegisterExeAddr(0x140004fe0, reinterpret_cast<uintptr_t>(&WorkLeaf_FindComp_4fe0));
    RegisterExeAddr(0x1400063d0, reinterpret_cast<uintptr_t>(&WorkLeaf_FindComp_63d0));
    RegisterExeAddr(0x140128350, reinterpret_cast<uintptr_t>(&WorkLeaf_Pasture_8350));
    RegisterExeAddr(0x1401193a0, reinterpret_cast<uintptr_t>(&WorkLeaf_ConsumeProduce_93a0));
    RegisterExeAddr(0x14012b310, reinterpret_cast<uintptr_t>(&WorkLeaf_ConsumeProduce_b310));
}

#endif // STANDALONE_EXE
