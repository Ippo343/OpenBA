// ============================================================================
// runtime/src/io/persistence_startup.cpp — Wave 9 lane ENG, iteration 3
// IO::Persistence::Startup — Engine::Startup's step 4
// ============================================================================
//
//   | Export                             | Retail      | Ord  | Instrs |
//   |------------------------------------|-------------|------|--------|
//   | ?Startup@Persistence@IO@@QEAA_NXZ  | 0x1800b7ee0 | 4103 |   45   |
//
// Name checked against Runtime-x64.dll's export directory with pefile before a
// line was written. No new Win32 API and no new import library.
//
// Its own file rather than an edit to `persistence.cpp` (Wave 4 lane C) purely
// so two lanes are not editing one file while both are in flight. Registered
// through `RtIoPersistenceStartupRegister()`, wired into `rt_io_port.cpp` with
// its own switch `the old DLL-era switch` — it is on the process's single boot
// path, so it must be A/B-able without giving up lane C's `Get`/`Set` family.
//
// This file touches nothing under `runtime/src/io/filesystem*`; lane FS owns
// those. It CALLS `?GetSaveDirectory@FileSystem@IO@@` by name, which is a
// resolve, not an edit.
//
// ---------------------------------------------------------------------------
// THE BODY
// ---------------------------------------------------------------------------
//   0x1800b7ef7  ReadSettings(this)                       ; ord 3462
//   0x1800b7f01  Filename tmp = FileSystem::_singleton->GetSaveDirectory()
//                                                          ; ord 2886, hidden
//                                                            return slot in RDX
//   0x1800b7f10  this->_savePath = tmp                    ; String::operator=,
//                                                            INLINED
//   0x1800b7f5d  ~tmp
//   0x1800b7f8e  return true                              ; MOV AL,1 -- retail
//                                                            has NO failure arm
//
// `GetSaveDirectory` returns a `System::Filename` BY VALUE, so the hidden
// return slot is the FIRST argument and `this` is the second
// (reference_hidden_retval_and_int_pointer_args.md). Retail passes
// `RCX = FileSystem::_singleton`, `RDX = &tmp` and then reads the result back
// out of `[RAX]`, where `RAX == RDX`. `Filename` and `String` are
// layout-identical (runtime/types/system/Filename.h), one pointer.
//
// The assignment at 0x1800b7f10 is `String::operator=` inlined, and it is
// written out here rather than delegated because of its FIRST test:
//
//     CMP RCX,[RAX]   ; if the two String::data pointers are already EQUAL,
//     JZ  skip        ; do nothing at all -- no release, no AddRef
//
// A self-assignment through the release-then-adopt path would free the buffer
// and then AddRef freed memory. Retail's guard is what prevents that, and it is
// the one line here that a "tidy" rewrite would drop.
//
// ---------------------------------------------------------------------------
// THIS ROW CLOSES; ITS FIRST CALLEE DOES NOT
// ---------------------------------------------------------------------------
// Stated per the triage rule: porting this does NOT get past step 4. It moves
// the fault to **`?ReadSettings@Persistence@IO@@QEAAXXZ`** (0x1800b84b0, ord
// 3462, **256 instructions**), which is the registry reader. Behind that is
// `?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` (0x1800acfd0,
// ord 2886), which is lane FS's class.
//
// Both are reached by NAME, so each arrives as one `[RtUnresolved]` line rather
// than an anonymous EXEC-of-0 — which is the whole value of closing this row
// ahead of the wall.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// None. It runs once, from `Engine::Startup`; `ReadSettings` reads the Windows
// registry into the object and a replay would do it twice. The shadow checks
// the post-condition the assignment produced and this body did not compute:
// `_savePath` holds the same `data` pointer the temporary did, with a refcount
// that accounts for both. `the diff oracle` is the effectiveness control.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <utility>    // std::move -- the pointer steal out of a POD out-param

#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../../types/io/FileSystem.h"   // ENDGAME B.3.9 IO: the class static
#include "../../types/system/String.h"
#include "../system/pod_bridge.h"
#include "../../types/system/Filename.h"
#include <new>

using RtIo::Persistence;
using RtSystem::String;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)

namespace {

// ?_singleton@FileSystem@IO@@0PEAV12@EA, ord 4811, retail 0x1801fc6a8.
// Written by `??0FileSystem@IO@@QEAA@XZ`, which Wave 9 lane X ported; read here
// through the PORT-AWARE door so it follows lane C2's mint at cutover.
void** FileSystemSingletonSlot() {
    return reinterpret_cast<void**>(&::IO::FileSystem::_singleton);   // ENDGAME B.3.9 IO (D7): the class static.
}

}  // namespace

// ============================================================================
// ?Startup@Persistence@IO@@QEAA_NXZ                        retail 0x1800b7ee0
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 6: the body is now
// `IO::Persistence::Startup()`, below. The door keeps its name, its
// `extern "C"` linkage and both registration lines, and delegates. No qualified
// `IO::` call is needed: `IO::Persistence` has no vtable of its own -- the vptr
// at +0x10 is the EMBEDDED worker's -- so B34_BRIEFS section A.1's re-entry
// hazard cannot arise.
extern "C" bool RtIo_Persistence_Startup(Persistence* self) {
    return reinterpret_cast<IO::Persistence*>(self)->Startup();
}

// ============================================================================
// `IO::Persistence::Startup` -- the member body
// ============================================================================
// Defined in `namespace IO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); the file-local helper
// lives in this translation unit's GLOBAL anonymous namespace and is reached as
// `::FileSystemSingletonSlot`.
//
// B3R RT-IO: `ReadSettings` and `FileSystem::GetSaveDirectory` are called by
// name (they were two RT_EXPORT_ONCE doors). The unguarded FileSystem-singleton
// DEREFERENCE stays unguarded and the guard on whether the SLOT EXISTS stays
// exactly where it was.
namespace IO {

bool Persistence::Startup() {

    ReadSettings();

    // GetSaveDirectory's by-value Filename is constructed into the POD slot
    // (as the door that stood here did) and then stolen into the class -- a
    // pure pointer steal, no AddRef, `tmpPod` left null.
    RtSystem::String tmpPod{nullptr};

    // Retail dereferences FileSystem::_singleton without a check (0x1800b7f01
    // is a bare `MOV RCX,[0x1801fc6a8]`). The DEREFERENCE stays unguarded. What
    // is guarded is whether the SLOT EXISTS at all: with no DLL that variable
    // exists only if the mint provides it. Our difference from retail, not a
    // guard over retail's logic.
    if (void** fsSlot = ::FileSystemSingletonSlot()) {
        ::new (static_cast<void*>(&tmpPod)) System::Filename(          // B.3.x bridge
            static_cast<const ::IO::FileSystem*>(*fsSlot)->GetSaveDirectory());
    }
    System::String tmp(std::move(::Sys(tmpPod)));

    // String::operator=, retail 0x1800c9810, inlined at 0x1800b7f10 -- the
    // exact self-assignment guard on the BUFFER (the banner above). `savePath`
    // is a `RtSystem::String` member of the not-yet-converted `Persistence`,
    // reached through the one transitional bridge.
    ::Sys(_savePath) = tmp;

    // ~tmp is retail's release at 0x1800b7f5d.

    return true;
}

}  // namespace IO

// ============================================================================
// Registration
// ============================================================================
int RtIoPersistenceStartupRegister() {
    int n = 0;
    RegisterRuntimeExport("?Startup@Persistence@IO@@QEAA_NXZ", (void*)&RtIo_Persistence_Startup); ++n;
    RegisterRuntimeAddr(0x1800b7ee0, (void*)&RtIo_Persistence_Startup);

    // DELIBERATELY ABSENT -- both callees, both exported, both unported:
    //   ?ReadSettings@Persistence@IO@@QEAAXXZ   0x1800b84b0  ord 3462  256 instrs
    //   ?GetSaveDirectory@FileSystem@IO@@…      0x1800acfd0  ord 2886  (lane FS)
    return n;
}
