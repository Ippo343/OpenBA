// Animal spawn wave — Pasture/Livestock live bring-up (cows/sheep/chickens).
// OWNED BY the pasture bring-up lane: every RegisterExeAddr this lane needs lives
// HERE (code in pasturecomponent*.cpp / livestockcomponent.cpp). Shared type-id
// globals are published by reg_component_animal.cpp — do NOT re-register them.
// Hook is already wired: address_registry.cpp calls RegisterPastureLive().
//
// Registered here (all gated on BANISHED_LIVE_PASTURE):
//   0x140128670  Game_helper_8670                    -> Pasture_SpawnLivestock
//   0x140060290  Game_SpawnItemsInStorage (MISNAMED, = debug "fill pastures")
//                                                    -> Pasture_FillAllPastures
//   0x140123cc0  PastureComponent_Slot14_sub_3cc0    -> Pasture_BindAnimal
//   0x140127140  PastureDescription_helper_sub_7140  -> Pasture_AdoptWanderers
//   0x140127370  PastureDescription_helper_7370      -> Pasture_Reproduce
//   0x1401270c0  PastureDescription_helper_70c0      -> Pasture_State2Enter
//   0x140126e40  PastureDescription_helper_6e40      -> Pasture_State3Enter
//   0x1401267e0  ..._helper_81a0_sub_67e0            -> Pasture_State2PerFrame
//
// PastureComponent slots 7/13/14 and LivestockComponent slots 6/7 are real-class
// virtuals since ENDGAME B.3.8; nothing read their addresses back, so their rows
// are gone (slot 15 OnMessage keeps its row: pasture_harness.cpp resolves it).
//
// Already registered elsewhere and deliberately NOT duplicated here:
//   0x140128570 / 0x140105ed0 / 0x1401240a0  (reg_component_nature.cpp)
//   0x140105e20                              (gap_D)
//   0x140004680 0x1400c3470 0x1400012d0 0x140002cc0 0x140003110 0x1401189b0
//   0x1400032e0 0x140002340 0x1400c48a0 0x140005500 0x140003050 0x140006330
//   0x140013320 0x140088f90 0x140044810 0x140123070 0x1400223c0 0x1400a6fe0
//   0x1400a8c90 0x1400c3720                 (common-helper waves)
//   type ids 0x1401c7970 / 0x1401c79a8 / 0x1401c6f98 / float 0x140172220
//                                           (reg_component_animal.cpp)
//   pasture ids 0x1401c7ba0 / 0x1401c7bd8, 0x1401c6fd0
//                                           (reg_work_tail_serialize.cpp)
#include "resolve.h"
#include "Component/component_factory_base.h"
#include <cstdint>

extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP);
extern "C" int  Pasture_FillAllPastures();
extern "C" void Pasture_BindAnimal(void* animalComp, void* pastureIBP);
extern "C" void Pasture_AdoptWanderers(void* pasture);
extern "C" void Pasture_Reproduce(void* pasture);
extern "C" char Pasture_State2Enter(void* data, void* sm);
extern "C" char Pasture_State3Enter(void* data, void* sm);
extern "C" char Pasture_State2PerFrame(void* data, void* sm, void* time);
extern "C" void Pasture_MsgReceive(void* pasture, int msg);
extern "C" void* Pasture_FindLivestockComp(int64_t entity, void* retIBP);
extern "C" uint64_t MapGrid_StampCellRefs_9f770(void* grid, int x, int y, int w, int h,
                                                void* ref);

// 0x140172204 (.rdata) = 90.0f — the quarter-turn CreateRawMaterial_helper_9760
// rotates a dropped produce pile by. Nothing else in standalone registers it.
static float g_f_QuarterTurn = 90.0f;

void RegisterPastureLive() {
    RegisterExeAddr(0x140172204, reinterpret_cast<uintptr_t>(&g_f_QuarterTurn));

    // Spawn / stocking
    RegisterExeAddr(0x140128670, reinterpret_cast<uintptr_t>(&Pasture_SpawnLivestock));
    RegisterExeAddr(0x140060290, reinterpret_cast<uintptr_t>(&Pasture_FillAllPastures));
    RegisterExeAddr(0x140123cc0, reinterpret_cast<uintptr_t>(&Pasture_BindAnimal));

    // Controller state machine
    RegisterExeAddr(0x140127140, reinterpret_cast<uintptr_t>(&Pasture_AdoptWanderers));
    RegisterExeAddr(0x140127370, reinterpret_cast<uintptr_t>(&Pasture_Reproduce));
    RegisterExeAddr(0x1401270c0, reinterpret_cast<uintptr_t>(&Pasture_State2Enter));
    RegisterExeAddr(0x140126e40, reinterpret_cast<uintptr_t>(&Pasture_State3Enter));
    RegisterExeAddr(0x1401267e0, reinterpret_cast<uintptr_t>(&Pasture_State2PerFrame));
    // Slot 15 OnMessage — the breed tag (herder work-complete -> random animal
    // gets flag bit 23; sole setter of the bit in the exe).
    RegisterExeAddr(0x140127720, reinterpret_cast<uintptr_t>(&Pasture_MsgReceive));
    // Map-cell footprint stamp (shared leaf; also called by HarvestOrchard_impl_0800).
    // Un-no-ops the state-2 per-frame footprint stamp (deferred since the animal wave).
    RegisterExeAddr(0x14009f770, reinterpret_cast<uintptr_t>(&MapGrid_StampCellRefs_9f770));
    // LivestockComponent finder (FUN_14002f1d0) — previously unregistered, which
    // silently skipped the spawn-path bind for every spawned/born livestock.
    RegisterExeAddr(0x14002f1d0, reinterpret_cast<uintptr_t>(&Pasture_FindLivestockComp));
}
