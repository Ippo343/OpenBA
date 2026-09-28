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

#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ComboElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/EditElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/NumberEditElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/NewGameDialog.h"
#include "Dialog/ui_dialogs.h"
#include "options.h"
#include "Dialog/dialog_exe_helpers.h"
#include "engine_funcs.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Random
#include "Game/GameUpdateLoop.h"       // ENDGAME B.3.9 EXA: the real loop classes
#include "Game/GameLoadDialogLoopT.h"
#include "Game/GameLoaderLoop.h"
#include "Game/MapGenerator.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Game/ClimateGenerator.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Game/TerrainGenerator.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Dialog/exe_bodies.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// Helper: find element by name, returns raw element pointer
// ============================================================
static void* FindElement(void* dialog, const char* name) {
    System::String nameStr(name);
    uint8_t retBuf[16];
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge
    void* elem = *(void**)retBuf;
    return elem;
}

// ENDGAME B.3.9: MapGenerator / ClimateGenerator / TerrainGenerator / NameDatabase all
// have a ported TypeId() now (class statics, always populated), so the
// EnsureGeneratorTypeIds()/LazyTypeId() lazy-cache over the exe .rdata globals is dead.

// Type-check a loaded resource. In STANDALONE the Runtime factory tree is
// incomplete, so Object::IsDerivedFrom walks up the type's parent chain into a
// NULL factory and crashes (NULL-this). Per feedback_factory_cascade_strategy we
// trust FindOrLoad's result and skip the derivation check. typeId is unused (kept
// as a parameter so every call site still names which type it means).
static inline bool ResourceIsType(void* resource, uint32_t typeId) {
    if (!resource) return false;
    (void)typeId;
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
    // Now shared: GameContainer_EnsureLists (dialog_exe_helpers.cpp) fills the same
    // three slots plus +0x00 achievements and +0x08 scenarios. ScenarioDialog::Slot4
    // reads +0x08 and derefs it unguarded, so filling only these three left opening
    // the Tutorial/Scenario dialog as a null-deref crash.
    GameContainer_EnsureLists();
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
    int count = reinterpret_cast<const ::Object::ExternalList*>(externalList)  // B.3.3 bridge
                     ->GetResourceCount();
    for (int i = 0; i < count; i++) {
        // ExternalList+0x20 holds pointer to the resource pointer array
        void** ptrArray = *(void***)((uintptr_t)externalList + 0x20);
        void* resource = ptrArray[i];

        // IsDerivedFrom check (trust-helper: skips the crashing walk in standalone)
        void* typed = nullptr;
        if (ResourceIsType(resource, typeId)) {
            typed = resource;
        }

        // Check visibility flag at offset (0x38 for Map, 0x30 for Climate/Terrain)
        if (typed && *(uint8_t*)((uintptr_t)typed + visibilityOffset) != 0) {
            // Build UI::String from resource name (at +0x20 stringTable, +0x28 stringName)
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)typed + 0x20))), *reinterpret_cast<const System::String*>((void*)((uintptr_t)typed + 0x28)));  // B.3.6 bridge

            // Convert UI::String to System::String
            System::String sysStr;
            ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStr)->operator System::String());  // B.3.6 bridge

            void* combo = *comboFieldPtr;

            // Create empty sprite
            uint8_t sprite[16];
            // Sprite(ExternalPtr<SpriteSheet>(null), name) stores {null, -1} without
            // reading the name (sprite.cpp, retail 0x18011b9c0) == the default ctor.
            ::new (static_cast<void*>(sprite)) UI::Sprite();  // B.3.6 bridge

            reinterpret_cast<UI::ComboElement*>(combo)->AddItem(*reinterpret_cast<const UI::Sprite*>(sprite), sysStr, (void*)(intptr_t)i);  // B.3.6 bridge
        }

        // Re-fetch count in case it changed (matches original)
        count = reinterpret_cast<const ::Object::ExternalList*>(externalList)  // B.3.3 bridge
                     ->GetResourceCount();
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
static void GenerateTownName(System::String& retStr, void* random, int maxLen) {
    // Load the city-name database. The IsDerivedFrom type-check retail runs on
    // the result is skipped: the standalone factory tree is incomplete and the
    // walk NULL-faults, while FindOrLoad by exact filename is authoritative
    // (same call shape as citizen_names.cpp's Male.ndb/Female.ndb load).
    void* nameDB = nullptr;
    if (::Object::ExternalFactory::_singleton) {
        System::Filename fn("Game/Names/Cities.ndb");
        nameDB = const_cast<::Object::External*>(
            ::Object::ExternalFactory::_singleton   // B.3.3 bridge
                ->FindOrLoad(fn));
    }

    // minLen 5 is retail's constant; maxLen is derived from the edit box's
    // _maxLength by the caller. NameDB_GenerateName (0x1400a4b30, registered by
    // standalone/reg_namedb.cpp) constructs the String in retStr even when the
    // database is missing, so retStr is always live for the suffix fix-up below.
    NameDB_GenerateName(&retStr, nameDB, random, 5, maxLen);   // 0x1400a4b30

    // Retail: a name longer than 4 chars ending in "vill" gets an "e" appended,
    // so the generator's "-vill" stems read as "-ville".
    bool endsVill = false;
    int slen = retStr.GetLength();
    if (slen > 4) {
        System::String sfx = retStr.SubString(slen - 4, slen - 1);
        endsVill = (sfx == "vill");
    }
    if (endsVill) {
        retStr += "e";
    }
}

static void GenerateRandomName(void* editNamePtr, void* random) {
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

    System::String nameStr;
    GenerateTownName(nameStr, random, nameLen);

    // Mirrors the seed logging below: both halves of the "randomise" row report
    // what they produced, so a dead generator is visible in the log instead of
    // only on screen.
    {
        const char* sp = nameStr._data;
        fprintf(stderr, "[NewGameDialog] town name generated: \"%s\" (maxLen=%d)\n",
                sp ? sp : "(empty)", nameLen);
        fflush(stderr);
    }

    reinterpret_cast<UI::EditElement*>(*(void**)editNamePtr)->SetText(nameStr);  // B.3.6 bridge
}

// ============================================================
// Helper: generate random seed
// ============================================================
static void GenerateRandomSeed(void* editSeedPtr, void* random) {
    // NumberEditElement+0xFC contains max value
    int maxVal = *(int*)((uintptr_t)*(void**)editSeedPtr + 0xFC);

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
    const char* envSeed = Opt().seed;
    if (envSeed && envSeed[0]) {
        uint32_t fixed = (uint32_t)strtoul(envSeed, nullptr, 0);
        if (maxVal > 0) fixed %= (uint32_t)(maxVal + 1);
        reinterpret_cast<UI::NumberEditElement*>(seedElem)->SetValue(fixed);  // B.3.6 bridge
        fprintf(stderr, "[NewGameDialog] BANISHED_SEED=%s -> seed PINNED to %u "
                "(also seeds the gameplay RNG at gw+0xd8; runs still diverge — see the "
                "scope note above)\n", envSeed, fixed);
        fflush(stderr);
        return;
    }

    uint32_t seed = reinterpret_cast<System::Random*>(random)->Rand((uint32_t)(maxVal + 1));   // B.3.4 bridge
    reinterpret_cast<UI::NumberEditElement*>(seedElem)->SetValue(seed);  // B.3.6 bridge
    fprintf(stderr, "[NewGameDialog] BANISHED_SEED unset -> world seed RANDOMISED to %u\n",
            seed);
    fflush(stderr);
}

// ============================================================
// Helper: update description text from selected map generator
// ============================================================
static void UpdateDescription(NewGameDialog* self) {
    // Standalone safety: skip if the map-generator list is unresolved (covers both
    // Slot4 and the OnEvent combo-change call site).
    if (!self->mapGeneratorList) return;

    void* condCombo = self->comboCondition;

    // Get selected map generator resource
    void** ptrArray = *(void***)((uintptr_t)self->mapGeneratorList + 0x20);
    int selectedIdx = *(int*)((uintptr_t)condCombo + 0x118);
    void* resource = ptrArray[selectedIdx];

    void* mapGen = nullptr;
    if (ResourceIsType(resource, MapGenerator::TypeId())) {
        mapGen = resource;
    }

    // Build UI::String from description (at +0x20 stringTable, +0x30 stringName)
    uint8_t uiStr[16];
    ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)mapGen + 0x20))), *reinterpret_cast<const System::String*>((void*)((uintptr_t)mapGen + 0x30)));  // B.3.6 bridge

    // Set on textDescription element
    reinterpret_cast<UI::TextElement*>(self->textDescription)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
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
    const char* want = Opt().startCondition;
    if (!want || !want[0]) {
        if (!Opt().autostart)
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
    int count = reinterpret_cast<const ::Object::ExternalList*>(list)  // B.3.3 bridge
                     ->GetResourceCount();
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
        reinterpret_cast<UI::ComboElement*>(self->comboCondition)->SetCurrentItem(target);  // B.3.6 bridge
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

void NewGameDialog::OnOpened() {
    uintptr_t s = (uintptr_t)this;

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
    this->editName       = ::FindElement(this, "editName");
    this->editSeed       = ::FindElement(this, "editSeed");
    this->textDescription = ::FindElement(this, "textDescription");
    this->comboCondition = ::FindElement(this, "comboCondition");
    this->comboTerrain   = ::FindElement(this, "comboTerrain");
    this->comboTerrainSize = ::FindElement(this, "comboTerrainSize");
    this->comboClimate   = ::FindElement(this, "comboClimate");
    this->comboDisaster  = ::FindElement(this, "comboDisaster");

    // --- Cache ExternalList pointers from keymap singleton ---
    void* keyMapData = (g_gameContainer);
    if (keyMapData) {
        this->mapGeneratorList     = *(void**)((uintptr_t)keyMapData + 0x18);
        this->climateGeneratorList = *(void**)((uintptr_t)keyMapData + 0x10);
        this->terrainGeneratorList = *(void**)((uintptr_t)keyMapData + 0x20);
    }

    // --- Populate generator combos. Guarded for standalone: the ExternalLists may
    //     be unpopulated (PopulateGeneratorCombo null-checks the list internally). ---
    PopulateGeneratorCombo((void**)&this->comboCondition, this->mapGeneratorList,
                           MapGenerator::TypeId(), 0x38);
    PopulateGeneratorCombo((void**)&this->comboClimate, this->climateGeneratorList,
                           ClimateGenerator::TypeId(), 0x30);
    PopulateGeneratorCombo((void**)&this->comboTerrain, this->terrainGeneratorList,
                           TerrainGenerator::TypeId(), 0x30);

    // --- Find button elements ---
    void* buttonNewName = ::FindElement(this, "buttonNewName");
    void* buttonNewSeed = ::FindElement(this, "buttonNewSeed");
    void* buttonCancel  = ::FindElement(this, "buttonCancel");
    void* buttonOk      = ::FindElement(this, "buttonOk");

    // --- Assign element IDs ---
    // buttonNewName = 0, buttonNewSeed = 1, buttonOk = 2, buttonCancel = 3, comboCondition = 4
    reinterpret_cast<UI::Element*>(buttonNewName)->SetElementId(0);  // B.3.6 bridge

    reinterpret_cast<UI::Element*>(buttonNewSeed)->SetElementId(1);  // B.3.6 bridge

    // Store buttonOk in struct at +0xF0
    this->buttonOk = buttonOk;
    reinterpret_cast<UI::Element*>(this->buttonOk)->SetElementId(2);  // B.3.6 bridge

    reinterpret_cast<UI::Element*>(buttonCancel)->SetElementId(3);  // B.3.6 bridge

    reinterpret_cast<UI::Element*>(this->comboCondition)->SetElementId(4);  // B.3.6 bridge

    // --- Generate random name (guarded: editName must be present) ---
    if (this->editName)
        GenerateRandomName(&this->editName, (void*)(s + 0x118));

    // --- Generate random seed (guarded: editSeed must be present) ---
    if (this->editSeed)
        GenerateRandomSeed(&this->editSeed, (void*)(s + 0x118));

    // --- Restore saved combo selections from game settings ---
    // Standalone: g_pNewGameSettings (exe-data slot) may be NULL — skip the
    // whole restore block rather than dereferencing null.
    void* settings = g_gameSettings;
    uintptr_t cfg = (uintptr_t)settings;
    if (settings) {

    // Map condition/generator combo (+0x2c)
    {
        int idx = *(int*)(cfg + 0x2c);
        void* combo = this->comboCondition;
        if (idx < 0) {
            int count = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
            if (idx < count) {
                // Use the index as-is
                reinterpret_cast<UI::ComboElement*>(this->comboCondition)->SetCurrentItem(*(int*)(cfg + 0x2c));  // B.3.6 bridge
            } else {
                reinterpret_cast<UI::ComboElement*>(this->comboCondition)->SetCurrentItem(0);  // B.3.6 bridge
            }
        } else {
            reinterpret_cast<UI::ComboElement*>(this->comboCondition)->SetCurrentItem(*(int*)(cfg + 0x2c));  // B.3.6 bridge
        }
    }

    // Terrain combo (+0x20)
    {
        int idx = *(int*)(cfg + 0x20);
        void* combo = this->comboTerrain;
        if (idx < 0) {
            int count = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
            if (idx < count) {
                reinterpret_cast<UI::ComboElement*>(this->comboTerrain)->SetCurrentItem(*(int*)(cfg + 0x20));  // B.3.6 bridge
            } else {
                reinterpret_cast<UI::ComboElement*>(this->comboTerrain)->SetCurrentItem(0);  // B.3.6 bridge
            }
        } else {
            reinterpret_cast<UI::ComboElement*>(this->comboTerrain)->SetCurrentItem(*(int*)(cfg + 0x20));  // B.3.6 bridge
        }
    }

    // Terrain size combo (+0x24)
    {
        int idx = *(int*)(cfg + 0x24);
        void* combo = this->comboTerrainSize;
        if (idx < 0) {
            int count = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
            if (idx < count) {
                reinterpret_cast<UI::ComboElement*>(this->comboTerrainSize)->SetCurrentItem(*(int*)(cfg + 0x24));  // B.3.6 bridge
            } else {
                reinterpret_cast<UI::ComboElement*>(this->comboTerrainSize)->SetCurrentItem(0);  // B.3.6 bridge
            }
        } else {
            reinterpret_cast<UI::ComboElement*>(this->comboTerrainSize)->SetCurrentItem(*(int*)(cfg + 0x24));  // B.3.6 bridge
        }
    }

    // Climate combo (+0x28)
    {
        int idx = *(int*)(cfg + 0x28);
        void* combo = this->comboClimate;
        if (idx < 0) {
            int count = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
            if (idx < count) {
                reinterpret_cast<UI::ComboElement*>(this->comboClimate)->SetCurrentItem(*(int*)(cfg + 0x28));  // B.3.6 bridge
            } else {
                reinterpret_cast<UI::ComboElement*>(this->comboClimate)->SetCurrentItem(0);  // B.3.6 bridge
            }
        } else {
            reinterpret_cast<UI::ComboElement*>(this->comboClimate)->SetCurrentItem(*(int*)(cfg + 0x28));  // B.3.6 bridge
        }
    }

    // Disaster combo (+0x30)
    {
        int idx = *(int*)(cfg + 0x30);
        void* combo = this->comboDisaster;
        if (idx < 0) {
            int count = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
            if (count <= idx) {
                reinterpret_cast<UI::ComboElement*>(this->comboDisaster)->SetCurrentItem(0);  // B.3.6 bridge
            } else {
                reinterpret_cast<UI::ComboElement*>(this->comboDisaster)->SetCurrentItem(*(int*)(cfg + 0x30));  // B.3.6 bridge
            }
        } else {
            reinterpret_cast<UI::ComboElement*>(this->comboDisaster)->SetCurrentItem(*(int*)(cfg + 0x30));  // B.3.6 bridge
        }
    }

    }  // end if (settings) — standalone null-settings guard

    // --- Standalone: env/autostart start-condition override (wins over the
    //     saved-selection restore above; see SelectStartConditionOverride) ---
    SelectStartConditionOverride(this);

    // --- Combo inventory (standalone verification) ---
    // Retail contents, for comparison: condition 3 (Hard/Medium/Easy),
    // terrain 2 (Valleys/Mountains), terrainSize 3 (Small/Medium/Large),
    // climate 3 (Mild/Fair/Harsh), disaster 2 (On/Off). The first three come
    // from the generator ExternalLists, the last two from the .rsc combo's
    // _initialData StringTable, so a 0 count tells you which half is missing.
    {
        struct { const char* name; void* combo; } combos[] = {
            { "comboCondition",   this->comboCondition   },
            { "comboTerrain",     this->comboTerrain     },
            { "comboTerrainSize", this->comboTerrainSize },
            { "comboClimate",     this->comboClimate     },
            { "comboDisaster",    this->comboDisaster    },
        };
        for (auto& c : combos) {
            if (!c.combo) { fprintf(stderr, "[NewGameDialog] %s MISSING\n", c.name); continue; }
            fprintf(stderr, "[NewGameDialog] %s items=%d selected=%d\n", c.name,
                    reinterpret_cast<const UI::ComboElement*>(c.combo)->GetItemCount(),  // B.3.6 bridge
                    *(int*)((uintptr_t)c.combo + 0x118));
        }
        fflush(stderr);
    }

    // --- Update description text from selected map generator ---
    UpdateDescription(this);
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
    int condIdx = *(int*)((uintptr_t)self->comboCondition + 0x118);

    int climateIdx = *(int*)((uintptr_t)self->comboClimate + 0x118);

    // --- Climate override (standalone autostart + testing) ---
    // ClimateList.rsc order: 0 = Mild, 1 = Fair, 2 = Harsh. BANISHED_CLIMATE =
    // mild|fair|harsh (or 0|1|2). Autostart otherwise takes the combo default, which is
    // whatever the saved settings or index 0 give — the stress harness pins "fair" so the
    // seasonal temperature curve (and with it snow cover, fuel burn and field idling) is
    // the same town-to-town.
    if (const char* envClimate = Opt().climate) {
        int want = -1;
        if (!strcmp(envClimate, "0") || !_stricmp(envClimate, "mild"))       want = 0;
        else if (!strcmp(envClimate, "1") || !_stricmp(envClimate, "fair"))  want = 1;
        else if (!strcmp(envClimate, "2") || !_stricmp(envClimate, "harsh")) want = 2;
        if (want >= 0) {
            fprintf(stderr, "[NewGameDialog] BANISHED_CLIMATE=%s -> climateIdx %d -> %d\n",
                    envClimate, climateIdx, want);
            climateIdx = want;
        } else {
            fprintf(stderr, "[NewGameDialog] BANISHED_CLIMATE=%s not understood "
                    "(mild|fair|harsh|0|1|2); combo value %d kept\n", envClimate, climateIdx);
        }
    }

    int terrainIdx = *(int*)((uintptr_t)self->comboTerrain + 0x118);

    // Get combo item data pointers from ExternalList arrays
    void* mapGenItem = reinterpret_cast<const UI::ComboElement*>(self->comboCondition)->GetItemData(condIdx);  // B.3.6 bridge
    void** mapPtrArray = *(void***)((uintptr_t)self->mapGeneratorList + 0x20);
    void* mapGenResource = mapPtrArray[(intptr_t)mapGenItem];
    void* mapGen = nullptr;
    if (ResourceIsType(mapGenResource, MapGenerator::TypeId())) mapGen = mapGenResource;

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
        const char* envDiff = Opt().difficulty;
        if (envDiff) {
            if (!strcmp(envDiff, "2") || !strcmp(envDiff, "easy") || !strcmp(envDiff, "Easy"))
                wantIdx = 2;
            else if (!strcmp(envDiff, "1") || !strcmp(envDiff, "medium") || !strcmp(envDiff, "Medium"))
                wantIdx = 1;
            else if (!strcmp(envDiff, "0") || !strcmp(envDiff, "hard") || !strcmp(envDiff, "Hard"))
                wantIdx = 0;
        } else {
            if (Opt().autostart)
                wantIdx = 2;  // Easy default for the autostart demo town
        }
        if (wantIdx >= 0 && self->mapGeneratorList && mapPtrArray) {
            int listCount = reinterpret_cast<const ::Object::ExternalList*>(
                self->mapGeneratorList)->GetResourceCount();   // B.3.3 bridge
            if (wantIdx < listCount) {
                void* r = mapPtrArray[wantIdx];
                if (ResourceIsType(r, MapGenerator::TypeId())) {
                    mapGen = r;
                    fprintf(stderr, "[NewGameDialog] difficulty override -> listIdx=%d (0=Hard 1=Medium 2=Easy) mapGen=%p\n",
                            wantIdx, mapGen);
                    fflush(stderr);
                }
            }
        }
    }

    void* climateItem = reinterpret_cast<const UI::ComboElement*>(self->comboClimate)->GetItemData(climateIdx);  // B.3.6 bridge
    void** climatePtrArray = *(void***)((uintptr_t)self->climateGeneratorList + 0x20);
    void* climateResource = climatePtrArray[(intptr_t)climateItem];
    void* climateGen = nullptr;
    if (ResourceIsType(climateResource, ClimateGenerator::TypeId())) climateGen = climateResource;

    void* terrainItem = reinterpret_cast<const UI::ComboElement*>(self->comboTerrain)->GetItemData(terrainIdx);  // B.3.6 bridge
    void** terrainPtrArray = *(void***)((uintptr_t)self->terrainGeneratorList + 0x20);
    void* terrainResource = terrainPtrArray[(intptr_t)terrainItem];
    void* terrainGen = nullptr;
    if (ResourceIsType(terrainResource, TerrainGenerator::TypeId())) terrainGen = terrainResource;

    // Get terrain size index
    uint32_t terrainSize = *(uint32_t*)((uintptr_t)self->comboTerrainSize + 0x118);

    // --- Map size override (standalone autostart + testing) ---
    // terrainSize is a row index into the TerrainGenerator's size table
    // (Valleys.rsc / Mountains.rsc: 0 = 256 small, 1 = 384 medium, 2 = 512 large).
    // BANISHED_MAP_SIZE = small|medium|large (or 0|1|2). The stress harness sets
    // "medium" by default; unset leaves the combo's value alone.
    if (const char* envSize = Opt().mapSize) {
        int want = -1;
        if (!strcmp(envSize, "0") || !_stricmp(envSize, "small"))       want = 0;
        else if (!strcmp(envSize, "1") || !_stricmp(envSize, "medium")) want = 1;
        else if (!strcmp(envSize, "2") || !_stricmp(envSize, "large"))  want = 2;
        if (want >= 0) {
            fprintf(stderr, "[NewGameDialog] BANISHED_MAP_SIZE=%s -> terrainSize %u -> %d\n",
                    envSize, terrainSize, want);
            terrainSize = (uint32_t)want;
        } else {
            fprintf(stderr, "[NewGameDialog] BANISHED_MAP_SIZE=%s not understood "
                    "(small|medium|large|0|1|2); combo value %u kept\n", envSize, terrainSize);
        }
    }

    // Get seed value
    uint32_t seed = (uint32_t)reinterpret_cast<const UI::NumberEditElement*>(self->editSeed)->GetValue();  // B.3.6 bridge

    // Get disaster enabled (0 = disasters on, nonzero = off)
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
    void* nameStr = (void*)((uintptr_t)self->editName + 0xC8);

    // Save combo selections to game settings
    void* settings = g_gameSettings;
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
                savedStateIBP, GameUpdateLoop::Vtable());
        fflush(stderr);

        // Step 1: GameUpdateLoop (0x160 bytes). ENDGAME B.3.9 EXA: the real class
        // (its ctor is 0x140098060, which retail CALLs here). Retail passes
        // savedState = 0 and a copy of *param_10 (the saved-state IBP) by
        // address; the tree's new-game caller hands a null param_10, which the
        // tree has always read as a null IBP. Retail's +0xD0 is `p8 ? 0 : 0xF0`;
        // this function's `disastersOff` is the inverse sense, so p8 = !disastersOff
        // writes exactly the bytes the old inline stores wrote.
        void* gameLoop = System::MemoryAllocator::MemAlloc(0x160, 8);
        Engine::UpdateLoop* pGameLoop = nullptr;
        if (gameLoop) {
            const uint64_t ibp = savedStateIBP ? *(uint64_t*)savedStateIBP : 0;
            pGameLoop = ::new (gameLoop) GameUpdateLoop(
                nullptr, ibp, (int32_t)scenarioResIdx,
                *reinterpret_cast<const System::String*>(nameStr),   // editName+0xC8
                (int32_t)seed, (int32_t)terrainSize, /*p8=*/!disastersOff,
                mapGen, terrainGen, climateGen);
        }

        // Step 2: GameLoadDialogLoopT (0xB60 bytes) — retail inlines the subclass
        // ctor here; the real class's ctor is that body.
        void* dlLoop = System::MemoryAllocator::MemAlloc(0xB60, 4);
        Engine::UpdateLoop* pDlLoop = nullptr;
        if (dlLoop) {
            uint64_t delayTime = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&delayTime), 1.0f);
            System::String loadingStr;
            GetLoadingSoundString(&loadingStr);
            pDlLoop = ::new (dlLoop) GameLoadDialogLoopT(
                loadingStr, pGameLoop, *reinterpret_cast<const System::Time*>(&delayTime));
        }

        // Step 3: outer LoaderLoop (0x48 bytes): `??0LoaderLoop` + the 0x1401588d0
        // stamp, i.e. the real GameLoaderLoop.
        void* outerLoop = System::MemoryAllocator::MemAlloc(0x48, 4);
        void* pOuterLoop = nullptr;
        if (outerLoop) {
            uint64_t zeroTime = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&zeroTime), 0.0f);
            pOuterLoop = ::new (outerLoop) GameLoaderLoop(
                pDlLoop, *reinterpret_cast<const System::Time*>(&zeroTime));
        }

        // Step 4: Set next loop
        // In StartNewGame, the check is: +0xC0 (loaderLoop1) == 0 -> use engine at +0xB8
        uintptr_t loaderLoop1 = *(uintptr_t*)(ws + 0xC0);
        uintptr_t engPtr0 = *(uintptr_t*)(ws + 0xB8);
        fprintf(stderr, "[StartNewGame] loops built game=%p dl=%p outer=%p; "
                "ws+0xC0(loop)=%p ws+0xB8(engine)=%p -> %s\n",
                (void*)pGameLoop, (void*)pDlLoop, pOuterLoop, (void*)loaderLoop1, (void*)engPtr0,
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

void NewGameDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t eventType, int32_t eventData, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    uintptr_t s = (uintptr_t)this;
    fprintf(stderr, "[NewGameDialog::OnEvent] this=%p type=0x%x data=%d\n",
            (void*)this, eventType, eventData);
    fflush(stderr);

    if (eventType == 0x12) {
        // Button click
        if (eventData == 2) {
            // buttonOk: start game
            NewGameDialog_StartGame(this);
            return;
        }
        if (eventData == 3) {
            // buttonCancel: close dialog
            void* ui = *(void**)(s + 0x10);
            uint64_t basePtr[2];
            new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(basePtr));  // B.3.6 bridge
            return;
        }
        if (eventData == 0) {
            // buttonNewName: generate random name
            GenerateRandomName(&this->editName, (void*)(s + 0x118));
            return;
        }
        if (eventData == 1) {
            // buttonNewSeed: generate random seed
            GenerateRandomSeed(&this->editSeed, (void*)(s + 0x118));
            return;
        }
    } else if (eventType == 0x14 && eventData == 4) {
        // comboCondition selection changed: update description text
        UpdateDescription(this);
    }
}

// ============================================================
// Slot5 (vtable +0x28, exe 0x1400706c0): Shutdown — RET 0 (3 bytes)
// ============================================================
// No-op shutdown; nothing to clean up.
void NewGameDialog::OnClosed() {}

void NewGameDialog::OnUpdateLayout() {
    void* okBtn = this->buttonOk;

    void* editElem = this->editName;

    // EditElement+0xC8 = text string pointer (System::String)
    int64_t strPtr = *(int64_t*)((uintptr_t)editElem + 200);  // 200 = 0xC8
    int len = 0;
    if (strPtr != 0) {
        // String length is at strPtr-8 (COW header)
        len = *(int*)(strPtr - 8);
    }

    // Virtual call: vtable[8] = SetEnabled(bool)
    reinterpret_cast<UI::Element*>(okBtn)->Enable(len > 0 ? 1 : 0);  // B.3.6 bridge
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

// ENDGAME B.3.8 block DLG: NewGameDialog is a real class (Dialog/NewGameDialog.h);
// the hand-built table is gone. Slot 7 is INHERITED: the old table called
// DialogInstance::OnUpdate there with a qualified call (retail's import thunk).
// 0x14015c920 has no by-address reader, so its registration went with the table.

// The real ctor — retail 0x140071040 ("NewGameDialog_CopyCtor" in Ghidra):
//   DialogInstance::DialogInstance(self, ui, extRes, &nullParent, 0,0,0)
//   zero +0xB8..+0x110 (the 12 element-pointer fields)
//   System::Random::Random(self+0x118)
//   worldSetup(+0xAE0) = *parentMenuIBP   (the MainMenuDialog, used by StartGame)
NewGameDialog::NewGameDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                             Object::InternalPtr<UI::DialogInstance> parent,
                             uint64_t parentMenu)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      comboTerrain(nullptr), comboTerrainSize(nullptr), comboClimate(nullptr),
      comboCondition(nullptr), comboDisaster(nullptr), editName(nullptr),
      editSeed(nullptr), buttonOk(nullptr), textDescription(nullptr),
      mapGeneratorList(nullptr), climateGeneratorList(nullptr),
      terrainGeneratorList(nullptr) {
    // System::Random sub-object at +0x118: ??0Random@System@@QEAA@XZ, whose body is
    // RtSystem_Random_Ctor (called by name, B3R).
    RtSystem_Random_Ctor(reinterpret_cast<System::Random*>(random));
    worldSetup = reinterpret_cast<void*>(parentMenu);
}

// The reflected create path (create 0x140012c70). The tree's factory handed out
// a zero-filled object with no base ctor; this keeps the zero fields and runs
// the default DialogInstance ctor.
NewGameDialog::NewGameDialog()
    : comboTerrain(nullptr), comboTerrainSize(nullptr), comboClimate(nullptr),
      comboCondition(nullptr), comboDisaster(nullptr), editName(nullptr),
      editSeed(nullptr), buttonOk(nullptr), textDescription(nullptr),
      mapGeneratorList(nullptr), climateGeneratorList(nullptr),
      terrainGeneratorList(nullptr), random{}, _pad_AD8(0), worldSetup(nullptr) {}

// create 0x140012c70: MemAlloc(0xae8, 8), then the ctor.
void* NewGameDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NewGameDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) NewGameDialog());
}
// createInPlace 0x140012cb0 (the tree's in-place build is kept, B8-13).
void* NewGameDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) NewGameDialog());
}


// The ctor DOOR, reached by address: fn_NewGameDialog_Ctor (engine_funcs.cpp)
// resolves 0x140071040. Signature matches fn_NewGameDialog_Ctor_t
// (self, ui, extRes, nullParent, parentMenuIBP).
void* NewGameDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                    void* nullParent, void* parentMenuIBP) {
    if (!self) return nullptr;
    printf("[NewGameDialog_Ctor] self=%p ui=%p extRes=%p nullParent=%p parentMenu=%p\n",
           self, ui, extRes, nullParent, parentMenuIBP);
    Object::InternalPtr<UI::DialogInstance> parent;
    if (nullParent)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(nullParent);
    NewGameDialog* dlg = ::new (self) NewGameDialog(
        static_cast<UI::UserInterface*>(ui), static_cast<const UI::Dialog*>(extRes), parent,
        parentMenuIBP ? *static_cast<const uint64_t*>(parentMenuIBP) : 0);

    // Publish the just-constructed dialog so the auto-start driver
    // (standalone/autostart.cpp) can locate it and fire Start once OnOpened has
    // populated the combos + OK button. Harmless when autostart is disabled.
    g_StandaloneNewGameDialog = dlg;
    return dlg;
}

extern "C" void RegisterNewGameDialogVtable() {
    RegisterExeAddr(0x140071040, reinterpret_cast<uintptr_t>(&NewGameDialog_Ctor_Standalone));
}

DEFINE_EXE_TYPE(NewGameDialog, "UI::DialogInstance")
