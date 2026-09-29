// ============================================================================
// runtime/src/io/filesystem_openfile_win.cpp — Wave 9 lane PKG2, rung 3
// IO::FileSystem::OpenFile / ReadFile / PlatformOpenFile / CloseFile helpers
// ============================================================================
//
//   | Export                                                | Retail      | Ord  | Instrs |
//   |-------------------------------------------------------|-------------|------|--------|
//   | ?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBV…@Z   | 0x1800ac880 | 3309 |  264   |
//   | ?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z| 0x1800ace00 | 3460 |   90   |
//   | ?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAU…@Z        | 0x1800adce0 | 3384 |  101   |
//   | ?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@…@XZ     | 0x180025400 | 2601 |   15   |
//
// plus the unexported FileInfo copy constructor at `0x18000c110` (29 instrs),
// which `GetFileInfo` is a three-line wrapper around.
//
// Names and ordinals printed out of Runtime-x64.dll's export directory with
// `pefile`. `tools/export_name_audit.py` re-checks them.
//
//
// **THIS FILE IS `_win` BECAUSE IT GENUINELY IS** (PLAN §4.1): the bodies below
// call CreateFileW, ReadFile, SetFilePointer and GetFileSize. All four were
// resolved out of the RUNTIME DLL's own import table with `pefile`, not guessed
// from the call shape — `tools/dispatch_slot_map.py` is for the EXE's imports
// and reports these as "NOT AN IMPORT SLOT":
//
//     0x18014b320 CreateFileW   0x18014b110 ReadFile
//     0x18014b0f0 SetFilePointer 0x18014b0f8 GetFileSize
//
// ---------------------------------------------------------------------------
// 1. WHAT `FileInfo::handle` ACTUALLY IS — the fact the whole rung turns on
// ---------------------------------------------------------------------------
// It is not a handle. It is a discriminated tag:
//
//     -1   nothing opened
//     -2   a LOOSE file; the real Win32 HANDLE is at +0x18
//     else a PACKED file, encoded `(packIndex << 24) | fileIndex`
//
// OpenFile builds that word with `MOV R8D,[RBX+8] ; SHL R8D,0x18 ; OR R8D,[RBX+0xc]`
// (0x1800ac9c5) straight out of the packfile-index BST node lane PKG's
// LoadPackages fills in. ReadFile takes it apart with `MOVZX R10D,byte [RDX+3]`
// — **the pack index is recovered as the top BYTE**, which only works because
// the shift is 24 and is why the field cannot be modelled as an opaque handle.
//
// The discrimination is UNSIGNED: `CMP dword [RDX],-3 ; JA loose`. -1 and -2 are
// both above 0xFFFFFFFD, so both take the loose path; every real pack encoding
// is below it. A signed test would send every pack whose index is >= 0x80 down
// the wrong path, and there is no such pack today — which is exactly the kind of
// thing that works until it does not.
//
// ---------------------------------------------------------------------------
// 2. OpenFile's SHAPE
// ---------------------------------------------------------------------------
//   key = name->data + strlen(this->dataDirectory)      // STRIP the prefix
//   if (strlen(name) > strlen(dataDirectory)) {
//       String k; k.Set(key); Filename::AdjustDelimiters(&k);
//       node = <BST walk of this->packfileIndexRoot, _stricmp>;
//       release(k);
//       if (node && this->existsInPackfileGate) {
//           lf.handle     = (node->packIndex << 24) | node->fileIndex;
//           e             = &packfiles[node->packIndex]->entries[node->fileIndex];
//           lf.size       = e->size;      // +0xC of the 16-byte entry
//           lf.baseOffset = e->offset;    // +0x8
//           lf.cursor     = 0;
//           goto emit;
//       }
//   }
//   if (!this->platformExistsGate) { *ret = <empty>; goto done; }
//   if (!PlatformOpenFile(this, &lf, name)) { *ret = <empty>; goto done; }
//   lf.handle = -2;
// emit:
//   *ret = lf (field by field), Filename-copy the two names
//
// **THE PACK-ENTRY READS ARE AN INDEPENDENT CONFIRMATION OF RUNG 1'S LAYOUT.**
// `PackageFileEntry` was pinned in iteration 1 from `?GetFileSize@`/`?GetFileOffset@`
// (`+0xC` and `+0x8`, stride 16). OpenFile reaches the same two dwords by a
// completely different route — `[pkg+0xb0] + fileIndex*16`, at `+0xc` and `+0x8`
// (0x1800ac9ee / 0x1800aca0c) — and agrees. Two unrelated functions, one layout.
//
// ---------------------------------------------------------------------------
// 3. ReadFile IS RECURSIVE, and that is the elegant part
// ---------------------------------------------------------------------------
//   if ((uint32)fi->handle <= 0xFFFFFFFD) {          // PACKED
//       FileInfo pack = packfiles[fi->handle >> 24]->GetFileInfo();
//       int abs = fi->baseOffset + fi->cursor;
//       if ((uint32)pack.handle > 0xFFFFFFFD)        // the .pkg is itself loose
//           if (SetFilePointer(pack.winHandle, abs, 0, FILE_BEGIN) != abs) fail;
//       ok = ReadFile(this, &pack, n, dst);          // <-- ITSELF
//       if (ok) fi->cursor += n;
//       ~pack;
//       return ok;
//   }
//   // LOOSE
//   DWORD got = 0;
//   ::ReadFile(fi->winHandle, dst, n, &got, NULL);
//   fi->cursor += got;
//   return got == n;
//
// So a packed read seeks the containing .pkg's handle and then re-enters itself
// with the PACK's FileInfo, which takes the loose branch. One body, both cases.
// A port that flattened the recursion into an if/else would be equivalent today
// and would stop being equivalent the moment a pack lived inside a pack.
//
// ---------------------------------------------------------------------------
// 4. THE UNINITIALISED FIELD, and what it does to the oracle
// ---------------------------------------------------------------------------
// OpenFile's local FileInfo initialises +0x00, +0x08, +0x0C, +0x10, +0x18, +0x20
// and +0x28 — **not +0x04**. The emit block then copies it out anyway:
// `MOV EAX,[RBP-0x2c] ; MOV [RDI+4],EAX` (0x1800acb8e). Retail returns a dword
// of its own stack garbage in every FileInfo it produces.
//
// That is faithful and it is reproduced. It also means **the oracle must not
// compare +0x04** — ours and theirs have different stacks, so it differs on
// every call and would report a 100% divergence
// (reference_oracle_must_not_compare_unwritten_bytes.md). The exclusion is
// stated at the comparison site, not hidden in a memcmp length.
//
// ---------------------------------------------------------------------------
// 5. WHAT IS NOT PORTED
// ---------------------------------------------------------------------------
//   ?CloseFile@FileSystem@IO@@   0x1800accc0, ord 1726 — reached through the
//       proper door. It is the teardown half and lane PKG's ClosePackages
//       already calls it; porting the OPEN half without it is deliberate, and it
//       is named in the registration block rather than silently absent.
//   FUN_18000c200 — FileInfo::operator=, used by PlatformOpenFile to reset the
//       caller's FileInfo. Called by ADDRESS through ResolveAddr, which is the
//       proper door for an unexported internal, and named below.
//   FUN_18007eefc — the CRT `_stricmp` OpenFile's BST walk needs. Same address
//       lane PKG reaches for `IndexFind`; it is one of the four doors the
//       integrator's address-door census flagged, and this rung ADDS a second
//       live caller of it rather than removing the first.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

#include "../../funcs/rt_libm.h"        // rtcrt::StrICmp
#include "../../types/io/FileSystem.h"
#include "../../types/io/PackageFile.h"   // ENDGAME B.3.4a closing: the two members
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../types/system/WideString.h"
#include "../../src/system/string.h"
#include "../../src/system/pod_bridge.h"

#include <new>        // placement new into a raw FileInfo Filename slot

// ENDGAME B.3.1 step 2: this file no longer forward-declares lane String's
// `RtSystem_Filename_AdjustDelimiters` (0x1800c6970). Every call it made was
// the tail of a mutating Filename entry point — the `const char*` ctor
// (0x180026cf0) or `operator=(const String&)` (0x180026d40) — and the class
// spells those as themselves, with AdjustDelimiters private inside them.

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)
using RtSystem::SysFile;  //   "

namespace {

// The CRT `_stricmp` OpenFile's BST walk uses: 0x18007eefc, rtcrt::StrICmp
// (runtime/src/crt/msvc_stricmp.cpp), called by name (B3R RT-IO).
// It must NOT be replaced by std::strcasecmp or shared with
// runtime/src/system/configuration.cpp's comparator: 0x18007eec0
// (`__ascii_stricmp`, the tail it jumps to) lowers only 'A'-'Z' and compares
// UNSIGNED, while the config map inlines a SIGNED-char strcmp. They disagree on
// any byte >= 0x80 and a package path can contain one.
int PathStrICmp(const char* a, const char* b) {
    return rtcrt::StrICmp(a, b);
}

// FileInfo::operator= is now PORTED (rung 4, below) -- it used to be reached
// here by ResolveAddr(0x18000c200), which was rung 3's one new cutover blocker.

inline int32_t StrLenOf(const char* data) {
    // retail: `if (p) len = [p-8] else len = 0` — the String block's length word.
    return data ? RtSystem::StringLength(data) : 0;
}

}  // namespace

// ============================================================================
// FileInfo's copy constructor                                retail 0x18000c110
// ============================================================================
// Five DWORD moves, one QWORD move, two Filename copy ctors — and NOT +0x14,
// which is why the struct carries an explicit `_pad14`. Ported rather than
// called by address because `?GetFileInfo@PackageFile@IO@@` is a three-line
// wrapper around it and porting one without the other would leave the wrapper
// standing on a null at cutover.
extern "C" FileInfo* RtIo_FileInfo_CopyCtor(FileInfo* dst, const FileInfo* src) {

    dst->handle     = src->handle;       // 18000c12e
    dst->_unk04     = src->_unk04;       // 18000c132
    dst->size       = src->size;         // 18000c138
    dst->cursor     = src->cursor;       // 18000c13e
    dst->baseOffset = src->baseOffset;   // 18000c144
    dst->winHandle  = src->winHandle;    // 18000c14a  (+0x14 is NOT copied)
    // `dst` is a raw FileInfo: these two slots are uninitialised bytes, so the
    // copy ctor (??0Filename@System@@QEAA@AEBV01@@Z, 0x1800067d0) is placement-
    // constructed into them, which is what retail's two CALLs do. The members
    // are `RtSystem::String` on the not-yet-converted FileInfo, hence SysFile.
    new (&dst->_name0) System::Filename(SysFile(src->_name0));   // 18000c15a
    new (&dst->_name1) System::Filename(SysFile(src->_name1));   // 18000c168

    // No replay oracle: the Filename copy ctors AddRef, so running the shipped
    // body a second time would leave two extra references on every call. The
    // lockstep probe compares whole FileInfos produced through this path.
    return dst;                          // 18000c16e
}

// ============================================================================
// FileInfo::operator=(const FileInfo&)                       retail 0x18000c200
// ============================================================================
// Rung 3 named this as its ONE new cutover blocker: unexported, unported, and
// called from inside our own PlatformOpenFile, so at cutover ResolveAddr would
// hand back null and our port would die on a null call. 61 instructions.
//
// It also supplies a FOURTH independent proof of the FileInfo layout rung 3
// corrected: the field moves are +0x00, +0x04, +0x08, +0x0C, +0x10 and the
// QWORD at +0x18 -- and +0x14 is skipped, exactly as in the copy ctor. A
// `uint64_t size` at +0x08 cannot produce two separate DWORD moves at +0x08 and
// +0x0C.
//
// The two Filename members are refcounted Strings assigned with the release/
// adopt idiom, and there are three details a paraphrase gets wrong:
//
//   1. SELF-ASSIGNMENT OF THE BUFFER SHORT-CIRCUITS. `CMP RAX,[RDX+0x20]; JZ`
//      jumps past BOTH the release and the retain -- not just the release. Doing
//      release-then-retain on a shared buffer would drop the count to zero and
//      free a string both sides still point at.
//   2. THE ADOPT IS UNCONDITIONAL, THE INCREMENT IS NOT. Retail stores theirs
//      into our slot whatever it is, and only then tests it for the `INC`. This
//      is why RtString_AdoptInline is NOT used here: it skips the store when the
//      source is null, which would leave our slot holding the DANGLING `data - 8`
//      that the freeing branch of the release deliberately leaves behind.
//   3. AdjustDelimiters RUNS ON BOTH PATHS. `LEA RCX,[RSI+0x20]; CALL` sits at
//      the merge point that the self-assignment JZ targets, so it runs even when
//      nothing was copied.
extern "C" FileInfo* RtIo_FileInfo_Assign(FileInfo* dst, const FileInfo* src) {

    dst->handle     = src->handle;       // 18000c217
    dst->_unk04     = src->_unk04;       // 18000c21c
    dst->size       = src->size;         // 18000c222
    dst->cursor     = src->cursor;       // 18000c228
    dst->baseOffset = src->baseOffset;   // 18000c22e
    dst->winHandle  = src->winHandle;    // 18000c235  (+0x14 is NOT copied)

    // ENDGAME B.3.1 step 2: all three details above ARE
    // `??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z` (0x180026d40), which is
    // why the two blocks collapse to one operator= each. Read the class body
    // (runtime/src/system/filename.cpp) against the three numbered notes: the
    // guard is on the BUFFER and skips both halves; the store is unconditional
    // and only the increment is guarded; AdjustDelimiters is outside the `if`.

    SysFile(dst->_name0) = Sys(src->_name0);             // 18000c239..c27d
    SysFile(dst->_name1) = Sys(src->_name1);             // 18000c282..c2c6

    // No replay oracle, for the same reason as the copy ctor: the adopt half
    // AddRefs, so running the shipped body a second time on the same pair would
    // leave two extra references on every call. The lockstep probe compares
    // whole FileInfos produced through this path, and rung 4's probe below adds
    // a dedicated comparison with the buffer-identity and null cases forced.
    return dst;                          // 18000c2d0
}

// ============================================================================
// ?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@FileSystem@2@XZ  retail 0x180025400
// ============================================================================
// `return FileInfo(this->_fileInfo)`, where `_fileInfo` is the PackageFile's own
// embedded FileInfo at +0xC0 — the same +0xC0 lane PKG's ClosePackages hands to
// CloseFile, which is the independent confirmation of the offset.
// The body is `IO::PackageFile::GetFileInfo()` (runtime/types/io/PackageFile.h
// declares it; the definition is in filesystem_packages.cpp beside GetFileName).
// B.4 R-IO: the `RtIo_PackageFile_GetFileInfo` door is gone; ReadFile below,
// its one caller, calls the member on the typed `_packfiles` element.

// ============================================================================
// ?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBV…@Z  0x1800adce0
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 5b: the three IO::FileSystem bodies in this
// file move onto the class VERBATIM; the doors keep their names, their
// `extern "C"` linkage, their signatures and every registration line, and
// delegate in one line each. No qualified `IO::` call is needed: IO::FileSystem
// has no vtable at all, so B34_BRIEFS section A.1's re-entry hazard cannot
// arise. This file is bound by B34_BRIEFS section D — nothing was added,
// removed or reordered, and in particular NO lock, NO retry, NO positioned read
// and NO null check was introduced on the open/read path. The two-loader race
// LOADER_READ_SPIN.md describes has its own pending fix and it is not this one.

// B.4 R-IO: the `RtIo_FileSystem_ReadFile` / `RtIo_FileSystem_OpenFile` doors
// that stood here are gone; every caller calls the member on
// `IO::FileSystem::_singleton`.

}  // namespace RtIO

// ============================================================================
// `IO::FileSystem`'s member bodies for this file — B34_BRIEFS section D BINDING
// ============================================================================
// Defined outside `namespace RtIO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); these bodies reach
// this translation unit's `RtIO`-scoped names — including the
// anonymous-namespace helpers `PathStrICmp` and `StrLenOf`
// — as `RtIO::X`. Every POD is spelled `RtIO::` or `RtSystem::` and every real
// class `System::` (B34_RULES section 7.2).
//
// *** VERBATIM. *** Identical control flow, identical callees, identical
// statement order, identical comments. The only changes are the receiver's
// spelling (`self->field` to `_field`) and three call sites that were ALREADY
// direct calls within this file and are now direct MEMBER calls of the same
// bodies: ReadFile's recursion into itself (0x1800ace7e) and OpenFile's call to
// PlatformOpenFile (0x1800acace). Neither ever went through a resolve door, so
// neither changes which body runs.
namespace IO {

bool FileSystem::PlatformOpenFile(RtIO::FileInfo& fiRef,
                                  const System::Filename& name) {
    RtIO::FileInfo* const fi = &fiRef;
    // retail never reads `this` here

    // An empty FileInfo, assigned over the caller's — retail builds it on the
    // stack and calls FileInfo::operator= rather than clearing in place, which
    // is what releases whatever the caller's Strings were holding.
    RtIO::FileInfo blank;                // 1800add08..1800add30
    blank.handle     = -1;
    blank.size       = 0;
    blank.cursor     = 0;
    blank.baseOffset = 0;
    // Two empty Filenames constructed into the raw stack slots, retail's
    // ??0Filename@System@@QEAA@XZ (0x180006220) — a zero store, and no dtor has
    // run over them yet.
    new (&blank._name0) System::Filename();
    new (&blank._name1) System::Filename();
    blank.winHandle  = INVALID_HANDLE_VALUE;
    blank._unk04     = 0;                // retail leaves this whatever the stack
    blank._pad14     = 0;                // held; see §4 — zeroed here only so the
                                         // value we copy out is defined.
    RtIO::RtIo_FileInfo_Assign(fi, &blank);    // 1800add3f
    // ~blank, reverse order. FileInfo is not a converted class, so its two
    // Filename members are destroyed by hand — one release each, as retail.
    RtIO::SysFile(blank._name1).~Filename();   // 1800add45
    RtIO::SysFile(blank._name0).~Filename();   // 1800add75

    // WideString ws(*name); CreateFileW(ws, ...). ??0WideString(const String&),
    // constructed into the POD slot and released by hand, as before.
    struct { void* data; } ws = { nullptr };                 // 1800add98
    ::new (static_cast<void*>(&ws)) System::WideString(       // B.3.x bridge
        name.AsString());                                    // 1800addad

    // 1800addb3..1800addd7 — the argument block, transcribed rather than named
    // from habit: dwDesiredAccess 0x80000000 (GENERIC_READ), dwShareMode 1
    // (FILE_SHARE_READ), lpSecurityAttributes NULL, dwCreationDisposition 3
    // (OPEN_EXISTING), dwFlagsAndAttributes 0x80 (FILE_ATTRIBUTE_NORMAL),
    // hTemplateFile NULL.
    const HANDLE h = ::CreateFileW(static_cast<LPCWSTR>(ws.data),
                                   0x80000000u, 1u, nullptr, 3u, 0x80u, nullptr);
    fi->winHandle = h;          // 1800adddd

    RtIO::RtString_ReleaseInline(
        reinterpret_cast<RtSystem::String*>(&ws));           // 1800adde1

    // `LEA RAX,[RCX-1] ; CMP RAX,-3 ; JA fail` — an UNSIGNED test that rejects
    // exactly two values, INVALID_HANDLE_VALUE (-1) and NULL (0): unsigned
    // `h - 1 > 2^64 - 3` holds for h - 1 in {2^64 - 2, 2^64 - 1}, i.e. for
    // h == 0 and h == -1 and for nothing else. B.4 R-IO: `winHandle` is the
    // HANDLE it holds, so the two values are named.
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {         // 1800ade12
        return false;                                        // 1800ade3c
    }

    fi->cursor = static_cast<int32_t>(
        ::SetFilePointer(h, 0, nullptr, FILE_BEGIN));        // 1800ade20/26
    fi->size = static_cast<int32_t>(::GetFileSize(h, nullptr));  // 1800ade2f/35

    return true;                                             // 1800ade38
}

// ============================================================================
// ?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z      retail 0x1800ace00
// ============================================================================
bool FileSystem::ReadFile(RtIO::FileInfo& fiRef, int32_t n, void* dst) {
    RtIO::FileInfo* const fi = &fiRef;

    // UNSIGNED: -1 and -2 both fall through to the loose path (§1).
    if (static_cast<uint32_t>(fi->handle) > 0xFFFFFFFDu) {   // 1800ace23  JA
        // ---- LOOSE ------------------------------------------------------
        DWORD got = 0;                                       // 1800acf0f
        ::ReadFile(fi->winHandle, dst,
                   static_cast<DWORD>(n), &got, nullptr);    // 1800acf2a
        fi->cursor += static_cast<int32_t>(got);             // 1800acf37
        return static_cast<int32_t>(got) == n;               // 1800acf3c  SETZ
    }

    // ---- PACKED ---------------------------------------------------------
    // The pack index is the TOP BYTE of the tag (§1).
    const uint32_t packIndex =
        static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&fi->handle)[3]);
    PackageFile* const pkg = _packfiles[packIndex];          // 1800ace31/39

    RtIO::FileInfo pack = pkg->IO::PackageFile::GetFileInfo();  // 1800ace3d

    const int32_t abs = fi->baseOffset + fi->cursor;         // 1800ace43/46
    bool ok;
    if (static_cast<uint32_t>(pack.handle) <= 0xFFFFFFFDu) {  // 1800ace4d  JBE
        // The containing .pkg is itself packed — no seek, just recurse.
        ok = true;
    } else {
        ok = (static_cast<int32_t>(::SetFilePointer(
                  pack.winHandle, abs, nullptr,
                  FILE_BEGIN)) == abs);                      // 1800ace61/67
    }
    if (ok) {
        ok = ReadFile(pack, n, dst);                         // 1800ace7e — ITSELF
        if (ok) fi->cursor += n;                             // 1800ace87
    }

    // ~pack — the two Filenames released in REVERSE order, +0x28 then +0x20.
    RtIO::SysFile(pack._name1).~Filename();                  // 1800acea1
    RtIO::SysFile(pack._name0).~Filename();                  // 1800aceda

    return ok;                                               // 1800acf07
}

// ============================================================================
// ?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z  0x1800ac880
// ============================================================================
// A FileInfo-RETURNING member, so the hidden return slot is the SECOND argument
// (RCX = this, RDX = ret) and RAX is the slot on the way out — `MOV RAX,RDI` at
// 0x1800acc2b. Getting that wrong is reference_hidden_retslot_wild_write.md.
RtIO::FileInfo* FileSystem::OpenFile(RtIO::FileInfo* ret,
                                     const System::Filename& name) {

    RtIO::FileInfo lf;                                       // 1800ac8b3..
    lf.handle     = -1;
    lf.size       = 0;
    lf.cursor     = 0;
    lf.baseOffset = 0;
    // Two empty Filenames constructed into raw stack slots (0x180006220), so
    // that the FileInfo::operator= PlatformOpenFile runs over them below has a
    // live object to release. Their destructors are at the bottom.
    new (&lf._name0) System::Filename();
    new (&lf._name1) System::Filename();
    lf.winHandle  = INVALID_HANDLE_VALUE;
    lf._pad14     = 0;
    // +0x04 is DELIBERATELY left as-is: retail never writes it and copies it out
    // anyway (§4). Reading it below reproduces that; the oracle excludes it.

    bool emitted = false;

    const int32_t nameLen = RtIO::StrLenOf(name._data);      // 1800ac8e2
    const int32_t dirLen  = RtIO::StrLenOf(_dataDirectory.data); // 1800ac8f2
    if (nameLen > dirLen) {                                  // 1800ac904  JLE skips
        // Strip the data-directory prefix; the packfile index is keyed on the
        // remainder, which is what LoadPackages inserted.
        // The BST walk. Same tree and same comparator as lane PKG's IndexFind,
        // and the same null asymmetries: a null on EITHER side stops the descent
        // rather than comparing.
        RtIO::PackfileIndexNode* node = _packfileIndexRoot;  // 1800ac938
        {
            // Set-then-AdjustDelimiters IS the Filename const char* ctor,
            // ??0Filename@System@@QEAA@PEBD@Z (0x180026cf0); the inner scope is
            // retail's release point at 0x1800ac991, before the node is used.
            System::Filename key(name._data + dirLen);       // 1800ac91a..932

            while (node != nullptr) {                        // 1800ac98c
                const char* const nk = node->key;            // 1800ac945
                if (nk != nullptr && key._data != nullptr &&
                    RtIO::PathStrICmp(key._data, nk) < 0) {  // 1800ac955  JNS skips
                    node = node->left;                       // 1800ac962  +0x18
                    continue;
                }
                if (key._data != nullptr && nk != nullptr &&
                    RtIO::PathStrICmp(nk, key._data) < 0) {  // 1800ac978
                    node = node->right;                      // 1800ac985  +0x20
                    continue;
                }
                break;                                       // found
            }
        }                                                    // ~key  1800ac991

        if (node != nullptr && _existsInPackfileGate != 0) {  // 1800ac9bf
            lf.handle = static_cast<int32_t>(
                (node->packIndex << 24) | node->fileIndex);  // 1800ac9c5..d1
            const PackageFile* const pkg = _packfiles[node->packIndex];
            const RtIO::PackageFileEntry* const e = &pkg->_entries[node->fileIndex];
            lf.size       = static_cast<int32_t>(e->size);   // 1800ac9ee  +0xc
            lf.baseOffset = static_cast<int32_t>(e->offset); // 1800aca0c  +0x8
            lf.cursor     = 0;                               // 1800aca13
            emitted = true;                                  // JMP 1800acb8b
        }
    }

    if (!emitted) {
        if (_platformExistsGate == 0) {                      // 1800aca1e
            ret->handle     = -1;                            // 1800aca28
            ret->size       = 0;
            ret->cursor     = 0;
            ret->baseOffset = 0;
            // `ret` is the hidden return slot: raw bytes, so these are two
            // default ctors (0x180006220), not stores through live objects.
            new (&ret->_name0) System::Filename();
            new (&ret->_name1) System::Filename();
            ret->winHandle  = INVALID_HANDLE_VALUE;                            // 1800aca4c
        } else if (!PlatformOpenFile(lf, name)) {            // 1800acace
            ret->handle     = -1;                            // 1800acadb
            ret->size       = 0;
            ret->cursor     = 0;
            ret->baseOffset = 0;
            new (&ret->_name0) System::Filename();           // as above
            new (&ret->_name1) System::Filename();
            ret->winHandle  = INVALID_HANDLE_VALUE;                            // 1800acaff
        } else {
            lf.handle = -2;                                  // 1800acb77
            emitted   = true;
        }
    }

    if (emitted) {                                           // 1800acb8b
        ret->handle     = lf.handle;
        ret->_unk04     = lf._unk04;   // retail's uninitialised dword — §4
        ret->size       = lf.size;
        ret->cursor     = lf.cursor;
        ret->baseOffset = lf.baseOffset;
        ret->winHandle  = lf.winHandle;                      // 1800acba2
        // The copy ctor (0x1800067d0) into the raw return slot, as retail's two
        // CALLs. It AddRefs, which is why ~lf below is still exactly one
        // release per name and not a drop to zero.
        new (&ret->_name0) System::Filename(RtIO::SysFile(lf._name0));  // 1800acbae
        new (&ret->_name1) System::Filename(RtIO::SysFile(lf._name1));  // 1800acbbc
    }

    // ~lf, reverse order.
    RtIO::SysFile(lf._name1).~Filename();                    // 1800acbc9
    RtIO::SysFile(lf._name0).~Filename();                    // 1800acbfa

    return ret;                                              // 1800acc2b
}

}  // namespace IO

