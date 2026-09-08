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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>

namespace {

bool Enabled() {
    const char* e = getenv("BANISHED_RT_ORPHAN_PROBE");
    return e && *e && strcmp(e, "0") != 0;
}

int g_pass = 0;
int g_fail = 0;

void Report(const char* subject, const char* caseName, bool ok, const char* detail) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        fprintf(stderr, "[RtOrphan] FAIL %s [%s]: %s\n", subject, caseName, detail);
    }
}

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

} // namespace

void RtOrphanProbe() {
    if (!Enabled()) return;

    fprintf(stderr, "[RtOrphan] ==== probing ports with no caller in our tree ====\n");
    ProbeVector2NormalizeWithMag();
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
    fprintf(stderr, "[RtOrphan] ==== running total: %d pass, %d fail ====\n", g_pass, g_fail);
    fflush(stderr);
}
