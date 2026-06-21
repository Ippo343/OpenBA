// code/Game/Component/character_trig.cpp
// CharacterComponent transcendental helpers used by the move-tick TAIL
// (CharacterComponent_MoveTick_1094c0 heading quaternion).
//
//   0x140131c90  FUN_140131c90                  — sinf (IEEE arg-reduced)
//   0x140131090  CharacterComponent_helper_1090 — cosf (IEEE arg-reduced)
//
// The originals are full libm sinf/cosf (Payne-Hanek argument reduction + minimax
// polynomials, ~20 .rdata double constants + deep error-path callees). A faithful,
// clean reimpl is simply the CRT sinf/cosf — identical to within ULP, and exactly the
// precedent set by WorkImpl_Sqrtf_132010 (0x140132010 -> sqrtf). The result feeds a
// character heading angle, so any sub-ULP difference is irrelevant.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#ifdef STANDALONE_EXE

#include <cmath>

extern "C" float CharacterComponent_Sinf_131c90(float x) { return sinf(x); }
extern "C" float CharacterComponent_Cosf_131090(float x) { return cosf(x); }

#endif // STANDALONE_EXE
