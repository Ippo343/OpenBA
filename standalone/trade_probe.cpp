// standalone/trade_probe.cpp
// BANISHED_TRADE_PROBE=<frame> — Wave-0 gating diagnostic for the trading-post /
// livestock-acquisition wave. READ-ONLY: resolves resources and reads fields, never
// spawns, places, or mutates anything.
//
// Answers two questions that decide whether waves B-D are worth building:
//
//   Q1  Does Template\TradingPost.rsc (and Template\TraderBoat.rsc, the merchant
//       entity) resolve at runtime, and does the post carry the TradeDescription +
//       CreatePlacedDescription blocks a placement would need?
//   Q2  Is TradeDescription's merchant data non-empty — the +0x30 merchant-entity
//       template, the +0x48 merchant array (count +0x50, stride 0x48), and the first
//       merchant's goods list (ext-array +0x20, count +0x28)?
//
// Field layout is taken from code/Game/Component/tradedescription.cpp's verified
// Serialize order, and the +0x30 read is corroborated by the retail merchant-spawn
// tick 0x14005aa60, which does ComponentEntity_CopyCtor(mem, *(tradeDesc + 0x30))
// and then reads +0x20 off that template as the entity category.
//
// Unproven diagnostic => strictly OPT-IN. It is deliberately NOT in kDefaultOnFlags;
// an unset env var makes this a no-op. Value "1" picks the default frame 900 (late
// enough that Object::Factory::Startup has assigned real CRC type ids and the world
// is live); any value > 1 overrides the frame.
//
// Per the project's "make periodic diagnostics print when their subject is empty"
// rule, EVERY branch below prints — a resolve failure, a missing description, and a
// zero count each emit a loud line. A silent log from this probe means it never ran,
// never that it passed.
#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>

extern "C" void* StartTown_ResolveTemplateByPath(const char* path);

// WorkPlaceComponent construction census (code/Game/Component/workplacecomponent.cpp).
// Every WPC the standalone exe builds is recorded there, so the sweep below can report
// per-object whether our factory produced it — the runtime test for "is there a second
// construction path?".
extern "C" int  WorkPlaceComponent_CensusCount();
extern "C" bool WorkPlaceComponent_WasFactoryBuilt(const void* p);
extern "C" bool WorkPlaceComponent_CensusComplete();

// Template sub-block (description) list — same offsets starttown.cpp uses
// (DESC_SUB_ARR / DESC_SUB_CNT).
static constexpr int kDescSubArr = 0x28;
static constexpr int kDescSubCnt = 0x30;

static void* FindDescOnTemplate(void* tmpl, uint32_t typeId) {
    if (!tmpl || typeId == 0) return nullptr;
    uint8_t* t = reinterpret_cast<uint8_t*>(tmpl);
    int count = *reinterpret_cast<int32_t*>(t + kDescSubCnt);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(t + kDescSubArr);
    if (!arr || count <= 0) return nullptr;
    for (int i = 0; i < count; ++i) {
        void* desc = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!desc) continue;
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(desc);
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        if ((*reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10))(desc) == typeId) return desc;
    }
    return nullptr;
}

// List every description block on a template, so a missing TradeDescription can be
// told apart from a template that failed to load its sub-blocks at all.
static void DumpTemplateBlocks(const char* label, void* tmpl) {
    if (!tmpl) return;
    uint8_t* t = reinterpret_cast<uint8_t*>(tmpl);
    int count = *reinterpret_cast<int32_t*>(t + kDescSubCnt);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(t + kDescSubArr);
    fprintf(stderr, "[TradeProbe]   %s: %d description block(s), arr=%p\n",
            label, count, reinterpret_cast<void*>(arr));
    if (!arr || count <= 0) {
        fprintf(stderr, "[TradeProbe]   %s: <NO BLOCKS> — template loaded but empty\n", label);
        return;
    }
    for (int i = 0; i < count && i < 32; ++i) {
        void* desc = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!desc) { fprintf(stderr, "[TradeProbe]     [%2d] <null>\n", i); continue; }
        uintptr_t vt = *reinterpret_cast<uintptr_t*>(desc);
        typedef uint32_t (*fn_GetTypeId_t)(void*);
        uint32_t id = (*reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10))(desc);
        fprintf(stderr, "[TradeProbe]     [%2d] desc=%p typeId=0x%08x\n", i, desc, id);
    }
}

// ── WorkPlaceComponent "accepting" sweep ─────────────────────────────────────
// FindAvailableBuilding requires WPC+0x50 (accepting) to be non-zero, so a finished
// building that registers with accepting=0 is unstaffable. Retail asserts it from the
// construction-activation path FUN_14012f520, which tails into Entity_impl_fc00(wpc, 1)
// = WorkPlaceComponent::SetAccepting(true) (0x14012fc00).
//
// There is no PLACED trading post in a default start town — this probe reads the
// TEMPLATE — so the trading post's own value cannot be sampled until a force-place
// harness exists. What we CAN do is sweep every workplace that does exist and report
// the accepting byte, which tells us whether a zero is pasture-specific or general.
//
// WPC layout (as used by standalone/pasture_harness.cpp): +0x18 description,
// desc+0x20 profession key, +0x50 accepting byte.
static void DumpWorkplaceAccepting() {
    typedef void* (*fn_FindWPC_t)(int64_t entity, void* outIBP);
    uintptr_t f2e10 = ResolveAddr(Module::Exe, 0x140002e10);   // Entity_FindComponent_WorkPlace
    if (!f2e10 || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) {
        fprintf(stderr, "[TradeProbe] WPC sweep: unavailable (finder=%p gameWorld=%p)\n",
                reinterpret_cast<void*>(f2e10),
                g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
        return;
    }
    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(gw + 0xe8);
    if (!entityList) {
        fprintf(stderr, "[TradeProbe] WPC sweep: EntityList (gameWorld+0xe8) is NULL\n");
        return;
    }
    // EntityList::masterArray = {data @ +0x08, count @ +0x10} (engine/types/EntityList.h).
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);
    fprintf(stderr, "[TradeProbe] WPC sweep: EntityList=%p masterArray=%p count=%d\n",
            reinterpret_cast<void*>(entityList), reinterpret_cast<void*>(arr), n);
    if (!arr || n <= 0) {
        fprintf(stderr, "[TradeProbe] WPC sweep: <EMPTY ENTITY LIST> — nothing to sample\n");
        return;
    }

    const uint32_t tidTradeComp = ::Object::GetTypeIdByName("TradeComponent");
    int workplaces = 0, accepting1 = 0, accepting0 = 0, tradePosts = 0, foreign = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent) continue;

        uint64_t wpcIBP = 0;
        reinterpret_cast<fn_FindWPC_t>(f2e10)(reinterpret_cast<int64_t>(ent), &wpcIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIBP);
        if (!wpcIBP) continue;

        ++workplaces;
        int      acc     = *reinterpret_cast<uint8_t*>(wpcIBP + 0x50);
        uint64_t desc    = *reinterpret_cast<uint64_t*>(wpcIBP + 0x18);
        uint64_t profKey = desc ? *reinterpret_cast<uint64_t*>(desc + 0x20) : 0;
        if (acc) ++accepting1; else ++accepting0;

        // Is this entity a trading post? (component scan, so it works even though the
        // raw .rdata type-id slot 0x1401c4b80 is unregistered in standalone.)
        bool isTrade = false;
        if (tidTradeComp) {
            int      cN = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ent) + 0x38);
            uintptr_t cA = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uint8_t*>(ent) + 0x30);
            for (int c = 0; c < cN && cA && !isTrade; ++c) {
                void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
                void* comp = *cs;
                if (!comp) continue;
                uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                if ((*reinterpret_cast<fn_GetTypeId_t*>(vt + 0x10))(comp) == tidTradeComp)
                    isTrade = true;
            }
        }
        if (isTrade) ++tradePosts;

        if (!WorkPlaceComponent_WasFactoryBuilt(reinterpret_cast<void*>(wpcIBP))) ++foreign;

        if (workplaces <= 24 || isTrade) {
            fprintf(stderr, "[TradeProbe]   wpc[%2d] ent=%p wpc=%p accepting=%d desc=%p profKey=0x%llx%s%s%s\n",
                    workplaces - 1, ent, reinterpret_cast<void*>(wpcIBP), acc,
                    reinterpret_cast<void*>(desc),
                    static_cast<unsigned long long>(profKey),
                    // Per-object answer to "did OUR factory build this?" — the whole
                    // point of the census in workplacecomponent.cpp. A NOT-OURS line is
                    // the only thing that proves a second construction path exists.
                    WorkPlaceComponent_WasFactoryBuilt(reinterpret_cast<void*>(wpcIBP))
                        ? "" : "  <== NOT BUILT BY OUR FACTORY",
                    isTrade ? "  <== TRADING POST" : "",
                    acc == 0 ? "  <-- UNSTAFFABLE (FindAvailableBuilding skips it)" : "");
            // Raw WPC +0x40..+0x5f: distinguishes "never activated" (all zero) from
            // "activated then cleared". +0x48 is the scheduler stamp FUN_14012f520 writes
            // just before it calls SetAccepting(true), so a zero there means the whole
            // activation path never ran on this workplace.
            const uint8_t* w = reinterpret_cast<const uint8_t*>(wpcIBP);
            char line[128]; int p = 0;
            p += snprintf(line + p, sizeof(line) - p, "      wpc+0x40:");
            for (int b = 0; b < 32; ++b)
                p += snprintf(line + p, sizeof(line) - p, " %02x", w[0x40 + b]);
            fprintf(stderr, "[TradeProbe]%s\n", line);
        }
    }
    fprintf(stderr, "[TradeProbe] WPC sweep: %d workplace(s): accepting=1 -> %d, accepting=0 -> %d\n",
            workplaces, accepting1, accepting0);
    // Construction-path census cross-check. `constructed` counts EVERY WorkPlaceComponent
    // our factory ever built (including any since destroyed, so it can legitimately
    // exceed the live sweep count); `foreign` counts live workplaces our factory did NOT
    // build. foreign=0 means there is no second construction path — the whole question
    // this probe was extended to answer.
    fprintf(stderr, "[TradeProbe] WPC sweep: constructed-by-our-factory=%d (census %s), "
            "live-but-NOT-ours=%d%s\n",
            WorkPlaceComponent_CensusCount(),
            WorkPlaceComponent_CensusComplete() ? "complete" : "OVERFLOWED - negatives unreliable",
            foreign,
            foreign == 0 ? "  <== every live workplace came through our factory"
                         : "  <== SECOND CONSTRUCTION PATH EXISTS");
    if (workplaces == 0)
        fprintf(stderr, "[TradeProbe] WPC sweep: <NO WORKPLACES FOUND> — sweep proves nothing\n");
    fprintf(stderr, "[TradeProbe] WPC sweep: trading posts placed in world = %d%s\n", tradePosts,
            tradePosts == 0 ? "  (none — the post's own accepting byte is UNTESTABLE until a "
                              "force-place harness exists)" : "");
}

extern "C" void TradeProbe_Tick(int frameNum) {
    static int s_frame = -2;              // -2 = env unread, -1 = disabled
    static bool s_done = false;
    if (s_frame == -2) {
        const char* e = std::getenv("BANISHED_TRADE_PROBE");
        if (!e || !e[0] || e[0] == '0') s_frame = -1;
        else {
            int v = std::atoi(e);
            s_frame = (v > 1) ? v : 900;
            fprintf(stderr, "[TradeProbe] armed: one-shot trade/merchant data dump at frame %d\n",
                    s_frame);
            fflush(stderr);
        }
    }
    if (s_frame < 0 || s_done || frameNum < s_frame) return;
    s_done = true;

    fprintf(stderr, "\n[TradeProbe] ===== frame %d: trading-post gating probe =====\n", frameNum);

    // ── Q1: do the resources resolve? ────────────────────────────────────────────
    void* post = StartTown_ResolveTemplateByPath("Template\\TradingPost.rsc");
    void* boat = StartTown_ResolveTemplateByPath("Template\\TraderBoat.rsc");
    fprintf(stderr, "[TradeProbe] Q1 Template\\TradingPost.rsc -> %p  %s\n",
            post, post ? "OK" : "<<< DID NOT RESOLVE");
    fprintf(stderr, "[TradeProbe] Q1 Template\\TraderBoat.rsc  -> %p  %s\n",
            boat, boat ? "OK" : "<<< DID NOT RESOLVE");
    if (!post) {
        fprintf(stderr, "[TradeProbe] Q1 VERDICT: NO-GO — the trading-post template does not "
                        "load; nothing downstream is testable.\n");
        fprintf(stderr, "[TradeProbe] ===== end =====\n\n");
        fflush(stderr);
        return;
    }

    DumpTemplateBlocks("TradingPost", post);
    if (boat) DumpTemplateBlocks("TraderBoat", boat);

    const uint32_t tidTrade  = ::Object::GetTypeIdByName("TradeDescription");
    const uint32_t tidPlaced = ::Object::GetTypeIdByName("CreatePlacedDescription");
    fprintf(stderr, "[TradeProbe] typeIds: TradeDescription=0x%08x CreatePlacedDescription=0x%08x\n",
            tidTrade, tidPlaced);

    void* placed = FindDescOnTemplate(post, tidPlaced);
    if (placed) {
        int w = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(placed) + 0x44);
        int h = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(placed) + 0x48);
        fprintf(stderr, "[TradeProbe] Q1 CreatePlacedDescription=%p footprint=%dx%d "
                        "(placement metadata present)\n", placed, w, h);
    } else {
        fprintf(stderr, "[TradeProbe] Q1 CreatePlacedDescription: <NOT FOUND> — the post has no "
                        "placement block; a force-place harness cannot size it.\n");
    }

    // ── Q2: is the merchant data non-empty? ──────────────────────────────────────
    void* td = FindDescOnTemplate(post, tidTrade);
    if (!td) {
        fprintf(stderr, "[TradeProbe] Q2 TradeDescription: <NOT FOUND on TradingPost> — either "
                        "the type id is wrong or the block did not deserialize.\n");
        fprintf(stderr, "[TradeProbe] Q2 VERDICT: NO-GO\n");
        fprintf(stderr, "[TradeProbe] ===== end =====\n\n");
        fflush(stderr);
        return;
    }
    uint8_t* d = reinterpret_cast<uint8_t*>(td);
    fprintf(stderr, "[TradeProbe] Q2 TradeDescription=%p\n", td);

    void* merchantTmpl = *reinterpret_cast<void**>(d + 0x30);
    fprintf(stderr, "[TradeProbe] Q2 +0x30 merchant entity template = %p  %s\n",
            merchantTmpl, merchantTmpl ? "OK (aa60 clones this)"
                                       : "<<< NULL — aa60 would clone nothing");
    if (merchantTmpl) {
        // NB: +0x30 is a ComponentEntity (aa60 CopyCtors it), NOT a description
        // template — so read the ENTITY component array (+0x30 arr / +0x38 cnt),
        // not the template sub-block offsets used above.
        uint8_t*  me   = reinterpret_cast<uint8_t*>(merchantTmpl);
        int       cat  = *reinterpret_cast<int32_t*>(me + 0x20);
        uintptr_t cArr = *reinterpret_cast<uintptr_t*>(me + 0x30);
        int       cCnt = *reinterpret_cast<int32_t*>(me + 0x38);
        fprintf(stderr, "[TradeProbe] Q2 +0x30 -> +0x20 entity category = %d "
                        "(aa60 passes this to EntityList_RegisterEntity)\n", cat);
        fprintf(stderr, "[TradeProbe] Q2 +0x30 merchant entity: %d component(s), arr=%p%s\n",
                cCnt, reinterpret_cast<void*>(cArr),
                (cCnt <= 0 || !cArr) ? "   <-- NO COMPONENTS (no BoatComponent to route)" : "");
    }

    uintptr_t mArr = *reinterpret_cast<uintptr_t*>(d + 0x48);
    int       mCnt = *reinterpret_cast<int32_t*>(d + 0x50);
    fprintf(stderr, "[TradeProbe] Q2 +0x48 merchant array = %p, +0x50 count = %d\n",
            reinterpret_cast<void*>(mArr), mCnt);
    if (!mArr || mCnt <= 0) {
        fprintf(stderr, "[TradeProbe] Q2 MERCHANT ARRAY IS EMPTY — there is nothing to buy; "
                        "waves B-D would build an inert chain.\n");
        fprintf(stderr, "[TradeProbe] Q2 VERDICT: NO-GO\n");
        fprintf(stderr, "[TradeProbe] ===== end =====\n\n");
        fflush(stderr);
        return;
    }

    // Raw hex of the first two elements. The derived-field reads below depend on a
    // layout recovered from FUN_14000f550; dumping the bytes makes a bad offset
    // distinguishable from genuinely empty data instead of silently reporting "0".
    for (int i = 0; i < mCnt && i < 2; ++i) {
        const uint8_t* m = reinterpret_cast<const uint8_t*>(mArr + static_cast<uintptr_t>(i) * 0x48);
        fprintf(stderr, "[TradeProbe] Q2   raw merchant[%d] @%p:\n", i, static_cast<const void*>(m));
        for (int row = 0; row < 0x48; row += 16) {
            char line[128]; int p = 0;
            p += snprintf(line + p, sizeof(line) - p, "      +0x%02x:", row);
            for (int b = 0; b < 16 && row + b < 0x48; ++b)
                p += snprintf(line + p, sizeof(line) - p, " %02x", m[row + b]);
            fprintf(stderr, "[TradeProbe]%s\n", line);
        }
    }

    // Per-merchant: ExternalPtr@+0x00, String@+0x08, enums@+0x10/+0x14/+0x18,
    // goods ext-array@+0x20 count@+0x28, int-array@+0x30 count@+0x38, int@+0x40.
    // (All verified against FUN_14000f550's uint*-relative indices; stride 0x48 is
    // confirmed by TradeComponent_impl_f370's MemAlloc(cap * 0x48).)
    int totalGoods = 0;
    for (int i = 0; i < mCnt && i < 16; ++i) {
        uint8_t* m = reinterpret_cast<uint8_t*>(mArr + static_cast<uintptr_t>(i) * 0x48);
        void*    ext      = *reinterpret_cast<void**>(m + 0x00);
        uintptr_t goodsA  = *reinterpret_cast<uintptr_t*>(m + 0x20);
        int       goodsN  = *reinterpret_cast<int32_t*>(m + 0x28);
        uintptr_t intA    = *reinterpret_cast<uintptr_t*>(m + 0x30);
        int       intN    = *reinterpret_cast<int32_t*>(m + 0x38);
        int       tail    = *reinterpret_cast<int32_t*>(m + 0x40);
        uint32_t  e10     = *reinterpret_cast<uint32_t*>(m + 0x10);
        uint32_t  e14     = *reinterpret_cast<uint32_t*>(m + 0x14);
        uint32_t  e18     = *reinterpret_cast<uint32_t*>(m + 0x18);
        totalGoods += (goodsN > 0) ? goodsN : 0;
        fprintf(stderr, "[TradeProbe] Q2   merchant[%d] ext=%p goods=%d (arr=%p) ints=%d (arr=%p) "
                        "enums={%u,%u,%u} tail=%d%s\n",
                i, ext, goodsN, reinterpret_cast<void*>(goodsA), intN,
                reinterpret_cast<void*>(intA), e10, e14, e18, tail,
                (goodsN <= 0) ? "   <-- NO GOODS" : "");
        // First merchant: list its goods entries so we can see they are real refs.
        if (i == 0 && goodsA && goodsN > 0) {
            for (int g = 0; g < goodsN && g < 16; ++g) {
                void* item = *reinterpret_cast<void**>(goodsA + static_cast<uintptr_t>(g) * 8);
                fprintf(stderr, "[TradeProbe] Q2     merchant[0].good[%2d] = %p%s\n",
                        g, item, item ? "" : "  <-- null ref");
            }
        }
    }
    fprintf(stderr, "[TradeProbe] Q2 total goods entries across %d merchant(s) = %d\n",
            mCnt, totalGoods);

    // ── TradeUI dependency audit, verified at RUNTIME ───────────────────────────
    // The promotion in ui_dialog_stub_factories.cpp gates itself on these, but its
    // DepsReady() only runs on first slot dispatch — which needs someone to OPEN the
    // panel, and the autostart path never clicks a building. Reporting the same
    // pointers here checks the static audit against the running process without
    // depending on panel construction.
    {
        struct Dep { const char* name; const void* fn; };
        const Dep deps[] = {
            { "fn_Building_GetSellableCount (0x14005a520)", (const void*)fn_Building_GetSellableCount },
            { "fn_Building_GetBuyEntries    (0x140059670)", (const void*)fn_Building_GetBuyEntries },
            { "fn_Building_GetOrderName     (0x140015fd0)", (const void*)fn_Building_GetOrderName },
            { "fn_Building_AddPurchaseOrder (0x140059490)", (const void*)fn_Building_AddPurchaseOrder },
            { "fn_DynArray_Clear_0x20       (0x140016050)", (const void*)fn_DynArray_Clear_0x20 },
            { "fn_DynArray_Clear_0x18_Buy   (0x140016130)", (const void*)fn_DynArray_Clear_0x18_Buy },
            { "fn_DynArray_Clear_0x18       (0x140016200)", (const void*)fn_DynArray_Clear_0x18 },
            { "fn_IntroSort_ByComponent     (0x1400165d0)", (const void*)fn_IntroSort_ByComponent },
            { "fn_IntroSort_ByName          (0x1400162d0)", (const void*)fn_IntroSort_ByName },
            { "fn_StorageTree_Insert        (0x14000e1c0)", (const void*)fn_StorageTree_Insert },
            { "fn_Element_SetNeedsSizeCalc",                (const void*)fn_Element_SetNeedsSizeCalc },
            { "fn_TabElement_SetActiveTab",                 (const void*)fn_TabElement_SetActiveTab },
            { "fn_UIString_Ctor",                           (const void*)fn_UIString_Ctor },
            { "fn_Sprite_Ctor",                             (const void*)fn_Sprite_Ctor },
        };
        int missing = 0;
        fprintf(stderr, "[TradeProbe] TradeUI dependency audit (gate needs ALL non-null):\n");
        for (const Dep& d : deps) {
            if (!d.fn) ++missing;
            fprintf(stderr, "[TradeProbe]   %-46s = %p%s\n", d.name, d.fn,
                    d.fn ? "" : "   <-- NULL");
        }
        fprintf(stderr, "[TradeProbe] TradeUI gate: %d/%d resolved -> panel would %s\n",
                (int)(sizeof(deps)/sizeof(deps[0])) - missing,
                (int)(sizeof(deps)/sizeof(deps[0])),
                missing ? "STAY STUBBED" : "GO LIVE");
        // Wave D only; null-guarded at its call site, deliberately outside the gate.
        fprintf(stderr, "[TradeProbe]   fn_TradeUI_OnTradeButton (0x140081420, Wave D, "
                "NOT gated) = %p\n", (const void*)fn_TradeUI_OnTradeButton);
    }

    // ── River polyline availability (timing check for BUILD_DEMO_TRADINGPOST) ──
    // The force-place harness reads worldGeom+0x1e0 during StartTown_SpawnStructures
    // (world-build time) and sees pathCount=4 with every entry {pts=NULL, n=0}. Re-read
    // the SAME array here, at frame 900, to tell "the polylines are generated later"
    // apart from "we are reading the wrong field".
    {
        uintptr_t gw2 = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                      ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
        uintptr_t wgeom = gw2 ? *reinterpret_cast<uintptr_t*>(gw2 + 0x110) : 0;
        int    pc = wgeom ? *reinterpret_cast<int32_t*>(wgeom + 0x1e8) : -1;
        uint8_t* pa = wgeom ? *reinterpret_cast<uint8_t**>(wgeom + 0x1e0) : nullptr;
        fprintf(stderr, "[TradeProbe] river check @frame %d: worldGeom=%p pathCount=%d "
                "pathArr=%p\n", frameNum, reinterpret_cast<void*>(wgeom), pc, (void*)pa);
        int nonEmpty = 0;
        for (int i = 0; i < pc && pa && i < 8; ++i) {
            uint8_t* e = pa + static_cast<size_t>(i) * 0x10;
            const float* pts = *reinterpret_cast<const float**>(e);
            int n = *reinterpret_cast<int32_t*>(e + 8);
            if (pts && n > 0) ++nonEmpty;
            fprintf(stderr, "[TradeProbe]   path[%d] pts=%p n=%d", i, (const void*)pts, n);
            if (pts && n > 0)
                for (int k = 0; k < n && k < 3; ++k)
                    fprintf(stderr, "  (%.2f,%.2f)", pts[2*k], pts[2*k+1]);
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "[TradeProbe] river check: %d/%d path(s) populated%s\n",
                nonEmpty, pc > 0 ? pc : 0,
                nonEmpty == 0 ? "  <-- STILL EMPTY at frame 900: this array is not the "
                                "river polylines, or is never filled in standalone"
                              : "  <-- populated later than world build: the force-place "
                                "harness must run at a frame, not in StartTown");
    }

    // ── WorkPlaceComponent accepting sweep (see DumpWorkplaceAccepting). ────────
    DumpWorkplaceAccepting();

    // ── Confirm the two known-NULL fn ptrs are still unregistered (do not fix). ──
    // Read the ALREADY-RESOLVED engine_funcs pointers rather than calling ResolveAddr
    // again: a fresh ResolveAddr on an unregistered address makes the registry emit
    // "[STANDALONE] NOT IMPLEMENTED: exe function at 0x...", which reads as an error to
    // anyone grepping the log even though it is the expected Wave-0 state. (The earlier
    // 0x14005c650 probe did exactly that and has been dropped — its unregistered status
    // is established statically and does not need a runtime check.)
    fprintf(stderr, "[TradeProbe] registry check (expect both null at Wave 0): "
                    "fn_Building_AddBuyToStorage(0x14005ace0)=%p  "
                    "fn_TradeUI_OnTradeButton(0x140081420)=%p\n",
            reinterpret_cast<void*>(fn_Building_AddBuyToStorage),
            reinterpret_cast<void*>(fn_TradeUI_OnTradeButton));

    // IMPORTANT — do NOT gate the verdict on totalGoods. The per-merchant goods
    // ExternalPtr array (+0x20/+0x28) is empty in retail data, and that is CORRECT:
    // the goods a player can actually buy are never read from the description at
    // arrival. TradeDescription_helper_sub_9ad0 (0x140059ad0, the state-0x13 "merchant
    // docked" handler) walks the *TradeComponent's* item array (+0x50 base / +0x58
    // count, stride 0x10) and buy-order array (+0x40/+0x48, stride 0xc) — a different
    // object, populated at runtime. An earlier revision of this probe reported NO-GO
    // off totalGoods and was measuring the wrong thing.
    // What Wave C actually needs from the description is the merchant ENTITY template
    // (+0x30) and a non-empty merchant-type list (+0x48/+0x50); those are the gate.
    const bool go = (merchantTmpl != nullptr) && (mCnt > 0);
    fprintf(stderr, "[TradeProbe] Q2 NOTE: per-merchant goods arrays (+0x20/+0x28) are expected "
                    "EMPTY — the buyable manifest lives on the TradeComponent (+0x50/+0x58) and "
                    "is filled at runtime, not in this description.\n");
    fprintf(stderr, "[TradeProbe] Q2 VERDICT: %s\n",
            go ? "GO — merchant entity template + merchant-type list are present"
               : "NO-GO — merchant template and/or merchant-type list are missing");
    fprintf(stderr, "[TradeProbe] ===== end =====\n\n");
    fflush(stderr);
}
