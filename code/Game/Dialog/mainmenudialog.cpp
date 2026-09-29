// code/Game/Dialog/mainmenudialog.cpp
// Clean reimplementation of MainMenuDialog methods.
//
// MainMenuDialog (0xC8 bytes): Main menu / pause menu screen.
// Layout: DialogInstance base, +0xB8 = the running game (GameUpdateLoop*, non-zero
// in the pause menu), +0xC0 = the owning MenuDialogLoopT (front end).
//
// Original source: C:\Projects\Buildy\code\Game\Dialog/MainMenuDialog.h
//                  Dialog\MainMenuDialog.cpp

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ButtonElement.h"
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
#include "Dialog/ModDialog.h"
#include "Dialog/NewGameDialog.h"
#include "Dialog/MapParamsDialog.h"
#include "Dialog/AchievementDialog.h"
#include "Dialog/GoalDialog.h"
#include "Dialog/ui_dialogs.h"
#include "options.h"
#include "system/Value.h"
#include "runtime/types/ui/DialogInstance.h"
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
// Helper: find element by name
// ============================================================
static UI::Element* FindElement(MainMenuDialog* dialog, const char* name) {
    System::String nameStr(name);
    return dialog->FindElement(nameStr).Get();
}

// ============================================================
// Button element IDs (assigned in OnOpened, dispatched in OnElementEvent)
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

static UI::Dialog* LoadDialogResource(const char* rscPath) {
    System::Filename filenameBuf(rscPath);
    // The dialog doors of the other blocks take the resource as a non-const void*.
    ::Object::External* resource = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton->FindOrLoad(filenameBuf));
    printf("[LoadDialogResource] '%s' -> FindOrLoad=%p\n", rscPath, (const void*)resource);
    UI::Dialog* extRes = nullptr;
    if (resource) {
        const uint32_t typeId = UI::Dialog::TypeId();
        bool derived = (typeId && ::Object::IsDerivedFromNullSafe(resource, typeId));
        printf("[LoadDialogResource] '%s' typeId=%u IsDerivedFrom=%d\n",
               rscPath, typeId, derived ? 1 : 0);
        if (derived) {
            extRes = static_cast<UI::Dialog*>(resource);
        }
    }
    return extRes;
}

// ============================================================
// Helper: wrap dialog in InternalPtr and add via UI
// ============================================================
static void AddDialogToUI(MainMenuDialog* self, UI::DialogInstance* dlgObj) {
    Object::InternalPtr<UI::DialogInstance> ibp(dlgObj);
    self->_ui->AddDialog(&ibp);
}

// ============================================================
// MainMenuDialog Open* handlers
// ============================================================
// Reimplemented from decompiled exe functions.
// Pattern: Load .rsc -> MemAlloc(size) -> call dialog ctor -> AddDialog

// OpenNewGame (0x1400B3620): "Dialog/NewGame.rsc", size 0xAE8
// Ctor: 0x140071040(self, ui, extRes, nullParent, parentMenuIBP)
// Passes this menu as the new-game dialog's owner (StartGame builds through it).
void MainMenuDialog::OpenNewGame() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/NewGame.rsc");
    NewGameDialog* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xAE8, 8);
    if (dlgMem) {
        printf("[NewGameDialog_Ctor] self=%p ui=%p extRes=%p parentMenu=%p\n",
               dlgMem, (void*)_ui, (const void*)extRes, (void*)this);
        dlgObj = ::new (dlgMem) NewGameDialog(_ui, extRes,   // 0x140071040
                                              Object::InternalPtr<UI::DialogInstance>(),
                                              Object::InternalPtr<MainMenuDialog>(this));
        // Publish the just-constructed dialog so the auto-start driver
        // (standalone/autostart.cpp) can locate it and fire Start once OnOpened has
        // populated the combos + OK button. Harmless when autostart is disabled.
        g_StandaloneNewGameDialog = dlgObj;
    }
    AddDialogToUI(this, dlgObj);
}

// OpenLoad (0x1400B3770): "Dialog/SaveLoad.rsc", size 0x160, mode=1 (load)
// Ctor: 0x1400b8270(self, ui, extRes, nullParent, mode, menuLoop, game)
void MainMenuDialog::OpenLoad() {
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

    UI::Dialog* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x160, 8);
    if (dlgMem) {
        Object::InternalPtr<UI::DialogInstance> nullIBP;
        dlgObj = SaveLoadDialog_Ctor_8270(dlgMem,   // 0x1400b8270
                                         _ui, extRes, &nullIBP,
                                         1,  // mode = load
                                         menuLoop,   // the menu loop
                                         game);      // the running game
    }
    AddDialogToUI(this, static_cast<UI::DialogInstance*>(dlgObj));
    // Standalone: expose the instance so the BANISHED_AUTOLOAD driver
    // (standalone/autostart.cpp) can select a save + inject the Load click.
    extern void* g_StandaloneSaveLoadDialog;
    g_StandaloneSaveLoadDialog = dlgObj;
}

// OpenSave (0x1400B38B0): "Dialog/SaveLoad.rsc", size 0x160, mode=0 (save)
void MainMenuDialog::OpenSave() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x160, 8);
    if (dlgMem) {
        Object::InternalPtr<UI::DialogInstance> nullIBP;
        dlgObj = SaveLoadDialog_Ctor_8270(dlgMem,   // 0x1400b8270
                                         _ui, extRes, &nullIBP,
                                         0,  // mode = save
                                         menuLoop,   // the menu loop
                                         game);      // the running game
    }
    AddDialogToUI(this, static_cast<UI::DialogInstance*>(dlgObj));
}

// OpenOptions (0x1400B3A10): "Dialog/Options.rsc", size 0x1C0
// Ctor: 0x140075150(self, ui, extRes, nullParent)
void MainMenuDialog::OpenOptions() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/Options.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0x1C0, 8);
    if (dlgMem) {
        Object::InternalPtr<UI::DialogInstance> nullIBP;
        dlgObj = OptionsDialog_Ctor_Standalone(dlgMem,   // 0x140075150
                                        _ui, extRes, &nullIBP);
    }
    AddDialogToUI(this, static_cast<UI::DialogInstance*>(dlgObj));
}

// ReturnToMenu (0x1400B4790): Destroys SaveLoad, creates MenuDialogLoopT
// + GameLoadDialogLoopT (with "Dialog/Loading.rsc") + outer LoaderLoop.
// Sets next loop on either the menu loop or the running game.
void MainMenuDialog::ReturnToMenu() {
    GameUpdateLoop* gameWorld = game;

    if (gameWorld == nullptr) {
        // No game active: destroy the menu loop's SaveLoad.
        // LEFT: MenuDialogLoopT::_saveLoad is void* (block GAME's header).
        void* saveLoad = menuLoop->_saveLoad;
        if (saveLoad) {
            RtSave_SaveLoad_Dtor(reinterpret_cast<Save::SaveLoad*>(saveLoad));   // ??1SaveLoad 0x1800c3c90
            System::MemoryAllocator::MemFree(saveLoad);
        }
        menuLoop->_saveLoad = nullptr;
    } else {
        // Game active: destroy the game's SaveLoad
        Save::SaveLoad* saveLoad = gameWorld->m_saveLoad;
        if (saveLoad) {
            RtSave_SaveLoad_Dtor(saveLoad);   // ??1SaveLoad 0x1800c3c90
            System::MemoryAllocator::MemFree(saveLoad);
        }
        gameWorld->m_saveLoad = nullptr;
    }

    // ENDGAME B.3.9 EXA: the three real loop classes, built as retail's
    // 0x1400B4790 builds them (same allocations, same ctor arguments).
    // Step 1: MenuDialogLoopT (0xC00 bytes) with "Dialog/MainMenu.rsc" (ctor 0x14009bb00)
    Engine::UpdateLoop* newMenuLoop = nullptr;
    void* menuMem = System::MemoryAllocator::MemAlloc(0xC00, 8);
    if (menuMem) {
        System::String menuStr("Dialog/MainMenu.rsc");
        newMenuLoop = ::new (menuMem) MenuDialogLoopT(menuStr);
    }

    // Step 2: GameLoadDialogLoopT (0xB60 bytes) wrapping the menu loop (the
    // subclass ctor retail inlines here)
    Engine::UpdateLoop* dlLoop = nullptr;
    void* dlMem = System::MemoryAllocator::MemAlloc(0xB60, 4);
    if (dlMem) {
        System::Time delayTime;
        delayTime._ticks = 0;
        RtSystem_Time_Ctor(&delayTime, 1.0f);
        System::String loadingStr("Dialog/Loading.rsc");
        dlLoop = ::new (dlMem) GameLoadDialogLoopT(loadingStr, newMenuLoop, delayTime);
    }

    // Step 3: outer GameLoaderLoop (0x48 bytes) wrapping the dialog loader
    GameLoaderLoop* outerLoop = nullptr;
    void* outerMem = System::MemoryAllocator::MemAlloc(0x48, 4);
    if (outerMem) {
        System::Time zeroTime;
        zeroTime._ticks = 0;
        RtSystem_Time_Ctor(&zeroTime, 0.0f);
        outerLoop = ::new (outerMem) GameLoaderLoop(dlLoop, zeroTime);
    }

    // Step 4: Set next loop on the appropriate target
    if (gameWorld == nullptr) {
        // No game: fade the menu loop's UI and queue the chain
        menuLoop->_ui.SetFullscreenFade(true, 0xFF000000, false);
        menuLoop->_pendingLoop = outerLoop;
    } else {
        // Game active: set cursor, fade, store as the game's next loop
        UI::UserInterface* ui = gameWorld->m_userInterface;
        ui->SetCursor(g_gameContainer->cursorHourglass);
        ui->SetFullscreenFade(true, 0xFF000000, false);
        gameWorld->m_nextLoop = outerLoop;
    }
}

// OpenAchievement (0x1400B34C0): "Dialog/Achievement.rsc", size 0xD0
// The AchievementDialog ctor, inlined by retail.
void MainMenuDialog::OpenAchievement() {
    // Retail fills the Game container (and hands slot [0] to the AchievementManager)
    // once at app init, in GameContainer_Init. Standalone has no master ExternalList,
    // so that init is replaced by GameContainer_EnsureLists, which the New Game and
    // Scenario dialogs already call on open. The Achievements dialog needs the same
    // guarantee: opened straight from the front end it is the FIRST thing to touch the
    // achievement list, and its Setup calls GetAchievementCount unguarded.
    GameContainer_EnsureLists();

    UI::Dialog* extRes = LoadDialogResource("Dialog/Achievement.rsc");
    AchievementDialog* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(AchievementDialog), 8);
    if (dlgMem) {
        // ENDGAME B.3.8 DLG: the real class (retail inlines this ctor here).
        dlgObj = ::new (dlgMem) AchievementDialog(
            _ui, extRes, Object::InternalPtr<UI::DialogInstance>());
    }
    AddDialogToUI(this, dlgObj);
}

// OpenScenario (0x1400B3380): "Dialog/Scenario.rsc", size 0xE8
// Ctor: 0x1400786b0 (shared with the Tutorial dialog)
// Passes this menu as the owner and difficulty=0 for scenarios from main menu.
void MainMenuDialog::OpenScenario() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/Scenario.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xE8, 8);
    if (dlgMem) {
        Object::InternalPtr<UI::DialogInstance> parentIBP(this);
        Object::InternalPtr<UI::DialogInstance> nullIBP;
        dlgObj = ScenarioDialog_Ctor(dlgMem,   // 0x1400786b0
                                     _ui, extRes, &nullIBP,
                                     &parentIBP,
                                     0);  // difficulty (main menu = 0)
    }
    AddDialogToUI(this, static_cast<UI::DialogInstance*>(dlgObj));
}

// OpenGoal (0x1400B3220): "Dialog/Goal.rsc", size 0xC8 (200 decimal)
// The GoalDialog ctor, inlined by retail.
void MainMenuDialog::OpenGoal() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/Goal.rsc");
    GoalDialog* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(GoalDialog), 8);
    if (dlgMem) {
        // ENDGAME B.3.8 DLG: the real class (retail inlines this ctor here).
        dlgObj = ::new (dlgMem) GoalDialog(
            _ui, extRes, Object::InternalPtr<UI::DialogInstance>());
    }
    AddDialogToUI(this, dlgObj);
}

// OpenMap (0x1400B3120): "Dialog/MapParams.rsc", size 0xF8
// Ctor: 0x14006bd80(self, ui, extRes, nullParent)
void MainMenuDialog::OpenMap() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    MapParamsDialog* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xF8, 8);
    if (dlgMem) {
        dlgObj = ::new (dlgMem) MapParamsDialog(   // 0x14006bd80
            _ui, extRes, Object::InternalPtr<UI::DialogInstance>());
    }
    AddDialogToUI(this, dlgObj);
}

// OpenTutorial (0x1400B2FE0): "Dialog/Tutorial.rsc", size 0xE8
// Uses same ctor as Scenario (0x1400786b0).
void MainMenuDialog::OpenTutorial() {
    printf("[MainMenuDialog] OpenTutorial called, self=%p\n", (void*)this);
    UI::Dialog* extRes = LoadDialogResource("Dialog/Tutorial.rsc");
    printf("[MainMenuDialog] OpenTutorial: extRes=%p\n", (const void*)extRes);
    void* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xE8, 8);
    printf("[MainMenuDialog] OpenTutorial: dlgMem=%p\n", dlgMem);
    if (dlgMem) {
        Object::InternalPtr<UI::DialogInstance> parentIBP(this);
        Object::InternalPtr<UI::DialogInstance> nullIBP;
        dlgObj = ScenarioDialog_Ctor(dlgMem,   // 0x1400786b0
                                     _ui, extRes, &nullIBP,
                                     &parentIBP,
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
    AddDialogToUI(this, static_cast<UI::DialogInstance*>(dlgObj));
    printf("[MainMenuDialog] OpenTutorial: dialog added to UI\n");
}

// OpenMod (0x1400B2EA0): "Dialog/Mod.rsc", size 0xD8
// Ctor: 0x140070030(self, ui, extRes, parentIBP, parentMenuIBP)
void MainMenuDialog::OpenMod() {
    UI::Dialog* extRes = LoadDialogResource("Dialog/Mod.rsc");
    ModDialog* dlgObj = nullptr;
    void* dlgMem = System::MemoryAllocator::MemAlloc(0xD8, 8);
    if (dlgMem) {
        dlgObj = ::new (dlgMem) ModDialog(_ui, extRes,   // 0x140070030
                                          Object::InternalPtr<UI::DialogInstance>(),
                                          Object::InternalPtr<MainMenuDialog>(this));
    }
    AddDialogToUI(this, dlgObj);
}

// ============================================================
// CreateBuildIdChildDialog
// ============================================================
// Loads "Dialog/BuildId.rsc", constructs a child DialogInstance, attaches it
// to the parent's UI, finds "textBuildNotice" inside, and sets parameters
// 0=CurrentVersion and 1=CurrentBuildId on the TextElement.
//
// Mirrors the bottom half of MainMenuDialog_Slot4 (exe @ 0x1400b3cd0) so it
// can be reused from OnOpened. Returns true on success.
//
// The child is a plain UI::DialogInstance (retail stamps the exe's copy of its
// table, 0x14015f5a0; the real class's compiler table is that table).
//
// Returns true if the child dialog was created and added to the UI.
static bool CreateBuildIdChildDialog(MainMenuDialog* selfDialog, UI::UserInterface* userInterface) {
    if (!selfDialog || !userInterface) {
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
    const ::Object::External* resource =
        ::Object::ExternalFactory::_singleton->FindOrLoad(filenameStr);
    const UI::Dialog* dialogResource = nullptr;
    if (resource && ::Object::IsDerivedFromNullSafe(resource, UI::Dialog::TypeId())) {
        dialogResource = static_cast<const UI::Dialog*>(resource);
    }
    if (!dialogResource) {
        printf("[BuildId] Dialog/BuildId.rsc not found / wrong type, skipping\n");
        return false;
    }

    // --- Allocate + construct child DialogInstance (0xB8 bytes) ---
    void* childMem = System::MemoryAllocator::MemAlloc(0xB8, 8);
    if (!childMem) {
        printf("[BuildId] MemAlloc(0xB8) failed\n");
        return false;
    }

    // ENDGAME B.3.9 UI-D (B9-13 foreign site): the real 6-arg ctor
    // (0x180114870), parent = this menu dialog. Its compiler table is the one
    // retail's copy at 0x14015f5a0 describes, so the (standalone-null)
    // g_MainMenuDialog_BuildIdVtable stamp is gone.
    UI::DialogInstance* childDialog = ::new (childMem) UI::DialogInstance(
        userInterface, dialogResource,
        Object::InternalPtr<UI::DialogInstance>(selfDialog),
        nullptr, nullptr, 0);

    // --- Add child to UI ---
    Object::InternalPtr<UI::DialogInstance> childInternalPtr(childDialog);
    userInterface->AddDialog(&childInternalPtr);

    // --- Find textBuildNotice inside the child dialog ---
    System::String nameStr("textBuildNotice");

    UI::DialogInstance* childDI = childInternalPtr.Get();
    UI::Element* textElem = childDI->FindElement(nameStr).Get();

    printf("[BuildId] childDI=%p textElem=%p\n", (void*)childDI, (void*)textElem);

    if (!textElem) {
        printf("[BuildId] textBuildNotice element not found in BuildId.rsc\n");
        return true;  // child dialog is still up, just no version text
    }
    // The original asm uses a TextElement-specific type ID slot for the
    // IsDerivedFrom check, but in standalone we don't have that wired. Skip
    // the type check — if it's the wrong element type the SetParameter call
    // below will harmlessly no-op (or vtable-dispatch into the right spot).
    Object::InternalPtr<UI::TextElement> textBuildNotice(static_cast<UI::TextElement*>(textElem));

    // --- Set parameter 0 = CurrentVersion, 1 = CurrentBuildId ---
    // ?CurrentVersion@Configuration@System@@ is a by-value static: its registered door
    // (RtSystem_Configuration_CurrentVersion) placement-constructs the result in the
    // caller's slot, which is what this does.
    {
        System::String versionStr(System::Configuration::CurrentVersion());
        textBuildNotice->SetParameter(0, versionStr);
    }

    // ?CurrentBuildId@ has no member yet (Configuration.h note 2): its body is the
    // retslot function, called by name on the caller's zeroed slot.
    {
        System::String buildIdStr;
        RtSystem_Configuration_CurrentBuildId(&buildIdStr);
        textBuildNotice->SetParameter(1, buildIdStr);
    }

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
// Original: 445 instructions. NOT the member OnOpened runs (MainMenuDialog.h,
// PORT GAP): the tree's OnOpened below is the slim standalone Setup.

void MainMenuDialog_Slot4(MainMenuDialog* self) {
    // --- Find all 13 buttons by name ---
    UI::Element* buttonNew         = FindElement(self, "buttonNew");
    UI::Element* buttonResume      = FindElement(self, "buttonResume");
    UI::Element* buttonLoad        = FindElement(self, "buttonLoad");
    UI::Element* buttonSave        = FindElement(self, "buttonSave");
    UI::Element* buttonOptions     = FindElement(self, "buttonOptions");
    UI::Element* buttonMenu        = FindElement(self, "buttonMenu");
    UI::Element* buttonQuit        = FindElement(self, "buttonQuit");
    UI::Element* buttonAchievement = FindElement(self, "buttonAchievement");
    UI::Element* buttonScenario    = FindElement(self, "buttonScenario");
    UI::Element* buttonGoal        = FindElement(self, "buttonGoal");
    UI::Element* buttonMap         = FindElement(self, "buttonMap");
    UI::Element* buttonTutorial    = FindElement(self, "buttonTutorial");
    UI::Element* buttonMod         = FindElement(self, "buttonMod");

    // Which buttons does this .rsc actually contain? A null here means the click can
    // never dispatch, because no element ever receives the id.
    fprintf(stderr, "[MainMenu] buttons: new=%p resume=%p load=%p save=%p options=%p menu=%p "
                    "quit=%p achv=%p scenario=%p goal=%p map=%p tutorial=%p mod=%p\n",
            buttonNew, buttonResume, buttonLoad, buttonSave,
            buttonOptions, buttonMenu, buttonQuit,
            buttonAchievement, buttonScenario, buttonGoal,
            buttonMap, buttonTutorial, buttonMod);
    fflush(stderr);

    // --- Assign element IDs (with CheckPtr safety) ---
    if (buttonNew) {
        buttonNew->SetElementId(kButton_New);
    }
    if (buttonResume) {
        buttonResume->SetElementId(kButton_Resume);
        // key index 8 (the button's hot key)
        static_cast<UI::ButtonElement*>(buttonResume)->_keyIndex = 8;
    }
    if (buttonOptions) {
        buttonOptions->SetElementId(kButton_Options);
    }
    if (buttonQuit) {
        buttonQuit->SetElementId(kButton_Quit);
    }
    if (buttonLoad) {
        buttonLoad->SetElementId(kButton_Load);
    }
    if (buttonSave) {
        buttonSave->SetElementId(kButton_Save);
    }
    if (buttonMenu) {
        buttonMenu->SetElementId(kButton_Menu);
    }
    if (buttonAchievement) {
        buttonAchievement->SetElementId(kButton_Achievement);
    }
    if (buttonScenario) {
        buttonScenario->SetElementId(kButton_Scenario);
    }
    if (buttonGoal) {
        buttonGoal->SetElementId(kButton_Goal);
    }
    if (buttonMap) {
        buttonMap->SetElementId(kButton_Map);
    }
    if (buttonTutorial) {
        buttonTutorial->SetElementId(kButton_Tutorial);
    }
    if (buttonMod) {
        buttonMod->SetElementId(kButton_Mod);
    }

    // --- If game is active: pause audio and conditionally hide buttons ---
    GameUpdateLoop* gameWorld = self->game;
    if (gameWorld != nullptr) {
        GameUpdateLoop* world = GameUpdateLoop::_singleton;
        // LEFT: GameUpdateLoop::m_audioScene is void* (W0 header).
        Audio::Scene* audioScene = reinterpret_cast<Audio::Scene*>(world->m_audioScene);

        audioScene->Pause(4, true);
        audioScene->Pause(3, true);
        audioScene->Pause(2, true);

        // The scenario index: if < 0, hide buttonGoal; else hide buttonMap
        int32_t scenarioIdx = world->m_scenarioIndex;
        if (scenarioIdx < 0) {
            if (buttonGoal) {
                buttonGoal->SetFlagsOnOff(1, 0); // hide
            }
        } else {
            if (buttonMap) {
                buttonMap->SetFlagsOnOff(1, 0); // hide
            }
        }

        // The game flags, bit 10: if set, disable save & mod buttons
        uint32_t gameFlags = gameWorld->m_gameFlags;
        if (gameFlags & (1 << 10)) {
            if (buttonSave) {
                buttonSave->Enable(0);   // slot 8
            }
            if (buttonMod) {
                buttonMod->Enable(0);
            }
        }
    }

    // --- Create BuildId child dialog (delegates to shared helper) ---
    CreateBuildIdChildDialog(self, self->_ui);
}

// ============================================================
// Slot5 (0x1400B3C60): Shutdown — unpause audio scenes
// ============================================================
// Called when the menu closes (returning to gameplay). NOT the member OnClosed
// runs (MainMenuDialog.h, PORT GAP).

void MainMenuDialog_Slot5(MainMenuDialog* self) {
    // Only unpause if a game is active
    if (self->game == nullptr)
        return;

    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    // LEFT: GameUpdateLoop::m_audioScene is void* (W0 header).
    Audio::Scene* audioScene = reinterpret_cast<Audio::Scene*>(world->m_audioScene);

    audioScene->Pause(4, false);
    audioScene->Pause(3, false);
    audioScene->Pause(2, false);
}

// (MainMenuDialog_Slot8, the C copy of retail's OnElementEvent 0x1400b3b50, is
// deleted: its dispatch is the member's below, which adds only the standalone
// harness wiring — the Mods opener's autostart hook, the Quit fallback and the
// log lines. No retail difference a player sees.)


// ============================================================
// MainMenuDialog as a real class (ENDGAME B.3.8 block DLG)
// ============================================================
// Dialog/MainMenuDialog.h carries the retail slot list. The live bodies below
// are the ones the old hand-built 20-slot table installed (its slots 10-19 were
// a phantom "DialogController secondary interface" retail does not have).
// Slots 6/7 are INHERITED: the old table called DialogInstance's bodies there
// with a qualified call, which is exactly what retail's import thunks reach.

#include "Object/factory_registration.h"

// [4] OnOpened — PORT GAP (header): slim standalone Setup. Mirrors the
// button-finding + element ID assignment portion of MainMenuDialog_Slot4
// (above) and the BuildId child dialog; skips the game-world audio block.
void MainMenuDialog::OnOpened() {
    printf("[MainMenuDialog::Setup] self=%p assigning button element IDs\n", (void*)this);

    auto findAndAssign = [this](const char* name, int id) {
        System::String nameStr(name);
        UI::Element* elem = FindElement(nameStr).Get();
        if (elem) {
            // CheckPtr resolves any lazy/weak pointer inside the IBP, then
            // SetElementId writes the int into the element.
            elem->SetElementId(id);
            printf("[Setup]   %-20s -> id=%2d (elem=%p)\n", name, id, (void*)elem);
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
    CreateBuildIdChildDialog(this, _ui);
}

// [5] OnClosed — PORT GAP (header): the tree's no-op; retail unpauses the
// audio scenes (MainMenuDialog_Slot5 above is that port).
void MainMenuDialog::OnClosed() {}

// [8] OnElementEvent — button click dispatch. The runtime sends msg=0x12 with
// the element ID as the second int.
static void StandaloneOpenMapParams_UI(UI::UserInterface* ui);  // defined below (map dialog bring-up)

void MainMenuDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                    int32_t msg, int32_t id, int32_t /*c*/,
                                    int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (msg != 0x12) return;
    printf("[MainMenuDialog::OnCommand] self=%p msg=0x%x id=%d\n", (void*)this, msg, id);

    switch (id) {
    case kButton_New:
        printf("[MainMenuDialog::OnCommand] -> OpenNewGame\n");
        OpenNewGame();
        break;
    case kButton_Resume: {
        // Resume = close this in-game menu, which removes it from the UI's
        // dialog list. UI::UserInterface::PausesGame then returns false and
        // GameUpdateLoop::Update resumes ticking the simulation.
        printf("[MainMenuDialog::OnCommand] -> Resume (CloseDialog)\n");
        Object::InternalPtr<UI::DialogInstance> selfPtr(this);
        _ui->CloseDialog(&selfPtr);
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
        // ReturnToMenu takes the game != 0 branch: destroy the GameUpdateLoop's
        // SaveLoad, build MenuDialogLoopT -> GameLoadDialogLoopT -> LoaderLoop,
        // fade to black, and park the chain in m_nextLoop (+0xC8).
        // GameUpdateLoop::Update step 17 picks that up once the fade completes
        // (UI+0xF0 == 0) and calls MainLoop::SetNextLoop, which runs the ported
        // teardown (gameupdateloop_teardown.cpp) on the way out.
        printf("[MainMenuDialog::OnCommand] -> ReturnToMenu (gameUpdateLoop=%p)\n",
               (void*)game);
        ReturnToMenu();
        break;
    case kButton_Options:
        printf("[MainMenuDialog::OnCommand] -> OpenOptions\n");
        OpenOptions();
        break;
    case kButton_Load:
        // Save/load wave 2026-08-27: SaveLoadDialog ctor + vtable are now
        // registered, the menu-side Save::SaveLoad exists (+0xb70) and is
        // ticked — the retail load-from-save path is standalone-ready.
        printf("[MainMenuDialog::OnCommand] -> OpenLoad (SaveLoadDialog)\n");
        OpenLoad();
        break;
    case kButton_Save:
        printf("[MainMenuDialog::OnCommand] -> OpenSave (SaveLoadDialog)\n");
        OpenSave();
        break;
    case kButton_Map:
        printf("[MainMenuDialog::OnCommand] -> OpenMap (MapParamsDialog)\n");
        StandaloneOpenMapParams_UI(_ui);
        break;
    case kButton_Tutorial:
        // The main menu .rsc has NO buttonScenario - the [Setup] log prints it
        // "NOT FOUND". Scenarios live behind the Tutorial button, which opens the same
        // ScenarioDialog with difficulty=1.
        printf("[MainMenuDialog::OnCommand] -> OpenTutorial (ScenarioDialog, difficulty=1)\n");
        OpenTutorial();
        break;
    case kButton_Scenario:
        printf("[MainMenuDialog::OnCommand] -> OpenScenario (ScenarioDialog, difficulty=0)\n");
        OpenScenario();
        break;
    case kButton_Achievement:
        // Wired 2026-09-05 (front end AND in-game pause menu, same dialog class).
        printf("[MainMenuDialog::OnCommand] -> OpenAchievement (AchievementDialog)\n");
        OpenAchievement();
        break;
    case kButton_Goal:
        printf("[MainMenuDialog::OnCommand] -> OpenGoal (GoalDialog)\n");
        OpenGoal();
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
static void StandaloneOpenMapParams_UI(UI::UserInterface* ui) {
    if (!ui) { printf("[OpenMap] ui null\n"); return; }
    if (!System::MemoryAllocator::_singleton) {   // B3R: the g_pMemAllocSingleton snapshot, read live
        printf("[OpenMap] core fns unresolved\n"); return;
    }
    UI::Dialog* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    if (!extRes) { printf("[OpenMap] Dialog/MapParams.rsc not loaded\n"); return; }
    void* dlgMem = System::MemoryAllocator::MemAlloc(sizeof(MapParamsDialog), 8);
    if (!dlgMem) return;
    MapParamsDialog* dlg = ::new (dlgMem) MapParamsDialog(
        ui, extRes, Object::InternalPtr<UI::DialogInstance>());
    Object::InternalPtr<UI::DialogInstance> ibp(dlg);
    ui->AddDialog(&ibp);
    printf("[OpenMap] MapParamsDialog opened + AddDialog'd (dlg=%p)\n", (void*)dlg);
}

// Harness entry — open the map dialog headlessly (the game's UserInterface).
extern "C" void MapParamsDialog_HarnessOpen() {
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    if (!gw) { printf("[OpenMap] gameWorld null\n"); return; }
    UI::UserInterface* ui = gw->m_userInterface;
    printf("[OpenMap] harness open: ui=%p\n", (void*)ui);
    StandaloneOpenMapParams_UI(ui);
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// Retail ctor 0x14009BA30 (from MenuDLT): DialogInstance::DialogInstance with
// (this, the loop's UserInterface, the loop's dialog resource, nullParent, 0, 0, 0),
// then the two MainMenuDialog fields:
//   game     = the running game (0 at the menu; set by the in-game pause path)
//   menuLoop = the OWNING LOOP (param_1 == the MenuDLT) — `*(this+0xc0) = param_1`.
// This wiring is load-bearing: StartNewGame reads menuLoop to decide its
// transition path. If it is 0, StartNewGame falls into the
// SetEngineNextLoop(game) branch and dereferences a NULL game. With menuLoop set
// it correctly takes SetDialogLoaderNextLoop(loop) and writes loop+0xb68.

MainMenuDialog::MainMenuDialog() : game(nullptr), menuLoop(nullptr) {}

MainMenuDialog::MainMenuDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                               Object::InternalPtr<UI::DialogInstance> parent,
                               GameUpdateLoop* gameLoop, MenuDialogLoopT* loop)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      game(gameLoop), menuLoop(loop) {}

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
// The loop's dialog resource is always loaded by then; a loop without one
// keeps the tree's bare-bones fallback (a default-constructed dialog, no element
// tree) instead of the Runtime ctor faulting on a null Dialog — a standalone-only
// guard retail does not have.
// B4 FORWARD: 1 outside caller (standalone/menudialogloopt_stubs.cpp), which
// holds the loop as its `this` and the return slot as void*.
extern "C" void* MainMenuDialog_FromMenuDLT(void* loop, void* retInternalBasePtr) {
    if (!System::MemoryAllocator::_singleton) {   // B3R: the g_pMemAllocSingleton snapshot, read live
        if (retInternalBasePtr) {
            ::new (retInternalBasePtr) Object::InternalPtr<UI::DialogInstance>();
        }
        return retInternalBasePtr;
    }
    void* mem = System::MemoryAllocator::MemAlloc(sizeof(MainMenuDialog), 8);
    MenuDialogLoopT* menuLoop = static_cast<MenuDialogLoopT*>(loop);
    // LEFT: DialogLoopT::_dialogExternal is void* (runtime Engine header).
    const UI::Dialog* rsc = menuLoop ? static_cast<const UI::Dialog*>(menuLoop->_dialogExternal) : nullptr;
    MainMenuDialog* dlg;
    if (rsc) {
        dlg = ::new (mem) MainMenuDialog(&menuLoop->_ui, rsc,
                                         Object::InternalPtr<UI::DialogInstance>(),
                                         nullptr, menuLoop);
    } else {
        printf("[MainMenuDialog_FromLoop] bare-bones init (loop=%p rsc=%p) — "
               "DialogInstance ctor skipped\n", loop, (const void*)rsc);
        dlg = ::new (mem) MainMenuDialog();
        dlg->menuLoop = menuLoop;
    }
    ::new (retInternalBasePtr) Object::InternalPtr<UI::DialogInstance>(dlg);
    return retInternalBasePtr;
}

DEFINE_EXE_TYPE(MainMenuDialog, "UI::DialogInstance")
