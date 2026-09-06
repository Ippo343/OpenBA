#pragma once
// ============================================================================
// runtime/types/system/Value.h — System::Random / Time / Color layouts
// ============================================================================
//
// Wave 2, Lane B of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Every layout below was read out of Runtime-x64.dll in Ghidra (image base
// 0x180000000, decompiled 2026-09-06); the function that pins it is named.
//
// ABI NOTE (same rule as runtime/types/math/Math.h)
// --------------------------------------------------
// Random and Time each have a user-declared constructor, so a by-value return
// of either (there are none in this lane's scope, but future ports must not
// assume otherwise) would be returned BY MEMORY with the hidden slot as an
// explicit argument. Every member port here is a plain free function.
//
// System::DateTime and System::Configuration are NOT modeled below.
// DateTime::AsString is the only DateTime function in this lane's scope and
// treats `this` as opaque (it forwards to the DLL's own DateAsString/
// TimeAsString), so no layout is needed. Configuration has no instance data
// (every member in scope is `static`). See value_types.cpp for the reasoning
// on why System::String is likewise never modeled here: it is Lane A's
// (runtime/types/system/String.h), and every String this file produces is
// built by calling the DLL's own String::Set/Format/operator+=/Release
// through RtOriginal<>(), never by touching its refcount/length header.
// ============================================================================

#include <cstdint>
#include <cstddef>

// ============================================================
// System::Random — a Mersenne-Twister-shaped PRNG (NOT the textbook MT19937:
// the tempering constants differ, see below). Pinned by the default ctor
// (??0Random@System@@QEAA@XZ, 0x1800c7f20) and NextGroup
// (?NextGroup@Random@System@@AEAAXXZ, 0x1800c7dd0):
//   +0x00: int32  index   — position in mt[], drives the "regenerate every
//                           624 draws" cadence; starts at 0, which is also
//                           what forces the first-ever Rand() to regenerate.
//   +0x04: uint32 mt[624] — the state array (624*4 = 2496 bytes).
// Total size 4 + 2496 = 2500 (0x9c4) bytes.
//
// Seeding (both ctors) is the standard MT19937 init_genrand recurrence
// (mt[i] = 1812433253*(mt[i-1]^(mt[i-1]>>30)) + i), but NextGroup's tempering
// after the twist uses 0xff3a58ad/0xffffdf8c where textbook MT19937 uses
// 0x9d2c5680/0xefc60000 — Shining Rock's own constants, transcribed from
// ?Rand@Random@System@@QEAAIXZ (0x1800c7f90), not assumed from the textbook.
// ============================================================
struct Random {
    int32_t  index;
    uint32_t mt[624];
};
static_assert(offsetof(Random, mt) == 4, "Random::mt must sit at +0x04");
static_assert(sizeof(Random) == 2500, "System::Random must be 2500 (0x9c4) bytes");

// ============================================================
// System::Time — a tick count at Timer's resolution.
// Pinned by ??0Time@System@@QEAA@M@Z (0x1800078c0): writes one qword.
//   +0x00: int64 ticks
// The float<->ticks conversion is Timer::SecondsToTicks/TicksToSeconds
// (Timer is out of this lane's scope and not ported here); our ctor and
// AsSeconds delegate to those two exports via RtOriginal<>() rather than
// duplicating Timer::_secondsPerTick, which is a runtime-computed global
// (QueryPerformanceFrequency-derived) and not a compile-time constant.
// ============================================================
struct Time {
    int64_t ticks;
};
static_assert(sizeof(Time) == 8, "System::Time must be 8 bytes");

// ============================================================
// System::Color — one packed uint32, byte-addressed B,G,R,A (little-endian,
// so as a uint32 it reads 0xAARRGGBB). Pinned by the accessors:
//   ?Blue@Color@System@@QEBAHXZ   (0x18001fe60): byte 0
//   ?Green@Color@System@@QEBAHXZ  (0x18001fe50): byte 1
//   ?Red@Color@System@@QEBAHXZ    (0x18001fe40): byte 2
//   ?Alpha@Color@System@@QEBAHXZ  (0x18001fe30): byte 3
// Normalized* divide by 255.0f (DAT_180161718, confirmed by reading the
// bytes: 0x437f0000 = 255.0f).
// ============================================================
struct Color {
    uint32_t value;
};
static_assert(sizeof(Color) == 4, "System::Color must be 4 bytes");
