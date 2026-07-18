#pragma once
// Dialogs.h -- Binary-compatible struct layouts for UI dialog controller classes.
// Application-x64.exe, build 170910.
//
// All dialog classes extend DialogInstance (0xB8 base).
// These are minimal structs; field names are
// placeholders until fully reverse-engineered.

#include <cstdint>
#include <cstddef>

// Base DialogInstance (from Runtime-x64.dll)
// Size: 0xB8 bytes. We treat it as opaque.

struct AchievementDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    void*   field_B8;         // +0xB8: unknown
    void*   field_C0;         // +0xC0: MemAlloc'd data (freed in dtor)
    uint8_t field_C8[8];      // +0xC8: unknown
};
static_assert(sizeof(AchievementDialog) == 0xD0, "AchievementDialog size mismatch");

struct AutoSaveDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[0x18];   // +0xB8: unknown (0x18 bytes extra)
};
static_assert(sizeof(AutoSaveDialog) == 0xD0, "AutoSaveDialog size mismatch");

struct EventPopupDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[8];      // +0xB8: unknown
};
static_assert(sizeof(EventPopupDialog) == 0xC0, "EventPopupDialog size mismatch");

struct FollowDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    void*   field_B8;         // +0xB8: InternalBasePtr (entity being followed)
    uint8_t field_C0[0x18];   // +0xC0: unknown
};
static_assert(sizeof(FollowDialog) == 0xD8, "FollowDialog size mismatch");

struct GoalDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[0x10];   // +0xB8: unknown
};
static_assert(sizeof(GoalDialog) == 0xC8, "GoalDialog size mismatch");

struct LoaderDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    void*   field_B8;         // +0xB8: loading image element ptr
    float   field_C0;         // +0xC0: rotation angle
    uint8_t field_C4[4];      // +0xC4: padding
};
static_assert(sizeof(LoaderDialog) == 0xC8, "LoaderDialog size mismatch");

struct MainMenuDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    void*   field_B8;         // +0xB8: some ptr (checked in Slot5)
    uint8_t field_C0[8];      // +0xC0: unknown
};
static_assert(sizeof(MainMenuDialog) == 0xC8, "MainMenuDialog size mismatch");

struct MessageDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[8];      // +0xB8: unknown
};
static_assert(sizeof(MessageDialog) == 0xC0, "MessageDialog size mismatch");

struct ModDetailsDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[0x10];   // +0xB8: unknown
};
static_assert(sizeof(ModDetailsDialog) == 0xC8, "ModDetailsDialog size mismatch");

struct ModUploadDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base (no extra fields)
};
static_assert(sizeof(ModUploadDialog) == 0xB8, "ModUploadDialog size mismatch");

struct ModDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[0x20];   // +0xB8: unknown (0x20 bytes extra)
};
static_assert(sizeof(ModDialog) == 0xD8, "ModDialog size mismatch");

struct ScenarioDialog {
    uint8_t base[0xB8];       // +0x00: DialogInstance base
    uint8_t field_B8[0x30];   // +0xB8: unknown (includes scenario list, selection index)
};
static_assert(sizeof(ScenarioDialog) == 0xE8, "ScenarioDialog size mismatch");
