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

#include <cstdio>

#include <windows.h>
#include <process.h>   // _beginthreadex, the CRT routine 0x18007fbfc is a copy of

// ============================================================================
// ENDGAME B.3.4b — THE CLASSES ARE REAL; THE DOORS DELEGATE. READ THIS.
// ============================================================================
// `System::CriticalSection` and `System::Thread` are real C++ classes since
// B.3.4b (runtime/types/system/Thread.h). The PODs moved to `RtSystem` in the
// same commit (rule B4b-1) and are what the ctor/dtor doors below still operate
// on, deliberately:
//
//   * Lock / Unlock / Start / WaitForFinish / OnCompleted / ThreadFunc are
//     MEMBERS now; their registered doors keep their addresses, their names and
//     their signatures, and delegate in one line.
//   * The four ctor/dtor doors stay FREE FUNCTIONS over the POD and are NOT
//     converted — B34_RULES §4.3 and B34B_RULES rule B4b-2. For `Thread` that is
//     because all four constructing sites stamp a vtable over +0x00 and two of
//     them are outside this step's file set (singleton_ctors.cpp,
//     rt_data_mint.cpp). For `CriticalSection` it is because two of its three
//     embedders (IO::Persistence, Audio::Stream) call raw
//     InitializeCriticalSection and never reach the door at all, so a real
//     constructor would leave those two constructed a second, different way.
//     They take the POD precisely so they can write `self->vtable` — the class
//     has no such field, the compiler owns +0x00 there.
//
// ⚠ `RtSystem_Thread_OnCompleted` delegates with a QUALIFIED call. Slot 1 of the
// MINTED table 0x180155f20 resolves back to that door, so an unqualified virtual
// call inside it would recurse forever (B34_BRIEFS §A.1). Start/WaitForFinish
// are non-virtual and need no qualification.
//
// The member BODIES are defined at the bottom of this file inside
// `namespace System { ... }`, because C++ requires a member definition to sit in
// a namespace enclosing the class (B34_BRIEFS §A.2). They are byte-for-byte the
// bodies that used to live in the doors; nothing was reordered.
// ============================================================================

using RtSystem::CriticalSection;
using RtSystem::Thread;


// ---- System::CriticalSection -----------------------------------------------

// 0x18000c5c0  ??0CriticalSection@System@@QEAA@XZ
// NOT a real constructor — see the banner above.
CriticalSection* RtSystem_CriticalSection_Ctor(CriticalSection* self) {
    InitializeCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
    SetCriticalSectionSpinCount(reinterpret_cast<LPCRITICAL_SECTION>(self), 5000);
    return self;
}

// 0x18000c5b0  ??1CriticalSection@System@@QEAA@XZ
// NOT a real destructor — see the banner above.
void RtSystem_CriticalSection_Dtor(CriticalSection* self) {
    DeleteCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(self));
}

// ---- System::Thread ---------------------------------------------------------

// ENDGAME B.3.9 VS (SYS W3, row 140): the minted `??_7Thread@System@@6B@` is
// RETIRED, and with it the three things that read it here -- the by-name
// `RtSystemThreadVftable()` lookup and the two ctor/dtor DOORS
// (`??0Thread@System@@QEAA@I@Z` 0x1800ca4e0, `??1Thread@System@@QEAA@XZ`
// 0x1800ca590) that stamped it before running the shared body. Every Thread in
// the build is now a C++ subclass built by `: System::Thread(stackSize)` and
// torn down by its compiler-chained `~Thread()` (Engine::Loader, Audio::Stream,
// IO::PersistenceWorker, the exe PathfinderThread), wearing compiler tables;
// no caller of either door was left (none by name, none by address, no exe
// import slot registered). Retail's `ThreadFunc` handoff (`LEA R8,[0x1800ca440]`)
// is `Thread::ThreadFunc` named directly in `InitFields`, with no first-use
// cache (B9-10).

// ============================================================================
// unsigned int System::Thread::ThreadFunc(void*)      retail 0x1800ca440
// ?ThreadFunc@Thread@System@@CAIPEAX@Z
//
// Wave 9, lane P (tier 0). The worker body every System::Thread runs — the last
// unported member of the class, so System::Thread is now COMPLETE (§4.1
// decision 5).
//
// ENDGAME B.3.4b: THE BODY MOVED onto `System::Thread::ThreadFunc` at the
// bottom of this file and what follows is the delegating door. This banner
// stays here, with the door that carries the registered address, because it
// documents the RETAIL bytes; the member body carries the note about why its
// two slot calls stay raw.
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

// ============================================================================
// System::Thread::OnCompleted()   retail 0x18000c590   (Wave 9 lane B3)
// ?OnCompleted@Thread@System@@UEAAXXZ
// ----------------------------------------------------------------------------

// ============================================================================
// ENDGAME B.3.4b — the member BODIES.
// ============================================================================
// They live in `namespace System` because C++ requires a member definition to
// appear in a namespace ENCLOSING the class, and `RtSystem` is a SIBLING of
// `System`, not a parent (B34_BRIEFS §A.2). Every body below is the one that
// used to sit in the door directly above it: same control flow, same callees,
// same order.
//
// ⚠ GREP HAZARD (B34_RULES §7.2): inside this block a bare `Thread` or
// `CriticalSection` binds the CLASS. The PODs must be spelled `RtSystem::`.
// ============================================================================

namespace System {

// 0x18000c700  ?Lock@CriticalSection@System@@QEAAXXZ
void CriticalSection::Lock() {
    EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(this));
}

// 0x18000c6f0  ?Unlock@CriticalSection@System@@QEAAXXZ
void CriticalSection::Unlock() {
    LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(this));
}

// 0x1800ca4e0  ??0Thread@System@@QEAA@I@Z -- the body MINUS the vptr store
// (the compiler's, in `Thread(unsigned)`; the minted one, in the door).
void Thread::InitFields(unsigned int stackSize) {
    _runFlags = 0;

    // Retail: XOR ECX,ECX (security=NULL); [RSP+0x20]=4 (CREATE_SUSPENDED) is
    // the LAST arg written before the call, so RDX (stackSize) passes through
    // unmodified. Retail called its own statically-linked CRT copy of
    // _beginthreadex (0x18007fbfc); ours is the identical routine.
    // CREATE_SUSPENDED matters: ThreadFunc's first act is to wait on {hStart,
    // hStop}, and those two handles (plus hFinished) are not created until
    // AFTER this call returns. Starting running immediately would have
    // ThreadFunc wait on uninitialized handles.
    unsigned int threadId = 0;
    uintptr_t h = _beginthreadex(nullptr, stackSize, &Thread::ThreadFunc, this,
                                 CREATE_SUSPENDED, &threadId);
    _hThread = reinterpret_cast<void*>(h);
    SetThreadPriority(reinterpret_cast<HANDLE>(_hThread), THREAD_PRIORITY_NORMAL);

    _hStart    = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    _hStop     = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    _hFinished = CreateEventW(nullptr, TRUE,  TRUE,  nullptr);

    ResumeThread(reinterpret_cast<HANDLE>(_hThread));
}

// 0x1800ca590  ??1Thread@System@@QEAA@XZ -- the body MINUS the vptr store.
void Thread::ReleaseFields() {
    SetEvent(reinterpret_cast<HANDLE>(_hStop));
    WaitForSingleObject(reinterpret_cast<HANDLE>(_hThread), INFINITE);
    CloseHandle(reinterpret_cast<HANDLE>(_hThread));
    CloseHandle(reinterpret_cast<HANDLE>(_hStart));
    CloseHandle(reinterpret_cast<HANDLE>(_hStop));
    CloseHandle(reinterpret_cast<HANDLE>(_hFinished));
}

// 0x1800ca610  ?Start@Thread@System@@QEAAXXZ
void Thread::Start() {
    ResetEvent(reinterpret_cast<HANDLE>(_hFinished));
    SetEvent(reinterpret_cast<HANDLE>(_hStart));
}

// 0x1800ca5f0  ?WaitForFinish@Thread@System@@QEAAXXZ
void Thread::WaitForFinish() {
    WaitForSingleObject(reinterpret_cast<HANDLE>(_hFinished), INFINITE);
}

// 0x18000c590  ?OnCompleted@Thread@System@@UEAAXXZ  — slot 1
void Thread::OnCompleted() {
    // Retail's body, in full: the shipped 0x18000c590 is a bare RET.
}

// 0x1800ca440  ?ThreadFunc@Thread@System@@CAIPEAX@Z
//
// THE RAW SLOT CALLS STAY, DELIBERATELY (B34_RULES §3.1's rule, applied here).
// `param` is a Thread-DERIVED object wearing a minted or a derived table —
// `Engine::Loader` (0x180157a08), `IO::Persistence`'s worker (0x180155860),
// `Audio::Stream`, the pathfinder (exe 0x14015ef18). Slot 0 is `Run`, which is
// pure in this class, so there is nothing to virtual-call; slot 1 resolves back
// to this file's own OnCompleted door, so a virtual call would be §A.1's
// recursion. Both therefore keep reading the table by hand, exactly as retail's
// 0x1800ca440 does.
//
// Retail RELOADS the vtable between slot 0 and slot 1 (0x1800ca48c); that
// reload is reproduced faithfully, because a port that cached the vtable would
// call the wrong OnCompleted if the work call re-stamped it.
unsigned int __stdcall Thread::ThreadFunc(void* param) {
    // Spelled as the POD: this body reads the vptr AS A FIELD, which is the one
    // thing the class deliberately does not expose (+0x00 belongs to the
    // compiler there). Same bytes either way -- the cross-asserts in Thread.h
    // are what say so.
    RtSystem::Thread* self = static_cast<RtSystem::Thread*>(param);

    HANDLE handles[2] = { reinterpret_cast<HANDLE>(self->hStart),
                          reinterpret_cast<HANDLE>(self->hStop) };

    unsigned int r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    while (r == 0) {
        using WorkFn = void (*)(RtSystem::Thread*);

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

}  // namespace System

// ---- Registration ------------------------------------------------------------
