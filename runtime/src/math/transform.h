#pragma once
// ============================================================================
// runtime/src/math/transform.h
// ============================================================================
// Wave 1 / Lane 2 of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2).
// Owns Math::Matrix, Math::Quaternion, Math::Transform, Math::Transform2,
// Math::Transform3. See transform.cpp for the ported functions and retail
// addresses.
// ============================================================================

// Registers every function this file ports with RegisterRuntimeExport().
// Called by Lane 1's RtMathPortInit();
// do NOT read the env var here.
void RtMathTransformRegister();
