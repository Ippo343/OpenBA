// standalone/finish_builds_harness.cpp
//
// BANISHED_FINISH_BUILDS=<interval frames> — finish every structure still under
// construction, the way the stress placer already finishes its own.
//
// Two achievements are blocked by construction rather than by anything they measure:
//
//   Golden Gate wants a bridge at least 50 units long. The bridge tool commits a
//   137-cell span happily (21 segments, CalcCost valid, dock reachable), but stat 25
//   (count of a building type) and stat 29 (bridge length) count only COMPLETED
//   buildings -- a committed foundation reads as have=0 -- and 21 segments' worth of
//   material hauled to a river 70 tiles from town does not finish inside any run
//   length that is practical to sit through.
//
//   Highwaymen wants 2,000 stone-paved road tiles, which is the same problem two
//   orders of magnitude larger.
//
// The mechanism is the one code/Game/Game/starttown.cpp already uses for every
// structure it places (its "instant-path FALLBACK"): a StateMachineComponent carries
// its CURRENT state at +0x50 and its PENDING state at +0x52 (0xff = nothing pending),
// and stamping pending = 2 makes the next tick enter the built state through the
// component's own state machine. Nothing here writes a mesh, a stock level or a
// counter -- the state machine does the finishing, and every stat that then moves is
// computed by ported retail code from a genuinely-built structure.
//
// This is a TEST FIXTURE and a deviation, exactly as the placer's stamp is: it stands
// in for builders delivering material. It is not a claim that the build path works --
// the short bridge in an ordinary run finishes on its own and is the evidence for
// that. Opt-in, off by default.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

int s_interval = -2;      // -2 = env unread, -1 = disabled
int s_finished = 0;

void Init() {
    if (s_interval != -2) return;
    const char* e = Opt().finishBuilds;
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : 600;
    std::fprintf(stderr, "[FinishBuilds] armed: every %d frames, stamp pending state 2 on "
                         "every unbuilt structure (test fixture, stands in for builders "
                         "delivering material)\n", s_interval);
    std::fflush(stderr);
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    const uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (vt && vt[2] && reinterpret_cast<uint32_t (*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

}  // namespace

extern "C" void FinishBuildsHarness_Tick(int frameNum) {
    Init();
    if (s_interval <= 0 || frameNum == 0 || (frameNum % s_interval) != 0) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t entityList = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(gw) + 0xe8);
    const uintptr_t arr = entityList ? *reinterpret_cast<uintptr_t*>(entityList + 0x08) : 0;
    const int n = entityList ? *reinterpret_cast<int32_t*>(entityList + 0x10) : 0;
    if (!arr || n <= 0) return;

    const uint32_t tidBuild = Tid("BuildComponent");
    const uint32_t tidSM    = Tid("StateMachineComponent");
    if (!tidBuild || !tidSM) return;

    int scanned = 0, stamped = 0, alreadyBuilt = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;
        void* bc = FindComp(ent, tidBuild);
        if (!bc) continue;                       // not a constructable structure
        ++scanned;
        void* sm = FindComp(ent, tidSM);
        if (!sm) continue;
        const int cur  = *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(sm) + 0x50);
        const int pend = *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(sm) + 0x52);
        if (cur == 2 || pend == 2) { ++alreadyBuilt; continue; }

        // Drive the BuildComponent's OWN completion rather than poking the state.
        // Stamping pending = 2 works for a start-town structure the placer spawns
        // already-finished, but a tool-created entity (bridge segment, road tile) is
        // mid-construction and its state machine has no 8->2 edge to take on demand:
        // the stamp was re-applied to the same 25 segments every pass and none of them
        // ever moved. The real sequence is state 1 (awaiting materials) -> state 8
        // (building, progress bc+0x48 ramping toward target) -> state 2, so give each
        // stage what it is waiting for and let State8Update finish it:
        //   state 1  -> request 8 (materials treated as delivered)
        //   state 8  -> stamp progress at the target, which is what State8Update's own
        //               completion test reads before it requests 8->2
        if (cur == 8) {
            // bc+0x48 is progress, bc+0x4c the build multiplier. State8Update completes
            // on progress >= target; a saturated progress satisfies that whatever the
            // target works out to, and the transition is still made by retail code.
            *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(bc) + 0x48) = 0x40000000;
            ++stamped;
        } else {
            *reinterpret_cast<int8_t*>(static_cast<uint8_t*>(sm) + 0x52) = 8;
            ++stamped;
        }
    }
    s_finished += stamped;

    if (stamped > 0 || scanned > 0) {
        std::fprintf(stderr, "[FinishBuilds] f=%d structures=%d built/pending=%d stamped=%d "
                             "(cumulative %d)\n",
                     frameNum, scanned, alreadyBuilt, stamped, s_finished);
        std::fflush(stderr);
    }
}
