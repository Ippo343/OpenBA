// code/Game/Dialog/saveloaddialog.cpp
// Clean reimplementation of SaveLoadDialog methods.
//
// SaveLoadDialog (0x160 bytes): Save/load file browser dialog.
// Extends DialogInstance (0xB8 base). Contains UI::Sprite at +0xD0.
//
// Layout:
//   +0xB8  mode (byte): 0 = save mode, nonzero = load mode
//   +0xB9  saveFlag (byte): set to 1 when save completes
//   +0xBA  loadFlag (byte): set to 1 when load completes
//   +0xBB  refreshFlag (byte): set to 1 to trigger file list refresh
//   +0xBC  deleteFlag (byte): set to 1 when delete completes
//   +0xC0  selectedIndex (int32): currently selected save slot (-1 = none)
//   +0xC4  saveCount (int32): number of save files
//   +0xC8  rotation (float): loading spinner angle
//   +0xD0  sprite (16 bytes): UI::Sprite sub-object
//   +0xE0  loaderLoop1 (ptr): used for save/load transition
//   +0xE8  loaderLoop2 (ptr): used for save/load transition
//   +0xF0  mainPage (InternalBasePtr): PageElement for page switching
//   +0xF8  labelLoad (InternalBasePtr)
//   +0x100 labelSave (InternalBasePtr)
//   +0x108 tableSaveList (InternalBasePtr)
//   +0x110 entryArray (ptr): dynamic array of 0x40-byte save entries
//   +0x118 entryCount (int32)
//   +0x120 textLoading (InternalBasePtr)
//   +0x128 textSaving (InternalBasePtr)
//   +0x130 textPleaseWait (InternalBasePtr)
//   +0x138 loadingImage (InternalBasePtr)
//   +0x140 buttonSave (InternalBasePtr)
//   +0x148 buttonLoad (InternalBasePtr)
//   +0x150 buttonCancel (InternalBasePtr)
//   +0x158 buttonDelete (InternalBasePtr)

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "ui/Dialogs.h"
#include <cstdint>

// ============================================================
// Helper: Find element by name in dialog instance
// ============================================================
static uint64_t FindDialogElem(void* dialogInst, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_DialogInstance_FindElement(dialogInst, &result, nameStr);
    fn_String_Dtor(nameStr);
    return result;
}

// ============================================================
// Startup (vtable +0x20): Build save/load UI, populate file list
// ============================================================
// Original: 0x1400b7d00
//
// Finds 12 named UI elements and stores them in SaveLoadDialog fields.
// Sets button element IDs: buttonSave=1, buttonLoad=2, buttonCancel=0, buttonDelete=3.
// Sets cancel button close-on-click flag (+0xC0 = 8).
// Sets refresh flag (+0xBB = 1) to trigger file list population.
// Copies sprite data from DAT_1401c3de8+0xB8 (16 bytes) to SaveLoad singleton.
// Calls SetMinOperationTime(0.75f) and SetOperation(1, mode).

void SaveLoadDialog_Startup(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find all named elements ---
    *(uint64_t*)(s + 0xF8)  = FindDialogElem(self, "labelLoad");
    *(uint64_t*)(s + 0x100) = FindDialogElem(self, "labelSave");
    *(uint64_t*)(s + 0x140) = FindDialogElem(self, "buttonSave");
    *(uint64_t*)(s + 0x148) = FindDialogElem(self, "buttonLoad");
    *(uint64_t*)(s + 0x150) = FindDialogElem(self, "buttonCancel");
    *(uint64_t*)(s + 0x158) = FindDialogElem(self, "buttonDelete");
    *(uint64_t*)(s + 0xF0)  = FindDialogElem(self, "mainPage");
    *(uint64_t*)(s + 0x120) = FindDialogElem(self, "textLoading");
    *(uint64_t*)(s + 0x128) = FindDialogElem(self, "textSaving");
    *(uint64_t*)(s + 0x130) = FindDialogElem(self, "textPleaseWait");
    *(uint64_t*)(s + 0x138) = FindDialogElem(self, "loadingImage");
    *(uint64_t*)(s + 0x108) = FindDialogElem(self, "tableSaveList");

    // --- Set button element IDs ---
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x140), 1);   // buttonSave
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x148), 2);   // buttonLoad
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x150), 0);   // buttonCancel
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x158), 3);   // buttonDelete

    // --- Set cancel button close-on-click flag ---
    *(uint32_t*)(*(uintptr_t*)(s + 0x150) + 0xC0) = 8;

    // --- Set refresh flag ---
    *(uint8_t*)(s + 0xBB) = 1;

    // --- Copy sprite from DAT_1401c3de8 + 0xB8 and set on SaveLoad singleton ---
    uintptr_t keyMapObj = (uintptr_t)*g_pKeyMapSingleton;
    uint8_t spriteCopy[16];
    *(uint32_t*)(spriteCopy + 0)  = *(uint32_t*)(keyMapObj + 0xB8);
    *(uint32_t*)(spriteCopy + 4)  = *(uint32_t*)(keyMapObj + 0xBC);
    *(uint32_t*)(spriteCopy + 8)  = *(uint32_t*)(keyMapObj + 0xC0);
    *(uint32_t*)(spriteCopy + 12) = *(uint32_t*)(keyMapObj + 0xC4);
    fn_SaveLoad_SetSaveSprite(*g_pSaveLoadSingleton, spriteCopy);

    // --- Set minimum operation time (0.75f seconds) ---
    uint8_t timeVal[8];
    fn_Time_Ctor_Float(timeVal, 0.75f);
    fn_SaveLoad_SetMinOperationTime(*g_pSaveLoadSingleton, timeVal);

    // --- Start operation: 1 (list/scan), param = -2 or -3 based on mode ---
    uint8_t mode = *(uint8_t*)(s + 0xB8);
    int opParam = -2 - (int)(mode != 0);
    fn_SaveLoad_SetOperation(*g_pSaveLoadSingleton, 1, opParam);
}

// ============================================================
// Update (vtable +0x30): Refresh file list, update preview
// ============================================================
// Original: 0x1400b6880
//
// Two main states:
// 1. Operation active (op != 0) OR any flag set: Show loading page (page 0),
//    rotate loading spinner, show/hide text based on operation type.
// 2. Idle (op == 0, no flags): Show main page (page 1), enable/disable buttons,
//    iterate save entries to show/hide selection highlights.

void SaveLoadDialog_Update(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    uint8_t mode = *(uint8_t*)(s + 0xB8);
    void* saveLoad = *g_pSaveLoadSingleton;

    // --- Show/hide load/save labels based on mode ---
    // labelLoad (+0xF8): visible when mode == 0 (save mode shows "load" label? no...)
    // Actually: labelLoad visible when NOT save mode, labelSave visible when save mode
    uint32_t isLoadMode = (uint32_t)(mode == 0);
    uint32_t isSaveMode = (uint32_t)(mode != 0);
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0xF8), isLoadMode, 0);
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x100), isSaveMode, 0);

    // buttonSave (+0x140): visible in save mode
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x140), isSaveMode, 0);
    // buttonLoad (+0x148): visible in load mode
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x148), isLoadMode, 0);

    int operation = fn_SaveLoad_GetOperation(saveLoad);

    // Check if any operation is in progress or any deferred flag is set
    bool anyFlagsSet = (*(uint8_t*)(s + 0xBB) != 0) ||
                       (*(uint8_t*)(s + 0xB9) != 0) ||
                       (*(uint8_t*)(s + 0xBA) != 0) ||
                       (*(uint8_t*)(s + 0xBC) != 0);

    if (operation == 0 && !anyFlagsSet) {
        // --- IDLE STATE: show main page, update buttons and entry list ---
        fn_PageElement_SetActivePage((void*)*(uint64_t*)(s + 0xF0), 1);

        // Enable/disable buttonLoad based on selected save validity
        int selectedIdx = *(int*)(s + 0xC0);
        int saveCount = *(int*)(s + 0xC4);
        bool loadEnabled = false;
        if (saveCount >= 1 && selectedIdx >= 0) {
            void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
            // saveInfo+0x44 == 2 means valid save
            if (*(int*)((uintptr_t)saveInfo + 0x44) == 2) {
                loadEnabled = true;
            }
        }

        // buttonLoad: SetEnabled via vtable[8]
        {
            void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
            void** vtable = *(void***)loadBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(loadBtn, loadEnabled ? 1 : 0);
        }

        // buttonSave: always enabled
        {
            void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
            void** vtable = *(void***)saveBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(saveBtn, 1);
        }

        // buttonCancel: always enabled
        {
            void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
            void** vtable = *(void***)cancelBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(cancelBtn, 1);
        }

        // Check selected save for delete eligibility
        if (selectedIdx >= 0) {
            fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        }

        // buttonDelete: SetEnabled via vtable[8]
        {
            void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
            void** vtable = *(void***)deleteBtn;
            typedef void (*fn_SetEnabled_t)(void*, uint64_t);
            ((fn_SetEnabled_t)vtable[8])(deleteBtn, (uint64_t)(selectedIdx >= 0));
        }

        // --- Iterate save entries: show/hide selection highlighting ---
        if (saveCount < 1) return;

        uintptr_t entryArray = *(uintptr_t*)(s + 0x110);
        for (int i = 0; i < saveCount; i++) {
            uintptr_t entryBase = entryArray + (uintptr_t)i * 0x40;
            bool notSelected = (i != selectedIdx);

            // entry+0x28: imageCorrupt/selectSave — shown when selected, hidden otherwise
            void* selectElem = (void*)*(uint64_t*)(entryBase + 0x28);
            fn_Element_SetFlagsOnOff(selectElem, notSelected ? 1 : 0, notSelected ? 0 : 1);

            // entry+0x18: borderSave — shown when NOT selected, hidden when selected
            void* borderElem = (void*)*(uint64_t*)(entryBase + 0x18);
            fn_Element_SetFlagsOnOff(borderElem, notSelected ? 0 : 1, notSelected ? 1 : 0);
        }
        return;
    }

    // --- OPERATION IN PROGRESS: show loading page ---
    // Rotate loading spinner image
    fn_ImageElement_SetRotation((void*)*(uint64_t*)(s + 0x138),
                                *(float*)(s + 0xC8));

    // Switch to loading page (page 0)
    fn_PageElement_SetActivePage((void*)*(uint64_t*)(s + 0xF0), 0);

    // Disable all four buttons
    {
        void* loadBtn = (void*)*(uint64_t*)(s + 0x148);
        void** vtable = *(void***)loadBtn;
        typedef void (*fn_SetEnabled_t)(void*, uint64_t);
        ((fn_SetEnabled_t)vtable[8])(loadBtn, 0);
    }
    {
        void* saveBtn = (void*)*(uint64_t*)(s + 0x140);
        void** vtable = *(void***)saveBtn;
        typedef void (*fn_SetEnabled_t)(void*, uint64_t);
        ((fn_SetEnabled_t)vtable[8])(saveBtn, 0);
    }
    {
        void* cancelBtn = (void*)*(uint64_t*)(s + 0x150);
        void** vtable = *(void***)cancelBtn;
        typedef void (*fn_SetEnabled_t)(void*, uint64_t);
        ((fn_SetEnabled_t)vtable[8])(cancelBtn, 0);
    }
    {
        void* deleteBtn = (void*)*(uint64_t*)(s + 0x158);
        void** vtable = *(void***)deleteBtn;
        typedef void (*fn_SetEnabled_t)(void*, uint64_t);
        ((fn_SetEnabled_t)vtable[8])(deleteBtn, 0);
    }

    // --- Show/hide status text based on operation type ---
    // Operation types: 1 = listing, 2 = saving, 4 = deleting
    if (operation == 1) {
        // Listing: show textPleaseWait, hide textLoading, hide textSaving
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x130), 0, 1);  // show textPleaseWait
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x120), 1, 0);  // hide textLoading
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x128), 1, 0);  // hide textSaving
    } else if (operation == 2) {
        // Saving: hide textPleaseWait, show textLoading, hide textSaving
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x130), 1, 0);  // hide textPleaseWait
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x120), 0, 1);  // show textLoading
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x128), 1, 0);  // hide textSaving
    } else if (operation == 4) {
        // Deleting: hide textPleaseWait, hide textLoading, show textSaving
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x130), 1, 0);  // hide textPleaseWait
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x120), 1, 0);  // hide textLoading
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x128), 0, 1);  // show textSaving
    }
}

// ============================================================
// Load (vtable +0x38): Process timed operations, trigger state transitions
// ============================================================
// Original: 0x1400b79b0
//
// Timer/state machine called every frame.
//
// Phase 1 — While Save::SaveLoad operation is non-zero (file I/O in progress):
//   Accumulate rotation angle at +0xC8 (180 deg/sec from time param).
//
// Phase 2 — When operation reaches 0 (I/O complete), process one deferred flag:
//
//   deleteFlag (+0xBC): delete completed.
//     Write *(uint16_t*)(+0xBB) = 1 (sets +0xBB=1, +0xBC=0), start re-list operation.
//
//   refreshFlag (+0xBB): file list needs refresh.
//     Update saveCount (+0xC4), clamp selectedIndex (+0xC0), populate file list, clear flag.
//
//   saveFlag (+0xB9): save completed.
//     Close this dialog via UI::UserInterface::CloseDialog, clear flag.
//
//   loadFlag (+0xBA): load completed — transition to game.
//     Close this dialog.
//     Allocate GameUpdateLoop (0x160 bytes), construct via GameUpdateLoop ctor.
//     Allocate GameLoadDialogLoopT (0xb60 bytes), construct via DialogLoaderLoopT base ctor,
//       then patch vtable to exe subclass at PTR_FUN_140158800, set extra fields.
//     Allocate outer LoaderLoop (0x48 bytes), construct via LoaderLoop ctor with 0.0s delay,
//       patch vtable to PTR_FUN_1401588d0.
//     Set the outer LoaderLoop as the next loop on the engine (via field +0xE0/+0xE8).
//     Clear loadFlag.
//
// All constructors are available: GameUpdateLoop via exe address, LoaderLoop and
// DialogLoaderLoopT via Runtime-x64.dll exports. Vtable patches replicate the
// exe subclass polymorphism (the base Runtime ctors set the base vtable; exe code
// overwrites with the subclass vtable immediately after).

void SaveLoadDialog_Load(SaveLoadDialog* self, void* time, void** loaderLoopOut, uint64_t param4) {
    (void)loaderLoopOut;
    (void)param4;
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Phase 1: accumulate spinner rotation while I/O is in progress
    int operation = fn_SaveLoad_GetOperation(saveLoad);
    if (operation != 0) {
        float elapsed = fn_Time_AsSeconds(time);
        *(float*)(s + 0xC8) += elapsed * 180.0f;
    }

    // Re-check — may have just completed this frame
    operation = fn_SaveLoad_GetOperation(saveLoad);
    if (operation != 0) return;

    // Phase 2: I/O complete, process one deferred flag (priority order matches original)

    // deleteFlag (+0xBC): re-trigger list scan
    if (*(uint8_t*)(s + 0xBC) != 0) {
        *(uint16_t*)(s + 0xBB) = 1;   // sets +0xBB=1 (refresh), +0xBC=0 (clear delete)
        uint8_t mode = *(uint8_t*)(s + 0xB8);
        fn_SaveLoad_SetOperation(saveLoad, 1, -2 - (int)(mode != 0));
        return;
    }

    // refreshFlag (+0xBB): populate file list
    if (*(uint8_t*)(s + 0xBB) != 0) {
        int count = fn_SaveLoad_GetSaveCount(saveLoad);
        *(int*)(s + 0xC4) = count;
        if (count <= *(int*)(s + 0xC0)) {
            *(int*)(s + 0xC0) = count - 1;
        }
        fn_SaveLoadDialog_PopulateFileList((void*)s);
        *(uint8_t*)(s + 0xBB) = 0;
        return;
    }

    // saveFlag (+0xB9): save done, close dialog (no game transition needed)
    if (*(uint8_t*)(s + 0xB9) != 0) {
        uint64_t basePtr = 0;
        fn_InternalBasePtr_Ctor(&basePtr, (void*)s);
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, &basePtr);
        *(uint8_t*)(s + 0xB9) = 0;
        return;
    }

    // loadFlag (+0xBA): load done — close dialog and transition to game load screen
    if (*(uint8_t*)(s + 0xBA) != 0) {
        // Close the SaveLoad dialog
        uint64_t basePtr = 0;
        fn_InternalBasePtr_Ctor(&basePtr, (void*)s);
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, &basePtr);

        // --- Step 1: Allocate and construct the inner GameUpdateLoop (0x160 bytes) ---
        // This is the actual gameplay loop that will run after loading completes.
        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        void* pGameLoop = nullptr;
        if (gameLoop) {
            uint64_t nameStr = 0;  // empty string
            uint64_t loopPtrNull = 0;
            void* savedState = fn_SaveLoad_GetSaveState(saveLoad);
            // Signature: (self, savedState, loaderLoopPtr, opFlag, nameStr, 0, 0, 1, 0, 0, 0)
            // opFlag=0xffffffff, nameStr=empty string, param8='\x01'
            fn_GameUpdateLoop_Ctor(gameLoop, savedState, (void*)&loopPtrNull,
                                   0xffffffff, (void*)&nameStr, 0, 0, 1, 0, 0, 0);
            fn_String_Dtor((void*)&nameStr);
            pGameLoop = gameLoop;
        }

        // --- Step 2: Allocate and construct the GameLoadDialogLoopT (0xb60 bytes) ---
        // Base: Engine::DialogLoaderLoopT (Runtime export), then patch exe subclass vtable.
        void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
        void* pDlLoop = nullptr;
        if (dlLoop) {
            // Build Time from constant (originally DAT_14017214c = 1.0f)
            float delayVal = 1.0f;
            uint64_t delayTime[1]; // 8-byte Time
            fn_Time_Ctor_Float((void*)delayTime, delayVal);

            // Get the dialog filename string via fn_GetLoadingSoundString
            // This formats "Dialog/Loading.rsc:gameLoad%i" with a random 0-11 variant
            uint64_t dialogNameStr = 0;
            fn_GetLoadingSoundString(0, (void*)&dialogNameStr, 0, 0);

            // Construct base Engine::DialogLoaderLoopT
            fn_DialogLoaderLoopT_Ctor(dlLoop, (void*)&dialogNameStr, pGameLoop, (void*)delayTime);

            fn_String_Dtor((void*)&dialogNameStr);

            // Patch vtable to exe subclass (GameLoadDialogLoopT)
            *(void**)dlLoop = (void*)g_GameLoadDialogLoopT_Vtable;

            // Initialize extra fields added by exe subclass
            uintptr_t dl = (uintptr_t)dlLoop;
            *(uint32_t*)(dl + 0x188) = 0xff000000;
            *(uint32_t*)(dl + 0x18c) = 0xff000000;
            *(uint8_t*)(dl  + 0x190) = 0;  // [400]
            // System::Random::Random at +0x194
            fn_Random_Default_Ctor((void*)(dl + 0x194));
            *(uint64_t*)(dl + 0xb58) = 0;
            *(uint8_t*)(dl  + 0x40)  = 0;  // m_finished override = 0

            pDlLoop = dlLoop;
        }

        // --- Step 3: Allocate and construct the outer LoaderLoop (0x48 bytes) ---
        // Wraps the GameLoadDialogLoopT as its next loop with zero delay.
        void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
        void* pOuterLoop = nullptr;
        if (outerLoop) {
            uint64_t zeroTime[1];
            fn_Time_Ctor_Float((void*)zeroTime, 0.0f);
            fn_LoaderLoop_Ctor(outerLoop, pDlLoop, (void*)zeroTime);
            // Patch vtable to exe subclass (GameLoaderLoop)
            *(void**)outerLoop = (void*)g_GameLoaderLoop_Vtable;
            pOuterLoop = outerLoop;
        }

        // --- Step 4: Set the outer LoaderLoop as the engine's next loop ---
        uintptr_t loaderLoop1 = *(uintptr_t*)(s + 0xE0);
        if (loaderLoop1 == 0) {
            // No existing dialog loader loop: set next loop on the engine singleton
            fn_SetEngineNextLoop(*(uintptr_t*)(s + 0xE8), pOuterLoop);
        } else {
            // Inside a dialog loader loop: queue as its next transition
            fn_SetDialogLoaderNextLoop(loaderLoop1, pOuterLoop);
        }

        *(uint8_t*)(s + 0xBA) = 0;
    }
}

// ============================================================
// Helper: create a confirm-style MessageDialog from a .rsc path string
// ============================================================
// Loads the External resource by filename, verifies it is a DialogInstance,
// allocates a 0xC0-byte DialogInstance shell, constructs it, patches the vtable
// to MessageDialog (0x14015c480), and calls UI::ShowDialog.
// Returns the ShowDialog result (2 = confirmed/Yes, other = cancelled).
//
// This pattern appears identically in DoDelete, DoSave, and DoLoad.
static int SaveLoad_ShowConfirmDialog(void* self, const char* rscPath) {
    uintptr_t s = (uintptr_t)self;

    // Load the .rsc External resource
    uint8_t filename[8] = {};
    fn_Filename_CtorCStr(filename, rscPath);
    void* externalRes = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);
    fn_Filename_Dtor(filename);

    // IsDerivedFrom check — ensure it's a DialogInstance
    void* checkedRes = nullptr;
    if (externalRes) {
        bool isDialog = fn_IsDerivedFrom(externalRes, *g_TypeId_DialogInstance);
        if (isDialog) checkedRes = externalRes;
    }

    // Allocate 0xC0 bytes for a DialogInstance shell
    void* dlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
    if (dlg) {
        // Construct base DialogInstance(ui, externalRes, nullParent, 0, 0, 0)
        uint64_t nullParent = 0;
        fn_DialogInstance_Ctor(dlg, *(void**)(s + 0x10), checkedRes, &nullParent, 0, 0, 0);
        // Patch vtable to MessageDialog
        *(void**)dlg = (void*)MessageDialogAddr::Vtable;
    }

    // ShowDialog(ui, InternalPtr<DialogInstance>)
    uint64_t dlgPtr = 0;
    fn_InternalBasePtr_Ctor(&dlgPtr, dlg);
    int result = fn_UI_ShowDialog(*(void**)(s + 0x10), &dlgPtr);
    fn_InternalBasePtr_DeletePtr(&dlgPtr);
    return result;
}

// ============================================================
// DoDelete (0x1400b5e50): Show confirmDelete dialog, then trigger delete operation.
// ============================================================
// Shows "Dialog/SaveLoad.rsc:confirmDelete" dialog.
// If user confirms (result == 2): sets deleteFlag (+0xBC = 1), starts operation 5.
// Original: 0x1400b5e50.

static void SaveLoadDialog_DoDelete(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;

    int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmDelete");
    if (result == 2) {
        *(uint8_t*)(s + 0xBC) = 1;
        void* saveLoad = *g_pSaveLoadSingleton;
        fn_SaveLoad_SetOperation(saveLoad, 5, *(int*)(s + 0xC0));
    }
}

// ============================================================
// DoSave (0x1400b5fd0): Optional confirm-overwrite, then trigger save operation.
// ============================================================
// Sets min operation time (1.5f seconds).
// Gets save info for selected index: if save slot already has data (validState >= 0),
//   shows "Dialog/SaveLoad.rsc:confirmSave" dialog; if not confirmed, returns.
// Sets saveFlag (+0xB9 = 1), copies save name from text element at +0xE8+0x28,
//   calls SetSaveName, then SetOperation(4, selectedIndex).
// Original: 0x1400b5fd0.

static void SaveLoadDialog_DoSave(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Set minimum operation time (1.5f seconds — DAT_140172160)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    fn_SaveLoad_SetMinOperationTime(saveLoad, minTime);

    // Check if the selected save slot already contains data
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
    int validState = *(int*)((uintptr_t)saveInfo + 0x40);
    if (validState >= 0) {
        // Slot is occupied — ask user to confirm overwrite
        int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmSave");
        if (result != 2) return;  // user cancelled
    }

    // Mark save pending
    *(uint8_t*)(s + 0xB9) = 1;

    // Copy save name from the text element at +0xE8 (the name input field)
    // FUN_140011f90 inlined: String::String(retval, *(dialog+0xE8) + 0x28)
    uint8_t saveName[8] = {};
    void* nameSource = *(void**)(s + 0xE8);
    fn_String_Ctor_Copy(saveName, (void*)((uintptr_t)nameSource + 0x28));
    fn_SaveLoad_SetSaveName(saveLoad, saveName, 2);
    fn_String_Dtor(saveName);

    // Start save operation (4 = save, with selected slot index)
    fn_SaveLoad_SetOperation(saveLoad, 4, selectedIdx);
}

// ============================================================
// DoLoad (0x1400b61e0): Confirm load, check mod compatibility, then trigger load.
// ============================================================
// Sets min operation time (1.5f seconds).
// If game world is loaded (DAT_1401c3dd8 != 0):
//   Shows "Dialog/SaveLoad.rsc:confirmLoad" dialog; if not confirmed, returns.
// Iterates through all mods in the selected save's mod list:
//   For each mod with a non-zero CheckMod status, shows a mod-warning dialog
//   ("modsOutOfDate", "modsVersion", "modsChanged", or "modsMissing") with
//   mod name/version info set as TextElement parameters.
//   If user declines any warning, returns without loading.
// Sets loadFlag (+0xBA = 1), starts operation 2 (load) with selected index.
// Original: 0x1400b61e0.

static void SaveLoadDialog_DoLoad(SaveLoadDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* saveLoad = *g_pSaveLoadSingleton;

    // Set minimum operation time (1.5f seconds)
    uint8_t minTime[8] = {};
    fn_Time_Ctor_Float(minTime, 1.5f);
    fn_SaveLoad_SetMinOperationTime(saveLoad, minTime);

    // If game world is active, ask user to confirm leaving the current game
    if (*g_pGameWorldSingleton != nullptr) {
        int result = SaveLoad_ShowConfirmDialog(self, "Dialog/SaveLoad.rsc:confirmLoad");
        if (result != 2) return;
    }

    // Iterate through mods in the selected save and warn about any incompatibilities
    int selectedIdx = *(int*)(s + 0xC0);
    void* saveInfo = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
    int modCount = *(int*)((uintptr_t)saveInfo + 0x50);

    for (int i = 0; i < modCount; i++) {
        int modStatus = fn_SaveLoad_CheckMod(saveLoad, selectedIdx, i);
        if (modStatus == 0) continue;  // mod is fine

        // Get mod name string pointer: saveInfo->modList[i] (stride 0x20 from base at +0x48)
        // saveInfo+0x48 = base of mod name array; each entry is 0x20 bytes; entry+0 = String
        void* saveInfo2 = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        void* modNameBase = *(void**)((uintptr_t)saveInfo2 + 0x48);
        void* modNameStr = (void*)((uintptr_t)modNameBase + (uintptr_t)i * 0x20);  // String at stride 0x20

        // Select the appropriate warning dialog based on mod status
        const char* warnPath = nullptr;
        if      (modStatus == 2) warnPath = "Dialog/SaveLoad.rsc:modsOutOfDate";
        else if (modStatus == 3) warnPath = "Dialog/SaveLoad.rsc:modsVersion";
        else if (modStatus == 4) warnPath = "Dialog/SaveLoad.rsc:modsChanged";
        else                     warnPath = "Dialog/SaveLoad.rsc:modsMissing";

        // Load the warning dialog External resource
        uint8_t warnFilename[8] = {};
        fn_Filename_CtorCStr(warnFilename, warnPath);
        void* warnExt = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, warnFilename);
        fn_Filename_Dtor(warnFilename);

        void* checkedWarn = nullptr;
        if (warnExt) {
            if (fn_IsDerivedFrom(warnExt, *g_TypeId_DialogInstance)) {
                checkedWarn = warnExt;
            }
        }

        // Allocate and construct the warning DialogInstance shell
        void* warnDlg = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
        if (warnDlg) {
            uint64_t nullParent = 0;
            fn_DialogInstance_Ctor(warnDlg, *(void**)(s + 0x10), checkedWarn, &nullParent, 0, 0, 0);
            *(void**)warnDlg = (void*)MessageDialogAddr::Vtable;
        }

        // Find the TextElement inside the warning dialog and set mod info parameters
        // FindElementType on the dialog InternalPtr returns a TextElement InternalPtr
        uint64_t warnDlgPtr = 0;
        fn_InternalBasePtr_Ctor(&warnDlgPtr, warnDlg);
        fn_InternalBasePtr_CheckPtr(&warnDlgPtr);

        // Find TextElement within dialog (uses g_TypeId_TextElement)
        uint64_t textElemPtr = 0;
        fn_DialogInstance_FindElementType((void*)warnDlgPtr, &textElemPtr, *g_TypeId_TextElement);
        fn_InternalBasePtr_CheckPtr(&textElemPtr);

        // Check IsDerivedFrom TextElement
        void* rawElem = *(void**)&textElemPtr;
        bool isText = fn_IsDerivedFrom(rawElem, *g_TypeId_TextElement);
        void* textElem = isText ? rawElem : nullptr;

        uint64_t textPtr = 0;
        fn_InternalBasePtr_Ctor(&textPtr, textElem);
        fn_InternalBasePtr_CheckPtr(&textPtr);

        // SetParameter(0) = mod name, SetParameter(1) = mod name (again, as per decompilation)
        fn_TextElement_SetParameter((void*)textPtr, 0, modNameStr);
        fn_InternalBasePtr_CheckPtr(&textPtr);
        fn_TextElement_SetParameter((void*)textPtr, 1, (void*)((uintptr_t)modNameStr + 8));

        // SetParameter(2) = formatted version string "%i (%s)"
        // saveInfo+0x14 = isBeta (uint, sub-field), saveInfo+0x18 = version int
        // Configuration::GetVersionString returns a const String*
        void* saveInfo3 = fn_SaveLoad_GetSaveInfo(saveLoad, selectedIdx);
        void* modEntry = (void*)((uintptr_t)*(void**)((uintptr_t)saveInfo3 + 0x48) + (uintptr_t)i * 0x20);
        uint32_t isBeta   = *(uint32_t*)((uintptr_t)modEntry + 0x14);
        uint32_t modVer   = *(uint32_t*)((uintptr_t)modEntry + 0x18);
        uint8_t verStrBuf[8] = {};
        void* verStr = fn_Configuration_GetVersionString(verStrBuf, isBeta, (bool)(isBeta != 0));
        uint8_t fmtBuf[8] = {};
        fn_String_Format(fmtBuf, "%i (%s)", (unsigned)modVer, *(const char**)verStr);
        fn_InternalBasePtr_CheckPtr(&textPtr);
        fn_TextElement_SetParameter((void*)textPtr, 2, fmtBuf);
        fn_String_Dtor(fmtBuf);
        fn_String_Dtor(verStrBuf);

        // Show the warning dialog and check if user confirmed
        uint64_t showPtr = 0;
        fn_InternalBasePtr_Ctor(&showPtr, warnDlg);  // re-wrap for ShowDialog
        // Note: original passes warnDlgPtr (the dialog InternalPtr from +0x77 slot)
        // which was constructed above from warnDlg. Re-use warnDlgPtr.
        int warnResult = fn_UI_ShowDialog(*(void**)(s + 0x10), &warnDlgPtr);
        fn_InternalBasePtr_DeletePtr(&warnDlgPtr);
        fn_String_Dtor(&showPtr);  // cleanup the extra ptr we built

        if (warnResult != 2) return;  // user declined mod warning — abort load
    }

    // All checks passed — trigger load operation
    *(uint8_t*)(s + 0xBA) = 1;
    fn_SaveLoad_SetOperation(saveLoad, 2, selectedIdx);
}

// ============================================================
// SaveLoadDialog::OnEvent (0x1400b67e0) — Slot8
// ============================================================
// Handles button clicks and list selection changes.
// eventType 2 = list selection change:
//   eventData in (3, saveCount+4) → select entry at (eventData-4)
// eventType 0x12 = button click:
//   eventData 0 = buttonCancel → close dialog
//   eventData 1 = buttonSave → save with optional confirm dialog
//   eventData 2 = buttonLoad → load with optional confirm dialogs
//   eventData 3 = buttonDelete → delete with confirm dialog
//
// Original: 0x1400b67e0.

// Helper: set selected index and play UI sound
static void SaveLoadDialog_SelectEntry(SaveLoadDialog* self, int index) {
    uintptr_t s = (uintptr_t)self;
    // FUN_1400b6790: if index changed, play sound 4 first
    int oldIndex = *(int*)(s + 0xC0);
    if (oldIndex != index) {
        fn_UI_Play(*(void**)(s + 0x10), 4);
    }
    *(int*)(s + 0xC0) = index;
}

void SaveLoadDialog_OnEvent(SaveLoadDialog* self, void* element, int eventType, int eventData) {
    uintptr_t s = (uintptr_t)self;

    if (eventType == 2) {
        // List row selection change
        int saveCount = *(int*)(s + 0xC4);
        if (eventData > 3 && eventData < saveCount + 4) {
            SaveLoadDialog_SelectEntry(self, eventData - 4);
        }
        return;
    }

    if (eventType == 0x12) {
        if (eventData == 0) {
            // buttonCancel: close dialog
            void* ui = *(void**)(s + 0x10);
            uint64_t basePtr[2];
            fn_InternalBasePtr_Ctor(basePtr, self);
            fn_UI_CloseDialog(ui, basePtr);
            return;
        }
        if (eventData == 2) {
            SaveLoadDialog_DoLoad(self);
            return;
        }
        if (eventData == 1) {
            SaveLoadDialog_DoSave(self);
            return;
        }
        if (eventData == 3) {
            SaveLoadDialog_DoDelete(self);
            return;
        }
    }
}
