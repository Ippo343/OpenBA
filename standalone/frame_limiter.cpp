// standalone/frame_limiter.cpp
// See frame_limiter.h for rationale.

#ifdef STANDALONE_EXE

#include "frame_limiter.h"

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace FrameLimiter {

namespace {

bool      g_initialized   = false;
bool      g_enabled       = true;     // false => fully uncapped (no-op Tick)
int64_t   g_qpcFreq       = 0;        // QueryPerformanceFrequency
int64_t   g_intervalTicks = 0;        // QPC ticks per frame at the ceiling
double    g_capFps        = 0.0;      // resolved ceiling (FPS)

int64_t   g_lastFrameTick = 0;        // QPC stamp the prior frame was released at
bool      g_havePrev      = false;    // false until the first Tick primes it

// High-resolution waitable timer (Win10 1803+). Lets us sleep with sub-ms
// precision without timeBeginPeriod's process-wide clock bump. Null => fall
// back to a plain Sleep(1)+spin loop.
HANDLE    g_timer         = nullptr;

// FPS accounting (updated once per ~second).
int64_t   g_fpsWindowStart = 0;
int       g_fpsFrameCount  = 0;
double    g_measuredFps    = 0.0;

// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is only in newer SDK headers.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

inline int64_t QpcNow() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

// Query the primary display's current refresh rate in Hz. Returns 0 if unknown.
int QueryDisplayRefreshHz() {
    DEVMODEW dm;
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
        // dmDisplayFrequency of 0 or 1 means "default/unknown" per MSDN.
        if (dm.dmDisplayFrequency > 1) {
            return (int)dm.dmDisplayFrequency;
        }
    }
    return 0;
}

void Init() {
    g_initialized = true;

    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpcFreq = f.QuadPart;

    // --- Resolve the ceiling FPS ---
    double targetFps = 0.0;
    bool   capFromEnv = false;

    if (const char* capEnv = getenv("BANISHED_FPS_CAP")) {
        if (_stricmp(capEnv, "off") == 0 || _stricmp(capEnv, "false") == 0) {
            g_enabled = false;
        } else {
            double v = atof(capEnv);
            if (v <= 0.0) {
                g_enabled = false;      // "0" => disabled
            } else {
                targetFps  = v;
                capFromEnv = true;
            }
        }
    }

    if (g_enabled && !capFromEnv) {
        int refresh = QueryDisplayRefreshHz();
        if (refresh <= 0) refresh = 60;     // safe default if the query fails

        double headroomPct = 50.0;          // default: ceiling = refresh * 1.5
        if (const char* hrEnv = getenv("BANISHED_FPS_HEADROOM")) {
            double h = atof(hrEnv);
            if (h >= 0.0) headroomPct = h;
        }
        targetFps = (double)refresh * (1.0 + headroomPct / 100.0);
    }

    if (g_enabled) {
        g_capFps        = targetFps;
        g_intervalTicks = (int64_t)((double)g_qpcFreq / targetFps);
        if (g_intervalTicks < 1) g_intervalTicks = 1;

        g_timer = CreateWaitableTimerExW(
            nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!g_timer) {
            // Older OS without high-res flag — fall back to a manual-reset timer.
            g_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }

        // Log to stderr: in standalone, stdout is freopen'd to standalone_debug.log
        // which a stale instance can hold open (redirect silently fails, output
        // lost); stderr -> standalone_error.log is reliable and survives close.
        fprintf(stderr,
               "[FrameLimiter] ceiling = %.1f FPS (%.3f ms/frame)%s, "
               "hi-res timer=%s. External limiters (vsync/G-Sync/RTSS) at or "
               "below refresh are never affected.\n",
               g_capFps, 1000.0 / g_capFps,
               capFromEnv ? " [BANISHED_FPS_CAP]" : " [refresh+headroom]",
               g_timer ? "yes" : "no (Sleep fallback)");
    } else {
        fprintf(stderr, "[FrameLimiter] disabled (uncapped) via BANISHED_FPS_CAP\n");
    }
    fflush(stderr);
}

// Block until `deadlineTick`. Coarse-sleeps the bulk via the waitable timer,
// then busy-spins the final fraction for an accurate release time.
void WaitUntil(int64_t deadlineTick) {
    // Tiny spin tail (~0.25 ms): the high-res timer is accurate enough that we
    // don't want to burn a core fraction spinning — this limiter exists to SAVE
    // CPU, so favor the sleep.
    const int64_t spinMarginTicks = g_qpcFreq / 4000;

    for (;;) {
        int64_t now = QpcNow();
        int64_t remaining = deadlineTick - now;
        if (remaining <= 0) break;

        if (remaining > spinMarginTicks && g_timer) {
            // Sleep all but the spin tail. SetWaitableTimer due time is in
            // 100 ns units, negative = relative.
            int64_t sleepTicks = remaining - spinMarginTicks;
            LARGE_INTEGER due;
            due.QuadPart = -((sleepTicks * 10000000) / g_qpcFreq);
            if (due.QuadPart < 0 &&
                SetWaitableTimer(g_timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(g_timer, INFINITE);
            } else {
                Sleep(0);
            }
        } else {
            // Final sub-ms tail: spin politely (hint the CPU, don't burn power).
            YieldProcessor();
        }
    }
}

} // namespace

void Tick() {
    if (!g_initialized) Init();
    if (!g_enabled) return;

    int64_t now = QpcNow();

    if (!g_havePrev) {
        g_havePrev       = true;
        g_lastFrameTick  = now;
        g_fpsWindowStart = now;
        g_fpsFrameCount  = 0;
        return;
    }

    int64_t deadline = g_lastFrameTick + g_intervalTicks;

    if (now < deadline) {
        WaitUntil(deadline);
        now = QpcNow();
    }

    // Advance the cadence. If we fell behind by more than one full interval
    // (breakpoint, alt-tab, a long hitch), resync to `now` instead of trying
    // to "catch up" with a burst of un-throttled frames.
    if (now > deadline + g_intervalTicks) {
        g_lastFrameTick = now;
    } else {
        g_lastFrameTick = deadline;
    }

    // --- FPS measurement (once per ~second) ---
    g_fpsFrameCount++;
    int64_t elapsed = now - g_fpsWindowStart;
    if (elapsed >= g_qpcFreq) {
        g_measuredFps    = (double)g_fpsFrameCount * (double)g_qpcFreq / (double)elapsed;
        g_fpsWindowStart = now;
        g_fpsFrameCount  = 0;
        // Periodic readout to stderr (reliable; see Init()).
        fprintf(stderr, "[FrameLimiter] fps=%.1f (cap=%.1f)\n",
                g_measuredFps, g_capFps);
        fflush(stderr);
    }
}

double GetFps()    { return g_measuredFps; }
double GetCapFps() { return g_capFps; }

} // namespace FrameLimiter

#endif // STANDALONE_EXE
