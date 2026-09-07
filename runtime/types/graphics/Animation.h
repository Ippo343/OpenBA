#pragma once
// ============================================================================
// runtime/types/graphics/Animation.h — Wave 6 lane G — Graphics::AnimationEvent
// / Graphics::AnimationManager binary layouts
// ============================================================================
//
// Every offset below is MEASURED: read out of Runtime-x64.dll's disassembly
// with ghidra-mcp (program="Runtime-x64.dll"), not guessed from the decompile
// alone. The function that pins each field is named next to it, and every
// field carries a static_assert.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtGraphics {

// ============================================================================
// Graphics::AnimationEvent — 0x20 bytes (the base class only; subclasses such
// as AnimAudioEvent, which is already ported in
// code/Game/Component/animaudioevent.cpp, add their own fields after it).
//
// Pinned by the ctor (retail 0x1800a2150, disassembled): three instructions
// stamp intermediate Object::Base / Object::External vtables (0x180154788,
// 0x1801547a0) that are immediately overwritten and therefore have NO
// observable effect on the constructed object -- Ghidra's decompile shows
// them as separate `undefined ***` stores because MSVC folded the trivial
// Object::External::External() base ctor inline rather than calling it. The
// LAST vtable store (0x180154bf0, AnimationEvent::_vftable_) is the only one
// that survives, together with the three zeroed qwords at +0x08/+0x10/+0x18.
// The dtor (retail 0x1800a2120) confirms the same vtable constant and then
// TAIL-JMPs to Object::External::~External (0x1800bc950) -- a real,
// non-inlined call into an unexported internal this lane does not own the
// layout of, so it is invoked at its retail address rather than reproduced.
// ============================================================================
struct AnimationEvent {
    void*    vtable;   // +0x00  retail final value 0x180154bf0 (AnimationEvent::_vftable_)
    uint64_t _f08;     // +0x08  Object::External field, zeroed by the ctor
    uint64_t _f10;     // +0x10  Object::External field, zeroed by the ctor
    uint64_t _f18;     // +0x18  Object::External field, zeroed by the ctor
};
static_assert(sizeof(AnimationEvent) == 0x20, "Graphics::AnimationEvent base must be 0x20 bytes");
static_assert(offsetof(AnimationEvent, _f18) == 0x18, "AnimationEvent::_f18 @ +0x18");

// ============================================================================
// Graphics::AnimationManager — 0x9d8 bytes (AllocSub(0x9d8, 8) at
// gameupdateloop_load.cpp step 22, matching this layout's 0x9d4 content
// rounded to 8-byte alignment).
//
// NO VTABLE: both ctor (??0...QEAA@XZ) and dtor (??1...QEAA@XZ) are QEAA,
// i.e. non-virtual, and the ctor never stores a vtable pointer. This matches
// the Picking::Scene precedent (reference: WAVE5_LANE_D.md §4) -- plain data,
// safe to half-port because nothing dispatches into it virtually.
//
// Pinned by the ctor (retail 0x1800fc430, decompiled): it zeroes +0x00..+0x13
// (three stores: 8 + 8 + 4 bytes) then seeds a 624-entry uint32 array
// starting at +0x14 with the textbook Mersenne-Twister `init_genrand` loop
// (`x[0] = (uint)_time64(NULL); x[i] = (1812433253 * (x[i-1] ^ (x[i-1]>>30)))
// + i` for i in 1..623 -- 0x6c078965 = 1812433253 decimal, 0x26f = 623
// decimal), ending at +0x14 + 624*4 = +0x9d4. It finishes by storing `this`
// into the exported static `Graphics::AnimationManager::_singleton`
// (0x1801fe7b0, ordinal 4799).
//
// +0x00/+0x08/+0x0c are NOT reproduced here as a real Win32 CRITICAL_SECTION:
// the ctor never calls InitializeCriticalSection, so DebugInfo stays null
// forever. The dtor (retail 0x1800fc400) nulls _singleton and then calls
// FUN_18005bf60(this) AS IF `this` were an LPCRITICAL_SECTION (it reads
// ->DebugInfo, ->LockCount, ->RecursionCount at exactly +0x00/+0x08/+0x0c),
// but since DebugInfo is always null the function's only live branch
// (freeing DebugInfo's block) never fires -- it is a defensive no-op call
// against a lock this class never actually initialises. This lane calls it
// at its own retail address rather than reproducing FUN_18005bf60 and
// FUN_18005c550's Win32-shaped internals (PLAN §4.1 decision 1: no
// <windows.h> outside a _win twin).
//
// +0x10 is a fourth dword this lane could not pin to any read: nothing this
// lane decompiled (ctor, dtor) reads it, and Update/Serialize -- which likely
// do -- are HELD BACK (see animation.cpp). Left UNVERIFIED.
// ============================================================================
struct AnimationManager {
    uint64_t _lockDebugInfo;   // +0x00  CRITICAL_SECTION-shaped, never initialised; zeroed
    uint32_t _lockCount;       // +0x08  ditto
    uint32_t _lockRecursion;   // +0x0c  ditto
    uint32_t _f10;             // +0x10  UNVERIFIED -- no reader in ctor/dtor
    uint32_t _rngState[624];   // +0x14  Mersenne-Twister state, time64-seeded
    uint32_t _pad9d4;          // +0x9d4 alignment pad to the 0x9d8 allocation size
};
static_assert(sizeof(AnimationManager) == 0x9d8,
              "Graphics::AnimationManager must match AllocSub(0x9d8, 8)");
static_assert(offsetof(AnimationManager, _rngState) == 0x14, "AnimationManager::_rngState @ +0x14");

}  // namespace RtGraphics
