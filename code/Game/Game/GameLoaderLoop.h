#pragma once
// ============================================================================
// code/Game/Game/GameLoaderLoop.h — ENDGAME B.3.8 block LOOP (B38_RULES D8)
//
// The exe's second `Engine::LoaderLoop` subclass: the OUTER loader loop that
// every loop chain is wrapped in (GameMain, NewGame, SaveLoad, MainMenu,
// the tutorial paths). Unreflected. It overrides only its deleting dtor.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md sec. 6, B37_RULES sec. 3.1):
//   table 0x1401588d0, 12 slots, sizeof 0x48 (= Engine::LoaderLoop)
//   [0]  0x140001190  own scalar deleting dtor (~LoaderLoop + MemFree on &1)
//   [1]-[11] IMPORTS: LoaderLoop's Startup/Shutdown/Update/Draw/Load/OnVideoError,
//        UpdateLoop's DrawFailed/SaveRestore/OnVideoRestart/GetResourceFilename/
//        AllowsModChanges
//   stamped by 7 sites (GameMain, NewGameDialog, SaveLoadDialog_OnCreate,
//   MainMenuDialog, the tutorial helpers) after `??0LoaderLoop@Engine@@`.
//
// Slots the tree still overrides although retail inherits the base body are
// marked `(base in retail)`: the old hand-built table's stubs, moved here
// unchanged; the titled behaviour commit after this one makes them inherited.
// ============================================================================

#include <cstdint>
#include "engine/UpdateLoop.h"   // -Iruntime/types: Engine::LoaderLoop

class GameLoaderLoop final : public Engine::LoaderLoop {
public:
    // `??0LoaderLoop@Engine@@` then the 0x1401588d0 stamp (the compiler's).
    GameLoaderLoop(Engine::UpdateLoop* nextLoop, const System::Time& delay)
        : Engine::LoaderLoop(nextLoop, delay) {}

    Engine::UpdateLoop* DeletingDtor(unsigned flags) override;   // [0] 0x140001190
    void Startup() override;                                     // [1] (base in retail)
    void Shutdown() override;                                    // [2] (base in retail)
    bool Update(const System::Time& time) override;              // [3] (base in retail)
    void Draw(float interpolation) override;                     // [4] (base in retail)
    void DrawFailed() override;                                  // [5] (base in retail)
    void Load() override;                                        // [6] (base in retail)
    void SaveRestore(bool saving, Save::SavedState* state) override;   // [7] (base in retail)
    void OnVideoError() override;                                // [8] (base in retail)
    void OnVideoRestart() override;                              // [9] (base in retail)
    const char* GetResourceFilename() override;                  // [10] (base in retail)
    bool AllowsModChanges() override;                            // [11] (base in retail)

    static const void* Vtable();

protected:
    ~GameLoaderLoop() {}
};

static_assert(sizeof(GameLoaderLoop) == 0x48, "GameLoaderLoop adds nothing to Engine::LoaderLoop (MemAlloc(0x48) at every site)");
static_assert(alignof(GameLoaderLoop) == 8, "qword-aligned (the vptr)");
