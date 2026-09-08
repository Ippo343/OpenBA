// ============================================================================
// runtime/src/io/filesystem_openfile_probe.cpp — rung 3's oracle
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// WHY IT IS SHAPED LIKE THIS
//
// `OpenFile` is nearly pure — a FileSystem and a Filename in, a FileInfo out —
// so unlike rung 2 it CAN be replayed. Two things stop a plain byte compare:
//
//  1. **The loose path opens a real Win32 HANDLE**, so ours and theirs get
//     different values at +0x18 by construction. Compared as "both valid or both
//     invalid", never for equality.
//  2. **+0x04 is retail's own uninitialised stack** (see §4 of the port). It
//     differs on every call and would report a 100% divergence
//     (reference_oracle_must_not_compare_unwritten_bytes.md). Excluded, and the
//     exclusion is named here rather than buried in a memcmp length.
//
// `ReadFile` mutates `cursor` and consumes the file, so it gets rung 2's
// treatment: two independently opened FileInfos over the same path, read in
// LOCKSTEP, compared after every read on the bytes AND on the whole FileInfo.
//
// THE CORPUS IS REAL AND COVERS BOTH BRANCHES, which is the part that matters:
// after LoadPackages the packfile index is populated, so
//   * a name taken from a real package entry drives the PACKED path (and with
//     it the recursion, the seek, and GetFileInfo);
//   * `WinData/data0.pkg` itself drives the LOOSE path (CreateFileW, GetFileSize).
// A probe that only had one of the two would leave half of `ReadFile` and most
// of `OpenFile` unentered while reporting green.
//
// EVERY FileInfo THIS PROBE OPENS IS CLOSED through `?CloseFile@`, which this
// lane does not port. Leaking a Win32 handle per probe run would be a slow leak
// nobody would attribute to a diff harness.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/string.h"

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;

extern "C" FileInfo* RtIo_FileSystem_OpenFile(FileSystem*, FileInfo*, const Filename*);
extern "C" bool      RtIo_FileSystem_ReadFile(FileSystem*, FileInfo*, int32_t, void*);

namespace {

typedef FileInfo* (*OpenFileFn)(FileSystem*, FileInfo*, const Filename*);
typedef bool      (*ReadFileFn)(FileSystem*, FileInfo*, int32_t, void*);
typedef void      (*CloseFileFn)(FileSystem*, FileInfo*);
typedef Filename* (*GetFileNameFn)(const PackageFileView*, Filename*, int);
typedef void      (*StringSetFn)(String*, const char*);
// `??HSystem@@YA?AVString@0@AEBV10@PEBD@Z` is a FREE function returning String
// by value: `String operator+(const String&, const char*)`. For a free function
// the hidden return slot is the FIRST argument (RCX = ret, RDX = lhs, R8 = rhs)
// -- unlike a member, where it is the second. The first draft of this typedef
// had it second and only worked because both pointers happened to be the same
// object, which is also the SECOND bug it hid: writing the result over its own
// input (reference_oracle_must_snapshot_aliased_inputs.md). The result now goes
// to a separate String.
typedef String*   (*ConcatFn)(String* ret, const String* lhs, const char* rhs);

const char* const kOpen  = "?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z";
const char* const kRead  = "?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z";
const char* const kClose = "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z";

void Close(FileSystem* fs, FileInfo* fi) {
    RT_EXPORT_ONCE(CloseFileFn, kClose)(fs, fi);
    RtString_ReleaseInline(&fi->_name1);
    RtString_ReleaseInline(&fi->_name0);
}

// Compare two FileInfos, naming the failing term. `winHandle` and `_unk04` are
// EXCLUDED and the reason is stated at the site, per the header.
const char* CompareInfo(const FileInfo* a, const FileInfo* b) {
    if (a->handle     != b->handle)     return "handle";
    if (a->size       != b->size)       return "size";
    if (a->cursor     != b->cursor)     return "cursor";
    if (a->baseOffset != b->baseOffset) return "baseOffset";
    // A HANDLE cannot be compared for equality -- two opens of the same file
    // legitimately differ. What must agree is VALIDITY.
    const bool va = (static_cast<uint64_t>(a->winHandle) - 1u) <= static_cast<uint64_t>(-3);
    const bool vb = (static_cast<uint64_t>(b->winHandle) - 1u) <= static_cast<uint64_t>(-3);
    if (va != vb) return "winHandle validity";
    // _unk04 is retail's uninitialised stack: deliberately NOT compared.
    return nullptr;
}

struct PathResult {
    int64_t opens = 0, reads = 0, bytes = 0;
    int64_t packed = 0, loose = 0, missing = 0;
    const char* bad = nullptr;
};

// One subject: open the same name twice (ours and theirs), compare the
// FileInfos, then read in lockstep comparing bytes and state after every read.
void RunOne(FileSystem* fs, const Filename* name, PathResult* r) {
    // A LOCAL guard, not one inherited from the caller: the raw doors below
    // reach the shipped bodies on purpose and that is only sound while the diff
    // harness is on. Rung 2 learned this from tools/port_bypass_audit.py, whose
    // guard analysis is per-site.
    if (!RtDiffEnabled()) return;
    OpenFileFn theirsOpen = RtOriginal<OpenFileFn>(kOpen);   // RT_RAW_OK: harness-only
    ReadFileFn theirsRead = RtOriginal<ReadFileFn>(kRead);   // RT_RAW_OK: harness-only
    if (!theirsOpen || !theirsRead) { r->bad = "resolve shipped OpenFile/ReadFile"; return; }

    FileInfo mine, theirs;
    std::memset(&mine, 0, sizeof mine);
    std::memset(&theirs, 0, sizeof theirs);
    RtIo_FileSystem_OpenFile(fs, &mine, name);
    theirsOpen(fs, &theirs, name);
    ++r->opens;

    const char* bad = CompareInfo(&mine, &theirs);
    if (bad) { r->bad = bad; Close(fs, &mine); Close(fs, &theirs); return; }

    if (mine.handle == -1) { ++r->missing; Close(fs, &mine); Close(fs, &theirs); return; }
    if (mine.handle == -2) ++r->loose; else ++r->packed;

    // Lockstep reads. Sizes chosen so at least one read straddles the end of a
    // small file and one is larger than a typical pack entry.
    static const int32_t kSizes[] = { 1, 13, 512, 4096, 100, 65536 };
    static uint8_t bufA[65536];
    static uint8_t bufB[65536];
    const int32_t total = mine.size;
    int32_t done = 0;
    for (int i = 0; i < 6 && done < total && !r->bad; ++i) {
        int32_t n = kSizes[i];
        if (n > total - done) n = total - done;
        if (n <= 0) break;
        std::memset(bufA, 0xA5, static_cast<size_t>(n));
        std::memset(bufB, 0x5A, static_cast<size_t>(n));

        const bool ra = RtIo_FileSystem_ReadFile(fs, &mine, n, bufA);
        const bool rb = theirsRead(fs, &theirs, n, bufB);
        ++r->reads; r->bytes += n; done += n;

        if (ra != rb)                                       { r->bad = "read return"; break; }
        if (std::memcmp(bufA, bufB, static_cast<size_t>(n))) { r->bad = "read bytes"; break; }
        const char* s = CompareInfo(&mine, &theirs);
        if (s) { r->bad = s; break; }
    }

    Close(fs, &mine);
    Close(fs, &theirs);
}

}  // namespace

// ============================================================================
void RtIo_FileSystemOpenFileProbe(const FileSystem* cfs) {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::FileSystem::OpenFile/ReadFile [lockstep probe]");

    FileSystem* const fs = const_cast<FileSystem*>(cfs);
    if (fs == nullptr || fs->packfiles == nullptr || fs->packfileCount <= 0) {
        std::printf("[OpenFile] probe SKIPPED: no mounted packages\n");
        RT_DIFF_SHADOW(false, "no packages", "a mounted FileSystem is required");
        return;
    }

    PathResult r;

    // ---- the PACKED path -------------------------------------------------
    // A real entry name from a real package. OpenFile strips `dataDirectory`
    // from the front of what it is given, so the name has to be rebuilt WITH
    // that prefix -- which is itself a check that the two halves agree about
    // where the boundary is.
    for (int32_t pi = 0; pi < fs->packfileCount && pi < 2 && !r.bad; ++pi) {
        const PackageFileView* const pkg = fs->packfiles[pi];
        if (pkg == nullptr || pkg->entryCount <= 0) continue;
        for (int32_t k = 0; k < 4 && k < pkg->entryCount && !r.bad; ++k) {
            const int32_t fi = (pkg->entryCount / 4) * k;
            Filename entry;
            entry.data = nullptr;
            RT_EXPORT_ONCE(GetFileNameFn,
                "?GetFileName@PackageFile@IO@@QEBA?AVFilename@System@@H@Z")(
                    pkg, &entry, fi);
            if (entry.data != nullptr) {
                String dir;    dir.data    = nullptr;
                String joined; joined.data = nullptr;
                RT_EXPORT_ONCE(StringSetFn, "?Set@String@System@@QEAAXPEBD@Z")(
                    &dir, fs->dataDirectory.data ? fs->dataDirectory.data : "");
                RT_EXPORT_ONCE(ConcatFn, "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z")(
                    &joined, &dir, entry.data);
                RunOne(fs, reinterpret_cast<const Filename*>(&joined), &r);
                RtString_ReleaseInline(&joined);
                RtString_ReleaseInline(&dir);
            }
            RtString_ReleaseInline(reinterpret_cast<String*>(&entry));
        }
    }

    // ---- the LOOSE path --------------------------------------------------
    // The .pkg files themselves are ordinary files on disk, so asking for one
    // by its real path drives CreateFileW / GetFileSize / the loose ReadFile.
    if (!r.bad) {
        static const char* const kLoose[] = { "data0.pkg", "patch0.pkg", "nope_missing.bin" };
        for (int i = 0; i < 3 && !r.bad; ++i) {
            String dir;    dir.data    = nullptr;
            String joined; joined.data = nullptr;
            RT_EXPORT_ONCE(StringSetFn, "?Set@String@System@@QEAAXPEBD@Z")(
                &dir, fs->packDirectory.data ? fs->packDirectory.data : "");
            RT_EXPORT_ONCE(ConcatFn, "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z")(
                &joined, &dir, kLoose[i]);
            RunOne(fs, reinterpret_cast<const Filename*>(&joined), &r);
            RtString_ReleaseInline(&joined);
            RtString_ReleaseInline(&dir);
        }
    }

    // Effectiveness control. Three outcomes must all occur or the run has not
    // exercised the function: a PACKED open (the BST hit + the recursion), a
    // LOOSE open (CreateFileW), and a MISS (the -1 return). A green report with
    // any of the three at zero is a green report about code that did not run.
    const bool covered = r.packed > 0 && r.loose > 0 && r.missing > 0;
    const bool pass = (r.bad == nullptr) && covered;

    char a[192], b[192];
    std::snprintf(a, sizeof(a), "opens=%lld packed=%lld loose=%lld miss=%lld reads=%lld bytes=%lld",
                  (long long)r.opens, (long long)r.packed, (long long)r.loose,
                  (long long)r.missing, (long long)r.reads, (long long)r.bytes);
    if (r.bad) std::snprintf(b, sizeof(b), "DIVERGED on '%s'", r.bad);
    else if (!covered) std::snprintf(b, sizeof(b), "no divergence, but a PATH READS ZERO");
    else std::snprintf(b, sizeof(b), "identical on handle, size, cursor, baseOffset, "
                                     "handle validity and every byte read");
    RT_DIFF_SHADOW(pass, a, b);
    RT_DIFF_WORK(r.opens > 0);

    std::printf("[OpenFile] lockstep probe: %lld opens (packed=%lld loose=%lld miss=%lld), "
                "%lld reads, %lld bytes -- %s\n",
                (long long)r.opens, (long long)r.packed, (long long)r.loose,
                (long long)r.missing, (long long)r.reads, (long long)r.bytes, b);
    std::printf("[OpenFile] NOT compared, and deliberately: winHandle (two opens of one "
                "file differ by construction -- validity is compared instead) and +0x04 "
                "(retail's own uninitialised stack, copied out by OpenFile).\n");
}

}  // namespace RtIO
