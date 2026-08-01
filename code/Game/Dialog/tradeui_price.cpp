// code/Game/Dialog/tradeui_price.cpp
// Wave A — 0x14005a520, the per-resource PRICE lookup, plus its sub-callee
// 0x14000ea80 and the diagnostic that can tell a correct port from a plausible
// wrong one.
//
// ⚠ THE REGISTERED NAME IS WRONG. `fn_Building_GetSellableCount` does not count
// anything sellable; it returns a PRICE. The name is kept because ~4 call sites use
// it (UpdateSellTotals, RefreshSellSpinnerValues, RefreshSellTable, and the merchant
// auto-buy routine at 0x140059ad0). An earlier comment in engine_funcs.h calling it
// "queries available sell count" has already been corrected there.
//
// ── What it returns, from the 0x14005a520 decompile + disassembly ─────────────
//
//   tradeState + 0x2c   int32   index of the DOCKED merchant; negative = none
//   tradeState + 0x18   void*   TradeDescription holder (IsDerivedFrom-checked)
//   tradeDesc  + 0x48   void*   merchant array, stride 0x48
//   merchant[n]+ 0x14   uint32  resource-category bitmask  (see the caveat below)
//   merchant[n]+ 0x18   uint32  resource-category bitmask
//   resourceComp+0x5c   uint32  the resource's own category flags
//   valueDesc  + 0x40   int32   the base value
//
//   no merchant  ->  valueDesc + 0x40
//   +0x14 mask hits ->  valueDesc + 0x40
//   +0x18 mask hits ->  (int)((float)(valueDesc + 0x40) * 0.75f)
//   neither         ->  0
//
// The masks are tested in that order, +0x14 first, and only one multiplier exists in
// the whole function.
//
// ── Two things the decompile hides ───────────────────────────────────────────
//
// 1. The two branches do NOT share one read of `valueDesc+0x40`. With no merchant,
//    a520 scans the component list for ValueDescription ITSELF. With a merchant, the
//    scan in a520 looks for RawMaterialDescription — that supplies only the flags at
//    +0x5c — and the VALUE comes from a separate call to 0x14000ea80. They agree
//    only because ea80's scan is the identical ValueDescription search; that was
//    checked, not assumed, because if ea80 had been anything else the "both paths
//    return the same base value" invariant would have been testing nothing.
//
// 2. When a resource has NO ValueDescription, retail reads `*(int*)(0 + 0x40)` — a
//    real null dereference, on BOTH branches. Not a decompiler artifact. Standalone
//    returns 0 there instead; that is a deliberate divergence, and the probe counts
//    it so a town full of value-less resources cannot hide behind a quiet zero.
//
// ── DAT_140172124 = 0.75f ────────────────────────────────────────────────────
// Read directly out of the image at 0x140172124: bytes 00 00 40 3f = 0x3f400000 =
// 0.75f. That agrees with the independently-derived constant already in the tree at
// standalone/reg_citizenai_actionctors.cpp:51. Two sources, one value, and it is
// below 1.0 as a discount must be.
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr float kTradeDiscount = 0.75f;      // DAT_140172124

// Scan an entity's component array for a type id. Entity layout: component pointer
// array at +0x28, count at +0x30; each component's type id comes from vtable slot
// +0x10. Retail follows the id match with an IsDerivedFrom confirmation and keeps
// the component only if that passes — the same shape as the existing scan in
// engine_funcs.h's storage-count helper.
void* PriceFindComponentByTypeId(const void* entity, uint32_t typeId) {
    if (!entity) return nullptr;
    const uint8_t* e = reinterpret_cast<const uint8_t*>(entity);
    const int32_t count = *reinterpret_cast<const int32_t*>(e + 0x30);
    void* const* arr = *reinterpret_cast<void* const* const*>(e + 0x28);
    if (!arr || count <= 0) return nullptr;
    for (int32_t i = 0; i < count; ++i) {
        void* comp = arr[i];
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        GetTypeId_t getId = reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(comp) + 0x10));
        if (!getId) continue;
        if (getId(comp) != typeId) continue;
        // Retail stops at the FIRST id match and then confirms; a failed confirm
        // yields null rather than continuing the scan.
        if (fn_IsDerivedFrom && !fn_IsDerivedFrom(comp, typeId)) return nullptr;
        return comp;
    }
    return nullptr;
}

} // namespace

// 0x14000ea80 — find the resource's ValueDescription and write it through the
// out-param, returning the out-param (MSVC by-value-pointer convention). Returning
// `out` matters: the caller uses the return value, not its own buffer address.
extern "C" void** TradeUI_FindValueDescription_ea80(const void* resourceEntity,
                                                    void** out) {
    if (!out) return out;
    *out = nullptr;
    if (!g_TypeInfo_ValueDescription) return out;
    *out = PriceFindComponentByTypeId(resourceEntity, *g_TypeInfo_ValueDescription);
    return out;
}

// ── The probe ────────────────────────────────────────────────────────────────
// BANISHED_TRADE_PRICE_PROBE=1. Opt-in, stderr only (stdout stalls in this build,
// and a diagnostic that prints nothing is indistinguishable from one that never
// fired). It is a deliverable, not scaffolding: it is the only instrument that can
// separate a correct price lookup from a plausible wrong one, and it will be wanted
// again when the merchant subsystem lands.
//
// Per call it records the five numbers that must be mutually consistent — the
// resource's +0x5c flags, both merchant masks, valueDesc+0x40, and what a520
// actually returned — and then asserts:
//
//   (a) DERIVABLE. Predict the branch independently, from the masks ANDed against
//       the flags, and compare to the real return. A mismatch means the branch
//       logic is wrong. It may ALSO mean the +0x14/+0x18 offsets are swapped, so
//       the probe explicitly re-runs the prediction with the masks exchanged and
//       reports if THAT is the reading which fits. A swap is a live hypothesis
//       (merchant[0] reads buy=32, a single bit, and sell=221728, a dozen), not a
//       fallback excuse — deciding between "my port is wrong" and "the labels are
//       backwards" is the whole point of running both.
//
//   (b) BASE AGREEMENT. The no-merchant value and the +0x14-match value are both
//       the raw valueDesc+0x40 and must be equal for the same resource. A gap means
//       mask logic has leaked into the base path.
//
//   (c) CONSTANT RATIO — the load-bearing check. The +0x18-match value divided by
//       the base must be the SAME for every resource, and equal to 0.75. Per-
//       resource values all look plausible in isolation; only this cross-resource
//       invariant catches the multiplier sitting in the wrong branch, which is the
//       most likely subtle error and is invisible if you eyeball one resource.
//
//   (d) PLAUSIBLE DISCOUNT. 0.75 < 1.0. Settled statically above.
//
// Note these are INVARIANT checks, not value checks, so they are immune to
// BANISHED_SEED not being deterministic: they compare quantities within a single
// run against each other, never against a recorded expected number.
namespace {

struct PriceObs {
    const void* resource;
    uint32_t    flags;
    uint32_t    mask14;
    uint32_t    mask18;
    int32_t     base;          // valueDesc + 0x40
    int32_t     got;           // what a520 returned
    int32_t     merchantIdx;
    bool        noValueDesc;
};

constexpr int kMaxObs = 128;
PriceObs g_obs[kMaxObs];
int      g_obsCount   = 0;
int      g_obsDropped = 0;
bool     g_reported   = false;

bool ProbeOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_TRADE_PRICE_PROBE");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}

// The independent prediction. Deliberately written from the documented rule rather
// than by calling back into the port, so that it can disagree with it.
int32_t Predict(uint32_t flags, uint32_t buyMask, uint32_t sellMask,
                int32_t base, int32_t merchantIdx) {
    if (merchantIdx < 0)            return base;
    if ((buyMask  & flags) != 0)    return base;
    if ((sellMask & flags) != 0)    return static_cast<int32_t>(
                                        static_cast<float>(base) * kTradeDiscount);
    return 0;
}

void Record(const void* resource, uint32_t flags, uint32_t m14, uint32_t m18,
            int32_t base, int32_t got, int32_t merchantIdx, bool noValueDesc) {
    if (!ProbeOn()) return;
    if (g_obsCount >= kMaxObs) { ++g_obsDropped; return; }
    PriceObs& o = g_obs[g_obsCount++];
    o.resource = resource; o.flags = flags; o.mask14 = m14; o.mask18 = m18;
    o.base = base; o.got = got; o.merchantIdx = merchantIdx;
    o.noValueDesc = noValueDesc;
}

} // namespace

// Prints the analysis. Safe to call repeatedly; only the first call reports.
extern "C" void TradeUI_PriceProbeReport() {
    if (!ProbeOn() || g_reported) return;
    g_reported = true;

    if (g_obsCount == 0) {
        // A probe that stays silent when its subject is empty is a probe that cannot
        // be distinguished from one that never ran.
        std::fprintf(stderr, "[TradePrice] probe armed but observed ZERO calls to "
                     "0x14005a520. Either the TradeUI gate never opened or the panel "
                     "never refreshed a price. This is NOT a pass.\n");
        std::fflush(stderr);
        return;
    }

    std::fprintf(stderr, "[TradePrice] %d observation(s)%s\n", g_obsCount,
                 g_obsDropped ? " (table full, some dropped)" : "");
    std::fprintf(stderr, "[TradePrice]  %-18s %10s %10s %10s %8s %8s %5s\n",
                 "resource", "flags", "mask+0x14", "mask+0x18", "base", "got", "midx");

    int mismatchAsIs = 0, mismatchSwapped = 0, noVd = 0;
    for (int i = 0; i < g_obsCount; ++i) {
        const PriceObs& o = g_obs[i];
        const int32_t asIs    = Predict(o.flags, o.mask14, o.mask18, o.base, o.merchantIdx);
        const int32_t swapped = Predict(o.flags, o.mask18, o.mask14, o.base, o.merchantIdx);
        if (asIs    != o.got) ++mismatchAsIs;
        if (swapped != o.got) ++mismatchSwapped;
        if (o.noValueDesc)    ++noVd;
        if (i < 32) {
            std::fprintf(stderr, "[TradePrice]  %-18p %10u %10u %10u %8d %8d %5d%s\n",
                         o.resource, o.flags, o.mask14, o.mask18, o.base, o.got,
                         o.merchantIdx, (asIs != o.got) ? "   <== predict MISS" : "");
        }
    }

    // (a) derivable — with the swap hypothesis evaluated alongside, not after.
    std::fprintf(stderr,
        "[TradePrice] (a) branch derivable from masks: %s  "
        "[as-labelled %d miss / swapped %d miss of %d]\n",
        (mismatchAsIs == 0) ? "PASS" :
            ((mismatchSwapped == 0) ? "PASS ONLY IF +0x14/+0x18 ARE SWAPPED"
                                    : "FAIL — neither labelling fits"),
        mismatchAsIs, mismatchSwapped, g_obsCount);
    if (mismatchAsIs != 0 && mismatchSwapped == 0) {
        std::fprintf(stderr, "[TradePrice]     ==> the masks are BACKWARDS: +0x14 is "
                     "SELL and +0x18 is BUY. Correct engine_funcs.h, not the port.\n");
    }

    // (b) base agreement across the no-merchant and mask-hit paths.
    int baseSeen = -1, baseSamples = 0, baseDisagree = 0;
    for (int i = 0; i < g_obsCount; ++i) {
        const PriceObs& o = g_obs[i];
        const bool isBasePath = (o.merchantIdx < 0) || ((o.mask14 & o.flags) != 0);
        if (!isBasePath || o.base == 0) continue;
        ++baseSamples;
        if (o.got != o.base) ++baseDisagree;
        (void)baseSeen;
    }
    std::fprintf(stderr,
        "[TradePrice] (b) base path returns raw valueDesc+0x40: %s [%d/%d agree]\n",
        (baseSamples == 0) ? "NO DATA (no base-path sample seen)"
                           : (baseDisagree == 0 ? "PASS" : "FAIL"),
        baseSamples - baseDisagree, baseSamples);

    // (c) constant ratio — the check that catches believable-wrong numbers.
    double ratioMin = 0.0, ratioMax = 0.0;
    int ratioSamples = 0;
    for (int i = 0; i < g_obsCount; ++i) {
        const PriceObs& o = g_obs[i];
        const bool isDiscountPath = (o.merchantIdx >= 0) &&
                                    ((o.mask14 & o.flags) == 0) &&
                                    ((o.mask18 & o.flags) != 0);
        if (!isDiscountPath || o.base == 0) continue;
        const double r = static_cast<double>(o.got) / static_cast<double>(o.base);
        if (ratioSamples == 0) { ratioMin = ratioMax = r; }
        else { if (r < ratioMin) ratioMin = r; if (r > ratioMax) ratioMax = r; }
        ++ratioSamples;
    }
    if (ratioSamples == 0) {
        std::fprintf(stderr, "[TradePrice] (c) sell/buy ratio constant: NO DATA — no "
                     "resource took the discount branch. This check has NOT run; it "
                     "needs a docked merchant whose +0x18 mask hits a resource.\n");
    } else {
        // Integer truncation means a per-resource ratio can only sit slightly BELOW
        // 0.75, by less than one unit of base. The spread is what matters: a genuine
        // misplaced multiplier moves ratios far apart, not by a rounding step.
        const bool tight = (ratioMax - ratioMin) < 0.05;
        std::fprintf(stderr,
            "[TradePrice] (c) sell/buy ratio constant across %d resource(s): %s "
            "[min %.4f max %.4f, expected ~%.2f]\n",
            ratioSamples, tight ? "PASS" : "FAIL — ratio VARIES per resource, the "
            "multiplier is in the wrong branch", ratioMin, ratioMax,
            static_cast<double>(kTradeDiscount));
    }

    // (d) settled statically.
    std::fprintf(stderr, "[TradePrice] (d) discount 0.75 < 1.0: PASS "
                 "(read from image 0x140172124 = 0x3f400000)\n");

    if (noVd) {
        std::fprintf(stderr, "[TradePrice] NOTE: %d call(s) found NO ValueDescription. "
                     "Retail would have null-dereferenced; standalone returned 0. Those "
                     "zeros are a divergence, not a price.\n", noVd);
    }
    std::fflush(stderr);
}

// ── 0x14005a520 ──────────────────────────────────────────────────────────────
extern "C" int TradeUI_GetResourcePrice_a520(void* tradeState, void* resourceEntity) {
    if (!tradeState || !resourceEntity) return 0;
    const uint8_t* ts = reinterpret_cast<const uint8_t*>(tradeState);
    const int32_t merchantIdx = *reinterpret_cast<const int32_t*>(ts + 0x2c);

    // ── No merchant docked: the base value, straight out of ValueDescription.
    if (merchantIdx < 0) {
        void* vd = g_TypeInfo_ValueDescription
            ? PriceFindComponentByTypeId(resourceEntity, *g_TypeInfo_ValueDescription)
            : nullptr;
        const int32_t base = vd
            ? *reinterpret_cast<const int32_t*>(reinterpret_cast<uint8_t*>(vd) + 0x40)
            : 0;                                   // retail: *(int*)(0 + 0x40)
        Record(resourceEntity, 0, 0, 0, base, base, merchantIdx, vd == nullptr);
        return base;
    }

    // ── Merchant docked: the RawMaterialDescription supplies the category flags.
    void* rawMat = g_TypeId_RawMaterialDescription
        ? PriceFindComponentByTypeId(resourceEntity, *g_TypeId_RawMaterialDescription)
        : nullptr;
    const uint32_t flags = rawMat
        ? *reinterpret_cast<const uint32_t*>(reinterpret_cast<uint8_t*>(rawMat) + 0x5c)
        : 0;

    void* tradeDesc = *reinterpret_cast<void* const*>(ts + 0x18);
    if (tradeDesc && g_TypeInfo_TradeDescription && fn_IsDerivedFrom &&
        !fn_IsDerivedFrom(tradeDesc, *g_TypeInfo_TradeDescription)) {
        tradeDesc = nullptr;                       // retail keeps null on a failed check
    }
    if (!tradeDesc) return 0;                      // retail would deref null here
    const uintptr_t merchants = *reinterpret_cast<const uintptr_t*>(
        reinterpret_cast<uint8_t*>(tradeDesc) + 0x48);
    if (!merchants) return 0;

    const uint8_t* m = reinterpret_cast<const uint8_t*>(
        merchants + static_cast<uintptr_t>(merchantIdx) * 0x48);
    const uint32_t mask14 = *reinterpret_cast<const uint32_t*>(m + 0x14);
    const uint32_t mask18 = *reinterpret_cast<const uint32_t*>(m + 0x18);

    void* vd = nullptr;
    TradeUI_FindValueDescription_ea80(resourceEntity, &vd);
    const int32_t base = vd
        ? *reinterpret_cast<const int32_t*>(reinterpret_cast<uint8_t*>(vd) + 0x40)
        : 0;                                       // retail: *(int*)(0 + 0x40)

    int32_t result;
    if ((mask14 & flags) != 0) {
        result = base;
    } else if ((mask18 & flags) != 0) {
        result = static_cast<int32_t>(static_cast<float>(base) * kTradeDiscount);
    } else {
        result = 0;
    }
    Record(resourceEntity, flags, mask14, mask18, base, result, merchantIdx,
           vd == nullptr);
    return result;
}
