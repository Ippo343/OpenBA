// ============================================================================
// runtime/src/ui/combodialog.cpp — VTABLE BATCH 9
//   UI::ComboDialog, 6 of 10 vtable slots (0, 1, 2, 6, 7, 8). Slots 3-5 and 9
//   (Persist/OnOpened/OnClosed/OnSerializeSettings, all inherited
//   DialogInstance bodies) are already ported elsewhere; the ctor at
//   0x180111d00 belongs to the UI-text agent (v5) per the batch brief and is
//   NOT in this file.
// ============================================================================
//
// ??_7ComboDialog@UI@@6B@ (retail 0x1801590e0), 10 slots:
//   0  deleting dtor    0x180074a10  deleting_dtor.cpp row (sizeof 0xd0)
//   1  TypeName         0x180074760  here
//   2  Type             0x180074740  here
//   3  Persist          0x180114fa0  already ported (DialogInstance)
//   4  OnOpened         0x180114fb0  already ported (DialogInstance)
//   5  OnClosed         0x180115330  already ported (DialogInstance)
//   6  OnUpdateLayout   0x180111cb0  here
//   7  OnUpdate         0x180111bd0  here
//   8  OnElementEvent   0x180111af0  here
//   9  OnSerializeSettings 0x180115470  already ported (DialogInstance)
// plus ??1ComboDialog@UI@@UEAA@XZ (0x180111cd0), the deleting-dtor row's
// dtorVa: stamp own vtable, tail-call ~DialogInstance (already ported,
// runtime/src/ui/dialog.cpp).
//
// LAYOUT. `DialogInstance` is 0xb8 bytes (runtime/types/ui/DialogInstance.h);
// this class adds THREE pointers, to 0xd0 (the deleting destructor's own
// `MOV EDX,0xd0`):
//   +0xb8  ComboDescription*  _description   (read by OnElementEvent's +0xd8)
//   +0xc0  DialogController*  _controller    (read by OnElementEvent)
//   +0xc8  ComboElement*      _comboElement  (this+200 in the decompile)
//
// EVERY NON-SLOT HELPER THESE THREE BODIES REACH IS ALREADY PORTED:
//   ?SetUpperLeft@DialogInstance@UI@@       0x180114a60  runtime/src/ui/dialog.cpp
//   ?SetCurrentItem@ComboElement@UI@@       0x180112540  runtime/src/ui/widgets_input.cpp
//   ?Play@UserInterface@UI@@                0x1801223d0  runtime/src/ui/userinterface.cpp
//   ?CloseDialog@UserInterface@UI@@         0x180120f20  runtime/src/ui/userinterface_close.cpp
// so this file, like external_list.cpp and compressedcopyfilestream.cpp in
// this same batch, is composition only.
//
// ---------------------------------------------------------------------------
// ?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ   0x180111cb0
// ---------------------------------------------------------------------------
// A pure tail jump: `SetUpperLeft(this, this->_root->+0x68, this->_root->+0x74,
// false)`, where `this->_root` (DialogInstance +0x50) is read straight off
// the disassembly — R9D(centered)=0 is set BEFORE R8D/RDX, and RDX/R8D come
// from the SAME object, +0x68 first:
//     MOV RAX,[this+0xc8]          ; the same object OnElementEvent reads,
//                                    though unused by name here -- see below
//     XOR R9D,R9D
//     MOV R8D,[RAX+0x74]
//     MOV EDX,[RAX+0x68]
//     JMP SetUpperLeft
// *** CORRECTION AGAINST THE DECOMPILE, READ OFF THE RAW BYTES ***: Ghidra's C
// prints `*(this+200)` (i.e. +0xc8, `_comboElement`) as the object read here,
// matching the disassembly's `[RCX+0xc8]` exactly -- so OnUpdateLayout reads
// the SAME `_comboElement` pointer OnElementEvent does, not `_root`. Recorded
// because the header comment for `_root` (+0x50) does not apply to this call.
//
// ---------------------------------------------------------------------------
// ?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z   0x180111bd0
// ---------------------------------------------------------------------------
// The `Time&` argument is never read (retail computes everything from the
// Input::Device singleton and DialogInstance's own `_root`).
//
//   mx = (int)((float)(int16)Input::Device::_singleton[0x109c] * _root->+0xdc)
//   my = (int)((float)(int16)Input::Device::_singleton[0x10b4] * _root->+0xdc)
//   if (no mouse button down at 0x10fe/0x1116/0x112e) return;
//   flags = this->_flags (+0x20)
//   if (!(flags & 4)) {
//       if (flags & 8) { if (this->_comboElement->vt[0x48]()) return; }
//       else { rect = this->_comboElement;
//              if (mx in [rect->+0x68, rect->+0x70] &&
//                  my in [rect->+0x6c, rect->+0x74]) return; }
//   }
//   if (this->_controller->+0x120 != 0) this->_controller->+0x120 = 0;
//   CloseDialog(this->_ui, &this)
//
// *** SAME CORRECTION AS ABOVE ***: the rect/vtable-call subject is
// `this->_comboElement` (+0xc8 via `MOV R9,[RBX+0x10]` for the scale factor
// only; the rect/Contains subject is `[RBX+0x50]`). Both offsets are read
// verbatim below with their own citation.
//
// ---------------------------------------------------------------------------
// ?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z
// 0x180111af0
// ---------------------------------------------------------------------------
// Ghidra drops the InternalPtr<Element> argument (arg2, unused by the body)
// and the trailing two ints + void* (also unused) entirely --
// reference_ghidra_dropped_args.md again. Read off the disassembly, the
// live 5-argument shape is `(this, elem/*unused*/, evt, stage, value)`, with
// two more unused ints and an unused void* past `value`:
//
//   if (evt != 0x14 || stage != 0) return;
//   combo = this->_comboElement (+0xc8)
//   ComboElement::SetCurrentItem(combo, value)
//   UserInterface::Play(this->_ui, 5)
//   ComboElement::SetCurrentItem(combo, this->_description->+0xd8)
//   controller = this->_controller (+0xc0)
//   local = this->_comboElement (+0xc8)     // re-read, same value
//   controller->vt[0x40](controller, &local, 0x14, local->+0x88, value, 0, 0,
//                         <uninitialised>)
//   *** SLOT 0x40, NOT 0x48. *** runtime/types/ui/DialogInstance.h's own
//   `kControllerVtblOnElementEvent = 0x48` is DialogInstance's iteration loop
//   calling through a GENERIC DialogController; the disassembly here
//   (`CALL qword ptr [RAX+0x40]`, confirmed against `?OnElementEvent@
//   DialogInstance@UI@@`'s own dispatch at 0x180114f5e which uses 0x48) shows
//   ComboDialog calling its OWN controller's slot 0x40 (`kControllerVtblOnUpdate`)
//   with this OnElementEvent-shaped argument list instead. Transcribed exactly
//   as shipped -- the concrete controller subclass ComboDialog drives is free
//   to put a different signature at that slot than the generic base class
//   documents, and nothing in this lane's scope needs to resolve which
//   subclass that is.
//   if (this->_comboElement->+0x120 != 0) this->_comboElement->+0x120 = 0;
//   CloseDialog(this->_ui, &this)
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ComboDialog.h"

namespace RtUI {

namespace {

void* VtComboDialog() { return RT_DATA_SLOT(void*, "??_7ComboDialog@UI@@6B@"); }
const uint8_t* ComboDialogTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@ComboDialog@UI@@0VBaseTypeInfo@Object@@A");
}


using SetCurrentItemFn = void (*)(void* combo, int32_t index);
using PlayFn           = void (*)(void* ui, int32_t soundId);
using CloseDialogFn    = void (*)(void* ui, void** dlgPtr);

}  // namespace

// ---- ??1ComboDialog@UI@@UEAA@XZ   0x180111cd0 -----------------------------
// A DTOR DOOR (RULE B6-3): stamps the minted table and chains to
// ~DialogInstance by name. The C++ `~ComboDialog` below is empty.
extern "C" void RtUi_ComboDialog_Dtor(void* selfv) {
    void** v = reinterpret_cast<void**>(selfv);
    *v = VtComboDialog();                                            // 180111cd7
    RT_EXPORT_ONCE(void (*)(void*), "??1DialogInstance@UI@@UEAA@XZ")(selfv);  // 0x180114d40
}

// Other blocks' doors the member bodies reach (ComboElement, UserInterface),
// file-scope so that no member keeps a static of its own (RULE B6-5).
namespace {
inline void ComboSetCurrentItem(void* combo, int32_t index) {
    RT_EXPORT_ONCE(SetCurrentItemFn, "?SetCurrentItem@ComboElement@UI@@QEAAXH@Z")(combo, index);
}
inline void UiPlay(void* ui, int32_t sound) {
    RT_EXPORT_ONCE(PlayFn, "?Play@UserInterface@UI@@QEAAXH@Z")(ui, sound);
}
inline void UiCloseDialog(void* ui, void** dlgPtr) {
    RT_EXPORT_ONCE(CloseDialogFn,
        "?CloseDialog@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z")
        (ui, dlgPtr);
}
}  // namespace

}  // namespace RtUI

// ############################################################################
// ENDGAME B.3.6 block B — the five own slots are UI::ComboDialog members now
// (runtime/types/ui/ComboDialog.h). The raw offsets became the named fields:
// +0x10 `_ui` (the old code called it "combo"; it is DialogInstance::_ui, the
// UserInterface whose +0xdc is the UI scale), +0x20 `_flags`, +0x50 `_root`,
// +0xb8 `_list`, +0xc0 `_parentDialog`, +0xc8 `_combo`. Calls onto THIS
// block's classes became member calls:
//   SetUpperLeft by name              -> SetUpperLeft(x, y, false)    (non-virtual)
//   `_parentDialog` vtable +0x40      -> _parentDialog->OnElementEvent(...)  (VIRTUAL,
//                                        DialogInstance slot 8)
// ############################################################################
namespace UI {

// ?TypeName@ComboDialog@UI@@UEBAPEBDXZ   0x180074760
const char* ComboDialog::TypeName() const {
    const char* v = nullptr;
    __builtin_memcpy(&v, RtUI::ComboDialogTypeInfo() + 8, sizeof v);
    return v;
}
// ?Type@ComboDialog@UI@@UEBAIXZ          0x180074740
uint32_t ComboDialog::Type() const {
    uint32_t v = 0;
    __builtin_memcpy(&v, RtUI::ComboDialogTypeInfo(), sizeof v);
    return v;
}

ComboDialog::~ComboDialog() {}   // see the dtor door above

// ?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ   0x180111cb0 — a tail JMP into
// SetUpperLeft at the combo element's (left, bottom).
void ComboDialog::OnUpdateLayout() {
    const uint8_t* combo = static_cast<const uint8_t*>(_combo);
    const int32_t x = *reinterpret_cast<const int32_t*>(combo + 0x68);
    const int32_t y = *reinterpret_cast<const int32_t*>(combo + 0x74);
    SetUpperLeft(x, y, false);                                       // 180111cc1 JMP
}

// ?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z   0x180111bd0
// A click anywhere OUTSIDE the drop-down closes it.
//
// *** CORRECTED, B.3.6 block B: the root's Contains (Element slot 9, +0x48)
// takes (x, y, UserInterface*). 0x180111c50 `CALL [RAX+0x48]` is reached with
// EDX = mx (0x180111c0d), R8D = my (0x180111c1c) and R9 = `_ui`
// (0x180111bee) still live; the port passed `this` only and let three
// registers of garbage through — the register-passthrough arg drop
// (feedback_ghidra_dropped_args.md). Now passed, as UI::DialogInstance::
// Contains passes them. ***
void ComboDialog::OnUpdate(const System::Time& /*time*/) {
    void** devSlot = RT_DATA_SLOT(void**, "?_singleton@Device@Input@@0PEAV12@EA");
    uint8_t* dev = devSlot ? *reinterpret_cast<uint8_t**>(devSlot) : nullptr;
    if (dev == nullptr) return;   // pre-existing; retail derefs it unconditionally

    const float scale = *reinterpret_cast<const float*>(static_cast<const uint8_t*>(_ui) + 0xdc);
    const int16_t rawMx = *reinterpret_cast<int16_t*>(dev + 0x109c);
    const int16_t rawMy = *reinterpret_cast<int16_t*>(dev + 0x10b4);
    const int32_t mx = static_cast<int32_t>(static_cast<float>(rawMx) * scale);
    const int32_t my = static_cast<int32_t>(static_cast<float>(rawMy) * scale);

    const bool anyButtonDown = dev[0x10fe] != 0 || dev[0x1116] != 0 || dev[0x112e] != 0;
    if (!anyButtonDown) return;                                          // 180111c33

    if ((_flags & 0x4) == 0) {                                           // 180111c3f
        uint8_t* root = static_cast<uint8_t*>(_root);
        if (_flags & 0x8) {                                              // 180111c47
            using ContainsFn = bool (*)(void*, int32_t, int32_t, void*);
            void** vt = *reinterpret_cast<void***>(root);
            if (reinterpret_cast<ContainsFn>(vt[0x48 / 8])(root, mx, my, _ui))
                return;                                                  // 180111c55
        } else {
            const int32_t l = *reinterpret_cast<int32_t*>(root + 0x68);
            const int32_t r = *reinterpret_cast<int32_t*>(root + 0x70);
            const int32_t t = *reinterpret_cast<int32_t*>(root + 0x6c);
            const int32_t b = *reinterpret_cast<int32_t*>(root + 0x74);
            if (mx >= l && mx <= r && my >= t && my <= b) return;         // 180111c71
        }
    }

    int64_t* resultSlot = reinterpret_cast<int64_t*>(static_cast<uint8_t*>(_combo) + 0x120);
    if (*resultSlot != 0) *resultSlot = 0;                                // 180111c84

    void* selfCopy = this;
    RtUI::UiCloseDialog(_ui, &selfCopy);                                  // 180111c9d
}

// ?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z
//      0x180111af0 — a list pick: set the combo's item, click, forward the
// event to the dialog that owns the combo, close.
void ComboDialog::OnElementEvent(Object::InternalPtr<Element>* /*element*/,
                                 int32_t evt, int32_t stage, int32_t value,
                                 int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (evt != 0x14 || stage != 0) return;                                  // 180111af4/180111b05

    uint8_t* combo = static_cast<uint8_t*>(_combo);
    RtUI::ComboSetCurrentItem(combo, value);                                // 180111b25
    RtUI::UiPlay(_ui, 5);                                                   // 180111b33

    const int32_t listIndex =
        *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(_list) + 0xd8);
    RtUI::ComboSetCurrentItem(combo, listIndex);                            // 180111b4c

    // *** SLOT 0x40 on `_parentDialog`, i.e. DialogInstance slot 8 — see the
    // file banner. The handle is re-read into a local whose ADDRESS is passed.
    Object::InternalPtr<Element> local(static_cast<Element*>(_combo));
    _parentDialog->OnElementEvent(&local, 0x14,
                                  *reinterpret_cast<const int32_t*>(combo + 0x88),
                                  value, 0, 0, nullptr);                    // 180111b8a

    int64_t* resultSlot = reinterpret_cast<int64_t*>(combo + 0x120);
    if (*resultSlot != 0) *resultSlot = 0;                                  // 180111ba2

    void* selfCopy = this;
    RtUI::UiCloseDialog(_ui, &selfCopy);                                    // 180111bb7
}

}  // namespace UI

// The registered doors: QUALIFIED delegation (the minted table's slots
// resolve back to these doors).
namespace {
inline UI::ComboDialog* AsCombo(void* p) {
    return reinterpret_cast<UI::ComboDialog*>(p);           // B.3.6 bridge
}
}  // namespace

extern "C" const char* RtUi_ComboDialog_TypeName(const void* self) {
    return reinterpret_cast<const UI::ComboDialog*>(self)->UI::ComboDialog::TypeName();   // B.3.6 bridge
}
extern "C" uint32_t RtUi_ComboDialog_Type(const void* self) {
    return reinterpret_cast<const UI::ComboDialog*>(self)->UI::ComboDialog::Type();   // B.3.6 bridge
}
extern "C" void RtUi_ComboDialog_OnUpdateLayout(void* self) {
    AsCombo(self)->UI::ComboDialog::OnUpdateLayout();
}
extern "C" void RtUi_ComboDialog_OnUpdate(void* self, const void* time) {
    AsCombo(self)->UI::ComboDialog::OnUpdate(*static_cast<const System::Time*>(time));
}
// The full eight-argument retail signature (the door used to spell only the
// first five; the remaining three were never read and still are not).
extern "C" void RtUi_ComboDialog_OnElementEvent(void* self, void* element,
                                                int32_t a, int32_t b, int32_t c,
                                                int32_t d, int32_t e, void* userData) {
    AsCombo(self)->UI::ComboDialog::OnElementEvent(
        static_cast<Object::InternalPtr<UI::Element>*>(element), a, b, c, d, e, userData);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiComboDialogRegister() {
    using namespace RtUI;
    int n = 0;

    RegisterRuntimeExport("?TypeName@ComboDialog@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ComboDialog_TypeName); ++n;
    RegisterRuntimeAddr(0x180074760ull, (void*)&RtUi_ComboDialog_TypeName);

    RegisterRuntimeExport("?Type@ComboDialog@UI@@UEBAIXZ",
                          (void*)&RtUi_ComboDialog_Type); ++n;
    RegisterRuntimeAddr(0x180074740ull, (void*)&RtUi_ComboDialog_Type);

    RegisterRuntimeExport("??1ComboDialog@UI@@UEAA@XZ",
                          (void*)&RtUi_ComboDialog_Dtor); ++n;
    RegisterRuntimeAddr(0x180111cd0ull, (void*)&RtUi_ComboDialog_Dtor);

    RegisterRuntimeExport("?OnUpdateLayout@ComboDialog@UI@@UEAAXXZ",
                          (void*)&RtUi_ComboDialog_OnUpdateLayout); ++n;
    RegisterRuntimeAddr(0x180111cb0ull, (void*)&RtUi_ComboDialog_OnUpdateLayout);

    RegisterRuntimeExport("?OnUpdate@ComboDialog@UI@@UEAAXAEBVTime@System@@@Z",
                          (void*)&RtUi_ComboDialog_OnUpdate); ++n;
    RegisterRuntimeAddr(0x180111bd0ull, (void*)&RtUi_ComboDialog_OnUpdate);

    RegisterRuntimeExport(
        "?OnElementEvent@ComboDialog@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z",
        (void*)&RtUi_ComboDialog_OnElementEvent); ++n;
    RegisterRuntimeAddr(0x180111af0ull, (void*)&RtUi_ComboDialog_OnElementEvent);

    return n;
}
