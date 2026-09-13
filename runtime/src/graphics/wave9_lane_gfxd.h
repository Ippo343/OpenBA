#pragma once
// ============================================================================
// runtime/src/graphics/wave9_lane_gfxd.h — Wave 9 lane GFXD
// ============================================================================
//
// The bottom half of the `Graphics::Renderer` class lane: `Renderer::Reset`,
// `Renderer::AddScene`, the two `Graphics::Camera` constructors they use, and
// the five unexported `Collection::Array<T>` instantiations underneath them.
//
// One thing is public: `RtGraphicsWave9GfxdRegister()`, the wave init called
// from RtGraphicsPortInit().
// ============================================================================

// The wave init. Returns the number of EXPORT registrations it made (address
// registrations are counted separately by the caller's log line).
int RtGraphicsWave9GfxdRegister();
