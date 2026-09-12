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
// ----------------------------------------------------------------------------
// CORRECTION, WAVE 23 LANE GFXBODY: +0x00/+0x08/+0x0c ARE NOT A LOCK.
//
// This header used to call them a "CRITICAL_SECTION-shaped, never initialised"
// triple and called the dtor's helper "a defensive no-op call against a lock
// this class never actually initialises". Both statements are WRONG, and the
// wrongness was invisible while Update and Serialize were held back, because
// nothing else in the tree read those twelve bytes.
//
// They are a `Collection::Array<AnimationGroup>` — _data / _size / _capacity —
// and the proof is in `?Update@AnimationManager@Graphics@@` (0x1800fc240),
// which this lane disassembled:
//
//     1800fc268:  CMP dword ptr [RCX + 0x8],R14D   ; +0x08 is a LOOP BOUND
//     1800fc26c:  JLE <return>
//     1800fc2a0:  MOV RBP,qword ptr [R15]          ; +0x00 is an ARRAY BASE
//                                                   ; (elements of stride 0x30)
//
// A LockCount is not a loop bound and a DebugInfo pointer is not an array base.
// The dtor's helper, read the other way round, says the same thing: FUN_18005bf60
// destroys each element, frees the block at +0x00, then zeroes +0x00/+0x08/+0x0c
// — that is `Collection::Array<T>::~Array`, and Ghidra typed its parameter
// LPCRITICAL_SECTION only because the free happens under the ALLOCATOR's own
// critical section. So the dtor's call is load-bearing whenever any group has
// been registered, not a no-op.
//
// NO BEHAVIOURAL CHANGE FOLLOWS. animation.cpp's ported ctor zeroes the same
// twelve bytes it always did (a zeroed Array is an empty Array) and its ported
// dtor still delegates to the real helper at its retail address, which is
// correct under either reading. Only the NAMES and the explanation change —
// and they are what a future lane would have built on.
//
// The Mersenne-Twister block at +0x14 is unaffected and stands as decompiled.
// ----------------------------------------------------------------------------
//
// PLAN §4.1 decision 1 (no <windows.h> outside a _win twin) is still why
// animation.cpp calls FUN_18005bf60 / FUN_18005c550 at their retail addresses
// rather than reproducing them.
//
// *** DO-NOT-DELETE NOTE — checked by tools/check_standing_notes.py. ***
// A cleanup pass that removes this paragraph, or the `UNVERIFIED` marker on
// _f10 below, fails that checker. It is here because the note has already been
// re-opened once on a guess.
//
// +0x10 is a fourth dword no body this project has read touches: not the ctor,
// not the dtor, and not Update (which reads only +0x00 and +0x08). Left
// UNVERIFIED.
//
// WAVE 23 lane GFXBODY's residual R4 asked whoever ported
// `?Serialize@AnimationManager@Graphics@@` (0x1800fc190) to settle this field,
// on the guess that Serialize probably touches it. WAVE 24 lane GFXMAT ported
// that body and ANSWERED R4: **no**. The only AnimationManager fields it reads
// are +0x00 (_data) and +0x08 (_size) -- see the transcription above
// `RtGraphics_AnimationManager_Serialize` in
// runtime/src/graphics/wave24_lane_gfxmat.cpp. So the premise that used to
// stand here -- "Serialize is still unported" -- is no longer true, and the
// VERDICT is unchanged: +0x10 stays UNVERIFIED, and the body that was named as
// its answer is not its answer. Do not re-open it on the same guess.
// ============================================================================
struct AnimationManager {
    void*    _groupData;       // +0x00  Collection::Array<AnimationGroup>::_data (stride 0x30)
    int32_t  _groupCount;      // +0x08  ::_size      -- Update's outer loop bound
    int32_t  _groupCapacity;   // +0x0c  ::_capacity
    uint32_t _f10;             // +0x10  UNVERIFIED -- no reader in ctor/dtor/Update
    uint32_t _rngState[624];   // +0x14  Mersenne-Twister state, time64-seeded
    uint32_t _pad9d4;          // +0x9d4 alignment pad to the 0x9d8 allocation size
};
static_assert(sizeof(AnimationManager) == 0x9d8,
              "Graphics::AnimationManager must match AllocSub(0x9d8, 8)");
static_assert(offsetof(AnimationManager, _rngState) == 0x14, "AnimationManager::_rngState @ +0x14");
static_assert(offsetof(AnimationManager, _groupData) == 0x00, "AnimationManager::_groupData @ +0x00");
static_assert(offsetof(AnimationManager, _groupCount) == 0x08, "AnimationManager::_groupCount @ +0x08");
static_assert(offsetof(AnimationManager, _groupCapacity) == 0x0c, "AnimationManager::_groupCapacity @ +0x0c");

}  // namespace RtGraphics
