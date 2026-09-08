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
#include <cstring>
#include "rt_port.h"                        // RT_DATA_SLOT (never caches a null resolve)

extern "C" void StatGoal_ProbeOperands(void* goal, int* outA, int* outB);

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

// Which Goal subclass is this? The four exe vtable addresses resolve to our own
// synthesized arrays, so an identity compare on the object's vtable pointer names
// the class exactly — no type-id lookup and no guessing from field shapes.
const char* GoalClass(void* goal) {
    if (!goal) return "?";
    static uintptr_t vtStat = 0, vtPop = 0, vtBuild = 0, vtTime = 0;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        vtStat  = ResolveAddr(Module::Exe, 0x1401604c0);
        vtPop   = ResolveAddr(Module::Exe, 0x140160370);
        vtBuild = ResolveAddr(Module::Exe, 0x140160300);
        vtTime  = ResolveAddr(Module::Exe, 0x140160540);
    }
    const uintptr_t vt = *reinterpret_cast<uintptr_t*>(goal);
    if (vt == vtStat)  return "Stat";
    if (vt == vtPop)   return "Pop";
    if (vt == vtBuild) return "Build";
    if (vt == vtTime)  return "Time";
    return "Goal";
}

// Human-readable comparison, matching StatGoal::Check's switch. Even types compare
// against the goal's fixed value, odd ones against a second stat.
const char* CmpName(int t) {
    switch (t) {
        case 0: case 1:   return "==";
        case 2: case 3:   return "!=";
        case 4: case 5:   return "<";
        case 6: case 7:   return "<=";
        case 8: case 9:   return ">";
        case 10: case 11: return ">=";
        default:          return "??";
    }
}

// Per-class requirement + live progress, written into buf.
// For a StatGoal this is the whole point of the census: the two operands Check() is
// about to compare, so a goal reads as "have 412, need >= 900" rather than "not met".
void DescribeGoal(void* goal, char* buf, size_t n) {
    buf[0] = 0;
    if (!goal) return;
    const char* cls = GoalClass(goal);
    const uint8_t* g = static_cast<const uint8_t*>(goal);

    if (cls[0] == 'S') {              // StatGoal
        const int32_t statA = *reinterpret_cast<const int32_t*>(g + 0x30);
        const int32_t statB = *reinterpret_cast<const int32_t*>(g + 0x34);
        const int32_t cmp   = *reinterpret_cast<const int32_t*>(g + 0x38);
        const int32_t fixed = *reinterpret_cast<const int32_t*>(g + 0x3c);
        const uint8_t req   = *(g + 0x40);
        int a = 0, b = 0;
        StatGoal_ProbeOperands(goal, &a, &b);
        int len = std::snprintf(buf, n, "stat=%d have=%d %s %d%s (statB=%d fixed=%d res='%s'%s)",
                      statA, a, CmpName(cmp), b,
                      (cmp & 1) ? " [vs statB]" : "",
                      statB, fixed, StrOf(g + 0x48),
                      req ? " reqOthers" : "");
        if (statA == 0x14 && len > 0 && (size_t)len < n) {
            // Stat 20 (cumulative production, 0x1400d8c50) picks a stats-history
            // series from the goal's resource-flag word at +0x78 and sums it over the
            // last +0x7c years. Print the flag word and the series it resolves to, so a
            // zero can be told apart as "wrong flag" vs "nothing produced in that
            // category" (series 10+3k is category k's produced counter, see
            // CitizenBehavior_CollectStats).
            const uint32_t flags = *reinterpret_cast<const uint32_t*>(g + 0x78);
            const int years = *reinterpret_cast<const int32_t*>(g + 0x7c);
            int col = -1;
            if ((flags >> 0x11) & 1)     col = 0x22;
            else if ((flags >> 0xe) & 1) col = 0x1f;
            else if (flags & 0x40)       col = 0x13;
            else if (flags & 0x20)       col = 0x19;
            else if ((flags >> 0xd) & 1) col = 0x1c;
            else if ((flags >> 0xc) & 1) col = 0x10;
            else if ((flags >> 0xb) & 1) col = 0xd;
            else if ((flags >> 0xa) & 1) col = 0xa;
            else if ((flags >> 0x9) & 1) col = 0x16;
            std::snprintf(buf + len, n - (size_t)len, " flags=0x%x years=%d series=%d cat=%d",
                          flags, years, col, col >= 0 ? (col - 10) / 3 : -1);
        }
    } else if (cls[0] == 'P') {       // PopulationGoal
        std::snprintf(buf, n, "targetPop=%d", *reinterpret_cast<const int32_t*>(g + 0x30));
    } else if (cls[0] == 'T') {       // TimeGoal
        std::snprintf(buf, n, "years=%d%s", *reinterpret_cast<const int32_t*>(g + 0x30),
                      *(g + 0x34) ? " reqOthers" : "");
    } else if (cls[0] == 'B') {       // BuildingGoal
        std::snprintf(buf, n, "active=%d f38=%p f40=%p", (int)*(g + 0x30),
                      *reinterpret_cast<void* const*>(g + 0x38),
                      *reinterpret_cast<void* const*>(g + 0x40));
    }
}

// Production census: the town manager (gw+0xf0) keeps 21 per-category records at
// +0x170 (stride 0x20: +0x00 stock, +0x08 limit, +0x0c/+0x10 per-sample-period
// accumulators) and the stats history at +0x150 (0x47 series of {int* data, int
// count, int cap}); series 8+3k / 9+3k / 10+3k mirror record k's +0x00 / +0x0c /
// +0x10, pushed by CitizenBehavior_CollectStats. Stat 20 sums series 10+3k, so a
// production achievement's whole progress is visible here.
void ProdSummary() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!townMgr) return;
    const uintptr_t hist = *reinterpret_cast<uintptr_t*>(townMgr + 0x150);
    const uintptr_t recs = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
    const int nSeries = *reinterpret_cast<int32_t*>(townMgr + 0x158);
    const int nRecs   = *reinterpret_cast<int32_t*>(townMgr + 0x178);
    if (!hist || !recs) {
        std::fprintf(stderr, "[Ach] prod: hist=%p recs=%p (nothing to sum)\n",
                     reinterpret_cast<void*>(hist), reinterpret_cast<void*>(recs));
        return;
    }
    const int rows0 = (nSeries > 10) ? *reinterpret_cast<int32_t*>(hist + 10 * 0x10 + 8) : -1;
    std::fprintf(stderr, "[Ach] prod: series=%d recs=%d samples(series10)=%d  "
                         "[cat: produced-total/stock/limit/f0c/f10]", nSeries, nRecs, rows0);
    for (int k = 0; k < nRecs && k < 21; ++k) {
        const int s = 10 + 3 * k;
        long long sum = 0;
        if (s < nSeries) {
            const int32_t* data = *reinterpret_cast<int32_t**>(hist + s * 0x10);
            const int cnt = *reinterpret_cast<int32_t*>(hist + s * 0x10 + 8);
            for (int i = 0; i < cnt && data; ++i) sum += data[i];
        }
        const uintptr_t r = recs + k * 0x20;
        std::fprintf(stderr, " %d:%lld/%d/%d/%d/%d", k, sum,
                     *reinterpret_cast<int32_t*>(r + 0x00), *reinterpret_cast<int32_t*>(r + 0x08),
                     *reinterpret_cast<int32_t*>(r + 0x0c), *reinterpret_cast<int32_t*>(r + 0x10));
    }
    std::fprintf(stderr, "\n");
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

    // GetAchievementCount derefs the manager's achievement LIST (mgr+0x00) without a
    // null check -- faithful, because retail always has one by the time anything asks.
    // The manager itself exists from Engine::Startup, but the list is only filled by
    // GameContainer_Init's SetAchievements, so there IS a window (notably early in a
    // LOAD) where mgr is a fine pointer and mgr+0x00 is null; calling through it faults
    // as READ of 0x28. This probe is diagnostic code that must never be the thing that
    // kills a run, so it checks first and reports the window instead of entering it.
    void* const achList = *reinterpret_cast<void**>(static_cast<uint8_t*>(mgr) + 0x00);
    if (!achList) {
        std::fprintf(stderr, "[Ach] f=%d manager=%p has NO ACHIEVEMENT LIST yet "
                             "(mgr+0x00 null) -- SetAchievements has not run; skipping "
                             "this census tick\n", frameNum, mgr);
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
        void** s_ppFileSystem = RT_DATA_SLOT(void**,
        "?_singleton@FileSystem@IO@@0PEAV12@EA");
        if (s_ppFileSystem && *s_ppFileSystem)
            fsGate = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*s_ppFileSystem) + 0x38);
    }

    // The gate value here is the LIVE field. BANISHED_ACH_IGNORE_MODS masks it around
    // the AchievementManager::Update call only and restores it immediately, so with that
    // flag set this probe still reads non-zero while evaluation IS happening -- printing
    // a bare "ACHIEVEMENTS DISABLED" there would be exactly the kind of confidently wrong
    // diagnostic this census exists to replace.
    static int s_ignoreMods = -1;
    if (s_ignoreMods < 0) {
        const char* e = std::getenv("BANISHED_ACH_IGNORE_MODS");
        s_ignoreMods = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    const char* gateNote = (fsGate <= 0)      ? ""
                         : (s_ignoreMods == 1) ? " (masked by BANISHED_ACH_IGNORE_MODS -- "
                                                 "evaluation IS running; awards this run are "
                                                 "test results, not legitimate unlocks)"
                                               : " *** ACHIEVEMENTS DISABLED ***";
    std::fprintf(stderr, "[Ach] ===== f=%d mgr=%p achievements=%d updateGate(FileSystem+0x38)=%d%s =====\n",
                 frameNum, mgr, count, fsGate, gateNote);

    const bool dumpNow = s_dump;
    s_dump = false;                       // discovery dump is one-shot
    if (dumpNow) Hex("mgr", mgr, 0x60);
    ProdSummary();

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

            char desc[256];
            DescribeGoal(goal, desc, sizeof(desc));
            std::fprintf(stderr, "[Ach]      goal%-2d %-5s met=%d failed=%d check=%d  %s%s%s\n",
                         g, GoalClass(goal), gMet, gFailed, GoalCheck(goal), desc,
                         StrOf(textBuf)[0] ? " text=" : "", StrOf(textBuf));

            if (dumpNow && i == 0 && g == 0) {
                Hex("goalInst", gi, 0x40);
                Hex("goalRes", goal, 0x80);
            }
        }
    }
    std::fprintf(stderr, "[Ach] ----- awarded %d/%d -----\n", awardedTotal, count);
    std::fflush(stderr);
}
