// standalone/reg_action_hunt.cpp
// Wave 3 (Action) — HuntAnimal / RemoveAnimal / CompleteRoad shared sub-helper batch.
//
// Registers the exe addresses of action sub-helpers (resolved from
// code/Game/Action/*.cpp call sites — and, for 0x1400c3470 / 0x140003110, from many
// Component & Work files) to their reimplementations in
// code/Game/Action/action_base_hunt.cpp. The integrator wires RegisterActionHuntBatch()
// into RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
#include "resolve.h"
#include <cstdint>

// FindComponent family (action_base_hunt.cpp) — void* fn(uintptr_t entity, void* retIBP)
extern void* ActHunt_FindComponent_5500(uintptr_t, void*);  // 0x140005500 type 0x1401c6f98
extern void* ActHunt_FindComponent_27e0(uintptr_t, void*);  // 0x1400227e0 type 0x1401c8000

// HuntAnimal hold-time check — bool fn(void* self)
extern bool  ActHunt_IsAnimalReady_2a80(void*);             // 0x140122a80

// Dynamic 8-byte array set-count + insert — void fn(void* arr, int newCount, void* valuePtr)
extern void  ActHunt_DynArraySetCount_3110(void*, int, void*);   // 0x140003110

// Entity removal / changed-flags (high reuse) — void fn(void* self)
extern void  ActHunt_MarkEntityRemoval_c2ea0(void*);        // 0x1400c2ea0
extern void  ActHunt_NotifyEntityChanged_c3470(void*);      // 0x1400c3470  HIGH-REUSE

// Road-tile completion — void fn(void* terrain, int tile, uint8_t dir, uint32_t roadType)
// DEFERRED (body present, NOT registered): 0x14009c640 calls unregistered sub-helpers
// 0x14009f2f0 / 0x14009e0e0 / 0x1400af370 and mutates terrain cell + connectivity state —
// registering it would refresh tiles but skip connectivity (drift). Register once those land.
extern void  ActHunt_CompleteRoadTile_c640(void*, int, uint8_t, uint32_t);  // 0x14009c640 (deferred)

void RegisterActionHuntBatch() {
    RegisterExeAddr(0x140005500, reinterpret_cast<uintptr_t>(&ActHunt_FindComponent_5500));
    RegisterExeAddr(0x1400227e0, reinterpret_cast<uintptr_t>(&ActHunt_FindComponent_27e0));
    RegisterExeAddr(0x140122a80, reinterpret_cast<uintptr_t>(&ActHunt_IsAnimalReady_2a80));
    RegisterExeAddr(0x140003110, reinterpret_cast<uintptr_t>(&ActHunt_DynArraySetCount_3110));
    RegisterExeAddr(0x1400c2ea0, reinterpret_cast<uintptr_t>(&ActHunt_MarkEntityRemoval_c2ea0));
    RegisterExeAddr(0x1400c3470, reinterpret_cast<uintptr_t>(&ActHunt_NotifyEntityChanged_c3470));
    // 0x14009c640 deferred (see note above). 0x1400bbea0 RemoveAnimal_Action_Ctor SKIPPED
    // (609-insn builder; child actions via unregistered CopyCtors → no gain, crash risk).
    // 0x140122340 HerdComponent_StartMigration SKIPPED (no-migration branch infinite-loops
    // single-threaded in standalone — needs the async removal-queue drain).
}
