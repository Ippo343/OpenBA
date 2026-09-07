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
#include "resolve.h"
#include "Dialog/dialog_exe_helpers.h"
#include <cstdint>
#include <cstdio>
#include <windows.h>  // PostQuitMessage fallback in standalone Quit handler

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
// Exe data slot at 0x140151728. The exe loader patches this with the 4-byte
// type ID for UI::Dialog (NOT DialogInstance — Ghidra mislabeled). Used by
// LoadDialogResource to validate Dialog/*.rsc resources before construction.
static uint32_t* g_dialogTypeIdSlot = nullptr;

static void* LoadDialogResource(const char* rscPath) {
    uint8_t filenameBuf[16];
    fn_Filename_CtorCStr(filenameBuf, rscPath);
    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    printf("[LoadDialogResource] '%s' -> FindOrLoad=%p\n", rscPath, resource);
    void* extRes = nullptr;
    if (resource) {
        // The slot is read as a direct uint32_t in the original (single deref).
        if (!g_dialogTypeIdSlot) {
            g_dialogTypeIdSlot = reinterpret_cast<uint32_t*>(
                ResolveAddr(Module::Exe, 0x140151728));
        }
        uint32_t typeId = (g_dialogTypeIdSlot ? *g_dialogTypeIdSlot : 0);
        if (typeId == 0 && fn_Factory_GetTypeIdFromName &&
            g_pFactorySingleton && *g_pFactorySingleton) {
            // Standalone: slot wasn't filled by an exe loader, so look up
            // UI::Dialog via the Runtime factory. g_pFactorySingleton is a
            // void** pointing into the slot storage; *g points at the
            // Runtime DLL's _singleton variable; **g is the actual Factory*.
            void* factoryObj = *(void**)*g_pFactorySingleton;
            if (factoryObj) {
                typeId = fn_Factory_GetTypeIdFromName(factoryObj,
                                                      "Dialog", true);
                if (g_dialogTypeIdSlot) *g_dialogTypeIdSlot = typeId;
            }
        }
        bool derived = (typeId && fn_IsDerivedFrom(resource, typeId));
        printf("[LoadDialogResource] '%s' typeId=%u IsDerivedFrom=%d\n",
               rscPath, typeId, derived ? 1 : 0);
        if (derived) {
            extRes = resource;
        }
    }
    fn_Filename_Dtor(filenameBuf);
    return extRes;
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
            fn_MemFreeStatic(saveLoad);
        }
        *(void**)(engPtr + 0xB70) = nullptr;
    } else {
        // Game active: destroy SaveLoad at gameWorld+0x140
        void* saveLoad = *(void**)(gameWorld + 0x140);
        if (saveLoad) {
            fn_SaveLoad_Dtor(saveLoad);
            fn_MemFreeStatic(saveLoad);
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
    // Retail fills the Game container (and hands slot [0] to the AchievementManager)
    // once at app init, in GameContainer_Init. Standalone has no master ExternalList,
    // so that init is replaced by GameContainer_EnsureLists, which the New Game and
    // Scenario dialogs already call on open. The Achievements dialog needs the same
    // guarantee: opened straight from the front end it is the FIRST thing to touch the
    // achievement list, and its Setup calls GetAchievementCount unguarded.
    GameContainer_EnsureLists();

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
    printf("[MainMenuDialog] OpenTutorial called, self=%p\n", self);
    void* extRes = LoadDialogResource("Dialog/Tutorial.rsc");
    printf("[MainMenuDialog] OpenTutorial: extRes=%p\n", extRes);
    void* dlgObj = nullptr;
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xE8, 8);
    printf("[MainMenuDialog] OpenTutorial: dlgMem=%p\n", dlgMem);
    if (dlgMem) {
        uint8_t parentIBP[8];
        fn_InternalBasePtr_Ctor(parentIBP, (void*)self);
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);
        dlgObj = fn_ScenarioDialog_Ctor(dlgMem,
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
// CreateBuildIdChildDialog
// ============================================================
// Loads "Dialog/BuildId.rsc", constructs a child DialogInstance, attaches it
// to the parent's UI, finds "textBuildNotice" inside, and sets parameters
// 0=CurrentVersion and 1=CurrentBuildId on the TextElement.
//
// Mirrors the bottom half of MainMenuDialog_Slot4 (exe @ 0x1400b3cd0) so it
// can be reused from the standalone Slot4 stub. Returns true on success.
//
// Standalone gotcha: g_MainMenuDialog_BuildIdVtable resolves via the exe data
// section (0x14015f5a0). In standalone mode there's no exe loader to fill it,
// so it stays null. When null we skip the vtable override and let the default
// DialogInstance vtable handle Dtor/etc. (BuildId child has no overridden
// behavior beyond display.)
//
// Returns true if the child dialog was created and added to the UI.
static bool CreateBuildIdChildDialog(void* selfDialog, void* userInterfacePtr) {
    if (!selfDialog || !userInterfacePtr) {
        printf("[BuildId] selfDialog or UI null, skipping\n");
        return false;
    }
    if (!fn_String_Ctor_CStr || !fn_Filename_AdjustDelimiters ||
        !fn_ExternalFactory_FindOrLoad || !fn_IsDerivedFrom ||
        !fn_String_Dtor || !fn_MemAlloc || !fn_InternalBasePtr_Ctor ||
        !fn_DialogInstance_Ctor || !fn_UI_AddDialog ||
        !fn_DialogInstance_FindElement || !fn_InternalBasePtr_CheckPtr ||
        !fn_TextElement_SetParameter || !fn_Configuration_CurrentVersion ||
        !fn_Configuration_CurrentBuildId) {
        printf("[BuildId] missing engine fns, skipping\n");
        return false;
    }
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !g_pExternalTypeId || !g_pMemAllocSingleton) {
        printf("[BuildId] missing engine singletons, skipping\n");
        return false;
    }

    // --- Load Dialog/BuildId.rsc as External resource ---
    uint8_t filenameStr[16] = {};
    fn_String_Ctor_CStr(filenameStr, "Dialog/BuildId.rsc");
    fn_Filename_AdjustDelimiters(filenameStr);
    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton,
                                                    filenameStr);
    void* dialogResource = nullptr;
    if (resource && fn_IsDerivedFrom(resource, *g_pExternalTypeId)) {
        dialogResource = resource;
    }
    fn_String_Dtor(filenameStr);
    if (!dialogResource) {
        printf("[BuildId] Dialog/BuildId.rsc not found / wrong type, skipping\n");
        return false;
    }

    // --- Allocate + construct child DialogInstance (0xB8 bytes) ---
    void* childDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    if (!childDialog) {
        printf("[BuildId] MemAlloc(0xB8) failed\n");
        return false;
    }

    uint8_t parentPtr[16] = {};
    fn_InternalBasePtr_Ctor(parentPtr, selfDialog);

    fn_DialogInstance_Ctor(childDialog,
                           userInterfacePtr,
                           dialogResource,
                           parentPtr,
                           0, 0, 0);

    // Override vtable to BuildId secondary when that vtable is available; in
    // standalone the slot is null so we keep the default DI vtable from the ctor.
    if (g_MainMenuDialog_BuildIdVtable) {
        *(void**)childDialog = (void*)g_MainMenuDialog_BuildIdVtable;
    }

    // --- Add child to UI ---
    uint8_t childInternalPtr[16] = {};
    fn_InternalBasePtr_Ctor(childInternalPtr, childDialog);
    fn_UI_AddDialog(userInterfacePtr, childInternalPtr);

    // --- Find textBuildNotice inside the child dialog ---
    uint8_t nameStr[16] = {};
    fn_String_Ctor_CStr(nameStr, "textBuildNotice");

    fn_InternalBasePtr_CheckPtr(childInternalPtr);
    void* childDI = *(void**)childInternalPtr;
    uint8_t retBuf[16] = {};
    fn_DialogInstance_FindElement(childDI, retBuf, nameStr);
    fn_InternalBasePtr_CheckPtr(retBuf);
    void* textElem = *(void**)retBuf;
    fn_String_Dtor(nameStr);

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
    fn_InternalBasePtr_Ctor(textElemPtr, textBuildNotice);

    // --- Set parameter 0 = CurrentVersion, 1 = CurrentBuildId ---
    // Both Configuration:: helpers return UI::String via hidden return ptr ABI.
    uint8_t versionStr[16] = {};
    fn_Configuration_CurrentVersion(versionStr);
    fn_InternalBasePtr_CheckPtr(textElemPtr);
    fn_TextElement_SetParameter(*(void**)textElemPtr, 0, versionStr);
    fn_String_Dtor(versionStr);

    uint8_t buildIdStr[16] = {};
    fn_Configuration_CurrentBuildId(buildIdStr);
    fn_InternalBasePtr_CheckPtr(textElemPtr);
    fn_TextElement_SetParameter(*(void**)textElemPtr, 1, buildIdStr);
    fn_String_Dtor(buildIdStr);

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


// ============================================================
// MainMenuDialog standalone scaffolding
// ============================================================
// Provides a 20-slot vtable + factory + REGISTER_TYPE for standalone exe so
// MenuDLT::CreateDialog can produce a real MainMenuDialog instance instead of
// returning a null IBP.
//
// Vtable layout (exe @ 0x14015f550, 20 slots):
//   +0x00 Dtor             — sets vtable, ~DialogInstance, free
//   +0x08 Slot1            — exe returns DAT_1401c5c28 (TypeName const)
//   +0x10 Slot2            — exe returns DAT_1401c5c20 (TypeId const)
//   +0x18 Slot3 Persist    — thunk to Runtime DialogInstance::Persist
//   +0x20 Slot4 Setup      — REIMPL'd in MainMenuDialog_Slot4 (stubbed here)
//   +0x28 Slot5 Shutdown   — REIMPL'd (stubbed here)
//   +0x30 Slot6 OnUpdateLayout — thunk
//   +0x38 Slot7 OnUpdate   — thunk
//   +0x40 Slot8 OnCommand  — REIMPL'd (stubbed here)
//   +0x48 Slot9 OnSerializeSettings — thunk
//   +0x50 Slot10 DIDtor    — ~DialogInstance + free
//   +0x58..+0x98 Slot11-19 — DialogController interface, all thunks
//
// First pass: every slot is a no-op stub. Once Step 3 (rendering) lands and
// the resource/loader pipeline is alive, the four reimpls (Slot4/5/8/Dtor)
// can swap in. For now we just need the IBP non-null and Open/Close not to
// crash through a null vtable slot.

#include "Object/factory_registration.h"
#include "address_registry.h"

namespace {

// Used by Stub_Dtor / Stub_DIDtor to reset the vtable.
void* GetMainMenuDialogVtable();  // forward decl

// Slot 0 — MainMenuDialog::Dtor. Sets vtable, calls DialogInstance::~DialogInstance,
// then conditionally frees memory.
void* StandaloneStub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetMainMenuDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot 1 — TypeName placeholder. Exe returns a static value populated by the
// type registration system. Standalone returns 0 for now (no callers in current
// boot path).
uint64_t StandaloneStub_Slot1_TypeName(void* /*self*/) { return 0; }

// Slot 2 — TypeId placeholder.
uint32_t StandaloneStub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MainMenuDialog");
    return cached;
}

// Generic logging stub for the runtime-thunk slots (Slot3/6/7/9/11-19).
// Most are DialogInstance/DialogController virtuals that the boot path may
// invoke during Open/Update. Logged once each so we know when wiring matters.
template <int SlotIdx>
void StandaloneStub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[MainMenuDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
               SlotIdx, self);
    }
}

// Slot 4 (Setup) — slim standalone version. Mirrors the button-finding + element
// ID assignment portion of MainMenuDialog_Slot4 (above). Skips the game-world
// audio block (no game active in main menu) and the BuildId child dialog
// (cosmetic version overlay). What's left is just enough to make the OnCommand
// dispatch work when the user clicks a button.
void StandaloneStub_Slot4_Setup(void* self) {
    if (!fn_String_Ctor_CStr || !fn_String_Dtor || !fn_DialogInstance_FindElement ||
        !fn_InternalBasePtr_CheckPtr || !fn_Element_SetElementId) {
        printf("[MainMenuDialog::Setup] missing engine fns, skipping element ID assign\n");
        return;
    }
    printf("[MainMenuDialog::Setup] self=%p assigning button element IDs\n", self);

    auto findAndAssign = [self](const char* name, int id) {
        uint8_t nameStr[8] = {};
        fn_String_Ctor_CStr(nameStr, name);
        // FindElement uses non-trivial return ABI: RCX=self, RDX=retbuf, R8=name
        uint8_t retBuf[16] = {};
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *reinterpret_cast<void**>(retBuf);
        fn_String_Dtor(nameStr);
        if (elem) {
            // CheckPtr resolves any lazy/weak pointer inside the IBP, then
            // SetElementId writes the int into the element.
            fn_InternalBasePtr_CheckPtr(&elem);
            fn_Element_SetElementId(elem, id);
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
    void* userInterface = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(self) + 0x10);
    CreateBuildIdChildDialog(self, userInterface);
}

// Slot 5 (Shutdown) — depends on game world; stubbed.
void StandaloneStub_Slot5_Shutdown(void* /*self*/) {}

// Slot 6 (OnUpdateLayout) — DialogInstance virtual @ vtable +0x30.
// In the original exe this slot points at a 6-byte JMP-thunk
// (0x14003a98a -> [0x1401517b0]) that lands in
// UI::DialogInstance::OnUpdateLayout @ Runtime DLL 0x1801153a0:
//   for (int i = 0; i < count(+0x80); ++i)
//       children[+0x78][i]->vtable[+0x38](child);   // recurse
// MainMenuDialog itself doesn't override — it just inherits the base impl.
// Standalone delegates straight to the Runtime DLL function so child
// elements (buttons, labels, panels) get their layout updated each frame.
void StandaloneImpl_Slot6_OnUpdateLayout(void* self) {
    if (!self) return;
    if (fn_DialogInstance_OnUpdateLayout) {
        fn_DialogInstance_OnUpdateLayout(self);
        return;
    }
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[MainMenuDialog::Slot6] fn_DialogInstance_OnUpdateLayout NULL (mangled name?)\n");
    }
}

// Slot 7 (OnUpdate) — DialogInstance virtual @ vtable +0x38.
// In the original exe this slot points at a 6-byte JMP-thunk
// (0x14003a990 -> [0x1401517b8]) that lands in
// UI::DialogInstance::OnUpdate @ Runtime DLL 0x180115400:
//   for (int i = 0; i < count(+0x80); ++i)
//       children[+0x78][i]->vtable[+0x40](child, time);   // per-frame tick
// Same pattern as Slot6 — delegate to Runtime DLL so animated/dynamic UI
// elements (buildId scroller, hover effects, etc.) get their update tick.
void StandaloneImpl_Slot7_OnUpdate(void* self, const void* time) {
    if (!self) return;
    if (fn_DialogInstance_OnUpdate) {
        fn_DialogInstance_OnUpdate(self, time);
        return;
    }
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[MainMenuDialog::Slot7] fn_DialogInstance_OnUpdate NULL (mangled name?)\n");
    }
}

// Slot 8 (OnCommand) — button click dispatch. The runtime sends msg=0x12 with
// param4 = element ID. We mirror the switch from MainMenuDialog_Slot8 above,
// but for now only Quit is wired (the others need sub-dialog scaffolding that
// isn't standalone-ready yet — they'll just log a "not wired" warning so we
// can see clicks register).
void StandaloneOpenMapParams(void* self);  // defined below (map dialog bring-up)

extern "C" void ModDialog_OpenFromMainMenu(void* mainMenuSelf);

void StandaloneStub_Slot8_OnCommand(void* self, void* /*p2*/, int msg, int id) {
    if (msg != 0x12) return;
    printf("[MainMenuDialog::OnCommand] self=%p msg=0x%x id=%d\n", self, msg, id);

    switch (id) {
    case kButton_New:
        printf("[MainMenuDialog::OnCommand] -> OpenNewGame\n");
        MainMenuDialog_OpenNewGame(reinterpret_cast<MainMenuDialog*>(self));
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
        fn_InternalBasePtr_Ctor(selfPtr, self);
        fn_UI_CloseDialog(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x10),
                          selfPtr);
        break;
    }
    case kButton_Quit:
        printf("[MainMenuDialog::OnCommand] Quit: fn_App_Quit=%p g_pAppSingleton=%p *g=%p\n",
               (void*)fn_App_Quit, (void*)g_pAppSingleton,
               g_pAppSingleton ? *g_pAppSingleton : nullptr);
        if (fn_App_Quit && g_pAppSingleton && *g_pAppSingleton) {
            printf("[MainMenuDialog::OnCommand] -> App::Quit\n");
            fn_App_Quit(*g_pAppSingleton);
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
               *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0xB8));
        MainMenuDialog_ReturnToMenu(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Options:
        printf("[MainMenuDialog::OnCommand] -> OpenOptions\n");
        MainMenuDialog_OpenOptions(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Load:
        // Save/load wave 2026-08-27: SaveLoadDialog ctor + vtable are now
        // registered, the menu-side Save::SaveLoad exists (+0xb70) and is
        // ticked — the retail load-from-save path is standalone-ready.
        printf("[MainMenuDialog::OnCommand] -> OpenLoad (SaveLoadDialog)\n");
        MainMenuDialog_OpenLoad(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Save:
        printf("[MainMenuDialog::OnCommand] -> OpenSave (SaveLoadDialog)\n");
        MainMenuDialog_OpenSave(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Map:
        printf("[MainMenuDialog::OnCommand] -> OpenMap (MapParamsDialog)\n");
        StandaloneOpenMapParams(self);
        break;
    case kButton_Tutorial:
        // The main menu .rsc has NO buttonScenario - the [Setup] log prints it
        // "NOT FOUND". Scenarios live behind the Tutorial button, which opens the same
        // ScenarioDialog with difficulty=1. Neither id was in this switch, so a click
        // logged "not yet wired" and did nothing (the retail-shaped MainMenuDialog_Slot8
        // above HAS both cases, but it is not the handler the vtable installs).
        // Wired 2026-08-30 so LANE1 row 42 is reachable by hand.
        printf("[MainMenuDialog::OnCommand] -> OpenTutorial (ScenarioDialog, difficulty=1)\n");
        MainMenuDialog_OpenTutorial(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Scenario:
        printf("[MainMenuDialog::OnCommand] -> OpenScenario (ScenarioDialog, difficulty=0)\n");
        MainMenuDialog_OpenScenario(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Achievement:
        // Wired 2026-09-05. The retail-shaped MainMenuDialog_Slot8 above has had this
        // case all along, but that switch is NOT the handler the vtable installs --
        // so the Achievements button (front end AND in-game pause menu, same dialog
        // class) silently fell through to `default` and logged "not yet wired".
        // AchievementDialog's vtable is registered at its retail .rdata address by
        // RegisterMenuDialogVtables (menu_dialog_standalone_vtables.cpp) and all four
        // of its slots are ported, so the opener works as-is.
        printf("[MainMenuDialog::OnCommand] -> OpenAchievement (AchievementDialog)\n");
        MainMenuDialog_OpenAchievement(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Goal:
        // Same story as Achievement: present in the dead retail switch, missing here.
        // GoalDialog's vtable comes from the same registration and its slots are ported.
        printf("[MainMenuDialog::OnCommand] -> OpenGoal (GoalDialog)\n");
        MainMenuDialog_OpenGoal(reinterpret_cast<MainMenuDialog*>(self));
        break;
    case kButton_Mod:
        // ModDialog/ModDetailsDialog live in code/Game/Dialog/mod_dialog_vtables.cpp.
        // Default-on since 2026-09-05 (BANISHED_LIVE_MODDIALOG=0 is the kill-switch).
        printf("[MainMenuDialog::OnCommand] -> OpenMods (ModDialog)\n");
        ModDialog_OpenFromMainMenu(self);
        break;
    default:
        printf("[MainMenuDialog::OnCommand] button id=%d not yet wired in standalone\n", id);
        break;
    }
}

// Slot 10 — DialogInstance::~DialogInstance + conditional free.
void* StandaloneStub_Slot10_DIDtor(void* self, uint32_t flags) {
    if (!self) return self;
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void* g_MainMenuDialog_Vtable_Standalone[20] = {
    reinterpret_cast<void*>(&StandaloneStub_Dtor),                   // +0x00 Dtor
    reinterpret_cast<void*>(&StandaloneStub_Slot1_TypeName),         // +0x08 Slot1
    reinterpret_cast<void*>(&StandaloneStub_Slot2_TypeId),           // +0x10 Slot2
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<3>),        // +0x18 Persist
    reinterpret_cast<void*>(&StandaloneStub_Slot4_Setup),            // +0x20 Setup
    reinterpret_cast<void*>(&StandaloneStub_Slot5_Shutdown),         // +0x28 Shutdown
    reinterpret_cast<void*>(&StandaloneImpl_Slot6_OnUpdateLayout),   // +0x30 OnUpdateLayout
    reinterpret_cast<void*>(&StandaloneImpl_Slot7_OnUpdate),         // +0x38 OnUpdate
    reinterpret_cast<void*>(&StandaloneStub_Slot8_OnCommand),        // +0x40 OnCommand
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<9>),        // +0x48 OnSerializeSettings
    reinterpret_cast<void*>(&StandaloneStub_Slot10_DIDtor),          // +0x50 DI Dtor
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<11>),       // +0x58 TypeName (sec)
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<12>),       // +0x60 Type (sec)
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<13>),       // +0x68 Persist (sec)
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<14>),       // +0x70 OnOpened
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<15>),       // +0x78 OnClosed
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<16>),       // +0x80 OnUpdateLayout (sec)
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<17>),       // +0x88 OnUpdate (sec)
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<18>),       // +0x90 OnElementEvent
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<19>),       // +0x98 OnSerializeSettings (sec)
};

void* GetMainMenuDialogVtable() {
    return reinterpret_cast<void*>(g_MainMenuDialog_Vtable_Standalone);
}

// ============================================================
// MapParamsDialog standalone bring-up — in-game "Map" button (id 11)
// ============================================================
// buttonMap → MainMenuDialog_OpenMap → Dialog/MapParams.rsc (the map name/seed/
// terrain/climate/condition/disaster info dialog). MapParamsDialog is a no-op
// stub in standalone, so give it a real 20-slot vtable that wires its already-
// reimplemented slots (Startup +0x20, Shutdown +0x28, Update +0x30, OnEvent
// +0x40; exe vtable 0x14015c180) + reuses the shared DialogInstance base stubs.
// StandaloneOpenMapParams mirrors MainMenuDialog_OpenGoal: DialogInstance_Ctor
// (builds the element tree + AddController) → patch vtable → AddDialog (which
// dispatches the dialog's +0x20 Startup). All fn deps are already resolved.
// MapParamsDialog + its Startup/Slot5/Update/OnEvent are declared globally in
// Dialog/ui_dialogs.h (included above). Reference them via :: so unqualified
// lookup inside this anonymous namespace resolves to the real global symbols.
extern void* g_MapParamsDialog_Vtable_Standalone[20];

static void* MP_VDtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_MapParamsDialog_Vtable_Standalone;
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}
static uint32_t MP_VTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("MapParamsDialog");
    return c;
}
static void MP_VStartup(void* self) {
    printf("[MapParamsDialog::Startup] self=%p\n", self);
    if (!self) return;
    MapParamsDialog_Startup(reinterpret_cast<MapParamsDialog*>(self));
    // Startup stores the found element ptrs at +0xB8..+0xF0; non-null => the
    // dialog's element tree was built + the named elements resolved (content).
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    printf("[MapParamsDialog::Startup] elems editName=%p seed=%p terrain=%p size=%p "
           "climate=%p condition=%p disaster=%p ok=%p\n",
           *reinterpret_cast<void**>(s + 0xB8), *reinterpret_cast<void**>(s + 0xC0),
           *reinterpret_cast<void**>(s + 0xC8), *reinterpret_cast<void**>(s + 0xD0),
           *reinterpret_cast<void**>(s + 0xD8), *reinterpret_cast<void**>(s + 0xE0),
           *reinterpret_cast<void**>(s + 0xE8), *reinterpret_cast<void**>(s + 0xF0));
}
static int  MP_VSlot5(void* self) {
    return self ? MapParamsDialog_Slot5(reinterpret_cast<MapParamsDialog*>(self)) : 0;
}
static void MP_VUpdate(void* self) {
    if (self) MapParamsDialog_Update(reinterpret_cast<MapParamsDialog*>(self));
}
static void MP_VOnEvent(void* self, void* elem, int t, int d) {
    if (self) MapParamsDialog_OnEvent(reinterpret_cast<MapParamsDialog*>(self), elem, t, d);
}

void* g_MapParamsDialog_Vtable_Standalone[20] = {
    reinterpret_cast<void*>(&MP_VDtor),                          // +0x00 Dtor
    reinterpret_cast<void*>(&StandaloneStub_Slot1_TypeName),     // +0x08 TypeName
    reinterpret_cast<void*>(&MP_VTypeId),                        // +0x10 GetTypeId
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<3>),    // +0x18 Persist
    reinterpret_cast<void*>(&MP_VStartup),                       // +0x20 Startup
    reinterpret_cast<void*>(&MP_VSlot5),                         // +0x28 Shutdown
    reinterpret_cast<void*>(&MP_VUpdate),                        // +0x30 Update
    reinterpret_cast<void*>(&StandaloneImpl_Slot7_OnUpdate),     // +0x38 OnUpdate
    reinterpret_cast<void*>(&MP_VOnEvent),                       // +0x40 OnEvent
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<9>),    // +0x48
    reinterpret_cast<void*>(&StandaloneStub_Slot10_DIDtor),      // +0x50 DI Dtor
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<11>),   // +0x58
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<12>),   // +0x60
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<13>),   // +0x68
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<14>),   // +0x70
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<15>),   // +0x78
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<16>),   // +0x80
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<17>),   // +0x88
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<18>),   // +0x90
    reinterpret_cast<void*>(&StandaloneStub_RuntimeThunk<19>),   // +0x98
};

// Core open path — mirrors MainMenuDialog_OpenGoal but with the MapParams vtable.
static void StandaloneOpenMapParams_UI(void* ui) {
    if (!ui) { printf("[OpenMap] ui null\n"); return; }
    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_DialogInstance_Ctor ||
        !fn_InternalBasePtr_Ctor || !fn_UI_AddDialog) {
        printf("[OpenMap] core fns unresolved\n"); return;
    }
    void* extRes = LoadDialogResource("Dialog/MapParams.rsc");
    if (!extRes) { printf("[OpenMap] Dialog/MapParams.rsc not loaded\n"); return; }
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, 0xF8, 8);
    if (!dlgMem) return;
    for (size_t o = 0; o < 0xF8; o += 8)
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(dlgMem) + o) = 0;
    uint8_t nullIBP[8] = {};
    fn_InternalBasePtr_Ctor(nullIBP, nullptr);
    uint64_t parentBuf = *reinterpret_cast<uint64_t*>(nullIBP);
    fn_DialogInstance_Ctor(dlgMem, ui, extRes, &parentBuf, 0, 0, 0);
    *reinterpret_cast<void**>(dlgMem) = g_MapParamsDialog_Vtable_Standalone;
    uint8_t ibp[8] = {};
    fn_InternalBasePtr_Ctor(ibp, dlgMem);
    fn_UI_AddDialog(ui, ibp);
    printf("[OpenMap] MapParamsDialog opened + AddDialog'd (dlg=%p)\n", dlgMem);
}

// Called from the Slot8 OnCommand dispatch (id 11 = buttonMap).
void StandaloneOpenMapParams(void* self) {
    if (!self) return;
    StandaloneOpenMapParams_UI(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x10));
}

// Harness entry — open the map dialog headlessly (UI from gameWorld+0x98).
extern "C" void MapParamsDialog_HarnessOpen() {
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gw) { printf("[OpenMap] gameWorld null\n"); return; }
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x98);
    printf("[OpenMap] harness open: ui=%p\n", ui);
    StandaloneOpenMapParams_UI(ui);
}

// In-place construct MainMenuDialog from a MenuDLT loop. Mirrors exe ctor
// at 0x14009BA30: builds a null parent IBP, calls DialogInstance::DialogInstance
// with (this, ui_at_+0x40, rsc_at_+0x30, parentIBP, 0, 0, 0), then patches
// vtable and sets the two MainMenuDialog-specific fields:
//   +0xB8 = game world ptr (0 at the menu; only set once a game is active)
//   +0xC0 = the OWNING LOOP (param_1 == the MenuDLT) — original does
//           `*(this+0xc0) = param_1`. NOT the engine, despite earlier comments.
// This +0xC0 wiring is load-bearing: NewGameDialog::StartGame reads
// worldSetup(==this MainMenuDialog)+0xC0 to decide its transition path. If it
// is 0, StartGame falls into the SetEngineNextLoop(worldSetup+0xB8) branch and
// dereferences a NULL engine (AV read of +0x98) — the Stage-B blocker. With
// +0xC0 = loop it correctly takes SetDialogLoaderNextLoop(loop) and writes
// loop+0xb68 (the loop's pending next-loop).
//
// Bare-bones standalone fallback: when m_dialogExternal (loop+0x30) is null
// — which is the case before Loader::Load wires Dialog/MainMenu.rsc —
// DialogInstance::DialogInstance crashes inside Runtime reading [rsc+0x40].
// In that case we skip the Runtime ctor and return a zeroed buffer with our
// vtable patched in. The dialog has no children but downstream stubs are
// safe (they no-op on zero fields). M2 (Loader::Load) replaces this with a
// real DialogInstance ctor call.
void* MainMenuDialog_Ctor_FromLoop(void* mem, void* loop) {
    if (!mem) return nullptr;

    // Zero the full 0xC8-byte object up front so any field read through the
    // vtable (e.g. UI::UserInterface dialog list pointer at +0x10) sees zero
    // instead of uninit'd heap garbage.
    for (size_t off = 0; off < 0xC8; off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(mem) + off) = 0;
    }

    uintptr_t lp = reinterpret_cast<uintptr_t>(loop);
    void* rsc = loop ? *reinterpret_cast<void**>(lp + 0x30) : nullptr;

    if (loop && rsc && fn_DialogInstance_Ctor && fn_InternalBasePtr_Ctor) {
        // Real construction — rsc is loaded.
        void* ui = reinterpret_cast<void*>(lp + 0x40);
        uint8_t parentIBP[8] = {0};
        fn_InternalBasePtr_Ctor(parentIBP, nullptr);
        fn_DialogInstance_Ctor(mem, ui, rsc, parentIBP, nullptr, nullptr, 0);
    } else {
        printf("[MainMenuDialog_FromLoop] bare-bones init (loop=%p rsc=%p) — "
               "DialogInstance ctor skipped, awaiting Loader::Load (M2)\n",
               loop, rsc);
    }

    // +0xC0 = the owning loop (matches original `*(this+0xc0) = param_1`).
    // +0xB8 (game world) stays 0 — already zeroed above. Required so
    // NewGameDialog::StartGame takes the SetDialogLoaderNextLoop path instead
    // of dereferencing a NULL engine. Only meaningful when constructed from a
    // real loop (bare factory paths pass loop=nullptr and leave it 0).
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(mem) + 0xC0) =
        reinterpret_cast<uint64_t>(loop);

    *reinterpret_cast<void**>(mem) = GetMainMenuDialogVtable();
    return mem;
}

// Factory entry — no-arg, used by Object::Factory::Construct lookups (.rsc loader).
// Standalone has no .rsc-driven instantiation of MainMenuDialog, so this returns
// a bare-bones object that won't be wired into UI; primarily satisfies the
// REGISTER_TYPE contract.
void* MainMenuDialog_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    if (!p) return nullptr;
    return MainMenuDialog_Ctor_FromLoop(p, nullptr);
}

void* MainMenuDialog_CreateInPlace_Bare(void* mem) {
    return MainMenuDialog_Ctor_FromLoop(mem, nullptr);
}

}  // namespace

// Exposed for the MapParamsDialog ctor registered at exe 0x14006bd80
// (code/Game/Dialog/unreg_lane2_mapparamsdialog_ctor.cpp). The vtable array lives
// in the anonymous namespace above, so it has internal linkage and cannot be
// extern'd from another TU; a function defined out here still sees it. Same trick
// RegisterMainMenuDialogVtable below already uses.
extern "C" void* MapParamsDialog_GetStandaloneVtable() {
    return reinterpret_cast<void*>(g_MapParamsDialog_Vtable_Standalone);
}

extern "C" void RegisterMainMenuDialogVtable() {
    RegisterExeAddr(0x14015f550, reinterpret_cast<uintptr_t>(g_MainMenuDialog_Vtable_Standalone));
}

// Public entry called from MenuDLT::CreateDialog stub.
extern "C" void* MainMenuDialog_FromMenuDLT(void* loop, void* retInternalBasePtr) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        if (retInternalBasePtr) {
            *reinterpret_cast<uint64_t*>(retInternalBasePtr) = 0;
        }
        return retInternalBasePtr;
    }
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* dlg = MainMenuDialog_Ctor_FromLoop(mem, loop);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retInternalBasePtr, dlg);
    return retInternalBasePtr;
}

// Trampolines to satisfy REGISTER_TYPE's FactoryFn / InPlaceCreateFn signatures.
extern "C" void* MainMenuDialog_StandaloneFactory(void) {
    return MainMenuDialog_Factory_Bare(nullptr);
}

extern "C" void* MainMenuDialog_StandaloneCreateInPlace(void* memory) {
    return MainMenuDialog_CreateInPlace_Bare(memory);
}

REGISTER_TYPE(MainMenuDialog, "UI::DialogInstance",
              &MainMenuDialog_StandaloneFactory, &MainMenuDialog_StandaloneCreateInPlace);

