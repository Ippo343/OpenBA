// standalone/reg_build_actions.cpp
// Register the ported BuildStructure::SetupBuildActions (FUN_1400c0690) so the
// engine_funcs global fn_SetupBuildActions (= ResolveAddr(Exe, 0x1400c0690)) resolves
// to our reimpl instead of NULL. Without this, BuildStructure::Execute's Phase-7 action
// setup calls a null pointer (EXEC of 0) the moment a seated Builder starts labor.
// See code/Game/Work/build_actions.cpp.

#include "resolve.h"
#include <cstdint>

extern "C" void BuildStructure_SetupBuildActions(void* citizenIBPptr, uint32_t priority,
                    int64_t* buildingIBPptr, float* position, uint32_t taskParam, uint32_t altAnim);

extern "C" void RegisterBuildActions() {
    RegisterExeAddr(0x1400c0690, reinterpret_cast<uintptr_t>(&BuildStructure_SetupBuildActions));
}
