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

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Helper: find element by name, returns raw element pointer
// ============================================================
static void* FindElement(void* dialog, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(dialog, retBuf, nameStr);
    void* elem = *(void**)retBuf;
    fn_String_Dtor(nameStr);
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
static void* LoadDialogResource(const char* rscPath) {
    uint8_t filenameBuf[16];
    fn_Filename_CtorCStr(filenameBuf, rscPath);
    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    fn_Filename_Dtor(filenameBuf);
    return resource;
}

// ============================================================
// Helper: wrap dialog in InternalBasePtr and add via UI
// ============================================================
static void AddDialogToUI(MainMenuDialog* self, void* dlgObj) {
    uint8_t ibp[8];
    fn_InternalBasePtr_Ctor(ibp, dlgObj);
    fn_UI_AddDialog(*(void**)((uintptr_t)self + 0x10), ibp);
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
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xAE8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        fn_InternalBasePtr_Ctor(parentIBP, (void*)self);
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        // Ctor args: self, ui, extRes, nullParent, parentMenuIBP (stack arg)
        dlgObj = fn_NewGameDialog_Ctor(dlgMem,
                                        *(void**)((uintptr_t)self + 0x10),
                                        extRes, nullIBP, parentIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// OpenLoad (0x1400B3770): "Dialog/SaveLoad.rsc", size 0x160, mode=1 (load)
// Ctor: FUN_1400b8270(self, ui, extRes, nullParent, mode, enginePtr, gameWorldPtr)
void MainMenuDialog_OpenLoad(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_SaveLoadDialog_Ctor(dlgMem,
                                         *(void**)(s + 0x10),
                                         extRes, nullIBP,
                                         1,  // mode = load
                                         *(void**)(s + 0xC0),   // engine ptr
                                         *(void**)(s + 0xB8));  // game world ptr
    }
    AddDialogToUI(self, dlgObj);
}

// OpenSave (0x1400B38B0): "Dialog/SaveLoad.rsc", size 0x160, mode=0 (save)
void MainMenuDialog_OpenSave(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* extRes = LoadDialogResource("Dialog/SaveLoad.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_SaveLoadDialog_Ctor(dlgMem,
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
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0x1C0, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_OptionsDialog_Ctor(dlgMem,
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
            fn_SaveLoad_Dtor(saveLoad);
            fn_MemFree(g_pMemAllocSingleton, saveLoad);
        }
        *(void**)(engPtr + 0xB70) = nullptr;
    } else {
        // Game active: destroy SaveLoad at gameWorld+0x140
        void* saveLoad = *(void**)(gameWorld + 0x140);
        if (saveLoad) {
            fn_SaveLoad_Dtor(saveLoad);
            fn_MemFree(g_pMemAllocSingleton, saveLoad);
        }
        *(void**)(gameWorld + 0x140) = nullptr;
    }

    // Step 1: Create MenuDialogLoopT (0xC00 bytes) with "Dialog/MainMenu.rsc"
    void* menuLoop = nullptr;
    void* menuMem = fn_MemAlloc(g_pMemAllocSingleton, 0xC00, 8);
    if (menuMem) {
        uint8_t menuStr[8];
        fn_String_Ctor_CStr(menuStr, "Dialog/MainMenu.rsc");
        menuLoop = MenuDialogLoopT_Ctor(menuMem, menuStr);
        fn_String_Dtor(menuStr);
    }

    // Step 2: Create GameLoadDialogLoopT (0xB60 bytes) wrapping the menu loop
    void* dlLoop = nullptr;
    void* dlMem = fn_MemAlloc(g_pMemAllocSingleton, 0xB60, 4);
    if (dlMem) {
        uint8_t delayTime[8];
        fn_Time_Ctor_Float(delayTime, 1.0f);
        uint8_t loadingStr[8];
        fn_String_Ctor_CStr(loadingStr, "Dialog/Loading.rsc");
        fn_DialogLoaderLoopT_Ctor(dlMem, loadingStr, menuLoop, delayTime);
        *(void**)dlMem = (void*)g_GameLoadDialogLoopT_Vtable;
        *(uint32_t*)((uintptr_t)dlMem + 0x188) = 0xFF000000;
        *(uint32_t*)((uintptr_t)dlMem + 0x18C) = 0xFF000000;
        *(uint8_t*)((uintptr_t)dlMem + 400) = 0;
        fn_Random_Default_Ctor((void*)((uintptr_t)dlMem + 0x194));
        *(uint64_t*)((uintptr_t)dlMem + 0xB58) = 0;
        *(uint8_t*)((uintptr_t)dlMem + 0x40) = 0;
        fn_String_Dtor(loadingStr);
        dlLoop = dlMem;
    }

    // Step 3: Create outer LoaderLoop (0x48 bytes) wrapping the dialog loader
    void* outerLoop = nullptr;
    void* outerMem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
    if (outerMem) {
        uint8_t zeroTime[8];
        fn_Time_Ctor_Float(zeroTime, 0.0f);
        fn_LoaderLoop_Ctor(outerMem, dlLoop, zeroTime);
        *(void**)outerMem = (void*)g_GameLoaderLoop_Vtable;
        outerLoop = outerMem;
    }

    // Step 4: Set next loop on the appropriate target
    if (gameWorld == 0) {
        // No game: set on engine singleton (at +0xC0 = engine ptr)
        uintptr_t engPtr = *(uintptr_t*)(s + 0xC0);
        fn_UI_SetFullscreenFade((void*)(engPtr + 0x40), true, 0xFF000000, false);
        *(void**)(engPtr + 0xB68) = outerLoop;
    } else {
        // Game active: set cursor, fade, store at +0xC8
        void* ui = *(void**)(gameWorld + 0x98);
        uint64_t cursorVal = *(uint64_t*)((uintptr_t)(*g_pKeyMapSingleton) + 0x38);
        if (fn_UI_SetCursor) fn_UI_SetCursor(ui, cursorVal);
        fn_UI_SetFullscreenFade(ui, true, 0xFF000000, false);
        *(void**)(gameWorld + 0xC8) = outerLoop;
    }
}

// OpenAchievement (0x1400B34C0): "Dialog/Achievement.rsc", size 0xD0
// Uses DialogInstance_Ctor directly + vtable patch to PTR_FUN_14015b020.
// Extra fields: +0xB8, +0xC0, +0xC8 = 0.
void MainMenuDialog_OpenAchievement(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Achievement.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xD0, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        uint64_t parentBuf = *(uint64_t*)nullIBP;
        fn_DialogInstance_Ctor(dlgMem,
                                *(void**)((uintptr_t)self + 0x10),
                                extRes, &parentBuf, 0, 0, 0);
        *(void**)dlgMem = (void*)g_AchievementDialog_Vtable;
        *(uint64_t*)((uintptr_t)dlgMem + 0xB8) = 0;
        *(uint64_t*)((uintptr_t)dlgMem + 0xC0) = 0;
        *(uint64_t*)((uintptr_t)dlgMem + 0xC8) = 0;
        dlgObj = dlgMem;
    }
    AddDialogToUI(self, dlgObj);
}

// OpenScenario (0x1400B3380): "Dialog/Scenario.rsc", size 0xE8
// Ctor: FUN_1400786b0 (shared with TutorialDialog)
// Passes self as parent and difficulty=0 for scenarios from main menu.
void MainMenuDialog_OpenScenario(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Scenario.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xE8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        fn_InternalBasePtr_Ctor(parentIBP, (void*)self);
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_ScenarioDialog_Ctor(dlgMem,
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
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 200, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        uint64_t parentBuf = *(uint64_t*)nullIBP;
        fn_DialogInstance_Ctor(dlgMem,
                                *(void**)((uintptr_t)self + 0x10),
                                extRes, &parentBuf, 0, 0, 0);
        *(void**)dlgMem = (void*)g_GoalDialog_Vtable;
        *(uint64_t*)((uintptr_t)dlgMem + 0xB8) = 0;
        *(uint64_t*)((uintptr_t)dlgMem + 0xC0) = 0;
        dlgObj = dlgMem;
    }
    AddDialogToUI(self, dlgObj);
}

// OpenMap (0x1400B3120): "Dialog/MapParams.rsc", size 0xF8
// Ctor: FUN_14006bd80(self, ui, extRes, nullParent)
void MainMenuDialog_OpenMap(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xF8, 8);
    if (dlgMem) {
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_MapParamsDialog_Ctor(dlgMem,
                                          *(void**)((uintptr_t)self + 0x10),
                                          extRes, nullIBP);
    }
    AddDialogToUI(self, dlgObj);
}

// OpenTutorial (0x1400B2FE0): "Dialog/Tutorial.rsc", size 0xE8
// Uses same ctor as Scenario (FUN_1400786b0).
void MainMenuDialog_OpenTutorial(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Tutorial.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xE8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        fn_InternalBasePtr_Ctor(parentIBP, (void*)self);
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_ScenarioDialog_Ctor(dlgMem,
                                         *(void**)((uintptr_t)self + 0x10),
                                         extRes, nullIBP,
                                         parentIBP,
                                         0);  // difficulty
    }
    AddDialogToUI(self, dlgObj);
}

// OpenMod (0x1400B2EA0): "Dialog/Mod.rsc", size 0xD8
// Ctor: FUN_140070030(self, ui, extRes, parentIBP, parentMenuIBP)
void MainMenuDialog_OpenMod(MainMenuDialog* self) {
    void* extRes = LoadDialogResource("Dialog/Mod.rsc");
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xD8, 8);
    if (dlgMem) {
        uint8_t parentIBP[8];
        fn_InternalBasePtr_Ctor(parentIBP, (void*)self);
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_ModDialog_Ctor(dlgMem,
                                    *(void**)((uintptr_t)self + 0x10),
                                    extRes, nullIBP, parentIBP);
    }
    AddDialogToUI(self, dlgObj);
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

    // --- Assign element IDs (with CheckPtr safety) ---
    if (buttonNew) {
        fn_InternalBasePtr_CheckPtr(&buttonNew);
        fn_Element_SetElementId(buttonNew, kButton_New);
    }
    if (buttonResume) {
        fn_InternalBasePtr_CheckPtr(&buttonResume);
        fn_Element_SetElementId(buttonResume, kButton_Resume);
        // Set +0xC0 flag to 8 (command dispatch flag)
        fn_InternalBasePtr_CheckPtr(&buttonResume);
        *(uint32_t*)((uintptr_t)buttonResume + 0xC0) = 8;
    }
    if (buttonOptions) {
        fn_InternalBasePtr_CheckPtr(&buttonOptions);
        fn_Element_SetElementId(buttonOptions, kButton_Options);
    }
    if (buttonQuit) {
        fn_InternalBasePtr_CheckPtr(&buttonQuit);
        fn_Element_SetElementId(buttonQuit, kButton_Quit);
    }
    if (buttonLoad) {
        fn_InternalBasePtr_CheckPtr(&buttonLoad);
        fn_Element_SetElementId(buttonLoad, kButton_Load);
    }
    if (buttonSave) {
        fn_InternalBasePtr_CheckPtr(&buttonSave);
        fn_Element_SetElementId(buttonSave, kButton_Save);
    }
    if (buttonMenu) {
        fn_InternalBasePtr_CheckPtr(&buttonMenu);
        fn_Element_SetElementId(buttonMenu, kButton_Menu);
    }
    if (buttonAchievement) {
        fn_InternalBasePtr_CheckPtr(&buttonAchievement);
        fn_Element_SetElementId(buttonAchievement, kButton_Achievement);
    }
    if (buttonScenario) {
        fn_InternalBasePtr_CheckPtr(&buttonScenario);
        fn_Element_SetElementId(buttonScenario, kButton_Scenario);
    }
    if (buttonGoal) {
        fn_InternalBasePtr_CheckPtr(&buttonGoal);
        fn_Element_SetElementId(buttonGoal, kButton_Goal);
    }
    if (buttonMap) {
        fn_InternalBasePtr_CheckPtr(&buttonMap);
        fn_Element_SetElementId(buttonMap, kButton_Map);
    }
    if (buttonTutorial) {
        fn_InternalBasePtr_CheckPtr(&buttonTutorial);
        fn_Element_SetElementId(buttonTutorial, kButton_Tutorial);
    }
    if (buttonMod) {
        fn_InternalBasePtr_CheckPtr(&buttonMod);
        fn_Element_SetElementId(buttonMod, kButton_Mod);
    }

    // --- If game is active: pause audio and conditionally hide buttons ---
    uint64_t gameWorld = *(uint64_t*)(s + 0xB8);
    if (gameWorld != 0) {
        void* world = *g_pGameWorldSingleton;
        void* audioScene = *(void**)((uintptr_t)world + 0x78);

        fn_Audio_Scene_Pause(audioScene, 4, true);
        fn_Audio_Scene_Pause(audioScene, 3, true);
        fn_Audio_Scene_Pause(audioScene, 2, true);

        // Check scenario index at world+0x15C. If < 0, hide buttonGoal; else hide buttonMap
        int32_t scenarioIdx = *(int32_t*)((uintptr_t)world + 0x15C);
        if (scenarioIdx < 0) {
            if (buttonGoal) {
                fn_InternalBasePtr_CheckPtr(&buttonGoal);
                fn_Element_SetFlagsOnOff(buttonGoal, 1, 0); // hide
            }
        } else {
            if (buttonMap) {
                fn_InternalBasePtr_CheckPtr(&buttonMap);
                fn_Element_SetFlagsOnOff(buttonMap, 1, 0); // hide
            }
        }

        // Check game flags at gameWorld+0xD0, bit 10: if set, hide save & mod buttons
        uint32_t gameFlags = *(uint32_t*)(gameWorld + 0xD0);
        if (gameFlags & (1 << 10)) {
            if (buttonSave) {
                fn_InternalBasePtr_CheckPtr(&buttonSave);
                // Call vtable[8] = SetEnabled(false)
                void** vtable = *(void***)buttonSave;
                typedef void (*fn_SetEnabled_t)(void*, int);
                ((fn_SetEnabled_t)vtable[8])(buttonSave, 0);
            }
            if (buttonMod) {
                fn_InternalBasePtr_CheckPtr(&buttonMod);
                void** vtable = *(void***)buttonMod;
                typedef void (*fn_SetEnabled_t)(void*, int);
                ((fn_SetEnabled_t)vtable[8])(buttonMod, 0);
            }
        }
    }

    // --- Create BuildId child dialog ---
    // Load "Dialog/BuildId.rsc" resource
    uint8_t filenameStr[8];
    fn_String_Ctor_CStr(filenameStr, "Dialog/BuildId.rsc");
    fn_Filename_AdjustDelimiters(filenameStr);

    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameStr);
    void* dialogResource = nullptr;
    if (resource) {
        if (fn_IsDerivedFrom(resource, *g_pExternalTypeId)) {
            dialogResource = resource;
        }
    }
    fn_String_Dtor(filenameStr);

    // Allocate and construct a new DialogInstance (0xB8 bytes) for the BuildId child
    void* childDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    void* childPtr = nullptr;
    if (childDialog) {
        // Build InternalBasePtr for parent (self)
        uint8_t parentPtr[16];
        fn_InternalBasePtr_Ctor(parentPtr, (void*)self);

        // Call DialogInstance constructor
        fn_DialogInstance_Ctor(childDialog,
                               *(void**)(s + 0x10),   // UserInterface ptr
                               dialogResource,         // .rsc resource
                               parentPtr,              // parent dialog ptr
                               0, 0, 0);

        // Override vtable to BuildId secondary vtable
        *(void**)childDialog = (void*)g_MainMenuDialog_BuildIdVtable;
        childPtr = childDialog;
    }

    // Add child dialog via InternalPtr
    uint8_t childInternalPtr[16];
    fn_InternalBasePtr_Ctor(childInternalPtr, childPtr);
    uint8_t dialogRef[8];
    *(void**)dialogRef = *(void**)childInternalPtr;
    fn_UI_AddDialog(*(void**)(s + 0x10), dialogRef);

    // --- Find textBuildNotice in child dialog, set version & build text ---
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "textBuildNotice");

    fn_InternalBasePtr_CheckPtr(childInternalPtr);
    void* childDI = *(void**)childInternalPtr;
    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(childDI, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);
    void* textElem = *(void**)retBuf;

    // IsDerivedFrom check to safely cast to TextElement
    void* textBuildNotice = nullptr;
    if (textElem && fn_IsDerivedFrom(textElem, *g_pExternalTypeId)) {
        textBuildNotice = textElem;
    }

    // Build InternalBasePtr for the text element
    uint8_t textElemPtr[16];
    fn_InternalBasePtr_Ctor(textElemPtr, textBuildNotice);
    fn_String_Dtor(nameStr);

    // Set parameter 0 = CurrentVersion, parameter 1 = CurrentBuildId
    if (textBuildNotice) {
        uint8_t versionStr[8];
        fn_Configuration_CurrentVersion(versionStr);
        fn_InternalBasePtr_CheckPtr(textElemPtr);
        fn_TextElement_SetParameter(*(void**)textElemPtr, 0, versionStr);
        fn_String_Dtor(versionStr);

        uint8_t buildIdStr[8];
        fn_Configuration_CurrentBuildId(buildIdStr);
        fn_InternalBasePtr_CheckPtr(textElemPtr);
        fn_TextElement_SetParameter(*(void**)textElemPtr, 1, buildIdStr);
        fn_String_Dtor(buildIdStr);
    }
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

    void* world = *g_pGameWorldSingleton;
    void* audioScene = *(void**)((uintptr_t)world + 0x78);

    fn_Audio_Scene_Pause(audioScene, 4, false);
    fn_Audio_Scene_Pause(audioScene, 3, false);
    fn_Audio_Scene_Pause(audioScene, 2, false);
}

// ============================================================
// Slot8 (0x1400B3B50): OnCommand — handle menu button clicks
// ============================================================
// Dispatches button clicks based on element ID (param4) when
// the command type (param3) is 0x12 (button click event).
// Each handler opens a sub-dialog or performs an action.
//
// Private handler functions are now reimplemented above as clean C++.

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
        fn_InternalBasePtr_Ctor(selfPtr, (void*)self);
        fn_UI_CloseDialog(*(void**)(s + 0x10), selfPtr);
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
        fn_App_Quit(*g_pAppSingleton);
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
