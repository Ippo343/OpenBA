#pragma once
// ============================================================================
// runtime/types/io/Serializer.h — IO::Serializer / Stream / Encoding /
// ReaderWriter binary-compatible layouts.   Wave 4, lane A.
// ============================================================================
//
// Every offset below is pinned by a named instruction in RETAIL
// Runtime-x64.dll (image base 0x180000000), read out of the shipped binary on
// 2026-09-06. Nothing here was taken from Runtime-x64-profile.dll: the profile
// build's ReaderWriter vtable OFFSETS differ from retail's
// (reference_encoding_vtable_offsets_differ_between_builds.md), so it is a
// source of NAMES only, never of numbers.
//
// ----------------------------------------------------------------------------
// THE FOUR CLASSES, AND THE ONE THAT WAS CONFLATED
// ----------------------------------------------------------------------------
// engine/types/io_serializer.h calls BOTH `serializer+0x08` and `stream+0x10`
// an `Encoding*`, and describes the second as "the encoding factory". They are
// two DIFFERENT classes, and the export table says so:
//
//   IO::Encoding      — stream+0x10. Installed by ?SetEncoding@Stream@IO@@,
//                       returned by ?GetEncoding@Stream@IO@@ and by
//                       ?GetEncoding@Serializer@IO@@ (whose mangling
//                       `AEBVEncoding@2@` is what names the class). It is the
//                       encoding POLICY object: its vtable slot 3 (+0x18)
//                       makes a reader, slot 4 (+0x20) makes a writer. It has
//                       no non-virtual exported members at all.
//
//   IO::ReaderWriter  — serializer+0x08. Abstract read/write interface,
//                       vtable ??_7ReaderWriter@IO@@6B@ = 0x1801553b0.
//                       Concrete: IO::BinaryReader (vtable 0x1801552e0) and
//                       IO::BinaryWriter (vtable 0x180155210). THIS is the
//                       object every Serializer::Serialize forwarder
//                       tail-jumps through. Wave 3 lane A already called it
//                       ReaderWriter and was right.
//
//   IO::Stream        — base only here; the concrete stream family is lane B.
//   IO::Serializer    — 16 bytes, { Stream*, ReaderWriter* }.
//
// ----------------------------------------------------------------------------
// THE `int version` CTOR ARGUMENT IS **NOT** IGNORED
// ----------------------------------------------------------------------------
// serializer.cpp and engine/types/io_serializer.h both said "the version ctor
// arg is IGNORED". That is wrong, and it matters: the argument is the save
// format version stamped into the stream.
//
// ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z (0x1800b9960) never touches R8D, and
// R8D is the third argument. It stays live across the whole body and is still
// in R8 at `CALL qword ptr [RAX + 0x18]` / `[RAX + 0x20]`, so the factory slot
// is called with THREE arguments (factory, Serializer*, version) — which is
// exactly the signature of what it constructs:
//     ??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z   (Serializer&, int)
//     ??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z   (Serializer&, int)
// and BinaryWriter's ctor (0x1800a8a30) stores R8D at this+0x28 and then
// WRITES that dword to the stream. So `version` ends up in the file.
//
// (On the reading path BinaryReader stores R8D at +0x28 too and then
// immediately overwrites it with a 4-byte read from the stream — which is why
// the argument LOOKS dead if you only read the load path.)
//
// This also settles the arity question the Wave 4 brief left open: the two
// factory slot calls take (this, Serializer*, int), arity 3, pinned by
// `MOV RDX,RBX` at 0x1800b997f plus R8D's untouched liveness.
//
// ----------------------------------------------------------------------------
// CONSTRUCTING A SERIALIZER MOVES THE STREAM CURSOR — 12 BYTES
// ----------------------------------------------------------------------------
// This is the fact that decides Wave 4 lane A's whole oracle story, so it is
// recorded here and not only in the wave doc.
//
//   BinaryReader::BinaryReader  (0x1800a8340) calls stream->vtable[+0x18]
//     (Read) three times with count=4, and stream->vtable[+0x28] once.
//   BinaryWriter::BinaryWriter  (0x1800a8a30) calls stream->vtable[+0x10]
//     (Write) three times with count=4, and stream->vtable[+0x28] twice.
//   BinaryReader::~BinaryReader (0x1800a82f0) reads a further 4 bytes.
//
// So `IO::Serializer s(stream, v);` is NOT a cheap wrapper construction: it
// consumes or emits a 12-byte header. Running the DLL's ctor a second time as
// a differential oracle would consume/emit a SECOND header and desync the
// stream from that point on — the exact failure mode of
// reference_slot3_wrapper_must_resolve_first.md. The ctor and dtor therefore
// have NO runnable oracle, and lane A does not pretend otherwise.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace rt {
namespace io {

// ---------------------------------------------------------------------------
// IO::Encoding — the encoding policy object living at Stream+0x10.
// ---------------------------------------------------------------------------
// Abstract; we only ever touch its vtable. Two slots matter, both pinned by
// the Serializer ctor's `CALL qword ptr [RAX + 0x18]` (0x1800b9984, taken when
// stream->direction == 0) and `CALL qword ptr [RAX + 0x20]` (0x1800b9994).
struct Encoding {
    void** vtable;   // +0x00
};

namespace encoding_vtbl {
    // (this, Serializer*, int version) -> ReaderWriter*   [reading]
    constexpr int kCreateReader = 0x18;
    // (this, Serializer*, int version) -> ReaderWriter*   [writing]
    constexpr int kCreateWriter = 0x20;
}

// ---------------------------------------------------------------------------
// IO::ReaderWriter — serializer+0x08. vtable ??_7ReaderWriter@IO@@6B@.
// ---------------------------------------------------------------------------
// Layout pinned by BinaryReader::BinaryReader (0x1800a8340), which inlines the
// base construction:
//     MOV [RCX],      0x1801553b0     ; ReaderWriter vtable, stamped first
//     MOV byte [RCX+0x08], 0          ; _allowCustomSerialize
//     MOV [RCX+0x10], RDX             ; the Serializer&
//     MOV RAX,[RDX]; MOV [RCX+0x18],RAX ; serializer->_stream
// and by ?AllowCustomSerialize@ReaderWriter@IO@@QEAA_NXZ (0x1800b9100).
struct ReaderWriter {
    void**  vtable;                 // +0x00
    uint8_t allowCustomSerialize;   // +0x08
    uint8_t _pad09[7];              // +0x09
    void*   serializer;             // +0x10  the IO::Serializer that owns it
    void*   stream;                 // +0x18  == serializer->stream
};
static_assert(offsetof(ReaderWriter, allowCustomSerialize) == 0x08, "ReaderWriter+0x08");
static_assert(offsetof(ReaderWriter, serializer)           == 0x10, "ReaderWriter+0x10");
static_assert(offsetof(ReaderWriter, stream)               == 0x18, "ReaderWriter+0x18");

// IO::BinaryReader / IO::BinaryWriter — 0x30 bytes (the factory asks the
// allocator for exactly 0x30, FUN_1800a8120 / FUN_1800a8080).
//   +0x20 reader: a dword read from the stream / writer: a stream position
//   +0x24 reader: stream->vtable[+0x28]()  / writer: back-patch position
//   +0x28 THE VERSION — what ?GetVersion@BinaryReader@IO@@UEBAHXZ returns
//         (0x180023930 is `MOV EAX,[RCX+0x28]; RET`, and BinaryWriter's
//          0x180023ab0 is the same instruction).
struct BinaryCodec {
    ReaderWriter base;    // +0x00 .. +0x1f
    int32_t      word20;  // +0x20
    int32_t      word24;  // +0x24
    int32_t      version; // +0x28
    int32_t      _pad2c;  // +0x2c
};
static_assert(offsetof(BinaryCodec, version) == 0x28, "BinaryCodec.version @ 0x28");
static_assert(sizeof(BinaryCodec) == 0x30, "BinaryCodec is 0x30 bytes (allocator asks for 0x30)");

// The two concrete vtables, exported by name. Used by the ctor's shadow check.
constexpr const char* kBinaryReaderVtableName = "??_7BinaryReader@IO@@6B@";  // 0x1801552e0
constexpr const char* kBinaryWriterVtableName = "??_7BinaryWriter@IO@@6B@";  // 0x180155210

// ---------------------------------------------------------------------------
// ReaderWriter vtable — RETAIL byte offsets.
// ---------------------------------------------------------------------------
// DOUBLY pinned, from two independent sources that agree on every entry:
//
//   (a) each IO::Serializer forwarder's own tail-jump displacement, decoded
//       from the shipped DLL's bytes (see the address table in serializer.cpp);
//   (b) the contents of ??_7BinaryReader@IO@@6B@ (0x1801552e0) and
//       ??_7BinaryWriter@IO@@6B@ (0x180155210), each slot resolving to an
//       exported ?Serialize@Binary{Reader,Writer}@IO@@ whose mangled parameter
//       type matches the forwarder that jumps there.
//
// DO NOT copy these from a profile-build decompile: there the same three slots
// are bool +0x90, uint +0xb0, int +0xb8.
namespace rw_vtbl {
    constexpr int kDtor            = 0x00;  // scalar-deleting dtor(flags)
    constexpr int kGetVersion      = 0x08;  // () -> int
    constexpr int kSkip            = 0x10;  // ()            [?Skip@Serializer@IO@@]
    constexpr int kArrayChar       = 0x18;  // (n, char*,   name)
    constexpr int kArrayBool       = 0x20;  // (n, bool*,   name)
    constexpr int kArrayFloat      = 0x28;  // (n, float*,  name)
    constexpr int kArrayUInt64     = 0x30;  // (n, uint64*, name)
    constexpr int kArrayInt64      = 0x38;  // (n, int64*,  name)
    constexpr int kArrayUInt       = 0x40;  // (n, uint*,   name)
    constexpr int kArrayInt        = 0x48;  // (n, int*,    name)
    constexpr int kArrayUShort     = 0x50;  // (n, ushort*, name)
    constexpr int kArrayShort      = 0x58;  // (n, short*,  name)
    constexpr int kArrayUChar      = 0x60;  // (n, uchar*,  name)
    constexpr int kArraySChar      = 0x68;  // (n, schar*,  name)
    constexpr int kScalarChar      = 0x70;  // (char&,   name)
    constexpr int kScalarBool      = 0x78;  // (bool&,   name)
    constexpr int kScalarFloat     = 0x80;  // (float&,  name)
    constexpr int kScalarUInt64    = 0x88;  // (uint64&, name)
    constexpr int kScalarInt64     = 0x90;  // (int64&,  name)
    constexpr int kScalarUInt      = 0x98;  // (uint&,   name)
    constexpr int kScalarInt       = 0xA0;  // (int&,    name)
    constexpr int kScalarUShort    = 0xA8;  // (ushort&, name)
    constexpr int kScalarShort     = 0xB0;  // (short&,  name)
    constexpr int kScalarUChar     = 0xB8;  // (uchar&,  name)
    constexpr int kScalarSChar     = 0xC0;  // (schar&,  name)
    constexpr int kSerializeBytes  = 0xC8;  // (size, void*, name)
}

// ---------------------------------------------------------------------------
// IO::Stream — base sub-object only. The concrete family is Wave 4 lane B.
// ---------------------------------------------------------------------------
// Pinned by the Stream base ctor (0x1800b99d0), whose whole body is:
//     LEA RAX,[0x1801549a0]  ; ??_7Stream@IO@@6B@
//     MOV [RCX],RAX
//     XOR EAX,EAX
//     MOV dword [RCX+0x08],EAX     ; direction = 0
//     MOV qword [RCX+0x10],RAX     ; encoding  = nullptr
//     MOV RAX,RCX ; RET
// and by ?SetEncoding@Stream@IO@@QEAAXPEAVEncoding@2@@Z (0x1800b99f0) writing
// +0x10, ?GetEncoding@Stream@IO@@QEBAAEBVEncoding@2@XZ (0x1800b9a60) reading it.
//
// +0x08 is IO::Stream::Direction (the enum named by
// ?GetDirection@Serializer@IO@@QEAA?AW4Direction@Stream@2@XZ, 0x180006210,
// which is just `MOV RAX,[RCX]; MOV EAX,[RAX+8]; RET`). 0 == reading: the
// Serializer ctor takes the CreateReader slot when it is 0, and the reader is
// the one that calls Stream::Read.
struct Stream {
    void**    vtable;      // +0x00
    int32_t   direction;   // +0x08  0 = reading/decode, non-zero = writing
    int32_t   _pad0c;      // +0x0C
    Encoding* encoding;    // +0x10  the encoding POLICY object (see above)
};
static_assert(offsetof(Stream, direction) == 0x08, "Stream.direction @ 0x08");
static_assert(offsetof(Stream, encoding)  == 0x10, "Stream.encoding @ 0x10");

constexpr int32_t kDirectionReading = 0;

// ---------------------------------------------------------------------------
// IO::Serializer — 0x10 bytes.
// ---------------------------------------------------------------------------
// Pinned by the ctor (0x1800b9960): `MOV [RCX],RDX` stores the Stream& at
// +0x00 and `MOV qword [RCX+0x08],0` then `MOV [RBX+0x08],RAX` stores the
// ReaderWriter* the factory returned at +0x08. Every Serialize forwarder opens
// with `MOV RCX,[RCX+0x08]`, and ?GetStream@Serializer@IO@@ (0x18000a0a0) is
// `MOV RAX,[RCX]; RET`.
struct Serializer {
    Stream*       stream;        // +0x00
    ReaderWriter* readerWriter;  // +0x08
};
static_assert(offsetof(Serializer, stream)       == 0x00, "Serializer.stream @ 0x00");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "Serializer.readerWriter @ 0x08");
static_assert(sizeof(Serializer) == 0x10, "Serializer is 0x10 bytes");

} // namespace io
} // namespace rt
