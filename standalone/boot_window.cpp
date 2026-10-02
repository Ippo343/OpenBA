// standalone/boot_window.cpp — see boot_window.h for the rationale + measurements.

#include "boot_window.h"
#include "stall_debug.h"
#include "options.h"
#include "video/Device.h"   // B4: Video::Configuration::field42
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace BootWindow {

namespace {

// Video::Configuration+0x42 — the engine's own non-persisted "don't show the
// window" flag, read by VideoDX11 CreateDevice step 6. Mirrors Cfg::kSkipShow
// in dx11/src/devicedata.cpp; kept as a local constant so this file does not
// depend on the renderer's internal header.
constexpr size_t kCfgSkipShow = 0x42;

// Reveal the window regardless after this long, so a broken splash path can
// never leave the process running with nothing on screen. Generous: a cold
// first boot legitimately spends >1s before the splash presents.
constexpr double kWatchdogMs = 4000.0;

HWND   g_hwnd      = nullptr;
bool   g_armed     = false;
bool   g_shown     = false;
double g_armedAtMs = 0.0;

bool DeferralEnabled() {
    return Opt().deferWindowShow;
}

} // namespace

bool ArmDeferredShow(void* videoConfig, HWND hwnd) {
    if (!videoConfig || !hwnd) return false;
    if (!DeferralEnabled()) {
        printf("[BootWindow] deferral disabled (BANISHED_DEFER_WINDOW_SHOW=0) — "
               "shipped ShowWindow will run inline\n");
        std::fflush(stdout);
        return false;
    }
    if (g_armed || g_shown) return g_armed;   // only the first device creation arms

    // LEFT: dx11/src/devicedata.cpp (dx11 seam) hands the configuration as void*.
    static_cast< ::Video::Configuration*>(videoConfig)->field42 = 1;   // +0x42 (kCfgSkipShow)

    g_hwnd      = hwnd;
    g_armed     = true;
    g_shown     = false;
    g_armedAtMs = StallDebug::NowMs();
    printf("[BootWindow] armed: Configuration+0x42 set, hwnd=%p stays hidden "
           "until the splash presents a frame\n", (void*)hwnd);
    std::fflush(stdout);
    return true;
}

void ShowNow(const char* reason) {
    if (!g_armed || g_shown || !g_hwnd) return;
    g_shown = true;
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    printf("[BootWindow] revealing hwnd=%p (%s)\n", (void*)g_hwnd,
           reason ? reason : "?");
    std::fflush(stdout);
    StallDebug::Mark("  BootWindow reveal  <== WINDOW NOW VISIBLE");
}

bool IsPending() {
    return g_armed && !g_shown;
}

void PumpWatchdog() {
    if (!IsPending()) return;
    if (StallDebug::NowMs() - g_armedAtMs < kWatchdogMs) return;
    printf("[BootWindow] WARN: watchdog fired — no splash frame presented "
           "within %.0fms; revealing the window anyway\n", kWatchdogMs);
    std::fflush(stdout);
    ShowNow("watchdog");
}

} // namespace BootWindow
