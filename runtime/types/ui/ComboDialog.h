#pragma once
// ============================================================================
// runtime/types/ui/ComboDialog.h — UI::ComboDialog, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block B. The drop-down list a UI::ComboElement opens.
// Per-field evidence: the `RtUI::ComboDialog` POD's banner in DialogInstance.h,
// which stays (the ctor door `RtUi_ComboDialog_Ctor` builds through it).
//
// PARENT: `UI::DialogInstance`, proved by
//   0x180074730  ?_ParentTypeName@ComboDialog@UI@@CAPEBDXZ -> "DialogInstance"
//
// `??_7ComboDialog@UI@@6B@` @ 0x1801590e0, TEN slots (B36_SLOTS.md sec. 1):
//
//   [0] +0x00  0x180074a10  (unexported) vector deleting dtor
//   [1] +0x08  0x180074760  ?TypeName@ComboDialog@UI@@UEBAPEBDXZ
//   [2] +0x10  0x180074740  ?Type@ComboDialog@UI@@UEBAIXZ
//   [3..5]                  inherited from UI::DialogInstance (Persist/OnOpened/OnClosed)
//   [6] +0x30  0x180111cb0  ?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ
//   [7] +0x38  0x180111bd0  ?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z
//   [8] +0x40  0x180111af0  ?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z
//   [9]                     inherited (OnSerializeSettings)
//
// sizeof 0xd0: `MOV ECX,0xd0` at 0x18011282b (the one ??2 call site) and the
// deleting dtor's stride. ABSTRACT, no real ctor (B6-3; slot 0 is the mint's).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "DialogInstance.h"

namespace UI {

class ComboElement;   // block C1

class ComboDialog : public DialogInstance {
public:
    // [0] 0x180074a10 — pure on purpose; the RT_DELDTOR row is not retired.
    Object::Base* DeletingDtor(unsigned flags) override = 0;

    const char* TypeName() const override;                        // [1] 0x180074760
    uint32_t    Type() const override;                            // [2] 0x180074740
    void OnUpdateLayout() override;                               // [6] 0x180111cb0
    void OnUpdate(const System::Time& time) override;             // [7] 0x180111bd0
    void OnElementEvent(Object::InternalPtr<Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x180111af0

    // ---- exported, NOT PORTED as members ----------------------------------
    ComboDialog(const ComboDialog& other);                        // 0x180074940
    static uint32_t      TypeId();                                // 0x180074750
    // MemoryAllocator statics (B36_RULES 7): `??2` 0x180074770 IS ported as the
    // door `RtUi_ComboDialog_OperatorNew` (combodialog_ctor.cpp); the rest not.
    static void* operator new(std::size_t bytes);                 // 0x180074770
    static void* operator new(std::size_t bytes, void* place);    // 0x1800747e0
    static void  operator delete(void* p);                        // 0x1800747f0
    static Object::Base* CreateNewObject();                       // 0x180074850
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x1800748f0

protected:
    // NOT retail's ctors (0x180111d00 — the door `RtUi_ComboDialog_Ctor` —
    // and 0x180112130) — B6-3.
    ComboDialog() = default;
    // `??1ComboDialog@UI@@UEAA@XZ` 0x180111cd0, DEFINED EMPTY; the door
    // `RtUi_ComboDialog_Dtor` stamps the minted table and chains by name.
    ~ComboDialog();

    // RULE B5-15 / B6-7, COPIED from B36_SLOTS section 3 (the parent literal
    // is BARE here, unlike ToolTipDialog's):
    //   0x180074720  ?_TypeName@       -> "ComboDialog"
    //   0x180074730  ?_ParentTypeName@ -> "DialogInstance"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    // ---- the 0x18 bytes ComboDialog adds ------------------------------------
    void* _list;                                        // +0xb8  InternalPtr<UI::ListElement>
    // NOT the base's `_parent`: the ctor hands the base a zeroed parent and
    // keeps the real one here (POD banner).
    Object::InternalPtr<DialogInstance> _parentDialog;  // +0xc0
    void* _combo;                                       // +0xc8  InternalPtr<UI::ComboElement>
};

static_assert(sizeof(ComboDialog) == 0xd0,
              "UI::ComboDialog is 0xd0 bytes (MOV ECX,0xd0 at 0x18011282b)");
static_assert(alignof(ComboDialog) == 8, "qword-aligned (the vptr)");
static_assert(sizeof(ComboDialog) == sizeof(RtUI::ComboDialog),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(ComboDialog, _list) == sizeof(DialogInstance),
              "ComboDialog's own fields begin at the end of the DialogInstance base");
static_assert(offsetof(ComboDialog, _list)         == 0xb8, "_list @ +0xb8");
static_assert(offsetof(ComboDialog, _parentDialog) == 0xc0, "_parentDialog @ +0xc0");
static_assert(offsetof(ComboDialog, _combo)        == 0xc8, "_combo @ +0xc8");
static_assert(offsetof(ComboDialog, _parentDialog) == offsetof(RtUI::ComboDialog, _parentDialog),
              "alias _parentDialog");
static_assert(offsetof(ComboDialog, _combo) == offsetof(RtUI::ComboDialog, _combo), "alias _combo");

}  // namespace UI
