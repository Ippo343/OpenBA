// standalone/reg_town_storage_seed.cpp
// Economy Brick M-D (WORLD/STORAGE half) — registers the town storage-location
// registry + starting-resource seeding ports (code/Game/Game/town_storage_seed.cpp)
// so ResolveAddr(Module::Exe, ...) call sites resolve in the standalone exe.
//
// All four addresses were previously unregistered:
//   0x14012d230  StorageLocationDescription_helper_d230 (townMgr+0x00 ADD)
//   0x140107ff0  BuildDescription_helper_7ff0 (build-complete storage init)
//   0x140091660  Game_SpawnResources (starting-resource placement)
//   0x1400a1f00  Game_impl_1f00 (seven-mask seeding driver)
//
// The runtime behavior of these ports is additionally gated behind
// BANISHED_LIVE_STORAGE at the starttown.cpp call sites — registration itself
// is inert for the default build.
//
// The integrator wires RegisterTownStorageSeed() into address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// ---- code/Game/Game/town_storage_seed.cpp ----
extern "C" void TownRegistry_RegisterStorageLocation(void* storageLocationComp);   // 0x14012d230
extern "C" void TownStorage_BuildCompleteInit_7ff0(void* componentWithEntity);     // 0x140107ff0
extern "C" void TownStorage_SpawnResources_91660(void* townMgr, uint32_t mask, int count); // 0x140091660
extern "C" void StartTown_SeedStartingResources(void* startConfig);                // 0x1400a1f00

extern "C" void RegisterTownStorageSeed() {
    RegisterExeAddr(0x14012d230, reinterpret_cast<uintptr_t>(&TownRegistry_RegisterStorageLocation));
    RegisterExeAddr(0x140107ff0, reinterpret_cast<uintptr_t>(&TownStorage_BuildCompleteInit_7ff0));
    RegisterExeAddr(0x140091660, reinterpret_cast<uintptr_t>(&TownStorage_SpawnResources_91660));
    RegisterExeAddr(0x1400a1f00, reinterpret_cast<uintptr_t>(&StartTown_SeedStartingResources));
}
#endif // STANDALONE_EXE
