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
#include <type_traits>   // B.3.4 block 3: the "IO::Serializer has no vptr" pins

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
// The THIRD ReaderWriter the engine actually produces, and the one that made
// the Wave 4 integration run report 2,053 divergences on a correct port.
// IO::Encoding is a policy object: the Serializer ctor calls whichever factory
// slot the stream's Encoding provides and stores whatever comes back. On the
// resource-reference VISIT pass (WAVE3.md finding 5: ExternalFactory::Add
// re-invokes slot +0x28 against an IO::VisitorStream) that is an IO::Visitor,
// not a BinaryWriter -- 5,687 referenced externals were preloaded on the run
// that surfaced it. Verified by resolving the observed vtable pointer back
// through the run's ASLR delta: 0x1801558b0 == ??_7Visitor@IO@@6B@.
constexpr const char* kVisitorVtableName = "??_7Visitor@IO@@6B@";  // 0x1801558b0

// IO::Visitor — 0x28 bytes (the VISITOR encoding's two factories at
// 0x1800baf00 / 0x1800bae40 ask the allocator for exactly 0x28: MOV EDX,0x28 /
// LEA R8D,[RDX-0x24]). No stream header is read or written: the constructor is
// the inlined ReaderWriter base ctor, the ??_7Visitor@IO@@6B@ stamp, and one
// dword store of the version argument at +0x20 (MOV [RDI+0x20],EBP).
struct VisitorCodec {
    ReaderWriter base;    // +0x00 .. +0x1f
    int32_t      version; // +0x20
    int32_t      _pad24;  // +0x24
};
static_assert(offsetof(VisitorCodec, version) == 0x20, "VisitorCodec.version @ 0x20");
static_assert(sizeof(VisitorCodec) == 0x28, "VisitorCodec is 0x28 bytes (allocator asks for 0x28)");

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

// ===========================================================================
// ENDGAME B.3.4 block 1 — the REAL `IO::Encoding` hierarchy.
// ===========================================================================
//
// Design: docs/runtime_port/endgame_b/B34_RULES.md §1.1 and §2.
// Coexistence proof: tests/unit/io_vtable_abi_test.cpp.
//
// THE ONE SHAPE IN `IO` WHOSE SLOT 0 IS NOT A DESTRUCTOR
// -----------------------------------------------------
// Read byte-wise out of retail Runtime-x64.dll's .rdata on 2026-09-18. All
// THREE Encoding tables are UNEXPORTED, five slots each, and none of them has
// a deleting-destructor slot:
//
//   slot 0  +0x00  IsBinary()     Binary 0x1800a81e0 `MOV AL,1; RET`
//                                 Text   0x1800b9ab0 `XOR AL,AL; RET`
//                                 Visitor 0x1800bafe0 `XOR AL,AL; RET`
//   slot 1  +0x08  IsText()       Binary 0x1800a81d0 / Text 0x1800b9aa0 (true)
//                                 / Visitor 0x1800bafd0
//   slot 2  +0x10  GetKind()      Binary 0x1800a81c0 -> 2
//                                 Text   0x1800b9a90 -> 1
//                                 Visitor 0x1800bafc0 -> 3
//   slot 3  +0x18  CreateReader   Binary 0x1800a8120 / Text 0x1800b9a80 (NULL)
//                                 / Visitor 0x1800baf00
//   slot 4  +0x20  CreateWriter   Binary 0x1800a8080 / Text 0x1800b9a70 (NULL)
//                                 / Visitor 0x1800bae40
//
// Bounding: 0x1801554a8 + 0x28 == 0x1801554d0 (??_7BinaryFileStream@IO@@6B@);
// 0x1801559d0 + 0x28 == 0x1801559f8; 0x1801559f8 + 0x28 == 0x180155a20
// (??_7VisitorStream@IO@@6B@).
//
// **`IsBinary()` MUST BE DECLARED FIRST AND THERE MUST BE NO `DeletingDtor`
// SLOT.** A slot-0 destructor would push both factories down one slot and
// `IO::Serializer`'s constructor would call `GetKind()` as a factory — a
// build-clean, link-clean corruption of every save. The destructor here is
// protected and NON-virtual, which is safe because nothing virtual-deletes an
// Encoding: `IO::Stream`'s destructor frees `_encoding` with a direct
// `MemoryAllocator::MemFree` (runtime/types/io/Stream.h:83-101).
// `encoding_factory_slots_sit_at_0x18_and_0x20_with_arity_three` in
// tests/unit/io_vtable_abi_test.cpp is the regression guard for all of this.
//
// THE CLASS NAMES ARE ATTESTED, NOT INVENTED
// ------------------------------------------
// `IO\BinaryEncoding.cpp`, `IO\TextEncoding.cpp` and `IO\VisitorEncoding.cpp`
// are source-path strings in Runtime-x64-profile.dll (0x1801a0040, 0x1801a2d50,
// 0x1801a4d18) and are also three of the rows in
// docs/runtime_port/original_source_tree.txt. There is no `IO\Encoding.cpp`,
// which is consistent with `IO::Encoding` being a pure interface with no
// out-of-line body. ENDGAME decision 5: names are the original ones.
//
// The three SLOT names (`IsBinary`/`IsText`/`GetKind`) remain this project's
// behaviour-derived reading — no export and no debug string names them. What IS
// now attested is every return VALUE, decoded from the shipped bytes above, and
// the class names. encoding.cpp's older banner called the whole reading
// unattested; that was right when it was written and is now half-superseded.
//
// ARITY 3. `CreateReader`/`CreateWriter` take `(this, Serializer&, int)` —
// pinned by R8D's liveness across the Serializer ctor 0x1800b9960 (see the
// banner at the top of this header) and re-proved live in the unit test.
// `this` is NEVER READ by any of the fifteen bodies: an Encoding is a stateless
// policy tag. That is faithful, not an omission.
// ===========================================================================

namespace IO {

// Forward declarations only. `IO::ReaderWriter` landed in B.3.4 block 2 and
// `IO::Serializer` in block 3; declaring the retail signature NOW means these
// five slots never change shape again, and the two blocks that follow delete a
// bridge cast in encoding.cpp rather than editing this header.
//
// `IO::Stream` IS a real class as of B.3.4 block 4 (STREAMS) — it lives in
// runtime/types/io/Stream.h, which includes THIS header for `IO::Encoding`.
// The forward declaration therefore STAYS: reversing the include would make
// the pair cyclic, and `IO::Serializer` only ever needs `Stream&` / `Stream*`.
// `serializer_class.cpp` includes Stream.h and reads `_direction` /
// `_encoding` off the class; block 4 deleted the `rt::io::Stream` bridge casts
// and moved NO signature, exactly as block 2 did for `IO::ReaderWriter`.
class ReaderWriter;
class Serializer;
class Stream;

class Encoding {
public:
    virtual bool IsBinary() const = 0;                            // slot 0 +0x00
    virtual bool IsText() const = 0;                              // slot 1 +0x08
    virtual int  GetKind() const = 0;                             // slot 2 +0x10
    virtual ReaderWriter* CreateReader(Serializer& ser, int version) = 0;  // +0x18
    virtual ReaderWriter* CreateWriter(Serializer& ser, int version) = 0;  // +0x20

protected:
    // NON-virtual and protected — see the banner. There is no slot for it.
    ~Encoding() = default;
};

// 0x1801554a8 — the SHARED table every concrete stream's constructor installs.
class BinaryEncoding final : public Encoding {
public:
    bool IsBinary() const override { return true; }   // 0x1800a81e0
    bool IsText() const override { return false; }    // 0x1800a81d0
    int  GetKind() const override { return 2; }       // 0x1800a81c0
    ReaderWriter* CreateReader(Serializer& ser, int version) override;  // 0x1800a8120
    ReaderWriter* CreateWriter(Serializer& ser, int version) override;  // 0x1800a8080
    ~BinaryEncoding() = default;
};

// 0x1801559d0 — text serialization was STRIPPED from the shipped DLL: both
// factory bodies are a bare `XOR EAX,EAX; RET`. Reproduced, not "fixed".
class TextEncoding final : public Encoding {
public:
    bool IsBinary() const override { return false; }  // 0x1800b9ab0
    bool IsText() const override { return true; }     // 0x1800b9aa0
    int  GetKind() const override { return 1; }       // 0x1800b9a90
    ReaderWriter* CreateReader(Serializer& ser, int version) override;  // 0x1800b9a80
    ReaderWriter* CreateWriter(Serializer& ser, int version) override;  // 0x1800b9a70
    ~TextEncoding() = default;
};

// 0x1801559f8 — the reference-VISIT pass's encoding. Its two factory bodies are
// BYTE-IDENTICAL in retail (0x1800baf00 and 0x1800bae40), and neither consumes
// nor emits a stream header, unlike the Binary pair.
class VisitorEncoding final : public Encoding {
public:
    bool IsBinary() const override { return false; }  // 0x1800bafe0
    bool IsText() const override { return false; }    // 0x1800bafd0
    int  GetKind() const override { return 3; }       // 0x1800bafc0
    ReaderWriter* CreateReader(Serializer& ser, int version) override;  // 0x1800baf00
    ReaderWriter* CreateWriter(Serializer& ser, int version) override;  // 0x1800bae40
    ~VisitorEncoding() = default;
};

// A vptr and nothing else, exactly like the POD it replaces. The old
// `rt::io::Encoding` stays (B33 §7: the PODs are trivially destructible aliases
// until their last user converts) and is cross-asserted against the class here,
// which is what makes `reinterpret_cast` between the two views honest.
static_assert(sizeof(Encoding) == 8, "IO::Encoding is one vptr");
static_assert(sizeof(Encoding) == sizeof(rt::io::Encoding),
              "IO::Encoding and the rt::io::Encoding POD must stay interchangeable");
static_assert(sizeof(BinaryEncoding) == 8, "no Encoding subclass adds a field");
static_assert(sizeof(TextEncoding) == 8, "no Encoding subclass adds a field");
static_assert(sizeof(VisitorEncoding) == 8, "no Encoding subclass adds a field");

// The two factory slots keep the byte offsets the 15 registered address doors
// and every raw caller already use.
static_assert(rt::io::encoding_vtbl::kCreateReader == 0x18, "Encoding slot 3");
static_assert(rt::io::encoding_vtbl::kCreateWriter == 0x20, "Encoding slot 4");

// ===========================================================================
// ENDGAME B.3.4 block 2 — the REAL `IO::ReaderWriter` hierarchy.
// ===========================================================================
//
// Design: B34_RULES.md §1.2. 26 slots, confirmed FOUR independent ways:
//   (a) `??_7ReaderWriter@IO@@6B@` @ 0x1801553b0 is slot 0 = 0x180023bd0 (the
//       deleting dtor, also RT_DELDTOR(..., "IO::ReaderWriter")) followed by
//       twenty-five `_purecall` (0x18007ec28);
//   (b) the three concrete tables are adjacent and each is exactly 0xd0 bytes:
//       0x180155210 + 0xd0 == 0x1801552e0 + 0xd0 == 0x1801553b0;
//   (c) `rw_vtbl::kSerializeBytes == 0xC8`, so 0xC8/8 == 25 is the last index;
//   (d) each ARRAY body's own virtual self-call displacement names the scalar
//       slot for its type (serialize_family.cpp's header block re-derives all
//       eleven this way).
//
// THE DISTINCT-NAME SCHEME, and why it makes the reversal rule moot.
// ------------------------------------------------------------------
// Retail declares 22 of these 26 as `Serialize` OVERLOADS, and MSVC groups
// overloaded virtuals and REVERSES their declaration order. ENDGAME decision 2
// says to give them distinct names so the issue cannot arise — so they are
// declared HERE IN SLOT ORDER as `Serialize<Type>` / `Serialize<Type>Array`,
// and the mapping to the retail byte offsets is a plain 1:1 that needs no
// reversal argument at all. Anyone trying to recover retail's declaration order
// from this list is recovering a fact we deliberately stopped depending on.
//
// `IO::Serializer`'s NON-virtual forwarders keep the public name `Serialize`
// and stay real C++ overloads — that is block 3, and it is what lets the 861
// caller sites read `ser.Serialize(x, "name")`.
//
// NO VIRTUAL DESTRUCTOR ANYWHERE. Itanium spends two slots on one, which would
// push every slot after it and silently mis-aim the `rw_vtbl::k*` byte offsets
// that 54 files under runtime/src still use.
// `a_virtual_destructor_would_shift_every_ReaderWriter_slot` in
// tests/unit/io_vtable_abi_test.cpp is the guard.
//
// THE SIGNATURES ARE RETAIL'S. The scalar slots take `T&` (mangled `AEAD`,
// `AEA_N`, …) and the array slots take `(int, T*, const char*)` (`PEAD`, …).
// The `const char* name` argument is accepted and IGNORED by every body in the
// family: retail's scalar bodies CLOBBER R8 (which held `name`) with the value
// pointer before the tail jump, and the array bodies pass `nullptr` to the
// elements rather than forwarding it. The binary format carries no field names
// and a reader depends entirely on ORDER. That is a property of the FORMAT.
// ===========================================================================

class ReaderWriter {
public:
    // slot 0  +0x00  rw_vtbl::kDtor        ReaderWriter's own is 0x180023bd0
    virtual ReaderWriter* DeletingDtor(unsigned flags) = 0;
    // slot 1  +0x08  rw_vtbl::kGetVersion
    virtual int  GetVersion() const = 0;
    // slot 2  +0x10  rw_vtbl::kSkip
    virtual void Skip() = 0;

    // slots 3..13 — the eleven ARRAY forms, +0x18 .. +0x68.
    virtual void SerializeCharArray  (int n, char*          arr, const char* name) = 0;
    virtual void SerializeBoolArray  (int n, bool*          arr, const char* name) = 0;
    virtual void SerializeFloatArray (int n, float*         arr, const char* name) = 0;
    virtual void SerializeUInt64Array(int n, uint64_t*      arr, const char* name) = 0;
    virtual void SerializeInt64Array (int n, int64_t*       arr, const char* name) = 0;
    virtual void SerializeUIntArray  (int n, uint32_t*      arr, const char* name) = 0;
    virtual void SerializeIntArray   (int n, int32_t*       arr, const char* name) = 0;
    virtual void SerializeUShortArray(int n, uint16_t*      arr, const char* name) = 0;
    virtual void SerializeShortArray (int n, int16_t*       arr, const char* name) = 0;
    virtual void SerializeUCharArray (int n, unsigned char* arr, const char* name) = 0;
    virtual void SerializeSCharArray (int n, signed char*   arr, const char* name) = 0;

    // slots 14..24 — the eleven SCALAR forms, +0x70 .. +0xC0.
    virtual void SerializeChar  (char&          v, const char* name) = 0;
    virtual void SerializeBool  (bool&          v, const char* name) = 0;
    virtual void SerializeFloat (float&         v, const char* name) = 0;
    virtual void SerializeUInt64(uint64_t&      v, const char* name) = 0;
    virtual void SerializeInt64 (int64_t&       v, const char* name) = 0;
    virtual void SerializeUInt  (uint32_t&      v, const char* name) = 0;
    virtual void SerializeInt   (int32_t&       v, const char* name) = 0;
    virtual void SerializeUShort(uint16_t&      v, const char* name) = 0;
    virtual void SerializeShort (int16_t&       v, const char* name) = 0;
    virtual void SerializeUChar (unsigned char& v, const char* name) = 0;
    virtual void SerializeSChar (signed char&   v, const char* name) = 0;

    // slot 25 +0xC8  rw_vtbl::kSerializeBytes
    virtual void SerializeBytes(int n, void* buf, const char* name) = 0;

    // Fields at the retail offsets `rt::io::ReaderWriter` already pins.
    uint8_t allowCustomSerialize;   // +0x08
    uint8_t _pad09[7];              // +0x09
    void*   serializer;             // +0x10  the IO::Serializer that owns it
    void*   stream;                 // +0x18  == serializer->stream

protected:
    ~ReaderWriter() = default;      // NON-virtual — there is no second slot
};

static_assert(sizeof(ReaderWriter) == 0x20, "IO::ReaderWriter is 0x20 bytes");
static_assert(sizeof(ReaderWriter) == sizeof(rt::io::ReaderWriter),
              "IO::ReaderWriter and the rt::io::ReaderWriter POD stay interchangeable");
static_assert(rt::io::rw_vtbl::kDtor == 0x00, "slot 0");
static_assert(rt::io::rw_vtbl::kSerializeBytes == 0xC8, "slot 25 is the last");
static_assert(rt::io::rw_vtbl::kSerializeBytes / 8 == 25, "26 slots, 0..25");

// ---------------------------------------------------------------------------
// The three concretes.
// ---------------------------------------------------------------------------
// THERE IS DELIBERATELY NO `IO::BinaryCodec` CLASS. `rt::io::BinaryCodec` is
// this project's grouping name for the shared 0x30-byte layout, not a retail
// one — the recovered source tree has `IO\BinaryReader.cpp` and no Codec file —
// and ENDGAME decision 5 says the classes take the ORIGINAL names. So
// `BinaryReader` and `BinaryWriter` each declare the three fields, which is also
// honest about the fact that `word20`/`word24` MEAN different things on the two
// sides (reader: the back-patched record length and the position just after it,
// whose sum `Skip()` seeks to; writer: two back-patch positions).
//
// ⚠ AND THE ONE TRAP IN THE FAMILY: `IO::Visitor`'s version is at +0x20, NOT
// +0x28. `?GetVersion@Visitor@IO@@UEBAHXZ` (0x18002b670) is
// `MOV EAX,[RCX+0x20]; RET` where both BinaryReader's (0x180023930) and
// BinaryWriter's (0x180023ab0) read +0x28. A Visitor has no record framing to
// carry, so its version lands eight bytes earlier. Three identically-named
// GetVersion bodies where one reads a different offset is exactly the shape
// someone "tidies" into uniformity, so the type system carries the fact.

class BinaryReader final : public ReaderWriter {
public:
    ReaderWriter* DeletingDtor(unsigned flags) override = 0;               // 0x180023a10   (body: iobase_vtables.cpp)
    int  GetVersion() const override { return version; }               // 0x180023930
    void Skip() override;                                              // 0x1800a83f0
    void SerializeCharArray  (int, char*,          const char*) override;
    void SerializeBoolArray  (int, bool*,          const char*) override;
    void SerializeFloatArray (int, float*,         const char*) override;
    void SerializeUInt64Array(int, uint64_t*,      const char*) override;
    void SerializeInt64Array (int, int64_t*,       const char*) override;
    void SerializeUIntArray  (int, uint32_t*,      const char*) override;
    void SerializeIntArray   (int, int32_t*,       const char*) override;
    void SerializeUShortArray(int, uint16_t*,      const char*) override;
    void SerializeShortArray (int, int16_t*,       const char*) override;
    void SerializeUCharArray (int, unsigned char*, const char*) override;
    void SerializeSCharArray (int, signed char*,   const char*) override;
    void SerializeChar  (char&,          const char*) override;
    void SerializeBool  (bool&,          const char*) override;
    void SerializeFloat (float&,         const char*) override;
    void SerializeUInt64(uint64_t&,      const char*) override;
    void SerializeInt64 (int64_t&,       const char*) override;
    void SerializeUInt  (uint32_t&,      const char*) override;
    void SerializeInt   (int32_t&,       const char*) override;
    void SerializeUShort(uint16_t&,      const char*) override;
    void SerializeShort (int16_t&,       const char*) override;
    void SerializeUChar (unsigned char&, const char*) override;
    void SerializeSChar (signed char&,   const char*) override;
    void SerializeBytes(int, void*, const char*) override;             // 0x1800a8970
    ~BinaryReader() = default;

    int32_t word20;    // +0x20  the back-patched record LENGTH, read from the stream
    int32_t word24;    // +0x24  GetCurrentPos() just after it
    int32_t version;   // +0x28
    int32_t _pad2c;    // +0x2c
};

class BinaryWriter final : public ReaderWriter {
public:
    ReaderWriter* DeletingDtor(unsigned flags) override = 0;               // 0x180023b50   (body: iobase_vtables.cpp)
    int  GetVersion() const override { return version; }               // 0x180023ab0
    void Skip() override {}   // 0x1800a8b00 is a bare RET: a writer has nothing
                              // ahead of the cursor to skip over. Faithful, not a stub.
    void SerializeCharArray  (int, char*,          const char*) override;
    void SerializeBoolArray  (int, bool*,          const char*) override;
    void SerializeFloatArray (int, float*,         const char*) override;
    void SerializeUInt64Array(int, uint64_t*,      const char*) override;
    void SerializeInt64Array (int, int64_t*,       const char*) override;
    void SerializeUIntArray  (int, uint32_t*,      const char*) override;
    void SerializeIntArray   (int, int32_t*,       const char*) override;
    void SerializeUShortArray(int, uint16_t*,      const char*) override;
    void SerializeShortArray (int, int16_t*,       const char*) override;
    void SerializeUCharArray (int, unsigned char*, const char*) override;
    void SerializeSCharArray (int, signed char*,   const char*) override;
    void SerializeChar  (char&,          const char*) override;
    void SerializeBool  (bool&,          const char*) override;
    void SerializeFloat (float&,         const char*) override;
    void SerializeUInt64(uint64_t&,      const char*) override;
    void SerializeInt64 (int64_t&,       const char*) override;
    void SerializeUInt  (uint32_t&,      const char*) override;
    void SerializeInt   (int32_t&,       const char*) override;
    void SerializeUShort(uint16_t&,      const char*) override;
    void SerializeShort (int16_t&,       const char*) override;
    void SerializeUChar (unsigned char&, const char*) override;
    void SerializeSChar (signed char&,   const char*) override;
    void SerializeBytes(int, void*, const char*) override;             // 0x1800a9070
    ~BinaryWriter() = default;

    int32_t word20;    // +0x20  where the length placeholder lands, for back-patching
    int32_t word24;    // +0x24  the position just after it
    int32_t version;   // +0x28
    int32_t _pad2c;    // +0x2c
};

// Every one of its 23 Serialize slots, and Skip, is a three-byte `RET` in the
// shipped DLL (0x1800bacc0..0x1800bae30, sixteen bytes apart). That is the whole
// body, not a stub: IO::Visitor is the resource-REFERENCE discovery pass, so
// being blind to scalar data is the design.
class Visitor final : public ReaderWriter {
public:
    ReaderWriter* DeletingDtor(unsigned flags) override = 0;               // 0x18002b710   (body: iobase_vtables.cpp)
    int  GetVersion() const override { return version; }               // 0x18002b670
    void Skip() override {}
    void SerializeCharArray  (int, char*,          const char*) override {}
    void SerializeBoolArray  (int, bool*,          const char*) override {}
    void SerializeFloatArray (int, float*,         const char*) override {}
    void SerializeUInt64Array(int, uint64_t*,      const char*) override {}
    void SerializeInt64Array (int, int64_t*,       const char*) override {}
    void SerializeUIntArray  (int, uint32_t*,      const char*) override {}
    void SerializeIntArray   (int, int32_t*,       const char*) override {}
    void SerializeUShortArray(int, uint16_t*,      const char*) override {}
    void SerializeShortArray (int, int16_t*,       const char*) override {}
    void SerializeUCharArray (int, unsigned char*, const char*) override {}
    void SerializeSCharArray (int, signed char*,   const char*) override {}
    void SerializeChar  (char&,          const char*) override {}
    void SerializeBool  (bool&,          const char*) override {}
    void SerializeFloat (float&,         const char*) override {}
    void SerializeUInt64(uint64_t&,      const char*) override {}
    void SerializeInt64 (int64_t&,       const char*) override {}
    void SerializeUInt  (uint32_t&,      const char*) override {}
    void SerializeInt   (int32_t&,       const char*) override {}
    void SerializeUShort(uint16_t&,      const char*) override {}
    void SerializeShort (int16_t&,       const char*) override {}
    void SerializeUChar (unsigned char&, const char*) override {}
    void SerializeSChar (signed char&,   const char*) override {}
    void SerializeBytes(int, void*, const char*) override {}
    ~Visitor() = default;

    int32_t version;   // +0x20  NOT +0x28 — see the trap note above
    int32_t _pad24;    // +0x24
};

static_assert(sizeof(BinaryReader) == 0x30, "the factory asks MemAlloc for 0x30");
static_assert(sizeof(BinaryWriter) == 0x30, "the factory asks MemAlloc for 0x30");
static_assert(sizeof(Visitor)      == 0x28, "the factory asks MemAlloc for 0x28");
static_assert(sizeof(BinaryReader) == sizeof(rt::io::BinaryCodec), "POD agrees");
static_assert(sizeof(BinaryWriter) == sizeof(rt::io::BinaryCodec), "POD agrees");
static_assert(sizeof(Visitor)      == sizeof(rt::io::VisitorCodec), "POD agrees");

// Every field pinned against the same retail instruction the POD is pinned
// against. `-Wno-invalid-offsetof` has been in SA_CXXFLAGS and UNIT_CXXFLAGS
// since B.3.3 (Makefile:59, :120), which is what lets a polymorphic type keep
// decision 1's layout pins.
static_assert(offsetof(ReaderWriter, allowCustomSerialize) == 0x08, "ReaderWriter+0x08");
static_assert(offsetof(ReaderWriter, serializer)           == 0x10, "ReaderWriter+0x10");
static_assert(offsetof(ReaderWriter, stream)               == 0x18, "ReaderWriter+0x18");
static_assert(offsetof(BinaryReader, word20)  == 0x20, "BinaryReader+0x20");
static_assert(offsetof(BinaryReader, word24)  == 0x24, "BinaryReader+0x24");
static_assert(offsetof(BinaryReader, version) == 0x28, "0x180023930 MOV EAX,[RCX+0x28]");
static_assert(offsetof(BinaryWriter, word20)  == 0x20, "BinaryWriter+0x20");
static_assert(offsetof(BinaryWriter, word24)  == 0x24, "BinaryWriter+0x24");
static_assert(offsetof(BinaryWriter, version) == 0x28, "0x180023ab0 MOV EAX,[RCX+0x28]");
// THE TRAP, pinned so it cannot be "tidied" into uniformity with the other two:
static_assert(offsetof(Visitor, version) == 0x20,
              "0x18002b670 is MOV EAX,[RCX+0x20] -- Visitor's version is EIGHT "
              "bytes earlier than BinaryReader's and BinaryWriter's");

// ===========================================================================
// ENDGAME B.3.4 block 3 — the REAL `IO::Serializer`.
// ===========================================================================
//
// Design: B34_RULES.md §1.2 rule 2, §3.1, §4.3, §7.3. Bodies:
// runtime/src/io/serializer_class.cpp (registry-free). Doors + registration:
// runtime/src/io/serializer.cpp.
//
// *** IT HAS NO VTABLE AND MUST NEVER GET ONE. ***
// -----------------------------------------------
// All 22 exported members are `QEAA`/`QEBA` — non-virtual — and each one
// tail-jumps into the ReaderWriter's table. A vptr here would make the class
// 0x18 bytes instead of 0x10 and every one of the 861 `g_fn_Serializer_*`
// caller sites would write through the wrong offsets. The `static_assert`s
// below and `Serializer_is_layout_identical_to_both_legacy_spellings` in
// tests/unit/io_vtable_abi_test.cpp are the guards.
//
// Being non-polymorphic is also what lets this ONE class convert its
// constructor while every stream class must not (§4.3): converting a ctor is
// dangerous only when it changes which vtable a live object wears, and this
// object wears none.
//
// *** THE LIFETIME IS THE WHOLE RISK OF THIS CLASS. ***
// ----------------------------------------------------
// Retail 0x1800b9960, re-read from the shipped bytes on 2026-09-19:
//
//     MOV [RCX],RDX                 _stream = &stream
//     MOV qword [RCX+0x08],0        _readerWriter = nullptr
//     CMP dword [RDX+0x08],0        stream.direction
//     MOV RCX,[RDX+0x10]            the Encoding (policy object)
//     MOV RAX,[RCX]                 its vtable
//     MOV RDX,RBX                   arg 2 = this
//     JNZ writing
//     CALL [RAX+0x18] / [RAX+0x20]  arg 3 = R8D = version, NEVER TOUCHED
//     MOV [RBX+0x08],RAX
//     MOV RAX,RBX ; RET
//
// and 0x1800b99b0:
//
//     MOV RCX,[RCX+0x08] ; TEST RCX,RCX ; JZ out
//     MOV RAX,[RCX] ; MOV EDX,1 ; JMP [RAX]     ; slot 0, flags == 1
//
// The ReaderWriter that constructor creates CONSUMES (reader) or EMITS
// (writer) a TWELVE-BYTE stream header in its own constructor, and
// `~BinaryReader` reads a FURTHER FOUR BYTES on the way out while
// `~BinaryWriter` back-patches its placeholder. So constructing and destroying
// an `IO::Serializer` are STREAM OPERATIONS with a position in a sequence, not
// cheap wrapper lifetime events:
//
//   * `~Serializer()` is NOT IDEMPOTENT. Destroying twice reads or patches
//     twice and desyncs the file from that point on. That is why the copy
//     constructor and copy assignment are DELETED, and why §7.3 forbids an
//     explicit destructor call on a typed local (the implicit one at scope
//     exit would then be the second).
//   * Moving a construction or a destruction one statement relative to any
//     other stream operation IS A SAVE-FORMAT CHANGE. No guard, no early-out
//     and no reorder may be added anywhere in this class (CLAUDE.md: retail's
//     unguarded derefs are load-bearing).
//
// THE PUBLIC NON-VIRTUAL DESTRUCTOR is deliberate and is not decision 2's
// protected-base shape: nothing derives from `Serializer`, callers own one by
// value or in a raw buffer, and `runtime/src/object/internal_class.cpp`
// destroys one explicitly at the exact retail position.
//
// THE 22 MEMBERS KEEP THEIR RAW `rw_vtbl::k*` SLOT CALLS (§3.1). They do NOT
// become `_readerWriter->SerializeInt(v, name)`. The member call is correct
// only once nothing hands a Serializer a MINTED-table ReaderWriter any more,
// and §4.2 says that is the end of B.3, not this step. A faithful partial
// beats a complete-looking deletion. The win is already banked: these bodies
// are the LAST place a caller spells those byte offsets.
// ===========================================================================

class Serializer {
public:
    // ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z   0x1800b9960
    // `version` is NOT ignored: it reaches the factory as the THIRD argument
    // and `BinaryWriter` writes it into the file. See the banner at the top of
    // this header for R8D's liveness proof.
    Serializer(Stream& stream, int version);

    // ??1Serializer@IO@@QEAA@XZ                 0x1800b99b0
    // NON-virtual (there is no vtable) and PUBLIC. Retail does NOT null
    // `_readerWriter` afterwards; neither do we.
    ~Serializer();

    // A copy would destroy the same ReaderWriter twice — see the banner.
    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    // ---- the 17 ported `Serialize` OVERLOADS -------------------------------
    // ONE name, real C++ overloads, because retail's are (§1.2 rule 2) and
    // because that is what makes the 861 caller sites mechanical: overload
    // resolution picks the width from the reference's type. The 22 VIRTUALS
    // they forward to are the ones that need distinct names.
    //
    // Retail also exports `Serialize(char&)` (0x18007c3e0, +0x70) and the
    // int64/uint64 scalar and array forms. They are ABSENT ON PURPOSE: no call
    // site in this tree reaches them, and an uncalled port is an unverified
    // port. Adding one is a porting decision, not a tidy-up.
    void Serialize(bool&          v, const char* name);   // 0x1800061e0  +0x78
    void Serialize(signed char&   v, const char* name);   // 0x18007c3b0  +0xC0
    void Serialize(unsigned char& v, const char* name);   // 0x180055c80  +0xB8
    void Serialize(short&         v, const char* name);   // 0x18007c3c0  +0xB0
    void Serialize(unsigned short&v, const char* name);   // 0x1800675a0  +0xA8
    void Serialize(int&           v, const char* name);   // 0x1800061d0  +0xA0
    void Serialize(unsigned int&  v, const char* name);   // 0x1800061f0  +0x98
    void Serialize(float&         v, const char* name);   // 0x180021660  +0x80

    void Serialize(int n, char*           d, const char* name);  // 0x180034fd0 +0x18
    void Serialize(int n, signed char*    d, const char* name);  // 0x18007c3f0 +0x68
    void Serialize(int n, bool*           d, const char* name);  // 0x180052920 +0x20
    void Serialize(int n, unsigned char*  d, const char* name);  // 0x180054160 +0x60
    void Serialize(int n, short*          d, const char* name);  // 0x18007c400 +0x58
    void Serialize(int n, unsigned short* d, const char* name);  // 0x180034fe0 +0x50
    void Serialize(int n, int*            d, const char* name);  // 0x180077660 +0x48
    void Serialize(int n, unsigned int*   d, const char* name);  // 0x180029e60 +0x40
    void Serialize(int n, float*          d, const char* name);  // 0x1800150d0 +0x28

    // ?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z  0x1800346d0  +0xC8
    // Its own name in retail, not a `Serialize` overload, because the payload
    // is `void*` and would otherwise swallow every pointer.
    void SerializeBytes(int n, void* buf, const char* name);

    // ?GetVersion@Serializer@IO@@QEBAHXZ               0x1800070b0  +0x08
    int GetVersion() const;

    // ?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ 0x180006290
    // `MOV RAX,[RCX]; MOV RAX,[RAX+0x10]; RET` — two loads, no dispatch. It
    // returns the STREAM's encoding policy object, not this Serializer's
    // ReaderWriter. The reference return is retail's (`AEBVEncoding@2@`).
    const Encoding& GetEncoding();

    // ---- the 16 bytes, at retail's offsets ---------------------------------
    // PUBLIC for the same reason `Object::Internal`'s are: unconverted bodies
    // across the tree read them at these offsets through a cast, and
    // `offsetof` on them has to be well formed for the pins below.
    Stream*       _stream;        // +0x00
    ReaderWriter* _readerWriter;  // +0x08
};

static_assert(sizeof(Serializer) == 0x10, "IO::Serializer is 16 bytes");
static_assert(sizeof(Serializer) == sizeof(rt::io::Serializer),
              "IO::Serializer and the rt::io::Serializer POD stay interchangeable");
static_assert(offsetof(Serializer, _stream)       == 0x00, "Serializer.stream @ 0x00");
static_assert(offsetof(Serializer, _readerWriter) == 0x08, "Serializer.readerWriter @ 0x08");
static_assert(offsetof(Serializer, _stream) == offsetof(rt::io::Serializer, stream),
              "alias stream");
static_assert(offsetof(Serializer, _readerWriter) == offsetof(rt::io::Serializer, readerWriter),
              "alias readerWriter");
// THE GUARD. `offsetof` above is already unconditionally well formed only
// because this type is standard layout — i.e. because it has no vptr. Said
// again as an assertion so a future `virtual` fails the build rather than the
// save file.
static_assert(!std::is_polymorphic<Serializer>::value,
              "IO::Serializer must never acquire a vtable: it would become 0x18 "
              "bytes and every g_fn_Serializer_* caller site would write "
              "through the wrong offsets");
static_assert(std::is_standard_layout<Serializer>::value,
              "the 861 caller sites reach these two fields through a cast");

} // namespace IO
