// ============================================================================
// runtime/src/ui/element_persist_probe.cpp — WAVE 10E lane VERIFY
// A REAL-STREAM DIFFERENTIAL for ?Persist@Element@UI@@ (slot 3, 0x180116870)
// ============================================================================
//
// THE DEBT. `Persist=0` in every recorded run — a REAL zero, from
// element_vtable.cpp's unconditional ATTENDANCE counter, not a `calls=` from a
// diff site. The row's own header declined an oracle:
//
//     ORACLE: `pin`, and NOT replay. Running the shipped body a second time on
//     the same stream would open and close a SECOND scope — on the write path
//     that is an extra record in the file.
//
// That objection is about REPLAY — one stream, two runs. It does not apply to
// TWO STREAMS, one run each, which is what this file does.
//
// ----------------------------------------------------------------------------
// WHY THE GAME NEVER REACHES IT, established before building anything
// ----------------------------------------------------------------------------
// Slot 3 is `Object::Internal`'s Persist hook, and the thing that dispatches it
// is `?PersistObject@InternalFactory@Object@@` (0x1800bff10), whose two arms
// both end in `(**(code **)(*vptr + 0x18))(...)` — byte offset 0x18 is slot 3.
// So Element::Persist runs exactly when an `InternalPtr<Element>` is walked by
// the object-graph persister. Elements are built by the UI at run time and no
// saved graph holds one, which is why the counter is zero and why no amount of
// gameplay would move it.
//
// It is NOT dead in the shipped binaries, and that was checked rather than
// assumed: `?Persist@Element@UI@@` is in Application-x64.exe's import directory
// at 0x140151940 (read with pefile), reached through the thunk at 0x14003a9b4,
// and that thunk's address appears in exactly TWO exe .rdata vtables —
// 0x14015c2b8 and 0x14015d458, which are slot 3 (+0x18) of
// `FrustumOutlineElement` (0x14015c2a0) and `CaptureBoxElement` (0x14015d440).
// Both are debug/screenshot elements that inherit the base hook. So the slot is
// live in the shipped tables and simply never dispatched.
//
// ----------------------------------------------------------------------------
// THE SUBJECT
// ----------------------------------------------------------------------------
// The body reads NOTHING from `this` — it is `(void)self` in our port and RCX
// is untouched in the shipped listing. Its entire input is the stream:
//
//     enc  = stream[+0x10];                       // IO::Encoding*, real
//     mode = stream[+0x08];                       // Stream::direction
//     node = enc->vt[mode == 0 ? 0x18 : 0x20](enc, &scope, 0);  // THREE args
//     if (node) node->vt[0](node, 1);
//
// So the subject is a REAL `IO::MemoryStream` over a real buffer, constructed
// through the exported ctor — the same choice dialog_settings_probe.cpp made,
// and for the same reason: the Encoding, the scope object and its deleting
// destructor are then the DLL's own, and the only thing under test is which
// slot this body picks and what it does with the result. NOTHING IS STUBBED.
//
// ----------------------------------------------------------------------------
// THE SELF-CONSISTENCY SCREENS COME FIRST, AND THEY EARNED THEIR PLACE
// ----------------------------------------------------------------------------
// The first version of this probe reported 2 mismatches out of 4 on the WRITE
// path, both at byte 8, with the cursors equal at 16. Before believing that as
// a port defect it needed one thing ruling out: there is no a-priori guarantee
// that two successive scopes emit the SAME bytes — a scope id, a handle or an
// allocation address inside the record would make every call differ from every
// other call, on either side, and a differential that does not screen for that
// reports its own harness as a port defect.
//
// So OURS-vs-OURS and THEIRS-vs-THEIRS are measured BEFORE the subject and
// printed with hex, and any volatile byte range they expose is excluded from
// the cross-side comparison BY NAME, with the remaining comparable range stated
// in the verdict line.
//
// **On this body the screen came back CLEAN — both sides are reproducible call
// to call, firstDiff = -1 on both — so the whole 16-byte record is comparable
// and the divergence was real.** The screen is kept because it is what made
// that sentence sayable: without it, "2 of 4 differ" and "my harness is
// unstable" are the same reading.
//
// ----------------------------------------------------------------------------
// WHAT IT FOUND ON ITS FIRST CLEAN RUN
// ----------------------------------------------------------------------------
// A real defect, and the reason this file exists rather than another paragraph
// arguing the row is fine. Both encoding slots are
// `(Encoding*, Serializer*, int version)` -- runtime/types/io/Serializer.h:104
// says so and every OTHER call site in the tree passes three arguments
// (goal_serialize.cpp:176, serializer.cpp:453, dialog_vtable.cpp:656,
// elementdescription_serialize.cpp:171, spritesheet_vtable.cpp:493). The
// Element::Persist port declared `void* (*)(void*, void*)` and passed TWO, so
// R8 carried whatever the caller left and the codec wrote it into the record as
// the version:
//
//     ours    aa aa aa aa 04 00 00 00 80 80 07 ab bb bb bb bb
//     retail  aa aa aa aa 04 00 00 00 00 00 00 00 bb bb bb bb
//
// reproducibly, on both slots. The shipped body's `XOR R8D,R8D` at 0x180116889
// is that 0: it sets up the `CMP dword ptr [RDX+0x8],R8D` on the next line AND
// is left live across the call. A byte pin cannot see this (it hashes the
// SHIPPED body), an entry counter cannot see this, and the row had no other
// oracle -- which is exactly the gap this lane was asked to close.
//
// ----------------------------------------------------------------------------
// EFFECTIVENESS, and the trap it exists to avoid
// ----------------------------------------------------------------------------
// `bytes` counts what the WRITE cases actually emitted, and the report says so
// in words when the comparable range collapses to nothing: a comparison over
// bytes neither side wrote passes over nothing and reads like a verdict
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// NEGATIVE CONTROL (BANISHED_RT_ELEMPERSIST_PROBE_NEGCTL=1): OUR side's stream
// is given the OPPOSITE `direction`, and nothing else changes. That is targeted
// at the body's ONLY decision — the 0x18 / 0x20 slot choice — which is also the
// only thing a transcription slip could get wrong here. It is scored on its own
// line and is never added to the subject's.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

extern "C" void RtUi_Element_Persist(void* self, void* stream);

namespace {

using PersistFn = void (*)(void*, void*);
using MsCtorFn  = void* (*)(void*);
using MsDtorFn  = void  (*)(void*);

const char* const kPersist = "?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z";

constexpr int kBufBytes = 256;

struct Counters {
    int cases = 0, mismatches = 0;
    int bytes = 0;          // effectiveness: bytes the WRITE cases emitted
    int nodeSeen = 0;       // effectiveness: cases whose cursor moved at all
    int ctlChanged = 0;     // control only
};

PersistFn g_theirs = nullptr;
MsCtorFn  g_msCtor = nullptr;
MsDtorFn  g_msDtor = nullptr;

// Run one Persist into a freshly built MemoryStream over `buf`, and return the
// cursor the scope left behind. `ours` selects which body runs; everything else
// is identical, which is what makes the four combinations comparable.
uint32_t RunOne(bool ours, uint8_t* buf, int dir) {
    std::memset(buf, 0x5A, kBufBytes);
    RtIo::MemoryStream ms;
    g_msCtor(&ms);
    ms.data = buf; ms.size = kBufBytes; ms.cursor = 0; ms.ownsData = 0;
    ms.base.direction = dir;
    if (ours) RtUi_Element_Persist(nullptr, &ms);
    else      g_theirs(nullptr, &ms);
    const uint32_t cur = ms.cursor;
    g_msDtor(&ms);
    return cur;
}

void Hex(const char* tag, const uint8_t* b, uint32_t n) {
    char line[160];
    int k = 0;
    for (uint32_t i = 0; i < n && i < 32 && k < 140; ++i)
        k += std::snprintf(line + k, sizeof(line) - (size_t)k, "%02x ", b[i]);
    line[k] = '\0';
    std::fprintf(stderr, "[ElemPersistProbe]     %-9s %s\n", tag, line);
}

int FirstDiff(const uint8_t* a, const uint8_t* b, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) if (a[i] != b[i]) return (int)i;
    return -1;
}

}  // namespace

void RtUiElementPersistProbe() {
    if (!RtWaveEnabled("BANISHED_RT_ELEMPERSIST_PROBE")) return;
    if (std::getenv("BANISHED_RT_ELEMPERSIST_PROBE") == nullptr) return;

    g_theirs = reinterpret_cast<PersistFn>(ResolveExportRaw(kPersist));
    g_msCtor = reinterpret_cast<MsCtorFn>(
        ResolveExportRaw("??0MemoryStream@IO@@QEAA@XZ"));
    g_msDtor = reinterpret_cast<MsDtorFn>(
        ResolveExportRaw("??1MemoryStream@IO@@UEAA@XZ"));
    if (!g_theirs || !g_msCtor || !g_msDtor) {
        std::fprintf(stderr,
            "[ElemPersistProbe] SKIPPED: persist=%p msCtor=%p msDtor=%p -- one "
            "did not resolve, so there is no authority to compare against "
            "(this is the cutover value, not a pass)\n",
            (void*)g_theirs, (void*)g_msCtor, (void*)g_msDtor);
        std::fflush(stderr);
        return;
    }

    const bool negctl = RtWaveEnabled("BANISHED_RT_ELEMPERSIST_PROBE_NEGCTL") &&
                        std::getenv("BANISHED_RT_ELEMPERSIST_PROBE_NEGCTL") != nullptr;

    static uint8_t b1[kBufBytes], b2[kBufBytes], b3[kBufBytes], b4[kBufBytes];
    uint32_t stableTo   = 0;      // bytes [0, stableTo) are cross-side comparable
    bool     volatileRec = false;

    // ---- STEP 1: the two self-consistency screens, scored FIRST -------------
    {
        const int dir = 1;                              // the WRITE path
        const uint32_t c1 = RunOne(true,  b1, dir);
        const uint32_t c2 = RunOne(true,  b2, dir);
        const uint32_t c3 = RunOne(false, b3, dir);
        const uint32_t c4 = RunOne(false, b4, dir);
        const int dOO = FirstDiff(b1, b2, c1 < c2 ? c1 : c2);
        const int dTT = FirstDiff(b3, b4, c3 < c4 ? c3 : c4);
        std::fprintf(stderr,
            "[ElemPersistProbe] SELF-CONSISTENCY (write path): ours-vs-ours "
            "cursors %u/%u firstDiff=%d ; theirs-vs-theirs cursors %u/%u "
            "firstDiff=%d\n",
            (unsigned)c1, (unsigned)c2, dOO, (unsigned)c3, (unsigned)c4, dTT);
        Hex("ours#1",   b1, c1);
        Hex("ours#2",   b2, c2);
        Hex("theirs#1", b3, c3);
        Hex("theirs#2", b4, c4);

        if (dOO >= 0 || dTT >= 0) {
            volatileRec = true;
            const int first = (dOO < 0) ? dTT
                            : (dTT < 0 ? dOO : (dOO < dTT ? dOO : dTT));
            stableTo = (uint32_t)first;
            std::fprintf(stderr,
                "[ElemPersistProbe]   THE RECORD IS NOT REPRODUCIBLE CALL TO "
                "CALL ON EITHER SIDE: it carries per-call state from byte %d on. "
                "So bytes [0,%u) and the cursor are the only things a cross-side "
                "comparison can score. That is a property of the DLL's Encoding, "
                "not of the port, and it is stated rather than compared around "
                "silently.\n", first, (unsigned)stableTo);
        } else {
            stableTo = c1;
            std::fprintf(stderr,
                "[ElemPersistProbe]   both sides are reproducible call-to-call, "
                "so the WHOLE %u-byte record is comparable.\n", (unsigned)c1);
        }
    }

    // ---- STEP 2: the cross-side differential, over the comparable range -----
    Counters c;
    static const int kDirs[] = { 1, 0, 1, 0 };

    for (size_t i = 0; i < sizeof(kDirs)/sizeof(kDirs[0]); ++i) {
        const int dir    = kDirs[i];
        const int ourDir = negctl ? (dir == 0 ? 1 : 0) : dir;

        const uint32_t co = RunOne(true,  b1, ourDir);
        const uint32_t ct = RunOne(false, b2, dir);

        ++c.cases;
        if (ct != 0) ++c.nodeSeen;
        if (dir == 1) c.bytes += (int)ct;
        if (negctl && ourDir != dir) ++c.ctlChanged;

        uint32_t n = stableTo;
        if (n > co) n = co;
        if (n > ct) n = ct;

        bool ok = (co == ct);
        int  fd = -1;
        if (ok && n > 0) { fd = FirstDiff(b1, b2, n); ok = (fd < 0); }
        if (!ok) {
            ++c.mismatches;
            if (c.mismatches <= 4) {
                std::fprintf(stderr,
                    "[ElemPersistProbe] MISMATCH dir=%d ourCursor=%u "
                    "theirCursor=%u comparedBytes=%u firstDiff=%d\n",
                    dir, (unsigned)co, (unsigned)ct, (unsigned)n, fd);
                Hex("ours",   b1, co);
                Hex("theirs", b2, ct);
            }
        }
    }

    std::fprintf(stderr,
        "[ElemPersistProbe] %s: %d case(s), %d mismatch(es) "
        "(compared: the cursor, plus bytes [0,%u)%s)\n",
        negctl ? "NEGATIVE CONTROL (ours gets the opposite direction)"
               : "real-stream differential",
        c.cases, c.mismatches, (unsigned)stableTo,
        volatileRec ? "; the rest is per-call state, see above" : "");
    std::fprintf(stderr,
        "[ElemPersistProbe]   effectiveness: bytes emitted on the WRITE cases="
        "%d, cases whose cursor moved=%d/%d\n",
        c.bytes, c.nodeSeen, c.cases);

    if (negctl) {
        if (c.ctlChanged == 0)
            std::fprintf(stderr,
                "[ElemPersistProbe]   *** VACUOUS: the control did not actually "
                "differ from the subject in any case. Scores nothing. ***\n");
        else if (c.mismatches == 0)
            std::fprintf(stderr,
                "[ElemPersistProbe]   *** CONTROL FAILED: %d case(s) ran the "
                "OTHER encoding slot and the comparator saw none of it. Its "
                "clean runs are not evidence. ***\n", c.ctlChanged);
        else
            std::fprintf(stderr,
                "[ElemPersistProbe]   control OK: %d of %d perturbed case(s) "
                "caught -- the comparator can tell slot 0x18 from slot 0x20.\n",
                c.mismatches, c.ctlChanged);
    } else if (c.nodeSeen == 0) {
        std::fprintf(stderr,
            "[ElemPersistProbe]   *** WEAK: no case moved the cursor, so the "
            "comparison was over bytes NEITHER side wrote and scores nothing. "
            "***\n");
    } else if (stableTo == 0) {
        std::fprintf(stderr,
            "[ElemPersistProbe]   *** WEAK: the comparable byte range is EMPTY. "
            "The only thing this run scored is the cursor -- say that, do not "
            "say the record matched. ***\n");
    }
    std::fflush(stderr);
}
