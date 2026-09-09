// ============================================================================
// runtime/src/io/binary_writer_probe.cpp — the oracle for lane ENC-2's four.
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// ----------------------------------------------------------------------------
// WHAT IS COMPARED: A WHOLE RECORD, NOT FOUR FUNCTIONS SEPARATELY
// ----------------------------------------------------------------------------
// The four bodies only mean anything together — a Serialize that writes the
// right dword to the wrong position, or a destructor that back-patches the
// right length to the wrong offset, is invisible to any per-function check that
// does not look at the finished bytes. So each case runs a COMPLETE RECORD on
// each side:
//
//     CreateWriter -> Serialize(int&) -> Serialize(uint&) -> deletingDtor(1)
//
// and compares the finished BUFFER, byte for byte, plus the stream cursor. That
// is the emission sequence, which is what a write-path Serialize has to be
// judged on. It re-covers ??0BinaryWriter (merged in lane ENC) as a side
// effect, which is deliberate: the constructor and destructor are one framing
// scheme and this is the first time both halves can be exercised together.
//
// Each side gets its OWN MemoryStream over its OWN buffer, for the same reason
// as encoding_probe.cpp: the bodies MUTATE the stream, so they cannot share one.
//
// ----------------------------------------------------------------------------
// THE FRAMING ASSERTION — a check that does not go through THEIRS at all
// ----------------------------------------------------------------------------
// Byte equality with the shipped bodies proves the port matches. It does NOT
// prove that this lane's written description of the format is right, and a
// wrong description is what the next lane will build on. So each case also
// asserts the record's internal arithmetic directly:
//
//     buf[0] == 0xAAAAAAAA                       the head magic
//     buf[1] == 4 + 4*fieldCount                 the BACK-PATCHED length
//     buf[2] == version                          the version dword
//     buf[3 + fieldCount] == 0xBBBBBBBB          the trailer magic
//
// `buf[1]` is the load-bearing one: it is 4 + 4*fields and NOT 4*fields, because
// the constructor captures `_word24` before writing the version. If `_word20`
// and `_word24` had been swapped in the constructor that merged, this assertion
// fails even though ours and theirs might still agree with each other.
//
// ----------------------------------------------------------------------------
// CONTROLS
// ----------------------------------------------------------------------------
// EFFECTIVENESS — the corpus varies the version, the field values AND the field
// COUNT, so the back-patched length differs between cases. A run where every
// case produced identical bytes would be a uniform population and is failed
// explicitly, not reported green.
//
// NEGATIVE — one byte of a known-good record is flipped and the comparator must
// say no.
//
// **THE CONTROLS ACCUMULATE INDEPENDENTLY OF THE VERDICT.** In lane ENC's first
// run a reader failure zeroed a writer control, so `emitted=0` was printed for a
// reason that had nothing to do with emission and read exactly like the failure
// it was not. Here every counter is incremented before any `bad` check and none
// is guarded by `!bad`, so a control reading zero always means what it says.
//
// ----------------------------------------------------------------------------
// HONEST LIMITS
// ----------------------------------------------------------------------------
//  1. ONE STREAM CLASS (IO::MemoryStream). The bodies dispatch virtually.
//  2. THE FREE IS NOT COMPARED. `deletingDtor(flags=1)` releases the object
//     through MemAlloc's allocator; the probe compares the BYTES the record
//     left behind and that the call returns the object pointer. It does not and
//     cannot compare heap state between the two sides, so "the free happened
//     correctly" rests on the transcription, not on this oracle.
//  3. THE `flags & 2` ARM IS NOT EXERCISED and is a declared divergence — this
//     port aborts there instead of reproducing the shipped vector-dtor path.
//     See binary_writer.cpp's header for why it is unreachable.
//  4. `flags = 0` (destruct without freeing) is not exercised either; no caller
//     in this row uses it.
//  5. NOT PROOF THAT THE PORT RUNS. Attendance is a separate measurement — see
//     WAVE9_LANE_ENC2.md.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtIo {

using rt::io::BinaryCodec;
using rt::io::Serializer;

extern "C" BinaryCodec* RtIo_Encoding_CreateWriter(void*, Serializer*, int32_t);
extern "C" void  RtIo_BinaryWriter_SerializeInt(BinaryCodec*, int32_t*, const char*);
extern "C" void  RtIo_BinaryWriter_SerializeUInt(BinaryCodec*, uint32_t*, const char*);
extern "C" void* RtIo_BinaryWriter_DeletingDtor(BinaryCodec*, unsigned int);

namespace {

using CreateWriterFn = BinaryCodec* (*)(void*, Serializer*, int32_t);
using SerIntFn       = void  (*)(BinaryCodec*, int32_t*, const char*);
using SerUIntFn      = void  (*)(BinaryCodec*, uint32_t*, const char*);
using DelDtorFn      = void* (*)(BinaryCodec*, unsigned int);

// Same hand-built MemoryStream as encoding_probe.cpp. Only the fields the
// virtuals touch, plus a real vptr so the dispatch is genuine.
struct HandStream {
    void*    vptr;      int32_t direction;  int32_t pad0c;
    void*    encoding;  void*   data;
    uint8_t  ownsData;  uint8_t pad21[3];
    uint32_t cursor;    uint32_t size;      uint32_t pad2c;
};
static_assert(offsetof(HandStream, data)   == 0x18, "HandStream.data");
static_assert(offsetof(HandStream, cursor) == 0x24, "HandStream.cursor");
static_assert(offsetof(HandStream, size)   == 0x28, "HandStream.size");

void MakeStream(HandStream* s, void* vptr, void* buf, uint32_t size) {
    std::memset(s, 0, sizeof(*s));
    s->vptr = vptr; s->direction = 1; s->data = buf; s->size = size;
}

struct Res {
    int64_t cases = 0, records = 0, fieldsWritten = 0;
    int     distinct = 0, framingChecked = 0;
    bool    negativeFired = false;
    const char* bad = nullptr;
    int      badOff = -1;
    uint32_t badOurs = 0, badTheirs = 0;
};

void NoteFirstDiff(Res* r, const uint8_t* a, const uint8_t* b, int n) {
    if (r->badOff >= 0) return;
    for (int i = 0; i < n; ++i) {
        if (a[i] == b[i]) continue;
        r->badOff = i;
        const int base = i & ~3;
        std::memcpy(&r->badOurs,   a + base, 4);
        std::memcpy(&r->badTheirs, b + base, 4);
        return;
    }
}

}  // namespace

void RtIo_BinaryWriterProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::BinaryWriter record [probe]");

    void* vtblMemStream = ResolveExportRaw("??_7MemoryStream@IO@@6B@");
    if (vtblMemStream == nullptr) {
        printf("[BinWriter] probe SKIPPED: no shipped DLL to diff against "
                    "(expected once the DLL is gone).\n");
        return;
    }
    // The unexported factory and deleting dtor come by REBASED ADDRESS, not
    // through ResolveAddr: our own registration has taken that door, so
    // ResolveAddr would hand back OUR function and the probe would compare ours
    // to ours.
    const uintptr_t delta =
        reinterpret_cast<uintptr_t>(vtblMemStream) - 0x1801555b0ull;
    CreateWriterFn theirCreate = reinterpret_cast<CreateWriterFn>(0x1800a8080ull + delta);
    DelDtorFn      theirDelete = reinterpret_cast<DelDtorFn>(0x180023b50ull + delta);

    SerIntFn  theirSerInt  = reinterpret_cast<SerIntFn>(
        ResolveExportRaw("?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z"));
    SerUIntFn theirSerUInt = reinterpret_cast<SerUIntFn>(
        ResolveExportRaw("?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z"));
    if (!theirSerInt || !theirSerUInt) {
        printf("[BinWriter] probe SKIPPED: a shipped Serialize did not "
                    "resolve.\n");
        return;
    }

    Res r;
    const uint32_t kN = 128;

    // version, and how many int/uint field PAIRS to write. Varying the count is
    // what makes the back-patched length differ between cases.
    static const int32_t kVersions[] = { 0, 1, 9, -2 };
    static const int     kPairs[]    = { 1, 2, 3, 1 };

    uint8_t firstRecord[kN];
    bool haveFirst = false;

    for (int c = 0; c < 4; ++c) {
        uint8_t bufA[kN], bufB[kN];
        std::memset(bufA, 0x00, kN);
        std::memset(bufB, 0x00, kN);

        HandStream sa, sb;
        MakeStream(&sa, vtblMemStream, bufA, kN);
        MakeStream(&sb, vtblMemStream, bufB, kN);
        Serializer serA = { reinterpret_cast<rt::io::Stream*>(&sa), nullptr };
        Serializer serB = { reinterpret_cast<rt::io::Stream*>(&sb), nullptr };

        BinaryCodec* wa = RtIo_Encoding_CreateWriter(nullptr, &serA, kVersions[c]);
        BinaryCodec* wb = theirCreate(nullptr, &serB, kVersions[c]);
        ++r.cases;
        if (wa == nullptr || wb == nullptr) { r.bad = "CreateWriter returned null"; continue; }

        for (int p = 0; p < kPairs[c]; ++p) {
            int32_t  iv = 0x11111100 + p + c;
            uint32_t uv = 0xEE000000u + (unsigned)(p * 7 + c);
            int32_t  ib = iv;
            uint32_t ub = uv;
            RtIo_BinaryWriter_SerializeInt (wa, &iv, "i");
            theirSerInt                    (wb, &ib, "i");
            RtIo_BinaryWriter_SerializeUInt(wa, &uv, "u");
            theirSerUInt                   (wb, &ub, "u");
            r.fieldsWritten += 2;
        }

        void* ra = RtIo_BinaryWriter_DeletingDtor(wa, 1u);
        void* rb = theirDelete(wb, 1u);
        ++r.records;

        // ---- controls first, so nothing here is gated on the verdict --------
        if (!haveFirst) { std::memcpy(firstRecord, bufA, kN); haveFirst = true; }
        else if (std::memcmp(firstRecord, bufA, kN) != 0) ++r.distinct;

        if (c == 0) {
            uint8_t bad[kN];
            std::memcpy(bad, bufA, kN);
            bad[4] ^= 0xFF;                       // inside the back-patched length
            r.negativeFired = (std::memcmp(bad, bufB, kN) != 0);
        }

        // ---- the framing assertion, independent of THEIRS -------------------
        {
            const int fields = kPairs[c] * 2;
            uint32_t w[4];
            std::memcpy(&w[0], bufA + 0, 4);                    // head magic
            std::memcpy(&w[1], bufA + 4, 4);                    // back-patched len
            std::memcpy(&w[2], bufA + 8, 4);                    // version
            std::memcpy(&w[3], bufA + 12 + 4 * fields, 4);      // trailer magic
            ++r.framingChecked;
            if (!r.bad && w[0] != 0xAAAAAAAAu) r.bad = "head magic";
            if (!r.bad && w[1] != (uint32_t)(4 + 4 * fields))
                                               r.bad = "back-patched length";
            if (!r.bad && w[2] != (uint32_t)kVersions[c]) r.bad = "version dword";
            if (!r.bad && w[3] != 0xBBBBBBBBu) r.bad = "trailer magic";
        }

        // ---- the byte comparison against THEIRS ------------------------------
        if (!r.bad && ra != wa)               r.bad = "deleting dtor return (ours)";
        if (!r.bad && rb != wb)               r.bad = "deleting dtor return (theirs)";
        if (!r.bad && sa.cursor != sb.cursor) r.bad = "stream cursor";
        if (!r.bad && std::memcmp(bufA, bufB, kN) != 0) {
            NoteFirstDiff(&r, bufA, bufB, (int)kN);
            r.bad = "record bytes";
        }
    }

    const bool covered = r.records > 0 && r.fieldsWritten > 0 &&
                         r.distinct > 0 && r.framingChecked > 0 && r.negativeFired;
    const bool pass = (r.bad == nullptr) && covered;

    char sa[256], sb[256];
    std::snprintf(sa, sizeof(sa),
                  "cases=%lld records=%lld fields=%lld distinct=%d framing=%d neg=%s",
                  (long long)r.cases, (long long)r.records,
                  (long long)r.fieldsWritten, r.distinct, r.framingChecked,
                  r.negativeFired ? "FIRED" : "DID-NOT-FIRE");
    if (r.bad && r.badOff >= 0)
        std::snprintf(sb, sizeof(sb),
                      "DIVERGED on '%s' at +0x%02x (dword ours=%08x theirs=%08x)",
                      r.bad, r.badOff, r.badOurs, r.badTheirs);
    else if (r.bad)
        std::snprintf(sb, sizeof(sb), "DIVERGED on '%s'", r.bad);
    else if (!covered)
        std::snprintf(sb, sizeof(sb), "no divergence, but a CONTROL READS ZERO");
    else
        std::snprintf(sb, sizeof(sb),
                      "identical record bytes and cursor; framing arithmetic holds");
    RT_DIFF_SHADOW(pass, sa, sb);
    RT_DIFF_WORK(r.cases > 0);

    printf("[BinWriter] record probe: %s -- %s\n", sa, sb);
}

}  // namespace RtIo
