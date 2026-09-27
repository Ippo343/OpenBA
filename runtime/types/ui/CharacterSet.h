#pragma once
// ============================================================================
// runtime/types/ui/CharacterSet.h — UI::CharacterSet, 0x28 bytes, 9 slots
// ============================================================================
//
// ENDGAME B.3.6 block D. No header existed for this class before (RULE B6-8):
// the layout is MOVED here from the offsets runtime/src/ui/characterset.cpp's
// ported bodies read, and the size is retail's.
//
//   sizeof 0x28   `MOV EDX,0x28` in its own slot-0 vector deleting destructor
//                 (0x1800378a0, array arm at 0x1800378d2) — B36_SLOTS.md
//                 section 1 — and the `Allocate(0x28, 8)` in the factory row.
//   parent        `?_ParentTypeName@CharacterSet@UI@@CAPEBDXZ` (0x180037420)
//                 returns "Object::External" (B36_SLOTS.md section 3).
//   +0x20         ONE System::String. The destructor (0x1800cb840) releases it
//                 through the refcount-at-data[-4] protocol and Serialize
//                 (0x1800cb7b0) hands it to String::Serialize (0x1800c9930).
//                 Neither body says what the characters MEAN, so it is named by
//                 offset (B6-8: do not invent names).
//
// `??_7CharacterSet@UI@@6B@` @ 0x180156220, NINE slots (B36_SLOTS.md):
//   [0] +0x00 0x1800378a0  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [1] +0x08 0x180037450  ?TypeName@CharacterSet@UI@@UEBAPEBDXZ
//   [2] +0x10 0x180037430  ?Type@CharacterSet@UI@@UEBAIXZ
//   [3] +0x18 0x1800cb830  ?GetFileVersion@CharacterSet@UI@@UEBAHXZ  (MOV EAX,1)
//   [4] +0x20 0x1800066e0  ?Compile@External@Object@@  INHERITED — NOT declared
//   [5] +0x28 0x1800cb7b0  ?Serialize@CharacterSet@UI@@UEAA_NAEAVStream@IO@@@Z
//   [6] +0x30 0x180037650  ?Reconstruct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ
//   [7] +0x38 0x180037710  ?Destruct@CharacterSet@UI@@UEAAXXZ
//   [8] +0x40 0x1800376b0  ?Construct@CharacterSet@UI@@UEAAPEAVExternal@Object@@XZ
//                          — INTRODUCED HERE (RULE B6-1.1: slot 8 on a resource)
//
// ENDGAME B.3 step 9 (block UI-D, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's constructor body minus its vptr stores (retail has NO
// out-of-line `??0CharacterSet`: the body is inlined into Construct 0x1800376b0,
// Reconstruct 0x180037650 and both factory statics — seven stores, of which the
// last is +0x20 = 0); slot 0 is the class's own vector deleting dtor;
// ~CharacterSet is protected, non-virtual and holds the retired ??1 door's
// body. The factory pair is the class's own (characterset.cpp), replacing the
// FACTUI1 row; the minted `??_7CharacterSet@UI@@6B@` and its RT_DELDTOR row
// are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/External.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree
#include "../system/String.h"   // RtSystem::String — the POD, trivially destructible

namespace System { class Filename; }

namespace UI {

class CharacterSet : public Object::External {
public:
    // The REAL constructor: External() (+0x08..+0x18 zero), then the one own
    // store, +0x20 = 0 (0x1800376fa in Construct's inlined copy).
    CharacterSet() : Object::External() { _string20.data = nullptr; }

    // [0] 0x1800378a0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x28`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<CharacterSet*>(self + static_cast<int64_t>(i) * 0x28)
                    ->~CharacterSet();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~CharacterSet();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }

    const char* TypeName() const override;              // [1] 0x180037450
    uint32_t    Type() const override;                  // [2] 0x180037430
    int32_t     GetFileVersion() const override;        // [3] 0x1800cb830
    // [4] 0x1800066e0 is Object::External::Compile, inherited unchanged.
    bool        Serialize(void* stream) override;       // [5] 0x1800cb7b0
    Object::External* Reconstruct() override;           // [6] 0x180037650
    void        Destruct() override;                    // [7] 0x180037710
    virtual Object::External* Construct();              // [8] 0x1800376b0 — B6-1.1

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    CharacterSet(const CharacterSet& src);              // 0x180037720
    CharacterSet& operator=(const CharacterSet& src);   // 0x1800377b0
    // 0x180037440 — the minted record's type id (characterset.cpp).
    static uint32_t TypeId();
    // The reflection block's statics go through `System::MemoryAllocator`,
    // a door for the whole of step 6 (B36_RULES section 7). Placement is
    // spelt `::new (p) CharacterSet()` (B9-5): these stay undefined.
    static void* operator new(std::size_t bytes);              // 0x180037460
    static void* operator new(std::size_t bytes, void* place); // 0x1800374d0
    static void  operator delete(void* p);                     // 0x1800374e0
    // Retail's factory pair (B39_RULES D2): MemAlloc(0x28, 8) + the ctor, and
    // the null-tested in-place ctor. Defined and registered in characterset.cpp.
    static Object::Base* CreateNewObject();                    // 0x180037540
    static Object::Base* CreateInPlaceObject(void* place);     // 0x1800375f0

protected:
    // `??1CharacterSet@UI@@UEAA@XZ` 0x1800cb840. NON-VIRTUAL; holds the retired
    // dtor door's body (the +0x20 release); the compiler chains ~External.
    ~CharacterSet();

public:
    // ---- the 0x08 bytes CharacterSet adds (public: the doors reach them) ----
    RtSystem::String _string20;   // +0x20  the one member; see the banner
};

static_assert(sizeof(CharacterSet) == 0x28,
              "UI::CharacterSet is 0x28 bytes (MOV EDX,0x28 in slot 0, 0x1800378a0)");
static_assert(alignof(CharacterSet) == 8, "UI::CharacterSet aligns to 8 (the vptr)");
static_assert(offsetof(CharacterSet, _string20) == 0x20,
              "CharacterSet +0x20: released by ~CharacterSet 0x1800cb840, "
              "serialized by 0x1800cb7b0");
static_assert(offsetof(CharacterSet, _string20) == sizeof(Object::External),
              "CharacterSet's own bytes begin right after Object::External");

}  // namespace UI
