// src/diag/load_time.cpp -- see load_time.h (NOT RETAIL; block LOADSTAMP).
#include "load_time.h"

#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>   // abi::__cxa_demangle (GCC, both platforms)

#include "diag_threads.h"
#include "platform_thread.h"   // Platform::CurrentThreadId
#include "platform_time.h"     // Platform::PerfCounter / PerfFrequency

namespace LoadTime {

namespace {

int64_t g_freq = 0;
int64_t g_origin = 0;
uint32_t g_mainTid = 0;

// The origin: this file's static initialiser runs on the main thread before main.
struct Origin {
    Origin() {
        Platform::PerfFrequency(&g_freq);
        Platform::PerfCounter(&g_origin);
        g_mainTid = Platform::CurrentThreadId();
        DiagThreads::Register(DiagThreads::kMain);
    }
};
const Origin g_originInit;

}  // namespace

double CounterToMs(int64_t counter) {
    if (g_freq <= 0) return 0.0;
    return static_cast<double>(counter - g_origin) * 1000.0 / static_cast<double>(g_freq);
}

double NowMs() {
    int64_t c = 0;
    Platform::PerfCounter(&c);
    return CounterToMs(c);
}

void Stamp(const char* stage, const char* detail) {
    const double t = NowMs();
    const uint32_t tid = Platform::CurrentThreadId();
    char name[24];
    const char role = (tid == g_mainTid) ? DiagThreads::kMain : DiagThreads::RoleOf(tid);
    if (role != 0) std::snprintf(name, sizeof(name), "%s", DiagThreads::RoleName(role));
    else           std::snprintf(name, sizeof(name), "tid%u", tid);
    std::fprintf(stderr, "[LoadTime] %s t=%.1f thread=%s%s%s\n", stage, t, name,
                 detail ? " " : "", detail ? detail : "");
}

void StampType(const char* stage, const std::type_info& type) {
    int status = -1;
    char* demangled = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    Stamp(stage, (status == 0 && demangled) ? demangled : type.name());
    std::free(demangled);
}

}  // namespace LoadTime

uint64_t LoadTime::Hash64(const void* data, uint64_t bytes, uint64_t h) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (uint64_t i = 0; i < bytes; ++i) { h ^= p[i]; h *= 0x100000001b3ull; }
    return h;
}
