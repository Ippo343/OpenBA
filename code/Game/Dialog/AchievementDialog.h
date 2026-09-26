#pragma once
// ============================================================================
// code/Game/Dialog/AchievementDialog.h — ENDGAME B.3.8 block DLG
//
// The Achievements list opened from the main / pause menu.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "AchievementDialog", parent "UI::DialogInstance", ti 0x1401c4c98
//   create 0x140010320 (MemAlloc(0xd0, 8)), createInPlace 0x140010390
//   ctors 0x14005db90 / 0x14005dc00; MainMenuDialog::OpenAchievement
//   0x1400b34c0 inlines one (+0xb8, +0xc0, +0xc8 = 0)
//   table 0x14015b020, 10 slots:
//     [0] 0x1400103f0 scalar deleting dtor: MemFree(+0xc0), +0xc0/+0xc8 = 0,
//         ~DialogInstance, MemFree on &1
//     [1] TypeName  [2] Type  [3] DialogInstance::Persist (import)
//     [4] 0x14005d490 OnOpened  [5] DialogInstance::OnClosed
//     [6] 0x14005d240 OnUpdateLayout  [7] 0x14005d480 OnUpdate `RET 0`
//     [8] 0x14005d440 OnElementEvent  [9] DialogInstance::OnSerializeSettings
//   sizeof 0xd0. There is NO secondary vtable: Dialogs.h's old
//   `SecondaryVtable = 0x14015b070` was AttendanceUI's table (D9 #14).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class AchievementDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(AchievementDialog)

public:
    AchievementDialog();                                            // create 0x140010320
    AchievementDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                      Object::InternalPtr<UI::DialogInstance> parent);  // 0x1400b34c0 inlined

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x1400103f0
    void OnOpened() override;                                      // [4] 0x14005d490
    // [5] OnClosed: DialogInstance's (SaveSettings), inherited (B38 D3.1).
    void OnUpdateLayout() override;                                // [6] 0x14005d240
    void OnUpdate(const System::Time& time) override;              // [7] 0x14005d480 RET
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x14005d440

protected:
    ~AchievementDialog();

public:
    uint64_t dataB8;           // +0xB8: "textComplete" element
    uint64_t dataC0;           // +0xC0: entry array data (stride 0x20)
    uint64_t dataC8;           // +0xC8: entry array count / capacity
};

static_assert(offsetof(AchievementDialog, dataB8) == 0xB8, "AchievementDialog +0xb8");
static_assert(offsetof(AchievementDialog, dataC0) == 0xC0, "AchievementDialog +0xc0");
static_assert(offsetof(AchievementDialog, dataC8) == 0xC8, "AchievementDialog +0xc8");
static_assert(sizeof(AchievementDialog) == 0xD0, "AchievementDialog is 0xd0 (create 0x140010320)");
static_assert(alignof(AchievementDialog) == 8, "AchievementDialog is qword-aligned");
