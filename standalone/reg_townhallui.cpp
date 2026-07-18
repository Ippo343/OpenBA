// standalone/reg_townhallui.cpp
//
// TownHallUI promotion — register the exe helper addresses the town-hall panel's
// reimplementation (code/Game/Dialog/townhallui.cpp) reaches through engine_funcs
// fn_* pointers. Ports live in code/Game/Dialog/townhall_helpers.cpp; the four
// worker-seat strategies are the citizen_employ_cascade.cpp ports (their only exe
// dispatcher is the town-hall profession spinner, 0x14007dd10).
//
// Already registered elsewhere (NOT re-registered here):
//   0x14008d100  RequestWorker/FindAvailableBuilding (reg_town_workplace_registry_reader)
//   0x140013480  AVL rebalance 0x30-nodes            (reg_pf_avl)
//   0x140014890  BST_Insert_0x14                     (reg_toolbar_utility)
//   0x140014c30  BST subtree free                    (reg_work_base)
//   0x14000a590  DynArray_Resize                     (address_registry core)
//   0x1401c3dd8  game-world singleton                (address_registry core)
#include "resolve.h"
#include <cstdint>

extern "C" void TownHall_GroupRatingInit(void*, void*);                       // 0x14006abf0
extern "C" void TownHall_GroupRatingUpdate(void*, float, float);              // 0x14006aed0
extern "C" void TownHall_ProfArrayPush(void*, int, void*);                    // 0x140015800
extern "C" void TownHall_ProdArrayPush(void*, int, void*);                    // 0x1400159b0
extern "C" void TownHall_AvlRebalance28(void*, void*);                        // 0x1400149a0
extern "C" int  TownHall_QueryProductionStat(void*, int, int, int);           // 0x140090f20
extern "C" void TownHall_InventorySortAsc(void*, int, void*, void*);          // 0x140015200
extern "C" void TownHall_InventorySortDesc(void*, int, void*, void*);         // 0x140015500
extern "C" void TownHall_RemoveWorker(void*, uint64_t);                       // 0x140091180
extern "C" void* CitizenEmploy_Strat_cbf0(int64_t, void*, int64_t*, int64_t); // 0x14008cbf0
extern "C" void* CitizenEmploy_Strat_ca20(int64_t, void*, int64_t*, int64_t); // 0x14008ca20
extern "C" void* CitizenEmploy_Strat_c5f0(int64_t, void*, int64_t*, int64_t); // 0x14008c5f0
extern "C" void* CitizenEmploy_Strat_c4b0(int64_t, void*, int64_t*, int64_t); // 0x14008c4b0

void RegisterTownHallBatch() {
    RegisterExeAddr(0x14006abf0, reinterpret_cast<uintptr_t>(&TownHall_GroupRatingInit));
    RegisterExeAddr(0x14006aed0, reinterpret_cast<uintptr_t>(&TownHall_GroupRatingUpdate));
    RegisterExeAddr(0x140015800, reinterpret_cast<uintptr_t>(&TownHall_ProfArrayPush));
    RegisterExeAddr(0x1400159b0, reinterpret_cast<uintptr_t>(&TownHall_ProdArrayPush));
    RegisterExeAddr(0x1400149a0, reinterpret_cast<uintptr_t>(&TownHall_AvlRebalance28));
    RegisterExeAddr(0x140090f20, reinterpret_cast<uintptr_t>(&TownHall_QueryProductionStat));
    RegisterExeAddr(0x140015200, reinterpret_cast<uintptr_t>(&TownHall_InventorySortAsc));
    RegisterExeAddr(0x140015500, reinterpret_cast<uintptr_t>(&TownHall_InventorySortDesc));
    RegisterExeAddr(0x140091180, reinterpret_cast<uintptr_t>(&TownHall_RemoveWorker));
    RegisterExeAddr(0x14008cbf0, reinterpret_cast<uintptr_t>(&CitizenEmploy_Strat_cbf0));
    RegisterExeAddr(0x14008ca20, reinterpret_cast<uintptr_t>(&CitizenEmploy_Strat_ca20));
    RegisterExeAddr(0x14008c5f0, reinterpret_cast<uintptr_t>(&CitizenEmploy_Strat_c5f0));
    RegisterExeAddr(0x14008c4b0, reinterpret_cast<uintptr_t>(&CitizenEmploy_Strat_c4b0));
}
