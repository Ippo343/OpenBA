// ============================================================================
// runtime/src/audio/audio_stream_win.cpp — Wave 24 lane SMALLBODY
//   ??0Stream@Audio@@QEAA@XZ    retail 0x1800a0e30  ord 410  159 bytes  (ctor)
//   ??1Stream@Audio@@QEAA@XZ    retail 0x1800a0d90  ord 688  153 bytes  (dtor)
// ============================================================================
//
// `Audio::Stream` is a `System::Thread` SUBCLASS. These two are the whole of its
// construction and destruction. The run body (`?Run@Stream@Audio@@UEAAXXZ`,
// 0x1800a0f60) used to be "NOT in this lane's scope and stays on the DLL"; it is
// PORTED as of VTABLE BATCH 7b — runtime/src/audio/vtbatch7b_stream_run_win.cpp,
// which is also what publishes this ctor's +0x48 event into the Device's
// VoiceCallback record.
//
// Its own .cpp, and a `_win` one, because both bodies are almost entirely
// KERNEL32 synchronisation primitives and PLAN.md §4.1 decision 1 keeps
// <windows.h> out of audio.cpp — the same split audio_device.cpp already makes
// for the Device+0xA50 critical section.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED FIRST
// ---------------------------------------------------------------------------
// Grepped by ADDRESS and by NAME across runtime/ code/ standalone/ engine/ dx11/
// (scoped — WAVE24_KICKOFF §1a), before anything was written. Hits were
// CONSUMERS only:
//   ??0Stream@Audio@@QEAA@XZ -> runtime/src/engine/singleton_ctors.cpp:742
//   ??1Stream@Audio@@QEAA@XZ -> runtime/src/engine/engine_lifecycle.cpp:955
//   0x1800a0e30 / 0x1800a0d90 -> no hits at all.
// A body ported but never REGISTERED is indistinguishable from an absent one to
// the census (reference_half_registered_port_reads_as_absent.md), which is why
// the grep was by address as well as by name.
//
// ---------------------------------------------------------------------------
// THE STATED BLOCKER, RE-CHECKED — IT IS AN *ORACLE* VERDICT, NOT A PORT ONE
// ---------------------------------------------------------------------------
// `runtime/src/engine/singleton_ctors.cpp:702-710` (Wave 9 lane C2) holds it
// like this, verbatim:
//
//     "*** ORACLE DEFERRED — SAME BLOCKER AS PERSISTENCE. *** 0x1800a0e30 is
//      `??0Stream@Audio@@QEAA@XZ`, a System::Thread SUBCLASS: it calls the same
//      `??0Thread@System@@QEAA@I@Z` base that `_beginthreadex`s a suspended
//      worker and creates three events, then stamps `??_7Stream@Audio@@6B@`
//      (0x180154950) and initialises a second CRITICAL_SECTION. Running the
//      shipped body twice against scratch buffers would start two threads on
//      fake objects and the handle values would differ by construction."
//
// RE-CHECKED, and the re-check changes what it licenses:
//
//   * The technical content is CORRECT and this lane reproduces it from the
//     disassembly independently — the base ctor really is called with 0x10000,
//     the vptr really is 0x180154950, and there really is a second
//     CRITICAL_SECTION at +0x50.
//   * But it is a verdict about an ORACLE ("running the shipped body twice ...
//     would start two threads"), NOT about portability. It never says the body
//     cannot be written. Two waves of gap tables carried this row as though it
//     did. That is the Wave 23 §6 defect class — a discharge does not
//     propagate — in its other direction: a *narrow* holdback read as a *broad*
//     one.
//   * `??0Thread@System@@QEAA@I@Z` IS ALREADY PORTED and registered
//     (runtime/src/system/thread_win.cpp:457), so the base is not a wall
//     either. Re-derived here, not taken from the cross-lane note that also
//     said so.
//
// So: the bodies are ported. The ORACLE holdback SURVIVES re-checking and is
// restated honestly under ORACLE below, rather than quietly dropped.
//
// ---------------------------------------------------------------------------
// ??0Stream@Audio@@QEAA@XZ — the body, from the instruction listing
// ---------------------------------------------------------------------------
//   MOV [RSP+8],RCX / PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2   unwind
//   MOV RDI,RCX
//   MOV EDX,0x10000 ; CALL 0x1800ca4e0      ??0Thread@System@@QEAA@I@Z, 64 KB
//   LEA RAX,[0x180154950] ; MOV [RDI],RAX   stamp ??_7Stream@Audio@@6B@
//   XOR ESI,ESI
//   MOV [RDI+0x30],RSI ; MOV [RDI+0x38],RSI
//   LEA RCX,[RDI+0x50] ; CALL [0x18014b380]   InitializeCriticalSection
//   MOV EDX,0x1388 ; LEA RCX,[RDI+0x50]
//                    CALL [0x18014b208]       SetCriticalSectionSpinCount 5000
//   MOV [RDI+0x78],RSI
//   MOV BYTE PTR [RDI+0x80],SIL
//   XOR R9D/R8D/EDX/ECX ; CALL [0x18014b360] ; MOV [RDI+0x48],RAX  CreateEventW
//   XOR R9D/R8D/EDX/ECX ; CALL [0x18014b360] ; MOV [RDI+0x40],RAX  CreateEventW
//   MOV RAX,RDI / RET
//
// Every one of those five KERNEL32 slots was named from the DLL's OWN import
// table, not guessed: 0x18014b380 InitializeCriticalSection, 0x18014b208
// SetCriticalSectionSpinCount, 0x18014b360 CreateEventW, 0x18014b368 SetEvent,
// 0x18014b3b8 CloseHandle, 0x18014b3c0 WaitForSingleObject, 0x18014b370
// DeleteCriticalSection.
//
// BOTH events are `CreateEventW(NULL, FALSE, FALSE, NULL)` — all four arguments
// zeroed — i.e. AUTO-RESET and initially NON-SIGNALLED. Note the order: +0x48 is
// created FIRST, +0x40 second. Reproduced in that order because a faithful port
// reproduces side effects, and two successive CreateEventW calls are
// distinguishable in a handle trace.
//
// TWO events, not three. The "creates three events" in the Wave 9 comment
// belongs to the `System::Thread` BASE (hStart/hStop/hFinished,
// runtime/types/system/Thread.h), which is a separate object region; the
// sentence is accurate about the base and is not describing this body.
//
// ---------------------------------------------------------------------------
// ??1Stream@Audio@@QEAA@XZ — and the "every element or only one" check
// ---------------------------------------------------------------------------
// The lane brief warns that a prescribed dtor port once swapped a body that
// destroys EVERY element for one that destroys only the first. So this body was
// READ, and every teardown step enumerated from the listing:
//
//   MOV [RCX],&0x180154950                 re-stamp ??_7Stream@Audio@@6B@
//   CloseHandle([RCX+0x48])                the second event
//   CloseHandle([RBX+0x40])                the first event
//   LEA RCX,[RBX+0x50] ; DeleteCriticalSection
//   LEA RCX,[RBX+0x30] ; CALL 0x18000c4a0  release the +0x30 heap buffer
//   MOV [RBX],&0x180155f20                 re-stamp ??_7Thread@System@@6B@  |
//   SetEvent([RBX+0x20])                   hStop                            |
//   WaitForSingleObject([RBX+0x10], -1)    hThread, INFINITE                | the
//   CloseHandle([RBX+0x10])                hThread                          | INLINED
//   CloseHandle([RBX+0x18])                hStart                           | ~Thread
//   CloseHandle([RBX+0x20])                hStop                            |
//   JMP CloseHandle([RBX+0x28])            hFinished  (TAIL call)           |
//
// SIX CloseHandle calls in total: two Stream events and ALL FOUR of the base's
// handles. Nothing is closed twice and nothing is left open — checked against
// `runtime/types/system/Thread.h`, which names exactly four handles at
// +0x10/+0x18/+0x20/+0x28.
//
// THE INLINED TAIL IS `??1Thread@System@@QEAA@XZ`, PROVED NOT ASSUMED. The
// shipped `System::Thread` dtor at 0x1800ca590 was disassembled beside this one:
// it is the SAME twelve steps in the SAME order, from the 0x180155f20 stamp to
// the tail CloseHandle of +0x28. That body is already ported and registered
// (thread_win.cpp:458), so this port CALLS it rather than re-inlining a
// thirteenth hand copy — the same reasoning tooltipdialog.cpp gives for sending
// vtable slot 0 to deleting_dtor.cpp instead of copying it.
//
// 0x18000c4a0 is a heap-buffer release: `if (this[0]) { lock;
// Free(*0x1801fc718, this[0]); unlock; } this[0] = 0; this[1] = 0;` — i.e. the
// lock/Free/unlock triple that IS `?MemFree@MemoryAllocator@System@@`, plus a
// two-qword clear. It is reached BY ADDRESS through `ResolveAddr`, which
// consults the port registry first.
//
// *** WAVE 25 LANE INTERNAL42 CORRECTION, left in place rather than deleted so
// the next reader sees which half was wrong. *** This banner used to end: "NOT
// re-implemented here: it is 0x18 bytes of somebody else's object and this lane
// has no witness for what lives at +0x30." The observation was right and the
// inference was wrong, which is the DEPENDENCY-SHAPED blocker class Wave 24
// measured false 5 times in 9. 0x18000c4a0 is byte-for-byte
// `Collection::Array<T>::~Array` — the J1 body already ported seventeen times
// in runtime/src/collection/array.cpp — proved by an address-normalised diff
// against the canonical 0x180004890 with the J2 body as a negative control.
// Freeing a buffer does not need a witness for what is in it; that is exactly
// why J1 takes no stride and no parameters at all.
//
// It is now REGISTERED (array.cpp, the eighteenth/nineteenth J1 addresses), so
// the `ResolveAddr` below reaches OUR body. Nothing in this file changed: the
// call site was already a door, and a door is what made the port reachable
// without touching its caller.
//
// ---------------------------------------------------------------------------
// ??_7Stream@Audio@@6B@ (0x180154950) IS NOT MINTED, AND THAT IS DELIBERATE
// ---------------------------------------------------------------------------
// RE-DERIVED HERE, not accepted from a cross-lane note:
//     grep -n '180154950\|Stream@Audio@@6B' standalone/rt_data_mint_tables.inc
//                                           standalone/reg_mint_promoted.cpp
//     -> ZERO hits.
// So at cutover this vptr has no minted storage to point at.
//
// THIS PORT DOES NOT MAKE THAT WORSE — IT IS THE STEP THAT MAKES IT FIXABLE.
// Retail's ctor stamps the vptr with a RIP-relative LEA, and a RIP-relative LEA
// stamp IS NOT A DOOR: we intercept only at doors, so while the DLL owns this
// ctor nothing we do to the mint table can ever change which table gets stamped
// (reference_a_ported_row_can_be_permanently_unpromotable.md). Porting the ctor
// CONVERTS that LEA into a `ResolveAddr(Module::Runtime, 0x180154950)` door,
// which resolves to the minted row the moment one exists and to the DLL's table
// until then.
//
// MINTING IT IS NOT IN THIS LANE'S SCOPE: `standalone/rt_data_mint_tables.inc`
// is GENERATED by tools/rt_mint_gen.py, and regenerating it is a whole-file
// change that would capture six other live Wave 24 lanes mid-wave — the reason
// runtime/src/ui/tooltipdialog.cpp gives for leaving its own row to the wave
// owner. Handed forward explicitly instead:
//
//   HANDOVER: mint `??_7Stream@Audio@@6B@` (0x180154950).
//   [SCOPE: CUTOVER-ONLY] — with the DLL present this row is harmless: the door
//   resolves to the DLL's own table, which is what retail stamps anyway.
//   [ADDRESSEE: Wave 24 lane CUTOVER, whose attempt-25 boot faults at this very
//   ctor, or the wave owner at the close regeneration — whichever comes first.]
//
// ---------------------------------------------------------------------------
// ORACLE — WHAT IS AND IS NOT AVAILABLE, STATED PLAINLY
// ---------------------------------------------------------------------------
// PROBE SHAPE (WAVE24_KICKOFF §1c): **shadow/diff site**, and honestly labelled
// as a SELF-CHECK rather than an oracle. The other four shapes were considered
// and each is ruled out by a named fact, not by "no run, therefore STRUCTURAL":
//
//   * INPUT SWEEP — needs a PURE function. Neither body is: one creates two OS
//     event objects and a critical section, the other closes six handles and
//     joins a thread.
//   * SYNTHESISED ORACLE — a fabricated `Audio::Stream` is not a synthetic
//     subject but a FAKE: the base ctor `_beginthreadex`s a real worker, so
//     running either body against scratch memory starts (or joins) a real OS
//     thread. This is the Wave 9 holdback quoted above, re-checked and UPHELD.
//   * WRITE-PATH REPLAY — neither body is a Serialize.
//   * TWO-SIDED A/B — a destructor cannot be replayed: running theirs after ours
//     double-closes four handles, and the handle VALUES differ by construction
//     anyway, so they are not a comparable quantity.
//
// WHAT THE SELF-CHECK DOES ASSERT — the post-conditions the disassembly pins,
// which are invariants a WRONG port breaks even though no second party is
// consulted. It is exactly the class of site
// reference_shadow_marker_is_not_an_oracle.md warns about, and it is labelled
// SELF-CHECK in its own site name so a sweep report cannot count it as an
// oracle.
//
// EFFECTIVENESS COUNTERS: `g_ctorChecks` / `g_dtorChecks` count how many times
// the predicate was actually EVALUATED (not merely how many times the body ran),
// so a "0 divergences" line cannot be confused with a dead screen
// (KICKOFF §3 rule 3).
//
// NEGATIVE CONTROL: `BANISHED_RT_AUDIOSTREAM_CONTROL=1` skips the
// `[+0x38] = 0` store in the ctor. +0x38 IS in the checked set, so the control
// corrupts a subject the predicate actually examines
// (reference_control_must_pick_a_compared_subject.md); and it leaves the two
// handle fields alone, which MUST NOT be compared against any fixed value
// because they legitimately vary per run (the mirror failure,
// reference_a_control_can_corrupt_a_byte_that_should_not_be_compared.md).
//
// ---------------------------------------------------------------------------
// ONE KILL SWITCH: BANISHED_RT_AUDIOSTREAM
// ---------------------------------------------------------------------------
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Declared locally rather than by including standalone/resolve.h (PLAN §4.1
// decision 1), exactly as ~30 sibling runtime files do.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

// ---- field offsets, every one pinned by an instruction in the banner --------
constexpr int kStreamField30      = 0x30;   // the heap buffer 0x18000c4a0 releases
constexpr int kStreamField38      = 0x38;
constexpr int kStreamLock         = 0x50;   // the SECOND CRITICAL_SECTION
constexpr int kStreamField78      = 0x78;
constexpr int kStreamFlag80       = 0x80;   // a BYTE
constexpr int kStreamEventSecond  = 0x40;   // created SECOND, stored at +0x40
constexpr int kStreamEventFirst   = 0x48;   // created FIRST,  stored at +0x48
constexpr uint32_t kStreamStackSize = 0x10000;   // MOV EDX,0x10000 — 64 KB
constexpr DWORD    kStreamSpinCount = 0x1388;    // MOV EDX,0x1388  — 5000

// ---- attendance ------------------------------------------------------------
uint64_t g_ctorEntries = 0;
uint64_t g_dtorEntries = 0;
uint64_t g_ctorChecks  = 0;   // EFFECTIVENESS: predicate actually evaluated
uint64_t g_dtorChecks  = 0;   // EFFECTIVENESS: predicate actually evaluated
uint64_t g_ctorEventFail = 0; // control: CreateEventW refused

struct AudioStreamSummary {
    ~AudioStreamSummary() {
        if (g_ctorEntries == 0 && g_dtorEntries == 0) {
            printf("[RtPort] Audio::Stream ATTENDANCE: neither ctor nor dtor "
                   "was entered in this run.\n");
            fflush(stdout);
            return;
        }
        printf("[RtPort] Audio::Stream ATTENDANCE (entries, not comparisons): "
               "ctor=%llu dtor=%llu eventFail=%llu | SELF-CHECKS EVALUATED "
               "ctor=%llu dtor=%llu (these are SELF-CHECKS, not oracles -- they "
               "never consult the DLL)\n",
               (unsigned long long)g_ctorEntries,
               (unsigned long long)g_dtorEntries,
               (unsigned long long)g_ctorEventFail,
               (unsigned long long)g_ctorChecks,
               (unsigned long long)g_dtorChecks);
        fflush(stdout);
    }
};
AudioStreamSummary g_summary;

template <typename T> inline T& Field(void* p, int off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

// `??_7Stream@Audio@@6B@` @ 0x180154950. Reached through ResolveAddr so the
// stamp follows the mint table once that row exists — see the banner. Retail
// reaches it with a RIP-relative LEA, which is NOT a door; this IS one.
const void* StreamVtable() {
    static const void* v = nullptr;
    if (!v) v = reinterpret_cast<const void*>(ResolveAddr(Module::Runtime, 0x180154950));
    return v;
}

// `??_7Thread@System@@6B@` @ 0x180155f20 — the BASE vtable `??0Thread@System@@`
// stamps and `??1Thread@System@@` re-stamps. Read ONLY by the ctor self-check,
// to prove our stamp landed AFTER the base ctor's. Never written by this file.
const void* ThreadBaseVtable() {
    static const void* v = nullptr;
    if (!v) v = reinterpret_cast<const void*>(ResolveAddr(Module::Runtime, 0x180155f20));
    return v;
}

// `??0Thread@System@@QEAA@I@Z` @ 0x1800ca4e0 — PORTED and registered
// (runtime/src/system/thread_win.cpp:457). Reached BY NAME, which is what makes
// this call site follow the port.
void ThreadCtor(void* self, uint32_t stackSize) {
    using Fn = void* (*)(void*, uint32_t);
    Fn fn = RT_EXPORT_ONCE(Fn, "??0Thread@System@@QEAA@I@Z");
    if (fn) fn(self, stackSize);
}

// `??1Thread@System@@QEAA@XZ` @ 0x1800ca590 — PORTED and registered
// (thread_win.cpp:458). Retail INLINES it into the Stream dtor; the two were
// disassembled side by side and are the same twelve steps in the same order, so
// calling it reproduces the inlined tail exactly.
void ThreadDtor(void* self) {
    using Fn = void (*)(void*);
    Fn fn = RT_EXPORT_ONCE(Fn, "??1Thread@System@@QEAA@XZ");
    if (fn) fn(self);
}

// 0x18000c4a0 — the +0x30 `Collection::Array<T>::~Array`. PORTED (Wave 25 lane
// INTERNAL42, array.cpp's J1 family); reached by address so the registry gets
// first refusal and now answers with our body. See the banner.
void ReleaseField30Buffer(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18000c4a0));
    if (fn) fn(p);
}

bool ControlArmed() {
    static const int v = []() {
        const char* e = std::getenv("BANISHED_RT_AUDIOSTREAM_CONTROL");
        return (e && e[0] && e[0] != '0') ? 1 : 0;
    }();
    return v != 0;
}

}  // namespace

// ============================================================================
// ??0Stream@Audio@@QEAA@XZ                                 retail 0x1800a0e30
// ============================================================================
extern "C" void* RtAudio_Stream_Ctor(void* self) {
    RT_DIFF_BEGIN("Audio::Stream::Stream [SELF-CHECK, not an oracle]");
    ++g_ctorEntries;

    ThreadCtor(self, kStreamStackSize);                  // 0x1800a0e55
    Field<const void*>(self, 0x00) = StreamVtable();     // 0x1800a0e62
    Field<uint64_t>(self, kStreamField30) = 0;           // 0x1800a0e67
    if (!ControlArmed())                                 // 0x1800a0e6b
        Field<uint64_t>(self, kStreamField38) = 0;

    InitializeCriticalSection(                           // 0x1800a0e73
        reinterpret_cast<CRITICAL_SECTION*>(static_cast<uint8_t*>(self) + kStreamLock));
    SetCriticalSectionSpinCount(                         // 0x1800a0e82
        reinterpret_cast<CRITICAL_SECTION*>(static_cast<uint8_t*>(self) + kStreamLock),
        kStreamSpinCount);

    Field<uint64_t>(self, kStreamField78) = 0;           // 0x1800a0e89
    Field<uint8_t>(self, kStreamFlag80)   = 0;           // 0x1800a0e8d, a BYTE store

    // Both auto-reset, both initially non-signalled. +0x48 FIRST, then +0x40 —
    // the order retail creates them in.
    Field<HANDLE>(self, kStreamEventFirst)  =            // 0x1800a0e9e / 0x1800a0ea4
        CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Field<HANDLE>(self, kStreamEventSecond) =            // 0x1800a0eb2 / 0x1800a0eb8
        CreateEventW(nullptr, FALSE, FALSE, nullptr);

    if (Field<HANDLE>(self, kStreamEventFirst) == nullptr ||
        Field<HANDLE>(self, kStreamEventSecond) == nullptr)
        ++g_ctorEventFail;

    // ---- SELF-CHECK: the post-conditions the disassembly pins ---------------
    // The two HANDLE fields are checked only for NON-NULLNESS and DISTINCTNESS.
    // Their VALUES must not be compared against anything: they legitimately vary
    // per run, and a predicate that examined them would be the mirror control
    // failure (corrupting/comparing a byte that should not be compared).
    // THE VPTR TERM IS AN *ORDERING* CHECK, NOT AN IDENTITY ONE, AND THE
    // DIFFERENCE MATTERS. Wave 24 lane DIFF's armed sweep found
    // `IO::MemoryStream::MemoryStream`'s oracle permanently red because it
    // compares a vptr at +0x00 that MUST differ between our table and retail's
    // (reference_a_control_can_corrupt_a_byte_that_should_not_be_compared.md).
    // This site does NOT make that mistake: it never compares our vptr against
    // retail's. It compares the field against the value THIS BODY WROTE, which
    // can only disagree if the stamp landed in the wrong ORDER — and that is a
    // real and specific defect mode here, because `??0Thread@System@@` stamps
    // `??_7Thread@System@@6B@` (0x180155f20) and our stamp must come AFTER it.
    // The explicit `!= ThreadBaseVtable()` term is what makes that failure
    // detectable rather than merely improbable; without it the comparison is
    // close to a tautology.
    if (rt_diff_on__) {
        ++g_ctorChecks;
        const HANDLE h1 = Field<HANDLE>(self, kStreamEventFirst);
        const HANDLE h2 = Field<HANDLE>(self, kStreamEventSecond);
        const bool ok =
            Field<const void*>(self, 0x00) == StreamVtable() &&
            Field<const void*>(self, 0x00) != ThreadBaseVtable() &&
            Field<uint64_t>(self, kStreamField30) == 0 &&
            Field<uint64_t>(self, kStreamField38) == 0 &&
            Field<uint64_t>(self, kStreamField78) == 0 &&
            Field<uint8_t>(self, kStreamFlag80)   == 0 &&
            h1 != nullptr && h2 != nullptr && h1 != h2;
        // The message must name the FAILING term, not restate the verdict: a
        // shadow predicate's verdict and message are computed separately, and an
        // N-term predicate whose message cannot say WHICH term failed is
        // unusable (reference_shadow_predicate_must_name_its_failing_term.md).
        const char* which =
            (Field<const void*>(self, 0x00) != StreamVtable()) ? "vptr != ??_7Stream@Audio@@6B@"
            : (Field<const void*>(self, 0x00) == ThreadBaseVtable())
                  ? "vptr is STILL ??_7Thread@System@@6B@ -- the Stream stamp did "
                    "not land after the base ctor"
            : (Field<uint64_t>(self, kStreamField30) != 0)     ? "+0x30 != 0"
            : (Field<uint64_t>(self, kStreamField38) != 0)     ? "+0x38 != 0"
            : (Field<uint64_t>(self, kStreamField78) != 0)     ? "+0x78 != 0"
            : (Field<uint8_t>(self, kStreamFlag80)   != 0)     ? "+0x80 != 0"
            : (h1 == nullptr)                                  ? "+0x48 event is NULL"
            : (h2 == nullptr)                                  ? "+0x40 event is NULL"
            : (h1 == h2)                                       ? "+0x40 and +0x48 are the SAME handle"
                                                               : "all terms hold";
        RT_DIFF_SHADOW(ok, which,
                       "no second party consulted: the shipped ctor cannot be "
                       "run against a scratch subject without starting a real "
                       "OS thread");
    }

    return self;                                         // 0x1800a0ebc
}

// ============================================================================
// ??1Stream@Audio@@QEAA@XZ                                 retail 0x1800a0d90
// ============================================================================
extern "C" void RtAudio_Stream_Dtor(void* self) {
    RT_DIFF_BEGIN("Audio::Stream::~Stream [SELF-CHECK, not an oracle]");
    ++g_dtorEntries;

    const HANDLE hFirst  = Field<HANDLE>(self, kStreamEventFirst);
    const HANDLE hSecond = Field<HANDLE>(self, kStreamEventSecond);

    Field<const void*>(self, 0x00) = StreamVtable();      // 0x1800a0dad
    CloseHandle(hFirst);                                  // 0x1800a0db4, +0x48
    CloseHandle(hSecond);                                 // 0x1800a0dbe, +0x40
    DeleteCriticalSection(                                // 0x1800a0dc9
        reinterpret_cast<CRITICAL_SECTION*>(static_cast<uint8_t*>(self) + kStreamLock));
    ReleaseField30Buffer(static_cast<uint8_t*>(self) + kStreamField30);  // 0x1800a0dd4

    // The inlined ~Thread: the 0x180155f20 re-stamp, SetEvent(hStop),
    // WaitForSingleObject(hThread, INFINITE) and all FOUR base CloseHandles.
    // Proved equal to the shipped 0x1800ca590 body, which is already ported.
    ThreadDtor(self);                                     // 0x1800a0dda..0x1800a0e22

    if (rt_diff_on__) {
        ++g_dtorChecks;
        // The only post-condition this body leaves observable: the +0x30 slot
        // that 0x18000c4a0 clears. Everything else it touched is a closed
        // handle, and reading a closed handle is not a check, it is a use.
        const bool ok = Field<uint64_t>(self, kStreamField30) == 0;
        RT_DIFF_SHADOW(ok,
                       ok ? "+0x30 buffer slot cleared"
                          : "+0x30 buffer slot NOT cleared by 0x18000c4a0",
                       "no second party consulted: a destructor cannot be "
                       "replayed -- running theirs after ours double-closes six "
                       "handles");
    }
}

// ============================================================================
// Registration. BOTH doors for both rows — BY NAME and BY ADDRESS.
// ============================================================================
// The dtor's mangling was RE-DERIVED against the shipped export directory, not
// inherited: `??1Stream@Audio@@QEAA@XZ` is `QEAA` — NON-VIRTUAL — and it is
// present at 0x1800a0d90 / ordinal 688. The lane brief warns that the wrong one
// of `QEAA`/`UEAA` resolves NULL silently
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md,
//  reference_resolveexport_phantom_names.md). Checked by reading the export
// table directly: `??1Stream@Audio@@UEAA@XZ` is ABSENT from this DLL, and
// `??1Stream@Audio@@QEAA@XZ` is present — so `QEAA` is the spelling, and the
// sibling row `??1PackageFile@IO@@` genuinely takes the OTHER one.
// ============================================================================
int RtAudioStreamRegister() {
    if (!RtWaveEnabled("BANISHED_RT_AUDIOSTREAM")) {
        printf("[RtPort] BANISHED_RT_AUDIOSTREAM=0 -- Audio::Stream ctor/dtor "
               "left on the DLL\n");
        return 0;
    }
    int n = 0;

    RegisterRuntimeExport("??0Stream@Audio@@QEAA@XZ", (void*)&RtAudio_Stream_Ctor);
    RegisterRuntimeAddr(0x1800a0e30, (void*)&RtAudio_Stream_Ctor);   ++n;

    RegisterRuntimeExport("??1Stream@Audio@@QEAA@XZ", (void*)&RtAudio_Stream_Dtor);
    RegisterRuntimeAddr(0x1800a0d90, (void*)&RtAudio_Stream_Dtor);   ++n;

    return n;
}
