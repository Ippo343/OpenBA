// ============================================================================
// tests/unit/io_vtable_abi_test.cpp — ENDGAME B.3.4 step 2 (the IO coexistence
// proof), the sibling of `object_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// B.3.4 turns the `IO` PODs (`rt::io::*`, `RtIo::*`, `RtIO::*`) into real C++
// classes ONE AT A TIME, while three raw-offset caller idioms stay live for the
// whole step:
//
//   * `RtObject::SerializerLite` + `kEncSerialize{Bool,Uint,Int}`
//     (runtime/types/object/Internal.h) — 16 hits, all in runtime/{src,types}/object.
//     *** GONE as of B.3.4 block 3: all five files take the real
//     `IO::Serializer` and the one `rw_vtbl` spelling. The idiom below is what
//     they use now. ***
//   * `rt::io::rw_vtbl::k*` named byte offsets over a `ReaderWriter*`
//     (runtime/types/io/Serializer.h:187-214) — 54 files under runtime/src.
//   * `SerializerEncodingVCall(ser, off, field)` (engine/funcs/engine_funcs.h)
//     — 37 call sites in 22 files under code/Game.
//
// All three spell the same instruction: `(*(void***)p)[byteOffset/8](p, …)`.
// So the step is only sound if GCC's compiler-generated vtable for a class
// declared per ENDGAME §B.1 decision 2 is interchangeable, slot for slot, with
// a hand-built `void*[]` at RETAIL byte offsets — in BOTH directions:
//
//   A. an old raw caller reading `(*(void***)rw)[rw_vtbl::kScalarInt/8]` off a
//      REAL `ReaderWriter` lands on that class's override, `this` unadjusted;
//   B. a REAL class's ordinary virtual call on an object whose vptr was stamped
//      with a hand-built array of free functions lands on those free functions.
//
// Direction B is the one that matters for every object still wearing a MINTED
// table out of `standalone/rt_data_mint_tables.inc`, and it is the direction an
// ABI with a hidden prologue slot would break.
//
// THE RETAIL SLOT LISTS BEING MIRRORED
// ------------------------------------
// Read byte-wise out of the shipped Runtime-x64.dll's .rdata (image base
// 0x180000000) in Ghidra on 2026-09-18. Cited here, not re-derived; the full
// tables with every body address are in
// docs/runtime_port/endgame_b/B34_RULES.md §1.
//
//   ??_7ReaderWriter@IO@@6B@  @ 0x1801553b0 — 26 slots: [0] 0x180023bd0
//     (the deleting dtor, also `RT_DELDTOR(..., "IO::ReaderWriter")`), [1..25]
//     all `_purecall` (0x18007ec28). 26*8 = 0xd0, so the last slot's byte
//     offset is 0xc8 == `rw_vtbl::kSerializeBytes`, which is what bounds the
//     table at 26 independently of adjacency.
//   ??_7BinaryReader@IO@@6B@  @ 0x1801552e0 and
//   ??_7BinaryWriter@IO@@6B@  @ 0x180155210 — the same 26 slots with bodies;
//     each table is exactly 0xd0 bytes and they are adjacent
//     (0x180155210 + 0xd0 == 0x1801552e0 + 0xd0 == 0x1801553b0).
//   ??_7Visitor@IO@@6B@       @ 0x1801558b0 — 26 slots, ending at the
//     TextFileStream table 0x180155980.
//
//   The Binary `IO::Encoding` table @ 0x1801554a8 — FIVE slots and NO dtor
//   slot: [0] 0x1800a81e0 `MOV AL,1; RET`, [1] 0x1800a81d0 `XOR AL,AL; RET`,
//   [2] 0x1800a81c0 `MOV EAX,2; RET`, [3] 0x1800a8120 CreateReader,
//   [4] 0x1800a8080 CreateWriter. So slot 3 is at byte +0x18 and slot 4 at
//   +0x20 — `rt::io::encoding_vtbl::kCreateReader/kCreateWriter`.
//
//   ??_7Stream@IO@@6B@ @ 0x1801549a0 — 7 slots, [2..6] `_purecall`.
//   ??_7MemoryStream@IO@@6B@ @ 0x1801555b0 — 8 slots ([7] Close 0x1800ae9e0).
//   ??_7CompressedMemoryStream@IO@@6B@ @ 0x180155560 — 9 slots; the class is
//     0x98 bytes and holds an `IO::MemoryStream` MEMBER (not a base) at +0x68.
//
// WHAT IS *NOT* PROVEN HERE
// -------------------------
// Nothing about MSVC. The retail DLL is gone; every caller and callee is this
// same GCC. The binding binary facts are OBJECT LAYOUT and RETAIL SLOT ORDER,
// and slot order binds only because our own tables were transcribed from
// retail. These tests compare GCC against GCC, which is exactly the question.
// ============================================================================
#include "unit.h"

// The shipped constant tables. The tests index through THESE, not through
// local copies, so a test failure means the header and the ABI disagree.
#include "runtime/types/io/Serializer.h"
#include "runtime/types/io/Stream.h"
#include "runtime/types/io/FileStream.h"
#include "runtime/types/object/Internal.h"

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace {

namespace rw_vtbl = rt::io::rw_vtbl;
namespace enc_vtbl = rt::io::encoding_vtbl;

// ---------------------------------------------------------------------------
// Observation channel: every body below records WHICH one ran and on what
// `this`, so a test can assert the identity of the callee, not merely that
// something returned.
// ---------------------------------------------------------------------------
const void* g_lastSelf = nullptr;
int         g_lastMark = 0;
long long   g_lastArg  = 0;

void Note(const void* self, int mark, long long arg = 0) {
    g_lastSelf = self;
    g_lastMark = mark;
    g_lastArg  = arg;
}
void Reset() { g_lastSelf = nullptr; g_lastMark = 0; g_lastArg = 0; }

// Byte offset of a member measured from two live addresses. `offsetof` on a
// polymorphic type is only conditionally supported; the tree turns the warning
// off globally (Makefile UNIT_CXXFLAGS `-Wno-invalid-offsetof`) for its ~2,100
// layout pins, but this file measures instead so it stays honest C++.
template <typename Obj, typename Field>
uintptr_t FieldOffset(const Obj* obj, const Field* field) {
    return reinterpret_cast<uintptr_t>(field) - reinterpret_cast<uintptr_t>(obj);
}

// The one instruction all three legacy idioms compile to.
template <typename Fn>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(
        *reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(vt) + byteOffset));
}

// ===========================================================================
// PART A — `IO::ReaderWriter`: 26 distinctly-named virtuals, no virtual dtor,
// slot 0 is the deleting destructor declared as an ordinary virtual.
//
// ENDGAME §B.1 decision 2: "Overloaded virtuals: MSVC groups overloads and
// reverses their declaration order; we give them distinct names, so the issue
// does not arise." Retail declares 22 of these 26 as `Serialize` overloads; we
// declare them in SLOT order under distinct names, which is why the slot
// assignment below is a plain 1:1 with the retail byte offsets and needs no
// reversal rule at all. `IO::Serializer`'s NON-virtual forwarders keep the
// public name `Serialize` and stay real C++ overloads (PART B).
// ===========================================================================

class TestReaderWriter {
public:
    // slot 0  +0x00  rw_vtbl::kDtor          retail ??_7ReaderWriter@ [0] 0x180023bd0
    virtual TestReaderWriter* DeletingDtor(unsigned flags) { Note(this, 0, flags); return this; }
    // slot 1  +0x08  rw_vtbl::kGetVersion    (BinaryReader's is `MOV EAX,[RCX+0x28]; RET`)
    virtual int  GetVersion() const           { Note(this, 1); return 7; }
    // slot 2  +0x10  rw_vtbl::kSkip          retail BinaryReader::Skip 0x1800a83f0
    virtual void Skip()                       { Note(this, 2); }

    // slots 3..13 — the eleven ARRAY forms, +0x18 .. +0x68.
    virtual void SerializeCharArray  (int n, char*,     const char*) { Note(this, 3,  n); }
    virtual void SerializeBoolArray  (int n, bool*,     const char*) { Note(this, 4,  n); }
    virtual void SerializeFloatArray (int n, float*,    const char*) { Note(this, 5,  n); }
    virtual void SerializeUInt64Array(int n, uint64_t*, const char*) { Note(this, 6,  n); }
    virtual void SerializeInt64Array (int n, int64_t*,  const char*) { Note(this, 7,  n); }
    virtual void SerializeUIntArray  (int n, uint32_t*, const char*) { Note(this, 8,  n); }
    virtual void SerializeIntArray   (int n, int32_t*,  const char*) { Note(this, 9,  n); }
    virtual void SerializeUShortArray(int n, uint16_t*, const char*) { Note(this, 10, n); }
    virtual void SerializeShortArray (int n, int16_t*,  const char*) { Note(this, 11, n); }
    virtual void SerializeUCharArray (int n, uint8_t*,  const char*) { Note(this, 12, n); }
    virtual void SerializeSCharArray (int n, int8_t*,   const char*) { Note(this, 13, n); }

    // slots 14..24 — the eleven SCALAR forms, +0x70 .. +0xC0.
    virtual void SerializeChar  (char&     v, const char*) { Note(this, 14, v); }
    virtual void SerializeBool  (bool&     v, const char*) { Note(this, 15, v ? 1 : 0); }
    virtual void SerializeFloat (float&    v, const char*) { Note(this, 16, (long long)v); }
    virtual void SerializeUInt64(uint64_t& v, const char*) { Note(this, 17, (long long)v); }
    virtual void SerializeInt64 (int64_t&  v, const char*) { Note(this, 18, v); }
    virtual void SerializeUInt  (uint32_t& v, const char*) { Note(this, 19, (long long)v); }
    virtual void SerializeInt   (int32_t&  v, const char*) { Note(this, 20, v); }
    virtual void SerializeUShort(uint16_t& v, const char*) { Note(this, 21, v); }
    virtual void SerializeShort (int16_t&  v, const char*) { Note(this, 22, v); }
    virtual void SerializeUChar (uint8_t&  v, const char*) { Note(this, 23, v); }
    virtual void SerializeSChar (int8_t&   v, const char*) { Note(this, 24, v); }

    // slot 25 +0xC8  rw_vtbl::kSerializeBytes
    virtual void SerializeBytes(int size, void*, const char*) { Note(this, 25, size); }

    // Fields at RETAIL offsets (runtime/types/io/Serializer.h:118-127).
    uint8_t  allowCustomSerialize;  // +0x08
    uint8_t  _pad09[7];             // +0x09
    void*    serializer;            // +0x10
    void*    stream;                // +0x18

protected:
    // NOT virtual — decision 2. Deletion goes through slot 0, never `delete`.
    ~TestReaderWriter() = default;
};

// A concrete one so it can be instantiated and its own table read.
class TestBinaryReader final : public TestReaderWriter {
public:
    TestBinaryReader() { allowCustomSerialize = 0; serializer = nullptr; stream = nullptr; }
    ~TestBinaryReader() = default;
};

// ---------------------------------------------------------------------------
// DIRECTION A — the legacy raw caller. These three helpers are transcriptions
// of the three live idioms, written against `void*` exactly as the tree writes
// them today, so the test exercises the real shapes and not a tidied one.
// ---------------------------------------------------------------------------

// `runtime/src/io/serializer.cpp`'s `RwSlot()` + a call, i.e. every
// `rw_vtbl::k*` site under runtime/src.
void LegacySerializeInt(void* rw, int32_t* v, const char* name) {
    using Fn = void (*)(void*, int32_t*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kScalarInt)(rw, v, name);
}
void LegacySerializeUInt(void* rw, uint32_t* v, const char* name) {
    using Fn = void (*)(void*, uint32_t*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kScalarUInt)(rw, v, name);
}
void LegacySerializeBool(void* rw, bool* v, const char* name) {
    using Fn = void (*)(void*, bool*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kScalarBool)(rw, v, name);
}
void LegacySerializeFloat(void* rw, float* v, const char* name) {
    using Fn = void (*)(void*, float*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kScalarFloat)(rw, v, name);
}
void LegacySerializeIntArray(void* rw, int n, int32_t* p, const char* name) {
    using Fn = void (*)(void*, int, int32_t*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kArrayInt)(rw, n, p, name);
}
void LegacySerializeBytes(void* rw, int size, void* p, const char* name) {
    using Fn = void (*)(void*, int, void*, const char*);
    SlotAt<Fn>(rw, rw_vtbl::kSerializeBytes)(rw, size, p, name);
}

UNIT_TEST(raw_rw_vtbl_offsets_reach_a_real_ReaderWriter_class) {
    TestBinaryReader rw;
    void* raw = &rw;

    int32_t i = 41;
    Reset(); LegacySerializeInt(raw, &i, "i");
    CHECK_EQ(g_lastMark, 20);                       // SerializeInt, slot 20 / +0xA0
    CHECK(g_lastSelf == static_cast<void*>(&rw));   // `this` unadjusted
    CHECK_EQ(g_lastArg, 41LL);

    uint32_t u = 42u;
    Reset(); LegacySerializeUInt(raw, &u, "u");
    CHECK_EQ(g_lastMark, 19);                       // +0x98
    CHECK_EQ(g_lastArg, 42LL);

    bool b = true;
    Reset(); LegacySerializeBool(raw, &b, "b");
    CHECK_EQ(g_lastMark, 15);                       // +0x78
    CHECK_EQ(g_lastArg, 1LL);

    float f = 43.0f;
    Reset(); LegacySerializeFloat(raw, &f, "f");
    CHECK_EQ(g_lastMark, 16);                       // +0x80
    CHECK_EQ(g_lastArg, 43LL);

    int32_t arr[4] = {0, 0, 0, 0};
    Reset(); LegacySerializeIntArray(raw, 4, arr, "arr");
    CHECK_EQ(g_lastMark, 9);                        // +0x48
    CHECK_EQ(g_lastArg, 4LL);

    uint8_t bytes[8] = {0};
    Reset(); LegacySerializeBytes(raw, 8, bytes, "bytes");
    CHECK_EQ(g_lastMark, 25);                       // +0xC8, the last slot
    CHECK_EQ(g_lastArg, 8LL);

    // Slot 0 and slot 1: the two non-Serialize slots the Serializer dtor and
    // GetVersion forwarder reach (serializer.cpp rw_vtbl::kDtor / kGetVersion).
    Reset();
    using DtorFn = void* (*)(void*, unsigned);
    SlotAt<DtorFn>(raw, rw_vtbl::kDtor)(raw, 1u);
    CHECK_EQ(g_lastMark, 0);
    CHECK_EQ(g_lastArg, 1LL);

    Reset();
    using GetVerFn = int (*)(void*);
    CHECK_EQ(SlotAt<GetVerFn>(raw, rw_vtbl::kGetVersion)(raw), 7);
    CHECK_EQ(g_lastMark, 1);
}

UNIT_TEST(the_ReaderWriter_table_is_exactly_26_distinct_slots) {
    TestBinaryReader rw;
    void* const* vt = *reinterpret_cast<void* const* const*>(static_cast<void*>(&rw));

    // Every declared virtual got its own slot: no collapsing, no aliasing, and
    // in particular no pair of "overloads" sharing an entry. 26 slots is what
    // bounds ??_7ReaderWriter@IO@@6B@ at 0x1801553b0 (last byte offset 0xC8).
    for (int a = 0; a < 26; ++a) {
        for (int b = a + 1; b < 26; ++b) {
            CHECK(vt[a] != vt[b]);
        }
    }

    // The named constants are a contiguous 8-byte ladder over exactly those 26.
    static_assert(rw_vtbl::kDtor == 0x00,           "slot 0");
    static_assert(rw_vtbl::kSerializeBytes == 0xC8, "slot 25 — the table's last");
    static_assert(rw_vtbl::kSerializeBytes / 8 == 25, "26 slots, 0..25");
    static_assert(rw_vtbl::kScalarBool  == 0x78, "RETAIL bool  (profile build: 0x90)");
    static_assert(rw_vtbl::kScalarUInt  == 0x98, "RETAIL uint  (profile build: 0xb0)");
    static_assert(rw_vtbl::kScalarInt   == 0xA0, "RETAIL int   (profile build: 0xb8)");

    // B.3.4 block 3: `RtObject::kEncSerialize{Bool,Uint,Int}` USED TO BE
    // ASSERTED HERE, as a second spelling of these same three retail offsets.
    // They are deleted — `rw_vtbl` is now the only spelling — so the assert
    // that tied the two together has nothing left to tie and is replaced by
    // the pins on `rw_vtbl` alone. The three numbers themselves are unchanged
    // and still build-specific: the PROFILE build's ReaderWriter vtable puts
    // them at +0x90 / +0xb0 / +0xb8.
    static_assert(rw_vtbl::kScalarBool == 0x78, "the old kEncSerializeBool");
    static_assert(rw_vtbl::kScalarUInt == 0x98, "the old kEncSerializeUint");
    static_assert(rw_vtbl::kScalarInt  == 0xA0, "the old kEncSerializeInt");

    // And the ladder they sit on is contiguous and 8-byte strided across all
    // 26, which is the property the deleted cross-assert was really protecting:
    // a slot inserted anywhere would move at least one of the three.
    static_assert(rw_vtbl::kScalarBool - rw_vtbl::kScalarChar == 8, "14 -> 15");
    static_assert(rw_vtbl::kScalarInt  - rw_vtbl::kScalarUInt == 8, "19 -> 20");
    static_assert(rw_vtbl::kSerializeBytes - rw_vtbl::kScalarSChar == 8, "24 -> 25");
}

// ---------------------------------------------------------------------------
// DIRECTION B — a hand-built `void*[26]` of free functions, exactly the shape
// `rt_data_mint_tables.inc` publishes, stamped onto a bare object which is then
// driven through the REAL class's ordinary virtual calls.
// ---------------------------------------------------------------------------
namespace minted {

void* Dtor(void* self, unsigned flags)                      { Note(self, 1000 + (int)flags); return self; }
int   GetVersion(void* self)                                { Note(self, 1001); return 99; }
void  Skip(void* self)                                      { Note(self, 1002); }
void  ArrayInt(void* self, int n, int32_t*, const char*)    { Note(self, 1009, n); }
void  ScalarBool(void* self, bool* v, const char*)          { Note(self, 1015, *v ? 1 : 0); }
void  ScalarFloat(void* self, float* v, const char*)        { Note(self, 1016, (long long)*v); }
void  ScalarUInt(void* self, uint32_t* v, const char*)      { Note(self, 1019, (long long)*v); }
void  ScalarInt(void* self, int32_t* v, const char*)        { Note(self, 1020, *v); }
void  Bytes(void* self, int size, void*, const char*)       { Note(self, 1025, size); }
void  Unused(void* self)                                    { Note(self, -1); }

// Byte offset -> entry, written the way a generated `kVSlotsNN[]` row is.
void* g_table[26] = {
    (void*)&Dtor,        // +0x00  kDtor
    (void*)&GetVersion,  // +0x08  kGetVersion
    (void*)&Skip,        // +0x10  kSkip
    (void*)&Unused,      // +0x18  kArrayChar
    (void*)&Unused,      // +0x20  kArrayBool
    (void*)&Unused,      // +0x28  kArrayFloat
    (void*)&Unused,      // +0x30  kArrayUInt64
    (void*)&Unused,      // +0x38  kArrayInt64
    (void*)&Unused,      // +0x40  kArrayUInt
    (void*)&ArrayInt,    // +0x48  kArrayInt
    (void*)&Unused,      // +0x50  kArrayUShort
    (void*)&Unused,      // +0x58  kArrayShort
    (void*)&Unused,      // +0x60  kArrayUChar
    (void*)&Unused,      // +0x68  kArraySChar
    (void*)&Unused,      // +0x70  kScalarChar
    (void*)&ScalarBool,  // +0x78  kScalarBool
    (void*)&ScalarFloat, // +0x80  kScalarFloat
    (void*)&Unused,      // +0x88  kScalarUInt64
    (void*)&Unused,      // +0x90  kScalarInt64
    (void*)&ScalarUInt,  // +0x98  kScalarUInt
    (void*)&ScalarInt,   // +0xA0  kScalarInt
    (void*)&Unused,      // +0xA8  kScalarUShort
    (void*)&Unused,      // +0xB0  kScalarShort
    (void*)&Unused,      // +0xB8  kScalarUChar
    (void*)&Unused,      // +0xC0  kScalarSChar
    (void*)&Bytes,       // +0xC8  kSerializeBytes
};

}  // namespace minted

// The storage a minted-table object occupies: one vptr plus ReaderWriter's
// three fields. Deliberately NOT a TestReaderWriter — nothing constructs it.
struct MintedRwStorage {
    void*    vptr;                   // +0x00
    uint8_t  allowCustomSerialize;   // +0x08
    uint8_t  _pad09[7];
    void*    serializer;             // +0x10
    void*    stream;                 // +0x18
};
static_assert(sizeof(MintedRwStorage) == 0x20, "ReaderWriter storage is 0x20 bytes");

UNIT_TEST(real_class_virtual_calls_reach_a_hand_built_minted_table) {
    MintedRwStorage obj{};
    obj.vptr = static_cast<void*>(minted::g_table);

    // This is the whole point: a converted body holds an `IO::ReaderWriter*`
    // and calls members on it, while the pointee was published by the mint.
    TestReaderWriter* rw = reinterpret_cast<TestReaderWriter*>(&obj);

    int32_t i = 7;
    Reset(); rw->SerializeInt(i, "i");
    CHECK_EQ(g_lastMark, 1020);
    CHECK(g_lastSelf == static_cast<void*>(&obj));
    CHECK_EQ(g_lastArg, 7LL);

    uint32_t u = 8u;
    Reset(); rw->SerializeUInt(u, "u");
    CHECK_EQ(g_lastMark, 1019);
    CHECK_EQ(g_lastArg, 8LL);

    bool b = true;
    Reset(); rw->SerializeBool(b, "b");
    CHECK_EQ(g_lastMark, 1015);

    float f = 9.0f;
    Reset(); rw->SerializeFloat(f, "f");
    CHECK_EQ(g_lastMark, 1016);
    CHECK_EQ(g_lastArg, 9LL);

    int32_t arr[3] = {0, 0, 0};
    Reset(); rw->SerializeIntArray(3, arr, "arr");
    CHECK_EQ(g_lastMark, 1009);
    CHECK_EQ(g_lastArg, 3LL);

    uint8_t bytes[5] = {0};
    Reset(); rw->SerializeBytes(5, bytes, "bytes");
    CHECK_EQ(g_lastMark, 1025);
    CHECK_EQ(g_lastArg, 5LL);

    Reset(); CHECK_EQ(rw->GetVersion(), 99);
    CHECK_EQ(g_lastMark, 1001);

    // `IO::Serializer::~Serializer` (0x1800b99b0) is exactly this call.
    Reset(); rw->DeletingDtor(1u);
    CHECK_EQ(g_lastMark, 1001 + 0);  // 1000 + flags(1) == 1001
    CHECK(g_lastSelf == static_cast<void*>(&obj));
}

UNIT_TEST(a_virtual_destructor_would_shift_every_ReaderWriter_slot) {
    // The regression guard for decision 2, restated for IO. Itanium spends TWO
    // slots on a virtual destructor (complete + deleting), so the first
    // user-declared virtual would land at index 2 and every `rw_vtbl::k*`
    // offset in 54 files would silently point one-and-a-half slots early.
    struct WithVirtualDtor {
        virtual ~WithVirtualDtor() {}
        virtual int First() { return 1; }
    };
    WithVirtualDtor w;
    void* const* vt = *reinterpret_cast<void* const* const*>(static_cast<void*>(&w));
    using Fn = int (*)(void*);
    CHECK_EQ(reinterpret_cast<Fn>(vt[2])(&w), 1);

    // ...whereas the decision-2 shape puts its slot 0 at index 0.
    TestBinaryReader rw;
    void* const* rwvt = *reinterpret_cast<void* const* const*>(static_cast<void*>(&rw));
    Reset();
    using DtorFn = void* (*)(void*, unsigned);
    reinterpret_cast<DtorFn>(rwvt[0])(&rw, 2u);
    CHECK_EQ(g_lastMark, 0);
    CHECK_EQ(g_lastArg, 2LL);
}

// ===========================================================================
// PART B — `IO::Encoding`: five slots, NO dtor slot, CreateReader at +0x18 and
// CreateWriter at +0x20 with (this, Serializer*, int) arity.
//
// Encoding is the one IO vtable whose slot 0 is NOT a destructor: retail's
// three tables are {IsBinary, IsText, GetKind, CreateReader, CreateWriter},
// and `IO::Stream`'s destructor frees the encoding with a direct
// `MemoryAllocator::MemFree`, never a virtual call. A real `IO::Encoding` must
// therefore declare `IsBinary()` FIRST and declare no destructor slot at all.
// ===========================================================================

class TestSerializerShape;  // fwd — PART C's real Serializer

class TestEncoding {
public:
    // slot 0 +0x00 — retail Binary 0x1800a81e0 `MOV AL,1; RET`
    virtual bool IsBinary() const { return true; }
    // slot 1 +0x08 — retail Binary 0x1800a81d0 `XOR AL,AL; RET`
    virtual bool IsText() const { return false; }
    // slot 2 +0x10 — retail Binary 0x1800a81c0 `MOV EAX,2; RET`
    //                (Text 0x1800b9a90 returns 1, Visitor 0x1800bafc0 returns 3)
    virtual int  GetKind() const { return 2; }
    // slot 3 +0x18 — encoding_vtbl::kCreateReader, retail 0x1800a8120
    virtual void* CreateReader(void* serializer, int version) {
        Note(this, 200, version); return serializer;
    }
    // slot 4 +0x20 — encoding_vtbl::kCreateWriter, retail 0x1800a8080
    virtual void* CreateWriter(void* serializer, int version) {
        Note(this, 201, version); return serializer;
    }
protected:
    ~TestEncoding() = default;   // non-virtual: retail has no dtor slot here
};

class TestBinaryEncoding final : public TestEncoding {
public:
    ~TestBinaryEncoding() = default;
};

UNIT_TEST(encoding_factory_slots_sit_at_0x18_and_0x20_with_arity_three) {
    static_assert(enc_vtbl::kCreateReader == 0x18, "Encoding slot 3");
    static_assert(enc_vtbl::kCreateWriter == 0x20, "Encoding slot 4");

    TestBinaryEncoding enc;
    void* raw = &enc;

    // Exactly the call `RtSerializer_Ctor` makes (serializer.cpp:~310):
    //   using MakeRw = ReaderWriter* (*)(Encoding*, Serializer*, int);
    using MakeRw = void* (*)(void*, void*, int);

    int marker = 0;
    Reset();
    void* r = SlotAt<MakeRw>(raw, enc_vtbl::kCreateReader)(raw, &marker, 11);
    CHECK_EQ(g_lastMark, 200);
    CHECK_EQ(g_lastArg, 11LL);
    CHECK(r == static_cast<void*>(&marker));   // arg 2 survived: arity really is 3
    CHECK(g_lastSelf == static_cast<void*>(&enc));

    Reset();
    void* w = SlotAt<MakeRw>(raw, enc_vtbl::kCreateWriter)(raw, &marker, 12);
    CHECK_EQ(g_lastMark, 201);
    CHECK_EQ(g_lastArg, 12LL);
    CHECK(w == static_cast<void*>(&marker));

    // Slot 0 is IsBinary, NOT a deleting destructor. A real class that put a
    // `DeletingDtor` first here would move both factories one slot down and the
    // Serializer constructor would call GetKind() as a factory.
    using BoolFn = bool (*)(void*);
    CHECK(SlotAt<BoolFn>(raw, 0x00)(raw) == true);    // IsBinary
    CHECK(SlotAt<BoolFn>(raw, 0x08)(raw) == false);   // IsText
    using IntFn = int (*)(void*);
    CHECK_EQ(SlotAt<IntFn>(raw, 0x10)(raw), 2);       // GetKind == Binary
}

// ---------------------------------------------------------------------------
// B.3.4 block 1 landed the REAL classes. `TestEncoding` above stays as the
// executable ABI probe — it can be instantiated here, whereas the real
// subclasses cannot, because their vtables reference `CreateReader`/
// `CreateWriter` bodies that live in runtime/src/io/encoding*.cpp and stand on
// the port registry, which `make unit` does not link. What CAN be checked
// against the shipped header with no linking at all is the SHAPE, and the shape
// is the thing that goes wrong silently.
// ---------------------------------------------------------------------------
UNIT_TEST(the_real_IO_Encoding_classes_have_the_probes_shape) {
    // Same size as the probe, as the POD it replaces, and as retail's
    // `Allocate(8,4)` request in every stream constructor.
    static_assert(sizeof(IO::Encoding) == sizeof(TestEncoding), "probe matches the real base");
    static_assert(sizeof(IO::Encoding) == 8, "one vptr, no fields");
    static_assert(sizeof(IO::BinaryEncoding) == 8, "no subclass adds a field");
    static_assert(sizeof(IO::TextEncoding) == 8, "no subclass adds a field");
    static_assert(sizeof(IO::VisitorEncoding) == 8, "no subclass adds a field");
    static_assert(sizeof(IO::Encoding) == sizeof(rt::io::Encoding),
                  "the POD and the class stay interchangeable during coexistence");

    // The base is abstract (all five slots pure) and the three concretes are
    // not — which is exactly what says every one of the five slots is
    // overridden, with no gap left for the compiler to fill.
    static_assert(std::is_abstract<IO::Encoding>::value, "IO::Encoding is the interface");
    static_assert(!std::is_abstract<IO::BinaryEncoding>::value, "all five slots overridden");
    static_assert(!std::is_abstract<IO::TextEncoding>::value, "all five slots overridden");
    static_assert(!std::is_abstract<IO::VisitorEncoding>::value, "all five slots overridden");

    // THE GUARD THAT MATTERS: no virtual destructor anywhere in the hierarchy.
    // Itanium would spend two slots on one, pushing CreateReader from +0x18 to
    // +0x28 and CreateWriter from +0x20 to +0x30 — and `IO::Serializer`'s
    // constructor, which reads those two byte offsets RAW, would then call
    // `GetKind()` as a factory and store the int 2 as a ReaderWriter*.
    static_assert(!std::has_virtual_destructor<IO::Encoding>::value, "B34_RULES §1.1");
    static_assert(!std::has_virtual_destructor<IO::BinaryEncoding>::value, "B34_RULES §1.1");
    static_assert(!std::has_virtual_destructor<IO::TextEncoding>::value, "B34_RULES §1.1");
    static_assert(!std::has_virtual_destructor<IO::VisitorEncoding>::value, "B34_RULES §1.1");

    // A concrete type destructible at namespace scope is what lets encoding.cpp
    // and encoding_visitor_text.cpp hold one shared instance for their address
    // doors to delegate through with a qualified, non-virtual call.
    static_assert(std::is_destructible<IO::BinaryEncoding>::value, "door instance");
    static_assert(std::is_destructible<IO::TextEncoding>::value, "door instance");
    static_assert(std::is_destructible<IO::VisitorEncoding>::value, "door instance");
    CHECK(true);
}

// ===========================================================================
// PART C — `IO::Serializer`: layout-identical to BOTH legacy spellings, and a
// live round trip through a fake stream reproducing the retail constructor.
// ===========================================================================

class TestSerializer {
public:
    // Non-virtual: retail's `IO::Serializer` has no vtable at all — the 22
    // exported members are all `QEAA`/`QEBA` and tail-jump into the
    // ReaderWriter's. So the public name stays `Serialize` and these ARE real
    // C++ overloads; only the 22 VIRTUALS they land on need distinct names.
    TestSerializer(rt::io::Stream* s, int version) : stream(s), readerWriter(nullptr) {
        // ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z, retail 0x1800b9960.
        rt::io::Encoding* factory = s->encoding;
        const int slot = (s->direction == rt::io::kDirectionReading)
                             ? enc_vtbl::kCreateReader
                             : enc_vtbl::kCreateWriter;
        using MakeRw = void* (*)(void*, void*, int);
        readerWriter = reinterpret_cast<rt::io::ReaderWriter*>(
            SlotAt<MakeRw>(factory, slot)(factory, this, version));
    }
    // ??1Serializer@IO@@QEAA@XZ, retail 0x1800b99b0: slot 0 with flags == 1,
    // and retail does NOT null the field afterwards.
    ~TestSerializer() {
        if (readerWriter) {
            using Fn = void (*)(void*, unsigned);
            SlotAt<Fn>(readerWriter, rw_vtbl::kDtor)(readerWriter, 1u);
        }
    }

    void Serialize(int32_t& v, const char* name) {
        using Fn = void (*)(void*, int32_t*, const char*);
        SlotAt<Fn>(readerWriter, rw_vtbl::kScalarInt)(readerWriter, &v, name);
    }
    void Serialize(bool& v, const char* name) {
        using Fn = void (*)(void*, bool*, const char*);
        SlotAt<Fn>(readerWriter, rw_vtbl::kScalarBool)(readerWriter, &v, name);
    }

    rt::io::Stream*       stream;        // +0x00
    rt::io::ReaderWriter* readerWriter;  // +0x08
};

UNIT_TEST(Serializer_is_layout_identical_to_both_legacy_spellings) {
    static_assert(sizeof(TestSerializer) == 0x10, "IO::Serializer is 16 bytes");
    static_assert(sizeof(rt::io::Serializer) == sizeof(TestSerializer),
                  "rt::io::Serializer (Serializer.h:253) agrees");
    // B.3.4 block 3: the fourth spelling, `RtObject::SerializerLite`, is gone.
    // What stands in its place is the REAL class, which this file now links
    // against (runtime/src/io/serializer_class.cpp) rather than mirroring.
    static_assert(sizeof(IO::Serializer) == sizeof(TestSerializer),
                  "the REAL IO::Serializer agrees with the probe");

    // A non-polymorphic class, so `offsetof` is unconditionally supported here
    // — and it must stay non-polymorphic: a vptr would make it 0x18 bytes and
    // every one of the 861 `g_fn_Serializer_Ctor`-family sites would write
    // through the wrong offsets.
    static_assert(offsetof(TestSerializer, stream)       == 0x00, "Serializer.stream");
    static_assert(offsetof(TestSerializer, readerWriter) == 0x08, "Serializer.readerWriter");
    static_assert(offsetof(rt::io::Serializer, stream)       == 0x00, "rt::io agrees");
    static_assert(offsetof(rt::io::Serializer, readerWriter) == 0x08, "rt::io agrees");
    static_assert(offsetof(IO::Serializer, _stream)       == 0x00, "the real class agrees");
    static_assert(offsetof(IO::Serializer, _readerWriter) == 0x08, "the real class agrees");
    static_assert(std::is_standard_layout<IO::Serializer>::value,
                  "and it is still non-polymorphic, which is what makes those "
                  "two offsets meaningful at all");

    // And therefore the remaining views alias cleanly — which is what lets the
    // 37 `SerializerEncodingVCall` sites and every unconverted `fn_*` caller
    // keep working against a real `IO::Serializer` for the rest of B.3.4 (the
    // 16 `SerializerLite` sites no longer need to: they hold one). The live
    // proof of the aliasing is the round trip in the next test; here it is
    // enough that the compiler agrees on all six offsets and all three sizes.
    static_assert(std::is_standard_layout<TestSerializer>::value,
                  "a Serializer with a vptr would be 0x18 bytes and every "
                  "g_fn_Serializer_Ctor site would write through wrong offsets");
}

UNIT_TEST(a_real_Serializer_round_trips_through_a_fake_stream_and_encoding) {
    // The fake stream: a real `rt::io::Stream` POD whose encoding is a REAL
    // C++ `TestEncoding`. This is precisely the mixed state B.3.4 lives in —
    // an unconverted stream POD handing a converted encoding to a converted
    // serializer, which then drives an object wearing a MINTED ReaderWriter
    // table.
    struct RecordingEncoding final : TestEncoding {
        MintedRwStorage rw{};
        void* CreateReader(void*, int version) override {
            Note(this, 300, version);
            rw.vptr = static_cast<void*>(minted::g_table);
            return &rw;
        }
        void* CreateWriter(void*, int version) override {
            Note(this, 301, version);
            rw.vptr = static_cast<void*>(minted::g_table);
            return &rw;
        }
        ~RecordingEncoding() = default;
    };

    RecordingEncoding enc;
    rt::io::Stream stream{};
    stream.direction = rt::io::kDirectionReading;   // 0 == reading -> CreateReader
    stream.encoding  = reinterpret_cast<rt::io::Encoding*>(static_cast<TestEncoding*>(&enc));

    Reset();
    {
        TestSerializer ser(&stream, 17);
        CHECK_EQ(g_lastMark, 300);                        // the READER factory ran
        CHECK_EQ(g_lastArg, 17LL);                        // version reached it
        CHECK(ser.stream == &stream);
        CHECK(ser.readerWriter == reinterpret_cast<rt::io::ReaderWriter*>(&enc.rw));

        int32_t v = 5;
        Reset(); ser.Serialize(v, "v");
        CHECK_EQ(g_lastMark, 1020);                       // minted ScalarInt
        CHECK_EQ(g_lastArg, 5LL);

        bool b = true;
        Reset(); ser.Serialize(b, "b");                   // the OTHER overload
        CHECK_EQ(g_lastMark, 1015);

        Reset();
    }
    // Leaving scope runs ~Serializer -> ReaderWriter slot 0 with flags == 1.
    CHECK_EQ(g_lastMark, 1001);
    CHECK(g_lastSelf == static_cast<void*>(&enc.rw));

    // Writing direction takes the other factory slot.
    stream.direction = 1;
    Reset();
    {
        TestSerializer ser(&stream, 18);
        CHECK_EQ(g_lastMark, 301);                        // the WRITER factory ran
        CHECK_EQ(g_lastArg, 18LL);
    }
}

// ===========================================================================
// PART D — the Stream family's shape: a 0x18 base, a 0x30 derived with retail's
// field offsets, and a 0x98 class holding a MemoryStream MEMBER at +0x68.
// ===========================================================================

class TestStream {
public:
    // ??_7Stream@IO@@6B@ @ 0x1801549a0, 7 slots. Slot 0 is the deleting dtor
    // (0x18000c610, also RT_DELDTOR(..., "IO::Stream")); slot 1 GetSource has
    // a real body (0x18000c6a0); slots 2..6 are `_purecall` in the base.
    virtual TestStream* DeletingDtor(unsigned flags) { Note(this, 400, flags); return this; }
    virtual const char* GetSource() const            { Note(this, 401); return "stream"; }
    virtual bool        Write(int n, const void*)    { Note(this, 402, n); return true; }
    virtual bool        Read(int n, void*)           { Note(this, 403, n); return true; }
    virtual void        SetCurrentPos(uint32_t p)    { Note(this, 404, p); }
    virtual uint32_t    GetCurrentPos() const        { Note(this, 405); return 0; }
    virtual uint32_t    GetSize() const              { Note(this, 406); return 0; }

    int32_t   direction;   // +0x08
    uint32_t  _pad0c;      // +0x0C  never written by any ctor
    void*     encoding;    // +0x10  IO::Encoding*, owned; freed with MemFree
protected:
    ~TestStream() = default;
};

class TestMemoryStream : public TestStream {
public:
    // ??_7MemoryStream@IO@@6B@ @ 0x1801555b0, 8 slots: the seven above with
    // 2..6 overridden, plus Close at slot 7 (+0x38, retail 0x1800ae9e0).
    // MemoryStream does NOT add PreCommit.
    virtual void Close() { Note(this, 407); }

    void*    data;        // +0x18
    uint8_t  ownsData;    // +0x20
    uint8_t  _pad21[3];
    uint32_t cursor;      // +0x24
    uint32_t size;        // +0x28
    uint32_t _pad2c;

    // PUBLIC, unlike every other dtor in this file, and that is a finding:
    // `IO::CompressedMemoryStream` holds a `MemoryStream` BY VALUE at +0x68, so
    // the enclosing class's implicit destructor has to be able to destroy it.
    // A protected non-virtual `~MemoryStream()` (the shape decision 2 gives a
    // BASE) makes `CompressedMemoryStream` uninstantiable. So `IO::MemoryStream`
    // gets a PUBLIC non-virtual destructor — public, still not virtual.
    ~TestMemoryStream() = default;
};

// `IO::CompressedStream` is 0x68 bytes: Stream + position/activeWindow/source
// and three 0x10-byte `Collection::Array` fields + compression. Modelled here
// only as far as the size, because what PART D has to pin is the MEMBER at
// +0x68, not CompressedStream's own field names.
class TestCompressedStream : public TestStream {
public:
    // 0x180155640, 9 slots: Close at +0x38 (0x1800aa8d0) and PreCommit at
    // +0x40 (0x1800aa8c0) on top of the seven.
    virtual void Close()     { Note(this, 408); }
    virtual void PreCommit() { Note(this, 409); }

    uint32_t _unk18;      // +0x18
    int32_t  position;    // +0x1C
    int32_t  activeWindow;// +0x20
    uint32_t _unk24;      // +0x24
    void*    source;      // +0x28
    uint8_t  arrays[0x30];// +0x30 .. +0x5F  three Collection::Array fields
    uint64_t compression; // +0x60
protected:
    ~TestCompressedStream() = default;
};

class TestCompressedMemoryStream : public TestCompressedStream {
public:
    ~TestCompressedMemoryStream() = default;
    // ??_7CompressedMemoryStream@IO@@6B@ @ 0x180155560, 9 slots, class 0x98
    // bytes. `memoryStream` is a MEMBER, not a base — it has its own vptr.
    TestMemoryStream memoryStream;   // +0x68
};

UNIT_TEST(the_Stream_family_keeps_retail_offsets_as_real_classes) {
    static_assert(sizeof(TestStream) == 0x18, "IO::Stream is 0x18 bytes");
    static_assert(sizeof(TestMemoryStream) == 0x30, "IO::MemoryStream is 0x30 bytes");
    static_assert(sizeof(TestCompressedStream) == 0x68, "IO::CompressedStream is 0x68 bytes");
    static_assert(sizeof(TestCompressedMemoryStream) == 0x98,
                  "IO::CompressedMemoryStream is 0x98 bytes");

    // ... and the POD the tree uses today agrees, field for field.
    static_assert(sizeof(RtIo::Stream) == sizeof(TestStream), "RtIo::Stream agrees");
    static_assert(sizeof(RtIo::MemoryStream) == sizeof(TestMemoryStream),
                  "RtIo::MemoryStream agrees");
    static_assert(sizeof(rt::io::Stream) == sizeof(TestStream),
                  "the DUPLICATE POD rt::io::Stream agrees too (Serializer.h:234 vs "
                  "Stream.h:70 — B.3.4 collapses the pair)");

    TestCompressedMemoryStream probe;

    TestStream* s = static_cast<TestStream*>(&probe);
    CHECK_EQ(FieldOffset(s, &s->direction), (uintptr_t)0x08);
    CHECK_EQ(FieldOffset(s, &s->encoding),  (uintptr_t)0x10);

    TestMemoryStream* ms = &probe.memoryStream;
    CHECK_EQ(FieldOffset(ms, &ms->data),     (uintptr_t)0x18);
    CHECK_EQ(FieldOffset(ms, &ms->ownsData), (uintptr_t)0x20);
    CHECK_EQ(FieldOffset(ms, &ms->cursor),   (uintptr_t)0x24);
    CHECK_EQ(FieldOffset(ms, &ms->size),     (uintptr_t)0x28);

    // The member — not a base — at +0x68.
    TestCompressedStream* cs = static_cast<TestCompressedStream*>(&probe);
    CHECK_EQ(FieldOffset(cs, static_cast<TestStream*>(ms)), (uintptr_t)0x68);

    // The member carries its OWN vptr, so a raw slot call on it dispatches to
    // MemoryStream's table and not to the enclosing class's. This is what makes
    // `CompressedMemoryStream` drive its buffer through the same 8-slot table
    // an independent `MemoryStream` wears.
    void* rawMs = static_cast<void*>(static_cast<TestStream*>(ms));
    void* rawCs = static_cast<void*>(cs);
    CHECK(*reinterpret_cast<void**>(rawMs) != *reinterpret_cast<void**>(rawCs));

    // Slot 7 (+0x38) is Close on the MemoryStream branch...
    Reset();
    using VoidFn = void (*)(void*);
    SlotAt<VoidFn>(rawMs, 0x38)(rawMs);
    CHECK_EQ(g_lastMark, 407);
    // ...and Close on the Compressed branch too, with PreCommit at +0x40.
    Reset(); SlotAt<VoidFn>(rawCs, 0x38)(rawCs); CHECK_EQ(g_lastMark, 408);
    Reset(); SlotAt<VoidFn>(rawCs, 0x40)(rawCs); CHECK_EQ(g_lastMark, 409);

    // Slot 1 (GetSource) is where `??_7FileStream@` and `??_7BinaryFileStream@`
    // are byte-identical in retail, which is the whole reason
    // `file_stream.cpp`'s `NoteSlotDispatch` vptr counter exists. Two real
    // classes that do not override it share the compiler's entry as well:
    CHECK(SlotAt<void*>(rawMs, 0x08) == SlotAt<void*>(rawCs, 0x08));
}

UNIT_TEST(a_derived_stream_is_reachable_through_the_base_slot_indices) {
    // Direction A again, for the stream family: an unconverted C body holding a
    // `void*` and calling slot 3 (Read, +0x18) must reach the most-derived
    // override with `this` unadjusted, whichever concrete stream it is.
    struct Probe final : TestMemoryStream {
        ~Probe() = default;
        bool Read(int n, void*) override { Note(this, 500, n); return true; }
    };
    Probe p;
    void* raw = static_cast<void*>(static_cast<TestStream*>(&p));

    uint8_t buf[4] = {0};
    Reset();
    using ReadFn = bool (*)(void*, int, void*);
    CHECK(SlotAt<ReadFn>(raw, 0x18)(raw, 4, buf) == true);
    CHECK_EQ(g_lastMark, 500);
    CHECK_EQ(g_lastArg, 4LL);
    CHECK(g_lastSelf == raw);   // single inheritance: no this-adjustment
}

// ===========================================================================
// PART E — ENDGAME B.3.4 block 3. The REAL `IO::Serializer`'s LIFETIME.
//
// THIS DRIVES THE REAL CLASS, NOT A MIRROR. `TestSerializer` in PART C is a
// transcription, and a transcription can only ever test itself: it would keep
// passing while the shipped constructor drifted. `IO::Serializer`'s member
// bodies therefore live in a REGISTRY-FREE translation unit
// (runtime/src/io/serializer_class.cpp, in tests/unit/sources.mk) so that this
// file links the shipped code, exactly as B.3.3 did with type_reflection.cpp.
// PART C stays as the ABI probe; this is the behavioural one.
//
// WHAT IT PINS, and why each clause is load-bearing:
//
//   * Constructing consumes (reading) or emits (writing) a 12-BYTE STREAM
//     HEADER — `??0BinaryReader@IO@@` (0x1800a8340) calls the stream's Read
//     three times with count 4 and GetCurrentPos once; `??0BinaryWriter@IO@@`
//     (0x1800a8a30) calls Write three times with count 4 and GetCurrentPos
//     twice. EXACTLY ONCE each, on exactly one of the two factory slots.
//   * Destroying reads or back-patches FOUR MORE BYTES through the
//     ReaderWriter's slot 0 with flags == 1. EXACTLY ONCE.
//   * So `~Serializer` IS NOT IDEMPOTENT and construction is not free. The
//     double-construction hazard is asserted as a COUNTED FACT below rather
//     than left as a comment: two constructions into one buffer really do
//     consume two headers, and the file is desynced from that point on.
//
// The caller shape mirrored here is B34_RULES §7.3's: placement-new into an
// `alignas(8) uint8_t buf[24]` with an explicit destructor at the same
// position. That is the shape the 187 exe-side ctor sites convert to, the shape
// runtime/src/object/internal_class.cpp's `ResetPersistState` now uses, and the
// shape the registered `RtSerializer_Ctor` door itself uses.
// ===========================================================================

namespace lifetime {

struct Counters {
    int readCalls, readBytes;
    int writeCalls, writeBytes;
    int getPosCalls, setPosCalls;
    int createReader, createWriter;
    int tailReads, tailWrites, deletingDtors;
    int lastVersion;
};
Counters g_c{};
void ResetCounters() { g_c = Counters{}; }

// The stream. Slot order is `??_7Stream@IO@@6B@`'s: [2] Write +0x10,
// [3] Read +0x18, [4] SetCurrentPos +0x20, [5] GetCurrentPos +0x28. The
// Serializer's own constructor never calls a stream slot — it only reads the
// POD fields — so every count below is the ReaderWriter's doing, which is
// precisely the thing whose position must not move.
struct CountingStream final : TestStream {
    ~CountingStream() = default;
    bool Write(int n, const void*) override { ++g_c.writeCalls; g_c.writeBytes += n; return true; }
    bool Read (int n, void*)       override { ++g_c.readCalls;  g_c.readBytes  += n; return true; }
    void SetCurrentPos(uint32_t)   override { ++g_c.setPosCalls; }
    uint32_t GetCurrentPos() const override { ++g_c.getPosCalls; return 0; }
};

// A CONCRETE subclass of the REAL `IO::ReaderWriter` — every one of the 26
// slots overridden, because the base declares all 26 pure. Only slot 0 has
// behaviour here: it is the 4-byte tail that `~Serializer` triggers.
struct TailReaderWriter final : IO::ReaderWriter {
    CountingStream* src = nullptr;
    bool writing = false;

    IO::ReaderWriter* DeletingDtor(unsigned flags) override {
        ++g_c.deletingDtors;
        CHECK_EQ((int)flags, 1);          // retail's `MOV EDX,1`
        if (writing) { src->Write(4, nullptr); ++g_c.tailWrites; }
        else         { src->Read (4, nullptr); ++g_c.tailReads;  }
        return this;
    }
    int  GetVersion() const override { return 0; }
    void Skip() override {}
    void SerializeCharArray  (int, char*,          const char*) override {}
    void SerializeBoolArray  (int, bool*,          const char*) override {}
    void SerializeFloatArray (int, float*,         const char*) override {}
    void SerializeUInt64Array(int, uint64_t*,      const char*) override {}
    void SerializeInt64Array (int, int64_t*,       const char*) override {}
    void SerializeUIntArray  (int, uint32_t*,      const char*) override {}
    void SerializeIntArray   (int, int32_t*,       const char*) override {}
    void SerializeUShortArray(int, uint16_t*,      const char*) override {}
    void SerializeShortArray (int, int16_t*,       const char*) override {}
    void SerializeUCharArray (int, unsigned char*, const char*) override {}
    void SerializeSCharArray (int, signed char*,   const char*) override {}
    void SerializeChar  (char&,          const char*) override {}
    void SerializeBool  (bool&,          const char*) override {}
    void SerializeFloat (float&,         const char*) override {}
    void SerializeUInt64(uint64_t&,      const char*) override {}
    void SerializeInt64 (int64_t&,       const char*) override {}
    void SerializeUInt  (uint32_t&,      const char*) override {}
    void SerializeInt   (int32_t&,       const char*) override {}
    void SerializeUShort(uint16_t&,      const char*) override {}
    void SerializeShort (int16_t&,       const char*) override {}
    void SerializeUChar (unsigned char&, const char*) override {}
    void SerializeSChar (signed char&,   const char*) override {}
    void SerializeBytes(int, void*, const char*) override {}
    ~TailReaderWriter() = default;
};

// A CONCRETE subclass of the REAL `IO::Encoding`, whose two factory bodies
// reproduce the retail codec constructors' STREAM TRAFFIC and nothing else.
// Three fixed ReaderWriters so a second construction gets its own.
struct HeaderEncoding final : IO::Encoding {
    CountingStream* src = nullptr;
    TailReaderWriter rw[3];
    int handed = 0;

    bool IsBinary() const override { return true; }
    bool IsText() const override { return false; }
    int  GetKind() const override { return 2; }

    IO::ReaderWriter* CreateReader(IO::Serializer&, int version) override {
        ++g_c.createReader; g_c.lastVersion = version;
        src->Read(4, nullptr); src->Read(4, nullptr); src->Read(4, nullptr);  // 12 bytes
        src->GetCurrentPos();
        TailReaderWriter* r = &rw[handed++ % 3];
        r->src = src; r->writing = false;
        return r;
    }
    IO::ReaderWriter* CreateWriter(IO::Serializer&, int version) override {
        ++g_c.createWriter; g_c.lastVersion = version;
        src->Write(4, nullptr); src->Write(4, nullptr); src->Write(4, nullptr); // 12 bytes
        src->GetCurrentPos(); src->GetCurrentPos();
        TailReaderWriter* w = &rw[handed++ % 3];
        w->src = src; w->writing = true;
        return w;
    }
    ~HeaderEncoding() = default;
};

// Wire a `CountingStream` up as the POD the (still unconverted) `IO::Stream`
// is today, and hand it out as the class the constructor's signature names.
// This pair of casts is precisely what block 4 deletes.
IO::Stream& AsIoStream(CountingStream& cs, HeaderEncoding& enc, int32_t direction) {
    TestStream* base = static_cast<TestStream*>(&cs);
    base->direction = direction;
    base->encoding  = static_cast<void*>(static_cast<IO::Encoding*>(&enc));
    enc.src = &cs;
    return *reinterpret_cast<IO::Stream*>(base);
}

}  // namespace lifetime

UNIT_TEST(the_real_Serializer_consumes_one_12_byte_header_and_one_4_byte_tail) {
    using namespace lifetime;

    // ---- READING: direction 0 takes CreateReader (encoding slot 3, +0x18) --
    {
        CountingStream cs;
        HeaderEncoding enc;
        IO::Stream& s = AsIoStream(cs, enc, rt::io::kDirectionReading);

        ResetCounters();
        alignas(8) uint8_t buf[24];      // the exe-side caller shape, §7.3
        IO::Serializer* ser = ::new (static_cast<void*>(buf)) IO::Serializer(s, 17);

        // Exactly ONE factory call, and it was the READ one.
        CHECK_EQ(g_c.createReader, 1);
        CHECK_EQ(g_c.createWriter, 0);
        CHECK_EQ(g_c.lastVersion, 17);   // arg 3 reached the factory
        // Exactly TWELVE bytes, in three 4-byte reads, and nothing written.
        CHECK_EQ(g_c.readCalls, 3);
        CHECK_EQ(g_c.readBytes, 12);
        CHECK_EQ(g_c.getPosCalls, 1);
        CHECK_EQ(g_c.writeCalls, 0);
        // The object is wired the way retail wires it.
        CHECK(ser->_stream == &s);
        CHECK(ser->_readerWriter == static_cast<IO::ReaderWriter*>(&enc.rw[0]));
        CHECK(static_cast<void*>(ser) == static_cast<void*>(buf));

        // ---- the tail, on destruction, exactly once --------------------
        ser->~Serializer();
        CHECK_EQ(g_c.deletingDtors, 1);
        CHECK_EQ(g_c.tailReads, 1);
        CHECK_EQ(g_c.readCalls, 4);      // 3 header + 1 tail
        CHECK_EQ(g_c.readBytes, 16);     // 12 + 4
        CHECK_EQ(g_c.tailWrites, 0);
    }

    // ---- WRITING: any non-zero direction takes CreateWriter (+0x20) --------
    {
        CountingStream cs;
        HeaderEncoding enc;
        IO::Stream& s = AsIoStream(cs, enc, 1);

        ResetCounters();
        alignas(8) uint8_t buf[24];
        IO::Serializer* ser = ::new (static_cast<void*>(buf)) IO::Serializer(s, 18);

        CHECK_EQ(g_c.createWriter, 1);
        CHECK_EQ(g_c.createReader, 0);
        CHECK_EQ(g_c.lastVersion, 18);
        CHECK_EQ(g_c.writeCalls, 3);
        CHECK_EQ(g_c.writeBytes, 12);
        CHECK_EQ(g_c.getPosCalls, 2);    // the writer records a back-patch pos
        CHECK_EQ(g_c.readCalls, 0);

        ser->~Serializer();
        CHECK_EQ(g_c.deletingDtors, 1);
        CHECK_EQ(g_c.tailWrites, 1);
        CHECK_EQ(g_c.writeCalls, 4);
        CHECK_EQ(g_c.writeBytes, 16);
    }
}

UNIT_TEST(constructing_a_Serializer_twice_consumes_two_headers) {
    using namespace lifetime;

    // THE HAZARD, AS A COUNTED FACT rather than a comment in a rule sheet.
    // This is why §7.3 forbids `x.~Serializer()` on a TYPED local (the implicit
    // destructor at scope exit would be a second one) and why the caller
    // conversion is placement-new into a RAW buffer, which has no implicit
    // destructor of its own. Nothing here is a bug being demonstrated — it is
    // the measurement that says a second construction is not free, so a
    // conversion that accidentally doubles one shifts every record after it.
    CountingStream cs;
    HeaderEncoding enc;
    IO::Stream& s = AsIoStream(cs, enc, rt::io::kDirectionReading);

    ResetCounters();
    alignas(8) uint8_t buf[24];

    IO::Serializer* a = ::new (static_cast<void*>(buf)) IO::Serializer(s, 3);
    CHECK_EQ(g_c.readBytes, 12);

    // Same bytes, second construction. The stream has now given up 24.
    IO::Serializer* b = ::new (static_cast<void*>(buf)) IO::Serializer(s, 3);
    CHECK_EQ(g_c.createReader, 2);
    CHECK_EQ(g_c.readCalls, 6);
    CHECK_EQ(g_c.readBytes, 24);      // <- the desync, measured
    CHECK(a == b);                    // into the same storage, at one address

    // The first ReaderWriter is now unreachable and its 4-byte tail will never
    // be consumed: one destruction cannot undo two constructions. Faithful, and
    // exactly why the destructor is not idempotent.
    b->~Serializer();
    CHECK_EQ(g_c.deletingDtors, 1);
    CHECK_EQ(g_c.readBytes, 28);      // 24 + one tail, NOT 24 + two
}

// ===========================================================================
// PART F — ENDGAME B.3.4 block 3. PIN THE REAL CLASSES' VTABLE ORDER.
//
// WHY THIS TEST HAS TO EXIST. Nothing else in the tree exercises the real
// `IO::ReaderWriter`/`IO::Encoding` vtable ORDER: every registered door calls
// qualified or non-virtually, and every live object wears a MINTED table out of
// `standalone/rt_data_mint_tables.inc`. So a mis-ordered declaration in
// runtime/types/io/Serializer.h would stay invisible right through B.3.4 and
// surface only when B.3 steps 5-8 retire the minted tables — as a corrupted
// save file, with no recent change to blame. The order was checked by eye at
// the block-2 close and is right. This is so it STAYS right.
//
// PARTS A and B probe TRANSCRIPTIONS (`TestReaderWriter`, `TestEncoding`),
// which prove the ABI rule but not the shipped declaration. These probe the
// SHIPPED classes: a concrete subclass of each, every slot overridden with its
// own marker, then a raw call at every `rw_vtbl::k*` / `encoding_vtbl::k*` byte
// offset asserting that the marker equals offset/8.
//
// Two compiler facts, so the next reader does not fight them:
//   * `IO::ReaderWriter`'s destructor is protected, so the probe must be a
//     SUBCLASS (it is) and must give itself a public one.
//   * `IO::BinaryReader`/`BinaryWriter`/`Visitor` are `final` AND abstract
//     (slot 0 is left pure; its body is registered by address in
//     iobase_vtables.cpp), so they can be neither subclassed nor instantiated.
//     The BASE is the right thing to probe anyway: it is the table whose order
//     the 54 `rw_vtbl::` files depend on.
// ===========================================================================

namespace order {

struct OrderProbe final : IO::ReaderWriter {
    IO::ReaderWriter* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    int  GetVersion() const override { Note(this, 1); return 0; }
    void Skip() override { Note(this, 2); }
    void SerializeCharArray  (int, char*,          const char*) override { Note(this, 3); }
    void SerializeBoolArray  (int, bool*,          const char*) override { Note(this, 4); }
    void SerializeFloatArray (int, float*,         const char*) override { Note(this, 5); }
    void SerializeUInt64Array(int, uint64_t*,      const char*) override { Note(this, 6); }
    void SerializeInt64Array (int, int64_t*,       const char*) override { Note(this, 7); }
    void SerializeUIntArray  (int, uint32_t*,      const char*) override { Note(this, 8); }
    void SerializeIntArray   (int, int32_t*,       const char*) override { Note(this, 9); }
    void SerializeUShortArray(int, uint16_t*,      const char*) override { Note(this, 10); }
    void SerializeShortArray (int, int16_t*,       const char*) override { Note(this, 11); }
    void SerializeUCharArray (int, unsigned char*, const char*) override { Note(this, 12); }
    void SerializeSCharArray (int, signed char*,   const char*) override { Note(this, 13); }
    void SerializeChar  (char&,          const char*) override { Note(this, 14); }
    void SerializeBool  (bool&,          const char*) override { Note(this, 15); }
    void SerializeFloat (float&,         const char*) override { Note(this, 16); }
    void SerializeUInt64(uint64_t&,      const char*) override { Note(this, 17); }
    void SerializeInt64 (int64_t&,       const char*) override { Note(this, 18); }
    void SerializeUInt  (uint32_t&,      const char*) override { Note(this, 19); }
    void SerializeInt   (int32_t&,       const char*) override { Note(this, 20); }
    void SerializeUShort(uint16_t&,      const char*) override { Note(this, 21); }
    void SerializeShort (int16_t&,       const char*) override { Note(this, 22); }
    void SerializeUChar (unsigned char&, const char*) override { Note(this, 23); }
    void SerializeSChar (signed char&,   const char*) override { Note(this, 24); }
    void SerializeBytes(int, void*, const char*) override { Note(this, 25); }
    ~OrderProbe() = default;          // public: the base's is protected
};

struct EncOrderProbe final : IO::Encoding {
    bool IsBinary() const override { Note(this, 0); return true; }
    bool IsText() const override { Note(this, 1); return false; }
    int  GetKind() const override { Note(this, 2); return 2; }
    IO::ReaderWriter* CreateReader(IO::Serializer&, int) override { Note(this, 3); return nullptr; }
    IO::ReaderWriter* CreateWriter(IO::Serializer&, int) override { Note(this, 4); return nullptr; }
    ~EncOrderProbe() = default;
};

}  // namespace order

UNIT_TEST(the_real_ReaderWriter_declares_its_26_slots_in_retail_order) {
    order::OrderProbe p;
    void* raw = static_cast<void*>(static_cast<IO::ReaderWriter*>(&p));

    // The three singular shapes first.
    Reset();
    using DtorFn = void* (*)(void*, unsigned);
    SlotAt<DtorFn>(raw, rw_vtbl::kDtor)(raw, 1u);
    CHECK_EQ(g_lastMark, rw_vtbl::kDtor / 8);
    CHECK(g_lastSelf == raw);                       // `this` unadjusted

    Reset();
    using IntFn = int (*)(void*);
    SlotAt<IntFn>(raw, rw_vtbl::kGetVersion)(raw);
    CHECK_EQ(g_lastMark, rw_vtbl::kGetVersion / 8);

    Reset();
    using VoidFn = void (*)(void*);
    SlotAt<VoidFn>(raw, rw_vtbl::kSkip)(raw);
    CHECK_EQ(g_lastMark, rw_vtbl::kSkip / 8);

    // The eleven ARRAY slots, (int, T*, const char*) — one shape, eleven
    // offsets, each asserted to land on the override declared at that index.
    const int arrayOffsets[11] = {
        rw_vtbl::kArrayChar,   rw_vtbl::kArrayBool,   rw_vtbl::kArrayFloat,
        rw_vtbl::kArrayUInt64, rw_vtbl::kArrayInt64,  rw_vtbl::kArrayUInt,
        rw_vtbl::kArrayInt,    rw_vtbl::kArrayUShort, rw_vtbl::kArrayShort,
        rw_vtbl::kArrayUChar,  rw_vtbl::kArraySChar,
    };
    uint8_t scratch[8] = {0};
    using ArrFn = void (*)(void*, int, void*, const char*);
    for (int i = 0; i < 11; ++i) {
        Reset();
        SlotAt<ArrFn>(raw, arrayOffsets[i])(raw, 1, scratch, "n");
        CHECK_EQ(g_lastMark, arrayOffsets[i] / 8);
        CHECK_EQ(g_lastMark, 3 + i);          // contiguous, slots 3..13
    }

    // The eleven SCALAR slots, (T&, const char*).
    const int scalarOffsets[11] = {
        rw_vtbl::kScalarChar,   rw_vtbl::kScalarBool,   rw_vtbl::kScalarFloat,
        rw_vtbl::kScalarUInt64, rw_vtbl::kScalarInt64,  rw_vtbl::kScalarUInt,
        rw_vtbl::kScalarInt,    rw_vtbl::kScalarUShort, rw_vtbl::kScalarShort,
        rw_vtbl::kScalarUChar,  rw_vtbl::kScalarSChar,
    };
    using ScaFn = void (*)(void*, void*, const char*);
    for (int i = 0; i < 11; ++i) {
        Reset();
        SlotAt<ScaFn>(raw, scalarOffsets[i])(raw, scratch, "n");
        CHECK_EQ(g_lastMark, scalarOffsets[i] / 8);
        CHECK_EQ(g_lastMark, 14 + i);         // contiguous, slots 14..24
    }

    Reset();
    SlotAt<ArrFn>(raw, rw_vtbl::kSerializeBytes)(raw, 8, scratch, "b");
    CHECK_EQ(g_lastMark, rw_vtbl::kSerializeBytes / 8);
    CHECK_EQ(g_lastMark, 25);                 // the last slot in the table

    // 26 distinct entries, on the REAL class this time — no slot shared, and
    // in particular no pair of same-named virtuals collapsed into one.
    void* const* vt = *reinterpret_cast<void* const* const*>(raw);
    for (int a = 0; a < 26; ++a)
        for (int b = a + 1; b < 26; ++b)
            CHECK(vt[a] != vt[b]);

    // No virtual destructor anywhere in the shipped hierarchy: one would spend
    // two Itanium slots and mis-aim every offset above by one and a half.
    static_assert(!std::has_virtual_destructor<IO::ReaderWriter>::value, "B34_RULES 1.2.3");
    static_assert(!std::has_virtual_destructor<IO::BinaryReader>::value, "B34_RULES 1.2.3");
    static_assert(!std::has_virtual_destructor<IO::BinaryWriter>::value, "B34_RULES 1.2.3");
    static_assert(!std::has_virtual_destructor<IO::Visitor>::value,      "B34_RULES 1.2.3");
}

UNIT_TEST(the_real_Encoding_declares_its_5_slots_in_retail_order) {
    order::EncOrderProbe e;
    void* raw = static_cast<void*>(static_cast<IO::Encoding*>(&e));

    // Slot 0 is `IsBinary`, NOT a destructor. This is the assertion that keeps
    // `IO::Serializer`'s constructor calling a FACTORY at +0x18/+0x20 rather
    // than `GetKind()` — which would build-clean, link-clean store the int 2 as
    // a ReaderWriter* and corrupt every save.
    using BoolFn = bool (*)(void*);
    using IntFn  = int (*)(void*);
    Reset(); CHECK(SlotAt<BoolFn>(raw, 0x00)(raw) == true);  CHECK_EQ(g_lastMark, 0);
    Reset(); CHECK(SlotAt<BoolFn>(raw, 0x08)(raw) == false); CHECK_EQ(g_lastMark, 1);
    Reset(); CHECK_EQ(SlotAt<IntFn>(raw, 0x10)(raw), 2);     CHECK_EQ(g_lastMark, 2);

    // The two factories, at the byte offsets the Serializer constructor reads
    // RAW, with retail's arity 3.
    int marker = 0;
    using MakeRw = void* (*)(void*, void*, int);
    Reset();
    CHECK(SlotAt<MakeRw>(raw, enc_vtbl::kCreateReader)(raw, &marker, 11) == nullptr);
    CHECK_EQ(g_lastMark, enc_vtbl::kCreateReader / 8);
    CHECK_EQ(g_lastMark, 3);
    Reset();
    CHECK(SlotAt<MakeRw>(raw, enc_vtbl::kCreateWriter)(raw, &marker, 12) == nullptr);
    CHECK_EQ(g_lastMark, enc_vtbl::kCreateWriter / 8);
    CHECK_EQ(g_lastMark, 4);

    // Five distinct entries and no more: the table is bounded at 5 in retail by
    // the adjacency ladder (0x1801554a8 + 0x28 == 0x1801554d0).
    void* const* vt = *reinterpret_cast<void* const* const*>(raw);
    for (int a = 0; a < 5; ++a)
        for (int b = a + 1; b < 5; ++b)
            CHECK(vt[a] != vt[b]);

    static_assert(!std::has_virtual_destructor<IO::Encoding>::value, "B34_RULES 1.1");
}

// ===========================================================================
// PART G — ENDGAME B.3.4 block 4 (STREAMS). PIN THE REAL STREAM CLASSES'
// VTABLE ORDER, and their layout against BOTH duplicate PODs.
//
// Same argument as PART F, one family over. Every live stream in this build
// wears a MINTED table out of `standalone/rt_data_mint_tables.inc` and every
// registered door calls QUALIFIED, so nothing else compiles a real
// `IO::Stream` vtable at all. A slot declared one line out of place in
// runtime/types/io/Stream.h or FileStream.h would stay invisible until B.3
// steps 5-8 retire the minted rows, and then surface as a stream that reads
// through `Close` or seeks through `Open`.
//
// HOW SLOT 1 IS PINNED WITHOUT CALLING IT. `GetSource` returns a
// `System::String` BY VALUE, so it carries a hidden return pointer, and the
// registered door (`RtIo_Stream_GetSource`, iobase_vtables.cpp) spells that
// slot in MSVC's argument order while a C++ member compiled here uses the
// compiler's. Raw-calling it from the test would be asserting a calling
// convention, not a slot order. So slot 1 is pinned BY ELIMINATION instead:
// every OTHER slot is raw-called and lands on its own marker, and all the
// entries in the table are asserted distinct. With n distinct entries and n-1
// of them pinned, the remaining one is at the remaining index. That is a
// complete proof of the order and it depends on no convention at all.
// ===========================================================================

namespace streamorder {

// Byte offsets, spelled once. `IO::Stream` is 7 slots; the concrete classes
// append theirs (B34_RULES section 1.3).
constexpr int kDtor          = 0x00;
constexpr int kGetSource     = 0x08;
constexpr int kWrite         = 0x10;
constexpr int kRead          = 0x18;
constexpr int kSetCurrentPos = 0x20;
constexpr int kGetCurrentPos = 0x28;
constexpr int kGetSize       = 0x30;
constexpr int kSlot7         = 0x38;   // Close on MemoryStream, Open on FileStream

struct StreamProbe final : IO::Stream {
    IO::Stream* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    System::String GetSource() const override { Note(this, 1); return System::String(); }
    bool     Write(int, const void*) override  { Note(this, 2); return true; }
    bool     Read(int, void*) override         { Note(this, 3); return true; }
    bool     SetCurrentPos(int) override       { Note(this, 4); return true; }
    uint32_t GetCurrentPos() const override    { Note(this, 5); return 0; }
    uint32_t GetSize() const override          { Note(this, 6); return 0; }
    ~StreamProbe() = default;          // public: the base's is protected
};

struct MemoryStreamProbe final : IO::MemoryStream {
    IO::Stream* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    System::String GetSource() const override { Note(this, 1); return System::String(); }
    bool     Write(int, const void*) override  { Note(this, 2); return true; }
    bool     Read(int, void*) override         { Note(this, 3); return true; }
    bool     SetCurrentPos(int) override       { Note(this, 4); return true; }
    uint32_t GetCurrentPos() const override    { Note(this, 5); return 0; }
    uint32_t GetSize() const override          { Note(this, 6); return 0; }
    void     Close() override                  { Note(this, 7); }
    ~MemoryStreamProbe() = default;
};

// The six slots every Stream in the family shares, raw-called at their retail
// byte offsets. `raw` is the object; each CHECK says the entry at that offset
// is the override declared at offset/8, with `this` unadjusted.
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

// n distinct entries — which is what turns "every slot but one is pinned" into
// "slot 1 is `GetSource`".
void CheckDistinctSlots(void* raw, int n) {
    void* const* vt = *reinterpret_cast<void* const* const*>(raw);
    for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b)
            CHECK(vt[a] != vt[b]);
}

}  // namespace streamorder

UNIT_TEST(the_real_Stream_declares_its_7_slots_in_retail_order) {
    streamorder::StreamProbe p;
    void* raw = static_cast<void*>(static_cast<IO::Stream*>(&p));

    streamorder::CheckSharedStreamSlots(raw);
    streamorder::CheckDistinctSlots(raw, 7);

    // Slot 1 by elimination (see the banner), and confirmed to be a virtual at
    // all by an ordinary C++ call through the base.
    Reset();
    { const IO::Stream& s = p; (void)s.GetSource(); }
    CHECK_EQ(g_lastMark, streamorder::kGetSource / 8);

    // No virtual destructor: Itanium spends TWO slots on one, which would push
    // `Write` out of slot 2 and mis-aim every raw caller in the tree.
    static_assert(!std::has_virtual_destructor<IO::Stream>::value,
                  "IO::Stream must not acquire a virtual destructor");
    static_assert(!std::has_virtual_destructor<IO::MemoryStream>::value,
                  "IO::MemoryStream must not acquire a virtual destructor");
}

UNIT_TEST(the_real_MemoryStream_adds_Close_at_slot_7_and_no_PreCommit) {
    streamorder::MemoryStreamProbe p;
    void* raw = static_cast<void*>(static_cast<IO::Stream*>(&p));

    streamorder::CheckSharedStreamSlots(raw);

    // Slot 7 is `Close` (0x1800ae9e0) and MemoryStream adds NOTHING after it —
    // the nine-slot shape with `PreCommit` at +0x40 belongs to the Compressed
    // branch. The same +0x38 means `Open` on the FileStream branch.
    Reset();
    using VoidFn = void (*)(void*);
    SlotAt<VoidFn>(raw, streamorder::kSlot7)(raw);
    CHECK_EQ(g_lastMark, streamorder::kSlot7 / 8);
    CHECK_EQ(g_lastMark, 7);

    streamorder::CheckDistinctSlots(raw, 8);

    Reset();
    { const IO::Stream& s = p; (void)s.GetSource(); }
    CHECK_EQ(g_lastMark, 1);

    // `this` is unadjusted through the single-inheritance chain: the raw slot
    // callers all over runtime/src hand the same pointer to base and derived.
    CHECK(static_cast<void*>(static_cast<IO::Stream*>(&p)) ==
          static_cast<void*>(static_cast<IO::MemoryStream*>(&p)));
}

UNIT_TEST(the_real_Stream_family_is_layout_identical_to_both_duplicate_PODs) {
    // B34_RULES section 1.5.2: there are TWO `Stream` PODs, declared either
    // side of the Wave-4 lane boundary in two namespaces, and neither header
    // includes the other. `IO::Stream` replaces both, and both stay alive and
    // trivially destructible until their last user converts. If any of the
    // three ever drifts, this fails here rather than in a save file.
    static_assert(sizeof(IO::Stream) == 0x18, "IO::Stream is 0x18");
    static_assert(sizeof(IO::Stream) == sizeof(RtIo::Stream), "RtIo POD agrees");
    static_assert(sizeof(IO::Stream) == sizeof(rt::io::Stream), "rt::io POD agrees");
    static_assert(sizeof(IO::MemoryStream) == 0x30, "IO::MemoryStream is 0x30");
    static_assert(sizeof(IO::MemoryStream) == sizeof(RtIo::MemoryStream), "POD agrees");

    CHECK_EQ((int)offsetof(IO::Stream, _direction), 0x08);
    CHECK_EQ((int)offsetof(IO::Stream, _encoding),  0x10);
    CHECK_EQ((int)offsetof(IO::MemoryStream, _data),     0x18);
    CHECK_EQ((int)offsetof(IO::MemoryStream, _ownsData), 0x20);
    CHECK_EQ((int)offsetof(IO::MemoryStream, _cursor),   0x24);
    CHECK_EQ((int)offsetof(IO::MemoryStream, _size),     0x28);

    // A field write through the CLASS is visible through BOTH PODs at the same
    // bytes — the property every `reinterpret_cast` in runtime/src/io relies on.
    streamorder::MemoryStreamProbe p;
    p._cursor = 0x1234;
    p._size   = 0x5678;
    p._direction = 1;
    const RtIo::MemoryStream* a =
        reinterpret_cast<const RtIo::MemoryStream*>(static_cast<IO::MemoryStream*>(&p));
    const rt::io::Stream* b =
        reinterpret_cast<const rt::io::Stream*>(static_cast<IO::Stream*>(&p));
    CHECK_EQ((int)a->cursor, 0x1234);
    CHECK_EQ((int)a->size,   0x5678);
    CHECK_EQ((int)a->base.direction, 1);
    CHECK_EQ((int)b->direction, 1);
}

// ---------------------------------------------------------------------------
// The FileStream branch, where slot 7 means something ELSE.
// ---------------------------------------------------------------------------
namespace streamorder {

struct FileStreamProbe final : IO::FileStream {
    IO::Stream* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    System::String GetSource() const override { Note(this, 1); return System::String(); }
    bool     Write(int, const void*) override  { Note(this, 2); return true; }
    bool     Read(int, void*) override         { Note(this, 3); return true; }
    bool     SetCurrentPos(int) override       { Note(this, 4); return true; }
    uint32_t GetCurrentPos() const override    { Note(this, 5); return 0; }
    uint32_t GetSize() const override          { Note(this, 6); return 0; }
    bool     Open(const System::Filename&) override { Note(this, 7); return true; }
    ~FileStreamProbe() = default;
};

// BinaryFileStream differs from FileStream in slot 0 ONLY, so probing it is
// probing that claim: the same eight overrides must land at the same eight
// offsets through a table one class further down.
struct BinaryFileStreamProbe final : IO::BinaryFileStream {
    IO::Stream* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    System::String GetSource() const override { Note(this, 1); return System::String(); }
    bool     Write(int, const void*) override  { Note(this, 2); return true; }
    bool     Read(int, void*) override         { Note(this, 3); return true; }
    bool     SetCurrentPos(int) override       { Note(this, 4); return true; }
    uint32_t GetCurrentPos() const override    { Note(this, 5); return 0; }
    uint32_t GetSize() const override          { Note(this, 6); return 0; }
    bool     Open(const System::Filename&) override { Note(this, 7); return true; }
    ~BinaryFileStreamProbe() = default;
};

}  // namespace streamorder

UNIT_TEST(the_real_FileStream_puts_Open_at_slot_7_not_Close) {
    streamorder::FileStreamProbe p;
    void* raw = static_cast<void*>(static_cast<IO::Stream*>(&p));

    streamorder::CheckSharedStreamSlots(raw);

    // THE TRAP IN THE FAMILY, asserted rather than commented. `IO::Stream`
    // declares seven virtuals, so slot 7 is whatever the derived class declares
    // FIRST: `Close()` on the MemoryStream/Compressed branch, `Open(Filename)`
    // here. A port that dispatched +0x38 on a FileStream expecting `Close`
    // would call `Open` — same offset, same table shape, and nothing else in
    // the build would diagnose it.
    Reset();
    using OpenFn = bool (*)(void*, const void*);
    System::Filename fn;
    CHECK(SlotAt<OpenFn>(raw, streamorder::kSlot7)(raw, &fn) == true);
    CHECK_EQ(g_lastMark, streamorder::kSlot7 / 8);
    CHECK_EQ(g_lastMark, 7);

    streamorder::CheckDistinctSlots(raw, 8);

    Reset();
    { const IO::Stream& s = p; (void)s.GetSource(); }
    CHECK_EQ(g_lastMark, 1);

    // FileStream's slot-7 override takes a Filename and MemoryStream's takes
    // nothing: the two branches genuinely disagree about +0x38, and the type
    // system now carries that rather than a comment.
    static_assert(!std::has_virtual_destructor<IO::FileStream>::value,
                  "IO::FileStream must not acquire a virtual destructor");
    static_assert(!std::has_virtual_destructor<IO::BinaryFileStream>::value,
                  "IO::BinaryFileStream must not acquire a virtual destructor");
}

UNIT_TEST(BinaryFileStream_differs_from_FileStream_in_slot_0_only) {
    streamorder::FileStreamProbe fs;
    streamorder::BinaryFileStreamProbe bfs;
    void* rawBfs = static_cast<void*>(static_cast<IO::Stream*>(&bfs));

    // All eight slots land where FileStream's do.
    streamorder::CheckSharedStreamSlots(rawBfs);
    Reset();
    using OpenFn = bool (*)(void*, const void*);
    System::Filename fn;
    SlotAt<OpenFn>(rawBfs, streamorder::kSlot7)(rawBfs, &fn);
    CHECK_EQ(g_lastMark, 7);
    streamorder::CheckDistinctSlots(rawBfs, 8);

    // The class adds NO state — which is the whole content of the retail fact
    // that `??_7BinaryFileStream@` and `??_7FileStream@` are byte-identical in
    // slots 1..7 and differ only at slot 0.
    static_assert(sizeof(IO::BinaryFileStream) == sizeof(IO::FileStream),
                  "BinaryFileStream exists to swap slot 0, not to add a field");
    static_assert(sizeof(IO::BinaryFileStream) == 0x50, "0x50, the eh_vector stride");

    // `this` is unadjusted two levels down, which is what every raw slot caller
    // in runtime/src assumes when it hands the same pointer to either type.
    CHECK(static_cast<void*>(static_cast<IO::Stream*>(&bfs)) ==
          static_cast<void*>(static_cast<IO::FileStream*>(&bfs)));
    CHECK(static_cast<void*>(static_cast<IO::FileStream*>(&bfs)) ==
          static_cast<void*>(static_cast<IO::BinaryFileStream*>(&bfs)));

    // And the two probes' tables are DIFFERENT objects, so the test is not
    // accidentally reading one table twice.
    void* const* vtFs  = *reinterpret_cast<void* const* const*>(
        static_cast<void*>(static_cast<IO::Stream*>(&fs)));
    void* const* vtBfs = *reinterpret_cast<void* const* const*>(rawBfs);
    CHECK(vtFs != vtBfs);
}

UNIT_TEST(the_FileStream_pair_is_layout_identical_to_its_PODs) {
    static_assert(sizeof(IO::FileStream) == sizeof(RtIo::FileStream), "POD agrees");
    static_assert(sizeof(IO::BinaryFileStream) == sizeof(RtIo::BinaryFileStream),
                  "POD agrees");
    CHECK_EQ((int)offsetof(IO::FileStream, _filename), 0x18);
    CHECK_EQ((int)offsetof(IO::FileStream, _file),     0x20);
    // The three offsets FileStream.h pins THROUGH the embedded FileInfo, which
    // is what identified it at +0x20 in the first place.
    CHECK_EQ((int)(offsetof(IO::FileStream, _file) +
                   offsetof(RtIO::FileInfo, cursor)), 0x2c);
    CHECK_EQ((int)(offsetof(IO::FileStream, _file) +
                   offsetof(RtIO::FileInfo, size)), 0x28);
    CHECK_EQ((int)(offsetof(IO::FileStream, _file) +
                   offsetof(RtIO::FileInfo, winHandle)), 0x38);

    streamorder::BinaryFileStreamProbe p;
    p._filename = nullptr;
    p._file.cursor = 0x99;
    const RtIo::BinaryFileStream* pod =
        reinterpret_cast<const RtIo::BinaryFileStream*>(
            static_cast<IO::BinaryFileStream*>(&p));
    CHECK_EQ((int)pod->base.file.cursor, 0x99);
    CHECK(pod->base.filename == nullptr);
}

}  // namespace
