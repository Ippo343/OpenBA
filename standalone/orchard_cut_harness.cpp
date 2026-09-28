// standalone/orchard_cut_harness.cpp
// Drives the orchard panel's CUT button (command id 3) without a human click, so the
// newly-ported state 20 can be tested from `make sa-run`.
//
// This is a stand-in for the PLAYER, not for any game code: it asks CanCommand(3) exactly
// as the panel does, and then calls the real idx14 Command(comp, 3), which posts node
// event 5 -> Orchard_Event5Clear -> request state 20. Every gate downstream (the event
// list, the transition table, the state-20 handlers, the ClearArea work) still runs for
// real. The user's 2026-08-10 hand-test proved this exact call path works; the harness
// only removes the human from the loop so a cut can be observed in one automated run.
//
// Env:
//   BANISHED_ORCHARD_CUT=<frame>   enable; fires once at that frame (a bare "1" means
//                                  the default frame below, so the value reads as
//                                  "when", not "how many").
//
// Default OFF. Cutting is destructive to the orchard under test, so it must never fire
// during the plant/tend/harvest runs.
#include "resolve.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/OrchardComponent.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool s_init = false, s_enabled = false, s_fired = false;
int  s_frame = 3000;   // late enough that a demo orchard has planted its grid

void Init() {
    if (s_init) return;
    s_init = true;
    const char* env = Opt().orchardCut;
    if (!env || !env[0] || std::strcmp(env, "0") == 0 || std::strcmp(env, "false") == 0) {
        std::fprintf(stderr, "[OrchardCut] disabled (BANISHED_ORCHARD_CUT unset)\n");
        return;
    }
    s_enabled = true;
    const int v = std::atoi(env);
    if (v > 1) s_frame = v;
    std::fprintf(stderr, "[OrchardCut] ENABLED — will click CUT once at frame %d\n", s_frame);
    std::fflush(stderr);
}

}  // namespace

extern "C" void OrchardCutHarness_Tick(int frameNum) {
    Init();
    if (!s_enabled || s_fired || frameNum < s_frame) return;
    s_fired = true;

    void* gw = (GameUpdateLoop::_singleton != nullptr) ? GameUpdateLoop::_singleton : nullptr;
    uintptr_t entityList = gw ? *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8) : 0;
    uintptr_t arr = entityList ? *reinterpret_cast<uintptr_t*>(entityList + 0x08) : 0;
    const int n   = entityList ? *reinterpret_cast<int32_t*>(entityList + 0x10) : 0;
    const uint32_t tid = OrchardComponent::TypeId();
    if (!arr || n <= 0 || !tid) {
        std::fprintf(stderr, "[OrchardCut] frame=%d: cannot scan (arr=%p n=%d tid=%u)\n",
                     frameNum, reinterpret_cast<void*>(arr), n, tid);
        std::fflush(stderr);
        return;
    }

    int found = 0, clicked = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;
        const int cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        OrchardComponent* comp = nullptr;
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* cc = *cs;
            if (!cc) continue;
            if (reinterpret_cast<Object::Base*>(cc)->Type() == tid) {   // B.3.8 bridge
                comp = reinterpret_cast<OrchardComponent*>(cc);         // B.3.8 bridge
                break;
            }
        }
        if (!comp) continue;
        ++found;

        // Ask before clicking, and report BOTH arms: a greyed-out CUT is itself the answer
        // to "does the button work", so a silent skip would be the worst outcome here.
        const uint8_t can = static_cast<uint8_t>(comp->CommandMask(3));
        std::fprintf(stderr, "[OrchardCut] frame=%d comp=%p CanCommand(3)=%u -> %s\n",
                     frameNum, static_cast<void*>(comp), can,
                     can ? "clicking CUT" : "SKIPPED (button greyed)");
        if (!can) continue;
        comp->Command(3);
        ++clicked;
    }
    std::fprintf(stderr, "[OrchardCut] frame=%d: %d orchard(s) found, %d CUT click(s) issued\n",
                 frameNum, found, clicked);
    std::fflush(stderr);
}
