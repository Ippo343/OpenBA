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
#include "../engine/types/common.h"
#include "../runtime/types/math/Math.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

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

}  // namespace

extern "C" void RtOrphanProbeInGame(void* keyMap) {
    if (g_inGameDone || !Enabled()) return;
    g_inGameDone = true;

    fprintf(stderr, "[RtOrphan] ==== in-game probe (Input::Device, Input::KeyMap) ====\n");
    ProbeInputDevice();
    ProbeInputKeyMap(keyMap);
    fprintf(stderr, "[RtOrphan] ==== running total: %d pass, %d fail ====\n", g_pass, g_fail);
    fflush(stderr);
}
