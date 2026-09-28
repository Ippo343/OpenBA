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
#include "Entity/exe_bodies.h"
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
// Helper: find element by name via parent dialog
// ============================================================
static void* FindElement(void* dialogController, const char* name) {
    uintptr_t s = (uintptr_t)dialogController;
    System::String nameStr(name);

    void* dialog = *(void**)(s + 0x18);

    uint8_t retBuf[16];
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge
    void* elem = *(void**)retBuf;
    return elem;
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

// The world-side data Startup reads unguarded: townMgr (gw+0xf0) with its
// resource stock block (+0x170), the game clock (gw+0x128), and the weather
// object (gw+0x130). The citizen roster (+0xa0/+0xa8) may legitimately be
// count 0 (handled by the impl).
bool TownReady() {
    if (!(GameUpdateLoop::_singleton != nullptr)) return false;
    uintptr_t gw = ((uintptr_t)GameUpdateLoop::_singleton);
    uintptr_t townMgr = *(uintptr_t*)(gw + 0xf0);
    if (!townMgr) return false;
    if (*(uintptr_t*)(townMgr + 0x170) == 0) return false;  // resource stock
    if (*(uintptr_t*)(gw + 0x128) == 0) return false;       // game clock
    if (*(uintptr_t*)(gw + 0x130) == 0) return false;       // weather
    return true;
}

// Controller bound to its element + dialog instance (framework sets +0x18/+0x20).
inline bool Bound(void* self) {
    uintptr_t s = (uintptr_t)self;
    return *(void**)(s + 0x18) != nullptr && *(void**)(s + 0x20) != nullptr;
}

// Draw bound the core elements (they all exist in Dialog/StatusBar.rsc:content).
inline bool Built(void* self) {
    StatusBarUI* ui = (StatusBarUI*)self;
    return ui->textSeason && ui->textPopulation && ui->numberYear && ui->numberWood;
}



// One-shot gate diagnostic — a silently-skipping gate is indistinguishable from
// a zeroed dialog (same lesson as TownHallSA_Diag).
static void DiagOnce(void* self) {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;
    uintptr_t gw = (GameUpdateLoop::_singleton != nullptr)
                 ? ((uintptr_t)GameUpdateLoop::_singleton) : 0;
    uintptr_t townMgr = gw ? *(uintptr_t*)(gw + 0xf0) : 0;
    fprintf(stderr, "[StatusBarUI] SA first OnUpdate: self=%p bound=%d built=%d ready=%d "
            "town=%d citizens=%d stock=%p\n",
            self, self ? (Bound(self) ? 1 : 0) : -1, self ? (Built(self) ? 1 : 0) : -1,
            Ready() ? 1 : 0, TownReady() ? 1 : 0,
            townMgr ? *(int*)(townMgr + 0xa8) : -1,
            townMgr ? *(void**)(townMgr + 0x170) : nullptr);
    fflush(stderr);
}

// Delayed one-shot stock dump — fires on the ~10th throttled refresh (~5s in),
// after CitizenBehavior_Update's CountResources has re-tallied townMgr+0x170,
// so the log shows the values the dialog is actually displaying.
static void DiagStockOnce(void) {
    static int s_calls = 0;
    if (s_calls < 0 || ++s_calls != 10) return;
    s_calls = -1;
    uintptr_t gw = (GameUpdateLoop::_singleton != nullptr)
                 ? ((uintptr_t)GameUpdateLoop::_singleton) : 0;
    uintptr_t townMgr = gw ? *(uintptr_t*)(gw + 0xf0) : 0;
    uintptr_t stock = townMgr ? *(uintptr_t*)(townMgr + 0x170) : 0;
    if (!stock) return;
    fprintf(stderr, "[StatusBarUI] SA stock slots (cur@+0):");
    for (int i = 0; i < 0x15; i++)
        fprintf(stderr, " %d:%d", i, *(int*)(stock + (uintptr_t)i * 0x20));
    fprintf(stderr, "\n");
    fflush(stderr);
}



// Destructor: the only owned heap is the two GroupRating icon arrays built by
// CitizenUI_BuildIconArray_Impl (whole+half InternalBasePtr[] each, at +0x108
// and +0x128).
}  // namespace statusbarui_sa

// ============================================================
// StatusBarUI_Draw (0x14007a3d0, vtable +0x28):
// Element initialization — finds all named UI elements
// ============================================================

// [5] 0x14007a3d0
void StatusBarUI::OnOpened() {
    if (!statusbarui_sa::Bound(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;
    if (statusbarui_sa::Built(this)) return;   // idempotence
    uintptr_t s = (uintptr_t)this;

    // --- Date, population, temperature ---
    this->textSeason        = FindElement(this, "textSeason");
    this->textPopulation    = FindElement(this, "textPopulation");
    this->numberYear        = FindElement(this, "numberYear");
    this->numberTemperature = FindElement(this, "numberTemperature");

    // --- Resource counts ---
    this->numberWood     = FindElement(this, "numberWood");
    this->numberStone    = FindElement(this, "numberStone");
    this->numberIron     = FindElement(this, "numberIron");
    this->numberFireWood = FindElement(this, "numberFireWood");
    this->numberTools    = FindElement(this, "numberTools");
    this->numberFood     = FindElement(this, "numberFood");
    this->numberHerbs    = FindElement(this, "numberHerbs");
    this->numberClothes  = FindElement(this, "numberClothes");
    this->numberAlcohol  = FindElement(this, "numberAlcohol");
    this->numberCoal     = FindElement(this, "numberCoal");
    this->numberTextile  = FindElement(this, "numberTextile");

    // --- Custom resource counters (numberCustom0..9) ---
    for (int i = 0; i < 10; i++) {
        System::String fmtStr = System::String::Format("numberCustom%i", (uint64_t)i);

        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(fmtStr));  // B.3.6 bridge
        this->numberCustom[i] = *(void**)retBuf;
    }

    // --- Status icons (imageStatus*) ---
    this->imageStatusHome    = FindElement(this, "imageStatusHome");
    this->imageStatusFood    = FindElement(this, "imageStatusFood");
    this->imageStatusCold    = FindElement(this, "imageStatusCold");
    this->imageStatusJob     = FindElement(this, "imageStatusJob");
    this->imageStatusTool    = FindElement(this, "imageStatusTool");
    this->imageStatusDisease = FindElement(this, "imageStatusDisease");

    // Set element IDs 100-105 and hide initially (flag 0x10000 off)
    void** statusIcons = &this->imageStatusHome;
    for (int i = 0; i < 6; i++) {
        reinterpret_cast<UI::Element*>(statusIcons[i])->SetElementId(100 + i);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(statusIcons[i])->SetFlagsOnOff(0x10000, 0);  // B.3.6 bridge
    }

    // --- Status numbers (numberStatus*) ---
    this->numberStatusHome    = FindElement(this, "numberStatusHome");
    this->numberStatusFood    = FindElement(this, "numberStatusFood");
    this->numberStatusCold    = FindElement(this, "numberStatusCold");
    this->numberStatusJob     = FindElement(this, "numberStatusJob");
    this->numberStatusTool    = FindElement(this, "numberStatusTool");
    this->numberStatusDisease = FindElement(this, "numberStatusDisease");

    // --- Weather icons ---
    this->imageSunny       = FindElement(this, "imageSunny");
    this->imagePartlySunny = FindElement(this, "imagePartlySunny");
    this->imageCloudy      = FindElement(this, "imageCloudy");
    this->imageRain        = FindElement(this, "imageRain");
    this->imageSnow        = FindElement(this, "imageSnow");

    // --- Hearts/Stars icon groups ---
    {
        System::String nameStr("groupHearts");
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge

        // NOTE: retail type-checks the group against a UI type-info whose decompiled
        // label ("TypeId_HelpButtonElem" = ButtonElement) is wrong for these ribbon
        // groups — with that id the check rejects the element and hearts stay blank.
        // The elements come from our own StatusBar.rsc, so accept the find result
        // directly (CitizenUI's identical groupHearts/groupStars binding does the same).
        void* typedGroup = *(void**)retBuf;
        uint8_t groupPtr[16];
        new (groupPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(typedGroup));   // B.3.3 bridge
        CitizenUI_BuildIconArray_Impl(s + 0x108, (uintptr_t)groupPtr);
    }
    {
        System::String nameStr("groupStars");
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge

        void* typedGroup = *(void**)retBuf;
        uint8_t groupPtr[16];
        new (groupPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(typedGroup));   // B.3.3 bridge
        CitizenUI_BuildIconArray_Impl(s + 0x128, (uintptr_t)groupPtr);
    }

    // --- Title label ---
    {
        System::String nameStr("labelTitleParam");
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nameStr));  // B.3.6 bridge

        // Same wrong-label situation as the hearts/stars groups above: retail checks
        // against the LabelElement type-info, not ButtonElement — accept directly.
        uint8_t labelPtr[16];
        new (labelPtr) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(*(void**)retBuf));   // B.3.3 bridge
        this->labelTitleParam = *(void**)labelPtr;
    }

    // --- Set initial title text from map name ---
    void* gameWorld = GameUpdateLoop::_singleton;
    System::String mapName(*reinterpret_cast<const System::String*>((uintptr_t)gameWorld + 0x28));  // B.3.1 bridge; FUN_140011f90: copies gameWorld+0x28 String
    if (this->labelTitleParam)
        reinterpret_cast<UI::TextElement*>(this->labelTitleParam)->SetParameter(0, mapName);  // B.3.6 bridge
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
    uintptr_t s = (uintptr_t)this;
    void* gameWorld = GameUpdateLoop::_singleton;

    // Get population list from game world (+0xF0)
    uintptr_t popList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    int citizenCount = *(int*)(popList + 0xA8);

    // Accumulators matching decompilation locals
    uint32_t statusBitfield = 0;     // local_res10[0]
    uint32_t adultCount = 0;         // local_res18 — healthy adults
    uint32_t dyingCount = 0;         // local_res20 — has death status
    uint32_t buildingCount = 0;      // local_d0 — health declining but not dead
    float healthSum = 0.0f;          // fVar28
    float happySum = 0.0f;           // fVar29
    uint32_t statusCounts[6] = {0};  // local_b8[0..5]

    // Iterate all citizens
    for (int i = 0; i < citizenCount; i++) {
        uintptr_t citizen = *(uintptr_t*)(*(uintptr_t*)(popList + 0xA0) + (uintptr_t)i * 8);

        // Check citizen state: +0x3C = current health
        // *(+0x288) + 0x44 = max health from description
        // +0x208 = death/special status pointer
        float currentHealth = *(float*)(citizen + 0x3C);
        float maxHealth = *(float*)(*(uintptr_t*)(citizen + 0x288) + 0x44);
        uintptr_t deathStatus = *(uintptr_t*)(citizen + 0x208);

        if (currentHealth < maxHealth || deathStatus != 0) {
            if (deathStatus == 0) {
                buildingCount++;  // health declining but alive
            } else {
                dyingCount++;    // has death/special status
            }
        } else {
            adultCount++;        // healthy
        }

        // Find HappinessComponent on this citizen's entity
        uintptr_t entity = *(uintptr_t*)(citizen + 0x10);

        int compCount = *(int*)(entity + 0x38);
        void* happyComp = nullptr;
        for (int j = 0; j < compCount; j++) {
            void** compArray = *(void***)(entity + 0x30);
            void* comp = compArray[j];

            // Check component type via vtable GetTypeId (vtable +0x10)
            int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

            if (typeId == (int)StatusIconComponent::TypeId()) {
                void* pComp = compArray[j];
                bool isDerived = ::Object::IsDerivedFromNullSafe(pComp, StatusIconComponent::TypeId());
                happyComp = nullptr;
                if (isDerived) {
                    happyComp = pComp;
                }
                goto found_happiness;
            }
        }
        happyComp = nullptr;
found_happiness:

        {
            uint8_t happyPtr[16];
            new (happyPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(happyComp));   // B.3.3 bridge
            uintptr_t hc = *(uintptr_t*)happyPtr;

            // Read status bitfield from HappinessComponent+0xF0
            uint32_t status = *(uint32_t*)(hc + 0xF0);
            statusBitfield |= status;

            // Count individual status bits
            statusCounts[0] += (status & 1);           // home
            statusCounts[1] += ((status >> 1) & 1);    // food
            statusCounts[2] += ((status >> 2) & 1);    // cold
            statusCounts[3] += ((status >> 3) & 1);    // job
            statusCounts[4] += ((status >> 4) & 1);    // tool
            statusCounts[5] += ((status >> 5) & 1);    // disease
        }

        // Accumulate health (+0x50) and happiness (+0x80)
        healthSum += *(float*)(citizen + 0x50);
        happySum += *(float*)(citizen + 0x80);
    }

    // --- Update hearts (health) icon display ---
    float maxHappy = kMaxHealth;  // default if no citizens
    if (citizenCount == 0) {
        CitizenUI_UpdateIconDisplay_Impl(s + 0x108, 0.0f, kMaxHealth);
        happySum = 0.0f;
    } else {
        // Get max health from first citizen's description
        uintptr_t firstCitizen = *(uintptr_t*)(*(uintptr_t*)(popList + 0xA0));
        void* desc = *(void**)(firstCitizen + 0x18);
        void* citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())) {
            citizenDesc = desc;
        }
        CitizenUI_UpdateIconDisplay_Impl(s + 0x108,
                                        healthSum / (float)citizenCount,
                                        *(float*)((uintptr_t)citizenDesc + 0x70));

        // Get max happiness from first citizen's description
        firstCitizen = *(uintptr_t*)(*(uintptr_t*)(popList + 0xA0));
        desc = *(void**)(firstCitizen + 0x18);
        citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())) {
            citizenDesc = desc;
        }
        happySum = happySum / (float)citizenCount;
        maxHappy = *(float*)((uintptr_t)citizenDesc + 0xA4);
    }

    // --- Update stars (happiness) icon display ---
    CitizenUI_UpdateIconDisplay_Impl(s + 0x128, happySum, maxHappy);

    // --- Update status icon visibility and counts ---
    // Original iterates from numberStatus (+0x178), uses offset -0x30 for imageStatus
    uint32_t mask = 1;
    uintptr_t pNum = s + 0x178;   // numberStatusHome
    int loopCount = 6;
    uint32_t* pCounts = statusCounts;
    do {
        bool hasStatus = (statusBitfield & mask) != 0;
        uint32_t hideFlag = hasStatus ? 0 : 1;
        uint32_t showFlag = hasStatus ? 1 : 0;

        // imageStatus is at pNum - 0x30 (6 pointers before numberStatus)
        reinterpret_cast<UI::Element*>(*(void**)(pNum - 0x30))->SetFlagsOnOff(hideFlag, showFlag);  // B.3.6 bridge

        reinterpret_cast<UI::Element*>(*(void**)pNum)->SetFlagsOnOff(hideFlag, showFlag);  // B.3.6 bridge

        // Set count text as parameter 0
        System::String countStr = System::String::Format("%i", (uint64_t)*pCounts);
        reinterpret_cast<UI::TextElement*>(*(void**)pNum)->SetParameter(0, countStr);  // B.3.6 bridge

        mask = (mask << 1) | (uint32_t)((int32_t)mask < 0);  // rotate left
        pNum += 8;
        pCounts++;
        loopCount--;
    } while (loopCount != 0);

    // --- Update population text (3 params: adults, dying, building) ---
    {
        System::String str = System::String::Format("%i", (uint64_t)adultCount);
        reinterpret_cast<UI::TextElement*>(this->textPopulation)->SetParameter(0, str);  // B.3.6 bridge
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)dyingCount);
        reinterpret_cast<UI::TextElement*>(this->textPopulation)->SetParameter(1, str);  // B.3.6 bridge
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)buildingCount);
        reinterpret_cast<UI::TextElement*>(this->textPopulation)->SetParameter(2, str);  // B.3.6 bridge
    }

    // --- Update year ---
    int* timeData = *(int**)((uintptr_t)gameWorld + 0x128);
    int year = *timeData;
    reinterpret_cast<UI::NumberElement*>(this->numberYear)->SetValue(static_cast<int32_t>(year));  // B.3.6 bridge

    // --- Update month/season ---
    float seconds = reinterpret_cast<const System::Time*>((void*)(timeData + 2))->AsSeconds();   // B.3.4 bridge
    uint32_t monthIdx = (uint32_t)((seconds / kSecondsPerYear) * kMonthsPerYear);
    if ((int)monthIdx > 11) monthIdx = 0;

    {
        System::String monthKey = System::String::Format("Month%i", (uint64_t)monthIdx);

        void* seasonElem = this->textSeason;

        // UIString ctor takes two params from the text element: +0xD8 and the formatted
        // key. Guard the string table (retail asserts it; a missing table would crash
        // StringTable::GetIndexFromName in standalone).
        if (seasonElem && *(uint64_t*)((uintptr_t)seasonElem + 0xD8) != 0) {
            uint8_t uiStr[16];
            uint64_t* pResult = reinterpret_cast<uint64_t*>(::new (uiStr) UI::String(  // B.3.6 bridge
                                  Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(
                                      *(uint64_t*)((uintptr_t)seasonElem + 0xD8))),
                                  monthKey));
            // Copy UIString result to local for SetText (16 bytes: text + string table ref)
            uint64_t localUIStr[2];
            localUIStr[0] = pResult[0];
            localUIStr[1] = pResult[1];

            reinterpret_cast<UI::TextElement*>(this->textSeason)->SetText(reinterpret_cast<const UI::String*>(localUIStr));  // B.3.6 bridge
        }
    }

    // --- Update temperature ---
    uintptr_t weatherData = *(uintptr_t*)((uintptr_t)gameWorld + 0x130);
    float rawTemp = *(float*)(weatherData + 0x18);
    float displayTemp = rawTemp;

    // Check if Celsius mode (game settings byte +0x15 = showCelsius, see
    // optionsdialog.cpp). Null-guarded: the settings slot can be unresolved in
    // standalone — default to Fahrenheit (setting off) like a fresh install.
    void* settings = g_gameSettings;
    if (settings && *(uint8_t*)((uintptr_t)settings + 0x15) != 0) {
        displayTemp = ((rawTemp - kFreezingF) * kFtoC_Num) / kFtoC_Den;
    }
    reinterpret_cast<UI::NumberElement*>(this->numberTemperature)->SetValue(static_cast<int32_t>((int)displayTemp));  // B.3.6 bridge

    // --- Update weather icons ---
    // Weather check uses displayTemp (after conversion), compared to freezing F
    int weatherType = *(int*)(weatherData + 0x10);
    if (displayTemp <= kFreezingF && weatherType == 0) {
        weatherType = 4;  // snow
    }

    // Weather icons at +0x1A8..+0x1C8: rain(0), cloudy(1), partly(2), sunny(3), snow(4)
    uintptr_t pWeather = s + 0x1A8;
    int weatherCount = 5;
    int weatherIdx = 0;
    do {
        reinterpret_cast<UI::Element*>(*(void**)pWeather)->SetFlagsOnOff((uint32_t)(weatherIdx != weatherType), (uint32_t)(weatherIdx == weatherType));  // B.3.6 bridge
        weatherIdx++;
        pWeather += 8;
        weatherCount--;
    } while (weatherCount != 0);

    // --- Update resource counts ---
    // piVar5 = *(int**)(popList + 0x170)
    // int* array with stride 8 ints (0x20 bytes) per resource type
    int* piVar5 = *(int**)(popList + 0x170);

    // Match exact decompilation order: pre-read into locals
    int iVar23_food = piVar5[0x28];
    int iVar21_fw   = piVar5[0x18];
    int iVar1_tool  = piVar5[0x20];
    int iVar7_wood  = piVar5[0];
    int iVar2_stone = piVar5[8];
    int iVar3_iron  = piVar5[0x10];
    int valHerbs    = piVar5[0x30];
    int valClothes  = piVar5[0x38];
    int valAlcohol  = piVar5[0x40];
    int valCoal     = piVar5[0x48];
    int valTextile  = piVar5[0x50];

    // Assignments match decompilation: SetValue(*(param_1 + offset), value)
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x58))->SetValue(static_cast<int32_t>(iVar7_wood));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x60))->SetValue(static_cast<int32_t>(iVar2_stone));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x68))->SetValue(static_cast<int32_t>(iVar3_iron));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x78))->SetValue(static_cast<int32_t>(iVar1_tool));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x70))->SetValue(static_cast<int32_t>(iVar21_fw));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x80))->SetValue(static_cast<int32_t>(iVar23_food));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x88))->SetValue(static_cast<int32_t>(valHerbs));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x90))->SetValue(static_cast<int32_t>(valClothes));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0x98))->SetValue(static_cast<int32_t>(valAlcohol));  // B.3.6 bridge
    reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0xA0))->SetValue(static_cast<int32_t>(valCoal));  // B.3.6 bridge

    // Textile (numberTextile at +0xA8) — may be null
    if (*(void**)(s + 0xA8) != nullptr) {
        reinterpret_cast<UI::NumberElement*>(*(void**)(s + 0xA8))->SetValue(static_cast<int32_t>(valTextile));  // B.3.6 bridge
    }

    // Custom resources (numberCustom0..9 at +0xB0..+0xF8)
    // Original: lVar25 from 0x160 to <0x2A0 step 0x20
    uintptr_t resOffset = 0x160;
    uintptr_t pCustom = s + 0xB0;
    do {
        if (*(void**)pCustom != nullptr) {
            int value = *(int*)(resOffset + *(uintptr_t*)(popList + 0x170));
            reinterpret_cast<UI::NumberElement*>(*(void**)pCustom)->SetValue(static_cast<int32_t>(value));  // B.3.6 bridge
        }
        resOffset += 0x20;
        pCustom += 8;
    } while (resOffset < 0x2A0);

    // --- Update title text (map name) ---
    System::String mapName(*reinterpret_cast<const System::String*>((uintptr_t)gameWorld + 0x28));  // B.3.1 bridge; FUN_140011f90: copies gameWorld+0x28 String
    if (this->labelTitleParam)
        reinterpret_cast<UI::TextElement*>(this->labelTitleParam)->SetParameter(0, mapName);  // B.3.6 bridge
}

// ============================================================
// StatusBarUI_StatusIconHandler_Impl (was fn_StatusBarUI_StatusIconHandler, FUN_14007a120)
//
// Cycles through citizens to find the next one with the given status problem.
// Uses self->statusCitizenIndex (+0x100) to remember position between clicks.
// On match, navigates camera to the citizen (opens CitizenUI).
//
// iconIndex: 0=home, 1=food, 2=cold, 3=job, 4=tool, 5=disease
// Bitmask: 1 << iconIndex tested against HappinessComponent status bits (+0xF0).
// ============================================================
static void StatusBarUI_StatusIconHandler_Impl(StatusBarUI* self, int iconIndex) {
    void* gameWorld = GameUpdateLoop::_singleton;
    if (!gameWorld) return;

    // Check game state flag: bit 10 of flags at game+0xD0 = locked/paused
    uint32_t gameFlags = *(uint32_t*)((uintptr_t)gameWorld + 0xD0);
    if ((gameFlags >> 10) & 1) return;

    // Get citizen list entity from game world +0xF0
    uintptr_t citizenList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    if (!citizenList) return;

    // Citizen count at citizenList+0xA8, array at citizenList+0xA0
    int citizenCount = *(int*)(citizenList + 0xA8);
    if (citizenCount <= 0) return;

    uintptr_t citizenArray = *(uintptr_t*)(citizenList + 0xA0);
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

        // Get citizen entity ptr at citizenArray[idx]
        uintptr_t entityPtr = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);
        if (!entityPtr) goto next;

        // CheckPtr on entity (InternalBasePtr validation)

        {
            // Get component entity at entity+0x10
            uintptr_t compEntity = *(uintptr_t*)(entityPtr + 0x10);

            // Search components for HappinessComponent (DAT_1401c7e08)
            void** compArray = *(void***)(compEntity + 0x30);
            int compCount = *(int*)(compEntity + 0x38);
            void* happinessComp = nullptr;

            for (int i = 0; i < compCount; i++) {
                void* comp = compArray[i];
                if (!comp) continue;

                // vtable slot +0x10 (index 2) returns type ID
                int compTypeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

                if ((uint32_t)compTypeId == StatusIconComponent::TypeId()) {
                    // Confirm with IsDerivedFrom
                    if (::Object::IsDerivedFromNullSafe(comp, StatusIconComponent::TypeId())) {
                        happinessComp = comp;
                    }
                    break;
                }
            }

            // If no HappinessComponent found, store null
            uintptr_t happinessPtr = (uintptr_t)happinessComp;
            new (&happinessPtr) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(happinessComp));   // B.3.3 bridge

            // Check status bits at HappinessComponent+0xF0
            uint32_t statusBits = happinessPtr ? *(uint32_t*)(happinessPtr + 0xF0) : 0;

            if ((statusBits & bitMask) != 0 || checked >= citizenCount) {
                // Found a match (or exhausted all citizens)
                // Re-fetch entity for navigation
                uintptr_t navEntity = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);

                uintptr_t navCompEntity = *(uintptr_t*)(navEntity + 0x10);

                // FindComponent_MeshInstance (uses same type ID DAT_1401c7e08).
                // Null-guarded: the exe finder may be unregistered in standalone —
                // skip the redundant double-check in that case (the loop above
                // already verified the status bit).
                if (fn_FindComponent_MeshInstance) {
                    uintptr_t meshComp = 0;
                    fn_FindComponent_MeshInstance((void*)navCompEntity, &meshComp);

                    // Double-check: if meshComp's status doesn't have our bit, bail
                    if (meshComp) {
                        uint32_t meshStatus = *(uint32_t*)(meshComp + 0xF0);
                        if ((meshStatus & bitMask) == 0) return;
                    }
                }

                // Navigate to the citizen entity
                // FUN_140103040(game+0x120, &dialogRef, &entityRef)
                uintptr_t navEntity2 = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);
                uintptr_t entityRef = *(uintptr_t*)(navEntity2 + 0x10);

                uintptr_t dialogRef = *(uintptr_t*)((uintptr_t)self + 0x18);
                void* entityController = *(void**)((uintptr_t)gameWorld + 0x120);

                Toolbar_CenterOnEntity_Impl(entityController, &dialogRef, &entityRef);
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
// On each click of icon N, cycles to the NEXT citizen whose HappinessComponent
// status bits include bit N (1 << N), increments self->statusCitizenIndex,
// navigates camera and opens CitizenUI for that citizen.
//
// The actual citizen search + navigation is done by the shared helper at
// 0x14007a120 (fn_StatusBarUI_StatusIconHandler), which:
//   1. Checks game state flag (bails if paused/locked)
//   2. Increments and wraps self->statusCitizenIndex within citizen count
//   3. Iterates citizens starting at that index, finds HappinessComponent
//   4. Tests status & (1 << iconIndex)
//   5. On match: calls FUN_140103040 (navigate camera, open CitizenUI)
// ============================================================

// [9] 0x14007a3a0
void StatusBarUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t eventType, int32_t eventId,
                                 int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    void* param2 = element;
    if (!statusbarui_sa::Built(this) || !statusbarui_sa::Ready() || !statusbarui_sa::TownReady()) return;
    (void)param2;

    // Only handle click events (eventType == 2)
    if (eventType != 2) return;

    // Status icons have element IDs 100-105 (set in Draw via fn_Element_SetElementId)
    // Icon 0 = home (bit 0), icon 1 = food (bit 1), ..., icon 5 = disease (bit 5)
    int iconIndex = eventId - 100;
    if (iconIndex < 0 || iconIndex >= 6) return;

    // Inline reimplementation of FUN_14007a120 (fn_StatusBarUI_StatusIconHandler).
    // Cycles through citizens starting at self->statusCitizenIndex, finds the next
    // citizen whose HappinessComponent status bits include (1 << iconIndex), then
    // navigates the camera to that citizen and opens CitizenUI.
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


// ~StatusBarUI: the tree's own teardown (its hand-built slot 0 ran this, then freed
// WITHOUT ~DialogController); moved here unchanged. The compiler now chains
// ~DialogController -> ~Internal after it.
StatusBarUI::~StatusBarUI() {
    void* self = this;
    uintptr_t s = (uintptr_t)self;
    {
        static const uintptr_t kIconArrays[4] = { 0x108, 0x118, 0x128, 0x138 };
        for (uintptr_t off : kIconArrays) {
            System::MemoryAllocator::MemFree(*(void**)(s + off));
            *(uint64_t*)(s + off) = 0;
            *(uint64_t*)(s + off + 8) = 0;   // count/capacity pair
        }
    }
}

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
    // _pad104 (struct tail of the +0x100 cycling index) as a throttle counter.
    uint32_t* throttle = (uint32_t*)((uintptr_t)this + 0x104);
    if ((*throttle)++ % 30 == 0) {
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

