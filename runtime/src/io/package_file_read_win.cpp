// ============================================================================
// runtime/src/io/package_file_read_win.cpp — Wave 9 lane PKG3
// ?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z  0x1800b46b0
// ============================================================================
//
// THE LAST BODY BETWEEN LoadPackages AND A MOUNTED PACKAGE. Lane PKG2 ported
// every callee across five rungs and then spent four commits decoding this
// function without writing it (WAVE9_LANE_PKG2.md §67-§90). This file is the
// transcription, plus the two targeted reads §87 said had to happen first.
//
// 1,864 instructions in the shipped body; ~660 of distinct logic once the 28
// inlined copies of `?Read@CompressedStream@IO@@` collapse back to calls. That
// substitution is not a paraphrase and it is proved twice — §68 (the inlined
// loop IS rung 2's body, and the copies discard the return value exactly as
// ignoring the `bool` does) and §75 (retail itself left TWO of the sites as
// real `CALL 0x1800aa780`, to the same function).
//
// WHY THE FILE IS `_win`: ONE Win32 call. `SetFilePointer` at 0x1800b47e3,
// through Runtime's IAT slot 0x18014b0f0, and only on the loose-file branch. A
// `_posix` twin needs exactly that one seam (PLAN.md §4.1 decision 1).
//
// Kill switch: BANISHED_RT_PKG3=0 leaves ReadPackfile on the DLL.
//
// ---------------------------------------------------------------------------
// §91. THE TWO READS §87 REFUSED TO GUESS — BOTH ANSWERED, AND ONE OF THEM
//      WAS ABOUT TO BE GUESSED THE WRONG WAY ROUND BY FIELD NAME
// ---------------------------------------------------------------------------
// §87 stopped on the packfile-entry record's field assignment because "which
// goes to +0x08 and which to +0x0c is a coin flip on a field pair whose swap
// produces a packfile index that is wrong in a way no green run detects".
// The four instructions are at 0x1800b5fa4, and the frame identity that reads
// them is `RBP = RSP + 0x100` (from `LEA RBP,[RSP-0x1860]` at 0x1800b46c0
// followed by `SUB RSP,0x1960`; every other slot in this file is resolved by
// that same equation and each one cross-checks):
//
//   1800b5fa4  MOV EAX,dword ptr [RBP + -0x10]   ; the FIRST of the two reads
//   1800b5fa7  MOV dword ptr [RBP + -0x18],EAX   ; -> temp + 0x08  == offset
//   1800b5faa  MOV EAX,dword ptr [RSP + 0x3c]    ; the SECOND
//   1800b5fae  MOV dword ptr [RBP + -0x14],EAX   ; -> temp + 0x0c  == size
//
// The temp is at `[RBP-0x20]`, so `-0x18` is `+0x08` and `-0x14` is `+0x0c`.
// Against `PackageFileEntry` in runtime/types/io/FileSystem.h (+0x08 offset,
// +0x0C size, both pinned by the accessors' one-instruction bodies) that makes
// the ORDER ON DISK **offset first, size second**.
//
// And the second read, at 0x1800b5fbe:
//
//   1800b5fb1  MOV EDX,dword ptr [R13 + 0x8]     ; entries._size
//   1800b5fb5  INC EDX                           ; n = _size + 1
//   1800b5fb7  LEA R8,[RBP + -0x20]              ; fill = &temp
//   1800b5fbb  MOV RCX,R13
//   1800b5fbe  CALL 0x1800298f0                  ; Array<Entry>::SetSize(a,n,fill)
//
// `_size + 1`, as §87 predicted — and R13 is `this + 0xb0` (set at 0x1800b5b86),
// which independently re-confirms the `entries` / `entryCount` pair at +0xB0 /
// +0xB8 that FileSystem.h carries.
//
// ---------------------------------------------------------------------------
// §92. FOUR CORRECTIONS TO THE RECORDED DECODE. Three change what gets written.
// ---------------------------------------------------------------------------
//
// (a) **The +0xAC read is ONE BYTE, not four.** §75's table lists eight fields
//     as "4 each" and +0xac is in that list. At 0x1800b4b4b the count register
//     is `MOV ESI,0x1`, not `MOV ESI,0x4`; every other field in the run is 4.
//     +0xAC is `overridable`, a `uint8_t` in FileSystem.h — so retail reads a
//     one-byte bool, and a 4-byte read there would have consumed three bytes of
//     the next field (`crc`, +0xA8) and desynced everything after it. This is
//     the transcription's single most consequential correction.
//
// (b) **The uncompressed ELSE arm writes FOUR MORE FIELDS than §86 recorded.**
//     §86 has it as `+0x78 = 1; +0x7c = 1; SetSize(+0x80,1); [0] = 0xff000000`.
//     The full arm at 0x1800b5b25 is:
//         +0x68 = 0x010007ff   (dword)      <- MISSING from §86
//         +0x70 = 0x2710       (QWORD: priority 10000, platform 0)  <- MISSING
//         +0xa8 = 0            (crc)                                <- MISSING
//         +0xac = 1            (overridable)                        <- MISSING
//         +0x78 = 1;  +0x7c = 1;  Array<int32>::SetSize(+0x80, 1);
//         ((int32*)this->[0x80])[0] = 0xff000000
//     Three of the four are re-stating what the constructor already wrote, so
//     omitting them would have been invisible — except +0x68, which the
//     constructor leaves at 0 and which `?GetVersion@` returns.
//
// (c) **PATH A and PATH B do NOT merge at the same address.** §80 says groups
//     1-3 `JMP 0x1800b5b6c`; they do. Groups 4-6 jump to **0x1800b5b73**, one
//     instruction later — skipping `MOV R15D,[RBP+0x18b0]`. That is not
//     cosmetic: on PATH A the format version has to be RELOADED from the stack
//     before the entry loop reads it, because PATH A's decode loop clobbers
//     R15; on PATH B it is still live. Merging both arms at the same point in a
//     C++ transcription is harmless (the value is the same either way), and the
//     asymmetry is recorded here rather than silently smoothed over.
//
// (d) **`[RBP+0x18a0]` is `this`'s incoming home slot**, not a scratch length
//     slot as §80's table implies. `MOV [RSP+0x8],RCX` at 0x1800b46b0 stores
//     `this` there, and PATH A groups 1 and 2 RELOAD `this` from it after their
//     decode loops (0x1800b4fba, 0x1800b522a) because the loop uses R13 as a
//     constant register. Group 3 then reuses the slot as its String temp
//     (0x1800b54c2) — which is why group 3 is last and why nothing reloads
//     `this` after it. In C++ `this` is a parameter and none of this survives;
//     it is written down because reading §80's table as "these are two length
//     temps" is how a transcription ends up spilling a live pointer over.
//
// ---------------------------------------------------------------------------
// §93. THE TWO STACK BUFFERS OVERLAP, AND THE 0x800 CAP IS THE OVERLAP POINT
// ---------------------------------------------------------------------------
// The decoded-output buffer is `[RBP+0x60]` and the raw-read buffer is
// `[RBP+0x860]`. **0x860 - 0x60 = 0x800**, and 0x800 is exactly the cap the
// re-encode loop enforces (`CMP ESI,0x800 ; JG`). They are one region: the
// output buffer runs from the frame slot up to where the input buffer begins,
// and the cap is what stops the decoder from writing over its own input. So
// this file models them as ONE array with `buf860 = buf60 + 0x800`, which is
// what retail has, rather than as two independent arrays — which would silently
// change what happens on any packfile that hits the cap.
//
// Neither read is bounds-checked against the frame (`Read(len, buf860)` with
// `len` straight off disk). Reproduced, not "fixed": CLAUDE.md's rule, and a
// clamp here would change which packfiles this function accepts.
//
// ---------------------------------------------------------------------------
// §94. THE CODEC PAIR IS TRANSCRIBED FROM THE JUMP-TABLE BODIES
// ---------------------------------------------------------------------------
// The two dispatchers are `0x1800c9fc0` (decode) and `0x1800c9b60` (encode),
// both a `JMP` through a table indexed by an encoding id. Ghidra cannot make
// the table bodies into functions, so runtime/src/system/string.cpp and
// widestring.cpp both wrote their pair FROM SPEC and said so. This file does
// not have to: the two entries ReadPackfile actually uses were read out of
// .rdata and disassembled by hand, and they are nine and about forty bytes.
//
//   decode table 0x1800ca1b8, entry **1** -> 0x1800c9ff1, NINE instructions:
//       0fb602 0fb64a01 c1e108 0bc8 b802000000 418909 4883c408 c3
//     = `*cp = p[0] | (p[1] << 8); return 2;`
//     UTF-16LE, TWO BYTES PER CODEPOINT, **`end` is accepted and never read**,
//     no surrogate handling, no validation. This is §76's over-read at its
//     source: the caller reads `len` bytes and then walks `2 * len`.
//
//     NOTE the id: ReadPackfile passes **1**, not the 4 that widestring.cpp's
//     `Utf16DecodeOne` reproduces. Table entry 4 (0x1800ca0e8) is a different,
//     surrogate-aware body. Reusing widestring's helper here because "it is the
//     UTF-16 decoder" would have been the fourth by-resemblance defect this
//     lane has caught (cf. §51's alignment, §74's AdjustDelimiters).
//
//   encode table 0x1800c9d40, entry **2** -> 0x1800c9baf: plain UTF-8, the
//     four-way `< 0x80 / < 0x800 / < 0x10000 / < 0x200000` ladder, returning
//     1/2/3/4. Since the decoder above can only ever produce 0..0xFFFF, the
//     4-byte and error arms are UNREACHABLE from this caller; they are written
//     anyway (the ladder is the body) and the fact is recorded rather than the
//     arms omitted.
//
// ---------------------------------------------------------------------------
// §95. THE ORACLE, AND THE ONE EXCLUSION §76 REQUIRES
// ---------------------------------------------------------------------------
// `ReadPackfile` is not pure, but it is REPLAYABLE: everything it produces
// lands in `*this`, and `this` can be a second, freshly-constructed scratch
// object. So the oracle runs our body on the real object, then constructs a
// scratch `PackageFile` with our own (byte-proven, §5) constructor and runs the
// SHIPPED body on it with the same filename, then compares field by field.
//
// EXCLUSIONS, each with the instruction that justifies it:
//
//  * `_fileInfo.winHandle` (+0xD8) — two independent `CreateFileW` calls return
//    two different HANDLEs. Compared as "both valid or both -1", not by value.
//  * `_fileInfo._unk04` (+0xC4) — `?OpenFile@` never writes it
//    (filesystem_openfile_win.cpp §4); comparing memory neither side wrote is
//    reference_oracle_must_not_compare_unwritten_bytes.md.
//  * `_fileInfo._name0/_name1` — different String objects; compared by TEXT.
//  * **AND THE ONE THAT MATTERS: a string whose real data does not terminate
//    inside the bytes actually read.** §76: retail decodes `2 * len` bytes out
//    of a buffer it filled with `len`, so the tail comes from whatever was on
//    the stack. Ours and theirs decode from DIFFERENT stack frames with
//    DIFFERENT stale contents, and the resulting `strlen` — and therefore the
//    whole String, not just its tail — is legitimately allowed to differ.
//
//    This file does NOT hand-wave that as "compare up to the terminator". It
//    DECIDES it, per string, from data both sides read identically: a string is
//    DETERMINISTIC iff a terminator appears within the region actually read
//    (a zero 16-bit unit fully inside `[0, len)` on the UTF-16 arm, a zero byte
//    inside `[0, len)` on the raw arm). Deterministic strings are compared
//    strictly. Non-deterministic ones are counted and reported by name in the
//    census line, and excluded from the verdict. A run in which that count is
//    zero has NO exclusion in force at all, and says so.
//
// ---------------------------------------------------------------------------
// §96. COVERAGE — READ THE CENSUS LINE, NOT THE `divergences=0`
// ---------------------------------------------------------------------------
// Two conditionals gate large amounts of code and a corpus of shipped `.pkg`
// files probably cannot cover both sides of either (§72, §81):
//
//   * the header's SIGN BIT (`compressed`) — gates the whole metadata block;
//   * the FORMAT VERSION (`> 2` vs `<= 2`) — selects between two entirely
//     separate string implementations, TWICE (metadata and per-entry names),
//     and in the entry loop the arms are in the OPPOSITE order.
//
// So this file counts, INSIDE ITS OWN BODY, how many times each arm was taken,
// and prints them. A branch with `0` is UNTESTED and the report must say so; a
// green run over one arm is not coverage of the other. Counting branches taken
// inside the function rather than proxies observed outside it is the correction
// §65 earned twice.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/Stream.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../src/system/string.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Lane String's port. Forward-declared rather than pulled in from a header for
// the same reason filesystem_openfile_win.cpp does it: that lane publishes
// none, and the linker checks the two declarations against each other.
namespace RtSystem {
void RtSystem_Filename_AdjustDelimiters(Filename* self);
}  // namespace RtSystem

// ---- the ported callees, called DIRECTLY --------------------------------
// Every one of these is this lane's own port from an earlier rung. They are
// called by name, not through a resolver door: a door here would only be a way
// to reach the shipped body by accident
// (reference_bypassed_port_needs_a_source_checker.md).
namespace RtIO {
extern "C" FileInfo* RtIo_FileSystem_OpenFile(FileSystem*, FileInfo*,
                                              const RtSystem::Filename*);   // 0x1800ac880
extern "C" bool      RtIo_FileSystem_ReadFile(FileSystem*, FileInfo*,
                                              int32_t, void*);              // 0x1800ace00
extern "C" FileInfo* RtIo_FileInfo_Assign(FileInfo*, const FileInfo*);      // 0x18000c200
extern "C" PackageFileView* RtIo_PackageFile_Ctor(PackageFileView*);        // 0x1800b4440
}  // namespace RtIO

namespace RtIo {
extern "C" CompressedStream* RtIo_CompressedStream_Ctor(CompressedStream*);   // 0x1800aa3d0
extern "C" MemoryStream*     RtIo_MemoryStream_Ctor(MemoryStream*);           // 0x1800ae710
extern "C" bool RtIo_CompressedStream_Open(CompressedStream*, Stream*, int32_t); // 0x1800aa560
extern "C" bool RtIo_CompressedStream_Read(CompressedStream*, int32_t, void*);   // 0x1800aa780
extern "C" void RtIo_CompressedMemoryStream_Dtor(CompressedMemoryStream*);       // 0x1800a9560
}  // namespace RtIo

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;
using RtSystem::StringRefCount;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtString_SetLength;
using RtSystem::RtSystem_Filename_AdjustDelimiters;

using RtIo::CompressedStream;
using RtIo::CompressedMemoryStream;
using RtIo::MemoryStream;
using RtIo::Stream;

namespace {

const char* const kReadPackfile =
    "?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z";   // ord 3461

typedef bool (*ReadPackfileFn)(PackageFileView*, const Filename*);
typedef void (*VoidThisFn)(void*);
typedef void (*ArraySetSizeFn)(void*, int32_t);
typedef void (*ArraySetSizeFillFn)(void*, int32_t, const void*);
typedef void (*StreamVFn)(void*);

// ---- the frame's two overlapping stack buffers, §93 ------------------------
// `[RBP+0x60] .. [RBP+0x18a0)` is 0x1840 bytes; the raw-read buffer starts
// 0x800 into it. Sized and positioned exactly as retail's frame.
constexpr size_t kFrameBufBytes = 0x1840;
constexpr size_t kOutCap        = 0x800;    // = 0x860 - 0x60

// `?_singleton@FileSystem@IO@@0PEAV12@EA`  (0x1801fc6a8) — the global retail
// loads into RSI at 0x1800b46e8 and RELOADS at 0x1800b478b.
FileSystem** FileSystemSingletonCell() {
    return RT_DATA_SLOT(FileSystem**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
}

// ??_7CompressedMemoryStream@IO@@6B@ (0x180155560, ordinal 1409, verified in
// the DLL's own export directory). Through the data door, never the literal VA:
// at cutover a raw 0x180155560 would be a pointer into a module that is not
// loaded, stamped into the stack object every mount goes through.
const void* VtblCompressedMemoryStream() {
    static const void* v = ResolveExport("??_7CompressedMemoryStream@IO@@6B@");
    return v;
}

// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` / `?MemFree@`. Retail inlines
// the three-step allocator sequence at 0x1800b4807 and 0x1800c7a50 IS those
// three steps; reaching them by address puts lane E's port in the path, exactly
// as compressed_stream_open.cpp does.
void* MemAlloc(uint64_t bytes, int32_t align) {
    using Fn = void* (*)(uint64_t, int32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn(bytes, align);
}
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    fn(p);
}

// Lane Q's two-argument and three-argument Array::SetSize instantiations, both
// registered by lane PKG2's earlier rungs (runtime/src/collection/array.cpp).
// Neither has an exported name, so the address door is the only one there is.
void ArraySetSize(uintptr_t addr, void* arr, int32_t n) {
    ArraySetSizeFn fn = reinterpret_cast<ArraySetSizeFn>(
        ResolveAddr(Module::Runtime, addr));
    fn(arr, n);
}
void ArraySetSizeFill(uintptr_t addr, void* arr, int32_t n, const void* fill) {
    ArraySetSizeFillFn fn = reinterpret_cast<ArraySetSizeFillFn>(
        ResolveAddr(Module::Runtime, addr));
    fn(arr, n, fill);
}

// A Stream virtual through the OBJECT's vtable, exactly as retail dispatches it
// at 0x1800b603e (`MOV RAX,[RBP-0x58] ; CALL qword ptr [RAX+0x38]`).
void CallStreamClose(MemoryStream* ms) {
    const void* const* vtbl = *reinterpret_cast<const void* const* const*>(ms);
    reinterpret_cast<StreamVFn>(
        const_cast<void*>(vtbl[RtIo::kStreamVtblCloseSlot]))(ms);
}

// ---- §94: the two jump-table bodies ---------------------------------------

// decode table 0x1800ca1b8 entry 1 -> 0x1800c9ff1. Nine instructions, and
// `end` (R8) is accepted and NEVER READ. That is the over-read, at its source.
inline int32_t Decode1(const uint8_t* p, const uint8_t* /*end, ignored*/,
                       int32_t* cp) {
    const uint32_t lo = p[0];                            // 1800c9ff1 MOVZX EAX,[RDX]
    const uint32_t hi = p[1];                            // 1800c9ff4 MOVZX ECX,[RDX+1]
    *cp = static_cast<int32_t>((hi << 8) | lo);          // 1800c9ff8 SHL/OR/MOV [R9]
    return 2;                                            // 1800c9ffd MOV EAX,2
}

// encode table 0x1800c9d40 entry 2 -> 0x1800c9baf. The comparisons are SIGNED
// (`JL` / `JGE`), which is why a negative codepoint would take the one-byte
// arm — the caller never lets one through (`TEST ECX,ECX ; JS`).
inline int32_t Encode2(int32_t cp, uint8_t* dst) {
    if (cp < 0x80) {                                     // 1800c9baf CMP 0x80 / JL
        dst[0] = static_cast<uint8_t>(cp);               // 1800c9b88
        return 1;
    }
    if (cp < 0x800) {                                    // 1800c9bb8 CMP 0x800 / JGE
        dst[0] = static_cast<uint8_t>(((cp >> 6) & 0x1F) | 0xC0);   // 1800c9bc1..cf
        dst[1] = static_cast<uint8_t>((cp & 0x3F) | 0x80);          // 1800c9bd3
        return 2;
    }
    if (cp < 0x10000) {                                  // 1800c9be1 CMP 0x10000 / JGE
        dst[0] = static_cast<uint8_t>(((cp >> 12) & 0x0F) | 0xE0);  // 1800c9bea..f4
        dst[1] = static_cast<uint8_t>(((cp >> 6)  & 0x3F) | 0x80);  // 1800c9bf6..c08
        dst[2] = static_cast<uint8_t>((cp & 0x3F) | 0x80);          // 1800c9c0b
        return 3;
    }
    // UNREACHABLE FROM THIS CALLER: Decode1 tops out at 0xFFFF. Written because
    // the ladder is the body, and flagged rather than omitted.
    if (cp < 0x200000) {                                 // 1800c9c14 CMP 0x200000
        dst[0] = static_cast<uint8_t>(((cp >> 18) & 0x07) | 0xF0);
        dst[1] = static_cast<uint8_t>(((cp >> 12) & 0x3F) | 0x80);
        dst[2] = static_cast<uint8_t>(((cp >> 6)  & 0x3F) | 0x80);
        dst[3] = static_cast<uint8_t>((cp & 0x3F) | 0x80);
        return 4;
    }
    return 0;                                            // the 0x1800c9d3b error arm
}

// ---- branch census, §96 ---------------------------------------------------
// Counted INSIDE the body, one increment per arm actually entered. Not derived
// from anything observable outside the function.
struct Census {
    long long calls        = 0;
    long long earlyOut     = 0;   // _fileInfo.handle == -1
    long long armCompressed = 0;  // header sign bit set
    long long armPlain      = 0;  // header sign bit clear (the 0x1800b5b25 else)
    long long armV2Block    = 0;  // formatVersion >= 2  (the +0x90 / +0x98 pair)
    long long armMetaUtf16  = 0;  // formatVersion <= 2  PATH A
    long long armMetaRaw    = 0;  // formatVersion >  2  PATH B
    long long armNameUtf16  = 0;  // per entry, formatVersion <= 2
    long long armNameRaw    = 0;  // per entry, formatVersion >  2
    long long entries       = 0;
    long long strNonDet     = 0;  // strings excluded by §95's terminator rule
    long long strTotal      = 0;
    long long commitTaken   = 0;  // §70 says PROVABLY zero; measured, not assumed
} g_census;

bool CensusWanted() {
    static const int on = (RtDiffEnabled() || std::getenv("BANISHED_PKG3_CENSUS")) ? 1 : 0;
    return on != 0;
}

// ---- the String tail every one of the six metadata groups and the entry
//      loop share (0x1800b4fce..0x1800b509d and its five clones) -------------
//
// THREE THINGS A TIDIED-UP VERSION GETS WRONG, all reproduced:
//  1. `strlen(buf)` is computed TWICE, into two different registers, with an
//     allocation in between. They cannot differ here, but the second one is
//     what the copy length is built from and collapsing them is a change to
//     what the code does.
//  2. `CMP ESI,EDI ; CMOVL EAX,ESI` on `EAX = EDI` is `min`, not `max` — the
//     copy length is `min(len + 1, strlen + 1)`.
//  3. The trailing `if (n2 > cap) data[cap-1] = 0` is retail's own truncation
//     guard, and it indexes `cap - 1`, not `n2 - 1`.
void BuildStringFrom(const uint8_t* buf, String* tmp) {
    tmp->data = nullptr;                                   // 1800b4fce

    int32_t n1 = 0;                                        // 1800b4fd3..4fe9
    while (buf[n1] != 0) ++n1;
    RtString_SetLength(tmp, n1);                           // 1800b4ff2  0x1800c8220

    const int32_t cap = n1 + 1;                            // 1800b4ff7  LEA ESI,[RBX+1]
    int32_t n2 = 0;                                        // 1800b4ffa..5019
    while (buf[n2] != 0) ++n2;
    ++n2;                                                  // 1800b501b  INC EDI

    const int32_t copyLen = (cap < n2) ? cap : n2;         // 1800b501d..5021  MIN
    std::memcpy(tmp->data, buf, static_cast<size_t>(copyLen));   // 1800b5033
    if (n2 > cap)                                          // 1800b5038  CMP EDI,ESI / JLE
        tmp->data[cap - 1] = 0;                            // 1800b503f
}

// The inlined `String::operator=` at 0x1800b5044 and its five clones. Retail
// SKIPS the whole thing (including the AddRef) when the pointers are already
// equal, and on the freeing branch leaves `data - 8` in the slot before
// overwriting it — both of which RtString_ReleaseInline already reproduces.
void AssignStringField(String* field, const String* tmp) {
    if (field->data != tmp->data) {                        // 1800b5048  CMP / JZ
        RtString_ReleaseInline(field);                     // 1800b504d..506b
        field->data = tmp->data;                           // 1800b506f
        if (field->data != nullptr)                        // 1800b5073
            ++StringRefCount(field->data);                 // 1800b5078
    }
}

// ---- §95's determinism test ------------------------------------------------
// "Did a terminator appear inside the bytes actually read?" — decided from data
// both sides read identically, so the answer itself cannot diverge.
bool Utf16TerminatorInRead(const uint8_t* buf, int32_t len) {
    for (int32_t i = 0; i + 1 < len; i += 2)
        if (buf[i] == 0 && buf[i + 1] == 0) return true;
    return false;
}
bool RawTerminatorInRead(const uint8_t* buf, int32_t len) {
    for (int32_t i = 0; i < len; ++i)
        if (buf[i] == 0) return true;
    return false;
}

// ---- PATH A's string group (0x1800b4e1f, twice more at 5309/. and 530d) ----
// Read a length, read that many BYTES, then decode `2 * len` of them as
// UTF-16LE and re-encode to UTF-8 into the overlapping output buffer.
// Returns whether the result is deterministic (§95).
bool ReadStringUtf16(CompressedStream* cs, uint8_t* buf60, uint8_t* buf860,
                     String* tmp) {
    int32_t len = 0;                                       // 1800b4e22
    RtIo::RtIo_CompressedStream_Read(cs, 4, &len);         // 1800b4e2e group
    if (len > 0)                                           // 1800b4eba  TEST/JLE
        RtIo::RtIo_CompressedStream_Read(cs, len, buf860); // 1800b4eb3 group

    const bool deterministic = (len <= 0) || Utf16TerminatorInRead(buf860, len);

    // `LEA EAX,[R15+R15*1] ; CDQE` — TWICE what was read. §76.
    const uint8_t* src       = buf860;                     // 1800b4f2a  RDI
    const uint8_t* const end = buf860 + 2 * static_cast<int64_t>(len);  // 1800b4f23/f33
    uint8_t*       dst       = buf60;                      // 1800b4f3d  R14
    int32_t        total     = 0;                          // 1800b4f27  ESI

    while (src < end) {                                    // 1800b4f48/fb5  unsigned CMP
        int32_t cp = -1;                                   // 1800b4f53
        src += Decode1(src, end, &cp);                     // 1800b4f6e / f73
        if (cp >= 0) {                                     // 1800b4f7d  TEST/JS
            uint8_t scratch[4];
            const int32_t m = Encode2(cp, scratch);        // 1800b4f8c
            total += m;                                    // 1800b4f91
            // The cap SKIPS THE COPY and keeps looping — it does not break.
            if (total <= 0x800 && m > 0) {                 // 1800b4f93/9e
                std::memcpy(dst, scratch, static_cast<size_t>(m));   // 1800b4fad
                dst += m;                                  // 1800b4fb2
            }
        }
    }
    if (total + 2 <= 0x800)                                // 1800b4fc1/c4  LEA/CMP/JG
        *dst = 0;                                          // 1800b4fcb

    BuildStringFrom(buf60, tmp);
    return deterministic;
}

// ---- PATH B's string group (0x1800b559f and its two clones) ----------------
// Read a length, read that many bytes STRAIGHT into the output buffer. No
// transcode, and — note — no terminator is written either.
bool ReadStringRaw(CompressedStream* cs, uint8_t* buf60, String* tmp) {
    int32_t len = 0;                                       // 1800b55a2
    RtIo::RtIo_CompressedStream_Read(cs, 4, &len);         // 1800b55ae group
    if (len > 0)                                           // 1800b5633  TEST/JLE
        RtIo::RtIo_CompressedStream_Read(cs, len, buf60);  // 1800b562f group

    const bool deterministic = (len > 0) && RawTerminatorInRead(buf60, len);
    BuildStringFrom(buf60, tmp);
    return deterministic;
}

// ---- the oracle's field-by-field comparison -------------------------------
int32_t ArrSize(const void* obj, size_t off) {
    int32_t v; std::memcpy(&v, static_cast<const uint8_t*>(obj) + off, 4); return v;
}
void* ArrData(const void* obj, size_t off) {
    void* v; std::memcpy(&v, static_cast<const uint8_t*>(obj) + off, 8); return v;
}
bool TextEqual(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return a == b;
    return std::strcmp(a, b) == 0;
}

}  // namespace

// ============================================================================
// ?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z  retail 0x1800b46b0
// ============================================================================
extern "C" bool RtIo_PackageFile_ReadPackfile(PackageFileView* self,
                                              const Filename* name) {
    RT_DIFF_BEGIN("IO::PackageFile::ReadPackfile");
    ++g_census.calls;

    // ---- the re-entrancy flag (0x1800b46e8..0x1800b4795) -------------------
    // `fs->platformExistsGate` (+0x19) is Exists()'s second gate. ReadPackfile
    // forces it to 1 across the OPEN so the file is found on disk rather than
    // through the (still empty) packfile index, then restores the SAVED byte.
    // Retail RE-READS the singleton before restoring; reproduced.
    FileSystem* fs = *FileSystemSingletonCell();               // 1800b46e8
    const uint8_t savedGate = fs->platformExistsGate;          // 1800b46ef
    fs->platformExistsGate = 1;                                // 1800b46f9

    FileInfo* const fi =
        reinterpret_cast<FileInfo*>(reinterpret_cast<uint8_t*>(self) + 0xC0);

    {
        // Deliberately uninitialised: retail hands OpenFile an untouched
        // `[RBP+0x10]` as the hidden return slot and OpenFile writes it.
        FileInfo opened;
        RtIo_FileSystem_OpenFile(fs, &opened, name);           // 1800b4707
        RtIo_FileInfo_Assign(fi, &opened);                     // 1800b471a
        // Retail releases +0x38 (_name1) FIRST, then +0x30 (_name0).
        RtString_ReleaseInline(&opened._name1);                // 1800b472e..4757
        RtString_ReleaseInline(&opened._name0);                // 1800b4762..478b
    }

    fs = *FileSystemSingletonCell();                           // 1800b478b
    fs->platformExistsGate = savedGate;                        // 1800b4792

    if (fi->handle == -1) {                                    // 1800b4795  CMP [RDI],-1
        ++g_census.earlyOut;
        RT_DIFF_WORK(false);
        return false;                                          // 1800b479a  XOR AL,AL
    }

    // ---- the header word, its sign bit, and the seek (§69) -----------------
    int32_t hdr = 0;
    RtIo_FileSystem_ReadFile(fs, fi, 4, &hdr);                 // 1800b47b1
    const int32_t compressed = (hdr & static_cast<int32_t>(0x80000000)) ? 1 : 0;
                                                               // 1800b47b9  BT/CMOVC
    hdr &= 0x7fffffff;                                         // 1800b47ca  BTR
    fi->cursor = hdr;                                          // 1800b47d1  the seek
    // Only when the handle is -1 or -2, i.e. a LOOSE file, is there an OS
    // handle to mirror it into. `CMP dword [RDI],-3 ; JBE` is UNSIGNED.
    if (static_cast<uint32_t>(fi->handle) > 0xFFFFFFFDu) {     // 1800b47d4
        ::SetFilePointer(reinterpret_cast<HANDLE>(
                             static_cast<intptr_t>(fi->winHandle)),
                         hdr, nullptr, FILE_BEGIN);            // 1800b47e3
    }

    // ---- the compressed blob -----------------------------------------------
    int32_t blobLen = 0;
    RtIo_FileSystem_ReadFile(fs, fi, 4, &blobLen);             // 1800b47f9
    void* const blob = MemAlloc(
        static_cast<uint64_t>(static_cast<int64_t>(blobLen)), 4);   // 1800b47fe MOVSXD
    RtIo_FileSystem_ReadFile(fs, fi, blobLen, blob);           // 1800b4820

    // ---- ONE CompressedMemoryStream, built in halves (§48) -----------------
    // Not two objects: the CompressedStream base ctor, then the class's own
    // vtable stamped over it, then the EMBEDDED MemoryStream's ctor at +0x68,
    // then four raw field stores, then Open with the embedded stream as source.
    CompressedMemoryStream cms;
    RtIo::RtIo_CompressedStream_Ctor(&cms.base);               // 1800b482a
    *reinterpret_cast<const void**>(&cms) = VtblCompressedMemoryStream();  // 1800b4837
    RtIo::RtIo_MemoryStream_Ctor(&cms.memoryStream);           // 1800b4840
    cms.memoryStream.data           = blob;                    // 1800b4846
    cms.memoryStream.size   = static_cast<uint32_t>(blobLen);  // 1800b484d
    cms.memoryStream.cursor         = 0;                       // 1800b4850
    cms.memoryStream.base.direction = 0;                       // 1800b4854
    RtIo::RtIo_CompressedStream_Open(&cms.base, &cms.memoryStream.base, 1);  // 1800b4864

    CompressedStream* const cs = &cms.base;

    // The frame's one buffer region, §93.
    uint8_t  frameBuf[kFrameBufBytes];
    uint8_t* const buf60  = frameBuf;                          // [RBP+0x60]
    uint8_t* const buf860 = frameBuf + kOutCap;                // [RBP+0x860]

    int32_t fileCount = 0;
    RtIo::RtIo_CompressedStream_Read(cs, 4, &fileCount);       // 1800b4869 group

    int32_t formatVersion = 0;                                 // 1800b48e9

    if (compressed >= 1) {                                     // 1800b48f0  CMP/JL
        ++g_census.armCompressed;

        RtIo::RtIo_CompressedStream_Read(cs, 4, &formatVersion);      // 1800b48fa
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->version);      // 1800b4979  +0x68
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->userVersion);  // 1800b49eb  +0x6c
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->priority);     // 1800b4a5d  +0x70
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->platform);     // 1800b4ad9  +0x74
        // ONE BYTE — §92(a). `MOV ESI,0x1` at 0x1800b4b4b.
        RtIo::RtIo_CompressedStream_Read(cs, 1, &self->overridable);  // 1800b4b4b  +0xac
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->packfileId);   // 1800b4bc9  +0xa8
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->iconWidth);    // 1800b4c49  +0x78
        RtIo::RtIo_CompressedStream_Read(cs, 4, &self->iconHeight);   // 1800b4cc0  +0x7c

        // The icon pixel array. Retail sizes it from height*width and then
        // reads `array._size * 4` bytes — the array's OWN size, not the product
        // it just passed. Identical unless SetSize clamped; transcribed as
        // spelled.
        uint8_t* const iconArr = reinterpret_cast<uint8_t*>(self) + 0x80;
        ArraySetSize(0x180029510, iconArr,
                     self->iconHeight * self->iconWidth);      // 1800b4d27/d37
        RtIo::RtIo_CompressedStream_Read(
            cs, ArrSize(self, 0x88) * 4, ArrData(self, 0x80)); // 1800b4d56  REAL CALL

        if (formatVersion >= 2) {                              // 1800b4d62  CMP 2 / JL
            ++g_census.armV2Block;
            int32_t* const n90 =
                reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(self) + 0x90);
            RtIo::RtIo_CompressedStream_Read(cs, 4, n90);      // 1800b4d6c  +0x90
            uint8_t* const flagArr = reinterpret_cast<uint8_t*>(self) + 0x98;
            ArraySetSize(0x180029630, flagArr, *n90);          // 1800b4dea/df1
            // NOTE the bound: retail reads `*n90` bytes, not the ARRAY's size.
            RtIo::RtIo_CompressedStream_Read(cs, *n90, ArrData(self, 0x98));  // 1800b4e09
        }

        String tmp;
        if (formatVersion <= 2) {
            // ---- PATH A (§81): UTF-16 metadata, with the §76 over-read -----
            ++g_census.armMetaUtf16;
            bool d;
            d = ReadStringUtf16(cs, buf60, buf860, &tmp);      // 1800b4e1f  group 1
            AssignStringField(&self->name, &tmp);              // 1800b5044  +0x40
            RtString_ReleaseInline(&tmp);                      // 1800b507b
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;

            d = ReadStringUtf16(cs, buf60, buf860, &tmp);      // 1800b530d  group 2
            AssignStringField(&self->author, &tmp);            // 1800b52b4  +0x48
            RtString_ReleaseInline(&tmp);
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;

            d = ReadStringUtf16(cs, buf60, buf860, &tmp);      // group 3
            AssignStringField(&self->description, &tmp);       // 1800b5536  +0x50
            RtString_ReleaseInline(&tmp);
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;
            // 1800b558e / 559a  JMP 0x1800b5b6c  (reloads formatVersion; §92(c))
        } else {
            // ---- PATH B (§81): the bytes are already UTF-8 -----------------
            ++g_census.armMetaRaw;
            bool d;
            d = ReadStringRaw(cs, buf60, &tmp);                // 1800b559f  group 4
            AssignStringField(&self->name, &tmp);              // 1800b5726  +0x40
            RtString_ReleaseInline(&tmp);
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;

            d = ReadStringRaw(cs, buf60, &tmp);                // 1800b5783  group 5
            AssignStringField(&self->author, &tmp);            // 1800b58f6  +0x48
            RtString_ReleaseInline(&tmp);
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;

            d = ReadStringRaw(cs, buf60, &tmp);                // 1800b5953  group 6
            AssignStringField(&self->description, &tmp);       // 1800b5ac6  +0x50
            RtString_ReleaseInline(&tmp);
            ++g_census.strTotal; if (!d) ++g_census.strNonDet;
            // 1800b5b1a / 5b23  JMP 0x1800b5b73
        }
    } else {
        // ---- the uncompressed default, 0x1800b5b25. §92(b): SEVEN stores. --
        ++g_census.armPlain;
        self->version = 0x010007ff;                            // 1800b5b25  dword
        // `MOV qword ptr [R13+0x70],0x2710` — one store, two fields.
        self->priority = 0x2710;                               // 1800b5b2d
        self->platform = 0;
        self->packfileId  = 0;                                 // 1800b5b35
        self->overridable = 1;                                 // 1800b5b3c
        self->iconWidth  = 1;                                  // 1800b5b49
        self->iconHeight = 1;                                  // 1800b5b4d
        uint8_t* const iconArr = reinterpret_cast<uint8_t*>(self) + 0x80;
        ArraySetSize(0x180029510, iconArr, 1);                 // 1800b5b5a
        *static_cast<uint32_t*>(ArrData(self, 0x80)) = 0xff000000u;   // 1800b5b66
        // falls into 0x1800b5b6c, which reloads formatVersion — still 0 here.
    }

    // ---- the entry directory (0x1800b5b73) ---------------------------------
    if (fileCount > 0) {                                       // 1800b5b73  TEST/JLE
        uint8_t* const entriesArr = reinterpret_cast<uint8_t*>(self) + 0xb0;  // 1800b5b86

        for (int32_t i = fileCount; i != 0; --i) {             // 1800b5b99 / 601a
            ++g_census.entries;

            int32_t nameLen = 0;
            RtIo::RtIo_CompressedStream_Read(cs, 4, &nameLen); // 1800b5ba0 group

            // THE SAME SELECTOR AS THE METADATA BLOCK, WITH THE ARMS IN THE
            // OPPOSITE ORDER — raw first at 0x1800b5c1f, jumping over the
            // UTF-16 arm at 0x1800b5c9d. §86.
            bool nameDeterministic;
            if (formatVersion > 2) {                           // 1800b5c19  CMP 2 / JLE
                ++g_census.armNameRaw;
                if (nameLen > 0)                               // 1800b5c29
                    RtIo::RtIo_CompressedStream_Read(cs, nameLen, buf60);  // 1800b5c25
                nameDeterministic = (nameLen > 0) && RawTerminatorInRead(buf60, nameLen);
                // 1800b5c98  JMP 0x1800b5de3 — NO terminator is written here.
            } else {
                ++g_census.armNameUtf16;
                if (nameLen > 0)                               // 1800b5cae
                    RtIo::RtIo_CompressedStream_Read(cs, nameLen, buf860); // 1800b5ca7
                nameDeterministic =
                    (nameLen <= 0) || Utf16TerminatorInRead(buf860, nameLen);

                const uint8_t* src       = buf860;             // 1800b5d22
                const uint8_t* const end =
                    buf860 + 2 * static_cast<int64_t>(nameLen);// 1800b5d1c/d32
                uint8_t*       dst       = buf60;              // 1800b5d35
                int32_t        total     = 0;                  // 1800b5d20
                while (src < end) {                            // 1800b5d40/db8
                    int32_t cp = -1;                           // 1800b5d50
                    src += Decode1(src, end, &cp);             // 1800b5d6f/74
                    if (cp >= 0) {                             // 1800b5d80
                        uint8_t scratch[4];
                        const int32_t m = Encode2(cp, scratch);// 1800b5d8f
                        total += m;                            // 1800b5d94
                        if (total <= 0x800 && m > 0) {         // 1800b5d96/da1
                            std::memcpy(dst, scratch, static_cast<size_t>(m));  // 1800b5db0
                            dst += m;                          // 1800b5db5
                        }
                    }
                }
                // 1800b5dc5 `MOV byte ptr [RSP+0x38],0` zeroes the encode
                // scratch on the way out. A dead store on a slot nothing reads
                // again; recorded, not reproduced, because our scratch is a
                // block-scoped local that does not exist here.
                if (total + 2 <= 0x800)                        // 1800b5dca/d4
                    *dst = 0;                                  // 1800b5ddb
            }

            int32_t entryOffset = 0, entrySize = 0;
            RtIo::RtIo_CompressedStream_Read(cs, 4, &entryOffset);  // 1800b5de3  -> +0x08
            RtIo::RtIo_CompressedStream_Read(cs, 4, &entrySize);    // 1800b5e59  -> +0x0c

            String tmp;
            BuildStringFrom(buf60, &tmp);                      // 1800b5ec6..5f3d
            RtSystem_Filename_AdjustDelimiters(
                reinterpret_cast<Filename*>(&tmp));            // 1800b5f42

            PackageFileEntry entry;
            entry.name.data = nullptr;                         // 1800b5f4a
            AssignStringField(&entry.name, &tmp);              // 1800b5f4e..5f8e
            // TWICE. 0x1800b5f95 and 0x1800b5f9e, back to back, on the same
            // object. Idempotent, and reproduced rather than folded.
            RtSystem_Filename_AdjustDelimiters(
                reinterpret_cast<Filename*>(&entry.name));     // 1800b5f95
            RtSystem_Filename_AdjustDelimiters(
                reinterpret_cast<Filename*>(&entry.name));     // 1800b5f9e

            entry.offset = static_cast<uint32_t>(entryOffset); // 1800b5fa4  §91
            entry.size   = static_cast<uint32_t>(entrySize);   // 1800b5faa  §91

            ArraySetSizeFill(0x1800298f0, entriesArr,
                             ArrSize(self, 0xb8) + 1, &entry); // 1800b5fb1..fbe

            RtString_ReleaseInline(&entry.name);               // 1800b5fc4..5fe9
            RtString_ReleaseInline(&tmp);                      // 1800b5fed..6015

            ++g_census.strTotal;
            if (!nameDeterministic) ++g_census.strNonDet;
        }
    }

    // ---- teardown (0x1800b6023) --------------------------------------------
    // §70 proves this Commit is unreachable on the read path: `?Open@` sets
    // direction = 0 and only direction == 1 gets here. The GUARD is
    // transcribed and the arm is COUNTED, so "provably dead" is measured on
    // every run rather than asserted once.
    if (cs->headerSize > 0 && cs->base.direction == 1) {       // 1800b6023/29
        ++g_census.commitTaken;
        RT_EXPORT_ONCE(VoidThisFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(cs);  // 1800b6035
    }
    CallStreamClose(&cms.memoryStream);                        // 1800b603a  slot 7
    MemFree(blob);                                             // 1800b6045
    RtIo::RtIo_CompressedMemoryStream_Dtor(&cms);              // 1800b604f
    const bool ours = true;                                    // 1800b6059  MOV AL,1

    // ========================================================================
    // The oracle, §95
    // ========================================================================
    if (rt_diff_on__) {
        alignas(16) uint8_t scratch[0xF0];
        std::memset(scratch, 0xCD, sizeof scratch);
        PackageFileView* const t = reinterpret_cast<PackageFileView*>(scratch);
        RtIo_PackageFile_Ctor(t);                              // our proven ctor

        const bool theirs =
            RT_ORIGINAL_ONCE(ReadPackfileFn, kReadPackfile)(t, name);

        char d[192];
        d[0] = '\0';
        bool eq = (ours == theirs);
        if (!eq) std::snprintf(d, sizeof d, "return %d vs %d", (int)ours, (int)theirs);

#define PKG3_CMP(FIELD)                                                        \
        do {                                                                   \
            if (eq && self->FIELD != t->FIELD) {                               \
                eq = false;                                                    \
                std::snprintf(d, sizeof d, #FIELD " %lld vs %lld",             \
                              (long long)self->FIELD, (long long)t->FIELD);    \
            }                                                                  \
        } while (0)
        PKG3_CMP(version);      PKG3_CMP(userVersion);
        PKG3_CMP(priority);     PKG3_CMP(platform);
        PKG3_CMP(iconWidth);    PKG3_CMP(iconHeight);
        PKG3_CMP(packfileId);   PKG3_CMP(overridable);
        PKG3_CMP(entryCount);
#undef PKG3_CMP

        // +0x90 and the two arrays. Compared by SIZE and by CONTENT — the
        // content is what a size-only check would let through, and the icon
        // array is the one field big enough for a transcription slip to hide in.
        auto cmpArr = [&](size_t sizeOff, size_t dataOff, int32_t elem,
                          const char* what) {
            if (!eq) return;
            const int32_t a = ArrSize(self, sizeOff), b = ArrSize(t, sizeOff);
            if (a != b) {
                eq = false;
                std::snprintf(d, sizeof d, "%s size %d vs %d", what, a, b);
                return;
            }
            if (a > 0 && std::memcmp(ArrData(self, dataOff), ArrData(t, dataOff),
                                     static_cast<size_t>(a) * elem) != 0) {
                eq = false;
                std::snprintf(d, sizeof d, "%s content differs (%d elems)", what, a);
            }
        };
        // *** +0x90 IS ONLY COMPARABLE WHEN IT WAS WRITTEN. *** The write above
        // is guarded on `formatVersion >= 2` (0x1800b4d62, CMP 2 / JL), so for a
        // format-1 packfile NEITHER body touches this field: `t` still holds the
        // 0xCD scratch fill and `self` holds whatever was on the stack. Comparing
        // it anyway reported 2-3 DIVERGENCEs per run the moment lane MOD's
        // RefreshList went live and put a PackageFile on the stack — `theirs`
        // constantly -842150451 (0xCDCDCDCD) against an `ours` that differed
        // every run, which is the signature of comparing bytes neither side
        // wrote (reference_oracle_must_not_compare_unwritten_bytes.md).
        //
        // The port was never wrong. §95 already makes exactly this exclusion for
        // `_fileInfo._unk04`; it was applied to one unwritten field and not the
        // other. Guarded on the SAME condition as the write rather than on a
        // value test, so a format-2 packfile that legitimately writes 0 is still
        // compared. Found by lane MOD, which reported it rather than editing
        // another lane's file mid-wave.
        if (eq && formatVersion >= 2) {
            int32_t a90 = ArrSize(self, 0x90), b90 = ArrSize(t, 0x90);
            if (a90 != b90) {
                eq = false;
                std::snprintf(d, sizeof d, "+0x90 %d vs %d", a90, b90);
            }
        }
        cmpArr(0x88, 0x80, 4, "icon[+0x80]");
        cmpArr(0xa0, 0x98, 1, "flags[+0x98]");

        // The FileInfo. winHandle EXCLUDED (two CreateFileW calls, two
        // handles), +0x04 EXCLUDED (never written by OpenFile), the two
        // Filenames compared by TEXT.
        const FileInfo* const tf =
            reinterpret_cast<const FileInfo*>(scratch + 0xC0);
        if (eq && (fi->handle != tf->handle || fi->size != tf->size ||
                   fi->cursor != tf->cursor || fi->baseOffset != tf->baseOffset)) {
            eq = false;
            std::snprintf(d, sizeof d, "FileInfo %d/%d/%d/%d vs %d/%d/%d/%d",
                          fi->handle, fi->size, fi->cursor, fi->baseOffset,
                          tf->handle, tf->size, tf->cursor, tf->baseOffset);
        }
        if (eq && ((fi->winHandle == -1) != (tf->winHandle == -1))) {
            eq = false;
            std::snprintf(d, sizeof d, "winHandle validity differs");
        }
        if (eq && (!TextEqual(fi->_name0.data, tf->_name0.data) ||
                   !TextEqual(fi->_name1.data, tf->_name1.data))) {
            eq = false;
            std::snprintf(d, sizeof d, "FileInfo name text differs");
        }

        // The three metadata Strings and every entry name, by TEXT — but only
        // where §95 says the answer is determined by bytes that were read.
        // `g_strNonDet` is the count of the ones this run had to skip, and it
        // is printed whether it is zero or not.
        const bool metaDet = (g_census.strNonDet == 0);
        if (eq && metaDet) {
            if (!TextEqual(self->name.data, t->name.data) ||
                !TextEqual(self->author.data, t->author.data) ||
                !TextEqual(self->description.data, t->description.data)) {
                eq = false;
                std::snprintf(d, sizeof d, "metadata text: \"%.40s\"/\"%.40s\"/\"%.40s\"",
                              self->name.data ? self->name.data : "(null)",
                              self->author.data ? self->author.data : "(null)",
                              self->description.data ? self->description.data : "(null)");
            }
        }
        if (eq && self->entryCount == t->entryCount && self->entries && t->entries) {
            const PackageFileEntry* ea =
                static_cast<const PackageFileEntry*>(self->entries);
            const PackageFileEntry* eb =
                static_cast<const PackageFileEntry*>(t->entries);
            for (int32_t k = 0; k < self->entryCount; ++k) {
                if (ea[k].offset != eb[k].offset || ea[k].size != eb[k].size) {
                    eq = false;
                    std::snprintf(d, sizeof d, "entry %d %u/%u vs %u/%u", k,
                                  ea[k].offset, ea[k].size, eb[k].offset, eb[k].size);
                    break;
                }
                if (metaDet && !TextEqual(ea[k].name.data, eb[k].name.data)) {
                    eq = false;
                    std::snprintf(d, sizeof d, "entry %d name \"%.60s\" vs \"%.60s\"", k,
                                  ea[k].name.data ? ea[k].name.data : "(null)",
                                  eb[k].name.data ? eb[k].name.data : "(null)");
                    break;
                }
            }
        }

        RT_DIFF_SHADOW(eq, eq ? "all compared fields agree" : d,
                       eq ? "all compared fields agree" : "shipped ReadPackfile");
        RT_DIFF_WORK(true);

        // ---- tear the scratch down ----------------------------------------
        // The shipped body leaves the scratch owning three Strings, one String
        // per entry, three array blocks and — for a loose file — its OWN
        // CreateFileW handle. ~PackageFile is unported (package_file.cpp §4),
        // so the teardown is written out here rather than leaked: a diff run
        // mounts every package in the install and this would otherwise hold an
        // extra handle on each for the life of the process.
        RtString_ReleaseInline(&t->name);
        RtString_ReleaseInline(&t->author);
        RtString_ReleaseInline(&t->description);
        if (t->entries) {
            PackageFileEntry* te = static_cast<PackageFileEntry*>(t->entries);
            for (int32_t k = 0; k < t->entryCount; ++k)
                RtString_ReleaseInline(&te[k].name);
            MemFree(t->entries);
        }
        if (ArrData(t, 0x80)) MemFree(ArrData(t, 0x80));
        if (ArrData(t, 0x98)) MemFree(ArrData(t, 0x98));
        FileInfo* const tfi = reinterpret_cast<FileInfo*>(scratch + 0xC0);
        RtString_ReleaseInline(&tfi->_name0);
        RtString_ReleaseInline(&tfi->_name1);
        // ONLY the loose case: handle -2 means PlatformOpenFile did its own
        // CreateFileW and this HANDLE is the scratch's alone. A packed file's
        // winHandle belongs to the owning .pkg and closing it would shut down
        // the live FileSystem.
        if (tfi->handle == -2 && tfi->winHandle != -1)
            ::CloseHandle(reinterpret_cast<HANDLE>(
                static_cast<intptr_t>(tfi->winHandle)));
    } else {
        RT_DIFF_WORK(true);
    }

    if (CensusWanted()) {
        printf("[PKG3] ReadPackfile \"%.60s\" -> %d | calls=%lld early=%lld "
                    "| compressed=%lld plain=%lld | v2block=%lld "
                    "| meta: utf16=%lld raw=%lld | names: utf16=%lld raw=%lld "
                    "| entries=%lld | strings=%lld nondet=%lld | commit=%lld\n",
                    name && name->data ? name->data : "(null)", (int)ours,
                    g_census.calls, g_census.earlyOut,
                    g_census.armCompressed, g_census.armPlain, g_census.armV2Block,
                    g_census.armMetaUtf16, g_census.armMetaRaw,
                    g_census.armNameUtf16, g_census.armNameRaw,
                    g_census.entries, g_census.strTotal, g_census.strNonDet,
                    g_census.commitTaken);
    }
    return ours;
}

}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
// BOTH DOORS, as every row in this lane gets. The name was read out of
// Runtime-x64.dll's own export directory with pefile (ordinal 3461,
// 0x1800b46b0) rather than transcribed from a decompile — lane PKG2 nearly
// shipped `SetWindowCount` with the private `AEAA` spelling when it is public
// `QEAA`, and a name the DLL does not contain resolves NULL silently and
// forever behind the ResolveAddr fallback.
int RtIoPackageFileReadRegister() {
    using namespace RtIO;
    int n = 0;

    // THE NAME IS SPELT OUT AS A LITERAL HERE, not as `kReadPackfile`, and that
    // is not style. `tools/unsatisfied_export_scan.py` and
    // `tools/runtime_census.py` match `RegisterRuntimeExport("` followed by a
    // STRING; a named constant is invisible to both
    // (reference_census_cannot_see_nonliteral_registrations.md, which cost six
    // LANDED Wave 3 ports their visibility). The first version of this line used
    // the constant and the scan duly reported ReadPackfile as still unsatisfied
    // on a build where it was registered and running -- caught by the
    // instrument, not by review, which is this wave's pattern.
    RegisterRuntimeExport("?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z",
                          (void*)&RtIo_PackageFile_ReadPackfile); ++n;
    RegisterRuntimeAddr(0x1800b46b0, (void*)&RtIo_PackageFile_ReadPackfile);

    // A LANE-OWNED IDENTITY MARKER, for the same reason package_file.cpp has
    // one: tools/pkg2_run.sh gates on a banner the RUN wrote, and a banner that
    // keys on a shared total moves whenever any other lane lands in IO.
    printf("[RtPort]   IO::PackageFile::ReadPackfile (lane PKG3): "
                "%d exports registered\n", n);
    return n;
}
