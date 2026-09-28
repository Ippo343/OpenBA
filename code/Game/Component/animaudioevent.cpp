// code/Game/Component/animaudioevent.cpp
// AnimAudioEvent — a Graphics::AnimationEvent subclass (0x30 bytes) loaded as
// part of the Character animation closure when an entity template with a
// Character component is deserialized (Template\Citizen.rsc).
//
// Layout (from AnimAudioEvent_Factory 0x140016f00 + Serialize 0x140087ed0):
//   +0x00 vtable (0x14015e0c0)
//   +0x08 Graphics::AnimationEvent / Object::External base
//   +0x20 External*  audio resource reference (default 0)
//   +0x28 float      play chance (default 1.0)
//
// Serialize (slot 5) reads exactly the two subclass fields: the External ref at
// +0x20 (via the generic External-pointer serializer FUN_1400019a0, which is
// stream-identical to fn_ExternalPtr_BuildSerialize / 0x140002340 — only the
// type-validation global differs, not the bytes consumed) and the float at
// +0x28. GetFileVersion (slot 3) = 2 (exe body `mov eax,2; ret`).
//
// ENDGAME B.3.8 VIS: a REAL class (AnimAudioEvent.h). Before: an 11-slot
// hand-built g_AAE_Vtable registered at 0x14015e0c0, the object built through
// the `??0AnimationEvent@Graphics@@` DOOR (lazily resolved by name) and then
// re-stamped. After: the compiler's 10-slot vtable; AnimationEvent() is the
// base's real protected ctor (block RB). Nothing reads 0x14015e0c0 back by
// address, so its RegisterExeAddr row is gone (B38 D5).

#include "Component/AnimAudioEvent.h"
#include "math/Math.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include "object/Internal.h"  // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>
#include "Game/exe_bodies.h"

// 0x140016f00 (inlined): AnimationEvent() is `??0AnimationEvent@Graphics@@`
// (External's three zero stores); the 0x14015e0c0 stamp is the compiler's;
// +0x20 = 0, +0x28 = 1.0f.
AnimAudioEvent::AnimAudioEvent() : _sound(nullptr), _chance(1.0f), _pad2c{} {}

// [5] 0x140087ed0: Serializer(stream, 0); ExternalPtr +0x20; float +0x28;
// ~Serializer; return 1. (The hand-built slot was a `void` function, so its AL
// was whatever the Serializer dtor left there.)
bool AnimAudioEvent::Serialize(void* stream) {
    alignas(8) uint8_t serBuf[24];
    ::new (static_cast<void*>(serBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // +0x20 external audio reference (stream-identical generic external-ptr serialize)
    Reimpl_ExternalPtr_Serialize_StringTable(&_sound, serBuf);
    // +0x28 float
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(_chance, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serBuf)->~Serializer();                 // B.3.4 bridge
    return true;
}

// [6] 0x140016fb0: retail destroys and rebuilds in place: a virtual
// `~T()` (slot 0, flags 0), then the ctor again on `this` (B38_RULES §2.2, D9 #10).
Object::External* AnimAudioEvent::Reconstruct() {
    this->~AnimAudioEvent();
    return new (static_cast<void*>(this)) AnimAudioEvent();
}
// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0).
void AnimAudioEvent::Destruct() { this->~AnimAudioEvent(); }
// [8] 0x140087e40. PORT GAP (retail: the audio trigger — roll a random float,
// and when it is under +0x28 play +0x20 at the event transform's position).
// RDX is the transform ApplyEvents received (0x140087e54 MOV RBX,RDX); the
// body returns nothing. Until the trigger is ported the body does nothing (the
// old `return null` stub's value was never read: ApplyEvents ignores RAX).
void AnimAudioEvent::Construct(const Math::Transform&) {}
// [9] 0x140016f60 — placement construction on `this` (the createInPlace body).
Object::External* AnimAudioEvent::ConstructInPlace() {
    return new (static_cast<void*>(this)) AnimAudioEvent();
}

// create 0x140016f00: MemAlloc(0x30, 8), then the (inlined) ctor.
void* AnimAudioEvent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AnimAudioEvent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AnimAudioEvent());
}
// createInPlace 0x140016f60: placement construction.
void* AnimAudioEvent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AnimAudioEvent());
}

DEFINE_EXE_TYPE(AnimAudioEvent, "Graphics::AnimationEvent")
