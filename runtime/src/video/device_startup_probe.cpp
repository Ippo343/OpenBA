// ============================================================================
// runtime/src/video/device_startup_probe.cpp — Wave 9 lane VID2
//   The lockstep synthetic-subject oracle for
//   ?ValidateConfiguration@Device@Video@@QEAA_NXZ and, transitively, the seven
//   unexported Collection::Array helpers it drives.
// ============================================================================
//
// WHY A PROBE
// -----------
// `ValidateConfiguration` MUTATES the `Video::Configuration` singleton and
// REALLOCATES two arrays inside the Device. Running ours and then the shipped
// body at a live call site would (a) start the second run from the state the
// first one left, which is a different experiment, and (b) free and reallocate
// the same two arrays twice. So: two private `Video::Device`s and two private
// `Video::Configuration`s over a SHARED, read-only adapter graph, the same
// input applied to both, compared field by field.
//
// THE SINGLETON SWAP, and why it is safe here
// -------------------------------------------
// `ValidateConfiguration` does not take the configuration as a parameter — it
// reads `?_singleton@Configuration@Video@@`. So the probe points that slot at
// our configuration, runs ours, points it at theirs, runs the shipped body, and
// restores the original. That is a global mutation, which is exactly the kind
// of thing that has bitten this project, so it is bounded three ways: the probe
// runs ONCE, from the top of `PlatformStartup` before that function has read
// anything, and it restores the slot in every exit path including the failure
// ones.
//
// The shipped body is reached through **`ResolveExportRaw`**, never
// `ResolveExport` — this lane registers `?ValidateConfiguration@Device@Video@@QEAA_NXZ`,
// so `ResolveExport` would hand back our own port and the probe would compare
// ours to ours and always agree
// (reference_resolveaddr_consults_port_registry_first.md).
//
// WHY THE COMPARISON IS FIELD-BY-FIELD AND NOT A memcmp
// -----------------------------------------------------
// Neither implementation writes `Device::gap50`, the critical section, the
// window handle or the interface slots, and a `Device` that has only been
// memset is 0 there on both sides — so a memcmp would pass today and start
// failing the moment anything else touched the object. More importantly the
// two arrays hold DIFFERENT POINTERS by construction (two independent
// allocations), so their `_data` fields must be compared by CONTENT, not by
// value (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// THE POPULATION IS THE POINT
// ---------------------------
// A single well-formed adapter with one mode and one refresh rate would make
// every branch below take the same path and the sweep would prove nothing
// (reference_uniform_population_is_not_a_verification.md). The graph and the
// case list are built so that each of these is exercised at least once, and the
// per-branch counters at the end say how many times:
//
//   * interface index in range / negative / past the end
//   * adapter index in range / negative / past the end
//   * a requested resolution that IS in the mode list, and one that is NOT
//     (the only path into GetDefaultResolution and the two-pass re-lookup)
//   * a refresh rate that matches EXACTLY, one that matches only by INTEGER
//     RATIO, and one that matches neither
//   * a multisample index whose adapter entry is 0 (falls back) and one whose
//     entry is 1 (kept)
//   * a texture filter above the adapter cap and one below it
//   * shadowScalar / shaderDetail / reflectionDetail below 0, in range, and
//     above their (different: 2, 3, 2) caps
//
// Opt-in: `BANISHED_RT_VIDDEVSTART_PROBE=1`.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"

using RtVideo::Adapter;
using RtVideo::Configuration;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;
using RtVideo::RefreshRate;

// `ResolveExportRaw` comes from rt_port.h — redeclaring it here would make
// tools/port_bypass_audit.py count the DECLARATION as a raw-door site and
// report it as UNRESOLVED-NAME.

extern "C" bool RtVideo_Device_ValidateConfiguration(Device* self);

namespace {

bool g_armed = false;
bool g_running = false;

// ---------------------------------------------------------------------------
// The synthetic adapter graph. READ-ONLY for both sides, so one copy serves
// both — `ValidateConfiguration` never writes through the interface pointer,
// only through `this` and the configuration.
//
// Two interfaces, so an out-of-range interface index has somewhere wrong to
// point. Interface 0 has two adapters; interface 1 has one.
// ---------------------------------------------------------------------------
RefreshRate g_rr0[3] = { {60, 1}, {120000, 1001}, {30, 1} };   // 60, 119, 30
RefreshRate g_rr1[2] = { {75, 1}, {144, 1} };
RefreshRate g_rr2[1] = { {60, 1} };

DisplayMode g_modesA[4] = {
    { 1920, 1080, g_rr0, 3, 3 },
    { 1280,  720, g_rr1, 2, 2 },
    {  800,  600, g_rr2, 1, 1 },
    { 3840, 2160, g_rr0, 3, 3 },
};
DisplayMode g_modesB[2] = {
    { 1024, 768, g_rr2, 1, 1 },
    { 1600, 900, g_rr1, 2, 2 },
};

// Non-uniform on purpose: index 0 and 2 supported, 1 and 3 not — so a
// multisample fallback has both a "keep" and a "reset" case to find.
uint8_t g_msA[4] = { 1, 0, 1, 0 };
uint8_t g_msB[2] = { 1, 0 };

Adapter g_adapters0[2] = {
    { nullptr, g_modesA, 4, 4, g_msA, 4, 4, /*maxFilter*/ 8,  0x11, 0x22, 0 },
    { nullptr, g_modesB, 2, 2, g_msB, 2, 2, /*maxFilter*/ 2,  0x33, 0x44, 0 },
};
Adapter g_adapters1[1] = {
    { nullptr, g_modesB, 2, 2, g_msB, 2, 2, /*maxFilter*/ 16, 0x55, 0x66, 0 },
};

GraphicsInterface g_ifaces[2] = {
    { nullptr, g_adapters0, 2, 2 },
    { nullptr, g_adapters1, 1, 1 },
};

// ---------------------------------------------------------------------------
// One case: a starting configuration.
// ---------------------------------------------------------------------------
struct Case {
    const char* name;
    int32_t iface, adapter, w, h, refN, refD, ms, filter;
    int32_t shadow, shader, reflect;
};

const Case kCases[] = {
    // name                       if ad    w    h   refN refD ms flt sh sd rf
    { "exact-everything",          0, 0, 1920,1080,   60,  1,  0,  4, 1, 2, 1 },
    { "refresh-exact-fractional",  0, 0, 1920,1080,120000,1001,2,  4, 1, 2, 1 },
    { "refresh-ratio-only",        0, 0, 1920,1080,  119,  1,  0,  4, 1, 2, 1 },
    { "refresh-no-match-at-all",   0, 0, 1280, 720,   17,  1,  0,  4, 1, 2, 1 },
    { "resolution-absent",         0, 0, 1234, 567,   60,  1,  0,  4, 1, 2, 1 },
    { "ms-entry-false-resets",     0, 0,  800, 600,   60,  1,  1,  4, 1, 2, 1 },
    { "ms-entry-true-kept",        0, 0,  800, 600,   60,  1,  2,  4, 1, 2, 1 },
    { "filter-above-cap",          0, 0, 1920,1080,   60,  1,  0, 99, 1, 2, 1 },
    { "filter-below-cap",          0, 1, 1024, 768,   60,  1,  0,  1, 1, 2, 1 },
    { "iface-negative",           -3, 0, 1920,1080,   60,  1,  0,  4, 1, 2, 1 },
    { "iface-past-end",            9, 0, 1920,1080,   60,  1,  0,  4, 1, 2, 1 },
    { "iface-1-adapter-0",         1, 0, 1600, 900,   75,  1,  0,  4, 1, 2, 1 },
    { "adapter-negative",          0,-2, 1920,1080,   60,  1,  0,  4, 1, 2, 1 },
    { "adapter-past-end",          0, 7, 1920,1080,   60,  1,  0,  4, 1, 2, 1 },
    { "adapter-1-of-iface-0",      0, 1, 1600, 900,  144,  1,  0,  9, 1, 2, 1 },
    { "detail-all-negative",       0, 0, 1920,1080,   60,  1,  0,  4,-5,-5,-5 },
    { "detail-all-over-cap",       0, 0, 1920,1080,   60,  1,  0,  4, 9, 9, 9 },
    { "detail-at-cap",             0, 0, 1920,1080,   60,  1,  0,  4, 2, 3, 2 },
    { "4k-three-refresh",          0, 0, 3840,2160,   30,  1,  2,  8, 0, 0, 0 },
};

void InitDevice(Device* d) {
    std::memset(d, 0, sizeof(Device));
    d->interfaceData     = g_ifaces;
    d->interfaceCount    = 2;
    d->interfaceCapacity = 2;
    d->activeInterface   = -1;
}

void InitConfig(Configuration* c, const Case& k) {
    std::memset(c, 0, sizeof(Configuration));
    c->settingsValid       = 1;
    c->interfaceIndex      = k.iface;
    c->adapterIndex        = k.adapter;
    c->width               = k.w;
    c->height              = k.h;
    c->refreshNumerator    = k.refN;
    c->refreshDenominator  = k.refD;
    c->multisample         = k.ms;
    c->textureFilter       = k.filter;
    c->shadowScalar        = k.shadow;
    c->shaderDetail        = k.shader;
    c->reflectionDetail    = k.reflect;
    c->fullscreen          = 1;
    c->vsync               = 1;
}

bool SameConfig(const Configuration& a, const Configuration& b) {
    return a.interfaceIndex     == b.interfaceIndex &&
           a.adapterIndex       == b.adapterIndex &&
           a.width              == b.width &&
           a.height             == b.height &&
           a.refreshNumerator   == b.refreshNumerator &&
           a.refreshDenominator == b.refreshDenominator &&
           a.multisample        == b.multisample &&
           a.textureFilter      == b.textureFilter &&
           a.shadowScalar       == b.shadowScalar &&
           a.shaderDetail       == b.shaderDetail &&
           a.reflectionDetail   == b.reflectionDetail;
}

// The two arrays hold independent allocations, so `_data` is compared by
// CONTENT. `_capacity` IS compared: it is the growth rule's output and the
// whole reason GrowFor is transcribed rather than simplified.
bool SameDevice(const Device& a, const Device& b) {
    if (a.displayModeIndex != b.displayModeIndex) return false;
    if (a.refreshRateIndex != b.refreshRateIndex) return false;
    if (a.adapterField2C   != b.adapterField2C)   return false;
    if (a.adapterField30   != b.adapterField30)   return false;
    if (a.maxAnisotropy    != b.maxAnisotropy)    return false;

    if (a.modeCount != b.modeCount || a.modeCapacity != b.modeCapacity) return false;
    for (int32_t i = 0; i < a.modeCount; ++i) {
        const DisplayMode& x = a.modeData[i];
        const DisplayMode& y = b.modeData[i];
        if (x.width != y.width || x.height != y.height) return false;
        if (x.refreshCount != y.refreshCount || x.refreshCapacity != y.refreshCapacity)
            return false;
        for (int32_t j = 0; j < x.refreshCount; ++j) {
            if (x.refreshData[j].numerator   != y.refreshData[j].numerator ||
                x.refreshData[j].denominator != y.refreshData[j].denominator)
                return false;
        }
    }

    if (a.multisampleCount != b.multisampleCount ||
        a.multisampleCap   != b.multisampleCap) return false;
    for (int32_t i = 0; i < a.multisampleCount; ++i)
        if (a.multisampleData[i] != b.multisampleData[i]) return false;

    return true;
}

}  // namespace

extern "C" void RtVideoDeviceStartupProbeArm(bool on) { g_armed = on; }

extern "C" void RtVideoDeviceStartupProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_armed = false;
    g_running = true;

    using Fn = bool (*)(Device*);
    Fn theirs = reinterpret_cast<Fn>(
        ResolveExportRaw("?ValidateConfiguration@Device@Video@@QEAA_NXZ"));
    if (!theirs) {
        printf("[VidCfg] SKIP -- the shipped "
                    "?ValidateConfiguration@Device@Video@@QEAA_NXZ is not reachable "
                    "(cutover, or the DLL is absent). No comparison was made.\n");
        g_running = false;
        return;
    }

    Configuration** slot =
        RT_DATA_SLOT(Configuration**, "?_singleton@Configuration@Video@@0PEAV12@EA");
    if (!slot) {
        printf("[VidCfg] SKIP -- Video::Configuration::_singleton slot is null.\n");
        g_running = false;
        return;
    }
    Configuration* saved = *slot;

    static Device devOurs, devTheirs;
    static Configuration cfgOurs, cfgTheirs;

    int cases = 0, mismatches = 0, retvalMismatch = 0;
    int hitDefaultResolution = 0, hitMsReset = 0, hitFilterClamp = 0;
    int hitDetailClamp = 0, hitRefreshFallback = 0, hitIndexClamp = 0;

    for (const Case& k : kCases) {
        InitDevice(&devOurs);   InitConfig(&cfgOurs, k);
        InitDevice(&devTheirs); InitConfig(&cfgTheirs, k);

        *slot = &cfgOurs;
        const bool ro = RtVideo_Device_ValidateConfiguration(&devOurs);

        *slot = &cfgTheirs;
        const bool rt = theirs(&devTheirs);

        ++cases;
        if (ro != rt) ++retvalMismatch;

        const bool same = SameConfig(cfgOurs, cfgTheirs) && SameDevice(devOurs, devTheirs);
        if (!same) {
            if (mismatches < 4) {
                printf("[VidCfg] MISMATCH %-26s ret=%d/%d\n"
                            "[VidCfg]   ours   if=%d ad=%d %dx%d @%d/%d ms=%d flt=%d "
                            "sh=%d sd=%d rf=%d | mode=%d rr=%d modes=%d/%d ms=%d/%d aniso=%d\n"
                            "[VidCfg]   theirs if=%d ad=%d %dx%d @%d/%d ms=%d flt=%d "
                            "sh=%d sd=%d rf=%d | mode=%d rr=%d modes=%d/%d ms=%d/%d aniso=%d\n",
                            k.name, static_cast<int>(ro), static_cast<int>(rt),
                            cfgOurs.interfaceIndex, cfgOurs.adapterIndex,
                            cfgOurs.width, cfgOurs.height,
                            cfgOurs.refreshNumerator, cfgOurs.refreshDenominator,
                            cfgOurs.multisample, cfgOurs.textureFilter,
                            cfgOurs.shadowScalar, cfgOurs.shaderDetail,
                            cfgOurs.reflectionDetail,
                            devOurs.displayModeIndex, devOurs.refreshRateIndex,
                            devOurs.modeCount, devOurs.modeCapacity,
                            devOurs.multisampleCount, devOurs.multisampleCap,
                            devOurs.maxAnisotropy,
                            cfgTheirs.interfaceIndex, cfgTheirs.adapterIndex,
                            cfgTheirs.width, cfgTheirs.height,
                            cfgTheirs.refreshNumerator, cfgTheirs.refreshDenominator,
                            cfgTheirs.multisample, cfgTheirs.textureFilter,
                            cfgTheirs.shadowScalar, cfgTheirs.shaderDetail,
                            cfgTheirs.reflectionDetail,
                            devTheirs.displayModeIndex, devTheirs.refreshRateIndex,
                            devTheirs.modeCount, devTheirs.modeCapacity,
                            devTheirs.multisampleCount, devTheirs.multisampleCap,
                            devTheirs.maxAnisotropy);
            }
            ++mismatches;
        }

        // ---- EFFECTIVENESS, measured on THEIRS -----------------------------
        // Every counter below reads the SHIPPED body's output, so none of them
        // can be satisfied by our own port agreeing with itself. A case only
        // counts as exercising a branch if retail's result shows the branch
        // ran.
        if (cfgTheirs.width != k.w || cfgTheirs.height != k.h) ++hitDefaultResolution;
        if (cfgTheirs.multisample != k.ms)                     ++hitMsReset;
        if (cfgTheirs.textureFilter != k.filter)               ++hitFilterClamp;
        if (cfgTheirs.shadowScalar != k.shadow ||
            cfgTheirs.shaderDetail != k.shader ||
            cfgTheirs.reflectionDetail != k.reflect)           ++hitDetailClamp;
        if (cfgTheirs.refreshNumerator != k.refN ||
            cfgTheirs.refreshDenominator != k.refD)            ++hitRefreshFallback;
        if (cfgTheirs.interfaceIndex != k.iface ||
            cfgTheirs.adapterIndex != k.adapter)               ++hitIndexClamp;
    }

    // ---- NEGATIVE CONTROL -------------------------------------------------
    // The comparison must be able to SEE a wrong answer. Corrupt one field of
    // our result in a way a plausible transcription slip would produce — a
    // shadowScalar cap of 3 instead of 2 (the shaderDetail cap, one line above
    // it in the disassembly, and the exact copy-paste this function invites) —
    // and confirm the comparator rejects it.
    bool negativeCaught = false;
    {
        InitDevice(&devOurs);   InitConfig(&cfgOurs, kCases[16]);   // detail-all-over-cap
        InitDevice(&devTheirs); InitConfig(&cfgTheirs, kCases[16]);
        *slot = &cfgOurs;
        RtVideo_Device_ValidateConfiguration(&devOurs);
        *slot = &cfgTheirs;
        theirs(&devTheirs);
        cfgOurs.shadowScalar = 3;                                   // the slip
        negativeCaught = !(SameConfig(cfgOurs, cfgTheirs) && SameDevice(devOurs, devTheirs));
    }

    *slot = saved;      // restore on EVERY path, including the failures above

    printf("[VidCfg] cases=%d mismatches=%d return-value-mismatches=%d\n"
                "[VidCfg] effectiveness (all measured on THEIRS): defaultRes=%d "
                "msReset=%d filterClamp=%d detailClamp=%d refreshFallback=%d "
                "indexClamp=%d\n"
                "[VidCfg] negative-control=%s\n",
                cases, mismatches, retvalMismatch,
                hitDefaultResolution, hitMsReset, hitFilterClamp,
                hitDetailClamp, hitRefreshFallback, hitIndexClamp,
                negativeCaught ? "CAUGHT" : "*** MISSED ***");

    if (!hitDefaultResolution || !hitMsReset || !hitFilterClamp ||
        !hitDetailClamp || !hitRefreshFallback || !hitIndexClamp)
        printf("[VidCfg] *** at least one branch counter is ZERO: the sweep did "
                    "not reach that branch and says NOTHING about it ***\n");
    if (!negativeCaught)
        printf("[VidCfg] *** the negative control was NOT caught: the comparator "
                    "cannot see a wrong clamp, so the green cases are void ***\n");

    g_running = false;
}
