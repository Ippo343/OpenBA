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
#include <cstdint>

namespace {

typedef void (*fn_ComponentSerialize_t)(void* self, void* stream);
typedef void (*fn_SerCtor_t)(void* ser, void* stream, int version);
typedef void (*fn_SerDtor_t)(void* ser);
typedef void (*fn_SerInt_t)(void* ser, void* field, const char* name);
typedef void (*fn_Vector3Serialize_t)(void* vec3, void* ser);

fn_ComponentSerialize_t s_Base = nullptr;  // 0x1401189b0
fn_SerCtor_t            s_Ctor = nullptr;  // slot 0x140152ad8
fn_SerDtor_t            s_Dtor = nullptr;  // slot 0x140152ad0
fn_SerInt_t             s_Int  = nullptr;  // slot 0x140152ac8
fn_Vector3Serialize_t   s_Vec3 = nullptr;  // Math::Vector3::Serialize (mangled export)

bool s_resolved = false;

// Resolved unconditionally at the top of every call — this can be the very
// first call into the class on load-from-save, so lazy/cached resolution
// elsewhere is not safe (see animalwave_serialize.cpp's header note).
void Resolve() {
    if (s_resolved) return;
    s_Base = reinterpret_cast<fn_ComponentSerialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_Ctor = *reinterpret_cast<fn_SerCtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_Dtor = *reinterpret_cast<fn_SerDtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_Int = *reinterpret_cast<fn_SerInt_t*>(slot);

    s_Vec3 = reinterpret_cast<fn_Vector3Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));

    s_resolved = true;
}

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

}  // namespace

// ============================================================
// AmbientEnvironmentComponent::Serialize (vtable slot [3], 0x1400437e0)
// ============================================================
extern "C" void AmbientEnvironmentComponent_Serialize(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor || !s_Int) return;
    if (s_Base) s_Base(self, stream);

    uint8_t ser[24];
    s_Ctor(ser, stream, 0);

    if (s_Vec3) s_Vec3(At(self, 0x38), ser);
    s_Int(ser, At(self, 0x28), nullptr);
    s_Int(ser, At(self, 0x2c), nullptr);
    s_Int(ser, At(self, 0x30), nullptr);
    s_Int(ser, At(self, 0x34), nullptr);

    s_Dtor(ser);
}
