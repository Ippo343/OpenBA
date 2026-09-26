#pragma once
// ============================================================================
// code/Game/Dialog/SpriteCaptureUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "SpriteCaptureUI", parent
//   "UI::DialogController", table 0x14015d4f0, 11 slots, sizeof 0x70.
//   create 0x140013d10: MemAlloc(0x70, 8), ??0DialogController, stamp, fields.
//   [0] 0x140013fc0 scalar deleting dtor (~SpriteCaptureUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x140079850  RET 0
//   [4] 0x140079640  OnCreated
//   [8] 0x1400793a0  OnUpdate
//   [9] 0x140079620  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class SpriteCaptureUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(SpriteCaptureUI)

public:
    SpriteCaptureUI() = default;                         // 0x140013d10 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140013fc0
    void Persist(void* /*stream*/) override {}   // [3] 0x140079850  RET 0 in retail
    void OnCreated() override;   // [4] 0x140079640  PORT GAP
    void OnUpdate(const System::Time& time) override;   // [8] 0x1400793a0  PORT GAP
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140079620  PORT GAP

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    void* editCapture = nullptr;      // +0x30: EditElement* (InternalBasePtr)
    void* buttonCapture = nullptr;    // +0x38: Element* (InternalBasePtr)
    void* captureBox = nullptr;       // +0x40: Element* (InternalBasePtr)
    int32_t curX = 0;             // +0x48: current drag mouse X
    int32_t curY = 0;             // +0x4C: current drag mouse Y
    int32_t anchorX = 0;          // +0x50: drag anchor X (init 100)
    int32_t anchorY = 0;          // +0x54: drag anchor Y (init 100)
    uint8_t dragging = 0;         // +0x58: 1 = left mouse button held, dragging
    uint8_t _pad59[7] = {};        // +0x59: pad to +0x60
    void* pendingExternal = nullptr;  // +0x60: External* awaiting save (set externally)
    int32_t saveFrameCount = 0;   // +0x68: incremented each frame; triggers save at ==1
    uint8_t _pad6C[4] = {};        // +0x6C: pad to 0x70

protected:
    // 0x140013fc0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~SpriteCaptureUI() = default;
};

static_assert(sizeof(SpriteCaptureUI) == 0x70, "SpriteCaptureUI is 0x70 (create 0x140013d10 MemAlloc(0x70))");
static_assert(alignof(SpriteCaptureUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(SpriteCaptureUI, editCapture) == 0x30, "editCapture @ 0x30");
static_assert(offsetof(SpriteCaptureUI, curX) == 0x48, "curX @ 0x48");
static_assert(offsetof(SpriteCaptureUI, anchorX) == 0x50, "anchorX @ 0x50");
static_assert(offsetof(SpriteCaptureUI, dragging) == 0x58, "dragging @ 0x58");
static_assert(offsetof(SpriteCaptureUI, pendingExternal) == 0x60, "pendingExternal @ 0x60");
static_assert(offsetof(SpriteCaptureUI, saveFrameCount) == 0x68, "saveFrameCount @ 0x68");
