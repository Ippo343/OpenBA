// engine/src/ui/scenariodialog.cpp
// Clean reimplementation of ScenarioDialog methods.
//
// ScenarioDialog (0xE8 bytes): Scenario selection screen.
// Layout: +0xC8 = selected index (int32), +0xD8 = scenario array,
//         +0xE0 = scenario count

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K1)
#include "save/SaveLoad.h"   // ENDGAME B.3.7 callers (K1)
#include "Dialog/ScenarioDialog.h"
#include "Dialog/ui_dialogs.h"
#include "Object/factory_registration.h"
#include "Dialog/dialog_exe_helpers.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "io/Persistence.h"   // ENDGAME B.3.4 caller batch P
#include "io/FileSystem.h"    // ENDGAME B.3.4 caller batch P
#include <new>
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Game/Scenario.h"   // X::TypeId() (B.3.9 CG2)
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

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

void ScenarioDialog::OnOpened() {
    // The Game container slot this function reads (+0x08 scenario ExternalList) is
    // NULL until something loads it - standalone never runs retail GameContainer_Init.
    // Fill it upstream rather than guarding the GetResourceCount deref below, which is
    // byte-faithful and load-bearing.
    GameContainer_EnsureLists();
    uintptr_t s = (uintptr_t)this;
    uint8_t selfDifficulty = *(uint8_t*)(s + 0xB8);
    printf("[ScenarioDialog::Slot4] Setup called, this=%p, difficulty=%d\n", this, selfDifficulty);

    // --- Find "buttonCancel" → set element ID 100, set +0xC0 flag = 8 ---
    {
        System::String nameStr("buttonCancel");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(this)->FindElement(nameStr));  // B.3.6 bridge
        void* cancelBtn = *(void**)retBuf;
        reinterpret_cast<UI::Element*>(cancelBtn)->SetElementId(100);  // B.3.6 bridge
        *(uint32_t*)((uintptr_t)cancelBtn + 0xC0) = 8;
    }

    // --- Find "buttonOk" → store at +0xD0, set ID 0x65 (101), disable it ---
    {
        System::String nameStr("buttonOk");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(this)->FindElement(nameStr));  // B.3.6 bridge
        void* okBtn = *(void**)retBuf;

        *(void**)(s + 0xD0) = okBtn;
        reinterpret_cast<UI::Element*>(okBtn)->SetElementId(0x65);  // B.3.6 bridge

        // Disable via vtable[8]: SetEnabled(false)
        reinterpret_cast<UI::Element*>(okBtn)->Enable(false);  // B.3.6 bridge
    }

    // --- Get scenario list from singleton at *g_pKeyMapSingleton + 8 ---
    // The global at 0x1401c3de8 points to a management struct:
    //   +0x00 = KeyMap, +0x08 = ExternalList (scenario list)
    void* mgmtObj = *g_pKeyMapSingleton;
    void* scenarioList = *(void**)((uintptr_t)mgmtObj + 8);

    // --- Find "tableScenario" element ---
    void* table = nullptr;
    {
        System::String nameStr("tableScenario");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(this)->FindElement(nameStr));  // B.3.6 bridge
        table = *(void**)retBuf;
    }

    // --- Get scenario count and iterate ---
    int scenarioCount = reinterpret_cast<const ::Object::ExternalList*>(scenarioList)  // B.3.3 bridge
                             ->GetResourceCount();
    int entryIndex = 0;  // counts matched scenarios (for button IDs and array)
    printf("[ScenarioDialog::Slot4] scenarioCount=%d, mgmtObj=%p, scenarioList=%p\n",
           scenarioCount, mgmtObj, scenarioList);

    for (int i = 0; i < scenarioCount; i++) {
        // Get resource[i] from ExternalList: *(array+0x20)[i*8]
        void* resource = *(void**)(*(uintptr_t*)((uintptr_t)scenarioList + 0x20) + (uintptr_t)i * 8);

        // Check IsDerivedFrom(Scenario). ENDGAME B.3.9 CG2: TypeId_Scenario (cell
        // 0x1401c5b08) -> Scenario::TypeId(); the "unregistered" null guard was
        // scaffolding for the unresolved global and goes with the conversion.
        // This rejection used to be silent, which made an empty scenario table
        // indistinguishable from "no scenarios installed". Report it once.
        bool isScenario = ::Object::IsDerivedFromNullSafe(resource, Scenario::TypeId());
        void* scenario = isScenario ? resource : nullptr;
        if (!scenario) {
            fprintf(stderr, "[ScenarioDialog] resource[%d]=%p rejected by IsDerivedFrom(typeId=%u)\n", i, resource, Scenario::TypeId());
            fflush(stderr);
            continue;
        }

        // Filter by difficulty: scenario+0x69 must match this+0xB8
        uint8_t scenDifficulty = *(uint8_t*)((uintptr_t)scenario + 0x69);
        uint8_t selfDifficultyVal = *(uint8_t*)(s + 0xB8);
        printf("[ScenarioDialog::Slot4] scenario[%d] at %p: difficulty=%d (want %d), isScenario=%d\n",
               i, scenario, scenDifficulty, selfDifficultyVal, isScenario);
        if (scenDifficulty != selfDifficultyVal) continue;

        // --- Clone template element from Dialog ---
        // GetTemplate(dialog, retval, 0)
        uint8_t templateBuf[8];
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18))->GetTemplate(0));  // B.3.6 bridge

        // Clone via vtable[8]: templateObj->Clone(&cloneBuf, dialog)
        // (= UI::ElementDescription slot 8 CreateInstance, RETSLOT per B6-2)
        void* templateObj = *(void**)templateBuf;
        uint8_t cloneBuf[16];
        reinterpret_cast<const UI::ElementDescription*>(templateObj)->CreateInstance(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(cloneBuf),
            reinterpret_cast<const UI::Dialog*>(*(void**)(s + 0x18)));
        void* clonedElem = *(void**)cloneBuf;

        // --- Insert clone into table ---
        uint8_t clonePtr[8];
        *(void**)clonePtr = clonedElem;
        reinterpret_cast<UI::TableElement*>(table)->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(clonePtr), entryIndex);  // B.3.6 bridge

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
        reinterpret_cast<UI::Element*>(clonedElem)->SetFlagsOnOff(0x10000, 0);  // B.3.6 bridge

        // Set element ID = entryIndex + 1000
        reinterpret_cast<UI::Element*>(clonedElem)->SetElementId(entryIndex + 1000);  // B.3.6 bridge

        // --- Find sub-elements in the clone ---

        // "imageIcon" → entry[0x10]
        {
            System::String nameStr("imageIcon");
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(clonedElem)->FindElement(nameStr));  // B.3.6 bridge
            *(void**)(entry + 0x10) = *(void**)retBuf;
        }

        // "textTitle" → entry[0x18]
        {
            System::String nameStr("textTitle");
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(clonedElem)->FindElement(nameStr));  // B.3.6 bridge
            *(void**)(entry + 0x18) = *(void**)retBuf;
        }

        // "textDescription" → entry[0x20]
        {
            System::String nameStr("textDescription");
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(clonedElem)->FindElement(nameStr));  // B.3.6 bridge
            *(void**)(entry + 0x20) = *(void**)retBuf;
        }

        // "selectSave" → entry[0x08]
        {
            System::String nameStr("selectSave");
            uint8_t retBuf[16];
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(clonedElem)->FindElement(nameStr));  // B.3.6 bridge
            *(void**)(entry + 0x08) = *(void**)retBuf;
        }

        // Store scenario pointer and resource index
        *(void**)(entry + 0x28) = scenario;
        *(int*)(entry + 0x30) = i;

        // --- Set icon sprite from scenario ---
        // GetSpriteName returns String (non-trivial return: RCX=this, RDX=retval)
        {
            System::String spriteNameStr;
            ::new (&spriteNameStr) System::String(reinterpret_cast<const Achievement::Achievement*>(scenario)->GetSpriteName());  // B.3.7 bridge

            // Create UI::Sprite(retval, spriteSheet, spriteName)
            // spriteSheet = *(scenario + 0x38)
            uint64_t spriteSheet = *(uint64_t*)((uintptr_t)scenario + 0x38);
            uint8_t sprite[16];
            ::new (sprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheet)), spriteNameStr);  // B.3.6 bridge

            // SetSprite on imageIcon
            void* imageIcon = *(void**)(entry + 0x10);
            reinterpret_cast<UI::ImageElement*>(imageIcon)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge
        }

        // --- Set title text from scenario ---
        {
            System::String titleStr;
            ::new (&titleStr) System::String(reinterpret_cast<const Achievement::Achievement*>(scenario)->GetTitle());  // B.3.7 bridge

            // Create UI::String(retval, stringTable, titleStr)
            // stringTable = *(scenario + 0x20)
            uint64_t stringTable = *(uint64_t*)((uintptr_t)scenario + 0x20);
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), titleStr);  // B.3.6 bridge

            // SetText on textTitle
            void* textTitle = *(void**)(entry + 0x18);
            reinterpret_cast<UI::TextElement*>(textTitle)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
        }

        // --- Set description text from scenario ---
        {
            System::String descStr;
            ::new (&descStr) System::String(reinterpret_cast<const Achievement::Achievement*>(scenario)->GetDescription());  // B.3.7 bridge

            // Create UI::String(retval, stringTable, descStr)
            uint64_t stringTable = *(uint64_t*)((uintptr_t)scenario + 0x20);
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), descStr);  // B.3.6 bridge

            // SetText on textDescription
            void* textDesc = *(void**)(entry + 0x20);
            reinterpret_cast<UI::TextElement*>(textDesc)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
        }

        // --- Push entry to dynamic array at this+0xD8 ---
        // B.3.2 bridge: ScenarioDialog+0xD8 is Collection::Array<{7 x uint64
        // fields}> (stride 0x38, count +0xE0, capacity +0xE4) inside a struct
        // §B.2 has not converted, so it is reached at its offset. Was
        // FUN_140013b60's own inlined SetSize (ScenarioDialog_ArrayPush_Impl).
        {
            uint64_t* entryQW = (uint64_t*)entry;
            auto& rows = GameColl::ArrayAt<GameColl::Bytes<0x38, 8>>(s, 0xD8);
            rows.SetSize(rows.GetSize() + 1,
                         GameColl::Bytes<0x38, 8>(entryQW[0], entryQW[1], entryQW[2], entryQW[3],
                                        entryQW[4], entryQW[5], entryQW[6]));
        }

        printf("[ScenarioDialog::Slot4] Added entry[%d] for scenario[%d]\n", entryIndex, i);
        // Identity dump: which scenario record backs each list row, and which .sav
        // it starts from. A tutorial opening on an already-developed town is
        // expected -- the series is progressive -- so this is the only way to tell
        // "tutorial N legitimately continues N-1's town" from a real off-by-one.
        {
            void* nameData = *(void**)(scenario + 0x58);
            void* saveData = *(void**)(scenario + 0x88);
            int nameLen = nameData ? *(int*)((uint8_t*)nameData - 8) : 0;
            int saveLen = saveData ? *(int*)((uint8_t*)saveData - 8) : 0;
            printf("[ScenarioID] entry[%d] scenario[%d] name=\"%.*s\" (len=%d) "
                   "save=\"%.*s\" (len=%d)\n",
                   entryIndex, i,
                   nameLen, (const char*)nameData, nameLen,
                   saveLen, (const char*)saveData, saveLen);
        }
        entryIndex++;
    }
    printf("[ScenarioDialog::Slot4] Done. Total entries: %d\n", entryIndex);
}

// ============================================================
// ScenarioDialog::Slot7 (vtable +0x38, exe 0x140078000): RET 0 (3 bytes)
// ============================================================
// No-op handler; returns 0.
void ScenarioDialog::OnUpdate(const System::Time& /*time*/) {
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
void ScenarioDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

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
        reinterpret_cast<UI::Element*>(elem)->SetFlagsOnOff(isNotSelected ? 1 : 0, isNotSelected ? 0 : 1);  // B.3.6 bridge
    }

    // Enable/disable confirm button based on valid selection
    void* confirmBtn = (void*)*(uint64_t*)(s + 0xD0);
    reinterpret_cast<UI::Element*>(confirmBtn)->Enable(selected >= 0);  // B.3.6 bridge
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

    // Get save name string from scenario+0x88. B.3.1 bridge: raw exe-struct
    // SystemString fields, not owned here.
    System::String saveNameStr = *reinterpret_cast<System::String*>(scenario + 0x88);

    // Get display name string from scenario+0x58
    System::String displayNameStr = *reinterpret_cast<System::String*>(scenario + 0x58);

    uint64_t saveStrPtr = (uint64_t)(uintptr_t)saveNameStr._data;
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
        void* parentCtrl = *(void**)(s + 0xC0);

        // Build InternalBasePtr(null) for retval
        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        printf("[ScenarioDialog::LoadSelected] NEW SCENARIO: seed=%u, terrainSize=%u, disasterOff=%d, scenarioResIdx=%u\n",
               seedVal, terrainSizeVal, (int)disasterOff, scenarioResIdx);
        printf("[ScenarioDialog::LoadSelected] mapGen=%p, terrainGen=%p, climateGen=%p, parentCtrl=%p\n",
               mapGen, terrainGen, climateGen, parentCtrl);

        // Call original FUN_1400b4540 directly — constructs GameUpdateLoop ->
        // GameLoadDialogLoopT -> LoaderLoop chain and sets next loop.
        // 0x1400b4540 StartNewGame_Reg. The door typedef passed seed / terrainSize as
        // uint32 values; the body takes pointer-width slots and reads their low dword
        // ((uint32_t)(uintptr_t)), so the zero-extended value is the same bits (B3R).
        StartNewGame_Reg(parentCtrl, &displayNameStr,
                         reinterpret_cast<void*>(static_cast<uintptr_t>(seedVal)),
                         reinterpret_cast<void*>(static_cast<uintptr_t>(terrainSizeVal)),
                         disasterOff, mapGen, terrainGen, climateGen,
                         scenarioResIdx, nullIBP);
        printf("[ScenarioDialog::LoadSelected] fn_StartNewGame returned\n");
    } else {
        fflush(stderr);
        {
            const char* missing = nullptr;
            if (!::Save::SaveLoad::_singleton) missing = "SaveLoad singleton";
            else if (!::IO::Persistence::_singleton) missing = "Persistence singleton";
            else if (!::IO::FileSystem::_singleton) missing = "FileSystem singleton";
            if (missing) {
                fprintf(stderr, "[ScenarioDialog] SAVED branch ABORTED - %s is NULL\n", missing);
                fflush(stderr);
                return;
            }
        }
        void* saveLoad = ::Save::SaveLoad::_singleton;
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(3), 0);  // B.3.7 bridge
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveName(saveNameStr, 2);  // B.3.7 bridge

        // Get save path and set data directory
        // Original flow (from assembly at 0x140077d41-0x140077d96):
        //   1. GetSavePath → Filename at RSP+0xa8 (kept alive until after loop)
        //   2. GetDataDirectory → temp at RSP+0xb0, returns ref in RAX
        //   3. persistence+0x90 = RAX (data dir)
        //   4. Destroy RSP+0xb0 (GetDataDirectory temp)
        //   5. SaveLoad::Update loop
        //   6. persistence+0x90 = RSP+0xa8 (restore from GetSavePath result)
        //
        // Despite the "Filename return buffer" comment this slot's actual runtime
        // type (per Persistence::GetSavePath's export signature) is System::String;
        // String and Filename share the exact same 8-byte layout and release
        // sequence (Filename.h), so this was always safe, just misnamed.
        void* persistence = ::IO::Persistence::_singleton;
        System::String savePathBuf;  // kept alive until after the SaveLoad::Update loop
        // B.3.4 bridge. This IS the door's own body moved to the call site: the
        // member returns `System::String` BY VALUE through MSVC's hidden return
        // slot, and `&savePathBuf` is the slot the door was handed, so the
        // prvalue is constructed straight into it (guaranteed elision) — exactly
        // one AddRef, as before, and ~String at scope exit is the one release.
        ::new (&savePathBuf) System::String(
            reinterpret_cast<const IO::Persistence*>(persistence)->GetSavePath());
        // B.3.1 bridge: raw exe-struct SystemString field, not owned here.
        System::String& persistenceDataDir = *reinterpret_cast<System::String*>(reinterpret_cast<uintptr_t>(::IO::Persistence::_singleton) + 0x90);
        {
            void* fileSystem = ::IO::FileSystem::_singleton;
            System::String dataDirBuf;  // destroyed at the end of this block (matches retail's early release)
            // B.3.4 bridge. The member returns `System::Filename` by value; the
            // door placement-constructed it into this very slot, and a Filename
            // and a String are the same 8 bytes with the same release sequence
            // (Filename.h), which is what the comment above already records. One
            // AddRef here, one ~String release at the end of the block.
            ::new (reinterpret_cast<System::Filename*>(&dataDirBuf)) System::Filename(
                reinterpret_cast<const IO::FileSystem*>(fileSystem)->GetDataDirectory());
            persistenceDataDir = dataDirBuf;
        }

        // Run SaveLoad::Update loop until operation completes
        int operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
        printf("[ScenarioDialog::LoadSelected] SAVED SCENARIO: entering SaveLoad loop, initial operation=%d\n", operation);
        int loopCount = 0;
        while (operation != 0) {
            uint8_t zeroTime[8];
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(zeroTime), 0.0f);
            reinterpret_cast<Save::SaveLoad*>(saveLoad)->Update(*reinterpret_cast<const System::Time*>(zeroTime));  // B.3.7 bridge
            operation = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
            loopCount++;
            if (loopCount % 100 == 0) {
                printf("[ScenarioDialog::LoadSelected] SaveLoad loop iteration %d, operation=%d\n", loopCount, operation);
            }
        }
        printf("[ScenarioDialog::LoadSelected] SaveLoad loop done after %d iterations\n", loopCount);

        // Restore persistence path from GetSavePath result (matches original at 0x140077df0)
        persistenceDataDir = savePathBuf;
        // NOT Filename::~Filename, and NOT here. Persistence::GetSavePath returns a
        // System::String (verified in the DLL export table), so destroying its 8-byte
        // result with the Filename dtor read the uninitialised tail of this 16-byte
        // stack buffer and faulted on the garbage pointer it found. Retail destroys
        // it as a String, and only after WorldSetup_LoadFromSave has run - see the
        // String::~String(local_res10) at the tail of 0x140077cb0. Moved below.

        // Get parent controller and load from saved state
        void* parentCtrl = *(void**)(s + 0xC0);

        uint8_t nullIBP[8];
        new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        uint32_t scenarioResIdx = *(uint32_t*)(entryArray + (uintptr_t)selectedIdx * 0x38 + 0x30);
        void* savedState = reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetSaveState();  // B.3.7 bridge

        // Use fn_LoadScenarioFromSave for the complex loop construction
        WorldSetup_LoadFromSave(parentCtrl, savedState,   // 0x1400b49f0
                                (void*)(uintptr_t)scenarioResIdx, nullIBP);
        // retail: String::~String(local_res10) — savePathBuf's scope ends here, matching.
    }
}

// ============================================================
// ScenarioDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// Slot8 (0x140077F80): OnCommand — selection change and button dispatch.
// param3=2 (selection change): updates selected index from button IDs 1000+
// param3=0x12 (button click): id=100 → close, id=101 → load selected scenario
void ScenarioDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    uintptr_t s = (uintptr_t)this;
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
            new (basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            void* ui = *(void**)(s + 0x10);
            reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(basePtr));  // B.3.6 bridge
        } else if (param4 == 0x65) {
            // Confirm/load button — reimpl of FUN_140077cb0
            ScenarioDialog_LoadSelected(this);
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

ScenarioDialog::ScenarioDialog()
    : difficulty(0), dataC0(0), dataC8(-1), dataD0(0), dataD8(0), dataE0(0) {}

// ScenarioDialog_CopyCtor (0x1400786b0): shared by ScenarioDialog and the
// TutorialDialog (same class; difficulty byte at +0xB8 differentiates the two —
// MainMenuDialog_OpenScenario passes 0, MainMenuDialog_OpenTutorial passes 1).
//   base ctor: DialogInstance(ui, extRes, parent, 0, 0, 0)
//   +0xB8 (u8)  = difficulty
//   +0xC0 (u64) = *parentMenuIBP   (selection's parent-controller InternalBasePtr)
//   +0xC8 (i32) = -1               (selectedIndex, read by OnElementEvent)
//   +0xD0 (u64) = 0
//   +0xD8 (u64) = 0                (scenario entry array)
//   +0xE0 (u64) = 0                (scenario count, read by OnOpened)
ScenarioDialog::ScenarioDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                               Object::InternalPtr<UI::DialogInstance> parent,
                               uint64_t parentMenu, uint8_t difficultyArg)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0),
      difficulty(difficultyArg), dataC0(parentMenu), dataC8(-1),
      dataD0(0), dataD8(0), dataE0(0) {}

// The body of ScenarioDialog_ScalarDtor (0x140013af0) before ~DialogInstance:
// free the +0xD8 scenario entry array (entries are plain-copy 7 x u64, no
// per-entry String dtor), clear its header.
ScenarioDialog::~ScenarioDialog() {
    System::MemoryAllocator::MemFree(reinterpret_cast<void*>(dataD8));
    dataD8 = 0;
    dataE0 = 0;
}

// create 0x140013970: MemAlloc(0xe8, 8), then the ctor.
void* ScenarioDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ScenarioDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) ScenarioDialog());
}
// createInPlace 0x1400139f0 (the tree's in-place build is kept, B8-13).
void* ScenarioDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) ScenarioDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4). Nothing reads 0x14015d2d0
// back by address any more, so its registration (reg_nullsweep.cpp) is gone.

// The ctor DOOR, bound by name in engine_funcs.cpp (fn_ScenarioDialog_Ctor).
void* ScenarioDialog_Ctor(void* self, void* ui, void* extRes, void* parentIBP,
                          void* parentMenuIBP, uint8_t difficulty) {
    if (!self) return nullptr;
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) ScenarioDialog(static_cast<UI::UserInterface*>(ui),
                                       static_cast<const UI::Dialog*>(extRes), parent,
                                       parentMenuIBP ? *static_cast<const uint64_t*>(parentMenuIBP) : 0,
                                       difficulty);
}
