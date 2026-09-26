// engine/src/ui/eventlogui.cpp
// Clean reimplementation of EventLogUI game logic.
//
// EventLogUI (0x80 bytes) — event log panel.
// Extends DialogController (0x30 base).
// +0x30 = buttonBottom, +0x38 = buttonPrevious, +0x40 = buttonNext,
// +0x48 = scroll offset (int), +0x50/+0x58 = display text element array (ptrs),
// +0x60/+0x68 = display button element array (ptrs),
// +0x70/+0x78 = notification button array (ptrs, dynamic).
//
// Element IDs: buttonPrevious=100, buttonNext=0x65, buttonBottom=0x66,
//              notification buttons=0x67..0x6B, display rows>=0x6C.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)

// ============================================================
// EventLogUI::Slot5 (vtable +0x28) — Setup event log display
// ============================================================
// Finds navigation buttons, "display" container, notification buttons.
// Stores button ptrs, resizes display arrays, sets element IDs.
//
// Original: 0x140067640.

void EventLogUI_Slot5(EventLogUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);
    void* dialogInst = *(void**)(s + 0x18);

    // --- Find navigation buttons ---
    // "buttonPrevious" -> +0x38
    {
        System::String nameStr("buttonPrevious");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
        *(uint64_t*)(s + 0x38) = *(uint64_t*)bpBuf;
    }

    // "buttonNext" -> +0x40
    {
        System::String nameStr("buttonNext");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
        *(uint64_t*)(s + 0x40) = *(uint64_t*)bpBuf;
    }

    // "buttonBottom" -> +0x30
    {
        System::String nameStr("buttonBottom");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
        *(uint64_t*)(s + 0x30) = *(uint64_t*)bpBuf;
    }

    // Set element IDs on buttons
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x38))->SetElementId(100);   // B.3.6 bridge; buttonPrevious = 100
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x40))->SetElementId(0x65);  // B.3.6 bridge; buttonNext = 0x65
    reinterpret_cast<UI::Element*>(*(void**)(s + 0x30))->SetElementId(0x66);  // B.3.6 bridge; buttonBottom = 0x66

    // --- Find "display" container element ---
    void* displayElem;
    {
        System::String nameStr("display");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        void* elem = *(void**)retBuf;
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(elem));   // B.3.3 bridge
        displayElem = *(void**)bpBuf;
    }

    // Resize display text array (+0x50) and display button array (+0x60) to row count
    int displayRowCount = reinterpret_cast<const UI::Element*>(displayElem)->ChildCount();  // B.3.6 bridge
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Both elements are 8-byte InternalBasePtr handles (byte-copy).
    GameColl::ArrayAt<GameColl::Ptr8>(s, 0x50).SetSize(displayRowCount);
    GameColl::ArrayAt<GameColl::Ptr8>(s, 0x60).SetSize(displayRowCount);

    // Iterate display rows: store child text element in +0x50 array,
    // find button child, store in +0x60 array, set element IDs
    int rowCount = *(int*)(s + 0x58);
    for (int i = 0; i < rowCount; i++) {
        // Get child text element (direct child at index i)
        uint8_t childBuf[8];
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(displayElem)->Child(i));  // B.3.6 bridge
        *(uint64_t*)(*(uintptr_t*)(s + 0x50) + i * 8) = *(uint64_t*)childBuf;

        // Get button child: Child(display, i) -> find typed element in that child
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(displayElem)->Child(i));  // B.3.6 bridge
        void* rowChild = *(void**)childBuf;
        // The button child is obtained by IsDerivedFrom check (generic element type)
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rowChild));   // B.3.3 bridge
        *(uint64_t*)(*(uintptr_t*)(s + 0x60) + i * 8) = *(uint64_t*)bpBuf;

        // Set element ID on the button: 0x6C + index
        reinterpret_cast<UI::Element*>(*(void**)(*(uintptr_t*)(s + 0x60) + i * 8))->SetElementId(0x6C + i);  // B.3.6 bridge
    }

    // Reset scroll offset to 0
    *(int*)(s + 0x48) = 0;

    // --- Find "notifybuttons" container in dialog instance ---
    void* notifyContainer;
    {
        System::String nameStr("notifybuttons");
        uint8_t retBuf[16];
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        notifyContainer = *(void**)retBuf;
    }

    // Iterate notification button children, store in dynamic array at +0x70, set IDs
    int notifyCount = reinterpret_cast<const UI::Element*>(notifyContainer)->ChildCount();  // B.3.6 bridge
    void* eventLogSingleton = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);

    for (int i = 0; i < notifyCount; i++) {
        // Get child button element
        uint8_t childBuf[8];
        ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(notifyContainer)->Child(i));  // B.3.6 bridge
        void* childElem = *(void**)childBuf;
        uint8_t bpBuf[8];
        new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(childElem));   // B.3.3 bridge
        void* btnElem = *(void**)bpBuf;

        // Set element ID: 0x67 + index
        reinterpret_cast<UI::Element*>(btnElem)->SetElementId(0x67 + i);  // B.3.6 bridge

        // Copy notification flag from event log singleton: singleton[i] + 0x10 -> button element +0xD8
        *(uint64_t*)((uintptr_t)btnElem + 0xd8) =
            *(uint64_t*)((uintptr_t)eventLogSingleton + 0x10 + i * sizeof(uintptr_t));

        // Push button ptr to notification array at +0x70.
        // B.3.2 bridge: EventLogUI+0x70 is Collection::Array<{InternalBasePtr}>
        // (stride 8, count +0x78, capacity +0x7C) inside a struct §B.2 has not
        // converted, so it is reached at its offset.
        auto& notifyBtns = GameColl::ArrayAt<GameColl::Bytes<8, 8>>(s, 0x70);
        notifyBtns.SetSize(notifyBtns.GetSize() + 1,
                           GameColl::Bytes<8, 8>((uint64_t)btnElem));
    }
}

// ============================================================
// EventLogUI::Slot7 (vtable +0x38) — Update event log entries
// ============================================================
// Iterates display rows from bottom to top, filling each with event
// data from the event log singleton. Updates text parameters and
// button visibility based on scroll offset. Enables/disables
// prev/next scroll buttons.
//
// Original: 0x140067440.

void EventLogUI_Slot7(EventLogUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Event log singleton: g_pGameWorldSingleton -> +0x138
    uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
    int totalEvents = *(int*)(eventLog + 0x20);  // total event count

    int displayCount = *(int*)(s + 0x58);  // number of display rows
    int scrollOffset = *(int*)(s + 0x48);

    // Rows are filled bottom-up: the newest visible event lands in the BOTTOM row.
    // Entry index for row `row` = firstIdx - (displayCount-1) + row, where
    // firstIdx = totalEvents - scrollOffset - 1 (the newest visible entry). The exe
    // skips the whole fill if firstIdx < 0 (no events yet). (0x140067471..)
    int firstIdx = totalEvents - scrollOffset - 1;
    if (firstIdx >= 0) {
        int base = firstIdx - (displayCount - 1);
        uintptr_t eventArray = *(uintptr_t*)(eventLog + 0x18);
        for (int row = displayCount - 1; row >= 0; row--) {
            int eventIdx = base + row;

            void* textElem = *(void**)(*(uintptr_t*)(s + 0x50) + row * 8);
            void* btnElem  = *(void**)(*(uintptr_t*)(s + 0x60) + row * 8);

            int enabled;
            if (eventIdx < 0) {
                // No event for this row — blank the text.
                System::String emptyStr("");
                reinterpret_cast<UI::TextElement*>(textElem)->SetParameter(0, emptyStr);  // B.3.6 bridge
                enabled = 0;
            } else {
                uintptr_t eventEntry = eventArray + (uintptr_t)eventIdx * 0x28;
                // Text = event entry +0x08.
                reinterpret_cast<UI::TextElement*>(textElem)->SetParameter(0, *reinterpret_cast<const System::String*>(eventEntry + 0x08));  // B.3.6 bridge
                // Colour = entry +0x00 via text element vtable[+0x50] (SetColor).
                reinterpret_cast<UI::Element*>(textElem)->SetColor(*(uint32_t*)eventEntry);  // B.3.6 bridge: Element slot 10 SetColor
                // The nav button is enabled only if the event has a map location
                // (tileIdx at entry+0x18 >= 0).
                enabled = (*(int*)(eventEntry + 0x18) >= 0) ? 1 : 0;
            }
            // btnElem vtable[+0x40] (SetEnabled) — DL = enabled. Common to both branches.
            reinterpret_cast<UI::Element*>(btnElem)->Enable(enabled != 0);  // B.3.6 bridge: Element slot 8 Enable
        }
    }

    // Enable/disable buttonBottom (+0x30) based on scroll offset > 0
    {
        void* btnBottom = *(void**)(s + 0x30);
        reinterpret_cast<UI::Element*>(btnBottom)->Enable(scrollOffset > 0);  // B.3.6 bridge: Element slot 8 Enable
    }

    // Enable/disable buttonNext (+0x40) based on scroll offset > 0
    {
        void* btnNext = *(void**)(s + 0x40);
        reinterpret_cast<UI::Element*>(btnNext)->Enable(scrollOffset > 0);  // B.3.6 bridge: Element slot 8 Enable
    }
}

// ============================================================
// EventLogUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles scroll buttons, notification toggles, and row click navigation.
//
// Original: 0x1400675D0.

void EventLogUI_Slot9(EventLogUI* self, void* param2, int msg, int id) {
    (void)param2;
    if (msg != 0x12) return;

    uintptr_t s = (uintptr_t)self;
    int* scrollOffset = (int*)(s + 0x48);

    if (id == 100) {
        // buttonPrevious — scroll to older events
        (*scrollOffset)++;
    } else if (id == 0x65) {
        // buttonNext — scroll to newer events
        (*scrollOffset)--;
    } else if (id == 0x66) {
        // buttonBottom — jump to most recent
        *scrollOffset = 0;
    } else if (id >= 0x67 && id <= 0x6B) {
        // Notification toggle buttons (5 categories)
        // FUN_140067320: reads flag from button element at +0xD8, copies to event log global +0x10+index
        {
            int btnIdx = id - 0x67;
            // Button ptr is stored in the notification array at +0x70 (array of 8-byte ptrs)
            void* ibp = (void*)(*(uintptr_t*)(s + 0x70) + (uintptr_t)btnIdx * 8);
            void* btnElem = *(void**)ibp;
            // Read button's flag from +0xD8 and write to event log singleton +0x10+btnIdx
            uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
            *(uint8_t*)(eventLog + 0x10 + btnIdx) = *(uint8_t*)((uintptr_t)btnElem + 0xD8);
        }
        return;
    } else if (id >= 0x6C) {
        // Display row click — navigate camera to event location
        // FUN_140067370: computes tile index from event log, calls terrain pick, sets camera target
        {
            int rowIdx = id - 0x6C;
            // Get event log
            uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
            int totalEvents = *(int*)(eventLog + 0x20);
            int displayCount = *(int*)(s + 0x58);
            int scrollOffset = *(int*)(s + 0x48);
            int eventIdx = (totalEvents - displayCount - scrollOffset) + rowIdx;
            // Get tile index from event entry at entryArray + eventIdx*0x28 + 0x18
            uintptr_t entryArray = *(uintptr_t*)(eventLog + 0x18);
            int tileIdx = *(int*)(entryArray + 0x18 + (uintptr_t)eventIdx * 0x28);
            if (tileIdx >= 0) {
                int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xF8);
                int gridSize = *mapGrid;
                float halfSize = (float)(gridSize >> 1) - 0.5f;
                float inXY[3] = {
                    (float)(tileIdx % gridSize) - halfSize,
                    (float)(tileIdx / gridSize) - halfSize,
                    0.0f
                };
                float outPos[3] = {};
                if (fn_MapGetTerrainPos) {
                    fn_MapGetTerrainPos(mapGrid, outPos, inXY, '\0', nullptr);
                } else {
                    outPos[0] = inXY[0]; outPos[1] = inXY[1]; outPos[2] = 0.0f;
                }
                uintptr_t camTarget = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0xE0);
                *(float*)(camTarget + 0x1C) = outPos[0];
                *(float*)(camTarget + 0x20) = outPos[1];
                *(float*)(camTarget + 0x24) = outPos[2];
            }
        }
        return;
    }

    // Clamp scroll offset to >= 0
    if (*scrollOffset < 0) *scrollOffset = 0;
}

// EventLogUI_Slot6 — inline in ui_panels.h (3-byte no-op, 0x140067630)

// ============================================================
// STANDALONE EXE: real EventLogUI vtable + registration
// ============================================================
// EventLogUI (DialogController, 0x80 bytes, exe vtable 0x14015bc40). In standalone
// this type was a dormant UI_DIALOG_STUB (all slots no-op) so the event-log panel
// opened blank. Promote it to a real per-class vtable wiring the reimplemented
// slots, following the CitizenUI promotion pattern (citizenui.cpp).
//
// DialogController slot semantics (verified against CitizenUI + the exe vtable):
//   +0x00 idx0 ScalarDtor
//   +0x10 idx2 GetTypeId
//   +0x28 idx5 Slot5  = setup (find nav buttons, "display" rows, "notifybuttons")
//   +0x30 idx6 Slot6  = 3-byte RET 0 no-op
//   +0x38 idx7 Slot7  = content update (fill display rows from the event log)
//   +0x40 idx8        = base DialogController::OnUpdate (per-frame). Left INHERITED,
//                       matching retail. This comment used to claim the framework
//                       "dispatches ONLY +0x40 each frame" and therefore drove Slot7
//                       from here — that claim is FALSE (it also propagated to
//                       pastureui.cpp, where it caused the same bug). Ghidra:
//                       UI::UserInterface::Update (Runtime 0x1801213c0) ->
//                       DialogInstance::ComputeLayout (0x180113d90) -> vtable idx6
//                       OnUpdateLayout (0x1801153a0) walks +0x78 and dispatches
//                       controller idx7 EVERY frame with no dirty flag. So idx7 is
//                       the per-frame sync path; driving Slot7 from idx8 as well ran
//                       it twice per frame. Also note idx8's real signature takes a
//                       `const Time&`, which a void(*)(void*) override silently drops.
//   +0x48 idx9 Slot9  = button dispatch (scroll / notify toggle / row click)
//
// EventLogUI_Slot5/7/9 read the event-log struct (gameWorld+0x138) but dereference
// no exe-.rdata type-id globals, so no EnsureGlobals lazy-fill is needed.

#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace eventlogui_sa {

void W_Slot5(void* self) {
    if (!self) return;
    EventLogUI_Slot5(reinterpret_cast<EventLogUI*>(self));
}
void W_Slot7(void* self) {
    if (!self) return;
    EventLogUI_Slot7(reinterpret_cast<EventLogUI*>(self));
}
// (W_OnUpdate removed — idx8 stays inherited, see the vtable note at the top.)
void W_Slot9(void* self, void* p2, int msg, int id) {
    if (self) EventLogUI_Slot9(reinterpret_cast<EventLogUI*>(self), p2, msg, id);
}
}  // namespace eventlogui_sa

// [5] 0x140067640
void EventLogUI::OnOpened() {
    eventlogui_sa::W_Slot5(this);
}

// [7] 0x140067440
void EventLogUI::OnUpdateLayout() {
    eventlogui_sa::W_Slot7(this);
}

// [9] 0x1400675d0
void EventLogUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                                int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    eventlogui_sa::W_Slot9(this, element, a, b);
}

// [0] 0x1400114b0: ~EventLogUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* EventLogUI::DeletingDtor(unsigned flags) {
    this->~EventLogUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400113e0: MemAlloc(0x80, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* EventLogUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EventLogUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) EventLogUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* EventLogUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) EventLogUI());
}

DEFINE_EXE_VTABLE(EventLogUI, "_ZTV10EventLogUI")
DEFINE_EXE_TYPE(EventLogUI, "UI::DialogController")

