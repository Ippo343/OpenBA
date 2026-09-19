// code/Game/Component/animalwave_serialize.cpp
// Slot-3 (+0x18) stream Serialize bodies for the animal spawn wave:
//
//   HerdComponent_Serialize        0x140121f90
//   AnimalComponent_Serialize      0x140106e40
//   LivestockComponent_Serialize   0x140123e10
//   PopulationComponent_Serialize  0x1400544a0  (Ghidra: PopulationComponent_Shutdown;
//                                                the fn Ghidra CALLS *_Serialize at
//                                                0x140054220 is really the Startup)
//
// PastureComponent's body (0x140127f40) already lives in pasturecomponent.cpp
// (PastureComponent_Shutdown — Ghidra's shifted name) and WildAnimalComponent's
// (0x14012e300) in wildanimalcomponent.cpp; reg_animal_serialize.cpp registers
// all six retail addresses together.
//
// ⚠ NOT WIRED, BY DESIGN. Standalone has no save/load driver yet
// (GameUpdateLoop::SaveRestore 0x140095a90 is a no-op stub and nothing calls
// Save::SaveLoad::SetOperation for a game save), and a save stream is
// all-or-nothing: filling vtable slot 3 for SOME live components while others
// keep the inert no-op would desync every byte after the first missing
// component on load. Slot-3 wiring therefore lands as one repo-wide pass in
// the future save/load wave; these bodies bring the animal family to parity
// with the ~99 other ported-but-unwired component serializers until then.
//
// Serializer facts used below (verified against the retail decompiles):
//   * Serializer is {Stream* stream; int version; ...}; ctor(self, stream, v).
//   * The load-vs-save test retail emits before every load-side array resize
//     is *(int*)(stream + 8) == 0, read THROUGH the serializer's first field
//     (`*(int*)(*(int64*)ser + 8)`), not serializer+8.
//   * After serializing an InternalBasePtr, retail validates the pointee with
//     IsDerivedFrom(ptr, typeId) and nulls the ref on mismatch. With an
//     unregistered typeId storage (ResolveAddr -> null / id 0) we SKIP the
//     validation rather than null every ref: on the only path that matters
//     (load), skipping matches retail-with-valid-data behaviour.
#include "resolve.h"
#include "Component/coll_bridge.h"
#include "Game/serialize_externals.h"          // ExternalPtrSerializeGeneric
#include "Component/populationcomponent_funcs.h" // PopulationComponent_ResizeCells
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

namespace {

// ---- resolved engine helpers (shared by all four bodies) -------------------
typedef void (*fn_ComponentSerialize_t)(void* self, void* stream);
typedef void (*fn_SerCtor_t)(void* ser, void* stream, int version);
typedef void (*fn_SerDtor_t)(void* ser);
typedef void (*fn_SerInt_t)(void* ser, void* field, const char* name);
typedef void (*fn_SerBool_t)(void* ser, void* field, const char* name);
typedef void (*fn_SerFloat_t)(void* ser, float* field, const char* name);
typedef void (*fn_SerShort_t)(void* ser, void* field, const char* name);
typedef int  (*fn_SerGetVersion_t)(void* ser);
typedef void (*fn_TimeSerialize_t)(void* time, void* ser);
typedef void (*fn_IBPSerialize_t)(void* ibp, void* ser);
typedef bool (*fn_IsDerived_t)(void* obj, uint32_t typeId);
// 0x1400032e0 was Collection::Array<void*>::SetSize(n) monomorphised into the
// exe (ENDGAME B.3.2 step 2, ImmigrationComp_DynArrayResize body: stride 8,
// align 8, zero tail) — the resolved pointer is gone, see the call sites below.

fn_ComponentSerialize_t s_Base       = nullptr;  // 0x1401189b0
fn_SerCtor_t            s_Ctor       = nullptr;  // slot 0x140152ad8
fn_SerDtor_t            s_Dtor       = nullptr;  // slot 0x140152ad0
fn_SerInt_t             s_Int        = nullptr;  // slot 0x140152ac8
fn_SerBool_t            s_Bool       = nullptr;  // slot 0x140152a58
fn_SerFloat_t           s_Float      = nullptr;  // slot 0x140152a60
fn_SerShort_t           s_Short      = nullptr;  // slot 0x140152468
fn_SerGetVersion_t      s_GetVersion = nullptr;  // slot 0x140152a48
fn_TimeSerialize_t      s_TimeSer    = nullptr;  // slot 0x140152aa8
fn_IBPSerialize_t       s_IBPSer     = nullptr;  // slot 0x140152aa0
fn_IsDerived_t          s_IsDerived  = nullptr;  // slot 0x140152a98

// type-id storage (null when the owning reg wave is off; see header note)
const uint32_t* s_tidWildAnimal = nullptr;  // 0x1401c8000  herd members
const uint32_t* s_tidPopulation = nullptr;  // 0x1401c4790  herd/animal backref
const uint32_t* s_tidPasture    = nullptr;  // 0x1401c7ba0  livestock backref
const uint32_t* s_tidEntity     = nullptr;  // 0x1401c5d70  ComponentEntity base
const uint32_t* s_tidPopDesc    = nullptr;  // 0x1401c47c8  PopulationDescription

bool s_resolved = false;

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
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_Bool = *reinterpret_cast<fn_SerBool_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_Float = *reinterpret_cast<fn_SerFloat_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152468);
    if (slot) s_Short = *reinterpret_cast<fn_SerShort_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a48);
    if (slot) s_GetVersion = *reinterpret_cast<fn_SerGetVersion_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_TimeSer = *reinterpret_cast<fn_TimeSerialize_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152aa0);
    if (slot) s_IBPSer = *reinterpret_cast<fn_IBPSerialize_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_IsDerived = *reinterpret_cast<fn_IsDerived_t*>(slot);

    s_tidWildAnimal = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8000));
    s_tidPopulation = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4790));
    s_tidPasture    = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ba0));
    s_tidEntity     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));
    s_tidPopDesc    = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c47c8));
    s_resolved = true;
}

// True when the serializer is READING (retail: *(int*)(stream + 8) == 0,
// reached through the serializer's first field).
inline bool Loading(void* ser) {
    return *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(ser) + 8) == 0;
}

// Serialize one InternalBasePtr and apply retail's post-load type validation
// (null the ref when the pointee is not derived from *tid).
inline void SerializeValidatedIBP(void* ibp, void* ser, const uint32_t* tid) {
    if (s_IBPSer) reinterpret_cast<Object::InternalBasePtr*>(ibp)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    void* obj = *reinterpret_cast<void**>(ibp);
    if (obj && s_IsDerived && tid && *tid && !s_IsDerived(obj, *tid))
        *reinterpret_cast<uint64_t*>(ibp) = 0;
}

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

} // namespace

// ============================================================
// HerdComponent::Serialize (vtable slot [3], 0x140121f90)
// Stream: base, member count(+0x40) with load-side resize of the member IBP
// array(+0x38, validated WildAnimalComponent), phase bool(+0x48), graze
// Time(+0x50), spawn Time(+0x58), grazing cell(+0x4c), PopulationComponent
// backref IBP(+0x28), habitability cell(+0x30). Serializer version 0.
// ============================================================
extern "C" void HerdComponent_Serialize(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor || !s_Int) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int count = *reinterpret_cast<int*>(At(self, 0x40));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
    // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
    if (Loading(ser)) GameColl::AsArray<GameColl::Ptr8>(At(self, 0x38)).SetSize(count);

    uint8_t* member = *reinterpret_cast<uint8_t**>(At(self, 0x38));
    for (int i = 0; i < count && member; ++i, member += 8)
        SerializeValidatedIBP(member, ser, s_tidWildAnimal);

    if (s_Bool)    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(self, 0x48)), nullptr);   // B.3.4 bridge
    if (s_TimeSer) s_TimeSer(At(self, 0x50), ser);
    if (s_TimeSer) s_TimeSer(At(self, 0x58), ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x4c)), nullptr);   // B.3.4 bridge
    SerializeValidatedIBP(At(self, 0x28), ser, s_tidPopulation);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x30)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AnimalComponent::Serialize (vtable slot [3], 0x140106e40)
// Stream (version 1): base, wander Time(+0x30), habitat cell(+0x28),
// age(+0x44), deathAge(+0x48), wanders bool(+0x40), graze Time(+0x50),
// wander box shorts(+0x38/+0x3a/+0x3c/+0x3e). Version 0 additionally read a
// legacy IBP that is discarded after an IsDerivedFrom probe.
// ============================================================
extern "C" void AnimalComponent_Serialize(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor || !s_Int) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    if (s_TimeSer) s_TimeSer(At(self, 0x30), ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x28)), nullptr);   // B.3.4 bridge
    if (s_Float) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x44)), nullptr);   // B.3.4 bridge
    if (s_Float) reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x48)), nullptr);   // B.3.4 bridge
    if (s_Bool)  reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(self, 0x40)), nullptr);   // B.3.4 bridge
    if (s_TimeSer) s_TimeSer(At(self, 0x50), ser);
    if (s_Short) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(self, 0x38)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(self, 0x3a)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(self, 0x3c)), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(self, 0x3e)), nullptr);   // B.3.4 bridge
    }
    if (s_GetVersion && reinterpret_cast<IO::Serializer*>(ser)->GetVersion() == 0) {   // B.3.4 bridge
        // v0 compatibility: read-and-discard one IBP (retail probes the type
        // and drops the value; the local never leaves the frame).
        uint64_t legacy = 0;
        if (s_IBPSer) reinterpret_cast<Object::InternalBasePtr*>(&legacy)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
        if (legacy && s_IsDerived && s_tidPopulation && *s_tidPopulation)
            s_IsDerived(reinterpret_cast<void*>(legacy), *s_tidPopulation);
    }

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// LivestockComponent::Serialize (vtable slot [3], 0x140123e10)
// Stream (version 0): base, assigned-pasture backref IBP(+0x28, validated
// PastureComponent), production Time(+0x30).
// ============================================================
extern "C" void LivestockComponent_Serialize(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    SerializeValidatedIBP(At(self, 0x28), ser, s_tidPasture);
    if (s_TimeSer) s_TimeSer(At(self, 0x30), ser);
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// PopulationComponent::Serialize (vtable slot [3], 0x1400544a0 — Ghidra
// mislabels it PopulationComponent_Shutdown)
// Stream (version 0): base, PopulationDescription external ref(+0x28,
// FUN_14000b1b0 = the ExternalPtr<T>::Serialize template), habitability
// dim(+0x30), herd grid dim(+0x48), density-cell count(+0x40) with load-side
// 12-byte-cell resize(+0x38) and per-cell field order 0/8/4, herd-entity
// count(+0x58) with load-side IBP resize(+0x50, validated ComponentEntity),
// smooth Time(+0x60), ints +0x68/+0x6c.
// ============================================================
extern "C" void PopulationComponent_Serialize_Real(void* self, void* stream) {
    Resolve();
    if (!self || !stream || !s_Ctor || !s_Dtor || !s_Int) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x28)), ser,
                                s_tidPopDesc, "PopulationDescription");
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x30)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x48)), nullptr);   // B.3.4 bridge

    int cells = *reinterpret_cast<int*>(At(self, 0x40));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(cells, nullptr);   // B.3.4 bridge
    if (Loading(ser))
        PopulationComponent_ResizeCells(
            reinterpret_cast<int64_t*>(At(self, 0x38)), cells);
    uint8_t* cell = *reinterpret_cast<uint8_t**>(At(self, 0x38));
    for (int i = 0; i < cells && cell; ++i, cell += 0xc) {
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(cell + 0x0), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(cell + 0x8), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(cell + 0x4), nullptr);   // B.3.4 bridge
    }

    int herds = *reinterpret_cast<int*>(At(self, 0x58));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(herds, nullptr);   // B.3.4 bridge
    // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
    if (Loading(ser)) GameColl::AsArray<GameColl::Ptr8>(At(self, 0x50)).SetSize(herds);
    uint8_t* herd = *reinterpret_cast<uint8_t**>(At(self, 0x50));
    for (int i = 0; i < herds && herd; ++i, herd += 8)
        SerializeValidatedIBP(herd, ser, s_tidEntity);

    if (s_TimeSer) s_TimeSer(At(self, 0x60), ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x68)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x6c)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
