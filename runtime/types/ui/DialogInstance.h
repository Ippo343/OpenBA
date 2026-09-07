#pragma once
// ============================================================================
// runtime/types/ui/DialogInstance.h — UI::DialogInstance + UI::DialogController
// ============================================================================
//
// Wave 7 lane B (docs/runtime_port/WAVE7_LANE_B.md).
//
// `UI::DialogInstance` is one LIVE dialog: the runtime counterpart of the
// resource-side `UI::Dialog` (Dialog.h). It owns an Element tree root, an array
// of `UI::DialogController`s, an array of timers, and a linked list of child
// dialogs. Every exe dialog class (MainMenuDialog, ModDialog, ToolUI, …)
// derives from it, which is why its layout is load-bearing far outside this
// lane.
//
// Inheritance:  Object::Base -> Object::Internal -> UI::DialogInstance
//   `??_7DialogInstance@UI@@6B@` is EXPORTED, at 0x180156d70 (ordinal 1420).
//
// `UI::DialogController` is in this header rather than one of its own because
// it has no independent existence: it is created, owned, iterated and destroyed
// by `DialogInstance`, and every field of it below was pinned from a
// `DialogInstance` method.
//
// ---------------------------------------------------------------------------
// SIZE — WHAT IS PROVEN AND WHAT IS NOT
// ---------------------------------------------------------------------------
// `UI::UserInterface::BeginToolTip` (0x180120860, ord 1650) is the one place in
// the DLL that allocates a DialogInstance-derived object and then runs the base
// ctor on it:
//
//     pDlg = UI::ToolTipDialog::operator new(0xc0);
//     UI::DialogInstance::DialogInstance(pDlg, this, dlg->_root, parent, ..., 0, 0);
//     *(void**)pDlg = &UI::ToolTipDialog::_vftable;
//
// so sizeof(UI::ToolTipDialog) == 0xc0 and therefore sizeof(DialogInstance)
// <= 0xc0. `ToolTipDialog`'s ctor adds nothing but the vtable store, but that
// is not proof it adds no members, so **0xc0 is an upper bound, not the size.**
// The last field the base ctor writes is the int32 at +0xb0.
//
// What the replay oracle in dialog.cpp actually compares is
// `kDialogInstanceCtorExtent` = 0xb4 — the ctor-initialised prefix — and the
// reason that is safe is spelled out there: the scratch copy is a memcpy of the
// LIVE object, so the padding holes below hold identical bytes on both sides
// and are not "bytes neither side wrote"
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// THE BASE CTOR LEAVES FOUR HOLES UNINITIALISED, listed here because a future
// oracle that builds a DialogInstance from scratch MUST skip them:
//   +0x4c..0x4f   LinkedList's tail padding (Collection.h calls it `_pad14`)
//   +0x64..0x67   after the int32 at +0x60
//   +0x99..0x9f   after the bool at +0x98
//   +0xb4..        after the int32 at +0xb0
//
// ---------------------------------------------------------------------------
// EVIDENCE FOR EVERY OFFSET
// ---------------------------------------------------------------------------
// Retail addresses from the export directory of the shipped
// C:/GOG Games/Banished/Runtime-x64.dll, read with pefile before any body was
// written.
//
//   off   field              proven by                                  retail
//   ----  -----------------  -----------------------------------------  -----------
//   0x00  Object::Internal   ctor calls Object::Internal::Internal
//                            first, then stamps ??_7DialogInstance      0x180114870
//   0x10  _ui                ctor param 1 (UserInterface*)              0x180114870
//   0x18  _dialog            ctor param 2 (Dialog const*)               0x180114870
//   0x20  _flags             ctor seeds it from _dialog->+0x40;
//                            ?SetFlagsOnOff@ is `(_flags|on) & ~off`    0x180114690
//   0x24  _flags2            ctor zeroes it; no ported body reads it    0x180114870
//   0x28  _parent            ctor param 3, an InternalPtr<DialogInstance>
//                            passed BY VALUE, so the ctor takes *param  0x180114870
//   0x30  _dataSource        ctor param 4 (Object::Base*); the ctor
//                            forwards it as AddController's 2nd arg     0x180114870
//   0x38  _children          Collection::LinkedList<InternalPtr<
//                            DialogInstance>>: head +0x38, tail +0x40,
//                            count +0x48. The ctor PREPENDS itself to
//                            its parent's list — see the note below.    0x180114870
//   0x50  _root              ?GetRoot@ is one line: `*out = this[0x50]` 0x180045580
//   0x58  _spriteSheet       ctor copies _dialog->+0x20                 0x180114870
//   0x60  _spriteIndex       ctor: SpriteSheet::GetSpriteIndexFromName,
//                            or -1 when the sheet is null               0x180114870
//   0x68  _timers            Collection::Array<DialogTimer>: data +0x68,
//                            size +0x70, capacity +0x74. ?StartTimer@
//                            walks it with a stride of 0x18 and
//                            ?StopTimer@ swap-downs by 0x18.       0x1801137c0/730
//   0x78  _controllers       Collection::Array<DialogController*>: data
//                            +0x78, size +0x80, capacity +0x84. Five
//                            methods iterate exactly this pair.         0x180113320
//   0x88  _upperLeftX        ?SetUpperLeft@ delta base                  0x180114a60
//   0x8c  _upperLeftY        ?SetUpperLeft@ delta base                  0x180114a60
//   0x90  _timerHandle       ctor sets the qword to -1; no ported body
//                            reads it. NAME IS A GUESS, flagged.        0x180114870
//   0x98  _closing           ctor sets the byte to 0. NAME IS A GUESS.  0x180114870
//   0xa0  _opaqueA0          ctor zeroes it; nothing in this lane reads it
//   0xa8  _userData          ctor param 5 (void*)                       0x180114870
//   0xb0  _userFlags         ctor param 6 (int)                         0x180114870
//
// THE CHILD LIST IS A PREPEND, AND THAT MATTERS. The ctor's insert reads
//
//     if (parent->[0x38] == 0)  parent->[0x40] = node;
//     else { parent->[0x38]->[+8] = node;  node->[+0x10] = parent->[0x38]; }
//     parent->[0x38] = node;  parent->[0x48]++;
//
// which is ambiguous on its own — it is equally consistent with
// {tail@0x38, head@0x40} + append and with {head@0x38, tail@0x40} + prepend.
// The tie-break is `?GetFirstDialog@UserInterface@UI@@…` (0x180031150), which
// is literally `MOV RAX,[RCX]; MOV [RDX],RAX; RET` — the FIRST element lives at
// the list's +0x00. So +0x38 is the head, +0x40 is the tail, node+0x08 is
// `prev` and node+0x10 is `next`, and this matches
// runtime/types/collection/Collection.h's LinkedList exactly, INCLUDING its
// otherwise-unpinned `_pad14`, which is the +0x4c hole above. Recorded because
// the naive reading of the ctor alone gets it backwards.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"
#include "../object/Internal.h"

namespace RtUI {

// ============================================================
// UI::DialogController — one behaviour object bolted onto a dialog.
//
// Created by `DialogInstance::AddController` (0x1801133b0): it looks the type
// id up in `Object::Factory::_singleton`'s AVL tree, calls the factory's vtable
// slot +0x20 to construct one, checks `IsDerivedFrom(DialogController)`, then
// fills the four fields below and calls the controller's OWN vtable slot +0x20.
//
//   off   field         proven by
//   ----  ------------  ----------------------------------------------------
//   0x00  Object::Internal   the IsDerivedFrom check and the vtable calls
//   0x10  _config       AddController stores its ExternalPtr<
//                       DialogControllerConfig> argument here     0x1801133b0
//   0x18  _dialog       AddController stores the owning DialogInstance*
//   0x20  _container    ?GetContainer@ is one line: `*out = this[0x20]`
//                                                                 0x18003b700
//   0x28  _dataSource   AddController stores its Object::Base* argument
//
// SIZE IS DELIBERATELY NOT ASSERTED. Every real controller is a derived class
// allocated by the factory, and nothing in this lane allocates one, so an
// invented sizeof would be a claim we cannot back.
//
// Vtable slots this lane dispatches through, all read off the retail
// disassembly of the five DialogInstance iteration loops:
//   +0x10  GetTypeId()                 — ?GetController@   0x18011335b
//   +0x38  OnUpdateLayout()            — ?OnUpdateLayout@  0x1801153cb
//   +0x40  OnUpdate(const Time&)       — ?OnUpdate@        0x18011543e
//   +0x48  OnElementEvent(...)         — ?OnElementEvent@  0x180114f5e
// (+0x10 is `Object::Internal`'s Type slot — runtime/types/object/Internal.h
// calls the same offset kInternalVtblType, an independent confirmation.)
// ============================================================
struct DialogController {
    RtObject::Internal _base;       // +0x00 .. +0x0f
    void*              _config;     // +0x10  ExternalPtr<DialogControllerConfig>
    void*              _dialog;     // +0x18  DialogInstance*
    void*              _container;  // +0x20  InternalPtr<Element>
    void*              _dataSource; // +0x28  Object::Base*
};
static_assert(offsetof(DialogController, _config)     == 0x10, "DialogController::_config @ +0x10");
static_assert(offsetof(DialogController, _dialog)     == 0x18, "DialogController::_dialog @ +0x18");
static_assert(offsetof(DialogController, _container)  == 0x20, "DialogController::_container @ +0x20");
static_assert(offsetof(DialogController, _dataSource) == 0x28, "DialogController::_dataSource @ +0x28");

enum : int {
    kControllerVtblGetTypeId       = 0x10,
    kControllerVtblOnUpdateLayout  = 0x38,
    kControllerVtblOnUpdate        = 0x40,
    kControllerVtblOnElementEvent  = 0x48,
};

// ============================================================
// One entry of DialogInstance::_timers. Stride 0x18, pinned twice over:
// ?StartTimer@ (0x1801137c0) builds a 0x18-byte local {Time, Time, int} and
// hands it to the Array grow helper, and ?StopTimer@ (0x180113730) shifts
// entries down by copying three qwords from +0x18/+0x20/+0x28 to +0x00/+0x08/
// +0x10 while stepping the cursor by 0x18.
//
// `System::Time` is one int64 tick count (runtime/types/system/Value.h). It is
// spelled as a bare int64_t here so this header does not have to pull in a
// non-namespaced global `Time` struct.
// ============================================================
struct DialogTimer {
    int64_t _remaining;   // +0x00  System::Time
    int64_t _period;      // +0x08  System::Time — StartTimer writes the same
                          //        value into both, so this is the reload
    int32_t _id;          // +0x10
    int32_t _pad14;       // +0x14  never read or written by either method
};
static_assert(sizeof(DialogTimer) == 0x18, "DialogInstance timer stride is 0x18");
static_assert(offsetof(DialogTimer, _id) == 0x10, "DialogTimer::_id @ +0x10 (StopTimer's compare)");

// ============================================================
// UI::DialogInstance
// ============================================================
struct DialogInstance {
    RtObject::Internal _base;            // +0x00 .. +0x0f
    void*    _ui;                        // +0x10  UI::UserInterface*
    void*    _dialog;                    // +0x18  const UI::Dialog*
    uint32_t _flags;                     // +0x20
    uint32_t _flags2;                    // +0x24
    void*    _parent;                    // +0x28  DialogInstance*
    void*    _dataSource;                // +0x30  Object::Base*
    Collection::LinkedList<RtObject::InternalBasePtr> _children;  // +0x38..0x4f
    void*    _root;                      // +0x50  UI::Element*
    void*    _spriteSheet;               // +0x58  UI::SpriteSheet*
    int32_t  _spriteIndex;               // +0x60
    int32_t  _hole64;                    // +0x64  NOT written by the ctor
    Collection::Array<DialogTimer>       _timers;       // +0x68..0x77
    Collection::Array<DialogController*> _controllers;  // +0x78..0x87
    int32_t  _upperLeftX;                // +0x88
    int32_t  _upperLeftY;                // +0x8c
    int64_t  _timerHandle;               // +0x90  ctor: -1. NAME IS A GUESS.
    uint8_t  _closing;                   // +0x98  ctor: 0.  NAME IS A GUESS.
    uint8_t  _hole99[7];                 // +0x99  NOT written by the ctor
    void*    _opaqueA0;                  // +0xa0
    void*    _userData;                  // +0xa8  ctor param 5
    int32_t  _userFlags;                 // +0xb0  ctor param 6
    int32_t  _holeB4;                    // +0xb4  NOT written by the ctor
};

static_assert(offsetof(DialogInstance, _ui)          == 0x10, "DialogInstance::_ui @ +0x10");
static_assert(offsetof(DialogInstance, _dialog)      == 0x18, "DialogInstance::_dialog @ +0x18");
static_assert(offsetof(DialogInstance, _flags)       == 0x20, "DialogInstance::_flags @ +0x20 (SetFlagsOnOff)");
static_assert(offsetof(DialogInstance, _flags2)      == 0x24, "DialogInstance::_flags2 @ +0x24");
static_assert(offsetof(DialogInstance, _parent)      == 0x28, "DialogInstance::_parent @ +0x28");
static_assert(offsetof(DialogInstance, _dataSource)  == 0x30, "DialogInstance::_dataSource @ +0x30");
static_assert(offsetof(DialogInstance, _children)    == 0x38, "DialogInstance::_children @ +0x38");
static_assert(offsetof(DialogInstance, _root)        == 0x50, "DialogInstance::_root @ +0x50 (GetRoot)");
static_assert(offsetof(DialogInstance, _spriteSheet) == 0x58, "DialogInstance::_spriteSheet @ +0x58");
static_assert(offsetof(DialogInstance, _spriteIndex) == 0x60, "DialogInstance::_spriteIndex @ +0x60");
static_assert(offsetof(DialogInstance, _timers)      == 0x68, "DialogInstance::_timers @ +0x68 (Start/StopTimer)");
static_assert(offsetof(DialogInstance, _controllers) == 0x78, "DialogInstance::_controllers @ +0x78 (GetController)");
static_assert(offsetof(DialogInstance, _upperLeftX)  == 0x88, "DialogInstance::_upperLeftX @ +0x88");
static_assert(offsetof(DialogInstance, _upperLeftY)  == 0x8c, "DialogInstance::_upperLeftY @ +0x8c");
static_assert(offsetof(DialogInstance, _timerHandle) == 0x90, "DialogInstance +0x90 (ctor writes -1)");
static_assert(offsetof(DialogInstance, _closing)     == 0x98, "DialogInstance +0x98 (ctor writes a 0 byte)");
static_assert(offsetof(DialogInstance, _opaqueA0)    == 0xa0, "DialogInstance +0xa0");
static_assert(offsetof(DialogInstance, _userData)    == 0xa8, "DialogInstance::_userData @ +0xa8");
static_assert(offsetof(DialogInstance, _userFlags)   == 0xb0, "DialogInstance::_userFlags @ +0xb0");
static_assert(sizeof(DialogInstance) == 0xb8,
              "the ctor-initialised extent rounds to 0xb8; the ALLOCATED size of a "
              "DialogInstance-derived object is 0xc0 (ToolTipDialog, 0x180120860) and "
              "this struct deliberately does not claim to be that");

// The prefix the replay oracle compares: everything the base ctor initialises.
constexpr size_t kDialogInstanceCtorExtent = 0xb4;

// Upper bound on the base's size, from ToolTipDialog's `operator new(0xc0)`.
// Not sizeof(DialogInstance) — see the size note at the top of this file.
constexpr size_t kDialogInstanceAllocSize = 0xc0;

}  // namespace RtUI
