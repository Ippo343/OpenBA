// ============================================================================
// runtime/src/io/persistence_saves_win.cpp — Wave 19 lane PERSIST
// ============================================================================
//
// One row: ?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z
// (retail 0x1800b7870, ordinal 1642).
//
// It sits in a _win file for one reason: it is the ONLY member of this lane's
// seven that reaches the Win32 event API in its own body. Its two siblings
// (BeginDelete, BeginLoad) reach the same two calls, but through an
// unexported DLL helper, so their C++ stays platform-neutral and lives in
// persistence_saves.cpp. That asymmetry is retail's, not ours: 0x1800b7870
// does not delegate to an enqueue helper, it inlines the whole issue
// sequence.
//
// The two imports are declared here rather than pulled in from <windows.h>,
// which rt_port.h's banner explains cannot be included anywhere under
// runtime/src/ (it #defines `near`/`far` away and declares a function called
// `Rectangle`). The IAT slots were identified from the shipped binary, not
// guessed: 0x18014b198 is KERNEL32!ResetEvent and 0x18014b368 is
// KERNEL32!SetEvent, read out of Runtime-x64.dll's import directory.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../system/string.h"

using RtSystem::String;
using RtSystem::StringRefCount;
using RtIo::Persistence;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The port's own GetSaveFilename, from the sibling file. Called directly and
// not through the registry: retail's 0x1800b789f is a direct rel32 CALL to
// 0x1800b7c80, so a direct call is the faithful spelling, and it also means
// this row cannot silently end up half on the DLL.
void* RtIo_Persistence_GetSaveFilename(const void* self, void* retval, const void* nameRef);

extern "C" __declspec(dllimport) int __stdcall ResetEvent(void* handle);
extern "C" __declspec(dllimport) int __stdcall SetEvent(void* handle);

namespace {

// Two unexported Collection internals, called by address. 0x18002a930 clears
// the stride-0xc block-descriptor array at +0x60; 0x180024aa0 clears the
// pointer array at +0x70. Both are shared DLL code (the same 0x180024aa0 is
// reached from the BeginLoad enqueue helper at 0x1800b7508), not
// Persistence-specific, so this lane does not own them.
using ArrayClearFn = void (*)(void* array);

ArrayClearFn ClearBlockDescs() {
    static auto fn = reinterpret_cast<ArrayClearFn>(
        ResolveAddr(Module::Runtime, 0x18002a930));
    return fn;
}
ArrayClearFn ClearBlockPtrs() {
    static auto fn = reinterpret_cast<ArrayClearFn>(
        ResolveAddr(Module::Runtime, 0x180024aa0));
    return fn;
}

}  // namespace

// ============================================================================
// bool IO::Persistence::BeginGetSaveGameInfo(const String& name, int count)
// retail 0x1800b7870
// ----------------------------------------------------------------------------
// Instruction-order note: retail writes `command = 3` (0x1800b78b7) BEFORE the
// filename assignment and `status = 0` (0x1800b7903) after it. The order is
// preserved below even though nothing in this thread can observe the
// difference — the worker thread can, and it is woken only by the SetEvent at
// the end, so any reordering here is a change to what a racing reader could
// see. Faithful transcription costs nothing; reasoning about the race costs
// a wave.
// ============================================================================
bool RtIo_Persistence_BeginGetSaveGameInfo(void* self, const void* nameRef, int count) {

    auto* p = static_cast<Persistence*>(self);
    if (p->flags & 1u) return false;

    String fn;
    fn.data = nullptr;
    RtIo_Persistence_GetSaveFilename(self, &fn, nameRef);

    ClearBlockDescs()(&p->blockDescData);
    ClearBlockPtrs()(&p->blockPtrData);

    p->command = 3;

    // The guarded String assignment at 0x1800b78be..0x1800b7903: retail skips
    // the whole release/adopt when the two buffers are already the same
    // pointer, which is not just an optimisation — releasing first would drop
    // the last reference to the buffer it is about to re-adopt.
    if (p->pendingFilename.data != fn.data) {
        RtSystem::RtString_ReleaseInline(&p->pendingFilename);
        p->pendingFilename.data = fn.data;
        if (p->pendingFilename.data) ++StringRefCount(p->pendingFilename.data);
    }

    p->status     = 0;
    p->blockCount = count;

    ResetEvent(p->doneEvent);
    SetEvent(p->workEvent);

    RtSystem::RtString_ReleaseInline(&fn);
    return true;
}

// ---------------------------------------------------------------------------
int RtIoPersistenceSavesWinRegister() {
    RegisterRuntimeExport(
        "?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z",
        (void*)&RtIo_Persistence_BeginGetSaveGameInfo);
    RegisterRuntimeAddr(0x1800b7870, (void*)&RtIo_Persistence_BeginGetSaveGameInfo);
    return 1;
}
