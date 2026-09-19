// code/Game/Component/growthcomponent_serialize.cpp
// GrowthComponent::Serialize (vtable slot [3], +0x18, original 0x1401207c0 —
// Ghidra mislabels it GrowthComponent_Shutdown; the second parameter is a
// Stream*, and the body is a textbook stream serializer, not a dtor).
//
// This is the missing piece behind the save/load tree-regrowth bug: on load,
// GrowthComponent had NO stream serializer at all (vtable slot 3 stayed the
// base-Component no-op), so every plant's growth state came back zeroed and
// GrowthComponent_Update (growthcomponent.cpp) re-derives the model's visual
// scale from that zeroed progress — trees visibly shrink to nothing and
// regrow from scratch, in EntityList update-bucket batches as each bucket's
// components first tick post-load.
//
// Fields (verified against the retail decompile / disassembly at 0x1401207c0):
//   +0x28  InternalBasePtr  sibling natural-resource component (validated
//                           against type id DAT_1401c7b30 — see
//                           growthcomponent.cpp's GrowthComponent_Startup
//                           comment: "resource@+0x28 = first sibling
//                           component of the natural-resource type").
//   +0x30  InternalBasePtr  sibling ModelComponent (validated against type id
//                           DWORD_1401c7a50 — same Startup comment: "+0x30 =
//                           sibling of type DAT_1401c7a50").
//   +0x48  float            smoothed displayed visual scale (see
//                           GrowthComponent_ApplyVisualScale, vtable slot 10).
//   +0x4c  float            growthScale — the randomized per-instance harvest
//                           threshold set once by GrowthComponent_Startup.
//   +0x40  Time (8 bytes)   sub-resource spawn timer (see GrowthComponent_Update).
//
// NOTE: the actual growth PROGRESS value that Update reads/writes every tick
// lives on the SIBLING resource component (resourceComp+0x28, where
// resourceComp = *(void**)(self+0x28) after this Serialize restores the IBP),
// not on GrowthComponent itself. This serializer restores GrowthComponent's
// own fields (including the +0x28 IBP that RE-LINKS to that sibling), which
// is everything retail's GrowthComponent::Serialize touches — the resource's
// own growth-progress field must come back through THAT component's own
// serializer (out of scope here; flagged for the save/load wave).
//
// Serializer facts reused from animalwave_serialize.cpp (verified against
// this function's own disassembly, same dispatch slots):
//   * Serializer is {Stream* stream; int version; ...}; ctor(self, stream, v).
//   * After serializing an InternalBasePtr, retail validates the pointee with
//     IsDerivedFrom(ptr, typeId) and nulls the ref on mismatch.
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

namespace {

typedef void (*fn_ComponentSerialize_t)(void* self, void* stream);
typedef void (*fn_TimeSerialize_t)(void* time, void* ser);
typedef bool (*fn_IsDerived_t)(void* obj, uint32_t typeId);

fn_ComponentSerialize_t s_Base   = nullptr;  // 0x1401189b0
fn_TimeSerialize_t      s_TimeSer = nullptr; // slot 0x140152aa8
fn_IsDerived_t           s_IsDerived = nullptr; // slot 0x140152a98

const uint32_t* s_tidResource = nullptr;  // 0x1401c7b30  natural-resource sibling
const uint32_t* s_tidModel    = nullptr;  // 0x1401c7a50  ModelComponent sibling

bool s_resolved = false;

void Resolve() {
    if (s_resolved) return;
    s_Base = reinterpret_cast<fn_ComponentSerialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_TimeSer = *reinterpret_cast<fn_TimeSerialize_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_IsDerived = *reinterpret_cast<fn_IsDerived_t*>(slot);

    s_tidResource = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_tidModel    = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a50));
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
// GrowthComponent::Serialize (vtable slot [3], 0x1401207c0)
// ============================================================
extern "C" void GrowthComponent_Serialize2(void* self, void* stream) {
    Resolve();
    if (!self || !stream) return;
    if (s_Base) s_Base(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    SerializeValidatedIBP(At(self, 0x28), ser, s_tidResource);
    SerializeValidatedIBP(At(self, 0x30), ser, s_tidModel);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x48)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<float*>(At(self, 0x4c)), nullptr);   // B.3.4 bridge
    if (s_TimeSer) reinterpret_cast<System::Time*>(At(self, 0x40))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge

    // Everything above is retail 0x1401207c0 field-for-field. What follows is a
    // deliberate, load-only repair -- see below for why it cannot be avoided.
    const bool loading =
        (static_cast<int>(reinterpret_cast<IO::Serializer*>(ser)->GetDirection()) == 0);   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge

    // config@+0x38 is NOT in the stream: retail's serialize (above) persists only
    // +0x28, +0x30, +0x48, +0x4c and +0x40. Its one writer is GrowthComponent's
    // Startup (retail 0x1401206f0), which is never dispatched for a stream-restored
    // component -- and which cannot simply be re-run here, because it also RE-ROLLS
    // growthScale@+0x4c from the RNG and would throw away the per-instance harvest
    // threshold this stream just restored (a silent load-fidelity bug: the plant
    // keeps its growth but changes when it becomes harvestable, differently on
    // every load).
    //
    // In Startup, +0x38 is only ever a copy of the description at +0x18 -- which
    // the base Component_Serialize above has just restored -- so fill it directly.
    //
    // This repair previously lived lazily inside GrowthComponent_Update, which left
    // an ordering hole: CropFieldComponent slot 15 (OnMessage, retail 0x14011ab50)
    // reads `*(float*)(growth[7] + 0x28)` unguarded, exactly as retail does, and a
    // crop field can message a restored plant BEFORE that plant's first Update tick.
    // That was a null deref a few hundred frames into a loaded tutorial 4. Doing it
    // here means no reader can ever observe +0x38 null on a loaded plant.
    if (loading && *reinterpret_cast<uintptr_t*>(At(self, 0x38)) == 0)
        *reinterpret_cast<uintptr_t*>(At(self, 0x38)) =
            *reinterpret_cast<uintptr_t*>(At(self, 0x18));
}
