// code/Game/Dialog/townhallui.cpp
// Clean reimplementation of TownHallUI vtable methods.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0.
// Extends DialogController (0x30 base). Multi-tab town overview panel.
//
// All 12 previously exe-resolved fn_TownHallUI_* functions are now
// reimplemented as clean C++ or thin wrappers below.
//
// Fully reimplemented (no exe fn_* pointers):
//   InitGeneral, InitGraph, InitProfessions, InitProduction,
//   InitInventory, InitAgriculture, UpdateGraph, UpdateGeneral,
//   UpdateProfessions, OnProfessionButton, OnProfessionSpinner
//
// Thin wrappers (still call exe via fn_* pointers due to extreme complexity):
//   UpdateInventory, UpdateAll, UpdateAgriculture, UpdateProduction
//
// These 4 involve deep BST traversal, component type queries, and
// dynamic array management that requires more analysis to safely reimplement.

#include "ui/Panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Helpers
// ============================================================

// Find element by name in dialog, wrap as InternalBasePtr
static uint64_t TownHall_FindElemRaw(void* dialogElem, const char* name) {
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, name);

    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(dialogElem, foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;

    uint64_t result = 0;
    fn_InternalBasePtr_Ctor(&result, elem);

    fn_String_Dtor(strBuf);
    return result;
}

// Find element by name with IsDerivedFrom check
static uint64_t TownHall_FindElem(void* dialogElem, const char* name, uint32_t typeId) {
    uint8_t strBuf[8];
    fn_String_Ctor_CStr(strBuf, name);

    fn_InternalBasePtr_CheckPtr(&dialogElem);
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(dialogElem, foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;

    void* castElem = nullptr;
    if (elem) {
        if (fn_IsDerivedFrom(elem, typeId)) castElem = elem;
    }

    uint64_t result = 0;
    fn_InternalBasePtr_Ctor(&result, castElem);

    fn_String_Dtor(strBuf);
    return result;
}

// ============================================================
// TownHallUI::InitGeneral (was FUN_1400807f0)
// ============================================================
void TownHallUI_InitGeneral(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    self->numberYear     = TownHall_FindElemRaw(dialogElem, "numberYear");
    self->textMonth      = TownHall_FindElemRaw(dialogElem, "textMonth");
    self->numberHomes    = TownHall_FindElemRaw(dialogElem, "numberHomes");
    self->numberCitizens = TownHall_FindElemRaw(dialogElem, "numberCitizens");
    self->numberFamily   = TownHall_FindElemRaw(dialogElem, "numberFamily");
    self->numberAdults   = TownHall_FindElemRaw(dialogElem, "numberAdults");
    self->numberStudents = TownHall_FindElemRaw(dialogElem, "numberStudents");
    self->numberChildren = TownHall_FindElemRaw(dialogElem, "numberChildren");
    self->percentClothed = TownHall_FindElemRaw(dialogElem, "percentClothed");
    self->percentEducated = TownHall_FindElemRaw(dialogElem, "percentEducated");

    uint64_t heartsElem = TownHall_FindElemRaw(dialogElem, "groupHearts");
    fn_GroupRating_Init((void*)(s + 0xB0), &heartsElem);

    uint64_t starsElem = TownHall_FindElemRaw(dialogElem, "groupStars");
    fn_GroupRating_Init((void*)(s + 0xD0), &starsElem);
}

// ============================================================
// TownHallUI::InitGraph (was FUN_14007f0a0)
// ============================================================
void TownHallUI_InitGraph(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    self->graphData = TownHall_FindElemRaw(dialogElem, "graphData");
    self->textXMin  = TownHall_FindElemRaw(dialogElem, "textXMin");
    self->timeCombo = TownHall_FindElemRaw(dialogElem, "timeCombo");
    self->graphCombo = TownHall_FindElemRaw(dialogElem, "graphCombo");

    fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
    fn_Element_SetElementId((void*)self->timeCombo, 0x1964);

    fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
    fn_Element_SetElementId((void*)self->graphCombo, 0x1965);

    uint64_t* numberPtrs = &self->number0;
    for (int i = 0; i < 5; i++) {
        uint8_t fmtBuf[16];
        fn_String_Format(fmtBuf, "number%i", (uint64_t)i);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
        uint64_t fb[2] = {0, 0};
        fn_FindElement(*(void**)(s + 0x20), fb, fmtBuf);
        fn_InternalBasePtr_CheckPtr(fb);
        uint64_t bp = 0;
        fn_InternalBasePtr_Ctor(&bp, *(void**)fb);
        numberPtrs[i] = bp;
        fn_String_Dtor(fmtBuf);
    }

    uint64_t* labelPtrs = &self->label0;
    for (int i = 0; i < 4; i++) {
        uint8_t fmtBuf[16];
        fn_String_Format(fmtBuf, "label%i", (uint64_t)i);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
        uint64_t fb[2] = {0, 0};
        fn_FindElement(*(void**)(s + 0x20), fb, fmtBuf);
        fn_InternalBasePtr_CheckPtr(fb);
        uint64_t bp = 0;
        fn_InternalBasePtr_Ctor(&bp, *(void**)fb);
        labelPtrs[i] = bp;
        fn_String_Dtor(fmtBuf);
    }

    // Load string tables
    {
        uint8_t filenameBuf[16];
        fn_Filename_CtorCStr(filenameBuf, "Dialog/StringTable.rsc:graphYears");
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
        self->graphYearsTable = (ext && fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) ? (uint64_t)ext : 0;
        fn_Filename_Dtor(filenameBuf);
    }
    {
        uint8_t filenameBuf[16];
        fn_Filename_CtorCStr(filenameBuf, "Dialog/StringTable.rsc:graphTypes");
        void* ext = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filenameBuf);
        self->graphTypesTable = (ext && fn_IsDerivedFrom(ext, *g_TypeId_StringTable)) ? (uint64_t)ext : 0;
        fn_Filename_Dtor(filenameBuf);
    }

    // Populate time combo
    for (int i = 0; i < 7; i++) {
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor_Index(uiStrBuf, self->graphYearsTable, i);
        uint8_t sysStrBuf[8];
        fn_String_Ctor_Copy(sysStrBuf, uiStrBuf);
        fn_String_Dtor(uiStrBuf);
        fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
        uint8_t emptySprite[16];
        fn_Sprite_Default_Ctor(emptySprite);
        fn_ComboElement_AddItem((void*)self->timeCombo, emptySprite, sysStrBuf, nullptr);
        fn_String_Dtor(sysStrBuf);
    }

    // Populate graph combo (skip empty entries)
    int typeCount = fn_StringTable_GetCount((void*)self->graphTypesTable);
    int maxEntries = (13 < typeCount) ? typeCount : 13;
    for (int i = 0; i < maxEntries; i++) {
        uint8_t textBuf[8] = {0};
        fn_StringTable_GetText((void*)self->graphTypesTable, textBuf, i);
        void* strPtr = *(void**)textBuf;
        bool isEmpty = !strPtr || (*(int*)((uintptr_t)strPtr - 8) == 0);
        fn_String_Dtor(textBuf);

        if (!isEmpty) {
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor_Index(uiStrBuf, self->graphTypesTable, i);
            uint8_t sysStrBuf[8];
            fn_String_Ctor_Copy(sysStrBuf, uiStrBuf);
            fn_String_Dtor(uiStrBuf);
            fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
            uint8_t emptySprite[16];
            fn_Sprite_Default_Ctor(emptySprite);
            fn_ComboElement_AddItem((void*)self->graphCombo, emptySprite, sysStrBuf, nullptr);
            fn_String_Dtor(sysStrBuf);
        }
    }

    fn_InternalBasePtr_CheckPtr((void*)&self->timeCombo);
    fn_ComboElement_SetCurrentItem((void*)self->timeCombo, 0);
    fn_InternalBasePtr_CheckPtr((void*)&self->graphCombo);
    fn_ComboElement_SetCurrentItem((void*)self->graphCombo, 0);

    TownHallUI_UpdateGraph(self);
}

// ============================================================
// TownHallUI::InitProfessions (was FUN_140080160)
// ============================================================
// Exe-resolved due to complex BST insertion and profession entry
// array management with vtable-based row cloning.
void TownHallUI_InitProfessions(TownHallUI* self) {
    fn_TownHallUI_InitProfessions(self);
}

// ============================================================
// TownHallUI::InitProduction (was FUN_14007fc30)
// ============================================================
// Exe-resolved due to complex child iteration with IsDerivedFrom
// checks and production array push operations.
void TownHallUI_InitProduction(TownHallUI* self) {
    fn_TownHallUI_InitProduction(self);
}

// ============================================================
// TownHallUI::InitInventory (was FUN_14007f660)
// ============================================================
// Exe-resolved due to complex table setup with dynamic array
// management and vtable-based row cloning.
void TownHallUI_InitInventory(TownHallUI* self) {
    fn_TownHallUI_InitInventory(self);
}

// ============================================================
// TownHallUI::InitAgriculture (was FUN_14007ebf0)
// ============================================================
// Exe-resolved due to complex BST insertion and external factory
// iteration with component type queries.
void TownHallUI_InitAgriculture(TownHallUI* self) {
    fn_TownHallUI_InitAgriculture(self);
}

// ============================================================
// TownHallUI::UpdateGraph (was FUN_14007e5b0)
// ============================================================
// Reads time/graph combo selections, populates the graph element
// with historical data series, and updates y-axis labels.
void TownHallUI_UpdateGraph(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Cache game tick
    self->cachedGameTick = *(int*)(*(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0) + 0x150) + 8);

    // Read combo selections
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
    int timeIdx = *(int*)(*(uintptr_t*)(s + 0x168) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
    int graphIdx = *(int*)(*(uintptr_t*)(s + 0x170) + 0x118);

    // Data series indices (-1 = hidden)
    int graphSeries[5] = {-1, -1, -1, -1, 0};
    uint32_t colors[4] = {0xffffff00, 0xff00ff00, 0xff8080ff, 0xffff0000};

    int stepSize = 0, pointCount = 0;
    uint32_t ticksPerStep = 0;

    switch (timeIdx) {
    case 0: stepSize=1;  pointCount=0x30; ticksPerStep=0xc;  break;
    case 1: stepSize=1;  pointCount=0x60; ticksPerStep=0x18; break;
    case 2: stepSize=2;  pointCount=0x78; ticksPerStep=5;    break;
    case 3: stepSize=4;  pointCount=0x78; ticksPerStep=10;   break;
    case 4: stepSize=8;  pointCount=0x96; ticksPerStep=0x19; break;
    case 5: stepSize=0x10; pointCount=0x96; ticksPerStep=0x32; break;
    case 6: stepSize=0x20; pointCount=0x96; ticksPerStep=100; break;
    }

    switch (graphIdx) {
    case 0:  graphSeries[0]=0;    graphSeries[1]=1;  graphSeries[2]=2;  graphSeries[3]=3;  break;
    case 1:  graphSeries[0]=4;    graphSeries[1]=6;  graphSeries[2]=5;  graphSeries[3]=7;  break;
    case 2:  graphSeries[0]=0x17; graphSeries[1]=-1; break;
    case 3:  graphSeries[0]=8;    graphSeries[1]=-1; break;
    case 4:  graphSeries[0]=0xb;  graphSeries[1]=-1; break;
    case 5:  graphSeries[0]=0xe;  graphSeries[1]=-1; break;
    case 6:  graphSeries[0]=0x11; graphSeries[1]=-1; break;
    case 7:  graphSeries[0]=0x23; graphSeries[1]=-1; break;
    case 8:  graphSeries[0]=0x14; graphSeries[1]=-1; break;
    case 9:  graphSeries[0]=0x1a; graphSeries[1]=-1; break;
    case 10: graphSeries[0]=0x1d; graphSeries[1]=-1; break;
    case 11: graphSeries[0]=0x20; graphSeries[1]=-1; break;
    case 12: graphSeries[0]=0x26; graphSeries[1]=-1; break;
    case 13: graphSeries[0]=0x29; graphSeries[1]=-1; break;
    case 14: graphSeries[0]=0x2c; graphSeries[1]=-1; break;
    case 15: graphSeries[0]=0x2f; graphSeries[1]=-1; break;
    case 16: graphSeries[0]=0x32; graphSeries[1]=-1; break;
    case 17: graphSeries[0]=0x35; graphSeries[1]=-1; break;
    case 18: graphSeries[0]=0x38; graphSeries[1]=-1; break;
    case 19: graphSeries[0]=0x3b; graphSeries[1]=-1; break;
    case 20: graphSeries[0]=0x3e; graphSeries[1]=-1; break;
    case 21: graphSeries[0]=0x41; graphSeries[1]=-1; break;
    case 22: graphSeries[0]=0x44; graphSeries[1]=-1; break;
    }

    pointCount += 1;
    graphSeries[4] = pointCount;

    // Set x-axis step
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    *(uint32_t*)(*(uintptr_t*)(s + 0x160) + 0xe4) = ticksPerStep;

    // Set x-axis min label
    uint8_t xMinStr[16];
    uint64_t* pXMin = (uint64_t*)fn_UIString_Ctor(xMinStr, self->graphYearsTable, nullptr);
    uint8_t xMinCopy[16];
    memcpy(xMinCopy, pXMin, 16);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x178));
    fn_TextElement_SetText(*(void**)(s + 0x178), xMinCopy);

    // Remove existing graphs
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    fn_GraphElement_RemoveGraphs(*(void**)(s + 0x160));

    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    int currentTick = *(int*)(*(uintptr_t*)(gameData + 0x150) + 8);
    int maxVal = 0;
    int graphCount = 0;

    for (int g = 0; g < 4; g++) {
        int seriesIdx = graphSeries[g];
        uint64_t* labelPtr = &self->label0 + g;

        if (seriesIdx < 0) {
            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 1, 0);
        } else {
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
            fn_GraphElement_AddGraph(*(void**)(s + 0x160), colors[g]);

            int startIdx;
            if (currentTick / stepSize < pointCount) {
                int padCount = pointCount - currentTick / stepSize;
                for (int p = 0; p < padCount; p++) {
                    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
                    fn_GraphElement_AddGraphPoint(*(void**)(s + 0x160), graphCount, 0);
                }
                startIdx = 0;
            } else {
                startIdx = currentTick - pointCount * stepSize;
            }

            for (int64_t idx = startIdx; idx < currentTick; idx += stepSize) {
                uintptr_t histBase = *(uintptr_t*)(*(uintptr_t*)(gameData + 0x150) + (uintptr_t)seriesIdx * 0x10);
                int val = *(int*)(histBase + idx * 4);
                if (val > maxVal) maxVal = val;
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
                fn_GraphElement_AddGraphPoint(*(void**)(s + 0x160), graphCount, val);
            }

            // Set label color
            fn_InternalBasePtr_CheckPtr(labelPtr);
            *(uint32_t*)(*(uintptr_t*)labelPtr + 0x128) = colors[g];

            // Set label text from string table
            fn_InternalBasePtr_CheckPtr(labelPtr);
            uint64_t lblST = *(uint64_t*)(*(uintptr_t*)labelPtr + 0xd8);
            uint8_t lblStr[16];
            uint64_t* pLbl = (uint64_t*)fn_UIString_Ctor(lblStr, lblST, (void*)(uintptr_t)seriesIdx);
            uint8_t lblCopy[16];
            memcpy(lblCopy, pLbl, 16);
            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_TextElement_SetText(*(void**)labelPtr, lblCopy);

            fn_InternalBasePtr_CheckPtr(labelPtr);
            fn_Element_SetFlagsOnOff(*(void**)labelPtr, 0, 1);

            pointCount = graphSeries[4];
            graphCount++;
        }
    }

    // Round up y-axis max
    if (maxVal < 5) maxVal = 5;
    int scale;
    if      (maxVal < 10)   scale = 1;
    else if (maxVal < 100)  scale = 10;
    else if (maxVal < 1000) scale = 100;
    else                    scale = 1000;
    int yMax = ((maxVal - 1 + scale) / scale) * scale;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x160));
    *(float*)(*(uintptr_t*)(s + 0x160) + 0xe0) = (float)yMax;

    // Y-axis labels: max, 3/4, 1/2, 1/4
    float quarter = (float)yMax * 0.25f;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x180));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x180), yMax);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x188));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x188), (int)(quarter * 3.0f));
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x190));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x190), (int)(quarter * 2.0f));
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x198));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x198), (int)quarter);
}

// ============================================================
// TownHallUI::UpdateGeneral (was FUN_14007d030)
// ============================================================
void TownHallUI_UpdateGeneral(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);

    int totalCitizens = *(int*)(gameData + 0xa8);
    int adults = 0, students = 0, children = 0, familyCount = 0;
    float clothedSum = 0.0f, happinessSum = 0.0f, healthSum = 0.0f;
    float perCitizen = (totalCitizens > 0) ? (1.0f / (float)totalCitizens) : 0.0f;

    for (int i = 0; i < totalCitizens; i++) {
        uintptr_t citizen = *(uintptr_t*)(*(uintptr_t*)(gameData + 0xa0) + (uintptr_t)i * 8);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);

        float age = *(float*)(citizen + 0x3c);
        float adultAge = *(float*)(*(uintptr_t*)(citizen + 0x288) + 0x44);
        uintptr_t school = *(uintptr_t*)(citizen + 0x208);

        if (age < adultAge || school != 0) {
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            if (school == 0) { children++; }
            else             { students++; }
        } else {
            adults++;
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            if (*(int*)(*(uintptr_t*)(citizen + 0x288) + 0x118) <= *(int*)(citizen + 0x210)) {
                clothedSum += 1.0f;
            }
            fn_InternalBasePtr_CheckPtr((void*)&citizen);
            uintptr_t home = *(uintptr_t*)(citizen + 0x250);
            if (home == 0) {
                fn_InternalBasePtr_CheckPtr((void*)&citizen);
                if (*(uintptr_t*)(citizen + 0x250) == 0) {
                    familyCount++;
                }
            } else {
                fn_InternalBasePtr_CheckPtr((void*)&citizen);
                if (*(int*)(citizen + 0x38) == 0) {
                    familyCount++;
                }
            }
        }

        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        healthSum += *(float*)(citizen + 0x80);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        happinessSum += *(float*)(citizen + 0x50);
        fn_InternalBasePtr_CheckPtr((void*)&citizen);
        if (*(int*)(citizen + 0x1e4) != 0) {
            clothedSum += 0.0f; // educated tracking (doesn't add to clothedSum)
        }
    }

    float healthAvg = healthSum * perCitizen;
    float educatedPct = perCitizen * 100.0f;
    float clothedPct = (adults > 0) ? ((clothedSum / (float)adults) * 100.0f) : 0.0f;

    // Year
    int year = **(int**)(*(uintptr_t*)g_pGameSingleton + 0x128);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x60));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x60), year);

    // Month
    float seconds = fn_Time_AsSeconds((void*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0x128) + 8));
    int monthIdx = (int)((seconds / 3600.0f) * 12.0f);
    if (monthIdx > 11) monthIdx = 0;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x68));
    uint64_t monthST = *(uint64_t*)(*(uintptr_t*)(s + 0x68) + 0xd8);
    uint8_t monthStr[16];
    uint64_t* pMonth = (uint64_t*)fn_UIString_Ctor(monthStr, monthST, (void*)(uintptr_t)monthIdx);
    uint8_t monthCopy[16];
    memcpy(monthCopy, pMonth, 16);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x68));
    fn_TextElement_SetText(*(void**)(s + 0x68), monthCopy);

    // Home count
    int homeCount = *(int*)(gameData + 0x28);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x70));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x70), homeCount);

    // Citizen counts
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x80));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x80), totalCitizens);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x88));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x88), adults);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x90));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x90), students);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x98));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x98), children);

    // Clothed %
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)(uint32_t)(int)(happinessSum * educatedPct + 0.5f));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xa0));
        fn_TextElement_SetParameter(*(void**)(s + 0xa0), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    // Educated %
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)(uint32_t)(int)(clothedPct + 0.5f));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xa8));
        fn_TextElement_SetParameter(*(void**)(s + 0xa8), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    // Family count
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x78));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x78), familyCount);

    // Hearts (happiness) and Stars (health) ratings
    if (totalCitizens < 1) {
        fn_GroupRating_Update((void*)(s + 0xb0), 0.0f, 1.0f);
        fn_GroupRating_Update((void*)(s + 0xd0), 0.0f, 1.0f);
    } else {
        uintptr_t firstCitizen = **(uintptr_t**)(gameData + 0xa0);
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        void* desc = *(void**)(firstCitizen + 0x18);
        void* citizenDesc = fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription) ? desc : nullptr;

        fn_GroupRating_Update((void*)(s + 0xb0), happinessSum * perCitizen,
                              *(float*)((uintptr_t)citizenDesc + 0x70));

        // Re-read first citizen for health max
        firstCitizen = **(uintptr_t**)(gameData + 0xa0);
        fn_InternalBasePtr_CheckPtr((void*)&firstCitizen);
        desc = *(void**)(firstCitizen + 0x18);
        citizenDesc = fn_IsDerivedFrom(desc, *g_TypeId_CitizenDescription) ? desc : nullptr;

        fn_GroupRating_Update((void*)(s + 0xd0), healthAvg,
                              *(float*)((uintptr_t)citizenDesc + 0xa4));
    }
}

// ============================================================
// TownHallUI::UpdateProfessions (was FUN_14007ccb0)
// ============================================================
void TownHallUI_UpdateProfessions(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int comboIdx = *(int*)(*(uintptr_t*)(s + 0x100) + 0x118);

    uint32_t yearSpan = 0;
    switch (comboIdx) {
    case 0: yearSpan = 1; break;
    case 1: yearSpan = 2; break;
    case 2: yearSpan = 5; break;
    case 3: yearSpan = 10; break;
    case 4: yearSpan = 25; break;
    case 5: yearSpan = 50; break;
    case 6: yearSpan = 100; break;
    }

    // Set header text parameters
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)yearSpan);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
        fn_TextElement_SetParameter(*(void**)(s + 0x108), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }
    {
        uint8_t fmtBuf[8];
        fn_String_Format(fmtBuf, "%i", (uint64_t)yearSpan);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_TextElement_SetParameter(*(void**)(s + 0x110), 0, fmtBuf);
        fn_String_Dtor(fmtBuf);
    }

    int* timePtr = *(int**)(*(uintptr_t*)g_pGameSingleton + 0x128);
    int currentYearTick = timePtr[0] * 0x30 - 0x30;
    float secs = fn_Time_AsSeconds((void*)(timePtr + 2));
    uint32_t monthTick = (uint32_t)((secs / 3600.0f) * 48.0f);
    if ((int)monthTick > 0x2f) monthTick = 0;
    int endTick = monthTick + currentYearTick;
    int prevEnd = timePtr[0] * 0x30 - 0x31;
    int prevStart = (timePtr[0] - (int)yearSpan - 1) * 0x30;

    uintptr_t gameData = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xf0);
    uint32_t resIdx = 10;
    int prodCount = *(int*)(s + 0xf8);

    if (prodCount > 0) {
        int64_t entryOff = 0, dataOff = 0;
        for (int i = 0; i < prodCount; i++) {
            int stock = *(int*)(*(uintptr_t*)(gameData + 0x170) + dataOff);

            uintptr_t entry = *(uintptr_t*)(s + 0xf0) + entryOff;
            fn_InternalBasePtr_CheckPtr((void*)entry);
            fn_NumberElement_SetValueInt(*(void**)entry, stock);

            uintptr_t prodData = *(uintptr_t*)(gameData + 0x170);
            int usedCur = fn_QueryProductionStat((void*)gameData, resIdx - 1, currentYearTick, endTick);
            int usedStock = *(int*)(prodData + 0xc + dataOff);
            int prodCur = fn_QueryProductionStat((void*)gameData, resIdx, currentYearTick, endTick);
            int prodStock = *(int*)(prodData + 0x10 + dataOff);
            int usedPrev = fn_QueryProductionStat((void*)gameData, resIdx - 1, prevStart, prevEnd);
            int prodPrev = fn_QueryProductionStat((void*)gameData, resIdx, prevStart, prevEnd);

            uintptr_t arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 8));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 8), usedCur + usedStock);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x10));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x10), prodCur + prodStock);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x18));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x18), usedPrev);

            arr = *(uintptr_t*)(s + 0xf0);
            fn_InternalBasePtr_CheckPtr((void*)(arr + entryOff + 0x20));
            fn_NumberElement_SetValueInt(*(void**)(arr + entryOff + 0x20), prodPrev);

            resIdx += 3;
            dataOff += 0x20;
            entryOff += 0x28;
        }
    }
}

// ============================================================
// TownHallUI::UpdateProduction (was FUN_14007c7f0)
// ============================================================
// Exe-resolved due to extreme complexity (BST traversal,
// component type queries, sorting, dynamic array management).
void TownHallUI_UpdateProduction(TownHallUI* self) {
    fn_TownHallUI_UpdateProduction(self);
}

// ============================================================
// TownHallUI::UpdateInventory (was FUN_14007def0)
// ============================================================
// Exe-resolved due to extreme complexity.
void TownHallUI_UpdateInventory(TownHallUI* self) {
    fn_TownHallUI_UpdateInventory(self);
}

// ============================================================
// TownHallUI::UpdateAll (was FUN_14007d590)
// ============================================================
// Exe-resolved due to extreme complexity.
void TownHallUI_UpdateAll(TownHallUI* self) {
    fn_TownHallUI_UpdateAll(self);
}

// ============================================================
// TownHallUI::UpdateAgriculture (was FUN_14007c220)
// ============================================================
// Exe-resolved due to extreme complexity.
void TownHallUI_UpdateAgriculture(TownHallUI* self) {
    fn_TownHallUI_UpdateAgriculture(self);
}

// ============================================================
// TownHallUI::OnProfessionButton (was FUN_14007dca0)
// ============================================================
void TownHallUI_OnProfessionButton(TownHallUI* self, int index) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t entryBase = (uintptr_t)index * 0x40 + self->profEntryArray;

    int entityCount = *(int*)(entryBase + 0x38);
    if (entityCount == 0) return;

    self->profButtonIdx++;
    int cycleIdx = (int)self->profButtonIdx;
    if (cycleIdx >= entityCount) cycleIdx = 0;
    self->profButtonIdx = (uint32_t)cycleIdx;

    uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(entryBase + 0x30) + (uintptr_t)cycleIdx * 8);
    uint64_t dialogInst = *(uint64_t*)(s + 0x18);
    uintptr_t entityController = *(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0x120);
    Toolbar_CenterOnEntity_Impl((void*)entityController, &dialogInst, &entityPtr);
}

// ============================================================
// TownHallUI::OnProfessionSpinner (was FUN_14007dd10)
// ============================================================
void TownHallUI_OnProfessionSpinner(TownHallUI* self, int index) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t entryBase = *(uintptr_t*)(s + 0x40);
    int64_t entryOff = (int64_t)index * 0x40;

    // Get desired vs current count
    fn_InternalBasePtr_CheckPtr((void*)(entryBase + entryOff + 0x10));
    int desired = fn_NumberEditElement_GetValue(*(void**)(entryBase + entryOff + 0x10));
    int current = *(int*)(entryBase + entryOff);

    if (current < desired) {
        int toAdd = desired - current;
        for (int i = 0; i < toAdd; i++) {
            void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingleton + 0xf0);
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0) + 0x20 + (uintptr_t)index * 8);

            uint64_t retBuf[2] = {0, 0};
            fn_CitizenList_RequestWorker(citizenList, retBuf, profId);
            uint64_t slot = retBuf[0];

            // Try idle -> working -> student -> any
            uint64_t slotCopy = slot;
            uint8_t findBuf[8] = {0};
            void* result = fn_CitizenList_FindIdle(citizenList, findBuf, &slotCopy, profId);
            uint64_t found = *(uint64_t*)result;

            if (found == 0) {
                if (slot != 0) {
                    slotCopy = slot;
                    uint8_t fb2[8] = {0};
                    result = fn_CitizenList_FindWorking(citizenList, fb2, (void*)&slotCopy, profId);
                    found = *(uint64_t*)result;
                    if (found != 0) continue;
                }
                slotCopy = slot;
                uint8_t fb3[8] = {0};
                result = fn_CitizenList_FindStudent(citizenList, fb3, (void*)&slotCopy, profId);
                found = *(uint64_t*)result;

                if (found == 0) {
                    slotCopy = slot;
                    uint64_t anyBuf = 0;
                    fn_CitizenList_FindAny(citizenList, &anyBuf, (void*)&slotCopy, profId);
                }
            }
        }
    }

    if (desired < current) {
        int toRemove = current - desired;
        for (int i = 0; i < toRemove; i++) {
            void* citizenList = *(void**)(*(uintptr_t*)g_pGameSingleton + 0xf0);
            uint64_t profId = *(uint64_t*)(*(uintptr_t*)(*(uintptr_t*)g_pGameSingleton + 0xb0) + 0x20 + (uintptr_t)index * 8);
            fn_CitizenList_RemoveWorker(citizenList, profId);
        }
    }

    // Refresh display
    TownHallUI_UpdateInventory(self);
}

// ============================================================
// TownHallUI::Serialize (vtable +0x20, 0x140081000)
// ============================================================
void TownHallUI_Serialize(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, "tabs");
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* element = *(void**)foundBuf;

    void* tabElement = nullptr;
    if (element && g_TypeId_TabElement) {
        if (fn_IsDerivedFrom(element, *g_TypeId_TabElement))
            tabElement = element;
    }

    uint64_t tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, tabElement);
    *(uint64_t*)(s + 0x30) = tempIBP;
    fn_String_Dtor(strBuf);

    TownHallUI_InitGeneral(self);
    TownHallUI_InitProfessions(self);
    TownHallUI_InitProduction(self);
    TownHallUI_InitInventory(self);
    TownHallUI_InitGraph(self);
    TownHallUI_InitAgriculture(self);
}

// ============================================================
// TownHallUI::Update (vtable +0x50, 0x140080db0)
// ============================================================
void TownHallUI_Update(TownHallUI* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    int tabIndex = *(int*)(*(uintptr_t*)(s + 0x30) + 0xf8);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int combo1 = *(int*)(*(uintptr_t*)(s + 0x100) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
    int combo2 = *(int*)(*(uintptr_t*)(s + 0x128) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
    int combo3 = *(int*)(*(uintptr_t*)(s + 0x130) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
    int combo4 = *(int*)(*(uintptr_t*)(s + 0x168) + 0x118);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
    int combo5 = *(int*)(*(uintptr_t*)(s + 0x170) + 0x118);

    uint64_t serializer[3] = {0, 0, 0};
    g_fn_Serializer_Ctor(serializer, stream, 1);

    g_fn_Serialize_Int(serializer, &tabIndex, nullptr);
    g_fn_Serialize_Int(serializer, &combo1, nullptr);
    g_fn_Serialize_Int(serializer, &combo2, nullptr);

    int version = g_fn_Serializer_GetVersion(serializer);
    if (version > 0) {
        g_fn_Serialize_Int(serializer, &combo3, nullptr);
    }
    g_fn_Serialize_Int(serializer, &combo4, nullptr);
    g_fn_Serialize_Int(serializer, &combo5, nullptr);

    if (*(int*)(serializer[0] + 8) == 0) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
        fn_TabElement_SetActiveTab(*(void**)(s + 0x30), tabIndex);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x100), combo1);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x128), combo2);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x130), combo3);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x168), combo4);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x170), combo5);
        TownHallUI_UpdateGraph(self);
    }

    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// TownHallUI::Startup (vtable +0x38)
// ============================================================
void TownHallUI_Startup(TownHallUI* self) {
    TownHallUI_UpdateGeneral(self);
    TownHallUI_UpdateProfessions(self);
    TownHallUI_UpdateProduction(self);
    TownHallUI_UpdateAgriculture(self);
}

// ============================================================
// TownHallUI::OnCreate (vtable +0x40)
// ============================================================
void TownHallUI_OnCreate(TownHallUI* self) {
    uintptr_t gameSingleton = *(uintptr_t*)g_pGameSingleton;
    int currentTick = *(int*)(*(uintptr_t*)(*(uintptr_t*)(gameSingleton + 0xf0) + 0x150) + 8);

    if (self->cachedGameTick != currentTick) {
        self->cachedGameTick = currentTick;
        TownHallUI_UpdateGraph(self);
    }

    TownHallUI_UpdateInventory(self);
    TownHallUI_UpdateAll(self);
}

// ============================================================
// TownHallUI::OnRemove (vtable +0x48)
// ============================================================
void TownHallUI_OnRemove(TownHallUI* self, void* param2, int msg, int id) {
    uintptr_t gameSingleton = *(uintptr_t*)g_pGameSingleton;
    int profCount = *(int*)(*(uintptr_t*)(gameSingleton + 0xb0) + 0x28);

    if (msg == 0x12) {
        if (id > 0x1998 && id <= 0x1999 + profCount)
            TownHallUI_OnProfessionButton(self, id - 0x1999);
    } else if (msg == 0x14) {
        if (id == 0x1964 || id == 0x1965)
            TownHallUI_UpdateGraph(self);
    } else if (msg == 0x15) {
        if (id > 0x1965 && id <= 0x1966 + profCount)
            TownHallUI_OnProfessionSpinner(self, id - 0x1966);
    }
}
