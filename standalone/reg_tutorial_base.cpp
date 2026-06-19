// standalone/reg_tutorial_base.cpp
// Tutorial wave — registers the 10 unregistered exe helpers the Tutorial module
// resolves via ResolveAddr(Module::Exe, ...) (the Tutorial ResolveAddr gap), plus
// the internal compute sub-helper 0x14012e5c0 referenced by the work-capacity port.
//
// Reimplementations live in code/Game/Tutorial/tutorial_base.cpp. The integrator
// wires RegisterTutorialBaseBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified in Ghidra as a real function ENTRY
// (get_function_by_address Entry == address). Closure verified: family A
// (serialize) calls only Runtime exports + engine_funcs (no unregistered exe
// sub-callees); family B calls only FUN_140003050 (reg, Work wave),
// FUN_140004680 (reg, component-nature wave), and FUN_14012e5c0 (reg below).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// Family A — ExternalPtr serialize helpers
extern "C" void TutBase_SerializeStepArray(void*, int, void*, void*);   // 0x140026fb0
extern "C" void TutBase_SerializeImageArray(void*, int, void*, void*);  // 0x140028cf0
extern "C" void TutBase_SerializeSetupArray(void*, int, void*, void*);  // 0x140028f40
extern "C" void TutBase_SerializeGoalArray(void*, int, void*, void*);   // 0x140029190
extern "C" void TutBase_SerializeImageRef(void*, void*);                // 0x140027ad0
extern "C" void TutBase_ResolveToolbarRef(void*);                       // 0x140027de0

// Family B — TutorialGoalCount::Check data helpers
extern "C" uint32_t TutBase_CountBuildingsByType(uintptr_t, uintptr_t);   // 0x140092010
extern "C" uint32_t TutBase_CountProfessionsByType(uintptr_t, uintptr_t); // 0x1400910a0
extern "C" void*    TutBase_CastToSliderElement(void*);                   // 0x140013920
extern "C" int      TutBase_ComputeWorkCapacity(uintptr_t);               // 0x14012e5c0
extern "C" int      TutBase_GetWorkCapacity(uintptr_t);                   // 0x14012fbb0

void RegisterTutorialBaseBatch() {
    RegisterExeAddr(0x140026fb0, reinterpret_cast<uintptr_t>(&TutBase_SerializeStepArray));
    RegisterExeAddr(0x140028cf0, reinterpret_cast<uintptr_t>(&TutBase_SerializeImageArray));
    RegisterExeAddr(0x140028f40, reinterpret_cast<uintptr_t>(&TutBase_SerializeSetupArray));
    RegisterExeAddr(0x140029190, reinterpret_cast<uintptr_t>(&TutBase_SerializeGoalArray));
    RegisterExeAddr(0x140027ad0, reinterpret_cast<uintptr_t>(&TutBase_SerializeImageRef));
    RegisterExeAddr(0x140027de0, reinterpret_cast<uintptr_t>(&TutBase_ResolveToolbarRef));
    RegisterExeAddr(0x140092010, reinterpret_cast<uintptr_t>(&TutBase_CountBuildingsByType));
    RegisterExeAddr(0x1400910a0, reinterpret_cast<uintptr_t>(&TutBase_CountProfessionsByType));
    RegisterExeAddr(0x140013920, reinterpret_cast<uintptr_t>(&TutBase_CastToSliderElement));
    RegisterExeAddr(0x14012e5c0, reinterpret_cast<uintptr_t>(&TutBase_ComputeWorkCapacity));
    RegisterExeAddr(0x14012fbb0, reinterpret_cast<uintptr_t>(&TutBase_GetWorkCapacity));
}
#endif
