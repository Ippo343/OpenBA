// ============================================================================
// runtime/src/io/filesystem_timestamp_win.cpp — BATCH 5 (2026-09-12)
//   `?GetFileTimeStamp@FileSystem@IO@@QEBA?AVDateTime@System@@AEBVFilename@4@@Z`
//   retail 0x1800ae270, ordinal 2607, 215 bytes / 48 instructions.
// ============================================================================
//
//     System::DateTime FileSystem::GetFileTimeStamp(const Filename& path) const
//
// The last-write time of a LOOSE FILE ON DISK, as a raw FILETIME qword. Used by
// the save-list job (0x1800b6290, ported in this same commit) to stamp every
// `.sav` it finds.
//
// ---------------------------------------------------------------------------
// THE HIDDEN RETURN SLOT — `?AV` IN THE MANGLING, RDX ON THE WIRE
// ---------------------------------------------------------------------------
// `?AVDateTime@System@@` is a CLASS RETURNED BY VALUE, so the MS x64 ABI gives
// it a hidden return slot in RDX and shifts every real argument right: RCX is
// `this`, RDX is the slot, R8 is `path` (0x1800ae289 `MOV RBX,RDX` /
// 0x1800ae28c `MOV RDX,R8`). Declaring it without that slot is a WILD WRITE
// through a stale RDX — Wave 1's defect 1, which scribbled 0x30 bytes on every
// menu scene load (reference_hidden_retslot_wild_write.md). The function
// returns the slot in RAX (0x1800ae334), and the port does too.
//
// `this` IS READ BY NOTHING. RCX is overwritten at 0x1800ae28f and never
// reloaded. The parameter is kept because the mangling is a const member and
// the caller passes it.
//
// ---------------------------------------------------------------------------
// THE WIN32 BLOCK, TRANSCRIBED RATHER THAN NAMED FROM HABIT
// ---------------------------------------------------------------------------
//   0x1800ae294  ??0WideString@System@@QEAA@AEBVString@1@@Z  (lane FS's port)
//   0x1800ae2c0  CreateFileW  [0x18014b320]
//                  dwDesiredAccess       0x80000000  GENERIC_READ
//                  dwShareMode           1           FILE_SHARE_READ
//                  lpSecurityAttributes  NULL
//                  dwCreationDisposition 3           OPEN_EXISTING
//                  dwFlagsAndAttributes  0x80        FILE_ATTRIBUTE_NORMAL
//                  hTemplateFile         NULL
//   0x1800ae30c  GetFileTime  [0x18014b1e8]  — creation and access times are
//                  BOTH NULL; only lpLastWriteTime is asked for
//   0x1800ae319 / 0x1800ae321  CloseHandle  [0x18014b3b8]  — on BOTH arms
//
// THE WIDESTRING IS RELEASED BEFORE THE HANDLE IS TESTED (0x1800ae2c9), not
// after the read. Kept in that order.
//
// THE HANDLE TEST IS `LEA RAX,[RDI-1] / CMP RAX,-3 / JA fail` — an UNSIGNED
// test that rejects exactly two values, NULL (0) and INVALID_HANDLE_VALUE (-1).
// Spelled as retail spells it rather than as `h == INVALID_HANDLE_VALUE || !h`,
// because the two are only equivalent by accident of those being the only wrap
// cases (the same note filesystem_openfile_win.cpp carries for the identical
// sequence at 0x1800ade12).
//
// EVERY FAILURE RETURNS A ZERO DateTime (0x1800ae331, `MOV [RBX],RSI` with RSI
// zero): a missing file, a locked file, and a `GetFileTime` that fails all
// produce the same 0 — there is no error channel. The caller cannot tell a
// 1601-01-01 timestamp from a failure, and that is retail's design.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A `_win` FILE (PLAN.md §4.1 decision 1)
// ---------------------------------------------------------------------------
// The whole body is three Win32 calls on a UTF-16 path; there is no
// platform-neutral half worth splitting out. A `_posix` twin would be `stat`
// and a FILETIME conversion, and it belongs next to this file rather than
// inside it.
//
// ---------------------------------------------------------------------------
// ORACLE — a full one is possible and is NOT added, on purpose
// ---------------------------------------------------------------------------
// The function is a PURE READ of the filesystem, so an ours-then-theirs replay
// would be safe. It is declined for one reason: the replay opens the file a
// SECOND time, and the population is "every .sav in the save directory on every
// refresh", so a diff run would double the file-open traffic of the save
// browser for a comparison of one qword that the save list's own contents
// already expose. DIRECTION.md §2 — use the oracle, do not build more of it.
// The end-to-end check a boot gives is stronger anyway: a wrong timestamp here
// reorders the save list visibly.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;

namespace {
typedef void* (*WideStringCtorFn)(void*, const String*);
}  // namespace

// ============================================================================
// ?GetFileTimeStamp@FileSystem@IO@@QEBA?AVDateTime@System@@AEBVFilename@4@@Z
//                                                          retail 0x1800ae270
//
//   (this, hidden return slot, path) — see the banner.
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 5a: the body is now
// `IO::FileSystem::GetFileTimeStamp`, below. The door keeps its name, its
// (self, hidden return slot, path) signature and both registration lines, and
// delegates. No qualified `IO::` call is needed: `IO::FileSystem` has no vtable
// at all (runtime/types/io/FileSystem.h), so B34_BRIEFS section A.1's re-entry
// hazard cannot arise.
//
// THE HIDDEN RETURN SLOT STAYS ON THE DOOR. The member returns the qword by
// value (`System::DateTime` does not exist in this tree — FileSystem.h's
// declaration comment says why one is not invented), and the door writes it
// into RDX's slot and returns the slot in RAX, exactly as 0x1800ae334 does.
uint64_t* RtIo_FileSystem_GetFileTimeStamp(const void* self,
                                           uint64_t* ret,
                                           const Filename* path) {
    *ret = reinterpret_cast<const IO::FileSystem*>(self)->GetFileTimeStamp(*path);
    return ret;
}

}  // namespace RtIO

// ============================================================================
// `IO::FileSystem::GetFileTimeStamp` — the member body
// ============================================================================
// Defined outside `namespace RtIO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); it reaches this
// translation unit's `RtIO`-scoped names — including the anonymous-namespace
// typedef — as `RtIO::X`.
//
// NOTHING ELSE CHANGED: same three Win32 calls in the same order, the same
// release-the-WideString-before-testing-the-handle order, the same unsigned
// two-value handle test, the same zero on every failure arm. `this` is still
// read by nothing, exactly as retail's RCX is not.
namespace IO {

uint64_t FileSystem::GetFileTimeStamp(const RtSystem::Filename& pathRef) const {
    const RtSystem::Filename* const path = &pathRef;

    // WideString ws(*path) — lane FS's port, reached through the name door so
    // a promotion of the row is followed.
    struct { void* data; } ws = { nullptr };                     // 0x1800ae28f
    RT_EXPORT_ONCE(RtIO::WideStringCtorFn, "??0WideString@System@@QEAA@AEBVString@1@@Z")(
        &ws, reinterpret_cast<const RtSystem::String*>(path));   // 0x1800ae294

    const HANDLE h = ::CreateFileW(static_cast<LPCWSTR>(ws.data),
                                   0x80000000u, 1u, nullptr, 3u, 0x80u, nullptr);

    RtIO::RtString_ReleaseInline(
        reinterpret_cast<RtSystem::String*>(&ws));               // 0x1800ae2c9

    // 0x1800ae2f5 — rejects exactly NULL and INVALID_HANDLE_VALUE. See banner.
    const uint64_t hm1 = reinterpret_cast<uint64_t>(h) - 1u;
    if (hm1 <= static_cast<uint64_t>(-3)) {
        FILETIME lastWrite;                                      // [RSP+0x40]
        if (::GetFileTime(h, nullptr, nullptr, &lastWrite) != 0) {
            ::CloseHandle(h);                                    // 0x1800ae321
            return *reinterpret_cast<const uint64_t*>(&lastWrite);
        }
        ::CloseHandle(h);                                        // 0x1800ae319
    }

    return 0;                                                    // 0x1800ae331
}

}  // namespace IO

// ============================================================================
// Registration — BOTH DOORS. `QEBA` is public and the name is ordinal 2607 in
// the retail export directory, verified with pefile before this was written.
// The name is spelled as a LITERAL so tools/runtime_census.py can see it.
// ============================================================================
int RtIoFileSystemTimeStampRegister() {
    int n = 0;
    RegisterRuntimeExport("?GetFileTimeStamp@FileSystem@IO@@QEBA?AVDateTime@System@@"
                          "AEBVFilename@4@@Z",
                          (void*)&RtIO::RtIo_FileSystem_GetFileTimeStamp); ++n;
    RegisterRuntimeAddr(0x1800ae270, (void*)&RtIO::RtIo_FileSystem_GetFileTimeStamp);
    return n;
}
