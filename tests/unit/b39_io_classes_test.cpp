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

#include <cstdint>
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
