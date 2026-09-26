// engine/src/ui/achievementdialog.cpp
// Clean reimplementation of AchievementDialog game logic.
//
// AchievementDialog (0xD0 bytes) — achievement progress display.
// Extends DialogInstance (0xB8 base).
// +0x20 = parent element, +0x28 = entity/data source, +0x30 = progress element.
//
// Decompiled from Application-x64.exe.

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ProgressElement.h"   // ENDGAME B.3.6 UI callers (K2)
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
#include "object/External.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
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
        ::new (&btnResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)s)->FindElement(nameStr));  // B.3.6 bridge
    }
    void* buttonOk = (void*)btnResult;
    reinterpret_cast<UI::Element*>(buttonOk)->SetElementId(100);  // B.3.6 bridge
    *(uint32_t*)((uintptr_t)buttonOk + 0xC0) = 8;

    // Get AchievementManager singleton
    void* manager = **g_ppAchievementManager;

    // Find "tableAchievement" element
    uint64_t tableResult = 0;
    {
        System::String nameStr("tableAchievement");
        ::new (&tableResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)s)->FindElement(nameStr));  // B.3.6 bridge
    }
    void* tableElem = (void*)tableResult;

    // Get achievement count
    int achievementCount = reinterpret_cast<const Achievement::AchievementManager*>(manager)->GetAchievementCount();  // B.3.7 bridge

    // Get dialog template for cloning rows
    void* dialog = *(void**)(s + 0x18);

    // For each achievement: clone template, populate elements, push to array
    for (int i = 0; i < achievementCount; i++) {
        // Clone template element from dialog
        uint8_t templateBuf[16] = {0};
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(dialog)->GetTemplate(0));  // B.3.6 bridge
        void* elemDesc = *(void**)templateBuf;

        uint8_t newElemBuf[16] = {0};
        if (elemDesc) {
            reinterpret_cast<const UI::ElementDescription*>(elemDesc)->CreateInstance(  // B.3.6 bridge
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(newElemBuf),
                reinterpret_cast<const UI::Dialog*>(dialog));
        }
        void* rowElem = *(void**)newElemBuf;

        // Set as table child at index i
        reinterpret_cast<UI::TableElement*>(tableElem)->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(newElemBuf), i);  // B.3.6 bridge

        // Find child elements in the cloned row
        // imageIcon
        uint64_t iconResult = 0;
        {
            System::String nameStr("imageIcon");
            ::new (&iconResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->FindElement(nameStr));  // B.3.6 bridge
        }
        void* imageIcon = (void*)iconResult;

        // imageStar
        uint64_t starResult = 0;
        {
            System::String nameStr("imageStar");
            ::new (&starResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->FindElement(nameStr));  // B.3.6 bridge
        }
        void* imageStar = (void*)starResult;

        // textTitle
        uint64_t titleResult = 0;
        {
            System::String nameStr("textTitle");
            ::new (&titleResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->FindElement(nameStr));  // B.3.6 bridge
        }
        void* textTitle = (void*)titleResult;

        // textDescription
        uint64_t descResult = 0;
        {
            System::String nameStr("textDescription");
            ::new (&descResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(rowElem)->FindElement(nameStr));  // B.3.6 bridge
        }
        void* textDesc = (void*)descResult;

        // Get achievement object at index i
        uint8_t achievBuf[8] = {0};
        ::new (achievBuf) Object::ExternalPtr<Achievement::Achievement>(reinterpret_cast<const Achievement::AchievementManager*>(manager)->GetAchievement(i));  // B.3.7 bridge
        void* achievement = *(void**)achievBuf;

        // Set sprite on imageIcon from achievement sprite sheet + sprite name
        System::String spriteName;
        ::new (&spriteName) System::String(reinterpret_cast<const Achievement::Achievement*>(achievement)->GetSpriteName());  // B.3.7 bridge
        uint64_t spriteSheet = *(uint64_t*)((uintptr_t)achievement + 0x38);
        uint8_t spriteBuf[16];
        ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheet)), spriteName);  // B.3.6 bridge
        reinterpret_cast<UI::ImageElement*>(imageIcon)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge

        // Hide imageStar initially (Slot6 will show it if awarded)
        reinterpret_cast<UI::Element*>(imageStar)->SetFlagsOnOff(1, 0);  // B.3.6 bridge

        // Set title text
        System::String titleText;
        ::new (&titleText) System::String(reinterpret_cast<const Achievement::Achievement*>(achievement)->GetTitle());  // B.3.7 bridge
        uint8_t titleStrBuf[16];
        ::new (titleStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)achievement + 0x20))), titleText);  // B.3.6 bridge
        reinterpret_cast<UI::TextElement*>(textTitle)->SetText(reinterpret_cast<const UI::String*>(titleStrBuf));  // B.3.6 bridge

        // Set description text
        uint8_t descStrBuf[16];
        System::String descText;
        ::new (&descText) System::String(reinterpret_cast<const Achievement::Achievement*>(achievement)->GetDescription());  // B.3.7 bridge
        ::new (descStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)achievement + 0x20))), descText);  // B.3.6 bridge
        reinterpret_cast<UI::TextElement*>(textDesc)->SetText(reinterpret_cast<const UI::String*>(descStrBuf));  // B.3.6 bridge

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
        ::new (&completeResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>((void*)s)->FindElement(nameStr));  // B.3.6 bridge
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

        bool awarded = reinterpret_cast<const Achievement::AchievementManager*>(manager)->IsAchievementAwarded(i);  // B.3.7 bridge
        if (awarded) awardedCount++;

        uint32_t primaryColor   = awarded ? 0xFFE0E0E0u : 0xFF303030u;
        uint32_t secondaryColor = awarded ? 0xFFC0C0C0u : 0xFFB0B0B0u;

        // Check element at entry+0x08: show if awarded, hide if not
        void* checkElem = *(void**)(entryArray + entryOff + 0x08);
        reinterpret_cast<UI::Element*>(checkElem)->SetFlagsOnOff(0, awarded ? 1 : 0);  // B.3.6 bridge

        // Title element at entry+0x00: set primary color via vtable[10]
        void* titleElem = *(void**)(entryArray + entryOff);
        reinterpret_cast<UI::Element*>(titleElem)->SetColor(primaryColor);  // B.3.6 bridge

        // Desc element at entry+0x10: set secondary color via vtable[10]
        void* descElem = *(void**)(entryArray + entryOff + 0x10);
        reinterpret_cast<UI::Element*>(descElem)->SetColor(secondaryColor);  // B.3.6 bridge

        // Extra element at entry+0x18: set secondary color via vtable[10]
        void* extraElem = *(void**)(entryArray + entryOff + 0x18);
        reinterpret_cast<UI::Element*>(extraElem)->SetColor(secondaryColor);  // B.3.6 bridge
    }

    // Format percentage: (awarded / total) * 100 + 0.5 (rounding)
    float pct = ((float)awardedCount / (float)count) * 100.0f + 0.5f;
    System::String pctStr = System::String::Format("%i", (uint64_t)(uint32_t)(int)pct);

    void* pctText = (void*)*(uint64_t*)(s + 0xB8);
    reinterpret_cast<UI::TextElement*>(pctText)->SetParameter(0, pctStr);  // B.3.6 bridge
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
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElementType(*g_TypeId_ProgressElement));  // B.3.6 bridge

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
    reinterpret_cast<UI::ProgressElement*>(progressElem)->SetProgressTextParameter(0, countStr);  // B.3.6 bridge
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
        reinterpret_cast<UI::UserInterface*>(*(void**)((uintptr_t)self + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
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
