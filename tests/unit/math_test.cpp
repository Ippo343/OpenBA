// ============================================================================
// tests/unit/math_test.cpp — the Math value classes standing on their own
// ============================================================================
//
// ENDGAME B.3.1 step 1, MATH-CORE batch (B31_RULES rule 6): every public method
// of Math::Scalar / Vector2 / Vector3 / Vector4 / Quaternion / Matrix /
// Transform / Transform2 / Transform3 gets a numeric case here, including the
// edge cases the ported bodies' comments call out:
//
//   * normalising a ZERO vector (both Vector2 and Vector3, both overloads) —
//     the bodies branch on `len == 0.0f` and pass the input through unscaled.
//   * Quaternion::Lerp at t = 0 and t = 1 — it is a shortest-path NORMALISED
//     lerp with no zero guard, so the endpoints must come back as the
//     normalised operands, not as the operands verbatim.
//   * Transform composition against a HAND-COMPUTED matrix.
//   * Matrix::Inverse on a singular matrix — retail returns the IDENTITY, not
//     zeroes and not garbage.
//   * Serialize's vtable-slot-0x28 protocol, driven by a fake encoding object.
//
// No game directory, no registry: tests/unit/rt_port_stub.cpp supplies the
// three port-registry symbols and the bodies run directly.
// ============================================================================

#include "unit.h"

#include "math/Math.h"
#include "slot_check.h"
#include "runtime/types/io/Serializer.h"   // IO::ReaderWriter (the slot shift)

#include <cmath>
#include <cstdint>

namespace {

constexpr float kEps = 1e-5f;

bool Near(float a, float b, float eps = kEps) {
    return std::fabs(a - b) <= eps;
}
bool Near(const Math::Vector2& a, float x, float y, float eps = kEps) {
    return Near(a.x, x, eps) && Near(a.y, y, eps);
}
bool Near(const Math::Vector3& a, float x, float y, float z, float eps = kEps) {
    return Near(a.x, x, eps) && Near(a.y, y, eps) && Near(a.z, z, eps);
}
bool Near(const Math::Vector4& a, float x, float y, float z, float w, float eps = kEps) {
    return Near(a.x, x, eps) && Near(a.y, y, eps) && Near(a.z, z, eps) && Near(a.w, w, eps);
}
bool NearQ(const Math::Quaternion& q, float x, float y, float z, float w, float eps = kEps) {
    return Near(q.x, x, eps) && Near(q.y, y, eps) && Near(q.z, z, eps) && Near(q.w, w, eps);
}

// A row-vector times a Matrix, written independently of Matrix::operator* so it
// can cross-check the class against its own AsMatrix.
Math::Vector3 RowVecTimesMatrix(const Math::Vector3& v, const Math::Matrix& m) {
    Math::Vector3 out;
    out.x = v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8]  + m.m[12];
    out.y = v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9]  + m.m[13];
    out.z = v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10] + m.m[14];
    return out;
}

// ---------------------------------------------------------------------------
// A fake IO::Serializer for the Serialize tests.
//
// The ported bodies read `serializer + 8` as the object to talk to (the
// IO::ReaderWriter), take its first qword as a vtable pointer and call
// `SerializeFloatArray` — retail byte offset 0x28 — with (object, count, data).
// Nothing else about the layout is touched, so this hand-built pair reproduces
// the whole protocol. B3X W1: the call is BY NAME on a class with a virtual
// destructor, so it reads the COMPILER slot, retail 0x28 + 8 (D1 and D0 take
// slots 0 and 1); the entry is placed through RetailSlot<IO::ReaderWriter>.
// ---------------------------------------------------------------------------
struct FakeSink {
    void*   vtable;      // +0x00, points at gFakeVtable
    int     lastCount;
    const float* lastData;
    int     calls;
};
struct FakeSerializer {
    void*      unusedStreamSlot;   // +0x00, never read by these bodies
    FakeSink*  sink;               // +0x08
};

FakeSink  gSink;
void*     gFakeVtable[8];          // retail 0x28 == index 5 -> compiler index 6
constexpr long kFakeFloatArraySlot = RetailSlot<::IO::ReaderWriter>(0x28 / 8);
static_assert(kFakeFloatArraySlot == 6, "retail slot 5 behind the D1/D0 pair");

void FakeSerializeFloats(void* self, int count, void* data) {
    FakeSink* s = static_cast<FakeSink*>(self);
    s->lastCount = count;
    s->lastData  = static_cast<const float*>(data);
    ++s->calls;
}

FakeSerializer MakeFakeSerializer() {
    for (int i = 0; i < 8; ++i) gFakeVtable[i] = nullptr;
    gFakeVtable[kFakeFloatArraySlot] = reinterpret_cast<void*>(&FakeSerializeFloats);

    gSink.vtable = gFakeVtable;
    gSink.lastCount = 0;
    gSink.lastData = nullptr;
    gSink.calls = 0;
    FakeSerializer ser;
    ser.unusedStreamSlot = nullptr;
    ser.sink = &gSink;
    return ser;
}

}  // namespace

using Math::Matrix;
using Math::Quaternion;
using Math::Scalar;
using Math::Transform;
using Math::Transform2;
using Math::Transform3;
using Math::Vector2;
using Math::Vector3;
using Math::Vector4;

// ============================================================================
// Math::Scalar
// ============================================================================

UNIT_TEST(scalar_default_ctor_is_zero) {
    Scalar s;                     // retail 0x180021930 stores 0
    CHECK(s.v == 0.0f);
    Scalar t{};
    CHECK(t.v == 0.0f);
}

UNIT_TEST(scalar_float_ctor) {
    Scalar s(2.5f);               // retail 0x180015340
    CHECK(s.v == 2.5f);
}

UNIT_TEST(scalar_abs) {
    CHECK(Scalar(-3.25f).Abs().v == 3.25f);
    CHECK(Scalar(3.25f).Abs().v == 3.25f);
    CHECK(Scalar(0.0f).Abs().v == 0.0f);
}

UNIT_TEST(scalar_lerp) {
    Scalar a(2.0f);
    CHECK(Near(a.Lerp(Scalar(6.0f), Scalar(0.0f)).v, 2.0f));
    CHECK(Near(a.Lerp(Scalar(6.0f), Scalar(1.0f)).v, 6.0f));
    CHECK(Near(a.Lerp(Scalar(6.0f), Scalar(0.25f)).v, 3.0f));
    // Extrapolation is not clamped in retail.
    CHECK(Near(a.Lerp(Scalar(6.0f), Scalar(2.0f)).v, 10.0f));
}

UNIT_TEST(scalar_max) {
    CHECK(Scalar(1.0f).Max(Scalar(2.0f)).v == 2.0f);
    CHECK(Scalar(2.0f).Max(Scalar(1.0f)).v == 2.0f);
    // Ties take `other` (the body is `(other.v < this->v) ? this->v : other.v`).
    CHECK(Scalar(2.0f).Max(Scalar(2.0f)).v == 2.0f);
}

// ============================================================================
// Math::Vector2
// ============================================================================

UNIT_TEST(vector2_ctors) {
    Vector2 z;                                  // retail 0x180060030 zeroes
    CHECK(Near(z, 0.0f, 0.0f));
    Vector2 f(1.5f, -2.5f);
    CHECK(Near(f, 1.5f, -2.5f));
    Vector2 s(Scalar(3.0f), Scalar(4.0f));      // retail 0x180070db0
    CHECK(Near(s, 3.0f, 4.0f));
}

UNIT_TEST(vector2_mul_assign) {
    Vector2 a(2.0f, 3.0f);
    a *= Vector2(4.0f, 5.0f);
    CHECK(Near(a, 8.0f, 15.0f));
}

UNIT_TEST(vector2_dot_and_lengths) {
    Vector2 a(3.0f, 4.0f);
    CHECK(Near(a.Dot(Vector2(2.0f, 1.0f)).v, 10.0f));
    CHECK(Near(a.Length().v, 5.0f));
    CHECK(Near(a.LengthSquared().v, 25.0f));
    CHECK(Vector2().Length().v == 0.0f);
}

UNIT_TEST(vector2_min_max) {
    Vector2 a(1.0f, 5.0f);
    Vector2 b(3.0f, 2.0f);
    CHECK(Near(a.Max(b), 3.0f, 5.0f));
    CHECK(Near(a.Min(b), 1.0f, 2.0f));
}

UNIT_TEST(vector2_normalize) {
    Vector2 a(3.0f, 4.0f);
    CHECK(Near(a.Normalize(), 0.6f, 0.8f));
    Scalar mag;
    Vector2 n = a.Normalize(mag);
    CHECK(Near(n, 0.6f, 0.8f));
    CHECK(Near(mag.v, 5.0f));
    // The source is not mutated (retail's Normalize is const, by value).
    CHECK(Near(a, 3.0f, 4.0f));
}

UNIT_TEST(vector2_normalize_zero_vector_passes_through) {
    Vector2 zero;
    CHECK(Near(zero.Normalize(), 0.0f, 0.0f));
    Scalar mag(99.0f);
    Vector2 n = zero.Normalize(mag);
    CHECK(Near(n, 0.0f, 0.0f));
    CHECK(mag.v == 0.0f);           // the magnitude is written even on the zero arm
}

UNIT_TEST(vector2_serialize_uses_slot_0x28_with_count_2) {
    FakeSerializer ser = MakeFakeSerializer();
    Vector2 v(1.0f, 2.0f);
    v.Serialize(&ser);
    CHECK_EQ(gSink.calls, 1);
    CHECK_EQ(gSink.lastCount, 2);
    CHECK(gSink.lastData == &v.x);
}

// ============================================================================
// Math::Vector3
// ============================================================================

UNIT_TEST(vector3_ctors) {
    Vector3 z;                                             // retail 0x180015070
    CHECK(Near(z, 0.0f, 0.0f, 0.0f));
    Vector3 f(1.0f, 2.0f, 3.0f);                           // retail 0x1800152d0
    CHECK(Near(f, 1.0f, 2.0f, 3.0f));
    Vector3 s(Scalar(4.0f), Scalar(5.0f), Scalar(6.0f));   // retail 0x180015120
    CHECK(Near(s, 4.0f, 5.0f, 6.0f));
    Vector3 splat{Scalar(7.0f)};                           // retail 0x180015360
    CHECK(Near(splat, 7.0f, 7.0f, 7.0f));
}

UNIT_TEST(vector3_cross) {
    Vector3 x(1.0f, 0.0f, 0.0f);
    Vector3 y(0.0f, 1.0f, 0.0f);
    CHECK(Near(x.Cross(y), 0.0f, 0.0f, 1.0f));
    CHECK(Near(y.Cross(x), 0.0f, 0.0f, -1.0f));
    CHECK(Near(x.Cross(x), 0.0f, 0.0f, 0.0f));
}

UNIT_TEST(vector3_dot_and_length) {
    Vector3 a(1.0f, 2.0f, 3.0f);
    CHECK(Near(a.Dot(Vector3(4.0f, 5.0f, 6.0f)).v, 32.0f));
    CHECK(Near(Vector3(2.0f, 3.0f, 6.0f).Length().v, 7.0f));
}

UNIT_TEST(vector3_from_yaw_pitch) {
    // x = -cos(pitch)*sin(yaw); y = cos(pitch)*cos(yaw); z = sin(pitch), degrees in.
    CHECK(Near(Vector3::FromYawPitch(Scalar(0.0f), Scalar(0.0f)), 0.0f, 1.0f, 0.0f));
    CHECK(Near(Vector3::FromYawPitch(Scalar(90.0f), Scalar(0.0f)), -1.0f, 0.0f, 0.0f));
    CHECK(Near(Vector3::FromYawPitch(Scalar(0.0f), Scalar(90.0f)), 0.0f, 0.0f, 1.0f));
}

UNIT_TEST(vector3_heading) {
    // atan2(-x, y) in degrees. +Y is 0, -X is +90.
    CHECK(Near(Vector3(0.0f, 1.0f, 0.0f).Heading(), 0.0f, 1e-3f));
    CHECK(Near(Vector3(-1.0f, 0.0f, 0.0f).Heading(), 90.0f, 1e-3f));
    CHECK(Near(Vector3(1.0f, 0.0f, 0.0f).Heading(), -90.0f, 1e-3f));
    // FromYawPitch and Heading are inverses on the horizontal plane.
    Vector3 d = Vector3::FromYawPitch(Scalar(37.0f), Scalar(0.0f));
    CHECK(Near(d.Heading(), 37.0f, 1e-3f));
}

UNIT_TEST(vector3_lerp) {
    Vector3 a(0.0f, 0.0f, 0.0f);
    Vector3 b(2.0f, 4.0f, 8.0f);
    CHECK(Near(a.Lerp(b, Scalar(0.0f)), 0.0f, 0.0f, 0.0f));
    CHECK(Near(a.Lerp(b, Scalar(1.0f)), 2.0f, 4.0f, 8.0f));
    CHECK(Near(a.Lerp(b, Scalar(0.5f)), 1.0f, 2.0f, 4.0f));
}

UNIT_TEST(vector3_min) {
    Vector3 a(1.0f, 5.0f, 3.0f);
    Vector3 b(4.0f, 2.0f, 3.0f);
    CHECK(Near(a.Min(b), 1.0f, 2.0f, 3.0f));
}

UNIT_TEST(vector3_normalize) {
    Vector3 a(0.0f, 3.0f, 4.0f);
    CHECK(Near(a.Normalize(), 0.0f, 0.6f, 0.8f));
    Scalar mag;
    CHECK(Near(a.Normalize(mag), 0.0f, 0.6f, 0.8f));
    CHECK(Near(mag.v, 5.0f));
    CHECK(Near(a, 0.0f, 3.0f, 4.0f));
}

UNIT_TEST(vector3_normalize_zero_vector_passes_through) {
    Vector3 zero;
    CHECK(Near(zero.Normalize(), 0.0f, 0.0f, 0.0f));
    Scalar mag(99.0f);
    CHECK(Near(zero.Normalize(mag), 0.0f, 0.0f, 0.0f));
    CHECK(mag.v == 0.0f);
}

UNIT_TEST(vector3_equality) {
    CHECK(Vector3(1.0f, 2.0f, 3.0f) == Vector3(1.0f, 2.0f, 3.0f));
    CHECK(!(Vector3(1.0f, 2.0f, 3.0f) == Vector3(1.0f, 2.0f, 3.5f)));
}

UNIT_TEST(vector3_serialize_uses_count_3) {
    FakeSerializer ser = MakeFakeSerializer();
    Vector3 v(1.0f, 2.0f, 3.0f);
    v.Serialize(&ser);
    CHECK_EQ(gSink.calls, 1);
    CHECK_EQ(gSink.lastCount, 3);
    CHECK(gSink.lastData == &v.x);
}

// ============================================================================
// Math::Vector4
// ============================================================================

UNIT_TEST(vector4_ctors) {
    Vector4 z;                                     // retail 0x18001e0a0
    CHECK(Near(z, 0.0f, 0.0f, 0.0f, 0.0f));
    Vector4 f(1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(Near(f, 1.0f, 2.0f, 3.0f, 4.0f));
    Vector4 splat{Scalar(5.0f)};                   // retail 0x18001d750
    CHECK(Near(splat, 5.0f, 5.0f, 5.0f, 5.0f));
    Vector4 v3w(Vector3(1.0f, 2.0f, 3.0f), Scalar(9.0f));   // retail 0x18001e070
    CHECK(Near(v3w, 1.0f, 2.0f, 3.0f, 9.0f));
}

UNIT_TEST(vector4_serialize_uses_count_4) {
    FakeSerializer ser = MakeFakeSerializer();
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    v.Serialize(&ser);
    CHECK_EQ(gSink.calls, 1);
    CHECK_EQ(gSink.lastCount, 4);
    CHECK(gSink.lastData == &v.x);
}

// ============================================================================
// Math::Quaternion
// ============================================================================

UNIT_TEST(quaternion_default_ctor_is_identity) {
    Quaternion q;                 // retail 0x180015050 writes 1.0f at +0x0c
    CHECK(NearQ(q, 0.0f, 0.0f, 0.0f, 1.0f));
}

UNIT_TEST(quaternion_four_float_ctor) {
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);   // retail 0x18001e260
    CHECK(NearQ(q, 1.0f, 2.0f, 3.0f, 4.0f));
}

UNIT_TEST(quaternion_multiply_by_identity) {
    Quaternion q(0.1f, 0.2f, 0.3f, 0.9f);
    Quaternion id;
    CHECK(NearQ(q * id, q.x, q.y, q.z, q.w));
    CHECK(NearQ(id * q, q.x, q.y, q.z, q.w));
}

UNIT_TEST(quaternion_rotate) {
    // (x,y,z,w) = (0, 0, sin45, cos45), a quarter turn about Z.
    //
    // THE SENSE IS THE ONE RETAIL HAS, not the textbook one, and it is pinned
    // here on purpose. Rotate is `(q * Quaternion(v,0)) * conjugate(q)` with
    // this file's operator* operand order, and Transform::AsMatrix expands the
    // same quaternion into a matrix that the tree applies as a ROW vector
    // (v * M). Both agree that this quaternion sends (1,0,0) to (0,-1,0):
    // AsMatrix's row 0 is (0,-1,0) — see transform_as_matrix_hand_computed —
    // and transform_multiply_vector3 cross-checks Rotate against that matrix
    // independently. Flipping either one alone would break that agreement.
    const float s = std::sqrt(0.5f);
    Quaternion zq(0.0f, 0.0f, s, s);
    CHECK(Near(zq.Rotate(Vector3(1.0f, 0.0f, 0.0f)), 0.0f, -1.0f, 0.0f));
    CHECK(Near(zq.Rotate(Vector3(0.0f, 1.0f, 0.0f)), 1.0f, 0.0f, 0.0f));
    CHECK(Near(zq.Rotate(Vector3(0.0f, 0.0f, 1.0f)), 0.0f, 0.0f, 1.0f));
    // Length is preserved, so it really is a rotation.
    Vector3 r = zq.Rotate(Vector3(1.0f, 2.0f, 3.0f));
    CHECK(Near(r.Length().v, Vector3(1.0f, 2.0f, 3.0f).Length().v));
    // The identity rotates nothing.
    CHECK(Near(Quaternion().Rotate(Vector3(1.0f, 2.0f, 3.0f)), 1.0f, 2.0f, 3.0f));
}

UNIT_TEST(quaternion_rotate_composes_like_multiply) {
    const float s = std::sqrt(0.5f);
    Quaternion a(0.0f, 0.0f, s, s);           // +90 about Z
    Quaternion b(s, 0.0f, 0.0f, s);           // +90 about X
    Vector3 v(1.0f, 2.0f, 3.0f);
    // (a*b).Rotate(v) applies b then a in the convention these bodies use.
    Vector3 viaProduct = (a * b).Rotate(v);
    Vector3 viaSteps   = a.Rotate(b.Rotate(v));
    CHECK(Near(viaProduct, viaSteps.x, viaSteps.y, viaSteps.z));
}

UNIT_TEST(quaternion_lerp_endpoints_are_the_normalised_operands) {
    // A shortest-path NORMALISED lerp: at t=0 it must return `this`
    // renormalised, at t=1 `other` renormalised. Both operands are already
    // unit here, so the endpoints are the operands themselves.
    const float s = std::sqrt(0.5f);
    Quaternion a;                       // identity
    Quaternion b(0.0f, 0.0f, s, s);     // +90 about Z
    CHECK(NearQ(a.Lerp(b, Scalar(0.0f)), 0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(NearQ(a.Lerp(b, Scalar(1.0f)), 0.0f, 0.0f, s, s));
    // A non-unit operand comes back normalised, which is the whole point.
    Quaternion big(0.0f, 0.0f, 0.0f, 4.0f);
    CHECK(NearQ(a.Lerp(big, Scalar(1.0f)), 0.0f, 0.0f, 0.0f, 1.0f));
}

UNIT_TEST(quaternion_lerp_midpoint_is_unit_and_between) {
    const float s = std::sqrt(0.5f);
    Quaternion a;
    Quaternion b(0.0f, 0.0f, s, s);
    Quaternion mid = a.Lerp(b, Scalar(0.5f));
    CHECK(Near(mid.x * mid.x + mid.y * mid.y + mid.z * mid.z + mid.w * mid.w, 1.0f));
    CHECK(mid.z > 0.0f && mid.z < s);
    CHECK(mid.w > s && mid.w < 1.0f);
}

UNIT_TEST(quaternion_lerp_takes_the_short_way_round) {
    // dot <= 0 flips the sign of t rather than of `other`, so the result must
    // still be near `a` for a small t even when `other` is the long-way twin.
    const float s = std::sqrt(0.5f);
    Quaternion a;
    Quaternion farSide(0.0f, 0.0f, -s, -s);   // same rotation as (0,0,s,s), negated
    Quaternion nearSide(0.0f, 0.0f, s, s);
    Quaternion viaFar  = a.Lerp(farSide, Scalar(0.25f));
    Quaternion viaNear = a.Lerp(nearSide, Scalar(0.25f));
    // Equal up to overall sign, which is the same rotation.
    CHECK(Near(std::fabs(viaFar.z), std::fabs(viaNear.z)));
    CHECK(Near(std::fabs(viaFar.w), std::fabs(viaNear.w)));
}

UNIT_TEST(quaternion_from_yaw_pitch) {
    Quaternion id = Quaternion::FromYawPitch(Scalar(0.0f), Scalar(0.0f));
    CHECK(NearQ(id, 0.0f, 0.0f, 0.0f, 1.0f));
    // Yaw only: a rotation about Z, so x == y == 0 and w == cos(yaw/2).
    Quaternion yaw = Quaternion::FromYawPitch(Scalar(90.0f), Scalar(0.0f));
    const float s = std::sqrt(0.5f);
    CHECK(Near(yaw.x, 0.0f));
    CHECK(Near(yaw.y, 0.0f));
    CHECK(Near(yaw.z, -s));
    CHECK(Near(yaw.w, s));
    CHECK(Near(yaw.x * yaw.x + yaw.y * yaw.y + yaw.z * yaw.z + yaw.w * yaw.w, 1.0f));
}

UNIT_TEST(quaternion_basis_ctor_round_trips_through_as_matrix) {
    // The basis ctor's three arguments are the COLUMNS of AsMatrix
    // (m[0..2], m[4..6], m[8..10]) — see the comment on the body — so feeding
    // it a quaternion's own matrix columns must give that quaternion back.
    // The identity basis first.
    Quaternion q(Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f));
    CHECK(NearQ(q, 0.0f, 0.0f, 0.0f, 1.0f));

    const float s = std::sqrt(0.5f);
    Quaternion q0(0.0f, 0.0f, s, s);          // +90 about Z
    Transform t;
    t.rotation = q0;
    Matrix m = t.AsMatrix();
    Quaternion r(Vector3(m.m[0], m.m[1], m.m[2]),
                 Vector3(m.m[4], m.m[5], m.m[6]),
                 Vector3(m.m[8], m.m[9], m.m[10]));
    CHECK(Near(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w, 1.0f, 1e-4f));
    CHECK(NearQ(r, q0.x, q0.y, q0.z, q0.w, 1e-4f));
}

// ============================================================================
// Math::Matrix
// ============================================================================

UNIT_TEST(matrix_default_ctor_is_all_zero_not_identity) {
    Matrix m;                     // retail 0x18001d770 zeroes all sixteen cells
    for (int i = 0; i < 16; ++i) CHECK(m.m[i] == 0.0f);
}

UNIT_TEST(matrix_identity) {
    Matrix m = Matrix::Identity();
    for (int i = 0; i < 16; ++i) {
        CHECK(m.m[i] == ((i % 5 == 0) ? 1.0f : 0.0f));
    }
}

UNIT_TEST(matrix_column_ctor) {
    Matrix m(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
             Vector4(5.0f, 6.0f, 7.0f, 8.0f),
             Vector4(9.0f, 10.0f, 11.0f, 12.0f),
             Vector4(13.0f, 14.0f, 15.0f, 16.0f));
    for (int i = 0; i < 16; ++i) CHECK(m.m[i] == static_cast<float>(i + 1));
}

UNIT_TEST(matrix_float_array_ctor) {
    const float cells[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  7, 8, 9, 1};
    Matrix m(cells);
    CHECK(m.m[12] == 7.0f);
    CHECK(m.m[13] == 8.0f);
    CHECK(m.m[14] == 9.0f);
    CHECK(m.m[15] == 1.0f);
}

UNIT_TEST(matrix_multiply_by_identity) {
    Matrix a(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
             Vector4(5.0f, 6.0f, 7.0f, 8.0f),
             Vector4(9.0f, 1.0f, 2.0f, 3.0f),
             Vector4(4.0f, 5.0f, 6.0f, 7.0f));
    Matrix p = a * Matrix::Identity();
    for (int i = 0; i < 16; ++i) CHECK(Near(p.m[i], a.m[i]));
}

UNIT_TEST(matrix_multiply_hand_computed) {
    // Two translations compose by adding their translation rows.
    const float ta[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  1, 2, 3, 1};
    const float tb[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  10, 20, 30, 1};
    Matrix p = Matrix(ta) * Matrix(tb);
    CHECK(Near(p.m[12], 11.0f));
    CHECK(Near(p.m[13], 22.0f));
    CHECK(Near(p.m[14], 33.0f));
    CHECK(Near(p.m[15], 1.0f));
}

UNIT_TEST(matrix_multiply_vector4) {
    // out[c] = v.y*m[4+c] + v.x*m[0+c] + v.z*m[8+c] + v.w*m[12+c] — the
    // row-vector convention, so a w of 1 picks up the translation row.
    const float t[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  5, 6, 7, 1};
    Vector4 r = Matrix(t) * Vector4(1.0f, 2.0f, 3.0f, 1.0f);
    CHECK(Near(r, 6.0f, 8.0f, 10.0f, 1.0f));
    Vector4 dir = Matrix(t) * Vector4(1.0f, 2.0f, 3.0f, 0.0f);
    CHECK(Near(dir, 1.0f, 2.0f, 3.0f, 0.0f));
}

UNIT_TEST(matrix_inverse_round_trip) {
    Transform t;
    t.rotation = Quaternion::FromYawPitch(Scalar(35.0f), Scalar(-20.0f));
    t.translation = Vector3(3.0f, -4.0f, 5.0f);
    Matrix m = t.AsMatrix();
    Matrix p = m * m.Inverse();
    for (int i = 0; i < 16; ++i) {
        CHECK(Near(p.m[i], (i % 5 == 0) ? 1.0f : 0.0f, 1e-4f));
    }
}

UNIT_TEST(matrix_inverse_of_singular_is_identity) {
    // A zero determinant returns the IDENTITY in retail (0x1800bb545), not
    // zeroes and not garbage. The all-zero matrix is the simplest such input.
    Matrix zero;
    Matrix inv = zero.Inverse();
    for (int i = 0; i < 16; ++i) CHECK(inv.m[i] == ((i % 5 == 0) ? 1.0f : 0.0f));
}

// ============================================================================
// Math::Transform
// ============================================================================

UNIT_TEST(transform_default_ctor_is_identity) {
    Transform t;                  // retail 0x180015080
    CHECK(NearQ(t.rotation, 0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(Near(t.translation, 0.0f, 0.0f, 0.0f));
}

UNIT_TEST(transform_copy_and_assign) {
    Transform a;
    a.rotation = Quaternion(0.1f, 0.2f, 0.3f, 0.9f);
    a.translation = Vector3(1.0f, 2.0f, 3.0f);
    Transform b(a);
    CHECK(NearQ(b.rotation, 0.1f, 0.2f, 0.3f, 0.9f));
    CHECK(Near(b.translation, 1.0f, 2.0f, 3.0f));
    Transform c;
    c = a;
    CHECK(NearQ(c.rotation, 0.1f, 0.2f, 0.3f, 0.9f));
    CHECK(Near(c.translation, 1.0f, 2.0f, 3.0f));
}

UNIT_TEST(transform_static_identity) {
    Transform t = Transform::Identity();
    CHECK(NearQ(t.rotation, 0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(Near(t.translation, 0.0f, 0.0f, 0.0f));
}

UNIT_TEST(transform_as_matrix_hand_computed) {
    // 90 degrees about +Z, translated by (5, 6, 7). By hand from the body's
    // quaternion expansion with (x,y,z,w) = (0, 0, s, s), s = sqrt(1/2):
    //   qzqz2 = 2*z*z = 1, qwqz2 = 2*w*z = 1, every other product term is 0.
    //   m[0] = 1 - (qzqz2 + qyqy2) = 0     m[1] = qyqx2 - qwqz2 = -1
    //   m[4] = qwqz2 + qyqx2       = 1     m[5] = 1 - (qzqz2 + qxqx2) = 0
    //   m[10] = 1 - (qyqy2 + qxqx2) = 1    m[15] = 1
    //   m[12..14] = translation            m[2,3,6,7,8,9,11] = 0
    const float s = std::sqrt(0.5f);
    Transform t;
    t.rotation = Quaternion(0.0f, 0.0f, s, s);
    t.translation = Vector3(5.0f, 6.0f, 7.0f);
    Matrix m = t.AsMatrix();

    const float expect[16] = {
         0.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  0.0f, 0.0f, 0.0f,
         0.0f,  0.0f, 1.0f, 0.0f,
         5.0f,  6.0f, 7.0f, 1.0f,
    };
    for (int i = 0; i < 16; ++i) CHECK(Near(m.m[i], expect[i]));
}

UNIT_TEST(transform_multiply_vector3) {
    const float s = std::sqrt(0.5f);
    Transform t;
    t.rotation = Quaternion(0.0f, 0.0f, s, s);
    t.translation = Vector3(5.0f, 6.0f, 7.0f);
    // rotate (1,0,0) to (0,-1,0) — see quaternion_rotate for the sense — then
    // translate by (5,6,7).
    CHECK(Near(t * Vector3(1.0f, 0.0f, 0.0f), 5.0f, 5.0f, 7.0f));
    // Agrees with the matrix form under the row-vector convention.
    Vector3 v(1.0f, 2.0f, 3.0f);
    Vector3 viaTransform = t * v;
    Vector3 viaMatrix    = RowVecTimesMatrix(v, t.AsMatrix());
    CHECK(Near(viaTransform, viaMatrix.x, viaMatrix.y, viaMatrix.z));
}

UNIT_TEST(transform_compose_against_hand_computed_matrix) {
    // A = +90 about Z, translation (1,0,0).  B = no rotation, translation (0,2,0).
    // The body computes:
    //   (A*B).translation = B.rotation.Rotate(A.translation) + B.translation
    //                     = (1,0,0) + (0,2,0) = (1,2,0)
    //   (A*B).rotation    = B.rotation * A.rotation = A.rotation
    // so A*B is "+90 about Z, translated by (1,2,0)" and its matrix is the
    // 90-degree Z basis with that translation row.
    const float s = std::sqrt(0.5f);
    Transform a;
    a.rotation = Quaternion(0.0f, 0.0f, s, s);
    a.translation = Vector3(1.0f, 0.0f, 0.0f);
    Transform b;
    b.translation = Vector3(0.0f, 2.0f, 0.0f);

    Transform ab = a * b;
    CHECK(NearQ(ab.rotation, 0.0f, 0.0f, s, s));
    CHECK(Near(ab.translation, 1.0f, 2.0f, 0.0f));

    const float expect[16] = {
         0.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  0.0f, 0.0f, 0.0f,
         0.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  2.0f, 0.0f, 1.0f,
    };
    Matrix m = ab.AsMatrix();
    for (int i = 0; i < 16; ++i) CHECK(Near(m.m[i], expect[i]));

    // And composition agrees with applying the two transforms in sequence.
    Vector3 v(3.0f, -1.0f, 2.0f);
    Vector3 viaProduct = ab * v;
    Vector3 viaSteps   = b * (a * v);
    CHECK(Near(viaProduct, viaSteps.x, viaSteps.y, viaSteps.z));
}

UNIT_TEST(transform_inverse) {
    const float s = std::sqrt(0.5f);
    Transform t;
    t.rotation = Quaternion(0.0f, 0.0f, s, s);
    t.translation = Vector3(4.0f, -3.0f, 2.0f);
    Transform inv = t.Inverse();
    // The inverse rotation is the conjugate.
    CHECK(NearQ(inv.rotation, 0.0f, 0.0f, -s, s));
    // Applying one then the other is the identity on a point.
    Vector3 v(1.0f, 2.0f, 3.0f);
    Vector3 back = inv * (t * v);
    CHECK(Near(back, 1.0f, 2.0f, 3.0f, 1e-4f));
}

UNIT_TEST(transform_as_transform3) {
    const float s = std::sqrt(0.5f);
    Transform t;
    t.rotation = Quaternion(0.0f, 0.0f, s, s);
    t.translation = Vector3(5.0f, 6.0f, 7.0f);
    Transform3 t3 = t.AsTransform3();
    Matrix m = t.AsMatrix();
    CHECK(Near(t3.axisX, m.m[0], m.m[1], m.m[2]));
    CHECK(Near(t3.axisY, m.m[4], m.m[5], m.m[6]));
    CHECK(Near(t3.axisZ, m.m[8], m.m[9], m.m[10]));
    CHECK(Near(t3.translation, 5.0f, 6.0f, 7.0f));
}

UNIT_TEST(transform_from_yaw_pitch_offset) {
    Transform t = Transform::FromYawPitchOffset(Scalar(0.0f), Scalar(0.0f), Vector3(1.0f, 2.0f, 3.0f));
    CHECK(NearQ(t.rotation, 0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(Near(t.translation, 1.0f, 2.0f, 3.0f));
    // The rotation half matches Quaternion::FromYawPitch for a yaw-only angle.
    Transform y = Transform::FromYawPitchOffset(Scalar(90.0f), Scalar(0.0f), Vector3());
    Quaternion q = Quaternion::FromYawPitch(Scalar(90.0f), Scalar(0.0f));
    CHECK(NearQ(y.rotation, q.x, q.y, q.z, q.w));
}

UNIT_TEST(transform_from_direction) {
    // `direction` must already be unit. Looking along +Y from the origin is the
    // identity basis, so the rotation is the identity.
    Transform t = Transform::FromDirection(Vector3(0.0f, 1.0f, 0.0f), Vector3(1.0f, 2.0f, 3.0f));
    CHECK(NearQ(t.rotation, 0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(Near(t.translation, 1.0f, 2.0f, 3.0f));
    // Whatever the direction, the rotation is unit and maps +Y onto it.
    Vector3 dir = Vector3(1.0f, 2.0f, -0.5f).Normalize();
    Transform d = Transform::FromDirection(dir, Vector3());
    Vector3 fwd = d.rotation.Rotate(Vector3(0.0f, 1.0f, 0.0f));
    CHECK(Near(fwd, dir.x, dir.y, dir.z, 1e-3f));
}

UNIT_TEST(transform_from_look_at) {
    // FromLookAt normalises (to - from) and hands it to FromDirection with
    // `from` as the offset.
    Transform t = Transform::FromLookAt(Vector3(1.0f, 2.0f, 3.0f), Vector3(1.0f, 12.0f, 3.0f));
    CHECK(Near(t.translation, 1.0f, 2.0f, 3.0f));
    CHECK(NearQ(t.rotation, 0.0f, 0.0f, 0.0f, 1.0f));
    // A degenerate look (from == to) must not divide by zero; it leaves the
    // direction at (0,0,0) rather than producing NaN.
    Transform deg = Transform::FromLookAt(Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f));
    CHECK(Near(deg.translation, 1.0f, 1.0f, 1.0f));
    CHECK(!std::isnan(deg.rotation.w));
}

UNIT_TEST(transform_serialize_writes_quaternion_then_translation) {
    FakeSerializer ser = MakeFakeSerializer();
    Transform t;
    t.translation = Vector3(1.0f, 2.0f, 3.0f);
    t.Serialize(&ser);
    // Two calls: count 4 over the quaternion, then count 3 over the translation.
    CHECK_EQ(gSink.calls, 2);
    CHECK_EQ(gSink.lastCount, 3);
    CHECK(gSink.lastData == &t.translation.x);
}

// ============================================================================
// Math::Transform2
// ============================================================================

UNIT_TEST(transform2_default_ctor_is_all_zero) {
    Transform2 t;                 // retail 0x18006c8b0 zeroes 0x18 bytes
    CHECK(Near(t.rotX, 0.0f, 0.0f));
    CHECK(Near(t.rotY, 0.0f, 0.0f));
    CHECK(Near(t.translation, 0.0f, 0.0f));
}

UNIT_TEST(transform2_angle_ctor) {
    Transform2 t(Scalar(90.0f), Vector2(3.0f, 4.0f));   // retail 0x18007dca0
    CHECK(Near(t.rotX, 0.0f, 1.0f));       // { cos,  sin }
    CHECK(Near(t.rotY, -1.0f, 0.0f));      // { -sin, cos }
    CHECK(Near(t.translation, 3.0f, 4.0f));
}

UNIT_TEST(transform2_from_yaw_offset_matches_the_ctor) {
    Transform2 a(Scalar(37.0f), Vector2(1.0f, -2.0f));
    Transform2 b = Transform2::FromYawOffset(Scalar(37.0f), Vector2(1.0f, -2.0f));
    CHECK(Near(a.rotX, b.rotX.x, b.rotX.y));
    CHECK(Near(a.rotY, b.rotY.x, b.rotY.y));
    CHECK(Near(a.translation, b.translation.x, b.translation.y));
}

UNIT_TEST(transform2_multiply_vector2) {
    Transform2 t(Scalar(90.0f), Vector2(5.0f, 6.0f));
    // (1,0) rotated by +90 is (0,1), then translated.
    CHECK(Near(t * Vector2(1.0f, 0.0f), 5.0f, 7.0f));
    // The zero-angle case is a pure translation.
    Transform2 tr(Scalar(0.0f), Vector2(2.0f, 3.0f));
    CHECK(Near(tr * Vector2(1.0f, 1.0f), 3.0f, 4.0f));
}

UNIT_TEST(transform2_multiply_transform2) {
    Transform2 a(Scalar(90.0f), Vector2(1.0f, 0.0f));
    Transform2 b(Scalar(0.0f), Vector2(0.0f, 2.0f));
    Transform2 ab = a * b;
    // Composition agrees with applying the two in sequence to a point.
    Vector2 v(3.0f, -1.0f);
    Vector2 viaProduct = ab * v;
    Vector2 viaSteps   = b * (a * v);
    CHECK(Near(viaProduct, viaSteps.x, viaSteps.y));
}

// ============================================================================
// Math::Transform3
// ============================================================================

UNIT_TEST(transform3_default_ctor_is_all_zero) {
    Transform3 t;                 // retail 0x18005cc20 zeroes 0x30 bytes
    CHECK(Near(t.axisX, 0.0f, 0.0f, 0.0f));
    CHECK(Near(t.axisY, 0.0f, 0.0f, 0.0f));
    CHECK(Near(t.axisZ, 0.0f, 0.0f, 0.0f));
    CHECK(Near(t.translation, 0.0f, 0.0f, 0.0f));
}

UNIT_TEST(transform3_identity) {
    Transform3 t = Transform3::Identity();
    CHECK(Near(t.axisX, 1.0f, 0.0f, 0.0f));
    CHECK(Near(t.axisY, 0.0f, 1.0f, 0.0f));
    CHECK(Near(t.axisZ, 0.0f, 0.0f, 1.0f));
    CHECK(Near(t.translation, 0.0f, 0.0f, 0.0f));
}

UNIT_TEST(transform3_copy_ctor) {
    Transform3 a = Transform3::Identity();
    a.translation = Vector3(1.0f, 2.0f, 3.0f);
    Transform3 b(a);
    CHECK(Near(b.axisX, 1.0f, 0.0f, 0.0f));
    CHECK(Near(b.translation, 1.0f, 2.0f, 3.0f));
}

UNIT_TEST(transform3_multiply_vector3_and_rotate) {
    Transform3 t = Transform3::Identity();
    t.axisX = Vector3(0.0f, 1.0f, 0.0f);     // a +90 about Z basis
    t.axisY = Vector3(-1.0f, 0.0f, 0.0f);
    t.translation = Vector3(5.0f, 6.0f, 7.0f);
    CHECK(Near(t * Vector3(1.0f, 0.0f, 0.0f), 5.0f, 7.0f, 7.0f));
    // Rotate is the same without the translation.
    CHECK(Near(t.Rotate(Vector3(1.0f, 0.0f, 0.0f)), 0.0f, 1.0f, 0.0f));
}

UNIT_TEST(transform3_as_matrix) {
    Transform3 t = Transform3::Identity();
    t.translation = Vector3(1.0f, 2.0f, 3.0f);
    Matrix m = t.AsMatrix();
    const float expect[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        1, 2, 3, 1,
    };
    for (int i = 0; i < 16; ++i) CHECK(Near(m.m[i], expect[i]));
}

UNIT_TEST(transform3_inverse) {
    Transform3 t = Transform3::Identity();
    t.axisX = Vector3(0.0f, 1.0f, 0.0f);
    t.axisY = Vector3(-1.0f, 0.0f, 0.0f);
    t.translation = Vector3(4.0f, -3.0f, 2.0f);
    Transform3 inv = t.Inverse();
    // The basis is transposed.
    CHECK(Near(inv.axisX, 0.0f, -1.0f, 0.0f));
    CHECK(Near(inv.axisY, 1.0f, 0.0f, 0.0f));
    CHECK(Near(inv.axisZ, 0.0f, 0.0f, 1.0f));
    // And it undoes the transform on a point.
    Vector3 v(1.0f, 2.0f, 3.0f);
    Vector3 back = inv * (t * v);
    CHECK(Near(back, 1.0f, 2.0f, 3.0f, 1e-4f));
}

UNIT_TEST(transform3_from_look_at_argument_roles) {
    // RDX is the EYE (it lands in the translation row), R8 is the TARGET, and
    // the SECOND axis is the view direction. Getting either round the other way
    // silently rotates every shadow frustum (from_lookat.cpp's banner).
    Vector3 eye(1.0f, 2.0f, 3.0f);
    Vector3 target(1.0f, 12.0f, 3.0f);          // straight along +Y
    Transform3 t = Transform3::FromLookAt(eye, target);
    CHECK(Near(t.translation, 1.0f, 2.0f, 3.0f));
    CHECK(Near(t.axisY, 0.0f, 1.0f, 0.0f));     // forward
    // right = normalize(cross(forward, up=(0,0,1))) = (1,0,0)
    CHECK(Near(t.axisX, 1.0f, 0.0f, 0.0f));
    // up' = normalize(cross(right, forward)) = (0,0,1)
    CHECK(Near(t.axisZ, 0.0f, 0.0f, 1.0f));
}

UNIT_TEST(transform3_from_look_at_pole_flips_the_up_vector) {
    // Looking straight up (+Z) is within 0.999 of the world Z axis, so the
    // reference up flips to (0,-1,0) rather than degenerating.
    Vector3 eye;
    Transform3 up = Transform3::FromLookAt(eye, Vector3(0.0f, 0.0f, 10.0f));
    CHECK(Near(up.axisY, 0.0f, 0.0f, 1.0f));
    CHECK(Near(up.axisX.x * up.axisX.x + up.axisX.y * up.axisX.y + up.axisX.z * up.axisX.z, 1.0f));
    Transform3 down = Transform3::FromLookAt(eye, Vector3(0.0f, 0.0f, -10.0f));
    CHECK(Near(down.axisY, 0.0f, 0.0f, -1.0f));
    CHECK(Near(down.axisX.x * down.axisX.x + down.axisX.y * down.axisX.y + down.axisX.z * down.axisX.z, 1.0f));
    // The two poles pick OPPOSITE reference ups, so their up' axes differ.
    CHECK(!(up.axisZ == down.axisZ));
}

UNIT_TEST(transform3_from_look_at_degenerate_leaves_zero_axes) {
    // eye == target: forward has zero length and is left unscaled, and the two
    // cross products then have zero length too. Retail has no guard and
    // produces zeroes, not NaN — do not "fix" that.
    Transform3 t = Transform3::FromLookAt(Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f));
    CHECK(Near(t.axisY, 0.0f, 0.0f, 0.0f));
    CHECK(Near(t.translation, 1.0f, 1.0f, 1.0f));
    CHECK(!std::isnan(t.axisX.x));
}
