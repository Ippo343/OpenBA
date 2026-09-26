#pragma once
// ============================================================================
// code/Game/Dialog/ScenarioDialog.h — ENDGAME B.3.8 block DLG
//
// The Scenario / Tutorial picker (one class; the byte at +0xB8 tells them apart).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "ScenarioDialog", parent "UI::DialogInstance", ti 0x1401c5558
//   create 0x140013970 (MemAlloc(0xe8, 8)), createInPlace 0x1400139f0
//   ctors 0x140078650, 0x1400786b0 (the tree's ScenarioDialog_Ctor), 0x140078750
//   table 0x14015d2d0, 10 slots:
//     [0] 0x140013af0 scalar deleting dtor: MemFree(+0xd8), +0xd8/+0xe0 = 0,
//         ~DialogInstance, MemFree on &1
//     [1] TypeName  [2] Type  [3] DialogInstance::Persist (import)
//     [4] 0x140078010 OnOpened  [5] DialogInstance::OnClosed
//     [6] 0x140077be0 OnUpdateLayout  [7] 0x140078000 OnUpdate `RET 0`
//     [8] 0x140077f80 OnElementEvent  [9] DialogInstance::OnSerializeSettings
//   sizeof 0xe8.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class ScenarioDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(ScenarioDialog)

public:
    ScenarioDialog();                                               // create 0x140013970
    ScenarioDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                   Object::InternalPtr<UI::DialogInstance> parent,
                   uint64_t parentMenu, uint8_t difficulty);        // 0x1400786b0

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140013af0
    void OnOpened() override;                                      // [4] 0x140078010
    // [5]: retail INHERITS DialogInstance::OnClosed; the hand-built table had a
    // no-op. Kept (PORT GAP, B38 D3.1).
    void OnClosed() override;                                      // [5] PORT GAP
    void OnUpdateLayout() override;                                // [6] 0x140077be0
    void OnUpdate(const System::Time& time) override;              // [7] 0x140078000 RET
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x140077f80

protected:
    ~ScenarioDialog();                                             // 0x140013af0's body

public:
    uint8_t  difficulty;       // +0xB8: 0 scenario, 1 tutorial
    uint8_t  _padB9[7];        // +0xB9
    uint64_t dataC0;           // +0xC0: the parent menu's InternalBasePtr
    int32_t  dataC8;           // +0xC8: selected index, ctor -1
    uint8_t  _padCC[4];        // +0xCC
    uint64_t dataD0;           // +0xD0: confirm button
    uint64_t dataD8;           // +0xD8: scenario entry array data (stride 0x38)
    uint64_t dataE0;           // +0xE0: scenario entry count / capacity
};

static_assert(offsetof(ScenarioDialog, difficulty) == 0xB8, "ScenarioDialog +0xb8");
static_assert(offsetof(ScenarioDialog, dataC0) == 0xC0, "ScenarioDialog +0xc0");
static_assert(offsetof(ScenarioDialog, dataC8) == 0xC8, "ScenarioDialog +0xc8");
static_assert(offsetof(ScenarioDialog, dataD8) == 0xD8, "ScenarioDialog +0xd8");
static_assert(offsetof(ScenarioDialog, dataE0) == 0xE0, "ScenarioDialog +0xe0");
static_assert(sizeof(ScenarioDialog) == 0xE8, "ScenarioDialog is 0xe8 (create 0x140013970)");
static_assert(alignof(ScenarioDialog) == 8, "ScenarioDialog is qword-aligned");

// The ctor DOOR (scenariodialog.cpp), bound by name in engine_funcs.cpp as
// fn_ScenarioDialog_Ctor (retail 0x1400786b0).
void* ScenarioDialog_Ctor(void* self, void* ui, void* extRes, void* parentIBP,
                          void* parentMenuIBP, uint8_t difficulty);
