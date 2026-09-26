#pragma once
// ============================================================================
// runtime/src/engine/mainloop_internal.h — Wave 25 lane BODY
//
// ONE owner for the clock expression `MainLoop` inlines in three places
// (`Run` 0x1800dd3da, `Frame` 0x1800dd119, `Draw` 0x1800dcd40).
// ============================================================================
//
// WHY THIS FILE EXISTS AT ALL.  `mainloop_run.cpp` carried this helper in an
// anonymous namespace, which was right while it had exactly one caller.  Lane
// BODY adds `MainLoop::Frame`, which inlines the SAME four instructions against
// the SAME three globals.  Two copies of a clock that must agree on an EPOCH is
// how the two-epoch bug that mainloop_run.cpp §2 describes gets reintroduced by
// a later edit to one copy — so the helper gets ONE definition (in
// mainloop_frame.cpp) and both files call it through this declaration.
// Nothing about its behaviour changes; this is a move, not a rewrite.
//
// mainloop_run.cpp §2 is still where the REASONING lives: we deliberately do
// NOT call `RtSystem_Timer_CurrentTimeTicks()`, because our Timer keeps its own
// `_systemInitialTick` sampled by OUR PlatformStartup while the DLL's
// `MainLoop::Draw` still reads the DLL's globals.  Mixing the two epochs would
// put `_frameStartTicks` on one and everything `Frame` computes on another.
//
// The header deliberately declares NO types.  `Module` is defined both by
// standalone/resolve.h and locally by several runtime/src files; a third
// definition here would be a hard redefinition in any TU that saw two of them.
// ============================================================================

#include <atomic>
#include <cstdint>

// runtime/funcs/rt_port.h's caching resolver, redeclared so this header stays
// include-order independent (the declaration must stay byte-identical).
void* RtResolveCached(const char* mangled, void** cache);

// ----------------------------------------------------------------------------
// RULE B6-5 / B7-7 door caches (ENDGAME B.3.7 block B1). `Engine::Loader::Run`
// runs on the LOADER thread and dispatches the loop's slot 6 `Load` there, so
// the Engine/Application member bodies keep no function-local first-call cache.
// A door owned by another namespace is resolved through a NAMESPACE-SCOPE
// `std::atomic<void*>` slot with one of these two, the `rtuielem::AtomicExport`
// shape (runtime/src/ui/element_common.h, after the B36_RULES 14.4 fix): same
// never-cache-a-null contract as RT_EXPORT_ONCE, and a by-name answer is kept
// only when RtResolveCached itself would have cached it (a pre-init hit is not
// frozen, rt_resolve_cache.cpp section 6).
// ----------------------------------------------------------------------------
namespace rtengine {

inline void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (scratch != nullptr) slot.store(scratch, std::memory_order_release);
    }
    return p;
}

// For the few address doors (unexported helpers): `resolve` is the caller's own
// `ResolveAddr(Module::Runtime, addr)` wrapper, so this header needs no `Module`.
template <typename Resolve>
inline void* AtomicResolve(std::atomic<void*>& slot, Resolve resolve) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        p = resolve();
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

}  // namespace rtengine

namespace rtmainloop {

// Retail's inlined System::Timer::CurrentTimeTicks:
//
//     CALL [0x18014b1a8]              ; QueryPerformanceCounter
//     SUB  RAX,[0x1801fe738]          ; _systemInitialTick
//     IMUL RAX,[0x1801fe720]          ; _ticksPerSecond (1000000)
//     CQO / IDIV [0x1801fe730]        ; _systemTicksPerSecond (QPF)
//
// Defined in mainloop_frame.cpp.
int64_t CurrentTimeTicksViaGlobals();

}  // namespace rtmainloop
