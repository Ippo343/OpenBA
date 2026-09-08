// ============================================================================
// runtime/src/io/filesystem.cpp — Wave 4 lane D: IO::FileSystem + Modification
// ============================================================================
// Byte-faithful ports of the 6 FileSystem/Modification exports our tree calls
// (docs/runtime_port/call_surface_20260906.csv) that do not need Win32.
// GetSaveDirectory (SHGetFolderPathW) lives in filesystem_win.cpp, per
// PLAN.md §4.1 decision 1's platform seam — <windows.h> must never appear
// here. runtime/types/io/FileSystem.h and Modification.h carry the layouts;
// read them first.
//
//   ?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z          retail 0x1800ab310
//   ?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ   retail 0x180007bd0
//   ?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ retail 0x180007b90
//   ?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z    retail 0x1800ab050
//   ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z           retail 0x1800aea10
//
// (GetSaveDirectory: ?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ
//  retail 0x1800acfd0, in filesystem_win.cpp.)
//
// All six names were checked against the shipped DLL's export directory with
// pefile BEFORE any code was written here (WAVE4_LANE_D.md §2) — none is a
// reference_resolveexport_phantom_names.md case.
//
// HIDDEN RETURN SLOTS. GetDataDirectory, GetSourceDirectory and
// GetSaveDirectory all return `System::Filename` BY VALUE (`?AV` in the
// mangling), so each has an RDX hidden-return-slot argument. Checked against
// both the existing central engine_funcs.h typedefs (`fn_FileSystem_Get*Directory_t`,
// already `(self, retval)` — correct) and the local one in
// code/Game/Game/w2_takescreenshot.cpp (`FS_GetSaveDirectory_t`, also correct).
// No wild-write defect exists in this family. See WAVE4_LANE_D.md §2 for the
// tools/abi_retslot_audit.py run.
//
// STATE. All five functions here are read-only walks of the LIVE DLL
// structures at their real addresses (the FileSystem/Modification objects
// keep existing on the DLL's heap; nothing here replicates them). No
// detour is installed — registering redirects our call sites only, which is
// safe for a read-only walk exactly as it was for Wave 3's Object reflection
// lane (WAVE3_LANE_A.md §6).
//
// NO ADDED NULL GUARDS. Exists, GetPackfileId and Find all deref fields
// unconditionally exactly as retail's disassembly does; see each function's
// comment for the specific instruction.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../types/io/FileSystem.h"
#include "../../types/io/Modification.h"
#include "../../funcs/rt_port.h"
#include "../system/string.h"

// Filename entry points this lane reuses from Wave 2 lane A
// (runtime/src/system/filename.cpp). They are ordinary (non-static) free
// functions in namespace RtSystem, declared here rather than by editing that
// lane's file or its string.h, which this lane does not own.
namespace RtSystem {
struct Filename;
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);
void      RtSystem_Filename_Dtor(Filename* self);
void      RtSystem_Filename_AdjustDelimiters(Filename* self);
}  // namespace RtSystem

extern int RtIoFileSystemWinRegister();   // filesystem_win.cpp — GetSaveDirectory

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::StringLength;
using RtSystem::RtString_ReleaseInline;

// ---- Raw ABI typedefs for the originals -----------------------------------
typedef bool     (*fn_Exists_t)(const void* fs, const void* filename);
typedef void*    (*fn_GetDirectory_t)(const void* fs, void* retval);
typedef uint32_t (*fn_GetPackfileId_t)(const void* fs, const void* filename);
typedef int      (*fn_ModFind_t)(const void* mod, const void* filename);

const char* const kNameExists =
    "?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z";
const char* const kNameGetDataDirectory =
    "?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ";
const char* const kNameGetSourceDirectory =
    "?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ";
const char* const kNameGetPackfileId =
    "?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z";
const char* const kNameModFind =
    "?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z";

// Case-insensitive ASCII compare, sign-preserving (needed by GetPackfileId's
// tree descent, not just equality). Duplicated rather than shared from
// filename.cpp's file-local StriCmpAscii for the same reason that one gives:
// runtime/src stays free of a CRT `_stricmp` whose locale behaviour differs
// between toolchains (PLAN.md §4.1 portability seam).
static int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb) return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0) return 0;
    }
}

// ============================================================================
// ?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z   retail 0x1800ab310
//
//     bool FileSystem::Exists(const Filename& path) const
//     {
//         if (this->existsInPackfileGate && ExistsInPackfile(path)) return true;
//         if (!this->platformExistsGate) return false;
//         return PlatformExists(path);
//     }
//
// ExistsInPackfile (0x1800ab180) and PlatformExists (0x1800adc70) are exported
// but NOT in this lane's scope (our tree never calls either directly — only
// through Exists), so they are called by resolved export, exactly as Wave 3's
// Type::Serialize dispatched into the DLL's (also out-of-scope) ReaderWriter
// vtable. That is a real transcription of Exists's own logic, not a
// forwarder: the two gate reads and the short-circuit order are ours.
//
// DIFF: full oracle. Read-only on both sides (PlatformExists stats a file,
// no mutation).
// ============================================================================
bool RtIo_FileSystem_Exists(const void* fsVoid, const Filename* path) {
    RT_DIFF_BEGIN("IO::FileSystem::Exists");
    const FileSystem* fs = static_cast<const FileSystem*>(fsVoid);

    // RT_EXPORT_ONCE, not bare RtOriginal and not RT_ORIGINAL_ONCE.
    //
    // The _ONCE half is the original reason and still holds: these two calls
    // are on the NORMAL path, not inside a diff guard, and bare RtOriginal is
    // GetProcAddress -- an export-table binary search by string under the
    // loader lock on EVERY call. Two of them in the body of a function the
    // quicksave path calls per file is the Wave 2 perf-gate defect exactly
    // (WAVE3_LANE_P.md): it cost 5.6 points of sim time from inside
    // System::Time and was invisible to both profilers because it lands in
    // ntdll self time.
    //
    // WAVE 9 LANE V2, 2026-09-07 changed only the DOOR. These two calls ARE
    // Exists's logic, so they are a REAL DEPENDENCY, and the raw door resolves
    // to NULL with Runtime-x64.dll absent. Note rawdoor_audit did NOT report
    // these: its resolve-and-call regex is per-line and here the argument list
    // opens on the following line. Found by a paren-matching re-scan, not by
    // the checker.
    //
    // The diff line below stays on RtOriginal deliberately -- it is the oracle,
    // it must reach the SHIPPED body, and it only evaluates under the guard.
    bool ours;
    if (fs->existsInPackfileGate != 0 &&
        RT_EXPORT_ONCE(fn_Exists_t,
            "?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z")(fs, path)) {
        ours = true;
    } else if (fs->platformExistsGate == 0) {
        ours = false;
    } else {
        ours = RT_EXPORT_ONCE(fn_Exists_t,
            "?PlatformExists@FileSystem@IO@@AEBA_NAEBVFilename@System@@@Z")(fs, path);
    }

    RT_DIFF_SCALAR(ours, RtOriginal<fn_Exists_t>(kNameExists)(fs, path));
    return ours;
}

// ============================================================================
// ?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ  retail 0x180007bd0
//
//     Filename FileSystem::GetDataDirectory() const
//     { return Filename(this->dataDirectory); }    // copy ctor, AddRef only
//
// Disassembly (0x180007bd0..0x180007c0b) is a single tail-call:
//   LEA RDX,[RCX+0x8]; MOV RCX,RBX(=hidden retval); CALL 0x1800067d0
// 0x1800067d0 is `??0Filename@System@@QEAA@AEBV01@@Z`, Wave 2's
// RtSystem_Filename_CtorCopy — confirmed by disassembling both this function
// and GetSourceDirectory (same CALL target) rather than assumed from the
// pseudocode's `Filename::Filename(...)` name.
//
// DIFF: full oracle. The copy ctor only AddRefs a shared buffer, so running
// both sides is safe as long as each result is released afterward — comparing
// the resulting Filename BY VALUE necessarily has ours->data == theirs->data
// (same shared string), which is exactly what this proves: our AddRef landed
// on the DLL's own live dataDirectory buffer, not a stale or wrong one.
// ============================================================================
Filename* RtIo_FileSystem_GetDataDirectory(const void* fsVoid, Filename* ret) {
    RT_DIFF_BEGIN("IO::FileSystem::GetDataDirectory");
    const FileSystem* fs = static_cast<const FileSystem*>(fsVoid);

    RtSystem::RtSystem_Filename_CtorCopy(ret, reinterpret_cast<const Filename*>(&fs->dataDirectory));

    if (rt_diff_on__) {
        Filename theirs{};
        RtOriginal<fn_GetDirectory_t>(kNameGetDataDirectory)(fs, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Filename));
        RtSystem::RtSystem_Filename_Dtor(&theirs);
    }
    return ret;
}

// ============================================================================
// ?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ retail 0x180007b90
//
//     Filename FileSystem::GetSourceDirectory() const
//     { return Filename(*this); }   // reinterprets `this` itself as the Filename
//
// Disassembly (0x180007b90..0x180007bca): `MOV RDX,RCX(=this); MOV RCX,RBX
// (=hidden retval); CALL 0x1800067d0` — the SAME copy ctor GetDataDirectory
// uses, called on `this` rather than `this+8`. That is only possible because
// FileSystem+0x00 IS a String/Filename object (see FileSystem.h); "source
// directory" is the class's own first field, not a nested lookup.
//
// DIFF: full oracle, same reasoning as GetDataDirectory.
// ============================================================================
Filename* RtIo_FileSystem_GetSourceDirectory(const void* fsVoid, Filename* ret) {
    RT_DIFF_BEGIN("IO::FileSystem::GetSourceDirectory");
    const FileSystem* fs = static_cast<const FileSystem*>(fsVoid);

    RtSystem::RtSystem_Filename_CtorCopy(ret, reinterpret_cast<const Filename*>(&fs->sourceDirectory));

    if (rt_diff_on__) {
        Filename theirs{};
        RtOriginal<fn_GetDirectory_t>(kNameGetSourceDirectory)(fs, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Filename));
        RtSystem::RtSystem_Filename_Dtor(&theirs);
    }
    return ret;
}

// ============================================================================
// ?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z    retail 0x1800ab050
//
//     unsigned int FileSystem::GetPackfileId(const Filename& path) const
//
// Only proceeds when the path is longer than dataDirectory (the length check
// substitutes for an actual prefix compare — retail trusts the caller passes
// a full path under dataDirectory, and strips the prefix BY LENGTH alone).
// Builds the relative substring, normalises it, then walks the
// PackfileIndexNode tree at `this+0x40` with a case-insensitive compare,
// descending via the pattern documented at the node struct: effectively "if
// relPath >= key, check relPath == key or go right; else go left". On a hit,
// returns `packfiles[node.packIndex]->packfileId`; on a miss, or if the
// length check fails, returns 0.
//
// NO ADDED NULL GUARDS: retail's own length check (`iVar4 < iVar3`) is the
// only guard here, transcribed exactly; a match failure returns 0, which is
// retail's own "in no package" sentinel — not an added defensive path.
//
// DIFF: full oracle. Read-only on both sides.
// ============================================================================
uint32_t RtIo_FileSystem_GetPackfileId(const void* fsVoid, const Filename* path) {
    RT_DIFF_BEGIN("IO::FileSystem::GetPackfileId");
    const FileSystem* fs = static_cast<const FileSystem*>(fsVoid);

    uint32_t ours = 0;

    const char* pathData = path->data;
    const int pathLen = pathData ? StringLength(pathData) : 0;
    const char* dataDirData = fs->dataDirectory.data;
    const int dataDirLen = dataDirData ? StringLength(dataDirData) : 0;

    if (dataDirLen < pathLen) {
        // Relative path = pathData + dataDirLen (a suffix of the SAME buffer,
        // not a copy yet — String::Set below is what allocates the copy).
        String relPath{nullptr};
        RtSystem::RtSystem_String_Set(&relPath, pathData + dataDirLen);
        RtSystem::RtSystem_Filename_AdjustDelimiters(reinterpret_cast<Filename*>(&relPath));

        const char* str1 = relPath.data;
        PackfileIndexNode* node = fs->packfileIndexRoot;
        PackfileIndexNode* found = nullptr;
        while (node) {
            const char* key = node->key;
            bool condA;
            if (!key) condA = true;
            else if (!str1) condA = false;
            else condA = StriCmpAscii(str1, key) >= 0;

            if (condA) {
                bool condB;
                if (!str1) condB = true;
                else if (!key) condB = false;
                else condB = StriCmpAscii(key, str1) >= 0;

                if (condB) { found = node; break; }
                node = node->right;
            } else {
                node = node->left;
            }
        }

        if (found)
            ours = fs->packfiles[found->packIndex]->packfileId;

        RtString_ReleaseInline(&relPath);
    }

    RT_DIFF_SCALAR(ours, RtOriginal<fn_GetPackfileId_t>(kNameGetPackfileId)(fs, path));
    return ours;
}

// ============================================================================
// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z   retail 0x1800aea10
//
//     int Modification::Find(const Filename& path) const
//     { for each Mod, case-insensitively compare its file to path; return
//       the index of the first match or -1. }
//
// Length-prefixed compare identical in shape to GetPackfileId's: compare
// stored lengths first, then StriCmp only if they match.
//
// DIFF: full oracle. Read-only on both sides.
// ============================================================================
int RtIo_Modification_Find(const void* modVoid, const Filename* path) {
    RT_DIFF_BEGIN("IO::Modification::Find");
    const Modification* mod = static_cast<const Modification*>(modVoid);

    int ours = -1;
    const char* pathData = path->data;
    const int pathLen = pathData ? StringLength(pathData) : 0;

    for (int i = 0; i < mod->count; ++i) {
        const char* modData = mod->mods[i].file.data;
        const int modLen = modData ? StringLength(modData) : 0;
        if (modLen == pathLen && StriCmpAscii(modData, pathData) == 0) {
            ours = i;
            break;
        }
    }

    RT_DIFF_SCALAR(ours, RtOriginal<fn_ModFind_t>(kNameModFind)(mod, path));
    return ours;
}

}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
// `?_singleton@FileSystem@IO@@0PEAV12@EA` (0x1801fc6a8) is a DATA symbol, as
// the brief flagged. Confirmed: pefile lists it in the export table with no
// associated code, exactly like Wave 3's Object::Factory::_singleton
// (WAVE3_LANE_A.md §7/§10). Wave 3's reasoning holds unchanged here: a data
// symbol is a variable's ADDRESS, not a callable, so there is nothing for
// RegisterRuntimeExport/RegisterRuntimeAddr to redirect — every one of our
// ~20 existing call sites that dereferences it (g_pFileSystemSingleton,
// g_pFileSystemSingleton2, code/Game/Game/filesystem_setup.cpp,
// code/Game/Dialog/mod_dialog_vtables.cpp) keeps reading the DLL's live
// FileSystem object by address, which is correct and required until
// FileSystem's OWNER (construction/destruction) is ported. It is NOT
// registered here, and should not be.
//
// `?_singleton@Modification@IO@@2PEAV12@EA` (0x1801fc6b0) is likewise data —
// note the mangling digit is `2` (protected static), not `0` like
// FileSystem's (private static); the brief's suggested `1PEAV12@A` form is
// not an export of either. Out of this lane's 7-symbol scope regardless.
// ============================================================================
int RtIoFileSystemRegister() {
    using namespace RtIO;
    int n = 0;

    // The mangled names are LITERALS here, not the kName* constants above, and
    // that is load-bearing rather than style: tools/runtime_census.py matches
    // `RegisterRuntimeExport("<literal>"` and CANNOT see through a named
    // constant. Registering through the constants made all six of this lane's
    // ports invisible to the census -- it reported IO 30/41 while 36 were
    // actually registered. The census is this project's authority over any
    // agent's "all N ported" claim, so a registration it cannot see is worse
    // than no registration. It also keeps a human grep for the mangled name
    // landing on the registration site. The census now WARNS on this pattern.
    RegisterRuntimeExport("?Exists@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileSystem_Exists); ++n;
    RegisterRuntimeExport("?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtIo_FileSystem_GetDataDirectory); ++n;
    RegisterRuntimeExport("?GetSourceDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtIo_FileSystem_GetSourceDirectory); ++n;
    RegisterRuntimeExport("?GetPackfileId@FileSystem@IO@@QEBAIAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileSystem_GetPackfileId); ++n;
    RegisterRuntimeExport("?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z",
                          (void*)&RtIo_Modification_Find); ++n;

    RegisterRuntimeAddr(0x1800ab310, (void*)&RtIo_FileSystem_Exists);
    RegisterRuntimeAddr(0x180007bd0, (void*)&RtIo_FileSystem_GetDataDirectory);
    RegisterRuntimeAddr(0x180007b90, (void*)&RtIo_FileSystem_GetSourceDirectory);
    RegisterRuntimeAddr(0x1800ab050, (void*)&RtIo_FileSystem_GetPackfileId);
    RegisterRuntimeAddr(0x1800aea10, (void*)&RtIo_Modification_Find);

    n += RtIoFileSystemWinRegister();   // GetSaveDirectory, filesystem_win.cpp

    printf("[RtPort]   IO::FileSystem/Modification: %d exports registered\n", n);
    return n;
}
