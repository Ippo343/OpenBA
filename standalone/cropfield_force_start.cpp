// standalone/cropfield_force_start.cpp
// BANISHED_CROPFIELD_FORCE_START — switch a demo crop field ON and (optionally) fire
// its plant command, so states 9/10/11 are reachable without the drag tool or panel.
//
// WHY: CropFieldComponent_Ctor seeds +0x48 = 0, and state-2 enter publishes that byte
// into the workplace's accepting flag while the state-2 per-frame planting trigger
// returns immediately unless it is set. So a freshly spawned demo field parks in state 2
// forever — correctly. In retail the player drags the field out and picks a crop in the
// panel, which is what sets it. This is the crop-field analogue of
// BANISHED_ORCHARD_FORCE_START and, like it, only pokes already-published state.
//
// Values:
//   1  set +0x48 = 1 and let the SEASONAL trigger fire the plant on its own. This is the
//      faithful path and also tests the trigger itself, but it only fires inside the
//      year window the trigger checks, so it can wait a long time.
//   2  also post node event 3 (the panel's plant command) immediately — the same event
//      the state table registers for state 2 — so state 9 is entered on the spot.
//
// Test-only, opt-in, default off.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "Component/CropFieldComponent.h"
#include "Component/NaturalResourceDescription.h"
#include "Component/StateMachineComponent.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kStartFrame = 300;
constexpr int kRetry      = 120;
constexpr int kMaxTracked = 8;

bool  s_init = false;
int   s_mode = 0;
void* s_done[kMaxTracked];
int   s_doneN = 0;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = Opt().cropFieldForceStart;
    if (!e || !e[0] || std::strcmp(e, "0") == 0) {
        std::fprintf(stderr, "[CropStart] disabled (BANISHED_CROPFIELD_FORCE_START unset)\n");
        std::fflush(stderr);
        return;
    }
    s_mode = std::atoi(e);
    if (s_mode <= 0) s_mode = 1;
    std::fprintf(stderr, "[CropStart] ENABLED mode=%d — switch demo crop fields on%s\n",
                 s_mode, s_mode >= 2 ? " and post the plant command" : "");
    std::fflush(stderr);
}

bool AlreadyDone(void* c) {
    for (int i = 0; i < s_doneN; ++i) if (s_done[i] == c) return true;
    return false;
}

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        void* c = *cs;
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tid) return c;   // B.3.8 bridge
    }
    return nullptr;
}

// Post a node event through the entity's StateMachineComponent, exactly as
// OrchardComponent_Command does: smc+0x48 is the SM, sm+0x00 the node, node vtable
// slot 2 is PostEvent(node, sm, curIdx, eventId), and smc+0x51 carries the index.
void PostNodeEvent(void* smc, int eventId) {
    if (!smc) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(smc);
    uintptr_t sm = s + 0x48;
    void* node = *reinterpret_cast<void**>(sm);
    if (!node) return;
    void** vt = *reinterpret_cast<void***>(node);
    if (!vt || !vt[2]) return;
    const int curIdx = *reinterpret_cast<int8_t*>(s + 0x51);
    reinterpret_cast<char (*)(void*, void*, int, int)>(vt[2])(
        node, reinterpret_cast<void*>(sm), curIdx, eventId);
    std::fprintf(stderr, "[CropStart]   posted node event %d (node=%p curIdx=%d)\n",
                 eventId, node, curIdx);
}

void Fire(int frame) {
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);
    const uint32_t tidCrop = CropFieldComponent::TypeId();
    const uint32_t tidSM   = StateMachineComponent::TypeId();
    if (!arr || n <= 0 || !tidCrop) return;

    int found = 0;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;
        void* comp = FindComp(ent, tidCrop);
        if (!comp) continue;
        ++found;
        if (AlreadyDone(comp)) continue;

        uintptr_t c = reinterpret_cast<uintptr_t>(comp);

        // ── pick a CROP TYPE (the analogue of the orchard's tree-type combo) ──────
        // State-9 enter reads comp+0x50 as a description LIST and looks inside it for a
        // NaturalResourceDescription (0x1401c7b68); that entry IS the crop. A null
        // +0x50 means "no crop chosen", so the state clears +0x49, marks no cells and
        // the field stays bare — correct for an unconfigured field, and exactly what a
        // freshly spawned demo field is.
        //
        // The panel fills it from the world's resource-type list: town manager
        // (gw+0xf0) +0x120 array / +0x128 count, each entry carrying its
        // sub-description list at +0x28 / +0x30. So pick the first entry that actually
        // contains a NaturalResourceDescription and write it in — the same value a
        // player's combo pick would produce.
        if (*reinterpret_cast<uintptr_t*>(c + 0x50) == 0) {
            const uint32_t tidNatRes = NaturalResourceDescription::TypeId();
            void* gw2 = *g_pGameWorldSingleton;
            uintptr_t town = gw2 ? *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(gw2) + 0xf0) : 0;
            void** types = town ? *reinterpret_cast<void***>(town + 0x120) : nullptr;
            const int typeN = town ? *reinterpret_cast<int32_t*>(town + 0x128) : 0;
            // VARIETY, not just "a crop": collect every planable type and hand each
            // field the NEXT one, rather than the first match for all of them.
            //
            // Food Variety wants 8 different crops and 8 different orchard trees
            // harvested in a single year, and Farmer wants 8 crop seed types owned.
            // Taking types[0] for every field in the world makes both permanently
            // unreachable no matter how many fields are built -- the town grows one
            // crop, eight times over. The turn is static and advances per ASSIGNED
            // field, so the spread stays even.
            static int s_cropTurn = 0;
            void* chosen = nullptr;
            {
                void* planable[64];
                int planableN = 0;
                for (int t = 0; t < typeN && types && planableN < 64; ++t) {
                    void* entry = types[t];
                    if (!entry) continue;
                    uintptr_t e = reinterpret_cast<uintptr_t>(entry);
                    void** subs = *reinterpret_cast<void***>(e + 0x28);
                    const int subN = *reinterpret_cast<int32_t*>(e + 0x30);
                    for (int k = 0; k < subN && subs; ++k)
                        if (subs[k] && tidNatRes && fn_IsDerivedFrom &&
                            fn_IsDerivedFrom(subs[k], tidNatRes)) {
                            planable[planableN++] = entry; break;
                        }
                }
                if (planableN > 0) {
                    chosen = planable[s_cropTurn % planableN];
                    ++s_cropTurn;
                }
            }
            if (chosen) {
                *reinterpret_cast<void**>(c + 0x50) = chosen;
                std::fprintf(stderr, "[CropStart]   crop type set: %p (from %d world "
                             "resource types)\n", chosen, typeN);
            } else {
                std::fprintf(stderr, "[CropStart]   NO crop type available — scanned %d "
                             "world resource types, none carries a "
                             "NaturalResourceDescription. The field will plant nothing; "
                             "that is a world-data gap, not a component defect.\n", typeN);
            }
        }

        const uint8_t before = *reinterpret_cast<uint8_t*>(c + 0x48);
        *reinterpret_cast<uint8_t*>(c + 0x48) = 1;
        if (s_doneN < kMaxTracked) s_done[s_doneN++] = comp;

        std::fprintf(stderr, "[CropStart] frame=%d comp=%p master(+0x48) %u -> 1, "
                     "hasCrop(+0x49)=%u slots(+0x60)=%d\n",
                     frame, comp, (unsigned)before,
                     (unsigned)*reinterpret_cast<uint8_t*>(c + 0x49),
                     *reinterpret_cast<int32_t*>(c + 0x60));
        if (s_mode >= 2) PostNodeEvent(FindComp(ent, tidSM), 3);
        std::fflush(stderr);
    }
    if (found == 0) {
        static bool said = false;
        if (!said) {
            said = true;
            std::fprintf(stderr, "[CropStart] frame=%d: no CropFieldComponent in the world "
                         "(set BANISHED_BUILD_DEMO_CROPFIELD=1 and BANISHED_LIVE_CROPFIELD=1?)\n",
                         frame);
            std::fflush(stderr);
        }
    }
}

}  // namespace

extern "C" void CropFieldForceStart_Tick(int frameNum) {
    Init();
    if (s_mode <= 0) return;
    if (frameNum < kStartFrame) return;
    if (((frameNum - kStartFrame) % kRetry) != 0) return;
    Fire(frameNum);
}
