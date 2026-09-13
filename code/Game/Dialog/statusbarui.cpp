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

#include "Dialog/ui_panels.h"
#include "Dialog/citizen_icon_helpers.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>

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

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
    void* dialog = *(void**)(s + 0x18);

    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(dialog, retBuf, &nameStr);
    void* elem = *(void**)retBuf;
    return elem;
}

// ============================================================
// StatusBarUI_Draw (0x14007a3d0, vtable +0x28):
// Element initialization — finds all named UI elements
// ============================================================

void StatusBarUI_Draw(StatusBarUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Date, population, temperature ---
    self->textSeason        = FindElement(self, "textSeason");
    self->textPopulation    = FindElement(self, "textPopulation");
    self->numberYear        = FindElement(self, "numberYear");
    self->numberTemperature = FindElement(self, "numberTemperature");

    // --- Resource counts ---
    self->numberWood     = FindElement(self, "numberWood");
    self->numberStone    = FindElement(self, "numberStone");
    self->numberIron     = FindElement(self, "numberIron");
    self->numberFireWood = FindElement(self, "numberFireWood");
    self->numberTools    = FindElement(self, "numberTools");
    self->numberFood     = FindElement(self, "numberFood");
    self->numberHerbs    = FindElement(self, "numberHerbs");
    self->numberClothes  = FindElement(self, "numberClothes");
    self->numberAlcohol  = FindElement(self, "numberAlcohol");
    self->numberCoal     = FindElement(self, "numberCoal");
    self->numberTextile  = FindElement(self, "numberTextile");

    // --- Custom resource counters (numberCustom0..9) ---
    for (int i = 0; i < 10; i++) {
        System::String fmtStr = System::String::Format("numberCustom%i", (uint64_t)i);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, &fmtStr);
        self->numberCustom[i] = *(void**)retBuf;
    }

    // --- Status icons (imageStatus*) ---
    self->imageStatusHome    = FindElement(self, "imageStatusHome");
    self->imageStatusFood    = FindElement(self, "imageStatusFood");
    self->imageStatusCold    = FindElement(self, "imageStatusCold");
    self->imageStatusJob     = FindElement(self, "imageStatusJob");
    self->imageStatusTool    = FindElement(self, "imageStatusTool");
    self->imageStatusDisease = FindElement(self, "imageStatusDisease");

    // Set element IDs 100-105 and hide initially (flag 0x10000 off)
    void** statusIcons = &self->imageStatusHome;
    for (int i = 0; i < 6; i++) {
        fn_InternalBasePtr_CheckPtr(&statusIcons[i]);
        fn_Element_SetElementId(statusIcons[i], 100 + i);
        fn_InternalBasePtr_CheckPtr(&statusIcons[i]);
        fn_Element_SetFlagsOnOff(statusIcons[i], 0x10000, 0);
    }

    // --- Status numbers (numberStatus*) ---
    self->numberStatusHome    = FindElement(self, "numberStatusHome");
    self->numberStatusFood    = FindElement(self, "numberStatusFood");
    self->numberStatusCold    = FindElement(self, "numberStatusCold");
    self->numberStatusJob     = FindElement(self, "numberStatusJob");
    self->numberStatusTool    = FindElement(self, "numberStatusTool");
    self->numberStatusDisease = FindElement(self, "numberStatusDisease");

    // --- Weather icons ---
    self->imageSunny       = FindElement(self, "imageSunny");
    self->imagePartlySunny = FindElement(self, "imagePartlySunny");
    self->imageCloudy      = FindElement(self, "imageCloudy");
    self->imageRain        = FindElement(self, "imageRain");
    self->imageSnow        = FindElement(self, "imageSnow");

    // --- Hearts/Stars icon groups ---
    {
        System::String nameStr("groupHearts");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, &nameStr);

        // NOTE: retail type-checks the group against a UI type-info whose decompiled
        // label ("g_TypeId_HelpButtonElem" = ButtonElement) is wrong for these ribbon
        // groups — with that id the check rejects the element and hearts stay blank.
        // The elements come from our own StatusBar.rsc, so accept the find result
        // directly (CitizenUI's identical groupHearts/groupStars binding does the same).
        fn_InternalBasePtr_CheckPtr(retBuf);
        void* typedGroup = *(void**)retBuf;
        uint8_t groupPtr[16];
        fn_InternalBasePtr_Ctor(groupPtr, typedGroup);
        CitizenUI_BuildIconArray_Impl(s + 0x108, (uintptr_t)groupPtr);
    }
    {
        System::String nameStr("groupStars");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, &nameStr);

        fn_InternalBasePtr_CheckPtr(retBuf);
        void* typedGroup = *(void**)retBuf;
        uint8_t groupPtr[16];
        fn_InternalBasePtr_Ctor(groupPtr, typedGroup);
        CitizenUI_BuildIconArray_Impl(s + 0x128, (uintptr_t)groupPtr);
    }

    // --- Title label ---
    {
        System::String nameStr("labelTitleParam");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, &nameStr);

        // Same wrong-label situation as the hearts/stars groups above: retail checks
        // against the LabelElement type-info, not ButtonElement — accept directly.
        fn_InternalBasePtr_CheckPtr(retBuf);
        uint8_t labelPtr[16];
        fn_InternalBasePtr_Ctor(labelPtr, *(void**)retBuf);
        self->labelTitleParam = *(void**)labelPtr;
    }

    // --- Set initial title text from map name ---
    void* gameWorld = *g_pGameWorldSingleton;
    System::String mapName(*reinterpret_cast<const System::String*>((uintptr_t)gameWorld + 0x28));  // B.3.1 bridge; FUN_140011f90: copies gameWorld+0x28 String
    fn_InternalBasePtr_CheckPtr(&self->labelTitleParam);
    if (self->labelTitleParam)
        fn_TextElement_SetParameter(self->labelTitleParam, 0, &mapName);
}

// ============================================================
// StatusBarUI_Startup (0x1400798d0, vtable +0x38):
// Main HUD update — called every frame
// ============================================================
// Iterates all citizens to count status problems (homeless, hungry,
// cold, jobless, toolless, diseased), accumulates health/happiness,
// updates population text, resource counts, weather icons,
// temperature, date, and status indicators.

void StatusBarUI_Startup(StatusBarUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* gameWorld = *g_pGameWorldSingleton;

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

        fn_InternalBasePtr_CheckPtr((void*)&citizen);

        // Check citizen state: +0x3C = current health
        // *(+0x288) + 0x44 = max health from description
        // +0x208 = death/special status pointer
        float currentHealth = *(float*)(citizen + 0x3C);
        float maxHealth = *(float*)(*(uintptr_t*)(citizen + 0x288) + 0x44);
        uintptr_t deathStatus = *(uintptr_t*)(citizen + 0x208);

        if (currentHealth < maxHealth || deathStatus != 0) {
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            if (deathStatus == 0) {
                buildingCount++;  // health declining but alive
            } else {
                dyingCount++;    // has death/special status
            }
        } else {
            adultCount++;        // healthy
        }

        // Find HappinessComponent on this citizen's entity
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        uintptr_t entity = *(uintptr_t*)(citizen + 0x10);
        fn_InternalBasePtr_CheckPtr((void*)&entity);

        int compCount = *(int*)(entity + 0x38);
        void* happyComp = nullptr;
        for (int j = 0; j < compCount; j++) {
            void** compArray = *(void***)(entity + 0x30);
            void* comp = compArray[j];
            fn_InternalBasePtr_CheckPtr(&comp);

            // Check component type via vtable GetTypeId (vtable +0x10)
            void** vtable = *(void***)comp;
            typedef int (*fn_GetTypeId_t)(void*);
            int typeId = ((fn_GetTypeId_t)vtable[2])(comp);

            if (typeId == (int)*g_TypeId_StatusIconComponent) {
                fn_InternalBasePtr_CheckPtr(&compArray[j]);
                void* pComp = compArray[j];
                bool isDerived = fn_IsDerivedFrom(pComp, *g_TypeId_StatusIconComponent);
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
            fn_InternalBasePtr_Ctor(happyPtr, happyComp);
            fn_InternalBasePtr_CheckPtr(happyPtr);
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
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        healthSum += *(float*)(citizen + 0x50);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
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
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        void* desc = *(void**)(firstCitizen + 0x18);
        void* citizenDesc = nullptr;
        if (fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription)) {
            citizenDesc = desc;
        }
        CitizenUI_UpdateIconDisplay_Impl(s + 0x108,
                                        healthSum / (float)citizenCount,
                                        *(float*)((uintptr_t)citizenDesc + 0x70));

        // Get max happiness from first citizen's description
        firstCitizen = *(uintptr_t*)(*(uintptr_t*)(popList + 0xA0));
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        desc = *(void**)(firstCitizen + 0x18);
        citizenDesc = nullptr;
        if (fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription)) {
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
        fn_InternalBasePtr_CheckPtr((void*)(pNum - 0x30));
        fn_Element_SetFlagsOnOff(*(void**)(pNum - 0x30), hideFlag, showFlag);

        fn_InternalBasePtr_CheckPtr((void*)pNum);
        fn_Element_SetFlagsOnOff(*(void**)pNum, hideFlag, showFlag);

        // Set count text as parameter 0
        System::String countStr = System::String::Format("%i", (uint64_t)*pCounts);
        fn_InternalBasePtr_CheckPtr((void*)pNum);
        fn_TextElement_SetParameter(*(void**)pNum, 0, &countStr);

        mask = (mask << 1) | (uint32_t)((int32_t)mask < 0);  // rotate left
        pNum += 8;
        pCounts++;
        loopCount--;
    } while (loopCount != 0);

    // --- Update population text (3 params: adults, dying, building) ---
    {
        System::String str = System::String::Format("%i", (uint64_t)adultCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 0, &str);
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)dyingCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 1, &str);
    }
    {
        System::String str = System::String::Format("%i", (uint64_t)buildingCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 2, &str);
    }

    // --- Update year ---
    int* timeData = *(int**)((uintptr_t)gameWorld + 0x128);
    int year = *timeData;
    fn_InternalBasePtr_CheckPtr(&self->numberYear);
    fn_NumberElement_SetValueInt(self->numberYear, year);

    // --- Update month/season ---
    float seconds = fn_Time_AsSeconds((void*)(timeData + 2));
    uint32_t monthIdx = (uint32_t)((seconds / kSecondsPerYear) * kMonthsPerYear);
    if ((int)monthIdx > 11) monthIdx = 0;

    {
        System::String monthKey = System::String::Format("Month%i", (uint64_t)monthIdx);

        fn_InternalBasePtr_CheckPtr(&self->textSeason);
        void* seasonElem = self->textSeason;

        // UIString ctor takes two params from the text element: +0xD8 and the formatted
        // key. Guard the string table (retail asserts it; a missing table would crash
        // StringTable::GetIndexFromName in standalone).
        if (seasonElem && *(uint64_t*)((uintptr_t)seasonElem + 0xD8) != 0) {
            uint8_t uiStr[16];
            uint64_t* pResult = (uint64_t*)fn_UIString_Ctor(uiStr,
                                  *(uint64_t*)((uintptr_t)seasonElem + 0xD8),
                                  &monthKey);
            // Copy UIString result to local for SetText (16 bytes: text + string table ref)
            uint64_t localUIStr[2];
            localUIStr[0] = pResult[0];
            localUIStr[1] = pResult[1];

            fn_InternalBasePtr_CheckPtr(&self->textSeason);
            fn_TextElement_SetText(self->textSeason, localUIStr);
        }
    }

    // --- Update temperature ---
    uintptr_t weatherData = *(uintptr_t*)((uintptr_t)gameWorld + 0x130);
    float rawTemp = *(float*)(weatherData + 0x18);
    float displayTemp = rawTemp;

    // Check if Celsius mode (game settings byte +0x15 = showCelsius, see
    // optionsdialog.cpp). Null-guarded: the settings slot can be unresolved in
    // standalone — default to Fahrenheit (setting off) like a fresh install.
    void* settings = (g_pNewGameSettings ? *g_pNewGameSettings : nullptr);
    if (settings && *(uint8_t*)((uintptr_t)settings + 0x15) != 0) {
        displayTemp = ((rawTemp - kFreezingF) * kFtoC_Num) / kFtoC_Den;
    }
    fn_InternalBasePtr_CheckPtr(&self->numberTemperature);
    fn_NumberElement_SetValueInt(self->numberTemperature, (int)displayTemp);

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
        fn_InternalBasePtr_CheckPtr((void*)pWeather);
        fn_Element_SetFlagsOnOff(*(void**)pWeather,
                                  (uint32_t)(weatherIdx != weatherType),
                                  (uint32_t)(weatherIdx == weatherType));
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
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x58));       // numberWood
    fn_NumberElement_SetValueInt(*(void**)(s + 0x58), iVar7_wood);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x60));       // numberStone
    fn_NumberElement_SetValueInt(*(void**)(s + 0x60), iVar2_stone);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x68));       // numberIron
    fn_NumberElement_SetValueInt(*(void**)(s + 0x68), iVar3_iron);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x78));       // numberTools
    fn_NumberElement_SetValueInt(*(void**)(s + 0x78), iVar1_tool);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x70));       // numberFireWood
    fn_NumberElement_SetValueInt(*(void**)(s + 0x70), iVar21_fw);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x80));       // numberFood
    fn_NumberElement_SetValueInt(*(void**)(s + 0x80), iVar23_food);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x88));       // numberHerbs
    fn_NumberElement_SetValueInt(*(void**)(s + 0x88), valHerbs);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x90));       // numberClothes
    fn_NumberElement_SetValueInt(*(void**)(s + 0x90), valClothes);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x98));       // numberAlcohol
    fn_NumberElement_SetValueInt(*(void**)(s + 0x98), valAlcohol);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xA0));       // numberCoal
    fn_NumberElement_SetValueInt(*(void**)(s + 0xA0), valCoal);

    // Textile (numberTextile at +0xA8) — may be null
    if (*(void**)(s + 0xA8) != nullptr) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xA8));
        fn_NumberElement_SetValueInt(*(void**)(s + 0xA8), valTextile);
    }

    // Custom resources (numberCustom0..9 at +0xB0..+0xF8)
    // Original: lVar25 from 0x160 to <0x2A0 step 0x20
    uintptr_t resOffset = 0x160;
    uintptr_t pCustom = s + 0xB0;
    do {
        if (*(void**)pCustom != nullptr) {
            int value = *(int*)(resOffset + *(uintptr_t*)(popList + 0x170));
            fn_InternalBasePtr_CheckPtr((void*)pCustom);
            fn_NumberElement_SetValueInt(*(void**)pCustom, value);
        }
        resOffset += 0x20;
        pCustom += 8;
    } while (resOffset < 0x2A0);

    // --- Update title text (map name) ---
    System::String mapName(*reinterpret_cast<const System::String*>((uintptr_t)gameWorld + 0x28));  // B.3.1 bridge; FUN_140011f90: copies gameWorld+0x28 String
    fn_InternalBasePtr_CheckPtr(&self->labelTitleParam);
    if (self->labelTitleParam)
        fn_TextElement_SetParameter(self->labelTitleParam, 0, &mapName);
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
    void* gameWorld = *g_pGameWorldSingleton;
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
        fn_InternalBasePtr_CheckPtr((void*)&entityPtr);

        {
            // Get component entity at entity+0x10
            uintptr_t compEntity = *(uintptr_t*)(entityPtr + 0x10);
            fn_InternalBasePtr_CheckPtr((void*)&compEntity);

            // Search components for HappinessComponent (DAT_1401c7e08)
            void** compArray = *(void***)(compEntity + 0x30);
            int compCount = *(int*)(compEntity + 0x38);
            void* happinessComp = nullptr;

            for (int i = 0; i < compCount; i++) {
                void* comp = compArray[i];
                if (!comp) continue;

                // vtable slot +0x10 (index 2) returns type ID
                void** vtable = *(void***)comp;
                typedef int (*fn_GetTypeId_t)(void*);
                int compTypeId = ((fn_GetTypeId_t)vtable[2])(comp);

                if (g_TypeId_StatusIconComponent && (uint32_t)compTypeId == *g_TypeId_StatusIconComponent) {
                    // Confirm with IsDerivedFrom
                    if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *g_TypeId_StatusIconComponent)) {
                        happinessComp = comp;
                    }
                    break;
                }
            }

            // If no HappinessComponent found, store null
            uintptr_t happinessPtr = (uintptr_t)happinessComp;
            fn_InternalBasePtr_Ctor(&happinessPtr, happinessComp);

            fn_InternalBasePtr_CheckPtr((void*)&happinessPtr);

            // Check status bits at HappinessComponent+0xF0
            uint32_t statusBits = happinessPtr ? *(uint32_t*)(happinessPtr + 0xF0) : 0;

            if ((statusBits & bitMask) != 0 || checked >= citizenCount) {
                // Found a match (or exhausted all citizens)
                // Re-fetch entity for navigation
                uintptr_t navEntity = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);
                fn_InternalBasePtr_CheckPtr((void*)&navEntity);

                uintptr_t navCompEntity = *(uintptr_t*)(navEntity + 0x10);
                fn_InternalBasePtr_CheckPtr((void*)&navCompEntity);

                // FindComponent_MeshInstance (uses same type ID DAT_1401c7e08).
                // Null-guarded: the exe finder may be unregistered in standalone —
                // skip the redundant double-check in that case (the loop above
                // already verified the status bit).
                if (fn_FindComponent_MeshInstance) {
                    uintptr_t meshComp = 0;
                    fn_FindComponent_MeshInstance((void*)navCompEntity, &meshComp);
                    fn_InternalBasePtr_CheckPtr((void*)&meshComp);

                    // Double-check: if meshComp's status doesn't have our bit, bail
                    if (meshComp) {
                        uint32_t meshStatus = *(uint32_t*)(meshComp + 0xF0);
                        if ((meshStatus & bitMask) == 0) return;
                    }
                }

                // Navigate to the citizen entity
                // FUN_140103040(game+0x120, &dialogRef, &entityRef)
                uintptr_t navEntity2 = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);
                fn_InternalBasePtr_CheckPtr((void*)&navEntity2);
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

void StatusBarUI_OnRemove(StatusBarUI* self, void* param2, int eventType, int eventId) {
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
    StatusBarUI_StatusIconHandler_Impl(self, iconIndex);
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
#include "Object/factory_registration.h"
#include <cstddef>
#include "dialog_controller_vtable.h"

namespace statusbarui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("StatusBarUI");
    return c;
}

// Backfill the exe-.rdata type-id globals the impl paths dereference:
//  - DAT_1401c7e08 (g_TypeId_StatusIconComponent) is the StatusIconComponent type id
//    (Ghidra name-table; see statusiconui.cpp) and is unfilled (0/0xFFFFFFFF) in
//    standalone -> resolve by TRUE name.
//  - g_TypeId_CitizenDescription is normally filled by the townhall promotion's
//    EnsureGlobals; fill it here too so the status dialog works on its own.
void EnsureGlobals() {
    static uint32_t s_statusIconTid = 0;
    if (!g_TypeId_StatusIconComponent || *g_TypeId_StatusIconComponent == 0 ||
        *g_TypeId_StatusIconComponent == 0xFFFFFFFFu) {
        if (!s_statusIconTid)
            s_statusIconTid = ::Object::ResolveTypeIdByNameLazy("StatusIconComponent");
        if (s_statusIconTid) g_TypeId_StatusIconComponent = &s_statusIconTid;
    }
    static uint32_t s_citDescTid = 0;
    if (!g_TypeId_CitizenDescription) {
        if (!s_citDescTid)
            s_citDescTid = ::Object::ResolveTypeIdByNameLazy("CitizenDescription");
        if (s_citDescTid) g_TypeId_CitizenDescription = &s_citDescTid;
    }
}

// Every function/global the Draw+Startup+event paths dereference unguarded must
// be live, or a null-deref faults (no SEH under MinGW).
bool Ready() {
    if (!(fn_DialogInstance_FindElement && fn_FindElement &&
          fn_Element_Child && fn_Element_ChildCount && fn_Element_SetElementId &&
          fn_Element_SetFlagsOnOff && fn_TextElement_SetParameter &&
          fn_TextElement_SetText && fn_NumberElement_SetValueInt &&
          fn_UIString_Ctor && fn_Time_AsSeconds && fn_IsDerivedFrom &&
          fn_InternalBasePtr_Ctor && fn_InternalBasePtr_CheckPtr &&
          fn_MemAlloc && fn_MemFreeStatic && g_pMemAllocSingleton))
        return false;
    if (!(g_TypeId_StatusIconComponent && g_TypeId_CitizenDescription)) return false;
    return true;
}

// The world-side data Startup reads unguarded: townMgr (gw+0xf0) with its
// resource stock block (+0x170), the game clock (gw+0x128), and the weather
// object (gw+0x130). The citizen roster (+0xa0/+0xa8) may legitimately be
// count 0 (handled by the impl).
bool TownReady() {
    if (!(g_pGameWorldSingleton && *g_pGameWorldSingleton)) return false;
    uintptr_t gw = *(uintptr_t*)g_pGameWorldSingleton;
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

// idx4 (+0x20) / idx5 (+0x28): setup — element discovery + initial title.
void W_Setup(void* self) {
    EnsureGlobals();
    if (!self || !Bound(self) || !Ready() || !TownReady()) return;
    if (Built(self)) return;   // idempotence
    StatusBarUI_Draw((StatusBarUI*)self);
}

// idx7 (+0x38): full statistics refresh.
void W_Refresh(void* self) {
    EnsureGlobals();
    if (!self || !Bound(self) || !Ready() || !TownReady()) return;
    if (!Built(self)) StatusBarUI_Draw((StatusBarUI*)self);
    if (!Built(self)) return;
    StatusBarUI_Startup((StatusBarUI*)self);
}

// One-shot gate diagnostic — a silently-skipping gate is indistinguishable from
// a zeroed dialog (same lesson as TownHallSA_Diag).
static void DiagOnce(void* self) {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;
    uintptr_t gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                 ? *(uintptr_t*)g_pGameWorldSingleton : 0;
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
    uintptr_t gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                 ? *(uintptr_t*)g_pGameWorldSingleton : 0;
    uintptr_t townMgr = gw ? *(uintptr_t*)(gw + 0xf0) : 0;
    uintptr_t stock = townMgr ? *(uintptr_t*)(townMgr + 0x170) : 0;
    if (!stock) return;
    fprintf(stderr, "[StatusBarUI] SA stock slots (cur@+0):");
    for (int i = 0; i < 0x15; i++)
        fprintf(stderr, " %d:%d", i, *(int*)(stock + (uintptr_t)i * 0x20));
    fprintf(stderr, "\n");
    fflush(stderr);
}

// idx8 (+0x40): per-frame — self-heal the build, then refresh the statistics
// twice a second (retail refreshes every frame; the citizen scan is cheap but
// the ~40 element writes are not worth 60Hz).
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    DiagOnce(self);
    if (!self || !Bound(self) || !Ready() || !TownReady()) return;
    if (!Built(self)) {
        StatusBarUI_Draw((StatusBarUI*)self);
        if (Built(self)) {
            fprintf(stderr, "[StatusBarUI] SA build OK — elements bound, first refresh\n");
            fflush(stderr);
        } else {
            return;
        }
        StatusBarUI_Startup((StatusBarUI*)self);
        return;
    }
    // _pad104 (struct tail of the +0x100 cycling index) as a throttle counter.
    uint32_t* throttle = (uint32_t*)((uintptr_t)self + 0x104);
    if ((*throttle)++ % 30 == 0) {
        StatusBarUI_Startup((StatusBarUI*)self);
        DiagStockOnce();
    }
}

// idx9 (+0x48): status-icon clicks (element ids 100-105) — cycle the camera to
// the next citizen with that problem.
void W_OnEvent(void* self, void* param2, int msg, int id) {
    EnsureGlobals();
    if (!self || !Built(self) || !Ready() || !TownReady()) return;
    StatusBarUI_OnRemove((StatusBarUI*)self, param2, msg, id);
}

// Destructor: the only owned heap is the two GroupRating icon arrays built by
// CitizenUI_BuildIconArray_Impl (whole+half InternalBasePtr[] each, at +0x108
// and +0x128).
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = g_vt;
    uintptr_t s = (uintptr_t)self;
    if (fn_MemFreeStatic) {
        static const uintptr_t kIconArrays[4] = { 0x108, 0x118, 0x128, 0x138 };
        for (uintptr_t off : kIconArrays) {
            fn_MemFreeStatic(*(void**)(s + off));
            *(uint64_t*)(s + off) = 0;
            *(uint64_t*)(s + off + 8) = 0;   // count/capacity pair
        }
        if (flags & 1) fn_MemFreeStatic(self);
    }
    return self;
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Setup);
        g_vt[5] = reinterpret_cast<void*>(&W_Setup);     // retail setup slot
        g_vt[7] = reinterpret_cast<void*>(&W_Refresh);   // retail refresh slot
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_OnEvent);
    }
} g_init;

// Wave 10D lane VTFID: retail's StatusBarUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "StatusBarUI", &StubNop);

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x1D0); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x1D0), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace statusbarui_sa

REGISTER_TYPE(StatusBarUI, "UI::DialogController",
              &statusbarui_sa::Factory, &statusbarui_sa::InPlace);

