#pragma once
// ============================================================================
// runtime/funcs/rt_port.h — shared scaffolding for Runtime port wave files
// ============================================================================
//
// A ported Runtime function lands in three places:
//
//   1. The implementation, a plain free function with the retail raw ABI
//      (see the ABI note in runtime/types/math/Math.h).
//   2. A RegisterRuntimeExport(<mangled name>, &ours) call in the wave's
//      *_PortInit(), which flips every one of our ~1,180 call sites.
//   3. Optionally a detour (standalone/runtime_detour.h) so the DLL's OWN
//      internal callers come to us too.
//
// This header holds only what every wave file needs: the per-wave kill switch
// and the "original DLL function" lookup the diff harness compares against.
//
// Wave init functions are declared at the bottom and called from
// RuntimePortInit() in standalone/resolve_standalone.cpp.
// ============================================================================

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "rt_diff.h"

// The seam's four entry points, DECLARED HERE rather than pulled in from
// standalone/resolve.h. That header includes <windows.h>, and rt_port.h is
// included by every file under runtime/src/ — which PLAN.md §4.1 requires to
// stay platform-neutral so a _posix/_sdl twin can be written later. Including
// resolve.h here leaked <windows.h> into the whole runtime tree, where it
// promptly collided with the DLL's own type names: <wingdi.h> declares a
// function called `Rectangle` (so Volume::Rectangle would not name a type) and
// <windef.h> still #defines the Win16 `near` and `far` keywords away (so a
// frustum's near/far plane distances would not parse).
//
// These declarations must stay byte-identical to the ones in
// standalone/resolve.h; the two are checked against each other by the linker
// and by standalone/resolve_standalone.cpp, which includes both.
void  RegisterRuntimeExport(const char* mangled_name, void* our_fn);
void  RegisterRuntimeAddr(uintptr_t ghidra_addr, void* our_fn);
void  RegisterRuntimeOrdinal(uint16_t ordinal, void* our_fn);
void* ResolveExportRaw(const char* mangled_name);
// The PORT-REGISTRY-AWARE resolver — checks our registrations first and only
// then the shipped DLL. Note the difference from ResolveExportRaw above, which
// deliberately skips the registry for the diff oracle: this is the one to use
// when runtime/ code wants to CALL a function, so that a port of it is reached.
//
// Declared here 2026-09-07 (Wave 9 lane R) because the runtime tree had no
// port-aware name resolver at all, and that absence had a cost:
// runtime/src/graphics/scene.cpp reached Math::Transform::AsMatrix by ADDRESS,
// which consults only the address registry and so walked past our own Wave 1
// port of it into the DLL — a port bypassing a port inside runtime/. See
// tools/port_bypass_audit.py.
void* ResolveExport(const char* mangled_name);

// ---- Resolve-and-cache that NEVER caches a null ----------------------------
//
// runtime/funcs/rt_resolve_cache.cpp; read its banner before using either of
// the macros below. Short version: caching a null in a function-local static
// lets the FIRST caller fix the answer for the whole process, so a symbol
// minted later — which is how cutover supplies the exported DATA the DLL used
// to provide — can never be picked up, and nothing says which symbol was
// missing. This caches a hit (so the steady-state cost is one load) and retries
// a miss, naming it once.
void*    RtResolveCached(const char* mangled, void** cache);
uint64_t RtResolveMissCount();     // 0 across a run == every cached resolve hit
void     RtResolveMissSummary();   // one line; call beside RtDiffSummary()

// ---- Early-resolve recorder ------------------------------------------------
//
// Measures the defect the retry contract above does NOT fix: a NON-null answer
// is still frozen, so a site first called before RuntimePortInit() registered
// its port keeps the DLL's address for the life of the process and bypasses our
// body. Invisible to tools/port_bypass_audit.py (the spelling and the door are
// both correct -- only the ORDERING is wrong) and invisible to the diff oracle
// (a bypassed port never runs, so it never disagrees). Live today, not at
// cutover. See runtime/funcs/rt_resolve_cache.cpp and WAVE9_LANE_H.md §5.
void     RtResolveMarkPortInitDone();   // called at the end of RuntimePortInit()
bool     RtResolvePortInitDone();
void     RtResolveNoteEarly(const char* mangled, uintptr_t ghidra_addr,
                            uint16_t ordinal, void* caller);
uint64_t RtResolveEarlyCount();
void     RtResolveEarlySummary();

// Was this name registered by a port? Distinct from "does ResolveExport return
// something", which cannot tell a registry hit from a DLL hit. Implemented in
// standalone/resolve_standalone.cpp, which owns the table.
bool     RuntimeExportIsRegistered(const char* mangled_name);

// Per-wave kill switch: BANISHED_RT_<NAME>=0 leaves the wave unregistered, so
// every call goes to the DLL exactly as before. Checked once, at init.
inline bool RtWaveEnabled(const char* envName) {
    const char* e = std::getenv(envName);
    return !(e && (std::strcmp(e, "0") == 0 || std::strcmp(e, "false") == 0));
}

// The shipped DLL's implementation of an export, for the diff harness and for
// detour trampolines. NEVER use ResolveExport here: once the port registers,
// that would hand back our own function and the oracle would compare ours to
// ours and always agree.
//
// *** THIS IS NOT A CHEAP ACCESSOR. *** ResolveExportRaw is GetProcAddress: an
// export-table binary search by string, taken under the loader lock, EVERY time
// you call it. It reads like a table lookup and costs like a syscall.
//
// That is not a theoretical worry — it is the whole of the Wave 2 perf-gate
// breach. Two calls sitting in the BODY of System::Time's ctor and AsSeconds
// (the value type every Update(const Time&) passes) cost 5.6 percentage points
// of sim time on their own, and hiding as ntdll self time they were invisible
// to both the sampling profiler's self-time column and the per-component
// profile. See docs/runtime_port/WAVE3_LANE_P.md.
//
// So: inside RT_DIFF_* it is fine (the macro only evaluates it when diffing is
// on, which no shipping run does). ANYWHERE ELSE — that is, on a path a normal
// run executes — resolve once and cache. RT_ORIGINAL_ONCE does exactly that;
// prefer it, and reach for bare RtOriginal only inside a diff guard.
template <typename Fn>
inline Fn RtOriginal(const char* mangled) {
    return reinterpret_cast<Fn>(ResolveExportRaw(mangled));
}

// Resolve once per call site, then reuse. Same value as RtOriginal(MANGLED),
// minus the GetProcAddress on every call after the first.
//
//   using SecondsToTicksFn = int64_t(*)(float);
//   self->ticks = RT_ORIGINAL_ONCE(SecondsToTicksFn,
//                                  "?SecondsToTicks@Timer@System@@SA_JM@Z")(seconds);
//
// The static lives in the enclosing function, so one cache per call site. Fn
// must be a typedef'd name rather than an inline function-pointer type: a bare
// `float(*)(const char*)` contains a comma the preprocessor would split on.
#define RT_ORIGINAL_ONCE(Fn, MANGLED)                                              ([]() -> Fn {                                                                      static Fn rt_once__ = RtOriginal<Fn>(MANGLED);                                 return rt_once__;                                                          }())

// A DEPENDENCY a ported body calls, resolved through the PORT REGISTRY and
// cached, i.e. RT_ORIGINAL_ONCE's shape with ResolveExport instead of
// ResolveExportRaw. Use this whenever the call is part of the work, and
// RT_ORIGINAL_ONCE only when the point is to reach the SHIPPED body (an oracle).
//
//   using QuatMulFn = void* (*)(const void*, void*, const void*);
//   return RT_EXPORT_ONCE(QuatMulFn, "??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z");
//
// It exists because the tree had no cheap port-aware spelling, so nine live
// dependency call sites reached for RT_ORIGINAL_ONCE -- correctly worried about
// GetProcAddress cost (reference_rtoriginal_is_getprocaddress.md), and thereby
// bypassing our own ports. Wave 1 proved ??DQuaternion bit-exact and
// MeshInstance::UpdateHierarchy had been running the DLL's copy ever since.
// One static per call site, and Fn must be a typedef'd name.
//
// It does NOT share RT_ORIGINAL_ONCE's caching contract any more (Wave 9 lane
// H): a HIT is cached exactly as before, but a MISS is named and retried rather
// than frozen. RT_ORIGINAL_ONCE keeps the old behaviour deliberately — it is
// the oracle door, it must reach the SHIPPED body, and at cutover its call
// sites are compiled out rather than made to retry a GetProcAddress against a
// module that is not there (WAVE9.md §2).
#define RT_EXPORT_ONCE(Fn, MANGLED)  ([]() -> Fn { static void* rt_export_once__ = nullptr; return reinterpret_cast<Fn>(RtResolveCached((MANGLED), &rt_export_once__)); }())

// A DATA slot the DLL exports -- ?_singleton@..., ?_typeInfo@..., ?_firstTypeInfo@...
// The export ADDRESS is the slot, so T is normally a pointer-to-pointer.
//
//     MemoryAllocator** SingletonSlot() {
//         return RT_DATA_SLOT(MemoryAllocator**,
//                             "?_singleton@MemoryAllocator@System@@0PEAV12@EA");
//     }
//
// Same never-cache-a-null contract as RT_EXPORT_ONCE, and for data it matters
// more: a null function pointer dies loudly as EXEC-of-0 with the VEH handler
// naming the detour, while a null data slot is either swallowed by an `if` or
// dereferenced somewhere else entirely. T must be a typedef'd name or a
// comma-free type, because the preprocessor splits on commas.
#define RT_DATA_SLOT(T, MANGLED)  ([]() -> T { static void* rt_data_slot__ = nullptr; return reinterpret_cast<T>(RtResolveCached((MANGLED), &rt_data_slot__)); }())

// ---- Wave init entry points ------------------------------------------------
void RtMathPortInit();     // runtime/src/math/
void RtVolumePortInit();   // runtime/src/volume/
void RtSystemPortInit();   // runtime/src/system/ — Wave 2
void RtObjectPortInit();   // runtime/src/object/ — Wave 3
void RtIoPortInit();       // runtime/src/io/ — Wave 4
void RtAchievementPortInit(); // runtime/src/achievement/ — Wave 5
void RtSavePortInit();     // runtime/src/save/ — Wave 5
void RtPickingPortInit();  // runtime/src/picking/ — Wave 5
void RtVideoPortInit();    // runtime/src/video/ — Wave 6
void RtGraphicsPortInit(); // runtime/src/graphics/ — Wave 6
void RtUiPortInit();       // runtime/src/ui/ - Wave 7
// Wave 8 (Top). Lanes A+B share runtime/src/engine/rt_engine_port.cpp; C, D and E
// each own their own aggregator. Save's Wave 8 remainder is registered from the
// existing RtSavePortInit() above rather than a second Save aggregator.
void RtEnginePortInit();      // runtime/src/engine/ — Wave 8 lanes A+B
void RtAudioPortInit();       // runtime/src/audio/ — Wave 8 lane C
void RtInputPortInit();       // runtime/src/input/ — Wave 8 lane D
void RtApplicationPortInit(); // runtime/src/application/ — Wave 8 lane E
void RtUtilityPortInit();     // runtime/src/utility/ — Wave 8 lane E
// Wave 9 lane Q. Collection::Array<T> is header-only with NO exports in either
// build, so this wave registers only ADDRESSES (RegisterRuntimeAddr) — 27 of
// them for two bodies. It must run LAST of the wave inits: registering
// 0x18007a0d0 and friends changes what ResolveAddr hands back at ~20 existing
// call sites in the other waves, and running it last keeps that one effect at
// one point in the init order.
void RtCollectionPortInit();  // runtime/src/collection/ — Wave 9 lane Q
// Wave 9 lane D2. The same header-only cluster one level deeper: ~Array for
// non-trivially-destructible T (lane D's J2/J2b), the eleven per-element
// destroy helpers those bodies call -- which turned out to be THREE bodies --
// and Array<T*>::operator=. 18 addresses.
//
// It must run AFTER RtCollectionPortInit(), for one reason only: the probe it
// arms drives lane Q's ArrayDtorImpl, and arming a probe before the code it
// probes is registered is a hazard nobody needs. The REGISTRATIONS themselves
// are order-independent -- array.cpp reaches this file's three shared helper
// addresses through function-local `static Fn fn = ResolveAddr(...)`, which
// initialises on first USE (a real Array call) and not at init.
void RtCollectionDestroyPortInit();  // runtime/src/collection/ -- Wave 9 lane D2
// Wave 9 lane K. The unnamed deleting-destructor family: slot 0 of 37 of lane
// C2's minted vtables, all ONE body (MSVC's VECTOR deleting destructor, not the
// scalar one) parameterized by ~Class and sizeof(Class). Registered by ADDRESS
// only -- none of the 37 is exported under any name.
//
// It must run AFTER every other wave init: it resolves ~Class through the
// port-aware ResolveAddr, and its probe runs at the end of the init, so a
// destructor registered by a LATER init would be invisible to the probe. It
// must run BEFORE RtDataMintInit(), which is what fills the 37 slots.
void RtObjectDeletingDtorPortInit();  // runtime/src/object/ -- Wave 9 lane K
