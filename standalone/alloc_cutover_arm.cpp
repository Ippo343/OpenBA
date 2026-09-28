// ============================================================================
// standalone/alloc_cutover_arm.cpp — System::MemoryAllocator: the composed
//                                    (locked) Allocate/Free entry points
// ============================================================================
//
// This is the surviving half of the old standalone/rt_alloc_swap.cpp. The other
// half — the byte-patch detour installer and the observe-only shadow driver —
// existed only to take the heap away from a loaded Runtime-x64.dll and to
// compare our arena against retail's. There is no DLL any more, so there are no
// DLL-internal callers to redirect and nothing to compare against; both were
// deleted with the rest of the raw door (ENDGAME.md A.3/A.4).
//
// WHAT IS STILL NEEDED. `RtSystemAllocatorRegister()` would otherwise register
// the RAW ported Allocate/Free for ?Allocate@/?Free@. Those bodies are
// byte-faithful and therefore take no lock, exactly as retail's do. Our
// standalone needs the lock (see standalone/allocator_lock.h: Runtime-internal
// code called the raw Free directly, which races Allocate), so we hand back
// composed stubs that take the allocator critical section at self+0x640 around
// the raw members. Composing it here rather than inside the port keeps
// runtime/src free of <windows.h> and free of non-retail behaviour.
//
// System::MemoryAllocator::MemAlloc/MemFree take the same lock themselves,
// faithfully, as retail's statics do.
//
// B3R W0-A0: this file also composes the two diagnostic instruments on the
// allocator's ONE instrument seat (System::g_memoryAllocatorInstrument, see
// RtAllocInstrument_MemAlloc below).
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "system/MemoryAllocator.h"   // -Iruntime/types
#include "options.h"
#include "alloc_cutover_arm.h"        // RtAllocInstrument_MemAlloc / _MemFree (B3R W2-EZ)

using rt::system::MemoryAllocator;

// allocator_lock.cpp / redzone_alloc.cpp — the diagnostics the composed stub
// must keep, in the order the lock's own wrappers ran them.
extern "C" void RedzoneAlloc_TeardownOp(const char* what);
extern "C" void AllocStats_RecordAlloc(void* p, uint64_t size, void* caller);
extern "C" void AllocStats_RecordFree(void* p);

namespace {

inline LPCRITICAL_SECTION AllocCs(void* self) {
    return reinterpret_cast<LPCRITICAL_SECTION>(
        reinterpret_cast<uint8_t*>(self) + 0x640);   // MemoryAllocator::lock
}

// The composed body: lock, the teardown check, the RAW member Allocate (which
// never consults the instrument seat), the stats record, unlock.
void* ComposedAllocate(MemoryAllocator* self, uint64_t size, int align, void* caller) {
    LPCRITICAL_SECTION cs = AllocCs(self);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-alloc");
    void* r = self->Allocate(size, align);
    AllocStats_RecordAlloc(r, size, caller);
    LeaveCriticalSection(cs);
    return r;
}

void* SwapAllocate(MemoryAllocator* self, uint64_t size, uintptr_t align) {
    return ComposedAllocate(self, size, static_cast<int>(align), __builtin_return_address(0));
}

void SwapFree(MemoryAllocator* self, void* p) {
    if (!p) return;                       // retail's TEST RDX,RDX; JZ
    LPCRITICAL_SECTION cs = AllocCs(self);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-free");
    AllocStats_RecordFree(p);
    self->Free(p);                        // RAW member: never the instrument seat
    LeaveCriticalSection(cs);
}

}  // namespace

// ---------------------------------------------------------------------------
// The instrument seat's composed pair (B3R W0-A0, `behaviour:`).
//
// System::g_memoryAllocatorInstrument (MemoryAllocator.h) is consulted at the
// entry of MemoryAllocator::MemAlloc / ::MemFree. These two are what it holds
// when BANISHED_ALLOC_STATS is armed, and what BANISHED_REDZONE's shim calls
// underneath itself for frees (redzone_alloc.cpp's g_realFree). They reach the
// heap through the RAW members only (ComposedAllocate / SwapFree above), never
// through MemAlloc / MemFree, so the seat cannot re-enter itself and an
// allocation is instrumented once.
//
// `_singleton` is read LIVE here, exactly as retail's MemAlloc reads it (not
// the engine_funcs g_pMemAllocSingleton snapshot, which is null until the
// startup self-test refreshes it).
//
// ALLOCSTATS caller attribution on this path is the return address into the
// allocator entry (MemAlloc, or its caller when the compiler tail-calls the
// seat), not always the allocating site: a limitation of the seat, diagnostic
// only.
// ---------------------------------------------------------------------------
extern "C" void* RtAllocInstrument_MemAlloc(uint64_t size, int align) {
    return ComposedAllocate(System::MemoryAllocator::_singleton, size, align,
                            __builtin_return_address(0));
}

extern "C" void RtAllocInstrument_MemFree(void* p) {
    SwapFree(System::MemoryAllocator::_singleton, p);
}

// Handed to RtSystemAllocatorSetCutoverArmer() BEFORE RuntimePortInit().
// Signature matches RtAllocCutoverArmer in runtime/types/system/MemoryAllocator.h.
//
// Nothing here installs a detour, and that is not a shortcut: a detour existed
// only to redirect the DLL's OWN internal callers, which were direct rel32 CALLs
// inside the DLL. With no DLL there are none, so registering the exports IS the
// complete swap. The composed stubs call the RAW members directly; the two
// pointers handed in are checked, not stored.
//
// This is also where BANISHED_ALLOC_STATS arms the instrument seat: the place
// that installs its feeder (SwapAllocate) for ?Allocate@. The seat is taken
// only if nothing holds it; a redzone shim that holds it already feeds the
// stats (its g_realAlloc is SwapAllocate, its g_realFree RtAllocInstrument_MemFree).
extern "C" bool RtAllocSwapArmCutover(void* ourAllocate, void* ourFree,
                                      void** outComposedAllocate,
                                      void** outComposedFree) {
    if (!ourAllocate || !ourFree || !outComposedAllocate || !outComposedFree) return false;
    *outComposedAllocate = reinterpret_cast<void*>(&SwapAllocate);
    *outComposedFree     = reinterpret_cast<void*>(&SwapFree);
    fprintf(stderr, "[RtAlloc] cutover arm: ?Allocate@/?Free@ will be the composed "
                    "stubs, which take the allocator CS at self+0x640.\n");
    if (Opt().allocStats && System::g_memoryAllocatorInstrument.memAlloc == nullptr) {
        System::g_memoryAllocatorInstrument.memAlloc = &RtAllocInstrument_MemAlloc;
        System::g_memoryAllocatorInstrument.memFree  = &RtAllocInstrument_MemFree;
        fprintf(stderr, "[ALLOCSTATS] instrument seat armed: MemoryAllocator::MemAlloc/"
                        "MemFree -> the composed stats pair\n");
    }
    fflush(stderr);
    return true;
}
