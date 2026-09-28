// code/Game/Component/tornadocomponent.cpp
// Clean reimplementation of the TornadoComponent lane from Application-x64.exe.
//
// TornadoComponent (0x68 bytes, vtable 0x14015aa80) carries ALL tornado gameplay:
// the path across the map, the movement integration, and the destruction. The
// `Tornado` type in code/Game/Game/tornado.cpp is the *particle* (Graphics::Particle)
// and is purely cosmetic — it holds no gameplay code whatsoever.
//
// SLOT IDENTIFICATION — from the vtable BYTES at 0x14015aa80, not Ghidra's names.
// Ghidra labels these three "_Shutdown", "_Serialize" and "_OnRemove"; all three
// are off by one. Only these three are overridden, the other 13 are base-Component
// no-ops in 0x140118xxx (e.g. 0x140118a60 is a bare `ret`):
//   slot 3 (+0x18) 0x140058b80  Serialize  — save/load
//   slot 4 (+0x20) 0x1400585a0  Startup    — pick endpoints, build the path, post the event
//   slot 8 (+0x40) 0x140057cd0  Update     — per-frame movement + damage
//
// WHY SLOT 8 AND NOT SLOT 9. The house rule (charactercomponent.cpp) is that slot 8
// (+0x40) is the full-rate EntityList update-TREE walk (component flag bit 2) and
// slot 9 (+0x48) is the throttled 5-bucket round-robin (flag bit 3). The ctor ORs
// 0x84 = bit 2 | bit 7, so the tornado is a bit-2 component and ticks at full rate
// through slot 8. Wiring the movement at slot 9 would run it at the bucket rate.
//
// LIFETIME IS THE PARTICLE'S, NOT THE PATH'S. Update's first act is to fetch the
// entity's ParticleComponent (0x140005200) and test `count(+0x40) > 0 &&
// firstInstance.field8 >= 0`; when that fails the tornado queues ITSELF for
// destruction. Path exhaustion does not kill it directly — it calls 0x140053720,
// which flips the particle's playback rate NEGATIVE so the funnel winds down, and
// the gate trips a few frames later. (The type-id at 0x1401c4758 that 0x140053720
// checks is ParticleDescription, which is what identifies the component.)
//
// SPAWNING is debug-only in retail: "Template/Tornado.rsc" has exactly one xref,
// inside Game_SpawnTornado (0x14005fd20), whose only caller is the debug panel.
// That function is already ported at code/Game/Dialog/debugui.cpp (command 329).
//
// GATE: BANISHED_LIVE_TORNADO — DEFAULT-ON since 2026-08-22 (the default-on table); the
// env var is now a kill-switch (=0). Trace tag [Tornado] on stderr.
//
// The particle half is done too: Tornado vtable slots 1/2/3/5/6/7/8/9 and
// TornadoInstance::Update (0x1400d2470) are all ported in code/Game/Game/tornado.cpp,
// so the lifetime gate above is now driven by a real particle system finishing rather
// than by a dormant one.

// graphics/Mesh.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include "Component/tornadocomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "options.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A residual
#include "io/Stream.h"      // ENDGAME B.3.4 callers batch H: IO::Stream::GetDirection
#include <new>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Object/factory_registration.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include "Component/TornadoDescription.h"
#include "Component/ParticleDescription.h"
#include <atomic>

// "[Tornado] STRUCK a building" trace budget (was a function-local static; B6-5).
static std::atomic<int> s_tornadoStruckTraced{0};

// EventLog::AddEvent (0x140095580) — already ported in code/Game/Game/eventlog.cpp.
// Reused rather than re-inlined: retail's Startup builds the same 0x28-byte record
// by hand, and its literals (colour 0xffff6060, icon 4, popup -1) are exactly what
// that port's severity/icon/popup tables produce for type 0x1e / severity 3.
// Porting the same retail body a second time here is the trap that
// reference_one_retail_fn_ported_twice_diverges records.
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);

// 0x140131c90 = sinf, 0x140131090 = cosf, both already ported in
// code/Game/Component/character_trig.cpp (IEEE arg-reduced in retail).
extern "C" float CharacterComponent_Sinf_131c90(float x);
extern "C" float CharacterComponent_Cosf_131090(float x);

// ============================================================
// .rdata constants (read from the binary, values in the comments)
// ============================================================
namespace {

constexpr float kHalf            = 0.5f;          // DAT_140172114
constexpr float kPathJitter      = 0.25f;         // DAT_140172100
constexpr float kArriveRadius    = 3.0f;          // DAT_140172180
constexpr float kSkipRadius      = 10.0f;         // DAT_1401721b0
constexpr float kHalfCircleDeg   = 180.0f;        // DAT_140172220
constexpr float kDegToRad        = 0.017453292f;  // DAT_1401720cc

// The path is generated over indices [0 .. kPathLast]; Startup then pushes
// kPathLast + 1 waypoints. Both numbers are literals in 0x1400585a0.
constexpr int kPathLast  = 0x80;
constexpr int kPathNodes = kPathLast + 1;   // 129

// ============================================================
// Resolved helpers
// ============================================================


// Entity component finders. All take (entity, InternalBasePtr* out) and write the
// component pointer (or null) into *out.
typedef void* (*fn_Finder_t)(void* entity, void* outIbp);

typedef void  (*fn_ParticleReverse_t)(void* particleComp, int index);          // 0x140053720
typedef void  (*fn_AudioMoveEmitters_t)(void* audioComp, void* newPos);        // 0x140042960
typedef float*(*fn_TerrainClamp_t)(void* map, float* out, const float* in, char flag, void* extra); // 0x14009f3e0
typedef void  (*fn_MarkForRemoval_t)(void* entity);                            // 0x1400c2ea0
typedef void  (*fn_EntityListDestroy_t)(void* entityList, void* ibp);          // 0x1400c49d0
typedef void  (*fn_ClearPathQueue_t)(void* entity, void* outIbp);              // 0x140003f60
typedef void  (*fn_GrowthKnockdown_t)(void* naturalResourceComp);              // 0x140126780
typedef bool  (*fn_FireBurning_t)(void* fireComp);                             // 0x14011e690
typedef bool  (*fn_FireIgnite_t)(void* fireComp, void* time);                  // 0x14011e7b0
typedef void  (*fn_FireIgniteFinish_t)(void* fireComp);                        // 0x14011ea10
typedef void  (*fn_CitizenLifeEvent_t)(void* citizenComp, int event, const char* p); // 0x140116dd0
typedef void* (*fn_Map_RadiusQuery_t)(float* map, void** out, float* pos, float* radius,
                                      int typeId, void* outArr);               // 0x140092990

// Graphics leaf reached through the exe's own .rdata dispatch slot.

struct Helpers {
    fn_Finder_t               findParticle         = nullptr;  // 0x140005200
    fn_Finder_t               findAudio            = nullptr;  // 0x140002280
    fn_Finder_t               findNaturalResource  = nullptr;  // 0x1400052c0
    fn_Finder_t               findGrowth           = nullptr;  // 0x14000c460
    fn_Finder_t               findBuild            = nullptr;  // 0x14000d6c0
    fn_Finder_t               findFire             = nullptr;  // 0x140005840
    fn_Finder_t               findFireOwner        = nullptr;  // 0x140005780
    fn_Finder_t               findCitizen          = nullptr;  // 0x140009790
    fn_Finder_t               findAnimal           = nullptr;  // 0x140005500 AnimalComponent

    fn_ParticleReverse_t      particleReverse      = nullptr;
    fn_AudioMoveEmitters_t    audioMoveEmitters    = nullptr;
    fn_TerrainClamp_t         terrainClamp         = nullptr;
    fn_MarkForRemoval_t       markForRemoval       = nullptr;
    fn_ClearPathQueue_t       clearPathQueue       = nullptr;
    fn_GrowthKnockdown_t      growthKnockdown      = nullptr;
    fn_FireBurning_t          fireBurning          = nullptr;
    fn_FireIgnite_t           fireIgnite           = nullptr;
    fn_FireIgniteFinish_t     fireIgniteFinish     = nullptr;
    fn_CitizenLifeEvent_t     citizenLifeEvent     = nullptr;
    fn_Map_RadiusQuery_t      radiusQuery          = nullptr;

};

Helpers g_h;
bool g_helpersReady = false;

template <typename T>
T ResolveSlotPtr(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<T*>(slot) : nullptr;
}

// ---- tracing -------------------------------------------------------------
// The lane is opt-in and the volume is a handful of lines per tornado (spawn,
// one per damage tick, one per 16 waypoints, exhaustion, expiry), so tracing is on
// by default whenever the feature is. BANISHED_TORNADO_TRACE=0 silences it.
int g_trace = -1;
bool TraceOn() {
    if (g_trace < 0) {
        const char* v = Opt().tornadoTrace;
        g_trace = (v && (std::strcmp(v, "0") == 0 || std::strcmp(v, "false") == 0)) ? 0 : 1;
    }
    return g_trace != 0;
}
#define TORNADO_TRACE(...)                                       \
    do {                                                         \
        if (TraceOn()) {                                         \
            std::fprintf(stderr, "[Tornado] " __VA_ARGS__);      \
            std::fflush(stderr);                                 \
        }                                                        \
    } while (0)

void* GameWorld() {
    return g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
}

// The TornadoDescription behind this component. Retail:
//     obj = *(void**)(self + 0x18);
//     desc = IsDerivedFrom(obj, TypeId_TornadoDescription) ? obj : nullptr;
// and then dereferences `desc` unguarded. The type-id global (0x1401c4aa0) has no
// storage in standalone, so the id comes from the runtime type registry instead —
// the standard substitution here (see tornadodescription.cpp).
void* TornadoDesc(void* self) {
    void* obj = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x18);
    // GetTypeIdByName returns 0xFFFFFFFF before Object::Factory::Startup has run, so
    // only latch a real id — otherwise the sentinel gets cached forever.
    const uint32_t typeId = TornadoDescription::TypeId();
    if (!fn_IsDerivedFrom || typeId == 0 || typeId == 0xFFFFFFFFu) return obj;
    return fn_IsDerivedFrom(obj, typeId) ? obj : nullptr;
}


}  // namespace

// ============================================================
// Init: resolve function pointers
// ============================================================
void TornadoComponent_InitFuncs() {
    if (g_helpersReady) return;
    g_helpersReady = true;

    g_h.findParticle        = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140005200));
    g_h.findAudio           = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140002280));
    g_h.findNaturalResource = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    g_h.findGrowth          = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x14000c460));
    g_h.findBuild           = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x14000d6c0));
    g_h.findFire            = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140005840));
    g_h.findFireOwner       = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140005780));
    g_h.findCitizen         = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140009790));
    g_h.findAnimal           = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140005500));

    g_h.particleReverse   = reinterpret_cast<fn_ParticleReverse_t>(ResolveAddr(Module::Exe, 0x140053720));
    g_h.audioMoveEmitters = reinterpret_cast<fn_AudioMoveEmitters_t>(ResolveAddr(Module::Exe, 0x140042960));
    g_h.terrainClamp      = reinterpret_cast<fn_TerrainClamp_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    g_h.markForRemoval    = reinterpret_cast<fn_MarkForRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    g_h.clearPathQueue    = reinterpret_cast<fn_ClearPathQueue_t>(ResolveAddr(Module::Exe, 0x140003f60));
    g_h.growthKnockdown   = reinterpret_cast<fn_GrowthKnockdown_t>(ResolveAddr(Module::Exe, 0x140126780));
    g_h.citizenLifeEvent  = reinterpret_cast<fn_CitizenLifeEvent_t>(ResolveAddr(Module::Exe, 0x140116dd0));
    g_h.radiusQuery       = reinterpret_cast<fn_Map_RadiusQuery_t>(ResolveAddr(Module::Exe, 0x140092990));

    // The FIRE lane owns these three (0x14011e690 / 0x14011e7b0 / 0x14011ea10).
    // They are resolved, never ported here — see the note on Tornado_StrikeEntity.
    g_h.fireBurning     = reinterpret_cast<fn_FireBurning_t>(ResolveAddr(Module::Exe, 0x14011e690));
    g_h.fireIgnite      = reinterpret_cast<fn_FireIgnite_t>(ResolveAddr(Module::Exe, 0x14011e7b0));
    g_h.fireIgniteFinish= reinterpret_cast<fn_FireIgniteFinish_t>(ResolveAddr(Module::Exe, 0x14011ea10));

    // ── Arm-viability report ────────────────────────────────────────────────
    // Each damage arm needs EVERY leaf on its own path; a single null one makes the
    // arm silently unreachable and its counter reads 0 exactly like "the funnel never
    // crossed one". That is what hid an unregistered FindComponent<FireComponent>
    // (0x140005840) for a whole wave: the building arm could not fire, and
    // `building=0 building-NOFIRE=0` on 40,000 damage ticks looked like bad luck.
    // Print each arm's verdict ONCE, unconditionally — not behind the trace env,
    // because the case that matters is the one nobody is looking at.
    {
        auto arm = [](const char* name, bool ok, const char* missing) {
            std::fprintf(stderr, "[Tornado] arm %-9s %s%s%s\n", name,
                         ok ? "LIVE" : "DEAD", ok ? "" : " — unresolved: ",
                         ok ? "" : missing);
        };
        arm("tree",     g_h.findNaturalResource && g_h.findGrowth && g_h.growthKnockdown,
            "findNaturalResource/findGrowth/growthKnockdown");
        arm("building", g_h.findBuild && g_h.findFire && g_h.findFireOwner &&
                        g_h.fireIgnite && g_h.fireIgniteFinish,
            "findBuild(d6c0)/findFire(5840)/findFireOwner(5780)/ignite(e7b0)/finish(ea10)");
        arm("animal",   g_h.findAnimal && g_h.markForRemoval,
            "findAnimal(5500)/markForRemoval");
        arm("citizen",  g_h.findCitizen && g_h.citizenLifeEvent,
            "findCitizen(9790)/citizenLifeEvent(116dd0)");
        arm("audio",    g_h.findAudio && g_h.audioMoveEmitters,
            "findAudio(2280)/audioMoveEmitters");
        arm("path",     g_h.terrainClamp != nullptr,
            "terrainClamp(9f3e0) - endpoints are NOT terrain-clamped");
        std::fflush(stderr);
    }


    TORNADO_TRACE("init: fire lane %s (e690=%p e7b0=%p ea10=%p) — the building arm is "
                  "%s until the fire lane lands\n",
                  (g_h.fireIgnite && g_h.fireIgniteFinish) ? "RESOLVED" : "NULL",
                  (void*)g_h.fireBurning, (void*)g_h.fireIgnite, (void*)g_h.fireIgniteFinish,
                  (g_h.fireIgnite && g_h.fireIgniteFinish) ? "live" : "INERT");
}

// 0x140058b80 door: registered by address (standalone/reg_tornado.cpp).
void TornadoComponent_Shutdown(TornadoComponent* self, void* stream) {
    self->TornadoComponent::Persist(stream);
}


// ============================================================
// Tornado_PathFractal — 0x140058980
// ============================================================
// Recursive midpoint displacement between path[lo] and path[hi]:
//     mid       = (a + b) * 0.5
//     dir,len   = normalize(b - a)                 (Normalize with an out-length)
//     perp      = normalize(-dir.y, dir.x, 0)      (XY perpendicular)
//     path[m]   = mid + perp * len * 0.25 * RandFloat()
// then recurse on both halves while the span is wider than one index.
//
// THE `* 0.0` IN THE DECOMPILATION IS AN ARTIFACT — the path really is fractal.
// Ghidra renders the displacement terms as `local_6X * 0.0 * DAT_140172100`, which
// would collapse the whole thing into a straight line. The disassembly at
// 0x140058ad8..0x140058b00 is `MULSS XMM4,[0x140172100]` (0.25) then `MULSS XMM4,XMM0`
// (the RandFloat result) then `ADDSS XMM4,XMM6` (the midpoint) — no zero anywhere.
// The decompiler invented it by mis-modelling Normalize's out-parameters.
// The one genuine zero is the Z term: perp.z is 0 by construction, so mid.z is the
// plain average of the endpoints. That one is real and is kept.
extern "C" void Tornado_PathFractal_58980(void* self, float* path, int lo, int hi) {
    const float* a = path + static_cast<ptrdiff_t>(lo) * 3;
    const float* b = path + static_cast<ptrdiff_t>(hi) * 3;

    Math::Vector3 seg(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    Math::Scalar len(0.0f);
    Math::Vector3 dir = seg.Normalize(len);

    const float midX = (a[0] + b[0]) * kHalf;
    const float midY = (a[1] + b[1]) * kHalf;
    const float midZ = (a[2] + b[2]) * kHalf;

    Math::Vector3 perpIn(-dir.y, dir.x, 0.0f);
    Math::Vector3 perp = perpIn.Normalize();

    void* gw = GameWorld();
    void* rng = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xd8) : nullptr;
    const float r = rng ? reinterpret_cast<System::Random*>(rng)->RandFloat() : 0.0f;   // B.3.4b bridge

    const int mid = (lo + hi) >> 1;
    float* m = path + static_cast<ptrdiff_t>(mid) * 3;
    m[0] = perp.x * len.v * kPathJitter * r + midX;
    m[1] = perp.y * len.v * kPathJitter * r + midY;
    m[2] = perp.z * len.v * kPathJitter * r + midZ;

    if (mid - lo > 1) Tornado_PathFractal_58980(self, path, lo, mid);
    if (hi - mid > 1) Tornado_PathFractal_58980(self, path, mid, hi);
}

// ============================================================
// Tornado_PathFindNext — 0x1400a70c0
// ============================================================
// Scan forward from the current index for the first waypoint whose squared XY
// distance from `pos` is at least `radius * radius`. Returns -1 past the end.
// `block` is `self + 0x30`, so block+0x10/+0x18/+0x20 are pathData / pathCount /
// pathIndex. The Z term is `pos.z - 0.0` — retail ignores the waypoint's Z here.
extern "C" int Tornado_PathFindNext_a70c0(uint8_t* block, const float* pos, float radius) {
    int i = *reinterpret_cast<int*>(block + 0x20);
    for (;;) {
        ++i;
        if (i >= *reinterpret_cast<int*>(block + 0x18)) return -1;
        const float* wp = *reinterpret_cast<float**>(block + 0x10) + static_cast<ptrdiff_t>(i) * 3;
        const float dx = pos[0] - wp[0];
        const float dy = pos[1] - wp[1];
        const float dz = pos[2] - 0.0f;
        // Math::Vector3::Dot, inlined: a pure leaf with no side effects.
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= radius * radius) return i;
    }
}

// ============================================================
// Tornado_PathAdvance — 0x1400a71e0
// ============================================================
// If the current waypoint is closer than `arriveRadius`, pick the next waypoint
// more than `skipRadius` away and latch it into block[0..2] (self+0x30..0x38, with
// Z forced to 0). Returns false only when the path is exhausted.
extern "C" bool Tornado_PathAdvance_a71e0(uint8_t* block, const float* pos,
                                          float skipRadius, float arriveRadius) {
    Math::Vector3 d(*reinterpret_cast<float*>(block + 0x00) - pos[0],
                    *reinterpret_cast<float*>(block + 0x04) - pos[1],
                    0.0f);
    const float len = d.Length().v;

    if (len < arriveRadius) {
        const int next = Tornado_PathFindNext_a70c0(block, pos, skipRadius);
        if (next < 0) return false;
        *reinterpret_cast<int*>(block + 0x20) = next;
        const float* wp = *reinterpret_cast<float**>(block + 0x10) + static_cast<ptrdiff_t>(next) * 3;
        *reinterpret_cast<float*>(block + 0x00) = wp[0];
        *reinterpret_cast<float*>(block + 0x04) = wp[1];
        *reinterpret_cast<float*>(block + 0x08) = 0.0f;
    }
    return true;
}

// ============================================================
// Tornado_StrikeEntity — 0x1400583c0
// ============================================================
// The per-entity effect. Finds six components off the entity and picks one arm.
//
// FAITHFUL ODDITY: `unusedFirstArg` is dead in retail — the prologue stores it to a
// local that the very next block overwrites, and one of the three call sites passes
// outright junk (`cellIndex * 6`). It is kept in the signature, and the junk is
// still passed, so the port matches the binary rather than tidying it.
//
// The building arm ignites via the FIRE lane's 0x14011e7b0 / 0x14011ea10. Those are
// NOT ported here (a peer owns them); when they resolve null the arm is inert and
// says so, instead of silently doing nothing.
static void Tornado_StrikeEntityCounted(int64_t unusedFirstArg, void* entityIbp,
                                 int* hitTree, int* hitTreeNoKnock,
                                 int* hitBuilding, int* hitBuildingNoFire,
                                 int* hitAnimal, int* hitCitizen) {
    (void)unusedFirstArg;

    void* entity = *reinterpret_cast<void**>(entityIbp);

    int64_t fireComp = 0, buildComp = 0, growthComp = 0, natResComp = 0;
    int64_t citizenComp = 0, animalComp = 0;

    if (g_h.findFire)            { g_h.findFire(entity, &fireComp); }
    if (g_h.findBuild)           { g_h.findBuild(entity, &buildComp); }
    if (g_h.findGrowth)          { g_h.findGrowth(entity, &growthComp); }
    if (g_h.findNaturalResource) { g_h.findNaturalResource(entity, &natResComp); }
    if (g_h.findCitizen)         { g_h.findCitizen(entity, &citizenComp); }
    if (g_h.findAnimal)           { g_h.findAnimal(entity, &animalComp); }

    // The first few buildings the funnel touches, and which way the arm went.
    // Unconditional and one-shot: if this never appears, no tornado has swept a
    // building's cell and the arm is merely untested — a fact `building=0` cannot
    // convey on its own. See the arm-viability report in TornadoComponent_InitFuncs.
    if (buildComp) {
        if (s_tornadoStruckTraced.fetch_add(1, std::memory_order_relaxed) < 4) {
            std::fprintf(stderr, "[Tornado] STRUCK a building: entity=%p build=%llx fire=%llx"
                                 " -> arm %s\n",
                         entity, (unsigned long long)buildComp, (unsigned long long)fireComp,
                         fireComp ? "TAKEN (igniting)"
                                  : "SKIPPED - entity carries no FireComponent");
            std::fflush(stderr);
        }
    }

    if (natResComp != 0 && growthComp != 0) {
        // --- tree / crop: stop it walking its growth queue and knock it down ---
        int64_t meshIbp = 0;
        if (g_h.clearPathQueue) { g_h.clearPathQueue(entity, &meshIbp); }
        const bool animating =
            meshIbp
                ? reinterpret_cast<const Graphics::MeshInstance*>(
                      static_cast<uintptr_t>(meshIbp) + 0x28)->IsAnimating()   // B.3.5 bridge
                : false;
        if (!animating) {
            *reinterpret_cast<int32_t*>(static_cast<uintptr_t>(natResComp) + 0x28) = 0;
            // 0x140126780 is what actually FELLS the tree: it sets flag bit 0x200000,
            // notifies, and starts fall animation mode 2. Zeroing +0x28 alone only empties
            // the resource. Count the two outcomes apart -- a hitTree that also counted an
            // unresolved knockdown would report a felled forest that is still standing.
            if (!g_h.growthKnockdown) {
                if (hitTreeNoKnock) ++*hitTreeNoKnock;
                return;
            }
            g_h.growthKnockdown(reinterpret_cast<void*>(static_cast<uintptr_t>(natResComp)));
            if (hitTree) ++*hitTree;
        }
        return;
    }

    if (buildComp != 0 && fireComp != 0) {
        // --- building: set it alight (the tornado does NOT demolish) ---
        int64_t fireOwner = 0;
        if (g_h.findFireOwner) { g_h.findFireOwner(entity, &fireOwner); }
        const bool alreadyBurning =
            (g_h.fireBurning && fireOwner)
                ? g_h.fireBurning(reinterpret_cast<void*>(static_cast<uintptr_t>(fireOwner)))
                : false;
        if (!alreadyBurning) {
            if (!g_h.fireIgnite || !g_h.fireIgniteFinish) {
                if (hitBuildingNoFire) ++*hitBuildingNoFire;
                return;
            }
            int64_t zero = 0;
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&zero, 0.0f);
            const bool lit =
                g_h.fireIgnite(reinterpret_cast<void*>(static_cast<uintptr_t>(fireOwner)), &zero);
            if (lit) {
                g_h.fireIgniteFinish(reinterpret_cast<void*>(static_cast<uintptr_t>(fireOwner)));
                if (hitBuilding) ++*hitBuilding;
            }
        }
        return;
    }

    if (animalComp != 0) {
        // --- ANIMAL (livestock or wild): remove it from the world ---
        // 0x140005500 matches type id 0x1401c6f98 = AnimalComponent (verified: the
        // finder compares against DAT_1401c6f98, and factory_registrations.csv maps
        // that global to AnimalComponent_RegisterType @ 0x14014e510). Its other
        // readers agree — HerdComponent_StartMigration, PopulationComponent_helper_0f50,
        // and 0x140047d70 (the crop lane's KillOneAnimal); reg_action_hunt.cpp:16
        // registers it as ActHunt_FindComponent_5500.
        // This arm was originally named "loose entity" here, which was wrong and would
        // have mis-read the soak: a tornado crossing a pasture reports its kills here,
        // and filing livestock losses under "debris" hides a herd wipe.
        void* gw = GameWorld();
        void* entityMgr = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xe8)
                             : nullptr;
        int64_t ibp = reinterpret_cast<int64_t>(entity);
        // Inlined 0x1400c49d0 (EntityList::QueueDestroy): skip entities already
        // flagged (bit 10 of entity+0x40), else MarkForRemoval + push onto the
        // pending-destroy array at entityList+0xd0 / count +0xd8.
        if (entityMgr && ibp != 0) {
            const uint32_t f = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ibp) + 0x40);
            if (((f >> 10) & 1) == 0) {
                if (g_h.markForRemoval)
                    g_h.markForRemoval(reinterpret_cast<void*>(static_cast<uintptr_t>(ibp)));
                uintptr_t mgr = reinterpret_cast<uintptr_t>(entityMgr);
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(mgr + 0xd0))
                    .SetSizeFill(*reinterpret_cast<int*>(mgr + 0xd8) + 1, &ibp);
                if (hitAnimal) ++*hitAnimal;
            }
        }
        return;
    }

    if (citizenComp != 0) {
        // --- citizen: life event 5 (killed) ---
        if (g_h.citizenLifeEvent)
            g_h.citizenLifeEvent(reinterpret_cast<void*>(static_cast<uintptr_t>(citizenComp)),
                                 5, nullptr);
        if (hitCitizen) ++*hitCitizen;
    }
}

// ============================================================
// Tornado_DamageTick — 0x140058040
// ============================================================
// Runs down a countdown; on each expiry it strikes three sets of entities:
//   1. one random cell within desc+0x34 of the funnel, but ONLY if that cell's
//      entity owns a NaturalResourceComponent (retail's own extra guard);
//   2. everything the quadtree radius query returns within desc+0x30;
//   3. every occupied cell of the (2R+1)^2 square around the funnel.
// The cell entity raster is map->cells[idx].+8 (stride 0x30) — the cell+8 raster,
// not the cell+0 zone raster.
extern "C" void Tornado_DamageTick_58040(TornadoComponent* self, const int64_t* dt) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* desc = TornadoDesc(self);
    uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    *reinterpret_cast<int64_t*>(s + 0x28) -= *dt;
    if (*reinterpret_cast<int64_t*>(s + 0x28) > 0) return;
    *reinterpret_cast<int64_t*>(s + 0x28) += *reinterpret_cast<int64_t*>(d + 0x28);

    void* gw = GameWorld();
    if (!gw) return;
    uintptr_t g = reinterpret_cast<uintptr_t>(gw);
    void* rng = *reinterpret_cast<void**>(g + 0xd8);
    int* map = *reinterpret_cast<int**>(g + 0xf8);

    // Random point within the scatter radius: angle in [-pi, pi], radius scaled by
    // RandFloat. Z is a genuine hard zero here (`XORPS XMM4,XMM4` at 0x140058110).
    const float angle = fn_Random_RandFloatTol(rng) * kHalfCircleDeg * kDegToRad;
    const float frac  = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
    const float scatter = static_cast<float>(*reinterpret_cast<int32_t*>(d + 0x34));

    // The Vector3 ctor argument order (XMM1/XMM2/XMM3 at 0x140058121) puts sin on X
    // and cos on Y.
    const float ox = CharacterComponent_Sinf_131c90(angle) * scatter * frac;
    const float oy = CharacterComponent_Cosf_131090(angle) * scatter * frac;

    const float px = ox + *reinterpret_cast<float*>(s + 0x58);
    const float py = oy + *reinterpret_cast<float*>(s + 0x5c);
    const float pz = 0.0f + *reinterpret_cast<float*>(s + 0x60);

    int hitTree = 0, hitTreeNoKnock = 0, hitBuilding = 0, hitBuildingNoFire = 0;
    int hitAnimal = 0, hitCitizen = 0;

    const int N = *map;
    uintptr_t cells = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(map) + 0x80);

    auto clampCell = [N](float v) {
        int i = static_cast<int>(v + static_cast<float>(N >> 1));
        if (i < 0) i = 0;
        if (i >= N) i = N - 1;
        return i;
    };

    // --- 1. the scattered single strike (natural resources only) ---
    {
        const int ix = clampCell(px);
        const int iy = clampCell(py);
        int64_t cellEntity =
            *reinterpret_cast<int64_t*>(cells + 8 + static_cast<uintptr_t>(iy * N + ix) * 0x30);
        if (cellEntity != 0) {
            int64_t probe = 0;
            if (g_h.findNaturalResource)
                g_h.findNaturalResource(reinterpret_cast<void*>(static_cast<uintptr_t>(cellEntity)),
                                        &probe);
            if (probe != 0) {
                int64_t ibp = cellEntity;
                Tornado_StrikeEntityCounted(cellEntity, &ibp, &hitTree, &hitTreeNoKnock,
                                     &hitBuilding, &hitBuildingNoFire, &hitAnimal, &hitCitizen);
            }
        }
    }

    // --- 2. the quadtree radius query ---
    struct { uintptr_t data; int count; int cap; } found = { 0, 0, 0 };
    const char* queryState = "ran";
    {
        float radius = static_cast<float>(*reinterpret_cast<int32_t*>(d + 0x30));
        float pos[3] = { px, py, pz };
        void* scratch = nullptr;
        // The quadtree this query walks is the CHARACTER SPATIAL-HASH MANAGER at
        // GameWorld+0x88, NOT the map grid. Retail 0x14005825c loads 0x88(g_GameWorld)
        // into rcx immediately before the call; gw+0xf8 (r14, loaded at 0x140058188) is
        // a separate live value in the same function, used for the cell raster above --
        // which is exactly why passing it here looked plausible. It is not a quadtree,
        // so 0x140092990 read its +0x50 node-count table as garbage and faulted on null.
        // citizen_updatehealth.cpp:687 was already calling this correctly.
        //
        // The null test is a DELIBERATE standalone-only deviation, not a papered-over
        // fault: retail always has this manager, but ours is built by CharManager_Seed
        // (charmanager_seed.cpp), which early-returns unless MoveTickEnabled(). With
        // that gate off gw+0x88 is legitimately null here, so skip the query rather
        // than fault. Every other deref in this function stays unguarded.
        void* quadtree = *reinterpret_cast<void**>(g + 0x88);
        if (g_h.radiusQuery && quadtree)
            g_h.radiusQuery(reinterpret_cast<float*>(quadtree), &scratch, pos, &radius, 0, &found);
        else
            queryState = quadtree ? "SKIPPED(no radiusQuery fn)"
                                  : "SKIPPED(GameWorld+0x88 quadtree is null - "
                                    "CharManager_Seed gated off by MoveTickEnabled)";

        for (int i = 0; i < found.count; i++) {
            void* slot = reinterpret_cast<void*>(found.data + static_cast<uintptr_t>(i) * 8);
            int64_t ownerEntity =
                *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(slot) + 0x10);
            Tornado_StrikeEntityCounted(reinterpret_cast<int64_t>(slot), &ownerEntity,
                                 &hitTree, &hitTreeNoKnock, &hitBuilding, &hitBuildingNoFire,
                                 &hitAnimal, &hitCitizen);
        }
    }

    // --- 3. the (2R+1)^2 square sweep around the funnel itself ---
    {
        const int ix = clampCell(*reinterpret_cast<float*>(s + 0x58));
        const int iy = clampCell(*reinterpret_cast<float*>(s + 0x5c));
        const int idx = N * iy + ix;
        const int row = idx / N;
        const int col = idx % N;
        const int R = *reinterpret_cast<int32_t*>(d + 0x30);
        for (int dy = -R; dy <= R; dy++) {
            for (int dx = -R; dx <= R; dx++) {
                const int cx = col + dx;
                const int cy = row + dy;
                if (cx < 0 || cy < 0 || cx >= N || cy >= N) continue;
                const int64_t off = static_cast<int64_t>(N) * cy + cx;
                int64_t cellEntity =
                    *reinterpret_cast<int64_t*>(cells + 8 + static_cast<uintptr_t>(off) * 0x30);
                if (cellEntity == 0) continue;
                // Retail passes `off * 6` here — junk into a dead parameter. Kept.
                Tornado_StrikeEntityCounted(off * 6, &cellEntity, &hitTree, &hitTreeNoKnock,
                                     &hitBuilding, &hitBuildingNoFire, &hitAnimal, &hitCitizen);
            }
        }
    }

    if (found.data && fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(found.data));

    TORNADO_TRACE("damage at (%.1f,%.1f) radius=%d scatter=%d: tree=%d tree-NOKNOCK=%d "
                  "building=%d building-NOFIRE=%d animal=%d citizen=%d "
                  "(quadtree query %s, returned %d)\n",
                  *reinterpret_cast<float*>(s + 0x58), *reinterpret_cast<float*>(s + 0x5c),
                  *reinterpret_cast<int32_t*>(d + 0x30), *reinterpret_cast<int32_t*>(d + 0x34),
                  hitTree, hitTreeNoKnock, hitBuilding, hitBuildingNoFire, hitAnimal,
                  hitCitizen, queryState, found.count);
}

// 0x1400585a0 door: registered by address (standalone/reg_tornado.cpp).
void TornadoComponent_Serialize(TornadoComponent* self) {
    self->TornadoComponent::Startup();
}


// 0x140057cd0 door: registered by address (standalone/reg_tornado.cpp).
void TornadoComponent_OnRemove(TornadoComponent* self, void* time) {
    self->TornadoComponent::Tick(time);
}


// ============================================================
// Ported leaves that belong to other classes but had no owner yet
// ============================================================

// 0x140053720 — ParticleComponent: flip instance `index`'s playback rate negative
// so the effect winds down. rate = -(1.0 / desc->entries[index].duration).
//
// CROSS-LANE: this body and 0x140042960 below are LOAD-BEARING FOR THE FIRE LANE TOO.
// Both lanes independently ported these two (my address grep was clean when I ran it —
// the fire lane's files did not exist yet), which is how
// reference_one_retail_fn_ported_twice_diverges lands. The fire lane deleted its copies
// and now resolves these addresses, so this is the single surviving implementation.
// The two copies differed: theirs guarded `duration == 0` before the divide. This one
// divides unguarded, matching retail (`g_fUVScale / AsSeconds(...)` at 0x14005375e with
// no test), per the project rule against papering over a faithful crash. If a
// divide-by-zero ever appears here, a zero-duration particle entry is the upstream bug
// to find — do not add the guard.
// Description array stride is 0x28 (duration at +0x20); the component's geometry
// array stride is 0x18 (rate at +0x10). The mismatch is retail's, not a typo.
extern "C" void Tornado_ParticleReverse_53720(void* particleComp, int index) {
    uintptr_t p = reinterpret_cast<uintptr_t>(particleComp);
    void* obj = *reinterpret_cast<void**>(p + 0x18);
    const uint32_t typeId = ParticleDescription::TypeId();
    void* desc = obj;
    if (fn_IsDerivedFrom && typeId != 0 && typeId != 0xFFFFFFFFu)
        desc = fn_IsDerivedFrom(obj, typeId) ? obj : nullptr;

    uintptr_t geoArray = *reinterpret_cast<uintptr_t*>(p + 0x38);
    uintptr_t entries  = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
    const float duration = reinterpret_cast<const System::Time*>(
        reinterpret_cast<void*>(entries + static_cast<uintptr_t>(index) * 0x28 + 0x20))->AsSeconds();   // B.3.4b bridge
    const float rate = 1.0f / duration;
    uint32_t bits = *reinterpret_cast<const uint32_t*>(&rate);
    bits ^= 0x80000000u;  // DAT_1401722c0 sign flip
    *reinterpret_cast<uint32_t*>(geoArray + 0x10 + static_cast<uintptr_t>(index) * 0x18) = bits;
}

// 0x140042960 — AudioComponent: move every live emitter to `newPos`, then latch it
// into the component's cached position at +0x28. No-op when the position is
// unchanged (that early-out is retail's, and it is what keeps this cheap per frame).
extern "C" void Tornado_AudioMoveEmitters_42960(void* audioComp, void* newPos) {
    uintptr_t a = reinterpret_cast<uintptr_t>(audioComp);
    float* np = reinterpret_cast<float*>(newPos);
    float* cached = reinterpret_cast<float*>(a + 0x28);
    if (*reinterpret_cast<const Math::Vector3*>(np) == *reinterpret_cast<const Math::Vector3*>(cached))   // B.3.1 bridge
        return;

    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    const int count = *reinterpret_cast<int*>(a + 0x40);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(a + 0x38);
    for (int i = 0; i < count; i++) {
        const uint32_t id = *reinterpret_cast<uint32_t*>(arr + static_cast<uintptr_t>(i) * 0x10);
        if (id != 0 && gw)
            reinterpret_cast<Audio::Scene*>(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x78))
                ->MoveEmitter(id, *reinterpret_cast<const Vector3*>(cached),
                              *reinterpret_cast<const Vector3*>(np));   // B.3.7 bridge
    }
    cached[0] = np[0]; cached[1] = np[1]; cached[2] = np[2];
}

// ============================================================
// ENDGAME B.3.8 CIT — TornadoComponent as a REAL class (TornadoComponent.h)
// ============================================================
// Before: TWO tables — the 48-slot g_TornadoComponent_LiveVtable this file
// filled at every create, and a 16-slot copy in standalone/reg_tornado.cpp
// registered at the retail address 0x14015aa80 (the same three bodies at the
// same indices; only the Ghidra-derived NAMES differed). After: ONE compiler
// 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2); the retail
// address had no by-address reader, so its registration went too (B38 D5).
// Slot 8 — NOT 9 — is the per-frame tick for a flag-0x84 (bit 2) component.
#include "Object/factory_registration.h"
#include "Component/TornadoComponent.h"

// 0x140058cf0. Component() is the CALL 0x1401188b0; the 0x14015aa80 stamp is
// the compiler's; +0x28 = 0, the path block {+0x30 = 0, +0x38 (dword) = 0,
// +0x40 = +0x48 = 0, +0x50 = -1}, +0x58 = 0, +0x60 (dword) = 0; flags |= 0x84.
TornadoComponent::TornadoComponent()
    : damageCountdown(0), waypointX(0.0f), waypointY(0.0f), waypointZ(0.0f), _pad3C{},
      pathData(nullptr), pathCount(0), pathCapacity(0), pathIndex(-1), _pad54{},
      posX(0.0f), posY(0.0f), posZ(0.0f), _pad64{} {
    OrFlagsInCtor(0x84);
}

// ============================================================
// TornadoComponent::Serialize — vtable slot 3 (+0x18), 0x140058b80
// ============================================================
// Stream order: Vector3 waypoint(+0x30), int pathIndex(+0x50), int pathCount(+0x48),
// [on load: resize the Vector3 array], each waypoint, Time(+0x28), Vector3 pos(+0x58).
void TornadoComponent::Persist(void* stream) {
    TornadoComponent_InitFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<Math::Vector3*>(s + 0x30)->Serialize(serializer);   // B.3.1 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x50), nullptr);   // B.3.4 bridge

    int pathCount = *reinterpret_cast<int*>(s + 0x48);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(pathCount, nullptr);   // B.3.4 bridge

    // stream+8 == 0 means "loading": grow the array before reading the waypoints.
    // B.3.2 bridge: FUN_14000d030 is Collection::Array<Vector3>::SetSize(n)
    if (static_cast<int>(reinterpret_cast<IO::Stream*>(stream)->GetDirection()) == 0)   // B.3.4 bridge  (IO::Stream::GetDirection, retail 0x180006200)
        GameColl::AsArray<GameColl::Vec3>(reinterpret_cast<void*>(s + 0x40)).SetSize(pathCount);

    uint8_t* pVec = *reinterpret_cast<uint8_t**>(s + 0x40);
    if (pathCount > 0 && pVec) {
        for (int i = 0; i < pathCount; i++) {
            reinterpret_cast<Math::Vector3*>(pVec)->Serialize(serializer);   // B.3.1 bridge
            pVec += 0xC;  // sizeof(Math::Vector3)
        }
    }

    reinterpret_cast<System::Time*>(reinterpret_cast<void*>(s + 0x28))->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge
    reinterpret_cast<Math::Vector3*>(s + 0x58)->Serialize(serializer);   // B.3.1 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// ============================================================
// TornadoComponent::Startup — vtable slot 4 (+0x20), 0x1400585a0
// ============================================================
void TornadoComponent::Startup() {
    TornadoComponent_InitFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    void* desc = TornadoDesc(this);
    uintptr_t d = reinterpret_cast<uintptr_t>(desc);

    void* gw = GameWorld();
    if (!gw) return;
    uintptr_t g = reinterpret_cast<uintptr_t>(gw);
    void* rng = *reinterpret_cast<void**>(g + 0xd8);
    uint32_t* map = *reinterpret_cast<uint32_t**>(g + 0xf8);

    // --- pick two cells at least desc+0x24 apart (at most 100 tries) ---
    const int minSep = *reinterpret_cast<int32_t*>(d + 0x24);
    uint32_t ax = 0, ay = 0, bx = 0, by = 0;
    int tries = 0;
    do {
        ax = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(*map));   // B.3.4b bridge
        ay = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(*map));   // B.3.4b bridge
        bx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(*map));   // B.3.4b bridge
        by = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(*map));   // B.3.4b bridge
        if (tries > 99) break;
        ++tries;
    } while (static_cast<int>((bx - ax) * (bx - ax) + (by - ay) * (by - ay)) < minSep * minSep);

    // --- terrain-clamp both endpoints and fractal-fill between them ---
    float path[kPathNodes * 3];
    std::memset(path, 0, sizeof(path));

    const float half = static_cast<float>(static_cast<int>(*map) >> 1) - kHalf;

    auto seedEndpoint = [&](uint32_t cx, uint32_t cy, float* out) {
        const int n = static_cast<int>(*map);
        const int lin = static_cast<int>(n * cy + cx);
        float in[3] = { static_cast<float>(lin % n) - half,
                        static_cast<float>(lin / n) - half,
                        0.0f };
        float clamped[3] = { in[0], in[1], 0.0f };
        if (g_h.terrainClamp) {
            float* r = g_h.terrainClamp(map, clamped, in, 0, nullptr);
            if (r) { clamped[0] = r[0]; clamped[1] = r[1]; clamped[2] = r[2]; }
        }
        out[0] = clamped[0]; out[1] = clamped[1]; out[2] = clamped[2];
    };

    seedEndpoint(ax, ay, &path[0]);

    float endB[3];
    seedEndpoint(bx, by, endB);
    // The tornado STARTS at the first endpoint: retail copies it into +0x58 before
    // overwriting the scratch with the second endpoint.
    *reinterpret_cast<float*>(s + 0x58) = path[0];
    *reinterpret_cast<float*>(s + 0x5c) = path[1];
    *reinterpret_cast<float*>(s + 0x60) = path[2];
    path[2] = 0.0f;
    path[kPathLast * 3 + 0] = endB[0];
    path[kPathLast * 3 + 1] = endB[1];
    path[kPathLast * 3 + 2] = 0.0f;

    Tornado_PathFractal_58980(this, path, 0, kPathLast);

    // --- push the waypoints; the first one seeds the current target ---
    for (int i = 0; i < kPathNodes; i++) {
        float* p = &path[i * 3];
        if (*reinterpret_cast<int*>(s + 0x48) == 0) {
            *reinterpret_cast<float*>(s + 0x30) = p[0];
            *reinterpret_cast<float*>(s + 0x34) = p[1];
            *reinterpret_cast<float*>(s + 0x38) = p[2];
            *reinterpret_cast<int32_t*>(s + 0x50) = 0;
        }
        // B.3.2 bridge: FUN_14000cf40 is Collection::Array<Vector3>::SetSize(n, fill)
        GameColl::AsArray<GameColl::Vec3>(reinterpret_cast<void*>(s + 0x40))
            .SetSizeFill(*reinterpret_cast<int*>(s + 0x48) + 1, p);
    }

    // --- post the warning to the event log, and pause if the setting says so ---
    void* eventLog = *reinterpret_cast<void**>(g + 0x138);
    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    EventLog_AddEvent(eventLog, 0x1e, 3, &nullIbp);

    if (g_pGameSettingsPtr && *g_pGameSettingsPtr &&
        *(reinterpret_cast<const char*>(*g_pGameSettingsPtr) + 0x17) != 0) {
        void* speedObj = *reinterpret_cast<void**>(g + 0x120);
        if (speedObj) *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(speedObj) + 0x34) = 1;
    }

    // Diagnostic only: whether the fractal actually bent the path. Straight would
    // mean the `* 0.0` reading had been right after all.
    float maxDev = 0.0f;
    {
        const float sx = path[0], sy = path[1];
        const float ex = path[kPathLast * 3], ey = path[kPathLast * 3 + 1];
        const float vx = ex - sx, vy = ey - sy;
        const float vlen = vx * vx + vy * vy;
        for (int i = 1; i < kPathLast; i++) {
            const float wx = path[i * 3] - sx, wy = path[i * 3 + 1] - sy;
            const float cross = (vlen > 0.0f) ? (wx * vy - wy * vx) : 0.0f;
            const float dev = (vlen > 0.0f) ? (cross < 0 ? -cross : cross) : 0.0f;
            if (dev > maxDev) maxDev = dev;
        }
        if (vlen > 0.0f) maxDev /= (vlen > 0.0f ? __builtin_sqrtf(vlen) : 1.0f);
    }

    int64_t particleIbp = 0;
    if (g_h.findParticle) {
        void* owner = *reinterpret_cast<void**>(s + 0x10);
        g_h.findParticle(owner, &particleIbp);
    }

    TORNADO_TRACE("spawn: start=(%.1f,%.1f) end=(%.1f,%.1f) waypoints=%d "
                  "endpointTries=%d path=%s (maxLateralDeviation=%.2f) "
                  "speed=%.2f damageRadius=%d scatterRadius=%d particleComponent=%s\n",
                  path[0], path[1], path[kPathLast * 3], path[kPathLast * 3 + 1],
                  *reinterpret_cast<int*>(s + 0x48), tries,
                  (maxDev > 0.01f) ? "FRACTAL" : "STRAIGHT", maxDev,
                  *reinterpret_cast<float*>(d + 0x20),
                  *reinterpret_cast<int32_t*>(d + 0x30),
                  *reinterpret_cast<int32_t*>(d + 0x34),
                  particleIbp ? "found" : "MISSING (Update will fault on it, as retail would)");
}
// ============================================================
// TornadoComponent::Update — vtable slot 8 (+0x40), 0x140057cd0
// ============================================================
// Every frame: check the particle is still alive (else self-destruct), advance
// along the path, integrate the position, damage, then push the new transform and
// audio position out.
void TornadoComponent::Tick(void* time) {
    TornadoComponent_InitFuncs();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    const int64_t* dt = reinterpret_cast<const int64_t*>(time);
    void* desc = TornadoDesc(this);

    void* gw = GameWorld();
    if (!gw) return;
    uintptr_t g = reinterpret_cast<uintptr_t>(gw);

    void* owner = *reinterpret_cast<void**>(s + 0x10);
    int64_t particleIbp = 0;
    if (g_h.findParticle) g_h.findParticle(owner, &particleIbp);
    int* map = *reinterpret_cast<int**>(g + 0xf8);

    // FAITHFUL: retail dereferences the ParticleComponent unguarded — Tornado.rsc
    // always has one, so a null here is an upstream problem, not something to paper
    // over. Startup already logged whether the finder found it.
    uintptr_t pc = static_cast<uintptr_t>(particleIbp);
    const bool particleAlive =
        (*reinterpret_cast<int*>(pc + 0x40) > 0) &&
        (*reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(pc + 0x38) + 8) >= 0);

    if (!particleAlive) {
        // --- the funnel is gone: queue the tornado entity for destruction ---
        int64_t entity = *reinterpret_cast<int64_t*>(s + 0x10);
        uintptr_t entityMgr = *reinterpret_cast<uintptr_t*>(g + 0xe8);
        if (entity != 0) {
            const uint32_t f = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(entity) + 0x40);
            if (((f >> 10) & 1) == 0) {
                if (g_h.markForRemoval)
                    g_h.markForRemoval(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)));
                // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
                GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(entityMgr + 0xd0))
                    .SetSizeFill(*reinterpret_cast<int*>(entityMgr + 0xd8) + 1, &entity);
                TORNADO_TRACE("expired: particle finished, entity queued for destruction "
                              "(pathIndex=%d of %d)\n",
                              *reinterpret_cast<int32_t*>(s + 0x50),
                              *reinterpret_cast<int*>(s + 0x48));
            }
        }
        return;
    }

    // --- advance along the path ---
    float pos[3] = { *reinterpret_cast<float*>(s + 0x58),
                     *reinterpret_cast<float*>(s + 0x5c),
                     0.0f };
    const int idxBefore = *reinterpret_cast<int32_t*>(s + 0x50);
    const bool stillOnPath = Tornado_PathAdvance_a71e0(reinterpret_cast<uint8_t*>(s + 0x30), pos,
                                                       kSkipRadius, kArriveRadius);
    const int idxAfter = *reinterpret_cast<int32_t*>(s + 0x50);

    Math::Vector3 dir(*reinterpret_cast<float*>(s + 0x30) - *reinterpret_cast<float*>(s + 0x58),
                      *reinterpret_cast<float*>(s + 0x34) - *reinterpret_cast<float*>(s + 0x5c),
                      *reinterpret_cast<float*>(s + 0x38) - *reinterpret_cast<float*>(s + 0x60));
    Math::Vector3 unit = dir.Normalize();

    if (!stillOnPath) {
        // Path exhausted: reverse the particle's playback so the funnel winds down.
        if (g_h.particleReverse) g_h.particleReverse(reinterpret_cast<void*>(pc), 0);
        TORNADO_TRACE("path exhausted at waypoint %d of %d — funnel winding down\n",
                      idxAfter, *reinterpret_cast<int*>(s + 0x48));
    } else {
        Tornado_DamageTick_58040(this, dt);
        if (idxAfter != idxBefore && (idxAfter % 16) == 0)
            TORNADO_TRACE("waypoint %d of %d at (%.1f,%.1f)\n",
                          idxAfter, *reinterpret_cast<int*>(s + 0x48),
                          *reinterpret_cast<float*>(s + 0x58),
                          *reinterpret_cast<float*>(s + 0x5c));
    }

    // --- integrate the position ---
    const float secs = reinterpret_cast<const System::Time*>(const_cast<int64_t*>(dt))->AsSeconds();   // B.3.4b bridge
    const float step = secs * *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x20);
    // Math::Vector3::operator+= (slot 0x1401514b8), inlined: a componentwise add
    // with no side effects.
    *reinterpret_cast<float*>(s + 0x58) += unit.x * step;
    *reinterpret_cast<float*>(s + 0x5c) += unit.y * step;
    *reinterpret_cast<float*>(s + 0x60) += unit.z * step;

    if (g_h.terrainClamp) {
        float out[3] = { 0.0f, 0.0f, 0.0f };
        float* r = g_h.terrainClamp(map, out, reinterpret_cast<float*>(s + 0x58), 1, nullptr);
        if (r) {
            *reinterpret_cast<float*>(s + 0x58) = r[0];
            *reinterpret_cast<float*>(s + 0x5c) = r[1];
            *reinterpret_cast<float*>(s + 0x60) = r[2];
        }
    }

    // --- push the transform to the particle geometry ---
    {
        // Transform = { Quaternion(0,0,0,1), Vector3 position }.
        Math::Transform transform;
        transform.rotation = Math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        transform.translation.x = *reinterpret_cast<float*>(s + 0x58);
        transform.translation.y = *reinterpret_cast<float*>(s + 0x5c);
        transform.translation.z = *reinterpret_cast<float*>(s + 0x60);
        void* geometry = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(pc + 0x38));
        reinterpret_cast<Graphics::Geometry*>(geometry)->SetTransform(transform);   // B.3.5 bridge
    }

    // --- move the audio emitters and refresh the volume ---
    {
        int64_t audioIbp = 0;
        if (g_h.findAudio) g_h.findAudio(owner, &audioIbp);
        if (audioIbp && g_h.audioMoveEmitters)
            g_h.audioMoveEmitters(reinterpret_cast<void*>(static_cast<uintptr_t>(audioIbp)),
                                  reinterpret_cast<void*>(s + 0x58));

        const float volume =
            *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(pc + 0x38) + 0x7c);

        int64_t audioIbp2 = 0;
        if (g_h.findAudio) g_h.findAudio(owner, &audioIbp2);
        if (audioIbp2) {
            uintptr_t a = static_cast<uintptr_t>(audioIbp2);
            uint32_t* emitter = *reinterpret_cast<uint32_t**>(a + 0x38);
            if (emitter) {
                if (*emitter == 0) {
                    emitter[1] = *reinterpret_cast<const uint32_t*>(&volume);
                } else if (*reinterpret_cast<float*>(&emitter[1]) != volume) {
                    emitter[1] = *reinterpret_cast<const uint32_t*>(&volume);
                    reinterpret_cast<Audio::Scene*>(*reinterpret_cast<void**>(g + 0x78))->SetEmitterVolume(
                        *emitter, *reinterpret_cast<const Vector3*>(a + 0x28), volume);   // B.3.7 bridge
                }
            }
        }
    }
}

// create 0x14000d500: MemAlloc(0x68, 8), then the ctor.
void* TornadoComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TornadoComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TornadoComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* TornadoComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TornadoComponent());
}

DEFINE_EXE_TYPE(TornadoComponent, "Component")
