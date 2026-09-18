// engine/src/ui/achievementdialog.cpp
// Clean reimplementation of AchievementDialog game logic.
//
// AchievementDialog (0xD0 bytes) — achievement progress display.
// Extends DialogInstance (0xB8 base).
// +0x20 = parent element, +0x28 = entity/data source, +0x30 = progress element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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
    uint64_t btnResult = 0;
    {
        System::String nameStr("buttonOk");
        fn_DialogInstance_FindElement((void*)s, &btnResult, &nameStr);
    }
    void* buttonOk = (void*)btnResult;
    fn_Element_SetElementId(buttonOk, 100);
    *(uint32_t*)((uintptr_t)buttonOk + 0xC0) = 8;

    // Get AchievementManager singleton
    void* manager = **g_ppAchievementManager;

    // Find "tableAchievement" element
    uint64_t tableResult = 0;
    {
        System::String nameStr("tableAchievement");
        fn_DialogInstance_FindElement((void*)s, &tableResult, &nameStr);
    }
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
        uint64_t iconResult = 0;
        {
            System::String nameStr("imageIcon");
            fn_FindElement(rowElem, &iconResult, &nameStr);
        }
        void* imageIcon = (void*)iconResult;

        // imageStar
        uint64_t starResult = 0;
        {
            System::String nameStr("imageStar");
            fn_FindElement(rowElem, &starResult, &nameStr);
        }
        void* imageStar = (void*)starResult;

        // textTitle
        uint64_t titleResult = 0;
        {
            System::String nameStr("textTitle");
            fn_FindElement(rowElem, &titleResult, &nameStr);
        }
        void* textTitle = (void*)titleResult;

        // textDescription
        uint64_t descResult = 0;
        {
            System::String nameStr("textDescription");
            fn_FindElement(rowElem, &descResult, &nameStr);
        }
        void* textDesc = (void*)descResult;

        // Get achievement object at index i
        uint8_t achievBuf[8] = {0};
        fn_AchievementManager_GetAchievement(manager, achievBuf, i);
        void* achievement = *(void**)achievBuf;

        // Set sprite on imageIcon from achievement sprite sheet + sprite name
        System::String spriteName;
        fn_Achievement_GetSpriteName(achievement, &spriteName);
        uint64_t spriteSheet = *(uint64_t*)((uintptr_t)achievement + 0x38);
        uint8_t spriteBuf[16];
        fn_Sprite_Ctor(spriteBuf, spriteSheet, &spriteName);
        fn_ImageElement_SetSprite(imageIcon, spriteBuf);

        // Hide imageStar initially (Slot6 will show it if awarded)
        fn_Element_SetFlagsOnOff(imageStar, 1, 0);

        // Set title text
        System::String titleText;
        fn_Achievement_GetTitle(achievement, &titleText);
        uint8_t titleStrBuf[16];
        fn_UIString_Ctor(titleStrBuf, *(uint64_t*)((uintptr_t)achievement + 0x20),
                         &titleText);
        fn_TextElement_SetText(textTitle, titleStrBuf);

        // Set description text
        uint8_t descStrBuf[16];
        System::String descText;
        fn_Achievement_GetDescription(achievement, &descText);
        fn_UIString_Ctor(descStrBuf, *(uint64_t*)((uintptr_t)achievement + 0x20),
                         &descText);
        fn_TextElement_SetText(textDesc, descStrBuf);

        // Push 4-pointer entry (stride 0x20) to dynamic array at +0xC0.
        // B.3.2 bridge: <AchievementDialog>+0xC0 is Collection::Array<{imageIcon,
        // imageStar, textTitle, textDesc}> (stride 0x20, count +0xC8, capacity +0xCC)
        // inside a struct §B.2 has not converted, so it is reached at its offset.
        // The old hand-rolled copy guarded with `if (newCount != count)` — that is
        // retail's `if (newSize == oldSize) return;` early return, which
        // rtcoll::SetSizeCore already performs, so it is not carried forward.
        auto& entries = GameColl::ArrayAt<GameColl::Bytes<0x20, 8>>(s, 0xC0);
        entries.SetSize(entries.GetSize() + 1,
                         GameColl::Bytes<0x20, 8>((uint64_t)imageIcon, (uint64_t)imageStar,
                                        (uint64_t)textTitle, (uint64_t)textDesc));
    }

    // Find "textComplete" element, store at +0xB8
    uint64_t completeResult = 0;
    {
        System::String nameStr("textComplete");
        fn_DialogInstance_FindElement((void*)s, &completeResult, &nameStr);
    }
    *(uint64_t*)(s + 0xB8) = completeResult;
}

// ============================================================
// AchievementDialog::Slot7 (vtable +0x38, exe 0x14005D480): RET 0 (3 bytes)
// ============================================================
// No-op handler; returns 0.
int AchievementDialog_Slot7(AchievementDialog* self) {
    (void)self;
    return 0;
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
    System::String pctStr = System::String::Format("%i", (uint64_t)(uint32_t)(int)pct);

    void* pctText = (void*)*(uint64_t*)(s + 0xB8);
    fn_TextElement_SetParameter(pctText, 0, &pctStr);
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
    new (&basePtrVal) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    *(uint64_t*)(s + 0x30) = basePtrVal;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

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
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    // Find AttendanceComponent on entity
    uint64_t attComp = 0;
    fn_FindComponent_Attendance((void*)entityBasePtr, &attComp);

    // Read current attendance count
    int count = *(int32_t*)(attComp + 0x30);

    // Set progress bar value at element+0x128
    void* progressElem = (void*)*(uint64_t*)(s + 0x30);
    *(float*)((uintptr_t)progressElem + 0x128) = (float)count;

    // Format count text and set as progress parameter
    System::String countStr = System::String::Format("%i", (uint64_t)(uint32_t)count);
    fn_ProgressElement_SetProgressTextParameter(progressElem, 0, &countStr);
}

// ============================================================
// AchievementDialog::Slot8 (vtable +0x40, exe 0x14005D440) -- OnCommand
// ============================================================
// Byte-identical to GoalDialog::Slot8 (0x1400697C0): on a button click (msg 0x12)
// from buttonOk (id 100), wrap self in an InternalBasePtr and ask the UserInterface
// at self+0x10 to close this dialog. Without it the Achievements panel had no way to
// dismiss itself -- the Setup body sets buttonOk id 100 and nothing consumed it.
void AchievementDialog_Slot8(AchievementDialog* self, void* /*param2*/,
                             int param3, int param4) {
    if (param3 == 0x12 && param4 == 100) {
        uint8_t selfIBP[16];
        new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
    }
}

// ============================================================
// WARNING -- AchievementDialog_Slot14 / _Slot17 above are MISATTRIBUTED
// ============================================================
// Their retail addresses (0x14005DD40 / 0x14005DC50) are .rdata entries 14 and 17
// counting from 0x14015b020, but AchievementDialog's own vtable ENDS at index 9.
// Those two are slots 4 and 7 of the NEXT table, which starts at 0x14015b070 and is
// AttendanceUI's (its name string sits right after it in .rdata). Both bodies read
// +0x20/+0x28/+0x30 and an AttendanceComponent -- DialogController layout, not
// DialogInstance layout -- so on a real AchievementDialog they would read the wrong
// fields. The SAME two retail functions are already ported under their correct names
// as AttendanceUI_Slot4 / AttendanceUI_Slot7 (attendanceui.cpp), and it is those
// copies that are wired. Neither of the two below is installed anywhere; they are
// kept only so the addresses stay searchable. Do not wire them into the
// AchievementDialog vtable (menu_dialog_standalone_vtables.cpp).
