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
// Kill switch: BANISHED_RT_FILESTREAM=0 leaves all of it on the DLL.
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
//   and ??_7TextFileStream@ (0x180155980) are DEFERRED to lane MINT by the
//   Wave 19 brief: WAVE19_IO.md §0b found all three appear ZERO times by VA in
//   standalone/rt_data_mint_tables.inc, and phase 0c reproduced that from a
//   second instrument. Every vtable store below therefore goes through a door
//   that today reads the DLL. AT CUTOVER THEY NEED LANE MINT'S MINT. This is a
//   real residual and it is named in WAVE19_LANE_FILE.md rather than glossed.
//
//   ??_7Stream@IO@@6B@ (0x1801549a0) and the shared Encoding table
//   (0x1801554a8) ARE minted already — checked BY VA in
//   rt_data_mint_tables.inc, not by name. Phase 0b warns that the name grep
//   returns `adjacent-export:` labels on OTHER rows and looks exactly like a
//   hit; 0x1801559d0 is NOT minted.
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
#include "../../funcs/rt_diff.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Rung 4 of Wave 9 lane PKG2 (filesystem_openfile_win.cpp), already ported and
// registered. Declared here rather than pulled in from a header because that
// lane publishes none and this lane does not edit another lane's files.
extern "C" RtIO::FileInfo* RtIo_FileInfo_Assign(RtIO::FileInfo*, const RtIO::FileInfo*);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::RtString_ReleaseInline;
using RtSystem::StringRefCount;

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
    RtString_ReleaseInline(reinterpret_cast<String*>(slot));
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
// The table goes in BY ADDRESS because neither of the two this family uses has
// an exported name — see the header note.
// ---------------------------------------------------------------------------
void InstallEncoding(Stream* s, uintptr_t vtableAddr) {
    void* enc = RtSystem::RtMemAlloc(8, 4);                                // 1800a82a5
    if (enc) *reinterpret_cast<uintptr_t*>(enc) =                          // 1800a82bb
        ResolveAddr(Module::Runtime, vtableAddr);
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
// FIRST: BANISHED_RT_DIFF=1 makes every diffed port also run the original, and
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
    RT_DIFF_BEGIN("IO::FileSystem::FileInfo::~FileInfo");
    ++g_fsc.fileInfoDtor;
    RtString_ReleaseInline(&self->_name1);   // 18000c19f  +0x28 FIRST
    RtString_ReleaseInline(&self->_name0);   // 18000c1ce  +0x20
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
    RT_DIFF_BEGIN("IO::FileStream::~FileStream");
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
    RT_DIFF_BEGIN("IO::BinaryFileStream::~BinaryFileStream");
    ++g_fsc.binDtor;
    RtIo_FileStream_Dtor(&self->base);                // 180023c96 JMP
}

// ============================================================================
// IO::BinaryFileStream::BinaryFileStream                 retail 0x1800a8200
// ============================================================================
// The inlined FileStream prologue, then the class's own vtable, then a fresh
// Encoding carrying the SHARED table at 0x1801554a8 (unexported — by address).
// ============================================================================
extern "C" BinaryFileStream* RtIo_BinaryFileStream_Ctor(BinaryFileStream* self) {
    RT_DIFF_BEGIN("IO::BinaryFileStream::BinaryFileStream");
    ++g_fsc.binCtor;

    FileStreamCtorInline(&self->base);                                   // 1800a8219..70
    self->base.base.vptr =
        RT_DATA_SLOT(const void*, "??_7BinaryFileStream@IO@@6B@");        // 1800a8270

    InstallEncoding(&self->base.base, 0x1801554a8);                       // 1800a827a..d8

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
    RT_DIFF_BEGIN("IO::TextFileStream::TextFileStream");
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

    InstallEncoding(&self->base.base, 0x1801559d0);                       // 1800ba3ee..4e

    RT_EXPORT_ONCE(SetObjectFn,
        "?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z")(
            self, type, name, version);                                   // 1800ba45e

    return self;                                                          // 1800ba464
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
    RT_DIFF_BEGIN("IO::FileStream::Create");
    ++g_fsc.create;

    RtIo_FileSystem_CreatePath(Fs(), path);                     // 1800aa919

    // ---- 1800aa91e..1800aa958: this->filename = *path -------------------
    char* const incoming = reinterpret_cast<const String*>(path)->data;
    if (self->filename != incoming) {                           // 1800aa924
        if (self->filename) {                                   // 1800aa929
            if (--StringRefCount(self->filename) == 0) {        // 1800aa92e/931
                self->filename -= 8;                            // 1800aa936
                RtSystem::RtMemFree(self->filename);            // 1800aa93f
            } else {
                self->filename = nullptr;                       // 1800aa946
            }
        }
        self->filename = incoming;                              // 1800aa94d
        if (incoming) ++StringRefCount(incoming);               // 1800aa956
    }

    self->base.direction = 1;                                   // 1800aa959

    // ---- 1800aa960..1800aa9ef -------------------------------------------
    // NOT zero-initialised: CreateFileW writes every field of it that anything
    // reads, and _unk04 is one neither side ever writes.
    FileInfo tmp;
    RtIo_FileSystem_CreateFileW(Fs(), &tmp, path);              // 1800aa96f
    RtIo_FileInfo_Assign(&self->file, &tmp);                    // 1800aa97c
    RtString_ReleaseInline(&tmp._name1);                        // 1800aa982  +0x28 first
    RtString_ReleaseInline(&tmp._name0);                        // 1800aa9b9  +0x20

    const bool ok = (self->file.handle != -1);                  // 1800aa9f0
    if (ok) ++g_fsc.createTrue;
    RT_DIFF_WORK(ok);
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

    RegisterRuntimeExport("??1BinaryFileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_BinaryFileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x180023c80, (void*)&RtIo_BinaryFileStream_Dtor);

    RegisterRuntimeExport("??1FileStream@IO@@UEAA@XZ",
                          (void*)&RtIo_FileStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800aaab0, (void*)&RtIo_FileStream_Dtor);

    RegisterRuntimeExport("??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z",
                          (void*)&RtIo_TextFileStream_Ctor); ++n;
    RegisterRuntimeAddr(0x1800ba340, (void*)&RtIo_TextFileStream_Ctor);

    RegisterRuntimeExport("?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_FileStream_Create); ++n;
    RegisterRuntimeAddr(0x1800aa8f0, (void*)&RtIo_FileStream_Create);

    // UNEXPORTED — address only, and that is the ONLY door it has.
    RegisterRuntimeAddr(0x18000c180, (void*)&RtIo_FileInfo_Dtor);

    printf("[RtPort]   IO::FileStream family (wave 19 lane FILE): "
           "%d exports + 1 unexported address registered\n", n);
    return n;
}
