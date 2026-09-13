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

#include "../../funcs/rt_port.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/io/Serializer.h"

using RtAchievement::Achievement;
using RtAchievement::AchievementManager;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// The shared prologue/epilogue, transcribed identically to
// goal_serialize.cpp's MakeCodec/DestroyCodec. Deliberately duplicated rather
// than hoisted into a header: these two files are gated by DIFFERENT env
// switches (RT_GOAL vs RT_ACHMGR), and a shared helper would
// make one lane's kill-switch silently affect the other's bodies.
//
//   Serializer ser{ &stream, nullptr };
//   Encoding* enc = stream.encoding;                       // [stream+0x10]
//   ser.readerWriter = stream.direction == 0               // [stream+0x08]
//        ? enc->vtable[+0x18](enc, &ser, 0)                // CreateReader
//        : enc->vtable[+0x20](enc, &ser, 0);               // CreateWriter
//
// The third argument is the save-format VERSION and it is ZERO: retail zeroes
// R8D for the `CMP [stream+8],R8D` at 0x18009e094 / 0x18009e934 and never
// reloads it before the factory call.
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

// `if (rw) rw->vtable[0](rw, 1)` — the scalar DELETING destructor. Skipping it
// leaks the codec the factory just allocated, once per Serialize.
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

// ---- the unexported internals, by address ---------------------------------
using PtrSerializeFn   = void (*)(void*, Serializer*);
using ArraySetSizeFn   = void (*)(void*, int);
using ArraySerializeFn = void (*)(Serializer*, int, void*);

// CALL 0x1800042a0 from Achievement::Serialize at 0x18009e0bd (over +0x20).
PtrSerializeFn ExternalPtrSerialize_StringTable() {
    static PtrSerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<PtrSerializeFn>(ResolveAddr(Module::Runtime, 0x1800042a0));
    return f;
}
// CALL 0x180004590 from Achievement::Serialize at 0x18009e0e6 (over +0x38).
// Module::Runtime is explicit: dialog_vtable.cpp:168 records that this same
// address is a DIFFERENT function inside VideoDX11-x64.dll.
PtrSerializeFn ExternalPtrSerialize_SpriteSheet() {
    static PtrSerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<PtrSerializeFn>(ResolveAddr(Module::Runtime, 0x180004590));
    return f;
}
// Collection::Array<ExternalPtr<Goal>>::SetSize — read side only.
ArraySetSizeFn ArraySetSizeGoalPtrs() {
    static ArraySetSizeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySetSizeFn>(ResolveAddr(Module::Runtime, 0x180005110));
    return f;
}
// Collection::Array<InternalPtr<AchievementInstance>>::SetSize — read side only.
// PORTED by Wave 9 lane Q; this resolves to our body, not the DLL's.
ArraySetSizeFn ArraySetSizeInternalPtrs() {
    static ArraySetSizeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySetSizeFn>(ResolveAddr(Module::Runtime, 0x180008d60));
    return f;
}
// The two N-element serializers. Signature (Serializer*, int count, T* data).
ArraySerializeFn SerializeGoalPtrs() {
    static ArraySerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySerializeFn>(ResolveAddr(Module::Runtime, 0x180005220));
    return f;
}
ArraySerializeFn SerializeInstancePtrs() {
    static ArraySerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<ArraySerializeFn>(ResolveAddr(Module::Runtime, 0x180009a20));
    return f;
}

// ?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z — retail 0x1800c9930,
// ported by Wave 2 and registered BY ADDRESS (string.cpp:958), so the address
// is what finds it. By name it would miss.
using StringSerializeFn = void (*)(void*, Serializer*);
StringSerializeFn StringSerialize() {
    static StringSerializeFn f = nullptr;
    if (!f) f = reinterpret_cast<StringSerializeFn>(ResolveAddr(Module::Runtime, 0x1800c9930));
    return f;
}

}  // namespace

// ============================================================================
// bool Achievement::Achievement::Serialize(IO::Stream&)
// ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z        retail 0x18009e070
//
// VIRTUAL (UEAA) — slot 5 of ??_7Achievement@0@6B@ (DLL 0x180154740).
// RCX = this, RDX = the Stream. Always returns true (`MOV AL,1`); no failure
// path.
//
// SIX RECORDS, in this exact order, and the order is the whole contract:
//   +0x20 _stringTable  ExternalPtr<UI::StringTable>  -> 0x1800042a0
//   +0x28 _title        System::String                -> String::Serialize
//   +0x30 _description  System::String                -> String::Serialize
//   +0x38 _spriteSheet  ExternalPtr<UI::SpriteSheet>  -> 0x180004590
//   +0x40 _spriteName   System::String                -> String::Serialize
//   +0x50 _goals._size  int                           -> ReaderWriter +0xA0
//   then _goals._size elements of _goals._data        -> 0x180005220
//
// NOTE THE LAST PAIR CAREFULLY. The COUNT is read from +0x50 — that is
// `_goals._size`, because `_goals` is the Array at +0x48 and an Array's _size
// sits at +0x08 of it. The ELEMENTS come from the pointer at +0x48. Retail
// spells these as two different offsets on two adjacent instructions and it is
// easy to transcribe one of them as the other; this port reads them through the
// named struct so the offsets are checked by the header's static_asserts.
//
// The SetSize at 0x180005110 fires only when `stream.direction == 0` (reading);
// it grows the array to the count just read before the elements are filled in.
//
// ORACLE: the emission oracle, WRITING direction, two synthetic sinks,
// stopAfter = 0 (whole body). Legitimate because every emission on the write
// path is a pure read of this object: the three String::Serialize calls read,
// the two ExternalPtr serializers take the GetReferenceId branch which
// goal_serialize.cpp already proved const and idempotent (0x1800bf100 is QEBA,
// a pure lookup, no insert and no counter), and 0x180005220's write path is the
// same lookup per element. The `Encoding` slot +0x10 == 3 VISITOR branch — the
// one that calls ExternalFactory::FindOrLoad for its side effect — is
// unreachable under the sink, which binds every Encoding slot to a recording
// thunk and never returns 3.
// ============================================================================
bool RtAch_Achievement_Serialize(Achievement* self, Stream* stream) {
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    ExternalPtrSerialize_StringTable()(&self->stringTable, &ser);   // +0x20
    StringSerialize()(&self->title, &ser);                          // +0x28
    StringSerialize()(&self->description, &ser);                    // +0x30
    ExternalPtrSerialize_SpriteSheet()(&self->spriteSheet, &ser);   // +0x38
    StringSerialize()(&self->spriteName, &ser);                     // +0x40

    int count = self->goals._size;                                  // +0x50
    using IntFn = void (*)(ReaderWriter*, int*, const char*);
    reinterpret_cast<IntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &count, nullptr);

    if (stream->direction == rt::io::kDirectionReading) {
        ArraySetSizeGoalPtrs()(&self->goals, count);
    }
    SerializeGoalPtrs()(&ser, count, self->goals._data);            // +0x48

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// void Achievement::AchievementManager::Persist(IO::Stream&)
// ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z
//                                                         retail 0x18009e910
//
// NON-virtual (QEAA). RCX = this, RDX = the Stream. Returns nothing.
//
// Same prologue and epilogue; the middle is four steps, TWO of which fire only
// when reading:
//   READ ONLY:  RemoveAll(this)                  -- destroy every existing
//                                                   instance before refilling
//   BOTH:       count = _instances._size   -> ReaderWriter +0xA0
//   READ ONLY:  Array<InternalPtr>::SetSize(&_instances, count)
//   BOTH:       serialize `count` instances from _instances._data
//
// THE ORDER OF THE FIRST TWO IS LOAD-BEARING AND IS EASY TO GET BACKWARDS.
// `RemoveAll` zeroes `_instances._size`, and the count is read AFTER it. On the
// reading side the value emitted into the codec is therefore ALWAYS 0 — but
// that does not matter, because on the reading side the codec OVERWRITES the
// variable with the value from the file. Transcribing `count` before
// `RemoveAll` would be invisible on the write path and would corrupt nothing
// on the read path either, yet it would emit a nonzero count from a manager
// being loaded into. It is kept in retail's order because the bytes are the
// specification, not because a test would catch it.
//
// ORACLE: PREFIX. See the banner — stopAfter = 1, covering the count record
// only, because the element loop mutates the global Object::Factory.
// ============================================================================
void RtAch_AchievementManager_Persist(AchievementManager* self, Stream* stream) {
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    if (stream->direction == rt::io::kDirectionReading) {
        using RemoveAllFn = void (*)(void*);
        auto removeAll = RT_EXPORT_ONCE(RemoveAllFn,
            "?RemoveAll@AchievementManager@Achievement@@QEAAXXZ");
        removeAll(self);
    }

    int count = self->instances._size;
    using IntFn = void (*)(ReaderWriter*, int*, const char*);
    reinterpret_cast<IntFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, &count, nullptr);

    if (stream->direction == rt::io::kDirectionReading) {
        ArraySetSizeInternalPtrs()(&self->instances, count);
    }
    SerializeInstancePtrs()(&ser, count, self->instances._data);

    DestroyCodec(rw);
}

// ---- Registration ----------------------------------------------------------
// Gated by RT_ACHMGR with achievement_manager.cpp and
// achievement_state.cpp — class atomicity (PLAN §4.1 decision 5).
//
// `?Persist@AchievementManager@…` is a LIVE door today:
// code/Game/Game/gameupdateloop_serialize.cpp:165 resolves it by name on every
// save and every load. `?Serialize@Achievement@1@` is slot 5 of a MINTED,
// UNPROMOTED vtable, so registering it changes nothing until that table is
// promoted — see the VTABLE section of the lane doc. It is registered anyway,
// because an unregistered body is indistinguishable from an absent one
// (reference_half_registered_port_reads_as_absent.md) and because the slot is
// exactly what COMPLETE-ness of ??_7Achievement@0@6B@ is waiting on.
int RtAchSerializeRegister() {
    int n = 0;

    RegisterRuntimeExport("?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtAch_Achievement_Serialize); ++n;
    RegisterRuntimeAddr(0x18009e070, (void*)&RtAch_Achievement_Serialize);

    RegisterRuntimeExport("?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z",
                          (void*)&RtAch_AchievementManager_Persist); ++n;
    RegisterRuntimeAddr(0x18009e910, (void*)&RtAch_AchievementManager_Persist);

    return n;
}
