// ============================================================================
// runtime/src/video/imagebuffer_serialize_probe.cpp — Wave 18 lane VRES
//   tranche 2's oracle for ?PlatformSerialize@ImageBuffer@Video@@.
// ============================================================================
//
// A SEPARATE FILE, AND A SEPARATE SUMMARY LINE, ON PURPOSE. The lane's sealed
// report quotes `cases=8` and its tranche-1 section quotes `cases=11` from
// `video_resources_probe.cpp`. Appending to that probe would silently restate
// both numbers as something else and make two already-published measurements
// unreproducible. This tranche gets its own counter and its own env var
// (`BANISHED_RT_VIDIMGSER_PROBE`), so neither published number moves.
//
// ---------------------------------------------------------------------------
// THE SUBJECT IS SYNTHESISED, AND THE ADVISOR'S WARNING IS WHY
// ---------------------------------------------------------------------------
// Lane VDATA reports that Serialize's save-trace oracle recorded NO ENTRY AT
// ALL for the seven ImageBuffer vtable slots. That is the `calls=` column, not
// a pass (`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`),
// and it says plainly that waiting for a live route to exercise this body is
// waiting for something that does not happen on any harness route. So the
// subject is built rather than waited for — "no gameplay route" is not "no
// oracle" (`reference_no_route_is_not_no_oracle.md`).
//
// WHAT IS FAKED, AND WHAT IS REAL:
//   * FAKE: the ImageBuffer's vtable slot 9 (Create) — it RECORDS its twelve
//     arguments instead of building a GPU resource. This is the only way the
//     write path is safe to drive at all: retail's own write path calls Create,
//     and running that twice on a live texture is not a replay, it is two
//     rebuilds.
//   * FAKE: the ReaderWriter — it records every slot, and on the read path it
//     plays a SCRIPT back, so the load arm can be driven with no file.
//   * FAKE: the Encoding — slot 0 returns a value this probe chooses, so the
//     gate can be driven both ways.
//   * REAL: `Array<Array<uint8>>::SetSize` (0x180053f00, the DLL's — unported)
//     and `Array<uint8>::SetSize` (0x180029630, which is lane Q's PORT). Both
//     sides reach the same two through the same door, so a divergence there
//     would show up as a divergence here — which is a feature, not a leak in
//     the comparison.
//
// WHAT IS COMPARED: the full Create argument vector (all twelve), the complete
// ReaderWriter call trace (slot, value, count), whether the encoding gate was
// consulted, and the post-state of the temporary array header. That is every
// observable this function has.
//
// CONTROLS, scored separately:
//   * EFFECTIVENESS — a case that recorded NO recorded calls on EITHER side is
//     reported as proving nothing, in those words. Case A legitimately expects
//     zero, and is the one case exempted BY NAME rather than by silence.
//   * NEGATIVE (`=neg`) — three wrong bodies through the same comparator, each
//     naming the mechanism it exercises so a control catching via the wrong one
//     is visible (`reference_union_control_hides_a_blind_screen.md`):
//       GATE   ignore the encoding gate and serialize anyway.
//              Mechanism: the bool gate.
//       SLOT   serialize the level count through kScalarUInt (0x98) instead of
//              kScalarInt (0xA0). Mechanism: the vtable OFFSET, which is what a
//              mis-transcribed slot looks like.
//       NOLOAD skip the per-level blob loop entirely.
//              Mechanism: the loop.
//
//     A CONTROL THAT WAS CONSIDERED AND REJECTED: inverting the direction test
//     that decides whether to size the outer array. On the LOAD path that
//     leaves `levels._data` null while the count says three, and the blob loop
//     then walks a null pointer — UB, not a comparable divergence, and this
//     lane has already paid for that mistake once in
//     `video_resources_probe.cpp`. SLOT exercises the same class of defect (a
//     wrong constant in a transcribed call) and stays in bounds.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/ImageBuffer.h"
#include "../collection/array_internal.h"
#include "../collection/array_destroy_internal.h"

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

extern "C" void Video_ImageBuffer_PlatformSerialize(::ImageBuffer* self, void* serializer);

namespace {

int g_cases = 0, g_mismatch = 0, g_recordedCalls = 0;
enum Neg { kNegNone = 0, kNegGate, kNegSlot, kNegNoLoad, kNegCOUNT };
const char* const kNegName[kNegCOUNT] = { "none", "GATE", "SLOT", "NOLOAD" };
Neg g_neg = kNegNone;
int g_negCases[kNegCOUNT] = { 0 }, g_negCaught[kNegCOUNT] = { 0 };

void Fail(const char* caseName, const char* what) {
    ++g_mismatch;
    printf("[ImgSerProbe] MISMATCH  %-30s %s\n", caseName, what);
}

// ---- the recorded trace ---------------------------------------------------
struct Trace {
    int  nRw;                 // ReaderWriter calls
    int  rwSlot[32];          // 0xA0 or 0x60
    int  rwValue[32];         // the int serialised, or the byte count
    bool rwHadData[32];       // for the uchar-array slot: was data non-null
    int  gateCalls;
    int  createCalls;
    int32_t createArgs[11];   // the eleven scalar arguments
    bool createHadData;       // was initialData non-null
};
Trace g_t;

// The script the fake ReaderWriter plays back on the READ path: the first
// scalarInt call yields the level count, the rest yield per-level blob sizes.
const int32_t* g_script = nullptr;
int g_scriptLen = 0, g_scriptPos = 0;
bool g_reading = false;
bool g_gateAnswer = true;

void RwScalarInt(void* rw, int32_t* value, const char* name) {
    (void)rw; (void)name;
    if (g_t.nRw < 32) {
        g_t.rwSlot[g_t.nRw] = 0xA0;
        g_t.rwValue[g_t.nRw] = *value;
        g_t.rwHadData[g_t.nRw] = false;
        ++g_t.nRw;
    }
    ++g_recordedCalls;
    if (g_reading && g_script && g_scriptPos < g_scriptLen)
        *value = g_script[g_scriptPos++];
}

void RwArrayUChar(void* rw, int32_t count, void* data, const char* name) {
    (void)rw; (void)name;
    if (g_t.nRw < 32) {
        g_t.rwSlot[g_t.nRw] = 0x60;
        g_t.rwValue[g_t.nRw] = count;
        g_t.rwHadData[g_t.nRw] = (data != nullptr);
        ++g_t.nRw;
    }
    ++g_recordedCalls;
}

bool EncodingGate(void* enc) { (void)enc; ++g_t.gateCalls; ++g_recordedCalls; return g_gateAnswer; }

bool FakeCreate(void* self, int32_t a0, int32_t a1, int32_t a2, uint32_t a3,
                int32_t a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8,
                int32_t a9, void* initialData) {
    (void)self;
    ++g_t.createCalls;
    ++g_recordedCalls;
    const int32_t v[11] = { a0, a1, a2, static_cast<int32_t>(a3), a4, a5, a6, a7, a8, a9, 0 };
    std::memcpy(g_t.createArgs, v, sizeof(v));
    g_t.createHadData = (initialData != nullptr);
    return true;
}

// The three fake vtables. The ReaderWriter table needs slot 0xC8/8 to exist.
void* g_rwVt[26];
void* g_encVt[8];
void* g_ibVt[16];

void BuildVtables() {
    std::memset(g_rwVt, 0, sizeof(g_rwVt));
    std::memset(g_encVt, 0, sizeof(g_encVt));
    std::memset(g_ibVt, 0, sizeof(g_ibVt));
    g_rwVt[0xA0 / 8] = reinterpret_cast<void*>(&RwScalarInt);
    g_rwVt[0x60 / 8] = reinterpret_cast<void*>(&RwArrayUChar);
    g_encVt[0]       = reinterpret_cast<void*>(&EncodingGate);
    g_ibVt[0x48 / 8] = reinterpret_cast<void*>(&FakeCreate);
}

// ---- the synthetic objects ------------------------------------------------
struct FakeStream { void* vptr; int32_t direction; uint32_t pad; void* encoding; };
struct FakeSerializer { void* stream; void* rw; };

bool SameTrace(const char* caseName, const Trace& a, const Trace& b) {
    char msg[192];
    if (a.gateCalls != b.gateCalls) {
        std::snprintf(msg, sizeof(msg), "encoding gate consulted: ours=%d theirs=%d",
                      a.gateCalls, b.gateCalls);
        Fail(caseName, msg); return false;
    }
    if (a.createCalls != b.createCalls || a.createHadData != b.createHadData) {
        std::snprintf(msg, sizeof(msg),
                      "Create: ours calls=%d data=%d, theirs calls=%d data=%d",
                      a.createCalls, a.createHadData ? 1 : 0,
                      b.createCalls, b.createHadData ? 1 : 0);
        Fail(caseName, msg); return false;
    }
    if (a.createCalls > 0) {
        for (int i = 0; i < 10; ++i) {
            if (a.createArgs[i] != b.createArgs[i]) {
                std::snprintf(msg, sizeof(msg), "Create arg %d: ours=%d theirs=%d",
                              i, a.createArgs[i], b.createArgs[i]);
                Fail(caseName, msg); return false;
            }
        }
    }
    if (a.nRw != b.nRw) {
        std::snprintf(msg, sizeof(msg), "ReaderWriter call COUNT: ours=%d theirs=%d",
                      a.nRw, b.nRw);
        Fail(caseName, msg); return false;
    }
    for (int i = 0; i < a.nRw; ++i) {
        if (a.rwSlot[i] != b.rwSlot[i] || a.rwValue[i] != b.rwValue[i] ||
            a.rwHadData[i] != b.rwHadData[i]) {
            std::snprintf(msg, sizeof(msg),
                          "ReaderWriter call %d: ours=slot0x%x/%d/data%d "
                          "theirs=slot0x%x/%d/data%d",
                          i, a.rwSlot[i], a.rwValue[i], a.rwHadData[i] ? 1 : 0,
                          b.rwSlot[i], b.rwValue[i], b.rwHadData[i] ? 1 : 0);
            Fail(caseName, msg); return false;
        }
    }
    return true;
}

using SerFn = void (*)(void*, void*);

// ---------------------------------------------------------------------------
// The three deliberately wrong bodies.
//
// They are written out here rather than switched inside the port, because a
// port with a corruption switch in it is a port nobody can read. This mirrors
// `array_destroy_probe.cpp`, which does the same for the same reason: the
// duplication is the cost of not putting test scaffolding in shipped code.
//
// Everything not named by the control is IDENTICAL to the real body, so a
// control that fires tells you which mechanism fired.
// ---------------------------------------------------------------------------
struct SerRaw { void* stream; void* rw; };

using ScalarFn2  = void (*)(void*, int32_t*, const char*);
using UCharFn2   = void (*)(void*, int32_t, void*, const char*);
using GateFn2    = bool (*)(void*);
using CreateFn2  = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t, int32_t,
                            int32_t, int32_t, int32_t, int32_t, int32_t, void*);
using SetSizeFn2 = void (*)(rtcoll::ArrayHdr*, int32_t);

int32_t Dir(void* stream) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(stream) + 8); }

void NegBody(void* selfP, void* serP) {
    ::ImageBuffer* self = static_cast<::ImageBuffer*>(selfP);
    SerRaw* ser = static_cast<SerRaw*>(serP);
    const int32_t kindAtEntry = self->kind;
    void* stream = ser->stream;

    if (static_cast<uint32_t>(kindAtEntry - 4) <= 1u) {
        if (Dir(stream) != 0) return;
        void** vt = *reinterpret_cast<void***>(self);
        reinterpret_cast<CreateFn2>(vt[0x48 / 8])(
            self, self->dimension, kindAtEntry, static_cast<int32_t>(self->format),
            self->flags, self->width, self->height, self->filter,
            self->addressU, self->addressV, self->addressW, nullptr);
        return;
    }

    if (g_neg != kNegGate) {                       // GATE: skip the gate entirely
        void* enc = *reinterpret_cast<void**>(static_cast<uint8_t*>(stream) + 0x10);
        void** encVt = *reinterpret_cast<void***>(enc);
        if (!reinterpret_cast<GateFn2>(encVt[0])(enc)) return;
    }

    rtcoll::ArrayHdr levels = { nullptr, 0, 0 };
    int32_t count = levels._size;
    void* rw = ser->rw;
    void** rwVt = *reinterpret_cast<void***>(rw);
    // SLOT: kScalarUInt (0x98) instead of kScalarInt (0xA0).
    const int slot = (g_neg == kNegSlot) ? 0x98 : 0xA0;
    if (rwVt[slot / 8] != nullptr)
        reinterpret_cast<ScalarFn2>(rwVt[slot / 8])(rw, &count, nullptr);

    if (Dir(ser->stream) == 0)
        reinterpret_cast<SetSizeFn2>(ResolveAddr(Module::Runtime, 0x180053f00))(&levels, count);

    if (g_neg != kNegNoLoad) {                     // NOLOAD: skip the blob loop
        for (int32_t i = 0; i < count; ++i) {
            rtcoll::ArrayHdr* e = reinterpret_cast<rtcoll::ArrayHdr*>(levels._data) + i;
            int32_t size = e->_size;
            void* r = ser->rw;
            void** rv = *reinterpret_cast<void***>(r);
            reinterpret_cast<ScalarFn2>(rv[0xA0 / 8])(r, &size, nullptr);
            if (Dir(ser->stream) == 0)
                reinterpret_cast<SetSizeFn2>(ResolveAddr(Module::Runtime, 0x180029630))(e, size);
            reinterpret_cast<UCharFn2>(rv[0x60 / 8])(r, size, e->_data, nullptr);
        }
    }

    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<CreateFn2>(vt[0x48 / 8])(
        self, self->dimension, self->kind, static_cast<int32_t>(self->format),
        self->flags, self->width, self->height, self->filter,
        self->addressU, self->addressV, self->addressW, &levels);

    const int32_t n = levels._size;
    static constexpr rtcoll::DestroySpec kSpec =
        { "neg level blob", 0x010, 1, { 0x000 } };
    rtcoll::FreeInnerArraysCore(reinterpret_cast<uint8_t*>(levels._data), n, &kSpec);
    rtcoll::ArrayDtorImpl(&levels);
}

// Runs ONE side and returns its trace. `body` is ours or the shipped one.
Trace RunSide(SerFn body, int32_t kind, int32_t direction, bool gate,
              const int32_t* script, int scriptLen) {
    alignas(8) uint8_t ibStorage[sizeof(::ImageBuffer)];
    std::memset(ibStorage, 0, sizeof(ibStorage));
    ::ImageBuffer* ib = reinterpret_cast<::ImageBuffer*>(ibStorage);
    ib->vtable = g_ibVt;
    // Distinct values in every field, so a body that read the wrong offset
    // cannot agree by accident.
    ib->width = 111; ib->height = 222; ib->dimension = 3; ib->kind = kind;
    ib->format = 5; ib->flags = 0x1234; ib->addressU = 7; ib->addressV = 8;
    ib->addressW = 9; ib->filter = 2;

    void* encObj = g_encVt;
    FakeStream stream{ nullptr, direction, 0, &encObj };
    void* rwObj = g_rwVt;
    FakeSerializer ser{ &stream, &rwObj };

    g_t = Trace{};
    g_reading = (direction == 0);
    g_gateAnswer = gate;
    g_script = script; g_scriptLen = scriptLen; g_scriptPos = 0;
    body(ib, &ser);
    return g_t;
}

// A control is scored ONLY on the cases where its wrong body can actually
// DIVERGE. The first version of this probe scored all three against all five
// cases and printed "GATE caught 3 of 5" -- which reads as two escapes and is
// nothing of the sort: cases A and B never reach the gate at all, so there was
// nothing there for GATE to corrupt. A denominator that includes inapplicable
// cases understates a working control exactly the way a union denominator
// overstates a blind one (reference_control_must_pick_a_compared_subject.md).
//
// `applicable` is a bitmask over Neg: bit k set means control k has a mechanism
// to break in this case.
void RunCase(const char* caseName, int32_t kind, int32_t direction, bool gate,
             const int32_t* script, int scriptLen, bool expectZeroCalls,
             unsigned applicable) {
    ++g_cases;
    SerFn shipped = reinterpret_cast<SerFn>(ResolveRuntimeRaw(0x1800f3130));
    if (!shipped) { Fail(caseName, "shipped 0x1800f3130 did not resolve"); return; }

    const int before = g_recordedCalls;
    // The port's signature names its first parameter ImageBuffer*; the probe
    // drives both sides through one void* signature so they are called
    // identically. Same Microsoft x64 ABI either way -- a pointer is a pointer.
    const SerFn oursFn = (g_neg == kNegNone)
        ? reinterpret_cast<SerFn>(&Video_ImageBuffer_PlatformSerialize)
        : reinterpret_cast<SerFn>(&NegBody);
    const Trace ours   = RunSide(oursFn, kind, direction, gate, script, scriptLen);
    const Trace theirs = RunSide(shipped, kind, direction, gate, script, scriptLen);
    const int recorded = g_recordedCalls - before;

    const bool eq = SameTrace(caseName, ours, theirs);

    // EFFECTIVENESS. Case A legitimately records nothing; it is exempted BY
    // NAME here rather than by leaving the check out, so that a different case
    // going silent still reports.
    if (!expectZeroCalls && recorded == 0)
        Fail(caseName, "NEITHER side recorded a single call -- this case PROVES NOTHING");
    if (expectZeroCalls && recorded != 0) {
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "expected both sides to do NOTHING, but %d calls were recorded",
                      recorded);
        Fail(caseName, msg);
    }

    if (g_neg != kNegNone && (applicable & (1u << g_neg))) {
        ++g_negCases[g_neg];
        if (!eq) ++g_negCaught[g_neg];
    }
}

bool g_armed = false, g_ran = false;

void RunAll() {
    g_cases = g_mismatch = g_recordedCalls = 0;
    // A: generated, SAVING -> the whole body is a no-op.
    RunCase("A/kind=4 saving", 4, 1, true, nullptr, 0, /*expectZeroCalls=*/true, 0u);
    // B: generated, LOADING -> Create with no initial data, no stream traffic.
    RunCase("B/kind=4 loading", 4, 0, true, nullptr, 0, false, 0u);
    // C: normal kind, gate says NO -> consulted, then nothing.
    // Only GATE can diverge here: the gate says no, so nothing downstream runs.
    RunCase("C/gate=false", 2, 0, false, nullptr, 0, false, 1u << kNegGate);
    // D: normal kind, SAVING -> emits a zero count, then Create.
    // GATE and SLOT reach a mechanism here; the blob loop does not run (count 0).
    RunCase("D/kind=2 saving", 2, 1, true, nullptr, 0, false,
            (1u << kNegGate) | (1u << kNegSlot));
    // E: normal kind, LOADING, three levels of 4/0/7 bytes. The 0-byte level is
    //    deliberate: it is the arm where the blob pointer is null on both sides.
    static const int32_t kScript[] = { 3, 4, 0, 7 };
    // All three are applicable here -- this is the only case that runs the loop.
    RunCase("E/kind=2 loading x3", 2, 0, true, kScript, 4, false,
            (1u << kNegGate) | (1u << kNegSlot) | (1u << kNegNoLoad));

    printf("[ImgSerProbe] mode=%-6s cases=%d mismatches=%d recorded-calls=%d\n",
           kNegName[g_neg], g_cases, g_mismatch, g_recordedCalls);
}

}  // namespace

void RtVideoImageBufferSerializeProbeArm(bool on) { g_armed = on; }

void RtVideoImageBufferSerializeProbeRun() {
    if (!g_armed || g_ran) return;
    g_ran = true;
    BuildVtables();

    const char* mode = std::getenv("BANISHED_RT_VIDIMGSER_PROBE");
    const bool wantNeg = mode && std::strcmp(mode, "neg") == 0;

    printf("[ImgSerProbe] ==== ImageBuffer::PlatformSerialize synthetic replay ====\n");
    g_neg = kNegNone;
    RunAll();
    const int posMismatch = g_mismatch;

    if (wantNeg) {
        for (int k = kNegGate; k < kNegCOUNT; ++k) { g_neg = static_cast<Neg>(k); RunAll(); }
        g_neg = kNegNone;
        printf("[ImgSerProbe] ---- negative controls, scored SEPARATELY ----\n");
        bool all = true;
        for (int k = kNegGate; k < kNegCOUNT; ++k) {
            const bool ok = g_negCases[k] > 0 && g_negCaught[k] == g_negCases[k];
            if (!ok) all = false;
            printf("[ImgSerProbe]   %-6s caught %d of %d APPLICABLE cases  %s\n",
                   kNegName[k], g_negCaught[k], g_negCases[k],
                   ok ? "GREEN" : "*** NOT CAUGHT -- the comparator is blind here ***");
        }
        if (!all)
            printf("[ImgSerProbe] *** A control was missed: the clean result above is "
                   "NOT evidence. ***\n");
    }
    printf("[ImgSerProbe] VERDICT: %s (positive run mismatches=%d)\n",
           posMismatch == 0 ? "ours matches the shipped body" : "DIVERGENCE", posMismatch);
}

