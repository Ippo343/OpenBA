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
    // (SecondaryVtable = 0x14015b070 deleted, ENDGAME B.3.8 D9 #14: that is
    //  AttendanceUI's table; AchievementDialog has no second vptr. 0 readers.)
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
    constexpr uintptr_t SecondaryVtable = 0x14015c8C0; // vtable+0x50 (ImageBuffer secondary interface)
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

// --- AchievementDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/AchievementDialog.h"

// --- AutoSaveDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/AutoSaveDialog.h"

// --- EventNotifyDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/EventNotifyDialog.h"

// --- EventPopupDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/EventPopupDialog.h"

// --- FollowDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/FollowDialog.h"

// --- GoalDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/GoalDialog.h"

// --- LoaderDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/LoaderDialog.h"

// --- MainMenuDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MainMenuDialog.h"

// --- MapParamsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MapParamsDialog.h"

// --- MessageDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/MessageDialog.h"

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

// --- NewGameDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/NewGameDialog.h"

// --- OptionsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionsDialog.h"

// --- OptionInputBindDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionInputBindDialog.h"

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

// --- ScenarioDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ScenarioDialog.h"

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
