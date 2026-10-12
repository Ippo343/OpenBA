// ============================================================================
// tests/unit/ext_eye_camera_test.cpp -- ext.eye_camera's pure math
// ============================================================================
// src/ext/eye_camera/eye_math.{h,cpp} without the world: the yaw read back
// from a model rotation built the engine's way (Transform::FromYawPitchOffset,
// and the move tick's (0, 0, -sin(Y/2), cos(Y/2))), the heading convention
// against Vector3::FromYawPitch / Heading, the eye pose from feet + heading +
// distance, the zoom clamp, and the smoothing step (frame-rate independence,
// the short way round a turn, the teleport snap).
// ============================================================================
#include "unit.h"
#include "ext/eye_camera/eye_math.h"
#include "math/Math.h"

#include <cmath>
#include <initializer_list>

namespace {

bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
bool NearDeg(float a, float b, float eps = 1e-2f) {
    return std::fabs(EyeCamera::WrapDegrees(a - b)) <= eps;
}

}  // namespace

UNIT_TEST(ext_eye_camera_wrap_degrees) {
    using EyeCamera::WrapDegrees;
    CHECK(Near(WrapDegrees(0.0f), 0.0f));
    CHECK(Near(WrapDegrees(180.0f), 180.0f));
    CHECK(Near(WrapDegrees(-180.0f), 180.0f));
    CHECK(Near(WrapDegrees(190.0f), -170.0f));
    CHECK(Near(WrapDegrees(-190.0f), 170.0f));
    CHECK(Near(WrapDegrees(725.0f), 5.0f));
}

UNIT_TEST(ext_eye_camera_yaw_from_model_rotation) {
    // The engine's builder, and the move tick's hand-built quaternion.
    for (float yaw : {0.0f, 30.0f, 90.0f, 135.0f, 179.0f, -45.0f, -120.0f, 250.0f}) {
        const Math::Transform t = Math::Transform::FromYawPitchOffset(
            Math::Scalar(yaw), Math::Scalar(0.0f), Math::Vector3(1.0f, 2.0f, 3.0f));
        CHECK(NearDeg(EyeCamera::ModelYawFromQuaternion(t.rotation.z, t.rotation.w), yaw));
        // q and -q are the same rotation.
        CHECK(NearDeg(EyeCamera::ModelYawFromQuaternion(-t.rotation.z, -t.rotation.w), yaw));
        const float half = yaw * 0.5f * 0.017453292f;
        CHECK(NearDeg(EyeCamera::ModelYawFromQuaternion(-std::sin(half), std::cos(half)), yaw));
    }
    // A lerped (renderer-interpolated) model transform reads back in between.
    const Math::Transform a = Math::Transform::FromYawPitchOffset(
        Math::Scalar(10.0f), Math::Scalar(0.0f), Math::Vector3(0.0f, 0.0f, 0.0f));
    const Math::Transform b = Math::Transform::FromYawPitchOffset(
        Math::Scalar(50.0f), Math::Scalar(0.0f), Math::Vector3(2.0f, 0.0f, 0.0f));
    const Math::Transform mid = a.Lerp(b, Math::Scalar(0.5f));
    CHECK(NearDeg(EyeCamera::ModelYawFromQuaternion(mid.rotation.z, mid.rotation.w), 30.0f, 0.5f));
    CHECK(Near(mid.translation.x, 1.0f));
}

UNIT_TEST(ext_eye_camera_forward_matches_engine_heading) {
    for (float h : {0.0f, 45.0f, 90.0f, -90.0f, 170.0f}) {
        const EyeCamera::Vec3 f = EyeCamera::Forward(h);
        const Math::Vector3 e = Math::Vector3::FromYawPitch(Math::Scalar(h), Math::Scalar(0.0f));
        CHECK(Near(f.x, e.x));
        CHECK(Near(f.y, e.y));
        CHECK(Near(f.z, 0.0f));
        // Heading() (the move tick's facing -> heading) inverts it.
        CHECK(NearDeg(Math::Vector3(f.x, f.y, 0.0f).Heading(), h));
    }
}

UNIT_TEST(ext_eye_camera_pose) {
    using namespace EyeCamera;
    const Vec3 feet{10.0f, -4.0f, 2.0f};
    // First person, heading 0 (facing +Y): the eye just ahead of the face, at
    // eye height, looking level along +Y.
    Pose p = ComputePose(feet, 0.0f, 1.0f, 0.0f);
    CHECK(Near(p.eye.x, 10.0f));
    CHECK(Near(p.eye.y, -4.0f + kFaceForward));
    CHECK(Near(p.eye.z, 2.0f + kEyeHeight));
    CHECK(Near(p.look.x, 10.0f));
    CHECK(Near(p.look.y, -4.0f + kLookAhead));
    CHECK(Near(p.look.z, p.eye.z));
    // Facing +X is heading -90: backed off 2 units, the eye is behind (-X) and raised.
    p = ComputePose(feet, -90.0f, 1.0f, 2.0f);
    CHECK(Near(p.eye.x, 10.0f + (kFaceForward - 2.0f)));
    CHECK(Near(p.eye.y, -4.0f));
    CHECK(Near(p.eye.z, 2.0f + kEyeHeight + 2.0f * kRisePerDistance));
    CHECK(Near(p.look.x, 10.0f + kLookAhead));
    CHECK(Near(p.look.z, 2.0f + kEyeHeight));
    // The look point is always ahead of the eye (FromLookAt never degenerates).
    CHECK(p.look.x - p.eye.x > 1.0f);
}

UNIT_TEST(ext_eye_camera_pose_child_scale) {
    using namespace EyeCamera;
    // A child is the same mesh drawn at its age scale (SetupElder's 0.66):
    // the eye and the face offset shrink with it, about the feet.
    const Vec3 feet{0.0f, 0.0f, 1.0f};
    const float child = 0.66f;
    Pose p = ComputePose(feet, 0.0f, child, 0.0f);
    CHECK(Near(p.eye.z, 1.0f + kEyeHeight * child));
    CHECK(Near(p.eye.y, kFaceForward * child));
    CHECK(Near(p.look.z, p.eye.z));
    // Backed off, the rise per unit is the same as an adult's; only the head moved.
    const Pose adult = ComputePose(feet, 0.0f, 1.0f, 1.5f);
    p = ComputePose(feet, 0.0f, child, 1.5f);
    CHECK(Near(adult.eye.z - p.eye.z, kEyeHeight * (1.0f - child)));
    CHECK(Near(p.eye.y, kFaceForward * child - 1.5f));
}

UNIT_TEST(ext_eye_camera_zoom) {
    using namespace EyeCamera;
    CHECK(Near(ZoomDistance(1.0f, 1), 1.0f - kDistanceStep));    // zoom in: closer
    CHECK(Near(ZoomDistance(1.0f, -1), 1.0f + kDistanceStep));   // zoom out: farther
    CHECK(Near(ZoomDistance(1.0f, -120), 1.0f + kDistanceStep)); // a raw wheel value counts by sign
    CHECK(Near(ZoomDistance(1.0f, 0), 1.0f));
    CHECK(Near(ZoomDistance(0.1f, 1), 0.0f));                    // first person is the floor
    CHECK(Near(ZoomDistance(kMaxDistance, -1), kMaxDistance));   // the over-the-shoulder cap
}

UNIT_TEST(ext_eye_camera_smoothing) {
    using namespace EyeCamera;
    CHECK(Near(BlendFactor(0.0f, 0.1f), 0.0f));
    CHECK(Near(BlendFactor(0.1f, 0.0f), 1.0f));
    CHECK(Near(BlendFactor(0.1f, 0.1f), 1.0f - std::exp(-1.0f)));

    // Not valid yet: the first step snaps.
    Smooth s;
    Step(s, Vec3{1.0f, 2.0f, 3.0f}, 40.0f, 1.0f, 0.016f);
    CHECK(s.valid);
    CHECK(Near(s.feet.x, 1.0f) && Near(s.heading, 40.0f) && Near(s.distance, 1.0f));

    // Frame-rate independent: two half steps == one whole step.
    Smooth a, b;
    Snap(a, Vec3{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f);
    b = a;
    Step(a, Vec3{1.0f, 0.0f, 0.0f}, 60.0f, 2.0f, 0.1f);
    Step(b, Vec3{1.0f, 0.0f, 0.0f}, 60.0f, 2.0f, 0.05f);
    Step(b, Vec3{1.0f, 0.0f, 0.0f}, 60.0f, 2.0f, 0.05f);
    CHECK(Near(a.feet.x, b.feet.x, 1e-4f));
    CHECK(Near(a.heading, b.heading, 1e-2f));
    CHECK(Near(a.distance, b.distance, 1e-4f));
    // It moved part of the way, not all of it.
    CHECK(a.feet.x > 0.5f && a.feet.x < 1.0f);
    CHECK(a.heading > 0.0f && a.heading < 60.0f);

    // A turn across +-180 takes the short way round (170 -> -170 is +20).
    Smooth t;
    Snap(t, Vec3{0.0f, 0.0f, 0.0f}, 170.0f, 0.0f);
    Step(t, Vec3{0.0f, 0.0f, 0.0f}, -170.0f, 0.0f, 0.05f);
    CHECK(t.heading > 170.0f || t.heading < -170.0f);
    for (int i = 0; i < 200; ++i) Step(t, Vec3{0.0f, 0.0f, 0.0f}, -170.0f, 0.0f, 0.016f);
    CHECK(NearDeg(t.heading, -170.0f, 0.05f));

    // A jump past kSnapDistance (a teleport, a reload) snaps.
    Smooth j;
    Snap(j, Vec3{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f);
    Step(j, Vec3{kSnapDistance + 1.0f, 0.0f, 0.0f}, 90.0f, 1.0f, 0.016f);
    CHECK(Near(j.feet.x, kSnapDistance + 1.0f));
    CHECK(Near(j.heading, 90.0f));
}
