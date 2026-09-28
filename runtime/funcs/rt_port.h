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
// The PORT-REGISTRY resolver: returns our registration for the name, or null
// (reported once by RecordResolveMiss). This is the one to use when runtime/
// code wants to CALL a function.
//
// Declared here 2026-09-07 (Wave 9 lane R) because the runtime tree had no
// port-aware name resolver at all, and that absence had a cost:
// runtime/src/graphics/scene.cpp reached Math::Transform::AsMatrix by ADDRESS,
// which consults only the address registry and so walked past our own Wave 1
// port of it into the DLL — a port bypassing a port inside runtime/. See
// tools/port_bypass_audit.py.
void* ResolveExport(const char* mangled_name);
// B.3.9 (D7): give a minted storage row a C++ home. Call from a *PortInit();
// contract and seeding rule in standalone/rt_data_mint.h / rt_mint_redirect.h.
// Byte-identical to the declaration in standalone/rt_data_mint.h.
void  RtMintRedirectStorage(const char* symbol, void* cell);

// ---- Resolve-and-cache that NEVER caches a null ----------------------------
//
// runtime/funcs/rt_resolve_cache.cpp; read its banner before using either of
// the macros below. Short version: caching a null in a function-local static
// lets the FIRST caller fix the answer for the whole process, so a symbol
// minted later — which is how cutover supplies the exported DATA the DLL used
// to provide — can never be picked up, and nothing says which symbol was
// missing. This caches a hit (so the steady-state cost is one load) and retries
// a miss, naming it once.
// It ALSO refuses to freeze a hit obtained before RuntimePortInit() finished
// (Wave 15 lane ORDER, rt_resolve_cache.cpp §6): that answer is returned but
// not stored, so a site whose first call lands during startup cannot keep the
// DLL's address while the rest of the tree moves to a promoted row. `SaveLoad`
// is promoted on every default run, so that is a live subject, not a
// hypothetical one.
void*    RtResolveCached(const char* mangled, void** cache);
uint64_t RtResolveMissCount();     // 0 across a run == every cached resolve hit
void     RtResolveMissSummary();   // one line; call beside RtDiffSummary()

// ---- Pre-init slot audit (Wave 15 lane ORDER) ------------------------------
//
// RtResolveSlotAudit() re-resolves every NAME that first came through the
// caching door before init finished and compares it with the answer that name
// gave then. Returns the number of LIVE SPLIT BRAINS (answer changed AND the
// site froze the old one), which is 0 by construction unless the negative
// control BANISHED_RT_FREEZE_EARLY=1 restores the old freeze-always behaviour.
// The number to read for EFFECTIVENESS is the `CHANGED answer` count it prints:
// a run where that is 0 says the rule had nothing to bite on
// (reference_probe_needs_an_effectiveness_control.md). Called from
// RtResolveEarlySummary(), so it needs no new call site in standalone/.
int      RtResolveSlotAudit();
uint64_t RtResolvePreInitHitCount();     // hits handed back WITHOUT freezing
uint64_t RtResolvePreInitFrozenCount();  // hits frozen anyway (control armed)

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

// A DEPENDENCY a ported body calls, resolved through the PORT REGISTRY and
// cached once per call site.
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
// Wave 28 lane FACTREST. The `?Create{New,InPlace}Object@` pair for the 35
// NON-UI classes of the absent-factory roster (Graphics 11, Video 7, Object 6,
// Achievement 4, Collision2D 2, Audio 2, IO 1, Pathfinding 1, Picking 1).
// Cross-namespace on purpose: what the 35 share is the SHAPE and the fact that
// the mint reaches every one of them through BaseTypeInfo::factoryFn, not a
// namespace. Kill switch BANISHED_RT_FACTREST, DEFAULT ON — an opt-in switch
// is why dialog_factory.cpp has cleared no trap since Wave 11.
//
// It must run BEFORE RtDataMintInit(), which is what reads these registrations
// and decides whether each of the 70 slots is a port or an abort trap. It may
// run in any position among the other *PortInit()s: nothing in our tree calls
// any of these 70 names today (verified by grep over runtime/ code/ standalone/
// engine/ dx11/ at the branch point), so its registrations change no live call
// site.
void RtFactoriesPortInit();   // runtime/src/*/rt_factories_*.cpp — Wave 28
void RtFactoriesSummary();    // one attendance line; expected to read all zeros
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
// Wave 9 lane X. The fourteen constructors that publish a `?_singleton@...`
// DATA export -- lane C2's tier-1 rows, where the mint would otherwise hand out
// zeroed storage nothing ever writes. Cross-namespace on purpose: what the
// fourteen share is the ACCEPTANCE CRITERION (the ctor must write its singleton
// through the port-aware door), not a namespace.
//
// It must run BEFORE RtDataMintInit(), which reads the port registry to decide
// what each minted row is worth, and it may run after the other waves: nothing
// in our tree calls any of these constructors today, so its registrations
// change no live call site. Its probe -- the only thing that executes these
// bodies before cutover -- runs at the end of it and only when the diff harness
// is on.
void RtSingletonCtorsPortInit();      // runtime/src/engine/ -- Wave 9 lane X
