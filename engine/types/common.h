#pragma once
// Primitive engine types used across all reimplemented code.
// These match the binary layout of Banished's engine types.
// Build 170910 (2017-09-10), MSVC x64.

#include <cstdint>
#include <cstddef>

// ============================================================
// System::Time — 8-byte tick counter
// ============================================================
struct Time {
    int64_t ticks;
};
static_assert(sizeof(Time) == 8, "Time must be 8 bytes");

// ============================================================
// Math types
// ============================================================
struct Vector2 {
    float x, y;
};
static_assert(sizeof(Vector2) == 8, "Vector2 must be 8 bytes");

struct Vector3 {
    float x, y, z;
};
static_assert(sizeof(Vector3) == 12, "Vector3 must be 12 bytes");

struct Vector4 {
    float x, y, z, w;
};
static_assert(sizeof(Vector4) == 16, "Vector4 must be 16 bytes");

// ============================================================
// Callback entry used by MainLoop post-update/post-draw lists.
// Array of these, each 0x10 bytes.
// ============================================================
struct PostCallback {
    void* funcPtr;   // +0x00: function pointer
    void* userData;   // +0x08: user data passed to callback
};
static_assert(sizeof(PostCallback) == 0x10, "PostCallback must be 0x10 bytes");

// ============================================================
// Opaque forward declarations — filled in as we decompile.
// Use pointers to these; never dereference until the type is defined.
// ============================================================
struct VideoContext;       // Video::Context (0x2168 bytes, partially known)
struct EngineInstance;     // Engine::Engine (large, back-pointer target from MainLoop)
struct InputDevice;        // Input::Device
struct AudioDevice;        // Audio::Device
struct ApplicationInstance; // Application::Application
