#pragma once
// ============================================================================
// runtime/types/ui/SliderDescription.h — UI::SliderDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. RULE B6-8: this class had NO header; its layout
// lived as raw offsets in runtime/src/ui/sliderdescription.cpp, MOVED here.
// Evidence, all in that file (read from the retail bytes when it was ported):
//   * `??0SliderDescription@UI@@QEAA@XZ` 0x1800d7b60 — every store below;
//     +0xb4..+0xb7 is never written;
//   * `?Serialize@` 0x1800d7890 — ExternalPtr<ElementDescription> (0x180038f90)
//     at +0x68, ExternalPtr<SpriteSheet> (0x180004590) at +0x70, five
//     System::Strings, then int, int, float, float, int;
//   * `?CreateInstance@` 0x1800d7630 — +0x68 becomes the element's one child,
//     the five names its five sprites, and +0xa0..+0xb0 are copied to
//     SliderElement +0x110..+0x120 (the names below follow SliderElement.h);
//   * sizeof 0xb8 — `MOV EDX,0xb8` in the slot-0 vector deleting dtor
//     0x180049c60 (B36_SLOTS.md section 1).
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@SliderDescription@UI@@CAPEBDXZ       @0x180049710 -> "SliderDescription"
//     ?_ParentTypeName@SliderDescription@UI@@CAPEBDXZ @0x180049720 -> "ElementDescription"
//
// `??_7SliderDescription@UI@@6B@` @ 0x1801575e0, TEN slots:
//   [0] 0x180049c60 deleting dtor (RT_DELDTOR row)  [1] 0x180049750 TypeName
//   [2] 0x180049730 Type            [3] 0x1800d79f0 GetFileVersion (0x304)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x1800d7890 Serialize       [6] 0x1800498d0 Reconstruct
//   [7] 0x180049900 Destruct        [8] 0x1800d7630 CreateInstance RETSLOT (B6-2)
//   [9] 0x1800498f0 Construct — INTRODUCED here (B6-1.1)
//
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2) -- the paragraph below is
// HISTORY: the class is now CONCRETE. A real public ctor (retail's stores), its
// own slot 0 (the vector deleting dtor), a protected non-virtual ~SliderDescription holding
// the retired ??1 door's body, and the factory pair as the class's own statics;
// every live one wears the COMPILER's table. The minted `??_7SliderDescription@UI@@6B@`,
// its RT_DELDTOR row 0x180049c60 and the ctor / dtor / slot doors are retired
// (tools/rt_mint_retired/ui-b.txt).
// ABSTRACT (slot 0 `= 0`), NO real ctor. The destructor is declared only:
// `??1` (0x1800d7a00) is the registered door.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class SliderDescription : public ElementDescription {
public:
    // ??0SliderDescription@UI@@QEAA@XZ 0x1800d7b60 -- the REAL ctor:
    // ElementDescription's stores (inlined in retail), then +0x68..+0xa7 zero,
    // +0xa8 0.0f, +0xac 1.0f (stored as the integer 0x3f800000), +0xb0 10.
    // +0xb4 is written by nothing.
    SliderDescription() : ElementDescription() {
        _childDescription = nullptr;
        _spriteSheet      = nullptr;
        _trackStartName   = nullptr;
        _trackMiddleName  = nullptr;
        _trackEndName     = nullptr;
        _thumbName        = nullptr;
        _thumbHotName     = nullptr;
        _trackOffsetY     = 0;
        _thumbMargin      = 0;
        _minimum          = 0.0f;
        _maximum          = 1.0f;
        _steps            = 10;
    }

    // [0] 0x180049c60 -- MSVC's VECTOR deleting destructor, both arms exactly as the
    // retired RT_DELDTOR row ran them (`MOV EDX,0xb8` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<SliderDescription*>(self + static_cast<int64_t>(i) * 0xb8)->~SliderDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~SliderDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                        // [1] 0x180049750
    uint32_t    Type() const override;                            // [2] 0x180049730
    int32_t     GetFileVersion() const override;                  // [3] 0x1800d79f0
    bool        Serialize(void* stream) override;                 // [5] 0x1800d7890 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x1800498d0
    void        Destruct() override;                              // [7] 0x180049900
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800d7630 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x1800498f0

    SliderDescription(const SliderDescription& src);              // 0x180049910  not ported
    SliderDescription& operator=(const SliderDescription& src);   // 0x180049a30  not ported
    static uint32_t      TypeId();          // over the minted `?_typeInfo@` record until W3
    static Object::Base* CreateNewObject();                       // (D2) the class's own
    static Object::Base* CreateInPlaceObject(void* place);        // (D2) the class's own

protected:
    // `??1SliderDescription@UI@@UEAA@XZ` (0x1800d7a00): NON-VIRTUAL (no virtual dtor until
    // the normal-virtual cutover). Retail's body is in sliderdescription.cpp;
    // the compiler chains the base's.
    ~SliderDescription();

private:
    static const char* _TypeName();                               // 0x180049710
    static const char* _ParentTypeName();                         // 0x180049720
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    void*    _childDescription;   // +0x68  ExternalPtr<UI::ElementDescription> -> children[0]
    void*    _spriteSheet;        // +0x70  ExternalPtr<UI::SpriteSheet>
    char*    _trackStartName;     // +0x78  System::String -> SliderElement::_trackStart
    char*    _trackMiddleName;    // +0x80  -> _trackMiddle
    char*    _trackEndName;       // +0x88  -> _trackEnd
    char*    _thumbName;          // +0x90  -> _thumb
    char*    _thumbHotName;       // +0x98  -> _thumbHot
    int32_t  _trackOffsetY;       // +0xa0  ctor 0  -> SliderElement +0x110
    int32_t  _thumbMargin;        // +0xa4  ctor 0  -> +0x114
    float    _minimum;            // +0xa8  ctor 0.0f -> +0x118
    float    _maximum;            // +0xac  ctor 1.0f (stored as 0x3f800000) -> +0x11c
    int32_t  _steps;              // +0xb0  ctor 10 -> +0x120
    uint32_t _padB4;              // +0xb4  written by nothing
};

static_assert(sizeof(SliderDescription) == 0xb8,
              "UI::SliderDescription is 0xb8 (MOV EDX,0xb8 in 0x180049c60)");
static_assert(alignof(SliderDescription) == 8, "UI::SliderDescription aligns to 8");
static_assert(offsetof(SliderDescription, _childDescription) == sizeof(ElementDescription), "");
static_assert(offsetof(SliderDescription, _childDescription) == 0x68, "");
static_assert(offsetof(SliderDescription, _spriteSheet)      == 0x70, "");
static_assert(offsetof(SliderDescription, _trackStartName)   == 0x78, "");
static_assert(offsetof(SliderDescription, _thumbHotName)     == 0x98, "");
static_assert(offsetof(SliderDescription, _trackOffsetY)     == 0xa0, "");
static_assert(offsetof(SliderDescription, _thumbMargin)      == 0xa4, "");
static_assert(offsetof(SliderDescription, _minimum)          == 0xa8, "");
static_assert(offsetof(SliderDescription, _maximum)          == 0xac, "");
static_assert(offsetof(SliderDescription, _steps)            == 0xb0, "");

}  // namespace UI
