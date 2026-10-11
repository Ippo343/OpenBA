// src/app/frame_limiter.cpp
// See frame_limiter.h for rationale.


#include "frame_limiter.h"
#include "options.h"

#include "platform_window.h"   // Platform::Window::GetCurrentDisplayRefreshHz (SDL W4)
#include "platform_thread.h"   // Platform::SleepMs
#include "platform_time.h"     // Platform::PerfCounter / PerfFrequency / WaitTimer* / CpuRelax
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
// back to a plain Sleep(1)+spin loop. Platform::WaitTimerCreate keeps the
// high-resolution-then-plain fallback (src/platform/win/time_win.cpp).
Platform::WaitTimerHandle g_timer = nullptr;

// FPS accounting (updated once per ~second).
int64_t   g_fpsWindowStart = 0;
int       g_fpsFrameCount  = 0;
double    g_measuredFps    = 0.0;

inline int64_t QpcNow() {
    int64_t li;
    Platform::PerfCounter(&li);   // QueryPerformanceCounter
    return li;
}

// Query the primary display's current refresh rate in Hz. Returns 0 if unknown.
// Phase D.4 SDL W4: was EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS)'s
// dmDisplayFrequency (0 or 1 = unknown); now SDL's current mode of the primary display,
// rounded to whole Hz (display_sdl.cpp).
int QueryDisplayRefreshHz() {
    return Platform::Window::GetCurrentDisplayRefreshHz();
}

void Init() {
    g_initialized = true;

    int64_t f;
    Platform::PerfFrequency(&f);  // QueryPerformanceFrequency
    g_qpcFreq = f;

    // --- Resolve the ceiling FPS ---
    double targetFps = 0.0;
    bool   capFromEnv = false;

    if (const char* capEnv = Opt().fpsCap) {
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
        if (const char* hrEnv = Opt().fpsHeadroom) {
            double h = atof(hrEnv);
            if (h >= 0.0) headroomPct = h;
        }
        targetFps = (double)refresh * (1.0 + headroomPct / 100.0);
    }

    if (g_enabled) {
        g_capFps        = targetFps;
        g_intervalTicks = (int64_t)((double)g_qpcFreq / targetFps);
        if (g_intervalTicks < 1) g_intervalTicks = 1;

        // CreateWaitableTimerExW(.., HIGH_RESOLUTION, TIMER_ALL_ACCESS), and on an
        // older OS without the high-res flag CreateWaitableTimerW(NULL, FALSE, NULL).
        g_timer = Platform::WaitTimerCreate();

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
            // WaitTimerSleep is SetWaitableTimer(-due) + WaitForSingleObject(INFINITE);
            // it returns false, without waiting, when the timer cannot be armed.
            int64_t sleepTicks = remaining - spinMarginTicks;
            const int64_t due100ns = (sleepTicks * 10000000) / g_qpcFreq;
            if (!(due100ns > 0 && Platform::WaitTimerSleep(g_timer, due100ns))) {
                Platform::SleepMs(0);   // Sleep(0)
            }
        } else {
            // Final sub-ms tail: spin politely (hint the CPU, don't burn power).
            Platform::CpuRelax();       // YieldProcessor
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

