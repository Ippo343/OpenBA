// ============================================================================
// runtime/src/ui/comboelement.cpp — UI::ComboElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ComboElement@UI@@6B@` (retail 0x180159030),
// a `UI::ButtonElement` subclass whose Update opens a popup list.
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180075390  ComboElement.h (the class's own)
//    1    TypeName                     0x180074f30  here
//    2    Type                         0x180074f10  here
//    4    ButtonElement::ComputeSize   0x180110d10  buttonelement.cpp
//    5    ButtonElement::ComputeLayout 0x180110e70  buttonelement.cpp
//    6    ButtonElement::Draw          0x180111020  buttonelement.cpp
//    7    Update                       0x1801125d0  here
//    8    ButtonElement::Enable        0x180110cc0  buttonelement.cpp
//
// plus ??1ComboElement@UI@@UEAA@XZ 0x180112490 and the factory pair (ENDGAME
// B.3.9 UI-A: the real ctor ??0 0x1801123f0 is in the header; the copy-ctor
// door is retired).
//
// ENDGAME B.3.6 block C1: the three slot bodies are MEMBERS of
// `UI::ComboElement` (runtime/types/ui/ComboElement.h, which now owns the
// whole 0x128-byte layout, including the two description handles this file
// used to carry as file-local constants). `?AddItem@`, `?RemoveAllItems@`,
// `?SetCurrentItem@`, the three getters and `?UpdateText@` are declared on the
// class and stay DOORS in runtime/src/ui/widgets_input.cpp (fragment pass).
// ============================================================================
#include "element_common.h"
#include "../../types/ui/ComboElement.h"
#include "../../types/ui/ComboDialog.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/ListElement.h"   // B.3.9 TI-UI: X::TypeId()
#include "../../types/ui/TextElement.h"
#include <new>

using namespace rtuielem;

namespace {

// UI::TextElement (block C3's class), the per-item element the list is filled
// with — reached at raw offsets.
constexpr int kTextParams      = 0xc8;   // Array<System::String>::_data
constexpr int kTextParamCount  = 0xd0;   // Array<System::String>::_size
constexpr int kTextSprite      = 0x100;  // UI::Sprite {sheet +0x100, index +0x108}
constexpr int kTextSpriteIndex = 0x108;

// UI::ListElement (block C4's class)
constexpr int kListSelected = 0xd8;
constexpr int kListCount    = 0xe8;

// UI::DialogInstance (block B's class)
constexpr int kDlgInstFlags = 0x20;

// UI::UserInterface (block E's): the open-dialog array at +0x28 {data, size, capacity}.
constexpr int kUiDialogs      = 0x28;
constexpr int kUiDialogCount  = 0x30;

constexpr int kDialogSize = 0xd0;

// Element::_flags bits Element.h does not name (see buttonelement.cpp).
constexpr uint32_t kFlagHover        = 0x00020000u;
constexpr uint32_t kFlagPressStarted = 0x00200000u;

// ENDGAME B.3.9 TI-UI: the two type ids Update tests against are
// `UI::ListElement::TypeId()` / `UI::TextElement::TypeId()`, read at the use.

using IsDerivedFromFn = bool  (*)(const void* self, uint32_t typeId);
using StringAssignFn   = void* (*)(void* dst, const void* src);
using CacheTextFn      = void  (*)(void* textElement);
using ListAddItemFn    = void  (*)(void* list, void** item, int flags);
using OperatorNewFn    = void* (*)(uint64_t size);
using SetUpperLeftFn   = void  (*)(void* dlgInstance, int x, int y, bool centre);
using DynArrayPushFn   = void  (*)(void* array, int newSize, const void* fill);
using CloseDialogFn    = void  (*)(void* ui, void** dlgInstance);
using ArrayDtorFn      = void* (*)(void* array);

}  // namespace

namespace UI {

// ============================================================================
// ?Update@ComboElement@UI@@   0x1801125d0
// ============================================================================
// On a click, build the popup: instantiate the list description, instantiate
// one item description per entry, fill each item's text and sprite, add it to
// the list, then wrap the list in a `UI::ComboDialog` and push that dialog onto
// the UserInterface. Then run ButtonElement::Update (QUALIFIED) either way.
//
// ---------------------------------------------------------------------------
// THE STACK `UI::Dialog` IS RETAIL'S, AND IT IS THE ARGUMENT EVERY
// CreateInstance RECEIVES.
// ---------------------------------------------------------------------------
// Retail constructs a 0xd0-byte `UI::Dialog` IN THE FRAME (the three vtable
// stamps at 0x180112646/0x180112652/0x180112675 are Base, External and
// `??_7Dialog@UI@@6B@`), hands its address to both descriptions, and destroys
// it before returning. The constructor and destructor are INLINED there;
// `??0Dialog@UI@@QEAA@XZ` (0x180112db0) and `??1Dialog@UI@@UEAA@XZ`
// (0x180112e40) are the SAME two bodies out of line — runtime/src/ui/
// dialog_ctor_dtor.cpp transcribes them instruction for instruction — so they
// are CALLED here rather than re-inlined. That is the same deliberate departure
// runtime/src/object/deleting_dtor.cpp's banner records for its 22 inlined
// destructors: identical computation, one extra call.
//
// The two descriptions' slot 8 is a VIRTUAL call on `UI::ElementDescription`
// through the description's own (minted) table, with the hidden return slot in
// retail order (RULE B6-2).
//
// ---------------------------------------------------------------------------
// FOUR THINGS THE BYTES SAY
// ---------------------------------------------------------------------------
//  1. `IsDerivedFrom` failing yields a NULL that is then dereferenced —
//     `pLVar10` for the list (0x1801127fc reads `+0xe8` off it) and `pDVar` for
//     each item. Retail relies on the descriptions being the right types. No
//     guard is added.
//  2. The item's text is written into `_parameters[i]` — the TextElement's
//     parameter array at +0xc8 — and ONLY when that array is non-empty
//     (`if (_paramCount > 0)`). An item description with no parameter slot
//     silently shows its own default text.
//  3. The dirty walk around the text write happens BEFORE the assignment
//     (0x180112750), not after it.
//  4. The sprite comparison is sheet + index, and the store is one 16-byte
//     move — the same shape as AnimatedImageElement::Update.
void ComboElement::Update(const System::Time& time, UserInterface* ui,
                          Object::InternalPtr<DialogInstance>* dialog,
                          const UpdateState& state) {
    const uint32_t flags = _flags;
    const bool open = (flags & kFlagDisabledMask) == 0 &&
                      _strings._size > 0 &&
                      state.b04 != 0 &&
                      (flags & kFlagHover) != 0 &&
                      (flags & kFlagPressStarted) != 0;

    if (open) {
        alignas(16) uint8_t dialogBuf[kDialogSize];
        Dialog* const stackDialog = ::new (static_cast<void*>(dialogBuf)) Dialog();   // ??0Dialog

        const auto isDerived = RtAddr<IsDerivedFromFn>(0x1800c0390ull);

        Object::InternalPtr<Element> listSlot;
        Object::InternalPtr<Element>* made =
            reinterpret_cast<const ElementDescription*>(_listDesc)       // B.3.6 bridge
                ->CreateInstance(&listSlot, stackDialog);
        void* madeList = *reinterpret_cast<void**>(made);
        void* list = isDerived(madeList, UI::ListElement::TypeId()) ? madeList : nullptr;

        const int32_t count = _strings._size;
        for (int32_t i = 0; i < count; ++i) {
            Object::InternalPtr<Element> itemSlot;
            Object::InternalPtr<Element>* madeItem =
                reinterpret_cast<const ElementDescription*>(_itemDesc)   // B.3.6 bridge
                    ->CreateInstance(&itemSlot, stackDialog);
            void* madeItemRaw = *reinterpret_cast<void**>(madeItem);
            void* item = isDerived(madeItemRaw, UI::TextElement::TypeId()) ? madeItemRaw : nullptr;

            const uint8_t* strings = reinterpret_cast<const uint8_t*>(_strings._data);
            if (I32(item, kTextParamCount) > 0) {
                rtuielem::SetNeedsSizeCalc(item);
                RtAddr<StringAssignFn>(0x1800c9810ull)(P(item, kTextParams),
                                                       strings + static_cast<int64_t>(i) * 8);
                RtAddr<CacheTextFn>(0x18011ec10ull)(item);
            }

            const uint8_t* sp = reinterpret_cast<const uint8_t*>(_sprites._data) +
                                static_cast<int64_t>(i) * 0x10;
            uint64_t lo = 0, hi = 0;
            __builtin_memcpy(&lo, sp + 0, 8);
            __builtin_memcpy(&hi, sp + 8, 8);
            if (*reinterpret_cast<const uint64_t*>(static_cast<uint8_t*>(item) + kTextSprite) != lo ||
                I32(item, kTextSpriteIndex) != static_cast<int32_t>(hi)) {
                rtuielem::SetNeedsSizeCalc(item);
            }
            __builtin_memcpy(static_cast<uint8_t*>(item) + kTextSprite + 0, &lo, 8);
            __builtin_memcpy(static_cast<uint8_t*>(item) + kTextSprite + 8, &hi, 8);

            void* tmp = item;
            RtAddr<ListAddItemFn>(0x1800d44b0ull)(list, &tmp, 0);
        }

        const int32_t cur = _currentItem;
        if (cur < 0 || cur >= I32(list, kListCount)) I32(list, kListSelected) = 0;
        else                                        I32(list, kListSelected) = cur;
        I32(list, kElemAlignment) = 0;

        void* combo = RtAddr<OperatorNewFn>(0x180074770ull)(kDialogSize);
        void* made2 = nullptr;
        if (combo != nullptr) {
            // ENDGAME B.3.9 UI-D (B9-13 foreign site): ??0ComboDialog, the real ctor.
            made2 = ::new (combo) ComboDialog(
                ui, Object::InternalPtr<Element>(static_cast<Element*>(list)),
                *dialog, Object::InternalPtr<ComboElement>(this));
        }
        _openDialog = made2;

        RtAddr<SetUpperLeftFn>(0x180114a60ull)(made2, _layoutLeft, _layoutBottom, false);
        U32(_openDialog, kDlgInstFlags) |= 2u;

        void* pushed = _openDialog;
        static_cast<DialogInstance*>(pushed)->OnOpened();              // DialogInstance slot 4
        uint8_t* u = reinterpret_cast<uint8_t*>(ui);
        RtAddr<DynArrayPushFn>(0x18007a0d0ull)(u + kUiDialogs,
                                               I32(u, kUiDialogCount) + 1, &pushed);

        stackDialog->UI::Dialog::~Dialog();                          // ??1Dialog
    }

    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;
    ButtonElement::Update(time, ui, &dlgCopy, state);
}

}  // namespace UI

// ============================================================================
// ??1ComboElement@UI@@UEAA@XZ   0x180112490 -- retail's body, moved here from
// the retired door (ENDGAME B.3.9 UI-A)
// ============================================================================
// Close the popup if it is still open -- through the UserInterface the
// DialogInstance itself points at (`_openDialog->[+0x10]`), NOT through any
// cached manager -- then destroy the three arrays with their element
// destructors (by address, retail's three instantiations: Strings released,
// Sprites and user data trivially). Each of those ZEROES the array header, so
// the compiler-emitted member `~Array`s that run after this body see an empty
// array (no element destroy, MemFree(null)). Then the compiler restamps and
// chains ~ButtonElement / ~Element.
namespace UI {
ComboElement::~ComboElement() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    void* open = _openDialog;
    if (open != nullptr) {
        void* slot = open;
        RtAddr<CloseDialogFn>(0x180120f20ull)(Pc(open, 0x10), &slot);
    }
    RtAddr<ArrayDtorFn>(0x180024aa0ull)(self + offsetof(ComboElement, _userData));
    RtAddr<ArrayDtorFn>(0x180037310ull)(self + offsetof(ComboElement, _sprites));
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(self + offsetof(ComboElement, _strings));
}
}  // namespace UI

namespace UI {
// ?CreateNewObject@ComboElement@UI@@SAPEAVBase@Object@@XZ  0x180075020
// The factory-interpreter row this replaces: MemAlloc(0x128, 8), retail's
// null branch, then the out-of-line ctor ??0ComboElement (0x1801123f0).
Object::Base* ComboElement::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ComboElement), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ComboElement();
}
// ?CreateInPlaceObject@ComboElement@UI@@SAPEAVBase@Object@@PEAX@Z  0x1800750a0
Object::Base* ComboElement::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ComboElement();
}
}  // namespace UI

// ============================================================================
// Registration. ENDGAME B.3.9 UI-A: the slot doors and the copy / ??1 doors
// (and FACTUI1's ??0 door), read only by the retired minted table, its
// RT_DELDTOR row and the interpreter row, are gone.
// ============================================================================
int RtUiComboElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@ComboElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ComboElement::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x180075020ull, (void*)&UI::ComboElement::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ComboElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ComboElement::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x1800750a0ull, (void*)&UI::ComboElement::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ComboElement's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ComboElement@UI@@ (0x1801faa50): name "ComboElement", parent
// "ButtonElement" (retail ?_ParentTypeName@ComboElement@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ComboElement, "ButtonElement", nullptr)
