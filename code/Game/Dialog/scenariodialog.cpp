// engine/src/ui/scenariodialog.cpp
// Clean reimplementation of ScenarioDialog methods.
//
// ScenarioDialog (0xE8 bytes): Scenario selection screen.
// Layout: +0xC8 = selected index (int32), +0xD8 = scenario array,
//         +0xE0 = scenario count

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// File-local helper: ScenarioDialog_ArrayPush_Impl (FUN_140013b60)
// ============================================================
// Resizes dynamic array to newCount entries (stride 0x38 bytes, no String fields).
// Entry layout: 7 × uint64 (plain-copy, no String ctor needed).
// arrayBase layout: [data* 8][count int32][capacity int32]
// Original: 0x140013b60.
static void ScenarioDialog_ArrayPush_Impl(void* arrayBase, int newCount, void* entry7) {
    uintptr_t* dataPtr  = (uintptr_t*)arrayBase;
    int* countPtr       = (int*)((uintptr_t)arrayBase + 8);
    int* capacityPtr    = (int*)((uintptr_t)arrayBase + 0xc);
    int oldCount        = *countPtr;

    if (newCount == oldCount) return;

    void* oldData = (void*)*dataPtr;
    *countPtr = newCount;

    if (*capacityPtr < newCount) {
        *capacityPtr = newCount;
        if (oldCount != 0 && newCount == oldCount + 1) {
            *capacityPtr = newCount * 3;
        }
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)*capacityPtr * 0x38, 8);
        *dataPtr = (uintptr_t)newData;
    }

    int startCopy = (oldData == (void*)*dataPtr) ? oldCount : 0;
    for (int i = startCopy; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*dataPtr + (uintptr_t)i * 0x38);
        if (dst != nullptr) {
            uint64_t* src = (i < oldCount)
                ? (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 0x38)
                : (uint64_t*)entry7;
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
            dst[4] = src[4]; dst[5] = src[5]; dst[6] = src[6];
        }
    }

    if (oldData != (void*)*dataPtr && oldData != nullptr) {
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// ScenarioDialog::Slot4 (vtable +0x20) — Setup
// ============================================================
// Populates the scenario selection table. For each scenario matching
// the current difficulty (self+0xB8), creates a table row with icon,
// title, description, and selection button. Stores 0x38-byte entries
// in the dynamic array at +0xD8.
//
// Original: 0x140078010 (348 instructions, retail).
// Profile: FUN_14005f490 — confirms element names:
//   "buttonCancel", "buttonOk", "tableScenario", "imageIcon",
//   "textTitle", "textDescription", "selectSave"
// Assertion: "table->ChildCount() >= scenarioCount"
// Source: Dialog\ScenarioDialog.cpp, line 0x3B

void ScenarioDialog_Slot4(ScenarioDialog* self) {
    uintptr_t s = (uintptr_t)self;
    uint8_t selfDifficulty = *(uint8_t*)(s + 0xB8);
    printf("[ScenarioDialog::Slot4] Setup called, self=%p, difficulty=%d\n", self, selfDifficulty);

    // --- Find "buttonCancel" → set element ID 100, set +0xC0 flag = 8 ---
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonCancel");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* cancelBtn = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        fn_Element_SetElementId(cancelBtn, 100);
        *(uint32_t*)((uintptr_t)cancelBtn + 0xC0) = 8;
    }

    // --- Find "buttonOk" → store at +0xD0, set ID 0x65 (101), disable it ---
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonOk");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* okBtn = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        *(void**)(s + 0xD0) = okBtn;
        fn_Element_SetElementId(okBtn, 0x65);

        // Disable via vtable[8]: SetEnabled(false)
        void** vtable = *(void***)okBtn;
        typedef void (*fn_SetEnabled_t)(void*, bool);
        ((fn_SetEnabled_t)vtable[8])(okBtn, false);
    }

    // --- Get scenario list from singleton at *g_pKeyMapSingleton + 8 ---
    // The global at 0x1401c3de8 points to a management struct:
    //   +0x00 = KeyMap, +0x08 = ExternalList (scenario list)
    void* mgmtObj = *g_pKeyMapSingleton;
    void* scenarioList = *(void**)((uintptr_t)mgmtObj + 8);

    // --- Find "tableScenario" element ---
    void* table = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "tableScenario");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        table = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // --- Get scenario count and iterate ---
    int scenarioCount = fn_ExternalList_GetResourceCount(scenarioList);
    int entryIndex = 0;  // counts matched scenarios (for button IDs and array)
    printf("[ScenarioDialog::Slot4] scenarioCount=%d, mgmtObj=%p, scenarioList=%p\n",
           scenarioCount, mgmtObj, scenarioList);

    for (int i = 0; i < scenarioCount; i++) {
        // Get resource[i] from ExternalList: *(array+0x20)[i*8]
        void* resource = *(void**)(*(uintptr_t*)((uintptr_t)scenarioList + 0x20) + (uintptr_t)i * 8);

        // Check IsDerivedFrom(Scenario)
        if (!g_TypeId_Scenario) continue;
        bool isScenario = fn_IsDerivedFrom(resource, *g_TypeId_Scenario);
        void* scenario = isScenario ? resource : nullptr;
        if (!scenario) continue;

        // Filter by difficulty: scenario+0x69 must match self+0xB8
        uint8_t scenDifficulty = *(uint8_t*)((uintptr_t)scenario + 0x69);
        uint8_t selfDifficultyVal = *(uint8_t*)(s + 0xB8);
        printf("[ScenarioDialog::Slot4] scenario[%d] at %p: difficulty=%d (want %d), isScenario=%d\n",
               i, scenario, scenDifficulty, selfDifficultyVal, isScenario);
        if (scenDifficulty != selfDifficultyVal) continue;

        // --- Clone template element from Dialog ---
        // GetTemplate(dialog, retval, 0)
        uint8_t templateBuf[8];
        fn_Dialog_GetTemplate(*(void**)(s + 0x18), templateBuf, 0);

        // Clone via vtable[8]: templateObj->Clone(&cloneBuf, dialog)
        void* templateObj = *(void**)templateBuf;
        void** templateVt = *(void***)templateObj;
        typedef void (*fn_Clone_t)(void*, void*, void*);
        uint8_t cloneBuf[16];
        ((fn_Clone_t)templateVt[8])(templateObj, cloneBuf, *(void**)(s + 0x18));
        void* clonedElem = *(void**)cloneBuf;

        // --- Insert clone into table ---
        uint8_t clonePtr[8];
        *(void**)clonePtr = clonedElem;
        fn_TableElement_SetChild2(table, clonePtr, entryIndex);

        // --- Build 0x38-byte entry ---
        // Layout: +0x00 = element ptr, +0x08 = selectSave ptr,
        //         +0x10 = imageIcon ptr, +0x18 = textTitle ptr,
        //         +0x20 = textDescription ptr, +0x28 = scenario ptr,
        //         +0x30 = resource index
        uint8_t entry[0x38];
        memset(entry, 0, 0x38);

        // entry[0] = cloned element
        *(void**)entry = clonedElem;

        // Set element hidden initially (flags 0x10000 = hidden)
        fn_Element_SetFlagsOnOff(clonedElem, 0x10000, 0);

        // Set element ID = entryIndex + 1000
        fn_Element_SetElementId(clonedElem, entryIndex + 1000);

        // --- Find sub-elements in the clone ---

        // "imageIcon" → entry[0x10]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "imageIcon");
            uint8_t retBuf[16];
            fn_FindElement(clonedElem, retBuf, nameStr);
            *(void**)(entry + 0x10) = *(void**)retBuf;
            fn_String_Dtor(nameStr);
        }

        // "textTitle" → entry[0x18]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "textTitle");
            uint8_t retBuf[16];
            fn_FindElement(clonedElem, retBuf, nameStr);
            *(void**)(entry + 0x18) = *(void**)retBuf;
            fn_String_Dtor(nameStr);
        }

        // "textDescription" → entry[0x20]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "textDescription");
            uint8_t retBuf[16];
            fn_FindElement(clonedElem, retBuf, nameStr);
            *(void**)(entry + 0x20) = *(void**)retBuf;
            fn_String_Dtor(nameStr);
        }

        // "selectSave" → entry[0x08]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "selectSave");
            uint8_t retBuf[16];
            fn_FindElement(clonedElem, retBuf, nameStr);
            *(void**)(entry + 0x08) = *(void**)retBuf;
            fn_String_Dtor(nameStr);
        }

        // Store scenario pointer and resource index
        *(void**)(entry + 0x28) = scenario;
        *(int*)(entry + 0x30) = i;

        // --- Set icon sprite from scenario ---
        // GetSpriteName returns String (non-trivial return: RCX=this, RDX=retval)
        {
            uint8_t spriteNameStr[8];
            fn_Achievement_GetSpriteName(scenario, spriteNameStr);

            // Create UI::Sprite(retval, spriteSheet, spriteName)
            // spriteSheet = *(scenario + 0x38)
            uint64_t spriteSheet = *(uint64_t*)((uintptr_t)scenario + 0x38);
            uint8_t sprite[16];
            fn_Sprite_Ctor(sprite, spriteSheet, spriteNameStr);

            // SetSprite on imageIcon
            void* imageIcon = *(void**)(entry + 0x10);
            fn_ImageElement_SetSprite(imageIcon, sprite);

            // Clean up sprite name string
            fn_String_Dtor(spriteNameStr);
        }

        // --- Set title text from scenario ---
        {
            uint8_t titleStr[8];
            fn_Achievement_GetTitle(scenario, titleStr);

            // Create UI::String(retval, stringTable, titleStr)
            // stringTable = *(scenario + 0x20)
            uint64_t stringTable = *(uint64_t*)((uintptr_t)scenario + 0x20);
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, stringTable, titleStr);

            // SetText on textTitle
            void* textTitle = *(void**)(entry + 0x18);
            fn_TextElement_SetText(textTitle, uiStr);

            fn_String_Dtor(titleStr);
        }

        // --- Set description text from scenario ---
        {
            uint8_t descStr[8];
            fn_Achievement_GetDescription(scenario, descStr);

            // Create UI::String(retval, stringTable, descStr)
            uint64_t stringTable = *(uint64_t*)((uintptr_t)scenario + 0x20);
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, stringTable, descStr);

            // SetText on textDescription
            void* textDesc = *(void**)(entry + 0x20);
            fn_TextElement_SetText(textDesc, uiStr);

            fn_String_Dtor(descStr);
        }

        // --- Push entry to dynamic array at self+0xD8 ---
        int newCount = *(int*)(s + 0xE0) + 1;
        ScenarioDialog_ArrayPush_Impl((void*)(s + 0xD8), newCount, entry);

        printf("[ScenarioDialog::Slot4] Added entry[%d] for scenario[%d]\n", entryIndex, i);
        entryIndex++;
    }
    printf("[ScenarioDialog::Slot4] Done. Total entries: %d\n", entryIndex);
}

// ============================================================
// ScenarioDialog::Slot7 (vtable +0x38, exe 0x140078000): RET 0 (3 bytes)
// ============================================================
// No-op handler; returns 0.
int ScenarioDialog_Slot7(ScenarioDialog* self) {
    (void)self;
    return 0;
}

// ============================================================
// ScenarioDialog::Slot6 (vtable +0x30) — Update entry visibility
// ============================================================
// Iterates scenario entries at +0xD8 (stride 0x38), shows entry matching
// selected index at +0xC8, hides all others. Enables/disables confirm
// button at +0xD0 based on whether a valid selection exists.
//
// Original: 0x140077BE0.

static int s_slot6LogCount = 0;
void ScenarioDialog_Slot6(ScenarioDialog* self) {
    uintptr_t s = (uintptr_t)self;

    int count = *(int*)(s + 0xE0);
    int selected = *(int*)(s + 0xC8);
    if (s_slot6LogCount < 3) {
        printf("[ScenarioDialog::Slot6] Draw: count=%d, selected=%d\n", count, selected);
        s_slot6LogCount++;
    }
    uintptr_t entryArray = *(uintptr_t*)(s + 0xD8);

    // Show selected entry, hide all others
    for (int i = 0; i < count; i++) {
        void* elem = *(void**)(entryArray + 8 + (uintptr_t)i * 0x38);
        bool isNotSelected = (i != selected);
        fn_Element_SetFlagsOnOff(elem, isNotSelected ? 1 : 0, isNotSelected ? 0 : 1);
    }

    // Enable/disable confirm button based on valid selection
    void* confirmBtn = (void*)*(uint64_t*)(s + 0xD0);
    void** vtable = *(void***)confirmBtn;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable[8])(confirmBtn, selected >= 0);
}

// ============================================================
// ScenarioDialog_LoadSelected — reimplementation of FUN_140077cb0
// ============================================================
// Loads the selected scenario. Two paths:
// 1. New scenario (no save name): reads terrain/climate/seed params from scenario
//    resource and starts a new game via StartNewGame logic.
// 2. Saved scenario (has save name): loads the save file first, then uses the
//    saved state to start the game.
//
// Entry array: self+0xD8 = data*, self+0xE0 = count, stride 0x38 bytes.
// Entry layout: +0x28 = scenario resource pointer.
// Scenario resource: +0x58 = display name, +0x60 = seed, +0x64 = terrainSize,
//   +0x68 = disasterOff, +0x70 = climate, +0x78 = terrain, +0x80 = mapGen,
//   +0x88 = save name string (empty = new scenario).

static void ScenarioDialog_LoadSelected(ScenarioDialog* self) {
    uintptr_t s = (uintptr_t)self;
    printf("[ScenarioDialog::LoadSelected] Called, self=%p\n", self);

    int selectedIdx = *(int*)(s + 0xC8);  // +0xC8 = selectedIndex (200 decimal = 0xC8)
    uintptr_t entryArray = *(uintptr_t*)(s + 0xD8);
    uintptr_t scenario = *(uintptr_t*)(entryArray + (uintptr_t)selectedIdx * 0x38 + 0x28);

    // Get save name string from scenario+0x88
    uint8_t saveNameStr[8];
    fn_String_Ctor_CStr(saveNameStr, "");
    fn_String_Assign(saveNameStr, (void*)(scenario + 0x88));

    // Get display name string from scenario+0x58
    uint8_t displayNameStr[8];
    fn_String_Ctor_CStr(displayNameStr, "");
    fn_String_Assign(displayNameStr, (void*)(scenario + 0x58));

    uint64_t saveStrPtr = *(uint64_t*)saveNameStr;
    bool hasSaveState = (saveStrPtr != 0 && *(int*)(saveStrPtr - 8) > 0);
    printf("[ScenarioDialog::LoadSelected] selectedIdx=%d, scenario=%p, hasSaveState=%d, saveStrPtr=%p\n",
           selectedIdx, (void*)scenario, hasSaveState, (void*)saveStrPtr);

    if (!hasSaveState) {
        // New scenario: start fresh game with scenario parameters
        // Read scenario params
        uint32_t seedVal = *(uint32_t*)(scenario + 0x60);
        uint32_t terrainSizeVal = *(uint32_t*)(scenario + 0x64);
        char disasterOff = *(char*)(scenario + 0x68);
        // Scenario generator triple. These offsets were labelled climate/terrain/map;
        // retail's own LoadSelected (0x140077cb0, the call at +0x243) proves the outer
        // two are the other way round: it passes +0x70 as arg6 and +0x80 as arg8, and
        // 0x1400b4540 stores args 6/7/8 into GameUpdateLoop +0x40/+0x48/+0x50, which
        // Load reads as mapGen / terrainGen / climateGen. The positional order passed
        // below is unchanged and already matched retail - only the names were wrong.
        void* mapGen = *(void**)(scenario + 0x70);
        void* terrainGen = *(void**)(scenario + 0x78);
        void* climateGen = *(void**)(scenario + 0x80);

        // Get scenario index from entry array: +0x30 in the entry
        uint32_t scenarioResIdx = *(uint32_t*)(entryArray + (uintptr_t)selectedIdx * 0x38 + 0x30);

        // Get parent controller (self+0xC0 is InternalBasePtr to parent MainMenuDialog)
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
        void* parentCtrl = *(void**)(s + 0xC0);

        // Build InternalBasePtr(null) for retval
        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);

        printf("[ScenarioDialog::LoadSelected] NEW SCENARIO: seed=%u, terrainSize=%u, disasterOff=%d, scenarioResIdx=%u\n",
               seedVal, terrainSizeVal, (int)disasterOff, scenarioResIdx);
        printf("[ScenarioDialog::LoadSelected] mapGen=%p, terrainGen=%p, climateGen=%p, parentCtrl=%p\n",
               mapGen, terrainGen, climateGen, parentCtrl);

        // Call original FUN_1400b4540 directly — constructs GameUpdateLoop ->
        // GameLoadDialogLoopT -> LoaderLoop chain and sets next loop.
        fn_StartNewGame(parentCtrl, displayNameStr, seedVal, terrainSizeVal,
                        disasterOff, mapGen, terrainGen, climateGen,
                        scenarioResIdx, nullIBP);
        printf("[ScenarioDialog::LoadSelected] fn_StartNewGame returned\n");
    } else {
        // Saved scenario: load the save file first, then start from saved state
        void* saveLoad = *g_pSaveLoadSingleton;
        fn_SaveLoad_SetOperation(saveLoad, 3, 0);
        fn_SaveLoad_SetSaveName(saveLoad, saveNameStr, 2);

        // Get save path and set data directory
        // Original flow (from assembly at 0x140077d41-0x140077d96):
        //   1. GetSavePath → Filename at RSP+0xa8 (kept alive until after loop)
        //   2. GetDataDirectory → temp at RSP+0xb0, returns ref in RAX
        //   3. persistence+0x90 = RAX (data dir)
        //   4. Destroy RSP+0xb0 (GetDataDirectory temp)
        //   5. SaveLoad::Update loop
        //   6. persistence+0x90 = RSP+0xa8 (restore from GetSavePath result)
        void* persistence = *g_pPersistenceSingleton;
        uint8_t savePathBuf[16];  // Filename return buffer (non-trivial return ABI)
        fn_Persistence_GetSavePath(persistence, savePathBuf);
        void* fileSystem = *g_pFileSystemSingleton2;
        uint8_t dataDirBuf[16];  // String return buffer (non-trivial return ABI)
        void* dataDir = fn_FileSystem_GetDataDirectory(fileSystem, dataDirBuf);
        fn_String_Assign((void*)(*(uintptr_t*)g_pPersistenceSingleton + 0x90), dataDir);
        fn_Filename_Dtor(dataDirBuf);  // destroy GetDataDirectory temp (NOT savePathBuf!)

        // Run SaveLoad::Update loop until operation completes
        int operation = fn_SaveLoad_GetOperation(saveLoad);
        printf("[ScenarioDialog::LoadSelected] SAVED SCENARIO: entering SaveLoad loop, initial operation=%d\n", operation);
        int loopCount = 0;
        while (operation != 0) {
            uint8_t zeroTime[8];
            fn_Time_Ctor_Float(zeroTime, 0.0f);
            fn_SaveLoad_Update(saveLoad, zeroTime);
            operation = fn_SaveLoad_GetOperation(saveLoad);
            loopCount++;
            if (loopCount % 100 == 0) {
                printf("[ScenarioDialog::LoadSelected] SaveLoad loop iteration %d, operation=%d\n", loopCount, operation);
            }
        }
        printf("[ScenarioDialog::LoadSelected] SaveLoad loop done after %d iterations\n", loopCount);

        // Restore persistence path from GetSavePath result (matches original at 0x140077df0)
        fn_String_Assign((void*)(*(uintptr_t*)g_pPersistenceSingleton + 0x90), savePathBuf);
        fn_Filename_Dtor(savePathBuf);  // destroy GetSavePath Filename after use

        // Get parent controller and load from saved state
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
        void* parentCtrl = *(void**)(s + 0xC0);

        uint8_t nullIBP[8];
        fn_InternalBasePtr_Ctor(nullIBP, nullptr);

        uint32_t scenarioResIdx = *(uint32_t*)(entryArray + (uintptr_t)selectedIdx * 0x38 + 0x30);
        void* savedState = fn_SaveLoad_GetSaveState(saveLoad);

        // Use fn_LoadScenarioFromSave for the complex loop construction
        fn_LoadScenarioFromSave(parentCtrl, savedState,
                                 (void*)(uintptr_t)scenarioResIdx, nullIBP);
    }

    fn_String_Dtor(displayNameStr);
    fn_String_Dtor(saveNameStr);
}

// ============================================================
// ScenarioDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// Slot8 (0x140077F80): OnCommand — selection change and button dispatch.
// param3=2 (selection change): updates selected index from button IDs 1000+
// param3=0x12 (button click): id=100 → close, id=101 → load selected scenario
void ScenarioDialog_Slot8(ScenarioDialog* self, uint64_t param2,
                          int param3, int param4) {
    uintptr_t s = (uintptr_t)self;
    printf("[ScenarioDialog::Slot8] OnCommand: param3=0x%x, param4=%d\n", param3, param4);

    if (param3 == 2) {
        // Selection change — button IDs are 1000 + index
        int maxIndex = *(int*)(s + 0xE0);
        if (param4 > 999 && param4 < maxIndex + 1000) {
            *(int*)(s + 0xC8) = param4 - 1000;
        }
    } else if (param3 == 0x12) {
        if (param4 == 100) {
            // Close button
            uint8_t basePtr[16];
            fn_InternalBasePtr_Ctor(basePtr, self);
            void* ui = *(void**)(s + 0x10);
            fn_UI_CloseDialog(ui, basePtr);
        } else if (param4 == 0x65) {
            // Confirm/load button — reimpl of FUN_140077cb0
            ScenarioDialog_LoadSelected(self);
        }
    }
}

// ============================================================
// ScenarioDialog vtable + ctor (was ResolveAddr(Module::Exe, 0x1400786b0) /
// unregistered vtable_ScenarioDialog @ 0x14015d2d0)
// ============================================================
// ScenarioDialog_CopyCtor (0x1400786b0) resolved to null in standalone, so both
// MainMenuDialog_OpenScenario and MainMenuDialog_OpenTutorial called it UNGUARDED
// (EXEC-of-0). It also hardcodes `self->vtable = &vtable_ScenarioDialog` inline —
// that raw .rdata address (0x14015d2d0) was itself never registered, so even a
// null-safe ctor would have handed the dialog a null vtable. Both are ported here.
//
// vtable_ScenarioDialog layout, dumped straight from .rdata (tools/dump_vtable.py
// 0x14015d2d0) — same 10-slot shape as AchievementDialog/GoalDialog
// (menu_dialog_standalone_vtables.cpp):
//   [0] 0x140013af0 ScalarDtor   (frees +0xD8 array, NOT a bare base thunk)
//   [1] 0x140013960 static getter (returns DAT_1401c5560 — use GetTypeIdByName instead)
//   [2] 0x140013950 GetTypeId     (returns DAT_1401c5558 — ditto)
//   [3] 0x14003a948 Persist       (base thunk)
//   [4] 0x140078010 Setup         -> ScenarioDialog_Slot4 (ported)
//   [5] 0x14003a94e Shutdown      (base thunk)
//   [6] 0x140077be0 OnUpdate      -> ScenarioDialog_Slot6 (ported)
//   [7] 0x140078000 OnUpdateLayout-> ScenarioDialog_Slot7 (ported)
//   [8] 0x140077f80 OnCommand     -> ScenarioDialog_Slot8 (ported)
//   [9] 0x14003a954 OnSerialize   (base thunk)
namespace {

void SD_Nop(void*, ...) {}

void* GetScenarioVtable();

// ScenarioDialog_ScalarDtor (0x140013af0): unlike the Achievement/Goal base-thunk
// dtors, this one frees the +0xD8 scenario entry array before tearing down the
// DialogInstance base (entries are plain-copy 7xu64, no per-entry String dtor —
// see ScenarioDialog_ArrayPush_Impl above).
void* SD_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = GetScenarioVtable();
    uintptr_t s = (uintptr_t)self;
    if (fn_MemFreeStatic) fn_MemFreeStatic(*(void**)(s + 0xD8));
    *(uint64_t*)(s + 0xD8) = 0;
    *(uint64_t*)(s + 0xE0) = 0;
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}
uint32_t SD_TypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ScenarioDialog");
    return c;
}
void SD_Slot4(void* self) { ScenarioDialog_Slot4(reinterpret_cast<ScenarioDialog*>(self)); }
void SD_Slot6(void* self) { ScenarioDialog_Slot6(reinterpret_cast<ScenarioDialog*>(self)); }
int  SD_Slot7(void* self) { return ScenarioDialog_Slot7(reinterpret_cast<ScenarioDialog*>(self)); }
void SD_Slot8(void* self, uint64_t p2, int p3, int p4) {
    ScenarioDialog_Slot8(reinterpret_cast<ScenarioDialog*>(self), p2, p3, p4);
}

void* g_ScenarioDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&SD_Dtor),      // +0x00 idx0 ScalarDtor
    reinterpret_cast<void*>(&SD_Nop),       // +0x08 idx1 static getter (base thunk)
    reinterpret_cast<void*>(&SD_TypeId),    // +0x10 idx2 GetTypeId
    reinterpret_cast<void*>(&SD_Nop),       // +0x18 idx3 Persist (base thunk)
    reinterpret_cast<void*>(&SD_Slot4),     // +0x20 idx4 Setup
    reinterpret_cast<void*>(&SD_Nop),       // +0x28 idx5 Shutdown (base thunk)
    reinterpret_cast<void*>(&SD_Slot6),     // +0x30 idx6 OnUpdate
    reinterpret_cast<void*>(&SD_Slot7),     // +0x38 idx7 OnUpdateLayout
    reinterpret_cast<void*>(&SD_Slot8),     // +0x40 idx8 OnCommand
    reinterpret_cast<void*>(&SD_Nop),       // +0x48 idx9 OnSerializeSettings (base thunk)
};
void* GetScenarioVtable() { return reinterpret_cast<void*>(g_ScenarioDialog_Vtable_Standalone); }

}  // namespace

void* ScenarioDialog_GetStandaloneVtable() { return GetScenarioVtable(); }

// ScenarioDialog_CopyCtor (0x1400786b0): shared ctor for both ScenarioDialog and
// TutorialDialog (same class; difficulty byte at +0xB8 differentiates the two —
// MainMenuDialog_OpenScenario passes 0, MainMenuDialog_OpenTutorial passes 1).
//
// Byte-faithful to the disasm:
//   base ctor: fn_DialogInstance_Ctor(self, ui, extRes, &(*parentIBP), 0, 0, 0)
//   self->vtable = vtable_ScenarioDialog
//   +0xB8 (u8)  = difficulty
//   +0xC0 (u64) = *parentMenuIBP   (selection's parent-controller InternalBasePtr)
//   +0xC8 (i32) = -1               (selectedIndex, matches Slot8's read of +0xC8)
//   +0xD0 (u64) = 0
//   +0xD8 (u64) = 0                (scenario entry array, matches ArrayPush_Impl)
//   +0xE0 (u64) = 0                (scenario count, matches Slot4's read of +0xE0)
void* ScenarioDialog_Ctor(void* self, void* ui, void* extRes, void* parentIBP,
                          void* parentMenuIBP, uint8_t difficulty) {
    if (!self) return nullptr;
    uint64_t parentBuf = parentIBP ? *(uint64_t*)parentIBP : 0;
    fn_DialogInstance_Ctor(self, ui, extRes, &parentBuf, nullptr, nullptr, 0);

    uintptr_t s = (uintptr_t)self;
    *reinterpret_cast<void**>(self) = GetScenarioVtable();
    *(uint8_t*)(s + 0xB8) = difficulty;
    *(uint64_t*)(s + 0xC0) = parentMenuIBP ? *(uint64_t*)parentMenuIBP : 0;
    *(int32_t*)(s + 0xC8) = -1;
    *(uint64_t*)(s + 0xD0) = 0;
    *(uint64_t*)(s + 0xD8) = 0;
    *(uint64_t*)(s + 0xE0) = 0;
    return self;
}
