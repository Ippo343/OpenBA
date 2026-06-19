// code/Game/Work/work_base_impl2.cpp
// Wave 2 (Work) — impl/geometry/math sub-batch.
//
// Currently contains ONLY the closure-safe math helper:
//   0x140132010  FUN_140132010  — sqrtf (NaN/Inf-aware)  [no sub-callees → safe to register]
//
// The five INTRICATE helpers originally drafted for this sub-batch
//   0x140087fb0 Entity_impl_7fb0, 0x1400f6ef0 BuildStructure_impl_6ef0,
//   0x140122b00 / 0x140122be0 walkable-position search, 0x140122e20 Entity_impl_2e20
// were moved to notes/wave2_drafts/work_base_impl2_full.cpp.draft. They are NOT yet
// registered because each calls exe sub-callees that are still UNREGISTERED
// (0x140003f60 ClearPathQueue, 0x140123070 PosCount, 0x14009f810 MapPathCost, plus
// several dispatch-slot helpers). Registering a helper whose sub-callee closure is
// absent would build a half-null / drifted result — per the wave plan's safety rule
// that is strictly WORSE than the safe no-op of leaving it unregistered. Re-land them
// together with their sub-callee closure during the Action wave, after Ghidra-verifying
// the drafts offset-by-offset.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include <cmath>

// ============================================================
// 0x140132010 — sqrtf with NaN/Inf handling
// ============================================================
// The original routes Inf/NaN/negative through the CRT sqrtf error path
// (CharacterComponent_helper_3e1c) and SQRT()s finite non-negative inputs. A faithful,
// clean reimpl is simply sqrtf, which already produces NaN for negative and +Inf for +Inf.
float WorkImpl_Sqrtf_132010(float param_1) {
    return sqrtf(param_1);
}
