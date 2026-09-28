#pragma once
// ============================================================================
// code/Game/Dialog/ModUploadDialog.h — ENDGAME B.3.8 block DLG
//
// The mod upload / delete page opened from ModDetailsDialog (Dialog/ModUpdate.rsc).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6; Ghidra calls the table
// g_ModUpdateDialog_Vtable):
//   reflected "ModUploadDialog", parent "UI::DialogInstance", ti 0x1401c52b8
//   create 0x140012bc0 (MemAlloc(0xb8, 8)), createInPlace 0x140012c10
//   ModDetailsDialog's OpenUpdateDialog 0x14006d400 / DeleteMod 0x14006d550
//   inline the ctor (DialogInstance ctor, then the table store)
//   table 0x14015c6c0, 10 slots: [0] 0x140012840 scalar deleting dtor
//   (~DialogInstance, MemFree on &1), [1] TypeName, [2] Type, and EVERY other
//   slot DialogInstance's own body (import thunks; [8] the 6-byte thunk
//   0x14003aa1a shared by four tables).
//   sizeof 0xb8: no fields of its own.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "runtime/types/ui/DialogInstance.h"
#include "Object/exe_type.h"

class ModUploadDialog final : public UI::DialogInstance {
    DECLARE_EXE_TYPE(ModUploadDialog)

public:
    ModUploadDialog() = default;                                    // create 0x140012bc0
    ModUploadDialog(UI::UserInterface* ui, const UI::Dialog* dialog,
                    Object::InternalPtr<UI::DialogInstance> parent);  // inlined 0x14006d400

    ~ModUploadDialog() override = default;                         // [0] 0x140012840
};

static_assert(sizeof(ModUploadDialog) == 0xB8, "ModUploadDialog is 0xb8 (create 0x140012bc0)");
static_assert(alignof(ModUploadDialog) == 8, "ModUploadDialog is qword-aligned");
