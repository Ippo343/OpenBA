// ============================================================================
// runtime/src/system/thread_win.cpp — System::CriticalSection / System::Thread
// ============================================================================
// Win32 half of the platform seam (PLAN.md §4.1 decision 1): this file, and
// only this file in the pair, may include <windows.h>. The type layouts live
// in runtime/types/system/Thread.h, which stays platform-neutral so a
// `_posix` twin can implement the same offsets later.
//
// SCOPE (Wave 2, lane D): the 8 exports System::CriticalSection and
// System::Thread expose to our own call sites (pathfinder_seed.cpp,
// keystone_a_gamedtor.cpp, action_pf_astar.cpp, keystone_a_zone.cpp):
//   ??0CriticalSection@System@@QEAA@XZ    ??1CriticalSection@System@@QEAA@XZ
//   ?Lock@CriticalSection@System@@QEAAXXZ ?Unlock@CriticalSection@System@@QEAAXXZ
//   ??0Thread@System@@QEAA@I@Z            ??1Thread@System@@QEAA@XZ
//   ?Start@Thread@System@@QEAAXXZ         ?WaitForFinish@Thread@System@@QEAAXXZ
//
// CORRECTNESS ARGUMENT (objects cross the DLL boundary both ways — see the
// lane brief / PLAN.md §4.3 risk 3, vtable ownership):
//
//   CriticalSection has no vtable and wraps a real Win32 CRITICAL_SECTION with
//   the exact same spin count (5000) retail uses. A CS our ctor initializes
//   and the DLL's Lock()/Unlock() later touches (or vice versa) is the same
//   object either way — EnterCriticalSection/LeaveCriticalSection only care
//   about the bytes, not who wrote them.
//
//   Thread DOES have a vtable, but our ctor/dtor do not invent one: they read
//   the real ??_7Thread@System@@6B@ vtable OUT OF THE LOADED DLL (it is a
//   genuine PE export, confirmed via pefile — not just a Ghidra label) and
//   stamp that same address, exactly like retail's own ctor/dtor do. A Thread
//   object is never used un-subclassed in practice (slot 0 is _purecall), and
//   every real subclass — ours included (pathfinder_seed.cpp,
//   keystone_a_gamedtor.cpp) — re-stamps its OWN vtable over this one right
//   after construction, exactly as retail's subclasses do. So whichever side
//   constructs the base and whichever side subclasses it, the vtable pointer
//   in play at any given moment is always either the real base vtable or a
//   real derived vtable — never a fabricated one. Vtable ownership is a
//   non-issue here specifically because we point at the DLL's own object
//   instead of cloning it.
//
//   The worker thread's entry point WAS intentionally left as the DLL's own
//   ?ThreadFunc@Thread@System@@CAIPEAX@Z, on the grounds that it was not one of
//   the 8 exports in scope. **Wave 9 lane P ports it** (bottom of this file),
//   and the ctor below now asks for it BY NAME through the port-aware resolver,
//   so it gets ours when System is enabled and the DLL's under
//   BANISHED_RT_THREAD=0. The old reasoning stays true of the LAYOUT — it only
//   touches +0x00 vtable, +0x08 runFlags, +0x10/0x18/0x20/0x28 handles, all of
//   which this header reproduces exactly — which is what made it safe to port.
//
//   CRT OWNERSHIP: the ctor calls the DLL's OWN _beginthreadex (0x18007fbfc),
//   resolved by raw address via ResolveRuntimeRaw, not MinGW/UCRT's. ThreadFunc
//   is DLL code compiled against the DLL's statically-linked CRT; if OUR CRT's
//   _beginthreadex had set up the per-thread CRT state instead, ThreadFunc
//   would run under one CRT's bookkeeping and unwind out through the other's
//   _endthreadex — a CRT boundary crossing of exactly the kind PLAN §4.3 risk 5
//   warns about. Calling the DLL's own _beginthreadex keeps the whole
//   thread's CRT lifetime on the DLL side, same as retail.
//
// DIFF HARNESS: none of these 8 are diffable with RT_DIFF_SCALAR/RT_DIFF_SHADOW
// — they mutate OS state (create a CRITICAL_SECTION / OS thread / kernel
// events, or block on them). Running "ours" and "theirs" back to back would
// double-initialize or double-start a real OS object, not compare pure
// outputs. There is also no side-channel replica to shadow-check against
// (unlike the allocator, there's no independent expected value to compute).
// So these are left undiffed by design; correctness rests on the byte-for-
// byte transcription above instead of a runtime oracle.
// ============================================================================

#include "../../types/system/Thread.h"
#include "../../funcs/rt_port.h"

#include <cstdio>

#include <windows.h>
#include <process.h>   // _beginthreadex, the CRT routine 0x18007fbfc is a copy of

using System::CriticalSection;
using System::Thread;

// Declared in standalone/resolve.h (Module::Runtime ASLR-delta resolver for a
// raw, non-exported address). Not re-declared in rt_port.h — that header's
// seam is exports-only — so this file adds the one declaration it needs
// rather than including resolve.h (which pulls in <windows.h> in a way that
// collides with Math's `near`/`far`/`Rectangle` names; irrelevant here since
// this file already includes <windows.h> for its own reasons, but the
// declaration is kept local rather than assuming resolve.h stays includable).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

// OPT-IN, unlike RtWaveEnabled(): absent env means OFF. RtWaveEnabled is a kill
// switch and defaults to true when unset, which is the wrong polarity for a
// diagnostic. Read once into a function-local static — cheap, and getenv is
// process-cached anyway (standalone/env_cache.h).
static bool RtWaveTraceOn(const char* envName) {
    const char* e = std::getenv(envName);
    return e && *e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// ---- System::CriticalSection -----------------------------------------------

// 0x18000c5c0  ??0CriticalSection@System@@QEAA@XZ
CriticalSection* RtSystem_CriticalSection_Ctor(CriticalSection* self) {
    InitializeCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
    SetCriticalSectionSpinCount(reinterpret_cast<LPCRITICAL_SECTION>(self), 5000);
    return self;
}

// 0x18000c5b0  ??1CriticalSection@System@@QEAA@XZ
void RtSystem_CriticalSection_Dtor(CriticalSection* self) {
    DeleteCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
}

// 0x18000c700  ?Lock@CriticalSection@System@@QEAAXXZ
void RtSystem_CriticalSection_Lock(CriticalSection* self) {
    EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
}

// 0x18000c6f0  ?Unlock@CriticalSection@System@@QEAAXXZ
void RtSystem_CriticalSection_Unlock(CriticalSection* self) {
    LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
}

// ---- System::Thread ---------------------------------------------------------

// The DLL's own base vtable (??_7Thread@System@@6B@, a real PE export at
// 0x180155f20 — slot 0 _purecall, slot 1 OnCompleted), used AS-IS and never
// reimplemented, for the reasons in the file banner.
static void* RtSystemThreadVftable() {
    static void* v = ResolveExport("??_7Thread@System@@6B@");
    return v;
}

// The worker entry point. Asked for BY NAME through the PORT-AWARE resolver, so
// this hands back our own RtSystem_Thread_ThreadFunc (below) once
// RtSystemThreadRegister has run, and the shipped 0x1800ca440 under
// BANISHED_RT_THREAD=0. That is deliberate and is the only A/B this function
// gets: it cannot be diffed (see the DIFF HARNESS note in the banner), so
// "boot with the port, boot without it, compare behaviour" is the oracle.
//
// It was ResolveExportRaw until Wave 9 lane P. Leaving it raw after porting
// would have been the port-bypass defect in its purest form — the port
// registered, the census counting it, and the DLL's body still the one every
// thread in the process actually runs.
static unsigned(__stdcall* RtSystemThreadFunc())(void*) {
    using Fn = unsigned(__stdcall*)(void*);
    static Fn f = reinterpret_cast<Fn>(ResolveExport("?ThreadFunc@Thread@System@@CAIPEAX@Z"));
    return f;
}

// The DLL's OWN _beginthreadex (its statically-linked CRT's, not MinGW/UCRT's
// — see the CRT OWNERSHIP note in the file banner), resolved by raw address:
// 0x18007fbfc, confirmed as the ctor's CALL target in the disassembly.
using BeginThreadExFn = uintptr_t(__cdecl*)(void*, unsigned, unsigned(__stdcall*)(void*), void*, unsigned, unsigned*);
static BeginThreadExFn RtDllBeginThreadEx() {
    // 0x18007fbfc is the DLL's OWN copy of the CRT's _beginthreadex -- a
    // statically-linked MSVC CRT routine, not engine code.  Resolving it by
    // address made this a REAL DEPENDENCY on the raw door (lane V's row
    // "CRT: addr:0x18007fbfc"), and the THIRD controlled DLL-absent boot died
    // exactly here: ResolveRuntimeRaw returns null with the module gone and the
    // call is an EXEC of 0, inside ConstructLoader -> Thread ctor, on the boot
    // path.
    //
    // Our own CRT provides the identical routine, so prefer it and keep the
    // DLL's only as a fallback while the DLL is present.  This is NOT a
    // behavioural change: both are _beginthreadex with the same contract, and
    // retail's call site passes CREATE_SUSPENDED for the reason documented
    // below.  It is the difference between a boot and a null call at cutover.
    static BeginThreadExFn f = []() -> BeginThreadExFn {
        BeginThreadExFn dll =
            reinterpret_cast<BeginThreadExFn>(ResolveRuntimeRaw(0x18007fbfc));
        if (dll) return dll;
        return reinterpret_cast<BeginThreadExFn>(&_beginthreadex);
    }();
    return f;
}

// 0x1800ca4e0  ??0Thread@System@@QEAA@I@Z
Thread* RtSystem_Thread_Ctor(Thread* self, unsigned int stackSize) {
    self->runFlags = 0;
    self->vtable = RtSystemThreadVftable();

    // Retail: XOR ECX,ECX (security=NULL); [RSP+0x20]=4 (CREATE_SUSPENDED) is
    // the LAST arg written before the call, so RDX (stackSize) passes through
    // unmodified. CREATE_SUSPENDED matters: ThreadFunc's first act is to wait
    // on {hStart, hStop}, and those two handles (plus hFinished) are not
    // created until AFTER this call returns. Starting running immediately
    // would have ThreadFunc wait on uninitialized handles.
    unsigned int threadId = 0;
    uintptr_t h = RtDllBeginThreadEx()(nullptr, stackSize, RtSystemThreadFunc(), self, CREATE_SUSPENDED, &threadId);
    self->hThread = reinterpret_cast<void*>(h);
    SetThreadPriority(reinterpret_cast<HANDLE>(self->hThread), THREAD_PRIORITY_NORMAL);

    self->hStart    = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    self->hStop     = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    self->hFinished = CreateEventW(nullptr, TRUE,  TRUE,  nullptr);

    ResumeThread(reinterpret_cast<HANDLE>(self->hThread));
    return self;
}

// 0x1800ca590  ??1Thread@System@@QEAA@XZ
void RtSystem_Thread_Dtor(Thread* self) {
    self->vtable = RtSystemThreadVftable();
    SetEvent(reinterpret_cast<HANDLE>(self->hStop));
    WaitForSingleObject(reinterpret_cast<HANDLE>(self->hThread), INFINITE);
    CloseHandle(reinterpret_cast<HANDLE>(self->hThread));
    CloseHandle(reinterpret_cast<HANDLE>(self->hStart));
    CloseHandle(reinterpret_cast<HANDLE>(self->hStop));
    CloseHandle(reinterpret_cast<HANDLE>(self->hFinished));
}

// 0x1800ca610  ?Start@Thread@System@@QEAAXXZ
void RtSystem_Thread_Start(Thread* self) {
    ResetEvent(reinterpret_cast<HANDLE>(self->hFinished));
    SetEvent(reinterpret_cast<HANDLE>(self->hStart));
}

// 0x1800ca5f0  ?WaitForFinish@Thread@System@@QEAAXXZ
void RtSystem_Thread_WaitForFinish(Thread* self) {
    WaitForSingleObject(reinterpret_cast<HANDLE>(self->hFinished), INFINITE);
}

// ============================================================================
// unsigned int System::Thread::ThreadFunc(void*)      retail 0x1800ca440
// ?ThreadFunc@Thread@System@@CAIPEAX@Z
//
// Wave 9, lane P (tier 0). The worker body every System::Thread runs — the last
// unported member of the class, so System::Thread is now COMPLETE (§4.1
// decision 5).
//
// NO NEW LANE-D ROWS. Its only callees are Win32: WaitForMultipleObjects
// through the DLL's IAT slot 0x18014b358 and SetEvent through 0x18014b368. We
// call kernel32 directly, which is the same function — the IAT slot is where
// the DLL's loader wrote kernel32's address, not a Runtime internal.
//
// STRUCTURE, from the bytes:
//
//   handles[2] = { self->hStart, self->hStop };   // filled ONCE, before the
//                                                 // loop, and REUSED by the
//                                                 // second wait (RSP+0x20 is
//                                                 // never rewritten)
//   r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
//   while (r == 0) {                              // TEST EAX,EAX / JNZ
//       self->runFlags |= 1;                      // OR [RBX+8],1
//       self->vtable[0](self);                    // the work call
//       self->vtable[1](self);                    // OnCompleted — NOTE the
//                                                 // vtable is RELOADED at
//                                                 // 0x1800ca48c, so a work
//                                                 // call that re-stamps the
//                                                 // vtable is honoured
//       h = self->hFinished;                      // loaded BEFORE the AND
//       self->runFlags &= ~1;
//       SetEvent(h);
//       r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
//   }
//   return (r == 1) ? 0 : 0xFFFFFFFF;             // CMP EAX,1 / CMOVZ
//
// The return value distinguishes an orderly stop (index 1 == hStop, the
// destructor's signal) from anything else — abandoned, failed or timed out —
// which all report -1. Nothing in the DLL reads it; it lands in the thread's
// exit code.
//
// ORACLE: NONE, by construction, and this function is the reason the file
// banner's "DIFF HARNESS: none of these 8" paragraph exists at all. It blocks
// on kernel objects and dispatches into caller-supplied virtual work; running
// it twice would run the work twice and drain the events. The A/B is
// BANISHED_RT_THREAD=0 (see RtSystemThreadFunc above), and the observable is
// coarse but real: the async pathfinder and the game-teardown worker are
// System::Thread subclasses, so a wrong ThreadFunc is a hang or a dead
// pathfinder, not a subtle drift.
//
// *** IT ALSO HAS NO `calls=` COLUMN, AND THAT NEEDED FIXING SEPARATELY. ***
//
// RT_DIFF_BEGIN is not usable here and the reason is not squeamishness:
// runtime/funcs/rt_diff.cpp has NO synchronisation at all — a bare
// `RtDiffSite* g_sites[]`, a bare `g_siteCount++`, bare `site->calls++`. This
// body runs on a WORKER THREAD, so a diff site inside it would be a data race
// in every thread the game creates, and the counter it produced could not be
// trusted anyway.
//
// So the port carries the witness below instead. This matters because of the
// exact hat `reference_uncalled_port_is_not_verified.md` describes: the residue
// census shows `?ThreadFunc@Thread@System@@CAIPEAX@Z` being RESOLVED, and
// resolved is not called. An eagerly-resolved name appears whether or not
// anything ever calls through the pointer. Without a witness, this port would
// land looking fine and be entirely unexercised, which is how a wrong
// Quaternion::operator* survived the whole of Wave 1.
//
// BANISHED_RT_THREADFUNC_TRACE=1 prints ONE line the first time each worker
// dispatches work, naming the Thread object and its vtable — which identifies
// the subclass, so the log says WHICH workers ran, not just that some did.
// Bounded by the number of threads (a handful), so it is cheap enough to leave
// in, and `getenv` is process-cached (standalone/env_cache.h).
//
// **Zero lines under that variable means zero dispatches, and that is the
// signal to read.** It is an EXECUTION WITNESS, not a correctness oracle;
// nothing about it says the body is right, only that it ran.
//
// The second line it can print is a real measurement rather than a tautology:
// retail RELOADS the vtable between slot 0 and slot 1 (0x1800ca48c), and
// whether that reload ever observes a different pointer decides whether a port
// that cached it would be wrong. If the line never appears, the reload is dead
// in practice and we know it; if it appears, our fidelity to it is load-bearing.
// ============================================================================
unsigned int __stdcall RtSystem_Thread_ThreadFunc(void* param) {
    Thread* self = static_cast<Thread*>(param);

    const bool trace = RtWaveTraceOn("BANISHED_RT_THREADFUNC_TRACE");
    bool announced = false;        // per-thread: this frame is one worker
    bool reloadReported = false;

    HANDLE handles[2] = { reinterpret_cast<HANDLE>(self->hStart),
                          reinterpret_cast<HANDLE>(self->hStop) };

    unsigned int r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    while (r == 0) {
        using WorkFn = void (*)(Thread*);

        void** vt = static_cast<void**>(self->vtable);
        if (trace && !announced) {
            announced = true;
            std::fprintf(stderr,
                         "[RtThreadFunc] PORT DISPATCHED work: thread=%lu self=%p vtable=%p\n",
                         GetCurrentThreadId(), (void*)self, (void*)vt);
        }
        self->runFlags |= 1u;
        reinterpret_cast<WorkFn>(vt[0])(self);        // slot 0 — the work

        void** vtAfter = static_cast<void**>(self->vtable);   // reloaded, as retail does
        if (trace && !reloadReported && vtAfter != vt) {
            reloadReported = true;
            std::fprintf(stderr,
                         "[RtThreadFunc] vtable RELOAD IS LOAD-BEARING: thread=%lu %p -> %p "
                         "(a port that cached the vtable would call the wrong OnCompleted)\n",
                         GetCurrentThreadId(), (void*)vt, (void*)vtAfter);
        }
        reinterpret_cast<WorkFn>(vtAfter[1])(self);   // slot 1 — OnCompleted

        HANDLE finished = reinterpret_cast<HANDLE>(self->hFinished);
        self->runFlags &= ~1u;
        SetEvent(finished);

        r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }
    return (r == 1) ? 0u : 0xFFFFFFFFu;
}

// ============================================================================
// System::Thread::OnCompleted()   retail 0x18000c590   (Wave 9 lane B3)
// ?OnCompleted@Thread@System@@UEAAXXZ
// ----------------------------------------------------------------------------
// `RET`. One instruction: the base class's do-nothing hook, called from
// ThreadFunc's slot 1 above once the work function returns. Derived classes
// that care override it; `Engine::Loader` does not.
//
// WHY A ONE-INSTRUCTION PORT IS WORTH LANDING. It fills a trapped slot in TWO
// minted vtables at once, which is the only reason it was picked up here:
//   ??_7Thread@System@@6B@  0x180155f20 = [ _purecall, OnCompleted ]
//   ??_7Loader@Engine@@6B@  0x180157a08 inherits OnCompleted in slot 1
// `Run` is pure in Thread and Loader overrides it, so slot 0 differs between
// the two while slot 1 is the same function. Lane C2's mint picks this up with
// no regeneration.
//
// NO DIFF SITE, and this is a rule rather than a judgement call. This body runs
// on a WORKER thread (ThreadFunc dispatches it), and `runtime/funcs/rt_diff.cpp`
// contains zero synchronisation primitives -- no mutex, no atomic, no
// Interlocked, no CRITICAL_SECTION -- so registration is a bare
// `g_sites[g_siteCount++]` and counting a bare `site->calls++`. A site here
// would be a data race in every worker the game creates and would produce a
// count nobody could trust (WAVE9.md §2). Lane P hit exactly this porting
// ThreadFunc and shipped an opt-in execution witness instead; the same witness
// is used below, and the same property holds for it: ZERO LINES MEANS ZERO
// DISPATCHES, so an absence is informative rather than merely quiet.
//
// There is also nothing an oracle could compare: the shipped body is `RET`, so
// ours and theirs agree on every input by construction and a green row would be
// a no-op (reference_probe_needs_an_effectiveness_control.md).
void RtSystem_Thread_OnCompleted(Thread* self) {
    static const bool trace = std::getenv("BANISHED_RT_THREADFUNC_TRACE") != nullptr;
    static bool announced = false;
    if (trace && !announced) {
        announced = true;
        std::fprintf(stderr,
                     "[RtThreadFunc] PORT DISPATCHED OnCompleted: thread=%lu self=%p\n",
                     GetCurrentThreadId(), (void*)self);
    }
    // Retail's body, in full.
}

// ---- Registration ------------------------------------------------------------

int RtSystemThreadRegister() {
    int n = 0;
    // Wave 9 lane B3: fills slot 1 of BOTH ??_7Thread@System@@ and
    // ??_7Loader@Engine@@ -- see the note above the body.
    RegisterRuntimeExport("?OnCompleted@Thread@System@@UEAAXXZ", (void*)&RtSystem_Thread_OnCompleted); ++n;
    RegisterRuntimeExport("??0CriticalSection@System@@QEAA@XZ", (void*)&RtSystem_CriticalSection_Ctor); ++n;
    RegisterRuntimeExport("??1CriticalSection@System@@QEAA@XZ", (void*)&RtSystem_CriticalSection_Dtor); ++n;
    RegisterRuntimeExport("?Lock@CriticalSection@System@@QEAAXXZ", (void*)&RtSystem_CriticalSection_Lock); ++n;
    RegisterRuntimeExport("?Unlock@CriticalSection@System@@QEAAXXZ", (void*)&RtSystem_CriticalSection_Unlock); ++n;

    RegisterRuntimeExport("??0Thread@System@@QEAA@I@Z", (void*)&RtSystem_Thread_Ctor); ++n;
    RegisterRuntimeExport("??1Thread@System@@QEAA@XZ", (void*)&RtSystem_Thread_Dtor); ++n;
    RegisterRuntimeExport("?Start@Thread@System@@QEAAXXZ", (void*)&RtSystem_Thread_Start); ++n;
    RegisterRuntimeExport("?WaitForFinish@Thread@System@@QEAAXXZ", (void*)&RtSystem_Thread_WaitForFinish); ++n;

    // Wave 9 lane P. Registered by ADDRESS as well as by name: the ctor above
    // asks by name, but the DLL's own ??0Thread ctor (still live for any Thread
    // the DLL builds itself) hands 0x1800ca440 to _beginthreadex, and Wave 3's
    // seam only redirects a call that goes through a resolver. Registering the
    // address costs nothing and means a future by-address caller lands on us.
    RegisterRuntimeExport("?ThreadFunc@Thread@System@@CAIPEAX@Z", (void*)&RtSystem_Thread_ThreadFunc); ++n;
    RegisterRuntimeAddr(0x1800ca440, (void*)&RtSystem_Thread_ThreadFunc);

    return n;
}
