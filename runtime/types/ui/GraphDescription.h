#pragma once
// ============================================================================
// runtime/types/ui/GraphDescription.h — UI::GraphDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. The layout was pinned by the Wave 7 batch as
// the POD `RtUI::GraphDescription` in DescriptionsBatch1.h, which four leaf
// clusters share and none may edit (RULE B6-4.4); this header carries the class
// and cross-asserts every offset against that POD. The closing pass turns the
// POD into an alias.
//
// PARENT, read (B36_SLOTS.md section 3):
//     ?_TypeName@GraphDescription@UI@@CAPEBDXZ       @0x180040050 -> "GraphDescription"
//     ?_ParentTypeName@GraphDescription@UI@@CAPEBDXZ @0x180040060 -> "ElementDescription"
// Recorded, not used (RULE B6-7).
//
// `??_7GraphDescription@UI@@6B@` @ 0x180156a30, TEN slots; sizeof 0x88
// (`MOV EDX,0x88` in the slot-0 vector deleting dtor 0x1800403a0):
//   [0] 0x1800403a0 deleting dtor (RT_DELDTOR row)  [1] 0x180040090 TypeName
//   [2] 0x180040070 Type            [3] 0x1800d1b20 GetFileVersion (0x104)
//   [4] External::Compile (inherited — NOT declared, B6-1.5)
//   [5] 0x1800d1a30 Serialize       [6] 0x180040210 Reconstruct
//   [7] 0x180040240 Destruct        [8] 0x1800d1920 CreateInstance RETSLOT (B6-2)
//   [9] 0x180040230 Construct — INTRODUCED here, behind CreateInstance (B6-1.1)
//
// ABSTRACT (slot 0 `= 0`), NO real ctor: every live one is built by the
// `??0GraphDescription@UI@@QEAA@XZ` door (0x1800d1bd0, factory_ui1.cpp) and
// wears the MINTED table.
//
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2): CONCRETE. A real public
// ctor (retail's 0x1800d1bd0 stores), its own slot 0, the factory pair as the
// class's own statics; ~GraphDescription holds the retired ??1 door's body. The
// minted `??_7GraphDescription@UI@@6B@`, its RT_DELDTOR row 0x1800403a0, the
// FACTUI1 row and the ctor / dtor / slot doors are retired.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::GraphDescription aliases this class

namespace UI {

class GraphDescription : public ElementDescription {
public:
    // ??0GraphDescription@UI@@QEAA@XZ 0x1800d1bd0 -- the REAL ctor:
    // ElementDescription's (inlined in retail), then the six tail stores.
    GraphDescription() : ElementDescription() {
        _width       = 100;
        _height      = 100;
        _tickSize    = 4;
        _unknown74   = 0xffffffffu;
        _spriteSheet = nullptr;
        _spriteName  = nullptr;
    }

    // ---- the ten slots, in retail order -----------------------------------
    // [0] 0x1800403a0 -- MSVC's VECTOR deleting destructor, both arms exactly as the
    // retired RT_DELDTOR row ran them (`MOV EDX,0x88` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<GraphDescription*>(self + static_cast<int64_t>(i) * 0x88)->~GraphDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~GraphDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180040090, Type 0x180040070, _TypeName 0x180040050, _ParentTypeName 0x180040060
    //         CreateNewObject 0x180040180, CreateInPlaceObject 0x180040200
    DECLARE_TYPE(GraphDescription)
public:
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d1b20
    // [4] Compile — Object::External's, inherited
    bool        Serialize(void* stream) override;                 // [5] 0x1800d1a30 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x180040210
    void        Destruct() override;                              // [7] 0x180040240
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d1920 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180040230

    GraphDescription(const GraphDescription& src);                // 0x180040250  not ported
    GraphDescription& operator=(const GraphDescription& src);     // 0x1800402d0  not ported

protected:
    // ??1GraphDescription 0x1800d1b30 -- NON-VIRTUAL; retail's body in
    // graphdescription.cpp, the compiler chains ~ElementDescription / ~External.
    ~GraphDescription();

private:

public:
    // Ctor 0x1800d1bd0 stores (factory_ui1_tables.inc kGraphDescription) and
    // what CreateInstance (0x1800d1920) copies each into on the GraphElement.
    int32_t  _width;        // +0x68  ctor 100 -> GraphElement::_width     (+0xc0)
    int32_t  _height;       // +0x6c  ctor 100 -> GraphElement::_height    (+0xc4)
    int32_t  _tickSize;     // +0x70  ctor 4   -> GraphElement::_tickSize  (+0xc8)
    uint32_t _unknown74;    // +0x74  ctor -1  -> GraphElement::_unknownCC (+0xcc); kScalarUInt
    void*    _spriteSheet;  // +0x78  ExternalPtr<UI::SpriteSheet>
    char*    _spriteName;   // +0x80  System::String, looked up in _spriteSheet
};

static_assert(sizeof(GraphDescription) == 0x88,
              "UI::GraphDescription is 0x88 (MOV EDX,0x88 in 0x1800403a0)");
static_assert(alignof(GraphDescription) == 8, "UI::GraphDescription aligns to 8");
static_assert(offsetof(GraphDescription, _width) == sizeof(ElementDescription), "");
static_assert(offsetof(GraphDescription, _width)       == 0x68, "");
static_assert(offsetof(GraphDescription, _height)      == 0x6c, "");
static_assert(offsetof(GraphDescription, _tickSize)    == 0x70, "");
static_assert(offsetof(GraphDescription, _unknown74)   == 0x74, "");
static_assert(offsetof(GraphDescription, _spriteSheet) == 0x78, "");
static_assert(offsetof(GraphDescription, _spriteName)  == 0x80, "");
// RtUI::GraphDescription is an ALIAS of this class since fragment pass F2;
// the POD's offsets are the absolute asserts above.

}  // namespace UI
