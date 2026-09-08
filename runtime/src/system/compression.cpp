// ============================================================================
// runtime/src/system/compression.cpp — Wave 9 lane PKG2, iteration 2
// System::Compression::Decompress — the bottom of the ReadPackfile stack
// ============================================================================
//
//   | Export                                            | Retail      | Ord  | Instrs |
//   |---------------------------------------------------|-------------|------|--------|
//   | ?Decompress@Compression@System@@SAHPEBXIPEAXI@Z   | 0x1800c4660 | 2150 |   72   |
//
// Name and ordinal printed out of Runtime-x64.dll's own export directory with
// `pefile`, not transcribed. `tools/export_name_audit.py` re-checks it.
//
// Kill switch: BANISHED_RT_COMPRESSION=0 leaves the call on the DLL.
//
// ---------------------------------------------------------------------------
// 1. WHY THIS FUNCTION, AND WHY FIRST
// ---------------------------------------------------------------------------
// `?ReadPackfile@PackageFile@IO@@` (0x1800b46b0) is **1,864 instructions**, the
// largest single body of the wave. Reading its callee set explains the size and
// dictates the order — see docs/runtime_port/WAVE9_READPACKFILE_PLAN.md. The
// short version: ReadPackfile is not 1,864 instructions of novel logic, it is a
// header/directory parser in which MSVC **inlined `CompressedStream::Read`
// twenty-five times**, and the whole of it stands on an `IO::CompressedStream`
// read path that nothing in this tree has ported.
//
// `Compression::Decompress` is the bottom of that stack. It is 72 instructions,
// it is PURE, it calls nothing, and it is the function whose correctness
// decides **every byte of every package the game loads**. A silent defect here
// is not a crash; it is subtly wrong resource data.
//
// It is also the only member of the stack with a *perfect* oracle available
// today — see §3.
//
// ---------------------------------------------------------------------------
// 2. THE CODEC, INSTRUCTION BY INSTRUCTION
// ---------------------------------------------------------------------------
// A bespoke LZ77/LZSS byte codec. **No zlib, no LZ4, no external library** —
// which is worth stating because "the package format is compressed" invites the
// assumption that a library is involved and that assumption would have sent
// this lane looking for one.
//
//   static int Decompress(const void* src, unsigned srcLen,
//                         void* dst, unsigned dstCap)
//
// Signature read off the mangled name AND off the registers, which agree:
// `SAH` = static, returns int; `PEBX I PEAX I` = (const void*, unsigned, void*,
// unsigned) = RCX, EDX, R8, R9D.
//
//   1800c466a  MOV EDI,EDX / ADD RDI,RCX      srcEnd = src + srcLen
//   1800c466c  MOV R11D,R9D / ADD R11,R8      dstEnd = dst + dstCap
//   1800c4675  R10 = sp (read cursor), RAX = op (write cursor), RBX = dst
//
// Both length arguments are moved through a 32-bit register first (`MOV EDI,EDX`,
// `MOV R11D,R9D`), i.e. **zero-extended**, then added to a 64-bit pointer. They
// are unsigned and cannot go negative; transcribed as uint32 -> uint64 for that
// reason and not as a sign-extended int.
//
//   loop:
//     c = *sp++
//     if (c < 0x20) {                                     // LITERAL RUN
//         n = c + 1;                                      // 1..32
//         if (op + n > dstEnd) return 0;                  // 1800c4696
//         copy n bytes sp -> op
//     } else {                                            // MATCH
//         len = c >> 5;                                   // 1..7
//         mp  = op - ((c & 0x1f) << 8) - 1;
//         if (len == 7) len = 7 + *sp++;                  // 7..262
//         mp -= *sp++;                                    // the low offset byte
//         if (op + 2 + len > dstEnd) return 0;            // 1800c46f3
//         if (mp < dst)            return 0;              // 1800c46f8, UNSIGNED (JC)
//         copy 2 + len bytes mp -> op, ONE BYTE AT A TIME
//     }
//   while (sp < srcEnd);                                  // 1800c4727, UNSIGNED (JC)
//   return (int)(op - dst);
//
// FOUR THINGS THAT ARE EASY TO GET WRONG AND ARE LOAD-BEARING:
//
//  1. **The match copy MUST be byte-at-a-time.** `mp` and `op` overlap whenever
//     the distance is less than the length, and retail's loop reads each byte
//     *after* the previous one was written — that is the RLE idiom every LZ77
//     depends on. A `memcpy` or `memmove` here compiles, runs, and produces
//     different bytes for exactly the inputs that compress best. The two
//     unrolled bytes at 0x1800c46fa..0x1800c470e are part of the same chain and
//     are transcribed in place rather than folded into the loop.
//
//  2. **Total copied is `2 + len`, and the loop runs `len` times.** Two bytes
//     are copied unrolled before the `do { } while (--len)`, which is why the
//     bounds check is `op + 2 + len` and why the minimum match is 3 bytes.
//
//  3. **The loop is a do-while: `srcLen == 0` still reads one control byte.**
//     There is no test before the first iteration (0x1800c4678 falls straight
//     into 0x1800c4680); the only test is at the bottom. That is a one-byte read
//     past the end of a zero-length buffer, and it is retail's behaviour, not a
//     transcription slip. It is NOT guarded here — the project rule is that a
//     faithful unguarded read stays faithful, and a guard would change what the
//     function returns rather than protect anything (the caller always passes a
//     non-empty window). The probe in §3 never passes srcLen == 0.
//
//  4. **`mp < dst` is an UNSIGNED compare (`JC`, not `JL`).** With `mp`
//     computed by subtraction it can wrap below the buffer; the unsigned form is
//     what makes that wrap a rejection rather than a wild read.
//
// Distance is `((c & 0x1f) << 8) + lo + 1`, so 1..8192 — an 8 KB window, which
// is consistent with CompressedStream's 32 KB (0x8000) page size.
//
// ---------------------------------------------------------------------------
// 3. THE ORACLE — THREE INDEPENDENT CHECKS, AND VALID INPUTS BY CONSTRUCTION
// ---------------------------------------------------------------------------
// Nothing in our tree calls Decompress today: its only caller is the DLL's own
// `CompressedStream::DecompressFromSource`, and a `RegisterRuntimeExport` does
// not redirect the DLL's internal direct CALLs. So registering it alone would
// report `calls=0` — the failure this project keeps paying for
// (reference_uncalled_port_is_not_verified.md). It needs a driver, and it gets
// a good one.
//
// `?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z` (0x1800c4230, ord 1749) is
// exported. So the probe **compresses a real buffer with the DLL's own
// compressor and decompresses it with both implementations**, which gives:
//
//   A. ours == theirs, on the returned length AND on every byte written;
//   B. ours round-trips: the decompressed bytes equal the ORIGINAL input.
//      This check does not involve the DLL's Decompress at all, so a shared
//      misreading of the format cannot pass it;
//   C. the three failure exits return 0 on both sides.
//
// Check B is the one that matters most. A and B together mean a defect would
// have to be present in our port, absent from the DLL's, and still reproduce
// the original bytes — which is to say there is no such defect.
//
// EFFECTIVENESS CONTROL (reference_probe_needs_an_effectiveness_control.md).
// A codec probe is exactly where a uniform population hides: feed it garbage and
// almost every case returns 0 from the same bounds check, and 5,000 agreeing
// zeroes read as a thorough test. So the port counts which branch each case
// actually took — literal run, short match, extended-length match, and each of
// the three failure exits — and the probe prints all six. **A bucket that reads
// 0 is reported as uncovered rather than averaged into a pass**, and the
// corpus is built to fill all six on purpose (§4).
//
// ---------------------------------------------------------------------------
// 4. THE CORPUS
// ---------------------------------------------------------------------------
// Compressible-real, incompressible-real, and pathological, because the three
// exercise different branches:
//
//   * **Runtime-x64.dll's own loaded image**, in sliding windows. Real, highly
//     non-uniform data — x86 code, ASCII string tables, relocation tables, long
//     zero runs — and it is already mapped, so reading it needs no file I/O, no
//     new Win32 API and no new import (this file adds neither). The zero runs
//     drive long overlapping matches; the code bytes drive literal runs.
//   * **Synthetic RLE**: a single repeated byte, which forces the
//     extended-length branch (`len == 7`, read another byte) and maximal
//     overlap, the case a memcpy-based port gets wrong.
//   * **Synthetic incompressible**: a counter-derived pattern with no repeats,
//     which forces literal runs almost exclusively.
//   * **Malformed and tight-buffer cases**: RAW (uncompressed) bytes fed in as
//     `src` to make the decoder walk nonsense, and valid streams given a `dstCap`
//     one byte short of what they need — that is what fills the failure buckets
//     deliberately rather than by luck.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"

namespace RtSystem {

// Branch counters. Null on every normal call — the probe points this at its own
// storage for the duration of the sweep, so the shipped path pays one
// predictable null test and nothing else.
// The first three count branches ENTERED, not branches that succeeded — all
// three are incremented before that branch's bounds checks, so
// `literalRun + matchShort + matchExtended` is the total control bytes decoded
// and the three fail counters are a disjoint tally of how those entries ended.
// Counting entries for two of them and successes for the third would make the
// printed line quietly incomparable across buckets.
struct CompressionBranchCounts {
    int64_t literalRun    = 0;   // c < 0x20
    int64_t matchShort    = 0;   // c >= 0x20, len != 7
    int64_t matchExtended = 0;   // c >= 0x20, len == 7 (reads an extra byte)
    int64_t failLiteral   = 0;   // op + n > dstEnd
    int64_t failMatchDst  = 0;   // op + 2 + len > dstEnd
    int64_t failMatchSrc  = 0;   // mp < dst
    // How each successful decode LEFT THE READ CURSOR — see the note on the
    // lossy-transform hazard below.
    int64_t exitExact     = 0;   // sp landed exactly on srcEnd
    int64_t exitOvershoot = 0;   // sp landed PAST srcEnd (a run straddled the end)
};
static CompressionBranchCounts* g_branch = nullptr;

// The oracle's scratch size. A live CompressedStream window is 0x8000; 0x10000
// leaves headroom and still fits comfortably in .bss.
static constexpr uint32_t kOracleScratch = 0x10000;

namespace {

typedef int (*DecompressFn)(const void*, uint32_t, void*, uint32_t);

// The body, factored out so the oracle can run the identical code onto scratch.
int DecompressBody(const void* src, uint32_t srcLen, void* dst, uint32_t dstCap) {
    const uint8_t* sp          = static_cast<const uint8_t*>(src);
    const uint8_t* const srcEnd = sp + srcLen;          // 1800c4672, zero-extended
    uint8_t* const dstBase      = static_cast<uint8_t*>(dst);
    uint8_t* const dstEnd       = dstBase + dstCap;     // 1800c467b, zero-extended
    uint8_t* op                 = dstBase;

    // do-while: retail has NO test before the first iteration (§2 note 3).
    do {
        const uint32_t c = *sp++;                       // 1800c4680
        if (c < 0x20u) {                                // 1800c468a  JNC -> match
            // ---- literal run -------------------------------------------
            if (g_branch) ++g_branch->literalRun;
            uint32_t n = c + 1u;                        // 1800c468c
            if (op + n > dstEnd) {                      // 1800c4696  JA
                if (g_branch) ++g_branch->failLiteral;
                return 0;
            }
            do {                                        // 1800c46a0
                *op++ = *sp++;
            } while (--n);
        } else {
            // ---- match --------------------------------------------------
            uint32_t len = c >> 5;                      // 1800c46bf   1..7
            // mp = op - ((c & 0x1f) << 8) - 1, computed exactly as retail does
            // it (1800c46b6/bc/c5/c8), before the low byte is subtracted.
            const uint8_t* mp = op - ((c & 0x1fu) << 8) - 1;
            if (len == 7u) {                            // 1800c46cf
                len = 7u + *sp++;                       // 1800c46d1..d9
                if (g_branch) ++g_branch->matchExtended;
            } else if (g_branch) {
                ++g_branch->matchShort;
            }
            mp -= *sp++;                                // 1800c46dc/e7, the low offset byte
            if (op + 2 + len > dstEnd) {                // 1800c46f3  JA
                if (g_branch) ++g_branch->failMatchDst;
                return 0;
            }
            // UNSIGNED compare (JC at 1800c46f8): mp can wrap below the buffer.
            if (reinterpret_cast<uintptr_t>(mp) < reinterpret_cast<uintptr_t>(dstBase)) {
                if (g_branch) ++g_branch->failMatchSrc;
                return 0;
            }
            // The two unrolled bytes, then `len` more — 2 + len total, and
            // BYTE AT A TIME because mp and op overlap whenever the distance is
            // less than the length (§2 note 1). This is not a memcpy.
            op[0] = mp[0];                              // 1800c46fa/4706
            op[1] = mp[1];                              // 1800c4709/470e
            op += 2;
            mp += 2;
            do {                                        // 1800c4711
                *op++ = *mp++;
            } while (--len);
        }
    } while (sp < srcEnd);                              // 1800c4727  JC (unsigned)

    // THE READ CURSOR, and why it is counted here and not compared.
    //
    // Lane PICK's finding generalises to: if a result is observed only through a
    // lossy transform, the oracle is blind to everything the transform discards.
    // The decompressor's analogue is comparing the output bytes while ignoring
    // where the decode LEFT THE STREAM — two bodies can emit identical bytes and
    // stop at different source positions.
    //
    // For THIS function that hazard does not apply, and the reason is worth
    // stating rather than assuming: `sp` is a LOCAL. Decompress is `static`, it
    // owns no object, and its only observable outputs are the returned length and
    // the destination bytes. Both are compared, and the destination is compared
    // OVER THE WHOLE dstCap rather than the prefix the return value claims — so a
    // body that returned the right length while writing a different amount fails.
    // There is no cursor field, no checksum and no end-of-block state to miss.
    //
    // It is counted anyway, because "the corpus never exercised the overshoot
    // exit" and "the overshoot exit does not exist" look identical in a report
    // that prints neither. A literal run may straddle srcEnd, so an exact landing
    // is not guaranteed and both outcomes are real.
    //
    // AT RUNG 2 THIS BECOMES LOAD-BEARING. CompressedStream carries `position`
    // (+0x1c), `activeWindow` (+0x20) and per-window use counts; there, comparing
    // returned bytes alone IS the lossy transform, and the oracle must compare
    // the object's state too. Recorded in WAVE9_READPACKFILE_PLAN.md.
    if (g_branch) {
        if (sp == srcEnd) ++g_branch->exitExact;
        else              ++g_branch->exitOvershoot;
    }
    return static_cast<int>(op - dstBase);              // 1800c472d
}

}  // namespace

// ============================================================================
// ?Decompress@Compression@System@@SAHPEBXIPEAXI@Z            retail 0x1800c4660
// ============================================================================
extern "C" int RtSystem_Compression_Decompress(const void* src, uint32_t srcLen,
                                               void* dst, uint32_t dstCap) {
    RT_DIFF_BEGIN("System::Compression::Decompress");

    const int ours = DecompressBody(src, srcLen, dst, dstCap);

    if (rt_diff_on__ && dstCap <= kOracleScratch) {
        // Both sides onto private scratch prefilled with the SAME byte, so a
        // byte neither writes cannot fail and a byte only ONE writes does
        // (reference_oracle_must_not_compare_unwritten_bytes.md). The real
        // output above is not disturbed.
        //
        // Static rather than stack or heap: a live CompressedStream window is
        // 32 KB (0x8000), which does not belong on a stack frame, and calling
        // the allocator from inside a diff guard would put allocator traffic in
        // the middle of an oracle for a function that has none. 64 KB of .bss
        // costs nothing and is reached only when diffing is on. Diff runs are
        // single-threaded through this path; a live re-entrant caller would need
        // this revisited, and there is none today (§3: the only caller is the
        // probe below until the CompressedStream lane lands).
        static uint8_t mineBuf[kOracleScratch];
        static uint8_t theirsBuf[kOracleScratch];
        std::memset(mineBuf,   0xCD, dstCap);
        std::memset(theirsBuf, 0xCD, dstCap);

        CompressionBranchCounts* const saved = g_branch;
        g_branch = nullptr;      // the replay must not double-count branches
        const int mine = DecompressBody(src, srcLen, mineBuf, dstCap);
        g_branch = saved;

        const int theirs = RT_ORIGINAL_ONCE(DecompressFn,
            "?Decompress@Compression@System@@SAHPEBXIPEAXI@Z")(
                src, srcLen, theirsBuf, dstCap);

        // Every term of the verdict is named in the message, per
        // reference_shadow_predicate_must_name_its_failing_term.md: the two
        // return values AND the whole output buffer, not just the prefix one
        // side claims to have written.
        const bool bytesEqual = (std::memcmp(mineBuf, theirsBuf, dstCap) == 0);
        char a[128], b[128];
        std::snprintf(a, sizeof(a), "ret=%d bytes=%s", mine,
                      bytesEqual ? "match" : "DIFFER");
        std::snprintf(b, sizeof(b), "ret=%d bytes=%s", theirs,
                      bytesEqual ? "match" : "DIFFER");
        RT_DIFF_SHADOW(mine == theirs && bytesEqual, a, b);
        // "Did work" is "the decode produced output", not "we were called": a
        // run of rejected inputs all returning 0 must not read as covered.
        RT_DIFF_WORK(ours > 0);
    } else if (rt_diff_on__) {
        // Counted and named rather than silently skipped, so an oracle that
        // stopped running cannot look like one that keeps passing.
        static int64_t skipped = 0;
        if (++skipped == 1)
            std::printf("[PKG2/Compression] oracle SKIPPED for dstCap=%u > %u scratch\n",
                        dstCap, (unsigned)kOracleScratch);
    }
    return ours;
}

// ============================================================================
// The probe — §3 and §4
// ============================================================================
namespace {

typedef void* (*CompressionCtorFn)(void*);
typedef void  (*CompressionDtorFn)(void*);
typedef int   (*CompressFn)(void*, const void*, uint32_t, void*, uint32_t);

// One case: compress `n` bytes of `raw` with the DLL, decompress with both.
// Returns true if the round-trip reproduced the input exactly.
bool ProbeRoundTrip(void* compression, const uint8_t* raw, uint32_t n,
                    uint8_t* packed, uint32_t packedCap,
                    uint8_t* out, uint32_t outCap, int64_t* rejected) {
    const int packedLen = RT_ORIGINAL_ONCE(CompressFn,
        "?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z")(
            compression, raw, n, packed, packedCap);
    if (packedLen <= 0) { ++*rejected; return true; }   // the compressor declined; not our case

    // This call goes through OUR port (it is registered by the time the probe
    // runs), so the comparison in the body above is what actually executes.
    std::memset(out, 0, outCap);
    const int got = RtSystem_Compression_Decompress(packed,
                        static_cast<uint32_t>(packedLen), out, outCap);
    return got == static_cast<int>(n) && std::memcmp(out, raw, n) == 0;
}

}  // namespace

void RtSystem_CompressionProbe() {
    if (!RtDiffEnabled()) return;

    // The corpus source is the loaded Runtime image itself — real, varied, and
    // already mapped, so this needs no file I/O and no new import (§4).
    //
    // The base is derived from the DLL's OWN Decompress, whose Ghidra address is
    // 0x1800c4660 and whose preferred image base is 0x180000000, i.e. RVA
    // 0xC4660. Deriving it this way rather than through standalone/resolve.h's
    // GetModuleInfo is deliberate: that header drags in <windows.h>, whose
    // <wingdi.h> declares a function called `Rectangle` and whose <windef.h>
    // #defines `near` and `far` away — the same reason lane PKG forward-declares
    // ResolveAddr at file scope instead of including it. RtOriginal is
    // ResolveExportRaw, so this is the shipped DLL's address and never a port's.
    const uint8_t* const decompressFn = reinterpret_cast<const uint8_t*>(
        RtOriginal<DecompressFn>("?Decompress@Compression@System@@SAHPEBXIPEAXI@Z"));
    if (decompressFn == nullptr) {
        std::printf("[PKG2/Compression] probe SKIPPED: cannot locate the DLL's Decompress\n");
        return;
    }
    const uint8_t* const image = decompressFn - 0xC4660;

    // System::Compression is an 8-byte object over a 0x40000 hash table the ctor
    // allocates and the dtor frees (0x1800c41a0 / 0x1800c4170). Built through
    // the DLL's own ctor: this lane ports the DECODER, not the encoder, and a
    // hand-built object would be an unverified assumption in the oracle itself.
    void* compression[1] = { nullptr };
    // RT_RAW_OK: probe-only. This whole function early-returns unless
    // RtDiffEnabled(), so the three Compression doors below are unreachable in
    // any run that is not the diff harness -- and unreachable at cutover, where
    // the harness is compiled out. tools/unsatisfied_export_scan.py cannot see a
    // function-level early return as a guard and lists them as live; that is a
    // known limit of the scan, recorded rather than worked around, and the marker
    // is the same one tools/rt_data_door_audit.py already honours.
    RT_ORIGINAL_ONCE(CompressionCtorFn, "??0Compression@System@@QEAA@XZ")(compression);

    // Static, for the same reason as the oracle's scratch: no allocator traffic
    // inside a verification path, and the probe runs once.
    constexpr uint32_t kChunk  = 4096;
    constexpr uint32_t kPacked = kChunk * 2 + 64;   // generous; Compress may expand
    static uint8_t raw[kChunk];
    static uint8_t packed[kPacked];
    static uint8_t out[kChunk + 64];

    CompressionBranchCounts counts;
    g_branch = &counts;

    int64_t cases = 0, roundTripOk = 0, roundTripBad = 0, declined = 0;

    {
        // ---- 1. real image bytes, sliding windows ------------------------
        // 0x1000 stride over the first 8 MB of the mapped image, skipping the
        // PE header. Covers code, rdata string tables and long .bss-style runs.
        for (uint32_t off = 0x1000; off < 0x200000u; off += 0x4000u) {
            std::memcpy(raw, image + off, kChunk);
            ++cases;
            if (ProbeRoundTrip(compression, raw, kChunk, packed, kPacked,
                               out, kChunk + 64, &declined)) ++roundTripOk;
            else ++roundTripBad;
        }

        // ---- 2. synthetic RLE: forces the extended-length branch ----------
        for (int b = 0; b < 8; ++b) {
            std::memset(raw, static_cast<int>(b * 37), kChunk);
            ++cases;
            if (ProbeRoundTrip(compression, raw, kChunk, packed, kPacked,
                               out, kChunk + 64, &declined)) ++roundTripOk;
            else ++roundTripBad;
        }

        // ---- 3. synthetic incompressible: forces literal runs -------------
        {
            uint32_t s = 0x12345678u;
            for (uint32_t i = 0; i < kChunk; ++i) {
                s = s * 1664525u + 1013904223u;       // a plain LCG; no libm, no RNG dep
                raw[i] = static_cast<uint8_t>(s >> 24);
            }
            ++cases;
            if (ProbeRoundTrip(compression, raw, kChunk, packed, kPacked,
                               out, kChunk + 64, &declined)) ++roundTripOk;
            else ++roundTripBad;
        }

        // ---- 4. the failure exits, driven ON PURPOSE ----------------------
        // A codec probe that only feeds it valid data leaves three of the six
        // buckets at zero and calls the result thorough. These fill them:
        //   (a) a valid stream with dstCap one byte short  -> the two dst-bounds
        //       rejections, whichever the stream reaches first;
        //   (b) RAW image bytes fed in AS a compressed stream -> nonsense
        //       control bytes, which is what reaches `mp < dst`.
        std::memcpy(raw, image + 0x1000, kChunk);
        const int packedLen = RT_ORIGINAL_ONCE(CompressFn,
            "?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z")(
                compression, raw, kChunk, packed, kPacked);
        if (packedLen > 0) {
            for (uint32_t cap = 1; cap < kChunk; cap = cap * 3 + 1) {
                RtSystem_Compression_Decompress(packed,
                    static_cast<uint32_t>(packedLen), out, cap);
                ++cases;
            }
        }
        for (uint32_t off = 0x1000; off < 0x40000u; off += 0x2000u) {
            // Raw code bytes as a "compressed" stream. Bounded output so a
            // runaway decode cannot walk off the buffer — both sides see the
            // same cap, so this stays a fair comparison.
            RtSystem_Compression_Decompress(image + off, 256, out, 1024);
            ++cases;
        }
    }

    g_branch = nullptr;
    RT_ORIGINAL_ONCE(CompressionDtorFn, "??1Compression@System@@QEAA@XZ")(compression);

    const bool allCovered = counts.literalRun && counts.matchShort &&
                            counts.matchExtended && counts.failLiteral &&
                            counts.failMatchDst && counts.failMatchSrc;
    std::printf("[PKG2/Compression] probe: %lld cases, round-trip ok=%lld bad=%lld "
                "(compressor declined %lld)\n",
                (long long)cases, (long long)roundTripOk, (long long)roundTripBad,
                (long long)declined);
    std::printf("[PKG2/Compression] branch coverage: literal=%lld matchShort=%lld "
                "matchExtended=%lld | failLiteral=%lld failMatchDst=%lld "
                "failMatchSrc=%lld -- %s\n",
                (long long)counts.literalRun, (long long)counts.matchShort,
                (long long)counts.matchExtended, (long long)counts.failLiteral,
                (long long)counts.failMatchDst, (long long)counts.failMatchSrc,
                allCovered ? "ALL SIX BRANCHES COVERED"
                           : "*** A BRANCH READS ZERO -- NOT a full verification ***");
    // Printed but NOT compared, and labelled as such so nobody reads it as a
    // verified term: the read cursor is a local this function discards, so there
    // is nothing on the DLL's side to compare it against. See the note at the
    // return site for why that is sound here and why it will not be at rung 2.
    std::printf("[PKG2/Compression] read-cursor exits (ours only, NOT compared -- "
                "sp is a local): exact=%lld overshoot=%lld\n",
                (long long)counts.exitExact, (long long)counts.exitOvershoot);
}

}  // namespace RtSystem

// ============================================================================
// Registration
// ============================================================================
int RtSystemCompressionRegister() {
    using namespace RtSystem;
    int n = 0;
    RegisterRuntimeExport("?Decompress@Compression@System@@SAHPEBXIPEAXI@Z",
                          (void*)&RtSystem_Compression_Decompress); ++n;
    RegisterRuntimeAddr(0x1800c4660, (void*)&RtSystem_Compression_Decompress);

    // DELIBERATELY ABSENT, and the reason is the point of the whole lane:
    //   ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z  0x1800c4230  ord 1749
    //       The ENCODER. It is not on the game's read path at all (only the
    //       ToolKit writes packages), and this lane USES it as the oracle's
    //       input generator. Porting the thing that generates your test inputs
    //       is how a shared misreading passes its own test.
    //   ??0Compression@System@@ / ??1Compression@System@@  ctor/dtor
    //       Only the encoder needs the 0x40000 hash table; Decompress is static
    //       and touches no object. They are needed at cutover only if Compress is.
    return n;
}
