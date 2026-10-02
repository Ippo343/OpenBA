// standalone/trade_buy_harness.cpp
//
// BANISHED_TRADE_SELL=<interval frames> — THE VIRTUAL MERCHANT CUSTOMER.
//
// A merchant arrives and docks on its own: the trade controller arms a ~6,583s timer
// at state-2 enter and "[TradeCtrl] event 9: MERCHANT DOCKED" fires on schedule
// (~12,000 frames at 50x with a dock-reachable post). What never happens is the
// TRADE, because in retail a person opens the post's panel, drags the spinners and
// clicks Trade. Nothing in an unattended run does that, so units-traded
// (townMgr+0x1ac, the value stat 13 reads) stays at exactly 0 for the life of the
// world however many merchants come and go.
//
// Six achievements sit behind that one missing click -- Trader (50,000 units),
// MasterTrader (100,000), Exports, Farmer, Food Variety and Foodie -- so this drives
// it, through the real ported path rather than by poking the counter:
//
//   1. find a trading post whose TradeUI panel can be opened (the same EditTool
//      select an in-world click uses, borrowed from the tradepanel harness),
//   2. fill every SELL row's pending quantity from the post's own stock,
//   3. call TradeUI_UpdateSellTotals / _UpdateBuyTotals so the total ELEMENTS carry
//      the numbers the checkout reads (it takes the sell total off the
//      numberSellTotal element at self+0x100 field +0xd8, not from a recompute --
//      writing the row quantities alone would credit nothing),
//   4. call TradeUI_OnTradeButton (retail 0x140081420) — the ported checkout, which
//      does the whole two-phase commit including crediting townMgr+0x1ac.
//
// So the harness supplies the intent a player would supply; every number that moves
// afterwards is computed by ported retail code.
//
// Opt-in, off by default, and it prints on every failure branch — a silent log means
// it never ran, not that it traded nothing.

#include <windows.h>
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Entity/ComponentEntity.h"   // B4: the component array by name
#include "Entity/EntityList.h"        // B4: GameUpdateLoop::m_entityList
#include "Entity/CitizenBehavior.h"   // B4: m_citizenList fields
#include "Component/ComponentDescription.h"
#include "options.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Component/StorageComponent.h"
#include "Component/TradeComponent.h"
#include "Dialog/TradeUI.h"
#include "Action/exe_bodies.h"   // ActRm_StorageAdd_d000 (0x14012d000)

class TradeUI;
void TradeUI_UpdateBuyTotals(TradeUI* self);
// The sell/buy tables are built ONCE when the panel opens. Seeding the trade
// stock afterwards leaves the rows as they were, so the refresh has to be
// re-run explicitly -- otherwise the tree fills and sellRows stays 0 forever.
void TradeUI_RefreshSellTable(TradeUI* self);
void TradeUI_RefreshBuyTable(TradeUI* self);
void TradeUI_UpdateSellTotals(TradeUI* self);
extern "C" void TradeUI_OnTradeButton(TradeUI* self, void* p2, void* p3, int64_t p4);

// edittool.cpp — activate the EditTool and drive the select path the way an
// in-world click does. HarnessSelectEntity is the wrapper (it builds the
// InternalBasePtr the impl expects); calling EditTool_SelectEntity_Impl with a
// bare entity pointer instead faults on a -1 read inside the tool.
extern "C" void* EditTool_HarnessAcquire(void* entityUI);
// tradecomponent_serialize.cpp — insert/top-up one {resource, qty} pair in the
// TradeComponent's +0x60 AVL tree, the tree the sell table is built from.
extern "C" void TradeStock_SeedForTest(void* tradeComp, void* resourceEntity, int qty);
extern "C" int   EditTool_HarnessSelectEntity(void* editTool, void* entity);

namespace {

int  s_interval = -2;      // -2 = env unread, -1 = disabled
int  s_perRow   = 0;       // max units per sell row per visit (0 = whole row)
long long s_totalSold = 0;
int  s_visits   = 0;

void Init() {
    if (s_interval != -2) return;
    const char* e = Opt().tradeSell;
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : 1200;
    if (const char* r = Opt().tradeSellPerRow) s_perRow = std::atoi(r);
    std::fprintf(stderr, "[TradeSell] armed: every %d frames, per-row cap %d "
                         "(0 = sell the whole row)\n", s_interval, s_perRow);
    std::fflush(stderr);
}

Component* FindCompOnEntity(ComponentEntity* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = ent->_componentCount;                          // +0x38
    Object::InternalPtr<Component>* a = ent->_components;        // +0x30
    for (int i = 0; i < n && a; ++i) {
        Component* c = a[i].Get();
        if (!c) continue;
        if (c->Type() == tid) return c;
    }
    return nullptr;
}

}  // namespace

// The panel's row arrays are not filled by the select itself: TradeUI builds them in
// its own per-frame update, so reading self+0x30/+0x38 in the same tick that opened
// the dialog always finds sellCount = 0 however well stocked the post is. Open on
// one tick, act a few ticks later with the dialog still up.
static int s_openedAtFrame = -1;
static const int kSettleFrames = 30;

extern "C" void TradeSellHarness_Tick(void* entityUI, int frameNum) {
    Init();
    if (s_interval <= 0 || frameNum == 0) return;
    if (!entityUI) return;
    const bool openTick = (frameNum % s_interval) == 0;
    const bool actTick  = (s_openedAtFrame >= 0 && frameNum - s_openedAtFrame >= kSettleFrames);
    if (!openTick && !actTick) return;

    GameUpdateLoop* gw = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
    if (!gw) return;
    CitizenBehavior* townMgr = gw->m_citizenList;   // +0xf0
    if (!townMgr) return;

    // Scan the entity list for a trading post (an entity carrying a TradeComponent).
    const uint32_t tidTrade = TradeComponent::TypeId();
    EntityList* entityList = gw->m_entityList;   // +0xe8
    ComponentEntity** arr = entityList ? entityList->masterArray.data : nullptr;   // +0x08
    const int n = entityList ? entityList->masterArray.count : 0;   // +0x10
    if (!arr || n <= 0 || !tidTrade) {
        std::fprintf(stderr, "[TradeSell] f=%d cannot scan (arr=%p n=%d tid=%u)\n",
                     frameNum, reinterpret_cast<void*>(arr), n, tidTrade);
        std::fflush(stderr);
        return;
    }

    ComponentEntity* postEnt = nullptr;
    for (int i = 0; i < n && !postEnt; ++i) {
        ComponentEntity* ent = arr[i];
        if (ent && FindCompOnEntity(ent, tidTrade)) postEnt = ent;
    }
    if (!postEnt) {
        std::fprintf(stderr, "[TradeSell] f=%d no trading post in the world\n", frameNum);
        std::fflush(stderr);
        return;
    }

    void* tool = EditTool_HarnessAcquire(entityUI);
    if (!tool) { std::fprintf(stderr, "[TradeSell] f=%d EditTool activation FAILED\n", frameNum);
                 std::fflush(stderr); return; }
    if (openTick) {
        EditTool_HarnessSelectEntity(tool, postEnt);
        s_openedAtFrame = frameNum;
        return;                     // let the panel populate its rows first
    }
    s_openedAtFrame = -1;
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    if (!dialog) { std::fprintf(stderr, "[TradeSell] f=%d post opened no dialog\n", frameNum);
                   std::fflush(stderr); return; }

    const uint32_t tidTradeUI = TradeUI::TypeId();
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    const int cnt  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    void* tui = nullptr;
    for (int i = 0; ctrlArr && i < cnt; ++i) {
        void* c = ctrlArr[i];
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tidTradeUI) tui = c;   // B.3.8 bridge
    }
    if (!tui) {
        std::fprintf(stderr, "[TradeSell] f=%d panel opened but no TradeUI controller\n", frameNum);
        std::fflush(stderr);
        return;
    }

    const uintptr_t s = reinterpret_cast<uintptr_t>(tui);
    const int creditBefore = townMgr->field_0x1AC;   // +0x1ac

    // Fill the SELL rows. self+0x30 array, count +0x38, stride 0x20, pending qty at
    // entry+0x18 — the same layout the checkout consumes. entry+0x10 is the row's
    // available stock (what the panel shows as sellable); cap the pending quantity to
    // it so the checkout never subtracts more than the post holds.
    int rowsFilled = 0, unitsQueued = 0;
    const int sellCount = *reinterpret_cast<int*>(s + 0x38);
    const uintptr_t sellBase = *reinterpret_cast<uintptr_t*>(s + 0x30);
    // The row is {resEntity, valueDesc, resTypeComp, pendingQty} -- 0x20 bytes, the
    // shape RefreshSellTable pushes. entry+0x10 is resTypeComp, a COMPONENT POINTER,
    // not an available-stock count: reading it as one and skipping rows where it was 0
    // skipped every row whose resource carries no RawMaterialDescription, which is why
    // a table with 7 populated rows still queued nothing. The sellable amount lives in
    // the trade stock tree, and the checkout clamps against the post's storage anyway,
    // so queue a fixed per-row amount and let the ported checkout be the judge.
    const int perRow = (s_perRow > 0) ? s_perRow : 100;
    for (int i = 0; i < sellCount && sellBase; ++i) {
        const uintptr_t entry = sellBase + static_cast<uintptr_t>(i) * 0x20;
        *reinterpret_cast<int*>(entry + 0x18) = perRow;
        unitsQueued += perRow;
        ++rowsFilled;
    }

    if (rowsFilled == 0) {
        // Nothing to sell yet. The post's sell table is built from the post's OWN
        // StorageComponent, and the stress town stocks barns and yards, not the post --
        // in retail a hauler fills it only after the player marks goods for trade, which
        // is a second thing nobody clicks in an unattended run.
        //
        // BANISHED_TRADE_SELL_SEED=<n> puts n of every town raw-material template into
        // the post through AddItem (retail 0x14012d000), the same faithful storage-add
        // the founding-house food seed uses. It stands in for the hauling, not for the
        // trade: the sale itself still goes through the ported checkout on the next tick.
        int seedQty = 0;
        if (const char* sq = Opt().tradeSellSeed) seedQty = std::atoi(sq);
        int seeded = 0;
        if (seedQty > 0) {
            // Seed the TRADE stock, not the post's StorageComponent.
            //
            // TradeUI builds its sell rows from fn_FindComponent_Building(entity) +0x60 --
            // and that finder is MISNAMED: DAT_1401c4b80 is the TradeComponent, so the
            // tree is the post's TRADE stock, not its storage. Seeding the
            // StorageComponent instead (and reading storage+0x60 as a tree) produced a
            // successful-looking AddItem every tick, sellRows stuck at 0, and finally a
            // fault walking a field that was never a tree.
            void* tradeComp = FindCompOnEntity(postEnt, tidTrade);
            ComponentDescription** tmpls = townMgr->field_0x110;   // +0x110
            const int tmplN = townMgr->field_0x118;                 // +0x118
            if (tradeComp && tmpls && tmplN > 0) {
                for (int i = 0; i < tmplN; ++i) {
                    if (!tmpls[i]) continue;
                    TradeStock_SeedForTest(tradeComp, tmpls[i], seedQty);
                    ++seeded;
                }
            }
            // ALSO stock the post's StorageComponent. Exports reads stat 14 filtered to
            // Template\TradingPost.rsc, and stat 14 (dispatcher case 0xe) sums STORAGE
            // quantities, not the trade tree -- so the two seeds serve two different
            // achievements and both are needed. AddItem is retail 0x14012d000, the same
            // faithful storage-add the founding-house food seed uses.
            {
                void* storage = FindCompOnEntity(postEnt, StorageComponent::TypeId());
                if (storage && tmpls) {
                    for (int i = 0; i < tmplN; ++i)
                        if (tmpls[i])
                            ActRm_StorageAdd_d000(reinterpret_cast<uintptr_t>(storage),
                                                  reinterpret_cast<int64_t>(tmpls[i]), seedQty);
                }
            }
            const uintptr_t hdr = reinterpret_cast<uintptr_t>(tradeComp) + 0x60;
            std::fprintf(stderr, "[TradeSell] f=%d seeded %d/%d resource(s) x%d into the "
                                 "TRADE stock (tradeComp=%p tree root=%p count=%d)\n",
                         frameNum, seeded, tmplN, seedQty, tradeComp,
                         tradeComp ? *reinterpret_cast<void**>(hdr) : nullptr,
                         tradeComp ? *reinterpret_cast<int32_t*>(hdr + 8) : -1);
        }
        if (seeded > 0) {
            TradeUI_RefreshSellTable(reinterpret_cast<TradeUI*>(tui));
            TradeUI_RefreshBuyTable(reinterpret_cast<TradeUI*>(tui));
            const int after = *reinterpret_cast<int*>(s + 0x38);
            std::fprintf(stderr, "[TradeSell] f=%d refreshed after seeding: sellRows %d -> %d\n",
                         frameNum, sellCount, after);
            std::fflush(stderr);
            if (after > 0) return;   // next interval sells them
        }
        std::fprintf(stderr, "[TradeSell] f=%d TradeUI=%p sellRows=%d but every row has 0 "
                             "stock — nothing to sell%s\n",
                     frameNum, tui, sellCount,
                     seeded ? " even after refresh" : " (set BANISHED_TRADE_SELL_SEED=<n>)");
        std::fflush(stderr);
        return;
    }

    // The checkout reads the sell TOTAL off the numberSellTotal element, not from the
    // rows, so the totals have to be recomputed before it runs or the credit is 0.
    TradeUI_UpdateSellTotals(reinterpret_cast<TradeUI*>(tui));
    TradeUI_UpdateBuyTotals(reinterpret_cast<TradeUI*>(tui));

    TradeUI_OnTradeButton(reinterpret_cast<TradeUI*>(tui), nullptr, nullptr, 0);

    const int creditAfter = townMgr->field_0x1AC;    // +0x1ac
    s_totalSold += (creditAfter - creditBefore);
    ++s_visits;
    std::fprintf(stderr, "[TradeSell] f=%d visit=%d rows=%d/%d queued=%d units -> credit "
                         "%d -> %d (+%d), cumulative +%lld  [stat 13 reads this]\n",
                 frameNum, s_visits, rowsFilled, sellCount, unitsQueued,
                 creditBefore, creditAfter, creditAfter - creditBefore,
                 (long long)s_totalSold);
    std::fflush(stderr);
}
