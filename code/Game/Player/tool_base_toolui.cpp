// code/Game/Player/tool_base_toolui.cpp
// Player tool-UI dialog open helper (DragTool / PlacementTool family).
//
// This is an exe sub-helper resolved via ResolveAddr(Module::Exe, 0x1400ff9e0)
// from code/Game/Player/dragtool.cpp (g_fn_OpenToolDialog) and placementtool.cpp
// (s_fn_FUN_1400ff9e0). It resolves to 0 (the guarded call is silently skipped)
// unless registered. This file provides a faithful 1:1 reimplementation plus the
// supporting DialogInstance child-dialog vtable that the original installs.
//
// Player registration wave (round 2c — tool-ui). 1:1 port of the decompiled exe
// helper; do not "improve" behavior. Ghidra-verified against the disassembly.

#include "engine_funcs.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/ToolbarDescription.h"   // X::TypeId() (B.3.8 callers)
#include "ui/DialogInstance.h"  // ENDGAME B.3.8 CB4: the real UI::DialogInstance
#include "ui/Dialog.h"          // UI::Dialog::TypeId() (D5, ENDGAME B.3.9 CG3)
#include <new>

// ============================================================
// The ToolUI child dialog is a plain UI::DialogInstance (exe table 0x14015f5a0)
// ============================================================
// FUN_1400ff9e0 allocates 0xB8 bytes and runs the DialogInstance ctor with the
// constructor inlined, so the exe carries ITS OWN copy of UI::DialogInstance's
// vtable at 0x14015f5a0 (B38_SLOTS section 4: 10 slots, slot 0 the scalar
// deleting dtor 0x140020cb0 = `CALL [??1DialogInstance]; TEST BL,1; JZ; CALL
// [MemFree]`, slots 1..9 JMP-thunks onto the Runtime DialogInstance virtuals).
// It is NOT an exe class (B38_RULES 1.2): the object is a UI::DialogInstance.
//
// ENDGAME B.3.8 (CB4, D5 slot-copy site): the dialog is now built with the REAL
// constructor, `::new (dialog) UI::DialogInstance(...)`, so its vptr is the
// compiler's UI::DialogInstance table (block RB), whose slot 0 is the same
// dtor + MemFree-on-&1 and slots 1..9 the same members. That retires the old
// `g_ToolUI_ChildDialog_Vtable_Standalone[10]`, its `ToolUI_Dtor` slot-0 wrapper
// and `ToolUI_InitChildDialogVtable`, which copied slots 1..9 out of the minted
// Runtime table at registration time.
//
// ENDGAME B.3.9 close: the engine_funcs global `g_ToolUI_ChildDialog_Vtable` and
// ToolUI_ChildDialogVtable(), which pointed it at the compiler table by a
// hand-spelt `_ZTVN2UI14DialogInstanceE` symbol, are deleted: no stamper reads
// the global any more. 0x14015f5a0 stays UNregistered (reg_player_toolui.cpp:
// registering it makes the menu's BuildId child dispatch a real OnOpened).

// ============================================================
// OpenToolDialog (FUN_1400ff9e0)
// ============================================================
// Opens the placement/tool dialog ("Dialog/CreateBar.rsc"):
//   1. If param_1 (the tool's DialogInstance IBP slot) already holds a live
//      dialog, close it (UserInterface::CloseDialog) and null the IBP.
//   2. FindOrLoad the CreateBar dialog resource (External, type-checked vs the
//      External type id).
//   3. MemAlloc(0xB8, 8) a DialogInstance and run the DialogInstance ctor
//      (this, ui, resource, parentIBP=null, dataSource=descriptionPtr, 0, 0);
//      retail's inlined ctor stamps the exe's copy of its table (0x14015f5a0).
//   4. Wrap the new dialog in an IBP, store it in *param_1, and AddDialog it.
//   5. Init the tool's two float fields (param_1+0x8, param_1+0xc) to -1.0f
//      (0xbf800000) — the cached drag start/end coords.
//   6. Scan the description's component list (base +0x28, count +0x30) for the
//      type-id DWORD_1401c7f58; the original calls IsDerivedFrom on the first
//      match but discards the result (a probe with no stored side-effect — kept
//      for fidelity / its CheckPtr-style behaviour).
//   7. *param_2 = *param_1; return param_2.
//
// Caller signatures:
//   dragtool.cpp     : void* fn(void* dialogIBP, void* retval, uint64_t descriptionPtr)
//   placementtool.cpp: same shape.
//
// Closure (all satisfied):
//   - fn_UI_CloseDialog, fn_InternalBasePtr_Ctor, fn_IsDerivedFrom,
//     fn_MemAlloc, fn_UI_AddDialog  — engine_funcs exports; UI::DialogInstance's
//     real ctor (block RB)
//   - ::Object::ExternalFactory::FindOrLoad — member call (B.3.3)
//   - g_pGameWorldSingleton, g_pExternalFactorySingleton, UI::Dialog::TypeId(),
//     g_pMemAllocSingleton  — singleton globals
//   - DWORD_1401c7f58  — exe type-id storage global (ResolveAddr, lazily)
//   No unregistered exe sub-calls.
extern "C" void* ToolUI_OpenToolDialog_ff9e0(void* param_1, void* param_2, uint64_t param_3) {
    uint64_t* slot = reinterpret_cast<uint64_t*>(param_1);

    // (1) Close any existing dialog and null the IBP.
    if (*slot != 0) {
        uint64_t local = *slot;
        void* ui = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&local));
        if (fn_InternalBasePtr_Ctor) new (&local) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *slot = local;
    }

    void* ui = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);

    // (2) Load "Dialog/CreateBar.rsc" and type-check it against the External type.
    void* resource = nullptr;
    {
        System::Filename fname("Dialog/CreateBar.rsc");
        void* obj = nullptr;
        if (::Object::ExternalFactory::_singleton) {
            ::Object::ExternalFactory* efactory =
                ::Object::ExternalFactory::_singleton;  // B.3.3 bridge
            obj = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fname)));
        }
        if (obj && fn_IsDerivedFrom &&
            fn_IsDerivedFrom(obj, UI::Dialog::TypeId())) {
            resource = obj;
        }
    }

    // (3) Allocate + construct the DialogInstance (the real class: its compiler
    // table is the one retail's copy at 0x14015f5a0 describes).
    void* dialog = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        dialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    }
    void* dialogToWrap = nullptr;
    if (dialog) {
        ::new (dialog) UI::DialogInstance(
            reinterpret_cast<UI::UserInterface*>(ui),            // B.3.8 bridge
            reinterpret_cast<const UI::Dialog*>(resource),       // B.3.8 bridge
            Object::InternalPtr<UI::DialogInstance>(),           // parent: null
            reinterpret_cast<Object::Base*>(param_3),            // B.3.8 bridge (description entity)
            nullptr, 0);
        dialogToWrap = dialog;
    }

    // (4) Wrap into IBP, store, and add to the UI.
    {
        uint64_t newIBP = 0;
        if (fn_InternalBasePtr_Ctor) new (&newIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(dialogToWrap));   // B.3.3 bridge
        *slot = newIBP;
        uint64_t addIBP = newIBP;
        reinterpret_cast<UI::UserInterface*>(ui)->AddDialog(  // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&addIBP));
    }

    // (5) Reset the two cached float coords to -1.0f.
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x08) = 0xbf800000u;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x0c) = 0xbf800000u;

    // (6) Probe the description's component list for the CreateBar type id.
    // 0x1401c7f58 is ToolbarDescription's cell (Table 1).
    {
        const uint32_t tid = ToolbarDescription::TypeId();
        if (param_3) {
            int n = *reinterpret_cast<int*>(param_3 + 0x30);
            uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(param_3 + 0x28);
            for (int i = 0; i < n; i++) {
                void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(
                    *reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == tid) {
                    void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                    if (fn_IsDerivedFrom) fn_IsDerivedFrom(c, tid);  // probe; result discarded
                    break;
                }
            }
        }
    }

    // (7) Return *param_1 through param_2.
    *reinterpret_cast<uint64_t*>(param_2) = *slot;
    return param_2;
}
