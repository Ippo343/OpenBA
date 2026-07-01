// standalone/reg_town_workplace_registry.cpp
// Phase-1 economy: register the TownManager profession-BST workplace insert
// (0x1400895a0 AddWorkplaceToProfessionTree), ported in
// code/Game/Game/town_workplace_registry.cpp. Its AVL-insert closure (0x140017230
// wrapper etc.) is already registered by RegisterKeystoneARestreeBatch, and the
// array-append 0x140003110 by reg_action_hunt. So registering 0x1400895a0 is
// closure-safe. The convenience helper TownRegistry_RegisterWorkplace is called
// directly by the spawn wiring (not via ResolveAddr), so it needs no registration.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern "C" void ResidenceComponent_impl_95a0(int64_t townMgr, void* workplaceRefIBP, uint64_t key); // 0x1400895a0

void RegisterTownWorkplaceRegistry() {
    RegisterExeAddr(0x1400895a0, reinterpret_cast<uintptr_t>(&ResidenceComponent_impl_95a0));
}
#endif // STANDALONE_EXE
