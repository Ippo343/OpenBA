#pragma once
// ============================================================================
// runtime/funcs/rt_libm.h — the transcendental seam for ported Runtime math
// ============================================================================
//
// THE PROBLEM. Runtime-x64.dll's math routines call its own statically-linked
// MSVC CRT. BATCH 4 named all six in retail Ghidra (they were FUN_ until then)
// and registered every one at its retail address, so they are now ordinary
// ported functions rather than a private arrangement of this file:
//   0x180128110 Crt_sqrtf   0x180127d90 Crt_sinf    0x180126d10 Crt_cosf
//   0x180126680 Crt_atan2f  0x180127560 Crt_powf    0x180127010 Crt_fmodf
// `Crt_fmodf` has no seam entry below: its one caller (SunLight::GetSunCamera)
// calls MsvcLibm::Fmodf directly, so BANISHED_RT_LIBM does not cover it.
//
// The five with seam entries below are still routed through it:
// Crt_sqrtf, Crt_sinf, Crt_cosf, Crt_atan2f, Crt_powf (added by Wave 8 lane E for
// Utility::PerlinNoise::Generate — the frequency/amplitude terms are
// pow(2, i) and pow(persistence, i)). Our ports are compiled by MinGW GCC against a
// different libm. Where the two disagree they disagree in the last bit or two,
// which is invisible in gameplay but shows up as a divergence in the Wave 0
// diff oracle (rt_diff.h) — and a last-bit difference in a routine that feeds
// world generation is not automatically harmless, because generation is
// seed-deterministic and small differences compound.
//
// THE WRONG FIX is to call the DLL's helpers by address and call the port done.
// It makes the oracle green by construction while quietly making the port
// depend on the very DLL we are removing: PLAN.md §4.1 decision 1 is that
// reimpl-x64.exe must eventually run with Runtime-x64.dll absent, on non-Windows
// systems. A port that cannot survive the DLL's removal is not a port.
//
// SO: the DEFAULT is our own libm, and the DLL's routines are available only as
// a DIAGNOSTIC, to answer "is this divergence really just libm rounding?"
//
//   (ENDGAME A.3: the DLL routing arm is gone; the ported MSVC CRT bodies in
//   runtime/src/crt/msvc_libm.cpp are the only implementation.)
//
// Resolution happens once, in RtLibmInit(), called from RuntimeInit()
// (standalone/runtime_init.cpp) before anything caches a pointer.
// The indirection is a plain function-pointer call with no branch, so the hot
// path costs one indirect call and no per-call env or guard-variable check.
// ============================================================================

// Set from RuntimeInit(). Defaults are installed at static-init time, so a
// caller that runs before RtLibmInit() still gets our own libm rather than null.
namespace RtLibm {

extern float (*Sqrt)(float);
extern float (*Sin)(float);
extern float (*Cos)(float);
extern float (*Atan2)(float y, float x);
extern float (*Pow)(float base, float exp);

// Registers the ported CRT math bodies and installs them behind the five
// pointers above. Safe to call more than once.
void Init();

}  // namespace RtLibm

// ==== B3R W0-P0c: Runtime bodies reached by retail address ====
// ENDGAME B.3R (B3R_RULES R5): each body below is registered at the retail
// address in its trailing comment and read through the registry today. It is
// declared here, copied from its definition (linkage, namespace, types), so the
// reader can call it by name. Nothing calls it through this declaration yet.
#include <cstddef>   // size_t (RtCrt_Qsort)

// ---- crt/msvc_qsort.cpp
extern "C" void RtCrt_Qsort_18007f050(void* base, size_t num, size_t width, int (*comp)(const void*, const void*));  // 0x18007f050

// ---- crt/msvc_libm.cpp (B3R RT-Core: external linkage given for the reader)
extern "C" float RtCrt_Powf(float b, float e);  // 0x180127560

namespace rtcrt {
// ---- crt/msvc_stricmp.cpp
int StrICmp(const char* a, const char* b);  // 0x18007eefc
}  // namespace rtcrt
// ==== end B3R W0-P0c ====
