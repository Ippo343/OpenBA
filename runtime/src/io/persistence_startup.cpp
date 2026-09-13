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

#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../../types/system/String.h"

using RtIo::Persistence;
using RtSystem::String;

namespace {

typedef void    (*VoidThisFn)(void*);
typedef String* (*GetSaveDirFn)(void* self, String* ret);
typedef void    (*StringDtorFn)(String*);

// ?_singleton@FileSystem@IO@@0PEAV12@EA, ord 4811, retail 0x1801fc6a8.
// Written by `??0FileSystem@IO@@QEAA@XZ`, which Wave 9 lane X ported; read here
// through the PORT-AWARE door so it follows lane C2's mint at cutover.
void** FileSystemSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

}  // namespace

// ============================================================================
// ?Startup@Persistence@IO@@QEAA_NXZ                        retail 0x1800b7ee0
// ============================================================================
extern "C" bool RtIo_Persistence_Startup(Persistence* self) {

    RT_EXPORT_ONCE(VoidThisFn, "?ReadSettings@Persistence@IO@@QEAAXXZ")(self);

    String tmp;
    tmp.data = nullptr;

    // Retail dereferences FileSystem::_singleton without a check (0x1800b7f01
    // is a bare `MOV RCX,[0x1801fc6a8]`). The DEREFERENCE stays unguarded. What
    // is guarded is whether the SLOT EXISTS at all: with no DLL that variable
    // exists only if the mint provides it. Our difference from retail, not a
    // guard over retail's logic.
    if (void** fsSlot = FileSystemSingletonSlot()) {
        RT_EXPORT_ONCE(GetSaveDirFn,
            "?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")(*fsSlot, &tmp);
    }

    // String::operator=, inlined at 0x1800b7f10. The leading equality test is
    // retail's self-assignment guard -- see the banner.
    if (self->savePath.data != tmp.data) {
        if (char* old = self->savePath.data) {
            if (--RtSystem::StringRefCount(old) == 0) {
                self->savePath.data = old - 8;
                RT_EXPORT_ONCE(VoidThisFn,
                    "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(self->savePath.data);
            } else {
                self->savePath.data = nullptr;
            }
        }
        self->savePath.data = tmp.data;
        if (tmp.data) ++RtSystem::StringRefCount(tmp.data);
    }

    RT_EXPORT_ONCE(StringDtorFn, "??1String@System@@QEAA@XZ")(&tmp);


    return true;
}

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
