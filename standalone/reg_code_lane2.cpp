// standalone/reg_code_lane2.cpp
//
// LANE 2 of the unported-exe-.text wave (docs/waves/code_fns_wave.md, 2026-09-04).
// Lane list: docs/waves/lanes/code_lane2.txt (8 addresses).
//
// Only ONE of the eight was a live defect (0x14006bd80). The other seven are either
// no-ops at their single call site, or resolves into `fn_*` globals that no longer
// have any caller because the function was reimplemented natively and wired through
// our own vtable/factory instead. Two of those are registered here anyway, because
// the reimpl already exists with an identical signature and pointing the retail
// address at it is free insurance against the next caller added; the rest are left
// alone and argued in the lane report.
//
// This file touches nothing shared: address_registry.cpp only needs the one-line
// `RegisterCodeLane2();` call added next to the other lane batches.

#include "address_registry.h"
#include "resolve.h"
#include <cstdint>

// ---- 0x14006bd80 — MapParamsDialog ctor (code/Game/Dialog/unreg_lane2_mapparamsdialog_ctor.cpp)
extern "C" void* MapParamsDialog_Ctor_Standalone(void* self, void* ui, void* extRes,
                                                 void* parentIBP);

// ---- 0x14005e230 / 0x140068ab0 — AutoSaveDialog / EventNotifyDialog ctors
extern "C" void* AutoSaveDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);
extern "C" void* EventNotifyDialog_Ctor_Standalone(void* self, void* ui, void* extRes, void* parentIBP);

void RegisterCodeLane2() {
    // ------------------------------------------------------------------
    // 0x14006bd80 — MapParamsDialog::MapParamsDialog(this, ui, extRes, parentIBP)
    //
    // REAL GAP. MainMenuDialog_OpenMap (mainmenudialog.cpp:344), reached from
    // `case kButton_Map:` in MainMenuDialog_OnCommand (line 765), calls
    // fn_MapParamsDialog_Ctor UNGUARDED. That pointer is
    // ResolveAddr(Module::Exe, 0x14006bd80) (engine_funcs.cpp:3203); with the
    // address unregistered it was NotImplementedStub_impl, which returns 0, so the
    // freshly MemAlloc'd 0xF8 dialog was handed to AddDialogToUI as a null IBP and
    // dropped. The in-game "Map" button leaked and opened nothing.
    //
    // The dialog is otherwise fully brought up already — mapparamsdialog.cpp ports
    // Startup/Shutdown/Update/OnEvent and mainmenudialog.cpp builds
    // g_MapParamsDialog_Vtable_Standalone — but the working open path
    // (StandaloneOpenMapParams_UI) is only reachable from the headless harness
    // MapParamsDialog_HarnessOpen, so the harness proved the panel while the button
    // stayed dead. Our ctor mirrors retail byte-for-byte except that it stamps the
    // standalone vtable in place of exe vtable 0x14015c180 (registering the RETAIL
    // ctor verbatim is what made naive registration crash for AutoSaveDialog).
    // ------------------------------------------------------------------
    RegisterExeAddr(0x14006bd80, reinterpret_cast<uintptr_t>(&MapParamsDialog_Ctor_Standalone));

    // ------------------------------------------------------------------
    // 0x14005e230 — AutoSaveDialog::AutoSaveDialog(this, ui, extRes, parentIBP)
    // 0x140068ab0 — EventNotifyDialog::EventNotifyDialog(this, ui, extRes, parentIBP)
    //
    // HYGIENE, not a fix — and specifically NOT the retail bodies. Both dialogs are
    // already constructed by GameUpdateLoop::Startup through
    // AutoSaveDialog_Ctor_Standalone (gameupdateloop.cpp:2528) and
    // EventNotifyDialog_Ctor_Standalone (gameupdateloop.cpp:2479), which stamp our
    // hand-built vtables instead of exe vtables 0x14015b120 / 0x14015b1d0. The census
    // flagged these two addresses as LOAD-WINDOW only because
    // `fn_AutoSaveDialog_Ctor` / `fn_EventNotifyDialog_Ctor`
    // (gameupdateloop.cpp:537-540) are still resolved during postload — but neither
    // global has a single call site, so nothing was ever skipped. Publishing the
    // standalone ctors at the retail addresses keeps those vestigial pointers valid
    // and, per autosavedialog.cpp:202, is the ONLY safe thing to publish there: the
    // retail ctor stamps an exe vtable with no standalone counterpart, which is what
    // made naive registration of 0x14005e230 crash previously.
    // ------------------------------------------------------------------
    RegisterExeAddr(0x14005e230, reinterpret_cast<uintptr_t>(&AutoSaveDialog_Ctor_Standalone));
    RegisterExeAddr(0x140068ab0, reinterpret_cast<uintptr_t>(&EventNotifyDialog_Ctor_Standalone));
}
