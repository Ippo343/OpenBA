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
//   The worker thread's entry point is intentionally left as the DLL's own
//   ?ThreadFunc@Thread@System@@CAIPEAX@Z (resolved by address, not ported —
//   it is not one of the 8 exports in scope). It only touches the layout we
//   already reproduce exactly (+0x00 vtable, +0x08 runFlags, +0x10/0x18/0x20/
//   0x28 handles), so running the real one guarantees the worker's virtual
//   dispatch (slot 0 = work, slot 1 = OnCompleted) behaves identically to
//   retail regardless of which ctor built the object.
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

#include <windows.h>

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
// 0x180155f20 — slot 0 _purecall, slot 1 OnCompleted) and its own ThreadFunc
// (?ThreadFunc@Thread@System@@CAIPEAX@Z, 0x1800ca440), resolved once. Both are
// used AS-IS, never reimplemented, for the reasons in the file banner.
static void* RtSystemThreadVftable() {
    static void* v = ResolveExportRaw("??_7Thread@System@@6B@");
    return v;
}
static unsigned(__stdcall* RtSystemThreadFunc())(void*) {
    using Fn = unsigned(__stdcall*)(void*);
    static Fn f = reinterpret_cast<Fn>(ResolveExportRaw("?ThreadFunc@Thread@System@@CAIPEAX@Z"));
    return f;
}

// The DLL's OWN _beginthreadex (its statically-linked CRT's, not MinGW/UCRT's
// — see the CRT OWNERSHIP note in the file banner), resolved by raw address:
// 0x18007fbfc, confirmed as the ctor's CALL target in the disassembly.
using BeginThreadExFn = uintptr_t(__cdecl*)(void*, unsigned, unsigned(__stdcall*)(void*), void*, unsigned, unsigned*);
static BeginThreadExFn RtDllBeginThreadEx() {
    static BeginThreadExFn f = reinterpret_cast<BeginThreadExFn>(ResolveRuntimeRaw(0x18007fbfc));
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

// ---- Registration ------------------------------------------------------------

int RtSystemThreadRegister() {
    int n = 0;
    RegisterRuntimeExport("??0CriticalSection@System@@QEAA@XZ", (void*)&RtSystem_CriticalSection_Ctor); ++n;
    RegisterRuntimeExport("??1CriticalSection@System@@QEAA@XZ", (void*)&RtSystem_CriticalSection_Dtor); ++n;
    RegisterRuntimeExport("?Lock@CriticalSection@System@@QEAAXXZ", (void*)&RtSystem_CriticalSection_Lock); ++n;
    RegisterRuntimeExport("?Unlock@CriticalSection@System@@QEAAXXZ", (void*)&RtSystem_CriticalSection_Unlock); ++n;

    RegisterRuntimeExport("??0Thread@System@@QEAA@I@Z", (void*)&RtSystem_Thread_Ctor); ++n;
    RegisterRuntimeExport("??1Thread@System@@QEAA@XZ", (void*)&RtSystem_Thread_Dtor); ++n;
    RegisterRuntimeExport("?Start@Thread@System@@QEAAXXZ", (void*)&RtSystem_Thread_Start); ++n;
    RegisterRuntimeExport("?WaitForFinish@Thread@System@@QEAAXXZ", (void*)&RtSystem_Thread_WaitForFinish); ++n;

    return n;
}
