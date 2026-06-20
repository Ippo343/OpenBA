// standalone/reg_action_child_ctors2.cpp
// Registration for the B3 leaf Action copy-ctors + ConsumeRawMaterial/PickupStored
// helpers (code/Game/Action/action_child_ctors2.cpp). Wired via RegisterActionChildCtors2Batch().
// Closure-safe (all sub-callees registered); faithful Ghidra-verified ports.
//
// Also fills the RawMaterialDescription type-id global 0x1401c7cb8 — read by ca520's
// component-array scan AND by several already-landed functions (action_base_rawmat,
// action_pf_*, StorageComponent_QueryCount) that previously saw it null. Filled via
// GetTypeIdByName AFTER Engine::Startup (Object::Factory::Startup assigns the real CRC
// id only then), called from game_main.cpp alongside the other FillComponentTypeIdGlobals_*.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>

extern void* ActChild_CompleteRoadCopyCtor_c59d0(void*, uint32_t, uint32_t, uint8_t); // 0x1400c59d0
extern void* ActChild_HuntAnimalCopyCtor_c8400(void*, uint64_t*, uint64_t*);          // 0x1400c8400
extern void* ActChild_DropOffRawCopyCtor_c7cd0(void*, uint64_t*, uint64_t*, uint64_t, uint32_t, uint8_t); // 0x1400c7cd0
extern void  Entity_ConsumeListAppend_64b0(int64_t, int64_t, int);                    // 0x1400c64b0
extern void* ActChild_ConsumeRawCopyCtor_c63b0(void*, uint64_t*, int64_t, uint64_t, uint8_t); // 0x1400c63b0
extern void  ActChild_TransferToCarrier_ca230(int64_t, int64_t, int);                 // 0x1400ca230
extern void* ActChild_PickupStoredCopyCtor_ca520(void*, uint64_t*, uint64_t*, int64_t, int, uint8_t); // 0x1400ca520

// RawMaterialDescription type-id global (0x1401c7cb8), filled after Engine::Startup.
static uint32_t g_tid_RawMaterialDescription = 0;

extern "C" void FillTypeIdGlobals_actionchild() {
    g_tid_RawMaterialDescription = ::Object::GetTypeIdByName("RawMaterialDescription");
}

void RegisterActionChildCtors2Batch() {
    RegisterExeAddr(0x1400c59d0, reinterpret_cast<uintptr_t>(&ActChild_CompleteRoadCopyCtor_c59d0));
    RegisterExeAddr(0x1400c8400, reinterpret_cast<uintptr_t>(&ActChild_HuntAnimalCopyCtor_c8400));
    RegisterExeAddr(0x1400c7cd0, reinterpret_cast<uintptr_t>(&ActChild_DropOffRawCopyCtor_c7cd0));
    RegisterExeAddr(0x1400c64b0, reinterpret_cast<uintptr_t>(&Entity_ConsumeListAppend_64b0));
    RegisterExeAddr(0x1400c63b0, reinterpret_cast<uintptr_t>(&ActChild_ConsumeRawCopyCtor_c63b0));
    RegisterExeAddr(0x1400ca230, reinterpret_cast<uintptr_t>(&ActChild_TransferToCarrier_ca230));
    RegisterExeAddr(0x1400ca520, reinterpret_cast<uintptr_t>(&ActChild_PickupStoredCopyCtor_ca520));
    // type-id global storage (value filled later by FillTypeIdGlobals_actionchild)
    RegisterExeAddr(0x1401c7cb8, reinterpret_cast<uintptr_t>(&g_tid_RawMaterialDescription));
}
#endif // STANDALONE_EXE
