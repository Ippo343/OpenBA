#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// MessageDialog::Slot4 (0x14006D140) — Setup
// Finds buttonCancel, buttonOk, buttonYes, buttonNo elements by name.
// Sets element IDs: Cancel=0, Ok=1, Yes=2, No=3.
void MessageDialog_Slot4(MessageDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find and ID each button element
    struct { const char* name; int id; } buttons[] = {
        { "buttonCancel", 0 },
        { "buttonOk",     1 },
        { "buttonYes",    2 },
        { "buttonNo",     3 },
    };

    for (auto& btn : buttons) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, btn.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (elem) {
            fn_Element_SetElementId(elem, btn.id);
        }
    }
}

// ============================================================
// MessageDialog::Slot5 (vtable +0x28) — Shutdown
// ============================================================
// 3-byte no-op: XOR EAX,EAX; RET. Returns 0.
// Original: 0x14006D0C0.

int MessageDialog_Slot5(MessageDialog* /*self*/) {
    return 0;
}

// ============================================================
// MessageDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// Dispatches button clicks to the appropriate result handler.
// When msg==0x12 (button click):
//   id=0 → vtable[+0x58] (ButtonCancel/Slot11)
//   id=1 → vtable[+0x50] (ButtonOk/Slot10)
//   id=2 → vtable[+0x60] (ButtonYes/Slot12)
//   id=3 → vtable[+0x68] (ButtonNo/Slot13)
//
// Original: 0x14006D070.

void MessageDialog_Slot8(MessageDialog* self, void* /*param2*/, int param3, int param4) {
    if (param3 != 0x12)
        return;

    // Dispatch through vtable — allows subclass overrides
    uintptr_t* vtable = *(uintptr_t**)self;
    typedef void (*fn_Handler_t)(void*);

    switch (param4) {
    case 0:  // Cancel → vtable[+0x58 / 8 = 11]
        ((fn_Handler_t)vtable[0x58 / 8])(self);
        break;
    case 1:  // Ok → vtable[+0x50 / 8 = 10]
        ((fn_Handler_t)vtable[0x50 / 8])(self);
        break;
    case 2:  // Yes → vtable[+0x60 / 8 = 12]
        ((fn_Handler_t)vtable[0x60 / 8])(self);
        break;
    case 3:  // No → vtable[+0x68 / 8 = 13]
        ((fn_Handler_t)vtable[0x68 / 8])(self);
        break;
    }
}

// ============================================================
// MessageDialog::Slot10 (vtable +0x50) — ButtonOk
// ============================================================
// Sets result=1 at self+0x24, closes dialog.
// Original: 0x14006CFF0.

void MessageDialog_Slot10(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 1;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot11 (vtable +0x58) — ButtonCancel
// ============================================================
// Sets result=0 at self+0x24, closes dialog.
// Original: 0x14006D030.

void MessageDialog_Slot11(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 0;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot12 (vtable +0x60) — ButtonYes
// ============================================================
// Sets result=2 at self+0x24, closes dialog.
// Original: 0x14006CFB0.

void MessageDialog_Slot12(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 2;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// MessageDialog::Slot13 (vtable +0x68) — ButtonNo
// ============================================================
// Sets result=3 at self+0x24, closes dialog.
// Original: 0x14006CF70.

void MessageDialog_Slot13(MessageDialog* self) {
    *(uint32_t*)((uintptr_t)self + 0x24) = 3;
    uint8_t selfIBP[16];
    fn_InternalBasePtr_Ctor(selfIBP, self);
    fn_UI_CloseDialog(*(void**)((uintptr_t)self + 0x10), selfIBP);
}

// ============================================================
// STANDALONE EXE: vtable + ctor + factory wiring
// ============================================================
// In standalone mode the original exe ctor at 0x14006d300 doesn't exist —
// fn_MessageDialog_Ctor used to resolve to NULL. The conflict-confirmation
// path in OptionsDialog::OnInputBindClick calls it (at line 1577) when a new
// keybind clashes with an existing one, → JIT crash at addr 0.
//
// Original ctor (decompiled @ 0x14006d300): three-liner that calls
// DialogInstance::DialogInstance and patches the vtable to MessageDialog's
// own at 0x14015c480. No additional fields to init beyond DialogInstance's.
//
// Mirrors the OptionInputBindDialog standalone wiring above.
#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include <cstdio>

namespace {

void* GetMessageDialogVtable();  // forward decl

void* MD_Stub_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *(void**)self = GetMessageDialogVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

const char* MD_Stub_Slot1_TypeName(void* /*self*/) { return "MessageDialog"; }

uint32_t MD_Stub_Slot2_TypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MessageDialog");
    return cached;
}

template <int SlotIdx>
void MD_Stub_RuntimeThunk(void* self, ...) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        printf("[MessageDialog::Slot%d_Stub] self=%p (runtime thunk not wired)\n",
                    SlotIdx, self);
    }
}

// Trampolines wrap typed reimpls into the void*-based vtable ABI.
void MD_Slot4_Setup(void* self) {
    MessageDialog_Slot4(reinterpret_cast<MessageDialog*>(self));
}
int MD_Slot5_Shutdown(void* self) {
    return MessageDialog_Slot5(reinterpret_cast<MessageDialog*>(self));
}
void MD_Slot8_OnCommand(void* self, void* element, int eventType, int eventData) {
    MessageDialog_Slot8(reinterpret_cast<MessageDialog*>(self),
                        element, eventType, eventData);
}
void MD_Slot10_OnOk(void* self) {
    MessageDialog_Slot10(reinterpret_cast<MessageDialog*>(self));
}
void MD_Slot11_OnCancel(void* self) {
    MessageDialog_Slot11(reinterpret_cast<MessageDialog*>(self));
}
void MD_Slot12_OnYes(void* self) {
    MessageDialog_Slot12(reinterpret_cast<MessageDialog*>(self));
}
void MD_Slot13_OnNo(void* self) {
    MessageDialog_Slot13(reinterpret_cast<MessageDialog*>(self));
}

void* g_MessageDialog_Vtable_Standalone[14] = {
    reinterpret_cast<void*>(&MD_Stub_Dtor),                  // +0x00 Dtor
    reinterpret_cast<void*>(&MD_Stub_Slot1_TypeName),        // +0x08 TypeName
    reinterpret_cast<void*>(&MD_Stub_Slot2_TypeId),          // +0x10 TypeId
    reinterpret_cast<void*>(&MD_Stub_RuntimeThunk<3>),       // +0x18 Persist
    reinterpret_cast<void*>(&MD_Slot4_Setup),                // +0x20 Setup
    reinterpret_cast<void*>(&MD_Slot5_Shutdown),             // +0x28 Shutdown
    reinterpret_cast<void*>(&MD_Stub_RuntimeThunk<6>),       // +0x30 OnUpdateLayout
    reinterpret_cast<void*>(&MD_Stub_RuntimeThunk<7>),       // +0x38 OnUpdate / Load
    reinterpret_cast<void*>(&MD_Slot8_OnCommand),            // +0x40 OnCommand
    reinterpret_cast<void*>(&MD_Stub_RuntimeThunk<9>),       // +0x48 OnSerializeSettings
    reinterpret_cast<void*>(&MD_Slot10_OnOk),                // +0x50 OnOk
    reinterpret_cast<void*>(&MD_Slot11_OnCancel),            // +0x58 OnCancel
    reinterpret_cast<void*>(&MD_Slot12_OnYes),               // +0x60 OnYes
    reinterpret_cast<void*>(&MD_Slot13_OnNo),                // +0x68 OnNo
};

void* GetMessageDialogVtable() {
    return reinterpret_cast<void*>(g_MessageDialog_Vtable_Standalone);
}

// Clean reimpl of FUN_14006d300. Trivial — DialogInstance ctor + vtable patch.
// Signature matches fn_MessageDialog_Ctor_t in engine_funcs.h:
//   (mem, userInterface, externalResource, internalBasePtrOrNull) -> void*
void* MessageDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                    void* parentIBP) {
    if (!self) return nullptr;
    printf("[MessageDialog_Ctor] self=%p ui=%p extRes=%p parentIBP=%p\n",
                self, ui, extRes, parentIBP);

    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;

    if (fn_DialogInstance_Ctor && extRes) {
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy,
                               nullptr, nullptr, 0);
    } else {
        printf("[MessageDialog_Ctor] skipping DialogInstance::DialogInstance "
                    "(fn=%p extRes=%p) — bare-bones init\n",
                    (void*)fn_DialogInstance_Ctor, extRes);
    }

    *reinterpret_cast<void**>(self) = GetMessageDialogVtable();
    return self;
}

void* MD_Factory_Bare(void* /*loop*/) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(MessageDialog), 8);
    if (!p) return nullptr;
    for (size_t off = 0; off < sizeof(MessageDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(p) + off) = 0;
    }
    *reinterpret_cast<void**>(p) = GetMessageDialogVtable();
    return p;
}

void* MD_CreateInPlace_Bare(void* mem) {
    if (!mem) return nullptr;
    for (size_t off = 0; off < sizeof(MessageDialog); off += 8) {
        *reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(mem) + off) = 0;
    }
    *reinterpret_cast<void**>(mem) = GetMessageDialogVtable();
    return mem;
}

}  // namespace

extern "C" void RegisterMessageDialogVtable() {
    RegisterExeAddr(0x14015c480,
                    reinterpret_cast<uintptr_t>(g_MessageDialog_Vtable_Standalone));
    RegisterExeAddr(0x14006d300,
                    reinterpret_cast<uintptr_t>(&MessageDialog_Ctor_Standalone));
}

extern "C" void* MessageDialog_StandaloneFactory(void) {
    return MD_Factory_Bare(nullptr);
}
extern "C" void* MessageDialog_StandaloneCreateInPlace(void* memory) {
    return MD_CreateInPlace_Bare(memory);
}

REGISTER_TYPE(MessageDialog, "UI::DialogInstance",
              &MessageDialog_StandaloneFactory,
              &MessageDialog_StandaloneCreateInPlace);

#endif  // STANDALONE_EXE
