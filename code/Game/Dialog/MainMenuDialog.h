#pragma once
// ============================================================================
// code/Game/Dialog/MainMenuDialog.h — ENDGAME B.3.8 block DLG
//
// The front-end main menu AND the in-game pause menu (same class; Pause.rsc
// is built by the toolbar's pause path with +0xB8 = the game).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6; Ghidra calls the table
// g_PauseDialog_Vtable):
//   reflected "MainMenuDialog", parent "UI::DialogInstance", ti 0x1401c5c20
//   create 0x140020d10 (MemAlloc(0xc8, 8)), createInPlace 0x140020d60
//   ctors 0x14009ba30 (from MenuDLT), 0x1400b4450/80/c0, and
//   EntityUI_OpenPauseMenu 0x140101ce0 (inlined)
//   table 0x14015f550, 10 slots:
//     [0] 0x140020da0 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x1400b3cd0 OnOpened
//     [5] 0x1400b3c60 OnClosed (unpause audio)
//     [6] DialogInstance::OnUpdateLayout    [7] DialogInstance::OnUpdate
//     [8] 0x1400b3b50 OnElementEvent        [9] DialogInstance::OnSerializeSettings
//   sizeof 0xc8. The tree's 20-slot table carried a phantom "DialogController
//   secondary interface" in 10-19; retail has none (B38_RULES D3.5).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class GameUpdateLoop;    // Game/GameUpdateLoop.h
class MenuDialogLoopT;   // Game/MenuDialogLoopT.h

class MainMenuDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(MainMenuDialog)

public:
    MainMenuDialog();                                               // create 0x140020d10
    MainMenuDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                   Object::InternalPtr<UI::DialogInstance> parent,
                   GameUpdateLoop* game, MenuDialogLoopT* loop);    // 0x14009ba30 / pause

    ~MainMenuDialog() override = default;                          // [0] 0x140020da0
    // [4] PORT GAP: the tree runs a slim Setup (buttons + BuildId child) where
    // retail 0x1400b3cd0 also pauses the audio scenes and hides the in-game
    // buttons (MainMenuDialog_Slot4, kept in mainmenudialog.cpp, is that port).
    void OnOpened() override;                                      // [4] PORT GAP
    // [5] PORT GAP: the tree has a no-op; retail 0x1400b3c60 unpauses the audio
    // scenes (MainMenuDialog_Slot5 is that port).
    void OnClosed() override;                                      // [5] PORT GAP
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x1400b3b50

    // The button handlers retail's OnElementEvent 0x1400b3b50 calls.
    void OpenMod();                                                // 0x1400b2ea0
    void OpenTutorial();                                           // 0x1400b2fe0
    void OpenMap();                                                // 0x1400b3120
    void OpenGoal();                                               // 0x1400b3220
    void OpenScenario();                                           // 0x1400b3380
    void OpenAchievement();                                        // 0x1400b34c0
    void OpenNewGame();                                            // 0x1400b3620
    void OpenLoad();                                               // 0x1400b3770
    void OpenSave();                                               // 0x1400b38b0
    void OpenOptions();                                            // 0x1400b3a10
    void ReturnToMenu();                                           // 0x1400b4790

    // 0x1400b4540: build GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop and
    // hand it to the menu loop (front end) or the running game as the next loop.
    // NewGameDialog's OK and ScenarioDialog's Load call it (newgamedialog.cpp).
    void* StartNewGame(const System::String& name, uint32_t seed, uint32_t terrainSize,
                       bool disastersOff, void* mapGen, void* terrainGen, void* climateGen,
                       uint32_t scenarioResIdx, const void* savedStateIBP);  // 0x1400b4540

public:
    GameUpdateLoop*  game;       // +0xB8: the running game (0 at the front end; the pause menu's)
    MenuDialogLoopT* menuLoop;   // +0xC0: the owning menu loop (front end; 0 in the pause menu)
};

static_assert(offsetof(MainMenuDialog, game) == 0xB8, "MainMenuDialog +0xb8");
static_assert(offsetof(MainMenuDialog, menuLoop) == 0xC0, "MainMenuDialog +0xc0");
static_assert(sizeof(MainMenuDialog) == 0xC8, "MainMenuDialog is 0xc8 (create 0x140020d10)");
static_assert(alignof(MainMenuDialog) == 8, "MainMenuDialog is qword-aligned");
