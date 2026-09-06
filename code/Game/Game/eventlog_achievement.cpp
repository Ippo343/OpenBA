// code/Game/Game/eventlog_achievement.cpp
//
// OnAchievementAwarded (exe 0x140096960) — the achievement award TOAST.
//
// This is the `void (*)(int)` callback retail installs with
// Achievement::AchievementManager::SetEventHandler at GameUpdateLoop::Load step 30
// (call site 0x14009752e; the LEA that loads the handler address is 0x140097524).
// Runtime's AchievementManager invokes it with the awarded achievement's INDEX the
// moment the award is recorded, and all it does is push one event-log entry — i.e.
// the on-screen announcement. With the handler left null the award is still recorded
// and still written to the registry; only the toast is lost. That was the state of
// PLAN.md §1.2 item 6 ("Achievement award toast unported").
//
// Retail body (disasm-verified, 0x140096960 .. 0x140096aea, 97 instructions):
//   RSI = eventLog = *(g_pGameWorldSingleton /*0x1401c3dd8*/) + 0x138
//   ExternalPtr<Achievement> ap;
//   AchievementManager::GetAchievement(**0x1401514f0, &ap, id)          [0x140151540]
//   System::String titleKey;  Achievement::GetTitle(ap, &titleKey)      [0x140151598]
//   UI::String uiName(*(ap+0x20) /*ExternalPtr<StringTable>*/, titleKey)[0x140151300]
//   ~titleKey                                                           [0x140152af0]
//   System::String titleText = (System::String)uiName                   [0x140151308]
//   InternalBasePtr nullEntity(nullptr)                                 [0x140152a90]
//   ... then the AddEventWithName body INLINED with baked-in constants:
//       entry+0x00 colour   = 0xFFEA7517   (what severity 4 maps to)
//       entry+0x10 type     = 0x19
//       entry+0x14 severity = 4
//       entry+0x18 tile     = EventLog_TileIndex(nullEntity)  -> -1
//       entry+0x1c icon     = 9            (what IconIndex(0x19) returns)
//       entry+0x20 popup    = -1           (what PopupIndex(0x19) returns)
//       EventArray_Grow(eventLog+0x18, count+1, entry)                  [0x140019b70]
//   FormatIndexed args: {0}=season {1}=year {2}=titleText — i.e. exactly the
//   AddEventWithName (0x140095750) shape. Every derived value above matches what
//   EventLog_AddEventCore already computes for (0x19, 4, null entity), so we route
//   through the shared core rather than transcribing a 4th copy of the append —
//   see the "AddEvent family core" note in eventlog.cpp on how the copies drift.
//   There is no speed-drop tail (0x19 is outside the 0xb/0xc/0x1e/0x1f set), and
//   retail's tail-free body agrees.
//
// NOTE the UI::String temporary is never destructed in retail either (it holds an
// ExternalPtr + an index, no owned heap String). Kept faithful.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// eventlog.cpp — the shared AddEvent family core, exposed for this sibling.
extern "C" void EventLog_AddEventWithName(void* eventLog, int eventType, int severity,
                                          void* entityIBP, void* nameStr);

// NB: BANISHED_ACH_TRACE is already the achievement CENSUS interval
// (standalone/achievement_probe.cpp) — this toast gets its own name.
static bool ToastTrace() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_ACH_TOAST_TRACE");
        if (!e || !e[0]) e = std::getenv("BANISHED_EVENT_TRACE");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}

extern "C" void EventLog_OnAchievementAwarded(int achievementId) {
    // Trace at ENTRY, not just at the end: every guard below is an early return,
    // and a silent one is indistinguishable from "the handler was never called".
    if (ToastTrace()) {
        fprintf(stderr, "[AchToast] handler ENTER id=%d\n", achievementId);
        fflush(stderr);
    }

    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* eventLog = *(void**)((uintptr_t)gameWorld + 0x138);
    if (!eventLog) return;

    // AchievementManager singleton — exe import slot 0x1401514f0 holds the address
    // of Runtime's ?_singleton@AchievementManager@Achievement@@0PEAV12@EA, so the
    // manager is **g_ppAchievementManager. Retail derefs it unguarded; we only ever
    // reach here from Runtime's own handler call, so it is always live — the guard
    // below only covers the case where the slot never resolved at all.
    void* mgr = (g_ppAchievementManager && *g_ppAchievementManager)
                    ? **g_ppAchievementManager : nullptr;
    if (!mgr || !fn_AchievementManager_GetAchievement) return;

    uint64_t achievement = 0;   // ExternalPtr<Achievement> (8 bytes, trivially destructible)
    fn_AchievementManager_GetAchievement(mgr, &achievement, achievementId);
    if (achievement == 0) {
        if (ToastTrace()) {
            fprintf(stderr, "[AchToast] GetAchievement(%d) -> null, no toast\n", achievementId);
            fflush(stderr);
        }
        return;
    }

    uint8_t titleKey[8] = {0};  // System::String — the achievement's title KEY
    fn_Achievement_GetTitle((void*)achievement, titleKey);

    // UI::String(ExternalPtr<StringTable> = *(achievement+0x20), key), then
    // operator System::String() — the LOOKED-UP display text. This is the
    // NAME-LOOKUP UI::String ctor (0x140151300), not the index one.
    uint8_t uiName[24] = {0};
    uint8_t titleText[8] = {0};
    if (fn_UIString_Ctor && fn_UIString_ToSystemString) {
        fn_UIString_Ctor(uiName, *(uint64_t*)((uintptr_t)achievement + 0x20), titleKey);
        fn_String_Dtor(titleKey);
        fn_UIString_ToSystemString(uiName, titleText);
    } else {
        // Unresolved exports: announce with the raw key rather than dropping the
        // toast entirely. Standalone-only deviation, stated.
        fn_String_Ctor_Copy(titleText, titleKey);
        fn_String_Dtor(titleKey);
    }

    int64_t nullEntity[1] = {0};
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(nullEntity, nullptr);

    if (ToastTrace()) {
        const char* t = *(const char**)titleText;
        fprintf(stderr, "[AchToast] awarded id=%d title=\"%s\"\n",
                achievementId, t ? t : "(null)");
        fflush(stderr);
    }

    EventLog_AddEventWithName(eventLog, /*eventType*/ 0x19, /*severity*/ 4,
                              nullEntity, titleText);

    fn_String_Dtor(titleText);
}
