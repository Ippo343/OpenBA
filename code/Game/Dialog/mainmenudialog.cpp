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
// Helper: open a dialog using a typed ctor function pointer
// ============================================================
// Used by the Open* handlers below. Loads the .rsc resource,
// allocates the dialog buffer, calls the ctor, wraps in
// InternalBasePtr, and calls AddDialog.
//
// param_1 = self (MainMenuDialog*)
// rscPath  = e.g. "Dialog/NewGame.rsc"
// ctorSize = byte size of the dialog object to allocate
// ctorFn   = function pointer to the specific dialog ctor (takes only `self`)
// passParent = true if the ctor needs self as the parent ptr
static void MainMenuDialog_OpenDialog(MainMenuDialog* param_1,
                                      const char* rscPath,
                                      uint64_t ctorSize,
                                      void* (*ctorFn)(void*))
{
    // Load .rsc resource
    uint8_t filenameBuf[8];
    fn_Filename_CtorCStr(filenameBuf, rscPath);
    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    void* extRes = nullptr;
    if (resource && fn_IsDerivedFrom(resource, *g_pExternalTypeId)) {
        extRes = resource;
    }
    fn_Filename_Dtor(filenameBuf);

    // Allocate and construct the dialog
    void* dlgMem = fn_MemAlloc(g_pMemAllocSingleton, ctorSize, 8);
    void* dlgObj = nullptr;
    if (dlgMem) {
        // Most ctors set up their own parent via saved register or take no parent
        (void)extRes;  // ctor reads extRes from stack — but we can't pass it here easily.
        // For these Open* functions, the original just calls the ctor with param_1 on stack
        // as a saved register. We call via fn_* pointer which was resolved earlier.
        dlgObj = ctorFn(dlgMem);
    }

    // Wrap in InternalBasePtr and call AddDialog
    uint8_t ibp[8];
    fn_InternalBasePtr_Ctor(ibp, dlgObj);
    fn_UI_AddDialog(*(void**)((uintptr_t)param_1 + 0x10), ibp);
}

// ============================================================
// MainMenuDialog Open* handler implementations
// ============================================================
// These are called from Slot8 (OnCommand) when menu buttons are clicked.
// Each opens a sub-dialog by loading the .rsc resource, allocating and
// constructing the dialog object, then calling AddDialog.
//
// Pattern observed from decompile:
//   Load rsc -> MemAlloc(size) -> call ctor(mem) -> InternalBasePtr -> AddDialog
//
// The ctor functions set up the specific dialog state (vtable, fields, etc.).
// The resource pointer is passed as part of the ctor arguments via the calling
// convention's stack frame -- the original code passes it via SavedRegister/local.
//
// Since we cannot pass the rsc pointer directly to these ctor fn_* pointers
// without re-implementing the full ctor, we keep these as fn_* pointer calls
// which are already resolved at runtime. The fn_* pointers call directly into
// the original exe functions.

void MainMenuDialog_OpenNewGame(MainMenuDialog* self) {
    fn_MainMenu_OpenNewGame(self);
}

void MainMenuDialog_OpenLoad(MainMenuDialog* self) {
    fn_MainMenu_OpenLoad(self);
}

void MainMenuDialog_OpenSave(MainMenuDialog* self) {
    fn_MainMenu_OpenSave(self);
}

void MainMenuDialog_OpenOptions(MainMenuDialog* self) {
    fn_MainMenu_OpenOptions(self);
}

void MainMenuDialog_ReturnToMenu(MainMenuDialog* self) {
    fn_MainMenu_ReturnToMenu(self);
}

void MainMenuDialog_OpenAchievement(MainMenuDialog* self) {
    fn_MainMenu_OpenAchievement(self);
}

void MainMenuDialog_OpenScenario(MainMenuDialog* self) {
    fn_MainMenu_OpenScenario(self);
}

void MainMenuDialog_OpenGoal(MainMenuDialog* self) {
    fn_MainMenu_OpenGoal(self);
}

void MainMenuDialog_OpenMap(MainMenuDialog* self) {
    fn_MainMenu_OpenMap(self);
}

void MainMenuDialog_OpenTutorial(MainMenuDialog* self) {
    fn_MainMenu_OpenTutorial(self);
}

void MainMenuDialog_OpenMod(MainMenuDialog* self) {
    fn_MainMenu_OpenMod(self);
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
// Private handler functions are called via resolved exe function pointers
// (fn_MainMenu_* in engine_funcs) since they are complex and will be
// reimplemented individually later.

void MainMenuDialog_Slot8(MainMenuDialog* self, void* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    // Only handle button click events (command type 0x12)
    if (param3 != 0x12)
        return;

    switch (param4) {
    case kButton_New:
        fn_MainMenu_OpenNewGame(self);
        break;

    case kButton_Resume: {
        // Close this dialog (return to gameplay)
        uint8_t selfPtr[16];
        fn_InternalBasePtr_Ctor(selfPtr, (void*)self);
        fn_UI_CloseDialog(*(void**)(s + 0x10), selfPtr);
        break;
    }

    case kButton_Load:
        fn_MainMenu_OpenLoad(self);
        break;

    case kButton_Save:
        fn_MainMenu_OpenSave(self);
        break;

    case kButton_Options:
        fn_MainMenu_OpenOptions(self);
        break;

    case kButton_Menu:
        fn_MainMenu_ReturnToMenu(self);
        break;

    case kButton_Quit:
        fn_App_Quit(*g_pAppSingleton);
        break;

    case kButton_Mod:
        fn_MainMenu_OpenMod(self);
        break;

    case kButton_Achievement:
        fn_MainMenu_OpenAchievement(self);
        break;

    case kButton_Scenario:
        fn_MainMenu_OpenScenario(self);
        break;

    case kButton_Goal:
        fn_MainMenu_OpenGoal(self);
        break;

    case kButton_Map:
        fn_MainMenu_OpenMap(self);
        break;

    case kButton_Tutorial:
        fn_MainMenu_OpenTutorial(self);
        break;
    }
}
