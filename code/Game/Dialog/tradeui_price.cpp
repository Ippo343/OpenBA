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
#include "object/Internal.h"   // Object::Base::Type (B.3.8 callers)
#include "object/TypeInfo.h"
#include "Component/ValueDescription.h"   // ENDGAME B.3.9 TR: ValueDescription::TypeId()
#include "Component/TradeDescription.h"   // ENDGAME B.3.9 TR: TradeDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.9 CG2)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Dialog/exe_bodies.h"

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
        if (reinterpret_cast<Object::Base*>(comp)->Type() != typeId) continue;  // B.3.8 bridge
        // Retail stops at the FIRST id match and then confirms; a failed confirm
        // yields null rather than continuing the scan.
        if (!::Object::IsDerivedFromNullSafe(comp, typeId)) return nullptr;
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
    *out = PriceFindComponentByTypeId(resourceEntity, ValueDescription::TypeId());
    return out;
}

// The BANISHED_TRADE_PRICE_PROBE invariant probe was deleted with the flag (ENDGAME C).
// standalone/tool_harness.cpp still calls the report entry point; the stub goes with it.


// ── 0x14005a520 ──────────────────────────────────────────────────────────────
extern "C" int TradeUI_GetResourcePrice_a520(void* tradeState, void* resourceEntity) {
    if (!tradeState || !resourceEntity) return 0;
    const uint8_t* ts = reinterpret_cast<const uint8_t*>(tradeState);
    const int32_t merchantIdx = *reinterpret_cast<const int32_t*>(ts + 0x2c);

    // ── No merchant docked: the base value, straight out of ValueDescription.
    if (merchantIdx < 0) {
        void* vd = PriceFindComponentByTypeId(resourceEntity, ValueDescription::TypeId());
        const int32_t base = vd
            ? *reinterpret_cast<const int32_t*>(reinterpret_cast<uint8_t*>(vd) + 0x40)
            : 0;                                   // retail: *(int*)(0 + 0x40)
        return base;
    }

    // ── Merchant docked: the RawMaterialDescription supplies the category flags.
    void* rawMat = PriceFindComponentByTypeId(resourceEntity, RawMaterialDescription::TypeId());
    const uint32_t flags = rawMat
        ? *reinterpret_cast<const uint32_t*>(reinterpret_cast<uint8_t*>(rawMat) + 0x5c)
        : 0;

    void* tradeDesc = *reinterpret_cast<void* const*>(ts + 0x18);
    if (tradeDesc && !::Object::IsDerivedFromNullSafe(tradeDesc, TradeDescription::TypeId())) {
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
    return result;
}
