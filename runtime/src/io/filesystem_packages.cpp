// ============================================================================
// runtime/src/io/filesystem_packages.cpp — Wave 9 lane PKG
// IO::FileSystem::LoadPackages — the cutover wall of attempt 11
// ============================================================================
//
//   | Export                                       | Retail      | Ord  | Instrs |
//   |----------------------------------------------|-------------|------|--------|
//   | ?LoadPackages@FileSystem@IO@@QEAAXXZ         | 0x1800ab460 | 3209 |  430   |
//   | ?ClosePackages@FileSystem@IO@@QEAAXXZ        | 0x1800abab0 | 1728 |   90   |
//   | ?GetPackDirectory@FileSystem@IO@@QEBA?AV…XZ  | 0x180007c10 | 2828 |   15   |
//   | ?GetFileName@PackageFile@IO@@QEBA?AV…H@Z     | 0x180025450 | 2602 |   18   |
//
// Every mangled name above, and every name reached through RT_EXPORT_ONCE
// below, was read out of Runtime-x64.dll's own export directory with `pefile`
// BEFORE a line of this file was written — and so was every ORDINAL in this
// file, printed straight from the export table's ordinal array plus its
// ordinal base rather than transcribed from a note. That is not ceremony:
// the first draft of this header carried eight ordinals reconstructed by
// hand and SEVEN OF THE EIGHT WERE WRONG (ClosePackages 1904 vs 1728,
// GetPackDirectory 2872 vs 2828, GetFileName 2650 vs 2602, FindFiles 2306 vs
// 2350, ??0PackageFile 505 vs 289, ReadPackfile 3560 vs 3461, CloseFile 1900
// vs 1726). An ordinal is exactly the kind of detail that reads as evidence
// and is never checked by the compiler — not looked up in a decompile, and
// not typed from memory. A name the DLL does not contain resolves NULL
// silently and forever (reference_resolveexport_phantom_names.md).
//
// Kill switch: BANISHED_RT_PKG=0 removes this entire file — all four rows —
// and leaves every call on the DLL.
//
// NO NEW WIN32 API AND NO NEW IMPORT LIBRARY. This file calls nothing outside
// the Runtime DLL's own surface; SA_LIBS is untouched.
//
// ---------------------------------------------------------------------------
// 1. THE ROW WAS SIZED WRONG GOING IN, AND THE CORRECTION MATTERS
// ---------------------------------------------------------------------------
// The lane brief described LoadPackages as "~300 instructions". It is **430**,
// and the extra hundred are not padding: they are a THIRD loop (the path→entry
// index build) and a call to a packfile SORT that the brief's structural
// sketch did not mention at all. The sketch also had the two array-growth
// loops iterating over "[RSI+0x20]/[+0x28]/[+0x2c]" and "[RSI+0x30]/[+0x38]"
// as if those were two different destination arrays. They are not. BOTH loops
// APPEND TO THE SAME ARRAY at +0x20; the difference is the SOURCE they read:
// loop 1 walks the `*.pkg` files FindFiles just discovered, loop 2 walks the
// user/mod pack filenames already sitting in the FileSystem at +0x30.
// Everything below is transcribed from the disassembly at 0x1800ab460, not
// from that sketch or from Ghidra's C (which drops dereferences and prints
// MOVSXD as a zero extension — reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// 2. THE BODY
// ---------------------------------------------------------------------------
//   if (this->existsInPackfileGate == 0) return;          // +0x18, the gate
//
//   ClosePackages(this);                                  // drop any previous mount
//   String pattern = "*.pkg";                             // literal 0x1801556f0
//   Filename packDir = this->GetPackDirectory();          // a copy of +0x10
//   Array<Filename> found;                                // {ptr, count} on the stack
//   this->FindFiles(packDir, pattern, /*recursive*/false, found);
//   ~packDir; ~pattern;
//
//   for (i = 0; i < found.count; ++i)                     // LOOP 1
//       append(this->packfiles, ReadPackfile(new PackageFile, found[i]));
//   if (this->packfileCount > 1) SortPackfiles(...);      // 0x1800264a0
//   for (i = 0; i < this->userPackFilenameCount; ++i)     // LOOP 2
//       append(this->packfiles, ReadPackfile(new PackageFile, this->userPackFilenames[i]));
//
//   for (pi = 0; pi < this->packfileCount; ++pi)          // LOOP 3 — the index
//       for (fi = 0; fi < packfiles[pi]->entryCount; ++fi) {
//           Filename key = packfiles[pi]->GetFileName(fi);
//           node = IndexFind(this->packfileIndexRoot, key);
//           if (node) { if (packfiles[node->packIndex]->overridable) node->value = {pi,fi}; }
//           else      BstInsert(&this->tree, &out, key, {pi,fi});
//           ~key;
//       }
//
//   ...then the temporary Filename array is released element by element and
//   its block freed.
//
// The `*.pkg` literal was read with `read_memory` at 0x1801556f0 and is the
// five bytes `*.pkg`, NUL-terminated — not guessed from the function's name.
// It agrees with the install on disk: C:\GOG Games\Banished\WinData holds
// data0.pkg, data1.pkg, patch0.pkg and patch1.pkg, and lane FS independently
// established that GetPackDirectory's +0x10 resolves to that same WinData
// directory. So loop 1 runs FOUR times on a stock install, which is the fact
// §5 turns on.
//
// ---------------------------------------------------------------------------
// 3. THE PACKFILE GATE IS OPEN HERE, AND THAT IS THE WHOLE DIFFERENCE FROM
//    LANE FS
// ---------------------------------------------------------------------------
// `??0FileSystem@IO@@QEAA@XZ` (0x1800ac7c0) writes `MOV word ptr [RCX+0x18],
// 0x100`, i.e. +0x18 = 0 and +0x19 = 1. Lane FS used exactly that to prove the
// packfile arm of `FileSystem::Exists` was DEAD inside SetupDirectories.
//
// It is not dead here. `FileSystem::Startup` sets `this->existsInPackfileGate
// = 1` BETWEEN the two calls (runtime/src/io/filesystem_startup.cpp), so by the
// time control reaches LoadPackages the byte is 1 — and attempt 11's crash
// dump read `this[+0x18] = 0x101`, which is the same fact measured live rather
// than derived. The `JZ epilogue` at 0x1800ab483 is therefore NOT taken and the
// entire body below is live.
//
// See §6 for what that does to lane FS's ExistsInPackfile holdback. Short
// version: it kills it.
//
// ---------------------------------------------------------------------------
// 4. WHAT THIS FILE DELIBERATELY DOES NOT PORT
// ---------------------------------------------------------------------------
// LoadPackages' callee set does not close in this lane, and pretending
// otherwise would be worse than saying so. Written out here so the next lane
// inherits a list and not an archaeology task. Each is called through the
// PROPER door — RT_EXPORT_ONCE for an exported name, ResolveAddr for an
// unexported internal — so a run that reaches one dies with its NAME on
// [RtUnresolved] rather than silently doing nothing:
//
//   ?FindFiles@FileSystem@IO@@QEBAX…      0x1800ad7a0  ord 2350   289 instrs
//       The `*.pkg` enumeration. A FindFirstFileW/FindNextFileW/FindClose loop
//       through dispatch slots 0x18014b0d8/0e0/0e8, plus a recursive second
//       half behind its `bool` parameter (passed FALSE here). Its own callees
//       are already ported: WideString ctor/Set/AsUTF8 (lane FS), String
//       operator+ (0x1800c9440), Filename::AdjustDelimiters (0x1800c6970);
//       the one that is not is Array<Filename>::Insert at 0x180026880.
//       **THIS IS THE NEXT WALL — see §8.**
//   ??0PackageFile@IO@@QEAA@XZ            0x1800b4440  ord  289    48 instrs
//       A plain field-initialising constructor, and cheap — EXCEPT that it
//       writes three vtable pointers and only two of them are minted. See §7.
//   ?ReadPackfile@PackageFile@IO@@…       0x1800b46b0  ord 3461  6,597 BYTES
//       The .pkg format parser itself. This is a lane, not a tail-end.
//   ?CloseFile@FileSystem@IO@@…           0x1800accc0  ord 1726
//   FUN_180008340 (recursive index-node free)          0x180008340
//   FUN_180025a80 → FUN_180025c70 (BST insert)         0x180025a80
//       …which in turn needs FUN_180008520 (node alloc) and FUN_180025e10
//       (the rebalance). Structurally the insert twin of the config map's find
//       that runtime/src/system/configuration.cpp already ports, comparator and
//       null-key asymmetries included — but keyed with _stricmp (see below),
//       not the signed-char strcmp that file uses.
//   FUN_1800264a0 (the packfile sort)                  0x1800264a0  110 instrs
//       Documented in full at its call site below. NOT ported on purpose, and
//       the reason is stated there rather than hidden: it is an introsort with
//       no oracle available in this lane, and a plausible-looking replacement
//       would change tie-breaking silently.
//   FUN_18007eefc / FUN_18007eec0 (_stricmp)           0x18007eefc
//       The CRT's own copy. 0x18007eec0 is `__ascii_stricmp`: it lowers only
//       'A'-'Z' and compares the bytes UNSIGNED (`MOVZX`). Worth writing down
//       because it is the MIRROR IMAGE of the trap configuration.cpp records:
//       that file's comparator must use SIGNED chars, this one must not.
//       A single shared "TreeStrCmp" serving both would be wrong for one of
//       them on any byte >= 0x80 — and a package path can contain one.
//
// ---------------------------------------------------------------------------
// 5. THE ORACLE: THERE IS NONE, AND HERE IS WHY, RATHER THAN A WEAK ONE
// ---------------------------------------------------------------------------
// LoadPackages cannot be replayed and cannot be shadowed on a scratch object:
//
//   * It is NOT a pure read. It heap-allocates one 0xF0-byte PackageFile per
//     package, opens each .pkg on disk (172 MB of them on this install), grows
//     an array, builds a tree of one node per FILE INSIDE every package, and
//     frees a temporary array. Running it twice mounts every package twice.
//   * A scratch-`FileSystem` stand-in — the trick that made lane FS's
//     SetupDirectories replayable — does not work either. SetupDirectories
//     reads two bytes and writes three Strings; LoadPackages' output IS the
//     process's file-system state, and the objects it builds are handed to a
//     singleton the rest of the engine reads.
//   * The RT_DIFF_SHADOW below is therefore a SHADOW, not a comparison, and it
//     is labelled as one. It prints the four counts the body produced. It can
//     catch "the port ran and did nothing"; it CANNOT catch "the port ran and
//     built the wrong tree", and quoting it as a verification would be the
//     thing this project has been burned by repeatedly
//     (reference_probe_needs_an_effectiveness_control.md).
//
// The three small rows in this file DO have real oracles — they are pure, they
// take a hidden return slot, and replaying them onto a second scratch Filename
// costs nothing. Those replays snapshot their inputs before the call, per
// reference_oracle_must_snapshot_aliased_inputs.md.
//
// And the honest bottom line, stated once: **nothing in this file has been
// executed.** No build of the game, no run, no log. §8 is a prediction, not a
// report.
//
// ---------------------------------------------------------------------------
// 6. LANE FS's `ExistsInPackfile` HOLDBACK, RE-DERIVED FROM SCRATCH
// ---------------------------------------------------------------------------
// Lane FS declined `?ExistsInPackfile@FileSystem@IO@@QEBA_N…` (0x1800ab180,
// ord 2322) because inside SetupDirectories the packfile gate is 0, so the arm
// is dead. **That reasoning is correct and it is now spent.** Re-derived here
// without inheriting any of it:
//
//   1. `?Exists@FileSystem@IO@@QEBA_N…` is ALREADY PORTED
//      (runtime/src/io/filesystem.cpp:151) and its first arm is
//      `if (fs->existsInPackfileGate != 0 && ExistsInPackfile(fs, path))`.
//   2. That call goes through RT_EXPORT_ONCE — the port-aware door — and
//      `?ExistsInPackfile@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` has NO
//      RegisterRuntimeExport and NO RegisterRuntimeAddr anywhere in the tree
//      (checked by grep over runtime/, standalone/, engine/, code/, dx11/).
//   3. FileSystem::Startup sets the gate to 1, and it does so BEFORE calling
//      LoadPackages — i.e. before this file even runs.
//
// So from the instant `FileSystem::Startup` executes, EVERY `Exists()` call in
// the process evaluates `ExistsInPackfile` first, and at cutover that is a call
// through a null pointer. The same paragraph applies verbatim to
// `?PlatformExists@FileSystem@IO@@AEBA_N…` (0x1800adc70), the other arm, also
// unported, also reached through a live door.
//
// **This is a live cutover hole that no instrument in this project reports**,
// for exactly the reason lane FS §4.1 gave about WideString: an RT_EXPORT_ONCE
// to an unported symbol is indistinguishable from one to a ported symbol until
// it runs. It is not in THIS row's call chain, so this lane does not port it —
// but it is upstream of everything, and the integrator should treat it as
// higher priority than the row I was given.
//
// ---------------------------------------------------------------------------
// 7. ONE MINT ROW IS NEEDED, AND HOW ITS SIZE WAS ESTABLISHED
// ---------------------------------------------------------------------------
// The mint is the integrator's file and is not touched here. The row:
//
//     ??_7PackageFile@IO@@6B@   DLL 0x180155700   9 slots
//
// Size established by READING THE BYTES at 0x180155700, not by trusting the
// mangled name — the name says nothing about length, and this wave has already
// paid once for a `PAD` suffix that put an 8 KB buffer in an 8-byte slot. Nine
// consecutive qwords there are code addresses inside .text (0x1800259d0,
// 0x180025150, 0x180025130, 0x1800b6080, 0x1800b6090, 0x1800b60a0, 0x1800252d0,
// 0x180025300, 0x1800252f0); the TENTH qword is 0x0000000000000000 and the
// bytes immediately following it are the ASCII "%08lX%04hX%04hX…" format
// string, i.e. the next object in .rdata. So the table is nine slots and the
// terminator is observed, not assumed.
//
// `??_7Base@Object@@6B@` (0x180154788) and `??_7External@Object@@6B@`
// (0x1801547a0) — the other two vtables that constructor writes — are ALREADY
// in rt_data_mint_tables.inc (lines 1212 and 1229). Only PackageFile's is
// missing, which is why ??0PackageFile is declined in §4 rather than ported:
// porting it today would write a garbage vtable pointer into every package.
//
// ---------------------------------------------------------------------------
// 8. PREDICTION FOR ATTEMPT 12, AND WHAT WOULD FALSIFY IT
// ---------------------------------------------------------------------------
// The next fault is `[RtUnresolved] ?FindFiles@FileSystem@IO@@QEBAX…`
// (0x1800ad7a0, ord 2350), EXEC of 0, symbolising inside
// **RtIo_FileSystem_LoadPackages** — a function that does not exist in the
// binary today — and **THE STACK FRAME MOVES DOWN ONE LEVEL**: #1
// RtIo_FileSystem_LoadPackages, #2 RtIo_FileSystem_Startup, #3
// RtEngine_Engine_Startup. This deliberately differs from lane FS's attempt-10
// prediction, which said the frame would NOT move; the frame moves here
// because LoadPackages now HAS a frame, where before it was the callee that
// never existed.
//
// FALSIFIERS, in the order they are worth reading:
//
//  * A fault naming `?ClosePackages@` means the gate byte at +0x18 was ZERO
//    and §3 is wrong — no, worse: it means the gate was zero and yet
//    ClosePackages was reached, which is impossible, so it would instead mean
//    the ClosePackages REGISTRATION did not take. Check the
//    `[RtPort] IO: N exports registered` count moved by 4.
//  * A fault naming `?GetPackDirectory@` means the early `if (gate == 0)`
//    return fired and then… it cannot: GetPackDirectory is past the gate. It
//    would mean lane FS's `packDirectory` field at +0x10 is not where they
//    said, and the copy ctor faulted on a bad `this`.
//  * **NO fault at all, and the boot proceeding past FileSystem::Startup,
//    falsifies this hardest.** That would mean `FindFiles` is somehow already
//    satisfied — which the source checker says it is not — or that the gate is
//    zero and the whole body was skipped. The tell between those two is
//    trivial: with the gate zero, `[RtDiff] IO::FileSystem::LoadPackages` shows
//    `calls=1` with the shadow ABSENT, because the early return is before the
//    shadow. `calls=1` plus a shadow printing `pkgs=0` would mean the body ran
//    and mounted nothing, which is a different and much more interesting bug.
//  * A fault naming `??0PackageFile@` or `?ReadPackfile@` instead of
//    `?FindFiles@` would mean FindFiles IS satisfied from somewhere this lane
//    did not find — and would be good news, since it moves the wall two calls
//    further in one step.
//
// And the honest caveat on all four: **no run has happened.** This is derived
// from the disassembly and from the registration list, not observed.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/string.h"

// ResolveAddr, forward-declared at FILE scope rather than pulled in from
// standalone/resolve.h — that header drags in <windows.h>, whose <wingdi.h>
// declares a function called `Rectangle` and whose <windef.h> #defines `near`
// and `far` away. Must stay byte-identical to resolve.h's declaration.
//
// AND THE WARNING THAT GOES WITH IT: ResolveAddr(Module::Runtime, addr) is NOT
// "the DLL". It consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md). Each of the four
// addresses this file reaches by it was grepped for `RegisterRuntimeAddr(0x…`
// across runtime/ and standalone/ before use; none of the four is registered
// today, so each reaches the shipped body now and dies loudly at cutover, which
// is the intent (§4).
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIO {

// Wave 9 lane PKG2's accessor sweep, defined in package_file.cpp. Called from
// LoadPackages' tail inside the diff guard — see the comment at the call site.
void RtIo_PackageFile_SweepProbe(const FileSystem* fs);
// Lane PKG2 rung 3's lockstep probe (filesystem_openfile_probe.cpp).
void RtIo_FileSystemOpenFileProbe(const FileSystem* fs);
// Rung 4's operator= oracle (same file).
void RtIo_FileInfoAssignProbe();
// Rung 4b-ii's MemoryStream oracle (memory_stream_probe.cpp, namespace RtIo).
}  // namespace RtIO
// Lane PKG2 rung 2's lockstep probe (compressed_stream_probe.cpp). It lives in
// `RtIo`, not `RtIO`: the tree carries BOTH spellings — runtime/types/io/Stream.h
// is `RtIo` and runtime/types/io/FileSystem.h is `RtIO` — and rung 2 is built on
// Stream.h, so it follows Stream.h. Worth a line rather than a silent
// qualification: the two names differ by one character's case and the linker is
// the only thing that notices.
namespace RtIo { void RtIo_CompressedStreamReadProbe(); }
namespace RtIo { void RtIo_MemoryStreamProbe(); }   // rung 4b-ii (memory_stream_probe.cpp)
// Lane PKG2 iteration 2's codec probe. Declared here for the same reason and
// called from the same place — see the call site at the tail of LoadPackages.
namespace RtSystem { void RtSystem_CompressionProbe(); }
namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtMemAlloc;
using RtSystem::RtMemFree;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Set;

namespace {

// ---- doors ----------------------------------------------------------------
typedef void      (*VoidThisFn)(FileSystem*);
typedef Filename* (*GetPackDirectoryFn)(const FileSystem*, Filename*);
typedef Filename* (*FilenameCopyCtorFn)(Filename*, const Filename*);
typedef Filename* (*GetFileNameFn)(const PackageFileView*, Filename*, int);
typedef void      (*DeletingDtorFn)(void*, unsigned);

// `Collection::Array<Filename>` as LoadPackages builds it on its own stack:
// only _data (+0x00) and _size (+0x08) are ever written or read here — the
// body zeroes both at 0x1800ab48e/96 and never touches a capacity — so this is
// deliberately NOT the full 16-byte ArrayHdr. FindFiles receives `&tmp` and
// owns the growth.
struct FoundArray {
    Filename* data;
    int32_t   count;
    int32_t   _capacity;   // FindFiles' business, not read here
};

// `?_singleton@FileSystem@IO@@0PEAV12@EA`, ord 4811, retail 0x1801fc6a8, and
// MINTED (rt_data_mint_tables.inc:1569). Read through the port-aware door so it
// follows the mint at cutover.
//
// ClosePackages passes THIS, not its own `this`, to CloseFile — retail loads
// `MOV RCX,qword ptr [0x1801fc6a8]` at 0x1800abaef even though `this` is in
// RDI and is almost certainly the same object. Reproduced literally.
FileSystem** FileSystemSingletonSlot() {
    return RT_DATA_SLOT(FileSystem**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

// ---- the array-growth idiom, inlined TWICE by retail ----------------------
//
// This is Collection::Array<PackageFile*>::SetSize(n+1) with the element
// written in, and MSVC emitted it in full at 0x1800ab5bb and again at
// 0x1800ab75d — the two copies are instruction-for-instruction identical apart
// from register allocation. One C++ function serves both, exactly as
// runtime/src/collection/array.cpp does for its eleven identical destructors.
//
// Four things a "cleanup" would quietly break, so none of them is cleaned up:
//
//  * `if (newCount != oldCount)` guards the whole thing. newCount is
//    oldCount+1, so it is always true. Retail tests it anyway; so do we.
//  * The growth factor is `newCount * 3`, and ONLY when this is the very first
//    element past capacity (`newCount == oldCount+1`) AND the array was not
//    empty (`oldCount != 0`). A first-ever append therefore allocates capacity
//    1, not 3 — `LEA EAX,[R13+0x1] ; CMP R14D,EAX ; JNZ ; TEST R13D,R13D ; JZ`
//    at 0x1800ab5de.
//  * The copy loop starts at `oldCount` when the block did NOT move
//    (`CMOVZ EAX,R13D` on `CMP R15,[RSI+0x20]`) and at 0 when it did. It is one
//    loop doing both the memcpy and the append.
//  * `if (dst)` inside the copy loop is retail's and is kept: it can only fire
//    when the base is null, which is the out-of-memory path.
//
// The allocation goes through RtMemAlloc, which is
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) — verified by
// disassembly to be exactly the lock / Allocate(_singleton,size,align) /
// unlock sequence retail inlines here, same slots (0x18014b390 / 0x18014b388),
// same allocator singleton (0x1801fc718), same align 8. Likewise RtMemFree is
// 0x1800c7ad0 and carries retail's `if (p)` inside itself, which is why the
// callers below hand it a possibly-null pointer exactly as retail does.
void AppendPackfile(FileSystem* self, PackageFileView* obj) {
    PackageFileView** const old   = self->packfiles;
    const int32_t           oldN  = self->packfileCount;
    const int32_t           newN  = oldN + 1;
    if (newN == oldN)
        return;
    self->packfileCount = newN;
    if (newN > self->packfileCapacity) {
        self->packfileCapacity = newN;
        if (newN == oldN + 1 && oldN != 0)
            self->packfileCapacity = newN * 3;
        // MOVSXD RBX,dword[RSI+0x2c] ; SHL RBX,3 — a SIGN extension of the
        // capacity, then times 8. Ghidra's C prints this as a zero extension.
        const uint64_t bytes =
            static_cast<uint64_t>(static_cast<int64_t>(self->packfileCapacity)) * 8u;
        self->packfiles = static_cast<PackageFileView**>(RtMemAlloc(bytes, 8));
    }
    int64_t k = (old == self->packfiles) ? static_cast<int64_t>(oldN) : 0;
    for (; k < static_cast<int64_t>(newN); ++k) {
        PackageFileView** const dst = self->packfiles + k;
        if (dst != nullptr)
            *dst = (k < static_cast<int64_t>(oldN)) ? old[k] : obj;
    }
    if (old != self->packfiles && old != nullptr)
        RtMemFree(old);
}

// ---- the path -> {pack,file} index find -----------------------------------
//
// Transcribed from 0x1800ab8b6-0x1800ab90c. It is the same walk, with the same
// two ASYMMETRIC null-key arms, that runtime/src/system/configuration.cpp
// ports for the config map at 0x18002ace0 — a null SEARCH key returns the
// current node, a null NODE key descends — and it is the same walk again inside
// the unported insert at 0x180025c70. The ONE difference, and it is the whole
// reason this is not shared code with configuration.cpp: the comparator here is
// the CRT's `_stricmp` (0x18007eefc → 0x18007eec0), which lowercases 'A'-'Z'
// and compares UNSIGNED bytes, whereas the config map inlines a SIGNED-char
// strcmp. On any byte >= 0x80 those two disagree on the sign, and a package
// path can contain one.
//
// The comparator is reached by ADDRESS because 0x18007eefc has no export name
// in either build. It is NOT ported (§4), so today this reaches the shipped
// CRT copy and at cutover it is the loud death this lane wants rather than a
// quiet wrong answer from a substituted std::strcmp.
int PathStrICmp(const char* a, const char* b) {
    using Fn = int (*)(const char*, const char*);
    static Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18007eefc));
    return fn(a, b);
}

PackfileIndexNode* IndexFind(PackfileIndexNode* node, const char* key) {
    while (node != nullptr) {
        if (node->key != nullptr) {
            if (key == nullptr)                     { node = node->left; continue; }
            if (PathStrICmp(key, node->key) < 0)    { node = node->left; continue; }
        }
        // rightTest — 0x1800ab8e6
        if (key == nullptr)
            return node;
        if (node->key != nullptr && PathStrICmp(node->key, key) >= 0)
            return node;
        node = node->right;
    }
    return nullptr;
}

}  // namespace

// ============================================================================
// ?GetPackDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ  retail 0x180007c10
// ============================================================================
//     Filename FileSystem::GetPackDirectory() const
//     { return Filename(this->packDirectory); }      // copy ctor, AddRef only
//
// A String-RETURNING const member, so the hidden return slot is the SECOND
// argument (RCX=this, RDX=ret) and RAX is the return slot on the way out —
// `MOV RAX,RBX` at 0x180007c43. Getting that wrong is
// reference_hidden_retslot_wild_write.md.
//
// The `MOV dword ptr [RSP+0x20],0` / `,1` pair around the call is MSVC's EH
// state slot for the partially-constructed return value. It has no effect on
// the object and is not reproduced.
//
// ORACLE: full replay onto a second scratch Filename, compared BY POINTER —
// and with a disclosure attached, because this is the exact shape the lead
// warned about and it would be dishonest to print `divergences=0` and stop.
//
// The body AddRefs an existing buffer rather than allocating, so both sides
// come back holding the SAME `char*`, and a pointer compare is the strong form
// of the check: it verifies we copied from the right FIELD, which a text
// compare would not.
//
// **It still cannot distinguish +0x08 from +0x10 on a stock install.** Lane FS
// established that SetupDirectories seeds packDirectory as a copy of
// dataDirectory (retail's inlined `String::operator=` with RSI = this+0x10,
// RDI = this+8), and that assignment AddRefs the same buffer rather than
// duplicating it. So unless a `-pathpkg` switch is passed, this->dataDirectory
// and this->packDirectory are LITERALLY THE SAME POINTER, and a port that read
// the wrong one of the two would pass this oracle every time — pointer compare
// and text compare alike. It is a two-value population where both values are
// equal, which is not a verification
// (reference_recheck_must_match_its_own_proof.md, and the `TreeStrCmp` bug that
// hid behind 13 identical zeros for four waves).
//
// What actually pins the field is the disassembly: `LEA RDX,[RCX+0x10]` at
// 0x180007c2e. The oracle is a regression net, not the evidence.
extern "C" Filename* RtIo_FileSystem_GetPackDirectory(const FileSystem* self, Filename* ret) {
    RT_DIFF_BEGIN("IO::FileSystem::GetPackDirectory");

    RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(
        ret, reinterpret_cast<const Filename*>(&self->packDirectory));

    if (rt_diff_on__) {
        Filename theirs;
        theirs.data = nullptr;
        RtOriginal<GetPackDirectoryFn>(
            "?GetPackDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")(self, &theirs);
        char ours[80], desc[80];
        std::snprintf(ours, sizeof(ours), "%p \"%.40s\"", static_cast<void*>(ret->data),
                      ret->data ? ret->data : "(null)");
        std::snprintf(desc, sizeof(desc), "%p \"%.40s\"", static_cast<void*>(theirs.data),
                      theirs.data ? theirs.data : "(null)");
        RT_DIFF_SHADOW(ret->data == theirs.data, ours, desc);
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
        RT_DIFF_WORK(true);
    }
    return ret;
}

// ============================================================================
// ?GetFileName@PackageFile@IO@@QEBA?AVFilename@System@@H@Z    retail 0x180025450
// ============================================================================
//     Filename PackageFile::GetFileName(int i) const
//     { return Filename(*(Filename*)((char*)this->entries + i * 16)); }
//
//   MOVSXD RDX,R8D ; SHL RDX,0x4 ; ADD RDX,qword ptr [RCX+0xb0]
//
// The MOVSXD is a SIGN extension of the int index — the same instruction
// Ghidra's C prints as a zero extension, which cost Wave 6 three defects. It
// only matters for a negative index, which retail never passes; it is written
// as int64 anyway because that is what the instruction says.
//
// There is NO bounds check. Retail's caller (LOOP 3 below) bounds the index by
// `entryCount` itself, so the unguarded read is load-bearing and stays.
//
// ORACLE: full replay onto a scratch Filename, same reasoning as
// GetPackDirectory. Pure; reads `this` and writes only the return slot.
extern "C" Filename* RtIo_PackageFile_GetFileName(const PackageFileView* self,
                                                  Filename* ret, int index) {
    RT_DIFF_BEGIN("IO::PackageFile::GetFileName");

    const Filename* entry = reinterpret_cast<const Filename*>(
        static_cast<const uint8_t*>(self->entries) + static_cast<int64_t>(index) * 16);
    RT_EXPORT_ONCE(FilenameCopyCtorFn, "??0Filename@System@@QEAA@AEBV01@@Z")(ret, entry);

    if (rt_diff_on__) {
        Filename theirs;
        theirs.data = nullptr;
        RtOriginal<GetFileNameFn>(
            "?GetFileName@PackageFile@IO@@QEBA?AVFilename@System@@H@Z")(self, &theirs, index);
        // Same AddRef-not-allocate shape as GetPackDirectory, so pointer
        // equality is the right predicate. Unlike that one, the population here
        // is NOT uniform: this runs once per file in every package (tens of
        // thousands of distinct entries across data0/data1), and reading the
        // wrong entry — a stride of 8 instead of 16, say — gives a different
        // pointer on all but the zeroth. The text is printed so a mismatch
        // names both sides rather than two hex addresses.
        char ours[64], desc[64];
        std::snprintf(ours, sizeof(ours), "%.48s", ret->data    ? ret->data    : "(null)");
        std::snprintf(desc, sizeof(desc), "%.48s", theirs.data ? theirs.data : "(null)");
        RT_DIFF_SHADOW(ret->data == theirs.data, ours, desc);
        RtString_ReleaseInline(reinterpret_cast<String*>(&theirs));
        RT_DIFF_WORK(true);
    }
    return ret;
}

// ============================================================================
// ?ClosePackages@FileSystem@IO@@QEAAXXZ                      retail 0x1800abab0
// ============================================================================
// Unmounts everything LoadPackages mounted: closes each package's file handle,
// runs each PackageFile's deleting destructor, frees the array, then destroys
// the path index tree and zeroes both containers.
//
// ---------------------------------------------------------------------------
// ON THE BOOT PATH ITS ENTIRE BODY IS UNREACHABLE, AND THAT IS WHY IT LANDS
// ---------------------------------------------------------------------------
// This is lane FS §2's control-flow-reach pruning applied again. The FIRST call
// in the process comes from FileSystem::Startup on a freshly constructed
// FileSystem: packfileCount is 0, so the loop body never runs; packfiles is
// null, so the free is a no-op; packfileIndexRoot is null, so the tree teardown
// is skipped. Three unported callees — CloseFile, the recursive node free at
// 0x180008340, and PackageFile's deleting destructor through vtable slot 0 —
// are therefore NOT REACHED, and this row closes without them.
//
// **THAT IS TRUE OF THE FIRST CALL ONLY, AND IT IS NOT A GENERAL LICENCE.**
// The mods menu remounts packages (see project_mods_menu_closed.md), which
// calls this again with a POPULATED array and a POPULATED tree — and then all
// three run. They are written out below through their proper doors so that path
// dies with a name instead of silently leaking every package. Do not read the
// paragraph above as "these callees are dead"; read it as "these callees are
// dead on iteration 12's boot".
//
// ORACLE: none, and it is not close. It frees everything it touches; running it
// twice is a double free, and there is no scratch stand-in because the objects
// it destroys are the process's real packages.
extern "C" void RtIo_FileSystem_ClosePackages(FileSystem* self) {
    RT_DIFF_BEGIN("IO::FileSystem::ClosePackages");
    const int32_t hadPackfiles = self->packfileCount;
    const bool    hadIndex     = self->packfileIndexRoot != nullptr;

    for (int32_t i = 0; i < self->packfileCount; ++i) {
        // Retail RELOADS the array base on every iteration and indexes it twice
        // (0x1800abae0 and 0x1800abafb) rather than caching the element. Kept:
        // the deleting destructor below could in principle write the array.
        PackageFileView* pkg = self->packfiles[i];

        // CloseFile(FileSystem::_singleton, &pkg->fileInfo). The FileInfo lives
        // at pkg+0xC0 (`ADD RDX,0xc0`) and the receiver is the SINGLETON, not
        // `this` — see FileSystemSingletonSlot()'s comment.
        typedef void (*CloseFileFn)(FileSystem*, void*);
        RT_EXPORT_ONCE(CloseFileFn, "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z")(
            *FileSystemSingletonSlot(), reinterpret_cast<uint8_t*>(pkg) + 0xC0);

        PackageFileView* again = self->packfiles[i];
        if (again != nullptr) {
            // `MOV RAX,[RCX] ; MOV EDX,1 ; CALL qword ptr [RAX]` — vtable slot
            // 0, the VECTOR deleting destructor, flag 1 = also free the block.
            // Slot 0 of ??_7PackageFile@IO@@6B@ is 0x1800259d0 (§7); it is
            // reached through the object's own vtable pointer, faithfully, and
            // not by address, because after the mint the object will carry OUR
            // table and this must follow it.
            void** vt = *reinterpret_cast<void***>(again);
            reinterpret_cast<DeletingDtorFn>(vt[0])(again, 1u);
        }
    }

    // `if (p) { lock; Free; unlock }` with the null test inside 0x1800c7ad0.
    RtMemFree(self->packfiles);
    self->packfiles = nullptr;
    // Retail zeroes +0x28 as ONE QWORD (`MOV qword ptr [RDI+0x28],RBP`), which
    // clears the count AND the capacity. Unlike Array::SetSize, this DOES
    // shrink capacity — the next LoadPackages therefore reallocates from 1.
    self->packfileCount    = 0;
    self->packfileCapacity = 0;

    PackfileIndexNode* const root = self->packfileIndexRoot;
    if (root != nullptr) {
        // FUN_180008340(&tree, node) — the recursive subtree free, unexported
        // and UNPORTED (§4). Retail passes `this+0x40`, the tree object, as its
        // first argument and the child as its second, and calls it once per
        // child of the root; the root itself is unwound inline below.
        using TreeFreeFn = void (*)(void*, PackfileIndexNode*);
        static TreeFreeFn treeFree =
            reinterpret_cast<TreeFreeFn>(ResolveAddr(Module::Runtime, 0x180008340));
        void* tree = &self->packfileIndexRoot;
        treeFree(tree, root->left);
        treeFree(tree, root->right);

        // The root's key String, released with retail's inlined idiom — the
        // freeing branch leaves the slot holding `data - 8`, not null. That
        // asymmetry is retail's and RtString_ReleaseInline reproduces it.
        RtString_ReleaseInline(reinterpret_cast<String*>(const_cast<char**>(&root->key)));
        RtMemFree(root);
    }
    // Zeroed unconditionally, even when root was already null.
    self->packfileIndexCount = 0;
    self->packfileIndexRoot  = nullptr;

    if (rt_diff_on__) {
        char ours[96], desc[96];
        std::snprintf(ours, sizeof(ours), "closed=%d hadIndex=%d -> count=%d root=%p",
                      hadPackfiles, static_cast<int>(hadIndex), self->packfileCount,
                      static_cast<void*>(self->packfileIndexRoot));
        std::snprintf(desc, sizeof(desc), "SHADOW ONLY (destructive, not replayable)");
        RT_DIFF_SHADOW(self->packfiles == nullptr && self->packfileCount == 0 &&
                       self->packfileIndexRoot == nullptr && self->packfileIndexCount == 0,
                       ours, desc);
        // Deliberately reports work only when there WAS something to close, so
        // `calls=` does not read as "did work" on the boot call that finds an
        // empty FileSystem (reference_probe_needs_an_effectiveness_control.md).
        RT_DIFF_WORK(hadPackfiles != 0 || hadIndex);
    }
}

// ============================================================================
// ?LoadPackages@FileSystem@IO@@QEAAXXZ                       retail 0x1800ab460
// ============================================================================
// See §2 for the shape and §5 for why there is no oracle.
extern "C" void RtIo_FileSystem_LoadPackages(FileSystem* self) {
    RT_DIFF_BEGIN("IO::FileSystem::LoadPackages");

    // 0x1800ab47f — the packfile gate. See §3: it is 1 by the time we get here,
    // set by FileSystem::Startup between SetupDirectories and this call, so the
    // early return is NOT the live path. It is transcribed because it IS the
    // body's first instruction, and because `-onlypkg` is not the only thing
    // that can clear the byte over the life of a process.
    if (self->existsInPackfileGate == 0)
        return;

    RT_EXPORT_ONCE(VoidThisFn, "?ClosePackages@FileSystem@IO@@QEAAXXZ")(self);

    FoundArray found;
    found.data      = nullptr;   // MOV qword ptr [RBP-0x20],0
    found.count     = 0;         // MOV qword ptr [RBP-0x18],0
    found._capacity = 0;

    String pattern;
    pattern.data = nullptr;      // MOV qword ptr [RBP+0x48],0
    RtSystem_String_Set(&pattern, "*.pkg");   // literal read at 0x1801556f0

    Filename packDir;
    packDir.data = nullptr;
    RT_EXPORT_ONCE(GetPackDirectoryFn,
        "?GetPackDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ")(self, &packDir);

    // FindFiles(dir, pattern, recursive=false, out). Retail passes the value
    // GetPackDirectory RETURNED (RAX) as RDX, which is the same pointer as
    // &packDir — the hidden return slot is handed straight on. Fifth argument
    // goes on the stack at [RSP+0x20].
    //
    // UNPORTED (§4). THIS IS THE PREDICTED ATTEMPT-12 FAULT (§8).
    typedef void (*FindFilesFn)(const FileSystem*, const Filename*, const String*,
                                bool, FoundArray*);
    RT_EXPORT_ONCE(FindFilesFn,
        "?FindFiles@FileSystem@IO@@QEBAXAEBVString@System@@0_NAEAV?$Array@VFilename@System@@@Collection@@@Z")(
            self, &packDir, &pattern, false, &found);

    // Released in retail's order: packDir (+0x50) first, then pattern (+0x48).
    RtString_ReleaseInline(reinterpret_cast<String*>(&packDir));
    RtString_ReleaseInline(&pattern);

    // ---- LOOP 1: one PackageFile per discovered *.pkg ---------------------
    // Retail allocates 0xF0 bytes aligned 8 under the allocator lock, runs the
    // constructor only `if (p)`, then ReadPackfile, then appends. `??0PackageFile`
    // and `?ReadPackfile@` are both UNPORTED (§4); ReadPackfile is 6,597 bytes
    // and is a lane of its own.
    typedef PackageFileView* (*PackageFileCtorFn)(void*);
    typedef bool             (*ReadPackfileFn)(PackageFileView*, const Filename*);
    for (int32_t i = 0; i < found.count; ++i) {
        void* block = RtMemAlloc(0xF0u, 8);
        PackageFileView* pkg = nullptr;
        if (block != nullptr)
            pkg = RT_EXPORT_ONCE(PackageFileCtorFn, "??0PackageFile@IO@@QEAA@XZ")(block);
        // Unguarded on `pkg` — retail calls ReadPackfile with whatever the
        // constructor branch left in R12, including zero on the out-of-memory
        // path (`XOR R12D,R12D` at 0x1800ab5a5). Faithful; not a null check.
        RT_EXPORT_ONCE(ReadPackfileFn,
            "?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z")(
                pkg, &found.data[i]);
        AppendPackfile(self, pkg);
    }

    // ---- the packfile sort ------------------------------------------------
    // FUN_1800264a0(packfiles, count) — an MSVC introsort over PackageFile*
    // whose ENTIRE comparator is `MOV EAX,[RAX+0x70] ; CMP dword ptr [RCX+0x70],
    // EAX`, i.e. the +0x70 priority, and which orders DESCENDING (its selection
    // pass swaps the ARGMIN to the tail — `CMOVG RDX,R8` moves the tracker to
    // the SMALLER key). The third argument retail passes at the call site
    // (`LEA R8,[RBP+0x48]`) is DEAD: the callee never reads R8.
    //
    // DELIBERATELY NOT PORTED, and the reason is worth stating rather than
    // burying. It is 110 instructions of introsort with an explicit 62-entry
    // pending-range stack, median-of-3 and two partition scans, and every
    // PackageFile arrives here with the SAME +0x70 (the constructor seeds
    // 0x2710 and only ReadPackfile changes it). A tie-heavy population is
    // exactly where an unstable sort's exact swap sequence decides the answer,
    // and that answer decides which package owns a colliding path in LOOP 3.
    // A "clean" std::sort here would compile, run, and silently pick different
    // winners. With no run and no oracle available in this lane, transcribing
    // it would be an unverified guess at a precedence rule, so it goes through
    // the address door and is named in §4 for whoever ports it with a harness.
    if (self->packfileCount > 1) {
        using SortFn = void (*)(PackageFileView**, int32_t);
        static SortFn sortFn =
            reinterpret_cast<SortFn>(ResolveAddr(Module::Runtime, 0x1800264a0));
        sortFn(self->packfiles, self->packfileCount);
    }

    // ---- LOOP 2: the user/mod pack filenames already on the FileSystem -----
    // Same allocation and append as loop 1; the only difference is the source
    // (this->userPackFilenames[i], +0x30, bounded by +0x38) and that there is
    // NO sort afterwards — mods keep the order they were mounted in.
    for (int32_t i = 0; i < self->userPackFilenameCount; ++i) {
        void* block = RtMemAlloc(0xF0u, 8);
        PackageFileView* pkg = nullptr;
        if (block != nullptr)
            pkg = RT_EXPORT_ONCE(PackageFileCtorFn, "??0PackageFile@IO@@QEAA@XZ")(block);
        RT_EXPORT_ONCE(ReadPackfileFn,
            "?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z")(
                pkg, reinterpret_cast<const Filename*>(&self->userPackFilenames[i]));
        AppendPackfile(self, pkg);
    }

    // ---- LOOP 3: the path -> {packIndex, fileIndex} index ------------------
    int64_t inserted = 0, overridden = 0, kept = 0;
    for (int32_t pi = 0; pi < self->packfileCount; ++pi) {
        PackageFileView* pkg = self->packfiles[pi];
        const int32_t entries = pkg->entryCount;          // MOV EAX,[RCX+0xb8]
        for (int32_t fi = 0; fi < entries; ++fi) {
            // Retail RE-INDEXES the array here too rather than reusing `pkg`.
            PackageFileView* owner = self->packfiles[pi];

            // GetFileName writes through the hidden return slot and its callee
            // (the Filename copy ctor) zeroes the slot first, so `key` is
            // deliberately not pre-initialised — matching retail, which passes
            // an untouched [RBP+0x50].
            Filename key;
            RT_EXPORT_ONCE(GetFileNameFn,
                "?GetFileName@PackageFile@IO@@QEBA?AVFilename@System@@H@Z")(owner, &key, fi);

            PackfileIndexNode* node = IndexFind(self->packfileIndexRoot, key.data);
            if (node != nullptr) {
                // The incumbent's OWN +0xAC decides, not the newcomer's: retail
                // indexes packfiles[] with the FOUND NODE's packIndex
                // (`MOV ECX,[RBX+0x8]`) and tests that object's byte.
                PackageFileView* incumbent = self->packfiles[node->packIndex];
                if (incumbent->overridable != 0) {
                    // Retail merges these into ONE qword store at [RBX+8].
                    node->packIndex = static_cast<uint32_t>(pi);
                    node->fileIndex = static_cast<uint32_t>(fi);
                    ++overridden;
                } else {
                    ++kept;
                }
            } else {
                // FUN_180025a80(&tree, &out, &key, &value) — UNPORTED (§4).
                // RCX is `this+0x40`, the {root,count} tree object; RDX is a
                // scratch out-slot retail never reads afterwards.
                using BstInsertFn = void* (*)(void*, PackfileIndexNode**,
                                              const Filename*, const uint64_t*);
                static BstInsertFn insertFn =
                    reinterpret_cast<BstInsertFn>(ResolveAddr(Module::Runtime, 0x180025a80));
                const uint64_t value = (static_cast<uint64_t>(static_cast<uint32_t>(fi)) << 32) |
                                        static_cast<uint32_t>(pi);
                PackfileIndexNode* out = nullptr;
                insertFn(&self->packfileIndexRoot, &out, &key, &value);
                ++inserted;
            }

            RtString_ReleaseInline(reinterpret_cast<String*>(&key));
        }
    }

    // ---- teardown of the temporary Filename array -------------------------
    // Retail releases each element with the inlined String idiom and then frees
    // the block, and it does BOTH unconditionally of the count being zero (the
    // block free is guarded only on the pointer). Note it uses the same
    // lock/Free/unlock as everywhere else; RtMemFree is that sequence with the
    // null test inside.
    const int32_t foundCount = found.count;   // the shadow below runs after the zeroing
    for (int64_t i = 0; i < static_cast<int64_t>(found.count); ++i)
        RtString_ReleaseInline(reinterpret_cast<String*>(&found.data[i]));
    RtMemFree(found.data);
    found.data  = nullptr;
    found.count = 0;

    if (rt_diff_on__) {
        char ours[160], desc[96];
        std::snprintf(ours, sizeof(ours),
                      "pkgs=%d cap=%d found=%d index{ins=%lld over=%lld kept=%lld} n=%d",
                      self->packfileCount, self->packfileCapacity, foundCount,
                      static_cast<long long>(inserted), static_cast<long long>(overridden),
                      static_cast<long long>(kept), self->packfileIndexCount);
        std::snprintf(desc, sizeof(desc), "SHADOW ONLY (mounts packages, not replayable)");
        // The predicate is what the body CLAIMS, and every term is named in the
        // message above, per reference_shadow_predicate_must_name_its_failing_term.md:
        // the tree got a node for each first-seen path, and the array is
        // non-empty iff something was mounted.
        RT_DIFF_SHADOW(self->packfileIndexCount == static_cast<int32_t>(inserted) &&
                       (self->packfileCount == 0) == (self->packfiles == nullptr),
                       ours, desc);
        RT_DIFF_WORK(self->packfileCount != 0);

        // Wave 9 lane PKG2. This is the one moment in the process where every
        // mounted PackageFile is reachable AND fully populated, so it is the
        // only place lane PKG2's twenty leaf accessors can get real inputs
        // instead of reporting calls=0 (reference_uncalled_port_is_not_verified.md).
        // Inside the diff guard on purpose: it is an oracle driver, not work,
        // and a normal run must not pay for it. Declared here rather than in a
        // header because it is one symbol used at one site.
        RtIo_PackageFile_SweepProbe(self);

        // Wave 9 lane PKG2 iteration 2. Same reasoning, one level down: the
        // Compression probe builds a System::Compression through the DLL's ctor,
        // which ALLOCATES. It cannot run from RtSystemPortInit, where the
        // allocator singleton at 0x1801fc718 is still null (a first draft did,
        // and crashed writing to 0x648 with nothing in the dump naming it).
        // Here the heap is provably live — the body above allocated a 0xF0
        // PackageFile per package to get this far.
        RtSystem::RtSystem_CompressionProbe();

        // Rung 2's lockstep probe. Same placement and the same reason as the
        // two above: it builds two CompressedMemoryStreams, which ALLOCATE, so
        // it cannot run from a *PortInit where the allocator singleton is still
        // null. This is the first point on the boot path where the heap is
        // provably live.
        RtIo::RtIo_CompressedStreamReadProbe();

        // Rung 3's probe. It needs the packfile INDEX populated, which is what
        // the loops above just built -- this is the earliest point in the
        // process where an OpenFile of a packed name can succeed at all.
        RtIo_FileSystemOpenFileProbe(self);
        RtIo_FileInfoAssignProbe();
        RtIo::RtIo_MemoryStreamProbe();
    }
}

}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
int RtIoFileSystemPackagesRegister() {
    using namespace RtIO;
    int n = 0;
    RegisterRuntimeExport("?LoadPackages@FileSystem@IO@@QEAAXXZ",
                          (void*)&RtIo_FileSystem_LoadPackages); ++n;
    RegisterRuntimeAddr(0x1800ab460, (void*)&RtIo_FileSystem_LoadPackages);

    RegisterRuntimeExport("?ClosePackages@FileSystem@IO@@QEAAXXZ",
                          (void*)&RtIo_FileSystem_ClosePackages); ++n;
    RegisterRuntimeAddr(0x1800abab0, (void*)&RtIo_FileSystem_ClosePackages);

    RegisterRuntimeExport("?GetPackDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ",
                          (void*)&RtIo_FileSystem_GetPackDirectory); ++n;
    RegisterRuntimeAddr(0x180007c10, (void*)&RtIo_FileSystem_GetPackDirectory);

    RegisterRuntimeExport("?GetFileName@PackageFile@IO@@QEBA?AVFilename@System@@H@Z",
                          (void*)&RtIo_PackageFile_GetFileName); ++n;
    RegisterRuntimeAddr(0x180025450, (void*)&RtIo_PackageFile_GetFileName);

    // DELIBERATELY ABSENT — the callees this row exposes. Sizes and reasons in
    // §4; each is called through its proper door above, so reaching one is a
    // NAMED death, not a silent no-op.
    //   ?FindFiles@FileSystem@IO@@QEBAX…       0x1800ad7a0  ord 2350   289 instrs
    //   ??0PackageFile@IO@@QEAA@XZ             0x1800b4440  ord  289    48 instrs
    //   ?ReadPackfile@PackageFile@IO@@QEAA_N…  0x1800b46b0  ord 3461  6,597 bytes
    //   ?CloseFile@FileSystem@IO@@QEAAX…       0x1800accc0  ord 1726
    //   FUN_1800264a0 (packfile sort)          0x1800264a0             110 instrs
    //   FUN_180025a80 (BST insert)             0x180025a80
    //   FUN_180008340 (index-node free)        0x180008340
    //   FUN_18007eefc (_stricmp)               0x18007eefc
    return n;
}
