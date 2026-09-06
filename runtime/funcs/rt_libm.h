#pragma once
// ============================================================================
// runtime/funcs/rt_libm.h — the transcendental seam for ported Runtime math
// ============================================================================
//
// THE PROBLEM. Runtime-x64.dll's math routines call its own statically-linked
// MSVC CRT: FUN_180128110 (sqrtf), FUN_180127d90 (sinf), FUN_180126d10 (cosf),
// FUN_180126680 (atan2f). Our ports are compiled by MinGW GCC against a
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
//   BANISHED_RT_LIBM=dll   route Sqrt/Sin/Cos/Atan2 through Runtime-x64.dll's
//                          own CRT. If a diff site that was diverging goes
//                          silent under this, the divergence WAS libm rounding
//                          and nothing else. Never a shipping configuration —
//                          it stops working the moment the DLL is unloaded.
//   (unset, the default)   std::sqrt / sin / cos / atan2 from our own libm.
//
// Resolution happens once, in RtLibmInit(), called from RtMathPortInit() — i.e.
// after ResolveInit() has mapped the DLL and before anything caches a pointer.
// The indirection is a plain function-pointer call with no branch, so the hot
// path costs one indirect call and no per-call env or guard-variable check.
// ============================================================================

// Set from RtMathPortInit(). Defaults are installed at static-init time, so a
// caller that runs before RtLibmInit() still gets our own libm rather than null.
namespace RtLibm {

extern float (*Sqrt)(float);
extern float (*Sin)(float);
extern float (*Cos)(float);
extern float (*Atan2)(float y, float x);

// Reads BANISHED_RT_LIBM and, when it says "dll", repoints the four above at
// Runtime-x64.dll's CRT. Reports which set is live. Safe to call more than once.
void Init();

// True when the DLL's CRT is live — for a port that wants to say so in a log.
bool UsingDllLibm();

}  // namespace RtLibm
