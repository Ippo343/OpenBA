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

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct TradeUI;
void TradeUI_UpdateBuyTotals(TradeUI* self);
void TradeUI_UpdateSellTotals(TradeUI* self);
extern "C" void TradeUI_OnTradeButton(TradeUI* self, void* p2, void* p3, int64_t p4);

// edittool.cpp — activate the EditTool and drive the select path the way an
// in-world click does. HarnessSelectEntity is the wrapper (it builds the
// InternalBasePtr the impl expects); calling EditTool_SelectEntity_Impl with a
// bare entity pointer instead faults on a -1 read inside the tool.
extern "C" void* EditTool_HarnessAcquire(void* entityUI);
extern "C" int   EditTool_HarnessSelectEntity(void* editTool, void* entity);

namespace {

int  s_interval = -2;      // -2 = env unread, -1 = disabled
int  s_perRow   = 0;       // max units per sell row per visit (0 = whole row)
long long s_totalSold = 0;
int  s_visits   = 0;

void Init() {
    if (s_interval != -2) return;
    const char* e = std::getenv("BANISHED_TRADE_SELL");
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : 1200;
    if (const char* r = std::getenv("BANISHED_TRADE_SELL_PERROW")) s_perRow = std::atoi(r);
    std::fprintf(stderr, "[TradeSell] armed: every %d frames, per-row cap %d "
                         "(0 = sell the whole row)\n", s_interval, s_perRow);
    std::fflush(stderr);
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindCompOnEntity(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    const uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (vt && vt[2] && reinterpret_cast<uint32_t (*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

}  // namespace

extern "C" void TradeSellHarness_Tick(void* entityUI, int frameNum) {
    Init();
    if (s_interval <= 0 || frameNum == 0 || (frameNum % s_interval) != 0) return;
    if (!entityUI) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xf0);
    if (!townMgr) return;

    // Scan the entity list for a trading post (an entity carrying a TradeComponent).
    const uint32_t tidTrade = Tid("TradeComponent");
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    uintptr_t arr = entityList ? *reinterpret_cast<uintptr_t*>(entityList + 0x08) : 0;
    const int n   = entityList ? *reinterpret_cast<int32_t*>(entityList + 0x10) : 0;
    if (!arr || n <= 0 || !tidTrade) {
        std::fprintf(stderr, "[TradeSell] f=%d cannot scan (arr=%p n=%d tid=%u)\n",
                     frameNum, reinterpret_cast<void*>(arr), n, tidTrade);
        std::fflush(stderr);
        return;
    }

    void* postEnt = nullptr;
    for (int i = 0; i < n && !postEnt; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
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
    const int opened = EditTool_HarnessSelectEntity(tool, postEnt);
    (void)opened;
    void* dialog = *reinterpret_cast<void**>(static_cast<uint8_t*>(tool) + 0x28);
    if (!dialog) { std::fprintf(stderr, "[TradeSell] f=%d post opened no dialog\n", frameNum);
                   std::fflush(stderr); return; }

    const uint32_t tidTradeUI = Tid("TradeUI");
    void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
    const int cnt  = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
    void* tui = nullptr;
    for (int i = 0; ctrlArr && i < cnt; ++i) {
        void* c = ctrlArr[i];
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (vt && vt[2] && reinterpret_cast<uint32_t (*)(void*)>(vt[2])(c) == tidTradeUI) tui = c;
    }
    if (!tui) {
        std::fprintf(stderr, "[TradeSell] f=%d panel opened but no TradeUI controller\n", frameNum);
        std::fflush(stderr);
        return;
    }

    const uintptr_t s = reinterpret_cast<uintptr_t>(tui);
    const int creditBefore = *reinterpret_cast<int*>(townMgr + 0x1ac);

    // Fill the SELL rows. self+0x30 array, count +0x38, stride 0x20, pending qty at
    // entry+0x18 — the same layout the checkout consumes. entry+0x10 is the row's
    // available stock (what the panel shows as sellable); cap the pending quantity to
    // it so the checkout never subtracts more than the post holds.
    int rowsFilled = 0, unitsQueued = 0;
    const int sellCount = *reinterpret_cast<int*>(s + 0x38);
    const uintptr_t sellBase = *reinterpret_cast<uintptr_t*>(s + 0x30);
    for (int i = 0; i < sellCount && sellBase; ++i) {
        const uintptr_t entry = sellBase + static_cast<uintptr_t>(i) * 0x20;
        const int have = *reinterpret_cast<int*>(entry + 0x10);
        if (have <= 0) continue;
        int qty = have;
        if (s_perRow > 0 && qty > s_perRow) qty = s_perRow;
        *reinterpret_cast<int*>(entry + 0x18) = qty;
        unitsQueued += qty;
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
        if (const char* sq = std::getenv("BANISHED_TRADE_SELL_SEED")) seedQty = std::atoi(sq);
        int seeded = 0;
        if (seedQty > 0) {
            typedef void (*AddItem_t)(int64_t storage, int64_t material, int n);
            static AddItem_t addItem = reinterpret_cast<AddItem_t>(
                ResolveAddr(Module::Exe, 0x14012d000));
            void* storage = FindCompOnEntity(postEnt, Tid("StorageComponent"));
            void** tmpls = *reinterpret_cast<void***>(townMgr + 0x110);
            const int tmplN = *reinterpret_cast<int*>(townMgr + 0x118);
            if (addItem && storage && tmpls && tmplN > 0) {
                for (int i = 0; i < tmplN; ++i) {
                    if (!tmpls[i]) continue;
                    addItem(reinterpret_cast<int64_t>(storage),
                            reinterpret_cast<int64_t>(tmpls[i]), seedQty);
                    ++seeded;
                }
            }
            std::fprintf(stderr, "[TradeSell] f=%d seeded %d/%d raw-material template(s) "
                                 "x%d into the post (addItem=%p storage=%p)\n",
                         frameNum, seeded, tmplN, seedQty,
                         reinterpret_cast<void*>(addItem), storage);
        }
        std::fprintf(stderr, "[TradeSell] f=%d TradeUI=%p sellRows=%d but every row has 0 "
                             "stock — nothing to sell%s\n",
                     frameNum, tui, sellCount,
                     seeded ? " YET (seeded this tick; retry next interval)"
                            : " (set BANISHED_TRADE_SELL_SEED=<n> to stock the post)");
        std::fflush(stderr);
        return;
    }

    // The checkout reads the sell TOTAL off the numberSellTotal element, not from the
    // rows, so the totals have to be recomputed before it runs or the credit is 0.
    TradeUI_UpdateSellTotals(reinterpret_cast<TradeUI*>(tui));
    TradeUI_UpdateBuyTotals(reinterpret_cast<TradeUI*>(tui));

    TradeUI_OnTradeButton(reinterpret_cast<TradeUI*>(tui), nullptr, nullptr, 0);

    const int creditAfter = *reinterpret_cast<int*>(townMgr + 0x1ac);
    s_totalSold += (creditAfter - creditBefore);
    ++s_visits;
    std::fprintf(stderr, "[TradeSell] f=%d visit=%d rows=%d/%d queued=%d units -> credit "
                         "%d -> %d (+%d), cumulative +%lld  [stat 13 reads this]\n",
                 frameNum, s_visits, rowsFilled, sellCount, unitsQueued,
                 creditBefore, creditAfter, creditAfter - creditBefore,
                 (long long)s_totalSold);
    std::fflush(stderr);
}
