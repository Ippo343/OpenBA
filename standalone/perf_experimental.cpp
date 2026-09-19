// standalone/perf_experimental.cpp — see perf_experimental.h.
#include "perf_experimental.h"
// ENDGAME B.3.5: Graphics::Camera / Renderer / Geometry, ahead of the
// <windows.h> that "resolve.h" pulls in — <wingdi.h> declares a FUNCTION named
// Rectangle, which breaks Volume.h's Volume::Rectangle (the <windows.h> trap).
#include "graphics/Camera.h"
#include "graphics/Geometry.h"
#include "perf_terrain.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "options.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>       // placement new — the retslot door body at the call site
#include <vector>
#include "math/Math.h"       // -Iruntime/types

namespace PerfX {

Flags g_flags = {};
uint8_t* g_rootOnlyPtr = nullptr;

namespace {

struct Config {
    bool  master;
    bool  animCull;
    bool  geomXfLastOnly;
    bool  iconLastOnly;
    int   ambientThrottle;
    bool  treeSnapshot;
    bool  particleCull;
    bool  pickCull;
    bool  geomCull;
    bool  iconCull;
    bool  animBatchCulled;
    bool  animBatchAll;
    bool  terrainSidecar;
    float cullMargin;
    bool  trace;
};
const Config& Cfg() {
    static Config c = [] {
        Config k;
        const int def = Opt().perfBuildDefault;
        k.master          = Opt().perfExperimental;
        k.animCull        = Opt().perfAnimCull;
        k.geomXfLastOnly  = Opt().perfGeomXfLastOnly;
        k.iconLastOnly    = Opt().perfIconLastOnly;
        k.ambientThrottle = Opt().perfAmbientThrottle;
        k.treeSnapshot    = Opt().perfTreeSnapshot;
        k.particleCull    = Opt().perfParticleCull;
        k.pickCull        = Opt().perfPickCull;
        k.geomCull        = Opt().perfGeomCull;
        k.iconCull        = Opt().perfIconCull;
        k.animBatchCulled = Opt().perfAnimBatchCulled;
        k.animBatchAll    = Opt().perfAnimBatchAll;
        k.terrainSidecar  = Opt().perfTerrainSidecar;
        k.cullMargin      = Opt().perfCullMargin;
        k.trace           = Opt().perfTrace;
        g_flags.master = k.master; g_flags.animCull = k.animCull;
        g_flags.geomXfLastOnly = k.geomXfLastOnly; g_flags.iconLastOnly = k.iconLastOnly;
        g_flags.treeSnapshot = k.treeSnapshot; g_flags.particleCull = k.particleCull;
        g_flags.pickCull = k.pickCull; g_flags.geomCull = k.geomCull; g_flags.iconCull = k.iconCull;
        g_flags.ambientThrottle = k.ambientThrottle;
        g_flags.animBatchCulled = k.animBatchCulled; g_flags.animBatchAll = k.animBatchAll;
        g_flags.terrainSidecar = k.terrainSidecar;
        std::fprintf(stderr, "[PerfX] master=%d animCull=%d geomXfLastOnly=%d iconLastOnly=%d "
                     "ambientThrottle=%d treeSnapshot=%d particleCull=%d pickCull=%d geomCull=%d iconCull=%d animBatchCulled=%d animBatchAll=%d terrainSidecar=%d margin=%.1f (build default %d)\n",
                     (int)k.master, (int)k.animCull, (int)k.geomXfLastOnly, (int)k.iconLastOnly,
                     k.ambientThrottle, (int)k.treeSnapshot, (int)k.particleCull, (int)k.pickCull,
                     (int)k.geomCull, (int)k.iconCull, (int)k.animBatchCulled, (int)k.animBatchAll,
                     (int)k.terrainSidecar, k.cullMargin, def);
        return k;
    }();
    return c;
}

// ---- per-frame frustum -------------------------------------------------------
// Volume::Frustum = 6 planes of {nx,ny,nz,d} at +0x00..+0x60 (Frustum::Contains,
// Runtime 0x1800f8e80: inside side is n.p - d > 0), plus one trailing dword; 0x64 bytes.
// Camera::GetFrustum copies 0x64 bytes; MulTransform3 with the camera-to-world Transform3
// at camera+0x64 (48 bytes) gives world-space planes — the same math as
// GeomDraw_CollectDraw (code/Game/Game/geometry_draw.cpp).
float    s_planes[6][4];
bool     s_haveFrustum = false;
unsigned s_frameSerial = 0;
bool     s_simRanThisFrame = false;
uint8_t* s_rootOnly = nullptr;
bool     s_rootOnlyResolved = false;
long     s_counts[kCntMax];
std::vector<void*> s_culledModels;   // models whose pose was left root-only this frame
struct DeferredPick { void* model; void* scene; uint32_t id; PickXfCtor_t ctor; PickSetXf_t set; };
std::vector<DeferredPick> s_deferredPicks;   // citizens whose pick transform was not pushed
struct DeferredReplay { void* obj; void (*fn)(void*); };
std::vector<DeferredReplay> s_deferredReplays;  // icons etc. whose presentation tick was skipped
bool s_flushing = false;   // replays re-enter the tick bodies; no re-deferral while flushing
int  s_iterIndex = 0, s_iterCount = 1;   // SetIteration
long s_cntBatchSkipped = 0;

void ResolveRootOnly() {
    if (s_rootOnlyResolved) return;
    s_rootOnlyResolved = true;
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140151f30);
    s_rootOnly = slot ? *reinterpret_cast<uint8_t**>(slot) : nullptr;
    if (!s_rootOnly)
        s_rootOnly = reinterpret_cast<uint8_t*>(
            ResolveExport("?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA"));
    g_rootOnlyPtr = s_rootOnly;
}

void RefreshFrustum(void* renderer) {
    s_haveFrustum = false;
    if (!renderer || !fn_Frustum_MulTransform3) return;
    // Renderer::GetCullCamera returns a reference into the camera object; before the first
    // Draw the pointer it adjusts is null, so the "reference" is a small negative address
    // (-0x110 seen live). Anything outside user space is "no camera yet". Taking the
    // address of the returned reference does not dereference it, so the range test below
    // still runs before any load.
    const ::Graphics::Camera* camera =
        &reinterpret_cast<::Graphics::Renderer*>(renderer)->GetCullCamera();  // B.3.5 bridge
    if (reinterpret_cast<intptr_t>(camera) <= 0x10000 ||
        reinterpret_cast<uintptr_t>(camera) >= 0x00007fffffff0000ull) return;
    alignas(16) uint8_t camFrustum[256] = {0};
    alignas(16) uint8_t worldFrustum[256] = {0};
    // B.3.5: the retslot door's body moved to the call site — exactly one
    // construction, into the SAME caller storage the door used to be handed.
    ::new (static_cast<void*>(camFrustum)) ::Frustum(camera->GetFrustum());
    // Graphics::Camera keeps its world transform at +0x64; the copy is
    // Math::Transform3's copy ctor, now through the real class's named field.
    static_assert(offsetof(::Graphics::Camera, _cameraToWorld) == 0x64,
                  "Camera::_cameraToWorld @ +0x64 — the offset this read used to spell out");
    Math::Transform3 camXf3 = camera->_cameraToWorld;
    fn_Frustum_MulTransform3(camFrustum, worldFrustum, &camXf3);
    std::memcpy(s_planes, worldFrustum, sizeof(s_planes));
    // Sanity: a zero frustum (renderer not yet drawn) must not cull everything.
    float mag = 0.0f;
    for (int i = 0; i < 6; ++i)
        mag += s_planes[i][0] * s_planes[i][0] + s_planes[i][1] * s_planes[i][1] +
               s_planes[i][2] * s_planes[i][2];
    s_haveFrustum = mag > 1e-6f;
}

// Bring every culled model's pose up to date: MeshInstance::Update with a ZERO time and
// the root-only flag clear. AnimationState::Update(dt=0) recomputes the node pose at the
// current animation time and snaps the interpolated state (prev = cur), so nothing moves.
void FlushCulled() {
    if (s_culledModels.empty()) return;
    ResolveRootOnly();
    uint8_t saved = s_rootOnly ? *s_rootOnly : 0;
    if (s_rootOnly) *s_rootOnly = 0;
    int64_t zeroTime = 0;
    for (void* model : s_culledModels) {
        uintptr_t s = reinterpret_cast<uintptr_t>(model);
        // GEOM_CULL left the render-scene transform stale: push model+0x148 first.
        // The receiver is the Geometry the vtable dispatch below also drives: every
        // drawable component in this port embeds its Geometry at owner+0x28
        // (scene_watch.cpp:176-178 and engine/types/ModelComponent.h).
        if (Cfg().geomCull)
            reinterpret_cast<::Graphics::Geometry*>(s + 0x28)->SetTransform(
                *reinterpret_cast<const Math::Transform*>(s + 0x148));  // B.3.5 bridge
        // ENDGAME B.3.5 caller batch, COORDINATOR RULING. This was a hand-inlined
        // read of slot 1 (`vt + 0x8`) on the same proved `Graphics::Geometry` at
        // owner+0x28, guarded by `if (upd)` — a test of the OBJECT'S OWN VTABLE
        // SLOT, not of a door pointer. Slot 1 is `_purecall` only on the abstract
        // bases, and every live Geometry in this process wears a complete minted
        // table (`[RtMint] 152 of 152 vtables COMPLETE`), so that guard tested
        // nothing that can happen and it goes with the inlined dispatch. It is now
        // the virtual call it always was. The OBJECT-pointer reasoning above — the
        // receiver is proved from the code that produced it, not from its name —
        // is unchanged, and `Cfg().geomCull` still guards the SetTransform above.
        reinterpret_cast<::Graphics::Geometry*>(s + 0x28)                // B.3.5 bridge
            ->Update(*reinterpret_cast<const System::Time*>(&zeroTime));
    }
    if (Cfg().trace)
        std::fprintf(stderr, "[PerfX] flushed %zu culled poses (sim idle)\n", s_culledModels.size());
    s_culledModels.clear();
    if (s_rootOnly) *s_rootOnly = saved;
}

void FlushReplays() {
    if (s_deferredReplays.empty()) return;
    ResolveRootOnly();
    uint8_t saved = s_rootOnly ? *s_rootOnly : 0;
    if (s_rootOnly) *s_rootOnly = 0;
    std::vector<DeferredReplay> replays;
    replays.swap(s_deferredReplays);
    s_flushing = true;
    for (const DeferredReplay& d : replays) d.fn(d.obj);
    s_flushing = false;
    if (Cfg().trace)
        std::fprintf(stderr, "[PerfX] replayed %zu deferred presentation ticks (sim idle)\n", replays.size());
    if (s_rootOnly) *s_rootOnly = saved;
}

void FlushPicks() {
    if (s_deferredPicks.empty()) return;
    for (const DeferredPick& d : s_deferredPicks) {
        alignas(16) uint8_t xf[136];
        d.ctor(xf, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(d.model) + 0x148));
        d.set(d.scene, d.id, xf);
    }
    if (Cfg().trace)
        std::fprintf(stderr, "[PerfX] flushed %zu deferred pick transforms (sim idle)\n", s_deferredPicks.size());
    s_deferredPicks.clear();
}

// ---- ANIM_BATCH per-model bookkeeping ------------------------------------------------
// Which models may skip their intermediate root-only updates is decided per model per
// iteration from the instance AnimationState (MeshInstance+0xd8):
//   layers at +0x08 (stride 0x38: group +0x00, animation index +0x08, pool id +0x0c,
//   time +0x18, blend total +0x20 / remaining +0x28), count at +0x10.
//   Animation = *(group+0x20) + index*0x70: node count +0x4c, frame count +0x50,
//   keyframes +0x60 (0x1c bytes each: quaternion, then position at +0x10).
// The move tick's walk speed is |MeshInstance::RootAnimationOffset()| per tick.
//  * count == 1 and the layer has a pool id: AnimationState::Update flags the state
//    "instanced" and RootAnimationOffset reads the AnimationManager pool pose, which
//    AnimationManager::Update refreshes with the real tick on EVERY iteration. The
//    instance update only advances the clock: batching it is exact for walk speed.
//  * otherwise (a blend between layers, or a non-instanced animation) the offset is the
//    instance's own +0x100, written by UpdateRootOnly from the layer times and the
//    per-tick blend weight, and Animation::GetRootPoseAtTime scales it by the dt it is
//    given. Holding it for a frame under-samples the blend ramp (the -2% mean step) and
//    an N x dt call would make a non-instanced walk N x too fast. Such a model gets the
//    plain per-tick update as long as any of its layers' animations carries root motion
//    (any non-zero node-0 keyframe position, cached per Animation); with no root motion
//    every dt yields the same zero offset and batching is exact again.
// A model is re-decided every iteration, so a PlayAnimation between iterations switches
// it to per-tick from that iteration on; the last iteration then advances it by the
// ticks it actually skipped (+1), not a blind N.
struct BatchSlot { const void* key; unsigned serial; unsigned skipped; };
BatchSlot s_batchTbl[32768];
struct RootMotionSlot { const void* key; bool has; };
RootMotionSlot s_rootMotionTbl[4096];
unsigned s_batchSerial = 0;
long s_cntBatchPerTick = 0;

BatchSlot* BatchEntry(const void* model) {
    unsigned h = (unsigned)(((reinterpret_cast<uintptr_t>(model) >> 4) * 2654435761u) & 32767);
    for (int probe = 0; probe < 64; ++probe) {
        BatchSlot& e = s_batchTbl[h];
        if (e.serial == s_batchSerial && e.key == model) return &e;
        if (e.serial != s_batchSerial) { e.key = model; e.serial = s_batchSerial; e.skipped = 0; return &e; }
        h = (h + 1) & 32767;   // linear probe: a live entry of another model
    }
    return nullptr;   // table saturated (never at sane model counts): caller does not batch
}

bool AnimationHasRootMotion(const uint8_t* anim) {
    RootMotionSlot& e = s_rootMotionTbl[(unsigned)(((reinterpret_cast<uintptr_t>(anim) >> 4) * 2654435761u) & 4095)];
    if (e.key == anim) return e.has;
    const int nodes = *reinterpret_cast<const int*>(anim + 0x4c);
    const int frames = *reinterpret_cast<const int*>(anim + 0x50);
    const uint8_t* keys = *reinterpret_cast<const uint8_t* const*>(anim + 0x60);
    bool has = false;
    if (keys && nodes > 0) {
        for (int f = 0; f < frames && !has; ++f) {
            const float* pos = reinterpret_cast<const float*>(keys + (size_t)nodes * f * 0x1c + 0x10);
            has = pos[0] != 0.0f || pos[1] != 0.0f || pos[2] != 0.0f;
        }
    }
    e.key = anim; e.has = has;
    return has;
}

// True when this model's root-motion offset would differ between one update of N ticks
// and N updates of one tick (see above).
bool NeedsPerTickRootUpdate(const void* model) {
    const uint8_t* state = reinterpret_cast<const uint8_t*>(model) + 0x28 + 0xd8;
    const int count = *reinterpret_cast<const int*>(state + 0x10);
    if (count <= 0) return false;                                   // nothing playing
    const uint8_t* layers = *reinterpret_cast<const uint8_t* const*>(state + 0x08);
    if (!layers) return false;
    // Instanced: RootAnimationOffset reads the pool pose -- but only once the state's
    // "instanced" byte (+0x00) is set, which the next Update does. Right after a blend
    // finished the count is 1 with the byte still clear, and the offset would be the
    // last blended +0x100 held for the frame: give that model one more per-tick update.
    if (count == 1 && *reinterpret_cast<const int*>(layers + 0x0c) >= 0) return state[0] == 0;
    for (int i = 0; i < count; ++i) {
        const uint8_t* layer = layers + (size_t)i * 0x38;
        const uint8_t* group = *reinterpret_cast<const uint8_t* const*>(layer);
        const int idx = *reinterpret_cast<const int*>(layer + 0x08);
        if (!group || idx < 0) continue;
        const uint8_t* anims = *reinterpret_cast<const uint8_t* const*>(group + 0x20);
        if (!anims) continue;
        if (AnimationHasRootMotion(anims + (size_t)idx * 0x70)) return true;
    }
    return false;
}

} // namespace


uint8_t* RootOnlyFlag() { ResolveRootOnly(); return s_rootOnly; }

void BeginFrame(void* renderer) {
    if (!Cfg().master) return;
    ResolveRootOnly();
    // The sim did not run last frame (pause / speed 0) but poses were left stale before
    // that: bring them up to date once so a paused camera pan shows the real pose.
    if (!s_simRanThisFrame) { FlushCulled(); FlushPicks(); FlushReplays(); }
    s_simRanThisFrame = false;
    ++s_frameSerial;
    if (Cfg().animCull && !s_haveFrustum) RefreshFrustum(renderer);   // fallback; Draw normally feeds SetWorldFrustum
    if (Cfg().trace && (s_frameSerial % 600) == 0) {
        std::fprintf(stderr, "[PerfX] frame %u frustum=%d modelVisible=%ld modelCulled=%ld "
                     "geomXfSkipped=%ld iconSkipped=%ld ambientSkipped=%ld particleSkipped=%ld pickSkipped=%ld geomCulled=%ld iconCulled=%ld\n",
                     s_frameSerial, (int)s_haveFrustum, s_counts[kCntModelVisible],
                     s_counts[kCntModelCulled], s_counts[kCntGeomXfSkipped],
                     s_counts[kCntIconSkipped], s_counts[kCntAmbientSkipped],
                     s_counts[kCntParticleSkipped], s_counts[kCntPickSkipped],
                     s_counts[kCntGeomCulled], s_counts[kCntIconCulled]);
        std::fprintf(stderr, "[PerfX] animBatchSkipped=%ld animBatchPerTick=%ld\n", s_cntBatchSkipped, s_cntBatchPerTick);
        s_cntBatchSkipped = 0; s_cntBatchPerTick = 0;
        std::memset(s_counts, 0, sizeof(s_counts));
    }
}

void SetWorldFrustum(const void* frustum) {
    if (!Cfg().animCull || !frustum) return;
    std::memcpy(s_planes, frustum, sizeof(s_planes));
    float mag = 0.0f;
    for (int i = 0; i < 6; ++i)
        mag += s_planes[i][0] * s_planes[i][0] + s_planes[i][1] * s_planes[i][1] +
               s_planes[i][2] * s_planes[i][2];
    s_haveFrustum = mag > 1e-6f;
}

void Reset() {
    TerrainReport();
    TerrainInvalidate();   // the grid is about to be freed
    s_culledModels.clear();
    s_deferredPicks.clear();
    s_deferredReplays.clear();
    s_haveFrustum = false;
    s_simRanThisFrame = false;
}

static inline bool PlanesVisible(const float* p, float m) {
    for (int i = 0; i < 6; ++i) {
        const float* n = s_planes[i];
        if (n[0] * p[0] + n[1] * p[1] + n[2] * p[2] - n[3] < -m) return false;
    }
    return true;
}

// The same object is tested several times per frame (every speed iteration for
// particles; move tick + model tick + icon for a citizen). The frustum is fixed for the
// frame, so the answer is cached per (object, frame) in a direct-mapped table; a
// collision just recomputes. Keyed on the position's address, which is stable for the
// life of the component.
struct VisSlot { const void* key; unsigned serial; bool vis; };
static VisSlot s_visTbl[8192];

static bool CachedVisible(const float* p, float m) {
    if (!s_haveFrustum) return true;
    VisSlot& e = s_visTbl[((reinterpret_cast<uintptr_t>(p) >> 4) * 2654435761u) & 8191];
    if (e.key == p && e.serial == s_frameSerial) return e.vis;
    const bool v = PlanesVisible(p, m);
    e.key = p; e.serial = s_frameSerial; e.vis = v;
    return v;
}

bool PointVisible(const float* p) {
    return CachedVisible(p, Cfg().cullMargin);
}

bool AnimCullSkip(void* model) {
    if (!Cfg().animCull) return false;
    if (!s_simRanThisFrame) {
        // First model tick of this frame: the previous frame's skip list is superseded
        // (every culled model is re-decided this frame; a model that became visible gets
        // its full update from the normal path).
        s_simRanThisFrame = true;
        s_culledModels.clear();
        s_deferredPicks.clear();
        s_deferredReplays.clear();
    }
    ResolveRootOnly();
    if (!s_rootOnly || *s_rootOnly != 0) return false;   // intermediate iteration: already root-only
    const float* pos = reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(model) + 0x158);
    if (PointVisible(pos)) { ++s_counts[kCntModelVisible]; return false; }
    ++s_counts[kCntModelCulled];
    s_culledModels.push_back(model);
    return true;
}

void Count(int what) { if (what >= 0 && what < kCntMax) ++s_counts[what]; }

bool PickCullSkip(void* model, void* scene, uint32_t id, PickXfCtor_t ctor, PickSetXf_t set) {
    if (!Cfg().pickCull || !s_haveFrustum || !ctor || !set) return false;
    if (!s_simRanThisFrame) { s_simRanThisFrame = true; s_culledModels.clear(); s_deferredPicks.clear(); s_deferredReplays.clear(); }
    const float* pos = reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(model) + 0x158);
    if (PointVisible(pos)) return false;
    ++s_counts[kCntPickSkipped];
    s_deferredPicks.push_back(DeferredPick{ model, scene, id, ctor, set });
    return true;
}

void SetIteration(int index, int count) {
    s_iterIndex = index; s_iterCount = count > 0 ? count : 1;
    if (index == 0) ++s_batchSerial;   // new sim frame: every per-model skip count is stale
}

bool AnimBatchDecide(void* model, const void* time, int64_t* timeOut) {
    const int64_t tick = *reinterpret_cast<const int64_t*>(time);
    *timeOut = tick;
    if (s_iterCount <= 1) return false;                       // speed 1: nothing to batch
    const bool last = (s_iterIndex >= s_iterCount - 1);
    bool batch = g_flags.animBatchAll;
    if (!batch && g_flags.animBatchCulled) {
        // The per-frame visibility cache makes this answer identical on every iteration
        // of the frame, so a model skipped on the intermediate ones is exactly the model
        // that gets the N x tick here.
        batch = s_haveFrustum && !s_flushing &&
                !PointVisible(reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(model) + 0x158));
    }
    if (!batch) return false;
    BatchSlot* e = BatchEntry(model);
    if (!e) return false;
    if (!last) {
        if (NeedsPerTickRootUpdate(model)) { ++s_cntBatchPerTick; return false; }   // exact 1 x tick
        ++e->skipped; ++s_cntBatchSkipped;
        return true;
    }
    *timeOut = tick * (int64_t)(e->skipped + 1);   // exactly the ticks this model skipped
    return false;
}

bool ModelCulled(void* model) {
    if (s_flushing) return false;
    if (!Cfg().animCull || !s_haveFrustum || !model) return false;
    return !PointVisible(reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(model) + 0x158));
}

void NoteDeferred(void* obj, void (*replay)(void*)) {
    if (s_flushing) return;
    if (!s_simRanThisFrame) { s_simRanThisFrame = true; s_culledModels.clear(); s_deferredPicks.clear(); s_deferredReplays.clear(); }
    s_deferredReplays.push_back(DeferredReplay{ obj, replay });
}

bool ParticleCullSkip(const float* pos) {
    if (!Cfg().particleCull || !s_haveFrustum) return false;
    // Smoke rises and drifts: widen the margin so a plume whose emitter is just outside
    // the frame keeps simulating.
    if (CachedVisible(pos, Cfg().cullMargin + 10.0f)) return false;
    ++s_counts[kCntParticleSkipped];
    return true;
}

} // namespace PerfX
