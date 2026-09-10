// ============================================================================
// standalone/rt_orphan_probe.cpp — verify ports the harness cannot reach
// ============================================================================
//
// `tools/runtime_census.py` ends with a list headed "Ports with no caller in
// our tree". Those rows are the exact failure mode
// `reference_uncalled_port_is_not_verified.md` was written about: a port with
// zero callers reports `calls=0 divergences=0` and reads as green. A WRONG
// `Quaternion::operator*` survived the whole of Wave 1 in that state.
//
// Wave 9 prep gave each of the eight a verdict (docs/runtime_port/WAVE9.md).
// Six of them turned out to be fine for a stated reason — three are exercised
// through a DIRECT C++ call from a sibling port rather than through the export
// (so the body runs and only the registration is idle), and three are the
// char/schar/short array overloads of the 18-member `IO::Serializer` forwarder
// family, all of which `RtIoSerializerRegister` verifies structurally at
// registration by decoding the DLL's own tail-jump and checking the
// ReaderWriter vtable slot matches.
//
// This file is for the remainder: a port that no harness path reaches, that no
// sibling calls, and that no structural check covers. There is no way to earn
// a `calls=` number for those, so they are probed directly — our registered
// port and the shipped DLL's body, side by side, over inputs chosen to cover
// every branch.
//
// THE PROBE IS NOT A SUBSTITUTE FOR A CALL SITE. It proves the body computes
// what the DLL computes for the inputs given; it cannot prove the harness would
// ever reach it, and it cannot see an ABI mistake that only shows up under a
// real caller's register state. It is the best available oracle for a function
// nothing calls, and it is strictly better than the `calls=0 divergences=0`
// line that reads as a pass today.
//
//     BANISHED_RT_ORPHAN_PROBE=1 make sa-run-menu
//     grep RtOrphan "C:/GOG Games/Banished/standalone_error.log"
//
// It runs once, before the game starts, and does nothing unless the env var is
// set. It must run AFTER RuntimePortInit() so the ports are registered.
// ============================================================================

#include "resolve.h"
#include "runtime_detour.h"
#include "../engine/types/common.h"
#include "../runtime/types/math/Math.h"
#include "../runtime/types/audio/Audio.h"
#include "../runtime/types/application/Application.h"
#include "../runtime/types/save/SaveLoad.h"
#include "../runtime/src/collection/set_avl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace {

bool Enabled() {
    const char* e = getenv("BANISHED_RT_ORPHAN_PROBE");
    return e && *e && strcmp(e, "0") != 0;
}

int g_pass = 0;
int g_fail = 0;

// Set while a NEGATIVE CONTROL is being driven through the same comparator the
// real cases used. A control's failure is the desired outcome, so it must not
// be counted as a probe failure — the first run of the Application section
// finished "16 pass, 2 fail" where both "failures" were controls working
// exactly as designed, which is a headline number that lies in the safe
// direction and would still have to be explained by every later reader.
bool g_inControl = false;

void Report(const char* subject, const char* caseName, bool ok, const char* detail) {
    if (ok) {
        ++g_pass;
    } else if (g_inControl) {
        fprintf(stderr, "[RtOrphan] CONTROL-CAUGHT %s [%s]: %s\n", subject, caseName, detail);
    } else {
        ++g_fail;
        fprintf(stderr, "[RtOrphan] FAIL %s [%s]: %s\n", subject, caseName, detail);
    }
}

// RAII so an early return cannot leave the flag set and silence a real failure.
struct ControlScope {
    ControlScope()  { g_inControl = true;  }
    ~ControlScope() { g_inControl = false; }
};

// ---------------------------------------------------------------------------
// Math::Vector2::Normalize(Scalar&) const   —  ?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z
// retail 0x18006c7b0. Member with a struct return: RCX=this, RDX=hidden ret,
// R8=&mag.
//
// WHY THIS ONE. It is the "with magnitude" overload; the plain `Normalize`
// beside it is called ~100k times a run, but nothing in the tree has ever asked
// for this one — it appears in no `[RtDiff]` line of any recorded wave run
// (checked across docs/runtime_port/wave*.txt: only Vector3::NormalizeWithMag
// is ever there). It is also pure, tiny, and has exactly one branch, which
// makes it cheap to probe exhaustively rather than argue about.
// ---------------------------------------------------------------------------
typedef Vector2* (*Vec2NormMagFn)(const Vector2*, Vector2*, Scalar*);

void ProbeVector2NormalizeWithMag() {
    const char* kName = "?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z";
    const char* kSubject = "Math::Vector2::Normalize(Scalar&)";

    Vec2NormMagFn ours = reinterpret_cast<Vec2NormMagFn>(ResolveExport(kName));
    Vec2NormMagFn theirs = reinterpret_cast<Vec2NormMagFn>(ResolveExportRaw(kName));

    if (!ours || !theirs) {
        fprintf(stderr, "[RtOrphan] SKIP %s: ours=%p theirs=%p\n", kSubject,
                (void*)ours, (void*)theirs);
        ++g_fail;
        return;
    }
    if (ours == theirs) {
        // ResolveExport consults the port registry FIRST
        // (reference_resolveaddr_consults_port_registry_first.md). Getting the
        // same pointer twice means the port is NOT registered, so the probe
        // would be comparing the DLL against itself and passing vacuously.
        fprintf(stderr, "[RtOrphan] FAIL %s: port not registered — ResolveExport "
                        "and ResolveExportRaw both returned %p, so this probe "
                        "would compare the DLL against itself\n", kSubject, (void*)ours);
        ++g_fail;
        return;
    }

    struct Case { const char* name; float x, y; };
    static const Case kCases[] = {
        // The one branch: len == 0 returns *self untouched and writes mag 0.
        { "zero",             0.0f,      0.0f      },
        { "unit-x",           1.0f,      0.0f      },
        { "unit-y",           0.0f,      1.0f      },
        { "negative",        -3.0f,     -4.0f      },   // len 5 exactly
        { "3-4",              3.0f,      4.0f      },
        { "tiny",             1e-20f,    1e-20f    },   // len underflows toward 0 but is not 0
        { "huge",             1e20f,     1e20f     },   // len overflows toward inf
        { "mixed-magnitude",  1e-8f,     1e8f      },
        { "irrational",       0.7071068f, 0.7071068f },
        { "one-zero-signed", -0.0f,      0.0f      },   // -0.0: len is 0, sign must survive
    };

    for (const Case& c : kCases) {
        Vector2 self{ c.x, c.y };

        // Poison both output buffers identically so a field NEITHER side writes
        // cannot read as agreement OR as a divergence
        // (reference_oracle_must_not_compare_unwritten_bytes.md).
        Vector2 outOurs, outTheirs;
        Scalar  magOurs,  magTheirs;
        memset(&outOurs, 0xCD, sizeof(outOurs));
        memset(&outTheirs, 0xCD, sizeof(outTheirs));
        memset(&magOurs, 0xCD, sizeof(magOurs));
        memset(&magTheirs, 0xCD, sizeof(magTheirs));

        ours(&self, &outOurs, &magOurs);
        theirs(&self, &outTheirs, &magTheirs);

        bool ok = memcmp(&outOurs, &outTheirs, sizeof(Vector2)) == 0
               && memcmp(&magOurs, &magTheirs, sizeof(Scalar)) == 0;
        if (!ok) {
            char detail[256];
            snprintf(detail, sizeof(detail),
                     "in=(%g,%g) ours=(%g,%g) mag=%g / theirs=(%g,%g) mag=%g",
                     (double)c.x, (double)c.y,
                     (double)outOurs.x, (double)outOurs.y, (double)magOurs.v,
                     (double)outTheirs.x, (double)outTheirs.y, (double)magTheirs.v);
            Report(kSubject, c.name, false, detail);
        } else {
            Report(kSubject, c.name, true, nullptr);
        }
    }
}

// ---------------------------------------------------------------------------
// Save::SaveLoad::GenerateSaveName() const   retail 0x1800c1250   ordinal 2401
// ?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ
//
// NOT actually an orphan — `UpdateSaveBody` (saveload_update.cpp) calls it
// through the same NameGetterFn door when the selected save row's `saveIndex`
// is negative (the blank "new save" slot). But reaching that call site needs
// the Save dialog open with the blank row selected and its in-dialog
// Save/confirm button clicked, which Wave 14 lane GENNAME could not drive
// through the existing pause-menu harness in one session — so the live
// `RT_DIFF` site's `calls=` reads 0 for this run and would read as an
// unproven green line on its own
// (reference_uncalled_port_is_not_verified.md). Probed directly here instead,
// over a sweep of synthetic save lists that exercises every branch: an empty
// list (no comparison loop at all), no collision, one collision, a run of N
// collisions, a gap the loop must not skip over, and a null row name (the
// state an unwritten SaveInfo.name leaves it in).
//
// Row names are built as LOCAL, stack-resident fake System::String blocks
// (`length` at -8, a nonzero placeholder `refcount` at -4, per
// runtime/types/system/String.h) rather than through the real allocator:
// GenerateSaveName only ever READS a row's name (length-then-bytes, mirrored
// by RtString_StrCompare) and never releases it, so a real heap Wallocation
// buys nothing here and a stack fake is simpler to prove leak-free. The
// CANDIDATE string coming back out of `ret`, by contrast, is a real heap
// String from `String::Format` on both sides (ours and theirs each format
// their own), and each is released after the comparison through the same
// refcount-idiom the port itself uses.
// ---------------------------------------------------------------------------
namespace {

using GenNameFn      = const char** (*)(const RtSave::SaveLoad*, const char**);
using StrFormatRawFn  = void* (*)(void*, const char*, ...);
using MemFreeRawFn    = void (*)(void*);

// `p + 0`: int32 length (excluding NUL). `p + 4`: int32 refcount. `p + 8`:
// the bytes GenerateSaveName actually reads.
struct FakeStringBlock {
    int32_t length;
    int32_t refcount;
    char    data[32];
};

const char* MakeFakeRowName(FakeStringBlock& blk, const char* text) {
    if (text == nullptr) return nullptr;
    size_t len = strlen(text);
    if (len >= sizeof(blk.data)) len = sizeof(blk.data) - 1;
    blk.length   = static_cast<int32_t>(len);
    blk.refcount = 1;   // never decremented by the subject; kept non-zero defensively
    memcpy(blk.data, text, len);
    blk.data[len] = '\0';
    return blk.data;
}

void ReleaseProbeAnswer(const char** slot, MemFreeRawFn memFree) {
    const char* p = *slot;
    if (p == nullptr) return;
    int32_t* refc = reinterpret_cast<int32_t*>(const_cast<char*>(p) - 4);
    if (--(*refc) == 0) memFree(const_cast<char*>(p) - 8);
    *slot = nullptr;
}

void ProbeSaveLoadGenerateSaveName() {
    const char* kName    = "?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ";
    const char* kSubject = "Save::SaveLoad::GenerateSaveName";

    GenNameFn ours   = reinterpret_cast<GenNameFn>(ResolveExport(kName));
    GenNameFn theirs = reinterpret_cast<GenNameFn>(ResolveExportRaw(kName));
    if (!ours || !theirs) {
        fprintf(stderr, "[RtOrphan] SKIP %s: ours=%p theirs=%p\n", kSubject, (void*)ours, (void*)theirs);
        ++g_fail;
        return;
    }
    if (ours == theirs) {
        fprintf(stderr, "[RtOrphan] FAIL %s: port not registered -- ResolveExport and "
                        "ResolveExportRaw both returned %p\n", kSubject, (void*)ours);
        ++g_fail;
        return;
    }

    StrFormatRawFn fmt = reinterpret_cast<StrFormatRawFn>(
        ResolveExportRaw("?Format@String@System@@SA?AV12@PEBDZZ"));
    MemFreeRawFn memFree = reinterpret_cast<MemFreeRawFn>(
        ResolveExportRaw("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    if (!fmt || !memFree) {
        fprintf(stderr, "[RtOrphan] SKIP %s: fmt=%p memFree=%p\n", kSubject, (void*)fmt, (void*)memFree);
        ++g_fail;
        return;
    }

    struct Case { const char* name; std::initializer_list<const char*> rowNames; };
    static const Case kCases[] = {
        { "empty-list",              {} },
        { "no-collision",            { "Foo", "Bar" } },
        { "collides-Save0",          { "Save0" } },
        { "collides-0-and-1",        { "Save0", "Save1" } },
        { "collides-0-1-2-3",        { "Save0", "Save1", "Save2", "Save3" } },
        { "gap-loop-must-not-skip",  { "Save0", "Save2" } },   // must land on "Save1", not "Save3"
        { "null-row-name",           { nullptr } },
    };

    for (const Case& c : kCases) {
        FakeStringBlock blocks[8]{};
        RtSave::SaveInfo rows[8]{};
        int n = 0;
        for (const char* t : c.rowNames) {
            rows[n].name = MakeFakeRowName(blocks[n], t);
            ++n;
        }

        RtSave::SaveLoad selfOurs{};
        RtSave::SaveLoad selfTheirs{};
        selfOurs._savesData   = rows;
        selfOurs._savesSize   = n;
        selfTheirs._savesData = rows;
        selfTheirs._savesSize = n;

        const char* ansOurs   = nullptr;
        const char* ansTheirs = nullptr;
        ours(&selfOurs, &ansOurs);
        theirs(&selfTheirs, &ansTheirs);

        bool ok = (ansOurs && ansTheirs && strcmp(ansOurs, ansTheirs) == 0) ||
                  (!ansOurs && !ansTheirs);
        char detail[128];
        snprintf(detail, sizeof(detail), "ours=\"%s\" theirs=\"%s\"",
                 ansOurs ? ansOurs : "(null)", ansTheirs ? ansTheirs : "(null)");
        Report(kSubject, c.name, ok, detail);

        ReleaseProbeAnswer(&ansOurs, memFree);
        ReleaseProbeAnswer(&ansTheirs, memFree);
    }

    // NEGATIVE CONTROL -- same comparator, same subject the real cases compare
    // (the returned string's content), deliberately fed the WRONG expectation
    // so the control itself is the thing that must fail
    // (reference_control_must_pick_a_compared_subject.md).
    {
        ControlScope cs;
        FakeStringBlock blk{};
        RtSave::SaveInfo rows[1]{};
        rows[0].name = MakeFakeRowName(blk, "Save0");
        RtSave::SaveLoad self{};
        self._savesData = rows;
        self._savesSize = 1;

        const char* ans = nullptr;
        ours(&self, &ans);   // real answer is "Save1" (Save0 collides)
        bool ok = (ans && strcmp(ans, "Save0") == 0);   // deliberately the WRONG expectation
        Report(kSubject, "control-wrong-expectation", ok, "expected mismatch: real answer is \"Save1\"");
        ReleaseProbeAnswer(&ans, memFree);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Audio::Scene — the channel-teardown family, on a SYNTHETIC scene
// ---------------------------------------------------------------------------
//
// WAVE8_5.md §9 held six Audio functions back behind one root cause — every
// path ends in a stored completion callback fired through a function pointer,
// so "replaying either function fires every live effect's completion handler
// twice", and concluded the lane needed a **Device-level fake**.
//
// The root cause is right and the conclusion does not follow. Reading the five
// retail bodies per function shows the device is reachable ONLY through a
// non-null `Channel::platformChannel[b]`:
//
//   PauseEffect  0x1800df4f8   TEST RDX,RDX / JZ  before CALL 0x1800de200
//   StopEffect   0x1800df5ab   TEST RDX,RDX / JZ  before CALL 0x1800de200
//   StopAll      0x1800df1c8   TEST RCX,RCX / JZ  before the inlined body
//   Pause        reaches the device only via PauseEffect
//   StopEffect3D reaches it only via StopEffect
//
// and `platformChannel[]` is a field of the **Scene**, not of the Device. So a
// synthetic Scene whose 256 channels have null platform channels puts the
// device out of reach BY CONSTRUCTION — no interposer, no recorder, no fake —
// and both implementations can then be stepped side by side over the whole
// state space. `StopAll` even loads the Device singleton into RBX at
// 0x1800df192 before the null test; it is never dereferenced, so the singleton
// may legitimately be null here.
//
// TWO INDEPENDENT SCENES, not one scene twice. That is what makes this a
// replay rather than the `RegisterInput` trap WAVE8_5.md §9 warns about:
// `Pause`'s first line is `if (paused[type] == paused) return;`, so a second
// call on ONE scene would early-return and report green having compared
// nothing. Each side gets its own byte-identical copy, seeded from the same
// deterministic PRNG.
//
// EXCLUDED FROM THE COMPARISON, and why: `Channel::_pad0c` and
// `Channel::_pad6d[3]`. Retail's `StopEffect` (0x1800df5e6) and `StopAll`
// (0x1800df2af) build the reset record by writing a DWORD to a stack slot and
// reading a QWORD back, so those 7 bytes per channel receive whatever the
// CALLER left on the stack. Comparing them would fail 100% of calls and read
// as a catastrophic port defect — the mirror image of the Wave 6 oracle that
// compared bytes neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes.md). Both are documented
// as never read, in runtime/types/audio/Audio.h.
//
// `EffectSlot::positions._data` is excluded for a different and weaker reason:
// the two scenes allocate independently, so the POINTERS cannot match. Its
// null-ness, `_size` and `_capacity` are all compared.
// ---------------------------------------------------------------------------

typedef void (*ScenePauseFn)      (void*, int32_t, bool);
typedef void (*SceneHandleFn)     (void*, uint32_t);
typedef void (*SceneStopAllFn)    (void*);
typedef void* (*RtMemAllocFn)     (uint64_t, int);

// Phase-2 sentinel platform-channel pointers. Chosen far from any real
// mapping and never dereferenced: Device::Stop is detoured to a recorder
// before any scene carrying these is stepped, and the detour is removed
// immediately afterwards.
constexpr uint64_t kFakeChannelBase = 0x0000600000000000ull;

struct AudioStopCall { void* device; void* channel; };
AudioStopCall g_audioStopLog[2][2048];
int      g_audioStopCount[2] = {0, 0};
int      g_audioStopSide = -1;          // -1 = not inside a probe case
RtDetour g_audioStopDetour;
typedef void (*DeviceStopFn)(void*, void*);
DeviceStopFn g_audioStopTramp = nullptr;

// The whole "device fake" WAVE8_5.md §12 item 2 asked for, in fifteen lines.
// It is not a COM interposer and does not model a device: it records the one
// call the Scene-level family makes and returns. Anything arriving while the
// probe is not running is passed through to the real body, so installing it
// cannot silently swallow a genuine stop.
void DetouredAudioDeviceStop(void* device, void* channel) {
    if (g_audioStopSide < 0) {
        if (g_audioStopTramp) g_audioStopTramp(device, channel);
        return;
    }
    int side = g_audioStopSide;
    if (g_audioStopCount[side] < 2048)
        g_audioStopLog[side][g_audioStopCount[side]] = { device, channel };
    ++g_audioStopCount[side];
}

struct SynthScene {
    RtAudio::Scene      scene;
    RtAudio::Channel    channels[256];
    RtAudio::EffectSlot effects[4096];
};

// xorshift32 — deterministic, and identical for both sides because each side
// is seeded with the same value rather than sharing a stream.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
    uint32_t operator()() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
};

// Seed one scene. Called once per side with the SAME seed, so the two scenes
// start byte-identical (modulo the two `positions` allocations, which are
// separate buffers with identical contents).
// `fakeChannels` switches the probe from phase 1 to phase 2 (see
// ProbeAudioScene): it fills a deterministic subset of platformChannel[]
// with sentinel pointers that are NEVER dereferenced, because Device::Stop
// is detoured to a recorder for the duration.
void SeedSynthScene(SynthScene* ss, uint32_t seed, RtMemAllocFn memAlloc,
                    bool fakeChannels = false) {
    Rng rng(seed);
    memset(ss, 0, sizeof(*ss));

    ss->scene.time           = (int64_t)rng() << 8;
    ss->scene.playGeneration = (int32_t)(rng() & 0x7fff) + 1;
    ss->scene.channels._data     = ss->channels;
    ss->scene.channels._size     = 256;
    ss->scene.channels._capacity = 256;
    ss->scene.effects._data      = ss->effects;
    ss->scene.effects._size      = 4096;
    ss->scene.effects._capacity  = 4096;
    for (int i = 0; i < 5; ++i) ss->scene.paused[i] = (uint8_t)(rng() & 1);

    uint8_t aliasWanted[256] = {0};
    for (int i = 0; i < 256; ++i) {
        RtAudio::Channel& ch = ss->channels[i];
        uint32_t r = rng();
        uint32_t gen = (rng() & 0x7fffff);
        switch (r & 7) {
            case 0:
            case 1:
                ch.handleTag = 0;                       // dead slot
                break;
            case 2:
                // ALIASED: this channel stores a handle belonging to a
                // DIFFERENT channel. It is the one case that separates a port
                // which works on channel `i` from one that works on channel
                // `(tag>>1)&0xff` -- which is what StopAll and Pause actually
                // do. Filled in by the second pass below, because the tag has
                // to be COPIED from the other channel: a synthesised one with
                // this channel's own generation would fail the 0xfffffe00
                // guard and the case would never fire. (It did not, for one
                // whole run -- the negative control caught it: a StopAll
                // mutated to work on `i` instead of `idx` passed 264 cases.)
                ch.handleTag = 0;
                aliasWanted[i] = 1;
                break;
            case 3:
                // Generation that will NOT validate against whatever channel
                // the index bits land on -- exercises the 0xfffffe00 guard.
                ch.handleTag = ((gen ^ 0x5a5a5au) << 9) | (uint32_t)((i & 0xff) << 1);
                break;
            default:
                ch.handleTag = (gen << 9) | (uint32_t)((i & 0xff) << 1);
                break;
        }
        // soundType deliberately ranges past the 5 real types: the loops filter
        // on it with a plain !=, and a port that masked or clamped it would
        // pass a 0..4-only test.
        ch.soundType = rng() % 7;
        ch.startTime = (int64_t)rng();
        ch.volume    = (float)(rng() & 0xffff) / 65535.0f;
        ch.pitch     = (float)(rng() & 0xffff) / 65535.0f;
        ch.started   = (uint8_t)(rng() & 1);
        for (int b = 0; b < 8; ++b) {
            uint32_t pick = rng();
            ch.platformChannel[b] =
                (fakeChannels && (pick & 3) != 0)
                    ? (void*)(uintptr_t)(kFakeChannelBase + (uint64_t)i * 0x400 + (uint64_t)b * 0x50)
                    : nullptr;                      // phase 1: THE INVARIANT.
            ch.channelFlags[b]    = (uint8_t)(rng() & 0xff);
        }
        ch.soundEffectRef = ((uint64_t)rng() << 32) | rng();
    }

    // Second pass: give every channel marked ALIASED the verbatim handle of
    // another channel that has one, so the handle validates against the OTHER
    // channel and the two candidate implementations diverge.
    for (int i = 0; i < 256; ++i) {
        if (!aliasWanted[i]) continue;
        for (int k = 1; k < 256; ++k) {
            int j = (i + 37 * k) & 0xff;
            if (j != i && ss->channels[j].handleTag != 0 && !aliasWanted[j]) {
                ss->channels[i].handleTag = ss->channels[j].handleTag;
                break;
            }
        }
    }

    // Only the first 64 EffectSlots are given content; StopEffect3D indexes one
    // slot per call and seeding 4096 would cost 4096 allocations for nothing.
    for (int i = 0; i < 64; ++i) {
        RtAudio::EffectSlot& es = ss->effects[i];
        uint32_t r = rng();
        uint32_t gen = rng() & 0x7ffff;
        es.idTag = (gen << 13) | (uint32_t)((i & 0xfff) << 1) | 1u;
        if ((r & 3) == 0) es.idTag = 0;
        es.soundType      = rng() % 7;
        es.soundEffectRef = ((uint64_t)rng() << 32) | rng();
        es.startTime      = (int64_t)rng();
        es.baseVolume     = (float)(rng() & 0xffff) / 65535.0f;
        es.posX = (float)(int32_t)rng() / 4096.0f;
        es.posY = (float)(int32_t)rng() / 4096.0f;
        es.posZ = (float)(int32_t)rng() / 4096.0f;
        // Half the slots point at a live channel, so the StopEffect3D ->
        // StopEffect edge is taken; the other half take the skip.
        es.channelHandle = (r & 4) ? ss->channels[(i * 3) & 0xff].handleTag : 0;

        int n = (int)(r % 4);      // 0..3 emitter instances
        if (n > 0 && memAlloc) {
            es.positions._data =
                (RtAudio::EmitterInstance*)memAlloc(sizeof(RtAudio::EmitterInstance) * (uint64_t)n, 4);
            es.positions._size = n;
            es.positions._capacity = n;
            for (int k = 0; k < n; ++k) {
                es.positions._data[k].x = (float)(int32_t)rng() / 4096.0f;
                es.positions._data[k].y = (float)(int32_t)rng() / 4096.0f;
                es.positions._data[k].z = (float)(int32_t)rng() / 4096.0f;
                es.positions._data[k].volume = (float)(rng() & 0xffff) / 65535.0f;
            }
        }
    }
}

// Field-wise comparison. Returns the number of differing fields and writes the
// first difference into `detail`.
int CompareSynthScenes(const SynthScene* a, const SynthScene* b, char* detail, size_t detailSize) {
    int diffs = 0;
    detail[0] = '\0';
    auto note = [&](const char* what, int index, unsigned long long x, unsigned long long y) {
        ++diffs;
        if (detail[0] == '\0')
            snprintf(detail, detailSize, "%s[%d] ours=0x%llx theirs=0x%llx", what, index, x, y);
    };

    for (int i = 0; i < 5; ++i)
        if (a->scene.paused[i] != b->scene.paused[i])
            note("paused", i, a->scene.paused[i], b->scene.paused[i]);

    for (int i = 0; i < 256; ++i) {
        const RtAudio::Channel& x = a->channels[i];
        const RtAudio::Channel& y = b->channels[i];
        if (x.soundEffectRef != y.soundEffectRef) note("ch.soundEffectRef", i, x.soundEffectRef, y.soundEffectRef);
        if (x.soundType != y.soundType)           note("ch.soundType", i, x.soundType, y.soundType);
        if (x.startTime != y.startTime)           note("ch.startTime", i, (unsigned long long)x.startTime, (unsigned long long)y.startTime);
        if (memcmp(&x.volume, &y.volume, 4) != 0) note("ch.volume", i, 0, 0);
        if (memcmp(&x.pitch, &y.pitch, 4) != 0)   note("ch.pitch", i, 0, 0);
        if (x.handleTag != y.handleTag)           note("ch.handleTag", i, x.handleTag, y.handleTag);
        if (x.started != y.started)               note("ch.started", i, x.started, y.started);
        for (int b2 = 0; b2 < 8; ++b2) {
            if (x.platformChannel[b2] != y.platformChannel[b2])
                note("ch.platformChannel", i * 8 + b2,
                     (unsigned long long)(uintptr_t)x.platformChannel[b2],
                     (unsigned long long)(uintptr_t)y.platformChannel[b2]);
            if (x.channelFlags[b2] != y.channelFlags[b2])
                note("ch.channelFlags", i * 8 + b2, x.channelFlags[b2], y.channelFlags[b2]);
        }
        // _pad0c and _pad6d[3] are NOT compared -- retail writes caller stack
        // garbage there. See the note at the top of this section.
    }

    for (int i = 0; i < 64; ++i) {
        const RtAudio::EffectSlot& x = a->effects[i];
        const RtAudio::EffectSlot& y = b->effects[i];
        if (memcmp(&x.posX, &y.posX, 12) != 0) note("es.pos", i, 0, 0);
        if (x.soundType != y.soundType)         note("es.soundType", i, x.soundType, y.soundType);
        if (x.soundEffectRef != y.soundEffectRef) note("es.soundEffectRef", i, x.soundEffectRef, y.soundEffectRef);
        if (x.startTime != y.startTime)         note("es.startTime", i, (unsigned long long)x.startTime, (unsigned long long)y.startTime);
        if (x.channelHandle != y.channelHandle) note("es.channelHandle", i, x.channelHandle, y.channelHandle);
        if (x.idTag != y.idTag)                 note("es.idTag", i, x.idTag, y.idTag);
        if (memcmp(&x.baseVolume, &y.baseVolume, 4) != 0) note("es.baseVolume", i, 0, 0);
        if (x.positions._size != y.positions._size)
            note("es.positions._size", i, (unsigned)x.positions._size, (unsigned)y.positions._size);
        if (x.positions._capacity != y.positions._capacity)
            note("es.positions._capacity", i, (unsigned)x.positions._capacity, (unsigned)y.positions._capacity);
        // The buffers themselves are separate allocations, so only null-ness is
        // comparable; the CONTENTS are compared when both are live.
        if ((x.positions._data == nullptr) != (y.positions._data == nullptr))
            note("es.positions._data null-ness", i,
                 (unsigned long long)(uintptr_t)x.positions._data,
                 (unsigned long long)(uintptr_t)y.positions._data);
        else if (x.positions._data && y.positions._data && x.positions._size == y.positions._size &&
                 memcmp(x.positions._data, y.positions._data,
                        sizeof(RtAudio::EmitterInstance) * (size_t)x.positions._size) != 0)
            note("es.positions contents", i, 0, 0);
    }
    return diffs;
}

// The device must remain unreachable for the whole probe. If any platform
// channel is ever non-null the probe is UNSOUND, not merely failing, so it
// says so in those words rather than reporting a mismatch.
bool PlatformChannelsAllNull(const SynthScene* ss) {
    for (int i = 0; i < 256; ++i)
        for (int b = 0; b < 8; ++b)
            if (ss->channels[i].platformChannel[b]) return false;
    return true;
}

// Phase 2's weaker invariant: a platform channel may be non-null, but only if
// it is one of OUR sentinels. A pointer from anywhere else would mean the
// scene had acquired a real channel and the recorder is no longer sufficient.
bool PlatformChannelsAllSentinel(const SynthScene* ss) {
    for (int i = 0; i < 256; ++i)
        for (int b = 0; b < 8; ++b) {
            uint64_t v = (uint64_t)(uintptr_t)ss->channels[i].platformChannel[b];
            if (v != 0 && (v < kFakeChannelBase || v >= kFakeChannelBase + 0x100000ull))
                return false;
        }
    return true;
}

// ---------------------------------------------------------------------------
// UI::GraphElement::AddGraph  —  ?AddGraph@GraphElement@UI@@QEAAXI@Z
// retail 0x180040620.                                   (Wave 9 lane UIW)
//
// WHY IT IS HERE. Lane UIW ported it, and no unattended harness path reaches
// it: its only caller in this tree is `code/Game/Dialog/townhallui.cpp:1060`,
// which needs a built Town Hall clicked in the world. Both of the lane's diff
// runs report `calls=0` for it, which is exactly the line
// `reference_uncalled_port_is_not_verified.md` says reads as a pass and is not
// one. So it is probed against the shipped body instead.
//
// A SYNTHETIC SUBJECT, NOT A FAKE. `AddGraph` reads and writes ONE field of the
// object — the `Collection::Array<GraphSeries>` header at `this+0xF0` — and
// touches nothing else. That is read out of the body, not assumed: the only
// base-relative accesses in 0x180040620..0x18004068b are `[RCX+0xF8]` (the
// size) and `LEA RBX,[RCX+0xF0]`. So a zeroed buffer with a valid Array header
// at +0xF0 IS a GraphElement as far as this function can tell, and two of them
// are two independent subjects — which is what makes ours-and-theirs safe here
// when a replay on ONE object would merely append twice
// (reference_synthetic_subject_beats_a_fake.md).
//
// WHAT IS COMPARED, AND WHAT MUST NOT BE:
//   * size and capacity of the outer array — capacity is the real check, since
//     it exercises the 1 -> 6 -> 21 growth treble against the shipped rule.
//   * every element's colour dword at +0x00, and its nested point-array header
//     at +0x08 (which AddGraph leaves empty, so {null,0,0} on both sides).
//   * NOT the outer `_data` POINTER: two separate allocations, guaranteed to
//     differ, and comparing it would fail 100% of runs.
//   * NOT bytes +0x04..+0x07 of any element: NEITHER SIDE WRITES THEM. They
//     hold whatever the allocator returned. Comparing them is the
//     `reference_oracle_must_not_compare_unwritten_bytes.md` mistake that cost
//     Wave 6 four false alarms, and here it would fire on almost every run.
//
// TWO CONTROLS, because a green comparison proves nothing without them
// (reference_probe_needs_an_effectiveness_control.md):
//   * EFFECTIVENESS: count how many of the appends actually grew the array and
//     how many of those REALLOCATED. If the realloc count is zero the growth
//     rule was never exercised and the pass is thin; it is printed either way.
//   * NEGATIVE: after the comparisons pass, corrupt one colour in OUR object
//     and re-run the comparator. If it still says equal, the comparator is
//     broken and every pass above it is worthless.
void ProbeGraphElementAddGraph() {
    static const char* const kSubject = "UI::GraphElement::AddGraph";
    static const char* const kName    = "?AddGraph@GraphElement@UI@@QEAAXI@Z";
    static const char* const kClear   = "?RemoveGraphs@GraphElement@UI@@QEAAXXZ";

    using AddGraphFn = void (*)(void*, uint32_t);
    using ClearFn    = void (*)(void*);

    AddGraphFn ours   = reinterpret_cast<AddGraphFn>(ResolveExport(kName));
    AddGraphFn theirs = reinterpret_cast<AddGraphFn>(ResolveExportRaw(kName));
    ClearFn    clear  = reinterpret_cast<ClearFn>(ResolveExportRaw(kClear));

    if (!ours || !theirs || ours == theirs) {
        // ours == theirs means the port is not registered and BOTH doors landed
        // on the DLL: comparing a function with itself is a guaranteed pass and
        // a lie. Refuse rather than report one.
        fprintf(stderr, "[RtOrphan] SKIP %s (ours=%p theirs=%p clear=%p)\n",
                kSubject, (void*)ours, (void*)theirs, (void*)clear);
        return;
    }

    // 0x120 bytes: past +0xF8, with room to spare. Zeroed, so the Array header
    // at +0xF0 starts {data=null, size=0, capacity=0} — a legitimately empty
    // Collection::Array, which is what a freshly constructed GraphElement has.
    alignas(16) uint8_t oursObj[0x120];
    alignas(16) uint8_t theirsObj[0x120];
    memset(oursObj, 0, sizeof(oursObj));
    memset(theirsObj, 0, sizeof(theirsObj));

    // Distinct, and deliberately including the two values a byte-copy bug is
    // most likely to survive (0 and 0xffffffff) plus six that differ in every
    // byte. A uniform population is not a verification
    // (reference_uniform_population_is_not_a_verification.md) — 13 of 14 equal
    // values once let a broken strcmp pass four waves of oracles.
    static const uint32_t kColours[] = {
        0x00000000u, 0xffffffffu, 0xdeadbeefu, 0x00ff00ffu,
        0x12345678u, 0x87654321u, 0x0000ff00u, 0xa5a5a5a5u,
    };
    const int kN = (int)(sizeof(kColours) / sizeof(kColours[0]));

    auto arrOf = [](uint8_t* o) { return o + 0xF0; };
    auto dataOf = [&](uint8_t* o) { return *reinterpret_cast<uint8_t**>(arrOf(o)); };
    auto sizeOf = [&](uint8_t* o) { return *reinterpret_cast<int32_t*>(arrOf(o) + 8); };
    auto capOf  = [&](uint8_t* o) { return *reinterpret_cast<int32_t*>(arrOf(o) + 12); };

    // The comparator, as a lambda so the negative control drives the SAME code
    // the positive cases did — a control that re-implements the check proves
    // nothing about the check (reference_recheck_must_match_its_own_proof.md).
    auto compare = [&](char* detail, size_t detailN) -> bool {
        if (sizeOf(oursObj) != sizeOf(theirsObj) || capOf(oursObj) != capOf(theirsObj)) {
            snprintf(detail, detailN, "header ours={size=%d cap=%d} theirs={size=%d cap=%d}",
                     sizeOf(oursObj), capOf(oursObj), sizeOf(theirsObj), capOf(theirsObj));
            return false;
        }
        const int32_t n = sizeOf(oursObj);
        const uint8_t* a = dataOf(oursObj);
        const uint8_t* b = dataOf(theirsObj);
        if ((a == nullptr) != (b == nullptr)) {
            snprintf(detail, detailN, "one buffer is null: ours=%p theirs=%p", (void*)a, (void*)b);
            return false;
        }
        for (int32_t i = 0; i < n; ++i) {
            const uint8_t* ea = a + (size_t)i * 0x18;
            const uint8_t* eb = b + (size_t)i * 0x18;
            const uint32_t ca = *reinterpret_cast<const uint32_t*>(ea);
            const uint32_t cb = *reinterpret_cast<const uint32_t*>(eb);
            if (ca != cb) {
                snprintf(detail, detailN, "elem %d colour ours=0x%08x theirs=0x%08x", i, ca, cb);
                return false;
            }
            // +0x04..+0x07 SKIPPED — unwritten by both. See the banner.
            // The nested point Array at +0x08: compare size and capacity, and
            // that both data pointers are null (AddGraph appends an EMPTY
            // series). Comparing the pointers themselves would be wrong for the
            // same reason as the outer one; comparing them against null is not.
            const void* pa = *reinterpret_cast<void* const*>(ea + 0x08);
            const void* pb = *reinterpret_cast<void* const*>(eb + 0x08);
            const int32_t sa = *reinterpret_cast<const int32_t*>(ea + 0x10);
            const int32_t sb = *reinterpret_cast<const int32_t*>(eb + 0x10);
            const int32_t ka = *reinterpret_cast<const int32_t*>(ea + 0x14);
            const int32_t kb = *reinterpret_cast<const int32_t*>(eb + 0x14);
            if ((pa == nullptr) != (pb == nullptr) || sa != sb || ka != kb) {
                snprintf(detail, detailN,
                         "elem %d points ours={%p,%d,%d} theirs={%p,%d,%d}",
                         i, pa, sa, ka, pb, sb, kb);
                return false;
            }
            // CONTENTS of the nested point array, when there are any. This is
            // the only thing that exercises `Array<int32>::operator=`'s copy
            // loop with n > 0 — the element-assign arm of kSpec_GraphSeries,
            // which every append of an EMPTY series leaves untouched. See the
            // point-seeding below for why the arrays are not empty.
            if (sa > 0) {
                if (memcmp(pa, pb, (size_t)sa * 4) != 0) {
                    snprintf(detail, detailN,
                             "elem %d point CONTENTS differ (%d int32)", i, sa);
                    return false;
                }
            }
        }
        return true;
    };

    // POINT SEEDING — and this is not decoration, it is what makes the probe
    // cover the element assign at all.
    //
    // The first version of this probe appended eight EMPTY series, and every
    // series' nested point array stayed {null,0,0}. That meant
    // `Array<int32>::operator=` — the assign arm of `kSpec_GraphSeries`, and a
    // whole ported body of its own — ran only on its trivial path: it destroyed
    // an empty destination and copied zero elements, on all three reallocs. The
    // copy LOOP was never entered. The probe passed anyway, which is precisely
    // the effectiveness-control failure `reference_probe_needs_an_effectiveness_
    // control.md` describes: green comparisons where nothing did the work.
    //
    // So each new series is given a few points before the next append. The
    // NEXT append's realloc then has to copy non-empty nested arrays forward,
    // and the comparator checks their contents. The shipped `AddGraphPoint` is
    // used on BOTH objects: it is not the subject here, and using our port on
    // one side would make a difference in IT look like a difference in AddGraph.
    using AddPointFn = void (*)(void*, int32_t, int32_t);
    AddPointFn addPoint = reinterpret_cast<AddPointFn>(
        ResolveExportRaw("?AddGraphPoint@GraphElement@UI@@QEAAXHH@Z"));

    int grew = 0, realloced = 0, mismatches = 0, seeded = 0, copiedNonEmpty = 0;
    for (int i = 0; i < kN; ++i) {
        const uint8_t* before = dataOf(oursObj);
        const int32_t  beforeSize = sizeOf(oursObj);

        // How many of the existing series carry points that this append, if it
        // reallocates, will have to copy forward. Counted BEFORE the call.
        int nonEmptyBefore = 0;
        for (int32_t k = 0; k < beforeSize; ++k) {
            const uint8_t* e = dataOf(oursObj) + (size_t)k * 0x18;
            if (*reinterpret_cast<const int32_t*>(e + 0x10) > 0) ++nonEmptyBefore;
        }

        ours(oursObj, kColours[i]);
        theirs(theirsObj, kColours[i]);

        if (sizeOf(oursObj) != beforeSize) ++grew;
        if (dataOf(oursObj) != before) {
            ++realloced;
            copiedNonEmpty += nonEmptyBefore;
        }

        // Seed the series just appended, so the NEXT realloc has real data to
        // copy. Three points, distinct across series and across positions.
        if (addPoint && sizeOf(oursObj) > 0) {
            const int32_t g = sizeOf(oursObj) - 1;
            for (int p = 0; p < 3; ++p) {
                const int32_t v = 0x1000 * (i + 1) + p;
                addPoint(oursObj,   g, v);
                addPoint(theirsObj, g, v);
            }
            ++seeded;
        }

        char detail[192];
        if (!compare(detail, sizeof(detail))) {
            char full[256];
            snprintf(full, sizeof(full), "after append %d (colour 0x%08x): %s",
                     i, kColours[i], detail);
            Report(kSubject, "append", false, full);
            ++mismatches;
            break;                       // one report is enough; state is now suspect
        }
    }

    // NEGATIVE CONTROL. Break ours on purpose and demand that the comparator
    // notice. If this "passes", every green line above it is meaningless.
    // TWO of them, one per thing the comparator checks: the colour (the outer
    // element) and a POINT VALUE (the nested array's contents). One control
    // over one field would leave the other check unproven, which is how a
    // comparator ends up passing on a term it never evaluates
    // (reference_shadow_predicate_must_name_its_failing_term.md).
    bool negColour = false, negPoint = false;
    char detail[192];
    if (mismatches == 0 && sizeOf(oursObj) > 0 && dataOf(oursObj)) {
        uint8_t* last = dataOf(oursObj) + (size_t)(sizeOf(oursObj) - 1) * 0x18;

        const uint32_t saveC = *reinterpret_cast<uint32_t*>(last);
        *reinterpret_cast<uint32_t*>(last) = ~saveC;
        negColour = !compare(detail, sizeof(detail));
        *reinterpret_cast<uint32_t*>(last) = saveC;
        if (!negColour)
            Report(kSubject, "negative-control-colour", false,
                   "the comparator did NOT notice a corrupted colour");

        int32_t* pts = *reinterpret_cast<int32_t**>(last + 0x08);
        const int32_t nPts = *reinterpret_cast<int32_t*>(last + 0x10);
        if (pts && nPts > 0) {
            const int32_t saveP = pts[nPts - 1];
            pts[nPts - 1] = ~saveP;
            negPoint = !compare(detail, sizeof(detail));
            pts[nPts - 1] = saveP;
            if (!negPoint)
                Report(kSubject, "negative-control-point", false,
                       "the comparator did NOT notice a corrupted POINT value");
        } else {
            // No points means the contents check was never exercised and there
            // is nothing to control. Say so rather than passing vacuously.
            Report(kSubject, "negative-control-point", false,
                   "no points on the last series — the contents check is UNCONTROLLED");
        }
    }
    const bool negativeCaught = negColour && negPoint;

    if (mismatches == 0 && negativeCaught) {
        ++g_pass;
        fprintf(stderr,
                "[RtOrphan] pass %s %d appends, %d grew, %d REALLOCATED "
                "(final size=%d cap=%d), %d series seeded with points, "
                "%d NON-EMPTY nested arrays copied through a realloc, "
                "negative control CAUGHT\n",
                kSubject, kN, grew, realloced, sizeOf(oursObj), capOf(oursObj),
                seeded, copiedNonEmpty);
        // The number that matters for coverage is the last one. If
        // `copiedNonEmpty` is 0 the element assign's copy loop never ran and
        // this pass says nothing about `Array<int32>::operator=` — which is
        // exactly the state the probe's first version was silently in.
        if (copiedNonEmpty == 0)
            fprintf(stderr, "[RtOrphan] WARN %s: 0 non-empty nested arrays were "
                            "copied — the element assign's copy loop is UNCOVERED\n",
                    kSubject);
    }

    // Give the two buffers back. The shipped RemoveGraphs is used for BOTH:
    // this is teardown, not the thing under test, and driving our own port here
    // would put a second registered body in the middle of the probe's own
    // cleanup.
    if (clear) { clear(oursObj); clear(theirsObj); }
    fflush(stderr);
}

void ProbeAudioScene() {
    struct Fn { const char* name; const char* subject; };
    static const Fn kFns[] = {
        { "?Pause@Scene@Audio@@QEAAXH_N@Z",        "Audio::Scene::Pause"        },
        { "?PauseEffect@Scene@Audio@@QEAAXI@Z",    "Audio::Scene::PauseEffect"  },
        { "?StopEffect@Scene@Audio@@QEAAXI@Z",     "Audio::Scene::StopEffect"   },
        { "?StopEffect3D@Scene@Audio@@QEAAXI@Z",   "Audio::Scene::StopEffect3D" },
        { "?StopAll@Scene@Audio@@QEAAXXZ",         "Audio::Scene::StopAll"      },
    };
    void* ours[5];
    void* theirs[5];
    for (int f = 0; f < 5; ++f) {
        ours[f]   = ResolveExport(kFns[f].name);
        theirs[f] = ResolveExportRaw(kFns[f].name);
        if (!ours[f] || !theirs[f] || ours[f] == theirs[f]) {
            fprintf(stderr, "[RtOrphan] FAIL %s: ours=%p theirs=%p%s\n", kFns[f].subject,
                    ours[f], theirs[f],
                    (ours[f] && ours[f] == theirs[f]) ? " (port NOT registered)" : "");
            ++g_fail;
            return;
        }
    }
    auto memAlloc = reinterpret_cast<RtMemAllocFn>(
        ResolveExportRaw("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z"));

    SynthScene* A = (SynthScene*)malloc(sizeof(SynthScene));
    SynthScene* B = (SynthScene*)malloc(sizeof(SynthScene));
    // C is the UNTOUCHED control. Without it a case in which BOTH sides did
    // nothing -- every Pause hitting its early-return, every handle failing the
    // guard -- reports "identical" and reads exactly like a verified one. That
    // is the vacuous pass this whole file exists to refuse
    // (reference_uncalled_port_is_not_verified.md) one level down: not a port
    // with no caller, but a caller that reaches no code. It EARNED ITS KEEP on
    // the first run: phase 1 gave PauseEffect 64 green cases of which 0 changed
    // anything, because with null platform channels PauseEffect cannot do
    // anything. Phase 2 exists because of that line.
    SynthScene* C = (SynthScene*)malloc(sizeof(SynthScene));
    if (!A || !B || !C) {
        fprintf(stderr, "[RtOrphan] SKIP Audio::Scene: out of memory\n");
        free(A); free(B); free(C);
        return;
    }

    int comparisons = 0, mismatches = 0, unsound = 0, effective = 0;
    int effectivePerFn[5] = {0, 0, 0, 0, 0};
    int stopCalls = 0;
    bool fakeChannels = false;      // phase selector, read by runCase

    // A case is (seed, which function, argument). Both sides are re-seeded from
    // `seed` before every case, so cases never contaminate each other.
    auto runCase = [&](uint32_t seed, int f, uint32_t arg, int32_t argB, const char* label) {
        SeedSynthScene(A, seed, memAlloc, fakeChannels);
        SeedSynthScene(B, seed, memAlloc, fakeChannels);
        SeedSynthScene(C, seed, memAlloc, fakeChannels);
        char pre[160];
        if (CompareSynthScenes(A, B, pre, sizeof(pre)) != 0) {
            fprintf(stderr, "[RtOrphan] UNSOUND Audio::Scene seeding: %s\n", pre);
            ++unsound;
            return;
        }
        g_audioStopCount[0] = g_audioStopCount[1] = 0;
        g_audioStopSide = fakeChannels ? 0 : -1;
        switch (f) {
            case 0:
                reinterpret_cast<ScenePauseFn>(ours[0])(&A->scene, argB, arg != 0);
                if (fakeChannels) g_audioStopSide = 1;
                reinterpret_cast<ScenePauseFn>(theirs[0])(&B->scene, argB, arg != 0);
                break;
            case 1: case 2: case 3:
                reinterpret_cast<SceneHandleFn>(ours[f])(&A->scene, arg);
                if (fakeChannels) g_audioStopSide = 1;
                reinterpret_cast<SceneHandleFn>(theirs[f])(&B->scene, arg);
                break;
            case 4:
                reinterpret_cast<SceneStopAllFn>(ours[4])(&A->scene);
                if (fakeChannels) g_audioStopSide = 1;
                reinterpret_cast<SceneStopAllFn>(theirs[4])(&B->scene);
                break;
        }
        g_audioStopSide = -1;
        ++comparisons;

        bool invariantOk = fakeChannels ? (PlatformChannelsAllSentinel(A) && PlatformChannelsAllSentinel(B))
                                        : (PlatformChannelsAllNull(A) && PlatformChannelsAllNull(B));
        if (!invariantOk) {
            fprintf(stderr, "[RtOrphan] UNSOUND %s [%s]: a platform channel outside the "
                            "expected set appeared, so the Device was reachable after all\n",
                    kFns[f].subject, label);
            ++unsound;
            return;
        }

        // Phase 2 also compares the DEVICE CALL SEQUENCE. This is the half of
        // these bodies that state comparison cannot see: StopEffect does not
        // null the pointers it releases (the record is overwritten anyway), so
        // a port that released the WRONG channel, or none, would leave an
        // identical scene behind.
        if (fakeChannels) {
            if (g_audioStopCount[0] != g_audioStopCount[1]) {
                ++mismatches;
                fprintf(stderr, "[RtOrphan] FAIL %s [%s]: Device::Stop called %d times by "
                                "ours, %d by theirs\n", kFns[f].subject, label,
                        g_audioStopCount[0], g_audioStopCount[1]);
                return;
            }
            int n = g_audioStopCount[0];
            if (n > 2048) n = 2048;
            for (int k = 0; k < n; ++k) {
                if (g_audioStopLog[0][k].channel != g_audioStopLog[1][k].channel ||
                    g_audioStopLog[0][k].device  != g_audioStopLog[1][k].device) {
                    ++mismatches;
                    fprintf(stderr, "[RtOrphan] FAIL %s [%s]: Device::Stop #%d ours=(%p,%p) "
                                    "theirs=(%p,%p)\n", kFns[f].subject, label, k,
                            g_audioStopLog[0][k].device, g_audioStopLog[0][k].channel,
                            g_audioStopLog[1][k].device, g_audioStopLog[1][k].channel);
                    return;
                }
            }
            stopCalls += n;
        }

        char detail[160];
        int d = CompareSynthScenes(A, B, detail, sizeof(detail));
        if (d != 0) {
            ++mismatches;
            fprintf(stderr, "[RtOrphan] FAIL %s [%s]: %d differing fields, first: %s\n",
                    kFns[f].subject, label, d, detail);
            return;
        }
        // Agreement is only worth something if there was something to agree
        // ABOUT. Count the cases in which the scene actually moved.
        char ctl[160];
        if (CompareSynthScenes(A, C, ctl, sizeof(ctl)) != 0 ||
            (fakeChannels && g_audioStopCount[0] > 0)) {
            ++effective;
            ++effectivePerFn[f];
        }
    };

    // The case sweep, run once per phase. `fns` selects which of the five are
    // eligible: phase 2 excludes StopAll, and the exclusion is not a shortcut.
    // Retail's StopAll does not CALL Device::Stop, it INLINES it (0x1800df1eb
    // .. 0x1800df289), so the detour would catch our call and miss theirs and
    // the two sides would be asymmetric by construction -- the same shape as
    // the longjmp-unwind trap in WAVE8_5.md §8, where the harness manufactured
    // its own divergence. StopAll keeps null channels and phase 1's verdict.
    auto sweep = [&](bool doStopAll) {
        for (uint32_t seed = 1; seed <= 8; ++seed) {
            for (int32_t type = 0; type <= 4; ++type) {
                runCase(seed, 0, 1, type, "pause=true");
                runCase(seed, 0, 0, type, "pause=false");
            }

            SeedSynthScene(A, seed, memAlloc, fakeChannels);   // read the seeded tags back
            uint32_t handles[8];
            handles[0] = 0;                                   // the zero handle
            handles[1] = 0xffffffffu;                         // never validates
            handles[2] = A->channels[3].handleTag;
            handles[3] = A->channels[17].handleTag;
            handles[4] = A->channels[200].handleTag;
            handles[5] = A->channels[17].handleTag ^ 0x200u;  // one generation bit off
            handles[6] = A->channels[17].handleTag ^ 0x2u;    // one INDEX bit off (not masked)
            handles[7] = A->channels[17].handleTag | 1u;      // bit 0 set (never compared)
            for (int h = 0; h < 8; ++h) {
                char lbl[48];
                snprintf(lbl, sizeof(lbl), "handle#%d=0x%08x", h, handles[h]);
                runCase(seed, 1, handles[h], 0, lbl);
                runCase(seed, 2, handles[h], 0, lbl);
            }

            uint32_t ids[6];
            ids[0] = 0;
            ids[1] = A->effects[0].idTag;
            ids[2] = A->effects[5].idTag;
            ids[3] = A->effects[13].idTag;
            ids[4] = A->effects[13].idTag ^ 0x2000u;   // one generation bit off
            ids[5] = A->effects[13].idTag ^ 0x2u;      // one index bit off -- retail's
                                                       // guard does NOT cover it, so
                                                       // this must still act
            for (int k = 0; k < 6; ++k) {
                char lbl[48];
                snprintf(lbl, sizeof(lbl), "id#%d=0x%08x", k, ids[k]);
                runCase(seed, 3, ids[k], 0, lbl);
            }

            if (doStopAll) runCase(seed, 4, 0, 0, "whole table");
        }
    };

    // ---- Phase 1: null platform channels, device unreachable by construction.
    fakeChannels = false;
    sweep(/*doStopAll=*/true);
    int phase1 = comparisons;

    // ---- Phase 2: sentinel platform channels + Device::Stop detoured to a
    //      recorder. This is the "device-level fake" WAVE8_5.md §12 item 2
    //      asked for, and it turned out to be one detour and a two-element
    //      struct rather than a Device model -- because the only thing the
    //      Scene-level family asks of the Device is Stop(channel).
    if (InstallRuntimeDetourExport("probe:Audio::Device::Stop",
                                   "?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z",
                                   reinterpret_cast<void*>(&DetouredAudioDeviceStop),
                                   &g_audioStopDetour)) {
        g_audioStopTramp = reinterpret_cast<DeviceStopFn>(g_audioStopDetour.trampoline);
        fakeChannels = true;
        sweep(/*doStopAll=*/false);
        fakeChannels = false;
        g_audioStopSide = -1;
        RemoveDetour(&g_audioStopDetour);
        g_audioStopTramp = nullptr;
    } else {
        fprintf(stderr, "[RtOrphan] SKIP Audio::Scene phase 2: could not detour "
                        "Audio::Device::Stop -- PauseEffect's release loop stays UNVERIFIED\n");
    }

    free(A);
    free(B);
    free(C);

    if (unsound) {
        fprintf(stderr, "[RtOrphan] Audio::Scene: %d UNSOUND cases -- the probe's own "
                        "premise failed, treat every pass below as void\n", unsound);
        ++g_fail;
        return;
    }
    if (mismatches) {
        ++g_fail;
    } else {
        ++g_pass;
        fprintf(stderr, "[RtOrphan] pass Audio::Scene teardown family: %d comparisons "
                        "(%d phase 1 null-channel, %d phase 2 recorded-device), 0 mismatches, "
                        "%d Device::Stop calls matched pairwise; %d cases CHANGED the scene "
                        "(Pause %d, PauseEffect %d, StopEffect %d, StopEffect3D %d, StopAll %d)\n",
                comparisons, phase1, comparisons - phase1, stopCalls, effective,
                effectivePerFn[0], effectivePerFn[1], effectivePerFn[2],
                effectivePerFn[3], effectivePerFn[4]);
        // A function with zero effective cases was never actually exercised,
        // so its share of the "0 mismatches" is worth nothing. Say so rather
        // than let the headline number cover for it.
        for (int f = 0; f < 5; ++f) {
            if (effectivePerFn[f] == 0)
                fprintf(stderr, "[RtOrphan] WARNING %s: 0 of its cases changed "
                                "the scene -- agreement here is VACUOUS\n",
                        kFns[f].subject);
        }
    }
}

// ============================================================================
// WAVE 12 — Application::Application, the ten rows with NO CALLER in this tree
// ============================================================================
//
// Lanes A and C landed eleven Application rows. A source grep over
// `code/ engine/ runtime/ standalone/ dx11/` (excluding the port directory
// itself) finds a caller for exactly ONE of them —
// `?GetInstance@` from `dx11/src/devicedata.cpp:270`. The other ten are
// reached by nothing:
//
//   ?Get@  ?GetWindow@  ?IsCursorEnabled@  ?PlatformIsPaused@
//   ?PlatformDestruct@  ?Update@  ?Shutdown@  ?Startup@
//   ?EnableCursor@  ?LaunchProcess@
//
// So in any `BANISHED_RT_DIFF=1` run all ten print `calls=0 divergences=0`,
// which is the line `reference_uncalled_port_is_not_verified.md` exists to
// refuse. This section gives each one the strongest oracle its shape admits,
// and says which kind that is.
//
// THREE KINDS OF LINE, NEVER MIXED. Every line this section prints is tagged,
// because the trap here is that all three read the same when you skim:
//
//   [live-diff]   OUR registered body and the SHIPPED DLL body were both
//                 executed on equivalent state and the results compared.
//                 This is the only kind that is an oracle.
//   [structural]  Nothing was executed. Bytes in the DLL (or in our own
//                 compiled body) were decoded and compared against what the
//                 port claims. Real evidence, different claim.
//   [attendance]  A body ran; nothing was compared. Printed only to say so.
//
// AND A WARNING ABOUT `calls=`. `RtDiffReport` (runtime/funcs/rt_diff.cpp:84)
// increments `site->calls` on EVERY report, including one made from here. So
// after this probe runs, the `[RtDiff]` summary shows non-zero `calls=` for
// these rows and THE PROBE IS THE ONLY CALLER. A `calls=` number on an
// Application row in a probe run is not evidence of a gameplay route.
//
// WHAT THE ELEVEN LOOK LIKE IN A DIFF RUN TODAY, audited by this lane:
//   * real comparison against the DLL (RT_DIFF_SCALAR / RT_DIFF_BYTES):
//     Get, GetInstance, GetWindow, IsCursorEnabled, PlatformIsPaused
//     (and the pre-existing IsPaused, Quit). Sound instruments with no caller.
//   * ATTENDANCE ONLY — `RT_DIFF_SHADOW(true, …)`, a literal `true`, so not
//     even a self-check with a predicate; the DLL is never consulted and no
//     term is evaluated: Update, Shutdown, Startup, PlatformDestruct,
//     LaunchProcess (and the pre-existing PlatformUpdate, SleepThread,
//     YieldToThreads). Their `divergences=0` compares nothing at all.
//   * `EnableCursor` carries lane C's PREDICTING oracle (rt_predict.h), which
//     is a genuine oracle — but it is gated on `BANISHED_RT_PREDICT=1` AND on
//     something calling `RtApp_EnableCursor`. Nothing does. See screen 4.
// ============================================================================

using RtApplication::Application;

typedef void*        (*AppPtrFn)   (const Application*);
typedef uint8_t      (*AppByteFn)  (const Application*);   // raw AL, see screen 1
typedef Application* (*AppGetFn)   ();
typedef void         (*AppVoidFn)  (Application*);
typedef void         (*AppCursorFn)(Application*, bool);
typedef bool         (*AppLaunchFn)(Application*, const char*, bool);

// Per-SCREEN negative-control bookkeeping. Deliberately not a union across
// screens: a union reads 3/3 while hiding that one screen caught 1 of 3
// (reference_union_control_hides_a_blind_screen.md).
enum AppScreen { kScrReaders = 0, kScrDestruct, kScrThunkRetail, kScrThunkOurs,
                 kScrGet, kScrCursor, kScrLaunch, kAppScreenCount };
const char* const kAppScreenName[kAppScreenCount] = {
    "S1 readers (live-diff)", "S2 PlatformDestruct (live-diff, negative claim)",
    "S3a thunks: retail jump target (structural)",
    "S3b thunks: our body calls the sibling (structural)",
    "S4 Get / _singleton sweep (live-diff)",
    "S5 EnableCursor two-object (live-diff)",
    "S6 LaunchProcess failure path (live-diff, partial)",
};
int g_appCtlRun[kAppScreenCount]    = {0};
int g_appCtlCaught[kAppScreenCount] = {0};

void AppControl(AppScreen s, bool caught, const char* what) {
    ++g_appCtlRun[s];
    if (caught) ++g_appCtlCaught[s];
    else
        fprintf(stderr, "[RtOrphan][App] CONTROL NOT CAUGHT on %s: %s — every "
                        "green line on this screen is worthless\n",
                kAppScreenName[s], what);
}

// Resolve both doors for one export. `ours == theirs` means the port is not
// registered and both doors landed on the DLL, so any comparison would be the
// DLL against itself and a guaranteed vacuous pass
// (reference_resolveaddr_consults_port_registry_first.md).
bool AppPair(const char* name, const char* subject, void** ours, void** theirs) {
    *ours   = ResolveExport(name);
    *theirs = ResolveExportRaw(name);
    if (!*ours || !*theirs) {
        fprintf(stderr, "[RtOrphan][App] SKIP %s: ours=%p theirs=%p\n",
                subject, *ours, *theirs);
        ++g_fail;
        return false;
    }
    if (*ours == *theirs) {
        fprintf(stderr, "[RtOrphan][App] FAIL %s: port NOT registered (both doors "
                        "returned %p) — a comparison here would be the DLL against "
                        "itself\n", subject, *ours);
        ++g_fail;
        return false;
    }
    return true;
}

// A synthetic Application, plus a guard band. The guard band is not padding:
// screen 2's claim is that PlatformDestruct writes NOTHING, and "nothing"
// has to include bytes past the struct this lane's header describes.
struct AppSubject {
    alignas(16) uint8_t bytes[0x100];
    Application* app() { return reinterpret_cast<Application*>(bytes); }
};

// ---------------------------------------------------------------------------
// SCREEN 1 — the five pure field readers, swept.
//
//   ?GetWindow@         MOV RAX,[RCX]          (+0x00)
//   ?GetInstance@       MOV RAX,[RCX+0x8]      (+0x08)
//   ?IsCursorEnabled@   MOVZX EAX,byte [RCX+0x10]
//   ?PlatformIsPaused@  XOR EAX,EAX / CMP byte [RCX+0x11],AL / SETE AL
//   ?IsPaused@          byte-identical to PlatformIsPaused (already ported)
//
// All five disassemblies above were re-read from the shipped DLL by this lane
// with objdump, not taken from the lane docs.
//
// WHY THE BOOL RETURNS ARE TYPED `uint8_t` HERE AND NOT `bool`. Retail's
// `IsCursorEnabled` is a MOVZX of the raw byte: for a stored byte of 2 it
// returns 2 in AL. Our port returns `(_cursorEnabled != 0)`, which GCC
// normalises to 1. Comparing through `bool` would coerce both to `true` and
// hide the difference; comparing the raw AL byte is what a caller that does
// `movzx eax,al; cmp eax,1` would see. The sweep therefore includes bytes
// 2, 0x7f, 0x80 and 0xff on purpose, and the report distinguishes a
// BOOL-EQUIVALENT difference (both non-zero) from a truth-value one. A
// uniform 0/1 population would have proved nothing here
// (reference_uniform_population_is_not_a_verification.md).
//
// PROBED ON A SYNTHETIC OBJECT, NOT THE LIVE SINGLETON. These five only read
// `[this+0x00..0x11]`, so a buffer with those bytes set IS an Application as
// far as they can tell (reference_synthetic_subject_beats_a_fake.md) — and a
// synthetic one can be swept over values the live object never holds.
//
// THE NEGATIVE CONTROL is a stand-in reader that takes the byte from +0x11
// instead of +0x10 and is fed through the SAME comparison loop the real cases
// used — a control that re-implements the check proves nothing about the check
// (reference_recheck_must_match_its_own_proof.md).
// ---------------------------------------------------------------------------
uint8_t AppBadIsCursorEnabled(const Application* self) {   // control only
    return self->_running;      // WRONG offset on purpose
}

void ProbeApplicationReaders() {
    void* getWindowOurs;   void* getWindowTheirs;
    void* getInstOurs;     void* getInstTheirs;
    void* cursorOurs;      void* cursorTheirs;
    void* platPausedOurs;  void* platPausedTheirs;
    void* pausedOurs;      void* pausedTheirs;

    if (!AppPair("?GetWindow@Application@1@QEBAPEAUHWND__@@XZ",
                 "Application::GetWindow", &getWindowOurs, &getWindowTheirs)) return;
    if (!AppPair("?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ",
                 "Application::GetInstance", &getInstOurs, &getInstTheirs)) return;
    if (!AppPair("?IsCursorEnabled@Application@1@QEAA_NXZ",
                 "Application::IsCursorEnabled", &cursorOurs, &cursorTheirs)) return;
    if (!AppPair("?PlatformIsPaused@Application@1@AEAA_NXZ",
                 "Application::PlatformIsPaused", &platPausedOurs, &platPausedTheirs)) return;
    if (!AppPair("?IsPaused@Application@1@QEAA_NXZ",
                 "Application::IsPaused", &pausedOurs, &pausedTheirs)) return;

    // CROSSED-REGISTRATION CHECK. `IsPaused` (0x18009f4e0) and
    // `PlatformIsPaused` (0x18009fe80) are byte-identical bodies at two
    // addresses under two export names. Two distinct ports register them. If a
    // wave ever wired one name to the other's body nothing behavioural would
    // change — and nothing would notice. So assert the pointers DIFFER, both
    // on our side and on the DLL's.
    if (platPausedOurs == pausedOurs)
        fprintf(stderr, "[RtOrphan][App] WARN [structural] IsPaused and "
                        "PlatformIsPaused resolve to the SAME body on OUR side "
                        "(%p) — the two registrations are crossed or aliased\n",
                pausedOurs);
    else
        fprintf(stderr, "[RtOrphan][App] pass [structural] IsPaused (%p) and "
                        "PlatformIsPaused (%p) are DISTINCT ported bodies; the DLL's "
                        "two are distinct too (%p / %p)\n",
                pausedOurs, platPausedOurs, pausedTheirs, platPausedTheirs);

    static const uint64_t kHandles[] = {
        0x0000000000000000ull, 0xffffffffffffffffull, 0x00000000deadbeefull,
        0x12345678abcdef01ull, 0x0000000000000001ull, 0x8000000000000000ull,
    };
    static const uint8_t kBytes[] = { 0x00, 0x01, 0x02, 0x7f, 0x80, 0xff };
    const int nH = (int)(sizeof(kHandles) / sizeof(kHandles[0]));
    const int nB = (int)(sizeof(kBytes) / sizeof(kBytes[0]));

    int cases = 0, cmps = 0, bad = 0, boolEquiv = 0;

    // The comparison, as one lambda so the negative control drives the same
    // code the real cases did.
    auto cmpByte = [&](const char* subject, uint8_t stored, uint8_t o, uint8_t t) {
        ++cmps;
        if (o == t) return;
        if (o != 0 && t != 0) {
            ++boolEquiv;
            if (boolEquiv <= 2)
                fprintf(stderr, "[RtOrphan][App] NOTE [live-diff] %s: stored byte "
                        "0x%02x -> ours AL=0x%02x theirs AL=0x%02x. Both non-zero, so "
                        "every `bool` caller agrees; the RAW byte does not.\n",
                        subject, stored, o, t);
            return;
        }
        ++bad;
        char detail[160];
        snprintf(detail, sizeof(detail), "stored byte 0x%02x: ours AL=0x%02x theirs AL=0x%02x",
                 stored, o, t);
        Report(subject, "reader-sweep", false, detail);
    };
    auto cmpPtr = [&](const char* subject, uint64_t stored, void* o, void* t) {
        ++cmps;
        if (o == t) return;
        ++bad;
        char detail[160];
        snprintf(detail, sizeof(detail), "stored 0x%llx: ours=%p theirs=%p",
                 (unsigned long long)stored, o, t);
        Report(subject, "reader-sweep", false, detail);
    };

    AppSubject s;
    for (int h = 0; h < nH; ++h) {
        for (int cb = 0; cb < nB; ++cb) {
            for (int rb = 0; rb < nB; ++rb) {
                memset(s.bytes, 0xA5, sizeof(s.bytes));     // poison the rest
                Application* a = s.app();
                a->_windowHandle   = (void*)(uintptr_t)kHandles[h];
                a->_platformHandle = (void*)(uintptr_t)kHandles[(h + 3) % nH];
                a->_cursorEnabled  = kBytes[cb];
                a->_running        = kBytes[rb];
                ++cases;

                cmpPtr("Application::GetWindow", kHandles[h],
                       reinterpret_cast<AppPtrFn>(getWindowOurs)(a),
                       reinterpret_cast<AppPtrFn>(getWindowTheirs)(a));
                cmpPtr("Application::GetInstance", kHandles[(h + 3) % nH],
                       reinterpret_cast<AppPtrFn>(getInstOurs)(a),
                       reinterpret_cast<AppPtrFn>(getInstTheirs)(a));
                cmpByte("Application::IsCursorEnabled", kBytes[cb],
                        reinterpret_cast<AppByteFn>(cursorOurs)(a),
                        reinterpret_cast<AppByteFn>(cursorTheirs)(a));
                cmpByte("Application::PlatformIsPaused", kBytes[rb],
                        reinterpret_cast<AppByteFn>(platPausedOurs)(a),
                        reinterpret_cast<AppByteFn>(platPausedTheirs)(a));
                cmpByte("Application::IsPaused", kBytes[rb],
                        reinterpret_cast<AppByteFn>(pausedOurs)(a),
                        reinterpret_cast<AppByteFn>(pausedTheirs)(a));
            }
        }
    }

    // NEGATIVE CONTROL — one case, same comparator, deliberately wrong reader.
    // Its subject (the returned byte) is inside the set this screen compares
    // (reference_control_must_pick_a_compared_subject.md).
    {
        ControlScope cs;
        memset(s.bytes, 0, sizeof(s.bytes));
        Application* a = s.app();
        a->_cursorEnabled = 0x01;
        a->_running       = 0x00;        // so +0x11 gives a DIFFERENT answer
        const uint8_t o = AppBadIsCursorEnabled(a);
        const uint8_t t = reinterpret_cast<AppByteFn>(cursorTheirs)(a);
        const int badBefore = bad;
        cmpByte("Application::IsCursorEnabled [CONTROL]", a->_cursorEnabled, o, t);
        AppControl(kScrReaders, bad > badBefore, "reader taking the byte from +0x11");
        bad = badBefore;                 // the control's own miss is not a failure
    }

    if (bad == 0) {
        ++g_pass;
        fprintf(stderr, "[RtOrphan][App] pass [live-diff] readers: %d states x 5 "
                        "readers = %d comparisons against the shipped DLL, 0 truth-value "
                        "divergences, %d raw-AL-only differences\n", cases, cmps, boolEquiv);
    } else {
        ++g_fail;
    }
}

// ---------------------------------------------------------------------------
// SCREEN 2 — `?PlatformDestruct@` is literally `ret 0x0` (0x18009fc00,
// re-read with objdump by this lane). There is no output to compare, so the
// only verifiable claim is a NEGATIVE one: our body writes nothing, anywhere.
//
// "VERIFIED" FOR THIS ROW MEANS "VERIFIED TO DO NOTHING". That is all there is
// to verify, and it is stated rather than dressed up: the probe brackets a
// memcmp of the whole 0x100-byte subject (struct plus guard band) around the
// call, on both sides, over several fill patterns.
//
// The negative control is a stand-in that writes one byte, because a bracket
// that cannot fail would report "writes nothing" for a body that writes
// everything.
// ---------------------------------------------------------------------------
void AppDestructThatWrites(Application* self) {            // control only
    self->_quitRequested ^= 0x01;
}

void ProbeApplicationPlatformDestruct() {
    void* ours; void* theirs;
    if (!AppPair("?PlatformDestruct@Application@1@AEAAXXZ",
                 "Application::PlatformDestruct", &ours, &theirs)) return;

    static const uint8_t kFills[] = { 0x00, 0xff, 0xa5, 0x5a };
    int cases = 0, bad = 0;

    AppSubject a, b;
    alignas(16) uint8_t snapA[0x100], snapB[0x100];

    auto bracket = [&](AppVoidFn fn, uint8_t fill) -> bool {
        memset(a.bytes, fill, sizeof(a.bytes));
        memcpy(snapA, a.bytes, sizeof(snapA));
        fn(a.app());
        return memcmp(snapA, a.bytes, sizeof(snapA)) == 0;
    };

    for (uint8_t fill : kFills) {
        ++cases;
        if (!bracket(reinterpret_cast<AppVoidFn>(ours), fill)) {
            ++bad;
            Report("Application::PlatformDestruct", "ours wrote something", false,
                   "our body modified the object; retail's is `ret 0x0`");
        }
        // And the DLL's, for the same 0x100 bytes — this is what makes the
        // claim a comparison and not just an assertion about our own code.
        memset(b.bytes, fill, sizeof(b.bytes));
        memcpy(snapB, b.bytes, sizeof(snapB));
        reinterpret_cast<AppVoidFn>(theirs)(b.app());
        if (memcmp(snapB, b.bytes, sizeof(snapB)) != 0) {
            ++bad;
            Report("Application::PlatformDestruct", "theirs wrote something", false,
                   "the SHIPPED body modified the object — the premise is wrong, "
                   "not the port");
        }
    }

    {
        const bool caught = !bracket(&AppDestructThatWrites, 0x00);
        AppControl(kScrDestruct, caught, "stand-in that flips one byte at +0x28");
    }

    if (bad == 0) {
        ++g_pass;
        fprintf(stderr, "[RtOrphan][App] pass [live-diff] PlatformDestruct: %d fill "
                        "patterns x 0x100 bytes bracketed on BOTH sides — neither body "
                        "wrote anything. VERIFIED TO DO NOTHING, which is the whole of "
                        "this row's behaviour\n", cases);
    } else {
        ++g_fail;
    }
}

// ---------------------------------------------------------------------------
// SCREEN 3 — the three tail-call thunks, verified STRUCTURALLY.
//
// `?Update@` / `?Shutdown@` / `?Startup@` are each one `jmp rel32`. They are
// NOT executed here and must not be: Startup and Update pump message loops and
// Shutdown tears the platform down. Instead the DLL's own bytes are decoded —
// the technique the file banner above credits to `RtIoSerializerRegister`,
// which verifies the 18-member `IO::Serializer` forwarder family at
// registration by decoding the DLL's tail-jump and checking the target.
//
// TWO SEPARATE CLAIMS, and a lane that checks only the first has proved the
// wrong thing:
//   3a  RETAIL's jump target is the sibling the port names.
//   3b  OUR compiled body actually calls that sibling's PORT.
// (3a alone would pass even if our thunk forwarded somewhere else entirely.)
//
// 3b is a byte scan of our own compiled body for a direct `call`/`jmp rel32`
// whose target is the sibling's entry point. Its control is the discriminating
// one: the same scanner must NOT find a call to PlatformUpdate inside
// RtApp_Shutdown. A scanner that says yes to everything would pass 3b for free.
//
// SHUTDOWN IS DIFFERENT AND THE DIFFERENCE IS REPORTED, NOT PAPERED OVER.
// `RtApp_Shutdown` forwards through `RT_ORIGINAL_ONCE("?PlatformShutdown@…")`
// because lane B's body is not in this tree (application_thunks.cpp says so).
// There is no direct call to scan for, so 3b for Shutdown is a NAME check:
// the export name our body forwards to resolves to the address retail's jump
// targets. That is strictly weaker than the other two and is labelled so.
// ---------------------------------------------------------------------------
bool AppDecodeJmpRel32(const void* at, const void** target) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(at);
    if (p[0] != 0xE9) return false;
    int32_t rel;
    memcpy(&rel, p + 1, 4);
    *target = p + 5 + rel;
    return true;
}

// Scan a compiled body for a direct E8/E9 rel32 to `target`.
bool AppBodyCallsTarget(const void* fn, const void* target, size_t window) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(fn);
    for (size_t i = 0; i + 5 <= window; ++i) {
        if (p[i] != 0xE8 && p[i] != 0xE9) continue;
        int32_t rel;
        memcpy(&rel, p + i + 1, 4);
        if (p + i + 5 + rel == reinterpret_cast<const uint8_t*>(target)) return true;
    }
    return false;
}

void ProbeApplicationThunks() {
    struct Row {
        const char* thunk;
        const char* sibling;
        const char* subject;
        bool        oursCallsSiblingDirectly;   // false = Shutdown's name seam
    };
    static const Row kRows[] = {
        { "?Update@Application@1@QEAA_NXZ",
          "?PlatformUpdate@Application@1@AEAA_NXZ",
          "Application::Update",   true  },
        { "?Startup@Application@1@QEAA_NXZ",
          "?PlatformStartup@Application@1@AEAA_NXZ",
          "Application::Startup",  true  },
        // ROUND 2: lane B landed `RtApp_PlatformShutdown`
        // (platform_startup_win.cpp) and the integrator re-pointed
        // `RtApp_Shutdown` to call it DIRECTLY instead of forwarding through
        // RT_ORIGINAL_ONCE. So this row is no longer the weak one — it now
        // gets the same scan Update and Startup get, and the previous
        // "[structural, WEAKER]" name-only check is retired.
        { "?Shutdown@Application@1@QEAAXXZ",
          "?PlatformShutdown@Application@1@AEAAXXZ",
          "Application::Shutdown", true },
    };

    int bad3a = 0, bad3b = 0, done3a = 0, done3b = 0;
    const void* retailTarget[3] = { nullptr, nullptr, nullptr };
    void* oursThunk[3] = { nullptr, nullptr, nullptr };
    void* oursSibling[3] = { nullptr, nullptr, nullptr };

    for (int i = 0; i < 3; ++i) {
        const Row& r = kRows[i];
        void* ours; void* theirs;
        if (!AppPair(r.thunk, r.subject, &ours, &theirs)) continue;
        oursThunk[i] = ours;

        // ---- 3a: decode retail's own jmp rel32.
        const void* tgt = nullptr;
        if (!AppDecodeJmpRel32(theirs, &tgt)) {
            ++bad3a;
            Report(r.subject, "3a decode", false,
                   "the shipped body does not begin with a jmp rel32 — the port's "
                   "whole premise is wrong");
            continue;
        }
        retailTarget[i] = tgt;
        void* siblingTheirs = ResolveExportRaw(r.sibling);
        ++done3a;
        if (tgt != siblingTheirs) {
            ++bad3a;
            char detail[192];
            snprintf(detail, sizeof(detail),
                     "retail jumps to %p, but %s is at %p", tgt, r.sibling, siblingTheirs);
            Report(r.subject, "3a retail jump target", false, detail);
        } else {
            fprintf(stderr, "[RtOrphan][App] pass [structural] %s: shipped body is "
                            "`jmp %p` and that IS %s\n", r.subject, tgt, r.sibling);
        }

        // ---- 3b: does OUR body reach that sibling?
        ++done3b;
        if (r.oursCallsSiblingDirectly) {
            // The sibling's PORT, not the DLL's body: ResolveExport consults the
            // port registry first, so this is our own compiled function.
            void* sib = ResolveExport(r.sibling);
            oursSibling[i] = sib;
            if (sib && sib == ResolveExportRaw(r.sibling)) {
                ++bad3b;
                Report(r.subject, "3b", false,
                       "the SIBLING is not ported in this tree, so a 'forwards to our "
                       "sibling' claim cannot be checked");
            } else if (!sib || !AppBodyCallsTarget(oursThunk[i], sib, 0x400)) {
                ++bad3b;
                char detail[192];
                snprintf(detail, sizeof(detail),
                         "no direct call/jmp to %s (%p) in the first 0x400 bytes of our "
                         "body at %p", r.sibling, sib, oursThunk[i]);
                Report(r.subject, "3b our body calls the sibling", false, detail);
            } else {
                fprintf(stderr, "[RtOrphan][App] pass [structural] %s: our compiled "
                                "body contains a direct call to our own %s (%p)\n",
                        r.subject, r.sibling, sib);
            }
        } else {
            // Shutdown: the seam is a NAME, resolved at run time. All that can
            // be checked is that the name lands where retail's jump lands.
            void* byName = ResolveExportRaw(r.sibling);
            if (byName != tgt) {
                ++bad3b;
                Report(r.subject, "3b name seam", false,
                       "the export name our body forwards to does not resolve to "
                       "retail's jump target");
            } else {
                fprintf(stderr, "[RtOrphan][App] pass [structural, WEAKER] %s: the "
                                "export name our body forwards to ('%s') resolves to "
                                "%p, retail's own jump target. This does NOT prove our "
                                "body forwards to it — lane B's sibling is absent from "
                                "this tree, so there is no ported callee to scan for\n",
                        r.subject, r.sibling, byName);
            }
        }
    }

    // ---- CONTROL 3a: the same decoder and comparator, wrong expected target.
    if (retailTarget[0]) {
        void* wrong = ResolveExportRaw("?PlatformStartup@Application@1@AEAA_NXZ");
        AppControl(kScrThunkRetail, retailTarget[0] != wrong,
                   "Update's jump target compared against PlatformStartup");
    }
    // ---- CONTROL 3b: the same scanner, a call that is NOT there. This is the
    // discriminating control — a scanner that returns true for anything would
    // have passed 3b above for free.
    if (oursThunk[2] && oursSibling[0]) {
        const bool caught = !AppBodyCallsTarget(oursThunk[2], oursSibling[0], 0x400);
        AppControl(kScrThunkOurs, caught,
                   "scanning RtApp_Shutdown for a call to RtApp_PlatformUpdate");
    }

    if (bad3a == 0 && done3a == 3) ++g_pass; else ++g_fail;
    if (bad3b == 0 && done3b == 3) ++g_pass; else ++g_fail;
    fprintf(stderr, "[RtOrphan][App] thunks: 3a %d/%d rows, 3b %d/%d rows "
                    "(NOTHING was executed — no message loop was pumped and nothing "
                    "was torn down)\n", done3a - bad3a, done3a, done3b - bad3b, done3b);
}

// ---------------------------------------------------------------------------
// SCREEN 4 — `?Get@` (static; returns `_singleton`).
//
//   18000a2f0: MOV RAX,[RIP+0x1f2381]   # 0x1801fc678
//   18000a2f7: RET
//
// Our port reads the same symbol through `RT_DATA_SLOT`. THAT IS ONLY THE SAME
// SLOT WHILE THE MINT ROW IS OFF. `standalone/rt_data_mint_tables.inc:2159`
// carries a minted row for `?_singleton@Application@1@0PEAV11@EA` at
// 0x1801fc678; it is not in `reg_mint_promoted.cpp`, so in an ordinary run it
// is unregistered and `RT_DATA_SLOT` falls through to the DLL's own slot. But
// under `BANISHED_RT_MINT=1` (or `BANISHED_RT_MINT_ROWS` naming it) our reader
// would read OUR storage while retail's `Get` reads the DLL's — a split brain,
// and every sweep case would "diverge" for a reason that is not a port defect.
//
// So the probe checks the two doors resolve to the SAME ADDRESS first, and
// refuses to sweep (reporting SPLIT, not FAIL) when they do not. Checking the
// premise before the comparison is the whole point of the check.
//
// THE SWEEP WRITES THE SINGLETON. It saves the slot, writes sentinels, calls
// both bodies, compares, and restores — verifying the restore. It runs at boot
// from `RtOrphanProbe()`, before `Engine::Startup` builds any thread, so there
// is no concurrent reader of that slot.
// ---------------------------------------------------------------------------
void ProbeApplicationGet() {
    void* ours; void* theirs;
    if (!AppPair("?Get@Application@1@SAPEAV11@XZ", "Application::Get", &ours, &theirs))
        return;

    const char* kSlot = "?_singleton@Application@1@0PEAV11@EA";
    void** oursSlot   = reinterpret_cast<void**>(ResolveExport(kSlot));
    void** theirsSlot = reinterpret_cast<void**>(ResolveExportRaw(kSlot));
    if (!theirsSlot) {
        fprintf(stderr, "[RtOrphan][App] SKIP Application::Get: the DLL has no "
                        "'%s' data export\n", kSlot);
        ++g_fail;
        return;
    }
    if (oursSlot != theirsSlot) {
        fprintf(stderr, "[RtOrphan][App] SPLIT Application::Get: our door resolves "
                        "'%s' to %p, the DLL's slot is %p. The minted row is ACTIVE, so "
                        "our Get and retail's Get read DIFFERENT storage and no sweep "
                        "here could distinguish a port defect from that split. Not "
                        "swept, and NOT counted as a pass.\n",
                kSlot, (void*)oursSlot, (void*)theirsSlot);
        return;
    }

    static const uint64_t kVals[] = {
        0x0000000000000000ull, 0xffffffffffffffffull, 0x00000000deadbeefull,
        0x0000000180000000ull, 0x0000000000000008ull, 0x7fffffffffffffffull,
    };
    void* const saved = *theirsSlot;
    int cmps = 0, bad = 0;

    auto cmpGot = [&](uint64_t wrote, void* o, void* t) {
        ++cmps;
        if (o == t) return;
        ++bad;
        char detail[160];
        snprintf(detail, sizeof(detail), "slot=0x%llx ours=%p theirs=%p",
                 (unsigned long long)wrote, o, t);
        Report("Application::Get", "singleton-sweep", false, detail);
    };

    for (uint64_t v : kVals) {
        *theirsSlot = (void*)(uintptr_t)v;
        void* o = reinterpret_cast<AppGetFn>(ours)();
        void* t = reinterpret_cast<AppGetFn>(theirs)();
        cmpGot(v, o, t);
        if ((uint64_t)(uintptr_t)t != v) {
            ++bad;
            Report("Application::Get", "premise", false,
                   "the SHIPPED Get did not return the value written into the slot — "
                   "this is not the slot retail reads");
        }
    }

    // NEGATIVE CONTROL, through the same comparator, on the value it compares.
    {
        ControlScope cs;
        const int badBefore = bad;
        cmpGot(0, (void*)0x1, (void*)0x2);
        AppControl(kScrGet, bad > badBefore, "two deliberately unequal Get results");
        bad = badBefore;
    }

    *theirsSlot = saved;
    const bool restored = (*theirsSlot == saved);
    if (!restored) { ++bad; Report("Application::Get", "restore", false,
                                   "failed to restore the singleton slot"); }

    if (bad == 0) {
        ++g_pass;
        fprintf(stderr, "[RtOrphan][App] pass [live-diff] Application::Get: %d sentinel "
                        "values written into the DLL's own 0x1801fc678 slot, ours and the "
                        "shipped body agreed on every one; slot restored to %p\n",
                cmps - 1, saved);
    } else {
        ++g_fail;
    }
}

// ---------------------------------------------------------------------------
// SCREEN 5 — `?EnableCursor@` on TWO INDEPENDENT SYNTHETIC APPLICATIONS.
//
// Lane C gave this row rt_predict.h's PREDICTING oracle, and the reason it
// gave is right: a replay on ONE object cannot work, because our body has
// already flipped the flag by the time retail's runs, so retail takes the
// early-out at 0x18009f803 and the tree walk — the only thing that can go
// wrong — is never executed.
//
// CAN THAT ORACLE EVER FIRE? NO, as the tree stands. It is gated on
// `RT_PREDICT_ON` (BANISHED_RT_PREDICT=1) *and* on somebody calling
// `RtApp_EnableCursor`. Nothing in `code/ engine/ runtime/ standalone/ dx11/`
// does. So the predicting oracle is presently unreachable, exactly the way the
// row's `calls=0` is unreachable. This probe is the missing driver, and it
// serves both modes:
//
//   BANISHED_RT_PREDICT unset  ->  the probe runs its OWN live diff, below.
//   BANISHED_RT_PREDICT=1      ->  calling ours enters lane C's predicting
//                                  round, which calls the DLL itself. The
//                                  probe MUST NOT then compare A against B:
//                                  both objects would have been mutated by the
//                                  DLL and the comparison would be retail
//                                  against retail — a vacuous pass. It detects
//                                  the mode, drives the rounds anyway (so the
//                                  oracle fires and prints its own verdict),
//                                  and says that rt_predict.h owns the verdict.
//
// THE LIVE DIFF: two independent synthetic Applications with their own
// synthetic AVL nodes. Retail's descent (re-read at 0x18009f811..824) is
// `key > 0x20 -> left(+0x18)`, `key < 0x20 -> right(+0x20)`, equal ->
// tail-jmp `[node+0x8]` with (rcx=[this], edx=0x20, r8=0, r9=0). The trees
// below make the target reachable at the root, only through a LEFT step, only
// through a RIGHT step, and not at all.
//
// The callback is OURS — a recorder. That is not a Device-style fake: the one
// thing EnableCursor asks of the world is "call this pointer with these four
// arguments", so recording the call IS the observation
// (the shape ProbeAudioScene's Device::Stop recorder uses).
// ---------------------------------------------------------------------------
typedef int (*AppCursorCbFn)(void*, unsigned, uint64_t, int64_t);

struct AppCbCall { void* hwnd; unsigned msg; uint64_t w; int64_t l; };
AppCbCall g_appCbLog[2][8];
int       g_appCbCount[2] = {0, 0};
int       g_appCbSide     = -1;

int AppProbeCursorCallback(void* hwnd, unsigned msg, uint64_t w, int64_t l) {
    const int s = g_appCbSide;
    if (s >= 0) {
        if (g_appCbCount[s] < 8) g_appCbLog[s][g_appCbCount[s]] = { hwnd, msg, w, l };
        ++g_appCbCount[s];
    }
    return 0x5A5A;
}

void AppSeedNode(rtsetavl::Node* n, unsigned key, AppCursorCbFn cb) {
    memset(n, 0, sizeof(*n));
    memcpy(n->element + 0, &key, sizeof(key));
    memcpy(n->element + 8, &cb, sizeof(cb));
}

// Build one of five tree shapes into `nodes[3]`; returns the root.
rtsetavl::Node* AppBuildCursorTree(int shape, rtsetavl::Node* nodes) {
    const unsigned kTarget = 0x20;
    switch (shape) {
        case 0:                                        // empty
            return nullptr;
        case 1:                                        // hit at the root
            AppSeedNode(&nodes[0], kTarget, &AppProbeCursorCallback);
            return &nodes[0];
        case 2:                                        // one LEFT step (key > 0x20)
            AppSeedNode(&nodes[0], 0x0030, &AppProbeCursorCallback);
            AppSeedNode(&nodes[1], kTarget, &AppProbeCursorCallback);
            nodes[0].left = &nodes[1];
            return &nodes[0];
        case 3:                                        // one RIGHT step (key < 0x20)
            AppSeedNode(&nodes[0], 0x0010, &AppProbeCursorCallback);
            AppSeedNode(&nodes[1], kTarget, &AppProbeCursorCallback);
            nodes[0].right = &nodes[1];
            return &nodes[0];
        default:                                       // miss, both compares taken
            AppSeedNode(&nodes[0], 0x0030, &AppProbeCursorCallback);
            AppSeedNode(&nodes[1], 0x0010, &AppProbeCursorCallback);
            nodes[0].left = &nodes[1];
            return &nodes[0];
    }
}

void ProbeApplicationEnableCursor() {
    void* ours; void* theirs;
    if (!AppPair("?EnableCursor@Application@1@QEAAX_N@Z",
                 "Application::EnableCursor", &ours, &theirs)) return;

    const char* predictEnv = getenv("BANISHED_RT_PREDICT");
    const bool predicting = predictEnv && *predictEnv && strcmp(predictEnv, "0") != 0;

    static const uint8_t kPrev[]  = { 0x00, 0x01, 0x02 };
    static const bool    kArg[]   = { false, true };
    const int kShapes = 5;

    int cases = 0, bad = 0, dispatched = 0, flipped = 0;

    AppSubject A, B;
    rtsetavl::Node nodesA[3], nodesB[3];
    alignas(16) uint8_t snapNodesA[sizeof(nodesA)], snapNodesB[sizeof(nodesB)];

    // The comparator: everything the two objects can be compared on, plus the
    // recorded dispatch. `_messageCallbackRoot` is EXCLUDED and the exclusion
    // is not a convenience — the two sides own SEPARATE node arrays, so the two
    // root pointers can never be equal. Its immutability is checked instead, by
    // the node-array snapshots.
    auto compare = [&](char* detail, size_t n) -> bool {
        const Application* a = reinterpret_cast<const Application*>(A.bytes);
        const Application* b = reinterpret_cast<const Application*>(B.bytes);
        if (a->_cursorEnabled != b->_cursorEnabled) {
            snprintf(detail, n, "_cursorEnabled ours=0x%02x theirs=0x%02x",
                     a->_cursorEnabled, b->_cursorEnabled);
            return false;
        }
        if (a->_windowHandle != b->_windowHandle || a->_platformHandle != b->_platformHandle ||
            a->_running != b->_running || a->_quitRequested != b->_quitRequested ||
            a->_messageCallbackCount != b->_messageCallbackCount) {
            snprintf(detail, n, "a field OUTSIDE _cursorEnabled diverged");
            return false;
        }
        if (memcmp(A.bytes + 0x30, B.bytes + 0x30, sizeof(A.bytes) - 0x30) != 0) {
            snprintf(detail, n, "a byte in the guard band past the struct diverged");
            return false;
        }
        if (memcmp(snapNodesA, nodesA, sizeof(nodesA)) != 0 ||
            memcmp(snapNodesB, nodesB, sizeof(nodesB)) != 0) {
            snprintf(detail, n, "the callback tree was MODIFIED; EnableCursor only reads it");
            return false;
        }
        if (g_appCbCount[0] != g_appCbCount[1]) {
            snprintf(detail, n, "callback fired %d times by ours, %d by theirs",
                     g_appCbCount[0], g_appCbCount[1]);
            return false;
        }
        for (int k = 0; k < g_appCbCount[0] && k < 8; ++k) {
            const AppCbCall& x = g_appCbLog[0][k];
            const AppCbCall& y = g_appCbLog[1][k];
            if (x.hwnd != y.hwnd || x.msg != y.msg || x.w != y.w || x.l != y.l) {
                snprintf(detail, n,
                         "dispatch #%d ours=(%p,0x%x,%llu,%lld) theirs=(%p,0x%x,%llu,%lld)",
                         k, x.hwnd, x.msg, (unsigned long long)x.w, (long long)x.l,
                         y.hwnd, y.msg, (unsigned long long)y.w, (long long)y.l);
                return false;
            }
        }
        return true;
    };

    // One case, as a lambda, so the negative controls can RE-RUN a case of
    // their choosing and then corrupt it. The first version of this screen let
    // the controls inherit whatever state the LAST sweep case happened to
    // leave, and the last case is a tree MISS — so the dispatch control fired
    // on a case with no dispatch to corrupt and honestly reported itself
    // UNCONTROLLED (1/2). A control has to pick its subject from the set the
    // probe actually compares
    // (reference_control_must_pick_a_compared_subject.md), and that means
    // choosing the case, not accepting one.
    auto runCase = [&](uint8_t prev, bool arg, int shape) -> bool {
        memset(A.bytes, 0, sizeof(A.bytes));
        memset(B.bytes, 0, sizeof(B.bytes));
        Application* a = A.app();
        Application* b = B.app();
        a->_windowHandle = b->_windowHandle = (void*)(uintptr_t)0xBADC0FFEE0DDF00Dull;
        a->_running      = b->_running      = 1;
        a->_cursorEnabled = b->_cursorEnabled = prev;
        a->_messageCallbackRoot = AppBuildCursorTree(shape, nodesA);
        b->_messageCallbackRoot = AppBuildCursorTree(shape, nodesB);
        memcpy(snapNodesA, nodesA, sizeof(nodesA));
        memcpy(snapNodesB, nodesB, sizeof(nodesB));
        g_appCbCount[0] = g_appCbCount[1] = 0;

        g_appCbSide = 0;
        reinterpret_cast<AppCursorFn>(ours)(a, arg);
        if (predicting) {
            // Lane C's predicting round already ran the DLL on `a`. Do NOT run
            // it again on `b`, and do NOT compare: that would be retail against
            // retail.
            g_appCbSide = -1;
            return true;
        }
        g_appCbSide = 1;
        reinterpret_cast<AppCursorFn>(theirs)(b, arg);
        g_appCbSide = -1;

        if (g_appCbCount[0] > 0) ++dispatched;
        if (a->_cursorEnabled != prev) ++flipped;

        char detail[224];
        if (compare(detail, sizeof(detail))) return true;
        ++bad;
        char full[320];
        snprintf(full, sizeof(full), "prev=0x%02x arg=%d shape=%d: %s",
                 prev, (int)arg, shape, detail);
        Report("Application::EnableCursor", "two-object", false, full);
        return false;
    };

    for (uint8_t prev : kPrev) {
        for (bool arg : kArg) {
            for (int shape = 0; shape < kShapes; ++shape) {
                ++cases;
                if (!runCase(prev, arg, shape)) goto sweepDone;
            }
        }
    }
sweepDone:;

    if (predicting) {
        fprintf(stderr, "[RtOrphan][App] Application::EnableCursor: BANISHED_RT_PREDICT "
                        "is set, so this probe DROVE lane C's predicting oracle %d times "
                        "and made no comparison of its own (comparing here would be "
                        "retail against retail). The verdict is rt_predict.h's — read its "
                        "[RtPredict] lines, including changed=N/M\n", cases);
        return;
    }

    // TWO NEGATIVE CONTROLS, one per thing this comparator actually checks:
    // the flag byte and the recorded dispatch. One control over one term leaves
    // the other unproven (reference_shadow_predicate_must_name_its_failing_term.md).
    if (bad == 0) {
        char detail[224];
        // Re-run a case chosen for the controls: prev=0, arg=true, shape=1 is
        // the one that BOTH flips the flag AND dispatches the callback, so
        // both terms the comparator checks have something to corrupt.
        const int savedDispatched = dispatched, savedFlipped = flipped;
        runCase(0x00, true, 1);
        dispatched = savedDispatched;   // the control re-run is not a sweep case
        flipped    = savedFlipped;
        Application* a = A.app();
        const uint8_t saveFlag = a->_cursorEnabled;
        a->_cursorEnabled = (uint8_t)(saveFlag ^ 0xFF);
        AppControl(kScrCursor, !compare(detail, sizeof(detail)),
                   "corrupted _cursorEnabled on our object");
        a->_cursorEnabled = saveFlag;

        if (g_appCbCount[0] > 0) {
            const unsigned saveMsg = g_appCbLog[0][0].msg;
            g_appCbLog[0][0].msg = saveMsg + 1;
            AppControl(kScrCursor, !compare(detail, sizeof(detail)),
                       "corrupted the recorded dispatch message on our side");
            g_appCbLog[0][0].msg = saveMsg;
        } else {
            AppControl(kScrCursor, false,
                       "no dispatch was recorded on the last case, so the dispatch "
                       "comparison is UNCONTROLLED");
        }
    }

    if (bad == 0) {
        ++g_pass;
        fprintf(stderr, "[RtOrphan][App] pass [live-diff] EnableCursor: %d cases "
                        "(3 prior flag bytes x 2 args x 5 tree shapes) on two INDEPENDENT "
                        "synthetic Applications, 0 divergences; %d cases actually flipped "
                        "the flag and %d actually DISPATCHED a WM_SETCURSOR callback\n",
                cases, flipped, dispatched);
        if (dispatched == 0)
            fprintf(stderr, "[RtOrphan][App] WARN EnableCursor: 0 cases dispatched — the "
                            "tree walk, the only thing that can go wrong here, was NOT "
                            "exercised and this pass is thin\n");
    } else {
        ++g_fail;
    }
}

void ProbeApplication() {
    fprintf(stderr, "[RtOrphan][App] ==== Wave 12: Application rows with no caller ====\n");
    ProbeApplicationReaders();
    ProbeApplicationPlatformDestruct();
    ProbeApplicationThunks();
    ProbeApplicationGet();
    ProbeApplicationEnableCursor();
    for (int i = 0; i < kAppScreenCount; ++i)
        if (g_appCtlRun[i])
            fprintf(stderr, "[RtOrphan][App] negative control %s: %d/%d caught\n",
                    kAppScreenName[i], g_appCtlCaught[i], g_appCtlRun[i]);
    fflush(stderr);
}

// ---------------------------------------------------------------------------
// Save::SaveLoad::_singleton  —  ?_singleton@SaveLoad@Save@@0PEAV12@EA
// Runtime 0x1801fc6f0, ordinal 4817.                      (Wave 14 lane SINGLETON)
//
// This is a DATA row, so there is no body to diff. The question it has to
// answer is the one a data row's port turns on: DOES OUR WRITER PUBLISH INTO
// THE SLOT OUR READERS READ, and is that the same slot retail's own
// instructions read? A port that owns storage instead of publishing it is
// invisible until something reads the slot
// (reference_port_must_reproduce_side_effects.md).
//
// The complete reference set for 0x1801fc6f0 was enumerated twice, by two
// routes that could have disagreed (Ghidra xrefs, and a decoder-independent
// scan of every disp32 field in .text whose RIP-relative target is the slot —
// the scan initially reported 4 against Ghidra's 5 because it assumed no
// trailing immediate, which is exactly the encoding `MOV qword ptr [rip+d],0`
// uses; corrected, both routes give the same five):
//
//   0x1800c3d9b  W  ??0SaveLoad@Save@@QEAA@XZ           PORTED, registered
//   0x1800c3ca6  W  ??1SaveLoad@Save@@QEAA@XZ           PORTED, registered
//   0x1800dc3e9  R  ?OnVideoRestart@MainLoop@Engine@@   PORTED, registered
//   0x1800dcc1d  R  ?OnVideoError@MainLoop@Engine@@     PORTED, registered
//   0x180030d40  R  ?Get@SaveLoad@Save@@SAPEAV12@XZ     UNPORTED — and DEAD:
//        zero call sites in the DLL (Ghidra xrefs are the export table entry
//        and the entry-point sweep only), and it is not among the 899 imports
//        of Application-x64.exe nor the 163/146 of VideoDX11/VideoDX9.
//
// Plus 20 read sites in Application-x64.exe, every one of them through the
// import slot 0x140151610 — a DOOR, registered in reg_unreg_lane1.cpp — and no
// pointer-valued reference to the slot anywhere in any of the four binaries.
//
// So retail's `Get` is the ONE retail instruction that reads this slot and can
// never be redirected. That makes it useless as a consumer and perfect as an
// ORACLE: it is retail's own RIP-relative load of 0x1801fc6f0, and it can tell
// us where our ported constructor actually published.
//
// WHAT THIS PROBE COSTS, stated because the census cannot: naming `?Get@` as a
// string literal here PUT IT IN THE CENSUS'S `called` COLUMN. Save went
// called 29 -> 30 in the same commit that took ported 22 -> 23, so the set
// difference is `-_singleton, +Get` and not simply "one row closed"
// (reference_diff_the_set_not_the_count.md). The dependency is real but it is
// DIAGNOSTIC ONLY: the call is `ResolveExportRaw` under
// BANISHED_RT_ORPHAN_PROBE, it must be retail's body or the oracle is a
// self-check, and at cutover it resolves null and the probe says so and stops.
// Nobody should port `?Get@` on the strength of that row.
// ---------------------------------------------------------------------------
void ProbeSaveLoadSingleton() {
    const char* const kSlot = "?_singleton@SaveLoad@Save@@0PEAV12@EA";
    const char* const kGet  = "?Get@SaveLoad@Save@@SAPEAV12@XZ";
    const char* const kCtor = "??0SaveLoad@Save@@QEAA@XZ";
    const char* const kDtor = "??1SaveLoad@Save@@QEAA@XZ";
    const char* const kSubject = "Save::SaveLoad::_singleton";

    typedef void* (*GetFn)(void);
    typedef void* (*CtorFn)(void*);
    typedef void  (*DtorFn)(void*);

    void** oursSlot   = reinterpret_cast<void**>(ResolveExport(kSlot));
    void** theirsSlot = reinterpret_cast<void**>(ResolveExportRaw(kSlot));
    GetFn  retailGet  = reinterpret_cast<GetFn>(ResolveExportRaw(kGet));
    CtorFn ourCtor    = reinterpret_cast<CtorFn>(ResolveExport(kCtor));
    DtorFn ourDtor    = reinterpret_cast<DtorFn>(ResolveExport(kDtor));

    if (!theirsSlot || !retailGet || !ourCtor || !ourDtor) {
        Report(kSubject, "resolve", false, "one of the four doors resolved null");
        return;
    }

    // ARRANGEMENT. This line is the A/B: with the minted row NOT registered the
    // two doors are the same address and our ctor writes the DLL's own .data;
    // with it registered they differ and our storage is the live one.
    const bool minted = (oursSlot != theirsSlot);
    fprintf(stderr, "[RtOrphan][SL] arrangement=%s  ourDoor=%p  dllSlot=%p\n",
            minted ? "MINTED (our storage is live)" : "DLL-SLOT (retail .data is live)",
            (void*)oursSlot, (void*)theirsSlot);

    void* const savedOurs   = oursSlot ? *oursSlot : nullptr;
    void* const savedTheirs = *theirsSlot;

    // 1. PREMISE. Prove by execution — not by disassembly — that retail's Get
    //    reads exactly this slot. If this fails, every verdict below is void.
    {
        static const uint64_t kVals[] = {
            0x0000000000000000ull, 0xffffffffffffffffull, 0x00000000deadbeefull,
            0x0000000180000000ull, 0x0000000000000008ull, 0x7fffffffffffffffull,
        };
        int bad = 0;
        for (uint64_t v : kVals) {
            *theirsSlot = reinterpret_cast<void*>(static_cast<uintptr_t>(v));
            void* got = retailGet();
            if (got != reinterpret_cast<void*>(static_cast<uintptr_t>(v))) ++bad;
        }
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "the shipped Get returned the written value for %d of 6 sentinels",
                 6 - bad);
        Report(kSubject, "retail-Get-reads-this-slot", bad == 0, detail);
        *theirsSlot = savedTheirs;
    }

    // 2. THE PUBLISH CHECK. Run our REGISTERED constructor on a private object
    //    and ask where the pointer landed. `alignas(16)` and 0x90 bytes are the
    //    object's real size (runtime/types/save/SaveLoad.h); the ctor allocates
    //    nothing, so the matching dtor frees nothing.
    alignas(16) uint8_t scratch[0x90];
    memset(scratch, 0xCD, sizeof(scratch));
    *theirsSlot = nullptr;
    if (oursSlot) *oursSlot = nullptr;

    void* const obj = ourCtor(scratch);
    {
        char detail[224];
        snprintf(detail, sizeof(detail),
                 "after our ctor: ourDoor slot holds %p, dll slot holds %p, "
                 "retail Get returns %p, object is %p",
                 oursSlot ? *oursSlot : nullptr, *theirsSlot, retailGet(), obj);
        // The verdict that matters is the same in both halves of the A/B: our
        // writer must publish into the slot OUR readers resolve. Whether
        // retail's dead Get can also see it is reported, never asserted.
        Report(kSubject, "ctor-publishes-into-our-door",
               oursSlot && *oursSlot == scratch, detail);
        if (!minted) {
            Report(kSubject, "retail-Get-sees-our-object", retailGet() == scratch,
                   detail);
        } else {
            fprintf(stderr, "[RtOrphan][SL] SPLIT (expected, row is registered): "
                            "retail's Get reads the DLL slot and returns %p while our "
                            "readers hold %p. Harmless ONLY because Get is dead in all "
                            "four shipped binaries — see the banner.\n",
                    retailGet(), oursSlot ? *oursSlot : nullptr);
        }
    }

    // 3. NEGATIVE CONTROL, through the SAME comparator, on the value it
    //    compares: corrupt the published pointer and the publish check must
    //    report CONTROL-CAUGHT. (A control that corrupts something nothing
    //    compares indicts a sound instrument —
    //    reference_control_must_pick_a_compared_subject.md.)
    if (oursSlot) {
        ControlScope cs;
        void* const keep = *oursSlot;
        *oursSlot = reinterpret_cast<void*>(static_cast<uintptr_t>(0xBADC0DEull));
        Report(kSubject, "control:corrupted-published-pointer",
               *oursSlot == scratch, "the published pointer was overwritten and the "
                                     "publish comparator must NOT report ok");
        *oursSlot = keep;
    }

    // 4. The dtor's unconditional clear, through the same door.
    ourDtor(scratch);
    Report(kSubject, "dtor-clears-our-door",
           oursSlot && *oursSlot == nullptr, "our dtor must null the same slot");

    if (oursSlot) *oursSlot = savedOurs;
    *theirsSlot = savedTheirs;
    fprintf(stderr, "[RtOrphan][SL] restored: ourDoor=%p dllSlot=%p\n",
            oursSlot ? *oursSlot : nullptr, *theirsSlot);
}

} // namespace

void RtOrphanProbe() {
    if (!Enabled()) return;

    fprintf(stderr, "[RtOrphan] ==== probing ports with no caller in our tree ====\n");
    ProbeVector2NormalizeWithMag();
    ProbeApplication();
    ProbeSaveLoadSingleton();
    ProbeSaveLoadGenerateSaveName();
    fprintf(stderr, "[RtOrphan] ==== %d pass, %d fail ====\n", g_pass, g_fail);
    fflush(stderr);
}

// ============================================================================
// The in-game half: Input::Device
// ============================================================================
//
// WAVE8.md's "Coverage: what the runs did NOT exercise" opens with
//
//     "The whole of Input::Device (GetValue, Pressed, Released, ResetState) ...
//      These need an ATTENDED run with real keyboard and mouse input before
//      they count as exercised -- the harness injects dialog clicks by calling
//      handlers directly and never passes through the input layer."
//
// THE FIRST HALF OF THAT IS RIGHT AND THE SECOND HALF IS NOT, and the
// difference is worth writing down because it turns a blocked item into a
// finished one. `Device::GetValue`, `Pressed` and `Released` do not READ the
// hardware. They are const reads of already-latched state: each one computes
// `DeviceArrayBase(this, device, deviceCtx) + inputIdx` and returns one field
// of the State record it lands on. WAVE8_LANE_D gave all three a FULL REPLAY
// oracle precisely because they are pure with respect to the object.
//
// So what they need is not a human at a keyboard -- it is a CALLER. Whether a
// key was actually down changes the VALUE both sides read; it does not change
// whether they read the same address. All the port logic being verified is the
// address arithmetic, and a probe sweeps that better than a person can: an
// attended run only reaches whichever (device, deviceCtx, inputIdx) triples the
// player happens to press, while this covers every valid one -- all four
// joystick contexts, all 105 keyboard inputs, all 13 mouse inputs.
//
// NOT PROBED, and why:
//   * `Device::ResetState` WRITES: it memsets the three State ranges. Its
//     oracle is a self-check, not a replay, so a probe would add nothing and
//     would zero live input state mid-frame.
//   * `KeyMap::SetInput` / `ClearInput` mutate the binding tree -- probing them
//     would rewrite the player's key bindings.
//   * `KeyMap::GetDeviceForId` / `GetInputForId` / `GetIdForDeviceInput` need a
//     POPULATED binding tree -- which is exactly what a live gameplay frame
//     has, so they are probed here too rather than deferred. The tree is the
//     player's real key configuration, read through the same
//     `g_pKeyMapObjectExref` triple-deref `Camera_UpdateMovement` itself uses.
//
// Runs ONCE, from Camera_UpdateMovement (code/Game/Game/camera_input.cpp),
// which is on the input path and only ticks once the world is live.
// ============================================================================

namespace {

// Layout constants duplicated from runtime/types/input/Input.h rather than
// included: standalone/ does not build against runtime/types, and these are
// static_assert-pinned there. If they ever disagree, the probe's own bounds go
// wrong before its comparison does, so a mismatch shows up as a fault here
// rather than as a silent pass.
constexpr int kStateSize          = 0x18;
constexpr int kJoystickCount      = 4;
constexpr int kJoystickStride     = 0x1B0;
constexpr int kJoystickInputCount = kJoystickStride / kStateSize;   // 18
constexpr int kKeyboardInputCount = 0x69;                           // 105
constexpr int kMouseInputCount    = 0x0D;                           // 13

typedef int16_t (*DevGetValueFn)(const void*, int32_t, int32_t, int32_t);
typedef bool    (*DevBoolFn)    (const void*, int32_t, int32_t, int32_t);

bool g_inGameDone = false;

void ProbeInputDevice() {
    const char* kGetValue = "?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z";
    const char* kPressed  = "?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z";
    const char* kReleased = "?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z";

    void** slot = reinterpret_cast<void**>(
        // bodies; the whole probe dies with the oracle at cutover.
        // RT_RAW_OK: probe code -- reads the DLL's own singleton to exercise the shipped
        ResolveExportRaw("?_singleton@Device@Input@@0PEAV12@EA"));
    void* device = slot ? *slot : nullptr;
    if (!device) {
        fprintf(stderr, "[RtOrphan] SKIP Input::Device: singleton not built yet\n");
        return;
    }

    struct Fn3 { const char* name; void* ours; void* theirs; };
    Fn3 fns[3] = {
        { kGetValue, ResolveExport(kGetValue), ResolveExportRaw(kGetValue) },
        { kPressed,  ResolveExport(kPressed),  ResolveExportRaw(kPressed)  },
        { kReleased, ResolveExport(kReleased), ResolveExportRaw(kReleased) },
    };
    for (const Fn3& f : fns) {
        if (!f.ours || !f.theirs || f.ours == f.theirs) {
            // Same guard as the Vector2 probe: identical pointers mean the port
            // is not registered and the probe would compare the DLL with itself.
            fprintf(stderr, "[RtOrphan] FAIL Input::Device: %s ours=%p theirs=%p%s\n",
                    f.name, f.ours, f.theirs,
                    (f.ours && f.ours == f.theirs) ? " (port NOT registered)" : "");
            ++g_fail;
            return;
        }
    }

    // device 0 = joystick (deviceCtx selects one of four), 1 = keyboard,
    // 2 = mouse. Anything else makes DeviceArrayBase return nullptr and BOTH
    // sides then deref a near-null State* -- faithful, and deliberately NOT
    // probed: it would fault identically in retail and prove nothing.
    struct Range { int32_t device; int32_t ctxCount; int32_t inputCount; const char* name; };
    static const Range kRanges[] = {
        { 0, kJoystickCount, kJoystickInputCount, "joystick" },
        { 1, 1,              kKeyboardInputCount, "keyboard" },
        { 2, 1,              kMouseInputCount,    "mouse"    },
    };

    for (const Range& r : kRanges) {
        int mismatches = 0;
        int compared = 0;
        for (int32_t ctx = 0; ctx < r.ctxCount; ++ctx) {
            for (int32_t idx = 0; idx < r.inputCount; ++idx) {
                int16_t vOurs = reinterpret_cast<DevGetValueFn>(fns[0].ours)(device, r.device, ctx, idx);
                int16_t vThem = reinterpret_cast<DevGetValueFn>(fns[0].theirs)(device, r.device, ctx, idx);
                bool pOurs = reinterpret_cast<DevBoolFn>(fns[1].ours)(device, r.device, ctx, idx);
                bool pThem = reinterpret_cast<DevBoolFn>(fns[1].theirs)(device, r.device, ctx, idx);
                bool rOurs = reinterpret_cast<DevBoolFn>(fns[2].ours)(device, r.device, ctx, idx);
                bool rThem = reinterpret_cast<DevBoolFn>(fns[2].theirs)(device, r.device, ctx, idx);
                compared += 3;
                if (vOurs != vThem || pOurs != pThem || rOurs != rThem) {
                    if (++mismatches <= 3) {
                        char detail[192];
                        snprintf(detail, sizeof(detail),
                                 "ctx=%d idx=%d value %d/%d pressed %d/%d released %d/%d",
                                 (int)ctx, (int)idx, (int)vOurs, (int)vThem,
                                 (int)pOurs, (int)pThem, (int)rOurs, (int)rThem);
                        Report("Input::Device::{GetValue,Pressed,Released}", r.name, false, detail);
                    }
                }
            }
        }
        if (mismatches == 0) {
            ++g_pass;
            fprintf(stderr, "[RtOrphan] pass Input::Device %-8s %d comparisons\n",
                    r.name, compared);
        }
    }
}


// ---------------------------------------------------------------------------
// Input::KeyMap -- the three const readers WAVE8.md left on the attended list.
//
// All three are pure reads of the binding tree with FULL REPLAY oracles:
//   GetDeviceForId(id, slot)      BST lookup by id, then element.device[slot],
//                                 SIGN-extended (-1 means both "no such id" and
//                                 "slot unbound").
//   GetInputForId(id, slot)       the same lookup, element.inputId[slot],
//                                 ZERO-extended -- the asymmetry is retail's.
//   GetIdForDeviceInput(dev, in)  NOT a lookup: an in-order walk of the WHOLE
//                                 tree checking both slots of every binding.
//
// The domains are small enough to cover exhaustively, which matters most for
// the third: its Leftmost/Successor walk is the only place in this family where
// a port could get the tree traversal wrong, and a lookup-shaped test would
// never touch it. `id` is narrowed to a uint8_t by the port (and by retail), so
// 0..255 IS the whole domain, not a sample.
// ---------------------------------------------------------------------------
typedef int32_t (*KeyMapPairFn)(const void*, int32_t, int32_t);

void ProbeInputKeyMap(void* keyMap) {
    if (!keyMap) {
        fprintf(stderr, "[RtOrphan] SKIP Input::KeyMap: no live KeyMap this frame\n");
        return;
    }

    struct Sub { const char* name; const char* mangled; int32_t aLo, aHi, bLo, bHi; };
    static const Sub kSubs[] = {
        // id 0..255 x slot 0..1 -- the full domain of both id lookups.
        { "GetDeviceForId",      "?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z",      0, 255, 0, 1 },
        { "GetInputForId",       "?GetInputForId@KeyMap@Input@@QEBAHHH@Z",       0, 255, 0, 1 },
        // device 0..2 x inputId 0..104 -- every device and every keyboard input.
        { "GetIdForDeviceInput", "?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z", 0, 2,   0, 104 },
    };

    for (const Sub& sub : kSubs) {
        KeyMapPairFn ours = reinterpret_cast<KeyMapPairFn>(ResolveExport(sub.mangled));
        KeyMapPairFn theirs = reinterpret_cast<KeyMapPairFn>(ResolveExportRaw(sub.mangled));
        if (!ours || !theirs || ours == theirs) {
            fprintf(stderr, "[RtOrphan] FAIL Input::KeyMap::%s ours=%p theirs=%p%s\n",
                    sub.name, (void*)ours, (void*)theirs,
                    (ours && ours == theirs) ? " (port NOT registered)" : "");
            ++g_fail;
            continue;
        }

        int mismatches = 0, compared = 0;
        for (int32_t a = sub.aLo; a <= sub.aHi; ++a) {
            for (int32_t b = sub.bLo; b <= sub.bHi; ++b) {
                int32_t vOurs = ours(keyMap, a, b);
                int32_t vThem = theirs(keyMap, a, b);
                ++compared;
                if (vOurs != vThem && ++mismatches <= 3) {
                    char detail[160];
                    snprintf(detail, sizeof(detail), "(%d,%d) ours=%d theirs=%d",
                             (int)a, (int)b, (int)vOurs, (int)vThem);
                    Report("Input::KeyMap", sub.name, false, detail);
                }
            }
        }
        if (mismatches == 0) {
            ++g_pass;
            fprintf(stderr, "[RtOrphan] pass Input::KeyMap::%-20s %d comparisons\n",
                    sub.name, compared);
        }
    }
}


// ---------------------------------------------------------------------------
// Engine::UpdateLoop::IsRunning -- WAVE8.md: "never observed executing in any
// of the four runs", and lane A asked specifically that it not be assumed.
//
// It is the ONE function in lane A with a full replay oracle, which is where a
// divergence would be most informative, and it had no caller. Retail is two
// instructions:
//
//     MOV RAX,[0x1801fe750]        ; Engine::MainLoop::_singleton
//     CMP RCX,[RAX+0x20]           ; this == _singleton->_currentLoop ?
//
// `this` is COMPARED, never dereferenced -- which is what makes the false
// branch safe to probe with pointers that are not UpdateLoops at all. The true
// branch needs the genuinely running loop, and only a live frame has one.
//
// Both branches matter: a port that returned a constant, or that compared
// against `_nextLoop` (+0x28) instead of `_currentLoop` (+0x20), would pass a
// one-sided test. `_nextLoop` is included below for exactly that reason -- it
// is the near-miss field, and on a frame where a loop switch is pending it is a
// DIFFERENT pointer that must still answer false.
// ---------------------------------------------------------------------------
typedef bool (*IsRunningFn)(const void*);

void ProbeUpdateLoopIsRunning() {
    const char* kName = "?IsRunning@UpdateLoop@Engine@@QEBA_NXZ";
    const char* kSubject = "Engine::UpdateLoop::IsRunning";

    IsRunningFn ours = reinterpret_cast<IsRunningFn>(ResolveExport(kName));
    IsRunningFn theirs = reinterpret_cast<IsRunningFn>(ResolveExportRaw(kName));
    if (!ours || !theirs || ours == theirs) {
        fprintf(stderr, "[RtOrphan] FAIL %s: ours=%p theirs=%p%s\n", kSubject,
                (void*)ours, (void*)theirs,
                (ours && ours == theirs) ? " (port NOT registered)" : "");
        ++g_fail;
        return;
    }

    void** slot = reinterpret_cast<void**>(
        // RT_RAW_OK: probe code -- see the note at the Input::Device probe above.
        ResolveExportRaw("?_singleton@MainLoop@Engine@@1PEAV12@EA"));
    uint8_t* mainLoop = slot ? reinterpret_cast<uint8_t*>(*slot) : nullptr;
    if (!mainLoop) {
        fprintf(stderr, "[RtOrphan] SKIP %s: no MainLoop singleton\n", kSubject);
        return;
    }
    void* currentLoop = *reinterpret_cast<void**>(mainLoop + 0x20);
    void* nextLoop    = *reinterpret_cast<void**>(mainLoop + 0x28);

    struct Case { const char* name; void* self; };
    int dummy = 0;
    const Case kCases[] = {
        { "current-loop (true branch)", currentLoop },
        { "next-loop (the near miss)",  nextLoop    },
        { "null",                       nullptr     },
        { "unrelated pointer",          &dummy      },
        { "the MainLoop itself",        mainLoop    },
    };

    int mismatches = 0;
    for (const Case& c : kCases) {
        bool a = ours(c.self);
        bool b = theirs(c.self);
        if (a != b) {
            char detail[128];
            snprintf(detail, sizeof(detail), "self=%p ours=%d theirs=%d",
                     c.self, (int)a, (int)b);
            Report(kSubject, c.name, false, detail);
            ++mismatches;
        }
    }
    if (mismatches == 0) {
        ++g_pass;
        // Say whether the TRUE branch was actually reached: if currentLoop were
        // null this would be five agreeing falses, which is a vacuous pass of
        // exactly the kind this file exists to refuse.
        fprintf(stderr, "[RtOrphan] pass %s 5 cases (true branch %s)\n", kSubject,
                (currentLoop && ours(currentLoop)) ? "REACHED" : "NOT reached");
    }
}

// ---------------------------------------------------------------------------
// SCREEN 6 — `?LaunchProcess@` — the FAILURE path of the `wait == true` arm,
// and nothing else.
//
// THE WAVE BRIEF SAID "no oracle possible" AND THAT IS TRUE OF MOST OF THIS
// FUNCTION, but not of all of it. The two arms are:
//
//   wait == false   ShellExecuteW(NULL, L"open", cmd, …, SW_SHOWDEFAULT)
//   wait == true    CreateProcessW(…) ; WaitForSingleObject ; CloseHandle
//
// NOT PROBED, and the reasons are structural, not awkward:
//   * the whole `wait == false` arm. ShellExecuteW on a bad path can raise a
//     shell error dialog, and on a good one it opens a real document. Running
//     it twice does it twice. No.
//   * the `wait == true` SUCCESS path. It would spawn two real child
//     processes and block on both.
//
// PROBED: `wait == true` where `CreateProcessW` FAILS. Nothing is spawned,
// nothing is waited on, no handle is opened — the function allocates a
// WideString, calls CreateProcessW, gets FALSE, frees, and returns false. That
// is safe to run on both sides, and it is where the ONE piece of arithmetic in
// this function lives: retail's MAX_PATH-shaped 0x104 clamp
// (`CMP EBX,R14D` / `MOV R14D,0x104` at 0x18009f703/0x18009f709) and the
// force-terminate of a truncated command. The sweep straddles that boundary at
// 0x103 / 0x104 / 0x105 characters, which a single short command would not.
//
// WHAT THIS SCREEN CANNOT SEE: the ShellExecute arm at all, the success path,
// the leaked `pi.hThread`, and the exact command line CreateProcessW received
// (only the boolean outcome is observable from here — a port that truncated at
// the wrong offset but still produced a non-existent path would return the
// same `false`). Said plainly rather than folded into a pass.
//
// RUNS IN-GAME, not at boot: both sides construct a `System::WideString`,
// which allocates through `System::MemoryAllocator` — the same reason
// ProbeAudioScene lives here.
// ---------------------------------------------------------------------------
void ProbeApplicationLaunchProcess() {
    const char* kName = "?LaunchProcess@Application@1@QEAA_NPEBD_N@Z";
    void* ours   = ResolveExport(kName);
    void* theirs = ResolveExportRaw(kName);
    if (!ours || !theirs || ours == theirs) {
        fprintf(stderr, "[RtOrphan][App] SKIP Application::LaunchProcess: ours=%p "
                        "theirs=%p%s\n", ours, theirs,
                (ours && ours == theirs) ? " (port NOT registered)" : "");
        ++g_fail;
        return;
    }

    // A name that cannot resolve to an executable, padded to the length the
    // case wants. `.exe` is kept on the front so the failure is
    // ERROR_FILE_NOT_FOUND rather than a parse quirk.
    static const int kLens[] = { 40, 0x103, 0x104, 0x105, 0x180 };
    int cases = 0, bad = 0, control = 0, controlCaught = 0;

    auto cmpBool = [&](int len, bool o, bool t) {
        if (o == t) return;
        ++bad;
        char detail[128];
        snprintf(detail, sizeof(detail), "len=%d ours=%d theirs=%d", len, (int)o, (int)t);
        Report("Application::LaunchProcess", "createprocess-failure", false, detail);
    };

    char cmd[0x200];
    for (int len : kLens) {
        if (len >= (int)sizeof(cmd)) continue;
        memset(cmd, 'q', (size_t)len);
        cmd[len] = '\0';
        memcpy(cmd, "__rt_orphan_probe_no_such_binary__", 34);   // never a real file
        ++cases;

        const bool o = reinterpret_cast<AppLaunchFn>(ours)(nullptr, cmd, true);
        const bool t = reinterpret_cast<AppLaunchFn>(theirs)(nullptr, cmd, true);
        cmpBool(len, o, t);
        if (o || t) {
            ++bad;
            Report("Application::LaunchProcess", "premise", false,
                   "a side returned TRUE — a real process was created and waited on; "
                   "this case is not the failure path and the screen is UNSOUND");
        }
    }

    // NEGATIVE CONTROL, through the same comparator, on the value it compares.
    {
        ControlScope cs;
        const int badBefore = bad;
        cmpBool(-1, false, true);
        ++control;
        if (bad > badBefore) ++controlCaught;
        bad = badBefore;
    }
    fprintf(stderr, "[RtOrphan][App] negative control %s: %d/%d caught\n",
            kAppScreenName[kScrLaunch], controlCaught, control);

    if (bad == 0) {
        ++g_pass;
        fprintf(stderr, "[RtOrphan][App] pass [live-diff, PARTIAL] LaunchProcess: %d "
                        "wait=true FAILURE cases (command lengths straddling retail's "
                        "0x104 clamp), ours and the shipped body agreed on every one and "
                        "neither spawned anything. The ShellExecute arm and the SUCCESS "
                        "path remain UNVERIFIED and cannot be probed\n", cases);
    } else {
        ++g_fail;
    }
}

}  // namespace

extern "C" void RtOrphanProbeInGame(void* keyMap) {
    if (g_inGameDone || !Enabled()) return;
    g_inGameDone = true;

    fprintf(stderr, "[RtOrphan] ==== in-game probe (Input::Device, Input::KeyMap, UpdateLoop, Audio::Scene) ====\n");
    ProbeInputDevice();
    ProbeInputKeyMap(keyMap);
    ProbeUpdateLoopIsRunning();
    // Audio::Scene runs HERE and not in the boot probe: seeding a synthetic
    // scene calls System::MemoryAllocator::MemAlloc, and RtOrphanProbe() is
    // reached from Init() before Engine::Startup has built the allocator --
    // where the first call faults on a WRITE to +0x648 of a null singleton.
    // It needs no live world of its own; it only needs the process to be past
    // that point.
    ProbeAudioScene();
    // Wave 9 lane UIW. In the IN-GAME probe and not the boot one for exactly
    // the reason ProbeAudioScene gives above: it allocates through
    // System::MemoryAllocator, which Engine::Startup has not built yet when
    // RtOrphanProbe() runs from Init(). It needs no world of its own.
    ProbeGraphElementAddGraph();
    // Wave 12 lane VERIFY. In the in-game probe for the same reason: both
    // sides construct a System::WideString through System::MemoryAllocator.
    ProbeApplicationLaunchProcess();
    fprintf(stderr, "[RtOrphan] ==== running total: %d pass, %d fail ====\n", g_pass, g_fail);
    fflush(stderr);
}
