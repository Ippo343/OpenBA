// runtime/funcs/rt_libm.cpp — see rt_libm.h for why this seam exists.
#include "rt_libm.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// Declared here rather than via standalone/resolve.h, which drags in
// <windows.h> — see the note in rt_port.h.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

float OurSqrt(float v)            { return std::sqrt(v); }
float OurSin(float v)             { return std::sin(v); }
float OurCos(float v)             { return std::cos(v); }
float OurAtan2(float y, float x)  { return std::atan2(y, x); }

bool s_usingDll = false;

// Runtime-x64.dll's own statically-linked MSVC CRT (image base 0x180000000).
// Identified from the call sites this wave ported: Vector3::Length calls
// 0x180128110 on a sum of squares; Transform2's ctor calls 0x180127d90 then
// 0x180126d10 on a degrees->radians product to build {cos, sin, -sin, cos};
// Vector3::Heading calls 0x180126680 with two arguments and scales by 180/pi.
constexpr uintptr_t kDllSqrtf = 0x180128110;
constexpr uintptr_t kDllSinf  = 0x180127d90;
constexpr uintptr_t kDllCosf  = 0x180126d10;
constexpr uintptr_t kDllAtan2f = 0x180126680;

}  // namespace

namespace RtLibm {

float (*Sqrt)(float)           = &OurSqrt;
float (*Sin)(float)            = &OurSin;
float (*Cos)(float)            = &OurCos;
float (*Atan2)(float, float)   = &OurAtan2;

bool UsingDllLibm() { return s_usingDll; }

void Init() {
    const char* e = std::getenv("BANISHED_RT_LIBM");
    if (!e || std::strcmp(e, "dll") != 0) {
        // The default, and the only configuration that survives cutover.
        Sqrt = &OurSqrt; Sin = &OurSin; Cos = &OurCos; Atan2 = &OurAtan2;
        s_usingDll = false;
        return;
    }

    typedef float (*Unary)(float);
    typedef float (*Binary)(float, float);
    Unary  sq = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllSqrtf));
    Unary  si = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllSinf));
    Unary  co = reinterpret_cast<Unary>(ResolveRuntimeRaw(kDllCosf));
    Binary at = reinterpret_cast<Binary>(ResolveRuntimeRaw(kDllAtan2f));

    // ResolveRuntimeRaw returns 0 when the DLL is not mapped. Falling back to
    // our own libm is right here: this path is a diagnostic, and a null call
    // would be an EXEC-of-0 crash for a debugging convenience.
    if (!sq || !si || !co || !at) {
        std::fprintf(stderr, "[RtLibm] BANISHED_RT_LIBM=dll requested but the DLL CRT did "
                             "not resolve (sqrt=%p sin=%p cos=%p atan2=%p) — keeping our libm\n",
                     (void*)sq, (void*)si, (void*)co, (void*)at);
        return;
    }
    Sqrt = sq; Sin = si; Cos = co; Atan2 = at;
    s_usingDll = true;
    std::fprintf(stderr, "[RtLibm] BANISHED_RT_LIBM=dll — transcendentals routed through "
                         "Runtime-x64.dll's CRT (DIAGNOSTIC ONLY, does not survive cutover)\n");
}

}  // namespace RtLibm
