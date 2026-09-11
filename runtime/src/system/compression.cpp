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
#include "string.h"   // RtMemAlloc -- the lock-wrapped MemoryAllocator door

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

// ============================================================================
// WAVE 22 LANE SYSBODY — the ENCODER: Compression::Compress and its ctor
// ============================================================================
//   ??0Compression@System@@QEAA@XZ                    0x1800c41a0  ord  109
//   ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z   0x1800c4230  ord 1749
//
// THE PRIOR HOLDBACK, RE-CHECKED RATHER THAN INHERITED. The registration block
// at the bottom of this file records Compress as DELIBERATELY ABSENT, with a
// stated reason: this file's Decompress probe USES the shipped Compress as its
// input generator, and "porting the thing that generates your test inputs is
// how a shared misreading passes its own test." That was the right call when
// it was written. It does not survive being checked
// (reference_holdback_blockers_are_stale_check_them_first.md), for a reason
// that is mechanical rather than a matter of judgement:
//
//     RT_ORIGINAL_ONCE expands to RtOriginal<Fn>(name), and RtOriginal is
//     ResolveExportRaw (runtime/funcs/rt_port.h:140,154). It does NOT consult
//     the port registry.
//
// All three of the probe's Compression doors — the ctor at line ~417, the
// Compress at ~475, the dtor at ~494 — are RT_ORIGINAL_ONCE. So registering
// our encoder cannot change what the Decompress probe calls: it keeps
// generating its corpus with the SHIPPED compressor, and check B (ours
// round-trips to the original bytes) never involved the DLL's decoder in the
// first place. At cutover the probe is compiled out entirely. The holdback's
// premise is false in the DLL-present route and moot in the other; it is
// discharged here rather than quietly dropped, and the registration block
// below is rewritten to say so.
//
// WHAT THE ENCODER IS. LZ77 with a 16-bit hash chain of ONE slot per bucket
// (a "hash head" table, no chain walking, no lazy matching) producing exactly
// the stream the Decompress above consumes. 0x40000 bytes = 0x10000 uint32
// entries, each a byte OFFSET from `src`, which is why the table is useless
// across calls and why the ctor zeroes it.
//
//   op starts at dst+1 (0x1800c429b: LEA R10,[R9+1]). The first byte is
//   RESERVED for the literal-run control that has not been emitted yet. Every
//   emit path writes that control BACKWARDS, at op-ii-1, and every match path
//   reserves a fresh one by advancing op TWO past the low offset byte.
//
// THE SIX THINGS THAT ARE EASY TO GET WRONG HERE, all verified against the
// disassembly rather than the decompiler (which renders this body as a
// 60-line ternary nest):
//
//  1. **`dv` IS A 32-BIT ROLLING REGISTER AND IS NEVER MASKED TO 24 BITS —
//     BUT THE UPPER BYTE IS PROVABLY UNOBSERVABLE, AND THE FIRST DRAFT OF
//     THIS NOTE CLAIMED THE OPPOSITE.** EDI starts as `(src[0]<<8)|src[1]`
//     and every loop top does `SHL EDI,8 / OR EDI,ip[2]`. On the LITERAL path
//     it is NOT reset, so after k consecutive literals the register holds k+2
//     bytes. This note used to say that those upper bytes therefore "CHANGE
//     THE BUCKET" and that masking to 24 bits would silently produce a
//     different stream. **That is false, and a negative control found it**:
//     masking dv to 24 bits was injected on purpose, and the sweep reported
//     ZERO divergences over 40 cases. The sweep was right and the note was
//     wrong. Write dv = M + (U << 24), M = dv & 0xFFFFFF:
//
//         hash = (uint16)((dv >> 8) - 5*dv)
//         (dv >> 8)  low 16 bits = dv bits 8..23  = M >> 8      (U is at 16+)
//         (5 * dv)   low 16 bits = 5*M low 16     (5*(U<<24) is all bits 24+)
//         subtraction borrows propagate UPWARD only
//         => hash = (uint16)((M >> 8) - 5*M), independent of U.
//
//     And dv feeds nothing but the hash. So the upper byte cannot reach any
//     output, and masking is a NO-OP rather than a silent defect. It is still
//     transcribed unmasked, because the register is what retail has and a
//     mask would be an unforced claim about what is safe -- but the reason is
//     now "no reason to deviate", not "load-bearing".
//     The lesson generalises and is why this note survives its own
//     retraction: a control that corrupts a value NOTHING OBSERVES reports
//     NOT CAUGHT and reads as an indictment of a sound instrument
//     (reference_control_must_pick_a_compared_subject.md). Note 5 is the
//     round-trip-invisible hazard on this body that IS real; that is the one
//     the effectiveness control uses.
//
//  2. **`dv` IS CLOBBERED BY THE MATCH PATH.** EDI is reused as `maxLen`
//     (0x1800c4332). Retail rebuilds it afterwards from ip-1 (0x1800c4566),
//     24-bit-clean, which is why the rolling value restarts clean after every
//     match and only accumulates across literal runs. Reproduced exactly.
//
//  3. **THE HASH IS `(uint16)((dv>>8) - 5*dv)`**, all in 32-bit arithmetic
//     (`SHR EDX,8`, `LEA ECX,[RDI+RDI*4]`, `SUB EDX,ECX`, `MOVZX EDX,DX`).
//     The subtraction wraps; the truncation to 16 bits is what bounds it to
//     the table. Written with explicit uint32_t so the wrap is defined.
//
//  4. **THE MATCH GUARD IS `m_pos > src`, STRICTLY** (`CMP R11,R14 / JBE`),
//     not `>=`. Offset 0 in the table means "nothing stored", and a stored
//     offset of 0 at position 0 is indistinguishable from it — so retail
//     simply never matches against the very first byte. A `>=` here matches
//     one byte earlier than retail and diverges on inputs that begin with a
//     repeat.
//
//  5. **THE TWO HASH-TABLE BACKFILLS AFTER A MATCH ARE NOT OPTIONAL.**
//     0x1800c4522..0x1800c4585 stores the buckets for ip-2 and ip-1 (the two
//     positions the match skipped over). Dropping them still round-trips and
//     still compresses; it just finds different matches later. Byte-visible,
//     round-trip-invisible.
//
//  6. **THE TAIL'S 3-BYTE RESERVATION CAN BE OVERRUN BY ONE**, and that is
//     retail's. `CMP RCX(op+3),R12 / JBE` reserves three bytes, then the tail
//     literal loop appends up to two bytes with NO further bounds check; if
//     `ii` reaches 0x20 inside that loop it writes a control byte and advances
//     again, so worst case is 2 data + 1 flush control + 1 final control = 4.
//     Not guarded — the project rule is that a faithful unguarded write stays
//     faithful, and a guard would change the RETURN VALUE rather than protect
//     anything. Callers reach this only through CompressedStream, which sizes
//     its output window at 0x8000 for a 0x8000 input.
//
// ALSO FAITHFUL AND ALSO UNGUARDED: the two loads at 0x1800c425e/0x1800c4272
// read src[0] AND src[1] before anything has established srcLen >= 2, so
// Compress(src, 1, ...) reads one byte past the input. Retail's. The sweep
// below passes srcLen == 1 on purpose so the behaviour is exercised and
// compared rather than merely asserted.

// BRANCH COVERAGE FOR THE ENCODER. Same reason the decoder above counts its
// six branches: a codec sweep is exactly where a uniform population hides. A
// corpus of incompressible bytes drives ONLY the literal path and the two
// 0x20-flushes, agrees perfectly with retail on every case, and says nothing
// at all about the match encoder -- which is two thirds of the body. Six
// counters, printed, with a verdict line that FAILS when any of them is zero.
struct CompressBranchCounts {
    long long literal      = 0;   // a byte emitted as a literal
    long long literalFlush = 0;   // a run hit 0x20 and flushed its control
    long long matchShort   = 0;   // code < 7, one control byte
    long long matchLong    = 0;   // code >= 7, control + extra length byte
    long long matchUnroll  = 0;   // the maxLen > 0x10 unrolled comparison ran
    long long failDst      = 0;   // an output-bounds rejection (either site)
};
CompressBranchCounts* g_cbranch = nullptr;

// The hash retail computes, in the width retail computes it in.
static inline uint32_t CompressHash(uint32_t dv) {
    return static_cast<uint16_t>((dv >> 8) - dv * 5u);   // 1800c42bc..1800c42c9
}

int CompressBody(void* self, const void* src, uint32_t srcLen,
                 void* dst, uint32_t dstCap) {
    // 1800c4247 / 1800c4255 — both length tests, before anything else.
    if (srcLen == 0 || dstCap == 0)
        return 0;                                        // 1800c4645

    uint32_t* const table = *static_cast<uint32_t**>(self);   // 1800c42c6: MOV RCX,[R11]
    const uint8_t* const in     = static_cast<const uint8_t*>(src);
    const uint8_t* const inEnd  = in + srcLen;                // 1800c4289
    const uint8_t* const inMax  = inEnd - 2;                  // 1800c4292: R15
    uint8_t* const outBase      = static_cast<uint8_t*>(dst);
    uint8_t* const outEnd       = outBase + dstCap;           // 1800c428e: R12

    const uint8_t* ip = in;                                   // 1800c4296: RAX
    uint8_t* op = outBase + 1;                                // 1800c429b: the RESERVED control byte
    int32_t  ii = 0;                                          // 1800c4299: EBX, the pending literal count
    uint32_t dv = (static_cast<uint32_t>(in[0]) << 8) | in[1];   // 1800c427d..1800c428c

    bool fail = false;

    if (ip < inMax) {                                         // 1800c429f: CMP RDX,R15 / JNC
        for (;;) {
            dv = (dv << 8) | ip[2];                           // 1800c42b0..1800c42ba
            uint32_t* const slot = &table[CompressHash(dv)];
            const uint8_t* const mpos = in + *slot;           // 1800c42d2: zero-extended uint32
            *slot = static_cast<uint32_t>(ip - in);           // 1800c42db
            const uint64_t off = static_cast<uint64_t>(ip - mpos) - 1u;   // 1800c42de..42e1

            const bool isMatch =
                off < 0x2000u &&                              // 1800c42e4  JNC (unsigned)
                mpos > in &&                                  // 1800c42f1  JBE (STRICT, see note 4)
                mpos[2] == ip[2] &&                           // 1800c42fa
                mpos[0] == ip[0] && mpos[1] == ip[1];         // 1800c4308..4327 (as one 16-bit cmp)

            if (!isMatch) {
                // ---- LITERAL ---- 1800c458f
                if (op >= outEnd) { fail = true; if (g_cbranch) ++g_cbranch->failDst; break; }  // 1800c458f  JNC
                *op++ = *ip++;
                ++ii;
                if (g_cbranch) ++g_cbranch->literal;
                if (ii == 0x20) {                             // 1800c45a7
                    if (g_cbranch) ++g_cbranch->literalFlush;
                    op[-0x21] = 0x1f;                         // 1800c45ac: the reserved slot, 32 literals back
                    ++op;                                     // and reserve the next one
                    ii = 0;
                }
                if (!(ip < inMax)) break;                     // 1800c45b6  JC
                continue;
            }

            // ---- MATCH ---- how long?  1800c432d..1800c4342
            uint32_t maxLen = static_cast<uint32_t>(inEnd - ip) - 2u;
            if (maxLen > 0x108u) maxLen = 0x108u;             // CMOVA

            // The output reservation, BEFORE the literal header is flushed.
            // 1800c4345..1800c4363. Note the second test backs `op` up by one
            // when there is no pending literal run, because that reserved byte
            // is about to be reclaimed.
            if (!(op + 4 < outEnd)) {
                if (op - (ii == 0 ? 1 : 0) + 4 >= outEnd) {
                    fail = true; if (g_cbranch) ++g_cbranch->failDst; break;
                }
            }

            // Flush the pending literal run header at op-ii-1. 1800c4369..4382.
            // When ii == 0 this writes 0xff into the reserved byte and then
            // reclaims it, so the match control overwrites it. Faithful, and
            // the write is retail's, not a transcription slip.
            op[-ii - 1] = static_cast<uint8_t>(ii - 1);
            if (ii == 0) --op;

            uint32_t len;                                      // R9D
            if (maxLen <= 0x10u) {                             // 1800c4385  JBE
                len = 2;
                goto generic;                                  // 1800c44b2
            }
            // The unrolled 3..0x12 comparison. Transcribed as a loop: every
            // step is `if (mpos[k] != ip[k]) { len = k; goto emit; }` with the
            // SAME k in the same order, and the fallthrough sets len = 0x12
            // and enters the generic loop (1800c44a2..44b0).
            len = 0x12;
            if (g_cbranch) ++g_cbranch->matchUnroll;
            for (uint32_t k = 3; k <= 0x12u; ++k) {
                if (mpos[k] != ip[k]) { len = k; goto emit; }
            }
        generic:
            // 1800c44b2..44c3
            for (;;) {
                ++len;
                if (len >= maxLen) break;                      // JNC (unsigned)
                if (ip[len] != mpos[len]) break;
            }
        emit:
            {
                const uint32_t mlen = len;                     // total bytes matched
                const uint32_t code = mlen - 2;                // 1800c44c5: ADD R9D,-2
                ++ip;                                          // 1800c44c9: INC RAX
                if (code < 7u) {                               // 1800c44cc  JNC
                    // 1800c44d2..44e5
                    if (g_cbranch) ++g_cbranch->matchShort;
                    *op++ = static_cast<uint8_t>((off >> 8) + (code << 5));
                } else {
                    // 1800c44eb..4501
                    if (g_cbranch) ++g_cbranch->matchLong;
                    *op++ = static_cast<uint8_t>((off >> 8) - 0x20u);
                    *op++ = static_cast<uint8_t>(code - 7u);
                }
                *op = static_cast<uint8_t>(off);               // 1800c450d: the low offset byte
                op += 2;                                       // ...and RESERVE the next control
                ip += mlen - 1;                                // 1800c4510 (ip was already +1)
                ii = 0;                                        // 1800c4517
            }
            if (!(ip < inMax)) break;                          // 1800c4519  JNC -> 45bf

            // The two skipped positions get their buckets filled.
            // 1800c4522..1800c4585.  q = ip-2, then ip-1.
            {
                const uint8_t* const q = ip - 2;
                uint32_t v = (static_cast<uint32_t>(q[0]) << 16) |
                             (static_cast<uint32_t>(q[1]) << 8)  |
                              static_cast<uint32_t>(q[2]);
                table[CompressHash(v)] = static_cast<uint32_t>(q - in);
                v = (v << 8) | q[3];                           // 1800c4557/4566: 3 bytes at ip-1
                table[CompressHash(v)] = static_cast<uint32_t>((q + 1) - in);
                dv = v;                                        // EDI, 24-bit-clean again (note 2)
            }
        }
    }

    if (fail)
        return 0;                                              // 1800c45cd

    // ---- THE TAIL ---- 1800c45c4
    if (op + 3 > outEnd)
        return 0;
    while (ip < inEnd) {                                       // 1800c4600..4621
        *op++ = *ip++;
        ++ii;
        if (ii == 0x20) {
            op[-0x21] = 0x1f;
            ++op;
            ii = 0;
        }
    }
    // 1800c4623..4640
    op[-ii - 1] = static_cast<uint8_t>(ii - 1);
    if (ii == 0) --op;
    return static_cast<int>(op - outBase);
}

}  // namespace

// ============================================================================
// ??0Compression@System@@QEAA@XZ                             retail 0x1800c41a0
// ============================================================================
// EnterCS(_singleton+0x640); Allocate(_singleton, 0x40000, 4); LeaveCS;
// *this = p; memset(p, 0, 0x40000).
//
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) IS that exact
// lock/Allocate/unlock sequence — verified by decompiling it, not assumed — so
// `RtMemAlloc(0x40000, 4)` from string.cpp is the same three instructions plus
// a call, and it keeps standalone/allocator_lock.cpp's detour on Allocate in
// the path. Open-coding EnterCriticalSection on `_singleton + 0x640` here
// would be a second, private opinion about where that lock lives.
//
// NOT NULL-CHECKED: retail stores the result and memsets it unconditionally.
// An allocation failure here is an upstream fault to find.
extern "C" void* RtSystem_Compression_Ctor(void* self) {
    void* p = RtMemAlloc(0x40000u, 4);
    *static_cast<void**>(self) = p;
    std::memset(p, 0, 0x40000u);
    return self;   // 1800c41a0 is QEAA: a ctor returns `this` in RAX
}

// ============================================================================
// ?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z             retail 0x1800c4230
// ============================================================================
extern "C" int RtSystem_Compression_Compress(void* self, const void* src, uint32_t srcLen,
                                             void* dst, uint32_t dstCap) {
    RT_DIFF_BEGIN("System::Compression::Compress");
    return CompressBody(self, src, srcLen, dst, dstCap);
}

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
            printf("[PKG2/Compression] oracle SKIPPED for dstCap=%u > %u scratch\n",
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
        printf("[PKG2/Compression] probe SKIPPED: cannot locate the DLL's Decompress\n");
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
    printf("[PKG2/Compression] probe: %lld cases, round-trip ok=%lld bad=%lld "
                "(compressor declined %lld)\n",
                (long long)cases, (long long)roundTripOk, (long long)roundTripBad,
                (long long)declined);
    printf("[PKG2/Compression] branch coverage: literal=%lld matchShort=%lld "
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
    printf("[PKG2/Compression] read-cursor exits (ours only, NOT compared -- "
                "sp is a local): exact=%lld overshoot=%lld\n",
                (long long)counts.exitExact, (long long)counts.exitOvershoot);
}

// ============================================================================
// WAVE 22 LANE SYSBODY — the Compress input sweep
// ============================================================================
// WHY A SWEEP RATHER THAN WAITING FOR A CALLER. Compress has NO caller in this
// tree and none in the game: only the ToolKit writes packages, so a registered
// encoder would sit at `calls=0` forever and a `calls=0` row is not a verified
// row (reference_uncalled_port_is_not_verified.md). For a function that is
// pure given its object, an INPUT SWEEP against the shipped export is strictly
// better than waiting — it is available now, it covers cases play never
// produces, and it is the method that caught a real defect on
// GetAlignedPosition when an in-game run reported calls=0 across 16.9M calls
// (reference_project_pure_functions_verify_by_sweep.md).
//
// THE OBJECT IS STATEFUL, AND THAT IS THE WHOLE DESIGN OF THIS PROBE.
// Compression's 0x40000-byte table carries over between calls: bucket contents
// from case N change which matches case N+1 finds. So the sweep keeps TWO
// objects and feeds both the SAME sequence:
//
//     theirObj — built with the DLL's ctor, driven with the DLL's Compress
//     ourObj   — built with OUR ctor,       driven with OUR   Compress
//
// which makes the two tables shadow each other case by case. That is what
// turns the sweep into a test of the ctor as well: a ctor that allocated but
// did not zero the table would diverge on the very first case, and a ctor that
// zeroed the wrong size would diverge partway through the first buffer bigger
// than the region it cleared. There is no separate ctor probe because this IS
// the ctor probe.
//
// THE COMPARISON IS BYTE-EXACT ON THE OUTPUT, NOT A ROUND-TRIP. A round-trip
// check (compress with ours, decompress, compare to the input) is the WEAK
// oracle here: this format has many valid encodings of the same bytes, so a
// port that masked the rolling hash to 24 bits, or dropped the two
// post-match bucket backfills, round-trips perfectly and emits a different
// stream. Both of those are live hazards in the body above (notes 1 and 5), so
// the sweep compares the returned length AND every emitted byte against the
// shipped encoder. The round-trip is ALSO run, as an independent second check
// that does not involve the DLL's encoder at all.
//
// THE EFFECTIVENESS CONTROL. Six encoder branches are counted (literal,
// literal-flush, short match, long match, the unrolled comparison, and the
// output-bounds rejection) and all six are printed. A corpus of random bytes
// drives only the first two and would agree with retail on every case while
// leaving the match encoder — two thirds of the body — completely untested.
// The corpus below is built to fill all six on purpose, and the verdict line
// FAILS if any bucket reads zero, so "the sweep did not reach it" and "the
// branch is correct" cannot be confused.
void RtSystem_CompressProbe() {
    if (!RtDiffEnabled()) return;

    typedef void* (*CompressionCtorFn)(void*);
    typedef void  (*CompressionDtorFn)(void*);
    typedef int   (*CompressFn)(void*, const void*, uint32_t, void*, uint32_t);

    // RT_RAW_OK: probe-only, and an ORACLE. RT_ORIGINAL_ONCE is
    // ResolveExportRaw, so `theirs` is the SHIPPED encoder even though our own
    // is registered under the same name by the time this runs. Reaching it
    // through ResolveExport would compare our body with itself
    // (reference_a_probe_whose_ours_side_reaches_the_body_by_address...).
    CompressFn theirs = RT_ORIGINAL_ONCE(CompressFn,
        "?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z");
    CompressionCtorFn theirCtor = RT_ORIGINAL_ONCE(CompressionCtorFn,
        "??0Compression@System@@QEAA@XZ");
    CompressionDtorFn theirDtor = RT_ORIGINAL_ONCE(CompressionDtorFn,
        "??1Compression@System@@QEAA@XZ");
    if (!theirs || !theirCtor || !theirDtor) {
        printf("[SYSBODY/Compress] sweep SKIPPED: no shipped encoder (cutover build?)\n");
        return;
    }

    const uint8_t* const decompressFn = reinterpret_cast<const uint8_t*>(
        RtOriginal<DecompressFn>("?Decompress@Compression@System@@SAHPEBXIPEAXI@Z"));
    if (decompressFn == nullptr) {
        printf("[SYSBODY/Compress] sweep SKIPPED: cannot locate the DLL image\n");
        return;
    }
    const uint8_t* const image = decompressFn - 0xC4660;   // same derivation as the probe above

    void* theirObj[1] = { nullptr };
    void* ourObj[1]   = { nullptr };
    theirCtor(theirObj);
    RtSystem_Compression_Ctor(ourObj);   // OUR ctor -- see the note above
    if (ourObj[0] == nullptr || theirObj[0] == nullptr) {
        printf("[SYSBODY/Compress] sweep SKIPPED: a ctor returned no table\n");
        return;
    }

    static const uint32_t kMaxIn = 0x8000;   // CompressedStream's window size
    static uint8_t raw[kMaxIn];
    static uint8_t ourPacked[kMaxIn + 4096];
    static uint8_t theirPacked[kMaxIn + 4096];
    static uint8_t rt[kMaxIn + 64];

    CompressBranchCounts counts;
    g_cbranch = &counts;

    long long cases = 0, lenDiff = 0, byteDiff = 0, accepted = 0, rejected = 0;
    long long roundTripOk = 0, roundTripBad = 0;
    int distinctLen[64]; int distinctCount = 0;

    // One case. `n` bytes of `raw` through both encoders, into separate
    // buffers, compared exactly.
    struct Case {
        static void Run(void* ourO, void* theirO, CompressFn theirsFn,
                        const uint8_t* src, uint32_t n, uint32_t cap,
                        uint8_t* ourBuf, uint8_t* theirBuf, uint8_t* rtBuf,
                        long long* cases, long long* lenDiff, long long* byteDiff,
                        long long* accepted, long long* rejected,
                        long long* rtOk, long long* rtBad,
                        int* seen, int* seenCount) {
            std::memset(ourBuf, 0xCD, cap);
            std::memset(theirBuf, 0xCD, cap);
            const int ourLen   = RtSystem_Compression_Compress(ourO, src, n, ourBuf, cap);
            const int theirLen = theirsFn(theirO, src, n, theirBuf, cap);
            ++*cases;
            if (ourLen != theirLen) {
                if (*lenDiff < 8)
                    printf("[SYSBODY/Compress] LENGTH DIVERGENCE n=%u cap=%u ours=%d theirs=%d\n",
                           n, cap, ourLen, theirLen);
                ++*lenDiff;
                return;
            }
            if (ourLen == 0) { ++*rejected; return; }
            ++*accepted;
            // Compare the WHOLE capacity, not the prefix the return claims: a
            // body that returned the right length while writing further is a
            // defect this catches and a prefix compare does not. Both sides
            // were pre-filled with the same 0xCD.
            if (std::memcmp(ourBuf, theirBuf, cap) != 0) {
                if (*byteDiff < 8)
                    printf("[SYSBODY/Compress] BYTE DIVERGENCE n=%u len=%d\n", n, ourLen);
                ++*byteDiff;
            }
            bool isNew = true;
            for (int k = 0; k < *seenCount; ++k) if (seen[k] == ourLen) { isNew = false; break; }
            if (isNew && *seenCount < 64) seen[(*seenCount)++] = ourLen;

            // Independent second check: OUR stream through OUR decoder must
            // reproduce the input. Involves the DLL's encoder nowhere.
            const int got = RtSystem_Compression_Decompress(
                ourBuf, static_cast<uint32_t>(ourLen), rtBuf, n + 64);
            if (got == static_cast<int>(n) && std::memcmp(rtBuf, src, n) == 0) ++*rtOk;
            else ++*rtBad;
        }
    };

    // ---- 1. real code bytes: literals, short matches, the unrolled arm -----
    for (uint32_t off = 0x1000; off < 0x30000u; off += 0x2800u) {
        const uint32_t n = 0x2000;
        std::memcpy(raw, image + off, n);
        Case::Run(ourObj, theirObj, theirs, raw, n, sizeof(ourPacked),
                  ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
                  &accepted, &rejected, &roundTripOk, &roundTripBad,
                  distinctLen, &distinctCount);
    }

    // ---- 2. highly repetitive: LONG matches, the >0x108 cap, RLE offsets ---
    // Runs of one byte drive maxLen to its 0x108 ceiling and the match-length
    // encoder into its long form; without these `matchLong` reads zero.
    for (uint32_t rep = 1; rep <= 64u; rep *= 4) {
        const uint32_t n = 0x1800;
        for (uint32_t i = 0; i < n; ++i) raw[i] = static_cast<uint8_t>((i / rep) & 0x0f);
        Case::Run(ourObj, theirObj, theirs, raw, n, sizeof(ourPacked),
                  ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
                  &accepted, &rejected, &roundTripOk, &roundTripBad,
                  distinctLen, &distinctCount);
    }

    // ---- 3. incompressible: the 0x20-literal flush, over and over ----------
    {
        uint32_t x = 0x12345678u;
        const uint32_t n = 0x1000;
        for (uint32_t i = 0; i < n; ++i) {
            x = x * 1103515245u + 12345u;
            raw[i] = static_cast<uint8_t>(x >> 16);
        }
        Case::Run(ourObj, theirObj, theirs, raw, n, sizeof(ourPacked),
                  ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
                  &accepted, &rejected, &roundTripOk, &roundTripBad,
                  distinctLen, &distinctCount);
    }

    // ---- 4. the degenerate lengths, including the ones that read OOB -------
    // srcLen 1 makes retail read src[1] before any length test (see the banner).
    // `raw` is a static 0x8000 buffer, so that read is in-bounds of real memory
    // and both sides read the SAME byte -- the behaviour is exercised and
    // compared rather than merely asserted, and nothing faults.
    std::memcpy(raw, image + 0x1000, 64);
    for (uint32_t n = 0; n <= 8u; ++n) {
        Case::Run(ourObj, theirObj, theirs, raw, n, sizeof(ourPacked),
                  ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
                  &accepted, &rejected, &roundTripOk, &roundTripBad,
                  distinctLen, &distinctCount);
    }

    // ---- 5. the output-bounds rejections, driven ON PURPOSE ----------------
    // Without these `failDst` reads zero and the two rejection sites are
    // untested. Both sides get the same cap, so this stays a fair comparison.
    std::memcpy(raw, image + 0x4000, 0x1000);
    for (uint32_t cap = 1; cap < 0x400u; cap = cap * 3 + 1) {
        Case::Run(ourObj, theirObj, theirs, raw, 0x1000, cap,
                  ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
                  &accepted, &rejected, &roundTripOk, &roundTripBad,
                  distinctLen, &distinctCount);
    }
    // dstCap == 0 -- the second early exit.
    Case::Run(ourObj, theirObj, theirs, raw, 0x1000, 0,
              ourPacked, theirPacked, rt, &cases, &lenDiff, &byteDiff,
              &accepted, &rejected, &roundTripOk, &roundTripBad,
              distinctLen, &distinctCount);

    g_cbranch = nullptr;
    theirDtor(theirObj);
    // OUR object is freed with the DLL's dtor on purpose: our ctor allocated
    // through the same MemoryAllocator door, and this is the cross-check that
    // says so. A split heap shows up here as a crash or a redzone violation,
    // not as a silent pass.
    theirDtor(ourObj);

    printf("[SYSBODY/Compress] sweep: %lld cases, %lld length divergences, "
           "%lld byte divergences (accepted=%lld rejected=%lld)\n",
           cases, lenDiff, byteDiff, accepted, rejected);
    printf("[SYSBODY/Compress] independent round-trip (ours->ours, DLL not involved): "
           "ok=%lld bad=%lld\n", roundTripOk, roundTripBad);
    printf("[SYSBODY/Compress] encoder branches: literal=%lld literalFlush=%lld "
           "matchShort=%lld matchLong=%lld matchUnroll=%lld failDst=%lld\n",
           counts.literal, counts.literalFlush, counts.matchShort,
           counts.matchLong, counts.matchUnroll, counts.failDst);

    const bool allBranches = counts.literal && counts.literalFlush &&
                             counts.matchShort && counts.matchLong &&
                             counts.matchUnroll && counts.failDst;
    const bool populated = accepted > 0 && rejected > 0 && distinctCount > 3;
    printf("[SYSBODY/Compress] control: %d distinct output lengths; branches %s; "
           "population %s -- %s\n",
           distinctCount,
           allBranches ? "ALL SIX COVERED" : "*** A BRANCH READS ZERO ***",
           populated ? "NOT UNIFORM" : "*** UNIFORM ***",
           (lenDiff == 0 && byteDiff == 0 && roundTripBad == 0 &&
            allBranches && populated) ? "VERIFIED" : "*** NOT a verification ***");
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

    // WAVE 22 LANE SYSBODY — the encoder and its ctor, and the holdback that
    // used to stand here, DISCHARGED rather than dropped.
    //
    // What this block said before: Compress is "DELIBERATELY ABSENT" because
    // the Decompress probe in this file uses it as the oracle's input
    // generator, and porting your test-input generator is how a shared
    // misreading passes its own test. Re-checked at the top of this wave
    // rather than inherited, and the premise is FALSE in the only route where
    // it could bite: all three of the probe's Compression doors are
    // RT_ORIGINAL_ONCE, which is RtOriginal == ResolveExportRaw and does NOT
    // consult the port registry (rt_port.h:140,154). The probe keeps building
    // its corpus with the SHIPPED encoder no matter what is registered here,
    // and at cutover the probe is compiled out. See the banner above
    // CompressBody for the full argument.
    //
    // The dtor ??1Compression@System@@ (0x1800c4170) stays on the DLL. It is
    // not a census gap row, it is not in this lane's scope, and our ctor
    // allocates through the SAME MemoryAllocator door the DLL's dtor frees
    // through (RtMemAlloc -> ?MemAlloc@MemoryAllocator@, which decompiles to
    // exactly the ctor's inlined lock/Allocate/unlock), so an object built by
    // our ctor is destroyed correctly by retail's dtor. That is a DLL-present
    // statement; at cutover the dtor becomes a required row and is recorded as
    // such in docs/runtime_port/WAVE22_LANE_SYSBODY.md rather than left to be
    // rediscovered.
    //
    // Name only for both. 0x1800c4230 and 0x1800c41a0 appear in this tree
    // exactly three times between them and every hit is a comment (grep over
    // runtime/, engine/, code/, standalone/), so there is no by-address
    // consumer to satisfy and RegisterRuntimeAddr would only add a door
    // nothing asked for.
    RegisterRuntimeExport("?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z",
                          (void*)&RtSystem_Compression_Compress); ++n;
    RegisterRuntimeExport("??0Compression@System@@QEAA@XZ",
                          (void*)&RtSystem_Compression_Ctor); ++n;
    return n;
}
