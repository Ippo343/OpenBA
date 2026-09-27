// ============================================================================
// tests/unit/b39_io_classes_test.cpp -- ENDGAME B.3.9 block IO (B39_RULES D1,
// rule B9-16): the IO classes whose minted tables were retired, built through
// their REAL constructors.
// ============================================================================
//
// Each class gets: not abstract, the dtor shape (non-virtual), slot order via
// pointer-to-virtual-member (adjustment 0), the ctor's retail stores, and --
// where the class has a slot-0 deleting dtor -- both arms with a recording
// marker. The out-of-line member bodies live in runtime/src/io/, which stands
// on the port registry and does not link here; the few this test needs to
// reach are stubbed below and say so.
// ============================================================================

#include "unit.h"

#include "runtime/types/io/Serializer.h"
#include "runtime/types/io/FileStream.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting
    return words[0] - 1;
}

const void* const* VTableOf(const void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

}  // namespace

// ---------------------------------------------------------------------------
// Host stand-ins for the encodings' factory bodies (runtime/src/io/encoding.cpp
// and encoding_visitor_text.cpp). CreateReader is each class's key function, so
// defining the pair here is what emits the three vtables into the test binary.
// The tests never call them: a factory allocates through the MemoryAllocator.
// ---------------------------------------------------------------------------
namespace IO {
ReaderWriter* BinaryEncoding::CreateReader(Serializer&, int)  { return nullptr; }
ReaderWriter* BinaryEncoding::CreateWriter(Serializer&, int)  { return nullptr; }
ReaderWriter* TextEncoding::CreateReader(Serializer&, int)    { return nullptr; }
ReaderWriter* TextEncoding::CreateWriter(Serializer&, int)    { return nullptr; }
ReaderWriter* VisitorEncoding::CreateReader(Serializer&, int) { return nullptr; }
ReaderWriter* VisitorEncoding::CreateWriter(Serializer&, int) { return nullptr; }
}  // namespace IO

// ===========================================================================
// IO::BinaryEncoding / TextEncoding / VisitorEncoding -- the three unexported
// minted tables 0x1801554a8 / 0x1801559d0 / 0x1801559f8 (rows 0, 2, 3).
// ===========================================================================
// Every stamper now spells `::new (p) IO::XEncoding()` into the 8-byte
// MemAlloc(8,4) block. Retail's whole "constructor" is ONE vptr store, so the
// real class must write the first 8 bytes and nothing after them.
UNIT_TEST(b39_io_encoding_slot_order_by_pmf) {
    CHECK_EQ(SlotOffset(&IO::Encoding::IsBinary),     (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::Encoding::IsText),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&IO::Encoding::GetKind),      (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&IO::Encoding::CreateReader), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&IO::Encoding::CreateWriter), (uintptr_t)0x20);
    CHECK(!std::is_abstract<IO::BinaryEncoding>::value);
    CHECK(!std::is_abstract<IO::TextEncoding>::value);
    CHECK(!std::is_abstract<IO::VisitorEncoding>::value);
    CHECK(!std::has_virtual_destructor<IO::Encoding>::value);
}

template <class E>
static bool PlacementWritesOnlyTheVptr(bool bin, bool txt, int kind) {
    alignas(8) uint8_t buf[16];
    std::memset(buf, 0xCD, sizeof buf);
    IO::Encoding* e = ::new (buf) E();
    bool ok = static_cast<void*>(e) == static_cast<void*>(buf);
    for (int i = 8; i < 16; ++i) ok = ok && buf[i] == 0xCD;   // one qword only
    ok = ok && VTableOf(buf) != nullptr;
    ok = ok && e->IsBinary() == bin && e->IsText() == txt && e->GetKind() == kind;
    alignas(8) uint8_t buf2[8];
    ::new (buf2) E();
    ok = ok && VTableOf(buf) == VTableOf(buf2);                // one table per class
    return ok;
}

UNIT_TEST(b39_io_encoding_ctor_is_one_vptr_store_with_retails_constants) {
    CHECK(PlacementWritesOnlyTheVptr<IO::BinaryEncoding>(true, false, 2));    // 0x1800a81e0/d0/c0
    CHECK(PlacementWritesOnlyTheVptr<IO::TextEncoding>(false, true, 1));      // 0x1800b9ab0/aa0/a90
    CHECK(PlacementWritesOnlyTheVptr<IO::VisitorEncoding>(false, false, 3));  // 0x1800bafe0/d0/c0
    alignas(8) uint8_t a[8], b[8], c[8];
    ::new (a) IO::BinaryEncoding();
    ::new (b) IO::TextEncoding();
    ::new (c) IO::VisitorEncoding();
    CHECK(VTableOf(a) != VTableOf(b));
    CHECK(VTableOf(b) != VTableOf(c));
    CHECK(VTableOf(a) != VTableOf(c));
}

// ===========================================================================
// IO::BinaryReader / BinaryWriter / Visitor, and their base IO::ReaderWriter
// -- rows 14 / 15 / 150 / 109.
// ===========================================================================

// Host stand-ins for the out-of-line virtual bodies (runtime/src/io/
// serialize_family.cpp), so the REAL classes' vtables land in the test binary.
// The tests below never dispatch them.
namespace IO {
void BinaryReader::Skip() {}
#define B39_IO_RW_STUBS(CLS)                                                      \
    void CLS::SerializeCharArray  (int, char*,          const char*) {}          \
    void CLS::SerializeBoolArray  (int, bool*,          const char*) {}          \
    void CLS::SerializeFloatArray (int, float*,         const char*) {}          \
    void CLS::SerializeUInt64Array(int, uint64_t*,      const char*) {}          \
    void CLS::SerializeInt64Array (int, int64_t*,       const char*) {}          \
    void CLS::SerializeUIntArray  (int, uint32_t*,      const char*) {}          \
    void CLS::SerializeIntArray   (int, int32_t*,       const char*) {}          \
    void CLS::SerializeUShortArray(int, uint16_t*,      const char*) {}          \
    void CLS::SerializeShortArray (int, int16_t*,       const char*) {}          \
    void CLS::SerializeUCharArray (int, unsigned char*, const char*) {}          \
    void CLS::SerializeSCharArray (int, signed char*,   const char*) {}          \
    void CLS::SerializeChar  (char&,          const char*) {}                    \
    void CLS::SerializeBool  (bool&,          const char*) {}                    \
    void CLS::SerializeFloat (float&,         const char*) {}                    \
    void CLS::SerializeUInt64(uint64_t&,      const char*) {}                    \
    void CLS::SerializeInt64 (int64_t&,       const char*) {}                    \
    void CLS::SerializeUInt  (uint32_t&,      const char*) {}                    \
    void CLS::SerializeInt   (int32_t&,       const char*) {}                    \
    void CLS::SerializeUShort(uint16_t&,      const char*) {}                    \
    void CLS::SerializeShort (int16_t&,       const char*) {}                    \
    void CLS::SerializeUChar (unsigned char&, const char*) {}                    \
    void CLS::SerializeSChar (signed char&,   const char*) {}                    \
    void CLS::SerializeBytes (int, void*,     const char*) {}
B39_IO_RW_STUBS(BinaryReader)
B39_IO_RW_STUBS(BinaryWriter)
#undef B39_IO_RW_STUBS
}  // namespace IO

namespace {

// A stream reached ONLY through the four raw slots the family uses (+0x10
// Write, +0x18 Read, +0x20 SetCurrentPos, +0x28 GetCurrentPos) -- the same
// shape a live IO::Stream presents -- over a 256-byte buffer, counting reads.
struct FakeStream {
    const void* const* vt;
    uint8_t buf[256];
    int32_t pos;
    int32_t reads;
};
bool FsWrite(void* s, int32_t n, void* src) {
    FakeStream* f = static_cast<FakeStream*>(s);
    std::memcpy(f->buf + f->pos, src, (size_t)n); f->pos += n; return true;
}
bool FsRead(void* s, int32_t n, void* dst) {
    FakeStream* f = static_cast<FakeStream*>(s);
    std::memcpy(dst, f->buf + f->pos, (size_t)n); f->pos += n; ++f->reads; return true;
}
bool FsSeek(void* s, int32_t p) { static_cast<FakeStream*>(s)->pos = p; return true; }
int32_t FsPos(void* s) { return static_cast<FakeStream*>(s)->pos; }
const void* const kFakeStreamVt[6] = {
    nullptr, nullptr,
    reinterpret_cast<const void*>(&FsWrite), reinterpret_cast<const void*>(&FsRead),
    reinterpret_cast<const void*>(&FsSeek),  reinterpret_cast<const void*>(&FsPos),
};

void InitFake(FakeStream& f) {
    std::memset(&f, 0, sizeof f);
    f.vt = kFakeStreamVt;
}

// A Serializer's 16 bytes pointing at the fake stream (the Serializer ctor
// itself asks the stream's Encoding for a ReaderWriter; these tests build the
// ReaderWriter directly, as the Encoding factories do).
struct SerBuf { alignas(8) uint8_t b[16]; };
IO::Serializer& SerOver(SerBuf& sb, FakeStream& f) {
    std::memset(sb.b, 0, sizeof sb.b);
    IO::Serializer& s = *reinterpret_cast<IO::Serializer*>(sb.b);
    s._stream = reinterpret_cast<IO::Stream*>(&f);
    return s;
}

int32_t Dword(const FakeStream& f, int off) {
    int32_t v; std::memcpy(&v, f.buf + off, 4); return v;
}

}  // namespace

UNIT_TEST(b39_io_readerwriter_family_shape) {
    CHECK(std::is_abstract<IO::ReaderWriter>::value);          // 25 _purecall slots
    CHECK(!std::is_abstract<IO::BinaryReader>::value);
    CHECK(!std::is_abstract<IO::BinaryWriter>::value);
    CHECK(!std::is_abstract<IO::Visitor>::value);
    CHECK(!std::is_destructible<IO::BinaryReader>::value);     // protected, slot 0 only
    CHECK(!std::is_destructible<IO::BinaryWriter>::value);
    CHECK(!std::is_destructible<IO::Visitor>::value);
    CHECK(!std::has_virtual_destructor<IO::BinaryReader>::value);
    CHECK(!std::has_virtual_destructor<IO::BinaryWriter>::value);
    CHECK(!std::has_virtual_destructor<IO::Visitor>::value);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::GetVersion),     (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::Skip),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::SerializeCharArray), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::SerializeChar),  (uintptr_t)0x70);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::SerializeInt),   (uintptr_t)0xA0);
    CHECK_EQ(SlotOffset(&IO::BinaryReader::SerializeBytes), (uintptr_t)0xC8);
    CHECK_EQ(SlotOffset(&IO::BinaryWriter::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::BinaryWriter::SerializeUInt),  (uintptr_t)0x98);
    CHECK_EQ(SlotOffset(&IO::BinaryWriter::SerializeBytes), (uintptr_t)0xC8);
    CHECK_EQ(SlotOffset(&IO::Visitor::DeletingDtor),        (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::Visitor::GetVersion),          (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&IO::Visitor::SerializeBytes),      (uintptr_t)0xC8);
}

// 0x1800a8a30 then 0x1800a8980: magic, -1 placeholder, version; then the
// trailer and the BACK-PATCHED length (everything after the placeholder,
// version dword included) -- the 12 + 4 bytes the resave gate sees per record.
UNIT_TEST(b39_io_binarywriter_ctor_and_dtor_frame_the_record) {
    FakeStream f; InitFake(f);
    SerBuf sb; IO::Serializer& ser = SerOver(sb, f);
    alignas(8) uint8_t mem[0x30];
    std::memset(mem, 0xCD, sizeof mem);
    IO::BinaryWriter* w = ::new (mem) IO::BinaryWriter(ser, 7);
    CHECK_EQ((int)mem[0x08], 0);                         // allowCustomSerialize
    for (int i = 0x09; i < 0x10; ++i) CHECK_EQ((int)mem[i], 0xCD);   // not written
    CHECK(w->serializer == &ser);
    CHECK(w->stream == static_cast<void*>(&f));
    CHECK_EQ(w->word20, 4);                              // after the magic
    CHECK_EQ(w->word24, 8);                              // after the placeholder
    CHECK_EQ(w->version, 7);
    CHECK_EQ(w->GetVersion(), 7);
    CHECK_EQ((uint32_t)Dword(f, 0), 0xaaaaaaaau);
    CHECK_EQ(Dword(f, 4), -1);
    CHECK_EQ(Dword(f, 8), 7);
    int32_t payload = 0x1234;
    FsWrite(&f, 4, &payload);                            // one field of the record
    CHECK(w->DeletingDtor(0) == w);                      // flags 0: no free
    CHECK_EQ((uint32_t)Dword(f, 16), 0xbbbbbbbbu);       // trailer
    CHECK_EQ(Dword(f, 4), 8);                            // len = 16 - 8, back-patched
    CHECK_EQ(f.pos, 20);                                 // and forward again
}

// 0x1800a8340 then 0x1800a82f0: reads magic / length / version (the argument is
// overwritten), remembers the position after the length, and the dtor swallows
// the trailer. Slot 0 with flags 1 frees through MemFree (malloc on the host).
UNIT_TEST(b39_io_binaryreader_consumes_the_record_both_arms) {
    FakeStream f; InitFake(f);
    const int32_t rec[5] = { (int32_t)0xaaaaaaaau, 8, 9, 0x1234, (int32_t)0xbbbbbbbbu };
    std::memcpy(f.buf, rec, sizeof rec);
    SerBuf sb; IO::Serializer& ser = SerOver(sb, f);
    void* p = std::malloc(0x30);
    IO::BinaryReader* r = ::new (p) IO::BinaryReader(ser, 99);
    CHECK_EQ(r->word20, 8);
    CHECK_EQ(r->word24, 8);
    CHECK_EQ(r->version, 9);                             // 99 overwritten by the read
    CHECK_EQ(f.reads, 3);
    f.pos = 16;                                          // past the payload
    CHECK(r->DeletingDtor(1) == r);                      // frees p
    CHECK_EQ(f.reads, 4);                                // the trailer read
    CHECK_EQ(f.pos, 20);

    // The VECTOR arm (0x180023a10: count at this-8, stride 0x30, downward).
    InitFake(f);
    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 2 * 0x30));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    IO::BinaryReader* e0 = ::new (block + 8)        IO::BinaryReader(ser, 0);
    ::new (block + 8 + 0x30) IO::BinaryReader(ser, 0);
    f.reads = 0;
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
    CHECK_EQ(f.reads, 2);                                // both dtors ran
}

// The Visitor factories' inlined ctor: base stores + version at +0x20; no
// stream traffic. Both slot-0 arms (stride 0x28).
UNIT_TEST(b39_io_visitor_ctor_stores_and_both_arms) {
    FakeStream f; InitFake(f);
    SerBuf sb; IO::Serializer& ser = SerOver(sb, f);
    alignas(8) uint8_t mem[0x28];
    std::memset(mem, 0xCD, sizeof mem);
    IO::Visitor* v = ::new (mem) IO::Visitor(ser, 5);
    CHECK_EQ((int)mem[0x08], 0);
    CHECK(v->serializer == &ser);
    CHECK(v->stream == static_cast<void*>(&f));
    CHECK_EQ(v->GetVersion(), 5);
    int32_t at20; std::memcpy(&at20, mem + 0x20, 4);
    CHECK_EQ(at20, 5);
    for (int i = 0x24; i < 0x28; ++i) CHECK_EQ((int)mem[i], 0xCD);  // _pad24 untouched
    CHECK_EQ(f.pos, 0);
    CHECK(v->DeletingDtor(0) == v);

    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 3 * 0x28));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    IO::Visitor* e0 = nullptr;
    for (int i = 0; i < 3; ++i) {
        IO::Visitor* e = ::new (block + 8 + i * 0x28) IO::Visitor(ser, i);
        if (i == 0) e0 = e;
    }
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);   // freed
}

// ===========================================================================
// IO::VisitorStream -- row 151. `?Add@ExternalFactory@` stack-builds one.
// ===========================================================================
UNIT_TEST(b39_io_visitorstream_ctor_and_both_arms) {
    CHECK(!std::is_abstract<IO::VisitorStream>::value);
    CHECK(!std::is_destructible<IO::VisitorStream>::value);
    CHECK(!std::has_virtual_destructor<IO::VisitorStream>::value);
    CHECK_EQ(SlotOffset(&IO::VisitorStream::DeletingDtor),  (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::VisitorStream::GetSource),     (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&IO::VisitorStream::Write),         (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&IO::VisitorStream::GetSize),       (uintptr_t)0x30);

    // 0x1800bb090: +0x08 = 1 (WRITING), +0x10 = a fresh VISITOR encoding,
    // +0x0c never written.
    alignas(8) uint8_t mem[0x18];
    std::memset(mem, 0xCD, sizeof mem);
    IO::VisitorStream* vs = ::new (mem) IO::VisitorStream();
    CHECK_EQ(vs->_direction, 1);
    CHECK_EQ(vs->GetDirection(), IO::kWriting);
    for (int i = 0x0c; i < 0x10; ++i) CHECK_EQ((int)mem[i], 0xCD);
    CHECK(vs->_encoding != nullptr);
    CHECK_EQ(vs->_encoding->GetKind(), 3);
    CHECK(!vs->_encoding->IsBinary());
    CHECK(vs->DeletingDtor(0) == vs);          // ~VisitorStream -> ~Stream frees the encoding

    // The vector arm (0x18002bf80: stride 0x18).
    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 2 * 0x18));
    const int64_t cookie = 2;
    std::memcpy(block, &cookie, sizeof cookie);
    IO::VisitorStream* e0 = ::new (block + 8) IO::VisitorStream();
    ::new (block + 8 + 0x18) IO::VisitorStream();
    CHECK(reinterpret_cast<uint8_t*>(e0->DeletingDtor(3)) == block);
}

// ===========================================================================
// IO::FileStream / BinaryFileStream / TextFileStream -- rows 58 / 13 / 139.
// ===========================================================================
// ~FileStream / ~TextFileStream and the TextFileStream ctor are file_stream.cpp's
// (host stubs in io_stream_abi_stubs.cpp); the FileStream and BinaryFileStream
// constructors and all three slot-0 bodies are inline and REAL here.
UNIT_TEST(b39_io_filestream_family_shape) {
    CHECK(!std::is_abstract<IO::FileStream>::value);
    CHECK(!std::is_abstract<IO::BinaryFileStream>::value);
    CHECK(!std::is_abstract<IO::TextFileStream>::value);
    CHECK(!std::is_destructible<IO::FileStream>::value);
    CHECK(!std::is_destructible<IO::BinaryFileStream>::value);
    CHECK(!std::is_destructible<IO::TextFileStream>::value);
    CHECK(!std::has_virtual_destructor<IO::FileStream>::value);
    CHECK(!std::has_virtual_destructor<IO::TextFileStream>::value);
    CHECK_EQ(SlotOffset(&IO::FileStream::DeletingDtor),        (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::FileStream::GetSource),           (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&IO::FileStream::Open),                (uintptr_t)0x38);   // NOT Close
    CHECK_EQ(SlotOffset(&IO::BinaryFileStream::DeletingDtor),  (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::TextFileStream::DeletingDtor),    (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::TextFileStream::OpenWithFormat),  (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&IO::TextFileStream::Create),          (uintptr_t)0x48);
}

// 0x1800aaa10: Stream base, `_filename` null, FileInfo handle -1, size/cursor
// 0, baseOffset 0, both names null, winHandle -1; +0x24 (_unk04), +0x34
// (_pad14) and +0x0c never written; NO encoding. 0x1800a8200 adds the BINARY
// encoding and nothing else.
UNIT_TEST(b39_io_filestream_and_binaryfilestream_ctor_stores) {
    alignas(8) uint8_t mem[0x50];
    std::memset(mem, 0xCD, sizeof mem);
    IO::FileStream* fs = ::new (mem) IO::FileStream();
    CHECK_EQ(fs->_direction, 0);
    CHECK(fs->_encoding == nullptr);
    CHECK(fs->_filename == nullptr);
    CHECK_EQ(fs->_file.handle, -1);
    CHECK_EQ(fs->_file.size, 0);
    CHECK_EQ(fs->_file.cursor, 0);
    CHECK_EQ(fs->_file.baseOffset, 0);
    CHECK_EQ(fs->_file.winHandle, (int64_t)-1);
    CHECK(fs->_file._name0.data == nullptr);
    CHECK(fs->_file._name1.data == nullptr);
    for (int i = 0x0c; i < 0x10; ++i) CHECK_EQ((int)mem[i], 0xCD);
    for (int i = 0x24; i < 0x28; ++i) CHECK_EQ((int)mem[i], 0xCD);   // FileInfo::_unk04
    for (int i = 0x34; i < 0x38; ++i) CHECK_EQ((int)mem[i], 0xCD);   // FileInfo::_pad14
    CHECK(fs->DeletingDtor(0) == fs);

    alignas(8) uint8_t mem2[0x50];
    IO::BinaryFileStream* bfs = ::new (mem2) IO::BinaryFileStream();
    CHECK(bfs->_encoding != nullptr);
    CHECK(bfs->_encoding->IsBinary());
    CHECK_EQ(bfs->_encoding->GetKind(), 2);
    CHECK_EQ(bfs->_file.handle, -1);
    CHECK(VTableOf(bfs) != VTableOf(fs));               // slot 0 differs, one table per class
    CHECK(bfs->DeletingDtor(0) == bfs);                 // ~Stream frees the encoding
}

// Both arms of the three slot-0 bodies (0x18000c090 / 0x180023d80 stride 0x50,
// 0x18002ba90 stride 0x80): flags 1 frees `this`, flags 3 frees `this - 8` and
// returns it.
UNIT_TEST(b39_io_filestream_family_deletingdtor_arms) {
    IO::FileStream* fs = ::new (std::malloc(0x50)) IO::FileStream();
    CHECK(fs->DeletingDtor(1) == fs);
    IO::BinaryFileStream* bfs = ::new (std::malloc(0x50)) IO::BinaryFileStream();
    CHECK(bfs->DeletingDtor(1) == bfs);

    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 2 * 0x50));
    const int64_t two = 2;
    std::memcpy(block, &two, sizeof two);
    IO::BinaryFileStream* b0 = ::new (block + 8) IO::BinaryFileStream();
    ::new (block + 8 + 0x50) IO::BinaryFileStream();
    CHECK(reinterpret_cast<uint8_t*>(b0->DeletingDtor(3)) == block);

    uint8_t* tblock = static_cast<uint8_t*>(std::malloc(8 + 2 * 0x80));
    std::memcpy(tblock, &two, sizeof two);
    IO::TextFileStream* t0 =
        ::new (tblock + 8) IO::TextFileStream(System::String(), System::String(), 0);
    ::new (tblock + 8 + 0x80) IO::TextFileStream(System::String(), System::String(), 0);
    CHECK_EQ(t0->_requestedFormat, 6);
    CHECK(reinterpret_cast<uint8_t*>(t0->DeletingDtor(3)) == tblock);
}

// ===========================================================================
// IO::MemoryStream -- row 88. 0x1800ae710 / 0x1800ae890 / slot 0 0x1800242a0.
// ===========================================================================
UNIT_TEST(b39_io_memorystream_shape_ctor_and_both_arms) {
    CHECK(!std::is_abstract<IO::MemoryStream>::value);
    CHECK(!std::is_destructible<IO::MemoryStream>::value);
    CHECK(!std::has_virtual_destructor<IO::MemoryStream>::value);
    CHECK_EQ(SlotOffset(&IO::MemoryStream::DeletingDtor), (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::MemoryStream::Write),        (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&IO::MemoryStream::Close),        (uintptr_t)0x38);   // NOT Open

    // +0x08 = 0, +0x18 = null, +0x20 = 0 (one byte), +0x24..+0x2b = 0, +0x10 =
    // a fresh BINARY encoding; +0x0c, +0x21..+0x23 and +0x2c never written.
    alignas(8) uint8_t mem[0x30];
    std::memset(mem, 0xCD, sizeof mem);
    IO::MemoryStream* ms = ::new (mem) IO::MemoryStream();
    CHECK_EQ(ms->_direction, 0);
    CHECK(ms->_data == nullptr);
    CHECK_EQ((int)ms->_ownsData, 0);
    CHECK_EQ(ms->_cursor, 0u);
    CHECK_EQ(ms->_size, 0u);
    CHECK(ms->_encoding != nullptr);
    CHECK(ms->_encoding->IsBinary());
    for (int i = 0x0c; i < 0x10; ++i) CHECK_EQ((int)mem[i], 0xCD);
    for (int i = 0x21; i < 0x24; ++i) CHECK_EQ((int)mem[i], 0xCD);
    for (int i = 0x2c; i < 0x30; ++i) CHECK_EQ((int)mem[i], 0xCD);
    // An OWNED buffer is freed by ~MemoryStream, then ~Stream frees the encoding.
    ms->_data = std::malloc(16);
    ms->_ownsData = 1;
    CHECK(ms->DeletingDtor(0) == ms);

    // flags 1 frees `this`; the vector arm (stride 0x30) frees `this - 8`.
    IO::MemoryStream* heap = ::new (std::malloc(0x30)) IO::MemoryStream();
    CHECK(heap->DeletingDtor(1) == heap);
    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 2 * 0x30));
    const int64_t two = 2;
    std::memcpy(block, &two, sizeof two);
    IO::MemoryStream* m0 = ::new (block + 8) IO::MemoryStream();
    ::new (block + 8 + 0x30) IO::MemoryStream();
    CHECK(reinterpret_cast<uint8_t*>(m0->DeletingDtor(3)) == block);
}

// ===========================================================================
// IO::CompressedFileStream -- row 31. 0x1800a93b0 / slot 0 0x180024100.
// ===========================================================================
// ~CompressedFileStream and ~CompressedStream are host stubs (release nothing);
// the constructors and slot 0 are inline and REAL here.
UNIT_TEST(b39_io_compressedfilestream_shape_ctor_and_both_arms) {
    CHECK(!std::is_abstract<IO::CompressedFileStream>::value);
    CHECK(!std::is_destructible<IO::CompressedFileStream>::value);
    CHECK(!std::has_virtual_destructor<IO::CompressedFileStream>::value);
    CHECK_EQ(SlotOffset(&IO::CompressedFileStream::DeletingDtor), (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&IO::CompressedFileStream::Close),        (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&IO::CompressedFileStream::PreCommit),    (uintptr_t)0x40);

    // 0x1800aa3d0: +0x1c = 0, +0x20 = -1, +0x28..+0x67 = 0, a BINARY encoding;
    // +0x0c, +0x18, +0x24 never written. 0x1800a93b0 then builds the
    // BinaryFileStream member at +0x68 (its own Binary encoding, handle -1).
    alignas(8) uint8_t mem[0xb8];
    std::memset(mem, 0xCD, sizeof mem);
    IO::CompressedFileStream* cfs = ::new (mem) IO::CompressedFileStream();
    CHECK_EQ(cfs->_direction, 0);
    CHECK_EQ(cfs->_position, 0);
    CHECK_EQ(cfs->_activeWindow, -1);
    CHECK(cfs->_source == nullptr);
    CHECK(cfs->_windowData == nullptr);
    CHECK_EQ(cfs->_headerSize, 0);
    CHECK(cfs->_blockData == nullptr);
    CHECK(cfs->_compression == nullptr);
    CHECK(cfs->_encoding != nullptr);
    CHECK(cfs->_encoding->IsBinary());
    for (int i = 0x0c; i < 0x10; ++i) CHECK_EQ((int)mem[i], 0xCD);
    for (int i = 0x18; i < 0x1c; ++i) CHECK_EQ((int)mem[i], 0xCD);
    for (int i = 0x24; i < 0x28; ++i) CHECK_EQ((int)mem[i], 0xCD);
    CHECK(cfs->_file._encoding != nullptr);
    CHECK(cfs->_file._encoding != cfs->_encoding);
    CHECK_EQ(cfs->_file._file.handle, -1);
    CHECK(VTableOf(cfs) != VTableOf(&cfs->_file));
    CHECK(cfs->DeletingDtor(0) == cfs);

    IO::CompressedFileStream* heap = ::new (std::malloc(0xb8)) IO::CompressedFileStream();
    CHECK(heap->DeletingDtor(1) == heap);
    uint8_t* block = static_cast<uint8_t*>(std::malloc(8 + 2 * 0xb8));
    const int64_t two = 2;
    std::memcpy(block, &two, sizeof two);
    IO::CompressedFileStream* c0 = ::new (block + 8) IO::CompressedFileStream();
    ::new (block + 8 + 0xb8) IO::CompressedFileStream();
    CHECK(reinterpret_cast<uint8_t*>(c0->DeletingDtor(3)) == block);
}
