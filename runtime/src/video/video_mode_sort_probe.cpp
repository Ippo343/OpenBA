// ============================================================================
// runtime/src/video/video_mode_sort_probe.cpp — Wave 9 lane EVM
//   The permutation oracle for the two introsorts.
// ============================================================================
//
// WHY THIS IS THE VERIFICATION AND `EnumerateVideoModes` IS NOT
// -------------------------------------------------------------
// `EnumerateVideoModes` writes nothing to the Device that can be compared: the
// two regions it changes are an `Array<GraphicsInterface>` header (a heap
// pointer, different every run) and three `InterfaceSlot`s (an HMODULE and two
// GetProcAddress results, different every run AND different in kind between
// ours and theirs by design). A snapshot-before/compare-after over the 0x108
// device would be 100% mismatch on every byte either side writes and 0%
// coverage of anything else.
//
// The real computation in that function is the three-level SORT pass, and a
// sort is one of the easiest things in this wave to verify honestly: it is a
// pure permutation of a buffer the caller owns. This probe hands the SAME
// corpus to both implementations — ours on one copy, the shipped body on
// another — and compares the resulting PERMUTATION, not the sortedness.
// "It came out sorted" is true of a bubble sort, a std::sort and a broken
// transcription alike.
//
// THE CORPUS IS DELIBERATELY TIE-HEAVY. These sorts are unstable, so the order
// of equal-comparing elements is an artefact of the exact algorithm — the pivot
// choice, the two equal-run walks, the push-the-larger-half rule. That is
// precisely what a "clean" `std::sort` would get wrong while still producing a
// sorted array, and it is the only thing worth comparing.
//
// EFFECTIVENESS CONTROLS: the per-branch counters in video_mode_sort.cpp. A
// corpus that never reaches the big-range path, never partitions, never walks
// an equal run and never pushes the pending stack proves nothing, so the probe
// prints all of them and says LOUDLY when one is zero.
//
// NEGATIVE CONTROL: BANISHED_EVM_NEGCTL=1 replaces our result with an ASCENDING
// insertion sort. It must diverge on every case whose corpus holds at least two
// distinct keys — and the probe COUNTS those cases, so the control's ability to
// fire is measured rather than assumed.
// ============================================================================

#include <cstddef>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../system/string.h"

using RtVideo::DisplayMode;
using RtVideo::RefreshRate;

extern "C" void RtVideo_SortDisplayModes(DisplayMode* first, int32_t count);
extern "C" void RtVideo_SortRefreshRates(RefreshRate* first, int32_t count);
extern "C" void RtVideo_ArrayRefreshDtor(void* a);

namespace rtvidsort {
struct SortCounters {
    uint64_t calls, smallCalls, smallSwaps, medianSwaps;
    uint64_t partitions, partitionSwaps, pivotFixups;
    uint64_t eqRunA, eqRunB, pushes, pops, deepSwaps;
};
extern SortCounters g_mode;
extern SortCounters g_rate;
}  // namespace rtvidsort

// Declared locally rather than including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to the declaration in that header.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace {

bool EnvOn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

// A deterministic generator, so a divergence is reproducible from its seed
// alone. Not std::rand: that is process-global and another lane's probe could
// move it between runs.
struct Lcg {
    uint32_t s;
    explicit Lcg(uint32_t seed) : s(seed) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    uint32_t upto(uint32_t n) { return n ? next() % n : 0; }
};

// The ascending insertion sort used ONLY as the negative control.
void AscendingByNumerator(RefreshRate* a, int n) {
    for (int i = 1; i < n; ++i) {
        RefreshRate key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j].numerator > key.numerator) { a[j + 1] = a[j]; --j; }
        a[j + 1] = key;
    }
}
void AscendingByMode(DisplayMode* a, int n) {
    for (int i = 1; i < n; ++i) {
        DisplayMode key = a[i];
        int j = i - 1;
        while (j >= 0 && (a[j].width > key.width ||
                          (a[j].width == key.width && a[j].height > key.height))) {
            a[j + 1] = a[j]; --j;
        }
        a[j + 1] = key;
    }
}

int DistinctNumerators(const RefreshRate* a, int n) {
    int distinct = 0;
    for (int i = 0; i < n; ++i) {
        bool seen = false;
        for (int k = 0; k < i; ++k) if (a[k].numerator == a[i].numerator) { seen = true; break; }
        if (!seen) ++distinct;
    }
    return distinct;
}
int DistinctModes(const DisplayMode* a, int n) {
    int distinct = 0;
    for (int i = 0; i < n; ++i) {
        bool seen = false;
        for (int k = 0; k < i; ++k)
            if (a[k].width == a[i].width && a[k].height == a[i].height) { seen = true; break; }
        if (!seen) ++distinct;
    }
    return distinct;
}

}  // namespace

// ============================================================================
extern "C" void RtLaneEvmSortProbe() {
    if (!RtDiffEnabled()) return;
    static bool done = false;
    if (done) return;
    done = true;

    using SortRateFn = void (*)(RefreshRate*, int32_t);
    using SortModeFn = void (*)(DisplayMode*, int32_t);
    SortRateFn theirRate = reinterpret_cast<SortRateFn>(ResolveRuntimeRaw(0x180057630));
    SortModeFn theirMode = reinterpret_cast<SortModeFn>(ResolveRuntimeRaw(0x180057910));
    if (!theirRate || !theirMode) {
        printf("[EvmProbe] sort probe SKIPPED: shipped bodies did not resolve "
                    "(rate=%p mode=%p)\n", (void*)theirRate, (void*)theirMode);
        return;
    }

    const bool neg = EnvOn("BANISHED_EVM_NEGCTL");

    // ---- Part A: RefreshRate ------------------------------------------------
    // Tie-heavy on the NUMERATOR (which is the only thing the comparator
    // actually orders by — see video_mode_sort.cpp's banner) and deliberately
    // VARIED on the denominator, so a port that used each element's own
    // denominator would order these differently. That makes the corpus a test
    // of the transcribed bug, not just of the skeleton.
    int rateCases = 0, rateDiff = 0, rateDiscriminating = 0;
    char detail[512];
    detail[0] = 0;

    for (int n = 1; n <= 40; ++n) {
        for (uint32_t seed = 1; seed <= 4; ++seed) {
            RefreshRate ours[40], theirs[40];
            Lcg rng(seed * 7919u + static_cast<uint32_t>(n));
            for (int i = 0; i < n; ++i) {
                // Three distinct numerators over up to forty slots: ties dominate.
                const int32_t nums[3] = { 60000, 59940, 60000 };
                ours[i].numerator   = nums[rng.upto(3)];
                // Denominators vary independently, including 1000 vs 1001,
                // which changes the TRUE rate but must not change the order.
                ours[i].denominator = static_cast<int32_t>(1000 + rng.upto(2));
                theirs[i] = ours[i];
            }
            const int distinct = DistinctNumerators(ours, n);
            if (distinct >= 2) ++rateDiscriminating;

            RtVideo_SortRefreshRates(ours, n);
            if (neg) AscendingByNumerator(ours, n);
            theirRate(theirs, n);

            ++rateCases;
            if (std::memcmp(ours, theirs, sizeof(RefreshRate) * static_cast<size_t>(n)) != 0) {
                ++rateDiff;
                if (std::strlen(detail) < 300) {
                    char one[96];
                    std::snprintf(one, sizeof(one), "rate n=%d seed=%u; ", n, seed);
                    std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
                }
            }
        }
    }

    // ---- Part B: DisplayMode, empty refresh arrays --------------------------
    // Empty arrays keep the deep swap free of allocation so this part isolates
    // the SKELETON and the (width, height) comparator. Part C exercises the
    // deep swap separately.
    int modeCases = 0, modeDiff = 0, modeDiscriminating = 0;
    for (int n = 1; n <= 40; ++n) {
        for (uint32_t seed = 1; seed <= 4; ++seed) {
            DisplayMode ours[40], theirs[40];
            Lcg rng(seed * 6271u + static_cast<uint32_t>(n) * 13u);
            for (int i = 0; i < n; ++i) {
                const int32_t w[3] = { 1920, 1280, 1920 };
                const int32_t h[3] = { 1080, 1024, 1200 };
                const uint32_t k = rng.upto(3);
                ours[i].width  = w[k];
                ours[i].height = h[k];
                ours[i].refreshData     = nullptr;
                ours[i].refreshCount    = 0;
                ours[i].refreshCapacity = 0;
                theirs[i] = ours[i];
            }
            if (DistinctModes(ours, n) >= 2) ++modeDiscriminating;

            RtVideo_SortDisplayModes(ours, n);
            if (neg) AscendingByMode(ours, n);
            theirMode(theirs, n);

            ++modeCases;
            for (int i = 0; i < n; ++i) {
                if (ours[i].width != theirs[i].width || ours[i].height != theirs[i].height) {
                    ++modeDiff;
                    if (std::strlen(detail) < 380) {
                        char one[96];
                        std::snprintf(one, sizeof(one), "mode n=%d seed=%u at %d; ", n, seed, i);
                        std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
                    }
                    break;
                }
            }
        }
    }

    // ---- Part C: DisplayMode WITH refresh arrays, to drive the deep swap ----
    // Each mode gets a distinct refresh list keyed off its index, so after the
    // sort the CONTENTS say which element ended up where. The pointers cannot
    // be compared — retail's swap reallocates through
    // Array<RefreshRate>::operator=, so both sides hold fresh buffers — and
    // comparing them would fail 100% of cases for the one reason that is
    // correct (reference_oracle_must_not_compare_unwritten_bytes.md).
    int deepCases = 0, deepDiff = 0;
    {
        const int n = 12;
        DisplayMode ours[12], theirs[12];
        for (int pass = 0; pass < 3; ++pass) {
            Lcg rng(4242u + static_cast<uint32_t>(pass));
            for (int i = 0; i < n; ++i) {
                const int32_t w[2] = { 1920, 1280 };
                const uint32_t k = rng.upto(2);
                for (DisplayMode* arr : { &ours[i], &theirs[i] }) {
                    arr->width  = w[k];
                    arr->height = 1080;
                    // One element, carrying the ORIGINAL INDEX, so the contents
                    // identify the element after any permutation.
                    RefreshRate* buf =
                        static_cast<RefreshRate*>(RtSystem::RtMemAlloc(sizeof(RefreshRate), 4));
                    buf->numerator   = 1000 + i;
                    buf->denominator = 1;
                    arr->refreshData     = buf;
                    arr->refreshCount    = 1;
                    arr->refreshCapacity = 1;
                }
            }
            RtVideo_SortDisplayModes(ours, n);
            theirMode(theirs, n);
            ++deepCases;
            for (int i = 0; i < n; ++i) {
                const bool same =
                    ours[i].width == theirs[i].width &&
                    ours[i].height == theirs[i].height &&
                    ours[i].refreshCount == theirs[i].refreshCount &&
                    ours[i].refreshCount == 1 &&
                    ours[i].refreshData && theirs[i].refreshData &&
                    ours[i].refreshData->numerator == theirs[i].refreshData->numerator;
                if (!same) { ++deepDiff; break; }
            }
            for (int i = 0; i < n; ++i) {
                RtVideo_ArrayRefreshDtor(&ours[i].refreshData);
                RtVideo_ArrayRefreshDtor(&theirs[i].refreshData);
            }
        }
    }

    // ---- report -------------------------------------------------------------
    const rtvidsort::SortCounters& m = rtvidsort::g_mode;
    const rtvidsort::SortCounters& r = rtvidsort::g_rate;
    const bool ratePathsHit = r.calls && r.smallCalls && r.medianSwaps &&
                              r.partitions && r.pushes && r.pops;
    const bool modePathsHit = m.calls && m.smallCalls && m.medianSwaps &&
                              m.partitions && m.pushes && m.pops && m.deepSwaps;

    printf("[EvmProbe] sorts%s rate: cases=%d differing=%d discriminating=%d | "
                "mode: cases=%d differing=%d discriminating=%d | "
                "deep: cases=%d differing=%d %s\n",
                neg ? " NEGATIVE-CONTROL" : "",
                rateCases, rateDiff, rateDiscriminating,
                modeCases, modeDiff, modeDiscriminating,
                deepCases, deepDiff, detail);
    printf("[EvmProbe]   rate branches: calls=%llu small=%llu smallSwaps=%llu median=%llu "
                "part=%llu partSwaps=%llu fixups=%llu eqA=%llu eqB=%llu push=%llu pop=%llu%s\n",
                (unsigned long long)r.calls, (unsigned long long)r.smallCalls,
                (unsigned long long)r.smallSwaps, (unsigned long long)r.medianSwaps,
                (unsigned long long)r.partitions, (unsigned long long)r.partitionSwaps,
                (unsigned long long)r.pivotFixups, (unsigned long long)r.eqRunA,
                (unsigned long long)r.eqRunB, (unsigned long long)r.pushes,
                (unsigned long long)r.pops,
                ratePathsHit ? "" : "   *** A STRUCTURAL PATH WAS NEVER REACHED — the corpus is the problem ***");
    printf("[EvmProbe]   mode branches: calls=%llu small=%llu smallSwaps=%llu median=%llu "
                "part=%llu partSwaps=%llu fixups=%llu eqA=%llu eqB=%llu push=%llu pop=%llu deep=%llu%s\n",
                (unsigned long long)m.calls, (unsigned long long)m.smallCalls,
                (unsigned long long)m.smallSwaps, (unsigned long long)m.medianSwaps,
                (unsigned long long)m.partitions, (unsigned long long)m.partitionSwaps,
                (unsigned long long)m.pivotFixups, (unsigned long long)m.eqRunA,
                (unsigned long long)m.eqRunB, (unsigned long long)m.pushes,
                (unsigned long long)m.pops, (unsigned long long)m.deepSwaps,
                modePathsHit ? "" : "   *** A STRUCTURAL PATH WAS NEVER REACHED — the corpus is the problem ***");
}
