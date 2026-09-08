#pragma once
// ============================================================================
// runtime/types/io/Stream.h — IO::Stream / MemoryStream / CompressedStream /
//                             CompressedMemoryStream   (Wave 4, lane B)
// ============================================================================
//
// The four layouts the IO stream family needs. Every offset below was read out
// of a decompiled Runtime-x64.dll body and the body is named next to it; where
// the retail build inlines something, Runtime-x64-profile.dll is quoted as the
// independent witness.
//
// ORIGINAL SOURCE FILES (profile-build strings, docs/runtime_port/
// original_source_tree.txt): there is no IO\Stream.cpp and no
// IO\CompressedMemoryStream.cpp. The recovered tree has
//     IO\MemoryStream.cpp        — Stream, MemoryStream
//     IO\CompressedStream.cpp    — CompressedStream, CompressedMemoryStream,
//                                  CompressedFileStream, CompressedCopyFileStream
// so the bodies live in runtime/src/io/compressedstream.cpp (scaffolded as
// stream.cpp, renamed by the orchestrator: the recovered tree has a separate
// IO\Stream.cpp for the IO::Stream base and the old name would have collided
// with it). Recorded here because
// runtime/src/io/README.md otherwise reads as a mirror of the original tree.
//
// LANE BOUNDARY: IO::Encoding and IO::Serializer belong to lane A
// (runtime/types/io/Encoding.h, Serializer.h). This header only ever treats an
// Encoding as an opaque void*, so the two headers cannot disagree.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtIo {

// ============================================================
// IO::Stream — 0x18 bytes, abstract base.
//
//   +0x00  void*   vptr        ??_7Stream@IO@@6B@   = 0x1801549a0 (EXPORTED)
//   +0x08  int32   _direction  0 == reading, 1 == writing
//   +0x10  void*   _encoding   IO::Encoding*, owned
//
// Pinned by ??0Stream@IO@@QEAA@XZ (0x1800b99d0), whose whole body is
//   vptr = &Stream::_vftable_;  *(dword*)(this+8) = 0;  *(qword*)(this+0x10) = 0;
//
// _direction: re-verified here rather than taken on trust from Wave 3 lane B.
// ?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z (0x1800aa4b0) opens with
// *(dword*)(this+8) = 1 and ?Open@CompressedStream (0x1800aa560) with
// *(dword*)(this+8) = 0. Create is the WRITE path, Open the READ path, so
// 0 == reading stands.
//
// _encoding: a heap object the stream owns. Every concrete ctor allocates it
// (Allocate(8,4), then stores the shared Encoding vtable 0x1801554a8 into it)
// and every dtor frees it. So a Stream's encoding pointer is never shared and
// never null in a live object.
//
// VTABLE (0x1801549a0, EXPORTED as ??_7Stream@IO@@6B@) — 7 usable slots, read
// byte-wise from .rdata; slot 7 is a null qword and 0x1801549c0 starts the next
// class's table:
//   [0] +0x00  0x18000c610  deleting destructor thunk
//   [1] +0x08  0x18000c6a0  GetSource
//   [2] +0x10  0x18007ec28  Write         (_purecall)
//   [3] +0x18  0x18007ec28  Read          (_purecall)
//   [4] +0x20  0x18007ec28  SetCurrentPos (_purecall)
//   [5] +0x28  0x18007ec28  GetCurrentPos (_purecall)
//   [6] +0x30  0x18007ec28  GetSize       (_purecall)
// Derived classes add Close at +0x38 and PreCommit at +0x40; see the two tables
// below, both of which have a real body in those slots.
// ============================================================
struct Stream {
    const void* vptr;        // +0x00
    int32_t     direction;   // +0x08   0 = reading, 1 = writing
    uint32_t    pad0c;       // +0x0c   never written by any ctor
    void*       encoding;    // +0x10   IO::Encoding* (lane A owns the type)
};
static_assert(offsetof(Stream, direction) == 0x08, "Stream.direction @ 0x08");
static_assert(offsetof(Stream, encoding)  == 0x10, "Stream.encoding @ 0x10");
static_assert(sizeof(Stream) == 0x18, "sizeof(IO::Stream) == 0x18");

// ============================================================
// IO::MemoryStream — 0x30 bytes, Stream over a flat buffer.
//
//   +0x00..0x17  Stream
//   +0x18  void*    _data     the buffer
//   +0x20  bool     _ownsData free _data in the dtor
//   +0x24  uint32   _cursor   read/write position IN THE BUFFER
//   +0x28  uint32   _size     bytes valid in _data
//
// Pinned by ??0MemoryStream@IO@@QEAA@XZ (0x1800ae710): after the Stream ctor it
// writes qword 0 at +0x18, byte 0 at +0x20 and qword 0 at +0x24 (one store
// covering _cursor and _size), then installs its own vtable and its Encoding.
//
// _ownsData / _data / _encoding are confirmed a second time by
// ??1MemoryStream@IO@@UEAA@XZ (0x1800ae890):
//   vptr = &MemoryStream::_vftable_;
//   if (*(bool*)(this+0x20) && *(void**)(this+0x18)) MemFree(*(void**)(this+0x18));
//   vptr = &Stream::_vftable_;
//   if (*(void**)(this+0x10)) MemFree(*(void**)(this+0x10));
// and _cursor at +0x24 by ?Read@MemoryStream@IO@@UEAA_NHPEAX@Z (0x1800ae950),
// which is where engine/types/io_serializer.h already had it.
//
// SIZE. 0x30 is the stride implied by CompressedMemoryStream: its MemoryStream
// sub-object starts at +0x68 and sizeof(CompressedMemoryStream) is 0x98
// (below), leaving exactly 0x30. Nothing writes past +0x2b.
//
// VTABLE (0x1801555b0, EXPORTED as ??_7MemoryStream@IO@@6B@) — 8 slots:
//   [0] 0x1800242a0 deleting dtor   [4] 0x1800ae9b0 SetCurrentPos
//   [1] 0x18000c6a0 GetSource       [5] 0x1800ae9c0 GetCurrentPos
//   [2] 0x1800ae8f0 Write           [6] 0x1800ae9d0 GetSize
//   [3] 0x1800ae950 Read            [7] 0x1800ae9e0 Close   <-- +0x38
// MemoryStream does NOT add PreCommit; 0x1801555f0 is CompressedFileStream's
// table. The +0x38 == Close identity is what the ported dtor and Close call.
// ============================================================
struct MemoryStream {
    Stream   base;        // +0x00
    void*    data;        // +0x18
    uint8_t  ownsData;    // +0x20
    uint8_t  pad21[3];
    uint32_t cursor;      // +0x24
    uint32_t size;        // +0x28
    uint32_t pad2c;
};
static_assert(offsetof(MemoryStream, data)     == 0x18, "MemoryStream.data @ 0x18");
static_assert(offsetof(MemoryStream, ownsData) == 0x20, "MemoryStream.ownsData @ 0x20");
static_assert(offsetof(MemoryStream, cursor)   == 0x24, "MemoryStream.cursor @ 0x24");
static_assert(offsetof(MemoryStream, size)     == 0x28, "MemoryStream.size @ 0x28");
static_assert(sizeof(MemoryStream) == 0x30, "sizeof(IO::MemoryStream) == 0x30");

// ============================================================
// IO::CompressedStream — 0x68 bytes. A Stream that codes a windowed,
// block-compressed byte stream onto ANOTHER Stream (_source).
//
//   +0x00..0x17  Stream
//   +0x18  uint32   (never written by the ctor — unknown, left as padding)
//   +0x1c  int32    _position       logical position in the DECOMPRESSED stream
//   +0x20  int32    _activeWindow   ctor writes -1; SetWindowCount overwrites
//                                   it with GetAvailableWindow()
//   +0x24  uint32   (never written by the ctor — unknown)
//   +0x28  Stream*  _source         the stream the compressed bytes live on
//   +0x30  Array<Window>   {data +0x30, size +0x38, cap +0x3c}  0x18-byte elems
//   +0x40  Array<Header>   {data +0x40, size +0x48, cap +0x4c}  8-byte elems
//   +0x50  Array<void*>    {data +0x50, size +0x58, cap +0x5c}  8-byte elems
//   +0x60  void*    _compression    System::Compression*, owned
//
// Pinned by ??0CompressedStream@IO@@QEAA@XZ (0x1800aa3d0), disassembled:
//   MOV [RCX+0x1c],EBP(=0) / MOV dword [RCX+0x20],0xffffffff / eight qword
//   zero stores at +0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60.
// +0x18 and +0x24 are the two gaps that leaves; NOTHING in this wave writes or
// reads them and they are recorded as unknown rather than guessed.
//
// _source at +0x28 and _compression at +0x60: ?Create@CompressedStream
// (0x1800aa4b0) is  direction=1; _source=arg; _compression=new Compression;
// SetWindowCount(arg2); return true.
//
// The three Arrays follow the Wave 2 Collection layout ({_data, _size:i32,
// _capacity:i32}, WAVE2_COLLECTION.md) and each is confirmed by an indexed walk
// in ?Close@CompressedStream@IO@@UEAAXXZ (0x1800aa8d0): it strides +0x30 by
// 0x18 bounded by *(int*)(this+0x38), and strides +0x40 and +0x50 by 8 bounded
// by *(int*)(this+0x48).
//
// +0x48 IS THE FIELD THE PORTED DTOR AND Close GUARD ON. *(int*)(this+0x48) is
// the header array's element COUNT, i.e. "has anything been written yet".
//
// VTABLE 0x180155640, EXPORTED as ??_7CompressedStream@IO@@6B@ (ordinal 1410).
// Not used by this lane; CompressedMemoryStream overrides both of the slots
// that would matter.
// ============================================================
struct CompressedStream {
    Stream   base;            // +0x00
    uint32_t unknown18;       // +0x18  never initialised by the ctor
    int32_t  position;        // +0x1c
    int32_t  activeWindow;    // +0x20  ctor: -1
    uint32_t unknown24;       // +0x24  never initialised by the ctor
    Stream*  source;          // +0x28
    void*    windowData;      // +0x30  Array<Window>::_data
    int32_t  windowSize;      // +0x38  Array<Window>::_size
    int32_t  windowCap;       // +0x3c
    void*    headerData;      // +0x40  Array<Header>::_data
    int32_t  headerSize;      // +0x48  Array<Header>::_size   <-- the guard
    int32_t  headerCap;       // +0x4c
    void*    blockData;       // +0x50
    int32_t  blockSize;       // +0x58
    int32_t  blockCap;        // +0x5c
    void*    compression;     // +0x60  System::Compression*, owned
};
static_assert(offsetof(CompressedStream, position)     == 0x1c, "CompressedStream.position @ 0x1c");
static_assert(offsetof(CompressedStream, activeWindow) == 0x20, "CompressedStream.activeWindow @ 0x20");
static_assert(offsetof(CompressedStream, source)       == 0x28, "CompressedStream.source @ 0x28");
static_assert(offsetof(CompressedStream, windowData)   == 0x30, "CompressedStream.windowData @ 0x30");
static_assert(offsetof(CompressedStream, windowSize)   == 0x38, "CompressedStream.windowSize @ 0x38");
static_assert(offsetof(CompressedStream, headerData)   == 0x40, "CompressedStream.headerData @ 0x40");
static_assert(offsetof(CompressedStream, headerSize)   == 0x48, "CompressedStream.headerSize @ 0x48");
static_assert(offsetof(CompressedStream, blockData)    == 0x50, "CompressedStream.blockData @ 0x50");
static_assert(offsetof(CompressedStream, compression)  == 0x60, "CompressedStream.compression @ 0x60");
static_assert(sizeof(CompressedStream) == 0x68, "sizeof(IO::CompressedStream) == 0x68");

// ---- the two record types the +0x30 and +0x40 arrays hold -------------------
// Wave 9 lane PKG2 rung 2. Wave 4 named the ARRAYS; these are their ELEMENTS.
//
// One decompression window. Stride 24 (0x18), pinned four independent ways:
// SetWindow's `LEA RCX,[R10 + R10*0x2]` then `*8` (0x1800a9acb), Read's identical
// pair (0x1800aa7c5), GetAvailableWindow's `ADD RAX,0x18` walk (0x1800a99c2) and
// SetWindowCount's `LEA RSI,[RSI + 0x18]` (0x1800aa0bf).
struct CompressedStreamWindow {
    // +0x00 — the LRU age. Zeroed on the window that just became active and
    // incremented on every OTHER window, so the largest value is the least
    // recently used. GetAvailableWindow evicts the max.
    int32_t useCount;
    // +0x04 — first stream offset this window covers, or -1 when the window
    // holds nothing. The -1 is load-bearing in two places: it is how
    // GetAvailableWindow finds a free window, and it is what SetWindow's fast
    // path tests before trusting the active window.
    int32_t start;
    int32_t end;          // +0x08 — one past the last offset covered
    uint32_t _pad0c;      // +0x0C — never written by anything this lane read
    // +0x10 — the 0x8000-byte decompressed page. Allocated one per window by
    // SetWindowCount (`MOV EDX,0x8000 ; MOV R8D,4` at 0x1800aa07d) and memset to
    // zero immediately after.
    void*    buffer;
};
static_assert(sizeof(CompressedStreamWindow) == 0x18,
              "SetWindow 0x1800a9acb: `LEA RCX,[R10+R10*2]` then scale 8 = stride 24");
static_assert(offsetof(CompressedStreamWindow, start)  == 0x04,
              "SetWindow 0x1800a9acf: `MOV R8D,[RDI + RCX*8 + 0x4]`");
static_assert(offsetof(CompressedStreamWindow, end)    == 0x08,
              "Read 0x1800aa7cd: `MOV EDX,[RAX + RCX*8 + 0x8]`");
static_assert(offsetof(CompressedStreamWindow, buffer) == 0x10,
              "SetWindowCount 0x1800aa0a4: `MOV [RSI + RCX + 0x10],RBX`");

// One compressed block. Stride 8, and the whole on-disk directory is an array of
// these. Read three ways that agree: Open's `LEA EAX,[RDX*0x8]` size check
// (0x1800aa5c4), its `ADD RAX,8` verification walk (0x1800aa61f), and
// DecompressFromSource's `LEA RDI,[RAX + RCX*0x8]` (0x1800a96ec).
//
// THE STREAM FORMAT, which falls out of ?Open@CompressedStream@IO@@ in full:
//
//     [uint32 blockCount]
//     [blockCount x CompressedStreamHeader]
//     [the blocks themselves, in order]
//
// and Open validates it by summing: `4 + blockCount*8 + sum(block sizes)` must
// equal the source's total size exactly (`SETZ AL` at 0x1800aa63b).
struct CompressedStreamHeader {
    uint32_t sourceOffset;      // +0x00 — where the block starts in the source
    // +0x04 — compressed length, and ZERO IS NOT "empty": it is the STORED
    // (uncompressed) marker. Both DecompressFromSource (0x1800a9723 `TEST DX,DX`)
    // and Open's size walk (0x1800aa613) branch on it, and Open's walk falls back
    // to `uncompressedSize` when it is zero — which is what proves the meaning.
    uint16_t compressedSize;
    uint16_t uncompressedSize;  // +0x06 — always the real size, <= 0x8000
};
static_assert(sizeof(CompressedStreamHeader) == 8,
              "Open 0x1800aa5c4: `LEA EAX,[RDX*0x8]` sizes the directory");
static_assert(offsetof(CompressedStreamHeader, compressedSize) == 4,
              "DecompressFromSource 0x1800a9718: `MOVZX EDX,word ptr [RDI + 0x4]`");
static_assert(offsetof(CompressedStreamHeader, uncompressedSize) == 6,
              "DecompressFromSource 0x1800a9703: `MOVZX EAX,word ptr [RDI + 0x6]`");

// The window size, and the reason every `>> 15` and `<< 15` in this family is
// the same constant. Blocks are a fixed 32 KB of DECOMPRESSED data, so the block
// index for a stream offset is `offset >> 15` (SetWindow 0x1800a9b7d,
// `SAR EDX,0xf`) and a block's first offset is `index << 15`.
constexpr int32_t kCompressedStreamWindowSize = 0x8000;

// ============================================================
// IO::CompressedMemoryStream — 0x98 bytes.
//
//   +0x00..0x67  CompressedStream base
//   +0x68..0x97  MemoryStream _memoryStream  (a MEMBER, not a second base)
//
// SIZE IS MEASURED, NOT INFERRED: the class's deleting-destructor thunk
// (0x1800244a0, vtable slot 0) calls
//     eh_vector_destructor_iterator(p, 0x98, count, ~CompressedMemoryStream)
// and 0x98 is the element stride an array-delete uses, i.e. sizeof.
//
// The +0x68 member offset is pinned three times over:
//   ??0CompressedMemoryStream (0x1800a9520)  LEA RCX,[RBX+0x68] before the
//                                            MemoryStream ctor call
//   ?Create@... (0x1800a9600) / ?Open@... (0x1800a9500) pass this+0x68 as the
//                                            Stream& to the CompressedStream base
//   ?GetMemoryStream@... (0x180024350) is literally LEA RAX,[RCX+0x68]; RET
// and independently by the profile build (0x18005c240 / 0x18005c280), which
// uses the same +0x68 with the calls left OUT of line.
//
// So a CompressedMemoryStream is a CompressedStream whose _source is its own
// embedded MemoryStream: compressed bytes are produced into, or consumed from,
// a buffer that lives inside the same object.
//
// engine/types/io_serializer.h already carried the two derived constants
// kCompressedMemoryStream_MemBase = 0x68 and _Cursor = 0x8c. Both agree with
// this header. NOTE the two different "cursors" — +0x8c is the MemoryStream's
// position in the COMPRESSED buffer, +0x1c is the CompressedStream's position in
// the DECOMPRESSED stream. gameupdateloop_serialize.cpp uses +0x1c and
// componententity.cpp uses +0x8c; both are right for what they measure.
//
// VTABLE (0x180155560, EXPORTED as ??_7CompressedMemoryStream@IO@@6B@,
// ordinal 1409) — 9 slots:
//   [0] 0x1800244a0 deleting dtor       [5] 0x1800aa850 GetCurrentPos (base)
//   [1] 0x18000c6a0 GetSource           [6] 0x1800aa860 GetSize       (base)
//   [2] 0x1800aa6c0 Write   (base)      [7] 0x1800a9620 Close      <-- OVERRIDE
//   [3] 0x1800aa780 Read    (base)      [8] 0x1800a9660 PreCommit  <-- OVERRIDE
//   [4] 0x1800aa830 SetCurrentPos(base)
// The only code xref to ?Close@CompressedMemoryStream (0x1800a9620) anywhere in
// the DLL is slot 7 itself, at 0x180155598 = 0x180155560 + 0x38.
// ============================================================
struct CompressedMemoryStream {
    CompressedStream base;          // +0x00
    MemoryStream     memoryStream;  // +0x68
};
static_assert(offsetof(CompressedMemoryStream, memoryStream) == 0x68,
              "CompressedMemoryStream.memoryStream @ 0x68");
static_assert(sizeof(CompressedMemoryStream) == 0x98,
              "sizeof(IO::CompressedMemoryStream) == 0x98 (eh_vector_dtor stride)");

// Stream vtable slot 7 (+0x38) is Close() on every concrete Stream in this
// header. The ported dtor and Close both dispatch the embedded MemoryStream's
// Close through it, exactly as retail does.
constexpr size_t kStreamVtblCloseSlot = 7;

}  // namespace RtIo
