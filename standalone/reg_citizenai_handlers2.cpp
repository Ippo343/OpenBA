// standalone/reg_citizenai_handlers2.cpp
// Phase 3 (Keystone D) — Brick 3b handler wave (part 2) registration.
//
// Registers the three closure-ready CitizenAIComponent state handlers ported in
// code/Game/Component/citizenai_handlers2.cpp to their Ghidra exe addresses. The lead
// wires RegisterCitizenAIHandlers2() into RegisterCodeFunctions() in
// standalone/address_registry.cpp AFTER Ghidra-verification + A/B.
//
// Handlers registered (every exe sub-callee already registered — grep-verified):
//   0x14010c360  CitizenAI_State_Education      (case 3)
//   0x14010c580  CitizenAI_State_SeekHappiness  (case 8)
//   0x14010d560  CitizenAI_State_FindWork       (case 10)
//
// NO new rdata float / type-id globals are registered here: DAT_1401721fc is read via
// ResolveAddr in the port, and the two type-id globals (DAT_1401c7938 "InteractDescription",
// DWORD_1401c7190 "CitizenDescription") are resolved self-contained via
// Object::GetTypeIdByName (the "Brick" pattern), so they need NO .rdata registration.
//
// Closure (all already registered — verified by grep of standalone/*.cpp):
//   0x14008dd30 CitizenAI_FindBestTarget_dd30   (reg_citizenai_findtarget.cpp)
//   0x140087fb0 Pf_EntityAnchorCell_7fb0        (reg_pf_road.cpp)
//   0x1400bccb0 PerformWork_Action_Ctor10       (reg_citizenai_actionctors.cpp)
//   0x1400bd070 PerformWork_Action_Ctor5        (reg_citizenai_actionctors.cpp)
//   0x1400bb490 Pathfinding_WalkTowards         (reg_citizenai_actionctors.cpp)
//   0x140002f90 Entity_FindComponent_MeshInstance (reg_work_base.cpp)
//   0x140002ed0 Entity_FindComponent_Position   (reg_work_base.cpp)
//   0x140122be0 FUN_140122be0 walkable-pos       (reg_work_walkpos.cpp)
//   0x140122b00 FUN_140122b00 walkable-pos recur (reg_work_walkpos.cpp)
//   0x140115a50 FUN_140115a50                    (reg_component_gap_A.cpp)
//   0x1400c3470 FUN_1400c3470 notify-entity      (reg_action_hunt.cpp)
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" uint64_t CitizenAI_State_Education(int64_t);      // 0x14010c360
extern "C" uint64_t CitizenAI_State_SeekHappiness(int64_t);  // 0x14010c580
extern "C" uint64_t CitizenAI_State_FindWork(int64_t);       // 0x14010d560

void RegisterCitizenAIHandlers2() {
    RegisterExeAddr(0x14010c360, reinterpret_cast<uintptr_t>(&CitizenAI_State_Education));
    RegisterExeAddr(0x14010c580, reinterpret_cast<uintptr_t>(&CitizenAI_State_SeekHappiness));
    RegisterExeAddr(0x14010d560, reinterpret_cast<uintptr_t>(&CitizenAI_State_FindWork));
}
#endif
