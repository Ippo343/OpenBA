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
// Lazy type-ID resolution (standalone safety)
// ============================================================
// In standalone the exe .rdata type-ID slots (g_TypeId_MapGenerator etc.) are
// never populated by Runtime reflection, so they start NULL and every
// `*g_TypeId_X` deref in Slot4 is a read of address 0 (hard crash). Mirror the
// OptionsDialog/inputbindings StringTable pattern: lazy-resolve each via
// Object::Factory::GetTypeIdFromName into a static, then point the global at it.
// No-op in DLL mode (the slots are already non-null there).
static uint32_t* LazyTypeId(uint32_t* current, const char* typeName, uint32_t* storage) {
    if (current) return current;
    if (!fn_Factory_GetTypeIdFromName || !g_pFactorySingleton || !*g_pFactorySingleton)
        return nullptr;
    if (*storage == 0) {
        void* factoryObj = *(void**)*g_pFactorySingleton;
        if (factoryObj) {
            *storage = fn_Factory_GetTypeIdFromName(factoryObj, typeName, true);
            printf("[NewGameDialog] Lazy-resolved %s typeId = %u\n", typeName, *storage);
        }
    }
    return (*storage != 0) ? storage : nullptr;
}

static void EnsureGeneratorTypeIds() {
    static uint32_t s_map = 0, s_climate = 0, s_terrain = 0, s_nameDB = 0;
    if (!g_TypeId_MapGenerator)
        g_TypeId_MapGenerator = LazyTypeId(g_TypeId_MapGenerator, "MapGenerator", &s_map);
    if (!g_TypeId_ClimateGenerator)
        g_TypeId_ClimateGenerator = LazyTypeId(g_TypeId_ClimateGenerator, "ClimateGenerator", &s_climate);
    if (!g_TypeId_TerrainGenerator)
        g_TypeId_TerrainGenerator = LazyTypeId(g_TypeId_TerrainGenerator, "TerrainGenerator", &s_terrain);
    if (!g_TypeId_NameDatabase)
        g_TypeId_NameDatabase = LazyTypeId(g_TypeId_NameDatabase, "NameDatabase", &s_nameDB);
}

// Type-check a loaded resource. In STANDALONE the Runtime factory tree is
// incomplete, so Object::IsDerivedFrom walks up the type's parent chain into a
// NULL factory and crashes (NULL-this). Per feedback_factory_cascade_strategy we
// trust FindOrLoad's result and skip the derivation check. DLL mode keeps the
// real check (full factory tree present). typeIdPtr is passed (not deref'd in
// standalone) so a still-unresolved (NULL) type-ID can't fault at the call site.
static inline bool ResourceIsType(void* resource, uint32_t* typeIdPtr) {
    if (!resource) return false;
#ifdef STANDALONE_EXE
    (void)typeIdPtr;
    return true;
#else
    return typeIdPtr && fn_IsDerivedFrom(resource, *typeIdPtr);
#endif
}

#ifdef STANDALONE_EXE
// ============================================================
// EnsureGeneratorListsLoaded (standalone) — populate the combos for real
// ============================================================
// Slot4 reads the three world-generator ExternalLists from the Game/KeyMap
// container (g_pKeyMapSingleton): +0x10 climate, +0x18 map/start-condition,
// +0x20 terrain. Retail fills them in GameContainer_Init (exe 0x140099db0) via
// Object::ExternalList::Find on the master list (with the External base type-id
// 0xE771F47F). Standalone has no master list and never runs that init, so the
// slots are NULL and the combos come up empty. Mirror inputbindings.cpp::
// LoadInputNameTables: ExternalFactory::FindOrLoad each list .rsc directly and
// trust the result (the IsDerivedFrom type-check crashes against the incomplete
// standalone factory tree — see feedback_factory_cascade_strategy). Idempotent.
static void EnsureGeneratorListsLoaded() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return;
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton ||
        !fn_ExternalFactory_FindOrLoad || !fn_String_Ctor_CStr ||
        !fn_String_Dtor || !fn_Filename_AdjustDelimiters) return;

    char* container = reinterpret_cast<char*>(*g_pKeyMapSingleton);

    auto loadInto = [&](size_t off, const char* path) {
        if (*reinterpret_cast<void**>(container + off)) return;  // already loaded
        printf("[NewGameDialog] generator list LOADING '%s' (container+0x%zx)...\n",
               path, off);
        fflush(stdout);
        uint8_t fnameBuf[8];
        fn_String_Ctor_CStr(fnameBuf, path);
        fn_Filename_AdjustDelimiters(fnameBuf);
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnameBuf);
        fn_String_Dtor(fnameBuf);
        *reinterpret_cast<void**>(container + off) = ext;  // trust FindOrLoad
        printf("[NewGameDialog] generator list '%s' -> %p (container+0x%zx)\n",
               path, ext, off);
        fflush(stdout);
    };

    loadInto(0x10, "Game/Climate/ClimateList.rsc");
    loadInto(0x18, "Game/StartCondition/StartConditions.rsc");
    loadInto(0x20, "Game/Terrain/TerrainList.rsc");
}
#endif  // STANDALONE_EXE

// ============================================================
// Helper: populate combo from ExternalList with type check
// ============================================================
static void PopulateGeneratorCombo(void** comboFieldPtr, void* externalList,
                                    uint32_t typeId, int visibilityOffset) {
    // Standalone safety: the generator ExternalLists (KeyMap +0x10/+0x18/+0x20)
    // may be unpopulated if the Game resource container hasn't run yet — leave
    // the combo empty rather than dereferencing a null list. (No-op in DLL mode.)
    if (!externalList) return;
    int count = fn_ExternalList_GetResourceCount(externalList);
    for (int i = 0; i < count; i++) {
        // ExternalList+0x20 holds pointer to the resource pointer array
        void** ptrArray = *(void***)((uintptr_t)externalList + 0x20);
        void* resource = ptrArray[i];

        // IsDerivedFrom check (trust-helper: skips the crashing walk in standalone)
        void* typed = nullptr;
        if (ResourceIsType(resource, &typeId)) {
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
// ============================================================
// GenerateTownName — reimplementation of FUN_140099ac0
// ============================================================
// Loads "Game/Names/Cities.ndb", generates a random town name using the
// NameDatabase, then checks if it ends in "vill" and appends "e" to make "ville".
// Original: 0x140099ac0.
static void* GenerateTownName(void* retStr, void* random, int maxLen) {
#ifdef STANDALONE_EXE
    // Standalone: ExternalFactory::FindOrLoad("Game/Names/Cities.ndb") NULL-derefs
    // inside its OWN internal IsDerivedFrom (the NameDatabase factory subtree is
    // incomplete), and NameDatabase::GenerateName (exe 0x1400a4b30) isn't
    // reimplemented anyway. Skip the load entirely and use a placeholder so the
    // OK button stays enabled. DLL mode does the real name generation below.
    (void)random; (void)maxLen;
    fn_String_Ctor_CStr(retStr, "New Town");
    return retStr;
#else
    // Load NameDatabase resource
    uint8_t filenameBuf[8];
    fn_Filename_CtorCStr(filenameBuf, "Game/Names/Cities.ndb");
    void* resource = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
    void* nameDB = nullptr;
    if (ResourceIsType(resource, g_TypeId_NameDatabase)) {
        nameDB = resource;
    }
    fn_Filename_Dtor(filenameBuf);

    // Standalone fallback: NameDatabase::GenerateName (exe 0x1400a4b30) isn't
    // reimplemented and/or the NameDatabase type-ID/resource didn't resolve.
    // Produce a valid placeholder String so the caller's SetText/Dtor don't
    // operate on uninitialized memory (and the OK button stays enabled so the
    // Start path remains testable). No-op in DLL mode.
    if (!fn_NameDB_GenerateName || !nameDB) {
        fn_String_Ctor_CStr(retStr, "New Town");
        return retStr;
    }

    // Generate name using inner algorithm (retry loop, binary search uniqueness check)
    fn_NameDB_GenerateName(retStr, nameDB, random, 5, maxLen);

    // Check if name ends in "vill" — if so, append "e" to make "ville"
    uint64_t strPtr = *(uint64_t*)retStr;
    bool endsInVill = false;
    if (strPtr != 0) {
        int len = *(int*)(strPtr - 8);  // COW header: length at strPtr-8
        if (len > 4) {
            uint8_t suffixStr[8];
            fn_String_SubString(retStr, suffixStr, len - 4, len - 1);
            endsInVill = fn_String_Equals(suffixStr, "vill");
            fn_String_Dtor(suffixStr);
        }
    }
    if (endsInVill) {
        uint8_t tmpRet[8];
        fn_String_Append(retStr, tmpRet, "e");
        fn_String_Dtor(tmpRet);
    }

    return retStr;
#endif  // STANDALONE_EXE
}

static void GenerateRandomName(void* editNamePtr, void* random) {
    fn_InternalBasePtr_CheckPtr(editNamePtr);
    void* editElem = *(void**)editNamePtr;
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
    GenerateTownName(nameStr, random, nameLen);

    fn_InternalBasePtr_CheckPtr(editNamePtr);
    fn_EditElement_SetText_SysStr(*(void**)editNamePtr, nameStr);
    fn_String_Dtor(nameStr);
}

// ============================================================
// Helper: generate random seed
// ============================================================
static void GenerateRandomSeed(void* editSeedPtr, void* random) {
    fn_InternalBasePtr_CheckPtr(editSeedPtr);
    // NumberEditElement+0xFC contains max value
    int maxVal = *(int*)((uintptr_t)*(void**)editSeedPtr + 0xFC);

    fn_InternalBasePtr_CheckPtr(editSeedPtr);
    void* seedElem = *(void**)editSeedPtr;
    uint32_t seed = fn_Random_Rand(random, (uint32_t)(maxVal + 1));
    fn_NumberEditElement_SetValue(seedElem, seed);
}

// ============================================================
// Helper: update description text from selected map generator
// ============================================================
static void UpdateDescription(NewGameDialog* self) {
    // Standalone safety: skip if the map-generator list/type-ID is unresolved
    // (covers both Slot4 and the OnEvent combo-change call site).
    if (!self->mapGeneratorList || !g_TypeId_MapGenerator) return;
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr(&self->comboCondition);
    void* condCombo = self->comboCondition;

    // Get selected map generator resource
    void** ptrArray = *(void***)((uintptr_t)self->mapGeneratorList + 0x20);
    int selectedIdx = *(int*)((uintptr_t)condCombo + 0x118);
    void* resource = ptrArray[selectedIdx];

    void* mapGen = nullptr;
    if (ResourceIsType(resource, g_TypeId_MapGenerator)) {
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

    // Standalone: lazy-resolve the generator/NameDB type-IDs (the exe .rdata
    // slots are NULL until resolved via the Factory). No-op in DLL mode.
    EnsureGeneratorTypeIds();
#ifdef STANDALONE_EXE
    // Standalone: loading the three generator ExternalLists would populate the
    // combos, BUT ExternalFactory::FindOrLoad runs an internal IsDerivedFrom that
    // NULL-derefs against the incomplete standalone factory tree (the
    // ClimateGenerator/MapGenerator/TerrainGenerator resource types are now
    // REGISTER_TYPE'd with real factories + verified 9-slot vtables (each
    // GetVersion stub returns the exe's constant: Climate 3 / Map 8 / Terrain 4,
    // so Object::External::Load's fileVersion gate passes and the element
    // Serialize runs). The required dispatch slots (float-array 0x140151ea8,
    // Filename ctor/dtor 0x140151d40/0x1401529e8) and the shared ExternalPtr
    // helper (0x14000bf40) are wired in address_registry.cpp. With those in
    // place FindOrLoad's internal IsDerivedFrom resolves cleanly, so load the
    // three generator lists to populate the combos for real.
    static const bool s_loadGeneratorLists = true;
    if (s_loadGeneratorLists) EnsureGeneratorListsLoaded();
#endif

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
    void* keyMapData = (g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    if (keyMapData) {
        self->mapGeneratorList     = *(void**)((uintptr_t)keyMapData + 0x18);
        self->climateGeneratorList = *(void**)((uintptr_t)keyMapData + 0x10);
        self->terrainGeneratorList = *(void**)((uintptr_t)keyMapData + 0x20);
    }

    // --- Populate generator combos. Guarded for standalone: the type-ID pointers
    //     may still be NULL (deref would crash) and the ExternalLists may be
    //     unpopulated (PopulateGeneratorCombo null-checks the list internally). ---
    if (g_TypeId_MapGenerator)
        PopulateGeneratorCombo((void**)&self->comboCondition, self->mapGeneratorList,
                               *g_TypeId_MapGenerator, 0x38);
    if (g_TypeId_ClimateGenerator)
        PopulateGeneratorCombo((void**)&self->comboClimate, self->climateGeneratorList,
                               *g_TypeId_ClimateGenerator, 0x30);
    if (g_TypeId_TerrainGenerator)
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

    // --- Generate random name (guarded: editName must be present) ---
    if (self->editName)
        GenerateRandomName(&self->editName, (void*)(s + 0x118));

    // --- Generate random seed (guarded: editSeed must be present) ---
    if (self->editSeed)
        GenerateRandomSeed(&self->editSeed, (void*)(s + 0x118));

    // --- Restore saved combo selections from game settings ---
    // Standalone: g_pNewGameSettings (exe-data slot) may be NULL — skip the
    // whole restore block rather than dereferencing null. No-op in DLL mode.
    void* settings = (g_pNewGameSettings ? *g_pNewGameSettings : nullptr);
    uintptr_t cfg = (uintptr_t)settings;
    if (settings) {

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

    }  // end if (settings) — standalone null-settings guard

    // --- Update description text from selected map generator ---
    UpdateDescription(self);
}

// ============================================================
// Slot6 (0x140070650): Update — sync OK button enabled state
// ============================================================
// Enables buttonOk if editName has content, disables if empty.

// ============================================================
// NewGameDialog::OnEvent (0x140070110) — Slot8
// ============================================================
// Handles button clicks and combo changes in the New Game dialog.
// eventType 0x12 = button click:
//   eventData 0 = buttonNewName → generate random name
//   eventData 1 = buttonNewSeed → generate random seed
//   eventData 2 = buttonOk     → start new game
//   eventData 3 = buttonCancel → close dialog
// eventType 0x14 = combo change:
//   eventData 4 = comboCondition → update description text
//
// Original: 0x140070110.

static void NewGameDialog_StartGame(NewGameDialog* self) {
    // Read combo indices from elements
    // comboCondition at +0xD0, comboTerrain at +0xB8, comboClimate at +0xC8
    // comboTerrainSize at +0xC0, comboDisaster at +0xD8

    // Read combo selected items
    fn_InternalBasePtr_CheckPtr(&self->comboCondition);
    int condIdx = *(int*)((uintptr_t)self->comboCondition + 0x118);

    fn_InternalBasePtr_CheckPtr(&self->comboClimate);
    int climateIdx = *(int*)((uintptr_t)self->comboClimate + 0x118);

    fn_InternalBasePtr_CheckPtr(&self->comboTerrain);
    int terrainIdx = *(int*)((uintptr_t)self->comboTerrain + 0x118);

    // Get combo item data pointers from ExternalList arrays
    fn_InternalBasePtr_CheckPtr(&self->comboCondition);
    void* mapGenItem = fn_ComboElement_GetItemData(self->comboCondition, condIdx);
    void** mapPtrArray = *(void***)((uintptr_t)self->mapGeneratorList + 0x20);
    void* mapGenResource = mapPtrArray[(intptr_t)mapGenItem];
    void* mapGen = nullptr;
    if (ResourceIsType(mapGenResource, g_TypeId_MapGenerator)) mapGen = mapGenResource;

    fn_InternalBasePtr_CheckPtr(&self->comboClimate);
    void* climateItem = fn_ComboElement_GetItemData(self->comboClimate, climateIdx);
    void** climatePtrArray = *(void***)((uintptr_t)self->climateGeneratorList + 0x20);
    void* climateResource = climatePtrArray[(intptr_t)climateItem];
    void* climateGen = nullptr;
    if (ResourceIsType(climateResource, g_TypeId_ClimateGenerator)) climateGen = climateResource;

    fn_InternalBasePtr_CheckPtr(&self->comboTerrain);
    void* terrainItem = fn_ComboElement_GetItemData(self->comboTerrain, terrainIdx);
    void** terrainPtrArray = *(void***)((uintptr_t)self->terrainGeneratorList + 0x20);
    void* terrainResource = terrainPtrArray[(intptr_t)terrainItem];
    void* terrainGen = nullptr;
    if (ResourceIsType(terrainResource, g_TypeId_TerrainGenerator)) terrainGen = terrainResource;

    // Get terrain size index
    fn_InternalBasePtr_CheckPtr(&self->comboTerrainSize);
    uint32_t terrainSize = *(uint32_t*)((uintptr_t)self->comboTerrainSize + 0x118);

    // Get seed value
    fn_InternalBasePtr_CheckPtr(&self->editSeed);
    uint32_t seed = (uint32_t)fn_NumberEditElement_GetValue(self->editSeed);

    // Get disaster enabled (0 = disasters on, nonzero = off)
    fn_InternalBasePtr_CheckPtr(&self->comboDisaster);
    int32_t disasterIdx = *(int32_t*)((uintptr_t)self->comboDisaster + 0x118);
    int disasterOff = (disasterIdx == 0) ? 0 : 1;

    // Get map name from editName element — String at editName+0xC8
    fn_InternalBasePtr_CheckPtr(&self->editName);
    void* nameStr = (void*)((uintptr_t)self->editName + 0xC8);

    // Save combo selections to game settings
    void* settings = *g_pNewGameSettings;
    uintptr_t cfg = (uintptr_t)settings;
    *(uint32_t*)(cfg + 0x2c) = (uint32_t)condIdx;
    *(uint32_t*)(cfg + 0x20) = (uint32_t)terrainIdx;
    *(uint32_t*)(cfg + 0x24) = terrainSize;
    *(uint32_t*)(cfg + 0x28) = (uint32_t)climateIdx;
    *(uint32_t*)(cfg + 0x30) = (uint32_t)disasterIdx;

    // StartNewGame — reimplementation of FUN_1400b4540
    // Constructs GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop chain,
    // then sets next loop on the appropriate engine or dialog loader target.
    {
        uintptr_t ws = (uintptr_t)self->worldSetup;

        // Step 1: Allocate and construct GameUpdateLoop (0x160 bytes)
        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        void* pGameLoop = nullptr;
        if (gameLoop) {
            // GameUpdateLoop_Ctor reimplemented inline:
            // Calls UpdateLoop base ctor, then sets vtable and fields.
            fn_UpdateLoop_Ctor(gameLoop);
            *(void**)gameLoop = (void*)g_GameUpdateLoop_Vtable;
            // Copy name string to +0x28
            fn_String_Ctor_CStr((void*)((uintptr_t)gameLoop + 0x28), "");
            fn_String_Assign((void*)((uintptr_t)gameLoop + 0x28), nameStr);
            *(uint32_t*)((uintptr_t)gameLoop + 0x30) = (uint32_t)(uintptr_t)seed;
            *(uint32_t*)((uintptr_t)gameLoop + 0x34) = terrainSize;
            *(uint64_t*)((uintptr_t)gameLoop + 0x38) = 0;
            *(uint64_t*)((uintptr_t)gameLoop + 0x40) = (uint64_t)mapGen;
            *(uint64_t*)((uintptr_t)gameLoop + 0x48) = (uint64_t)terrainGen;
            *(uint64_t*)((uintptr_t)gameLoop + 0x50) = (uint64_t)climateGen;
            *(uint64_t*)((uintptr_t)gameLoop + 0x58) = 0;  // savedState = null
            // Zero remaining fields
            for (uintptr_t off = 0x60; off <= 0xC0; off += 8) {
                *(uint64_t*)((uintptr_t)gameLoop + off) = 0;
            }
            *(uint16_t*)((uintptr_t)gameLoop + 0xC0) = 0;
            *(uint64_t*)((uintptr_t)gameLoop + 0xC8) = 0;
            // disaster flag: combo idx 0 = disasters ON -> +0xD0 = 0
            //                combo idx != 0 = disasters OFF -> +0xD0 = 0xF0
            *(uint32_t*)((uintptr_t)gameLoop + 0xD0) = (disasterIdx == 0) ? 0 : 0xF0;
            for (uintptr_t off = 0xD8; off <= 0x148; off += 8) {
                *(uint64_t*)((uintptr_t)gameLoop + off) = 0;
            }
            // Store retval ptr at +0x150
            uint8_t nullIBP[8];
            fn_InternalBasePtr_Ctor(nullIBP, nullptr);
            *(uint64_t*)((uintptr_t)gameLoop + 0x150) = *(uint64_t*)nullIBP;
            *(uint8_t*)((uintptr_t)gameLoop + 0x158) = 0;
            *(uint32_t*)((uintptr_t)gameLoop + 0x15C) = (uint32_t)-1;  // param_9 = -1
            pGameLoop = gameLoop;
        }

        // Step 2: Allocate and construct GameLoadDialogLoopT (0xB60 bytes)
        void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xB60, 4);
        void* pDlLoop = nullptr;
        if (dlLoop) {
            uint8_t delayTime[8];
            fn_Time_Ctor_Float(delayTime, 1.0f);
            uint8_t loadingStr[8];
            GetLoadingSoundString(loadingStr);
            fn_DialogLoaderLoopT_Ctor(dlLoop, loadingStr, pGameLoop, delayTime);
            *(void**)dlLoop = (void*)g_GameLoadDialogLoopT_Vtable;
            *(uint32_t*)((uintptr_t)dlLoop + 0x188) = 0xFF000000;
            *(uint32_t*)((uintptr_t)dlLoop + 0x18C) = 0xFF000000;
            *(uint8_t*)((uintptr_t)dlLoop + 400) = 0;
            fn_Random_Default_Ctor((void*)((uintptr_t)dlLoop + 0x194));
            *(uint64_t*)((uintptr_t)dlLoop + 0xB58) = 0;
            *(uint8_t*)((uintptr_t)dlLoop + 0x40) = 0;
            fn_String_Dtor(loadingStr);
            pDlLoop = dlLoop;
        }

        // Step 3: Allocate and construct outer LoaderLoop (0x48 bytes)
        void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
        void* pOuterLoop = nullptr;
        if (outerLoop) {
            uint8_t zeroTime[8];
            fn_Time_Ctor_Float(zeroTime, 0.0f);
            fn_LoaderLoop_Ctor(outerLoop, pDlLoop, zeroTime);
            *(void**)outerLoop = (void*)g_GameLoaderLoop_Vtable;
            pOuterLoop = outerLoop;
        }

        // Step 4: Set next loop
        // In StartNewGame, the check is: +0xC0 (loaderLoop1) == 0 -> use engine at +0xB8
        uintptr_t loaderLoop1 = *(uintptr_t*)(ws + 0xC0);
        if (loaderLoop1 == 0) {
            uintptr_t engPtr = *(uintptr_t*)(ws + 0xB8);
            SetEngineNextLoop(engPtr, pOuterLoop);
        } else {
            SetDialogLoaderNextLoop(loaderLoop1, pOuterLoop);
        }
    }
}

void NewGameDialog_OnEvent(NewGameDialog* self, void* element, int eventType, int eventData) {
    uintptr_t s = (uintptr_t)self;

    if (eventType == 0x12) {
        // Button click
        if (eventData == 2) {
            // buttonOk: start game
            NewGameDialog_StartGame(self);
            return;
        }
        if (eventData == 3) {
            // buttonCancel: close dialog
            void* ui = *(void**)(s + 0x10);
            uint64_t basePtr[2];
            fn_InternalBasePtr_Ctor(basePtr, self);
            fn_UI_CloseDialog(ui, basePtr);
            return;
        }
        if (eventData == 0) {
            // buttonNewName: generate random name
            GenerateRandomName(&self->editName, (void*)(s + 0x118));
            return;
        }
        if (eventData == 1) {
            // buttonNewSeed: generate random seed
            GenerateRandomSeed(&self->editSeed, (void*)(s + 0x118));
            return;
        }
    } else if (eventType == 0x14 && eventData == 4) {
        // comboCondition selection changed: update description text
        UpdateDescription(self);
    }
}

// ============================================================
// Slot5 (vtable +0x28, exe 0x1400706c0): Shutdown — RET 0 (3 bytes)
// ============================================================
// No-op shutdown; nothing to clean up.
int NewGameDialog_Shutdown(NewGameDialog* self) {
    (void)self;
    return 0;
}

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

#ifdef STANDALONE_EXE

// ============================================================
// NewGameDialog standalone scaffolding
// ============================================================
// Provides the 10-slot DialogInstance vtable + a standalone in-place ctor +
// factory + REGISTER_TYPE so MainMenuDialog::OpenNewGame can construct a real
// NewGameDialog in standalone mode (no exe loader / no trampoline). Mirrors the
// OptionsDialog template (code/Game/Dialog/optionsdialog.cpp).
//
// Vtable layout (exe @ 0x14015c920, 10 slots — verified via read_memory):
//   +0x00 Slot0 Dtor             — 0x140012cc0 (scalar-deleting dtor thunk)
//   +0x08 Slot1 TypeName         — 0x140012c60 (const-load)
//   +0x10 Slot2 TypeId           — 0x140012c50 (const-load)
//   +0x18 Slot3 Persist          — 0x14003a948 (runtime thunk)
//   +0x20 Slot4 Startup          — 0x1400706d0  → NewGameDialog_Slot4 (REIMPL'd)
//   +0x28 Slot5 Shutdown         — 0x1400706c0  → NewGameDialog_Shutdown (REIMPL'd)
//   +0x30 Slot6 OnUpdateLayout   — 0x140070650  → NewGameDialog_Slot6 (REIMPL'd, OK-btn sync)
//   +0x38 Slot7 OnUpdate         — 0x14003a990 (runtime thunk → DialogInstance::OnUpdate)
//   +0x40 Slot8 OnEvent          — 0x140070110  → NewGameDialog_OnEvent (REIMPL'd)
//   +0x48 Slot9 OnSerializeSettings — 0x14003a954 (runtime thunk)
//
// The real ctor is exe 0x140071040 (Ghidra mislabels it "NewGameDialog_CopyCtor";
// it is the 5-arg ctor that MainMenuDialog::OpenNewGame calls). 0x140071120 is the
// Dtor. fn_NewGameDialog_Ctor (engine_funcs.cpp) resolves 0x140071040 — we register
// our standalone ctor at that address below.

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"

namespace {

void* GetNewGameDialogVtable();  // forward decl

// Slot 0 — Dtor. Mirrors generic ~DialogInstance scalar-deleting dtor.
void* NewGameStub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetNewGameDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot 1 — TypeName.
const char* NewGameStub_Slot1_TypeName(void* /*self*/) { return "NewGameDialog"; }

// Slot 2 — TypeId.
uint32_t NewGameStub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("NewGameDialog");
    return cached;
}

// Generic logging stub for the runtime-thunk slots (3, 9). Logs once per slot.
template <int SlotIdx>
void NewGameStub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[NewGameDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
               SlotIdx, self);
    }
}

// Slot 7 (OnUpdate, +0x38) — original is a base thunk into
// UI::DialogInstance::OnUpdate which recurses children each frame (this is where
// child element Update fires — combos, edits, buttons + UI hover/click sounds).
// Delegate straight to the Runtime function so the dialog's controls tick.
void NewGameImpl_Slot7_OnUpdate(void* self, const void* time) {
    if (!self) return;
    if (fn_DialogInstance_OnUpdate) {
        fn_DialogInstance_OnUpdate(self, time);
        return;
    }
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[NewGameDialog::Slot7] fn_DialogInstance_OnUpdate NULL (mangled name?)\n");
    }
}

void* g_NewGameDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&NewGameStub_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&NewGameStub_Slot1_TypeName),     // +0x08 TypeName
    reinterpret_cast<void*>(&NewGameStub_Slot2_TypeId),       // +0x10 TypeId
    reinterpret_cast<void*>(&NewGameStub_RuntimeThunk<3>),    // +0x18 Persist
    reinterpret_cast<void*>(&NewGameDialog_Slot4),            // +0x20 Startup
    reinterpret_cast<void*>(&NewGameDialog_Shutdown),         // +0x28 Shutdown
    reinterpret_cast<void*>(&NewGameDialog_Slot6),            // +0x30 OnUpdateLayout (OK-btn sync)
    reinterpret_cast<void*>(&NewGameImpl_Slot7_OnUpdate),     // +0x38 OnUpdate
    reinterpret_cast<void*>(&NewGameDialog_OnEvent),          // +0x40 OnEvent
    reinterpret_cast<void*>(&NewGameStub_RuntimeThunk<9>),    // +0x48 OnSerializeSettings
};

void* GetNewGameDialogVtable() {
    return reinterpret_cast<void*>(g_NewGameDialog_Vtable_Standalone);
}

// In-place ctor — clean reimpl of exe 0x140071040 ("NewGameDialog_CopyCtor").
// Signature matches fn_NewGameDialog_Ctor_t (self, ui, extRes, nullParent,
// parentMenuIBP). Mirrors the decompile:
//   DialogInstance::DialogInstance(self, ui, extRes, &nullParent, 0,0,0)
//   *self = vtable_NewGameDialog
//   zero +0xB8..+0x110 (the 12 element-pointer fields)
//   System::Random::Random(self+0x118)
//   worldSetup(+0xAE0) = *parentMenuIBP   (the MainMenuDialog, used by StartGame)
void* NewGameDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                    void* nullParent, void* parentMenuIBP) {
    if (!self) return nullptr;
    printf("[NewGameDialog_Ctor] self=%p ui=%p extRes=%p nullParent=%p parentMenu=%p\n",
           self, ui, extRes, nullParent, parentMenuIBP);

    uint64_t parentCopy = nullParent ? *reinterpret_cast<uint64_t*>(nullParent) : 0;

    if (fn_DialogInstance_Ctor && extRes) {
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy, nullptr, nullptr, 0);
    } else {
        printf("[NewGameDialog_Ctor] skipping DialogInstance::DialogInstance "
               "(fn=%p extRes=%p) — bare-bones init\n",
               (void*)fn_DialogInstance_Ctor, extRes);
    }

    *reinterpret_cast<void**>(self) = GetNewGameDialogVtable();

    // Zero +0xB8..+0x110 inclusive (comboTerrain .. terrainGeneratorList).
    char* base = reinterpret_cast<char*>(self);
    for (size_t off = 0xB8; off <= 0x110; off += 8) {
        *reinterpret_cast<uint64_t*>(base + off) = 0;
    }

    // System::Random sub-object at +0x118.
    if (fn_Random_Default_Ctor) {
        fn_Random_Default_Ctor(base + 0x118);
    }

    // worldSetup (+0xAE0) = *parentMenuIBP (the MainMenuDialog ptr).
    *reinterpret_cast<uint64_t*>(base + 0xAE0) =
        parentMenuIBP ? *reinterpret_cast<uint64_t*>(parentMenuIBP) : 0;

    return self;
}

void* NewGameDialog_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(NewGameDialog), 8);
    if (!p) return nullptr;
    for (size_t off = 0; off < sizeof(NewGameDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(p) + off) = 0;
    }
    *reinterpret_cast<void**>(p) = GetNewGameDialogVtable();
    return p;
}

void* NewGameDialog_CreateInPlace_Bare(void* mem) {
    if (!mem) return nullptr;
    for (size_t off = 0; off < sizeof(NewGameDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(mem) + off) = 0;
    }
    *reinterpret_cast<void**>(mem) = GetNewGameDialogVtable();
    return mem;
}

}  // namespace

extern "C" void RegisterNewGameDialogVtable() {
    RegisterExeAddr(0x14015c920, reinterpret_cast<uintptr_t>(g_NewGameDialog_Vtable_Standalone));
    RegisterExeAddr(0x140071040, reinterpret_cast<uintptr_t>(&NewGameDialog_Ctor_Standalone));
}

extern "C" void* NewGameDialog_StandaloneFactory(void) {
    return NewGameDialog_Factory_Bare(nullptr);
}
extern "C" void* NewGameDialog_StandaloneCreateInPlace(void* memory) {
    return NewGameDialog_CreateInPlace_Bare(memory);
}

REGISTER_TYPE(NewGameDialog, "UI::DialogInstance",
              &NewGameDialog_StandaloneFactory, &NewGameDialog_StandaloneCreateInPlace);

#endif  // STANDALONE_EXE
