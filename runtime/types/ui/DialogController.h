#pragma once
// ============================================================================
// runtime/types/ui/DialogController.h — UI::DialogController, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block B. The byte evidence for every field is the
// `RtUI::DialogController` POD's banner in DialogInstance.h (which stays: the
// engine-side alias `engine/types/ui/DialogController.h` still names it. The 38
// exe panel structs that embedded it BY VALUE are gone — ENDGAME B.3.8 block PNL
// made every panel a real subclass of this class — so the POD retires at the END
// of B.3 with its alias.)
//
// PARENT: `Object::Internal`, proved by
//   0x18003b6a0  ?_ParentTypeName@DialogController@UI@@CAPEBDXZ -> "Object::Internal"
//
// `??_7DialogController@UI@@6B@` @ 0x1801565f0, ELEVEN slots, read as `.rdata`
// qwords (B36_SLOTS.md section 1):
//
//   [ 0] +0x00  0x18003b800  (unexported) vector deleting dtor
//   [ 1] +0x08  0x18003b6d0  ?TypeName@DialogController@UI@@UEBAPEBDXZ
//   [ 2] +0x10  0x18003b6b0  ?Type@DialogController@UI@@UEBAIXZ
//   [ 3] +0x18  0x1800bfda0  ?Persist@Internal@Object@@  INHERITED — not declared
//   [ 4] +0x20  0x1800cd2b0  ?OnCreated@DialogController@UI@@UEAAXXZ
//   [ 5] +0x28  0x1800cd2a0  ?OnOpened@DialogController@UI@@UEAAXXZ
//   [ 6] +0x30  0x1800cd290  ?OnClosed@DialogController@UI@@UEAAXXZ
//   [ 7] +0x38  0x1800cd280  ?OnUpdateLayout@DialogController@UI@@UEAAXXZ
//   [ 8] +0x40  0x1800cd270  ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z
//   [ 9] +0x48  0x1800cd260  ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z
//   [10] +0x50  0x1800cd250  ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z
//
// *** RULE B6-1.2: THIS NUMBERING IS ONE SLOT APART FROM DialogInstance's. ***
// `OnCreated` is slot 4 HERE and does not exist on DialogInstance, so
// `OnOpened` is 5 here and 4 there, `OnSerializeSettings` 10 here and 9 there.
// Never copy one class's slot comments onto the other.
//
// sizeof 0x30: the deleting dtor's `MOV EDX,0x30` AND all 38 exe panel structs
// in engine/types/ui/Panels.h start their own members at +0x30.
//
// ABSTRACT (RULE B6-3): the exe panel classes derive it as real C++ classes
// (ENDGAME B.3.8 block PNL; the hand-built tables and
// code/Game/Dialog/dialog_controller_vtable.cpp are gone) and slot 0 is the
// mint's `RT_DELDTOR` row until END of B.3. The exe
// never instantiates the base itself (no exe copy of its table, B38_SLOTS
// section 4), so slot 0 stays `= 0`. ENDGAME B.3.8 block RB gave it retail's
// REAL constructor (0x1800cd320) for PNL's converted panels.
//
// ONE ABI NOTE, carried by slot 9: `InternalPtr<Element>` is BY VALUE in the
// mangled name, but MSVC passes an 8-byte class with a user-declared copy
// ctor BY ADDRESS (DialogInstance::OnElementEvent 0x180114ed0 re-reads `*RDX`
// every iteration and hands the ADDRESS of a stack copy on). Our
// `Object::InternalPtr<T>` is trivially copyable and GCC would pass it in a
// register, so the slot is declared with the POINTER it really receives —
// the parameter-side twin of RULE B6-2's return slots.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../object/Internal.h"
#include "../object/External.h"

namespace System { class Time; }

namespace UI {

class DialogInstance;           // DialogInstance.h (this block)
class DialogControllerConfig;   // DialogControllerConfig.h (this block)
class Element;                  // block A — runtime/types/ui/Element.h

class DialogController : public Object::Internal {
public:
    // [0] 0x18003b800 — pure on purpose; the RT_DELDTOR row is not retired.
    Object::Base* DeletingDtor(unsigned flags) override = 0;

    const char* TypeName() const override;                        // [1] 0x18003b6d0
    uint32_t    Type() const override;                            // [2] 0x18003b6b0
    // [3] Persist — Object::Internal's own `RET` (0x1800bfda0), inherited.
    virtual void OnCreated();                                     // [4] 0x1800cd2b0  RET
    virtual void OnOpened();                                      // [5] 0x1800cd2a0  RET
    virtual void OnClosed();                                      // [6] 0x1800cd290  RET
    virtual void OnUpdateLayout();                                // [7] 0x1800cd280  RET
    virtual void OnUpdate(const System::Time& time);              // [8] 0x1800cd270  RET
    // [9] 0x1800cd260  RET. `element` is the ADDRESS of the caller's
    // InternalPtr copy — see the banner.
    virtual void OnElementEvent(Object::InternalPtr<Element>* element,
                                int32_t a, int32_t b, int32_t c,
                                int32_t d, int32_t e, void* userData);
    // [10] 0x1800cd250  RET. `IO::Stream&` in retail; spelled as the pointer
    // every C caller passes, the `Object::Internal::Persist` precedent.
    virtual void OnSerializeSettings(void* stream);

    // ---- non-virtual exported surface ------------------------------------
    // RETSLOT in retail (`MOV RAX,[RCX+0x20]; MOV [RDX],RAX; MOV RAX,RDX`);
    // a plain by-value return here, the door placement-news it (B5-12).
    Object::InternalPtr<Element> GetContainer();                  // 0x18003b700

    // Exported, NOT PORTED, no caller: declared, deliberately undefined.
    void Setup(Object::Base* dataSource, Object::InternalPtr<DialogInstance> dialog,
               Object::ExternalPtr<DialogControllerConfig> config,
               Object::InternalPtr<Element> container);           // 0x1800cd2c0
    DialogController(const DialogController& other);              // 0x18003b710
    DialogController& operator=(const DialogController& other);   // 0x18003b780

    static uint32_t      TypeId();                                // 0x18003b6c0  not ported
    static Object::Base* CreateNewObject();                       // 0x18003b6e0  not ported
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18003b6f0  not ported

protected:
    // ??0DialogController@UI@@QEAA@XZ  0x1800cd320 — the REAL constructor
    // (ENDGAME B.3.8 block RB, RULE B8-1). Retail, whole:
    //     CALL 0x1800bfd10 (??0Internal) ; LEA RAX,[0x1801565f0] ; MOV [RBX],RAX
    //     XOR EAX,EAX ; MOV [RBX+0x10],RAX ; MOV [RBX+0x18],RAX ; MOV [RBX+0x20],RAX
    // The two stamps are the compiler's; the body is the three zero stores.
    // +0x28 (`_dataSource`) is NOT written — AddController fills it.
    // THERE IS NO CTOR DOOR for this class (B38_RULES D1 lists one; there is
    // none — 0x1800cd320 was never ported or registered, and the 38 exe panel
    // factories build their base as alloc + zero + table, EXE_SUBCLASSES.md
    // 1B), so nothing is shared: this is the port.
    DialogController() : _config(), _dialog(nullptr), _container(nullptr) {}
    // `??1DialogController@UI@@UEAA@XZ` 0x1800cd2f0. NON-VIRTUAL (decision 2)
    // and EMPTY, which is retail's body: 0x1800cd2f0 is the restamp plus the
    // inlined ~Internal, both emitted by the compiler chain. The registered
    // door `RtUiDialogController_Dtor` (dialog.cpp) does the same two things
    // by hand for the minted objects (body in dialog_controller_base.cpp).
    ~DialogController();

    // RULE B5-15 / B6-7, both COPIED from B36_SLOTS section 3:
    //   0x18003b690  ?_TypeName@       -> "DialogController"
    //   0x18003b6a0  ?_ParentTypeName@ -> "Object::Internal"
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    // ---- the 0x20 bytes DialogController adds ----------------------------
    // PUBLIC for the reason Object::Internal's are: unconverted bodies reach
    // them at these offsets through a cast. Filled by AddController
    // (0x1801133b0) after the factory built the derived object.
    Object::ExternalPtr<DialogControllerConfig> _config;   // +0x10
    DialogInstance* _dialog;       // +0x18  the owning dialog (AddController's `this`)
    void*           _container;    // +0x20  InternalPtr<UI::Element> (block A's class)
    Object::Base*   _dataSource;   // +0x28
};

static_assert(sizeof(DialogController) == 0x30,
              "UI::DialogController is 0x30 bytes (MOV EDX,0x30 in 0x18003b800; "
              "38 Panels.h subclasses start their fields at +0x30)");
static_assert(alignof(DialogController) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(DialogController, _config)     == 0x10, "_config @ +0x10 (AddController 0x180113422)");
static_assert(offsetof(DialogController, _dialog)     == 0x18, "_dialog @ +0x18 (AddController 0x18011342a)");
static_assert(offsetof(DialogController, _container)  == 0x20, "_container @ +0x20 (GetContainer 0x18003b700)");
static_assert(offsetof(DialogController, _dataSource) == 0x28, "_dataSource @ +0x28 (AddController 0x180113426)");
static_assert(offsetof(DialogController, _config) == sizeof(Object::Internal),
              "DialogController's own fields begin at the end of the Internal base");

}  // namespace UI
