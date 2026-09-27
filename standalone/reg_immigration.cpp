// standalone/reg_immigration.cpp
// Immigrants wave (2026-09-05): the town hall's nomad-arrival state machine
// (immigrationcomponent_states.cpp) and the per-nomad state machine
// (nomadcomponent_states.cpp). Every address below is a retail function whose body
// is now ported; the state handlers are reached through the controller state tables,
// the helpers through these registrations.
#include "resolve.h"
#include <cstdint>

extern "C" bool  Immigration_CanAccept(void* comp);                                   // 0x14004e3f0
extern "C" int   Immigration_FindSpawnCell(void* comp, int x, int y, int hallCell);   // 0x14004e270
extern "C" void  Immigration_Spawn(void* comp);                                       // 0x14004dbe0
extern "C" void  Immigration_Dismiss(void* comp);                                     // 0x14004d300
extern "C" void  Immigration_Accept(void* comp);                                      // 0x14004d450
extern "C" void  Nomad_Init(void* nomad, void* ownerIBP, const float* spawn, const float* meet); // 0x140050030
extern "C" void* Nomad_ConvertToCitizen(void* nomad, uint64_t* outIBP, uint64_t* parentIBP, int gender, char isChild); // 0x1400501a0
extern "C" void  Imm_RequestStateTransition_9ac0(void* sm, int stateId);              // 0x1400a9ac0
extern "C" char  Imm_State2_Enter(void*, void*);            // 0x14004d290
extern "C" char  Imm_State2_Tick(void*, void*, void*);      // 0x14004d220
extern "C" char  Imm_StateF_Enter(void*, void*);            // 0x14004d1a0
extern "C" char  Imm_StateF_Tick(void*, void*, void*);      // 0x14004d0d0
extern "C" char  Imm_State10_Enter(void*, void*);           // 0x14004d090
extern "C" char  Imm_State10_Tick(void*, void*, void*);     // 0x14004d070
extern "C" char  Imm_State10_Event6_Arrived(void*, void*);  // 0x14004cff0
extern "C" void  Imm_Trans3_Dismiss(void*);                 // 0x14004cd00
extern "C" char  Imm_State11_Enter(void*, void*);           // 0x14004ceb0
extern "C" char  Imm_State11_Tick(void*, void*, void*);     // 0x14004ce00
extern "C" char  Imm_Event7_Allow(void*, void*);            // 0x14004cd90
extern "C" char  Imm_Event8_Deny(void*, void*);             // 0x14004cd20

extern "C" void  Nomad_Remove(void* entityList, void* entityIBP);   // 0x1400c49d0
extern "C" void  Nomad_MoveTo(void* nomad, float* dest);            // 0x14004fb60
extern "C" char  Nomad_State0_Enter(void*, void*);          // 0x14004faf0
extern "C" char  Nomad_State0_Tick(void*, void*, void*);    // 0x14004f9c0
extern "C" char  Nomad_Event1_Leave(void*, void*);          // 0x14004f4a0
extern "C" char  Nomad_State1_Tick(void*, void*, void*);    // 0x14004f600
extern "C" char  Nomad_Event0_Destroy(void*, void*);        // 0x14004f420
extern "C" char  Nomad_State2_Enter(void*, void*);          // 0x14004f5e0
extern "C" char  Nomad_State2_Tick(void*, void*, void*);    // 0x14004f530

void RegisterImmigrationWave() {
    RegisterExeAddr(0x1400a9ac0, reinterpret_cast<uintptr_t>(&Imm_RequestStateTransition_9ac0));

}
