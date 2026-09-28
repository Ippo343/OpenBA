#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/EventPopupDialog.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ENDGAME B.3.8 block DLG: EventPopupDialog is a real class (Dialog/EventPopupDialog.h).

// [4] OnOpened (0x140068C20) — Setup
// Finds imageIcon, textEvent, buttonOk elements.
// Reads event entry from world event list using index at self+0xB8.
// Sets imageIcon sprite to "EventIcon<N>", sets textEvent parameter 0
// to event text, sets buttonOk element ID to 200.
void EventPopupDialog::OnOpened() {
    uintptr_t s = (uintptr_t)this;

    // Find imageIcon element
    uint8_t retBuf[16];
    { System::String nameStr("imageIcon");
      ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr)); }  // B.3.6 bridge
    void* imageIcon = *(void**)retBuf;

    // Find textEvent element
    { System::String nameStr("textEvent");
      ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr)); }  // B.3.6 bridge
    void* textEvent = *(void**)retBuf;

    // Find buttonOk element
    { System::String nameStr("buttonOk");
      ::new (retBuf) Object::InternalPtr<UI::Element>(FindElement(nameStr)); }  // B.3.6 bridge
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
    ::new (oldSprite) UI::Sprite(reinterpret_cast<const UI::ImageElement*>(imageIcon)->GetSprite());  // B.3.6 bridge
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
    reinterpret_cast<UI::ImageElement*>(imageIcon)->SetSprite(reinterpret_cast<const UI::Sprite*>(newSprite));  // B.3.6 bridge

    // Set text parameter 0 on textEvent to event text string at eventEntry+0x08
    reinterpret_cast<UI::TextElement*>(textEvent)->SetParameter(0, *reinterpret_cast<const System::String*>(eventEntry + 0x08));  // B.3.6 bridge

    // Set button ID 200 on buttonOk
    reinterpret_cast<UI::Element*>(buttonOk)->SetElementId(200);  // B.3.6 bridge
}

// ============================================================
// [8] OnElementEvent (0x140068BD0) — vtable +0x40, OnCommand
// ============================================================
// When msg==0x12 (button click) and id==200 (buttonOk):
//   Wraps self in InternalBasePtr, calls UserInterface::CloseDialog.
//
// Original: 0x140068BD0.

void EventPopupDialog::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                      int32_t param3, int32_t param4, int32_t /*c*/,
                                      int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 == 0x12 && param4 == 200) {
        uint8_t selfIBP[16];
        new (selfIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        static_cast<UI::UserInterface*>(_ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(selfIBP));  // B.3.6 bridge
    }
}

// ============================================================
// Construction, destruction, reflection
// ============================================================
// EventPopupDialog (DialogInstance, 0xC0 bytes, exe vtable 0x14015bd80): the
// DialogInstance base ctor, then the event index this popup displays at +0xB8
// (OnOpened reads it). EventNotifyDialog's popup branch builds it.

#include "Object/factory_registration.h"
#include <cstddef>

EventPopupDialog::EventPopupDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                                   Object::InternalPtr<UI::DialogInstance> parent,
                                   int32_t index)
    : UI::DialogInstance(ui, dialog, parent, nullptr, nullptr, 0), eventIndex(index) {}

// [0] 0x1400119b0: ~EventPopupDialog -> ~DialogInstance -> ~Internal, MemFree on &1.
Object::Base* EventPopupDialog::DeletingDtor(unsigned flags) {
    this->~EventPopupDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011920: MemAlloc(0xc0, 8), then the ctor.
void* EventPopupDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EventPopupDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) EventPopupDialog());
}
// createInPlace 0x140011970 (the tree's in-place build is kept, B8-13).
void* EventPopupDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) EventPopupDialog());
}

DEFINE_EXE_TYPE(EventPopupDialog, "UI::DialogInstance")
