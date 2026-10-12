// ============================================================================
// src/ext/eye_camera/eye_math.cpp -- ext.eye_camera: the pure math
// ============================================================================
// NOT RETAIL. eye_math.h has the conventions and the constants.
// ============================================================================

#include "ext/eye_camera/eye_math.h"

#include <cmath>

namespace EyeCamera {

namespace {
constexpr float kDegToRad = 0.017453292f;
constexpr float kRadToDeg = 57.29578f;
}  // namespace

float WrapDegrees(float degrees) {
    float d = std::fmod(degrees, 360.0f);
    if (d <= -180.0f) d += 360.0f;
    else if (d > 180.0f) d -= 360.0f;
    return d;
}

float ModelYawFromQuaternion(float qz, float qw) {
    // q = (0, 0, -sin(Y/2), cos(Y/2)): Y/2 = atan2(-qz, qw). Negating both
    // components adds 180 to the half angle, 360 to Y: the same yaw.
    return WrapDegrees(2.0f * std::atan2(-qz, qw) * kRadToDeg);
}

Vec3 Forward(float headingDegrees) {
    const float r = headingDegrees * kDegToRad;
    return Vec3{-std::sin(r), std::cos(r), 0.0f};
}

float ZoomDistance(float distance, int zoomDir) {
    float d = distance;
    if (zoomDir > 0) d -= kDistanceStep;
    else if (zoomDir < 0) d += kDistanceStep;
    if (d < 0.0f) d = 0.0f;
    if (d > kMaxDistance) d = kMaxDistance;
    return d;
}

Pose ComputePose(const Vec3& feet, float headingDegrees, float eyeHeight, float distance) {
    const Vec3 f = Forward(headingDegrees);
    const Vec3 head{feet.x, feet.y, feet.z + eyeHeight};
    const float back = kFaceForward - distance;   // ahead of the face at 0, behind the head past it
    Pose p;
    p.eye  = Vec3{head.x + f.x * back, head.y + f.y * back,
                  head.z + distance * kRisePerDistance};
    p.look = Vec3{head.x + f.x * kLookAhead, head.y + f.y * kLookAhead, head.z};
    return p;
}

float BlendFactor(float dt, float tau) {
    if (dt <= 0.0f) return 0.0f;
    if (tau <= 0.0f) return 1.0f;
    return 1.0f - std::exp(-dt / tau);
}

void Snap(Smooth& s, const Vec3& feet, float heading, float distance) {
    s.valid = true;
    s.feet = feet;
    s.heading = WrapDegrees(heading);
    s.distance = distance;
}

void Step(Smooth& s, const Vec3& feet, float heading, float distance, float dt) {
    const float dx = feet.x - s.feet.x, dy = feet.y - s.feet.y, dz = feet.z - s.feet.z;
    if (!s.valid || dx * dx + dy * dy + dz * dz > kSnapDistance * kSnapDistance) {
        Snap(s, feet, heading, distance);
        return;
    }
    const float ap = BlendFactor(dt, kPositionTau);
    s.feet.x += dx * ap;
    s.feet.y += dy * ap;
    s.feet.z += dz * ap;
    s.heading = WrapDegrees(s.heading + WrapDegrees(heading - s.heading) * BlendFactor(dt, kHeadingTau));
    s.distance += (distance - s.distance) * BlendFactor(dt, kDistanceTau);
}

}  // namespace EyeCamera
