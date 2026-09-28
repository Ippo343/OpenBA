// ============================================================================
// runtime/src/ui/elementdescription_serialize.cpp — Wave 10, lane EXTPTR
//   `?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z`
//   retail 0x180116b00, ordinal 3687, 57 instructions.
// ============================================================================
//
// THE ROW THIS WAVE EXISTS TO CLOSE. Wave 7 lane B declined it, Wave 9 lane
// UIW2 declined it twice, and `docs/runtime_port/WAVE10_EXTPTR_SERIALIZE.md`
// answered the last blocker: its three unported callees at +0x48/+0x50/+0x58
// are ONE body. That body is now
// `runtime/src/object/external_ptr_serialize.cpp`, registered at all three
// addresses, and this file is what it unblocks.
//
// It lives in its own file rather than in `dialog.cpp` for the reason
// `rt_object_port.cpp` states at the top: one lane, one file, so two lanes
// never edit the same source. Four UI lanes are live in this wave.
//
// ---------------------------------------------------------------------------
// SHAPE
// ---------------------------------------------------------------------------
// VIRTUAL (`UEAA`). RCX = this, RDX = an `IO::Stream&` — NOT a Serializer. The
// body builds a 16-byte `IO::Serializer` in its own frame ([RSP+0x28] = the
// Stream*, [RSP+0x30] = the codec) and asks the stream's `Encoding` for a
// reader or a writer depending on `Stream+0x08`:
//
//     Serializer ser{ &stream, nullptr };
//     Encoding* enc = stream.encoding;                  // [stream+0x10]
//     ser.readerWriter = stream.direction == 0          // [stream+0x08]
//         ? enc->vtable[+0x18](enc, &ser, 0)            // CreateReader
//         : enc->vtable[+0x20](enc, &ser, 0);           // CreateWriter
//
// The third argument is the save-format VERSION and it is ZERO: `XOR R8D,R8D`
// at 0x180116b24 is what the direction test compares against, and R8D is never
// reloaded before the factory call. Serializer.h records that this argument is
// NOT ignored — BinaryWriter writes it into the file — so it is worth spelling
// out. The epilogue is `if (rw) rw->vtable[0](rw, 1)`, the scalar DELETING
// destructor; skipping it leaks 0x30 bytes per call.
//
// It always returns `true` (`MOV AL,1`); there is no failure path.
//
// ---------------------------------------------------------------------------
// THE FOURTEEN RECORDS, IN RETAIL'S ORDER — WHICH IS NOT DECLARATION ORDER
// ---------------------------------------------------------------------------
// Getting the ORDER or the COUNT wrong here is the silent save-desync this
// project has paid for repeatedly, so the sequence is transcribed field by
// field from the listing and not from the header's field order:
//
//   +0x20 _x            int    slot +0xA0
//   +0x24 _y            int    slot +0xA0
//   +0x28 _width        int    slot +0xA0
//   +0x2c _height       int    slot +0xA0
//   +0x30 _minWidth     int    slot +0xA0
//   +0x34 _minHeight    int    slot +0xA0
//   +0x40 _elementType  uint   slot +0x98
//   +0x44 _flags        uint   slot +0x98
//   +0x3c _id           uint   slot +0x98   <-- declared int32_t; retail
//                                               serialises it through the
//                                               UNSIGNED slot. Transcribed as
//                                               retail has it: the two slots
//                                               are different vtable entries
//                                               and a reader picks by slot.
//   +0x38 _tabOrder     int    slot +0xA0   <-- and it comes AFTER +0x3c/+0x40
//   +0x48 ExternalPtr<UI::ElementDescription>   CALL 0x180038f90
//   +0x50 ExternalPtr<UI::Dialog>               CALL 0x180076610
//   +0x58 ExternalPtr<UI::StringTable>          CALL 0x1800042a0
//   +0x60 System::String                        CALL String::Serialize
//
// The +0x98 / +0xA0 slot numbers are RETAIL's. In Runtime-x64-profile.dll the
// same two live at +0xb0 and +0xb8, and copying them off a profile decompile
// would emit the wrong record type into every save
// ([[reference_encoding_vtable_offsets_differ_between_builds]]). They come from
// `rt::io::rw_vtbl`, which Wave 4 pinned twice against the shipped bytes.
//
// The third argument at every slot call is the record's NAME and retail passes
// NULL (`XOR R8D,R8D` before each one).
//
// ---------------------------------------------------------------------------
// BLAST RADIUS, AND HOW IT IS CHECKED
// ---------------------------------------------------------------------------
// This is not a leaf in a save file. It is the `UIDescription` parse path of
// EVERY `.rsc` in the game, and the neighbouring registration in
// `standalone/address_registry.cpp` carries the scar: a previously closed
// defect on the sibling slot 0x1401516b0 "silently SKIPPED the per-element Type
// read -> stream drift -> bogus String length -> CompressedStream::Read
// zero-progress spin on Dialog\StandardDialog.rsc". A related slot-8 defect
// "doubled every loaded entity's roster and made FindComponent return the empty
// duplicate — invisible in a screenshot."
//
// Registering the NAME here also re-points the exe's own `.rdata` import slot
// `0x1401518c8` (`standalone/address_registry.cpp:1109` binds that slot BY NAME
// through `ResolveExport`, and `RuntimePortInit()` runs before
// `AddressRegistryInit()`), so the blast radius is not limited to our `fn_*`
// pointers. `engine/funcs/engine_funcs.cpp:4063` reads the same slot.
//
// So the verification is not a screenshot. It is (a) the emission oracle below,
// which compares our record sequence against retail's, and (b) a full boot,
// which is what actually drives `.rsc` parsing.
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// the emission oracle in the WRITING direction against two synthetic sinks,
// in situ on the first 16 real calls — the same instrument
// `runtime/src/achievement/goal_serialize.cpp` uses, and this file follows its
// shape deliberately.
//
// NO PREFIX STOP. The recipe warned to cap the prefix with `stopAfter` before
// `GetReferenceId` in case it registers a new id. Re-measured rather than
// inherited (0x1800bf100): that body is `QEBA`, builds a Filename, descends the
// map at `this+0x10`, returns `[node+0x14]` and releases two Strings. No
// insert, no counter, no store. Nothing on this write path mutates anything a
// second call would see, so the whole body replays and the oracle covers all
// fourteen records rather than a prefix of them.
//
// The one thing to read in its report is the `calls=` column, not just
// `divergences=`: a body with no callers reports clean
// ([[reference_uncalled_port_is_not_verified]]).
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/object/External.h"
#include "../../types/system/String.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

namespace {

// The three ExternalPtr<T>::Serialize instantiations this body dispatches
// (runtime/src/object/external_ptr_serialize.cpp, declared in
// runtime/types/object/External.h) and System::String::Serialize, called by name.
RtObject::External** ExtField(void* field) {
    return reinterpret_cast<RtObject::External**>(field);                  // B.3.x bridge
}

// The prologue and epilogue, transcribed from 0x180116b20 and 0x180116c4d.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw =
        reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    delete reinterpret_cast<IO::ReaderWriter*>(rw);   // B.3.x bridge
}

void RecInt(ReaderWriter* rw, int32_t* v) {
    reinterpret_cast<IO::ReaderWriter*>(rw)->SerializeInt(*v, nullptr);   // B.3.x bridge
}

void RecUInt(ReaderWriter* rw, void* v) {
    reinterpret_cast<IO::ReaderWriter*>(rw)->SerializeUInt(*reinterpret_cast<uint32_t*>(v), nullptr);   // B.3.x bridge
}

}  // namespace

// ============================================================================
// bool UI::ElementDescription::Serialize(IO::Stream&) — the MEMBER
// ============================================================================
// ENDGAME B.3.6 block A: the body MOVED here from the door (`self->x` became
// `x`). The slot's parameter is `void*` because `Object::External` declares
// slot 5 that way (IO::Stream is still spelled as the rt::io POD in this tree).
namespace UI {

bool ElementDescription::Serialize(void* streamArg) {
    Stream* const stream = static_cast<Stream*>(streamArg);
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    RecInt(rw, &_x);            // +0x20
    RecInt(rw, &_y);            // +0x24
    RecInt(rw, &_width);        // +0x28
    RecInt(rw, &_height);       // +0x2c
    RecInt(rw, &_minWidth);     // +0x30
    RecInt(rw, &_minHeight);    // +0x34

    RecUInt(rw, &_elementType); // +0x40
    RecUInt(rw, &_flags);       // +0x44
    RecUInt(rw, &_id);          // +0x3c — unsigned slot, see the banner

    RecInt(rw, &_tabOrder);     // +0x38

    RtObject_ExternalPtrSerialize_ElementDescription(ExtField(&_childDesc), &ser);   // +0x48 0x180038f90
    RtObject_ExternalPtrSerialize_Dialog(ExtField(&_userData), &ser);                // +0x50 0x180076610
    RtObject_ExternalPtrSerialize_StringTable(ExtField(&_stringTable), &ser);        // +0x58 0x1800042a0
    reinterpret_cast<System::String*>(&_textName)->Serialize(&ser);                  // +0x60 0x1800c9930

    DestroyCodec(rw);
    return true;
}

// (`~ElementDescription` is inline in ElementDescription.h; its body,
// `ReleaseFields`, is in dialog.cpp beside the key function.)

}  // namespace UI

// (ENDGAME B.3.9 UI-D: the slot door `RtUi_ElementDescription_Serialize` is
// retired with the minted `??_7ElementDescription@UI@@6B@`; every leaf's
// Serialize calls the member QUALIFIED, and the exe import slot that once
// bound the name is gone.)

// ---- Registration ----------------------------------------------------------
int RtUiElementDescriptionSerializeRegister() {
    return 0;
}
