// standalone/orchard_diag.cpp
// Periodic OrchardComponent observability + PlantOrchard/HarvestOrchard work-class
// counters, built for the OrchardComponent live-porting pass (see
// code/Game/Component/orchardcomponent.cpp). Everything here is diagnostic-only:
// with BANISHED_ORCHARD_DIAG unset it does nothing and touches no game state.
//
// Env:
//   BANISHED_ORCHARD_DIAG=1        enable, dump every 120 ticks (default period)
//   BANISHED_ORCHARD_DIAG=<N>      enable, dump every N ticks (N>0, numeric)
//
// Per the project convention (see reference_diagnostics_must_report_outcomes.md):
// every branch below prints something, including the "found nothing" branches, so
// an empty log can never be misread as a pass.
//
// Offsets used (cross-checked against engine/types/*.h and existing scan code in
// tool_harness.cpp's FireOrchardPanel/FireTradePanel, which use the same EntityList
// walk):
//   gameWorld+0xe8         -> EntityList
//   EntityList+0x08/+0x10  -> masterArray {data, count}
//   entity+0x30/+0x38      -> component array {data, count}
//   component+0x10         -> owner entity (ControllerComponent, engine/types/ControllerComponent.h)
//   OrchardComponent (engine/types/OrchardComponent.h):
//     +0x50 slot array ptr, +0x58 slot count (int32), +0x60 tree-type ExternalPtr
//     slot stride 0x28: +0x00 tileIdx, +0x04 state, +0x08 owned entity, +0x18 growth IBP
//     (documented in code/Game/Component/orchardcomponent.cpp's OrchardComponent_GetGrowth)
//   WorkPlaceComponent (engine/types/WorkPlaceComponent.h + code/Game/Component/workplacecomponent.cpp):
//     +0x58 worker IBP array, +0x60 roster count (int32) — CONFIRMED live (workplacecomponent.cpp
//     uses these throughout Attach/Detach/eviction).
//   StateMachineComponent (code/Game/Component/statemachinecomponent.cpp, verified against that
//   file's own header comment + StateMachine_TickReimpl / the link loop at lines ~803-805):
//     +0x28 root controller sub-object / chain root (vtable, then root+0x08=parent(zeroed),
//           root+0x10=first-child == SM base+0x38)
//     +0x38 root's first-child pointer (chain head; zero == nothing linked yet)
//     +0x48 controllerSelf — current/active node pointer (StateMachine::Tick's sm[0])
//     +0x50 stateFlags (uint32): byte +0x50 currentInput, +0x51 activeState,
//           +0x52 pendingState (this is what GetDefaultState/CreateState write — see
//           controller_live_base.h), +0x53 transitionState
//   Each linked chain node (any ControllerComponent's embedded node, e.g. OrchardComponent's
//   at comp+0x28) has node+0x08=parent(prev), node+0x10=child(next) — a flat singly-linked
//   list, not a branching tree (confirmed by the link loop always appending at the current
//   tail's +0x10 and advancing the tail). Walking from SM+0x38 via +0x10 and comparing node
//   addresses to comp+0x28 is therefore an exact (not best-effort) linked-check.
//   Max-worker cap: NOT on WorkPlaceComponent itself. code/Game/Work/plantorchard.cpp's
//   Slot4 caches it at matDesc+0x30 (matDesc = FindComp_MatDesc(workplaceEntity), exe
//   0x140003050) the first time it's needed (matDesc+0x30==0 check). That's a Work-side
//   lazy cache, not a struct field we can trust to be populated before any planting has
//   ever been attempted, so we read it best-effort and print "not yet cached" rather than
//   guess at an unverified WorkPlaceDescription offset — see the CLAUDE.md project note
//   "if an offset is unverified, say so".

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"     // ::Object::GetTypeIdByName
#include "Component/orchardcomponent_funcs.h" // extern OrchardComponent_GetGrowth (do not modify that file)
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool s_initialized = false;
bool s_enabled     = false;
int  s_periodTicks = 120;

void Init() {
    if (s_initialized) return;
    s_initialized = true;

    const char* env = Opt().orchardDiag;
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        s_enabled = false;
        fprintf(stderr, "[OrchardDiag] disabled (BANISHED_ORCHARD_DIAG unset)\n");
        return;
    }
    s_enabled = true;

    // Numeric override for the dump period.
    char* endp = nullptr;
    long v = std::strtol(env, &endp, 10);
    if (endp != env && *endp == '\0' && v > 0) s_periodTicks = static_cast<int>(v);

    fprintf(stderr, "[OrchardDiag] ENABLED (BANISHED_ORCHARD_DIAG=\"%s\") — dumping every "
                    "%d ticks\n", env, s_periodTicks);
    std::fflush(stderr);
}

// ------------------------------------------------------------------
// Work-class construction/execution counters (PlantOrchard / HarvestOrchard).
// Incremented from code/Game/Work/plantorchard.cpp and harvestorchard.cpp via the
// extern "C" hooks below — a single call each, guarded by s_enabled internally so
// the callers don't need their own env check.
// ------------------------------------------------------------------
uint64_t s_plantConstructed = 0;
uint64_t s_plantExecuted    = 0;   // Slot4 successfully created a planting action (return 2)
uint64_t s_harvestConstructed = 0;
uint64_t s_harvestExecuted    = 0; // Slot4 successfully created a harvest action (return 2)

// ------------------------------------------------------------------
// Periodic per-entity dump.
// ------------------------------------------------------------------
// ------------------------------------------------------------------
// Town-wide stored-goods total.
//
// "Does the harvested fruit reach storage" was the last unmeasured link in the orchard
// chain: the harvest completes (msg=1, ascending slots, machine leaves state 11), and
// CreateRawMaterial spawns piles on the ground, but nothing in this codebase reported
// whether a citizen ever hauls one into a barn. Counting the piles is NOT the same
// question — a pile that no one collects is a yield of zero.
//
// StorageComponent inventory layout (see action_base_rawmat.cpp): an array at +0x28 of
// {resType(8 bytes), count(int)} with stride 0x10, entry count at +0x30. Summing every
// storage in town gives a number that only moves when goods are actually deposited.
//
// This is a HARNESS probe, not a port: it reads published fields and writes nothing.
void DumpTownStorage(int frameNum, uintptr_t arr, int n) {
    const uint32_t tidStorage = ::Object::GetTypeIdByName("StorageComponent");
    if (!tidStorage || !arr || n <= 0) {
        fprintf(stderr, "[OrchardDiag] frame=%d: town storage — cannot scan "
                        "(tidStorageComponent=%u entities=%d)\n", frameNum, tidStorage, n);
        return;
    }

    // Aggregate by resType so the fruit shows up as its own line rather than lost in a total.
    struct Stack { uintptr_t resType; long long count; };
    Stack byType[24] = {};
    int    types = 0;
    long long grandTotal = 0;
    int    storages = 0, nonEmpty = 0;

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;
        const int cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* comp = *cs;
            if (!comp) continue;
            void** vt = *reinterpret_cast<void***>(comp);
            if (!vt || !vt[2]) continue;
            if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(comp) != tidStorage) continue;
            ++storages;

            uintptr_t st = reinterpret_cast<uintptr_t>(comp);
            const int stacks = *reinterpret_cast<int32_t*>(st + 0x30);
            uintptr_t inv = *reinterpret_cast<uintptr_t*>(st + 0x28);
            if (stacks > 0) ++nonEmpty;
            for (int k = 0; inv && k < stacks; ++k) {
                uintptr_t e = inv + static_cast<uintptr_t>(k) * 0x10;
                uintptr_t rt = *reinterpret_cast<uintptr_t*>(e);
                const int ct = *reinterpret_cast<int32_t*>(e + 8);
                grandTotal += ct;
                int f = -1;
                for (int t = 0; t < types; ++t) if (byType[t].resType == rt) { f = t; break; }
                if (f < 0 && types < 24) { f = types++; byType[f].resType = rt; byType[f].count = 0; }
                if (f >= 0) byType[f].count += ct;
            }
        }
    }

    fprintf(stderr, "[OrchardDiag] frame=%d: town storage — %d StorageComponent(s), %d holding "
                    "goods, %d distinct type(s), TOTAL ITEMS=%lld\n",
            frameNum, storages, nonEmpty, types, grandTotal);
    for (int t = 0; t < types; ++t)
        fprintf(stderr, "[OrchardDiag]   resType=%p count=%lld\n",
                reinterpret_cast<void*>(byType[t].resType), byType[t].count);
}

void DumpOrchards(int frameNum) {
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) {
        fprintf(stderr, "[OrchardDiag] frame=%d: gameWorld singleton null — cannot scan\n", frameNum);
        return;
    }
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!entityList) {
        fprintf(stderr, "[OrchardDiag] frame=%d: EntityList (gw+0xe8) null — cannot scan\n", frameNum);
        return;
    }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);   // masterArray.data
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);     // masterArray.count
    const uint32_t tidOrchardComp = ::Object::GetTypeIdByName("OrchardComponent");
    const uint32_t tidWorkPlaceComp = ::Object::GetTypeIdByName("WorkPlaceComponent");
    const uint32_t tidStateMachineComp = ::Object::GetTypeIdByName("StateMachineComponent");
    if (!arr || n <= 0 || !tidOrchardComp) {
        fprintf(stderr, "[OrchardDiag] frame=%d: cannot scan (arr=%p n=%d tidOrchardComponent=%u) — "
                        "no entities or type-id unresolved\n", frameNum, reinterpret_cast<void*>(arr), n,
                        tidOrchardComp);
        return;
    }

    int found = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;

        int       cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        void* orchComp = nullptr;
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* comp = *cs;
            if (!comp) continue;
            void** vt = *reinterpret_cast<void***>(comp);
            typedef uint32_t (*GetTypeId_t)(void*);
            if (vt && vt[2] && reinterpret_cast<GetTypeId_t>(vt[2])(comp) == tidOrchardComp) {
                orchComp = comp;
                break;
            }
        }
        if (!orchComp) continue;
        ++found;

        uintptr_t comp = reinterpret_cast<uintptr_t>(orchComp);
        uintptr_t treeType = *reinterpret_cast<uintptr_t*>(comp + 0x60);
        uintptr_t slots     = *reinterpret_cast<uintptr_t*>(comp + 0x50);
        int32_t   slotCount = *reinterpret_cast<int32_t*>(comp + 0x58);

        // Histogram over the slot array (stride 0x28): state int at +0x04, plus
        // counts of non-null entity (+0x08) / non-null growth object (+0x18).
        // Small fixed table keyed by state value, -1..7 buckets + "other".
        constexpr int kStateBuckets = 9;   // -1..7
        int stateHist[kStateBuckets] = {0};
        int otherStates = 0;
        int nonNullEntity = 0, nonNullGrowth = 0;
        if (slots && slotCount > 0) {
            for (int s = 0; s < slotCount; ++s) {
                uintptr_t slot = slots + static_cast<uintptr_t>(s) * 0x28;
                int32_t state = *reinterpret_cast<int32_t*>(slot + 0x04);
                if (state >= -1 && state < kStateBuckets - 1) stateHist[state + 1]++;
                else ++otherStates;
                if (*reinterpret_cast<void**>(slot + 0x08) != nullptr) ++nonNullEntity;
                if (*reinterpret_cast<void**>(slot + 0x18) != nullptr) ++nonNullGrowth;
            }
        }

        float avgGrowth = OrchardComponent_GetGrowth(orchComp);

        // Sibling WorkPlaceComponent on the same entity, via the owner-entity scan
        // (owner = comp+0x10, CheckPtr'd per the project's raw-owner-pointer convention —
        // see e.g. code/Game/Work/cleararea.cpp:635, harvestorchard.cpp:713).
        void* owner = *reinterpret_cast<void**>(comp + 0x10);
        void* wpc = nullptr;
        void* smc = nullptr;
        if (owner && (tidWorkPlaceComp || tidStateMachineComp)) {
            int wN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(owner) + 0x38);
            uintptr_t wA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(owner) + 0x30);
            for (int c = 0; c < wN && wA; ++c) {
                void** cs = reinterpret_cast<void**>(wA + static_cast<uintptr_t>(c) * 8);
                void* wc = *cs;
                if (!wc) continue;
                void** vt = *reinterpret_cast<void***>(wc);
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t wtid = (vt && vt[2]) ? reinterpret_cast<GetTypeId_t>(vt[2])(wc) : 0;
                if (!wpc && tidWorkPlaceComp && wtid == tidWorkPlaceComp) wpc = wc;
                if (!smc && tidStateMachineComp && wtid == tidStateMachineComp) smc = wc;
                if (wpc && smc) break;
            }
        }

        fprintf(stderr, "[OrchardDiag] frame=%d entity=%p comp=%p treeType(+0x60)=%s "
                        "slots(+0x50)=%p count(+0x58)=%d avgGrowth=%.6f\n",
                frameNum, ent, orchComp,
                treeType ? "SET" : "UNTYPED", reinterpret_cast<void*>(slots), slotCount, avgGrowth);
        fprintf(stderr, "[OrchardDiag]   slot histogram (state:count) = "
                        "[-1:%d 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d other:%d]  "
                        "nonNullEntity=%d nonNullGrowth=%d\n",
                stateHist[0], stateHist[1], stateHist[2], stateHist[3], stateHist[4],
                stateHist[5], stateHist[6], stateHist[7], otherStates,
                nonNullEntity, nonNullGrowth);

        // avgGrowth is an average over ALL slots and prints 0.000000 both when nothing is
        // growing and when growth is merely tiny — two very different states. Show the raw
        // per-tree field (fruit/growth object at slot+0x18, ratio +0x30, active flag +0x34)
        // so "not ticking" is distinguishable from "early".
        if (slots && slotCount > 0) {
            float gMin = 1e30f, gMax = -1e30f;
            float mMin = 1e30f, mMax = -1e30f;
            int active = 0, sampled = 0, matured = 0;
            for (int s = 0; s < slotCount; ++s) {
                uintptr_t slot = slots + static_cast<uintptr_t>(s) * 0x28;
                // The TREE's own maturity lives on the NaturalResourceComponent bound at
                // slot+0x10, field +0x28. FruitComponent::Update gates ALL ripening on
                // `growthRate >= 1.0f`, so a sapling correctly bears nothing and a fruit
                // ratio of 0 says nothing about whether anything is wrong — this is the
                // number that decides it.
                if (void* nr = *reinterpret_cast<void**>(slot + 0x10)) {
                    float mat = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(nr) + 0x28);
                    if (mat < mMin) mMin = mat;
                    if (mat > mMax) mMax = mat;
                    if (mat >= 1.0f) ++matured;
                }
                void* g = *reinterpret_cast<void**>(slot + 0x18);
                if (!g) continue;
                float ratio = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(g) + 0x30);
                if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(g) + 0x34)) ++active;
                if (ratio < gMin) gMin = ratio;
                if (ratio > gMax) gMax = ratio;
                ++sampled;
            }
            if (sampled)
                fprintf(stderr, "[OrchardDiag]   per-tree growth: sampled=%d activeFlag(+0x34)=%d "
                                "fruitRatio(+0x30) min=%.6f max=%.6f | treeMaturity(natRes+0x28) "
                                "min=%.6f max=%.6f matured(>=1.0)=%d\n",
                        sampled, active, gMin, gMax, mMin, mMax, matured);
            else
                fprintf(stderr, "[OrchardDiag]   per-tree growth: NO slot carries a growth object "
                                "(+0x18 null on all %d)\n", slotCount);
        }

        if (wpc) {
            int32_t roster = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(wpc) + 0x60);
            fprintf(stderr, "[OrchardDiag]   WorkPlaceComponent=%p roster(+0x60)=%d  "
                            "(max-worker cap not read here — see file header comment; it's a "
                            "Work-side lazy cache on matDesc+0x30, not a WPC field)\n",
                    wpc, roster);
        } else {
            fprintf(stderr, "[OrchardDiag]   no WorkPlaceComponent found on owner entity %p "
                            "(tidWorkPlaceComponent=%u)\n", owner, tidWorkPlaceComp);
        }

        // OrchardComponent's own embedded state node (comp+0x28, written by
        // CONTROLLER_LIVE_FACTORY_STATES's InstallLive — see controller_live_base.h) and
        // its owner back-pointer (comp+0x40, node+0x18 relative to comp+0x28). Printed
        // raw regardless of whether a StateMachineComponent exists, so InstallLive's
        // writes are visible even on an entity that can never tick them.
        void* nodeVtbl  = *reinterpret_cast<void**>(comp + 0x28);
        void* nodeOwner = *reinterpret_cast<void**>(comp + 0x40);
        fprintf(stderr, "[OrchardDiag]   node(+0x28).vtable=%p  node(+0x28+0x18)/backptr(+0x40)=%p\n",
                nodeVtbl, nodeOwner);

        // StateMachineComponent — the go/no-go for whether that node ever ticks.
        // Offsets verified against code/Game/Component/statemachinecomponent.cpp's own
        // header comment + its link loop (~lines 803-805) and StateMachine_TickReimpl.
        if (!smc) {
            fprintf(stderr, "[OrchardDiag]   hasStateMachine=NO (tidStateMachineComponent=%u) — "
                            "the node at comp+0x28 can never tick regardless of the port; the fix "
                            "would be elsewhere (template/component list), not in OrchardComponent\n",
                    tidStateMachineComp);
        } else {
            uintptr_t sm = reinterpret_cast<uintptr_t>(smc);
            uint8_t currentInput  = *reinterpret_cast<uint8_t*>(sm + 0x50);
            uint8_t activeState   = *reinterpret_cast<uint8_t*>(sm + 0x51);
            uint8_t pendingState  = *reinterpret_cast<uint8_t*>(sm + 0x52);
            uint8_t transitionSt  = *reinterpret_cast<uint8_t*>(sm + 0x53);

            // Walk the chain from the SM root's first child (sm+0x38) via each node's
            // +0x10 (child/next) pointer — a flat singly-linked list, not a tree (see
            // file header comment). Bounded to guard against a corrupt/cyclic chain.
            void* target = reinterpret_cast<void*>(comp + 0x28);
            void* walk = *reinterpret_cast<void**>(sm + 0x38);
            bool linked = false;
            int chainLen = 0;
            constexpr int kMaxWalk = 256;
            while (walk && chainLen < kMaxWalk) {
                ++chainLen;
                if (walk == target) { linked = true; break; }
                walk = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(walk) + 0x10);
            }

            fprintf(stderr, "[OrchardDiag]   hasStateMachine=YES smc=%p  stateFlags: "
                            "currentInput(+0x50)=%u activeState(+0x51)=%u pendingState(+0x52)=%u "
                            "transitionState(+0x53)=%u  linked=%s (chainLen scanned=%d%s)\n",
                    smc, currentInput, activeState, pendingState, transitionSt,
                    linked ? "YES" : "NO", chainLen,
                    chainLen >= kMaxWalk ? ", TRUNCATED — possible cycle" : "");
        }
    }

    if (found == 0) {
        fprintf(stderr, "[OrchardDiag] frame=%d: scanned %d entities, found ZERO with an "
                        "OrchardComponent\n", frameNum, n);
    } else {
        fprintf(stderr, "[OrchardDiag] frame=%d: %d orchard component(s) reported above\n",
                frameNum, found);
    }

    fprintf(stderr, "[OrchardDiag] frame=%d: work counters — PlantOrchard constructed=%llu "
                    "executed=%llu | HarvestOrchard constructed=%llu executed=%llu\n",
            frameNum, static_cast<unsigned long long>(s_plantConstructed),
            static_cast<unsigned long long>(s_plantExecuted),
            static_cast<unsigned long long>(s_harvestConstructed),
            static_cast<unsigned long long>(s_harvestExecuted));

    DumpTownStorage(frameNum, arr, n);
    std::fflush(stderr);
}

}  // namespace

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------
extern "C" void OrchardDiag_Tick(int frameNum) {
    Init();
    if (!s_enabled) return;
    if (frameNum <= 0 || (frameNum % s_periodTicks) != 0) return;
    DumpOrchards(frameNum);
}

extern "C" void OrchardDiag_NotePlantConstructed()   { Init(); if (s_enabled) ++s_plantConstructed; }
extern "C" void OrchardDiag_NotePlantExecuted()      { Init(); if (s_enabled) ++s_plantExecuted; }
extern "C" void OrchardDiag_NoteHarvestConstructed() { Init(); if (s_enabled) ++s_harvestConstructed; }
extern "C" void OrchardDiag_NoteHarvestExecuted()    { Init(); if (s_enabled) ++s_harvestExecuted; }
