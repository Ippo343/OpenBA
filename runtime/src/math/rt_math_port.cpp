// ============================================================================
// runtime/src/math/rt_math_port.cpp — RtMathPortInit()
// ============================================================================
// Registers Lane 1's ports (Scalar/Vector2/Vector3/Vector4) with the runtime
// registry, then calls Lane 2's RtMathTransformRegister() (Matrix/Quaternion/
// Transform family) so a single BANISHED_RT_MATH switch controls the whole
// Math namespace. See docs/runtime_port/PLAN.md §6 step 5.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "../../funcs/rt_libm.h"
#include "vector.h"

// Lane 2's half of Math (runtime/src/math/transform.cpp), registered here so
// BANISHED_RT_MATH is the one switch for the whole namespace.
extern void RtMathTransformRegister();
// Wave 9 lane P (runtime/src/math/wave9_lane_p.cpp): Quaternion::Lerp and
// Transform::AsTransform3, the two Math exports waves 1-8 left on the DLL.
extern int RtMathWave9LanePRegister();
// Wave 9 lane GFXR (runtime/src/math/wave9_lane_gfxr.cpp): Matrix::Inverse,
// the last Math export on the residue list and an unsatisfied door inside our
// own Video::Context::SetCamera.
extern int RtMathWave9LaneGfxrRegister();

void RtMathPortInit() {
    // Must run before the BANISHED_RT_MATH check below: Volume's ports use the
    // same RtLibm seam and RtVolumePortInit() must not depend on whether Math
    // happens to be enabled. Safe to call more than once.
    RtLibm::Init();

    if (!RtWaveEnabled("BANISHED_RT_MATH")) {
        printf("[RtMathPortInit] BANISHED_RT_MATH=0 -- Math namespace port disabled, DLL exports stay live\n");
        return;
    }

    int n = 0;
    // Scalar
    RegisterRuntimeExport("?Abs@Scalar@Math@@QEBA?AV12@XZ", (void*)&RtMath_Scalar_Abs); ++n;
    RegisterRuntimeExport("?Lerp@Scalar@Math@@QEAA?AV12@AEBV12@0@Z", (void*)&RtMath_Scalar_Lerp); ++n;
    RegisterRuntimeExport("?Max@Scalar@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Scalar_Max); ++n;

    // Vector2
    RegisterRuntimeExport("??0Vector2@Math@@QEAA@XZ", (void*)&RtMath_Vector2_Ctor); ++n;
    RegisterRuntimeExport("??0Vector2@Math@@QEAA@AEBVScalar@1@0@Z", (void*)&RtMath_Vector2_Ctor_ss); ++n;
    RegisterRuntimeExport("??XVector2@Math@@QEAAXAEBV01@@Z", (void*)&RtMath_Vector2_MulAssign); ++n;
    RegisterRuntimeExport("?Dot@Vector2@Math@@QEBA?AVScalar@2@AEBV12@@Z", (void*)&RtMath_Vector2_Dot); ++n;
    RegisterRuntimeExport("?Max@Vector2@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Vector2_Max); ++n;
    RegisterRuntimeExport("?Min@Vector2@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Vector2_Min); ++n;
    RegisterRuntimeExport("?Length@Vector2@Math@@QEBA?AVScalar@2@XZ", (void*)&RtMath_Vector2_Length); ++n;
    RegisterRuntimeExport("?LengthSquared@Vector2@Math@@QEBA?AVScalar@2@XZ", (void*)&RtMath_Vector2_LengthSquared); ++n;
    RegisterRuntimeExport("?Normalize@Vector2@Math@@QEBA?AV12@XZ", (void*)&RtMath_Vector2_Normalize); ++n;
    RegisterRuntimeExport("?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z", (void*)&RtMath_Vector2_NormalizeWithMag); ++n;
    RegisterRuntimeExport("?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtMath_Vector2_Serialize); ++n;

    // Vector3
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@XZ", (void*)&RtMath_Vector3_Ctor); ++n;
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@MMM@Z", (void*)&RtMath_Vector3_Ctor_fff); ++n;
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@AEBVScalar@1@00@Z", (void*)&RtMath_Vector3_Ctor_sss); ++n;
    RegisterRuntimeExport("??0Vector3@Math@@QEAA@AEBVScalar@1@@Z", (void*)&RtMath_Vector3_Ctor_s); ++n;
    RegisterRuntimeExport("?Cross@Vector3@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Vector3_Cross); ++n;
    RegisterRuntimeExport("?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z", (void*)&RtMath_Vector3_Dot); ++n;
    RegisterRuntimeExport("?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z", (void*)&RtMath_Vector3_FromYawPitch); ++n;
    RegisterRuntimeExport("?Heading@Vector3@Math@@QEBAMXZ", (void*)&RtMath_Vector3_Heading); ++n;
    RegisterRuntimeExport("?Length@Vector3@Math@@QEBA?AVScalar@2@XZ", (void*)&RtMath_Vector3_Length); ++n;
    RegisterRuntimeExport("?Lerp@Vector3@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z", (void*)&RtMath_Vector3_Lerp); ++n;
    RegisterRuntimeExport("?Min@Vector3@Math@@QEBA?AV12@AEBV12@@Z", (void*)&RtMath_Vector3_Min); ++n;
    RegisterRuntimeExport("?Normalize@Vector3@Math@@QEBA?AV12@XZ", (void*)&RtMath_Vector3_Normalize); ++n;
    RegisterRuntimeExport("?Normalize@Vector3@Math@@QEBA?AV12@AEAVScalar@2@@Z", (void*)&RtMath_Vector3_NormalizeWithMag); ++n;
    RegisterRuntimeExport("??8Vector3@Math@@QEBA_NAEBV01@@Z", (void*)&RtMath_Vector3_Equals); ++n;
    RegisterRuntimeExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtMath_Vector3_Serialize); ++n;

    // Vector4
    RegisterRuntimeExport("??0Vector4@Math@@QEAA@AEBVScalar@1@@Z", (void*)&RtMath_Vector4_Ctor_s); ++n;
    RegisterRuntimeExport("??0Vector4@Math@@QEAA@AEBVVector3@1@AEBVScalar@1@@Z", (void*)&RtMath_Vector4_Ctor_v3s); ++n;
    RegisterRuntimeExport("?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtMath_Vector4_Serialize); ++n;

    printf("[RtMathPortInit] Lane 1 (Scalar/Vector2/Vector3/Vector4): %d exports registered\n", n);

    RtMathTransformRegister();  // Lane 2: Matrix/Quaternion/Transform/Transform2/Transform3

    printf("[RtMathPortInit] Wave 9 lane P: %d exports registered (Quaternion::Lerp, "
           "Transform::AsTransform3)\n", RtMathWave9LanePRegister());
    printf("[RtMathPortInit] Wave 9 lane GFXR: %d exports registered (Matrix::Inverse)\n",
           RtMathWave9LaneGfxrRegister());
}
