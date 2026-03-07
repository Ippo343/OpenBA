// engine/src/ui/tutorialui.cpp
// Clean reimplementation of TutorialUI panel methods.
//
// TutorialUI (0x60 bytes): Tutorial step display panel.
// Layout: +0x30 = unknown, +0x38 = "preText" element, +0x40 = "postText" element,
//         +0x48 = "imageTable" element, +0x50 = "nextButton" element,
//         +0x58 = "quitButton" element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: FindElemByName with IsDerivedFrom check, stores InternalBasePtr
static void FindAndStore(uintptr_t self, const char* name, int fieldOffset) {
    char nameBuf[8] = {};
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t foundResult = 0;
    fn_FindElement((void*)*(uint64_t*)(self + 0x20), &foundResult, nameBuf);

    void* found = (void*)foundResult;
    void* checked = nullptr;
    // The exref type check uses the generic Element type from the import table.
    // In practice these all succeed, so we just do a null check and store.
    if (found) {
        checked = found;
    }
    uint64_t stored = 0;
    fn_InternalBasePtr_Ctor(&stored, checked);
    *(uint64_t*)(self + fieldOffset) = stored;

    fn_String_Dtor(nameBuf);
}

// Serialize (vtable +0x20): Finds named elements, sets button IDs, hides close button.
void TutorialUI_Serialize(TutorialUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find and cache UI elements
    FindAndStore(s, "preText",   0x38);
    FindAndStore(s, "postText",  0x40);
    FindAndStore(s, "imageTable", 0x48);
    FindAndStore(s, "nextButton", 0x50);
    FindAndStore(s, "quitButton", 0x58);

    // Set button IDs
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x50), 0x578);  // nextButton = 1400
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x58), 0x579);  // quitButton = 1401

    // Hide the "buttonClose" element in the dialog instance
    char closeName[8] = {};
    fn_String_Ctor_CStr(closeName, "buttonClose");
    uint64_t closeElem = 0;
    fn_DialogInstance_FindElement((void*)*(uint64_t*)(s + 0x18), &closeElem, closeName);
    // SetFlagsOnOff(elem, hideFlags=1, showFlags=0) — hide element
    fn_Element_SetFlagsOnOff((void*)closeElem, 1, 0);
    fn_String_Dtor(closeName);
}

// OnRemove (vtable +0x48): Quit tutorial button handler.
// On button click (eventType=0x12) for quitButton (id=0x579):
//   - Destroys the SaveLoad singleton stored at game+0x140.
//   - Allocates and constructs three nested loop objects:
//       MenuDialogLoopT (0xc00): "Dialog/MainMenu.rsc" — the main menu dialog loop
//       GameLoadDialogLoopT (0xb60): "Dialog/Loading.rsc" — loading screen wrapper
//       GameLoaderLoop (0x48): outermost loader, queued as engine's next loop
//   - Sets fullscreen fade to black and transitions back to main menu.
// Called from reimpl/tutorialui.cpp Reimpl_OnRemove.
void TutorialUI_OnRemove(TutorialUI* self, void* param2, int eventType, int eventId) {
    (void)self;
    (void)param2;
    if (eventType != 0x12 || eventId != 0x579)
        return;

    // Grab the engine singleton (DAT_1401c3dd8 = *g_pGameWorldSingleton)
    uintptr_t eng = (uintptr_t)*g_pGameWorldSingleton;

    // Destroy and free the SaveLoad object stored at eng+0x140
    void* saveLoad = *(void**)(eng + 0x140);
    if (saveLoad) {
        fn_SaveLoad_Dtor(saveLoad);
        fn_MemFreeStatic(saveLoad);
    }
    *(void**)(eng + 0x140) = nullptr;

    // --- Step 1: Allocate and construct the MenuDialogLoopT (0xc00 bytes) ---
    // This is the main menu dialog loop that will display after the loading screen.
    void* menuLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xc00, 8);
    void* pMenuLoop = nullptr;
    if (menuLoop) {
        uint64_t menuNameStr = 0;
        fn_String_Ctor_CStr(&menuNameStr, "Dialog/MainMenu.rsc");
        fn_MenuDialogLoopT_Ctor(menuLoop, &menuNameStr);
        fn_String_Dtor(&menuNameStr);
        pMenuLoop = menuLoop;
    }

    // --- Step 2: Allocate and construct the GameLoadDialogLoopT (0xb60 bytes) ---
    // Wraps the menu loop; shows "Dialog/Loading.rsc" while the menu is loading.
    void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
    void* pDlLoop = nullptr;
    if (dlLoop) {
        // Delay constant is 1.0f (DAT_14017214c — bytes 0x3f800000).
        uint64_t delayTime[1];
        fn_Time_Ctor_Float((void*)delayTime, 1.0f);

        uint64_t loadingNameStr = 0;
        fn_String_Ctor_CStr(&loadingNameStr, "Dialog/Loading.rsc");
        fn_DialogLoaderLoopT_Ctor(dlLoop, &loadingNameStr, pMenuLoop, (void*)delayTime);
        fn_String_Dtor(&loadingNameStr);

        // Patch vtable to exe subclass (GameLoadDialogLoopT)
        *(void**)dlLoop = (void*)g_GameLoadDialogLoopT_Vtable;

        // Initialize extra fields added by the exe subclass
        uintptr_t dl = (uintptr_t)dlLoop;
        *(uint32_t*)(dl + 0x188) = 0xff000000;
        *(uint32_t*)(dl + 0x18c) = 0xff000000;
        *(uint8_t*)(dl  + 0x190) = 0;   // [400] = 0
        fn_Random_Default_Ctor((void*)(dl + 0x194));
        *(uint64_t*)(dl + 0xb58) = 0;
        *(uint8_t*)(dl  + 0x40)  = 0;   // m_finished override = 0

        pDlLoop = dlLoop;
    }

    // --- Step 3: Allocate and construct the outer GameLoaderLoop (0x48 bytes) ---
    // Wraps the GameLoadDialogLoopT with a zero-delay outer loader.
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

    // --- Step 4: Set cursor, fullscreen fade to black, and queue next loop ---
    // FUN_140097fb0: SetCursor(null), SetFullscreenFade(black), store next loop at eng+0xC8.
    {
        void* ui = *(void**)(eng + 0x98);
        uint64_t nullCursor = *(uint64_t*)((uintptr_t)(*g_pKeyMapSingleton) + 0x38);
        if (fn_UI_SetCursor) fn_UI_SetCursor(ui, nullCursor);
        if (fn_UI_SetFullscreenFade) fn_UI_SetFullscreenFade(ui, true, 0xFF000000, false);
        *(void**)(eng + 0xC8) = pOuterLoop;
    }
}

// Startup (vtable +0x38): Toggle visibility of next/quit buttons.
// If the element has visibility flags (0x1800000) set, hide it; otherwise show it.
void TutorialUI_Startup(TutorialUI* self) {
    uintptr_t s = (uintptr_t)self;

    // nextButton (+0x50): check flags at element +0x34
    void* nextBtn = (void*)*(uint64_t*)(s + 0x50);
    bool nextHidden = (*(uint32_t*)((uintptr_t)nextBtn + 0x34) & 0x1800000) != 0;
    fn_Element_SetFlagsOnOff(nextBtn, nextHidden ? 1 : 0, nextHidden ? 0 : 1);

    // quitButton (+0x58): same check
    void* quitBtn = (void*)*(uint64_t*)(s + 0x58);
    bool quitHidden = (*(uint32_t*)((uintptr_t)quitBtn + 0x34) & 0x1800000) != 0;
    fn_Element_SetFlagsOnOff(quitBtn, quitHidden ? 1 : 0, quitHidden ? 0 : 1);
}
