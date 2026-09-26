// code/Game/Dialog/optioninputbinddialog.cpp
// Clean reimplementation of OptionInputBindDialog methods.
//
// OptionInputBindDialog (0xC8 bytes): Key binding capture dialog.
// Extends MessageDialog (which extends DialogInstance at 0xB8 base).
//
// Layout:
//   +0xB8  (from MessageDialog, uninitialized)
//   +0xC0  keyIndex (int32): captured key/button index
//   +0xC4  inputType (int32): 0=none, 1=keyboard, 2=mouse, -1=initial
//
// The dialog captures a single keypress or mouse button click and stores
// the result. The parent OptionsDialog reads +0xC0 and +0xC4 after close.

#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Helper: Find element by name in dialog instance
// ============================================================
static void* FindDialogElem(void* dialogInst, const char* name) {
    System::String nameStr(name);
    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
    return (void*)result;
}

// ============================================================
// Startup (vtable +0x20): Set up input bind dialog UI
// ============================================================
// Original: 0x14006d140
//
// Finds buttonCancel/Ok/Yes/No elements by name, sets element IDs 0-3.
// This is identical to MessageDialog::Slot4 — the base class setup.

void OptionInputBindDialog_Startup(OptionInputBindDialog* self) {
    void* cancelBtn = FindDialogElem(self, "buttonCancel");
    void* okBtn     = FindDialogElem(self, "buttonOk");
    void* yesBtn    = FindDialogElem(self, "buttonYes");
    void* noBtn     = FindDialogElem(self, "buttonNo");

    if (cancelBtn) reinterpret_cast<UI::Element*>(cancelBtn)->SetElementId(0);  // B.3.6 bridge
    if (okBtn)     reinterpret_cast<UI::Element*>(okBtn)->SetElementId(1);  // B.3.6 bridge
    if (yesBtn)    reinterpret_cast<UI::Element*>(yesBtn)->SetElementId(2);  // B.3.6 bridge
    if (noBtn)     reinterpret_cast<UI::Element*>(noBtn)->SetElementId(3);  // B.3.6 bridge
}

// ============================================================
// Shutdown (vtable +0x28, exe 0x14006d0c0): RET 0 (3 bytes)
// ============================================================
// No-op shutdown; nothing to clean up.
int OptionInputBindDialog_Shutdown(OptionInputBindDialog* self) {
    (void)self;
    return 0;
}

// ============================================================
// Load (vtable +0x38): Scan for key/mouse input releases
// ============================================================
// Original: 0x140071c80
//
// Polls Input::Device::Released for all keyboard keys (0..0x68)
// and mouse buttons (4..0xB). When a release is detected, stores
// the key index at +0xC0, input type (1=keyboard, 2=mouse) at +0xC4,
// and calls vtable[10] (OnOk) to close the dialog with result=1.

void OptionInputBindDialog_Load(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* inputDevice = *g_pInputDeviceSingleton;

    // Scan keyboard keys (InputDevice=1, keys 0..0x68)
    for (int key = 0; key < 0x69; key++) {
        if (fn_Input_Device_Released(inputDevice, 1, 0, key)) {
            *(int*)(s + 0xC4) = 1;  // keyboard
            *(int*)(s + 0xC0) = key;
            // Call vtable[10] = OnOk
            void** vtable = *(void***)self;
            typedef void (*fn_OnOk_t)(void*);
            ((fn_OnOk_t)vtable[10])(self);
            return;
        }
    }

    // Scan mouse buttons (InputDevice=2, buttons 4..0xB)
    for (int btn = 4; btn <= 0xB; btn++) {
        if (fn_Input_Device_Released(inputDevice, 2, 0, btn)) {
            *(int*)(s + 0xC4) = 2;  // mouse
            *(int*)(s + 0xC0) = btn;
            // Call vtable[10] = OnOk
            void** vtable = *(void***)self;
            typedef void (*fn_OnOk_t)(void*);
            ((fn_OnOk_t)vtable[10])(self);
            return;
        }
    }
}

// ============================================================
// OnOk (vtable +0x50): Set result value 1 (OK), close dialog
// ============================================================
// Original: 0x14006cff0

void OptionInputBindDialog_OnOk(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 1;
    uint64_t basePtr = 0;
    new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* ui = *(void**)(s + 0x10);
    reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge
}

// ============================================================
// OnCancel (vtable +0x58): Set result value 0 (Cancel), close dialog
// ============================================================
// Original: 0x14006d030

void OptionInputBindDialog_OnCancel(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 0;
    uint64_t basePtr = 0;
    new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* ui = *(void**)(s + 0x10);
    reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge
}

// ============================================================
// OnButton2 (vtable +0x60): Set result value 2, close dialog
// ============================================================
// Original: 0x14006cfb0

void OptionInputBindDialog_OnButton2(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 2;
    uint64_t basePtr = 0;
    new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* ui = *(void**)(s + 0x10);
    reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge
}

// ============================================================
// OnButton3 (vtable +0x68): Set result value 3, close dialog
// ============================================================
// Original: 0x14006cf70

void OptionInputBindDialog_OnButton3(OptionInputBindDialog* self) {
    uintptr_t s = (uintptr_t)self;
    *(int*)(s + 0x24) = 3;
    uint64_t basePtr = 0;
    new (&basePtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    void* ui = *(void**)(s + 0x10);
    reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&basePtr));  // B.3.6 bridge
}

// ============================================================
// OptionInputBindDialog::OnEvent (0x14006d070) — Slot8
// ============================================================
// Dispatches button clicks to the appropriate handler via vtable.
// eventType 0x12 = button click:
//   eventData 0 = buttonCancel → OnCancel (sets value=0, closes)
//   eventData 1 = buttonOk     → OnOk (sets value=1, closes)
//   eventData 2 = buttonYes    → OnButton2 (sets value=2, closes)
//   eventData 3 = buttonNo     → OnButton3 (sets value=3, closes)
//
// Original: 0x14006d070 (reads as direct vtable dispatch bytecode).

void OptionInputBindDialog_OnEvent(OptionInputBindDialog* self, void* element, int eventType, int eventData) {
    if (eventType != 0x12) return;

    // Dispatch through vtable to the appropriate handler
    void** vtable = *(void***)self;
    if (eventData == 0) {
        // buttonCancel: vtable[+0x58/8 = slot 11] = OnCancel
        typedef void (*fn_t)(void*);
        ((fn_t)vtable[11])(self);
    } else if (eventData == 1) {
        // buttonOk: vtable[+0x50/8 = slot 10] = OnOk
        typedef void (*fn_t)(void*);
        ((fn_t)vtable[10])(self);
    } else if (eventData == 2) {
        // buttonYes/button2: vtable[+0x60/8 = slot 12] = OnButton2
        typedef void (*fn_t)(void*);
        ((fn_t)vtable[12])(self);
    } else if (eventData == 3) {
        // buttonNo/button3: vtable[+0x68/8 = slot 13] = OnButton3
        typedef void (*fn_t)(void*);
        ((fn_t)vtable[13])(self);
    }
}

// ============================================================
// STANDALONE EXE: vtable + ctor + factory wiring
// ============================================================
// In standalone mode the original exe ctor at 0x140071d30 doesn't exist —
// fn_OptionInputBindDialog_Ctor used to resolve to NULL, and OptionsDialog's
// "click an input bind row to remap" path called 0x0 → JIT crash at addr 0.
//
// Original ctor (decompiled @ 0x140071d30):
//   undefined8 * OptionInputBindDialog_Ctor(self) {
//     UI::DialogInstance::DialogInstance(self, ui, extRes, &localParent, 0,0,0);
//     *self = &g_ReloadDialog_Vtable;          // base MessageDialog vtable (0x14015c480)
//     *self = &vtable_OptionInputBindDialog;   // override (0x14015ce40)
//     *(int*)(self + 0xC0) = 0;
//     *(int*)(self + 0xC4) = 0xFFFFFFFF;       // -1
//     return self;
//   }
//
// Vtable layout (0x14015ce40, 14 slots — 4 more than MessageDialog because
// it adds Load/OnEvent/OnOk-OnButton3 overrides):
//   +0x00 Slot0 ScalarDtor              — exe internal
//   +0x08 Slot1 GetTypeInfo (TypeName)  — exe const-load
//   +0x10 Slot2 GetTypeId               — exe const-load
//   +0x18 Slot3 Persist                 — runtime thunk
//   +0x20 Slot4 Setup                   — REIMPL'd (OptionInputBindDialog_Startup)
//   +0x28 Slot5 Shutdown                — REIMPL'd (OptionInputBindDialog_Shutdown)
//   +0x30 Slot6 OnUpdateLayout          — runtime thunk
//   +0x38 Slot7 OnUpdate (Load)         — REIMPL'd (OptionInputBindDialog_Load)
//   +0x40 Slot8 OnEvent                 — REIMPL'd (OptionInputBindDialog_OnEvent)
//   +0x48 Slot9 OnSerializeSettings     — runtime thunk
//   +0x50 Slot10 OnOk                   — REIMPL'd (OptionInputBindDialog_OnOk)
//   +0x58 Slot11 OnCancel               — REIMPL'd (OptionInputBindDialog_OnCancel)
//   +0x60 Slot12 OnButton2              — REIMPL'd (OptionInputBindDialog_OnButton2)
//   +0x68 Slot13 OnButton3              — REIMPL'd (OptionInputBindDialog_OnButton3)
//
// Mirrors the OptionsDialog standalone wiring at the bottom of optionsdialog.cpp.


#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include "ui/Dialogs.h"

namespace {

void* GetOptionInputBindDialogVtable();  // forward decl

// Slot 0 — Dtor. Same scalar-deleting dtor pattern as OptionsDialog.
void* OIBD_Stub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetOptionInputBindDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Slot 1 — TypeName.
const char* OIBD_Stub_Slot1_TypeName(void* /*self*/) { return "OptionInputBindDialog"; }

// Slot 2 — TypeId (lazy-resolved by name).
uint32_t OIBD_Stub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("OptionInputBindDialog");
    return cached;
}

// Generic logging stub for runtime-thunk slots (3, 6, 9). Logs once per slot.
template <int SlotIdx>
void OIBD_Stub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[OptionInputBindDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
                    SlotIdx, self);
    }
}

// Trampolines: vtable slots are typed as void*, but our reimpls use specific
// typed signatures. Wrap to match the (void*) ABI without exposing the typed
// header to the vtable array (avoids implicit-function-pointer warnings).
void OIBD_Slot4_Startup(void* self) {
    OptionInputBindDialog_Startup(reinterpret_cast<OptionInputBindDialog*>(self));
}
int OIBD_Slot5_Shutdown(void* self) {
    return OptionInputBindDialog_Shutdown(reinterpret_cast<OptionInputBindDialog*>(self));
}
void OIBD_Slot7_Load(void* self) {
    OptionInputBindDialog_Load(reinterpret_cast<OptionInputBindDialog*>(self));
}
void OIBD_Slot8_OnEvent(void* self, void* element, int eventType, int eventData) {
    OptionInputBindDialog_OnEvent(reinterpret_cast<OptionInputBindDialog*>(self),
                                  element, eventType, eventData);
}
void OIBD_Slot10_OnOk(void* self) {
    OptionInputBindDialog_OnOk(reinterpret_cast<OptionInputBindDialog*>(self));
}
void OIBD_Slot11_OnCancel(void* self) {
    OptionInputBindDialog_OnCancel(reinterpret_cast<OptionInputBindDialog*>(self));
}
void OIBD_Slot12_OnButton2(void* self) {
    OptionInputBindDialog_OnButton2(reinterpret_cast<OptionInputBindDialog*>(self));
}
void OIBD_Slot13_OnButton3(void* self) {
    OptionInputBindDialog_OnButton3(reinterpret_cast<OptionInputBindDialog*>(self));
}

void* g_OptionInputBindDialog_Vtable_Standalone[14] = {
    reinterpret_cast<void*>(&OIBD_Stub_Dtor),                  // +0x00 Dtor
    reinterpret_cast<void*>(&OIBD_Stub_Slot1_TypeName),        // +0x08 TypeName
    reinterpret_cast<void*>(&OIBD_Stub_Slot2_TypeId),          // +0x10 TypeId
    reinterpret_cast<void*>(&OIBD_Stub_RuntimeThunk<3>),       // +0x18 Persist
    reinterpret_cast<void*>(&OIBD_Slot4_Startup),              // +0x20 Setup
    reinterpret_cast<void*>(&OIBD_Slot5_Shutdown),             // +0x28 Shutdown
    reinterpret_cast<void*>(&OIBD_Stub_RuntimeThunk<6>),       // +0x30 OnUpdateLayout
    reinterpret_cast<void*>(&OIBD_Slot7_Load),                 // +0x38 OnUpdate / Load
    reinterpret_cast<void*>(&OIBD_Slot8_OnEvent),              // +0x40 OnEvent
    reinterpret_cast<void*>(&OIBD_Stub_RuntimeThunk<9>),       // +0x48 OnSerializeSettings
    reinterpret_cast<void*>(&OIBD_Slot10_OnOk),                // +0x50 OnOk
    reinterpret_cast<void*>(&OIBD_Slot11_OnCancel),            // +0x58 OnCancel
    reinterpret_cast<void*>(&OIBD_Slot12_OnButton2),           // +0x60 OnButton2
    reinterpret_cast<void*>(&OIBD_Slot13_OnButton3),           // +0x68 OnButton3
};

void* GetOptionInputBindDialogVtable() {
    return reinterpret_cast<void*>(g_OptionInputBindDialog_Vtable_Standalone);
}

// In-place ctor — clean reimpl of FUN_140071d30.
// Original mirrors: copy parentIBP qword to a local stack slot, call
// DialogInstance::DialogInstance(self, ui, extRes, &localParent, 0,0,0),
// patch *self with vtable_OptionInputBindDialog, write +0xC0=0, +0xC4=-1.
//
// Signature matches fn_OptionInputBindDialog_Ctor_t in engine_funcs.h:
//   (mem, userInterface, externalResource, internalBasePtrOrNull) -> void*
void* OptionInputBindDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                            void* parentIBP) {
    if (!self) return nullptr;
    printf("[OptionInputBindDialog_Ctor] self=%p ui=%p extRes=%p parentIBP=%p\n",
                self, ui, extRes, parentIBP);

    // Stack copy of the parent IBP qword (matches original SEH frame layout
    // — original loads [R9] to a local before passing &local as R9 to the
    // base ctor).
    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;

    if (fn_DialogInstance_Ctor && extRes) {
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy,
                               nullptr, nullptr, 0);
    } else {
        printf("[OptionInputBindDialog_Ctor] skipping DialogInstance::DialogInstance "
                    "(fn=%p extRes=%p) — bare-bones init\n",
                    (void*)fn_DialogInstance_Ctor, extRes);
    }

    // Set vtable + initial field values per original ctor.
    *reinterpret_cast<void**>(self) = GetOptionInputBindDialogVtable();
    auto* d = reinterpret_cast<OptionInputBindDialog*>(self);
    d->dataC0 = 0;
    d->dataC4 = -1;  // 0xFFFFFFFF — "no key captured yet"
    return self;
}

// Bare factory — used by reflection if .rsc loader ever instantiates this
// type without an external. Currently OptionsDialog::OnInputBindClick always
// builds it via the named ctor path (fn_OptionInputBindDialog_Ctor).
void* OIBD_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(OptionInputBindDialog), 8);
    if (!p) return nullptr;
    for (size_t off = 0; off < sizeof(OptionInputBindDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(p) + off) = 0;
    }
    *reinterpret_cast<void**>(p) = GetOptionInputBindDialogVtable();
    auto* d = reinterpret_cast<OptionInputBindDialog*>(p);
    d->dataC0 = 0;
    d->dataC4 = -1;
    return p;
}

void* OIBD_CreateInPlace_Bare(void* mem) {
    if (!mem) return nullptr;
    for (size_t off = 0; off < sizeof(OptionInputBindDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(mem) + off) = 0;
    }
    *reinterpret_cast<void**>(mem) = GetOptionInputBindDialogVtable();
    auto* d = reinterpret_cast<OptionInputBindDialog*>(mem);
    d->dataC0 = 0;
    d->dataC4 = -1;
    return mem;
}

}  // namespace

// Vtable + ctor address registration. Called from RegisterCodeFunctions in
// standalone/address_registry.cpp.
extern "C" void RegisterOptionInputBindDialogVtable() {
    RegisterExeAddr(0x14015ce40,
                    reinterpret_cast<uintptr_t>(g_OptionInputBindDialog_Vtable_Standalone));
    RegisterExeAddr(0x140071d30,
                    reinterpret_cast<uintptr_t>(&OptionInputBindDialog_Ctor_Standalone));
}

extern "C" void* OptionInputBindDialog_StandaloneFactory(void) {
    return OIBD_Factory_Bare(nullptr);
}
extern "C" void* OptionInputBindDialog_StandaloneCreateInPlace(void* memory) {
    return OIBD_CreateInPlace_Bare(memory);
}

// Real reflection factory (parent class is "MessageDialog" — matches the
// original RegisterType_OptionInputBindDialog at 0x14014c480).
// NOTE: factory_registrations_bulk.cpp still emits a REGISTER_NULL_FACTORY
// for this type, mirroring the OptionsDialog setup that already works in
// production. Object::Factory tolerates the duplicate (the per-class
// REGISTER_TYPE wins).
REGISTER_TYPE(OptionInputBindDialog, "MessageDialog",
              &OptionInputBindDialog_StandaloneFactory,
              &OptionInputBindDialog_StandaloneCreateInPlace);

