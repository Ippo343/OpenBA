// engine/src/ui/eventlogui.cpp
// Clean reimplementation of EventLogUI game logic.
//
// EventLogUI (0x80 bytes) — event log panel.
// Extends DialogController (0x30 base). Fields: EventLogUI.h.
//
// Element IDs: buttonPrevious=100, buttonNext=0x65, buttonBottom=0x66,
//              notification buttons=0x67..0x6B, display rows>=0x6C.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "runtime/types/collection/Collection.h"
#include "runtime/types/ui/CheckElement.h"
#include "Game/PlayerController.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/EventLog.h"         // GameUpdateLoop::m_eventNotifier, EventLogEntry
#include "Game/Map.h"              // GameUpdateLoop::m_game
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)

// The three element arrays are EventLogUI's Collection::Array members.
using ElementArray = EventLogUI::ElementArray;

// The event log (GameUpdateLoop +0x138; Game/EventLog.h): +0x10 the category
// filter bytes, +0x18 the entry array (EventLogEntry, 0x28 stride), +0x20 count.
static EventLog* EventLogAddress() {
    return GameUpdateLoop::_singleton->m_eventNotifier;
}

// ============================================================
// EventLogUI::OnOpened (vtable +0x28) — Setup event log display
// ============================================================
// Finds navigation buttons, "display" container, notification buttons.
// Stores button ptrs, resizes display arrays, sets element IDs.
//
// Original: 0x140067640.

// [5] 0x140067640
void EventLogUI::OnOpened() {
    UI::Element* dialogElem = _container;
    UI::DialogInstance* dialogInst = _dialog;

    // --- Find navigation buttons ---
    {
        System::String nameStr("buttonPrevious");
        buttonPrevious = dialogElem->FindElement(nameStr);
    }
    {
        System::String nameStr("buttonNext");
        buttonNext = dialogElem->FindElement(nameStr);
    }
    {
        System::String nameStr("buttonBottom");
        buttonBottom = dialogElem->FindElement(nameStr);
    }

    // Set element IDs on buttons
    buttonPrevious->SetElementId(100);   // buttonPrevious = 100
    buttonNext->SetElementId(0x65);      // buttonNext = 0x65
    buttonBottom->SetElementId(0x66);    // buttonBottom = 0x66

    // --- Find "display" container element ---
    UI::Element* displayElem;
    {
        System::String nameStr("display");
        displayElem = dialogElem->FindElement(nameStr).Get();
    }

    // Resize display text array and display button array to the row count.
    // Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Both elements are 8-byte InternalPtr handles (byte-copy).
    int displayRowCount = displayElem->ChildCount();
    displayTexts.SetSize(displayRowCount);
    displayButtons.SetSize(displayRowCount);

    // Iterate display rows: store child text element, store the row as a
    // button, set element IDs
    int rowCount = displayTexts._size;
    for (int i = 0; i < rowCount; i++) {
        // Child text element (direct child at index i)
        displayTexts[i] = displayElem->Child(i);

        // The same child, as the row's button (the retail check is against
        // Object::Internal, i.e. any element)
        displayButtons[i] = displayElem->Child(i);

        // Set element ID on the button: 0x6C + index
        displayButtons[i]->SetElementId(0x6C + i);
    }

    // Reset scroll offset to 0
    scrollOffset = 0;

    // --- Find "notifybuttons" container in the dialog instance ---
    UI::Element* notifyContainer;
    {
        System::String nameStr("notifybuttons");
        notifyContainer = dialogInst->FindElement(nameStr).Get();
    }

    // Iterate notification button children, store them in the notify array, set IDs
    int notifyCount = notifyContainer->ChildCount();
    EventLog* eventLogSingleton = EventLogAddress();

    for (int i = 0; i < notifyCount; i++) {
        // Child button element
        UI::Element* btnElem = notifyContainer->Child(i).Get();

        // Set element ID: 0x67 + index
        btnElem->SetElementId(0x67 + i);

        // Copy the notification flag from the event log into the check box:
        // ONE byte, flags[i] (retail 0x140067a55 / 0x140067a69).
        static_cast<UI::CheckElement*>(btnElem)->_checked =
            eventLogSingleton->field_0x10[i];

        // Push button ptr to the notification array.
        ElementArray& notifyBtns = notifyButtons;
        notifyBtns.SetSize(notifyBtns.GetSize() + 1, Object::InternalPtr<UI::Element>(btnElem));
    }
}

// ============================================================
// EventLogUI::OnUpdateLayout (vtable +0x38) — Update event log entries
// ============================================================
// Iterates display rows from bottom to top, filling each with event
// data from the event log. Updates text parameters and button state
// based on scroll offset. Enables/disables the bottom / next buttons.
//
// Original: 0x140067440.

// [7] 0x140067440
void EventLogUI::OnUpdateLayout() {
    EventLog* eventLog = EventLogAddress();
    int totalEvents = eventLog->entryCount;  // total event count

    int displayCount = displayTexts._size;  // number of display rows
    int scroll = scrollOffset;

    // Rows are filled bottom-up: the newest visible event lands in the BOTTOM row.
    // Entry index for row `row` = firstIdx - (displayCount-1) + row, where
    // firstIdx = totalEvents - scrollOffset - 1 (the newest visible entry). The exe
    // skips the whole fill if firstIdx < 0 (no events yet). (0x140067471..)
    int firstIdx = totalEvents - scroll - 1;
    if (firstIdx >= 0) {
        int base = firstIdx - (displayCount - 1);
        EventLogEntry* eventArray = eventLog->entries;
        for (int row = displayCount - 1; row >= 0; row--) {
            int eventIdx = base + row;

            UI::TextElement* textElem = static_cast<UI::TextElement*>(displayTexts[row].Get());
            UI::Element* btnElem = displayButtons[row].Get();

            int enabled;
            if (eventIdx < 0) {
                // No event for this row — blank the text.
                System::String emptyStr("");
                textElem->SetParameter(0, emptyStr);
                enabled = 0;
            } else {
                const EventLogEntry& eventEntry = eventArray[eventIdx];
                // Text = event entry +0x08.
                textElem->SetParameter(0, eventEntry.text);
                // Colour = entry +0x00 (Element slot 10 SetColor).
                textElem->SetColor(eventEntry.color);
                // The nav button is enabled only if the event has a map location
                // (tileIdx at entry+0x18 >= 0).
                enabled = (eventEntry.tileIndex >= 0) ? 1 : 0;
            }
            // Element slot 8 Enable — common to both branches.
            btnElem->Enable(enabled != 0);
        }
    }

    // Enable/disable buttonBottom based on scroll offset > 0 (slot 8 Enable)
    buttonBottom->Enable(scroll > 0);

    // Enable/disable buttonNext based on scroll offset > 0
    buttonNext->Enable(scroll > 0);
}

// ============================================================
// EventLogUI::ToggleNotification (0x140067320)
// ============================================================
// Reads the check box's state and writes it into the event log's category
// filter byte (+0x10 + index).
void EventLogUI::ToggleNotification(int32_t btnIdx) {
    UI::CheckElement* btnElem = static_cast<UI::CheckElement*>(notifyButtons[btnIdx].Get());
    EventLog* eventLog = EventLogAddress();
    // LEFT: EventLog::field_0x10 is bool[6] (Game/EventLog.h, GAME-a's): a bool
    // store would normalise the check box's raw byte, the byte store does not.
    *reinterpret_cast<uint8_t*>(&eventLog->field_0x10[btnIdx]) = btnElem->_checked;
}

// ============================================================
// EventLogUI::NavigateToRow (0x140067370)
// ============================================================
// Computes the row's event, its tile index, picks the terrain there and sets
// the camera's look-at target.
void EventLogUI::NavigateToRow(int32_t rowIdx) {
    GameUpdateLoop* game = GameUpdateLoop::_singleton;
    EventLog* eventLog = EventLogAddress();
    int totalEvents = eventLog->entryCount;
    int eventIdx = (totalEvents - displayTexts._size - scrollOffset) + rowIdx;
    // Tile index from the event entry (+0x18)
    EventLogEntry* entryArray = eventLog->entries;
    int tileIdx = entryArray[eventIdx].tileIndex;
    if (tileIdx >= 0) {
        // Map::size at +0.
        Map* mapGrid = game->m_game;
        int gridSize = mapGrid->size;
        float halfSize = (float)(gridSize >> 1) - 0.5f;
        float inXY[3] = {
            (float)(tileIdx % gridSize) - halfSize,
            (float)(tileIdx / gridSize) - halfSize,
            0.0f
        };
        float outPos[3] = {};
        Reimpl_MapGetTerrainPos(mapGrid, outPos, inXY, '\0', nullptr);   // 0x14009f3e0
        PlayerController* camTarget = game->m_playerController;
        camTarget->targetX = outPos[0];
        camTarget->targetY = outPos[1];
        camTarget->targetZ = outPos[2];
    }
}

// ============================================================
// EventLogUI::OnElementEvent (vtable +0x48) — Button dispatch
// ============================================================
// Handles scroll buttons, notification toggles, and row click navigation.
//
// Original: 0x1400675D0.

// [9] 0x1400675d0
void EventLogUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t msg, int32_t id,
                                int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (msg != 0x12) return;

    if (id == 100) {
        // buttonPrevious — scroll to older events
        scrollOffset++;
    } else if (id == 0x65) {
        // buttonNext — scroll to newer events
        scrollOffset--;
    } else if (id == 0x66) {
        // buttonBottom — jump to most recent
        scrollOffset = 0;
    } else if (id >= 0x67 && id <= 0x6B) {
        // Notification toggle buttons (5 categories)
        ToggleNotification(id - 0x67);
        return;
    } else if (id >= 0x6C) {
        // Display row click — navigate camera to event location
        NavigateToRow(id - 0x6C);
        return;
    }

    // Clamp scroll offset to >= 0
    if (scrollOffset < 0) scrollOffset = 0;
}

// [6] 0x140067630 (3-byte no-op) is EventLogUI::OnClosed, inline in EventLogUI.h.

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

DEFINE_EXE_TYPE(EventLogUI, "UI::DialogController")

