// ============================================================================
// runtime/src/io/compressed_stream_write_probe.cpp — Wave 19 lane CSTREAM
// The WRITER oracle: our bytes against RETAIL'S bytes, plus retail's reader as
// an independent structural validator.
// ============================================================================
//
// WHY THIS IS AN ORACLE AND NOT A SELF-CHECK
//
// The Wave 19 plan §5 names this case exactly: "an artefact-producing body is
// compared by its ARTEFACT". Compression produces one — a byte stream — and a
// byte stream can be compared to another byte stream with no interpretation.
//
// What this file deliberately is NOT is a round trip. Writing with our code and
// reading it back with our code proves only that the two agree with each other,
// which is the flaw reference_shadow_marker_is_not_an_oracle.md records in the
// ~250 RT_DIFF_SHADOW sites: they take the port's OWN verdict and never consult
// the DLL, so a shared misunderstanding of retail passes every time. Both
// screens below consult the DLL.
//
//   SCREEN A — byte identity.  The same content is written twice, once through
//     our twelve bodies and once through the shipped ones, and the two produced
//     images are memcmp'd. A divergence in ANY of block layout, header
//     directory, compressed payload, stored-vs-compressed decision or running
//     offset shows up as a byte difference.
//
//   SCREEN B — retail reads what we wrote.  Our image alone is handed to the
//     DLL's ?Open@CompressedStream (which validates `4 + n*8 + sum(block sizes)
//     == source size` EXACTLY — it is a total structural checker, not a
//     heuristic) and then to the DLL's ?Read, and the bytes that come back are
//     compared to the original content. This screen does not look at retail's
//     image at all, so it is independent of screen A and survives the failure
//     mode where BOTH writers are driven wrongly by this probe.
//
// The two screens are SCORED SEPARATELY and each is quoted with its own
// denominator (Wave 18 §4 doctrine §3: a UNION denominator overstates a blind
// control — reference_union_control_hides_a_blind_screen.md is the wave in
// which a 3/3 union hid a screen that caught 1 of 3).
//
// ---------------------------------------------------------------------------
// WHAT MAKES THE TWO SIDES GENUINELY INDEPENDENT
// ---------------------------------------------------------------------------
// reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md: a
// probe whose OURS side reaches the body BY ADDRESS compares retail with
// ITSELF, and one was dark for five waves on exactly that. So:
//
//   * the OURS side calls `RtIo_CompressedStream_*` by C++ symbol. No
//     ResolveAddr, no ResolveExport, no vtable that could be the DLL's.
//   * the THEIRS side uses RT_ORIGINAL_ONCE, which is ResolveExportRaw and
//     therefore the shipped body regardless of what is registered.
//   * retail's ?Commit reaches its ?CompressWindow, ?Release and its virtual
//     ?PreCommit by direct rel32 CALL and by its OWN vtable, so nothing on that
//     side can be entered by accident from ours.
//   * ours dispatches ?PreCommit through a vtable THIS FILE builds, whose slot
//     8 is our body. Stamping the DLL's table would have left our
//     PreCommit@CompressedMemoryStream — the body that SIZES THE OUTPUT BUFFER
//     — never entered, and screen A would have been green while one of the
//     twelve bodies was untested. The attendance counter below is what makes
//     that checkable rather than asserted.
//
// ---------------------------------------------------------------------------
// WHAT IS SHARED BETWEEN THE TWO SIDES, STATED SO NO GREEN IS OVERREAD
// ---------------------------------------------------------------------------
//   * `??0CompressedMemoryStream` and `?Create@MemoryStream` are the DLL's on
//     BOTH sides. They are the subject FACTORY; a subject built by the code
//     under test would be worthless, and neither is this lane's row.
//   * `?SetWindowCount` (ported, lane PKG2) and `?Release` (ported, lane PKG2)
//     are OURS on the ours side and the DLL's on the theirs side, as they should
//     be — but they are not this lane's rows and a divergence in them would be
//     misattributed here. Named so it is not mistaken for a claim about them.
//   * **The GROW branch of ?SetWindow is the DLL's on BOTH sides.**
//     compressed_stream_read.cpp §4 delegates it, and a write into a new block
//     always takes it. So this probe CANNOT attribute a divergence to that
//     branch, and equally cannot clear it. It is a named cutover blocker
//     already and this file does not change its status either way.
//
// ---------------------------------------------------------------------------
// CONTROLS
// ---------------------------------------------------------------------------
// 1. POSITIVE CONTROL, required to read non-zero before any green is believed.
//    A third image is built with `WriteBuggy`, which is our ?Write with ONE
//    change: it clamps the chunk against `w->end` instead of `w->start +
//    0x8000`. That is not an arbitrary corruption — it is the exact defect a
//    careless transcription produces, because ?Read (the body one file over)
//    genuinely does clamp against `end`. It mutates a value BOTH screens
//    compare (reference_control_must_pick_a_compared_subject.md: a control that
//    corrupts something nothing compares reports NOT CAUGHT and indicts a sound
//    instrument), and it produces a well-defined stream, not UB.
//    Scored on APPLICABLE cases only: the cases where the content is long
//    enough for the clamp to differ at all.
//
// 2. EFFECTIVENESS CONTROL. A corpus in which every block is stored never
//    exercises the JBE rule, and one that never spans a block boundary never
//    exercises the loop. The probe counts blocks produced, COMPRESSED blocks and
//    STORED blocks, and says so. A screen that can only say "no difference
//    found" is indistinguishable from one that measured nothing (Wave 19 §6.6);
//    these counters are what let this one say which it did.
//
// 3. ATTENDANCE. Every one of the twelve bodies increments a counter, printed
//    with the report. `calls=0` on a body is an UNVERIFIED body no matter how
//    green the memcmp is (reference_uncalled_port_is_not_verified.md), and
//    `calls=0 with worked>0` means the body ran and always took the uncovered
//    branch (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"
#include "../../types/system/String.h"

namespace RtIo {

// The twelve bodies under test, by C++ symbol — never by address.
// LANE IOBASE'S BODY, not this lane's — declared with ITS signature
// (`void* (const void* self, void** ret)`), which is the same ABI spelled
// through void* instead of System::String*. Both spell the HIDDEN RETURN SLOT
// as an explicit parameter, which is the whole point: a typedef that omits it
// is a wild write that still returns the right answer
// (reference_hidden_retslot_wild_write.md).
//
// This probe keeps comparing it against the DLL even though the body is not
// ours, because it is the ONLY differential oracle either lane has for it —
// IOBASE names "no differential oracle in this lane" as its own largest gap —
// and because a body with no call door cannot be verified by a live run at all.
extern "C" void* RtIo_Stream_GetSource(const void* self, void** ret);
extern "C" bool     RtIo_CompressedStream_Write(CompressedStream*, int32_t, const void*);
extern "C" bool     RtIo_CompressedStream_SetCurrentPos(CompressedStream*, int32_t);
extern "C" uint32_t RtIo_CompressedStream_GetCurrentPos(const CompressedStream*);
extern "C" uint32_t RtIo_CompressedStream_GetSize(const CompressedStream*);
extern "C" void     RtIo_CompressedStream_PreCommit(CompressedStream*);
extern "C" void     RtIo_CompressedMemoryStream_PreCommit(CompressedStream*);
extern "C" void     RtIo_CompressedStream_Close(CompressedStream*);
extern "C" void     RtIo_CompressedStream_Commit(CompressedStream*);
extern "C" void     RtIo_CompressedStream_CompressWindow(CompressedStream*, int32_t);
extern "C" bool     RtIo_CompressedStream_Create(CompressedStream*, Stream*, int32_t);
// Ported elsewhere; used to build the ours-side vtable so it is entirely ours.
extern "C" bool     RtIo_CompressedStream_Read(CompressedStream*, int32_t, void*);
// Ported elsewhere (lane PKG2); the positive control's buggy Write needs it.
extern "C" bool     RtIo_CompressedStream_SetWindow(CompressedStream*, int32_t);

namespace {

using Header = CompressedStreamHeader;

constexpr size_t kCmsSize   = 0x98;   // sizeof(IO::CompressedMemoryStream)
constexpr size_t kMemAt     = 0x68;   // the embedded MemoryStream
constexpr size_t kMemData   = 0x18;
constexpr size_t kMemCursor = 0x24;

inline uint8_t* MemOf(void* cms) { return static_cast<uint8_t*>(cms) + kMemAt; }

typedef void (*CmsCtorFn)(void*);
typedef bool (*MemStreamCreateFn)(void*, int32_t);
typedef bool (*MemStreamOpenPtrFn)(void*, void*, int32_t);
typedef bool (*CmsCreateFn)(void*);
typedef bool (*StreamWriteFn)(void*, int32_t, const void*);
typedef void (*CsCommitFn)(void*);
typedef void (*CsCloseFn)(void*);
typedef bool (*CsOpenFn)(void*, void*, int32_t);
typedef bool (*CsReadFn)(void*, int32_t, void*);

const char* const kCmsCtor    = "??0CompressedMemoryStream@IO@@QEAA@XZ";
const char* const kCmsCreate  = "?Create@CompressedMemoryStream@IO@@QEAA_NXZ";
const char* const kMemCreate  = "?Create@MemoryStream@IO@@QEAA_NH@Z";
const char* const kMemOpenPtr = "?Open@MemoryStream@IO@@QEAA_NPEAXH@Z";
const char* const kCsWrite    = "?Write@CompressedStream@IO@@UEAA_NHPEBX@Z";
const char* const kCsCommit   = "?Commit@CompressedStream@IO@@QEAAXXZ";
const char* const kCsClose    = "?Close@CompressedStream@IO@@UEAAXXZ";
const char* const kCsOpen     = "?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z";
const char* const kCsRead     = "?Read@CompressedStream@IO@@UEAA_NHPEAX@Z";

// ---------------------------------------------------------------------------
// Attendance. One counter per body, so a green screen cannot hide an unentered
// body. `bodies` are ours; they are incremented by the driver rather than by the
// bodies themselves, so nothing in the shipped path is instrumented.
// ---------------------------------------------------------------------------
// NOTE ON WHAT THIS COUNTS AND WHAT IT CANNOT. These counters are incremented
// by the DRIVER, at the call sites it owns, so nothing in the shipped path is
// instrumented. That leaves two bodies the driver never calls directly:
// ?CompressWindow (reached only from inside ?Commit) and, on the ours side,
// ?Release. Their attendance comes from the RT_DIFF harness's own per-body
// calls=/worked= report, which every body in compressed_stream_write.cpp opens
// with -- NOT from this struct. A counter that is structurally unable to see a
// body must not be read as that body's attendance
// (reference_a_screen_that_cannot_name_its_subject.md).
struct Attend {
    int64_t create = 0, write = 0, commit = 0;
    int64_t preCommitBase = 0, preCommitMem = 0, close = 0;
    int64_t getSize = 0, getCurrentPos = 0, setCurrentPos = 0, getSource = 0;
};
Attend g_at;

// Slot 8 of the ours-side vtable. A counting wrapper, not the body itself, so
// that "?Commit dispatched slot 8 and it landed on OUR PreCommit" is MEASURED
// rather than assumed. Without it, stamping the DLL's table by mistake would
// leave this body unentered and every screen still green.
void PreCommitMemCounted(CompressedStream* self) {
    ++g_at.preCommitMem;
    RtIo_CompressedMemoryStream_PreCommit(self);
}

// The ours-side vtable. Nine slots, ALL OURS, laid out exactly as
// rt_data_mint_tables.inc records ??_7CompressedMemoryStream@IO@@6B@ — the
// point being that ?Commit's `CALL [RAX+0x40]` must land on OUR
// PreCommit@CompressedMemoryStream and not on the DLL's.
//
// Slot 0 (the deleting-destructor thunk) is deliberately left null: nothing in
// this probe destroys a subject through the vtable, and pointing it at anything
// would be a claim this file cannot support. A null there is a crash if it is
// ever dispatched, which is the honest outcome — not a silent wrong answer.
void* g_oursVtable[9] = {
    nullptr,                                                  // 0 deleting dtor
    reinterpret_cast<void*>(&RtIo_Stream_GetSource),          // 1
    reinterpret_cast<void*>(&RtIo_CompressedStream_Write),    // 2
    reinterpret_cast<void*>(&RtIo_CompressedStream_Read),     // 3
    reinterpret_cast<void*>(&RtIo_CompressedStream_SetCurrentPos),  // 4
    reinterpret_cast<void*>(&RtIo_CompressedStream_GetCurrentPos),  // 5
    reinterpret_cast<void*>(&RtIo_CompressedStream_GetSize),        // 6
    reinterpret_cast<void*>(&RtIo_CompressedStream_Close),          // 7
    reinterpret_cast<void*>(&PreCommitMemCounted)                   // 8
};

// ---------------------------------------------------------------------------
// THE POSITIVE CONTROL, AND WHY IT IS NOT THE OBVIOUS ONE
// ---------------------------------------------------------------------------
// The obvious control is the realistic transcription defect: clamp the chunk
// against `w->end` instead of `w->start + 0x8000`, which is what ?Read (the
// body one file over) genuinely does. **That control was written, run, and it
// CRASHED** — ACCESS_VIOLATION, a WRITE of a page-aligned address inside
// ucrtbase's memcpy, at `WriteBuggy+0xc2`, symbolized off the crash log.
//
// That is disqualifying, and it is disqualifying by rule, not by taste: Wave 19
// §6.4 requires that **a control produce a comparable divergence, not UB.** A
// control that faults tells you nothing about the screen, because the screen
// never ran — and this one took the whole probe down with it before a single
// denominator was printed.
//
// It is recorded rather than quietly replaced, because the crash is itself a
// finding about the body under test: **retail's clamp against the window
// CAPACITY is load-bearing for memory safety, not just for correctness.** A
// port that clamped against `end` would be a heap overflow on the first write
// into a fresh window, not a wrong number — which is a stronger reason to get
// that line right than "the bytes would differ".
//
// So the control below corrupts the PAYLOAD instead: it complements the first
// byte of every piece written. It is artificial, and saying so is part of the
// claim. What it still does correctly is the thing that matters:
//
//   * it mutates a value BOTH screens compare — the byte lands in a block, the
//     block lands in the image, screen A memcmps that image against retail's
//     and screen B asks retail's reader to reproduce the original content from
//     it (reference_control_must_pick_a_compared_subject.md: a control that
//     corrupts a value NOTHING compares reports NOT CAUGHT and indicts a sound
//     instrument);
//   * it stays entirely inside the window buffer, so it is defined behaviour;
//   * it is applicable to every case with at least one byte of content.
//
// What it does NOT do, stated so the green is not overread: it does not prove
// the screens would catch a defect in block LAYOUT (a wrong sourceOffset, a
// wrong directory length). Screen B's Open would catch those structurally —
// it validates `4 + n*8 + sum(block sizes) == size` exactly — but that is an
// argument from reading Open's disassembly, not a measurement, and it is
// labelled as such.
bool WriteBuggy(CompressedStream* self, int32_t n, const void* src) {
    const uint8_t* in = static_cast<const uint8_t*>(src);
    bool firstOfPiece = true;
    while (n > 0) {
        RtIo_CompressedStream_SetWindow(self, self->position);
        CompressedStreamWindow* const w =
            &static_cast<CompressedStreamWindow*>(self->windowData)[self->activeWindow];
        const int32_t pos = self->position;
        int32_t chunk = n;
        const int32_t cap = w->start + kCompressedStreamWindowSize;
        if (pos + n > cap) chunk = cap - pos;
        uint8_t* const dst = static_cast<uint8_t*>(w->buffer) + (pos - w->start);
        std::memcpy(dst, in, static_cast<size_t>(chunk));
        if (firstOfPiece && chunk > 0) {        // <-- THE DEFECT, and only this
            dst[0] = static_cast<uint8_t>(~dst[0]);
            firstOfPiece = false;
        }
        self->position += chunk;
        n  -= chunk;
        in += chunk;
        if (self->position > w->end) w->end = self->position;
    }
    return true;
}

// Odd, prime piece size so block boundaries fall INSIDE a piece rather than
// between pieces — the read probe's reasoning, and it matters more here.
constexpr int32_t kPiece = 4093;

enum class Side { Ours, Theirs, Buggy };

// Build one image. `windows` is passed through so the window set can be varied.
// Returns the image length, or -1 with *step naming the stage that failed.
int32_t BuildImage(Side side, void* cms, const uint8_t* content, int32_t n,
                   uint8_t* imageOut, int32_t imageCap, int32_t windows,
                   const char** step) {
    // A LOCAL guard, not an inherited one — the same reason the read probe
    // repeats it: every raw door below is sound only while the harness is on,
    // and relying on the caller's check makes the guard invisible to
    // tools/port_bypass_audit.py's per-site analysis.
    if (!RtDiffEnabled()) return -1;

    std::memset(cms, 0, kCmsSize);

    // ---- shared factory (the DLL's on every side) -------------------------
    *step = "ctor";
    RT_ORIGINAL_ONCE(CmsCtorFn, kCmsCtor)(cms);

    *step = "MemoryStream::Create";     // Create, NOT Open: Open sets direction=0
    if (!RT_ORIGINAL_ONCE(MemStreamCreateFn, kMemCreate)(MemOf(cms), imageCap)) return -1;

    CompressedStream* const cs = static_cast<CompressedStream*>(cms);

    // ---- open the compressed stream for writing ---------------------------
    if (side == Side::Theirs) {
        *step = "CompressedMemoryStream::Create";
        // `LEA RDX,[RCX+0x68] ; MOV R8D,2 ; JMP Create@CompressedStream` — it
        // hardcodes TWO windows, which is why the ours side passes `windows`
        // explicitly rather than calling this and hoping they match.
        if (windows == 2) {
            if (!RT_ORIGINAL_ONCE(CmsCreateFn, kCmsCreate)(cms)) return -1;
        } else {
            typedef bool (*CsCreateFn)(void*, void*, int32_t);
            if (!RT_ORIGINAL_ONCE(CsCreateFn,
                    "?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z")(
                        cms, MemOf(cms), windows)) return -1;
        }
    } else {
        *step = "our Create";
        ++g_at.create;
        if (!RtIo_CompressedStream_Create(cs, reinterpret_cast<Stream*>(MemOf(cms)),
                                          windows)) return -1;
        // Stamp OUR vtable so ?Commit's virtual PreCommit lands on our body.
        cs->base.vptr = g_oursVtable;
    }

    // ---- write ------------------------------------------------------------
    *step = "Write";
    for (int32_t off = 0; off < n; ) {
        int32_t piece = kPiece;
        if (off + piece > n) piece = n - off;
        bool ok;
        if (side == Side::Theirs) {
            ok = RT_ORIGINAL_ONCE(StreamWriteFn, kCsWrite)(cms, piece, content + off);
        } else if (side == Side::Buggy) {
            ok = WriteBuggy(cs, piece, content + off);
        } else {
            ++g_at.write;
            ok = RtIo_CompressedStream_Write(cs, piece, content + off);
        }
        if (!ok) return -1;
        off += piece;
    }

    // ---- close, which is what a real caller does --------------------------
    // Both sides go through ?Close rather than straight to ?Commit, so ?Close's
    // `headerSize > 0 && direction == 1` guard is exercised on both sides too
    // and the two paths stay symmetric. Close tail-calls Commit.
    *step = "Close";
    if (side == Side::Theirs) {
        RT_ORIGINAL_ONCE(CsCloseFn, kCsClose)(cms);
    } else {
        ++g_at.close; ++g_at.commit;
        RtIo_CompressedStream_Close(cs);
    }

    // ---- lift -------------------------------------------------------------
    uint8_t* const mem = MemOf(cms);
    void* data = nullptr; std::memcpy(&data, mem + kMemData, sizeof data);
    int32_t len = 0;      std::memcpy(&len,  mem + kMemCursor, sizeof len);
    if (data == nullptr || len <= 0 || len > imageCap) { *step = "lift"; return -1; }
    std::memcpy(imageOut, data, static_cast<size_t>(len));
    *step = nullptr;
    return len;
}

}  // namespace

// ============================================================================
// The driver.
// ============================================================================
namespace {

struct Corpus { const char* name; int32_t len; int kind; };

// Four corpora, chosen so the counters below cannot all read zero on any of
// them and so each exercises something the others do not.
//   kind 0  mixed compressible / incompressible / patterned  (the read probe's)
//   kind 1  entirely incompressible  -> every block STORED, the JBE rule
//   kind 2  entirely compressible    -> every block COMPRESSED
//   kind 3  short, under one block   -> the single-block, no-boundary case
const Corpus kCorpora[] = {
    { "mixed-200k",         200000, 0 },
    { "incompressible-90k",  90000, 1 },
    { "compressible-90k",    90000, 2 },
    { "short-1000",           1000, 3 },
};

void Fill(uint8_t* p, int32_t n, int kind) {
    uint32_t s = 0xC0FFEEu;
    for (int32_t i = 0; i < n; ++i) {
        switch (kind) {
        case 1: s = s * 1664525u + 1013904223u; p[i] = uint8_t(s >> 24); break;
        case 2: p[i] = uint8_t(0xA5); break;
        case 3: p[i] = uint8_t(i & 0x7F); break;
        default:
            if ((i / 8192) % 3 == 0)      p[i] = uint8_t(i & 0x1F);
            else if ((i / 8192) % 3 == 1) { s = s * 1664525u + 1013904223u;
                                            p[i] = uint8_t(s >> 24); }
            else                          p[i] = uint8_t((i * 7) ^ (i >> 5));
        }
    }
}

// Read an image's directory without trusting anything of ours: the format is
// [uint32 count][count x 8-byte header][blocks], and ?Open validates it.
void CountBlocks(const uint8_t* img, int32_t len,
                 int64_t* blocks, int64_t* compressed, int64_t* stored) {
    if (len < 4) return;
    uint32_t n = 0; std::memcpy(&n, img, 4);
    if (n > 0x10000u || 4u + n * 8u > uint32_t(len)) return;
    *blocks += n;
    for (uint32_t i = 0; i < n; ++i) {
        Header h; std::memcpy(&h, img + 4 + i * 8, 8);
        if (h.compressedSize != 0) ++*compressed; else ++*stored;
    }
}

}  // namespace

// ============================================================================
void RtIo_CompressedStreamWriteProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::CompressedStream::WritePath [artefact oracle]");

    constexpr int32_t kMax   = 200000;
    constexpr int32_t kImage = kMax + 0x20000;
    static uint8_t content[kMax];
    static uint8_t imgOurs[kImage];
    static uint8_t imgTheirs[kImage];
    static uint8_t imgBuggy[kImage];
    static uint8_t readback[kMax];
    alignas(16) static uint8_t cmsA[kCmsSize], cmsB[kCmsSize], cmsC[kCmsSize];
    alignas(16) static uint8_t cmsR[kCmsSize];

    // Screen A and screen B are scored SEPARATELY, each with its own
    // denominator. A union would let one hide the other.
    int64_t aCases = 0, aPass = 0;
    int64_t bCases = 0, bPass = 0;
    int64_t ctrlApplicable = 0, ctrlCaught = 0;
    int64_t blocks = 0, compressedBlocks = 0, storedBlocks = 0;
    int64_t readerOpened = 0, readerAgree = 0;
    int64_t skipped = 0;
    const char* firstFail = nullptr;

    const int32_t kWindowSets[] = { 2, 1, 4 };

    for (const Corpus& c : kCorpora) {
        Fill(content, c.len, c.kind);
        for (int32_t windows : kWindowSets) {
            const char* step = nullptr;

            const int32_t nT = BuildImage(Side::Theirs, cmsB, content, c.len,
                                          imgTheirs, kImage, windows, &step);
            if (nT < 0) {
                printf("[CSWrite] SKIP %s w=%d: theirs failed at '%s'\n",
                       c.name, windows, step ? step : "?");
                ++skipped; continue;
            }
            const int32_t nO = BuildImage(Side::Ours, cmsA, content, c.len,
                                          imgOurs, kImage, windows, &step);
            if (nO < 0) {
                printf("[CSWrite] SKIP %s w=%d: ours failed at '%s'\n",
                       c.name, windows, step ? step : "?");
                ++skipped; continue;
            }

            // ---- SCREEN A: our bytes == retail's bytes ----------------------
            ++aCases;
            const bool aOk = (nO == nT) &&
                             std::memcmp(imgOurs, imgTheirs, size_t(nO)) == 0;
            if (aOk) ++aPass;
            else if (!firstFail) {
                firstFail = c.name;
                printf("[CSWrite] SCREEN A DIVERGED  corpus=%s windows=%d "
                       "ourLen=%d theirLen=%d\n", c.name, windows, nO, nT);
            }

            CountBlocks(imgOurs, nO, &blocks, &compressedBlocks, &storedBlocks);

            // ---- SCREEN B: retail's reader reads OUR image ------------------
            // Independent of screen A: it never looks at imgTheirs.
            ++bCases;
            std::memset(cmsR, 0, kCmsSize);
            RT_ORIGINAL_ONCE(CmsCtorFn, kCmsCtor)(cmsR);
            bool bOk = RT_ORIGINAL_ONCE(MemStreamOpenPtrFn, kMemOpenPtr)(
                           MemOf(cmsR), imgOurs, nO);
            if (bOk) {
                // ?Open validates 4 + n*8 + sum(block sizes) == size EXACTLY.
                bOk = RT_ORIGINAL_ONCE(CsOpenFn, kCsOpen)(cmsR, MemOf(cmsR), 2);
                if (bOk) ++readerOpened;
            }
            if (bOk) {
                std::memset(readback, 0, size_t(c.len));
                bOk = RT_ORIGINAL_ONCE(CsReadFn, kCsRead)(cmsR, c.len, readback);
                if (bOk && std::memcmp(readback, content, size_t(c.len)) == 0)
                    ++readerAgree;
                else
                    bOk = false;
            }
            if (bOk) ++bPass;
            else if (!firstFail) {
                firstFail = c.name;
                printf("[CSWrite] SCREEN B FAILED  corpus=%s windows=%d "
                       "(retail's reader could not recover our image)\n",
                       c.name, windows);
            }

            // ---- POSITIVE CONTROL, applicable cases only -------------------
            // Applicable to every case with at least one byte: the corrupted
            // byte is written on the first piece, which every case has.
            if (c.len > 0) {
                ++ctrlApplicable;
                const int32_t nX = BuildImage(Side::Buggy, cmsC, content, c.len,
                                              imgBuggy, kImage, windows, &step);
                const bool caught = (nX < 0) || (nX != nT) ||
                                    std::memcmp(imgBuggy, imgTheirs, size_t(nX)) != 0;
                if (caught) ++ctrlCaught;
                else
                    printf("[CSWrite] CONTROL NOT CAUGHT on %s w=%d -- screen A "
                           "is BLIND to a wrong Write clamp; treat its green as "
                           "MEANINGLESS\n", c.name, windows);
            }
        }
    }

    // ---- the pure readers, ours against the DLL's -------------------------
    // Small, separate, and scored on its own: these three never touch the image.
    int64_t pureCases = 0, purePass = 0;
    {
        std::memset(cmsR, 0, kCmsSize);
        RT_ORIGINAL_ONCE(CmsCtorFn, kCmsCtor)(cmsR);
        if (RT_ORIGINAL_ONCE(MemStreamCreateFn, kMemCreate)(MemOf(cmsR), 0x20000) &&
            RT_ORIGINAL_ONCE(CmsCreateFn, kCmsCreate)(cmsR)) {
            Fill(content, 50000, 0);
            RT_ORIGINAL_ONCE(StreamWriteFn, kCsWrite)(cmsR, 50000, content);
            CompressedStream* const cs =
                reinterpret_cast<CompressedStream*>(cmsR);

            typedef uint32_t (*U32Fn)(const void*);
            typedef void*    (*GetSourceFn)(const void*, void*);

            ++pureCases; ++g_at.getCurrentPos;
            if (RtIo_CompressedStream_GetCurrentPos(cs) ==
                RT_ORIGINAL_ONCE(U32Fn, "?GetCurrentPos@CompressedStream@IO@@UEBAIXZ")(cs))
                ++purePass;
            ++pureCases; ++g_at.getSize;
            if (RtIo_CompressedStream_GetSize(cs) ==
                RT_ORIGINAL_ONCE(U32Fn, "?GetSize@CompressedStream@IO@@UEBAIXZ")(cs))
                ++purePass;

            // GetSource: both must write a null qword through the RETURN SLOT
            // and return that slot. The slot is pre-poisoned so "never written"
            // and "written null" are DIFFERENT outcomes -- without the poison
            // this comparison would pass on a body that writes nothing, which is
            // exactly the hidden-return-slot defect it exists to catch.
            ++pureCases; ++g_at.getSource;
            RtSystem::String so, st;
            so.data = reinterpret_cast<char*>(~uintptr_t(0));
            st.data = reinterpret_cast<char*>(~uintptr_t(0));
            void* const ro = RtIo_Stream_GetSource(
                cs, reinterpret_cast<void**>(&so));
            void* const rt = RT_ORIGINAL_ONCE(GetSourceFn,
                "?GetSource@Stream@IO@@UEBA?AVString@System@@XZ")(cs, &st);
            if (so.data == st.data && so.data == nullptr &&
                ro == static_cast<void*>(&so) && rt == static_cast<void*>(&st))
                ++purePass;

            // SetCurrentPos: ours and the DLL's, on two objects in the same
            // state, must leave `position` equal. It returns true either way, so
            // the return value is not the subject -- the STATE is.
            ++pureCases; ++g_at.setCurrentPos;
            {
                typedef bool (*SetPosFn)(void*, int32_t);
                const int32_t before = cs->position;
                RtIo_CompressedStream_SetCurrentPos(cs, 1234);
                const int32_t ours = cs->position;
                cs->position = before;
                RT_ORIGINAL_ONCE(SetPosFn,
                    "?SetCurrentPos@CompressedStream@IO@@UEAA_NH@Z")(cs, 1234);
                if (ours == cs->position) ++purePass;
            }

            // PreCommit on the BASE is one instruction, `RET`. There is nothing
            // to compare and nothing to get wrong -- but calling it is what
            // turns its attendance counter non-zero, and an uncalled body is an
            // unverified one whatever its size.
            ++g_at.preCommitBase;
            RtIo_CompressedStream_PreCommit(cs);
        }
    }

    // ---- report ------------------------------------------------------------
    // Every green quoted WITH its denominator (Wave 19 section 6.1).
    const bool controlGreen = (ctrlApplicable > 0 && ctrlCaught == ctrlApplicable);

    printf("[CSWrite] CONTROL (a wrong Write clamp must be caught): %lld of %lld "
           "applicable -- %s\n", (long long)ctrlCaught, (long long)ctrlApplicable,
           controlGreen ? "GREEN" : "*** RED or NOT RUN ***");
    if (!controlGreen)
        printf("[CSWrite] the control did not read non-zero on every applicable "
               "case, so NO GREEN BELOW IS TRUSTED. The numbers are printed "
               "rather than suppressed, but they are not evidence.\n");

    printf("[CSWrite] SCREEN A (our bytes == retail's bytes): %lld of %lld cases\n",
           (long long)aPass, (long long)aCases);
    printf("[CSWrite] SCREEN B (retail's reader recovers our image): %lld of %lld "
           "cases; retail's Open ACCEPTED %lld, Read+compare AGREED %lld\n",
           (long long)bPass, (long long)bCases,
           (long long)readerOpened, (long long)readerAgree);
    printf("[CSWrite] PURE READERS (ours == the DLL's): %lld of %lld\n",
           (long long)purePass, (long long)pureCases);
    printf("[CSWrite] EFFECTIVENESS: %lld blocks written by us, %lld COMPRESSED, "
           "%lld STORED; %lld case(s) skipped\n",
           (long long)blocks, (long long)compressedBlocks,
           (long long)storedBlocks, (long long)skipped);
    if (compressedBlocks == 0 || storedBlocks == 0)
        printf("[CSWrite] EFFECTIVENESS WARNING: one of the two block kinds was "
               "never produced, so the compressedSize > uncompressedSize rule is "
               "UNEXERCISED and screen A's green does not cover it\n");
    printf("[CSWrite] ATTENDANCE ours: Create=%lld Write=%lld Commit=%lld "
           "PreCommit(base)=%lld PreCommit(mem)=%lld Close=%lld GetSize=%lld "
           "GetCurrentPos=%lld SetCurrentPos=%lld GetSource=%lld\n",
           (long long)g_at.create, (long long)g_at.write, (long long)g_at.commit,
           (long long)g_at.preCommitBase, (long long)g_at.preCommitMem,
           (long long)g_at.close, (long long)g_at.getSize,
           (long long)g_at.getCurrentPos, (long long)g_at.setCurrentPos,
           (long long)g_at.getSource);
    // NOTE ON THE PREVIOUS WORDING OF THIS LINE. It used to name
    // ?Close@CompressedStream as NOT COVERED. That was written before both sides
    // of BuildImage were routed through ?Close instead of straight to ?Commit,
    // and it was STALE from that moment: the ATTENDANCE line above reads
    // Close=24 in the very same report. A "not covered" claim sitting beside a
    // non-zero counter for the same body is exactly the kind of thing a reader
    // believes because it sounds more specific. Corrected here; NO measured
    // number changes, and the archived log in
    // docs/runtime_port/wave19_cstream/run_gameplay/ still carries the old text.
    printf("[CSWrite] NOT COVERED HERE: ?Close@CompressedFileStream (needs a real "
           "file and lane FILE's ?CloseFile -- it gets NO RtDiff row at all, which "
           "is the honest signal that it was never entered), and the SetWindow "
           "GROW branch, which is the DLL's on BOTH sides by construction and can "
           "be neither blamed nor cleared here.\n");
    printf("[CSWrite] READ THE RtDiff TABLE AS `worked=`, NOT `calls=`: these "
           "bodies carry RT_DIFF_BEGIN/RT_DIFF_WORK and no in-body oracle, so "
           "calls=0 is BY CONSTRUCTION and says nothing at all. "
           "?PreCommit@CompressedStream further reports worked=0 because it is a "
           "no-op that passes false -- its attendance is the PreCommit(base) "
           "counter above, not that row.\n");

    RT_DIFF_SHADOW(controlGreen && aCases > 0 && aPass == aCases && bPass == bCases,
                   "writer artefact matches retail and retail reads it back",
                   "SEE THE PRINTED DENOMINATORS -- this predicate is a summary, "
                   "not the measurement");
    RT_DIFF_WORK(aCases > 0);
}

}  // namespace RtIo
