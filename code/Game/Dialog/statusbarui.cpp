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
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
    void* dialog = *(void**)(s + 0x18);

    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(dialog, retBuf, nameStr);
    void* elem = *(void**)retBuf;
    fn_String_Dtor(nameStr);
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
        uint8_t fmtStr[8];
        fn_String_Format((char*)fmtStr, "numberCustom%i", (uint64_t)i);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, fmtStr);
        self->numberCustom[i] = *(void**)retBuf;
        fn_String_Dtor(fmtStr);
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
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "groupHearts");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, nameStr);

        fn_InternalBasePtr_CheckPtr(retBuf);
        void* groupElem = *(void**)retBuf;
        void* typedGroup = nullptr;
        if (groupElem && fn_IsDerivedFrom(groupElem, *g_TypeId_HelpButtonElem)) {
            typedGroup = groupElem;
        }
        uint8_t groupPtr[16];
        fn_InternalBasePtr_Ctor(groupPtr, typedGroup);
        CitizenUI_BuildIconArray_Impl(s + 0x108, (uintptr_t)groupPtr);
        fn_String_Dtor(nameStr);
    }
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "groupStars");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, nameStr);

        fn_InternalBasePtr_CheckPtr(retBuf);
        void* groupElem = *(void**)retBuf;
        void* typedGroup = nullptr;
        if (groupElem && fn_IsDerivedFrom(groupElem, *g_TypeId_HelpButtonElem)) {
            typedGroup = groupElem;
        }
        uint8_t groupPtr[16];
        fn_InternalBasePtr_Ctor(groupPtr, typedGroup);
        CitizenUI_BuildIconArray_Impl(s + 0x128, (uintptr_t)groupPtr);
        fn_String_Dtor(nameStr);
    }

    // --- Title label ---
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "labelTitleParam");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialog = *(void**)(s + 0x18);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(dialog, retBuf, nameStr);

        fn_InternalBasePtr_CheckPtr(retBuf);
        void* labelElem = *(void**)retBuf;
        void* typedLabel = nullptr;
        if (labelElem && fn_IsDerivedFrom(labelElem, *g_TypeId_HelpButtonElem)) {
            typedLabel = labelElem;
        }
        uint8_t labelPtr[16];
        fn_InternalBasePtr_Ctor(labelPtr, typedLabel);
        self->labelTitleParam = *(void**)labelPtr;
        fn_String_Dtor(nameStr);
    }

    // --- Set initial title text from map name ---
    void* gameWorld = *g_pGameWorldSingleton;
    uint8_t mapName[8];
    fn_String_Ctor_Copy(mapName, (void*)((uintptr_t)gameWorld + 0x28));  // FUN_140011f90: copies gameWorld+0x28 String
    fn_InternalBasePtr_CheckPtr(&self->labelTitleParam);
    fn_TextElement_SetParameter(self->labelTitleParam, 0, mapName);
    fn_String_Dtor(mapName);
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

            if (typeId == (int)*g_TypeId_HappinessComponent) {
                fn_InternalBasePtr_CheckPtr(&compArray[j]);
                void* pComp = compArray[j];
                bool isDerived = fn_IsDerivedFrom(pComp, *g_TypeId_HappinessComponent);
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
        uint8_t countStr[8];
        fn_String_Format((char*)countStr, "%i", (uint64_t)*pCounts);
        fn_InternalBasePtr_CheckPtr((void*)pNum);
        fn_TextElement_SetParameter(*(void**)pNum, 0, countStr);
        fn_String_Dtor(countStr);

        mask = (mask << 1) | (uint32_t)((int32_t)mask < 0);  // rotate left
        pNum += 8;
        pCounts++;
        loopCount--;
    } while (loopCount != 0);

    // --- Update population text (3 params: adults, dying, building) ---
    {
        uint8_t str[8];
        fn_String_Format((char*)str, "%i", (uint64_t)adultCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 0, str);
        fn_String_Dtor(str);
    }
    {
        uint8_t str[8];
        fn_String_Format((char*)str, "%i", (uint64_t)dyingCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 1, str);
        fn_String_Dtor(str);
    }
    {
        uint8_t str[8];
        fn_String_Format((char*)str, "%i", (uint64_t)buildingCount);
        fn_InternalBasePtr_CheckPtr(&self->textPopulation);
        fn_TextElement_SetParameter(self->textPopulation, 2, str);
        fn_String_Dtor(str);
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
        uint8_t monthKey[8];
        fn_String_Format((char*)monthKey, "Month%i", (uint64_t)monthIdx);

        fn_InternalBasePtr_CheckPtr(&self->textSeason);
        void* seasonElem = self->textSeason;

        // UIString ctor takes two params from the text element: +0xD8 and the formatted key
        uint8_t uiStr[16];
        uint64_t* pResult = (uint64_t*)fn_UIString_Ctor(uiStr,
                              *(uint64_t*)((uintptr_t)seasonElem + 0xD8),
                              monthKey);
        // Copy UIString result to local for SetText (16 bytes: text + string table ref)
        uint64_t localUIStr[2];
        localUIStr[0] = pResult[0];
        localUIStr[1] = pResult[1];

        fn_InternalBasePtr_CheckPtr(&self->textSeason);
        fn_TextElement_SetText(self->textSeason, localUIStr);
        fn_String_Dtor(monthKey);
    }

    // --- Update temperature ---
    uintptr_t weatherData = *(uintptr_t*)((uintptr_t)gameWorld + 0x130);
    float rawTemp = *(float*)(weatherData + 0x18);
    float displayTemp = rawTemp;

    // Check if Celsius mode (game settings +0x15)
    void* settings = *g_pNewGameSettings;
    if (*(uint8_t*)((uintptr_t)settings + 0x15) != 0) {
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
    uint8_t mapName[8];
    fn_String_Ctor_Copy(mapName, (void*)((uintptr_t)gameWorld + 0x28));  // FUN_140011f90: copies gameWorld+0x28 String
    fn_InternalBasePtr_CheckPtr(&self->labelTitleParam);
    fn_TextElement_SetParameter(self->labelTitleParam, 0, mapName);
    fn_String_Dtor(mapName);
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

                if (g_TypeId_HappinessComponent && (uint32_t)compTypeId == *g_TypeId_HappinessComponent) {
                    // Confirm with IsDerivedFrom
                    if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, *g_TypeId_HappinessComponent)) {
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

                // FindComponent_MeshInstance (uses same type ID DAT_1401c7e08)
                uintptr_t meshComp = 0;
                fn_FindComponent_MeshInstance((void*)navCompEntity, &meshComp);
                fn_InternalBasePtr_CheckPtr((void*)&meshComp);

                // Double-check: if meshComp's status doesn't have our bit, bail
                if (meshComp) {
                    uint32_t meshStatus = *(uint32_t*)(meshComp + 0xF0);
                    if ((meshStatus & bitMask) == 0) return;
                }

                // Navigate to the citizen entity
                // FUN_140103040(game+0x120, &dialogRef, &entityRef)
                uintptr_t navEntity2 = *(uintptr_t*)(citizenArray + (int64_t)self->statusCitizenIndex * 8);
                fn_InternalBasePtr_CheckPtr((void*)&navEntity2);
                uintptr_t entityRef = *(uintptr_t*)(navEntity2 + 0x10);

                uintptr_t dialogRef = *(uintptr_t*)((uintptr_t)self + 0x18);
                void* entityController = *(void**)((uintptr_t)gameWorld + 0x120);

                if (fn_Toolbar_CenterOnEntity) {
                    fn_Toolbar_CenterOnEntity(entityController, &dialogRef, &entityRef);
                }
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
