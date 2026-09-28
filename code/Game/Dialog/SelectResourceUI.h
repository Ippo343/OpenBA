#pragma once
// ============================================================================
// code/Game/Dialog/SelectResourceUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// The abstract base of the three "pick a resource" panels (CropUI, OrchardUI,
// PastureUI): a combo of resource types (+0x30) and three pure virtuals
// that the leaves answer.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1): reflected "SelectResourceUI",
//   parent "UI::DialogController", table 0x14015d330 (Ghidra mislabels it
//   `vtable_CropUI`), 14 slots, ABSTRACT (create = 0x140003730, `return 0`).
//   No ctor function of its own: each leaf's ctor INLINES it — after
//   ??0DialogController, stamp 0x14015d330, InternalBasePtr(+0x30, null),
//   then the leaf's own stamp (retail_ctor_dtor survey).
//   [0] 0x140013ca0 scalar deleting dtor (the table's own)
//   [1] TypeName  [2] Type
//   [3] Internal::Persist (IMPORT, inherited)
//   [4] 0x140078be0  OnCreated       — find "selectResource", id 500, populate
//   [5] [6] DialogController's (IMPORT, inherited)
//   [7] 0x140078cb0  OnUpdateLayout  — enable the combo iff it has items
//   [8] 0x140078d30  OnUpdate        — repopulate when the count changed
//   [9] 0x140078d80  OnElementEvent  — a pick -> slot 11
//   [10] DialogController's (IMPORT, inherited)
//   [11] [12] [13] _purecall: SetResource / GetResource / GetCategory
// The reflected record is DEFINE_EXE_TYPE with the abstract factory
// (ENDGAME B.3.9 RF).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"   // DECLARE_EXE_TYPE (ENDGAME B.3.9 RF)

class SelectResourceUI : public UI::DialogController {
    DECLARE_EXE_TYPE(SelectResourceUI)

public:
    // The leaf dtors restamp 0x14015d330 and chain ~DialogController; the
    // +0x30 InternalBasePtr has no release in retail either.
    ~SelectResourceUI() override = default;                       // [0]  0x140013ca0
    // [1] / [2]: the abstract base answers its own registration (only a
    // half-built object dispatches here) — the pilot's Component shape.
    // [1] TypeName 0x140013c90 and [2] Type 0x140013c80: DECLARE_EXE_TYPE (retail `MOV RAX,[ti+8]` / `MOV EAX,[ti]`).
    void OnCreated() override;                                    // [4]  0x140078be0
    void OnUpdateLayout() override;                               // [7]  0x140078cb0
    void OnUpdate(const System::Time& time) override;             // [8]  0x140078d30
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;  // [9] 0x140078d80

    // [11] set the selected resource type on the leaf's component.
    virtual void  SetResource(int64_t type) = 0;                  // [11] _purecall
    // [12] the current resource type, written through `out` (retslot shape:
    // RCX this, RDX out; returns out).
    virtual void* GetResource(void* out) = 0;                     // [12] _purecall
    // [13] the SelectDescription category the combo lists.
    virtual int   GetCategory() = 0;                              // [13] _purecall

    // +0x30 the "selectResource" ComboElement (Object::InternalBasePtr). The
    // name is the Panels.h CropUI POD's, kept for cropui.cpp's bodies.
    uint64_t internalBasePtr = 0;                                 // +0x30

protected:
    // Inlined into every leaf ctor by retail; the InternalBasePtr(null) is
    // the field initialiser above.
    SelectResourceUI() = default;
};

static_assert(sizeof(SelectResourceUI) == 0x38, "SelectResourceUI adds one qword to DialogController");
static_assert(alignof(SelectResourceUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(SelectResourceUI, internalBasePtr) == 0x30, "combo @ +0x30 (0x140078be0)");
