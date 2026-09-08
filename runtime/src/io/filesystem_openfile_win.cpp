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
// Kill switch: BANISHED_RT_OPENFILE=0 leaves all of it on the DLL.
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

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;

namespace {

typedef FileInfo* (*OpenFileFn)(FileSystem*, FileInfo*, const Filename*);
typedef bool      (*ReadFileFn)(FileSystem*, FileInfo*, int32_t, void*);
typedef bool      (*PlatformOpenFileFn)(FileSystem*, FileInfo*, const Filename*);
typedef FileInfo* (*GetFileInfoFn)(const PackageFileView*, FileInfo*);
typedef FileInfo* (*FileInfoCopyFn)(FileInfo*, const FileInfo*);
typedef FileInfo* (*FileInfoAssignFn)(FileInfo*, const FileInfo*);
typedef void      (*CloseFileFn)(FileSystem*, FileInfo*);
typedef Filename* (*FilenameCopyCtorFn)(Filename*, const Filename*);
typedef void      (*StringSetFn)(String*, const char*);
typedef void      (*AdjustDelimitersFn)(Filename*);
typedef void*     (*WideStringCtorFn)(void*, const String*);
typedef int       (*StrICmpFn)(const char*, const char*);

// The CRT `_stricmp` OpenFile's BST walk uses. Unexported, so ResolveAddr is the
// proper door — the same one lane PKG's `PathStrICmp` uses for the same address.
// It must NOT be replaced by std::strcasecmp or shared with
// runtime/src/system/configuration.cpp's comparator: 0x18007eec0
// (`__ascii_stricmp`, the tail it jumps to) lowers only 'A'-'Z' and compares
// UNSIGNED, while the config map inlines a SIGNED-char strcmp. They disagree on
// any byte >= 0x80 and a package path can contain one.
int PathStrICmp(const char* a, const char* b) {
    static StrICmpFn fn =
        reinterpret_cast<StrICmpFn>(ResolveAddr(Module::Runtime, 0x18007eefc));
    return fn(a, b);
}

// FileInfo::operator= (0x18000c200), unexported. PlatformOpenFile uses it to
// reset the caller's FileInfo to an empty one before opening.
FileInfo* FileInfoAssign(FileInfo* dst, const FileInfo* src) {
    static FileInfoAssignFn fn =
        reinterpret_cast<FileInfoAssignFn>(ResolveAddr(Module::Runtime, 0x18000c200));
    return fn(dst, src);
}

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
    RT_DIFF_BEGIN("IO::FileSystem::FileInfo::FileInfo(const&)");

    dst->handle     = src->handle;       // 18000c12e
    dst->_unk04     = src->_unk04;       // 18000c132
    dst->size       = src->size;         // 18000c138
    dst->cursor     = src->cursor;       // 18000c13e
    dst->baseOffset = src->baseOffset;   // 18000c144
    dst->winHandle  = src->winHandle;    // 18000c14a  (+0x14 is NOT copied)
    RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(
        reinterpret_cast<Filename*>(&dst->_name0),
        reinterpret_cast<const Filename*>(&src->_name0));   // 18000c15a
    RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(
        reinterpret_cast<Filename*>(&dst->_name1),
        reinterpret_cast<const Filename*>(&src->_name1));   // 18000c168

    // No replay oracle: the Filename copy ctors AddRef, so running the shipped
    // body a second time would leave two extra references on every call. The
    // lockstep probe compares whole FileInfos produced through this path.
    RT_DIFF_SHADOW(dst->handle == src->handle && dst->size == src->size,
                   "copied 5 dwords + qword + 2 Filenames",
                   "SHADOW ONLY (AddRefs; verified by the rung-3 probe)");
    RT_DIFF_WORK(true);
    return dst;                          // 18000c16e
}

// ============================================================================
// ?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@FileSystem@2@XZ  retail 0x180025400
// ============================================================================
// `return FileInfo(this->_fileInfo)`, where `_fileInfo` is the PackageFile's own
// embedded FileInfo at +0xC0 — the same +0xC0 lane PKG's ClosePackages hands to
// CloseFile, which is the independent confirmation of the offset.
extern "C" FileInfo* RtIo_PackageFile_GetFileInfo(const PackageFileView* self,
                                                  FileInfo* ret) {
    RT_DIFF_BEGIN("IO::PackageFile::GetFileInfo");
    const FileInfo* const src = reinterpret_cast<const FileInfo*>(
        reinterpret_cast<const uint8_t*>(self) + 0xC0);      // 18002541e
    RtIo_FileInfo_CopyCtor(ret, src);                        // 180025428
    RT_DIFF_SHADOW(ret->handle == src->handle,
                   "copied the package's own FileInfo",
                   "SHADOW ONLY (AddRefs; verified by the rung-3 probe)");
    RT_DIFF_WORK(true);
    return ret;                                              // 180025436
}

// ============================================================================
// ?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBV…@Z  0x1800adce0
// ============================================================================
extern "C" bool RtIo_FileSystem_PlatformOpenFile(FileSystem* self, FileInfo* fi,
                                                 const Filename* name) {
    RT_DIFF_BEGIN("IO::FileSystem::PlatformOpenFile");
    (void)self;                          // retail never reads `this` here

    // An empty FileInfo, assigned over the caller's — retail builds it on the
    // stack and calls FileInfo::operator= rather than clearing in place, which
    // is what releases whatever the caller's Strings were holding.
    FileInfo blank;                      // 1800add08..1800add30
    blank.handle     = -1;
    blank.size       = 0;
    blank.cursor     = 0;
    blank.baseOffset = 0;
    blank._name0.data = nullptr;
    blank._name1.data = nullptr;
    blank.winHandle  = -1;
    blank._unk04     = 0;                // retail leaves this whatever the stack
    blank._pad14     = 0;                // held; see §4 — zeroed here only so the
                                         // value we copy out is defined.
    FileInfoAssign(fi, &blank);          // 1800add3f
    RtString_ReleaseInline(&blank._name1);  // 1800add45 — reverse order
    RtString_ReleaseInline(&blank._name0);  // 1800add75

    // WideString ws(*name); CreateFileW(ws, ...). The ctor is lane FS's port.
    struct { void* data; } ws = { nullptr };                 // 1800add98
    RT_EXPORT_ONCE(WideStringCtorFn, "??0WideString@System@@QEAA@AEBVString@1@@Z")(
        &ws, reinterpret_cast<const String*>(name));         // 1800addad

    // 1800addb3..1800addd7 — the argument block, transcribed rather than named
    // from habit: dwDesiredAccess 0x80000000 (GENERIC_READ), dwShareMode 1
    // (FILE_SHARE_READ), lpSecurityAttributes NULL, dwCreationDisposition 3
    // (OPEN_EXISTING), dwFlagsAndAttributes 0x80 (FILE_ATTRIBUTE_NORMAL),
    // hTemplateFile NULL.
    const HANDLE h = ::CreateFileW(static_cast<LPCWSTR>(ws.data),
                                   0x80000000u, 1u, nullptr, 3u, 0x80u, nullptr);
    fi->winHandle = reinterpret_cast<int64_t>(h);            // 1800adddd

    RtString_ReleaseInline(reinterpret_cast<String*>(&ws));  // 1800adde1

    // `LEA RAX,[RCX-1] ; CMP RAX,-3 ; JA fail` — an UNSIGNED test that rejects
    // exactly two values, INVALID_HANDLE_VALUE (-1) and NULL (0). Spelled as
    // retail spells it rather than as `h == INVALID_HANDLE_VALUE || !h`, because
    // the two are only equivalent by accident of those being the only wrap cases.
    const uint64_t hm1 = static_cast<uint64_t>(fi->winHandle) - 1u;
    if (hm1 > static_cast<uint64_t>(-3)) {                   // 1800ade12
        RT_DIFF_WORK(true);
        return false;                                        // 1800ade3c
    }

    fi->cursor = static_cast<int32_t>(
        ::SetFilePointer(h, 0, nullptr, FILE_BEGIN));        // 1800ade20/26
    fi->size = static_cast<int32_t>(::GetFileSize(h, nullptr));  // 1800ade2f/35

    RT_DIFF_WORK(true);
    return true;                                             // 1800ade38
}

// ============================================================================
// ?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z      retail 0x1800ace00
// ============================================================================
extern "C" bool RtIo_FileSystem_ReadFile(FileSystem* self, FileInfo* fi,
                                         int32_t n, void* dst) {
    RT_DIFF_BEGIN("IO::FileSystem::ReadFile");

    // UNSIGNED: -1 and -2 both fall through to the loose path (§1).
    if (static_cast<uint32_t>(fi->handle) > 0xFFFFFFFDu) {   // 1800ace23  JA
        // ---- LOOSE ------------------------------------------------------
        DWORD got = 0;                                       // 1800acf0f
        ::ReadFile(reinterpret_cast<HANDLE>(fi->winHandle), dst,
                   static_cast<DWORD>(n), &got, nullptr);    // 1800acf2a
        fi->cursor += static_cast<int32_t>(got);             // 1800acf37
        RT_DIFF_WORK(true);
        return static_cast<int32_t>(got) == n;               // 1800acf3c  SETZ
    }

    // ---- PACKED ---------------------------------------------------------
    // The pack index is the TOP BYTE of the tag (§1).
    const uint32_t packIndex =
        static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&fi->handle)[3]);
    PackageFileView* const pkg = self->packfiles[packIndex];  // 1800ace31/39

    FileInfo pack;
    RtIo_PackageFile_GetFileInfo(pkg, &pack);                // 1800ace3d

    const int32_t abs = fi->baseOffset + fi->cursor;         // 1800ace43/46
    bool ok;
    if (static_cast<uint32_t>(pack.handle) <= 0xFFFFFFFDu) {  // 1800ace4d  JBE
        // The containing .pkg is itself packed — no seek, just recurse.
        ok = true;
    } else {
        ok = (static_cast<int32_t>(::SetFilePointer(
                  reinterpret_cast<HANDLE>(pack.winHandle), abs, nullptr,
                  FILE_BEGIN)) == abs);                      // 1800ace61/67
    }
    if (ok) {
        ok = RtIo_FileSystem_ReadFile(self, &pack, n, dst);  // 1800ace7e — ITSELF
        if (ok) fi->cursor += n;                             // 1800ace87
    }

    // ~pack — the two Filenames released in REVERSE order, +0x28 then +0x20.
    RtString_ReleaseInline(&pack._name1);                    // 1800acea1
    RtString_ReleaseInline(&pack._name0);                    // 1800aceda

    RT_DIFF_WORK(true);
    return ok;                                               // 1800acf07
}

// ============================================================================
// ?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z  0x1800ac880
// ============================================================================
// A FileInfo-RETURNING member, so the hidden return slot is the SECOND argument
// (RCX = this, RDX = ret) and RAX is the slot on the way out — `MOV RAX,RDI` at
// 0x1800acc2b. Getting that wrong is reference_hidden_retslot_wild_write.md.
extern "C" FileInfo* RtIo_FileSystem_OpenFile(FileSystem* self, FileInfo* ret,
                                              const Filename* name) {
    RT_DIFF_BEGIN("IO::FileSystem::OpenFile");

    FileInfo lf;                                             // 1800ac8b3..
    lf.handle     = -1;
    lf.size       = 0;
    lf.cursor     = 0;
    lf.baseOffset = 0;
    lf._name0.data = nullptr;
    lf._name1.data = nullptr;
    lf.winHandle  = -1;
    lf._pad14     = 0;
    // +0x04 is DELIBERATELY left as-is: retail never writes it and copies it out
    // anyway (§4). Reading it below reproduces that; the oracle excludes it.

    bool emitted = false;

    const int32_t nameLen = StrLenOf(name->data);            // 1800ac8e2
    const int32_t dirLen  = StrLenOf(self->dataDirectory.data);  // 1800ac8f2
    if (nameLen > dirLen) {                                  // 1800ac904  JLE skips
        // Strip the data-directory prefix; the packfile index is keyed on the
        // remainder, which is what LoadPackages inserted.
        String key;
        key.data = nullptr;                                  // 1800ac91a
        RT_EXPORT_ONCE(StringSetFn, "?Set@String@System@@QEAAXPEBD@Z")(
            &key, name->data + dirLen);                      // 1800ac928
        RT_EXPORT_ONCE(AdjustDelimitersFn,
            "?AdjustDelimiters@Filename@System@@AEAAXXZ")(
                reinterpret_cast<Filename*>(&key));          // 1800ac932

        // The BST walk. Same tree and same comparator as lane PKG's IndexFind,
        // and the same null asymmetries: a null on EITHER side stops the descent
        // rather than comparing.
        PackfileIndexNode* node = self->packfileIndexRoot;   // 1800ac938
        while (node != nullptr) {                            // 1800ac98c
            const char* const nk = node->key;                // 1800ac945
            if (nk != nullptr && key.data != nullptr &&
                PathStrICmp(key.data, nk) < 0) {             // 1800ac955  JNS skips
                node = node->left;                           // 1800ac962  +0x18
                continue;
            }
            if (key.data != nullptr && nk != nullptr &&
                PathStrICmp(nk, key.data) < 0) {             // 1800ac978
                node = node->right;                          // 1800ac985  +0x20
                continue;
            }
            break;                                           // found
        }
        RtString_ReleaseInline(&key);                        // 1800ac991

        if (node != nullptr && self->existsInPackfileGate != 0) {   // 1800ac9bf
            lf.handle = static_cast<int32_t>(
                (node->packIndex << 24) | node->fileIndex);  // 1800ac9c5..d1
            const PackageFileView* const pkg = self->packfiles[node->packIndex];
            const PackageFileEntry* const e =
                static_cast<const PackageFileEntry*>(pkg->entries) + node->fileIndex;
            lf.size       = static_cast<int32_t>(e->size);   // 1800ac9ee  +0xc
            lf.baseOffset = static_cast<int32_t>(e->offset); // 1800aca0c  +0x8
            lf.cursor     = 0;                               // 1800aca13
            emitted = true;                                  // JMP 1800acb8b
        }
    }

    if (!emitted) {
        if (self->platformExistsGate == 0) {                 // 1800aca1e
            ret->handle     = -1;                            // 1800aca28
            ret->size       = 0;
            ret->cursor     = 0;
            ret->baseOffset = 0;
            ret->_name0.data = nullptr;
            ret->_name1.data = nullptr;
            ret->winHandle  = -1;                            // 1800aca4c
        } else if (!RtIo_FileSystem_PlatformOpenFile(self, &lf, name)) {  // 1800acace
            ret->handle     = -1;                            // 1800acadb
            ret->size       = 0;
            ret->cursor     = 0;
            ret->baseOffset = 0;
            ret->_name0.data = nullptr;
            ret->_name1.data = nullptr;
            ret->winHandle  = -1;                            // 1800acaff
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
        RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(
            reinterpret_cast<Filename*>(&ret->_name0),
            reinterpret_cast<const Filename*>(&lf._name0));  // 1800acbae
        RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(
            reinterpret_cast<Filename*>(&ret->_name1),
            reinterpret_cast<const Filename*>(&lf._name1));  // 1800acbbc
    }

    // ~lf, reverse order.
    RtString_ReleaseInline(&lf._name1);                      // 1800acbc9
    RtString_ReleaseInline(&lf._name0);                      // 1800acbfa

    RT_DIFF_WORK(true);
    return ret;                                              // 1800acc2b
}

}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
int RtIoFileSystemOpenFileRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport(
        "?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z",
        (void*)&RtIo_FileSystem_OpenFile); ++n;
    RegisterRuntimeAddr(0x1800ac880, (void*)&RtIo_FileSystem_OpenFile);

    RegisterRuntimeExport("?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z",
                          (void*)&RtIo_FileSystem_ReadFile); ++n;
    RegisterRuntimeAddr(0x1800ace00, (void*)&RtIo_FileSystem_ReadFile);

    RegisterRuntimeExport(
        "?PlatformOpenFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z",
        (void*)&RtIo_FileSystem_PlatformOpenFile); ++n;
    RegisterRuntimeAddr(0x1800adce0, (void*)&RtIo_FileSystem_PlatformOpenFile);

    RegisterRuntimeExport("?GetFileInfo@PackageFile@IO@@QEBA?AUFileInfo@FileSystem@2@XZ",
                          (void*)&RtIo_PackageFile_GetFileInfo); ++n;
    RegisterRuntimeAddr(0x180025400, (void*)&RtIo_PackageFile_GetFileInfo);

    // The FileInfo copy ctor is UNEXPORTED, so it gets the address door only —
    // the same shape as lane Q's Collection helpers.
    RegisterRuntimeAddr(0x18000c110, (void*)&RtIo_FileInfo_CopyCtor);

    printf("[RtPort]   IO::FileSystem open/read (lane PKG2 rung 3): %d exports registered\n", n);

    // DELIBERATELY ABSENT — §5.
    //   ?CloseFile@FileSystem@IO@@   0x1800accc0  ord 1726   (the teardown half)
    //   FUN_18000c200  FileInfo::operator=   (called by address, named above)
    //   FUN_18007eefc  the CRT _stricmp      (called by address; this rung ADDS
    //                  a second live caller of it — see the census note)
    return n;
}
