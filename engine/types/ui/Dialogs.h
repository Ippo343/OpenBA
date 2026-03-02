#pragma once
// Game UI Dialog Controllers -- binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// All 22 Dialog Controller classes that extend UI::DialogInstance (base 0xB8).
// Decompiled from factory functions and constructors.
//
// Pattern: UI::DialogInstance::DialogInstance(this) -> set vtable -> init subclass fields
// DialogInstance base occupies +0x00..+0xB7 (0xB8 bytes).
// Subclass-specific fields start at +0xB8.

#include <cstdint>
#include <cstddef>
#include "DialogInstance.h"

// ============================================================
// Address constants for all Dialog Controllers
// ============================================================

namespace AchievementDialogAddr {
    constexpr uintptr_t Factory          = 0x140010320;
    constexpr uintptr_t Vtable           = 0x14015b020;
    constexpr uintptr_t SecondaryVtable  = 0x14015b070; // DialogController secondary vtable
}

namespace AutoSaveDialogAddr {
    constexpr uintptr_t Factory = 0x1400106c0;
    constexpr uintptr_t Vtable  = 0x14015b120;
}

namespace EventNotifyDialogAddr {
    constexpr uintptr_t Factory = 0x140011500;
    constexpr uintptr_t Ctor    = 0x140068b60;
    constexpr uintptr_t Vtable  = 0x14015bca0;
}

namespace EventPopupDialogAddr {
    constexpr uintptr_t Factory = 0x140011920;
    constexpr uintptr_t Vtable  = 0x14015bd80;
}

namespace FollowDialogAddr {
    constexpr uintptr_t Factory = 0x140011a20;
    constexpr uintptr_t Vtable  = 0x14015be00;
}

namespace GoalDialogAddr {
    constexpr uintptr_t Factory = 0x140011b60;
    constexpr uintptr_t Vtable  = 0x14015beb0;
}

namespace LoaderDialogAddr {
    constexpr uintptr_t Factory = 0x140020bb0;
    constexpr uintptr_t Vtable  = 0x14015f300;
}

namespace MainMenuDialogAddr {
    constexpr uintptr_t Factory = 0x140020d10;
    constexpr uintptr_t Vtable  = 0x14015f550;
}

namespace MapParamsDialogAddr {
    constexpr uintptr_t Factory = 0x140012100;
    constexpr uintptr_t Ctor    = 0x14006be20;
    constexpr uintptr_t Vtable  = 0x14015c180;
}

namespace MessageDialogAddr {
    constexpr uintptr_t Factory = 0x1400125d0;
    constexpr uintptr_t Vtable  = 0x14015c480;
}

namespace ModDetailsDialogAddr {
    constexpr uintptr_t Factory = 0x140012790;
    constexpr uintptr_t Vtable  = 0x14015c670;
}

namespace ModDialogAddr {
    constexpr uintptr_t Factory = 0x140012900;
    constexpr uintptr_t Vtable  = 0x14015c870;
}

namespace ModUploadDialogAddr {
    constexpr uintptr_t Factory = 0x140012bc0;
    constexpr uintptr_t Vtable  = 0x14015c6c0;
}

namespace NewGameDialogAddr {
    constexpr uintptr_t Factory = 0x140012c70;
    constexpr uintptr_t Ctor    = 0x140071120;
    constexpr uintptr_t Vtable  = 0x14015c920;
}

namespace OptionsDialogAddr {
    constexpr uintptr_t Factory = 0x140012df0;
    constexpr uintptr_t Ctor    = 0x140075280;
    constexpr uintptr_t Vtable  = 0x14015ceb0;
}

namespace OptionInputBindDialogAddr {
    constexpr uintptr_t Factory    = 0x140012e60;
    constexpr uintptr_t Vtable     = 0x14015ce40;
    constexpr uintptr_t BaseVtable = 0x14015c480; // MessageDialog vtable (parent)
}

namespace SaveLoadDialogAddr {
    constexpr uintptr_t Factory = 0x140021160;
    constexpr uintptr_t Ctor    = 0x1400b81c0;
    constexpr uintptr_t Vtable  = 0x14015f900;
}

namespace ScenarioDialogAddr {
    constexpr uintptr_t Factory = 0x140013970;
    constexpr uintptr_t Vtable  = 0x14015d2d0;
}

namespace ToolbarDialogAddr {
    constexpr uintptr_t Factory = 0x140021710;
    constexpr uintptr_t Ctor    = 0x1400b9890;
    constexpr uintptr_t Vtable  = 0x14015fa00;
}

namespace TutorialGoalDialogAddr {
    constexpr uintptr_t Factory = 0x140027440;
    constexpr uintptr_t Vtable  = 0x140160870; // final vtable (not 0x1401607a0)
}

namespace TutorialHighlightDialogAddr {
    constexpr uintptr_t Factory = 0x1400278c0;
    constexpr uintptr_t Ctor    = 0x1400e0290;
    constexpr uintptr_t Vtable  = 0x1401609e0;
}

namespace TutorialSetupDialogAddr {
    constexpr uintptr_t Factory   = 0x1400285b0;
    constexpr uintptr_t Ctor      = 0x1400e2cc0;
    constexpr uintptr_t Vtable    = 0x140160d20;
    constexpr uintptr_t BaseVtable= 0x140160c30; // intermediate vtable
}

// ============================================================
// Struct definitions
// ============================================================

// --- AchievementDialog (0xD0) ---
// Factory inits: +0xB8=0, +0xC0=0, +0xC8=0
struct AchievementDialog {
    DialogInstance base;        // +0x00: DialogInstance (0xB8)
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
};
static_assert(sizeof(AchievementDialog) == 0xD0, "AchievementDialog size mismatch");
static_assert(offsetof(AchievementDialog, dataB8) == 0xB8, "");

// --- AutoSaveDialog (0xD0) ---
// Factory inits: +0xB8=0, +0xC0=0
struct AutoSaveDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint8_t  _padC8[8];        // +0xC8: not initialized by factory
};
static_assert(sizeof(AutoSaveDialog) == 0xD0, "AutoSaveDialog size mismatch");
static_assert(offsetof(AutoSaveDialog, dataB8) == 0xB8, "");

// --- EventNotifyDialog (0xF0) ---
// Ctor inits: +0xB8=0, +0xC0(u32)=0, +0xC4(u32)=0xFFFFFFFF, +0xC8..+0xE8=0
struct EventNotifyDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint32_t dataC0;           // +0xC0: zeroed
    int32_t  dataC4;           // +0xC4: initialized to -1
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
    uint64_t dataD8;           // +0xD8: zeroed
    uint64_t dataE0;           // +0xE0: zeroed
    uint64_t dataE8;           // +0xE8: zeroed
};
static_assert(sizeof(EventNotifyDialog) == 0xF0, "EventNotifyDialog size mismatch");
static_assert(offsetof(EventNotifyDialog, dataB8) == 0xB8, "");
static_assert(offsetof(EventNotifyDialog, dataC4) == 0xC4, "");

// --- EventPopupDialog (0xC0) ---
// Factory: no subclass fields initialized (base only)
struct EventPopupDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: uninitialized by factory
};
static_assert(sizeof(EventPopupDialog) == 0xC0, "EventPopupDialog size mismatch");

// --- FollowDialog (0xD8) ---
// Factory inits: +0xB8=0, +0xC0=0, +0xC8=0, +0xD0=0
struct FollowDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
};
static_assert(sizeof(FollowDialog) == 0xD8, "FollowDialog size mismatch");
static_assert(offsetof(FollowDialog, dataB8) == 0xB8, "");

// --- GoalDialog (0xC8) ---
// Factory inits: +0xB8=0, +0xC0=0
struct GoalDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
};
static_assert(sizeof(GoalDialog) == 0xC8, "GoalDialog size mismatch");
static_assert(offsetof(GoalDialog, dataB8) == 0xB8, "");

// --- LoaderDialog (0xC8) ---
// Factory inits: +0xB8=0, +0xC0(u32)=0
struct LoaderDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint32_t dataC0;           // +0xC0: zeroed (u32 write)
    uint8_t  _padC4[4];        // +0xC4: gap
};
static_assert(sizeof(LoaderDialog) == 0xC8, "LoaderDialog size mismatch");
static_assert(offsetof(LoaderDialog, dataB8) == 0xB8, "");

// --- MainMenuDialog (0xC8) ---
// Factory: no subclass fields initialized
struct MainMenuDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: uninitialized
    uint64_t dataC0;           // +0xC0: uninitialized
};
static_assert(sizeof(MainMenuDialog) == 0xC8, "MainMenuDialog size mismatch");

// --- MapParamsDialog (0xF8) ---
// Ctor inits: +0xB8..+0xF0 = 0 (all zeroed, 8 qwords)
struct MapParamsDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
    uint64_t dataD8;           // +0xD8: zeroed
    uint64_t dataE0;           // +0xE0: zeroed
    uint64_t dataE8;           // +0xE8: zeroed
    uint64_t dataF0;           // +0xF0: zeroed
};
static_assert(sizeof(MapParamsDialog) == 0xF8, "MapParamsDialog size mismatch");
static_assert(offsetof(MapParamsDialog, dataB8) == 0xB8, "");

// --- MessageDialog (0xC0) ---
// Factory: no subclass fields initialized
struct MessageDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: uninitialized
};
static_assert(sizeof(MessageDialog) == 0xC0, "MessageDialog size mismatch");

// --- ModDetailsDialog (0xC8) ---
// Factory: no subclass fields initialized
struct ModDetailsDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: uninitialized
    uint64_t dataC0;           // +0xC0: uninitialized
};
static_assert(sizeof(ModDetailsDialog) == 0xC8, "ModDetailsDialog size mismatch");

// --- ModDialog (0xD8) ---
// Factory inits: +0xB8=0, +0xC0=0, +0xC8=0, +0xD0=0
struct ModDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
};
static_assert(sizeof(ModDialog) == 0xD8, "ModDialog size mismatch");
static_assert(offsetof(ModDialog, dataB8) == 0xB8, "");

// --- ModUploadDialog (0xB8) ---
// Factory: no subclass fields (size == base DialogInstance)
struct ModUploadDialog {
    DialogInstance base;        // +0x00
};
static_assert(sizeof(ModUploadDialog) == 0xB8, "ModUploadDialog size mismatch");

// --- NewGameDialog (0xAE8) ---
// Ctor inits: +0xB8..+0x110=0, Random@+0x118, +0xAE0=0
// Largest dialog. Contains System::Random sub-object at +0x118.
struct NewGameDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
    uint64_t dataD8;           // +0xD8: zeroed
    uint64_t dataE0;           // +0xE0: zeroed
    uint64_t dataE8;           // +0xE8: zeroed
    uint64_t dataF0;           // +0xF0: zeroed
    uint64_t dataF8;           // +0xF8: zeroed
    uint64_t data100;          // +0x100: zeroed
    uint64_t data108;          // +0x108: zeroed
    uint64_t data110;          // +0x110: zeroed
    uint8_t  random[0x9C0];   // +0x118: System::Random (initialized by Random::Random)
                               //         extends to +0xAD8
    uint64_t _pad_AD8;         // +0xAD8: gap
    uint64_t dataAE0;          // +0xAE0: zeroed
};
static_assert(sizeof(NewGameDialog) == 0xAE8, "NewGameDialog size mismatch");
static_assert(offsetof(NewGameDialog, dataB8) == 0xB8, "");
static_assert(offsetof(NewGameDialog, random) == 0x118, "");
static_assert(offsetof(NewGameDialog, dataAE0) == 0xAE0, "");

// --- OptionsDialog (0x1C0) ---
// Ctor inits: +0xB8..+0x188=0 (all zeroed), +0x1B0=0, +0x1B8=0
struct OptionsDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0
    uint64_t dataC8;           // +0xC8
    uint64_t dataD0;           // +0xD0
    uint64_t dataD8;           // +0xD8
    uint64_t dataE0;           // +0xE0
    uint64_t dataE8;           // +0xE8
    uint64_t dataF0;           // +0xF0
    uint64_t dataF8;           // +0xF8
    uint64_t data100;          // +0x100
    uint64_t data108;          // +0x108
    uint64_t data110;          // +0x110
    uint64_t data118;          // +0x118
    uint64_t data120;          // +0x120
    uint64_t data128;          // +0x128
    uint64_t data130;          // +0x130
    uint64_t data138;          // +0x138
    uint64_t data140;          // +0x140
    uint64_t data148;          // +0x148
    uint64_t data150;          // +0x150
    uint64_t data158;          // +0x158
    uint64_t data160;          // +0x160
    uint64_t data168;          // +0x168
    uint64_t data170;          // +0x170
    uint64_t data178;          // +0x178
    uint64_t data180;          // +0x180
    uint64_t data188;          // +0x188
    uint8_t  _pad190[0x20];    // +0x190: gap (not init by ctor)
    uint64_t data1B0;          // +0x1B0: zeroed
    uint64_t data1B8;          // +0x1B8: zeroed
};
static_assert(sizeof(OptionsDialog) == 0x1C0, "OptionsDialog size mismatch");
static_assert(offsetof(OptionsDialog, dataB8) == 0xB8, "");
static_assert(offsetof(OptionsDialog, data1B0) == 0x1B0, "");

// --- OptionInputBindDialog (0xC8) ---
// Extends MessageDialog (0xC0). Factory sets MessageDialog vtable then overrides.
// Adds: +0xC0(u32)=0, +0xC4(u32)=0xFFFFFFFF
struct OptionInputBindDialog {
    DialogInstance base;        // +0x00 (MessageDialog base)
    uint64_t dataB8;           // +0xB8: from MessageDialog
    uint32_t dataC0;           // +0xC0: zeroed
    int32_t  dataC4;           // +0xC4: initialized to -1
};
static_assert(sizeof(OptionInputBindDialog) == 0xC8, "OptionInputBindDialog size mismatch");
static_assert(offsetof(OptionInputBindDialog, dataC4) == 0xC4, "");

// --- SaveLoadDialog (0x160) ---
// Ctor: Sprite@+0xD0, zeroed +0xF0..+0x158
struct SaveLoadDialog {
    DialogInstance base;        // +0x00
    uint8_t  _padB8[0x18];     // +0xB8: gap (not initialized by ctor, may be from base)
    uint8_t  sprite[0x20];     // +0xD0: UI::Sprite sub-object (initialized by Sprite::Sprite)
    uint64_t dataF0;           // +0xF0: zeroed
    uint64_t dataF8;           // +0xF8: zeroed
    uint64_t data100;          // +0x100
    uint64_t data108;          // +0x108
    uint64_t data110;          // +0x110
    uint64_t data118;          // +0x118
    uint64_t data120;          // +0x120
    uint64_t data128;          // +0x128
    uint64_t data130;          // +0x130
    uint64_t data138;          // +0x138
    uint64_t data140;          // +0x140
    uint64_t data148;          // +0x148
    uint64_t data150;          // +0x150
    uint64_t data158;          // +0x158
};
static_assert(sizeof(SaveLoadDialog) == 0x160, "SaveLoadDialog size mismatch");
static_assert(offsetof(SaveLoadDialog, sprite) == 0xD0, "");

// --- ScenarioDialog (0xE8) ---
// Factory inits: +0xC0=0, +0xC8(u32)=0xFFFFFFFF, +0xD0=0, +0xD8=0, +0xE0=0
struct ScenarioDialog {
    DialogInstance base;        // +0x00
    uint8_t  _padB8[8];        // +0xB8: gap
    uint64_t dataC0;           // +0xC0: zeroed
    int32_t  dataC8;           // +0xC8: initialized to -1 (0xFFFFFFFF)
    uint8_t  _padCC[4];        // +0xCC: gap
    uint64_t dataD0;           // +0xD0: zeroed
    uint64_t dataD8;           // +0xD8: zeroed
    uint64_t dataE0;           // +0xE0: zeroed
};
static_assert(sizeof(ScenarioDialog) == 0xE8, "ScenarioDialog size mismatch");
static_assert(offsetof(ScenarioDialog, dataC0) == 0xC0, "");
static_assert(offsetof(ScenarioDialog, dataC8) == 0xC8, "");

// --- ToolbarDialog (0x108) ---
// Ctor inits: +0xB8..+0xE8=0, +0xF4(u32)=0, +0xF8=0, +0x100=0
struct ToolbarDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0
    uint64_t dataC8;           // +0xC8
    uint64_t dataD0;           // +0xD0
    uint64_t dataD8;           // +0xD8
    uint64_t dataE0;           // +0xE0
    uint64_t dataE8;           // +0xE8
    uint8_t  _padF0[4];        // +0xF0: gap
    uint32_t dataF4;           // +0xF4: zeroed (u32 write)
    uint64_t dataF8;           // +0xF8: zeroed
    uint64_t data100;          // +0x100: zeroed
};
static_assert(sizeof(ToolbarDialog) == 0x108, "ToolbarDialog size mismatch");
static_assert(offsetof(ToolbarDialog, dataB8) == 0xB8, "");
static_assert(offsetof(ToolbarDialog, dataF4) == 0xF4, "");

// --- TutorialGoalDialog (0x40) ---
// NOT DialogInstance-based. Extends Object::External (base ~0x20).
// Factory inits: +0x20=0, +0x28=0, +0x30(u32)=0xFFFFFFFF, +0x38=0
struct TutorialGoalDialog {
    void*    vtable;            // +0x00
    uint8_t  externalBase[0x18];// +0x08: Object::External base data
    uint64_t data20;           // +0x20: zeroed
    uint64_t data28;           // +0x28: zeroed
    int32_t  data30;           // +0x30: initialized to -1
    uint8_t  _pad34[4];        // +0x34: gap
    uint64_t data38;           // +0x38: zeroed
};
static_assert(sizeof(TutorialGoalDialog) == 0x40, "TutorialGoalDialog size mismatch");
static_assert(offsetof(TutorialGoalDialog, data30) == 0x30, "");

// --- TutorialHighlightDialog (0xF0) ---
// Ctor inits: +0xB8=0, +0xC0=0, +0xC8=0, +0xD0=0, +0xD8(u32)=0, +0xE0=0, +0xE8=0
struct TutorialHighlightDialog {
    DialogInstance base;        // +0x00
    uint64_t dataB8;           // +0xB8: zeroed
    uint64_t dataC0;           // +0xC0: zeroed
    uint64_t dataC8;           // +0xC8: zeroed
    uint64_t dataD0;           // +0xD0: zeroed
    uint32_t dataD8;           // +0xD8: zeroed (u32 write)
    uint8_t  _padDC[4];        // +0xDC: gap
    uint64_t dataE0;           // +0xE0: zeroed
    uint64_t dataE8;           // +0xE8: zeroed
};
static_assert(sizeof(TutorialHighlightDialog) == 0xF0, "TutorialHighlightDialog size mismatch");
static_assert(offsetof(TutorialHighlightDialog, dataB8) == 0xB8, "");

// --- TutorialSetupDialog (0x78) ---
// NOT DialogInstance-based. Extends Object::External (base ~0x20).
// Ctor inits: +0x20..+0x58=0, +0x60(u32)=0xFFFFFFFF, +0x68=0, +0x70(u32)=0
struct TutorialSetupDialog {
    void*    vtable;            // +0x00
    uint8_t  externalBase[0x18];// +0x08: Object::External base data
    uint64_t data20;           // +0x20: zeroed
    uint64_t data28;           // +0x28: zeroed
    uint64_t data30;           // +0x30: zeroed
    uint64_t data38;           // +0x38: zeroed
    uint64_t data40;           // +0x40: zeroed
    uint32_t data48;           // +0x48: zeroed (u32 write)
    uint8_t  _pad4C[4];        // +0x4C: gap
    uint64_t data50;           // +0x50: zeroed
    uint64_t data58;           // +0x58: zeroed
    int32_t  data60;           // +0x60: initialized to -1 (0xFFFFFFFF)
    uint8_t  _pad64[4];        // +0x64: gap
    uint64_t data68;           // +0x68: zeroed
    uint32_t data70;           // +0x70: zeroed
    uint8_t  _pad74[4];        // +0x74: gap
};
static_assert(sizeof(TutorialSetupDialog) == 0x78, "TutorialSetupDialog size mismatch");
static_assert(offsetof(TutorialSetupDialog, data60) == 0x60, "");
