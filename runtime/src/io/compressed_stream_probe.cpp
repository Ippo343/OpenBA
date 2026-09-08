// ============================================================================
// runtime/src/io/compressed_stream_probe.cpp — Wave 9 lane PKG2, rung 2's oracle
// ============================================================================
//
// The lockstep dual-stream probe for the CompressedStream read path. It exists
// as its own file because it is a HARNESS, not a port: nothing in it is
// registered, and it must never be mistaken for a body.
//
// WHY A BESPOKE ORACLE INSTEAD OF THE USUAL REPLAY
//
// Two reasons, both from lane PICK, and both fatal to the ordinary
// call-ours-then-call-theirs pattern:
//
//  1. **A read mutates state a byte comparison cannot see.** `position` (+0x1c),
//     `activeWindow` (+0x20) and EVERY window's `useCount` change on every call.
//     Two implementations can hand back identical bytes and leave the object in
//     different places; the divergence then surfaces on a LATER read, attributed
//     to the wrong function. Comparing only the returned bytes IS the lossy
//     transform that finding is about.
//  2. **Replay is impossible anyway.** Calling the shipped Read after ours would
//     consume the stream twice and compare a second read against a first.
//
// So: two streams, byte-identical content, one driven entirely through our
// ports and the other entirely through the DLL's, compared AFTER EVERY READ.
//
// The isolation is real rather than hoped for: our `Read` calls our `SetWindow`
// by direct C++ call, and the DLL's calls its own by a direct relative CALL that
// no registration can redirect. Neither stack can be entered by accident.
//
// WHERE THE CONTENT COMES FROM
//
// The DLL's own writer (`Create` + `Write` + `Close`), which is the same trick
// rung 1 used with `Compress`: input that is valid by construction, produced by
// code this lane deliberately does not port, so a shared misreading of the
// format cannot pass its own test.
//
// WHAT IS COMPARED, AFTER EVERY SINGLE READ
//
//   the returned bool, the bytes, `position`, `activeWindow`, and for EVERY
//   window: `useCount`, `start`, `end`, and the full 0x8000 bytes of any window
//   whose `start >= 0`.
//
// The window CONTENT matters as much as the metadata: a wrong block index would
// leave the counters agreeing while the page held the wrong 32 KB.
//
// The probe stops at the first mismatch and names the failing term, per
// reference_shadow_predicate_must_name_its_failing_term.md — a probe that says
// "streams differ" after 400 reads has told you almost nothing.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

namespace RtIo {

using Window = CompressedStreamWindow;

// The four ports, called directly so the probe cannot accidentally measure the
// DLL through a door.
extern "C" bool RtIo_CompressedStream_Read(CompressedStream*, int32_t, void*);
int64_t RtIo_CompressedStreamGrowDelegations();
struct SetWindowBranchCounts {
    int64_t fastPath, scanHit, readMiss, inMemory, growDelegated;
};
const SetWindowBranchCounts& RtIo_SetWindowBranchCounts();

namespace {

typedef void* (*CmsCtorFn)(void*);
typedef void  (*CmsDtorFn)(void*);
typedef bool  (*CmsCreateFn)(void*);
typedef bool  (*CmsOpenFn)(void*);
typedef void  (*CmsCloseFn)(void*);
typedef bool  (*StreamWriteFn)(void*, int32_t, const void*);
typedef bool  (*ReadFn)(CompressedStream*, int32_t, void*);
typedef bool  (*MemStreamCreateFn)(void*, int32_t);
typedef bool  (*MemStreamOpenPtrFn)(void*, void*, int32_t);
typedef void  (*CsCommitFn)(void*);
typedef bool  (*CsOpenFn)(void*, void*, int32_t);

// sizeof(IO::CompressedMemoryStream) == 0x98, established by Wave 4 lane B from
// the class's deleting-destructor thunk (the element stride an array-delete
// uses). Its CompressedStream base is at +0x00.
constexpr size_t kCmsSize = 0x98;

const char* const kCmsCtor  = "??0CompressedMemoryStream@IO@@QEAA@XZ";
const char* const kCmsDtor  = "??1CompressedMemoryStream@IO@@UEAA@XZ";
const char* const kCmsCreate= "?Create@CompressedMemoryStream@IO@@QEAA_NXZ";
const char* const kCmsOpen  = "?Open@CompressedMemoryStream@IO@@QEAA_NXZ";
const char* const kCmsClose = "?Close@CompressedMemoryStream@IO@@UEAAXXZ";
const char* const kCsRead   = "?Read@CompressedStream@IO@@UEAA_NHPEAX@Z";
const char* const kCsWrite  = "?Write@CompressedStream@IO@@UEAA_NHPEBX@Z";
// The member MemoryStream lives at CompressedMemoryStream+0x68 (Wave 4 lane B
// pinned it three ways); ?Create@CompressedMemoryStream@IO@@ passes exactly
// `LEA RDX,[RCX+0x68]` as the source stream.
const char* const kMemCreate   = "?Create@MemoryStream@IO@@QEAA_NH@Z";
const char* const kMemOpenPtr  = "?Open@MemoryStream@IO@@QEAA_NPEAXH@Z";
const char* const kCsCommit    = "?Commit@CompressedStream@IO@@QEAAXXZ";
const char* const kCsOpen      = "?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z";

// Build one written-then-reopened CompressedMemoryStream over `n` bytes of
// `content`. Everything here is the DLL's: this is the SUBJECT FACTORY, and a
// subject built by the code under test would be worthless.
//
// RT_RAW_OK: harness-only. Every door in this file is reached solely from
// RtIo_CompressedStreamReadProbe(), which returns immediately unless the diff
// harness is on, so none of them is live at cutover.
// `step` names the stage that failed. A build helper that returns a bare false
// tells you the subject could not be made and nothing about why — which is how
// the first run of this probe produced "could not build the two subjects" and no
// way to act on it.
// The MemoryStream member's own fields, needed to lift the finished image out of
// the writer. Wave 4 lane B pinned these: _data +0x18, _ownsData +0x20,
// _cursor +0x24, _size +0x28, all relative to the MemoryStream, which itself sits
// at CompressedMemoryStream+0x68.
constexpr size_t kMemAt      = 0x68;
constexpr size_t kMemData    = 0x18;
constexpr size_t kMemCursor  = 0x24;

inline uint8_t* MemOf(void* cms) { return static_cast<uint8_t*>(cms) + kMemAt; }

// ---------------------------------------------------------------------------
// PHASE 1 — write `content` through a CompressedMemoryStream and lift the
// finished on-disk image out of it.
//
// THREE FACTS THAT COST TWO RUNS TO ESTABLISH, all of them in the DLL and none
// of them guessable from the class name:
//
//  1. `?Create@CompressedMemoryStream@IO@@` does NOT give its member
//     MemoryStream any storage. It is three instructions —
//     `LEA RDX,[RCX+0x68] ; MOV R8D,2 ; JMP Create@CompressedStream` — which
//     only points the compressed stream at the member and sizes the window set.
//     Writing then works until the first window fills, and the flush copies into
//     a stream with no buffer: `REP MOVSB` with RDI = 0, inside the DLL's memcpy
//     at Runtime-x64.dll + 0x8A2C9, two calls away from the actual mistake.
//  2. `?Open@MemoryStream@IO@@QEAA_NH@Z` — the obvious fix — is the WRONG
//     overload. It sets `direction = 0` (READING, 0x1800ae6de). The writer needs
//     `?Create@MemoryStream@IO@@QEAA_NH@Z`, which is the same body except
//     `MOV dword ptr [RBP+0x8],1` at 0x1800ae860. Two exports, one letter apart
//     in intent, and the compiler cannot tell them apart.
//  3. `?Commit@CompressedStream@IO@@` is what serialises the image — compress
//     every dirty window, compute each header's `sourceOffset` as a running sum
//     from `4 + headerCount*8`, then write the count, the directory and the
//     blocks to the source. It ends by calling `Release` (0x1800aa100), so the
//     writer is spent afterwards and the image must be lifted, not re-read.
bool BuildImage(void* cms, const uint8_t* content, int32_t n,
                uint8_t* imageOut, int32_t imageCap, int32_t* imageLen,
                const char** step) {
    // A LOCAL guard, not an inherited one. Every raw door below reaches the
    // shipped body on purpose (subject factory / oracle), and that is only
    // sound while the diff harness is on. Relying on the caller's check made
    // the guard invisible to tools/port_bypass_audit.py's per-site analysis
    // AND made it an invariant a future caller could break silently. Repeating
    // it costs one test and removes both problems.
    if (!RtDiffEnabled()) return false;
    *step = "ctor";
    RT_ORIGINAL_ONCE(CmsCtorFn, kCmsCtor)(cms);

    *step = "MemoryStream::Create";           // NOT Open — fact 2 above
    if (!RT_ORIGINAL_ONCE(MemStreamCreateFn, kMemCreate)(MemOf(cms), imageCap))
        return false;

    *step = "CompressedMemoryStream::Create";
    if (!RT_ORIGINAL_ONCE(CmsCreateFn, kCmsCreate)(cms)) return false;

    // Odd-sized pieces so the writer produces several blocks and block
    // boundaries fall inside a piece rather than between pieces.
    *step = "Write";
    int32_t off = 0;
    while (off < n) {
        int32_t piece = 4093;                       // prime, deliberately not 0x8000
        if (off + piece > n) piece = n - off;
        if (!RT_ORIGINAL_ONCE(StreamWriteFn, kCsWrite)(cms, piece, content + off))
            return false;
        off += piece;
    }

    *step = "Commit";
    RT_ORIGINAL_ONCE(CsCommitFn, kCsCommit)(cms);

    // Lift the image: the MemoryStream's write cursor is the length.
    uint8_t* const mem = MemOf(cms);
    void* data = nullptr;  std::memcpy(&data, mem + kMemData, sizeof data);
    int32_t len = 0;       std::memcpy(&len,  mem + kMemCursor, sizeof len);
    if (data == nullptr || len <= 0 || len > imageCap) { *step = "lift"; return false; }
    std::memcpy(imageOut, data, static_cast<size_t>(len));
    *imageLen = len;

    *step = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
// PHASE 2 — open a reader over the lifted image.
//
// `?Open@MemoryStream@IO@@QEAA_NPEAXH@Z` (the void*,int overload, six
// instructions) points the stream at a buffer it does NOT own and sets
// `direction = 0`, which is exactly what a reader wants. Both readers share the
// one image buffer: reading does not mutate it, so the two subjects are
// byte-identical by construction rather than by a copy that could differ.
//
// `?Open@CompressedMemoryStream@IO@@` then calls
// `Open(&base, &this->_memoryStream, 1)` — ONE window. Worth knowing rather than
// assuming: with a single window every block change is an eviction, so this
// subject drives GetAvailableWindow at every boundary instead of only once the
// window set fills.
// `windows` is why this does NOT call ?Open@CompressedMemoryStream@IO@@, which
// hardcodes ONE. The first run of this probe used it and the effectiveness
// control caught the consequence: with a single window `GetAvailableWindow`
// always returns 0, so neither the free-window scan nor the LRU maximum search
// is ever exercised, and SetWindow's scan-hit branch is unreachable because
// there is no second window to find. A green run in that configuration would
// have said nothing about three of the four bodies.
//
// ?Open@CompressedStream@IO@@ takes the count as its third argument, and
// ?Open@CompressedMemoryStream@IO@@ is literally
// `LEA RDX,[RCX+0x68] ; MOV R8D,1 ; JMP Open@CompressedStream` — so calling the
// base directly with a different count is the same code path, not a shortcut
// around it.
bool OpenReader(void* cms, uint8_t* image, int32_t len, int32_t windows,
                const char** step) {
    // A LOCAL guard, not an inherited one. Every raw door below reaches the
    // shipped body on purpose (subject factory / oracle), and that is only
    // sound while the diff harness is on. Relying on the caller's check made
    // the guard invisible to tools/port_bypass_audit.py's per-site analysis
    // AND made it an invariant a future caller could break silently. Repeating
    // it costs one test and removes both problems.
    if (!RtDiffEnabled()) return false;
    *step = "ctor";
    RT_ORIGINAL_ONCE(CmsCtorFn, kCmsCtor)(cms);

    *step = "MemoryStream::Open(ptr,len)";
    if (!RT_ORIGINAL_ONCE(MemStreamOpenPtrFn, kMemOpenPtr)(MemOf(cms), image, len))
        return false;

    *step = "CompressedStream::Open";
    if (!RT_ORIGINAL_ONCE(CsOpenFn, kCsOpen)(cms, MemOf(cms), windows)) return false;

    *step = nullptr;
    return true;
}

struct Mismatch { const char* term; int32_t index; };

// Compare EVERYTHING observable. Returns {nullptr,0} when the two streams agree.
Mismatch CompareStreams(CompressedStream* a, CompressedStream* b) {
    if (a->position != b->position)         return { "position", 0 };
    if (a->activeWindow != b->activeWindow) return { "activeWindow", 0 };
    if (a->windowSize != b->windowSize)     return { "windowSize", 0 };

    const Window* wa = static_cast<const Window*>(a->windowData);
    const Window* wb = static_cast<const Window*>(b->windowData);
    for (int32_t i = 0; i < a->windowSize; ++i) {
        if (wa[i].useCount != wb[i].useCount) return { "window.useCount", i };
        if (wa[i].start    != wb[i].start)    return { "window.start", i };
        if (wa[i].end      != wb[i].end)      return { "window.end", i };
        // The page itself. A wrong block index leaves every counter agreeing
        // and the 32 KB different, which is the defect worth catching.
        if (wa[i].start >= 0 &&
            std::memcmp(wa[i].buffer, wb[i].buffer,
                        kCompressedStreamWindowSize) != 0)
            return { "window.buffer", i };
    }
    return { nullptr, 0 };
}

}  // namespace

// ============================================================================
// One configuration: build two readers over `image` with `windows` windows each,
// drive them in lockstep, and report. Returns false on divergence.
// ============================================================================
namespace {

struct ConfigResult {
    bool     pass = false;
    int64_t  reads = 0;
    int64_t  bytes = 0;
    int64_t  refills = 0;        // window (re)fills — the REAL eviction count
    int64_t  scanHits = 0;       // reads served by a window that was already loaded
    Mismatch bad = { nullptr, 0 };
};

ConfigResult RunConfig(uint8_t* image, int32_t imageLen, int32_t windows,
                       const uint8_t* content, int32_t contentLen, bool revisit) {
    ConfigResult r;
    // A LOCAL guard, not an inherited one. Every raw door below reaches the
    // shipped body on purpose (subject factory / oracle), and that is only
    // sound while the diff harness is on. Relying on the caller's check made
    // the guard invisible to tools/port_bypass_audit.py's per-site analysis
    // AND made it an invariant a future caller could break silently. Repeating
    // it costs one test and removes both problems.
    if (!RtDiffEnabled()) return r;


    alignas(16) static uint8_t cmsOurs[kCmsSize];
    alignas(16) static uint8_t cmsTheirs[kCmsSize];
    static uint8_t bufOurs[8192];
    static uint8_t bufTheirs[8192];

    std::memset(cmsOurs, 0, sizeof cmsOurs);
    std::memset(cmsTheirs, 0, sizeof cmsTheirs);
    const char* stepA = nullptr;
    const char* stepB = nullptr;
    if (!OpenReader(cmsOurs, image, imageLen, windows, &stepA) ||
        !OpenReader(cmsTheirs, image, imageLen, windows, &stepB)) {
        std::printf("[CSRead] cfg(windows=%d) SKIPPED: reader open failed at '%s'/'%s'\n",
                    windows, stepA ? stepA : "(ok)", stepB ? stepB : "(ok)");
        r.bad = { "reader open", 0 };
        return r;
    }

    CompressedStream* const sa = reinterpret_cast<CompressedStream*>(cmsOurs);
    CompressedStream* const sb = reinterpret_cast<CompressedStream*>(cmsTheirs);

    ReadFn ours   = &RtIo_CompressedStream_Read;
    ReadFn theirs = RtOriginal<ReadFn>(kCsRead);          // RT_RAW_OK: harness-only
    if (theirs == nullptr) {
        std::printf("[CSRead] cfg(windows=%d) SKIPPED: cannot resolve the shipped Read\n",
                    windows);
        r.bad = { "resolve shipped Read", 0 };
        return r;
    }

    // Sizes that are not multiples of the window, sizes that straddle a block
    // boundary, and one larger than a whole block so a single call must cross
    // two windows.
    static const int32_t kSizes[] = { 1, 7, 100, 4093, 8192, 33, 1, 8191, 5000, 777 };
    constexpr int kNumSizes = static_cast<int>(sizeof(kSizes) / sizeof(kSizes[0]));

    // Track every window's `start` so a REFILL is counted even when the window
    // INDEX cannot change. That distinction is not pedantry: the first version
    // of this probe counted changes of `activeWindow`, which with a
    // single-window stream is 0 forever, and so reported "no eviction" on a run
    // that evicted on every block. An effectiveness control that measures a
    // proxy which is constant by construction is worse than none — it looks
    // like a finding.
    static int32_t prevStart[64];
    const int32_t nw = sa->windowSize < 64 ? sa->windowSize : 64;
    for (int32_t i = 0; i < nw; ++i)
        prevStart[i] = static_cast<const Window*>(sa->windowData)[i].start;

    int32_t total = 0;
    while (total < contentLen - 8192 && r.bad.term == nullptr) {
        const int32_t n = kSizes[r.reads % kNumSizes];

        // REVISIT MODE. Sequential reading never returns to a block that is
        // still resident, so SetWindow's SCAN-HIT branch is unreachable no
        // matter how many windows exist -- which is why the multi-window
        // configuration alone did NOT verify the LRU, and why the first two
        // effectiveness controls here both said it had.
        //
        // Jumping the position backwards by a few blocks puts the reader back
        // on a window the eviction policy has kept. `position` is written
        // directly on BOTH streams rather than through SetCurrentPos: that
        // export is unported, so routing one stream through it and not the other
        // would put a body this lane does not own inside the comparison.
        if (revisit && (r.reads % 5) == 4 && total > 3 * kCompressedStreamWindowSize) {
            const int32_t back = total - 2 * kCompressedStreamWindowSize;
            sa->position = back;
            sb->position = back;
            // `total` is deliberately NOT rewound with the position. It is the
            // loop's READ BUDGET, not the stream cursor, and rewinding it made
            // the loop non-terminating: five reads advance ~13 KB while each
            // revisit jumps back 64 KB, so progress was net negative and the
            // probe hung until the harness killed it. The content check reads
            // `posBefore`, so correctness does not depend on the two agreeing.
        }

        const int32_t posBefore = sa->position;
        std::memset(bufOurs, 0xAA, sizeof bufOurs);
        std::memset(bufTheirs, 0xAA, sizeof bufTheirs);

        // LOCKSTEP: one read on each, compare immediately.
        const bool ra = ours(sa, n, bufOurs);
        const bool rb = theirs(sb, n, bufTheirs);

        ++r.reads; r.bytes += n; total += n;

        if (ra != rb) { r.bad = { "return value", 0 }; break; }
        if (std::memcmp(bufOurs, bufTheirs, static_cast<size_t>(n)) != 0) {
            r.bad = { "bytes", static_cast<int32_t>(r.reads) }; break;
        }
        // Also RIGHT, not merely equal — two identically wrong readers agree.
        if (std::memcmp(bufOurs, content + (posBefore), static_cast<size_t>(n)) != 0) {
            r.bad = { "bytes vs the ORIGINAL content", static_cast<int32_t>(r.reads) };
            break;
        }
        r.bad = CompareStreams(sa, sb);

        bool refilled = false;
        const Window* const w = static_cast<const Window*>(sa->windowData);
        for (int32_t i = 0; i < nw; ++i) {
            if (w[i].start != prevStart[i]) { prevStart[i] = w[i].start; refilled = true; }
        }
        if (refilled) ++r.refills; else ++r.scanHits;
    }

    r.pass = (r.bad.term == nullptr);
    // RT_EXPORT_ONCE, not RT_ORIGINAL_ONCE, and the difference is not cosmetic:
    // ??1CompressedMemoryStream IS PORTED (Wave 4 lane B), so reaching the DLL's
    // copy here would be a genuine bypass -- our own tree calling the shipped
    // body for a function we own. tools/port_bypass_audit.py flagged exactly
    // these two lines, and it was right: the RT_RAW_OK marker on them was
    // borrowed from tools/rt_data_door_audit.py's convention, which this audit
    // does not share, and in any case an exemption would have been the wrong
    // answer. The SUBJECT FACTORY above deliberately uses the DLL (a subject
    // built by the code under test is worthless); TEARDOWN has no such reason,
    // and routing it through the port exercises Wave 4's dtor for free.
    RT_EXPORT_ONCE(CmsDtorFn, kCmsDtor)(cmsOurs);
    RT_EXPORT_ONCE(CmsDtorFn, kCmsDtor)(cmsTheirs);
    return r;
}

}  // namespace

// ============================================================================
void RtIo_CompressedStreamReadProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::CompressedStream::ReadPath [lockstep probe]");

    // ~200 KB so the stream spans several 32 KB blocks. Static: no allocator
    // traffic inside an oracle.
    constexpr int32_t kContent = 200000;
    static uint8_t content[kContent];
    static uint8_t image[kContent + 0x10000];

    // Compressible AND incompressible in the same stream, so blocks of both
    // kinds exist — `compressedSize == 0` (STORED) is a real branch in
    // DecompressFromSource and a corpus of only-compressible data never takes it.
    {
        uint32_t s = 0xC0FFEEu;
        for (int32_t i = 0; i < kContent; ++i) {
            if ((i / 8192) % 3 == 0) {
                content[i] = static_cast<uint8_t>(i & 0x1F);     // very compressible
            } else if ((i / 8192) % 3 == 1) {
                s = s * 1664525u + 1013904223u;                  // incompressible
                content[i] = static_cast<uint8_t>(s >> 24);
            } else {
                content[i] = static_cast<uint8_t>((i * 7) ^ (i >> 5));
            }
        }
    }

    // PHASE 1 — one writer produces the compressed image.
    alignas(16) static uint8_t cmsWriter[kCmsSize];
    int32_t imageLen = 0;
    const char* step = nullptr;
    std::memset(cmsWriter, 0, sizeof cmsWriter);
    if (!BuildImage(cmsWriter, content, kContent, image,
                    static_cast<int32_t>(sizeof image), &imageLen, &step)) {
        std::printf("[CSRead] probe SKIPPED: image build failed at '%s'\n", step);
        RT_DIFF_SHADOW(false, step ? step : "?",
                       "a compressed image is required before anything can be compared");
        return;
    }

    // PHASE 2 — TWO CONFIGURATIONS, because one window cannot exercise the LRU.
    //   windows=1: every block change is a refill; SetWindow's scan can never hit.
    //   windows=4: the free-window scan, the LRU maximum search AND SetWindow's
    //              scan-hit branch all become reachable.
    const ConfigResult c1 = RunConfig(image, imageLen, 1, content, kContent, false);
    const ConfigResult c4 = RunConfig(image, imageLen, 4, content, kContent, false);
    // Third configuration: 4 windows AND a schedule that returns to blocks the
    // eviction policy has kept, which is the only way SetWindow's scan-hit
    // branch and GetAvailableWindow's LRU maximum search are ever entered.
    const ConfigResult cR = RunConfig(image, imageLen, 4, content, kContent, true);

    const bool pass = c1.pass && c4.pass && cR.pass;
    // The LRU is only verified if the multi-window configuration actually
    // evicted AND actually re-used a loaded window.
    // The real control: the BRANCH COUNTERS inside SetWindow, not a proxy
    // inferred from window state outside it.
    const SetWindowBranchCounts& sw = RtIo_SetWindowBranchCounts();
    const bool lruExercised = sw.scanHit > 0 && sw.readMiss > 0 && sw.fastPath > 0;

    char a[192], b[192];
    std::snprintf(a, sizeof(a),
                  "w1{reads=%lld refills=%lld scanHits=%lld} w4{reads=%lld refills=%lld scanHits=%lld}",
                  (long long)c1.reads, (long long)c1.refills, (long long)c1.scanHits,
                  (long long)c4.reads, (long long)c4.refills, (long long)c4.scanHits);
    if (pass) {
        std::snprintf(b, sizeof(b), "identical on bytes, ORIGINAL content, position, "
                                    "activeWindow and every window record + page");
    } else {
        const ConfigResult& f = !c1.pass ? c1 : (!c4.pass ? c4 : cR);
        std::snprintf(b, sizeof(b), "DIVERGED on '%s' (index %d) at read #%lld in cfg %s",
                      f.bad.term ? f.bad.term : "?", f.bad.index, (long long)f.reads,
                      !c1.pass ? "windows=1" : (!c4.pass ? "windows=4" : "windows=4+revisit"));
    }
    RT_DIFF_SHADOW(pass, a, b);
    RT_DIFF_WORK(c1.reads + c4.reads + cR.reads > 0);

    std::printf("[CSRead] lockstep probe: image=%d bytes; cfg w=1 %lld reads, "
                "w=4 %lld reads, w=4+revisit %lld reads (%lld bytes total) -- %s\n",
                imageLen, (long long)c1.reads, (long long)c4.reads, (long long)cR.reads,
                (long long)(c1.bytes + c4.bytes + cR.bytes),
                pass ? "IDENTICAL (bytes, ORIGINAL content, position, activeWindow, "
                       "every window record and page)" : b);
    // THE EFFECTIVENESS CONTROL, and it is the branch counters INSIDE SetWindow
    // rather than anything inferred from window state outside it. Two earlier
    // versions of this line inferred it from a proxy and both were wrong: the
    // first counted changes of `activeWindow`, which is 0 forever on a
    // single-window stream, and the second counted "reads that did not refill",
    // which is dominated by the FAST PATH and says nothing about the scan.
    std::printf("[CSRead] SetWindow branches: fastPath=%lld scanHit=%lld readMiss=%lld "
                "inMemory=%lld growDelegated=%lld -- %s\n",
                (long long)sw.fastPath, (long long)sw.scanHit, (long long)sw.readMiss,
                (long long)sw.inMemory, (long long)sw.growDelegated,
                lruExercised
                    ? "the three READ-PATH branches are all exercised"
                    : "*** A READ-PATH BRANCH READS ZERO -- NOT a full verification ***");
    std::printf("[CSRead] note: inMemory and growDelegated are WRITE-side branches, so a "
                "read-only probe leaving them at 0 is expected; growDelegated>0 would mean "
                "a run took the UNPORTED grow path (a cutover blocker). "
                "Per-config refills: w1=%lld w4=%lld w4+revisit=%lld\n",
                (long long)c1.refills, (long long)c4.refills, (long long)cR.refills);
}

}  // namespace RtIo
