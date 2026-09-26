#pragma once
// ============================================================================
// code/Game/Dialog/SaveLoadDialog.h — ENDGAME B.3.8 block DLG
//
// The Save / Load dialog (mode byte at +0xB8: 0 save, 1 load).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "SaveLoadDialog", parent "UI::DialogInstance", ti 0x1401c5cc8
//   create 0x140021160 (MemAlloc(0x160, 8)), createInPlace 0x1400211a0
//   ctors 0x1400b80f0, 0x1400b81c0, 0x1400b8270 (the door the tree registers)
//   table 0x14015f900, 10 slots:
//     [0] 0x1400211b0 scalar deleting dtor  [1] TypeName  [2] Type
//     [3] DialogInstance::Persist (import)  [4] 0x1400b7d00 OnOpened
//     [5] DialogInstance::OnClosed          [6] 0x1400b6880 OnUpdateLayout
//     [7] 0x1400b79b0 OnUpdate              [8] 0x1400b67e0 OnElementEvent
//     [9] DialogInstance::OnSerializeSettings
//   sizeof 0x160.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class SaveLoadDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(SaveLoadDialog)

public:
    SaveLoadDialog();                                               // create 0x140021160
    SaveLoadDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                   Object::InternalPtr<UI::DialogInstance> parent,
                   uint8_t mode, void* engine, void* gameWorld);    // 0x1400b8270

    Object::Base* DeletingDtor(unsigned flags) override;           // [0] 0x1400211b0
    void OnOpened() override;                                      // [4] 0x1400b7d00
    // [5]: retail INHERITS DialogInstance::OnClosed; the hand-built table had a
    // print-once no-op. Kept (PORT GAP, B38 D3.1).
    void OnClosed() override;                                      // [5] PORT GAP
    void OnUpdateLayout() override;                                // [6] 0x1400b6880
    void OnUpdate(const System::Time& time) override;              // [7] 0x1400b79b0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element,
                        int32_t a, int32_t b, int32_t c,
                        int32_t d, int32_t e, void* userData) override;  // [8] 0x1400b67e0

protected:
    ~SaveLoadDialog() = default;

private:
    void InitFields(uint8_t mode, void* engine, void* gameWorld);

public:
    uint8_t  mode;             // +0xB8: 0 save, 1 load
    uint8_t  saveFlag;         // +0xB9
    uint8_t  loadFlag;         // +0xBA
    uint8_t  refreshFlag;      // +0xBB
    uint8_t  deleteFlag;       // +0xBC
    uint8_t  _padBD[3];        // +0xBD
    int32_t  selectedIndex;    // +0xC0
    int32_t  saveCount;        // +0xC4
    float    rotation;         // +0xC8: spinner angle
    uint8_t  _padCC[4];        // +0xCC
    uint8_t  sprite[0x10];     // +0xD0: UI::Sprite (default-constructed)
    void*    engine;           // +0xE0
    void*    gameWorld;        // +0xE8
    uint64_t dataF0[14];       // +0xF0..+0x158: element pointers, zeroed
};

static_assert(offsetof(SaveLoadDialog, mode) == 0xB8, "SaveLoadDialog +0xb8");
static_assert(offsetof(SaveLoadDialog, selectedIndex) == 0xC0, "SaveLoadDialog +0xc0");
static_assert(offsetof(SaveLoadDialog, rotation) == 0xC8, "SaveLoadDialog +0xc8");
static_assert(offsetof(SaveLoadDialog, sprite) == 0xD0, "SaveLoadDialog +0xd0");
static_assert(offsetof(SaveLoadDialog, engine) == 0xE0, "SaveLoadDialog +0xe0");
static_assert(offsetof(SaveLoadDialog, dataF0) == 0xF0, "SaveLoadDialog +0xf0");
static_assert(sizeof(SaveLoadDialog) == 0x160, "SaveLoadDialog is 0x160 (create 0x140021160)");
static_assert(alignof(SaveLoadDialog) == 8, "SaveLoadDialog is qword-aligned");
