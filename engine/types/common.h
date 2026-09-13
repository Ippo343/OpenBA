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
// ------------------------------------------------------------
// ENDGAME B.3.1 step 1: these are no longer PODs declared here. They are the
// real classes Math::Vector2/Vector3/Vector4 in runtime/types/math/Math.h,
// with retail's methods on them; the names below are transitional aliases so
// the ~800 sites that spell them at global scope keep compiling. Step 2
// converts the callers and deletes the aliases.
//
// Math.h includes this header back (for Time / PostCallback / the opaque
// forward declarations), which is why the include sits inside the guard and
// why the aliases come after it.
// ============================================================
#include "../../runtime/types/math/Math.h"

using Vector2 = Math::Vector2;
static_assert(sizeof(Vector2) == 8, "Vector2 must be 8 bytes");

using Vector3 = Math::Vector3;
static_assert(sizeof(Vector3) == 12, "Vector3 must be 12 bytes");

using Vector4 = Math::Vector4;
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
