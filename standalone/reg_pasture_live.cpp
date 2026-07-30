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
//   0x1401277e0  PastureComponent slot 7 (release)   -> Pasture_ReleaseAllAnimals
//   0x140123c70  LivestockComponent_OnAdd
//   0x140123c00  LivestockComponent_OnRemove
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

struct PastureComponent;
struct LivestockComponent;

extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP);
extern "C" int  Pasture_FillAllPastures();
extern "C" void Pasture_BindAnimal(void* animalComp, void* pastureIBP);
extern "C" void Pasture_AdoptWanderers(void* pasture);
extern "C" void Pasture_Reproduce(void* pasture);
extern "C" void Pasture_ReleaseAllAnimals(void* pasture);
extern "C" char Pasture_State2Enter(void* data, void* sm);
extern "C" char Pasture_State3Enter(void* data, void* sm);
extern "C" char Pasture_State2PerFrame(void* data, void* sm, void* time);

void LivestockComponent_OnAdd(LivestockComponent* self);
void LivestockComponent_OnRemove(LivestockComponent* self);

// 0x140172204 (.rdata) = 90.0f — the quarter-turn CreateRawMaterial_helper_9760
// rotates a dropped produce pile by. Nothing else in standalone registers it.
static float g_f_QuarterTurn = 90.0f;

void RegisterPastureLive() {
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_PASTURE")) return;

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
    RegisterExeAddr(0x1401277e0, reinterpret_cast<uintptr_t>(&Pasture_ReleaseAllAnimals));

    // Livestock lifecycle
    RegisterExeAddr(0x140123c70, reinterpret_cast<uintptr_t>(&LivestockComponent_OnAdd));
    RegisterExeAddr(0x140123c00, reinterpret_cast<uintptr_t>(&LivestockComponent_OnRemove));
}
