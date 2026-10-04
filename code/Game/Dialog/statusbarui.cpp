// code/Game/Dialog/statusbarui.cpp
// Clean reimplementation of StatusBarUI methods.
//
// StatusBarUI (0x1D0 bytes): Main HUD bar showing date, population,
// resources, weather, citizen status indicators.
//
// Vtable layout (reimpl names -> actual function):
//   +0x28 (Draw/Slot5):     0x14007a3d0 — element finding/init (319 lines)
//   +0x38 (Startup/Slot7):  0x1400798d0 — main HUD update (299 lines)
//   +0x48 (OnRemove/Slot9): 0x14007a3a0 — status icon click handler (reimplemented)
//
// Original source: C:\Projects\Buildy\code\Game\Dialog/StatusBarUI.h
//                  Dialog\StatusBarUI.cpp

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/NumberElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/ui_panels.h"
#include "Dialog/citizen_icon_helpers.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/CitizenDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Component/StatusIconComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/CitizenComponent.h"
#include "Entity/CitizenBehavior.h"
#include "Entity/ComponentEntity.h"
#include "Game/Calendar.h"
#include "Game/WeatherSimulation.h"
#include "Game/PlayerToolManager.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// CitizenUI_UpdateIconDisplay_Impl and CitizenUI_BuildIconArray_Impl
// are now in the shared header: Dialog/citizen_icon_helpers.h
// (removed local copies — see git history for originals)

// ============================================================
// Constants from exe .rdata
// ============================================================
static constexpr float kMaxHealth       = 1.0f;   // DAT_14017214c
static constexpr float kSecondsPerYear  = 3600.0f; // DAT_14017225c
static constexpr float kMonthsPerYear   = 12.0f;   // DAT_1401721b8
static constexpr float kFreezingF       = 32.0f;   // DAT_1401721d0
static constexpr float kFtoC_Num        = 5.0f;    // DAT_140172198
static constexpr float kFtoC_Den        = 9.0f;    // DAT_1401721ac

// ============================================================
// Helper: the dialog's element of that name (retail: DialogInstance::
// FindElement, then an IsDerivedFrom check the tree does not repeat)
// ============================================================
template <class T>
static T* FindNamed(StatusBarUI* self, const System::String& name) {
    return static_cast<T*>(self->_dialog->FindElement(name).Get());
}
template <class T>
static T* FindNamed(StatusBarUI* self, const char* name) {
    return FindNamed<T>(self, System::String(name));
}

namespace statusbarui_sa {

// ENDGAME B.3.9 CG2: TypeId_StatusIconComponent (cell 0x1401c7e08) ->
// StatusIconComponent::TypeId(), TypeId_CitizenDescription -> CitizenDescription::
// TypeId(). Both class statics always answer, so the backfill retires (and the
// empty EnsureGlobals that was left went with B3R).

// Every function/global the Draw+Startup+event paths dereference unguarded must
// be live, or a null-deref faults (no SEH under MinGW). B3R: the engine_funcs door
// readiness tests were always true and are gone; the allocator singleton (the old
// g_pMemAllocSingleton snapshot) is read live.
bool Ready() {
    return System::MemoryAllocator::_singleton != nullptr;
}

// The world-side data Startup reads unguarded: the town (m_citizenList) with its
// resource stock block (professionResources), the calendar and the weather. The
// citizen roster may legitimately be count 0 (handled by the impl).
bool TownReady() {
    const GameUpdateLoop* game = GameUpdateLoop::_singleton;
    if (game == nullptr) return false;
    const CitizenBehavior* town = game->m_citizenList;
    if (!town) return false;
    if (town->professionResources == nullptr) return false;  // resource stock
    if (game->m_dayCounter == nullptr) return false;          // game clock
    if (game->m_weather == nullptr) return false;             // weather
    return true;
}

// Controller bound to its element + dialog instance (framework sets +0x18/+0x20).
inline bool Bound(const StatusBarUI* self) {
    return self->_dialog != nullptr && self->_container != nullptr;
}

// Draw bound the core elements (they all exist in Dialog/StatusBar.rsc:content).
inline bool Built(const StatusBarUI* ui) {
    return ui->textSeason && ui->textPopulation && ui->numberYear && ui->numberWood;
}



// One-shot gate diagnostic — a silently-skipping gate is indistinguishable from
// a zeroed dialog (same lesson as TownHallSA_Diag).
static void DiagOnce(const StatusBarUI* self) {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;
    const GameUpdateLoop* game = GameUpdateLoop::_singleton;
    const CitizenBehavior* town = game ? game->m_citizenList : nullptr;
    fprintf(stderr, "[StatusBarUI] SA first OnUpdate: self=%p bound=%d built=%d ready=%d "
            "town=%d citizens=%d stock=%p\n",
            static_cast<const void*>(self), self ? (Bound(self) ? 1 : 0) : -1,
            self ? (Built(self) ? 1 : 0) : -1,
            Ready() ? 1 : 0, TownReady() ? 1 : 0,
            town ? town->citizenCount : -1,
            town ? static_cast<const void*>(town->professionResources) : nullptr);
    fflush(stderr);
}

// Delayed one-shot stock dump — fires on the ~10th throttled refresh (~5s in),
// after CitizenBehavior_Update's CountResources has re-tallied the stock,
// so the log shows the values the dialog is actually displaying.
static void DiagStockOnce(void) {
    static int s_calls = 0;
    if (s_calls < 0 || ++s_calls != 10) return;
    s_calls = -1;
    const GameUpdateLoop* game = GameUpdateLoop::_singleton;
    const CitizenBehavior* town = game ? game->m_citizenList : nullptr;
    const ProfessionResourceEntry* stock = town ? town->professionResources : nullptr;
    if (!stock) return;
    fprintf(stderr, "[StatusBarUI] SA stock slots (cur@+0):");
    for (int i = 0; i < 0x15; i++)
        fprintf(stderr, " %d:%d", i, stock[i].currentCount);
    fprintf(stderr, "\n");
    fflush(stderr);
}

}  // namespace statusbarui_sa

// ============================================================
// StatusBarUI_Draw (0x14007a3d0, vtable +0x28):
// Element initialization — finds all named UI elements
// ============================================================

// [5] 0x14007a3d0
void StatusBarUI::OnOpened() {
    if (!statusbarui_sa::Bound(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;
    if (statusbarui_sa::Built(this)) return;   // idempotence

    // --- Date, population, temperature ---
    textSeason        = FindNamed<UI::TextElement>(this, "textSeason");
    textPopulation    = FindNamed<UI::TextElement>(this, "textPopulation");
    numberYear        = FindNamed<UI::NumberElement>(this, "numberYear");
    numberTemperature = FindNamed<UI::NumberElement>(this, "numberTemperature");

    // --- Resource counts ---
    numberWood     = FindNamed<UI::NumberElement>(this, "numberWood");
    numberStone    = FindNamed<UI::NumberElement>(this, "numberStone");
    numberIron     = FindNamed<UI::NumberElement>(this, "numberIron");
    numberFireWood = FindNamed<UI::NumberElement>(this, "numberFireWood");
    numberTools    = FindNamed<UI::NumberElement>(this, "numberTools");
    numberFood     = FindNamed<UI::NumberElement>(this, "numberFood");
    numberHerbs    = FindNamed<UI::NumberElement>(this, "numberHerbs");
    numberClothes  = FindNamed<UI::NumberElement>(this, "numberClothes");
    numberAlcohol  = FindNamed<UI::NumberElement>(this, "numberAlcohol");
    numberCoal     = FindNamed<UI::NumberElement>(this, "numberCoal");
    numberTextile  = FindNamed<UI::NumberElement>(this, "numberTextile");

    // --- Custom resource counters (numberCustom0..9) ---
    for (int i = 0; i < 10; i++) {
        System::String fmtStr = System::String::Format("numberCustom%i", (uint64_t)i);
        numberCustom[i] = FindNamed<UI::NumberElement>(this, fmtStr);
    }

    // --- Status icons (imageStatus*) ---
    imageStatusHome    = FindNamed<UI::Element>(this, "imageStatusHome");
    imageStatusFood    = FindNamed<UI::Element>(this, "imageStatusFood");
    imageStatusCold    = FindNamed<UI::Element>(this, "imageStatusCold");
    imageStatusJob     = FindNamed<UI::Element>(this, "imageStatusJob");
    imageStatusTool    = FindNamed<UI::Element>(this, "imageStatusTool");
    imageStatusDisease = FindNamed<UI::Element>(this, "imageStatusDisease");

    // Set element IDs 100-105 and hide initially (flag 0x10000 off)
    UI::Element* const statusIcons[6] = { imageStatusHome, imageStatusFood, imageStatusCold,
                                          imageStatusJob, imageStatusTool, imageStatusDisease };
    for (int i = 0; i < 6; i++) {
        statusIcons[i]->SetElementId(100 + i);
        statusIcons[i]->SetFlagsOnOff(0x10000, 0);
    }

    // --- Status numbers (numberStatus*) ---
    numberStatusHome    = FindNamed<UI::TextElement>(this, "numberStatusHome");
    numberStatusFood    = FindNamed<UI::TextElement>(this, "numberStatusFood");
    numberStatusCold    = FindNamed<UI::TextElement>(this, "numberStatusCold");
    numberStatusJob     = FindNamed<UI::TextElement>(this, "numberStatusJob");
    numberStatusTool    = FindNamed<UI::TextElement>(this, "numberStatusTool");
    numberStatusDisease = FindNamed<UI::TextElement>(this, "numberStatusDisease");

    // --- Weather icons ---
    imageSunny       = FindNamed<UI::Element>(this, "imageSunny");
    imagePartlySunny = FindNamed<UI::Element>(this, "imagePartlySunny");
    imageCloudy      = FindNamed<UI::Element>(this, "imageCloudy");
    imageRain        = FindNamed<UI::Element>(this, "imageRain");
    imageSnow        = FindNamed<UI::Element>(this, "imageSnow");

    // --- Hearts/Stars icon groups ---
    // NOTE: retail type-checks the group against a UI type-info whose decompiled
    // label ("TypeId_HelpButtonElem" = ButtonElement) is wrong for these ribbon
    // groups — with that id the check rejects the element and hearts stay blank.
    // The elements come from our own StatusBar.rsc, so accept the find result
    // directly (CitizenUI's identical groupHearts/groupStars binding does the same).
    // The helper (DLG-a's citizen_icon_helpers.h) takes the group's two Arrays
    // and the InternalPtr to the group element by address.
    {
        Object::InternalPtr<UI::Element> group(FindNamed<UI::Element>(this, "groupHearts"));
        CitizenUI_BuildIconArray_Impl(heartsWhole, heartsHalf, &group);
    }
    {
        Object::InternalPtr<UI::Element> group(FindNamed<UI::Element>(this, "groupStars"));
        CitizenUI_BuildIconArray_Impl(starsWhole, starsHalf, &group);
    }

    // --- Title label ---
    // Same wrong-label situation as the hearts/stars groups above: retail checks
    // against the LabelElement type-info, not ButtonElement — accept directly.
    labelTitleParam = FindNamed<UI::TextElement>(this, "labelTitleParam");

    // --- Set initial title text from map name ---
    System::String mapName(GameUpdateLoop::_singleton->m_saveName);   // FUN_140011f90: the String copy
    if (labelTitleParam)
        labelTitleParam->SetParameter(0, mapName);
}

// ============================================================
// StatusBarUI_Startup (0x1400798d0, vtable +0x38):
// Main HUD update — called every frame
// ============================================================
// Iterates all citizens to count status problems (homeless, hungry,
// cold, jobless, toolless, diseased), accumulates health/happiness,
// updates population text, resource counts, weather icons,
// temperature, date, and status indicators.

// [7] 0x1400798d0
void StatusBarUI::OnUpdateLayout() {
    if (!statusbarui_sa::Bound(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;
    if (!statusbarui_sa::Built(this)) StatusBarUI::OnOpened();
    if (!statusbarui_sa::Built(this)) return;
    GameUpdateLoop* game = GameUpdateLoop::_singleton;

    // The town (+0xF0)
    const CitizenBehavior* town = game->m_citizenList;
    int citizenCount = town->citizenCount;

    // Accumulators matching decompilation locals
    uint32_t statusBitfield = 0;     // local_res10[0]
    uint32_t adultCount = 0;         // local_res18 — age >= adultAge, not at school
    uint32_t studentCount = 0;       // local_res20 — at school
    uint32_t childCount = 0;         // local_d0 — under adultAge, not at school
    float healthSum = 0.0f;          // fVar28
    float happySum = 0.0f;           // fVar29
    uint32_t statusCounts[6] = {0};  // local_b8[0..5]

    // Iterate all citizens
    for (int i = 0; i < citizenCount; i++) {
        const CitizenComponent* citizen = town->citizens[i];

        if (citizen->age < citizen->description->adultAge || citizen->school != nullptr) {
            if (citizen->school == nullptr) {
                childCount++;
            } else {
                studentCount++;
            }
        } else {
            adultCount++;
        }

        // The citizen's StatusIconComponent (retail dereferences it unguarded)
        const StatusIconComponent* icons = citizen->_entity->FindComponent<StatusIconComponent>();

        uint32_t status = icons->_geometry.activeMask;
        statusBitfield |= status;

        // Count individual status bits
        statusCounts[0] += (status & 1);           // home
        statusCounts[1] += ((status >> 1) & 1);    // food
        statusCounts[2] += ((status >> 2) & 1);    // cold
        statusCounts[3] += ((status >> 3) & 1);    // job
        statusCounts[4] += ((status >> 4) & 1);    // tool
        statusCounts[5] += ((status >> 5) & 1);    // disease

        healthSum += citizen->health;
        happySum += citizen->happiness;
    }

    // --- Update hearts (health) icon display ---
    float maxHappy = kMaxHealth;  // default if no citizens
    if (citizenCount == 0) {
        CitizenUI_UpdateIconDisplay_Impl(heartsWhole, heartsHalf, 0.0f, kMaxHealth);
        happySum = 0.0f;
    } else {
        // The first citizen's description (Component::_description, checked)
        const Description* desc = town->citizens[0]->_description;
        const CitizenDescription* citizenDesc =
            ::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())
                ? static_cast<const CitizenDescription*>(desc) : nullptr;
        CitizenUI_UpdateIconDisplay_Impl(heartsWhole, heartsHalf,
                                        healthSum / (float)citizenCount,
                                        citizenDesc->maxHealth);

        // Retail reads the first citizen's description a second time
        desc = town->citizens[0]->_description;
        citizenDesc = ::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())
                          ? static_cast<const CitizenDescription*>(desc) : nullptr;
        happySum = happySum / (float)citizenCount;
        maxHappy = citizenDesc->maxHappiness;
    }

    // --- Update stars (happiness) icon display ---
    CitizenUI_UpdateIconDisplay_Impl(starsWhole, starsHalf, happySum, maxHappy);

    // --- Update status icon visibility and counts ---
    UI::Element* const statusIcons[6] = { imageStatusHome, imageStatusFood, imageStatusCold,
                                          imageStatusJob, imageStatusTool, imageStatusDisease };
    UI::TextElement* const statusNumbers[6] = { numberStatusHome, numberStatusFood, numberStatusCold,
                                                numberStatusJob, numberStatusTool, numberStatusDisease };
    uint32_t mask = 1;
    for (int k = 0; k < 6; k++) {
        bool hasStatus = (statusBitfield & mask) != 0;
        uint32_t hideFlag = hasStatus ? 0 : 1;
        uint32_t showFlag = hasStatus ? 1 : 0;

        statusIcons[k]->SetFlagsOnOff(hideFlag, showFlag);
        statusNumbers[k]->SetFlagsOnOff(hideFlag, showFlag);

        // Set count text as parameter 0
        System::String countStr = System::String::Format("%i", (uint64_t)statusCounts[k]);
        statusNumbers[k]->SetParameter(0, countStr);

        mask = (mask << 1) | (uint32_t)((int32_t)mask < 0);  // rotate left
    }

    // --- Update population text (3 params: adults, students, children) ---
    {
        System::String str = System::String::Format("%i", (uint64_t)adultCount);
        textPopulation->SetParameter(0, str);
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)studentCount);
        textPopulation->SetParameter(1, str);
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)childCount);
        textPopulation->SetParameter(2, str);
    }

    // --- Update year ---
    const Calendar* calendar = game->m_dayCounter;
    numberYear->SetValue(static_cast<int32_t>(calendar->year));

    // --- Update month/season ---
    float seconds = calendar->yearTime.AsSeconds();
    uint32_t monthIdx = (uint32_t)((seconds / kSecondsPerYear) * kMonthsPerYear);
    if ((int)monthIdx > 11) monthIdx = 0;

    {
        System::String monthKey = System::String::Format("Month%i", (uint64_t)monthIdx);

        // A UI::String of textSeason's own string table and the key. Guard the
        // string table (retail asserts it; a missing table would crash
        // StringTable::GetIndexFromName in standalone).
        if (textSeason && textSeason->_text.table != nullptr) {
            UI::String month(textSeason->_text.GetStringTable(), monthKey);
            textSeason->SetText(&month);
        }
    }

    // --- Update temperature ---
    const WeatherSimulation* weather = game->m_weather;
    float rawTemp = weather->temperature;
    float displayTemp = rawTemp;

    // Celsius mode (GameSettings::showCelsius). Null-guarded: the settings slot
    // can be unresolved in standalone — default to Fahrenheit (setting off)
    // like a fresh install.
    const GameSettings* settings = g_gameSettings;
    if (settings && settings->showCelsius != 0) {
        displayTemp = ((rawTemp - kFreezingF) * kFtoC_Num) / kFtoC_Den;
    }
    numberTemperature->SetValue(static_cast<int32_t>((int)displayTemp));

    // --- Update weather icons ---
    // Weather check uses displayTemp (after conversion), compared to freezing F
    int weatherType = weather->field_0x10;
    if (displayTemp <= kFreezingF && weatherType == 0) {
        weatherType = 4;  // snow
    }

    // rain(0), cloudy(1), partly(2), sunny(3), snow(4)
    UI::Element* const weatherIcons[5] = { imageRain, imageCloudy, imagePartlySunny, imageSunny, imageSnow };
    for (int weatherIdx = 0; weatherIdx < 5; weatherIdx++) {
        weatherIcons[weatherIdx]->SetFlagsOnOff((uint32_t)(weatherIdx != weatherType),
                                                (uint32_t)(weatherIdx == weatherType));
    }

    // --- Update resource counts: the town's stock, one entry per resource ---
    const ProfessionResourceEntry* stock = town->professionResources;

    // Match exact decompilation order: pre-read into locals
    int valFood     = stock[5].currentCount;
    int valFireWood = stock[3].currentCount;
    int valTools    = stock[4].currentCount;
    int valWood     = stock[0].currentCount;
    int valStone    = stock[1].currentCount;
    int valIron     = stock[2].currentCount;
    int valHerbs    = stock[6].currentCount;
    int valClothes  = stock[7].currentCount;
    int valAlcohol  = stock[8].currentCount;
    int valCoal     = stock[9].currentCount;
    int valTextile  = stock[10].currentCount;

    numberWood->SetValue(static_cast<int32_t>(valWood));
    numberStone->SetValue(static_cast<int32_t>(valStone));
    numberIron->SetValue(static_cast<int32_t>(valIron));
    numberTools->SetValue(static_cast<int32_t>(valTools));
    numberFireWood->SetValue(static_cast<int32_t>(valFireWood));
    numberFood->SetValue(static_cast<int32_t>(valFood));
    numberHerbs->SetValue(static_cast<int32_t>(valHerbs));
    numberClothes->SetValue(static_cast<int32_t>(valClothes));
    numberAlcohol->SetValue(static_cast<int32_t>(valAlcohol));
    numberCoal->SetValue(static_cast<int32_t>(valCoal));

    // Textile — may be null
    if (numberTextile != nullptr) {
        numberTextile->SetValue(static_cast<int32_t>(valTextile));
    }

    // Custom resources (numberCustom0..9): stock entries 11..20
    // (retail: lVar25 from 0x160 to <0x2A0 step 0x20, re-reading the stock each time)
    for (int k = 0; k < 10; k++) {
        if (numberCustom[k] != nullptr) {
            int value = town->professionResources[11 + k].currentCount;
            numberCustom[k]->SetValue(static_cast<int32_t>(value));
        }
    }

    // --- Update title text (map name) ---
    System::String mapName(game->m_saveName);   // FUN_140011f90: the String copy
    if (labelTitleParam)
        labelTitleParam->SetParameter(0, mapName);
}

// ============================================================
// StatusBarUI_StatusIconHandler_Impl (was fn_StatusBarUI_StatusIconHandler, FUN_14007a120)
//
// Cycles through citizens to find the next one with the given status problem.
// Uses self->statusCitizenIndex (+0x100) to remember position between clicks.
// On match, navigates camera to the citizen (opens CitizenUI).
//
// iconIndex: 0=home, 1=food, 2=cold, 3=job, 4=tool, 5=disease
// Bitmask: 1 << iconIndex tested against StatusIconComponent's activeMask (+0xF0).
// ============================================================
static void StatusBarUI_StatusIconHandler_Impl(StatusBarUI* self, int iconIndex) {
    GameUpdateLoop* game = GameUpdateLoop::_singleton;
    if (!game) return;

    // Bit 10 of the game flags = locked/paused
    if ((game->m_gameFlags >> 10) & 1) return;

    // The town's citizens
    const CitizenBehavior* town = game->m_citizenList;
    if (!town) return;

    int citizenCount = town->citizenCount;
    if (citizenCount <= 0) return;

    CitizenComponent* const* citizens = town->citizens;
    uint32_t bitMask = 1u << (iconIndex & 0x1F);

    // Increment cycling index
    self->statusCitizenIndex++;
    int checked = 0;

    // Wrap if past end
    if ((int)self->statusCitizenIndex >= citizenCount) {
        self->statusCitizenIndex = 0;
    }

    while (true) {
        int idx = (int)self->statusCitizenIndex;
        if (idx >= citizenCount) {
            self->statusCitizenIndex = 0;
            // Continue outer loop (wrap and keep searching)
        }

        const CitizenComponent* citizen = citizens[self->statusCitizenIndex];
        if (!citizen) goto next;

        {
            // The citizen's StatusIconComponent (null when the entity has none)
            const StatusIconComponent* icons = citizen->_entity->FindComponent<StatusIconComponent>();

            uint32_t statusBits = icons ? icons->_geometry.activeMask : 0;

            if ((statusBits & bitMask) != 0 || checked >= citizenCount) {
                // Found a match (or exhausted all citizens): retail finds the
                // component again and bails when the bit is not set
                ComponentEntity* navEntity = citizens[self->statusCitizenIndex]->_entity;
                const StatusIconComponent* navIcons = navEntity->FindComponent<StatusIconComponent>();
                if (navIcons) {
                    if ((navIcons->_geometry.activeMask & bitMask) == 0) return;
                }

                // Navigate to the citizen entity (0x140103040)
                ComponentEntity* entityRef = citizens[self->statusCitizenIndex]->_entity;
                UI::DialogInstance* dialogRef = self->_dialog;
                game->m_toolManager->CenterOnEntity(&dialogRef, &entityRef);
                return;
            }
        }

next:
        self->statusCitizenIndex++;
        checked++;
        if ((int)self->statusCitizenIndex >= citizenCount) {
            self->statusCitizenIndex = 0;
        }
    }
}

// ============================================================
// StatusBarUI_OnRemove (0x14007a3a0, vtable +0x48):
// Status icon click handler.
//
// Called when an element is activated (eventType == 2 = element click).
// Status icons imageStatusHome..imageStatusDisease have element IDs 100-105
// (set during Draw/Startup initialization).
//
// On each click of icon N, cycles to the NEXT citizen whose StatusIconComponent
// bits include bit N (1 << N), increments self->statusCitizenIndex,
// navigates camera and opens CitizenUI for that citizen.
//
// The actual citizen search + navigation is done by the shared helper at
// 0x14007a120 (fn_StatusBarUI_StatusIconHandler), which:
//   1. Checks game state flag (bails if paused/locked)
//   2. Increments and wraps self->statusCitizenIndex within citizen count
//   3. Iterates citizens starting at that index, finds StatusIconComponent
//   4. Tests status & (1 << iconIndex)
//   5. On match: calls FUN_140103040 (navigate camera, open CitizenUI)
// ============================================================

// [9] 0x14007a3a0
void StatusBarUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t eventType, int32_t eventId,
                                 int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (!statusbarui_sa::Built(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;

    // Only handle click events (eventType == 2)
    if (eventType != 2) return;

    // Status icons have element IDs 100-105 (set in Draw via fn_Element_SetElementId)
    // Icon 0 = home (bit 0), icon 1 = food (bit 1), ..., icon 5 = disease (bit 5)
    int iconIndex = eventId - 100;
    if (iconIndex < 0 || iconIndex >= 6) return;

    StatusBarUI_StatusIconHandler_Impl(this, iconIndex);
}

// ============================================================================
// STANDALONE promotion — real SA vtable for StatusBarUI.
// ============================================================================
// The toolbar "town status" utility dialog (Template/UtilityStatusBar.rsc ->
// StandardDialog.rsc:resourceUserTitle + Dialog/StatusBar.rsc:content, toolbar
// group 4 — same submenu as the Map dialog) is driven by a StatusBarUI
// controller. In standalone this type was a UI_DIALOG_STUB (all slots no-op,
// registration REMOVED from ui_dialog_stub_factories.cpp), so the dialog opened
// with every field on its .rsc default: title untitled, all statistics zero.
//
// Retail vtable (exe @ 0x14015d810):
//   idx5 (+0x28) = 0x14007a3d0  element discovery      -> StatusBarUI_Draw
//   idx7 (+0x38) = 0x1400798d0  full stats refresh      -> StatusBarUI_Startup
//   idx9 (+0x48) = 0x14007a3a0  status-icon click       -> StatusBarUI_OnRemove
//
// The utility DialogInstance uses the same ToolUI child-dialog vtable as the
// entity-info dialogs (toolbardialog.cpp Toolbar_Utility_Impl), whose SA
// framework dispatches ONLY controller slot idx8 (+0x40) per frame — so, like
// the TownHallUI promotion, the self-heal build + a throttled refresh run from
// idx8, with idx4/idx5/idx7 also wired for any direct framework dispatch.
//
// Data sources (all live after the town-registry passes in starttown.cpp):
// town citizen list townMgr+0xa0/+0xa8 (population/status/hearts/stars),
// resource stock townMgr+0x170, game clock gw+0x128, weather gw+0x130, and
// the town name String at gw+0x28 (title parameter).


// [4] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void StatusBarUI::OnCreated() {
    StatusBarUI::OnOpened();
}



// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void StatusBarUI::OnUpdate(const System::Time& /*time*/) {
    statusbarui_sa::DiagOnce(this);
    if (!statusbarui_sa::Bound(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;
    if (!statusbarui_sa::Built(this)) {
        StatusBarUI::OnOpened();
        if (statusbarui_sa::Built(this)) {
            fprintf(stderr, "[StatusBarUI] SA build OK — elements bound, first refresh\n");
            fflush(stderr);
        } else {
            return;
        }
        StatusBarUI::OnUpdateLayout();
        return;
    }
    // +0x104 (a gap in retail) as a throttle counter.
    if (refreshThrottle++ % 30 == 0) {
        StatusBarUI::OnUpdateLayout();
        statusbarui_sa::DiagStockOnce();
    }
}


// create 0x1400140e0: MemAlloc(0x1d0, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* StatusBarUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatusBarUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StatusBarUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* StatusBarUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StatusBarUI());
}

DEFINE_EXE_TYPE(StatusBarUI, "UI::DialogController")

