// standalone/stall_debug.cpp — see stall_debug.h.


#include "stall_debug.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

namespace StallDebug {

namespace {

LARGE_INTEGER g_freq{};
LARGE_INTEGER g_bootCounter{};
double g_lastTickMs = 0.0;
const char* g_lastTickTag = "(none)";
double g_thresholdMs = 30.0;
bool g_initialized = false;

double CounterToMs(LARGE_INTEGER c) {
    if (!g_freq.QuadPart) return 0.0;
    return (double)(c.QuadPart - g_bootCounter.QuadPart) * 1000.0
         / (double)g_freq.QuadPart;
}

} // namespace

void Initialize() {
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_bootCounter);
    g_lastTickMs = 0.0;
    g_lastTickTag = "(boot)";
    g_initialized = true;
    printf("[Stall] Initialize: freq=%lld Hz boot=%lld\n",
                (long long)g_freq.QuadPart, (long long)g_bootCounter.QuadPart);
    std::fflush(stdout);
}

double NowMs() {
    if (!g_initialized) return 0.0;
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return CounterToMs(c);
}

void SetStallThresholdMs(double thresholdMs) {
    g_thresholdMs = thresholdMs;
}

void Tick(const char* tag) {
    if (!g_initialized) return;
    double now = NowMs();
    double gap = now - g_lastTickMs;
    if (gap > g_thresholdMs) {
        printf("[Stall] gap=%.2fms (>%.0fms threshold) — was \"%s\" "
                    "now \"%s\" t=%.2fms\n",
                    gap, g_thresholdMs, g_lastTickTag, tag, now);
        std::fflush(stdout);
    }
    g_lastTickMs = now;
    g_lastTickTag = tag;
}

void Mark(const char* what) {
    if (!g_initialized) return;
    static double lastMarkMs = 0.0;
    double now = NowMs();
    printf("[BootTime] %-44s +%8.2fms   (t=%8.2fms)\n", what, now - lastMarkMs, now);
    std::fflush(stdout);
    lastMarkMs = now;
}

void BeginPhase(const char* name) {
    if (!g_initialized) return;
    printf("[Stall] BEGIN %s @ t=%.2fms\n", name, NowMs());
    std::fflush(stdout);
}

void EndPhase(const char* name) {
    if (!g_initialized) return;
    printf("[Stall] END   %s @ t=%.2fms\n", name, NowMs());
    std::fflush(stdout);
}

Phase::Phase(const char* n) : name(n), startMs(NowMs()) {
    if (!g_initialized) return;
    printf("[Stall] BEGIN %s @ t=%.2fms\n", name, startMs);
    std::fflush(stdout);
}

Phase::~Phase() {
    if (!g_initialized) return;
    double now = NowMs();
    printf("[Stall] END   %-40s duration=%.2fms (t=%.2fms)\n",
                name, now - startMs, now);
    std::fflush(stdout);
}

} // namespace StallDebug

