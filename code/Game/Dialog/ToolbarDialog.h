#pragma once
// ============================================================================
// code/Game/Dialog/ToolbarDialog.h — ENDGAME B.3.8 block DLG
//
// The in-game toolbar (Dialog/Toolbar.rsc) and each sub-toolbar it opens.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "ToolbarDialog", parent "UI::DialogInstance", ti 0x1401c5d38
//   create 0x140021710 (MemAlloc(0x108, 8)), ctor 0x1400b9910
//   table 0x14015fa00, 10 slots:
//     [0] 0x140021760 scalar deleting dtor (frees +0xF8, +0xD0, +0xC0; ~DialogInstance)
//     [1] TypeName  [2] Type  [3] DialogInstance::Persist (import)
//     [4] 0x1400b97c0 OnOpened (RET)
//     [5] 0x1400b97b0 OnClosed (JMP CloseSubDialog 0x1400b9730; no SaveSettings)
//     [6] 0x1400b8d40 OnUpdateLayout (sync the buttons with the game state)
//     [7] DialogInstance::OnUpdate (import)
//     [8] 0x1400b92a0 OnElementEvent (the button clicks)
//     [9] DialogInstance::OnSerializeSettings (import)
//   sizeof 0x108.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class ToolbarDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(ToolbarDialog)

public:
    ToolbarDialog();                                                // create 0x140021710
    ToolbarDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                  Object::InternalPtr<UI::DialogInstance> parent,
                  uint64_t toolbarGroup);                           // 0x1400b9910

    ~ToolbarDialog() override;                                     // [0] 0x140021760
    void OnOpened() override;                                      // [4] 0x1400b97c0
    void OnClosed() override;                                      // [5] 0x1400b97b0
    void OnUpdateLayout() override;                                // [6] 0x1400b8d40
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x1400b92a0

    // FUN_1400b9730: close the open sub-toolbar dialog and reset the radio. Not a
    // slot: slot 5 is a JMP to it and button case 5 calls it on the parent.
    void CloseSubDialog();

public:
    uint64_t radioElement;      // +0xB8: the "toolbarContent" RadioElement
    uint64_t buttonElements;    // +0xC0: Array<Element*> data
    int32_t  buttonCount;       // +0xC8
    int32_t  buttonCapacity;    // +0xCC
    uint64_t toolbarItems;      // +0xD0: Array<ToolbarItem*> data
    int32_t  itemCount;         // +0xD8
    int32_t  itemCapacity;      // +0xDC
    uint64_t toolbarGroupId;    // +0xE0: the group this (sub-)toolbar shows
    uint64_t subDialog;         // +0xE8: InternalBasePtr to the open sub-toolbar
    int32_t  openSubIndex;      // +0xF0: -1 = none
    int32_t  currentSpeedIndex; // +0xF4
    uint64_t speedValues;       // +0xF8: Array<int32> data
    int32_t  speedValueCount;   // +0x100
    int32_t  speedValueCapacity;// +0x104
};

static_assert(offsetof(ToolbarDialog, radioElement) == 0xB8, "ToolbarDialog +0xb8");
static_assert(offsetof(ToolbarDialog, buttonElements) == 0xC0, "ToolbarDialog +0xc0");
static_assert(offsetof(ToolbarDialog, buttonCount) == 0xC8, "ToolbarDialog +0xc8");
static_assert(offsetof(ToolbarDialog, toolbarItems) == 0xD0, "ToolbarDialog +0xd0");
static_assert(offsetof(ToolbarDialog, itemCount) == 0xD8, "ToolbarDialog +0xd8");
static_assert(offsetof(ToolbarDialog, toolbarGroupId) == 0xE0, "ToolbarDialog +0xe0");
static_assert(offsetof(ToolbarDialog, subDialog) == 0xE8, "ToolbarDialog +0xe8");
static_assert(offsetof(ToolbarDialog, openSubIndex) == 0xF0, "ToolbarDialog +0xf0");
static_assert(offsetof(ToolbarDialog, currentSpeedIndex) == 0xF4, "ToolbarDialog +0xf4");
static_assert(offsetof(ToolbarDialog, speedValues) == 0xF8, "ToolbarDialog +0xf8");
static_assert(offsetof(ToolbarDialog, speedValueCount) == 0x100, "ToolbarDialog +0x100");
static_assert(sizeof(ToolbarDialog) == 0x108, "ToolbarDialog is 0x108 (create 0x140021710)");
static_assert(alignof(ToolbarDialog) == 8, "ToolbarDialog is qword-aligned");

// The ctor door, registered at 0x1400b9910 (address_registry.cpp) and read through
// fn_ToolbarDialog_Ctor by GameUpdateLoop::Startup: placement-news the class.
void* ToolbarDialog_Ctor_Impl(void* self, void* userInterface, void* dialog,
                              void* parentPtr, void* toolbarData);
