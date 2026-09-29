// ============================================================================
// runtime/src/achievement/achievement_serialize.cpp — Wave 23, lane ACHIEVE
// Achievement::Achievement::Serialize and AchievementManager::Persist
// ============================================================================
//
//   ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z            0x18009e070
//   ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z 0x18009e910
//
// ----------------------------------------------------------------------------
// A STATED BLOCKER, RE-CHECKED — AND IT WAS STALE (Wave 23 rule 3)
// ----------------------------------------------------------------------------
// `achievement_manager.cpp`'s banner holds `Persist` back in these words:
//
//     "it inlines an IO::Serializer construction, and Wave 4 lane A measured
//      that a Serializer ctor CONSUMES OR EMITS A 12-BYTE STREAM HEADER. So it
//      has no runnable oracle of any kind: a second construction desyncs the
//      save."
//
// That was true when written (Wave 5, 2026-09-06) and has not been true since
// Wave 9. Lane S built the save-stream emission oracle, which ran each side
// against its OWN INDEPENDENT SYNTHETIC SINK — there is no shared stream to
// desync, and each side constructs its own codec. Lane P then used exactly that
// to discharge the SAME blocker, stated in the SAME words, for
// `Achievement::Goal::Serialize` and `GoalInstance::Persist`
// (runtime/src/achievement/goal_serialize.cpp's banner). The two functions in
// this file are the third and fourth members of that family and the mechanism
// is identical. The holdback survived fourteen waves because nobody re-read it.
//
// The banner in achievement_manager.cpp is corrected in the same commit.
//
// ----------------------------------------------------------------------------
// THE TWO ORACLES ARE NOT THE SAME STRENGTH, AND THE DIFFERENCE IS THE POINT
// ----------------------------------------------------------------------------
// `Achievement::Serialize` on the WRITE path is a pure read of the object, so
// the whole body replays: stopAfter = 0, everything compared.
//
// `AchievementManager::Persist` is NOT. Its tail (FUN_180009a20) is a loop of
// `Object::InternalBasePtr::Serialize`, which the emission oracle names in as many
// words as one of the mutating members — it "ends in PersistObject -- mutates
// the pointer AND the global factory". Replaying that performs the mutation
// twice on the one live factory. So `Persist` gets the PREFIX oracle, stopping
// after the ONE emission that precedes the loop (the int count, emitted through
// ReaderWriter slot +0xA0 at 0x18009e988). THE COST IS REAL AND IS NOT HEDGED:
// everything after emission 1 — i.e. every instance record — is NOT compared by
// this oracle. What it does prove is the half that decides the stream layout:
// that the count record is emitted, through the right slot, with the right
// value, after the right codec was built.
//
// ----------------------------------------------------------------------------
// FOUR NEW ADDRESS DOORS, NAMED RATHER THAN HIDDEN
// ----------------------------------------------------------------------------
// Both bodies reach unexported Collection/Object internals. They are CALLED by
// address, not reimplemented — the same cost goal_serialize.cpp recorded for
// its two, and the same growth in the cutover surface
// (reference_porting_can_grow_the_cutover_surface.md). Two of the four were
// already open:
//
//   0x1800042a0  ExternalPtr<UI::StringTable>::Serialize   ALREADY A DOOR
//                (goal_serialize.cpp:130, external_ptr_serialize.cpp:47)
//   0x180004590  ExternalPtr<UI::SpriteSheet>::Serialize   ALREADY A DOOR
//                (ui/dialog_vtable.cpp:167 — note its recorded COLLISION
//                 HAZARD: 0x180004590 is a different function in
//                 VideoDX11-x64.dll. Module::Runtime is explicit below.)
//   0x180005110  Array<ExternalPtr<Goal>>::SetSize         *** NEW ***
//   0x180005220  serialize N ExternalPtr<Goal>             *** NEW ***
//   0x180009a20  serialize N InternalPtr<AchievementInstance> *** NEW ***
//
// 0x180008d60 (Array<InternalPtr>::SetSize) is NOT new and is NOT a DLL call:
// Wave 9 lane Q ported it and registered the address, so
// ResolveAddr(Module::Runtime, 0x180008d60) lands on our body
// (reference_resolveaddr_consults_port_registry_first.md).
//
// NEITHER SetSize CALL IS REACHED ON THE WRITE PATH — both are guarded by
// `stream.direction == 0`. They are transcribed because the read path is the
// one that loads every save, and the oracle above does not cover it. Said
// plainly: the reading direction of both bodies is UNVERIFIED by this file.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/object/Internal.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

namespace {

// ---------------------------------------------------------------------------
// The shared prologue/epilogue, transcribed identically to goal_serialize.cpp's.
//
//   Serializer ser{ &stream, nullptr };
//   Encoding* enc = stream.encoding;                       // [stream+0x10]
//   ser.readerWriter = stream.direction == 0               // [stream+0x08]
//        ? enc->vtable[+0x18](enc, &ser, 0)                // CreateReader
//        : enc->vtable[+0x20](enc, &ser, 0);               // CreateWriter
//
// The version argument is ZERO: retail zeroes R8D at 0x18009e094 / 0x18009e934
// and never reloads it before the factory call.
// ---------------------------------------------------------------------------
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;

    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

// The `rt::io::ReaderWriter` POD and `IO::ReaderWriter` are asserted
// interchangeable (Serializer.h); the members are called by name on the class.
IO::ReaderWriter* AsRw(ReaderWriter* rw) {
    return reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}

// `if (rw) rw->vtable[0](rw, 1)` — the scalar DELETING destructor.
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    delete AsRw(rw);
}

// ---- the unexported internals, called by name ----------------------------
// All four bodies are ours (runtime/src/object/external_ptr_serialize.cpp,
// internal_ptr_array_serialize.cpp), declared in object/External.h and
// object/Internal.h (B3R: they were read by retail address before).
//   0x1800042a0  ExternalPtr<UI::StringTable>::Serialize      (+0x20)
//   0x180004590  ExternalPtr<UI::SpriteSheet>::Serialize      (+0x38)
//   0x180005220  serialize N ExternalPtr<Goal>                (+0x48)
//   0x180009a20  serialize N InternalPtr<AchievementInstance>
RtObject::External** AsExt(void* p) {
    return reinterpret_cast<RtObject::External**>(p);   // B.3.x bridge
}

}  // namespace

namespace Achievement {

// ============================================================================
// bool Achievement::Serialize(IO::Stream&) — SLOT 5
// ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z        retail 0x18009e070
//
// SIX RECORDS, in this exact order, and the order is the whole contract:
//   +0x20 _stringTable  ExternalPtr<UI::StringTable>  -> 0x1800042a0
//   +0x28 _title        System::String                -> String::Serialize
//   +0x30 _description  System::String                -> String::Serialize
//   +0x38 _spriteSheet  ExternalPtr<UI::SpriteSheet>  -> 0x180004590
//   +0x40 _spriteName   System::String                -> String::Serialize
//   +0x50 _goals._size  int                           -> ReaderWriter +0xA0
//   then _goals._size elements of _goals._data        -> 0x180005220
// The SetSize (0x180005110) fires only when reading. Always `MOV AL,1`.
// ============================================================================
bool Achievement::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    RtObject_ExternalPtrSerialize_StringTable(AsExt(&_stringTable), &ser);   // +0x20
    _title.Serialize(&ser);                                         // +0x28
    _description.Serialize(&ser);                                   // +0x30
    RtObject_ExternalPtrSerialize_SpriteSheet(AsExt(&_spriteSheet), &ser);   // +0x38
    _spriteName.Serialize(&ser);                                    // +0x40

    int count = _goals.GetSize();                                   // +0x50
    AsRw(rw)->SerializeInt(count, nullptr);                         // +0xA0

    if (stream->direction == rt::io::kDirectionReading) {
        _goals.SetSize(count);   // 0x180005110 — `GoalArray` names kSpec_U64Zero
    }
    RtObject_ExternalPtrArraySerialize_Goal(&ser, count, AsExt(_goals.GetData()));   // +0x48

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// void AchievementManager::Persist(IO::Stream&)
// ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z
//                                                         retail 0x18009e910
//
// NON-virtual. Four steps, two of them read-only:
//   READ ONLY:  RemoveAll()
//   BOTH:       count = _instances._size   -> ReaderWriter +0xA0
//   READ ONLY:  _instances.SetSize(count)  (0x180008d60, kSpec_PtrZero)
//   BOTH:       serialize `count` instances
// RemoveAll zeroes `_size` and the count is read AFTER it — retail's order.
// ============================================================================
void AchievementManager::Persist(IO::Stream& streamRef) {
    // B.3.7 bridge: the ported body walks the stream through the `rt::io`
    // view of retail's IO::Stream (runtime/types/io/Serializer.h), the same
    // bytes; the IO classes' own members are not what this body calls.
    Stream* stream = reinterpret_cast<Stream*>(&streamRef);   // B.3.7 bridge
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    if (stream->direction == rt::io::kDirectionReading)
        RemoveAll();

    int count = _instances.GetSize();
    AsRw(rw)->SerializeInt(count, nullptr);                         // +0xA0

    if (stream->direction == rt::io::kDirectionReading)
        _instances.SetSize(count);
    RtObject_InternalPtrArraySerialize_AchievementInstance(
        reinterpret_cast<::IO::Serializer*>(&ser), count,            // B.3.x bridge
        reinterpret_cast<RtObject::InternalBasePtr*>(_instances.GetData()));

    DestroyCodec(rw);
}

}  // namespace Achievement

// ---- The registered doors — QUALIFIED delegations --------------------------
// (ENDGAME B.3.9 ENG: the `?Serialize@Achievement@1@` slot door is gone with
// the minted `??_7Achievement@0@6B@` row, its only reader; Scenario calls the
// member QUALIFIED.)

void RtAch_AchievementManager_Persist(Achievement::AchievementManager* self, IO::Stream* stream) {
    self->Achievement::AchievementManager::Persist(*stream);
}
