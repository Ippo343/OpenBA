#pragma once
// ============================================================================
// code/Game/Dialog/ToolUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ToolUI", parent
//   "UI::DialogController", table 0x14015da70, 11 slots, sizeof 0x58.
//   create 0x140014580: MemAlloc(0x58, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400145f0 scalar deleting dtor (~ToolUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14007c070  RET 0
//   [4] 0x14007bd70  OnCreated
//   [8] 0x14007b9b0  OnUpdate
//   [9] 0x14007bc30  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ToolUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ToolUI)

public:
    ToolUI() = default;                                  // 0x140014580 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x1400145f0
    void Persist(void* /*stream*/) override {}   // [3] 0x14007c070  RET 0 in retail
    void OnCreated() override;   // [4] 0x14007bd70
    void OnUpdate(const System::Time& time) override;   // [8] 0x14007b9b0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14007bc30

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    void* componentData = nullptr;    // +0x30: BuildComponent or sub-component ptr
    void* labelTitle = nullptr;       // +0x38: TextElement "labelTitle"
    uint8_t hasBorderTitle = 0;   // +0x40: borderTitle element exists (enables title drag)
    uint8_t isDragging = 0;       // +0x41: currently dragging dialog
    uint8_t _pad42[2] = {};        // +0x42: gap
    int32_t dragOffsetX = 0;      // +0x44: mouse offset from dialog left
    int32_t dragOffsetY = 0;      // +0x48: mouse offset from dialog top
    uint8_t hasTitleDragZone = 0; // +0x4C: borderTitle has drag zone enabled
    uint8_t isDragPending = 0;    // +0x4D: waiting for mouse movement threshold
    uint8_t _pad4E[2] = {};        // +0x4E: gap
    int32_t dragStartX = 0;       // +0x50: drag start mouse X
    int32_t dragStartY = 0;       // +0x54: drag start mouse Y

protected:
    // 0x1400145f0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~ToolUI() = default;
};

static_assert(sizeof(ToolUI) == 0x58, "ToolUI is 0x58 (create 0x140014580 MemAlloc(0x58))");
static_assert(alignof(ToolUI) == 8, "qword-aligned (the vptr)");
