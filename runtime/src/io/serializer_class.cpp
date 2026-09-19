// ============================================================================
// runtime/src/io/serializer_class.cpp — `IO::Serializer`'s member bodies.
// ENDGAME B.3.4 block 3.
// ============================================================================
//
// THIS FILE IS DELIBERATELY REGISTRY-FREE, and that is its whole reason to
// exist as a separate translation unit.
// ---------------------------------------------------------------------------
// `runtime/src/io/serializer.cpp` holds the 22 registered `RtSerializer_*`
// doors and `RtIoSerializerRegister()`, so it stands on `RegisterRuntimeExport`
// and cannot link into `make unit`. Nothing in the 22 BODIES needs the
// registry, though: the constructor reaches the factory through the stream's
// Encoding VTABLE (a raw slot call), every Serialize member reaches its target
// through the ReaderWriter's vtable, `GetEncoding` is two loads and the
// destructor is one raw slot call. So the bodies live here, this file links
// into BOTH the standalone build (picked up by the Makefile's
// `$(wildcard runtime/src/*/*.cpp)`) and `make unit` (tests/unit/sources.mk),
// and `tests/unit/io_vtable_abi_test.cpp` exercises the REAL class rather than
// a local mirror of it. A mirror tests itself.
//
// This is exactly the split `runtime/src/object/type_reflection.cpp` (B.3.3
// step 3) and `runtime/src/object/internal_class.cpp` (step 5) already use:
// class bodies here, ABI shims and name/address doors there.
//
// ---------------------------------------------------------------------------
// WHAT MAY NOT CHANGE IN THIS FILE, EVER
// ---------------------------------------------------------------------------
// Constructing an `IO::Serializer` consumes (reading) or emits (writing) a
// TWELVE-BYTE stream header, and destroying one reads or back-patches FOUR
// more. The header block of runtime/types/io/Serializer.h carries the retail
// disassembly of both. Therefore:
//
//   * the constructor makes exactly ONE factory call, with exactly the three
//     arguments retail passes, in retail's order, with `version` reaching the
//     factory as the third;
//   * the destructor makes exactly ONE slot-0 call with flags == 1, under
//     retail's own `TEST RCX,RCX` and nothing else;
//   * no null check, no guard, no early-out and no reordering is added
//     anywhere (CLAUDE.md: retail's unguarded derefs are load-bearing, and a
//     guard skips the behaviour rather than restoring it).
//
// THE RAW `rw_vtbl::k*` SLOT CALLS STAY (B34_RULES.md §3.1). They do not
// become `_readerWriter->SerializeInt(v, name)` in this step: a live
// ReaderWriter still wears the MINTED table 0x1801552e0 whose slots resolve
// back to registered doors, and §4.2 keeps every one of those rows alive until
// the end of B.3. These 22 bodies are nonetheless the LAST place a CALLER
// spells those offsets, which is the win the block was for.
//
// THE `rt::io::Stream` BRIDGE CASTS ARE GONE (B.3.4 block 4). Block 3 read
// `direction` and `encoding` through the POD because `IO::Stream` was only
// forward-declared; it is a real class now, so the constructor and
// `GetEncoding` read `_direction` / `_encoding` off the class directly. NO
// SIGNATURE MOVED — block 3 already carried retail's final `(Stream&, int)`,
// exactly as block 2 did for `IO::ReaderWriter`.
//
// PORTABILITY: no <windows.h>, no port registry, no allocator. The only
// includes are the two layout headers (PLAN §4.1 decision 1).
// ============================================================================

#include "../../types/io/Serializer.h"
#include "../../types/io/Stream.h"

#include <cstdint>

// ---------------------------------------------------------------------------
// The one instruction all 22 bodies compile to: `JMP [RAX + off]`.
// ---------------------------------------------------------------------------
// Faithfully UNGUARDED. Retail dereferences `_readerWriter` and its vtable with
// no null test, and that fault is how a Serializer used before construction
// announces itself.
//
// INSIDE `namespace IO` A BARE `ReaderWriter` BINDS TO THE REAL CLASS
// (B34_RULES §7.2's grep hazard), so this helper sits at global scope in an
// anonymous namespace and spells its POD view `rt::io::` explicitly — as does
// every other POD mention below.
namespace {

inline void* RwSlot(const IO::ReaderWriter* rw, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(rw);
    return *reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vt) + byteOffset);
}

// The scalar shape: `MOV RCX,[RCX+0x08]; MOV RAX,[RCX]; JMP [RAX+SLOT]`.
// `name` is forwarded although every retail body ignores it — the scalar
// bodies clobber R8 before the tail jump. Forwarding it is what the exported
// signature says, and the binary format carries no field names either way.
template <typename T>
inline void ScalarCall(IO::ReaderWriter* rw, int slot, T& v, const char* name) {
    using Fn = void (*)(IO::ReaderWriter*, T*, const char*);
    reinterpret_cast<Fn>(RwSlot(rw, slot))(rw, &v, name);
}

template <typename T>
inline void ArrayCall(IO::ReaderWriter* rw, int slot, int n, T* d, const char* name) {
    using Fn = void (*)(IO::ReaderWriter*, int, T*, const char*);
    reinterpret_cast<Fn>(RwSlot(rw, slot))(rw, n, d, name);
}

}  // namespace

// A member of `IO::X` cannot be defined inside another namespace, so the
// bodies open `namespace IO` and reach the helpers above as `::RwSlot` etc.
namespace IO {

namespace rw_vtbl = rt::io::rw_vtbl;

// ============================================================================
// Lifetime — 0x1800b9960 / 0x1800b99b0
// ============================================================================
//
// THE CONSTRUCTOR, transcribed instruction for instruction (re-read from the
// shipped bytes 2026-09-19):
//
//     MOV [RCX],RDX                 _stream = &stream
//     MOV qword [RCX+0x08],0        _readerWriter = nullptr
//     CMP dword [RDX+0x08],0        stream.direction
//     MOV RCX,[RDX+0x10]            the Encoding policy object
//     MOV RAX,[RCX]                 its vtable
//     MOV RDX,RBX                   arg 2 = this
//     JNZ writing
//     CALL [RAX+0x18] / [RAX+0x20]  arg 3 = R8D = version, NEVER TOUCHED
//     MOV [RBX+0x08],RAX
//     MOV RAX,RBX ; RET             (MSVC ctors return `this`; ours cannot and
//                                    does not need to — no caller reads it,
//                                    the registered door supplies it)
//
// The `_readerWriter = nullptr` store before the factory call is retail's and
// is NOT dead: the factory is handed `this` and can read the field.
//
// ARITY 3 is pinned by R8D's untouched liveness across the whole body, and the
// thing being called is
// `??0Binary{Reader,Writer}@IO@@QEAA@AEAVSerializer@1@H@Z` — `(Serializer&,
// int)`. `BinaryWriter` stores that int and WRITES IT TO THE STREAM, so losing
// the third argument would silently change the save format's version word.
Serializer::Serializer(Stream& stream, int version) {
    _stream       = &stream;
    _readerWriter = nullptr;

    // The FACTORY SLOT IS STILL A RAW SLOT CALL, and stays one for the same
    // reason the 22 Serialize members keep theirs (B34_RULES section 3.1): the
    // Encoding a live stream carries wears one of the three MINTED tables
    // (0x1801554a8 / 0x1801559d0 / 0x1801559f8), whose slots resolve back to
    // registered doors. `CreateReader`/`CreateWriter` are slots 3 and 4 of a
    // five-slot table whose slot 0 is NOT a destructor, which is why the
    // constant is spelled from `encoding_vtbl` rather than counted.
    Encoding* factory = stream._encoding;
    // `MOV RAX,[RCX]` — the vptr AT the object, which on a live Encoding is
    // the minted table, not GCC's.
    void** const fvt = *reinterpret_cast<void** const*>(factory);
    const int slot = (stream._direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRw = ReaderWriter* (*)(Encoding*, Serializer*, int);
    _readerWriter = reinterpret_cast<MakeRw>(
        *reinterpret_cast<void* const*>(
            reinterpret_cast<const uint8_t*>(fvt) + slot))(factory, this, version);
}

// THE DESTRUCTOR, 0x1800b99b0:
//     MOV RCX,[RCX+0x08] ; TEST RCX,RCX ; JZ out
//     MOV RAX,[RCX] ; MOV EDX,1 ; JMP [RAX]
// Flags == 1 is MSVC's scalar deleting destructor: it runs
// `~Binary{Reader,Writer}` — which reads four further bytes on the read side
// and back-patches the length placeholder on the write side — AND frees the
// block. So this is a stream operation with a POSITION, not a cleanup.
//
// The `if` is RETAIL'S OWN `TEST RCX,RCX`, not a guard we added.
// Retail does NOT null `_readerWriter` afterwards, so neither do we: the
// destructor is NOT idempotent and nothing may make it look as though it were.
Serializer::~Serializer() {
    ReaderWriter* rw = _readerWriter;
    if (rw) {
        using Fn = void (*)(ReaderWriter*, int);
        reinterpret_cast<Fn>(::RwSlot(rw, rw_vtbl::kDtor))(rw, 1);
    }
}

// ============================================================================
// Accessors
// ============================================================================
// 0x1800070b0 — the same three-instruction forwarder, tail-jumping +0x08.
int Serializer::GetVersion() const {
    ReaderWriter* rw = _readerWriter;
    using Fn = int (*)(ReaderWriter*);
    return reinterpret_cast<Fn>(::RwSlot(rw, rw_vtbl::kGetVersion))(rw);
}

// 0x180006210 — `MOV RAX,[RCX]; MOV EAX,[RAX+0x8]; RET`. Two loads, no
// dispatch, no side effect, and — like `GetEncoding` — it answers about the
// STREAM, not about this Serializer. Faithfully UNGUARDED: retail dereferences
// `_stream` with no null test here, and so does every one of the ~37 caller
// sites this member replaces (they are transcriptions of the exe's INLINED
// copy of these same two loads).
//
// ENDGAME B.3.4 callers batch H. serializer.cpp's banner listed this under
// "NOT PORTED, because our tree does not call them"; the tree calls it now.
Direction Serializer::GetDirection() {
    return static_cast<Direction>(_stream->_direction);
}

// 0x180006290 — `MOV RAX,[RCX]; MOV RAX,[RAX+0x10]; RET`. Two loads, no
// dispatch, no side effect. It answers with the STREAM's encoding policy
// object, which is a different class from this Serializer's ReaderWriter.
const Encoding& Serializer::GetEncoding() {
    return *_stream->_encoding;
}

// ============================================================================
// The 17 ported `Serialize` overloads + SerializeBytes
// ============================================================================
// Each is retail's three instructions and nothing else. The slot constant on
// each line is the tail-jump displacement decoded from that member's own bytes
// in the shipped DLL, independently confirmed by the contents of
// ??_7BinaryReader@IO@@6B@ (0x1801552e0) and ??_7BinaryWriter@IO@@6B@
// (0x180155210): the entry at that offset is the exported
// `?Serialize@Binary{Reader,Writer}@IO@@` whose mangled parameter type matches.

// ?Serialize@Serializer@IO@@QEAAXAEA_JPEBD@Z  0x180035000  +0x90
// ENDGAME B.3.4b SYSTEM (5). The 18th overload, and the only one added since
// block 3. Its justifying call site is `System::Time::Serialize` (retail
// 0x1800ca740), which hand-inlined this very slot until this member existed —
// see the declaration's comment in runtime/types/io/Serializer.h. Identical in
// shape to every scalar below it; the width is the whole difference, and it is
// the reason the site could not use the `int&` door.
void Serializer::Serialize(int64_t& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarInt64, v, name);
}
void Serializer::Serialize(bool& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarBool, v, name);
}
void Serializer::Serialize(signed char& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarSChar, v, name);
}
void Serializer::Serialize(unsigned char& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarUChar, v, name);
}
void Serializer::Serialize(short& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarShort, v, name);
}
void Serializer::Serialize(unsigned short& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarUShort, v, name);
}
void Serializer::Serialize(int& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarInt, v, name);
}
void Serializer::Serialize(unsigned int& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarUInt, v, name);
}
void Serializer::Serialize(float& v, const char* name) {
    ::ScalarCall(_readerWriter, rw_vtbl::kScalarFloat, v, name);
}

void Serializer::Serialize(int n, char* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayChar, n, d, name);
}
void Serializer::Serialize(int n, signed char* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArraySChar, n, d, name);
}
void Serializer::Serialize(int n, bool* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayBool, n, d, name);
}
void Serializer::Serialize(int n, unsigned char* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayUChar, n, d, name);
}
void Serializer::Serialize(int n, short* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayShort, n, d, name);
}
void Serializer::Serialize(int n, unsigned short* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayUShort, n, d, name);
}
void Serializer::Serialize(int n, int* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayInt, n, d, name);
}
void Serializer::Serialize(int n, unsigned int* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayUInt, n, d, name);
}
void Serializer::Serialize(int n, float* d, const char* name) {
    ::ArrayCall(_readerWriter, rw_vtbl::kArrayFloat, n, d, name);
}

// 0x1800346d0 — the same three instructions, but the payload is `void*`, which
// is why retail gives it its own name instead of a nineteenth overload.
void Serializer::SerializeBytes(int n, void* buf, const char* name) {
    ReaderWriter* rw = _readerWriter;
    using Fn = void (*)(ReaderWriter*, int, void*, const char*);
    reinterpret_cast<Fn>(::RwSlot(rw, rw_vtbl::kSerializeBytes))(rw, n, buf, name);
}

}  // namespace IO
