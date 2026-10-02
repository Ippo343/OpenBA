// ============================================================================
// runtime/src/system/widestring.cpp — Wave 9 lane FS: System::WideString
// ============================================================================
//
//   | Export                                          | Retail      | Ord  |
//   |-------------------------------------------------|-------------|------|
//   | ?SetLength@WideString@System@@AEAAXH@Z          | 0x1800ca9b0 | 3902 |
//   | ?Set@WideString@System@@QEAAXPEB_W@Z            | 0x1800caf40 | 3823 |
//   | ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ   | 0x1800cacd0 | 1625 |
//   | ??0WideString@System@@QEAA@AEBVString@1@@Z      | 0x1800cab50 |  513 |
//
// Every one of those four names was read out of Runtime-x64.dll's export
// directory with pefile BEFORE a line of body was written. `SetLength` is
// PRIVATE and therefore AEAA, not QEAA — the QEAA spelling is not an export
// and would resolve NULL forever
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md, and the same trap
// Filename::AdjustDelimiters fell into in Wave 2).
//
// WHY THIS CLASS, AND WHY NOW
// ---------------------------
// PLAN §5 recorded WideString as "not called by our tree" and Wave 2 left it
// unported on that basis. That has been stale since Wave 4 lane D:
// `runtime/src/io/filesystem_win.cpp` (FileSystem::GetSaveDirectory) already
// calls `Set` and `AsUTF8` through RT_EXPORT_ONCE and has had NO port behind
// either door since the day it landed — two null calls at cutover on the save
// path. Wave 9 lane FS needs the same two for `FileSystem::GetRootDirectory`
// plus the `String` constructor for `FileSystem::SetupDirectories`. Porting
// them here closes lane D's pre-existing hole as a side effect; nothing in
// filesystem_win.cpp changes, because it already asks through the port-aware
// door.
//
// THE OBJECT
// ----------
// WideString is System::String with wchar_t characters. ONE pointer; the
// allocation header in front of `data` is byte-identical to String's:
//
//     block + 0 : int32 length        (CHARACTER count, not bytes)
//     block + 4 : int32 refcount      (born at 1)
//     block + 8 : wchar_t data[length + 1]
//     WideString::data == block + 8
//
// pinned by SetLength (0x1800ca9b0): `MemAlloc(8 + (len+1)*2, 4)`, then
// `data[-8] = len` and `data[-4] = 1`. The RELEASE idiom is the same one
// String.h documents, including the asymmetry (freeing branch leaves
// `data - 8` in the object, sharing branch nulls it) — retail's
// `ADD qword [RCX],-8 ; MOV RCX,[RCX] ; MemFree` versus `MOV qword [RCX],0`.
// So this file reuses `RtString_ReleaseInline` through a pointer cast instead
// of re-deriving a WideString-specific copy, exactly as filesystem_win.cpp
// argued in Wave 4.
//
// THE ALLOCATOR
// -------------
// Retail's SetLength and AsUTF8 do NOT call ?MemAlloc@MemoryAllocator@; they
// inline it — `EnterCriticalSection(_singleton+0x640); Allocate(_singleton,
// n, 4); LeaveCriticalSection`. `RtMemAlloc`/`RtMemFree` in string.cpp ARE
// that sequence's exported wrapper (0x1800c7a50 / 0x1800c7ad0) and string.cpp
// §"The DLL's allocator" already establishes the two are interchangeable and
// that going through the export keeps standalone/allocator_lock.cpp's detour
// in the path. Using them here also inherits the cutover fallback string.cpp
// added in lane IOA: when the raw door yields null (no DLL) they fall through
// to the registered MemoryAllocator port, so WideString work is sound with
// Runtime-x64.dll absent. THIS LANE RELIES ON THAT FALLBACK.
//
// THE ONE PIECE THAT IS *NOT* TRANSCRIBED FROM RETAIL
// ---------------------------------------------------
// The conversions go through two unexported CRT-style dispatchers:
//
//     0x1800c9fc0  decode(encodingId, src, srcEnd, &codepoint, encodingId)
//                      -> bytes consumed; codepoint < 0 means "invalid"
//     0x1800c9b60  encode(codepoint, dst, encodingId)
//                      -> bytes written
//
// `0x1800c9b60` is literally six instructions and a jump table at
// 0x1800c9d40 indexed by the encoding id, so the ids are readable off the
// table: **2 = UTF-8, 4 = UTF-16LE** (AsUTF8 decodes with 4 and encodes with
// 2; the String constructor decodes with 2 and encodes with 4). Ghidra cannot
// recover the table's bodies as functions ("Too many branches"), which is the
// same wall Wave 2 lane A hit on the SAME PAIR when it ported
// String::Serialize — see runtime/src/system/string.cpp's Utf16DecodeOne /
// Utf8EncodeOne and its identical disclosure. So the four codec helpers below
// are written FROM THE SPECIFICATION, not lifted from the DLL, and that is
// stated here rather than buried.
//
// What was checked in the disassembly rather than assumed: the UTF-16 encoder
// at table entry 4 (0x1800c9c58) rejects a lone surrogate (`LEA EAX,[R9 -
// 0xD800] ; CMP EAX,0x7FF ; JBE <error>`) and returns 4 for a supplementary
// codepoint after `SUB R9D,0x10000`, i.e. ordinary surrogate-pair encoding.
//
// This matters less than it reads. On this project's only caller chain the
// input is the process's own module path and the strings that come out of it,
// which are ASCII on every machine this has ever run on; every codepoint takes
// the one-byte UTF-8 branch and the two-byte UTF-16 branch. The oracles below
// compare against the shipped bodies on exactly the data the boot produces.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>          // placement new into the retail hidden return slot

#include "../../types/system/String.h"
#include "string.h"
#include "pod_bridge.h"
#include "widestring.h"

namespace RtSystem {

namespace {

// ---------------------------------------------------------------------------
// The codec pair, ids 2 (UTF-8) and 4 (UTF-16LE). See the header note.
// ---------------------------------------------------------------------------
// Contract copied from the call sites, not invented: `*cp` is pre-set to -1 by
// the caller and a decoder that cannot make a codepoint leaves it negative and
// still reports a nonzero consumption, so the loops always advance.

int Utf8DecodeOne(const uint8_t* p, const uint8_t* end, int32_t* cp) {
    const uint32_t b0 = p[0];
    if (b0 < 0x80) { *cp = static_cast<int32_t>(b0); return 1; }
    int n;
    uint32_t v;
    if      ((b0 & 0xE0) == 0xC0) { n = 2; v = b0 & 0x1F; }
    else if ((b0 & 0xF0) == 0xE0) { n = 3; v = b0 & 0x0F; }
    else if ((b0 & 0xF8) == 0xF0) { n = 4; v = b0 & 0x07; }
    else                          { *cp = -1; return 1; }   // stray continuation
    if (p + n > end) { *cp = -1; return 1; }
    for (int i = 1; i < n; ++i) {
        const uint32_t b = p[i];
        if ((b & 0xC0) != 0x80) { *cp = -1; return 1; }
        v = (v << 6) | (b & 0x3F);
    }
    *cp = static_cast<int32_t>(v);
    return n;
}

int Utf16DecodeOne(const uint8_t* p, const uint8_t* end, int32_t* cp) {
    if (p + 2 > end) { *cp = -1; return 2; }
    const uint32_t w = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8);
    if (w >= 0xD800 && w < 0xDC00) {
        if (p + 4 > end) { *cp = -1; return 2; }
        const uint32_t w2 =
            static_cast<uint32_t>(p[2]) | (static_cast<uint32_t>(p[3]) << 8);
        if (w2 < 0xDC00 || w2 >= 0xE000) { *cp = -1; return 2; }
        *cp = static_cast<int32_t>(0x10000u + (((w - 0xD800u) << 10) | (w2 - 0xDC00u)));
        return 4;
    }
    if (w >= 0xDC00 && w < 0xE000) { *cp = -1; return 2; }   // lone low surrogate
    *cp = static_cast<int32_t>(w);
    return 2;
}

int Utf8EncodeOne(int32_t codepoint, uint8_t* dst) {
    const uint32_t c = static_cast<uint32_t>(codepoint);
    if (c < 0x80)    { dst[0] = static_cast<uint8_t>(c); return 1; }
    if (c < 0x800)   { dst[0] = static_cast<uint8_t>(0xC0 | (c >> 6));
                       dst[1] = static_cast<uint8_t>(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { dst[0] = static_cast<uint8_t>(0xE0 | (c >> 12));
                       dst[1] = static_cast<uint8_t>(0x80 | ((c >> 6) & 0x3F));
                       dst[2] = static_cast<uint8_t>(0x80 | (c & 0x3F));  return 3; }
    dst[0] = static_cast<uint8_t>(0xF0 | (c >> 18));
    dst[1] = static_cast<uint8_t>(0x80 | ((c >> 12) & 0x3F));
    dst[2] = static_cast<uint8_t>(0x80 | ((c >> 6) & 0x3F));
    dst[3] = static_cast<uint8_t>(0x80 | (c & 0x3F));
    return 4;
}

// Table entry 4 at 0x1800c9c58: a lone surrogate is an ERROR (0 written), a
// supplementary codepoint becomes a surrogate pair (4 bytes), everything else
// is one code unit (2 bytes).
int Utf16EncodeOne(int32_t codepoint, uint8_t* dst) {
    const uint32_t c = static_cast<uint32_t>(codepoint);
    if (c - 0xD800u <= 0x7FFu) return 0;             // `CMP EAX,0x7FF ; JBE error`
    if (c < 0x10000u) {
        dst[0] = static_cast<uint8_t>(c & 0xFF);
        dst[1] = static_cast<uint8_t>((c >> 8) & 0xFF);
        return 2;
    }
    const uint32_t v  = c - 0x10000u;
    const uint32_t hi = 0xD800u | (v >> 10);
    const uint32_t lo = 0xDC00u | (v & 0x3FFu);
    dst[0] = static_cast<uint8_t>(hi & 0xFF);
    dst[1] = static_cast<uint8_t>((hi >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>(lo & 0xFF);
    dst[3] = static_cast<uint8_t>((lo >> 8) & 0xFF);
    return 4;
}

// The release idiom, over the SAME header String uses. Cast, do not re-derive.
// ENDGAME B.3.4b SYSTEM (4a): the parameter is the CLASS now — every one of
// the three callers is a `System::WideString` member body. The cast itself
// stays, and stays here: `System::String` and `System::WideString` are the
// same eight bytes with the same allocation header, which is the fact this
// whole file is built on, and `RtString_ReleaseInline` is String's own
// primitive.
inline void WideReleaseInline(System::WideString* self) {
    RtString_ReleaseInline(reinterpret_cast<String*>(self));  // LEFT: POD String / Filename bridge (H10)
}

inline int WideLength(const wchar_t* d) {
    return d ? reinterpret_cast<const int32_t*>(d)[-2] : 0;  // CAST-OK: string_header
}

int WideStrLen(const wchar_t* s) {
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

bool SameWide(const wchar_t* a, const wchar_t* b) {
    if ((a == nullptr) != (b == nullptr)) return false;
    if (a == nullptr) return true;
    if (WideLength(a) != WideLength(b)) return false;
    for (int i = 0; a[i] != 0 || b[i] != 0; ++i)
        if (a[i] != b[i]) return false;
    return true;
}

bool SameText(const char* a, const char* b) {
    if ((a == nullptr) != (b == nullptr)) return false;
    if (a == nullptr) return true;
    return std::strcmp(a, b) == 0;
}

}  // namespace

// ============================================================================
// ?SetLength@WideString@System@@AEAAXH@Z                   retail 0x1800ca9b0
// ============================================================================

// ============================================================================
// ?Set@WideString@System@@QEAAXPEB_W@Z                     retail 0x1800caf40
// ============================================================================
// Three things in this body are not obvious and all three are retail's:
//
//  1. `Set(nullptr)` is a RELEASE, not "become empty" — retail tail-jumps
//     straight into MemFree (`JMP 0x1800c7ad0` at 0x1800caf84).
//  2. There is an inlined wcscmp FIRST and an equal string is a NO-OP, so
//     `Set` on an unchanged value does not reallocate and does not disturb the
//     refcount. Anything sharing the buffer keeps sharing it.
//  3. After SetLength, retail RE-WALKS the source for its length instead of
//     reusing the one it just computed, copies `min(len2+1, len+1)` characters
//     and then re-terminates. The two are equal by construction; transcribed
//     as written rather than folded, because folding it would silently change
//     what happens if a caller ever mutated `src` between the two walks.
void RtSystem_WideString_Set(WideString* self, const wchar_t* src) {
    Sys(self)->Set(src);
}

// ============================================================================
// ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ            retail 0x1800cacd0
// ============================================================================
// Two passes over the UTF-16 buffer: the first only counts the UTF-8 bytes,
// the second writes them. Between them retail allocates `total + 1` RAW bytes
// (NOT a String block) and NUL-terminates at [total]; at the end it hands the
// raw buffer to String::Set, which makes the real String, and frees it.
//
// The three guards inside the write loop are retail's and are kept verbatim:
// `written > total` (the second pass measured more than the first — impossible
// for a stable input, and a silent truncation if it ever happened), a null
// buffer, and a non-positive encode result. The last one is the one that
// matters: a lone surrogate encodes to ZERO bytes, so a malformed UTF-16
// buffer loses that character rather than corrupting the output.
// ENDGAME B.3.4b SYSTEM (4a): THIS DOOR IS A HIDDEN-RETSLOT DOOR. `ret` is the
// caller's RAW BYTES — an uninitialised `RtSystem::String` local — so the
// member's prvalue is placement-CONSTRUCTED into it and never assigned to; an
// assignment would Release() whatever garbage pointer the caller's stack
// happened to hold. Same shape as `string.cpp`'s `RtSystem_String_SubString`.
String* RtSystem_WideString_AsUTF8(const WideString* self, String* ret) {
    new (Sys(ret)) System::String(Sys(self)->AsUTF8());
    return ret;
}

}  // namespace RtSystem

// ============================================================================
// ENDGAME B.3.4b SYSTEM (4a) — the `System::WideString` member BODIES.
// ============================================================================
// Defined at GLOBAL scope with qualified names rather than inside a
// `namespace System { ... }` block: rule B4b-5, because `RtSystem::WideString`
// and `System::WideString` are layout-identical and a bare `WideString` inside
// such a block would bind the class silently. Every body below is the one that
// used to sit in the door of the same name above, moved VERBATIM — the only
// edits are `self->data` becoming `_data`, the two internal calls becoming
// member calls, and the anonymous-namespace helpers picking up their `RtSystem::`
// qualification (reachable from here in the same TU, B34_BRIEFS §A.2).
// ============================================================================

void System::WideString::SetLength(int len) {
    RtSystem::WideReleaseInline(this);
    int32_t* block = static_cast<int32_t*>(RtSystem::RtMemAlloc(  // LEFT: an alloc result; the tier-I shape does not match the RtMemAlloc spelling
        static_cast<uint64_t>(8 + (static_cast<int64_t>(len) + 1) * 2),
        RtSystem::kStringAllocAlign));
    _data = reinterpret_cast<wchar_t*>(block + 2);  // CAST-OK: string_header
    block[0] = len;
    block[1] = 1;
}

void System::WideString::Set(const wchar_t* src) {

    // Snapshot BEFORE we touch `this`: the oracle below replays onto a scratch
    // object, and a replay whose input our own write has already destroyed is
    // how a correct port reads as a 10% divergence
    // (reference_oracle_must_snapshot_aliased_inputs.md).
    wchar_t* const before = _data;

    if (src == nullptr) {
        RtSystem::WideReleaseInline(this);
    } else {
        const wchar_t* cur = _data;
        bool unchanged = false;
        if (cur != nullptr) {
            int i = 0;
            if (cur[0] == src[0]) {
                // `TEST CX,CX / CMP word[src+i],0 / INC i / CMP` — stop at
                // either terminator or at the first difference.
                for (;;) {
                    if (cur[i] == 0) break;
                    if (src[i] == 0) break;
                    ++i;
                    if (cur[i] != src[i]) break;
                }
            }
            unchanged = (cur[i] == src[i]);
        }
        if (!unchanged) {
            const int len = RtSystem::WideStrLen(src);
            SetLength(len);
            wchar_t* dst = _data;

            const int n1   = len + 1;                 // R14D
            const int len2 = RtSystem::WideStrLen(src) + 1;  // EBX, retail's re-walk
            const int copy = (n1 < len2) ? n1 : len2; // CMOVL — the MINIMUM
            std::memcpy(dst, src, static_cast<size_t>(copy) * 2);
            if (len2 >= n1) dst[n1 - 1] = 0;
        }
    }

}

// ============================================================================
// ?AsUTF8@WideString@System@@QEBA?AVString@2@XZ            retail 0x1800cacd0
// ============================================================================
// Two passes over the UTF-16 buffer: the first only counts the UTF-8 bytes,
// the second writes them. Between them retail allocates `total + 1` RAW bytes
// (NOT a String block) and NUL-terminates at [total]; at the end it hands the
// raw buffer to String::Set, which makes the real String, and frees it.
//
// The three guards inside the write loop are retail's and are kept verbatim:
// `written > total` (the second pass measured more than the first — impossible
// for a stable input, and a silent truncation if it ever happened), a null
// buffer, and a non-positive encode result. The last one is the one that
// matters: a lone surrogate encodes to ZERO bytes, so a malformed UTF-16
// buffer loses that character rather than corrupting the output.
System::String System::WideString::AsUTF8() const {

    const uint8_t* p   = reinterpret_cast<const uint8_t*>(_data);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* end = p + static_cast<size_t>(RtSystem::WideLength(_data)) * 2;

    int total = 0;
    for (const uint8_t* q = p; q < end;) {
        int32_t cp = -1;
        q += RtSystem::Utf16DecodeOne(q, end, &cp);
        if (cp >= 0) {
            uint8_t scratch[4];
            total += RtSystem::Utf8EncodeOne(cp, scratch);
        }
    }

    uint8_t* buf = static_cast<uint8_t*>(  // LEFT: an alloc result; the tier-I shape does not match the RtMemAlloc spelling
        RtSystem::RtMemAlloc(static_cast<uint64_t>(total) + 1,
                             RtSystem::kStringAllocAlign));
    buf[total] = 0;

    uint8_t* out    = buf;
    int      written = 0;
    for (const uint8_t* q = p; q < end;) {
        int32_t cp = -1;
        q += RtSystem::Utf16DecodeOne(q, end, &cp);
        if (cp < 0) continue;
        uint8_t scratch[4];
        const int m = RtSystem::Utf8EncodeOne(cp, scratch);
        written += m;
        if (written > total) continue;
        if (buf == nullptr)  continue;
        if (m <= 0)          continue;
        std::memcpy(out, scratch, static_cast<size_t>(m));
        out += m;
    }

    // ENDGAME B.3.1 step 2: retail's `*ret = 0` is the default ctor
    // (0x180006760) and the result is the hidden return slot — raw bytes — so
    // the String was placement-constructed into it before Set (0x1800c95d0)
    // ran. B.3.4b SYSTEM (4a): the hidden slot is the COMPILER's business now,
    // so the member holds an ordinary local and returns it; the DOOR is what
    // still placement-constructs into a caller-supplied slot.
    System::String ret;
    ret.Set(reinterpret_cast<const char*>(buf));  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    if (buf != nullptr) RtSystem::RtMemFree(buf);

    // `this` is const and nothing in the body touches it, so the shipped body
    // runs on identical input. Its result is a freshly allocated String, so
    // ours and theirs never share a pointer — compare by TEXT.
    return ret;
}

// ============================================================================
// ??0WideString@System@@QEAA@AEBVString@1@@Z               retail 0x1800cab50
// ============================================================================
// The mirror of AsUTF8: two passes over a UTF-8 String producing UTF-16.
//
// The one line worth reading twice is retail's `SAR EAX,1` at 0x1800cabdf: the
// first pass accumulates the encoder's return in BYTES, and the character
// count handed to SetLength is that halved. So a supplementary codepoint
// correctly costs TWO characters, and the terminator is written at
// `data[bytes/2]`.
System::WideString::WideString(const System::String& s) {

    _data = nullptr;

    // ENDGAME B.3.1 step 2's bridge is gone from this body: `s` IS the class
    // now, because the parameter is the class. `GetLength()` (0x1800063c0) is
    // still the null-guarded stored-length read this line used to spell by
    // hand; the POD-to-class conversion moved up into the door.

    const uint8_t* p   = reinterpret_cast<const uint8_t*>(s._data);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* end = p + static_cast<size_t>(s.GetLength());

    int totalBytes = 0;
    for (const uint8_t* q = p; q < end;) {
        int32_t cp = -1;
        q += RtSystem::Utf8DecodeOne(q, end, &cp);
        if (cp >= 0) {
            uint8_t scratch[4];
            totalBytes += RtSystem::Utf16EncodeOne(cp, scratch);
        }
    }

    const int chars = totalBytes >> 1;
    SetLength(chars);

    // Retail RE-READS `src->data` here rather than reusing `p`; kept.
    const uint8_t* r    = reinterpret_cast<const uint8_t*>(s._data);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* rend = r + static_cast<size_t>(s.GetLength());
    uint8_t* const base = reinterpret_cast<uint8_t*>(_data);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    uint8_t*       out  = base;
    int            written = 0;
    for (const uint8_t* q = r; q < rend;) {
        int32_t cp = -1;
        q += RtSystem::Utf8DecodeOne(q, rend, &cp);
        if (cp < 0) continue;
        uint8_t scratch[4];
        const int m = RtSystem::Utf16EncodeOne(cp, scratch);
        written += m;
        if (written > totalBytes) continue;
        if (base == nullptr)      continue;
        if (m <= 0)               continue;
        std::memcpy(out, scratch, static_cast<size_t>(m));
        out += m;
    }
    _data[chars] = 0;

}

// ============================================================================
// The three rows dx11 has been asking for and nobody was answering
// ============================================================================
// `tools/runtime_census.py` names `dx11/src/devicedata.cpp` as the CALLER of
// four WideString exports, and before this lane all four had `ported=0`.
// AsUTF8 is closed above because GetRootDirectory needed it; these three were
// found by reading the census AFTER the port rather than before, and they are
// on the DEVICE-ENUMERATION path — DeviceData::EnumerateInterfaces turns each
// adapter's UTF-16 Description into a String — which runs at Video startup on
// every boot. Leaving them would have closed one wall and left three null
// calls a few frames behind it.
//
// All three are small enough to quote in full, and all three were read from
// the DISASSEMBLY: retail dereferences `this` and `src` unguarded in every one
// of them, and so does this port.

// ============================================================================
// ??0WideString@System@@QEAA@PEBD@Z                        retail 0x1800cb110
//                                                   ord 514, Wave 9 lane PLAT
// ============================================================================
// The SAME two-pass UTF-8 -> UTF-16 body as the `const String&` constructor
// above, differing in exactly one thing: the source extent comes from an
// INLINE strlen of a raw `const char*` (0x1800cb132..0x1800cb14a and again at
// 0x1800cb1c3..0x1800cb1d9) instead of from String's stored length.
//
// Retail walks the source twice for its extent — once for the first pass's
// `end` and once again before the second pass. The two walks are transcribed
// separately rather than folded, for the same reason `Set` keeps its re-walk:
// folding them would silently change behaviour if a caller ever mutated `src`
// in between, and this is a transcription.
//
// NOT NULL-TOLERANT. Retail's first act after zeroing `this->data` is
// `CMP byte ptr [RDX],BPL` — an unguarded load of `src[0]`. Kept unguarded.
//
// WHY THIS LANE NEEDED IT: `?PlatformStartup@Application@1@AEAA_NXZ`
// (0x18009fc10) opens with `LEA RDX,[0x180154858] ; CALL 0x1800cb110`, i.e.
// `WideString className("Banished")`, and the resulting buffer is the window
// class name it hands to RegisterClassW and CreateWindowExW. Wave 9 lane FS
// ported four of this class's constructors and left this one, because nothing
// in the tree called it then.
System::WideString::WideString(const char* src) {

    _data = nullptr;

    // 0x1800cb132: the first inline strlen. `R8D` counts, `RSI = RDX + count`.
    int srcLen = 0;
    while (src[srcLen] != 0) ++srcLen;

    const uint8_t* const p   = reinterpret_cast<const uint8_t*>(src);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* const end = p + static_cast<size_t>(srcLen);

    int totalBytes = 0;
    for (const uint8_t* q = p; q < end;) {
        int32_t cp = -1;
        q += RtSystem::Utf8DecodeOne(q, end, &cp);
        if (cp >= 0) {
            uint8_t scratch[4];
            totalBytes += RtSystem::Utf16EncodeOne(cp, scratch);
        }
    }

    // 0x1800cb1ab: `SAR EAX,1` — the accumulator is in BYTES, the length handed
    // to SetLength is in CHARACTERS.
    const int chars = totalBytes >> 1;
    SetLength(chars);

    // 0x1800cb1c3: retail's SECOND inline strlen, over the ORIGINAL pointer.
    int srcLen2 = 0;
    while (src[srcLen2] != 0) ++srcLen2;

    const uint8_t* const r    = reinterpret_cast<const uint8_t*>(src);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    const uint8_t* const rend = r + static_cast<size_t>(srcLen2);
    uint8_t* const base = reinterpret_cast<uint8_t*>(_data);  // LEFT: the UTF-8 / UTF-16 transcoders walk the text as bytes; retyping them over char / wchar_t units is owed
    uint8_t*       out  = base;
    int            written = 0;
    for (const uint8_t* q = r; q < rend;) {
        int32_t cp = -1;
        q += RtSystem::Utf8DecodeOne(q, rend, &cp);
        if (cp < 0) continue;
        uint8_t scratch[4];
        const int m = RtSystem::Utf16EncodeOne(cp, scratch);
        written += m;
        if (written > totalBytes) continue;   // 0x1800cb236  CMP R15D,R14D ; JG
        if (base == nullptr)      continue;   // 0x1800cb23b  TEST R12,R12  ; JZ
        if (m <= 0)               continue;   // 0x1800cb243  TEST EAX,EAX  ; JLE
        std::memcpy(out, scratch, static_cast<size_t>(m));
        out += m;
    }
    _data[chars] = 0;                         // 0x1800cb275

}

// ??0WideString@System@@QEAA@PEB_W@Z                       retail 0x18000a6e0
// `this->data = 0; Set(this, src);` and nothing else — eight instructions.
System::WideString::WideString(const wchar_t* src) {
    _data = nullptr;
    Set(src);
}

// ??1WideString@System@@QEAA@XZ                            retail 0x18000a730
// Byte-for-byte the String release idiom, asymmetry included: the freeing
// branch leaves `data - 8` in the object (`ADD qword [RCX],-8` then a TAIL JMP
// to MemFree), the sharing branch nulls it.
System::WideString::~WideString() {
    RtSystem::WideReleaseInline(this);
}

// ?StrLength@WideString@System@@SAHPEB_W@Z                 retail 0x1800cab20
// wcslen, and NOT null-tolerant: retail's first instruction after the zeroing
// XOR is `CMP word ptr [RCX],AX`, an unguarded load. Kept unguarded — a null
// here is an upstream fault to find, not one to paper over.
int System::WideString::StrLength(const wchar_t* s) {
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

// ============================================================================
// The remaining doors — every name, signature and registration row unchanged.
// ============================================================================
// These four could not sit with the others above, because a door that
// placement-constructs the class has to be written after the class's own
// constructors are defined for the reader, not for the compiler. Each is a
// CONSTRUCTOR door: `self` is the caller's RAW BYTES (an uninitialised
// `RtSystem::WideString` local in platform_startup_win.cpp,
// application_win.cpp, updateloop_loadresources.cpp and value_types.cpp), so
// the class is placement-constructed into it, never assigned — an assignment
// would first run the destructor over whatever garbage the caller's stack held.
// The return value is retail's `MOV RAX,RCX`, i.e. `self` itself.
namespace RtSystem {

WideString* RtSystem_WideString_CtorFromString(WideString* self, const String* src) {
    new (Sys(self)) System::WideString(*Sys(src));
    return self;
}

WideString* RtSystem_WideString_CtorFromChars(WideString* self, const char* src) {
    new (Sys(self)) System::WideString(src);
    return self;
}

void RtSystem_WideString_Dtor(WideString* self) {
    Sys(self)->~WideString();
}

}  // namespace RtSystem
