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

    // The three `RtObject::SerializerLite` constants are the SAME three retail
    // offsets under a second spelling (Internal.h:227-231). If B.3.4 ever moves
    // one and not the other, this is where it is caught.
    static_assert((int)RtObject::kEncSerializeBool == rw_vtbl::kScalarBool, "SerializerLite bool");
    static_assert((int)RtObject::kEncSerializeUint == rw_vtbl::kScalarUInt, "SerializerLite uint");
    static_assert((int)RtObject::kEncSerializeInt  == rw_vtbl::kScalarInt,  "SerializerLite int");
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
    static_assert(sizeof(RtObject::SerializerLite) == sizeof(TestSerializer),
                  "RtObject::SerializerLite (Internal.h:212) agrees");

    // A non-polymorphic class, so `offsetof` is unconditionally supported here
    // — and it must stay non-polymorphic: a vptr would make it 0x18 bytes and
    // every one of the 861 `g_fn_Serializer_Ctor`-family sites would write
    // through the wrong offsets.
    static_assert(offsetof(TestSerializer, stream)       == 0x00, "Serializer.stream");
    static_assert(offsetof(TestSerializer, readerWriter) == 0x08, "Serializer.readerWriter");
    static_assert(offsetof(rt::io::Serializer, stream)       == 0x00, "rt::io agrees");
    static_assert(offsetof(rt::io::Serializer, readerWriter) == 0x08, "rt::io agrees");
    static_assert(offsetof(RtObject::SerializerLite, stream)       == 0x00, "Lite agrees");
    static_assert(offsetof(RtObject::SerializerLite, readerWriter) == 0x08, "Lite agrees");

    // And therefore the three views alias cleanly — which is what lets the 16
    // `SerializerLite` users and the 37 `SerializerEncodingVCall` sites keep
    // working against a real `IO::Serializer` for the whole of B.3.4. The live
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

}  // namespace
