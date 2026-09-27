// code/Game/Component/ambientenvironmentcomponent_serialize.cpp
// Slot-3 (+0x18) stream Serialize body for AmbientEnvironmentComponent
// (retail 0x1400437e0 — Ghidra mislabels it AmbientEnvironmentComponent_Shutdown;
// the fn it CALLS is Component_Serialize, confirming this is really Serialize).
//
// Retail body (verified by decompile + disassembly of 0x1400437e0):
//   Component_Serialize(self, stream)
//   Serializer ser(stream, /*version*/0)
//   Vector3::Serialize(self+0x38, ser)   // region-center world position
//   Serialize(ser, self+0x28, null)      // gridX   (int)
//   Serialize(ser, self+0x2c, null)      // gridY   (int)
//   Serialize(ser, self+0x30, null)      // rw      (int)
//   Serialize(ser, self+0x34, null)      // rh      (int)
//   ~Serializer
//
// These are exactly the fields AmbientEnvironmentComponent_InitRegion (in
// ambientenvironmentcomponent.cpp) already writes at spawn time (gridX/gridY/
// rw/rh at +0x28.. +0x34, and the Vector3 region-center position at +0x38),
// so this body round-trips real per-instance state through save/load, not
// filler.
//
// ⚠ NOT WIRED as an opt-in feature: slot 3 is wired UNCONDITIONALLY (see
// AmbientEnvironmentComponent_FinishLiveVtable in ambientenvironmentcomponent.cpp)
// per the save/load wave — serialize round-trip is not behind BANISHED_AMBIENT_AUDIO.
// Standalone has no save/load driver yet (GameUpdateLoop::SaveRestore is a no-op
// stub), so this is unreachable today; it brings the class to parity with the
// rest of the save/load wave for when the driver lands.
#include "resolve.h"
#include "math/Math.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "Component/AmbientEnvironmentComponent.h"

namespace {

// (The base serialize 0x1401189b0 is Component::Persist, called directly below.)

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

}  // namespace

// ============================================================
// AmbientEnvironmentComponent::Serialize (vtable slot [3], 0x1400437e0)
// ============================================================

// [3] 0x1400437e0.
void AmbientEnvironmentComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<Math::Vector3*>(At(this, 0x38))->Serialize(ser);   // B.3.1 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x28)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x2c)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x30)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x34)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
