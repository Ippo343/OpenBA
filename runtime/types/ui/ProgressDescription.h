#pragma once
// ============================================================================
// runtime/types/ui/ProgressDescription.h — UI::ProgressDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. RULE B6-8: this class had NO header; its layout
// lived as raw offsets in runtime/src/ui/progressdescription.cpp. Those offsets
// are MOVED here under the retail name. Evidence, all in that file (read from
// the retail bytes when it was ported):
//   * `??0ProgressDescription@UI@@QEAA@XZ` 0x18011a210 — every store, one per
//     field below (the +0x68 store is a DWORD, so +0x6c is never written);
//   * `?Serialize@` 0x180119f60 — the field types (kScalarUInt at +0x68,
//     ExternalPtr<SpriteSheet> 0x180004590 at +0x70, six System::Strings
//     0x1800c9930, ExternalPtr<ElementDescription> 0x180038f90 at +0xa8);
//   * `?CreateInstance@` 0x180119d00 — the six names' destinations on the
//     ProgressElement (names below follow ProgressElement.h's sprites).
//   * sizeof 0xb0 — `MOV EDX,0xb0` in the slot-0 vector deleting dtor
//     0x180077c80 (B36_SLOTS.md section 1).
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@ProgressDescription@UI@@CAPEBDXZ       @0x180077730 -> "ProgressDescription"
//     ?_ParentTypeName@ProgressDescription@UI@@CAPEBDXZ @0x180077740 -> "ElementDescription"
//
// `??_7ProgressDescription@UI@@6B@` @ 0x1801591e0, TEN slots:
//   [0] 0x180077c80 deleting dtor (RT_DELDTOR row)  [1] 0x180077770 TypeName
//   [2] 0x180077750 Type            [3] 0x18011a070 GetFileVersion (0x204)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x180119f60 Serialize       [6] 0x1800778f0 Reconstruct
//   [7] 0x180077920 Destruct        [8] 0x180119d00 CreateInstance RETSLOT (B6-2)
//   [9] 0x180077910 Construct — INTRODUCED here (B6-1.1)
//
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2) -- the paragraph below is
// HISTORY: the class is now CONCRETE. A real public ctor (retail's stores), its
// own slot 0 (the vector deleting dtor), a protected non-virtual ~ProgressDescription holding
// the retired ??1 door's body, and the factory pair as the class's own statics;
// every live one wears the COMPILER's table. The minted `??_7ProgressDescription@UI@@6B@`,
// its RT_DELDTOR row 0x180077c80 and the ctor / dtor / slot doors are retired
// (tools/rt_mint_retired/ui-b.txt).
// ABSTRACT (slot 0 `= 0`), NO real ctor (the `??0` door builds every live one
// and it wears the MINTED table). The destructor is declared only: `??1` is the
// registered door (0x18011a080).
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"

namespace UI {

class ProgressDescription : public ElementDescription {
public:
    // ??0ProgressDescription@UI@@QEAA@XZ 0x18011a210 -- the REAL ctor:
    // ElementDescription's stores (inlined in retail), then +0x68 ONE DWORD
    // (+0x6c stays untouched) and +0x70..+0xaf zero.
    ProgressDescription() : ElementDescription() {
        _unknown68        = 0;
        _spriteSheet      = nullptr;
        _fillLeftName     = nullptr;
        _fillMiddleName   = nullptr;
        _fillRightName    = nullptr;
        _trackLeftName    = nullptr;
        _trackMiddleName  = nullptr;
        _trackRightName   = nullptr;
        _childDescription = nullptr;
    }

    // [0] 0x180077c80 -- MSVC's VECTOR deleting destructor, both arms exactly as the
    // retired RT_DELDTOR row ran them (`MOV EDX,0xb0` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ProgressDescription*>(self + static_cast<int64_t>(i) * 0xb0)->~ProgressDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ProgressDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                        // [1] 0x180077770
    uint32_t    Type() const override;                            // [2] 0x180077750
    int32_t     GetFileVersion() const override;                  // [3] 0x18011a070
    bool        Serialize(void* stream) override;                 // [5] 0x180119f60 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x1800778f0
    void        Destruct() override;                              // [7] 0x180077920
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x180119d00 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180077910

    ProgressDescription(const ProgressDescription& src);          // 0x180077930  not ported
    ProgressDescription& operator=(const ProgressDescription& src); // 0x180077a30  not ported
    static uint32_t      TypeId();          // over the minted `?_typeInfo@` record until W3
    static Object::Base* CreateNewObject();                       // (D2) the class's own
    static Object::Base* CreateInPlaceObject(void* place);        // (D2) the class's own

protected:
    // `??1ProgressDescription@UI@@UEAA@XZ` (0x18011a080): NON-VIRTUAL (no virtual dtor until
    // the normal-virtual cutover). Retail's body is in progressdescription.cpp;
    // the compiler chains the base's.
    ~ProgressDescription();

private:
    static const char* _TypeName();                               // 0x180077730
    static const char* _ParentTypeName();                         // 0x180077740
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    uint32_t _unknown68;          // +0x68  ctor DWORD 0; kScalarUInt. Serialized, read by no slot
    uint32_t _pad6c;              // +0x6c  written by nothing
    void*    _spriteSheet;        // +0x70  ExternalPtr<UI::SpriteSheet>
    char*    _fillLeftName;       // +0x78  System::String -> ProgressElement::_fillLeft
    char*    _fillMiddleName;     // +0x80  -> _fillMiddle
    char*    _fillRightName;      // +0x88  -> _fillRight
    char*    _trackLeftName;      // +0x90  -> _trackLeft
    char*    _trackMiddleName;    // +0x98  -> _trackMiddle
    char*    _trackRightName;     // +0xa0  -> _trackRight
    void*    _childDescription;   // +0xa8  ExternalPtr<UI::ElementDescription>, OPTIONAL
};

static_assert(sizeof(ProgressDescription) == 0xb0,
              "UI::ProgressDescription is 0xb0 (MOV EDX,0xb0 in 0x180077c80)");
static_assert(alignof(ProgressDescription) == 8, "UI::ProgressDescription aligns to 8");
static_assert(offsetof(ProgressDescription, _unknown68) == sizeof(ElementDescription), "");
static_assert(offsetof(ProgressDescription, _unknown68)        == 0x68, "");
static_assert(offsetof(ProgressDescription, _spriteSheet)      == 0x70, "");
static_assert(offsetof(ProgressDescription, _fillLeftName)     == 0x78, "");
static_assert(offsetof(ProgressDescription, _fillMiddleName)   == 0x80, "");
static_assert(offsetof(ProgressDescription, _fillRightName)    == 0x88, "");
static_assert(offsetof(ProgressDescription, _trackLeftName)    == 0x90, "");
static_assert(offsetof(ProgressDescription, _trackMiddleName)  == 0x98, "");
static_assert(offsetof(ProgressDescription, _trackRightName)   == 0xa0, "");
static_assert(offsetof(ProgressDescription, _childDescription) == 0xa8, "");

}  // namespace UI
