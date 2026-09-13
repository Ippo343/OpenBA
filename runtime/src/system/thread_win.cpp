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
//   and the ctor below now asks for it BY NAME through the port-aware resolver.
//   The old reasoning stays true of the LAYOUT — it only touches +0x00 vtable,
//   +0x08 runFlags, +0x10/0x18/0x20/0x28 handles, all of which this header
//   reproduces exactly — which is what made it safe to port.
// ============================================================================

#include "../../types/system/Thread.h"
#include "../../funcs/rt_port.h"

#include <cstdio>

#include <windows.h>
#include <process.h>   // _beginthreadex, the CRT routine 0x18007fbfc is a copy of

using System::CriticalSection;
using System::Thread;


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
//
// WAVE 9 LANE SYS2: this used to be `static void* v = ResolveExport(...)`, a
// plain function-local static — the exact defect attempt 14 fixed for the
// allocator's three by-address call sites, one file over. The Thread ctor runs
// DURING RuntimePortInit (ConstructLoader, from a singleton ctor), which is
// BEFORE RtDataMintInit() ever builds the minted table. At cutover the first
// call therefore froze a NULL in for the life of the process, and attempt 14's
// log shows it as the very first `[RtUnresolved]` line of the whole run —
// line 6, above every other symbol, before the mint had a chance to exist.
//
// The mint DOES carry this row (rt_data_mint_tables.inc, 2 slots: _purecall +
// ?OnCompleted@Thread@System@@UEAAXXZ), so the row was never missing -- only
// unreachable through a cache that had already answered. Going through
// RT_DATA_SLOT (RtResolveCached: cache a hit, RETRY a miss, name it once) is
// the whole fix.
//
// WAVE 22 LANE SYSBODY: this sentence used to name the slot array `kVSlots42`.
// It is `kVSlots53` today. The array names are GENERATOR-ASSIGNED indices and
// they renumber whenever a row is added or removed above this one, so any
// comment quoting one goes stale silently and with no detector
// (reference_a_generated_file_goes_stale_with_no_detector.md). The name is
// DROPPED rather than corrected -- the row is findable by its symbol, which
// does not drift.
//
// WAVE 22 LANE SYSBODY -- THE PROMOTION VERDICT, AND IT IS ROUTE-SCOPED.
// The promotion backlog reads this row 1/1 COMPLETE and that is CORRECT: the
// table has two slots, slot 0 is _purecall, slot 1 is OnCompleted, and
// OnCompleted is ported (below). But COMPLETE says a promotion puts no trap in
// a live table; it does NOT say the row should be promoted
// (reference_a_green_readiness_screen_is_not_a_route.md). Promotability turns
// on owning what STAMPS the table, and the answer differs by route:
//
//   DLL PRESENT -- DO NOT PROMOTE. Two things stamp this vptr and we own only
//   one. `RtSystemThreadVftable()` below is a real door feeding our ported
//   Thread ctor/dtor (all three constructing call sites -- pathfinder_seed.cpp,
//   engine/singleton_ctors.cpp, standalone/rt_data_mint.cpp -- reach us by
//   NAME). Retail's own Thread ctor stamps 0x180155f20 with a RIP-relative LEA
//   inside the DLL, which is not a door and never will be
//   (reference_a_ported_row_can_be_permanently_unpromotable.md). Promoting
//   therefore puts TWO different vptr values in play for one class where today
//   there is one, and buys nothing: the only dispatched slot is slot 1, and
//   our OnCompleted and retail's are both a bare RET. Small cost, zero
//   benefit -- a reason not to do it, not a demonstration that it is safe.
//
//   CUTOVER -- REQUIRED. With Runtime-x64.dll absent, RT_DATA_SLOT has nothing
//   to fall through to and this function returns NULL, so our Thread ctor
//   stamps a null vptr into every Thread the process builds. That is the
//   attempt-14 failure this block was written about, one layer along.
//
// The two verdicts are opposite, which is the shape
// reference_unpromotable_is_route_scoped_not_permanent.md names, so neither
// may be quoted without its route. NOT PROMOTED by this lane: DLL-present is
// the route every run today takes, and promoting for a route that cannot boot
// yet would be a change nothing can measure.
static void* RtSystemThreadVftable() {
    return RT_DATA_SLOT(void*, "??_7Thread@System@@6B@");
}

// The worker entry point. Asked for BY NAME through the PORT-AWARE resolver, so
// this hands back our own RtSystem_Thread_ThreadFunc (below) once
// RtSystemThreadRegister has run.
//
// WAVE 9 LANE SYS2: same freeze hazard as the vtable above — a null or
// pre-registration answer frozen here is a bypassed port. RT_EXPORT_ONCE
// retries a miss.
static unsigned(__stdcall* RtSystemThreadFunc())(void*) {
    using Fn = unsigned(__stdcall*)(void*);
    return RT_EXPORT_ONCE(Fn, "?ThreadFunc@Thread@System@@CAIPEAX@Z");
}

// _beginthreadex. Retail called its own statically-linked CRT's copy
// (0x18007fbfc, the ctor's CALL target in the disassembly); ours is the
// identical routine with the same contract.
using BeginThreadExFn = uintptr_t(__cdecl*)(void*, unsigned, unsigned(__stdcall*)(void*), void*, unsigned, unsigned*);
static BeginThreadExFn RtDllBeginThreadEx() {
    return reinterpret_cast<BeginThreadExFn>(&_beginthreadex);
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
// Retail RELOADS the vtable between slot 0 and slot 1 (0x1800ca48c); that
// reload is reproduced faithfully below, because a port that cached the vtable
// would call the wrong OnCompleted if it ever changed.
// ============================================================================
unsigned int __stdcall RtSystem_Thread_ThreadFunc(void* param) {
    Thread* self = static_cast<Thread*>(param);

    HANDLE handles[2] = { reinterpret_cast<HANDLE>(self->hStart),
                          reinterpret_cast<HANDLE>(self->hStop) };

    unsigned int r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    while (r == 0) {
        using WorkFn = void (*)(Thread*);

        void** vt = static_cast<void**>(self->vtable);
        self->runFlags |= 1u;
        reinterpret_cast<WorkFn>(vt[0])(self);        // slot 0 — the work

        void** vtAfter = static_cast<void**>(self->vtable);   // reloaded, as retail does
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
void RtSystem_Thread_OnCompleted(Thread* self) {
    (void)self;
    // Retail's body, in full: the shipped 0x1800ca580 is a bare RET.
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
