// standalone/reg_town_workplace_registry.cpp
// Phase-1 economy: register the TownManager profession-BST workplace insert
// (0x1400895a0 AddWorkplaceToProfessionTree), ported in
// code/Game/Game/town_workplace_registry.cpp. Its AVL-insert closure (0x140017230
// wrapper etc.) is already registered by RegisterKeystoneARestreeBatch, and the
// array-append 0x140003110 by reg_action_hunt. So registering 0x1400895a0 is
// closure-safe. The convenience helper TownRegistry_RegisterWorkplace is called
// directly by the spawn wiring (not via ResolveAddr), so it needs no registration.
#include "resolve.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" void ResidenceComponent_impl_95a0(int64_t townMgr, void* workplaceRefIBP, uint64_t key); // 0x1400895a0
extern "C" void TownRegistry_ShuffleProfessionList(int64_t townMgr, uint64_t professionKey);        // 0x140089470

// BANISHED_LIVE_SEAT_SHUFFLE — OPT-IN (deliberately not in kDefaultOnFlags).
//
// 0x140089470 is FindAvailableBuilding's step 1: a Fisher-Yates shuffle of the
// candidate building list for one profession. Leaving it unregistered (the state
// before this wave) makes ResolveAddr return 0, FindAvailableBuilding's
// `if (s_optimize89470)` skip it, and seat order become deterministic — always the
// first registered building that has room. Registering it restores retail's random
// pick.
//
// That is a behaviour change on the DEFAULT path, so it is gated rather than flipped
// on: the project scar here is explicit — an unflagged change on the default path is
// an unnoticed behaviour change, and defaults-only must stay testable. A/B it with
// BANISHED_LIVE_SEAT_SHUFFLE=1 against the unset default before proposing it for
// kDefaultOnFlags.
static bool SeatShuffleOn() {
    return Opt().liveSeatShuffle;
}

void RegisterTownWorkplaceRegistry() {
    RegisterExeAddr(0x1400895a0, reinterpret_cast<uintptr_t>(&ResidenceComponent_impl_95a0));

    if (SeatShuffleOn()) {
        RegisterExeAddr(0x140089470, reinterpret_cast<uintptr_t>(&TownRegistry_ShuffleProfessionList));
        std::fprintf(stderr, "[SeatShuffle] 0x140089470 registered — building pick is now "
                             "RANDOM (retail behaviour). Unset BANISHED_LIVE_SEAT_SHUFFLE "
                             "for the deterministic first-with-room default.\n");
        std::fflush(stderr);
    }
}
