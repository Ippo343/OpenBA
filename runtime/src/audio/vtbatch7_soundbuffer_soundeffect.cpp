// ============================================================================
// runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp
//   VTABLE BATCH 7 — Audio::SoundBuffer and Audio::SoundEffect
// ============================================================================
//
// Both Object::External subclasses. Retail addresses read with objdump against
// the shipped Runtime-x64.dll (not Ghidra's decompile --
// reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// Audio::SoundBuffer — ??_7SoundBuffer@Audio@@6B@, 0x80 bytes
// ---------------------------------------------------------------------------
//   slot0 deleting dtor   0x18000bef0  runtime/src/object/deleting_dtor.cpp
//   slot1 TypeName        0x18000b8b0  here
//   slot2 Type            0x18000b890  here
//   slot3 GetFileVersion  0x1800e0470  here (returns 0xa)
//   slot5 Serialize       0x1800e0480  here
//   slot6 Reconstruct     0x18000bb30  here
//   slot7 Destruct        0x18000bc70  here
//   slot8 Construct       0x18000bbd0  here
//
// Layout (ctor 0x18000ba90, inlined into both Reconstruct and Construct):
// three-level chain External -> CompiledExternal -> SoundBuffer (matches
// Video::VertexLayout's shape in vertexlayout.cpp).
//   +0x00 vtable = ??_7SoundBuffer@Audio@@6B@
//   +0x08/+0x10/+0x18  External fields, zeroed
//   +0x20 byte   _loopForever
//   +0x21 byte   _streaming
//   +0x24 float  _loopDurationSeconds
//   +0x28 String _str1   (WRITER-confirmed by Serialize -- ctor zeroes it as a
//                         plain qword, Serialize is what proves it is a String)
//   +0x30 String _str2
//   +0x38 void*  _audioData
//   +0x40 int32  _audioBytes
//   +0x48 void*  _segmentTable   (ascending byte-offset boundaries, uint32[])
//   +0x50 int32  _segmentBytes   (that table's BYTE length)
//   +0x54 uint8_t[0x2c] _format  (WAVEFORMATEX + trailing bytes; Serialize
//                         streams exactly the first 0x28 of these 0x2c as raw
//                         bytes -- kSbFormat's field breakdown in
//                         audio_play_platform.cpp names +0x54/+0x56/+0x58/
//                         +0x60/+0x62 within this same window)
// Total 0x80, matches the deleting dtor's measured elemSize.
//
// ---------------------------------------------------------------------------
// Audio::SoundEffect — ??_7SoundEffect@Audio@@6B@, 0x40 bytes
// ---------------------------------------------------------------------------
//   slot0 deleting dtor   0x180050820  runtime/src/object/deleting_dtor.cpp
//   slot1 TypeName        0x180050340  here
//   slot2 Type            0x180050320  here
//   slot3 GetFileVersion  0x1800e0990  here (returns 3)
//   slot4 Compile         0x1800e09a0  here
//   slot5 Serialize       0x1800e0aa0  here
//   slot6 Reconstruct     0x180050560  here
//   slot7 Destruct        0x180050640  here
//   slot8 Construct       0x1800505d0  here
//
// Layout (ctor inlined into Reconstruct/Construct, two-level chain, no
// CompiledExternal step -- confirmed: only Base+External+SoundEffect stamps):
//   +0x00 vtable = ??_7SoundEffect@Audio@@6B@
//   +0x08/+0x10/+0x18  External fields, zeroed
//   +0x20 float  _f20        default 1.0f  (ctor stores +0x20/+0x24 together
//   +0x24 float  _f24        default 0.0f   as one qword immediate 0x3f800000)
//   +0x28 int64  _offsetTicks
//   +0x30 Array<T> _buffers  {data, size@+0x38, cap@+0x3c} -- Serialize hands
//                            SetSize the address of +0x30 directly as the
//                            Array<T>*, so size/cap are NOT a separate cache
// Total 0x40.
//
// `Compile` (0x1800e09a0) IS fully reimplemented as of VTABLE BATCH 7b -- this
// banner used to say "NOT fully reimplemented ... only the outer shape is
// faithful", which was true of batch 7 and is no longer true of this file. The
// whole body, including the two-level sample/variant walk, is transcribed and
// its evidence is set out above the function.
// `Serialize`'s two nested-array helpers (0x180053190 SetSize,
// 0x180053300 element loop) were NOT ported by batch 7; they are reached
// through ResolveAddr (port registry first, DLL fallback), same reasoning as
// the AnimationGroup SetSize calls in vtbatch7_animgroup_particlesystem.cpp.
//
// UPDATE (cutover closure batch D1, 2026-09-12): BOTH ARE NOW PORTED —
// 0x180053190 as `kSpec_SfxBuffer18` in runtime/src/collection/array.cpp, and
// 0x180053300 at the bottom of THIS file. The ResolveAddr calls are unchanged
// because the registry is consulted first; they now land on our bodies. The
// element layouts the two ports settled are recorded with them:
//   _buffers element  {float @+0x00, Array<Sample> @+0x08}      stride 0x18
//   Sample   element  {ExternalPtr @+0x00, 3x float @+0x08..}   stride 0x18
//
// ENDGAME B.3 step 7, block B4: both classes are REAL C++ classes now
// (runtime/types/audio/SoundBuffer.h, SoundEffect.h). The two anonymous-
// namespace structs that carried the layouts here MOVED into those headers
// under the retail names and the getter-attested field names (B6-8): the
// SoundBuffer's `str1`/`str2` are `_string28`/`_streamPath`, `audioData`/
// `audioBytes` are `_data`/`_length`, `segmentTable`/`segmentBytes` are
// `_wmaData`/`_wmaLength`; the SoundEffect's `f20`/`f24` are `_radius`/
// `_innerRadius`, `offsetTicks` is `_maxLength` and the `buffer*` triple is
// `_samples` (Array<Sample>). The slot bodies are the members below; every
// registered door is a one-line QUALIFIED delegation, because a live object
// wears the MINTED table whose slots resolve back to these doors.
//
// The by-address calls the move converted (RULE B7-11, hex literals checked
// against standalone/rt_export_addrs.inc):
//   0x1800c9930 `?Serialize@String@System@@` -> `System::String::Serialize`
//   0x1800c7ad0 `?MemFree@MemoryAllocator@System@@` -> `rtcoll::MemFree`, the
//               one allocator door PlatformSerialize's `rtcoll::MemAlloc`
//               already uses (Collection.h's allocator banner: a heap must not
//               be half-shimmed; same allocator on a normal run)
//   0x180053300 (unexported, this file's own body) -> a direct call
// 0x180053190 / 0x180052e60 (the two SoundEffect Array SetSize bodies,
// Collection code in array.cpp) and 0x180052a50 (the ExternalPtr<SoundBuffer>
// serialize helper) stay by-address calls: unexported, not this block's, and
// resolved per call with no cache.
//
// ENDGAME B.3.9 ENG (B39_RULES D1/D2): both classes are CONCRETE. Their real
// constructors (the headers) hold the factory rows' stores, their DeletingDtor
// the retired RT_DELDTOR rows, and every live object wears the COMPILER's
// table. The sixteen slot doors, `InlineCtorStores` and `CallSlot0` went with
// the minted `??_7SoundBuffer@Audio@@6B@` / `??_7SoundEffect@Audio@@6B@` rows
// and the FACTREST pair (rt_factories_audio.{cpp,inc}); the factory pairs are
// the classes' own statics below. ~SoundEffect's body moved here from the
// retired deleting_dtor_leaves.cpp door; ~SoundBuffer's is in
// batchc_audio_leaves.cpp.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>          // placement new of the two SoundBuffer Strings

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/collection/Collection.h"   // rtcoll::MemAlloc / MemFree
#include "../../types/system/String.h"
#include "../../types/audio/SoundBuffer.h"
#include "../../types/audio/SoundEffect.h"
#include "../../types/system/Timer.h"   // ENDGAME B.3.9 SS1: Timer::_secondsPerTick

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
namespace rw_vtbl = rt::io::rw_vtbl;

extern "C" void RtAudio_SoundEffect_SerializeBuffers(void* serv, int32_t count, void* basev);

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using ScalarFn = void (*)(ReaderWriter*, void*, const char*);
using BytesFn  = void (*)(ReaderWriter*, int32_t, void*, const char*);

void RwScalar(ReaderWriter* rw, int slot, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[slot / 8])(rw, p, nullptr);
}
void RwBytes(ReaderWriter* rw, int32_t count, void* data) {
    reinterpret_cast<BytesFn>(rw->vtable[rw_vtbl::kSerializeBytes / 8])(rw, count, data, nullptr);
}
bool IsReading(const Stream* s) { return s->direction == rt::io::kDirectionReading; }

ReaderWriter* MakeReaderWriter(Serializer* ser, Stream* stream) {
    ser->stream = stream;
    ser->readerWriter = nullptr;
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = IsReading(stream) ? rt::io::encoding_vtbl::kCreateReader
                                       : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, ser, 0);
    ser->readerWriter = rw;
    return rw;
}
void DestroyReaderWriter(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[0])(rw, 1);
}
// `?MemFree@MemoryAllocator@System@@` — retail tests for null OUTSIDE the call.
void MemFreeIfSet(void* p) {
    if (!p) return;
    rtcoll::MemFree(p);
}

const uint8_t* SoundBufferTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@SoundBuffer@Audio@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* SoundEffectTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@SoundEffect@Audio@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

// ############################################################################
// Audio::SoundBuffer's member bodies
// ############################################################################
namespace Audio {

// 0x18000b8a0: `MOV EAX,[_typeInfo] ; RET` -- the static accessor.
uint32_t SoundBuffer::TypeId() {
    uint32_t v = 0; std::memcpy(&v, SoundBufferTypeInfo(), sizeof v); return v;
}
// [2] 0x18000b890: the same load.
uint32_t SoundBuffer::Type() const { return TypeId(); }
// [1] 0x18000b8b0: the same node at +8.
const char* SoundBuffer::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, SoundBufferTypeInfo() + 8, sizeof v); return v;
}
// [3] 0x1800e0470: `MOV EAX,0xa ; RET`.
int32_t SoundBuffer::GetFileVersion() const { return 0xa; }

// [8] 0x18000bbd0 -- the inlined constructor, i.e. placement construction.
// (Retail's null test sat in the retired door; a virtual call always has a
// `this`.) `::new`: the class declares its own placement new.
Object::External* SoundBuffer::Construct() {
    return ::new (this) SoundBuffer();
}
// [6] 0x18000bb30: slot0(this, 0) dispatched VIRTUALLY, then the same stores.
Object::External* SoundBuffer::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) SoundBuffer();
}
// [7] 0x18000bc70: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void SoundBuffer::Destruct() { DeletingDtor(0); }

// ?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ  0x18000b9a0
// The FACTREST row this replaces: MemAlloc of 0x80 bytes, align 8 (the locked
// `?MemAlloc@MemoryAllocator@System@@` Object::detail::InternalMemAlloc
// reaches); retail's null branch; then the inlined constructor.
Object::Base* SoundBuffer::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SoundBuffer), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) SoundBuffer();
}
// ?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z  0x18000ba90
Object::Base* SoundBuffer::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) SoundBuffer();
}

// 0x1800e05c0 — the raw-buffer half of Serialize: two counts, a conditional
// (re)allocate-on-read of the two owned buffers, then three raw byte streams
// (seek table, format window, audio data), in that exact order.
void SoundBuffer::PlatformSerialize(void* serializer) {
    Serializer* ser = static_cast<Serializer*>(serializer);
    ReaderWriter* rw = ser->readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &_length);
    rw = ser->readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &_wmaLength);

    if (IsReading(ser->stream)) {
        MemFreeIfSet(_data);
        MemFreeIfSet(_wmaData);
        _data    = (_length    > 0) ? rtcoll::MemAlloc(_length, 4) : nullptr;
        _wmaData = (_wmaLength > 0) ? static_cast<uint32_t*>(rtcoll::MemAlloc(_wmaLength, 4)) : nullptr;
    }

    rw = ser->readerWriter;
    RwBytes(rw, _wmaLength, _wmaData);
    rw = ser->readerWriter;
    RwBytes(rw, 0x28, _format);
    rw = ser->readerWriter;
    RwBytes(rw, _length, _data);
}

// [5] 0x1800e0480.
bool SoundBuffer::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarBool, &_loopForever);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarBool, &_streamed);

    // `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z` (0x1800c9930).
    _string28.Serialize(&ser);
    _streamPath.Serialize(&ser);

    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarFloat, &_durationSeconds);

    PlatformSerialize(&ser);

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

// (~SoundBuffer, 0x1800e03b0, is defined in batchc_audio_leaves.cpp next to
// the Device callback it opens with.)

// ############################################################################
// Audio::SoundEffect's member bodies
// ############################################################################

// 0x180050330: `MOV EAX,[_typeInfo] ; RET` -- the static accessor.
uint32_t SoundEffect::TypeId() {
    uint32_t v = 0; std::memcpy(&v, SoundEffectTypeInfo(), sizeof v); return v;
}
// [2] 0x180050320 / [1] 0x180050340.
uint32_t SoundEffect::Type() const { return TypeId(); }
const char* SoundEffect::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, SoundEffectTypeInfo() + 8, sizeof v); return v;
}
// [3] 0x1800e0990: `MOV EAX,3 ; RET`.
int32_t SoundEffect::GetFileVersion() const { return 3; }

// [8] 0x1800505d0 -- the inlined constructor (placement construction).
Object::External* SoundEffect::Construct() {
    return ::new (this) SoundEffect();
}
// [6] 0x180050560: slot0(this, 0) VIRTUALLY, then the same stores.
Object::External* SoundEffect::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) SoundEffect();
}
// [7] 0x180050640.
void SoundEffect::Destruct() { DeletingDtor(0); }

// ?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ  0x180050430
// MemAlloc of 0x40 bytes, align 8; retail's null branch; the inlined ctor.
Object::Base* SoundEffect::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SoundEffect), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) SoundEffect();
}
// ?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z  0x1800504f0
Object::Base* SoundEffect::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) SoundEffect();
}

// ============================================================================
// 0x1800e09a0 — `?Compile@SoundEffect@Audio@@`, WHOLE, byte for byte.
//
// *** BATCH 7 LEFT THIS HALF-PORTED AND REGISTERED, WHICH IS WORSE THAN A TRAP.
// *** Its banner said "the per-buffer inner loop ... was read from the
// disassembly but not fully re-derived to certainty (the inner table's element
// shape is inferred)". Batch 7b re-read the whole body (Ghidra
// disassemble_function on retail 0x1800e09a0..0x1800e0a8b) and the inner
// table's shape is NOT inferred — it has an independent second witness in this
// tree, written months earlier by a READER:
// runtime/src/audio/audio_scene_update.cpp:145-158 measured the same two levels
// off `?PlayEffect@Scene@Audio@@` (0x1800df7e0 / 0x1800dfc42 / 0x1800df9fe):
//
//     SoundEffect  +0x28 int64 duration, +0x30 sample table, +0x38 sample count
//     sample entry stride 0x18:  +0x00 float start SECONDS, +0x08 variant
//                                table, +0x10 variant count
//     variant entry stride 0x18: +0x00 ExternalPtr<SoundBuffer>, then floats
//
// This body (a WRITER) reaches exactly those offsets with exactly those
// strides, and it dereferences the variant's +0x00 as a SoundBuffer, reading
// +0x20 and +0x24 — which this same file's SoundBuffer struct above already
// pins as `loopForever` and `loopDurationSeconds` from a third direction (that
// class's own ctor and Serialize). Three independent witnesses; nothing here is
// inferred from a shape.
//
// THE BODY, instruction for instruction:
//
//   1800e09a6  XORPS XMM0,XMM0 / DIVSD XMM0,[_secondsPerTick] / CVTTSD2SI
//   1800e09bc  MOV [RCX+0x28],RAX            self->_durationTicks = (i64)(0.0/spt)
//   1800e09c0  MOV EAX,[RCX+0x38] ; TEST ; JLE 1800e0a80 -> return 1
//   1800e09d5  RDI = self->_samples ; EBP = count ; ESI = 1 ; R11B = 0
//   1800e09e0  R9 = RDI+0x10 (&sample->variantCount)
//   -- outer, back edge 1800e0a5e -> 1800e09e4, RDI/R9 both += 0x18 --
//     1800e09e6  if (*R9 <= 0) skip the inner loop entirely
//     1800e09eb  R10 = self->_durationTicks              (RELOADED per sample)
//     -- inner, RDX = k*0x18 --
//       1800e0a00  RAX = *(R9-8) = sample->_variants
//       1800e0a04  XMM1 = 0.0f
//       1800e0a07  R8 = *(SoundBuffer**)(RAX + RDX)
//       1800e0a0e  if (R8 == 0) leave XMM1 at 0.0f
//       1800e0a10  CMP [R8+0x20],0 / MOVSS XMM1,[R8+0x24] / ADDSS XMM1,[RDI]
//       1800e0a23  CMOVNZ R11D,ESI            anyLoopForever |= buf->loopForever
//       1800e0a2a  CVTSI2SS XMM0,R10 ; COMISS XMM1,XMM0 ; JBE skip
//       1800e0a34  R10 = (i64)((double)XMM1 / spt) ; MOV [RBX+0x28],R10
//   1800e0a6c  if (R11B) MOV qword [RBX+0x28],0
//   1800e0a6a/80 EAX = 1 on both exits
//
// TWO THINGS THAT LOOK LIKE TRANSCRIPTION ERRORS AND ARE NOT:
//
//  1. `COMISS XMM1, (float)R10` compares a value in SECONDS against a value in
//     TICKS. R10 starts at 0, so the first candidate always wins; after that
//     R10 holds a tick count (typically 10^4..10^7) and no later per-buffer
//     SECONDS value can exceed it, so in practice only the first non-zero
//     candidate is ever taken. That is retail's arithmetic, reproduced as
//     written. Fixing it would be a behaviour change, not a correction.
//  2. The running maximum is cached in R10 but RELOADED from self->+0x28 at the
//     top of every OUTER iteration (0x1800e09eb is inside the outer loop, past
//     the `JLE`), and only when that sample has variants. A sample with zero
//     variants therefore leaves the cache untouched — which is the same value,
//     so the reload is observationally a no-op and is written that way below.
//
// `anyLoopForever` clears the duration to 0 — the "this effect never ends"
// sentinel. It is set even for a buffer whose duration does NOT win the
// comparison, because the CMOVNZ is before the COMISS.
//
// Self-contained: no calls at all, and one data read (_secondsPerTick).
// ============================================================================
// (B.3.7: `self->_durationTicks` / `self->_samples` above are the members
// `_maxLength` / `_samples`; the SoundBuffer's +0x20/+0x24 are `_loopForever`
// and `_durationSeconds`.)
Object::External::CompileResult SoundEffect::Compile(const System::Filename&) {
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    const double secondsPerTick = ::System::Timer::_secondsPerTick;
    _maxLength = static_cast<int64_t>(0.0 / secondsPerTick);          // 1800e09bc

    const int32_t sampleCount = _samples._size;                        // 1800e09c0
    if (sampleCount <= 0)
        return 1;                                                      // 1800e0a80

    const Sample* sample = _samples._data;                             // 1800e09d5
    bool anyLoopForever = false;                                        // R11B

    for (int32_t s = 0; s < sampleCount; ++s, ++sample) {
        const int32_t variantCount = sample->variants._size;
        if (variantCount <= 0) continue;                                // 1800e09e9

        int64_t running = _maxLength;                                   // 1800e09eb
        const Variant* const variants = sample->variants._data;
        const float startSeconds = sample->startSeconds;

        for (int32_t k = 0; k < variantCount; ++k) {
            const SoundBuffer* buf = variants[k].buffer.Get();          // 1800e0a07
            float end = 0.0f;                                            // XMM1
            if (buf) {                                                   // 1800e0a0e
                end = buf->_durationSeconds + startSeconds;              // 1800e0a15/1f
                if (buf->_loopForever != 0) anyLoopForever = true;       // 1800e0a23
            }
            // Retail's own mixed-unit compare; see note 1 in the banner.
            if (end > static_cast<float>(running)) {                     // 1800e0a2f
                running = static_cast<int64_t>(static_cast<double>(end) / secondsPerTick);
                _maxLength = running;                                    // 1800e0a44
            }
        }
    }

    if (anyLoopForever)                                                  // 1800e0a6f
        _maxLength = 0;                                                  // 1800e0a71

    return 1;  // External::CompileResult::Success — retail returns 1 on both exits
}

// [5] 0x1800e0aa0.
bool SoundEffect::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarFloat, &_radius);
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarFloat, &_innerRadius);

    int32_t n = _samples._size;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        // 0x180053190 — kSpec_SfxBuffer18 (array.cpp), unexported Collection code.
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180053190ull)(&_samples, n);
    }
    // 0x180053300 — this file's own element walk, called directly.
    RtAudio_SoundEffect_SerializeBuffers(&ser, n, _samples._data);

    // GetVersion()==0 (writer path skips; reader path on an OLD save consumes
    // and discards one legacy field via a throwaway CreateReader/CreateWriter
    // pair). retail: `if (rw->GetVersion() >= 1) ScalarInt64(self+0x28) else if
    // (writing) {} else { consume-and-release a temp }`.
    using GetVersionFn = int32_t (*)(ReaderWriter*);
    rw = ser.readerWriter;
    const int32_t version = reinterpret_cast<GetVersionFn>(rw->vtable[rw_vtbl::kGetVersion / 8])(rw);
    if (version >= 1) {
        rw = ser.readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt64, &_maxLength);
    } else if (IsReading(stream)) {
        // retail (0x1800e0b88..0x1800e0bc3) calls vtable[0x20](this->encoding,
        // &ser, 0) to create ANOTHER throwaway ReaderWriter here, checks it for
        // non-null, and immediately releases it via vtable[0] -- the value is
        // never read either way. A pure allocate-then-free with no observable
        // effect; skipped rather than reproduced.
    }

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

// ??1SoundEffect@Audio@@UEAA@XZ  0x1800e0880 -- retail's body, moved here from
// the retired deleting_dtor_leaves.cpp door (ENDGAME B.3.9 ENG): restamp (the
// compiler's), then 0x1800516c0 over `_samples` -- the element destroy of every
// Sample's variant table followed by the plain ~Array, which frees and ZEROES
// the header, so the compiler-emitted `_samples.~Array()` afterwards is a
// no-op -- then ~External (the compiler's chain, retail's tail call).
SoundEffect::~SoundEffect() {
    using ArrayDtorFn = void (*)(void*);
    RtAddr<ArrayDtorFn>(0x1800516c0ull)(&_samples);
}

}  // namespace Audio

// ============================================================================
// The `_buffers` element serialize walk   0x180053300   (batch D1)
// ============================================================================
// SoundEffect::Serialize above calls this directly (B.3.7). Element {float @+0x00,
// Collection::Array<Sample> @+0x08}, stride 0x18:
//
//   18005333e  kScalarFloat  e+0x00
//   180053344  n = e->samples._size   (e+0x10)
//   18005335a  kScalarInt(&n)                     — a LOCAL, not the field
//   180053360  if (reading) 0x180052e60(&e->samples, n)    SetSize
//   180053379  p = e->samples._data   (e+0x08)
//   180053390  per sample (stride 0x18):
//                 0x180052a50(p+0x00, ser)        ExternalPtr<SoundBuffer>
//                 kScalarFloat p+0x08 / p+0x0c / p+0x10
//
// `ser->readerWriter` is re-read before every virtual call, as retail does.
// 0x180052a50 is the ExternalPtr serialize helper of the sibling batch and is
// reached BY ADDRESS — registry-first, so it needs no edit here when it lands.
extern "C" void RtAudio_SoundEffect_SerializeBuffers(void* serv, int32_t count,
                                                    void* basev) {
    if (count <= 0) return;                                   // 180053300 TEST/JLE
    Serializer* ser = static_cast<Serializer*>(serv);
    uint8_t*    e   = static_cast<uint8_t*>(basev);

    using ExtPtrSerFn = void (*)(void* field, Serializer* ser);
    ExtPtrSerFn extSer = RtAddr<ExtPtrSerFn>(0x180052a50ull);
    using SetSizeFn = void (*)(void*, int32_t);

    for (int32_t i = 0; i < count; ++i, e += 0x18) {
        RwScalar(ser->readerWriter, rw_vtbl::kScalarFloat, e + 0x00);

        int32_t n = *reinterpret_cast<int32_t*>(e + 0x10);     // 180053344
        RwScalar(ser->readerWriter, rw_vtbl::kScalarInt, &n);  // 18005335a

        if (IsReading(ser->stream))                            // 180053360
            RtAddr<SetSizeFn>(0x180052e60ull)(e + 0x08, n);    // 180053370

        uint8_t* p = *reinterpret_cast<uint8_t**>(e + 0x08);   // 180053379
        for (int32_t k = 0; k < n; ++k, p += 0x18) {           // 180053390
            extSer(p + 0x00, ser);
            RwScalar(ser->readerWriter, rw_vtbl::kScalarFloat, p + 0x08);
            RwScalar(ser->readerWriter, rw_vtbl::kScalarFloat, p + 0x0c);
            RwScalar(ser->readerWriter, rw_vtbl::kScalarFloat, p + 0x10);
        }
    }
}

// ============================================================================
// BATCH C (2026-09-12): ?PlatformSerialize@SoundBuffer@Audio@@ (retail
// 0x1800e05c0, ordinal 3389) BECOMES A REGISTERED PORT.
//
// The BODY already existed — `PlatformSerialize` above, written by this batch's
// predecessor as a file-local helper for `SoundBuffer::Serialize`. It was never
// registered, so every OTHER caller (and the census) still reached the DLL's
// copy. Registration is the whole of the port; a second body would be exactly
// the duplication this file's family exists to prevent, so there is one body
// and this is a thin extern "C" door onto it.
//
// `AEAA` = PRIVATE, which this DLL still exports
// (reference_aeaa_is_private_not_unexported.md), so the NAME door is real.
// ============================================================================

// ============================================================================
// The PlatformSerialize name door (an exported non-slot member, AEAA =
// private). ENDGAME B.3.9 ENG: the sixteen slot doors are gone -- their only
// reader was the two minted tables.
// ============================================================================
extern "C" void RtAudio_SoundBuffer_PlatformSerialize(void* selfv, void* serv) {
    reinterpret_cast<Audio::SoundBuffer*>(selfv)->Audio::SoundBuffer::PlatformSerialize(serv);        // B.3.7 bridge
}

int RtAudioBatch7SoundBufferSoundEffectRegister() {
    int n = 0;

    // Batch D1: unexported in both builds, address door only.
    RegisterRuntimeAddr  (0x180053300ull, (void*)&RtAudio_SoundEffect_SerializeBuffers); ++n;

    RegisterRuntimeExport("?PlatformSerialize@SoundBuffer@Audio@@AEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtAudio_SoundBuffer_PlatformSerialize); ++n;
    RegisterRuntimeAddr  (0x1800e05c0ull, (void*)&RtAudio_SoundBuffer_PlatformSerialize);

    // ---- the factory pairs (B39_RULES D2): the classes' OWN statics, both
    // doors, replacing the FACTREST rows. The minted `?_typeInfo@` records'
    // factoryFn / inPlaceFactoryFn resolve these by name (MintTypeInfos runs
    // after every *PortInit).
    RegisterRuntimeExport("?CreateNewObject@SoundBuffer@Audio@@SAPEAVBase@Object@@XZ",
                          (void*)&Audio::SoundBuffer::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x18000b9a0ull, (void*)&Audio::SoundBuffer::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@SoundBuffer@Audio@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&Audio::SoundBuffer::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x18000ba90ull, (void*)&Audio::SoundBuffer::CreateInPlaceObject);
    RegisterRuntimeExport("?CreateNewObject@SoundEffect@Audio@@SAPEAVBase@Object@@XZ",
                          (void*)&Audio::SoundEffect::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x180050430ull, (void*)&Audio::SoundEffect::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@SoundEffect@Audio@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&Audio::SoundEffect::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x1800504f0ull, (void*)&Audio::SoundEffect::CreateInPlaceObject);
    return n;
}
