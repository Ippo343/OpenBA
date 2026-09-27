// code/Game/Component/fruitcomponent_serialize.cpp
// FruitComponent::Serialize (vtable slot [3], +0x18, original 0x14004a070 —
// Ghidra mislabels it FruitComponent_Shutdown; the second parameter is a
// Stream*, and the body is a textbook stream serializer, not a dtor).
//
// Without this, a loaded save's FruitComponent kept the base-Component
// no-op serialize: the sibling NaturalResourceComponent link (+0x28) came
// back null (FruitComponent_Update's [LIVE-COMPONENT BRING-UP GUARD] then
// re-derives it from FruitComponent_Startup, which is harmless since the
// link is idempotently recomputable from the entity), but ripeness (+0x30)
// and fruitFlag (+0x34) — genuinely per-instance state with no other source
// — came back zeroed, so every fruit tree's ripening progress reset on load.
//
// Fields (verified against the retail decompile at 0x14004a070):
//   +0x28  InternalBasePtr  sibling NaturalResourceComponent (validated
//                           against type id DAT_1401c7b30 — matches
//                           FruitComponent_Startup's own sibling lookup via
//                           FUN_1400052c0, see fruitcomponent.cpp).
//   +0x30  float            ripeness accumulator (FruitComponent_Update's
//                           `field30`).
//   +0x34  bool             fruitFlag (fruit-bearing-season latch).
//
// Serializer facts reused from animalwave_serialize.cpp / growthcomponent_serialize.cpp
// (verified against this function's own disassembly, same dispatch slots):
//   * Serializer is {Stream* stream; int version; ...}; ctor(self, stream, v).
//   * After serializing an InternalBasePtr, retail validates the pointee with
//     IsDerivedFrom(ptr, typeId) and nulls the ref on mismatch.
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "Component/FruitComponent.h"

namespace {

typedef bool (*fn_IsDerived_t)(void* obj, uint32_t typeId);

// (The base serialize 0x1401189b0 is Component::Persist, called qualified.)
fn_IsDerived_t          s_IsDerived = nullptr;  // slot 0x140152a98

const uint32_t* s_tidResource = nullptr;  // 0x1401c7b30  NaturalResourceComponent sibling

bool s_resolved = false;

void Resolve() {
    if (s_resolved) return;
    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_IsDerived = *reinterpret_cast<fn_IsDerived_t*>(slot);

    s_tidResource = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_resolved = true;
}

inline void SerializeValidatedIBP(void* ibp, void* ser, const uint32_t* tid) {
    reinterpret_cast<Object::InternalBasePtr*>(ibp)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    void* obj = *reinterpret_cast<void**>(ibp);
    if (obj && s_IsDerived && tid && *tid && !s_IsDerived(obj, *tid))
        *reinterpret_cast<uint64_t*>(ibp) = 0;
}

inline uint8_t* At(void* self, ptrdiff_t off) {
    return reinterpret_cast<uint8_t*>(self) + off;
}

} // namespace

// ============================================================
// FruitComponent::Serialize (vtable slot [3], 0x14004a070)
// ============================================================

void FruitComponent::Persist(void* stream) {
    Resolve();
    if (!stream) return;
    Component::Persist(stream);   // retail's direct CALL 0x1401189b0

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    SerializeValidatedIBP(At(this, 0x28), ser, s_tidResource);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(this, 0x30)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(At(this, 0x34)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
