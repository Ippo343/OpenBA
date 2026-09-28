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
#include <new>          // placement new into the retail hidden return slot

#include "../../types/io/FileSystem.h"
#include "../../types/io/Modification.h"
#include "../../funcs/rt_port.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::StringLength;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)
using RtSystem::SysFile;  //   "

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
// The body is the member `IO::FileSystem::Exists`, at the bottom of this file.

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
// ENDGAME B.3.4 PACKAGE+FS commit 4: the body is now a member of the real
// `IO::FileSystem`, at the bottom of this file. The door keeps its name and
// both registrations and delegates. No qualified `IO::` call is needed:
// `IO::FileSystem` has no vtable at all (runtime/types/io/FileSystem.h), so
// B34_BRIEFS section A.1's re-entry hazard cannot arise.
//
// The return is BY VALUE, so `ret` IS retail's hidden return slot (RDX); the
// member's by-value return fills it through the same ABI, so the door
// constructs the member's result into it -- one AddRef, exactly as before.
Filename* RtIo_FileSystem_GetDataDirectory(const void* fsVoid, Filename* ret) {
    new (ret) System::Filename(
        reinterpret_cast<const IO::FileSystem*>(fsVoid)->GetDataDirectory());
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
// The body is the member `IO::FileSystem::GetSourceDirectory`, at the bottom of
// this file.

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
// The body is the member `IO::FileSystem::GetPackfileId`, at the bottom of this
// file.

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
// The body is the member `IO::Modification::Find(const RtSystem::Filename&)`,
// below.

}  // namespace RtIO

// ============================================================================
// `IO::Modification::Find` — the member body.
// ============================================================================
// Defined outside `namespace RtIO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS §A.2), and reaching this file's
// `RtIO`-scoped helpers as `RtIO::X` is what lets it stay where it is rather
// than moving to modification.cpp. It lives in filesystem.cpp because this is
// where the door and its registration have always been.
namespace IO {

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — `IO::FileSystem`'s member bodies (PARTIAL)
// ============================================================================
// Only the four members whose doors live in THIS file. The other fifteen are
// still free functions in the eight sibling `filesystem*.cpp` files, nine of
// them on the OPEN/READ path B34_BRIEFS section D binds; runtime/types/io/
// FileSystem.h's banner lists exactly which and where.
//
// Defined outside `namespace RtIO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); this block reaches
// this file's `RtIO`-scoped helpers as `RtIO::X`, which works for its `static`
// members too in this same translation unit. Every POD is spelled `RtIO::` or
// `RtSystem::` and every real class `System::` (B34_RULES section 7.2).
//
// NO ADDED NULL GUARDS ANYWHERE BELOW. This file's own banner already records
// that Exists, GetPackfileId and Find deref fields unguarded because retail
// does; the move changes nothing about that.

bool FileSystem::Exists(const RtSystem::Filename& pathRef) const {
    const RtSystem::Filename* const path = &pathRef;

    // B3R RT-IO: the two arms call ExistsInPackfile / PlatformExists as
    // members (filesystem_find_win.cpp); they were RT_EXPORT_ONCE doors.
    bool ours;
    if (_existsInPackfileGate != 0 &&
        ExistsInPackfile(*path)) {
        ours = true;
    } else if (_platformExistsGate == 0) {
        ours = false;
    } else {
        ours = PlatformExists(*path);
    }

    return ours;
}

System::Filename FileSystem::GetDataDirectory() const {

    // ENDGAME B.3.1 step 2: retail's copy ctor, spelled as itself. `ret` is the
    // hidden return slot — raw, uninitialised bytes — so the object is placement-
    // constructed into it, which is exactly what the CALL at 0x180007c06 does.
    // `dataDirectory` is a `RtSystem::String` member of the not-yet-converted
    // `IO::FileSystem`, so it comes through the one transitional bridge.
    return System::Filename(RtIO::SysFile(_dataDirectory));

}

System::Filename FileSystem::GetSourceDirectory() const {

    // ENDGAME B.3.1 step 2, as GetDataDirectory above. `sourceDirectory` is
    // FileSystem+0x00, which is why retail's `MOV RDX,RCX` passes `this` itself.
    return System::Filename(RtIO::SysFile(_sourceDirectory));

}

uint32_t FileSystem::GetPackfileId(const RtSystem::Filename& pathRef) const {
    const RtSystem::Filename* const path = &pathRef;

    uint32_t ours = 0;

    const char* pathData = path->data;
    const int pathLen = pathData ? RtIO::StringLength(pathData) : 0;
    const char* dataDirData = _dataDirectory.data;
    const int dataDirLen = dataDirData ? RtIO::StringLength(dataDirData) : 0;

    if (dataDirLen < pathLen) {
        // Relative path = pathData + dataDirLen (a suffix of the SAME buffer,
        // not a copy yet — String::Set below is what allocates the copy).
        // ENDGAME B.3.1 step 2: `Filename relPath(pathData + dataDirLen)` is
        // exactly retail's Set-then-AdjustDelimiters pair (the Filename
        // const char* ctor, 0x180026cf0). RAII replaces the manual release at
        // the bottom of the block.
        System::Filename relPath(pathData + dataDirLen);

        const char* str1 = relPath._data;
        RtIO::PackfileIndexNode* node = _packfileIndexRoot;
        RtIO::PackfileIndexNode* found = nullptr;
        while (node) {
            const char* key = node->key;
            bool condA;
            if (!key) condA = true;
            else if (!str1) condA = false;
            else condA = RtIO::StriCmpAscii(str1, key) >= 0;

            if (condA) {
                bool condB;
                if (!str1) condB = true;
                else if (!key) condB = false;
                else condB = RtIO::StriCmpAscii(key, str1) >= 0;

                if (condB) { found = node; break; }
                node = node->right;
            } else {
                node = node->left;
            }
        }

        if (found)
            ours = _packfiles[found->packIndex]->packfileId;

        // (relPath's release is ~Filename now — retail's own 0x1800ab0f2.)
    }

    return ours;
}


// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z       retail 0x1800aea10
//
//     int Modification::Find(const Filename& path) const
//     { for each Mod, case-insensitively compare its file to path; return
//       the index of the first match or -1. }
//
// Length-prefixed compare identical in shape to GetPackfileId's: compare
// stored lengths first, then StriCmp only if they match. NO ADDED NULL GUARD
// on `_mods` — this file's own banner records that Exists, GetPackfileId and
// Find all deref fields unguarded because retail does.
int Modification::Find(const RtSystem::Filename& path) const {
    int ours = -1;
    const char* pathData = path.data;
    const int pathLen = pathData ? RtIO::StringLength(pathData) : 0;

    for (int i = 0; i < _count; ++i) {
        const char* modData = _mods[i].file.data;
        const int modLen = modData ? RtIO::StringLength(modData) : 0;
        if (modLen == pathLen && RtIO::StriCmpAscii(modData, pathData) == 0) {
            ours = i;
            break;
        }
    }

    return ours;
}

// ?_singleton@FileSystem@IO@@0PEAV12@EA (0x1801fc6a8) -- the class static
// (ENDGAME B.3.9 IO, D7); RtIoPortInit redirects the minted row onto it.
FileSystem* FileSystem::_singleton = nullptr;

}  // namespace IO

