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
    // (SecondaryVtable = 0x14015c8c0 deleted, ENDGAME B.3.8: that is the exe copy
    //  of Video::ImageBuffer's table, stamped on the separately allocated
    //  thumbnail objects; ModDialog has no second vptr. 0 readers.)
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

// --- ModDetailsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModDetailsDialog.h"

// --- ModDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModDialog.h"

// --- ModUploadDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ModUploadDialog.h"

// --- NewGameDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/NewGameDialog.h"

// --- OptionsDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionsDialog.h"

// --- OptionInputBindDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/OptionInputBindDialog.h"

// --- SaveLoadDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/SaveLoadDialog.h"

// --- ScenarioDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ScenarioDialog.h"

// --- ToolbarDialog --- ENDGAME B.3.8 DLG: a real class now (B8-4 forwarder)
#include "Dialog/ToolbarDialog.h"

// --- TutorialGoalDialog --- ENDGAME B.3.8 GOAL: a real class now (B8-4 forwarder)
#include "Tutorial/TutorialGoalDialog.h"

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
