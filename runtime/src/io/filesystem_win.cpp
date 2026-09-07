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
// and System::Error. Both are exported; both are called via RtOriginal
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
// and theirs never share a pointer — RT_DIFF_BYTES on the raw 8-byte struct
// would show a "divergence" on every single call even when both sides did
// the right thing. Comparing by TEXT is what proves anything, the same shape
// as Filename::GetPathFilename's oracle in runtime/src/system/filename.cpp.
// ============================================================================

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>

#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../funcs/rt_port.h"

// Wave 2 lane A entry points this file reuses (runtime/src/system/), declared
// locally rather than by editing that lane's string.h/Filename.h, which this
// lane does not own. RtSystem_String_Format is not exposed by string.h today
// (no existing caller outside string.cpp needed it); every other symbol here
// mirrors an existing declaration in runtime/src/system/string.h.
namespace RtSystem {
String* RtSystem_String_Format(String* ret, const char* fmt, ...);   // 0x1800c9730
void    RtString_ReleaseInline(String* self);
void    RtSystem_Filename_AdjustDelimiters(Filename* self);
void    RtSystem_Filename_Dtor(Filename* self);
}  // namespace RtSystem

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::StringRefCount;

// WideString is not modelled as its own type (out of this lane's 7-symbol
// scope): every use here is through a resolved export, and the only fact
// this file relies on is the single-pointer, String-shaped release idiom
// documented above — RtSystem::String already models exactly that.
using WideStringHandle = String;

typedef void     (*fn_WideStringSet_t)(WideStringHandle*, const wchar_t*);
typedef void*    (*fn_WideStringAsUTF8_t)(const WideStringHandle*, void* retval);
typedef void     (*fn_ErrorSetText_t)(const char*, bool);
typedef void*    (*fn_GetSaveDirectory_t)(const void* fs, void* retval);

const char* const kNameGetSaveDirectory =
    "?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ";
const char* const kNameWideStringSet =
    "?Set@WideString@System@@QEAAXPEB_W@Z";
const char* const kNameWideStringAsUTF8 =
    "?AsUTF8@WideString@System@@QEBA?AVString@2@XZ";
const char* const kNameErrorSetText =
    "?SetText@Error@System@@SAXPEBD_N@Z";

bool SameFilenameText(const Filename* a, const Filename* b) {
    if ((a->data == nullptr) != (b->data == nullptr)) return false;
    if (a->data == nullptr) return true;
    return std::strcmp(a->data, b->data) == 0;
}

// The body, independent of the `this` FileSystem pointer (see header note).
// Every RtOriginal here is RT_ORIGINAL_ONCE: these run on the NORMAL path, not
// inside a diff guard, and bare RtOriginal is GetProcAddress on every call
// (WAVE3_LANE_P.md, the Wave 2 perf-gate defect).
void ComputeSaveDirectory(Filename* ret) {
    wchar_t buf[264];
    const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, buf);
    if (hr != S_OK) {
        RT_ORIGINAL_ONCE(fn_ErrorSetText_t, kNameErrorSetText)(
            "FileSystem: Unable to retrive the Documents folder for saving!", true);
        ret->data = nullptr;
        return;
    }

    WideStringHandle ws{nullptr};
    RT_ORIGINAL_ONCE(fn_WideStringSet_t, kNameWideStringSet)(&ws, buf);

    String utf8{nullptr};
    RT_ORIGINAL_ONCE(fn_WideStringAsUTF8_t, kNameWideStringAsUTF8)(&ws, &utf8);

    String formatted{nullptr};
    RtSystem::RtSystem_String_Format(&formatted, "%s\\%s\\",
                                     utf8.data ? utf8.data : "", "Banished");

    // AddRef idiom, not a copy ctor: retail stores the formatted buffer's own
    // pointer directly into the return slot and bumps its refcount
    // (0x1800acfd0: `*(longlong*)in_RDX = lVar2; *(int*)(lVar2-4) += 1;`).
    ret->data = nullptr;
    if (formatted.data != nullptr) {
        ret->data = formatted.data;
        ++StringRefCount(formatted.data);
    }
    RtSystem::RtSystem_Filename_AdjustDelimiters(ret);

    RtSystem::RtString_ReleaseInline(&formatted);
    RtSystem::RtString_ReleaseInline(&utf8);
    RtSystem::RtString_ReleaseInline(&ws);
}

Filename* RtIo_FileSystem_GetSaveDirectory(const void* fsVoid, Filename* ret) {
    RT_DIFF_BEGIN("IO::FileSystem::GetSaveDirectory");
    (void)fsVoid;   // retail's `this` is loaded but never dereferenced

    ComputeSaveDirectory(ret);

    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<fn_GetSaveDirectory_t>(kNameGetSaveDirectory)(fsVoid, &theirs);
        char a[280], b[280];
        std::snprintf(a, sizeof(a), "%s", ret->data ? ret->data : "(null)");
        std::snprintf(b, sizeof(b), "%s", theirs.data ? theirs.data : "(null)");
        RtDiffReport(&rt_diff_site__, SameFilenameText(ret, &theirs), a, b);
        RtSystem::RtSystem_Filename_Dtor(&theirs);
    }
    return ret;
}

}  // namespace RtIO

// ============================================================================
// Registration — called from filesystem.cpp's RtIoFileSystemRegister(), which
// owns the switch/printf line for the whole lane.
// ============================================================================
int RtIoFileSystemWinRegister() {
    using namespace RtIO;
    RegisterRuntimeExport(kNameGetSaveDirectory, (void*)&RtIo_FileSystem_GetSaveDirectory);
    RegisterRuntimeAddr(0x1800acfd0, (void*)&RtIo_FileSystem_GetSaveDirectory);
    return 1;
}
