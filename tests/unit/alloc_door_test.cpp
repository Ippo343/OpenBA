// ============================================================================
// tests/unit/alloc_door_test.cpp — ENDGAME B.3.2 closing pass, item 1
//   `rtcoll::DoorAlloc` / `rtcoll::DoorFree` (runtime/src/collection/alloc_door.cpp)
// ============================================================================
//
// WHAT THERE IS TO TEST, GIVEN THERE IS NO ALLOCATOR HERE
// -------------------------------------------------------
// The door does not allocate: it ANSWERS WITH THE FUNCTION TO CALL. Everything
// that can go wrong with it is in that answer, and all of it is reachable from
// a host test through `g_unitResolveAddr` (tests/unit/rt_port_stub.cpp):
//
//   1. the exe dispatch slot (0x140152b60 / 0x140152b68) wins over the runtime
//      address — that is the whole point, because the slot is the door
//      `standalone/redzone_alloc.cpp` rewrites and the runtime address is the
//      one the containers used to resolve behind its back;
//   2. THE SLOT IS RE-READ ON EVERY CALL. `RedzoneAlloc_Install()` overwrites
//      the slot's CONTENTS after the containers have already allocated, so a
//      door that cached the function it first found there would pin the
//      pre-install pointer and put the containers back outside the instrument.
//      This is the case the whole file exists for;
//   3. a MISS IS RETRIED, not frozen — the RtResolveCached contract. Freezing
//      a null would fail every later allocation for the life of the process
//      (Wave 9 attempt 14);
//   4. an unregistered or null slot falls back to the runtime address, which
//      is the ordinary pre-registration boot state.
//
// The two doors have INDEPENDENT caches, so each test below drives its own from
// the virgin state; nothing else in the suite calls either one. The resolver
// hook is installed and cleared inside each test.
// ============================================================================

#include "unit.h"

#include <cstdint>

#include "collection/Collection.h"

extern uintptr_t (*g_unitResolveAddr)(int module, uintptr_t ghidraAddr);

namespace {

// The two candidate answers, told apart by identity only.
void* RuntimeAlloc(uint64_t, int) { return nullptr; }
void* SlotAllocA(uint64_t, int)   { return nullptr; }
void* SlotAllocB(uint64_t, int)   { return nullptr; }
void  RuntimeFree(void*) {}
void  SlotFreeA(void*)   {}
void  SlotFreeB(void*)   {}

// Stand-ins for the exe `.rdata` slots: one pointer of storage each, which the
// resolver hands back the ADDRESS of, exactly as `ResolveAddr(Module::Exe, …)`
// hands back the address of the real slot.
void* g_allocSlot = nullptr;
void* g_freeSlot  = nullptr;

bool g_allocSlotRegistered = false;
bool g_freeSlotRegistered  = false;
bool g_runtimeRegistered   = false;

uintptr_t Resolve(int module, uintptr_t addr) {
    if (module == 0) {   // Module::Exe — the dispatch slots
        if (addr == 0x140152b60 && g_allocSlotRegistered)
            return reinterpret_cast<uintptr_t>(&g_allocSlot);
        if (addr == 0x140152b68 && g_freeSlotRegistered)
            return reinterpret_cast<uintptr_t>(&g_freeSlot);
        return 0;
    }
    if (module == 1 && g_runtimeRegistered) {   // Module::Runtime — the fallback
        if (addr == 0x1800c7a50) return reinterpret_cast<uintptr_t>(&RuntimeAlloc);
        if (addr == 0x1800c7ad0) return reinterpret_cast<uintptr_t>(&RuntimeFree);
    }
    return 0;
}

}  // namespace

UNIT_TEST(alloc_door_prefers_the_dispatch_slot_and_rereads_it) {
    g_unitResolveAddr = &Resolve;

    // (3) Nothing registered yet: no answer — and, crucially, the null is not
    // remembered.
    g_allocSlotRegistered = false;
    g_runtimeRegistered   = false;
    CHECK(rtcoll::DoorAlloc() == nullptr);

    // (3) The runtime address arms (RegisterAllocatorAddresses). The door must
    // pick it up despite having already answered null once.
    g_runtimeRegistered = true;
    CHECK(rtcoll::DoorAlloc() == &RuntimeAlloc);

    // (4) The slot is registered but still empty — the ordinary boot state
    // before standalone/address_registry.cpp fills it. Fall back.
    g_allocSlotRegistered = true;
    g_allocSlot           = nullptr;
    CHECK(rtcoll::DoorAlloc() == &RuntimeAlloc);

    // (1) The slot is filled: it wins over the runtime address.
    g_allocSlot = reinterpret_cast<void*>(&SlotAllocA);
    CHECK(rtcoll::DoorAlloc() == &SlotAllocA);

    // (2) THE CASE THIS FILE EXISTS FOR: `RedzoneAlloc_Install()` rewrites the
    // slot's contents. A door that cached the pointer would still answer
    // SlotAllocA and every container allocation would bypass the shim.
    g_allocSlot = reinterpret_cast<void*>(&SlotAllocB);
    CHECK(rtcoll::DoorAlloc() == &SlotAllocB);

    // (4) And a slot emptied again falls back rather than answering null.
    g_allocSlot = nullptr;
    CHECK(rtcoll::DoorAlloc() == &RuntimeAlloc);

    g_unitResolveAddr = nullptr;
}

UNIT_TEST(free_door_prefers_the_dispatch_slot_and_rereads_it) {
    g_unitResolveAddr = &Resolve;

    g_freeSlotRegistered = false;
    g_runtimeRegistered  = false;
    CHECK(rtcoll::DoorFree() == nullptr);

    g_runtimeRegistered = true;
    CHECK(rtcoll::DoorFree() == &RuntimeFree);

    g_freeSlotRegistered = true;
    g_freeSlot           = nullptr;
    CHECK(rtcoll::DoorFree() == &RuntimeFree);

    g_freeSlot = reinterpret_cast<void*>(&SlotFreeA);
    CHECK(rtcoll::DoorFree() == &SlotFreeA);

    // The alloc and the free must move TOGETHER — a block allocated through the
    // shim and freed around it leaves a stale live-block entry and the canary
    // scan then reports a phantom overflow (redzone_alloc.cpp's
    // `ShimAllocStatic` banner: seen twice as a "corrupt decal vertex pool").
    g_freeSlot = reinterpret_cast<void*>(&SlotFreeB);
    CHECK(rtcoll::DoorFree() == &SlotFreeB);

    g_freeSlot = nullptr;
    CHECK(rtcoll::DoorFree() == &RuntimeFree);

    g_unitResolveAddr = nullptr;
}
