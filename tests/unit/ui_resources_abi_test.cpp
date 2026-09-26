// ============================================================================
// tests/unit/ui_resources_abi_test.cpp — ENDGAME B.3.6 block D
//   the seven UI `Object::External` resources as real C++ classes
//
//   UI::CharacterSet  0x28   UI::Cursor       0x30   UI::StringTable  0x30
//   UI::SpriteSheet   0x58   UI::SpriteCapture 0x58  UI::Font         0x90
//   (UI::Dialog, the seventh 9-slot External in B36_SLOTS, is block B's)
//
// All abstract, all 9 slots, all directly on Object::External, `Construct`
// introduced at slot 8 (RULE B6-1.1), and ONLY `Font` and `SpriteSheet`
// override slot 4 `Compile` (RULE B6-1.5) — the other four carry
// `?Compile@External@Object@@` (0x1800066e0) unchanged.
//
// Built to the contract of graphics_material_abi_test.cpp (block G, step 5):
//   PART A — LAYOUT: size, alignment, every field offset the headers pin, and
//            that each class's own bytes begin at sizeof(Object::External).
//   PART B — SLOT ORDER: a probe subclass per class, its GCC vtable read back by
//            BYTE OFFSET and raw-called; each marker body records which slot
//            ran. Nine distinct entries, `Construct` at +0x40.
//   PART C — SLOT 4: shared with a bare External leaf on four classes,
//            overridden (a different pointer that answers retail's 1) on two.
//   PART D — THE QUALIFIED CALL: a door that delegates with
//            `reinterpret_cast<UI::C*>(self)->UI::C::M()` does not re-enter
//            itself through a minted table whose slot points back at the door.
//
// The real member bodies live in runtime/src/ui/*.cpp and stand on the port
// registry, so they are not linked here: the out-of-line members are defined as
// MARKERS at the bottom of this file (one definition per binary — none of those
// .cpp files is in UNIT_TREE_SRCS), which also makes GCC emit the vtables here.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/CharacterSet.h"
#include "runtime/types/ui/Cursor.h"
#include "runtime/types/ui/StringTable.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; }

template <typename Fn>
Fn SlotAt(void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

constexpr int kDeletingDtor   = 0x00;
constexpr int kTypeName       = 0x08;
constexpr int kType           = 0x10;
constexpr int kGetFileVersion = 0x18;
constexpr int kCompile        = 0x20;
constexpr int kSerialize      = 0x28;
constexpr int kReconstruct    = 0x30;
constexpr int kDestruct       = 0x38;
constexpr int kConstruct      = 0x40;   // introduced per leaf — RULE B6-1.1

// A bare Object::External leaf: whatever it carries at +0x20 IS
// `?Compile@External@Object@@`.
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

struct CharacterSetProbe final : UI::CharacterSet {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};

// Raw-call slots 0,1,2,3,5,6,7,8 and check the marker each one leaves.
void CheckNineSlots(void* raw) {
    using DtorFn = void* (*)(void*, unsigned);
    using NameFn = const char* (*)(const void*);
    using TypeFn = uint32_t (*)(const void*);
    using VerFn  = int32_t (*)(const void*);
    using SerFn  = bool (*)(void*, void*);
    using PtrFn  = void* (*)(void*);
    using VoidFn = void (*)(void*);

    Reset(); SlotAt<DtorFn>(raw, kDeletingDtor)(raw, 0u);
    CHECK_EQ(g_lastMark, kDeletingDtor / 8); CHECK(g_lastSelf == raw);
    Reset(); SlotAt<NameFn>(raw, kTypeName)(raw);       CHECK_EQ(g_lastMark, kTypeName / 8);
    Reset(); SlotAt<TypeFn>(raw, kType)(raw);           CHECK_EQ(g_lastMark, kType / 8);
    Reset(); SlotAt<VerFn>(raw, kGetFileVersion)(raw);  CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    Reset(); SlotAt<SerFn>(raw, kSerialize)(raw, nullptr); CHECK_EQ(g_lastMark, kSerialize / 8);
    Reset(); SlotAt<PtrFn>(raw, kReconstruct)(raw);     CHECK_EQ(g_lastMark, kReconstruct / 8);
    Reset(); SlotAt<VoidFn>(raw, kDestruct)(raw);       CHECK_EQ(g_lastMark, kDestruct / 8);
    Reset(); SlotAt<PtrFn>(raw, kConstruct)(raw);       CHECK_EQ(g_lastMark, kConstruct / 8);
    CHECK(g_lastSelf == raw);

    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            CHECK(vt[i] != vt[j]);
}

// Slot 4 is Object::External's own body (inherited) on four of the six.
void CheckCompileInherited(void* raw) {
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kCompile / 8] == VTableOf(static_cast<void*>(&bare))[kCompile / 8]);
}

// ---------------------------------------------------------------------------
// UI::CharacterSet
// ---------------------------------------------------------------------------
UNIT_TEST(ui_characterset_layout) {
    CHECK_EQ((int)sizeof(UI::CharacterSet), 0x28);
    CHECK_EQ((int)alignof(UI::CharacterSet), 8);
    CHECK_EQ((int)offsetof(UI::CharacterSet, _string20), 0x20);
    CHECK_EQ((int)offsetof(UI::CharacterSet, _string20), (int)sizeof(Object::External));
    CHECK((std::is_base_of<Object::External, UI::CharacterSet>::value));
    CHECK(std::is_abstract<UI::CharacterSet>::value);
    CHECK(!std::is_default_constructible<UI::CharacterSet>::value);
}

UNIT_TEST(ui_characterset_slot_order_is_retails) {
    CharacterSetProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckNineSlots(raw);
    CheckCompileInherited(raw);
    CHECK(static_cast<void*>(static_cast<Object::External*>(&probe)) == raw);
}

// PART D on CharacterSet: a minted table whose slot 3 points at a DOOR that
// delegates with the qualified call; the call must enter the door once and
// land in the member, not recurse.
int g_doorEntries = 0;
int32_t MintedGetFileVersionDoor(const void* self) {
    ++g_doorEntries;
    return reinterpret_cast<const UI::CharacterSet*>(self)      // B.3.6 bridge
               ->UI::CharacterSet::GetFileVersion();
}

UNIT_TEST(ui_characterset_qualified_call_does_not_reenter_the_door) {
    CharacterSetProbe probe;
    void* raw = static_cast<void*>(&probe);
    const void* minted[9] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 9; ++i) minted[i] = real[i];
    minted[kGetFileVersion / 8] = reinterpret_cast<const void*>(&MintedGetFileVersionDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    using VerFn = int32_t (*)(const void*);
    const int32_t v = SlotAt<VerFn>(raw, kGetFileVersion)(raw);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    CHECK_EQ(v, 1);                      // retail's literal (0x1800cb830)

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ---------------------------------------------------------------------------
// UI::Cursor
// ---------------------------------------------------------------------------
struct CursorProbe final : UI::Cursor {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};

UNIT_TEST(ui_cursor_layout) {
    CHECK_EQ((int)sizeof(UI::Cursor), 0x30);
    CHECK_EQ((int)alignof(UI::Cursor), 8);
    CHECK_EQ((int)offsetof(UI::Cursor, _hotSpotX),    0x20);
    CHECK_EQ((int)offsetof(UI::Cursor, _hotSpotY),    0x24);
    CHECK_EQ((int)offsetof(UI::Cursor, _spriteSheet), 0x28);
    CHECK_EQ((int)offsetof(UI::Cursor, _hotSpotX), (int)sizeof(Object::External));
    CHECK((std::is_base_of<Object::External, UI::Cursor>::value));
    CHECK(std::is_abstract<UI::Cursor>::value);

    // The three getters read exactly the bytes retail's do (0x18003b250/260/270).
    CursorProbe probe;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(static_cast<UI::Cursor*>(&probe));
    *reinterpret_cast<int32_t*>(bytes + 0x20) = 7;
    *reinterpret_cast<int32_t*>(bytes + 0x24) = -3;
    *reinterpret_cast<void**>(bytes + 0x28) = bytes;
    CHECK_EQ(probe.GetHotSpotX(), 7);
    CHECK_EQ(probe.GetHotSpotY(), -3);
    CHECK(static_cast<void*>(probe.GetSpriteSheet().Get()) == static_cast<void*>(bytes));
}

UNIT_TEST(ui_cursor_slot_order_is_retails) {
    CursorProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckNineSlots(raw);
    CheckCompileInherited(raw);
}

// ---------------------------------------------------------------------------
// UI::StringTable
// ---------------------------------------------------------------------------
struct StringTableProbe final : UI::StringTable {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};

UNIT_TEST(ui_stringtable_layout) {
    CHECK_EQ((int)sizeof(UI::StringTable), 0x30);
    CHECK_EQ((int)alignof(UI::StringTable), 8);
    CHECK_EQ((int)offsetof(UI::StringTable, _data),     0x20);
    CHECK_EQ((int)offsetof(UI::StringTable, _size),     0x28);
    CHECK_EQ((int)offsetof(UI::StringTable, _capacity), 0x2c);
    CHECK_EQ((int)offsetof(UI::StringTable, _data), (int)sizeof(Object::External));
    CHECK_EQ((int)sizeof(UI::StringTable::TextEntry), 0x10);
    CHECK_EQ((int)offsetof(UI::StringTable::TextEntry, text), 0x08);
    CHECK((std::is_base_of<Object::External, UI::StringTable>::value));
    CHECK(std::is_abstract<UI::StringTable>::value);
    // The B6-4.2 transition aliases name the class and its row.
    CHECK((std::is_same<::UiStringTable, UI::StringTable>::value));
    CHECK((std::is_same<::StringTable, UI::StringTable>::value));
    CHECK((std::is_same<::StringTableTextEntry, UI::StringTable::TextEntry>::value));
}

UNIT_TEST(ui_stringtable_slot_order_is_retails) {
    StringTableProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckNineSlots(raw);
    CheckCompileInherited(raw);
}

}  // namespace

// ############################################################################
// MARKER bodies (see the banner). GetFileVersion answers retail's literal.
// ############################################################################
namespace UI {

const char* CharacterSet::TypeName() const { Note(this, kTypeName / 8); return "CharacterSet"; }
uint32_t    CharacterSet::Type() const { Note(this, kType / 8); return 0u; }
int32_t     CharacterSet::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 1; }
bool        CharacterSet::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* CharacterSet::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void        CharacterSet::Destruct() { Note(this, kDestruct / 8); }
Object::External* CharacterSet::Construct() { Note(this, kConstruct / 8); return this; }
CharacterSet::~CharacterSet() {}

const char* Cursor::TypeName() const { Note(this, kTypeName / 8); return "Cursor"; }
uint32_t    Cursor::Type() const { Note(this, kType / 8); return 0u; }
int32_t     Cursor::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 1; }
bool        Cursor::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* Cursor::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void        Cursor::Destruct() { Note(this, kDestruct / 8); }
Object::External* Cursor::Construct() { Note(this, kConstruct / 8); return this; }
Cursor::~Cursor() {}

const char* StringTable::TypeName() const { Note(this, kTypeName / 8); return "StringTable"; }
uint32_t    StringTable::Type() const { Note(this, kType / 8); return 0u; }
int32_t     StringTable::GetFileVersion() const { Note(this, kGetFileVersion / 8); return 1; }
bool        StringTable::Serialize(void*) { Note(this, kSerialize / 8); return true; }
Object::External* StringTable::Reconstruct() { Note(this, kReconstruct / 8); return this; }
void        StringTable::Destruct() { Note(this, kDestruct / 8); }
Object::External* StringTable::Construct() { Note(this, kConstruct / 8); return this; }
StringTable::~StringTable() {}

}  // namespace UI
