// ============================================================================
// runtime/src/io/file_stream.cpp — Wave 19 lane FILE
// IO::FileStream / BinaryFileStream / TextFileStream lifecycle + Create
// ============================================================================
//
//   | Export                                              | Retail      | Ord  | Instrs |
//   |-----------------------------------------------------|-------------|------|--------|
//   | ??0BinaryFileStream@IO@@QEAA@XZ                     | 0x1800a8200 |   55 |   45   |
//   | ??1BinaryFileStream@IO@@UEAA@XZ                     | 0x180023c80 |  532 |    5   |
//   | ??1FileStream@IO@@UEAA@XZ                           | 0x1800aaab0 |  593 |   32   |
//   | ??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z  | 0x1800ba340 |  441 |   58   |
//   | ?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z| 0x1800aa8f0 | 1876 |   63   |
//
// plus the UNEXPORTED `~FileInfo` at `0x18000c180` (25 instrs), which
// `~FileStream` calls and which nothing in this tree had ported —
// external_savetext_win.cpp transcribes it inline for its own inlined epilogue
// but registers nothing, so to a callee census it was absent
// (`reference_half_registered_port_reads_as_absent.md`).
//
// Names, ordinals and addresses printed out of Runtime-x64.dll's export
// directory with `pefile`, not grepped and not carried over from a doc.
//
//
// ---------------------------------------------------------------------------
// WHY THIS LANE EXISTS: THE SCREENSHOT CUTOVER HOLE
// ---------------------------------------------------------------------------
// Wave 18 lane VRES ported `?SaveImageToFile@ImageBuffer@Video@@` and said so
// in its own §T1.2: porting it moved the screenshot hole from "no
// implementation" to "an implementation with unresolved IO dependencies". Those
// dependencies are the rows above plus the FileSystem rows in
// filesystem_createfile_win.cpp. `ResolveExport` FALLS THROUGH to the shipped
// body for an unported symbol, so before this file existed the whole path
// worked with the DLL present and resolved to null without it.
//
// NO LAYOUT IN THIS FILE WAS RE-DERIVED. `runtime/types/io/FileStream.h` has
// pinned this whole family since Wave 11 lane IOSTREAM — sizeof 0x50, filename
// +0x18, FileInfo +0x20, the Win32 handle at +0x38, the byte cursor at +0x2c,
// and TextFileStream's ten fields with the `objectName`/`objectType` pair the
// RIGHT way round (that header records having had them swapped until retail's
// own one-instruction accessors were run against it).
//
// ---------------------------------------------------------------------------
// THE ONE FACT A SIGNATURE WOULD GET WRONG
// ---------------------------------------------------------------------------
// `?CreateFileW@FileSystem@IO@@` RETURNS A `FileInfo` BY VALUE, so its hidden
// return slot is RDX and the `Filename*` is R8 — three arguments in the
// typedef, not two (`reference_hidden_retslot_wild_write.md`). `?Create@
// FileStream@` at 0x1800aa963/96f loads `LEA RDX,[RSP+0x28]` (a 0x30-byte stack
// FileInfo) before the call and then hands the RETURNED pointer to
// FileInfo::operator=. A two-argument typedef writes 0x30 bytes of the caller's
// frame and still returns the right answer.
//
// ---------------------------------------------------------------------------
// TWO VTABLES IN THIS FILE HAVE NO EXPORTED NAME — CHECKED, NOT ASSUMED
// ---------------------------------------------------------------------------
// The first draft of this file stamped the text Encoding through a name it had
// invented, `??_7TextEncoding@IO@@6B@`. That name is a PHANTOM: 0x1801559d0 has
// NO export at all, and a ResolveExport name the DLL does not contain returns
// NULL SILENTLY (`reference_resolveexport_phantom_names.md`). It was caught by
// running every mangled name in this file against the shipped export directory
// with `pefile` BEFORE the body was written, which is the check the doctrine
// asks for and the only reason it was not shipped.
//
// Verified, by VA, against the export directory:
//   0x1801549a0  ??_7Stream@IO@@6B@             EXPORTED   — reached by name
//   0x180154960  ??_7FileStream@IO@@6B@         EXPORTED   — reached by name
//   0x1801554d0  ??_7BinaryFileStream@IO@@6B@   EXPORTED   — reached by name
//   0x180155980  ??_7TextFileStream@IO@@6B@     EXPORTED   — reached by name
//   0x1801554a8  the shared binary Encoding     NO EXPORT  — reached by ADDRESS
//   0x1801559d0  the text Encoding              NO EXPORT  — reached by ADDRESS
//
// compressedstream.cpp:524 reached the same conclusion about 0x1801554a8: for
// an unexported table the address door is the ONLY port-aware door.
//
// ---------------------------------------------------------------------------
// WHAT IS *NOT* PORTED HERE, NAMED RATHER THAN SILENT
// ---------------------------------------------------------------------------
//   ??_7FileStream@IO@@6B@ (0x180154960), ??_7BinaryFileStream@ (0x1801554d0)
//   and ??_7TextFileStream@ (0x180155980) were DEFERRED to lane MINT by the
//   Wave 19 brief.
//
//   [CORRECTED 2026-09-10, post-report. What stood here said all three appear
//   ZERO times by VA in standalone/rt_data_mint_tables.inc, citing WAVE19_IO.md
//   §0b and phase 0c. That was measured at 20aa588 and is now FALSE: phase 0b
//   regenerated the mint and all three are minted, verified by VA on this tip.
//   The cause was neither a generator bug nor a harvest gap -- the generated
//   file was simply STALE, last regenerated at 4f037fd (Wave 10D) while the
//   consumers that put these tables into the census landed after it. A
//   GENERATED ARTEFACT IN THE TREE IS A MEASUREMENT WITH A TIMESTAMP, exactly
//   like a stated blocker, and this comment was a second-hand copy of one.
//   Recorded rather than quietly rewritten.]
//
//   ??_7FileStream@ and ??_7BinaryFileStream@ are minted, and this file's
//   post-report tranche clears BOTH to TRAP=0. Promotion is a separate question
//   and the two rows differ on it.
//
//   THE STAMPER COUNTS BELOW ARE MEASURED HERE, and both differ from the ones
//   this lane was briefed with. `get_xrefs_to` is cheap and neither figure had
//   been run:
//
//     ??_7BinaryFileStream@ (0x1801554d0) — briefed as ONE stamper, measured
//       **TWO**: 0x1800a8270 (the default ctor) and 0x180023cfe (the COPY
//       ctor, 0x180023ca0). Both are ported by this lane, so the row IS fully
//       owned — but only because the copy ctor was added. On the briefed claim
//       it would have been declared owned with a stamper still in the DLL.
//
//     ??_7FileStream@ (0x180154960) — briefed as NINE stampers, measured about
//       **SEVENTEEN**, of which five are already ours (SaveText, ~FileStream,
//       ??0TextFileStream, ??0BinaryFileStream, SaveImageToFile). The
//       CONCLUSION the brief drew still holds — Engine::Run (two sites) and
//       three unexported bodies are not ours, so TRAP=0 here is NOT a
//       promotion (reference_a_ported_row_can_be_permanently_unpromotable.md)
//       — but the number it rested on does not.
//
//   ??_7TextFileStream@ is minted at 20 SLOTS AND IS REALLY 10 -- an unexported
//   table sits at 0x1801559d0, exactly ten slots in, and the generator's
//   adjacent-export bound cannot see it. Reported to lane MINT; the row is
//   handed back rather than taken here.
//
//   ??_7Stream@IO@@6B@ (0x1801549a0) and the shared Encoding table
//   (0x1801554a8) ARE minted already — checked BY VA in
//   rt_data_mint_tables.inc, not by name. Phase 0b warns that the name grep
//   returns `adjacent-export:` labels on OTHER rows and looks exactly like a
//   hit; 0x1801559d0 is NOT minted.
//
//   MINTED IS NOT PROMOTED, and this is where a reader of the above will go
//   wrong. None of these tables is promoted: a run reports
//   only **5 of 62** minted VTABLES are registered on a default run, so
//   ??_7Stream@ prints
//   READY / TRAP=0 on the readiness screen and is STILL AN OPEN DOOR that this
//   lane's own cutover screen catches
//   (reference_a_green_readiness_screen_is_not_a_route.md).
//
//   ??0FileStream@IO@@QEAA@XZ (0x1800aaa10) is deliberately absent. Retail
//   INLINES it into both constructors below rather than calling it, and the
//   census does not list it as called-but-unported. Both constructors here
//   transcribe the inlined copy, which is what retail executes.
//
// PORTABILITY: no <windows.h> in this file (PLAN.md §4.1 decision 1). Nothing
// here touches Win32 — the bodies that do are in filesystem_createfile_win.cpp
// beside it.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileStream.h"
#include <new>          // B.3.4 block 4: the GetSource door placement-constructs
                       // retail's by-value String into the caller's raw slot

#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// standalone/resolve.h:108. The RAW door: it answers from the loaded DLL and
// never consults the port registry, which is what makes it usable as the
// "theirs" side of a comparison. Declared here rather than by including
// standalone/resolve.h, matching how ResolveAddr is already declared above.

// Rung 4 of Wave 9 lane PKG2 (filesystem_openfile_win.cpp), already ported and
// registered. Declared here rather than pulled in from a header because that
// lane publishes none and this lane does not edit another lane's files.
extern "C" RtIO::FileInfo* RtIo_FileInfo_Assign(RtIO::FileInfo*, const RtIO::FileInfo*);
extern "C" RtIO::FileInfo* RtIo_FileInfo_CopyCtor(RtIO::FileInfo*, const RtIO::FileInfo*);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;
using RtSystem::StringRefCount;
using RtSystem::Sys;      // the transitional POD->class bridge (pod_bridge.h)
using RtSystem::SysFile;  //   "

// The stream layouts live in `RtIo` (lower-case o) while FileInfo/FileSystem
// live in `RtIO`. Both spellings are long-standing in this tree — FileSystem.h
// says so at the top of the file — and this lane does not rename either.
using RtIo::Stream;
using RtIo::FileStream;
using RtIo::BinaryFileStream;
using RtIo::TextFileStream;

// The FileSystem rows this file calls, ported in filesystem_createfile_win.cpp
// by this same lane. Called DIRECTLY, not through RT_EXPORT_ONCE: they are
// ported, and a door here would only be a way to reach the shipped copy by
// accident (`reference_converting_a_door_is_not_satisfying_it.md`).
bool      RtIo_FileSystem_CreatePath(const FileSystem* self, const Filename* path);
void      RtIo_FileSystem_CloseFile(FileSystem* self, FileInfo* info);
FileInfo* RtIo_FileSystem_CreateFileW(FileSystem* self, FileInfo* ret,
                                      const Filename* path);

// Wave 9 lane PKG2 rung 3 (filesystem_openfile_win.cpp), already ported and
// registered. The post-report slot tranche below calls both DIRECTLY.
}  // namespace RtIO
extern "C" bool      RtIo_FileSystem_ReadFile(RtIO::FileSystem*, RtIO::FileInfo*,
                                             int32_t, void*);
extern "C" RtIO::FileInfo* RtIo_FileSystem_OpenFile(RtIO::FileSystem*, RtIO::FileInfo*,
                                                   const RtSystem::Filename*);
namespace RtIO {

namespace {

// The FileSystem singleton, read exactly where retail reads it. `?_singleton@
// FileSystem@IO@@0PEAV12@EA` is exported DATA (ord 4811) — a `0`/`AEAA` in a
// mangling means PRIVATE, not unexported
// (`reference_aeaa_is_private_not_unexported.md`).
FileSystem* Fs() {
    FileSystem** slot = RT_DATA_SLOT(FileSystem**,
                                     "?_singleton@FileSystem@IO@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}

// `if (p) { <guarded delete> }` — the unexported object free at 0x180006a10 that
// every Encoding teardown in this family calls. Registered by
// runtime/src/video/context_lifecycle.cpp:697, so ResolveAddr finds our body
// first: ResolveAddr(Module::Runtime, …) is REGISTRY-FIRST, not "the DLL"
// (`reference_resolveaddr_consults_port_registry_first.md`).
void EncodingDelete(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(p);
}

// The String-release idiom on a `char*` slot. FileStream::filename and
// TextFileStream's two object strings are declared `char*` in FileStream.h (a
// System::String is exactly that one pointer).
void ReleaseStrSlot(char** slot) {
    // ~String (0x180006c80), including retail's release asymmetry (String.h).
    reinterpret_cast<System::String*>(slot)->~String();
}

// ---------------------------------------------------------------------------
// The inlined `??0FileStream` prologue both constructors below open with.
//
// Retail writes +0x00 (twice — Stream's table then FileStream's), +0x08, +0x10,
// +0x18, then the FileInfo's +0x00 / +0x08 / +0x0c / +0x10, +0x20, +0x28 and
// +0x18.
//
// IT NEVER TOUCHES FileInfo::_unk04 (object +0x24). FileStream.h already
// records that, and filesystem_openfile_win.cpp's oracle already excludes the
// same field for the same reason. A `= {}` here would be a behavioural
// difference from retail, not tidiness.
// ---------------------------------------------------------------------------
void FileStreamCtorInline(FileStream* self) {
    self->base.vptr = RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");      // 1800a8219
    self->base.direction = 0;                                              // 1800a8225
    self->base.encoding  = nullptr;                                        // 1800a8228
    self->base.vptr = RT_DATA_SLOT(const void*, "??_7FileStream@IO@@6B@");  // 1800a822c
    self->filename  = nullptr;                                             // 1800a8236

    FileInfo* fi = &self->file;                                            // 1800a823a
    fi->handle      = -1;                                                  // 1800a8243
    fi->size        = 0;                                                   // 1800a8249 (qword)
    fi->cursor      = 0;                                                   //   covers +0x0c
    fi->baseOffset  = 0;                                                   // 1800a824d
    fi->_name0.data = nullptr;                                             // 1800a8259
    fi->_name1.data = nullptr;                                             // 1800a8265
    fi->winHandle   = -1;                                                  // 1800a8268
}

// ??0FileStream@IO@@QEAA@XZ (0x1800aaa10, ordinal per list_exports) is exactly
// the prologue above and nothing else -- eleven stores, `MOV RAX,RCX`, RET --
// which is why every caller in this tree that constructs a FileStream on the
// stack (io/closure_batch2b_win.cpp, the persistence jobs) reaches it by name.
// Registered so those by-name resolves land here at cutover.
extern "C" FileStream* RtIo_FileStream_Ctor(FileStream* self) {
    FileStreamCtorInline(self);
    return self;
}

// ---------------------------------------------------------------------------
// The Encoding both constructors install at Stream::encoding (+0x10).
//
// Retail open-codes `EnterCriticalSection(_singleton+0x640);
// Allocate(_singleton, 8, 4); LeaveCriticalSection(...)`, which is byte-for-byte
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) — the same
// composition runtime/src/system/allocator.cpp:684 documents. So RtMemAlloc IS
// the faithful transcription, not a paraphrase of it.
//
// THE NULL BRANCH IS RETAIL'S, NOT A GUARD. `TEST RSI,RSI / JZ` at 0x1800a82b9
// stamps the vtable only when the allocation succeeded and otherwise installs a
// NULL encoding. Reproduced rather than tidied.
//
// The old encoding is deleted BEFORE the new one is stored, and the delete is
// itself conditional (0x1800a82ce). On a fresh object it is always null; the
// branch is transcribed because it is a statement retail makes.
//
// ENDGAME B.3.9 IO: the encoding is the REAL class (`IO::BinaryEncoding` /
// `IO::TextEncoding`), constructed in place; its ctor is retail's single vptr
// store, so the 8-byte object is what the old by-address stamp produced.
// ---------------------------------------------------------------------------
template <class EncodingT>
void InstallEncoding(Stream* s) {
    void* enc = RtSystem::RtMemAlloc(8, 4);                                // 1800a82a5
    if (enc) ::new (enc) EncodingT();                                      // 1800a82bb
    if (s->encoding) EncodingDelete(s->encoding);                          // 1800a82d3
    s->encoding = enc;                                                     // 1800a82d8
}

typedef void (*SetObjectFn)(TextFileStream*, const String*, const String*, int32_t);

}  // namespace

// ============================================================================
// ATTENDANCE COUNTERS — the `calls=` half of the verification
// ============================================================================
// The door screen in file_stream_cutover_probe.cpp is STATIC: it proves the
// registry answers each door, not that anything walked through one. A body that
// is registered and never entered is indistinguishable from a working one on
// that screen alone, and `calls=0` has four causes that invert the conclusion
// (WAVE19_IO.md §6.5).
//
// These are unconditional and cheap (one increment). They are printed by the
// cutover probe so the attendance figures sit beside the door verdicts rather
// than in a different part of the log.
//
// RT_DIFF's counters would have answered the same question and were tried
// FIRST: the old DLL-era switch makes every diffed port also run the original, and
// the autostart route did not reach its exit inside a 900s timeout, so the
// atexit report never printed at all. Recorded rather than dropped — "the
// instrument existed and was too slow for this route" is a different fact from
// "there was no instrument".
// ============================================================================
struct FileStreamCounters {
    uint64_t binCtor, binDtor, fsDtor, textCtor, create, createTrue, fileInfoDtor;
};
FileStreamCounters g_fsc{};

// ============================================================================
// WAVE 20 LANE PROMOTE'S `BfsPromoteCounters` IS GONE — B34_RULES section 4.1
// ============================================================================
// That struct, its global, `OurBfsTable()`, `NoteBfsStamp()`,
// `NoteSlotDispatch()`, their seven call sites and the six-number exporter were
// all deleted by ENDGAME B.3.4 block 4, in the commit that made
// `IO::BinaryFileStream` a real class. They are not left counting zero and not
// left counting a constant.
//
// WHAT THEY ASKED, AND WHY THE QUESTION NO LONGER EXISTS. `??_7FileStream@` and
// `??_7BinaryFileStream@` are byte-identical in slots 1..7 and differ only at
// slot 0, so a per-body counter could not say WHICH of the two promoted rows
// had dispatched into a shared body; the instrument compared the live object's
// vptr against the table our own constructors stamp, to separate the tiers.
//
// With `IO::FileStream` and `IO::BinaryFileStream` declared, that is answered
// structurally: there is exactly one table per class, the compiler's, the two
// are distinct types, and `p->Type()`-style identity is the standing rule
// (B33 section 5). There is nothing to port forward.
//
// Two of its six fields had already stopped meaning anything: the DLL-side
// counter was never incremented once the DLL was removed, and the two ctor
// counters could no longer differ — the stamp helper's own body said so ("the
// minted table is the only table now").
//
// `FileStreamCounters` and `SlotCounters` in this file are a DIFFERENT
// instrument (plain per-body attendance, no vptr identity) and are untouched.
// ============================================================================


// ============================================================================
// ~FileInfo                                              retail 0x18000c180
// ============================================================================
// UNEXPORTED, 25 instructions, and it is nothing but the String release idiom
// twice — _name1 (+0x28) FIRST, then _name0 (+0x20). Ported and registered by
// ADDRESS here because `~FileStream` calls it and nothing else in the tree had
// registered it; at cutover an unregistered address is a call through null
// (`reference_by_address_door_dies_at_cutover.md`).
//
// The release idiom's ASYMMETRY is retail's and is preserved by
// RtString_ReleaseInline: on the freeing branch the slot is left holding
// `data - 8` (0x18000c1b0 `ADD qword [RAX],-0x8` before the free) and only the
// NON-freeing branch nulls it (0x18000c1be).
// ============================================================================
extern "C" void RtIo_FileInfo_Dtor(FileInfo* self) {
    ++g_fsc.fileInfoDtor;
    SysFile(self->_name1).~Filename();       // 18000c19f  +0x28 FIRST
    SysFile(self->_name0).~Filename();       // 18000c1ce  +0x20
}

// ============================================================================
// IO::FileStream::~FileStream                            retail 0x1800aaab0
// ============================================================================
// Six steps, in retail's order:
//   1. stamp ??_7FileStream@IO@@6B@                          1800aaacb
//   2. if (file.handle != -1) FileSystem::CloseFile(_singleton, &file)
//                                                            1800aaad5..ae6
//   3. ~FileInfo(&file)                                      1800aaaf0
//   4. release filename (+0x18)                              1800aaaf6..b1e
//   5. stamp ??_7Stream@IO@@6B@                              1800aab20
//   6. if (encoding) <guarded delete>                        1800aab2a..33
//
// STEP 2 READS THE SINGLETON, IT DOES NOT USE `this`. Retail loads
// `MOV RCX,[0x1801fc6a8]` even though the FileInfo it closes belongs to this
// object — the same shape filesystem_packages.cpp:387 records for LoadPackages.
// ============================================================================
extern "C" void RtIo_FileStream_Dtor(FileStream* self) {
    ++g_fsc.fsDtor;

    self->base.vptr = RT_DATA_SLOT(const void*, "??_7FileStream@IO@@6B@");

    if (self->file.handle != -1)                      // 1800aaad5
        RtIo_FileSystem_CloseFile(Fs(), &self->file); // 1800aaae6

    RtIo_FileInfo_Dtor(&self->file);                  // 1800aaaf0
    ReleaseStrSlot(&self->filename);                  // 1800aaaf6

    self->base.vptr = RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");
    if (self->base.encoding) EncodingDelete(self->base.encoding);  // 1800aab33
}

// ============================================================================
// IO::BinaryFileStream::~BinaryFileStream                retail 0x180023c80
// ============================================================================
// FIVE INSTRUCTIONS: a frame, an SEH cookie, and `JMP 0x1800aaab0`. It is a
// pure tail-jump onto ~FileStream and adds NOTHING of its own — not even a
// vtable stamp, which is why ~FileStream's first store puts ??_7FileStream@
// there and no BinaryFileStream table is ever written on the way down.
//
// Compared instruction by instruction before writing this, because Wave 18
// records a lane that nearly shipped a leak by assuming an exported body
// matched an inlined one.
// ============================================================================
extern "C" void RtIo_BinaryFileStream_Dtor(BinaryFileStream* self) {
    ++g_fsc.binDtor;
    RtIo_FileStream_Dtor(&self->base);                // 180023c96 JMP
}

// ============================================================================
// IO::TextFileStream::~TextFileStream                    retail 0x1800ba490
//                                                  (WAVE 27 LANE IOTEXT)
// ============================================================================
// Four steps then a tail jump, transcribed from the RETAIL disassembly:
//   1. stamp ??_7TextFileStream@IO@@6B@ (0x180155980)        1800ba4a6/ad
//   2. if (buffer) MemFree(buffer)          — BARE, +0x70    1800ba4b0..b9
//   3. release objectType  (+0x60)                           1800ba4bf..e9
//   4. release objectName  (+0x58)                           1800ba4e9..513
//   5. JMP ~FileStream(this)                0x1800aaab0      1800ba51b
//
// WHY THIS ROW IS WORTH PORTING, and it is NOT the census. The census lists this
// symbol under INSTRUMENT-SOURCED -- its only *export-name* caller in the tree
// is runtime/src/io/file_stream_probe.cpp, and a probe's resolve is a dependency
// but not a port obligation. The REAL caller is not a name at all:
// runtime/src/object/deleting_dtor.cpp's row 41 ("IO::TextFileStream", slot 0 of
// `??_7TextFileStream@IO@@6B@`) reaches 0x1800ba490 BY ADDRESS through
// `ResolveAddr`, and that row's own comment says in as many words that this made
// the address "newly `called`". With this body absent that edge went to the DLL
// and, after cutover, to NULL -- so every `delete` of a TextFileStream faults.
// A census that buckets a row by its NAME callers cannot see an ADDRESS caller
// in another file, which is why the instrument tag is right about what it
// measured and wrong as a scoping signal here.
//
// THREE THINGS THIS DOES NOT DO, all of them retail's choices and all faithful:
//   * it does NOT null `buffer` after freeing it (no store follows 1800ba4b9);
//   * it does NOT stamp ??_7Stream@ or ??_7FileStream@ -- ~FileStream does both;
//   * step 2 is a BARE MemFree, not the refcounted String idiom. The buffer is
//     owned memory with no header, which is the independent second pin on
//     FileStream.h's `void* buffer; // +0x70  owned, bare MemFree`.
//
// The release ORDER is DESCENDING (+0x60 before +0x58), like ~PackageFile's five
// and ~ElementDescription's one. Irrelevant to the result and transcribed anyway.
//
// NO ORACLE, for the reason ~ElementDescription and ~Element state and for the
// same reason: a replay would decrement each refcount a second time and, where
// our body already took one to zero, free a block twice. `the diff oracle` records
// whether a call actually released something; `calls=` stays 0 because there is
// no comparison to report. Liveness, not correctness.
//
// ⚠ THE PROBE THAT *LOOKS* LIKE THIS ROW'S ORACLE IS NOT ONE. See the
// INSTRUMENT DEFECT section of docs/runtime_port/WAVE27_LANE_IOTEXT.md: two of
// three vptr checks in file_stream_probe.cpp build their subject with retail's
// constructor and compare against `RT_DATA_SLOT`, which is registry-first, so
// they compare retail with itself. Not repaired by this lane (Wave 27 §4 #1).
extern "C" void RtIo_TextFileStream_Dtor(RtIo::TextFileStream* self) {
    bool didWork = false;

    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7TextFileStream@IO@@6B@");   // 1800ba4a6

    if (self->buffer) {                                           // 1800ba4b4
        RtSystem::RtMemFree(self->buffer);                        // 1800ba4b9
        didWork = true;
        // No `self->buffer = nullptr;` -- retail leaves the dangling pointer.
    }

    if (self->objectType) { ReleaseStrSlot(&self->objectType); didWork = true; }  // 1800ba4bf
    if (self->objectName) { ReleaseStrSlot(&self->objectName); didWork = true; }  // 1800ba4e9

    RtIo_FileStream_Dtor(&self->base);                            // 1800ba51b JMP
}

// Two callees this file does not otherwise use, declared rather than included:
// both are PORTED and registered by address in their own translation units, and
// calling them directly (not through a door) is the choice the rest of this file
// already makes -- a door would resolve to the same body and add a resolve site
// the cutover census has to carry (reference_count_the_doors_not_the_calls.md).
extern "C" void RtIo_CompressedStream_Commit(RtIo::CompressedStream* self);
extern "C" void RtIo_CompressedStream_Dtor(RtIo::CompressedStream* self);

// ============================================================================
// IO::CompressedFileStream::~CompressedFileStream        retail 0x1800a9330
//                                                  (WAVE 27 LANE IOTEXT)
// ============================================================================
// Read with objdump against C:/GOG Games/Banished/Runtime-x64.dll — the retail
// file on disk, addressed by true VA — after the integrator measured TWO Ghidra
// defects on this wave's session (a program-selection defect serving the profile
// build, and `disassemble_function` relabelling one body's listing with another
// body's address). objdump shares no mechanism with Ghidra, and stream classes
// are the "near-identical sibling" shape the second defect corrupts invisibly.
//
// Five steps then a tail jump:
//   1800a934b  stamp ??_7CompressedFileStream@IO@@6B@ (0x1801555f0)
//   1800a9355  if (headerSize > 0 && direction == 1) Commit(this)
//   1800a9366  rdi = &this->file                      (+0x68)
//   1800a936e  if (file.file.handle != -1) CloseFile(_singleton, &file.file)
//   1800a9388  ~FileStream(&this->file)
//   1800a939b  JMP ~CompressedStream(this)
//
// STEP 2 IS THE FLUSH, AND ITS GUARD IS TWO CONDITIONS, NOT ONE.
// `CMPL $0,0x48(%rcx) ; JLE` is `headerSize > 0` (Stream.h names +0x48 the
// header Array's _size and calls it "the guard" already), and
// `CMPL $1,0x8(%rcx) ; JNE` is `direction == 1`, i.e. WRITING. So a read-only
// compressed stream, and a writing one that never produced a header, both skip
// Commit. Dropping either half would write a block on teardown of a stream that
// never wrote one.
//
// STEP 4 CLOSES THE INNER STREAM'S FileInfo, NOT ITS OWN. `LEA RDX,[RDI+0x20]`
// with RDI = this+0x68 puts the FileInfo at this+0x88 — FileStream's own
// `file` member inside the embedded BinaryFileStream. And like ~FileStream it
// reaches the FileSystem through the exported singleton rather than through
// `this`, which is the shape filesystem_packages.cpp:387 already records.
//
// EVERY CALLEE IS OURS — the row CLOSES rather than deepening. Checked by
// grepping for each RegisterRuntimeAddr, not assumed:
//   0x1800a9ee0  CompressedStream::Commit  PORTED  compressed_stream_write.cpp:783
//   0x1800accc0  FileSystem::CloseFile     PORTED  filesystem_createfile_win.cpp:636
//   0x1800aaab0  ~FileStream               PORTED  this file, :867
//   0x1800aa650  ~CompressedStream         PORTED  compressed_stream_release.cpp:196
//
// WHY THIS ROW, given the census tags it INSTRUMENT-SOURCED: the same reason as
// ~TextFileStream above. Its only export-NAME caller is file_stream_probe.cpp,
// but deleting_dtor.cpp's row 40 ("IO::CompressedFileStream", slot 0 of
// `??_7CompressedFileStream@IO@@6B@`) reaches 0x1800a9330 BY ADDRESS, and a
// census that buckets a row by its name callers cannot see that.
//
// NO ORACLE, same reason as the two destructors above: a replay would Commit
// twice, close a closed handle and free the window arrays twice.
extern "C" void RtIo_CompressedFileStream_Dtor(RtIo::CompressedFileStream* self) {
    bool didWork = false;

    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7CompressedFileStream@IO@@6B@");  // 1800a934b

    if (self->base.headerSize > 0 &&                                   // 1800a9355
        self->base.base.direction == 1) {                               // 1800a935b
        RtIo_CompressedStream_Commit(&self->base);                      // 1800a9361
        didWork = true;
    }

    FileStream* const inner = &self->file.base;                         // 1800a9366
    if (inner->file.handle != -1) {                                     // 1800a936e
        RtIo_FileSystem_CloseFile(Fs(), &inner->file);                  // 1800a937a
        didWork = true;
    }

    RtIo_FileStream_Dtor(inner);                                        // 1800a9388
    RtIo_CompressedStream_Dtor(&self->base);                            // 1800a939b JMP
}

// ============================================================================
// IO::BinaryFileStream::BinaryFileStream                 retail 0x1800a8200
// ============================================================================
// The inlined FileStream prologue, then the class's own vtable, then a fresh
// Encoding carrying the SHARED table at 0x1801554a8 (unexported — by address).
// ============================================================================
extern "C" BinaryFileStream* RtIo_BinaryFileStream_Ctor(BinaryFileStream* self) {
    ++g_fsc.binCtor;

    FileStreamCtorInline(&self->base);                                   // 1800a8219..70
    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7BinaryFileStream@IO@@6B@");        // 1800a8270

    InstallEncoding<IO::BinaryEncoding>(&self->base.base);                    // 1800a827a..d8

    return self;                                                          // 1800a82dc
}

// ============================================================================
// IO::TextFileStream::TextFileStream(String const&, String const&, int)
//                                                         retail 0x1800ba340
// ============================================================================
// The same inlined FileStream prologue, its own vtable, seven of its own
// fields, a TEXT Encoding (0x1801559d0 — a DIFFERENT table from the binary
// one, and also unexported), and then a tail call to `?SetObject@` with the
// three arguments verbatim.
//
// TWO THINGS THIS BODY DOES *NOT* DO, both recorded because they are the
// difference from the default constructor and from a reasonable guess:
//
//   * IT NEVER WRITES +0x50. The DEFAULT ctor (0x1800ba530) writes `+0x50 = 0`;
//     this one leaves it to SetObject's third argument. So +0x50 is the
//     VERSION, not a flag — the two constructors write the same slot from two
//     places. FileStream.h states this; the disassembly confirms it.
//   * IT NEVER WRITES +0x54. Neither constructor does.
//
// `_requestedFormat` (+0x68) and `_activeEncoding` (+0x6c) are BOTH 6 here and
// are indistinguishable in this body alone. They are distinguished by
// `?SaveText@External@Object@@`, which writes both to 2 and then branches on
// +0x6c ALONE for the byte-order mark. Not re-derived — FileStream.h's note.
// ============================================================================
extern "C" TextFileStream* RtIo_TextFileStream_Ctor(TextFileStream* self,
                                                    const String* type,
                                                    const String* name,
                                                    int32_t version) {
    ++g_fsc.textCtor;

    FileStreamCtorInline(&self->base);                                   // 1800ba36f..c6
    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7TextFileStream@IO@@6B@");          // 1800ba3c6

    self->objectName      = nullptr;   // 1800ba3d0  +0x58
    self->objectType      = nullptr;   // 1800ba3d4  +0x60
    self->requestedFormat = 6;         // 1800ba3d8  +0x68
    self->activeEncoding  = 6;         // 1800ba3df  +0x6c
    self->buffer          = nullptr;   // 1800ba3e6  +0x70
    self->_unk78          = nullptr;   // 1800ba3ea  +0x78
    // +0x50 and +0x54 deliberately untouched — see the header note.

    InstallEncoding<IO::TextEncoding>(&self->base.base);                    // 1800ba3ee..4e

    RT_EXPORT_ONCE(SetObjectFn,
        "?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z")(
            self, type, name, version);                                   // 1800ba45e

    return self;                                                          // 1800ba464
}

// ============================================================================
// IO::TextFileStream::SetObject(String const&, String const&, int)  0x1800ba280
// ============================================================================
// WAVE 23 lane IOPORT.  Ordinal 3930.  42 instructions.
//
// THE ARGUMENT ORDER IS THE WHOLE RISK IN THIS ROW, so it was derived from the
// REGISTERS, not from the decompiler's parameter names and not from the callers:
//
//   1800ba29e  MOV RDI,RDX   arg1 ... 1800ba313  MOV [RBX+0x60],RAX  objectType
//   1800ba29b  MOV RSI,R8    arg2 ... 1800ba2d2  MOV [RBX+0x58],RAX  objectName
//   1800ba298  MOV EBP,R9D   arg3 ... 1800ba2de  MOV [RBX+0x50],EBP  objectVersion
//
// So SetObject(a, b, v) assigns objectTYPE = a and objectNAME = b: the FIRST
// String is the type and the SECOND is the name.  That is the reverse of the
// reading order the field layout suggests (+0x58 name comes before +0x60 type),
// and this exact pair has been swapped in this project before —
// runtime/types/io/FileStream.h records having carried them the wrong way round
// until retail settled it.  Three independent confirmations of the orientation:
//   1. the register trace quoted above (this lane, retail Runtime-x64.dll);
//   2. RtIo_TextFileStream_Ctor immediately above already passes
//      `(self, type, name, version)` in that order to reach 1800ba45e;
//   3. ?GetObjectName@ returns +0x58 and ?GetObjectType@ returns +0x60
//      (FileStream.h's static_asserts quote both LEAs).
//
// THE BODY, in retail's order — the version store sits BETWEEN the two string
// assignments, not before or after both:
//     objectName (+0x58) <- arg2      [release-then-addref, guarded]
//     objectVersion (+0x50) <- arg3
//     objectType (+0x60) <- arg1      [release-then-addref, guarded]
//
// THE SELF-ASSIGNMENT GUARD IS LOAD-BEARING AND IS REPRODUCED.  `CMP RAX,[R8] /
// JZ` at 1800ba2a4 skips the ENTIRE release-and-addref when the incoming String
// already holds the same char*.  Dropping it would release and re-add a
// reference in the same breath: a no-op for a live string, but it frees the
// buffer outright when the field holds the last reference, which is a
// use-after-free the moment a caller passes a field back to itself.
//
// THE STRING IDIOM is System::String's one-pointer representation with the
// refcount at [p-4] and the allocation base at p-8 — the shape avl_trees.cpp's
// StringReleaseInline and a dozen other ported bodies already use.  It is
// written out inline here rather than through that helper because retail's
// spelling differs in one visible way: the assignment stores the new pointer
// DIRECTLY (1800ba2cf `MOV RAX,[RSI] / MOV [RBX+0x58],RAX`) with no preceding
// `slot = 0`, whereas StringAddRefInto stores the zero first.  Equivalent in
// effect, NOT equivalent in bytes; this file follows the bytes.
//
// MemFree is 1800c7ad0 reached through RtSystem::RtMemFree, the port-aware door
// lane E's allocator replaces when it arms — the door every other free here uses.
static void TfsReleaseAssign(char** slot, char* const* incoming) {
    char* cur = *slot;
    if (cur == *incoming) return;                          // 1800ba2a4 / 1800ba2e5
    if (cur != nullptr) {
        if (--reinterpret_cast<int32_t*>(cur)[-1] == 0) {  // 1800ba2ae
            *slot = cur - 8;                               // 1800ba2b7
            RtSystem::RtMemFree(*slot);                    // 1800ba2c0
        } else {
            *slot = nullptr;                               // 1800ba2c7
        }
    }
    char* nw = *incoming;                                  // 1800ba2cf
    *slot = nw;
    if (nw != nullptr) ++reinterpret_cast<int32_t*>(nw)[-1];  // 1800ba2db
}

// ---------------------------------------------------------------------------
// ORACLE: A/B ON SYNTHETIC SUBJECTS, and why it is NOT a replay is the point.
// ---------------------------------------------------------------------------
// SetObject cannot be replayed over the live object.  Running theirs AFTER ours
// is VACUOUS: ours has already stored the incoming pointers, so theirs takes
// the self-assignment guard on both strings and does nothing, and the
// comparison passes no matter what this body did.  That is the
// "a predicate nobody evaluates" shape — permanently green, permanently blind.
//
// Running theirs on a scratch object whose fields ALIAS the real object's
// strings is worse than vacuous: both bodies decrement the same refcount, which
// double-releases a live string.
//
// So the subject is SYNTHETIC.  Each armed call builds independent
// TextFileStream-shaped buffers and freshly allocated refcounted strings, runs
// the DLL's body on one and this port's on the other with identical arguments,
// and compares.  Nothing live is touched and nothing is aliased.
//
// WHAT IS COMPARED: all three written fields (+0x50, +0x58, +0x60) on both
// subjects AND the refcount of every string involved.  The refcounts are the
// half a field comparison alone would miss, and they are exactly where a
// dropped addref or a doubled release shows up.
//
// WHICH BRANCHES IT COVERS — three cases per armed call:
//   (a) old == nullptr            fresh assign, no release
//   (b) old != new, refcount > 1  release-without-free      (1800ba2c7)
//   (c) old == new                the self-assignment guard (1800ba2a7)
// Case (d), release-WITH-free (refcount reaches 0, 1800ba2b7), is NOT
// exercised: driving it means handing the DLL a string it will MemFree, and the
// two subjects would then need separately-allocated-but-equal strings, at which
// point the compared pointers legitimately differ and the comparison stops
// meaning anything.  That branch is covered by STATIC BYTE READING ONLY and is
// listed as such in docs/runtime_port/WAVE23_LANE_IOPORT.md rather than being
// quietly folded into a green result.
//
// NEGATIVE CONTROL: the old DLL-era switch makes the port's objectType
// store write nullptr.  +0x60 IS in the compared set, so the control corrupts
// something the oracle actually reads — the property a control needs.  A
// control that corrupts an unobserved value reports NOT CAUGHT and indicts a
// sound instrument; the mirror (corrupting a value that SHOULD differ)
// exonerates a broken one.  Neither applies here: +0x60 must be EQUAL on both
// subjects, because both are handed the same incoming pointer.
extern "C" void RtIo_TextFileStream_SetObject(TextFileStream* self,
                                              char* const* type,
                                              char* const* name,
                                              int version) {

    TfsReleaseAssign(&self->objectName, name);   // 1800ba294..ba2dd   arg2
    self->objectVersion = version;               // 1800ba2de          arg3
    TfsReleaseAssign(&self->objectType, type);   // 1800ba2e1..ba31e   arg1

}

// ============================================================================
// IO::FileStream::Create(Filename const&)                retail 0x1800aa8f0
// ============================================================================
// PROTECTED (`IEAA`), which means PRIVATE-to-the-hierarchy, NOT unexported —
// this DLL exports its private members and ord 1876 is right there
// (`reference_aeaa_means_private_not_unexported.md`).
//
// Five steps:
//   1. FileSystem::CreatePath(_singleton, path)              1800aa919
//   2. this->filename = path  (release/adopt, self-assign short-circuited)
//                                                            1800aa91e..58
//   3. this->direction = 1  (writing)                        1800aa959
//   4. FileInfo tmp = FileSystem::CreateFileW(_singleton, path);
//      this->file = tmp;  ~tmp;                              1800aa960..9ef
//   5. return this->file.handle != -1                        1800aa9f0
//
// STEP 2's SELF-ASSIGNMENT SHORT-CIRCUIT JUMPS PAST BOTH HALVES.
// `CMP RAX,[RSI] / JZ 0x1800aa959` skips the release AND the adopt, not just
// the release — the same detail filesystem_openfile_win.cpp records for
// FileInfo::operator=, and getting it wrong drops a shared buffer's count to
// zero and frees a string both sides still point at.
//
// STEP 4's TEMPORARY IS DESTROYED IN RETAIL'S ORDER: _name1 (+0x28, at
// rsp+0x50) before _name0 (+0x20, at rsp+0x48). That is the inlined ~FileInfo,
// and it is the same order the standalone one at 0x18000c180 uses.
// ============================================================================
extern "C" bool RtIo_FileStream_Create(FileStream* self, const Filename* path) {
    ++g_fsc.create;

    RtIo_FileSystem_CreatePath(Fs(), path);                     // 1800aa919

    // ---- 1800aa91e..1800aa958: this->filename = *path -------------------
    // ENDGAME B.3.1 step 2: this whole block IS `System::String::operator=`
    // (0x1800c9810) — the buffer-pointer guard that skips BOTH halves, the
    // release with its asymmetry, the unconditional store and the guarded
    // increment. `FileStream::filename` is declared `char*` in FileStream.h,
    // which is exactly the one pointer a String is, so it is reached as one.
    // NOTE there is NO AdjustDelimiters here: retail's Create does not
    // normalise, so this is String's operator=, not Filename's.
    reinterpret_cast<System::String&>(self->filename) = Sys(*path).AsString();

    self->base.direction = 1;                                   // 1800aa959

    // ---- 1800aa960..1800aa9ef -------------------------------------------
    // NOT zero-initialised: CreateFileW writes every field of it that anything
    // reads, and _unk04 is one neither side ever writes.
    FileInfo tmp;
    RtIo_FileSystem_CreateFileW(Fs(), &tmp, path);              // 1800aa96f
    RtIo_FileInfo_Assign(&self->file, &tmp);                    // 1800aa97c
    SysFile(tmp._name1).~Filename();                            // 1800aa982  +0x28 first
    SysFile(tmp._name0).~Filename();                            // 1800aa9b9  +0x20

    const bool ok = (self->file.handle != -1);                  // 1800aa9f0
    if (ok) ++g_fsc.createTrue;
    return ok;
}

// Read by file_stream_cutover_probe.cpp, so the attendance figures print beside
// the door verdicts and a reader cannot see one without the other.
void RtIoFileStreamCounters(unsigned long long* out) {
    out[0]=g_fsc.binCtor;  out[1]=g_fsc.binDtor;   out[2]=g_fsc.fsDtor;
    out[3]=g_fsc.textCtor; out[4]=g_fsc.create;    out[5]=g_fsc.createTrue;
    out[6]=g_fsc.fileInfoDtor;
}

}  // namespace RtIO

// ============================================================================
// Registration — BY NAME *AND* BY ADDRESS for every row.
//
// `ResolveExport` falls through silently to the shipped body for an unported
// symbol, and `ResolveAddr(Module::Runtime, …)` is registry-first; a caller
// that reaches a body by ADDRESS is invisible to a name-only registration and
// dies at cutover (`reference_count_the_doors_not_the_calls.md`).
// ============================================================================
int RtIoFileStreamRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("??0BinaryFileStream@IO@@QEAA@XZ",
                          (void*)&RtIo_BinaryFileStream_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a8200, (void*)&RtIo_BinaryFileStream_Ctor);
    RegisterRuntimeExport("??0FileStream@IO@@QEAA@XZ",
                          (void*)&RtIo_FileStream_Ctor); ++n;
    RegisterRuntimeAddr(0x1800aaa10, (void*)&RtIo_FileStream_Ctor);

    RegisterRuntimeExport("??1BinaryFileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_BinaryFileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x180023c80, (void*)&RtIo_BinaryFileStream_Dtor);

    RegisterRuntimeExport("??1FileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_FileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800aaab0, (void*)&RtIo_FileStream_Dtor);

    // WAVE 27 lane IOTEXT. `UEAA`, the VIRTUAL spelling -- `??1TextFileStream@
    // IO@@QEAA@XZ` is a phantom that resolves NULL for ever
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). The ADDRESS door is the
    // one that matters: deleting_dtor.cpp's row 41 asks for 0x1800ba490 by
    // address and by nothing else.
    RegisterRuntimeExport("??1TextFileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_TextFileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800ba490, (void*)&RtIo_TextFileStream_Dtor);

    // WAVE 27 lane IOTEXT. `UEAA` again; deleting_dtor.cpp row 40 asks for
    // 0x1800a9330 by ADDRESS and that is the door that matters.
    RegisterRuntimeExport("??1CompressedFileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_CompressedFileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800a9330, (void*)&RtIo_CompressedFileStream_Dtor);

    RegisterRuntimeExport("??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z",
                          (void*)&RtIo_TextFileStream_Ctor); ++n;
    RegisterRuntimeAddr(0x1800ba340, (void*)&RtIo_TextFileStream_Ctor);

    RegisterRuntimeExport("?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileStream_Create); ++n;
    RegisterRuntimeAddr(0x1800aa8f0, (void*)&RtIo_FileStream_Create);

    // ---- Wave 23 lane IOPORT ----
    // RtIo_TextFileStream_Ctor above already CALLS this symbol through
    // RT_EXPORT_ONCE, so registering it converts that existing call site from
    // the DLL's body to ours with no edit at the call site.
    RegisterRuntimeExport("?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z",
                          (void*)&RtIo_TextFileStream_SetObject); ++n;
    RegisterRuntimeAddr(0x1800ba280, (void*)&RtIo_TextFileStream_SetObject);

    // UNEXPORTED — address only, and that is the ONLY door it has.
    RegisterRuntimeAddr(0x18000c180, (void*)&RtIo_FileInfo_Dtor);

    printf("[RtPort]   IO::FileStream family (wave 19 lane FILE): "
           "%d exports + 1 unexported address registered\n", n);
    return n;
}

// ============================================================================
// THE VTABLE SLOT BODIES — Wave 19 lane FILE, POST-REPORT TRANCHE (see §6.16:
// sealing forbids altering, not adding; this is new work, clearly marked).
// ============================================================================
//
//   | Export                                             | Retail      | Ord  | Slot |
//   |----------------------------------------------------|-------------|------|------|
//   | ?GetSource@FileStream@IO@@UEBA?AVString@System@@XZ | 0x1800aab50 | 2920 |  1   |
//   | ?Read@FileStream@IO@@UEAA_NHPEAX@Z                 | 0x1800aabf0 | 3450 |  3   |
//   | ?GetCurrentPos@FileStream@IO@@UEBAIXZ              | 0x1800aac50 | 2534 |  5   |
//   | ?GetSize@FileStream@IO@@UEBAIXZ                    | 0x1800aac60 | 2904 |  6   |
//   | ?Open@FileStream@IO@@UEAA_NAEBVFilename@System@@@Z | 0x1800aac70 | 3304 |  7   |
//
// plus the two UNEXPORTED deleting-destructor thunks at slot 0 —
// `0x18000c090` (FileStream's) and `0x180023d80` (BinaryFileStream's).
// `?Write@` (slot 2) and `?SetCurrentPos@` (slot 4) are in file_stream_win.cpp
// because they call Win32 directly; the split is the platform seam, not a
// division of the row.
//
//
// ---------------------------------------------------------------------------
// WHY NINE BODIES CLEAR TWO WHOLE DATA ROWS
// ---------------------------------------------------------------------------
// `??_7FileStream@IO@@6B@` (0x180154960, 8 slots) and
// `??_7BinaryFileStream@IO@@6B@` (0x1801554d0, 8 slots) were both minted by
// Wave 19 phase 0b at 0 ported / 8 TRAP. **Their slots 1..7 are byte-identical**
// — re-derived here by reading both tables' bytes rather than inherited:
//
//   0x1801554d0: 180023d80 1800aab50 1800aab90 1800aabf0
//                1800aac10 1800aac50 1800aac60 1800aac70
//   0x180154960: 18000c090 <the same seven>
//
// So the two rows differ ONLY in slot 0, and 7 shared bodies + 2 thunks = 9
// functions cover 16 trap slots. That is the "scope a DATA row by FUNCTION, not
// by slot" shape, and it was found independently by two Wave 19 cells before it
// was found here.
//
// ---------------------------------------------------------------------------
// WHAT CLEARING THEM DOES AND DOES NOT BUY — the STAMPER criterion
// ---------------------------------------------------------------------------
// A minted vtable row is promotable exactly insofar as WE OWN THE FUNCTION THAT
// STAMPS IT: every stamp of these tables is an in-DLL RIP-relative LEA
// (`1800a8270 LEA RAX,[0x1801554d0] -> MOV [RDI],RAX`), and a LEA is not a door
// (`reference_a_ported_row_can_be_permanently_unpromotable.md`).
//
//   * `??_7BinaryFileStream@` has ONE stamper — `??0BinaryFileStream@`, which
//     this lane already ported. Clearing these traps makes that row promotable.
//   * `??_7FileStream@` has NINE distinct stampers, including `Engine::Run`.
//     **TRAP=0 there is NOT a promotion** and must not be read as one; most
//     stamps stay on retail's side whatever we do to the slots.
//
// The consequence for these bodies is that they must be byte-faithful for BOTH
// tables: an object WE construct gets our minted table and our slot bodies, an
// object retail constructs gets retail's, and the two have to behave alike.
//
// ---------------------------------------------------------------------------
// THE ARRAY ARM OF BOTH THUNKS IS A NAMED ABORT — a declared divergence
// ---------------------------------------------------------------------------
// Both thunks are MSVC's standard scalar/vector deleting destructor, and the
// `flags & 2` (delete[]) arm calls the CRT's `eh_vector_destructor_iterator` at
// `0x18007ec98`, which is unexported and unported. `binary_writer.cpp` faced
// exactly this and made the arm a NAMED ABORT rather than open an address door
// onto a CRT helper for a path no caller takes; this file takes the same
// disposition for consistency, and states its limit:
//
//   * nothing in this tree array-allocates a FileStream or BinaryFileStream —
//     every one is stack-constructed by its caller (`external_load.cpp`,
//     `external_savetext_win.cpp`, `save_image_to_file.cpp`) or embedded in a
//     `CompressedFileStream`;
//   * **but unlike BinaryWriter, whose only producer is a `MemAlloc(0x30,4)`
//     with no array cookie, this lane did NOT prove no such producer can
//     exist.** The claim here is "no caller in either binary that we have
//     found", which is weaker, and the abort message says so.
//
// This is the one place in this tranche that is not byte-faithful. It is
// deliberate, it is declared, and if it ever fires the message names what would
// have had to happen.
// ============================================================================

namespace RtIO {

// The two Win32 slot bodies, and their attendance counters, live next door.
extern uint64_t g_fsWrite, g_fsWriteFull, g_fsSeek, g_fsSeekPlatform;

// Slot-body attendance. Same reasoning as the lifecycle block above: a static
// door screen cannot tell a registered body from an entered one.
struct SlotCounters {
    uint64_t getSource, getSourceNonNull;
    uint64_t read, getPos, getSize;
    uint64_t open, openTrue;
    uint64_t dtorScalar, dtorArrayAbort;
    uint64_t copyCtor;
};
SlotCounters g_fss{};

// ?GetSource@FileStream@IO@@UEBA?AVString@System@@XZ (0x1800aab50) — slot 1.
extern "C" RtSystem::String* RtIo_FileStream_GetSource(const FileStream* self,
                                                       RtSystem::String* ret) {
    // Construct retail's by-value result directly INTO the caller's raw slot:
    // `ret` is uninitialised memory, so this is a placement construction, not
    // an assignment over a live String (which would release a string the caller
    // never owned).
    new (ret) System::String(
        reinterpret_cast<const IO::FileStream*>(self)->IO::FileStream::GetSource());
    return ret;
}

// ?Read@FileStream@IO@@UEAA_NHPEAX@Z (0x1800aabf0) — slot 3.
extern "C" bool RtIo_FileStream_Read(FileStream* self, int32_t len, void* buf) {
    return reinterpret_cast<IO::FileStream*>(self)->IO::FileStream::Read(len, buf);
}

// ?GetCurrentPos@FileStream@IO@@UEBAIXZ (0x1800aac50) — slot 5.
// ?GetSize@FileStream@IO@@UEBAIXZ       (0x1800aac60) — slot 6.
extern "C" uint32_t RtIo_FileStream_GetCurrentPos(const FileStream* self) {
    return reinterpret_cast<const IO::FileStream*>(self)
               ->IO::FileStream::GetCurrentPos();
}

extern "C" uint32_t RtIo_FileStream_GetSize(const FileStream* self) {
    return reinterpret_cast<const IO::FileStream*>(self)->IO::FileStream::GetSize();
}

// ?Open@FileStream@IO@@UEAA_NAEBVFilename@System@@@Z (0x1800aac70) — slot 7,
// and slot 7 on THIS branch is Open, not Close.
extern "C" bool RtIo_FileStream_Open(FileStream* self, const Filename* path) {
    return reinterpret_cast<IO::FileStream*>(self)->IO::FileStream::Open(Sys(*path));
}

// ============================================================================
// The two slot-0 deleting-destructor thunks
//   FileStream        retail 0x18000c090   (unexported)
//   BinaryFileStream  retail 0x180023d80   (unexported)
// ============================================================================
// Both are MSVC's standard scalar/vector deleting destructor and both are five
// statements. They differ ONLY in which destructor they name — 0x1800aaab0
// (`~FileStream`) for the first, 0x180023c80 (`~BinaryFileStream`) for the
// second — which is the whole reason the two vtable rows are distinct.
//
// `0x180006a10`, the unexported guarded free both arms call, is registered
// (context_lifecycle.cpp) and byte-identical to the exported
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` — binary_writer.cpp compared
// them instruction for instruction and this lane inherits that rather than
// re-deriving it.
//
// The `flags & 2` arm is a NAMED ABORT. See the header note for why, and for
// the exact strength of the claim behind it.
// ============================================================================
namespace {

void* DeletingDtorCommon(void* self, unsigned flags, void (*dtor)(void*),
                         const char* which) {
    if (flags & 2) {                                      // 18000c09f / 180023da2
        ++g_fss.dtorArrayAbort;
        printf("[RtPort] FATAL: %s deleting destructor reached its "
                    "delete[] arm. Retail would call the CRT vector-destructor "
                    "iterator at 0x18007ec98, which is unexported and unported, "
                    "so this arm is a declared divergence (see the header note). "
                    "Reaching it means something ARRAY-ALLOCATED a FileStream, "
                    "which no caller found in either binary does.\n", which);
        std::abort();
    }
    dtor(self);                                            // 18000c0e1 / 180023dd1
    if (flags & 1) EncodingDelete(self);                   // 18000c0ef / 180023ddf
    return self;                                           // 18000c0f4 / 180023de4
}

}  // namespace

extern "C" void* RtIo_FileStream_DeletingDtor(void* self, unsigned flags) {
    ++g_fss.dtorScalar;
    return DeletingDtorCommon(self, flags,
        [](void* p) { RtIo_FileStream_Dtor(static_cast<FileStream*>(p)); },
        "IO::FileStream");
}

extern "C" void* RtIo_BinaryFileStream_DeletingDtor(void* self, unsigned flags) {
    ++g_fss.dtorScalar;
    return DeletingDtorCommon(self, flags,
        [](void* p) { RtIo_BinaryFileStream_Dtor(static_cast<BinaryFileStream*>(p)); },
        "IO::BinaryFileStream");
}

// ============================================================================
// IO::BinaryFileStream::BinaryFileStream(BinaryFileStream const&)
//                                                         retail 0x180023ca0
// ============================================================================
// THE COPY CONSTRUCTOR, AND IT IS HERE FOR ONE REASON: IT IS THE SECOND STAMPER
// OF `??_7BinaryFileStream@IO@@6B@`.
//
// The Wave 19 brief stated that `??0BinaryFileStream@IO@@QEAA@XZ` is that
// table's ONLY stamper, and that porting it would therefore make the row
// promotable. **That is not what the binary says.** `get_xrefs_to 0x1801554d0`
// returns exactly two code sites: 0x1800a8270 (the default ctor, ported above)
// and **0x180023cfe, inside this function**. Acting on the brief's claim would
// have declared the row fully owned while a second stamper sat in the DLL
// putting retail's table into every copied object.
//
// The same query also refutes the brief's "nine distinct stampers" for
// `??_7FileStream@` — there are about seventeen, five of which are already
// ours. The conclusion the brief drew from that number still holds (Engine::Run
// and three unexported bodies are not ours, so TRAP=0 there is not a
// promotion); the number itself does not. `get_xrefs_to` is cheap and neither
// figure had been run.
//
// ---------------------------------------------------------------------------
// IT COPIES THE ENCODING AS A RAW POINTER, AND THAT IS NOT A BUG TO FIX
// ---------------------------------------------------------------------------
// 0x180023cc6 is `MOV RAX,[RDX+0x10] ; MOV [RCX+0x10],RAX` — a shallow copy of
// `Stream::encoding`. Both objects then point at ONE heap Encoding, and both
// destructors free it: `~FileStream` ends in `if (encoding) <guarded free>`.
// **A copy of a BinaryFileStream is a double-free waiting for the second
// destructor**, which is a plausible reason nothing in either binary calls
// this.
//
// Reproduced exactly. Deep-copying the Encoding here would be a divergence that
// changes what retail does on a path retail may rely on never taking, and this
// project's rule is that an unguarded hazard in a faithful body is retail's
// statement, not ours to correct (CLAUDE.md, "Don't 'fix' a faithful crash").
// The hazard is recorded here instead.
//
// The three vtable stores are Stream -> FileStream -> BinaryFileStream, in that
// order, exactly as in the default constructor: MSVC emits one per sub-object
// as each base finishes.
// ============================================================================
extern "C" BinaryFileStream* RtIo_BinaryFileStream_CopyCtor(
        BinaryFileStream* self, const BinaryFileStream* src) {
    ++g_fss.copyCtor;

    self->base.base.vptr = RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");   // 180023cb6
    self->base.base.direction = src->base.base.direction;                     // 180023cc0
    self->base.base.encoding  = src->base.base.encoding;                      // 180023cc6 SHALLOW
    self->base.base.vptr = RT_DATA_SLOT(const void*, "??_7FileStream@IO@@6B@"); // 180023cce

    self->base.filename = nullptr;                                            // 180023cd8
    if (src->base.filename) {                                                 // 180023ce4
        self->base.filename = src->base.filename;                             // 180023ce9
        ++StringRefCount(self->base.filename);                                // 180023ced
    }

    RtIo_FileInfo_CopyCtor(&self->base.file, &src->base.file);                // 180023cf8

    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7BinaryFileStream@IO@@6B@");            // 180023cfe
    return self;                                                              // 180023d08
}

// Published to the cutover probe alongside the lifecycle counters.
void RtIoFileStreamSlotCounters(unsigned long long* out) {
    out[0]=g_fss.getSource; out[1]=g_fss.getSourceNonNull;
    out[2]=g_fsWrite;       out[3]=g_fsWriteFull;
    out[4]=g_fss.read;      out[5]=g_fsSeek;
    out[6]=g_fsSeekPlatform;out[7]=g_fss.getPos;
    out[8]=g_fss.getSize;   out[9]=g_fss.open;
    out[10]=g_fss.openTrue; out[11]=g_fss.dtorScalar;
    out[12]=g_fss.dtorArrayAbort; out[13]=g_fss.copyCtor;
}

}  // namespace RtIO

// ============================================================================
// `IO::FileStream`'s member bodies — ENDGAME B.3.4 block 4 (STREAMS)
// ============================================================================
// A member of `IO::X` cannot be defined inside `namespace RtIO`: `IO` and
// `RtIO` are siblings and C++ requires the definition to appear in an
// ENCLOSING namespace (B34_BRIEFS section A.2). So the five bodies this file
// owns open their own block here, and reach this file's helpers and counters by
// their `RtIO::` qualification. The two Win32 slots (`Write`, `SetCurrentPos`)
// are in file_stream_win.cpp, next to their own `<windows.h>`.
//
// The attendance counters are BUMPED IN THE MEMBER, not in the door, so they
// keep counting entries to the BODY rather than entries to the door — which is
// what they have always measured and what makes their numbers comparable
// across this change.
namespace IO {

// ============================================================================
// IO::FileStream::GetSource() const                      retail 0x1800aab50
// ============================================================================
// IT IS AN ADD-REF, NOT A COPY. The caller receives the same buffer and owes it
// a release. A port that duplicated the characters would be observably wrong to
// anyone comparing pointers, and would leak.
//
// Retail returns the `System::String` by value through a hidden slot in RDX;
// here that is the compiler's business again and the member simply returns one.
// The `[RSP]` slot retail writes 0 then 1 into is the SEH unwind state for the
// temporary, not a field of anything.
System::String FileStream::GetSource() const {
    ++RtIO::g_fss.getSource;

    System::String ret;                                   // 1800aab67: *ret = nullptr
    if (_filename) {                                      // 1800aab6e
        // The ADD-REF, spelled exactly as retail spells it and NOT through
        // `String`'s copy constructor: two stores, the pointer and the
        // refcount in front of it. `_data` is public during the migration
        // (String.h) for exactly this kind of transcription.
        ret._data = _filename;                                 // 1800aab73
        ++RtSystem::StringRefCount(ret._data);                 // 1800aab76
        ++RtIO::g_fss.getSourceNonNull;
    }
    return ret;                                           // 1800aab80
}

// ============================================================================
// IO::FileStream::Read(int, void*)                       retail 0x1800aabf0
// ============================================================================
// SIX INSTRUCTIONS AND A TAIL-JUMP. The whole body is
//
//     return FileSystem::ReadFile(_singleton, &this->file, len, buf);
//
// and the argument shuffle is the only content: `len` arrives in EDX and has to
// reach R8D, `buf` arrives in R8 and has to reach R9, because the callee takes
// `this` in RCX and the FileInfo in RDX.
//
// **READS GO THROUGH FileSystem, WRITES DO NOT.** `?Write@FileStream@` calls
// `WriteFile` directly (file_stream_win.cpp). That asymmetry is retail's, and
// it is why only one of the two needed a `_win` file: reading a packed file has
// to consult the package index, and `FileSystem::ReadFile` is where that lives.
//
// `?ReadFile@FileSystem@IO@@` is already ported and registered (Wave 9 lane
// PKG2 rung 3), so it is called DIRECTLY here rather than through a door.
bool FileStream::Read(int len, void* buf) {
    ++RtIO::g_fss.read;
    const bool ok = RtIo_FileSystem_ReadFile(RtIO::Fs(), &_file, len, buf);  // 1800aac03
    return ok;
}

// ============================================================================
// IO::FileStream::GetCurrentPos() const                  retail 0x1800aac50
// IO::FileStream::GetSize() const                        retail 0x1800aac60
// ============================================================================
// One instruction each: `MOV EAX,[RCX+0x2c]` and `MOV EAX,[RCX+0x28]`.
//
// BOTH RETURN **UNSIGNED** (`UEBAIXZ`), while `FileInfo::cursor` and `::size`
// are modelled `int32_t`. The bits are identical; the return TYPE is what the
// caller's comparison sees, and a signed reading flips every test above 2GB.
// The signature comes from the export directory, which is the only place it is
// visible — a one-instruction body cannot show a return type.
//
// These two are also what `FileStream.h` used to PIN `FileInfo` at +0x20 (its
// route 2), so they are the cheapest possible cross-check on that claim: if the
// offset were wrong these would return the wrong field on every call, on a path
// the package loader takes thousands of times per run.
uint32_t FileStream::GetCurrentPos() const {
    ++RtIO::g_fss.getPos;
    return static_cast<uint32_t>(_file.cursor);            // 1800aac50
}

uint32_t FileStream::GetSize() const {
    ++RtIO::g_fss.getSize;
    return static_cast<uint32_t>(_file.size);              // 1800aac60
}

// ============================================================================
// IO::FileStream::Open(Filename const&)                  retail 0x1800aac70
// ============================================================================
// THE READ TWIN OF `?Create@FileStream@`, and comparing the two is the clearest
// statement of what each does. Same five steps, three differences:
//
//   | step                | Create (0x1800aa8f0)   | Open (0x1800aac70)      |
//   |---------------------|------------------------|-------------------------|
//   | 1. prepare          | CreatePath(_singleton) | *(nothing)*             |
//   | 2. filename         | release/adopt          | release/adopt, IDENTICAL|
//   | 3. direction (+0x8) | **1** (writing)        | **0** (reading)         |
//   | 4. get the FileInfo | FileSystem::CreateFileW| FileSystem::**OpenFile**|
//   | 5. return           | file.handle != -1      | file.handle != -1       |
//
// Step 2's SELF-ASSIGNMENT SHORT-CIRCUIT JUMPS PAST BOTH HALVES here too
// (`CMP RAX,[RDX] ; JZ 0x1800aaccd` skips the release AND the adopt), and step
// 4's temporary is destroyed in the same order — `_name1` (rsp+0x50) before
// `_name0` (rsp+0x48).
//
// **Open does NOT call CreatePath.** Creating directories on the way to a file
// you are about to READ would be wrong, and retail does not do it. Worth
// stating because the two bodies are otherwise near-identical and a port
// written by analogy from `Create` would add it.
//
// `?OpenFile@FileSystem@IO@@` is already ported and registered (Wave 9 lane
// PKG2 rung 3) and returns a `FileInfo` BY VALUE — RDX is the hidden return
// slot, R8 the Filename.
//
// THE POD CASTS ON `path` ARE THE `pod_bridge.h` SEAM, not a bridge this block
// adds: `?OpenFile@FileSystem@IO@@`'s ported entry point still takes the
// `RtSystem::Filename` POD, and it belongs to PACKAGE+FS's file set. Retail's
// own signature is the class, which is what the declaration says.
bool FileStream::Open(const System::Filename& path) {
    ++RtIO::g_fss.open;
    const RtSystem::Filename* podPath =
        reinterpret_cast<const RtSystem::Filename*>(&path);

    // ---- 1800aac92..1800aaccc: this->filename = *path -------------------
    // `System::String::operator=` (0x1800c9810) — the short-circuit that jumps
    // past BOTH halves is the class's, and Open does not normalise either.
    reinterpret_cast<System::String&>(_filename) = path.AsString();

    _direction = 0;                                       // 1800aaccd — READING

    RtIO::FileInfo tmp;                                   // not zero-initialised
    RtIo_FileSystem_OpenFile(RtIO::Fs(), &tmp, podPath);  // 1800aacdf
    RtIo_FileInfo_Assign(&_file, &tmp);                   // 1800aacec
    RtSystem::SysFile(tmp._name1).~Filename();            // 1800aacf2  +0x28 first
    RtSystem::SysFile(tmp._name0).~Filename();            // 1800aad29  +0x20

    const bool ok = (_file.handle != -1);                 // 1800aad60
    if (ok) ++RtIO::g_fss.openTrue;
    return ok;
}

// ============================================================================
// IO::BinaryFileStream slot 0                            retail 0x180023d80
// ============================================================================
// The BODY is `RtIo_BinaryFileStream_DeletingDtor` above, registered by address
// at 0x180023d80 and filling slot 0 of the live minted table
// (rt_data_mint_tables.inc:1662). B34_RULES section 4.2 forbids retiring that
// row in this step, so the override NAMES the existing door rather than
// restating the thunk.
//
// It exists at all only because `IO::CompressedFileStream` holds a
// `BinaryFileStream` BY VALUE at +0x68: a member of abstract type is
// ill-formed, so this one slot cannot stay `= 0` the way `IO::FileStream`'s
// does. Same shape, same reason, as `IO::MemoryStream`'s.
Stream* BinaryFileStream::DeletingDtor(unsigned flags) {
    return static_cast<BinaryFileStream*>(
        RtIO::RtIo_BinaryFileStream_DeletingDtor(this, flags));
}

}  // namespace IO


// ============================================================================
// Registration for the seven slot bodies in this file.
// ============================================================================
int RtIoFileStreamSlotsRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("?GetSource@FileStream@IO@@UEBA?AVString@System@@XZ",
                          (void*)&RtIo_FileStream_GetSource); ++n;
    RegisterRuntimeAddr(0x1800aab50, (void*)&RtIo_FileStream_GetSource);

    RegisterRuntimeExport("?Read@FileStream@IO@@UEAA_NHPEAX@Z",
                          (void*)&RtIo_FileStream_Read); ++n;
    RegisterRuntimeAddr(0x1800aabf0, (void*)&RtIo_FileStream_Read);

    RegisterRuntimeExport("?GetCurrentPos@FileStream@IO@@UEBAIXZ",
                          (void*)&RtIo_FileStream_GetCurrentPos); ++n;
    RegisterRuntimeAddr(0x1800aac50, (void*)&RtIo_FileStream_GetCurrentPos);

    RegisterRuntimeExport("?GetSize@FileStream@IO@@UEBAIXZ",
                          (void*)&RtIo_FileStream_GetSize); ++n;
    RegisterRuntimeAddr(0x1800aac60, (void*)&RtIo_FileStream_GetSize);

    RegisterRuntimeExport("?Open@FileStream@IO@@UEAA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileStream_Open); ++n;
    RegisterRuntimeAddr(0x1800aac70, (void*)&RtIo_FileStream_Open);

    // Both UNEXPORTED — the address is their only door.
    RegisterRuntimeAddr(0x18000c090, (void*)&RtIo_FileStream_DeletingDtor);
    RegisterRuntimeAddr(0x180023d80, (void*)&RtIo_BinaryFileStream_DeletingDtor);

    // The SECOND stamper of ??_7BinaryFileStream@. Without it that row keeps a
    // door onto retail's table and is NOT promotable, whatever its trap count
    // says -- see the body's header note.
    RegisterRuntimeExport("??0BinaryFileStream@IO@@QEAA@AEBV01@@Z",
                          (void*)&RtIo_BinaryFileStream_CopyCtor); ++n;
    RegisterRuntimeAddr(0x180023ca0, (void*)&RtIo_BinaryFileStream_CopyCtor);

    extern int RtIoFileStreamWinRegister();
    n += RtIoFileStreamWinRegister();

    printf("[RtPort]   IO::FileStream vtable slots (wave 19 lane FILE, post-report): "
           "%d exports + 2 unexported addresses registered -- "
           "clears ??_7FileStream@ and ??_7BinaryFileStream@ to TRAP=0\n", n);
    return n;
}
