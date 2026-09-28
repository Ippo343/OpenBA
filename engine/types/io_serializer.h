#pragma once
// ============================================================================
// IO::Serializer / IO::Stream / IO::Encoding — binary-compatible layouts
// ============================================================================
//
// Reverse-engineered from Runtime-x64.dll (image base 0x180000000).
// These structs mirror the engine's serialization read/write machinery.
//
// Purpose: debuggability. We are chasing a deserialize byte-misalignment in
// the menu-scene geometry load and need NAMED fields — especially the
// read-position / cursor — so we can step the serializer with source and watch
// the stream position advance.
//
// ----------------------------------------------------------------------------
// CALL CHAIN (decode / loading path)
// ----------------------------------------------------------------------------
//   Serializer::Serialize(int& v, name)             [Runtime 0x1800061d0]
//     -> JMP encoding->vtable[0xA0](encoding, &v, name)
//        = BinaryReader::Serialize(int&)            [Runtime 0x1800a88d0]
//          -> stream->vtable[0x18](stream, 4, &v)   (Stream::Read)
//             = MemoryStream::Read(this, count, dst)[Runtime 0x1800ae950]
//               if (cursor + count <= size) {
//                   memcpy(dst, base + cursor, count);
//                   cursor += count;            <-- THE CURSOR ADVANCE
//               }
//
//   So the byte cursor lives in the concrete Stream subclass. For a plain
//   IO::MemoryStream it is at MemoryStream + 0x24. For the menu's
//   IO::CompressedMemoryStream the MemoryStream sub-object begins at +0x68, so
//   the same cursor is at CompressedMemoryStream + 0x68 + 0x24 = + 0x8C.
//
// Every IO::Serializer::Serialize/SerializeBytes/GetVersion method is a pure
// tail-jump forwarder: it loads RCX = serializer->m_readerWriter (this+0x08), then
// `JMP [encoding->vtable + slot]`, forwarding RDX/R8/... unchanged. The
// Encoding (a BinaryReader on load, BinaryWriter on save) does the real work.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace io {

// ----------------------------------------------------------------------------
// IO::Encoding  (abstract base; concrete = IO::BinaryReader / IO::BinaryWriter)
// ----------------------------------------------------------------------------
// Only the vtable pointer is part of the abstract layout; concrete subclasses
// (BinaryReader is 0x30 bytes) extend it. We interact with it purely through
// its vtable, whose relevant slots (verified against BinaryReader vtable
// 0x1801552e0 in Runtime-x64.dll) are:
//
//   +0x00  scalar-deleting destructor (arg = delete flag)
//   +0x08  GetVersion() -> int
//   +0x48  Serialize(int count, int*    data, name)   [bulk int32  array]
//   +0x50  Serialize(int count, ushort* data, name)   [bulk uint16 array]
//   +0x58  Serialize(int count, short*  data, name)   [bulk int16  array]
//   +0x60  Serialize(int count, uchar*  data, name)   [bulk uint8  array]
//   +0x78  Serialize(bool&  v, name)
//   +0x80  Serialize(float& v, name)
//   +0x98  Serialize(uint&  v, name)
//   +0xA0  Serialize(int&   v, name)
//   +0xB8  Serialize(uchar& v, name)
//   +0xC8  SerializeBytes(int size, void* ptr, name)
//
// (Offsets in bytes, RETAIL order. These are IO::ReaderWriter's slots; the
// class and its by-name members are runtime/types/io/Serializer.h. B3X W0
// deleted the `enc_vtbl` byte-offset constants that stood here: no user.)
struct Encoding {
    void** vtable;   // +0x00
};

// ----------------------------------------------------------------------------
// IO::Stream  (abstract base; concrete = MemoryStream / CompressedMemoryStream /
//              BinaryFileStream / ...)
// ----------------------------------------------------------------------------
// Base ctor (Runtime 0x1800b99d0) writes:
//   this[0x00] = &Stream::vftable
//   this[0x08] = 0   (int)
//   this[0x10] = 0   (Encoding*)
//
//   +0x00  vtable
//   +0x08  m_loadingFlag : int   — 0 = LOADING (decode), non-zero = SAVING
//   +0x10  m_encoding    : Encoding*  — the encoding *factory* whose vtable
//                                       slot +0x18 yields a decode Encoding and
//                                       slot +0x20 yields an encode Encoding.
//
// NOTE: in the Serializer ctor decompile, `stream + 0x10` is described as a
// "factory" — it IS the Encoding* stored here; that object's vtable exposes
// the +0x18 (decode) / +0x20 (encode) factory getters.
//
// Stream::vtable[0x18] = Read(int count, void* dst) -> bool  (the read path).
struct Stream {
    void**    vtable;        // +0x00
    int32_t   m_loadingFlag; // +0x08  0 = load/decode, !=0 = save/encode
    int32_t   _pad0c;        // +0x0C  (alignment)
    Encoding* m_encoding;    // +0x10  encoding factory (see note)
};
static_assert(offsetof(Stream, m_loadingFlag) == 0x08, "Stream.m_loadingFlag @ 0x08");
static_assert(offsetof(Stream, m_encoding)    == 0x10, "Stream.m_encoding @ 0x10");

// ----------------------------------------------------------------------------
// IO::MemoryStream  (concrete Stream over an in-memory buffer)
// ----------------------------------------------------------------------------
// Ctor (Runtime 0x1800ae710) writes:
//   +0x00 vtable = &MemoryStream::vftable (0x1801555b0)
//   +0x08 = 0    (loading flag, from Stream base)
//   +0x10 = 0    (encoding,     from Stream base)  -> later a factory @0x18
//   +0x18 = 0    (m_base   : void*  — buffer base pointer)
//   +0x20 = 0    (m_owns   : byte   — owns/allocated flag)
//   +0x24 = 0    (m_cursor : int    — READ POSITION)        <<< THE CURSOR
//   +0x28 = ...  (m_size   : int    — total byte count, bounds check)
//
// MemoryStream::Read(count, dst)  [Runtime 0x1800ae950]:
//   if (m_cursor + count <= m_size) {
//       memcpy(dst, m_base + m_cursor, count);
//       m_cursor += count;
//       return true;
//   }
//   return false;
struct MemoryStream {
    void**    vtable;        // +0x00
    int32_t   m_loadingFlag; // +0x08
    int32_t   _pad0c;        // +0x0C
    Encoding* m_encoding;    // +0x10
    void*     m_base;        // +0x18  buffer base pointer
    uint8_t   m_owns;        // +0x20  ownership flag
    uint8_t   _pad21[3];     // +0x21
    int32_t   m_cursor;      // +0x24  *** READ POSITION / CURSOR ***
    int32_t   m_size;        // +0x28  total size in bytes (bounds check)
};
static_assert(offsetof(MemoryStream, m_loadingFlag) == 0x08, "MemoryStream.m_loadingFlag @ 0x08");
static_assert(offsetof(MemoryStream, m_encoding)    == 0x10, "MemoryStream.m_encoding @ 0x10");
static_assert(offsetof(MemoryStream, m_base)        == 0x18, "MemoryStream.m_base @ 0x18");
static_assert(offsetof(MemoryStream, m_owns)        == 0x20, "MemoryStream.m_owns @ 0x20");
static_assert(offsetof(MemoryStream, m_cursor)      == 0x24, "MemoryStream.m_cursor @ 0x24 (CURSOR)");
static_assert(offsetof(MemoryStream, m_size)        == 0x28, "MemoryStream.m_size @ 0x28");

// Offset of the MemoryStream sub-object inside an IO::CompressedMemoryStream
// (the menu's ~56MB stream). So its cursor = CompressedMemoryStream + 0x8C.
constexpr size_t kCompressedMemoryStream_MemBase = 0x68;
constexpr size_t kCompressedMemoryStream_Cursor  = kCompressedMemoryStream_MemBase + 0x24; // 0x8C
constexpr size_t kCompressedMemoryStream_Size    = kCompressedMemoryStream_MemBase + 0x28; // 0x90
// NOTE: this is the COMPRESSED-image cursor. The CompressedStream's own
// position in the DECOMPRESSED stream is a different field, at +0x1c.
// Full layout for all four stream classes: runtime/types/io/Stream.h.

// ----------------------------------------------------------------------------
// IO::Serializer  (0x10 bytes)
// ----------------------------------------------------------------------------
// Ctor (Runtime 0x1800b9960), transcribed from the shipped bytes by Wave 4
// lane A and re-verified by the orchestrator against the same disassembly:
//   m_stream       = stream                      // MOV [RCX],RDX
//   m_readerWriter = 0                           // MOV [RCX+8],0
//   factoryVtbl    = *(stream->m_encoding)       // MOV RCX,[RDX+0x10]; MOV RAX,[RCX]
//   arg2 = this                                  // MOV RDX,RBX  @0x1800b997f
//   if (stream->m_loadingFlag == 0)              // LOADING
//       m_readerWriter = factoryVtbl[0x18](factory, this, version)
//   else                                         // SAVING
//       m_readerWriter = factoryVtbl[0x20](factory, this, version)
//
// TWO CORRECTIONS, both previously wrong HERE and in the project's memory
// notes, and both load-bearing:
//
//  1. The `version` ctor arg is NOT IGNORED. This comment said it was. The
//     body never TOUCHES R8D -- which is exactly why it is still live as
//     argument 3 at the CALL. The callee is
//     ??0Binary{Reader,Writer}@IO@@QEAA@AEAVSerializer@1@H@Z, i.e.
//     (Serializer&, int), and BinaryWriter's ctor stores it at +0x28 and
//     WRITES IT TO THE STREAM. It is the save-format version stamped into
//     the file. "Not stored by this function" was read as "discarded".
//
//  2. THE CALL ARITY IS 3, NOT 1 AND 2. This comment showed the decode slot
//     taking (factory) and the encode slot taking (factory, this). Both take
//     (factory, this, version): `MOV RDX,RBX` sets arg 2 before the branch,
//     so it is common to both paths, and R8D carries arg 3 through untouched.
//     The project's memory note recorded this arity as "NOT pinned -- Ghidra's
//     analysis at those sites is demonstrably wrong". It is pinned now, from
//     the instruction bytes. Wave 3's ResetPersistState carries a stated
//     non-byte-faithful deviation that rests on the old uncertainty and can
//     now be revisited.
//
// Dtor (Runtime 0x1800b99b0):
//   if (m_readerWriter) m_readerWriter->vtable[0](m_readerWriter, 1)  // scalar-deleting dtor
//
//   +0x00  m_stream       : Stream*
//   +0x08  m_readerWriter : ReaderWriter*  (BinaryReader on load, BinaryWriter on save)
//
// NAMING CORRECTION: this field was called `m_encoding` and typed `Encoding*`.
// It is an IO::ReaderWriter, a DIFFERENT class. IO::Encoding is the policy
// object at stream+0x10 whose slots +0x18/+0x20 CREATE ReaderWriters; calling
// both "Encoding" hid that distinction. No alias is kept: nothing in the tree
// read this field by name, so the rename breaks nothing. The C++ TYPE is left
// as Encoding* rather than introducing a ReaderWriter struct here -- both are
// one pointer and runtime/types/io/Serializer.h (Wave 4 lane A) is now the
// authoritative layout for this class.
struct Serializer {
    Stream*   m_stream;     // +0x00
    Encoding* m_readerWriter;   // +0x08  -- really ReaderWriter*, see above
};
static_assert(offsetof(Serializer, m_stream)   == 0x00, "Serializer.m_stream @ 0x00");
static_assert(offsetof(Serializer, m_readerWriter) == 0x08, "Serializer.m_readerWriter @ 0x08");
static_assert(sizeof(Serializer) == 0x10, "Serializer is 0x10 bytes");

} // namespace io
