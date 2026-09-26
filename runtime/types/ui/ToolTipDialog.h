#pragma once
// ============================================================================
// runtime/types/ui/ToolTipDialog.h — UI::ToolTipDialog, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block B. The tooltip popup `UserInterface::BeginToolTip`
// (0x180120860) allocates (0xc0), constructs through the DialogInstance BASE
// ctor and stamps with this class's table. It overrides only slots 0/1/2.
//
// PARENT: `UI::DialogInstance`, proved by
//   0x18004cf90  ?_ParentTypeName@ToolTipDialog@UI@@CAPEBDXZ -> "UI::DialogInstance"
// *** NAMESPACE-QUALIFIED although the parent is in the SAME namespace
// (ComboDialog's literal is bare). RULE B6-7: copied, never derived. ***
//
// `??_7ToolTipDialog@UI@@6B@` @ 0x180157880, TEN slots (B36_SLOTS.md sec. 1):
//
//   [0] +0x00  0x18004d270  (unexported) vector deleting dtor
//   [1] +0x08  0x18004cfc0  ?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ
//   [2] +0x10  0x18004cfa0  ?Type@ToolTipDialog@UI@@UEBAIXZ
//   [3..9]                  ALL inherited from UI::DialogInstance
//
// sizeof 0xc0 (deleting dtor; BeginToolTip's `operator new(0xc0)`). The eight
// bytes past the base are TWO DWORDS, read from retail this block:
//   * `??0ToolTipDialog@UI@@QEAA@AEBV01@@Z` (copy ctor, 0x18004d180) copies
//     `[RBX+0xb8]` and `[RBX+0xbc]` as two separate DWORD moves;
//   * the element ctor (0x1800daca0) forwards its fourth argument to the base
//     ctor as `dataSource` and writes NOTHING past +0xb8; no ported body reads
//     them either. So they are named `_unknownB8`/`_unknownBC` (RULE B6-8: a
//     writer exists — the copy ctor — but nothing names them).
//
// ABSTRACT, no real ctor (B6-3; slot 0 is the mint's RT_DELDTOR row).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "DialogInstance.h"

namespace UI {

class ToolTipDialog : public DialogInstance {
public:
    // [0] 0x18004d270 — pure on purpose; the RT_DELDTOR row is not retired.
    Object::Base* DeletingDtor(unsigned flags) override = 0;

    const char* TypeName() const override;                        // [1] 0x18004cfc0
    uint32_t    Type() const override;                            // [2] 0x18004cfa0

    // ---- PORTED non-virtual member ---------------------------------------
    void SetText(const System::String& text);                    // 0x1800dab00

    // ---- exported, NOT PORTED as members ----------------------------------
    // SetPosition is `SetUpperLeft(x, y - root->_layoutHeight, false)`
    // (0x1800dac50, four instructions); no caller in the tree.
    void SetPosition(int32_t x, int32_t y);                       // 0x1800dac50
    ToolTipDialog(const ToolTipDialog& other);                    // 0x18004d180
    ToolTipDialog& operator=(const ToolTipDialog& other);         // 0x18004d1e0
    static uint32_t      TypeId();                                // 0x18004cfb0
    // MemoryAllocator statics (B36_RULES 7): `??2` 0x18004cfd0 is the door
    // `RtUi_ToolTipDialog_OperatorNew` (tooltipdialog.cpp); CreateNewObject /
    // CreateInPlaceObject are doors in ui_factory_wave28.cpp.
    static void* operator new(std::size_t bytes);                 // 0x18004cfd0
    static void* operator new(std::size_t bytes, void* place);    // 0x18004d040
    static void  operator delete(void* p);                        // 0x18004d050
    static Object::Base* CreateNewObject();                       // 0x18004d0b0
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18004d140

protected:
    // NOT retail's ctors (0x1800daca0, 0x1800dad00) — B6-3.
    ToolTipDialog() = default;
    // `??1ToolTipDialog@UI@@UEAA@XZ` 0x1800dac70 (stamp + tail-JMP to
    // ~DialogInstance). Defined EMPTY; its row is deleting_dtor_leaves.cpp's.
    ~ToolTipDialog();

    // RULE B5-15 / B6-7, COPIED from B36_SLOTS section 3:
    //   0x18004cf80  ?_TypeName@       -> "ToolTipDialog"
    //   0x18004cf90  ?_ParentTypeName@ -> "UI::DialogInstance"   (QUALIFIED)
    static const char* _TypeName();
    static const char* _ParentTypeName();

public:
    int32_t _unknownB8;   // +0xb8  copied by 0x18004d180, otherwise unread
    int32_t _unknownBC;   // +0xbc  copied by 0x18004d180, otherwise unread
};

static_assert(sizeof(ToolTipDialog) == 0xc0,
              "UI::ToolTipDialog is 0xc0 bytes (deleting dtor 0x18004d270; "
              "BeginToolTip allocates 0xc0)");
static_assert(alignof(ToolTipDialog) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ToolTipDialog, _unknownB8) == sizeof(DialogInstance),
              "ToolTipDialog's own fields begin at the end of the DialogInstance base");
static_assert(offsetof(ToolTipDialog, _unknownBC) == 0xbc, "_unknownBC @ +0xbc");

}  // namespace UI
