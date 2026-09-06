// ============================================================================
// runtime/src/math/transform.cpp
// ============================================================================
// Wave 1 / Lane 2 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// §6 recipe). Owns Math::Matrix, Math::Quaternion, Math::Transform,
// Math::Transform2, Math::Transform3.
//
// ABI (see the header comment in runtime/types/math/Math.h): every one of
// these classes has a user-declared ctor, so MSVC returns it BY MEMORY. The
// register order is strict Microsoft x64:
//   member function, struct return : RCX=this,   RDX=hidden ret, R8=arg1, ...
//   static function,  struct return : RCX=hidden ret, RDX=arg1, R8=arg2, ...
//   ctor (no separate "return", `this` IS the target)  : RCX=this, RDX=arg1, ...
//   function returning a reference (operator=)          : RCX=this, RDX=arg1, ... (no hidden slot)
// Every port below is declared with parameters in EXACTLY that register
// order, matching the fn_Transform_AsMatrix_t-style typedefs already in
// engine/funcs/engine_funcs.h (self, then retval) and extending the same
// convention to the functions that also take an explicit argument (self,
// retval, arg) — get this wrong and the stack is silently corrupted.
//
// Every arithmetic port is a literal transliteration of the decompiled
// expression, term order preserved, NOT a "cleaned up" reformulation — see
// CLAUDE.md ("float addition is not associative") and PLAN.md §6 step 2.
// Variable names mirror the meaning of each retail fVarN once traced back to
// a named field, but the operation ORDER is copied verbatim.
//
// Transcendentals (sqrtf/sinf/cosf) go through RtLibm (runtime/funcs/rt_libm.h)
// rather than a direct call into Runtime-x64.dll's own CRT (at 0x180128110 /
// 0x180127d90 / 0x180126d10 respectively, confirmed by disassembly below) —
// calling the DLL directly would make the diff oracle green by construction
// while leaving the port permanently dependent on a DLL PLAN.md §4.1 says
// must eventually be absent. RtLibm defaults to our own libm; BANISHED_RT_LIBM=dll
// is a diagnostic-only escape hatch to tell libm-rounding divergence apart
// from a real bug.
// Known .rdata constants (RtMathConst in Math.h):
//   kDegToRad, kHalf, kOne, kTwo, kMinusOne, kSignMask
// Two more thresholds used only by FromDirection, not in Math.h's shared
// table (used nowhere else in this wave, kept local to this file):
//   0x180161698 = 0x3f7fbe77  (~0.99857, "nearly parallel" dot-product bound)
//   0x180161758 = 0xbf7fbe77  (its negation)
// ============================================================================

#include "transform.h"
#include "../../types/math/Math.h"
#include "rt_port.h"
#include "rt_libm.h"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace {

// FromDirection's two "nearly parallel with world Z" thresholds.
inline float BitsToFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }
constexpr uint32_t kNearParallelPosBits = 0x3f7fbe77u; //  0.998573... (0x180161698)
constexpr uint32_t kNearParallelNegBits = 0xbf7fbe77u; // -0.998573... (0x180161758)

} // namespace

// ============================================================================
// Math::Quaternion::Quaternion(float,float,float,float)   -- ctor, this=RCX
// retail 0x18001e260  ??0Quaternion@Math@@QEAA@MMMM@Z
// ============================================================================
Quaternion* RtMath_Quaternion_Ctor4(Quaternion* self, float x, float y, float z, float w) {
    RT_DIFF_BEGIN("Math::Quaternion::Quaternion(float,float,float,float)");
    self->x = x;
    self->y = y;
    self->w = w;   // retail writes w (this+0xc) before z (this+8); independent
    self->z = z;   // field stores, order is inconsequential here.
    if (rt_diff_on__) {
        Quaternion theirs{};
        typedef void (*OrigFn)(Quaternion*, float, float, float, float);
        RtOriginal<OrigFn>("??0Quaternion@Math@@QEAA@MMMM@Z")(&theirs, x, y, z, w);
        RT_DIFF_BYTES(self, &theirs, sizeof(Quaternion));
    }
    return self;
}

// ============================================================================
// Math::Transform::Transform()  -- default ctor, this=RCX
// retail 0x180015080  ??0Transform@Math@@QEAA@XZ
// ============================================================================
Transform* RtMath_Transform_CtorDefault(Transform* self) {
    RT_DIFF_BEGIN("Math::Transform::Transform()");
    self->rotation.x = 0.0f;
    self->rotation.y = 0.0f;
    self->rotation.z = 0.0f;
    self->rotation.w = RtMathConst::kOne;
    self->translation.x = 0.0f;
    self->translation.y = 0.0f;
    self->translation.z = 0.0f;
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*);
        RtOriginal<OrigFn>("??0Transform@Math@@QEAA@XZ")(&theirs);
        RT_DIFF_BYTES(self, &theirs, sizeof(Transform));
    }
    return self;
}

// ============================================================================
// Math::Transform::Transform(const Transform&)  -- copy ctor, RCX=this,RDX=other
// retail 0x180015020  ??0Transform@Math@@QEAA@AEBV01@@Z
// A plain 7-dword field copy in retail; memcpy is bit-identical.
// ============================================================================
Transform* RtMath_Transform_CtorCopy(Transform* self, const Transform* other) {
    RT_DIFF_BEGIN("Math::Transform::Transform(const Transform&)");
    std::memcpy(self, other, sizeof(Transform));
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*, const Transform*);
        RtOriginal<OrigFn>("??0Transform@Math@@QEAA@AEBV01@@Z")(&theirs, other);
        RT_DIFF_BYTES(self, &theirs, sizeof(Transform));
    }
    return self;
}

// ============================================================================
// Math::Transform::operator=(const Transform&)  -- returns Transform&, RCX=this,RDX=other
// retail 0x180014fd0  ??4Transform@Math@@QEAAAEAV01@AEBV01@@Z
// A reference return has NO hidden retval slot (unlike a by-value struct
// return); the only "output" is the mutation of `self` itself.
// ============================================================================
Transform* RtMath_Transform_Assign(Transform* self, const Transform* other) {
    RT_DIFF_BEGIN("Math::Transform::operator=(const Transform&)");
    std::memcpy(self, other, sizeof(Transform));
    if (rt_diff_on__) {
        // The result depends only on `other`, so run the original on a fresh
        // scratch object rather than touching live state a second time.
        Transform scratch{};
        typedef Transform* (*OrigFn)(Transform*, const Transform*);
        RtOriginal<OrigFn>("??4Transform@Math@@QEAAAEAV01@AEBV01@@Z")(&scratch, other);
        RT_DIFF_BYTES(self, &scratch, sizeof(Transform));
    }
    return self;
}

// ============================================================================
// Math::Transform3::Transform3(const Transform3&)  -- copy ctor
// retail 0x18001dee0  ??0Transform3@Math@@QEAA@AEBV01@@Z
// ============================================================================
Transform3* RtMath_Transform3_CtorCopy(Transform3* self, const Transform3* other) {
    RT_DIFF_BEGIN("Math::Transform3::Transform3(const Transform3&)");
    std::memcpy(self, other, sizeof(Transform3));
    if (rt_diff_on__) {
        Transform3 theirs{};
        typedef void (*OrigFn)(Transform3*, const Transform3*);
        RtOriginal<OrigFn>("??0Transform3@Math@@QEAA@AEBV01@@Z")(&theirs, other);
        RT_DIFF_BYTES(self, &theirs, sizeof(Transform3));
    }
    return self;
}

// ============================================================================
// Math::Transform2::Transform2(const Scalar& angleDeg, const Vector2& offset)
// ctor: RCX=this, RDX=angleDeg, R8=offset
// retail 0x18007dca0  ??0Transform2@Math@@QEAA@AEBVScalar@1@AEBVVector2@1@@Z
// this+0x00 = cos, +0x04 = sin, +0x08 = -sin, +0x0c = cos, +0x10 = offset.
// ============================================================================
Transform2* RtMath_Transform2_Ctor(Transform2* self, const Scalar* angleDeg, const Vector2* offset) {
    RT_DIFF_BEGIN("Math::Transform2::Transform2(const Scalar&, const Vector2&)");
    self->translation = *offset;             // retail writes the offset qword first
    float rad = angleDeg->v * RtMathConst::kDegToRad;
    float s = RtLibm::Sin(rad);
    float c = RtLibm::Cos(rad);
    self->rotX.x = c;
    self->rotX.y = s;
    self->rotY.x = -s;   // bit-exact XOR sign-flip in retail
    self->rotY.y = c;
    if (rt_diff_on__) {
        Transform2 theirs{};
        typedef void (*OrigFn)(Transform2*, const Scalar*, const Vector2*);
        RtOriginal<OrigFn>("??0Transform2@Math@@QEAA@AEBVScalar@1@AEBVVector2@1@@Z")(&theirs, angleDeg, offset);
        RT_DIFF_BYTES(self, &theirs, sizeof(Transform2));
    }
    return self;
}

// ============================================================================
// Math::Matrix::operator*(const Matrix&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=other
// retail 0x18001d980  ??DMatrix@Math@@QEBA?AV01@AEBV01@@Z
// Standard row-major 4x4 multiply, result = this * other. Verified against
// the decompiled term order for every one of the 16 output cells: k ascending
// 0..3 throughout. Computed into a local scratch first so aliasing `ret` with
// `self`/`other` (legal under the retail ABI) cannot corrupt an in-flight read.
// ============================================================================
Matrix* RtMath_Matrix_MulMatrix(const Matrix* self, Matrix* ret, const Matrix* other) {
    RT_DIFF_BEGIN("Math::Matrix::operator*(const Matrix&)");
    float out[16];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[r * 4 + c] =
                self->m[r * 4 + 0] * other->m[0 * 4 + c] +
                self->m[r * 4 + 1] * other->m[1 * 4 + c] +
                self->m[r * 4 + 2] * other->m[2 * 4 + c] +
                self->m[r * 4 + 3] * other->m[3 * 4 + c];
        }
    }
    std::memcpy(ret->m, out, sizeof(out));
    if (rt_diff_on__) {
        Matrix theirs{};
        typedef void (*OrigFn)(const Matrix*, Matrix*, const Matrix*);
        RtOriginal<OrigFn>("??DMatrix@Math@@QEBA?AV01@AEBV01@@Z")(self, &theirs, other);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
    }
    return ret;
}

// ============================================================================
// Math::Matrix::operator*(const Vector4&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18007d030  ??DMatrix@Math@@QEBA?AVVector4@1@AEBV21@@Z
// result[c] = v.y*m[4+c] + v.x*m[0+c] + v.z*m[8+c] + v.w*m[12+c]  (verified
// per-component; term order y,x,z,w is retail's, not a typo).
// ============================================================================
Vector4* RtMath_Matrix_MulVector4(const Matrix* self, Vector4* ret, const Vector4* v) {
    RT_DIFF_BEGIN("Math::Matrix::operator*(const Vector4&)");
    float out[4];
    for (int c = 0; c < 4; ++c) {
        out[c] = v->y * self->m[4 + c] + v->x * self->m[0 + c] +
                 v->z * self->m[8 + c] + v->w * self->m[12 + c];
    }
    ret->x = out[0]; ret->y = out[1]; ret->z = out[2]; ret->w = out[3];
    if (rt_diff_on__) {
        Vector4 theirs{};
        typedef void (*OrigFn)(const Matrix*, Vector4*, const Vector4*);
        RtOriginal<OrigFn>("??DMatrix@Math@@QEBA?AVVector4@1@AEBV21@@Z")(self, &theirs, v);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector4));
    }
    return ret;
}

// ============================================================================
// Math::Transform2::operator*(const Transform2&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=other
// retail 0x180078650  ??DTransform2@Math@@QEBA?AV01@AEBV01@@Z
// ============================================================================
Transform2* RtMath_Transform2_MulTransform2(const Transform2* self, Transform2* ret, const Transform2* other) {
    RT_DIFF_BEGIN("Math::Transform2::operator*(const Transform2&)");
    float rotXx = self->rotX.x * other->rotX.x + self->rotX.y * other->rotY.x;
    float rotXy = self->rotX.x * other->rotX.y + self->rotX.y * other->rotY.y;
    float rotYx = self->rotY.x * other->rotX.x + self->rotY.y * other->rotY.x;
    float rotYy = self->rotY.x * other->rotX.y + self->rotY.y * other->rotY.y;
    float tx = other->rotX.x * self->translation.x + self->translation.y * other->rotY.x + other->translation.x;
    float ty = other->rotX.y * self->translation.x + other->rotY.y * self->translation.y + other->translation.y;
    ret->rotX.x = rotXx; ret->rotX.y = rotXy;
    ret->rotY.x = rotYx; ret->rotY.y = rotYy;
    ret->translation.x = tx; ret->translation.y = ty;
    if (rt_diff_on__) {
        Transform2 theirs{};
        typedef void (*OrigFn)(const Transform2*, Transform2*, const Transform2*);
        RtOriginal<OrigFn>("??DTransform2@Math@@QEBA?AV01@AEBV01@@Z")(self, &theirs, other);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform2));
    }
    return ret;
}

// ============================================================================
// Math::Transform2::operator*(const Vector2&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18006c8e0  ??DTransform2@Math@@QEBA?AVVector2@1@AEBV21@@Z
// ============================================================================
Vector2* RtMath_Transform2_MulVector2(const Transform2* self, Vector2* ret, const Vector2* v) {
    RT_DIFF_BEGIN("Math::Transform2::operator*(const Vector2&)");
    float x = v->y * self->rotY.x + v->x * self->rotX.x + self->translation.x;
    float y = v->y * self->rotY.y + v->x * self->rotX.y + self->translation.y;
    ret->x = x; ret->y = y;
    if (rt_diff_on__) {
        Vector2 theirs{};
        typedef void (*OrigFn)(const Transform2*, Vector2*, const Vector2*);
        RtOriginal<OrigFn>("??DTransform2@Math@@QEBA?AVVector2@1@AEBV21@@Z")(self, &theirs, v);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector2));
    }
    return ret;
}

// ============================================================================
// Math::Transform3::operator*(const Vector3&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18001fbc0  ??DTransform3@Math@@QEBA?AVVector3@1@AEBV21@@Z
// ============================================================================
Vector3* RtMath_Transform3_MulVector3(const Transform3* self, Vector3* ret, const Vector3* v) {
    RT_DIFF_BEGIN("Math::Transform3::operator*(const Vector3&)");
    float x = v->y * self->axisY.x + v->x * self->axisX.x + v->z * self->axisZ.x + self->translation.x;
    float z = v->y * self->axisY.z + v->x * self->axisX.z + v->z * self->axisZ.z + self->translation.z;
    float y = v->y * self->axisY.y + v->x * self->axisX.y + v->z * self->axisZ.y + self->translation.y;
    ret->x = x; ret->y = y; ret->z = z;
    if (rt_diff_on__) {
        Vector3 theirs{};
        typedef void (*OrigFn)(const Transform3*, Vector3*, const Vector3*);
        RtOriginal<OrigFn>("??DTransform3@Math@@QEBA?AVVector3@1@AEBV21@@Z")(self, &theirs, v);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector3));
    }
    return ret;
}

// ============================================================================
// Math::Transform::AsMatrix() const
// member, struct return: RCX=this, RDX=hidden ret
// retail 0x18001e5f0  ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ
// ============================================================================
Matrix* RtMath_Transform_AsMatrix(const Transform* self, Matrix* ret) {
    RT_DIFF_BEGIN("Math::Transform::AsMatrix()");
    float qx = self->rotation.x;
    float qy = self->rotation.y;
    float qz = self->rotation.z;
    float qw = self->rotation.w;

    float qwqx2 = qw * qx * RtMathConst::kTwo;
    float qwqz2 = qw * qz * RtMathConst::kTwo;
    float qyqy2 = qy * qy * RtMathConst::kTwo;
    float qxqx2 = qx * qx * RtMathConst::kTwo;
    float qyqx2 = qy * qx * RtMathConst::kTwo;
    float qzqx2 = qz * qx * RtMathConst::kTwo;
    float qzqz2 = qz * qz * RtMathConst::kTwo;
    float qzqy2 = qz * qy * RtMathConst::kTwo;
    float qwqy2 = qw * qy * RtMathConst::kTwo;

    float out[16] = {0};
    out[4] = qwqz2 + qyqx2;
    out[8] = qzqx2 - qwqy2;
    out[9] = qwqx2 + qzqy2;
    out[10] = RtMathConst::kOne - (qyqy2 + qxqx2);
    out[5] = RtMathConst::kOne - (qzqz2 + qxqx2);
    out[0] = RtMathConst::kOne - (qzqz2 + qyqy2);
    out[1] = qyqx2 - qwqz2;
    out[12] = self->translation.x;
    out[2] = qwqy2 + qzqx2;
    out[6] = qzqy2 - qwqx2;
    out[13] = self->translation.y;
    out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f;
    out[14] = self->translation.z;
    out[15] = RtMathConst::kOne;
    std::memcpy(ret->m, out, sizeof(out));
    if (rt_diff_on__) {
        Matrix theirs{};
        typedef void (*OrigFn)(const Transform*, Matrix*);
        RtOriginal<OrigFn>("?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ")(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
    }
    return ret;
}

// ============================================================================
// Math::Transform3::AsMatrix() const
// member, struct return: RCX=this, RDX=hidden ret
// retail 0x1800346e0  ?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ
// Pure rearrangement, no arithmetic — order does not matter for fidelity.
// ============================================================================
Matrix* RtMath_Transform3_AsMatrix(const Transform3* self, Matrix* ret) {
    RT_DIFF_BEGIN("Math::Transform3::AsMatrix()");
    float out[16] = {0};
    out[0] = self->axisX.x; out[1] = self->axisX.y; out[2] = self->axisX.z; out[3] = 0.0f;
    out[4] = self->axisY.x; out[5] = self->axisY.y; out[6] = self->axisY.z; out[7] = 0.0f;
    out[8] = self->axisZ.x; out[9] = self->axisZ.y; out[10] = self->axisZ.z; out[11] = 0.0f;
    out[12] = self->translation.x; out[13] = self->translation.y; out[14] = self->translation.z;
    out[15] = RtMathConst::kOne;
    std::memcpy(ret->m, out, sizeof(out));
    if (rt_diff_on__) {
        Matrix theirs{};
        typedef void (*OrigFn)(const Transform3*, Matrix*);
        RtOriginal<OrigFn>("?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ")(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
    }
    return ret;
}

// ============================================================================
// Math::Transform::Inverse() const
// member, struct return: RCX=this, RDX=hidden ret
// retail 0x18005db50  ?Inverse@Transform@Math@@QEBA?AV12@XZ
// Rigid inverse: rotation' = conjugate(q); translation' is formed from a
// quaternion-vector-rotate expansion of -translation by the conjugate.
// Transliterated verbatim from the decompile — NOT the clean textbook
// formula, retail's exact term order.
// ============================================================================
Transform* RtMath_Transform_Inverse(const Transform* self, Transform* ret) {
    RT_DIFF_BEGIN("Math::Transform::Inverse()");
    float qx = self->rotation.x;
    float negQx = -qx;
    float negTx = -self->translation.x;
    float negTz = -self->translation.z;
    float negTy = -self->translation.y;
    float qw = self->rotation.w;
    float qz = self->rotation.z;
    float negQz = -qz;
    float qy = self->rotation.y;
    float negQy = -qy;

    float t10 = (negTx * qw + negQx * 0.0f + negTy * negQz) - negTz * negQy;
    float t11 = (negQy * 0.0f - negTx * negQz) + negTy * qw + negTz * negQx;
    float t7  = ((negTx * negQy + negQz * 0.0f) - negTy * negQx) + negTz * qw;
    float t6  = ((qw * 0.0f - negTx * negQx) - negTy * negQy) - negTz * negQz;

    ret->rotation.x = negQx;
    ret->rotation.y = negQy;
    ret->rotation.z = negQz;
    ret->rotation.w = qw;
    ret->translation.x = (t6 * qx + t10 * qw + t7 * qy) - t11 * qz;
    ret->translation.y = (t11 * qw - t7 * qx) + t6 * qy + t10 * qz;
    ret->translation.z = ((t11 * qx + t7 * qw) - t10 * qy) + t6 * qz;
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(const Transform*, Transform*);
        RtOriginal<OrigFn>("?Inverse@Transform@Math@@QEBA?AV12@XZ")(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Math::Transform3::Inverse() const
// member, struct return: RCX=this, RDX=hidden ret
// retail 0x180060040  ?Inverse@Transform3@Math@@QEBA?AV12@XZ
// Rigid inverse: rotation' = transpose(R) (orthonormal basis), translation' =
// -transpose(R) * translation.
// ============================================================================
Transform3* RtMath_Transform3_Inverse(const Transform3* self, Transform3* ret) {
    RT_DIFF_BEGIN("Math::Transform3::Inverse()");
    float negTz = -self->translation.z;
    float negTx = -self->translation.x;
    float negTy = -self->translation.y;

    ret->axisX.x = self->axisX.x;
    ret->axisX.y = self->axisY.x;
    ret->axisX.z = self->axisZ.x;
    ret->axisY.x = self->axisX.y;
    ret->axisY.y = self->axisY.y;
    ret->axisY.z = self->axisZ.y;
    ret->axisZ.x = self->axisX.z;
    ret->axisZ.y = self->axisY.z;
    ret->axisZ.z = self->axisZ.z;

    ret->translation.x = negTy * self->axisX.y + negTx * self->axisX.x + negTz * self->axisX.z;
    ret->translation.y = negTy * self->axisY.y + negTx * self->axisY.x + negTz * self->axisY.z;
    ret->translation.z = negTy * self->axisZ.y + negTx * self->axisZ.x + negTz * self->axisZ.z;
    if (rt_diff_on__) {
        Transform3 theirs{};
        typedef void (*OrigFn)(const Transform3*, Transform3*);
        RtOriginal<OrigFn>("?Inverse@Transform3@Math@@QEBA?AV12@XZ")(self, &theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform3));
    }
    return ret;
}

// ============================================================================
// Math::Quaternion::operator*(const Quaternion&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=other
// retail 0x18001e280  ??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z
// NOT in the assigned function list, but required by Quaternion::Rotate below
// (retail calls it internally) — ported here since it is genuine
// Math::Quaternion surface, and registered as its own export too.
// ============================================================================
Quaternion* RtMath_Quaternion_MulQuaternion(const Quaternion* self, Quaternion* ret, const Quaternion* other) {
    RT_DIFF_BEGIN("Math::Quaternion::operator*(const Quaternion&)");
    float x = (self->x * other->w + other->x * self->w + self->z * other->y) - self->y * other->z;
    float y = (self->y * other->w - self->z * other->x) + other->y * self->w + other->z * self->x;
    float w = ((other->w * self->w - self->x * other->x) - self->y * other->y) - other->z * self->z;
    // Final term is other.z * self.W, not self.y. Retail (0x18001e280) loads
    // fVar2 = *(this + 0xc) — the w component — and the z row reuses it:
    //   ((a.z*b.w + a.y*b.x) - b.y*a.x) + b.z*a.w
    // Using self->y here made every quaternion product wrong in z. It stayed
    // invisible through the whole of Wave 1 because NOTHING called it: this
    // export and Quaternion::Rotate above it had zero calls in every diff run
    // until Transform::operator*(Vector3) was added, at which point they
    // diverged on 82% and 100% of calls respectively.
    float z = ((self->z * other->w + self->y * other->x) - other->y * self->x) + other->z * self->w;
    ret->x = x; ret->y = y; ret->w = w; ret->z = z;
    if (rt_diff_on__) {
        Quaternion theirs{};
        typedef void (*OrigFn)(const Quaternion*, Quaternion*, const Quaternion*);
        RtOriginal<OrigFn>("??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z")(self, &theirs, other);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Quaternion));
    }
    return ret;
}

// ============================================================================
// Math::Quaternion::Rotate(const Vector3&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18001e140  ?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z
// v' = q * Quaternion(v, 0) * conjugate(q). Confirmed by disassembly: RCX at
// both inner operator* calls is `this` unmodified — NOT the conjugate — so
// the multiplication order is (this * vectorQuat) * conjugate(this), not the
// textbook conj(q)*v*q.
// ============================================================================
Vector3* RtMath_Quaternion_Rotate(const Quaternion* self, Vector3* ret, const Vector3* v) {
    RT_DIFF_BEGIN("Math::Quaternion::Rotate(const Vector3&)");
    Quaternion vectorQuat{v->x, v->y, v->z, 0.0f};
    Quaternion conj{-self->x, -self->y, -self->z, self->w};
    Quaternion tmp{}, rotated{};
    RtMath_Quaternion_MulQuaternion(self, &tmp, &vectorQuat);
    RtMath_Quaternion_MulQuaternion(&tmp, &rotated, &conj);
    ret->x = rotated.x; ret->y = rotated.y; ret->z = rotated.z;
    if (rt_diff_on__) {
        Vector3 theirs{};
        typedef void (*OrigFn)(const Quaternion*, Vector3*, const Vector3*);
        RtOriginal<OrigFn>("?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z")(self, &theirs, v);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector3));
    }
    return ret;
}

// ============================================================================
// Math::Transform3::Rotate(const Vector3&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18001e470  ?Rotate@Transform3@Math@@QEBA?AVVector3@2@AEBV32@@Z
// Same as MulVector3 but WITHOUT adding translation (a basis rotate only).
// ============================================================================
Vector3* RtMath_Transform3_Rotate(const Transform3* self, Vector3* ret, const Vector3* v) {
    RT_DIFF_BEGIN("Math::Transform3::Rotate(const Vector3&)");
    float x = v->y * self->axisY.x + v->x * self->axisX.x + v->z * self->axisZ.x;
    float z = v->y * self->axisY.z + v->x * self->axisX.z + v->z * self->axisZ.z;
    float y = v->y * self->axisY.y + v->x * self->axisX.y + v->z * self->axisZ.y;
    ret->x = x; ret->y = y; ret->z = z;
    if (rt_diff_on__) {
        Vector3 theirs{};
        typedef void (*OrigFn)(const Transform3*, Vector3*, const Vector3*);
        RtOriginal<OrigFn>("?Rotate@Transform3@Math@@QEBA?AVVector3@2@AEBV32@@Z")(self, &theirs, v);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector3));
    }
    return ret;
}

// ============================================================================
// Math::Matrix::Identity()  -- static, struct return: RCX=hidden ret
// retail 0x18002c030  ?Identity@Matrix@Math@@SA?AV12@XZ
// ============================================================================
Matrix* RtMath_Matrix_Identity(Matrix* ret) {
    RT_DIFF_BEGIN("Math::Matrix::Identity()");
    float out[16] = {0};
    out[0] = out[5] = out[10] = out[15] = RtMathConst::kOne;
    std::memcpy(ret->m, out, sizeof(out));
    if (rt_diff_on__) {
        Matrix theirs{};
        typedef void (*OrigFn)(Matrix*);
        RtOriginal<OrigFn>("?Identity@Matrix@Math@@SA?AV12@XZ")(&theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Matrix));
    }
    return ret;
}

// ============================================================================
// Math::Transform::Identity()  -- static, struct return: RCX=hidden ret
// retail 0x18001fab0  ?Identity@Transform@Math@@SA?AV12@XZ
// ============================================================================
Transform* RtMath_Transform_Identity(Transform* ret) {
    RT_DIFF_BEGIN("Math::Transform::Identity()");
    ret->rotation.x = 0.0f;
    ret->rotation.y = 0.0f;
    ret->rotation.z = 0.0f;
    ret->rotation.w = RtMathConst::kOne;
    ret->translation.x = 0.0f;
    ret->translation.y = 0.0f;
    ret->translation.z = 0.0f;
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*);
        RtOriginal<OrigFn>("?Identity@Transform@Math@@SA?AV12@XZ")(&theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Math::Transform3::Identity()  -- static, struct return: RCX=hidden ret
// retail 0x1800347f0  ?Identity@Transform3@Math@@SA?AV12@XZ
// ============================================================================
Transform3* RtMath_Transform3_Identity(Transform3* ret) {
    RT_DIFF_BEGIN("Math::Transform3::Identity()");
    ret->axisX = {RtMathConst::kOne, 0.0f, 0.0f};
    ret->axisY = {0.0f, RtMathConst::kOne, 0.0f};
    ret->axisZ = {0.0f, 0.0f, RtMathConst::kOne};
    ret->translation = {0.0f, 0.0f, 0.0f};
    if (rt_diff_on__) {
        Transform3 theirs{};
        typedef void (*OrigFn)(Transform3*);
        RtOriginal<OrigFn>("?Identity@Transform3@Math@@SA?AV12@XZ")(&theirs);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform3));
    }
    return ret;
}

// ============================================================================
// Math::Transform2::FromYawOffset(const Scalar& yawDeg, const Vector2& offset)
// static, struct return: RCX=hidden ret, RDX=yawDeg, R8=offset
// retail 0x1800787f0  ?FromYawOffset@Transform2@Math@@SA?AV12@AEBVScalar@2@AEBVVector2@2@@Z
// Identical construction to the Transform2 ctor above.
// ============================================================================
Transform2* RtMath_Transform2_FromYawOffset(Transform2* ret, const Scalar* yawDeg, const Vector2* offset) {
    RT_DIFF_BEGIN("Math::Transform2::FromYawOffset(const Scalar&, const Vector2&)");
    float rad = yawDeg->v * RtMathConst::kDegToRad;
    float s = RtLibm::Sin(rad);
    float c = RtLibm::Cos(rad);
    ret->rotX.x = c;
    ret->rotX.y = s;
    ret->rotY.x = -s;
    ret->rotY.y = c;
    ret->translation = *offset;
    if (rt_diff_on__) {
        Transform2 theirs{};
        typedef void (*OrigFn)(Transform2*, const Scalar*, const Vector2*);
        RtOriginal<OrigFn>("?FromYawOffset@Transform2@Math@@SA?AV12@AEBVScalar@2@AEBVVector2@2@@Z")(&theirs, yawDeg, offset);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform2));
    }
    return ret;
}

// ============================================================================
// Math::Quaternion::FromYawPitch(const Scalar& yawDeg, const Scalar& pitchDeg)
// static, struct return: RCX=hidden ret, RDX=yawDeg, R8=pitchDeg
// retail 0x18007d710  ?FromYawPitch@Quaternion@Math@@SA?AV12@AEBVScalar@2@0@Z
// Half-angle yaw/pitch quaternion (retail term order transliterated
// verbatim; multiplication is commutative under IEEE754 so an operand-order
// difference from FromYawPitchOffset below cannot diverge).
// ============================================================================
Quaternion* RtMath_Quaternion_FromYawPitch(Quaternion* ret, const Scalar* yawDeg, const Scalar* pitchDeg) {
    RT_DIFF_BEGIN("Math::Quaternion::FromYawPitch(const Scalar&, const Scalar&)");
    float yawHalfRad = yawDeg->v * RtMathConst::kHalf * RtMathConst::kDegToRad;
    float pitchHalfRad = pitchDeg->v * RtMathConst::kHalf * RtMathConst::kDegToRad;
    float sinYaw = RtLibm::Sin(yawHalfRad);
    float sinPitch = RtLibm::Sin(pitchHalfRad);
    float cosYaw = RtLibm::Cos(yawHalfRad);
    float cosPitch = RtLibm::Cos(pitchHalfRad);

    ret->w = cosPitch * cosYaw;
    ret->x = -(cosYaw * sinPitch);
    ret->y = -(sinPitch * sinYaw);
    ret->z = -(cosPitch * sinYaw);
    if (rt_diff_on__) {
        Quaternion theirs{};
        typedef void (*OrigFn)(Quaternion*, const Scalar*, const Scalar*);
        RtOriginal<OrigFn>("?FromYawPitch@Quaternion@Math@@SA?AV12@AEBVScalar@2@0@Z")(&theirs, yawDeg, pitchDeg);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Quaternion));
    }
    return ret;
}

// ============================================================================
// Math::Transform::FromYawPitchOffset(const Scalar& yawDeg, const Scalar& pitchDeg, const Vector3& offset)
// static, struct return: RCX=hidden ret, RDX=yawDeg, R8=pitchDeg, R9=offset
// retail 0x18007da70  ?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z
// Same rotation math as FromYawPitch, plus a copied translation.
// ============================================================================
Transform* RtMath_Transform_FromYawPitchOffset(Transform* ret, const Scalar* yawDeg, const Scalar* pitchDeg,
                                                const Vector3* offset) {
    RT_DIFF_BEGIN("Math::Transform::FromYawPitchOffset(const Scalar&, const Scalar&, const Vector3&)");
    float yawHalfRad = yawDeg->v * RtMathConst::kHalf * RtMathConst::kDegToRad;
    float pitchHalfRad = pitchDeg->v * RtMathConst::kHalf * RtMathConst::kDegToRad;
    float sinYaw = RtLibm::Sin(yawHalfRad);
    float sinPitch = RtLibm::Sin(pitchHalfRad);
    float cosYaw = RtLibm::Cos(yawHalfRad);
    float cosPitch = RtLibm::Cos(pitchHalfRad);

    ret->rotation.x = -(cosYaw * sinPitch);
    ret->rotation.y = -(sinYaw * sinPitch);
    ret->rotation.z = -(cosPitch * sinYaw);
    ret->rotation.w = cosPitch * cosYaw;
    ret->translation = *offset;
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*, const Scalar*, const Scalar*, const Vector3*);
        RtOriginal<OrigFn>("?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z")(&theirs, yawDeg, pitchDeg, offset);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Math::Quaternion::Quaternion(const Vector3& right, const Vector3& forward, const Vector3& up)
// ctor: RCX=this, RDX=right, R8=forward, R9=up
// retail 0x180021b70  ??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z
// NOT in the assigned function list; ported as a dependency of FromDirection
// (below), and genuinely Math::Quaternion surface, so registered too.
// Mike Day / Shepperd's robust matrix->quaternion construction from an
// orthonormal basis {right, forward, up} (right = local X axis of the
// resulting rotation's matrix column 0, etc — see AsMatrix's column layout).
// ============================================================================
Quaternion* RtMath_Quaternion_CtorBasis(Quaternion* self, const Vector3* right, const Vector3* forward, const Vector3* up) {
    RT_DIFF_BEGIN("Math::Quaternion::Quaternion(const Vector3&, const Vector3&, const Vector3&)");
    float sumX = (right->x + RtMathConst::kOne - forward->y) - up->z;
    if (sumX < 0.0f) sumX = 0.0f;
    float x = RtLibm::Sqrt(sumX) * RtMathConst::kHalf;

    float sumY = ((RtMathConst::kOne - right->x) + forward->y) - up->z;
    if (sumY < 0.0f) sumY = 0.0f;
    float y = RtLibm::Sqrt(sumY) * RtMathConst::kHalf;

    float sumZ = ((RtMathConst::kOne - right->x) - forward->y) + up->z;
    if (sumZ < 0.0f) sumZ = 0.0f;
    float z = RtLibm::Sqrt(sumZ) * RtMathConst::kHalf;

    float sumW = 0.0f;
    float trace = (right->x + RtMathConst::kOne) + forward->y + up->z;
    if (0.0f <= trace) sumW = trace;
    float w = RtLibm::Sqrt(sumW) * RtMathConst::kHalf;

    self->x = std::copysignf(x, up->y - forward->z);
    self->y = std::copysignf(y, right->z - up->x);
    self->z = std::copysignf(z, forward->x - right->y);
    self->w = w;
    if (rt_diff_on__) {
        Quaternion theirs{};
        typedef void (*OrigFn)(Quaternion*, const Vector3*, const Vector3*, const Vector3*);
        RtOriginal<OrigFn>("??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z")(&theirs, right, forward, up);
        RT_DIFF_BYTES(self, &theirs, sizeof(Quaternion));
    }
    return self;
}

// ============================================================================
// Math::Transform::FromDirection(const Vector3& direction, const Vector3& offset)
// static, struct return: RCX=hidden ret, RDX=direction, R8=offset
// retail 0x180021d70  ?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z
// NOT in the assigned function list; ported as a dependency of FromLookAt
// (below), and genuinely Math::Transform surface, so registered too.
// Builds a right-handed look basis (right = dir x refUp, up = dir x right),
// picking refUp = world -Y or +Y instead of +Z when `direction` is nearly
// parallel to world Z (else refUp = world +Z), to avoid a degenerate cross
// product. `direction` MUST already be a unit vector — retail does not
// re-normalize it here (FromLookAt normalizes before calling in).
// ============================================================================
Transform* RtMath_Transform_FromDirection(Transform* ret, const Vector3* direction, const Vector3* offset) {
    RT_DIFF_BEGIN("Math::Transform::FromDirection(const Vector3&, const Vector3&)");
    float dz = direction->z; // dot(direction, world +Z)

    float refY, refZ;
    if (BitsToFloat(kNearParallelPosBits) < dz) {
        refY = RtMathConst::kMinusOne;
        refZ = 0.0f;
    } else if (dz < BitsToFloat(kNearParallelNegBits)) {
        refY = RtMathConst::kOne;
        refZ = 0.0f;
    } else {
        refY = 0.0f;
        refZ = RtMathConst::kOne;
    }
    // refVec = (0, refY, refZ)

    float rightX = direction->y * refZ - direction->z * refY;
    float rightY = direction->z * 0.0f - direction->x * refZ;
    float rightZ = direction->x * refY - direction->y * 0.0f;
    float rightLen = RtLibm::Sqrt(rightY * rightY + rightX * rightX + rightZ * rightZ);
    if (rightLen != 0.0f) {
        float inv = RtMathConst::kOne / rightLen;
        rightX *= inv; rightY *= inv; rightZ *= inv;
    }

    float upX = direction->z * rightY - direction->y * rightZ;
    float upY = direction->x * rightZ - direction->z * rightX;
    float upZ = direction->y * rightX - direction->x * rightY;
    // Term order is load-bearing: retail sums upY^2 + upX^2 + upZ^2 (the
    // decompilation at 0x180021d70 adds them in that order), and float addition
    // is not associative. Summing upZ^2 first instead diverged from the DLL on
    // 7 of 980 calls by ~20 ULP in the resulting quaternion — caught by the
    // Wave 1 diff oracle, and NOT attributable to libm: it persisted with
    // BANISHED_RT_LIBM=dll, which is what separated it from the 1-ULP sin/cos
    // noise at the other three divergent sites.
    float upLen = RtLibm::Sqrt(upY * upY + upX * upX + upZ * upZ);
    if (upLen != 0.0f) {
        float inv = RtMathConst::kOne / upLen;
        upX *= inv; upY *= inv; upZ *= inv;
    }

    Vector3 right{rightX, rightY, rightZ};
    Vector3 up{upX, upY, upZ};
    RtMath_Quaternion_CtorBasis(&ret->rotation, &right, direction, &up);
    ret->translation = *offset;
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*, const Vector3*, const Vector3*);
        RtOriginal<OrigFn>("?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z")(&theirs, direction, offset);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Math::Transform::FromLookAt(const Vector3& from, const Vector3& to)
// static, struct return: RCX=hidden ret, RDX=from, R8=to
// retail 0x18007dbb0  ?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z
// direction = normalize(to - from); FromDirection(direction, from).
// ============================================================================
Transform* RtMath_Transform_FromLookAt(Transform* ret, const Vector3* from, const Vector3* to) {
    RT_DIFF_BEGIN("Math::Transform::FromLookAt(const Vector3&, const Vector3&)");
    float dx = to->x - from->x;
    float dy = to->y - from->y;
    float dz = to->z - from->z;
    float len = RtLibm::Sqrt(dy * dy + dx * dx + dz * dz);
    Vector3 dir{dx, dy, dz};
    if (len != 0.0f) {
        float inv = RtMathConst::kOne / len;
        dir.x = inv * dx; dir.y = inv * dy; dir.z = inv * dz;
    }
    RtMath_Transform_FromDirection(ret, &dir, from);
    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(Transform*, const Vector3*, const Vector3*);
        RtOriginal<OrigFn>("?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z")(&theirs, from, to);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Math::Transform::Serialize(IO::Serializer&)
// member, void return: RCX=this, RDX=serializer
// retail 0x180059b90  ?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z
// Two calls through the encoding's vtable slot +0x28 — an undocumented
// "Serialize(int count, float* data)" bulk-float-array forwarder (see
// engine/types/io_serializer.h's enc_vtbl table, which only documents +0x48
// and up; this slot is local to this port and not added to that shared
// header). count=4 serializes the Quaternion (this+0x00), count=3 the
// translation Vector3 (this+0x10).
//
// MUTATES the IO stream — NOT diffable via rt_diff.h (running "ours" and
// "theirs" would both try to read/write the same stream position). No
// RT_DIFF wrapper here; correctness is exercised by save/load parity in
// sa-run per PLAN.md §6 step 6, not by the differential oracle.
// ============================================================================
namespace {
constexpr int kEncSlotSerializeFloatArr = 0x28;
typedef void (*SerializeFloatArrFn)(void* encoding, int count, void* data);

inline void* EncodingVtableSlot(void* encoding, int byteOffset) {
    void** vtbl = *reinterpret_cast<void***>(encoding);
    return *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtbl) + byteOffset);
}
} // namespace

void RtMath_Transform_Serialize(Transform* self, void* serializer /* io::Serializer* */) {
    // io::Serializer { Stream* m_stream; Encoding* m_encoding; }  (engine/types/io_serializer.h)
    void* encoding = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(serializer) + 8);
    auto fn = reinterpret_cast<SerializeFloatArrFn>(EncodingVtableSlot(encoding, kEncSlotSerializeFloatArr));
    fn(encoding, 4, &self->rotation);
    fn(encoding, 3, &self->translation);
}

// ============================================================================
// Graphics::InterpolatedState<Math::Transform>::Lerp(const Scalar&) const
// retail 0x18001aff0
// ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z
//
// NOT PORTED — skipped, per instructions, rather than guessed.
// The call is `Math::Transform::Lerp(this+0x1c /*self*/, retval, this /*t*/)`:
// only ONE Transform operand (at +0x1c) plus a bare Scalar (at +0x00) reach
// Transform::Lerp, with no second Transform argument visible at this call
// site. That means either (a) Transform::Lerp is a 1-argument blend against
// state implicit in InterpolatedState<Transform> beyond the +0x1c field, or
// (b) the "previous" value lives at some OTHER offset this call site does not
// reveal. Neither Transform::Lerp's own address nor InterpolatedState<T>'s
// layout is pinned by any decompiled ctor in this wave's function list, and
// guessing the class shape here is exactly the kind of wrong-guess PLAN.md
// §4.3 risk 4 warns against for the unnamed pool. Left for a wave that also
// covers Graphics::InterpolatedState<T> (needs its ctor and at least one
// other instantiation to cross-check the layout) and Math::Transform::Lerp
// itself (retail address not yet found — not among this file's exports).
// ============================================================================

// ============================================================================
// Math::Transform::operator*(const Vector3&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=v
// retail 0x18001e200  ??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z
//
// Transcribed from the DISASSEMBLY, not the decompilation: Ghidra drops the
// third argument of the inner Quaternion::Rotate call (it cannot track R8
// across a hidden-return-slot call), which makes the decompiled body look as
// though Rotate takes no vector at all. The disassembly shows R8 arriving
// untouched from our own caller, so the rotated vector is `v`.
//
//     ret = this->rotation.Rotate(v) + this->translation
// ============================================================================
Vector3* RtMath_Transform_MulVector3(const Transform* self, Vector3* ret, const Vector3* v) {
    RT_DIFF_BEGIN("Math::Transform::operator*(const Vector3&)");
    // Snapshot the inputs BEFORE computing, because `ret` may alias `v` or
    // `self` — `vec = transform * vec;` is an ordinary thing for a caller to
    // write, and it is what most of our call sites actually do. Retail is safe
    // under that aliasing because it consumes v inside Rotate before storing
    // anything; so are we. The DIFF is not: it runs the original AFTER ours has
    // written ret, so without a snapshot the original is handed an input we
    // just overwrote and "diverges" on almost every call. That is a harness
    // artifact, and it looked exactly like a real port bug until the constant
    // "theirs" value gave it away.
    Vector3   vIn;
    Transform selfIn;
    if (rt_diff_on__) { vIn = *v; selfIn = *self; }

    Vector3 rotated;
    RtMath_Quaternion_Rotate(&self->rotation, &rotated, v);
    // Retail's operand order per component (0x18001e21a..0x18001e248): y and z
    // add translation + rotated, x adds rotated + translation. Float addition
    // is commutative, so this is bit-identical either way; kept as retail wrote
    // it so a future reader diffing against the listing sees the same shape.
    ret->y = self->translation.y + rotated.y;
    ret->x = rotated.x + self->translation.x;
    ret->z = self->translation.z + rotated.z;
    if (rt_diff_on__) {
        Vector3 theirs{};
        typedef void (*OrigFn)(const Transform*, Vector3*, const Vector3*);
        RtOriginal<OrigFn>("??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z")(&selfIn, &theirs, &vIn);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Vector3));
    }
    return ret;
}

// ============================================================================
// Math::Transform::operator*(const Transform&) const
// member, struct return: RCX=this, RDX=hidden ret, R8=other
// retail 0x18001e3c0  ??DTransform@Math@@QEBA?AV01@AEBV01@@Z
//
// Also transcribed from the disassembly. Note which operand is which — the
// decompilation attributes both inner calls to `in_R8` and reads as though
// `this` is unused, which is wrong. The listing at 0x18001e3e9/0x18001e402
// shows OTHER supplying the rotation on both calls and THIS supplying the
// operands:
//
//     ret.translation = other.rotation.Rotate(this->translation) + other.translation
//     ret.rotation    = other.rotation * this->rotation
//
// Retail also stamps `ret` with the identity Transform on entry (0x18001e3d1)
// before overwriting every field of it. That store is dead in every ordinary
// call, but it is reproduced here: it is observable if either inner call ever
// fails to write, and this port is byte-faithful first.
// ============================================================================
Transform* RtMath_Transform_MulTransform(const Transform* self, Transform* ret, const Transform* other) {
    RT_DIFF_BEGIN("Math::Transform::operator*(const Transform&)");
    // Same aliasing snapshot as operator*(Vector3) above: `ret` may be `self`
    // or `other`, and the diff must hand the original the ORIGINAL inputs.
    Transform selfIn, otherIn;
    if (rt_diff_on__) { selfIn = *self; otherIn = *other; }

    ret->rotation.x = 0.0f;
    ret->rotation.y = 0.0f;
    ret->rotation.z = 0.0f;
    ret->rotation.w = RtMathConst::kOne;
    ret->translation.x = 0.0f;
    ret->translation.y = 0.0f;
    ret->translation.z = 0.0f;

    Vector3 rotated;
    RtMath_Quaternion_Rotate(&other->rotation, &rotated, &self->translation);
    ret->translation.x = rotated.x + other->translation.x;
    ret->translation.y = other->translation.y + rotated.y;
    ret->translation.z = other->translation.z + rotated.z;

    Quaternion combined;
    RtMath_Quaternion_MulQuaternion(&other->rotation, &combined, &self->rotation);
    ret->rotation = combined;

    if (rt_diff_on__) {
        Transform theirs{};
        typedef void (*OrigFn)(const Transform*, Transform*, const Transform*);
        RtOriginal<OrigFn>("??DTransform@Math@@QEBA?AV01@AEBV01@@Z")(&selfIn, &theirs, &otherIn);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// Registration
// ============================================================================
void RtMathTransformRegister() {
    int count = 0;

    RegisterRuntimeExport("??DTransform@Math@@QEBA?AVVector3@1@AEBV21@@Z", (void*)&RtMath_Transform_MulVector3); ++count;
    RegisterRuntimeExport("??DTransform@Math@@QEBA?AV01@AEBV01@@Z", (void*)&RtMath_Transform_MulTransform); ++count;

    RegisterRuntimeExport("??0Quaternion@Math@@QEAA@MMMM@Z", (void*)&RtMath_Quaternion_Ctor4); ++count;
    RegisterRuntimeExport("??0Transform@Math@@QEAA@XZ", (void*)&RtMath_Transform_CtorDefault); ++count;
    RegisterRuntimeExport("??0Transform@Math@@QEAA@AEBV01@@Z", (void*)&RtMath_Transform_CtorCopy); ++count;
    RegisterRuntimeExport("??4Transform@Math@@QEAAAEAV01@AEBV01@@Z", (void*)&RtMath_Transform_Assign); ++count;
    RegisterRuntimeExport("??0Transform3@Math@@QEAA@AEBV01@@Z", (void*)&RtMath_Transform3_CtorCopy); ++count;
    RegisterRuntimeExport("??0Transform2@Math@@QEAA@AEBVScalar@1@AEBVVector2@1@@Z", (void*)&RtMath_Transform2_Ctor); ++count;
    RegisterRuntimeExport("??DMatrix@Math@@QEBA?AV01@AEBV01@@Z", (void*)&RtMath_Matrix_MulMatrix); ++count;
    RegisterRuntimeExport("??DMatrix@Math@@QEBA?AVVector4@1@AEBV21@@Z", (void*)&RtMath_Matrix_MulVector4); ++count;
    RegisterRuntimeExport("??DTransform2@Math@@QEBA?AV01@AEBV01@@Z", (void*)&RtMath_Transform2_MulTransform2); ++count;
    RegisterRuntimeExport("??DTransform2@Math@@QEBA?AVVector2@1@AEBV21@@Z", (void*)&RtMath_Transform2_MulVector2); ++count;
    RegisterRuntimeExport("??DTransform3@Math@@QEBA?AVVector3@1@AEBV21@@Z", (void*)&RtMath_Transform3_MulVector3); ++count;
    RegisterRuntimeExport("?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ", (void*)&RtMath_Transform_AsMatrix); ++count;
    RegisterRuntimeExport("?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ", (void*)&RtMath_Transform3_AsMatrix); ++count;
    RegisterRuntimeExport("?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z", (void*)&RtMath_Transform_FromLookAt); ++count;
    RegisterRuntimeExport("?FromYawOffset@Transform2@Math@@SA?AV12@AEBVScalar@2@AEBVVector2@2@@Z", (void*)&RtMath_Transform2_FromYawOffset); ++count;
    RegisterRuntimeExport("?FromYawPitch@Quaternion@Math@@SA?AV12@AEBVScalar@2@0@Z", (void*)&RtMath_Quaternion_FromYawPitch); ++count;
    RegisterRuntimeExport("?FromYawPitchOffset@Transform@Math@@SA?AV12@AEBVScalar@2@0AEBVVector3@2@@Z", (void*)&RtMath_Transform_FromYawPitchOffset); ++count;
    RegisterRuntimeExport("?Identity@Matrix@Math@@SA?AV12@XZ", (void*)&RtMath_Matrix_Identity); ++count;
    RegisterRuntimeExport("?Identity@Transform@Math@@SA?AV12@XZ", (void*)&RtMath_Transform_Identity); ++count;
    RegisterRuntimeExport("?Identity@Transform3@Math@@SA?AV12@XZ", (void*)&RtMath_Transform3_Identity); ++count;
    RegisterRuntimeExport("?Inverse@Transform@Math@@QEBA?AV12@XZ", (void*)&RtMath_Transform_Inverse); ++count;
    RegisterRuntimeExport("?Inverse@Transform3@Math@@QEBA?AV12@XZ", (void*)&RtMath_Transform3_Inverse); ++count;
    RegisterRuntimeExport("?Rotate@Quaternion@Math@@QEBA?AVVector3@2@AEBV32@@Z", (void*)&RtMath_Quaternion_Rotate); ++count;
    RegisterRuntimeExport("?Rotate@Transform3@Math@@QEBA?AVVector3@2@AEBV32@@Z", (void*)&RtMath_Transform3_Rotate); ++count;
    RegisterRuntimeExport("?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z", (void*)&RtMath_Transform_Serialize); ++count;

    // Bonus: dependencies of the above that are genuine Math:: exports in
    // their own right (not on the assigned list, but required internally by
    // Rotate/FromLookAt — see the comments on each definition).
    RegisterRuntimeExport("??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z", (void*)&RtMath_Quaternion_MulQuaternion); ++count;
    RegisterRuntimeExport("??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z", (void*)&RtMath_Quaternion_CtorBasis); ++count;
    RegisterRuntimeExport("?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z", (void*)&RtMath_Transform_FromDirection); ++count;

    fprintf(stderr, "[RtMathTransform] registered %d functions (Matrix/Quaternion/Transform/Transform2/Transform3)\n", count);
}
