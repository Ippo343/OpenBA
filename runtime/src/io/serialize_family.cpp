// ============================================================================
// runtime/src/io/serialize_family.cpp — the IO scalar-serialization dispatch
// family: 73 vtable slots across IO::BinaryReader, IO::BinaryWriter and
// IO::Visitor.   Wave 9 lane SERIAL.
// ============================================================================
//
// Lane ENC-2 ported two members of this family (?Serialize@BinaryWriter@IO@@ at
// 0x1800a8fb0 and 0x1800a8fd0, in binary_writer.cpp) and the framing tail around
// them. This file is the other 73. Those two stay where they are and are NOT
// re-registered here — a duplicate registration is last-binding-wins and would
// silently make one of the two bodies dead code.
//
// The three classes are the three concrete IO::ReaderWriter implementations the
// engine produces (runtime/types/io/Serializer.h names all three and explains
// why the Visitor exists). Each has the SAME 26-slot vtable shape:
//
//     +0x00        scalar-deleting dtor            (UNEXPORTED, not this lane)
//     +0x08        GetVersion() -> int
//     +0x10        Skip()
//     +0x18..+0x68 Serialize(int n, T* arr, const char* name)   x11 types
//     +0x70..+0xC0 Serialize(T& value,       const char* name)  x11 types
//     +0xC8        SerializeBytes(int n, void* buf, const char* name)
//
// and the eleven types are, in slot order:
//
//     char, bool, float, unsigned __int64, __int64, unsigned int, int,
//     unsigned short, short, unsigned char, signed char
//
// (MSVC manglings D, _N, M, _K, _J, I, H, G, F, E, C.)
//
// ----------------------------------------------------------------------------
// THE SLOT TABLE IS RE-DERIVED HERE, NOT INHERITED
// ----------------------------------------------------------------------------
// Serializer.h's `rw_vtbl` constants were pinned in Wave 4 from the Serializer
// forwarders' tail-jump displacements and from the vtable contents. This lane
// confirmed all eleven scalar slots a THIRD and independent way, out of bytes
// this family carries itself: each ARRAY body ends in a virtual self-call whose
// displacement is the scalar slot for its own type.
//
//     0x1800a8410 Serialize[](char)   ... CALL [RAX+0x70]   == kScalarChar
//     0x1800a8460 Serialize[](bool)   ... CALL [RAX+0x78]   == kScalarBool
//     0x1800a84b0 Serialize[](float)  ... CALL [RAX+0x80]   == kScalarFloat
//     0x1800a8510 Serialize[](u64)    ... CALL [RAX+0x88]   == kScalarUInt64
//     0x1800a8570 Serialize[](i64)    ... CALL [RAX+0x90]   == kScalarInt64
//     0x1800a85d0 Serialize[](uint)   ... CALL [RAX+0x98]   == kScalarUInt
//     0x1800a8630 Serialize[](int)    ... CALL [RAX+0xa0]   == kScalarInt
//     0x1800a8690 Serialize[](ushort) ... CALL [RAX+0xa8]   == kScalarUShort
//     0x1800a86f0 Serialize[](short)  ... CALL [RAX+0xb0]   == kScalarShort
//     0x1800a8750 Serialize[](uchar)  ... CALL [RAX+0xb8]   == kScalarUChar
//     0x1800a87b0 Serialize[](schar)  ... CALL [RAX+0xc0]   == kScalarSChar
//
// All eleven agree with `rw_vtbl`, and the BinaryWriter set is byte-identical to
// the BinaryReader set at the same displacements. That third derivation is worth
// having written down because the profile build DISAGREES about these offsets
// (reference_encoding_vtable_offsets_differ_between_builds.md), so a future lane
// reading a profile decompile will find bool +0x90 / uint +0xb0 / int +0xb8 and
// needs to know which source won and why.
//
// ----------------------------------------------------------------------------
// THE FOUR BODY SHAPES — this whole family is four functions and a table
// ----------------------------------------------------------------------------
//
// (1) SCALAR, BinaryReader (0x1800a8810 + 0x20*i), five instructions:
//
//         MOV RCX,[RCX+0x18]     ; RCX = this->stream
//         MOV R8,RDX             ; R8  = &value   <-- OVERWRITES the `name` arg
//         MOV EDX,<sizeof(T)>
//         MOV RAX,[RCX]
//         JMP [RAX+0x18]         ; TAIL JUMP to Stream::Read(size, buf)
//
//     BinaryWriter's (0x1800a8f10 + 0x20*i) is the same five instructions with
//     `JMP [RAX+0x10]` — Stream::Write. Lane ENC-2 established this shape on two
//     slots; the other nine per class are byte-identical to them apart from the
//     size immediate, which is sizeof(T) for all eleven types.
//
//     `const char* name` is not merely unused, it is CLOBBERED before the call:
//     R8 held it on entry and is overwritten with the value pointer. **The
//     binary format carries no field names and a reader depends entirely on
//     ORDER.** That is a property of the FORMAT, not of these functions.
//
// (2) SerializeBytes (0x1800a8970 / 0x1800a9070), three instructions:
//
//         MOV RCX,[RCX+0x18]
//         MOV RAX,[RCX]
//         JMP [RAX+0x18]         ; reader; writer jumps [RAX+0x10]
//
//     No register shuffling at all, because the incoming (int count, void* buf)
//     ALREADY sit in RDX/R8 exactly as Stream::Read(size, buf) wants them. Same
//     body as the scalar case with the marshalling deleted.
//
// (3) ARRAY (0x1800a8410.. / 0x1800a8b10..), one loop, identical in both
//     classes — the array forms do NOT touch the stream at all:
//
//         TEST EDX,EDX / JLE ret          ; count <= 0 -> nothing, not even a
//                                         ;   vtable load
//         RBX = arr ; RSI = this ; EDI = count
//     loop: MOV RAX,[RSI]                 ; this->vtable  (RELOADED each pass)
//         XOR R8D,R8D                     ; name = nullptr   <-- NOT forwarded
//         MOV RDX,RBX ; MOV RCX,RSI
//         CALL [RAX+<scalar slot>]        ; VIRTUAL self-dispatch
//         ADD RBX,<sizeof(T)> ; DEC RDI ; JNZ loop
//
//     Two things here are load-bearing and neither is guessable:
//
//     * **The array form DROPS `name` and passes nullptr.** It does not forward
//       the array's own name to the elements. Anything downstream that reads the
//       name argument (nothing in this family does — see (1)) sees null for every
//       element of every array.
//     * **The dispatch is VIRTUAL and the vtable is RELOADED INSIDE THE LOOP.**
//       So a subclass overriding only the scalar slot gets its override honoured
//       for array serialization too, and a body that re-stamps `this->vtable`
//       mid-array would be observed. Calling the scalar body directly would be
//       observationally different and is not what is written here.
//
//     The count guard is `<= 0`, signed: a negative count returns rather than
//     looping ~4 billion times.
//
// (4) VISITOR — every one of its 23 Serialize slots, and Skip, is `RET`.
//
//         0x1800bacc0..0x1800bae30, sixteen bytes apart, each `C2 00 00`.
//
//     This is not a stub this lane is declining to port: it is the whole shipped
//     body. IO::Visitor is the resource-reference discovery pass (Serializer.h,
//     and WAVE3.md finding 5: ExternalFactory::Add re-invokes the encoding's
//     factory slot against an IO::VisitorStream, and 5,687 referenced externals
//     were preloaded on the run that surfaced it). It walks the object graph to
//     find REFERENCES and is deliberately blind to scalar data, so every scalar
//     slot returning immediately is the design, not an omission.
//
// ----------------------------------------------------------------------------
// ⚠ THE ONE TRAP IN THE FAMILY: Visitor's version field is at +0x20, NOT +0x28
// ----------------------------------------------------------------------------
//     ?GetVersion@BinaryReader@IO@@UEBAHXZ  0x180023930   MOV EAX,[RCX+0x28]
//     ?GetVersion@BinaryWriter@IO@@UEBAHXZ  0x180023ab0   MOV EAX,[RCX+0x28]
//     ?GetVersion@Visitor@IO@@UEBAHXZ       0x18002b670   MOV EAX,[RCX+0x20]
//
// IO::Visitor is NOT a BinaryCodec. It shares the IO::ReaderWriter base (+0x00
// vtable, +0x08 allowCustomSerialize, +0x10 serializer, +0x18 stream) and then
// diverges: it has no `word20`/`word24` record framing to carry, so its version
// lands eight bytes earlier. Three identically-named GetVersion functions where
// one reads a different offset is exactly the shape that gets "fixed" into
// uniformity by the next person to tidy this file, so Visitor gets its own
// struct below rather than borrowing BinaryCodec — the type system is made to
// carry the fact instead of a comment.
//
// ----------------------------------------------------------------------------
// BinaryReader::Skip vs BinaryWriter::Skip — asymmetric, and the reader's Skip
// is what finally explains the reader's word20/word24
// ----------------------------------------------------------------------------
//     0x1800a83f0  ?Skip@BinaryReader@IO@@UEAAXXZ
//         MOV RAX,[RCX+0x18]      ; stream
//         MOV EDX,[RCX+0x24]      ; word24
//         ADD EDX,[RCX+0x20]      ; + word20
//         MOV R8,[RAX] ; MOV RCX,RAX
//         JMP [R8+0x20]           ; TAIL JUMP to Stream::SetCurrentPos(pos)
//
//     0x1800a8b00  ?Skip@BinaryWriter@IO@@UEAAXXZ      RET
//
// So on the reading side `word20 + word24` is the ONE-PAST-THE-END position of
// the current record, and Skip is "abandon the rest of this record and jump to
// the next one" — the reader's half of the length-prefixed, magic-bracketed
// frame whose writing half lane ENC-2 documented in binary_writer.cpp. Serializer.h
// describes the reader's +0x20 as "a dword read from the stream" and +0x24 as
// "stream->vtable[+0x28]()", i.e. the back-patched LENGTH and the position just
// after it; their sum being the record end is consistent with that and is the
// first thing in the tree to state what the pair is FOR on the read path.
//
// BinaryWriter::Skip is a bare RET because a writer cannot skip: there is
// nothing ahead of the cursor to jump over. Reproduced as an empty body, which
// is byte-faithful, not a stub.
//
// ----------------------------------------------------------------------------
// DOOR ARITHMETIC — this lane opens NOTHING, and that is checkable
// ----------------------------------------------------------------------------
// Every body here is either a field read or a dispatch through a vtable pointer
// the caller already handed us (the stream's, for shapes 1-2; our own, for shape
// 3). **Not one of the 73 calls ResolveExport, ResolveExportRaw, ResolveAddr, or
// RT_EXPORT_ONCE.** There is no MemFree, no vtable name, no helper. So the
// predicted door set-diff for this file is the empty set in BOTH directions:
// zero addresses or names OPENED, zero CLOSED. That prediction is recorded in
// WAVE9_LANE_SERIAL.md before it was measured, and it is falsifiable by grepping
// this file for `Resolve`.
//
// ----------------------------------------------------------------------------
// WHY MACROS, IN A TREE THAT PREFERS SPELLING THINGS OUT
// ----------------------------------------------------------------------------
// Forty-four of these bodies differ from one another only in a type name and a
// size immediate. Hand-typing forty-four near-identical functions is precisely
// the situation in which one wrong one hides inside a uniform-looking pass — the
// failure recorded in reference_uniform_population_is_not_a_verification.md,
// where a ported inline strcmp returned 0 for every pair sharing a first byte and
// read as fully verified. A macro cannot get the eleventh one wrong while the
// other ten are right: there is one body, instantiated eleven times, and the
// size immediate is `sizeof(T)` rather than a transcribed number.
//
// The registration table below is spelled out in full, because THAT is where a
// transcription error is possible (a mangled name or an address can be wrong
// without the compiler noticing) and where the probe cannot help — a
// mis-registered body would be verified perfectly and then wired to the wrong
// slot. serialize_family_probe.cpp checks the manglings against the shipped
// export table for exactly this reason.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

namespace RtIo {

using rt::io::BinaryCodec;
using rt::io::ReaderWriter;
namespace rw = rt::io::rw_vtbl;

// ---------------------------------------------------------------------------
// IO::Visitor — the ReaderWriter base, then a version at +0x20.
// ---------------------------------------------------------------------------
// See the trap note in the header block. Pinned by
// ?GetVersion@Visitor@IO@@UEBAHXZ (0x18002b670), whose whole body is
// `MOV EAX,[RCX+0x20]; RET`. Deliberately a DIFFERENT type from BinaryCodec so
// that using one where the other belongs is a compile error rather than an
// eight-byte silent misread.
struct Visitor {
    ReaderWriter base;      // +0x00 .. +0x1f
    int32_t      version;   // +0x20   <-- NOT +0x28
    int32_t      _pad24;    // +0x24
};
static_assert(offsetof(Visitor, version) == 0x20,
              "IO::Visitor version @ +0x20 -- 0x18002b670 is MOV EAX,[RCX+0x20]");

namespace {

// ---- stream dispatch ------------------------------------------------------
// Shapes (1) and (2) tail-jump through the STREAM's vtable. Offsets from
// Serializer.h's stream_vtbl; repeated here as named constants so the two
// direction-specific slots sit beside the bodies that use them.
constexpr int kStreamWrite  = 0x10;   // Write(size, buf)  -- BinaryWriter
constexpr int kStreamRead   = 0x18;   // Read (size, buf)  -- BinaryReader
constexpr int kStreamSetPos = 0x20;   // SetCurrentPos(pos)

using StreamRwFn   = bool (*)(void* self, int32_t size, void* buf);
using StreamSeekFn = bool (*)(void* self, int32_t pos);

inline void* VSlot(void* obj, int off) {
    return (*reinterpret_cast<void***>(obj))[off / 8];
}

// The scalar bodies' single instruction of work.
inline void StreamMove(int slot, void* stream, int32_t size, void* buf) {
    reinterpret_cast<StreamRwFn>(VSlot(stream, slot))(stream, size, buf);
}

// Shape (3)'s virtual self-dispatch. Signature of every scalar slot.
using ScalarSlotFn = void (*)(void* self, void* value, const char* name);

// The array loop, once. `slot` is the scalar vtable offset for T.
//
// Faithful to 0x1800a8410's shape in the three ways that are observable:
// the `<= 0` signed guard returns without loading the vtable; the vtable is
// re-read from `self` on every iteration; and `name` is dropped in favour of
// nullptr. Retail walks a pointer and counts down a register — index arithmetic
// on T* is the same sequence of addresses.
template <typename T>
inline void SerializeArray(int slot, void* self, int32_t count, T* arr) {
    if (count <= 0) return;
    for (int32_t i = 0; i < count; ++i) {
        ScalarSlotFn f = reinterpret_cast<ScalarSlotFn>(VSlot(self, slot));
        f(self, &arr[i], nullptr);          // name is DROPPED, not forwarded
    }
}

}  // namespace

// ===========================================================================
// IO::BinaryReader — 25 slots
// ===========================================================================

// ?GetVersion@BinaryReader@IO@@UEBAHXZ   0x180023930   MOV EAX,[RCX+0x28]; RET
extern "C" int32_t RtIo_BinaryReader_GetVersion(BinaryCodec* self) {
    return self->version;
}

// ?Skip@BinaryReader@IO@@UEAAXXZ   0x1800a83f0
// Seek to word20 + word24 == one past the end of the current record.
extern "C" void RtIo_BinaryReader_Skip(BinaryCodec* self) {
    void* s = self->base.stream;
    const int32_t end = self->word24 + self->word20;
    reinterpret_cast<StreamSeekFn>(VSlot(s, kStreamSetPos))(s, end);
}

// The eleven scalar slots and the eleven array slots.
//
// SUFFIX is only there to make eleven distinct C++ symbols; MANGLED in the
// registration table is what actually names the slot.
#define RT_SERIAL_READER_PAIR(SUFFIX, T, SCALAR_SLOT, ARRAY_SLOT)              \
    extern "C" void RtIo_BinaryReader_Ser##SUFFIX(                             \
            BinaryCodec* self, T* value, const char* name) {                   \
        (void)name;                     /* CLOBBERED by retail before use */   \
        StreamMove(kStreamRead, self->base.stream, (int32_t)sizeof(T), value); \
    }                                                                          \
    extern "C" void RtIo_BinaryReader_SerArr##SUFFIX(                          \
            BinaryCodec* self, int32_t count, T* arr, const char* name) {      \
        (void)name;                     /* dropped, not forwarded -- see (3) */\
        SerializeArray<T>(SCALAR_SLOT, self, count, arr);                      \
    }                                                                          \
    static_assert(sizeof(T) == (SCALAR_SLOT == rw::kScalarChar   ? 1 :         \
                                SCALAR_SLOT == rw::kScalarBool   ? 1 :         \
                                SCALAR_SLOT == rw::kScalarFloat  ? 4 :         \
                                SCALAR_SLOT == rw::kScalarUInt64 ? 8 :         \
                                SCALAR_SLOT == rw::kScalarInt64  ? 8 :         \
                                SCALAR_SLOT == rw::kScalarUInt   ? 4 :         \
                                SCALAR_SLOT == rw::kScalarInt    ? 4 :         \
                                SCALAR_SLOT == rw::kScalarUShort ? 2 :         \
                                SCALAR_SLOT == rw::kScalarShort  ? 2 :         \
                                SCALAR_SLOT == rw::kScalarUChar  ? 1 : 1),     \
                  "reader " #SUFFIX ": sizeof(T) disagrees with the size "     \
                  "immediate retail's MOV EDX carries for this slot");         \
    static_assert((ARRAY_SLOT) == (SCALAR_SLOT) - 0x58,                        \
                  "reader " #SUFFIX ": the array slot is always the scalar "   \
                  "slot minus 0x58 (11 slots x 8 bytes)")

RT_SERIAL_READER_PAIR(Char,   char,               rw::kScalarChar,   rw::kArrayChar);
RT_SERIAL_READER_PAIR(Bool,   bool,               rw::kScalarBool,   rw::kArrayBool);
RT_SERIAL_READER_PAIR(Float,  float,              rw::kScalarFloat,  rw::kArrayFloat);
RT_SERIAL_READER_PAIR(UInt64, uint64_t,           rw::kScalarUInt64, rw::kArrayUInt64);
RT_SERIAL_READER_PAIR(Int64,  int64_t,            rw::kScalarInt64,  rw::kArrayInt64);
RT_SERIAL_READER_PAIR(UInt,   uint32_t,           rw::kScalarUInt,   rw::kArrayUInt);
RT_SERIAL_READER_PAIR(Int,    int32_t,            rw::kScalarInt,    rw::kArrayInt);
RT_SERIAL_READER_PAIR(UShort, uint16_t,           rw::kScalarUShort, rw::kArrayUShort);
RT_SERIAL_READER_PAIR(Short,  int16_t,            rw::kScalarShort,  rw::kArrayShort);
RT_SERIAL_READER_PAIR(UChar,  unsigned char,      rw::kScalarUChar,  rw::kArrayUChar);
RT_SERIAL_READER_PAIR(SChar,  signed char,        rw::kScalarSChar,  rw::kArraySChar);

// ?SerializeBytes@BinaryReader@IO@@UEAAXHPEAXPEBD@Z   0x1800a8970
// The count comes straight through: (int, void*) already sit where
// Stream::Read(size, buf) wants them, which is why retail emits no marshalling.
extern "C" void RtIo_BinaryReader_SerializeBytes(BinaryCodec* self, int32_t count,
                                                 void* buf, const char* name) {
    (void)name;
    StreamMove(kStreamRead, self->base.stream, count, buf);
}

// ===========================================================================
// IO::BinaryWriter — 23 slots here; 0x1800a8fb0 / 0x1800a8fd0 belong to
// binary_writer.cpp (lane ENC-2) and are deliberately absent.
// ===========================================================================

// ?GetVersion@BinaryWriter@IO@@UEBAHXZ   0x180023ab0
extern "C" int32_t RtIo_BinaryWriter_GetVersion(BinaryCodec* self) {
    return self->version;
}

// ?Skip@BinaryWriter@IO@@UEAAXXZ   0x1800a8b00   — a bare RET. A writer has
// nothing ahead of the cursor to skip over.
extern "C" void RtIo_BinaryWriter_Skip(BinaryCodec* self) {
    (void)self;
}

#define RT_SERIAL_WRITER_PAIR(SUFFIX, T, SCALAR_SLOT, ARRAY_SLOT)              \
    extern "C" void RtIo_BinaryWriter_Ser##SUFFIX(                             \
            BinaryCodec* self, T* value, const char* name) {                   \
        (void)name;                                                            \
        StreamMove(kStreamWrite, self->base.stream, (int32_t)sizeof(T), value);\
    }                                                                          \
    extern "C" void RtIo_BinaryWriter_SerArr##SUFFIX(                          \
            BinaryCodec* self, int32_t count, T* arr, const char* name) {      \
        (void)name;                                                            \
        SerializeArray<T>(SCALAR_SLOT, self, count, arr);                      \
    }                                                                          \
    static_assert((ARRAY_SLOT) == (SCALAR_SLOT) - 0x58,                        \
                  "writer " #SUFFIX ": array slot == scalar slot - 0x58")

RT_SERIAL_WRITER_PAIR(Char,   char,          rw::kScalarChar,   rw::kArrayChar);
RT_SERIAL_WRITER_PAIR(Bool,   bool,          rw::kScalarBool,   rw::kArrayBool);
RT_SERIAL_WRITER_PAIR(Float,  float,         rw::kScalarFloat,  rw::kArrayFloat);
RT_SERIAL_WRITER_PAIR(UInt64, uint64_t,      rw::kScalarUInt64, rw::kArrayUInt64);
RT_SERIAL_WRITER_PAIR(Int64,  int64_t,       rw::kScalarInt64,  rw::kArrayInt64);
RT_SERIAL_WRITER_PAIR(UShort, uint16_t,      rw::kScalarUShort, rw::kArrayUShort);
RT_SERIAL_WRITER_PAIR(Short,  int16_t,       rw::kScalarShort,  rw::kArrayShort);
RT_SERIAL_WRITER_PAIR(UChar,  unsigned char, rw::kScalarUChar,  rw::kArrayUChar);
RT_SERIAL_WRITER_PAIR(SChar,  signed char,   rw::kScalarSChar,  rw::kArraySChar);

// uint and int: lane ENC-2 owns the two SCALAR bodies. The two ARRAY bodies are
// still this lane's, and they dispatch through the vtable into ENC-2's — which
// is the ordinary case, since the dispatch is virtual and slot-based either way.
extern "C" void RtIo_BinaryWriter_SerArrUInt(BinaryCodec* self, int32_t count,
                                             uint32_t* arr, const char* name) {
    (void)name;
    SerializeArray<uint32_t>(rw::kScalarUInt, self, count, arr);
}
extern "C" void RtIo_BinaryWriter_SerArrInt(BinaryCodec* self, int32_t count,
                                            int32_t* arr, const char* name) {
    (void)name;
    SerializeArray<int32_t>(rw::kScalarInt, self, count, arr);
}

// ?SerializeBytes@BinaryWriter@IO@@UEAAXHPEAXPEBD@Z   0x1800a9070
extern "C" void RtIo_BinaryWriter_SerializeBytes(BinaryCodec* self, int32_t count,
                                                 void* buf, const char* name) {
    (void)name;
    StreamMove(kStreamWrite, self->base.stream, count, buf);
}

// ===========================================================================
// IO::Visitor — 25 slots, of which 24 are `RET`.
// ===========================================================================
//
// One body serves all 23 Serialize slots plus Skip. They are separate
// REGISTRATIONS (each mangled name and each address is its own door) but there
// is no reason for them to be separate C++ functions: the shipped bodies are the
// same three bytes and giving each its own empty function would invent eleven
// distinctions the binary does not make.
//
// The two signatures differ only in arity, and both are reached through a vtable
// slot, so the ABI is whatever the caller pushed — a callee that returns
// immediately reads none of it. Declared with the real parameter lists anyway so
// the registration table type-checks.
extern "C" void RtIo_Visitor_Nop(void* self) {
    (void)self;
}
extern "C" void RtIo_Visitor_SerScalar(void* self, void* value, const char* name) {
    (void)self; (void)value; (void)name;
}
extern "C" void RtIo_Visitor_SerArray(void* self, int32_t count, void* arr,
                                      const char* name) {
    (void)self; (void)count; (void)arr; (void)name;
}

// ?GetVersion@Visitor@IO@@UEBAHXZ   0x18002b670   MOV EAX,[RCX+0x20]; RET
// +0x20. See the trap note in the header block.
extern "C" int32_t RtIo_Visitor_GetVersion(Visitor* self) {
    return self->version;
}

}  // namespace RtIo

// ============================================================================
// Registration — both doors for all 73.
// ============================================================================
// Every row gets RegisterRuntimeExport (the name door, which is what the mint's
// RT_MINT_NAMED slot resolves through at cutover) AND RegisterRuntimeAddr (the
// address door, which is what a by-address caller and a RT_MINT_UNNAMED slot
// use). reference_by_address_door_dies_at_cutover.md and
// reference_count_the_doors_not_the_calls.md between them say why one is not
// enough: a port reachable through only one of the two doors is invisible to
// callers using the other, and both doors exist in this tree today.
//
// `n` counts NAME registrations, matching the convention in the other IO files.
//
// ----------------------------------------------------------------------------
// WHY THESE 146 CALLS ARE SPELLED OUT INSTEAD OF WRAPPED IN A MACRO
// ----------------------------------------------------------------------------
// A first draft of this file wrote them as `RT_SERIAL_REG(name, addr, fn)`, one
// line per slot. It compiled, it registered correctly, and it was WRONG, for a
// reason that has now cost this project six separate lanes:
//
//     tools/runtime_census.py matches
//         RE_REG_EXPORT = RegisterRuntimeExport\s*\(\s*(?=")
//         RE_REG_ADDR   = RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)
//
// -- a LITERAL call with a literal first argument. A macro wrapper is invisible
// to it, exactly as a name spelled through a `const char* const k...` is
// (reference_census_cannot_see_nonliteral_registrations.md), exactly as the
// dx11 name table was (WAVE9.md census defect 1), and exactly as
// reference_literal_grep_blind_to_variables.md says it will keep being.
//
// So the macro would have made **73 live ports read as unported to every
// instrument in the tree**, including the mint-wall sweep this lane exists to
// satisfy. The deliverable here IS visibility to a sweep; a spelling that
// defeats the sweep defeats the lane.
//
// The macro was caught by this lane's own row checker returning 73 WALL after
// the port was written and building — i.e. by a prediction (P1 in
// WAVE9_LANE_SERIAL_PREDICTIONS.md) FAILING, which is what predictions are for.
// It is recorded here rather than quietly fixed because the next person to look
// at 146 repetitive lines will want to collapse them, and this is the reason not
// to.
int RtIoSerializeFamilyRegister() {
    using namespace RtIo;
    int n = 0;


    // ---- IO::BinaryReader -------------------------------------------------
    RegisterRuntimeExport("?GetVersion@BinaryReader@IO@@UEBAHXZ",
                          (void*)&RtIo_BinaryReader_GetVersion); ++n;
    RegisterRuntimeAddr(0x180023930, (void*)&RtIo_BinaryReader_GetVersion);
    RegisterRuntimeExport("?Skip@BinaryReader@IO@@UEAAXXZ",
                          (void*)&RtIo_BinaryReader_Skip); ++n;
    RegisterRuntimeAddr(0x1800a83f0, (void*)&RtIo_BinaryReader_Skip);

    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEADPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrChar); ++n;
    RegisterRuntimeAddr(0x1800a8410, (void*)&RtIo_BinaryReader_SerArrChar);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEA_NPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrBool); ++n;
    RegisterRuntimeAddr(0x1800a8460, (void*)&RtIo_BinaryReader_SerArrBool);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAMPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrFloat); ++n;
    RegisterRuntimeAddr(0x1800a84b0, (void*)&RtIo_BinaryReader_SerArrFloat);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEA_KPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrUInt64); ++n;
    RegisterRuntimeAddr(0x1800a8510, (void*)&RtIo_BinaryReader_SerArrUInt64);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEA_JPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrInt64); ++n;
    RegisterRuntimeAddr(0x1800a8570, (void*)&RtIo_BinaryReader_SerArrInt64);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAIPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrUInt); ++n;
    RegisterRuntimeAddr(0x1800a85d0, (void*)&RtIo_BinaryReader_SerArrUInt);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAHPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrInt); ++n;
    RegisterRuntimeAddr(0x1800a8630, (void*)&RtIo_BinaryReader_SerArrInt);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAGPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrUShort); ++n;
    RegisterRuntimeAddr(0x1800a8690, (void*)&RtIo_BinaryReader_SerArrUShort);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAFPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrShort); ++n;
    RegisterRuntimeAddr(0x1800a86f0, (void*)&RtIo_BinaryReader_SerArrShort);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEAEPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrUChar); ++n;
    RegisterRuntimeAddr(0x1800a8750, (void*)&RtIo_BinaryReader_SerArrUChar);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXHPEACPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerArrSChar); ++n;
    RegisterRuntimeAddr(0x1800a87b0, (void*)&RtIo_BinaryReader_SerArrSChar);

    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEADPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerChar); ++n;
    RegisterRuntimeAddr(0x1800a8810, (void*)&RtIo_BinaryReader_SerChar);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEA_NPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerBool); ++n;
    RegisterRuntimeAddr(0x1800a8830, (void*)&RtIo_BinaryReader_SerBool);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAMPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerFloat); ++n;
    RegisterRuntimeAddr(0x1800a8850, (void*)&RtIo_BinaryReader_SerFloat);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEA_KPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerUInt64); ++n;
    RegisterRuntimeAddr(0x1800a8870, (void*)&RtIo_BinaryReader_SerUInt64);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEA_JPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerInt64); ++n;
    RegisterRuntimeAddr(0x1800a8890, (void*)&RtIo_BinaryReader_SerInt64);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAIPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerUInt); ++n;
    RegisterRuntimeAddr(0x1800a88b0, (void*)&RtIo_BinaryReader_SerUInt);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAHPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerInt); ++n;
    RegisterRuntimeAddr(0x1800a88d0, (void*)&RtIo_BinaryReader_SerInt);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAGPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerUShort); ++n;
    RegisterRuntimeAddr(0x1800a88f0, (void*)&RtIo_BinaryReader_SerUShort);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAFPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerShort); ++n;
    RegisterRuntimeAddr(0x1800a8910, (void*)&RtIo_BinaryReader_SerShort);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEAEPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerUChar); ++n;
    RegisterRuntimeAddr(0x1800a8930, (void*)&RtIo_BinaryReader_SerUChar);
    RegisterRuntimeExport("?Serialize@BinaryReader@IO@@UEAAXAEACPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerSChar); ++n;
    RegisterRuntimeAddr(0x1800a8950, (void*)&RtIo_BinaryReader_SerSChar);

    RegisterRuntimeExport("?SerializeBytes@BinaryReader@IO@@UEAAXHPEAXPEBD@Z",
                          (void*)&RtIo_BinaryReader_SerializeBytes); ++n;
    RegisterRuntimeAddr(0x1800a8970, (void*)&RtIo_BinaryReader_SerializeBytes);

    // ---- IO::BinaryWriter -------------------------------------------------
    // NOTE: ?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z (0x1800a8fb0) and
    // ...AEAHPEBD@Z (0x1800a8fd0) are ABSENT ON PURPOSE — binary_writer.cpp
    // (lane ENC-2) registers both, and a second registration here would be
    // last-binding-wins and would quietly make one of the two bodies dead code.
    RegisterRuntimeExport("?GetVersion@BinaryWriter@IO@@UEBAHXZ",
                          (void*)&RtIo_BinaryWriter_GetVersion); ++n;
    RegisterRuntimeAddr(0x180023ab0, (void*)&RtIo_BinaryWriter_GetVersion);
    RegisterRuntimeExport("?Skip@BinaryWriter@IO@@UEAAXXZ",
                          (void*)&RtIo_BinaryWriter_Skip); ++n;
    RegisterRuntimeAddr(0x1800a8b00, (void*)&RtIo_BinaryWriter_Skip);

    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEADPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrChar); ++n;
    RegisterRuntimeAddr(0x1800a8b10, (void*)&RtIo_BinaryWriter_SerArrChar);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEA_NPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrBool); ++n;
    RegisterRuntimeAddr(0x1800a8b60, (void*)&RtIo_BinaryWriter_SerArrBool);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAMPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrFloat); ++n;
    RegisterRuntimeAddr(0x1800a8bb0, (void*)&RtIo_BinaryWriter_SerArrFloat);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEA_KPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrUInt64); ++n;
    RegisterRuntimeAddr(0x1800a8c10, (void*)&RtIo_BinaryWriter_SerArrUInt64);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEA_JPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrInt64); ++n;
    RegisterRuntimeAddr(0x1800a8c70, (void*)&RtIo_BinaryWriter_SerArrInt64);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAIPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrUInt); ++n;
    RegisterRuntimeAddr(0x1800a8cd0, (void*)&RtIo_BinaryWriter_SerArrUInt);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAHPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrInt); ++n;
    RegisterRuntimeAddr(0x1800a8d30, (void*)&RtIo_BinaryWriter_SerArrInt);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAGPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrUShort); ++n;
    RegisterRuntimeAddr(0x1800a8d90, (void*)&RtIo_BinaryWriter_SerArrUShort);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAFPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrShort); ++n;
    RegisterRuntimeAddr(0x1800a8df0, (void*)&RtIo_BinaryWriter_SerArrShort);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEAEPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrUChar); ++n;
    RegisterRuntimeAddr(0x1800a8e50, (void*)&RtIo_BinaryWriter_SerArrUChar);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXHPEACPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerArrSChar); ++n;
    RegisterRuntimeAddr(0x1800a8eb0, (void*)&RtIo_BinaryWriter_SerArrSChar);

    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEADPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerChar); ++n;
    RegisterRuntimeAddr(0x1800a8f10, (void*)&RtIo_BinaryWriter_SerChar);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEA_NPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerBool); ++n;
    RegisterRuntimeAddr(0x1800a8f30, (void*)&RtIo_BinaryWriter_SerBool);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAMPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerFloat); ++n;
    RegisterRuntimeAddr(0x1800a8f50, (void*)&RtIo_BinaryWriter_SerFloat);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEA_KPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerUInt64); ++n;
    RegisterRuntimeAddr(0x1800a8f70, (void*)&RtIo_BinaryWriter_SerUInt64);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEA_JPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerInt64); ++n;
    RegisterRuntimeAddr(0x1800a8f90, (void*)&RtIo_BinaryWriter_SerInt64);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAGPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerUShort); ++n;
    RegisterRuntimeAddr(0x1800a8ff0, (void*)&RtIo_BinaryWriter_SerUShort);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAFPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerShort); ++n;
    RegisterRuntimeAddr(0x1800a9010, (void*)&RtIo_BinaryWriter_SerShort);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAEPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerUChar); ++n;
    RegisterRuntimeAddr(0x1800a9030, (void*)&RtIo_BinaryWriter_SerUChar);
    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEACPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerSChar); ++n;
    RegisterRuntimeAddr(0x1800a9050, (void*)&RtIo_BinaryWriter_SerSChar);

    RegisterRuntimeExport("?SerializeBytes@BinaryWriter@IO@@UEAAXHPEAXPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerializeBytes); ++n;
    RegisterRuntimeAddr(0x1800a9070, (void*)&RtIo_BinaryWriter_SerializeBytes);

    // ---- IO::Visitor ------------------------------------------------------
    RegisterRuntimeExport("?GetVersion@Visitor@IO@@UEBAHXZ",
                          (void*)&RtIo_Visitor_GetVersion); ++n;
    RegisterRuntimeAddr(0x18002b670, (void*)&RtIo_Visitor_GetVersion);
    RegisterRuntimeExport("?Skip@Visitor@IO@@UEAAXXZ",
                          (void*)&RtIo_Visitor_Nop); ++n;
    RegisterRuntimeAddr(0x1800bacc0, (void*)&RtIo_Visitor_Nop);

    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEADPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bacd0, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEA_NPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bace0, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAMPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bacf0, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEA_KPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad00, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEA_JPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad10, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAIPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad20, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAHPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad30, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAGPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad40, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAFPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad50, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEAEPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad60, (void*)&RtIo_Visitor_SerArray);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXHPEACPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bad70, (void*)&RtIo_Visitor_SerArray);

    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEADPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bad80, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEA_NPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bad90, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAMPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bada0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEA_KPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800badb0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEA_JPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800badc0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAIPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800badd0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAHPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bade0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAGPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800badf0, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAFPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bae00, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEAEPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bae10, (void*)&RtIo_Visitor_SerScalar);
    RegisterRuntimeExport("?Serialize@Visitor@IO@@UEAAXAEACPEBD@Z",
                          (void*)&RtIo_Visitor_SerScalar); ++n;
    RegisterRuntimeAddr(0x1800bae20, (void*)&RtIo_Visitor_SerScalar);

    RegisterRuntimeExport("?SerializeBytes@Visitor@IO@@UEAAXHPEAXPEBD@Z",
                          (void*)&RtIo_Visitor_SerArray); ++n;
    RegisterRuntimeAddr(0x1800bae30, (void*)&RtIo_Visitor_SerArray);

    return n;
}
