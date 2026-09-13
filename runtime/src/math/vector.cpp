// ============================================================================
// runtime/src/math/vector.cpp — Math::Scalar / Vector2 / Vector3 / Vector4
// ============================================================================
//
// Wave 1, Lane 1 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Every function below is a byte-faithful port of the decompilation at the
// retail address named in its comment (Runtime-x64.dll, image base
// 0x180000000, decompiled 2026-09-06). See runtime/types/math/Math.h for the
// governing ABI note: every one of these classes has a user-declared ctor, so
// MSVC returns it BY MEMORY (hidden return-slot pointer, RAX = that pointer
// on return).
//
// FUN_180128110 (sqrt) and FUN_180126680/FUN_180127d90/FUN_180126d10
// (atan2/sinf/cosf) were decompiled and confirmed to be plain CRT math
// wrapped in matherr-style domain-error handling; for every finite,
// non-exceptional input actually reached by these callers they reduce to the
// hardware sqrtss/plain libm call. Transcendentals go through the RtLibm seam
// (runtime/funcs/rt_libm.h) rather than calling std:: or the DLL directly:
// our own libm by default (so the port survives Runtime-x64.dll's eventual
// removal, PLAN.md §4.1 decision 1), with the DLL's own CRT available as a
// diagnostic to tell a real bug from ordinary last-bit libm rounding.
// ============================================================================

#include <cmath>
#include "../../types/math/Math.h"
#include "../../funcs/rt_port.h"
#include "../../funcs/rt_libm.h"
#include "vector.h"

namespace {
// Deg->rad already lives in RtMathConst; rad->deg is only needed here.
// Pinned by Vector3::Heading (0x18007c900): _DAT_180161708 = 0x42652ee1.
constexpr float kRadToDeg = 57.29578f;  // 0x180161708
}  // namespace

// ============================================================
// Math::Scalar
// ============================================================

// ?Abs@Scalar@Math@@QEBA?AV12@XZ  retail 0x1800646b0
Scalar* RtMath_Scalar_Abs(const Scalar* self, Scalar* ret) {
    ret->v = std::fabs(self->v);
    return ret;
}

// ?Lerp@Scalar@Math@@QEAA?AV12@AEBV12@0@Z  retail 0x1800218c0
// Non-const (QEAA) but reads only `this`; t is the 3rd arg (R9).
Scalar* RtMath_Scalar_Lerp(const Scalar* self, Scalar* ret, const Scalar* to, const Scalar* t) {
    ret->v = self->v + (to->v - self->v) * t->v;
    return ret;
}

// ?Max@Scalar@Math@@QEBA?AV12@AEBV12@@Z  retail 0x18005ff60
Scalar* RtMath_Scalar_Max(const Scalar* self, Scalar* ret, const Scalar* other) {
    ret->v = (other->v < self->v) ? self->v : other->v;
    return ret;
}

// ============================================================
// Math::Vector2
// ============================================================

// ??0Vector2@Math@@QEAA@XZ  retail 0x180060030 (default ctor, RAX = this, not diffed: a ctor
// has no independent "original" result to compare — it just zeroes memory).
Vector2* RtMath_Vector2_Ctor(Vector2* self) {
    self->x = 0.0f;
    self->y = 0.0f;
    return self;
}

// ??0Vector2@Math@@QEAA@AEBVScalar@1@0@Z  retail 0x180070db0
Vector2* RtMath_Vector2_Ctor_ss(Vector2* self, const Scalar* x, const Scalar* y) {
    self->x = x->v;
    self->y = y->v;
    return self;
}

// ??XVector2@Math@@QEAAXAEBV01@@Z  retail 0x18005ffd0  (operator*=, void return, mutates self
// in place — not diffed here since diffing would require running the original on live state
// twice; the formula is a two-multiply and trivially verified by inspection).
void RtMath_Vector2_MulAssign(Vector2* self, const Vector2* rhs) {
    self->x *= rhs->x;
    self->y *= rhs->y;
}

// ?Dot@Vector2@Math@@QEBA?AVScalar@2@AEBV12@@Z  retail 0x18006c840
Scalar* RtMath_Vector2_Dot(const Vector2* self, Scalar* ret, const Vector2* rhs) {
    ret->v = self->x * rhs->x + self->y * rhs->y;
    return ret;
}

// ?Max@Vector2@Math@@QEBA?AV12@AEBV12@@Z  retail 0x180060330
Vector2* RtMath_Vector2_Max(const Vector2* self, Vector2* ret, const Vector2* rhs) {
    ret->x = (self->x <= rhs->x) ? rhs->x : self->x;
    ret->y = (self->y <= rhs->y) ? rhs->y : self->y;
    return ret;
}

// ?Min@Vector2@Math@@QEBA?AV12@AEBV12@@Z  retail 0x1800602e0
Vector2* RtMath_Vector2_Min(const Vector2* self, Vector2* ret, const Vector2* rhs) {
    ret->x = (rhs->x <= self->x) ? rhs->x : self->x;
    ret->y = (rhs->y <= self->y) ? rhs->y : self->y;
    return ret;
}

// ?Length@Vector2@Math@@QEBA?AVScalar@2@XZ  retail 0x18006c880
Scalar* RtMath_Vector2_Length(const Vector2* self, Scalar* ret) {
    ret->v = RtLibm::Sqrt(self->x * self->x + self->y * self->y);
    return ret;
}

// ?LengthSquared@Vector2@Math@@QEBA?AVScalar@2@XZ  retail 0x18006c860
Scalar* RtMath_Vector2_LengthSquared(const Vector2* self, Scalar* ret) {
    ret->v = self->x * self->x + self->y * self->y;
    return ret;
}

// ?Normalize@Vector2@Math@@QEBA?AV12@XZ  retail 0x18006c600  (by value; self unchanged)
Vector2* RtMath_Vector2_Normalize(const Vector2* self, Vector2* ret) {
    float len = RtLibm::Sqrt(self->x * self->x + self->y * self->y);
    if (len == 0.0f) {
        *ret = *self;
    } else {
        float inv = RtMathConst::kOne / len;
        ret->x = self->x * inv;
        ret->y = self->y * inv;
    }
    return ret;
}

// ?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z  retail 0x18006c7b0
// Same as above but also writes the pre-normalize length out through `mag`.
Vector2* RtMath_Vector2_NormalizeWithMag(const Vector2* self, Vector2* ret, Scalar* mag) {
    float len = RtLibm::Sqrt(self->x * self->x + self->y * self->y);
    mag->v = len;
    if (len == 0.0f) {
        *ret = *self;
    } else {
        float inv = RtMathConst::kOne / len;
        ret->x = self->x * inv;
        ret->y = self->y * inv;
    }
    return ret;
}

// ?Serialize@Vector2@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x18005ff40
// NOT diffable: mutates the live IO::Serializer stream (reads or writes bytes
// from/into it). Ported byte-faithfully as a raw vtable call because
// IO::Serializer itself is Wave 4 and not yet a typed struct here:
// `*(this+8)` is the Serializer's Stream*, and slot +0x28 on the Stream's
// vtable is "serialize N floats starting at this pointer" (retail passes the
// float count as arg 2: 2/3/4 for Vector2/3/4).
void RtMath_Vector2_Serialize(Vector2* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeFloatsFn = void(*)(void*, int, void*);
    SerializeFloatsFn fn = *reinterpret_cast<SerializeFloatsFn*>(reinterpret_cast<char*>(vtbl) + 0x28);
    fn(stream, 2, self);
}

// ============================================================
// Math::Vector3
// ============================================================

// ??0Vector3@Math@@QEAA@XZ  retail 0x180015070
Vector3* RtMath_Vector3_Ctor(Vector3* self) {
    self->x = 0.0f;
    self->y = 0.0f;
    self->z = 0.0f;
    return self;
}

// ??0Vector3@Math@@QEAA@MMM@Z  retail 0x1800152d0  (three plain floats)
Vector3* RtMath_Vector3_Ctor_fff(Vector3* self, float x, float y, float z) {
    self->x = x;
    self->y = y;
    self->z = z;
    return self;
}

// ??0Vector3@Math@@QEAA@AEBVScalar@1@00@Z  retail 0x180015120
Vector3* RtMath_Vector3_Ctor_sss(Vector3* self, const Scalar* x, const Scalar* y, const Scalar* z) {
    self->x = x->v;
    self->y = y->v;
    self->z = z->v;
    return self;
}

// ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z  retail 0x180015360  (splat one Scalar to x/y/z)
Vector3* RtMath_Vector3_Ctor_s(Vector3* self, const Scalar* s) {
    self->x = s->v;
    self->y = s->v;
    self->z = s->v;
    return self;
}

// ?Cross@Vector3@Math@@QEBA?AV12@AEBV12@@Z  retail 0x180021b10
Vector3* RtMath_Vector3_Cross(const Vector3* self, Vector3* ret, const Vector3* rhs) {
    float x = self->y * rhs->z - self->z * rhs->y;
    float y = rhs->x * self->z - self->x * rhs->z;
    float z = self->x * rhs->y - rhs->x * self->y;
    ret->x = x;
    ret->y = y;
    ret->z = z;
    return ret;
}

// ?Dot@Vector3@Math@@QEBA?AVScalar@2@AEBV12@@Z  retail 0x18001dff0
Scalar* RtMath_Vector3_Dot(const Vector3* self, Scalar* ret, const Vector3* rhs) {
    ret->v = rhs->y * self->y + rhs->x * self->x + rhs->z * self->z;
    return ret;
}

// ?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z  retail 0x1800220b0 (static)
// x = -cos(pitch_rad) * sin(yaw_rad); y = cos(pitch_rad) * cos(yaw_rad); z = sin(pitch_rad)
Vector3* RtMath_Vector3_FromYawPitch(Vector3* ret, const Scalar* yaw, const Scalar* pitch) {
    float yawRad = yaw->v * RtMathConst::kDegToRad;
    float pitchRad = pitch->v * RtMathConst::kDegToRad;
    float sinYaw = RtLibm::Sin(yawRad);
    float sinPitch = RtLibm::Sin(pitchRad);
    float cosYaw = RtLibm::Cos(yawRad);
    float cosPitch = RtLibm::Cos(pitchRad);
    ret->z = sinPitch;
    ret->x = -cosPitch * sinYaw;
    ret->y = cosPitch * cosYaw;
    return ret;
}

// ?Heading@Vector3@Math@@QEBAMXZ  retail 0x18007c900  (returns a plain float in XMM0)
// FUN_180126680 was decompiled and is the CRT double atan2() (its own
// domain-error fallback even names it "atan2f"); RtLibm::Atan2 reproduces it.
float RtMath_Vector3_Heading(const Vector3* self) {
    float ours = RtLibm::Atan2(-self->x, self->y) * kRadToDeg;
    return ours;
}

// ?Length@Vector3@Math@@QEBA?AVScalar@2@XZ  retail 0x1800219a0
Scalar* RtMath_Vector3_Length(const Vector3* self, Scalar* ret) {
    ret->v = RtLibm::Sqrt(self->x * self->x + self->y * self->y + self->z * self->z);
    return ret;
}

// ?Lerp@Vector3@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z  retail 0x18001d450
Vector3* RtMath_Vector3_Lerp(const Vector3* self, Vector3* ret, const Vector3* to, const Scalar* t) {
    float tt = t->v;
    ret->x = self->x + tt * (to->x - self->x);
    ret->y = self->y + (to->y - self->y) * tt;
    ret->z = self->z + (to->z - self->z) * tt;
    return ret;
}

// ?Min@Vector3@Math@@QEBA?AV12@AEBV12@@Z  retail 0x180015280
Vector3* RtMath_Vector3_Min(const Vector3* self, Vector3* ret, const Vector3* rhs) {
    ret->x = (rhs->x <= self->x) ? rhs->x : self->x;
    ret->y = (rhs->y <= self->y) ? rhs->y : self->y;
    ret->z = (rhs->z <= self->z) ? rhs->z : self->z;
    return ret;
}

// ?Normalize@Vector3@Math@@QEBA?AV12@AEAVScalar@2@@Z  retail 0x180021a20
// Takes an explicit Scalar& out-param for the pre-normalize magnitude.
Vector3* RtMath_Vector3_NormalizeWithMag(const Vector3* self, Vector3* ret, Scalar* mag) {
    float len = RtLibm::Sqrt(self->x * self->x + self->y * self->y + self->z * self->z);
    mag->v = len;
    if (len != 0.0f) {
        float inv = RtMathConst::kOne / len;
        ret->x = self->x * inv;
        ret->y = self->y * inv;
        ret->z = self->z * inv;
    } else {
        ret->x = self->x;
        ret->y = self->y;
        ret->z = self->z;
    }
    return ret;
}

// ?Normalize@Vector3@Math@@QEBA?AV12@XZ  retail 0x180021ae0
// Retail is a thin wrapper: allocates a throwaway Scalar on the stack and
// calls the Scalar&-out overload above, discarding the magnitude. Ported the
// same way (no independent diff site — it is exactly NormalizeWithMag's diff,
// twice, so a bare forwarding call keeps the two implementations in sync).
Vector3* RtMath_Vector3_Normalize(const Vector3* self, Vector3* ret) {
    Scalar scratchMag{};
    return RtMath_Vector3_NormalizeWithMag(self, ret, &scratchMag);
}

// ??8Vector3@Math@@QEBA_NAEBV01@@Z  retail 0x1800150a0  (operator==, bool in AL, no hidden slot)
bool RtMath_Vector3_Equals(const Vector3* self, const Vector3* rhs) {
    bool ours = self->x == rhs->x && self->y == rhs->y && self->z == rhs->z;
    return ours;
}

// ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800150e0 — see the Vector2
// Serialize comment above; NOT diffable, same vtable-slot pattern, count = 3.
void RtMath_Vector3_Serialize(Vector3* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeFloatsFn = void(*)(void*, int, void*);
    SerializeFloatsFn fn = *reinterpret_cast<SerializeFloatsFn*>(reinterpret_cast<char*>(vtbl) + 0x28);
    fn(stream, 3, self);
}

// ============================================================
// Math::Vector4
// ============================================================

// ??0Vector4@Math@@QEAA@AEBVScalar@1@@Z  retail 0x18001d750  (splat one Scalar to x/y/z/w)
Vector4* RtMath_Vector4_Ctor_s(Vector4* self, const Scalar* s) {
    self->x = s->v;
    self->y = s->v;
    self->z = s->v;
    self->w = s->v;
    return self;
}

// ??0Vector4@Math@@QEAA@AEBVVector3@1@AEBVScalar@1@@Z  retail 0x18001e070
Vector4* RtMath_Vector4_Ctor_v3s(Vector4* self, const Vector3* xyz, const Scalar* w) {
    self->x = xyz->x;
    self->y = xyz->y;
    self->z = xyz->z;
    self->w = w->v;
    return self;
}

// ?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x18001d360 — see the Vector2
// Serialize comment above; NOT diffable, same vtable-slot pattern, count = 4.
void RtMath_Vector4_Serialize(Vector4* self, void* serializer) {
    void* stream = *reinterpret_cast<void**>(reinterpret_cast<char*>(serializer) + 8);
    void* vtbl = *reinterpret_cast<void**>(stream);
    using SerializeFloatsFn = void(*)(void*, int, void*);
    SerializeFloatsFn fn = *reinterpret_cast<SerializeFloatsFn*>(reinterpret_cast<char*>(vtbl) + 0x28);
    fn(stream, 4, self);
}
