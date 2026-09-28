// ============================================================================
// tests/unit/alloc_door_test.cpp — `rtcoll::DoorAlloc` / `rtcoll::DoorFree`
//   (runtime/src/collection/alloc_door.cpp) and the allocator's instrument seat
// ============================================================================
//
// WHAT THESE TESTS PINNED BEFORE B3R W0-A0
// ----------------------------------------
// The door re-read the exe dispatch slot (0x140152b60 / 0x140152b68) on EVERY
// call, answering through `g_unitResolveAddr`, because `RedzoneAlloc_Install()`
// rewrote the slot's contents after the containers had already allocated: a
// door that cached the pointer would have put every container allocation
// outside BANISHED_REDZONE.
//
// WHAT THEY PIN NOW
// -----------------
// There is no slot to re-read: the door answers the allocator's own statics,
// `System::MemoryAllocator::MemAlloc` / `MemFree`, and the instruments sit on
// the ONE seat inside those statics (`System::g_memoryAllocatorInstrument`,
// runtime/types/system/MemoryAllocator.h). So the property the old tests
// protected — "an instrument armed AFTER the containers first allocated still
// sees every later container allocation, and alloc and free move together" —
// is now a property of the seat, and that is what is tested:
//
//   1. the door answers exactly the static (the one allocator API, B3R R9);
//   2. a call through the door reaches whichever hook is in the seat AT THE
//      TIME OF THE CALL: arming, re-arming and disarming after the door has
//      already answered all take effect on the next call;
//   3. the armed hook receives the arguments unchanged and is called once.
//
// The unit build links runtime/src/system/allocator.cpp (tests/unit/sources.mk)
// so the real statics run; with a hook in the seat they never touch
// `_singleton` (null here), which is the seat's contract. Each test clears the
// seat before it ends. No other test arms it.
// ============================================================================

#include "unit.h"

#include <cstdint>

#include "collection/Collection.h"
#include "system/MemoryAllocator.h"

namespace {

int      g_allocCallsA = 0, g_allocCallsB = 0;
uint64_t g_lastSize = 0;
int      g_lastAlign = 0;
char     g_blockA[16], g_blockB[16];

void* HookAllocA(uint64_t size, int align) {
    ++g_allocCallsA; g_lastSize = size; g_lastAlign = align; return g_blockA;
}
void* HookAllocB(uint64_t size, int align) {
    ++g_allocCallsB; g_lastSize = size; g_lastAlign = align; return g_blockB;
}

int   g_freeCallsA = 0, g_freeCallsB = 0;
void* g_lastFreed = nullptr;

void HookFreeA(void* p) { ++g_freeCallsA; g_lastFreed = p; }
void HookFreeB(void* p) { ++g_freeCallsB; g_lastFreed = p; }

}  // namespace

UNIT_TEST(alloc_door_is_memalloc_and_the_instrument_seat_is_read_per_call) {
    System::MemoryAllocatorInstrument& seat = System::g_memoryAllocatorInstrument;
    CHECK(seat.memAlloc == nullptr);

    // (1) The door answers the static itself — no cell, no resolver.
    const rtcoll::DoorAllocFn_t door = rtcoll::DoorAlloc();
    CHECK(door == &System::MemoryAllocator::MemAlloc);

    // (2) Armed AFTER the door answered: the next call is instrumented.
    g_allocCallsA = g_allocCallsB = 0;
    seat.memAlloc = &HookAllocA;
    CHECK(door(0x30, 8) == g_blockA);
    CHECK_EQ(g_allocCallsA, 1);                        // (3) once
    CHECK_EQ(static_cast<int>(g_lastSize), 0x30);      // (3) unchanged
    CHECK_EQ(g_lastAlign, 8);

    // (2) Re-armed: the door, asked again or not, reaches the new hook.
    seat.memAlloc = &HookAllocB;
    CHECK(rtcoll::DoorAlloc()(0x10, 4) == g_blockB);
    CHECK_EQ(g_allocCallsA, 1);
    CHECK_EQ(g_allocCallsB, 1);
    CHECK_EQ(g_lastAlign, 4);

    seat.memAlloc = nullptr;
}

UNIT_TEST(free_door_is_memfree_and_the_instrument_seat_is_read_per_call) {
    System::MemoryAllocatorInstrument& seat = System::g_memoryAllocatorInstrument;
    CHECK(seat.memFree == nullptr);

    const rtcoll::DoorFreeFn_t door = rtcoll::DoorFree();
    CHECK(door == &System::MemoryAllocator::MemFree);

    g_freeCallsA = g_freeCallsB = 0;
    seat.memFree = &HookFreeA;
    door(g_blockA);
    CHECK_EQ(g_freeCallsA, 1);
    CHECK(g_lastFreed == g_blockA);

    // The alloc and the free must move TOGETHER — a block allocated through the
    // shim and freed around it leaves a stale live-block entry and the canary
    // scan then reports a phantom overflow (redzone_alloc.cpp's
    // `ShimAllocStatic` banner). One seat for both makes that structural.
    seat.memFree = &HookFreeB;
    rtcoll::DoorFree()(g_blockB);
    CHECK_EQ(g_freeCallsA, 1);
    CHECK_EQ(g_freeCallsB, 1);
    CHECK(g_lastFreed == g_blockB);

    // A null reaches the hook too: the seat is consulted before retail's own
    // null test, and every armed hook handles null (ShimFree, SwapFree).
    door(nullptr);
    CHECK_EQ(g_freeCallsB, 2);
    CHECK(g_lastFreed == nullptr);

    seat.memFree = nullptr;
}
