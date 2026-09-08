#pragma once
// ============================================================================
// runtime/src/graphics/wave9_lane_gfxd.h — Wave 9 lane GFXD
// ============================================================================
//
// The bottom half of the `Graphics::Renderer` class lane: `Renderer::Reset`,
// `Renderer::AddScene`, the two `Graphics::Camera` constructors they use, and
// the five unexported `Collection::Array<T>` instantiations underneath them.
//
// Only two things are public. `RtGraphicsWave9GfxdRegister()` is the wave init,
// called from RtGraphicsPortInit(). `ProbeGate()` is the deferred half of this
// lane's oracle — see the PROBE banner in the .cpp for why it exists and why it
// cannot run at init.
// ============================================================================

namespace rtgfxd {

// Armed by RtGraphicsWave9GfxdRegister(); consumed by ProbeGate().
extern bool g_probePending;

// Runs the deferred (allocating) probe arms the first time it is called after
// arming. Cheap and branch-predictable once fired: one load and a not-taken
// branch. Placed at live, provably-post-Engine::Startup call sites.
void ProbeGate();

}  // namespace rtgfxd

// The wave init. Returns the number of EXPORT registrations it made (address
// registrations are counted separately by the caller's log line).
int RtGraphicsWave9GfxdRegister();
