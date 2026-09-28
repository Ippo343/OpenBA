// code/Game/Dialog/mainmenudialog.cpp
// Clean reimplementation of MainMenuDialog methods.
//
// MainMenuDialog (0xC8 bytes): Main menu / pause menu screen.
// Layout:
//   +0x00..+0xB7 = DialogInstance base
//   +0xB8 = game world ptr (non-zero when game is loaded/active)
//   +0xC0 = engine instance ptr (used by menu-to-main transitions)
//
// Original source: C:\Projects\Buildy\code\Game\Dialog/MainMenuDialog.h
//                  Dialog\MainMenuDialog.cpp

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Cursor.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "application/Application.h"   // ENDGAME B.3.7 callers (K1)
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include "Dialog/MainMenuDialog.h"
#include "Game/MenuDialogLoopT.h"       // ENDGAME B.3.9 EXA: the real loop classes
#include "Game/GameLoadDialogLoopT.h"
#include "Game/GameLoaderLoop.h"
#include "Dialog/MapParamsDialog.h"
#include "Dialog/AchievementDialog.h"
#include "Dialog/GoalDialog.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Dialog/ScenarioDialog.h"      // B3R: ScenarioDialog_Ctor
#include "system/Configuration.h"      // B3R: System::Configuration, RtSystem_Configuration_CurrentBuildId
#include "save/SaveLoad.h"             // B3R: RtSave_SaveLoad_Dtor
#include "Dialog/dialog_exe_helpers.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <windows.h>  // PostQuitMessage fallback in standalone Quit handler
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// Helper: find element by name, returns raw element pointer
// ============================================================
static void* FindElement(void* dialog, const char* name) {
    System::String nameStr(name);
    uint8_t retBuf[16];
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge
    void* elem = *(void**)retBuf;
    return elem;
}

// ============================================================
// Button element IDs (assigned in Slot4, dispatched in Slot8)
// ============================================================
enum MainMenuButtonId {
    kButton_New         = 0,
    kButton_Resume      = 1,
    kButton_Load        = 2,
    kButton_Save        = 3,
    kButton_Options     = 4,
    kButton_Menu        = 5,
    kButton_Quit        = 6,
    kButton_Mod         = 7,
    kButton_Achievement = 8,
    kButton_Scenario    = 9,
    kButton_Goal        = 10,
    kButton_Map         = 11,
    kButton_Tutorial    = 12,
};

// ============================================================
// Helper: load .rsc resource for dialog construction
// ============================================================
// Retail validates Dialog/*.rsc resources against UI::Dialog's type id, read through
// the import slot 0x140151728 (?_typeInfo@Dialog@UI@@). ENDGAME B.3.9 TR: the class
// accessor (was the slot, zero in standalone, back-filled by a by-name factory lookup).

static void* LoadDialogResource(const char* rscPath) {
    System::Filename filenameBuf(rscPath);
    void* resource = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton   // B.3.3 bridge
            ->FindOrLoad(filenameBuf));
    printf("[LoadDialogResource] '%s' -> FindOrLoad=%p\n", rscPath, resource);
    void* extRes = nullptr;
    if (resource) {
        const uint32_t typeId = UI::Dialog::TypeId();
        bool derived = (typeId && ::Object::IsDerivedFromNullSafe(resource, typeId));
        printf("[LoadDialogResource] '%s' typeId=%u IsDerivedFrom=%d\n",
               rscPath, typeId, derived ? 1 : 0);
        if (derived) {
            extRes = resource;
        }
    }
    return extRes;
}

// ============================================================
// Helper: wrap dialog in InternalBasePtr and add via UI
// ============================================================
static void AddDialogToUI(MainMenuDialog* self, void* dlgObj) {
    uint8_t ibp[8];
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dlgObj));   // B.3.3 bridge
    reinterpret_cast<UI::UserInterface*>(*(void**)((uintptr_t)self + 0x10))->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(ibp));  // B.3.6 bridge
}

// ============================================================
// MainMenuDialog Open* handler implementations
// ============================================================
// Reimplemented from decompiled exe functions.
// Pattern: Load .rsc -> MemAlloc(size) -> call dialog ctor -> AddDialog
// Each ctor is a dialog-specific function that calls DialogInstance::DialogInstance
// internally, then sets its subclass vtable and initializes fields.

// OpenNewGame (0x1400B3620): "Dialog/NewGame.rsc", size 0xAE8
// Ctor: FUN_140071040(self, ui, extRes, nullParent, parentMenuIBP)
// Passes self (MainMenuDialog) as parent via stack arg for the child to reference.
void MainMenuDialog_OpenNewGame(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/NewGame.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xAE8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        new (parentIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        // Ctor args: self, ui, extRes, nullParent, parentMenuIBP (stack arg)
        dlgObj = NewGameDialog_Ctor_Standalone(dlgMem,   // 0x140071040
                                        *(void**)((uintptr_t)self + 0x10),
                                        extRes, nullIBP, parentIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// OpenLoad (0x1400B3770): "Dialog/SaveLoad.rsc", size 0x160, mode=1 (load)
// Ctor: FUN_1400b8270(self, ui, extRes, nullParent, mode, enginePtr, gameWorldPtr)
void MainMenuDialog_OpenLoad(MainMenuDialog* self) {
    // Same reason as OpenAchievement below: retail fills the Game container and hands
    // slot [0] to the AchievementManager ONCE at app init (GameContainer_Init,
    // 0x140099db0, from FUN_140001120), so every path into a game finds the manager
    // populated. Standalone replaced that with per-dialog GameContainer_EnsureLists()
    // calls in New Game, Scenario and Achievements -- and the LOAD path had none. A
    // loaded game therefore reached GameUpdateLoop::Update with the manager's instance
    // list (mgr+0x18) still null and faulted READ of 0 inside
    // AchievementManager::Update (Runtime 0x18009e6f4). That is the "achievement
    // instances are null after a load" open: it was a MISSING INIT on this path, not a
    // load-path Persist defect.
    GameContainer_EnsureLists();

    uintptr_t s = (uintptr_t)self;
    void* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x160, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = SaveLoadDialog_Ctor_8270(dlgMem,   // 0x1400b8270
                                         *(void**)(s + 0x10),
                                         extRes, nullIBP,
                                         1,  // mode = load
                                         *(void**)(s + 0xC0),   // engine ptr
                                         *(void**)(s + 0xB8));  // game world ptr
    }
    AddDialogToUI(self, dlgObj);
    // Standalone: expose the instance so the BANISHED_AUTOLOAD driver
    // (standalone/autostart.cpp) can select a save + inject the Load click.
    extern void* g_StandaloneSaveLoadDialog;
    g_StandaloneSaveLoadDialog = dlgObj;
}

// OpenSave (0x1400B38B0): "Dialog/SaveLoad.rsc", size 0x160, mode=0 (save)
void MainMenuDialog_OpenSave(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x160, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = SaveLoadDialog_Ctor_8270(dlgMem,   // 0x1400b8270
                                         *(void**)(s + 0x10),
                                         extRes, nullIBP,
                                         0,  // mode = save
                                         *(void**)(s + 0xC0),   // engine ptr
                                         *(void**)(s + 0xB8));  // game world ptr
    }
    AddDialogToUI(self, dlgObj);
}

// OpenOptions (0x1400B3A10): "Dialog/Options.rsc", size 0x1C0
// Ctor: FUN_140075150(self, ui, extRes, nullParent)
void MainMenuDialog_OpenOptions(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Options.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x1C0, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = OptionsDialog_Ctor_Standalone(dlgMem,   // 0x140075150
                                        *(void**)((uintptr_t)self + 0x10),
                                        extRes, nullIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// ReturnToMenu (0x1400B4790): Destroys SaveLoad, creates MenuDialogLoopT
// + GameLoadDialogLoopT (with "Dialog/Loading.rsc") + outer LoaderLoop.
// Sets next loop on either the engine singleton or existing dialog loader.
void MainMenuDialog_ReturnToMenu(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;
    uint64_t gameWorld = *(uint64_t*)(s + 0xB8);

    if (gameWorld == 0) {
        // No game active: destroy SaveLoad at engine+0xB70
        uintptr_t engPtr = *(uintptr_t*)(s + 0xC0);
        void* saveLoad = *(void**)(engPtr + 0xB70);
        if (saveLoad) {
            RtSave_SaveLoad_Dtor(reinterpret_cast<Save::SaveLoad*>(saveLoad));   // ??1SaveLoad 0x1800c3c90
            System::MemoryAllocator::MemFree(saveLoad);
        }
        *(void**)(engPtr + 0xB70) = nullptr;
    } else {
        // Game active: destroy SaveLoad at gameWorld+0x140
        void* saveLoad = *(void**)(gameWorld + 0x140);
        if (saveLoad) {
            RtSave_SaveLoad_Dtor(reinterpret_cast<Save::SaveLoad*>(saveLoad));   // ??1SaveLoad 0x1800c3c90
            System::MemoryAllocator::MemFree(saveLoad);
        }
        *(void**)(gameWorld + 0x140) = nullptr;
    }

    // ENDGAME B.3.9 EXA: the three real loop classes, built as retail's
    // 0x1400B4790 builds them (same allocations, same ctor arguments).
    // Step 1: MenuDialogLoopT (0xC00 bytes) with "Dialog/MainMenu.rsc" (ctor 0x14009bb00)
    Engine::UpdateLoop* menuLoop = nullptr;
    void* menuMem = System::MemoryAllocator::MemAlloc(0xC00, 8);
    if (menuMem) {
        System::String menuStr("Dialog/MainMenu.rsc");
        menuLoop = ::new (menuMem) MenuDialogLoopT(menuStr);
    }

    // Step 2: GameLoadDialogLoopT (0xB60 bytes) wrapping the menu loop (the
    // subclass ctor retail inlines here)
    Engine::UpdateLoop* dlLoop = nullptr;
    void* dlMem = System::MemoryAllocator::MemAlloc(0xB60, 4);
    if (dlMem) {
        uint64_t delayTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&delayTime), 1.0f);
        System::String loadingStr("Dialog/Loading.rsc");
        dlLoop = ::new (dlMem) GameLoadDialogLoopT(
            loadingStr, menuLoop, *reinterpret_cast<const System::Time*>(&delayTime));
    }

    // Step 3: outer GameLoaderLoop (0x48 bytes) wrapping the dialog loader
    void* outerLoop = nullptr;
    void* outerMem = System::MemoryAllocator::MemAlloc(0x48, 4);
    if (outerMem) {
        uint64_t zeroTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&zeroTime), 0.0f);
        outerLoop = ::new (outerMem) GameLoaderLoop(
            dlLoop, *reinterpret_cast<const System::Time*>(&zeroTime));
    }

    // Step 4: Set next loop on the appropriate target
    if (gameWorld == 0) {
        // No game: set on engine singleton (at +0xC0 = engine ptr)
        uintptr_t engPtr = *(uintptr_t*)(s + 0xC0);
        reinterpret_cast<UI::UserInterface*>((void*)(engPtr + 0x40))->SetFullscreenFade(true, 0xFF000000, false);  // B.3.6 bridge
        *(void**)(engPtr + 0xB68) = outerLoop;
    } else {
        // Game active: set cursor, fade, store at +0xC8
        void* ui = *(void**)(gameWorld + 0x98);
        uint64_t cursorVal = *(uint64_t*)((uintptr_t)(g_gameContainer) + 0x38);
        reinterpret_cast<UI::UserInterface*>(ui)->SetCursor(Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(cursorVal)));  // B.3.6 bridge
        reinterpret_cast<UI::UserInterface*>(ui)->SetFullscreenFade(true, 0xFF000000, false);  // B.3.6 bridge
        *(void**)(gameWorld + 0xC8) = outerLoop;
    }
}

// OpenAchievement (0x1400B34C0): "Dialog/Achievement.rsc", size 0xD0
// Uses DialogInstance_Ctor directly + vtable patch to PTR_FUN_14015b020.
// Extra fields: +0xB8, +0xC0, +0xC8 = 0.
void MainMenuDialog_OpenAchievement(MainMenuDialog* self) {
    // Retail fills the Game container (and hands slot [0] to the AchievementManager)
    // once at app init, in GameContainer_Init. Standalone has no master ExternalList,
    // so that init is replaced by GameContainer_EnsureLists, which the New Game and
    // Scenario dialogs already call on open. The Achievements dialog needs the same
    // guarantee: opened straight from the front end it is the FIRST thing to touch the
    // achievement list, and its Setup calls GetAchievementCount unguarded.
    GameContainer_EnsureLists();

    void* extRes = LoadDialogResource("Dialog/Achievement.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(AchievementDialog), 8);
    if (dlgMem) {
        // ENDGAME B.3.8 DLG: the real class (retail inlines this ctor here).
        dlgObj = ::new (dlgMem) AchievementDialog(
            static_cast<UI::UserInterface*>(self->_ui), static_cast<const UI::Dialog*>(extRes),
            Object::InternalPtr<UI::DialogInstance>());
    }
    AddDialogToUI(self, dlgObj);
}

// OpenScenario (0x1400B3380): "Dialog/Scenario.rsc", size 0xE8
// Ctor: FUN_1400786b0 (shared with TutorialDialog)
// Passes self as parent and difficulty=0 for scenarios from main menu.
void MainMenuDialog_OpenScenario(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Scenario.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xE8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        new (parentIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = ScenarioDialog_Ctor(dlgMem,   // 0x1400786b0
                                         *(void**)((uintptr_t)self + 0x10),
                                         extRes, nullIBP,
                                         parentIBP,
                                         0);  // difficulty (main menu = 0)
    }
    AddDialogToUI(self, dlgObj);
}

// OpenGoal (0x1400B3220): "Dialog/Goal.rsc", size 0xC8 (200 decimal)
// Uses DialogInstance_Ctor directly + vtable patch to PTR_FUN_14015beb0.
// Extra fields: +0xB8, +0xC0 = 0.
void MainMenuDialog_OpenGoal(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Goal.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(GoalDialog), 8);
    if (dlgMem) {
        // ENDGAME B.3.8 DLG: the real class (retail inlines this ctor here).
        dlgObj = ::new (dlgMem) GoalDialog(
            static_cast<UI::UserInterface*>(self->_ui), static_cast<const UI::Dialog*>(extRes),
            Object::InternalPtr<UI::DialogInstance>());
    }
    AddDialogToUI(self, dlgObj);
}

// OpenMap (0x1400B3120): "Dialog/MapParams.rsc", size 0xF8
// Ctor: FUN_14006bd80(self, ui, extRes, nullParent)
void MainMenuDialog_OpenMap(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xF8, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = MapParamsDialog_Ctor_Standalone(dlgMem,   // 0x14006bd80
                                          *(void**)((uintptr_t)self + 0x10),
                                          extRes, nullIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// OpenTutorial (0x1400B2FE0): "Dialog/Tutorial.rsc", size 0xE8
// Uses same ctor as Scenario (FUN_1400786b0).
void MainMenuDialog_OpenTutorial(MainMenuDialog* self) {
    printf("[MainMenuDialog] OpenTutorial called, self=%p\n", self);
    void* extRes = LoadDialogResource("Dialog/Tutorial.rsc");
    printf("[MainMenuDialog] OpenTutorial: extRes=%p\n", extRes);
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xE8, 8);
    printf("[MainMenuDialog] OpenTutorial: dlgMem=%p\n", dlgMem);
    if (dlgMem) {
        uint8_t parentIBP[8];
        new (parentIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = ScenarioDialog_Ctor(dlgMem,   // 0x1400786b0
                                         *(void**)((uintptr_t)self + 0x10),
                                         extRes, nullIBP,
                                         parentIBP,
                                         1);  // difficulty: 1 = tutorial mode
        printf("[MainMenuDialog] OpenTutorial: dlgObj=%p after ctor\n", dlgObj);
    }
    {
        // Expose the instance so the BANISHED_AUTOTUTORIAL driver in
        // standalone/autostart.cpp can select an entry and inject the Load click,
        // the same way OpenLoad exposes the SaveLoadDialog.
        extern void* g_StandaloneScenarioDialog;
        g_StandaloneScenarioDialog = dlgObj;
    }
    AddDialogToUI(self, dlgObj);
    printf("[MainMenuDialog] OpenTutorial: dialog added to UI\n");
}

// OpenMod (0x1400B2EA0): "Dialog/Mod.rsc", size 0xD8
// Ctor: FUN_140070030(self, ui, extRes, parentIBP, parentMenuIBP)
void MainMenuDialog_OpenMod(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Mod.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xD8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        new (parentIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        dlgObj = ModDialog_Ctor_Standalone(dlgMem,   // 0x140070030
                                    *(void**)((uintptr_t)self + 0x10),
                                    extRes, nullIBP, parentIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// ============================================================
// CreateBuildIdChildDialog
// ============================================================
// Loads "Dialog/BuildId.rsc", constructs a child DialogInstance, attaches it
// to the parent's UI, finds "textBuildNotice" inside, and sets parameters
// 0=CurrentVersion and 1=CurrentBuildId on the TextElement.
//
// Mirrors the bottom half of MainMenuDialog_Slot4 (exe @ 0x1400b3cd0) so it
// can be reused from the standalone Slot4 stub. Returns true on success.
//
// The child is a plain UI::DialogInstance (retail stamps the exe's copy of its
// table, 0x14015f5a0; the real class's compiler table is that table).
//
// Returns true if the child dialog was created and added to the UI.
static bool CreateBuildIdChildDialog(void* selfDialog, void* userInterfacePtr) {
    if (!selfDialog || !userInterfacePtr) {
        printf("[BuildId] selfDialog or UI null, skipping\n");
        return false;
    }
    // (B3R: the readiness test over the engine_funcs doors was always true; gone.
    // The allocator singleton -- the old g_pMemAllocSingleton snapshot -- is read live.)
    if (!::Object::ExternalFactory::_singleton ||
        !System::MemoryAllocator::_singleton) {
        printf("[BuildId] missing engine singletons, skipping\n");
        return false;
    }

    // --- Load Dialog/BuildId.rsc as External resource ---
    // The Filename ctor already normalises delimiters (B.3.1 rule): no
    // separate AdjustDelimiters call is needed here.
    System::Filename filenameStr("Dialog/BuildId.rsc");
    void* resource = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton   // B.3.3 bridge
            ->FindOrLoad(filenameStr));
    void* dialogResource = nullptr;
    if (resource && ::Object::IsDerivedFromNullSafe(resource, UI::Dialog::TypeId())) {
        dialogResource = resource;
    }
    if (!dialogResource) {
        printf("[BuildId] Dialog/BuildId.rsc not found / wrong type, skipping\n");
        return false;
    }

    // --- Allocate + construct child DialogInstance (0xB8 bytes) ---
    void* childDialog = System::MemoryAllocator::MemAlloc(0xB8, 8);
    if (!childDialog) {
        printf("[BuildId] MemAlloc(0xB8) failed\n");
        return false;
    }

    // ENDGAME B.3.9 UI-D (B9-13 foreign site): the real 6-arg ctor
    // (0x180114870), parent = this menu dialog. Its compiler table is the one
    // retail's copy at 0x14015f5a0 describes, so the (standalone-null)
    // g_MainMenuDialog_BuildIdVtable stamp is gone.
    ::new (childDialog) UI::DialogInstance(
        reinterpret_cast<UI::UserInterface*>(userInterfacePtr),       // B.3.8 bridge
        reinterpret_cast<const UI::Dialog*>(dialogResource),          // B.3.8 bridge
        Object::InternalPtr<UI::DialogInstance>(
            reinterpret_cast<UI::DialogInstance*>(selfDialog)),       // B.3.8 bridge
        nullptr, nullptr, 0);

    // --- Add child to UI ---
    uint8_t childInternalPtr[16] = {};
    new (childInternalPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(childDialog));   // B.3.3 bridge
    reinterpret_cast<UI::UserInterface*>(userInterfacePtr)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(childInternalPtr));  // B.3.6 bridge

    // --- Find textBuildNotice inside the child dialog ---
    System::String nameStr("textBuildNotice");

    void* childDI = *(void**)childInternalPtr;
    uint8_t retBuf[16] = {};
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(childDI)->FindElement(nameStr));  // B.3.6 bridge
    void* textElem = *(void**)retBuf;

    printf("[BuildId] childDI=%p textElem=%p\n", childDI, textElem);

    if (!textElem) {
        printf("[BuildId] textBuildNotice element not found in BuildId.rsc\n");
        return true;  // child dialog is still up, just no version text
    }
    // The original asm uses a TextElement-specific type ID slot for the
    // IsDerivedFrom check, but in standalone we don't have that wired. Skip
    // the type check — if it's the wrong element type the SetParameter call
    // below will harmlessly no-op (or vtable-dispatch into the right spot).
    void* textBuildNotice = textElem;

    uint8_t textElemPtr[16] = {};
    new (textElemPtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(textBuildNotice));   // B.3.3 bridge

    // --- Set parameter 0 = CurrentVersion, 1 = CurrentBuildId ---
    // Both Configuration:: helpers return UI::String via hidden return ptr ABI.
    // versionStr/buildIdStr are UI::String (16 bytes), which embeds a
    // System::String at offset 0 — the two fn_String_Dtor calls below only
    // ever released that embedded field, so they become an explicit dtor call
    // through the bridge cast rather than a real System::String local.
    // ?CurrentVersion@Configuration@System@@ is a by-value static: its registered door
    // (RtSystem_Configuration_CurrentVersion) placement-constructs the result in the
    // caller's zeroed slot, which is what this does.
    uint8_t versionStr[16] = {};
    ::new (versionStr) System::String(System::Configuration::CurrentVersion());
    reinterpret_cast<UI::TextElement*>(*(void**)textElemPtr)->SetParameter(0, *reinterpret_cast<const System::String*>(versionStr));  // B.3.6 bridge
    reinterpret_cast<System::String*>(versionStr)->~String();  // B.3.1 bridge

    // ?CurrentBuildId@ has no member yet (Configuration.h note 2): its body is the
    // retslot function, called by name on the caller's zeroed slot.
    uint8_t buildIdStr[16] = {};
    RtSystem_Configuration_CurrentBuildId(reinterpret_cast<System::String*>(buildIdStr));
    reinterpret_cast<UI::TextElement*>(*(void**)textElemPtr)->SetParameter(1, *reinterpret_cast<const System::String*>(buildIdStr));  // B.3.6 bridge
    reinterpret_cast<System::String*>(buildIdStr)->~String();  // B.3.1 bridge

    printf("[BuildId] child dialog attached, version + build id set\n");
    return true;
}

// ============================================================
// Slot4 (0x1400B3CD0): Setup — initialize main menu UI
// ============================================================
// Finds all menu buttons by name, assigns element IDs.
// If a game is active: pauses audio, hides goal/map/save/mod buttons
// based on game state. Creates BuildId child dialog showing version info.
//
// Original: 445 instructions.

void MainMenuDialog_Slot4(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find all 13 buttons by name ---
    void* buttonNew         = FindElement(self, "buttonNew");
    void* buttonResume      = FindElement(self, "buttonResume");
    void* buttonLoad        = FindElement(self, "buttonLoad");
    void* buttonSave        = FindElement(self, "buttonSave");
    void* buttonOptions     = FindElement(self, "buttonOptions");
    void* buttonMenu        = FindElement(self, "buttonMenu");
    void* buttonQuit        = FindElement(self, "buttonQuit");
    void* buttonAchievement = FindElement(self, "buttonAchievement");
    void* buttonScenario    = FindElement(self, "buttonScenario");
    void* buttonGoal        = FindElement(self, "buttonGoal");
    void* buttonMap         = FindElement(self, "buttonMap");
    void* buttonTutorial    = FindElement(self, "buttonTutorial");
    void* buttonMod         = FindElement(self, "buttonMod");

    // Which buttons does this .rsc actually contain? A null here means the click can
    // never dispatch, because no element ever receives the id.
    fprintf(stderr, "[MainMenu] buttons: new=%p resume=%p load=%p save=%p options=%p menu=%p "
                    "quit=%p achv=%p scenario=%p goal=%p map=%p tutorial=%p mod=%p\n",
            buttonNew, buttonResume, buttonLoad, buttonSave, buttonOptions, buttonMenu,
            buttonQuit, buttonAchievement, buttonScenario, buttonGoal, buttonMap,
            buttonTutorial, buttonMod);
    fflush(stderr);

    // --- Assign element IDs (with CheckPtr safety) ---
    if (buttonNew) {
        reinterpret_cast<UI::Element*>(buttonNew)->SetElementId(kButton_New);  // B.3.6 bridge
    }
    if (buttonResume) {
        reinterpret_cast<UI::Element*>(buttonResume)->SetElementId(kButton_Resume);  // B.3.6 bridge
        // Set +0xC0 flag to 8 (command dispatch flag)
        *(uint32_t*)((uintptr_t)buttonResume + 0xC0) = 8;
    }
    if (buttonOptions) {
        reinterpret_cast<UI::Element*>(buttonOptions)->SetElementId(kButton_Options);  // B.3.6 bridge
    }
    if (buttonQuit) {
        reinterpret_cast<UI::Element*>(buttonQuit)->SetElementId(kButton_Quit);  // B.3.6 bridge
    }
    if (buttonLoad) {
        reinterpret_cast<UI::Element*>(buttonLoad)->SetElementId(kButton_Load);  // B.3.6 bridge
    }
    if (buttonSave) {
        reinterpret_cast<UI::Element*>(buttonSave)->SetElementId(kButton_Save);  // B.3.6 bridge
    }
    if (buttonMenu) {
        reinterpret_cast<UI::Element*>(buttonMenu)->SetElementId(kButton_Menu);  // B.3.6 bridge
    }
    if (buttonAchievement) {
        reinterpret_cast<UI::Element*>(buttonAchievement)->SetElementId(kButton_Achievement);  // B.3.6 bridge
    }
    if (buttonScenario) {
        reinterpret_cast<UI::Element*>(buttonScenario)->SetElementId(kButton_Scenario);  // B.3.6 bridge
    }
    if (buttonGoal) {
        reinterpret_cast<UI::Element*>(buttonGoal)->SetElementId(kButton_Goal);  // B.3.6 bridge
    }
    if (buttonMap) {
        reinterpret_cast<UI::Element*>(buttonMap)->SetElementId(kButton_Map);  // B.3.6 bridge
    }
    if (buttonTutorial) {
        reinterpret_cast<UI::Element*>(buttonTutorial)->SetElementId(kButton_Tutorial);  // B.3.6 bridge
    }
    if (buttonMod) {
        reinterpret_cast<UI::Element*>(buttonMod)->SetElementId(kButton_Mod);  // B.3.6 bridge
    }

    // --- If game is active: pause audio and conditionally hide buttons ---
    uint64_t gameWorld = *(uint64_t*)(s + 0xB8);
    if (gameWorld != 0) {
        void* world = GameUpdateLoop::_singleton;
        void* audioScene = *(void**)((uintptr_t)world + 0x78);

        reinterpret_cast<Audio::Scene*>(audioScene)->Pause(4, true);   // B.3.7 bridge
        reinterpret_cast<Audio::Scene*>(audioScene)->Pause(3, true);   // B.3.7 bridge
        reinterpret_cast<Audio::Scene*>(audioScene)->Pause(2, true);   // B.3.7 bridge

        // Check scenario index at world+0x15C. If < 0, hide buttonGoal; else hide buttonMap
        int32_t scenarioIdx = *(int32_t*)((uintptr_t)world + 0x15C);
        if (scenarioIdx < 0) {
            if (buttonGoal) {
                reinterpret_cast<UI::Element*>(buttonGoal)->SetFlagsOnOff(1, 0); // B.3.6 bridge; hide
            }
        } else {
            if (buttonMap) {
                reinterpret_cast<UI::Element*>(buttonMap)->SetFlagsOnOff(1, 0); // B.3.6 bridge; hide
            }
        }

        // Check game flags at gameWorld+0xD0, bit 10: if set, hide save & mod buttons
        uint32_t gameFlags = *(uint32_t*)(gameWorld + 0xD0);
        if (gameFlags & (1 << 10)) {
            if (buttonSave) {
                // Call vtable[8] = SetEnabled(false)
                reinterpret_cast<UI::Element*>(buttonSave)->Enable(0);  // B.3.6 bridge
            }
            if (buttonMod) {
                reinterpret_cast<UI::Element*>(buttonMod)->Enable(0);  // B.3.6 bridge
            }
        }
    }

    // --- Create BuildId child dialog (delegates to shared helper) ---
    CreateBuildIdChildDialog((void*)self, *(void**)(s + 0x10));
}

// ============================================================
// Slot5 (0x1400B3C60): Shutdown — unpause audio scenes
// ============================================================
// Called when the menu closes (returning to gameplay).

void MainMenuDialog_Slot5(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Only unpause if a game is active (flag at +0xB8 != 0)
    if (*(uint64_t*)(s + 0xB8) == 0)
        return;

    void* world = GameUpdateLoop::_singleton;
    void* audioScene = *(void**)((uintptr_t)world + 0x78);

    reinterpret_cast<Audio::Scene*>(audioScene)->Pause(4, false);   // B.3.7 bridge
    reinterpret_cast<Audio::Scene*>(audioScene)->Pause(3, false);   // B.3.7 bridge
    reinterpret_cast<Audio::Scene*>(audioScene)->Pause(2, false);   // B.3.7 bridge
}

// ============================================================
// Slot8 (0x1400B3B50): OnCommand — handle menu button clicks
// ============================================================
// Dispatches button clicks based on element ID (param4) when
// the command type (param3) is 0x12 (button click event).
// Each handler opens a sub-dialog or performs an action.
//
// Private handler functions are now reimplemented above as clean C++.

// NOTE (2026-08-30): this retail-shaped handler is NOT the one that runs. The
// standalone MainMenuDialog vtable installs StandaloneStub_Slot8_OnCommand at +0x40
// (see g_MainMenuDialog_Vtable_Standalone below), so THIS switch is dead code. Keep
// the two switches in sync, or better, delete this one - a button wired only here
// looks correct in review and does nothing in the game.
void MainMenuDialog_Slot8(MainMenuDialog* self, void* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    // Only handle button click events (command type 0x12)
    if (param3 != 0x12)
        return;

    switch (param4) {
    case kButton_New:
        MainMenuDialog_OpenNewGame(self);
        break;

    case kButton_Resume: {
        // Close this dialog (return to gameplay)
        uint8_t selfPtr[16];
        new (selfPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>((void*)self));   // B.3.3 bridge
        reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfPtr));  // B.3.6 bridge
        break;
    }

    case kButton_Load:
        MainMenuDialog_OpenLoad(self);
        break;

    case kButton_Save:
        MainMenuDialog_OpenSave(self);
        break;

    case kButton_Options:
        MainMenuDialog_OpenOptions(self);
        break;

    case kButton_Menu:
        MainMenuDialog_ReturnToMenu(self);
        break;

    case kButton_Quit:
        ::Application::Application::_singleton->Quit();  // B.3.7 bridge
        break;

    case kButton_Mod:
        MainMenuDialog_OpenMod(self);
        break;

    case kButton_Achievement:
        MainMenuDialog_OpenAchievement(self);
        break;

    case kButton_Scenario:
        MainMenuDialog_OpenScenario(self);
        break;

    case kButton_Goal:
        MainMenuDialog_OpenGoal(self);
        break;

    case kButton_Map:
        MainMenuDialog_OpenMap(self);
        break;

    case kButton_Tutorial:
        MainMenuDialog_OpenTutorial(self);
        break;
    }
}


// ============================================================
// MainMenuDialog as a real class (ENDGAME B.3.8 block DLG)
// ============================================================
// Dialog/MainMenuDialog.h carries the retail slot list. The live bodies below
// are the ones the old hand-built 20-slot table installed (its slots 10-19 were
// a phantom "DialogController secondary interface" retail does not have).
// Slots 6/7 are INHERITED: the old table called DialogInstance's bodies there
// with a qualified call, which is exactly what retail's import thunks reach.

#include "Object/factory_registration.h"
#include "address_registry.h"

// [4] OnOpened — PORT GAP (header): slim standalone Setup. Mirrors the
// button-finding + element ID assignment portion of MainMenuDialog_Slot4
// (above) and the BuildId child dialog; skips the game-world audio block.
void MainMenuDialog::OnOpened() {
    printf("[MainMenuDialog::Setup] self=%p assigning button element IDs\n", (void*)this);

    auto findAndAssign = [this](const char* name, int id) {
        System::String nameStr(name);
        // FindElement uses non-trivial return ABI: RCX=self, RDX=retbuf, R8=name
        uint8_t retBuf[16] = {};
        ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr));  // B.3.6 bridge
        void* elem = *reinterpret_cast<void**>(retBuf);
        if (elem) {
            // CheckPtr resolves any lazy/weak pointer inside the IBP, then
            // SetElementId writes the int into the element.
            reinterpret_cast<UI::Element*>(elem)->SetElementId(id);  // B.3.6 bridge
            printf("[Setup]   %-20s -> id=%2d (elem=%p)\n", name, id, elem);
        } else {
            printf("[Setup]   %-20s -> NOT FOUND\n", name);
        }
    };

    findAndAssign("buttonNew",         kButton_New);
    findAndAssign("buttonResume",      kButton_Resume);
    findAndAssign("buttonLoad",        kButton_Load);
    findAndAssign("buttonSave",        kButton_Save);
    findAndAssign("buttonOptions",     kButton_Options);
    findAndAssign("buttonMenu",        kButton_Menu);
    findAndAssign("buttonQuit",        kButton_Quit);
    findAndAssign("buttonAchievement", kButton_Achievement);
    findAndAssign("buttonScenario",    kButton_Scenario);
    findAndAssign("buttonGoal",        kButton_Goal);
    findAndAssign("buttonMap",         kButton_Map);
    findAndAssign("buttonTutorial",    kButton_Tutorial);
    findAndAssign("buttonMod",         kButton_Mod);

    // --- Create BuildId child dialog (cosmetic version overlay) ---
    // Pulls UI ptr from MainMenuDialog +0x10 (DialogInstance::m_userInterface).
    // Uses the shared CreateBuildIdChildDialog helper.
    CreateBuildIdChildDialog(this, _ui);
}

// [5] OnClosed — PORT GAP (header): the tree's no-op; retail unpauses the
// audio scenes (MainMenuDialog_Slot5 above is that port).
void MainMenuDialog::OnClosed() {}

// [8] OnElementEvent — button click dispatch. The runtime sends msg=0x12 with
// the element ID as the second int.
void StandaloneOpenMapParams(void* self);  // defined below (map dialog bring-up)

extern "C" void ModDialog_OpenFromMainMenu(void* mainMenuSelf);

void MainMenuDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                    int32_t msg, int32_t id, int32_t /*c*/,
                                    int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (msg != 0x12) return;
    printf("[MainMenuDialog::OnCommand] self=%p msg=0x%x id=%d\n", (void*)this, msg, id);

    switch (id) {
    case kButton_New:
        printf("[MainMenuDialog::OnCommand] -> OpenNewGame\n");
        MainMenuDialog_OpenNewGame(this);
        break;
    case kButton_Resume: {
        // Resume = close this in-game menu, which removes it from the UI's
        // dialog list. UI::UserInterface::PausesGame then returns false and
        // GameUpdateLoop::Update resumes ticking the simulation. This is the
        // same close path MainMenuDialog_Slot8 uses, and it
        // relies only on fn_UI_CloseDialog / fn_InternalBasePtr_Ctor, both of
        // which the standalone toolbar already exercises (so they're resolved).
        printf("[MainMenuDialog::OnCommand] -> Resume (CloseDialog)\n");
        uint8_t selfPtr[16];
        new (selfPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        static_cast<UI::UserInterface*>(_ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfPtr));  // B.3.6 bridge
        break;
    }
    case kButton_Quit:
        printf("[MainMenuDialog::OnCommand] Quit: Application::_singleton=%p\n",
               (void*)::Application::Application::_singleton);
        if (::Application::Application::_singleton) {
            printf("[MainMenuDialog::OnCommand] -> App::Quit\n");
            ::Application::Application::_singleton->Quit();  // B.3.7 bridge
        } else {
            printf("[MainMenuDialog::OnCommand] WARN: App::Quit not resolved — falling back to PostQuitMessage\n");
            // Last resort — close the window so the user isn't stuck
            PostQuitMessage(0);
        }
        break;
    case kButton_Menu:
        // "Quit to Main Menu" -- in-game only (buttonMenu is not in MainMenu.rsc,
        // only in Pause.rsc, so this case is unreachable from the front end).
        // ReturnToMenu takes the +0xB8 != 0 branch: destroy the GameUpdateLoop's
        // SaveLoad (+0x140), build MenuDialogLoopT -> GameLoadDialogLoopT ->
        // LoaderLoop, fade to black, and park the chain in m_nextLoop (+0xC8).
        // GameUpdateLoop::Update step 17 picks that up once the fade completes
        // (UI+0xF0 == 0) and calls MainLoop::SetNextLoop, which runs the ported
        // teardown (gameupdateloop_teardown.cpp) on the way out.
        printf("[MainMenuDialog::OnCommand] -> ReturnToMenu (gameUpdateLoop=%p)\n",
               reinterpret_cast<void*>(dataB8));
        MainMenuDialog_ReturnToMenu(this);
        break;
    case kButton_Options:
        printf("[MainMenuDialog::OnCommand] -> OpenOptions\n");
        MainMenuDialog_OpenOptions(this);
        break;
    case kButton_Load:
        // Save/load wave 2026-08-27: SaveLoadDialog ctor + vtable are now
        // registered, the menu-side Save::SaveLoad exists (+0xb70) and is
        // ticked — the retail load-from-save path is standalone-ready.
        printf("[MainMenuDialog::OnCommand] -> OpenLoad (SaveLoadDialog)\n");
        MainMenuDialog_OpenLoad(this);
        break;
    case kButton_Save:
        printf("[MainMenuDialog::OnCommand] -> OpenSave (SaveLoadDialog)\n");
        MainMenuDialog_OpenSave(this);
        break;
    case kButton_Map:
        printf("[MainMenuDialog::OnCommand] -> OpenMap (MapParamsDialog)\n");
        StandaloneOpenMapParams(this);
        break;
    case kButton_Tutorial:
        // The main menu .rsc has NO buttonScenario - the [Setup] log prints it
        // "NOT FOUND". Scenarios live behind the Tutorial button, which opens the same
        // ScenarioDialog with difficulty=1. Neither id was in this switch, so a click
        // logged "not yet wired" and did nothing (the retail-shaped MainMenuDialog_Slot8
        // above HAS both cases, but it is not the handler the vtable installs).
        // Wired 2026-08-30 so LANE1 row 42 is reachable by hand.
        printf("[MainMenuDialog::OnCommand] -> OpenTutorial (ScenarioDialog, difficulty=1)\n");
        MainMenuDialog_OpenTutorial(this);
        break;
    case kButton_Scenario:
        printf("[MainMenuDialog::OnCommand] -> OpenScenario (ScenarioDialog, difficulty=0)\n");
        MainMenuDialog_OpenScenario(this);
        break;
    case kButton_Achievement:
        // Wired 2026-09-05. The retail-shaped MainMenuDialog_Slot8 above has had this
        // case all along, but that switch is NOT the handler the vtable installs --
        // so the Achievements button (front end AND in-game pause menu, same dialog
        // class) silently fell through to `default` and logged "not yet wired".
        printf("[MainMenuDialog::OnCommand] -> OpenAchievement (AchievementDialog)\n");
        MainMenuDialog_OpenAchievement(this);
        break;
    case kButton_Goal:
        // Same story as Achievement: present in the dead retail switch, missing here.
        printf("[MainMenuDialog::OnCommand] -> OpenGoal (GoalDialog)\n");
        MainMenuDialog_OpenGoal(this);
        break;
    case kButton_Mod:
        // ModDialog/ModDetailsDialog live in code/Game/Dialog/mod_dialog_vtables.cpp.
        // Default-on since 2026-09-05 (BANISHED_LIVE_MODDIALOG=0 is the kill-switch).
        printf("[MainMenuDialog::OnCommand] -> OpenMods (ModDialog)\n");
        ModDialog_OpenFromMainMenu(this);
        break;
    default:
        printf("[MainMenuDialog::OnCommand] button id=%d not yet wired in standalone\n", id);
        break;
    }
}

// ============================================================
// MapParamsDialog open path — in-game "Map" button (id 11)
// ============================================================
// buttonMap -> Dialog/MapParams.rsc (the map name/seed/terrain/climate/
// condition/disaster info dialog). Builds the real MapParamsDialog
// (mapparamsdialog.cpp) and AddDialog's it (which dispatches its OnOpened).
static void StandaloneOpenMapParams_UI(void* ui) {
    if (!ui) { printf("[OpenMap] ui null\n"); return; }
    if (!System::MemoryAllocator::_singleton) {   // B3R: the g_pMemAllocSingleton snapshot, read live
        printf("[OpenMap] core fns unresolved\n"); return;
    }
    void* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    if (!extRes) { printf("[OpenMap] Dialog/MapParams.rsc not loaded\n"); return; }
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(MapParamsDialog), 8);
    if (!dlgMem) return;
    MapParamsDialog* dlg = ::new (dlgMem) MapParamsDialog(
        static_cast<UI::UserInterface*>(ui), static_cast<const UI::Dialog*>(extRes),
        Object::InternalPtr<UI::DialogInstance>());
    uint8_t ibp[8] = {};
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dlg));   // B.3.3 bridge
    static_cast<UI::UserInterface*>(ui)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(ibp));  // B.3.6 bridge
    printf("[OpenMap] MapParamsDialog opened + AddDialog'd (dlg=%p)\n", (void*)dlg);
}

// Called from the OnElementEvent dispatch (id 11 = buttonMap).
void StandaloneOpenMapParams(void* self) {
    if (!self) return;
    StandaloneOpenMapParams_UI(static_cast<MainMenuDialog*>(self)->_ui);
}

// Harness entry — open the map dialog headlessly (UI from gameWorld+0x98).
extern "C" void MapParamsDialog_HarnessOpen() {
    void* gw = GameUpdateLoop::_singleton;
    if (!gw) { printf("[OpenMap] gameWorld null\n"); return; }
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x98);
    printf("[OpenMap] harness open: ui=%p\n", ui);
    StandaloneOpenMapParams_UI(ui);
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// Retail ctor 0x14009BA30 (from MenuDLT): DialogInstance::DialogInstance with
// (this, ui_at_+0x40, rsc_at_+0x30, nullParent, 0, 0, 0), then the two
// MainMenuDialog fields:
//   +0xB8 = the running game (0 at the menu; set by the in-game pause path)
//   +0xC0 = the OWNING LOOP (param_1 == the MenuDLT) — `*(this+0xc0) = param_1`.
// This +0xC0 wiring is load-bearing: NewGameDialog::StartGame reads
// worldSetup(==this MainMenuDialog)+0xC0 to decide its transition path. If it
// is 0, StartGame falls into the SetEngineNextLoop(worldSetup+0xB8) branch and
// dereferences a NULL engine. With +0xC0 = loop it correctly takes
// SetDialogLoaderNextLoop(loop) and writes loop+0xb68.

MainMenuDialog::MainMenuDialog() : dataB8(0), dataC0(0) {}

MainMenuDialog::MainMenuDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                               Object::InternalPtr<UI::DialogInstance> parent,
                               void* game, void* loop)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      dataB8(reinterpret_cast<uint64_t>(game)), dataC0(reinterpret_cast<uint64_t>(loop)) {}

// create 0x140020d10: MemAlloc(0xc8, 8), then the ctor.
void* MainMenuDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MainMenuDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) MainMenuDialog());
}
// createInPlace 0x140020d60 (the tree's in-place build is kept, B8-13).
void* MainMenuDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) MainMenuDialog());
}


// Public entry called from MenuDLT::CreateDialog (standalone/menudialogloopt_stubs.cpp).
// The loop's dialog resource (+0x30) is always loaded by then; a loop without one
// keeps the tree's bare-bones fallback (a default-constructed dialog, no element
// tree) instead of the Runtime ctor faulting on a null Dialog — a standalone-only
// guard retail does not have.
extern "C" void* MainMenuDialog_FromMenuDLT(void* loop, void* retInternalBasePtr) {
    if (!System::MemoryAllocator::_singleton) {   // B3R: the g_pMemAllocSingleton snapshot, read live
        if (retInternalBasePtr) {
            *reinterpret_cast<uint64_t*>(retInternalBasePtr) = 0;
        }
        return retInternalBasePtr;
    }
    void* mem = System::MemoryAllocator::MemAlloc(sizeof(MainMenuDialog), 8);
    uintptr_t lp = reinterpret_cast<uintptr_t>(loop);
    void* rsc = loop ? *reinterpret_cast<void**>(lp + 0x30) : nullptr;
    MainMenuDialog* dlg;
    if (rsc) {
        dlg = ::new (mem) MainMenuDialog(reinterpret_cast<UI::UserInterface*>(lp + 0x40),
                                         static_cast<const UI::Dialog*>(rsc),
                                         Object::InternalPtr<UI::DialogInstance>(),
                                         nullptr, loop);
    } else {
        printf("[MainMenuDialog_FromLoop] bare-bones init (loop=%p rsc=%p) — "
               "DialogInstance ctor skipped\n", loop, rsc);
        dlg = ::new (mem) MainMenuDialog();
        dlg->dataC0 = reinterpret_cast<uint64_t>(loop);
    }
    new (retInternalBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(dlg));   // B.3.3 bridge
    return retInternalBasePtr;
}

DEFINE_EXE_TYPE(MainMenuDialog, "UI::DialogInstance")
