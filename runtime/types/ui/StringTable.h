#pragma once
// ============================================================================
// runtime/types/ui/StringTable.h — UI::StringTable, 0x30 bytes, 9 slots
// ============================================================================
//
// ENDGAME B.3.6 block D (RULE B6-4.2 / B6-8): the real class under retail's
// name. This header used to carry a bare `UiStringTable` POD, a bare
// `StringTableTextEntry` and a `StringTableOffsets` sub-namespace; all three
// fold in here, and `UiStringTable` / `StringTableTextEntry` stay as global
// transition aliases.
//
// THE LAYOUT, and where each fact was read (Runtime-x64.dll, image base
// 0x180000000):
//   sizeof 0x30   `?CreateNewObject@StringTable@UI@@` (0x1800022b1):
//                 `MOV EDX,0x30 ; LEA R8D,[RDX-0x28]` -> Allocate(0x30, 8);
//                 corroborated by B36_SLOTS.md section 1 (the slot-0 body's
//                 `MOV EDX,0x30`).
//   +0x20..+0x2f  a Collection::Array<TextEntry> {data, size, capacity}:
//                 `GetCount` (0x180002610) is `return *(int*)(this+0x28)`, and
//                 GetText(int) (0x180002550) / GetIndexFromName (0x180002470)
//                 walk a stride-0x10 array at `*(this+0x20)` bounded by it. The
//                 ctor `??0StringTable@UI@@QEAA@XZ` (0x18011d890) zeroes +0x20
//                 and +0x28 (one 8-byte store covering size AND capacity).
//   TextEntry     stride 0x10: +0x00 the NAME (GetIndexFromName compares it),
//                 +0x08 the TEXT (GetText bumps and returns it), both
//                 refcounted System::String data pointers.
//   parent        "Object::External" (`?_ParentTypeName@StringTable@UI@@`
//                 0x180002160).
//
// `??_7StringTable@UI@@6B@` @ 0x180154630, NINE slots (B36_SLOTS.md):
//   [0] +0x00 0x180002740  (unexported) vector deleting dtor — the class's own
//                          (B.3.9); it had no RT_DELDTOR row, its body was the
//                          by-address `RtUiStringTable_ScalarDelete` (retired)
//   [1] +0x08 0x180002190  ?TypeName@StringTable@UI@@UEBAPEBDXZ
//   [2] +0x10 0x180002170  ?Type@StringTable@UI@@UEBAIXZ
//   [3] +0x18 0x18011d930  ?GetFileVersion@StringTable@UI@@UEBAHXZ  (MOV EAX,1)
//   [4] +0x20 0x1800066e0  ?Compile@External@Object@@  INHERITED — NOT declared
//   [5] +0x28 0x18011d940  ?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 0x180002390  ?Reconstruct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ
//   [7] +0x38 0x180002460  ?Destruct@StringTable@UI@@UEAAXXZ
//   [8] +0x40 0x180002400  ?Construct@StringTable@UI@@UEAAPEAVExternal@Object@@XZ — B6-1.1
//
// ENDGAME B.3 step 9 (block UI-D, B39_RULES D1/D2): CONCRETE, the
// CharacterSet.h shape. The real public ctor is retail's `??0StringTable`
// (0x18011d890) minus its vptr stores; slot 0 the class's own vector deleting
// dtor; ~StringTable protected and non-virtual with the retired ??1 door's
// body. The factory pair is the class's own (stringtable_factory.cpp); the
// minted `??_7StringTable@UI@@6B@` is retired.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace System { class String; }

namespace UI {

class StringTable : public Object::External {
public:
    // One row of the table, stride 0x10 (see the banner). Retail's nested
    // record; its name is not exported, so it is named for what it holds.
    struct TextEntry {
        char* name;   // +0x00  System::String data ptr (refcounted key, NULL = empty)
        char* text;   // +0x08  System::String data ptr (refcounted value, NULL = empty)
    };

    // ??0StringTable@UI@@QEAA@XZ 0x18011d890 — the REAL constructor: External()
    // (+0x08..+0x18), then +0x20 = 0 and ONE 8-byte store covering _size and
    // _capacity (18011d8d5 / 18011d8d9).
    StringTable() : Object::External() { _data = nullptr; _size = 0; _capacity = 0; }

    // [0] 0x180002740 — MSVC's VECTOR deleting destructor (`MOV EDX,0x30`, the
    // element size; the scalar arm is ??1 inlined, then ??3StringTable — one
    // body with the global MemFree, stringtable_lifecycle.cpp's banner).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<StringTable*>(self + static_cast<int64_t>(i) * 0x30)->~StringTable();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~StringTable();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }

    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180002190, Type 0x180002170, CreateNewObject 0x180002280, CreateInPlaceObject 0x180002330
    DECLARE_TYPE(StringTable)
public:
    int32_t     GetFileVersion() const override;        // [3] 0x18011d930
    // [4] 0x1800066e0 is Object::External::Compile, inherited unchanged.
    bool        Serialize(void* stream) override;       // [5] 0x18011d940
    Object::External* Reconstruct() override;           // [6] 0x180002390
    void        Destruct() override;                    // [7] 0x180002460
    virtual Object::External* Construct();              // [8] 0x180002400 — B6-1.1

    // ---- ported non-virtual members (runtime/src/ui/sprite.cpp) ------------
    int32_t GetCount() const;                                       // 0x180002610
    // The two GetText overloads — non-virtual, THE DOOR DECIDES THE OVERLOAD.
    // Both are RETSLOT in retail; plain by-value returns here.
    System::String GetText(int32_t index) const;                    // 0x180002550
    System::String GetText(const System::String& name) const;       // 0x1800025b0
    int32_t GetIndexFromName(const System::String& name) const;     // 0x180002470

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    StringTable(const StringTable& src);                // 0x180002620  not ported
    StringTable& operator=(const StringTable& src);     // 0x1800026b0  not ported
    // 0x180002180 — the minted record's type id (stringtable_factory.cpp).
    // Placement is spelt `::new (p) StringTable()` (B9-5): these stay undefined;
    // `??3` keeps its registered door in stringtable_lifecycle.cpp.
    static void* operator new(std::size_t bytes);              // 0x1800021a0
    static void* operator new(std::size_t bytes, void* place); // 0x180002210
    static void  operator delete(void* p);                     // 0x180002220  (door)
    // Retail's factory pair (B39_RULES D2), defined and registered in
    // stringtable_factory.cpp.

protected:
    // 0x18011d8f0 — ~Array<TextEntry> (stringtable_factory.cpp), then the
    // compiler chains ~External.
    ~StringTable();

public:
    // ---- the 0x10 bytes StringTable adds: Collection::Array<TextEntry> -----
    // Spelled as the three raw words: `kSpec_StringPair10` (0x180078c90, the
    // array's SetSize) is a NON-TRIVIAL hand-derived row, so the one resize
    // bridges through it rather than through a trait-derived Array<TextEntry>.
    TextEntry* _data;       // +0x20  Collection::Array._data
    int32_t    _size;       // +0x28  Collection::Array._size   (GetCount)
    int32_t    _capacity;   // +0x2C  Collection::Array._capacity
};

static_assert(sizeof(StringTable::TextEntry) == 0x10, "StringTable entry stride 0x10");
static_assert(offsetof(StringTable::TextEntry, name) == 0x00, "entry name at +0x00");
static_assert(offsetof(StringTable::TextEntry, text) == 0x08, "entry text at +0x08");
static_assert(sizeof(StringTable) == 0x30,
              "UI::StringTable is 0x30 bytes (CreateNewObject 0x1800022b1 allocates 0x30)");
static_assert(alignof(StringTable) == 8, "UI::StringTable aligns to 8 (the vptr)");
static_assert(offsetof(StringTable, _data)     == 0x20, "GetText 0x180002550 reads *(this+0x20)");
static_assert(offsetof(StringTable, _size)     == 0x28, "GetCount 0x180002610 reads +0x28");
static_assert(offsetof(StringTable, _capacity) == 0x2C, "Array capacity at +0x2C");
static_assert(offsetof(StringTable, _data) == sizeof(Object::External),
              "StringTable's own bytes begin right after Object::External");

}  // namespace UI

// RULE B6-4.2 transition aliases for the three runtime/src/ui TUs that spell
// the old bare names.
using UiStringTable        = UI::StringTable;
using StringTableTextEntry = UI::StringTable::TextEntry;
using StringTable          = UI::StringTable;
