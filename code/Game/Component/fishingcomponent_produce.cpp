// code/Game/Component/fishingcomponent_produce.cpp
// FishingComponent vtable slots 19/20/21 — the production hooks of the shared
// ResourceProduceComponent state machine. Fishing is the only subclass in the food
// wave that overrides all three; Hunt and Herbalist leave the base no-ops in place.
//
//   slot 19 (+0x98) OnEnterProducing  retail 0x14011ee80
//   slot 20 (+0xa0) OnExitProducing   retail 0x14011ee50
//   slot 21 (+0xa8) ProduceTick       retail 0x14011ec10
//
// +0x50 (`fishingFlag`) is the registered latch: enter stamps the dock's radius into
// the map coverage grid with +1 and sets it, exit stamps -1 and clears it. Both are
// guarded on the latch so a double enter/exit cannot drift the shared grid counter —
// that guard is retail's, not ours.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Component/FishingComponent.h"
#include "Component/RadiusDescription.h"    // ENDGAME B.3.9 C1: 0x1401c4870 cell class
#include "Component/FishingDescription.h"   // ENDGAME B.3.9 C1: 0x1401c7698 cell class
#include "Component/exe_bodies.h"

namespace {

// 0x14009f990 FishingComponent_BrushTile(mgr, state, cx, cy, r, delta) — ported
// (happinesscomponent.cpp), called directly. Adds `delta` to the byte at
// cell+0x1c+state over a disk of radius r, then to the per-state total at
// mgr+0x90+state*2. The fishing dock uses state 8. The MapComponent finder
// 0x140004680 is FloorComponent_FindResData.

constexpr int kFishingCoverageState = 8;

// Walk the owning entity's component list (entity+0x28) for the RadiusDescription and
// return its radius (+0x28). Retail inlines this scan in both eed0 and the tick; it
// breaks at the FIRST slot whose GetTypeId matches, then re-validates with
// IsDerivedFrom — so a host without a RadiusDescription yields a null and, in retail,
// an unguarded deref. We return -1 instead ONLY because the standalone can legitimately
// build a dock whose description set is still partly inert; see the callers.
int RadiusOf(uintptr_t comp) {
    const uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);
    if (!entity) return -1;
    const uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x28);
    if (!list) return -1;
    const int n = *reinterpret_cast<int32_t*>(list + 0x30);
    void** items = *reinterpret_cast<void***>(list + 0x28);
    if (!items) return -1;
    for (int i = 0; i < n; ++i) {
        void* obj = items[i];
        if (!obj) continue;
        if (::Object::IsDerivedFromNullSafe(obj, RadiusDescription::TypeId()))
            return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(obj) + 0x28);
    }
    return -1;
}

// Entity tile centre, as both retail bodies compute it from the MapComponent:
//   cx = (mc+0x2c >> 1) + mc+0x28 ,  cy = (mc+0x2e >> 1) + mc+0x2a
// (+0x28/+0x2a are the footprint origin, +0x2c/+0x2e its extent, all int16.)
bool TileCentre(uintptr_t comp, int* cx, int* cy) {
    const uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);
    if (!entity) return false;
    uintptr_t mcIbp = 0;
    FloorComponent_FindResData(reinterpret_cast<void*>(entity), &mcIbp);
    if (!mcIbp) return false;
    // mcIbp already IS the MapComponent: the finder writes the component into the
    // out-param and returns that out-param's address, so retail's `*self_00` and this
    // local are the same value — no second dereference.
    const uintptr_t mc = mcIbp;
    *cx = (*reinterpret_cast<int16_t*>(mc + 0x2c) >> 1) + *reinterpret_cast<int16_t*>(mc + 0x28);
    *cy = (*reinterpret_cast<int16_t*>(mc + 0x2e) >> 1) + *reinterpret_cast<int16_t*>(mc + 0x2a);
    return true;
}

// retail 0x14011eed0 — FishingComponent_helper_eed0(self, delta)
void StampCoverage(uintptr_t comp, short delta) {
    const int r = RadiusOf(comp);
    int cx = 0, cy = 0;
    if (r < 0 || !TileCentre(comp, &cx, &cy) ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton) {
        return;
    }
    int* mgr = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!mgr) return;
    FishingComponent_BrushTile(mgr, kFishingCoverageState, cx, cy, r, delta);
}

}  // namespace

// [19] 0x14011ee80
// ── slot 19 (+0x98) OnEnterProducing — retail 0x14011ee80 ─────────────────────────
char FishingComponent::OnEnterProducing() {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<uint8_t*>(comp + 0x50) != 0) return 0;   // already registered
    StampCoverage(comp, +1);
    FishingComponent::ProduceTick(nullptr);                        // retail: Time(0.0); qualified, as the free call was
    *reinterpret_cast<uint8_t*>(comp + 0x50) = 1;
    return 0;
}
// [20] 0x14011ee50
// ── slot 20 (+0xa0) OnExitProducing — retail 0x14011ee50 ──────────────────────────
char FishingComponent::OnExitProducing() {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<uint8_t*>(comp + 0x50) == 0) return 0;   // not registered
    StampCoverage(comp, -1);
    *reinterpret_cast<uint8_t*>(comp + 0x50) = 0;
    return 0;
}
// [21] 0x14011ec10
// ── slot 21 (+0xa8) ProduceTick — retail 0x14011ec10 ──────────────────────────────
// Recomputes the dock's fishing rate: over the disk of the RadiusDescription radius,
// count in-bounds tiles (total) and those whose terrain byte (cell+0x29) is set in the
// FishingDescription's accepted-terrain bitmask (desc+0x2c), then
//     +0x54 = matching / (total * 0.5)          [DAT_140172114 = 0.5f]
// A dock with no water in range therefore rates 0 and produces nothing — which is the
// retail behaviour players see when a dock is placed too far up the bank.
char FishingComponent::ProduceTick(void* /*time*/) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(this);

    const int r = RadiusOf(comp);
    int cx = 0, cy = 0;
    if (r < 0 || !TileCentre(comp, &cx, &cy) ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return 0;

    int* mgr = *reinterpret_cast<int**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!mgr) return 0;
    const int width = *mgr;
    const uintptr_t grid = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(mgr) + 0x80);
    if (!grid || width <= 0) return 0;

    // The accepted-terrain mask lives on the description at owner+0x18, but only if it
    // really is a FishingDescription — retail runs the same IsDerivedFrom filter and
    // would read a null otherwise.
    void* descObj = *reinterpret_cast<void**>(comp + 0x18);
    uint32_t mask = 0;
    if (descObj && ::Object::IsDerivedFromNullSafe(descObj, FishingDescription::TypeId()))
        mask = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(descObj) + 0x2c);

    int total = 0, matching = 0;
    for (int dy = -r; dy < r; ++dy) {
        for (int dx = -r; dx < r; ++dx) {
            if (dx * dx + dy * dy > r * r) continue;
            const int x = cx + dx, y = cy + dy;
            if (x < 0 || y < 0 || x >= width || y >= width) continue;
            ++total;
            const uint8_t terrain =
                *reinterpret_cast<uint8_t*>(grid + (uintptr_t)(y * width + x) * 0x30 + 0x29);
            if ((mask >> (terrain & 0x1f)) & 1) ++matching;
        }
    }

    const float rate = (total > 0) ? (float)matching / ((float)total * 0.5f) : 0.0f;
    *reinterpret_cast<float*>(comp + 0x54) = rate;

    return 0;
}
