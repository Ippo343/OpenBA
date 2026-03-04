// engine/src/ui/achievementdialog.cpp
// Clean reimplementation of AchievementDialog game logic.
//
// AchievementDialog (0xD0 bytes) — achievement progress display.
// Extends DialogInstance (0xB8 base).
// +0x20 = parent element, +0x28 = entity/data source, +0x30 = progress element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// AchievementDialog::Slot6 (vtable +0x30) — Update achievement display
// ============================================================
// Iterates achievement entries at +0xC0 (stride 0x20), count at +0xC8.
// For each: checks IsAchievementAwarded, sets entry colors and check visibility.
// Calculates award percentage, formats as text on +0xB8.
//
// Entry layout (stride 0x20):
//   +0x00: Element* titleElement (set primary color)
//   +0x08: Element* checkElement (show/hide via SetFlagsOnOff)
//   +0x10: Element* descElement (set secondary color)
//   +0x18: Element* extraElement (set secondary color)
//
// Original: 0x14005D240.

void AchievementDialog_Slot6(AchievementDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get AchievementManager singleton (double-indirect via .rdata dispatch)
    void* manager = **g_ppAchievementManager;

    int count = *(int*)(s + 0xC8);
    uintptr_t entryArray = *(uintptr_t*)(s + 0xC0);

    int awardedCount = 0;

    for (int i = 0; i < count; i++) {
        uintptr_t entryOff = (uintptr_t)i * 0x20;

        bool awarded = fn_AchievementManager_IsAchievementAwarded(manager, i);
        if (awarded) awardedCount++;

        uint32_t primaryColor   = awarded ? 0xFFE0E0E0u : 0xFF303030u;
        uint32_t secondaryColor = awarded ? 0xFFC0C0C0u : 0xFFB0B0B0u;

        // Check element at entry+0x08: show if awarded, hide if not
        void* checkElem = *(void**)(entryArray + entryOff + 0x08);
        fn_Element_SetFlagsOnOff(checkElem, 0, awarded ? 1 : 0);

        // Title element at entry+0x00: set primary color via vtable[10]
        void* titleElem = *(void**)(entryArray + entryOff);
        typedef void (*fn_SetColor_t)(void*, uint32_t);
        void** titleVt = *(void***)titleElem;
        ((fn_SetColor_t)titleVt[10])(titleElem, primaryColor);

        // Desc element at entry+0x10: set secondary color via vtable[10]
        void* descElem = *(void**)(entryArray + entryOff + 0x10);
        void** descVt = *(void***)descElem;
        ((fn_SetColor_t)descVt[10])(descElem, secondaryColor);

        // Extra element at entry+0x18: set secondary color via vtable[10]
        void* extraElem = *(void**)(entryArray + entryOff + 0x18);
        void** extraVt = *(void***)extraElem;
        ((fn_SetColor_t)extraVt[10])(extraElem, secondaryColor);
    }

    // Format percentage: (awarded / total) * 100 + 0.5 (rounding)
    float pct = ((float)awardedCount / (float)count) * 100.0f + 0.5f;
    uint8_t textBuf[16];
    fn_String_Format((char*)textBuf, "%i", (uint64_t)(uint32_t)(int)pct);

    void* pctText = (void*)*(uint64_t*)(s + 0xB8);
    fn_TextElement_SetParameter(pctText, 0, textBuf);
    fn_String_Dtor(textBuf);
}

// ============================================================
// AchievementDialog::Slot14 (vtable +0x70) — Initialize progress element
// ============================================================
// Finds a ProgressElement by type in the parent element at +0x20,
// stores at +0x30. Then gets entity → AttendanceComponent → sub-object
// at comp+0x18, reads capacity at sub+0x20, sets as progress max value.
//
// Original: 0x14005DD40.

void AchievementDialog_Slot14(AchievementDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find ProgressElement by type in parent element at +0x20
    void* parentElem = (void*)*(uint64_t*)(s + 0x20);
    uint64_t findResult = 0;
    fn_Element_FindElementType(parentElem, &findResult, *g_TypeId_ProgressElement);

    // Store at +0x30 (skip IsDerivedFrom, null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    fn_InternalBasePtr_Ctor(&basePtrVal, checked);
    *(uint64_t*)(s + 0x30) = basePtrVal;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find AttendanceComponent on entity
    uint64_t attComp = 0;
    fn_FindComponent_Attendance((void*)entityBasePtr, &attComp);

    // Get sub-object at comp+0x18 (attendance capacity sub-component)
    void* subObj = *(void**)(attComp + 0x18);

    // Set progress max value: element+0x124 = (float)sub+0x20
    void* progressElem = (void*)*(uint64_t*)(s + 0x30);
    *(float*)((uintptr_t)progressElem + 0x124) = (float)*(int32_t*)((uintptr_t)subObj + 0x20);
}

// ============================================================
// AchievementDialog::Slot17 (vtable +0x88) — Update progress display
// ============================================================
// Gets entity → AttendanceComponent, reads current count at comp+0x30,
// sets progress bar value at element+0x128, formats percentage text.
//
// Original: 0x14005DC50.

void AchievementDialog_Slot17(AchievementDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find AttendanceComponent on entity
    uint64_t attComp = 0;
    fn_FindComponent_Attendance((void*)entityBasePtr, &attComp);

    // Read current attendance count
    int count = *(int32_t*)(attComp + 0x30);

    // Set progress bar value at element+0x128
    void* progressElem = (void*)*(uint64_t*)(s + 0x30);
    *(float*)((uintptr_t)progressElem + 0x128) = (float)count;

    // Format count text and set as progress parameter
    uint8_t textBuf[16];
    fn_String_Format((char*)textBuf, "%i", (uint64_t)(uint32_t)count);
    fn_ProgressElement_SetProgressTextParameter(progressElem, 0, textBuf);
    fn_String_Dtor(textBuf);
}
