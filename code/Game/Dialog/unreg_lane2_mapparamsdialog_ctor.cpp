// code/Game/Dialog/unreg_lane2_mapparamsdialog_ctor.cpp
//
// LANE 2 of the unported-exe-.text wave (docs/waves/code_fns_wave.md, 2026-09-04).
// Row: 0x14006bd80 = MapParamsDialog::MapParamsDialog(this, ui, extRes, parentIBP).
//
// WHAT WAS BROKEN WITH IT UNREGISTERED
// ------------------------------------
// MainMenuDialog_OpenMap (code/Game/Dialog/mainmenudialog.cpp:344) is the live
// handler for `case kButton_Map:` in MainMenuDialog_OnCommand (line 765). It does:
//
//     dlgObj = fn_MapParamsDialog_Ctor(dlgMem, ui, extRes, nullIBP);
//     AddDialogToUI(self, dlgObj);
//
// That is an UNGUARDED call through a pointer resolved in EngineFuncsInit as
// `ResolveAddr(Module::Exe, 0x14006bd80)` (engine/funcs/engine_funcs.cpp:3203).
// The address was never registered, so the pointer was NotImplementedStub_impl,
// which returns 0 -> dlgObj == nullptr -> AddDialogToUI wraps a null IBP and
// UI::AddDialog drops it. Net effect: the in-game "Map" button leaks a 0xF8-byte
// allocation and opens nothing at all.
//
// The dialog itself is NOT missing: mapparamsdialog.cpp already ports all four
// live slots, and mainmenudialog.cpp already builds
// g_MapParamsDialog_Vtable_Standalone[20] plus a working open path
// (StandaloneOpenMapParams_UI). But that open path has ZERO callers today — only
// the headless harness entry MapParamsDialog_HarnessOpen reaches it — so the
// harness proved the dialog works while the real button stayed dead. Supplying the
// ctor at its retail address fixes the button without touching either shared file.
//
// FAITHFULNESS
// ------------
// Retail 0x14006bd80 (disassembled 0x14006bd80-0x14006be0a) is exactly:
//   local = *param_4;                                     // copy the parent IBP
//   UI::DialogInstance::DialogInstance(this, ui, extRes, &local, 0, 0, 0);
//                                                         // via .rdata slot 0x140151518
//   *(void**)this = &vtable_MapParamsDialog;              // exe .rdata 0x14015c180
//   *(u64*)(this+0xB8 .. this+0xF0) = 0;                  // 8 qwords, object is 0xF8
//   return this;
// We substitute our own 20-slot standalone vtable for the exe one, the same
// substitution autosave_sa::Ctor and eventnotify_sa::Ctor already make for their
// dialogs (registering the RETAIL ctor verbatim is what made naive registration
// crash there — it stamps an exe vtable that has no standalone counterpart).
//
// Retail vtable 0x14015c180 is 10 entries (the "ElementDescription" type-name
// string starts at 0x14015c1d0 and terminates the table):
//   [0] 0x140012150 dtor        [5] 0x14006b800 Shutdown -> MapParamsDialog_Slot5
//   [1] 0x1400120f0 TypeName    [6] 0x14006b790 Update   -> MapParamsDialog_Update
//   [2] 0x1400120e0 GetTypeId   [7] 0x14003a990 stub
//   [3] 0x14003a948 stub        [8] 0x14006b5f0 OnEvent  -> MapParamsDialog_OnEvent
//   [4] 0x14006b810 Startup     [9] 0x14003a954 stub
//              -> MapParamsDialog_Startup
// which is exactly how g_MapParamsDialog_Vtable_Standalone is already wired.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// Defined at namespace scope in code/Game/Dialog/mainmenudialog.cpp.
extern void* g_MapParamsDialog_Vtable_Standalone[20];

// Registered at exe 0x14006bd80 by RegisterCodeLane2() (standalone/reg_code_lane2.cpp).
// Signature must match fn_MapParamsDialog_Ctor_t in engine/funcs/engine_funcs.h:
//   void* (*)(void* self, void* ui, void* extRes, void* nullParent)
extern "C" void* MapParamsDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                                 void* parentIBP) {
    if (!self) return nullptr;

    // Retail copies the caller's InternalBasePtr into its own stack slot and passes
    // the ADDRESS of that copy as arg 4 (MOV RAX,[R9] / LEA R9,[R11+0x20]).
    uint64_t parentCopy = parentIBP ? *reinterpret_cast<uint64_t*>(parentIBP) : 0;
    if (fn_DialogInstance_Ctor && extRes)
        fn_DialogInstance_Ctor(self, ui, extRes, &parentCopy, nullptr, nullptr, 0);

    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    *reinterpret_cast<void**>(self) = g_MapParamsDialog_Vtable_Standalone;

    // +0xB8 editName · +0xC0 numberSeed · +0xC8 textTerrain · +0xD0 textTerrainSize
    // +0xD8 textClimate · +0xE0 textCondition · +0xE8 textDisaster · +0xF0 buttonOk
    // All eight are borrowed element pointers filled in by Startup, which the
    // AddDialog below dispatches through vtable +0x20.
    for (uintptr_t off = 0xB8; off <= 0xF0; off += 8)
        *reinterpret_cast<uint64_t*>(s + off) = 0;

    return self;
}
