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
// plus ??0ComboElement@UI@@QEAA@AEBV01@@Z  0x1800751e0
//      ??1ComboElement@UI@@UEAA@XZ         0x180112490
// (`??0ComboElement@UI@@QEAA@XZ` 0x1801123f0 is factory_ui1.cpp's door.)
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

// B6-5: namespace-scope atomic door caches for the member bodies.
std::atomic<void*> g_comboTypeInfo{nullptr};
std::atomic<void*> g_listTypeInfo{nullptr};
std::atomic<void*> g_textTypeInfo{nullptr};

const uint8_t* ComboElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_comboTypeInfo, "?_typeInfo@ComboElement@UI@@0VBaseTypeInfo@Object@@A"));
}
uint32_t ListElementTypeId() {
    const uint32_t* p = static_cast<const uint32_t*>(AtomicExport(
        g_listTypeInfo, "?_typeInfo@ListElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}
uint32_t TextElementTypeId() {
    const uint32_t* p = static_cast<const uint32_t*>(AtomicExport(
        g_textTypeInfo, "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A"));
    return p ? *p : 0u;
}

// The ctor/dtor doors only.
void* VtComboElement()      { return RT_DATA_SLOT(void*, "??_7ComboElement@UI@@6B@"); }
void* VtButtonElementBase() { return RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@"); }

using IsDerivedFromFn = bool  (*)(const void* self, uint32_t typeId);
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
// UI::DialogInstance vtable slot 4 (+0x20) is OnOpened (RULE B6-1.2) — block
// B's class, reached through its table.
using DialogOnOpenedFn = void  (*)(void* self);
using ButtonCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn    = void  (*)(void* self);
using ArrayDtorFn      = void* (*)(void* array);
using ArrayCopyFn      = void* (*)(void* dst, const void* src);

}  // namespace

namespace UI {

uint32_t ComboElement::Type() const { return TypeIdOf(ComboElementTypeInfo()); }
const char* ComboElement::TypeName() const { return TypeNameOf(ComboElementTypeInfo()); }

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
        RtAddr<DialogCtorFn>(0x180112db0ull)(dialogBuf);
        const Dialog* stackDialog = reinterpret_cast<const Dialog*>(dialogBuf);

        const auto isDerived = RtAddr<IsDerivedFromFn>(0x1800c0390ull);

        Object::InternalPtr<Element> listSlot;
        Object::InternalPtr<Element>* made =
            reinterpret_cast<const ElementDescription*>(_listDesc)       // B.3.6 bridge
                ->CreateInstance(&listSlot, stackDialog);
        void* madeList = *reinterpret_cast<void**>(made);
        void* list = isDerived(madeList, ListElementTypeId()) ? madeList : nullptr;

        const int32_t count = _strings._size;
        for (int32_t i = 0; i < count; ++i) {
            Object::InternalPtr<Element> itemSlot;
            Object::InternalPtr<Element>* madeItem =
                reinterpret_cast<const ElementDescription*>(_itemDesc)   // B.3.6 bridge
                    ->CreateInstance(&itemSlot, stackDialog);
            void* madeItemRaw = *reinterpret_cast<void**>(madeItem);
            void* item = isDerived(madeItemRaw, TextElementTypeId()) ? madeItemRaw : nullptr;

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
            void* owner = this;
            void* dlgInst = *reinterpret_cast<void* const*>(dialog);
            void* listArg = list;
            made2 = RtAddr<ComboDialogCtorFn>(0x180111d00ull)(
                combo, ui, &listArg, &dlgInst, &owner);
        }
        _openDialog = made2;

        RtAddr<SetUpperLeftFn>(0x180114a60ull)(made2, _layoutLeft, _layoutBottom, false);
        U32(_openDialog, kDlgInstFlags) |= 2u;

        void* pushed = _openDialog;
        VSlot<DialogOnOpenedFn>(pushed, 4)(pushed);
        uint8_t* u = reinterpret_cast<uint8_t*>(ui);
        RtAddr<DynArrayPushFn>(0x18007a0d0ull)(u + kUiDialogs,
                                               I32(u, kUiDialogCount) + 1, &pushed);

        RtAddr<DialogDtorFn>(0x180112e40ull)(dialogBuf);
    }

    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;
    ButtonElement::Update(time, ui, &dlgCopy, state);
}

}  // namespace UI

// ============================================================================
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_ComboElement_Type(const void* self) {
    return reinterpret_cast<const UI::ComboElement*>(self)->UI::ComboElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_ComboElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::ComboElement*>(self)->UI::ComboElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_ComboElement_Update(void* selfv, const void* time, void* ui,
                                         void** dlg, const void* statev) {
    reinterpret_cast<UI::ComboElement*>(selfv)->UI::ComboElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(statev));
}

// ============================================================================
// Constructors / destructor — DOORS (B4b-2), unchanged
// ============================================================================
// ??0ComboElement@UI@@QEAA@AEBV01@@Z  0x1800751e0 — chains to the out-of-line
// ??0ButtonElement copy ctor, copies the two description handles, then
// deep-copies the three parallel arrays onto zeroed targets.
extern "C" void* RtUi_ComboElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    constexpr int kListDesc = 0xd8, kItemDesc = 0xe0, kStrings = 0xe8, kItemCount = 0xf0,
                  kSprites = 0xf8, kUserData = 0x108, kCurrentItem = 0x118,
                  kOpenDialog = 0x120;
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
static_assert(offsetof(UI::ComboElement, _listDesc) == 0xd8 &&
              offsetof(UI::ComboElement, _openDialog) == 0x120,
              "the copy-ctor door's offsets are the class's");

// ============================================================================
// ??1ComboElement@UI@@UEAA@XZ   0x180112490
// ============================================================================
// Close the popup if it is still open — through the UserInterface the
// DialogInstance itself points at (`_openDialog->[+0x10]`), NOT through any
// cached manager — then destroy the three arrays and chain to ~Element.
extern "C" void RtUi_ComboElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtComboElement();

    void* open = P(self, 0x120);                            // _openDialog
    if (open != nullptr) {
        void* slot = open;
        RtAddr<CloseDialogFn>(0x180120f20ull)(Pc(open, 0x10), &slot);
    }

    RtAddr<ArrayDtorFn>(0x180024aa0ull)(self + 0x108);      // _userData
    RtAddr<ArrayDtorFn>(0x180037310ull)(self + 0xf8);       // _sprites
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(self + 0xe8);       // _strings

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

    RegisterRuntimeExport("??0ComboElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ComboElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x1800751e0ull, (void*)&RtUi_ComboElement_CopyCtor);

    RegisterRuntimeExport("??1ComboElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ComboElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180112490ull, (void*)&RtUi_ComboElement_Dtor);
    return n;
}
