// ============================================================================
// runtime/src/io/memory_stream_probe.cpp — rung 4b-ii's oracle
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// MemoryStream's six virtuals are pure functions of {_data, _cursor, _size} and
// their arguments, so unlike rung 2 they replay cleanly — but only if the two
// sides get INDEPENDENT state. Ours and theirs each get their own MemoryStream
// over their own copy of the same buffer, and both are stepped in LOCKSTEP with
// the full stream state compared after EVERY call, not once at the end (lane
// PICK's rule). Comparing only the returned bool would miss a cursor that
// advanced by the wrong amount and then self-corrected.
//
// WHAT IS COMPARED: the return value, _cursor, _size, and — for Read — the
// bytes actually delivered. For Write, the DESTINATION BUFFER is compared,
// because a Write that returns true and copies nothing is otherwise
// indistinguishable from a correct one.
//
// THE EFFECTIVENESS CONTROL COUNTS OUTCOMES, NOT CALLS. Each of the two bulk
// bodies has exactly two: the copy and the refusal. A corpus that only ever
// succeeds leaves the bound check unentered while reporting green — this lane
// got that wrong twice in rung 2 — so the corpus deliberately contains reads
// and writes that run off the end, and the verdict FAILS if either outcome is
// unseen.
//
// THE BOUND CASES ARE CHOSEN TO STRADDLE THE 32-BIT SIGNED COMPARE that the
// port transcribes rather than "cleans up": exactly-at-end (must succeed),
// one-past-end (must fail), and a seek beyond _size followed by a read (the
// seek must succeed, the read must fail, because SetCurrentPos has no bounds
// check of its own).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIo {

extern "C" bool     RtIo_MemoryStream_Read(MemoryStream*, int32_t, void*);
extern "C" bool     RtIo_MemoryStream_Write(MemoryStream*, int32_t, const void*);
extern "C" bool     RtIo_MemoryStream_SetCurrentPos(MemoryStream*, int32_t);
extern "C" uint32_t RtIo_MemoryStream_GetCurrentPos(const MemoryStream*);
extern "C" uint32_t RtIo_MemoryStream_GetSize(const MemoryStream*);

namespace {

typedef bool     (*ReadFn)(MemoryStream*, int32_t, void*);
typedef bool     (*WriteFn)(MemoryStream*, int32_t, const void*);
typedef bool     (*SeekFn)(MemoryStream*, int32_t);
typedef uint32_t (*PosFn)(const MemoryStream*);

struct Res {
    int64_t calls = 0, copied = 0, refused = 0, seeks = 0, getters = 0;
    const char* bad = nullptr;
};

// A MemoryStream built by hand rather than through the ctor: the ctor allocates
// an Encoding and is NOT part of this rung, and the six bodies under test never
// touch anything but _data / _cursor / _size. Building it by hand also keeps the
// probe from depending on the very constructor 4b-ii has not ported yet.
void MakeStream(MemoryStream* s, void* buf, uint32_t size) {
    std::memset(s, 0, sizeof(MemoryStream));
    s->data   = buf;
    s->cursor = 0;
    s->size   = size;
}

// Compare the two streams AND the two output buffers after a single step.
void Step(Res* r, bool okA, bool okB, const MemoryStream* a, const MemoryStream* b,
          const void* outA, const void* outB, size_t outLen) {
    ++r->calls;
    if (r->bad) return;
    if (okA != okB)                     { r->bad = "return value";  return; }
    if (a->cursor != b->cursor)         { r->bad = "_cursor";       return; }
    if (a->size   != b->size)           { r->bad = "_size";         return; }
    if (outLen && std::memcmp(outA, outB, outLen) != 0)
                                        { r->bad = "bytes";         return; }
    if (okA) ++r->copied; else ++r->refused;
}

}  // namespace

void RtIo_MemoryStreamProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::MemoryStream [probe]");

    ReadFn  theirRead = reinterpret_cast<ReadFn>(ResolveAddr(Module::Runtime, 0x1800ae950));
    WriteFn theirWrite= reinterpret_cast<WriteFn>(ResolveAddr(Module::Runtime, 0x1800ae8f0));
    SeekFn  theirSeek = reinterpret_cast<SeekFn>(ResolveAddr(Module::Runtime, 0x1800ae9b0));
    PosFn   theirPos  = reinterpret_cast<PosFn>(ResolveAddr(Module::Runtime, 0x1800ae9c0));
    PosFn   theirSize = reinterpret_cast<PosFn>(ResolveAddr(Module::Runtime, 0x1800ae9d0));
    if (!theirRead || !theirWrite || !theirSeek || !theirPos || !theirSize) {
        printf("[MemStream] probe SKIPPED: no shipped bodies to diff against "
                    "(expected once the DLL is gone).\n");
        return;
    }

    Res r;
    const uint32_t kN = 256;
    uint8_t srcA[kN], srcB[kN], dstA[kN], dstB[kN];
    for (uint32_t i = 0; i < kN; ++i) srcA[i] = srcB[i] = static_cast<uint8_t>(i * 7 + 3);

    MemoryStream a, b;

    // ---- READ: a walk that ends EXACTLY at _size, then one byte too far ----
    MakeStream(&a, srcA, kN); MakeStream(&b, srcB, kN);
    static const int32_t kChunks[] = { 1, 3, 60, 64, 128 };   // sums to 256 exactly
    for (int i = 0; i < 5; ++i) {
        std::memset(dstA, 0xA5, kN); std::memset(dstB, 0xA5, kN);
        const bool okA = RtIo_MemoryStream_Read(&a, kChunks[i], dstA);
        const bool okB = theirRead(&b, kChunks[i], dstB);
        Step(&r, okA, okB, &a, &b, dstA, dstB, kN);
    }
    {   // now at end: one more byte must be REFUSED by both
        std::memset(dstA, 0xA5, kN); std::memset(dstB, 0xA5, kN);
        const bool okA = RtIo_MemoryStream_Read(&a, 1, dstA);
        const bool okB = theirRead(&b, 1, dstB);
        Step(&r, okA, okB, &a, &b, dstA, dstB, kN);
    }

    // ---- SEEK past the end (must SUCCEED — there is no bounds check) then
    //      READ (must FAIL). Proves the refusal lives in Read, not in the seek.
    {
        const bool sA = RtIo_MemoryStream_SetCurrentPos(&a, static_cast<int32_t>(kN) + 64);
        const bool sB = theirSeek(&b, static_cast<int32_t>(kN) + 64);
        Step(&r, sA, sB, &a, &b, nullptr, nullptr, 0);
        ++r.seeks;
        std::memset(dstA, 0xA5, kN); std::memset(dstB, 0xA5, kN);
        const bool okA = RtIo_MemoryStream_Read(&a, 1, dstA);
        const bool okB = theirRead(&b, 1, dstB);
        Step(&r, okA, okB, &a, &b, dstA, dstB, kN);
    }

    // ---- WRITE: rewind, fill, then overrun. The destination buffers are the
    //      thing compared, so a Write that returns true and copies nothing
    //      cannot pass.
    {
        std::memset(dstA, 0, kN); std::memset(dstB, 0, kN);
        MakeStream(&a, dstA, kN); MakeStream(&b, dstB, kN);
        const bool sA = RtIo_MemoryStream_SetCurrentPos(&a, 0);
        const bool sB = theirSeek(&b, 0);
        Step(&r, sA, sB, &a, &b, nullptr, nullptr, 0);
        ++r.seeks;
        for (int i = 0; i < 5; ++i) {
            const bool okA = RtIo_MemoryStream_Write(&a, kChunks[i], srcA);
            const bool okB = theirWrite(&b, kChunks[i], srcB);
            Step(&r, okA, okB, &a, &b, dstA, dstB, kN);
        }
        const bool okA = RtIo_MemoryStream_Write(&a, 1, srcA);   // one too far
        const bool okB = theirWrite(&b, 1, srcB);
        Step(&r, okA, okB, &a, &b, dstA, dstB, kN);
    }

    // ---- the two getters, at a non-zero cursor so a stub returning 0 fails --
    {
        RtIo_MemoryStream_SetCurrentPos(&a, 77);
        theirSeek(&b, 77);
        const bool posOk  = RtIo_MemoryStream_GetCurrentPos(&a) == theirPos(&b);
        const bool sizeOk = RtIo_MemoryStream_GetSize(&a)       == theirSize(&b);
        r.getters += 2;
        ++r.calls;
        if (!r.bad && !posOk)  r.bad = "GetCurrentPos";
        if (!r.bad && !sizeOk) r.bad = "GetSize";
    }

    // Effectiveness control: BOTH outcomes of the two bulk bodies must occur.
    // `calls` alone would pass a corpus that never once hit the bound check.
    const bool covered = r.copied > 0 && r.refused > 0 && r.seeks > 0 && r.getters > 0;
    const bool pass    = (r.bad == nullptr) && covered;

    char sa[192], sb[192];
    std::snprintf(sa, sizeof(sa), "calls=%lld copied=%lld refused=%lld seeks=%lld getters=%lld",
                  (long long)r.calls, (long long)r.copied, (long long)r.refused,
                  (long long)r.seeks, (long long)r.getters);
    if (r.bad) std::snprintf(sb, sizeof(sb), "DIVERGED on '%s'", r.bad);
    else if (!covered) std::snprintf(sb, sizeof(sb), "no divergence, but an OUTCOME READS ZERO");
    else std::snprintf(sb, sizeof(sb), "identical on return, _cursor, _size and every byte");
    RT_DIFF_SHADOW(pass, sa, sb);
    RT_DIFF_WORK(r.calls > 0);

    printf("[MemStream] lockstep probe: %lld calls (copied=%lld refused=%lld "
                "seeks=%lld getters=%lld) -- %s\n",
                (long long)r.calls, (long long)r.copied, (long long)r.refused,
                (long long)r.seeks, (long long)r.getters, sb);
}

}  // namespace RtIo
