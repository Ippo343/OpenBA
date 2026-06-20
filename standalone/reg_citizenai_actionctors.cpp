// standalone/reg_citizenai_actionctors.cpp
// Registration for the CitizenAI action-ctor family (Brick 4b):
//   code/Game/Component/citizenai_actionctors.cpp
// Wire RegisterCitizenAIActionCtors() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// VERIFIED addresses (each confirmed via get_function_callees on the KNOWN-CORRECT state
// handler + decompile of the ctor itself; NavigateTo/WalkTowards additionally disasm-
// verified offset-by-offset):
//   0x1400c1070  PerformWork_Action_Ctor4    <- State_LongTermPlan
//   0x1400bd070  PerformWork_Action_Ctor5    <- State_SeekHappiness
//   0x1400bd420  PerformWork_Action_Ctor6    <- State_AdultTransition
//   0x1400bd790  PerformWork_Action_Ctor8    <- State_ElderTransition
//   0x1400bccb0  PerformWork_Action_Ctor10   <- State_Education
//   0x1400bc790  PerformWork_Action_Ctor11   <- State_GrowUp
//   0x1400bda50  Pathfinding_NavigateTo      <- State_Work
//   0x1400bb490  Pathfinding_WalkTowards     <- State_Work / State_FindWork
//   0x1400ba6b0  PlayAnimation_Action_Ctor4  (sibling of ported Ctor5 0x1400c26c0)
//
// Closure-safety: ALL exe sub-callees are already registered (verified by grep):
//   FUN_1400c5400, FUN_1400c54e0, SendComponentMessage(0x1400cb1d0), MoveTo(0x1400c9170),
//   CompleteTask(0x1400c5da0), Entity_FindComponent_Family(0x1400012d0),
//   Entity_impl_29a0(0x1400c29a0), DropOffRaw(0x1400c7cd0), ConsumeRaw(0x1400c63b0),
//   PickupStored(0x1400ca520), ShowTool(0x140042890); vtables Action/PerformWork/
//   PlayAnimation registered by their reimpl vtable batches.
//
// rdata float consts registered here (real values read with read_memory; not registered
// anywhere else): 0x140172124=0.75, 0x140172174=2.0, 0x140172194=4.0, 0x140172198=5.0,
// 0x14017214c=1.0. (0x140172280=-1.0 already in reg_citizenai_handlers.)
//
// NOT registered (intentional self-contained-brick pattern, read 0 + null-guarded in the
// reimpl, matching ~30 existing action reimpls): the SendComponentMessage message-id
// globals 0x1401c70e8 / 0x1401c7158 and the HappinessComponent id 0x1401c42c0. The
// RawMaterialDescription id 0x1401c7cb8 is already registered+filled elsewhere.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void CitizenAI_PerformWorkCtor4_c1070(void*, int64_t*, float*, int, uint8_t, uint32_t);
extern void CitizenAI_PerformWorkCtor5_bd070(void*, uint64_t*, float*, int64_t);
extern void CitizenAI_PerformWorkCtor6_bd420(void*, int64_t*, float*, char);
extern void CitizenAI_PerformWorkCtor8_bd790(void*, uint64_t*, float*);
extern void CitizenAI_PerformWorkCtor10_bccb0(void*, uint64_t*, float*, uint64_t);
extern void CitizenAI_PerformWorkCtor11_bc790(void*, int64_t, int64_t*, float*, int64_t*, float*);
extern void CitizenAI_Pathfinding_NavigateTo_bda50(void*, int64_t*, uint32_t*, uint8_t);
extern void CitizenAI_Pathfinding_WalkTowards_bb490(void*, uint32_t, uint32_t*, uint32_t, float, uint32_t);
extern void CitizenAI_PlayAnimationCtor4_ba6b0(void*, int64_t*, uint32_t*, float, uint32_t);

// rdata float-const storage (filled with the real exe values).
static const float k_rdata_172124 = 0.75f;
static const float k_rdata_172174 = 2.0f;
static const float k_rdata_172194 = 4.0f;
static const float k_rdata_172198 = 5.0f;
static const float k_rdata_17214c = 1.0f;

void RegisterCitizenAIActionCtors() {
    RegisterExeAddr(0x1400c1070, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor4_c1070));
    RegisterExeAddr(0x1400bd070, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor5_bd070));
    RegisterExeAddr(0x1400bd420, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor6_bd420));
    RegisterExeAddr(0x1400bd790, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor8_bd790));
    RegisterExeAddr(0x1400bccb0, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor10_bccb0));
    RegisterExeAddr(0x1400bc790, reinterpret_cast<uintptr_t>(&CitizenAI_PerformWorkCtor11_bc790));
    RegisterExeAddr(0x1400bda50, reinterpret_cast<uintptr_t>(&CitizenAI_Pathfinding_NavigateTo_bda50));
    RegisterExeAddr(0x1400bb490, reinterpret_cast<uintptr_t>(&CitizenAI_Pathfinding_WalkTowards_bb490));
    RegisterExeAddr(0x1400ba6b0, reinterpret_cast<uintptr_t>(&CitizenAI_PlayAnimationCtor4_ba6b0));
    // rdata float consts (real exe values; not registered elsewhere).
    RegisterExeAddr(0x140172124, reinterpret_cast<uintptr_t>(&k_rdata_172124));
    RegisterExeAddr(0x140172174, reinterpret_cast<uintptr_t>(&k_rdata_172174));
    RegisterExeAddr(0x140172194, reinterpret_cast<uintptr_t>(&k_rdata_172194));
    RegisterExeAddr(0x140172198, reinterpret_cast<uintptr_t>(&k_rdata_172198));
    RegisterExeAddr(0x14017214c, reinterpret_cast<uintptr_t>(&k_rdata_17214c));
}
#endif // STANDALONE_EXE
