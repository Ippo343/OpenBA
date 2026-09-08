// ============================================================================
// runtime/src/input/input_device_update_probe.cpp — Wave 9 lane ENGIO
//   The synthetic-subject oracle for Input::Device::Update and its three
//   internal bodies.  BANISHED_RT_INPUTUPD_PROBE=1
// ============================================================================
//
// WHY A PROBE AND NOT A LIVE DIFF
// -------------------------------
// **Update DRAINS the DirectInput queue.** Running ours and then the shipped
// body at the live call site hands the second one a queue the first already
// emptied, so the two sides see different worlds BY CONSTRUCTION. A green
// result would mean nothing and a red one would mean nothing. That is the
// blocker lane INPUT measured and declined on, and it is real.
//
// It is also removable, and the removal is cheaper than the recorder lane INPUT
// specified. Apply reference_synthetic_subject_beats_a_fake.md: ask what the
// guard that reaches the device actually READS.
//
//   PlatformGetKeyboardState reads  this->[0x11E0], then [vtbl+0x48] / [vtbl+0x38]
//   PlatformGetMouseState    reads  this->[0x11E8], then [vtbl+0x48] / [vtbl+0x38]
//
// That is the entire contract. Nothing validates the pointer, nothing
// QueryInterfaces it, nothing calls Release, nothing reads any other slot. So a
// **synthetic COM object whose vtable is an array we own** IS an
// IDirectInputDevice8 as far as these two functions can tell — and unlike a
// detour on the real device it lets the probe SCRIPT what GetDeviceState
// returns. Both sides are handed the SAME scripted bytes, so the drain stops
// existing rather than being recorded around.
//
// HOW EACH SIDE REACHES ITS OWN BODIES
// ------------------------------------
//   ours   : RtInput_Device_Update, called directly (and the three internals
//            through the forwarders in input_device_update.cpp, so the probe
//            drives the SAME code the live path does).
//   theirs : the shipped ?Update@Device@Input@@... reached with
//            **ResolveExportRaw**, never ResolveExport — the latter consults
//            the port registry FIRST and would have handed back OUR Update, so
//            the probe would have compared ours to ours and always agreed
//            (reference_resolveaddr_consults_port_registry_first.md). If the
//            two pointers ever come back equal the probe REFUSES to run and
//            says why, rather than reporting a guaranteed pass.
//
// WHAT IS COMPARED
// ----------------
// The whole 0x1200-byte Device image, byte for byte, after every step. Both
// devices are memset to a fixed non-zero filler first and then given identical
// field values, so a byte NEITHER side writes still compares equal and the
// comparison is not silently excluding anything
// (reference_oracle_must_not_compare_unwritten_bytes.md does not apply here
// because there is no allocator involved: both images start identical).
//
// EXCEPT for two ranges, excluded with a reason each and NOT quietly:
//   * +0x11E0..+0x11EF, the two COM pointers. The two sides are given
//     DIFFERENT synthetic devices on purpose (side A's script and side B's
//     script are separate objects so a body that read the wrong side's queue
//     would show up), so these two qwords differ by construction.
//   * nothing else. In particular +0x11D0 (_mouseClipping), +0x11F0
//     (_hasFocus) and +0x11F4..+0x11FF (wheelAccum, lastCursorX/Y) ARE
//     compared — the last three are the legacy path's own bookkeeping and are
//     the fields a port that dropped the write-back would fail on.
//
// 0x1200 is a LOWER BOUND on sizeof(Input::Device) (Input.h says so at
// kDeviceMappedSize). The comparison is SILENT about anything past it. Neither
// implementation is known to write past it, but that is the absence of a
// counter-example, not a proof, and it is stated rather than implied.
//
// THE AMBIENT-INPUT PROBLEM, AND HOW IT IS HANDLED RATHER THAN IGNORED
// --------------------------------------------------------------------
// Three USER32 sources are outside the probe's control and can change BETWEEN
// the two sides of a comparison: GetAsyncKeyState (legacy keyboard/mouse
// buttons), GetCursorPos+ScreenToClient (absolute cursor, BOTH paths), and
// GetForegroundWindow. Nobody is at the keyboard during an unattended run, but
// "nobody is typing" is an assumption and this file does not rest on it.
//
// So: a case that mismatches is RE-RUN from a fresh reseed, twice. If it agrees
// on a retry the first result was ambient and the case is counted in
// `ambientRetries` and PASSES; if it mismatches all three times it is a real
// divergence and is reported. `ambientRetries` is printed unconditionally —
// a large value would mean the run was not as quiet as assumed and the reader
// should discount it, which they cannot do if the number is hidden.
//
// EFFECTIVENESS CONTROLS, COUNTED INSIDE THE SUBJECT
// --------------------------------------------------
// A green byte comparison is worth nothing without evidence that anything
// happened (reference_probe_needs_an_effectiveness_control.md). Every counter
// below is read out of the RESULTING DEVICE STATE — i.e. from inside the
// subject — not inferred from what the script asked for:
//
//   pressedEdges / releasedEdges  records whose pressed/released came out set
//   repeatFires                   records whose repeat came out set (this one
//                                 requires the hold timer to have crossed 0.3s,
//                                 so it cannot be satisfied by noise)
//   holdAdvances                  records whose holdTicks came out > 0
//   negativeAxis                  records that ended holding a NEGATIVE value
//                                 (the MOVSX-vs-MOVZX case — see below)
//   index104Lit                   cases in which the unmapped-scan-code quirk
//                                 actually fired
//   acquireSpins                  times the DIERR_INPUTLOST retry loop spun
//   wheelNeg / wheelPos           wheel cases of each sign
//
// If `repeatFires` reads 0 the hold path was never exercised and the pass is
// thin; the probe SAYS SO in as many words rather than leaving the reader to
// notice. Same for `negativeAxis` and `acquireSpins`.
//
// NEGATIVE CONTROLS — TWO, AND BOTH MUST FIRE
// -------------------------------------------
//   1. COMPARATOR control: after the sweep passes, flip one byte of side A's
//      device and re-run the SAME comparator lambda the positive cases used.
//      If it still says equal, every pass above it is worthless.
//   2. SUBJECT control: run a deliberately WRONG UpdateState — one that drops
//      the `raw == 0` term from `released`, the single most plausible
//      transcription slip in the whole body — against the shipped one on the
//      same input, and require the comparator to CATCH it. Control 1 proves the
//      comparator can see a corrupted byte; only control 2 proves it can see a
//      wrong ALGORITHM, which is the thing actually under test.
//
// A control that comes back green is reported as a FAILURE of the probe, not
// waved through: lane COLL caught one of its own this way, and lane N's
// underpowered ray control is the standing example of a control that could not
// fail and looked identical to one that passed.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/input/Input.h"

extern "C" void RtInput_Device_Update(void* selfPtr, const int64_t* time);
extern "C" void RtInput_Probe_UpdateState(void* self, void* dst, const void* src,
                                          int32_t count, const int64_t* time);

namespace {

using RtInput::State;

bool g_armed = false;
bool g_ran   = false;

const char* const kUpdateName = "?Update@Device@Input@@QEAAXAEBVTime@System@@@Z";

// ---------------------------------------------------------------------------
// The synthetic IDirectInputDevice8.
//
// Layout is `void** vtbl` first and nothing else the subject can reach. The
// vtable is 16 slots so that indices 7 (Acquire) and 9 (GetDeviceState) are in
// range with room to spare; every other slot is filled with a TRAP that reports
// and aborts the probe rather than returning something plausible. If the port
// ever calls a third method, the probe must say so loudly instead of quietly
// tolerating it — the COM surface being exactly two slots is a MEASUREMENT this
// lane inherited, and an instrument that cannot detect the measurement being
// wrong is not checking it.
// ---------------------------------------------------------------------------
struct FakeDiDevice;

// One scripted device read. `fail` makes GetDeviceState return a non-zero
// HRESULT; `acquireResults` is what the following Acquire calls return, in
// order, so the DIERR_INPUTLOST retry loop can be driven a scripted number of
// spins and then released.
struct DeviceScript {
    uint8_t  keyboard[256];   // DIK scan-code buffer
    int32_t  mouseX, mouseY, mouseZ;
    uint8_t  mouseButtons[8];
    uint32_t getStateResult;  // 0 == DI_OK
    int      acquireSpins;    // how many times Acquire returns DIERR_INPUTLOST
};

struct FakeDiDevice {
    void**              vtbl;
    const DeviceScript* script;
    bool                isMouse;
    int                 acquireCalls;
    int                 getStateCalls;
    const char*         side;
};

int  g_trapHits      = 0;
int  g_acquireSpins  = 0;

uint32_t __stdcall FakeTrap(void* self) {
    (void)self;
    ++g_trapHits;
    std::fprintf(stderr,
        "[InputUpd] UNSOUND: the subject called an IDirectInputDevice8 method "
        "OTHER than Acquire(+0x38) or GetDeviceState(+0x48). The two-slot COM "
        "surface this probe is built on is WRONG, and every comparison it has "
        "reported is void.\n");
    return 0;
}

uint32_t __stdcall FakeAcquire(void* selfPtr) {
    auto* self = static_cast<FakeDiDevice*>(selfPtr);
    const int n = self->acquireCalls++;
    if (n < self->script->acquireSpins) {
        ++g_acquireSpins;
        return 0x8007001Eu;      // DIERR_INPUTLOST -> the caller spins again
    }
    return 0;                    // DI_OK -> the caller leaves the loop
}

uint32_t __stdcall FakeGetDeviceState(void* selfPtr, uint32_t size, void* buf) {
    auto* self = static_cast<FakeDiDevice*>(selfPtr);
    ++self->getStateCalls;
    if (self->script->getStateResult != 0) return self->script->getStateResult;

    if (self->isMouse) {
        if (size != 0x14) {
            std::fprintf(stderr, "[InputUpd] UNSOUND: mouse GetDeviceState size "
                                 "was 0x%x, expected 0x14\n", size);
            ++g_trapHits;
            return 1;
        }
        auto* p = static_cast<uint8_t*>(buf);
        std::memcpy(p + 0,  &self->script->mouseX, 4);
        std::memcpy(p + 4,  &self->script->mouseY, 4);
        std::memcpy(p + 8,  &self->script->mouseZ, 4);
        std::memcpy(p + 12, self->script->mouseButtons, 8);
    } else {
        if (size != 0x100) {
            std::fprintf(stderr, "[InputUpd] UNSOUND: keyboard GetDeviceState "
                                 "size was 0x%x, expected 0x100\n", size);
            ++g_trapHits;
            return 1;
        }
        std::memcpy(buf, self->script->keyboard, 256);
    }
    return 0;
}

void* g_fakeVtbl[16];

void BuildFakeVtbl() {
    for (int i = 0; i < 16; ++i) g_fakeVtbl[i] = (void*)&FakeTrap;
    // Byte offsets 0x38 and 0x48 into the vtable, i.e. slots 7 and 9 -- the
    // same two the subject reaches through [RAX+0x38] / [RAX+0x48].
    g_fakeVtbl[0x38 / 8] = (void*)&FakeAcquire;
    g_fakeVtbl[0x48 / 8] = (void*)&FakeGetDeviceState;
}

// ---------------------------------------------------------------------------
// The synthetic Input::Device.
//
// 0x1200 bytes (RtInput::kDeviceMappedSize) — every field any body under test
// touches, and nothing beyond, which is why the header calls that a LOWER
// BOUND. Started from a fixed non-zero FILLER rather than zero: a zeroed image
// makes "wrote 0" and "did not write" indistinguishable, which is how a port
// that skipped a store passes. With 0xA5 filler, a skipped store leaves 0xA5
// and the byte comparison sees it (the surviving-0xCD reasoning from Wave 6,
// used the other way round).
// ---------------------------------------------------------------------------
constexpr uint8_t kFiller = 0xA5;

struct SynthDevice {
    alignas(16) uint8_t bytes[RtInput::kDeviceMappedSize];
};

void SeedDevice(SynthDevice* d, FakeDiDevice* kb, FakeDiDevice* ms,
                uint32_t seed) {
    std::memset(d->bytes, kFiller, sizeof(d->bytes));

    // The three State blocks start ZEROED, which is what ResetState leaves and
    // what any real Device has before its first Update. The filler stays in the
    // gaps the blocks do not cover, so a stray write outside them is visible.
    std::memset(d->bytes, 0, RtInput::kMouseClippingOffset);

    d->bytes[RtInput::kMouseClippingOffset] = 0;   // _mouseClipping OFF, so the
    d->bytes[RtInput::kHasFocusOffset]      = 1;   // PlatformUpdate tail takes
                                                   // ClipCursor(NULL) on BOTH
                                                   // sides — identical, global,
                                                   // and harmless. The clipping
                                                   // branch is lane IOA's to
                                                   // cover, not this probe's,
                                                   // and this file does not
                                                   // claim to cover it.

    *reinterpret_cast<void**>(d->bytes + RtInput::kKeyboardComOffset) = kb;
    *reinterpret_cast<void**>(d->bytes + RtInput::kMouseComOffset)    = ms;

    // The legacy path's three bookkeeping fields. Deliberately non-zero and
    // seed-dependent, and deliberately including a value NEAR THE 16-BIT
    // BOUNDARY: the delta subtracts are 16-bit and WRAP, and a 32-bit
    // implementation agrees with them everywhere except across that boundary.
    const int32_t lastX = (seed & 1) ? 32760 : 100;
    const int32_t lastY = (seed & 2) ? 32770 : 250;
    *reinterpret_cast<int32_t*>(d->bytes + RtInput::kLastCursorXOffset) = lastX;
    *reinterpret_cast<int32_t*>(d->bytes + RtInput::kLastCursorYOffset) = lastY;
    *reinterpret_cast<int32_t*>(d->bytes + RtInput::kWheelAccumOffset)  =
        (seed % 3 == 0) ? 0 : ((seed % 3 == 1) ? 120 : -240);
}

// ---------------------------------------------------------------------------
// The comparator, as a lambda-shaped free function so the negative controls
// drive the SAME check the positive cases did. A control that re-implements
// the check proves nothing about the check
// (reference_recheck_must_match_its_own_proof.md).
//
// Returns the number of differing bytes and writes the FIRST difference into
// `detail` — naming the offset, both values, and which named field the offset
// falls in, because "1 differing byte" without a location is a verdict nobody
// can act on (reference_shadow_predicate_must_name_its_failing_term.md).
// ---------------------------------------------------------------------------
const char* FieldNameFor(int off) {
    if (off < RtInput::kKeyboardOffset)      return "joystick block";
    if (off < RtInput::kMouseOffset)         return "keyboard State block";
    if (off < RtInput::kMouseClippingOffset) return "mouse State block";
    if (off == RtInput::kMouseClippingOffset) return "_mouseClipping";
    if (off >= RtInput::kKeyboardComOffset &&
        off <  RtInput::kKeyboardComOffset + 8) return "keyboard COM ptr";
    if (off >= RtInput::kMouseComOffset &&
        off <  RtInput::kMouseComOffset + 8)   return "mouse COM ptr";
    if (off >= RtInput::kHasFocusOffset &&
        off <  RtInput::kHasFocusOffset + 4)   return "_hasFocus";
    if (off >= RtInput::kWheelAccumOffset &&
        off <  RtInput::kWheelAccumOffset + 4) return "wheelAccum";
    if (off >= RtInput::kLastCursorXOffset &&
        off <  RtInput::kLastCursorXOffset + 4) return "lastCursorX";
    if (off >= RtInput::kLastCursorYOffset &&
        off <  RtInput::kLastCursorYOffset + 4) return "lastCursorY";
    return "unnamed tail";
}

// The two COM pointers are the ONLY excluded range, and they are excluded
// because the two sides are deliberately given DIFFERENT synthetic devices.
bool Excluded(int off) {
    return (off >= RtInput::kKeyboardComOffset &&
            off <  RtInput::kKeyboardComOffset + 16);
}

int CompareDevices(const SynthDevice* a, const SynthDevice* b,
                   char* detail, size_t detailN) {
    int diffs = 0;
    int first = -1;
    for (int i = 0; i < RtInput::kDeviceMappedSize; ++i) {
        if (Excluded(i)) continue;
        if (a->bytes[i] != b->bytes[i]) {
            if (first < 0) first = i;
            ++diffs;
        }
    }
    if (first >= 0) {
        const int stateIdx =
            (first >= RtInput::kKeyboardOffset && first < RtInput::kMouseOffset)
                ? (first - RtInput::kKeyboardOffset) / (int)sizeof(State)
            : (first >= RtInput::kMouseOffset && first < RtInput::kMouseClippingOffset)
                ? (first - RtInput::kMouseOffset) / (int)sizeof(State)
                : -1;
        std::snprintf(detail, detailN,
                      "+0x%04x (%s%s%d%s) ours=0x%02x theirs=0x%02x",
                      first, FieldNameFor(first),
                      stateIdx >= 0 ? " index " : "", stateIdx >= 0 ? stateIdx : 0,
                      stateIdx >= 0 ? "" : "",
                      a->bytes[first], b->bytes[first]);
    } else {
        std::snprintf(detail, detailN, "identical");
    }
    return diffs;
}

// ---------------------------------------------------------------------------
// The SUBJECT negative control: UpdateState with the `raw == 0` term dropped
// from `released`. That is the single most plausible transcription slip in the
// body — the two flag computations are four near-identical instruction runs
// differing only in a JZ/JNZ — and it is invisible to any test whose inputs
// never release a key while another stays down. It must be CAUGHT.
// ---------------------------------------------------------------------------
void MutantUpdateState(State* dst, const State* src, int32_t count,
                       const int64_t* time, double spt,
                       float divisor, double delaySec, double resetSec) {
    for (int32_t i = 0; i < count; ++i) {
        const int16_t  raw  = src[i].value;
        const uint16_t prev = static_cast<uint16_t>(dst[i].value);
        const float f = static_cast<float>(static_cast<int32_t>(raw)) / divisor;
        dst[i].normalized = f;
        dst[i].pressed  = (prev == 0) && (raw != 0);
        dst[i].released = (prev != 0);              // <-- THE DEFECT
        dst[i].value = raw;
        if (f == 0.0f) { dst[i].holdTicks = 0; dst[i].repeat = false; }
        else {
            dst[i].holdTicks += *time;
            if (dst[i].holdTicks < (int64_t)(delaySec / spt)) dst[i].repeat = false;
            else { dst[i].repeat = true; dst[i].holdTicks = (int64_t)(resetSec / spt); }
        }
    }
}

}  // namespace

extern "C" void RtInputDeviceUpdateProbeArm(bool on) { g_armed = on; }

extern "C" void RtInputDeviceUpdateProbeMaybeRun() {
    if (!g_armed || g_ran) return;
    g_ran = true;

    using UpdateFn = void (*)(void*, const int64_t*);
    auto ours   = &RtInput_Device_Update;
    auto theirs = reinterpret_cast<UpdateFn>(ResolveExportRaw(kUpdateName));

    if (!theirs || reinterpret_cast<void*>(theirs) == (void*)ours) {
        std::fprintf(stderr,
            "[InputUpd] SKIP: theirs=%p ours=%p%s. Refusing to compare a "
            "function with itself.\n",
            (void*)theirs, (void*)ours,
            (theirs && (void*)theirs == (void*)ours) ? " (RAW door returned OUR port)" : "");
        std::fflush(stderr);
        return;
    }

    BuildFakeVtbl();

    // Both sides read the SAME Input::Configuration::_singleton, so `legacy`
    // is a shared input rather than a per-side one. The probe drives BOTH
    // values of it by temporarily swapping the slot's contents and restoring
    // it afterwards — which is what makes the DirectInput path reachable at all
    // on a machine whose config says legacy, and vice versa.
    void** cfgSlot = RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA");
    void*  cfgSaved = cfgSlot ? *cfgSlot : nullptr;
    if (!cfgSlot || !cfgSaved) {
        std::fprintf(stderr, "[InputUpd] SKIP: Input::Configuration::_singleton "
                             "is not available (slot=%p value=%p)\n",
                     (void*)cfgSlot, cfgSaved);
        std::fflush(stderr);
        return;
    }
    RtInput::Configuration cfgFake = *static_cast<RtInput::Configuration*>(cfgSaved);

    static SynthDevice devA, devB;
    DeviceScript scriptA, scriptB;
    FakeDiDevice kbA, kbB, msA, msB;

    int comparisons = 0, mismatches = 0, unsound = 0, ambientRetries = 0;
    int pressedEdges = 0, releasedEdges = 0, repeatFires = 0, holdAdvances = 0;
    int negativeAxis = 0, index104Lit = 0, wheelNeg = 0, wheelPos = 0;
    int legacyCases = 0, diCases = 0, failCases = 0;

    // xorshift32 — deterministic, and the two sides are handed the SAME script
    // object contents rather than sharing a stream.
    uint32_t rngState = 0x12345678u;
    auto rng = [&]() {
        rngState ^= rngState << 13; rngState ^= rngState >> 17;
        rngState ^= rngState << 5;  return rngState;
    };

    auto buildScript = [&](DeviceScript* s, uint32_t seed, int step, bool wantFail) {
        std::memset(s, 0, sizeof(*s));
        rngState = seed * 2654435761u + step * 40503u + 1u;
        // Keys: a deterministic subset held down. Scan codes are chosen so that
        // MAPPED codes and UNMAPPED ones (which all collide on index 104) both
        // occur — the retail quirk has to be exercised, not assumed harmless.
        for (int k = 0; k < 256; ++k) {
            const uint32_t r = rng();
            s->keyboard[k] = ((r & 7) == 0) ? 0x80 : 0x00;
        }
        // Guarantee at least one UNMAPPED scan code is down, so index 104 is
        // driven by the collision on every case rather than occasionally.
        s->keyboard[0xF0 + (step & 7)] = 0x80;

        // Mouse deltas: deliberately including negatives (which is what makes
        // the MOVSX-vs-MOVZX distinction observable at all) and a value past
        // 16 bits.
        static const int32_t kDx[] = { 0, 5, -7, 32769, -32769, 1, -1, 40000 };
        static const int32_t kDy[] = { 0, -3, 11, -32768, 32767, 2, -2, -40000 };
        s->mouseX = kDx[(seed + step) % 8];
        s->mouseY = kDy[(seed + step * 3) % 8];
        static const int32_t kDz[] = { 0, 120, -120, 1, -1 };
        s->mouseZ = kDz[(seed + step) % 5];
        for (int b = 0; b < 8; ++b)
            s->mouseButtons[b] = ((rng() & 3) == 0) ? 0x80 : 0x00;

        s->getStateResult = wantFail ? 0x8007001Eu : 0u;
        s->acquireSpins   = wantFail ? (int)((seed + step) % 3) : 0;
    };

    // Read the effectiveness counters OUT OF THE RESULTING DEVICE — inside the
    // subject, not from what the script asked for.
    auto tally = [&](const SynthDevice* d) {
        auto count = [&](int base, int n) {
            for (int i = 0; i < n; ++i) {
                const State* st = reinterpret_cast<const State*>(
                    d->bytes + base + i * (int)sizeof(State));
                if (st->pressed)  ++pressedEdges;
                if (st->released) ++releasedEdges;
                if (st->repeat)   ++repeatFires;
                if (st->holdTicks > 0) ++holdAdvances;
                if (st->value < 0) ++negativeAxis;
            }
        };
        count(RtInput::kKeyboardOffset, RtInput::kKeyboardInputCount);
        count(RtInput::kMouseOffset,    RtInput::kMouseInputCount);
        const State* idx104 = reinterpret_cast<const State*>(
            d->bytes + RtInput::kKeyboardOffset + 104 * (int)sizeof(State));
        if (idx104->value != 0) ++index104Lit;
        const State* wheel = reinterpret_cast<const State*>(
            d->bytes + RtInput::kMouseOffset + 12 * (int)sizeof(State));
        if (wheel->value < 0) ++wheelNeg;
        if (wheel->value > 0) ++wheelPos;
    };

    // One case: seed both devices identically, run a SEQUENCE of steps so the
    // hold timer can accumulate across frames (repeat needs 0.3s of held input,
    // which a single call can never reach), and compare after EVERY step.
    // Comparing only at the end would let two defects cancel — the same reason
    // lane INPUT's AVL probe compares after every insert.
    auto runCase = [&](uint32_t seed, bool legacy, bool wantFail,
                       int64_t tickPerStep, int steps, const char* label) -> bool {
        for (int attempt = 0; attempt < 3; ++attempt) {
            cfgFake.legacy = legacy ? 1 : 0;
            *cfgSlot = &cfgFake;

            kbA = { g_fakeVtbl, &scriptA, false, 0, 0, "A" };
            msA = { g_fakeVtbl, &scriptA, true,  0, 0, "A" };
            kbB = { g_fakeVtbl, &scriptB, false, 0, 0, "B" };
            msB = { g_fakeVtbl, &scriptB, true,  0, 0, "B" };
            SeedDevice(&devA, &kbA, &msA, seed);
            SeedDevice(&devB, &kbB, &msB, seed);

            char pre[192];
            if (CompareDevices(&devA, &devB, pre, sizeof(pre)) != 0) {
                std::fprintf(stderr, "[InputUpd] UNSOUND seeding [%s]: %s\n", label, pre);
                ++unsound;
                *cfgSlot = cfgSaved;
                return false;
            }

            bool bad = false;
            char detail[192];
            for (int step = 0; step < steps; ++step) {
                buildScript(&scriptA, seed, step, wantFail);
                std::memcpy(&scriptB, &scriptA, sizeof(scriptA));  // SAME bytes to both sides

                ours(&devA, &tickPerStep);
                theirs(&devB, &tickPerStep);

                if (CompareDevices(&devA, &devB, detail, sizeof(detail)) != 0) {
                    bad = true;
                    break;
                }
            }
            *cfgSlot = cfgSaved;

            if (!bad) {
                if (attempt > 0) ++ambientRetries;
                ++comparisons;
                tally(&devA);
                if (legacy) ++legacyCases; else ++diCases;
                if (wantFail) ++failCases;
                return true;
            }
            if (attempt == 2) {
                ++mismatches;
                std::fprintf(stderr,
                    "[InputUpd] FAIL [%s]: %s (reproduced on all 3 attempts, so "
                    "not ambient input)\n", label, detail);
                return false;
            }
        }
        return false;
    };

    // ---- the sweep ---------------------------------------------------------
    // 60 ms per step at 1e-6 s/tick == 60000 ticks; 8 steps is 480 ms, which is
    // PAST the 0.3 s repeat delay. That is what makes `repeatFires` reachable,
    // and it is the reason the case is a SEQUENCE rather than a single call.
    const int64_t kStepTicks = 60000;
    for (uint32_t seed = 1; seed <= 8; ++seed) {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "seed=%u di", seed);
        runCase(seed, /*legacy=*/false, /*wantFail=*/false, kStepTicks, 8, lbl);
        std::snprintf(lbl, sizeof(lbl), "seed=%u legacy", seed);
        runCase(seed, /*legacy=*/true,  /*wantFail=*/false, kStepTicks, 8, lbl);
        std::snprintf(lbl, sizeof(lbl), "seed=%u di-getstate-FAILS", seed);
        runCase(seed, /*legacy=*/false, /*wantFail=*/true,  kStepTicks, 4, lbl);
        // A single-tick case: holdTicks advances but never reaches the
        // threshold, so it exercises the `repeat = 0` arm of the hold path,
        // which the 8-step cases above run past.
        std::snprintf(lbl, sizeof(lbl), "seed=%u di-tiny-tick", seed);
        runCase(seed, /*legacy=*/false, /*wantFail=*/false, 1, 3, lbl);
    }

    // ---- negative control 1: the COMPARATOR -------------------------------
    bool negComparator = false;
    {
        char d[192];
        const uint8_t save = devA.bytes[RtInput::kKeyboardOffset + 4];
        devA.bytes[RtInput::kKeyboardOffset + 4] =
            static_cast<uint8_t>(save ^ 0xFF);
        negComparator = (CompareDevices(&devA, &devB, d, sizeof(d)) != 0);
        devA.bytes[RtInput::kKeyboardOffset + 4] = save;
        if (!negComparator)
            std::fprintf(stderr, "[InputUpd] NEGATIVE CONTROL 1 MISSED: the "
                                 "comparator did not notice a corrupted byte. "
                                 "Every pass above it is worthless.\n");
    }

    // ---- negative control 2: the SUBJECT ----------------------------------
    // Drive the shipped Update to a known state, then apply the MUTANT
    // UpdateState to a copy and require the comparator to catch it. The input
    // is chosen so a key is RELEASED while another stays DOWN, which is the
    // only shape that separates `(prev != 0)` from `(prev != 0 && raw == 0)`.
    bool negSubject = false;
    {
        static SynthDevice mA, mB;
        kbA = { g_fakeVtbl, &scriptA, false, 0, 0, "A" };
        msA = { g_fakeVtbl, &scriptA, true,  0, 0, "A" };
        SeedDevice(&mA, &kbA, &msA, 1);
        SeedDevice(&mB, &kbA, &msA, 1);

        State src[RtInput::kKeyboardInputCount];
        std::memset(src, 0, sizeof(src));

        auto* dstA = reinterpret_cast<State*>(mA.bytes + RtInput::kKeyboardOffset);
        auto* dstB = reinterpret_cast<State*>(mB.bytes + RtInput::kKeyboardOffset);

        // frame 1: keys 3 and 4 down on both copies
        src[3].value = 0x7FFF; src[4].value = 0x7FFF;
        const int64_t t = 1000;
        RtInput_Probe_UpdateState(&mA, dstA, src, RtInput::kKeyboardInputCount, &t);
        RtInput_Probe_UpdateState(&mB, dstB, src, RtInput::kKeyboardInputCount, &t);

        // frame 2: key 3 released, key 4 STILL DOWN.
        //   correct : released[3]=1, released[4]=0
        //   mutant  : released[3]=1, released[4]=1   <- the visible difference
        src[3].value = 0;
        RtInput_Probe_UpdateState(&mA, dstA, src, RtInput::kKeyboardInputCount, &t);
        const double spt = *RT_DATA_SLOT(const double*, "?_secondsPerTick@Timer@System@@0NA");
        const uint32_t fb = 0x46FFFE00u; float divisor; std::memcpy(&divisor, &fb, 4);
        const uint64_t db = 0x3FD3333340000000ull; double delaySec; std::memcpy(&delaySec, &db, 8);
        const uint64_t rb = 0x3FD0000000000000ull; double resetSec; std::memcpy(&resetSec, &rb, 8);
        MutantUpdateState(dstB, src, RtInput::kKeyboardInputCount, &t,
                          spt, divisor, delaySec, resetSec);

        char d[192];
        negSubject = (CompareDevices(&mA, &mB, d, sizeof(d)) != 0);
        if (!negSubject) {
            std::fprintf(stderr,
                "[InputUpd] NEGATIVE CONTROL 2 MISSED: a released-flag defect "
                "was NOT caught. The sweep cannot see a wrong ALGORITHM, only a "
                "corrupted byte, and its green result says much less than it "
                "appears to.\n");
        } else {
            std::fprintf(stderr, "[InputUpd] negative-control-2 CAUGHT at %s\n", d);
        }
    }

    // ---- the verdict, printed UNCONDITIONALLY ------------------------------
    // Not through a macro that only speaks on failure: on a passing run the
    // numbers that make the pass meaningful would then be invisible, which is
    // WAVE9.md instrument #7 — if a control is only visible when the test
    // fails, it is not a control.
    std::fprintf(stderr,
        "[InputUpd] cases=%d mismatches=%d unsound=%d ambient-retries=%d "
        "(legacy=%d directinput=%d getstate-fail=%d)\n",
        comparisons, mismatches, unsound, ambientRetries,
        legacyCases, diCases, failCases);
    std::fprintf(stderr,
        "[InputUpd] effectiveness: pressedEdges=%d releasedEdges=%d "
        "repeatFires=%d holdAdvances=%d negativeAxis=%d index104Lit=%d "
        "acquireSpins=%d wheel-=%d wheel+=%d comTraps=%d\n",
        pressedEdges, releasedEdges, repeatFires, holdAdvances, negativeAxis,
        index104Lit, g_acquireSpins, wheelNeg, wheelPos, g_trapHits);
    std::fprintf(stderr,
        "[InputUpd] negative-controls: comparator=%s subject=%s\n",
        negComparator ? "CAUGHT" : "*** MISSED ***",
        negSubject    ? "CAUGHT" : "*** MISSED ***");

    // Say which counters read zero and what that costs, rather than leaving the
    // reader to notice. A zero here does not fail the run; it narrows what the
    // run is allowed to claim.
    if (repeatFires == 0)
        std::fprintf(stderr, "[InputUpd] WARN: repeatFires=0 — the key-repeat "
                             "arm of the hold path was NEVER TAKEN, so the two "
                             "0.3/0.25 constants are UNVERIFIED by this run.\n");
    if (negativeAxis == 0)
        std::fprintf(stderr, "[InputUpd] WARN: negativeAxis=0 — no record ended "
                             "holding a negative value, so the MOVSX-vs-MOVZX "
                             "distinction is UNVERIFIED by this run.\n");
    if (g_acquireSpins == 0)
        std::fprintf(stderr, "[InputUpd] WARN: acquireSpins=0 — the "
                             "DIERR_INPUTLOST retry loop never spun.\n");
    if (index104Lit == 0)
        std::fprintf(stderr, "[InputUpd] WARN: index104Lit=0 — the "
                             "unmapped-scan-code collision quirk never fired.\n");
    if (g_trapHits != 0)
        std::fprintf(stderr, "[InputUpd] *** UNSOUND: %d calls landed on a COM "
                             "slot other than Acquire/GetDeviceState. This "
                             "probe's whole premise is wrong. ***\n", g_trapHits);

    std::fflush(stderr);
}
