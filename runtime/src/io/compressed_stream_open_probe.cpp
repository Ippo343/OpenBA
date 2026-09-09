// ============================================================================
// runtime/src/io/compressed_stream_open_probe.cpp — rung 4b-iii's oracle
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// THIS IS THE ORACLE 4b-iii PART 1 PROMISED. `Release` and `~CompressedStream`
// landed as "executed-but-undiffed" because both FREE memory and replaying the
// shipped body over the same object double-frees it. The way out, as stated
// there, was a CONSTRUCTED probe needing two independently built
// CompressedStreams — which needs `SetWindowCount`, which lands in this same
// rung. So the oracle arrives with the port, and the executed-but-undiffed
// entry closes here rather than staying open.
//
// SHAPE: two CompressedStreams, ours and theirs, built and driven side by side.
// Each gets its own storage throughout; nothing is shared, so neither can
// perturb the other's allocator state.
//
// THREE PHASES, each compared before the next runs (lockstep, not end-to-end):
//
//   1. SetWindowCount  — compare the grown window array, the array header, and
//                        activeWindow.
//   2. Open            — over a MemoryStream on a HAND-BUILT block directory,
//                        so the accept and both reject paths can be driven.
//   3. Release         — compare the nine fields it resets. This is the phase
//                        that could not exist before.
//
// *** THE +0x0c..+0x0f EXCLUSION ***
// `Array<Window>`'s tail init (0x180024c10) writes [+0x00] as a dword, [+0x04]
// as an UNALIGNED qword covering +0x04..+0x0b, and [+0x10] as a qword. It never
// writes +0x0c..+0x0f, even though the element stride is 0x18. Those four bytes
// are therefore whatever the allocator last left there and WILL differ between
// two independently allocated arrays.
//
// The exclusion below is derived from THAT DISASSEMBLY, not from observing a
// mismatch and widening a tolerance until it passed. It is a statement about
// which bytes retail writes, and if retail ever wrote them the exclusion would
// be a defect rather than a tolerance. Recorded this way because a tolerance
// that started life as a failing comparison is indistinguishable, later, from
// one that was reasoned — and only one of the two is sound.
//
// The window BUFFER pointer at +0x10 is excluded for the same reason winHandle
// was in rung 3: two independent 32 KB allocations differ by construction. What
// is compared instead is that both are non-null and that both buffers are
// ZEROED, which is the property SetWindowCount is actually responsible for.
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

extern "C" void RtIo_CompressedStream_SetWindowCount(CompressedStream*, int32_t);
extern "C" bool RtIo_CompressedStream_Open(CompressedStream*, Stream*, int32_t);
extern "C" void RtIo_CompressedStream_Release(CompressedStream*);

namespace {

typedef void (*SetWindowCountFn)(CompressedStream*, int32_t);
typedef bool (*OpenFn)(CompressedStream*, Stream*, int32_t);
typedef void (*ReleaseFn)(CompressedStream*);
typedef CompressedStream* (*CsCtorFn)(CompressedStream*);
typedef void* (*MsCtorFn)(void*);

struct Res {
    int64_t windows = 0, opens = 0, accepted = 0, rejected = 0, releases = 0;
    const char* bad = nullptr;
};

// The window record, compared field by field with the two unwritten regions
// named rather than memcmp'd over.
bool WindowsEqual(const uint8_t* a, const uint8_t* b, int32_t n, const char** why) {
    for (int32_t i = 0; i < n; ++i) {
        const uint8_t* wa = a + i * 0x18;
        const uint8_t* wb = b + i * 0x18;
        if (*reinterpret_cast<const uint32_t*>(wa + 0x00) !=
            *reinterpret_cast<const uint32_t*>(wb + 0x00)) { *why = "window +0x00"; return false; }
        if (*reinterpret_cast<const int64_t*>(wa + 0x04) !=
            *reinterpret_cast<const int64_t*>(wb + 0x04)) { *why = "window +0x04 (the -1)"; return false; }
        // +0x0c..+0x0f: NEVER WRITTEN by the tail init — see the header comment.
        const void* ba = *reinterpret_cast<const void* const*>(wa + 0x10);
        const void* bb = *reinterpret_cast<const void* const*>(wb + 0x10);
        if ((ba == nullptr) != (bb == nullptr)) { *why = "window buffer nullness"; return false; }
        if (ba) {
            // Not compared for EQUALITY (two independent allocations); compared
            // for the property SetWindowCount is responsible for.
            const uint8_t* p = static_cast<const uint8_t*>(ba);
            const uint8_t* q = static_cast<const uint8_t*>(bb);
            for (int k = 0; k < 256; ++k)
                if (p[k] != 0 || q[k] != 0) { *why = "window buffer not zeroed"; return false; }
        }
    }
    return true;
}

}  // namespace

void RtIo_CompressedStreamOpenProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::CompressedStream SetWindowCount/Open/Release [probe]");

    SetWindowCountFn theirSWC = reinterpret_cast<SetWindowCountFn>(
        ResolveAddr(Module::Runtime, 0x1800aa020));
    OpenFn    theirOpen = reinterpret_cast<OpenFn>(ResolveAddr(Module::Runtime, 0x1800aa560));
    ReleaseFn theirRel  = reinterpret_cast<ReleaseFn>(ResolveAddr(Module::Runtime, 0x1800aa100));
    CsCtorFn  csCtor    = reinterpret_cast<CsCtorFn>(ResolveAddr(Module::Runtime, 0x1800aa3d0));
    MsCtorFn  msCtor    = reinterpret_cast<MsCtorFn>(ResolveAddr(Module::Runtime, 0x1800ae710));
    if (!theirSWC || !theirOpen || !theirRel || !csCtor || !msCtor) {
        printf("[CSOpen] probe SKIPPED: no shipped bodies to diff against "
                    "(expected once the DLL is gone).\n");
        return;
    }

    Res r;
    const char* why = nullptr;

    // ---- phase 1: SetWindowCount ----------------------------------------
    // Two independently constructed CompressedStreams. The ctor is lane W's
    // port on both sides -- it is the SAME body for ours and theirs, which is
    // fine: the ctor is not what this probe is testing, and using it on both
    // sides keeps the two objects identical going in.
    alignas(16) uint8_t rawA[0x68], rawB[0x68];
    CompressedStream* A = reinterpret_cast<CompressedStream*>(rawA);
    CompressedStream* B = reinterpret_cast<CompressedStream*>(rawB);
    csCtor(A); csCtor(B);

    const int32_t kWindows = 3;
    RtIo_CompressedStream_SetWindowCount(A, kWindows);
    theirSWC(B, kWindows);
    r.windows = kWindows;

    if (A->windowSize != B->windowSize) why = "windowSize";
    else if (A->windowCap != B->windowCap) why = "windowCap";
    else if (A->activeWindow != B->activeWindow) why = "activeWindow";
    else if (!WindowsEqual(static_cast<const uint8_t*>(A->windowData),
                           static_cast<const uint8_t*>(B->windowData), kWindows, &why)) { /* why set */ }
    if (why) r.bad = why;

    // ---- phase 2: Open ---------------------------------------------------
    // A hand-built block directory over a MemoryStream, so all three outcomes
    // are driven rather than waited for: one WELL-FORMED image (accepted), one
    // whose declared block count overruns the source (rejected at the second
    // test), and one shorter than 4 bytes (rejected at the first).
    if (!r.bad) {
        // FOUR CASES, and the sizing of the IMAGE is part of the design, not
        // an afterthought — the first draft of this probe got that wrong and
        // manufactured a divergence. It declared 64 blocks in a 512-byte
        // buffer, so the directory ran off the end and BOTH sides read
        // adjacent stack memory. Two separate stack images have different
        // neighbours, so the comparison failed on bytes NEITHER side owned
        // (reference_oracle_must_not_compare_unwritten_bytes, in its
        // read-past-the-end form). It reported as `rejected=1`, which looked
        // like a legitimate reject path being exercised.
        //
        //   [0] well-formed          -> ACCEPTED (total == len)
        //   [1] blockCount*8 > len   -> rejected at the SECOND test, and the
        //                               image stays small so nothing is read
        //   [2] len < 4              -> rejected at the FIRST test
        //   [3] well-formed shape, one size field corrupted -> the directory IS
        //                               fully read and the SUM LOOP rejects.
        //                               This is the only case that exercises
        //                               the per-entry accumulation on a reject.
        struct Case { int32_t blocks; int32_t declaredLen; bool corrupt; };
        static const Case kCases[] = {
            {  2,   -1, false },   // -1 = "use the well-formed total"
            { 64,  100, false },
            {  0,    2, false },
            {  3,   -1, true  },
        };

        for (int c = 0; c < 4 && !r.bad; ++c) {
            // Big enough for the largest directory any case declares, so no
            // case can read past it.
            uint8_t imgA[2048], imgB[2048];
            std::memset(imgA, 0, sizeof(imgA));
            const int32_t nb = kCases[c].blocks;
            *reinterpret_cast<int32_t*>(imgA) = nb;
            int32_t total = 4 + nb * 8;
            for (int32_t i = 0; i < nb; ++i) {
                uint8_t* e = imgA + 4 + i * 8;
                *reinterpret_cast<uint16_t*>(e + 4) = 16;   // compressed size
                *reinterpret_cast<uint16_t*>(e + 6) = 32;   // raw size
                total += 16;
            }
            if (kCases[c].corrupt)                       // shape valid, sum wrong
                *reinterpret_cast<uint16_t*>(imgA + 4 + 4) = 999;
            const int32_t imgLen =
                kCases[c].declaredLen >= 0 ? kCases[c].declaredLen : total;
            std::memcpy(imgB, imgA, sizeof(imgA));

            alignas(16) uint8_t msA[0x30], msB[0x30];
            msCtor(msA); msCtor(msB);
            MemoryStream* MA = reinterpret_cast<MemoryStream*>(msA);
            MemoryStream* MB = reinterpret_cast<MemoryStream*>(msB);
            MA->data = imgA; MA->cursor = 0; MA->size = static_cast<uint32_t>(imgLen);
            MB->data = imgB; MB->cursor = 0; MB->size = static_cast<uint32_t>(imgLen);

            alignas(16) uint8_t oA[0x68], oB[0x68];
            CompressedStream* OA = reinterpret_cast<CompressedStream*>(oA);
            CompressedStream* OB = reinterpret_cast<CompressedStream*>(oB);
            csCtor(OA); csCtor(OB);

            const bool okA = RtIo_CompressedStream_Open(OA, reinterpret_cast<Stream*>(MA), 1);
            const bool okB = theirOpen(OB, reinterpret_cast<Stream*>(MB), 1);
            ++r.opens;
            if (okA) ++r.accepted; else ++r.rejected;

            if (okA != okB) r.bad = "Open return";
            else if (OA->headerSize != OB->headerSize) r.bad = "headerSize";
            else if (MA->cursor != MB->cursor) r.bad = "source cursor";
            else if (OA->headerSize > 0 &&
                     std::memcmp(OA->headerData, OB->headerData,
                                 static_cast<size_t>(OA->headerSize) * 8) != 0)
                r.bad = "header directory bytes";

            // ---- phase 3: Release, on the streams Open just populated -----
            if (!r.bad) {
                RtIo_CompressedStream_Release(OA);
                theirRel(OB);
                ++r.releases;
                if (OA->windowData != nullptr || OB->windowData != nullptr) r.bad = "windowData not nulled";
                else if (OA->windowSize != 0 || OB->windowSize != 0)     r.bad = "windowSize not zeroed";
                else if (OA->headerData != nullptr || OB->headerData != nullptr) r.bad = "headerData not nulled";
                else if (OA->headerSize != 0 || OB->headerSize != 0)     r.bad = "headerSize not zeroed";
                else if (OA->blockData != nullptr || OB->blockData != nullptr)   r.bad = "blockData not nulled";
                else if (OA->blockSize != 0 || OB->blockSize != 0)       r.bad = "blockSize not zeroed";
                else if (OA->compression != nullptr || OB->compression != nullptr) r.bad = "compression not nulled";
                else if (OA->position != 0 || OB->position != 0)         r.bad = "position not zeroed";
                // activeWindow goes back to -1, NOT 0 -- the ctor's value.
                else if (OA->activeWindow != -1 || OB->activeWindow != -1) r.bad = "activeWindow not -1";
                else if (OA->source != nullptr || OB->source != nullptr)  r.bad = "source not nulled";
            }
        }
    }

    // Release the phase-1 pair too, so the probe leaks nothing.
    RtIo_CompressedStream_Release(A);
    theirRel(B);
    r.releases += 2;

    // Effectiveness control: SetWindowCount must have grown a non-empty window
    // array, Open must have BOTH accepted and rejected, and Release must have
    // run on a populated stream. Any at zero and the green is about nothing.
    const bool covered = r.windows > 0 && r.accepted > 0 && r.rejected > 0 && r.releases > 0;
    const bool pass = (r.bad == nullptr) && covered;

    char sa[192], sb[192];
    std::snprintf(sa, sizeof(sa), "windows=%lld opens=%lld accepted=%lld rejected=%lld releases=%lld",
                  (long long)r.windows, (long long)r.opens, (long long)r.accepted,
                  (long long)r.rejected, (long long)r.releases);
    if (r.bad) std::snprintf(sb, sizeof(sb), "DIVERGED on '%s'", r.bad);
    else if (!covered) std::snprintf(sb, sizeof(sb), "no divergence, but an OUTCOME READS ZERO");
    else std::snprintf(sb, sizeof(sb), "identical on the window array, the block directory "
                                       "and all nine fields Release resets");
    RT_DIFF_SHADOW(pass, sa, sb);
    RT_DIFF_WORK(r.opens > 0);

    printf("[CSOpen] lockstep probe: windows=%lld opens=%lld (accepted=%lld rejected=%lld) "
                "releases=%lld -- %s\n",
                (long long)r.windows, (long long)r.opens, (long long)r.accepted,
                (long long)r.rejected, (long long)r.releases, sb);
    printf("[CSOpen] NOT compared, and deliberately: Window +0x0c..+0x0f (never written by "
                "the tail init at 0x180024c10 -- exclusion derived from that disassembly, not "
                "from a failing run) and the window buffer POINTER (two independent 32KB "
                "allocations); both buffers are checked non-null and zeroed instead.\n");
}

}  // namespace RtIo
