// ============================================================================
// tests/unit/io_streams_abi_test.cpp — the REAL `IO` stream classes' vtable
// order and layout.   ENDGAME B.3.4 block 4 (STREAMS).
// ============================================================================
//
// WHY THIS IS ITS OWN FILE. `io_vtable_abi_test.cpp` is the shared IO ABI suite
// and it already carries the Encoding / ReaderWriter / Serializer pins from
// blocks 1-3, plus the first two STREAMS blocks' probes. Two parallel agents
// appending to the END of that one file produced a merge conflict whose marker
// split the single shared closing brace, so from block 4's third commit onward
// the stream probes live here. Registration is automatic (the Makefile globs
// `tests/unit/*.cpp`); the helper channel below is this file's own copy,
// deliberately, because the one in io_vtable_abi_test.cpp has internal linkage.
//
// WHAT IT PINS, AND WHY IT HAS TO EXIST
// -------------------------------------
// Nothing else in the build compiles a real `IO` stream vtable at all. Every
// live stream wears a MINTED table out of `standalone/rt_data_mint_tables.inc`
// and every registered door calls QUALIFIED, so a slot declared one line out of
// place in runtime/types/io/Stream.h or FileStream.h would stay invisible right
// through B.3.4 and surface only when B.3 steps 5-8 retire the minted rows — as
// a stream that reads through `Close` or seeks through `Open`, with no recent
// change to blame.
//
// The shape is block 3's section B.7 shape: a CONCRETE probe subclass of each
// real class, every slot overridden with its own marker, then a raw slot call
// at every retail byte offset asserting the marker equals offset/8.
//
// HOW SLOT 1 IS PINNED WITHOUT CALLING IT. `GetSource` returns a
// `System::String` BY VALUE, so it carries a hidden return pointer, and the
// registered door (`RtIo_Stream_GetSource`, iobase_vtables.cpp) spells that
// slot in MSVC's argument order while a C++ member compiled here uses the
// compiler's. Raw-calling it would be asserting a calling convention, not a
// slot order. So slot 1 is pinned BY ELIMINATION: every OTHER slot is
// raw-called and lands on its own marker, and all the table's entries are
// asserted distinct. With n distinct entries and n-1 of them pinned, the
// remaining one is at the remaining index — a complete proof that depends on
// no convention at all.
//
// The out-of-line bodies these probes need at link time are in
// tests/unit/io_stream_abi_stubs.cpp; that file explains why.
// ============================================================================

#include "unit.h"

#include "runtime/types/io/Stream.h"
#include "runtime/types/io/FileStream.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace {

// ---------------------------------------------------------------------------
// Observation channel — this file's own copy (the one in io_vtable_abi_test.cpp
// has internal linkage and is not reachable from here).
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = 0;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = 0; }

// The one instruction every raw slot caller in runtime/src compiles to.
template <typename Fn>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(
        *reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(vt) + byteOffset));
}

// ---------------------------------------------------------------------------
// Byte offsets, spelled once. `IO::Stream` is 7 slots; the concrete classes
// append theirs (B34_RULES section 1.3).
// ---------------------------------------------------------------------------
constexpr int kDtor          = 0x00;
constexpr int kGetSource     = 0x08;
constexpr int kWrite         = 0x10;
constexpr int kRead          = 0x18;
constexpr int kSetCurrentPos = 0x20;
constexpr int kGetCurrentPos = 0x28;
constexpr int kGetSize       = 0x30;
constexpr int kSlot7         = 0x38;   // Close on the memory/compressed branch,
                                       // Open on the FileStream branch
constexpr int kSlot8         = 0x40;   // PreCommit; the compressed branch only

// The six slots every Stream in the family shares, raw-called at their retail
// byte offsets. Each CHECK says the entry at that offset is the override
// declared at offset/8, with `this` unadjusted.
void CheckSharedStreamSlots(void* raw) {
    uint8_t scratch[8] = {0};

    Reset();
    using DtorFn = void* (*)(void*, unsigned);
    SlotAt<DtorFn>(raw, kDtor)(raw, 0u);
    CHECK_EQ(g_lastMark, kDtor / 8);
    CHECK(g_lastSelf == raw);

    Reset();
    using WriteFn = bool (*)(void*, int, const void*);
    CHECK(SlotAt<WriteFn>(raw, kWrite)(raw, 8, scratch) == true);
    CHECK_EQ(g_lastMark, kWrite / 8);

    Reset();
    using ReadFn = bool (*)(void*, int, void*);
    CHECK(SlotAt<ReadFn>(raw, kRead)(raw, 8, scratch) == true);
    CHECK_EQ(g_lastMark, kRead / 8);

    Reset();
    using SeekFn = bool (*)(void*, int);
    CHECK(SlotAt<SeekFn>(raw, kSetCurrentPos)(raw, 3) == true);
    CHECK_EQ(g_lastMark, kSetCurrentPos / 8);

    Reset();
    using U32Fn = uint32_t (*)(void*);
    SlotAt<U32Fn>(raw, kGetCurrentPos)(raw);
    CHECK_EQ(g_lastMark, kGetCurrentPos / 8);

    Reset();
    SlotAt<U32Fn>(raw, kGetSize)(raw);
    CHECK_EQ(g_lastMark, kGetSize / 8);
}

// Slots 7 and 8 on the compressed branch.
void CheckCompressedTail(void* raw) {
    using VoidFn = void (*)(void*);
    Reset();
    SlotAt<VoidFn>(raw, kSlot7)(raw);
    CHECK_EQ(g_lastMark, kSlot7 / 8);
    CHECK_EQ(g_lastMark, 7);
    Reset();
    SlotAt<VoidFn>(raw, kSlot8)(raw);
    CHECK_EQ(g_lastMark, kSlot8 / 8);
    CHECK_EQ(g_lastMark, 8);
}

// n distinct entries — which is what turns "every slot but one is pinned" into
// "slot 1 is `GetSource`".
void CheckDistinctSlots(void* raw, int n) {
    void* const* vt = *reinterpret_cast<void* const* const*>(raw);
    for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b)
            CHECK(vt[a] != vt[b]);
}

// ---------------------------------------------------------------------------
// The probes. Nine slots is the widest table in the family; the narrower
// classes simply declare fewer.
// ---------------------------------------------------------------------------
#define STREAM_PROBE_SEVEN                                                     \
    IO::Stream* DeletingDtor(unsigned) override { Note(this, 0); return this; }\
    System::String GetSource() const override { Note(this, 1); return System::String(); } \
    bool     Write(int, const void*) override  { Note(this, 2); return true; } \
    bool     Read(int, void*) override         { Note(this, 3); return true; } \
    bool     SetCurrentPos(int) override       { Note(this, 4); return true; } \
    uint32_t GetCurrentPos() const override    { Note(this, 5); return 0; }    \
    uint32_t GetSize() const override          { Note(this, 6); return 0; }

struct CompressedStreamProbe final : IO::CompressedStream {
    STREAM_PROBE_SEVEN
    void Close() override     { Note(this, 7); }
    void PreCommit() override { Note(this, 8); }
    ~CompressedStreamProbe() = default;
};

struct CompressedMemoryStreamProbe final : IO::CompressedMemoryStream {
    STREAM_PROBE_SEVEN
    void Close() override     { Note(this, 7); }
    void PreCommit() override { Note(this, 8); }
    ~CompressedMemoryStreamProbe() = default;
};

struct CompressedFileStreamProbe final : IO::CompressedFileStream {
    STREAM_PROBE_SEVEN
    void Close() override     { Note(this, 7); }
    void PreCommit() override { Note(this, 8); }
    ~CompressedFileStreamProbe() = default;
};

UNIT_TEST(the_real_CompressedStream_has_9_slots_Close_at_7_PreCommit_at_8) {
    CompressedStreamProbe p;
    void* raw = static_cast<void*>(static_cast<IO::Stream*>(&p));

    CheckSharedStreamSlots(raw);
    CheckCompressedTail(raw);
    CheckDistinctSlots(raw, 9);

    // Slot 1 by elimination, and confirmed to be a virtual at all by an
    // ordinary C++ call through the base.
    Reset();
    { const IO::Stream& s = p; (void)s.GetSource(); }
    CHECK_EQ(g_lastMark, kGetSource / 8);

    // Nine, not eight: this is the branch that adds PreCommit. The FileStream
    // branch stops at eight and gives slot 7 a DIFFERENT meaning.
    static_assert(!std::has_virtual_destructor<IO::CompressedStream>::value,
                  "IO::CompressedStream must not acquire a virtual destructor");
    static_assert(!std::has_virtual_destructor<IO::CompressedMemoryStream>::value,
                  "IO::CompressedMemoryStream must not acquire a virtual destructor");
    static_assert(!std::has_virtual_destructor<IO::CompressedFileStream>::value,
                  "IO::CompressedFileStream must not acquire a virtual destructor");
}

UNIT_TEST(the_two_compressed_subclasses_keep_the_9_slot_order) {
    CompressedMemoryStreamProbe cms;
    void* rawCms = static_cast<void*>(static_cast<IO::Stream*>(&cms));
    CheckSharedStreamSlots(rawCms);
    CheckCompressedTail(rawCms);
    CheckDistinctSlots(rawCms, 9);

    CompressedFileStreamProbe cfs;
    void* rawCfs = static_cast<void*>(static_cast<IO::Stream*>(&cfs));
    CheckSharedStreamSlots(rawCfs);
    CheckCompressedTail(rawCfs);
    CheckDistinctSlots(rawCfs, 9);

    // `this` is unadjusted at every level of the chain, which every raw slot
    // caller in runtime/src assumes when it hands the same pointer to any of
    // the three types.
    CHECK(static_cast<void*>(static_cast<IO::Stream*>(&cms)) ==
          static_cast<void*>(static_cast<IO::CompressedStream*>(&cms)));
    CHECK(static_cast<void*>(static_cast<IO::CompressedStream*>(&cfs)) ==
          static_cast<void*>(static_cast<IO::CompressedFileStream*>(&cfs)));
}

UNIT_TEST(the_embedded_stream_at_0x68_carries_its_own_vtable) {
    // B34_RULES section 2 finding 6, now expressed on the real classes: a
    // Compressed*Stream holds a CONCRETE stream BY VALUE at +0x68, and that
    // member is a separate sub-object with its OWN vptr. A raw slot call on the
    // member dispatches through MemoryStream's eight-slot table, not through
    // the enclosing object's nine-slot one — which is exactly what
    // ?Close@CompressedMemoryStream does at 0x1800a9647.
    static_assert(offsetof(IO::CompressedMemoryStream, _memoryStream) == 0x68,
                  "?GetMemoryStream@ is `LEA RAX,[RCX+0x68]; RET`");
    static_assert(offsetof(IO::CompressedFileStream, _file) == 0x68,
                  "ctor 0x1800a93c8: `LEA RCX,[RBX+0x68]`");
    static_assert(sizeof(IO::CompressedMemoryStream) == 0x98, "eh_vector stride");
    static_assert(sizeof(IO::CompressedFileStream) == 0xb8, "eh_vector stride");

    CompressedMemoryStreamProbe cms;
    void* const outer  = static_cast<void*>(static_cast<IO::Stream*>(&cms));
    void* const member = static_cast<void*>(
        static_cast<IO::Stream*>(&cms._memoryStream));

    CHECK_EQ((int)(static_cast<char*>(member) - static_cast<char*>(outer)), 0x68);
    // TWO DIFFERENT TABLES IN ONE OBJECT. If these ever compared equal the
    // member would dispatch through the enclosing class's nine slots and
    // `_memoryStream.Close()` would reach CompressedMemoryStream::Close —
    // unbounded recursion on the save path.
    CHECK(*reinterpret_cast<void**>(outer) != *reinterpret_cast<void**>(member));
    CHECK(*reinterpret_cast<void**>(member) != nullptr);

    CompressedFileStreamProbe cfs;
    void* const fOuter  = static_cast<void*>(static_cast<IO::Stream*>(&cfs));
    void* const fMember = static_cast<void*>(
        static_cast<IO::Stream*>(&cfs._file));
    CHECK_EQ((int)(static_cast<char*>(fMember) - static_cast<char*>(fOuter)), 0x68);
    CHECK(*reinterpret_cast<void**>(fOuter) != *reinterpret_cast<void**>(fMember));

    // ?Close@CompressedFileStream reads the FileInfo at this+0x88, which is
    // +0x68 (the member) + 0x20 (its FileInfo). The member body now spells that
    // as `_file._file`; this is the arithmetic it replaced.
    CHECK_EQ((int)(offsetof(IO::CompressedFileStream, _file) +
                   offsetof(IO::FileStream, _file)), 0x88);
}

UNIT_TEST(the_compressed_classes_are_layout_identical_to_their_PODs) {
    static_assert(sizeof(IO::CompressedStream) == sizeof(RtIo::CompressedStream), "POD");
    static_assert(sizeof(IO::CompressedMemoryStream) ==
                  sizeof(RtIo::CompressedMemoryStream), "POD");
    static_assert(sizeof(IO::CompressedFileStream) ==
                  sizeof(RtIo::CompressedFileStream), "POD");

    CHECK_EQ((int)offsetof(IO::CompressedStream, _position),     0x1c);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _activeWindow), 0x20);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _source),       0x28);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _windowData),   0x30);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _windowSize),   0x38);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _headerData),   0x40);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _headerSize),   0x48);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _blockData),    0x50);
    CHECK_EQ((int)offsetof(IO::CompressedStream, _compression),  0x60);

    // A field write through the CLASS is visible through the POD at the same
    // bytes — the property every reinterpret_cast in runtime/src/io relies on.
    CompressedStreamProbe p;
    p._position   = 0x111;
    p._headerSize = 0x222;
    p._direction  = 1;
    const RtIo::CompressedStream* pod =
        reinterpret_cast<const RtIo::CompressedStream*>(
            static_cast<IO::CompressedStream*>(&p));
    CHECK_EQ((int)pod->position,   0x111);
    CHECK_EQ((int)pod->headerSize, 0x222);
    CHECK_EQ((int)pod->base.direction, 1);
}

}  // namespace
