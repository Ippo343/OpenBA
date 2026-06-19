// standalone/reg_work_impl2.cpp
// Wave 2 (Work) — impl/geometry/math sub-batch registration.
//
// Registers only the closure-safe math helper. The five intricate impl/geometry
// helpers (Entity_impl_7fb0, BuildStructure_impl_6ef0, the walkable-position search
// pair, Entity_impl_2e20) are drafted in notes/wave2_drafts/work_base_impl2_full.cpp.draft
// and DEFERRED — they depend on still-unregistered exe sub-callees, so registering them
// would produce a half-null/drifted result. Land them with their sub-callee closure
// during the Action wave (after Ghidra-verifying the draft).
//
// Safety: address below verified as a real function ENTRY in Ghidra and a faithful port.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// 0x140132010  sqrtf (NaN/Inf-aware) — no sub-callees.
extern float WorkImpl_Sqrtf_132010(float val);

void RegisterWorkImpl2Batch() {
    RegisterExeAddr(0x140132010, reinterpret_cast<uintptr_t>(&WorkImpl_Sqrtf_132010));
}
#endif
