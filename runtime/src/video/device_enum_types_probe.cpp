// ============================================================================
// runtime/src/video/device_enum_types_probe.cpp — Wave 18 lane GFXIF
//   The lockstep oracle for the eight byte-comparable rows of the
//   Video::Device::Adapter / ::GraphicsInterface cluster.
// ============================================================================
//
// Opt-in: `BANISHED_RT_VIDENUMTYPES_PROBE=1`.
//
// WHY AN ORACLE AT ALL, WHEN THE BODIES RUN AT BOOT
// -------------------------------------------------
// Once registered, `DeviceData::EnumerateInterfaces` calls all of these through
// `GetImports()` on every boot, so "the game starts" already exercises them.
// That is an INTEGRATION signal and it is not a verdict: it says the bodies did
// not crash, not that they compute what retail computes. A registration is not
// a verdict and an untriaged caller-less port is not evidence of anything.
//
// WHY LOCKSTEP AND NOT `RT_DIFF_SHADOW`
// -------------------------------------
// The ~250 `RT_DIFF_SHADOW` sites in this tree are SELF-CHECKS: our body runs
// for real and a hand-written predicate asserts something about the result. The
// DLL is never consulted, so a shared misunderstanding of retail passes every
// one. Here the authority is the shipped DLL:
//
//     ours runs on subject A, THEIRS runs on subject B, A and B compared.
//
// THEIRS IS REACHED THROUGH `RtOriginal`, NEVER `ResolveExport`
// ------------------------------------------------------------
// This lane registers all nine names. `ResolveExport` is registry-first, so it
// would hand back our own port and the probe would compare ours to ours and
// agree on every case forever — a probe that reaches its own body by the
// registry door is a self-check wearing an oracle's clothes.
//
// AND THEIRS IS FULLY SELF-CONTAINED, WHICH IS THE POINT
// -----------------------------------------------------
// All nine subjects are exported, so `RtOriginal` reaches every one of them.
// Their internal calls to the six unexported `Collection::Array` instantiations
// are in-DLL `rel32`s, which we do not and cannot intercept. So THEIRS runs the
// DLL's array helpers and OURS runs the five this lane ported plus the six lane
// VID2 ported — and the comparison therefore covers all eleven transitively,
// even though not one of them is directly callable by name.
//
// THE COMPARISON IS BY CONTENT, NOT memcmp
// ----------------------------------------
// Every array in this cluster is a separate allocation on the two sides, so
// `_data` MUST be compared by what it points at, never by value. And
// `Adapter::field34` is written by NEITHER implementation, so a memcmp would
// compare bytes nobody writes — which fails 100% of calls and reads like a
// catastrophic port defect.
//
// THE POPULATION
// --------------
// A single well-formed adapter with one mode and one refresh rate would make
// every case take the same path. The cases below are built so each of these
// runs at least once, and the effectiveness counters — ALL MEASURED ON THEIRS,
// so none can be satisfied by our port agreeing with itself — say how often:
//
//   * a String with refcount 1 (the dtor takes the FREE path, and leaves the
//     slot pointing at the freed base) and one with refcount > 1 (the dtor
//     takes the NULL-the-slot path). These are different code, and the
//     difference is one instruction.
//   * a null String (the dtor's early-out)
//   * FindOrAddMode on a mode that is ALREADY present, and one that is not
//     (the SetSize-with-fill path — the three-argument call)
//   * a refresh rate already present, and one that is not
//   * an adapter array that must GROW on assignment, and one that need not
//   * an empty adapter list and a multi-adapter one
//   * a mode with an empty refresh list and one with several
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"

using RtVideo::Adapter;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;
using RtVideo::RefreshRate;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// Ours.
extern "C" void* RtVideo_GraphicsInterface_Ctor(void*);
extern "C" void* RtVideo_GraphicsInterface_CopyCtor(void*, const void*);
extern "C" void  RtVideo_GraphicsInterface_Dtor(void*);
extern "C" void* RtVideo_Adapter_Ctor(void*);
extern "C" void* RtVideo_Adapter_CopyCtor(void*, const void*);
extern "C" void  RtVideo_Adapter_Dtor(void*);
extern "C" void  RtVideo_Adapter_FindOrAddMode(void*, const void*, const void*);
extern "C" void* RtVideo_Device_GetDisplayMode(const void*, void*, int32_t);

namespace {

bool g_armed   = false;
bool g_running = false;

using CtorFn     = void* (*)(void*);
using CopyFn     = void* (*)(void*, const void*);
using DtorFn     = void  (*)(void*);
using FindFn     = void  (*)(void*, const void*, const void*);
using GetModeFn  = void* (*)(const void*, void*, int32_t);

void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// ---------------------------------------------------------------------------
// A System::String built by hand, so the probe controls the REFCOUNT — which is
// the thing that selects between the destructor's two paths. Layout from
// dx11/src/devicedata.cpp:518 and from the inlined dtor at 0x18000cde0:
// allocation base at [-8], refcount at [-4], the String value is the char*.
// ---------------------------------------------------------------------------
void* MakeString(const char* text, int32_t refcount) {
    const size_t len = std::strlen(text);
    uint8_t* base = static_cast<uint8_t*>(MemAlloc(8 + len + 1, 8));
    if (!base) return nullptr;
    std::memset(base, 0, 8);
    *reinterpret_cast<int32_t*>(base + 4) = refcount;
    std::memcpy(base + 8, text, len + 1);
    return base + 8;
}

// ---------------------------------------------------------------------------
// Content comparators. Sizes AND capacities are compared, because capacity is
// where the growth rule shows up and a wrong growth rule is invisible in the
// element values.
// ---------------------------------------------------------------------------
bool SameRefresh(const DisplayMode& a, const DisplayMode& b) {
    if (a.refreshCount != b.refreshCount) return false;
    if (a.refreshCapacity != b.refreshCapacity) return false;
    for (int32_t i = 0; i < a.refreshCount; ++i) {
        if (a.refreshData[i].numerator != b.refreshData[i].numerator) return false;
        if (a.refreshData[i].denominator != b.refreshData[i].denominator) return false;
    }
    return true;
}
bool SameMode(const DisplayMode& a, const DisplayMode& b) {
    return a.width == b.width && a.height == b.height && SameRefresh(a, b);
}
bool SameModeList(const Adapter& a, const Adapter& b) {
    if (a.modeCount != b.modeCount || a.modeCapacity != b.modeCapacity) return false;
    for (int32_t i = 0; i < a.modeCount; ++i)
        if (!SameMode(a.modeData[i], b.modeData[i])) return false;
    return true;
}
bool SameAdapter(const Adapter& a, const Adapter& b) {
    // `name` is SHARED, not deep-copied — both sides adopt the same char* and
    // bump the same refcount — so pointer identity is the right test here and a
    // content compare would pass even if one side forgot to adopt.
    if (a.name != b.name) return false;
    if (a.maxFilterLevel != b.maxFilterLevel) return false;
    if (a.field2C != b.field2C) return false;
    if (a.field30 != b.field30) return false;
    // field34 is written by NEITHER implementation. Not compared, deliberately.
    if (a.multisampleCount != b.multisampleCount) return false;
    if (a.multisampleCap != b.multisampleCap) return false;
    for (int32_t i = 0; i < a.multisampleCount; ++i)
        if (a.multisampleData[i] != b.multisampleData[i]) return false;
    return SameModeList(a, b);
}
bool SameInterface(const GraphicsInterface& a, const GraphicsInterface& b) {
    if (a.name != b.name) return false;
    if (a.adapterCount != b.adapterCount) return false;
    if (a.adapterCapacity != b.adapterCapacity) return false;
    for (int32_t i = 0; i < a.adapterCount; ++i)
        if (!SameAdapter(a.adapterData[i], b.adapterData[i])) return false;
    return true;
}

}  // namespace

extern "C" void RtVideoDeviceEnumTypesProbeArm(bool on) { g_armed = on; }

extern "C" void RtVideoDeviceEnumTypesProbeMaybeRun() {
    if (!g_armed || g_running) return;
    g_running = true;
    g_armed   = false;   // once

    CtorFn    theirAdapterCtor  = RtOriginal<CtorFn>("??0Adapter@Device@Video@@QEAA@XZ");
    CopyFn    theirAdapterCopy  = RtOriginal<CopyFn>("??0Adapter@Device@Video@@QEAA@AEBU012@@Z");
    DtorFn    theirAdapterDtor  = RtOriginal<DtorFn>("??1Adapter@Device@Video@@QEAA@XZ");
    CtorFn    theirGiCtor       = RtOriginal<CtorFn>("??0GraphicsInterface@Device@Video@@QEAA@XZ");
    CopyFn    theirGiCopy       = RtOriginal<CopyFn>("??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z");
    DtorFn    theirGiDtor       = RtOriginal<DtorFn>("??1GraphicsInterface@Device@Video@@QEAA@XZ");
    FindFn    theirFind         = RtOriginal<FindFn>(
        "?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z");
    GetModeFn theirGetMode      = RtOriginal<GetModeFn>(
        "?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z");
    // Not a row of this lane; used only to release the DisplayMode values
    // GetDisplayMode returns, on BOTH sides, so it cannot skew the compare.
    DtorFn    theirModeDtor     = RtOriginal<DtorFn>("??1DisplayMode@Device@Video@@QEAA@XZ");

    // A missing name resolves to NULL in silence and the probe would then run a
    // long way and report a plausible zero. Say so and stop instead.
    if (!theirAdapterCtor || !theirAdapterCopy || !theirAdapterDtor ||
        !theirGiCtor || !theirGiCopy || !theirGiDtor || !theirFind ||
        !theirGetMode || !theirModeDtor) {
        std::printf("[VidEnum] *** at least one shipped export did not resolve; "
                    "the probe did NOT run and its silence means nothing ***\n");
        g_running = false;
        return;
    }

    int cases = 0, mismatches = 0;

    // Effectiveness — every counter reads THEIRS.
    int hitDtorFree = 0, hitDtorShared = 0, hitDtorNull = 0;
    int hitModeAdded = 0, hitModeFound = 0;
    int hitRefreshAdded = 0, hitRefreshFound = 0;
    int hitArrayGrew = 0, hitEmptyAdapters = 0, hitEmptyRefresh = 0;

    // ---- 1. DEFAULT CTORS -------------------------------------------------
    // Both are pure stores into a caller-supplied block, so the whole struct is
    // comparable — including the bytes the ctor does NOT write, because the two
    // blocks are poisoned identically first. That is what makes "+0x34 is left
    // alone" a checkable claim rather than an assumption.
    {
        uint8_t a[0x38], b[0x38];
        std::memset(a, 0x5A, sizeof(a));
        std::memset(b, 0x5A, sizeof(b));
        RtVideo_Adapter_Ctor(a);
        theirAdapterCtor(b);
        ++cases;
        if (std::memcmp(a, b, sizeof(a)) != 0) {
            ++mismatches;
            std::printf("[VidEnum] MISMATCH Adapter::ctor (poisoned block compare)\n");
        }
    }
    {
        uint8_t a[0x18], b[0x18];
        std::memset(a, 0x5A, sizeof(a));
        std::memset(b, 0x5A, sizeof(b));
        RtVideo_GraphicsInterface_Ctor(a);
        theirGiCtor(b);
        ++cases;
        if (std::memcmp(a, b, sizeof(a)) != 0) {
            ++mismatches;
            std::printf("[VidEnum] MISMATCH GraphicsInterface::ctor\n");
        }
    }

    // ---- 2. FindOrAddMode -------------------------------------------------
    // Built with THEIRS on both subjects first, so the two inputs are identical
    // and authored by the authority; then one step applied by ours and by
    // theirs. Cases chosen so the mode is sometimes new and sometimes present,
    // and likewise the refresh rate.
    struct Step { int32_t w, h, num, den; };
    static const Step kSteps[] = {
        {1920, 1080, 60, 1},   // new mode, new rate
        {1920, 1080, 60, 1},   // same mode, same rate  -> both searches hit
        {1920, 1080, 30, 1},   // same mode, new rate
        {1280,  720, 60, 1},   // new mode, rate new for THAT mode
        {1280,  720, 24, 1},
        {2560, 1440, 144, 1},
        {1920, 1080, 24000, 1001},
        {1280,  720, 60, 1},   // both hit again, after the list has grown
    };
    for (size_t s = 0; s < sizeof(kSteps) / sizeof(kSteps[0]); ++s) {
        Adapter ao, at;
        RtVideo_Adapter_Ctor(&ao);
        theirAdapterCtor(&at);

        // Seed both identically, using THEIRS for every seeding step.
        for (size_t k = 0; k < s; ++k) {
            DisplayMode m; std::memset(&m, 0, sizeof(m));
            m.width = kSteps[k].w; m.height = kSteps[k].h;
            RefreshRate r; r.numerator = kSteps[k].num; r.denominator = kSteps[k].den;
            theirFind(&ao, &m, &r);
            theirFind(&at, &m, &r);
        }

        const int32_t modesBefore = at.modeCount;
        int32_t refreshBefore = 0;
        for (int32_t i = 0; i < at.modeCount; ++i)
            if (at.modeData[i].width == kSteps[s].w &&
                at.modeData[i].height == kSteps[s].h)
                refreshBefore = at.modeData[i].refreshCount;

        DisplayMode m; std::memset(&m, 0, sizeof(m));
        m.width = kSteps[s].w; m.height = kSteps[s].h;
        RefreshRate r; r.numerator = kSteps[s].num; r.denominator = kSteps[s].den;

        RtVideo_Adapter_FindOrAddMode(&ao, &m, &r);
        theirFind(&at, &m, &r);
        ++cases;
        if (!SameAdapter(ao, at)) {
            ++mismatches;
            std::printf("[VidEnum] MISMATCH FindOrAddMode step=%zu %dx%d @%d/%d "
                        "(ours modes=%d, theirs modes=%d)\n",
                        s, kSteps[s].w, kSteps[s].h, kSteps[s].num, kSteps[s].den,
                        ao.modeCount, at.modeCount);
        }

        if (at.modeCount > modesBefore) ++hitModeAdded; else ++hitModeFound;
        // The growth rule's amortising branch: capacity strictly above size
        // means SetSize took `capacity = n * 3`, which needs old != 0.
        if (at.modeCapacity > at.modeCount) ++hitArrayGrew;
        for (int32_t i = 0; i < at.modeCount; ++i)
            if (at.modeData[i].width == kSteps[s].w &&
                at.modeData[i].height == kSteps[s].h) {
                if (at.modeData[i].refreshCount > refreshBefore) ++hitRefreshAdded;
                else ++hitRefreshFound;
            }

        // ---- 3. GetDisplayMode over the list this step just built ----------
        // Reached through a Device whose mode array is BORROWED from the
        // adapter: GetDisplayMode reads Device+0x20/+0x28 and nothing else, so
        // pointing those at the adapter's list is a faithful subject and avoids
        // building a second one. The Device is never destroyed, so the borrow
        // cannot double-free.
        {
            Device dOurs, dTheirs;
            std::memset(&dOurs, 0, sizeof(dOurs));
            std::memset(&dTheirs, 0, sizeof(dTheirs));
            dOurs.modeData    = ao.modeData;
            dOurs.modeCount   = ao.modeCount;
            dTheirs.modeData  = at.modeData;
            dTheirs.modeCount = at.modeCount;

            for (int32_t i = 0; i < at.modeCount; ++i) {
                DisplayMode mo, mt;
                std::memset(&mo, 0x5A, sizeof(mo));
                std::memset(&mt, 0x5A, sizeof(mt));
                void* ro = RtVideo_Device_GetDisplayMode(&dOurs, &mo, i);
                void* rt = theirGetMode(&dTheirs, &mt, i);
                ++cases;
                if (ro != &mo || rt != &mt) {
                    ++mismatches;
                    std::printf("[VidEnum] MISMATCH GetDisplayMode return slot i=%d\n", i);
                } else if (!SameMode(mo, mt)) {
                    ++mismatches;
                    std::printf("[VidEnum] MISMATCH GetDisplayMode i=%d "
                                "(ours %dx%d/%d, theirs %dx%d/%d)\n", i,
                                mo.width, mo.height, mo.refreshCount,
                                mt.width, mt.height, mt.refreshCount);
                }
                if (mt.refreshCount == 0) ++hitEmptyRefresh;

                // Each returned DisplayMode OWNS a refresh list (GetDisplayMode
                // deep-copies it), so both must be released or the probe leaks
                // one allocation per index. `??1DisplayMode@Device@Video@@QEAA@XZ`
                // is exported, is not one of this lane's rows, and is used here
                // on BOTH sides so the release cannot itself skew the compare.
                theirModeDtor(&mo);
                theirModeDtor(&mt);
            }
        }
        RtVideo_Adapter_Dtor(&ao);
        theirAdapterDtor(&at);
    }

    // ---- 4. COPY CTOR + DTOR, over the three String refcount cases ---------
    // The dtor's two paths are selected by the refcount, and they differ by one
    // instruction. A population that only ever shares strings would exercise
    // exactly one of them and prove nothing about the other.
    struct StrCase { const char* text; int32_t rc; const char* what; };
    static const StrCase kStrs[] = {
        {"DirectX 11",             1, "refcount 1 -> dtor takes the FREE path"},
        {"DirectX 11 (Display 1)", 4, "refcount 4 -> dtor NULLs the slot"},
        {nullptr,                  0, "null String -> dtor early-outs"},
    };
    for (size_t s = 0; s < sizeof(kStrs) / sizeof(kStrs[0]); ++s) {
        for (int adapters = 0; adapters <= 3; ++adapters) {
            // Source graph, built entirely with THEIRS.
            GraphicsInterface src;
            theirGiCtor(&src);
            if (kStrs[s].text) src.name = MakeString(kStrs[s].text, kStrs[s].rc);

            // Adapters are added by growing the array by hand and running
            // THEIRS' ctor over each new element — Array<Adapter>::SetSize is
            // unexported, so there is no name door to it, and reaching it by
            // address would go through the registry into OUR port and make the
            // source graph partly ours.
            if (adapters > 0) {
                src.adapterData = static_cast<Adapter*>(
                    MemAlloc(static_cast<uint64_t>(adapters) * 0x38, 8));
                src.adapterCount = adapters;
                src.adapterCapacity = adapters;
                for (int i = 0; i < adapters; ++i) {
                    theirAdapterCtor(&src.adapterData[i]);
                    src.adapterData[i].name = MakeString("Adapter", 8);
                    src.adapterData[i].maxFilterLevel = 16;
                    src.adapterData[i].field2C = i;
                    src.adapterData[i].field30 = i * 2;
                    for (int k = 0; k <= i; ++k) {
                        DisplayMode m; std::memset(&m, 0, sizeof(m));
                        m.width = 640 * (k + 1); m.height = 360 * (k + 1);
                        RefreshRate r; r.numerator = 60 + k; r.denominator = 1;
                        theirFind(&src.adapterData[i], &m, &r);
                    }
                }
            }
            if (adapters == 0) ++hitEmptyAdapters;

            // Copy: ours and theirs, from the SAME source.
            GraphicsInterface go, gt;
            std::memset(&go, 0x5A, sizeof(go));
            std::memset(&gt, 0x5A, sizeof(gt));
            void* ro = RtVideo_GraphicsInterface_CopyCtor(&go, &src);
            void* rt = theirGiCopy(&gt, &src);
            ++cases;
            if (ro != &go || rt != &gt) {
                ++mismatches;
                std::printf("[VidEnum] MISMATCH GraphicsInterface::copy return value\n");
            } else if (!SameInterface(go, gt)) {
                ++mismatches;
                std::printf("[VidEnum] MISMATCH GraphicsInterface::copy "
                            "str=%s adapters=%d (ours n=%d cap=%d, theirs n=%d cap=%d)\n",
                            kStrs[s].what, adapters,
                            go.adapterCount, go.adapterCapacity,
                            gt.adapterCount, gt.adapterCapacity);
            }
            // (The tripling branch is NOT reachable from a copy ctor: the
            // destination always starts empty, so `old == 0` and the
            // `n == old + 1 && old != 0` guard is false by construction. It is
            // counted at the FindOrAddMode site instead, which is where it can
            // actually fire. A counter its subject structurally cannot move is
            // not a weak counter, it is a broken one.)

            // Destroy: ours and theirs. The POST-STATE is the subject — retail
            // leaves the name slot pointing at the freed base on the free path
            // and nulls it on the shared path, and those are different values.
            void* nameBefore = go.name;
            RtVideo_GraphicsInterface_Dtor(&go);
            theirGiDtor(&gt);
            ++cases;
            if (go.name != gt.name || go.adapterData != gt.adapterData ||
                go.adapterCount != gt.adapterCount ||
                go.adapterCapacity != gt.adapterCapacity) {
                ++mismatches;
                std::printf("[VidEnum] MISMATCH GraphicsInterface::dtor post-state "
                            "str=%s adapters=%d (ours name=%p, theirs name=%p)\n",
                            kStrs[s].what, adapters, go.name, gt.name);
            }
            if (gt.name == nullptr && nameBefore != nullptr)      ++hitDtorShared;
            else if (gt.name != nullptr)                          ++hitDtorFree;
            else                                                  ++hitDtorNull;

            theirGiDtor(&src);
        }
    }

    // ---- 5. THE DESTRUCTOR'S FREE PATH, on uniquely-owned strings ---------
    // The first pass at this population scored dtorFree=0 and the zero-counter
    // guard caught it. The cause was the POPULATION, not the port: every
    // subject there was copy-constructed from ONE shared source, so the source
    // held a reference the whole time and the refcount could never fall to
    // zero. Two dtors on a source with refcount 1 leave it at 1, not 0.
    //
    // Here each side is given its OWN string with refcount 1, so both dtors
    // take the free path -- the path that decrements to zero, rewinds the
    // pointer by 8, frees, and leaves the slot pointing at the freed base.
    // That last part is the one-instruction difference NC3 is aimed at, and
    // without these cases nothing but the control ever ran it.
    for (int adapters = 0; adapters <= 2; ++adapters) {
        GraphicsInterface go, gt;
        theirGiCtor(&go);
        theirGiCtor(&gt);
        go.name = MakeString("uniquely owned", 1);
        gt.name = MakeString("uniquely owned", 1);

        // Give each its own adapter block too, so the array teardown runs on
        // the free path as well as the String.
        for (int pass = 0; pass < 2; ++pass) {
            GraphicsInterface* g = pass ? &gt : &go;
            if (adapters == 0) continue;
            g->adapterData = static_cast<Adapter*>(
                MemAlloc(static_cast<uint64_t>(adapters) * 0x38, 8));
            g->adapterCount = adapters;
            g->adapterCapacity = adapters;
            for (int i = 0; i < adapters; ++i) {
                theirAdapterCtor(&g->adapterData[i]);
                g->adapterData[i].name = MakeString("owned adapter", 1);
                DisplayMode m; std::memset(&m, 0, sizeof(m));
                m.width = 800; m.height = 600;
                RefreshRate r; r.numerator = 60; r.denominator = 1;
                theirFind(&g->adapterData[i], &m, &r);
            }
        }

        // THE PREDICATE HERE COMPARES THE TRANSFORMATION, NOT THE VALUE — and
        // that is not a convenience, it is the only correct predicate.
        //
        // The first version of this block asked `go.name == gt.name` and
        // reported three mismatches. The port was right and the ORACLE was
        // wrong: each side owns a SEPARATE allocation here (that is the whole
        // point of the case), so the two slots can never hold the same value.
        // A pointer-identity test between two independent allocations is the
        // same defect as memcmp-ing bytes neither side writes — it fails 100%
        // of calls and reads like a catastrophic port defect.
        //
        // What retail actually does on the free path is `slot -= 8` and free.
        // So the checkable claim is that EACH side's slot ends at ITS OWN
        // original pointer minus 8, which is well-defined across two
        // allocations and is exactly the behaviour NC3's slip destroys.
        void* ourNameBefore   = go.name;
        void* theirNameBefore = gt.name;
        RtVideo_GraphicsInterface_Dtor(&go);
        theirGiDtor(&gt);
        ++cases;

        void* ourExpected   = static_cast<uint8_t*>(ourNameBefore) - 8;
        void* theirExpected = static_cast<uint8_t*>(theirNameBefore) - 8;
        const bool oursOk   = (go.name == ourExpected);
        const bool theirsOk = (gt.name == theirExpected);
        if (oursOk != theirsOk || go.adapterData != gt.adapterData ||
            go.adapterCount != gt.adapterCount ||
            go.adapterCapacity != gt.adapterCapacity) {
            ++mismatches;
            std::printf("[VidEnum] MISMATCH dtor FREE path adapters=%d "
                        "(ours slot rewound=%d, theirs slot rewound=%d)\n",
                        adapters, oursOk ? 1 : 0, theirsOk ? 1 : 0);
        }
        // Classified on THEIRS, like every other counter here. `theirsOk` means
        // the shipped dtor took the free path and left the rewound pointer.
        if (theirsOk)                          ++hitDtorFree;
        else if (theirNameBefore != nullptr)   ++hitDtorShared;
        else                                   ++hitDtorNull;
    }

    // ---- 6. GetDisplayMode over a mode with an EMPTY refresh list ----------
    // Also structurally unreachable through FindOrAddMode, which always adds a
    // rate. Built by hand so the nested-array assignment is exercised with
    // nothing to copy -- the n == 0 early shape of the growth rule.
    {
        DisplayMode bare[2];
        std::memset(bare, 0, sizeof(bare));
        bare[0].width = 1024; bare[0].height = 768;
        bare[1].width = 1600; bare[1].height = 900;

        Device dOurs, dTheirs;
        std::memset(&dOurs, 0, sizeof(dOurs));
        std::memset(&dTheirs, 0, sizeof(dTheirs));
        dOurs.modeData = bare;   dOurs.modeCount = 2;
        dTheirs.modeData = bare; dTheirs.modeCount = 2;

        for (int32_t i = 0; i < 2; ++i) {
            DisplayMode mo, mt;
            std::memset(&mo, 0x5A, sizeof(mo));
            std::memset(&mt, 0x5A, sizeof(mt));
            void* ro = RtVideo_Device_GetDisplayMode(&dOurs, &mo, i);
            void* rt = theirGetMode(&dTheirs, &mt, i);
            ++cases;
            if (ro != &mo || rt != &mt || !SameMode(mo, mt)) {
                ++mismatches;
                std::printf("[VidEnum] MISMATCH GetDisplayMode empty-refresh i=%d\n", i);
            }
            if (mt.refreshCount == 0) ++hitEmptyRefresh;
            theirModeDtor(&mo);
            theirModeDtor(&mt);
        }
    }

    // ========================================================================
    // NEGATIVE CONTROLS — four, SCORED SEPARATELY.
    //
    // A control scored on the UNION of several screens reads 3/3 and hides that
    // one screen caught 1/3. Each control below names the ONE screen it is
    // aimed at, is injected by the mechanism it names, and produces a
    // comparable divergence rather than undefined behaviour.
    // ========================================================================
    bool nc1 = false, nc2 = false, nc3 = false, nc4 = false;

    // NC1 -> the poisoned-block ctor compare.
    // The slip: reading "-1 as a QWORD at +0x28, -1 as a DWORD at +0x30" as
    // "-1 as a QWORD at +0x30", which also writes +0x34. This is the exact
    // shape of the mistake the disassembly was read to avoid.
    {
        uint8_t a[0x38], b[0x38];
        std::memset(a, 0x5A, sizeof(a));
        std::memset(b, 0x5A, sizeof(b));
        RtVideo_Adapter_Ctor(a);
        theirAdapterCtor(b);
        *reinterpret_cast<int32_t*>(a + 0x34) = -1;      // the slip
        nc1 = (std::memcmp(a, b, sizeof(a)) != 0);
    }

    // NC2 -> the FindOrAddMode content compare.
    // The slip: reading the SetSize call as two-argument, so the new element is
    // grown but never filled. Injected here by adding the mode with a ZEROED
    // fill, which is what a two-argument call leaves behind on a fresh block.
    {
        Adapter ao, at;
        RtVideo_Adapter_Ctor(&ao);
        theirAdapterCtor(&at);
        DisplayMode real; std::memset(&real, 0, sizeof(real));
        real.width = 1920; real.height = 1080;
        DisplayMode blank; std::memset(&blank, 0, sizeof(blank));
        RefreshRate r; r.numerator = 60; r.denominator = 1;
        RtVideo_Adapter_FindOrAddMode(&ao, &blank, &r);   // the slip: unfilled
        theirFind(&at, &real, &r);
        nc2 = !SameAdapter(ao, at);
        RtVideo_Adapter_Dtor(&ao);
        theirAdapterDtor(&at);
    }

    // NC3 -> the dtor POST-STATE compare, on the free path specifically.
    // The slip: "tidying" the free path by nulling the slot, which is the
    // single most tempting edit in this cluster and the one a byte-faithful
    // port must not make.
    //
    // THIS CONTROL WAS A FALSE GREEN IN ITS FIRST FORM AND IS RECORDED AS SUCH.
    // It read `nc3 = (go.name != gt.name)` — but the two subjects own SEPARATE
    // allocations, so those two pointers differ whether the slip is applied or
    // not. It scored CAUGHT on a run where it could not have scored anything
    // else, which is a control catching by a mechanism other than the one it
    // names. It now uses the SAME transformation predicate the live free-path
    // cases use, and it is verified below to read NOT-caught without the slip.
    {
        GraphicsInterface go, gt;
        theirGiCtor(&go);
        theirGiCtor(&gt);
        go.name = MakeString("x", 1);
        gt.name = MakeString("x", 1);
        void* ourBefore   = go.name;
        void* theirBefore = gt.name;
        RtVideo_GraphicsInterface_Dtor(&go);
        theirGiDtor(&gt);

        const bool theirsOk  = (gt.name == static_cast<uint8_t*>(theirBefore) - 8);
        // Without the slip the predicate must AGREE (both rewound). If this
        // reads true the control is inert and its CAUGHT below means nothing.
        const bool cleanAgree =
            ((go.name == static_cast<uint8_t*>(ourBefore) - 8) == theirsOk);

        go.name = nullptr;                                 // the slip
        const bool slippedAgree =
            ((go.name == static_cast<uint8_t*>(ourBefore) - 8) == theirsOk);

        nc3 = cleanAgree && !slippedAgree;   // agreed clean, rejected slipped
        if (!cleanAgree)
            std::printf("[VidEnum] *** NC3 is INERT: its predicate already "
                        "disagreed before the slip was applied ***\n");
    }

    // NC4 -> the copy-ctor scalar compare.
    // The slip: copying +0x28 and +0x2c but forgetting +0x30 — three adjacent
    // DWORD moves at 0x18000ccd0-0x18000cce2, and dropping the last is the
    // ordinary way to get that wrong.
    {
        Adapter src, ao, at;
        theirAdapterCtor(&src);
        src.maxFilterLevel = 16; src.field2C = 3; src.field30 = 7;
        RtVideo_Adapter_CopyCtor(&ao, &src);
        theirAdapterCopy(&at, &src);
        ao.field30 = -1;                                   // the slip
        nc4 = !SameAdapter(ao, at);
        RtVideo_Adapter_Dtor(&ao);
        theirAdapterDtor(&at);
        theirAdapterDtor(&src);
    }

    std::printf("[VidEnum] cases=%d mismatches=%d\n"
                "[VidEnum] effectiveness (ALL measured on THEIRS): "
                "dtorFree=%d dtorShared=%d dtorNull=%d modeAdded=%d modeFound=%d "
                "refreshAdded=%d refreshFound=%d arrayGrew=%d emptyAdapters=%d "
                "emptyRefresh=%d\n"
                "[VidEnum] negative controls (scored separately): "
                "NC1-ctor=%s NC2-findoraddmode=%s NC3-dtor-freepath=%s "
                "NC4-copyctor=%s\n",
                cases, mismatches,
                hitDtorFree, hitDtorShared, hitDtorNull,
                hitModeAdded, hitModeFound, hitRefreshAdded, hitRefreshFound,
                hitArrayGrew, hitEmptyAdapters, hitEmptyRefresh,
                nc1 ? "CAUGHT" : "*** MISSED ***",
                nc2 ? "CAUGHT" : "*** MISSED ***",
                nc3 ? "CAUGHT" : "*** MISSED ***",
                nc4 ? "CAUGHT" : "*** MISSED ***");

    if (!hitDtorFree || !hitDtorShared || !hitDtorNull || !hitModeAdded ||
        !hitModeFound || !hitRefreshAdded || !hitRefreshFound ||
        !hitArrayGrew || !hitEmptyAdapters || !hitEmptyRefresh)
        std::printf("[VidEnum] *** at least one branch counter is ZERO: the sweep "
                    "did not reach that branch and says NOTHING about it ***\n");
    if (!nc1 || !nc2 || !nc3 || !nc4)
        std::printf("[VidEnum] *** a negative control was NOT caught: the screen it "
                    "names cannot see a wrong answer, so its green cases are void ***\n");

    g_running = false;
}
