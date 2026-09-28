#pragma once
// Resolved function pointers for Runtime-x64.dll internal calls.
// These are for functions called BY reimplemented Runtime DLL code (internal calls).
// Function pointers called BY exe code (like ImageBuffer methods the UI panels call)
// stay in engine/funcs/engine_funcs.h per existing convention.
//
// Resolved by RuntimeFuncsInit() after ResolveInit() succeeds.

#include <cstdint>

// Resolve all Runtime DLL internal function pointers.
// Call after ResolveInit(). Returns true on success.
bool RuntimeFuncsInit();

// ==== B3R RT-UI: a Runtime body reached by name from runtime/src/ui ====
// Declared here (B3R_RULES R5: the defining module's header,
// runtime/types/graphics/Camera.h, is outside RT-UI's file set). Copied from
// its definition in runtime/src/graphics/renderer.cpp.
struct Frustum;                                   // runtime/types/volume/Volume.h
namespace Math { class Transform3; }              // runtime/types/math/Math.h
namespace RtGraphics { struct Camera; }           // runtime/types/graphics/Camera.h
// ---- graphics/renderer.cpp
extern "C" RtGraphics::Camera* RtGraphics_Camera_Ctor(RtGraphics::Camera* self, const Frustum* frustum, const Math::Transform3* cameraToWorld, bool swapZUpToYUp);  // 0x1800fe960 ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z
// ==== end B3R RT-UI ====
