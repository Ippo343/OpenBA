// standalone/achievement_probe.cpp
//
// BANISHED_ACH_TRACE=<n frames> — the ACHIEVEMENT census.
//
// The 36 achievements are pure data: each Achievement resource owns an
// Array<ExternalPtr<Goal>>, and the AchievementManager keeps one
// AchievementInstance per achievement, each holding a GoalList of GoalInstances.
// Everything from AchievementManager::Update down to GoalInstance::Update is
// RETAIL Runtime code we call straight out of the DLL; the only ported parts in
// the loop are the exe-side Goal::Check overrides (StatGoal / PopulationGoal /
// BuildingGoal / TimeGoal, vtable slot 8) and StatGoal's StatValueDispatch
// (0x1400da4e0). So an achievement that never completes is most likely a wrong
// STAT VALUE, not a broken award path — and a bare "0% complete" in the dialog
// cannot tell those two apart.
//
// This probe removes that ambiguity. Every n frames it prints, per achievement:
// awarded / instance IsMet / HasFailed, then one line per goal with the
// GoalInstance met+failed bits AND a direct call of the goal's own Check()
// (vtable slot 8), which is read-only in every ported override and so reports the
// live verdict even when the instance has not been ticked.
//
// BANISHED_ACH_TRACE_DUMP=1 additionally hexdumps the manager, the first goal
// instance and the first goal resource, so struct offsets can be read off
// evidence instead of guessed.
//
// BANISHED_ACH_RESET=1 calls AchievementManager::Reset() once at the first tick,
// so a run starts from a clean slate — awards otherwise persist under
// HKCU\Software\Shining Rock Software LLC\Banished and every later run inherits
// them, which would make a re-test look like a pass.

#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

typedef void  (*GetAchInstance_t)(void* self, void* retval, int index);
typedef void* (*GetGoalListOfInst_t)(void* self);
typedef bool  (*InstBool_t)(void* self);
typedef void  (*GetGoalOfInstance_t)(void* self, void* retval);
typedef void* (*MgrGet_t)(void);
typedef void  (*MgrReset_t)(void* self);

GetAchInstance_t     fnGetAchInstance  = nullptr;
GetGoalListOfInst_t  fnInstGetGoalList = nullptr;
InstBool_t           fnInstIsMet       = nullptr;
InstBool_t           fnInstHasFailed   = nullptr;
GetGoalOfInstance_t  fnGoalInstGetGoal = nullptr;
MgrGet_t             fnMgrGet          = nullptr;
MgrReset_t           fnMgrReset        = nullptr;

int  s_interval  = -2;      // -2 = env unread, -1 = disabled
bool s_dump      = false;
bool s_resetDone = false;
bool s_resolved  = false;

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    fnGetAchInstance = reinterpret_cast<GetAchInstance_t>(ResolveExport(
        "?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z"));
    fnInstGetGoalList = reinterpret_cast<GetGoalListOfInst_t>(ResolveExport(
        "?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ"));
    fnInstIsMet = reinterpret_cast<InstBool_t>(ResolveExport(
        "?IsMet@AchievementInstance@Achievement@@QEBA_NXZ"));
    fnInstHasFailed = reinterpret_cast<InstBool_t>(ResolveExport(
        "?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ"));
    fnGoalInstGetGoal = reinterpret_cast<GetGoalOfInstance_t>(ResolveExport(
        "?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ"));
    fnMgrGet = reinterpret_cast<MgrGet_t>(ResolveExport(
        "?Get@AchievementManager@Achievement@@SAPEAV12@XZ"));
    fnMgrReset = reinterpret_cast<MgrReset_t>(ResolveExport(
        "?Reset@AchievementManager@Achievement@@QEAAXXZ"));
}

void Init() {
    if (s_interval != -2) return;
    const char* e = std::getenv("BANISHED_ACH_TRACE");
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : 1800;
    const char* d = std::getenv("BANISHED_ACH_TRACE_DUMP");
    s_dump = (d && d[0] && d[0] != '0');
    std::fprintf(stderr, "[Ach] armed: achievement census every %d frames%s\n",
                 s_interval, s_dump ? " (+discovery dump)" : "");
    std::fflush(stderr);
}

void* Manager() {
    if (fnMgrGet) { void* m = fnMgrGet(); if (m) return m; }
    if (g_ppAchievementManager && *g_ppAchievementManager) return **g_ppAchievementManager;
    return nullptr;
}

// System::String is 8 bytes holding a char* straight at the character data.
const char* StrOf(const void* strBuf) {
    if (!strBuf) return "";
    const char* p = *reinterpret_cast<const char* const*>(strBuf);
    return p ? p : "";
}

void Hex(const char* label, const void* p, int bytes) {
    if (!p) { std::fprintf(stderr, "[Ach]   %s = null\n", label); return; }
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (int i = 0; i < bytes; i += 16) {
        std::fprintf(stderr, "[Ach]   %s +%02x:", label, i);
        for (int j = 0; j < 16 && i + j < bytes; ++j) std::fprintf(stderr, " %02x", b[i + j]);
        std::fprintf(stderr, "\n");
    }
}

// Call the goal's own Check (vtable slot 8 = byte offset +0x40). Read-only in
// every ported override, so it is safe to poll from outside the tick.
int GoalCheck(void* goal) {
    if (!goal) return -1;
    void** vt = *reinterpret_cast<void***>(goal);
    if (!vt || !vt[8]) return -1;
    return reinterpret_cast<int (*)(void*)>(vt[8])(goal);
}

}  // namespace

extern "C" void AchievementProbe_Tick(int frameNum) {
    Init();
    if (s_interval <= 0) return;
    Resolve();

    void* mgr = Manager();

    if (!s_resetDone) {
        s_resetDone = true;
        const char* r = std::getenv("BANISHED_ACH_RESET");
        if (r && r[0] && r[0] != '0' && fnMgrReset && mgr) {
            fnMgrReset(mgr);
            std::fprintf(stderr, "[Ach] AchievementManager::Reset() called (clean slate)\n");
        }
    }

    if (frameNum == 0 || (frameNum % s_interval) != 0) return;

    if (!mgr) {
        std::fprintf(stderr, "[Ach] f=%d MANAGER NULL (Get=%p ppMgr=%p)\n", frameNum,
                     reinterpret_cast<void*>(fnMgrGet),
                     reinterpret_cast<void*>(g_ppAchievementManager));
        std::fflush(stderr);
        return;
    }

    const int count = fn_AchievementManager_GetAchievementCount
                        ? fn_AchievementManager_GetAchievementCount(mgr) : -1;

    // AchievementManager::Update's very first act (Runtime 0x18009e6c6) is
    //     if (*(int*)(IO::FileSystem::_singleton + 0x38) > 0) return;
    // — retail's "achievements are off while mods/extra packages are loaded" gate.
    // It is a silent, total disable: every goal stops being evaluated and nothing in
    // the game says so. Report it, because a non-zero value makes every other number
    // in this census meaningless as evidence about award behaviour.
    int fsGate = -1;
    {
        static void** s_ppFileSystem = reinterpret_cast<void**>(
            ResolveExport("?_singleton@FileSystem@IO@@0PEAV12@EA"));
        if (s_ppFileSystem && *s_ppFileSystem)
            fsGate = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*s_ppFileSystem) + 0x38);
    }

    std::fprintf(stderr, "[Ach] ===== f=%d mgr=%p achievements=%d updateGate(FileSystem+0x38)=%d%s =====\n",
                 frameNum, mgr, count, fsGate,
                 fsGate > 0 ? " *** ACHIEVEMENTS DISABLED ***" : "");

    const bool dumpNow = s_dump;
    s_dump = false;                       // discovery dump is one-shot
    if (dumpNow) Hex("mgr", mgr, 0x60);

    // mgr+0x18 = the AchievementInstance array, mgr+0x20 its count. Both are filled
    // ONLY by AchievementManager::Reset (new game) or ::Persist (load). Retail's
    // GetAchievementInstance indexes +0x18 with no null check — faithful, because
    // retail always ran one of those two — so read the count first rather than
    // faulting inside the DLL when the port has skipped them.
    void* const  instArray = *reinterpret_cast<void**>(static_cast<uint8_t*>(mgr) + 0x18);
    const int    instCount = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(mgr) + 0x20);
    if (!instArray || instCount <= 0) {
        std::fprintf(stderr, "[Ach] NO INSTANCES (mgr+0x18=%p mgr+0x20=%d) — "
                             "AchievementManager::Update evaluates NOTHING; no achievement "
                             "can be awarded this run\n", instArray, instCount);
        std::fflush(stderr);
        return;
    }

    int awardedTotal = 0;
    for (int i = 0; i < count; ++i) {
        // Achievement resource. The ExternalPtr return collapses to the object
        // pointer in its first qword — the same read AchievementDialog::Slot4 does.
        uint8_t achBuf[64] = {0};
        void* ach = nullptr;
        if (fn_AchievementManager_GetAchievement) {
            fn_AchievementManager_GetAchievement(mgr, achBuf, i);
            ach = *reinterpret_cast<void**>(achBuf);
        }

        uint8_t titleBuf[8] = {0};
        if (ach && fn_Achievement_GetTitle) fn_Achievement_GetTitle(ach, titleBuf);
        const char* title = StrOf(titleBuf);

        const bool awarded = (fn_AchievementManager_IsAchievementAwarded &&
                              fn_AchievementManager_IsAchievementAwarded(mgr, i));
        if (awarded) ++awardedTotal;

        // AchievementInstance (InternalPtr return; object in the first qword).
        uint8_t instBuf[64] = {0};
        if (fnGetAchInstance) {
            fnGetAchInstance(mgr, instBuf, i);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(instBuf);
        }
        void* inst = *reinterpret_cast<void**>(instBuf);

        const int instMet    = (inst && fnInstIsMet)     ? (int)fnInstIsMet(inst)     : -1;
        const int instFailed = (inst && fnInstHasFailed) ? (int)fnInstHasFailed(inst) : -1;

        void* goalList = (inst && fnInstGetGoalList) ? fnInstGetGoalList(inst) : nullptr;
        const int goalCount = (goalList && fn_GoalList_GetGoalCount)
                                ? fn_GoalList_GetGoalCount(goalList) : -1;

        std::fprintf(stderr,
                     "[Ach] %2d %-18s awarded=%d met=%d failed=%d goals=%d (ach=%p inst=%p)\n",
                     i, title[0] ? title : "?", (int)awarded, instMet, instFailed,
                     goalCount, ach, inst);

        for (int g = 0; g < goalCount; ++g) {
            uint8_t giBuf[64] = {0};
            if (fn_GoalList_GetGoal) {
                fn_GoalList_GetGoal(goalList, giBuf, g);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(giBuf);
            }
            void* gi = *reinterpret_cast<void**>(giBuf);
            if (!gi) { std::fprintf(stderr, "[Ach]      goal%d <null instance>\n", g); continue; }

            const int gMet    = fn_GoalInstance_IsMet     ? (int)fn_GoalInstance_IsMet(gi)     : -1;
            const int gFailed = fn_GoalInstance_HasFailed ? (int)fn_GoalInstance_HasFailed(gi) : -1;

            // The Goal RESOURCE behind the instance (ExternalPtr return).
            uint8_t goalBuf[64] = {0};
            void* goal = nullptr;
            if (fnGoalInstGetGoal) {
                fnGoalInstGetGoal(gi, goalBuf);
                goal = *reinterpret_cast<void**>(goalBuf);
            }

            uint8_t textBuf[8] = {0};
            if (goal && fn_Goal_GetText) fn_Goal_GetText(goal, textBuf);

            std::fprintf(stderr, "[Ach]      goal%d met=%d failed=%d check=%d goal=%p '%s'\n",
                         g, gMet, gFailed, GoalCheck(goal), goal, StrOf(textBuf));

            if (dumpNow && i == 0 && g == 0) {
                Hex("goalInst", gi, 0x40);
                Hex("goalRes", goal, 0x80);
            }
        }
    }
    std::fprintf(stderr, "[Ach] ----- awarded %d/%d -----\n", awardedTotal, count);
    std::fflush(stderr);
}
