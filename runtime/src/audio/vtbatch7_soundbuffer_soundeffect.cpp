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
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../../standalone/resolve.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../object/deleting_dtor_internal.h"   // rtcoll::MemAlloc

using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;
namespace rw_vtbl = rt::io::rw_vtbl;

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
void MemFreeIfSet(void* p) {
    if (!p) return;
    using FreeFn = void (*)(void*);
    RtAddr<FreeFn>(0x1800c7ad0ull)(p);
}

void* BaseVTable()             { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()         { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }

using Slot0Fn = void* (*)(void* self, int flag);
void CallSlot0(void* self, int flag) {
    void** vt = *reinterpret_cast<void***>(self);
    reinterpret_cast<Slot0Fn>(vt[0])(self, flag);
}

}  // namespace

// ============================================================================
// Audio::SoundBuffer
// ============================================================================
namespace {

struct SoundBuffer {
    RtObject::External base;              // +0x00 (0x20)
    uint8_t loopForever;                   // +0x20
    uint8_t streaming;                      // +0x21
    uint8_t pad22[2];
    float   loopDurationSeconds;             // +0x24
    void*   str1;                              // +0x28
    void*   str2;                                // +0x30
    void*   audioData;                             // +0x38
    int32_t audioBytes; uint8_t pad44[4];            // +0x40
    void*   segmentTable;                              // +0x48
    int32_t segmentBytes;                                // +0x50
    uint8_t format[0x80 - 0x54];                           // +0x54
};
static_assert(offsetof(SoundBuffer, loopDurationSeconds) == 0x24, "+0x24");
static_assert(offsetof(SoundBuffer, str1) == 0x28, "+0x28");
static_assert(offsetof(SoundBuffer, str2) == 0x30, "+0x30");
static_assert(offsetof(SoundBuffer, audioData) == 0x38, "+0x38");
static_assert(offsetof(SoundBuffer, segmentTable) == 0x48, "+0x48");
static_assert(offsetof(SoundBuffer, segmentBytes) == 0x50, "+0x50");
static_assert(offsetof(SoundBuffer, format) == 0x54, "+0x54");
static_assert(sizeof(SoundBuffer) == 0x80, "Audio::SoundBuffer must be 0x80 bytes");

void* SoundBufferVTable() { return RT_DATA_SLOT(void*, "??_7SoundBuffer@Audio@@6B@"); }
const uint8_t* SoundBufferTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@SoundBuffer@Audio@@0VBaseTypeInfo@Object@@A");
}

void ConstructBody(SoundBuffer* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = CompiledExternalVTable();
    *vp = SoundBufferVTable();
    self->loopForever = 0; self->streaming = 0;
    self->loopDurationSeconds = 0.0f;
    self->str1 = nullptr; self->str2 = nullptr;
    std::memset(self->format, 0, sizeof self->format);
    self->audioData = nullptr; self->audioBytes = 0;
    self->segmentTable = nullptr; self->segmentBytes = 0;
}

// 0x1800e05c0 — the raw-buffer half of Serialize: two counts, a conditional
// (re)allocate-on-read of the two owned buffers, then three raw byte streams
// (segment table, format window, audio data), in that exact order.
void PlatformSerialize(SoundBuffer* self, Serializer* ser) {
    ReaderWriter* rw = ser->readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &self->audioBytes);
    rw = ser->readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &self->segmentBytes);

    if (IsReading(ser->stream)) {
        MemFreeIfSet(self->audioData);
        MemFreeIfSet(self->segmentTable);
        self->audioData    = (self->audioBytes   > 0) ? rtcoll::MemAlloc(self->audioBytes, 4)   : nullptr;
        self->segmentTable = (self->segmentBytes > 0) ? rtcoll::MemAlloc(self->segmentBytes, 4) : nullptr;
    }

    rw = ser->readerWriter;
    RwBytes(rw, self->segmentBytes, self->segmentTable);
    rw = ser->readerWriter;
    RwBytes(rw, 0x28, self->format);
    rw = ser->readerWriter;
    RwBytes(rw, self->audioBytes, self->audioData);
}

}  // namespace

extern "C" uint32_t RtAudio_SoundBuffer_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, SoundBufferTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtAudio_SoundBuffer_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, SoundBufferTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtAudio_SoundBuffer_GetFileVersion(const void*) { return 0xa; }

extern "C" void* RtAudio_SoundBuffer_Construct(void* selfv) {
    if (!selfv) return nullptr;
    ConstructBody(static_cast<SoundBuffer*>(selfv));
    return selfv;
}
extern "C" void* RtAudio_SoundBuffer_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<SoundBuffer*>(selfv));
    return selfv;
}
extern "C" void RtAudio_SoundBuffer_Destruct(void* selfv) { CallSlot0(selfv, 0); }

extern "C" bool RtAudio_SoundBuffer_Serialize(void* selfv, void* streamv) {
    SoundBuffer* self = static_cast<SoundBuffer*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarBool, &self->loopForever);
    rw = ser.readerWriter; RwScalar(rw, rw_vtbl::kScalarBool, &self->streaming);

    using StrSerFn = void (*)(void* str, Serializer* ser);
    StrSerFn strSer = RtAddr<StrSerFn>(0x1800c9930ull);
    strSer(&self->str1, &ser);
    strSer(&self->str2, &ser);

    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarFloat, &self->loopDurationSeconds);

    PlatformSerialize(self, &ser);

    DestroyReaderWriter(ser.readerWriter);
    return true;
}

// ============================================================================
// Audio::SoundEffect
// ============================================================================
namespace {

// Compile (0x1800e09a0) is the WRITER that settles +0x28's identity: it is a
// plain int64 duration/offset accumulator (`self->offsetTicks = (int64)(0.0 /
// secondsPerTick)`, then rewritten inside the buffer loop), NOT a pointer --
// the array data pointer used by both Compile and Serialize's SetSize call
// lives at +0x30, with the element COUNT cached separately at +0x38.
struct SoundEffect {
    RtObject::External base;   // +0x00 (0x20)
    float    f20;                // +0x20  default 1.0f (ctor stores both +0x20/+0x24
    float    f24;                 //        +0x24  default 0.0f  as ONE qword immediate 0x3f800000)
    int64_t  offsetTicks;          // +0x28
    // The SAMPLE table. Element stride 0x18, pinned by Compile's outer loop
    // (`ADD RDI,0x18` at 0x1800e0a53) and independently by
    // audio_scene_update.cpp:149-158's reader: {float startSeconds @+0x00,
    // variant table @+0x08, variant count @+0x10}. `buffer*` are batch 7's
    // names, kept because Serialize hands SetSize the address of +0x30.
    void*    bufferData;            // +0x30  Array<T>::_data -- Serialize's own
    int32_t  bufferCount;             // +0x38  ::_size  (SetSize is handed &+0x30
    int32_t  bufferCap;                // +0x3c  ::_capacity  directly as the Array<T>*)
};
static_assert(offsetof(SoundEffect, f20) == 0x20, "+0x20");
static_assert(offsetof(SoundEffect, f24) == 0x24, "+0x24");
static_assert(offsetof(SoundEffect, offsetTicks) == 0x28, "+0x28");
static_assert(offsetof(SoundEffect, bufferData) == 0x30, "+0x30");
static_assert(offsetof(SoundEffect, bufferCount) == 0x38, "+0x38");
static_assert(offsetof(SoundEffect, bufferCap) == 0x3c, "+0x3c");
static_assert(sizeof(SoundEffect) == 0x40, "Audio::SoundEffect must be 0x40 bytes");

void* SoundEffectVTable() { return RT_DATA_SLOT(void*, "??_7SoundEffect@Audio@@6B@"); }
const uint8_t* SoundEffectTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@SoundEffect@Audio@@0VBaseTypeInfo@Object@@A");
}

void ConstructBody(SoundEffect* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->base._path.data      = nullptr;
    self->base._refs._data     = nullptr;
    self->base._refs._size     = 0;
    self->base._refs._capacity = 0;
    *vp = SoundEffectVTable();
    self->f20 = 1.0f;
    self->f24 = 0.0f;
    self->offsetTicks = 0;
    self->bufferData = nullptr;
    self->bufferCount = 0;
    self->bufferCap = 0;
}

}  // namespace

extern "C" uint32_t RtAudio_SoundEffect_Type(const void*) {
    uint32_t v = 0; std::memcpy(&v, SoundEffectTypeInfo(), sizeof v); return v;
}
extern "C" const char* RtAudio_SoundEffect_TypeName(const void*) {
    const char* v = nullptr; std::memcpy(&v, SoundEffectTypeInfo() + 8, sizeof v); return v;
}
extern "C" int32_t RtAudio_SoundEffect_GetFileVersion(const void*) { return 3; }

extern "C" void* RtAudio_SoundEffect_Construct(void* selfv) {
    if (!selfv) return nullptr;
    ConstructBody(static_cast<SoundEffect*>(selfv));
    return selfv;
}
extern "C" void* RtAudio_SoundEffect_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    ConstructBody(static_cast<SoundEffect*>(selfv));
    return selfv;
}
extern "C" void RtAudio_SoundEffect_Destruct(void* selfv) { CallSlot0(selfv, 0); }

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
extern "C" int32_t RtAudio_SoundEffect_Compile(void* selfv, const void* /*filename*/) {
    SoundEffect* self = static_cast<SoundEffect*>(selfv);
    double* secondsPerTickSlot = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    const double secondsPerTick = secondsPerTickSlot ? *secondsPerTickSlot : 1.0;
    self->offsetTicks = static_cast<int64_t>(0.0 / secondsPerTick);   // 1800e09bc

    const int32_t sampleCount = self->bufferCount;                     // 1800e09c0
    if (sampleCount <= 0)
        return 1;                                                      // 1800e0a80

    uint8_t* sample = static_cast<uint8_t*>(self->bufferData);         // 1800e09d5
    bool anyLoopForever = false;                                        // R11B

    for (int32_t s = 0; s < sampleCount; ++s, sample += 0x18) {
        const int32_t variantCount = *reinterpret_cast<const int32_t*>(sample + 0x10);
        if (variantCount <= 0) continue;                                // 1800e09e9

        int64_t running = self->offsetTicks;                            // 1800e09eb
        uint8_t* const variants = *reinterpret_cast<uint8_t**>(sample + 0x08);
        const float startSeconds = *reinterpret_cast<const float*>(sample + 0x00);

        for (int32_t k = 0; k < variantCount; ++k) {
            const SoundBuffer* buf = *reinterpret_cast<SoundBuffer* const*>(
                variants + static_cast<int64_t>(k) * 0x18);             // 1800e0a07
            float end = 0.0f;                                            // XMM1
            if (buf) {                                                   // 1800e0a0e
                end = buf->loopDurationSeconds + startSeconds;            // 1800e0a15/1f
                if (buf->loopForever != 0) anyLoopForever = true;         // 1800e0a23
            }
            // Retail's own mixed-unit compare; see note 1 in the banner.
            if (end > static_cast<float>(running)) {                     // 1800e0a2f
                running = static_cast<int64_t>(static_cast<double>(end) / secondsPerTick);
                self->offsetTicks = running;                             // 1800e0a44
            }
        }
    }

    if (anyLoopForever)                                                  // 1800e0a6f
        self->offsetTicks = 0;                                           // 1800e0a71

    return 1;  // External::CompileResult::Success — retail returns 1 on both exits
}

extern "C" bool RtAudio_SoundEffect_Serialize(void* selfv, void* streamv) {
    SoundEffect* self = static_cast<SoundEffect*>(selfv);
    Stream* stream = static_cast<Stream*>(streamv);
    Serializer ser{};
    ReaderWriter* rw = MakeReaderWriter(&ser, stream);

    RwScalar(rw, rw_vtbl::kScalarFloat, &self->f20);
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarFloat, &self->f24);

    int32_t n = self->bufferCount;
    rw = ser.readerWriter;
    RwScalar(rw, rw_vtbl::kScalarInt, &n);
    if (IsReading(stream)) {
        using SetSizeFn = void (*)(void*, int32_t);
        RtAddr<SetSizeFn>(0x180053190ull)(&self->bufferData, n);
    }
    {
        using ElemSerFn = void (*)(Serializer*, int32_t, void*);
        RtAddr<ElemSerFn>(0x180053300ull)(&ser, n, self->bufferData);
    }

    // GetVersion()==0 (writer path skips; reader path on an OLD save consumes
    // and discards one legacy field via a throwaway CreateReader/CreateWriter
    // pair). retail: `if (rw->GetVersion() >= 1) ScalarInt64(self+0x28) else if
    // (writing) {} else { consume-and-release a temp }`.
    using GetVersionFn = int32_t (*)(ReaderWriter*);
    rw = ser.readerWriter;
    const int32_t version = reinterpret_cast<GetVersionFn>(rw->vtable[rw_vtbl::kGetVersion / 8])(rw);
    if (version >= 1) {
        rw = ser.readerWriter;
        RwScalar(rw, rw_vtbl::kScalarInt64, &self->offsetTicks);
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

// ============================================================================
// The `_buffers` element serialize walk   0x180053300   (batch D1)
// ============================================================================
// Serialize above calls this by address. Element {float @+0x00,
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

int RtAudioBatch7SoundBufferSoundEffectRegister() {
    int n = 0;

    // Batch D1: unexported in both builds, address door only.
    RegisterRuntimeAddr  (0x180053300ull, (void*)&RtAudio_SoundEffect_SerializeBuffers); ++n;

    RegisterRuntimeExport("?TypeName@SoundBuffer@Audio@@UEBAPEBDXZ", (void*)&RtAudio_SoundBuffer_TypeName); ++n;
    RegisterRuntimeAddr  (0x18000b8b0ull, (void*)&RtAudio_SoundBuffer_TypeName);
    RegisterRuntimeExport("?Type@SoundBuffer@Audio@@UEBAIXZ", (void*)&RtAudio_SoundBuffer_Type); ++n;
    RegisterRuntimeAddr  (0x18000b890ull, (void*)&RtAudio_SoundBuffer_Type);
    RegisterRuntimeExport("?GetFileVersion@SoundBuffer@Audio@@UEBAHXZ", (void*)&RtAudio_SoundBuffer_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800e0470ull, (void*)&RtAudio_SoundBuffer_GetFileVersion);
    RegisterRuntimeExport("?Serialize@SoundBuffer@Audio@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtAudio_SoundBuffer_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800e0480ull, (void*)&RtAudio_SoundBuffer_Serialize);
    RegisterRuntimeExport("?Reconstruct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ", (void*)&RtAudio_SoundBuffer_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x18000bb30ull, (void*)&RtAudio_SoundBuffer_Reconstruct);
    RegisterRuntimeExport("?Destruct@SoundBuffer@Audio@@UEAAXXZ", (void*)&RtAudio_SoundBuffer_Destruct); ++n;
    RegisterRuntimeAddr  (0x18000bc70ull, (void*)&RtAudio_SoundBuffer_Destruct);
    RegisterRuntimeExport("?Construct@SoundBuffer@Audio@@UEAAPEAVExternal@Object@@XZ", (void*)&RtAudio_SoundBuffer_Construct); ++n;
    RegisterRuntimeAddr  (0x18000bbd0ull, (void*)&RtAudio_SoundBuffer_Construct);

    RegisterRuntimeExport("?TypeName@SoundEffect@Audio@@UEBAPEBDXZ", (void*)&RtAudio_SoundEffect_TypeName); ++n;
    RegisterRuntimeAddr  (0x180050340ull, (void*)&RtAudio_SoundEffect_TypeName);
    RegisterRuntimeExport("?Type@SoundEffect@Audio@@UEBAIXZ", (void*)&RtAudio_SoundEffect_Type); ++n;
    RegisterRuntimeAddr  (0x180050320ull, (void*)&RtAudio_SoundEffect_Type);
    RegisterRuntimeExport("?GetFileVersion@SoundEffect@Audio@@UEBAHXZ", (void*)&RtAudio_SoundEffect_GetFileVersion); ++n;
    RegisterRuntimeAddr  (0x1800e0990ull, (void*)&RtAudio_SoundEffect_GetFileVersion);
    RegisterRuntimeExport("?Compile@SoundEffect@Audio@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z",
                          (void*)&RtAudio_SoundEffect_Compile); ++n;
    RegisterRuntimeAddr  (0x1800e09a0ull, (void*)&RtAudio_SoundEffect_Compile);
    RegisterRuntimeExport("?Serialize@SoundEffect@Audio@@UEAA_NAEAVStream@IO@@@Z", (void*)&RtAudio_SoundEffect_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800e0aa0ull, (void*)&RtAudio_SoundEffect_Serialize);
    RegisterRuntimeExport("?Reconstruct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ", (void*)&RtAudio_SoundEffect_Reconstruct); ++n;
    RegisterRuntimeAddr  (0x180050560ull, (void*)&RtAudio_SoundEffect_Reconstruct);
    RegisterRuntimeExport("?Destruct@SoundEffect@Audio@@UEAAXXZ", (void*)&RtAudio_SoundEffect_Destruct); ++n;
    RegisterRuntimeAddr  (0x180050640ull, (void*)&RtAudio_SoundEffect_Destruct);
    RegisterRuntimeExport("?Construct@SoundEffect@Audio@@UEAAPEAVExternal@Object@@XZ", (void*)&RtAudio_SoundEffect_Construct); ++n;
    RegisterRuntimeAddr  (0x1800505d0ull, (void*)&RtAudio_SoundEffect_Construct);
    return n;
}
