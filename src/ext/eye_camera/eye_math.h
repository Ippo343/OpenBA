// ============================================================================
// src/ext/eye_camera/eye_math.h -- ext.eye_camera: the pure math
// ============================================================================
// NOT RETAIL. The eye-level citizen camera's arithmetic, standard C++ only, so
// tests/unit/ext_eye_camera_test.cpp links it without the world. The engine
// glue (which citizen, its model's interpolated transform, the camera write) is
// eye_camera.cpp.
//
// CONVENTIONS (the engine's, read from the bodies that build them):
//   * Z is up; a heading H (degrees) faces Math::Vector3::FromYawPitch(H, 0)
//     = (-sin H, cos H, 0), and Math::Vector3::Heading() inverts it.
//   * A citizen's model rotation is a pure Z rotation built by the move tick
//     (CharacterComponent_MoveTick_1094c0, citizenai_executor.cpp) as
//     q = (0, 0, -sin(Y/2), cos(Y/2)) with Y = heading + CharacterDescription::_f24
//     (the description's model yaw offset); ModelYawFromQuaternion inverts it.
//   * Units: world units. A tile is one unit, and the shipped citizen mesh
//     (data0.pkg Models\Citizen\CitizenMaleMesh_resource.crs, its bounding box
//     read from the compiled resource) stands z 0.002 .. 0.946: a person is
//     ~0.95 units tall, so one unit is ~1.85 m and a 1.6 m eye is ~0.88 units.
//   * The model scale: a citizen's age scale is the uniform factor
//     CitizenComponent::SetupYoung / SetupAdult / SetupElder (0x140115110 /
//     0x1401151a0 / 0x140115230, citizenaicomponent.cpp) store into the
//     model's MeshInstance::_color[0..2] (+0x10c..+0x114): 1.0, 0.83 and 0.66
//     (a child). MeshInstance::AddDrawItems hands it to the vertex shader as
//     the w column of the instance transform (Context::SetTransformScale), so
//     the drawn mesh is the 0.946-tall mesh times it, about the feet. The eye
//     height and the face offset are kEyeHeight / kFaceForward times it.
// ============================================================================
#pragma once

namespace EyeCamera {

struct Vec3 {
    float x, y, z;
};

// ---- the constants (the hand-in, docs/extensions/handins/EYECAM.md §4) ----
constexpr float kEyeHeight       = 0.88f;  // feet -> eyes at model scale 1 (0.93 of the 0.946 mesh height)
constexpr float kFaceForward     = 0.10f;  // the eye ahead of the body axis at model scale 1 (face front ~0.09)
constexpr float kMaxDistance     = 3.0f;   // the over-the-shoulder limit (~5.5 m)
constexpr float kDefaultDistance = 0.9f;   // the first follow's distance (close third person)
constexpr float kDistanceStep    = 0.3f;   // one wheel notch / zoom key press
constexpr float kRisePerDistance = 0.30f;  // the camera rises 0.3 per unit it backs off
constexpr float kLookAhead       = 6.0f;   // the look-at point ahead of the eyes
constexpr float kNearPlane       = 0.1f;   // retail's 2.0 clips everything at eye height
constexpr float kPositionTau     = 0.06f;  // seconds: feet position smoothing
constexpr float kHeadingTau      = 0.25f;  // seconds: heading smoothing (a turn does not snap)
constexpr float kDistanceTau     = 0.10f;  // seconds: wheel distance smoothing
constexpr float kSnapDistance    = 4.0f;   // a jump farther than this (teleport) snaps
constexpr float kMaxFrameGap     = 0.5f;   // seconds without a frame: re-engage with a snap
constexpr float kMinClearance    = 0.15f;  // camera height kept above the terrain

// (-180, 180]
float WrapDegrees(float degrees);

// The model yaw Y (degrees) of a pure Z rotation q = (0, 0, -sin(Y/2), cos(Y/2)).
// Sign-safe (q and -q give the same yaw) and scale-safe (a lerped, unnormalised q).
float ModelYawFromQuaternion(float qz, float qw);

// FromYawPitch(heading, 0): (-sin H, cos H, 0).
Vec3 Forward(float headingDegrees);

// One zoom step: zoomDir > 0 is zoom IN (retail's sign, camera_input.cpp), which
// brings the eye closer; the result is clamped to [0, kMaxDistance].
float ZoomDistance(float distance, int zoomDir);

// The eye and its look-at point for a citizen standing at `feet` facing
// `headingDegrees`, drawn at `modelScale` (the age scale, see CONVENTIONS: the
// eye sits at kEyeHeight * modelScale, kFaceForward * modelScale ahead).
// distance 0 = first person (the eye just ahead of the face); > 0 backs the
// eye off along the heading and lifts it kRisePerDistance per unit.
struct Pose {
    Vec3 eye;
    Vec3 look;
};
Pose ComputePose(const Vec3& feet, float headingDegrees, float modelScale, float distance);

// Exponential smoothing, frame-rate independent: 1 - exp(-dt / tau), 0..1.
float BlendFactor(float dt, float tau);

// The smoothed inputs of ComputePose.
struct Smooth {
    bool  valid    = false;
    Vec3  feet     = {0.0f, 0.0f, 0.0f};
    float heading  = 0.0f;
    float distance = 0.0f;
};
void Snap(Smooth& s, const Vec3& feet, float heading, float distance);
// Advance toward the targets by dt seconds. Not valid yet, or a jump past
// kSnapDistance: snap. The heading takes the short way round.
void Step(Smooth& s, const Vec3& feet, float heading, float distance, float dt);

}  // namespace EyeCamera
