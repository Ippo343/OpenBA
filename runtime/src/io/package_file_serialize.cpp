// ============================================================================
// runtime/src/io/package_file_serialize.cpp — Wave 27 lane IOTEXT
//   ?Serialize@PackageFile@IO@@UEAA_NAEAVStream@2@@Z   retail 0x1800b60a0
//   `??_7PackageFile@IO@@6B@` slot 5 — THE LAST UNPORTED SLOT OF THAT TABLE.
// ============================================================================
//
// SOURCE OF EVERY FACT BELOW: the disassembly of 0x1800b60a0 in
// **Runtime-x64.dll** (retail), read instruction by instruction. Not the
// profile build, and not Ghidra's decompiled C — this body is nothing but
// indirect vtable calls through byte offsets, which is exactly the shape
// Ghidra's C output drops arguments at
// (reference_ghidra_c_output_vs_disassembly.md), and IO vtable offsets are the
// one family measured to DIFFER between the two builds
// (reference_encoding_vtable_offsets_differ_between_builds.md). Every slot
// number here comes from `rt::io::rw_vtbl` / `rt::io::encoding_vtbl`, which
// Wave 4 pinned against the shipped retail bytes.
//
// ---------------------------------------------------------------------------
// THE RECORD SEQUENCE, AND IT IS THE WHOLE OF THE FILE FORMAT
// ---------------------------------------------------------------------------
// A desync is "a different sequence of typed writes", so the order below is the
// specification and reordering any line to ascending-offset tidiness would
// break every .pkg this build has ever written.
//
//   1800b60ea  String::Serialize  +0x40  name
//   1800b60ff  String::Serialize  +0x48  author
//   1800b610c  String::Serialize  +0x50  description
//   1800b6119  String::Serialize  +0x58  _string58
//   1800b6126  String::Serialize  +0x60  _string60
//   1800b6139  rw kScalarInt      +0x6c  userVersion
//   1800b614d  rw kScalarInt      +0x70  priority
//   1800b6167  rw kScalarInt      (a LOCAL copy of +0x28 — see below)
//   1800b617e  [reading only] Array::SetSize(+0x20, n)
//   1800b618e  StringArray::Serialize(&ser, n, *(char**)(this+0x20))
//   1800b61a7  rw kScalarInt      (a LOCAL copy of +0x38)
//   1800b61be  [reading only] Array::SetSize(+0x30, m)
//   1800b61ce  StringArray::Serialize(&ser, m, *(char**)(this+0x30))
//   1800b61e4  rw kScalarBool     +0xac  overridable
//   1800b61f5  rw kScalarInt      +0x78  iconWidth
//   1800b6209  rw kScalarInt      +0x7c  iconHeight
//   1800b6226  rw kScalarInt      (a LOCAL copy of +0x88)
//   1800b6240  [reading only] Array<uint>::SetSize(+0x80, k)
//   1800b6259  rw kArrayUInt      (k, *(uint**)(this+0x80))
//   1800b626e  rw slot 0 (flags 1) — destroy the codec
//   1800b6270  MOV AL,1 — ALWAYS TRUE. There is no failure path.
//
// `packfileId` (+0xA8) IS NOT SERIALIZED. Neither is `version` (+0x68), nor
// `platform` (+0x74), nor the `FileInfo` the destructor tears down. Stated
// because their ABSENCE is as load-bearing as the presences: a reader that
// expects them desyncs.
//
// ---------------------------------------------------------------------------
// THE THREE COUNT FIELDS GO THROUGH A LOCAL, AND THAT IS NOT A DETAIL
// ---------------------------------------------------------------------------
// Retail copies `[this+0x28]` into the stack slot at `[rbp+0x10]`, serializes
// THE LOCAL, and then uses the local to size the array:
//
//     1800b6153  MOV EAX,dword ptr [RBX + 0x28]
//     1800b6156  MOV dword ptr [RBP + 0x10],EAX
//     1800b6167  CALL qword ptr [RAX + 0xa0]      ; kScalarInt(&local)
//     1800b6171  CMP dword ptr [RAX + 0x8],0x0    ; ser.stream->direction
//     1800b617e  CALL 0x180029a80                 ; SetSize(&this[0x20], local)
//
// On the WRITE path the local carries the field's value out. On the READ path
// the codec writes the incoming count INTO the local, and `SetSize` is what
// puts it back into the Array's own count word — the field is never assigned
// directly. Serializing `&this->count` in place would work on write and, on
// read, would leave a count that disagreed with the allocation for the window
// between the two calls. Transcribed as retail has it.
//
// The `[reading only]` guard is retail's, at 0x1800b6175 / 0x1800b61b5 /
// 0x1800b6234: `CMP dword ptr [ser.stream + 0x8],0x0 ; JNZ past`. `direction ==
// 0` is reading (runtime/types/io/Serializer.h:230).
//
// ---------------------------------------------------------------------------
// CALLEES — what is ours, what is a DOOR, and what this port COSTS
// ---------------------------------------------------------------------------
//   0x1800c9930  ?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z
//                DOOR, by address. The same door goal_serialize.cpp:147 and
//                elementdescription_serialize.cpp:155 already open — not a new
//                edge on the cutover surface, an existing one reused.
//   0x180029a80  Array<String>::SetSize (unexported, two-argument form)
//                PORTED — runtime/src/collection/array.cpp:2055 registers it,
//                so ResolveAddr (registry-first) hands back OUR body.
//   0x180029510  Array<uint>::SetSize (unexported)
//                PORTED — array.cpp:2002.
//   0x180029bb0  Array-of-String serialize (unexported, ~680 bytes)
//                PORTED — BATCH 8b, runtime/src/io/text_codec.cpp registers it,
//                so ResolveAddr (registry-first) hands back OUR body. It WAS a
//                new address door this file opened, and the door was honest;
//                it is now closed rather than merely declared.
//
// SO THIS PORT GREW THE CUTOVER SURFACE BY ONE FUNCTION for a day, and that was
// stated here rather than discovered later: the pattern is
// reference_porting_can_grow_the_cutover_surface.md. The trade it weighed —
// "reproducing a 680-byte UTF-8/UTF-16 transcoder to avoid one door is not a
// better trade" — was the right call at the time and is now moot: the
// transcoder came with the four text-codec internals, which had to be ported
// anyway for TextFileStream.
//
// ---------------------------------------------------------------------------
// ORACLE — the emission oracle, WRITING direction, FULL replay
// ---------------------------------------------------------------------------
// "No oracle possible" has been wrong four times across three waves for exactly
// this shape (reference_write_path_serialize_is_replayable.md), and it is wrong
// here too. On the write path this body mutates NOTHING:
//   * the three `SetSize` calls are behind retail's own reading-only guard;
//   * `String::Serialize` on the write path emits and does not touch the field;
//   * 0x180029bb0 on the write path takes its `direction != 0` arm at
//     0x180029c44 and 0x180029c66 and reduces to `kScalarUInt(len|0x80000000)`
//     then `kArrayChar(len, data)` per element — emission only.
// So NO PREFIX STOP is needed; the whole body replays.
//
// **NO GAMEPLAY ROUTE, AND THAT IS NOT THE SAME AS NO ORACLE**
// (reference_no_route_is_not_no_oracle.md). Nothing in the game writes a .pkg —
// that is the ToolKit's job — so this body has no natural caller and would
// report `calls=0` forever. It is therefore driven from
// `RtIo_PackageFile_SweepProbe` (package_file.cpp), which runs at the tail of
// LoadPackages with every mounted package fully populated, i.e. against REAL
// subjects rather than a fabricated one. The probe is gated on
// the old DLL-era switch and does nothing otherwise.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../types/io/Serializer.h"
#include "../../types/io/PackageFile.h"   // B.3.4 PACKAGE+FS: the real class
#include "../../types/system/String.h"
#include "../../types/collection/Collection.h"

using rt::io::Stream;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;

namespace {

// B3R RT-IO: the three by-address doors that stood here are direct calls now:
//   ?Serialize@String@System@@ 0x1800c9930   -> System::String::Serialize
//   the array-of-String walker  0x180029bb0  -> RtIo_SerializeStringArray
//     (text_codec.cpp). Its 4th parameter is not an argument: retail spills
//     R9 to its home slot at 0x180029bbb and only ever WRITES that slot again
//     (0x180029d00) before reading it, so the value passed is never read.
//   Array<T>::SetSize 0x180029a80 / 0x180029510 -> rtcoll::RtColl_ArraySetSize_*

// ---- prologue / epilogue, from 0x1800b60ca and 0x1800b626e ---------------
// Identical to goal_serialize.cpp's and elementdescription_serialize.cpp's,
// including the THIRD argument being 0: retail zeroes R8D at 0x1800b60ce for
// the `CMP [RDX+0x8],R8D` and never reloads it before the factory call, so the
// save-format version reaching BinaryReader/BinaryWriter's ctor is 0.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;                            // 1800b60ca
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader  // 1800b60de
                         : rt::io::encoding_vtbl::kCreateWriter; // 1800b60e3
    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw =
        reinterpret_cast<FactoryFn>(enc->vtable[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;                                      // 1800b60e6
    return rw;
}

// B3X W0: the four ReaderWriter calls by name. `rw` is the rt::io POD view of
// the object the Encoding factory built.
inline ::IO::ReaderWriter* Rw(ReaderWriter* rw) {
    return reinterpret_cast<::IO::ReaderWriter*>(rw);   // B.3.x bridge
}

void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;                                             // 1800b6261
    delete Rw(rw);                                     // slot 0, flags 1
}

void RecInt(ReaderWriter* rw, void* v) {
    Rw(rw)->SerializeInt(*static_cast<int32_t*>(v), nullptr);    // +0xa0
}

void RecBool(ReaderWriter* rw, void* v) {
    Rw(rw)->SerializeBool(*static_cast<bool*>(v), nullptr);      // +0x78
}

// `SerializeUIntArray` is (count, uint*, name) — EDX, R8, R9 at 0x1800b624c-0x1800b6259.
void RecArrayUInt(ReaderWriter* rw, int32_t n, void* data) {
    Rw(rw)->SerializeUIntArray(n, static_cast<uint32_t*>(data), nullptr);   // +0x40
}

// The five unnamed offsets this body touches. PackageFileView (FileSystem.h)
// models +0x20..+0x38 and +0x80..+0x88 as `_unknown` padding, and this lane does
// NOT rename them in the shared header: a Serialize pins a field's TYPE and its
// POSITION IN THE RECORD STREAM and says nothing about its meaning, and
// runtime/types/io/FileSystem.h:101 already refuses to invent names on exactly
// that argument for +0x58/+0x60. So they are reached by explicit offset here,
// where the disagreement is visible at the point of use.
inline void*   ArrayAt(void* self, int off) {
    return static_cast<uint8_t*>(self) + off;
}
inline int32_t CountAt(const void* self, int off) {
    return *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(self) + off);
}
inline void*   DataAt(void* self, int off) {
    return *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + off);
}
inline void*   FieldAt(void* self, int off) {
    return static_cast<uint8_t*>(self) + off;
}

}  // namespace

// ENDGAME B.3.9 IO: the ?Serialize@PackageFile slot door and its registration
// are gone with the minted ??_7PackageFile row, their only reader; the body is
// the member below, reached through the class's own table.

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — `IO::PackageFile::Serialize`, vtable slot 5
// ============================================================================
//
// A member of `IO::X` must be DEFINED in a namespace ENCLOSING the class
// (B34_BRIEFS section A.2), so it lives here rather than in the anonymous
// namespace with its helpers.
//
// *** THIS FILE IS WHERE B34_RULES section 7.2's GREP HAZARD IS LIVE. *** It
// carries `using rt::io::Stream; using rt::io::Serializer;` and friends at
// GLOBAL scope, and inside `namespace IO` a bare `Serializer` or `Stream` would
// bind to the REAL CLASS instead -- silently, and with a different size. So
// every POD in this body is spelled `rt::io::X` in full, and there is
// deliberately no `using namespace` anywhere in it.
//
// THE FIELD OFFSETS ARE GONE, AND THAT IS THE WIN. The body used to reach
// eleven fields through `FieldAt(self, 0xNN)` / `CountAt(self, 0xNN)` /
// `ArrayAt(self, 0xNN)` because no header named them. runtime/types/io/
// PackageFile.h names them now, each with a static_assert on its offset, so the
// record's SHAPE is checked by the compiler instead of by four spellings of a
// hex constant. The record ORDER is retail's and is untouched: a Serialize body
// that emits fields in a different order writes a different file.
namespace IO {

bool PackageFile::Serialize(void* streamv) {
    rt::io::Stream* const stream = static_cast<rt::io::Stream*>(streamv);
    rt::io::Serializer ser;
    rt::io::ReaderWriter* const rw = MakeCodec(stream, &ser);

    reinterpret_cast<::System::String*>(&_name)->Serialize(&ser);  // 1800b60ea  B.3.x bridge
    reinterpret_cast<::System::String*>(&_author)->Serialize(&ser);  // 1800b60ff  B.3.x bridge
    reinterpret_cast<::System::String*>(&_description)->Serialize(&ser);  // 1800b610c  B.3.x bridge
    reinterpret_cast<::System::String*>(&_string58)->Serialize(&ser);  // 1800b6119  B.3.x bridge
    reinterpret_cast<::System::String*>(&_string60)->Serialize(&ser);  // 1800b6126  B.3.x bridge

    RecInt(rw, &_userVersion);                // 1800b6139
    RecInt(rw, &_priority);                   // 1800b614d

    // The three counted arrays. Each: copy the count to a local, serialize the
    // LOCAL, size the array from it on the reading side only, then hand the
    // element walker the count and the data pointer. See the banner.
    {
        int32_t n = _stringArray20Count;                          // 1800b6153
        RecInt(rw, &n);                                           // 1800b6167
        if (ser.stream->direction == rt::io::kDirectionReading)    // 1800b6171
            rtcoll::RtColl_ArraySetSize_180029a80(&_stringArray20, n);  // 1800b617e
        RtIo_SerializeStringArray(&ser, n, _stringArray20, nullptr);  // 1800b618e
    }
    {
        int32_t m = _stringArray30Count;                          // 1800b6193
        RecInt(rw, &m);                                           // 1800b61a7
        if (ser.stream->direction == rt::io::kDirectionReading)    // 1800b61b1
            rtcoll::RtColl_ArraySetSize_180029a80(&_stringArray30, m);  // 1800b61be
        RtIo_SerializeStringArray(&ser, m, _stringArray30, nullptr);  // 1800b61ce
    }

    RecBool(rw, &_overridable);               // 1800b61e4
    RecInt(rw, &_iconWidth);                  // 1800b61f5
    RecInt(rw, &_iconHeight);                 // 1800b6209

    {
        int32_t k = _iconCount;                                   // 1800b620f
        RecInt(rw, &k);                                           // 1800b6226
        if (ser.stream->direction == rt::io::kDirectionReading)    // 1800b6230
            rtcoll::RtColl_ArraySetSize_180029510(&_iconData, k);       // 1800b6240
        RecArrayUInt(rw, k, _iconData);                            // 1800b6259
    }

    DestroyCodec(rw);                         // 1800b626e
    return true;                              // 1800b6270  MOV AL,1
}

}  // namespace IO
