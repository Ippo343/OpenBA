// ============================================================================
// runtime/src/io/filesystem_win.cpp — Wave 4 lane D: FileSystem::GetSaveDirectory
// ============================================================================
// The one FileSystem export in this lane's scope that touches Win32
// (SHGetFolderPathW), so it lives behind the `_win` seam per PLAN.md §4.1
// decision 1: <windows.h> must never appear in filesystem.cpp.
//
//   ?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ  retail 0x1800acfd0
//
//     Filename FileSystem::GetSaveDirectory() const
//     {
//         wchar_t buf[264];
//         if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, buf) != S_OK) {
//             Error::SetText("FileSystem: Unable to retrive the Documents "
//                             "folder for saving!", true);
//             return Filename();               // null
//         }
//         WideString ws;  ws.Set(buf);
//         String utf8 = ws.AsUTF8();
//         String formatted = String::Format("%s\\%s\\", utf8.data, "Banished");
//         Filename ret;  ret.data = formatted.data;  if (ret.data) ++refcount;
//         ret.AdjustDelimiters();
//         return ret;
//     }
//
// Retail's `this` (RCX) is loaded into RBX but never dereferenced — confirmed
// in the disassembly, not just the decompile — so GetSaveDirectory reads NO
// FileSystem state at all; it is purely "where does Windows keep My
// Documents". The port's `fs` parameter is accepted and ignored for the same
// reason a byte-faithful transcription keeps an unused parameter: it is what
// retail's signature is.
//
// OUT-OF-SCOPE DEPENDENCIES CALLED THROUGH, NOT PORTED: System::WideString
// and System::Error. Both are exported; both are called via the raw DLL door
// exactly as filesystem.cpp calls the out-of-scope ExistsInPackfile/
// PlatformExists, and as Wave 3's Type::Serialize called the DLL's own
// ReaderWriter vtable (WAVE3_LANE_A.md §4). WideString was explicitly listed
// as "not called by our tree" and left unported in Wave 2 (PLAN.md §5) —
// GetSaveDirectory is the first caller. System::String::Format IS already
// ported (Wave 2 lane A, runtime/src/system/string.cpp) and is reused
// directly rather than re-resolved.
//
// THE THREE INLINED RELEASES in the decompile (the WideString buffer, the
// AsUTF8 result, and the Format result) are the SAME refcounted-buffer
// release idiom as System::String's own (`runtime/types/system/String.h`'s
// "RETAIL'S RELEASE IDIOM" section) — WideString shares String's allocation
// header, just over wchar_t characters instead of char. Confirmed by reading
// the release sequences byte-for-byte identical to RtString_ReleaseInline's
// shape; reused here via a reinterpret_cast to RtSystem::String rather than
// re-deriving a WideString-specific idiom for a type this lane does not own.
//
// DIFF: value oracle, not a byte oracle. GetSaveDirectory reads no shared
// FileSystem state and allocates a FRESH buffer on every call (unlike
// GetDataDirectory/GetSourceDirectory, which AddRef a shared one), so ours
// and theirs never share a pointer — the diff oracle on the raw 8-byte struct
// would show a "divergence" on every single call even when both sides did
// the right thing. Comparing by TEXT is what proves anything, the same shape
// as Filename::GetPathFilename's oracle in runtime/src/system/filename.cpp.
// ============================================================================

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <new>        // placement new into the retail hidden return slot
#include <utility>    // std::move -- the pointer steal out of a POD out-param

#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/pod_bridge.h"
#include "../../types/io/FileSystem.h"   // ENDGAME B.3.4a closing: the member
#include "../../types/system/Error.h"
#include "../../types/system/WideString.h"

// ENDGAME B.3.1 step 2: three of the four Wave-2 lane-A entry points this file
// forward-declared here are gone from this translation unit.
// `RtSystem_String_Format` (0x1800c9730) is `System::String::Format`, and the
// `RtSystem_Filename_AdjustDelimiters` (0x1800c6970) / `RtSystem_Filename_Dtor`
// (0x180006980) pair were the tail of a mutating Filename entry point and of
// ~Filename, which the class spells as itself. Only the WideString release is
// still by hand, because System::WideString is not a B.3.1 class.
namespace RtSystem {
void    RtString_ReleaseInline(String* self);
}  // namespace RtSystem

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)

// WideString is not modelled as its own type (out of this lane's 7-symbol
// scope): every use here goes through System::WideString by cast, and the only fact
// this file relies on is the single-pointer, String-shaped release idiom
// documented above — RtSystem::String already models exactly that.
using WideStringHandle = String;

bool SameFilenameText(const Filename* a, const Filename* b) {
    if ((a->data == nullptr) != (b->data == nullptr)) return false;
    if (a->data == nullptr) return true;
    return std::strcmp(a->data, b->data) == 0;
}

// The body, independent of the `this` FileSystem pointer (see header note).
//
// WAVE 9 LANE V2, 2026-09-07: all three are RT_EXPORT_ONCE, not
// the raw DLL door. The observation that made them the raw DLL door is still
// true and is the reason they are not bare the raw DLL door — these run on the NORMAL
// path and the raw DLL door is GetProcAddress on every call (WAVE3_LANE_P.md, the
// Wave 2 perf-gate defect) — but the DOOR was wrong: they are the work this
// function does, not an oracle, so they are REAL DEPENDENCIES and belong on the
// port registry. the raw export door returns NULL with the DLL absent, which makes
// each of these a null call at cutover rather than a named unresolved import.
// None of the three is ported today, so this changes nothing about which body
// runs; it changes what happens when one of them IS ported, and what the
// cutover failure looks like if none ever is.
// ENDGAME B.3.4a closing: the body moved verbatim into
// `IO::FileSystem::GetSaveDirectory()` at the bottom of this file.
//
// `ComputeSaveDirectory(Filename*)`'s two placement-news become the member's
// two `return` statements. That is the SAME construction into the SAME slot:
// a by-value return with a hidden return slot constructs in place. The early
// return on `hr != S_OK` is retail's `*(longlong*)in_RDX = 0`, i.e. the default
// ctor, and stays the first arm.

}  // namespace RtIO

// ============================================================================
// The member. Defined outside `namespace RtIO` because a member of `IO::X` must
// appear in a namespace ENCLOSING the class (B34_BRIEFS §A.2); it reaches this
// file's RtIO-scoped helpers as `RtIO::X`. Every POD is spelled `RtIO::` /
// `RtSystem::` and every real class `System::` (B34_RULES §7.2).
// ============================================================================
namespace IO {

System::Filename FileSystem::GetSaveDirectory() const {
    // Retail's `this` is LOADED and never dereferenced — see the file banner,
    // which read that off the disassembly rather than the decompile. The member
    // therefore touches no field, and that is a fact about retail, not a gap.

    wchar_t buf[264];
    const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, buf);
    if (hr != S_OK) {
        System::Error::SetText(
            "FileSystem: Unable to retrive the Documents folder for saving!", true);
        // Retail's `*(longlong*)in_RDX = 0` is the default ctor (0x180006220),
        // not a store through a live object.
        return System::Filename();
    }

    RtIO::WideStringHandle ws{nullptr};
    reinterpret_cast<System::WideString*>(&ws)->Set(buf);          // B.3.x bridge

    // AsUTF8's hidden return slot is still POD-shaped (System::WideString is
    // not a B.3.1 class), so the value lands in a POD and is then STOLEN into
    // the class: the move ctor is a pure pointer steal, so `utf8Pod` is left
    // null and exactly one release happens, `utf8`'s destructor.
    RtSystem::String utf8Pod{nullptr};
    new (&RtSystem::Sys(utf8Pod)) System::String(                 // B.3.x bridge
        reinterpret_cast<const System::WideString*>(&ws)->AsUTF8());
    System::String utf8(std::move(RtSystem::Sys(utf8Pod)));

    System::String formatted = System::String::Format(
        "%s\\%s\\", utf8._data ? utf8._data : "", "Banished");

    // AddRef-then-AdjustDelimiters IS `??0Filename@System@@QEAA@AEBVString@1@@Z`
    // (0x18000c780): retail stores the formatted buffer's own pointer into the
    // return slot, bumps its refcount and normalises
    // (0x1800acfd0: `*(longlong*)in_RDX = lVar2; *(int*)(lVar2-4) += 1;`).
    System::Filename result(formatted);

    // ~formatted and ~utf8 are retail's last two releases; only the WideString,
    // whose class does not exist yet, is still released by hand. It is released
    // BEFORE the return for the same reason retail releases it before its
    // epilogue: the order of the three releases is retail's, not ours.
    RtSystem::RtString_ReleaseInline(&ws);

    return result;
}

}  // namespace IO
