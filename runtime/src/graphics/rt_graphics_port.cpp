// ============================================================================
// runtime/src/graphics/rt_graphics_port.cpp — RtGraphicsPortInit()  Wave 6
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_GRAPHICS=0   leave the whole Graphics namespace on the DLL
//   BANISHED_RT_GFXSCENE=0   ... just Graphics::Geometry + Scene       (lane D)
//   BANISHED_RT_GFXMESH=0    ... just MeshInstance/GraphicsMesh/EdgeMesh (lane E)
//   BANISHED_RT_GFXRENDER=0  ... just Camera/Renderer/SunLight/Material* (lane F)
//   BANISHED_RT_GFXANIM=0    ... just Animation*/Particle*/FrameCopy     (lane G)
//   BANISHED_RT_GFXSUN=0     ... just Graphics::SunLight ctor/Set/dtor  (lane GFX2)
//   BANISHED_RT_GFXGRID=0    ... just Graphics::Scene's quadtree grid   (lane GFX3):
//                            the five by-address cutover doors scene.cpp reaches
//                            plus GridThread. Its OWN switch on purpose — it is
//                            the part of this namespace that ALLOCATES and
//                            re-threads a live grid, so an integration run that
//                            sees a new failure can halve the search space with
//                            one env var. With it off, scene.cpp's five
//                            forwarders go back to the DLL exactly as before.
//   BANISHED_RT_GFX9=0       ... just the Wave 9 lane GFX four (Transform::Lerp,
//                                InterpolatedState<Transform>::Lerp,
//                                MaterialInstance::RenderTextures,
//                                Renderer::AddDrawItem)
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "scene_grid.h"

extern int RtGraphicsSceneRegister();     // scene.cpp     (lane D)
extern int RtGraphicsMeshRegister();      // mesh.cpp      (lane E)
extern int RtGraphicsRendererRegister();  // renderer.cpp  (lane F)
extern int RtGraphicsAnimationRegister(); // animation.cpp (lane G)
extern int RtGraphicsWave9GfxRegister();  // wave9_lane_gfx.cpp (Wave 9 lane GFX)
extern int RtGraphicsSunLightRegister();  // sunlight.cpp (Wave 9 lane GFX2)
extern int RtGraphicsGridRegister();      // scene_grid.cpp (Wave 9 lane GFX3)
extern int RtGraphicsWave9GfxdRegister(); // wave9_lane_gfxd.cpp (Wave 9 lane GFXD)

void RtGraphicsPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_GRAPHICS")) {
        printf("[RtPort] BANISHED_RT_GRAPHICS=0 -- Graphics namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXSCENE"))  n += RtGraphicsSceneRegister();
    if (RtWaveEnabled("BANISHED_RT_GFXMESH"))   n += RtGraphicsMeshRegister();
    if (RtWaveEnabled("BANISHED_RT_GFXRENDER")) n += RtGraphicsRendererRegister();
    if (RtWaveEnabled("BANISHED_RT_GFXANIM"))   n += RtGraphicsAnimationRegister();
    if (RtWaveEnabled("BANISHED_RT_GFX9"))      n += RtGraphicsWave9GfxRegister();
    if (RtWaveEnabled("BANISHED_RT_GFXSUN"))   n += RtGraphicsSunLightRegister();

    // WAVE 9 LANE GFXD -- the bottom half of the Graphics::Renderer class
    // lane: Renderer::Reset, Renderer::AddScene, two more Graphics::Camera
    // constructors, and the seven unexported Collection::Array
    // instantiations underneath them. Registered but INERT -- no door in
    // the tree resolves any of them today, so this changes nothing in a
    // live run. Its oracle is its own probe, behind BANISHED_RT_GFXD_PROBE.
    int gfxd = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXD")) gfxd = RtGraphicsWave9GfxdRegister();

    int grid = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXGRID")) {
        grid = RtGraphicsGridRegister();
        // ARM the probe rather than running it. This init runs right after
        // ResolveInit()'s LoadLibrary, where the DLL's MemoryAllocator is
        // CONSTRUCTED but not STARTED; a probe that allocates there takes the
        // allocator's critical section before the allocator exists and faults
        // on null+0x640 (WAVE9.md, lane PKG2). See scene_grid.h.
        rtgfxgrid::g_probePending = true;
    }
    // The grid count is printed SEPARATELY and names this lane, so a run log
    // carries an identity line only this build emits — the acceptance test that
    // works when six lanes share one game directory (the on-disk md5 says which
    // exe is on disk, not which one ran).
    printf("[RtPort] Graphics: %d exports + %d grid addresses registered\n", n, grid);
    // Lane GFXD's own identity line. A run log has to carry a line only
    // THIS build emits: the on-disk md5 says which exe is on disk, not
    // which one ran, and six lanes share one game directory.
    printf("[RtPort] Wave 9 lane GFXD: %d exports + 11 addresses registered\n", gfxd);
}
