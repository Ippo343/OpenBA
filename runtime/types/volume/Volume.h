#pragma once
// ============================================================================
// runtime/types/volume/Volume.h — binary layouts for the Volume namespace
// ============================================================================
//
// Wave 1, Lane 3 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Every layout below was read out of Runtime-x64.dll in Ghidra, not guessed;
// the function whose decompilation pins each one is named next to it.
//
// Vector2 / Vector3 / Vector4 are NOT redefined here: they already exist in
// engine/types/common.h with the same layout and are shared by the exe port.
// Scalar/Transform3 come from runtime/types/math/Math.h (Lane 1/2 of this wave).
//
// ABI NOTE — see runtime/types/math/Math.h for the full explanation. Every one
// of these classes has a user-declared constructor, so MSVC returns it BY
// MEMORY: the caller passes a hidden return-slot pointer in RDX (after `this`
// in RCX) and the callee returns that same pointer in RAX. Ports here are
// plain free functions with the hidden slot as an explicit parameter.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../../../engine/types/common.h"   // Vector2, Vector3, Vector4
#include "../math/Math.h"                   // Scalar, Transform3

// ============================================================
// Volume::Box — axis-aligned bounding box.
// Pinned by ??0Box@Volume@@QEAA@XZ (0x180011ea0): default ctor fills `min`
// with 0x4cbebc20 (= 1.0e8f, RtMathConst::kBigPos) and `max` with 0xccbebc20
// (= -1.0e8f, RtMathConst::kBigNeg), so the box is empty and the first
// Extend() call always wins on both ends.
// ============================================================
struct Box {
    Vector3 min;   // +0x00
    Vector3 max;   // +0x0c
};
static_assert(sizeof(Box) == 0x18, "Volume::Box must be 0x18 (24) bytes");
static_assert(offsetof(Box, max) == 0x0c, "Volume::Box::max @ +0x0c");

// ============================================================
// Volume::Rectangle — axis-aligned 2D rectangle.
// Pinned by ??0Rectangle@Volume@@QEAA@XZ (0x18005ddd0), same empty-box
// sentinel values as Box: min = +1.0e8f, max = -1.0e8f.
// ============================================================
struct Rectangle {
    Vector2 min;   // +0x00
    Vector2 max;   // +0x08
};
static_assert(sizeof(Rectangle) == 0x10, "Volume::Rectangle must be 0x10 (16) bytes");
static_assert(offsetof(Rectangle, max) == 0x08, "Volume::Rectangle::max @ +0x08");

// ============================================================
// Volume::Frustum — six clip planes plus a projection-kind flag.
//
// Pinned by ??0Frustum@Volume@@QEAA@AEBVScalar@Math@@000@Z (0x1800fa1e0, the
// 4-Scalar near/far/fovX/fovY perspective ctor) and
// ??0Frustum@Volume@@QEAA@_NAEBVScalar@Math@@11111@Z (0x1800fa3d0, the 6-arg
// ctor that takes the flag explicitly as its first parameter). Both zero the
// whole object (0x00..0x5f) before filling it in.
//
// Each plane is a Vector4 {x,y,z,w} where (x,y,z) is the outward plane normal
// and w is the plane's `d` term, tested as dot(normal, p) - d. Plane order as
// built by the perspective ctor (forward = +Y, right = +X, up = +Z; apex at
// the origin for the four side planes, whose w is always 0):
//   [0] near  {0,  1,        0,        near}
//   [1] far   {0, -1,        0,       -far}
//   [2] side  { cosHalfFovX,  sinHalfFovX, 0, 0}
//   [3] side  {-cosHalfFovX,  sinHalfFovX, 0, 0}
//   [4] side  {0,  sinHalfFovY, -cosHalfFovY, 0}
//   [5] side  {0,  sinHalfFovY,  cosHalfFovY, 0}
//
// +0x60 is a single byte, read/written as a bool by every non-ctor method
// (operator*, Contains via a straight copy, GetCorners, GetScreenDirection).
// It is FALSE for the perspective ctor (0x1800fa1e0) and comes directly from
// the caller's first argument in the 6-Scalar ctor (0x1800fa3d0): when false
// that ctor normalizes four side-plane directions computed from slope ratios
// (an asymmetric/off-axis PERSPECTIVE frustum); when true it copies the six
// scalar args straight into the plane fields with no normalization — i.e. an
// axis-aligned ORTHOGRAPHIC volume, where the plane "d" terms already sit at
// unit-normal offsets. GetCorners/GetScreenDirection branch on the same byte:
// the false (perspective) path divides the near-plane offset through the
// side planes' ratios to find the near-plane corners then normalizes toward
// the far plane; the true (orthographic) path reads the corner coordinates
// straight out of the plane "d" terms with no perspective divide. So this
// byte means "is this frustum orthographic" (true = orthographic, false =
// perspective); named accordingly below.
//
// sizeof: fields run through +0x60 inclusive (0x61 bytes used); the struct's
// natural alignment is 4 (from the Vector4/float members), so the compiler
// pads the tail to a multiple of 4 -> 0x64 (100) bytes. No retail call site
// was found that depends on any byte beyond +0x60, so the pad bytes carry no
// meaning; they exist only to satisfy alignment.
// ============================================================
struct Frustum {
    Vector4 planes[6];       // +0x00 .. +0x5f, one Vector4 {x,y,z,d} per plane
    bool    isOrthographic;  // +0x60
    // +0x61..+0x63: compiler alignment padding, not read by any known caller.
};
static_assert(sizeof(Frustum) == 0x64, "Volume::Frustum must be 0x64 (100) bytes");
static_assert(offsetof(Frustum, isOrthographic) == 0x60, "Volume::Frustum::isOrthographic @ +0x60");
