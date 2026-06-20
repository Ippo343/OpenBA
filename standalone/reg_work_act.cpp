// standalone/reg_work_act.cpp
// Phase 2 Keystone B3 — registration for the closure-safe Work action-queue builders
// (work_act_perform.cpp / work_act_rawmat.cpp / work_act_hunt.cpp). Wired via
// RegisterWorkActBatch(). Each is a faithful 1:1 port, opus-ported + lead-verified
// offset-by-offset against the disassembly. Closure: every action child-ctor they call
// is registered (B1 child-ctors + B3 leaf copy-ctors + the pre-existing pf ctors).
//
// bffd0 (PerformWork_Action_Ctor, cleararea) is now closure-safe after the ca520
// cascade landed (ca520/ca230 + RawMaterialDescription type-id global 0x1401c7cb8).
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void WorkAct_PerformWorkCopyCtor_ba9f0(void*, uint32_t, float*, float*, float*, int64_t*); // 0x1400ba9f0
extern void WorkAct_GatherWorkCtor_bb7b0(void*, uint32_t, int64_t*, float*, uint64_t, char, float, uint32_t, uint32_t); // 0x1400bb7b0
extern void WorkAct_FishWorkCtor_beae0(void*, uint32_t, int64_t*, float*, float, uint64_t, float, uint32_t, uint32_t); // 0x1400beae0
extern void WorkAct_HuntWorkCtor_bbbc0(void*, uint32_t, int64_t*, float*, float, char); // 0x1400bbbc0
extern void WorkAct_ShowToolCtor3_bdd20(void*, uint32_t, uint32_t*, uint32_t, float*, int, uint32_t*, uint32_t, int64_t*, uint32_t*, uint64_t, int64_t); // 0x1400bdd20
extern void WorkAct_PerformWorkCtor_bffd0(void*, uint32_t, uint32_t*, int, uint32_t, uint8_t, uint32_t, int64_t, int, float*, int64_t*); // 0x1400bffd0

void RegisterWorkActBatch() {
    RegisterExeAddr(0x1400ba9f0, reinterpret_cast<uintptr_t>(&WorkAct_PerformWorkCopyCtor_ba9f0));
    RegisterExeAddr(0x1400bb7b0, reinterpret_cast<uintptr_t>(&WorkAct_GatherWorkCtor_bb7b0));
    RegisterExeAddr(0x1400beae0, reinterpret_cast<uintptr_t>(&WorkAct_FishWorkCtor_beae0));
    RegisterExeAddr(0x1400bbbc0, reinterpret_cast<uintptr_t>(&WorkAct_HuntWorkCtor_bbbc0));
    RegisterExeAddr(0x1400bdd20, reinterpret_cast<uintptr_t>(&WorkAct_ShowToolCtor3_bdd20));
    RegisterExeAddr(0x1400bffd0, reinterpret_cast<uintptr_t>(&WorkAct_PerformWorkCtor_bffd0));
}
#endif // STANDALONE_EXE
