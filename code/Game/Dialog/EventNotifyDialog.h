#pragma once
// ============================================================================
// code/Game/Dialog/EventNotifyDialog.h — ENDGAME B.3.8 block DLG
//
// The event-notification ribbon under the toolbar.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "EventNotifyDialog", parent "UI::DialogInstance", ti 0x1401c4f70
//   create 0x140011500 (MemAlloc(0xf0, 8)), createInPlace 0x140011540
//   ctor 0x140068ab0 (the door the tree registers), 0x140068b60
//   table 0x14015bca0, 10 slots:
//     [0] 0x140011550 scalar deleting dtor (frees the two arrays)
//     [1] TypeName  [2] Type  [3] DialogInstance::Persist (import)
//     [4] 0x1400686d0 OnOpened   [5] 0x1400686c0 OnClosed `RET 0`
//     [6] 0x1400680c0 OnUpdateLayout   [7] 0x140067c30 OnUpdate
//     [8] 0x140068680 OnElementEvent   [9] DialogInstance::OnSerializeSettings
//   sizeof 0xf0.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class EventNotifyDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(EventNotifyDialog)

public:
    EventNotifyDialog() = default;                                  // create 0x140011500
    EventNotifyDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                      Object::InternalPtr<UI::DialogInstance> parent);  // 0x140068ab0

    // 0x140011550's body
    ~EventNotifyDialog() override;                                 // [0] 0x140011550
    void OnOpened() override;                                      // [4] 0x1400686d0
    void OnClosed() override;                                      // [5] 0x1400686c0 RET
    void OnUpdateLayout() override;                                // [6] 0x1400680c0
    void OnUpdate(const System::Time& time) override;              // [7] 0x140067c30
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x140068680

public:
    uint64_t dataB8;           // +0xB8: the parent (toolbar) dialog reference
    uint32_t dataC0;           // +0xC0: smoothed ribbon Y
    int32_t  dataC4;           // +0xC4: last event-log index seen, ctor -1
    uint64_t dataC8;           // +0xC8: slot array data (0x18 stride)
    uint64_t dataD0;           // +0xD0: slot array count / capacity
    uint64_t dataD8;           // +0xD8: active-event array data (0x30 stride)
    uint64_t dataE0;           // +0xE0: active-event array count / capacity
    uint64_t dataE8;           // +0xE8: "ribbonNotify" element
};

static_assert(offsetof(EventNotifyDialog, dataB8) == 0xB8, "EventNotifyDialog +0xb8");
static_assert(offsetof(EventNotifyDialog, dataC0) == 0xC0, "EventNotifyDialog +0xc0");
static_assert(offsetof(EventNotifyDialog, dataC4) == 0xC4, "EventNotifyDialog +0xc4");
static_assert(offsetof(EventNotifyDialog, dataC8) == 0xC8, "EventNotifyDialog +0xc8");
static_assert(offsetof(EventNotifyDialog, dataD8) == 0xD8, "EventNotifyDialog +0xd8");
static_assert(offsetof(EventNotifyDialog, dataE8) == 0xE8, "EventNotifyDialog +0xe8");
static_assert(sizeof(EventNotifyDialog) == 0xF0, "EventNotifyDialog is 0xf0 (create 0x140011500)");
static_assert(alignof(EventNotifyDialog) == 8, "EventNotifyDialog is qword-aligned");
