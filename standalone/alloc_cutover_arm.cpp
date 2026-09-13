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
// code called the raw Free directly, which races Allocate), so the allocator
// hands its two ported bodies here and we hand back composed stubs that take
// the allocator critical section at self+0x640 around them. Composing it here
// rather than inside the port keeps runtime/src free of <windows.h> and free of
// non-retail behaviour.
//
// rtalloc::MemAlloc/MemFree take the same lock themselves, faithfully, as
// retail's statics do.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "system/MemoryAllocator.h"   // -Iruntime/types

using rt::system::MemoryAllocator;

// allocator_lock.cpp / redzone_alloc.cpp — the diagnostics the composed stub
// must keep, in the order the lock's own wrappers ran them.
extern "C" void RedzoneAlloc_TeardownOp(const char* what);
extern "C" void AllocStats_RecordAlloc(void* p, uint64_t size, void* caller);
extern "C" void AllocStats_RecordFree(void* p);

namespace {

typedef void* (*OurAllocateFn)(MemoryAllocator* self, uint64_t size, int align);
typedef void  (*OurFreeFn)(MemoryAllocator* self, void* p);

OurAllocateFn g_ourAllocate = nullptr;
OurFreeFn     g_ourFree     = nullptr;

inline LPCRITICAL_SECTION AllocCs(void* self) {
    return reinterpret_cast<LPCRITICAL_SECTION>(
        reinterpret_cast<uint8_t*>(self) + 0x640);   // MemoryAllocator::lock
}

void* SwapAllocate(MemoryAllocator* self, uint64_t size, uintptr_t align) {
    LPCRITICAL_SECTION cs = AllocCs(self);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-alloc");
    void* r = g_ourAllocate(self, size, static_cast<int>(align));
    AllocStats_RecordAlloc(r, size, __builtin_return_address(0));
    LeaveCriticalSection(cs);
    return r;
}

void SwapFree(MemoryAllocator* self, void* p) {
    if (!p) return;                       // retail's TEST RDX,RDX; JZ
    LPCRITICAL_SECTION cs = AllocCs(self);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-free");
    AllocStats_RecordFree(p);
    g_ourFree(self, p);
    LeaveCriticalSection(cs);
}

}  // namespace

// Handed to RtSystemAllocatorSetCutoverArmer() BEFORE RuntimePortInit().
// Signature matches RtAllocCutoverArmer in runtime/types/system/MemoryAllocator.h.
//
// Nothing here installs a detour, and that is not a shortcut: a detour existed
// only to redirect the DLL's OWN internal callers, which were direct rel32 CALLs
// inside the DLL. With no DLL there are none, so registering the exports IS the
// complete swap.
extern "C" bool RtAllocSwapArmCutover(void* ourAllocate, void* ourFree,
                                      void** outComposedAllocate,
                                      void** outComposedFree) {
    if (!ourAllocate || !ourFree || !outComposedAllocate || !outComposedFree) return false;
    g_ourAllocate = reinterpret_cast<OurAllocateFn>(ourAllocate);
    g_ourFree     = reinterpret_cast<OurFreeFn>(ourFree);
    *outComposedAllocate = reinterpret_cast<void*>(&SwapAllocate);
    *outComposedFree     = reinterpret_cast<void*>(&SwapFree);
    fprintf(stderr, "[RtAlloc] cutover arm: ?Allocate@/?Free@ will be the composed "
                    "stubs, which take the allocator CS at self+0x640.\n");
    fflush(stderr);
    return true;
}
