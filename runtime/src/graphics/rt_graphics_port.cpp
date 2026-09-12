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
//   BANISHED_RT_GFXBODY=0    ... just the Wave 21 lane GFXBODY three
//                            (MaterialInstance::Setup and the two class
//                            operator news). Its OWN switch because Setup is
//                            the only one of the three that can change a
//                            PIXEL -- it binds textures and material constants
//                            onto the live Video::Context -- so an integration
//                            run that sees the UI or the tile overlay render
//                            wrong can halve the search space with one env var.
//   BANISHED_RT_GFXBODY23=0  ... just the Wave 23 lane GFXBODY five: the four
//                            Graphics::FrameCopyImageBuffer bodies (ctor, dtor,
//                            Create, Generate) and AnimationManager::Update. Its
//                            OWN switch, separate from BANISHED_RT_GFXBODY,
//                            because the two sets have DISJOINT failure modes:
//                            the FrameCopy four can only affect the screenshot /
//                            save-thumbnail path, while AnimationManager::Update
//                            runs every sim iteration and a defect in it freezes
//                            every skinned citizen at bind pose. One env var
//                            each keeps those two separable in an A/B.
//   BANISHED_RT_GFXBIG=0     ... just the Wave 27 lane GFXBIG three:
//                            MaterialInstance::BuildParameters, Renderer::Draw
//                            and AnimationState::Update. THE WIDEST BLAST RADIUS
//                            OF ANY SWITCH IN THIS FILE -- Renderer::Draw is the
//                            entire 3D frame dispatch -- so it gets its own
//                            variable rather than sharing one.
//   BANISHED_RT_GFX9=0       ... just the Wave 9 lane GFX four (Transform::Lerp,
//                                InterpolatedState<Transform>::Lerp,
//                                MaterialInstance::RenderTextures,
//                                Renderer::AddDrawItem)
//   BANISHED_RT_GFXTI=0      ... just the eight ?Type@/?TypeName@/?GetFileVersion@
//                                vtable-slot accessors of AnimationEvent /
//                                MaterialInstance / Particle    (Wave 21 lane GFXVT)
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
extern int RtGraphicsWave21GfxbodyRegister(); // wave21_lane_gfxbody.cpp (Wave 21 lane GFXBODY)
extern int RtGraphicsWave23GfxbodyRegister(); // wave23_lane_gfxbody.cpp (Wave 23 lane GFXBODY)
extern int RtGraphicsWave24GfxmatRegister();  // wave24_lane_gfxmat.cpp (Wave 24 lane GFXMAT)
extern int RtGraphicsWave27GfxbigRegister();  // wave27_lane_gfxbig.cpp (Wave 27 lane GFXBIG)

extern int RtGraphicsWave27GfxpartRegister(); // wave27_lane_gfxpart.cpp (Wave 27 lane GFXPART)
extern "C" int RtGraphicsWave28PartsysRegister(); // wave28_lane_partsys.cpp (Wave 28 lane PARTSYS)
extern int RtGraphicsTypeInfoAccessorsRegister();
                                          // graphics_typeinfo_accessors.cpp (Wave 21 lane GFXVT)
extern int RtGraphicsEdgeMeshVtableRegister();
                                          // edgemesh_vtable.cpp (Wave 27 lane GFXMESH)
extern int RtGraphicsMeshVtableRegister(); // mesh.cpp (Wave 27 lane GFXMESH)

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
    if (RtWaveEnabled("BANISHED_RT_GFXBODY")) n += RtGraphicsWave21GfxbodyRegister();

    // WAVE 23 LANE GFXBODY. Counted into `n` and ALSO printed on its own line
    // below, for the reason the GFXD and GFXVT lines give: a run log must carry
    // a line only THIS build emits, because the on-disk md5 says which exe is on
    // disk, not which one ran, and ten lanes share one game directory.
    int gfxbody23 = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXBODY23")) gfxbody23 = RtGraphicsWave23GfxbodyRegister();
    n += gfxbody23;

    // WAVE 21 LANE GFXVT — the eight ?Type@/?TypeName@/?GetFileVersion@ slot
    // bodies of ??_7AnimationEvent@ / ??_7MaterialInstance@ / ??_7Particle@.
    // Counted into `n` above so the existing identity line carries them; the
    // per-lane count is printed separately below for the same reason lane
    // GFXD's is (a run log must carry a line only THIS build emits).
    int gfxti = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXTI")) gfxti = RtGraphicsTypeInfoAccessorsRegister();
    n += gfxti;

    // WAVE 24 LANE GFXMAT -- the four remaining ??_7MaterialInstance@ slot
    // bodies (5 Serialize, 6 Reconstruct, 7 Destruct, 8 Construct) and the two
    // Graphics ?Serialize@ rows. Its own identity line is printed below for the
    // reason every other lane line here gives: the on-disk md5 says which exe
    // is on disk, not which one ran.
    int gfxmat = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXMAT")) gfxmat = RtGraphicsWave24GfxmatRegister();
    n += gfxmat;

    // WAVE 27 LANE GFXBIG -- the three large Graphics BODY rows
    // (MaterialInstance::BuildParameters, Renderer::Draw,
    // AnimationState::Update) plus the one unexported Array instantiation
    // BuildParameters cannot run without.
    //
    // ITS OWN SWITCH, and this is the switch in this file with the widest blast
    // radius: `Renderer::Draw` IS the 3D frame. With GFXBIG on, every frame's
    // cull, sort, clear and draw dispatch runs through our body; with it off all
    // three go back to the DLL and nothing else in this file changes. So an
    // integration run that renders wrong has a one-variable A/B available before
    // anything is bisected.
    //
    // All three rows are LIVE the moment they are registered -- each already has
    // at least one name door in the tree (see the registration banner in
    // wave27_lane_gfxbig.cpp) -- so unlike lane GFXD's inert rows, a zero
    // attendance count from this lane means something is wrong, not that the
    // rows are unreachable.
    int gfxbig = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXBIG")) gfxbig = RtGraphicsWave27GfxbigRegister();
    n += gfxbig;

    // WAVE 27 LANE GFXMESH -- the eight non-slot-0 bodies of
    // ??_7EdgeMesh@Graphics@@6B@ (slots 1..8). Slot 0 is the unnamed vector
    // deleting destructor and lives in runtime/src/object/deleting_dtor.cpp as
    // a table row, not here. Counted into `n` and printed on its own line for
    // the reason every other lane line here gives: the on-disk md5 says which
    // exe is on disk, not which one ran, and several lanes share one game dir.
    int gfxedgevt = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXEDGEVT"))
        gfxedgevt = RtGraphicsEdgeMeshVtableRegister();
    n += gfxedgevt;

    // WAVE 27 LANE GFXMESH, second half -- four slot bodies on the two INSTANCE
    // tables (EdgeMeshInstance Update + GetInstanceTransform, MeshInstance
    // GetSubTransform + GetInstanceTransform). They live in mesh.cpp because
    // MeshInstance::GetInstanceTransform reuses that file's RtInstancedPoolSlot
    // helper, which carries the MOVSXD/SAR/second-deref corrections
    // WAVE6_LANE_E.md paid for; a second copy is what this avoids.
    int gfxmeshvt = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXMESHVT"))
        gfxmeshvt = RtGraphicsMeshVtableRegister();
    n += gfxmeshvt;
    printf("[RtPort] GFXMESHVT (Wave 27 lane GFXMESH): %d instance vtable slot "
           "bodies registered\n", gfxmeshvt);

    printf("[RtPort] GFXEDGEVT (Wave 27 lane GFXMESH): %d EdgeMesh vtable slot "
           "bodies registered\n", gfxedgevt);

    // WAVE 27 LANE GFXPART -- the particle vtable slot bodies: the last
    // ??_7Particle@Graphics@@6B@ slot (Serialize) and four of
    // ??_7ParticleInstance@'s, three of which are SHARED slots that close
    // ??_7ParticleSystemInstance@ rows at the same time. Its own identity line is
    // printed below for the reason every other lane line here gives: the on-disk
    // md5 says which exe is on disk, not which one ran.
    int gfxpart = 0;
    if (RtWaveEnabled("BANISHED_RT_GFXPART")) gfxpart = RtGraphicsWave27GfxpartRegister();
    n += gfxpart;

    // WAVE 9 LANE GFXD -- the bottom half of the Graphics::Renderer class
    // lane: Renderer::Reset, Renderer::AddScene, two more Graphics::Camera
    // constructors, and the seven unexported Collection::Array
    // instantiations underneath them. Registered but INERT -- no door in
    // the tree resolves any of them today, so this changes nothing in a
    // live run. Its oracle is its own probe, behind BANISHED_RT_GFXD_PROBE.
    // WAVE 28 LANE PARTSYS -- ?Update@ParticleSystemInstance@ (the whole
    // particle simulation, and the LAST unported slot of
    // ??_7ParticleSystemInstance@Graphics@@6B@), ?RemoveState@ParticleInstance@,
    // and three ??_7SunLight@Graphics@@6B@ slots. DEFAULT ON: RtWaveEnabled is a
    // kill switch, so BANISHED_RT_PARTSYS=0 leaves every call on the DLL for an
    // A/B and anything else arms it (kickoff §2.2 caveat 1 -- a lane that ports
    // behind an opt-in switch moves the live count not at all).
    int partsys = 0;
    if (RtWaveEnabled("BANISHED_RT_PARTSYS")) partsys = RtGraphicsWave28PartsysRegister();
    n += partsys;

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
    printf("[RtPort] Wave 21 lane GFXVT: %d typeinfo-accessor exports registered\n", gfxti);
    printf("[RtPort] Wave 27 lane GFXPART: %d particle exports registered\n", gfxpart);
}
