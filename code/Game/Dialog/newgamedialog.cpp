// code/Game/Dialog/newgamedialog.cpp
// Clean reimplementation of NewGameDialog methods.
//
// NewGameDialog (0xAE8 bytes): New game setup dialog.
// Layout:
//   +0x00..+0xB7 = DialogInstance base
//   +0xB8..+0xF8 = UI element pointers (combos, edits, buttons, text)
//   +0x100..+0x110 = ExternalList ptrs (map/climate/terrain generators)
//   +0x118..+0xAD7 = System::Random sub-object
//   +0xAE0 = world setup ptr
//
// Original source: C:\Projects\Buildy\code\Game\Dialog/NewGameDialog.h
//                  Dialog\NewGameDialog.cpp

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
// Helper: populate combo from ExternalList with type check
// ============================================================
static void PopulateGeneratorCombo(void** comboFieldPtr, void* externalList,
                                    uint32_t typeId, int visibilityOffset) {
    int count = fn_ExternalList_GetResourceCount(externalList);
    for (int i = 0; i < count; i++) {
        // ExternalList+0x20 holds pointer to the resource pointer array
        void** ptrArray = *(void***)((uintptr_t)externalList + 0x20);
        void* resource = ptrArray[i];

        // IsDerivedFrom check
        void* typed = nullptr;
        if (fn_IsDerivedFrom(resource, typeId)) {
            typed = resource;
        }

        // Check visibility flag at offset (0x38 for Map, 0x30 for Climate/Terrain)
        if (typed && *(uint8_t*)((uintptr_t)typed + visibilityOffset) != 0) {
            // Build UI::String from resource name (at +0x20 stringTable, +0x28 stringName)
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, *(uint64_t*)((uintptr_t)typed + 0x20),
                              (void*)((uintptr_t)typed + 0x28));

            // Convert UI::String to System::String
            uint8_t sysStr[8];
            fn_UIString_ToSystemString(uiStr, sysStr);

            fn_InternalBasePtr_CheckPtr(comboFieldPtr);
            void* combo = *comboFieldPtr;

            // Create empty sprite
            uint8_t sprite[16];
            fn_Sprite_Ctor(sprite, 0, nullptr);

            fn_ComboElement_AddItem(combo, sprite, sysStr, (void*)(intptr_t)i);
            fn_String_Dtor(sysStr);
        }

        // Re-fetch count in case it changed (matches original)
        count = fn_ExternalList_GetResourceCount(externalList);
    }
}

// ============================================================
// Helper: generate random name with length limit
// ============================================================
static void GenerateRandomName(void* editNamePtr, void* random) {
    fn_InternalBasePtr_CheckPtr(&editNamePtr);
    void* editElem = *(void**)&editNamePtr;
    // EditElement+0xD0 contains max char count
    int maxLen = *(int*)((uintptr_t)editElem + 0xD0);

    // Original formula: (maxLen / 3 + adjustment) * 2
    // Integer division with sign correction
    int third = maxLen / 3;
    int signAdj = (maxLen >> 31);  // sign bit
    int divResult = third + signAdj;
    int longDivAdj = (int)(((int64_t)maxLen / 3 + ((int64_t)maxLen >> 63)) & 0xFFFFFFFF);
    int finalAdj = (int)((uint32_t)longDivAdj >> 31);
    int nameLen = (divResult + finalAdj) * 2;

    uint8_t nameStr[8];
    fn_GenerateTownName(0, nameStr, random, nameLen);

    fn_InternalBasePtr_CheckPtr(&editNamePtr);
    fn_EditElement_SetText(*(void**)&editNamePtr, nameStr);
    fn_String_Dtor(nameStr);
}

// ============================================================
// Helper: generate random seed
// ============================================================
static void GenerateRandomSeed(void* editSeedPtr, void* random) {
    fn_InternalBasePtr_CheckPtr(&editSeedPtr);
    // NumberEditElement+0xFC contains max value
    int maxVal = *(int*)((uintptr_t)*(void**)&editSeedPtr + 0xFC);

    fn_InternalBasePtr_CheckPtr(&editSeedPtr);
    void* seedElem = *(void**)&editSeedPtr;
    uint32_t seed = fn_Random_Rand(random, (uint32_t)(maxVal + 1));
    fn_NumberEditElement_SetValue(seedElem, seed);
}

// ============================================================
// Helper: update description text from selected map generator
// ============================================================
static void UpdateDescription(NewGameDialog* self) {
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr(&self->comboCondition);
    void* condCombo = self->comboCondition;

    // Get selected map generator resource
    void** ptrArray = *(void***)(*(uintptr_t*)self->mapGeneratorList + 0x20);
    int selectedIdx = *(int*)((uintptr_t)condCombo + 0x118);
    void* resource = ptrArray[selectedIdx];

    void* mapGen = nullptr;
    if (fn_IsDerivedFrom(resource, *g_TypeId_MapGenerator)) {
        mapGen = resource;
    }

    // Build UI::String from description (at +0x20 stringTable, +0x30 stringName)
    uint8_t uiStr[16];
    fn_UIString_Ctor(uiStr, *(uint64_t*)((uintptr_t)mapGen + 0x20),
                      (void*)((uintptr_t)mapGen + 0x30));

    // Set on textDescription element
    fn_InternalBasePtr_CheckPtr(&self->textDescription);
    fn_TextElement_SetText(self->textDescription, uiStr);
}

// ============================================================
// Slot4 (0x1400706d0): Startup — build new game UI
// ============================================================
// Finds all UI elements, populates combo boxes with map/climate/terrain
// generators, generates random name and seed, restores saved combo
// selections from game settings.
//
// Original: 315 lines decompiled.

void NewGameDialog_Slot4(NewGameDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find all UI elements ---
    self->editName       = FindElement(self, "editName");
    self->editSeed       = FindElement(self, "editSeed");
    self->textDescription = FindElement(self, "textDescription");
    self->comboCondition = FindElement(self, "comboCondition");
    self->comboTerrain   = FindElement(self, "comboTerrain");
    self->comboTerrainSize = FindElement(self, "comboTerrainSize");
    self->comboClimate   = FindElement(self, "comboClimate");
    self->comboDisaster  = FindElement(self, "comboDisaster");

    // --- Cache ExternalList pointers from keymap singleton ---
    void* keyMapData = *g_pKeyMapSingleton;
    self->mapGeneratorList     = *(void**)((uintptr_t)keyMapData + 0x18);
    self->climateGeneratorList = *(void**)((uintptr_t)keyMapData + 0x10);
    self->terrainGeneratorList = *(void**)((uintptr_t)keyMapData + 0x20);

    // --- Populate map generator combo (comboCondition) ---
    PopulateGeneratorCombo((void**)&self->comboCondition, self->mapGeneratorList,
                           *g_TypeId_MapGenerator, 0x38);

    // --- Populate climate combo ---
    PopulateGeneratorCombo((void**)&self->comboClimate, self->climateGeneratorList,
                           *g_TypeId_ClimateGenerator, 0x30);

    // --- Populate terrain combo ---
    PopulateGeneratorCombo((void**)&self->comboTerrain, self->terrainGeneratorList,
                           *g_TypeId_TerrainGenerator, 0x30);

    // --- Find button elements ---
    void* buttonNewName = FindElement(self, "buttonNewName");
    void* buttonNewSeed = FindElement(self, "buttonNewSeed");
    void* buttonCancel  = FindElement(self, "buttonCancel");
    void* buttonOk      = FindElement(self, "buttonOk");

    // --- Assign element IDs ---
    // buttonNewName = 0, buttonNewSeed = 1, buttonOk = 2, buttonCancel = 3, comboCondition = 4
    fn_InternalBasePtr_CheckPtr(&buttonNewName);
    fn_Element_SetElementId(buttonNewName, 0);

    fn_InternalBasePtr_CheckPtr(&buttonNewSeed);
    fn_Element_SetElementId(buttonNewSeed, 1);

    // Store buttonOk in struct at +0xF0
    self->buttonOk = buttonOk;
    fn_InternalBasePtr_CheckPtr(&self->buttonOk);
    fn_Element_SetElementId(self->buttonOk, 2);

    fn_InternalBasePtr_CheckPtr(&buttonCancel);
    fn_Element_SetElementId(buttonCancel, 3);

    fn_InternalBasePtr_CheckPtr(&self->comboCondition);
    fn_Element_SetElementId(self->comboCondition, 4);

    // --- Generate random name ---
    GenerateRandomName(&self->editName, (void*)(s + 0x118));

    // --- Generate random seed ---
    GenerateRandomSeed(&self->editSeed, (void*)(s + 0x118));

    // --- Restore saved combo selections from game settings ---
    void* settings = *g_pNewGameSettings;
    uintptr_t cfg = (uintptr_t)settings;

    // Map condition/generator combo (+0x2c)
    {
        int idx = *(int*)(cfg + 0x2c);
        void* combo = self->comboCondition;
        if (idx < 0) {
            fn_InternalBasePtr_CheckPtr(&combo);
            int count = fn_ComboElement_GetItemCount(combo);
            if (idx < count) {
                // Use the index as-is
                fn_InternalBasePtr_CheckPtr(&self->comboCondition);
                fn_ComboElement_SetCurrentItem(self->comboCondition, *(int*)(cfg + 0x2c));
            } else {
                fn_InternalBasePtr_CheckPtr(&self->comboCondition);
                fn_ComboElement_SetCurrentItem(self->comboCondition, 0);
            }
        } else {
            fn_InternalBasePtr_CheckPtr(&self->comboCondition);
            fn_ComboElement_SetCurrentItem(self->comboCondition, *(int*)(cfg + 0x2c));
        }
    }

    // Terrain combo (+0x20)
    {
        int idx = *(int*)(cfg + 0x20);
        void* combo = self->comboTerrain;
        if (idx < 0) {
            fn_InternalBasePtr_CheckPtr(&combo);
            int count = fn_ComboElement_GetItemCount(combo);
            if (idx < count) {
                fn_InternalBasePtr_CheckPtr(&self->comboTerrain);
                fn_ComboElement_SetCurrentItem(self->comboTerrain, *(int*)(cfg + 0x20));
            } else {
                fn_InternalBasePtr_CheckPtr(&self->comboTerrain);
                fn_ComboElement_SetCurrentItem(self->comboTerrain, 0);
            }
        } else {
            fn_InternalBasePtr_CheckPtr(&self->comboTerrain);
            fn_ComboElement_SetCurrentItem(self->comboTerrain, *(int*)(cfg + 0x20));
        }
    }

    // Terrain size combo (+0x24)
    {
        int idx = *(int*)(cfg + 0x24);
        void* combo = self->comboTerrainSize;
        if (idx < 0) {
            fn_InternalBasePtr_CheckPtr(&combo);
            int count = fn_ComboElement_GetItemCount(combo);
            if (idx < count) {
                fn_InternalBasePtr_CheckPtr(&self->comboTerrainSize);
                fn_ComboElement_SetCurrentItem(self->comboTerrainSize, *(int*)(cfg + 0x24));
            } else {
                fn_InternalBasePtr_CheckPtr(&self->comboTerrainSize);
                fn_ComboElement_SetCurrentItem(self->comboTerrainSize, 0);
            }
        } else {
            fn_InternalBasePtr_CheckPtr(&self->comboTerrainSize);
            fn_ComboElement_SetCurrentItem(self->comboTerrainSize, *(int*)(cfg + 0x24));
        }
    }

    // Climate combo (+0x28)
    {
        int idx = *(int*)(cfg + 0x28);
        void* combo = self->comboClimate;
        if (idx < 0) {
            fn_InternalBasePtr_CheckPtr(&combo);
            int count = fn_ComboElement_GetItemCount(combo);
            if (idx < count) {
                fn_InternalBasePtr_CheckPtr(&self->comboClimate);
                fn_ComboElement_SetCurrentItem(self->comboClimate, *(int*)(cfg + 0x28));
            } else {
                fn_InternalBasePtr_CheckPtr(&self->comboClimate);
                fn_ComboElement_SetCurrentItem(self->comboClimate, 0);
            }
        } else {
            fn_InternalBasePtr_CheckPtr(&self->comboClimate);
            fn_ComboElement_SetCurrentItem(self->comboClimate, *(int*)(cfg + 0x28));
        }
    }

    // Disaster combo (+0x30)
    {
        int idx = *(int*)(cfg + 0x30);
        void* combo = self->comboDisaster;
        if (idx < 0) {
            fn_InternalBasePtr_CheckPtr(&combo);
            int count = fn_ComboElement_GetItemCount(combo);
            if (count <= idx) {
                fn_InternalBasePtr_CheckPtr(&self->comboDisaster);
                fn_ComboElement_SetCurrentItem(self->comboDisaster, 0);
            } else {
                fn_ComboElement_SetCurrentItem(self->comboDisaster, *(int*)(cfg + 0x30));
            }
        } else {
            fn_InternalBasePtr_CheckPtr(&self->comboDisaster);
            fn_ComboElement_SetCurrentItem(self->comboDisaster, *(int*)(cfg + 0x30));
        }
    }

    // --- Update description text from selected map generator ---
    UpdateDescription(self);
}

// ============================================================
// Slot6 (0x140070650): Update — sync OK button enabled state
// ============================================================
// Enables buttonOk if editName has content, disables if empty.

void NewGameDialog_Slot6(NewGameDialog* self) {
    fn_InternalBasePtr_CheckPtr(&self->buttonOk);
    void* okBtn = self->buttonOk;

    fn_InternalBasePtr_CheckPtr(&self->editName);
    void* editElem = self->editName;

    // EditElement+0xC8 = text string pointer (System::String)
    int64_t strPtr = *(int64_t*)((uintptr_t)editElem + 200);  // 200 = 0xC8
    int len = 0;
    if (strPtr != 0) {
        // String length is at strPtr-8 (COW header)
        len = *(int*)(strPtr - 8);
    }

    // Virtual call: vtable[8] = SetEnabled(bool)
    void** vtable = *(void***)okBtn;
    typedef void (*fn_SetEnabled_t)(void*, int);
    ((fn_SetEnabled_t)vtable[8])(okBtn, len > 0 ? 1 : 0);
}
