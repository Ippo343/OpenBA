#pragma once
// ============================================================================
// code/Game/Dialog/NewGameDialog.h — ENDGAME B.3.8 block DLG
//
// The New Game dialog (map / terrain / climate / condition / disaster, name,
// seed). Original source: C:\Projects\Buildy\code\Game\Dialog/NewGameDialog.h
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "NewGameDialog", parent "UI::DialogInstance", ti 0x1401c52f0
//   create 0x140012c70 (MemAlloc(0xae8, 8)), createInPlace 0x140012cb0
//   ctor 0x140071040 (5 args; Ghidra's "CopyCtor"), the door the tree registers
//   table 0x14015c920, 10 slots:
//     [0] 0x140012cc0 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x1400706d0 OnOpened
//     [5] 0x1400706c0 OnClosed `RET 0`      [6] 0x140070650 OnUpdateLayout
//     [7] DialogInstance::OnUpdate          [8] 0x140070110 OnElementEvent
//     [9] DialogInstance::OnSerializeSettings
//   sizeof 0xae8 (a System::Random at +0x118).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class NewGameDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(NewGameDialog)

public:
    NewGameDialog();                                                // create 0x140012c70
    NewGameDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                  Object::InternalPtr<UI::DialogInstance> parent,
                  uint64_t parentMenu);                             // 0x140071040

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x140012cc0
    void OnOpened() override;                                      // [4] 0x1400706d0
    void OnClosed() override;                                      // [5] 0x1400706c0 RET
    void OnUpdateLayout() override;                                // [6] 0x140070650
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x140070110

protected:
    ~NewGameDialog() = default;

public:
    void* comboTerrain;         // +0xB8: "comboTerrain" ComboElement
    void* comboTerrainSize;     // +0xC0: "comboTerrainSize" ComboElement
    void* comboClimate;         // +0xC8: "comboClimate" ComboElement
    void* comboCondition;       // +0xD0: "comboCondition" ComboElement (map generator)
    void* comboDisaster;        // +0xD8: "comboDisaster" ComboElement
    void* editName;             // +0xE0: "editName" EditElement
    void* editSeed;             // +0xE8: "editSeed" NumberEditElement
    void* buttonOk;             // +0xF0: "buttonOk" ButtonElement
    void* textDescription;      // +0xF8: "textDescription" TextElement
    void* mapGeneratorList;     // +0x100: ExternalList*
    void* climateGeneratorList; // +0x108: ExternalList*
    void* terrainGeneratorList; // +0x110: ExternalList*
    uint8_t  random[0x9C0];     // +0x118: System::Random sub-object (to +0xAD8)
    uint64_t _pad_AD8;          // +0xAD8: gap
    void*    worldSetup;        // +0xAE0: the MainMenuDialog (used by StartGame)
};

static_assert(offsetof(NewGameDialog, comboTerrain) == 0xB8, "NewGameDialog +0xb8");
static_assert(offsetof(NewGameDialog, terrainGeneratorList) == 0x110, "NewGameDialog +0x110");
static_assert(offsetof(NewGameDialog, random) == 0x118, "NewGameDialog +0x118");
static_assert(offsetof(NewGameDialog, worldSetup) == 0xAE0, "NewGameDialog +0xae0");
static_assert(sizeof(NewGameDialog) == 0xAE8, "NewGameDialog is 0xae8 (create 0x140012c70)");
static_assert(alignof(NewGameDialog) == 8, "NewGameDialog is qword-aligned");
