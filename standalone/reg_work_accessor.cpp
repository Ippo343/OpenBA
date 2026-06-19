// standalone/reg_work_accessor.cpp
// Wave 2 (Work) — shared accessor sub-batch registration.
//
// Registers the exe addresses of shared Work accessor / small-loop helpers
// (resolved from several code/Game/Work/*.cpp call sites) to their
// reimplementations in code/Game/Work/work_base_accessor.cpp. The integrator
// wires RegisterWorkAccessorBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include <cstdint>

// FindComponent family (work_base_accessor.cpp) — void* fn(uintptr_t entity, void* retIBP)
extern void* WorkAcc_FindComponent_5380(uintptr_t, void*);   // 0x140005380 type 0x1401c7510
extern void* WorkAcc_FindComponent_5440(uintptr_t, void*);   // 0x140005440 type 0x1401c46b0
extern void* WorkAcc_FindComponent_22520(uintptr_t, void*);  // 0x140022520 type 0x1401c3fb0

// FindComponent (RawMaterial, +0x28/+0x30 layout) — void* fn(uintptr_t compList, void* retOut)
extern void* WorkAcc_FindComponent_6330(uintptr_t, void*);   // 0x140006330 type 0x1401c7cb8

// NOTE: 0x14009c480 (ZoneRefresh) is DEFERRED — its body is present in
// work_base_accessor.cpp but NOT registered here. It calls the still-unregistered
// flood-fill sub-helper 0x14009c170 and mutates per-cell region words (clears them,
// then can't refill without the sub-helper). Registering it would leave region state
// half-cleared; leaving it unregistered is the safe no-op (the call site is skipped).
// Register it once 0x14009c170 lands.

void RegisterWorkAccessorBatch() {
    RegisterExeAddr(0x140005380, reinterpret_cast<uintptr_t>(&WorkAcc_FindComponent_5380));
    RegisterExeAddr(0x140005440, reinterpret_cast<uintptr_t>(&WorkAcc_FindComponent_5440));
    RegisterExeAddr(0x140022520, reinterpret_cast<uintptr_t>(&WorkAcc_FindComponent_22520));
    RegisterExeAddr(0x140006330, reinterpret_cast<uintptr_t>(&WorkAcc_FindComponent_6330));
}
#endif
