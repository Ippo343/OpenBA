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
// (RtMintRedirectStorage, B.3.9 D7's storage redirect, is deleted: ENDGAME
// B.3.9 close retired its last rows. A class static is simply the cell.)

// ---- Resolve-and-cache that NEVER caches a null ----------------------------
//
// runtime/funcs/rt_resolve_cache.cpp; read its banner before using the macro
// below. Short version: caching a null in a function-local static
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

// (RT_DATA_SLOT -- the by-name door to an exported DATA cell -- went with the
// mint at B3R W0-SM M3: every exported static our tree touches is its class's
// own static.)
