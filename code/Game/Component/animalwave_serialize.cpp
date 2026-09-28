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
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/AnimalComponent.h"
#include "Component/HerdComponent.h"
#include "Component/LivestockComponent.h"
#include "Component/WildAnimalComponent.h"     // ENDGAME B.3.9 C1: 0x1401c8000 cell class
#include "Component/PopulationComponent.h"     // ENDGAME B.3.9 C1: 0x1401c4790 cell class
#include "Component/PastureComponent.h"        // ENDGAME B.3.9 C1: 0x1401c7ba0 cell class
#include "Entity/ComponentEntity.h"            // ENDGAME B.3.9 C1: 0x1401c5d70 cell class
#include "Component/PopulationDescription.h"   // ENDGAME B.3.9 C1: 0x1401c47c8 cell class

namespace {

// ---- engine helpers (shared by all four bodies) ----------------------------
// 0x1401189b0 is Component_SerializeReimpl; the import slot 0x140152a98 is
// Object::Base::IsDerivedFrom. Both are called directly.
// 0x1400032e0 was Collection::Array<void*>::SetSize(n) monomorphised into the
// exe (ENDGAME B.3.2 step 2, ImmigrationComp_DynArrayResize body: stride 8,
// align 8, zero tail) — the resolved pointer is gone, see the call sites below.

// True when the serializer is READING (retail: *(int*)(stream + 8) == 0,
// reached through the serializer's first field).
inline bool Loading(void* ser) {
    return static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
}

// Serialize one InternalBasePtr and apply retail's post-load type validation
// (null the ref when the pointee is not derived from *tid).
inline void SerializeValidatedIBP(void* ibp, void* ser, uint32_t tid) {
    reinterpret_cast<Object::InternalBasePtr*>(ibp)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    void* obj = *reinterpret_cast<void**>(ibp);
    if (obj && tid && !reinterpret_cast<const ::Object::Base*>(obj)->IsDerivedFrom(tid))   // B.3.x bridge
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

// ============================================================
// AnimalComponent::Serialize (vtable slot [3], 0x140106e40)
// Stream (version 1): base, wander Time(+0x30), habitat cell(+0x28),
// age(+0x44), deathAge(+0x48), wanders bool(+0x40), graze Time(+0x50),
// wander box shorts(+0x38/+0x3a/+0x3c/+0x3e). Version 0 additionally read a
// legacy IBP that is discarded after an IsDerivedFrom probe.
// ============================================================

// ============================================================
// LivestockComponent::Serialize (vtable slot [3], 0x140123e10)
// Stream (version 0): base, assigned-pasture backref IBP(+0x28, validated
// PastureComponent), production Time(+0x30).
// ============================================================

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
    if (!self || !stream) return;
    Component_SerializeReimpl(self, stream);   // 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c47c8 is PopulationDescription's
    // registered type-id cell (INVENTORY_C Table 1); ExternalPtrSerializeGeneric
    // (code/Game/Game, out of C1's file set) still wants a pointer.
    const uint32_t tidPopDesc = PopulationDescription::TypeId();
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(At(self, 0x28)), ser,
                                &tidPopDesc, "PopulationDescription");
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
        SerializeValidatedIBP(herd, ser, ComponentEntity::TypeId());

    reinterpret_cast<System::Time*>(At(self, 0x60))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x68)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(self, 0x6c)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

void AnimalComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    reinterpret_cast<System::Time*>(At(this, 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x28)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(this, 0x44)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(this, 0x48)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(this, 0x40)), nullptr);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(At(this, 0x50))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(this, 0x38)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(this, 0x3a)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(this, 0x3c)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<short*>(At(this, 0x3e)), nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() == 0) {   // B.3.4 bridge
        // v0 compatibility: read-and-discard one IBP (retail probes the type
        // and drops the value; the local never leaves the frame).
        uint64_t legacy = 0;
        reinterpret_cast<Object::InternalBasePtr*>(&legacy)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
        if (legacy)
            reinterpret_cast<const ::Object::Base*>(legacy)->IsDerivedFrom(PopulationComponent::TypeId());   // B.3.x bridge
    }

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

void HerdComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int count = *reinterpret_cast<int*>(At(this, 0x40));
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
    // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
    if (Loading(ser)) GameColl::AsArray<GameColl::Ptr8>(At(this, 0x38)).SetSize(count);

    uint8_t* member = *reinterpret_cast<uint8_t**>(At(this, 0x38));
    for (int i = 0; i < count && member; ++i, member += 8)
        SerializeValidatedIBP(member, ser, WildAnimalComponent::TypeId());

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(this, 0x48)), nullptr);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(At(this, 0x50))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(At(this, 0x58))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x4c)), nullptr);   // B.3.4 bridge
    SerializeValidatedIBP(At(this, 0x28), ser, PopulationComponent::TypeId());
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(At(this, 0x30)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

void LivestockComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    SerializeValidatedIBP(At(this, 0x28), ser, PastureComponent::TypeId());
    reinterpret_cast<System::Time*>(At(this, 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
