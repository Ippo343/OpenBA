#pragma once
// ============================================================================
// code/Game/Game/PlayerController.h — ENDGAME B.4 W0-H2
//
// The 0x10C-byte in-game camera controller at GameUpdateLoop+0xE0. Load builds
// it with MemAlloc(0x10c, 4) + ctor 0x1400985b0(this, frustum, yaw, pitch,
// distance) (Load passes 0, -40, 25).
//
// CLASS NAME NOT SETTLED (the tree's name; the profile exe was not open).
// No vtable: a plain struct built in raw memory.
//
// FIELDS (retail ctor 0x1400985b0, helper 0x1400983f0):
//   +0x00 Math::Transform  Transform::Transform(+0); 0x1400983f0 writes it from
//                          Transform::FromLookAt(target - dir(yaw,pitch) * distance)
//                          — raw storage (B.4 H5: the member's ctor would run)
//   +0x1C float[3] target  the look-at point (ctor 0; Load's start-town step
//                          writes the spawn tile's position here)
//   +0x28 float yaw        Vector3::FromYawPitch(+0x28, +0x2C) in 0x1400983f0
//   +0x2C float pitch
//   +0x30 float distance   the eye's distance back from the target
//   +0x34 Graphics::Camera Camera::Camera(+0x34, frustum, transform, true) —
//                          raw storage (H5)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "math/Math.h"   // Math::Transform (CalcTransform's by-value return)
#include <type_traits>

struct Frustum;   // ::Frustum (volume/Volume.h)

class PlayerController {
public:
    // 0x1400985b0 — the ctor Load runs in the MemAlloc(0x10c, 4) block. Builds
    // the transform and the camera in the raw storage below (bodies in
    // gameupdateloop_load.cpp).
    PlayerController(const ::Frustum& frustum, float yaw, float pitch, float distance);

    // 0x1400983f0 — the look-at transform from (target, yaw, pitch, distance):
    // stored into +0x00 and returned (retail's hidden return slot).
    Math::Transform CalcTransform();
    // 0x140098660 — rebuilds the camera's perspective frustum from the back
    // buffer's aspect (Load step 33, Draw).
    void UpdateCamera(float fovY, float nearZ, float farZ);

    uint8_t transform[0x1C];     // +0x00 Math::Transform (raw, H5)
    float   targetX;             // +0x1C look-at target
    float   targetY;             // +0x20
    float   targetZ;             // +0x24
    float   yaw;                 // +0x28
    float   pitch;               // +0x2C
    float   distance;            // +0x30
    uint8_t camera[0xD8];        // +0x34 Graphics::Camera (raw, H5)
};

static_assert(std::is_trivially_destructible<PlayerController>::value,
              "retail MemFrees the controller with no destructor (Shutdown)");
static_assert(sizeof(PlayerController) == 0x10C, "PlayerController is 0x10C (Load MemAlloc(0x10c, 4))");
static_assert(alignof(PlayerController) == 4, "PlayerController is dword-aligned");
static_assert(offsetof(PlayerController, transform) == 0x00, "PlayerController::transform @ +0x00");
static_assert(offsetof(PlayerController, targetX) == 0x1C, "PlayerController::targetX @ +0x1C");
static_assert(offsetof(PlayerController, targetY) == 0x20, "PlayerController::targetY @ +0x20");
static_assert(offsetof(PlayerController, targetZ) == 0x24, "PlayerController::targetZ @ +0x24");
static_assert(offsetof(PlayerController, yaw) == 0x28, "PlayerController::yaw @ +0x28");
static_assert(offsetof(PlayerController, pitch) == 0x2C, "PlayerController::pitch @ +0x2C");
static_assert(offsetof(PlayerController, distance) == 0x30, "PlayerController::distance @ +0x30");
static_assert(offsetof(PlayerController, camera) == 0x34, "PlayerController::camera @ +0x34");
