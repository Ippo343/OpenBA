// ============================================================================
// runtime/src/graphics/rt_graphics_port.cpp — RtGraphicsPortInit()  Wave 6
// ============================================================================
// Each file that still carries a registration exposes one Register function
// returning the number of RegisterRuntime* calls it made. B3R RT-VG: a file
// whose every row went DEAD lost its Register function and its call here.
// B3R W2-RT: so did BATCH C (batchc_gfx_leaves.cpp), Animation, SunLight,
// Wave 21 GFXBODY, AnimationState and the two Collection BATCH 6 / anim
// registrars: none of their rows had a reader.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtGraphicsSceneRegister();     // scene.cpp     (lane D)
extern int RtGraphicsMeshRegister();      // mesh.cpp      (lane E)
extern int RtGraphicsRendererRegister();  // renderer.cpp  (lane F)
extern "C" void RtGraphicsWave29SlotsSummary();  // wave29_lane_slots.cpp -- WIRED below, kickoff warning 10
extern int RtGraphicsMeshVtableRegister(); // mesh.cpp (Wave 27 lane GFXMESH)

void RtGraphicsPortInit() {
    int n = 0;
    n += RtGraphicsSceneRegister();
    n += RtGraphicsMeshRegister();
    n += RtGraphicsRendererRegister();

    // WAVE 27 LANE GFXMESH, second half -- the instance-table slot bodies in
    // mesh.cpp. Counted into `n` and printed on its own line.
    int gfxmeshvt = 0;
    gfxmeshvt = RtGraphicsMeshVtableRegister();
    n += gfxmeshvt;
    printf("[RtPort] GFXMESHVT (Wave 27 lane GFXMESH): %d instance vtable slot "
           "bodies registered\n", gfxmeshvt);

    printf("[RtPort] Graphics: %d exports registered\n", n);
    // kickoff warning 10: a *Summary() nothing calls prints a zero that means
    // "never printed", not "never called". It is WIRED here, at init, so a run
    // provably emits the line even if neither body ever runs -- which is
    // exactly the distinction warning 10 is about.
    RtGraphicsWave29SlotsSummary();
}
