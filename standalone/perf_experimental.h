// standalone/perf_experimental.h
// NON-RETAIL performance knobs ("PerfX"). Everything here does LESS than retail per
// sim tick, chosen so the difference is not visible: purely visual work is deferred to
// the last speed-iteration of a frame, skipped for entities outside the camera frustum,
// or run at a lower rate than the sim. Nothing here touches gameplay state.
//
// Build flag: -DBANISHED_PERF_EXPERIMENTAL=1 (the perf build sets it, see the Makefile's
// PERF_EXPERIMENTAL variable) makes every knob DEFAULT-ON. Without the define the knobs
// default OFF. Runtime overrides, all read once:
//   BANISHED_PERF_EXPERIMENTAL=0|1        master switch
//   BANISHED_PERF_ANIM_CULL=0|1           off-screen MeshInstance::Update stays root-only
//   BANISHED_PERF_GEOMXF_LASTONLY=0|1     move-tick Geometry::SetTransform only on the last
//                                         iteration (the intermediate ones are overwritten)
//   BANISHED_PERF_ICON_LASTONLY=0|1       status-icon repositioning only on the last iteration
//   BANISHED_PERF_AMBIENT_THROTTLE=<n>    biome histogram rescan every n-th tick (0 = off)
//   BANISHED_PERF_TREE_SNAPSHOT=0|1       walk the update tree from a flat in-order snapshot
//   BANISHED_PERF_PARTICLE_CULL=0|1       off-screen particle systems are frozen until seen
//   BANISHED_PERF_PICK_CULL=0|1           off-screen citizens skip the pick-scene transform
//   BANISHED_PERF_GEOM_CULL=0|1           off-screen citizens skip the render-scene transform
//   BANISHED_PERF_ICON_CULL=0|1           off-screen status icons skip repositioning
//   BANISHED_PERF_ANIM_BATCH_CULLED=0|1   off-screen models: the N-1 intermediate root-only
//                                         animation updates are folded into ONE update with
//                                         N x the tick on the last iteration
//   BANISHED_PERF_ANIM_BATCH_ALL=0|1      the same for EVERY model whose per-tick root-motion
//                                         offset cannot differ (instanced animations read the
//                                         AnimationManager pool, which is still ticked every
//                                         iteration; animations without root motion). A model
//                                         mid-blend or on a non-instanced root-motion animation
//                                         keeps its per-tick update, so walk speed is exact
//                                         (the drawn pose is computed once per frame at the exact
//                                         animation time; only animation EVENT timing within a
//                                         frame moves to the last iteration)
//                                         (rebuilt when the tree changes; same visit order)
//   BANISHED_PERF_ANIM_CULL_MARGIN=<f>    world-unit margin around the frustum (default 6)
//   BANISHED_PERF_TRACE=1                 one-line census per 600 frames on stderr
#pragma once
#include <cstdint>

namespace PerfX {

// Resolved once (first BeginFrame) into a plain global so the per-tick queries below are
// a load, not a call + static-init guard. All zero (= every knob off) until then.
struct Flags {
    bool master, animCull, geomXfLastOnly, iconLastOnly, treeSnapshot, particleCull,
         pickCull, geomCull, iconCull, animBatchCulled, animBatchAll, terrainSidecar;
    int  ambientThrottle;
};
extern Flags g_flags;
extern uint8_t* g_rootOnlyPtr;   // MeshInstance::_rootOnlyUpdates (resolved with the flags)

inline bool Enabled()         { return g_flags.master; }   // master switch
inline bool AnimCull()        { return g_flags.animCull; }
inline bool GeomXfLastOnly()  { return g_flags.geomXfLastOnly; }
inline bool IconLastOnly()    { return g_flags.iconLastOnly; }
inline int  AmbientThrottle() { return g_flags.ambientThrottle; }   // 0 = off, else rescan period
inline bool TreeSnapshot()    { return g_flags.treeSnapshot; }      // flat in-order walk snapshot
inline bool ParticleCull()    { return g_flags.particleCull; }      // off-screen particles frozen
inline bool PickCull()        { return g_flags.pickCull; }          // off-screen pick transform skipped
inline bool GeomCull()        { return g_flags.geomCull; }          // off-screen render transform skipped
inline bool IconCull()        { return g_flags.iconCull; }          // off-screen icons not repositioned
inline bool AnimBatchCulled() { return g_flags.animBatchCulled; }   // off-screen: one root-only update per frame
inline bool AnimBatchAll()    { return g_flags.animBatchAll; }      // every model: one root-only update per frame
inline bool TerrainSidecar()  { return g_flags.terrainSidecar; }    // 1-byte terrain-type mirror for the move tick (perf_terrain.h)

// GameUpdateLoop::Update start: refresh the per-frame world frustum from the renderer's
// cull camera and flush any deferred visual state when the sim did not run last frame
// (pause), so a paused pan never shows a stale pose.
void BeginFrame(void* renderer);
// Draw side: the terrain collect pass hands over the world-space Volume::Frustum it
// computed (6 planes {nx,ny,nz,d}); the cull camera reference is only valid inside Draw.
void SetWorldFrustum(const void* frustum);
// World teardown: forget every cached pointer.
void Reset();

// Current MeshInstance::_rootOnlyUpdates value (0 on the last iteration / speed 1).
inline bool RootOnlyNow() { return g_rootOnlyPtr && *g_rootOnlyPtr != 0; }
uint8_t* RootOnlyFlag();

// Conservative visibility test of a world position against the cached frustum
// (returns true when no frustum is available).
bool PointVisible(const float* pos);

// Sim loop: which speed iteration is running (0-based) out of how many this frame.
void SetIteration(int index, int count);
// ModelComponent tick, ANIM_BATCH: returns true when the update must be SKIPPED this
// iteration; otherwise *timeOut is the Time to pass (the tick, or (skipped+1) x the tick
// on the last iteration when intermediate ones were skipped for this model). A model whose
// root-motion offset the move tick samples per tick (blend, non-instanced root motion) is
// never skipped, so walk speed matches the unbatched sim exactly.
bool AnimBatchDecide(void* modelComponent, const void* time, int64_t* timeOut);
// ModelComponent tick: decide whether this model's animation pose update may stay
// root-only this iteration. Records the model for a later flush.
bool AnimCullSkip(void* modelComponent);

// Picking: an off-screen citizen skips its Picking::Scene::SetTransform; the deferred
// push is replayed on pause so a click while paused still hits the right mesh.
typedef void (*PickXfCtor_t)(void* xformOut, void* transformIn);
typedef void (*PickSetXf_t)(void* scene, uint32_t id, void* xform);
bool PickCullSkip(void* modelComponent, void* pickScene, uint32_t pickId,
                  PickXfCtor_t xfCtor, PickSetXf_t setXf);
// True when the model's position (model+0x158) is outside the frustum margin and the
// anim-cull machinery is active (so a later flush will repair anything deferred).
bool ModelCulled(void* modelComponent);
// Generic deferral: `replay(obj)` is called with the root-only flag clear when the sim
// goes idle (pause), so deferred presentation state catches up before a paused pan.
void NoteDeferred(void* obj, void (*replay)(void*));
// Particles: off-screen particle components are not ticked (frozen); returns true to skip.
bool ParticleCullSkip(const float* pos);

// Sim-thread census counters (BANISHED_PERF_TRACE).
void Count(int what);
enum { kCntModelVisible, kCntModelCulled, kCntGeomXfSkipped, kCntIconSkipped, kCntAmbientSkipped, kCntParticleSkipped, kCntPickSkipped, kCntGeomCulled, kCntIconCulled, kCntMax };

} // namespace PerfX
