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
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
// trust FindOrLoad's result and skip the derivation check. typeIdPtr is passed
// (not deref'd) so a still-unresolved (NULL) type-ID can't fault at the call site.
static inline bool ResourceIsType(void* resource, uint32_t* typeIdPtr) {
    if (!resource) return false;
    (void)typeIdPtr;
    return true;
}

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

// ============================================================
// Helper: populate combo from ExternalList with type check
// ============================================================
static void PopulateGeneratorCombo(void** comboFieldPtr, void* externalList,
                                    uint32_t typeId, int visibilityOffset) {
    // Standalone safety: the generator ExternalLists (KeyMap +0x10/+0x18/+0x20)
    // may be unpopulated if the Game resource container hasn't run yet — leave
    // the combo empty rather than dereferencing a null list.
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
    // Load the city-name database. The IsDerivedFrom type-check retail runs on
    // the result is skipped: the standalone factory tree is incomplete and the
    // walk NULL-faults, while FindOrLoad by exact filename is authoritative
    // (same call shape as citizen_names.cpp's Male.ndb/Female.ndb load).
    void* nameDB = nullptr;
    if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton &&
        fn_Filename_CtorCStr && fn_ExternalFactory_FindOrLoad && fn_Filename_Dtor) {
        uint8_t fnBuf[8] = {0};
        fn_Filename_CtorCStr(fnBuf, "Game/Names/Cities.ndb");
        nameDB = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, fnBuf);
        fn_Filename_Dtor(fnBuf);
    }

    // minLen 5 is retail's constant; maxLen is derived from the edit box's
    // _maxLength by the caller. NameDB_GenerateName (0x1400a4b30, registered by
    // standalone/reg_namedb.cpp) constructs the String in retStr even when the
    // database is missing, so retStr is always live for the suffix fix-up below.
    fn_NameDB_GenerateName(retStr, nameDB, random, 5, maxLen);

    // Retail: a name longer than 4 chars ending in "vill" gets an "e" appended,
    // so the generator's "-vill" stems read as "-ville".
    uint64_t sp = *(uint64_t*)retStr;
    bool endsVill = false;
    if (sp != 0 && fn_String_SubString && fn_String_Equals && fn_String_Append) {
        int slen = *(int*)(sp - 8);          // System::String length lives at ptr-8
        if (slen > 4) {
            uint8_t sfx[8] = {0};
            fn_String_SubString(retStr, sfx, slen - 4, slen - 1);
            endsVill = fn_String_Equals(sfx, "vill");
            fn_String_Dtor(sfx);
        }
    }
    if (endsVill) {
        uint8_t tmpRet[8] = {0};
        fn_String_Append(retStr, tmpRet, "e");
        fn_String_Dtor(tmpRet);
    }
    return retStr;
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

    // Mirrors the seed logging below: both halves of the "randomise" row report
    // what they produced, so a dead generator is visible in the log instead of
    // only on screen.
    {
        uint64_t sp = *(uint64_t*)nameStr;
        fprintf(stderr, "[NewGameDialog] town name generated: \"%s\" (maxLen=%d)\n",
                sp ? (const char*)sp : "(empty)", nameLen);
        fflush(stderr);
    }

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

    // BANISHED_SEED pins the world seed instead of randomising it. Needed for
    // A/B testing: a RenderDoc capture is only comparable against another
    // capture if both runs generated the same world, and autostart otherwise
    // rolls a fresh seed every launch. Purely additive — unset behaves exactly
    // like the original.
    //
    // ⚠ SCOPE — this value reaches FURTHER than "world generation", but runs still are
    // NOT reproducible, and the reason is not the seeding.
    //   BANISHED_SEED -> this seed element -> gameLoop+0x30 (newgamedialog.cpp:718)
    //                 -> Random(seed) for the gameplay RNG at gameWorld+0xd8
    //                    (gameupdateloop_load.cpp:777-782, whose "seeded with
    //                     mapSizeIndex" comment is WRONG — +0x30 is the seed, +0x34 is
    //                     terrainSize).
    // So the in-sim RNG that drives citizen gender, ages and model variants IS pinned.
    // Yet identical configs measurably diverge: 2026-07-31 gave gender splits 14/13,
    // 15/12 and 13/14 and extinction frames spanning ~7500-10500. Same seed, same
    // starting state, different outcomes => the divergence is in the ORDER or COUNT of
    // RNG consumption (or a second entropy source), NOT in the seed.
    // **Do not A/B two fixes on one run of each and rank them by outcome** — and do not
    // "fix" this by pinning gw+0xd8; it is already pinned.
    //
    // Both branches log, to stderr, ALWAYS. This used to be a printf on the pinned
    // branch only, i.e. into the stream that stalls (standalone_debug.log freezes at
    // ~22KB), so no log could confirm the seed had been applied and a silent random
    // seed was indistinguishable from a pinned one. Every past "this run used seed N"
    // was an assumption.
    const char* envSeed = std::getenv("BANISHED_SEED");
    if (envSeed && envSeed[0]) {
        uint32_t fixed = (uint32_t)strtoul(envSeed, nullptr, 0);
        if (maxVal > 0) fixed %= (uint32_t)(maxVal + 1);
        fn_NumberEditElement_SetValue(seedElem, fixed);
        fprintf(stderr, "[NewGameDialog] BANISHED_SEED=%s -> seed PINNED to %u "
                "(also seeds the gameplay RNG at gw+0xd8; runs still diverge — see the "
                "scope note above)\n", envSeed, fixed);
        fflush(stderr);
        return;
    }

    uint32_t seed = fn_Random_Rand(random, (uint32_t)(maxVal + 1));
    fn_NumberEditElement_SetValue(seedElem, seed);
    fprintf(stderr, "[NewGameDialog] BANISHED_SEED unset -> world seed RANDOMISED to %u\n",
            seed);
    fflush(stderr);
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
// SelectStartConditionOverride (standalone-only, additive)
// ============================================================
// BANISHED_START_CONDITION picks the start-condition combo entry: digits = list
// index, anything else = case-insensitive substring of the entry's name key
// (System::String at resource+0x28, e.g. "Easy"/"Medium"/"Hard"), "none" = off.
// When unset, an autostart run defaults to "easy" so the livestock start path
// (free-roaming cows/sheep/chickens from the simple pool) is exercised; manual
// runs are untouched. Runs after the saved-selection restore so it wins, and
// before UpdateDescription so the UI shows the forced choice. Logs every entry
// (index, name, visibility) so the index<->name mapping is verifiable in the log.
// NOTE: retail's StartGame treats combo+0x118 as a LIST index (all retail start
// conditions are visible, so combo index == list index); we match that.
static void SelectStartConditionOverride(NewGameDialog* self) {
    const char* want = std::getenv("BANISHED_START_CONDITION");
    if (!want || !want[0]) {
        const char* as = std::getenv("BANISHED_AUTOSTART");
        if (!as || !as[0] || std::strcmp(as, "0") == 0 ||
            std::strcmp(as, "false") == 0)
            return;                       // manual run, no override requested
        want = "easy";                    // autostart default
    }
    if (std::strcmp(want, "none") == 0) return;
    if (!self->mapGeneratorList || !self->comboCondition) return;

    bool numeric = true;
    for (const char* p = want; *p; ++p)
        if (*p < '0' || *p > '9') { numeric = false; break; }

    auto containsNoCase = [](const char* hay, const char* needle) -> bool {
        if (!hay || !needle || !*needle) return false;
        size_t nlen = std::strlen(needle);
        for (const char* h = hay; *h; ++h) {
            size_t i = 0;
            while (i < nlen && h[i] &&
                   std::tolower((unsigned char)h[i]) ==
                   std::tolower((unsigned char)needle[i]))
                ++i;
            if (i == nlen) return true;
        }
        return false;
    };

    void* list = self->mapGeneratorList;
    int count = fn_ExternalList_GetResourceCount(list);
    void** ptrArray = *(void***)((uintptr_t)list + 0x20);
    int target = -1;
    for (int i = 0; i < count; ++i) {
        void* res = ptrArray[i];
        const char* name = res ? *(char**)((uintptr_t)res + 0x28) : nullptr;
        uint8_t vis = res ? *(uint8_t*)((uintptr_t)res + 0x38) : 0;
        fprintf(stderr, "[NewGameDialog] startCondition[%d] name=\"%.32s\" visible=%d\n",
                i, name ? name : "(null)", (int)vis);
        if (target < 0 && !numeric && containsNoCase(name, want))
            target = i;
    }
    if (numeric) {
        int idx = std::atoi(want);
        if (idx >= 0 && idx < count) target = idx;
    }
    if (target >= 0) {
        fn_InternalBasePtr_CheckPtr(&self->comboCondition);
        fn_ComboElement_SetCurrentItem(self->comboCondition, target);
        fprintf(stderr, "[NewGameDialog] start condition FORCED to [%d] (want=\"%s\")\n",
                target, want);
    } else {
        fprintf(stderr, "[NewGameDialog] start condition override \"%s\" matched "
                "nothing (%d entries) — selection left alone\n", want, count);
    }
    fflush(stderr);
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
    // slots are NULL until resolved via the Factory).
    EnsureGeneratorTypeIds();
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
    // whole restore block rather than dereferencing null.
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

    // --- Standalone: env/autostart start-condition override (wins over the
    //     saved-selection restore above; see SelectStartConditionOverride) ---
    SelectStartConditionOverride(self);

    // --- Combo inventory (standalone verification) ---
    // Retail contents, for comparison: condition 3 (Hard/Medium/Easy),
    // terrain 2 (Valleys/Mountains), terrainSize 3 (Small/Medium/Large),
    // climate 3 (Mild/Fair/Harsh), disaster 2 (On/Off). The first three come
    // from the generator ExternalLists, the last two from the .rsc combo's
    // _initialData StringTable, so a 0 count tells you which half is missing.
    {
        struct { const char* name; void* combo; } combos[] = {
            { "comboCondition",   self->comboCondition   },
            { "comboTerrain",     self->comboTerrain     },
            { "comboTerrainSize", self->comboTerrainSize },
            { "comboClimate",     self->comboClimate     },
            { "comboDisaster",    self->comboDisaster    },
        };
        for (auto& c : combos) {
            if (!c.combo) { fprintf(stderr, "[NewGameDialog] %s MISSING\n", c.name); continue; }
            fprintf(stderr, "[NewGameDialog] %s items=%d selected=%d\n", c.name,
                    fn_ComboElement_GetItemCount(c.combo),
                    *(int*)((uintptr_t)c.combo + 0x118));
        }
        fflush(stderr);
    }

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

// Forward decl: the FUN_1400b4540 chain builder, defined just below.
void* StartNewGame_Impl(void* worldSetup, void* nameStr, uint32_t seed, uint32_t terrainSize,
                        bool disastersOff, void* mapGen, void* terrainGen, void* climateGen,
                        uint32_t scenarioResIdx, void* savedStateIBP);

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

    // --- Difficulty / StartCondition override (standalone autostart + testing) ---
    // The autostart harness auto-clicks "OK" without choosing a difficulty, and the
    // saved-settings combo restore is skipped when g_pNewGameSettings is null, so we
    // would otherwise get whatever the combo defaults to (Hard/Medium). Resolve the
    // MapGenerator directly from the StartCondition list by index instead.
    //   StartConditions.rsc list order: 0 = Hard, 1 = Medium, 2 = Easy.
    //   BANISHED_DIFFICULTY = easy|medium|hard (or 2|1|0) forces a level.
    //   Under autostart with no override, default to Easy so the full starting town
    //   (StorageBarn + StorageYard + 6 WoodHouses, 6 families) spawns.
    {
        int wantIdx = -1;
        const char* envDiff = getenv("BANISHED_DIFFICULTY");
        if (envDiff) {
            if (!strcmp(envDiff, "2") || !strcmp(envDiff, "easy") || !strcmp(envDiff, "Easy"))
                wantIdx = 2;
            else if (!strcmp(envDiff, "1") || !strcmp(envDiff, "medium") || !strcmp(envDiff, "Medium"))
                wantIdx = 1;
            else if (!strcmp(envDiff, "0") || !strcmp(envDiff, "hard") || !strcmp(envDiff, "Hard"))
                wantIdx = 0;
        } else {
            const char* envAuto = getenv("BANISHED_AUTOSTART");
            if (envAuto && strcmp(envAuto, "0") != 0 && strcmp(envAuto, "false") != 0)
                wantIdx = 2;  // Easy default for the autostart demo town
        }
        if (wantIdx >= 0 && self->mapGeneratorList && mapPtrArray) {
            int listCount = fn_ExternalList_GetResourceCount(self->mapGeneratorList);
            if (wantIdx < listCount) {
                void* r = mapPtrArray[wantIdx];
                if (ResourceIsType(r, g_TypeId_MapGenerator)) {
                    mapGen = r;
                    fprintf(stderr, "[NewGameDialog] difficulty override -> listIdx=%d (0=Hard 1=Medium 2=Easy) mapGen=%p\n",
                            wantIdx, mapGen);
                    fflush(stderr);
                }
            }
        }
    }

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

    // --- What the player actually chose (standalone verification) ---
    // Every row of the dialog lands in the GameUpdateLoop below, so log the
    // resolved pick for each one. The three generator rows are named from the
    // resource's StringTable key at +0x28; terrainSize and disaster have no
    // resource behind them, so they log as raw combo indices. worldFlags is the
    // word copied to GameWorld+0xD0, whose bits 4-7 are noSick / noCropDisease /
    // noFire / noTornado — 0x00 means all four disasters are live.
    {
        auto nameOf = [](void* res) -> const char* {
            if (!res) return "(null)";
            const char* n = *(char**)((uintptr_t)res + 0x28);
            return n ? n : "(unnamed)";
        };
        fprintf(stderr, "[NewGameDialog::StartGame] picks: condition[%d]=\"%s\" "
                "terrain[%d]=\"%s\" size=%u climate[%d]=\"%s\" disaster[%d]=%s "
                "seed=%u -> worldFlags=0x%02X\n",
                condIdx, nameOf(mapGen), terrainIdx, nameOf(terrainGen), terrainSize,
                climateIdx, nameOf(climateGen), disasterIdx,
                disasterOff ? "OFF" : "ON", seed,
                (disasterIdx == 0) ? 0 : 0xF0);
        fflush(stderr);
    }

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
    // Retail's argument order at 0x1400b4540 is (mapGen, terrainGen, climateGen):
    // args 6/7/8 land in GameUpdateLoop +0x40/+0x48/+0x50 in that order.
    StartNewGame_Impl(self->worldSetup, nameStr, (uint32_t)(uintptr_t)seed, terrainSize,
                      /*disastersOff=*/disasterIdx != 0, mapGen, terrainGen, climateGen,
                      /*scenarioResIdx=*/(uint32_t)-1, /*savedStateIBP=*/nullptr);
}

// ============================================================
// StartNewGame (exe 0x1400b4540)
// ============================================================
// Builds the GameUpdateLoop -> GameLoadDialogLoopT -> LoaderLoop chain and hands it
// to the engine (or to the owning dialog loader) as the next loop. This body was
// NewGameDialog_StartGame's inline "reimplementation of FUN_1400b4540"; it is lifted
// out here with retail's own 10-argument ABI so the address can be registered.
//
// Two callers, and only one of them worked:
//   NewGameDialog "Start"        -> called the inline copy directly (this is the
//                                   path BANISHED_AUTOSTART drives, so it is proven)
//   ScenarioDialog::LoadSelected -> called fn_StartNewGame (scenariodialog.cpp:404)
//                                   UNGUARDED. 0x1400b4540 was never registered, so
//                                   picking a scenario and clicking Load was an
//                                   EXEC-of-0. ScenarioDialog became reachable when
//                                   its ctor+vtable were wired for the Tutorial
//                                   button, which is why the sweep's "latent, not
//                                   active today" note had gone stale.
//
// The two extra parameters are exactly what NewGameDialog hardcoded: a new sandbox
// game passes scenarioResIdx = -1 and a null saved-state IBP, while a scenario passes
// its resource index and (for a saved scenario) the restored state.
void* StartNewGame_Impl(void* worldSetup, void* nameStr, uint32_t seed, uint32_t terrainSize,
                        bool disastersOff, void* mapGen, void* terrainGen, void* climateGen,
                        uint32_t scenarioResIdx, void* savedStateIBP) {
    {
        uintptr_t ws = (uintptr_t)worldSetup;
        fprintf(stderr, "[StartNewGame] ENTER worldSetup=%p seed=%u size=%u disastersOff=%d "
                "scenarioIdx=%d saved=%p GameUpdateLoop_Vtable=%p\n",
                (void*)ws, seed, terrainSize, (int)disastersOff, (int)scenarioResIdx,
                savedStateIBP, (void*)g_GameUpdateLoop_Vtable);
        fflush(stderr);

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
            *(uint32_t*)((uintptr_t)gameLoop + 0x30) = seed;
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
            // disaster flag: +0xD0 = 0 means disasters ON, 0xF0 means OFF.
            *(uint32_t*)((uintptr_t)gameLoop + 0xD0) = disastersOff ? 0xF0 : 0;
            for (uintptr_t off = 0xD8; off <= 0x148; off += 8) {
                *(uint64_t*)((uintptr_t)gameLoop + off) = 0;
            }
            // Store retval ptr at +0x150
            uint8_t nullIBP[8];
            fn_InternalBasePtr_Ctor(nullIBP, nullptr);
            // +0x150 = saved-state IBP (retail *param_10; null for a new sandbox game).
            *(uint64_t*)((uintptr_t)gameLoop + 0x150) =
                savedStateIBP ? *(uint64_t*)savedStateIBP : *(uint64_t*)nullIBP;
            *(uint8_t*)((uintptr_t)gameLoop + 0x158) = 0;
            // +0x15C = scenario resource index (retail param_9; -1 = plain new game).
            *(uint32_t*)((uintptr_t)gameLoop + 0x15C) = scenarioResIdx;
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
        uintptr_t engPtr0 = *(uintptr_t*)(ws + 0xB8);
        fprintf(stderr, "[StartNewGame] loops built game=%p dl=%p outer=%p; "
                "ws+0xC0(loop)=%p ws+0xB8(engine)=%p -> %s\n",
                pGameLoop, pDlLoop, pOuterLoop, (void*)loaderLoop1, (void*)engPtr0,
                loaderLoop1 ? "SetDialogLoaderNextLoop(loop)" : "SetEngineNextLoop(engine)");
        fflush(stderr);
        if (loaderLoop1 == 0) {
            SetEngineNextLoop(engPtr0, pOuterLoop);
        } else {
            SetDialogLoaderNextLoop(loaderLoop1, pOuterLoop);
        }
        fprintf(stderr, "[StartNewGame] EXIT — next loop set, transition pending\n");
        fflush(stderr);
        return pOuterLoop;
    }
}

// Retail-ABI entry for 0x1400b4540, registered so ScenarioDialog's by-address call
// lands in the same body the (autostart-proven) NewGameDialog path uses. Retail's
// param_3 / param_4 arrive as pointer-width slots carrying ints.
extern "C" void StartNewGame_Reg(void* worldSetup, void* nameStr, void* seed,
                                 void* terrainSize, char disastersOff, void* mapGen,
                                 void* terrainGen, void* climateGen, unsigned scenarioResIdx,
                                 void* savedStateIBP) {
    StartNewGame_Impl(worldSetup, nameStr, (uint32_t)(uintptr_t)seed,
                      (uint32_t)(uintptr_t)terrainSize, disastersOff != 0,
                      mapGen, terrainGen, climateGen, scenarioResIdx, savedStateIBP);
}

void NewGameDialog_OnEvent(NewGameDialog* self, void* element, int eventType, int eventData) {
    uintptr_t s = (uintptr_t)self;
    fprintf(stderr, "[NewGameDialog::OnEvent] self=%p type=0x%x data=%d\n",
            (void*)self, eventType, eventData);
    fflush(stderr);

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


// ============================================================
// NewGameDialog standalone scaffolding
// ============================================================
// Provides the 10-slot DialogInstance vtable + a standalone in-place ctor +
// factory + REGISTER_TYPE so MainMenuDialog::OpenNewGame can construct a real
// NewGameDialog (no exe loader). Mirrors the
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

// Defined in standalone/autostart.cpp — the auto-new-game driver reads this to
// find the dialog opened by MainMenuDialog::OpenNewGame.
extern "C" void* g_StandaloneNewGameDialog;

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

    // Publish the just-constructed dialog so the auto-start driver
    // (standalone/autostart.cpp) can locate it and fire Start once Slot4 has
    // populated the combos + OK button. Harmless when autostart is disabled.
    g_StandaloneNewGameDialog = self;

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

