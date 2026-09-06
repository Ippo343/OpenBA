#pragma once
// ============================================================================
// runtime/types/math/Math.h — binary layouts for the Math namespace
// ============================================================================
//
// Wave 1 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2).
// Every layout below was read out of Runtime-x64.dll in Ghidra, not guessed;
// the function whose decompilation pins each one is named next to it.
//
// Vector2 / Vector3 / Vector4 are NOT redefined here: they already exist in
// engine/types/common.h with the same layout and are shared by the exe port.
//
// ABI NOTE THAT GOVERNS EVERY PORT IN THIS WAVE
// ---------------------------------------------
// Every one of these classes has a user-declared constructor, so MSVC returns
// it BY MEMORY: the caller passes a hidden return-slot pointer in RDX (after
// `this` in RCX) and the callee returns that same pointer in RAX. Our ports are
// therefore written as plain free functions with the hidden slot as an explicit
// parameter, mirroring the fn_* typedefs the exe port already uses
// (engine/funcs/engine_funcs.h). Do NOT write them as C++ methods returning by
// value and hope the ABI lines up.
//
//   retail: Math::Scalar Math::Vector3::Length() const
//   ours:   Scalar* RtMath_Vector3_Length(const Vector3* self, Scalar* ret)
//           ... and it must `return ret;`
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../../../engine/types/common.h"   // Vector2, Vector3, Vector4

namespace RtMathConst {
// Float constants the DLL keeps in .rdata and that the ports must reproduce
// bit-for-bit. Addresses are Runtime-x64.dll (image base 0x180000000).
constexpr float kHalf     = 0.5f;             // 0x180161694
constexpr float kOne      = 1.0f;             // 0x18016169c
constexpr float kTwo      = 2.0f;             // 0x1801616d8
constexpr float kMinusOne = -1.0f;            // 0x18016175c
constexpr float kDegToRad = 0.01745329238474369f;  // 0x18016167c (pi/180, float)
// 0x1801617c0 is -0.0f, used as an XOR sign-flip mask, never as an addend.
constexpr uint32_t kSignMask = 0x80000000u;
// Volume::Box / Volume::Rectangle build their empty state from these.
constexpr float kBigPos = 1.0e8f;             // 0x4cbebc20
constexpr float kBigNeg = -1.0e8f;            // 0xccbebc20
}  // namespace RtMathConst

// ============================================================
// Math::Scalar — a single float in a class wrapper.
// Pinned by ?Length@Vector3@Math@@QEBA?AVScalar@2@XZ (0x1800219a0), which
// writes one float through the hidden return slot in RDX.
// ============================================================
struct Scalar {
    float v;
};
static_assert(sizeof(Scalar) == 4, "Math::Scalar must be 4 bytes");

// ============================================================
// Math::Quaternion — {x, y, z, w}, w LAST.
// Pinned by ??0Quaternion@Math@@QEAA@MMMM@Z (0x18001e260): param 1->+0x00,
// 2->+0x04, 3->+0x08, 4->+0x0c.
// ============================================================
struct Quaternion {
    float x, y, z, w;
};
static_assert(sizeof(Quaternion) == 16, "Math::Quaternion must be 16 bytes");

// ============================================================
// Math::Matrix — 16 floats, column-major storage in the sense used by
// Transform::AsMatrix (0x18001e5f0): the rotation basis lands in
// m[0..2], m[4..6], m[8..10]; the translation in m[12..14]; m[3]/m[7]/m[11]
// are cleared and m[15] is 1.0f.
// ============================================================
struct Matrix {
    float m[16];
};
static_assert(sizeof(Matrix) == 64, "Math::Matrix must be 64 bytes");

// ============================================================
// Math::Transform — a rigid transform stored as quaternion + translation.
// Pinned by ??0Transform@Math@@QEAA@XZ (0x180015080): zeroes +0x00..+0x08,
// writes 1.0f at +0x0c (quaternion w) and zeroes +0x10..+0x1b; and by the copy
// ctor (0x180015020) which copies exactly seven dwords.
// ============================================================
struct Transform {
    Quaternion rotation;      // +0x00
    Vector3    translation;   // +0x10
};
static_assert(sizeof(Transform) == 0x1c, "Math::Transform must be 0x1c bytes");
static_assert(offsetof(Transform, translation) == 0x10, "Transform::translation @ +0x10");

// ============================================================
// Math::Transform2 — 2x2 rotation matrix + 2D translation.
// Pinned by ??0Transform2@Math@@QEAA@AEBVScalar@1@AEBVVector2@1@@Z
// (0x18007dca0): +0x00 = cos, +0x04 = sin, +0x08 = -sin, +0x0c = cos,
// +0x10 = the Vector2 offset (written first, as one qword).
// ============================================================
struct Transform2 {
    Vector2 rotX;             // +0x00  { cos,  sin }
    Vector2 rotY;             // +0x08  { -sin, cos }
    Vector2 translation;      // +0x10
};
static_assert(sizeof(Transform2) == 0x18, "Math::Transform2 must be 0x18 bytes");
static_assert(offsetof(Transform2, translation) == 0x10, "Transform2::translation @ +0x10");

// ============================================================
// Math::Transform3 — full 3x3 basis + translation.
// Pinned by ?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ (0x1800346e0):
// matrix[0,1,2] <- this[+0x00,+0x04,+0x08]; matrix[4,5,6] <- +0x0c,+0x10,+0x14;
// matrix[8,9,10] <- +0x18,+0x1c,+0x20; matrix[12,13,14] <- +0x24,+0x28,+0x2c.
// So the three basis vectors are contiguous Vector3s and translation follows.
// ============================================================
struct Transform3 {
    Vector3 axisX;            // +0x00
    Vector3 axisY;            // +0x0c
    Vector3 axisZ;            // +0x18
    Vector3 translation;      // +0x24
};
static_assert(sizeof(Transform3) == 0x30, "Math::Transform3 must be 0x30 bytes");
static_assert(offsetof(Transform3, axisY) == 0x0c, "Transform3::axisY @ +0x0c");
static_assert(offsetof(Transform3, axisZ) == 0x18, "Transform3::axisZ @ +0x18");
static_assert(offsetof(Transform3, translation) == 0x24, "Transform3::translation @ +0x24");
