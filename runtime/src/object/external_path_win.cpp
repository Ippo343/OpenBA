// ============================================================================
// runtime/src/object/external_path_win.cpp — Wave 9 lane RRES
//   `Object::ExternalFactory::ExpandRelativePath` and the three readers it
//   needs, byte-faithful from the disassembly.
// ============================================================================
//
//   ?GetPathExtension@Filename@System@@QEBA?AV12@XZ                0x1800c64f0
//   ?GetPathStackFilename@ExternalFactory@Object@@QEBA?AV...XZ     0x1800bcf60
//   ?GetPathStackDirectory@ExternalFactory@Object@@QEBA?AV...XZ    0x1800bcfd0
//   ?ExpandRelativePath@ExternalFactory@Object@@QEBA?AV...AEBV34@_N@Z
//                                                                  0x1800bd360
//
// Every body below was transcribed from `disassemble_function(address=)` on
// Runtime-x64.dll (image base 0x180000000, read 2026-09-08), NOT from the
// decompiled C. Ghidra's C for 0x1800bd360 is actively misleading here: it
// mistakes the hidden return slot for `param_1`, so the `const Filename&`
// argument arrives as `param_2` reconstructed out of `in_register_00000081`,
// the `bool` shows up as a phantom `in_R9B`, and the destination slot of every
// one of the four `GetPath*` calls at the top of the function is DROPPED
// entirely (reference_ghidra_c_output_vs_disassembly.md).
//
// A `_win.cpp` because `ExpandRelativePath` calls `GetFileAttributesW`, exactly
// as `runtime/src/io/filesystem_setupdirs_win.cpp` does for the same inlined
// `FileSystem::Exists` — PLAN §4.1 decision 1 keeps <windows.h> out of the
// portable half and the `_win` suffix is where it is allowed to live.
//
// Layout evidence for the path stack: the `PathStackEntry` block in
// runtime/types/object/External.h. Wave write-up, including HONEST LIMITS and
// what this lane DECLINED: docs/runtime_port/WAVE9_LANE_RRES.md.
//
// ---------------------------------------------------------------------------
// THE RESOURCE-LOAD LOCK
// ---------------------------------------------------------------------------
// `ExpandRelativePath` takes `ResLoadGuard` (standalone/res_load_lock.h). It is
// on the racy path lane FNHOOK measured — it is called only from `FindOrLoad`,
// it does refcounted `String`/`Filename` traffic while building six
// concatenations, and that traffic on two threads at once is one of the two
// faces of the crash FNHOOK's lock closes.
//
// **THIS IS NOT THE GUARD THE BRIEF ASKED FOR, AND IT DOES NOT REPLACE IT.**
// The constraint was that the ported `FindOrLoad` body must take the lock;
// `FindOrLoad` is NOT ported by this lane (§5 of the write-up), so the hole
// FNHOOK described is still open. The lock is recursive, so a `FindOrLoad` that
// takes it later nests over this one for free.
// ---------------------------------------------------------------------------

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "external_path.h"

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/setupdir_helpers.h"
#include "../system/widestring.h"
#include "../../../standalone/res_load_lock.h"

// ---------------------------------------------------------------------------
// The seam to the detour installer. `standalone/res_path_detour.cpp` patches
// the DLL's own 0x1800bd360 so that RETAIL's callers — principally
// `FindOrLoad`, ~22.6k calls per menu-to-gameplay run (lane FNHOOK's
// `[ResLoad] census`) — land in the body below. Without it this port has NO
// caller at all in the tree and would be a textbook `calls=0` port.
//
// `RtResPathTrampoline()` returns the trampoline holding retail's displaced
// prologue, i.e. THE ORIGINAL BODY. The diff oracle must call that and never
// `RtOriginal("?ExpandRelativePath@...")`: with the detour installed,
// GetProcAddress hands back the PATCHED address and the "original" call would
// re-enter our own body forever (RtOriginal IS GetProcAddress —
// reference_rtoriginal_is_getprocaddress.md).
// ---------------------------------------------------------------------------
extern "C" bool  RtResPathInstallDetour(void* ourFn);
extern "C" void* RtResPathTrampoline();

namespace RtSystem {
// Declared locally, house style (see runtime/src/io/filesystem_setupdirs_win.cpp
// lines 138-158): filename.cpp exposes no header of its own.
void      RtSystem_Filename_AdjustDelimiters(Filename* self);              // 0x1800c6970
Filename* RtSystem_Filename_GetPathFilename(const Filename* self, Filename* ret,
                                            bool stripExtension);          // 0x1800c6740
Filename* RtSystem_Filename_GetPathIdentifer(const Filename* self, Filename* ret);
                                                                           // 0x1800c6610
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);
                                                                           // 0x1800067d0
}  // namespace RtSystem

namespace RtIO {
// runtime/src/io/filesystem.cpp:185. Takes the FileSystem as void* there.
RtSystem::Filename* RtIo_FileSystem_GetDataDirectory(const void* fsVoid,
                                                     RtSystem::Filename* ret);
}  // namespace RtIO

namespace RtObject {

using RtSystem::Filename;
using RtSystem::String;
using RtSystem::WideString;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Set;
using RtSystem::RtSystem_String_SubString;
using RtSystem::RtSystem_String_Concat;
using RtSystem::RtSystem_String_ConcatCStr;
using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_GetPathFilename;
using RtSystem::RtSystem_Filename_GetPathIdentifer;
using RtSystem::RtSystem_Filename_GetPathDirectory;
using RtSystem::RtSystem_WideString_CtorFromString;

PathCounters g_pc = {};

namespace {

// `String* ` and `Filename*` are layout-identical (runtime/types/system/
// Filename.h); retail casts between them by pointer at every site and so does
// every other file in this port.
inline String*       AsStr(Filename* f)             { return reinterpret_cast<String*>(f); }
inline const String* AsStr(const Filename* f)       { return reinterpret_cast<const String*>(f); }

// The int32 character count at data-8 (runtime/types/system/String.h).
inline int32_t BufLength(const char* d) {
    return reinterpret_cast<const int32_t*>(d)[-2];
}

// The release/adopt pair retail inlines at every `Filename x = y` site:
//     if (x.data != y.data) { release(x); x.data = y.data; if (x.data) ++rc; }
// Written out rather than folded into one helper call at each site because the
// `!=` guard is retail's own and skipping the whole block on a self-assign is
// observable (it is what keeps the refcount from hitting zero mid-assign).
inline void ReleaseThenAdopt(Filename* dst, char* src) {
    if (dst->data == src) return;
    RtString_ReleaseInline(AsStr(dst));
    RtString_AdoptInline(AsStr(dst), src);
}

typedef bool (*ExistsInPackfileFn)(const RtIO::FileSystem*, const Filename*);

inline bool ExistsInPackfile(const RtIO::FileSystem* fs, const Filename* f) {
    return RT_EXPORT_ONCE(ExistsInPackfileFn,
        "?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z")(fs, f);
}

// `?_singleton@FileSystem@IO@@0PEAV12@EA`, retail 0x1801fc6a8 — the qword
// `MOV R14,qword ptr [0x1801fc6a8]` at 0x1800bd5e3 loads. Read through the
// port-aware data door so it follows the mint at cutover, exactly as
// runtime/src/io/filesystem_packages.cpp does.
inline RtIO::FileSystem* FileSystemSingleton() {
    RtIO::FileSystem** slot =
        RT_DATA_SLOT(RtIO::FileSystem**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// ---------------------------------------------------------------------------
// THE NEGATIVE CONTROL.
//
// Rule 6: a probe that cannot be made to FAIL has proved nothing. Four
// perturbations, each one a single-token change to a different part of the
// body, each one expected to make the oracle diverge. `0` is the real body and
// is what every shipping and every measuring run uses.
//
// The self-test below drives all four in ONE process by setting this global
// around each call, so a negative control does not cost a game launch each.
// `BANISHED_RT_RESPATH_NEGCTL=<n>` also forces one for a live run.
// ---------------------------------------------------------------------------
int g_negCtl = 0;

const char* kUnderscore = "_";
const char* kDot        = ".";
const char* kColon      = ":";
const char* kCrs        = ".crs";
const char* kResource   = "resource";

}  // namespace

// ============================================================================
// ?GetPathExtension@Filename@System@@QEBA?AV12@XZ            retail 0x1800c64f0
// ============================================================================
//
//     Filename Filename::GetPathExtension() const
//
// Two backward scans over the buffer. The FIRST looks for ':' and, when it
// finds one past index 1, shortens the window to end just before it — that is
// what makes `"data/x.rsc:identifier"` yield `rsc` and not `identifier`. The
// SECOND looks for '.' inside the shortened window and the extension is
// everything after it.
//
// THREE EDGE CASES ARE RETAIL'S AND ARE KEPT:
//   * `CMP ECX,1 / JLE` — a ':' at index 0 or 1 does NOT shorten the window.
//   * the '.' scan runs over [0, end], so a '.' in a DIRECTORY name is found
//     when the filename itself has no extension.
//   * `LEA R8D,[RCX+1] / CMP R8D,R9D / JLE` — a trailing '.' (start > end)
//     returns the NULL Filename, not an empty one. Those two are different
//     values to every caller: `ExpandRelativePath` tests `data == null`.
// ============================================================================
Filename* RtSystem_Filename_GetPathExtension(const Filename* self, Filename* ret) {
    RT_DIFF_BEGIN("System::Filename::GetPathExtension");
    ++g_pc.extCalls;

    const char* const d = self->data;

    // 0x1800c6514: `MOV RDX,[RCX] ; TEST RDX,RDX ; JNZ`
    if (d == nullptr) {
        ret->data = nullptr;
        ++g_pc.extNullNoData;
        RT_DIFF_WORK(false);
        return ret;
    }

    // 0x1800c652c: `MOV R9D,[RDX-8] ; DEC R9D`
    int32_t end = BufLength(d) - 1;

    // 0x1800c6536-0x1800c654b: scan back for ':' from `end`. `c` and the index
    // move together, so `c` is the index of the ':' or -1 if there is none.
    int32_t c = end;
    for (int32_t i = end; i >= 0; --i) {
        if (d[i] == ':') break;
        --c;
    }

    // 0x1800c654d: `CMP ECX,1 ; JLE ; LEA R9D,[RCX-1]`
    if (c > 1) {
        end = c - 1;
        ++g_pc.extColonTrim;
    }

    // 0x1800c6556-0x1800c656c: the same scan for '.' over [0, end].
    c = end;
    for (int32_t i = end; i >= 0; --i) {
        if (d[i] == '.') break;
        --c;
    }

    // 0x1800c656e: `TEST ECX,ECX ; JNS`
    if (c < 0) {
        ret->data = nullptr;
        ++g_pc.extNoDot;
        RT_DIFF_WORK(false);
        return ret;
    }

    // 0x1800c657f: `LEA R8D,[RCX+1] ; CMP R8D,R9D ; JLE`
    const int32_t start = c + 1;
    if (start > end) {
        ret->data = nullptr;
        ++g_pc.extEmptyAfterDot;
        RT_DIFF_WORK(false);
        return ret;
    }

    // 0x1800c6595: SubString(self, &tmp, start, end) — inclusive at both ends,
    // which is what runtime/src/system/string.cpp:500 implements.
    String tmp{nullptr};
    RtSystem_String_SubString(AsStr(self), &tmp, start, end);

    ret->data = nullptr;                        // 0x1800c65a3
    RtString_AdoptInline(AsStr(ret), tmp.data); // 0x1800c65a6
    RtSystem_Filename_AdjustDelimiters(ret);    // 0x1800c65b4
    RtString_ReleaseInline(&tmp);               // 0x1800c65c5
    ++g_pc.extOk;

    // Pure: reads a const buffer, allocates its own. Safe to replay, and the
    // hidden return slot is a fresh temporary at every call site so ours and
    // theirs never alias (the trap string.cpp:515 guards against).
    if (rt_diff_on__) {
        Filename theirs{nullptr};
        RtOriginal<Filename* (*)(const Filename*, Filename*)>(
            "?GetPathExtension@Filename@System@@QEBA?AV12@XZ")(self, &theirs);
        const bool equal =
            ((ret->data == nullptr) == (theirs.data == nullptr)) &&
            (ret->data == nullptr || std::strcmp(ret->data, theirs.data) == 0);
        RT_DIFF_SHADOW(equal, ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RT_DIFF_WORK(true);
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
    }
    return ret;
}

// ============================================================================
// The two path-stack readers                     retail 0x1800bcf60/0x1800bcfd0
// ============================================================================
//
// Byte-identical bodies apart from the `ADD RDX,0x8` that selects the second
// half of the entry. Both return the TOP of the stack (`size - 1`) and both
// return a NULL Filename — not an empty one — when the stack is empty, which
// is the case `ExpandRelativePath` relies on to decide whether to substitute
// the stack's filename for a missing one.
//
// NO DIFF ORACLE, and the reason is specific rather than general: retail's body
// ends in the `Filename` COPY CONSTRUCTOR (0x1800067d0) on a live entry, so a
// replay bumps that entry's refcount twice and releases it twice. The
// arithmetic is balanced, but running a second copy of a refcount edit on live
// shared state to check a two-line accessor is the wrong trade — the evidence
// here is transcription plus `stackEmpty`/`stackNonEmpty`, which prove the body
// ran and which branch it took.
// ============================================================================
Filename* RtObject_ExternalFactory_GetPathStackFilename(const ExternalFactory* self,
                                                        Filename* ret) {
    // 0x1800bcf7c: `CMP dword ptr [RCX + 0x38],EAX` with EAX = 0
    if (self->_pathStack._size == 0) {
        ret->data = nullptr;                    // 0x1800bcf81
        ++g_pc.stackEmpty;
        return ret;
    }
    ++g_pc.stackNonEmpty;
    const PathStackEntry& top = self->_pathStack._data[self->_pathStack._size - 1];
    return RtSystem::RtSystem_Filename_CtorCopy(ret, &top.filename);
}

Filename* RtObject_ExternalFactory_GetPathStackDirectory(const ExternalFactory* self,
                                                         Filename* ret) {
    if (self->_pathStack._size == 0) {
        ret->data = nullptr;
        ++g_pc.stackEmpty;
        return ret;
    }
    ++g_pc.stackNonEmpty;
    const PathStackEntry& top = self->_pathStack._data[self->_pathStack._size - 1];
    return RtSystem::RtSystem_Filename_CtorCopy(ret, &top.directory);
}

// ============================================================================
// ?ExpandRelativePath@ExternalFactory@Object@@                retail 0x1800bd360
// ============================================================================
//
//     Filename ExternalFactory::ExpandRelativePath(const Filename& path,
//                                                  bool appendIdentifier) const
//
// WHAT IT COMPUTES, in one paragraph, because the 834 instructions are ~85%
// refcount boilerplate around six string concatenations:
//
//   Split `path` into extension / identifier / filename / directory. An empty
//   identifier defaults to "resource"; an empty filename is taken from the TOP
//   OF THE PATH STACK (this is what makes a bare ":identifier" resolve against
//   the file currently being parsed). Build the absolute candidate
//   `<dataDir><stackDir><dir><file>_<ident>.crs` and ask the filesystem whether
//   it exists. If it does, the answer is the STACK-QUALIFIED relative path
//   `<stackDir><dir><file>.<ext>`; if it does not, the answer is the bare
//   `<dir><file>.<ext>`. Append `:<ident>` when asked. Normalise, return.
//
// TWO THINGS THAT LOOK LIKE TRANSCRIPTION ERRORS AND ARE NOT
//
//  1. THE SECOND CANDIDATE IS THE SAME STRING AS THE FIRST. The `else` arm at
//     0x1800bd825 rebuilds `<dataDir><stackDir><dir><file>_<ident>.crs` operand
//     for operand — same six sources in the same order, same two literals — and
//     probes it again. It is not a different path. What differs is the PROBE:
//     the first `ExistsInPackfile` at 0x1800bd7c1 is behind
//     `fs->existsInPackfileGate` (it is MSVC's inlining of `FileSystem::Exists`,
//     gate and all), and the second at 0x1800bd8fb is UNGATED. So the retry is
//     reachable, and on a build with the gate clear it is the only packfile
//     probe that runs. Kept literally, allocations and all.
//
//  2. `this` IS DEAD AFTER 0x1800bd5f1. Retail reuses R15 for the
//     GetFileAttributesW result. `GetPathStackDirectory` is the last read of
//     the factory, which is why nothing below touches `self`.
//
// THE ORACLE. Replayable: the body reads the factory and the filesystem and
// writes only its own allocations. `RT_DIFF_SHADOW` compares the returned TEXT
// (the two buffers are always distinct pointers). The original is reached
// through the DETOUR TRAMPOLINE, never through GetProcAddress — see the
// `RtResPathTrampoline` note at the top of this file.
// ============================================================================
Filename* RtObject_ExternalFactory_ExpandRelativePath(const ExternalFactory* self,
                                                      Filename* ret,
                                                      const Filename* path,
                                                      bool appendIdentifier) {
    RT_DIFF_BEGIN("Object::ExternalFactory::ExpandRelativePath");
    ResLoadGuard resGuard;   // see THE RESOURCE-LOAD LOCK at the top
    ++g_pc.expandCalls;

    // ---- 0x1800bd3a3: `if (path->data == null || length == 0) return null` --
    if (path->data == nullptr || BufLength(path->data) == 0) {
        ret->data = nullptr;                          // 0x1800bde7b
        ++g_pc.expandEmptyPath;
        RT_DIFF_WORK(false);
        return ret;
    }

    const int ctl = g_negCtl;

    // ---- 0x1800bd3b9-0x1800bd3ea: the four splits ---------------------------
    // Retail hands these four uninitialised stack slots; every callee writes
    // `*ret` before it reads it. They are zeroed here so that a future callee
    // change cannot turn a missed write into a wild release.
    Filename ext{nullptr};    // [RBP-0x29]
    Filename ident{nullptr};  // [RBP-0x41]
    Filename fname{nullptr};  // [RBP+0x77]
    Filename dir{nullptr};    // [RBP+0x07]
    RtSystem_Filename_GetPathExtension(path, &ext);
    RtSystem_Filename_GetPathIdentifer(path, &ident);
    RtSystem_Filename_GetPathFilename(path, &fname, true);
    RtSystem_Filename_GetPathDirectory(path, &dir);

    // ---- 0x1800bd3f0-0x1800bd497: an empty identifier defaults to "resource"
    if (ident.data == nullptr || BufLength(ident.data) == 0) {
        ++g_pc.identDefaulted;
        Filename tmp{nullptr};                                   // [RBP-0x61]
        RtSystem_String_Set(AsStr(&tmp), kResource);             // 0x1800bd412
        RtSystem_Filename_AdjustDelimiters(&tmp);                // 0x1800bd418
        ReleaseThenAdopt(&ident, tmp.data);                      // 0x1800bd422
        RtSystem_Filename_AdjustDelimiters(&ident);              // 0x1800bd468
        RtString_ReleaseInline(AsStr(&tmp));                     // 0x1800bd46e
    } else {
        ++g_pc.identPresent;
    }

    // ---- 0x1800bd497-0x1800bd5e3: an empty filename comes off the stack -----
    // BOTH halves are replaced: the filename AND the extension. A resource
    // reference that is only an identifier therefore inherits the extension of
    // the file being parsed, not the one it was written next to.
    if (fname.data == nullptr || BufLength(fname.data) == 0) {
        ++g_pc.filenameFromStack;
        Filename stackFile{nullptr};                             // [RBP-0x59]
        RtObject_ExternalFactory_GetPathStackFilename(self, &stackFile);

        Filename tmp{nullptr};                                   // [RBP-0x61]
        RtSystem_Filename_GetPathFilename(&stackFile, &tmp, true);
        ReleaseThenAdopt(&fname, tmp.data);                      // 0x1800bd4ca
        RtSystem_Filename_AdjustDelimiters(&fname);              // 0x1800bd507
        RtString_ReleaseInline(AsStr(&tmp));                     // 0x1800bd511

        // 0x1800bd53a hands GetPathExtension the slot it just released. Retail
        // does exactly this and it is safe because the callee writes `*ret`
        // before reading it; transcribed rather than re-zeroed so the shape
        // stays the shape.
        RtSystem_Filename_GetPathExtension(&stackFile, &tmp);
        ReleaseThenAdopt(&ext, tmp.data);                        // 0x1800bd54a
        RtSystem_Filename_AdjustDelimiters(&ext);                // 0x1800bd587
        RtString_ReleaseInline(AsStr(&tmp));                     // 0x1800bd591
        RtString_ReleaseInline(AsStr(&stackFile));               // 0x1800bd5ba
    } else {
        ++g_pc.filenamePresent;
    }

    // ---- 0x1800bd5e3: R14 = IO::FileSystem::_singleton ---------------------
    const RtIO::FileSystem* const fs = FileSystemSingleton();

    Filename stackDir{nullptr};                                  // [RBP+0x0f]
    RtObject_ExternalFactory_GetPathStackDirectory(self, &stackDir);

    // ---- 0x1800bd5f7-0x1800bd68d: candidate =
    //      <dataDir><stackDir><dir><fname>_<ident>.crs
    Filename candidate{nullptr};                                 // [RBP-0x01]
    {
        Filename dataDir{nullptr};                               // [RBP-0x39]
        RtIO::RtIo_FileSystem_GetDataDirectory(fs, &dataDir);

        String t1{nullptr}, t2{nullptr}, t3{nullptr};            // -0x19/-0x11/-0x09
        String t4{nullptr}, t5{nullptr}, t6{nullptr};            // -0x21/-0x61/-0x59
        RtSystem_String_Concat(&t1, AsStr(&dataDir), AsStr(&stackDir));
        RtSystem_String_Concat(&t2, &t1, AsStr(&dir));
        RtSystem_String_Concat(&t3, &t2, AsStr(&fname));
        RtSystem_String_ConcatCStr(&t4, &t3, ctl == 1 ? "-" : kUnderscore);
        RtSystem_String_Concat(&t5, &t4, AsStr(&ident));
        RtSystem_String_ConcatCStr(&t6, &t5, kCrs);

        RtString_AdoptInline(AsStr(&candidate), t6.data);        // 0x1800bd670
        RtSystem_Filename_AdjustDelimiters(&candidate);          // 0x1800bd683

        RtString_ReleaseInline(&t6);                             // 0x1800bd68d
        RtString_ReleaseInline(&t5);
        RtString_ReleaseInline(&t4);
        RtString_ReleaseInline(&t3);
        RtString_ReleaseInline(&t2);
        RtString_ReleaseInline(&t1);
        RtString_ReleaseInline(AsStr(&dataDir));                 // 0x1800bd783
    }

    // ---- 0x1800bd7ac-0x1800bdab5: does it exist? ---------------------------
    bool found = false;
    bool decided = false;

    // MSVC's inlined FileSystem::Exists(candidate), gate and all.
    if (fs->existsInPackfileGate != 0 && ExistsInPackfile(fs, &candidate)) {
        found = true; decided = true;                            // 0x1800bd7c8
        ++g_pc.hitPackfile;
    }
    if (!decided && fs->platformExistsGate != 0) {
        WideString ws{nullptr};                                  // [RBP-0x39]
        RtSystem_WideString_CtorFromString(&ws, AsStr(&candidate));
        // `CMP EAX,-1 ; SETNZ` — INVALID_FILE_ATTRIBUTES, not a HRESULT.
        const bool ok = (::GetFileAttributesW(ws.data) != INVALID_FILE_ATTRIBUTES);
        RtString_ReleaseInline(reinterpret_cast<String*>(&ws));  // 0x1800bd7f3
        if (ok) {
            found = true; decided = true;                        // 0x1800bd81f
            ++g_pc.hitAttributes;
        }
    }
    if (!decided) {
        // 0x1800bd825: the SAME string, built again, probed UNGATED. See note 1.
        Filename candidate2{nullptr};                            // [RBP-0x59]
        Filename dataDir2{nullptr};                              // [RBP-0x49]
        RtIO::RtIo_FileSystem_GetDataDirectory(fs, &dataDir2);

        String u1{nullptr}, u2{nullptr}, u3{nullptr};            // -0x51/-0x31/-0x21
        String u4{nullptr}, u5{nullptr}, u6{nullptr};            // -0x09/-0x11/-0x19
        RtSystem_String_Concat(&u1, AsStr(&dataDir2), AsStr(&stackDir));
        RtSystem_String_Concat(&u2, &u1, AsStr(&dir));
        RtSystem_String_Concat(&u3, &u2, AsStr(&fname));
        RtSystem_String_ConcatCStr(&u4, &u3, kUnderscore);
        RtSystem_String_Concat(&u5, &u4, AsStr(&ident));
        RtSystem_String_ConcatCStr(&u6, &u5, kCrs);

        RtString_AdoptInline(AsStr(&candidate2), u6.data);       // 0x1800bd8cf
        RtSystem_Filename_AdjustDelimiters(&candidate2);         // 0x1800bd8e2

        found = ExistsInPackfile(fs, &candidate2);               // 0x1800bd8fb
        if (found) ++g_pc.hitPackfileRetry; else ++g_pc.notFound;

        // 0x1800bd90c-0x1800bdab5: retail unwinds these through an EDI bitmask
        // recording which of them had been constructed by the time the probe
        // ran. Scoping them here is the same set in the same order.
        RtString_ReleaseInline(AsStr(&candidate2));
        RtString_ReleaseInline(&u6);
        RtString_ReleaseInline(&u5);
        RtString_ReleaseInline(&u4);
        RtString_ReleaseInline(&u3);
        RtString_ReleaseInline(&u2);
        RtString_ReleaseInline(&u1);
        RtString_ReleaseInline(AsStr(&dataDir2));
    }

    // ---- 0x1800bdab5-0x1800bdc8c: the answer -------------------------------
    Filename result{nullptr};   // RBX / [RBP-0x61]
    if (found) {
        // 0x1800bdabe: <stackDir><dir><fname>.<ext>
        String a{nullptr}, b{nullptr}, c{nullptr}, d{nullptr};
        RtSystem_String_Concat(&a, AsStr(ctl == 3 ? &dir : &stackDir),
                                   AsStr(ctl == 3 ? &stackDir : &dir));
        RtSystem_String_Concat(&b, &a, AsStr(&fname));
        RtSystem_String_ConcatCStr(&c, &b, ctl == 2 ? "!" : kDot);
        RtSystem_String_Concat(&d, &c, AsStr(&ext));
        RtString_AdoptInline(AsStr(&result), d.data);            // 0x1800bdb06
        RtString_ReleaseInline(&d);
        RtString_ReleaseInline(&c);
        RtString_ReleaseInline(&b);
        RtString_ReleaseInline(&a);
    } else {
        // 0x1800bdbc8: <dir><fname>.<ext> — the stack directory is DROPPED.
        String a{nullptr}, b{nullptr}, c{nullptr};
        RtSystem_String_Concat(&a, AsStr(&dir), AsStr(&fname));
        RtSystem_String_ConcatCStr(&b, &a, ctl == 2 ? "!" : kDot);
        RtSystem_String_Concat(&c, &b, AsStr(&ext));
        RtString_AdoptInline(AsStr(&result), c.data);            // 0x1800bdbff
        RtString_ReleaseInline(&c);
        RtString_ReleaseInline(&b);
        RtString_ReleaseInline(&a);
    }

    // ---- 0x1800bdc8c-0x1800bdd43: `if (appendIdentifier) result += ":" + ident`
    if (appendIdentifier && ctl != 4) {
        ++g_pc.appendedIdentifier;
        String x{nullptr}, y{nullptr};                           // -0x51 / -0x49
        RtSystem_String_ConcatCStr(&x, AsStr(&result), kColon);
        RtSystem_String_Concat(&y, &x, AsStr(&ident));
        ReleaseThenAdopt(&result, y.data);                       // 0x1800bdcbd
        RtString_ReleaseInline(&y);
        RtString_ReleaseInline(&x);
    } else {
        ++g_pc.noIdentifierTail;
    }

    // ---- 0x1800bdd43-0x1800bdd59: publish, normalise ------------------------
    ret->data = nullptr;                                         // 0x1800bdd43
    RtString_AdoptInline(AsStr(ret), result.data);               // 0x1800bdd4b
    RtSystem_Filename_AdjustDelimiters(ret);                     // 0x1800bdd51

    // ---- 0x1800bdd60-0x1800bde76: the tail releases, retail's order ---------
    RtString_ReleaseInline(AsStr(&result));
    RtString_ReleaseInline(AsStr(&candidate));
    RtString_ReleaseInline(AsStr(&stackDir));
    RtString_ReleaseInline(AsStr(&dir));
    RtString_ReleaseInline(AsStr(&fname));
    RtString_ReleaseInline(AsStr(&ident));
    RtString_ReleaseInline(AsStr(&ext));

    // The oracle. `theirs` is the DETOUR TRAMPOLINE — retail's displaced
    // prologue followed by a jump back past our patch — so this calls the
    // ORIGINAL body even though our code owns the address. With no detour
    // installed there is no way to reach the original at all (GetProcAddress
    // would return our own body once the export is registered), and the site
    // correctly reports zero comparisons rather than comparing ours to ours
    // (reference_converting_a_door_is_not_satisfying_it.md).
    if (rt_diff_on__) {
        typedef Filename* (*ExpandFn)(const ExternalFactory*, Filename*,
                                      const Filename*, bool);
        ExpandFn theirs = reinterpret_cast<ExpandFn>(RtResPathTrampoline());
        if (theirs) {
            Filename mirror{nullptr};
            theirs(self, &mirror, path, appendIdentifier);
            const bool equal =
                ((ret->data == nullptr) == (mirror.data == nullptr)) &&
                (ret->data == nullptr || std::strcmp(ret->data, mirror.data) == 0);
            RT_DIFF_SHADOW(equal, ret->data ? ret->data : "(null)",
                           mirror.data ? mirror.data : "(null)");
            RT_DIFF_WORK(true);
            RtString_ReleaseInline(AsStr(&mirror));
        }
    }
    return ret;
}

// ============================================================================
// The self-test: five controls in one process
// ============================================================================
//
// `BANISHED_RT_RESPATH_SELFTEST=1`. Drives our body and the ORIGINAL body over
// a fixed script of inputs, once with the real code and once for each of the
// four perturbations, and reports how many of the four made the comparison
// FAIL. A control that does not fire is reported as a MISS, not explained away
// (reference_probe_needs_an_effectiveness_control.md).
//
// It runs after registration and before the first resource load, on the main
// thread, so the path stack is empty and the identifier/filename defaults are
// the branches it can reach. It is NOT a substitute for the live diff — it
// cannot reach `filenameFromStack` or `hitPackfile` — and the write-up says so.
// ============================================================================
namespace {

const char* const kSelfTestPaths[] = {
    "Terrain/Grass.rsc",
    "Terrain/Grass.rsc:material",
    "Dialog/MainMenu.rsc",
    "Models/House/House.rsc:mesh",
    "NoExtensionHere",
    ":identifierOnly",
    "Deep/Nested/Path/Thing.png",
};

bool RunOneSelfTestCase(const ExternalFactory* factory, const char* text,
                        bool appendIdentifier, int ctl, bool* comparedOut) {
    Filename in{nullptr};
    RtSystem_String_Set(AsStr(&in), text);
    RtSystem_Filename_AdjustDelimiters(&in);

    typedef Filename* (*ExpandFn)(const ExternalFactory*, Filename*,
                                  const Filename*, bool);
    ExpandFn theirs = reinterpret_cast<ExpandFn>(RtResPathTrampoline());
    if (!theirs) { *comparedOut = false; RtString_ReleaseInline(AsStr(&in)); return true; }

    Filename ours{nullptr}, mirror{nullptr};
    g_negCtl = ctl;
    RtObject_ExternalFactory_ExpandRelativePath(factory, &ours, &in, appendIdentifier);
    g_negCtl = 0;
    theirs(factory, &mirror, &in, appendIdentifier);

    const bool equal =
        ((ours.data == nullptr) == (mirror.data == nullptr)) &&
        (ours.data == nullptr || std::strcmp(ours.data, mirror.data) == 0);

    if (!equal && ctl == 0) {
        std::printf("[ResPath]   MISMATCH on \"%s\" append=%d: ours=\"%s\" theirs=\"%s\"\n",
                    text, appendIdentifier ? 1 : 0,
                    ours.data ? ours.data : "(null)",
                    mirror.data ? mirror.data : "(null)");
    }

    RtString_ReleaseInline(AsStr(&mirror));
    RtString_ReleaseInline(AsStr(&ours));
    RtString_ReleaseInline(AsStr(&in));
    *comparedOut = true;
    return equal;
}

void RunSelfTest() {
    ExternalFactory** slot =
        RT_DATA_SLOT(ExternalFactory**, "?_singleton@ExternalFactory@Object@@0PEAV12@EA");
    ExternalFactory* factory = slot ? *slot : nullptr;
    if (!factory) {
        std::printf("[ResPath] SELFTEST SKIPPED -- no ExternalFactory singleton yet; "
                    "NO COMPARISON WAS MADE\n");
        return;
    }
    if (!RtResPathTrampoline()) {
        std::printf("[ResPath] SELFTEST SKIPPED -- no detour trampoline, so the "
                    "original body is unreachable; NO COMPARISON WAS MADE\n");
        return;
    }

    const int nPaths = static_cast<int>(sizeof(kSelfTestPaths) / sizeof(kSelfTestPaths[0]));
    int compared = 0, mismatches = 0;
    int ctlFired[5] = { 0, 0, 0, 0, 0 };

    for (int ctl = 0; ctl <= 4; ++ctl) {
        for (int i = 0; i < nPaths; ++i) {
            for (int ap = 0; ap <= 1; ++ap) {
                bool did = false;
                const bool eq = RunOneSelfTestCase(factory, kSelfTestPaths[i],
                                                   ap != 0, ctl, &did);
                if (!did) continue;
                if (ctl == 0) { ++compared; if (!eq) ++mismatches; }
                else if (!eq) ++ctlFired[ctl];
            }
        }
    }

    std::printf("[ResPath] SELFTEST -- %d real comparisons, %d mismatches\n",
                compared, mismatches);
    int fired = 0;
    for (int c = 1; c <= 4; ++c) if (ctlFired[c] > 0) ++fired;
    std::printf("[ResPath]   negative controls: %d of 4 FIRED "
                "(ctl1 '_'->'-': %d, ctl2 '.'->'!': %d, ctl3 dir/stackDir swap: %d, "
                "ctl4 drop ':'+ident: %d)\n",
                fired, ctlFired[1], ctlFired[2], ctlFired[3], ctlFired[4]);
    if (fired != 4)
        std::printf("[ResPath]   *** UNDERPOWERED -- %d control(s) did NOT fire; the "
                    "script cannot reach the code they perturb.\n", 4 - fired);
    std::printf("[ResPath]   VERDICT: %s\n",
                (mismatches == 0 && fired == 4 && compared > 0)
                    ? "PASS" : "NOT A PASS -- see the lines above.");
}

void DumpCounters() {
    std::printf("[ResPath] counters: expand=%llu empty=%llu | ident def/present=%llu/%llu "
                "| fname stack/present=%llu/%llu | stack empty/nonempty=%llu/%llu\n",
                (unsigned long long)g_pc.expandCalls,
                (unsigned long long)g_pc.expandEmptyPath,
                (unsigned long long)g_pc.identDefaulted,
                (unsigned long long)g_pc.identPresent,
                (unsigned long long)g_pc.filenameFromStack,
                (unsigned long long)g_pc.filenamePresent,
                (unsigned long long)g_pc.stackEmpty,
                (unsigned long long)g_pc.stackNonEmpty);
    std::printf("[ResPath] counters: hit pkg/attr/retry=%llu/%llu/%llu notFound=%llu "
                "| tail ident/none=%llu/%llu\n",
                (unsigned long long)g_pc.hitPackfile,
                (unsigned long long)g_pc.hitAttributes,
                (unsigned long long)g_pc.hitPackfileRetry,
                (unsigned long long)g_pc.notFound,
                (unsigned long long)g_pc.appendedIdentifier,
                (unsigned long long)g_pc.noIdentifierTail);
    std::printf("[ResPath] counters: ext calls=%llu ok=%llu nodata=%llu nodot=%llu "
                "emptyAfterDot=%llu colonTrim=%llu\n",
                (unsigned long long)g_pc.extCalls,
                (unsigned long long)g_pc.extOk,
                (unsigned long long)g_pc.extNullNoData,
                (unsigned long long)g_pc.extNoDot,
                (unsigned long long)g_pc.extEmptyAfterDot,
                (unsigned long long)g_pc.extColonTrim);
    std::fflush(stdout);
}

}  // namespace

}  // namespace RtObject

// ============================================================================
// Registration
//
// Global scope, like every other wave's Register function, because
// runtime/src/object/rt_object_port.cpp declares it with a bare
// `extern int RtObjectExternalPathRegister();`.
// ============================================================================
int RtObjectExternalPathRegister() {
    using namespace RtObject;
    int n = 0;

    // Literal mangled names, NOT constants: tools/runtime_census.py matches
    // `RegisterRuntimeExport("<literal>"` and cannot see through a named
    // constant (reference_census_cannot_see_nonliteral_registrations.md).
    RegisterRuntimeExport("?GetPathExtension@Filename@System@@QEBA?AV12@XZ",
                          (void*)&RtSystem_Filename_GetPathExtension); ++n;
    RegisterRuntimeExport("?GetPathStackFilename@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtObject_ExternalFactory_GetPathStackFilename); ++n;
    RegisterRuntimeExport("?GetPathStackDirectory@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtObject_ExternalFactory_GetPathStackDirectory); ++n;
    RegisterRuntimeExport("?ExpandRelativePath@ExternalFactory@Object@@QEBA?AVFilename@System@@AEBV34@_N@Z",
                          (void*)&RtObject_ExternalFactory_ExpandRelativePath); ++n;

    RegisterRuntimeAddr(0x1800c64f0, (void*)&RtSystem_Filename_GetPathExtension);
    RegisterRuntimeAddr(0x1800bcf60, (void*)&RtObject_ExternalFactory_GetPathStackFilename);
    RegisterRuntimeAddr(0x1800bcfd0, (void*)&RtObject_ExternalFactory_GetPathStackDirectory);
    RegisterRuntimeAddr(0x1800bd360, (void*)&RtObject_ExternalFactory_ExpandRelativePath);

    // The live-coverage seam. OPT-IN: nothing goes default-on unproven
    // (standalone/runtime_detour.h, "every detour gets an env kill-switch").
    // With it off this port has no caller and honestly reports calls=0.
    bool detoured = false;
    if (RtWaveEnabled("BANISHED_RT_RESPATH_DETOUR") &&
        std::getenv("BANISHED_RT_RESPATH_DETOUR") != nullptr) {
        detoured = RtResPathInstallDetour(
            (void*)&RtObject_ExternalFactory_ExpandRelativePath);
    }

    if (const char* e = std::getenv("BANISHED_RT_RESPATH_NEGCTL"))
        g_negCtl = std::atoi(e);

    std::printf("[ResPath] registered %d doors + 4 addresses (detour=%s negctl=%d)\n",
                n, detoured ? "ON" : "off", g_negCtl);
    std::fflush(stdout);

    if (std::getenv("BANISHED_RT_RESPATH_SELFTEST") != nullptr) RunSelfTest();
    std::atexit(&DumpCounters);
    return n;
}
