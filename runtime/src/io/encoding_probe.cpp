// ============================================================================
// runtime/src/io/encoding_probe.cpp — the oracle for the lane ENC four.
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// ----------------------------------------------------------------------------
// WHY THIS SHAPE, AND NOT A PER-CALL DIFF SITE
// ----------------------------------------------------------------------------
// The usual RT_DIFF idiom runs OURS for real and THEIRS into a private scratch,
// then compares the scratch. That idiom CANNOT be used for these four, and the
// reason is the whole point of the row: a BinaryReader constructor CONSUMES 12
// bytes from the stream and a BinaryWriter constructor EMITS 12 bytes to it.
// Running theirs "into a scratch object" would still push the live save stream
// 12 bytes further along, because the mutated thing is not the object, it is
// the STREAM the object was handed.
//
// So each side gets its OWN IO::MemoryStream over its OWN buffer, seeded
// identically, and the two are compared afterwards on three axes at once:
//     * the 0x30 constructed object, byte for byte, minus the gaps below
//     * the stream's cursor (how far the header advanced it)
//     * for the writer, THE EMITTED BYTES — the emission sequence, which is
//       what a Serialize-shaped port has to be judged on
// A writer that returns a perfect object while emitting nothing would pass the
// first two checks and fail the third. That is the check worth having.
//
// ----------------------------------------------------------------------------
// THE SUBJECT IS SYNTHETIC BUT NOT FAKE
// ----------------------------------------------------------------------------
// The MemoryStream is built BY HAND (vptr + _data + _cursor + _size) rather
// than through ??0MemoryStream, for the same reason memory_stream_probe.cpp
// does it: the constructor allocates an IO::Encoding that plays no part in what
// is under test here. What is NOT faked is the DISPATCH — the vptr is the real
// ??_7MemoryStream@IO@@6B@, so both sides go through genuine virtual Read /
// Write / GetCurrentPos bodies, which is exactly the mechanism the constructors
// exercise. BOTH SIDES USE THE SAME VTABLE, deliberately: that isolates the
// difference to the constructor under test instead of confounding it with a
// difference in the stream.
//
// ----------------------------------------------------------------------------
// THREE POINTER FIELDS THAT MUST DIFFER — NORMALISED, NOT COMPARED RAW
// ----------------------------------------------------------------------------
// The first draft of this probe compared all 0x30 bytes raw and reported a
// divergence on EVERY reader case. The port was not wrong; the ORACLE was.
// Each side is deliberately given its OWN Serializer over its OWN stream, so
// +0x10 (the Serializer*) and +0x18 (the Stream*) CANNOT be equal — comparing
// them asks the two sides to agree on an address that the harness itself made
// different. +0x00 (the vtable) has the same problem for a different reason:
// ours is resolved through the PORT-AWARE door and theirs is the shipped
// table, so a future port of the vtable itself would "diverge" here while
// nothing was wrong.
//
// All three are therefore NORMALISED to a tag before the comparison: each side
// is asked whether it points at ITS OWN correct object, and the tag records the
// ANSWER rather than the address. A side that stores the wrong pointer tags
// BAD while the other tags GOOD, so the check still fails — what it no longer
// does is fail for the one reason that was never a defect. This is the same
// move runtime/src/io/compressedstream.cpp makes with TagEncodingPointer, and
// it is the "oracle must not compare bytes whose difference is not a defect"
// rule applied to pointers rather than to padding.
//
// ----------------------------------------------------------------------------
// BYTES NEITHER SIDE WRITES — EXCLUDED, NOT COMPARED
// ----------------------------------------------------------------------------
// Of the 0x30 bytes, retail writes +0x00..+0x08, +0x10..+0x2b. It never writes:
//     +0x09..+0x0f   the 7 bytes of padding after the _allowCustomSerialize BYTE
//     +0x2c..+0x2f   the tail past _version
// Those are excluded. Comparing them would be comparing whatever the allocator
// last left there, which fails or passes for reasons that have nothing to do
// with the port. (Both scratches are also pre-filled with the same pattern, so
// the exclusion is belt-and-braces rather than load-bearing — but the exclusion
// is the honest statement of what is actually proven.)
//
// ----------------------------------------------------------------------------
// CONTROLS
// ----------------------------------------------------------------------------
// EFFECTIVENESS CONTROL — a green count is worthless if every case compared the
// same bytes. This probe requires that the compared object bytes DIFFER between
// at least two reader cases and between at least two writer cases; the corpus
// varies both the version argument and the stream contents to force that. A run
// where all cases are identical FAILS with "an outcome reads zero" rather than
// reporting a green that means nothing.
//
// NEGATIVE CONTROL — the comparator itself is exercised: a known-good case is
// copied, ONE byte is flipped, and the comparator must report NOT-equal. If the
// negative control does not fire, the verdict FAILS, because a comparator that
// never says no cannot be trusted when it says yes. Both controls are reported
// in the identity line, not merely asserted internally.
//
// ----------------------------------------------------------------------------
// HONEST LIMITS — what this probe does NOT establish
// ----------------------------------------------------------------------------
//  1. ONE STREAM CLASS. Every case runs over an IO::MemoryStream. The
//     constructors dispatch virtually, so a CompressedStream or a
//     BinaryFileStream would exercise different Read/Write/GetCurrentPos
//     bodies. Nothing here says the constructors behave correctly over those;
//     it says the constructors issue the right CALLS in the right ORDER with
//     the right ARGUMENTS, which is the part that belongs to them.
//  2. NO FAILURE PATH. Every Read and Write in the corpus succeeds. The
//     reader's "version argument survives if the third Read fails" branch is
//     described in encoding.cpp and is NOT covered here — the buffers are
//     always large enough. A short-buffer case would cover it; it is left out
//     because the DLL-side behaviour on a partially-consumed header is not
//     something this lane established, and guessing at it in a probe would
//     manufacture a green.
//  3. NO ALLOCATION-FAILURE PATH. MemAlloc returning null is unreachable here.
//  4. THE PROBE IS NOT PROOF THAT THE PORT RUNS. It compares bodies; it says
//     nothing about whether the real game ever reaches them. That is a
//     SEPARATE question and it is answered separately — see WAVE9_LANE_ENC.md,
//     which records the live call counts from the save/load path. A probe that
//     is green while the port is dead is the "uncalled port is not verified"
//     trap, and the two measurements are kept apart precisely so that neither
//     can be mistaken for the other.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

// ResolveExport / ResolveExportRaw already come from rt_port.h.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtIo {

using rt::io::BinaryCodec;
using rt::io::Serializer;

extern "C" BinaryCodec* RtIo_Encoding_CreateReader(void*, Serializer*, int32_t);
extern "C" BinaryCodec* RtIo_Encoding_CreateWriter(void*, Serializer*, int32_t);

namespace {

using FactoryFn  = BinaryCodec* (*)(void*, Serializer*, int32_t);
using MemFreeFn  = void (*)(void*);

// A MemoryStream, by hand. Only the four fields the virtuals touch, plus the
// vptr that makes the dispatch real. Layout from runtime/types/io/Stream.h.
struct HandStream {
    void*    vptr;        // +0x00
    int32_t  direction;   // +0x08
    int32_t  pad0c;       // +0x0c
    void*    encoding;    // +0x10
    void*    data;        // +0x18
    uint8_t  ownsData;    // +0x20
    uint8_t  pad21[3];    // +0x21
    uint32_t cursor;      // +0x24
    uint32_t size;        // +0x28
    uint32_t pad2c;       // +0x2c
};
static_assert(offsetof(HandStream, data)   == 0x18, "HandStream.data");
static_assert(offsetof(HandStream, cursor) == 0x24, "HandStream.cursor");
static_assert(offsetof(HandStream, size)   == 0x28, "HandStream.size");

void MakeStream(HandStream* s, void* vptr, void* buf, uint32_t size, int32_t dir) {
    std::memset(s, 0, sizeof(*s));
    s->vptr      = vptr;
    s->direction = dir;
    s->data      = buf;
    s->cursor    = 0;
    s->size      = size;
}

// The bytes retail never writes in a BinaryReader / BinaryWriter. See the
// header block: comparing these would compare allocator residue.
struct Gap { unsigned short off, len; };
const Gap kGaps[] = { {0x09, 7}, {0x2c, 4} };

void Blank(unsigned char* p) {
    for (const Gap& g : kGaps) std::memset(p + g.off, 0, g.len);
}

// Replace the three pointer fields with a verdict. 0x600D is "this side points
// at its own correct object", 0xBAD0 is "it does not"; the offset is folded in
// so a wrong field is named by the byte offset the comparator reports.
void Normalize(unsigned char* obj, const void* ownVtbl, const void* ownSer,
               const void* ownStream) {
    const struct { size_t off; const void* expect; } kPtrs[] = {
        { 0x00, ownVtbl }, { 0x10, ownSer }, { 0x18, ownStream },
    };
    for (const auto& e : kPtrs) {
        void* got = nullptr;
        std::memcpy(&got, obj + e.off, sizeof(got));
        const uint64_t tag =
            (got == e.expect ? 0x600D0000ull : 0xBAD00000ull) + e.off;
        std::memcpy(obj + e.off, &tag, sizeof(tag));
    }
}

struct Res {
    int64_t cases = 0, readers = 0, writers = 0, emitted = 0;
    int     distinctReader = 0, distinctWriter = 0;
    bool    negativeFired = false;
    const char* bad = nullptr;
    // Where the first byte divergence was, and what the two sides had there.
    // A verdict that only names the FIELD ("reader object bytes") costs a whole
    // extra run to turn into a cause; this turns it into a log line.
    int      badOff  = -1;
    uint64_t badOurs = 0, badTheirs = 0;
};

// Record the first differing offset, reading 8 bytes of context from each side.
void NoteFirstDiff(Res* r, const unsigned char* a, const unsigned char* b) {
    if (r->badOff >= 0) return;
    for (int i = 0; i < 0x30; ++i) {
        if (a[i] == b[i]) continue;
        r->badOff = i;
        const int base = i & ~7;
        std::memcpy(&r->badOurs,   a + base, 8);
        std::memcpy(&r->badTheirs, b + base, 8);
        return;
    }
}

}  // namespace

void RtIo_EncodingProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::Encoding::Create{Reader,Writer} [probe]");

    // THEIRS must be the SHIPPED bodies. The two factories are unexported, so
    // they come by address — and deliberately through ResolveAddr's DLL side is
    // not available, so this reads the module directly the way every other
    // probe in this directory does: if our own registration has already taken
    // the address door, ResolveAddr would hand back OUR function and the probe
    // would compare ours to ours. That is the "converting a door is not
    // satisfying it" trap inverted, and it is why the raw DLL base is used.
    void* dllBase = ResolveExportRaw("??_7MemoryStream@IO@@6B@");
    if (dllBase == nullptr) {
        std::printf("[Encoding] probe SKIPPED: no shipped DLL to diff against "
                    "(expected once the DLL is gone).\n");
        return;
    }
    // The shipped image base, recovered from a known export, so the two
    // unexported factory addresses can be rebased under ASLR without going
    // through the port-aware door.
    const uintptr_t delta =
        reinterpret_cast<uintptr_t>(dllBase) - 0x1801555b0ull;
    FactoryFn theirReader = reinterpret_cast<FactoryFn>(0x1800a8120ull + delta);
    FactoryFn theirWriter = reinterpret_cast<FactoryFn>(0x1800a8080ull + delta);

    void* vtblMemStream = dllBase;   // both sides dispatch through the same table
    // Port-aware MemFree, to pair with the port-aware MemAlloc our factory used.
    // Both objects come from the one MemoryAllocator singleton either way, so
    // this is about matching the allocator door, not about which heap.
    MemFreeFn memFree = RT_EXPORT_ONCE(MemFreeFn,
        "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
    if (memFree == nullptr) {
        std::printf("[Encoding] probe SKIPPED: MemFree absent.\n");
        return;
    }

    // Each side's OWN expected vtable: ours through the port-aware door, theirs
    // straight out of the shipped image. Today these are the same address; the
    // normalisation is what keeps that from becoming a silent requirement.
    const void* ourRdVtbl    = ResolveExport("??_7BinaryReader@IO@@6B@");
    const void* theirRdVtbl  = ResolveExportRaw("??_7BinaryReader@IO@@6B@");
    const void* ourWrVtbl    = ResolveExport("??_7BinaryWriter@IO@@6B@");
    const void* theirWrVtbl  = ResolveExportRaw("??_7BinaryWriter@IO@@6B@");
    if (!ourRdVtbl || !theirRdVtbl || !ourWrVtbl || !theirWrVtbl) {
        std::printf("[Encoding] probe SKIPPED: a BinaryReader/Writer vtable did "
                    "not resolve.\n");
        return;
    }

    Res r;
    const uint32_t kN = 64;

    // Four cases, chosen so the compared bytes cannot come out uniform: the
    // version differs in every case and the seeded header bytes differ too, so
    // _word20 (which the reader takes FROM the stream) differs as well.
    static const int32_t kVersions[] = { 0, 1, 7, -3 };
    static const uint8_t kSeeds[]    = { 0x11, 0x5A, 0xC3, 0x7E };

    unsigned char firstReader[0x30], firstWriter[0x30];
    bool haveFirstReader = false, haveFirstWriter = false;

    for (int c = 0; c < 4; ++c) {
        // ---------------- READER ----------------
        {
            uint8_t bufA[kN], bufB[kN];
            for (uint32_t i = 0; i < kN; ++i)
                bufA[i] = bufB[i] = static_cast<uint8_t>(kSeeds[c] + i * 3);

            HandStream sa, sb;
            MakeStream(&sa, vtblMemStream, bufA, kN, 0);
            MakeStream(&sb, vtblMemStream, bufB, kN, 0);

            Serializer serA = { reinterpret_cast<rt::io::Stream*>(&sa), nullptr };
            Serializer serB = { reinterpret_cast<rt::io::Stream*>(&sb), nullptr };

            BinaryCodec* oa = RtIo_Encoding_CreateReader(nullptr, &serA, kVersions[c]);
            BinaryCodec* ob = theirReader(nullptr, &serB, kVersions[c]);
            ++r.cases; ++r.readers;

            if (oa == nullptr || ob == nullptr) { r.bad = "reader returned null"; }
            else {
                unsigned char a[0x30], b[0x30];
                std::memcpy(a, oa, 0x30);
                std::memcpy(b, ob, 0x30);
                Blank(a); Blank(b);
                Normalize(a, ourRdVtbl,   &serA, &sa);
                Normalize(b, theirRdVtbl, &serB, &sb);
                if (!r.bad && std::memcmp(a, b, 0x30) != 0) {
                    NoteFirstDiff(&r, a, b); r.bad = "reader object bytes";
                }
                if (!r.bad && sa.cursor != sb.cursor)       r.bad = "reader stream cursor";
                if (!r.bad && sa.cursor != 12)              r.bad = "reader did not consume 12 bytes";
                if (!r.bad && std::memcmp(bufA, bufB, kN) != 0)
                                                            r.bad = "reader touched the buffer";
                // Effectiveness: do the cases actually differ from each other?
                if (!haveFirstReader) { std::memcpy(firstReader, a, 0x30); haveFirstReader = true; }
                else if (std::memcmp(firstReader, a, 0x30) != 0) ++r.distinctReader;

                // Negative control, once: flip one byte and demand a NOT-equal.
                if (c == 0) {
                    unsigned char bad[0x30];
                    std::memcpy(bad, a, 0x30);
                    bad[0x28] ^= 0xFF;                    // the version field
                    r.negativeFired = (std::memcmp(bad, b, 0x30) != 0);
                }
            }
            if (oa) memFree(oa);
            if (ob) memFree(ob);
        }

        // ---------------- WRITER ----------------
        {
            uint8_t bufA[kN], bufB[kN];
            std::memset(bufA, kSeeds[c], kN);
            std::memset(bufB, kSeeds[c], kN);

            HandStream sa, sb;
            MakeStream(&sa, vtblMemStream, bufA, kN, 1);
            MakeStream(&sb, vtblMemStream, bufB, kN, 1);

            Serializer serA = { reinterpret_cast<rt::io::Stream*>(&sa), nullptr };
            Serializer serB = { reinterpret_cast<rt::io::Stream*>(&sb), nullptr };

            BinaryCodec* oa = RtIo_Encoding_CreateWriter(nullptr, &serA, kVersions[c]);
            BinaryCodec* ob = theirWriter(nullptr, &serB, kVersions[c]);
            ++r.cases; ++r.writers;

            if (oa == nullptr || ob == nullptr) { r.bad = "writer returned null"; }
            else {
                unsigned char a[0x30], b[0x30];
                std::memcpy(a, oa, 0x30);
                std::memcpy(b, ob, 0x30);
                Blank(a); Blank(b);
                Normalize(a, ourWrVtbl,   &serA, &sa);
                Normalize(b, theirWrVtbl, &serB, &sb);
                // The emission total is accumulated BEFORE the verdict checks:
                // a control that only counts while everything passes cannot
                // distinguish "nothing was emitted" from "an earlier case
                // failed", and the first draft of this probe printed
                // emitted=0 for the second reason while reading like the first.
                r.emitted += sa.cursor;
                if (!r.bad && std::memcmp(a, b, 0x30) != 0) {
                    NoteFirstDiff(&r, a, b); r.bad = "writer object bytes";
                }
                if (!r.bad && sa.cursor != sb.cursor)       r.bad = "writer stream cursor";
                // THE EMISSION SEQUENCE — the check that a writer producing a
                // perfect object while emitting nothing cannot survive.
                if (!r.bad && std::memcmp(bufA, bufB, kN) != 0) r.bad = "writer emitted bytes";
                if (!r.bad && sa.cursor != 12)                  r.bad = "writer did not emit 12 bytes";

                if (!haveFirstWriter) { std::memcpy(firstWriter, a, 0x30); haveFirstWriter = true; }
                else if (std::memcmp(firstWriter, a, 0x30) != 0) ++r.distinctWriter;
            }
            if (oa) memFree(oa);
            if (ob) memFree(ob);
        }
    }

    // A green count over a uniform population proves nothing; require that the
    // corpus actually moved the compared bytes, and that the comparator said no
    // when it was handed a wrong answer.
    const bool covered = r.readers > 0 && r.writers > 0 &&
                         r.distinctReader > 0 && r.distinctWriter > 0 &&
                         r.emitted > 0 && r.negativeFired;
    const bool pass = (r.bad == nullptr) && covered;

    char sa[224], sb[224];
    std::snprintf(sa, sizeof(sa),
                  "cases=%lld readers=%lld writers=%lld emitted=%lld "
                  "distinctR=%d distinctW=%d neg=%s",
                  (long long)r.cases, (long long)r.readers, (long long)r.writers,
                  (long long)r.emitted, r.distinctReader, r.distinctWriter,
                  r.negativeFired ? "FIRED" : "DID-NOT-FIRE");
    if (r.bad && r.badOff >= 0)
        std::snprintf(sb, sizeof(sb),
                      "DIVERGED on '%s' at +0x%02x (qword ours=%016llx theirs=%016llx)",
                      r.bad, r.badOff, (unsigned long long)r.badOurs,
                      (unsigned long long)r.badTheirs);
    else if (r.bad)
        std::snprintf(sb, sizeof(sb), "DIVERGED on '%s'", r.bad);
    else if (!covered)
        std::snprintf(sb, sizeof(sb), "no divergence, but a CONTROL READS ZERO");
    else
        std::snprintf(sb, sizeof(sb),
                      "identical object bytes, stream cursor and emitted bytes");
    RT_DIFF_SHADOW(pass, sa, sb);
    RT_DIFF_WORK(r.cases > 0);

    std::printf("[Encoding] lockstep probe: %s -- %s\n", sa, sb);
}

}  // namespace RtIo
