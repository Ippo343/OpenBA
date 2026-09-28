// ============================================================================
// runtime/src/math/rt_math_port.cpp — RtMathPortInit()
// ============================================================================
// Registers Lane 1's ports (Scalar/Vector2/Vector3/Vector4) with the runtime
// registry, then calls Lane 2's RtMathTransformRegister() (Matrix/Quaternion/
// Transform family) so the whole Math namespace is registered from one place.
// See docs/runtime_port/PLAN.md §6 step 5.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "../../funcs/rt_libm.h"
#include "vector.h"

// Lane 2's half of Math (runtime/src/math/transform.cpp).
extern void RtMathTransformRegister();
// Wave 9 lane P (runtime/src/math/wave9_lane_p.cpp): Quaternion::Lerp and
// Transform::AsTransform3, the two Math exports waves 1-8 left on the DLL.
extern int RtMathWave9LanePRegister();

void RtMathPortInit() {
    // Volume's ports use the same RtLibm seam, so this must run before
    // RtVolumePortInit(). Safe to call more than once.
    RtLibm::Init();

    int n = 0;
    // Scalar
    RegisterRuntimeExport("?Max@Scalar@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Scalar_Max); ++n;

    // Vector2
    RegisterRuntimeExport("??0Vector2@Math@@QEAA@XZ", (void*)&RtMath_Vector2_Ctor); ++n;
    RegisterRuntimeExport("?Normalize@Vector2@Math@@QEBA?AV12@XZ", (void*)&RtMath_Vector2_Normalize); ++n;
    RegisterRuntimeExport("?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtMath_Vector2_Serialize); ++n;

    // Vector3
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@MMM@Z", (void*)&RtMath_Vector3_Ctor_fff); ++n;
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@AEBVScalar@1@00@Z", (void*)&RtMath_Vector3_Ctor_sss); ++n;
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@AEBVScalar@1@@Z", (void*)&RtMath_Vector3_Ctor_s); ++n;
    RegisterRuntimeExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z", (void*)&RtMath_Vector3_Dot); ++n;
    RegisterRuntimeExport("?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z", (void*)&RtMath_Vector3_FromYawPitch); ++n;
    RegisterRuntimeExport("?Heading@Vector3@Math@@QEBAMXZ", (void*)&RtMath_Vector3_Heading); ++n;
    RegisterRuntimeExport("?Length@Vector3@Math@@QEBA?AVScalar@2@XZ", (void*)&RtMath_Vector3_Length); ++n;
    RegisterRuntimeExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ", (void*)&RtMath_Vector3_Normalize); ++n;
    RegisterRuntimeExport("?Normalize@Vector3@Math@@QEBA?AV12@AEAVScalar@2@@Z", (void*)&RtMath_Vector3_NormalizeWithMag); ++n;

    // Vector4
    RegisterRuntimeExport("??0Vector4@Math@@QEAA@AEBVScalar@1@@Z", (void*)&RtMath_Vector4_Ctor_s); ++n;

    printf("[RtMathPortInit] Lane 1 (Scalar/Vector2/Vector3/Vector4): %d exports registered\n", n);

    RtMathTransformRegister();  // Lane 2: Matrix/Quaternion/Transform/Transform2/Transform3

    // Hoisted out of the printf: the perf build compiles printf to ((void)0) (standalone/log_strip.h).
    const int n_mathwave9lanep = RtMathWave9LanePRegister();
    printf("[RtMathPortInit] Wave 9 lane P: %d exports registered (Quaternion::Lerp, "
           "Transform::AsTransform3)\n", n_mathwave9lanep);
}
