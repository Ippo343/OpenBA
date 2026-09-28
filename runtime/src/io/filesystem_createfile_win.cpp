// ============================================================================
// runtime/src/io/filesystem_createfile_win.cpp — Wave 19 lane FILE
// IO::FileSystem CreatePath / CreateFileW / PlatformCreateFile / CloseFile /
//                PlatformCloseFile / ~FileSystem
// ============================================================================
//
//   | Export                                                | Retail      | Ord  | Instrs |
//   |-------------------------------------------------------|-------------|------|--------|
//   | ?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@…@Z     | 0x1800ae430 | 2130 |  105   |
//   | ?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z     | 0x1800accc0 | 1726 |   50   |
//   | ??1FileSystem@IO@@QEAA@XZ                             | 0x1800ac6d0 |  594 |   50   |
//   | ?CreateFileW@FileSystem@IO@@QEAA?AUFileInfo@12@AEB…@Z | 0x1800acc50 | —    |   17   |
//   | ?PlatformCreateFile@FileSystem@IO@@AEAA_N…@Z          | 0x1800ade60 | 3357 |  ~95   |
//   | ?PlatformCloseFile@FileSystem@IO@@AEAAX…@Z            | 0x1800ae020 | —    |   62   |
//
//
// ---------------------------------------------------------------------------
// THE LAST THREE ROWS ARE NOT ON THE WAVE 19 ROW LIST, AND THAT IS THE POINT
// ---------------------------------------------------------------------------
// Lane FILE's brief names eight rows and its acceptance criterion is the
// SCREENSHOT CUTOVER HOLE, not the row count. Walking the call graph out of the
// two rows that ARE on the list found three more bodies between them and Win32,
// none of them ported:
//
//     ?Create@FileStream@       -> ?CreateFileW@FileSystem@   -> ?PlatformCreateFile@
//     ?CloseFile@FileSystem@    -> ?PlatformCloseFile@
//
// Porting only the named rows would have left `ResolveExport` falling through
// to the DLL at each of those three, and the DLL-absent screenshot would still
// have died — one link further down than before. `called` grows by these; that
// is DEFERRED WORK BEING SURFACED, not invented work, and it is exactly what
// Wave 18 lane VRES said about the rows that produced this lane.
//
// Two of the three are `AEAA` — PRIVATE, not unexported. This DLL exports its
// private members and `?PlatformCreateFile@` is ordinal 3357
// (`reference_aeaa_is_private_not_unexported.md`).
//
// ---------------------------------------------------------------------------
// THE FACT THAT DECIDES ALL OF THIS: A WRITE GOES TO %TEMP% FIRST
// ---------------------------------------------------------------------------
// `?PlatformCreateFile@` does NOT open the caller's path. It calls
// `GetTempPathW`, `GetTempFileNameW`, and opens THAT — then stores the temp
// path in `FileInfo::_name0` (+0x20) and the CALLER'S path in `_name1` (+0x28).
// `?PlatformCloseFile@` then closes the handle, `DeleteFileW`s the destination
// and `MoveFileW`s the temp over it.
//
// So the pair is an ATOMIC-REPLACE, and three consequences follow that a
// signature or a decompile summary would each get wrong:
//
//   1. **Nothing appears at the caller's path until CLOSE.** A screenshot that
//      is written but never closed leaves a stray file in %TEMP% and no BMP.
//      `~FileStream` is what closes it, which is why that row and this one had
//      to land in the same lane.
//   2. **`_name0` is the TEMP path and `_name1` is the FINAL path**, not the
//      other way round. `MoveFileW`'s arguments settle it: at 0x1800ae0d7/0da
//      RDX comes from the WideString built over `_name1` and RCX from the one
//      built over `_name0`, and MoveFileW's first argument is the EXISTING
//      file. Two same-typed adjacent String fields are otherwise a coin flip
//      (FileStream.h records losing exactly that coin flip once on
//      TextFileStream's `objectName`/`objectType` pair).
//   3. **The rename is skipped when `_name0` has ZERO LENGTH**, not merely when
//      it is null. 0x1800ae065 is `CMP dword ptr [RAX-0x8],0x0` — the String
//      block's LENGTH field, at -8, not the refcount at -4.
//
// ---------------------------------------------------------------------------
// `?CreatePath@` RETURNS TRUE UNCONDITIONALLY
// ---------------------------------------------------------------------------
// Its epilogue is `MOV AL,0x1` on the ONLY path out (0x1800ae637); every
// `CreateDirectoryW` result is discarded. It is not a status, and a port that
// propagated the Win32 return would diverge on every path that already exists.
// `?Create@FileStream@` discards it too.
//
// PORTABILITY: this file is `_win` BECAUSE IT GENUINELY IS (PLAN.md §4.1). The
// six Win32 entry points below were read out of Runtime-x64.dll's OWN import
// table with `pefile` — `tools/dispatch_slot_map.py` is for the EXE's imports:
//
//     0x18014b0c8 CreateDirectoryW   0x18014b340 GetTempPathW
//     0x18014b338 GetTempFileNameW   0x18014b320 CreateFileW
//     0x18014b3b8 CloseHandle        0x18014b330 DeleteFileW
//     0x18014b100 MoveFileW
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/FileStream.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"
#include "../../types/system/WideString.h"
#include "../../types/io/Modification.h"   // RtColl_ArrayFilenameDtor_1800098d0
#include "../collection/avl_trees.h"      // rtavl::SubtreeFree

#include <new>        // placement new into the retail hidden return slot
#include <utility>    // std::move -- the pointer steal out of a POD out-param

// ENDGAME B.3.1 step 2: the two lane-String entry points this file
// forward-declared here — `RtSystem_Filename_AdjustDelimiters` (0x1800c6970)
// and `RtSystem_Filename_Assign` (0x180006770) — are gone from this
// translation unit. The second is `Filename::operator=`, and every call to the
// first was the tail of a mutating Filename entry point, which the class
// spells as itself with AdjustDelimiters private inside it.

// Rung 4 of Wave 9 lane PKG2, already ported and registered.
extern "C" RtIO::FileInfo* RtIo_FileInfo_Assign(RtIO::FileInfo*, const RtIO::FileInfo*);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)
using RtSystem::SysFile;  //   "

namespace {

// ---- the two System::WideString entry points this file needs ---------------
// Both are EXPORTED and both are PORTED (runtime/src/system/widestring.cpp);
// B3R RT-IO: both are called by name on System::WideString.
//
// `??0WideString@System@@QEAA@AEBVString@1@@Z` RETURNS BY VALUE — RCX is the
// hidden return slot and RDX is the String. Getting that typedef wrong is a
// wild write that still returns the right answer
// (`reference_hidden_retslot_wild_write.md`).
//
// A System::WideString is one pointer, exactly like a String, and the release
// idiom on it is the same (refcount at -4, block at -8). Retail inlines that
// idiom at all four sites below rather than calling ~WideString.
struct WideStr { wchar_t* data; };
WideStr* WideFromString(WideStr* ret, const System::String* s) {
    ::new (static_cast<void*>(ret)) System::WideString(*s);   // B.3.x bridge
    return ret;
}

// The inlined WideString release. Same asymmetry as the String one: the
// freeing branch leaves the slot holding `data - 8`, the non-freeing branch
// nulls it. Reproduced, not tidied.
void WideRelease(WideStr* w) {
    if (!w->data) return;
    int32_t* rc = reinterpret_cast<int32_t*>(w->data) - 1;
    if (--*rc == 0) {
        w->data = reinterpret_cast<wchar_t*>(
            reinterpret_cast<char*>(w->data) - 8);
        RtSystem::RtMemFree(w->data);
    } else {
        w->data = nullptr;
    }
}

// ?SubString@String@System@@QEBA?AV12@HH@Z — ported (string.cpp), returns by
// value, so RCX = this, RDX = the return slot, R8/R9 the two ints.
typedef String* (*SubStringFn)(const String* self, String* ret, int a, int b);

// Collection::Array<T>::~Array for a POD element, retail 0x1800097f0.
// `if (data) MemFree(data); data = 0; size = capacity = 0;`
//
// TRANSCRIBED AS A FILE-LOCAL HELPER AND DELIBERATELY *NOT* REGISTERED. It is a
// Collection-namespace body, not IO's, and this lane does not mint rows in
// another namespace's census. Nothing else in the tree reaches 0x1800097f0
// (grepped), so calling our own copy directly leaves no door open at cutover;
// if a Collection lane later ports and registers it, this call site is
// unaffected because it never went through a door.
struct PodArray { void* data; int32_t size; int32_t capacity; };
void PodArrayDtor(PodArray* a) {
    if (a->data) RtSystem::RtMemFree(a->data);   // 180009814..44 (the locked free)
    a->data = nullptr;                           // 18000984c
    a->size = 0; a->capacity = 0;                // 18000984f (one qword store)
}

// Collection::Array<Filename>::~Array, retail 0x1800098d0, is
// RtIO::RtColl_ArrayFilenameDtor_1800098d0 (modification.cpp); the unexported
// AVL subtree free at 0x180008340 is rtavl::SubtreeFree (avl_trees.cpp). Both
// are called by name (B3R RT-IO).
//
// ENDGAME B.3.2 step 2 (2026-09-13) — NOT CONVERTED to `Collection::Set<T>`,
// deliberately. `FileSystem::packfileIndexRoot`/`packfileIndexCount` (+0x40/
// +0x48) ARE a `Collection::Set<T>` header in every way that matters here, and
// 0x180008340 IS `Set<T>::DestroySubtree` (avl_trees.cpp names it: "the
// recursive subtree free"). What blocks the conversion:
//   1. the element (`PackfileIndexNode`'s {key, packIndex, fileIndex}) is
//      released ASYMMETRICALLY — only the `key` slot (a raw `const char*`, not
//      a `System::String`) is `Release()`d, the packed {packIndex,fileIndex}
//      qword is not. `Set<T>::DestroySubtree` calls `n->element.~T()`, a real
//      C++ destructor over the WHOLE element; expressing "release only one
//      raw-pointer field" would need a hand-written element type with a
//      matching destructor, which is new class design, not a call-site swap.
//   2. `packfileIndexRoot`/`Count` are two loose fields, not a typed member —
//      other files reach the same tree by raw node pointer (`node[3]`/
//      `node[4]` walks in filesystem.cpp, filesystem_find_win.cpp,
//      filesystem_openfile_win.cpp, filesystem_packages.cpp), none of which
//      are in this batch, so retyping the header field is out of scope.
// The member that would be needed: `Collection::Set<PackfileIndexElem>` where
// `PackfileIndexElem`'s destructor implements the key-only String release
// (`StringReleaseInline`), once that type exists and the other four files'
// raw walks are converted alongside it.

}  // namespace

// ============================================================================
// ATTENDANCE COUNTERS - see the note in file_stream.cpp. `calls=` is the half
// the static door screen cannot answer, and a registered-but-never-entered body
// looks identical to a working one without it.
//
// `createOk` / `createFail` and `closeRenamed` are EFFECTIVENESS counters, not
// call counts: they say which BRANCH ran. `calls=N worked=0` is the shape that
// means the body ran every time and always took the arm nothing covers
// (`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).
// ============================================================================
struct FsCreateCounters {
    uint64_t createPath, createPathDirs;
    uint64_t closeFile, platClose, closeRenamed;
    uint64_t createFileW, platCreate, createOk, createFail;
    uint64_t fsDtor;
};
FsCreateCounters g_fcc{};

// ============================================================================
// IO::FileSystem::CreatePath(Filename const&) const       retail 0x1800ae430
// ============================================================================
// Walk the path's characters; at every `\` or `/`, take the prefix up to (but
// not including) that separator and `CreateDirectoryW` it. Returns TRUE
// UNCONDITIONALLY — see the header note.
//
// FOUR DETAILS THE DISASSEMBLY SETTLES:
//
//  1. THE LENGTH COMES FROM THE STRING BLOCK, NOT strlen. 0x1800ae459 is
//     `MOV EAX,[RAX-0x8]` — the block's length field — and a NULL `data` yields
//     length 0 and no iterations at all (0x1800ae45e).
//  2. `SubString(0, i)` IS EXCLUSIVE OF THE SEPARATOR. R8D is 0 and R9D is the
//     loop counter EBP, which is the index OF the separator, so the directory
//     created for `a\b\c` is `a`, then `a\b` — never a path ending in `\`.
//  3. `AdjustDelimiters` RUNS ON THE COPY, NOT ON THE SUBSTRING. Retail builds
//     the substring, ADOPTS it into a second String slot with an explicit
//     refcount increment (0x1800ae4aa/4b2), normalises THAT, and releases the
//     substring separately. Collapsing the two slots would normalise a buffer
//     the substring temporary still owns.
//  4. THE `this` POINTER IS NEVER READ. It is a `QEBA` const member that
//     touches no field — which is why `?Create@FileStream@` can and does call
//     it on the singleton rather than on anything of its own.
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 5a: the body is now
// `IO::FileSystem::CreatePath`, at the bottom of this file. The door keeps its
// name, its signature (file_stream.cpp forward-declares and calls it) and both
// registration lines, and delegates. No qualified `IO::` call is needed:
// `IO::FileSystem` has no vtable at all (runtime/types/io/FileSystem.h), so
// B34_BRIEFS section A.1's re-entry hazard cannot arise.
bool RtIo_FileSystem_CreatePath(const FileSystem* self, const Filename* path) {
    return reinterpret_cast<const IO::FileSystem*>(self)->CreatePath(*path);
}

// ============================================================================
// IO::FileSystem::PlatformCreateFile(FileInfo&, Filename const&)
//                                                         retail 0x1800ade60
// ============================================================================
// Open a NEW temp file in %TEMP% and record both names on the FileInfo — see
// the header note on the atomic-replace pair.
//
// THE CREATE FLAGS ARE READ OFF THE PUSHES, NOT GUESSED:
//   0x1800aded5  EDX = 0x40000000  GENERIC_WRITE
//   0x1800aded2  R8D = 0           dwShareMode = 0 (exclusive)
//   0x1800adecf  R9D = 0           lpSecurityAttributes = null
//   0x1800adec7  [rsp+0x20] = 2    CREATE_ALWAYS
//   0x1800adebf  [rsp+0x28] = 0x80 FILE_ATTRIBUTE_NORMAL
//   0x1800adeba  [rsp+0x30] = 0    hTemplateFile = null
//
// THE FAILURE TEST IS UNSIGNED AND CATCHES TWO VALUES, NOT ONE.
// `DEC RAX ; CMP RAX,-0x3 ; JA fail` fails when `handle - 1` is above
// 0xFFFFFFFFFFFFFFFD unsigned, i.e. when the handle is INVALID_HANDLE_VALUE
// (-1) **or NULL**. A port that tested only INVALID_HANDLE_VALUE would treat a
// null handle as success. On failure the FileInfo's `handle` is left at
// whatever the caller set (`?CreateFileW@` sets -1) and `false` is returned;
// the Win32 handle at +0x18 is stored EITHER WAY, before the test.
//
// `handle = -2` on success is the LOOSE-FILE sentinel FileSystem.h documents
// (-1 nothing, -2 loose, otherwise a packed (packIndex<<24)|fileIndex).
//
// GetTempFileNameW's prefix comes from the .rdata literal at 0x1801557e8, which
// READS AS `L"SRS"` (53 00 52 00 53 00 00 00 — Shining Rock Software). It is
// still fetched live through the address door rather than spelled here: it is an
// unexported literal, and a transcription of it is a guess no static check
// could catch. The bytes are quoted so a reader can see the door was verified.
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 5a: the body is now
// `IO::FileSystem::PlatformCreateFile`, at the bottom of this file. Same door
// rules as CreatePath above; this one is `AEAA` (private) in retail and keeps
// its own registration pair, which is why the member is public
// (runtime/types/io/FileSystem.h says so at the declaration).
bool RtIo_FileSystem_PlatformCreateFile(FileSystem* self, FileInfo* info,
                                        const Filename* path) {
    return reinterpret_cast<IO::FileSystem*>(self)->PlatformCreateFile(*info, *path);
}

// ENDGAME B.3.4 PACKAGE+FS commit 5a: the remaining four doors of this file,
// gathered here so every one of them is a single delegating line and the
// retail bodies are all in the one `namespace IO` block below. Each keeps its
// name, its signature — `CreateFileW` keeps the (self, hidden-return-slot,
// path) form and `CloseFile` is called cross-file by file_stream.cpp,
// text_file_stream_win.cpp, persistence_saveinfo_job.cpp and
// persistence_worker_win.cpp — and its registration lines, unchanged.
FileInfo* RtIo_FileSystem_CreateFileW(FileSystem* self, FileInfo* ret,
                                      const Filename* path) {
    return reinterpret_cast<IO::FileSystem*>(self)->CreateFileW(ret, *path);
}

void RtIo_FileSystem_PlatformCloseFile(FileSystem* self, FileInfo* info) {
    reinterpret_cast<IO::FileSystem*>(self)->PlatformCloseFile(*info);
}

void RtIo_FileSystem_CloseFile(FileSystem* self, FileInfo* info) {
    reinterpret_cast<IO::FileSystem*>(self)->CloseFile(*info);
}

// `??1FileSystem@IO@@QEAA@XZ` — the door for a REAL C++ destructor now. The
// explicit `->~FileSystem()` is the whole delegation; nothing else destroys an
// `IO::FileSystem`, because nothing in the tree ever holds one by value (the
// live object is the one the singleton data symbol points at, constructed by
// retail's still-unported 0x1800ac7c0).
void RtIo_FileSystem_Dtor(FileSystem* self) {
    reinterpret_cast<IO::FileSystem*>(self)->~FileSystem();
}

// Read by file_stream_cutover_probe.cpp, which prints attendance beside the
// door verdicts so a reader cannot see one without the other.
void RtIoFileSystemCreateFileCounters(unsigned long long* out) {
    out[0]=g_fcc.createPath;   out[1]=g_fcc.createPathDirs;
    out[2]=g_fcc.createFileW;  out[3]=g_fcc.platCreate;
    out[4]=g_fcc.createOk;     out[5]=g_fcc.createFail;
    out[6]=g_fcc.closeFile;    out[7]=g_fcc.platClose;
    out[8]=g_fcc.closeRenamed; out[9]=g_fcc.fsDtor;
}

}  // namespace RtIO

// ============================================================================
// `IO::FileSystem`'s member bodies for this file
// ============================================================================
// Defined outside `namespace RtIO` because a member of `IO::X` must appear in a
// namespace ENCLOSING the class (B34_BRIEFS section A.2); these bodies reach
// this translation unit's `RtIO`-scoped names — the anonymous-namespace
// helpers `WideStr`/`WideFromString`/`WideRelease`/`PodArrayDtor`, the
// attendance counters `g_fcc`, and the `using`-ed System names — as `RtIO::X`.
// Every POD is spelled `RtIO::` or `RtSystem::` and every real class `System::`
// (B34_RULES section 7.2).
//
// NOTHING IN ANY OF THE SIX BODIES CHANGED but the spelling of the receiver and
// of the two cross-body calls (`CloseFile` to `PlatformCloseFile`, `CreateFileW`
// to `PlatformCreateFile`, both now member calls rather than door calls —
// neither ever went through a resolve door). Same Win32 calls, same order, same
// discarded results, same early-out positions, same counter bumps, same
// destructor order.
namespace IO {

bool FileSystem::CreatePath(const RtSystem::Filename& pathRef) const {
    const RtSystem::Filename* const path = &pathRef;
    ++RtIO::g_fcc.createPath;
    // detail 4 — retail reads no field of `this`

    // Retail passes the Filename straight into String's members, which is what
    // AsString() spells (Filename.h).
    const System::String& src = RtIO::Sys(*path).AsString();
    const int32_t len = src.GetLength();                    // 1800ae459

    int32_t created = 0;
    for (int32_t i = 0; i < len; ++i) {                     // 1800ae467/62e
        const char c = src._data[i];                        // 1800ae473
        if (c != '\\' && c != '/') continue;                // 1800ae477/47d

        // Detail 3, in class terms: SubString (0x1800c9230) builds a temporary,
        // Filename(const String&) (0x18000c780) adopts it into a SECOND slot and
        // normalises that, and the temporary is released at the end of the full
        // expression — retail's 0x1800ae4c3, in the same place.
        System::Filename dir(src.SubString(0, i));          // 1800ae489..4bd

        RtIO::WideStr w{};                                  // 1800ae53f
        w.data = nullptr;
        RtIO::WideFromString(&w, &dir.AsString());
        ::CreateDirectoryW(w.data, nullptr);                // 1800ae54a — result DISCARDED
        ++created; ++RtIO::g_fcc.createPathDirs;
        RtIO::WideRelease(&w);                              // 1800ae551..5bd
        // ~dir is retail's release at 0x1800ae5bd.
    }

    return true;                                          // 1800ae637: MOV AL,1
}

bool FileSystem::PlatformCreateFile(RtIO::FileInfo& infoRef,
                                    const RtSystem::Filename& pathRef) {
    RtIO::FileInfo* const info = &infoRef;
    const RtSystem::Filename* const path = &pathRef;
    ++RtIO::g_fcc.platCreate;

    wchar_t tempDir[MAX_PATH];
    ::GetTempPathW(MAX_PATH, tempDir);                       // 1800ade91/96

    // PORT GAP 0x1801557e8 (the three-character .rdata prefix retail passes to
    // GetTempFileNameW): not ported; retail reads it here. Nothing registers
    // that address, so the address door that stood here answered null and the
    // call ran with a null prefix; that null arm is the code (B3R R4).
    const wchar_t* prefix = nullptr;
    wchar_t tempFile[MAX_PATH];
    ::GetTempFileNameW(tempDir, prefix, 0, tempFile);        // 1800adea4..b2

    HANDLE h = ::CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    info->winHandle = reinterpret_cast<int64_t>(h);          // 1800adee5 — stored EITHER WAY

    const uint64_t probe = static_cast<uint64_t>(info->winHandle) - 1;
    if (probe > static_cast<uint64_t>(-3)) {                 // 1800adee9..f0
        ++RtIO::g_fcc.createFail;
        return false;                                        // 1800adffa
    }

    info->handle = -2;                                       // 1800adef6 — LOOSE

    // temp path -> _name0, via WideString::Set / AsUTF8 / AdjustDelimiters.
    // Both WideString entry points are ported; the two temporaries are released
    // in retail's order (the UTF-8 String first, then the WideString).
    {
        RtIO::WideStr w{};  w.data = nullptr;                // 1800adefc
        reinterpret_cast<System::WideString*>(&w)->Set(      // B.3.x bridge
            tempFile);                                       // 1800adf0f

        // AsUTF8's hidden return slot is still POD-shaped (System::WideString
        // is not a B.3.1 class), so the value lands in a POD and the move ctor
        // steals it: `utf8Pod` is left null, so ~utf8 is the one release.
        RtSystem::String utf8Pod{};  utf8Pod.data = nullptr; // 1800adf15
        ::new (&RtIO::Sys(utf8Pod)) System::String(           // B.3.x bridge
            reinterpret_cast<const System::WideString*>(&w)->AsUTF8());   // 1800adf21
        System::String utf8(std::move(RtIO::Sys(utf8Pod)));

        // adopt-then-AdjustDelimiters == Filename(const String&), 0x18000c780.
        System::Filename norm(utf8);                         // 1800adf27..47
        // Filename::operator=(const Filename&), 0x180006770. `_name0` is a
        // `RtSystem::String` member of the not-yet-converted FileInfo.
        RtIO::SysFile(info->_name0) = norm;                  // 1800adf58

        // ~norm (0x1800adf5e) and ~utf8 (0x1800adf93) are the destructors now.
        RtIO::WideRelease(&w);                               // 1800adfc0..ea
    }

    // the CALLER's path -> _name1
    RtIO::SysFile(info->_name1) = RtIO::Sys(*path);          // 1800adfea/f1

    ++RtIO::g_fcc.createOk;
    return true;                                             // 1800adff6
}

// ============================================================================
// IO::FileSystem::CreateFileW(Filename const&)            retail 0x1800acc50
// ============================================================================
// SEVENTEEN INSTRUCTIONS: build an empty FileInfo in the caller's return slot,
// then hand it to PlatformCreateFile. It RETURNS THE FileInfo BY VALUE, so RDX
// is the hidden return slot and R8 is the Filename — the signature detail
// file_stream.cpp's header note is about.
//
// The initialisation is the SAME field list `??0FileStream`'s inlined prologue
// writes, +0x04 excluded in both. Two unrelated functions leaving the same
// field alone is the agreement FileStream.h's note is built on.
//
// THE PLATFORM RESULT IS DISCARDED (0x1800acca4 has no test after it). The
// caller learns success from `handle != -1`, which PlatformCreateFile sets to
// -2 only on the success path — so the information is carried by the field, not
// by the bool.
// ============================================================================
RtIO::FileInfo* FileSystem::CreateFileW(RtIO::FileInfo* ret,
                                        const RtSystem::Filename& pathRef) {
    const RtSystem::Filename* const path = &pathRef;
    ++RtIO::g_fcc.createFileW;

    ret->handle      = -1;          // 1800acc6c
    ret->size        = 0;           // 1800acc72 (qword +0x08, covers cursor)
    ret->cursor      = 0;
    ret->baseOffset  = 0;           // 1800acc76
    // `ret` is the hidden return slot: raw bytes, so these are two default
    // ctors (??0Filename@System@@QEAA@XZ, 0x180006220), not stores through
    // live objects.
    new (&ret->_name0) System::Filename();   // 1800acc82
    new (&ret->_name1) System::Filename();   // 1800acc8e
    ret->winHandle   = -1;          // 1800acc91
    // +0x04 deliberately untouched, exactly as in ??0FileStream's prologue.

    PlatformCreateFile(*ret, *path);         // 1800acca4 — result discarded

    return ret;                              // 1800acca9
}

// ============================================================================
// IO::FileSystem::PlatformCloseFile(FileInfo&)            retail 0x1800ae020
// ============================================================================
// Close the Win32 handle, then perform the atomic replace: DeleteFileW the
// destination, MoveFileW the temp over it, and clear both name slots.
//
// FIVE THINGS THE DISASSEMBLY SETTLES, each of which a paraphrase gets wrong:
//
//  1. EVERY EARLY-OUT IS A PLAIN RETURN, and the three of them are at
//     0x1800ae041 (no handle), 0x1800ae05f (`_name0` is null) and 0x1800ae069
//     (`_name0` has zero LENGTH). The last reads `[RAX-0x8]`, the block's
//     length, NOT the refcount at -4.
//  2. THE HANDLE IS RESET TO -1 BEFORE ANY OF THE NAME WORK (0x1800ae04d), so
//     a second close is a no-op even if the rename throws.
//  3. `DeleteFileW` IS CALLED ON `_name1`, the DESTINATION — 0x1800ae073 puts
//     RSI (`&_name1`) in RDX for the WideString ctor. It removes an existing
//     file so the move can land; its result is discarded.
//  4. `MoveFileW(_name0, _name1)`, in that order. See the header note — the
//     argument order is what identifies which field is which.
//  5. BOTH NAME SLOTS ARE CLEARED THROUGH `Filename::operator=` FROM A NULL
//     TEMPORARY (0x1800ae14b/15e), not by storing null. That runs the release
//     AND `AdjustDelimiters` on an empty string, which is what retail does; a
//     bare `= nullptr` would leak both buffers.
// ============================================================================
void FileSystem::PlatformCloseFile(RtIO::FileInfo& infoRef) {
    RtIO::FileInfo* const info = &infoRef;
    ++RtIO::g_fcc.platClose;

    if (info->winHandle == -1) return;                       // 1800ae03d/41

    ::CloseHandle(reinterpret_cast<HANDLE>(info->winHandle));// 1800ae047
    info->winHandle = -1;                                    // 1800ae04d

    if (!info->_name0.data) return;                          // 1800ae059/5f
    if (RtSystem::StringLength(info->_name0.data) == 0) return;  // 1800ae065/69

    ++RtIO::g_fcc.closeRenamed;

    // 3. delete the destination
    {
        RtIO::WideStr w{};  w.data = nullptr;
        RtIO::WideFromString(&w, &RtIO::Sys(info->_name1));  // 1800ae07b
        ::DeleteFileW(w.data);                               // 1800ae084 — discarded
        RtIO::WideRelease(&w);                               // 1800ae08d..b9
    }

    // 4. move the temp over it. Retail builds the DESTINATION wide string
    //    FIRST (0x1800ae0c1, over _name1) and the SOURCE second (0x1800ae0d1,
    //    over _name0), then calls MoveFileW(source, destination).
    {
        RtIO::WideStr dst{};  dst.data = nullptr;
        RtIO::WideFromString(&dst, &RtIO::Sys(info->_name1));// 1800ae0c1
        RtIO::WideStr src{};  src.data = nullptr;
        RtIO::WideFromString(&src, &RtIO::Sys(info->_name0));// 1800ae0d1
        ::MoveFileW(src.data, dst.data);                     // 1800ae0dd — discarded
        RtIO::WideRelease(&src);                             // 1800ae0e4..111  (src first)
        RtIO::WideRelease(&dst);                             // 1800ae111..13e
    }

    // 5. clear both, through Filename::operator= from a null temporary
    //    (0x180006770); the temporary's own destructor is a no-op on null.
    RtIO::SysFile(info->_name0) = System::Filename();        // 1800ae13e/14b
    RtIO::SysFile(info->_name1) = System::Filename();        // 1800ae151/15e
}

// ============================================================================
// IO::FileSystem::CloseFile(FileInfo&)                    retail 0x1800accc0
// ============================================================================
// `PlatformCloseFile(info); *info = FileInfo();`
//
// THE RESET IS A REAL ASSIGNMENT FROM A REAL TEMPORARY, not a field-by-field
// clear: retail builds a fresh FileInfo on its own stack (0x1800acce3..d0b),
// calls `FileInfo::operator=` (0x1800acd1a) and then destroys the temporary
// (0x1800acd20..d81, _name1 before _name0). That matters because
// `FileInfo::operator=` runs `AdjustDelimiters` on BOTH name slots on BOTH
// paths and short-circuits self-assignment of the buffer — behaviour a
// hand-written clear does not have.
//
// The temporary leaves +0x04 unwritten, so the assignment copies a dword of
// retail's own stack garbage into the caller's FileInfo. That is faithful and
// it is why filesystem_openfile_win.cpp's oracle already excludes +0x04
// (`reference_oracle_must_not_compare_unwritten_bytes.md`).
// ============================================================================
void FileSystem::CloseFile(RtIO::FileInfo& infoRef) {
    RtIO::FileInfo* const info = &infoRef;
    ++RtIO::g_fcc.closeFile;

    PlatformCloseFile(*info);                        // 1800accde

    RtIO::FileInfo fresh;                            // NOT zero-initialised: see above
    fresh.handle      = -1;                          // 1800acce3
    fresh.size        = 0;                           // 1800accec (qword)
    fresh.cursor      = 0;
    fresh.baseOffset  = 0;                           // 1800accf0
    // Two empty Filenames constructed into the raw stack slots (0x180006220);
    // FileInfo is not a converted class, so they are built and destroyed by
    // hand — one release each, in retail's order.
    new (&fresh._name0) System::Filename();          // 1800accfb
    new (&fresh._name1) System::Filename();          // 1800acd07
    fresh.winHandle   = -1;                          // 1800acd0b

    ::RtIo_FileInfo_Assign(info, &fresh);            // 1800acd1a

    RtIO::SysFile(fresh._name1).~Filename();         // 1800acd20  +0x28 first
    RtIO::SysFile(fresh._name0).~Filename();         // 1800acd50  +0x20

    // ---- NOT PART OF THE PORT ------------------------------------------
    // A one-shot trigger for file_stream_slots_probe.cpp, which needs a LIVE
    // FileSystem singleton (it builds its paths through GetSaveDirectory) and
    // therefore cannot run from RuntimePortInit or from an atexit handler.
    //
    // This body is the latest-and-most-attended anchor this lane owns: 2854
    // calls on the gameplay route, all of them after the package mount. Call
    // 2000 is arbitrary but comfortably inside that. It is gated on the probe's
}

// ============================================================================
// IO::FileSystem::~FileSystem                             retail 0x1800ac6d0
// ============================================================================
// Six steps, in retail's order:
//   1. SubtreeFree(&packfileIndexRoot, packfileIndexRoot)    1800ac6ff
//   2. packfileIndexRoot = null; packfileIndexCount = 0      1800ac706/09
//   3. ~Array<Filename>(&userPackFilenames)  (+0x30)         1800ac710
//   4. ~Array<PackageFile*>(&packfiles)      (+0x20)         1800ac71a
//   5. release packDirectory (+0x10), dataDirectory (+0x08),
//      sourceDirectory (+0x00), in THAT order                1800ac720..79c
//
// NOTE THE CLASS HAS NO VTABLE. `FileSystem::sourceDirectory` is at +0x00 and
// the destructor's last act is to release it — a `QEAA` non-virtual destructor
// on a class whose first field is a String, which is why no table is stamped
// anywhere in this body. FileSystem.h already pins that layout.
//
// STEP 4's ELEMENT DESTRUCTOR IS A POD FREE, NOT A PER-ELEMENT WALK.
// 0x1800097f0 frees the block and zeroes the three header fields; it does NOT
// release the PackageFile* elements. That is retail: `ClosePackages` owns those
// and has already run. A port that "helpfully" destroyed the elements here
// would double-free every mounted package
// (`reference_a_written_recipe_can_be_wrong.md` is exactly this shape — two
// destructors that look interchangeable and are not).
// ============================================================================
// THE THREE `RtSystem::String` MEMBERS STAY PODs, and it is THIS BODY that is
// the reason. Retyping +0x00/+0x08/+0x10 to `System::String` would give
// `~FileSystem()` three IMPLICIT member destructors running IN ADDITION to the
// three explicit releases below — the B31 `RtSystem::String` double-free,
// reached from the other direction. The releases stay explicit and by hand,
// exactly as retail's three inlined idioms are.
FileSystem::~FileSystem() {
    ++RtIO::g_fcc.fsDtor;

    // Step 1. The tree object is (root, count) at +0x40/+0x48; retail passes
    // its ADDRESS as `this` and the root as the node.
    void* tree = &_packfileIndexRoot;
    rtavl::SubtreeFree(tree,                          // 1800ac6ff
        reinterpret_cast<rtsetavl::Node*>(_packfileIndexRoot));

    _packfileIndexRoot  = nullptr;                    // 1800ac706
    _packfileIndexCount = 0;                          // 1800ac709

    // Step 3 — Array<Filename>, ported by modification.cpp and registered.
    RtIO::RtColl_ArrayFilenameDtor_1800098d0(&_userPackFilenames);   // 1800ac710

    // Step 4 — the POD array free, our own copy. See the note above.
    RtIO::PodArrayDtor(
        reinterpret_cast<RtIO::PodArray*>(&_packfiles));  // 1800ac71a

    // Step 5
    // The three path fields are `RtSystem::String` members, so ~String is
    // invoked on them explicitly; it is the same inlined release retail emits
    // at each of the three addresses.
    RtIO::Sys(_packDirectory).~String();              // 1800ac720  +0x10
    RtIO::Sys(_dataDirectory).~String();              // 1800ac74a  +0x08
    RtIO::Sys(_sourceDirectory).~String();            // 1800ac775  +0x00
}

}  // namespace IO

// ============================================================================
// Registration — BY NAME *AND* BY ADDRESS for every row.
// `?CreateFileW@FileSystem@IO@@` and `?PlatformCloseFile@FileSystem@IO@@` are
// exported under their mangled names but carry no ordinal in the table dump
// above; the name door is the one their callers use and the address door is the
// one an inlined caller would use.
// ============================================================================
int RtIoFileSystemCreateFileRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileSystem_CreatePath); ++n;
    RegisterRuntimeAddr(0x1800ae430, (void*)&RtIo_FileSystem_CreatePath);

    RegisterRuntimeExport("?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z",
                          (void*)&RtIo_FileSystem_CloseFile); ++n;
    RegisterRuntimeAddr(0x1800accc0, (void*)&RtIo_FileSystem_CloseFile);

    RegisterRuntimeExport("??1FileSystem@IO@@QEAA@XZ",
                          (void*)&RtIo_FileSystem_Dtor); ++n;
    RegisterRuntimeAddr(0x1800ac6d0, (void*)&RtIo_FileSystem_Dtor);

    // The three closure rows — not on the Wave 19 list, required by the
    // screenshot path. See the header note.



    printf("[RtPort]   IO::FileSystem create/close (wave 19 lane FILE): "
           "%d exports registered\n", n);
    return n;
}
