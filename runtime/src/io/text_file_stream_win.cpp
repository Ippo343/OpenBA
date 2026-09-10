// ============================================================================
// runtime/src/io/text_file_stream_win.cpp — Wave 20, lane SAVETEXT
//   Five of the eight unported slots of `??_7TextFileStream@IO@@6B@`.
// ============================================================================
//
//   slot [3] +0x18  ?Read@TextFileStream@IO@@UEAA_NHPEAX@Z          0x1800ba900
//   slot [4] +0x20  ?SetCurrentPos@TextFileStream@IO@@UEAA_NH@Z     0x1800babd0
//   slot [5] +0x28  ?GetCurrentPos@TextFileStream@IO@@UEBAIXZ       0x1800bac30
//   slot [6] +0x30  ?GetSize@TextFileStream@IO@@UEBAIXZ             0x1800ba8e0
//   slot [9] +0x48  ?Create@TextFileStream@IO@@UEAA_NAEBVFilename.. 0x1800ba940
//
// Slots [1] and [7] are FileStream's `GetSource` (0x1800aab50) and `Open`
// (0x1800aac70) and were ALREADY PORTED by Wave 19 lane FILE — verified here in
// all three forms before writing a line (mangled name, retail VA, and the
// `RtIo_FileStream_<Method>` convention; file_stream.cpp:685/784 and :958/974).
// They are not re-ported. Slots [0], [2] and [8] are NOT in this file; the
// DEFERRED section at the bottom says exactly why, each with its own reason.
//
// ---------------------------------------------------------------------------
// THE ONE FACT THAT ORGANISES ALL FIVE: TextFileStream IS TWO STREAMS
// ---------------------------------------------------------------------------
// `TextFileStream::buffer` (+0x70) is the discriminator, and FOUR of these five
// bodies branch on it:
//
//     buffer != 0   -> an in-MEMORY stream over that buffer,
//                      size at +0x78, cursor at +0x7c
//     buffer == 0   -> a FILE stream, delegating to the inherited FileInfo,
//                      size at +0x28, cursor at +0x2c
//
// That pairing is what pins +0x78 as the buffer's SIZE and +0x7c as its CURSOR:
// `GetSize` returns +0x78 or +0x28 from the two arms of one branch and
// `GetCurrentPos` returns +0x7c or +0x2c from the two arms of another, so each
// buffer field is identified by the inherited field it stands in for, and those
// two inherited offsets are independently asserted in FileStream.h.
//
// FileStream.h models +0x78 as `void* _unk78`, "zeroed by the ctor, never
// freed". THESE BODIES SHOW IT IS AN int32 SIZE, and the "never freed" half
// stops being an observation and becomes a consequence: it is not a pointer, so
// there is nothing to free. The header is not edited by this lane — the field
// is read here through an explicit offset rather than through the member, so
// that the disagreement is visible at the point of use instead of being quietly
// resolved in a shared header that other lanes' bodies also compile against.
//
// `Read` (slot 3) is the exception: it does NOT branch. It reads from the
// buffer unconditionally, with no null check — see its own note.
//
// ---------------------------------------------------------------------------
// WHY `_win`
// ---------------------------------------------------------------------------
// PLAN.md §4.1 decision 1 keeps <windows.h> out of `runtime/src`, with the
// `_win` suffix as the declared escape hatch. Two bodies here need it:
// `SetCurrentPos` calls `SetFilePointer` (import 0x18014b0f0) and `Create`
// calls `WriteFile` (import 0x18014b108), both DIRECTLY rather than through any
// stream virtual — the same bypass `?SaveText@External@Object@@` makes.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include <windows.h>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"

namespace RtIO {

// THE TWO NAMESPACES ARE ONE LETTER APART AND BOTH EXIST. The stream TYPES live
// in `RtIo` (runtime/types/io/FileStream.h:69) and the ported BODIES live in
// `RtIO` (this namespace, and file_stream.cpp:166). `FileInfo` is in `RtIO`.
// file_stream.cpp:176-179 carries the same four using-declarations for the same
// reason; without them `TextFileStream` does not name a type inside `RtIO` and
// the error points at the parameter list rather than at the namespace.
using RtIo::Stream;
using RtIo::FileStream;
using RtIo::TextFileStream;
using RtSystem::Filename;

// `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` (0x1800aa8f0) — PORTED
// and registered by lane FILE (file_stream.cpp:483 and :559). Called DIRECTLY,
// not through a door: a door would resolve to this same body and add a resolve
// site the cutover census has to carry
// (reference_count_the_doors_not_the_calls.md).
extern "C" bool RtIo_FileStream_Create(FileStream* self, const Filename* path);

// The two buffer-mode fields, reached by offset. See the header note on why
// they are not spelled as members of `TextFileStream`.
inline int32_t* TfsBufSize(TextFileStream* s) {
    return reinterpret_cast<int32_t*>(reinterpret_cast<char*>(s) + 0x78);
}
inline int32_t* TfsBufCursor(TextFileStream* s) {
    return reinterpret_cast<int32_t*>(reinterpret_cast<char*>(s) + 0x7c);
}
inline const int32_t* TfsBufSize(const TextFileStream* s) {
    return reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(s) + 0x78);
}
inline const int32_t* TfsBufCursor(const TextFileStream* s) {
    return reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(s) + 0x7c);
}

// ---------------------------------------------------------------------------
// EFFECTIVENESS COUNTERS. `calls` alone cannot separate a body that ran from
// one that did anything
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md), so every
// body here counts WHICH ARM it took. For the four that branch on `buffer` the
// two arms are counted separately; a run where one arm reads 0 has exercised
// the other arm only, and says so.
// ---------------------------------------------------------------------------
struct TfsCounters {
    uint64_t read;                             // slot 3
    uint64_t setPosBuffer, setPosFile;         // slot 4, by arm
    uint64_t setPosSeek;                       // ... and the Win32 sub-arm
    uint64_t getPosBuffer, getPosFile;         // slot 5, by arm
    uint64_t getSizeBuffer, getSizeFile;       // slot 6, by arm
    uint64_t create, createFailed, createBom;  // slot 9
};
TfsCounters g_tfs{};

// ============================================================================
// IO::TextFileStream::GetSize() const                    retail 0x1800ba8e0
// IO::TextFileStream::GetCurrentPos() const              retail 0x1800bac30
// ============================================================================
// Six instructions each, and the SAME six-instruction shape:
//
//     CMPQ $0,[RCX+0x70] ; JE file ; MOV EAX,[RCX+0x78 or 0x7c] ; RET
//     file: MOV EAX,[RCX+0x28 or 0x2c] ; RET
//
// NEITHER HAS A `.pdata` ENTRY — both are leaf functions with no prologue, so
// the unwinder needs none. Worth stating because `tools/mint_extent_audit.py`
// and every other `.pdata`-driven instrument CANNOT SEE THESE TWO FUNCTIONS AT
// ALL: asked "which function owns 0x1800ba8e0?" it has no answer, and a reader
// who takes that silence for "no such function" concludes the slot is empty.
// The vtable is the authority here and the vtable names both.
//
// Both return UNSIGNED (`UEBAIXZ`) while `FileInfo::cursor` and `::size` are
// modelled `int32_t` — identical bits, and the return TYPE is what the caller's
// comparison sees. Same note as FileStream's pair, and the same reason it is
// only visible in the export directory: a one-instruction body cannot show a
// return type.
// ============================================================================
extern "C" uint32_t RtIo_TextFileStream_GetSize(const TextFileStream* self) {
    RT_DIFF_BEGIN("IO::TextFileStream::GetSize");
    if (self->buffer != nullptr) {                                  // 1800ba8e0
        ++g_tfs.getSizeBuffer;
        return static_cast<uint32_t>(*TfsBufSize(self));            // 1800ba8e7  +0x78
    }
    ++g_tfs.getSizeFile;
    return static_cast<uint32_t>(self->base.file.size);             // 1800ba8eb  +0x28
}

extern "C" uint32_t RtIo_TextFileStream_GetCurrentPos(const TextFileStream* self) {
    RT_DIFF_BEGIN("IO::TextFileStream::GetCurrentPos");
    if (self->buffer != nullptr) {                                  // 1800bac30
        ++g_tfs.getPosBuffer;
        return static_cast<uint32_t>(*TfsBufCursor(self));          // 1800bac37  +0x7c
    }
    ++g_tfs.getPosFile;
    return static_cast<uint32_t>(self->base.file.cursor);           // 1800bac3b  +0x2c
}

// ============================================================================
// IO::TextFileStream::Read(int, void*)                   retail 0x1800ba900
// ============================================================================
//     memmove(buf, (char*)this->buffer + this->bufCursor, len);
//     this->bufCursor += len;
//     return true;
//
// THREE THINGS THIS BODY DOES NOT DO, each of which a port written from the
// signature rather than from the bytes would add:
//
//  1. IT DOES NOT BRANCH ON `buffer`. Unlike its three neighbours it reads the
//     buffer unconditionally. With `buffer == 0` and `bufCursor == 0` that is a
//     read from address 0 — retail's, not ours, and NOT to be guarded (the
//     project's "don't fix a faithful crash with a null check" rule). The
//     asymmetry is real and is why a `TextFileStream` cannot be read from until
//     something has installed a buffer; `?Open@TextFileStream@` (slot 8, NOT in
//     this file) is what installs one.
//  2. IT DOES NOT BOUNDS-CHECK. `bufCursor + len` is never compared against the
//     size at +0x78. `SetCurrentPos` range-checks and `Read` does not.
//  3. IT ALWAYS RETURNS TRUE. `MOV AL,1` at 0x1800ba92e is unconditional —
//     there is no failure path, and the return value carries no information.
//
// THE COPY IS `memmove`, NOT `memcpy`. 0x18008a290 is the DLL's statically
// linked `memmove`; runtime/src/system/error.cpp:152 established that from its
// prologue (the `SUB RDX,RCX / JAE` overlap test at 0x18008a2a0, which a
// `memcpy` does not have) and runtime/src/save/saveload_loadmeta.cpp:342 says
// it again. A `std::memcpy` here would be UB for an overlapping range that
// retail handles, and the compiler is free to make that difference visible.
//
// NOTE, NOT ACTED ON: runtime/src/ui/spritecontext_flush.cpp:146 calls
// 0x18008a290 "the DLL's `memcpy`". That comment disagrees with the two above
// and with the prologue. It is another row's file and this lane has not touched
// it; flagged for whoever owns it.
// ============================================================================
extern "C" bool RtIo_TextFileStream_Read(TextFileStream* self, int32_t len, void* buf) {
    RT_DIFF_BEGIN("IO::TextFileStream::Read");
    ++g_tfs.read;

    int32_t* const cursor = TfsBufCursor(self);

    // 1800ba90a..1800ba921. BOTH `len` and the cursor are sign-extended to 64
    // bits (`MOVSLQ`) before the add, so a negative cursor addresses BACKWARDS
    // from the buffer rather than wrapping to a huge positive offset.
    std::memmove(buf,
                 static_cast<const char*>(self->buffer) +
                     static_cast<int64_t>(*cursor),
                 static_cast<size_t>(static_cast<int64_t>(len)));   // 1800ba921

    *cursor += len;                                                // 1800ba926
    RT_DIFF_WORK(true);
    return true;                                                   // 1800ba92e
}

// ============================================================================
// IO::TextFileStream::SetCurrentPos(int)                 retail 0x1800babd0
// ============================================================================
// The two arms answer the same question in two different ways:
//
//   buffer:  bufCursor = pos   (ALWAYS, even when out of range)
//            return 0 <= pos < bufSize        <- a RANGE TEST, no I/O at all
//   file:    file.cursor = pos (ALWAYS, on both sub-arms, before the branch)
//            packed handle -> return true     <- no Win32 call at all
//            -1 or -2      -> return SetFilePointer(winHandle,pos,0,0) == pos
//
// THE FILE ARM IS `?SetCurrentPos@FileStream@IO@@` (0x1800aac10), which
// file_stream_win.cpp:128 already ports. It is TRANSCRIBED here rather than
// delegated to that body: retail duplicates the code rather than calling it, so
// delegating would attribute this slot's calls to the other slot's counters and
// would make an A/B on `BANISHED_RT_FSSLOTS` silently move this row too.
//
// The discriminator is `CMP dword [RCX+0x20],-0x3 ; JA` and it is **UNSIGNED**:
// the Win32 path is taken when `handle` is -1 or -2 ("nothing opened" and
// "loose file"), the early `return true` when it is a PACKED
// `(pack << 24) | fileIndex` descriptor. Read as a SIGNED test it inverts, and
// every packed seek issues a `SetFilePointer` on a handle that is not one.
// FileSystem.h documents the same `CMP ..,-3 ; JA` idiom in `ReadFile`; this is
// a second, independent body using it.
//
// THE BUFFER ARM'S RANGE TEST IS `pos < size`, NOT `pos <= size`
// (`CMP EDX,[RCX+0x78] ; JGE fail` at 0x1800babe6), so seeking exactly to the
// end reports FALSE while still moving the cursor there. Both halves matter: a
// caller that seeks to EOF and trusts the return would stop early, and one that
// ignores the return would still be correctly positioned.
// ============================================================================
extern "C" bool RtIo_TextFileStream_SetCurrentPos(TextFileStream* self, int32_t pos) {
    RT_DIFF_BEGIN("IO::TextFileStream::SetCurrentPos");

    if (self->buffer != nullptr) {                                 // 1800babd6
        ++g_tfs.setPosBuffer;
        const int32_t size = *TfsBufSize(self);
        *TfsBufCursor(self) = pos;                                 // 1800babdf  +0x7c
        const bool ok = (pos >= 0) && (pos < size);                // 1800babe2/e6
        RT_DIFF_WORK(ok);
        return ok;
    }

    ++g_tfs.setPosFile;
    self->base.file.cursor = pos;                                  // 1800bac02  +0x2c

    // UNSIGNED — see the header note.
    if (static_cast<uint32_t>(self->base.file.handle) <= 0xfffffffdu) {  // 1800babfe/05
        RT_DIFF_WORK(true);
        return true;                                               // 1800bac07
    }

    ++g_tfs.setPosSeek;
    const DWORD got = ::SetFilePointer(
        reinterpret_cast<HANDLE>(self->base.file.winHandle),       // 1800bac0f  +0x38
        pos, nullptr, FILE_BEGIN);                                 // 1800bac13/16
    const bool ok = (static_cast<int32_t>(got) == pos);            // 1800bac1f
    RT_DIFF_WORK(true);
    return ok;
}

// ============================================================================
// IO::TextFileStream::Create(Filename const&, Text::Format, Text::Format)
//                                                        retail 0x1800ba940
// ============================================================================
// THE ARGUMENT LIST IS TWO FORMATS, NOT A FORMAT AND A FILENAME. The mangled
// name ends `W4Format@Text@4@1@Z`, and that trailing `1` is a BACK-REFERENCE to
// back-reference-table entry 1, which is `W4Format@Text@4@` itself — entry 0 is
// `AEBVFilename@System@@`. So the signature is
// `(Filename const&, Text::Format, Text::Format)`.
//
// The DISASSEMBLY is the check that matters, and it agrees:
// `MOV [RCX+0x68],R8D ; MOV [RCX+0x6c],R9D` stores BOTH of the last two
// arguments as DWORDS, which a `Filename const&` arriving in R9 could not be.
// Read as `(Filename const&, Format, Filename const&)`, a port would truncate a
// pointer into `_activeEncoding` and then pick a byte-order mark out of the low
// half of an address.
//
// So the two format arguments land exactly on the pair FileStream.h names:
//     R8D -> +0x68 `_requestedFormat`      R9D -> +0x6c `_activeEncoding`
// and this is a THIRD, independent source for those being two separate fields.
// The constructor writes both to 6 and `?SaveText@External@Object@@` writes both
// to 2 — NEITHER of those can tell the two apart. A caller that can pass
// DIFFERENT values to them can, and this is that caller.
//
// The body is four steps:
//   1. store both formats                                    1800ba949/4d
//   2. `FileStream::Create(path)`; on false RETURN FALSE, no BOM    1800ba951
//   3. the byte-order mark, selected by `_activeEncoding` ALONE:
//        0 or 2 -> none    3 -> EF BB BF    4 -> FF FE    5 -> FE FF
//      written with a DIRECT `WriteFile` on `file.winHandle`, bypassing the
//      stream's own `Write` virtual, with the byte count added to `file.cursor`
//      by hand. Retail's chain is `test / sub 2 / dec / dec / dec`, so any value
//      outside {0,2,3,4,5} also falls through to "none".
//   4. `return true` — UNCONDITIONALLY, and NOT the result of `WriteFile`
//      (`MOV AL,1` at 0x1800ba9d6 is the join point of the no-BOM and the BOM
//      paths alike). A failed mark is invisible to the caller.
//
// THIS IS THE SAME BOM TABLE `?SaveText@External@Object@@` CARRIES, reached from
// a different entry point: external_savetext_win.cpp transcribed it from
// 0x1800bbaba..0x1800bbaca and this body has it at 0x1800ba978..0x1800ba9a0.
// The two agree, and that is the FIRST CORROBORATION that table has had —
// SaveText's copy is unreachable as shipped (it hard-sets `_activeEncoding` to 2
// eleven instructions earlier) and rested on inspection plus one negative
// control. Create's copy is reachable by any caller that passes a format.
//
// RETAIL BUILDS THE MARK IN A WAY WORTH DESCRIBING EXACTLY, because the obvious
// reading of the disassembly is wrong. It writes the two leading bytes to a
// scratch at RSP+0x50, the UTF-8 third byte 0xBF DIRECTLY to RSP+0x5a, then
// copies only the two scratch bytes to RSP+0x58 and passes RSP+0x58 to
// `WriteFile`. RSP+0x58..0x5a is therefore the real buffer, and its third byte
// was placed before its first two. The scratch at RSP+0x50 is then REUSED as
// `lpNumberOfBytesWritten` (zeroed at 0x1800ba9c0, after R9 already points at
// it). Modelled below as one 3-byte buffer plus a separate counter, which is
// what the machine ends up doing; the aliasing is retail's stack economy, not
// semantics, and nothing observable depends on it.
// ============================================================================
extern "C" bool RtIo_TextFileStream_Create(TextFileStream* self, const Filename* path,
                                           int32_t requestedFormat,
                                           int32_t activeEncoding) {
    RT_DIFF_BEGIN("IO::TextFileStream::Create");
    ++g_tfs.create;

    self->requestedFormat = requestedFormat;                 // 1800ba949  +0x68
    self->activeEncoding  = activeEncoding;                  // 1800ba94d  +0x6c

    if (!RtIo_FileStream_Create(&self->base, path)) {        // 1800ba951
        ++g_tfs.createFailed;
        RT_DIFF_WORK(false);
        return false;                                        // 1800ba95a
    }

    uint8_t bom[3] = { 0, 0, 0 };
    int32_t bomLen = 0;
    switch (self->activeEncoding) {                          // 1800ba960..76
        case 3:  bom[0] = 0xEF; bom[1] = 0xBB; bom[2] = 0xBF;  // 1800ba994
                 bomLen = 3; break;
        case 4:  bom[0] = 0xFF; bom[1] = 0xFE;                 // 1800ba985
                 bomLen = 2; break;
        case 5:  bom[0] = 0xFE; bom[1] = 0xFF;                 // 1800ba978
                 bomLen = 2; break;
        default: break;                                        // 0, 2, anything else
    }

    if (bomLen != 0) {
        ++g_tfs.createBom;
        DWORD written = 0;                                                    // 1800ba9c0
        ::WriteFile(reinterpret_cast<HANDLE>(self->base.file.winHandle),      // 1800ba9ab
                    bom, static_cast<DWORD>(bomLen), &written, nullptr);      // 1800ba9c9
        self->base.file.cursor += static_cast<int32_t>(written);              // 1800ba9d3
    }

    RT_DIFF_WORK(true);
    return true;                                             // 1800ba9d6
}

// Read by text_file_stream_probe.cpp so attendance prints beside the verdicts
// and a reader cannot see one without the other.
void RtIoTextFileStreamCounters(unsigned long long* out) {
    out[0]=g_tfs.read;
    out[1]=g_tfs.setPosBuffer;  out[2]=g_tfs.setPosFile;   out[3]=g_tfs.setPosSeek;
    out[4]=g_tfs.getPosBuffer;  out[5]=g_tfs.getPosFile;
    out[6]=g_tfs.getSizeBuffer; out[7]=g_tfs.getSizeFile;
    out[8]=g_tfs.create;        out[9]=g_tfs.createFailed;  out[10]=g_tfs.createBom;
}

}  // namespace RtIO

// ============================================================================
// DEFERRED — the three slots NOT in this file, each with its own reason and its
// own expiry condition, because "needs X" is a TODO and not a blocker.
// ============================================================================
//   [0] +0x00  0x18002ba90  deleting dtor — UNEXPORTED, so its address is its
//       only door, and it needs TWO more bodies first: `??1TextFileStream@`
//       (0x1800ba490 — EXPORTED, unported) and the CRT's vector-destructor
//       iterator (0x18007ec98, unexported). Porting it as it stands would add
//       two address doors to the cutover surface to close one slot
//       (reference_porting_can_grow_the_cutover_surface.md). EXPIRES when
//       `??1TextFileStream@` is ported — after that this slot is small.
//       IT IS ALSO THE SOURCE OF A LAYOUT FACT ALREADY IN USE: its `MOV EDX,0x80`
//       element stride is where FileStream.h's `sizeof(TextFileStream) == 0x80`
//       comes from.
//   [2] +0x10  0x1800ba9f0  Write — 462 bytes (`.pdata`). Branches on
//       `_requestedFormat != _activeEncoding` into a TRANSCODER (0x1800c9d60, a
//       format-detect helper) with its own five-way chain. NOT ATTEMPTED — this
//       is a size judgement, not a blocker, and it has no premise to expire.
//   [8] +0x40  0x1800ba640  Open(Filename, Text::Format) — 666 bytes, the
//       largest body in the table, and the one that INSTALLS the buffer that
//       four of the five bodies above branch on. NOT ATTEMPTED, same reason.
//
// So `??_7TextFileStream@IO@@6B@` goes from 2 ported / 10 slots to 7 / 10.
// IT IS STILL NOT PROMOTABLE: three trap slots remain. This file claims the
// slot count and nothing about promotion.
// ============================================================================

// ============================================================================
// Registration — BY NAME *AND* BY ADDRESS for every row, because
// `ResolveExport` falls through silently to the shipped body for an unported
// symbol and `ResolveAddr(Module::Runtime, ..)` is registry-first: a caller that
// arrives by ADDRESS is invisible to a name-only registration and dies at
// cutover (reference_count_the_doors_not_the_calls.md).
// ============================================================================
int RtIoTextFileStreamSlotsRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("?Read@TextFileStream@IO@@UEAA_NHPEAX@Z",
                          (void*)&RtIo_TextFileStream_Read); ++n;
    RegisterRuntimeAddr(0x1800ba900, (void*)&RtIo_TextFileStream_Read);

    RegisterRuntimeExport("?SetCurrentPos@TextFileStream@IO@@UEAA_NH@Z",
                          (void*)&RtIo_TextFileStream_SetCurrentPos); ++n;
    RegisterRuntimeAddr(0x1800babd0, (void*)&RtIo_TextFileStream_SetCurrentPos);

    RegisterRuntimeExport("?GetCurrentPos@TextFileStream@IO@@UEBAIXZ",
                          (void*)&RtIo_TextFileStream_GetCurrentPos); ++n;
    RegisterRuntimeAddr(0x1800bac30, (void*)&RtIo_TextFileStream_GetCurrentPos);

    RegisterRuntimeExport("?GetSize@TextFileStream@IO@@UEBAIXZ",
                          (void*)&RtIo_TextFileStream_GetSize); ++n;
    RegisterRuntimeAddr(0x1800ba8e0, (void*)&RtIo_TextFileStream_GetSize);

    RegisterRuntimeExport(
        "?Create@TextFileStream@IO@@UEAA_NAEBVFilename@System@@W4Format@Text@4@1@Z",
        (void*)&RtIo_TextFileStream_Create); ++n;
    RegisterRuntimeAddr(0x1800ba940, (void*)&RtIo_TextFileStream_Create);

    printf("[RtPort]   IO::TextFileStream vtable slots (wave 20 lane SAVETEXT): "
           "%d exports registered -- ??_7TextFileStream@ 2/10 -> 7/10 "
           "(slots 0, 2, 8 still trap)\n", n);
    return n;
}
