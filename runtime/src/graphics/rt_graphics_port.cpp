// ============================================================================
// runtime/src/graphics/rt_graphics_port.cpp — RtGraphicsPortInit()  Wave 6
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   RT_GRAPHICS=0   leave the whole Graphics namespace on the DLL
//   RT_GFXSCENE=0   ... just Graphics::Geometry + Scene       (lane D)
//   RT_GFXMESH=0    ... just MeshInstance/GraphicsMesh/EdgeMesh (lane E)
//   RT_GFXRENDER=0  ... just Camera/Renderer/SunLight/Material* (lane F)
//   RT_GFXANIM=0    ... just Animation*/Particle*/FrameCopy     (lane G)
//   RT_GFXSUN=0     ... just Graphics::SunLight ctor/Set/dtor  (lane GFX2)
//   RT_GFXGRID=0    ... just Graphics::Scene's quadtree grid   (lane GFX3):
//                            the five by-address cutover doors scene.cpp reaches
//                            plus GridThread. Its OWN switch on purpose — it is
//                            the part of this namespace that ALLOCATES and
//                            re-threads a live grid, so an integration run that
//                            sees a new failure can halve the search space with
//                            one env var. With it off, scene.cpp's five
//                            forwarders go back to the DLL exactly as before.
//   RT_GFXBODY=0    ... just the Wave 21 lane GFXBODY three
//                            (MaterialInstance::Setup and the two class
//                            operator news). Its OWN switch because Setup is
//                            the only one of the three that can change a
//                            PIXEL -- it binds textures and material constants
//                            onto the live Video::Context -- so an integration
//                            run that sees the UI or the tile overlay render
//                            wrong can halve the search space with one env var.
//   RT_GFXBODY23=0  ... just the Wave 23 lane GFXBODY five: the four
//                            Graphics::FrameCopyImageBuffer bodies (ctor, dtor,
//                            Create, Generate) and AnimationManager::Update. Its
//                            OWN switch, separate from RT_GFXBODY,
//                            because the two sets have DISJOINT failure modes:
//                            the FrameCopy four can only affect the screenshot /
//                            save-thumbnail path, while AnimationManager::Update
//                            runs every sim iteration and a defect in it freezes
//                            every skinned citizen at bind pose. One env var
//                            each keeps those two separable in an A/B.
//   RT_GFXBIG=0     ... just the Wave 27 lane GFXBIG three:
//                            MaterialInstance::BuildParameters, Renderer::Draw
//                            and AnimationState::Update. THE WIDEST BLAST RADIUS
//                            OF ANY SWITCH IN THIS FILE -- Renderer::Draw is the
//                            entire 3D frame dispatch -- so it gets its own
//                            variable rather than sharing one.
//   RT_GFX9=0       ... just the Wave 9 lane GFX four (Transform::Lerp,
//                                InterpolatedState<Transform>::Lerp,
//                                MaterialInstance::RenderTextures,
//                                Renderer::AddDrawItem)
//   RT_GFXTI=0      ... just the eight ?Type@/?TypeName@/?GetFileVersion@
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
extern int RtGraphicsWave28GfxbodyRegister(); // wave28_lane_gfxbody.cpp (Wave 28 lane GFXBODY)
extern int RtGraphicsWave29GfxbodyRegister(); // wave29_lane_gfxbody.cpp (Wave 29 lane GFXBODY)
// PARTSYS defines this with C linkage (wave28_lane_partsys.cpp:1076); GFXBODY's
// above is C++-linkage. The spellings differ on purpose -- each matches its own
// definition, and swapping either one to match the other is a link error.
extern "C" int RtGraphicsWave28PartsysRegister(); // wave28_lane_partsys.cpp (Wave 28 lane PARTSYS)
extern "C" int RtGraphicsWave29SlotsRegister();   // wave29_lane_slots.cpp (Wave 29 lane SLOTS)
extern "C" void RtGraphicsWave29SlotsSummary();  // ditto -- WIRED below, kickoff warning 10
extern int RtGraphicsTypeInfoAccessorsRegister();
                                          // graphics_typeinfo_accessors.cpp (Wave 21 lane GFXVT)
extern int RtGraphicsEdgeMeshVtableRegister();
                                          // edgemesh_vtable.cpp (Wave 27 lane GFXMESH)
extern int RtGraphicsMeshVtableRegister(); // mesh.cpp (Wave 27 lane GFXMESH)

extern int RtGraphicsMaterialRegister();   // material.cpp

// BATCH C registration entry point (batchc_*.cpp).
extern "C" int RtGraphicsBatchCLeavesRegister();

void RtGraphicsPortInit() {
    // BATCH C — the ten named Graphics leaves of batchc_gfx_leaves.cpp.
    // Registered BEFORE the namespace kill switch and outside every per-lane
    // switch, because DIRECTION.md §2.3 says a port registers unconditionally
    // and this batch adds no environment switch of its own.
    {
        printf("[RtPort] BATCH C: %d graphics leaves registered\n",
               RtGraphicsBatchCLeavesRegister());
    }
    int n = 0;
    n += RtGraphicsSceneRegister();
    n += RtGraphicsMeshRegister();
    n += RtGraphicsRendererRegister();
    n += RtGraphicsAnimationRegister();
    n += RtGraphicsWave9GfxRegister();
    n += RtGraphicsSunLightRegister();
    n += RtGraphicsWave21GfxbodyRegister();

    // WAVE 23 LANE GFXBODY. Counted into `n` and ALSO printed on its own line
    // below, for the reason the GFXD and GFXVT lines give: a run log must carry
    // a line only THIS build emits, because the on-disk md5 says which exe is on
    // disk, not which one ran, and ten lanes share one game directory.
    int gfxbody23 = 0;
    gfxbody23 = RtGraphicsWave23GfxbodyRegister();
    n += gfxbody23;

    // WAVE 21 LANE GFXVT — the eight ?Type@/?TypeName@/?GetFileVersion@ slot
    // bodies of ??_7AnimationEvent@ / ??_7MaterialInstance@ / ??_7Particle@.
    // Counted into `n` above so the existing identity line carries them; the
    // per-lane count is printed separately below for the same reason lane
    // GFXD's is (a run log must carry a line only THIS build emits).
    int gfxti = 0;
    gfxti = RtGraphicsTypeInfoAccessorsRegister();
    n += gfxti;

    // WAVE 24 LANE GFXMAT -- the four remaining ??_7MaterialInstance@ slot
    // bodies (5 Serialize, 6 Reconstruct, 7 Destruct, 8 Construct) and the two
    // Graphics ?Serialize@ rows. Its own identity line is printed below for the
    // reason every other lane line here gives: the on-disk md5 says which exe
    // is on disk, not which one ran.
    int gfxmat = 0;
    gfxmat = RtGraphicsWave24GfxmatRegister();
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
    gfxbig = RtGraphicsWave27GfxbigRegister();
    n += gfxbig;

    // WAVE 27 LANE GFXMESH -- the eight non-slot-0 bodies of
    // ??_7EdgeMesh@Graphics@@6B@ (slots 1..8). Slot 0 is the unnamed vector
    // deleting destructor and lives in runtime/src/object/deleting_dtor.cpp as
    // a table row, not here. Counted into `n` and printed on its own line for
    // the reason every other lane line here gives: the on-disk md5 says which
    // exe is on disk, not which one ran, and several lanes share one game dir.
    int gfxedgevt = 0;
    gfxedgevt = RtGraphicsEdgeMeshVtableRegister();
    n += gfxedgevt;

    // WAVE 27 LANE GFXMESH, second half -- four slot bodies on the two INSTANCE
    // tables (EdgeMeshInstance Update + GetInstanceTransform, MeshInstance
    // GetSubTransform + GetInstanceTransform). They live in mesh.cpp because
    // MeshInstance::GetInstanceTransform reuses that file's RtInstancedPoolSlot
    // helper, which carries the MOVSXD/SAR/second-deref corrections
    // WAVE6_LANE_E.md paid for; a second copy is what this avoids.
    int gfxmeshvt = 0;
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
    gfxpart = RtGraphicsWave27GfxpartRegister();
    n += gfxpart;

    // WAVE 28 LANE GFXBODY -- the remaining named Graphics BODY rows, every one
    // of them a CALLEE that Wave 27 lane GFXBIG's three bodies put into the
    // `called` column. ITS OWN SWITCH because its blast radius is narrower than
    // GFXBIG's but overlaps it exactly: with RT_GFXBIG=1 and this one
    // 0, GFXBIG's bodies run and reach the DLL for these callees, which is the
    // A/B that separates "the big body is wrong" from "one of its callees is".
    // Sharing GFXBIG's variable would destroy that separation.
    int gfxbody28 = 0;
    gfxbody28 = RtGraphicsWave28GfxbodyRegister();
    n += gfxbody28;
    printf("[RtPort] GFXBODY28 (Wave 28 lane GFXBODY): %d Graphics body exports "
           "registered\n", gfxbody28);

    // WAVE 29 LANE GFXBODY -- four of Graphics' eight remaining BODY rows:
    // Scene::AddGlobalDrawItems, Renderer::AddLight, Renderer::EndDrawGroup and
    // Animation::GetRootPoseAtTime. The first three are callees lane PARTSYS
    // put into the `called` column and did not port; the fourth is Wave 28
    // residual R6. ITS OWN SWITCH, deliberately not shared with GFXBODY28 or
    // PARTSYS: with PARTSYS armed and this at 0, PARTSYS's
    // SunLight::AddDrawItems runs and reaches the DLL for exactly these
    // callees, which separates "the caller is wrong" from "a callee is".
    // DEFAULT ON -- the wave kill switch is a kill switch, so =0 disarms and anything
    // else arms (a lane that ports behind an opt-in switch moves the live count
    // not at all).
    int gfxbody29 = 0;
    gfxbody29 = RtGraphicsWave29GfxbodyRegister();
    n += gfxbody29;
    printf("[RtPort] GFXBODY29 (Wave 29 lane GFXBODY): %d Graphics body exports "
           "registered\n", gfxbody29);

    // WAVE 9 LANE GFXD -- the bottom half of the Graphics::Renderer class
    // lane: Renderer::Reset, Renderer::AddScene, two more Graphics::Camera
    // constructors, and the seven unexported Collection::Array
    // instantiations underneath them. Registered but INERT -- no door in
    // the tree resolves any of them today, so this changes nothing in a
    // live run. Its oracle is its own probe, behind RT_GFXD_PROBE.
    // WAVE 28 LANE PARTSYS -- ?Update@ParticleSystemInstance@ (the whole
    // particle simulation, and the LAST unported slot of
    // ??_7ParticleSystemInstance@Graphics@@6B@), ?RemoveState@ParticleInstance@,
    // and three ??_7SunLight@Graphics@@6B@ slots. DEFAULT ON: the wave kill switch is a
    // kill switch, so RT_PARTSYS=0 leaves every call on the DLL for an
    // A/B and anything else arms it (kickoff §2.2 caveat 1 -- a lane that ports
    // behind an opt-in switch moves the live count not at all).
    int partsys = 0;
    partsys = RtGraphicsWave28PartsysRegister();
    n += partsys;

    // WAVE 29 LANE SLOTS -- two slots of ??_7MeshInstance@Graphics@@6B@:
    // ?Update@MeshInstance@ (slot 1) and ?SetSharedInstanceData@MeshInstance@
    // (slot 7), taking the table 6/9 -> 8/9. DEFAULT ON, same kill-switch shape
    // as PARTSYS above: RT_SLOTS29=0 leaves both calls on the DLL for
    // an A/B, anything else arms them.
    int slots29 = 0;
    slots29 = RtGraphicsWave29SlotsRegister();
    n += slots29;

    // 2026-09-12: Graphics::Material, the whole vtable (material.cpp). No switch.
    n += RtGraphicsMaterialRegister();

    // 2026-09-12, cutover closure batch 3: the Graphics animation / skeleton
    // cluster -- AnimationState::Serialize / ResetAnimationState,
    // AnimationManager::GetPose, Animation::GetPoseAtTime, the six Array<T>
    // serializers and the three element copy constructors underneath them
    // (animationstate.cpp), plus the seventeen Collection::Array<T>
    // instantiations they drive (collection/array_graphics_anim.cpp, whose
    // register function animationstate.cpp calls so this stays one line).
    // NO SWITCH -- DIRECTION.md §2: a port registers unconditionally.
    { extern int RtGraphicsAnimationStateRegister();
      n += RtGraphicsAnimationStateRegister(); }

    // VTABLE BATCH 7 (2026-09-12): Graphics::AnimationGroup and
    // Graphics::ParticleSystem (vtbatch7_animgroup_particlesystem.cpp).
    // NO SWITCH -- DIRECTION.md §2: a port registers unconditionally.
    { extern int RtGraphicsBatch7AnimGroupParticleSystemRegister();
      n += RtGraphicsBatch7AnimGroupParticleSystemRegister(); }

    int gfxd = 0;
    gfxd = RtGraphicsWave9GfxdRegister();

    int grid = 0;
    grid = RtGraphicsGridRegister();
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
    // Wave 28 lane PARTSYS's own identity line, for the same reason every other
    // lane line here gives: the on-disk md5 says which exe is on disk, not which
    // one ran, and seven lanes share one game directory.
    printf("[RtPort] Wave 28 lane PARTSYS: %d particle/sunlight exports registered\n", partsys);
    // Wave 29 lane SLOTS' identity line, same reason as every other line here.
    printf("[RtPort] Wave 29 lane SLOTS: %d MeshInstance vtable-slot exports registered\n", slots29);
    // kickoff warning 10: a *Summary() nothing calls prints a zero that means
    // "never printed", not "never called". It is WIRED here, at init, so a run
    // that registers this lane provably emits the line even if neither body ever
    // runs -- which is exactly the distinction warning 10 is about.
    RtGraphicsWave29SlotsSummary();

    // BATCH 4 — the render/cull/sun-camera cluster. Registered unconditionally
    // (DIRECTION.md §2.3: a port registers unconditionally; no new switches).
    //   cull.cpp       Scene::AddCulledDrawItems + the two cull-stack helpers
    //   sun_camera.cpp SunLight::GetSunCamera
    {
        extern int RtGraphicsCullRegister();
        extern int RtGraphicsSunCameraRegister();
        const int cull = RtGraphicsCullRegister();
        const int sun  = RtGraphicsSunCameraRegister();
        printf("[RtPort] BATCH 4: %d cull + %d sun-camera entries registered\n", cull, sun);
    }

    // BATCH 3b — the five animation bodies with hand-scheduled floating point:
    // AnimationState::PlayAnimation / CopyInstancedToUnique / UpdateNodes,
    // AnimationManager::GetInstanceId and MeshInstance::GetBones
    // (runtime/src/graphics/animationstate_transforms.cpp). Registered
    // unconditionally (DIRECTION.md §2.3); no switch.
    {
        extern int RtGraphicsAnimationTransformsRegister();
        const int fp = RtGraphicsAnimationTransformsRegister();
        printf("[RtPort] BATCH 3b: %d animation-transform exports registered\n", fp);
    }

    // VTABLE BATCH 6 — graphics buffers, meshes and lights. Registered
    // unconditionally (DIRECTION.md §2.3: a port registers unconditionally; no
    // new environment switches).
    //   imagebuffer_derived.cpp  GeneratedImageBuffer / FrameCopyImageBuffer /
    //                            ReflectionImageBuffer slots + ctors/dtors, and
    //                            Math::Transform3::operator*
    //   graphicsmesh_vtable.cpp  the whole ??_7GraphicsMesh@ table, its ctor and
    //                            dtor, and TriangleList::Serialize — it also
    //                            calls the Collection register below, so the
    //                            four Array<T> bodies arrive with it
    //   lights_vtable.cpp        AmbientLight / SpotLight whole tables + their
    //                            ctors and dtors, and SunLight's last two slots
    //   meshinstance_draw.cpp    ?Draw@MeshInstance@ and ?Draw@EdgeMeshInstance@
    //
    // CORRECTION to the "Wave 29 lane SLOTS" line printed above (PLAN.md §6
    // step 8): it says that lane took ??_7MeshInstance@Graphics@@6B@ "6/9 ->
    // 8/9". With ?Draw@MeshInstance@ (slot 2) ported here that table is 9/9,
    // and ??_7EdgeMeshInstance@ is complete too — batch 6 closed slot 2, the
    // last one either of them was missing. The older line is left as written so
    // a reader who meets the 8/9 figure elsewhere recognises it.
    {
        extern int RtGraphicsImageBufferDerivedRegister();
        extern int RtGraphicsMeshVtableBatch6Register();
        extern int RtCollectionGraphicsMeshRegister();
        extern int RtGraphicsLightsVtableRegister();
        extern int RtGraphicsMeshInstanceDrawRegister();
        const int buf   = RtGraphicsImageBufferDerivedRegister();
        const int mesh  = RtGraphicsMeshVtableBatch6Register();
        const int arr   = RtCollectionGraphicsMeshRegister();
        const int light = RtGraphicsLightsVtableRegister();
        const int draw  = RtGraphicsMeshInstanceDrawRegister();
        printf("[RtPort] BATCH 6: %d image-buffer + %d mesh + %d array + %d light "
               "+ %d instance-draw entries registered\n", buf, mesh, arr, light, draw);
    }
}
