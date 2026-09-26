#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// EventPopupDialog::Slot4 (0x140068C20) — Setup
// Finds imageIcon, textEvent, buttonOk elements.
// Reads event entry from world event list using index at self+0xB8.
// Sets imageIcon sprite to "EventIcon<N>", sets textEvent parameter 0
// to event text, sets buttonOk element ID to 200.
void EventPopupDialog_Slot4(EventPopupDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find imageIcon element
    uint8_t retBuf[16];
    { System::String nameStr("imageIcon");
      ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(nameStr)); }  // B.3.6 bridge
    void* imageIcon = *(void**)retBuf;

    // Find textEvent element
    { System::String nameStr("textEvent");
      ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(nameStr)); }  // B.3.6 bridge
    void* textEvent = *(void**)retBuf;

    // Find buttonOk element
    { System::String nameStr("buttonOk");
      ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(self)->FindElement(nameStr)); }  // B.3.6 bridge
    void* buttonOk = *(void**)retBuf;

    // Get event entry from world event list
    // world->+0x138 -> event manager, +0x18 -> event array
    // Each entry is 0x28 bytes; event index at self+0xB8
    void* world = *g_pGameWorldSingleton;
    uint8_t* eventMgr = *(uint8_t**)((uint8_t*)world + 0x138);
    uint8_t* eventArray = *(uint8_t**)(eventMgr + 0x18);
    int32_t eventIndex = *(int32_t*)(s + 0xB8);
    uint8_t* eventEntry = eventArray + (int64_t)eventIndex * 0x28;

    // Format sprite name "EventIcon<iconIdx>"
    uint32_t iconIdx = *(uint32_t*)(eventEntry + 0x20);
    System::String fmtStr = System::String::Format("EventIcon%i", iconIdx);

    // Get current sprite from imageIcon (16 bytes: sheetPtr + index)
    uint8_t oldSprite[16];
    fn_ImageElement_GetSprite(imageIcon, oldSprite);
    void* sheetPtr = *(void**)oldSprite;

    // Build new sprite: same sheet, new index from name
    int newIndex = -1;
    if (sheetPtr) {
        newIndex = reinterpret_cast<const UI::SpriteSheet*>(sheetPtr)->GetSpriteIndexFromName(fmtStr);  // B.3.6 bridge
    }

    // Construct 16-byte Sprite: {sheetPtr, index(4b) + pad(4b)}
    uint8_t newSprite[16];
    *(void**)newSprite = sheetPtr;
    *(int32_t*)(newSprite + 8) = newIndex;
    *(int32_t*)(newSprite + 12) = 0;

    // Set new sprite on imageIcon
    fn_ImageElement_SetSprite(imageIcon, newSprite);

    // Set text parameter 0 on textEvent to event text string at eventEntry+0x08
    fn_TextElement_SetParameter(textEvent, 0, eventEntry + 0x08);

    // Set button ID 200 on buttonOk
    reinterpret_cast<UI::Element*>(buttonOk)->SetElementId(200);  // B.3.6 bridge
}

// ============================================================
// EventPopupDialog::Slot8 (0x140068BD0) — vtable +0x40, OnCommand
// ============================================================
// When msg==0x12 (button click) and id==200 (buttonOk):
//   Wraps self in InternalBasePtr, calls UserInterface::CloseDialog.
//
// Original: 0x140068BD0.

void EventPopupDialog_Slot8(EventPopupDialog* self, void* /*param2*/, int param3, int param4) {
    if (param3 == 0x12 && param4 == 200) {
        uint8_t selfIBP[16];
        new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
        reinterpret_cast<UI::UserInterface*>(*(void**)((uintptr_t)self + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
    }
}

// ============================================================
// STANDALONE EXE: real EventPopupDialog vtable + ctor + registration
// ============================================================
// EventPopupDialog (DialogInstance, 0xC0 bytes, exe vtable 0x14015bd80) — the modal
// popup shown for high-severity events (popup field >= 0). In standalone this was a
// dormant UI_DIALOG_STUB. Promote it to a real per-class vtable + standalone ctor so
// EventNotifyDialog::Slot7's popup path (and any direct opener) can raise it.
//
// DialogInstance slots: idx0 Dtor, idx2 GetTypeId, idx4 Setup (Slot4), idx8 OnCommand
// (Slot8 -> close on buttonOk id=200); idx3/5/6/7/9 are base serialize/thunk no-ops.
// The event index this popup displays is stored at +0xB8 (read by Slot4).

#include "Object/factory_registration.h"
#include <cstddef>

namespace eventpopup_sa {

void* GetVtable();  // fwd

void StubNop(void*, ...) {}
void* Slot1Getter(void*) { return nullptr; }
uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("EventPopupDialog");
    return c;
}
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = GetVtable();
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}
void W_Slot4(void* self) { if (self) EventPopupDialog_Slot4(reinterpret_cast<EventPopupDialog*>(self)); }
void W_Slot8(void* self, void* e, int msg, int id) {
    if (self) EventPopupDialog_Slot8(reinterpret_cast<EventPopupDialog*>(self), e, msg, id);
}

static void* g_vt[10];
struct Init {
    Init() {
        for (int i = 0; i < 10; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[1] = reinterpret_cast<void*>(&Slot1Getter);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);
        g_vt[8] = reinterpret_cast<void*>(&W_Slot8);
    }
} g_init;

void* GetVtable() { return reinterpret_cast<void*>(g_vt); }

// Standalone ctor: DialogInstance base ctor + our vtable + store the event index
// at +0xB8 (Slot4 reads it to select the event entry to display).
void* Ctor(void* self, void* ui, void* extRes, void* parentIBP, int eventIndex) {
    if (!self) return nullptr;
    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;
    if (fn_DialogInstance_Ctor && extRes)
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy, nullptr, nullptr, 0);
    *reinterpret_cast<void**>(self) = GetVtable();
    *(int32_t*)((uintptr_t)self + 0xB8) = eventIndex;
    return self;
}

void Zero(void* p) { for (size_t o = 0; o < (size_t)0xC0; o += 8) *reinterpret_cast<uint64_t*>((char*)p + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0xC0, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace eventpopup_sa

REGISTER_TYPE(EventPopupDialog, "UI::DialogInstance",
              &eventpopup_sa::Factory, &eventpopup_sa::InPlace);

extern "C" void* EventPopupDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                                  void* parentIBP, int eventIndex) {
    return eventpopup_sa::Ctor(self, ui, extRes, parentIBP, eventIndex);
}
extern "C" void* EventPopupDialog_GetStandaloneVtable() { return eventpopup_sa::GetVtable(); }

