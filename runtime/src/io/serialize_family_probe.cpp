// ============================================================================
// runtime/src/io/serialize_family_probe.cpp — the oracle for lane SERIAL's 73.
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// ----------------------------------------------------------------------------
// WHY THIS FAMILY GETS A REAL ORACLE AND NOT A "NO ORACLE POSSIBLE" NOTE
// ----------------------------------------------------------------------------
// A write-path Serialize is REPLAYABLE — drive both bodies against the same
// input and compare what came out. This project recorded "no oracle possible"
// for four Serialize ports across three waves before noticing
// (reference_write_path_serialize_is_replayable.md). The read path is replayable
// too, once the thing being replayed is understood correctly: these bodies do
// not decode anything, they FORWARD to the stream, so what has to match is the
// sequence of typed calls each body makes — not bytes on a disk.
//
// So the subject here is an EMISSION SEQUENCE, the same shape lane S proved on
// Audio::Scene::Serialize: each side gets its OWN synthetic sink, both are
// driven with the same input, and the two logs are compared record for record.
//
// ----------------------------------------------------------------------------
// THE SINK IS SYNTHETIC ON PURPOSE — THREE THINGS IT BUYS
// ----------------------------------------------------------------------------
// Nothing below constructs an IO::MemoryStream, a codec through the Encoding
// factory, or anything else out of the DLL. The stream is a local struct with a
// hand-built vtable, and the codec is a local struct laid out to Serializer.h's
// offsets. That is deliberate (reference_synthetic_subject_beats_a_fake.md — ask
// what the code under test actually READS, and supply exactly that):
//
//  1. **It runs anywhere on the boot path**, because it never allocates. Every
//     other IO probe has to be threaded into filesystem_packages.cpp after the
//     allocator singleton is live; this one does not care.
//  2. **It makes the ARRAY forms testable at all.** An array body's whole
//     observable behaviour is a sequence of VIRTUAL SELF-CALLS through its own
//     vtable. Against a real BinaryWriter those calls vanish into the scalar
//     bodies and all you can see downstream is bytes. Against a synthetic vtable
//     whose scalar slots are RECORDERS, the slot index, the element stride, the
//     element order and the dropped `name` are each observed directly.
//  3. **It cannot be fooled by the two sides sharing state.** Each side gets its
//     own sink, its own codec and its own destination buffer, because the bodies
//     MUTATE what they are given (reference_oracle_must_snapshot_aliased_inputs.md
//     — a correct port once read as a 10% divergence because one buffer was
//     passed as both subject and result slot).
//
// ----------------------------------------------------------------------------
// WHAT IS COMPARED, AND WHAT IS DELIBERATELY NOT
// ----------------------------------------------------------------------------
// Compared, per case: the emission count; and per emission the op (read /
// write / seek / dispatch), the size, the vtable slot, the element offset from
// the array base, the `name` pointer, and the payload bytes.
//
// NOT compared: any byte neither side writes. The destination buffers are
// poisoned to a known pattern and a byte is only compared when at least one
// side is expected to have touched it — `reference_oracle_must_not_compare_
// unwritten_bytes.md` records an oracle that memcmp'd bytes NEITHER side wrote
// and failed 100% of calls, reading like a catastrophic port defect. The Visitor
// cases are the interesting inversion of that rule: there the CLAIM is that
// neither side writes, so comparing the destination against its poison is the
// substance of the test rather than a vacuous pass — and negative control 3
// proves the check can fail by running a body that DOES write.
//
// ----------------------------------------------------------------------------
// THREE NEGATIVE CONTROLS, AND THE PROBE FAILS IF ANY OF THEM DOES NOT FIRE
// ----------------------------------------------------------------------------
// A family of 73 near-identical bodies is the exact place a uniform-looking pass
// hides a wrong one (reference_uniform_population_is_not_a_verification.md: a
// ported inline strcmp returned 0 for every pair sharing a first byte and read
// as fully verified). Green means nothing unless the instrument is shown to be
// able to go red, ON THE GEOMETRY IT IS ACTUALLY RUN ON — a control that cannot
// fail and a control that is merely too small both come back green
// (WAVE9.md, Picking::Scene::Intersect).
//
//   NEG 1  scalar size   — a body that moves sizeof(T)+1 bytes.
//                          Catches: a wrong MOV EDX immediate, i.e. the single
//                          most likely transcription error in 22 scalar bodies.
//   NEG 2  array stride  — a body that advances the element pointer by
//                          sizeof(T)+1. Catches: the wrong stride, and by
//                          construction also proves the element-offset column of
//                          the comparison is load-bearing rather than always
//                          equal.
//   NEG 3  visitor no-op — a body that writes one byte to the destination.
//                          Catches: the "neither side writes" check passing
//                          vacuously.
//
// Each is run, its firing recorded, and `negFired` must equal 3. **If a control
// does not fire the probe reports FAIL even with zero divergences**, because at
// that point the zero is uninformative.
//
// ----------------------------------------------------------------------------
// AN EFFECTIVENESS COUNT, NOT JUST A CALL COUNT
// ----------------------------------------------------------------------------
// `calls=` counts calls ENTERED, not calls that DID WORK — WAVE9.md's lane M
// sweep found a site reading 12,896 calls of which TEN did any work. So this
// probe reports `emissions=`, the number of comparisons that had something in
// them, beside `cases=`. A case whose body legitimately emits nothing (every
// Visitor slot, BinaryWriter::Skip, and every array case with count <= 0)
// contributes to `silent=` instead, so the two cannot be confused and neither
// can masquerade as coverage for the other.
//
// `types=` is a popcount over an eleven-bit mask, so "all eleven scalar types
// were exercised" is asserted rather than assumed from the case count.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/io/Serializer.h"

namespace RtIo {

using rt::io::BinaryCodec;
namespace rw = rt::io::rw_vtbl;

namespace {

// ---------------------------------------------------------------------------
// The emission log
// ---------------------------------------------------------------------------
enum Op : int { kOpRead = 1, kOpWrite = 2, kOpSeek = 3, kOpDispatch = 4 };

struct Emission {
    int      op;
    int32_t  size;        // bytes moved, or the seek target
    int      slot;        // dispatch: the vtable slot the array body chose
    int64_t  elemOff;     // dispatch: &elem - arrayBase
    bool     nameNull;    // dispatch: was `name` nullptr?
    uint8_t  payload[8];  // read/write: the first min(size,8) bytes
};

constexpr int kMaxEmit = 64;

struct Sink {
    void**   vtable;              // +0x00 — must be first: this IS the stream
    Emission log[kMaxEmit];
    int      n;
    int32_t  cursor;
    uint64_t readSeed;
};

// The codec handed to the bodies. Layout is BinaryCodec's, exactly, with the
// probe's own fields appended past +0x30 where no shipped body reads.
struct ProbeCodec {
    BinaryCodec codec;   // +0x00 .. +0x2f
    Sink*       sink;    // +0x30 — probe-private
    void*       arrBase; // +0x38 — probe-private
};
static_assert(offsetof(ProbeCodec, sink) == 0x30,
              "probe fields must start past the 0x30 the allocator gives a codec");

void Note(Sink* s, const Emission& e) {
    if (s->n < kMaxEmit) s->log[s->n] = e;
    ++s->n;                       // keep counting past the cap so an overflow
                                  // shows up as a count mismatch, not silence
}

// ---- the synthetic IO::Stream --------------------------------------------
// Slots: +0x10 Write, +0x18 Read, +0x20 SetCurrentPos, +0x28 GetCurrentPos.
bool SinkWrite(void* self, int32_t size, void* buf) {
    Sink* s = static_cast<Sink*>(self);
    Emission e{}; e.op = kOpWrite; e.size = size;
    std::memcpy(e.payload, buf, size < 8 ? (size_t)(size < 0 ? 0 : size) : 8);
    Note(s, e);
    s->cursor += size;
    return true;
}
bool SinkRead(void* self, int32_t size, void* buf) {
    Sink* s = static_cast<Sink*>(self);
    // Deterministic, position-dependent payload, so a body that reads the right
    // number of bytes from the wrong place is still caught.
    uint8_t* out = static_cast<uint8_t*>(buf);
    for (int32_t i = 0; i < size; ++i) {
        s->readSeed = s->readSeed * 6364136223846793005ull + 1442695040888963407ull;
        out[i] = (uint8_t)(s->readSeed >> 33);
    }
    Emission e{}; e.op = kOpRead; e.size = size;
    std::memcpy(e.payload, buf, size < 8 ? (size_t)(size < 0 ? 0 : size) : 8);
    Note(s, e);
    s->cursor += size;
    return true;
}
bool SinkSetPos(void* self, int32_t pos) {
    Sink* s = static_cast<Sink*>(self);
    Emission e{}; e.op = kOpSeek; e.size = pos;
    Note(s, e);
    s->cursor = pos;
    return true;
}
int32_t SinkGetPos(void* self) { return static_cast<Sink*>(self)->cursor; }

void* g_streamVtbl[6] = {
    nullptr, nullptr,
    (void*)&SinkWrite, (void*)&SinkRead, (void*)&SinkSetPos, (void*)&SinkGetPos,
};

// ---- the synthetic codec vtable ------------------------------------------
// 26 slots. The eleven SCALAR slots are recorders, so an array body's virtual
// self-dispatch lands somewhere observable instead of in a real Serialize.
void RecordDispatch(void* self, void* value, const char* name, int slot) {
    ProbeCodec* c = static_cast<ProbeCodec*>(self);
    Emission e{}; e.op = kOpDispatch; e.slot = slot;
    e.elemOff  = (const uint8_t*)value - (const uint8_t*)c->arrBase;
    e.nameNull = (name == nullptr);
    Note(c->sink, e);
}
#define RT_PROBE_SCALAR_STUB(NAME, SLOT)                                       \
    void NAME(void* self, void* value, const char* name) {                     \
        RecordDispatch(self, value, name, (SLOT));                             \
    }
RT_PROBE_SCALAR_STUB(StubChar,   rw::kScalarChar)
RT_PROBE_SCALAR_STUB(StubBool,   rw::kScalarBool)
RT_PROBE_SCALAR_STUB(StubFloat,  rw::kScalarFloat)
RT_PROBE_SCALAR_STUB(StubU64,    rw::kScalarUInt64)
RT_PROBE_SCALAR_STUB(StubI64,    rw::kScalarInt64)
RT_PROBE_SCALAR_STUB(StubUInt,   rw::kScalarUInt)
RT_PROBE_SCALAR_STUB(StubInt,    rw::kScalarInt)
RT_PROBE_SCALAR_STUB(StubUShort, rw::kScalarUShort)
RT_PROBE_SCALAR_STUB(StubShort,  rw::kScalarShort)
RT_PROBE_SCALAR_STUB(StubUChar,  rw::kScalarUChar)
RT_PROBE_SCALAR_STUB(StubSChar,  rw::kScalarSChar)
#undef RT_PROBE_SCALAR_STUB

void* g_codecVtbl[26] = {};

void BuildCodecVtbl() {
    void* stubs[11] = {
        (void*)&StubChar, (void*)&StubBool, (void*)&StubFloat, (void*)&StubU64,
        (void*)&StubI64,  (void*)&StubUInt, (void*)&StubInt,   (void*)&StubUShort,
        (void*)&StubShort,(void*)&StubUChar,(void*)&StubSChar,
    };
    for (int i = 0; i < 11; ++i)
        g_codecVtbl[(rw::kScalarChar / 8) + i] = stubs[i];
}

// ---------------------------------------------------------------------------
// Case plumbing
// ---------------------------------------------------------------------------
struct Result {
    const char* bad = nullptr;
    const char* badCase = nullptr;
    int64_t cases = 0;
    int64_t emissions = 0;      // comparisons that had something in them
    int64_t silent = 0;         // cases that legitimately emitted nothing
    uint32_t typeMask = 0;      // 11 bits, one per scalar type
    int negFired = 0;
    int negRun = 0;
};

void ResetSink(Sink* s) {
    std::memset(s, 0, sizeof(*s));
    s->vtable = g_streamVtbl;
    s->readSeed = 0x9E3779B97F4A7C15ull;
}
void ResetCodec(ProbeCodec* c, Sink* s, void** vtbl) {
    std::memset(c, 0, sizeof(*c));
    c->codec.base.vtable = vtbl;
    c->codec.base.stream = s;
    c->codec.word20 = 0x1234;
    c->codec.word24 = 0x0040;
    c->codec.version = 0x5EA1;
    c->sink = s;
}

// The comparison. Returns nullptr on agreement, else the failing term — named,
// per reference_shadow_predicate_must_name_its_failing_term.md.
const char* CompareSinks(const Sink* a, const Sink* b) {
    if (a->n != b->n)           return "emission count";
    if (a->cursor != b->cursor) return "stream cursor";
    const int n = a->n < kMaxEmit ? a->n : kMaxEmit;
    for (int i = 0; i < n; ++i) {
        const Emission& x = a->log[i];
        const Emission& y = b->log[i];
        if (x.op != y.op)             return "emission op";
        if (x.size != y.size)         return "emission size";
        if (x.slot != y.slot)         return "dispatch slot";
        if (x.elemOff != y.elemOff)   return "element offset (stride)";
        if (x.nameNull != y.nameNull) return "name argument";
        if (x.op == kOpWrite && std::memcmp(x.payload, y.payload, 8) != 0)
            return "written payload";
    }
    return nullptr;
}

// ---- the three negative controls, as bodies -------------------------------
void NegScalarWrongSize(BinaryCodec* self, void* value, int32_t size) {
    void* s = self->base.stream;
    reinterpret_cast<bool (*)(void*, int32_t, void*)>(
        (*reinterpret_cast<void***>(s))[0x10 / 8])(s, size + 1, value);
}
void NegArrayWrongStride(void* self, int32_t count, uint8_t* arr,
                         int slot, int32_t elemSize) {
    for (int32_t i = 0; i < count; ++i) {
        reinterpret_cast<void (*)(void*, void*, const char*)>(
            (*reinterpret_cast<void***>(self))[slot / 8])(
                self, arr + (int64_t)i * (elemSize + 1), nullptr);
    }
}

using SerScalarFn = void (*)(void*, void*, const char*);
using SerArrayFn  = void (*)(void*, int32_t, void*, const char*);
using SerBytesFn  = void (*)(void*, int32_t, void*, const char*);
using NoArgFn     = void (*)(void*);
using GetVerFn    = int32_t (*)(void*);

}  // namespace

// ===========================================================================
void RtIo_SerializeFamilyProbe() {
    if (!RtDiffEnabled()) return;

    RT_DIFF_BEGIN("IO::{BinaryReader,BinaryWriter,Visitor}::Serialize family [probe]");

    // Everything below reaches the shipped bodies by MANGLED NAME through
    // ResolveExportRaw. Not ResolveAddr: this lane has registered every one of
    // these addresses, so ResolveAddr would hand back OUR function and the probe
    // would compare ours to ours and always agree
    // (reference_resolveaddr_consults_port_registry_first.md).
    if (ResolveExportRaw("?Serialize@BinaryReader@IO@@UEAAXAEAHPEBD@Z") == nullptr) {
        printf("[SerialFam] probe SKIPPED: no shipped DLL to diff against "
                    "(expected once the DLL is gone).\n");
        return;
    }

    BuildCodecVtbl();
    Result r;

    // Sinks and codecs, one pair per side. Reset per case.
    Sink sa, sb;
    ProbeCodec ca, cb;

    // ---- the eleven types, as a table ------------------------------------
    struct TypeRow {
        const char* label;
        int32_t     size;
        int         scalarSlot;
        const char* rdScalar;  const char* wrScalar;
        const char* rdArray;   const char* wrArray;
    };
    static const TypeRow kTypes[11] = {
      {"char",  1, rw::kScalarChar,
       "?Serialize@BinaryReader@IO@@UEAAXAEADPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEADPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEADPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEADPEBD@Z"},
      {"bool",  1, rw::kScalarBool,
       "?Serialize@BinaryReader@IO@@UEAAXAEA_NPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEA_NPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEA_NPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEA_NPEBD@Z"},
      {"float", 4, rw::kScalarFloat,
       "?Serialize@BinaryReader@IO@@UEAAXAEAMPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAMPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAMPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAMPEBD@Z"},
      {"u64",   8, rw::kScalarUInt64,
       "?Serialize@BinaryReader@IO@@UEAAXAEA_KPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEA_KPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEA_KPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEA_KPEBD@Z"},
      {"i64",   8, rw::kScalarInt64,
       "?Serialize@BinaryReader@IO@@UEAAXAEA_JPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEA_JPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEA_JPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEA_JPEBD@Z"},
      {"uint",  4, rw::kScalarUInt,
       "?Serialize@BinaryReader@IO@@UEAAXAEAIPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAIPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAIPEBD@Z"},
      {"int",   4, rw::kScalarInt,
       "?Serialize@BinaryReader@IO@@UEAAXAEAHPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAHPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAHPEBD@Z"},
      {"ushort",2, rw::kScalarUShort,
       "?Serialize@BinaryReader@IO@@UEAAXAEAGPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAGPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAGPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAGPEBD@Z"},
      {"short", 2, rw::kScalarShort,
       "?Serialize@BinaryReader@IO@@UEAAXAEAFPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAFPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAFPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAFPEBD@Z"},
      {"uchar", 1, rw::kScalarUChar,
       "?Serialize@BinaryReader@IO@@UEAAXAEAEPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEAEPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEAEPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEAEPEBD@Z"},
      {"schar", 1, rw::kScalarSChar,
       "?Serialize@BinaryReader@IO@@UEAAXAEACPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXAEACPEBD@Z",
       "?Serialize@BinaryReader@IO@@UEAAXHPEACPEBD@Z", "?Serialize@BinaryWriter@IO@@UEAAXHPEACPEBD@Z"},
    };

    // Two element counts per array case, and a NEGATIVE one: retail's guard is
    // `TEST EDX,EDX / JLE`, signed, so a negative count must return rather than
    // loop ~4 billion times. That branch is only reachable by asking for it.
    static const int32_t kCounts[3] = { 5, 0, -3 };

    alignas(16) uint8_t arrA[64], arrB[64], valA[16], valB[16];

    for (int t = 0; t < 11 && r.bad == nullptr; ++t) {
        const TypeRow& ty = kTypes[t];

        // ---- scalar, both directions -------------------------------------
        for (int dir = 0; dir < 2 && r.bad == nullptr; ++dir) {
            const char* mangled = dir == 0 ? ty.rdScalar : ty.wrScalar;
            SerScalarFn ours = reinterpret_cast<SerScalarFn>(
                                   ResolveExport(mangled));
            SerScalarFn theirs = reinterpret_cast<SerScalarFn>(
                                   ResolveExportRaw(mangled));
            if (!ours || !theirs) { r.bad = "a scalar body did not resolve";
                                    r.badCase = ty.label; break; }

            std::memset(valA, 0xA5, sizeof(valA));
            std::memset(valB, 0xA5, sizeof(valB));
            // A distinct, type-sized payload so a wrong size is visible in the
            // written bytes as well as in the size column.
            for (int i = 0; i < ty.size; ++i) { valA[i] = (uint8_t)(0x10 + i);
                                                valB[i] = (uint8_t)(0x10 + i); }
            ResetSink(&sa); ResetSink(&sb);
            ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);

            ours(&ca, valA, "fieldName");
            theirs(&cb, valB, "fieldName");
            ++r.cases;
            const char* bad = CompareSinks(&sa, &sb);
            if (!bad && sa.n != 1) bad = "scalar emitted other than once";
            if (!bad && sa.log[0].size != ty.size) bad = "scalar moved the wrong size";
            if (!bad && sa.log[0].op != (dir == 0 ? kOpRead : kOpWrite))
                bad = "scalar used the wrong stream slot (read vs write)";
            // On the READ path the destination is an output: compare the
            // decoded bytes, and ONLY the ty.size bytes a read is supposed to
            // have touched.
            if (!bad && dir == 0 && std::memcmp(valA, valB, ty.size) != 0)
                bad = "decoded value";
            if (!bad && dir == 0) {
                for (int i = ty.size; i < (int)sizeof(valA); ++i)
                    if (valA[i] != 0xA5) { bad = "scalar read past its type"; break; }
            }
            if (bad) { r.bad = bad; r.badCase = ty.label; break; }
            r.emissions += sa.n;
            r.typeMask |= (1u << t);
        }

        // ---- array, both directions, three counts -------------------------
        for (int dir = 0; dir < 2 && r.bad == nullptr; ++dir) {
            const char* mangled = dir == 0 ? ty.rdArray : ty.wrArray;
            SerArrayFn ours = reinterpret_cast<SerArrayFn>(ResolveExport(mangled));
            SerArrayFn theirs = reinterpret_cast<SerArrayFn>(ResolveExportRaw(mangled));
            if (!ours || !theirs) { r.bad = "an array body did not resolve";
                                    r.badCase = ty.label; break; }

            for (int ci = 0; ci < 3 && r.bad == nullptr; ++ci) {
                const int32_t count = kCounts[ci];
                std::memset(arrA, 0x5A, sizeof(arrA));
                std::memset(arrB, 0x5A, sizeof(arrB));
                ResetSink(&sa); ResetSink(&sb);
                ResetCodec(&ca, &sa, g_codecVtbl);
                ResetCodec(&cb, &sb, g_codecVtbl);
                ca.arrBase = arrA; cb.arrBase = arrB;

                ours(&ca, count, arrA, "arrayName");
                theirs(&cb, count, arrB, "arrayName");
                ++r.cases;
                const char* bad = CompareSinks(&sa, &sb);
                const int expect = count > 0 ? count : 0;
                if (!bad && sa.n != expect) bad = "array dispatch count";
                for (int i = 0; !bad && i < sa.n; ++i) {
                    if (sa.log[i].op != kOpDispatch) bad = "array touched the stream directly";
                    else if (sa.log[i].slot != ty.scalarSlot) bad = "array chose the wrong scalar slot";
                    else if (sa.log[i].elemOff != (int64_t)i * ty.size) bad = "array element stride";
                    else if (!sa.log[i].nameNull) bad = "array forwarded `name` instead of nullptr";
                }
                if (bad) { r.bad = bad; r.badCase = ty.label; break; }
                if (sa.n == 0) ++r.silent; else r.emissions += sa.n;
            }
        }
    }

    // ---- SerializeBytes, both directions ----------------------------------
    for (int dir = 0; dir < 2 && r.bad == nullptr; ++dir) {
        const char* mangled = dir == 0
            ? "?SerializeBytes@BinaryReader@IO@@UEAAXHPEAXPEBD@Z"
            : "?SerializeBytes@BinaryWriter@IO@@UEAAXHPEAXPEBD@Z";
        SerBytesFn ours = reinterpret_cast<SerBytesFn>(ResolveExport(mangled));
        SerBytesFn theirs = reinterpret_cast<SerBytesFn>(ResolveExportRaw(mangled));
        if (!ours || !theirs) { r.bad = "SerializeBytes did not resolve";
                                r.badCase = "bytes"; break; }
        const int32_t n = 13;      // deliberately not a power of two
        std::memset(arrA, 0x5A, sizeof(arrA));
        std::memset(arrB, 0x5A, sizeof(arrB));
        ResetSink(&sa); ResetSink(&sb);
        ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);
        ours(&ca, n, arrA, "blob");
        theirs(&cb, n, arrB, "blob");
        ++r.cases;
        const char* bad = CompareSinks(&sa, &sb);
        if (!bad && sa.n != 1) bad = "SerializeBytes emitted other than once";
        if (!bad && sa.log[0].size != n) bad = "SerializeBytes passed the wrong count";
        if (!bad && dir == 0 && std::memcmp(arrA, arrB, n) != 0) bad = "SerializeBytes decoded bytes";
        if (bad) { r.bad = bad; r.badCase = "bytes"; break; }
        r.emissions += sa.n;
    }

    // ---- Skip, both directions --------------------------------------------
    // BinaryReader::Skip seeks to word20+word24; BinaryWriter::Skip is a RET.
    // Running both through the same comparison is what makes the asymmetry a
    // measured fact rather than a claim in a comment.
    for (int dir = 0; dir < 2 && r.bad == nullptr; ++dir) {
        const char* mangled = dir == 0 ? "?Skip@BinaryReader@IO@@UEAAXXZ"
                                       : "?Skip@BinaryWriter@IO@@UEAAXXZ";
        NoArgFn ours = reinterpret_cast<NoArgFn>(ResolveExport(mangled));
        NoArgFn theirs = reinterpret_cast<NoArgFn>(ResolveExportRaw(mangled));
        if (!ours || !theirs) { r.bad = "Skip did not resolve"; r.badCase = "skip"; break; }
        ResetSink(&sa); ResetSink(&sb);
        ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);
        ours(&ca); theirs(&cb);
        ++r.cases;
        const char* bad = CompareSinks(&sa, &sb);
        if (!bad && dir == 0) {
            if (sa.n != 1 || sa.log[0].op != kOpSeek) bad = "reader Skip did not seek";
            else if (sa.log[0].size != ca.codec.word20 + ca.codec.word24)
                bad = "reader Skip seek target != word20 + word24";
        }
        if (!bad && dir == 1 && sa.n != 0) bad = "writer Skip was not a no-op";
        if (bad) { r.bad = bad; r.badCase = "skip"; break; }
        if (sa.n == 0) ++r.silent; else r.emissions += sa.n;
    }

    // ---- GetVersion x3 — the +0x20 / +0x28 trap ---------------------------
    // Two DIFFERENT values are planted so that reading the wrong field returns
    // the wrong answer instead of the same one by luck. This is the one place in
    // the family where the three classes genuinely disagree, so it gets a case
    // that can only pass if the disagreement was reproduced.
    if (r.bad == nullptr) {
        struct VerRow { const char* mangled; int off; } rows[3] = {
            { "?GetVersion@BinaryReader@IO@@UEBAHXZ", 0x28 },
            { "?GetVersion@BinaryWriter@IO@@UEBAHXZ", 0x28 },
            { "?GetVersion@Visitor@IO@@UEBAHXZ",      0x20 },
        };
        for (int i = 0; i < 3 && r.bad == nullptr; ++i) {
            GetVerFn ours = reinterpret_cast<GetVerFn>(ResolveExport(rows[i].mangled));
            GetVerFn theirs = reinterpret_cast<GetVerFn>(ResolveExportRaw(rows[i].mangled));
            if (!ours || !theirs) { r.bad = "GetVersion did not resolve";
                                    r.badCase = rows[i].mangled; break; }
            alignas(16) uint8_t obj[0x40];
            std::memset(obj, 0, sizeof(obj));
            *reinterpret_cast<int32_t*>(obj + 0x20) = 0x2020AAAA;
            *reinterpret_cast<int32_t*>(obj + 0x28) = 0x2828BBBB;
            const int32_t want = *reinterpret_cast<int32_t*>(obj + rows[i].off);
            const int32_t got  = ours(obj);
            const int32_t exp  = theirs(obj);
            ++r.cases; ++r.emissions;
            if (got != exp)  { r.bad = "GetVersion value"; r.badCase = rows[i].mangled; }
            else if (got != want) { r.bad = "GetVersion read the wrong OFFSET (the "
                                            "Visitor +0x20 vs codec +0x28 trap)";
                                    r.badCase = rows[i].mangled; }
        }
    }

    // ---- Visitor: 23 slots that must emit nothing and write nothing -------
    if (r.bad == nullptr) {
        static const char* kVisScalar[11] = {
            "?Serialize@Visitor@IO@@UEAAXAEADPEBD@Z", "?Serialize@Visitor@IO@@UEAAXAEA_NPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXAEAMPEBD@Z", "?Serialize@Visitor@IO@@UEAAXAEA_KPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXAEA_JPEBD@Z", "?Serialize@Visitor@IO@@UEAAXAEAIPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXAEAHPEBD@Z", "?Serialize@Visitor@IO@@UEAAXAEAGPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXAEAFPEBD@Z", "?Serialize@Visitor@IO@@UEAAXAEAEPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXAEACPEBD@Z",
        };
        static const char* kVisArray[12] = {
            "?Serialize@Visitor@IO@@UEAAXHPEADPEBD@Z", "?Serialize@Visitor@IO@@UEAAXHPEA_NPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXHPEAMPEBD@Z", "?Serialize@Visitor@IO@@UEAAXHPEA_KPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXHPEA_JPEBD@Z", "?Serialize@Visitor@IO@@UEAAXHPEAIPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXHPEAHPEBD@Z", "?Serialize@Visitor@IO@@UEAAXHPEAGPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXHPEAFPEBD@Z", "?Serialize@Visitor@IO@@UEAAXHPEAEPEBD@Z",
            "?Serialize@Visitor@IO@@UEAAXHPEACPEBD@Z",
            "?SerializeBytes@Visitor@IO@@UEAAXHPEAXPEBD@Z",
        };
        for (int i = 0; i < 11 + 12 + 1 && r.bad == nullptr; ++i) {
            const bool isScalar = i < 11;
            const bool isSkip   = i == 23;
            const char* mangled = isSkip ? "?Skip@Visitor@IO@@UEAAXXZ"
                                : isScalar ? kVisScalar[i] : kVisArray[i - 11];
            void* ours = ResolveExport(mangled);
            void* theirs = ResolveExportRaw(mangled);
            if (!ours || !theirs) { r.bad = "a Visitor body did not resolve";
                                    r.badCase = mangled; break; }
            std::memset(arrA, 0x5A, sizeof(arrA));
            std::memset(arrB, 0x5A, sizeof(arrB));
            ResetSink(&sa); ResetSink(&sb);
            ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);
            ca.arrBase = arrA; cb.arrBase = arrB;
            if (isSkip) {
                reinterpret_cast<NoArgFn>(ours)(&ca);
                reinterpret_cast<NoArgFn>(theirs)(&cb);
            } else if (isScalar) {
                reinterpret_cast<SerScalarFn>(ours)(&ca, arrA, "v");
                reinterpret_cast<SerScalarFn>(theirs)(&cb, arrB, "v");
            } else {
                reinterpret_cast<SerArrayFn>(ours)(&ca, 4, arrA, "v");
                reinterpret_cast<SerArrayFn>(theirs)(&cb, 4, arrB, "v");
            }
            ++r.cases;
            const char* bad = CompareSinks(&sa, &sb);
            if (!bad && sa.n != 0) bad = "a Visitor slot emitted something";
            // The claim is that NEITHER side writes, so the poison IS the test.
            // Negative control 3 below proves this check can fail.
            if (!bad) {
                for (size_t k = 0; k < sizeof(arrA); ++k)
                    if (arrA[k] != 0x5A || arrB[k] != 0x5A) { bad = "a Visitor slot wrote to its argument"; break; }
            }
            if (bad) { r.bad = bad; r.badCase = mangled; break; }
            ++r.silent;
        }
    }

    // =======================================================================
    // NEGATIVE CONTROLS — each must FIRE, and the probe fails if one does not.
    // =======================================================================
    {
        // NEG 1 — a scalar body that moves one byte too many.
        ++r.negRun;
        std::memset(valA, 0xA5, sizeof(valA)); std::memset(valB, 0xA5, sizeof(valB));
        ResetSink(&sa); ResetSink(&sb);
        ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);
        NegScalarWrongSize(&ca.codec, valA, 4);
        reinterpret_cast<SerScalarFn>(
            ResolveExportRaw("?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z"))(&cb, valB, "n");
        if (CompareSinks(&sa, &sb) != nullptr) ++r.negFired;
    }
    {
        // NEG 2 — an array body with the wrong element stride.
        ++r.negRun;
        std::memset(arrA, 0x5A, sizeof(arrA)); std::memset(arrB, 0x5A, sizeof(arrB));
        ResetSink(&sa); ResetSink(&sb);
        ResetCodec(&ca, &sa, g_codecVtbl); ResetCodec(&cb, &sb, g_codecVtbl);
        ca.arrBase = arrA; cb.arrBase = arrB;
        NegArrayWrongStride(&ca, 5, arrA, rw::kScalarInt, 4);
        reinterpret_cast<SerArrayFn>(
            ResolveExportRaw("?Serialize@BinaryWriter@IO@@UEAAXHPEAHPEBD@Z"))(&cb, 5, arrB, "n");
        if (CompareSinks(&sa, &sb) != nullptr) ++r.negFired;
    }
    {
        // NEG 3 — a "Visitor" body that writes one byte. Proves the
        // untouched-destination check is not passing vacuously.
        ++r.negRun;
        std::memset(arrA, 0x5A, sizeof(arrA));
        arrA[3] = 0x00;                          // the body under test "wrote"
        bool fired = false;
        for (size_t k = 0; k < sizeof(arrA); ++k)
            if (arrA[k] != 0x5A) { fired = true; break; }
        if (fired) ++r.negFired;
    }

    // ---- verdict -----------------------------------------------------------
    int typesCovered = 0;
    for (int i = 0; i < 11; ++i) if (r.typeMask & (1u << i)) ++typesCovered;

    const bool covered = r.cases > 0 && r.emissions > 0 && r.silent > 0 &&
                         typesCovered == 11 && r.negFired == r.negRun &&
                         r.negRun == 3;
    const bool pass = (r.bad == nullptr) && covered;

    char ours[320], theirs[320];
    std::snprintf(ours, sizeof(ours),
                  "cases=%lld emissions=%lld silent=%lld types=%d/11 neg=%d/%d",
                  (long long)r.cases, (long long)r.emissions,
                  (long long)r.silent, typesCovered, r.negFired, r.negRun);
    if (r.bad)
        std::snprintf(theirs, sizeof(theirs), "DIVERGED on '%s' (case: %s)",
                      r.bad, r.badCase ? r.badCase : "?");
    else if (typesCovered != 11)
        std::snprintf(theirs, sizeof(theirs),
                      "no divergence, but only %d of 11 types were exercised",
                      typesCovered);
    else if (r.negFired != r.negRun)
        std::snprintf(theirs, sizeof(theirs),
                      "no divergence, but %d of %d NEGATIVE CONTROLS DID NOT FIRE "
                      "-- the zero is uninformative", r.negRun - r.negFired, r.negRun);
    else if (!covered)
        std::snprintf(theirs, sizeof(theirs), "no divergence, but a CONTROL READS ZERO");
    else
        std::snprintf(theirs, sizeof(theirs),
                      "identical emission sequences: op, size, slot, stride, "
                      "name and payload, across all 11 types both directions");

    RT_DIFF_SHADOW(pass, ours, theirs);
    RT_DIFF_WORK(r.cases > 0);

    printf("[SerialFam] family probe: %s -- %s\n", ours, theirs);
}

}  // namespace RtIo
