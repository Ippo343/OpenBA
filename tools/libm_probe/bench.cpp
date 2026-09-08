// Throughput of the ported bodies vs MinGW's libm vs the DLL's own CRT.
// These five run on hot per-frame paths (Transform::FromYawPitchOffset alone is
// 322k calls in a save-load route), so "bit-exact but slower" is a real risk and
// worth a number rather than a shrug.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include "msvc_libm.h"

typedef float (*Unary)(float);
typedef float (*Binary)(float, float);

static float StdSqrt(float v)           { return std::sqrt(v); }
static float StdSin(float v)            { return std::sin(v); }
static float StdCos(float v)            { return std::cos(v); }
static float StdAtan2(float y, float x) { return std::atan2(y, x); }
static float StdPow(float b, float e)   { return std::pow(b, e); }

static float g_sink;
static const int N = 2000000;

template <class F> static double TimeUnary(F f) {
    auto t0 = std::chrono::steady_clock::now();
    float acc = 0.0f;
    for (int i = 0; i < N; ++i) acc += f((float)i * 0.0011f - 1000.0f);
    auto t1 = std::chrono::steady_clock::now();
    g_sink += acc;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
}
template <class F> static double TimeBinary(F f) {
    auto t0 = std::chrono::steady_clock::now();
    float acc = 0.0f;
    for (int i = 0; i < N; ++i) acc += f((float)i * 0.0011f + 0.5f, (float)(i % 97) * 0.031f + 0.5f);
    auto t1 = std::chrono::steady_clock::now();
    g_sink += acc;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
}

int main() {
    HMODULE h = LoadLibraryA("C:/GOG Games/Banished/Runtime-x64.dll");
    const uintptr_t base = (uintptr_t)h;
    auto R = [&](uintptr_t g) { return (void*)(base + (g - 0x180000000ull)); };
    Unary  dSq = (Unary)R(0x180128110), dSi = (Unary)R(0x180127d90), dCo = (Unary)R(0x180126d10);
    Binary dAt = (Binary)R(0x180126680), dPw = (Binary)R(0x180127560);

    std::printf("ns/call (%d calls each, best of 3)\n", N);
    std::printf("%-8s %10s %10s %10s\n", "fn", "port", "mingw", "dll");
    auto best3u = [](Unary f) { double b = 1e18; for (int i = 0; i < 3; ++i) { double t = TimeUnary(f); if (t < b) b = t; } return b; };
    auto best3b = [](Binary f) { double b = 1e18; for (int i = 0; i < 3; ++i) { double t = TimeBinary(f); if (t < b) b = t; } return b; };
    std::printf("%-8s %10.2f %10.2f %10.2f\n", "sqrtf", best3u(MsvcLibm::Sqrtf), best3u(StdSqrt), best3u(dSq));
    std::printf("%-8s %10.2f %10.2f %10.2f\n", "sinf",  best3u(MsvcLibm::Sinf),  best3u(StdSin),  best3u(dSi));
    std::printf("%-8s %10.2f %10.2f %10.2f\n", "cosf",  best3u(MsvcLibm::Cosf),  best3u(StdCos),  best3u(dCo));
    std::printf("%-8s %10.2f %10.2f %10.2f\n", "atan2f", best3b(MsvcLibm::Atan2f), best3b(StdAtan2), best3b(dAt));
    std::printf("%-8s %10.2f %10.2f %10.2f\n", "powf",  best3b(MsvcLibm::Powf),  best3b(StdPow),  best3b(dPw));
    std::printf("(sink %g)\n", (double)g_sink);
    return 0;
}
