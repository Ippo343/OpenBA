#pragma once
// ============================================================================
// runtime/src/math/vector.h — declarations for runtime/src/math/vector.cpp
// ============================================================================
// See vector.cpp for the retail address / mangled name of each function and
// the ABI note in runtime/types/math/Math.h.
// ============================================================================

#include "../../types/math/Math.h"

// ---- Math::Scalar -----------------------------------------------------
Scalar* RtMath_Scalar_Abs(const Scalar* self, Scalar* ret);
Scalar* RtMath_Scalar_Lerp(const Scalar* self, Scalar* ret, const Scalar* to, const Scalar* t);
Scalar* RtMath_Scalar_Max(const Scalar* self, Scalar* ret, const Scalar* other);

// ---- Math::Vector2 -----------------------------------------------------
Vector2* RtMath_Vector2_Ctor(Vector2* self);
Vector2* RtMath_Vector2_Ctor_ss(Vector2* self, const Scalar* x, const Scalar* y);
void     RtMath_Vector2_MulAssign(Vector2* self, const Vector2* rhs);
Scalar*  RtMath_Vector2_Dot(const Vector2* self, Scalar* ret, const Vector2* rhs);
Vector2* RtMath_Vector2_Max(const Vector2* self, Vector2* ret, const Vector2* rhs);
Vector2* RtMath_Vector2_Min(const Vector2* self, Vector2* ret, const Vector2* rhs);
Scalar*  RtMath_Vector2_Length(const Vector2* self, Scalar* ret);
Scalar*  RtMath_Vector2_LengthSquared(const Vector2* self, Scalar* ret);
Vector2* RtMath_Vector2_Normalize(const Vector2* self, Vector2* ret);
Vector2* RtMath_Vector2_NormalizeWithMag(const Vector2* self, Vector2* ret, Scalar* mag);
void     RtMath_Vector2_Serialize(Vector2* self, void* serializer);

// ---- Math::Vector3 -----------------------------------------------------
Vector3* RtMath_Vector3_Ctor(Vector3* self);
Vector3* RtMath_Vector3_Ctor_fff(Vector3* self, float x, float y, float z);
Vector3* RtMath_Vector3_Ctor_sss(Vector3* self, const Scalar* x, const Scalar* y, const Scalar* z);
Vector3* RtMath_Vector3_Ctor_s(Vector3* self, const Scalar* s);
Vector3* RtMath_Vector3_Cross(const Vector3* self, Vector3* ret, const Vector3* rhs);
Scalar*  RtMath_Vector3_Dot(const Vector3* self, Scalar* ret, const Vector3* rhs);
Vector3* RtMath_Vector3_FromYawPitch(Vector3* ret, const Scalar* yaw, const Scalar* pitch);
float    RtMath_Vector3_Heading(const Vector3* self);
Scalar*  RtMath_Vector3_Length(const Vector3* self, Scalar* ret);
Vector3* RtMath_Vector3_Lerp(const Vector3* self, Vector3* ret, const Vector3* to, const Scalar* t);
Vector3* RtMath_Vector3_Min(const Vector3* self, Vector3* ret, const Vector3* rhs);
Vector3* RtMath_Vector3_NormalizeWithMag(const Vector3* self, Vector3* ret, Scalar* mag);
Vector3* RtMath_Vector3_Normalize(const Vector3* self, Vector3* ret);
bool     RtMath_Vector3_Equals(const Vector3* self, const Vector3* rhs);
void     RtMath_Vector3_Serialize(Vector3* self, void* serializer);

// ---- Math::Vector4 -----------------------------------------------------
Vector4* RtMath_Vector4_Ctor_s(Vector4* self, const Scalar* s);
Vector4* RtMath_Vector4_Ctor_v3s(Vector4* self, const Vector3* xyz, const Scalar* w);
void     RtMath_Vector4_Serialize(Vector4* self, void* serializer);
