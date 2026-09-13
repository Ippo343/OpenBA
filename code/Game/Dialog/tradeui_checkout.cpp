// code/Game/Dialog/tradeui_checkout.cpp
// Wave D — the TradeUI CHECKOUT (buttonTrade, element id 0x4e20).
//
//   0x140081420  TradeUI_OnTradeButton          — the checkout itself
//   0x14005baa0  TradeComponent_RemoveBuyOrder  — its buy-order-list leaf
//   0x14005a490  TradeDescription_ExtendMerchantStay — its merchant-timer leaf
//
// ── What 0x140081420 actually does ───────────────────────────────────────────
// Ghidra names two of its callees "Game_DestroyEntity" (0x140091e60) and
// "Game_RemoveEntity" (0x140091a20). BOTH are mislabels, and believing them
// inverts the meaning of the whole second half of this function. Neither one
// destroys or removes anything:
//
//   0x140091e60 = RawMaterialComponent_RegisterRawMaterial
//                 append-if-absent into townMgr+0x110 (raw-material availability)
//   0x140091a20 = TownMgr_AddAvailableAgri
//                 append-if-absent into townMgr+0x120 (crop/livestock/orchard
//                 availability) — see the block comment above the reimpl in
//                 code/Game/Game/starttown.cpp, which already names FUN_140081420
//                 as the caller that grows this list after start-town seeds it.
//
// So the checkout is a two-phase commit of the panel's pending order, and the buy
// phase is what UNLOCKS newly purchased goods for the town, not a teardown:
//
//   Phase 1 — SELL (self+0x30 array, count +0x38, stride 0x20; qty at entry+0x18)
//     For each row with a non-zero quantity: subtract the goods from the trading
//     post's own StorageComponent, credit the town's per-category produced-goods
//     counter (townMgr+0x170[cat].+0xc), zero the row's spinner and the row's
//     pending quantity. Then add the sell total (read straight off the
//     numberSellTotal element at self+0x100, field +0xd8) to the town's trade
//     credit at townMgr+0x1ac.
//
//   Phase 2 — BUY (self+0x40 array, count +0x48, stride 0x18; qty at entry+0x10)
//     For each row with a non-zero quantity: retire that much of the order from
//     the TradeComponent's buy list, then dispatch on what was bought, by probing
//     the purchased entity's component array for a description type:
//       RawMaterialDescription  -> deliver into the post's storage, and register
//                                  the material as available to the town
//       NaturalResourceDescription -> register as available agriculture
//       LivestockDescription    -> register as available agriculture, and spawn
//                                  the purchased animals into the world
//                                  (0x14005ace0 — see the gate note below)
//     Then zero the row's spinner and pending quantity.
//
//   Phase 3 — refresh the whole panel and both totals.
//
// The tail of retail's function is UpdateBuyTotals (0x140082850) INLINED — same
// component scan, same self+0x40 walk, same write to numberBuyTotal at self+0x108.
// It is called out to the existing port here rather than duplicated.
//
// ── Why this is opt-in ───────────────────────────────────────────────────────
// UPDATE: the one genuinely missing dependency, 0x14005ace0 (the livestock
// spawn), is now ported in code/Game/Component/tradecomponent_livestock.cpp and
// registered, so fn_Building_AddBuyToStorage resolves and all three buy arms are
// complete. Nothing in the checkout is unported any more.
//
// The gate BANISHED_LIVE_TRADE_CHECKOUT remains, because this function mutates
// storage, town resource counters and the town availability pools, and that has
// to be exercised before it can be trusted by default (the CLAUDE.md rule that
// unproven features start outside the default-on table). Once a run shows a purchase
// committing correctly, this moves into the default-on table and the flag becomes a
// kill-switch.
//
// With the gate OFF the click is inert — which is exactly the "pressing Trade
// does nothing" symptom, and is the gate working as designed, not a defect.

#include "Dialog/ui_panels.h"
#include "Component/component_factory_base.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ── Cross-module reimpls this calls ──────────────────────────────────────────
// 0x140091e60 — appends into townMgr+0x110 (code/Game/Component/rawmaterialcomponent.cpp)
extern void RawMaterialComponent_RegisterRawMaterial(void* worldMgr, void* entityChain);
// 0x140091a20 — appends into townMgr+0x120 (code/Game/Game/starttown.cpp)
extern void TownMgr_AddAvailableAgri(uint8_t* townMgr, void* desc);

namespace {

// ── Registered exe leaves resolved on first use ──────────────────────────────
typedef void (*StorageSubtract_t)(void* storageComp, void* entity, int qty);   // 0x14012d150
typedef int  (*GetResourceCategory_t)(void* storageComp, void* entity);        // 0x140088f90

StorageSubtract_t     s_storageSubtract = nullptr;   // 0x14012d150 (reg_action_rawmat)
GetResourceCategory_t s_resourceCategory = nullptr;  // 0x140088f90 (reg_action_rawmat)

// The three purchased-goods description type-ids. These are plain .rdata uint32
// slots (RegisterExeAddr hands back the address OF the uint32), so they take a
// SINGLE deref — unlike the 0x14015xxxx dispatch slots, which take two.
const uint32_t* s_tidRawMaterialDesc = nullptr;      // 0x1401c7cb8 (reg_action_child_ctors2)
const uint32_t* s_tidNaturalResDesc  = nullptr;      // 0x1401c7b68 (reg_economy_typeids)
const uint32_t* s_tidLivestockDesc   = nullptr;      // 0x1401c79a8 (reg_component_animal)

bool s_resolved = false;

void ResolveOnce() {
    if (s_resolved) return;
    s_resolved = true;
    s_storageSubtract    = reinterpret_cast<StorageSubtract_t>(
                               ResolveAddr(Module::Exe, 0x14012d150));
    s_resourceCategory   = reinterpret_cast<GetResourceCategory_t>(
                               ResolveAddr(Module::Exe, 0x140088f90));
    s_tidRawMaterialDesc = reinterpret_cast<const uint32_t*>(
                               ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_tidNaturalResDesc  = reinterpret_cast<const uint32_t*>(
                               ResolveAddr(Module::Exe, 0x1401c7b68));
    s_tidLivestockDesc   = reinterpret_cast<const uint32_t*>(
                               ResolveAddr(Module::Exe, 0x1401c79a8));
}

// Probe a purchased entity's component array for an object of a given type-id.
//
// Retail inlines this scan three times, once per description type. All three
// copies share the same shape: walk entity+0x28 (object-pointer array, count at
// entity+0x30, stride 8), call vtable+0x10 (GetTypeId) on each element, and stop
// at the FIRST element whose type-id matches. Only then is IsDerivedFrom
// consulted; if it says no, retail breaks out of the loop rather than continuing
// the scan, so a null return here reproduces the fall-through to the next
// description type exactly.
void* FindComponentByTypeId(uintptr_t entity, uint32_t typeId) {
    if (!entity) return nullptr;
    void** objs  = *reinterpret_cast<void***>(entity + 0x28);
    int    count = *reinterpret_cast<int*>(entity + 0x30);
    if (!objs) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* obj = objs[i];
        if (!obj) continue;
        // vtable slot +0x10 = GetTypeId()
        uint32_t (*getTypeId)(void*) =
            *reinterpret_cast<uint32_t (**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x10);
        if (getTypeId(obj) != typeId) continue;
        return fn_IsDerivedFrom(obj, typeId) ? obj : nullptr;  // match, derived-or-bust
    }
    return nullptr;
}

}  // namespace

// ============================================================
// TradeComponent_RemoveBuyOrder (0x14005baa0)
// ============================================================
// Retires `qty` from the TradeComponent's pending buy-order list.
//
//   trade+0x50  entry array   (stride 0x10: +0x00 resource entity ptr,
//                              +0x08 unit price int32, +0x0c quantity int32)
//   trade+0x58  entry count
//
// Every entry matching `resourceEntity` has its quantity reduced; an entry that
// reaches exactly zero is erased by shifting the tail down one slot and
// decrementing the count. The outer index is then rewound so the shifted-in
// entry is examined too — retail does this with `i -= 1` before the `i += 1`,
// which is why the loop below re-tests the same index after an erase.
//
// The erase loop in the decompile looks like it copies 16 bytes per step from
// puVar3[2]/puVar3[3]; that is the two-qword move of the NEXT entry into the
// current one, i.e. a plain forward shift.
extern "C" void TradeComponent_RemoveBuyOrder(void* tradeComp, void* resourceEntity, int qty) {
    if (!tradeComp) return;
    uintptr_t tc = reinterpret_cast<uintptr_t>(tradeComp);

    for (int i = 0; i < *reinterpret_cast<int*>(tc + 0x58); ++i) {
        uintptr_t base  = *reinterpret_cast<uintptr_t*>(tc + 0x50);
        uintptr_t entry = base + static_cast<uintptr_t>(i) * 0x10;
        if (*reinterpret_cast<void**>(entry) != resourceEntity) continue;

        int* entryQty = reinterpret_cast<int*>(entry + 0x0c);
        *entryQty -= qty;
        if (*entryQty != 0) continue;

        // Erase: shift every later entry down one slot, then shrink the count.
        int count = *reinterpret_cast<int*>(tc + 0x58);
        for (int j = i; j + 1 < count; ++j) {
            uint64_t* dst = reinterpret_cast<uint64_t*>(base + static_cast<uintptr_t>(j) * 0x10);
            dst[0] = dst[2];
            dst[1] = dst[3];
        }
        *reinterpret_cast<int*>(tc + 0x58) = count - 1;
        --i;  // re-test this index: a different entry now occupies it
    }
}

// ============================================================
// TradeDescription_ExtendMerchantStay (0x14005a490)
// ============================================================
// Called once at the top of the checkout, on the trading post's TradeComponent.
// Reads the post's TradeDescription (component+0x18, type-id 0x1401c4bb8) and
// raises two high-water marks on the town manager (gameWorld+0xf0):
//
//   townMgr+0x1a8 (float) = max(itself, desc+0x68)  — merchant arrival weight
//   townMgr+0x1a0 (Time)  = max(itself, desc+0x6c * 300s)
//
// Both are raise-only. The Time value is built through the engine's
// System::Time::Time(float seconds) ctor, which writes EIGHT bytes through its
// return slot — so the destination below is a full int64, not the int32 the
// field's integer source might suggest.
//
// The multiplier 300.0f is DAT_140172234, read from the image.
extern "C" void TradeDescription_ExtendMerchantStay(void* tradeComp) {
    if (!tradeComp) return;
    uintptr_t tc = reinterpret_cast<uintptr_t>(tradeComp);

    void* desc = *reinterpret_cast<void**>(tc + 0x18);
    uint32_t tidTradeDesc = 0;
    if (uintptr_t slot = ResolveAddr(Module::Exe, 0x1401c4bb8))
        tidTradeDesc = *reinterpret_cast<uint32_t*>(slot);
    // Retail keeps the null and derefs it; standalone would take an access
    // violation here on a mis-typed post, which tells us nothing useful.
    if (!desc || !tidTradeDesc || !fn_IsDerivedFrom(desc, tidTradeDesc)) return;
    uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
                            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!townMgr) return;

    float weight = *reinterpret_cast<float*>(d + 0x68);
    float* dstWeight = reinterpret_cast<float*>(townMgr + 0x1a8);
    if (weight > *dstWeight) *dstWeight = weight;

    if (!fn_Time_Ctor_Float) return;
    int32_t units = *reinterpret_cast<int32_t*>(d + 0x6c);
    int64_t stay = 0;                                  // 8 bytes — the ctor fills all of it
    fn_Time_Ctor_Float(&stay, static_cast<float>(units) * 300.0f);
    int64_t* dstStay = reinterpret_cast<int64_t*>(townMgr + 0x1a0);
    if (*dstStay < stay) *dstStay = stay;
}

// ============================================================
// TradeUI_OnTradeButton (0x140081420) — buttonTrade / element id 0x4e20
// ============================================================
// p2/p3/p4 are the dispatcher's event arguments. Retail reads none of them: the
// only place they could surface is the RefreshDisplay tail call, where the
// compiler happens to forward whatever is left in the argument registers. That
// is register reuse, not an argument — RefreshDisplay's own ported call sites
// pass zeroes, and it ignores them.
extern "C" void TradeUI_OnTradeButton(TradeUI* self, void* /*p2*/, void* /*p3*/, int64_t /*p4*/) {
    if (!self) return;
    ResolveOnce();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // The panel's subject entity (self+0x28), narrowed to a ComponentEntity.
    void* subject = *reinterpret_cast<void**>(s + 0x28);
    void* entity  = fn_IsDerivedFrom(subject, *g_TypeId_ComponentEntity) ? subject : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, entity);

    // The trading post's own TradeComponent and StorageComponent.
    uint64_t tradePtr[2] = {0, 0};
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    fn_FindComponent_Building(reinterpret_cast<void*>(entityPtr[0]), &tradePtr);

    uint64_t storagePtr[2] = {0, 0};
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    fn_FindComponent_Storage(reinterpret_cast<void*>(entityPtr[0]), &storagePtr);

    fn_InternalBasePtr_CheckPtr(&tradePtr);
    TradeDescription_ExtendMerchantStay(reinterpret_cast<void*>(tradePtr[0]));

    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t townMgr   = *reinterpret_cast<uintptr_t*>(gameWorld + 0xf0);

    // OUTCOME counters. A click that commits nothing and a click that commits a full
    // basket look identical from outside this function, and "the button did nothing" is
    // exactly the report this wave was opened on — so the tail below prints what actually
    // moved, not that the handler was entered.
    int soldRows = 0, soldUnits = 0, boughtRows = 0, boughtUnits = 0;
    const int creditBefore = *reinterpret_cast<int*>(townMgr + 0x1ac);

    // ── Phase 1: sell rows ───────────────────────────────────────────────────
    // self+0x30 array, self+0x38 count, stride 0x20.
    //   entry+0x00  purchased/sold resource entity ptr
    //   entry+0x18  pending quantity (int32)
    // self+0x60 is the parallel UI row array (stride 0x20, spinner IBP at +0x18).
    {
        int sellCount = *reinterpret_cast<int*>(s + 0x38);
        for (int i = 0; i < sellCount; ++i) {
            uintptr_t sellBase = *reinterpret_cast<uintptr_t*>(s + 0x30);
            uintptr_t entry    = sellBase + static_cast<uintptr_t>(i) * 0x20;
            int qty = *reinterpret_cast<int*>(entry + 0x18);
            if (qty == 0) continue;
            ++soldRows; soldUnits += qty;

            fn_InternalBasePtr_CheckPtr(&storagePtr);
            void* storage = reinterpret_cast<void*>(storagePtr[0]);
            void* resEntity = *reinterpret_cast<void**>(entry);

            if (s_storageSubtract) s_storageSubtract(storage, resEntity, qty);

            // Credit the town's per-category produced-goods tally.
            //   townMgr+0x170 = category array, stride 0x20, running total at +0x0c
            if (s_resourceCategory) {
                int cat = s_resourceCategory(storage, resEntity);
                if (cat >= 0) {
                    uintptr_t catArray = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
                    int* total = reinterpret_cast<int*>(
                        catArray + static_cast<uintptr_t>(cat) * 0x20 + 0x0c);
                    *total += qty;
                }
            }

            // Zero the row's spinner and its pending quantity.
            uintptr_t uiRow = *reinterpret_cast<uintptr_t*>(s + 0x60)
                            + static_cast<uintptr_t>(i) * 0x20 + 0x18;
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(uiRow));
            fn_SpinnerElement_SetValue(*reinterpret_cast<void**>(uiRow), 0);
            *reinterpret_cast<int*>(
                *reinterpret_cast<uintptr_t*>(s + 0x30)
                + static_cast<uintptr_t>(i) * 0x20 + 0x18) = 0;
        }
    }

    // Bank the sell total. It is read straight off the numberSellTotal element
    // (self+0x100, current value at element+0xd8) rather than recomputed, so the
    // displayed figure and the credited figure cannot disagree.
    fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x100));
    {
        uintptr_t sellTotalElem = *reinterpret_cast<uintptr_t*>(s + 0x100);
        *reinterpret_cast<int*>(townMgr + 0x1ac) +=
            *reinterpret_cast<int*>(sellTotalElem + 0xd8);
    }

    // ── Phase 2: buy rows ────────────────────────────────────────────────────
    // self+0x40 array, self+0x48 count, stride 0x18.
    //   entry+0x00  purchased entity ptr
    //   entry+0x10  pending quantity (int32)
    // self+0x70 is the parallel UI row array (stride 0x20, spinner IBP at +0x10).
    {
        int buyCount = *reinterpret_cast<int*>(s + 0x48);
        for (int i = 0; i < buyCount; ++i) {
            uintptr_t buyBase = *reinterpret_cast<uintptr_t*>(s + 0x40);
            uintptr_t entry   = buyBase + static_cast<uintptr_t>(i) * 0x18;
            int qty = *reinterpret_cast<int*>(entry + 0x10);
            if (qty == 0) continue;
            ++boughtRows; boughtUnits += qty;

            void* bought = *reinterpret_cast<void**>(entry);

            // Retire this much of the order from the post's buy list.
            fn_InternalBasePtr_CheckPtr(&tradePtr);
            TradeComponent_RemoveBuyOrder(reinterpret_cast<void*>(tradePtr[0]), bought, qty);

            uintptr_t boughtEntity = reinterpret_cast<uintptr_t>(bought);

            // Raw material: deliver into the post's storage, and make the
            // material available to the town.
            if (s_tidRawMaterialDesc &&
                FindComponentByTypeId(boughtEntity, *s_tidRawMaterialDesc)) {
                fn_InternalBasePtr_CheckPtr(&storagePtr);
                fn_StorageAddItem(reinterpret_cast<void*>(storagePtr[0]), bought, qty);
                RawMaterialComponent_RegisterRawMaterial(
                    reinterpret_cast<void*>(townMgr), bought);
            }
            // Natural resource (crop / orchard seed): availability only.
            else if (s_tidNaturalResDesc &&
                     FindComponentByTypeId(boughtEntity, *s_tidNaturalResDesc)) {
                TownMgr_AddAvailableAgri(reinterpret_cast<uint8_t*>(townMgr), bought);
            }
            // Livestock: availability, plus spawn the animals into the world.
            else if (s_tidLivestockDesc &&
                     FindComponentByTypeId(boughtEntity, *s_tidLivestockDesc)) {
                TownMgr_AddAvailableAgri(reinterpret_cast<uint8_t*>(townMgr), bought);
                fn_InternalBasePtr_CheckPtr(&tradePtr);
                if (fn_Building_AddBuyToStorage) {
                    fn_Building_AddBuyToStorage(reinterpret_cast<void*>(tradePtr[0]), bought, qty);
                } else {
                    // 0x14005ace0 IS ported now
                    // (code/Game/Component/tradecomponent_livestock.cpp) and
                    // registered in reg_tradeui_checkout.cpp, so this arm survives
                    // only as EXEC-of-0 insurance for a build where that
                    // registration wave is not linked in. The order is still
                    // committed and the animal type is still unlocked for pastures
                    // — only the delivered animals would be missing.
                    static bool s_warned = false;
                    if (!s_warned) {
                        s_warned = true;
                        fprintf(stderr, "[TradeUI] livestock purchased, but the spawn leaf "
                                        "(0x14005ace0) did not resolve — is "
                                        "reg_tradeui_checkout.cpp linked? The order is "
                                        "committed and the type is unlocked, but NO ANIMALS "
                                        "were delivered.\n");
                        fflush(stderr);
                    }
                }
            }

            // Zero the row's spinner and its pending quantity.
            uintptr_t uiRow = *reinterpret_cast<uintptr_t*>(s + 0x70)
                            + static_cast<uintptr_t>(i) * 0x20 + 0x10;
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(uiRow));
            fn_SpinnerElement_SetValue(*reinterpret_cast<void**>(uiRow), 0);
            *reinterpret_cast<int*>(
                *reinterpret_cast<uintptr_t*>(s + 0x40)
                + static_cast<uintptr_t>(i) * 0x18 + 0x10) = 0;
        }
    }

    fprintf(stderr, "[TradeCheckout] committed: sold %d unit(s) over %d row(s), "
                    "bought %d unit(s) over %d row(s), town trade credit %d -> %d\n",
            soldUnits, soldRows, boughtUnits, boughtRows,
            creditBefore, *reinterpret_cast<int*>(townMgr + 0x1ac));
    fflush(stderr);

    // ── Phase 3: repaint ─────────────────────────────────────────────────────
    TradeUI_RefreshDisplay(self, 0, 0, 0);
    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshBuyTable(self);
    TradeUI_UpdateSellTotals(self);
    // Retail inlines UpdateBuyTotals (0x140082850) here rather than calling it;
    // the inlined body is instruction-for-instruction the same walk, so the
    // existing port stands in for it.
    TradeUI_UpdateBuyTotals(self);
}
