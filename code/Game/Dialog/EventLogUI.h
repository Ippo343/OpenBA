#pragma once
// ============================================================================
// code/Game/Dialog/EventLogUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "EventLogUI", parent
//   "UI::DialogController", table 0x14015bc40, 11 slots, sizeof 0x80.
//   create 0x1400113e0: MemAlloc(0x80, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400114b0 scalar deleting dtor (~EventLogUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [5] 0x140067640  OnOpened
//   [6] 0x140067630  RET 0
//   [7] 0x140067440  OnUpdateLayout
//   [9] 0x1400675d0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
//
// B.4 W1 (DLG-b): the three buttons and the three element arrays OnOpened
// 0x140067640 fills. The arrays are retail's Collection::Array<InternalPtr<
// Element>> (0x1400114b0 frees all three); H5 declares them as their headers
// until the members' ~Array runs (the leak below).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class EventLogUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(EventLogUI)

public:
    EventLogUI() = default;                              // 0x1400113e0 (ctor inlined in the factory)

    // 0x1400114b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail frees the Array members at +0x70, +0x60, +0x50; the
    // tree never did (leak kept).
    ~EventLogUI() override = default;                             // [0] 0x1400114b0
    void OnOpened() override;   // [5] 0x140067640
    void OnClosed() override {}   // [6] 0x140067630  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x140067440
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400675d0

    // 0x140067320: a notification check box's state into the event log's filter.
    void ToggleNotification(int32_t index);                       // 0x140067320
    // 0x140067370: centre the camera on display row `row`'s event.
    void NavigateToRow(int32_t row);                              // 0x140067370

    // ---- fields: OnOpened 0x140067640 ----------------------------------
    Object::InternalPtr<UI::Element> buttonBottom;     // +0x30 "buttonBottom", id 0x66
    Object::InternalPtr<UI::Element> buttonPrevious;   // +0x38 "buttonPrevious", id 100
    Object::InternalPtr<UI::Element> buttonNext;       // +0x40 "buttonNext", id 0x65
    int32_t scrollOffset = 0;                          // +0x48 rows scrolled back from the newest
    int32_t _pad4C = 0;                                // +0x4C
    // +0x50 Array<InternalPtr<Element>>: the "display" rows (text)
    Object::InternalPtr<UI::Element>* displayTextData = nullptr;    // +0x50
    int32_t displayTextCount = 0;                                    // +0x58
    int32_t displayTextCapacity = 0;                                 // +0x5C
    // +0x60 Array<InternalPtr<Element>>: the same rows as buttons (ids 0x6C + i)
    Object::InternalPtr<UI::Element>* displayButtonData = nullptr;  // +0x60
    int32_t displayButtonCount = 0;                                  // +0x68
    int32_t displayButtonCapacity = 0;                               // +0x6C
    // +0x70 Array<InternalPtr<Element>>: "notifybuttons"' check boxes (ids 0x67 + i)
    Object::InternalPtr<UI::Element>* notifyButtonData = nullptr;   // +0x70
    int32_t notifyButtonCount = 0;                                   // +0x78
    int32_t notifyButtonCapacity = 0;                                // +0x7C
};

static_assert(sizeof(EventLogUI) == 0x80, "EventLogUI is 0x80 (create 0x1400113e0 MemAlloc(0x80))");
static_assert(alignof(EventLogUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(EventLogUI, buttonBottom) == 0x30, "EventLogUI::buttonBottom @ 0x30");
static_assert(offsetof(EventLogUI, buttonPrevious) == 0x38, "EventLogUI::buttonPrevious @ 0x38");
static_assert(offsetof(EventLogUI, buttonNext) == 0x40, "EventLogUI::buttonNext @ 0x40");
static_assert(offsetof(EventLogUI, scrollOffset) == 0x48, "EventLogUI::scrollOffset @ 0x48");
static_assert(offsetof(EventLogUI, displayTextData) == 0x50, "EventLogUI::displayTextData @ 0x50");
static_assert(offsetof(EventLogUI, displayTextCount) == 0x58, "EventLogUI::displayTextCount @ 0x58");
static_assert(offsetof(EventLogUI, displayTextCapacity) == 0x5C, "EventLogUI::displayTextCapacity @ 0x5C");
static_assert(offsetof(EventLogUI, displayButtonData) == 0x60, "EventLogUI::displayButtonData @ 0x60");
static_assert(offsetof(EventLogUI, displayButtonCount) == 0x68, "EventLogUI::displayButtonCount @ 0x68");
static_assert(offsetof(EventLogUI, displayButtonCapacity) == 0x6C, "EventLogUI::displayButtonCapacity @ 0x6C");
static_assert(offsetof(EventLogUI, notifyButtonData) == 0x70, "EventLogUI::notifyButtonData @ 0x70");
static_assert(offsetof(EventLogUI, notifyButtonCount) == 0x78, "EventLogUI::notifyButtonCount @ 0x78");
static_assert(offsetof(EventLogUI, notifyButtonCapacity) == 0x7C, "EventLogUI::notifyButtonCapacity @ 0x7C");
