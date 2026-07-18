// Component registration wave — batch "people"
//
// Registers the 22 genuine-entry exe helper addresses in this batch to their
// clean C++ reimplementations (bodies live in the component .cpp files that
// reference them). The 8 mid-function/bogus addresses in the original batch list
// are intentionally NOT registered — see the final report / COMPONENT_REG_WAVE notes.
//
// The integrator wires RegisterComponentBatch_people() into address_registry.cpp
// (adds an extern decl + a call). This file is owned solely by batch "people".
#include "resolve.h"
#include <cstdint>

// --- reimpl bodies (non-static, defined in the component .cpp files) ---

// citizencomponent.cpp
extern void* CitizenComp_helper_fe40(int64_t self, void* outIBP);
extern void* CitizenComp_helper_ff80(int64_t self, void* outIBP);
extern int64_t* CitizenComp_sub_ffc0(int64_t param_1, int64_t* param_2);
extern int64_t* CitizenComp_sub_0470(int64_t param_1, int64_t* param_2);
extern void* CitizenComp_Update_sub_0e70(int64_t param_1, void* param_2, int* param_3, char param_4);
extern void  CitizenComp_helper_1b90(int64_t selfV);
extern void  CitizenComp_sub_1fa0(int64_t selfV, void* param2);
extern bool  CitizenComp_helper_a4f0(int64_t self, int param2);
extern void  CitizenComp_FUN_14012c3f0(int64_t* param_1, uint8_t flagIndex, char value);
extern void  CitizenComp_ReleaseWorkplace(int64_t selfV, void* param2);
extern char  CitizenComp_CanFindHousing(int64_t selfV);

// citizenaicomponent.cpp
extern void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP);
extern int   CitizenComp_StorageComponent_QueryCount(int64_t self);
extern int   CitizenComp_Storage_GetResourceCount(int64_t self);
extern void  CitizenComp_CitizenAIComponent_TransitionState(int64_t selfV, char param);
extern void  CitizenComp_CitizenAIComponent_CheckEducation(int64_t selfV);
extern void  CitizenComp_CitizenAIComponent_CheckAge(int64_t selfV);

// populationcomponent.cpp
extern void PopulationComponent_SmoothDensity_Reimpl(void* selfV);
extern void PopulationComponent_ScanHabitability_Reimpl(void* selfV, void* terrainGridV, int cellIndex);

// charactercomponent.cpp
extern void Character_UpdateAnimation_Reimpl(void* arrayBase, void* ptrRef, int64_t deltaTime);

// nomadcomponent.cpp
extern void NomadComp_FUN_140103480(int64_t param_1, void* param_2);

// immigrationcomponent.cpp
extern void ImmigrationComp_DynArrayResize(int64_t* param_1, int newCount);

void RegisterComponentBatch_people() {
    // --- citizencomponent.cpp (9 genuine entries from the citizencomponent batch) ---
    RegisterExeAddr(0x14010fe40, reinterpret_cast<uintptr_t>(&CitizenComp_helper_fe40));
    RegisterExeAddr(0x14010ff80, reinterpret_cast<uintptr_t>(&CitizenComp_helper_ff80));
    RegisterExeAddr(0x14010ffc0, reinterpret_cast<uintptr_t>(&CitizenComp_sub_ffc0));
    RegisterExeAddr(0x140110470, reinterpret_cast<uintptr_t>(&CitizenComp_sub_0470));
    RegisterExeAddr(0x140110e70, reinterpret_cast<uintptr_t>(&CitizenComp_Update_sub_0e70));
    RegisterExeAddr(0x140111b90, reinterpret_cast<uintptr_t>(&CitizenComp_helper_1b90));
    RegisterExeAddr(0x140111fa0, reinterpret_cast<uintptr_t>(&CitizenComp_sub_1fa0));
    RegisterExeAddr(0x14012a4f0, reinterpret_cast<uintptr_t>(&CitizenComp_helper_a4f0));
    RegisterExeAddr(0x14012c3f0, reinterpret_cast<uintptr_t>(&CitizenComp_FUN_14012c3f0));

    // ReleaseWorkplace + CanFindHousing (CitizenComponent methods, referenced from
    // the citizenaicomponent batch grouping; bodies live in citizencomponent.cpp).
    RegisterExeAddr(0x1401152e0, reinterpret_cast<uintptr_t>(&CitizenComp_ReleaseWorkplace));
    RegisterExeAddr(0x140115be0, reinterpret_cast<uintptr_t>(&CitizenComp_CanFindHousing));

    // --- citizenaicomponent.cpp (storage/finder helpers + AI state) ---
    // NOTE 0x140008090 and 0x14012cf10 are ALSO referenced outside this batch
    // (storagecomponent*, citizencomponent). Single registration here; integrator
    // must ensure no other batch double-registers them.
    RegisterExeAddr(0x140008090, reinterpret_cast<uintptr_t>(&CitizenComp_Entity_FindComponent_Storage));
    RegisterExeAddr(0x14012cf10, reinterpret_cast<uintptr_t>(&CitizenComp_StorageComponent_QueryCount));
    RegisterExeAddr(0x14012a9d0, reinterpret_cast<uintptr_t>(&CitizenComp_Storage_GetResourceCount));
    RegisterExeAddr(0x14010db90, reinterpret_cast<uintptr_t>(&CitizenComp_CitizenAIComponent_TransitionState));
    RegisterExeAddr(0x14010f7a0, reinterpret_cast<uintptr_t>(&CitizenComp_CitizenAIComponent_CheckEducation));
    RegisterExeAddr(0x14010f890, reinterpret_cast<uintptr_t>(&CitizenComp_CitizenAIComponent_CheckAge));

    // --- populationcomponent.cpp ---
    RegisterExeAddr(0x1400538c0, reinterpret_cast<uintptr_t>(&PopulationComponent_SmoothDensity_Reimpl));
    RegisterExeAddr(0x140053b50, reinterpret_cast<uintptr_t>(&PopulationComponent_ScanHabitability_Reimpl));

    // --- charactercomponent.cpp ---
    RegisterExeAddr(0x1400c4f80, reinterpret_cast<uintptr_t>(&Character_UpdateAnimation_Reimpl));

    // --- nomadcomponent.cpp (FUN_140103480 = entity-removal controller teardown) ---
    // NOTE 0x1400032e0 / 0x140103480 are shared with other files; see report.
    RegisterExeAddr(0x140103480, reinterpret_cast<uintptr_t>(&NomadComp_FUN_140103480));

    // --- immigrationcomponent.cpp (shared DynArray<ptr> resize) ---
    RegisterExeAddr(0x1400032e0, reinterpret_cast<uintptr_t>(&ImmigrationComp_DynArrayResize));
}
