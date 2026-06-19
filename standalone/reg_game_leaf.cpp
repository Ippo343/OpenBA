// standalone/reg_game_leaf.cpp
// Game-module registration wave — closure-safe leaf helpers.
//
// Registers exe helper addresses -> our clean reimplementations (in
// code/Game/Game/game_leaf_helpers.cpp + the terrain reimpls already living in
// terraingenerator.cpp) so the guarded ResolveAddr(Module::Exe, ...) call sites
// in code/Game/Game/{mapgenerator,menuscene,gameupdateloop,terraingenerator}.cpp
// resolve to real code in the standalone exe.
//
// Every address here is fully closure-safe (deps = Runtime exports / static
// MemFree / already-registered 0x14000e320) and runs ONLY on the shutdown / dtor
// / teardown path, which the autostart A/B path never reaches. Registration is
// purely additive — it removes a latent silent-skip without changing world-build
// behavior.
//
// The integrator wires RegisterGameLeafBatch() into address_registry.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// ---- game_leaf_helpers.cpp ----
extern void Game_DestroyStringArray18(void* arrayHeader);   // FUN_14001b030 (mapgenerator)
extern void Game_DestroyStringArray30(void* arrayHeader);   // FUN_14001b0a0 (mapgenerator)
extern void Game_EventNotifierCleanup(void* arrayHeader);   // FUN_14000a8a0 (gameupdateloop)
extern void Game_MusicPlayerDtor(void* self);               // FUN_140018220 (gameupdateloop)
extern void Game_ImageCaptureDtor(void* self);              // FUN_1400a4fc0 (gameupdateloop)
extern void Game_MenuSceneArrayDtor(void* self);            // FUN_14001b860 (menuscene)
extern void Game_WeatherDtor(void* self);                   // FUN_140093eb0 (gameupdateloop)

// ---- terraingenerator.cpp (terrain-data-array reimpls live there as statics) ----
extern void RegisterGameTerrainReimpls();

void RegisterGameLeafBatch() {
    // mapgenerator.cpp
    RegisterExeAddr(0x14001b030, reinterpret_cast<uintptr_t>(&Game_DestroyStringArray18));
    RegisterExeAddr(0x14001b0a0, reinterpret_cast<uintptr_t>(&Game_DestroyStringArray30));

    // gameupdateloop.cpp (shutdown sub-functions)
    RegisterExeAddr(0x14000a8a0, reinterpret_cast<uintptr_t>(&Game_EventNotifierCleanup));
    RegisterExeAddr(0x140018220, reinterpret_cast<uintptr_t>(&Game_MusicPlayerDtor));
    RegisterExeAddr(0x1400a4fc0, reinterpret_cast<uintptr_t>(&Game_ImageCaptureDtor));
    RegisterExeAddr(0x140093eb0, reinterpret_cast<uintptr_t>(&Game_WeatherDtor));

    // menuscene.cpp
    RegisterExeAddr(0x14001b860, reinterpret_cast<uintptr_t>(&Game_MenuSceneArrayDtor));

    // terraingenerator.cpp (resize 0x1400206b0 / serialize 0x140020870)
    RegisterGameTerrainReimpls();
}
#endif // STANDALONE_EXE
