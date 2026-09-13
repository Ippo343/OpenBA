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
#include "resolve.h"
#include <cstdint>

// ============================================================
// g_ToolUI_ChildDialog_Vtable  (exe vtable @ 0x14015f5a0)
// ============================================================
// FUN_1400ff9e0 constructs a plain UI::DialogInstance (Runtime ctor installs the
// canonical Runtime DialogInstance vtable) and then OVERWRITES vtable slot 0 with
// &g_ToolUI_ChildDialog_Vtable. In the exe that vtable is a 10-slot table that
// adds NO behaviour of its own — slot 0 is the standard scalar-deleting
// destructor (DialogInstance::~DialogInstance + conditional free) and slots 1-9
// are 6-byte JMP-thunks that forward straight to the base UI::DialogInstance
// virtuals living in Runtime-x64.dll.
//
// Exe layout (verified via read_memory @ 0x14015f5a0 + disassembly of each slot;
// the JMP targets are exe IAT dispatch slots whose stored values are Runtime RVAs):
//   +0x00  0x140020cb0  ScalarDeletingDtor : [0x1401514e8] (=Runtime 0x1801b800e
//                          DialogInstance::~DialogInstance) then MemFree(0x140152b68)
//   +0x08  0x14003ab4c  TypeName        -> [0x140152700] = Runtime 0x1801bf5c2
//   +0x10  0x14003ab52  Type/GetTypeId  -> [0x140152708] = Runtime 0x1801bf5ec
//   +0x18  0x14003a948  Serialize/Persist-> [0x140151568] = Runtime 0x1801b8444
//   +0x20  0x14003aa14  OnOpened        -> [0x140151a28] = Runtime 0x1801ba468
//   +0x28  0x14003a94e  OnClosed        -> [0x140151570] = Runtime 0x1801b847a
//   +0x30  0x14003a98a  OnUpdateLayout  -> [0x1401517b0] = Runtime 0x1801b940e
//   +0x38  0x14003a990  OnUpdate        -> [0x1401517b8] = Runtime 0x1801b943c
//   +0x40  0x14003aa1a  OnElementEvent  -> [0x140151a30] = Runtime 0x1801ba490
//   +0x48  0x14003a954  Serialize2/Persist-> [0x140151578] = Runtime 0x1801b84a2
//
// In standalone the exe vtable address (0x14015f5a0) and the exe thunk/dispatch
// addresses are all unregistered (resolve to 0). So we build a static C++ vtable
// whose 10 slots forward to the SAME Runtime-x64.dll functions, resolved directly
// by Runtime RVA at first use. This is fully closure-safe: every slot targets a
// real Runtime DLL function — no unregistered exe code address is involved.
//
// Slot 0 (scalar-deleting dtor) takes (self, flags): runs the Runtime dtor then,
// if (flags&1), frees via the static allocator wrapper. We use the already-
// resolved fn_DialogInstance_Dtor / fn_MemFreeStatic instead of re-resolving the
// raw Runtime addresses (identical targets, but reuses the canonical export).

namespace {

// (There used to be a `kRVA_Dtor = 0x1b800e` here, used as an else-arm fallback in
// ToolUI_Dtor when fn_DialogInstance_Dtor was null. It was deleted 2026-09-07: that
// RVA lands in .rdata, not on a function -- ??1DialogInstance@UI@@UEAA@XZ is at
// 0x180114d40 -- so the "fallback" was a call into read-only data. See ToolUI_Dtor.)

// ⚠️ The base DialogInstance virtuals (slots 1..9) are resolved by EXPORT NAME, NOT by
// a hardcoded Runtime RVA. The original version of this file derived RVAs (0x1bf5c2,
// 0x1b943c, …) from the exe .rdata dispatch-slot VALUES — but in the static exe those
// slots hold the exe-offset of the mangled-name STRING (the engine fills them by name
// resolution at startup; see CLAUDE.md "custom runtime dispatch mechanism"), NOT the
// Runtime function RVA. So those RVAs point at string data → calling slot +0x38
// (OnUpdate, dispatched every frame on an AddDialog'd dialog) EXEC-faults. This is the
// SAME bug the toolbar (toolbar_vtable_standalone.cpp) hit + fixed empirically. Names
// taken from engine_funcs.cpp's own DialogInstance resolutions + the Runtime DLL export
// table (verified). Slot map: +0x08 TypeName / +0x10 Type / +0x18 Persist / +0x20
// OnOpened / +0x28 OnClosed / +0x30 OnUpdateLayout / +0x38 OnUpdate / +0x40
// OnElementEvent / +0x48 OnSerializeSettings.
constexpr const char* kSym_TypeName       = "?TypeName@DialogInstance@UI@@UEBAPEBDXZ";              // vt[1] +0x08
constexpr const char* kSym_Type           = "?Type@DialogInstance@UI@@UEBAIXZ";                     // vt[2] +0x10
constexpr const char* kSym_Persist        = "?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z";   // vt[3] +0x18
constexpr const char* kSym_OnOpened       = "?OnOpened@DialogInstance@UI@@UEAAXXZ";                 // vt[4] +0x20
constexpr const char* kSym_OnClosed       = "?OnClosed@DialogInstance@UI@@UEAAXXZ";                 // vt[5] +0x28
constexpr const char* kSym_OnUpdateLayout = "?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ";           // vt[6] +0x30
constexpr const char* kSym_OnUpdate       = "?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z"; // vt[7] +0x38
constexpr const char* kSym_OnElementEvent = "?OnElementEvent@DialogInstance@UI@@UEAAXAEBVElementEvent@2@@Z"; // vt[8] +0x40
constexpr const char* kSym_OnSerSettings  = "?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z"; // vt[9] +0x48

// --- Slot 0: scalar-deleting destructor (self, flags) ---
// The ONLY slot that needs a wrapper: the exe slot-0 (0x140020cb0) is a scalar-
// deleting dtor that calls the inner ~DialogInstance then conditionally frees. The
// other 9 slots are the base DialogInstance virtuals themselves — we install the RAW
// Runtime function pointers directly (lazily, see EnsureToolUIVtable), so the engine
// calls them with their exact native argument lists. (Wrapping them in fixed-arg C++
// forwarders would silently drop register args for the multi-arg virtuals like
// OnElementEvent on the Microsoft x64 ABI — a real bug, not just a signature mismatch.)
void* ToolUI_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    // Guard and SKIP on null, which is what all 20 other dialog slot-0 wrappers in
    // code/Game/Dialog/ do with this same pointer -- `if (fn_DialogInstance_Dtor)
    // fn_DialogInstance_Dtor(self);` and nothing else. This site was the only one
    // with an else arm, and its address (0x180000000 + 0x1b800e) pointed into
    // .rdata, so the one case it existed for would have executed read-only data.
    // Deleted rather than corrected to 0x180114d40: fn_DialogInstance_Dtor is
    // assigned in exactly one place (engine/funcs/engine_funcs.cpp:1837), which
    // already carries its own -- correct -- 0x180114d40 fallback, so a second copy
    // here can only fire when that one has already failed, and would then be one
    // more of the 166 by-address branches lane F has to delete before cutover.
    if (fn_DialogInstance_Dtor) fn_DialogInstance_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

}  // namespace

// The static standalone replacement for exe vtable 0x14015f5a0. Registered (in
// reg_player_toolui.cpp) so ResolveAddr(Module::Exe, 0x14015f5a0) returns this
// array; OpenToolDialog writes &g_ToolUI_ChildDialog_Vtable_Standalone into the
// new dialog's slot 0, exactly mirroring the exe. Slot 0 = our scalar-deleting dtor;
// slots 1..9 are filled with raw Runtime pointers by EnsureToolUIVtable() before the
// table is installed (initialized to null here so a never-installed table is inert).
void* g_ToolUI_ChildDialog_Vtable_Standalone[10] = {
    reinterpret_cast<void*>(&ToolUI_Dtor),  // +0x00 ScalarDeletingDtor
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};

// Populate slots 1..9 with the raw Runtime-x64.dll DialogInstance virtuals (resolved
// by RVA, ASLR handled by ResolveAddr). MUST be called EAGERLY at registration time —
// 0x14015f5a0 is a SHARED child-dialog vtable: MainMenuDialog_Serialize / EntityUI /
// PlacementTool all install it on their child dialogs, and the MAIN MENU does so at
// STARTUP (before any tool dialog opens). A lazy fill (in OpenToolDialog) leaves slots
// 1..9 null when the menu first dispatches the child's virtuals -> EXEC of 0 crash.
// RegisterPlayerToolUIBatch (AddressRegistryInit, main.cpp:1133) runs AFTER ResolveInit
// (1113) loads the DLLs, so ResolveAddr(Module::Runtime) is valid here.
extern "C" void ToolUI_InitChildDialogVtable() {
    static bool s_ready = false;
    if (s_ready) return;
    auto* vt = g_ToolUI_ChildDialog_Vtable_Standalone;
    // Copy slots 1..9 straight from the CANONICAL Runtime DialogInstance vtable
    // (Runtime 0x180156d70 — the address the DialogInstance ctor stores into every
    // instance; verified via the ctor's `LEA RAX,[0x180156d70]`). Slot 0 stays our
    // scalar-deleting dtor. This is definitive and avoids per-slot mangled-name
    // resolution: `?OnElementEvent@DialogInstance@UI@@…` (slot 8, +0x40) did NOT
    // resolve by name → slot 8 was left NULL → UI::UserInterface::UpdateEvents
    // dispatched the map dialog's vtable+0x40 (OnElementEvent) to 0 (EXEC-of-0).
    uintptr_t canon = ResolveAddr(Module::Runtime, 0x180156d70);
    if (!canon) return;   // Runtime not loaded yet — retry on a later call.
    const uintptr_t* cv = reinterpret_cast<const uintptr_t*>(canon);
    for (int i = 1; i < 10; ++i) vt[i] = reinterpret_cast<void*>(cv[i]);
    for (int i = 1; i < 10; ++i) if (!vt[i]) return;  // partial vtable must not go live
    s_ready = true;
}

// ============================================================
// OpenToolDialog (FUN_1400ff9e0)
// ============================================================
// Opens the placement/tool dialog ("Dialog/CreateBar.rsc"):
//   1. If param_1 (the tool's DialogInstance IBP slot) already holds a live
//      dialog, close it (UserInterface::CloseDialog) and null the IBP.
//   2. FindOrLoad the CreateBar dialog resource (External, type-checked vs the
//      External type id).
//   3. MemAlloc(0xB8, 8) a DialogInstance, run the Runtime DialogInstance ctor
//      (this, ui, resource, parentIBP=null, dataSource=descriptionPtr, 0, 0),
//      then overwrite slot 0 with &g_ToolUI_ChildDialog_Vtable.
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
//   - fn_UI_CloseDialog, fn_InternalBasePtr_Ctor,
//     fn_ExternalFactory_FindOrLoad, fn_IsDerivedFrom,
//     fn_MemAlloc, fn_DialogInstance_Ctor, fn_UI_AddDialog  — engine_funcs exports
//   - g_pGameWorldSingleton, g_pExternalFactorySingleton, g_pExternalTypeId,
//     g_pMemAllocSingleton  — singleton globals
//   - DWORD_1401c7f58  — exe type-id storage global (ResolveAddr, lazily)
//   - g_ToolUI_ChildDialog_Vtable_Standalone (this file) — registered @ 0x14015f5a0
//   No unregistered exe sub-calls.
extern "C" void* ToolUI_OpenToolDialog_ff9e0(void* param_1, void* param_2, uint64_t param_3) {
    ToolUI_InitChildDialogVtable();  // idempotent safety-net (also called eagerly at registration)
    uint64_t* slot = reinterpret_cast<uint64_t*>(param_1);

    // (1) Close any existing dialog and null the IBP.
    if (*slot != 0) {
        uint64_t local = *slot;
        void* ui = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);
        if (fn_UI_CloseDialog) fn_UI_CloseDialog(ui, &local);
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local, nullptr);
        *slot = local;
    }

    void* ui = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x98);

    // (2) Load "Dialog/CreateBar.rsc" and type-check it against the External type.
    void* resource = nullptr;
    {
        System::Filename fname("Dialog/CreateBar.rsc");
        void* obj = nullptr;
        if (fn_ExternalFactory_FindOrLoad && g_pExternalFactorySingleton &&
            *g_pExternalFactorySingleton) {
            obj = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, &fname);
        }
        if (obj && fn_IsDerivedFrom && g_pExternalTypeId &&
            fn_IsDerivedFrom(obj, *g_pExternalTypeId)) {
            resource = obj;
        }
    }

    // (3) Allocate + construct the DialogInstance, then install our child vtable.
    void* dialog = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        dialog = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    }
    void* dialogToWrap = nullptr;
    if (dialog) {
        uint8_t parentIBP[16] = {};
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(parentIBP, nullptr);
        if (fn_DialogInstance_Ctor) {
            fn_DialogInstance_Ctor(dialog, ui, resource, parentIBP,
                                   reinterpret_cast<void*>(param_3), nullptr, 0);
        }
        *reinterpret_cast<void**>(dialog) =
            reinterpret_cast<void*>(g_ToolUI_ChildDialog_Vtable_Standalone);
        dialogToWrap = dialog;
    }

    // (4) Wrap into IBP, store, and add to the UI.
    {
        uint64_t newIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&newIBP, dialogToWrap);
        *slot = newIBP;
        uint64_t addIBP = newIBP;
        if (fn_UI_AddDialog) fn_UI_AddDialog(ui, &addIBP);
    }

    // (5) Reset the two cached float coords to -1.0f.
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x08) = 0xbf800000u;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x0c) = 0xbf800000u;

    // (6) Probe the description's component list for the CreateBar type id.
    {
        static const uint32_t* s_pTid = nullptr;
        static bool s_init = false;
        if (!s_init) {
            s_pTid = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7f58));
            s_init = true;
        }
        if (s_pTid && param_3) {
            int n = *reinterpret_cast<int*>(param_3 + 0x30);
            uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(param_3 + 0x28);
            for (int i = 0; i < n; i++) {
                void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(
                    *reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == *s_pTid) {
                    void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                    if (fn_IsDerivedFrom) fn_IsDerivedFrom(c, *s_pTid);  // probe; result discarded
                    break;
                }
            }
        }
    }

    // (7) Return *param_1 through param_2.
    *reinterpret_cast<uint64_t*>(param_2) = *slot;
    return param_2;
}
