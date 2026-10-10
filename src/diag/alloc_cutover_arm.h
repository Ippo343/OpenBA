#pragma once
// src/diag/alloc_cutover_arm.h -- the instrument seat's composed pair, defined in
// src/diag/alloc_cutover_arm.cpp (read its banner).
//
// Both reach the heap through the RAW MemoryAllocator members, under the allocator
// critical section, with the ALLOCSTATS record and the teardown heap check -- never
// through MemoryAllocator::MemAlloc / MemFree, so a caller sitting ON the seat
// (BANISHED_ALLOC_STATS, BANISHED_REDZONE's shim) cannot re-enter itself.
// `_singleton` is read live.

#include <cstdint>

extern "C" void* RtAllocInstrument_MemAlloc(uint64_t size, int align);
extern "C" void  RtAllocInstrument_MemFree(void* p);

// The allocator's cutover armer (RtAllocCutoverArmer, src/engine/system/
// MemoryAllocator.h), handed to RtSystemAllocatorSetCutoverArmer() by
// src/app/main.cpp before RuntimeInit(): arms BANISHED_ALLOC_STATS on the
// instrument seat. Always returns true.
extern "C" bool RtAllocSwapArmCutover();
