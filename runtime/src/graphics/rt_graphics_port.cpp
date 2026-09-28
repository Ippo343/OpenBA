// ============================================================================
// runtime/src/graphics/rt_graphics_port.cpp — RtGraphicsPortInit()  Wave 6
// ============================================================================
// Each file that still carries a registration exposes one Register function
// returning the number of RegisterRuntime* calls it made. B3R RT-VG: a file
// whose every row went DEAD lost its Register function and its call here.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "scene_grid.h"
#include "../../types/graphics/Mesh.h"              // B.3.9: MeshInstance::_rootOnlyUpdates
#include "../../types/graphics/ParticleInstance.h"  // B.3.9: ParticleInstance::_frameIndex

extern int RtGraphicsSceneRegister();     // scene.cpp     (lane D)
extern int RtGraphicsMeshRegister();      // mesh.cpp      (lane E)
extern int RtGraphicsRendererRegister();  // renderer.cpp  (lane F)
extern int RtGraphicsAnimationRegister(); // animation.cpp (lane G)
extern int RtGraphicsSunLightRegister();  // sunlight.cpp (Wave 9 lane GFX2)
extern int RtGraphicsWave21GfxbodyRegister(); // wave21_lane_gfxbody.cpp (Wave 21 lane GFXBODY)
extern "C" void RtGraphicsWave29SlotsSummary();  // wave29_lane_slots.cpp -- WIRED below, kickoff warning 10
extern int RtGraphicsMeshVtableRegister(); // mesh.cpp (Wave 27 lane GFXMESH)

// BATCH C registration entry point (batchc_*.cpp).
extern "C" int RtGraphicsBatchCLeavesRegister();

void RtGraphicsPortInit() {
    // ENDGAME B.3.9 GFX (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish). Both
    // rows keep foreign readers, so they retire later (STORAGE-LAST).
    // BATCH C — the named Graphics leaves of batchc_gfx_leaves.cpp.
    // NOT inside the printf: the perf build compiles printf to ((void)0)
    // (standalone/log_strip.h), which silently dropped this whole registration
    // and left GraphicsMesh::PlatformSerialize unresolved on every perf run.
    {
        const int leaves = RtGraphicsBatchCLeavesRegister();
        printf("[RtPort] BATCH C: %d graphics leaves registered\n", leaves);
    }
    int n = 0;
    n += RtGraphicsSceneRegister();
    n += RtGraphicsMeshRegister();
    n += RtGraphicsRendererRegister();
    n += RtGraphicsAnimationRegister();
    n += RtGraphicsSunLightRegister();
    n += RtGraphicsWave21GfxbodyRegister();

    // WAVE 27 LANE GFXMESH, second half -- the instance-table slot bodies in
    // mesh.cpp. Counted into `n` and printed on its own line.
    int gfxmeshvt = 0;
    gfxmeshvt = RtGraphicsMeshVtableRegister();
    n += gfxmeshvt;
    printf("[RtPort] GFXMESHVT (Wave 27 lane GFXMESH): %d instance vtable slot "
           "bodies registered\n", gfxmeshvt);

    // 2026-09-12, cutover closure batch 3: the Graphics animation / skeleton
    // cluster (animationstate.cpp), plus the Collection::Array<T>
    // instantiations they drive (collection/array_graphics_anim.cpp, whose
    // register function animationstate.cpp calls so this stays one line).
    // NO SWITCH -- DIRECTION.md §2: a port registers unconditionally.
    { extern int RtGraphicsAnimationStateRegister();
      n += RtGraphicsAnimationStateRegister(); }

    printf("[RtPort] Graphics: %d exports registered\n", n);
    // kickoff warning 10: a *Summary() nothing calls prints a zero that means
    // "never printed", not "never called". It is WIRED here, at init, so a run
    // provably emits the line even if neither body ever runs -- which is
    // exactly the distinction warning 10 is about.
    RtGraphicsWave29SlotsSummary();

    // VTABLE BATCH 6 — the four Array<T> bodies of the ??_7GraphicsMesh@ family
    // (runtime/src/collection/array_graphics_mesh.cpp).
    {
        extern int RtCollectionGraphicsMeshRegister();
        const int arr = RtCollectionGraphicsMeshRegister();
        printf("[RtPort] BATCH 6: %d array entries registered\n", arr);
    }
}
