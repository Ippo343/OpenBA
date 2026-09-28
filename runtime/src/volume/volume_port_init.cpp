// ============================================================================
// runtime/src/volume/volume_port_init.cpp — RtVolumePortInit()
// ============================================================================
//
// Wave 1, Lane 3 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Registers every Volume::Box / Volume::Rectangle / Volume::Frustum port
// (box.cpp, rectangle.cpp, frustum.cpp) so our ~1,180 call sites flip to them.
// Called from RuntimePortInit() in standalone/resolve_standalone.cpp.
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
    // B3R W2-EZ: the Box ctor / operator= / Extend and the Frustum ctor /
    // operator* / Contains rows went: their one reader was EngineFuncsInit (deleted).
    RegisterRuntimeExport("?Intersect@Box@Volume@@QEBA_NAEBV12@@Z", (void*)&RtVolume_Box_Intersect);

    RegisterRuntimeExport("??0Rectangle@Volume@@QEAA@XZ", (void*)&RtVolume_Rectangle_Ctor);

    printf("[RtVolume] 2 exports registered (Box::Intersect, Rectangle ctor)\n");
}
