// ============================================================================
// runtime/src/video/imagebuffer_platform_serialize.cpp — Wave 18 lane VRES,
//   post-report tranche 2.
//
//   ?PlatformSerialize@ImageBuffer@Video@@AEAAXAEAVSerializer@IO@@@Z
//   retail 0x1800f3130, ordinal 3387.   void(IO::Serializer&)
// ============================================================================
//
// THE ROW THIS WAVE CREATED FOR ITSELF. It entered the census's `called` column
// only because lane VDATA's ImageBuffer work reaches it, and nothing ported it.
// It is the last row between Wave 18 and Video's ceiling identity.
//
// `AEAA` MEANS PRIVATE, NOT UNEXPORTED. Checked against the shipped export
// directory before writing anything: ordinal 3387, address 0x1800f3130, sitting
// in a run with its four siblings (GraphicsMesh, ShaderProgram, SoundBuffer,
// VertexLayout). So this is a plain NAME door and there is no address-door
// justification to make (`reference_aeaa_means_private_not_unexported.md`).
//
// Confirmed absent before writing: `PlatformSerialize` appears NOWHERE in
// `code/ engine/ runtime/ standalone/ dx11/`. Not ported, not gated, not
// half-registered.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES
// ---------------------------------------------------------------------------
// Its only caller is `?Serialize@ImageBuffer@Video@@` (0x1800f26f0), which has
// already serialized width, height, dimension, kind, format, flags, the three
// address modes, the filter and the refcounted `pixelData` at +0x48 by the time
// it calls this at 0x1800f281a. So this function does exactly two things: it
// carries the PER-MIP-LEVEL pixel blobs, and it re-Creates the platform
// resource from the fields that were just read.
//
//   kind = this->kind                        (+0x2c, read ONCE at entry)
//
//   if (kind == 4 || kind == 5)              // IsGenerated() / IsRendered()
//       // no serialized pixels exist for these
//       if (stream->direction != 0) return;  // SAVING: nothing to do at all
//       Create(..., initialData = nullptr);
//       return;
//
//   if (!encoding->vtable[0]()) return;      // the gate; see below
//   Array<Array<uint8>> levels = {};
//   int count = levels._size;                // == 0
//   rw->Serialize(count)                     // slot 0xA0, kScalarInt
//   if (stream->direction == 0)              // READING
//       Array<Array<uint8>>::SetSize(&levels, count);
//   SerializeLevelBlobs(ser, count, levels._data);
//   Create(..., initialData = &levels);
//   destroy every element's buffer, then free the outer buffer
//
// `Stream::direction` is 0 = READING, 1 = WRITING — already pinned in
// `runtime/types/io/Stream.h`, not re-derived here. That is what makes the two
// `direction` tests readable: the generated/rendered arm does nothing on save,
// and the outer array is sized only on load.
//
// THE ENCODING GATE IS NOT NAMED, AND THIS FILE DOES NOT PRETEND OTHERWISE.
// `stream->encoding->vtable[0]()` returns a bool and the whole pixel path hangs
// off it. No `??_7…Encoding@IO@@6B@` is exported under any name, so this lane
// could not put a name to slot 0 the way `Serializer.h` names slots 3 and 4
// (the reader/writer factories). It is reproduced as "call slot 0, obey the
// bool" — which is what the instruction stream says — and it is called out here
// rather than guessed at, because a plausible-sounding name for an unidentified
// virtual is how a wrong model gets into a header and stays.
//
// ---------------------------------------------------------------------------
// A FAITHFULNESS DETAIL THAT LOOKS LIKE A TIDY-UP AND IS NOT
// ---------------------------------------------------------------------------
// `kind` is read ONCE at entry into R8D (0x1800f3154) and the branch is taken on
// that value. The generated/rendered arm then passes THAT SAME REGISTER to
// Create (0x1800f331d never reloads it), while the pixel arm RELOADS
// `[this+0x2c]` at 0x1800f3217. Both are reproduced separately below. They can
// only differ if something between the two points writes `this->kind` — nothing
// in this body does — but collapsing them to one read would be a silent change
// to which value reaches Create, and this project has paid for exactly that
// class of "equivalent" simplification before.
//
// ---------------------------------------------------------------------------
// THE TWO Collection HELPERS, AND WHY ONE OF THEM COST NOTHING
// ---------------------------------------------------------------------------
// Retail reaches both by in-DLL `rel32`, which our registry cannot intercept,
// so this body has to call something. Both go through
// `ResolveAddr(Module::Runtime, …)`, which is REGISTRY-FIRST and is therefore
// not "the DLL" (`reference_resolveaddr_consults_port_registry_first.md`):
//
//   0x180029630  Array<uint8>::SetSize        **ALREADY PORTED** — lane Q
//                registers it as `J3_ENTRY2(180029630, kSpec_Bool)`
//                (array.cpp:1469, RegisterRuntimeAddr at :1686), so this call
//                reaches OUR body today. Found by grepping the address before
//                writing a line, which is the whole of
//                `reference_grep_for_the_port_before_writing_it.md`.
//
//   0x180053f00  Array<Array<uint8>>::SetSize  UNPORTED. 107 instructions, the
//                non-trivial J3 shape with inner-array element operations.
//                It is COLLECTION lane work, not Video: porting it here would
//                mean adding an `ArraySpec` row to lane Q's `array.cpp`, which
//                another lane owns. It is named here so the Collection wave
//                inherits it as ONE address rather than rediscovering it.
//
// So this row grows the cutover surface by exactly ONE address, and that
// address has an owner (`reference_porting_can_grow_the_cutover_surface.md`).
//
// The per-element serialize loop at 0x180054080 IS written out below rather
// than called by address: it is 20 instructions, it is this function's own
// helper, and nothing else in the tree resolves it. It is deliberately NOT
// registered, for the reason `array_destroy.cpp` gives for the eight helpers it
// also declines to register — a registration with no caller dilutes the
// census's dead-port signal.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Stream.h"
#include "../../types/video/ImageBuffer.h"
#include "../collection/array_internal.h"
#include "../collection/array_destroy_internal.h"

// Locally declared for the reason device.cpp gives: resolve.h drags in
// <windows.h>, and PLAN §4.1 decision 1 keeps runtime/src/ platform-neutral.
// This declaration must stay byte-identical to resolve.h's.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// `kSpec_InnerArrayU8` (0x180053f00) is defined in
// runtime/src/collection/array.cpp but, like `kSpec_EmitterInstance`
// (runtime/src/audio/audio_effectslot_serialize.cpp), was never added to
// array_internal.h's `extern` list — declared locally rather than editing
// that shared header, which this batch does not own.
namespace rtcoll { extern const ArraySpec kSpec_InnerArrayU8; }

// The door over `Video::ImageBuffer::PlatformSerialize`, which is PRIVATE in
// retail (`AEAA`) and `protected` on the class. See the banner on
// `struct ImageBufferDoorAccess;` in runtime/types/video/ImageBuffer.h — the
// class friends it, and `imagebuffer_vtable.cpp` declares the same struct to
// call the same member from `Serialize`. Two translation units, one global type,
// one friend declaration.
struct ImageBufferDoorAccess {
    static void PlatformSerialize(Video::ImageBuffer* p, IO::Serializer& ser) {
        p->Video::ImageBuffer::PlatformSerialize(ser);
    }
};

namespace {

// ---------------------------------------------------------------------------
// ENDGAME B.3.5 block C — FOUR duplicate views are GONE from this file.
// ---------------------------------------------------------------------------
//   * `struct SerializerRaw { void* stream; void* readerWriter; }` and its size
//     assert: the member takes the REAL `IO::Serializer&` (B.3.4 block 3) and
//     reads `_stream` / `_readerWriter` by name.
//   * `kStreamDirection` / `kStreamEncoding`: `IO::Stream::GetDirection()` and
//     `IO::Stream::GetEncoding()` say the same thing from `runtime/types/io/
//     Stream.h`, which owns the evidence.
//   * `kRwScalarInt` (+0xa0) / `kRwArrayUChar` (+0x60) and their two `using`
//     typedefs: `IO::Serializer::Serialize(int&, name)` and
//     `Serialize(int, unsigned char*, name)` are the non-virtual forwarders that
//     reach exactly those two slots. This file was the last caller spelling
//     either offset.
//   * `kImageBufferCreateSlot` (+0x48) and `CreateFn`: slot 9 is
//     `Video::ImageBuffer::Create`, so `self->Create(...)` IS retail's
//     `CALL [R10+0x48]` — a C++ virtual call reads the object's own vptr, which
//     on every live image is still the MINTED table, so the dispatch is
//     unchanged and the slot INDEX is now checked by the compiler instead of by
//     a comment.
// ---------------------------------------------------------------------------

using SetSizeFn = void (*)(rtcoll::ArrayHdr* self, int32_t newSize);

// ENDGAME B.3.2 step 2 — both forwarders above (`OuterSetSize`, `InnerSetSize`)
// are GONE. STALE COMMENT KEPT FOR THE RECORD: "0x180053f00 ... unported
// today" stopped being true once array.cpp's `kSpec_InnerArrayU8` row was
// registered (array.cpp:4028) — it is a registered port, not a DLL call.
//
//   0x180053f00  kSpec_InnerArrayU8 {0x10,8,AssignInnerArrayU8,
//                InitInnerArrayU8,DestroyInnerArrayU8Range,byteCopy=false}
//                NON-TRIVIAL (the assign zeroes-then-conditionally-calls a
//                real deep `Array<uint8>::operator=`), so `levels` (still a
//                plain `rtcoll::ArrayHdr`/`Array<uint8_t>` locally — its
//                nominal element type is a byte cursor, not the semantic
//                `Array<Array<uint8>>`) reaches it through a hand-derived
//                `rtcoll::AsArray<&kSpec_InnerArrayU8>` bridge, not `.SetSize`.
//   0x180029630  kSpec_Bool == `SpecFor<uint8_t>()` field for field (see
//                achievement_manager.cpp's `completed` for the same row),
//                trait-derived. Each per-level element `e` in
//                `SerializeLevelBlobs` IS ALREADY a real
//                `Collection::Array<uint8_t>*` (retail's Array<uint8>
//                instantiation), so `e->SetSize(size)` is that exact body —
//                no bridge needed.

// ---- 0x180054080 — serialize N per-level byte blobs ------------------------
//
// Per element of an Array<Array<uint8>> (stride 0x10): serialize the element's
// own size as an int, size the element on the READ path only, then serialize
// that many raw bytes through the uchar-array slot.
//
// Note the `name` argument is NULL at every one of retail's call sites here
// (`XOR R8D,R8D` / `XOR R9D,R9D`), so the blobs are unnamed in the stream.
void SerializeLevelBlobs(IO::Serializer& ser, int32_t count, rtcoll::ArrayHdr* elems) {
    if (count <= 0) return;                       // TEST EDX,EDX / JLE
    const int64_t n = count;                      // MOVSXD
    for (int64_t i = 0; i < n; ++i) {
        rtcoll::ArrayHdr* e = elems + i;          // ADD RBX,0x10
        int32_t size = e->GetSize();               // MOV EAX,[RBX+8]

        ser.Serialize(size, nullptr);              // rw slot +0xa0, kScalarInt

        // Retail RE-READS ser->stream here rather than caching it — reproduced
        // by reading it through the Serializer again rather than a cached local.
        // retail 0x180029630 — kSpec_Bool == SpecFor<uint8_t>(); trait-derived.
        if (ser._stream->GetDirection() == IO::kReading)
            e->SetSize(size);

        ser.Serialize(size, e->GetData(), nullptr);  // rw slot +0x60, kArrayUChar
    }
}

// The element-destroy spec for the outer array: free the inner buffer at +0x00
// and clear its 16-byte header, per element, stride 0x10. Body A, with lane
// D2's verification attached rather than a fourth copy of that loop.
constexpr rtcoll::DestroySpec kV_LevelBlob =
    { "Array<uint8> @+0x000, stride 0x010", 0x010, 1, { 0x000 } };
static_assert(kV_LevelBlob.offs[0] + 16u <= kV_LevelBlob.stride, "kV_LevelBlob fits");

// The eleven-argument Create call, spelled once so the two arms cannot drift.
// `kind` is a PARAMETER rather than a field read, because the two arms source
// it differently — see the banner.
//
// ENDGAME B.3.5 block C: this is now slot 9 by NAME. `Create` is a virtual of
// the real `Video::ImageBuffer`, declared at index 9, so `self->Create(...)` is
// retail's `CALL [R10+0x48]` with the index checked by the compiler. The virtual
// call reads the object's own vptr, which on every live image is the MINTED
// table, so the dispatch is exactly what it was.
void RecreateFromFields(Video::ImageBuffer* self, int32_t kind,
                        Video::ImageBuffer::LevelArray* initialData) {
    self->Create(
        self->_dimension,  // EDX            +0x28  ImageType
        kind,              // R8D            +0x2c  Usage
        self->_format,     // R9D            +0x30  Format
        self->_flags,      // [RSP+0x20]     +0x34
        self->_width,      // [RSP+0x28]     +0x20
        self->_height,     // [RSP+0x30]     +0x24
        self->_filter,     // [RSP+0x38]     +0x44  FilterMode
        self->_addressU,   // [RSP+0x40]     +0x38
        self->_addressV,   // [RSP+0x48]     +0x3c
        self->_addressW,   // [RSP+0x50]     +0x40
        initialData);      // [RSP+0x58]
}

// Live coverage counter. `calls=` is the only thing that separates "verified
// clean" from "never ran", and lane VDATA's warning applies directly here: its
// save-trace oracle recorded NO ENTRY at all for the seven ImageBuffer vtable
// slots, which is the calls=0 column and not a pass. Static storage duration,
// per array.cpp's 144-dangling-registrations note.

}  // namespace

// ============================================================================
// The body — now a MEMBER of the real `Video::ImageBuffer` (B.3.5 block C).
// ============================================================================
namespace Video {

void ImageBuffer::PlatformSerialize(IO::Serializer& ser) {
    // 0x1800f3154: read ONCE, at entry. The generated/rendered arm reuses this
    // value; the pixel arm reloads. See the banner.
    const int32_t kindAtEntry = _kind;
    IO::Stream* stream = ser._stream;

    // ---- the generated / rendered arm --------------------------------------
    // `LEA EAX,[R8-4] / CMP EAX,1 / JBE` — an UNSIGNED compare, so this is
    // exactly kind == 4 || kind == 5, i.e. IsGenerated() || IsRendered().
    if (static_cast<uint32_t>(kindAtEntry - 4) <= 1u) {
        if (stream->GetDirection() != IO::kReading) {  // 0x1800f32d5: saving
            return;
        }
        RecreateFromFields(this, kindAtEntry, nullptr);
        return;
    }

    // ---- the gate ----------------------------------------------------------
    // ENDGAME B.3.5 block C FINDING — THE GATE HAS A NAME, and this file's
    // banner (written before `IO::Encoding` was a class) says it does not.
    // `stream->encoding->vtable[0]()` returning a bool is slot 0 of
    // `??_7…Encoding@IO@@6B@`, and B.3.4 block 1 named all five of those slots
    // from the three concrete tables: slot 0 is
    // `?IsBinary@Encoding@IO@@UEBA_NXZ` (`BinaryEncoding` 0x1800a81e0 returns
    // true, `TextEncoding` 0x1800b9ab0 and `VisitorEncoding` 0x1800bafd0 return
    // false — runtime/types/io/Serializer.h). So the gate is "carry the
    // per-mip-level pixel blobs only for a BINARY encoding", which is what the
    // behaviour always was: text serialization is stripped from the shipped DLL
    // and the Visitor pass is the resource-reference discovery pass, and neither
    // has any business moving pixels. The banner's caution was right for its
    // instrument and is now superseded by a read, not by a guess.
    // `MOV RCX,[stream+0x10]` then `CALL [[RCX]]`: retail loads the encoding off
    // the stream itself rather than through `?GetEncoding@Serializer@IO@@`, so
    // this reads `_encoding` directly too.
    if (!stream->_encoding->IsBinary()) {
        return;
    }

    // ---- the per-level pixel blobs ----------------------------------------
    // ENDGAME B.3.2: Array is a real class; the default ctor is these same
    // three zero stores, and the explicit teardown below leaves the header
    // zeroed, so the scope-exit ~Array is MemFree(nullptr).
    rtcoll::ArrayHdr levels;

    // 0x1800f3185 reads the count out of the array it has just zeroed, so on
    // the WRITE path the value serialised here is 0. Transcribed, not
    // "corrected" to the level count: this is what the shipped body emits.
    int32_t count = levels.GetSize();

    ser.Serialize(count, nullptr);                   // rw slot +0xa0, kScalarInt

    // 0x1800f31a8 RE-READS ser->stream rather than reusing the pointer it
    // loaded at entry. Reproduced — `ser._stream`, not the `stream` local above.
    // retail 0x180053f00 — kSpec_InnerArrayU8; see the banner.
    if (ser._stream->GetDirection() == IO::kReading)
        rtcoll::AsArray<&rtcoll::kSpec_InnerArrayU8>(&levels)->SetSize(count);

    SerializeLevelBlobs(ser, count,
                        reinterpret_cast<rtcoll::ArrayHdr*>(levels.GetData()));

    // 0x1800f31e2: the field reads for Create happen HERE, after the
    // serialization, and `kind` is RELOADED. Both are faithful.
    RecreateFromFields(this, _kind,
                       reinterpret_cast<LevelArray*>(&levels));   // B.3.5 bridge

    // ---- destroy the temporary --------------------------------------------
    // DESTROYS EVERY ELEMENT and then frees the block, in that order: the
    // element loop frees each level's byte buffer and clears its header, and
    // only then is the outer buffer released. Writing this as "free the block"
    // would leak one buffer per mip level, every load.
    const int32_t levelCount = levels.GetSize();
    rtcoll::FreeInnerArraysCore(reinterpret_cast<uint8_t*>(levels.GetData()),
                                levelCount, &kV_LevelBlob);
    // ENDGAME B.3.2 step 2 — the explicit `rtcoll::ArrayDtorImpl(&levels)` call
    // is GONE, not replaced by an explicit `levels.~Array()`: the per-element
    // inner buffers are already freed by `FreeInnerArraysCore` immediately
    // above, so all that is left to release is the OUTER buffer — exactly what
    // `levels`'s own IMPLICIT destructor does at the closing brace below, using
    // `levels`'s DEFAULT (trivial, `SpecFor<uint8_t>()`) spec, whose
    // `destroy` is null. Calling a spec-aware `~Array()` here (with
    // `kSpec_InnerArrayU8`) would re-run the inner-buffer free a second time —
    // a double free `FreeInnerArraysCore` already discharged.
}

}  // namespace Video

// ============================================================================
// The registered door — a QUALIFIED non-virtual delegation through the friend
// helper, because `PlatformSerialize` is `protected` (retail's `AEAA`, private).
// Name and address are UNCHANGED.
// ============================================================================
extern "C" void Video_ImageBuffer_PlatformSerialize(Video::ImageBuffer* self,
                                                    void* serializer) {
    ImageBufferDoorAccess::PlatformSerialize(
        self, *reinterpret_cast<IO::Serializer*>(serializer));   // B.3.5 bridge
}

// ============================================================================
// Registration — BY NAME AND BY ADDRESS
// ============================================================================
int RtVideoImageBufferSerializeRegister() {
    RegisterRuntimeExport(
        "?PlatformSerialize@ImageBuffer@Video@@AEAAXAEAVSerializer@IO@@@Z",
        (void*)&Video_ImageBuffer_PlatformSerialize);
    RegisterRuntimeAddr(0x1800f3130, (void*)&Video_ImageBuffer_PlatformSerialize);
    printf("[RtPort] Video::ImageBuffer::PlatformSerialize (Wave 18 lane VRES): "
           "1 export registered\n");
    return 1;
}
