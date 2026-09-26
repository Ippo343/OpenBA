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

class MainMenuDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(MainMenuDialog)

public:
    MainMenuDialog();                                               // create 0x140020d10
    MainMenuDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                   Object::InternalPtr<UI::DialogInstance> parent,
                   void* game, void* loop);                         // 0x14009ba30 / pause

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140020da0
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

protected:
    ~MainMenuDialog() = default;

public:
    uint64_t dataB8;           // +0xB8: the running game (0 at the front end)
    uint64_t dataC0;           // +0xC0: the owning MenuDLT loop (front end)
};

static_assert(offsetof(MainMenuDialog, dataB8) == 0xB8, "MainMenuDialog +0xb8");
static_assert(offsetof(MainMenuDialog, dataC0) == 0xC0, "MainMenuDialog +0xc0");
static_assert(sizeof(MainMenuDialog) == 0xC8, "MainMenuDialog is 0xc8 (create 0x140020d10)");
static_assert(alignof(MainMenuDialog) == 8, "MainMenuDialog is qword-aligned");
