// standalone/reg_pf_pools.cpp
// Pathfinder/Action node-pool grow + dynamic-array resize family registration.
//
// Registers the closure-safe ports in code/Game/Action/action_pf_pools.cpp to their
// Ghidra exe addresses. The integrator wires RegisterPfPoolsBatch() into
// RegisterCodeFunctions() in address_registry.cpp.
//
// Safety: every address registered below is a real function ENTRY and a faithful 1:1 port
// whose entire sub-callee closure is itself satisfiable:
//   * a9b0 / d730 / df40 — node-pool free-list grow (MemAlloc only).
//   * d400 — recursive BST free (self-recursion only).
//   * d830 / db40 / e490 / ee40 / f8d0 / f9e0 — array resize/free/clone (MemAlloc/MemFree only).
//   * d940 — calls f9e0 + db40 (both registered here).
//   * e210 — calls f9e0 + e490 (both registered here).
//
//   * 0x14001fa80 Pf_VectorResize60_fa80 — its 0x60-byte element copy-ctor (0x14001f5e0,
//     recurses fdb0/fe60 internally) and dtor (0x14001fb80) ARE registered as
//     Reimpl_Struct60B_CopyCtor/DtorRange (address_registry.cpp, A/B-proven serialize path),
//     so its closure IS satisfied — registered below.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

extern void* Pf_NodePoolAlloc18_a9b0(uintptr_t, uint32_t*);          // 0x14001a9b0
extern void* Pf_NodePoolAlloc30_d730(uintptr_t, uint32_t*, uint64_t); // 0x14001d730
extern void* Pf_WorklistAlloc_df40(uintptr_t, uint64_t*);            // 0x14001df40
extern void  Pf_BstFreeRecursive_d400(uintptr_t, uint64_t*);         // 0x14001d400
extern void  Pf_ArrayResize18_d830(int64_t*, int);                   // 0x14001d830
extern void  Pf_FreeArray18_db40(uint64_t, uintptr_t, int);          // 0x14001db40
extern void  Pf_FreeArray30_e490(uint64_t, uintptr_t, int);          // 0x14001e490
extern void  Pf_VectorResize14_ee40(int64_t*, int);                  // 0x14001ee40
extern void  Pf_VectorResize14Fill_f8d0(int64_t*, int, uint64_t*);   // 0x14001f8d0
extern void  Pf_VectorResize18Dup_d940(int64_t*, int, uint8_t*);     // 0x14001d940
extern void  Pf_VectorResize30_e210(int64_t*, uint32_t, uint32_t*);  // 0x14001e210
extern int64_t* Pf_IntArrayClone_f9e0(int64_t*, int64_t*);           // 0x14000f9e0
extern void  Pf_VectorResize60_fa80(int64_t*, int, uint64_t*);       // 0x14001fa80

void RegisterPfPoolsBatch() {
    RegisterExeAddr(0x14001a9b0, reinterpret_cast<uintptr_t>(&Pf_NodePoolAlloc18_a9b0));
    RegisterExeAddr(0x14001d730, reinterpret_cast<uintptr_t>(&Pf_NodePoolAlloc30_d730));
    RegisterExeAddr(0x14001df40, reinterpret_cast<uintptr_t>(&Pf_WorklistAlloc_df40));
    RegisterExeAddr(0x14001d400, reinterpret_cast<uintptr_t>(&Pf_BstFreeRecursive_d400));
    RegisterExeAddr(0x14001d830, reinterpret_cast<uintptr_t>(&Pf_ArrayResize18_d830));
    RegisterExeAddr(0x14001db40, reinterpret_cast<uintptr_t>(&Pf_FreeArray18_db40));
    RegisterExeAddr(0x14001e490, reinterpret_cast<uintptr_t>(&Pf_FreeArray30_e490));
    RegisterExeAddr(0x14001ee40, reinterpret_cast<uintptr_t>(&Pf_VectorResize14_ee40));
    RegisterExeAddr(0x14001f8d0, reinterpret_cast<uintptr_t>(&Pf_VectorResize14Fill_f8d0));
    RegisterExeAddr(0x14001d940, reinterpret_cast<uintptr_t>(&Pf_VectorResize18Dup_d940));
    RegisterExeAddr(0x14001e210, reinterpret_cast<uintptr_t>(&Pf_VectorResize30_e210));
    RegisterExeAddr(0x14000f9e0, reinterpret_cast<uintptr_t>(&Pf_IntArrayClone_f9e0));
    // 1fa80's 0x60-element copy-ctor (0x14001f5e0) + dtor (0x14001fb80) ARE registered
    // (Reimpl_Struct60B_CopyCtor/DtorRange in address_registry.cpp, A/B-proven on the
    // serialize path) — closure satisfied, so register it (the last Action-gap entry).
    RegisterExeAddr(0x14001fa80, reinterpret_cast<uintptr_t>(&Pf_VectorResize60_fa80));
}
#endif
