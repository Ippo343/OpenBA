// runtime/funcs/rt_libm.cpp — see rt_libm.h for why this seam exists.
#include "rt_libm.h"

#include "../src/crt/msvc_libm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// Declared here rather than via standalone/resolve.h, which drags in
// <windows.h> — see the note in rt_port.h.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

// Our own libm — kept only as an A/B kill switch (BANISHED_RT_LIBM=std). It is
// NOT the default any more: it disagrees with retail in the last bit on roughly
// 0.1–0.2% of ordinary arguments, which is what Vector3::FromYawPitch (16 of
// 3838 calls) and Transform::FromYawPitchOffset (88 of 45473) were reporting.
float StdSqrt(float v)            { return std::sqrt(v); }
float StdSin(float v)             { return std::sin(v); }
float StdCos(float v)             { return std::cos(v); }
float StdAtan2(float y, float x)  { return std::atan2(y, x); }
float StdPow(float b, float e)    { return std::pow(b, e); }

bool s_usingDll = false;

// Runtime-x64.dll's own statically-linked MSVC CRT (image base 0x180000000).
// Identified from the call sites this wave ported: Vector3::Length calls
// 0x180128110 on a sum of squares; Transform2's ctor calls 0x180127d90 then
// 0x180126d10 on a degrees->radians product to build {cos, sin, -sin, cos};
// Vector3::Heading calls 0x180126680 with two arguments and scales by 180/pi.
//
// These five addresses are now PORTED (runtime/src/crt/msvc_libm.cpp) and the
// port is the default. The addresses survive here only so BANISHED_RT_LIBM=dll
// can still be used as the comparison oracle for as long as the DLL exists.
constexpr uintptr_t kDllSqrtf = 0x180128110;
constexpr uintptr_t kDllSinf  = 0x180127d90;
constexpr uintptr_t kDllCosf  = 0x180126d10;
constexpr uintptr_t kDllAtan2f = 0x180126680;
// Wave 8 lane E: Utility::PerlinNoise::Generate (0x1801089b0) calls this at
// two sites — pow(2.0f, i) for the per-octave frequency and
// pow(persistence, i) for the per-octave amplitude.
constexpr uintptr_t kDllPowf  = 0x180127560;

void InstallPorted() {
    RtLibm::Sqrt  = &MsvcLibm::Sqrtf;
    RtLibm::Sin   = &MsvcLibm::Sinf;
    RtLibm::Cos   = &MsvcLibm::Cosf;
    RtLibm::Atan2 = &MsvcLibm::Atan2f;
    RtLibm::Pow   = &MsvcLibm::Powf;
    s_usingDll = false;
}

}  // namespace

// runtime/src/crt/msvc_libm.cpp — registers the six ported CRT math bodies at
// their retail addresses (sqrtf/sinf/cosf/atan2f/powf/fmodf). BATCH 4:
// unconditional, because a port registers unconditionally (DIRECTION.md §2.3)
// and because these six have been the default arithmetic here since Wave 8 —
// they were simply never registered, so the cutover closure still listed them
// and their whole error-reporting subtree as unported.
int RtCrtLibmRegister();
// runtime/src/crt/msvc_qsort.cpp — the CRT qsort/shortsort pair at 0x18007f050 /
// 0x18007f340, reached by ADDRESS from Save::SaveLoad::UpdateList's save-list sort.
int RtCrtQsortRegister();

namespace RtLibm {

// Static-init defaults are the ported bodies, so a caller that runs before
// Init() already gets retail's arithmetic rather than our own libm's.
float (*Sqrt)(float)           = &MsvcLibm::Sqrtf;
float (*Sin)(float)            = &MsvcLibm::Sinf;
float (*Cos)(float)            = &MsvcLibm::Cosf;
float (*Atan2)(float, float)   = &MsvcLibm::Atan2f;
float (*Pow)(float, float)     = &MsvcLibm::Powf;

bool UsingDllLibm() { return s_usingDll; }

void Init() {
    ::RtCrtLibmRegister();
    ::RtCrtQsortRegister();

    const char* e = std::getenv("BANISHED_RT_LIBM");

    if (e && std::strcmp(e, "std") == 0) {
        // A/B kill switch back to MinGW/UCRT's libm. Diverges from retail in
        // the last bit; useful only to reproduce the pre-port measurements.
        Sqrt = &StdSqrt; Sin = &StdSin; Cos = &StdCos;
        Atan2 = &StdAtan2; Pow = &StdPow;
        s_usingDll = false;
        std::fprintf(stderr, "[RtLibm] BANISHED_RT_LIBM=std — transcendentals routed through "
                             "our own libm (DIAGNOSTIC; diverges from retail in the last bit)\n");
        return;
    }

    if (!e || std::strcmp(e, "dll") != 0) {
        InstallPorted();     // the default, and the only one that survives cutover
        return;
    }

    typedef float (*Unary)(float);
    typedef float (*Binary)(float, float);
    Unary  sq = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllSqrtf));
    Unary  si = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllSinf));
    Unary  co = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllCosf));
    Binary at = reinterpret_cast<Binary>(ResolveRuntimeRaw(kDllAtan2f));
    Binary pw = reinterpret_cast<Binary>(ResolveRuntimeRaw(kDllPowf));

    // ResolveRuntimeRaw returns 0 when the DLL is not mapped. Falling back to
    // the ported bodies is right here: this path is a diagnostic, and a null
    // call would be an EXEC-of-0 crash for a debugging convenience.
    if (!sq || !si || !co || !at || !pw) {
        std::fprintf(stderr, "[RtLibm] BANISHED_RT_LIBM=dll requested but the DLL CRT did "
                             "not resolve (sqrt=%p sin=%p cos=%p atan2=%p pow=%p) — keeping the port\n",
                     (void*)sq, (void*)si, (void*)co, (void*)at, (void*)pw);
        InstallPorted();
        return;
    }
    Sqrt = sq; Sin = si; Cos = co; Atan2 = at; Pow = pw;
    s_usingDll = true;
    std::fprintf(stderr, "[RtLibm] BANISHED_RT_LIBM=dll — transcendentals routed through "
                         "Runtime-x64.dll's CRT (DIAGNOSTIC ONLY; the port is the default)\n");
}

}  // namespace RtLibm
