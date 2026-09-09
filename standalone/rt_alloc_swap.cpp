// ============================================================================
// standalone/rt_alloc_swap.cpp — System::MemoryAllocator: atomic swap + shadow
// ============================================================================
//
// Wave 9 lane E. This is gate steps 2 and 3 of docs/runtime_port/WAVE2_ALLOCATOR.md
// §5 — the code that makes MEASUREMENT possible. It does not flip the verdict;
// it makes the run that could flip it runnable.
//
// Everything here is default OFF. A build with this file linked and no env var
// set behaves exactly as the tree did before it existed.
//
// ---------------------------------------------------------------------------
// WHY THE SHADOW DRIVER IS *NOT* A SECOND DETOUR (correction to §3)
// ---------------------------------------------------------------------------
// WAVE2_ALLOCATOR.md §3's "status" list asks for "a driver in standalone/ that
// detours MemoryAllocator::Startup, Allocate and Free in observe-only mode".
// Read literally that is broken, twice over, and by the doc's own §4:
//
//   * allocator_lock.cpp is DEFAULT-ON and already owns both prologues. A
//     second detour on the same 13/12 bytes is precisely the hazard §4
//     describes — whichever installs second either clobbers the first's saved
//     trampoline bytes or is refused by its memcmp guard.
//   * The replica must be mutated in the SAME serialized order as the real
//     arena. The real arena is serialized by the critical section that
//     allocator_lock's wrappers take. An observer outside that lock would
//     replay two threads' allocations in an order the real allocator never
//     saw, and the offsets would desynchronise for reasons that have nothing
//     to do with the port.
//
// So the observer rides INSIDE allocator_lock's existing AllocateLocked /
// FreeLocked wrappers, through two function pointers that are null unless this
// file arms them. That is what §3's own next sentence — "this is the existing
// allocator_lock.cpp shape with two extra calls" — actually describes. Only
// Startup gets a real detour, because nobody else patches it.
//
// Consequence to state up front: the shadow REQUIRES the allocator lock. With
// BANISHED_ALLOC_LOCK=0 there are no wrappers and the shadow observes nothing.
// Arming refuses loudly in that case rather than reporting calls=0, which is
// the Wave 1 defect #4 trap (a green summary over an empty measurement).
//
// ---------------------------------------------------------------------------
// PROLOGUES (bytes read out of the shipped DLL with pefile, not from a
// mnemonic listing — this build emits redundant REX prefixes)
// ---------------------------------------------------------------------------
//   Allocate 0x1800c6e60: 40 53 | 57 | 41 54 | 41 56 | 41 57 | 48 83 EC 20
//                         PUSH RBX/RDI/R12/R14/R15; SUB RSP,0x20   -> 13 bytes
//   Free     0x1800c6c00: 48 85 D2 | 0F 84 4F 02 00 00 | 48 8B C4
//                         TEST RDX,RDX; JZ Free+0x258; MOV RAX,RSP -> 12 bytes
// Both boundaries counted from the disassembly, not from Ghidra's C.
//
// ---------------------------------------------------------------------------
// THE ARMING TRIGGER IS Engine::Engine::Startup, NOT MemoryAllocator::Startup
// ---------------------------------------------------------------------------
// Wave 9 lane E2 took the first shadow measurement and it came back
// `armed=0 observed=0 skipped_pre_startup=1788875`. The driver was waiting on
// ?Startup@MemoryAllocator@System@@QEAA_N_K0@Z (0x1800c7620), and that function
// is NEVER CALLED in the shipped game. Proved two independent ways over the
// shipped binaries, not inferred:
//
//   * a full rel32 scan of Runtime-x64.dll's .text for CALL/JMP to 0x1800c7620
//     finds ZERO sites. (The same scan finds 3 for AddNewMemoryRange, 348 for
//     Allocate, 1 for the ctor — so the scan works.)
//   * Application-x64.exe's PE import table carries exactly two MemoryAllocator
//     symbols: MemAlloc and MemFree. Startup is not among them.
//
// MSVC INLINED it into ?Startup@Engine@1@QEAAIPEBDII_N@Z (0x1800db8a0). That
// body writes minRangeMB<<20 to +0x10 and maxTotalMB<<20 to +0x18, reads the
// page size out of GetSystemInfo into +0x668, and then calls AddNewMemoryRange
// at 0x1800db905 — the inlined copy of Startup, instruction for instruction.
// It is the only place the arena is ever created.
//
//   Engine::Startup 0x1800db8a0: 48 8B C4 | 55 | 48 8B EC | 48 83 EC 60 |
//                                48 C7 45 C0 FE FF FF FF          -> 19 bytes
//     MOV RAX,RSP is an RSP capture; InstallDetour detects and corrects it.
//     RCX=this (the Engine, whose offset 0 IS the MemoryAllocator),
//     RDX=name, R8D=minRangeMB, R9D=maxTotalMB, stack=bool. Returns unsigned.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "rt_alloc_swap.h"
#include "allocator_lock.h"
#include "runtime_detour.h"
#include "resolve.h"

#include "system/MemoryAllocator.h"   // -Iruntime/types

using rt::system::MemoryAllocator;

// allocator_lock.cpp — the three behaviours the composed stub must keep, and
// the two observer hooks the shadow driver rides on.
extern "C" void RedzoneAlloc_TeardownOp(const char* what);      // redzone_alloc.cpp
extern "C" void AllocStats_RecordAlloc(void* p, uint64_t size, void* caller);
extern "C" void AllocStats_RecordFree(void* p);
extern "C" void (*g_allocShadowOnAlloc)(void* self, uint64_t size, uintptr_t align, void* result);
extern "C" void (*g_allocShadowOnFree)(void* self, void* ptr);
extern "C" void (*g_allocShadowPreOp)(void* self);
extern "C" void (*g_allocShadowPostOp)(void* self);

namespace {

typedef void* (*OurAllocateFn)(MemoryAllocator* self, uint64_t size, int align);
typedef void  (*OurFreeFn)(MemoryAllocator* self, void* p);

OurAllocateFn g_ourAllocate = nullptr;
OurFreeFn     g_ourFree     = nullptr;
bool          g_swapTaken   = false;

inline LPCRITICAL_SECTION AllocCs(void* self) {
    return reinterpret_cast<LPCRITICAL_SECTION>(
        reinterpret_cast<uint8_t*>(self) + 0x640);   // MemoryAllocator::lock
}

// ---------------------------------------------------------------------------
// The composed stubs. Lock + diagnostics + OUR body.
//
// The port itself stays byte-faithful and takes no lock, exactly as retail's
// Allocate/Free do; the lock is the standalone's own correction (see
// allocator_lock.h: Runtime-internal code calls the raw Free directly, which
// races Allocate). Composing it here rather than inside the port keeps
// runtime/src free of <windows.h> and free of non-retail behaviour.
// ---------------------------------------------------------------------------
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

RtDetour g_allocDetour = {};
RtDetour g_freeDetour  = {};

// ---------------------------------------------------------------------------
// Shadow driver state
// ---------------------------------------------------------------------------
bool             g_shadowOn      = false;   // env said yes AND we armed
MemoryAllocator* g_shadowSelf    = nullptr; // the ONE allocator we observe
uint64_t         g_shadowSkipped = 0;       // observations dropped (see below)
uint64_t         g_shadowOther   = 0;       // calls on a different allocator
RtDetour         g_startupDetour = {};

typedef unsigned (*EngineStartupFn)(void* self, const char* name, unsigned minMB,
                                    unsigned maxMB, bool noConfigure);

// Observe-only Engine::Startup — the function that INLINES
// MemoryAllocator::Startup (see the header comment; the exported one has no
// callers). `self` is the Engine, and the MemoryAllocator is embedded at its
// offset 0, so the same pointer is both.
//
// ARM BEFORE CALLING THE ORIGINAL. Lane E2's second measurement armed after it
// and read `live=12765 bytes=906440 ranges=1` at that point with 17,017 calls
// already skipped: Engine::Startup does not merely start the allocator, it
// boots the whole engine, and ~12.8k of the process's allocations happen inside
// it, AFTER the inlined MemoryAllocator::Startup at 0x1800db905. Arming on the
// way out therefore starts the replica from a state the real arena had long
// left, and every one of the 1,684,998 comparisons diverged for that reason and
// that reason alone — a harness defect that reads exactly like a catastrophic
// port defect (reference_probe_needs_an_effectiveness_control.md).
//
// Entry to Engine::Startup is the one virgin instant: the arena cannot predate
// it, because AddNewMemoryRange divides by self->pageSize and the ctor leaves
// pageSize at 0 (allocator.cpp Construct) — the first allocation before Startup
// would divide by zero. So the invariant asserted below (rangeCount==0 &&
// allocCount==0 && nothing skipped) is not a hope; it is structural, and if it
// ever fails the run is declared INVALID instead of measured.
unsigned ShadowStartup(void* selfv, const char* name, unsigned minMB,
                       unsigned maxMB, bool noConfigure) {
    EngineStartupFn orig = reinterpret_cast<EngineStartupFn>(g_startupDetour.trampoline);
    MemoryAllocator* self = static_cast<MemoryAllocator*>(selfv);

    if (g_shadowSelf == nullptr) {
        // EFFECTIVENESS CONTROL, at the one instant it means anything.
        fprintf(stderr, "[RtAllocShadow] Engine::Startup ENTRY (min=%uMB max=%uMB): "
                        "real arena live=%d bytes=%lld ranges=%d, skipped so far=%llu\n",
                minMB, maxMB, self->allocCount, (long long)self->allocatedBytes,
                self->rangeCount, (unsigned long long)g_shadowSkipped);
        if (self->rangeCount != 0 || self->allocCount != 0 ||
            self->allocatedBytes != 0 || g_shadowSkipped != 0) {
            fprintf(stderr, "[RtAllocShadow] REFUSING to arm: the arena is ALREADY "
                            "LIVE at Engine::Startup entry. The replica could only "
                            "start from a state the real arena has left, so every "
                            "comparison would diverge for a harness reason. This run "
                            "is INVALID, not clean.\n");
            g_shadowOn = false;
        } else {
            g_shadowSelf = self;
            if (!RtAllocShadowStartup(minMB, maxMB)) {
                fprintf(stderr, "[RtAllocShadow] replica arena failed to arm — "
                                "shadow DISABLED for this run\n");
                g_shadowSelf = nullptr;
                g_shadowOn   = false;
            } else {
                fprintf(stderr, "[RtAllocShadow] observing MemoryAllocator %p "
                                "(min=%uMB max=%uMB)\n", (void*)self, minMB, maxMB);
            }
        }
        fflush(stderr);
    } else if (self != g_shadowSelf) {
        // A SECOND allocator was started. Replaying its traffic into the one
        // replica would desynchronise it; say so rather than produce numbers.
        fprintf(stderr, "[RtAllocShadow] WARNING: a second MemoryAllocator (%p) "
                        "was started; its traffic is NOT observed\n", (void*)self);
        fflush(stderr);
    }
    // The replica is armed and virgin; NOW run the real body, so every
    // allocation Engine::Startup itself performs is observed.
    return orig(selfv, name, minMB, maxMB, noConfigure);
}

// Called from AllocateLocked/FreeLocked, INSIDE the allocator critical section.
void ShadowOnAlloc(void* self, uint64_t size, uintptr_t align, void* result) {
    if (self != g_shadowSelf) { if (g_shadowSelf) g_shadowOther++; else g_shadowSkipped++; return; }
    RtAllocShadowAlloc(size, static_cast<int>(align), result,
                       static_cast<const MemoryAllocator*>(self));
}

// Wave 10 lane SHADOWBISECT. Filtered to the replicated allocator exactly as
// the two observation hooks are; the detector reads only the REAL allocator.
void ShadowPreOp(void* self) {
    if (self != g_shadowSelf) return;
    RtAllocShadowPreOp(static_cast<const MemoryAllocator*>(self));
}
void ShadowPostOp(void* self) {
    if (self != g_shadowSelf) return;
    RtAllocShadowPostOp(static_cast<const MemoryAllocator*>(self));
}

void ShadowOnFree(void* self, void* ptr) {
    if (self != g_shadowSelf) { if (g_shadowSelf) g_shadowOther++; else g_shadowSkipped++; return; }
    RtAllocShadowFree(ptr, static_cast<const MemoryAllocator*>(self));
}

bool EnvOn(const char* name) {
    const char* e = getenv(name);
    return e && e[0] && e[0] != '0' && e[0] != 'f' && e[0] != 'F';
}

}  // namespace

// ============================================================================
// The atomic swap installer
// ============================================================================
//
// Contract (runtime/types/system/MemoryAllocator.h): return true only if BOTH
// detours took. RtSystemAllocatorRegister() registers NOTHING when this returns
// false, so a half-installed swap can never leave the exports pointing at a
// port the DLL does not use. Both failure paths below therefore leave the DLL
// byte-identical to how they found it.
// ============================================================================
extern "C" bool RtAllocSwapInstall(void* ourAllocate, void* ourFree) {
    if (!ourAllocate || !ourFree) return false;
    g_ourAllocate = reinterpret_cast<OurAllocateFn>(ourAllocate);
    g_ourFree     = reinterpret_cast<OurFreeFn>(ourFree);

    // Hazard A (PLAN.md §2.2, WAVE2_ALLOCATOR.md §4): RETIRE the allocator lock
    // patches, do not carry them. Retiring restores retail's bytes, which is
    // also what makes the InstallRuntimeDetour prologue checks below meaningful.
    if (AllocatorLockRetireForPort())
        fprintf(stderr, "[RtAllocSwap] retired the allocator_lock patches\n");

    // The composed stubs take the critical section themselves, so nothing is
    // lost by retiring — see SwapAllocate/SwapFree above.
    static const uint8_t kAllocPrologue[13] = {
        0x40,0x53,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20
    };
    static const uint8_t kFreePrologue[12] = {
        0x48,0x85,0xD2, 0x0F,0x84,0x4F,0x02,0x00,0x00, 0x48,0x8B,0xC4
    };

    // No trampolines are used: our body REPLACES retail's, it does not wrap it.
    if (!InstallRuntimeDetourChecked("MemoryAllocator::Allocate[port]", 0x1800c6e60,
                                     reinterpret_cast<void*>(&SwapAllocate),
                                     &g_allocDetour, 13,
                                     kAllocPrologue, sizeof(kAllocPrologue))) {
        fprintf(stderr, "[RtAllocSwap] Allocate detour FAILED — registering nothing\n");
        fflush(stderr);
        return false;
    }
    if (!InstallRuntimeDetourChecked("MemoryAllocator::Free[port]", 0x1800c6c00,
                                     reinterpret_cast<void*>(&SwapFree),
                                     &g_freeDetour, 12,
                                     kFreePrologue, sizeof(kFreePrologue))) {
        fprintf(stderr, "[RtAllocSwap] Free detour FAILED — rolling back Allocate, "
                        "registering nothing\n");
        RemoveDetour(&g_allocDetour);
        fflush(stderr);
        return false;
    }

    g_swapTaken = true;
    fprintf(stderr, "[RtAllocSwap] ATOMIC SWAP INSTALLED — every Allocate/Free in the "
                    "process now runs our port under the allocator CS\n");
    fflush(stderr);
    return true;
}

// ============================================================================
// CUTOVER ARMING -- no DLL, so no detour is possible OR necessary
// ============================================================================
//
// Wave 9 lane E4. The DLL-absent boot ran with BANISHED_RT_ALLOC=1 and MemAlloc
// and MemFree still came back on the unresolved list. The arming path was wired
// all along (runtime/src/system/rt_system_port.cpp reads the env var and calls
// RtSystemAllocatorRegister); what blocked it was its OWN SAFETY GUARD.
// FindPatchedPrologue() reads the DLL's bytes through ResolveExportRaw to check
// nobody else owns the two prologues, and with the module gone that resolves
// null, reports "unresolved (Runtime-x64.dll not loaded?)" and REFUSES. Correct
// by its own logic; wrong for the situation.
//
// Nothing here installs a detour, and that is not a shortcut. A detour exists
// only to redirect the DLL's OWN internal callers, which are direct rel32 CALLs
// inside the DLL. With no DLL there are none, so registering the exports IS the
// complete swap and the atomicity requirement has nothing left to protect.
//
// What does NOT come for free is the critical section. On the DLL-present path
// it arrives either from allocator_lock.cpp's prologue patches or from the
// composed detour stub; at cutover neither exists, and retail's own Allocate and
// Free take no lock (allocator_lock.h). So we hand back the SAME composed stubs
// the detour would have installed, and RtSystemAllocatorRegister registers those
// for ?Allocate@/?Free@ rather than the raw bodies. rtalloc::MemAlloc/MemFree
// take the lock themselves, faithfully, as retail's statics do.
extern "C" bool RtAllocSwapArmCutover(void* ourAllocate, void* ourFree,
                                      void** outComposedAllocate,
                                      void** outComposedFree) {
    if (!ourAllocate || !ourFree || !outComposedAllocate || !outComposedFree) return false;
    g_ourAllocate = reinterpret_cast<OurAllocateFn>(ourAllocate);
    g_ourFree     = reinterpret_cast<OurFreeFn>(ourFree);
    *outComposedAllocate = reinterpret_cast<void*>(&SwapAllocate);
    *outComposedFree     = reinterpret_cast<void*>(&SwapFree);

    // Nothing to retire: with no DLL there is no prologue for allocator_lock to
    // have patched. Say so rather than leave it ambiguous -- a silent skip here
    // is exactly how "the lock quietly disappeared" would look.
    fprintf(stderr, "[RtAllocSwap] cutover arm: no detour installed (no DLL, so no "
                    "DLL-internal callers to redirect); ?Allocate@/?Free@ will be the "
                    "composed stubs, which take the allocator CS at self+0x640.\n");
    fflush(stderr);
    return true;
}

// ============================================================================
// Post-registration fixup — see WAVE9_LANE_E.md "Correction 3"
// ============================================================================
// RtSystemAllocatorRegister() registers the RAW ported bodies for
// ?Allocate@ / ?Free@. Those bodies are byte-faithful and therefore take no
// lock. Retail's own callers reach the LOCKED composed stub through the detour;
// ours would reach the unlocked body through the registry — two entry points to
// one free list with different locking. Nothing in code/ resolves those two
// names today (every consumer uses the MemAlloc/MemFree statics, which do lock),
// so this is a latent trap rather than a live defect, and this closes it.
extern "C" void RtAllocSwapPostRegister() {
    if (!g_swapTaken) return;
    RegisterRuntimeExport("?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z",
                          reinterpret_cast<void*>(&SwapAllocate));
    RegisterRuntimeExport("?Free@MemoryAllocator@System@@QEAAXPEAX@Z",
                          reinterpret_cast<void*>(&SwapFree));
    fprintf(stderr, "[RtAllocSwap] ?Allocate@/?Free@ registrations repointed at the "
                    "composed (locked) stubs\n");
    fflush(stderr);
}

// ============================================================================
// The observe-only shadow driver
// ============================================================================
extern "C" bool RtAllocShadowInstall() {
    if (!EnvOn("BANISHED_RT_DIFF") || !EnvOn("BANISHED_RT_ALLOC_SHADOW")) return false;

    // The shadow rides inside allocator_lock's wrappers; without them it would
    // observe nothing and report a clean calls=0, which is exactly the reading
    // this wave must never produce.
    {
        const char* e = getenv("BANISHED_ALLOC_LOCK");
        if (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F')) {
            fprintf(stderr, "[RtAllocShadow] REFUSING to arm: BANISHED_ALLOC_LOCK=0 removes "
                            "the wrappers the observer rides in. The shadow needs the lock "
                            "installed (default on).\n");
            fflush(stderr);
            return false;
        }
    }
    if (EnvOn("BANISHED_RT_ALLOC")) {
        fprintf(stderr, "[RtAllocShadow] REFUSING to arm: BANISHED_RT_ALLOC=1 takes the heap. "
                        "The shadow is the measurement that must come FIRST.\n");
        fflush(stderr);
        return false;
    }

    // Engine::Startup, NOT MemoryAllocator::Startup — the latter has no callers
    // anywhere in the process. See the header comment for the proof.
    if (!InstallRuntimeDetour("Engine::Startup[allocshadow]", 0x1800db8a0,
                              reinterpret_cast<void*>(&ShadowStartup),
                              &g_startupDetour, 19)) {
        fprintf(stderr, "[RtAllocShadow] could not detour Engine::Startup "
                        "(0x1800db8a0) — not arming\n");
        fflush(stderr);
        return false;
    }

    g_allocShadowOnAlloc = &ShadowOnAlloc;
    g_allocShadowOnFree  = &ShadowOnFree;
    g_allocShadowPreOp   = &ShadowPreOp;
    g_allocShadowPostOp  = &ShadowPostOp;
    g_shadowOn = true;
    fprintf(stderr, "[RtAllocShadow] observe-only driver armed; waiting for "
                    "Engine::Startup (which INLINES MemoryAllocator::Startup)\n");
    fflush(stderr);
    return true;
}

// ============================================================================
// Summary — read this, not just the [RtDiff] divergences= column
// ============================================================================
// EFFECTIVENESS CONTROL (reference_probe_needs_an_effectiveness_control.md):
// `observed` is how many calls actually reached the comparison. `skipped` is
// how many arrived before Startup was seen — a non-zero value means the arena
// was already live when we armed and the replica can never agree, so the run is
// INVALID rather than clean. `other` counts traffic on a MemoryAllocator that
// is not the one we replicate.
extern "C" void RtAllocShadowSummary() {
    if (!g_shadowOn && g_shadowSkipped == 0) return;
    uint64_t calls = 0, divergences = 0, mapFull = 0, freeUnmapped = 0;
    RtAllocShadowReport(&calls, &divergences);
    RtAllocShadowHealth(&mapFull, &freeUnmapped);
    // COVERAGE CONTROL: did the arena-growth path (AddNewMemoryRange ->
    // ExpandMemoryRangeTable) run at all, and did it agree? divergences=0 over
    // a path never taken is the Wave 1 defect #4 trap one level down.
    int ourRanges = 0; uint64_t ourReserved = 0;
    RtAllocShadowCoverage(&ourRanges, &ourReserved);
    if (g_shadowSelf)
        fprintf(stderr, "[RtAllocShadow] COVERAGE ranges ours=%d theirs=%d  "
                        "reserved ours=%lluMB theirs=%lluMB\n",
                ourRanges, g_shadowSelf->rangeCount,
                (unsigned long long)(ourReserved >> 20),
                (unsigned long long)(g_shadowSelf->totalReserved >> 20));
    // EFFECTIVENESS CONTROL for a growth cell (lane E3). `ranges=N` above says
    // where the replica ENDED; these say what the growth path actually DID.
    // A cell run to close WAVE9_LANE_E2.md §5.1 is only evidence if grow>0 and
    // tableCopy>0 — the latter needs a THIRD range, because rangeCapacity goes
    // 0 -> 2 -> 4 and only the 2->4 step takes the memcpy+release branch.
    {
        uint64_t grow = 0, tableCopy = 0, crossRange = 0;
        RtAllocShadowGrowth(&grow, &tableCopy, &crossRange);
        fprintf(stderr, "[RtAllocShadow] GROWTH grow=%llu tableCopy=%llu crossRange=%llu\n",
                (unsigned long long)grow, (unsigned long long)tableCopy,
                (unsigned long long)crossRange);
        if (grow == 0)
            fprintf(stderr, "[RtAllocShadow] grow=0 — the arena-growth path did NOT run in "
                            "this cell. Whatever `divergences` says, this run is not "
                            "evidence about multi-range behaviour (WAVE9_LANE_E2.md 5.1).\n");
        else if (tableCopy == 0)
            fprintf(stderr, "[RtAllocShadow] tableCopy=0 — a second range was created but "
                            "ExpandMemoryRangeTable's copy-and-release branch never ran. "
                            "That needs a THIRD range; shrink BANISHED_ARENA_MB further.\n");
    }
    // Wave 10 lane SHADOWBISECT -- the DIRECT test of SHADOWRE's named hypothesis.
    // gaps>0 means the real arena was mutated between two consecutive wrapper
    // entries, i.e. by a route that skips the patched Allocate/Free prologues.
    // gaps==0 with checks>0 KILLS that hypothesis (checks==0 would mean the
    // detector never ran and the cell says nothing either way).
    {
        uint64_t gchecks = 0, gaps = 0, ctl = 0;
        int64_t dA = 0, dF = 0, dB = 0;
        RtAllocShadowGaps(&gchecks, &gaps, &dA, &dF, &dB, &ctl);
        fprintf(stderr, "[RtAllocShadow] GAPS checks=%llu gaps=%llu "
                        "sum_dAllocCount=%lld sum_dFreeTagCount=%lld sum_dAllocatedBytes=%lld "
                        "ctl_injected=%llu\n",
                (unsigned long long)gchecks, (unsigned long long)gaps,
                (long long)dA, (long long)dF, (long long)dB,
                (unsigned long long)ctl);
        if (gchecks == 0)
            fprintf(stderr, "[RtAllocShadow] GAP DETECTOR NEVER RAN (checks=0) -- this cell is "
                            "silent on the bypassing-free hypothesis.\n");
        else if (gaps == 0)
            fprintf(stderr, "[RtAllocShadow] gaps=0 over %llu checks -- NOTHING mutated the real "
                            "arena outside the two patched entries in this cell.\n",
                    (unsigned long long)gchecks);
    }
    fprintf(stderr,
            "[RtAllocShadow] SUMMARY armed=%d observed=%llu divergences=%llu "
            "skipped_pre_startup=%llu other_allocator=%llu "
            "map_full=%llu free_unmapped=%llu\n",
            g_shadowSelf ? 1 : 0,
            (unsigned long long)calls, (unsigned long long)divergences,
            (unsigned long long)g_shadowSkipped, (unsigned long long)g_shadowOther,
            (unsigned long long)mapFull, (unsigned long long)freeUnmapped);
    if (mapFull || freeUnmapped)
        fprintf(stderr, "[RtAllocShadow] HARNESS DEFECT: map_full=%llu free_unmapped=%llu. "
                        "Every divergence after the first of these is the replay map's "
                        "fault, not the port's. Grow kShadowSlots and re-run.\n",
                (unsigned long long)mapFull, (unsigned long long)freeUnmapped);
    if (g_shadowOn && calls == 0)
        fprintf(stderr, "[RtAllocShadow] observed=0 — this run MEASURED NOTHING. "
                        "Do not read it as a pass.\n");
    if (g_shadowSkipped)
        fprintf(stderr, "[RtAllocShadow] skipped_pre_startup=%llu — the arena was live before "
                        "the driver armed; this run is INVALID.\n",
                (unsigned long long)g_shadowSkipped);
    fflush(stderr);
}
