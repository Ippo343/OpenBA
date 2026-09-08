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
//   BANISHED_RT_GFX9=0       ... just the Wave 9 lane GFX four (Transform::Lerp,
//                                InterpolatedState<Transform>::Lerp,
//                                MaterialInstance::RenderTextures,
//                                Renderer::AddDrawItem)
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtGraphicsSceneRegister();     // scene.cpp     (lane D)
extern int RtGraphicsMeshRegister();      // mesh.cpp      (lane E)
extern int RtGraphicsRendererRegister();  // renderer.cpp  (lane F)
extern int RtGraphicsAnimationRegister(); // animation.cpp (lane G)
extern int RtGraphicsWave9GfxRegister();  // wave9_lane_gfx.cpp (Wave 9 lane GFX)
extern int RtGraphicsSunLightRegister();  // sunlight.cpp (Wave 9 lane GFX2)

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
    printf("[RtPort] Graphics: %d exports registered\n", n);
}
