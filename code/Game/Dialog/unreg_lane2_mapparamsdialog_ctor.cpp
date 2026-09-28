// code/Game/Dialog/unreg_lane2_mapparamsdialog_ctor.cpp
//
// LANE 2 of the unported-exe-.text wave (docs/waves/code_fns_wave.md, 2026-09-04).
// Row: 0x14006bd80 = MapParamsDialog::MapParamsDialog(this, ui, extRes, parentIBP).
//
// WHAT WAS BROKEN WITH IT UNREGISTERED
// ------------------------------------
// MainMenuDialog_OpenMap (code/Game/Dialog/mainmenudialog.cpp) is the retail-shaped
// handler for `case kButton_Map:`. It does:
//
//     dlgObj = fn_MapParamsDialog_Ctor(dlgMem, ui, extRes, nullIBP);
//     AddDialogToUI(self, dlgObj);
//
// That is an UNGUARDED call through a pointer resolved in EngineFuncsInit as
// `ResolveAddr(Module::Exe, 0x14006bd80)`. With the address unregistered the
// pointer returned 0 and the in-game "Map" button opened nothing. Registering this
// ctor at its retail address (standalone/reg_code_lane2.cpp) fixed it.
//
// FAITHFULNESS
// ------------
// Retail 0x14006bd80 (disassembled 0x14006bd80-0x14006be0a) is exactly:
//   local = *param_4;                                     // copy the parent IBP
//   UI::DialogInstance::DialogInstance(this, ui, extRes, &local, 0, 0, 0);
//   *(void**)this = &vtable_MapParamsDialog;              // exe .rdata 0x14015c180
//   *(u64*)(this+0xB8 .. this+0xF0) = 0;                  // 8 qwords, object is 0xF8
//   return this;
// ENDGAME B.3.8 block DLG: that body is MapParamsDialog's real constructor
// (Dialog/MapParamsDialog.h, mapparamsdialog.cpp); this door, reached by address,
// placement-news it.

#include "Dialog/MapParamsDialog.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/Internal.h"
#include <cstdint>
#include <new>
#include "Dialog/exe_bodies.h"

// Registered at exe 0x14006bd80 by RegisterCodeLane2() (standalone/reg_code_lane2.cpp).
// Signature must match fn_MapParamsDialog_Ctor_t in engine/funcs/engine_funcs.h:
//   void* (*)(void* self, void* ui, void* extRes, void* nullParent)
extern "C" void* MapParamsDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                                 void* parentIBP) {
    if (!self) return nullptr;
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentIBP)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentIBP);
    return ::new (self) MapParamsDialog(static_cast<UI::UserInterface*>(ui),
                                        static_cast<const UI::Dialog*>(extRes), parent);
}
