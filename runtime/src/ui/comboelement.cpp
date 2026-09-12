// ============================================================================
// runtime/src/ui/comboelement.cpp — UI::ComboElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ComboElement@UI@@6B@` (retail 0x180159030),
// a `UI::ButtonElement` subclass whose Update opens a popup list.
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180075390  deleting_dtor.cpp row
//    1    TypeName                     0x180074f30  here
//    2    Type                         0x180074f10  here
//    4    ButtonElement::ComputeSize   0x180110d10  buttonelement.cpp
//    5    ButtonElement::ComputeLayout 0x180110e70  buttonelement.cpp
//    6    ButtonElement::Draw          0x180111020  buttonelement.cpp
//    7    Update                       0x1801125d0  here
//    8    ButtonElement::Enable        0x180110cc0  buttonelement.cpp
//
// plus ??0ComboElement@UI@@QEAA@XZ         0x1801123f0
//      ??0ComboElement@UI@@QEAA@AEBV01@@Z  0x1800751e0
//      ??1ComboElement@UI@@UEAA@XZ         0x180112490
//
// `?AddItem@`, `?RemoveAllItems@`, `?SetCurrentItem@` and `?UpdateText@` are
// ALREADY PORTED in runtime/src/ui/widgets_input.cpp (Waves 7/9); the array
// layout is runtime/types/ui/ComboElement.h. This file adds the vtable rows.
//
// LAYOUT — 0x128 bytes (`MOV EDX,0x128` at 0x1800753af). ComboElement.h pins
// the three parallel arrays and the two scalars; this file adds the two
// description handles the constructor zeroes and Update dispatches on:
//
//   +0xd8  ExternalPtr<UI::ElementDescription>  the LIST description
//   +0xe0  ExternalPtr<UI::ElementDescription>  the per-ITEM description
//
// Both are proven by Update: it calls slot 8 (`+0x40`, CreateInstance) on each
// and type-tests the result against `UI::ListElement` and `UI::TextElement`
// respectively.
// ============================================================================
#include "widget_common.h"
#include "../../types/ui/ComboElement.h"

using namespace rtui;

static_assert(offsetof(UI::ComboElement, _strings)    == 0xE8, "");
static_assert(offsetof(UI::ComboElement, _sprites)    == 0xF8, "");
static_assert(offsetof(UI::ComboElement, _userData)   == 0x108, "");
static_assert(offsetof(UI::ComboElement, _currentItem)== 0x118, "");
static_assert(offsetof(UI::ComboElement, _openDialog) == 0x120, "");

namespace {

constexpr int kListDesc    = 0xd8;
constexpr int kItemDesc    = 0xe0;
constexpr int kStrings     = 0xe8;
constexpr int kItemCount   = 0xf0;
constexpr int kSprites     = 0xf8;
constexpr int kUserData    = 0x108;
constexpr int kCurrentItem = 0x118;
constexpr int kOpenDialog  = 0x120;

// UI::TextElement, the per-item element the list is filled with.
constexpr int kTextParams      = 0xc8;   // Array<System::String>::_data
constexpr int kTextParamCount  = 0xd0;   // Array<System::String>::_size
constexpr int kTextSprite      = 0x100;  // UI::Sprite {sheet +0x100, index +0x108}
constexpr int kTextSpriteIndex = 0x108;

// UI::ListElement
constexpr int kListSelected = 0xd8;
constexpr int kListCount    = 0xe8;

// UI::DialogInstance
constexpr int kDlgInstFlags = 0x20;

// UI::UserInterface: the open-dialog array at +0x28 {data, size, capacity}.
constexpr int kUiDialogs      = 0x28;
constexpr int kUiDialogCount  = 0x30;

// UI::Element::UpdateState
constexpr int kStateMouseUp = 0x04;

constexpr int kDialogSize = 0xd0;

// UI::ElementDescription's vtable slot 8 (+0x40) is CreateInstance — the same
// row runtime/src/ui/groupdescription.cpp implements for its own class.
constexpr int kSlotDescCreateInstance = 8;

RTUI_TYPE_SLOTS(ComboElement)
void* VtButtonElementBase() { return RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@"); }

uint32_t ListElementTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}
uint32_t TextElementTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}

using IsDerivedFromFn = bool  (*)(const void* self, uint32_t typeId);
// UI::ElementDescription vtable slot 8 (+0x40): CreateInstance(desc, &ret, dialog)
using CreateInstanceFn = void** (*)(const void* desc, void** ret, void* dialog);
using DialogCtorFn     = void* (*)(void* self);
using DialogDtorFn     = void  (*)(void* self);
using StringAssignFn   = void* (*)(void* dst, const void* src);
using CacheTextFn      = void  (*)(void* textElement);
using ListAddItemFn    = void  (*)(void* list, void** item, int flags);
using OperatorNewFn    = void* (*)(uint64_t size);
using ComboDialogCtorFn = void* (*)(void* self, void* ui, void** list,
                                    void** dlgInstance, void** owner);
using SetUpperLeftFn   = void  (*)(void* dlgInstance, int x, int y, bool centre);
using DynArrayPushFn   = void  (*)(void* array, int newSize, const void* fill);
using CloseDialogFn    = void  (*)(void* ui, void** dlgInstance);
using ButtonUpdateFn   = void  (*)(void* self, const void* time, void* ui,
                                   void** dlg, const void* state);
using ButtonCopyCtorFn = void* (*)(void* self, const void* other);
using ElementCtorFn    = void* (*)(void* self);
using ElementDtorFn    = void  (*)(void* self);
using ArrayDtorFn      = void* (*)(void* array);
using ArrayCopyFn      = void* (*)(void* dst, const void* src);

}  // namespace

RTUI_TYPE_BODIES(ComboElement)

// ============================================================================
// ?Update@ComboElement@UI@@   0x1801125d0
// ============================================================================
// On a click, build the popup: instantiate the list description, instantiate
// one item description per entry, fill each item's text and sprite, add it to
// the list, then wrap the list in a `UI::ComboDialog` and push that dialog onto
// the UserInterface. Then run ButtonElement::Update either way.
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
extern "C" void RtUi_ComboElement_Update(void* selfv, const void* time, void* ui,
                                         void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    const uint32_t flags = U32(self, kElemFlags);
    const bool open = (flags & kFlagDisabledMask) == 0 &&
                      I32(self, kItemCount) > 0 &&
                      st[kStateMouseUp] != 0 &&
                      ((flags >> 17) & 1u) != 0 &&
                      ((flags >> 21) & 1u) != 0;

    if (open) {
        alignas(16) uint8_t dialog[kDialogSize];
        RtAddr<DialogCtorFn>(0x180112db0ull)(dialog);

        const auto isDerived = RtAddr<IsDerivedFromFn>(0x1800c0390ull);

        void* listSlot = nullptr;
        void** made = VSlot<CreateInstanceFn>(P(self, kListDesc), kSlotDescCreateInstance)(
            P(self, kListDesc), &listSlot, dialog);
        void* list = isDerived(*made, ListElementTypeId()) ? *made : nullptr;

        const int32_t count = I32(self, kItemCount);
        for (int32_t i = 0; i < count; ++i) {
            void* itemSlot = nullptr;
            void** madeItem = VSlot<CreateInstanceFn>(P(self, kItemDesc), kSlotDescCreateInstance)(
                P(self, kItemDesc), &itemSlot, dialog);
            void* item = isDerived(*madeItem, TextElementTypeId()) ? *madeItem : nullptr;

            const uint8_t* strings = static_cast<const uint8_t*>(P(self, kStrings));
            if (I32(item, kTextParamCount) > 0) {
                SetNeedsSizeCalc(item);
                RtAddr<StringAssignFn>(0x1800c9810ull)(P(item, kTextParams),
                                                       strings + static_cast<int64_t>(i) * 8);
                RtAddr<CacheTextFn>(0x18011ec10ull)(item);
            }

            const uint8_t* sp = static_cast<const uint8_t*>(P(self, kSprites)) +
                                static_cast<int64_t>(i) * 0x10;
            uint64_t lo = 0, hi = 0;
            __builtin_memcpy(&lo, sp + 0, 8);
            __builtin_memcpy(&hi, sp + 8, 8);
            if (*reinterpret_cast<const uint64_t*>(static_cast<uint8_t*>(item) + kTextSprite) != lo ||
                I32(item, kTextSpriteIndex) != static_cast<int32_t>(hi)) {
                SetNeedsSizeCalc(item);
            }
            __builtin_memcpy(static_cast<uint8_t*>(item) + kTextSprite + 0, &lo, 8);
            __builtin_memcpy(static_cast<uint8_t*>(item) + kTextSprite + 8, &hi, 8);

            void* tmp = item;
            RtAddr<ListAddItemFn>(0x1800d44b0ull)(list, &tmp, 0);
        }

        const int32_t cur = I32(self, kCurrentItem);
        if (cur < 0 || cur >= I32(list, kListCount)) I32(list, kListSelected) = 0;
        else                                        I32(list, kListSelected) = cur;
        I32(list, kElemAlignment) = 0;

        void* combo = RtAddr<OperatorNewFn>(0x180074770ull)(kDialogSize);
        void* made2 = nullptr;
        if (combo != nullptr) {
            void* owner = self;
            void* dlgInst = *dlg;
            void* listArg = list;
            made2 = RtAddr<ComboDialogCtorFn>(0x180111d00ull)(
                combo, ui, &listArg, &dlgInst, &owner);
        }
        P(self, kOpenDialog) = made2;

        RtAddr<SetUpperLeftFn>(0x180114a60ull)(made2, I32(self, kElemLayoutLeft),
                                               I32(self, kElemLayoutBottom), false);
        U32(P(self, kOpenDialog), kDlgInstFlags) |= 2u;

        void* pushed = P(self, kOpenDialog);
        VSlot<ComputeSizeFn>(pushed, kSlotComputeSize)(pushed);
        RtAddr<DynArrayPushFn>(0x18007a0d0ull)(
            static_cast<uint8_t*>(ui) + kUiDialogs,
            I32(ui, kUiDialogCount) + 1, &pushed);

        RtAddr<DialogDtorFn>(0x180112e40ull)(dialog);
    }

    void* dlgCopy = *dlg;
    RtAddr<ButtonUpdateFn>(0x1801110b0ull)(self, time, ui, &dlgCopy, st);
}

// ============================================================================
// Constructors / destructor
// ============================================================================
// The default constructor INLINES ButtonElement's body (the base vtable stamp,
// +0xc0 = -1, +0xc4 = 0) and then zeroes everything from +0xd8 to +0x120 except
// +0x118, which starts at -1 ("no current item").
extern "C" void* RtUi_ComboElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtButtonElementBase();
    I32(self, 0xc0) = -1;
    I32(self, 0xc4) = 0;
    P(self, 0x00) = VtComboElement();
    for (int off = kListDesc; off <= 0x110; off += 8)
        *reinterpret_cast<uint64_t*>(self + off) = 0;
    I32(self, kCurrentItem) = -1;
    *reinterpret_cast<uint64_t*>(self + kOpenDialog) = 0;
    return self;
}

// ??0ComboElement@UI@@QEAA@AEBV01@@Z  0x1800751e0 — chains to the out-of-line
// ??0ButtonElement copy ctor, copies the two description handles, then
// deep-copies the three parallel arrays onto zeroed targets.
extern "C" void* RtUi_ComboElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ButtonCopyCtorFn>(0x180048460ull)(self, other);
    P(self, 0x00) = VtComboElement();
    P(self, kListDesc) = Pc(other, kListDesc);
    P(self, kItemDesc) = Pc(other, kItemDesc);

    *reinterpret_cast<uint64_t*>(self + kStrings)   = 0;
    *reinterpret_cast<uint64_t*>(self + kItemCount) = 0;
    RtAddr<ArrayCopyFn>(0x180010040ull)(self + kStrings, other + kStrings);

    *reinterpret_cast<uint64_t*>(self + kSprites)  = 0;
    *reinterpret_cast<uint64_t*>(self + 0x100)     = 0;
    RtAddr<ArrayCopyFn>(0x180037020ull)(self + kSprites, other + kSprites);

    *reinterpret_cast<uint64_t*>(self + kUserData) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x110)     = 0;
    RtAddr<ArrayCopyFn>(0x180024840ull)(self + kUserData, other + kUserData);

    I32(self, kCurrentItem) = I32c(other, kCurrentItem);
    P(self, kOpenDialog) = nullptr;
    P(self, kOpenDialog) = Pc(other, kOpenDialog);
    return self;
}

// ============================================================================
// ??1ComboElement@UI@@UEAA@XZ   0x180112490
// ============================================================================
// Close the popup if it is still open — through the UserInterface the
// DialogInstance itself points at (`_openDialog->[+0x10]`), NOT through any
// cached manager — then destroy the three arrays and chain to ~Element.
extern "C" void RtUi_ComboElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtComboElement();

    void* open = P(self, kOpenDialog);
    if (open != nullptr) {
        void* slot = open;
        RtAddr<CloseDialogFn>(0x180120f20ull)(Pc(open, 0x10), &slot);
    }

    RtAddr<ArrayDtorFn>(0x180024aa0ull)(self + kUserData);
    RtAddr<ArrayDtorFn>(0x180037310ull)(self + kSprites);
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(self + kStrings);

    P(self, 0x00) = VtButtonElementBase();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(self);
}

// ============================================================================
// Registration — slots 4/5/6/8 are ButtonElement's addresses, not re-registered.
// ============================================================================
int RtUiComboElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ComboElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ComboElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180074f30ull, (void*)&RtUi_ComboElement_TypeName);

    RegisterRuntimeExport("?Type@ComboElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ComboElement_Type);          ++n;
    RegisterRuntimeAddr(0x180074f10ull, (void*)&RtUi_ComboElement_Type);

    RegisterRuntimeExport(
        "?Update@ComboElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ComboElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1801125d0ull, (void*)&RtUi_ComboElement_Update);

    RegisterRuntimeExport("??0ComboElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ComboElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1801123f0ull, (void*)&RtUi_ComboElement_Ctor);

    RegisterRuntimeExport("??0ComboElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ComboElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x1800751e0ull, (void*)&RtUi_ComboElement_CopyCtor);

    RegisterRuntimeExport("??1ComboElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ComboElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180112490ull, (void*)&RtUi_ComboElement_Dtor);
    return n;
}
