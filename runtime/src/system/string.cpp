// ============================================================================
// runtime/src/system/string.cpp — System::String   (Wave 2, lane A)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
// runtime/types/system/String.h carries the layout and the release idiom;
// read it first.
//
// THE ABI RULE (docs/runtime_port/WAVE1.md, "The ABI rule this wave is built
// on"): System::String has a user-declared constructor, so MSVC returns it BY
// MEMORY. The hidden return-slot pointer is an ARGUMENT — the SECOND register
// for a member (RCX=this, RDX=ret, R8=arg1) and the FIRST for a static
// (RCX=ret, RDX=arg1) — and the callee returns it again in RAX. Every port
// below spells the slot out as a parameter and returns it. Writing these as
// C++ methods returning by value is the wild write described in the memory
// note reference_hidden_retslot_wild_write.md.
//
// ALLOCATION MUST STAY ON THE DLL'S HEAP. String objects cross the DLL
// boundary in both directions constantly (2,388 inbound call edges), so a
// buffer we allocate is routinely freed by the DLL and vice versa. Everything
// here allocates and frees through Runtime-x64.dll's own
// System::MemoryAllocator, resolved with RtOriginal<>() rather than
// ResolveExport() so that a future MemoryAllocator port (opt-in, another
// lane) cannot silently split the heap in two.
//
// DIFFABILITY. Most of this class mutates: a ctor writes into a live object, a
// dtor decrements a SHARED refcount, Set/operator=/operator+= do both. Running
// the original as an oracle would mutate twice, and on a refcounted buffer
// that is a double free. Those functions are therefore left undiffed, each
// with a comment saying so. What IS diffed: the three comparisons, ToFloat,
// SubString, and both free operator+ overloads — all const on their inputs,
// each producing an output the original can be handed its own copy of.
// ============================================================================

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "../../types/system/String.h"
#include "../../funcs/rt_port.h"
#include "string.h"

namespace RtSystem {

// ============================================================
// The DLL's allocator
// ============================================================
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  retail 0x1800c7a50
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     retail 0x1800c7ad0
// Both are the lock-wrapped statics: MemAlloc = EnterCS(_singleton+0x640);
// Allocate(_singleton, size, align); LeaveCS. String::SetLength inlines that
// same sequence rather than calling MemAlloc, so the two are interchangeable,
// and calling the export keeps standalone/allocator_lock.cpp's detour on
// Allocate/Free in the path exactly as before.
//
// NOTE: the ...@SAPEAXII@Z ("unsigned, unsigned") spelling of MemAlloc that
// circulates in this project's notes is NOT an export of Runtime-x64.dll. The
// only two forms are the _KH (size_t, int) and PEAX ones used here.
namespace {
using MemAllocFn = void* (*)(uint64_t, int);
using MemFreeFn  = void  (*)(void*);
MemAllocFn g_memAlloc = nullptr;
MemFreeFn  g_memFree  = nullptr;
}  // namespace

// ---------------------------------------------------------------------------
// CUTOVER: the raw door has nothing behind it, so fall back to the PORT
// ---------------------------------------------------------------------------
// The choice of RtOriginal<>() over ResolveExport() above is deliberate and
// still right WHILE THE DLL IS PRESENT: it pins both halves of every String
// buffer to one heap, so a half-ported MemoryAllocator cannot free on our heap
// something the DLL allocated. Lane IOA found the other half of that story.
//
// With Runtime-x64.dll ABSENT, RtOriginal<>() returns NULL and `g_memFree(p)`
// is an EXEC-of-0 -- on a tree where ?MemFree@MemoryAllocator@System@@ IS
// ported and registered (allocator.cpp:1274). The port exists and this door
// refuses to use it. That is the MIRROR of the failure this wave has been
// chasing (closing a raw door over a symbol with NO port); same file family,
// opposite direction, and it is TREE-WIDE, not one lane's: every landed
// String-using port on the boot path frees through here, IO::Persistence
// included.
//
// The fix keeps the original guarantee EXACTLY. The raw door is still tried
// first, so with the DLL present behaviour is byte-identical and the heap
// cannot split. Only when the raw door yields null -- which means there is no
// DLL heap to split with -- does it fall through to the registered port, which
// is then the only allocator in the process. Registration is all-or-nothing
// (RegisterAllocatorExports registers MemAlloc and MemFree together), so the
// two halves can never disagree about whose heap they are on.


void* RtMemAlloc(uint64_t size, int align) {
    if (!g_memAlloc) {
        g_memAlloc = RtOriginal<MemAllocFn>("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
        if (!g_memAlloc)   // DLL absent -- see the CUTOVER note above
            g_memAlloc = reinterpret_cast<MemAllocFn>(
                ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));
    }
    return g_memAlloc(size, align);
}

void RtMemFree(void* p) {
    if (!g_memFree) {
        g_memFree = RtOriginal<MemFreeFn>("?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
        if (!g_memFree)    // DLL absent -- see the CUTOVER note above
            g_memFree = reinterpret_cast<MemFreeFn>(
                ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    }
    g_memFree(p);
}

// ============================================================
// The inlined idioms
// ============================================================

// ?SetLength@String@System@@AEAAXH@Z  retail 0x1800c8220
// Release, then allocate (len+1)+8 bytes; length at block+0, refcount 1 at
// block+4, data = block+8. Does NOT write a terminator — every caller does.
void RtString_SetLength(String* self, int len) {
    RtString_ReleaseInline(self);
    int32_t* block = static_cast<int32_t*>(
        RtMemAlloc(static_cast<uint64_t>(static_cast<int64_t>(len) + 1) + 8, kStringAllocAlign));
    self->data = reinterpret_cast<char*>(block + 2);
    block[0] = len;
    block[1] = 1;
}

// The release sequence from ?Release@String@System@@AEAAXXZ (0x1800c9870) and
// ~String (0x180006c80). The asymmetry is retail's: on the freeing branch the
// object keeps `data - 8`, on the sharing branch it is nulled. See String.h.
void RtString_ReleaseInline(String* self) {
    char* d = self->data;
    if (d == nullptr)
        return;
    if (--StringRefCount(d) == 0) {
        self->data = d - 8;
        RtMemFree(self->data);
    } else {
        self->data = nullptr;
    }
}

// The AddRef half of the copy ctor (0x180006cb0). `self->data` must already
// have been zeroed or released.
void RtString_AdoptInline(String* self, char* src) {
    if (src != nullptr) {
        self->data = src;
        ++StringRefCount(src);
    }
}

// ?StrCompare@String@System@@SAHPEBD0@Z  retail 0x1800c98a0
// Null-tolerant strcmp: null sorts before everything, two nulls are equal.
// Retail walks to the first differing byte (or to either terminator) and then
// returns the signed difference of the CHARS at that index.
int RtString_StrCompare(const char* a, const char* b) {
    if (a == nullptr)
        return -static_cast<int>(b != nullptr);
    if (b == nullptr)
        return 1;
    int i = 0;
    if (a[0] == b[0]) {
        // Note the order: retail ADVANCES first and then tests, so on a
        // mismatch it reports the index of the differing pair, and on equal
        // strings it walks one past the last character and reports the two
        // terminators. Testing before advancing would return the previous
        // index and get the sign right by accident but the magnitude wrong.
        while (a[i] != '\0' && b[i] != '\0') {
            ++i;
            if (a[i] != b[i])
                break;
        }
    }
    return static_cast<int>(a[i]) - static_cast<int>(b[i]);
}

// ?GetLength@String@System@@QEBAHXZ  retail 0x1800063c0
int RtString_GetLength(const String* self) {
    return self->data ? StringLength(self->data) : 0;
}

// Text comparison used by every diff site in this file that compares two
// Strings rather than a scalar.
static bool RtStringSameText(const String* a, const String* b) {
    if ((a->data == nullptr) != (b->data == nullptr))
        return false;
    if (a->data == nullptr)
        return true;
    return StringLength(a->data) == StringLength(b->data) &&
           std::strcmp(a->data, b->data) == 0;
}

// ??HSystem@@YA?AVString@0@AEBV10@0@Z  retail 0x1800c9440
// The free operator+(const String&, const String&). NOT a member: it is a
// namespace-scope function, so the hidden return slot is the FIRST argument
// (RCX=ret, RDX=lhs, R8=rhs) — the static-function form of the ABI rule, not
// the member form. Ghidra shows it with `in_R8` because it cannot type the
// third argument across the hidden slot.
//
// Diffable: both inputs are const, and every branch either allocates a fresh
// buffer or takes a reference on an existing one, so the oracle's copy can be
// released afterwards with no net effect. The adopt branches make theirs
// AddRef an input buffer (+1) and the release afterwards undoes it (-1); the
// count can never reach 0 there, because the input itself still holds a
// reference.
static String* RtStringConcatImpl(String* ret, const String* lhs, const String* rhs) {
    const int lhsLen = lhs->data ? StringLength(lhs->data) : 0;
    const int rhsLen = rhs->data ? StringLength(rhs->data) : 0;

    // Either side empty (null OR zero stored length): the result ADOPTS the
    // other side's buffer rather than copying it.
    if (lhs->data == nullptr || lhsLen == 0) {
        ret->data = nullptr;
        RtString_AdoptInline(ret, rhs->data);
        return ret;
    }
    if (rhs->data == nullptr || rhsLen == 0) {
        ret->data = nullptr;
        RtString_AdoptInline(ret, lhs->data);
        return ret;
    }

    String tmp{nullptr};
    RtString_SetLength(&tmp, lhsLen + rhsLen);
    char* out = tmp.data;
    std::memcpy(out, lhs->data, static_cast<size_t>(lhsLen));
    std::memcpy(out + lhsLen, rhs->data, static_cast<size_t>(rhsLen));
    out[lhsLen + rhsLen] = '\0';

    // Retail's move-out: adopt (refcount 1 -> 2), then release the local
    // (2 -> 1). The pair is not a no-op — it is how the buffer changes owner
    // without a copy — so it is written out rather than folded away.
    ret->data = nullptr;
    if (out != nullptr) {
        ret->data = out;
        ++StringRefCount(out);
        if (--StringRefCount(out) == 0) {
            tmp.data = out - 8;
            RtMemFree(tmp.data);
        }
    }
    return ret;
}

String* RtSystem_String_Concat(String* ret, const String* lhs, const String* rhs) {
    RT_DIFF_BEGIN("System::operator+(String,String)");
    RtStringConcatImpl(ret, lhs, rhs);
    // `ret` aliasing an input would mean ours had overwritten the oracle's
    // input before it ran (WAVE1.md, "the oracle must not corrupt its own
    // experiment"). A hidden return slot is always a fresh temporary in
    // practice; the guard says so rather than assuming it.
    if (rt_diff_on__ && ret != lhs && ret != rhs) {
        String theirs{nullptr};
        RtOriginal<String* (*)(String*, const String*, const String*)>(
            "??HSystem@@YA?AVString@0@AEBV10@0@Z")(&theirs, lhs, rhs);
        RT_DIFF_SHADOW(RtStringSameText(ret, &theirs), ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(&theirs);
    }
    return ret;
}

// ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z  retail 0x1800c92d0
// Same shape against a C string, and the same static-function ABI: RCX=ret,
// RDX=lhs, R8=rhs. Note the asymmetry with the String overload: when `lhs` is
// empty this one Set()s from `rhs` (a COPY), because a C string has no
// refcounted buffer to adopt.
static String* RtStringConcatCStrImpl(String* ret, const String* lhs, const char* rhs) {
    const int lhsLen = lhs->data ? StringLength(lhs->data) : 0;

    if (lhs->data == nullptr || lhsLen == 0) {
        ret->data = nullptr;
        RtSystem_String_Set(ret, rhs);
        return ret;
    }
    if (rhs == nullptr) {
        ret->data = nullptr;
        RtString_AdoptInline(ret, lhs->data);
        return ret;
    }

    int rhsLen = 0;
    while (rhs[rhsLen] != '\0')
        ++rhsLen;

    String tmp{nullptr};
    RtString_SetLength(&tmp, lhsLen + rhsLen);
    char* out = tmp.data;
    std::memcpy(out, lhs->data, static_cast<size_t>(lhsLen));
    std::memcpy(out + lhsLen, rhs, static_cast<size_t>(rhsLen));
    out[lhsLen + rhsLen] = '\0';

    ret->data = nullptr;
    if (out != nullptr) {
        ret->data = out;
        ++StringRefCount(out);
        if (--StringRefCount(out) == 0) {
            tmp.data = out - 8;
            RtMemFree(tmp.data);
        }
    }
    return ret;
}

String* RtSystem_String_ConcatCStr(String* ret, const String* lhs, const char* rhs) {
    RT_DIFF_BEGIN("System::operator+(String,char*)");
    RtStringConcatCStrImpl(ret, lhs, rhs);
    if (rt_diff_on__ && ret != lhs) {
        String theirs{nullptr};
        RtOriginal<String* (*)(String*, const String*, const char*)>(
            "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z")(&theirs, lhs, rhs);
        RT_DIFF_SHADOW(RtStringSameText(ret, &theirs), ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(&theirs);
    }
    return ret;
}

// ============================================================
// Constructors / destructor  — NOT DIFFED
// ============================================================
// A ctor has no independent result to compare: running the original a second
// time over the same object would either re-zero it or take a second reference
// on a shared buffer. A dtor is worse — running it twice is a double release
// and, at refcount 1, a double free.

// ??0String@System@@QEAA@XZ  retail 0x180006760
String* RtSystem_String_Ctor(String* self) {
    self->data = nullptr;
    return self;
}

// ??0String@System@@QEAA@PEBD@Z  retail 0x18000b630
String* RtSystem_String_CtorCStr(String* self, const char* src) {
    self->data = nullptr;
    RtSystem_String_Set(self, src);
    return self;
}

// ??0String@System@@QEAA@AEBV01@@Z  retail 0x180006cb0
String* RtSystem_String_CtorCopy(String* self, const String* other) {
    self->data = nullptr;
    RtString_AdoptInline(self, other->data);
    return self;
}

// ??1String@System@@QEAA@XZ  retail 0x180006c80
void RtSystem_String_Dtor(String* self) {
    RtString_ReleaseInline(self);
}

// ============================================================
// Assignment / mutation — NOT DIFFED (see the header comment)
// ============================================================

// ??4String@System@@QEAAAEAV01@AEBV01@@Z  retail 0x1800c9810
// Self-assignment and same-buffer assignment are short-circuited on the
// POINTER, before the release; that guard is what makes `a = a` safe.
String* RtSystem_String_Assign(String* self, const String* rhs) {
    if (self->data != rhs->data) {
        RtString_ReleaseInline(self);
        self->data = rhs->data;
        if (self->data != nullptr)
            ++StringRefCount(self->data);
    }
    return self;
}

// ?Set@String@System@@QEAAXPEBD@Z  retail 0x1800c95d0
void RtSystem_String_Set(String* self, const char* src) {
    if (src == nullptr) {
        RtString_ReleaseInline(self);
        return;
    }
    // Setting to a string that compares equal to the current contents is a
    // no-op: retail does not reallocate, and does not un-share either.
    if (self->data != nullptr && RtString_StrCompare(self->data, src) == 0)
        return;

    int len = 0;
    while (src[len] != '\0')
        ++len;
    RtString_SetLength(self, len);
    // Retail recomputes the length a second time here and inlines StrCopy
    // (0x1800c96c0) with capacity len+1; the two lengths are necessarily
    // equal, so StrCopy's truncation arm is unreachable from Set.
    std::memcpy(self->data, src, static_cast<size_t>(len) + 1);
}

// ??YString@System@@QEAA?AV01@AEBV01@@Z  retail 0x1800c8a90
// ??Y is operator+= , not operator+ — and the `?AV01@` return type means it
// ALSO returns a copy of the new value by memory. The body proves the name:
// it Set()s `this`.
//
// ABI: RCX=this, RDX=hidden return slot, R8=rhs. Ghidra's decompilation drops
// R8 entirely — the inner concat call is itself a hidden-return-slot call and
// the argument is never re-materialised, exactly the trap WAVE1.md defect 5
// describes. Transcribed from the listing at 0x1800c8ab6..0x1800c8ac4, which
// shows RCX=&tmp, RDX=this and R8 passing straight through untouched.
String* RtSystem_String_AppendAssign(String* self, String* ret, const String* rhs) {
    String tmp{nullptr};
    RtSystem_String_Concat(&tmp, self, rhs);
    RtSystem_String_Set(self, tmp.data);
    RtString_ReleaseInline(&tmp);

    ret->data = nullptr;
    RtString_AdoptInline(ret, self->data);
    return ret;
}

// ??YString@System@@QEAA?AV01@PEBD@Z  retail 0x1800c9120
String* RtSystem_String_AppendAssignCStr(String* self, String* ret, const char* rhs) {
    String tmp{nullptr};
    RtSystem_String_ConcatCStr(&tmp, self, rhs);
    RtSystem_String_Set(self, tmp.data);
    RtString_ReleaseInline(&tmp);

    ret->data = nullptr;
    RtString_AdoptInline(ret, self->data);
    return ret;
}

// ============================================================
// Comparisons — fully diffable (pure, const, no allocation)
// ============================================================

// ??8String@System@@QEBA_NAEBV01@@Z  retail 0x1800063d0
// Compares the STORED lengths first (data[-8]), not strlen; two strings of
// different stored length are unequal without touching the characters.
bool RtSystem_String_Equals(const String* self, const String* other) {
    RT_DIFF_BEGIN("System::String::operator==(String)");
    const char* a = self->data;
    const char* b = other->data;
    const int la = a ? StringLength(a) : 0;
    const int lb = b ? StringLength(b) : 0;

    bool ours;
    if (la != lb)
        ours = false;
    else if (a != nullptr && StringLength(a) != 0)
        ours = RtString_StrCompare(a, b) == 0;
    else
        ours = true;

    RT_DIFF_SCALAR(ours,
        RtOriginal<bool (*)(const String*, const String*)>(
            "??8String@System@@QEBA_NAEBV01@@Z")(self, other));
    return ours;
}

// ??8String@System@@QEBA_NPEBD@Z  retail 0x180053c00
bool RtSystem_String_EqualsCStr(const String* self, const char* other) {
    RT_DIFF_BEGIN("System::String::operator==(char*)");
    bool ours;
    if (self->data == nullptr)
        ours = (other == nullptr);
    else if (other != nullptr)
        ours = RtString_StrCompare(self->data, other) == 0;
    else
        ours = false;

    RT_DIFF_SCALAR(ours,
        RtOriginal<bool (*)(const String*, const char*)>(
            "??8String@System@@QEBA_NPEBD@Z")(self, other));
    return ours;
}

// ??MString@System@@QEBA_NAEBV01@@Z  retail 0x180029360
// Null is LESS than everything, including another null: `x < null` is false
// and `null < null` is false, but `null < x` is true. That asymmetry is the
// ordering every Collection::Set<String> in the DLL is built on.
bool RtSystem_String_Less(const String* self, const String* other) {
    RT_DIFF_BEGIN("System::String::operator<");
    bool ours;
    if (other->data == nullptr)
        ours = false;
    else if (self->data == nullptr)
        ours = true;
    else
        ours = RtString_StrCompare(self->data, other->data) < 0;

    RT_DIFF_SCALAR(ours,
        RtOriginal<bool (*)(const String*, const String*)>(
            "??MString@System@@QEBA_NAEBV01@@Z")(self, other));
    return ours;
}

// ============================================================
// SubString — diffable
// ============================================================

// ?SubString@String@System@@QEBA?AV12@HH@Z  retail 0x1800c9230
// The two ints are START and END (inclusive), not start and count: retail
// computes `count = (end - start) + 1`. Nothing is clamped — an end past the
// terminator reads past it, and an end < start - 1 makes `count` negative and
// the trailing `data[count] = 0` writes BEHIND the buffer. Both are retail's,
// and the inclusive-end convention is load-bearing: Filename::AdjustDelimiters
// and String::FormatParamList both depend on it.
String* RtSystem_String_SubString(const String* self, String* ret, int start, int end) {
    RT_DIFF_BEGIN("System::String::SubString");
    const int count = (end - start) + 1;
    ret->data = nullptr;
    RtString_SetLength(ret, count);
    for (int i = 0; i < count; ++i)
        ret->data[i] = self->data[start + i];
    ret->data[count] = '\0';

    // The original gets its own freshly allocated buffer; `self` is const and
    // nothing is shared, so both can run on the same inputs — PROVIDED `ret`
    // is not `self`. A hidden return slot is always a fresh temporary in
    // practice, but if it ever were not, ours would have overwritten the input
    // before theirs ran and the oracle would manufacture a divergence
    // (WAVE1.md, "the oracle must not corrupt its own experiment").
    if (rt_diff_on__ && ret != self) {
        String theirs{nullptr};
        RtOriginal<String* (*)(const String*, String*, int, int)>(
            "?SubString@String@System@@QEBA?AV12@HH@Z")(self, &theirs, start, end);
        const bool equal =
            ((ret->data == nullptr) == (theirs.data == nullptr)) &&
            (ret->data == nullptr ||
             (StringLength(ret->data) == StringLength(theirs.data) &&
              std::strcmp(ret->data, theirs.data) == 0));
        RT_DIFF_SHADOW(equal, ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(&theirs);
    }
    return ret;
}

// ============================================================
// ToFloat — diffable, pure
// ============================================================

// ?ToFloat@String@System@@SAMPEBD@Z  retail 0x1800c8070
// A hand-rolled strtof. Every float constant below is the .rdata value retail
// loads, named by address; the exponent is applied by repeated multiplication
// in chunks of 1e20 / 1e8 / 10, which is NOT the same rounding as a single
// pow10 table lookup — hence the literal transcription.
float RtSystem_String_ToFloat(const char* s) {
    RT_DIFF_BEGIN("System::String::ToFloat");
    float ours;
    if (s == nullptr) {
        ours = 0.0f;
    } else {
        while (*s == ' ' || *s == '\t')
            ++s;

        float sign = -1.0f;                       // 0x18016175c
        if (*s == '-') {
            ++s;
        } else {
            sign = 1.0f;                          // 0x18016169c
            if (*s == '+')
                ++s;
        }

        float mant = 0.0f;
        for (char c = *s; c > '/' && c < ':'; c = *s) {
            ++s;
            mant = static_cast<float>(c - '0') + mant * 10.0f;   // 0x1801616f0
        }

        if (*s == '.') {
            ++s;
            float scale = 0.1f;                   // 0x180161684
            for (char c = *s; c > '/' && c < ':'; c = *s) {
                ++s;
                const float term = static_cast<float>(c - '0') * scale;
                scale *= 0.1f;
                mant += term;
            }
        }

        unsigned exp = 0;
        bool negExp = false;
        float escale = 1.0f;

        // ((*s + 0xbb) & 0xdf) == 0 selects exactly 'e' (0x65) and 'E' (0x45).
        if (((static_cast<unsigned>(static_cast<int>(*s)) + 0xbbu) & 0xdfu) == 0) {
            const char* p = s + 1;
            if (s[1] == '-') {
                negExp = true;
                p = s + 2;
            } else if (s[1] == '+') {
                p = s + 2;
            }
            char c = *p;
            if (c > '/') {
                while (c <= '9') {
                    ++p;
                    exp = exp * 10u + static_cast<unsigned>(c - '0');
                    c = *p;
                    if (c <= '/')
                        break;
                }
                bool bigChunks;
                if (exp < 0x80u) {
                    bigChunks = (exp > 0x13u);
                } else {
                    exp = 0x7fu;                  // clamped, not saturated to inf
                    bigChunks = true;
                }
                if (bigChunks) {
                    unsigned k = exp / 0x14u;
                    do {
                        exp -= 0x14u;
                        escale *= 1.0e20f;        // 0x180161748
                        --k;
                    } while (k != 0);
                }
                if (exp > 7u) {
                    unsigned k = exp >> 3;
                    do {
                        exp -= 8u;
                        escale *= 1.0e8f;         // 0x180161740
                        --k;
                    } while (k != 0);
                }
                for (; exp != 0u; --exp)
                    escale *= 10.0f;
            }
        }

        ours = negExp ? (mant / escale) * sign : escale * mant * sign;
    }

    // Bit equality demanded: this is integer-and-multiply arithmetic with no
    // transcendental in it, so a faithful port must round identically.
    RT_DIFF_FLOAT(ours,
        RtOriginal<float (*)(const char*)>("?ToFloat@String@System@@SAMPEBD@Z")(s), 0.0f);
    return ours;
}

// ============================================================
// Format / FormatIndexed — NOT DIFFED (varargs cannot be forwarded)
// ============================================================
// Both are `...` functions with no va_list twin in the DLL's export table, so
// there is no way to hand the original the same argument list a second time.
// The diff harness has nothing to offer here; faking it with a hand-built
// argument list would test a different call than the one that happened.

// ?Format@String@System@@SA?AV12@PEBDZZ  retail 0x1800c9730
// ABI: a static returning by memory, so RCX = the hidden return slot and RDX =
// the format string; the varargs start at R8, whose home slot is where retail
// points its va_list.
String* RtSystem_String_Format(String* ret, const char* fmt, ...) {
    if (fmt == nullptr) {
        ret->data = nullptr;
        return ret;
    }

    // Retail's buffer is 0x4000 bytes on the stack, filled with _vsnprintf and
    // then hard-terminated at [0x3fff]. _vsnprintf does not terminate on
    // truncation and C99 vsnprintf does; the explicit terminator makes the two
    // agree on the truncated result too, so this is not a divergence.
    char buf[0x4000];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, 0x4000, fmt, ap);
    va_end(ap);
    buf[0x3fff] = '\0';

    String tmp{nullptr};
    RtSystem_String_Set(&tmp, buf);

    ret->data = nullptr;
    if (tmp.data != nullptr) {
        ret->data = tmp.data;
        ++StringRefCount(tmp.data);
        if (--StringRefCount(tmp.data) == 0) {
            char* blk = tmp.data - 8;
            tmp.data = blk;
            RtMemFree(blk);
        }
    }
    return ret;
}

// Collection::Array<String>::SetCount, retail FUN_180029a80, reduced to the
// only case FormatIndexed reaches: a freshly zeroed {data,count,cap} array
// grown from empty to n > 0. In that case the general body takes cap(0) < n
// -> cap = n, the "grew by exactly one from non-empty" special case is false,
// a new block is allocated, every slot is zero-initialised, and both the
// destroy-old-range and free-old-block arms are no-ops because the old pointer
// is null. Nothing else in this file needs the general version.
namespace {
struct StringArray {
    String* data;
    int32_t count;
    int32_t capacity;
};

void StringArray_SetCountFromEmpty(StringArray* arr, int n) {
    arr->count = n;
    arr->capacity = n;
    arr->data = static_cast<String*>(RtMemAlloc(static_cast<uint64_t>(n) * 8, 8));
    for (int i = 0; i < n; ++i)
        arr->data[i].data = nullptr;
}

// retail FUN_180011010 — release every String in a range.
void StringArray_DestroyRange(String* p, int n) {
    for (int i = 0; i < n; ++i) {
        char* d = p[i].data;
        if (d == nullptr)
            continue;
        if (--StringRefCount(d) == 0) {
            p[i].data = d - 8;
            if (p[i].data != nullptr)
                RtMemFree(p[i].data);
        } else {
            p[i].data = nullptr;
        }
    }
}
}  // namespace

// ?FormatIndexed@String@System@@SA?AV12@PEBDZZ  retail 0x1800c8b40
// "@1".."@9" substitution, not printf: FormatParamCount scans the format for
// the highest @n used, an Array<String> of that many entries is built from the
// C-string varargs, and FormatParamList does the substitution.
//
// FormatParamCount (0x1800c8d20) and FormatParamList (0x1800c8dc0) were left on
// the DLL by Wave 2 as out of lane scope. **Wave 9 lane B3 ported both** (below,
// after this function) — they are two of WAVE9_LANE_V.md's REAL DEPENDENCY rows
// and the two calls here were the raw-door sites that made them so. The
// warning in the paragraph this replaced was accurate and was the reason to be
// careful rather than to stop: FormatParamList IS a hidden-return-slot function
// whose R8 argument Ghidra drops, so it was transcribed from the listing at
// 0x1800c8dc0..0x1800c9017 instruction by instruction and not from decompiled C.
// ============================================================
// Wave 9 lane B3 - the four System::String exports left on the DLL
// ============================================================
// All four are WAVE9_LANE_V.md REAL DEPENDENCY rows. Two are pure and get a
// full differential oracle; Release mutates a refcount and gets a shadow with
// the reason stated; FormatParamList allocates a fresh result from const
// inputs and so replays cleanly, exactly like SubString above.

// ?FormatParamCount@String@System@@SAHPEBD@Z  retail 0x1800c8d20
//
// Returns the HIGHEST parameter index used in `fmt`, where a parameter is the
// two characters `@` followed by an ASCII digit, and `@1` is index 1 -- so the
// answer is `digit - '0'`, computed by retail as `(int)(signed char)d - 0x2f`
// against a `- 0x30` range check. It is a MAXIMUM, not a count of occurrences:
// `"@2 @2"` answers 2 and `"@2"` alone also answers 2, which is why the
// StringArray FormatIndexed sizes from it can legitimately have holes.
//
// Retail's loop bound is the SECOND character's: a format ending in a bare `@`
// is not a parameter, because the `i + 1 >= len` test (0x1800c8d77
// `CMP R9D,R8D / JGE`) skips the case. `fmt == nullptr` returns 0 before the
// strlen.
int RtSystem_String_FormatParamCount(const char* fmt) {
    RT_DIFF_BEGIN("System::String::FormatParamCount");
    int result = 0;
    if (fmt != nullptr) {
        int len = 0;
        while (fmt[len] != '\0') ++len;
        for (int i = 0; i < len; ++i) {
            if (fmt[i] != '@' || i + 1 >= len) continue;
            const unsigned char d = static_cast<unsigned char>(fmt[i + 1]);
            if (static_cast<unsigned char>(d - '0') > 9u) continue;
            const int index = static_cast<int>(static_cast<signed char>(d)) - 0x2f;
            if (index > result) result = index;
        }
    }
    using Fn = int (*)(const char*);
    RT_DIFF_SCALAR(result, RtOriginal<Fn>("?FormatParamCount@String@System@@SAHPEBD@Z")(fmt));
    return result;
}

// ?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z  retail 0x1800c8dc0
//
// ABI: RCX = hidden return slot, RDX = `fmt`, R8 = `params` (an ARRAY of
// String indexed by the digit -- NOT a single String*, despite the `PEBV12@`
// spelling). Ghidra drops R8 entirely; the array indexing is visible only in
// the listing, at 0x1800c8efa `MOV RCX,[RBP+0x58]` (the home slot R8 was
// spilled to at entry) followed by `LEA R8,[RCX + RAX*0x8]`.
//
// The body walks `fmt` and appends alternately: the literal run since the last
// substitution, then `params[digit]`. It appends with `operator+=`
// (0x1800c8a90) and takes literal runs with `SubString` (0x1800c9230), both of
// which this file already ports.
//
// TWO DETAILS THAT ARE NOT SIMPLIFICATIONS:
//  * retail takes a REFCOUNTED COPY of `fmt` at entry (`work`, [RBP+0x50],
//    `INC [RAX-0x4]` at 0x1800c8e0a) and calls SubString on the copy while
//    reading characters out of `fmt` itself. Same buffer, so the characters
//    cannot differ -- but the AddRef and its matching Release at 0x1800c8fe0
//    are real refcount traffic, and a port that skipped them would leave a
//    different refcount behind and the next Release would free at the wrong
//    time.
//  * the literal-run guard is `if (i > 0)` on the ABSOLUTE index, not
//    `i > segStart` (0x1800c8e6d `TEST ESI,ESI / JLE`). For adjacent
//    parameters (`"@1@2"`) that produces `SubString(segStart, i-1)` with
//    `end < start`, i.e. a zero-length append, rather than skipping the append.
//    Transcribed as written.
String* RtSystem_String_FormatParamList(String* ret, const String* fmt, const String* params) {
    RT_DIFF_BEGIN("System::String::FormatParamList");

    String work{nullptr};
    RtString_AdoptInline(&work, fmt->data);          // refcounted copy of fmt
    ret->data = nullptr;

    const int len = (work.data != nullptr) ? StringLength(work.data) : 0;
    int segStart = 0;                                 // R8D
    int i = 0;                                        // RDI / ESI
    while (i < len) {
        const char* base = fmt->data;
        const bool isParam = base[i] == '@' && (i + 1) < len &&
                             base[i + 1] >= '0' && base[i + 1] <= '9';
        if (!isParam) { ++i; continue; }

        if (i > 0) {
            String lit{nullptr}, discard{nullptr};
            RtSystem_String_SubString(&work, &lit, segStart, i - 1);
            RtSystem_String_AppendAssign(ret, &discard, &lit);
            RtString_ReleaseInline(&discard);
            RtString_ReleaseInline(&lit);
        }

        const int digit = static_cast<int>(static_cast<signed char>(fmt->data[i + 1])) - '0';
        String discard{nullptr};
        RtSystem_String_AppendAssign(ret, &discard, &params[digit]);
        RtString_ReleaseInline(&discard);

        i += 2;
        segStart = i;
    }

    if (segStart != i) {                              // trailing literal run
        String lit{nullptr}, discard{nullptr};
        RtSystem_String_SubString(&work, &lit, segStart, i - 1);
        RtSystem_String_AppendAssign(ret, &discard, &lit);
        RtString_ReleaseInline(&discard);
        RtString_ReleaseInline(&lit);
    }

    RtString_ReleaseInline(&work);

    // A real oracle: both inputs are const and our body's only net effect on
    // them is an AddRef/Release pair that cancels, so the shipped body can be
    // replayed on the same `fmt`/`params` into an independent result. Guarded
    // against `ret` aliasing an input for the same reason SubString above is
    // (reference_oracle_must_snapshot_aliased_inputs.md): if it ever did, ours
    // would have destroyed the input before theirs ran and the oracle would
    // manufacture a divergence rather than find one.
    if (rt_diff_on__ && ret != fmt && ret != params) {
        String theirs{nullptr};
        RtOriginal<String* (*)(String*, const String*, const String*)>(
            "?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z")(&theirs, fmt, params);
        const bool equal =
            ((ret->data == nullptr) == (theirs.data == nullptr)) &&
            (ret->data == nullptr ||
             (StringLength(ret->data) == StringLength(theirs.data) &&
              std::strcmp(ret->data, theirs.data) == 0));
        RT_DIFF_SHADOW(equal, ret->data ? ret->data : "(null)",
                       theirs.data ? theirs.data : "(null)");
        RtString_ReleaseInline(&theirs);
    }
    return ret;
}

String* RtSystem_String_FormatIndexed(String* ret, const char* fmt, ...) {
    if (fmt == nullptr) {
        ret->data = nullptr;
        return ret;
    }

    // Called DIRECTLY now that both are ported below — not through
    // RT_ORIGINAL_ONCE (which would be a port bypass and a GetProcAddress) and
    // not through RT_EXPORT_ONCE either, since the callee is in this same
    // translation unit and needs no resolution at all.
    const int paramCount = RtSystem_String_FormatParamCount(fmt);

    if (paramCount == 0) {
        ret->data = nullptr;
        RtSystem_String_Set(ret, fmt);
        return ret;
    }

    StringArray params{nullptr, 0, 0};
    StringArray_SetCountFromEmpty(&params, paramCount);

    va_list ap;
    va_start(ap, fmt);
    for (int i = 0; i < paramCount; ++i) {
        // Retail walks the register home slots forward from the format
        // string's; every vararg is a `const char*`.
        String tmp{nullptr};
        RtSystem_String_Set(&tmp, va_arg(ap, const char*));
        RtSystem_String_Assign(&params.data[i], &tmp);
        RtString_ReleaseInline(&tmp);
    }
    va_end(ap);

    String fmtStr{nullptr};
    RtSystem_String_Set(&fmtStr, fmt);
    RtSystem_String_FormatParamList(ret, &fmtStr, params.data);
    RtString_ReleaseInline(&fmtStr);

    StringArray_DestroyRange(params.data, params.count);
    if (params.data != nullptr)
        RtMemFree(params.data);
    return ret;
}

// ?Release@String@System@@AEAAXXZ  retail 0x1800c9870
//
// The private release primitive, byte-for-byte the sequence String.h documents
// and RtString_ReleaseInline already implements -- including the asymmetry that
// the freeing branch leaves `data - 8` in the object while the sharing branch
// nulls it. This export is those same eleven instructions; it is registered so
// that the name is not missing at cutover, and so our callers stop reaching the
// DLL for a routine this file has had all along.
//
// NO ORACLE, and the reason is intrinsic rather than a gap in the harness: the
// refcount lives in the one shared block, so running the shipped body after
// ours would decrement a second time and free a live string. There is nothing
// to snapshot and restore -- a second String object referring to the same block
// IS the same counter. The shadow below is our own post-condition (the object
// is either nulled or holds block-8), which is a weaker claim than a diff and
// is labelled as one.
void RtSystem_String_Release(String* self) {
    RT_DIFF_BEGIN("System::String::Release [no oracle: mutates a shared refcount]");
    char* before = self->data;
    RtString_ReleaseInline(self);
    const bool ok = (before == nullptr) ? (self->data == nullptr)
                                        : (self->data == nullptr || self->data == before - 8);
    RT_DIFF_SHADOW(ok, "released", "not diffable");
}

// ?ToInteger@String@System@@SAHPEBD@Z  retail 0x18003f620
//
// A hand-rolled atoi over `int`: skip ' ' and '\t', one optional '+'/'-', then
// digits accumulated as `v = v*10 + (c - '0')` -- retail spells that
// `EDX = c + 2*(5*EDX - 24)` (0x18003f664..0x18003f670), which is the same
// value.
//
// The digit test is `CMP AL,0x30 / JL` and `CMP AL,0x39 / JG` on a byte loaded
// with MOVZX and compared SIGNED, so any byte >= 0x80 reads as negative and
// ends the number. Reproduced with a signed char rather than an isdigit(), so a
// UTF-8 continuation byte terminates it the same way it does in the DLL.
//
// No overflow handling and no errno: past INT_MAX retail simply wraps, and so
// do we.
int RtSystem_String_ToInteger(const char* s) {
    RT_DIFF_BEGIN("System::String::ToInteger");
    int result = 0;
    if (s != nullptr) {
        const char* p = s;
        while (*p == ' ' || *p == '\t') ++p;
        int sign = 1;
        if (*p == '-') { sign = -1; ++p; }
        else if (*p == '+') { ++p; }
        while (static_cast<signed char>(*p) >= '0' && static_cast<signed char>(*p) <= '9') {
            result = result * 10 + (static_cast<int>(static_cast<signed char>(*p)) - '0');
            ++p;
        }
        result *= sign;
    }
    using Fn = int (*)(const char*);
    RT_DIFF_SCALAR(result, RtOriginal<Fn>("?ToInteger@String@System@@SAHPEBD@Z")(s));
    return result;
}

// ============================================================
// Serialize — NOT DIFFED (consumes / produces stream bytes)
// ============================================================
// Running the original as an oracle would read or write the stream a second
// time and desync every record after it — the exact failure mode of the memory
// note reference_slot3_wrapper_must_resolve_first.md, manufactured on purpose.

namespace {
// Minimal views of IO::Serializer / IO::Stream / IO::Encoding. Deliberately
// local rather than including engine/types/io_serializer.h, which belongs to
// the exe port; the offsets are the same and are static_asserted there.
struct RtEncoding { void** vtable; };
struct RtStream   { void** vtable; int32_t loadingFlag; int32_t pad; void* encoding; };
struct RtSerializer { RtStream* stream; RtEncoding* encoding; };

// Encoding vtable byte offsets used here.
constexpr int kEncSerializeCharArr = 0x18;  // (count, char*,   name)
constexpr int kEncSerializeU16Arr  = 0x50;  // (count, ushort*, name)
constexpr int kEncSerializeUInt    = 0x98;  // (uint&,          name)

inline void EncCallUInt(RtEncoding* enc, uint32_t* v) {
    using Fn = void (*)(RtEncoding*, uint32_t*, const char*);
    reinterpret_cast<Fn>(
        *reinterpret_cast<void**>(reinterpret_cast<char*>(enc->vtable) + kEncSerializeUInt))(
        enc, v, nullptr);
}
inline void EncCallArr(RtEncoding* enc, int slot, uint64_t count, void* p) {
    using Fn = void (*)(RtEncoding*, uint64_t, void*, const char*);
    reinterpret_cast<Fn>(
        *reinterpret_cast<void**>(reinterpret_cast<char*>(enc->vtable) + slot))(
        enc, count, p, nullptr);
}

// ---- the legacy UTF-16 branch --------------------------------------------
// Retail decodes with FUN_1800c9fc0(1, p, end, &cp, 1) and re-encodes with
// FUN_1800c9b60(cp, dst, 2). Both are MSVC-CRT jump-table dispatchers indexed
// by an encoding id (1 = UTF-16, 2 = UTF-8) whose tables Ghidra cannot recover
// ("Could not recover jumptable ... Too many branches"), so THIS PAIR IS THE
// ONE PIECE OF THIS LANE THAT IS NOT TRANSCRIBED FROM RETAIL — it is a
// standard UTF-16LE decoder and UTF-8 encoder written from the specification.
//
// The branch is reachable only when READING a string whose serialized length
// word has bit 31 clear. Every write path sets that bit unconditionally
// (`BTS EAX,0x1f` at 0x1800c9959), so it can only fire on data produced by a
// build older than the one that added the flag. It is not expected to run at
// all, and it says so on stderr the first time it does.
int Utf16DecodeOne(const uint8_t* p, const uint8_t* end, int32_t* cp) {
    if (p + 2 > end) {
        *cp = -1;
        return 2;
    }
    const uint16_t w = static_cast<uint16_t>(p[0] | (p[1] << 8));
    if (w >= 0xD800 && w < 0xDC00 && p + 4 <= end) {
        const uint16_t w2 = static_cast<uint16_t>(p[2] | (p[3] << 8));
        if (w2 >= 0xDC00 && w2 < 0xE000) {
            *cp = 0x10000 + (((w - 0xD800) << 10) | (w2 - 0xDC00));
            return 4;
        }
    }
    *cp = w;
    return 2;
}

int Utf8EncodeOne(int32_t cp, char* dst) {
    const uint32_t c = static_cast<uint32_t>(cp);
    if (c < 0x80) { dst[0] = static_cast<char>(c); return 1; }
    if (c < 0x800) {
        dst[0] = static_cast<char>(0xC0 | (c >> 6));
        dst[1] = static_cast<char>(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        dst[0] = static_cast<char>(0xE0 | (c >> 12));
        dst[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        dst[2] = static_cast<char>(0x80 | (c & 0x3F));
        return 3;
    }
    dst[0] = static_cast<char>(0xF0 | (c >> 18));
    dst[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    dst[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    dst[3] = static_cast<char>(0x80 | (c & 0x3F));
    return 4;
}
}  // namespace

// ?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800c9930
void RtSystem_String_Serialize(String* self, void* serializerRaw) {
    RtSerializer* ser = static_cast<RtSerializer*>(serializerRaw);
    RtEncoding* enc = ser->encoding;

    // Length INCLUDING the terminator, with bit 31 as the "UTF-8 payload"
    // flag. On save this is what goes out; on load the call overwrites it.
    uint32_t n = self->data ? static_cast<uint32_t>(StringLength(self->data) + 1) : 0u;
    n |= 0x80000000u;
    EncCallUInt(enc, &n);

    const bool legacyWide = ((n >> 31) & 1u) == 0u;
    n &= 0x7fffffffu;

    // Stream +0x08: 0 = loading/decode, non-zero = saving/encode.
    const bool loading = (ser->stream->loadingFlag == 0);
    if (loading && n != 0)
        RtString_SetLength(self, static_cast<int>(n) - 1);

    if (legacyWide && loading && n != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr,
                "[RtString] legacy UTF-16 string in stream (length word bit 31 clear); "
                "converting with the spec decoder, not a transcription of the CRT helper\n");
        }

        uint8_t* buf = static_cast<uint8_t*>(RtMemAlloc(static_cast<uint64_t>(n) * 2, 4));
        EncCallArr(enc, kEncSerializeU16Arr, n, buf);

        // Retail stops two bytes short: the trailing UTF-16 NUL is not decoded.
        uint8_t* const end = buf + (static_cast<int>(n) * 2 - 2);

        int total = 0;
        for (uint8_t* p = buf; p < end;) {
            int32_t cp = -1;
            p += Utf16DecodeOne(p, end, &cp);
            if (cp >= 0) {
                char scratch[4];
                total += Utf8EncodeOne(cp, scratch);
            }
        }

        RtString_SetLength(self, total);
        self->data[total] = '\0';

        char* const base = self->data;
        char* out = base;
        int written = 0;
        for (uint8_t* p = buf; p < end;) {
            int32_t cp = -1;
            p += Utf16DecodeOne(p, end, &cp);
            if (cp < 0)
                continue;
            char scratch[4];
            const int m = Utf8EncodeOne(cp, scratch);
            written += m;
            if (written > total || base == nullptr || m <= 0)
                continue;
            std::memcpy(out, scratch, static_cast<size_t>(m));
            out += m;
        }

        RtMemFree(buf);
        return;
    }

    EncCallArr(enc, kEncSerializeCharArr, n, self->data);
}

}  // namespace RtSystem

// ============================================================
// Registration
// ============================================================
int RtSystemStringRegister() {
    using namespace RtSystem;
    int n = 0;

    RegisterRuntimeExport("??0String@System@@QEAA@XZ",              (void*)&RtSystem_String_Ctor); ++n;
    RegisterRuntimeExport("??0String@System@@QEAA@PEBD@Z",          (void*)&RtSystem_String_CtorCStr); ++n;
    RegisterRuntimeExport("??0String@System@@QEAA@AEBV01@@Z",       (void*)&RtSystem_String_CtorCopy); ++n;
    RegisterRuntimeExport("??1String@System@@QEAA@XZ",              (void*)&RtSystem_String_Dtor); ++n;
    RegisterRuntimeExport("??4String@System@@QEAAAEAV01@AEBV01@@Z", (void*)&RtSystem_String_Assign); ++n;
    RegisterRuntimeExport("??8String@System@@QEBA_NAEBV01@@Z",      (void*)&RtSystem_String_Equals); ++n;
    RegisterRuntimeExport("??8String@System@@QEBA_NPEBD@Z",         (void*)&RtSystem_String_EqualsCStr); ++n;
    RegisterRuntimeExport("??MString@System@@QEBA_NAEBV01@@Z",      (void*)&RtSystem_String_Less); ++n;
    RegisterRuntimeExport("??YString@System@@QEAA?AV01@AEBV01@@Z",  (void*)&RtSystem_String_AppendAssign); ++n;
    RegisterRuntimeExport("??YString@System@@QEAA?AV01@PEBD@Z",     (void*)&RtSystem_String_AppendAssignCStr); ++n;
    RegisterRuntimeExport("?Set@String@System@@QEAAXPEBD@Z",        (void*)&RtSystem_String_Set); ++n;
    RegisterRuntimeExport("?SubString@String@System@@QEBA?AV12@HH@Z", (void*)&RtSystem_String_SubString); ++n;
    RegisterRuntimeExport("?ToFloat@String@System@@SAMPEBD@Z",      (void*)&RtSystem_String_ToFloat); ++n;
    // Wave 9 lane B3 -- the four raw-door rows.
    RegisterRuntimeExport("?ToInteger@String@System@@SAHPEBD@Z",    (void*)&RtSystem_String_ToInteger); ++n;
    RegisterRuntimeExport("?Release@String@System@@AEAAXXZ",        (void*)&RtSystem_String_Release); ++n;
    RegisterRuntimeExport("?FormatParamCount@String@System@@SAHPEBD@Z",
                          (void*)&RtSystem_String_FormatParamCount); ++n;
    RegisterRuntimeExport("?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z",
                          (void*)&RtSystem_String_FormatParamList); ++n;
    RegisterRuntimeExport("?Format@String@System@@SA?AV12@PEBDZZ",  (void*)&RtSystem_String_Format); ++n;
    RegisterRuntimeExport("?FormatIndexed@String@System@@SA?AV12@PEBDZZ",
                          (void*)&RtSystem_String_FormatIndexed); ++n;
    RegisterRuntimeExport("?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtSystem_String_Serialize); ++n;
    // The two free operator+ overloads. They live in namespace System, not in
    // the class, so the mangling has no class component and the hidden return
    // slot is the FIRST argument.
    RegisterRuntimeExport("??HSystem@@YA?AVString@0@AEBV10@0@Z",
                          (void*)&RtSystem_String_Concat); ++n;
    RegisterRuntimeExport("??HSystem@@YA?AVString@0@AEBV10@PEBD@Z",
                          (void*)&RtSystem_String_ConcatCStr); ++n;

    // The SAME functions by Ghidra address. Half the exe's String call sites
    // resolve by name with an `if (!fn) ResolveAddr(Module::Runtime, addr)`
    // fallback (engine/funcs/engine_funcs.cpp:1649-1698), and ResolveAddr
    // consults its own registry (standalone/resolve_standalone.cpp:317).
    // Registering only the names would leave any site whose name lookup failed
    // silently on the DLL — which is not hypothetical: see the two dead
    // Filename manglings in filename.cpp's registration block.
    RegisterRuntimeAddr(0x180006760, (void*)&RtSystem_String_Ctor);
    RegisterRuntimeAddr(0x18000b630, (void*)&RtSystem_String_CtorCStr);
    RegisterRuntimeAddr(0x180006cb0, (void*)&RtSystem_String_CtorCopy);
    RegisterRuntimeAddr(0x180006c80, (void*)&RtSystem_String_Dtor);
    RegisterRuntimeAddr(0x1800c9810, (void*)&RtSystem_String_Assign);
    RegisterRuntimeAddr(0x1800063d0, (void*)&RtSystem_String_Equals);
    RegisterRuntimeAddr(0x180053c00, (void*)&RtSystem_String_EqualsCStr);
    RegisterRuntimeAddr(0x180029360, (void*)&RtSystem_String_Less);
    RegisterRuntimeAddr(0x1800c8a90, (void*)&RtSystem_String_AppendAssign);
    RegisterRuntimeAddr(0x1800c9120, (void*)&RtSystem_String_AppendAssignCStr);
    RegisterRuntimeAddr(0x1800c95d0, (void*)&RtSystem_String_Set);
    RegisterRuntimeAddr(0x1800c9230, (void*)&RtSystem_String_SubString);
    RegisterRuntimeAddr(0x1800c8070, (void*)&RtSystem_String_ToFloat);
    RegisterRuntimeAddr(0x1800c9730, (void*)&RtSystem_String_Format);
    RegisterRuntimeAddr(0x1800c8b40, (void*)&RtSystem_String_FormatIndexed);
    RegisterRuntimeAddr(0x1800c9930, (void*)&RtSystem_String_Serialize);
    RegisterRuntimeAddr(0x1800c9440, (void*)&RtSystem_String_Concat);
    RegisterRuntimeAddr(0x1800c92d0, (void*)&RtSystem_String_ConcatCStr);

    printf("[RtPort]   System::String: %d exports registered\n", n);
    return n;
}
