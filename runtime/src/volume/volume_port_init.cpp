// ============================================================================
// runtime/src/volume/volume_port_init.cpp — RtVolumePortInit()
// ============================================================================
//
// Wave 1, Lane 3 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Registers every Volume::Box / Volume::Rectangle / Volume::Frustum port
// (box.cpp, rectangle.cpp, frustum.cpp) so our ~1,180 call sites flip to them.
// Called from RuntimePortInit() in standalone/resolve_standalone.cpp.
//
// Kill switch: BANISHED_RT_VOLUME=0 leaves the whole namespace unregistered,
// so every caller keeps going through Runtime-x64.dll exactly as before.
// ============================================================================

#include <cstdio>
#include "../../types/volume/Volume.h"
#include "../../funcs/rt_port.h"

// box.cpp
Box*   RtVolume_Box_Ctor(Box* self);
Box*   RtVolume_Box_Assign(Box* self, const Box* other);
Box*   RtVolume_Box_Extend(const Box* self, Box* ret, const Vector3* p);
void   RtVolume_Box_GetCorners(const Box* self, Vector3* out);
bool   RtVolume_Box_Intersect(const Box* self, const Box* other);

// rectangle.cpp
Rectangle* RtVolume_Rectangle_Ctor(Rectangle* self);

// frustum.cpp
Frustum* RtVolume_Frustum_Ctor_Perspective(Frustum* self, const Scalar* nearZ, const Scalar* farZ,
                                            const Scalar* fovX, const Scalar* fovY);
Frustum* RtVolume_Frustum_Ctor_General(Frustum* self, bool isOrthographic,
                                        const Scalar* nearZ, const Scalar* farZ,
                                        const Scalar* edgeA, const Scalar* edgeB,
                                        const Scalar* edgeC, const Scalar* edgeD);
Frustum* RtVolume_Frustum_MulTransform3(const Frustum* self, Frustum* ret, const Transform3* t);
unsigned int RtVolume_Frustum_Contains(const Frustum* self, const Box* box, unsigned int mask);
void     RtVolume_Frustum_GetCorners(const Frustum* self, Vector3* out);
Vector3* RtVolume_Frustum_GetScreenDirection(const Frustum* self, Vector3* ret, float u, float v);
// Wave 9 lane P — the sixth and last Frustum member our tree asks for.
Matrix*  RtVolume_Frustum_Projection(const Frustum* self, Matrix* ret, bool zeroToOneDepth);

void RtVolumePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_VOLUME")) {
        printf("[RtVolume] wave disabled (BANISHED_RT_VOLUME=0) — Runtime-x64.dll originals in use\n");
        return;
    }

    RegisterRuntimeExport("??0Box@Volume@@QEAA@XZ", (void*)&RtVolume_Box_Ctor);
    RegisterRuntimeExport("??4Box@Volume@@QEAAAEAV01@AEBV01@@Z", (void*)&RtVolume_Box_Assign);
    RegisterRuntimeExport("?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z", (void*)&RtVolume_Box_Extend);
    RegisterRuntimeExport("?GetCorners@Box@Volume@@QEBAXPEAVVector3@Math@@@Z", (void*)&RtVolume_Box_GetCorners);
    RegisterRuntimeExport("?Intersect@Box@Volume@@QEBA_NAEBV12@@Z", (void*)&RtVolume_Box_Intersect);

    RegisterRuntimeExport("??0Rectangle@Volume@@QEAA@XZ", (void*)&RtVolume_Rectangle_Ctor);

    RegisterRuntimeExport("??0Frustum@Volume@@QEAA@AEBVScalar@Math@@000@Z",
                           (void*)&RtVolume_Frustum_Ctor_Perspective);
    RegisterRuntimeExport("??0Frustum@Volume@@QEAA@_NAEBVScalar@Math@@11111@Z",
                           (void*)&RtVolume_Frustum_Ctor_General);
    RegisterRuntimeExport("??DFrustum@Volume@@QEBA?AV01@AEBVTransform3@Math@@@Z",
                           (void*)&RtVolume_Frustum_MulTransform3);
    RegisterRuntimeExport("?Contains@Frustum@Volume@@QEBAIAEBVBox@2@I@Z", (void*)&RtVolume_Frustum_Contains);
    RegisterRuntimeExport("?GetCorners@Frustum@Volume@@QEBAXQEAVVector3@Math@@@Z",
                           (void*)&RtVolume_Frustum_GetCorners);
    RegisterRuntimeExport("?GetScreenDirection@Frustum@Volume@@QEBA?AVVector3@Math@@MM@Z",
                           (void*)&RtVolume_Frustum_GetScreenDirection);
    // Wave 9 lane P. Registered by NAME only: the one in-tree caller,
    // runtime/src/graphics/renderer.cpp's RtFrustumProjection, already goes
    // through ResolveExport, and no site reaches it by address.
    RegisterRuntimeExport("?Projection@Frustum@Volume@@QEBA?AVMatrix@Math@@_N@Z",
                           (void*)&RtVolume_Frustum_Projection);

    printf("[RtVolume] 12 exports registered (Box x5, Rectangle x1, Frustum x6)\n");
}
