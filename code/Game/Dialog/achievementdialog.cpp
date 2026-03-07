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
// AchievementDialog::Slot4 (vtable +0x20) — Setup/populate achievement table
// ============================================================
// Finds "buttonOk" (ID 100), "tableAchievement", clones template rows
// for each achievement, finds child elements (imageIcon, imageStar,
// textTitle, textDescription), sets sprite/text, pushes 4-ptr entries
// to dynamic array at +0xC0. Finally finds "textComplete" and stores at +0xB8.
//
// Original: 0x14005D490.

void AchievementDialog_Slot4(AchievementDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "buttonOk" element, set ID 100, set +0xC0 flags = 8
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "buttonOk");
    uint64_t btnResult = 0;
    fn_DialogInstance_FindElement((void*)s, &btnResult, nameBuf);
    fn_String_Dtor(nameBuf);
    void* buttonOk = (void*)btnResult;
    fn_Element_SetElementId(buttonOk, 100);
    *(uint32_t*)((uintptr_t)buttonOk + 0xC0) = 8;

    // Get AchievementManager singleton
    void* manager = **g_ppAchievementManager;

    // Find "tableAchievement" element
    fn_String_Ctor_CStr(nameBuf, "tableAchievement");
    uint64_t tableResult = 0;
    fn_DialogInstance_FindElement((void*)s, &tableResult, nameBuf);
    fn_String_Dtor(nameBuf);
    void* tableElem = (void*)tableResult;

    // Get achievement count
    int achievementCount = fn_AchievementManager_GetAchievementCount(manager);

    // Get dialog template for cloning rows
    void* dialog = *(void**)(s + 0x18);

    // For each achievement: clone template, populate elements, push to array
    for (int i = 0; i < achievementCount; i++) {
        // Clone template element from dialog
        uint8_t templateBuf[16] = {0};
        fn_Dialog_GetTemplate(dialog, templateBuf, 0);
        void* elemDesc = *(void**)templateBuf;

        uint8_t newElemBuf[16] = {0};
        if (elemDesc) {
            void** vtbl = *(void***)elemDesc;
            typedef void (*CreateFn)(void*, void*, void*);
            ((CreateFn)vtbl[8])(elemDesc, newElemBuf, dialog);
        }
        void* rowElem = *(void**)newElemBuf;

        // Set as table child at index i
        fn_TableElement_SetChild2(tableElem, newElemBuf, i);

        // Find child elements in the cloned row
        // imageIcon
        fn_String_Ctor_CStr(nameBuf, "imageIcon");
        uint64_t iconResult = 0;
        fn_FindElement(rowElem, &iconResult, nameBuf);
        fn_String_Dtor(nameBuf);
        void* imageIcon = (void*)iconResult;

        // imageStar
        fn_String_Ctor_CStr(nameBuf, "imageStar");
        uint64_t starResult = 0;
        fn_FindElement(rowElem, &starResult, nameBuf);
        fn_String_Dtor(nameBuf);
        void* imageStar = (void*)starResult;

        // textTitle
        fn_String_Ctor_CStr(nameBuf, "textTitle");
        uint64_t titleResult = 0;
        fn_FindElement(rowElem, &titleResult, nameBuf);
        fn_String_Dtor(nameBuf);
        void* textTitle = (void*)titleResult;

        // textDescription
        fn_String_Ctor_CStr(nameBuf, "textDescription");
        uint64_t descResult = 0;
        fn_FindElement(rowElem, &descResult, nameBuf);
        fn_String_Dtor(nameBuf);
        void* textDesc = (void*)descResult;

        // Get achievement object at index i
        uint8_t achievBuf[8] = {0};
        fn_AchievementManager_GetAchievement(manager, achievBuf, i);
        void* achievement = *(void**)achievBuf;

        // Set sprite on imageIcon from achievement sprite name
        uint8_t spriteNameBuf[8];
        fn_Achievement_GetSpriteName(achievement, spriteNameBuf);
        uint8_t spriteBuf[16];
        fn_Sprite_Ctor(spriteBuf, *(uint64_t*)spriteNameBuf, nullptr);
        fn_ImageElement_SetSprite(imageIcon, spriteBuf);
        fn_String_Dtor(spriteNameBuf);

        // Hide imageStar initially (Slot6 will show it if awarded)
        fn_Element_SetFlagsOnOff(imageStar, 1, 0);

        // Set title text
        uint8_t titleStrBuf[16];
        fn_UIString_Ctor(titleStrBuf, *(uint64_t*)((uintptr_t)achievement + 0x20),
                         nullptr);
        uint8_t titleTextBuf[8];
        fn_Achievement_GetTitle(achievement, titleTextBuf);
        fn_UIString_Ctor(titleStrBuf, *(uint64_t*)((uintptr_t)achievement + 0x20),
                         titleTextBuf);
        fn_TextElement_SetText(textTitle, titleStrBuf);
        fn_String_Dtor(titleTextBuf);

        // Set description text
        uint8_t descStrBuf[16];
        uint8_t descTextBuf[8];
        fn_Achievement_GetDescription(achievement, descTextBuf);
        fn_UIString_Ctor(descStrBuf, *(uint64_t*)((uintptr_t)achievement + 0x20),
                         descTextBuf);
        fn_TextElement_SetText(textDesc, descStrBuf);
        fn_String_Dtor(descTextBuf);

        // Push 4-pointer entry (stride 0x20) to dynamic array at +0xC0
        // Entry: {imageIcon, imageStar, textTitle, textDesc}
        uint32_t count = *(uint32_t*)(s + 0xC8);
        uint32_t newCount = count + 1;
        if (newCount != count) {
            void* oldData = *(void**)(s + 0xC0);
            *(uint32_t*)(s + 0xC8) = newCount;

            if ((int32_t)*(uint32_t*)(s + 0xCC) < (int32_t)newCount) {
                uint32_t newCap = newCount;
                if (count != 0) newCap = newCount * 3;
                *(uint32_t*)(s + 0xCC) = newCap;
                void* newData = fn_MemAlloc(g_pMemAllocSingleton,
                    (int64_t)(int32_t)newCap * 0x20, 8);
                *(void**)(s + 0xC0) = newData;
            }

            uint32_t copyStart = 0;
            if (oldData == *(void**)(s + 0xC0)) {
                copyStart = count;
            }

            for (uint32_t j = copyStart; j < newCount; j++) {
                uint64_t* dst = (uint64_t*)(*(uintptr_t*)(s + 0xC0) + (uintptr_t)j * 0x20);
                if (dst) {
                    if ((int32_t)j < (int32_t)count) {
                        // Copy from old data
                        uint64_t* src = (uint64_t*)((uintptr_t)oldData + (uintptr_t)j * 0x20);
                        dst[0] = src[0];
                        dst[1] = src[1];
                        dst[2] = src[2];
                        dst[3] = src[3];
                    } else {
                        // New entry: {imageIcon, imageStar, textTitle, textDesc}
                        dst[0] = (uint64_t)imageIcon;
                        dst[1] = (uint64_t)imageStar;
                        dst[2] = (uint64_t)textTitle;
                        dst[3] = (uint64_t)textDesc;
                    }
                }
            }

            if (oldData != *(void**)(s + 0xC0)) {
                fn_MemFreeStatic(oldData);
            }
        }
    }

    // Find "textComplete" element, store at +0xB8
    fn_String_Ctor_CStr(nameBuf, "textComplete");
    uint64_t completeResult = 0;
    fn_DialogInstance_FindElement((void*)s, &completeResult, nameBuf);
    fn_String_Dtor(nameBuf);
    *(uint64_t*)(s + 0xB8) = completeResult;
}

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
